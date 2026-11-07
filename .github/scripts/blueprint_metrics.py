#!/usr/bin/env python3
"""
Blueprint metrics for NSPanel Easy.

Measures the Blueprint as Home Assistant sees it once loaded: YAML anchors are
expanded, so every anchored block counts once per place it is used. This is
what each panel's automation holds in memory and what tools that inspect
automations (e.g. Spook) go through.

Reported:
  - File size, lines and number of inputs.
  - Expanded size (JSON) of each top-level automation section.
  - Templates: occurrences, distinct strings, the largest ones and the most
    repeated ones (by the characters their repeats add).
  - The largest `choose` branches, by expanded size.
  - The largest subtrees that appear more than once in the expanded config
    (usually an anchor used in several places).

Usage:
  blueprint_metrics.py [BLUEPRINT] [--base BASE_BLUEPRINT] [--markdown] [--output FILE]
                       [--top N] [--warn-distinct N] [--max-distinct N]

  BLUEPRINT        Blueprint file (default: nspanel_easy_blueprint.yaml)
  --base           Another version of the Blueprint to compare against
                   (e.g. the target branch of a pull request)
  --markdown       Markdown output, e.g. for $GITHUB_STEP_SUMMARY
  --output FILE    Append the report to FILE instead of printing it, so that
                   warnings on stdout stay separate (e.g. $GITHUB_STEP_SUMMARY)
  --top N          Rows per table (default: 15)
  --warn-distinct  Print a GitHub Actions warning when the number of distinct
                   templates reaches N (default: 1024, Spook's template cache
                   size; 0 disables it)
  --max-distinct   Fail (exit code 1, with a GitHub Actions error) when the
                   number of distinct templates is above N, keeping a margin
                   below Spook's template cache (default: 0, disabled)

With --base, a warning is also printed when the number of distinct templates
grows compared with the base.

The exit code is 0 unless --max-distinct is exceeded: the other metrics
inform, they don't block. The report is written before failing.
"""

import argparse
import collections
import hashlib
import json
import sys

import yaml

# Top-level keys of an automation Blueprint, in the order they are reported
AUTOMATION_SECTIONS = ("trigger_variables", "variables", "triggers", "conditions", "actions")

# Subtrees smaller than this (expanded JSON characters) are not reported as duplicates
DUPLICATE_MIN_SIZE = 2000

# Upper bound for the number of nodes once anchors are expanded. Aliases can nest so that the
# expanded size grows exponentially; above this bound no metrics are computed (the Blueprint
# has about 200,000).
MAX_EXPANDED_NODES = 5_000_000


class BlueprintLoader(yaml.SafeLoader):
    """SafeLoader that accepts Home Assistant tags (e.g. `!input`), keeping them as text."""


def _construct_tag(loader, tag_suffix, node):
    """Keep a tagged scalar as `!tag value`, so it still counts in sizes without being resolved."""
    if isinstance(node, yaml.ScalarNode):
        return f"!{tag_suffix} {loader.construct_scalar(node)}"
    if isinstance(node, yaml.SequenceNode):
        return loader.construct_sequence(node, deep=True)
    return loader.construct_mapping(node, deep=True)


BlueprintLoader.add_multi_constructor("!", _construct_tag)


def load_blueprint(path):
    """Load a Blueprint file, returning its raw text and its parsed (anchor-expanded) content."""
    with open(path, encoding="utf-8") as f:
        text = f.read()
    return text, yaml.load(text, Loader=BlueprintLoader)  # nosec B506 - SafeLoader subclass


def expanded_node_count(node, memo=None):
    """
    Number of nodes once every anchor is expanded, computed without expanding them.

    Aliases are the same Python object, so each distinct object is counted once and its
    count reused, keeping this linear in the size of the file.
    """
    memo = {} if memo is None else memo
    if id(node) in memo:
        return memo[id(node)]
    if isinstance(node, dict):
        total = 1 + sum(expanded_node_count(value, memo) for value in node.values())
    elif isinstance(node, list):
        total = 1 + sum(expanded_node_count(value, memo) for value in node)
    else:
        total = 1
    if isinstance(node, (dict, list)):
        memo[id(node)] = total
    return total


def expanded_size(node):
    """Size of a node as compact JSON, i.e. with every anchor expanded."""
    return len(json.dumps(node, separators=(",", ":"), ensure_ascii=False, default=str))


def is_template(value):
    """Whether a string holds a Jinja template."""
    return isinstance(value, str) and ("{{" in value or "{%" in value)


def collect_templates(node, found):
    """Append every template string under node to found (one entry per occurrence)."""
    if isinstance(node, dict):
        for value in node.values():
            collect_templates(value, found)
    elif isinstance(node, list):
        for value in node:
            collect_templates(value, found)
    elif is_template(node):
        found.append(node)


def count_inputs(inputs):
    """Number of inputs, counting those inside input sections."""
    total = 0
    for value in (inputs or {}).values():
        if isinstance(value, dict) and isinstance(value.get("input"), dict):
            total += len(value["input"])
        else:
            total += 1
    return total


def choose_branches(node, path, rows):
    """Collect every `choose` option as (path, alias, expanded size, templates, distinct templates)."""
    if isinstance(node, dict):
        if isinstance(node.get("choose"), list):
            for index, option in enumerate(node["choose"]):
                alias = option.get("alias") if isinstance(option, dict) else None
                templates = []
                collect_templates(option, templates)
                name = alias or f"(option {index + 1}, no alias)"
                rows.append((f"{path} > {name}" if path else name, expanded_size(option), len(templates),
                             len(set(templates))))
        alias = node.get("alias")
        child_path = f"{path} > {alias}" if alias and path else (alias or path)
        for value in node.values():
            choose_branches(value, child_path, rows)
    elif isinstance(node, list):
        for value in node:
            choose_branches(value, path, rows)


def duplicated_subtrees(node, top):
    """
    Find the largest subtrees that appear more than once.

    node maps each automation section to its content, so every section is scanned.
    Only maximal duplicates are reported: once a subtree is a duplicate, the
    subtrees inside it are not reported again.
    """
    # Size and digest per object, computed once: an aliased subtree is the same object in every place
    # it is used, and both passes visit it, so serializing it again for each visit would be wasted work.
    # Counts stay per occurrence, as each visit still counts.
    sizes = {}
    digests = {}

    def size(item):
        if id(item) not in sizes:
            sizes[id(item)] = expanded_size(item)
        return sizes[id(item)]

    def digest(item):  # Key order kept as loaded: anchor copies keep the same order
        if id(item) not in digests:
            digests[id(item)] = hashlib.sha1(json.dumps(item, default=str).encode(),
                                             usedforsecurity=False).hexdigest()
        return digests[id(item)]

    counts = collections.Counter()

    def count(item):
        if not isinstance(item, (dict, list)):
            return
        if size(item) >= DUPLICATE_MIN_SIZE:
            counts[digest(item)] += 1
        for child in (item.values() if isinstance(item, dict) else item):
            count(child)

    found = {}  # digest -> (copies, size, label)

    def collect(item, label):
        if not isinstance(item, (dict, list)):
            return
        if size(item) >= DUPLICATE_MIN_SIZE:
            key = digest(item)
            if counts[key] > 1:
                found.setdefault(key, (counts[key], size(item), label))
                return  # Maximal duplicate: don't report what's inside it
        child_label = item.get("alias", label) if isinstance(item, dict) else label
        for child in (item.values() if isinstance(item, dict) else item):
            collect(child, child_label)

    count(node)
    for section, value in node.items():
        collect(value, section)
    return sorted(found.values(), key=lambda entry: -(entry[0] - 1) * entry[1])[:top]


def measure(path, top):
    """Compute all metrics for one Blueprint file."""
    text, data = load_blueprint(path)
    sections = {key: data.get(key) for key in AUTOMATION_SECTIONS if key in data}
    nodes = expanded_node_count(sections)
    if nodes > MAX_EXPANDED_NODES:
        raise ValueError(f"{path}: {nodes:,} nodes once anchors are expanded, above the limit of "
                         f"{MAX_EXPANDED_NODES:,}; metrics skipped")
    templates = []
    collect_templates(sections, templates)
    counter = collections.Counter(templates)
    branches = []
    choose_branches(sections.get("actions"), "", branches)
    branches.sort(key=lambda row: -row[1])
    return {
        "file_bytes": len(text.encode("utf-8")),
        "lines": text.count("\n"),
        "inputs": count_inputs(data.get("blueprint", {}).get("input")),
        "section_sizes": {key: expanded_size(value) for key, value in sections.items()},
        "expanded_total": sum(expanded_size(value) for value in sections.values()),
        "templates": len(templates),
        "templates_distinct": len(counter),
        "template_chars_repeated": sum(len(t) * (n - 1) for t, n in counter.items()),
        "largest_templates": sorted(counter, key=len, reverse=True)[:top],
        "most_repeated": sorted(counter.items(), key=lambda item: -len(item[0]) * (item[1] - 1))[:top],
        "branches": branches[:top],
        "duplicates": duplicated_subtrees(sections, top),
    }


def delta(current, base, key):
    """Format a value with its change against the base, when there is one."""
    value = current[key]
    if base is None:
        return f"{value:,}"
    diff = value - base[key]
    return f"{value:,} ({diff:+,})" if diff else f"{value:,} (=)"


def markdown_escape(text):
    """
    Escape a value from the Blueprint for a Markdown table cell.

    Values come from pull requests, so whitespace (including newlines) is collapsed and every
    character with a meaning in Markdown or HTML is escaped: nothing in a cell can start a
    link, an image, HTML or a new table row.
    """
    flat = " ".join(str(text).split())
    return "".join("\\" + char if char in "\\`*_{}[]()<>#+-.!|~&" else char for char in flat)


def preview(template, width=90):
    """One-line preview of a template."""
    flat = " ".join(template.split())
    return flat if len(flat) <= width else flat[:width - 3] + "..."


def report(current, base, markdown):
    """Return the metrics report, as Markdown or plain text."""
    out = []

    def table(headers, rows):
        if markdown:
            out.append("| " + " | ".join(headers) + " |")
            out.append("|" + "|".join("---" for _ in headers) + "|")
            for row in rows:
                out.append("| " + " | ".join(markdown_escape(cell) for cell in row) + " |")
        else:
            for row in rows:
                out.append("  " + "  ".join(str(cell) for cell in row))
        out.append("")

    def heading(title):
        out.append(f"### {title}" if markdown else f"== {title} ==")
        out.append("")

    heading("Blueprint metrics" + (" (change against the base)" if base else ""))
    table(["Metric", "Value"], [
        ["File size (bytes)", delta(current, base, "file_bytes")],
        ["Lines", delta(current, base, "lines")],
        ["Inputs", delta(current, base, "inputs")],
        ["Expanded size, all sections (chars)", delta(current, base, "expanded_total")],
        ["Templates (occurrences)", delta(current, base, "templates")],
        ["Templates (distinct)", delta(current, base, "templates_distinct")],
        ["Template chars added by repeats", delta(current, base, "template_chars_repeated")],
    ])

    heading("Expanded size per section")
    table(["Section", "Chars"], [[key, f"{size:,}"] for key, size in current["section_sizes"].items()])

    heading("Largest choose branches (expanded)")
    table(["Branch", "Chars", "Templates", "Distinct"],
          [[path, f"{size:,}", count, distinct] for path, size, count, distinct in current["branches"]])

    heading("Largest repeated subtrees (usually anchors used in several places)")
    table(["First seen in", "Copies", "Chars each", "Chars added by copies"],
          [[label, count, f"{size:,}", f"{(count - 1) * size:,}"] for count, size, label in current["duplicates"]])

    heading("Most repeated templates (by chars added)")
    table(["Copies", "Chars", "Template"],
          [[count, len(t), preview(t)] for t, count in current["most_repeated"]])

    heading("Largest templates")
    table(["Chars", "Template"],
          [[len(t), preview(t)] for t in current["largest_templates"]])

    return "\n".join(out)


def main():
    parser = argparse.ArgumentParser(description="Blueprint metrics for NSPanel Easy.")
    parser.add_argument("blueprint", nargs="?", default="nspanel_easy_blueprint.yaml")
    parser.add_argument("--base", help="another version of the Blueprint to compare against")
    parser.add_argument("--markdown", action="store_true", help="Markdown output")
    parser.add_argument("--output", help="append the report to this file instead of printing it")
    parser.add_argument("--top", type=int, default=15, help="rows per table")
    parser.add_argument("--warn-distinct", type=int, default=1024,
                        help="warn when distinct templates reach this number (0 disables)")
    parser.add_argument("--max-distinct", type=int, default=0,
                        help="fail when distinct templates are above this number (0 disables)")
    args = parser.parse_args()

    try:
        current = measure(args.blueprint, args.top)
        base = measure(args.base, args.top) if args.base else None
    except ValueError as error:
        print(f"::warning file={args.blueprint}::{error}")
        return 0
    text = report(current, base, args.markdown)
    if args.output:
        with open(args.output, "a", encoding="utf-8") as f:
            f.write(text + "\n")
    else:
        print(text)

    # Workflow command for GitHub Actions; elsewhere it is just a readable line
    if args.warn_distinct and current["templates_distinct"] >= args.warn_distinct:
        print(f"::warning file={args.blueprint}::{current['templates_distinct']:,} distinct templates, at or above "
              f"{args.warn_distinct:,} (Spook's template cache size): inspections re-parse every template")
    if base and current["templates_distinct"] > base["templates_distinct"]:
        print(f"::warning file={args.blueprint}::Distinct templates grow from {base['templates_distinct']:,} to "
              f"{current['templates_distinct']:,}: the margin below Spook's template cache (1,024) gets smaller")
    if args.max_distinct and current["templates_distinct"] > args.max_distinct:
        print(f"::error file={args.blueprint}::{current['templates_distinct']:,} distinct templates, above the "
              f"limit of {args.max_distinct:,} (margin below Spook's template cache of 1,024, issue #636): "
              f"reduce the templates, or raise --max-distinct in validate_blueprint.yml as a deliberate decision")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
