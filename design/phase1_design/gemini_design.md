# Technical Design Document: NGCC Benchmark Tool

## 1. Requirement Understanding

The goal is to develop a C-based benchmarking utility for specific cryptographic algorithms (Hash, Signature, KEM, KEX). The target algorithms are provided as a dynamic shared object (`.so`) and must be loaded at runtime using `dlopen`.

**Key Features:**
*   **Dynamic Loading:** Load symbols from a user-specified `.so` file.
*   **Algorithms:** Support CryptHash, SIG (Signature), KEM (Key Encapsulation), and KEX (Key Exchange).
*   **Metrics:**
    *   **Correctness:** Functional verification.
    *   **Performance:** CPU clock cycles.
    *   **Throughput:** Operations per second (ops/s).
    *   **Memory:** Static memory footprint and Peak memory usage.
*   **Stability:** Support long-running tests (e.g., 6 hours or 3000 random iterations).
*   **Build System:** CMake.

## 2. Technical Solution

### 2.1 Architecture

The application will follow a modular architecture:
1.  **Core/Loader:** Handles `dlopen`, resolves symbols to function pointers, and manages error handling if symbols are missing.
2.  **Benchmark Engine:** The main driver that parses arguments (algorithm type, mode, iterations/duration) and orchestrates execution.
3.  **Algorithm Modules:** Separate handlers for Hash, SIG, KEM, and KEX that implement the specific logic for correctness and performance testing.
4.  **Telemetry:** Utilities for measuring CPU cycles (RDTSC/CNTVCT), Wall clock time, and Memory usage (Stack painting + `getrusage`).

### 2.2 Memory Measurement Strategy
*   **Peak Memory:** We will use `getrusage(RUSAGE_SELF, ...)` to track `ru_maxrss`. For finer granularity (if the library allocates internally), external tools like Valgrind `massif` are usually preferred, but within the app, `getrusage` is the standard POSIX approach.
*   **Static/Stack Memory:** To measure stack usage of the specific function calls, we will use the "Stack Painting" technique: fill the stack with a known canary value (e.g., `0xCC`) before the call, and scan for the high-water mark after the call.

### 2.3 Stability Testing
*   **Duration Mode:** Run a loop checking `clock_gettime` until the target duration (e.g., 6 hours) is reached.
*   **Count Mode:** Run a fixed number of iterations (e.g., 3000) with random inputs.

## 3. Implementation Plan

### 3.1 File Structure
```text
ngcc_benchmark/
├── CMakeLists.txt
├── src/
│   ├── main.c              # Entry point & Arg parsing
│   ├── loader.c            # dlopen & dlsym implementation
│   ├── loader.h            # API function pointer definitions
│   ├── bench_engine.c      # Generic benchmark loop & reporting
│   ├── bench_engine.h
│   ├── algo/
│   │   ├── bench_hash.c    # Hash specific tests
│   │   ├── bench_sig.c     # Signature specific tests
│   │   ├── bench_kem.c     # KEM specific tests
│   │   └── bench_kex.c     # KEX specific tests
│   └── utils/
│       ├── cycles.h        # CPU cycle counting
│       ├── memory.c        # Stack & Heap measurement
│       ├── memory.h
│       └── random.c        # Random data generation
└── include/
    └── api_types.h         # Common typedefs
```

### 3.2 Key Data Structures

**API Context (in `loader.h`):**
```c
typedef struct {
    void *handle;
    // Hash
    int (*CryptHash)(int, const unsigned char*, unsigned long long, unsigned char*);
    // SIG
    unsigned long long (*sig_get_pk_len_bytes)();
    int (*sig_keygen)(unsigned char*, unsigned long long*, unsigned char*, unsigned long long*);
    // ... all other function pointers
} LibContext;
```

**Benchmark Config:**
```c
typedef struct {
    char lib_path[256];
    TestMode mode; // FUNCTIONAL, BENCHMARK, STABILITY
    int duration_sec;
    int iterations;
} BenchConfig;
```

### 3.3 API Mapping
We will define strict function pointer types matching the provided requirement in `api_types.h` to ensure type safety when casting `dlsym` results.

## 4. Testing Strategy

### 4.1 Functional Verification
*   **Self-Consistency:**
    *   **SIG:** Generate Key -> Sign -> Verify (Expect Success). Sign -> Tamper Message -> Verify (Expect Fail).
    *   **KEM:** Keygen -> Encapsulate -> Decapsulate (Expect Shared Secret Match).
    *   **KEX:** Init A/B -> Exchange Passes -> Derive Secret (Expect A and B derive same Secret).
    *   **Hash:** Hash same input twice (Expect same Digest).

### 4.2 Performance Verification
*   **Cycles:** Capture start/end CPU cycles around the core function call. Average over N iterations.
*   **Throughput:** Measure total wall time for N iterations. `N / TotalTime`.

### 4.3 Stability Verification
*   Run the "Self-Consistency" checks in a loop for 6 hours.
*   Report any failures immediately.
*   Periodically log memory usage to detect leaks (rss growth).

## 5. Build System (CMake)

```cmake
cmake_minimum_required(VERSION 3.10)
project(ngcc_benchmark C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -Wall -Wextra")

# Link dl library for dlopen
find_library(DL_LIBRARY dl)

include_directories(include src)

file(GLOB_RECURSE SOURCES "src/*.c")

add_executable(ngcc_bench ${SOURCES})
target_link_libraries(ngcc_bench ${DL_LIBRARY} m)
```
