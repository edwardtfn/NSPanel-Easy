#!/usr/bin/env python3
"""Break the flash usage of an ESPHome build down by archive, file and symbol.

Run in the build job, right after the build: finds the build's linker map and
appends to the job summary the largest archives (libraries, ESPHome components),
the largest object files and the largest symbols of libsrc.a (where ESPHome,
this component and the code generated from the YAML lambdas are linked), as
reported by Espressif's esp-idf-size (pinned in
.github/requirements-esp-idf-size.txt). It shows where flash goes, so size work
targets the real contributors.

esp-idf-size is run with its CSV output, and the tables are written as Markdown
with the columns that matter here (total, flash code, flash data, DRAM), so the
size columns never get cut off by a long symbol name. Symbol names are
demangled with c++filt when it is available.

Informational only: it never fails the job, also when the map cannot be found
or read (e.g. builds whose map esp-idf-size does not understand). The tables
come from the build, so from the PR: every name is sanitized before it is
written to the summary.

Usage:
  esphome_size_breakdown.py --name NAME --search-dir DIR --summary FILE [--rows N]

  --name NAME        build name, for the summary heading
  --search-dir DIR   directory holding the build output (the YAML file's directory)
  --summary FILE     file to append the Markdown to (e.g. $GITHUB_STEP_SUMMARY)
  --rows N           rows kept from each table (default 30)
"""

import argparse
import csv
import io
import os
import shutil
import subprocess
import sys
from pathlib import Path

TIMEOUT_S = 300
COLUMNS = ("Total Size", "Flash Code", "Flash Data", "DRAM")
NAME_LENGTH = 110


def find_map(search_dir):
    """The newest linker map under search_dir, or None."""
    maps = [path for path in Path(search_dir).rglob("*.map") if path.is_file() and path.stat().st_size > 0]
    return max(maps, key=lambda path: path.stat().st_mtime) if maps else None


def run(command, stdin=None):
    """Output of a command, or None when it fails."""
    env = dict(os.environ, COLUMNS="160", NO_COLOR="1", TERM="dumb")
    try:
        result = subprocess.run(  # nosemgrep: python.lang.security.audit.dangerous-subprocess-use-audit
            command, input=stdin, capture_output=True, text=True, timeout=TIMEOUT_S, env=env, check=False)
    except (OSError, subprocess.TimeoutExpired) as error:
        print(f"{command[0]} failed: {error}")
        return None
    if result.returncode != 0 or not result.stdout.strip():
        print(f"{' '.join(command[:4])} failed ({result.returncode}): {result.stderr.strip()[:500]}")
        return None
    return result.stdout


def size_rows(map_path, options):
    """(header, rows) of an esp-idf-size table as CSV, or None when it fails."""
    # A fixed argument list, no shell: the map path is a file found by the script, passed as a single argument
    output = run([sys.executable, "-m", "esp_idf_size", "--format", "csv", *options, str(map_path)])
    if output is None:
        return None
    table = [row for row in csv.reader(io.StringIO(output)) if row]
    if len(table) < 2:
        return None
    return table[0], table[1:]


def demangle(names):
    """Names with C++ symbols demangled by c++filt, or the names unchanged."""
    if not names or shutil.which("c++filt") is None:
        return names
    output = run(["c++filt"], stdin="\n".join(names) + "\n")
    demangled = output.splitlines() if output else []
    return demangled if len(demangled) == len(names) else names


def sanitize(text):
    """A cell that cannot break the Markdown table nor carry Markdown or HTML out of it.

    Symbol and file names come from the build, so from the PR: characters with a meaning in
    Markdown or HTML are replaced, control characters dropped and the length capped.
    """
    for character, replacement in (("|", "/"), ("`", "'"), ("<", "("), (">", ")"), ("[", "("), ("]", ")"),
                                   ("*", "x"), ("~", "-"), ("\\", "/")):
        text = text.replace(character, replacement)
    text = "".join(character for character in text if character.isprintable())
    return text if len(text) <= NAME_LENGTH else text[: NAME_LENGTH - 3] + "..."


def number(text):
    """A size cell as an integer, or 0."""
    try:
        return int(text.replace(",", "").strip())
    except ValueError:
        return 0


def markdown(header, rows, limit, demangle_names):
    """The largest rows of a table as Markdown lines."""
    indexes = [header.index(column) for column in COLUMNS if column in header]
    shown = sorted(rows, key=lambda row: number(row[indexes[0]]) if indexes else 0, reverse=True)[:limit]
    # esp-idf-size marks functions with a trailing "()", which c++filt would keep after the demangled name
    names = [row[0][:-2] if row[0].endswith("()") else row[0] for row in shown]
    if demangle_names:
        names = demangle(names)
    lines = ["| " + " | ".join([header[0], *[header[index] for index in indexes]]) + " |",
             "|" + "---|" + "---:|" * len(indexes)]
    for name, row in zip(names, shown):
        cells = [f"{number(row[index]):,}" for index in indexes]
        lines.append("| " + " | ".join([sanitize(name), *cells]) + " |")
    return lines


def main():
    parser = argparse.ArgumentParser(description="Flash usage of an ESPHome build by archive, file and symbol.")
    parser.add_argument("--name", required=True)
    parser.add_argument("--search-dir", required=True)
    parser.add_argument("--summary", required=True)
    parser.add_argument("--rows", type=int, default=30)
    args = parser.parse_args()

    map_path = find_map(args.search_dir)
    lines = [f"### Flash breakdown: {sanitize(args.name)}", ""]
    if map_path is None:
        lines.append("No linker map found.")
    else:
        print(f"Linker map: {map_path}")
        tables = ((("--archives",), "By archive", False), (("--files",), "By file", False),
                  (("--archive-details", "libsrc.a"),
                   "By symbol in libsrc.a (ESPHome, this component and the YAML lambdas)", True))
        for options, title, symbols in tables:
            table = size_rows(map_path, options)
            if table is None:
                lines += [f"{title}: not available.", ""]
                continue
            header, rows = table
            lines += [f"<details><summary>{title} (largest {args.rows})</summary>", "",
                      *markdown(header, rows, args.rows, symbols), "", "</details>", ""]
    with open(args.summary, "a", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
