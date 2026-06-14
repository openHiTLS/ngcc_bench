# ngcc_bench

[Chinese](README-zh.md)

A cross-platform NGCC algorithm benchmarking tool written in C11 for Linux and
macOS. It loads the target dynamic library through `dlopen` / `dlsym` and runs
correctness, performance, memory, and stability tests.

## Test Targets

| Target | correctness | performance |
| --- | --- | --- |
| `hash` | Hash the same random message twice and compare the digests, or run KAT vectors | Report throughput and timing for each fixed message length |
| `sig` | Run `keygen + sign + verify`, or run verify KAT vectors | Report `keygen`, `sign`, and `verify` metrics separately |
| `kem` | Run `keygen + encap + decap`, or run decap KAT vectors | Report `keygen`, `encap`, and `decap` metrics separately |
| `kex` | Run the complete key-exchange flow, or run KAT vectors | Report `derive_ss_a` and `derive_ss_b` metrics separately |
| `all` | Run all four algorithm categories | Run all four algorithm categories |

Test modes:

- `correctness`
- `performance`
- `memory`
- `stability`
- `all`

## Build

Requirements:

- CMake >= 3.16
- GCC or Clang with C11 support
- On Linux, `libdl` and `libm`

```bash
mkdir build
cd build
cmake ..
make -j
```

Main executable:

```bash
./ngcc_bench
```

The target dynamic library does not need to be configured while building
`ngcc_bench`. Running `./ngcc_bench` directly starts interactive mode and
prompts for the library path. For scripted execution, pass the path with
`--lib PATH`. Both modes load the target library through `dlopen` / `dlsym`.

## Quick Start

Run without arguments to start interactive mode:

```bash
./ngcc_bench
```

The program prompts for the dynamic library path, test target, test mode, and
any additional configuration required by the selected mode.

Show help or version information:

```bash
./ngcc_bench --help
./ngcc_bench --version
```

The following non-interactive examples are suitable for scripts or CI.

Run KEM correctness:

```bash
./ngcc_bench \
  --lib /path/to/libalgo.so \
  --test kem \
  --mode correctness
```

Run SIG performance:

```bash
./ngcc_bench \
  --lib /path/to/libalgo.so \
  --test sig \
  --mode performance
```

Run all tests and write JSON reports in both Chinese and English:

```bash
./ngcc_bench \
  --lib /path/to/libalgo.so \
  --test all \
  --mode all \
  --digest-len-bits 256 \
  --json-out full_report.json
```

This command creates:

```text
full_report.json.zh
full_report.json.en
```

## CLI Options

| Option | Description | Default |
| --- | --- | --- |
| `--lib PATH` | Path to the target dynamic library; required only in non-interactive mode | Prompted in interactive mode |
| `--test hash\|sig\|kem\|kex\|all` | Test target | `all` |
| `--mode correctness\|performance\|memory\|stability\|all` | Test mode | `all` |
| `--digest-len-bits BITS` | Hash digest length in bits; required when `hash` is selected | None |
| `--duration-hours H` | Maximum stability-test duration; must be greater than `0` | `6.0` |
| `--stability-max-cases N` | Maximum number of stability-test cases; must be greater than `0` | `3000` |
| `--kat DIR` | KAT vector directory; only available when `correctness` is included | None |
| `--json-out PATH` | JSON output prefix; writes `PATH.zh` and `PATH.en` | None |
| `--help` | Show help | - |
| `--version` | Show version information | - |

The following settings cannot be changed through the CLI:

- Performance iterations: `10000`
- Hash performance message lengths: `32`, `128`, `512`, `1024`, `4096`,
  `8192`, `16384`, and `65536` bytes
- Stability-test message length: `131072` bytes
- Stability-test sampling window: `1.0` millisecond
- Stability tests attempt to collect cycles; unsupported platforms report
  `unavailable`
- Stability thresholds: throughput CV `< 5%`, timing CV `< 5%`, cycles CV
  `< 5%`, absolute memory growth `< 1%`, and error rate `<= 0%`

## KAT Directories

`--kat` accepts a directory, not a single file. Files are selected by prefix
for the requested test target:

| Target | Filename prefix |
| --- | --- |
| `hash` | `KAT_2_12_`, `KAT_2_23_`, `KAT_2_33_`, and `KAT_Loop_`; all four are required |
| `sig` | `KAT_SIG_` |
| `kem` | `KAT_KEM_` |
| `kex` | `KAT_KEX_` |

KAT parsing supports `#`, `;`, and `//` comments, the `0x` prefix, and common
field aliases.

## Output and Limitations

- Symbols are loaded on demand according to `--test`. A dynamic library only
  needs to export the symbols for the selected algorithms.
- `memory` runs each selected algorithm in an isolated helper process and
  reports `static_memory_bytes` and `peak_memory_bytes` in JSON output.
- Memory metrics use Linux `VmSize`/`VmPeak`; non-Linux platforms may not
  support this mode.
- Stability tests print `STABLE` or `UNSTABLE`, and print `STOPPED` when
  interrupted by `SIGINT` / `SIGTERM`.
- The program returns a non-zero exit code for `UNSTABLE` results, test
  failures, invalid arguments, or missing symbols.
- `--json-out` writes JSON reports in both Chinese and English.

## Dynamic Library Interface

The target dynamic library must export the interface symbols required by the
selected algorithms. See `ngcc_bench/include/ngcc_api.h` for the interface
definitions.
