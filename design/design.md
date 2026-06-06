# 算法测试平台设计方案

> [!IMPORTANT]
> 本文是“目标设计 + 参考实现草案”的合集，不等同于当前仓库的已实现状态。  
> 截至 2026-02-14，仓库已实现的是 `ngcc_bench` 非交互式 CLI 主程序（`ngcc_bench/src`），并已支持：
> `hash/sig/kem/kex` + `correctness/performance/memory/stability` + 交互式入口（无参数运行）+ 可选 JSON 报告（`--json-out`）。  
> 另外已支持 `hash/sig/kem/kex` 的 KAT（`--kat`）、性能分布统计（min/mean/median/max/stddev/CV）、稳定性阈值 CLI 可配置（`stable-*`/`warning-*`，JSON `schema_version=4` 记录阈值与原始稳定性等级），以及稳定性窗口采样（`--stability-sample-ms`）；多工具内存分析等仍属于设计项。  
> 详细一致性对照见：`docs/design_alignment.md`。

> [!NOTE]
> 本文件由原根目录 `design.md` 与 `design/final_design.md` 合并整理而成，作为仓库唯一设计文档保留在 `design/` 目录。

## 0. 实现就绪摘要

本文对应一个 Linux C 基准工具：通过 `dlopen/dlsym` 加载用户指定的 `.so`，对 `CryptHash`、`SIG`、`KEM`、`KEX` 统一执行正确性、性能、内存、稳定性测试。设计目标是模块化、最小依赖、可直接配合 CMake 落地。

### 0.1 关键设计决策

| 反馈/议题 | 结论 | 处理方式 |
|---|---|---|
| KEX vtable 需要完整函数签名 | 采纳 | 明确所有 KEX 原型 |
| 内存单位统一 | 采纳 | 全部改为 bytes |
| 稳定性测试需要信号处理 | 采纳 | 支持 SIGINT/SIGTERM 优雅退出 |
| 随机输入需要更可靠来源 | 采纳 | 优先 `getrandom(2)`，回退 `/dev/urandom` |
| `sig_verify` 返回值语义 | 采纳 | 约定 `0 = success` |
| Hash 需要显式摘要长度参数 | 采纳 | 增加 `--digest-len-bits` |
| memory 测试是否 fork 隔离 | 不采纳 | 记录为进程级口径并在文档中说明限制 |
| 是否默认引入 JSON/verbose/config file | 有选择采纳 | 当前保留 `--json-out`，其余保持最简 |

## 1. 概述

### 1.1 目标平台
- **架构**: ARMv8 (AArch64)
- **操作系统**: Linux (Ubuntu 22.04 ARM64)
- **编译器**: GCC 12+

### 1.2 测试维度
| 测试类型 | 核心指标 |
|---------|---------|
| **正确性测试** | 测试向量通过率 |
| **性能测试** | 时钟周期数、吞吐量 |
| **内存测试** | 静态/峰值内存占用 |
| **稳定性测试** | 长时间运行指标波动 |

---

## 2. 交互式命令行界面

### 2.1 启动界面

```
$ ./ngcc_bench ./libmyalgo.so

╔══════════════════════════════════════════════╗
║     NGCC 算法测试平台 v1.0                   ║
╠══════════════════════════════════════════════╣
║  已加载算法库: libmyalgo.so                  ║
║  算法名称: MyAlgorithm v1.0.0                ║
╠══════════════════════════════════════════════╣
║  请选择测试项目:                             ║
║                                              ║
║    [1] 正确性测试                            ║
║    [2] 性能测试                              ║
║    [3] 内存测试                              ║
║    [4] 稳定性测试                            ║
║    [5] 运行全部测试                          ║
║    [0] 退出                                  ║
╚══════════════════════════════════════════════╝

请输入选项 [0-5]: 
```

### 2.2 子菜单示例 (选择2-性能测试)

```
╔══════════════════════════════════════════════╗
║  性能测试配置                                ║
╠══════════════════════════════════════════════╣
║  [1] 快速测试 (1000次迭代)                   ║
║  [2] 标准测试 (10000次迭代) [推荐]           ║
║  [3] 精确测试 (100000次迭代)                 ║
║  [4] 自定义配置                              ║
║  [0] 返回上级                                ║
╚══════════════════════════════════════════════╝

请输入选项 [0-4]: 2

正在执行性能测试...
[████████████████████████████████] 100%

═══════════════════════════════════════════════
  性能测试结果
═══════════════════════════════════════════════
  迭代次数:     10,000 次
  预热次数:      1,000 次
  
  时钟周期:
    最小值:      1,523 cycles
    中位数:      1,645 cycles  
    平均值:      1,672 cycles
    
  吞吐量:       1,456,789 ops/sec
              93.23 MB/sec
              
  测试状态:     ✓ 完成
═══════════════════════════════════════════════

按回车键继续...
```

### 2.3 实现代码

```c
// tests/src/main.c

#include <stdio.h>
#include <stdlib.h>

typedef enum {
    MENU_MAIN,
    MENU_CORRECTNESS,
    MENU_PERFORMANCE,
    MENU_MEMORY,
    MENU_STABILITY
} MenuState;

void print_main_menu(const AlgorithmInfo* info) {
    printf("\n");
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║     NGCC 算法测试平台 v1.0                   ║\n");
    printf("╠══════════════════════════════════════════════╣\n");
    printf("║  已加载算法库: %-30s║\n", info->name);
    printf("╠══════════════════════════════════════════════╣\n");
    printf("║  请选择测试项目:                             ║\n");
    printf("║                                              ║\n");
    printf("║    [1] 正确性测试                            ║\n");
    printf("║    [2] 性能测试                              ║\n");
    printf("║    [3] 内存测试                              ║\n");
    printf("║    [4] 稳定性测试                            ║\n");
    printf("║    [5] 运行全部测试                          ║\n");
    printf("║    [0] 退出                                  ║\n");
    printf("╚══════════════════════════════════════════════╝\n");
    printf("\n请输入选项 [0-5]: ");
}

int get_user_choice(int min, int max) {
    int choice;
    while (1) {
        if (scanf("%d", &choice) == 1 && choice >= min && choice <= max) {
            return choice;
        }
        printf("无效输入，请输入 %d-%d: ", min, max);
        while (getchar() != '\n');  // 清空输入缓冲
    }
}

void run_interactive_menu(const char* algo_path) {
    // 加载算法库
    AlgorithmInfo info;
    void* handle = load_algorithm(algo_path, &info);
    if (!handle) {
        fprintf(stderr, "错误: 无法加载算法库\n");
        return;
    }
    
    int running = 1;
    while (running) {
        print_main_menu(&info);
        int choice = get_user_choice(0, 5);
        
        switch (choice) {
            case 0: running = 0; break;
            case 1: run_correctness_menu(); break;
            case 2: run_performance_menu(); break;
            case 3: run_memory_menu(); break;
            case 4: run_stability_menu(); break;
            case 5: run_all_tests(); break;
        }
    }
    
    unload_algorithm(handle);
}
```

---

## 3. 正确性测试 - KAT 模板方案

### 3.1 KAT (Known Answer Test) 概述

KAT 测试使用预定义的输入输出向量验证算法实现的正确性。测试框架读取 KAT 文件，逐条执行算法并比对输出。

### 3.2 KAT 文件格式

采用简洁的文本格式，易于人工编写和机器解析：

```
# KAT 文件格式 (.kat)
# 注释行以 # 开头
# 每个测试向量包含 COUNT, KEY, INPUT, OUTPUT 等字段
# 字段值为十六进制字符串

COUNT = 0
KEY = 0123456789ABCDEF0123456789ABCDEF
INPUT = 00112233445566778899AABBCCDDEEFF
OUTPUT = 69C4E0D86A7B0430D8CDB78070B4C55A

COUNT = 1
KEY = 00000000000000000000000000000000
INPUT = 00000000000000000000000000000000
OUTPUT = 66E94BD4EF8A2C3B884CFA59CA342B2E
```

### 3.3 KAT 解析器实现

```c
// tests/include/kat_parser.h

#ifndef KAT_PARSER_H
#define KAT_PARSER_H

#include <stdint.h>
#include <stddef.h>

#define KAT_MAX_FIELD_LEN 1024
#define KAT_MAX_FIELDS    16

typedef struct {
    char name[32];              // 字段名 (KEY, INPUT, OUTPUT, etc.)
    uint8_t data[KAT_MAX_FIELD_LEN];
    size_t len;
} KATField;

typedef struct {
    uint32_t count;             // 向量序号
    KATField fields[KAT_MAX_FIELDS];
    int num_fields;
} KATVector;

typedef struct {
    KATVector* vectors;
    size_t num_vectors;
    size_t capacity;
} KATFile;

/**
 * 解析KAT文件
 * @param path KAT文件路径
 * @param kat 输出的KAT结构
 * @return 0成功，-1失败
 */
int kat_parse_file(const char* path, KATFile* kat);

/**
 * 获取向量中的指定字段
 */
const KATField* kat_get_field(const KATVector* vec, const char* name);

/**
 * 释放KAT资源
 */
void kat_free(KATFile* kat);

#endif
```

### 3.4 KAT 测试执行流程

```c
// tests/src/correctness_test.c

typedef struct {
    uint32_t total;             // 总向量数
    uint32_t passed;            // 通过数
    uint32_t failed;            // 失败数
    char first_failure[256];    // 首个失败描述
} CorrectnessResult;

int run_kat_test(const char* kat_path, CorrectnessResult* result) {
    KATFile kat;
    if (kat_parse_file(kat_path, &kat) < 0) {
        return -1;
    }
    
    result->total = kat.num_vectors;
    result->passed = 0;
    result->failed = 0;
    
    for (size_t i = 0; i < kat.num_vectors; i++) {
        KATVector* vec = &kat.vectors[i];
        
        // 获取输入输出字段
        const KATField* input = kat_get_field(vec, "INPUT");
        const KATField* expected = kat_get_field(vec, "OUTPUT");
        const KATField* key = kat_get_field(vec, "KEY");
        
        // 初始化算法 (如有KEY)
        if (key) {
            algorithm_init(key->data, key->len);
        }
        
        // 执行算法
        uint8_t actual[KAT_MAX_FIELD_LEN];
        size_t actual_len = sizeof(actual);
        int ret = algorithm_execute(input->data, input->len, 
                                    actual, &actual_len);
        
        // 比对结果
        if (ret == 0 && actual_len == expected->len &&
            memcmp(actual, expected->data, actual_len) == 0) {
            result->passed++;
        } else {
            result->failed++;
            if (result->failed == 1) {
                snprintf(result->first_failure, sizeof(result->first_failure),
                        "向量 #%u 失败: 输出不匹配", vec->count);
            }
        }
        
        algorithm_cleanup();
    }
    
    kat_free(&kat);
    return 0;
}
```

### 3.5 KAT 文件示例

```
# AES-128 KAT 测试向量
# 参考: NIST FIPS 197

COUNT = 0
KEY = 000102030405060708090A0B0C0D0E0F
INPUT = 00112233445566778899AABBCCDDEEFF
OUTPUT = 69C4E0D86A7B0430D8CDB78070B4C55A

COUNT = 1
KEY = 2B7E151628AED2A6ABF7158809CF4F3C
INPUT = 6BC1BEE22E409F96E93D7E117393172A
OUTPUT = 3AD77BB40D7A3660A89ECAF32466EF97

COUNT = 2
KEY = 2B7E151628AED2A6ABF7158809CF4F3C
INPUT = AE2D8A571E03AC9C9EB76FAC45AF8E51
OUTPUT = F5D3D58503B9699DE785895A96FDBAAF
```

---

## 4. 性能测试方案 (当前实现)

### 4.1 周期与时间计量方案对比

| 方案 | 测量内容 | 精度 | 开销 | 适用场景 |
|-----|---------|------|------|---------|
| **perf_event_open** | `PERF_COUNT_HW_CPU_CYCLES` | 周期级 | 低 | Linux 首选周期来源，由内核配置 PMU |
| **x86_64 TSC** | `lfence + rdtsc` | 周期级 | 极低 | Linux perf 不可用时的 x86_64 回退 |
| **ARMv8 PMCCNTR_EL0** | PMU cycle counter | 周期级 | 极低 | AArch64 最后回退，要求内核/固件开放 EL0 PMU 访问 |
| **clock_gettime(CLOCK_MONOTONIC)** | 单调时间 | 纳秒级 | 低 | 吞吐量、耗时统计；周期不可用时仍可使用 |

### 4.2 推荐策略

> [!IMPORTANT]
> **当前实现使用 `cycle_counter_open()` 自动选择周期来源，同时始终使用 `clock_gettime(CLOCK_MONOTONIC)` 统计时间**：
> - Linux 下优先 `perf_event_open(PERF_COUNT_HW_CPU_CYCLES)`，由内核配置和读取 PMU 事件。
> - 如果 Linux perf 不可用，`x86_64` 回退到 `lfence + rdtsc`。
> - 如果 Linux perf 不可用且平台是 `aarch64`，代码会尝试 ARMv8 PMU direct path。
> - 如果周期来源不可用，性能/稳定性测试应降级为 time-only 指标。
> - 单次迭代同时测量周期和时间，避免为了不同指标重复执行算法。

**当前周期来源优先级**：

1. `CYCLE_SOURCE_PERF`: Linux `syscall(__NR_perf_event_open, ...)`
2. `CYCLE_SOURCE_TSC`: `x86_64` `lfence + rdtsc`
3. `CYCLE_SOURCE_ARMV8_PMU`: AArch64 `PMCCNTR_EL0`
4. `CYCLE_SOURCE_NONE`: 周期不可用，仅输出时间/吞吐指标

**ARMv8 direct PMU 注意事项**：

> [!WARNING]
> 当前 `armv8_init_pmu()` 会执行 `MSR PMCR_EL0` / `MSR PMCNTENSET_EL0`，随后读取 `PMCCNTR_EL0`。这不是普通用户态权限；很多 Linux/容器环境没有开放 EL0 PMU 访问时会触发非法指令。生产/CI 场景应优先依赖 `perf_event_open`，直接 PMU 路径只能作为受控平台上的回退。

### 4.3 当前周期计数器实现

```c
// ngcc_bench/src/cycle_counter.c 的实际策略

int cycle_counter_open(cycle_counter_t *counter, int cycles_enabled) {
    counter->source = CYCLE_SOURCE_NONE;
    counter->perf_fd = -1;

    if (!cycles_enabled) {
        return 0;
    }

#ifdef __linux__
    /* 首选 Linux perf_event_open，由内核配置 PERF_COUNT_HW_CPU_CYCLES。 */
    counter->perf_fd = syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0);
    if (counter->perf_fd >= 0) {
        counter->source = CYCLE_SOURCE_PERF;
        return 0;
    }
#endif

#if defined(__x86_64__)
    counter->source = CYCLE_SOURCE_TSC;
    return 0;
#elif defined(__aarch64__)
    armv8_init_pmu();  /* 需要内核/固件允许 EL0 PMU 访问。 */
    counter->source = CYCLE_SOURCE_ARMV8_PMU;
    return 0;
#else
    return -1;
#endif
}
```

### 4.4 性能测试数据结构

```c
// tests/include/perf_types.h

#ifndef PERF_TYPES_H
#define PERF_TYPES_H

#include <stdint.h>
#include <sys/time.h>

/**
 * 性能测试统计结果
 * 使用 Welford 算法在线计算，无需存储所有样本
 */
typedef struct {
    // 测试配置
    uint64_t iterations;            // 迭代次数
    uint64_t warmup;                // 预热次数
    size_t   input_size;            // 输入数据大小 (bytes)

    // CPU周期统计（来源可能是 perf_event_open、x86_64 TSC 或 ARMv8 PMU）
    uint64_t counts_total;          // 总计数值
    double   counts_mean;           // 平均计数值
    double   counts_stddev;         // 标准差
    double   counts_cv;             // 变异系数 (%)
    uint64_t counts_min;            // 最小值
    uint64_t counts_max;            // 最大值

    // 时间统计 (纳秒, Welford 算法)
    uint64_t time_total_ns;         // 累计时间 (纳秒)
    double   time_mean_ns;          // 平均时间 (纳秒)
    double   time_stddev_ns;        // 标准差 (纳秒)
    double   time_cv;               // 变异系数 (%)

    // 效率指标
    double   time_per_byte_ns;      // 每字节耗时 (纳秒/字节)
    double   time_per_op_ns;        // 单次操作耗时 (纳秒)

    // 吞吐量
    double   throughput_ops;        // 操作吞吐量 (ops/sec)
    double   throughput_bytes;      // 字节吞吐量 (bytes/sec)
    double   throughput_mbps;       // 吞吐量 (MB/sec)

    // 计数器信息
    uint64_t cpu_freq_mhz;          // CPU频率 (MHz)
    char     counter_type[32];      // 计数器类型，如 "perf_event_open"、"rdtsc"、"armv8_pmu"
} PerfResult;

/**
 * Welford 在线统计状态
 */
typedef struct {
    uint64_t n;         // 样本数
    double   mean;      // 均值
    double   M2;        // 累计平方差
    uint64_t min;       // 最小值
    uint64_t max;       // 最大值
    uint64_t sum;       // 累计和
} WelfordState;

#endif
```

### 4.5 Welford 算法实现

```c
// tests/utils/welford.h

#ifndef WELFORD_H
#define WELFORD_H

#include <math.h>
#include <stdint.h>

/**
 * 初始化 Welford 状态
 */
static inline void welford_init(WelfordState* state) {
    state->n = 0;
    state->mean = 0.0;
    state->M2 = 0.0;
    state->min = UINT64_MAX;
    state->max = 0;
    state->sum = 0;
}

/**
 * 更新 Welford 状态 (在线算法)
 * 算法来源: Welford's Online Algorithm
 * 优点: 数值稳定，无需存储所有样本
 */
static inline void welford_update(WelfordState* state, uint64_t value) {
    state->n += 1;
    state->sum += value;
    
    // 更新最小/最大值
    if (value < state->min) state->min = value;
    if (value > state->max) state->max = value;
    
    // Welford 算法核心
    double x = (double)value;
    double delta = x - state->mean;
    state->mean += delta / (double)state->n;
    double delta2 = x - state->mean;
    state->M2 += delta * delta2;
}

/**
 * 获取方差
 */
static inline double welford_variance(const WelfordState* state) {
    if (state->n < 2) return 0.0;
    return state->M2 / (double)(state->n - 1);  // 样本方差
}

/**
 * 获取标准差
 */
static inline double welford_stddev(const WelfordState* state) {
    return sqrt(welford_variance(state));
}

/**
 * 获取平均值
 */
static inline double welford_mean(const WelfordState* state) {
    return state->mean;
}

#endif
```

### 4.6 完整性能测试实现

```c
// ngcc_bench/src/bench_core.c 的当前结构（简化）

int ngcc_run_performance_op(const ngcc_perf_config_t *cfg,
                            ngcc_operation_fn op,
                            void *op_ctx,
                            ngcc_perf_result_t *out_result) {
    cycle_counter_t counter;
    running_stats_t time_stats;
    running_stats_t cycle_stats;
    unsigned long long warmup = cfg->iterations / 100;

    if (warmup < 10) {
        warmup = 10;
    }

    for (unsigned long long i = 0; i < warmup; ++i) {
        if (op(op_ctx) != 0) {
            return -1;
        }
    }

    if (cycle_counter_open(&counter, cycles_enabled) != 0) {
        counter.source = CYCLE_SOURCE_NONE;  // fallback to time-only
    }

    stats_init(&time_stats);
    stats_init(&cycle_stats);
    clock_gettime(CLOCK_MONOTONIC, &total_start);

    for (unsigned long long i = 0; i < cfg->iterations; ++i) {
        clock_gettime(CLOCK_MONOTONIC, &iter_start);
        cycles_start = cycle_counter_begin(&counter);

        if (op(op_ctx) != 0) {
            goto cleanup;
        }

        cycles = cycle_counter_end(&counter, cycles_start);
        clock_gettime(CLOCK_MONOTONIC, &iter_end);

        stats_update(&time_stats, elapsed_ms(iter_start, iter_end));
        if (counter.source != CYCLE_SOURCE_NONE && cycles > 0) {
            stats_update(&cycle_stats, (double) cycles);
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &total_end);
    cycle_counter_close(&counter);

    // 输出 elapsed_ms、ops/s、bytes/s、time mean/median/stddev/CV。
    // 如果 cycles_available，则同时输出 cycles/op、cycles min/median/max/stddev/CV 和 cycles/B。
    return 0;
}
```

### 4.7 性能测试输出格式

```
═══════════════════════════════════════════════════════════════════
  性能测试结果
═══════════════════════════════════════════════════════════════════
  算法:           AES-128-CBC
  输入大小:       16 bytes
  迭代次数:       100,000 次
  预热次数:       10,000 次
  
  ┌─ 时钟周期 ─────────────────────────────────────────────────────┐
  │  平均值 (Mean):       1,645 cycles                             │
  │  标准差 (Stddev):     38 cycles                                │
  │  变异系数 (CV):       2.31%              ✓ (<5%)               │
  │  最小值 (Min):        1,523 cycles                             │
  │  最大值 (Max):        2,103 cycles                             │
  │  ─────────────────────────────────────────────────────────     │
  │  Cycles per Byte:     102.81 cycles/byte                       │
  └────────────────────────────────────────────────────────────────┘
  
  ┌─ 吞吐量 ───────────────────────────────────────────────────────┐
  │  操作吞吐量:          1,456,789 ops/sec                        │
  │  字节吞吐量:          23,308,624 bytes/sec                     │
  │                       22.23 MB/sec                             │
  └────────────────────────────────────────────────────────────────┘
  
  ┌─ 时间统计 ─────────────────────────────────────────────────────┐
  │  总测试时间:          68.64 ms                                 │
  │  时间均值:            686.5 ns/op                              │
  │  时间标准差:          15.8 ns                                  │
  │  时间变异系数:        2.30%              ✓ (<5%)               │
  └────────────────────────────────────────────────────────────────┘
  
  测试状态:       ✓ 完成
═══════════════════════════════════════════════════════════════════
```

#### 4.12.6 JSON 输出格式

```json
{
  "test_type": "performance",
  "algorithm": "AES-128-CBC",
  "config": {
    "iterations": 100000,
    "warmup": 10000,
    "input_size_bytes": 16
  },
  "cycles": {
    "mean": 1645,
    "stddev": 38,
    "cv_percent": 2.31,
    "min": 1523,
    "max": 2103,
    "total": 164500000,
    "per_byte": 102.81
  },
  "throughput": {
    "ops_per_sec": 1456789,
    "bytes_per_sec": 23308624,
    "mbps": 22.23
  },
  "time": {
    "total_ms": 68.64,
    "mean_ns": 686.5,
    "stddev_ns": 15.8,
    "cv_percent": 2.30
  },
  "status": "COMPLETED"
}
```

#### 4.12.7 指标用途说明

| 指标 | 主要用途 | 关注点 |
|-----|---------|--------|
| **Cycles Mean** | 算法效率对比 | 越小越好，应在相同平台和相同计数源下比较 |
| **Cycles per Byte** | 吞吐效率评估 | 越小越好，适合对比不同数据大小 |
| **Ops/sec** | 实际应用性能 | 越大越好，受CPU频率影响 |
| **MB/sec** | 数据处理能力 | 越大越好，直观展示吞吐量 |
| **CV (%)** | 性能稳定性 | <5% 为稳定，>10% 需关注 |
| **Min/Max** | 极端情况分析 | 差距过大说明有干扰 |

#### 4.12.8 输出实现代码

```c
void print_perf_result_console(const PerfResult* r, const char* algo_name) {
    printf("\n═══════════════════════════════════════════════════════════════════\n");
    printf("  性能测试结果\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("  算法:           %s\n", algo_name);
    printf("  输入大小:       %zu bytes\n", r->input_size);
    printf("  迭代次数:       %'" PRIu64 " 次\n", r->iterations);
    printf("  预热次数:       %'" PRIu64 " 次\n\n", r->warmup);
    
    // 时钟周期
    printf("  ┌─ 时钟周期 ─────────────────────────────────────────────────────┐\n");
    printf("  │  平均值 (Mean):       %',.0f cycles%*s│\n", 
           r->cycles_mean, 28 - (int)log10(r->cycles_mean + 1), "");
    printf("  │  标准差 (Stddev):     %',.0f cycles%*s│\n", 
           r->cycles_stddev, 28 - (int)log10(r->cycles_stddev + 1), "");
    printf("  │  变异系数 (CV):       %.2f%%              %s│\n", 
           r->cycles_cv, r->cycles_cv < 5.0 ? "✓ (<5%)" : "✗ (>5%)");
    printf("  │  最小值 (Min):        %'" PRIu64 " cycles%*s│\n", 
           r->cycles_min, 28 - (int)log10((double)r->cycles_min + 1), "");
    printf("  │  最大值 (Max):        %'" PRIu64 " cycles%*s│\n", 
           r->cycles_max, 28 - (int)log10((double)r->cycles_max + 1), "");
    printf("  │  ─────────────────────────────────────────────────────────     │\n");
    printf("  │  Cycles per Byte:     %.2f cycles/byte%*s│\n", 
           r->cycles_per_byte, 24 - (int)log10(r->cycles_per_byte + 1), "");
    printf("  └────────────────────────────────────────────────────────────────┘\n\n");
    
    // 吞吐量
    printf("  ┌─ 吞吐量 ───────────────────────────────────────────────────────┐\n");
    printf("  │  操作吞吐量:          %',.0f ops/sec%*s│\n", 
           r->throughput_ops, 23 - (int)log10(r->throughput_ops + 1), "");
    printf("  │  字节吞吐量:          %',.0f bytes/sec%*s│\n", 
           r->throughput_bytes, 21 - (int)log10(r->throughput_bytes + 1), "");
    printf("  │                       %.2f MB/sec%*s│\n", 
           r->throughput_mbps, 27 - (int)log10(r->throughput_mbps + 1), "");
    printf("  └────────────────────────────────────────────────────────────────┘\n\n");
    
    // 时间统计
    printf("  ┌─ 时间统计 ─────────────────────────────────────────────────────┐\n");
    printf("  │  总测试时间:          %.2f ms%*s│\n", 
           r->time_total_us / 1000.0, 30, "");
    printf("  │  时间均值:            %.1f ns/op%*s│\n", 
           r->time_per_op_ns, 26, "");
    printf("  │  时间标准差:          %.1f ns%*s│\n", 
           r->time_stddev_us * 1000.0, 28, "");
    printf("  │  时间变异系数:        %.2f%%              %s│\n", 
           r->time_cv, r->time_cv < 5.0 ? "✓ (<5%)" : "✗ (>5%)");
    printf("  └────────────────────────────────────────────────────────────────┘\n\n");
    
    printf("  测试状态:       ✓ 完成\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
}
```

---

#### 4.12.9 测试指标解释


| # | 指标名称 | 变量名 | 单位 | 计算公式 | 含义 | 重要性 |
|---|---------|-------|------|---------|------|--------|
| **基础统计** |
| 1 | 迭代次数 | `iterations` | 次 | 直接计数 | 测试执行次数 | ⭐ |
| 2 | 预热次数 | `warmup` | 次 | 配置值 | 预热执行次数 | ⭐ |
| 3 | 输入大小 | `input_size` | bytes | 配置值 | 单次操作数据量 | ⭐⭐ |
| **CPU周期** |
| 4 | CPU周期数均值 | `cycles_mean` | cycles | Welford 均值 | 单次操作平均CPU周期 | ⭐⭐⭐ |
| 5 | CPU周期数标准差 | `cycles_stddev` | cycles | √(M2/n) | CPU周期波动程度 | ⭐⭐ |
| 6 | CPU周期数总计 | `cycles_total` | cycles | 累加和 | 总消耗CPU周期 | ⭐ |
| 7 | CPU周期数最小 | `cycles_min` | cycles | 记录最小 | 最优情况 | ⭐ |
| 8 | CPU周期数最大 | `cycles_max` | cycles | 记录最大 | 最差情况 | ⭐ |
| 9 | **Cycles per Byte** | `cycles_per_byte` | cycles/B | cycles_mean / input_size | **每字节消耗CPU周期** | ⭐⭐⭐ |
| **时间** |
| 10 | 时间均值 | `time_mean_us` | μs | Welford 均值 | 单次操作平均时间 | ⭐⭐⭐ |
| 11 | 时间标准差 | `time_stddev_us` | μs | √(M2/n) | 时间波动程度 | ⭐⭐ |
| 12 | 总时间 | `time_total_us` | μs | 累加和 | 总测试时间 | ⭐⭐ |
| 13 | 单次操作时间 | `time_per_op_ns` | ns | time_mean_us × 1000 | 纳秒级单次耗时 | ⭐⭐ |
| **吞吐量** |
| 14 | **操作吞吐量** | `throughput_ops` | ops/s | iterations / (time_total / 1e6) | **每秒操作次数** | ⭐⭐⭐ |
| 15 | **字节吞吐量** | `throughput_bytes` | B/s | throughput_ops × input_size | **每秒处理字节** | ⭐⭐⭐ |
| 16 | 吞吐量(MB) | `throughput_mbps` | MB/s | throughput_bytes / (1024×1024) | 直观吞吐表示 | ⭐⭐ |
| **稳定性** |
| 17 | CPU周期变异系数 | `cycles_cv` | % | (cycles_stddev / cycles_mean) × 100 | CPU周期稳定性 | ⭐⭐ |
| 18 | 时间变异系数 | `time_cv` | % | (time_stddev / time_mean) × 100 | 时间稳定性 | ⭐⭐ |

**三大核心指标**（用户重点关注）：

```
┌──────────────────────────────────────────────────────────────────────────┐
│  ★ CPU周期数 (cycles_mean)      → 衡量算法效率，来源为当前可用周期计数器 │
│  ★ 运算吞吐量 (throughput_ops)  → 衡量实际处理能力，ops/sec             │  
│  ★ Cycles per Byte (cpb)        → 每字节CPU周期，业界标准对比指标        │
└──────────────────────────────────────────────────────────────────────────┘
```
---

## 5. 内存测试方案

### 5.1 测试内容

> [!IMPORTANT]
> **内存测试的对象是算法库 (.so 文件)，而非测试工具本身**

| 类型 | 指标 | 测量方法 |
|-----|------|---------|
| **静态内存** | .text/.data/.bss/.rodata 段大小 | `size -A` 命令解析 ELF |
| **动态内存** | 堆分配增量（运行前后差值） | `mallinfo2()` 差值 |
| **深度分析（可选）** | 堆峰值、分配次数、**泄漏检测**、调用栈 | Heaptrack（~10%开销） |

**内存测试架构**：
- `size -A`: 回答"算法库本身有多大"（代码、常量、全局变量）
- `mallinfo2()` 差值: 回答"运行算法时动态分配了多少堆内存"

### 5.2 工具选择菜单

```
╔══════════════════════════════════════════════╗
║  内存测试配置                                ║
╠══════════════════════════════════════════════╣
║  [1] 快速分析 (静态段 + /proc/self/status)   ║
║  [2] Heaptrack 详细分析 [推荐]               ║
║  [0] 返回上级                                ║
╚══════════════════════════════════════════════╝
```

### 5.3 静态内存分析

> [!IMPORTANT]
> **测试对象：算法库 .so 文件，而非测试工具 ./ngcc_bench**

使用 `size` 命令解析算法库的 ELF 段：

```c
// tests/src/memory_test.c (静态分析部分)

typedef struct {
    size_t text_size;       // 代码段
    size_t data_size;       // 已初始化数据
    size_t bss_size;        // 未初始化数据
    size_t rodata_size;     // 只读数据
    size_t tdata_size;      // TLS 已初始化数据
    size_t tbss_size;       // TLS 未初始化数据
    size_t total_static;    // 总静态内存
} StaticMemoryResult;

int analyze_static_memory(const char* library_path, StaticMemoryResult* result) {
    char cmd[256];

    // 解析 .so 库的段大小
    snprintf(cmd, sizeof(cmd), "size -A %s", library_path);

    FILE* fp = popen(cmd, "r");
    if (!fp) return -1;

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        size_t size;
        if (sscanf(line, ".text %zu", &size) == 1)
            result->text_size = size;
        else if (sscanf(line, ".data %zu", &size) == 1)
            result->data_size = size;
        else if (sscanf(line, ".bss %zu", &size) == 1)
            result->bss_size = size;
        else if (sscanf(line, ".rodata %zu", &size) == 1)
            result->rodata_size = size;
        else if (sscanf(line, ".tdata %zu", &size) == 1)
            result->tdata_size = size;
        else if (sscanf(line, ".tbss %zu", &size) == 1)
            result->tbss_size = size;
    }
    pclose(fp);

    result->total_static = result->text_size + result->data_size +
                           result->bss_size + result->rodata_size +
                           result->tdata_size + result->tbss_size;

    return 0;
}
```

### 5.4 动态内存分析 (mallinfo2)

**优势**: 精确归因到算法库、零采样开销、无需外部工具

使用 glibc `mallinfo2()` 测量运行前后堆分配差值，精确反映算法运行时的动态内存占用：

```c
// ngcc_bench/src/mem_stat.c (动态分析部分)

// 需要 glibc >= 2.33
#if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 33))
#include <malloc.h>
#define HAVE_MALLINFO2 1
#endif

uint64_t ngcc_mem_heap_bytes(void) {
#if HAVE_MALLINFO2
    struct mallinfo2 mi = mallinfo2();
    return (uint64_t) mi.uordblks;    // 当前已分配的堆内存
#else
    return 0;
#endif
}
```

**使用方式**：

```c
uint64_t heap_before = ngcc_mem_heap_bytes();
run_algorithm();
uint64_t heap_after  = ngcc_mem_heap_bytes();
int64_t  heap_delta  = heap_after - heap_before;  // 算法的堆分配增量
```

> [!NOTE]
> `mallinfo2()` 只统计 glibc malloc 管理的堆内存，不包含 mmap 大块分配。
> 对于算法库场景，这通常已足够覆盖。

### 5.5 Heaptrack 详细分析 (可选扩展)

**优势**: 低开销、详细分析、支持GUI可视化

> [!IMPORTANT]
> **Heaptrack 无法直接运行 .so 文件，需要创建测试程序加载算法库**

```bash
# 安装
sudo apt install heaptrack
```

```c
// tests/src/memory_test.c (heaptrack 部分)

typedef struct {
    size_t heap_peak;           // 峰值堆内存
    size_t total_allocations;   // 总分配次数
    size_t leaked_bytes;        // 泄漏字节数
} HeaptrackResult;

/**
 * 创建测试程序加载算法库
 */
int create_test_harness(const char* library_path, const char* harness_path) {
    FILE* fp = fopen(harness_path, "w");
    if (!fp) return -1;

    fprintf(fp, "#include <dlfcn.h>\n");
    fprintf(fp, "#include <stdio.h>\n");
    fprintf(fp, "int main() {\n");
    fprintf(fp, "    void* handle = dlopen(\"%s\", RTLD_NOW);\n", library_path);
    fprintf(fp, "    if (!handle) return 1;\n");
    fprintf(fp, "    // 调用算法初始化和测试函数\n");
    fprintf(fp, "    void (*test_fn)() = dlsym(handle, \"algorithm_test\");\n");
    fprintf(fp, "    if (test_fn) test_fn();\n");
    fprintf(fp, "    dlclose(handle);\n");
    fprintf(fp, "    return 0;\n");
    fprintf(fp, "}\n");
    fclose(fp);

    // 编译测试程序
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "gcc -o %s.out %s -ldl", harness_path, harness_path);
    return system(cmd);
}

int run_heaptrack_test(const char* library_path, HeaptrackResult* result) {
    char harness_path[256], outfile[256], cmd[512];

    snprintf(harness_path, sizeof(harness_path), "/tmp/ngcc_harness_%d.c", getpid());
    snprintf(outfile, sizeof(outfile), "/tmp/ngcc_heap_%d.gz", getpid());

    // 创建并编译测试程序
    if (create_test_harness(library_path, harness_path) != 0) {
        return -1;
    }

    // 运行 heaptrack
    snprintf(cmd, sizeof(cmd), "heaptrack -o %s %s.out >/dev/null 2>&1",
             outfile, harness_path);
    system(cmd);

    // 解析结果
    snprintf(cmd, sizeof(cmd), "heaptrack_print %s 2>/dev/null", outfile);
    FILE* fp = popen(cmd, "r");

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "peak heap"))
            sscanf(line, "%*[^0-9]%zu", &result->heap_peak);
        else if (strstr(line, "total allocations"))
            sscanf(line, "%*[^0-9]%zu", &result->total_allocations);
        else if (strstr(line, "leaked"))
            sscanf(line, "%*[^0-9]%zu", &result->leaked_bytes);
    }
    pclose(fp);

    // 清理临时文件
    unlink(outfile);
    unlink(harness_path);
    snprintf(cmd, sizeof(cmd), "%s.out", harness_path);
    unlink(cmd);

    return 0;
}
```

### 5.6 工具对比

| 工具 | 开销 | 精度 | 适用场景 |
|-----|------|------|---------|
| **静态分析 (size -A)** | 无 | 精确 | ELF段大小 |
| **mallinfo2() 差值** | 无 | 高 | 堆分配增量，精确归因 ✅ |
| **Heaptrack** | 低(~10%) | 高 | 堆内存详细分析（可选） |

---

---

## 6. 稳定性测试方案

### 6.1 测试目标

稳定性测试用于评估算法在长时间运行条件下的可靠性，检测性能波动、内存泄漏等问题。

> [!IMPORTANT]
> **与其他测试模块保持一致**：
> - **计时方案**：复用 `cycle_counter` 周期来源选择，并使用 `clock_gettime(CLOCK_MONOTONIC)` 统计时间；周期不可用时降级为 time-only
> - **内存监控**：使用 `/proc/self/status` 的 `VmRSS`（与内存测试一致，实时当前值）
> - **统计算法**：使用 Welford 在线算法（避免存储所有轮次数据）

| 指标 | 说明 | 判定标准 |
|-----|------|---------|
| **吞吐量变异系数 (throughput_cv)** | 多轮测试吞吐量的波动程度 | CV < 5% 为稳定 |
| **CPU周期稳定性 (counts_cv)** | 当前可用周期来源的计数波动；不可用时标记为 unavailable | CV < 5% 为稳定 |
| **时间稳定性 (time_cv)** | 时间均值的波动程度 | CV < 5% 为稳定 |
| **内存增长率** | VmRSS 从开始到结束的增长 | 增长 < 1% 为无泄漏 |
| **错误率** | 长时间运行中的错误发生率 | 0% 为通过 |

### 6.2 测试配置

```
╔══════════════════════════════════════════════╗
║  稳定性测试配置                              ║
╠══════════════════════════════════════════════╣
║  [1] 快速测试 (10轮 × 1000次, 间隔100ms)     ║
║  [2] 标准测试 (30轮 × 10000次, 间隔500ms)    ║
║       [推荐]                                 ║
║  [3] 长时间测试 (100轮 × 10000次, 间隔1s)    ║
║  [4] 自定义配置                              ║
║  [0] 返回上级                                ║
╚══════════════════════════════════════════════╝
```

**注意**：轮间间隔（cooldown）用于避免 CPU 热节流影响测试稳定性。

### 6.3 数据结构

```c
// tests/include/stability_test.h

#ifndef STABILITY_TEST_H
#define STABILITY_TEST_H

#include <stdint.h>
#include "welford.h"

typedef struct {
    int num_rounds;             // 测试轮数
    int iterations_per_round;   // 每轮迭代次数
    int warmup_per_round;       // 每轮预热次数
    int cooldown_ms;            // 轮间冷却时间（毫秒）
} StabilityConfig;

typedef struct {
    // 吞吐量统计
    double throughput_mean;     // 平均吞吐量 (ops/sec)
    double throughput_stddev;   // 吞吐量标准差
    double throughput_cv;       // 变异系数 (CV = stddev/mean)
    double throughput_min;      // 最小吞吐量
    double throughput_max;      // 最大吞吐量

    // CPU周期统计（来源同性能测试；不可用时跳过）
    double counts_mean;         // 平均计数值
    double counts_stddev;       // 计数值标准差
    double counts_cv;           // 计数变异系数

    // 时间统计
    double time_mean_ns;        // 平均时间 (纳秒)
    double time_stddev_ns;      // 时间标准差 (纳秒)
    double time_cv;             // 时间变异系数

    // 内存统计 (VmRSS from /proc/self/status)
    size_t memory_start_kb;     // 初始内存占用 (KB)
    size_t memory_end_kb;       // 结束时内存占用 (KB)
    size_t memory_min_kb;       // 最小内存占用 (KB)
    size_t memory_max_kb;       // 最大内存占用 (KB)
    double memory_growth_rate;  // 内存增长率 (%)

    // 错误统计
    uint32_t total_executions;  // 总执行次数
    uint32_t error_count;       // 错误次数
    double error_rate;          // 错误率 (%)

    // 分项评定
    int performance_stable;     // 性能稳定性 (1=是, 0=否)
    int memory_stable;          // 内存稳定性 (1=是, 0=否)
    int correctness_stable;     // 正确性 (1=是, 0=否)

    // 综合评定
    int is_stable;              // 是否稳定 (1=是, 0=否)
    char status[32];            // "STABLE" / "UNSTABLE" / "WARNING"
    char failure_reasons[256];  // 失败原因描述
} StabilityResult;

#endif
```

### 6.4 实现代码

```c
// tests/src/stability_test.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include "stability_test.h"
#include "armv8_cycle.h"
#include "welford.h"

/**
 * 从 /proc/self/status 读取 VmRSS（当前物理内存占用）
 */
static size_t get_current_memory_kb(void) {
    FILE* fp = fopen("/proc/self/status", "r");
    if (!fp) return 0;

    char line[256];
    size_t vm_rss = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "VmRSS: %zu kB", &vm_rss) == 1) {
            break;
        }
    }
    fclose(fp);
    return vm_rss;
}

/**
 * 毫秒级延迟
 */
static void sleep_ms(int milliseconds) {
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

int run_stability_test(AlgorithmContext* ctx,
                       const StabilityConfig* config,
                       StabilityResult* result) {

    // 初始化 Welford 状态
    WelfordState throughput_stat, counts_stat, time_stat;
    welford_init(&throughput_stat);
    welford_init(&counts_stat);
    welford_init(&time_stat);

    // 初始化结果
    memset(result, 0, sizeof(StabilityResult));
    result->memory_start_kb = get_current_memory_kb();
    result->memory_min_kb = result->memory_start_kb;
    result->memory_max_kb = result->memory_start_kb;

    cycle_counter_t counter;
    int cycles_available = (cycle_counter_open(&counter, cycles_enabled) == 0 &&
                            counter.source != CYCLE_SOURCE_NONE);

    printf("\n正在执行稳定性测试...\n");

    for (int round = 0; round < config->num_rounds; round++) {
        // 显示进度
        int progress = (round + 1) * 100 / config->num_rounds;
        printf("\r[");
        for (int i = 0; i < 40; i++) {
            printf(i < progress * 40 / 100 ? "█" : " ");
        }
        printf("] %3d%% (轮次 %d/%d)", progress, round + 1, config->num_rounds);
        fflush(stdout);

        // 预热
        for (int i = 0; i < config->warmup_per_round; i++) {
            algorithm_execute(ctx->input, ctx->input_size,
                             ctx->output, &ctx->output_size);
        }

        // 测量本轮性能（单循环双计时）
        struct timespec time_start, time_end;
        uint64_t round_counts_total = 0;
        uint64_t round_time_total_ns = 0;

        for (int i = 0; i < config->iterations_per_round; i++) {
            // 同时开始计数器和时间测量
            uint64_t counts_start = cycle_counter_begin(&counter);
            clock_gettime(CLOCK_MONOTONIC, &time_start);

            int ret = algorithm_execute(ctx->input, ctx->input_size,
                                        ctx->output, &ctx->output_size);

            uint64_t counts_diff = cycle_counter_end(&counter, counts_start);
            clock_gettime(CLOCK_MONOTONIC, &time_end);

            // 累计本轮数据
            if (cycles_available && counts_diff > 0) {
                round_counts_total += counts_diff;
            }
            round_time_total_ns += (time_end.tv_sec - time_start.tv_sec) * 1000000000ULL +
                                   (time_end.tv_nsec - time_start.tv_nsec);

            result->total_executions++;
            if (ret != 0) result->error_count++;
        }

        // 计算本轮统计值
        double round_counts_avg = (double)round_counts_total / config->iterations_per_round;
        double round_time_avg_ns = (double)round_time_total_ns / config->iterations_per_round;
        double round_time_sec = (double)round_time_total_ns / 1e9;
        double round_throughput = config->iterations_per_round / round_time_sec;

        // 在线更新统计（Welford 算法）
        welford_update(&throughput_stat, (uint64_t)round_throughput);
        welford_update(&counts_stat, (uint64_t)round_counts_avg);
        welford_update(&time_stat, (uint64_t)round_time_avg_ns);

        // 采样内存
        size_t current_mem = get_current_memory_kb();
        if (current_mem < result->memory_min_kb) result->memory_min_kb = current_mem;
        if (current_mem > result->memory_max_kb) result->memory_max_kb = current_mem;

        // 轮间冷却
        if (round < config->num_rounds - 1 && config->cooldown_ms > 0) {
            sleep_ms(config->cooldown_ms);
        }
    }

    printf("\n");

    // === 计算最终统计结果 ===

    // 吞吐量统计
    result->throughput_mean = welford_mean(&throughput_stat);
    result->throughput_stddev = welford_stddev(&throughput_stat);
    result->throughput_cv = (result->throughput_mean > 0) ?
                            (result->throughput_stddev / result->throughput_mean) : 0;
    result->throughput_min = (double)throughput_stat.min;
    result->throughput_max = (double)throughput_stat.max;

    // 计数器统计
    result->counts_mean = welford_mean(&counts_stat);
    result->counts_stddev = welford_stddev(&counts_stat);
    result->counts_cv = (result->counts_mean > 0) ?
                        (result->counts_stddev / result->counts_mean) : 0;

    // 时间统计
    result->time_mean_ns = welford_mean(&time_stat);
    result->time_stddev_ns = welford_stddev(&time_stat);
    result->time_cv = (result->time_mean_ns > 0) ?
                      (result->time_stddev_ns / result->time_mean_ns) : 0;

    // 内存统计
    result->memory_end_kb = get_current_memory_kb();
    if (result->memory_start_kb > 0) {
        result->memory_growth_rate = 100.0 *
            (double)(result->memory_end_kb - result->memory_start_kb) /
            result->memory_start_kb;
    } else {
        result->memory_growth_rate = 0;
    }

    // 错误率
    result->error_rate = (result->total_executions > 0) ?
                         (100.0 * result->error_count / result->total_executions) : 0;

    // === 分项评定 ===

    result->performance_stable =
        (result->throughput_cv < 0.05 &&
         result->counts_cv < 0.03 &&
         result->time_cv < 0.05);

    result->memory_stable =
        (fabs(result->memory_growth_rate) < 1.0);

    result->correctness_stable =
        (result->error_rate == 0);

    // === 综合评定 ===

    result->failure_reasons[0] = '\0';

    if (result->performance_stable &&
        result->memory_stable &&
        result->correctness_stable) {
        result->is_stable = 1;
        strcpy(result->status, "STABLE");
    } else if (result->throughput_cv < 0.10 &&
               fabs(result->memory_growth_rate) < 5.0 &&
               result->error_rate < 0.01) {
        result->is_stable = 0;
        strcpy(result->status, "WARNING");

        // 记录失败原因
        char temp[128];
        if (!result->performance_stable) {
            snprintf(temp, sizeof(temp), "性能波动(吞吐CV=%.2f%%, 计数CV=%.2f%%, 时间CV=%.2f%%); ",
                    result->throughput_cv * 100, result->counts_cv * 100, result->time_cv * 100);
            strcat(result->failure_reasons, temp);
        }
        if (!result->memory_stable) {
            snprintf(temp, sizeof(temp), "内存增长(%.2f%%); ", result->memory_growth_rate);
            strcat(result->failure_reasons, temp);
        }
        if (!result->correctness_stable) {
            snprintf(temp, sizeof(temp), "错误率(%.4f%%); ", result->error_rate);
            strcat(result->failure_reasons, temp);
        }
    } else {
        result->is_stable = 0;
        strcpy(result->status, "UNSTABLE");

        // 记录失败原因
        char temp[128];
        if (result->throughput_cv >= 0.10) {
            snprintf(temp, sizeof(temp), "吞吐量CV过高(%.2f%%); ", result->throughput_cv * 100);
            strcat(result->failure_reasons, temp);
        }
        if (result->counts_cv >= 0.05) {
            snprintf(temp, sizeof(temp), "计数器CV过高(%.2f%%); ", result->counts_cv * 100);
            strcat(result->failure_reasons, temp);
        }
        if (result->time_cv >= 0.10) {
            snprintf(temp, sizeof(temp), "时间CV过高(%.2f%%); ", result->time_cv * 100);
            strcat(result->failure_reasons, temp);
        }
        if (fabs(result->memory_growth_rate) >= 5.0) {
            snprintf(temp, sizeof(temp), "内存异常增长(%.2f%%); ", result->memory_growth_rate);
            strcat(result->failure_reasons, temp);
        }
        if (result->error_rate >= 0.01) {
            snprintf(temp, sizeof(temp), "错误率过高(%.4f%%); ", result->error_rate);
            strcat(result->failure_reasons, temp);
        }
    }

    return 0;
}
```

---

## 7. 测试输出格式

### 7.1 输出策略

所有测试结果同时输出到：
1. **控制台** - 人性化格式，便于即时查看
2. **JSON文件** - 机器可读格式，便于后续处理

### 7.2 正确性测试输出

#### 控制台格式
```
═══════════════════════════════════════════════
  正确性测试结果
═══════════════════════════════════════════════
  KAT 文件:       aes128_kat.kat
  测试向量数:     1,000
  
  通过:           1,000  ✓
  失败:           0
  通过率:         100.00%
  
  测试状态:       ✓ 全部通过
═══════════════════════════════════════════════
```

#### JSON 格式
```json
{
  "test_type": "correctness",
  "kat_file": "aes128_kat.kat",
  "total": 1000,
  "passed": 1000,
  "failed": 0,
  "pass_rate": 100.0,
  "first_failure": null,
  "status": "PASSED"
}
```

### 7.3 性能测试输出

#### 控制台格式
```
═══════════════════════════════════════════════
  性能测试结果
═══════════════════════════════════════════════
  迭代次数:       10,000 次
  预热次数:       1,000 次
  
  ┌─ 时钟周期 ─────────────────────────────────┐
  │  最小值:      1,523 cycles                 │
  │  中位数:      1,645 cycles                 │
  │  平均值:      1,672 cycles                 │
  │  最大值:      2,103 cycles                 │
  └────────────────────────────────────────────┘
  
  ┌─ 吞吐量 ───────────────────────────────────┐
  │  操作数:      1,456,789 ops/sec            │
  │  字节数:      93.23 MB/sec                 │
  │  单次耗时:    686.5 ns/op                  │
  └────────────────────────────────────────────┘
  
  测试状态:       ✓ 完成
═══════════════════════════════════════════════
```

#### JSON 格式
```json
{
  "test_type": "performance",
  "config": {
    "iterations": 10000,
    "warmup": 1000
  },
  "cycles": {
    "min": 1523,
    "median": 1645,
    "avg": 1672,
    "max": 2103
  },
  "throughput": {
    "ops_per_sec": 1456789,
    "bytes_per_sec": 93230000,
    "time_per_op_ns": 686.5
  },
  "status": "COMPLETED"
}
```

### 7.4 内存测试输出

#### 控制台格式
```
═══════════════════════════════════════════════
  内存测试结果
═══════════════════════════════════════════════
  分析工具:       Heaptrack
  
  ┌─ 静态内存 ─────────────────────────────────┐
  │  .text  (代码段):     2,048 bytes          │
  │  .data  (数据段):       512 bytes          │
  │  .bss   (BSS段):      1,024 bytes          │
  │  .rodata(只读段):     1,280 bytes          │
  │  ─────────────────────────────             │
  │  总计:                4,864 bytes          │
  └────────────────────────────────────────────┘
  
  ┌─ 动态内存 ─────────────────────────────────┐
  │  峰值堆内存:          65,536 bytes (64 KB) │
  │  总分配次数:          150 次               │
  │  内存泄漏:            0 bytes     ✓        │
  └────────────────────────────────────────────┘
  
  测试状态:       ✓ 无泄漏
═══════════════════════════════════════════════
```

#### JSON 格式
```json
{
  "test_type": "memory",
  "tool": "heaptrack",
  "static": {
    "text": 2048,
    "data": 512,
    "bss": 1024,
    "rodata": 1280,
    "total": 4864
  },
  "dynamic": {
    "heap_peak": 65536,
    "total_allocations": 150,
    "leaked_bytes": 0
  },
  "status": "NO_LEAK"
}
```

### 7.5 稳定性测试输出

#### 控制台格式
```
═══════════════════════════════════════════════
  稳定性测试结果
═══════════════════════════════════════════════
  测试配置:       30 轮 × 10,000 次/轮
  总执行次数:     300,000 次
  
  ┌─ 吞吐量稳定性 ─────────────────────────────┐
  │  平均值:      1,456,789 ops/sec            │
  │  标准差:      33,506 ops/sec               │
  │  变异系数:    2.30%           ✓ (<5%)      │
  │  范围:        1,398,234 ~ 1,512,456        │
  └────────────────────────────────────────────┘
  
  ┌─ 周期数稳定性 ─────────────────────────────┐
  │  平均值:      1,672 cycles                 │
  │  标准差:      38 cycles                    │
  │  变异系数:    2.27%           ✓ (<3%)      │
  └────────────────────────────────────────────┘
  
  ┌─ 内存稳定性 ───────────────────────────────┐
  │  初始内存:    12,582,912 bytes (12.0 MB)   │
  │  结束内存:    12,648,448 bytes (12.1 MB)   │
  │  增长率:      0.52%           ✓ (<1%)      │
  └────────────────────────────────────────────┘
  
  ┌─ 错误统计 ─────────────────────────────────┐
  │  错误次数:    0                            │
  │  错误率:      0.000%          ✓            │
  └────────────────────────────────────────────┘
  
  综合评定:       ✓ STABLE
═══════════════════════════════════════════════
```

#### JSON 格式
```json
{
  "test_type": "stability",
  "config": {
    "num_rounds": 30,
    "iterations_per_round": 10000,
    "total_executions": 300000
  },
  "throughput": {
    "mean": 1456789,
    "stddev": 33506,
    "cv": 0.023,
    "min": 1398234,
    "max": 1512456
  },
  "cycles": {
    "mean": 1672,
    "stddev": 38,
    "cv": 0.0227
  },
  "memory": {
    "start_bytes": 12582912,
    "end_bytes": 12648448,
    "growth_rate": 0.0052
  },
  "errors": {
    "count": 0,
    "rate": 0.0
  },
  "is_stable": true,
  "status": "STABLE"
}
```

### 7.6 输出实现代码

```c
// tests/include/report.h

typedef enum {
    OUTPUT_CONSOLE = 1,
    OUTPUT_JSON = 2,
    OUTPUT_BOTH = 3
} OutputMode;

// 输出正确性测试结果
void output_correctness_result(const CorrectnessResult* r, 
                               const char* kat_file,
                               OutputMode mode);

// 输出性能测试结果
void output_performance_result(const PerfResult* r,
                               int iterations, int warmup,
                               OutputMode mode);

// 输出内存测试结果
void output_memory_result(const StaticMemoryResult* static_mem,
                          const HeaptrackResult* heap,
                          const char* tool_name,
                          OutputMode mode);

// 输出稳定性测试结果
void output_stability_result(const StabilityResult* r,
                             const StabilityConfig* config,
                             OutputMode mode);
```

```c
// tests/src/report.c

#include <stdio.h>
#include <time.h>
#include "report.h"

static FILE* open_json_report(const char* test_type) {
    char filename[256];
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    snprintf(filename, sizeof(filename), 
             "reports/%s_%04d%02d%02d_%02d%02d%02d.json",
             test_type,
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
             t->tm_hour, t->tm_min, t->tm_sec);
    return fopen(filename, "w");
}

void output_stability_result(const StabilityResult* r,
                             const StabilityConfig* config,
                             OutputMode mode) {
    // 控制台输出
    if (mode & OUTPUT_CONSOLE) {
        printf("\n═══════════════════════════════════════════════\n");
        printf("  稳定性测试结果\n");
        printf("═══════════════════════════════════════════════\n");
        printf("  测试配置:       %d 轮 × %d 次/轮\n", 
               config->num_rounds, config->iterations_per_round);
        printf("  总执行次数:     %'u 次\n\n", r->total_executions);
        
        printf("  ┌─ 吞吐量稳定性 ─────────────────────────────┐\n");
        printf("  │  平均值:      %',.0f ops/sec            │\n", r->throughput_mean);
        printf("  │  标准差:      %',.0f ops/sec               │\n", r->throughput_stddev);
        printf("  │  变异系数:    %.2f%%           %s      │\n", 
               r->throughput_cv * 100, r->throughput_cv < 0.05 ? "✓" : "✗");
        printf("  └────────────────────────────────────────────┘\n\n");
        
        printf("  ┌─ 周期数稳定性 ─────────────────────────────┐\n");
        printf("  │  平均值:      %lu cycles                 │\n", r->cycles_mean);
        printf("  │  变异系数:    %.2f%%           %s      │\n", 
               r->cycles_cv * 100, r->cycles_cv < 0.03 ? "✓" : "✗");
        printf("  └────────────────────────────────────────────┘\n\n");
        
        printf("  ┌─ 内存稳定性 ───────────────────────────────┐\n");
        printf("  │  增长率:      %.2f%%           %s      │\n", 
               r->memory_growth_rate, r->memory_growth_rate < 1.0 ? "✓" : "✗");
        printf("  └────────────────────────────────────────────┘\n\n");
        
        printf("  综合评定:       %s %s\n", 
               r->is_stable ? "✓" : "✗", r->status);
        printf("═══════════════════════════════════════════════\n");
    }
    
    // JSON 输出
    if (mode & OUTPUT_JSON) {
        FILE* fp = open_json_report("stability");
        if (fp) {
            fprintf(fp, "{\n");
            fprintf(fp, "  \"test_type\": \"stability\",\n");
            fprintf(fp, "  \"config\": {\n");
            fprintf(fp, "    \"num_rounds\": %d,\n", config->num_rounds);
            fprintf(fp, "    \"iterations_per_round\": %d,\n", config->iterations_per_round);
            fprintf(fp, "    \"total_executions\": %u\n", r->total_executions);
            fprintf(fp, "  },\n");
            fprintf(fp, "  \"throughput\": {\n");
            fprintf(fp, "    \"mean\": %.0f,\n", r->throughput_mean);
            fprintf(fp, "    \"stddev\": %.0f,\n", r->throughput_stddev);
            fprintf(fp, "    \"cv\": %.4f,\n", r->throughput_cv);
            fprintf(fp, "    \"min\": %.0f,\n", r->throughput_min);
            fprintf(fp, "    \"max\": %.0f\n", r->throughput_max);
            fprintf(fp, "  },\n");
            fprintf(fp, "  \"cycles\": {\n");
            fprintf(fp, "    \"mean\": %lu,\n", r->cycles_mean);
            fprintf(fp, "    \"stddev\": %.0f,\n", (double)r->cycles_stddev);
            fprintf(fp, "    \"cv\": %.4f\n", r->cycles_cv);
            fprintf(fp, "  },\n");
            fprintf(fp, "  \"memory\": {\n");
            fprintf(fp, "    \"start_bytes\": %zu,\n", r->memory_start);
            fprintf(fp, "    \"end_bytes\": %zu,\n", r->memory_end);
            fprintf(fp, "    \"growth_rate\": %.4f\n", r->memory_growth_rate / 100.0);
            fprintf(fp, "  },\n");
            fprintf(fp, "  \"errors\": {\n");
            fprintf(fp, "    \"count\": %u,\n", r->error_count);
            fprintf(fp, "    \"rate\": %.6f\n", r->error_rate / 100.0);
            fprintf(fp, "  },\n");
            fprintf(fp, "  \"is_stable\": %s,\n", r->is_stable ? "true" : "false");
            fprintf(fp, "  \"status\": \"%s\"\n", r->status);
            fprintf(fp, "}\n");
            fclose(fp);
        }
    }
}
```

---

## 8. 综合测试报告

运行全部测试后生成综合报告：

### 控制台汇总

```
╔══════════════════════════════════════════════════════╗
║              NGCC 算法测试综合报告                   ║
║              MyAlgorithm v1.0.0                      ║
║              2026-01-31 15:50:00                     ║
╠══════════════════════════════════════════════════════╣
║  测试项目          结果          关键指标             ║
╠══════════════════════════════════════════════════════╣
║  正确性测试        ✓ PASSED      1000/1000 (100%)    ║
║  性能测试          ✓ COMPLETED   1,645 cycles/op     ║
║  内存测试          ✓ NO_LEAK     峰值 64KB           ║
║  稳定性测试        ✓ STABLE      CV=2.30%            ║
╠══════════════════════════════════════════════════════╣
║  综合评定:         ✓ 全部通过                        ║
╚══════════════════════════════════════════════════════╝

报告已保存至: reports/full_report_20260131_155000.json
```

### JSON 综合报告

```json
{
  "timestamp": "2026-01-31T15:50:00+08:00",
  "algorithm": {
    "name": "MyAlgorithm",
    "version": "1.0.0",
    "library": "libmyalgo.so"
  },
  "correctness": {
    "status": "PASSED",
    "passed": 1000,
    "total": 1000,
    "pass_rate": 100.0
  },
  "performance": {
    "status": "COMPLETED",
    "cycles_median": 1645,
    "throughput_ops_sec": 1456789,
    "throughput_bytes_sec": 93230000
  },
  "memory": {
    "status": "NO_LEAK",
    "static_total": 4864,
    "heap_peak": 65536,
    "leaked_bytes": 0
  },
  "stability": {
    "status": "STABLE",
    "throughput_cv": 0.023,
    "cycles_cv": 0.0227,
    "memory_growth_rate": 0.0052,
    "error_rate": 0.0
  },
  "overall": {
    "all_passed": true,
    "summary": "全部测试通过"
  }
}
```

---

## 9. 目录结构

```
ngcc_bench/
├── CMakeLists.txt                  # 唯一 CMake 入口（编译选手 .so + 测试工具）
├── code/                           # ← 选手替换此目录
│   ├── API_BlockCipher/
│   │   ├── Implementations/
│   │   │   ├── Reference_Implementation/    # 选手源码 (.c/.h)
│   │   │   ├── Optimized_Implementation/    # 选手源码
│   │   │   └── Additional_Implementation/   # 选手源码
│   │   └── Test_Vector/                     # KAT 测试向量 (.kat)
│   ├── API_CryptHash/              # 同上结构
│   └── API_PKC/                    # 同上结构
├── tests/                          # 测试框架（平台维护）
│   ├── include/                    # 公共头文件
│   │   ├── algorithm_interface.h   # 算法接口定义 (dlsym 符号)
│   │   ├── kat_parser.h            # KAT 解析器头文件
│   │   ├── perf_types.h            # 性能测试数据结构
│   │   ├── stability_test.h        # 稳定性测试数据结构
│   │   ├── report.h                # 报告输出头文件
│   │   └── types.h                 # 通用类型
│   ├── src/                        # 测试框架源码
│   │   ├── main.c                  # 程序入口 + 交互式菜单
│   │   ├── algo_loader.c           # dlopen/dlsym 加载算法库
│   │   ├── correctness_test.c      # 正确性测试 (KAT)
│   │   ├── kat_parser.c            # KAT 文件解析器
│   │   ├── performance_test.c      # 性能测试
│   │   ├── memory_test.c           # 内存测试
│   │   ├── stability_test.c        # 稳定性测试
│   │   └── report.c                # 输出报告
│   └── utils/                      # 工具头文件 (header-only)
│       ├── cycle_counter.h         # perf/TSC/ARMv8 PMU 周期计数抽象
│       └── welford.h               # Welford 在线统计
├── reports/                        # 测试报告输出目录
├── design/
│   └── design.md
└── README.md
```

---

## 10. 构建系统

### 10.1 构建命令

```bash
mkdir build && cd build && cmake .. && make
```

### 10.2 编译产物

构建系统同时编译两部分：

| 产物 | 输出位置 | 来源 |
|------|---------|------|
| `ngcc_test_framework` | `build/` | `tests/` 测试框架 |
| `libXXX_ref.so` | `build/lib/` | `code/API_XXX/.../Reference_Implementation/` |
| `libXXX_opt.so` | `build/lib/` | `code/API_XXX/.../Optimized_Implementation/` |
| `libXXX_add.so` | `build/lib/` | `code/API_XXX/.../Additional_Implementation/` |

### 10.3 编译选项

通过 `-DIMPL_TYPE` 选择编译哪些选手实现：

```bash
# 编译全部实现（默认）
cmake .. && make

# 仅编译 reference 实现
cmake .. -DIMPL_TYPE=reference && make

# 仅编译 optimized 实现
cmake .. -DIMPL_TYPE=optimized && make

# 仅编译 additional 实现
cmake .. -DIMPL_TYPE=additional && make
```

### 10.4 CMake 实现要点

- **选手代码**：CMake 自动扫描 `code/*/Implementations/*/` 下的 `.c` 文件，编译为对应 `.so`
- **测试框架**：编译 `tests/src/*.c` 为 `ngcc_test_framework` 可执行文件
- **运行时加载**：`ngcc_test_framework` 通过 `dlopen` 加载选手 `.so`，不在编译时链接
- **路径宏**：`CODE_DIR` 和 `REPORT_DIR` 作为编译时宏传递给测试框架

### 10.5 运行方式

```bash
# 测试指定的选手实现
./ngcc_test_framework ./lib/libCryptHash_ref.so

# 测试向量路径自动从 code/API_XXX/Test_Vector/ 读取
```

---
