#!/bin/bash

# 编译项目
echo "正在编译项目..."
make clean && make

if [ $? -ne 0 ]; then
    echo "项目编译失败，请检查错误。"
    exit 1
fi

echo "项目编译成功。现在可以运行采集工具。"
echo ""

# 默认模式：监控所有进程，100Hz采样，寻找耗时最长的函数
echo "示例1: 标准性能分析 (默认模式，100Hz采样，详细输出)"
echo "sudo ./my_elf_reader --frequency=100 --verbose"
# sudo ./my_elf_reader --frequency=100 --verbose
echo ""

# 高精度调用栈分析 (LBR模式)
echo "示例2: 高精度调用栈分析 (LBR模式，100Hz采样，详细输出)"
echo "sudo ./my_elf_reader --frequency=100 --lbr --verbose"
# sudo ./my_elf_reader --frequency=100 --lbr --verbose
echo ""

# 只分析内核空间
echo "示例3: 只分析内核空间 (100Hz采样)"
echo "sudo ./my_elf_reader --frequency=100 --filter=kernel"
# sudo ./my_elf_reader --frequency=100 --filter=kernel
echo ""

# 只分析用户态，并启用LBR
echo "示例4: 只分析用户态，并启用LBR (100Hz采样)"
echo "sudo ./my_elf_reader --frequency=100 --filter=user --lbr"
# sudo ./my_elf_reader --frequency=100 --filter=user --lbr
echo ""

# 设置栈回溯深度为16
echo "示例5: 设置栈回溯深度为16 (默认模式，100Hz采样)"
echo "sudo ./my_elf_reader --frequency=100 --stack-depth=16 --verbose"
# sudo ./my_elf_reader --frequency=100 --stack-depth=16 --verbose
echo ""

echo "请根据需要取消注释并运行上述命令。"
echo "注意: 运行这些命令需要root权限 (sudo)。"
