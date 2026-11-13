# IgH EtherCAT Master vs SOEM 用户态接口对比

用于双协议栈兼容层设计。

- **IgH**：`ethercat-1.6.13/include/ecrt.h`，**105 个** `ecrt_*` 函数（全部有实现）
- **SOEM**：`SOEM-RTK/include/soem/*.h`，**100 个** `ec*` 函数

> 本包相对原版 IgH 新增了 2 个接口（`ecrt_master_sii_read/write`），
> 原版为 103 个。详见 [附录 A](#附录-a本包相对原版的改动)。

---

## 目录

- [零、IgH 接口完整性核对](#零igh-接口完整性核对)
- [一、架构差异](#一架构差异决定兼容层形态)
- [二、初始化与生命周期](#二初始化与生命周期)
- [三、总线扫描与从站信息](#三总线扫描与从站信息)
- [四、过程数据 PDO](#四过程数据pdo)
- [五、PDO 配置](#五pdo-配置映射修改)
- [六、SDO（CoE）](#六sdocoе)
- [七、分布式时钟（DC）](#七分布式时钟dc)
- [八、邮箱协议](#八邮箱协议)
- [九、紧急事件](#九紧急事件emergency)
- [十、底层寄存器与数据报](#十底层寄存器与数据报)
- [十一、EEPROM / SII](#十一eeprom--sii)
- [十二、XML / ESI](#十二xml--esi)
- [十三、邮箱底层](#十三邮箱底层)
- [十四、错误处理](#十四错误处理)
- [十五、数据类型转换](#十五数据类型转换)
- [十六、兼容层设计建议](#十六兼容层设计建议)
- [十七、能力矩阵总览](#十七能力矩阵总览)
- [十八、结论](#十八结论)
- [附录 A：本包相对原版的改动](#附录-a本包相对原版的改动)

---

## 零、IgH 接口完整性核对

`ecrt.h` 声明的函数与 `libethercat.so` 实际导出的符号**完全一致**：

```
头文件声明总数: 105
so 导出总数:   105
声明但未导出:   (无)
导出但未声明:   (无)
```

核对命令：

```bash
grep -oP 'EC_PUBLIC_API\s+.*?\K\becrt_[a-z_0-9]+(?=\s*\()' include/ecrt.h | sort -u > /tmp/decl.txt
nm -D --defined-only lib/.libs/libethercat.so.1.2.0 \
    | grep -oP '\becrt_[a-z_0-9]+' | sort -u > /tmp/exp.txt
comm -23 /tmp/decl.txt /tmp/exp.txt   # 声明但未实现
comm -13 /tmp/decl.txt /tmp/exp.txt   # 实现但未声明
```

### ⚠️ 编译开关会影响接口数量

部分接口受 `configure` 开关控制，**默认配置下会缺失**：

| 开关 | 影响的接口 | 数量 |
|---|---|---|
| `--enable-eoe` | `ecrt_slave_config_eoe_*` | 6 |
| `--enable-tty` | `ectty_*`（独立头文件 `ectty.h`） | — |

`--disable-eoe` 时，`lib/slave_config.c` 里 `#ifdef EC_EOE` 包裹的 6 个函数不编译：

```
ecrt_slave_config_eoe_ip_address
ecrt_slave_config_eoe_subnet_mask
ecrt_slave_config_eoe_default_gateway
ecrt_slave_config_eoe_dns_address
ecrt_slave_config_eoe_hostname
ecrt_slave_config_eoe_mac_address
```

**本包已启用 `--enable-eoe`**，105 个接口全部可用。

> 注意：`ecrt.h` 里这些函数**没有** `#ifdef` 保护，所以 `--disable-eoe` 时
> 编译能过但链接会失败（undefined reference）。这是 IgH 的一个设计缺陷。

### 完整 105 接口清单

按字母序排列。**加粗**为本包新增。

| # | 接口 | # | 接口 | # | 接口 |
|---|---|---|---|---|---|
| 1 | `ecrt_domain_data` | 36 | `ecrt_master_sync_reference_clock_to` | 71 | `ecrt_slave_config_flag` |
| 2 | `ecrt_domain_process` | 37 | `ecrt_master_sync_slave_clocks` | 72 | `ecrt_slave_config_idn` |
| 3 | `ecrt_domain_queue` | 38 | `ecrt_master_write_idn` | 73 | `ecrt_slave_config_pdo_assign_add` |
| 4 | `ecrt_domain_reg_pdo_entry_list` | 39 | `ecrt_open_master` | 74 | `ecrt_slave_config_pdo_assign_clear` |
| 5 | `ecrt_domain_size` | 40 | `ecrt_read_lreal` | 75 | `ecrt_slave_config_pdo_mapping_add` |
| 6 | `ecrt_domain_state` | 41 | `ecrt_read_real` | 76 | `ecrt_slave_config_pdo_mapping_clear` |
| 7 | `ecrt_master` | 42 | `ecrt_reg_request_data` | 77 | `ecrt_slave_config_pdos` |
| 8 | `ecrt_master_activate` | 43 | `ecrt_reg_request_read` | 78 | `ecrt_slave_config_reg_pdo_entry` |
| 9 | `ecrt_master_application_time` | 44 | `ecrt_reg_request_state` | 79 | `ecrt_slave_config_reg_pdo_entry_pos` |
| 10 | `ecrt_master_create_domain` | 45 | `ecrt_reg_request_write` | 80 | `ecrt_slave_config_sdo` |
| 11 | `ecrt_master_deactivate` | 46 | `ecrt_release_master` | 81 | `ecrt_slave_config_sdo16` |
| 12 | `ecrt_master_get_pdo` | 47 | `ecrt_request_master` | 82 | `ecrt_slave_config_sdo32` |
| 13 | `ecrt_master_get_pdo_entry` | 48 | `ecrt_sdo_request_data` | 83 | `ecrt_slave_config_sdo8` |
| 14 | `ecrt_master_get_slave` | 49 | `ecrt_sdo_request_data_size` | 84 | `ecrt_slave_config_state` |
| 15 | `ecrt_master_get_sync_manager` | 50 | `ecrt_sdo_request_index` | 85 | `ecrt_slave_config_state_timeout` |
| 16 | `ecrt_master_link_state` | 51 | `ecrt_sdo_request_read` | 86 | `ecrt_slave_config_sync_manager` |
| 17 | `ecrt_master_read_idn` | 52 | `ecrt_sdo_request_state` | 87 | `ecrt_slave_config_watchdog` |
| 18 | `ecrt_master_receive` | 53 | `ecrt_sdo_request_timeout` | 88 | `ecrt_soe_request_data` |
| 19 | `ecrt_master_reference_clock_time` | 54 | `ecrt_sdo_request_write` | 89 | `ecrt_soe_request_data_size` |
| 20 | `ecrt_master_reserve` | 55 | `ecrt_slave_config_complete_sdo` | 90 | `ecrt_soe_request_idn` |
| 21 | `ecrt_master_reset` | 56 | `ecrt_slave_config_create_reg_request` | 91 | `ecrt_soe_request_read` |
| 22 | `ecrt_master_scan_progress` | 57 | `ecrt_slave_config_create_sdo_request` | 92 | `ecrt_soe_request_state` |
| 23 | `ecrt_master_sdo_download` | 58 | `ecrt_slave_config_create_soe_request` | 93 | `ecrt_soe_request_timeout` |
| 24 | `ecrt_master_sdo_download_complete` | 59 | `ecrt_slave_config_create_voe_handler` | 94 | `ecrt_soe_request_write` |
| 25 | `ecrt_master_sdo_upload` | 60 | `ecrt_slave_config_dc` | 95 | `ecrt_version_magic` |
| 26 | `ecrt_master_select_reference_clock` | 61 | `ecrt_slave_config_emerg_clear` | 96 | `ecrt_voe_handler_data` |
| 27 | `ecrt_master_send` | 62 | `ecrt_slave_config_emerg_overruns` | 97 | `ecrt_voe_handler_data_size` |
| 28 | `ecrt_master_set_send_interval` | 63 | `ecrt_slave_config_emerg_pop` | 98 | `ecrt_voe_handler_execute` |
| 29 | **`ecrt_master_sii_read`** | 64 | `ecrt_slave_config_emerg_size` | 99 | `ecrt_voe_handler_read` |
| 30 | **`ecrt_master_sii_write`** | 65 | `ecrt_slave_config_eoe_default_gateway` | 100 | `ecrt_voe_handler_read_nosync` |
| 31 | `ecrt_master_slave_config` | 66 | `ecrt_slave_config_eoe_dns_address` | 101 | `ecrt_voe_handler_received_header` |
| 32 | `ecrt_master_state` | 67 | `ecrt_slave_config_eoe_hostname` | 102 | `ecrt_voe_handler_send_header` |
| 33 | `ecrt_master_sync_monitor_process` | 68 | `ecrt_slave_config_eoe_ip_address` | 103 | `ecrt_voe_handler_write` |
| 34 | `ecrt_master_sync_monitor_queue` | 69 | `ecrt_slave_config_eoe_mac_address` | 104 | `ecrt_write_lreal` |
| 35 | `ecrt_master_sync_reference_clock` | 70 | `ecrt_slave_config_eoe_subnet_mask` | 105 | `ecrt_write_real` |

---

## 一、架构差异（决定兼容层形态）

| 维度 | IgH | SOEM |
|---|---|---|
| 运行位置 | **内核模块** + 用户态库 | **纯用户态** |
| 访问方式 | `/dev/EtherCAT0` ioctl | raw socket（AF_PACKET） |
| 权限 | root（设备节点 0600） | root（raw socket） |
| 主站对象 | `ec_master_t *`（不透明句柄） | `ecx_contextt`（**公开结构体**，栈上分配） |
| 从站信息 | 通过 `ecrt_master_get_slave()` 查询 | 直接读 `context->slavelist[i]` |
| 过程数据 | `ec_domain_t` + 偏移量 | `context->slavelist[i].outputs/inputs` 指针 |
| 配置模型 | **声明式**：先注册 PDO，激活后拿偏移 | **命令式**：`config_init` → `config_map_group` 后直接读写 |
| 状态机 | 内核自动跑（INIT→OP） | 用户手动 `ecx_writestate` + `ecx_statecheck` |
| 周期循环 | 库内部线程 + 用户 `receive/send` | 完全由用户控制 |
| 多主站 | 支持（`ecrt_request_master(n)`） | 每 context 一个 |
| 冗余 | 支持（backup device） | 支持（`ecx_init_redundant`） |

**关键结论**：IgH 是"配置-激活-偏移"模型，SOEM 是"扫描-映射-指针"模型。兼容层必须做**语义转换**，不能简单改名。

---

## 二、初始化与生命周期

| 功能 | IgH | SOEM | 兼容难度 |
|---|---|---|---|
| 初始化 | `ecrt_request_master(0)` | `ecx_init(&ctx, ifname)` | 易 |
| 冗余初始化 | — | `ecx_init_redundant(&ctx, &red, if1, if2)` | 中 |
| 关闭 | `ecrt_release_master(m)` | `ecx_close(&ctx)` | 易 |
| 版本检查 | `ecrt_version_magic()` | — | 易 |
| 网卡枚举 | — | `ec_find_adapters()` / `ec_free_adapters()` | 易 |
| 激活 | `ecrt_master_activate(m)` | 无对应（映射后即可用） | **中** |
| 反激活 | `ecrt_master_deactivate(m)` | 无对应 | 易 |
| 复位 | `ecrt_master_reset(m)` | 无对应 | 中 |
| 预留 | `ecrt_master_reserve(m)` | 无对应 | 易 |

---

## 三、总线扫描与从站信息

| 功能 | IgH | SOEM |
|---|---|---|
| 扫描总线 | 内核自动（加载模块时） | `ecx_config_init(&ctx)` |
| 扫描进度 | `ecrt_master_scan_progress(m, &p)` | — |
| 从站数量 | `ecrt_master_state()` → `slaves_responding` | `ctx.slavecount` |
| 从站详情 | `ecrt_master_get_slave(m, i, &info)` | `ctx.slavelist[i]`（直接读） |
| 同步管理器 | `ecrt_master_get_sync_manager(m, i, sm, &sm_info)` | `ctx.slavelist[i].SM[]` |
| PDO 列表 | `ecrt_master_get_pdo(m, i, sm, pdo_pos, &pdo)` | `ecx_readPDOmap()` / `ecx_readPDOmapCA()` |
| PDO 条目 | `ecrt_master_get_pdo_entry(m, i, sm, pdo_pos, entry_pos, &e)` | 同上（填 `slavelist[i].pdo[]`） |
| 对象字典列表 | — | `ecx_readODlist()` |
| 对象字典描述 | — | `ecx_readODdescription()` |
| 对象描述 | — | `ecx_readOE()` / `ecx_readOEsingle()` |
| SII 读取 | 内核自动 | `ecx_siifind` / `ecx_siigetbyte` / `ecx_siistring` |
| SII 结构解析 | — | `ecx_siiFMMU` / `ecx_siiSM` / `ecx_siiSMnext` / `ecx_siiPDO` |
| ESI dump | — | `ecx_esidump()` |
| 状态读取 | `ecrt_master_state()` / `ecrt_slave_config_state()` | `ecx_readstate()` |
| 状态检查 | 内核自动 | `ecx_statecheck(&ctx, slave, req, timeout)` |
| 状态写入 | 内核自动 | `ecx_writestate(&ctx, slave)` |
| 从站恢复 | 内核自动 | `ecx_recover_slave()` / `ecx_reconfig_slave()` |

> **兼容层要点**：IgH 的 `ecrt_master_get_slave()` 返回 `ec_slave_info_t`，SOEM 的 `slavelist[i]` 是 `ec_slavet`。字段名和语义都不同，需要映射表。

---

## 四、过程数据（PDO）

| 功能 | IgH | SOEM |
|---|---|---|
| 创建域 | `ecrt_master_create_domain(m)` | 无（`config_map_group` 隐式） |
| 映射 PDO | — | `ecx_config_map_group(&ctx, &IOmap, group)` |
| 注册单个条目 | `ecrt_slave_config_reg_pdo_entry(sc, idx, sub, dom, &bit)` | 无（用 `slavelist[i].outputs/inputs` 指针） |
| 按位置注册 | `ecrt_slave_config_reg_pdo_entry_pos(...)` | 无 |
| 批量注册 | `ecrt_domain_reg_pdo_entry_list(dom, regs)` | 无 |
| 域大小 | `ecrt_domain_size(dom)` | `ctx.grouplist[g].digestsize` |
| 域数据指针 | `ecrt_domain_data(dom)` | `IOmap`（用户提供） |
| 域状态 | `ecrt_domain_state(dom, &st)` | `ctx.grouplist[g].docheckstate` |
| 处理接收 | `ecrt_domain_process(dom)` | 无（`ecx_receive_processdata` 内含） |
| 提交发送 | `ecrt_domain_queue(dom)` | 无（`ecx_send_processdata` 内含） |
| 发送 | `ecrt_master_send(m)` | `ecx_send_processdata(&ctx)` |
| 接收 | `ecrt_master_receive(m)` | `ecx_receive_processdata(&ctx, timeout)` |
| 分组发送 | — | `ecx_send_processdata_group(&ctx, g)` |
| 分组接收 | — | `ecx_receive_processdata_group(&ctx, g, timeout)` |
| 发送间隔 | `ecrt_master_set_send_interval(m, ns)` | 无 |

**这是最大的语义差异**：

```c
/* IgH：偏移量模型 */
uint8_t *pd = ecrt_domain_data(domain);
EC_WRITE_U16(pd + off_controlword, 0x000F);

/* SOEM：指针模型 */
uint16 *ctrl = (uint16 *)(ctx.slavelist[0].outputs);
*ctrl = 0x000F;
```

兼容层建议：统一用**偏移量**抽象，SOEM 侧把 `slavelist[i].outputs - IOmap` 算成偏移。

---

## 五、PDO 配置（映射修改）

| 功能 | IgH | SOEM |
|---|---|---|
| 设置完整 PDO 配置 | `ecrt_slave_config_pdos(sc, n, syncs)` | 无（依赖 ESI 默认映射） |
| 清空 PDO 分配 | `ecrt_slave_config_pdo_assign_clear(sc, sm)` | 无 |
| 添加 PDO 分配 | `ecrt_slave_config_pdo_assign_add(sc, sm, idx)` | 无 |
| 清空 PDO 映射 | `ecrt_slave_config_pdo_mapping_clear(sc, pdo)` | 无 |
| 添加 PDO 映射 | `ecrt_slave_config_pdo_mapping_add(sc, pdo, idx, sub, bits)` | 无 |
| 同步管理器配置 | `ecrt_slave_config_sync_manager(sc, sm, dir, mode, wd)` | 无 |
| 看门狗 | `ecrt_slave_config_watchdog(sc, div, time)` | 无 |
| 标志位 | `ecrt_slave_config_flag(sc, flag, set)` | 无 |

> **IgH 独有**。SOEM 只能改 ESI 里的默认映射（通过 `ecx_readPDOmap` 读，不能写）。
> 兼容层若需支持 PDO 重映射，SOEM 侧要自己发 CoE SDO 写 0x1C12/0x1C13/0x16xx/0x1Axx。

---

## 六、SDO（CoE）

| 功能 | IgH | SOEM |
|---|---|---|
| 同步读 | `ecrt_master_sdo_upload(m, slave, idx, sub, buf, size, &out, &abort)` | `ecx_SDOread(&ctx, slave, idx, sub, CA, &size, buf, timeout)` |
| 同步写 | `ecrt_master_sdo_download(m, slave, idx, sub, buf, size, &abort)` | `ecx_SDOwrite(&ctx, slave, idx, sub, CA, size, buf, timeout)` |
| 完整访问写 | `ecrt_master_sdo_download_complete(...)` | `ecx_SDOwrite(..., CA=TRUE, ...)` |
| 异步请求创建 | `ecrt_slave_config_create_sdo_request(sc, idx, sub, size)` | 无 |
| 异步读 | `ecrt_sdo_request_read(req)` | 无 |
| 异步写 | `ecrt_sdo_request_write(req)` | 无 |
| 请求状态 | `ecrt_sdo_request_state(req)` | 无 |
| 请求数据 | `ecrt_sdo_request_data(req)` / `_data_size(req)` | 无 |
| 设置索引 | `ecrt_sdo_request_index(req, idx, sub)` | 无 |
| 超时 | `ecrt_sdo_request_timeout(req, ms)` | 无 |
| 配置时写 SDO | `ecrt_slave_config_sdo8/16/32(sc, idx, sub, val)` | 无 |
| 配置时写任意长度 | `ecrt_slave_config_sdo(sc, idx, sub, data, size)` | 无 |
| 完整访问配置 | `ecrt_slave_config_complete_sdo(sc, idx, data, size)` | 无 |
| 错误码转字符串 | — | `ec_sdoerror2string(code)` |
| SDO 错误查询 | — | `ecx_SDOerror(&ctx, slave, idx, sub)` |
| RxPDO 重映射 | — | `ecx_RxPDO(&ctx, slave, pdo)` |
| TxPDO 重映射 | — | `ecx_TxPDO(&ctx, slave, pdo)` |

> **IgH 的异步 SDO 是独有优势**：可以在实时循环里发起 SDO 而不阻塞。
> SOEM 的 `ecx_SDOread/write` 是阻塞的，会破坏周期。

---

## 七、分布式时钟（DC）

| 功能 | IgH | SOEM |
|---|---|---|
| 配置 DC | `ecrt_slave_config_dc(sc, assign, c0, s0, c1, s1)` | `ecx_configdc(&ctx)` + `ecx_dcsync0(&ctx, slave, act, cycle, shift)` |
| SYNC0/1 同时 | 同上（一次调用） | `ecx_dcsync01(&ctx, slave, act, c0, s0, c1, s1)` |
| 应用时间 | `ecrt_master_application_time(m, ns)` | 无（用户自己算） |
| 参考时钟选择 | `ecrt_master_select_reference_clock(m, sc)` | 无（自动选第一个支持 DC 的） |
| 参考时钟时间 | `ecrt_master_reference_clock_time(m, &ns)` | 无 |
| 同步参考时钟 | `ecrt_master_sync_reference_clock(m)` | 无 |
| 同步到指定时间 | `ecrt_master_sync_reference_clock_to(m, ns)` | 无 |
| 同步从站时钟 | `ecrt_master_sync_slave_clocks(m)` | 无 |
| 同步监视入队 | `ecrt_master_sync_monitor_queue(m)` | 无 |
| 同步监视处理 | `ecrt_master_sync_monitor_process(m)` | 无 |
| LRW + DC | — | `ecx_LRWDC(...)` |

> **IgH 的 DC 是"主站驱动"**：主站负责把参考时钟同步到应用时间。
> **SOEM 的 DC 是"从站自治"**：只配置 SYNC0/1，主站不参与时钟同步。
> 兼容层若要求 DC 精度，SOEM 侧需要自己实现 `sync_reference_clock` 逻辑。

---

## 八、邮箱协议

### FoE（固件传输）

| 功能 | IgH | SOEM |
|---|---|---|
| 读文件 | `ethercat foe_read`（工具层） | `ecx_FOEread(&ctx, slave, name, password, &size, buf, timeout)` |
| 写文件 | `ethercat foe_write`（工具层） | `ecx_FOEwrite(&ctx, slave, name, password, &size, buf, timeout)` |
| 钩子 | — | `ecx_FOEdefinehook(&ctx, hook)` |

> **注意**：IgH 的 FoE **没有用户态库 API**，只有命令行工具（走 ioctl）。
> 本仓库已改造支持密码（`--password`）。
> SOEM 的 FoE 是库函数，**原生支持密码参数**。

### SoE（Sercos over EtherCAT）

| 功能 | IgH | SOEM |
|---|---|---|
| 同步读 | `ecrt_master_read_idn(m, slave, idn, buf, size, &out, &err)` | `ecx_SoEread(&ctx, slave, driveNo, idn, &size, buf, timeout)` |
| 同步写 | `ecrt_master_write_idn(m, slave, idn, buf, size, &err)` | `ecx_SoEwrite(&ctx, slave, driveNo, idn, size, buf, timeout)` |
| 异步请求 | `ecrt_slave_config_create_soe_request(sc, idn, size)` | 无 |
| 异步读/写 | `ecrt_soe_request_read/write(req)` | 无 |
| 请求状态/数据 | `ecrt_soe_request_state/data_size/idn/timeout` | 无 |
| 配置时写 IDN | `ecrt_slave_config_idn(sc, drive, idn, op, data, size)` | 无 |
| IDN 映射读取 | — | `ecx_readIDNmap(&ctx)` |
| 错误转字符串 | — | `ec_soeerror2string(code)` |

### EoE（Ethernet over EtherCAT）

| 功能 | IgH | SOEM |
|---|---|---|
| 设置 IP | `ecrt_slave_config_eoe_ip_address(sc, ip)` | `ecx_EOEsetIp(&ctx, slave, port, &ip, timeout)` |
| 设置子网掩码 | `ecrt_slave_config_eoe_subnet_mask(sc, mask)` | 同上（`ip` 结构含 mask） |
| 设置网关 | `ecrt_slave_config_eoe_default_gateway(sc, gw)` | 同上 |
| 设置 DNS | `ecrt_slave_config_eoe_dns_address(sc, dns)` | 同上 |
| 设置主机名 | `ecrt_slave_config_eoe_hostname(sc, name)` | 同上 |
| 设置 MAC | `ecrt_slave_config_eoe_mac_address(sc, mac)` | 同上 |
| 获取 IP | — | `ecx_EOEgetIp(&ctx, slave, port, &ip, timeout)` |
| 发送 | 内核自动 | `ecx_EOEsend(&ctx, slave, port, size, buf, timeout)` |
| 接收 | 内核自动 | `ecx_EOErecv(&ctx, slave, port, &size, buf, timeout)` |
| 分片接收 | — | `ecx_EOEreadfragment(...)` |
| 钩子 | — | `ecx_EOEdefinehook(&ctx, hook)` |

> **IgH 的 EoE 是内核态实现**（虚拟网口 `eoe0s0`），用户态只配置。
> **SOEM 的 EoE 是用户态实现**，需要自己收发分片。

### VoE（Vendor over EtherCAT）

| 功能 | IgH | SOEM |
|---|---|---|
| 创建 handler | `ecrt_slave_config_create_voe_handler(sc, size)` | 无 |
| 读/写 | `ecrt_voe_handler_read/write(h)` | 无 |
| 非同步读 | `ecrt_voe_handler_read_nosync(h)` | 无 |
| 执行 | `ecrt_voe_handler_execute(h)` | 无 |
| 数据/大小 | `ecrt_voe_handler_data/_data_size(h)` | 无 |
| 发送头 | `ecrt_voe_handler_send_header(h, vendor, type)` | 无 |
| 接收头 | `ecrt_voe_handler_received_header(h, &vendor, &type)` | 无 |

> **IgH 独有**。

---

## 九、紧急事件（Emergency）

| 功能 | IgH | SOEM |
|---|---|---|
| 设置队列大小 | `ecrt_slave_config_emerg_size(sc, size)` | 无 |
| 弹出事件 | `ecrt_slave_config_emerg_pop(sc, &msg)` | 无 |
| 清空 | `ecrt_slave_config_emerg_clear(sc)` | 无 |
| 溢出计数 | `ecrt_slave_config_emerg_overruns(sc)` | 无 |

> **IgH 独有**。SOEM 需要自己解析邮箱。

---

## 十、底层寄存器与数据报

| 功能 | IgH | SOEM |
|---|---|---|
| APRD | — | `ecx_APRD(...)` |
| APWR | — | `ecx_APWR(...)` |
| ARMW | — | `ecx_ARMW(...)` |
| FPRD | — | `ecx_FPRD(...)` |
| FPWR | — | `ecx_FPWR(...)` |
| FRMW | — | `ecx_FRMW(...)` |
| BRD | — | `ecx_BRD(...)` |
| BWR | — | `ecx_BWR(...)` |
| LRD | — | `ecx_LRD(...)` |
| LWR | — | `ecx_LWR(...)` |
| LRW | — | `ecx_LRW(...)` |
| LRW + DC | — | `ecx_LRWDC(...)` |
| 带宽度变体 | — | `ecx_APRDw/APWRw/FPRDw/FPWRw` |
| 自定义数据报 | — | `ecx_setupdatagram` / `ecx_adddatagram` |
| 寄存器请求（异步） | `ecrt_slave_config_create_reg_request(sc, size)` | 无 |
| 寄存器读/写 | `ecrt_reg_request_read/write(req)` | 无 |
| 寄存器请求状态 | `ecrt_reg_request_state(req)` | 无 |
| 寄存器请求数据 | `ecrt_reg_request_data(req)` | 无 |

> **SOEM 独有**：完整的数据报构造能力。IgH 把这些封装在内核里，用户态只能通过 `reg_request` 做异步读写。

**IgH 的 `reg_request` 用法**（异步，可在实时循环里用）：

```c
/* 配置阶段 */
ec_reg_request_t *reg = ecrt_slave_config_create_reg_request(sc, 16);

/* 激活后 */
ecrt_reg_request_read(reg, 0x0130, 2);        /* 发起读 AL Status */
while (ecrt_reg_request_state(reg) == EC_REQUEST_BUSY) { /* 等 */ }
uint8_t *data = ecrt_reg_request_data(reg);   /* 取数据 */
```

> ⚠️ `ec_request_state_t` 枚举值：`UNUSED=0, BUSY=1, SUCCESS=2, ERROR=3`。
> 注意 `SUCCESS` 是 **2** 不是 1，容易写错。

---

## 十一、EEPROM / SII

| 功能 | IgH | SOEM |
|---|---|---|
| SII 读（库 API） | ✅ `ecrt_master_sii_read`（本包新增） | `ecx_readeeprom` / `ecx_readeeprom1/2` / `ecx_readeepromAP/FP` |
| SII 写（库 API） | ✅ `ecrt_master_sii_write`（本包新增） | `ecx_writeeeprom` / `ecx_writeeepromAP/FP` |
| SII 读（工具） | ✅ `ethercat sii_read` | — |
| SII 写（工具） | ✅ `ethercat sii_write` | — |
| EEPROM 状态切换 | — | `ecx_eeprom2master` / `ecx_eeprom2pdi` |

### 本包新增的 2 个接口

原版 IgH 的 EEPROM 读写**只存在于内核 ioctl 和命令行工具**，用户态库完全没有封装。
本包已补充：

```c
int ecrt_master_sii_read(ec_master_t *master, uint16_t slave_position,
        uint16_t offset, uint32_t nwords, uint16_t *words);

int ecrt_master_sii_write(ec_master_t *master, uint16_t slave_position,
        uint16_t offset, uint32_t nwords, const uint16_t *words);
```

| 层级 | 读 | 写 |
|---|---|---|
| `ecrt.h` 用户态 API | ✅ `ecrt_master_sii_read` | ✅ `ecrt_master_sii_write` |
| `libethercat.so` 导出 | ✅ | ✅ |
| ioctl（`master/ioctl.h`） | ✅ `EC_IOCTL_SLAVE_SII_READ` | ✅ `EC_IOCTL_SLAVE_SII_WRITE` |
| 命令行工具 | ✅ `ethercat sii_read` | ✅ `ethercat sii_write` |

**参数说明**：

- `offset`：**word 偏移**（不是字节偏移），0 表示 SII 起始
- `nwords`：要读写的 word 数（1 word = 2 字节）
- `words`：`uint16_t` 数组，长度至少 `nwords`

**实测验证**（`examples/sii_test.c`）：

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

=== ecrt_master_sii_write 测试（写回相同内容）===
ecrt_master_sii_write() = 0
```

### SII 头部字段偏移

word 索引，与内核 `fsm_slave_scan.c` 一致：

| 字段 | word 偏移 | 长度 |
|---|---|---|
| PDI control | `0x00` | 1 word |
| ESC config | `0x01` | 1 word |
| Alias | `0x04` | 1 word |
| Vendor ID | `0x08` | 2 words |
| Product code | `0x0A` | 2 words |
| Revision number | `0x0C` | 2 words |
| Serial number | `0x0E` | 2 words |
| Boot RX mailbox offset | `0x14` | 1 word |
| Boot RX mailbox size | `0x15` | 1 word |
| Boot TX mailbox offset | `0x16` | 1 word |
| Boot TX mailbox size | `0x17` | 1 word |

> ⚠️ **写 SII 有风险**：内容无效会导致从站无法使用。务必先 `sii_read` 备份。
> SII 末尾有 CRC8 校验字，改字段后必须重算，否则从站可能拒绝加载。
> `ethercat sii_write` 工具会自动校验，但库接口不会。

---

## 十二、XML / ESI

| 功能 | IgH | SOEM |
|---|---|---|
| 从站 → XML（导出） | ✅ `ethercat xml` | — |
| XML/ESI → SII（导入） | ❌ | ❌ |
| ENI(XML) → C 代码 | ❌ | ✅ `scripts/eniconv.py` |

### IgH 只有单向导出

```bash
sudo ./bin/ethercat xml -p0 > slave.xml    # 从站 → XML
```

实测输出（Sanhua RJM_070S）：

```xml
<?xml version="1.0" ?>
<EtherCATInfo>
  <Vendor><Id>86</Id></Vendor>
  <Descriptions><Devices><Device>
    <Type ProductCode="#x00000102" RevisionNo="#x00000001">Sanhua_RJM_070S_1_1_3</Type>
    <Sm Enable="1" StartAddress="#x1000" ControlByte="#x26" DefaultSize="128" />
    <RxPdo Sm="2" Fixed="1" Mandatory="1">
      <Index>#x1600</Index>
      <Entry><Index>#x6040</Index><SubIndex>0</SubIndex><BitLen>16</BitLen></Entry>
      ...
```

### 没有反向能力（XML → SII 二进制）

| 检查项 | 结果 |
|---|---|
| `ethercat` 命令列表 | 只有 `xml`（导出），无 `xml2sii` / `sii_write -x` |
| 源码里的 XML 解析器 | ❌ 无（`grep xmlReadFile\|libxml\|tinyxml\|pugixml` → 空） |
| `configure.ac` 依赖 | ❌ 无 libxml 依赖 |
| `script/` 目录 | 只有 `ethercatctl`、`ifup-eoe.sh`、bash 补全 |

**SOEM 也没有**：`scripts/eniconv.py` 是 **ENI(XML) → C 代码**，不是 → SII 二进制：

```bash
python3 scripts/eniconv.py slave.xml > slave_config.c
```

**结论**：XML/ESI → SII 二进制转换，**两个协议栈都不提供**。

### 如果需要 XML → SII，可选方案

| 方案 | 说明 |
|---|---|
| **Beckhoff TwinCAT / SSC Tool** | 官方 ESI 工具，可生成 SII 二进制 |
| **ETG.2000 规范自己实现** | SII 是 word 数组 + CRC8，格式公开 |
| **`ethercat sii_write` + 手工构造** | 用本包新增的 `ecrt_master_sii_write()` 直接写 word 数组 |
| **从现有从站读回** | `sii_read` 备份 → 改字段 → `sii_write` 写回 |

最后一种最实用：如果只是改 Vendor ID / Product Code / Alias 等头部字段，
读回原 SII、改几个 word、写回即可，不需要完整 XML 解析。

```c
/* 示例：改 Vendor ID */
uint16_t sii[0x200];
ecrt_master_sii_read(master, 0, 0, 0x200, sii);
sii[0x08] = 0x0056;  /* Vendor ID 低 16 位 */
sii[0x09] = 0x0000;  /* Vendor ID 高 16 位 */
ecrt_master_sii_write(master, 0, 0, 0x200, sii);
```

---

## 十三、邮箱底层

| 功能 | IgH | SOEM |
|---|---|---|
| 邮箱发送 | 内核自动 | `ecx_mbxsend(&ctx, slave, &mbx, timeout)` |
| 邮箱接收 | 内核自动 | `ecx_mbxreceive(&ctx, slave, &mbx, timeout)` |
| 邮箱处理 | 内核自动 | `ecx_mbxhandler(&ctx, &mbx, size)` |
| 邮箱池初始化 | — | `ecx_initmbxpool` / `ecx_initmbxqueue` |
| 邮箱清空 | — | `ec_clearmbx` / `ecx_dropmbx` |
| 邮箱空判断 | — | `ecx_mbxempty` |
| 邮箱计数 | — | `ec_nextmbxcnt` |
| 获取邮箱 | — | `ecx_getmbx` |
| 循环邮箱 | — | `ecx_slavembxcyclic` |
| ENI 初始化命令 | — | `ecx_mbxENIinitcmds` |

---

## 十四、错误处理

| 功能 | IgH | SOEM |
|---|---|---|
| 错误转字符串 | — | `ecx_err2string(code)` |
| 错误列表转字符串 | — | `ecx_elist2string(&ctx)` |
| AL 状态码转字符串 | — | `ec_ALstatuscode2string(code)` |
| 邮箱错误转字符串 | — | `ec_mbxerror2string(code)` |
| SDO 错误转字符串 | — | `ec_sdoerror2string(code)` |
| SoE 错误转字符串 | — | `ec_soeerror2string(code)` |
| 是否错误 | — | `ecx_iserror(&ctx)` |
| 错误入队 | — | `ecx_pusherror(&ctx, &err)` |
| 错误出队 | — | `ecx_poperror(&ctx, &err)` |
| 丢包计数 | — | `ecx_packeterror(&ctx, slave, wkc, expected)` |

> **SOEM 的错误处理更完整**。IgH 的错误通过返回值 + `abort_code` 传递。

---

## 十五、数据类型转换

| 功能 | IgH | SOEM |
|---|---|---|
| 读 REAL | `ecrt_read_real(p)` | 无（用 `EC_READ_REAL` 宏） |
| 读 LREAL | `ecrt_read_lreal(p)` | 无（用 `EC_READ_LREAL` 宏） |
| 写 REAL | `ecrt_write_real(p, v)` | 无 |
| 写 LREAL | `ecrt_write_lreal(p, v)` | 无 |

---

## 十六、兼容层设计建议

### 16.1 抽象层接口（推荐）

```c
/* 统一句柄 */
typedef struct ec_stack ec_stack_t;

/* 生命周期 */
ec_stack_t *ec_stack_open(const char *ifname, ec_stack_type_t type);
void        ec_stack_close(ec_stack_t *s);

/* 扫描 */
int  ec_stack_scan(ec_stack_t *s);
int  ec_stack_slave_count(ec_stack_t *s);
int  ec_stack_slave_info(ec_stack_t *s, int i, ec_slave_info_t *out);

/* PDO 映射 */
int  ec_stack_map(ec_stack_t *s, uint8_t **pd, size_t *size);
int  ec_stack_reg_entry(ec_stack_t *s, int slave, uint16_t idx,
                        uint8_t sub, unsigned int *offset);

/* 周期 */
int  ec_stack_send(ec_stack_t *s);
int  ec_stack_receive(ec_stack_t *s, int timeout_us);

/* 状态 */
int  ec_stack_state(ec_stack_t *s, ec_state_t *out);
int  ec_stack_set_state(ec_stack_t *s, int slave, int state);

/* SDO */
int  ec_stack_sdo_read(ec_stack_t *s, int slave, uint16_t idx, uint8_t sub,
                       void *buf, size_t *size, uint32_t *abort);
int  ec_stack_sdo_write(ec_stack_t *s, int slave, uint16_t idx, uint8_t sub,
                        const void *buf, size_t size, uint32_t *abort);

/* DC */
int  ec_stack_dc_config(ec_stack_t *s, int slave, uint16_t assign,
                        uint32_t c0, int32_t s0, uint32_t c1, int32_t s1);
int  ec_stack_dc_app_time(ec_stack_t *s, uint64_t ns);

/* FoE */
int  ec_stack_foe_read(ec_stack_t *s, int slave, const char *name,
                       uint32_t password, void *buf, size_t *size);
int  ec_stack_foe_write(ec_stack_t *s, int slave, const char *name,
                        uint32_t password, const void *buf, size_t size);
```

### 16.2 各层实现要点

| 抽象接口 | IgH 实现 | SOEM 实现 |
|---|---|---|
| `open` | `ecrt_request_master(0)` | `ecx_init(&ctx, ifname)` |
| `scan` | 读 `ecrt_master_state()` | `ecx_config_init(&ctx)` |
| `map` | `create_domain` + `activate` + `domain_data` | `ecx_config_map_group(&ctx, &IOmap, 0)` |
| `reg_entry` | `ecrt_slave_config_reg_pdo_entry()` | 遍历 `slavelist[i].pdo[]` 算偏移 |
| `send` | `domain_queue` + `master_send` | `ecx_send_processdata(&ctx)` |
| `receive` | `master_receive` + `domain_process` | `ecx_receive_processdata(&ctx, t)` |
| `set_state` | 内核自动（`activate` 后） | `ecx_writestate` + `ecx_statecheck` |
| `sdo_read` | `ecrt_master_sdo_upload()` | `ecx_SDOread()` |
| `dc_config` | `ecrt_slave_config_dc()` | `ecx_dcsync0()` |
| `dc_app_time` | `ecrt_master_application_time()` | 空实现（SOEM 无此概念） |
| `foe_read` | ioctl（需自己封装） | `ecx_FOEread()` |

### 16.3 必须注意的坑

1. **PDO 偏移语义不同**
   - IgH：`ecrt_slave_config_reg_pdo_entry()` 返回**域内字节偏移**
   - SOEM：`slavelist[i].outputs` 是**绝对指针**
   - 统一方案：SOEM 侧算 `outputs - IOmap` 得到偏移

2. **状态机责任不同**
   - IgH：`activate()` 后内核自动推进到 OP
   - SOEM：必须手动 `writestate(OP)` + `statecheck(OP, timeout)`
   - 兼容层 `open()` 里要替 SOEM 做这件事

3. **DC 语义不同**
   - IgH：主站同步参考时钟到应用时间
   - SOEM：只配 SYNC0/1，主站不参与
   - 若项目依赖 DC 精度，SOEM 侧需自己实现同步逻辑

4. **SDO 阻塞性**
   - IgH：有异步 SDO（`sdo_request`），可在实时循环里用
   - SOEM：`ecx_SDOread/write` 阻塞，会破坏周期
   - 兼容层若在循环里用 SDO，SOEM 侧必须放独立线程

5. **FoE 接口层级不同**
   - IgH：只有命令行工具，无库 API
   - SOEM：有库函数
   - 兼容层 IgH 侧需要自己封装 ioctl

6. **EoE 实现位置不同**
   - IgH：内核虚拟网口
   - SOEM：用户态分片收发
   - 兼容层语义差异大，建议单独抽象

---

## 十七、能力矩阵总览

| 能力 | IgH | SOEM | 备注 |
|---|---|---|---|
| 基础 PDO 交换 | ✅ | ✅ | 语义不同 |
| 总线扫描 | ✅ 自动 | ✅ 手动 | |
| 从站信息查询 | ✅ | ✅ | 字段不同 |
| PDO 重映射 | ✅ | ⚠️ 需自己发 SDO | |
| 同步 SDO | ✅ | ✅ | |
| 异步 SDO | ✅ | ❌ | IgH 优势 |
| DC 配置 | ✅ | ✅ | |
| DC 主站同步 | ✅ | ❌ | IgH 优势 |
| FoE | ⚠️ 仅工具 | ✅ | |
| SoE | ✅ | ✅ | |
| EoE | ✅ 内核 | ✅ 用户态 | |
| VoE | ✅ | ❌ | IgH 独有 |
| Emergency | ✅ | ❌ | IgH 独有 |
| 底层数据报 | ❌ | ✅ | SOEM 优势 |
| EEPROM 直接读写 | ✅ 已补充 | ✅ 库 API | 本包新增 |
| XML/ESI → SII | ❌ | ❌ | 两边都没有 |
| 从站 → XML 导出 | ✅ `ethercat xml` | ❌ | IgH 独有 |
| ENI → C 代码 | ❌ | ✅ `eniconv.py` | SOEM 独有 |
| 对象字典浏览 | ❌ | ✅ | SOEM 优势 |
| 冗余 | ✅ | ✅ | |
| 多主站 | ✅ | ⚠️ 多 context | |
| 错误字符串化 | ❌ | ✅ | SOEM 优势 |
| 实时性 | ✅ 内核 | ⚠️ 用户态 | IgH 优势 |

---

## 十八、结论

**IgH 强在**：内核态实时性、异步 SDO、DC 主站同步、VoE、Emergency、声明式配置、从站 XML 导出。

**SOEM 强在**：纯用户态易部署、底层数据报完全可控、对象字典直接访问、错误处理完善、FoE 有库 API、ENI 转 C 代码。

**兼容层最小可行集**（覆盖 90% 伺服控制场景）：

```
open / close
scan / slave_count / slave_info
map / reg_entry
send / receive
state / set_state
sdo_read / sdo_write
dc_config / dc_app_time
foe_read / foe_write
```

这 15 个接口能覆盖 `servo_op` 示例的全部功能。其余能力按需增量添加。

---

## 附录 A：本包相对原版的改动

### A.1 FoE 密码支持

原版 IgH 1.6.13 的 FoE 密码字段是硬编码的：

- `master/fsm_foe.c` 的 `ec_foe_prepare_rrq_send()` 写死 `0x00000000`
- `ec_foe_prepare_wrq_send()` 误用了 `tx_packet_no`（恒为 0）

改动涉及 10 个文件：

| 文件 | 改动 |
|---|---|
| `master/foe_request.h` | `ec_foe_request_t` 加 `uint32_t password` |
| `master/foe_request.c` | 初始化 `password = 0` |
| `master/fsm_foe.h` | FSM 加 `tx_password` / `rx_password` |
| `master/fsm_foe.c` | 传输时取密码；RRQ/WRQ 帧写入密码；WRQ 加调试打印 |
| `master/ioctl.h` | `ec_ioctl_slave_foe_t` 加 `uint32_t password` |
| `master/ioctl.c` | `request.password = io.password` |
| `tool/Command.h/.cpp` | 加 `password` 成员和 `setPassword()` |
| `tool/main.cpp` | 注册 `--password` / `-P` 选项 |
| `tool/CommandFoeRead.cpp` | 解析密码填入 ioctl |
| `tool/CommandFoeWrite.cpp` | 同上 |

> `master/rtdm-ioctl.c` 是 `ioctl.c` 的符号链接，自动同步。

默认值仍为 0，**完全向后兼容**原版行为。

用法：

```bash
sudo ./bin/ethercat foe_write -p0 --password 0x12345678 firmware.bin
sudo ./bin/ethercat foe_read  -p0 --password 0x12345678 firmware.bin > out.bin
```

### A.2 SII / EEPROM 用户态接口

原版只有 ioctl 和命令行工具，用户态库无封装。本包新增 2 个接口：

| 文件 | 改动 |
|---|---|
| `include/ecrt.h` | 新增 `ecrt_master_sii_read` / `ecrt_master_sii_write` 声明 |
| `lib/master.c` | 新增 2 个函数实现（走 `EC_IOCTL_SLAVE_SII_READ/WRITE`） |

接口总数：103 → **105**。

### A.3 构建配置

```bash
./configure --enable-kernel --enable-generic --enable-hrtimer --enable-eoe \
    --disable-tty --disable-initd \
    --disable-8139too --disable-e100 --disable-e1000 --disable-e1000e \
    --disable-r8169 --disable-igb --disable-igc --disable-macb \
    --disable-genet --disable-stmmac-pci --disable-dwmac-intel
```

关键点：

- `--enable-eoe`：否则 6 个 EoE 接口不编译，但 `ecrt.h` 仍声明 → 链接失败
- `--enable-generic`：通用网卡驱动（内核 socket 收发）
- `--enable-hrtimer`：高精度定时器
- 其余设备驱动全关，省编译时间

### A.4 新增示例

| 文件 | 说明 |
|---|---|
| `examples/servo_op.c` | 1ms 周期 + DC shift 500us，CiA402 自动使能到 OP |
| `examples/sii_test.c` | SII 读写验证 |
| `examples/reg_test.c` | 寄存器请求（`reg_request`）验证 |
