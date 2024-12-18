#!/bin/bash

# 检查输入参数
if [ -z "$1" ]; then
    echo "Usage: $0 <number of streams>"
    exit 1
fi

# 获取参数
NUM=$1  # 表示每个设备的循环次数
shift   # 移除第一个参数，保留其他参数

# 清理旧的文件（如有）
rm -f *.dump.pid*

# 设置动态库路径
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/lib/axcl/ffmpeg

# 获取PCI设备列表
PCI_IDS=($(lspci | grep 650 | cut -d ":" -f 1))

# 遍历PCI设备
for PCI_ID in "${PCI_IDS[@]}"; do
    PCI_DECIMAL=$((16#$PCI_ID))  # 将16进制转为10进制
    echo "Launching streams for PCI ID: $PCI_DECIMAL"

    # 循环启动指定次数的流
    for ((i = 1; i <= NUM; i++)); do
        RTMP_URL="rtmp://192.168.0.134:1935/live/pci${PCI_DECIMAL}_stream${i}"
        echo "Launching stream $i for PCI ID $PCI_DECIMAL to $RTMP_URL"
        ./axcl_sample_transcode "$@" -i ./test.mp4 --loop 1 -d $PCI_DECIMAL -o $RTMP_URL &
    done
done

# 等待所有后台任务完成
wait
echo "All processes have finished."

