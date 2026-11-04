#!/usr/bin/env python3
"""
build_mdi_fonts.py
==================
Builds a MaterialDesignIconsDesktop TTF from MDI SVG sources and generates
derived artifacts for use with NSPanel Easy / Nextion ZI font generation.

Usage
-----
  python3 build_mdi_fonts.py [--svg-dir DIR] [--output-dir DIR] [--dry-run]

Prerequisites
-------------
  pip3 install fonttools cu2qu --break-system-packages

Input (expected in --svg-dir, default: current directory)
----------------------------------------------------------
  meta.json          MDI icon metadata  (from Templarian/MaterialDesign-SVG)
  svg/<n>.svg        MDI SVG sources    (from Templarian/MaterialDesign-SVG)

Outputs (written to --output-dir, default: ./output)
-----------------------------------------------------
  MaterialDesignIconsDesktop-<ver>.ttf
      Drop-in replacement for the joBr99 bundled TTF.
      Feed this into Generate-HASP-Fonts.ps1 to regenerate Nextion .zi files.

  all_icons.json
      Single-line name-to-ZI-codepoint lookup table, same format as
      HASwitchPlate_table___all_icons__ used by the NSPanel Easy Blueprint.
      Includes special entries: blank, void, unknown, unavailable.

  blueprint_icons.yaml
      The Blueprint's icon lookup: all_icon_names (one string with every icon
      name in codepoint order, so an icon's ZI codepoint is U+E000 + its
      position) and special_icons (blank, void, unknown, unavailable).
      A single string keeps the automation configuration small: tools that
      walk every string of every automation (e.g. Spook repairs) slow Home
      Assistant down with a 7,000-entry dictionary per panel (issue #636).
      Use --blueprint to replace these lines in the Blueprint directly.

  cheatsheet.html
      Searchable visual icon cheatsheet for GitHub Pages (docs/icons/).
      Click-to-copy copies "mdi:<n>" to clipboard.
      Shows ZI codepoints (post-ZiLib remapping) for each icon.

  all_icons.h
      C++ header with all icon ZI codepoints as constexpr constants,
      following ESPHome/NSPanel Easy coding conventions (clang-format
      aligned, Doxygen ///<, esphome::nspanel_easy::Icons namespace).

Codepoint scheme
----------------
  MDI SVG sources assign each icon a TTF codepoint in the F0xxx range
  (Supplementary PUA-B). ZiLib remaps these when writing .zi files using:

      zi_codepoint = ttf_codepoint - ZI_CP_OFFSET   (ZI_CP_OFFSET = 0xE2001)

  All artifacts in this script use ZI codepoints — these are the actual
  values sent over UART to the Nextion display and used in firmware/Blueprint.

Notes
-----
  - MDI SVGs use cubic Bezier curves; TTF requires quadratic. cu2qu handles
    the conversion automatically.
  - SVG paths that omit the leading moveTo command are auto-corrected by
    prepending an implicit "M 0,0" (mirrors browser behaviour; see SVG spec
    section 9.3.3).
  - 2 glyphs (ab-testing, google-ads) render blank due to upstream SVG defects
    that cannot be safely auto-corrected; both are in allowed_blank_glyphs.
  - Run this script whenever MDI releases new icons to keep all artifacts
    in sync. The TTF filename encodes the MDI version for traceability.
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path


# =============================================================================
# Configuration
# =============================================================================

UNITS_PER_EM = 2048     # Matches original MaterialDesignIconsDesktop.ttf
SVG_SIZE     = 24.0     # MDI viewBox is always 0 0 24 24
SCALE        = UNITS_PER_EM / SVG_SIZE

# -----------------------------------------------------------------------------
# Vertical metrics mirrored from the official @mdi/font webfont (UPM=512),
# scaled to our UPM=2048 (factor = 4).  These give MDI glyphs the same
# ~12.5% descender space the font is designed for, preventing bottom
# clipping of glyphs whose outlines extend below the SVG viewBox baseline
# (the vast majority — 4772 of the ~7600 icons in 7.4.47 have yMin < 0
# in the reference build).
#
# Reference values @ UPM=512   ->   Scaled values @ UPM=2048
#   ascent          = 448                1792
#   descent         = -64                -256
#   usWinAscent     = 453                1812
#   usWinDescent    = 66                 264
#   sTypoLineGap    = 46                 184
# -----------------------------------------------------------------------------
ASCENT         = 1792   # sTypoAscender / hhea.ascent
DESCENT        = -256   # sTypoDescender / hhea.descent  (negative)
WIN_ASCENT     = 1812   # usWinAscent  (includes a small safety margin vs ASCENT)
WIN_DESCENT    = 264    # usWinDescent (positive; includes safety margin vs |DESCENT|)
TYPO_LINE_GAP  = 184    # sTypoLineGap

# ZiLib codepoint remapping offset, as used in Generate-HASP-Fonts.ps1:
#   zi_codepoint = ttf_codepoint - ZI_CP_OFFSET
ZI_CP_OFFSET = 0xE2001

# Special entries prepended to all_icons.json.
# Values are ZI codepoints. unknown/unavailable resolve dynamically to
# whatever codepoint alert-circle has in the current MDI version.
SPECIAL_ICONS = {
    "blank":       0xFFFF,
    "void":        0xFFFF,
    "unknown":     None,  # resolved to alert-circle at runtime
    "unavailable": None,  # resolved to alert-circle at runtime
}
ALERT_CIRCLE_NAME = "alert-circle"


# =============================================================================
# Argument parsing
# =============================================================================

def parse_args():
    parser = argparse.ArgumentParser(
        description="Build MDI Desktop TTF and derived artifacts for NSPanel Easy."
    )
    parser.add_argument(
        "--svg-dir", default=".",
        help="Directory containing meta.json and svg/ folder (default: current dir)"
    )
    parser.add_argument(
        "--output-dir", default="./output",
        help="Directory to write all output files (default: ./output)"
    )
    parser.add_argument(
        "--docs-dir", default=None,
        help="If set, also copy cheatsheet.html, TTF, and all_icons.json here "
             "(e.g. path/to/NSPanel-Easy/docs/icons)"
    )
    parser.add_argument(
        "--header-dir", default=None,
        help="If set, also copy all_icons.h here "
             "(e.g. path/to/NSPanel-Easy/components/nspanel_easy)"
    )
    parser.add_argument(
        "--blueprint", default=None,
        help="If set, replace the all_icon_names and special_icons lines in this "
             "Blueprint file (e.g. path/to/NSPanel-Easy/nspanel_easy_blueprint.yaml)"
    )
    parser.add_argument(
        "--dry-run", action="store_true",
        help="Parse inputs and report counts without writing any files"
    )
    return parser.parse_args()


# =============================================================================
# Version detection
# =============================================================================

def detect_version(svg_dir):
    """
    Detect MDI version from package.json or font-build.json in the SVG repo.
    font-build.json stores version as {"major": N, "minor": N, "patch": N}.
    package.json stores version as a plain string.
    """
    for fname in ("package.json", "font-build.json"):
        p = svg_dir / fname
        if not p.exists():
            continue
        with open(p) as f:
            pkg = json.load(f)
        v = pkg.get("version")
        if isinstance(v, str):
            return v
        if isinstance(v, dict) and all(k in v for k in ("major", "minor", "patch")):
            return f"{v['major']}.{v['minor']}.{v['patch']}"
    return "unknown"


# =============================================================================
# TTF build
# =============================================================================

def _fix_svg_path_data(d):
    """
    Ensure an SVG path data string starts with a moveTo command.

    The SVG spec (section 9.3.3) requires that path data begin with a moveTo
    ('M' or 'm').  Some MDI sources (e.g. google-ads) violate this, causing
    strict parsers such as fontTools' parse_path to raise "moveTo is required".
    Browsers silently recover by treating a missing leading moveTo as "M 0,0".
    This helper applies the same recovery so that the glyph renders correctly
    instead of being discarded as blank.
    """
    stripped = d.lstrip()
    if stripped and stripped[0] not in ("M", "m"):
        return "M 0,0 " + d
    return d


def build_ttf(meta, svg_dir, output_path, version_str):
    """
    Build a TTF from MDI SVG sources using fonttools + cu2qu.
    Glyphs are mapped to their F0xxx TTF codepoints (not ZI codepoints) —
    ZiLib performs the remapping to ZI codepoints when generating .zi files.
    Returns name_to_ttf_cp dict for use by artifact generators.
    """
    try:
        from fontTools.fontBuilder import FontBuilder
        from fontTools.pens.ttGlyphPen import TTGlyphPen
        from fontTools.pens.transformPen import TransformPen
        from fontTools.svgLib.path import parse_path
        from cu2qu.pens import Cu2QuPen
    except ImportError as e:
        print(f"ERROR: Missing dependency: {e}")
        print("  Run: pip3 install fonttools cu2qu --break-system-packages")
        sys.exit(1)

    # Build icon list — only icons with a corresponding SVG file
    icons = []
    missing_svg = []
    for icon in meta:
        cp       = int(icon["codepoint"], 16)
        svg_path = os.path.join(svg_dir, "svg", f"{icon['name']}.svg")
        if os.path.exists(svg_path):
            icons.append((cp, icon["name"], svg_path))
        else:
            missing_svg.append(icon["name"])

    if missing_svg:
        print(f"  WARNING: {len(missing_svg)} icons have no SVG file and will be skipped.")

    print(f"  Building {len(icons)} glyphs...")

    fb = FontBuilder(UNITS_PER_EM, isTTF=True)
    fb.setupGlyphOrder([".notdef"] + [name for _, name, _ in icons])
    fb.setupCharacterMap({cp: name for cp, name, _ in icons})

    # Placeholder hmtx entry for .notdef so FontBuilder is satisfied during
    # the glyph-build pass below; the real per-glyph advances are filled in
    # after rendering (see the second setupHorizontalMetrics call below).
    metrics = {".notdef": (UNITS_PER_EM, 0)}
    fb.setupHorizontalMetrics(metrics)
    fb.setupHorizontalHeader(ascent=ASCENT, descent=DESCENT, lineGap=0)
    fb.setupNameTable({
        "familyName": "Material Design Icons",
        "styleName":  "Regular",
    })
    # OS/2 metrics mirror the reference build.  sxHeight/sCapHeight are
    # meaningless for an icon-only font but Windows/GDI still reads them,
    # so we set them to ASCENT (same value the reference uses at its scale).
    fb.setupOS2(
        sTypoAscender=ASCENT, sTypoDescender=DESCENT,
        sTypoLineGap=TYPO_LINE_GAP,
        usWinAscent=WIN_ASCENT, usWinDescent=WIN_DESCENT,
        sxHeight=ASCENT, sCapHeight=ASCENT,
        achVendID="MDI ",
    )
    fb.setupPost()
    fb.setupHead(unitsPerEm=UNITS_PER_EM)

    # Y-flip transform: SVG origin is top-left, TTF origin is bottom-left.
    # Mapping SVG y=0 → TTF y=ASCENT and SVG y=24 → TTF y=DESCENT gives
    # each glyph the full UPM of vertical room (ASCENT above baseline,
    # |DESCENT| below) — matching how the reference build positions
    # glyphs relative to the baseline.
    transform = (SCALE, 0, 0, -SCALE, 0, ASCENT)

    glyphs = {".notdef": TTGlyphPen(None).glyph()}
    failed = []

    for i, (cp, name, svg_path) in enumerate(icons):
        if i % 500 == 0:
            print(f"    {i}/{len(icons)}...")
        try:
            # First pass: render with the base y-flip transform to measure
            # the glyph's true horizontal bbox.
            probe_pen  = TTGlyphPen(None)
            probe_tpen = Cu2QuPen(
                TransformPen(probe_pen, transform),
                max_err=1.0,
                reverse_direction=True,
            )
            with open(svg_path) as f:
                svg_content = f.read()
            paths = re.findall(r'<path[^>]+d="([^"]+)"', svg_content)
            if paths:
                for d in paths:
                    parse_path(_fix_svg_path_data(d), probe_tpen)
            probe_glyph = probe_pen.glyph()
            probe_glyph.recalcBounds(None)

            # Shift the outline so xMin = 0.  ZiLib calls GDI+ MeasureString
            # to determine the glyph cell width and draws the outline at
            # location (0, y) inside a bitmap of that width — so the outline
            # must start at x=0 for the glyph to fit cleanly in the cell.
            # The matching advance width (= glyph width) is set in hmtx
            # below, which is what GDI+ returns from MeasureString.
            if probe_glyph.numberOfContours > 0:
                x_offset = -probe_glyph.xMin
            else:
                x_offset = 0

            # Second pass: render with the per-glyph offset applied.
            pen            = TTGlyphPen(None)
            shifted_xform  = (SCALE, 0, 0, -SCALE, x_offset, ASCENT)
            tpen           = Cu2QuPen(
                TransformPen(pen, shifted_xform),
                max_err=1.0,
                reverse_direction=True,
            )
            if paths:
                for d in paths:
                    parse_path(_fix_svg_path_data(d), tpen)
            glyphs[name] = pen.glyph()
        except Exception as e:
            glyphs[name] = TTGlyphPen(None).glyph()
            failed.append((name, str(e)))

    if failed:
        print(f"  WARNING: {len(failed)} glyphs failed (rendered as blank):")
        for name, err in failed[:5]:
            print(f"    {name}: {err}")
        if len(failed) > 5:
            print(f"    ... and {len(failed) - 5} more")

        # Prevent shipping silently-broken icon sets.
        # Known upstream SVG defects that cannot be safely auto-corrected:
        #   ab-testing  - unusual path structure produces no contours
        #   google-ads  - subpath after Z lacks a required leading M command;
        #                 the intended geometry cannot be inferred safely
        allowed_blank_glyphs = {"ab-testing", "google-ads"}
        unexpected_failed = [name for name, _ in failed if name not in allowed_blank_glyphs]
        if unexpected_failed:
            print("ERROR: Unexpected glyph failures detected; aborting artifact generation.")
            sys.exit(2)

    fb.setupGlyf(glyphs)

    # Per-glyph advance width equals each glyph's actual bounding-box width
    # (with xMin shifted to 0 above).  GDI+ MeasureString returns this
    # advance as the string width, and ZiLib uses that to size the bitmap
    # cell — so the cell ends up tight around the glyph, with no empty
    # padding on the left or right.  The Nextion Editor's horizontal-center
    # textbox alignment then centers this tight cell within the display
    # component, giving visually centered icons on the panel.
    metrics = {".notdef": (UNITS_PER_EM, 0)}
    for _, name, _ in icons:
        g = glyphs[name]
        g.recalcBounds(None)
        if g.numberOfContours > 0:
            # xMin is always 0 here (we shifted it in the render loop),
            # so the glyph width equals xMax.  Fall back to UPM for any
            # degenerate glyph that ended up empty.
            advance = max(1, g.xMax)
        else:
            advance = UNITS_PER_EM
        metrics[name] = (advance, 0)
    fb.setupHorizontalMetrics(metrics)
    fb.font.save(str(output_path))
    print(f"  Saved: {output_path}")

    return {name: cp for cp, name, _ in icons}


# =============================================================================
# Shared: ZI codepoint map
# =============================================================================

def build_zi_map(name_to_ttf_cp):
    """
    Convert TTF codepoints to ZI codepoints using the ZiLib offset formula:
        zi_codepoint = ttf_codepoint - ZI_CP_OFFSET
    Only includes icons whose ZI codepoint fits in 16 bits (U+0000-U+FFFF),
    as the Nextion .zi format stores codepoints as 16-bit values.
    """
    zi_map = {}
    skipped = []
    for name, ttf_cp in name_to_ttf_cp.items():
        zi_cp = ttf_cp - ZI_CP_OFFSET
        if 0 <= zi_cp <= 0xFFFF:
            zi_map[name] = zi_cp
        else:
            skipped.append((name, ttf_cp, zi_cp))
    if skipped:
        print(f"  WARNING: {len(skipped)} icons have ZI codepoint outside U+FFFF and are excluded:")
        for name, ttf_cp, zi_cp in skipped[:5]:
            print(f"    {name}: TTF U+{ttf_cp:05X} -> ZI {zi_cp:#x} (out of range)")
        if len(skipped) > 5:
            print(f"    ... and {len(skipped) - 5} more")
    return zi_map


# =============================================================================
# all_icons.json
# =============================================================================

def build_all_icons_json(zi_map, output_path):
    """
    Write all_icons.json — single-line Blueprint icon lookup table.
    Format matches HASwitchPlate_table___all_icons__ used by NSPanel Easy Blueprint:
        all_icons: { blank: "\\uFFFF", void: "\\uFFFF", unknown: "\\uXXXX", ... }
    Special entries (blank, void, unknown, unavailable) are prepended.
    All codepoints are ZI codepoints (post-ZiLib remapping).
    """
    # Resolve alert-circle ZI codepoint for unknown/unavailable
    alert_circle_zi = zi_map.get(ALERT_CIRCLE_NAME, 0xE027)

    entries = {}

    # Special entries first
    for special_name, zi_cp in SPECIAL_ICONS.items():
        if zi_cp is None:
            zi_cp = alert_circle_zi
        entries[special_name] = f"\\u{zi_cp:04X}"

    # All MDI icons, sorted by name
    for name, zi_cp in sorted(zi_map.items()):
        entries[name] = f"\\u{zi_cp:04X}"

    # Single-line format with unquoted keys, matching Blueprint YAML format
    pairs = ", ".join(f'{k}: "{v}"' for k, v in entries.items())
    line  = f"all_icons: {{ {pairs} }}\n"

    with open(output_path, "w", encoding="utf-8") as f:
        f.write(line)
    print(f"  Saved: {output_path}  ({len(entries)} entries)")


# =============================================================================
# blueprint_icons.yaml
# =============================================================================

# Icon names left out of the Blueprint's name string, so looking them up finds nothing,
# as before: "null" was a YAML null key in the former dictionary and is kept unresolvable on purpose.
BLUEPRINT_EXCLUDED_ICONS = {"null"}

# Placeholder for unused positions in the name string. A control character never occurs in
# an icon name, so no lookup can match it.
BLUEPRINT_EMPTY_SLOT = "\\x01"


def build_blueprint_icons(zi_map, output_path, blueprint_path=None):
    """
    Write blueprint_icons.yaml — the Blueprint's icon lookup:
        all_icon_names: ",<name for U+E000>,<name for U+E001>,...,"
        special_icons: { blank: "\\uFFFF", void: "\\uFFFF", unknown: "\\uXXXX", unavailable: "\\uXXXX" }
    An icon's ZI codepoint is U+E000 plus its position in all_icon_names, so the Blueprint
    finds it with string operations, without a dictionary.
    If blueprint_path is given, the two lines are replaced there, keeping their indentation.
    """
    # Slots always start at U+E000, as the Blueprint expects: an icon skipped for a missing SVG
    # (also the first one) only leaves its slot empty
    first = 0xE000
    last = max(zi_map.values())
    if min(zi_map.values()) < first:
        sys.exit(f"ERROR: first ZI codepoint is U+{min(zi_map.values()):04X}, the Blueprint lookup expects U+E000")
    by_cp = {}
    for name, zi_cp in zi_map.items():
        if zi_cp in by_cp:
            sys.exit(f"ERROR: U+{zi_cp:04X} is used by both {by_cp[zi_cp]} and {name}")
        by_cp[zi_cp] = name

    slots = []
    for zi_cp in range(first, last + 1):
        name = by_cp.get(zi_cp)
        slots.append(BLUEPRINT_EMPTY_SLOT if name is None or name in BLUEPRINT_EXCLUDED_ICONS else name)
    names_line = 'all_icon_names: ",' + ",".join(slots) + ',"'

    alert_circle_zi = zi_map.get(ALERT_CIRCLE_NAME, 0xE027)
    special = []
    for special_name, zi_cp in SPECIAL_ICONS.items():
        if zi_cp is None:
            zi_cp = alert_circle_zi
        special.append(f'{special_name}: "\\u{zi_cp:04X}"')
    special_line = "special_icons: { " + ", ".join(special) + " }"

    with open(output_path, "w", encoding="utf-8") as f:
        f.write(names_line + "\n" + special_line + "\n")
    print(f"  Saved: {output_path}  ({last - first + 1} positions)")

    if blueprint_path:
        content = Path(blueprint_path).read_text(encoding="utf-8")
        for key, line in (("all_icon_names", names_line), ("special_icons", special_line)):
            pattern = re.compile(rf"^(?P<ind>[ ]*){key}: .*$", re.M)
            if len(pattern.findall(content)) != 1:
                sys.exit(f"ERROR: expected exactly one '{key}:' line in {blueprint_path}")
            content = pattern.sub(lambda m, line=line: m.group("ind") + line, content)
        # Always LF, as the repository requires, also when running on Windows
        with open(blueprint_path, "w", encoding="utf-8", newline="\n") as f:
            f.write(content)
        print(f"  Updated: {blueprint_path}")


# =============================================================================
# HTML cheatsheet
# =============================================================================

def build_cheatsheet(name_to_ttf_cp, zi_map, ttf_filename, output_path, version_str):
    """
    Write searchable HTML cheatsheet for GitHub Pages.
    Glyphs are rendered using TTF codepoints (font loaded via @font-face).
    ZI codepoints (post-ZiLib remapping) are shown and noted on click.

    Click-to-copy behaviour (context-sensitive):
      - Click the glyph  -> copies the ZI Unicode character (paste into Nextion Editor)
      - Click the name   -> copies "mdi:<name>"
      - Click the code   -> copies the ZI codepoint string (e.g. "U+E1C8")
    """
    icon_count = len(zi_map)

    icon_html = []
    for name in sorted(zi_map):
        ttf_cp = name_to_ttf_cp[name]
        zi_cp  = zi_map[name]
        glyph  = chr(ttf_cp)  # Rendered glyph (TTF codepoint, required by @font-face)
        paste  = chr(zi_cp)   # Copy payload (ZI codepoint, matches Nextion .zi indexing)
        zi_str = f"U+{zi_cp:04X}"
        full   = f"mdi:{name} — {zi_str}"  # context shown in every toast
        icon_html.append(
            f'  <div class="icon">\n'
            f'    <span class="glyph" onclick="copy(this,\'{paste}\',\'glyph\',\'{full}\')">{glyph}</span>\n'
            f'    <span class="name"  onclick="copy(this,\'mdi:{name}\',\'name\',\'{full}\')">'
            f'mdi:{name}</span>\n'
            f'    <span class="cp"   onclick="copy(this,\'{zi_str}\',\'code\',\'{full}\')">'
            f'{zi_str}</span>\n'
            f'  </div>'
        )

    html = f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>NSPanel Easy — MDI Icons</title>
  <style>
    @font-face {{
      font-family: 'MDI';
      src: url('{ttf_filename}');
    }}
    *, *::before, *::after {{ box-sizing: border-box; margin: 0; padding: 0; }}
    body {{ font-family: sans-serif; background: #1e1e1e; color: #ccc; }}
    header {{
      position: sticky; top: 0; z-index: 10;
      padding: 0.8em 1.2em; background: #252526;
      border-bottom: 1px solid #3c3c3c;
      display: flex; align-items: center; gap: 0.8em; flex-wrap: wrap;
    }}
    header h1 {{ color: #fff; font-size: 1.1em; flex: 1; white-space: nowrap; }}
    header .meta {{ color: #888; font-size: 0.82em; white-space: nowrap; }}
    #search {{
      padding: 6px 10px; background: #2d2d2d; color: #fff;
      border: 1px solid #555; border-radius: 4px; font-size: 13px; width: 240px;
    }}
    #search:focus {{ outline: none; border-color: #569cd6; }}
    #count {{ color: #888; font-size: 0.82em; min-width: 70px; text-align: right; }}
    .grid {{ display: flex; flex-wrap: wrap; padding: 0.8em; gap: 5px; }}
    .icon {{
      width: 128px; padding: 10px 6px 7px; background: #2d2d2d;
      border-radius: 4px; text-align: center;
      border: 1px solid transparent; transition: border-color 0.15s;
      user-select: none;
    }}
    .icon:hover  {{ border-color: #569cd6; }}
    .icon.copied {{ border-color: #4ec94e; }}
    .glyph {{ font-family: 'MDI'; font-size: 28px; display: block; margin-bottom: 5px; color: #fff; cursor: pointer; }}
    .name  {{ word-break: break-all; color: #9cdcfe; font-size: 10px; display: block; margin-bottom: 2px; cursor: pointer; }}
    .cp    {{ color: #569cd6; font-size: 9.5px; cursor: pointer; }}
    .glyph:hover {{ color: #4ec94e; }}
    .name:hover  {{ color: #7ed4fe; }}
    .cp:hover    {{ color: #7eb4f6; }}
    .toast {{
      position: fixed; bottom: 1.2em; left: 50%; transform: translateX(-50%);
      background: #4ec94e; color: #000; padding: 5px 14px;
      border-radius: 4px; font-size: 12px; opacity: 0;
      transition: opacity 0.2s; pointer-events: none; white-space: nowrap;
    }}
    .toast.show {{ opacity: 1; }}
  </style>
</head>
<body>
<header>
  <h1>NSPanel Easy — MDI Icons</h1>
  <span class="meta">MDI v{version_str} &middot; {icon_count} icons &middot; ZI codepoints</span>
  <input type="text" id="search" placeholder="Search icons..."
         oninput="filter(this.value)" autocomplete="off">
  <span id="count">{icon_count} icons</span>
</header>
<div class="grid" id="grid">
{"".join(icon_html)}
</div>
<div class="toast" id="toast"></div>
<script>
  const allIcons = Array.from(document.querySelectorAll('.icon'));

  function filter(q) {{
    q = q.toLowerCase();
    let n = 0;
    allIcons.forEach(el => {{
      const show = el.querySelector('.name').textContent.includes(q);
      el.style.display = show ? '' : 'none';
      if (show) n++;
    }});
    document.getElementById('count').textContent = n + ' icons';
  }}

  function copy(el, text, kind, ctx) {{
    navigator.clipboard.writeText(text).then(() => {{
      const card = el.closest('.icon');
      card.classList.add('copied');
      setTimeout(() => card.classList.remove('copied'), 700);
      const t = document.getElementById('toast');
      t.textContent = 'Copied ' + kind + ': ' + text + '  (' + ctx + ')';
      t.classList.add('show');
      clearTimeout(window._tt);
      window._tt = setTimeout(() => t.classList.remove('show'), 1400);
    }});
  }}
</script>
</body>
</html>
"""
    with open(output_path, "w", encoding="utf-8") as f:
        f.write(html)
    print(f"  Saved: {output_path}")


# =============================================================================
# C++ header
# =============================================================================

def build_header(zi_map, version_str, output_path):
    """
    Write all_icons.h — constexpr ZI codepoint constants following ESPHome /
    NSPanel Easy coding conventions: clang-format aligned, Doxygen ///<,
    esphome::nspanel_easy::Icons namespace.

    The generator emits a readable first pass, then pipes the result through
    clang-format so the file on disk is byte-identical to what CI would produce.
    This keeps regeneration diffs focused on real content changes (new icons,
    codepoint shifts) rather than formatting churn.

    clang-format discovers .clang-format by walking up from --assume-filename.
    The output is initially written outside the repo tree, so we point
    --assume-filename at a virtual path inside the repo (derived from this
    script's location: hmi/dev/fonts/build_mdi_fonts.py -> repo root).
    """
    const_names = {
        name: "MDI_" + name.upper().replace("-", "_")
        for name in zi_map
    }
    if not const_names:
        raise ValueError("No icon constants generated (zi_map is empty).")

    lines = [
        "// all_icons.h",
        f"// Auto-generated from MDI v{version_str} — do not edit manually.",
        "// To regenerate: python3 hmi/dev/fonts/build_mdi_fonts.py",
        "// Source: https://github.com/Templarian/MaterialDesign-SVG",
        "",
        "#pragma once",
        "",
        "#include <cstdint>",
        "",
        "namespace esphome::nspanel_easy {",
        "",
        "/**",
        " * @namespace Icons",
        " * @brief MDI icon ZI codepoints for Nextion display visualization.",
        " *",
        " * These are post-ZiLib codepoints (BMP PUA range E000-FFFF) — the actual",
        " * values sent over UART to the Nextion display and used in Blueprint automations.",
        " *",
        " * Derivation: zi_codepoint = ttf_codepoint - 0xE2001",
        " * where ttf_codepoint is the F0xxx PUA-B address in MaterialDesignIconsDesktop.ttf.",
        " *",
        f" * Source:  https://github.com/Templarian/MaterialDesign-SVG v{version_str}",
        f" * Icons:   {len(zi_map)}",
        " */",
        "namespace Icons {",
        "",
    ]

    for name, zi_cp in sorted(zi_map.items()):
        const  = const_names[name]
        cp_str = f"\\u{zi_cp:04X}"
        # Emit with minimum spacing; clang-format below assigns the final
        # alignment column based on the repo's .clang-format settings.
        lines.append(f'constexpr const char *{const} = "{cp_str}";  ///< mdi:{name}')

    lines += [
        "",
        "}  // namespace Icons",
        "",
        "}  // namespace esphome::nspanel_easy",
        "",
    ]

    content = "\n".join(lines)

    # Normalize through clang-format so the file on disk matches exactly what
    # CI would produce. Script path: hmi/dev/fonts/build_mdi_fonts.py,
    # so the repo root is four parents up (file → fonts → dev → hmi → repo),
    # and .clang-format lives there.
    repo_root       = Path(__file__).resolve().parents[3]
    style_hint_path = repo_root / "components" / "nspanel_easy" / "all_icons.h"
    try:
        result = subprocess.run(
            ["clang-format", "--style=file",
             f"--assume-filename={style_hint_path}"],
            input=content, capture_output=True, text=True, check=True,
        )
        content = result.stdout
    except FileNotFoundError:
        print("  WARNING: clang-format not on PATH — file written unformatted.")
        print("           CI will reformat on next run, causing a spurious diff.")
    except subprocess.CalledProcessError as e:
        print(f"  WARNING: clang-format failed ({e.returncode}): {e.stderr.strip()}")
        print("           File written unformatted; CI will reformat on next run.")

    with open(output_path, "w", encoding="utf-8") as f:
        f.write(content)
    print(f"  Saved: {output_path}  ({len(zi_map)} constants)")


# =============================================================================
# Main
# =============================================================================

def main():
    args = parse_args()

    svg_dir    = Path(args.svg_dir).resolve()
    output_dir = Path(args.output_dir).resolve()
    meta_path  = svg_dir / "meta.json"

    # Validate inputs
    if not meta_path.exists():
        print(f"ERROR: meta.json not found in {svg_dir}")
        sys.exit(1)
    if not (svg_dir / "svg").is_dir():
        print(f"ERROR: svg/ folder not found in {svg_dir}")
        sys.exit(1)

    print(f"Loading {meta_path}...")
    with open(meta_path) as f:
        meta = json.load(f)

    version_str  = detect_version(svg_dir)
    ttf_filename = f"MaterialDesignIconsDesktop-{version_str}.ttf"

    print(f"MDI version: {version_str}")
    print(f"Total icons in meta.json: {len(meta)}")

    if args.dry_run:
        svg_count = sum(
            1 for icon in meta
            if (svg_dir / "svg" / f"{icon['name']}.svg").exists()
        )
        print(f"Dry run: {svg_count} SVG files found, would build {ttf_filename}")
        return

    output_dir.mkdir(parents=True, exist_ok=True)
    ttf_path = output_dir / ttf_filename

    # Build TTF (returns TTF F0xxx codepoints)
    print("\nBuilding TTF...")
    name_to_ttf_cp = build_ttf(meta, svg_dir, ttf_path, version_str)

    # Compute ZI codepoints (post-ZiLib remapping)
    zi_map = build_zi_map(name_to_ttf_cp)
    print(f"  ZI codepoint map: {len(zi_map)} icons within U+FFFF")

    # Generate artifacts
    print("\nGenerating artifacts...")
    build_all_icons_json(zi_map,                                        output_dir / "all_icons.json")
    build_blueprint_icons(zi_map, output_dir / "blueprint_icons.yaml", args.blueprint)
    build_cheatsheet(name_to_ttf_cp, zi_map, ttf_filename, output_dir / "cheatsheet.html", version_str)
    build_header(zi_map, version_str,                                   output_dir / "all_icons.h")

    # Copy to docs dir if specified
    if args.docs_dir:
        docs_dir = Path(args.docs_dir)
        docs_dir.mkdir(parents=True, exist_ok=True)
        for fname in (ttf_filename, "cheatsheet.html", "all_icons.json"):
            shutil.copy(output_dir / fname, docs_dir / fname)
            print(f"  Copied to docs: {docs_dir / fname}")

    # Copy header to component dir if specified
    if args.header_dir:
        header_dir = Path(args.header_dir)
        header_dir.mkdir(parents=True, exist_ok=True)
        shutil.copy(output_dir / "all_icons.h", header_dir / "all_icons.h")
        print(f"  Copied to component: {header_dir / 'all_icons.h'}")

    print(f"\nDone. {len(zi_map)} icons.")
    print(f"Output: {output_dir}")


if __name__ == "__main__":
    main()
