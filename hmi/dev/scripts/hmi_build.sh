#!/usr/bin/env bash
#
# hmi_build.sh
#
# Builds hmi/<name>.hmi and hmi/<name>.tft from each source folder in
# hmi/src/<name>/. The source is the source of truth; the HMI and TFT files
# are build outputs. CI commits them to dev after each merge that changes the
# source, since releases serve the TFT files from the repository.
#
# The fonts listed in each fonts.sha256 are taken from hmi/dev/fonts by
# checksum, and the display firmware from the pinned reference TFT. Every
# built TFT is checked to carry that firmware unchanged.
#
# The output depends only on the source, the fonts, the firmware reference
# and the nxwright pin, so a rebuild without changes gives identical files.
#
# A variant is a TFT built from the source of another one with a few settings
# changed (see VARIANTS). It has no source folder and no HMI file of its own,
# and is built whenever its source is.
#
# Usage:
#   ./hmi_build.sh [name ...]    (default: every hmi/src/*/)

set -euo pipefail

# shellcheck source=nxwright_env.sh
source "$(dirname -- "${BASH_SOURCE[0]}")/nxwright_env.sh"
nxwright_install
nxwright_check_firmware_reference
nxwright_index_fonts

mapfile -t NAMES < <(nxwright_projects src "$@")
if [[ ${#NAMES[@]} -eq 0 ]]; then
    echo "ERROR: No source folders to build." >&2
    exit 1
fi  # nothing to build

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "${WORK_DIR}"' EXIT

# Variants, as "name|source|rotation|hard_coded_display_mode":
# - nspanel_landscape_left: US panel in landscape with the hardware buttons on the left, which is
#   the landscape TFT rotated by 180 degrees (rotation 90 instead of 270), with its own display mode.
VARIANTS=(
    "nspanel_landscape_left|nspanel_landscape|90|5"
)
BUILT_VARIANTS=()

for name in "${NAMES[@]}"; do
    src="${SRC_ROOT}/${name}"
    staging="${WORK_DIR}/src/${name}"
    out="${WORK_DIR}/out/${name}"

    if [[ ! -f "${src}/app.toml" ]]; then
        echo "ERROR: ${src} is not a source folder." >&2
        exit 1
    fi  # missing source

    echo "Building ${name}"
    mkdir -p "${staging}" "${out}"
    cp -R "${src}/." "${staging}/"

    # Put the fonts back where the source folder expects them.
    if [[ -f "${staging}/fonts.sha256" ]]; then
        while read -r hash path; do
            font="${FONT_BY_HASH[${hash}]:-}"
            if [[ -z "${font}" ]]; then
                echo "ERROR: ${name}: no font in hmi/dev/fonts matches ${path} (${hash})." >&2
                exit 1
            fi  # font not found
            mkdir -p "$(dirname -- "${staging}/${path}")"
            cp "${font}" "${staging}/${path}"
        done < "${staging}/fonts.sha256"
        rm "${staging}/fonts.sha256"
    fi  # font manifest

    "${NXWRIGHT}" compile --firmware "${FIRMWARE_TFT}" \
        --project-file "${out}/${name}.hmi" --no-lock --no-backup \
        "${staging}" "${out}/${name}.tft" </dev/null

    "${NX_PYTHON}" "${NX_SCRIPT_DIR}/tft_firmware_check.py" \
        "${NXWRIGHT}" "${FIRMWARE_TFT}" "${out}/${name}.tft"

    # The variants of this source, built from the same staging folder (fonts included)
    for variant in "${VARIANTS[@]}"; do
        IFS='|' read -r variant_name variant_source variant_rotation variant_mode <<< "${variant}"
        if [[ "${variant_source}" != "${name}" ]]; then
            continue
        fi  # variant of another source

        echo "Building ${variant_name} (variant of ${name})"
        variant_staging="${WORK_DIR}/src/${variant_name}"
        variant_out="${WORK_DIR}/out/${variant_name}"
        mkdir -p "${variant_out}"
        cp -R "${staging}" "${variant_staging}"
        sed -i -E \
            -e "s/^name = \"${name}\"\$/name = \"${variant_name}\"/" \
            -e "s/^rotation = [0-9]+\$/rotation = ${variant_rotation}/" \
            "${variant_staging}/app.toml"
        sed -i -E "s/^int hard_coded_display_mode=[0-9]+/int hard_coded_display_mode=${variant_mode}/" \
            "${variant_staging}/program.nx"
        # Each setting must have been found, or the variant would silently be a copy of its source
        if ! grep -q "^name = \"${variant_name}\"\$" "${variant_staging}/app.toml" ||
           ! grep -q "^rotation = ${variant_rotation}\$" "${variant_staging}/app.toml" ||
           ! grep -q "^int hard_coded_display_mode=${variant_mode}\b" "${variant_staging}/program.nx"; then
            echo "ERROR: ${variant_name}: could not apply the variant settings to ${name}." >&2
            exit 1
        fi  # settings not applied

        "${NXWRIGHT}" compile --firmware "${FIRMWARE_TFT}" \
            --project-file "${variant_out}/${variant_name}.hmi" --no-lock --no-backup \
            "${variant_staging}" "${variant_out}/${variant_name}.tft" </dev/null

        "${NX_PYTHON}" "${NX_SCRIPT_DIR}/tft_firmware_check.py" \
            "${NXWRIGHT}" "${FIRMWARE_TFT}" "${variant_out}/${variant_name}.tft"
        BUILT_VARIANTS+=("${variant_name}")
    done  # for variant
done  # for name

# Replace the tracked files only after every build succeeded.
for name in "${NAMES[@]}"; do
    mv "${WORK_DIR}/out/${name}/${name}.hmi" "${HMI_DIR}/${name}.hmi"
    mv "${WORK_DIR}/out/${name}/${name}.tft" "${HMI_DIR}/${name}.tft"
done  # for name
# Only the TFT of a variant is kept: an HMI file would be imported as a source of its own
for name in "${BUILT_VARIANTS[@]}"; do
    mv "${WORK_DIR}/out/${name}/${name}.tft" "${HMI_DIR}/${name}.tft"
done  # for variant

echo "Done. Built ${#NAMES[@]} project(s) and ${#BUILT_VARIANTS[@]} variant(s) in ${HMI_DIR}"
