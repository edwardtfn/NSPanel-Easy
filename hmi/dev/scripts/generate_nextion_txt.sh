#!/usr/bin/env bash
#
# generate_nextion_txt.sh
#
# Regenerates the human-readable text dump of every Nextion HMI file, so that
# HMI changes become reviewable in diffs.
#
# The same pinned Nextion2Text revision is used locally and in CI. Changing the
# pin reformats the whole tree, so bump it deliberately.
#
# Usage:
#   ./generate_nextion_txt.sh
#
# Output:
#   hmi/dev/nextion2text/<hmi_basename>/*.txt
#
# Requirements:
#   - python3
#   - curl (first run only, to fetch the pinned Nextion2Text.py)

set -euo pipefail

# Pinned upstream revision of MMMZZZZ/Nextion2Text.
NEXTION2TEXT_REF="7edf48558208dcf92ba8d9dd649677b06c1362db"
NEXTION2TEXT_URL="https://raw.githubusercontent.com/MMMZZZZ/Nextion2Text/${NEXTION2TEXT_REF}/Nextion2Text.py"

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
HMI_DIR="$(cd -- "${SCRIPT_DIR}/../.." &>/dev/null && pwd)"
OUT_ROOT="${HMI_DIR}/dev/nextion2text"
CACHE_DIR="${HMI_DIR}/dev/.cache"
TOOL="${CACHE_DIR}/Nextion2Text-${NEXTION2TEXT_REF:0:7}.py"
SHIM="${SCRIPT_DIR}/nextion2text_shim.py"

PYTHON="${PYTHON:-python3}"

if ! command -v "${PYTHON}" &>/dev/null; then
    echo "ERROR: '${PYTHON}' not found." >&2
    exit 1
fi  # python missing

# Fetch the pinned tool once; the cache directory is not tracked.
if [[ ! -f "${TOOL}" ]]; then
    echo "Fetching Nextion2Text @ ${NEXTION2TEXT_REF:0:7}"
    mkdir -p "${CACHE_DIR}"
    curl -sSfL "${NEXTION2TEXT_URL}" -o "${TOOL}.tmp"
    mv "${TOOL}.tmp" "${TOOL}"
fi  # tool not cached

shopt -s nullglob
HMI_FILES=("${HMI_DIR}"/*.hmi)
shopt -u nullglob

if [[ ${#HMI_FILES[@]} -eq 0 ]]; then
    echo "ERROR: No .hmi files found in ${HMI_DIR}" >&2
    exit 1
fi  # no HMI files

for hmi in "${HMI_FILES[@]}"; do
    name="$(basename "${hmi}" .hmi)"
    echo "Generating text dump for ${name}.hmi"

    # -d clears the output folder first, so removed pages disappear from the tree.
    # -p must come last: it takes a variable number of values.
    "${PYTHON}" "${SHIM}" "${TOOL}" \
        -i "${hmi}" \
        -o "${OUT_ROOT}/${name}" \
        -d \
        -p visual unknown
done  # for hmi

# Drop output directories whose .hmi source no longer exists. Deliberately done
# after every conversion succeeded, so an aborted run never deletes valid output.
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
