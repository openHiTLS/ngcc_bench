# JSON Schema (v3)

当前报告版本：`schema_version = 3`。  
机器可读 schema 文件：`docs/json_schema_v3.json`。

## 兼容策略

- 小版本兼容：在 `schema_version=3` 下允许新增字段，但不应破坏既有字段语义。
- 破坏性变更：必须提升 `schema_version`。
- 下游消费方应先检查 `schema_version`，再按对应 schema 解析。

## 本地校验

回归脚本已包含结构校验：

```bash
ctest --test-dir build --output-on-failure
```

其中 `ngcc_cli_regression` 会生成 JSON 报告并运行 `tests/validate_json_report.py` 进行关键字段与约束检查。
