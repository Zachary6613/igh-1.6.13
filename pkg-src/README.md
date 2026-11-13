# IgH EtherCAT Master 1.6.13 使用包

已编译好的 IgH EtherCAT 主站，含内核模块、用户态库、命令行工具和示例程序。

- 内核版本：`6.14.0-rt3`（PREEMPT_RT）
- 构建配置：`--enable-kernel --enable-generic --enable-hrtimer --enable-eoe`
- 用户态接口：**105 个 `ecrt_*` 全部实现**（`ecrt.h` 声明 = `libethercat.so` 导出）
- 目标从站：Sanhua RJM_070S 伺服（Vendor `0x56`, Product `0x102`）

> 本包相对原版 IgH 有 3 处改动：FoE 密码支持、SII/EEPROM 用户态接口、启用 EoE。
> 详见 [本包相对原版的改动](#本包相对原版的改动)。

---

## 目录结构

```
igh-1.6.13-package/
├── README.md              本文件（使用指南）
├── API-COMPARISON.md      IgH vs SOEM 接口对比（双协议栈兼容参考）
├── modules/
│   ├── ec_master.ko       主站内核模块
│   └── ec_generic.ko      通用网卡驱动（用内核 socket 收发）
├── lib/
│   ├── libethercat.so.1.2.0   用户态库
│   ├── libethercat.so.1       -> libethercat.so.1.2.0
│   ├── libethercat.so         -> libethercat.so.1.2.0
│   └── libethercat.a          静态库
├── include/
│   └── ecrt.h             唯一需要的头文件
├── bin/
│   └── ethercat           命令行工具
├── examples/
│   ├── servo_op.c         1ms 周期 + DC shift 500us，CiA402 自动使能
│   ├── sii_test.c         SII/EEPROM 读写验证
│   ├── reg_test.c         寄存器请求（reg_request）验证
│   └── servo_op / sii_test / reg_test   已编译好的可执行文件
└── scripts/
    ├── load.sh            加载模块
    ├── unload.sh          卸载模块
    └── build.sh           编译示例
```

---

## 快速开始

### 1. 加载主站

```bash
cd igh-1.6.13-package
sudo ./scripts/load.sh enp173s0f0
```

脚本做的事：

```bash
MAC=$(cat /sys/class/net/enp173s0f0/address)
insmod modules/ec_master.ko main_devices=$MAC
insmod modules/ec_generic.ko
```

> **注意**：`main_devices` 参数必须是 **MAC 地址**，不是接口名。
> `ec_generic` 会把所有以太网口都 offer 给主站，主站按 MAC 只认一个。

成功后会出现 `/dev/EtherCAT0`，并打印从站列表。

### 2. 查看状态

```bash
sudo ./bin/ethercat master      # 主站状态、帧率、丢帧
sudo ./bin/ethercat slaves      # 从站列表
sudo ./bin/ethercat slaves -v   # 从站详细信息
sudo ./bin/ethercat pdos        # PDO 布局
sudo ./bin/ethercat domains     # 域状态
```

### 3. 运行示例

```bash
sudo ./examples/servo_op
```

实测输出（1ms 周期，shift 500us，已进入 OP）：

```
[master] 1 slave(s) responding
[master] AL states: 0x08          <- 0x08 = OP
[master] link UP
[domain] WC = 3
[domain] WC state = 2
status=0x1237 ctrl=0x000F pos=3392691 err=0x0000 | period 988595..1012388  exec 1801..11064  latency 1693..15788 (ns)
status=0x1237 ctrl=0x000F pos=3392691 err=0x0000 | period 991876..1012878  exec 1858..10226  latency 1606..14687 (ns)
```

- `status=0x1237`：CiA402 **Operation enabled**（已使能）
- `ctrl=0x000F`：Enable operation
- `pos`：实际位置（0x6064）
- `err=0x0000`：无故障
- `period`：周期抖动，约 ±12us
- `exec`：循环执行耗时，约 1.8~11us
- `latency`：唤醒延迟，约 1.6~16us

> 最后一行 `err=0x8611` 是 `timeout` 强杀进程导致的，正常退出（Ctrl-C）不会有。

### 4. 卸载

```bash
sudo ./scripts/unload.sh
```

---

## 命令行工具

### FoE 固件传输（支持密码）

原版 IgH 的 FoE 密码字段硬编码为 0，本包已改为可配置。

```bash
# 写文件到从站（带密码）
sudo ./bin/ethercat foe_write -p0 --password 0x12345678 firmware.bin

# 从从站读文件（带密码）
sudo ./bin/ethercat foe_read -p0 --password 0x12345678 firmware.bin > out.bin

# 不带密码（默认，等价于原版行为）
sudo ./bin/ethercat foe_write -p0 firmware.bin
```

`--password` / `-P` 支持十进制、八进制（`0` 前缀）、十六进制（`0x` 前缀）。

验证密码是否写入帧（开 debug 后看 dmesg）：

```bash
sudo ./bin/ethercat debug 1
sudo ./bin/ethercat foe_read -p0 --password 0x12345678 TESTFILE
sudo dmesg | tail -5
```

输出：

```
EtherCAT DEBUG 0-0: FoE Read Request:
EtherCAT DEBUG: 01 00 78 56 34 12 54 45 53 54 46 49 4C 45
                │  │  └─────┬─────┘ └──────┬──────┘
                │  │        │              └─ "TESTFILE"
                │  │        └─ 密码 0x12345678（小端）
                │  └─ 保留位
                └─ OpCode 0x0001 = RRQ
```

写请求同理，OpCode 为 `0x0002`（WRQ）。

### SII / EEPROM 读写

```bash
# 读 SII 到文件（备份）
sudo ./bin/ethercat sii_read -p0 > sii_backup.bin

# 写 SII（会校验 CRC）
sudo ./bin/ethercat sii_write -p0 sii_backup.bin
```

> ⚠️ 写 SII 有风险，内容无效会导致从站无法使用。务必先备份。

### 从站 XML 导出

```bash
sudo ./bin/ethercat xml -p0 > slave.xml
```

> 只有导出方向。XML → SII 的转换 IgH 和 SOEM 都不提供。

---

## 示例程序

### servo_op — 伺服控制主示例

```bash
sudo ./examples/servo_op [cycle_ns] [shift_ns] [assign_activate]
```

| 参数 | 默认值 | 说明 |
|---|---|---|
| `cycle_ns` | `1000000` | 周期，1ms |
| `shift_ns` | `500000` | SYNC0 shift time，500us |
| `assign_activate` | `0x0300` | DC 激活字 |

例：

```bash
sudo ./examples/servo_op 1000000 500000 0x0300   # 1ms, shift 500us
sudo ./examples/servo_op 2000000 1000000 0x0300  # 2ms, shift 1ms
sudo ./examples/servo_op 1000000 0 0x0000        # 关闭 DC，纯自由运行
```

#### AssignActivate 位含义

| 位 | 值 | 含义 |
|---|---|---|
| 8 | `0x0100` | 使能 SYNC0 |
| 9 | `0x0200` | 使能 SYNC1 |
| 10 | `0x0400` | SYNC0 由从站自身产生（否则由主站 DC 参考时钟驱动） |

`0x0300` = SYNC0 + SYNC1 都使能。若从站报 PLL 错误，先试 `0x0100`，再试 `0x0000`。

### sii_test — SII/EEPROM 读写验证

```bash
sudo ./examples/sii_test           # 只读
sudo ./examples/sii_test --write   # 读 + 写回相同内容
```

输出：

```
=== ecrt_master_sii_read 测试 ===
ecrt_master_sii_read() = 0

SII 头部解析:
  PDI control   : 0x0680
  ESC config    : 0x6681
  Alias         : 0x3412
  Vendor ID     : 0x00000056
  Product code  : 0x00000102
  Revision num  : 0x00000001
  Serial number : 0x00000000
```

### reg_test — 寄存器请求验证

```bash
sudo ./examples/reg_test
```

演示 `ecrt_slave_config_create_reg_request()` 的异步寄存器读写。

> ⚠️ `ec_request_state_t` 枚举值：`UNUSED=0, BUSY=1, SUCCESS=2, ERROR=3`。
> `SUCCESS` 是 **2** 不是 1，容易写错。

---

## 编程接口

### 编译

```bash
gcc -O2 -I<包目录>/include -o myapp myapp.c \
    -L<包目录>/lib -lethercat -lrt -lpthread \
    -Wl,-rpath,'$ORIGIN/../lib'
```

`-Wl,-rpath` 让程序运行时能找到同目录的 `libethercat.so`，否则需要：

```bash
export LD_LIBRARY_PATH=<包目录>/lib:$LD_LIBRARY_PATH
```

### 最小骨架

```c
#include "ecrt.h"

ec_master_t *master = ecrt_request_master(0);
ec_domain_t *domain = ecrt_master_create_domain(master);

ec_slave_config_t *sc = ecrt_master_slave_config(
        master, 0, 0, 0x00000056, 0x00000102);

unsigned int off_ctrl = ecrt_slave_config_reg_pdo_entry(
        sc, 0x6040, 0x00, domain, NULL);

ecrt_slave_config_dc(sc, 0x0300, 1000000, 500000, 0, 0);

ecrt_master_activate(master);
uint8_t *pd = ecrt_domain_data(domain);

/* 循环 */
while (1) {
    clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &wakeup, NULL);
    ecrt_master_application_time(master, TIMESPEC2NS(wakeup));

    ecrt_master_receive(master);
    ecrt_domain_process(domain);

    EC_WRITE_U16(pd + off_ctrl, 0x000F);   /* 写输出 */

    ecrt_master_sync_reference_clock_to(master, TIMESPEC2NS(now));
    ecrt_master_sync_slave_clocks(master);

    ecrt_domain_queue(domain);
    ecrt_master_send(master);
}
```

### 关键 API

| 函数 | 用途 |
|---|---|
| `ecrt_request_master(0)` | 获取主站句柄 |
| `ecrt_master_create_domain()` | 创建过程数据域 |
| `ecrt_master_slave_config()` | 按 alias/position + vendor/product 定位从站 |
| `ecrt_slave_config_reg_pdo_entry()` | 注册 PDO 条目，返回在 `pd` 中的字节偏移 |
| `ecrt_slave_config_dc()` | 配置 DC SYNC0/SYNC1 |
| `ecrt_master_activate()` | 激活主站（此后从站开始状态机） |
| `ecrt_domain_data()` | 取过程数据镜像指针 |
| `ecrt_master_receive()` | 收帧（每周期开头） |
| `ecrt_domain_process()` | 处理域数据 |
| `ecrt_domain_queue()` | 提交输出数据 |
| `ecrt_master_send()` | 发帧（每周期结尾） |
| `ecrt_master_application_time()` | 告诉主站当前应用时间（DC 必需） |
| `ecrt_master_sync_reference_clock_to()` | 同步参考时钟 |
| `ecrt_master_sync_slave_clocks()` | 同步从站时钟 |

### 本包新增的 API

#### SII / EEPROM 读写

原版 IgH 只有 ioctl 和命令行工具，用户态库无封装。本包新增：

```c
int ecrt_master_sii_read(ec_master_t *master, uint16_t slave_position,
        uint16_t offset, uint32_t nwords, uint16_t *words);

int ecrt_master_sii_write(ec_master_t *master, uint16_t slave_position,
        uint16_t offset, uint32_t nwords, const uint16_t *words);
```

- `offset`：**word 偏移**（不是字节偏移）
- `nwords`：word 数（1 word = 2 字节）
- `words`：`uint16_t` 数组

示例：

```c
uint16_t sii[0x200];
ecrt_master_sii_read(master, 0, 0, 0x200, sii);

printf("Vendor ID: 0x%08X\n",
        sii[0x08] | ((uint32_t)sii[0x09] << 16));
```

SII 头部字段偏移（word 索引）：

| 字段 | 偏移 | 长度 |
|---|---|---|
| PDI control | `0x00` | 1 |
| ESC config | `0x01` | 1 |
| Alias | `0x04` | 1 |
| Vendor ID | `0x08` | 2 |
| Product code | `0x0A` | 2 |
| Revision number | `0x0C` | 2 |
| Serial number | `0x0E` | 2 |

> ⚠️ 写 SII 有风险，且库接口**不做 CRC 校验**。务必先备份。

#### FoE 密码

FoE 走 ioctl，用户态库无封装。需要时用命令行工具，或参考
`tool/CommandFoeWrite.cpp` 自己封装 `EC_IOCTL_SLAVE_FOE_WRITE`。

### 数据访问宏

```c
EC_READ_U8(p)   EC_READ_S8(p)
EC_READ_U16(p)  EC_READ_S16(p)
EC_READ_U32(p)  EC_READ_S32(p)
EC_READ_U64(p)  EC_READ_S64(p)

EC_WRITE_U8(p, v)   EC_WRITE_S8(p, v)
EC_WRITE_U16(p, v)  EC_WRITE_S16(p, v)
EC_WRITE_U32(p, v)  EC_WRITE_S32(p, v)
EC_WRITE_U64(p, v)  EC_WRITE_S64(p, v)
```

`p` 是 `pd + offset`，offset 来自 `ecrt_slave_config_reg_pdo_entry()`。

---

## 本从站的 PDO 布局

```
SM2 (RxPDO, 主站->从站)  PhysAddr 0x1400, 11 字节
  0x6040:00  Controlword        16 bit
  0x607A:00  Target Position    32 bit

SM3 (TxPDO, 从站->主站)  PhysAddr 0x1600, 23 字节
  0x603F:00  Error Code         16 bit
  0x6041:00  Statusword         16 bit
  0x6064:00  Actual Position    32 bit
```

> SM2 声明 11 字节但映射只有 6 字节，SM3 声明 23 字节但映射只有 8 字节。
> 这是从站 ESI 的 padding，不影响使用。

### CiA402 状态机

示例里 `cia402_controlword()` 根据 statusword 自动推进：

| statusword & 0x6F | 状态 | controlword |
|---|---|---|
| `0x40` | Switch on disabled | `0x0006` Shutdown |
| `0x21` | Ready to switch on | `0x0007` Switch on |
| `0x23` | Switched on | `0x000F` Enable operation |
| `0x27` | Operation enabled | `0x000F` 保持 |
| `0x08` | Fault | `0x0080` Fault reset |

---

## 时序调优

### 绑核（最有效）

你的内核有 `isolcpus=4,5,6,7`。在 `main()` 里加：

```c
cpu_set_t set;
CPU_ZERO(&set);
CPU_SET(5, &set);
sched_setaffinity(0, sizeof(set), &set);
```

实测隔离核抖动 3us，普通核 37us。

### 中断亲和性

```bash
IRQS=$(grep enp173s0f0 /proc/interrupts | awk '{print $1}' | tr -d ':' | tr '\n' ' ')
for n in $IRQS; do echo 5 > /proc/irq/$n/smp_affinity_list; done
```

应用线程和网卡中断绑同一个隔离核效果最好。

### 其他

- `mlockall(MCL_CURRENT | MCL_FUTURE)` — 示例已有
- `SCHED_FIFO` 最高优先级 — 示例已有
- CPU governor 设 `performance`：
  ```bash
  sudo cpupower frequency-set -g performance
  ```

---

## 常见问题

### `ecrt_request_master` 返回 NULL

- 没加载模块 → `sudo ./scripts/load.sh`
- 不是 root → 用 `sudo`
- `/dev/EtherCAT0` 权限是 `crw------- root root`，普通用户无法访问

### 从站停在 PREOP，进不了 OP

1. 检查 PDO 映射是否匹配：`sudo ./bin/ethercat pdos`
2. 检查 AL status code：`sudo ./bin/ethercat slaves -v`
3. 常见原因：SM2/SM3 长度不对、PDO 条目注册失败

### 从站停在 INIT 且带 `E` 标志

说明从站有 AL 错误未清除。**重载模块**即可复位：

```bash
sudo ./scripts/unload.sh
sudo ./scripts/load.sh enp173s0f0
```

如果 `rmmod` 报 `File exists` / 引用计数不为 0，说明有残留进程占着主站：

```bash
sudo pkill -9 -f servo_op
sudo rmmod ec_generic
sudo rmmod ec_master
```

### `Failed to get DC times: No response`

从站 DC 还没同步好，通常出现在刚激活的头几个周期，之后会自动恢复。
如果持续出现，检查 `assign_activate` 是否正确。

### `Master already in use!`

上一个进程没退干净。`sudo pkill -9 -f servo_op` 后重试。

### DC 报 PLL 错误

- `assign_activate` 试 `0x0100`（只 SYNC0）
- `shift_ns` 试 `cycle_ns/3` 或 `cycle_ns*2/3`
- 确认 `ecrt_master_application_time()` 每周期都调用，且用**目标时间**不是实测时间

### 网口被主站接管后掉网

`ec_generic` 会接管网口，该口的 IP 配置失效。如果这口还连着别的网络，换一个专用口。

### 帧率不是 1000/s

`ethercat master` 里的 `Tx frame rate` 应该接近 `1e9/cycle_ns`。
如果偏低，检查循环里是否有阻塞调用（SDO 读写、`ecrt_master_state()` 等）。

---

## 重新编译

完整流程见源码树根目录的 `BUILD.md`。简要：

```bash
cd /home/zachary/Project/ethercat-1.6.13
./bootstrap
./configure --enable-kernel --enable-generic --enable-hrtimer --enable-eoe \
    --disable-tty --disable-initd \
    --disable-8139too --disable-e100 --disable-e1000 --disable-e1000e \
    --disable-r8169 --disable-igb --disable-igc --disable-macb \
    --disable-genet --disable-stmmac-pci --disable-dwmac-intel
make -j$(nproc)
make modules
./make-package.sh
```

产物位置：

- `master/ec_master.ko`
- `devices/ec_generic.ko`
- `lib/.libs/libethercat.so*`
- `tool/ethercat`

---

## 参考

- 官方文档：<https://docs.etherlab.org/>
- 源码：<https://gitlab.com/etherlab.org/ethercat>
- `ecrt.h` 里有完整的 API 注释

---

## 本包相对原版的改动

### 1. FoE 密码支持

原版 IgH 1.6.13 的 FoE 密码字段是硬编码的：

- `master/fsm_foe.c` 的 `ec_foe_prepare_rrq_send()` 写死 `0x00000000`
- `ec_foe_prepare_wrq_send()` 误用了 `tx_packet_no`（恒为 0）

改动涉及 10 个文件：

| 文件 | 改动 |
|---|---|
| `master/foe_request.h` | `ec_foe_request_t` 加 `uint32_t password` |
| `master/foe_request.c` | 初始化 `password = 0` |
| `master/fsm_foe.h` | FSM 加 `tx_password` / `rx_password` |
| `master/fsm_foe.c` | 传输时从 request 取密码；RRQ/WRQ 帧写入密码；WRQ 加调试打印 |
| `master/ioctl.h` | `ec_ioctl_slave_foe_t` 加 `uint32_t password` |
| `master/ioctl.c` | 读写处理中 `request.password = io.password` |
| `tool/Command.h/.cpp` | 加 `password` 成员和 `setPassword()` |
| `tool/main.cpp` | 注册 `--password` / `-P` 选项 |
| `tool/CommandFoeRead.cpp` | 解析密码并填入 ioctl 结构 |
| `tool/CommandFoeWrite.cpp` | 同上 |

> `master/rtdm-ioctl.c` 是 `ioctl.c` 的符号链接，自动同步。

默认值仍为 0，**完全向后兼容**原版行为。

### 2. SII / EEPROM 用户态接口

原版只有 ioctl 和命令行工具，用户态库无封装。本包新增 2 个接口：

| 文件 | 改动 |
|---|---|
| `include/ecrt.h` | 新增 `ecrt_master_sii_read` / `ecrt_master_sii_write` 声明 |
| `lib/master.c` | 新增 2 个函数实现（走 `EC_IOCTL_SLAVE_SII_READ/WRITE`） |

接口总数：103 → **105**。

### 3. 启用 EoE

原构建配置用了 `--disable-eoe`，导致 6 个 `ecrt_slave_config_eoe_*` 函数不编译。
但 `ecrt.h` 里这些声明**没有** `#ifdef` 保护，所以链接会失败。

本包改为 `--enable-eoe`，105 个接口全部可用。

### 4. 新增示例

| 文件 | 说明 |
|---|---|
| `examples/servo_op.c` | 1ms 周期 + DC shift 500us，CiA402 自动使能到 OP |
| `examples/sii_test.c` | SII 读写验证 |
| `examples/reg_test.c` | 寄存器请求（`reg_request`）验证 |

---

## 相关文档

- `API-COMPARISON.md` — IgH vs SOEM 接口对比，双协议栈兼容层设计参考
  - 105 个 IgH 接口完整清单
  - 逐类对照表（初始化、扫描、PDO、SDO、DC、FoE/SoE/EoE/VoE、EEPROM、XML）
  - 兼容层最小可行集（15 个接口）
  - 5 个必须注意的语义差异坑
- 源码树根目录 `BUILD.md` — 完整编译与打包指南
