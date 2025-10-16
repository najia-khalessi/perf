# 性能分析工具 (Profiling Tool)

基于 Linux perf 的采集与离线分析，支持历史火焰图回溯。

## 核心特性

- **低开销采集**：实时采集原始调用栈，写入 SQLite 数据库
- **离线分析**：按时间范围查询、符号化与聚合
- **火焰图生成**：自动生成交互式火焰图 SVG
- **历史回溯**：支持查询任意历史时间段的性能数据

## 快速开始

### 1. 构建项目
```bash
./perf_tool.sh build
```

### 2. 一键分析（推荐）
```bash
# 采集60秒并自动生成火焰图
sudo ./perf_tool.sh analysis 60 100
```

### 3. 查看结果
在浏览器中打开生成的 `.svg` 文件查看火焰图。

## 主要命令

```bash
./perf_tool.sh help              # 查看帮助
./perf_tool.sh build             # 构建项目
./perf_tool.sh test              # 测试功能

sudo ./perf_tool.sh collect 30 100      # 采集30秒数据
sudo ./perf_tool.sh analysis 60 100     # 一键分析（推荐）

./perf_tool.sh query data/perf.db       # 生成火焰图
./perf_tool.sh info data/perf.db        # 查看数据库信息
./perf_tool.sh recent data/perf.db 10   # 最近10分钟
```

## 文档

- **使用文档.md** - 完整使用指南
- **TEST_REPORT.md** - 测试报告

## 技术说明

- 时间戳以纳秒存储，命令行参数以秒输入
- 批量事务写入优化（默认每批 1000 条）
- 依赖 `third_party/flamegraph.pl` 生成火焰图
