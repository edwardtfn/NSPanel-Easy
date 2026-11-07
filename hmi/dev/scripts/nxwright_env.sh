# shellcheck shell=bash
#
# nxwright_env.sh
#
# Shared settings for the HMI scripts; sourced, never run directly.
#
# Defines the paths of the HMI tree, installs the pinned nxwright revision in
# its own venv on first use, and exports NXWRIGHT, the path of its executable.
#
# Changing the pin may change the built HMI and TFT files, so bump it
# deliberately and review the rebuilt output.
#
# Requirements:
#   - python3 (3.11 or later) with the venv module
#   - network access on the first run, to install the pinned nxwright

# Pinned upstream revision of nixiewright/nxwright.
NXWRIGHT_REF="a392a62fc16d63975990988f9d68930ed75a1ecb"
NXWRIGHT_URL="https://codeload.github.com/nixiewright/nxwright/tar.gz/${NXWRIGHT_REF}"

NX_SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
HMI_DIR="$(cd -- "${NX_SCRIPT_DIR}/../.." &>/dev/null && pwd)"
SRC_ROOT="${HMI_DIR}/src"
FONTS_DIR="${HMI_DIR}/dev/fonts"
CACHE_DIR="${HMI_DIR}/dev/.cache"

# Official Nextion Editor 1.68.2 firmware for the NX4832F035, inside a minimal
# TFT. Every compile takes the display firmware from it, and every built TFT
# must carry it unchanged (tft_firmware_check.py).
FIRMWARE_TFT="${HMI_DIR}/dev/firmware/nextion-1.68.2-NX4832F035.tft"
FIRMWARE_SHA256="61d724b6be9c9ac7aaba647ec4c8a928190762599ed53a1a12bb861211b017e7"

NX_VENV="${CACHE_DIR}/nxwright-${NXWRIGHT_REF:0:7}"
NX_PYTHON="${NX_VENV}/bin/python"
NXWRIGHT="${NX_VENV}/bin/nxwright"
export NXWRIGHT

# Installs the pinned nxwright once; the cache directory is not tracked.
# The stamp is written last, so an interrupted install is redone on the next run.
nxwright_install() {
    local python="${PYTHON:-python3}"
    local stamp="${NX_VENV}/.installed"

    if [[ -f "${stamp}" ]]; then
        return 0
    fi  # already installed

    if ! command -v "${python}" &>/dev/null; then
        echo "ERROR: '${python}' not found." >&2
        return 1
    fi  # python missing

    if ! "${python}" -c 'import sys; sys.exit(0 if sys.version_info >= (3, 11) else 1)'; then
        echo "ERROR: nxwright needs Python 3.11 or later." >&2
        return 1
    fi  # python too old

    echo "Installing nxwright @ ${NXWRIGHT_REF:0:7}"
    rm -rf "${NX_VENV}"
    mkdir -p "${CACHE_DIR}"
    "${python}" -m venv "${NX_VENV}"
    "${NX_PYTHON}" -m pip install --quiet --disable-pip-version-check "${NXWRIGHT_URL}"
    touch "${stamp}"
}

# Checks the firmware reference against its pinned checksum.
nxwright_check_firmware_reference() {
    local actual

    actual="$(sha256sum "${FIRMWARE_TFT}" | cut -d' ' -f1)"
    if [[ "${actual}" != "${FIRMWARE_SHA256}" ]]; then
        echo "ERROR: ${FIRMWARE_TFT} does not match its pinned SHA-256." >&2
        return 1
    fi  # checksum mismatch
}

# Prints the project names to process: the arguments, or every name found
# with the given glob ("hmi" for hmi/*.hmi, "src" for hmi/src/*/).
nxwright_projects() {
    local kind="$1"
    shift

    if [[ $# -gt 0 ]]; then
        printf '%s\n' "$@"
        return 0
    fi  # names given

    shopt -s nullglob
    local path
    if [[ "${kind}" == "hmi" ]]; then
        for path in "${HMI_DIR}"/*.hmi; do
            basename "${path}" .hmi
        done  # for path
    else
        for path in "${SRC_ROOT}"/*/; do
            basename "${path}"
        done  # for path
    fi  # kind
    shopt -u nullglob
}

# Fills FONT_BY_HASH, mapping each font's SHA-256 to its file in hmi/dev/fonts.
# shellcheck disable=SC2034  # read by the scripts that source this file
declare -gA FONT_BY_HASH=()
nxwright_index_fonts() {
    local hash path

    FONT_BY_HASH=()
    while read -r hash path; do
        # shellcheck disable=SC2034  # read by the scripts that source this file
        FONT_BY_HASH["${hash}"]="${path}"
    done < <(sha256sum "${FONTS_DIR}"/*.zi)
}
