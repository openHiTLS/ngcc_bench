# Stability CI / Soak

## 目标

- 把稳定性从“短时冒烟”扩展到“可重复的长跑专项”。
- 输出可归档 JSON，并支持基线对比。

## 本地执行

```bash
./build/ngcc_stability_profile \
  --benchmark ./build/ngcc_bench \
  --profile quick \
  --output-dir reports/stability
```

参数说明：

- `--profile quick|soak|nightly`
- `--lib /path/to/lib.so`（不传则默认使用 CMake 构建的 `mock_ngcc`）
- `--allow-unstable`（可选，允许命令非 0 继续）

## 基线对比

```bash
python3 tests/compare_stability_reports.py baseline.json current.json
```

默认策略：

- 稳定性等级不能比基线更差（`STABLE -> WARNING/UNSTABLE` 会失败）
- CV 指标允许相对回归 `+20%`
- 吞吐均值允许相对回归 `-10%`
- 时间/周期均值允许相对回归 `+10%`
- 内存增长允许绝对回归 `+1.0`
- 错误率允许绝对回归 `+0.0`

可通过参数调整：

```bash
python3 tests/compare_stability_reports.py \
  baseline.json current.json \
  --allow-cv-regress-ratio 0.10 \
  --allow-throughput-mean-regress-ratio 0.05 \
  --allow-time-mean-regress-ratio 0.05 \
  --allow-cycles-mean-regress-ratio 0.05 \
  --allow-memory-growth-abs 0.5 \
  --allow-error-rate-abs 0.0
```

## GitHub Actions

- `ci.yml`（分层：日常快速）：
  - `push` / `pull_request` 上执行构建 + `ctest` + `quick` 稳定性 profile。
  - 上传 `stability-quick-report` 产物。
- `stability-nightly.yml`（分层：长跑专项）：
  - 定时触发（UTC 03:30）：在 `self-hosted` runner 上矩阵执行 `soak`、`nightly`。
  - 手动触发：可选 runner（`self-hosted` / `ubuntu-latest`）、profile、lib_path、baseline_report。
  - 上传稳定性产物并支持可选基线对比。
