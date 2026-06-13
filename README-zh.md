# ngcc_bench

[English](README.md)

基于 C11 的跨平台 NGCC 算法测试基准工具（Linux / macOS）。程序通过
`dlopen` / `dlsym` 动态加载被测动态库，并执行正确性、性能、内存和稳定性测试。

## 测试目标

| 目标 | correctness | performance |
| --- | --- | --- |
| `hash` | 对同一随机消息重复计算摘要并比对，或执行 KAT | 按固定消息长度分别输出吞吐量和耗时 |
| `sig` | 执行 `keygen + sign + verify`，或执行 verify KAT | 分别输出 `keygen`、`sign`、`verify` 指标 |
| `kem` | 执行 `keygen + encap + decap`，或执行 decap KAT | 分别输出 `keygen`、`encap`、`decap` 指标 |
| `kex` | 执行完整密钥协商链路，或执行 KAT | 分别输出 `derive_ss_a`、`derive_ss_b` 指标 |
| `all` | 依次执行全部四类算法 | 依次执行全部四类算法 |

测试模式：

- `correctness`
- `performance`
- `memory`
- `stability`
- `all`

## 构建

要求：

- CMake >= 3.16
- 支持 C11 的 GCC 或 Clang
- Linux 下需要 `libdl`、`libm`

```bash
mkdir build
cd build
cmake ..
make -j
```

主程序：

```bash
./ngcc_bench
```

被测动态库不需要在编译 `ngcc_bench` 时配置。直接运行 `./ngcc_bench` 会进入交互模式，
程序会提示输入动态库路径。需要脚本化执行时，也可以使用 `--lib PATH` 通过命令行传入。
两种方式最终都会使用 `dlopen` / `dlsym` 动态加载被测库。

## 快速开始

无参数运行会进入交互模式：

```bash
./ngcc_bench
```

程序会依次提示输入动态库路径、测试目标、测试模式，以及当前模式需要的附加配置。

查看帮助或版本：

```bash
./ngcc_bench --help
./ngcc_bench --version
```

以下为适合脚本或 CI 使用的非交互式示例。

执行 KEM correctness：

```bash
./ngcc_bench \
  --lib /path/to/libalgo.so \
  --test kem \
  --mode correctness
```

执行 SIG performance：

```bash
./ngcc_bench \
  --lib /path/to/libalgo.so \
  --test sig \
  --mode performance
```

执行全量测试并输出中英文 JSON 报告：

```bash
./ngcc_bench \
  --lib /path/to/libalgo.so \
  --test all \
  --mode all \
  --digest-len-bits 256 \
  --json-out full_report.json
```

该命令实际生成：

```text
full_report.json.zh
full_report.json.en
```

## CLI 参数

| 参数 | 说明 | 默认值 |
| --- | --- | --- |
| `--lib PATH` | 被测动态库路径；仅非交互模式必填 | 交互模式中提示输入 |
| `--test hash\|sig\|kem\|kex\|all` | 测试目标 | `all` |
| `--mode correctness\|performance\|memory\|stability\|all` | 测试模式 | `all` |
| `--digest-len-bits BITS` | Hash 摘要位数；选中 `hash` 时必填 | 无 |
| `--duration-hours H` | stability 最长运行时间，必须大于 `0` | `6.0` |
| `--stability-max-cases N` | stability 最大 case 数，必须大于 `0` | `3000` |
| `--kat DIR` | KAT 向量目录，仅在包含 `correctness` 的模式下可用 | 无 |
| `--json-out PATH` | JSON 输出前缀，实际写出 `PATH.zh` 和 `PATH.en` | 无 |
| `--help` | 输出帮助 | - |
| `--version` | 输出版本 | - |

当前以下配置不可通过 CLI 修改：

- performance 迭代次数：`10000`
- Hash performance 消息长度：`32`、`128`、`512`、`1024`、`4096`、`8192`、`16384`、`65536` 字节
- stability 消息长度：`131072` 字节
- stability 采样窗口：`1.0` 毫秒
- stability 会尝试采集 cycles；平台不支持时输出 `unavailable`
- stability 判定阈值：吞吐 CV `< 5%`、耗时 CV `< 5%`、cycles CV `< 5%`、内存增长绝对值 `< 1%`、错误率 `<= 0%`

## KAT 目录

`--kat` 接收目录，不接收单个文件。程序会按测试目标筛选目录中的文件：

| 目标 | 文件名前缀 |
| --- | --- |
| `hash` | `KAT_2_12_`、`KAT_2_23_`、`KAT_2_33_`、`KAT_Loop_`，四类均必需 |
| `sig` | `KAT_SIG_` |
| `kem` | `KAT_KEM_` |
| `kex` | `KAT_KEX_` |

KAT 解析支持 `#`、`;`、`//` 注释，以及 `0x` 前缀和常见字段别名。

## 输出与限制

- 加载器按 `--test` 按需加载符号；动态库只需导出实际被测算法的符号。
- `memory` 会为每个算法启动独立 helper 进程，并在 JSON 中输出 `static_memory_bytes` 与 `peak_memory_bytes`。
- 内存指标使用 Linux `VmSize`/`VmPeak`；非 Linux 平台可能不支持该模式。
- stability 输出 `STABLE`、`UNSTABLE`，收到 `SIGINT` / `SIGTERM` 时输出 `STOPPED`。
- stability 为 `UNSTABLE`、测试失败、参数错误或缺失符号时，程序返回非 `0`。
- `--json-out` 生成中英文双份 JSON 报告。

## 动态库接口

被测动态库需要导出所选算法对应的接口符号。接口定义见
`ngcc_bench/include/ngcc_api.h`。
