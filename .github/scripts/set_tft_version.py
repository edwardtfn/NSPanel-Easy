#!/usr/bin/env python3
# .github/scripts/set_tft_version.py
#
# Sets the TFT version in the HMI source (hmi/src) without reformatting it.
#
# Each TFT carries its version in a string variable named "version" on the page
# that reports it to the firmware at boot. The versioning workflow sets it to the
# release version, so the TFT files built for a release carry the same version as
# the firmware and the Blueprint, and sets it back to the development version on
# dev after each release.
#
# Usage:
#   python3 set_tft_version.py <version>
#
# Arguments:
#   version    TFT version to set: a release (e.g. "2026.11.17") or the
#              development version "9999.99.9".
#
# The component is located by its name inside its own [[component]] table, and
# only its txt value is replaced. The script fails when a file, the component or
# its txt value is missing, or when the version does not fit the variable.

import re
import sys
from pathlib import Path

# Pages holding the "version" variable, relative to the repository root
TFT_VERSION_FILES = (
    'hmi/src/nspanel_blank/pages/blank.page.toml',
    'hmi/src/nspanel_landscape/pages/boot.page.toml',
    'hmi/src/nspanel_portrait/pages/boot.page.toml',
)

COMPONENT_NAME = 'version'

# CalVer, as produced by the versioning workflow (YYYY.M.seq)
VERSION_PATTERN = re.compile(r'^\d{4}\.\d{1,2}\.\d+$')


def set_version(content: str, version: str) -> tuple[str, str]:
    """Set the txt value of the "version" component in a page source.

    Args:
        content: Full page source (TOML) as a string.
        version: The version to set.

    Returns:
        Tuple of (updated content, error message). The error message is empty
        on success; the content is returned unchanged on error.
    """
    # Split into tables, keeping each header with its body
    tables = re.split(r'(?m)^(?=\[\[?[^\n]*\]\]?[ \t]*$)', content)
    matches = [
        index for index, table in enumerate(tables)
        if table.startswith('[[component]]')
        and re.search(r'(?m)^name = "' + re.escape(COMPONENT_NAME) + r'"[ \t]*$', table)
    ]
    if len(matches) != 1:
        return content, f'found {len(matches)} components named "{COMPONENT_NAME}", expected 1'

    table = tables[matches[0]]

    maxl = re.search(r'(?m)^txt_maxl = (\d+)[ \t]*$', table)
    if maxl is None:
        return content, f'component "{COMPONENT_NAME}" has no txt_maxl'
    if len(version) > int(maxl.group(1)):
        return content, (f'version "{version}" is longer than txt_maxl '
                         f'({maxl.group(1)}) of component "{COMPONENT_NAME}"')

    updated, count = re.subn(r'(?m)^txt = "[^"\n]*"[ \t]*$', f'txt = "{version}"', table, count=1)
    if count != 1:
        return content, f'component "{COMPONENT_NAME}" has no txt value'

    tables[matches[0]] = updated
    return ''.join(tables), ''


def main() -> None:
    if len(sys.argv) != 2:
        print(f'Usage: {sys.argv[0]} <version>', file=sys.stderr)
        sys.exit(1)

    version = sys.argv[1]
    if not VERSION_PATTERN.match(version):
        print(f'ERROR: Invalid version "{version}", expected YYYY.M.seq', file=sys.stderr)
        sys.exit(1)

    root = Path(__file__).resolve().parents[2]
    errors = []
    for relative in TFT_VERSION_FILES:
        path = root / relative
        if not path.is_file():
            errors.append(f'{relative}: file not found')
            continue

        # newline='' keeps the line endings exactly as they are
        with open(path, encoding='utf-8', newline='') as f:
            content = f.read()

        updated, error = set_version(content, version)
        if error:
            errors.append(f'{relative}: {error}')
            continue

        if updated != content:
            with open(path, 'w', encoding='utf-8', newline='') as f:
                f.write(updated)
            print(f'{relative}: TFT version set to {version}')
        else:
            print(f'{relative}: TFT version already {version}')
        # if updated != content

    if errors:
        for error in errors:
            print(f'ERROR: {error}', file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
