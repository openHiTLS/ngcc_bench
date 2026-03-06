#!/usr/bin/env python3
import argparse
import json
import math
import sys


def to_num(v):
    if isinstance(v, bool):
        return None
    if isinstance(v, (int, float)) and math.isfinite(float(v)):
        return float(v)
    return None


def load_json(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def get_metric(report, algo, key):
    try:
        m = report["tests"][algo]["stability_metrics"]
        if m is None:
            return None
        return to_num(m.get(key))
    except Exception:
        return None


def get_status(report, algo):
    try:
        return report["tests"][algo]["stability"]
    except Exception:
        return None


def fail(msg):
    print(f"compare failed: {msg}", file=sys.stderr)
    return False


def main():
    p = argparse.ArgumentParser(description="Compare stability reports (baseline vs candidate).")
    p.add_argument("baseline")
    p.add_argument("candidate")
    p.add_argument("--allow-cv-regress-ratio", type=float, default=0.20,
                   help="Allowed relative regression ratio for CV metrics (default 0.20 = +20%%)")
    p.add_argument("--allow-memory-growth-abs", type=float, default=1.0,
                   help="Allowed absolute regression for memory_growth_percent (default +1.0)")
    p.add_argument("--allow-error-rate-abs", type=float, default=0.0,
                   help="Allowed absolute regression for error_rate_percent (default +0.0)")
    args = p.parse_args()

    base = load_json(args.baseline)
    cur = load_json(args.candidate)

    ok = True
    algos = ("hash", "dsa", "kem", "kex")
    cv_keys = ("throughput_cv_percent", "time_cv_percent", "cycles_cv_percent")

    for algo in algos:
        base_status = get_status(base, algo)
        cur_status = get_status(cur, algo)

        if base_status in ("PASS",) and cur_status in ("FAIL",):
            ok = fail(f"{algo}: baseline PASS but candidate FAIL") and ok

        for k in cv_keys:
            b = get_metric(base, algo, k)
            c = get_metric(cur, algo, k)
            if b is None or c is None:
                continue
            limit = b * (1.0 + args.allow_cv_regress_ratio)
            if c > limit:
                ok = fail(f"{algo}.{k}: {c:.6f} > {limit:.6f} (baseline {b:.6f})") and ok

        b_mem = get_metric(base, algo, "memory_growth_percent")
        c_mem = get_metric(cur, algo, "memory_growth_percent")
        if b_mem is not None and c_mem is not None:
            if c_mem > b_mem + args.allow_memory_growth_abs:
                ok = fail(f"{algo}.memory_growth_percent: {c_mem:.6f} > {b_mem + args.allow_memory_growth_abs:.6f}") and ok

        b_err = get_metric(base, algo, "error_rate_percent")
        c_err = get_metric(cur, algo, "error_rate_percent")
        if b_err is not None and c_err is not None:
            if c_err > b_err + args.allow_error_rate_abs:
                ok = fail(f"{algo}.error_rate_percent: {c_err:.6f} > {b_err + args.allow_error_rate_abs:.6f}") and ok

    if not ok:
        return 1

    print("stability compare passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
