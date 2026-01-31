# Technical Design Document: NGCC Cryptographic Benchmark Framework

## 1. Requirement Understanding

### 1.1 Objective
Develop a C-based benchmark program to evaluate cryptographic libraries implementing NIST Gemstar Cipher Competition (NGCC) algorithms.

### 1.2 Target APIs
The benchmark needs to test four categories of cryptographic primitives:

| Category | APIs | Purpose |
|----------|------|---------|
| **Hash** | `CryptHash` | Digest computation |
| **Signature (SIG)** | Keygen, Sign, Verify | Digital signatures |
| **KEM** | Keygen, Encaps, Decaps | Key encapsulation |
| **KEX** | Init, 3-pass messages, Derive | Key exchange |

### 1.3 Requirements Summary

| Requirement | Description |
|-------------|-------------|
| **Dynamic Loading** | Use `dlopen` to load shared libraries (`.so`) |
| **Correctness** | Verify functional correctness |
| **Performance** | Clock cycles, throughput (ops/s) |
| **Memory** | Static and peak memory usage |
| **Stability** | 6-hour run OR 3000 random test cases |
| **Build System** | CMake-based |

---

## 2. Technical Solution

### 2.1 Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     NGCC Benchmark                          │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐         │
│  │  Hash Test  │  │   SIG Test  │  │   KEM Test  │         │
│  └─────────────┘  └─────────────┘  └─────────────┘         │
│                                                               │
│  ┌─────────────┐                                            │
│  │   KEX Test  │                                            │
│  └─────────────┘                                            │
│                                                               │
├─────────────────────────────────────────────────────────────┤
│                   Benchmark Core                            │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐      │
│  │ Correct- │ │ Perform- │ │  Memory  │ │ Stabil-  │      │
│  │  ness    │ │  ance    │ │  Test    │ │  ity    │      │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘      │
├─────────────────────────────────────────────────────────────┤
│                   Dynamic Loader                            │
│  ┌──────────────────────────────────────────────────┐      │
│  │  dlopen/dlsym wrapper for API symbols            │      │
│  └──────────────────────────────────────────────────┘      │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 Project Structure

```
ngcc_benchmark/
├── CMakeLists.txt
├── include/
│   ├── common.h           # Common types and utilities
│   ├── loader.h           # Dynamic loading wrapper
│   ├── bench_hash.h       # Hash benchmark interface
│   ├── bench_sig.h        # Signature benchmark interface
│   ├── bench_kem.h        # KEM benchmark interface
│   └── bench_kex.h        # KEX benchmark interface
├── src/
│   ├── loader.c           # dlopen/dlsym implementation
│   ├── bench_hash.c
│   ├── bench_sig.c
│   ├── bench_kem.c
│   ├── bench_kex.c
│   ├── correctness.c      # Correctness test suite
│   ├── performance.c      # Performance measurement (cycles, ops/s)
│   ├── memory.c           # Memory usage measurement
│   ├── stability.c        # Long-running stability tests
│   └── main.c             # CLI entry point
├── tests/
│   ├── test_vectors/      # Known answer test data
│   └── mock_lib/          # Mock library for testing
└── README.md
```

---

## 3. Implementation Plan

### 3.1 Phase 1: Core Infrastructure

**Files to create:**

| File | Purpose |
|------|---------|
| `CMakeLists.txt` | Build configuration |
| `include/common.h` | Data structures, error codes |
| `include/loader.h` / `src/loader.c` | Dynamic symbol loading |

**Key data structures:**

```c
// API function pointers for each primitive
typedef struct {
    // Hash
    int (*crypt_hash)(int, const unsigned char*, 
                      unsigned long long, unsigned char*);
    
    // SIG
    unsigned long long (*sig_get_pk_len)(void);
    unsigned long long (*sig_get_sk_len)(void);
    unsigned long long (*sig_get_sn_len)(void);
    int (*sig_keygen)(unsigned char*, unsigned long long*,
                      unsigned char*, unsigned long long*);
    int (*sig_sign)(unsigned char*, unsigned long long,
                    unsigned char*, unsigned long long,
                    unsigned char*, unsigned long long*);
    int (*sig_verify)(unsigned char*, unsigned long long,
                      unsigned char*, unsigned long long,
                      unsigned char*, unsigned long long);
    
    // KEM
    unsigned long long (*kem_get_pk_len)(void);
    unsigned long long (*kem_get_sk_len)(void);
    unsigned long long (*kem_get_ss_len)(void);
    unsigned long long (*kem_get_ct_len)(void);
    int (*kem_keygen)(unsigned char*, unsigned long long*,
                      unsigned char*, unsigned long long*);
    int (*kem_enc)(unsigned char*, unsigned long long,
                   unsigned char*, unsigned long long*,
                   unsigned char*, unsigned long long*);
    int (*kem_dec)(unsigned char*, unsigned long long,
                   unsigned char*, unsigned long long,
                   unsigned char*, unsigned long long*);
    
    // KEX
    unsigned long long (*kex_get_passes_num)(void);
    unsigned long long (*kex_get_pk_len)(void);
    unsigned long long (*kex_get_sk_len)(void);
    unsigned long long (*kex_get_sta_len)(void);
    unsigned long long (*kex_get_stb_len)(void);
    unsigned long long (*kex_get_ss_len)(void);
    unsigned long long (*kex_get_total_msg_len)(void);
    int (*kex_init_a)(unsigned char*, unsigned long long*,
                      unsigned char*, unsigned long long*,
                      unsigned char*, unsigned long long*);
    int (*kex_init_b)(unsigned char*, unsigned long long*,
                      unsigned char*, unsigned long long*,
                      unsigned char*, unsigned long long*);
    int (*kex_generate_pass1_msg_a)(...);
    int (*kex_generate_pass2_msg_b)(...);
    int (*kex_generate_pass3_msg_a)(...);
    int (*kex_derive_ss_a)(...);
    int (*kex_derive_ss_b)(...);
} ngcc_api_t;

// Library handle
typedef struct {
    void *handle;
    ngcc_api_t api;
} ngcc_lib_t;
```

**Loader functions:**

```c
int ngcc_load(const char *so_path, ngcc_lib_t *lib);
void ngcc_unload(ngcc_lib_t *lib);
int ngcc_check_symbols(const ngcc_lib_t *lib);
```

### 3.2 Phase 2: Correctness Testing

**File:** `src/correctness.c`

| Test | Description |
|------|-------------|
| Known Answer Tests | Use test vectors for expected outputs |
| Round-trip Tests | Sign→Verify, Encaps→Decaps, KEX both sides |
| Edge Cases | Empty input, max length inputs |

```c
typedef struct {
    int (*test_func)(const ngcc_lib_t *lib);
    const char *name;
} correctness_test_t;

int run_correctness_suite(const ngcc_lib_t *lib);
```

### 3.3 Phase 3: Performance Measurement

**File:** `src/performance.c`

**Metrics:**
- **Clock cycles:** Using `rdtsc` or `clock_gettime()`
- **Throughput:** Operations per second
- **Latency:** Per-operation timing

```c
typedef struct {
    double cycles_min;
    double cycles_avg;
    double cycles_max;
    double ops_per_sec;
} perf_result_t;

typedef enum {
    PERF_MODE_CYCLES,
    PERF_MODE_OPS,
    PERF_MODE_BOTH
} perf_mode_t;

// Warm-up to avoid cold start effects
int bench_warmup(const ngcc_lib_t *lib, int iterations);

// Measure single operation
perf_result_t bench_measure_cycles(int (*op)(void), int iterations);

// Measure throughput
perf_result_t bench_measure_throughput(int (*op)(void), double duration_sec);
```

### 3.4 Phase 4: Memory Measurement

**File:** `src/memory.c`

**Metrics:**
- **Static memory:** Code + data size from `/proc/self/maps` or `size` command
- **Peak memory:** Using `mallinfo2()` or `/proc/self/status`

```c
typedef struct {
    size_t static_memory;    // Code + static data
    size_t peak_memory;      // Runtime peak
    size_t heap_usage;       // Current heap
} memory_result_t;

memory_result_t bench_measure_memory(const char *so_path);
```

### 3.5 Phase 5: Stability Testing

**File:** `src/stability.c`

| Mode | Description |
|------|-------------|
| Time-based | Run for 6 hours continuously |
| Count-based | Run 3000 random test iterations |

```c
typedef struct {
    enum {
        STABILITY_MODE_TIME,
        STABILITY_MODE_COUNT
    } mode;
    union {
        unsigned int duration_hours;
        unsigned int iteration_count;
    };
} stability_config_t;

int run_stability_test(const ngcc_lib_t *lib, const stability_config_t *config);
```

### 3.6 Phase 6: Command-Line Interface

**File:** `src/main.c`

```c
// Usage:
// ngcc_benchmark --lib <path.so> --test <hash|sig|kem|kex|all> \
//                 --mode <correctness|performance|memory|stability|all> \
//                 [--iterations N] [--duration HOURS]
```

---

## 4. Testing Strategy

### 4.1 Unit Testing

| Component | Test Approach |
|-----------|---------------|
| Loader | Load invalid .so, missing symbols, successful load |
| Correctness | Mock library with known outputs |
| Performance | Verify timing logic, warm-up effects |

### 4.2 Integration Testing

1. **Mock Library:** Create a simple `.so` that implements all APIs with predictable behavior
2. **Real Libraries:** Test against actual NGCC submissions when available

### 4.3 Validation Checklist

| Category | Validation Method |
|----------|-------------------|
| Correctness | Compare against test vectors |
| Clock Cycles | Compare `rdtsc` vs `clock_gettime` |
| Memory | Cross-check with Valgrind massif |
| Stability | Verify no memory leaks (Valgrind) |

### 4.4 Test Data

- Generate random inputs for KEM, SIG, KEX
- Use NIST-provided test vectors when available
- Include edge cases (0-byte, min/max sizes)

---

## 5. Build Configuration

### 5.1 CMakeLists.txt Structure

```cmake
cmake_minimum_required(VERSION 3.15)
project(ngcc_benchmark C)

set(CMAKE_C_STANDARD 11)

# Find required libraries
find_package(Threads REQUIRED)

# Main executable
add_executable(ngcc_benchmark
    src/main.c
    src/loader.c
    src/bench_hash.c
    src/bench_sig.c
    src/bench_kem.c
    src/bench_kex.c
    src/correctness.c
    src/performance.c
    src/memory.c
    src/stability.c
)

target_include_directories(ngcc_benchmark PRIVATE include)
target_link_libraries(ngcc_benchmark PRIVATE Threads::Threads dl)

# Optional: Enable testing
enable_testing()
add_subdirectory(tests)
```

---

## 6. Summary

This design provides a modular, extensible benchmark framework for NGCC cryptographic algorithms. Key features:

- **Dynamic loading** via `dlopen` for flexible library testing
- **Four benchmark modes:** correctness, performance, memory, stability
- **Standardized metrics** for fair comparison across submissions
- **CMake-based** for easy integration and cross-platform support
