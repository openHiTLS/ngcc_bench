**Integrated Design Document: NGCC Benchmark (C/CMake)**

## 1. Requirement Summary
- Build a C benchmark program to test **CryptHash**, **SIG**, **KEM**, **KEX** APIs from a **user-specified `.so`** via `dlopen/dlsym`.
- Must test **correctness**, **performance (cycles + ops/s)**, **memory (static + peak)**, and **stability** (6 hours or 3000 random cases).
- Use **CMake** for build.
- Must run on Linux and accept library path at runtime.

---

## 2. Technical Design

### 2.1 Architecture
A simple modular CLI tool:
```
ngcc_benchmark
├─ Loader (dlopen/dlsym)
├─ Benchmark Core (timing, stats, RNG)
├─ Algorithm Modules (hash/sig/kem/kex)
├─ Memory & Stability Modules
└─ CLI (dispatch + reporting)
```

### 2.2 Components & Interfaces

#### A) Dynamic Loader (required)
- Loads `.so` with `dlopen`.
- Resolves required symbols into a single vtable struct.
- Fails fast if any required symbol missing.

**Vtable example (typed function pointers):**
```c
typedef struct {
    // Hash
    int (*CryptHash)(int, const unsigned char*, unsigned long long, unsigned char*);

    // SIG
    unsigned long long (*sig_get_pk_len_bytes)(void);
    unsigned long long (*sig_get_sk_len_bytes)(void);
    unsigned long long (*sig_get_sn_len_bytes)(void);
    int (*sig_keygen)(unsigned char*, unsigned long long*, unsigned char*, unsigned long long*);
    int (*sig_sign)(unsigned char*, unsigned long long, unsigned char*, unsigned long long,
                    unsigned char*, unsigned long long*);
    int (*sig_verify)(unsigned char*, unsigned long long, unsigned char*, unsigned long long,
                      unsigned char*, unsigned long long);

    // KEM
    unsigned long long (*kem_get_pk_len_bytes)(void);
    unsigned long long (*kem_get_sk_len_bytes)(void);
    unsigned long long (*kem_get_ss_len_bytes)(void);
    unsigned long long (*kem_get_ct_len_bytes)(void);
    int (*kem_keygen)(unsigned char*, unsigned long long*, unsigned char*, unsigned long long*);
    int (*kem_enc)(unsigned char*, unsigned long long, unsigned char*, unsigned long long*,
                   unsigned char*, unsigned long long*);
    int (*kem_dec)(unsigned char*, unsigned long long, unsigned char*, unsigned long long,
                   unsigned char*, unsigned long long*);

    // KEX
    unsigned long long (*kex_get_passes_num)(void);
    unsigned long long (*kex_get_pk_len_bytes)(void);
    unsigned long long (*kex_get_sk_len_bytes)(void);
    unsigned long long (*kex_get_sta_len_bytes)(void);
    unsigned long long (*kex_get_stb_len_bytes)(void);
    unsigned long long (*kex_get_ss_len_bytes)(void);
    unsigned long long (*kex_get_total_msg_len_bytes)(void);
    int (*kex_init_a)(unsigned char*, unsigned long long*, unsigned char*, unsigned long long*,
                      unsigned char*, unsigned long long*);
    int (*kex_init_b)(unsigned char*, unsigned long long*, unsigned char*, unsigned long long*,
                      unsigned char*, unsigned long long*);
    int (*kex_generate_pass1_msg_a)(...);
    int (*kex_generate_pass2_msg_b)(...);
    int (*kex_generate_pass3_msg_a)(...);
    int (*kex_derive_ss_a)(...);
    int (*kex_derive_ss_b)(...);
} ngcc_api_t;
```

#### B) Benchmark Core
- Timing: `clock_gettime(CLOCK_MONOTONIC_RAW)` for duration.
- Cycles: **prefer `perf_event_open`** for CPU cycles; **fallback to `rdtsc`** on x86_64 if perf unavailable.
- Throughput: `ops/s = iterations / elapsed_sec`.
- Random input generator for correctness/stability.

#### C) Algorithm Modules
Each module supports **correctness + performance** paths:

- **Hash:** same input → same digest; check digest length/return code.
- **SIG:** keygen → sign → verify (must succeed); tamper test optional (must fail).
- **KEM:** keygen → enc → dec → compare shared secret.
- **KEX:** run full 3-pass exchange → derive both sides → compare shared secret.

#### D) Memory Metrics
- **Static/Baseline:** RSS after load + initialization (from `/proc/self/status`).
- **Peak:** VmHWM or `getrusage(RUSAGE_SELF).ru_maxrss`.

#### E) Stability
Two modes:
- **Duration-based:** loop until 6 hours (21600 seconds).
- **Count-based:** 3000 random test cases.

### 2.3 CLI
Example:
```
ngcc_benchmark --lib /path/to/lib.so \
  --test hash|sig|kem|kex|all \
  --mode correctness|performance|memory|stability|all \
  [--iterations N] [--duration hours] [--msg-len bytes] [--cycles on|off]
```

---

## 3. Implementation Plan

### 3.1 File Layout (simple + maintainable)
```
ngcc_benchmark/
├─ CMakeLists.txt
├─ include/
│  ├─ ngcc_api.h         # API vtable and typedefs
│  ├─ loader.h
│  ├─ bench_core.h       # timing, stats, rng
│  ├─ mem_stat.h
│  ├─ bench_hash.h
│  ├─ bench_sig.h
│  ├─ bench_kem.h
│  └─ bench_kex.h
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

### 3.2 Steps
1. **Create loader** with strict symbol checking.
2. **Implement core timing + cycles + RNG**.
3. **Implement correctness tests** per algorithm.
4. **Add performance loops** (iterations + ops/s + cycles).
5. **Add memory sampling** (baseline + peak).
6. **Add stability runner** (time/count modes).
7. **Wire CLI** and output formatting.
8. **CMake** for build, `-ldl` + `-pthread` if needed.

---

## 4. Testing Strategy

### 4.1 Correctness
- Hash: deterministic output for same input; return code OK.
- SIG: sign/verify success; optional tamper failure.
- KEM: decapsulated secret == encapsulated secret.
- KEX: A and B derive identical secret.

### 4.2 Performance
- Run warmup iterations.
- Measure:
  - elapsed time
  - ops/s
  - cycles (if available)

### 4.3 Memory
- Baseline RSS after load.
- Peak RSS during workload.

### 4.4 Stability
- Run correctness tests repeatedly:
  - **6 hours**, or
  - **3000 random cases**.
- Stop and report on first failure.

---

## 5. Decisions (Adopted vs Rejected)

### Adopted
- **Single vtable for all APIs** (CLAUDE/CODEX): simplifies loader + dispatch.
- **Correctness via round-trip tests** (all designs): aligns with API nature.
- **Perf metrics: time + cycles + ops/s** (CODEX/CLAUDE).
- **Memory: baseline + peak RSS** (CODEX/GEMINI).
- **Stability: time or count** (all designs).

### Rejected / Simplified
- **Overly deep test vector infrastructure**: not required in user spec; optional later.
- **Too many directories**: kept layout minimal for clarity.
- **Mandatory external tools (Valgrind/Massif)**: kept optional; core uses `/proc` + `getrusage`.
- **Complex reporting formats**: default to text; JSON optional later.

---

If you want, I can proceed to implement this design directly in the repository.
