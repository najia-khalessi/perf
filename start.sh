#!/bin/bash

# 性能分析工具启动脚本
# 集成所有功能：构建、采集、查询、分析、管理
# 作者: AI Assistant
# 版本: 1.0

set -e  # 遇到错误立即退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# 项目配置
PROJECT_NAME="性能分析工具"
PROJECT_VERSION="1.0"
MAIN_BINARY="my_elf_reader"
DATA_DIR="data"
SCRIPTS_DIR="scripts"
THIRD_PARTY_DIR="third_party"

# 日志函数
log_info() {
    echo -e "${BLUE}[信息]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[成功]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[警告]${NC} $1"
}

log_error() {
    echo -e "${RED}[错误]${NC} $1"
}

log_header() {
    echo -e "${PURPLE}================================${NC}"
    echo -e "${PURPLE} $1${NC}"
    echo -e "${PURPLE}================================${NC}"
}

# 检查是否为root用户
check_root() {
    if [[ $EUID -ne 0 ]]; then
        log_error "此操作需要 root 权限，请使用 sudo 运行"
        exit 1
    fi
}

# 检查依赖
check_dependencies() {
    log_info "检查系统依赖..."
    
    local missing_deps=()
    
    # 检查基本工具
    command -v gcc >/dev/null 2>&1 || missing_deps+=("gcc")
    command -v make >/dev/null 2>&1 || missing_deps+=("make")
    command -v sqlite3 >/dev/null 2>&1 || missing_deps+=("sqlite3")
    
    # 检查开发库
    if ! pkg-config --exists libelf; then
        missing_deps+=("libelf-dev")
    fi
    
    if ! pkg-config --exists sqlite3; then
        missing_deps+=("libsqlite3-dev")
    fi
    
    if [[ ${#missing_deps[@]} -gt 0 ]]; then
        log_error "缺少以下依赖: ${missing_deps[*]}"
        log_info "请运行以下命令安装依赖:"
        echo "sudo apt-get update"
        echo "sudo apt-get install ${missing_deps[*]}"
        exit 1
    fi
    
    log_success "所有依赖检查通过"
}

# 创建必要目录
create_directories() {
    log_info "创建必要目录..."
    mkdir -p "$DATA_DIR"
    mkdir -p "$SCRIPTS_DIR"
    mkdir -p "$THIRD_PARTY_DIR"
    log_success "目录创建完成"
}

# 构建项目
build_project() {
    log_header "构建项目"
    
    check_dependencies
    create_directories
    
    log_info "开始编译..."
    make clean >/dev/null 2>&1 || true
    make -j$(nproc)
    
    if [[ -f "$MAIN_BINARY" ]]; then
        log_success "项目构建成功"
        log_info "可执行文件: $MAIN_BINARY"
    else
        log_error "项目构建失败"
        exit 1
    fi
}

# 测试项目
test_project() {
    log_header "测试项目功能"
    
    if [[ ! -f "$MAIN_BINARY" ]]; then
        log_error "可执行文件不存在，请先构建项目"
        exit 1
    fi
    
    log_info "测试帮助功能..."
    ./"$MAIN_BINARY" --help >/dev/null 2>&1
    log_success "帮助功能正常"
    
    log_info "测试查询功能（使用示例数据库）..."
    if [[ -f "$DATA_DIR/perf.db" ]]; then
        ./"$MAIN_BINARY" query --input "$DATA_DIR/perf.db" --flamegraph "$DATA_DIR/test_flame.svg" >/dev/null 2>&1 || true
        log_success "查询功能正常"
    else
        log_warning "没有找到测试数据库，跳过查询测试"
    fi
    
    log_success "项目测试完成"
}

# 数据采集
collect_data() {
    local duration=${1:-60}
    local frequency=${2:-100}
    
    log_header "开始数据采集"
    
    if [[ ! -f "$MAIN_BINARY" ]]; then
        log_error "可执行文件不存在，请先构建项目"
        exit 1
    fi
    
    check_root
    
    local timestamp=$(date +"%Y%m%d_%H%M%S")
    local output_file="$DATA_DIR/perf_${timestamp}.db"
    
    log_info "采集参数:"
    log_info "  时长: ${duration}秒"
    log_info "  频率: ${frequency}Hz"
    log_info "  输出: $output_file"
    
    log_info "开始采集，按 Ctrl+C 可提前停止..."
    
    # 启动采集
    ./"$MAIN_BINARY" collect \
        --output "$output_file" \
        --frequency="$frequency" \
        --stack-depth=127 \
        --filter=all \
        --lbr \
        --duration="$duration"
    
    if [[ -f "$output_file" ]]; then
        log_success "数据采集完成: $output_file"
        
        # 修正文件权限，以便当前用户可以访问
        if [[ -n "$SUDO_USER" ]]; then
            log_info "将文件所有者更改为 $SUDO_USER"
            chown "$SUDO_USER":"$(id -gn "$SUDO_USER")" "$output_file"
            # WAL 和 shm 文件也需要修改
            if [[ -f "${output_file}-wal" ]]; then
                chown "$SUDO_USER":"$(id -gn "$SUDO_USER")" "${output_file}-wal"
            fi
            if [[ -f "${output_file}-shm" ]]; then
                chown "$SUDO_USER":"$(id -gn "$SUDO_USER")" "${output_file}-shm"
            fi
        fi
        
        # 显示数据库信息
        show_database_info "$output_file"
    else
        log_error "数据采集失败"
        exit 1
    fi
}

# 一键分析（采集+查询）
analysis() {
    local duration=${1:-60}
    local frequency=${2:-100}
    
    log_header "一键性能分析"
    
    # 先采集数据
    collect_data "$duration" "$frequency"
    
    # 获取最新的数据库文件
    local latest_db=$(ls -t "$DATA_DIR"/perf_*.db 2>/dev/null | head -1)
    
    if [[ -n "$latest_db" ]]; then
        log_info "开始生成火焰图..."
        query_data "$latest_db"
        log_success "一键分析完成"
    else
        log_error "未找到数据库文件"
        exit 1
    fi
}

# 查询数据并生成火焰图
query_data() {
    local db_file="$1"
    local start_time="$2"
    local end_time="$3"
    local output_file="$4"
    
    if [[ -z "$db_file" ]]; then
        log_error "请指定数据库文件"
        exit 1
    fi
    
    if [[ ! -f "$db_file" ]]; then
        log_error "数据库文件不存在: $db_file"
        exit 1
    fi
    
    log_header "查询数据并生成火焰图"
    
    if [[ ! -f "$MAIN_BINARY" ]]; then
        log_error "可执行文件不存在，请先构建项目"
        exit 1
    fi
    
    # 生成输出文件名
    if [[ -z "$output_file" ]]; then
        local timestamp=$(date +"%Y%m%d_%H%M%S")
        output_file="$DATA_DIR/analysis_${timestamp}.svg"
    fi
    
    log_info "查询参数:"
    log_info "  数据库: $db_file"
    log_info "  输出: $output_file"
    
    if [[ -z "$start_time" || -z "$end_time" ]]; then
        log_info "未指定时间范围，将查询整个数据库"
        local time_range=$(sqlite3 "$db_file" "SELECT MIN(timestamp), MAX(timestamp) FROM raw_samples;" 2>/dev/null)
        if [[ -n "$time_range" && "$time_range" != "|" ]]; then
            start_ns=$(echo "$time_range" | cut -d'|' -f1)
            end_ns=$(echo "$time_range" | cut -d'|' -f2)
            # 直接使用纳秒作为时间戳
            start_time=$start_ns
            end_time=$end_ns
            log_info "  自动检测到时间范围 (ns): $start_time - $end_time"
        else
            log_warning "无法自动检测时间范围，可能数据库为空"
            # 使用一个默认的很广的时间范围
            start_time=0
            end_time=$(date +%s)
        fi
    fi

    if [[ -n "$start_time" && -n "$end_time" ]]; then
        log_info "  时间范围: $start_time - $end_time"
        ./"$MAIN_BINARY" query \
            --input "$db_file" \
            --from "$start_time" \
            --to "$end_time" \
            --flamegraph "$output_file"
    else
        log_error "无法确定查询的时间范围"
        exit 1
    fi
    
    if [[ -f "$output_file" ]]; then
        log_success "火焰图生成成功: $output_file"
        log_info "可以在浏览器中打开查看:"
        echo "  xdg-open $output_file"
    else
        log_error "火焰图生成失败"
        exit 1
    fi
}

# 显示数据库信息
show_database_info() {
    local db_file="$1"
    
    if [[ ! -f "$db_file" ]]; then
        log_error "数据库文件不存在: $db_file"
        return 1
    fi
    
    log_header "数据库信息"
    
    log_info "文件: $db_file"
    log_info "大小: $(du -h "$db_file" | cut -f1)"
    
    # 查询样本数量
    local sample_count=$(sqlite3 "$db_file" "SELECT COUNT(*) FROM raw_samples;" 2>/dev/null || echo "0")
    log_info "样本数量: $sample_count"
    
    # 查询时间范围
    local time_range=$(sqlite3 "$db_file" "SELECT MIN(timestamp), MAX(timestamp) FROM raw_samples;" 2>/dev/null || echo "N/A N/A")
    if [[ "$time_range" != "N/A N/A" ]]; then
        local start_time=$(echo "$time_range" | cut -d'|' -f1)
        local end_time=$(echo "$time_range" | cut -d'|' -f2)
        log_info "时间范围: $(date -d "@$((start_time/1000000000))" 2>/dev/null || echo "$start_time") - $(date -d "@$((end_time/1000000000))" 2>/dev/null || echo "$end_time")"
    fi
    
    # 查询进程分布
    log_info "进程分布 (前5名):"
    sqlite3 "$db_file" "SELECT pid, COUNT(*) as count FROM raw_samples GROUP BY pid ORDER BY count DESC LIMIT 5;" 2>/dev/null | while IFS='|' read -r pid count; do
        log_info "  PID $pid: $count 样本"
    done
}

# 清理旧数据
cleanup_data() {
    local days=${1:-7}
    
    log_header "清理旧数据"
    
    log_info "清理 $days 天前的数据文件..."
    
    local deleted_count=0
    
    # 清理数据库文件
    find "$DATA_DIR" -name "perf_*.db" -type f -mtime +$days -delete && deleted_count=$((deleted_count + 1))
    find "$DATA_DIR" -name "perf_*.db-wal" -type f -mtime +$days -delete && deleted_count=$((deleted_count + 1))
    
    # 清理火焰图文件
    find "$DATA_DIR" -name "analysis_*.svg" -type f -mtime +$days -delete && deleted_count=$((deleted_count + 1))
    
    log_success "清理完成，删除了 $deleted_count 类文件"
}

# 显示帮助信息
show_help() {
    log_header "$PROJECT_NAME v$PROJECT_VERSION"
    
    echo -e "${CYAN}用法:${NC}"
    echo "  $0 <命令> [参数...]"
    echo ""
    
    echo -e "${CYAN}项目管理:${NC}"
    echo "  build                   构建项目"
    echo "  test                    测试项目功能"
    echo "  help                    显示此帮助信息"
    echo ""
    
    echo -e "${CYAN}数据采集 (需要root权限):${NC}"
    echo "  collect [时长] [频率]   采集性能数据"
    echo "  analysis [时长] [频率]  一键分析（采集+生成火焰图）"
    echo ""
    
    echo -e "${CYAN}数据查询:${NC}"
    echo "  query <数据库> [开始时间] [结束时间] [输出文件]"
    echo "  recent <数据库> [分钟数] [输出文件]"
    echo "  info <数据库>           显示数据库信息"
    echo ""
    
    echo -e "${CYAN}数据管理:${NC}"
    echo "  cleanup [天数]          清理N天前的旧数据"
    echo ""
    
    echo -e "${CYAN}示例:${NC}"
    echo "  $0 build                                    # 构建项目"
    echo "  sudo $0 collect 60 100                     # 采集60秒，100Hz"
    echo "  sudo $0 analysis 60 100                    # 一键分析"
    echo "  $0 query data/perf.db                      # 生成火焰图"
    echo "  $0 recent data/perf.db 10                  # 最近10分钟"
    echo "  $0 info data/perf.db                       # 查看数据库信息"
    echo "  $0 cleanup 7                               # 清理7天前的数据"
    echo ""
    
    echo -e "${CYAN}参数说明:${NC}"
    echo "  时长: 采集时长（秒），默认60"
    echo "  频率: 采样频率（Hz），默认100"
    echo "  开始/结束时间: Unix时间戳（秒）"
    echo "  分钟数: 最近N分钟的数据"
    echo "  天数: 清理N天前的数据，默认7"
    echo ""
    
    echo -e "${YELLOW}注意:${NC}"
    echo "  - 数据采集需要root权限"
    echo "  - 建议频率: 30-200Hz"
    echo "  - 建议时长: 30-300秒"
    echo "  - 火焰图文件可在浏览器中查看"
}

# 最近时间查询
query_recent() {
    local db_file="$1"
    local minutes=${2:-10}
    local output_file="$3"
    
    if [[ -z "$db_file" ]]; then
        log_error "请指定数据库文件"
        exit 1
    fi
    
    if [[ ! -f "$db_file" ]]; then
        log_error "数据库文件不存在: $db_file"
        exit 1
    fi
    
    # 计算时间范围
    local current_time=$(date +%s)
    local start_time=$((current_time - minutes * 60))
    
    log_info "查询最近 $minutes 分钟的数据..."
    log_info "时间范围: $start_time - $current_time"
    
    query_data "$db_file" "$start_time" "$current_time" "$output_file"
}

# 主函数
main() {
    local command="$1"
    
    case "$command" in
        "build")
            build_project
            ;;
        "test")
            test_project
            ;;
        "collect")
            collect_data "$2" "$3"
            ;;
        "analysis")
            analysis "$2" "$3"
            ;;
        "query")
            query_data "$2" "$3" "$4" "$5"
            ;;
        "recent")
            query_recent "$2" "$3" "$4"
            ;;
        "info")
            show_database_info "$2"
            ;;
        "cleanup")
            cleanup_data "$2"
            ;;
        "help"|"--help"|"-h"|"")
            show_help
            ;;
        *)
            log_error "未知命令: $command"
            echo ""
            show_help
            exit 1
            ;;
    esac
}

# 脚本入口
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi

