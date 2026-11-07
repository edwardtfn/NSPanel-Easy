#!/usr/bin/env bash
#
# generate_nxwright.sh
#
# Extracts every Nextion HMI file into an nxwright source folder (page code as
# .nx files, components and settings as TOML, pictures as PNG), so that HMI
# changes become reviewable in diffs.
#
# The output is for review only: the .hmi files remain the source of truth and
# nothing is ever built back from this folder.
#
# Each HMI is extracted into a fresh folder, so the result depends only on the
# HMI and never on a previous extraction. Fonts are binary and large, so they
# are replaced by a SHA-256 manifest that still shows when a font changes.
#
# The same pinned nxwright revision is used locally and in CI. Changing the pin
# may reformat the whole tree, so bump it deliberately.
#
# Usage:
#   ./generate_nxwright.sh
#
# Output:
#   hmi/dev/nxwright/<hmi_basename>/
#
# Requirements:
#   - python3 (3.11 or later) with the venv module
#   - network access on the first run, to install the pinned nxwright

set -euo pipefail

# Pinned upstream revision of nixiewright/nxwright.
NXWRIGHT_REF="5f31671f2931cb6fcde4e0d56ce6c3e63954c6f6"
NXWRIGHT_URL="https://codeload.github.com/nixiewright/nxwright/tar.gz/${NXWRIGHT_REF}"

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
HMI_DIR="$(cd -- "${SCRIPT_DIR}/../.." &>/dev/null && pwd)"
CACHE_DIR="${HMI_DIR}/dev/.cache"
OUT_ROOT="${HMI_DIR}/dev/nxwright"
VENV="${CACHE_DIR}/nxwright-${NXWRIGHT_REF:0:7}"
NXWRIGHT="${VENV}/bin/nxwright"
STAMP="${VENV}/.installed"

PYTHON="${PYTHON:-python3}"

if ! command -v "${PYTHON}" &>/dev/null; then
    echo "ERROR: '${PYTHON}' not found." >&2
    exit 1
fi  # python missing

if ! "${PYTHON}" -c 'import sys; sys.exit(0 if sys.version_info >= (3, 11) else 1)'; then
    echo "ERROR: nxwright needs Python 3.11 or later." >&2
    exit 1
fi  # python too old

# Install the pinned tool once, in its own venv; the cache directory is not tracked.
# The stamp is written last, so an interrupted install is redone on the next run.
if [[ ! -f "${STAMP}" ]]; then
    echo "Installing nxwright @ ${NXWRIGHT_REF:0:7}"
    rm -rf "${VENV}"
    mkdir -p "${CACHE_DIR}"
    "${PYTHON}" -m venv "${VENV}"
    "${VENV}/bin/python" -m pip install --quiet --disable-pip-version-check "${NXWRIGHT_URL}"
    touch "${STAMP}"
fi  # tool not installed

shopt -s nullglob
HMI_FILES=("${HMI_DIR}"/*.hmi)
shopt -u nullglob

if [[ ${#HMI_FILES[@]} -eq 0 ]]; then
    echo "ERROR: No .hmi files found in ${HMI_DIR}" >&2
    exit 1
fi  # no HMI files

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "${WORK_DIR}"' EXIT

for hmi in "${HMI_FILES[@]}"; do
    name="$(basename "${hmi}" .hmi)"
    staging="${WORK_DIR}/${name}"
    echo "Extracting ${name}.hmi"

    # --continue-with-unknown: content nxwright does not know only matters to a
    # build, which this script never runs. nxwright still lists it as a warning.
    "${NXWRIGHT}" extract --continue-with-unknown "${hmi}" "${staging}" </dev/null

    # Replace the font binaries by a manifest of their checksums.
    if [[ -d "${staging}/fonts" ]]; then
        (
            cd "${staging}"
            find fonts -type f -print0 | LC_ALL=C sort -z | xargs -0 -r sha256sum > fonts.sha256
        )
        rm -rf "${staging}/fonts"
    fi  # fonts extracted
done  # for hmi

# Replace the tracked output only after every extraction succeeded, so an
# aborted run never leaves a partial tree behind.
mkdir -p "${OUT_ROOT}"
for hmi in "${HMI_FILES[@]}"; do
    name="$(basename "${hmi}" .hmi)"
    rm -rf "${OUT_ROOT:?}/${name}"
    mv "${WORK_DIR}/${name}" "${OUT_ROOT}/${name}"
done  # for hmi

# Drop output directories whose .hmi source no longer exists.
shopt -s nullglob
for dir in "${OUT_ROOT}"/*/; do
    name="$(basename "${dir}")"

    if [[ ! -f "${HMI_DIR}/${name}.hmi" ]]; then
        echo "Removing stale output for ${name}"
        rm -rf "${dir}"
    fi  # stale output
done  # for dir
shopt -u nullglob

echo "Done. Output in ${OUT_ROOT}"
