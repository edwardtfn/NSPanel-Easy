#!/usr/bin/env python3
"""
nextion2text_shim.py

Runs the pinned upstream Nextion2Text.py unmodified on any platform.

Nextion2Text decodes HMI strings with the "ansi" codec. That codec exists only
on Windows, where it resolves to the system ANSI code page (Windows-1252 on
Western European locales). On Linux the lookup raises LookupError, so this shim
registers "ansi" as an alias for cp1252 before handing over, keeping CI output
byte-identical to a local Windows run.

The upstream file is left untouched, so the pinned revision stays verifiable
against its source.

Usage:
    python3 nextion2text_shim.py <Nextion2Text.py> [tool arguments...]
"""

import codecs
import runpy
import sys


def _ansi_as_cp1252(name):
    """Resolve the Windows-only "ansi" codec to cp1252."""
    if name.lower() == "ansi":
        return codecs.lookup("cp1252")
    # if ansi

    return None
# _ansi_as_cp1252


def main():
    """Register the codec alias, then run the tool as if invoked directly."""
    if len(sys.argv) < 2:
        print("ERROR: Path to Nextion2Text.py is required.", file=sys.stderr)
        return 2
    # if tool path missing

    codecs.register(_ansi_as_cp1252)

    tool = sys.argv[1]
    sys.argv = [tool] + sys.argv[2:]  # Hide the shim from the tool's argparse

    # Nextion2Text has no main guard; run_path executes it as __main__.
    runpy.run_path(tool, run_name="__main__")

    return 0
# main


if __name__ == "__main__":
    sys.exit(main())
# __main__
