# Architecture

## 1. 模块结构

核心目录：`ngcc_bench/`

- `src/main.c`：程序入口、模式分发、总体退出码
- `src/cli_parser.c`：CLI 参数解析与校验
- `src/interactive.c`：交互式菜单模式
- `src/json_report.c`：JSON 报告生成
- `src/loader.c`：`dlopen/dlsym` 按需加载 API 符号（按 `--test` 控制）
- `src/bench_core.c`：计时、跨平台随机数据填充、统一性能执行器
- `src/cycle_counter.c`：CPU 周期计数器（perf/TSC/ARMv8）
- `src/stats_util.c`：Welford 在线统计（均值/方差/标准差）
- `src/bench_hash.c`：Hash 正确性与性能
- `src/bench_sig.c`：SIG 聚合测试（correctness/performance/KAT），性能测试分别输出 keygen/sign/verify 指标
- `src/bench_kem.c`：KEM 正确性与性能（支持整体测试及 keygen/encap/decap 分离测试）
- `src/bench_kex.c`：KEX 正确性与性能
- `src/mem_stat.c`：RSS 当前值与峰值
- `src/stability.c`：稳定性循环执行与信号中断处理
- `src/kat_parser.c`：KAT 文件解析（用于 `hash/sig/kem/kex` correctness）

头文件位于：`ngcc_bench/include/`（包含 `cli_types.h` 共享类型定义）

## 2. 执行流程

1. 解析 CLI 参数并填默认值（无参数时进入交互菜单）。支持 `--version` 查看版本号。
2. 校验参数（例如 hash 被选中时必须有 `--digest-len-bits`）。
3. 调用加载器打开 `.so` 并按 `--test` 按需绑定对应 API 符号组。
4. 按 `--mode` 选择执行：
   - `correctness`：每类算法跑一次正确性检查。
   - `performance`：按 `iterations` 进行预热和计时循环。
   - `memory`：记录 baseline RSS，执行 correctness，再记录 peak RSS。
   - `stability`：循环 correctness，直到时间/计数上限或外部中断。
5. 汇总失败状态并返回进程退出码。
6. 如设置 `--json-out`，将本次执行结果写入 JSON 报告。

## 3. 四类测试定义

`correctness`
- Hash：默认对随机消息做重复摘要一致性检查；若传 `--kat` 则按向量文件执行 KAT 比对。
- SIG：
  - `sig`：`keygen -> sign -> verify`，作为聚合测试项。
- KEM：默认执行 `keygen -> enc -> dec` 比较共享密钥一致性；若传 `--kat` 则按向量文件执行 decap/shared-secret 比对。
- KEX：模拟 A/B 三次消息交换并各自导出共享密钥，比较一致性。

`performance`
- 使用统一执行器 `ngcc_run_performance_op()`。
- 预热策略：`max(10, iterations / 100)`。
- 指标：`elapsed_ms/total_ms`、`ops/s`、`bytes/s`、可选 `cycles/op`，以及 `min/mean/median/max/stddev/CV`（time/cycles）。
- SIG `--test sig` 自动输出 keygen/sign/verify 各子操作性能指标。
- KEM `--test kem` 自动输出 keygen/encap/decap 各子操作性能指标。

`memory`
- `static_mem`：读取算法库的 `text/data/bss/rodata` 段大小。
- `heap_baseline_bytes` / `heap_peak_bytes`：围绕 correctness 执行采集堆口径动态内存。
- 单位统一为字节（bytes）。

`stability`
- 采用“批处理窗口采样”：在一个 `stability-sample-ms` 窗口内执行多次 correctness，再作为一个统计样本写入在线统计。
- 每个样本统计吞吐、单次均摊耗时、可选周期、当前内存，并在线计算均值/标准差/CV；报告中会记录 `sample_count` 以标识统计样本数。
- 终止条件（任一满足即停）：
  - 运行时长达到 `duration-hours`
  - case 数达到 `stability-max-cases`（默认 `3000`）
  - 收到 `SIGINT/SIGTERM`
  - 任一 correctness 失败
- 稳定性分级阈值支持 CLI 配置（`--stable-*-percent` / `--warning-*-percent`，默认 stable: `5/5/5/1/0`，warning: `10/10/10/5/1`）。
- 产出 `STABLE/WARNING/UNSTABLE` 评估结果；`UNSTABLE` 计入失败返回码。

## 4. 性能计数实现细节

cycle 统计优先级：

1. `perf_event_open(PERF_COUNT_HW_CPU_CYCLES)`
2. `x86_64` 下回退 `lfence + rdtsc`（降低乱序误差）
3. `ARMv8` 下回退 `cntvct_el0` 计数器
4. 都不可用时只输出时间指标

随机数据来源（跨平台）：

1. macOS：`arc4random_buf()`
2. Linux：`getrandom(2)`，失败回退 `/dev/urandom`
3. 其他：`/dev/urandom`

## 5. 关键限制

- 加载器按 `--test` 按需加载对应符号组，`.so` 只需导出实际被测的算法符号。
- 内存指标是进程级口径，不是每个算法独立进程隔离口径；当前 Linux 路径最完整，非 Linux 平台上部分字段可能不可用。
- JSON 报告为可选功能，需通过 `--json-out` 显式启用。
- JSON 结构定义：`docs/json_schema_v4.json`（配套说明：`docs/json_schema.md`）。
- `--kat` 可用于 `hash/sig/kem/kex` 的 correctness；若某算法无可用向量，会回退到运行时回归检查。

## 6. 测试

- 已接入 CTest：`ngcc_cli_regression`、`ngcc_mock_mlkem`、`ngcc_mock_mldsa`、`ngcc_mock_mlkex`、`ngcc_unit_tests`
- CLI 回归测试程序：`tests/test_cli_regression.c`
- 单元测试：`tests/test_unit.c`（覆盖 stats/cli_parser/timespec 等功能）
- mock 动态库源码：`tests/mock/mock_ngcc.c`、`tests/mock/mock_hash_only.c`、`tests/mock/mock_mlkem.c`、`tests/mock/mock_mldsa.c`、`tests/mock/mock_mlkex.c`
- mock 回归测试程序：`tests/test_mock_mlkem.c`、`tests/test_mock_mldsa.c`、`tests/test_mock_mlkex.c`
- 稳定性专项程序：`tests/ngcc_stability_profile.c`
- 稳定性基线对比脚本：`tests/compare_stability_reports.py`
- 稳定性 CI 说明：`docs/stability_ci.md`
