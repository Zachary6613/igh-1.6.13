#!/bin/bash
#------------------------------------------------------------------------------
# IgH EtherCAT Master 打包脚本
#
# 从已编译的源码树生成可分发的 igh-1.6.13-package 目录。
#
# 用法:
#   ./make-package.sh              # 打包到 ./igh-1.6.13-package
#   ./make-package.sh /tmp/out     # 打包到指定目录
#
# 前置条件: 已执行 make && make modules
#
# 源文件位置:
#   编译产物        -> master/ devices/ lib/.libs/ include/ tool/
#   示例/脚本/文档  -> pkg-src/
#------------------------------------------------------------------------------

set -e

SRCDIR="$(cd "$(dirname "$0")" && pwd)"
PKGDIR="${1:-$SRCDIR/igh-1.6.13-package}"
PKGSRC="$SRCDIR/pkg-src"

#------------------------------------------------------------------------------
# 检查编译产物
#------------------------------------------------------------------------------

echo "=== 检查编译产物 ==="

MISSING=0
check() {
    if [ ! -e "$1" ]; then
        echo "  ✗ 缺失: ${1#$SRCDIR/}"
        MISSING=1
    else
        echo "  ✓ ${1#$SRCDIR/}"
    fi
}

check "$SRCDIR/master/ec_master.ko"
check "$SRCDIR/devices/ec_generic.ko"
check "$SRCDIR/lib/.libs/libethercat.so.1.2.0"
check "$SRCDIR/lib/.libs/libethercat.a"
check "$SRCDIR/include/ecrt.h"
check "$SRCDIR/tool/ethercat"

if [ "$MISSING" -ne 0 ]; then
    echo
    echo "!! 产物不完整，请先编译（详见 BUILD.md）:"
    echo "   ./bootstrap"
    echo "   ./configure --enable-kernel --enable-generic --enable-hrtimer --enable-eoe \\"
    echo "       --disable-tty --disable-initd \\"
    echo "       --disable-8139too --disable-e100 --disable-e1000 --disable-e1000e \\"
    echo "       --disable-r8169 --disable-igb --disable-igc --disable-macb \\"
    echo "       --disable-genet --disable-stmmac-pci --disable-dwmac-intel"
    echo "   make -j\$(nproc)"
    echo "   make modules"
    exit 1
fi

#------------------------------------------------------------------------------
# 检查打包源
#------------------------------------------------------------------------------

echo
echo "=== 检查打包源 (pkg-src/) ==="

if [ ! -d "$PKGSRC" ]; then
    echo "  ✗ 缺失: pkg-src/ 目录"
    echo
    echo "!! pkg-src/ 存放示例源码、脚本和文档，是打包的输入。"
    echo "   如果丢失，需要从版本控制或备份恢复。"
    exit 1
fi

for f in "$PKGSRC"/examples/*.c "$PKGSRC"/scripts/*.sh "$PKGSRC"/*.md; do
    [ -e "$f" ] && echo "  ✓ ${f#$SRCDIR/}"
done

#------------------------------------------------------------------------------
# 创建目录结构
#------------------------------------------------------------------------------

echo
echo "=== 创建包目录: $PKGDIR ==="

rm -rf "$PKGDIR"
mkdir -p "$PKGDIR"/{modules,lib,include,bin,examples,scripts}

#------------------------------------------------------------------------------
# 复制编译产物
#------------------------------------------------------------------------------

echo "=== 复制编译产物 ==="

cp "$SRCDIR/master/ec_master.ko"          "$PKGDIR/modules/"
cp "$SRCDIR/devices/ec_generic.ko"        "$PKGDIR/modules/"
cp -a "$SRCDIR/lib/.libs/libethercat.so"* "$PKGDIR/lib/"
cp "$SRCDIR/lib/.libs/libethercat.a"      "$PKGDIR/lib/"
cp "$SRCDIR/include/ecrt.h"               "$PKGDIR/include/"
cp "$SRCDIR/tool/ethercat"                "$PKGDIR/bin/"

#------------------------------------------------------------------------------
# 复制打包源
#------------------------------------------------------------------------------

echo "=== 复制示例/脚本/文档 ==="

cp "$PKGSRC"/examples/*.c   "$PKGDIR/examples/" 2>/dev/null || true
cp "$PKGSRC"/scripts/*.sh   "$PKGDIR/scripts/"  2>/dev/null || true
cp "$PKGSRC"/*.md           "$PKGDIR/"          2>/dev/null || true

chmod +x "$PKGDIR/scripts/"*.sh 2>/dev/null || true

#------------------------------------------------------------------------------
# 编译示例
#------------------------------------------------------------------------------

echo "=== 编译示例 ==="

cd "$PKGDIR/examples"
for f in *.c; do
    [ -f "$f" ] || continue
    out="${f%.c}"
    echo "  CC $out"
    gcc -O2 -I../include -o "$out" "$f" \
        -L../lib -lethercat -lrt -lpthread \
        -Wl,-rpath,'$ORIGIN/../lib'
done

#------------------------------------------------------------------------------
# 汇总
#------------------------------------------------------------------------------

echo
echo "=== 完成 ==="
echo
echo "包位置: $PKGDIR"
echo "大小:   $(du -sh "$PKGDIR" | cut -f1)"
echo
echo "内容:"
find "$PKGDIR" -type f -o -type l | sed "s|$PKGDIR|  .|" | sort
echo
echo "使用:"
echo "  cd $PKGDIR"
echo "  sudo ./scripts/load.sh <网口名>"
echo "  sudo ./examples/servo_op"
