#!/usr/bin/env bash
#
# generate_images.sh
#
# Generates Nextion HMI picture files for all orientations and themes by
# compositing source images onto background canvases, as defined in
# mapping.yaml (located alongside this script).
#
# Usage:
#   ./generate_images.sh [orientation]
#
#   landscape | portrait | all
#
# Output directories (alongside this script, inside ui/pics/):
#   landscape/    Files named by HMI slot number (e.g. 0.png, 1.png, ...)
#   portrait/     Same structure for portrait orientation
#
# Both dark-theme and light-theme slot files are written to the same output
# directory. The slot numbers in mapping.yaml determine which filename each
# variant receives — there is no separate dark/light output folder.
#
# Requirements:
#   - ImageMagick (magick or convert) OR Docker with dpokidov/imagemagick
#   - python3 with PyYAML (pip install pyyaml)

set -euo pipefail

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
MAPPING="${SCRIPT_DIR}/mapping.yaml"
PLACEHOLDER="${SCRIPT_DIR}/misc/placeholder.png"

# ---------------------------------------------------------------------------
# Arguments
# ---------------------------------------------------------------------------

ORIENTATION="${1:-all}"

if [[ "${ORIENTATION}" == "all" ]]; then
    bash "${BASH_SOURCE[0]}" landscape
    bash "${BASH_SOURCE[0]}" portrait
    exit $?
fi

if [[ "${ORIENTATION}" != landscape* ]] && [[ "${ORIENTATION}" != portrait* ]]; then
    echo "ERROR: Unknown orientation '${ORIENTATION}'. Use 'landscape' or 'portrait'." >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# Dependency checks
# ---------------------------------------------------------------------------

if ! command -v python3 &>/dev/null; then
    echo "ERROR: python3 is required." >&2
    exit 1
fi

if ! python3 -c "import yaml" &>/dev/null; then
    echo "ERROR: PyYAML is required. Run: pip install pyyaml" >&2
    exit 1
fi

if [[ ! -f "${MAPPING}" ]]; then
    echo "ERROR: Mapping file not found: ${MAPPING}" >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# ImageMagick wrapper
# Runs magick/convert from within SCRIPT_DIR (ui/pics/) so that relative paths
# in the mapping resolve correctly.
# ---------------------------------------------------------------------------

magick_command() {
    if command -v magick &>/dev/null; then
        pushd "${SCRIPT_DIR}" &>/dev/null
            magick "$@"
        popd &>/dev/null
    elif command -v convert &>/dev/null; then
        pushd "${SCRIPT_DIR}" &>/dev/null
            convert "$@"
        popd &>/dev/null
    else
        # Docker fallback — paths must be relative to /imgs inside the container
        docker run --rm -v "${SCRIPT_DIR}:/imgs" -w /imgs dpokidov/imagemagick "$@"
    fi
}

# ---------------------------------------------------------------------------
# Output directory setup
# ---------------------------------------------------------------------------

OUT_DIR="${SCRIPT_DIR}/${ORIENTATION}"
rm -f "${OUT_DIR}"/*.png "${OUT_DIR}"/*.jpg 2>/dev/null || true
mkdir -p "${OUT_DIR}"

if [[ "${ORIENTATION}" == landscape* ]]; then
    echo "Generating landscape images..."
else
    echo "Generating portrait images..."
fi

# ---------------------------------------------------------------------------
# Placeholder generation
# Creates a 1x1 black PNG if it does not already exist.
# ---------------------------------------------------------------------------

if [[ ! -f "${PLACEHOLDER}" ]]; then
    echo "Creating placeholder: ${PLACEHOLDER}"
    mkdir -p "$(dirname "${PLACEHOLDER}")"
    magick_command -size 1x1 xc:black "misc/placeholder.png"
fi

# ---------------------------------------------------------------------------
# Main processing loop
# Driven entirely by mapping.yaml — no hardcoded slot numbers here.
# ---------------------------------------------------------------------------

python3 - "${MAPPING}" "${ORIENTATION}" "${SCRIPT_DIR}" "${OUT_DIR}" << 'PYEOF'
import sys
import os
import subprocess
import shutil
import yaml

mapping_path, orientation, pics_dir, out_dir = sys.argv[1:]

with open(mapping_path, "r", encoding="utf-8") as f:
    entries = yaml.safe_load(f)

is_landscape = orientation.startswith("landscape")
orient_key   = "landscape" if is_landscape else "portrait"

errors = []

def magick(*args):
    """Run ImageMagick from within pics_dir (SCRIPT_DIR)."""
    if shutil.which("magick"):
        cmd = ["magick"] + list(args)
    elif shutil.which("convert"):
        cmd = ["convert"] + list(args)
    else:
        cmd = ["docker", "run", "--rm",
               "-v", f"{pics_dir}:/imgs",
               "-w", "/imgs",
               "dpokidov/imagemagick"] + list(args)
    result = subprocess.run(cmd, cwd=pics_dir, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(result.stderr.strip())

def resolve_src(entry, orient_key):
    """
    Return the source path for the given orientation.
    Orientation-level src overrides top-level src.
    Returns None if neither is present (e.g. placeholder entries).
    """
    orient_block = entry.get(orient_key, {}) or {}
    src = orient_block.get("src") or entry.get("src")
    if src:
        return src
    return None

def output_path(slot):
    return os.path.join(out_dir, f"{slot}.png")

def background_path(theme):
    """
    Return the path to the background canvas for the given theme and orientation.
    Looks up the background_dark / background_light entries in the mapping.
    """
    bg_id = f"background_{theme}"
    for e in entries:
        if e.get("id") == bg_id:
            orient_block = e.get(orient_key, {}) or {}
            src = orient_block.get("src") or e.get("src")
            if src:
                return src
    raise RuntimeError(f"Background entry '{bg_id}' not found in mapping.")

bg_dark  = background_path("dark")
bg_light = background_path("light")

for entry in entries:
    entry_id = entry.get("id", "<unnamed>")

    # Skip background canvas entries — they are copied directly, not composited
    if entry_id in ("background_dark", "background_light"):
        dark_slot  = entry.get("dark")
        light_slot = entry.get("light")
        orient_block = entry.get(orient_key, {}) or {}
        src = orient_block.get("src") or entry.get("src")
        if not src:
            errors.append(f"{entry_id}: no src for orientation '{orient_key}'")
            continue
        src_path = os.path.join(pics_dir, src)
        if not os.path.isfile(src_path):
            errors.append(f"{entry_id}: source not found: {src_path}")
            continue
        if dark_slot is not None:
            shutil.copy(src_path, output_path(dark_slot))
            print(f"  copy  {src} -> {dark_slot}.png")
        if light_slot is not None:
            shutil.copy(src_path, output_path(light_slot))
            print(f"  copy  {src} -> {light_slot}.png")
        continue

    dark_slot  = entry.get("dark")
    light_slot = entry.get("light")
    is_copy    = entry.get("copy", False)
    is_placeholder = entry.get("placeholder", False)

    # ------------------------------------------------------------------
    # Placeholder entries — write the 1x1 pixel file to the dark slot
    # ------------------------------------------------------------------
    if is_placeholder:
        placeholder = os.path.join(pics_dir, "misc", "placeholder.png")
        if not os.path.isfile(placeholder):
            errors.append(f"{entry_id}: placeholder.png not found at {placeholder}")
            continue
        if dark_slot is not None:
            shutil.copy(placeholder, output_path(dark_slot))
            print(f"  placeholder -> {dark_slot}.png")
        if light_slot is not None:
            shutil.copy(placeholder, output_path(light_slot))
            print(f"  placeholder -> {light_slot}.png")
        continue

    # ------------------------------------------------------------------
    # Copy entries — no compositing, source copied directly
    # dark == light means same file goes to one slot only (written once)
    # ------------------------------------------------------------------
    if is_copy:
        src = resolve_src(entry, orient_key)
        if not src:
            errors.append(f"{entry_id}: copy=true but no src found")
            continue
        src_path = os.path.join(pics_dir, src)
        if not os.path.isfile(src_path):
            errors.append(f"{entry_id}: source not found: {src_path}")
            continue
        written = set()
        for slot in (dark_slot, light_slot):
            if slot is not None and slot not in written:
                shutil.copy(src_path, output_path(slot))
                print(f"  copy  {src} -> {slot}.png")
                written.add(slot)
        continue

    # ------------------------------------------------------------------
    # Composite entries — overlay source onto background canvas
    # ------------------------------------------------------------------
    orient_block = entry.get(orient_key, {}) or {}
    coords     = orient_block.get("coordinates")
    dimensions = orient_block.get("dimensions")

    if not coords or not dimensions:
        errors.append(f"{entry_id}: missing coordinates or dimensions for '{orient_key}'")
        continue

    # ------------------------------------------------------------------
    # Tiled entries — the source is placed at each of a list of
    # coordinates, and the whole canvas is kept (e.g. the buttons of the
    # button pages, cropped by each button's components on the display)
    # ------------------------------------------------------------------
    if isinstance(coords, list):
        src = resolve_src(entry, orient_key)
        if not src or not os.path.isfile(os.path.join(pics_dir, src)):
            errors.append(f"{entry_id}: source not found: {src}")
            continue
        placements = []
        for coord in coords:
            try:
                tx, ty = coord.split(",")
            except ValueError:
                errors.append(f"{entry_id}: invalid coordinates format '{coord}'")
                break
            placements += [src, "-geometry", f"+{tx.strip()}+{ty.strip()}", "-composite"]
        else:
            written = set()
            for theme, bg, slot in (("dark", bg_dark, dark_slot), ("light", bg_light, light_slot)):
                if slot is None or slot in written:
                    continue
                try:
                    magick(bg, *placements, "-background", "black", "-flatten", output_path(slot))
                    print(f"  tiles     [{theme}] {src} x{len(coords)} -> {slot}.png")
                except RuntimeError as e:
                    errors.append(f"{entry_id} [{theme}]: magick failed: {e}")
                written.add(slot)
        continue

    # Parse "X,Y" -> "+X+Y" for ImageMagick -geometry
    try:
        cx, cy = coords.split(",")
        geometry = f"+{cx.strip()}+{cy.strip()}"
        crop     = f"{dimensions}+{cx.strip()}+{cy.strip()}"
    except ValueError:
        errors.append(f"{entry_id}: invalid coordinates format '{coords}'")
        continue

    src = resolve_src(entry, orient_key)
    if not src:
        errors.append(f"{entry_id}: no src for orientation '{orient_key}'")
        continue
    src_path = os.path.join(pics_dir, src)
    if not os.path.isfile(src_path):
        errors.append(f"{entry_id}: source not found: {src_path}")
        continue

    written = set()
    for theme, bg, slot in (("dark", bg_dark, dark_slot), ("light", bg_light, light_slot)):
        if slot is None or slot in written:
            continue
        out = output_path(slot)
        try:
            magick(
                bg,
                src,
                "-geometry", geometry,
                "-composite",
                "-background", "black",
                "-flatten",
                "-crop", crop,
                "+repage",
                out,
            )
            print(f"  composite [{theme}] {src} -> {slot}.png")
        except RuntimeError as e:
            errors.append(f"{entry_id} [{theme}]: magick failed: {e}")
        written.add(slot)

# ---------------------------------------------------------------------------
# Report
# ---------------------------------------------------------------------------

if errors:
    print("\nERRORS:", file=sys.stderr)
    for err in errors:
        print(f"  - {err}", file=sys.stderr)
    sys.exit(1)

print(f"\nDone. Files written to: {out_dir}")
PYEOF
