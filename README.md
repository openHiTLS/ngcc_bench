# ngcc_bench

基于 C11 的跨平台 NGCC 算法测试基准工具（Linux / macOS）。程序通过 `dlopen/dlsym` 动态加载被测 `.so`，统一执行 `correctness`、`performance`、`memory`、`stability` 四类测试。

## 测试目标

- 测试目标：`hash`、`sig`、`kem`、`kex`

说明：
- `sig` 聚合执行 `keygen + sign + verify`；性能测试分别输出 keygen/sign/verify 指标。
- `kem` 聚合执行 `keygen + encap + decap`；性能测试分别输出 keygen/encap/decap 指标。
- `all` 包含所有四项：`hash`、`sig`、`kem`、`kex`。

## 构建

要求：
- CMake >= 3.16
- GCC/Clang（C11）
- `libdl`、`libm`

```bash
cmake -S . -B build
cmake --build build -j
```

主程序：

```bash
./build/ngcc_bench
```

若仓库内存在 `code/API_*/Implementations/*`，可选：

```bash
cmake -S . -B build -DIMPL_TYPE=all
```

## 快速开始

查看帮助：

```bash
./build/ngcc_bench --help
```

聚合 KEM correctness：

```bash
./build/ngcc_bench --lib /path/to/libalgo.so --test kem --mode correctness
```

SIG performance（子操作分别输出 keygen/sign/verify）：

```bash
./build/ngcc_bench \
  --lib /path/to/libalgo.so \
  --test sig \
  --mode performance
```

包含 hash 的全量聚合测试：

```bash
./build/ngcc_bench \
  --lib /path/to/libalgo.so \
  --test all \
  --mode all \
  --digest-len-bits 256 \
  --json-out reports/full_report.json
```

## 关键参数

- `--test hash|sig|kem|kex|all`
- `--mode correctness|performance|memory|stability|all`
- `--digest-len-bits BITS`：选中 `hash` 时必填
- `--msg-len BYTES`：Hash/SIG 消息长度，默认 `1024`
- `--iterations N`：performance 迭代次数，默认 `1000`
- `--duration-hours H`：stability 最长时长，默认 `6.0`
- `--stability-max-cases N`：stability 最大 case，默认 `3000`
- `--stability-sample-ms MS`：stability 采样窗口，默认 `1.0`
- `--cycles on|off`：是否尝试输出 cycles，默认 `on`
- `--kat FILE`：仅 correctness 模式有效；支持 `hash`、`sig`、`kem`、`kex`
- `--json-out PATH`：输出 JSON 报告

## 输出与限制

- `memory` 输出分为 `[memory][static]` 和 `[memory][dynamic]`
- `stability` 输出 `STABLE` / `WARNING` / `UNSTABLE`，其中 `UNSTABLE` 会导致失败返回码
- `sig`、`kem` 性能测试自动输出子操作指标（keygen/sign/verify, keygen/encap/decap）
- 加载器按测试目标按需加载符号；未选中的算法符号可以不导出
- `memory` 指标当前以 Linux 路径最完整；非 Linux 平台上部分字段可能返回 `unavailable` 或 `0`
- KAT 解析支持 `#` / `;` / `//` 注释、`0x` 前缀和常见字段别名

## 测试

```bash
ctest --test-dir build --output-on-failure
```

附加脚本：

```bash
./build/ngcc_cli_regression_test ./build/ngcc_bench ./build/tests/mock/libmock_ngcc.so ./build/tests/mock/libmock_hash_only.so
./build/ngcc_stability_profile --benchmark ./build/ngcc_bench --profile quick
python3 tests/validate_json_report.py report.json docs/json_schema_v4.json
```

## 文档

- CLI 说明：`docs/cli.md`
- JSON schema：`docs/json_schema.md`、`docs/json_schema_v4.json`
- 架构说明：`docs/architecture.md`
- 动态库契约：`docs/library_contract.md`
