# ngcc_bench

基于 Linux/C 的 NGCC 算法测试基准工具。  
程序通过 `dlopen/dlsym` 动态加载被测 `.so`，对统一 API 进行四类测试：

- `correctness`
- `performance`
- `memory`
- `stability`

支持算法类别：

- `hash`
- `sig`
- `kem`
- `kex`

## 1. 当前代码状态

- 主可执行文件：`ngcc_bench`（来源：`ngcc_bench/src/*.c`）
- 备用测试框架目标：`ngcc_test_framework`（仅当 `tests/src` 存在源码时构建）
- `tests/` 目录当前只有占位文件，默认不会生成 `ngcc_test_framework` 可执行文件

## 2. 构建

要求：

- Linux
- CMake >= 3.16
- GCC/Clang（支持 C11）
- `libdl`、`libm`

构建命令：

```bash
cmake -S . -B build
cmake --build build -j
```

构建完成后主程序位于：

```bash
./build/ngcc_bench
```

可选：如果仓库内存在 `code/API_*/Implementations/*`，可通过 `IMPL_TYPE` 控制自动构建的实现类型：

```bash
cmake -S . -B build -DIMPL_TYPE=all
# 可选值: reference | optimized | additional | all
```

## 3. 快速开始

查看帮助：

```bash
./build/ngcc_bench --help
```

进入交互式菜单：

```bash
./build/ngcc_bench
```

当交互模式选择 `stability` 时，会额外提示 10 个稳定性阈值（可直接回车使用默认值）。

最小示例（仅 KEM 正确性）：

```bash
./build/ngcc_bench \
  --lib /path/to/libalgo.so \
  --test kem \
  --mode correctness
```

哈希测试必须给 `--digest-len-bits`：

```bash
./build/ngcc_bench \
  --lib /path/to/libalgo.so \
  --test hash \
  --mode all \
  --digest-len-bits 256 \
  --msg-len 1024 \
  --iterations 5000 \
  --stability-max-cases 3000 \
  --kat vectors/hash.kat \
  --json-out reports/full_report.json
```

## 4. 命令行参数

必选参数：

- `--lib /path/to/lib.so`
- `--test hash|sig|kem|kex|all`
- `--mode correctness|performance|memory|stability|all`

可选参数：

- `--iterations N`（默认 `1000`）
- `--duration-hours H`（默认 `6.0`）
- `--stability-max-cases N`（默认 `3000`）
- `--stability-sample-ms MS`（默认 `1.0`，稳定性单次统计窗口毫秒数）
- `--msg-len BYTES`（默认 `1024`）
- `--digest-len-bits BITS`（`hash` 被选中时必须提供）
- `--cycles on|off`（默认 `on`）
- `--kat FILE`（可选，correctness 优先使用 KAT 向量）
- `--json-out PATH`（可选，输出 JSON 报告）
- 稳定性阈值（单位均为 `%`）：
- `--stable-throughput-cv-percent P`（默认 `5`）
- `--stable-cycles-cv-percent P`（默认 `5`）
- `--stable-time-cv-percent P`（默认 `5`）
- `--stable-memory-growth-percent P`（默认 `1`）
- `--stable-error-rate-percent P`（默认 `0`）
- `--warning-throughput-cv-percent P`（默认 `10`）
- `--warning-cycles-cv-percent P`（默认 `10`）
- `--warning-time-cv-percent P`（默认 `10`）
- `--warning-memory-growth-percent P`（默认 `5`）
- `--warning-error-rate-percent P`（默认 `1`）

## 5. 输出格式（示例）

```text
[hash][correctness] PASS
[hash][correctness] PASS total=128 passed=128 failed=0 source=kat
[hash][performance] ops=1000 warmup=10 elapsed_ms=12.345 total_ms=12.345 ops/s=80971.234 bytes/op=1024.000 bytes/s=82914543.616
[hash][performance][time] min_ms=0.011 mean_ms=0.012 median_ms=0.012 max_ms=0.018 stddev_ms=0.001 cv=5.211%
[hash][performance][cycles] min=390.000 mean=401.200 median=399.000 max=455.000 stddev=12.300 cv=3.064%
[memory] baseline_bytes=12328960 peak_bytes=15400960
[kem][stability] STABLE cases=3000 elapsed_s=123.456
[kem][stability][throughput] mean=12345.678 stddev=123.456 cv=1.000% min=12000.000 max=12600.000
[kem][stability][throughput_bytes] mean=34567890.123 stddev=345678.901 cv=1.000% min=34000000.000 max=35000000.000 bytes/case=2800.000
[kem][stability][memory] start=12328960 end=12394496 min=12328960 max=12410880 growth=0.531%
[kem][stability][errors] total=3000 failed=0 rate=0.000000% status=STABLE
```

如果硬件 cycle 计数不可用，程序会退化为仅时间统计（`ops/s` / `bytes/s` 仍会输出）。
cycle 统计优先级为：`perf_event_open` -> `x86_64 rdtsc` -> `ARMv8 cntvct_el0`。

## 6. 重要限制

- 加载器会一次性解析全部 API 符号。即使你只测 `hash`，`.so` 也必须实现 `hash + sig + kem + kex` 全部符号，否则加载失败。
- `memory` 模式统计的是进程级 RSS（baseline/peak），不是单算法隔离内存。
- `stability` 会在“达到持续时长”或“达到最大 case 数（默认 3000）”任一条件时停止；收到 `SIGINT/SIGTERM` 会优雅中断。
- `stability` 按 `stability-sample-ms` 聚合多次 correctness 后再统计一次样本，可降低超快算法的计时抖动误判。
- `--kat` 已支持 `hash/sig/kem/kex`。若某算法在该 KAT 文件里找不到可用字段，会自动回退到随机 correctness。
- JSON 报告默认不输出；传入 `--json-out` 后会写入指定路径。
- JSON 报告 `schema_version=3`，`options.stability_thresholds` 会记录本次阈值配置。
- 稳定性模式会输出 `STABLE/WARNING/UNSTABLE`。其中 `UNSTABLE` 会导致进程返回失败码。

KAT 常用字段（十六进制）：

- Hash: `INPUT`/`MSG` + `OUTPUT`/`DIGEST`
- SIG: `PK` + `MSG`/`INPUT` + `SN`/`SIG`/`OUTPUT`
- KEM: `SK` + `CT` + `SS`/`OUTPUT`
- KEX-A: `SKA` + `PKB` + `M2` + `STA` + `SSA`
- KEX-B: `SKB` + `PKA` + `M3` + `STB` + `SSB`

兼容性说明：
- KAT 解析支持 `#` / `;` / `//` 注释、`0x` 前缀、十六进制中的空白/`:`/`_` 分隔符。
- 支持常见字段别名（如 `MD`、`SIGNATURE`、`CIPHERTEXT`、`SHAREDSECRETA/B` 等）。
- 非关键元数据行（如 `*LEN`）会自动忽略。

## 7. 文档

- CLI 详细说明：`docs/cli.md`
- JSON 报告 schema：`docs/json_schema.md`、`docs/json_schema_v3.json`
- 动态库符号契约：`docs/library_contract.md`
- 架构与执行流程：`docs/architecture.md`
- 稳定性专项/CI：`docs/stability_ci.md`
- 设计一致性对照：`docs/design_alignment.md`

## 8. 回归测试

```bash
ctest --test-dir build --output-on-failure
```

当前包含 `ngcc_cli_regression`：自动构建 mock 动态库并校验 CLI/KAT/阈值/JSON 关键路径。

## 9. 稳定性专项

快速稳定性 profile（自动使用 mock so）：

```bash
./tests/run_stability_profile.sh \
  --benchmark ./build/ngcc_bench \
  --profile quick \
  --output-dir reports/stability
```

基线对比：

```bash
python3 tests/compare_stability_reports.py \
  baseline.json \
  reports/stability/stability_quick_YYYYMMDD_HHMMSS.json
```

CI 工作流：

- `.github/workflows/ci.yml`：PR/Push 构建 + `ctest` + `quick` 稳定性
- `.github/workflows/stability-nightly.yml`：定时 `soak/nightly`（self-hosted）+ 手动稳定性专项（可选 baseline 对比）
