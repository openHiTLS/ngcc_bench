#!/usr/bin/env bash
# run_mlkem_bench.sh – Build and run ML-KEM mock through all ngcc_bench modes.
#
# Usage:  bash run_mlkem_bench.sh /path/to/ngcc_bench
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "usage: $0 /path/to/ngcc_bench"
  exit 2
fi

BIN="$1"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

SO="$TMP_DIR/libmock_mlkem.so"
JSON_FILE="$TMP_DIR/mlkem_report.json"

echo "=== [1/5] Compiling mock ML-KEM-512 library ==="
cc -shared -fPIC -O2 "$ROOT_DIR/tests/mock/mock_mlkem.c" -o "$SO"
echo "  -> $SO"

# ── Correctness ────────────────────────────────────────────────────
echo ""
echo "=== [2/5] Correctness Test (KEM only) ==="
OUT="$TMP_DIR/correctness.log"
"$BIN" \
  --lib "$SO" \
  --test kem \
  --mode correctness \
  > "$OUT" 2>&1
cat "$OUT"
grep -q "\[kem\]\[correctness\] PASS" "$OUT"
echo "  ✓ KEM correctness PASS"

# ── Performance ────────────────────────────────────────────────────
echo ""
echo "=== [3/5] Performance Test (KEM, 1000 iterations) ==="
OUT="$TMP_DIR/performance.log"
"$BIN" \
  --lib "$SO" \
  --test kem \
  --mode performance \
  --iterations 1000 \
  --cycles off \
  > "$OUT" 2>&1
cat "$OUT"
grep -q "\[kem\]\[performance\] ops=" "$OUT"
grep -q "ops/s=" "$OUT"
grep -q "\[kem\]\[performance\]\[time\]" "$OUT"
echo "  ✓ KEM performance metrics collected"

# ── Stability ──────────────────────────────────────────────────────
echo ""
echo "=== [4/5] Stability Test (KEM, short run) ==="
OUT="$TMP_DIR/stability.log"
"$BIN" \
  --lib "$SO" \
  --test kem \
  --mode stability \
  --duration-hours 0.0001 \
  --stability-max-cases 200 \
  --stability-sample-ms 1.0 \
  --cycles off \
  --stable-throughput-cv-percent 100 \
  --stable-cycles-cv-percent 100 \
  --stable-time-cv-percent 100 \
  --stable-memory-growth-percent 100 \
  --warning-throughput-cv-percent 120 \
  --warning-cycles-cv-percent 120 \
  --warning-time-cv-percent 120 \
  --warning-memory-growth-percent 120 \
  --json-out "$JSON_FILE" \
  > "$OUT" 2>&1
cat "$OUT"
grep -q "\[kem\]\[stability\]" "$OUT"
grep -q "\[kem\]\[stability\]\[throughput\]" "$OUT"
grep -q "\[kem\]\[stability\]\[throughput_bytes\]" "$OUT"
grep -q "\[kem\]\[stability\]\[memory\]" "$OUT"
grep -q "\[kem\]\[stability\]\[errors\]" "$OUT"
echo "  ✓ KEM stability metrics collected"

# ── Memory ─────────────────────────────────────────────────────────
echo ""
echo "=== [5/5] Memory Test (KEM) ==="
OUT="$TMP_DIR/memory.log"
"$BIN" \
  --lib "$SO" \
  --test kem \
  --mode memory \
  > "$OUT" 2>&1
cat "$OUT"
grep -q "\[memory\]" "$OUT"
grep -q "baseline_bytes=" "$OUT"
grep -q "peak_bytes=" "$OUT"
echo "  ✓ KEM memory metrics collected"

# ── Summary ────────────────────────────────────────────────────────
echo ""
echo "============================================"
echo "  All ML-KEM-512 verification tests PASSED"
echo "============================================"

if [[ -f "$JSON_FILE" ]]; then
  echo ""
  echo "JSON report saved to: $JSON_FILE"
  echo "--- JSON snippet (first 20 lines) ---"
  head -20 "$JSON_FILE"
fi
