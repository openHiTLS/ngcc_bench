Here is the review of the **Integrated Design Document: NGCC Benchmark**.

### 1. Requirement Alignment
*   **Completeness**: The design excellently captures all specific function prototypes provided in the requirement (Hash, SIG, KEM, KEX) and maps them to a cohesive vtable structure.
*   **Dynamic Loading**: The use of `dlopen`/`dlsym` is explicitly handled, addressing the core constraint of testing an external `.so` file.
*   **Metrics**: The design explicitly covers all requested metrics: Correctness (logic verification), Performance (cycles, ops/s), Memory (static/peak), and Stability (6h/3000 counts).
*   **Environment**: CMake and Linux targets are correctly identified.

**Verdict**: The design is fully aligned with the user requirements with no visible gaps.

### 2. Technical Soundness
*   **Architecture**: The modular separation (Loader vs. Core vs. Algos) is the correct approach. It ensures that the benchmarking logic is decoupled from the specific library loading mechanism.
*   **Vtable Approach**: Mapping `dlsym` lookups to a `struct` of function pointers (`ngcc_api_t`) is best practice. It prevents global namespace pollution and allows for fail-fast validation if symbols are missing.
*   **Timing**: `CLOCK_MONOTONIC_RAW` is the correct clock for measuring intervals on Linux, avoiding issues with NTP skew.
*   **Cycle Counting**: The preference for `perf_event_open` is technically superior for accuracy but complex to implement (requires syscalls and often root/capability privileges). The fallback to `rdtsc` is a necessary safety net.
*   **Memory Measurement**: Using `/proc/self/status` (for VmRSS) and `getrusage` (for maxrss) is the standard Linux way to measure process memory.

### 3. Implementation Feasibility
*   **Clarity**: The file layout and implementation steps are logical and follow a standard C project structure.
*   **KEX Logic**: The Key Exchange (KEX) workflow is non-trivial (3-pass exchange). The design correctly identifies the need to simulate two parties (Alice and Bob) to derive and compare the shared secret (`ss`), which is critical for the "Correctness" test.
*   **Stability**: The two modes (Time-based vs. Count-based) are straightforward to implement using a `while` loop with a check against `time()` or a counter.

### 4. Suggestions for Improvement

*   **1. Hash Parameter Configuration (Functional)**
    *   *Issue*: The `CryptHash` function takes `digest_len_bits`. The current CLI design allows setting message length (`--msg-len`) but not the output digest length.
    *   *Suggestion*: Add a `--digest-len` argument to the CLI. Different algorithms (SM3, SHA256, SHA512) require specific bit lengths to function correctly.

*   **2. Memory Metric Isolation (Accuracy)**
    *   *Issue*: `getrusage` (`ru_maxrss`) reports the high-water mark for the *entire lifetime* of the process. If a user runs `./ngcc_benchmark --test all`, the memory peak of the most expensive algorithm (likely KEX) will mask the others.
    *   *Suggestion*: When `--test all` is used, the memory report might be misleading. Recommend implementing a "fork-per-test" mode or clearly documenting that for accurate memory profiling, algorithms should be tested individually (e.g., `./ngcc_benchmark --test sig --mode memory`).

*   **3. Cycle Counting Privileges (Robustness)**
    *   *Issue*: `perf_event_open` often fails in containerized environments (Docker) or standard user shells due to `paranoid` kernel settings.
    *   *Suggestion*: Ensure the fallback logic is robust. If `perf_event_open` fails, log a warning once and silently switch to `rdtsc` (or pure wall-time if `rdtsc` is deemed too unstable on the specific architecture).

*   **4. Output Format (Usability)**
    *   *Suggestion*: While text output is fine, adding a simple `--json` flag would make this tool significantly easier to integrate into CI/CD pipelines or automated reporting dashboards later.

### 5. Overall Assessment
**Rating: 5 / 5 Stars**

**Summary**:
This is a high-quality, professional design document. It addresses the prompt with precision, chooses appropriate system APIs for Linux C development, and proposes a maintainable code structure. The detailed API mapping in the Vtable section demonstrates a clear understanding of the integration points. It is ready for implementation immediately.
