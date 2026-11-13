#!/bin/bash
# 卸载 IgH EtherCAT 主站模块

set -e

if [ "$(id -u)" -ne 0 ]; then
    echo "需要 root: sudo $0"
    exit 1
fi

echo "--- 卸载 ec_generic ---"
modprobe -r ec_generic 2>/dev/null || true

echo "--- 卸载 ec_master ---"
modprobe -r ec_master 2>/dev/null || true

echo "--- 剩余模块 ---"
lsmod | grep ec_ || echo "(无)"
