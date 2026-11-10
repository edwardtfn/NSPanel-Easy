#!/usr/bin/env python3
"""Fail when an ESPHome build uses too much of its RAM or flash.

Reads the usage that PlatformIO prints after linking, from the GitHub Actions
logs of the build jobs (see esphome_size_report.py), and checks every build
against two limits:

  - WARN_PERCENT: a warning annotation, so growth is noticed early;
  - FAIL_PERCENT: an error annotation, and the script fails.

Flash is relative to the app partition, which must also hold the next OTA
image, so the limits apply to it as to RAM. Builds whose name contains an
excluded word (e.g. "Customizations", which tests users' own additions) are
reported but never checked.

The logs may come from PRs from forks, so they are untrusted: only numbers are
taken from them, and job names are reduced to a safe set of characters.

Usage:
  esphome_size_guard.py INDEX [--summary FILE]

  INDEX           tab-separated file, one line per job: log file, conclusion, job name
  --summary FILE  append a Markdown table to FILE (e.g. $GITHUB_STEP_SUMMARY)
"""

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from esphome_size_report import parse_log, safe_name  # noqa: E402

WARN_PERCENT = 85.0
FAIL_PERCENT = 90.0
EXCLUDED = ("Customizations",)


def check(index_path):
    """Return (rows, failures, warnings) for the successful builds in the index."""
    rows, failures, warnings = [], [], []
    with open(index_path, encoding="utf-8", errors="replace") as f:
        for line in f:
            parts = line.rstrip("\n").split("\t", 2)
            if len(parts) != 3 or parts[1] != "success":
                continue  # A failed build already fails the workflow
            log_path, _, raw_name = parts
            name = safe_name(raw_name)
            excluded = any(word in name for word in EXCLUDED)
            usage = parse_log(log_path)
            cells = []
            for kind in ("RAM", "Flash"):
                if kind not in usage:
                    cells.append("n/a")
                    continue
                used, total = usage[kind]
                percent = 100 * used / total
                cells.append(f"{percent:.1f}%")
                if excluded:
                    continue
                message = f"{name}: {kind} {percent:.1f}% ({used:,} of {total:,} bytes)"
                if percent >= FAIL_PERCENT:
                    failures.append(message)
                elif percent >= WARN_PERCENT:
                    warnings.append(message)
            rows.append((name, cells[0], cells[1], "not checked" if excluded else ""))
    return rows, failures, warnings


def main():
    parser = argparse.ArgumentParser(description="Fail when an ESPHome build uses too much RAM or flash.")
    parser.add_argument("index")
    parser.add_argument("--summary", help="append a Markdown table to this file")
    args = parser.parse_args()

    rows, failures, warnings = check(args.index)
    for message in warnings:
        print(f"::warning title=Build size::{message} is at or above {WARN_PERCENT:.0f}%")
    for message in failures:
        print(f"::error title=Build size::{message} is at or above {FAIL_PERCENT:.0f}%")

    if args.summary:
        lines = ["### ESPHome build size guard", ""]
        if rows:
            lines += ["| Build | RAM | Flash | |", "|---|---:|---:|---|"]
            lines += [f"| {name} | {ram} | {flash} | {note} |" for name, ram, flash, note in sorted(rows)]
        else:
            lines.append("No successful build to check.")
        lines += ["", f"Warning at {WARN_PERCENT:.0f}%, failure at {FAIL_PERCENT:.0f}% of RAM or of the app partition."]
        with open(args.summary, "a", encoding="utf-8") as f:
            f.write("\n".join(lines) + "\n")

    print(f"{len(rows)} build(s) checked: {len(failures)} over {FAIL_PERCENT:.0f}%, "
          f"{len(warnings)} over {WARN_PERCENT:.0f}%")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
