/*****************************************************************************
 * servo_op.c - IgH EtherCAT 最小可用示例
 *
 * 目标从站: Sanhua RJM_070S 伺服 (Vendor 0x56, Product 0x102)
 *   RxPDO (主站->从站): 0x6040 Controlword (16bit), 0x607A Target Position (32bit)
 *   TxPDO (从站->主站): 0x603F Error Code (16bit), 0x6041 Statusword (16bit),
 *                       0x6064 Actual Position (32bit)
 *
 * 功能:
 *   - 1ms 周期, DC SYNC0 shift = 500us
 *   - CiA402 状态机自动使能到 Operation Enabled
 *   - 保持 OP 状态, 打印时序统计
 *
 * 编译:
 *   gcc -O2 -I../include -o servo_op servo_op.c -L../lib -lethercat -lrt -lpthread
 *
 * 运行 (需要 root):
 *   sudo ./servo_op [cycle_ns] [shift_ns] [assign_activate]
 *   默认: 1000000 500000 0x0300
 ****************************************************************************/

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <time.h>
#include <unistd.h>
#include <sched.h>

#include "ecrt.h"

/****************************************************************************/

#define NSEC_PER_SEC 1000000000L
#define CLOCK_TO_USE CLOCK_MONOTONIC

#define DIFF_NS(A, B) \
    (((B).tv_sec - (A).tv_sec) * NSEC_PER_SEC + (B).tv_nsec - (A).tv_nsec)
#define TIMESPEC2NS(T) ((uint64_t)(T).tv_sec * NSEC_PER_SEC + (T).tv_nsec)

/* 从站标识 */
#define SLAVE_POS        0, 0
#define SANHUA_VENDOR    0x00000056
#define SANHUA_PRODUCT   0x00000102

/* 运行参数 (可被命令行覆盖) */
static uint32_t cycle_ns = 1000000;   /* 1ms */
static int32_t  shift_ns = 500000;    /* 500us */
static uint16_t assign_activate = 0x0300; /* DC SYNC0 + SYNC1 */

/****************************************************************************/

static volatile sig_atomic_t running = 1;

static void signal_handler(int sig)
{
    (void)sig;
    running = 0;
}

/****************************************************************************/

static ec_master_t *master = NULL;
static ec_domain_t *domain = NULL;
static uint8_t *domain_pd = NULL;
static ec_slave_config_t *sc = NULL;

static unsigned int off_controlword = 0;
static unsigned int off_target_pos = 0;
static unsigned int off_error_code = 0;
static unsigned int off_statusword = 0;
static unsigned int off_actual_pos = 0;

static ec_master_state_t master_state = {};
static ec_domain_state_t domain_state = {};

/****************************************************************************/

static struct timespec timespec_add(struct timespec a, struct timespec b)
{
    struct timespec r;
    r.tv_sec = a.tv_sec + b.tv_sec;
    r.tv_nsec = a.tv_nsec + b.tv_nsec;
    if (r.tv_nsec >= NSEC_PER_SEC) {
        r.tv_sec++;
        r.tv_nsec -= NSEC_PER_SEC;
    }
    return r;
}

/****************************************************************************/

static void check_master_state(void)
{
    ec_master_state_t ms;
    ecrt_master_state(master, &ms);

    if (ms.slaves_responding != master_state.slaves_responding)
        printf("[master] %u slave(s) responding\n", ms.slaves_responding);
    if (ms.al_states != master_state.al_states)
        printf("[master] AL states: 0x%02X\n", ms.al_states);
    if (ms.link_up != master_state.link_up)
        printf("[master] link %s\n", ms.link_up ? "UP" : "DOWN");

    master_state = ms;
}

/****************************************************************************/

static void check_domain_state(void)
{
    ec_domain_state_t ds;
    ecrt_domain_state(domain, &ds);

    if (ds.working_counter != domain_state.working_counter)
        printf("[domain] WC = %u\n", ds.working_counter);
    if (ds.wc_state != domain_state.wc_state)
        printf("[domain] WC state = %u\n", ds.wc_state);

    domain_state = ds;
}

/****************************************************************************/

/* CiA402 状态机: 根据 statusword 决定 controlword */
static uint16_t cia402_controlword(uint16_t statusword)
{
    switch (statusword & 0x006F) {
        case 0x0040: /* Switch on disabled */
            return 0x0006; /* Shutdown */
        case 0x0021: /* Ready to switch on */
            return 0x0007; /* Switch on */
        case 0x0023: /* Switched on */
            return 0x000F; /* Enable operation */
        case 0x0027: /* Operation enabled */
            return 0x000F; /* Keep enabled */
        case 0x0008: /* Fault */
            return 0x0080; /* Fault reset */
        default:
            return 0x0006; /* Shutdown */
    }
}

/****************************************************************************/

static void cyclic_task(void)
{
    struct timespec wakeup, now;
    struct timespec cycletime = {0, 0};
    uint32_t period_min = 0xFFFFFFFF, period_max = 0;
    uint32_t latency_min = 0xFFFFFFFF, latency_max = 0;
    uint32_t exec_min = 0xFFFFFFFF, exec_max = 0;
    struct timespec last_start = {}, end_time = {};
    unsigned int counter = 0;
    int32_t target_pos = 0;
    uint16_t statusword = 0;
    uint16_t controlword = 0;
    int32_t actual_pos = 0;
    uint16_t error_code = 0;

    cycletime.tv_nsec = cycle_ns;

    clock_gettime(CLOCK_TO_USE, &wakeup);

    printf("\n=== 开始循环: cycle=%u ns, shift=%d ns, AssignActivate=0x%04X ===\n\n",
            cycle_ns, shift_ns, assign_activate);

    while (running) {
        wakeup = timespec_add(wakeup, cycletime);
        clock_nanosleep(CLOCK_TO_USE, TIMER_ABSTIME, &wakeup, NULL);

        /* 应用时间: 用目标时间而非实测时间, 更稳定 */
        ecrt_master_application_time(master, TIMESPEC2NS(wakeup));

        clock_gettime(CLOCK_TO_USE, &now);
        {
            uint32_t latency = DIFF_NS(wakeup, now);
            uint32_t period = DIFF_NS(last_start, now);
            uint32_t exec = DIFF_NS(last_start, end_time);

            if (latency > latency_max) latency_max = latency;
            if (latency < latency_min) latency_min = latency;
            if (period > period_max) period_max = period;
            if (period < period_min) period_min = period;
            if (exec > exec_max) exec_max = exec;
            if (exec < exec_min) exec_min = exec;
        }
        last_start = now;

        /* 接收过程数据 */
        ecrt_master_receive(master);
        ecrt_domain_process(domain);

        /* 读取输入 */
        statusword = EC_READ_U16(domain_pd + off_statusword);
        actual_pos = EC_READ_S32(domain_pd + off_actual_pos);
        error_code = EC_READ_U16(domain_pd + off_error_code);

        /* CiA402 状态机 */
        controlword = cia402_controlword(statusword);

        /* 写输出 */
        EC_WRITE_U16(domain_pd + off_controlword, controlword);
        EC_WRITE_S32(domain_pd + off_target_pos, target_pos);

        /* 1Hz 打印状态与时序 */
        if (++counter >= 1000) {
            counter = 0;
            check_master_state();
            check_domain_state();

            printf("status=0x%04X ctrl=0x%04X pos=%d err=0x%04X | "
                   "period %u..%u  exec %u..%u  latency %u..%u (ns)\n",
                    statusword, controlword, actual_pos, error_code,
                    period_min, period_max,
                    exec_min, exec_max,
                    latency_min, latency_max);

            period_min = 0xFFFFFFFF; period_max = 0;
            exec_min = 0xFFFFFFFF;   exec_max = 0;
            latency_min = 0xFFFFFFFF; latency_max = 0;
        }

        /* DC 同步 */
        ecrt_master_sync_reference_clock_to(master, TIMESPEC2NS(now));
        ecrt_master_sync_slave_clocks(master);

        /* 发送过程数据 */
        ecrt_domain_queue(domain);
        ecrt_master_send(master);

        clock_gettime(CLOCK_TO_USE, &end_time);
    }

    printf("\n=== 循环结束 ===\n");
}

/****************************************************************************/

int main(int argc, char **argv)
{
    struct sched_param param = {};
    int ret;

    if (argc > 1) cycle_ns = (uint32_t)strtoul(argv[1], NULL, 0);
    if (argc > 2) shift_ns = (int32_t)strtol(argv[2], NULL, 0);
    if (argc > 3) assign_activate = (uint16_t)strtoul(argv[3], NULL, 0);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* 锁定内存, 防止换页导致抖动 */
    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        perror("mlockall failed");
        return -1;
    }

    printf("请求主站 0...\n");
    master = ecrt_request_master(0);
    if (!master) {
        fprintf(stderr, "ecrt_request_master 失败 (需要 root, 且模块已加载)\n");
        return -1;
    }

    domain = ecrt_master_create_domain(master);
    if (!domain) {
        fprintf(stderr, "create_domain 失败\n");
        return -1;
    }

    printf("配置从站 %u:%u (0x%08X:0x%08X)...\n",
            SLAVE_POS, SANHUA_VENDOR, SANHUA_PRODUCT);

    sc = ecrt_master_slave_config(master, SLAVE_POS,
            SANHUA_VENDOR, SANHUA_PRODUCT);
    if (!sc) {
        fprintf(stderr, "slave_config 失败 (从站不在总线上?)\n");
        return -1;
    }

    /* 注册 PDO 条目, 获取在过程数据镜像中的偏移 */
    off_controlword = ecrt_slave_config_reg_pdo_entry(
            sc, 0x6040, 0x00, domain, NULL);
    if ((int)off_controlword < 0) {
        fprintf(stderr, "注册 0x6040 失败\n");
        return -1;
    }

    off_target_pos = ecrt_slave_config_reg_pdo_entry(
            sc, 0x607A, 0x00, domain, NULL);
    if ((int)off_target_pos < 0) {
        fprintf(stderr, "注册 0x607A 失败\n");
        return -1;
    }

    off_error_code = ecrt_slave_config_reg_pdo_entry(
            sc, 0x603F, 0x00, domain, NULL);
    if ((int)off_error_code < 0) {
        fprintf(stderr, "注册 0x603F 失败\n");
        return -1;
    }

    off_statusword = ecrt_slave_config_reg_pdo_entry(
            sc, 0x6041, 0x00, domain, NULL);
    if ((int)off_statusword < 0) {
        fprintf(stderr, "注册 0x6041 失败\n");
        return -1;
    }

    off_actual_pos = ecrt_slave_config_reg_pdo_entry(
            sc, 0x6064, 0x00, domain, NULL);
    if ((int)off_actual_pos < 0) {
        fprintf(stderr, "注册 0x6064 失败\n");
        return -1;
    }

    /* 配置 DC: SYNC0 周期 + shift */
    ret = ecrt_slave_config_dc(sc, assign_activate,
            cycle_ns,   /* SYNC0 cycle */
            shift_ns,   /* SYNC0 shift */
            0,          /* SYNC1 cycle */
            0);         /* SYNC1 shift */
    if (ret) {
        fprintf(stderr, "slave_config_dc 失败: %d\n", ret);
        return -1;
    }

    printf("激活主站...\n");
    if (ecrt_master_activate(master)) {
        fprintf(stderr, "activate 失败\n");
        return -1;
    }

    domain_pd = ecrt_domain_data(domain);
    if (!domain_pd) {
        fprintf(stderr, "domain_data 失败\n");
        return -1;
    }

    /* 实时优先级 */
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
        perror("sched_setscheduler failed (继续运行)");
    } else {
        printf("SCHED_FIFO 优先级 %d\n", param.sched_priority);
    }

    cyclic_task();

    ecrt_release_master(master);
    return 0;
}
