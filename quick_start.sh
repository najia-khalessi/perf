#!/bin/bash

# 快速启动脚本 - 性能分析工具
# 简化版本，适合日常快速使用

set -e

# 颜色定义
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${BLUE}🚀 性能分析工具 - 快速启动${NC}"
echo ""

# 检查可执行文件
if [[ ! -f "my_elf_reader" ]]; then
    echo -e "${YELLOW}⚠️  可执行文件不存在，正在构建...${NC}"
    ./start.sh build
    echo ""
fi

# 显示菜单
echo -e "${GREEN}请选择操作:${NC}"
echo "1) 🔨 构建项目"
echo "2) 📊 快速分析 (30秒, 100Hz)"
echo "3) 📈 深度分析 (60秒, 100Hz)"
echo "4) 🔍 查看现有数据"
echo "5) 🧹 清理旧数据"
echo "6) ❓ 显示帮助"
echo ""

read -p "请输入选项 (1-6): " choice

case $choice in
    1)
        echo -e "${BLUE}🔨 构建项目...${NC}"
        ./start.sh build
        ;;
    2)
        echo -e "${BLUE}📊 开始快速分析 (30秒)...${NC}"
        echo -e "${YELLOW}需要root权限，请输入密码:${NC}"
        sudo ./start.sh analysis 30 100
        ;;
    3)
        echo -e "${BLUE}📈 开始深度分析 (60秒)...${NC}"
        echo -e "${YELLOW}需要root权限，请输入密码:${NC}"
        sudo ./start.sh analysis 60 100
        ;;
    4)
        echo -e "${BLUE}🔍 查看现有数据...${NC}"
        if ls data/perf_*.db >/dev/null 2>&1; then
            latest_db=$(ls -t data/perf_*.db | head -1)
            echo -e "${GREEN}最新数据库: $latest_db${NC}"
            ./start.sh info "$latest_db"
            echo ""
            read -p "是否生成火焰图? (y/n): " gen_flame
            if [[ "$gen_flame" == "y" || "$gen_flame" == "Y" ]]; then
                ./start.sh query "$latest_db"
            fi
        else
            echo -e "${YELLOW}⚠️  没有找到数据库文件${NC}"
        fi
        ;;
    5)
        echo -e "${BLUE}🧹 清理旧数据...${NC}"
        read -p "清理多少天前的数据? (默认7天): " days
        days=${days:-7}
        ./start.sh cleanup "$days"
        ;;
    6)
        ./start.sh help
        ;;
    *)
        echo -e "${YELLOW}⚠️  无效选项${NC}"
        exit 1
        ;;
esac

echo ""
echo -e "${GREEN}✅ 操作完成!${NC}"

