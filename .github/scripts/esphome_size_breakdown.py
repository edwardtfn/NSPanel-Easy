#!/usr/bin/env python3
"""Break the flash usage of an ESPHome build down by archive and by file.

Run in the build job, right after the build: finds the build's linker map and
appends to the job summary the largest archives (libraries, ESPHome components)
and the largest object files, as reported by Espressif's esp-idf-size
(pinned in .github/requirements-esp-idf-size.txt). It shows where flash goes,
so size work targets the real contributors.

Informational only: it never fails the job, also when the map cannot be found
or read (e.g. builds whose map esp-idf-size does not understand). The tables
come from the build, so from the PR: every line is sanitized before it is
written to the summary.

Usage:
  esphome_size_breakdown.py --name NAME --search-dir DIR --summary FILE [--rows N]

  --name NAME        build name, for the summary heading
  --search-dir DIR   directory holding the build output (the YAML file's directory)
  --summary FILE     file to append the Markdown to (e.g. $GITHUB_STEP_SUMMARY)
  --rows N           rows kept from each table (default 30)
"""

import argparse
import os
import subprocess
import sys
from pathlib import Path

TIMEOUT_S = 300


def find_map(search_dir):
    """The newest linker map under search_dir, or None."""
    maps = [path for path in Path(search_dir).rglob("*.map") if path.is_file() and path.stat().st_size > 0]
    return max(maps, key=lambda path: path.stat().st_mtime) if maps else None


def run_size(map_path, option):
    """Text table from esp-idf-size, or None when it fails."""
    env = dict(os.environ, COLUMNS="160", NO_COLOR="1", TERM="dumb")
    # A fixed argument list, no shell: the map path is a file found by the script, passed as a single argument
    command = [sys.executable, "-m", "esp_idf_size", "--format", "table", option, str(map_path)]
    try:
        result = subprocess.run(  # nosemgrep: python.lang.security.audit.dangerous-subprocess-use-audit
            command, capture_output=True, text=True, timeout=TIMEOUT_S, env=env, check=False)
    except (OSError, subprocess.TimeoutExpired) as error:
        print(f"esp-idf-size {option} failed: {error}")
        return None
    if result.returncode != 0 or not result.stdout.strip():
        print(f"esp-idf-size {option} failed ({result.returncode}): {result.stderr.strip()[:500]}")
        return None
    return result.stdout


def sanitize(line):
    """A table line that cannot close the code fence nor carry Markdown or HTML out of it.

    Symbol and file names come from the build, so from the PR: backticks and tildes, which
    could close the fence, are replaced, as are angle brackets, and control characters dropped.
    """
    line = line.replace("`", "'").replace("~", "-").replace("<", "(").replace(">", ")")
    return "".join(character for character in line if character.isprintable())[:200]


def main():
    parser = argparse.ArgumentParser(description="Flash usage of an ESPHome build by archive and file.")
    parser.add_argument("--name", required=True)
    parser.add_argument("--search-dir", required=True)
    parser.add_argument("--summary", required=True)
    parser.add_argument("--rows", type=int, default=30)
    args = parser.parse_args()

    map_path = find_map(args.search_dir)
    lines = [f"### Flash breakdown: {args.name}", ""]
    if map_path is None:
        lines.append("No linker map found.")
    else:
        print(f"Linker map: {map_path}")
        for option, title in (("--archives", "By archive"), ("--files", "By file")):
            table = run_size(map_path, option)
            if table is None:
                lines += [f"{title}: not available.", ""]
                continue
            rows = table.rstrip("\n").splitlines()
            # Heading lines of the table (title, column names, rule) plus the largest rows
            shown = [sanitize(row) for row in rows[: args.rows + 6]]
            lines += [f"<details><summary>{title} (largest {args.rows})</summary>", "", "```text", *shown, "```",
                      "", "</details>", ""]
    with open(args.summary, "a", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
