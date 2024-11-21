#!/bin/sh

if [ -z "$1" ]; then
    echo "usage: $0 <libaxcl_xxx.so directory>"
    exit 1
fi

lib_dir="$1"

# 查找所有符合条件的.so文件
so_files=$(find "$lib_dir" -name "libaxcl_*.so")

if [ -z "$so_files" ]; then
    echo "[fail] no libaxcl_xxx.so files found in $lib_dir"
    exit 0
fi

found_count=0
total_files=0

# 使用for循环处理找到的文件
for so_file in $so_files; do
    total_files=$((total_files + 1))
    if strings "$so_file" | grep -q "\[AXCL version\]:"; then
        found_count=$((found_count + 1))
    else
        echo "[fail] $so_file does not contain the '[AXCL version]:' keyword"
    fi
done

if [ "$found_count" -eq "$total_files" ]; then
    echo "libaxcl_xxx.so version check pass"
    exit 1
else
    exit 0
fi
