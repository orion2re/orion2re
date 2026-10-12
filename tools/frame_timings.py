#!/usr/bin/env python3
"""Summarize opt-in presentation timings without treating idle gaps as FPS."""
import argparse
import csv
import json
import math
from collections import defaultdict
from pathlib import Path


def percentile(values, fraction):
    ordered = sorted(values)
    position = (len(ordered) - 1) * fraction
    lower = int(position)
    upper = min(lower + 1, len(ordered) - 1)
    return ordered[lower] + (ordered[upper] - ordered[lower]) * (position - lower)


def statistics(values):
    if not values:
        return None
    return {
        "samples": len(values),
        "mean_ms": sum(values) / len(values),
        "p50_ms": percentile(values, 0.5),
        "p95_ms": percentile(values, 0.95),
        "p99_ms": percentile(values, 0.99),
        "max_ms": max(values),
    }


def summarize(rows, warmup_seconds):
    groups = defaultdict(lambda: {"present": [], "interval": [], "published": 0})
    previous = None
    for line, row in enumerate(rows, 2):
        try:
            values = {key: float(row[key]) for key in ("start_ms", "present_ms", "interval_ms")}
            if any(not math.isfinite(value) or value < 0 for value in values.values()):
                raise ValueError("timings must be finite and nonnegative")
            for key in ("published", "captured"):
                if row[key] not in ("0", "1"):
                    raise ValueError(f"invalid {key} flag")
            group = (row["renderer"], row["scaler"])
        except (KeyError, TypeError, ValueError) as error:
            raise ValueError(f"CSV line {line}: {error}") from error
        eligible = row["captured"] == "0" and values["start_ms"] >= warmup_seconds * 1000
        if eligible:
            target = groups[group]
            target["present"].append(values["present_ms"])
            target["published"] += row["published"] == "1"
            # Capture I/O occurs after the previous presentation. Exclude that interval
            # and boundaries between renderer/scaler groups and warmup periods.
            if previous == (group, True) and values["interval_ms"] > 0:
                target["interval"].append(values["interval_ms"])
        previous = (group, eligible)
    return [
        {"renderer": renderer, "scaler": scaler, "published_samples": values["published"],
         "presentation": statistics(values["present"]),
         "presentation_interval": statistics(values["interval"])}
        for (renderer, scaler), values in sorted(groups.items())
    ]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", type=Path)
    parser.add_argument("--warmup-seconds", type=float, default=2.0)
    args = parser.parse_args()
    if not math.isfinite(args.warmup_seconds) or args.warmup_seconds < 0:
        parser.error("warmup must be finite and nonnegative")
    try:
        with args.csv.open(newline="") as stream:
            result = summarize(csv.DictReader(stream), args.warmup_seconds)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
