# Architecture

## 1. 模块结构

核心目录：`ngcc_bench/`

- `src/main.c`：CLI/交互菜单、模式分发、总体退出码、JSON 报告输出
- `src/loader.c`：`dlopen/dlsym` 加载并绑定 API
- `src/bench_core.c`：计时、cycle 计数、随机数据填充、统一性能执行器
- `src/bench_hash.c`：Hash 正确性与性能
- `src/bench_sig.c`：SIG 正确性与性能
- `src/bench_kem.c`：KEM 正确性与性能
- `src/bench_kex.c`：KEX 正确性与性能
- `src/mem_stat.c`：RSS 当前值与峰值
- `src/stability.c`：稳定性循环执行与信号中断处理
- `src/kat_parser.c`：KAT 文件解析（用于 `hash/sig/kem/kex` correctness）

头文件位于：`ngcc_bench/include/`

## 2. 执行流程

1. 解析 CLI 参数并填默认值（无参数时进入交互菜单）。
2. 校验参数（例如 hash 被选中时必须有 `--digest-len-bits`）。
3. 调用加载器打开 `.so` 并绑定全部 API。
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
- SIG：`keygen -> sign -> verify`。
- KEM：`keygen -> enc -> dec`，比较共享密钥一致性。
- KEX：模拟 A/B 三次消息交换并各自导出共享密钥，比较一致性。

`performance`
- 使用统一执行器 `ngcc_run_performance_op()`。
- 预热策略：`max(10, iterations / 100)`。
- 指标：`elapsed_ms/total_ms`、`ops/s`、`bytes/s`、可选 `cycles/op`，以及 `min/mean/median/max/stddev/CV`（time/cycles）。

`memory`
- `baseline_bytes`：执行 correctness 前读取当前 RSS。
- `peak_bytes`：通过 `getrusage(RUSAGE_SELF).ru_maxrss` 读取进程峰值 RSS。
- 单位统一为字节（bytes）。

`stability`
- 采用“批处理窗口采样”：在一个 `stability-sample-ms` 窗口内执行多次 correctness，再作为一个统计样本写入在线统计。
- 每个样本统计吞吐、单次均摊耗时、可选周期、当前内存，并在线计算均值/标准差/CV。
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

随机数据来源：

1. 首选 `getrandom(2)`
2. 失败回退 `/dev/urandom`

## 5. 关键限制

- 加载器是全量符号解析，不支持“按测试子集懒加载”。
- 内存指标是进程级口径，不是每个算法独立进程隔离口径。
- JSON 报告为可选功能，需通过 `--json-out` 显式启用。
- JSON 结构定义：`docs/json_schema_v3.json`（配套说明：`docs/json_schema.md`）。
- `--kat` 可用于 `hash/sig/kem/kex` 的 correctness；若某算法无可用向量，会回退到运行时回归检查。

## 6. 回归测试

- 已接入 CTest：`ngcc_cli_regression`
- 测试脚本：`tests/run_cli_regression.sh`
- mock 动态库源码：`tests/mock/mock_ngcc.c`
- 稳定性专项脚本：`tests/run_stability_profile.sh`
- 稳定性基线对比脚本：`tests/compare_stability_reports.py`
- 稳定性 CI 说明：`docs/stability_ci.md`
