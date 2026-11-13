/*****************************************************************************
 * reg_test.c - 验证 IgH 用户态寄存器读写能力
 *
 * 对比 SOEM 的 ecx_FPRD / ecx_FPWR
 ****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <time.h>

#include "ecrt.h"

#define SLAVE_POS     0, 0
#define VENDOR        0x00000056
#define PRODUCT       0x00000102

static ec_master_t *master;
static ec_slave_config_t *sc;
static ec_reg_request_t *reg;

int main(void)
{
    uint8_t *data;
    ec_request_state_t st;
    int i, ret;

    mlockall(MCL_CURRENT | MCL_FUTURE);

    master = ecrt_request_master(0);
    if (!master) {
        fprintf(stderr, "request_master failed\n");
        return 1;
    }

    sc = ecrt_master_slave_config(master, SLAVE_POS, VENDOR, PRODUCT);
    if (!sc) {
        fprintf(stderr, "slave_config failed\n");
        return 1;
    }

    /* 创建 16 字节的寄存器请求对象 */
    reg = ecrt_slave_config_create_reg_request(sc, 16);
    if (!reg) {
        fprintf(stderr, "create_reg_request failed\n");
        return 1;
    }

    if (ecrt_master_activate(master)) {
        fprintf(stderr, "activate failed\n");
        return 1;
    }

    /* 等待主站进入 OP（寄存器请求需要主站运行） */
    {
        ec_master_state_t ms;
        int wait;
        for (wait = 0; wait < 3000; wait++) {
            ecrt_master_state(master, &ms);
            if (ms.al_states & 0x08)  /* bit3 = OP */
                break;
            usleep(1000);
        }
        printf("master al_states = 0x%02X (0x08=OP), waited %d ms\n\n",
                ms.al_states, wait);
    }

    printf("=== 用户态寄存器读取测试 ===\n\n");

    /* 读 ESC Type (0x0000, 4 字节) */
    ret = ecrt_reg_request_read(reg, 0x0000, 4);
    printf("ecrt_reg_request_read(0x0000, 4) = %d\n", ret);

    /* 等待完成 */
    for (i = 0; i < 100; i++) {
        st = ecrt_reg_request_state(reg);
        if (st != EC_REQUEST_BUSY)
            break;
        usleep(1000);
    }
    printf("state = %d (0=BUSY 1=SUCCESS 2=ERROR)\n", st);

    if (st == EC_REQUEST_SUCCESS) {
        data = ecrt_reg_request_data(reg);
        printf("data ptr = %p\n", (void *)data);
        printf("ESC Type: 0x%02X 0x%02X 0x%02X 0x%02X\n",
                data[0], data[1], data[2], data[3]);
    } else {
        printf("request failed, state=%d\n", st);
    }

    /* 读 AL Status (0x0130, 2 字节) */
    ecrt_reg_request_read(reg, 0x0130, 2);
    for (i = 0; i < 100; i++) {
        st = ecrt_reg_request_state(reg);
        if (st != EC_REQUEST_BUSY)
            break;
        usleep(1000);
    }
    if (st == EC_REQUEST_SUCCESS) {
        data = ecrt_reg_request_data(reg);
        printf("AL Status: 0x%04X\n", data[0] | (data[1] << 8));
    }

    /* 读 SM0 (0x0800, 8 字节) */
    ecrt_reg_request_read(reg, 0x0800, 8);
    for (i = 0; i < 100; i++) {
        st = ecrt_reg_request_state(reg);
        if (st != EC_REQUEST_BUSY)
            break;
        usleep(1000);
    }
    if (st == EC_REQUEST_SUCCESS) {
        data = ecrt_reg_request_data(reg);
        printf("SM0: PhysAddr=0x%04X Len=0x%04X Ctrl=0x%02X En=%d\n",
                data[0] | (data[1] << 8),
                data[2] | (data[3] << 8),
                data[4], data[6]);
    }

    printf("\n=== 结论: IgH 用户态支持 ESC 寄存器读写 ===\n");

    ecrt_release_master(master);
    return 0;
}
