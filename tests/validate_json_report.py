#!/usr/bin/env python3
import json
import math
import sys


def fail(msg: str) -> None:
    print(f"json validation failed: {msg}", file=sys.stderr)
    sys.exit(1)


def expect(cond: bool, msg: str) -> None:
    if not cond:
        fail(msg)


def is_number(v) -> bool:
    return isinstance(v, (int, float)) and not isinstance(v, bool) and math.isfinite(float(v))


def require_key(obj, key: str):
    expect(isinstance(obj, dict), f"object expected before key '{key}'")
    expect(key in obj, f"missing key '{key}'")
    return obj[key]


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: validate_json_report.py REPORT_JSON SCHEMA_JSON", file=sys.stderr)
        return 2

    report_path = sys.argv[1]
    schema_path = sys.argv[2]

    with open(report_path, "r", encoding="utf-8") as f:
        report = json.load(f)
    with open(schema_path, "r", encoding="utf-8") as f:
        schema = json.load(f)

    expect(schema.get("properties", {}).get("schema_version", {}).get("const") == 3,
           "schema const schema_version must be 3")

    expect(require_key(report, "schema_version") == 3, "schema_version must be 3")
    expect(isinstance(require_key(report, "timestamp"), str), "timestamp must be string")
    expect(isinstance(require_key(report, "library"), str), "library must be string")

    opts = require_key(report, "options")
    for k in ("test_mask", "mode_mask", "iterations", "stability_max_cases", "msg_len", "digest_len_bits"):
        expect(isinstance(require_key(opts, k), int), f"options.{k} must be integer")
    for k in ("duration_hours", "stability_sample_ms"):
        expect(is_number(require_key(opts, k)), f"options.{k} must be finite number")
        expect(float(opts[k]) > 0.0, f"options.{k} must be > 0")
    expect(require_key(opts, "cycles") in ("on", "off"), "options.cycles must be on|off")
    kat = require_key(opts, "kat")
    expect(kat is None or isinstance(kat, str), "options.kat must be string|null")

    thr = require_key(opts, "stability_thresholds")
    stable_keys = (
        "stable_throughput_cv_percent",
        "stable_cycles_cv_percent",
        "stable_time_cv_percent",
        "stable_memory_growth_percent",
        "stable_error_rate_percent",
    )
    warning_keys = (
        "warning_throughput_cv_percent",
        "warning_cycles_cv_percent",
        "warning_time_cv_percent",
        "warning_memory_growth_percent",
        "warning_error_rate_percent",
    )
    for k in stable_keys + warning_keys:
        expect(is_number(require_key(thr, k)), f"threshold {k} must be finite number")
        expect(float(thr[k]) >= 0.0, f"threshold {k} must be >= 0")
    for s, w in zip(stable_keys, warning_keys):
        expect(float(thr[w]) >= float(thr[s]), f"threshold pair invalid: {w} < {s}")

    tests = require_key(report, "tests")
    statuses = {"PASS", "FAIL", "STOPPED", "SKIPPED"}
    for name in ("hash", "dsa", "dsa-keygen", "dsa-sig", "dsa-verify", "kem", "kex",
                 "kem-keygen", "kem-encap", "kem-decap"):
        t = require_key(tests, name)
        expect(isinstance(require_key(t, "selected"), bool), f"tests.{name}.selected must be bool")
        for k in ("correctness", "performance", "stability"):
            expect(require_key(t, k) in statuses, f"tests.{name}.{k} invalid status")
        expect("kat" in t, f"tests.{name}.kat missing")
        expect("performance_metrics" in t, f"tests.{name}.performance_metrics missing")
        expect("stability_metrics" in t, f"tests.{name}.stability_metrics missing")

    mem = require_key(report, "memory")
    expect(require_key(mem, "status") in statuses, "memory.status invalid")
    static_mem = require_key(mem, "static_mem")
    for k in ("text_bytes", "data_bytes", "bss_bytes", "rodata_bytes", "total_bytes"):
        expect(isinstance(require_key(static_mem, k), int), f"memory.static_mem.{k} must be integer")
    expect(isinstance(require_key(mem, "heap_baseline_bytes"), int), "memory.heap_baseline_bytes must be integer")
    expect(isinstance(require_key(mem, "heap_peak_bytes"), int), "memory.heap_peak_bytes must be integer")

    overall = require_key(report, "overall")
    expect(require_key(overall, "status") in ("PASS", "FAIL"), "overall.status invalid")

    return 0


if __name__ == "__main__":
    sys.exit(main())
