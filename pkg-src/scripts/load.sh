#!/bin/bash
# 加载 IgH EtherCAT 主站模块
#
# 用法: sudo ./load.sh [网口名]
#   默认: enp173s0f0

set -e

IFACE="${1:-enp173s0f0}"
PKGDIR="$(cd "$(dirname "$0")/.." && pwd)"

if [ "$(id -u)" -ne 0 ]; then
    echo "需要 root: sudo $0 $*"
    exit 1
fi

if [ ! -e "/sys/class/net/$IFACE" ]; then
    echo "网口 $IFACE 不存在。可用:"
    ls /sys/class/net
    exit 1
fi

MAC=$(cat "/sys/class/net/$IFACE/address")
echo "=== 网口 $IFACE (MAC $MAC) ==="

echo "--- 卸载旧模块 ---"
modprobe -r ec_generic 2>/dev/null || true
modprobe -r ec_master 2>/dev/null || true

echo "--- 加载 ec_master ---"
insmod "$PKGDIR/modules/ec_master.ko" main_devices="$MAC"

echo "--- 加载 ec_generic ---"
insmod "$PKGDIR/modules/ec_generic.ko"

sleep 1

if [ ! -e /dev/EtherCAT0 ]; then
    echo "!! /dev/EtherCAT0 未创建"
    dmesg | tail -20
    exit 1
fi

echo "--- 主站状态 ---"
"$PKGDIR/bin/ethercat" master | head -8
echo
"$PKGDIR/bin/ethercat" slaves
