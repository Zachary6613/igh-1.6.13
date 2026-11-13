#!/bin/bash
# 编译示例程序
#
# 用法: ./build.sh

set -e

PKGDIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PKGDIR/examples"

echo "=== 编译 servo_op ==="
gcc -O2 -I../include -o servo_op servo_op.c \
    -L../lib -lethercat -lrt -lpthread \
    -Wl,-rpath,'$ORIGIN/../lib'

echo "=== 完成: $PKGDIR/examples/servo_op ==="
ldd servo_op | grep ethercat
