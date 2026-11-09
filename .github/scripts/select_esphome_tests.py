#!/usr/bin/env python3
"""Select the ESPHome build tests affected by a set of changed files.

Each test is an ESPHome YAML file. Its ``!include`` graph is resolved
recursively and a test is selected when any changed file is part of it.
Changes that cannot be attributed to specific tests (C++, Python, workflow,
tooling, secrets) select every test, as they may affect any build.

Inputs (environment variables):
    CHANGED_FILES: Whitespace-separated list of changed paths, relative to the
                   repository root. Ignored when ``SELECT_ALL`` is ``true``.
    SELECT_ALL:    ``true`` to select every test (schedule, workflow_dispatch).

Outputs (appended to ``$GITHUB_OUTPUT`` when set, printed otherwise):
    matrix:          JSON list of ``{"name", "yaml-file"}`` for the regular tests.
    prebuilt_matrix: JSON list of ``{"name", "yaml-file"}`` for pre-built tests.

A Markdown summary is appended to ``$GITHUB_STEP_SUMMARY`` when set.
"""

import json
import os
import re
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]

# Regular tests, ordered by the LPT heuristic (longest build first).
TESTS = [
    {"name": "Customizations", "yaml-file": ".test/esphome_idf_climate_heat_customizations.yaml"},
    {"name": "Climate Cool + Bluetooth Proxy", "yaml-file": ".test/esphome_idf_climate_cool_bluetooth_proxy.yaml"},
    {"name": "Bluetooth Proxy", "yaml-file": ".test/esphome_idf_bluetooth_proxy.yaml"},
    {"name": "ESPectre", "yaml-file": ".test/esphome_idf_espectre.yaml"},
    {"name": "BLE Tracker", "yaml-file": ".test/esphome_idf_ble_tracker.yaml"},
    {"name": "Basic (Arduino)", "yaml-file": ".test/esphome_ard_basic.yaml"},
    {"name": "Display as a Light", "yaml-file": ".test/esphome_idf_display_as_light.yaml"},
    {"name": "Basic (IDF)", "yaml-file": ".test/esphome_idf_basic.yaml"},
    {"name": "Climate Cool", "yaml-file": ".test/esphome_idf_climate_cool.yaml"},
    {"name": "Climate Heat + Backup", "yaml-file": ".test/esphome_idf_climate_heat_backup.yaml"},
    {"name": "Climate Heat", "yaml-file": ".test/esphome_idf_climate_heat.yaml"},
    {"name": "Climate Dual", "yaml-file": ".test/esphome_idf_climate_dual.yaml"},
    {"name": "Climate PID Single", "yaml-file": ".test/esphome_idf_climate_pid_single.yaml"},
    {"name": "Climate PID Dual", "yaml-file": ".test/esphome_idf_climate_pid_dual.yaml"},
    {"name": "Cover", "yaml-file": ".test/esphome_idf_cover.yaml"},
    {"name": "Core", "yaml-file": ".test/esphome_idf_core.yaml"},
    {"name": "Standard", "yaml-file": ".test/esphome_idf_standard.yaml"},
]

# Pre-built firmware tests.
PREBUILT_TESTS = [
    {"name": "NSPanel Easy", "yaml-file": "prebuilt/nspanel_esphome_prebuilt.yaml"},
    {"name": "Wall Display", "yaml-file": "prebuilt/wall_display.yaml"},
]

# Matches "!include <path>" on a line that is not commented out.
INCLUDE_RE = re.compile(r"^[^#]*?!include\s+['\"]?([^\s'\"#]+)")

# Directories (or root-level file prefix) holding YAML that is only ever
# reached through "!include". A file there outside every test graph is
# untested: running all tests would not cover it, so it only raises a warning.
INCLUDE_ONLY_DIRS = ("esphome/", "prebuilt/", ".test/")
INCLUDE_ONLY_ROOT_PREFIX = "nspanel_esphome"

# Files that are not part of any "!include" graph but affect every test.
SELECT_ALL_FILES = {".test/secrets.yaml"}


def resolve_includes(entry: str) -> set[str]:
    """Return the repo-relative paths in the ``!include`` closure of ``entry``.

    The closure contains ``entry`` itself. Missing files (e.g. deleted in the
    change) are kept in the closure but not followed.
    """
    closure: set[str] = set()
    pending = [entry]
    while pending:
        rel_path = pending.pop()
        if rel_path in closure:
            continue
        closure.add(rel_path)
        abs_path = REPO_ROOT / rel_path
        if not abs_path.is_file():
            continue
        for line in abs_path.read_text(encoding="utf-8").splitlines():
            match = INCLUDE_RE.match(line)
            if match is None:
                continue
            target = os.path.normpath(abs_path.parent / match.group(1))
            pending.append(Path(target).relative_to(REPO_ROOT).as_posix())
    return closure


def is_include_only_yaml(path: str) -> bool:
    """Return whether ``path`` is project YAML only reachable via ``!include``."""
    if not path.endswith(".yaml") or path in SELECT_ALL_FILES:
        return False
    if path.startswith(INCLUDE_ONLY_DIRS):
        return True
    return "/" not in path and path.startswith(INCLUDE_ONLY_ROOT_PREFIX)


def select(tests: list[dict], graphs: dict[str, set[str]], changed: set[str], select_all: bool) -> list[dict]:
    """Return the tests (in their original order) affected by ``changed``."""
    if select_all:
        return list(tests)
    return [test for test in tests if graphs[test["yaml-file"]] & changed]


def write_output(name: str, value: str) -> None:
    """Write a step output to ``$GITHUB_OUTPUT``, or print it when unset."""
    output_file = os.environ.get("GITHUB_OUTPUT")
    if output_file:
        with open(output_file, "a", encoding="utf-8") as handle:
            handle.write(f"{name}={value}\n")
    else:
        print(f"{name}={value}")


def write_summary(reason: str, selected: list[dict], prebuilt: list[dict]) -> None:
    """Append the selection to ``$GITHUB_STEP_SUMMARY``, or print it when unset."""
    lines = ["## ESPHome test selection", "", reason, ""]
    total = len(TESTS) + len(PREBUILT_TESTS)
    lines.append(f"Selected {len(selected) + len(prebuilt)} of {total} tests:")
    lines.append("")
    for test in selected + prebuilt:
        lines.append(f"- {test['name']} (`{test['yaml-file']}`)")
    summary = "\n".join(lines) + "\n"
    summary_file = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary_file:
        with open(summary_file, "a", encoding="utf-8") as handle:
            handle.write(summary)
    else:
        print(summary)


def main() -> None:
    """Compute and publish the test selection."""
    changed = set(os.environ.get("CHANGED_FILES", "").split())
    select_all = os.environ.get("SELECT_ALL", "false").lower() == "true"

    graphs = {test["yaml-file"]: resolve_includes(test["yaml-file"]) for test in TESTS + PREBUILT_TESTS}
    tested_files = set().union(*graphs.values())

    if select_all:
        reason = "Full run (not a pull request)."
    else:
        unattributed = sorted(path for path in changed if path not in tested_files)
        global_changes = [path for path in unattributed if not is_include_only_yaml(path)]
        for path in unattributed:
            if is_include_only_yaml(path):
                print(f"::warning file={path}::YAML file is not part of any ESPHome build test")
        if global_changes:
            select_all = True
            reason = "Full run, changes not limited to specific tests: " + ", ".join(
                f"`{path}`" for path in global_changes
            )
        else:
            reason = "Partial run, based on the `!include` graph of each test."

    selected = select(TESTS, graphs, changed, select_all)
    prebuilt = select(PREBUILT_TESTS, graphs, changed, select_all)

    write_output("matrix", json.dumps(selected, separators=(",", ":")))
    write_output("prebuilt_matrix", json.dumps(prebuilt, separators=(",", ":")))
    write_summary(reason, selected, prebuilt)


if __name__ == "__main__":
    main()
