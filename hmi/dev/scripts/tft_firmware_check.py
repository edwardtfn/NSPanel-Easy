#!/usr/bin/env python3
"""Checks that a built TFT carries the reference display firmware unchanged.

The firmware rows of a series F TFT (input.bin, the DLLs, syscom.bin, and so
on) come from the Nextion Editor. nxwright copies them from the reference TFT,
so they must match it byte for byte. Pictures, fonts and user code are the
project's own and are not compared.

Usage:
    tft_firmware_check.py NXWRIGHT REFERENCE.tft BUILT.tft
"""
import re
import subprocess
import sys

# "section at 0x10000, user code at 0xba0000"
_SECTION = re.compile(r"^section at (0x[0-9a-f]+)")
# "  row 1, asp100.dll: at 0x4532, 242280 bytes"
_ROW = re.compile(r"^\s+row \d+, (.+): at (0x[0-9a-f]+), (\d+) bytes$")
# Rows that hold the project's own data, not firmware.
_PROJECT_ROWS = ("the pictures", "the fonts")


def firmware_rows(nxwright: str, path: str) -> dict:
    """Returns {row name: bytes} for the firmware rows of the TFT at `path`."""
    listing = subprocess.run([nxwright, "tft-read", path], check=True, capture_output=True,
                             text=True, stdin=subprocess.DEVNULL).stdout
    with open(path, "rb") as tft_file:
        data = tft_file.read()

    section = None
    rows = {}
    for line in listing.splitlines():
        if match := _SECTION.match(line):
            section = int(match.group(1), 16)
        elif (match := _ROW.match(line)) and match.group(1) not in _PROJECT_ROWS:
            if section is None:
                raise ValueError(f"{path}: row listed before the section")
            start = section + int(match.group(2), 16)
            rows[match.group(1)] = data[start:start + int(match.group(3))]
        # end if line kind
    # end for line
    if not rows:
        raise ValueError(f"{path}: no firmware rows found")
    return rows


def main() -> int:
    """Compares the firmware rows of the built TFT with the reference's."""
    if len(sys.argv) != 4:
        print(__doc__, file=sys.stderr)
        return 2
    nxwright, reference, built = sys.argv[1:]
    expected = firmware_rows(nxwright, reference)
    actual = firmware_rows(nxwright, built)

    if expected != actual:
        changed = sorted(name for name in expected.keys() | actual.keys()
                         if expected.get(name) != actual.get(name))
        print(f"ERROR: {built}: firmware differs from the reference in: {', '.join(changed)}",
              file=sys.stderr)
        return 1
    print(f"{built}: firmware matches the reference ({len(actual)} rows)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
