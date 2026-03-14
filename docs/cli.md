# CLI Reference

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
  [--json-out PATH] \
  [--kat FILE]
```

无参数运行会进入交互模式。

## `--test`

- `hash`：Hash 测试
- `sig`：聚合执行 `keygen + sign + verify`；性能测试分别输出各子操作指标
- `kem`：聚合执行 `keygen + encap + decap`；性能测试分别输出各子操作指标
- `kex`：KEX 整体链路测试
- `all`：运行所有四项

## `--mode`

- `correctness`
- `performance`
- `memory`
- `stability`
- `all`

## 常用参数

- `--digest-len-bits`：选中 `hash` 时必填
- `--msg-len`：Hash/SIG 消息长度，默认 `1024`
- `--iterations`：performance 迭代次数，默认 `1000`
- `--duration-hours`：stability 最长时长，默认 `6.0`
- `--stability-max-cases`：stability 最大 case，默认 `3000`
- `--stability-sample-ms`：stability 采样窗口毫秒数，默认 `1.0`
- `--cycles on|off`：是否尝试输出周期计数，默认 `on`
- `--json-out`：写出 JSON 报告

## `--kat`

仅在 correctness 模式下有效。

支持的测试目标：
- `hash`
- `sig`
- `kem`
- `kex`

说明：
- 无可用向量时回退到随机 correctness 检测
- KAT 支持 `#` / `;` / `//` 注释、`0x` 前缀、空白/`:`/`_` 分隔

常用字段：
- Hash: `INPUT`/`MSG` + `OUTPUT`/`DIGEST`
- SIG verify: `PK` + `MSG`/`INPUT` + `SN`/`SIG`/`OUTPUT`
- KEM: `SK` + `CT` + `SS`/`OUTPUT`
- KEX-A: `SKA` + `PKB` + `M2` + `STA` + `SSA`
- KEX-B: `SKB` + `PKA` + `M3` + `STB` + `SSB`

## 默认值

- `test = all`
- `mode = all`
- `iterations = 1000`
- `duration-hours = 6.0`
- `stability-max-cases = 3000`
- `stability-sample-ms = 1.0`
- `msg-len = 1024`
- `cycles = on`

## 输出示例

聚合 correctness：

```text
[sig][correctness] PASS
[kem][correctness] PASS
```

子操作 performance：

```text
[sig][keygen][performance][0B] ops=1000 warmup=10 elapsed_ms=0.321 bytes/op=0.000
[sig][keygen][performance][0B][throughput] ops/s=3115264.798 bytes/s=0.000
[sig][keygen][performance][0B][time] mean_ms=0.000321 median_ms=0.000300 stddev_ms=0.000040 cv=12.462%
```

stability：

```text
[kex][stability] STABLE cases=3000 elapsed_s=456.789
[kex][stability][throughput] mean=1456789.000 stddev=33506.000 cv=2.300% min=1398234.000 max=1512456.000
[kex][stability][errors] total=3000 failed=0 rate=0.000000% status=STABLE
```

memory：

```text
[memory][static] text=4096 data=512 bss=128 rodata=2048 total=6784
[memory][dynamic] heap_baseline=12328960 heap_after=15400960 heap_delta=3072000
```

## 返回码

- `0`：全部通过
- `1`：任一测试失败、参数错误、符号缺失，或 stability 为 `UNSTABLE`
