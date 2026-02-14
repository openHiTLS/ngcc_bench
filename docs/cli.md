# CLI Reference

`ngcc_bench` 支持两种入口：

- 直接运行 `./build/ngcc_bench` 进入交互式菜单
- 使用完整参数走非交互式 CLI（如下）
- 交互模式若选择 `stability`，会提示配置 10 个稳定性阈值（回车保持默认）

## 用法

```bash
./build/ngcc_bench \
  --lib /path/to/lib.so \
  --test hash|sig|kem|kex|all \
  --mode correctness|performance|memory|stability|all \
  [--iterations N] \
  [--duration-hours H] \
  [--stability-max-cases N] \
  [--stability-sample-ms MS] \
  [--msg-len BYTES] \
  [--digest-len-bits BITS] \
  [--cycles on|off] \
  [--stable-throughput-cv-percent P] \
  [--stable-cycles-cv-percent P] \
  [--stable-time-cv-percent P] \
  [--stable-memory-growth-percent P] \
  [--stable-error-rate-percent P] \
  [--warning-throughput-cv-percent P] \
  [--warning-cycles-cv-percent P] \
  [--warning-time-cv-percent P] \
  [--warning-memory-growth-percent P] \
  [--warning-error-rate-percent P] \
  [--json-out PATH] \
  [--kat FILE]
```

## 参数说明

`--lib`
- 必选。
- 待测动态库路径。

`--test`
- 必选。
- 取值：`hash` / `sig` / `kem` / `kex` / `all`。

`--mode`
- 必选。
- 取值：`correctness` / `performance` / `memory` / `stability` / `all`。

`--iterations`
- 可选，默认 `1000`。
- 用于 `performance` 模式，每个测试项执行的迭代次数。
- 要求：正整数。

`--duration-hours`
- 可选，默认 `6.0`。
- 用于 `stability` 模式，最长运行小时数。
- 要求：大于 0。

`--msg-len`
- 可选，默认 `1024` 字节。
- Hash/SIG 使用的消息长度。
- 要求：正整数。

`--stability-max-cases`
- 可选，默认 `3000`。
- 用于 `stability` 模式，最大 case 数上限。
- 要求：正整数。

`--stability-sample-ms`
- 可选，默认 `1.0`。
- 用于 `stability` 模式，每个统计样本的目标聚合窗口（毫秒）。
- 值越大，抖动越小但反馈越慢；值越小，响应更快但更容易受计时量化噪声影响。
- 要求：大于 0 的有限数。

`--digest-len-bits`
- 可选，但当 `--test` 包含 `hash` 时为必选。
- Hash 输出摘要位长度。
- 要求：正整数。

`--cycles`
- 可选，默认 `on`。
- `on`：尝试输出 `cycles/op`。
- `off`：只输出时间与吞吐。

`--json-out`
- 可选。
- 指定 JSON 报告输出路径。
- 程序仍会输出控制台日志；JSON 为额外产物。

`--kat`
- 可选。
- 指定 correctness 的 KAT 文件。
- 需要 `--mode` 包含 `correctness`。
- 各算法字段模板：
- Hash: `INPUT`/`MSG` + `OUTPUT`/`DIGEST`
- SIG: `PK` + `MSG`/`INPUT` + `SN`/`SIG`/`OUTPUT`
- KEM: `SK` + `CT` + `SS`/`OUTPUT`
- KEX-A: `SKA` + `PKB` + `M2` + `STA` + `SSA`
- KEX-B: `SKB` + `PKA` + `M3` + `STB` + `SSB`
- 若某算法在 KAT 文件中无可用向量，会自动回退到随机 correctness。
- 兼容：支持 `#`/`;`/`//` 注释、`0x` 前缀、hex 分隔符（空白/`:`/`_`）和常见字段别名；`*LEN` 等元数据行会被忽略。

`--stable-*-percent` / `--warning-*-percent`
- 可选，单位均为 `%`，用于稳定性分级阈值（`STABLE/WARNING/UNSTABLE`）。
- 稳定档（默认）：
- `throughput_cv=5`，`cycles_cv=5`，`time_cv=5`，`memory_growth=1`，`error_rate=0`。
- 警告档（默认）：
- `throughput_cv=10`，`cycles_cv=10`，`time_cv=10`，`memory_growth=5`，`error_rate=1`。
- 要求：非负数，且每个 `warning` 阈值必须 `>=` 对应 `stable` 阈值。

## 默认值

- `test = all`
- `mode = all`
- `iterations = 1000`
- `duration-hours = 6.0`
- `stability-max-cases = 3000`
- `stability-sample-ms = 1.0`
- `msg-len = 1024`
- `cycles = on`
- `stable-throughput-cv-percent = 5`
- `stable-cycles-cv-percent = 5`
- `stable-time-cv-percent = 5`
- `stable-memory-growth-percent = 1`
- `stable-error-rate-percent = 0`
- `warning-throughput-cv-percent = 10`
- `warning-cycles-cv-percent = 10`
- `warning-time-cv-percent = 10`
- `warning-memory-growth-percent = 5`
- `warning-error-rate-percent = 1`

## 返回码

- `0`：所有被选中的测试/模式通过。
- `1`：任一测试失败、参数错误、库加载失败、符号缺失，或稳定性评估为 `UNSTABLE`。

JSON 报告：
- 当前 `schema_version = 3`。
- `options.stability_thresholds` 记录本次稳定性阈值配置。
- schema 定义见：`docs/json_schema_v3.json`。

## 输出字段

正确性：

```text
[kem][correctness] PASS
```

正确性（hash + KAT）：

```text
[hash][correctness] PASS total=128 passed=128 failed=0 source=kat
```

性能（可用 cycles）：

```text
[sig][performance] ops=1000 warmup=10 elapsed_ms=88.123 ops/s=11347.008
[sig][performance][time] min_ms=0.070000 mean_ms=0.088123 median_ms=0.086000 max_ms=0.130000 stddev_ms=0.010000 cv=11.348%
[sig][performance][cycles] min=28000.000 mean=30211.200 median=30010.000 max=35800.000 stddev=1300.000 cv=4.303%
```

性能（cycles 不可用或关闭）：

```text
[sig][performance] ops=1000 warmup=10 elapsed_ms=88.123 ops/s=11347.008 cycles=unavailable
[sig][performance][time] min_ms=0.070000 mean_ms=0.088123 median_ms=0.086000 max_ms=0.130000 stddev_ms=0.010000 cv=11.348%
```

内存：

```text
[memory] baseline_bytes=12328960 peak_bytes=15400960
```

稳定性：

```text
[kex][stability] STABLE cases=3000 elapsed_s=456.789
[kex][stability][throughput] mean=1456789.000 stddev=33506.000 cv=2.300% min=1398234.000 max=1512456.000
[kex][stability][cycles] mean=1672.000 stddev=38.000 cv=2.270% min=1523.000 max=2103.000
[kex][stability][time] mean_ms=0.686500 stddev_ms=0.015800 cv=2.300% min_ms=0.640000 max_ms=0.740000
[kex][stability][memory] start=12582912 end=12648448 min=12582912 max=12664832 growth=0.521%
[kex][stability][errors] total=3000 failed=0 rate=0.000000% status=STABLE
```

中断稳定性测试（`Ctrl+C`）时：

```text
[kex][stability] STOPPED cases=120 elapsed_s=33.102
```

稳定性不通过示例：

```text
[sig][stability] UNSTABLE cases=3000 elapsed_s=12.003
[sig][stability][reason] performance fluctuation; memory growth;
```

## 示例

仅跑 KEX 性能：

```bash
./build/ngcc_bench \
  --lib /path/to/libalgo.so \
  --test kex \
  --mode performance \
  --iterations 2000
```

跑全部测试与模式（包含 hash，需给摘要位数）：

```bash
./build/ngcc_bench \
  --lib /path/to/libalgo.so \
  --test all \
  --mode all \
  --digest-len-bits 256 \
  --kat vectors/hash.kat \
  --json-out reports/full_report.json
```
