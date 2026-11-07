#!/usr/bin/env bash
#
# hmi_sync.sh
#
# Brings the source and the HMI and TFT files in line, deciding per project
# from the git history which side changed last:
#
#   - The HMI file changed in a later commit than its source (an edit made in
#     the Nextion Editor): import the HMI into the source, then rebuild.
#   - Otherwise: rebuild the HMI and TFT files from the source.
#   - A project whose HMI and source changed on diverging histories, or in
#     the same commit, is refused when its HMI file does not match what the
#     source builds, since either side would silently overwrite the other.
#     Resolve it by hand with hmi_import.sh or hmi_build.sh.
#
# It works on committed history only: locally, run hmi_import.sh or
# hmi_build.sh directly instead. CI runs this on every pull request that
# touches the HMI tree, and commits what changed.
#
# Usage:
#   ./hmi_sync.sh

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
# shellcheck source=nxwright_env.sh
source "${SCRIPT_DIR}/nxwright_env.sh"

cd "${HMI_DIR}"

mapfile -t NAMES < <({ nxwright_projects hmi; nxwright_projects src; } | LC_ALL=C sort -u)

imports=()
guarded=()
for name in "${NAMES[@]}"; do
    if [[ ! -d "src/${name}" ]]; then
        echo "${name}: no source yet, importing the HMI file"
        imports+=("${name}")
        continue
    fi  # new HMI

    if [[ ! -f "${name}.hmi" ]]; then
        echo "${name}: no HMI file yet, building it"
        continue
    fi  # new source

    hmi_commit="$(git log -1 --format=%H -- "${name}.hmi")"
    src_commit="$(git log -1 --format=%H -- "src/${name}")"

    if [[ -z "${hmi_commit}" || -z "${src_commit}" ]]; then
        echo "${name}: not committed yet, building from source"
    elif [[ "${hmi_commit}" == "${src_commit}" ]]; then
        echo "${name}: HMI and source changed in the same commit, building from source"
        guarded+=("${name}")
    elif git merge-base --is-ancestor "${hmi_commit}" "${src_commit}"; then
        echo "${name}: source changed last, building from source"
    elif git merge-base --is-ancestor "${src_commit}" "${hmi_commit}"; then
        echo "${name}: HMI changed last, importing it into the source"
        imports+=("${name}")
    else
        echo "${name}: HMI and source changed on diverging histories, building from source"
        guarded+=("${name}")
    fi  # which side changed last
done  # for name

if [[ ${#imports[@]} -gt 0 ]]; then
    bash "${SCRIPT_DIR}/hmi_import.sh" "${imports[@]}"
fi  # imports

bash "${SCRIPT_DIR}/hmi_build.sh"

# A guarded project may only pass when the build reproduced its HMI file.
failed=0
for name in "${guarded[@]}"; do
    if ! git diff --quiet -- "${name}.hmi"; then
        echo "ERROR: ${name}: the HMI file and the source both changed, and the HMI file" \
             "does not match the source. Run hmi_import.sh ${name} to keep the HMI" \
             "changes, or hmi_build.sh ${name} to keep the source, and commit the result." >&2
        failed=1
    fi  # HMI not reproduced
done  # for name

exit "${failed}"
