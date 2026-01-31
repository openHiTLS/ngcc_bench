# Technical Design Review: NGCC Benchmark

## 1. Requirement Alignment

### Gaps Identified

| Requirement | Status | Notes |
|-------------|--------|-------|
| `dlopen` library at runtime | ✅ Covered | Loader module handles this |
| Test CryptHash/SIG/KEM/KEX APIs | ✅ Covered | All 4 APIs included in vtable |
| Correctness testing | ✅ Covered | Round-trip tests per algorithm |
| Performance (cycles + ops/s) | ✅ Covered | `perf_event_open` with RDTSC fallback |
| Memory (static + peak) | ⚠️ Partial | "Static memory" interpreted as baseline RSS - may not capture actual static allocation |
| Stability (6h OR 3000 cases) | ✅ Covered | Both modes supported |
| CMake build | ✅ Covered | CMakeLists.txt planned |
| Linux platform | ✅ Covered | Uses `/proc`, `perf_event_open`, `dlopen` |

### Clarification Needed - "Static Memory"

The design interprets "静态内存" (static memory) as **baseline RSS after load**. This may not be what the user intends:
- **If user means "static allocations"** (global/static variables in the library): baseline RSS is a reasonable approximation.
- **If user means "heap allocations before operations"**: this might differ from baseline RSS.

**Recommendation**: Document this interpretation clearly, or add a note that true static memory measurement would require library instrumentation.

---

## 2. Technical Soundness

### 2.1 Architecture

**Assessment**: Appropriate for the complexity. Single-purpose tool with clear separation of concerns.

### 2.2 Critical Issues

#### Issue 1: Missing Length Parameters in Vtable (MUST FIX)

The vtable typedef shows KEX functions with `...` instead of full signatures:

```c
int (*kex_generate_pass1_msg_a)(...);
int (*kex_generate_pass2_msg_b)(...);
int (*kex_generate_pass3_msg_a)(...);
int (*kex_derive_ss_a)(...);
int (*kex_derive_ss_b)(...);
```

This is incomplete. The actual signatures from requirements:

```c
int kex_generate_pass1_msg_a(unsigned char *ska, unsigned long long ska_len_bytes,
                              unsigned char *pkb, unsigned long long pkb_len_bytes,
                              unsigned char *sta, unsigned long long *sta_len_bytes,
                              unsigned char *m1, unsigned long long *m1_len_bytes);

int kex_generate_pass2_msg_b(unsigned char *skb, unsigned long long skb_len_bytes,
                              unsigned char *pka, unsigned long long pka_len_bytes,
                              unsigned char *m1, unsigned long long m1_len_bytes,
                              unsigned char *stb, unsigned long long *stb_len_bytes,
                              unsigned char *m2, unsigned long long *m2_len_bytes);

int kex_generate_pass3_msg_a(unsigned char *ska, unsigned long long ska_len_bytes,
                              unsigned char *pkb, unsigned long long pkb_len_bytes,
                              unsigned char *m2, unsigned long long m2_len_bytes,
                              unsigned char *sta, unsigned long long *sta_len_bytes,
                              unsigned char *m3, unsigned long long *m3_len_bytes);

int kex_derive_ss_a(unsigned char *ska, unsigned long long ska_len_bytes,
                    unsigned char *pkb, unsigned long long pkb_len_bytes,
                    unsigned char *mb, unsigned long long mb_len_bytes,
                    unsigned char *sta, unsigned long long sta_len_bytes,
                    unsigned char *ssa, unsigned long long *ssa_len_bytes);

int kex_derive_ss_b(unsigned char *skb, unsigned long long skb_len_bytes,
                    unsigned char *pka, unsigned long long pka_len_bytes,
                    unsigned char *ma, unsigned long long ma_len_bytes,
                    unsigned char *stb, unsigned long long stb_len_bytes,
                    unsigned char *ssb, unsigned long long *ssb_len_bytes);
```

#### Issue 2: Memory Measurement Unit Inconsistency (MUST FIX)

`getrusage(RUSAGE_SELF).ru_maxrss` returns:
- **Kilobytes** on Linux
- **Bytes** on some BSD variants

The design should normalize units. Also, `VmHWM` from `/proc/self/status` is in **KB**, while `/proc/self/status` field `VmPeak` is also in **KB**.

**Recommendation**: Explicitly convert to a consistent unit (KB) in the code and label output clearly.

#### Issue 3: SIG Verify Return Value Edge Case

The correctness test says "sign → verify (must succeed)" but doesn't handle the case where:
- `sig_verify()` returns 0 on success
- The specification says return type is `int` but doesn't specify success/failure values

**Recommendation**: Clarify assumption (0 = success, non-zero = failure) and add a note that this follows POSIX convention. If the library uses different conventions, this would need adjustment.

### 2.3 Minor Concerns

| Area | Concern | Severity |
|------|---------|----------|
| RDTSC fallback | May not work on all x86_64 CPUs (very old) or virtualized environments | Low |
| Random input quality | No specified RNG algorithm; `rand()` may be insufficient for crypto testing | Low |
| Signal handling | No mention of SIGINT/SIGTERM handling for stability tests | Medium |

---

## 3. Implementation Feasibility

### 3.1 Implementation Plan Clarity

The file layout is clear and modular. Steps are logical:
1. ✅ Loader → Core → Per-algorithm → Memory → Stability → CLI → CMake

### 3.2 Interface Well-Defined

The CLI interface is straightforward. Example usage is clear:
```bash
ngcc_benchmark --lib /path/to/lib.so \
  --test hash|sig|kem|kex|all \
  --mode correctness|performance|memory|stability|all \
  [--iterations N] [--duration hours] [--msg-len bytes] [--cycles on|off]
```

### 3.3 Missing Implementation Details

| Detail | Status | Recommendation |
|--------|--------|----------------|
| Error handling strategy | ⚠️ Vague | Define error codes and user-facing messages |
| Output format | ⚠️ Minimal | Specify table format or JSON structure |
| `--msg-len` handling for KEX | ⚠️ Unclear | KEX doesn't have a "message length" parameter; clarify |

---

## 4. Suggestions for Improvement

### Must Fix (Blockers)

1. **Complete the KEX function signatures** in the vtable
2. **Normalize memory units** and document KB vs bytes
3. **Add signal handling** for stability tests (graceful shutdown on Ctrl+C)
4. **Clarify RNG strategy** - use `/dev/urandom` or `getrandom(2)` for cryptographic-quality randomness

### Nice to Have

1. **Add `--verbose` flag** for debugging
2. **Add JSON output option** for programmatic consumption
3. **Document the KEX message flow** in code comments (3-pass exchange is complex)
4. **Add warmup count parameter** (currently hardcoded)
5. **Consider adding `--help` and `--version`**

### Low Priority

1. **Colored output** for pass/fail indicators
2. **Progress bar** for long-running stability tests
3. **Config file support** for repeated testing

---

## 5. Overall Assessment

| Criterion | Rating |
|-----------|--------|
| Requirement Alignment | ⭐⭐⭐⭐½ |
| Technical Soundness | ⭐⭐⭐⭐ |
| Implementation Clarity | ⭐⭐⭐⭐ |
| Completeness | ⭐⭐⭐½ |

**Overall: ⭐⭐⭐⭐ (4/5 stars)**

### Summary

This is a **solid, well-structured design** that addresses the core requirements. The modular architecture is appropriate for the complexity, and the choice of tools (`dlopen`, `perf_event_open`, `/proc`) is correct for Linux.

**Key strengths:**
- Clear separation of concerns (loader, core, per-algorithm modules)
- Comprehensive performance metrics (time + cycles + throughput)
- Dual-mode stability testing (time or count-based)

**Before implementation, address:**
1. Complete KEX vtable signatures
2. Normalize memory measurement units
3. Add signal handling for long-running tests
4. Use cryptographically-secure RNG

The design is **ready to implement** after fixing the identified blockers. No major architectural changes are needed.
