# NGCC Benchmark – Final Implementation‑Ready Design

## 1. Executive Summary
This document finalizes the design for a Linux C benchmark tool that loads a user‑specified `.so` via `dlopen/dlsym` and tests **CryptHash**, **SIG**, **KEM**, and **KEX** APIs for **correctness**, **performance (cycles + ops/s)**, **memory (baseline + peak)**, and **stability** (6 hours or 3000 random cases). The design is modular, minimal, and ready to implement with CMake.

---

## 2. Review Response Summary

| Feedback | Decision | Rationale | Action |
|---|---|---|---|
| Complete KEX function signatures in vtable | **Adopt** | Required for correctness and compilation | Fully specify all KEX prototypes |
| Normalize memory units (KB vs bytes) | **Adopt** | Avoid misleading reporting | Convert to **bytes** consistently |
| Signal handling for stability tests | **Adopt** | Needed for graceful stop | Add SIGINT/SIGTERM handler |
| Use crypto‑grade RNG | **Adopt** | Random test inputs must be robust | Use `getrandom(2)` with `/dev/urandom` fallback |
| Clarify “static memory” interpretation | **Adopt (Partial)** | Full static allocation measurement needs instrumentation | Define as **baseline RSS after load/init** and document limitation |
| Clarify `sig_verify` return convention | **Adopt** | Ambiguity otherwise | Assume **0 = success**, non‑zero = failure; document |
| Add `--digest-len-bits` for hash | **Adopt** | Hash API requires it | CLI requires this when hash tests run |
| Fork‑per‑test for memory isolation | **Reject** | Over‑engineers core requirement | Document that peak is process‑wide |
| JSON output option | **Reject** | Not required | Keep plain text output |
| Verbose / progress bar / config file | **Reject** | Not requested | Skip for simplicity |
| Warmup parameter exposure | **Reject** | Not required | Use fixed warmup internally |

---

## 3. Technical Design

### 3.1 Architecture Overview
```
ngcc_bench
├─ Loader (dlopen/dlsym + vtable)
├─ Benchmark Core (timing, cycles, RNG, stats)
├─ Algorithm Tests (hash/sig/kem/kex)
├─ Memory Metrics (baseline + peak)
├─ Stability Runner (duration or count)
└─ CLI (dispatch + reporting)
```

### 3.2 Data Flow
1. Parse CLI → load `.so` → resolve symbols into vtable.
2. Allocate buffers based on `*_get_*_len_bytes` APIs.
3. Run selected **mode**:
   - correctness: functional round‑trip tests
   - performance: warmup + timed iterations
   - memory: capture baseline + run workload + peak
   - stability: repeated correctness until duration/count satisfied
4. Print results in clear text format.

---

## 4. Implementation Specification

### 4.1 File Layout
```
ngcc_bench/
├─ CMakeLists.txt
├─ include/
│  ├─ ngcc_api.h
│  ├─ loader.h
│  ├─ bench_core.h
│  ├─ mem_stat.h
│  ├─ bench_hash.h
│  ├─ bench_sig.h
│  ├─ bench_kem.h
│  ├─ bench_kex.h
│  └─ stability.h
└─ src/
   ├─ main.c
   ├─ loader.c
   ├─ bench_core.c
   ├─ mem_stat.c
   ├─ bench_hash.c
   ├─ bench_sig.c
   ├─ bench_kem.c
   ├─ bench_kex.c
   └─ stability.c
```

### 4.2 API Vtable (Complete)
```c
typedef struct {
    // Hash
    int (*CryptHash)(int digest_len_bits,
                     const unsigned char *msg,
                     unsigned long long msg_len_bits,
                     unsigned char *digest);

    // SIG
    unsigned long long (*sig_get_pk_len_bytes)(void);
    unsigned long long (*sig_get_sk_len_bytes)(void);
    unsigned long long (*sig_get_sn_len_bytes)(void);
    int (*sig_keygen)(unsigned char *pk, unsigned long long *pk_len_bytes,
                      unsigned char *sk, unsigned long long *sk_len_bytes);
    int (*sig_sign)(unsigned char *sk, unsigned long long sk_len_bytes,
                    unsigned char *m, unsigned long long m_len_bytes,
                    unsigned char *sn, unsigned long long *sn_len_bytes);
    int (*sig_verify)(unsigned char *pk, unsigned long long pk_len_bytes,
                      unsigned char *sn, unsigned long long sn_len_bytes,
                      unsigned char *m, unsigned long long m_len_bytes);

    // KEM
    unsigned long long (*kem_get_pk_len_bytes)(void);
    unsigned long long (*kem_get_sk_len_bytes)(void);
    unsigned long long (*kem_get_ss_len_bytes)(void);
    unsigned long long (*kem_get_ct_len_bytes)(void);
    int (*kem_keygen)(unsigned char *pk, unsigned long long *pk_len_bytes,
                      unsigned char *sk, unsigned long long *sk_len_bytes);
    int (*kem_enc)(unsigned char *pk, unsigned long long pk_len_bytes,
                   unsigned char *ss, unsigned long long *ss_len_bytes,
                   unsigned char *ct, unsigned long long *ct_len_bytes);
    int (*kem_dec)(unsigned char *sk, unsigned long long sk_len_bytes,
                   unsigned char *ct, unsigned long long ct_len_bytes,
                   unsigned char *ss, unsigned long long *ss_len_bytes);

    // KEX
    unsigned long long (*kex_get_passes_num)(void);
    unsigned long long (*kex_get_pk_len_bytes)(void);
    unsigned long long (*kex_get_sk_len_bytes)(void);
    unsigned long long (*kex_get_sta_len_bytes)(void);
    unsigned long long (*kex_get_stb_len_bytes)(void);
    unsigned long long (*kex_get_ss_len_bytes)(void);
    unsigned long long (*kex_get_total_msg_len_bytes)(void);

    int (*kex_init_a)(unsigned char *pka, unsigned long long *pka_len_bytes,
                      unsigned char *ska, unsigned long long *ska_len_bytes,
                      unsigned char *sta, unsigned long long *sta_len_bytes);
    int (*kex_init_b)(unsigned char *pkb, unsigned long long *pkb_len_bytes,
                      unsigned char *skb, unsigned long long *skb_len_bytes,
                      unsigned char *stb, unsigned long long *stb_len_bytes);

    int (*kex_generate_pass1_msg_a)(unsigned char *ska, unsigned long long ska_len_bytes,
                                    unsigned char *pkb, unsigned long long pkb_len_bytes,
                                    unsigned char *sta, unsigned long long *sta_len_bytes,
                                    unsigned char *m1, unsigned long long *m1_len_bytes);

    int (*kex_generate_pass2_msg_b)(unsigned char *skb, unsigned long long skb_len_bytes,
                                    unsigned char *pka, unsigned long long pka_len_bytes,
                                    unsigned char *m1, unsigned long long m1_len_bytes,
                                    unsigned char *stb, unsigned long long *stb_len_bytes,
                                    unsigned char *m2, unsigned long long *m2_len_bytes);

    int (*kex_generate_pass3_msg_a)(unsigned char *ska, unsigned long long ska_len_bytes,
                                    unsigned char *pkb, unsigned long long pkb_len_bytes,
                                    unsigned char *m2, unsigned long long m2_len_bytes,
                                    unsigned char *sta, unsigned long long *sta_len_bytes,
                                    unsigned char *m3, unsigned long long *m3_len_bytes);

    int (*kex_derive_ss_a)(unsigned char *ska, unsigned long long ska_len_bytes,
                           unsigned char *pkb, unsigned long long pkb_len_bytes,
                           unsigned char *mb, unsigned long long mb_len_bytes,
                           unsigned char *sta, unsigned long long sta_len_bytes,
                           unsigned char *ssa, unsigned long long *ssa_len_bytes);

    int (*kex_derive_ss_b)(unsigned char *skb, unsigned long long skb_len_bytes,
                           unsigned char *pka, unsigned long long pka_len_bytes,
                           unsigned char *ma, unsigned long long ma_len_bytes,
                           unsigned char *stb, unsigned long long stb_len_bytes,
                           unsigned char *ssb, unsigned long long *ssb_len_bytes);
} ngcc_api_t;
```

### 4.3 Loader (`loader.c`)
- `dlopen(lib_path, RTLD_NOW)`; `dlsym` all required symbols.
- Fail fast on any missing symbol (print which one).
- Store function pointers in `ngcc_api_t`.

### 4.4 Benchmark Core (`bench_core.c`)
- **Timing**: `clock_gettime(CLOCK_MONOTONIC_RAW)`
- **Cycles**:
  - Try `perf_event_open` for `PERF_COUNT_HW_CPU_CYCLES`.
  - If unavailable, on `x86_64` use `rdtsc`.
  - If neither works, cycles are “unavailable” but time/ops still reported.
- **Warmup**: fixed at 10 iterations or 1% of total, whichever is larger.
- **RNG**:
  - Use `getrandom(2)`; fallback to `/dev/urandom`.
  - Fill buffers with random bytes.

### 4.5 Algorithm Modules

#### Hash
- Requires `--digest-len-bits` (integer > 0).
- Input: random `msg_len` bytes (default 1024 or provided `--msg-len`).
- Test: call `CryptHash` twice → output digests identical.
- Digest buffer size: `(digest_len_bits + 7) / 8`.

#### SIG
- `sig_keygen` → `sig_sign` → `sig_verify`.
- **Assumption**: return `0` means success; non‑zero is failure (documented).

#### KEM
- `kem_keygen` → `kem_enc` → `kem_dec`.
- Compare shared secret bytes for equality.

#### KEX
- Simulate Alice and Bob:
  1. `kex_init_a`, `kex_init_b`
  2. `pass1` (A→B), `pass2` (B→A), `pass3` (A→B)
  3. `kex_derive_ss_a` and `kex_derive_ss_b`
- Compare derived shared secrets.

### 4.6 Memory Metrics (`mem_stat.c`)
- **Baseline (static memory)**: RSS after library load and initial allocations.
- **Peak**: `getrusage(RUSAGE_SELF).ru_maxrss`.
- **Units**: convert all results to **bytes** for consistency.
- Note: peak is process‑wide; for per‑algorithm accuracy, run tests individually.

### 4.7 Stability Runner (`stability.c`)
- Two modes:
  - **Duration**: run until elapsed seconds ≥ specified hours.
  - **Count**: run N random cases (default 3000).
- Sampling mode:
  - Aggregate multiple correctness runs into one stats sample window (`--stability-sample-ms`, default 1.0ms).
  - Reduces timer quantization noise for very fast algorithms.
- Uses correctness checks for selected test.
- Handles SIGINT/SIGTERM gracefully (stop loop and report partial progress).

### 4.8 CLI Specification
```
ngcc_bench --lib /path/to/lib.so
               --test hash|sig|kem|kex|all
               --mode correctness|performance|memory|stability|all
               [--iterations N]
               [--duration-hours H]
               [--stability-max-cases N]
               [--stability-sample-ms MS]
               [--msg-len BYTES]
               [--digest-len-bits BITS]   (required if hash included)
               [--cycles on|off]
               [--stable-throughput-cv-percent P]
               [--stable-cycles-cv-percent P]
               [--stable-time-cv-percent P]
               [--stable-memory-growth-percent P]
               [--stable-error-rate-percent P]
               [--warning-throughput-cv-percent P]
               [--warning-cycles-cv-percent P]
               [--warning-time-cv-percent P]
               [--warning-memory-growth-percent P]
               [--warning-error-rate-percent P]
               [--json-out PATH]
               [--kat FILE]
```

**Defaults**:
- `--iterations`: 1000
- `--duration-hours`: 6 (if stability by time)
- `--stability-max-cases`: 3000
- `--stability-sample-ms`: 1.0
- `--msg-len`: 1024
- `--cycles`: on (auto‑fallback if unavailable)
- stable thresholds (%): throughput/cycles/time=5, memory=1, error=0
- warning thresholds (%): throughput/cycles/time=10, memory=5, error=1

**Errors**:
- Missing `--lib`: fatal.
- Hash test without `--digest-len-bits`: fatal.
- Warning thresholds smaller than stable thresholds: fatal.
- Any missing symbol: fatal with name.

### 4.9 Output Format (Plain Text)
Example (single test):
```
[hash][correctness] PASS
[hash][correctness] PASS total=128 passed=128 failed=0 source=kat
[hash][performance] ops=1000 warmup=10 elapsed_ms=12.3 ops/s=81234
[hash][performance][time] min_ms=0.010 mean_ms=0.012 median_ms=0.012 max_ms=0.018 stddev_ms=0.001 cv=5.2%
[hash][performance][cycles] min=390 mean=402 median=401 max=455 stddev=12 cv=3.0%
[memory] baseline_bytes=12328960 peak_bytes=15400960
```
If cycles unavailable: omit `cycles/op` and print a one‑time warning.

JSON report can be emitted with `--json-out /path/to/report.json` (`schema_version=3`, includes `options.stability_thresholds`).
KAT vectors can be provided with `--kat /path/to/vectors.kat` for correctness runs (hash/sig/kem/kex).

---

## 5. Testing Strategy

### 5.1 Correctness Tests
- Hash: deterministic repeat digest
- SIG: keygen → sign → verify == success
- KEM: ss_enc == ss_dec
- KEX: ss_a == ss_b

### 5.2 Performance Tests
- Warmup then timed iterations.
- Report elapsed time, ops/s, cycles/op (if available).

### 5.3 Memory Tests
- Capture baseline RSS after load/init.
- Run chosen workload; report peak RSS.

### 5.4 Stability Tests
- Run correctness in loop until:
  - **6 hours** elapsed, or
  - **3000** random cases
- Stop on first failure or signal.

---

## 6. Build Instructions

### 6.1 Build
```
mkdir -p build
cd build
cmake ..
make
```

### 6.2 Run
```
./ngcc_bench --lib /path/to/lib.so \
  --test all --mode all \
  --digest-len-bits 256
```

---

## Notes / Assumptions
- `sig_verify` success is **0** (common convention).
- “Static memory” is interpreted as **baseline RSS** after load/init.
- Peak memory is process‑wide; test algorithms individually for isolated peaks.
- `perf_event_open` may require permissions; tool will fallback automatically.

---

This design is coherent, minimal, and implementable without further clarification. If you want, I can proceed to implement it in the repository.
