# 设计一致性对照（基于实现）

对照基准：

- 设计文档：`design/design.md`
- 实现：`ngcc_bench/src/*.c`、`ngcc_bench/include/*.h`

更新时间：2026-02-14

## 1. 总体结论

- 与 `design/design.md`：主体一致，但其中仍保留部分目标设计/扩展草案，和当前实现并非完全一一对应。

## 2. 已实现且一致

- 动态库加载：`dlopen/dlsym` 全符号绑定，缺符号即失败。
- 测试目标：`hash/sig/kem/kex`，性能测试自动输出子操作指标。
- 四类模式：`correctness/performance/memory/stability`。
- 性能回退策略：优先 `perf_event_open`，`x86_64` 回退 `rdtsc`，`ARMv8` 回退 `cntvct_el0`，不可用时仅时间指标。
- 稳定性测试：支持时长上限、case 上限、`SIGINT/SIGTERM` 优雅中断。
- 内存指标：`baseline RSS + peak RSS`（字节）。
- 交互式入口：无参数运行可进入菜单配置执行。
- KAT correctness：已支持 `hash/sig/kem/kex`（`--kat`）。
- 性能统计：已输出 min/mean/median/max/stddev/CV，并补充 `bytes/s` 与总时间。

## 3. 本次补充/修正（已落地）

- 新增 `--stability-max-cases`，把稳定性 case 上限从“内部固定常量”改为“CLI 可配置（默认 3000）”。
- 新增 `--json-out PATH`，可导出本次执行的 JSON 报告（含参数、各算法状态、性能/稳定性指标、内存结果、整体状态）。
- 新增 `--kat FILE`，`hash/sig-verify/kem/kex` correctness 支持向量文件（无匹配向量时回退随机 correctness）。
- 新增交互式菜单入口（无参数运行）。
- 性能输出新增分布统计字段（time/cycles）。
- 新增稳定性阈值可配置参数（`stable-*`/`warning-*`），并在 `validate_options` 中校验 `warning >= stable`。
- 新增稳定性采样窗口参数 `--stability-sample-ms`，稳定性统计改为窗口内聚合采样，降低短函数计时噪声。
- JSON 报告当前为 `schema_version=4`，`options.stability_thresholds` 记录阈值配置，`tests.*.stability` 保留原始稳定性等级。
- 增加本地自动回归：`ctest` 下 `ngcc_cli_regression` 覆盖 CLI/KAT/阈值/JSON 关键路径。
- 新增 JSON schema 治理文件：`docs/json_schema_v4.json` + `docs/json_schema.md`，回归中执行结构校验。
- 新增稳定性专项与 CI 基础设施：`tests/ngcc_stability_profile.c`、`tests/compare_stability_reports.py`、`.github/workflows/stability-nightly.yml`。
- CI 分层策略已落地：`ci.yml` 跑 PR/Push quick 稳定性，`stability-nightly.yml` 跑定时 soak/nightly。
- 刷新文档：`README.md`、`docs/cli.md`、`docs/architecture.md`、`design/design.md`。

## 4. 仍不一致（设计项未实现）

以下内容在 `design/design.md` 中出现，但当前实现未覆盖：

- 多内存工具链（`size`/`/proc/self/status`/`heaptrack`）及泄漏统计。
- “始终双输出”策略（控制台 + JSON）。当前是控制台默认，JSON 通过 `--json-out` 可选开启。
- KAT 字段模板已接入四类算法，但尚未做“按算法分文件/目录自动发现”的完整工作流。
- 综合报告聚合与 `reports/` 自动命名落盘规范化流程。

## 5. 建议的后续对齐顺序

1. 增加内存工具链集成（`/proc/self/status`、`heaptrack`）与泄漏汇总。
2. 实现综合报告自动归档（`reports/` + 时间戳命名 + 全量聚合 JSON）。
3. 增加 KAT 目录自动发现与按算法模板校验。
4. 增加稳定性长跑专项回归（小时级、绑定 CPU 核、固定频率）并落地 CI 流水线。
