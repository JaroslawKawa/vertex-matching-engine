#!/usr/bin/env python3
import json
import sys

def main():
    if len(sys.argv) != 2:
        print("Usage: smoke_to_md.py <smoke.json>", file=sys.stderr)
        return 1

    with open(sys.argv[1], "r", encoding="utf-8") as f:
        data = json.load(f)

    aggr = data.get("aggregates", {})
    if not aggr:
        print("_No aggregates found in JSON output._")
        return 0

    print("### Aggregates")
    print("")
    print("| Scenario | median ops/sec | p50 | p95 | p99 |")
    print("|---|---:|---:|---:|---:|")

    for scenario in sorted(aggr.keys()):
        a = aggr[scenario]
        ops = a.get("median_ops_per_sec")
        lat = a.get("latency", {})
        p50 = lat.get("p50")
        p95 = lat.get("p95")
        p99 = lat.get("p99")

        def fmt(x):
            return "-" if x is None else f"{float(x):.2f}"

        print(f"| `{scenario}` | {fmt(ops)} | {fmt(p50)} | {fmt(p95)} | {fmt(p99)} |")

    return 0

if __name__ == "__main__":
    raise SystemExit(main())