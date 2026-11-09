#!/usr/bin/env bash
#
# hmi_sync.sh
#
# Brings the source in line with any HMI file changed by a pull request, then
# builds the HMI and TFT files from the source to check it.
#
# The source in hmi/src/<name>/ is the source of truth. The HMI and TFT files
# are build outputs, committed to dev by CI after each merge that changes the
# source. An HMI file is changed in a pull request only to bring an edit made
# in the Nextion Editor into the source. For each project, what the pull
# request changed (compared with BASE) decides what to do:
#
#   - Only the HMI file changed: import the HMI into the source.
#   - Only the source changed, or neither: build from the source.
#   - Both changed: built from the source, and accepted only when the build
#     gives the HMI and TFT files of the pull request, i.e. they were built
#     from its source (as on dev, merged into main by a release pull request).
#     Otherwise refused, since either side would silently overwrite the other.
#     Keep only one of the two changes, or run hmi_import.sh locally and review
#     the source diff.
#
# A project with an HMI file but no source yet is imported.
#
# CI runs this on every pull request that touches the HMI tree. It commits the
# imported source to the pull request and returns the HMI file to the base, so
# the pull request carries the source change only. The built HMI and TFT files
# are not committed there: they are attached to the workflow run for testing.
# Locally, run hmi_import.sh or hmi_build.sh directly instead.
#
# Usage:
#   ./hmi_sync.sh BASE    (BASE: the commit the pull request is compared with)

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
# shellcheck source=nxwright_env.sh
source "${SCRIPT_DIR}/nxwright_env.sh"

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 BASE" >&2
    exit 2
fi  # usage

BASE="$1"

cd "${HMI_DIR}"

# Changed by the pull request: compared with the merge base, so changes made
# to BASE after the branch started are not counted.
changed() {
    ! git diff --quiet "${BASE}...HEAD" -- "$@"
}

# Tracked HMI files only: a file built locally and left uncommitted is ignored.
# The glob magic keeps * from matching across directories.
mapfile -t NAMES < <(git ls-files -- ":(glob)*.hmi" | sed -e "s/\.hmi$//" | LC_ALL=C sort -u)

imports=()
verify=()
for name in "${NAMES[@]}"; do
    if [[ ! -d "src/${name}" ]]; then
        echo "${name}: no source yet, importing the HMI file"
        imports+=("${name}")
        continue
    fi  # new project

    hmi_changed=false
    src_changed=false
    if changed "${name}.hmi"; then
        hmi_changed=true
    fi  # HMI changed
    if changed "src/${name}"; then
        src_changed=true
    fi  # source changed

    if [[ "${hmi_changed}" == true && "${src_changed}" == true ]]; then
        echo "${name}: HMI file and source changed, checking that the HMI file is built from the source"
        verify+=("${name}")
    elif [[ "${hmi_changed}" == true ]]; then
        echo "${name}: HMI file changed, importing it into the source"
        imports+=("${name}")
    else
        echo "${name}: building from the source"
    fi  # which side changed
done  # for name

if [[ ${#imports[@]} -gt 0 ]]; then
    bash "${SCRIPT_DIR}/hmi_import.sh" "${imports[@]}"
fi  # imports

bash "${SCRIPT_DIR}/hmi_build.sh"

# The build is reproducible, so the HMI and TFT files of the pull request are
# unchanged by it only when they were built from its source.
failed=0
for name in "${verify[@]}"; do
    if git diff --quiet HEAD -- "${name}.hmi" "${name}.tft"; then
        echo "${name}: HMI and TFT files match the build of the source"
    else
        echo "ERROR: ${name}: this pull request changes both the HMI file and its source," \
             "and the HMI file is not the build of the source, so importing the HMI file" \
             "would undo the source changes, and building would undo the HMI changes." \
             "Keep only one of the two changes, or run hmi_import.sh ${name} locally," \
             "review the source diff, and commit it without the HMI file." >&2
        failed=1
    fi  # built files match
done  # for name

if [[ ${failed} -ne 0 ]]; then
    exit 1
fi  # refused projects
