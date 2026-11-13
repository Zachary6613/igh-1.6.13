# 编译与打包指南

从源码编译 IgH EtherCAT Master 1.6.13 并生成可分发的 `igh-1.6.13-package`。

---

## 环境要求

| 项目 | 要求 | 检查命令 |
|---|---|---|
| 内核源码 | 与运行内核版本一致 | `ls /lib/modules/$(uname -r)/build` |
| 内核头文件 | 已安装 | `ls /lib/modules/$(uname -r)/build/include/generated/utsrelease.h` |
| autoconf | ≥ 2.60 | `autoconf --version` |
| automake | ≥ 1.10 | `automake --version` |
| libtool | 任意 | `libtoolize --version` |
| gcc / make | 任意 | `gcc --version` |

本包实测环境：

```
内核: 6.14.0-rt3 (PREEMPT_RT)
CPU:  x86_64
发行版: Ubuntu/Debian 系
```

> **注意**：内核模块的 `vermagic` 必须与运行内核完全匹配。
> 换内核后必须重新编译，否则 `insmod` 会报 `Invalid module format`。

---

## 一、完整编译流程

### 1. 生成 configure

```bash
cd /home/zachary/Project/ethercat-1.6.13
./bootstrap
```

这一步会：

- 创建 `ChangeLog`（空文件，autotools 要求）
- 创建 `m4/` 目录
- 运行 `autoreconf -i`，生成 `configure`、`Makefile.in`、`config.h.in` 等

产物：`configure`（约 680KB）、`aclocal.m4`、`autoconf/`、`m4/`

### 2. 配置

```bash
./configure \
    --enable-kernel \
    --enable-generic \
    --enable-hrtimer \
    --enable-eoe \
    --disable-tty \
    --disable-initd \
    --disable-8139too \
    --disable-e100 \
    --disable-e1000 \
    --disable-e1000e \
    --disable-r8169 \
    --disable-igb \
    --disable-igc \
    --disable-macb \
    --disable-genet \
    --disable-stmmac-pci \
    --disable-dwmac-intel \
    --prefix=/usr/local
```

#### 关键选项说明

| 选项 | 作用 | 为什么需要 |
|---|---|---|
| `--enable-kernel` | 编译内核模块 | **必需**，否则没有 `ec_master.ko` |
| `--enable-generic` | 通用网卡驱动 | 用内核 socket 收发，兼容任意网卡 |
| `--enable-hrtimer` | 高精度定时器 | 提升周期精度 |
| `--enable-eoe` | EoE 支持 | **必需**，否则 6 个 EoE 接口不编译但头文件仍声明 → 链接失败 |
| `--disable-tty` | 关闭 TTY 仿真 | 用不到，省编译时间 |
| `--disable-initd` | 不生成 init 脚本 | 手动加载模块 |
| `--disable-<驱动>` | 关闭专用网卡驱动 | 只用 generic，其余全关省时间 |

> **为什么必须 `--enable-eoe`**：
> `lib/slave_config.c` 里 6 个 `ecrt_slave_config_eoe_*` 函数被 `#ifdef EC_EOE` 包裹，
> 但 `include/ecrt.h` 里的声明**没有** `#ifdef` 保护。
> `--disable-eoe` 时编译能过，但用户程序链接会报 `undefined reference`。
> 这是 IgH 的设计缺陷。

#### 内核源码路径

`configure` 会自动从 `/lib/modules/$(uname -r)/build` 解析。
如果失败，手动指定：

```bash
./configure --with-linux-dir=/path/to/linux-source ...
```

本机实测解析到 `/home/zachary/Downloads/linux-6.14`（RT 内核源码树）。

### 3. 编译用户态

```bash
make -j$(nproc)
```

产物：

| 文件 | 说明 |
|---|---|
| `lib/.libs/libethercat.so.1.2.0` | 用户态库（动态） |
| `lib/.libs/libethercat.a` | 用户态库（静态） |
| `tool/ethercat` | 命令行工具 |
| `examples/dc_user/ec_dc_user_example` | 官方 DC 示例 |
| `examples/user/ec_user_example` | 官方用户示例 |

### 4. 编译内核模块

```bash
make modules
```

产物：

| 文件 | 说明 |
|---|---|
| `master/ec_master.ko` | 主站内核模块（约 813KB） |
| `devices/ec_generic.ko` | 通用网卡驱动（约 23KB） |
| `examples/mini/ec_mini.ko` | 最小示例模块 |

> `make` 不会自动编译内核模块，**必须单独执行 `make modules`**。

### 5. 验证产物

```bash
# 检查模块 vermagic 是否匹配
modinfo master/ec_master.ko | grep vermagic
# 应输出: vermagic: 6.14.0-rt3 SMP preempt_rt mod_unload modversions

# 检查接口完整性
grep -oP 'EC_PUBLIC_API\s+.*?\K\becrt_[a-z_0-9]+(?=\s*\()' include/ecrt.h | sort -u | wc -l
# 应输出: 105

nm -D --defined-only lib/.libs/libethercat.so.1.2.0 | grep -oP '\becrt_[a-z_0-9]+' | sort -u | wc -l
# 应输出: 105
```

---

## 二、生成 package

### 一键打包

```bash
./make-package.sh
```

输出到 `./igh-1.6.13-package/`。

指定输出目录：

```bash
./make-package.sh /tmp/igh-pkg
```

### 打包源目录 `pkg-src/`

打包脚本的输入分两部分：

| 来源 | 内容 |
|---|---|
| 编译产物 | `master/ec_master.ko`、`devices/ec_generic.ko`、`lib/.libs/`、`include/ecrt.h`、`tool/ethercat` |
| `pkg-src/` | 示例源码、脚本、文档 |

`pkg-src/` 结构：

```
pkg-src/
├── README.md              使用指南（打包后放到包根目录）
├── API-COMPARISON.md      IgH vs SOEM 接口对比
├── examples/
│   ├── servo_op.c         1ms 周期 + DC shift 500us
│   ├── sii_test.c         SII/EEPROM 读写验证
│   └── reg_test.c         寄存器请求验证
└── scripts/
    ├── load.sh            加载模块
    ├── unload.sh          卸载模块
    └── build.sh           编译示例
```

> **为什么单独放 `pkg-src/`**：打包脚本会 `rm -rf` 目标包目录。
> 如果源文件放在包目录里，会被一起删掉。`pkg-src/` 是独立输入，不受影响。

### 脚本做的事

1. **检查编译产物**：确认 6 个关键文件存在，缺失则提示编译命令并退出
2. **检查打包源**：确认 `pkg-src/` 存在
3. **创建目录**：`modules/ lib/ include/ bin/ examples/ scripts/`
4. **复制编译产物**：
   - `master/ec_master.ko` → `modules/`
   - `devices/ec_generic.ko` → `modules/`
   - `lib/.libs/libethercat.so*` + `.a` → `lib/`
   - `include/ecrt.h` → `include/`
   - `tool/ethercat` → `bin/`
5. **复制打包源**：示例 `.c`、脚本 `.sh`、文档 `.md`
6. **编译示例**：用包内的 `lib/` 和 `include/` 编译，带 `rpath`

### 包结构

```
igh-1.6.13-package/
├── README.md              使用指南
├── API-COMPARISON.md      IgH vs SOEM 接口对比
├── modules/
│   ├── ec_master.ko       主站内核模块
│   └── ec_generic.ko      通用网卡驱动
├── lib/
│   ├── libethercat.so.1.2.0
│   ├── libethercat.so.1       -> libethercat.so.1.2.0
│   ├── libethercat.so         -> libethercat.so.1.2.0
│   └── libethercat.a
├── include/
│   └── ecrt.h
├── bin/
│   └── ethercat
├── examples/
│   ├── servo_op.c / servo_op
│   ├── sii_test.c / sii_test
│   └── reg_test.c / reg_test
└── scripts/
    ├── load.sh
    ├── unload.sh
    └── build.sh
```

---

## 三、清理编译产物

### 保留源码，清理编译结果

```bash
make distclean
```

清理内容：

- 所有 `.o`、`.lo`、`.ko`、`.mod.c`、`.cmd` 文件
- `Makefile`、`config.h`、`config.status`、`config.log`
- `lib/.libs/`、`tool/ethercat`、`examples/*/ec_*_example`
- `autom4te.cache/`

**保留**：`configure`、`aclocal.m4`、`autoconf/`、`m4/`、`Makefile.in`

> `make distclean` 后可以直接 `./configure` 重新配置，不需要再 `./bootstrap`。

### 完全恢复到原始状态

```bash
make maintainer-clean
rm -rf autom4te.cache autoconf m4 aclocal.m4 configure config.h.in \
       Makefile.in */Makefile.in */*/Makefile.in ChangeLog
```

**保留**：`configure.ac`、`Makefile.am`、`bootstrap`、源码 `.c/.h`

> 之后需要重新 `./bootstrap`。

### 清理后重新编译

```bash
# distclean 之后
./configure --enable-kernel --enable-generic --enable-hrtimer --enable-eoe \
    --disable-tty --disable-initd \
    --disable-8139too --disable-e100 --disable-e1000 --disable-e1000e \
    --disable-r8169 --disable-igb --disable-igc --disable-macb \
    --disable-genet --disable-stmmac-pci --disable-dwmac-intel
make -j$(nproc)
make modules
./make-package.sh
```

### ⚠️ `make distclean` 不清理内核模块产物

`make distclean` 只清理 autotools 的产物，**内核模块的 `.o`、`.ko`、`.mod.c`、`.cmd` 会残留**。

完整清理需要额外执行：

```bash
make distclean
find . \( -name "*.o" -o -name "*.ko" -o -name "*.mod" -o -name "*.mod.c" \
    -o -name ".*.cmd" -o -name "modules.order" -o -name "Module.symvers" \) \
    -not -path "./igh-1.6.13-package/*" -delete
rm -rf autom4te.cache
```

清理前后对比（本机实测）：

| 状态 | 大小 |
|---|---|
| 编译完成 | 136 MB |
| `make distclean` 后 | 108 MB |
| 完整清理后 | 98 MB |

> 剩余 98MB 主要是 `devices/` 下的原始驱动源码（各内核版本的网卡驱动，84MB），
> 这些是源码不是产物，**不要删**。

---

## 四、常见问题

### `configure` 报找不到内核源码

```
Failed to find Linux sources. Use --with-linux-dir!
```

原因：`/lib/modules/$(uname -r)/build` 不存在或不是符号链接。

解决：

```bash
# 检查
ls -la /lib/modules/$(uname -r)/build

# 如果缺失，安装内核头文件
sudo apt install linux-headers-$(uname -r)

# 或手动指定源码树
./configure --with-linux-dir=/path/to/linux-source ...
```

### `make modules` 报 `vermagic` 不匹配

```
ERROR: modpost: "xxx" [.../ec_master.ko] undefined!
```

原因：内核源码版本与运行内核不一致。

解决：确认 `--with-linux-dir` 指向的源码树版本与 `uname -r` 一致。

### 链接时报 `undefined reference to ecrt_slave_config_eoe_*`

原因：`configure` 时用了 `--disable-eoe`。

解决：重新配置加 `--enable-eoe`，然后 `make clean && make`。

### `insmod` 报 `Invalid module format`

原因：模块编译时的内核版本与当前运行内核不同（换过内核）。

解决：重新编译。

```bash
make modules
```

### `make distclean` 后 `./configure` 报 `configure: No such file`

原因：`distclean` 不会删除 `configure`，但如果之前跑过 `maintainer-clean` 就会删掉。

解决：

```bash
./bootstrap
```

---

## 五、快速参考

```bash
# 完整流程（从零开始）
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

# 只重新编译（已 configure 过）
make -j$(nproc) && make modules && ./make-package.sh

# 清理后重编
make distclean
./configure ...（同上）
make -j$(nproc) && make modules && ./make-package.sh
```
