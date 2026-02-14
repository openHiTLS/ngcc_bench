#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  run_stability_profile.sh --benchmark /path/to/ngcc_benchmark
                           [--lib /path/to/lib.so]
                           [--profile quick|soak|nightly]
                           [--output-dir DIR]
                           [--taskset-cpu CPU]
                           [--allow-unstable]

Notes:
  - If --lib is omitted, the script builds tests/mock/mock_ngcc.c and uses it.
  - Reports are written to OUTPUT_DIR with timestamp suffix.
EOF
}

BENCHMARK=""
LIB_PATH=""
PROFILE="quick"
OUTPUT_DIR="reports/stability"
TASKSET_CPU=""
ALLOW_UNSTABLE=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --benchmark)
      BENCHMARK="$2"
      shift 2
      ;;
    --lib)
      LIB_PATH="$2"
      shift 2
      ;;
    --profile)
      PROFILE="$2"
      shift 2
      ;;
    --output-dir)
      OUTPUT_DIR="$2"
      shift 2
      ;;
    --taskset-cpu)
      TASKSET_CPU="$2"
      shift 2
      ;;
    --allow-unstable)
      ALLOW_UNSTABLE=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "unknown arg: $1" >&2
      usage
      exit 2
      ;;
  esac
done

if [[ -z "$BENCHMARK" ]]; then
  echo "--benchmark is required" >&2
  usage
  exit 2
fi

if [[ ! -x "$BENCHMARK" ]]; then
  echo "benchmark binary not executable: $BENCHMARK" >&2
  exit 2
fi

case "$PROFILE" in
  quick)
    DURATION_HOURS="0.002"
    MAX_CASES="200000"
    SAMPLE_MS="2.0"
    ;;
  soak)
    DURATION_HOURS="0.05"
    MAX_CASES="2000000"
    SAMPLE_MS="5.0"
    ;;
  nightly)
    DURATION_HOURS="0.25"
    MAX_CASES="10000000"
    SAMPLE_MS="5.0"
    ;;
  *)
    echo "invalid --profile: $PROFILE" >&2
    exit 2
    ;;
esac

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

if [[ -z "$LIB_PATH" ]]; then
  LIB_PATH="$TMP_DIR/libmock_ngcc.so"
  cc -shared -fPIC "$ROOT_DIR/tests/mock/mock_ngcc.c" -o "$LIB_PATH"
  if [[ "$ALLOW_UNSTABLE" -eq 0 ]]; then
    # mock library may show memory growth noise on cold start
    ALLOW_UNSTABLE=1
  fi
fi

if [[ ! -f "$LIB_PATH" ]]; then
  echo "lib not found: $LIB_PATH" >&2
  exit 2
fi

mkdir -p "$OUTPUT_DIR"
TS="$(date +%Y%m%d_%H%M%S)"
REPORT_JSON="$OUTPUT_DIR/stability_${PROFILE}_${TS}.json"
META_TXT="$OUTPUT_DIR/stability_${PROFILE}_${TS}.meta.txt"
LOG_TXT="$OUTPUT_DIR/stability_${PROFILE}_${TS}.log"

{
  echo "timestamp=$(date -Iseconds)"
  echo "profile=$PROFILE"
  echo "benchmark=$BENCHMARK"
  echo "lib=$LIB_PATH"
  echo "duration_hours=$DURATION_HOURS"
  echo "max_cases=$MAX_CASES"
  echo "sample_ms=$SAMPLE_MS"
  echo "hostname=$(hostname)"
  echo "kernel=$(uname -srmo)"
  if [[ -r /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor ]]; then
    echo "cpu0_governor=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor)"
  fi
} > "$META_TXT"

CMD=(
  "$BENCHMARK"
  --lib "$LIB_PATH"
  --test all
  --mode stability
  --digest-len-bits 8
  --duration-hours "$DURATION_HOURS"
  --stability-max-cases "$MAX_CASES"
  --stability-sample-ms "$SAMPLE_MS"
  --json-out "$REPORT_JSON"
)

if [[ -n "$TASKSET_CPU" ]]; then
  if command -v taskset >/dev/null 2>&1; then
    CMD=(taskset -c "$TASKSET_CPU" "${CMD[@]}")
    echo "taskset_cpu=$TASKSET_CPU" >> "$META_TXT"
  else
    echo "taskset requested but command not found" >> "$META_TXT"
  fi
fi

set +e
"${CMD[@]}" | tee "$LOG_TXT"
RC=${PIPESTATUS[0]}
set -e

echo "report_json=$REPORT_JSON" | tee -a "$META_TXT"
echo "log_file=$LOG_TXT" | tee -a "$META_TXT"

if [[ "$RC" -ne 0 && "$ALLOW_UNSTABLE" -eq 0 ]]; then
  echo "stability run failed (rc=$RC)" >&2
  exit "$RC"
fi

if [[ "$RC" -ne 0 ]]; then
  echo "stability run returned rc=$RC but allowed by --allow-unstable"
fi

echo "stability profile completed: $REPORT_JSON"
