#!/usr/bin/env python3
"""Markdown report with the RAM and flash usage of ESPHome builds.

The usage is parsed from the GitHub Actions logs of the build jobs, from the
lines that PlatformIO prints after linking, e.g.:

  RAM:   [=         ]  12.3% (used 40308 bytes from 327680 bytes)
  Flash: [======    ]  58.1% (used 1066089 bytes from 1835008 bytes)

The logs may come from PRs from forks, so they are untrusted: only numbers are
taken from them, and job names are reduced to a safe set of characters.

Usage:
  esphome_size_report.py INDEX [--output FILE]

  INDEX          tab-separated file, one line per job: log file, conclusion, job name
  --output FILE  write the report to FILE instead of printing it
"""

import argparse
import re
import sys

USAGE_RE = re.compile(r"(RAM|Flash):\s*\[[= ]*\]\s*[0-9.]+%\s*\(used ([0-9]+) bytes from ([0-9]+) bytes\)")
ANSI_RE = re.compile(r"\x1b\[[0-9;]*m")
SUFFIX = " (latest)"
MAX_LOG_CHARS = 20_000_000


def parse_log(path):
    """Return {"RAM": (used, total), "Flash": (used, total)} with the last values found in the log."""
    usage = {}
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            text = f.read(MAX_LOG_CHARS)
    except OSError:
        return usage
    for match in USAGE_RE.finditer(ANSI_RE.sub("", text)):
        used, total = int(match.group(2)), int(match.group(3))
        if 0 < total:
            usage[match.group(1)] = (used, total)
    return usage


def safe_name(name):
    """Job name reduced to characters that cannot break the Markdown table."""
    name = name.strip()
    if name.endswith(SUFFIX):
        name = name[: -len(SUFFIX)]
    return re.sub(r"[^A-Za-z0-9 ._()+-]", "", name)[:80] or "unnamed"


def cell(usage, kind):
    """Table cell for one kind of usage."""
    if kind not in usage:
        return "n/a"
    used, total = usage[kind]
    return f"{100 * used / total:.1f}% ({used:,} of {total:,})"


def report(index_path):
    rows = []
    with open(index_path, encoding="utf-8", errors="replace") as f:
        for line in f:
            parts = line.rstrip("\n").split("\t", 2)
            if len(parts) != 3:
                continue
            log_path, conclusion, name = parts
            if conclusion == "success":
                usage = parse_log(log_path)
                rows.append((safe_name(name), cell(usage, "RAM"), cell(usage, "Flash")))
            else:
                rows.append((safe_name(name), "build failed", "build failed"))

    lines = ["### ESPHome build sizes (ESPHome latest)", ""]
    if not rows:
        lines.append("No ESPHome builds ran for this commit.")
        return "\n".join(lines) + "\n"
    lines += ["| Build | RAM (bytes) | Flash (bytes) |", "|---|---:|---:|"]
    lines += [f"| {name} | {ram} | {flash} |" for name, ram, flash in sorted(rows, key=lambda row: row[0].lower())]
    lines += ["", "Flash is relative to the app partition. Builds with ESPHome dev are not included."]
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description="RAM and flash usage of ESPHome builds.")
    parser.add_argument("index")
    parser.add_argument("--output", help="write the report to this file instead of printing it")
    args = parser.parse_args()

    text = report(args.index)
    if args.output:
        with open(args.output, "w", encoding="utf-8") as f:
            f.write(text)
    else:
        print(text, end="")
    return 0


if __name__ == "__main__":
    sys.exit(main())
