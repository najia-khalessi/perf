# 脚本与第三方工具摘要（scripts/*, third_party/*）

概览：
- 项目脚本：`start.sh`、`quick_start.sh`、`perf_tool.sh`（如存在），用于构建、采集、分析与便捷启动
- 第三方工具：`third_party/flamegraph.pl`（Brendan Gregg），将 `func;func2 <count>` 形式的折叠堆栈转换为 SVG 火焰图

项目脚本：
- `start.sh`：入口脚本，封装构建/运行/采集参数；可能包含 root 检查与环境准备
- `quick_start.sh`：快速体验，默认采样时长/频率与输出目录；调用 `perf` 二进制并保存结果
- `perf_tool.sh`（若存在）：统一命令：`build/test/collect/analysis/query/info/recent` 等，集成 `third_party/flamegraph.pl` 生成火焰图

flamegraph.pl：
- 使用：输入折叠堆栈文本（`A;B;C 1`），输出交互式 SVG
- 选项：颜色主题、方向、合并策略、最小样本、标题等，可通过命令行定制
- 内部逻辑：解析、聚合、合并帧、生成 SVG 块和交互脚本；常作为离线分析最后一步

集成方式：
- 在线采集：`handler.c` 将符号化后的 `full_stack` 写入数据库
- 离线生成：`query.c` 输出折叠堆栈文本，调用 `flamegraph.pl` 生成 SVG

优势与说明：
- 复用成熟工具，降低维护成本
- 脚本标准化了体验流程（构建→采集→查询→生成火焰图）
