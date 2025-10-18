# 头文件模块摘要（include/*）

概览：
- 组成：buffer.h、config.h、database.h、elf_cache.h、elf_utils.h、hash.h、header.h、kernel_symbol.h、perf.h、rbtree.h
- 职责：统一类型定义与函数原型，明确模块边界与协作接口

buffer.h：并发环形缓冲区接口
- 定义 `buffer_entry` 与缓冲句柄；提供 `buffer_init/destroy/push/pop/pop_batch/shutdown/is_empty` 原型
- 结合 `pthread` 互斥与条件变量实现生产者-消费者模型

config.h：采集/查询配置与命令行解析
- 定义核心配置结构（采样频率、时长、LBR/栈深、过滤模式、输入输出路径、时间窗口、verbose 等）
- 暴露 `parse_args`/`validate_config` 等原型以支撑 `src/utils/config.c`

database.h：SQLite3 持久化接口
- 提供 `db_open/db_close/db_insert_one/db_insert_batch` 原型；统一表 `call_stacks`（时间戳/进程/堆栈）
- 约定 WAL 模式与批量事务策略，提高写入性能

elf_cache.h：ELF 内存块缓存接口
- 定义缓存句柄与操作：`elf_cache_init/destroy/get/put/prefetch` 等；用于加速 ELF 解析
- 与 `src/utils/elf_cache.c` 对应实现，面向只读缓存与可能的 LRU 策略

elf_utils.h：统一符号化工具接口
- 暴露地址到符号的转换入口（结合进程/VMA/ELF），如 `symbolize_address`/`format_stack`
- 为 `handler.c` 提供高层封装，保证符号化路径一致

hash.h：通用哈希表接口
- 定义字符串键/整型键的哈希与桶结构；提供插入/查找/删除与迭代器原型
- 迭代器支持安全遍历与删除，便于进程/VMA等集合管理

header.h：核心类型与跨模块声明
- 包含：`virtual_memory_area`、`process_info`、`process_hash_table`、`symbol_info`、`elf_symbol_collection`、`elf_file`、`elf_file_cache`、`system_context` 等
- 统一常量（如 `HASHTABLE_SIZE`）与各模块函数原型（main_loop/system/process/handler/symbol_table/elf/vma/query）

kernel_symbol.h：内核符号加载与查询
- 定义 `ksym_init/ksym_lookup/ksym_destroy` 等原型，用于解析内核地址到符号
- 配合用户/内核过滤模式，完善全栈符号化能力

perf.h：perf 事件采集接口
- 暴露 `perf_event_open_syscall/build_perf_attr/perf_init/perf_consume/perf_close` 等原型
- 定义 `perf_event_manager` 管理结构，支持 mmap 环形缓冲与系统/逐进程附加

rbtree.h：红黑树接口
- 定义 `rb_node/rb_root` 与基础操作：`rb_insert_color/rb_erase/rb_first/rb_next` 等
- 为 VMA 索引与符号表索引提供高效有序集合

优势与设计亮点：
- 接口清晰：头文件划分模块边界，便于解耦与替换实现
- 类型统一：跨模块共享核心类型，减少重复定义与不一致
- 兼容扩展：为 LBR/Stack、用户/内核过滤、缓存与索引提供一致入口
