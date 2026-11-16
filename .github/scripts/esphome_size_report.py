#!/usr/bin/env python3
"""Markdown report with the RAM and flash usage of ESPHome builds.

The sizes come from the JSON files that the build jobs write with
esphome_size_guard.py --output and upload as artifacts, one per build:

  {"name": "Basic (IDF) (latest)", "flash_used": 1066089, "flash_total": 1835008, "ram_used": 40308}

The list of builds comes from the jobs of the run, so a build that failed
before it wrote its sizes is reported as failed, and one without sizes as n/a.

The artifacts may come from PRs from forks, so they are untrusted: only numbers
are taken from them, build names are reduced to a safe set of characters, and
the number and the size of the files read are limited.

Usage:
  esphome_size_report.py JOBS SIZES_DIR [--output FILE]

  JOBS           tab-separated file, one line per job: conclusion, job name
  SIZES_DIR      directory with the JSON files of the builds (searched recursively)
  --output FILE  write the report to FILE instead of printing it
"""

import argparse
import json
import re
import sys
from pathlib import Path

SUFFIX = " (latest)"
MAX_FILES = 200
MAX_FILE_BYTES = 4096
MAX_SIZE = 1 << 32


def safe_name(name):
    """Job name reduced to characters that cannot break the Markdown table."""
    name = name.strip()
    if name.endswith(SUFFIX):
        name = name[: -len(SUFFIX)]
    return re.sub(r"[^A-Za-z0-9 ._()+-]", "", name)[:80] or "unnamed"


def size(value):
    """A size in bytes as a positive integer, or None."""
    if isinstance(value, bool) or not isinstance(value, int) or not 0 < value < MAX_SIZE:
        return None
    return value


def load_sizes(sizes_dir):
    """{safe build name: {"flash_used", "flash_total", "ram_used"}} from the JSON files under sizes_dir."""
    sizes = {}
    for path in sorted(Path(sizes_dir).rglob("*.json"))[:MAX_FILES]:
        try:
            if not path.is_file() or path.is_symlink() or path.stat().st_size > MAX_FILE_BYTES:
                continue
            data = json.loads(path.read_text(encoding="utf-8", errors="replace"))
        except (OSError, ValueError):
            continue
        if not isinstance(data, dict) or not isinstance(data.get("name"), str):
            continue
        sizes[safe_name(data["name"])] = {key: size(data.get(key)) for key in ("flash_used", "flash_total", "ram_used")}
    return sizes


def flash_cell(build):
    """Table cell with the flash usage, relative to the app partition."""
    if not build or build["flash_used"] is None or build["flash_total"] is None:
        return "n/a"
    used, total = build["flash_used"], build["flash_total"]
    return f"{100 * used / total:.1f}% ({used:,} of {total:,})"


def ram_cell(build):
    """Table cell with the static RAM usage."""
    if not build or build["ram_used"] is None:
        return "n/a"
    return f"{build['ram_used']:,}"


def report(jobs_path, sizes_dir):
    sizes = load_sizes(sizes_dir)
    rows = []
    with open(jobs_path, encoding="utf-8", errors="replace") as f:
        for line in f:
            parts = line.rstrip("\n").split("\t", 1)
            if len(parts) != 2:
                continue
            conclusion, name = parts
            build = sizes.get(safe_name(name))
            if build is None and conclusion != "success":
                rows.append((safe_name(name), "build failed", "build failed"))
            else:
                # A build over the size limit fails after its sizes were written, so they are still shown
                rows.append((safe_name(name), ram_cell(build), flash_cell(build)))

    lines = ["### ESPHome build sizes (ESPHome latest)", ""]
    if not rows:
        lines.append("No ESPHome builds ran for this commit.")
        return "\n".join(lines) + "\n"
    lines += ["| Build | Static RAM (bytes) | Flash (bytes) |", "|---|---:|---:|"]
    lines += [f"| {name} | {ram} | {flash} |" for name, ram, flash in sorted(rows, key=lambda row: row[0].lower())]
    lines += ["", "Flash is relative to the app partition. Static RAM is the data and BSS of the firmware, "
                  "without the heap used at run time. Builds with ESPHome dev are not included."]
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description="RAM and flash usage of ESPHome builds.")
    parser.add_argument("jobs")
    parser.add_argument("sizes_dir")
    parser.add_argument("--output", help="write the report to this file instead of printing it")
    args = parser.parse_args()

    text = report(args.jobs, args.sizes_dir)
    if args.output:
        with open(args.output, "w", encoding="utf-8") as f:
            f.write(text)
    else:
        print(text, end="")
    return 0


if __name__ == "__main__":
    sys.exit(main())
