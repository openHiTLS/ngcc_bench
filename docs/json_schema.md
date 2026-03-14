# JSON Schema (v4)

当前报告版本：`schema_version = 4`。  
机器可读 schema 文件：`docs/json_schema_v4.json`。

当前 `tests` 节点包含四项：`hash`、`sig`、`kem`、`kex`。

## 兼容策略

- 小版本兼容：在同一 `schema_version` 下允许新增字段，但不应破坏既有字段语义。
- 破坏性变更：必须提升 `schema_version`。
- 下游消费方应先检查 `schema_version`，再按对应 schema 解析。

`v4` 相对 `v3` 的主要变化：

- `tests.*.stability` 从二元 `PASS/FAIL` 调整为保留原始稳定性等级 `STABLE/WARNING/UNSTABLE`（仍保留 `FAIL/STOPPED/SKIPPED`）。
- `tests.*.stability_metrics` 新增 `sample_count`，用于标识窗口统计样本数。

当前生成器还会额外写出两个 `v4` 兼容扩展字段：

- `report_metadata`：记录生成器名、生成器版本、当前 JSON 输出路径。
- `environment`：记录 `hostname/cwd/sysname/release/version/machine`，用于复现实验环境。

## 本地校验

回归脚本已包含结构校验：

```bash
ctest --test-dir build --output-on-failure
```

其中 `ngcc_cli_regression` 会生成 JSON 报告并运行 `tests/validate_json_report.py` 进行关键字段与约束检查。
