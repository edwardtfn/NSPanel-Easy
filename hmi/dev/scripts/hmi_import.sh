#!/usr/bin/env bash
#
# hmi_import.sh
#
# Brings Nextion Editor changes back into the source: extracts each named HMI
# file into hmi/src/<name>/, replacing that source folder.
#
# Use it after editing an HMI file in the Nextion Editor, then run
# hmi_build.sh to rebuild the HMI and TFT files from the updated source.
# CI runs it automatically when a pull request changes an HMI file after its
# source (hmi_sync.sh).
#
# Each HMI is extracted into a fresh folder, so the result depends only on the
# HMI. Fonts are not stored in the source: each source folder lists them in
# fonts.sha256, and the build takes them from hmi/dev/fonts by checksum. A font
# that is not there yet is copied into hmi/dev/fonts as imported-<hash>.zi;
# rename it freely, since only its content matters.
#
# Usage:
#   ./hmi_import.sh [name ...]    (default: every hmi/*.hmi)

set -euo pipefail

# shellcheck source=nxwright_env.sh
source "$(dirname -- "${BASH_SOURCE[0]}")/nxwright_env.sh"
nxwright_install
nxwright_index_fonts

mapfile -t NAMES < <(nxwright_projects hmi "$@")
if [[ ${#NAMES[@]} -eq 0 ]]; then
    echo "ERROR: No HMI files to import." >&2
    exit 1
fi  # nothing to import

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "${WORK_DIR}"' EXIT

for name in "${NAMES[@]}"; do
    hmi="${HMI_DIR}/${name}.hmi"
    staging="${WORK_DIR}/${name}"

    if [[ ! -f "${hmi}" ]]; then
        echo "ERROR: ${hmi} not found." >&2
        exit 1
    fi  # missing HMI

    echo "Importing ${name}.hmi"
    # --continue-with-unknown: content nxwright does not know is listed as a
    # warning; the build then refuses it if it matters to the output.
    "${NXWRIGHT}" extract --continue-with-unknown "${hmi}" "${staging}" </dev/null

    # Replace the font binaries by a manifest, keeping any new font.
    if [[ -d "${staging}/fonts" ]]; then
        (
            cd "${staging}"
            find fonts -type f -print0 | LC_ALL=C sort -z | xargs -0 -r sha256sum > fonts.sha256
        )
        while read -r hash path; do
            if [[ -z "${FONT_BY_HASH[${hash}]:-}" ]]; then
                echo "New font ${path}: adding it as hmi/dev/fonts/imported-${hash:0:12}.zi"
                cp "${staging}/${path}" "${FONTS_DIR}/imported-${hash:0:12}.zi"
                FONT_BY_HASH["${hash}"]="${FONTS_DIR}/imported-${hash:0:12}.zi"
            fi  # font not stored yet
        done < "${staging}/fonts.sha256"
        rm -rf "${staging}/fonts"
    fi  # fonts extracted
done  # for name

# Replace the source folders only after every extraction succeeded.
mkdir -p "${SRC_ROOT}"
for name in "${NAMES[@]}"; do
    rm -rf "${SRC_ROOT:?}/${name}"
    mv "${WORK_DIR}/${name}" "${SRC_ROOT}/${name}"
done  # for name

echo "Done. Source in ${SRC_ROOT}"
