# JSON Schema (v4)

当前报告版本：`schema_version = 4`。  
机器可读 schema 文件：`docs/json_schema_v4.json`。

当前 `tests` 节点包含顶层聚合项 `hash`、`dsa`、`kem`、`kex`，以及保留的细分项 `dsa-keygen`、`dsa-sig`、`dsa-verify`、`kem-keygen`、`kem-encap`、`kem-decap`。

## 兼容策略

- 小版本兼容：在同一 `schema_version` 下允许新增字段，但不应破坏既有字段语义。
- 破坏性变更：必须提升 `schema_version`。
- 下游消费方应先检查 `schema_version`，再按对应 schema 解析。

`v4` 相对 `v3` 的主要变化：

- `tests.*.stability` 从二元 `PASS/FAIL` 调整为保留原始稳定性等级 `STABLE/WARNING/UNSTABLE`（仍保留 `FAIL/STOPPED/SKIPPED`）。
- `tests.*.stability_metrics` 新增 `sample_count`，用于标识窗口统计样本数。

## 本地校验

回归脚本已包含结构校验：

```bash
ctest --test-dir build --output-on-failure
```

其中 `ngcc_cli_regression` 会生成 JSON 报告并运行 `tests/validate_json_report.py` 进行关键字段与约束检查。
