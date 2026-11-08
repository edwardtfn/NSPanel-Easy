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
done  # for name

# Replace the tracked files only after every build succeeded.
for name in "${NAMES[@]}"; do
    mv "${WORK_DIR}/out/${name}/${name}.hmi" "${HMI_DIR}/${name}.hmi"
    mv "${WORK_DIR}/out/${name}/${name}.tft" "${HMI_DIR}/${name}.tft"
done  # for name

echo "Done. Built ${#NAMES[@]} project(s) in ${HMI_DIR}"
