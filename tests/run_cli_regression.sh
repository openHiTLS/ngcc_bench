#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "usage: $0 /path/to/ngcc_bench"
  exit 2
fi

BIN="$1"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

MOCK_SO="$TMP_DIR/libmock_ngcc.so"
KAT_FILE="$TMP_DIR/vectors.kat"
JSON_FILE="$TMP_DIR/report.json"

cc -shared -fPIC "$ROOT_DIR/tests/mock/mock_ngcc.c" -o "$MOCK_SO"

cat > "$KAT_FILE" <<'EOF'
# compatibility: empty msg, md alias, comments, metadata and 0x prefix
COUNT = 0
MLEN = 0
INPUT =
MD = 08

; signature aliases
COUNT = 1
PUBLICKEY = 01
MESSAGE = aa
SIGNATURE = 03

// kem aliases + optional separators
COUNT = 2
SECRETKEY = 0x02
CIPHERTEXT = 08
SHAREDSECRET = 07

; kex A-side aliases
COUNT = 3
SK_A = 02
PK_B = 01
PASS2 = 22
STATEA = 03
SHAREDSECRETA = 09

; kex B-side aliases
COUNT = 4
SK_B = 02
PK_A = 01
PASS3 = 33
STATEB = 03
SHAREDSECRETB = 09
EOF

OUT1="$TMP_DIR/correctness.log"
"$BIN" \
  --lib "$MOCK_SO" \
  --test all \
  --mode correctness \
  --digest-len-bits 8 \
  --kat "$KAT_FILE" > "$OUT1" 2>&1

grep -q "\[hash\]\[correctness\] PASS" "$OUT1"
grep -q "\[sig\]\[correctness\] PASS" "$OUT1"
grep -q "\[kem\]\[correctness\] PASS" "$OUT1"
grep -q "\[kex\]\[correctness\] PASS" "$OUT1"

OUTP="$TMP_DIR/performance.log"
"$BIN" \
  --lib "$MOCK_SO" \
  --test hash \
  --mode performance \
  --digest-len-bits 8 \
  --msg-len 64 \
  --iterations 64 \
  --cycles off > "$OUTP" 2>&1

grep -q "\[hash\]\[performance\] ops=" "$OUTP"
grep -q "bytes/s=" "$OUTP"
grep -q "bytes/op=" "$OUTP"

OUT2="$TMP_DIR/threshold_invalid.log"
if "$BIN" \
  --lib "$MOCK_SO" \
  --test kem \
  --mode stability \
  --stability-sample-ms 1 \
  --stable-throughput-cv-percent 6 \
  --warning-throughput-cv-percent 5 > "$OUT2" 2>&1; then
  echo "expected invalid threshold command to fail"
  exit 1
fi
grep -q "warning thresholds must be >= stable thresholds" "$OUT2"

OUT3="$TMP_DIR/sample_invalid.log"
if "$BIN" \
  --lib "$MOCK_SO" \
  --test kem \
  --mode stability \
  --stability-sample-ms 0 > "$OUT3" 2>&1; then
  echo "expected invalid sample window command to fail"
  exit 1
fi
grep -q "invalid --stability-sample-ms value" "$OUT3"

OUT4="$TMP_DIR/stability.log"
"$BIN" \
  --lib "$MOCK_SO" \
  --test kem \
  --mode stability \
  --duration-hours 0.0001 \
  --stability-max-cases 128 \
  --stability-sample-ms 0.5 \
  --cycles off \
  --stable-throughput-cv-percent 100 \
  --stable-cycles-cv-percent 100 \
  --stable-time-cv-percent 100 \
  --stable-memory-growth-percent 100 \
  --warning-throughput-cv-percent 120 \
  --warning-cycles-cv-percent 120 \
  --warning-time-cv-percent 120 \
  --warning-memory-growth-percent 120 \
  --json-out "$JSON_FILE" > "$OUT4" 2>&1

grep -q "\[kem\]\[stability\]" "$OUT4"
grep -q "\[kem\]\[stability\]\[throughput_bytes\]" "$OUT4"
grep -q '"schema_version": 3' "$JSON_FILE"
grep -q '"stability_sample_ms": 0.500000' "$JSON_FILE"
grep -q '"stability_thresholds"' "$JSON_FILE"
grep -q '"throughput_mean_bytes":' "$JSON_FILE"
python3 "$ROOT_DIR/tests/validate_json_report.py" "$JSON_FILE" "$ROOT_DIR/docs/json_schema_v3.json"

# ── Partial-load: hash-only .so ────────────────────────────────────
HASH_ONLY_SO="$TMP_DIR/libmock_hash_only.so"
cc -shared -fPIC "$ROOT_DIR/tests/mock/mock_hash_only.c" -o "$HASH_ONLY_SO"

OUT5="$TMP_DIR/hash_only.log"
"$BIN" \
  --lib "$HASH_ONLY_SO" \
  --test hash \
  --mode correctness \
  --digest-len-bits 8 > "$OUT5" 2>&1

grep -q "\[hash\]\[correctness\] PASS" "$OUT5"

# Verify that running a non-hash test against hash-only .so fails
OUT6="$TMP_DIR/hash_only_sig.log"
if "$BIN" \
  --lib "$HASH_ONLY_SO" \
  --test sig \
  --mode correctness > "$OUT6" 2>&1; then
  echo "expected sig test on hash-only .so to fail"
  exit 1
fi
grep -q "missing symbol" "$OUT6"

echo "cli regression passed"
