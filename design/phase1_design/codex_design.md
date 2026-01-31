# 技术设计文档：加密接口 Benchmark 程序（C/CMake）

## 1. Requirement Understanding
用户需要一个使用 C 语言开发的 Benchmark 程序，用于调用外部动态库（Linux `.so`）中提供的加密接口进行功能正确性与性能评测。要求涵盖：
- 调用并测试 `CryptHash`、SIG、KEM、KEX 全套接口。
- 功能正确性验证（签名验签、KEM/KEX 共享密钥一致性、哈希输出长度与稳定性）。
- 性能指标：时钟周期数、吞吐量（ops/s）、耗时统计。
- 内存测试：静态内存与峰值内存。
- 稳定性测试：至少 6 小时或随机 3000 组测试数据。
- 构建系统使用 CMake。
- 动态加载 `dlopen` + `dlsym`，运行时指定测试库路径。

## 2. Technical Solution

### 2.1 Overall Architecture
- **CLI 驱动**：单一可执行程序 `ngcc_benchmark`，通过命令行参数控制测试项与模式。
- **动态库加载层**：封装 `dlopen/dlsym`，加载所有目标符号并做完整性校验。
- **Benchmark Harness**：通用计时、统计、循环控制、随机数据生成、结果汇总输出。
- **测试模块**：分别实现 Hash/SIG/KEM/KEX 的功能与性能路径。
- **内存与周期采集**：独立的测量模块，支持多种实现与回退。

### 2.2 CLI & 配置建议
示例参数（可按实现细化）：
- `--lib /path/to/libcrypto_impl.so`
- `--mode hash|sig|kem|kex|all`
- `--msg-len 32|256|1024|...`
- `--iter 10000` 或 `--duration 10s`
- `--stability 6h` 或 `--stability 3000`
- `--threads 1`（预留扩展）
- `--cycles on|off`（显式控制周期采集）
- `--report json|text`

### 2.3 动态库加载与符号绑定
- 在加载阶段构建 `struct api_vtable`，包含所有 API 函数指针。
- 任一符号缺失即返回错误并打印缺失列表。
- 支持用户指定 `.so` 路径。

### 2.4 正确性验证策略
- **Hash**：验证返回值、输出长度是否符合 `digest_len_bits`；可选支持外部测试向量文件（若用户提供）。
- **SIG**：`sig_keygen` 后进行签名与验签，验签必须成功。
- **KEM**：`kem_keygen` -> `kem_enc`/`kem_dec`，比较共享密钥一致性。
- **KEX**：执行完整消息流程，双方 `kex_derive_ss_*` 输出一致。

### 2.5 性能与周期测量
- **时间测量**：默认 `clock_gettime(CLOCK_MONOTONIC_RAW)`。
- **周期测量**：
  - 首选 `perf_event_open` 读取 `PERF_COUNT_HW_CPU_CYCLES`。
  - 若不可用，x86_64 可选 `rdtsc`（需明确提示环境限制）。
  - 若无法获取周期则输出 `N/A` 并继续性能统计。
- **吞吐量**：`ops/s = iterations / elapsed_seconds`。

### 2.6 内存测量
- **静态内存**：加载库与初始化后基线 RSS（例如 `/proc/self/statm` 或 `/proc/self/status`）。
- **峰值内存**：运行过程中的最大 RSS（`getrusage(RUSAGE_SELF).ru_maxrss` 或 `/proc/self/status` 的 VmHWM）。
- 报告以 KB/MB 输出，区分 baseline 与 peak。

### 2.7 稳定性测试
- 两种模式任选其一：
  - **时长模式**：持续运行 6 小时（21600 秒）。
  - **次数模式**：随机 3000 组测试数据。
- 记录失败次数、首次失败位置与输入摘要。

## 3. Implementation Plan

### 3.1 新增/修改文件（建议结构）
- `src/main.c`：CLI 解析、调度各模块。
- `src/loader.c`, `src/loader.h`：`dlopen/dlsym`、符号绑定与错误处理。
- `src/bench_common.c`, `src/bench_common.h`：计时、周期、统计、报告输出。
- `src/bench_hash.c`, `src/bench_sig.c`, `src/bench_kem.c`, `src/bench_kex.c`：功能+性能测试逻辑。
- `src/mem_stat.c`, `src/mem_stat.h`：RSS / peak 统计。
- `src/stability.c`, `src/stability.h`：稳定性测试逻辑（或合并到各模块）。
- `CMakeLists.txt`：构建与链接 `-ldl`，可选 `-lrt`。

### 3.2 实现步骤
1. **初始化项目结构**：CMake + 目录骨架。
2. **动态加载层**：定义函数指针表，绑定并校验。
3. **计时与周期模块**：实现多后端计数；失败回退。
4. **正确性测试逻辑**：先完成 Hash/SIG/KEM/KEX 基础流程。
5. **性能测试逻辑**：循环、吞吐量统计、统一报告。
6. **内存统计**：基线与峰值采样。
7. **稳定性模式**：按时长或次数运行，记录错误。
8. **输出格式**：文本与可选 JSON 结构化结果。
9. **文档与使用说明**：在 `README` 中提供示例命令。

## 4. Testing Strategy

### 4.1 功能正确性测试
- Hash：多长度消息、不同 `digest_len_bits`。
- SIG：签名后验签必须成功；可追加篡改签名测试（应失败）。
- KEM：封装与解封装共享密钥一致。
- KEX：完整三轮交互后共享密钥一致。

### 4.2 性能测试
- 在固定输入长度与迭代次数下测 `ops/s`。
- 可选多长度梯度（32B/256B/1KB/4KB）。
- 输出总耗时、均值耗时、吞吐量、周期数。

### 4.3 内存测试
- 记录加载后基线 RSS。
- 记录运行期间峰值 RSS。
- 若峰值明显高于基线，打印提示。

### 4.4 稳定性测试
- `--stability 6h`：长时间运行观测崩溃、错误码、内存趋势。
- `--stability 3000`：随机数据 3000 组快速覆盖。
- 输出失败统计与首个失败样例信息（长度、返回码）。

---

如果需要，我可以基于现有仓库结构进一步细化文件路径与命名，或直接进入实现阶段。
