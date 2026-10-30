#!/usr/bin/env python3
"""
nextion2text_shim.py

Runs the pinned upstream Nextion2Text.py unmodified on any platform, producing
the same pure-ASCII output format the project has always tracked.

Nextion2Text was written for Windows and relies on two behaviours that do not
survive a move to Linux:

1. It decodes HMI strings with the "ansi" codec, which exists only on Windows.
   This shim registers "ansi" as an alias for iso-8859-1, which maps every byte
   value to the identical code point and therefore never rejects input. That
   matters because HMI files contain bytes cp1252 leaves undefined.

2. It opens its output files with no explicit encoding, so the result depends on
   the machine's locale. This shim pins that to ASCII with backslashreplace, so
   every non-ASCII code point is written back as \\xHH. Combined with the
   latin-1 decode above, each original HMI byte reappears as its own escape:
   the MDI icon b"\\xee\\x92\\x97" is written as the text \\xee\\x92\\x97.

The result is byte-for-byte reproducible on any platform and contains no
non-ASCII characters, which keeps the generated dumps readable in diffs.

The input HMI is read once, before the tool runs, and the tool is served those
same bytes, so the pages and the exports below always come from one version of
the file.

The upstream file is left untouched, so the pinned revision stays verifiable
against its source.

Nextion Editor v1.68.2 added macros (named global events), stored in a new
container entry, "comcode.sn", which the pinned tool does not read. After the
tool finishes, this shim exports them to Macros.txt in the same output folder,
using the same indented layout as Program.s.txt.

Native macro export is proposed upstream in MMMZZZZ/Nextion2Text#20
(https://github.com/MMMZZZZ/Nextion2Text/pull/20), with the same output. Once
it is merged, move the pin past it and drop the macro export from this shim.

The pinned tool also does not export the project settings stored in the
"main.HMI" container entry. This shim writes the model and the display
orientation (Device > Display) to main.HMI.txt, so an orientation change shows
in reviews. The same output is produced by edwardtfn/Nextion2Text#2
(https://github.com/edwardtfn/Nextion2Text/pull/2), used by the Nextion Editor
Docker image.

Usage:
    python3 nextion2text_shim.py <Nextion2Text.py> [tool arguments...]
"""

import argparse
import builtins
import codecs
import io
import os
import runpy
import struct
import sys
from pathlib import Path

# Output encoding for the generated text dumps. ASCII plus backslashreplace is
# what produces the tracked \xHH escape format; do not change it without
# regenerating the whole hmi/dev/nextion2text tree.
_OUTPUT_ENCODING = "ascii"
_OUTPUT_ERRORS = "backslashreplace"


def _lookup_ansi(name):
    """Resolve the Windows-only "ansi" codec to iso-8859-1."""
    if name.lower() == "ansi":
        return codecs.lookup("iso-8859-1")
    # if ansi

    return None
# _lookup_ansi


_real_open = builtins.open

# Input HMI captured before the tool runs, as (resolved path, bytes).
_input_hmi = None


def _open_as_ascii(file, mode="r", buffering=-1, encoding=None,
                   errors=None, newline=None, closefd=True, opener=None):
    """Pin text-mode output to ASCII with backslashreplace.

    Nextion2Text opens its output with no explicit encoding, which would
    otherwise make the result depend on the locale of the machine running it.

    A binary read of the input HMI returns the captured bytes instead of
    reopening the file.
    """
    if (_input_hmi is not None and mode == "rb"
            and isinstance(file, (str, os.PathLike))
            and Path(file).resolve() == _input_hmi[0]):
        return io.BytesIO(_input_hmi[1])
    # if reading the captured input HMI

    if "b" not in mode and encoding is None:
        encoding = _OUTPUT_ENCODING

        if errors is None:
            errors = _OUTPUT_ERRORS
        # if default error handler
    # if text mode without explicit encoding

    return _real_open(file, mode, buffering, encoding,
                      errors, newline, closefd, opener)
# _open_as_ascii


# HMI container layout: a uint32 entry count followed by fixed-size entries of
# name[16], start[4], size[4], deleted[1] and three bytes not used here.
_HMI_ENTRY_FORMAT = "<16sII?3x"
_HMI_ENTRY_SIZE = struct.calcsize(_HMI_ENTRY_FORMAT)

# Container entry holding the macros (Nextion Editor v1.68.2 or later).
_MACROS_ENTRY = "comcode.sn"
_MACROS_TITLE = "Macros"
_INDENT = "    "

# Container entry holding the project settings. The model ID is a uint32 at
# offset 16 (as read by the tool) and the display orientation a uint8 at
# offset 12, storing 0..3 for 0, 90, 180 or 270 degrees.
_SETTINGS_ENTRY = "main.HMI"
_SETTINGS_MODEL_OFFSET = 16
_SETTINGS_ORIENTATION_OFFSET = 12
_ORIENTATIONS = {
    0: 0,
    1: 90,
    2: 180,
    3: 270,
}


def _read_hmi_entry(raw, entry_name):
    """Return the bytes of a live (not deleted) container entry, or None."""
    (count,) = struct.unpack_from("<I", raw, 0)

    for index in range(count):
        offset = 4 + index * _HMI_ENTRY_SIZE
        name, start, size, deleted = struct.unpack_from(
            _HMI_ENTRY_FORMAT, raw, offset)

        if deleted:
            continue
        # if deleted

        if name.split(b"\x00", 1)[0].decode("iso-8859-1") == entry_name:
            return raw[start:start + size]
        # if name matches
    # for index

    return None
# _read_hmi_entry


def _parse_macros(data):
    """Split the "comcode.sn" text into (name, code lines) records.

    Each record is the macro name, its line count, then that many code lines,
    all CRLF-terminated.
    """
    # Split on CRLF only: str.splitlines() would also break on bytes such as
    # 0x85, which occur inside MDI icon glyphs.
    lines = data.decode("iso-8859-1").split("\r\n")
    macros = []
    index = 0

    while index < len(lines):
        name = lines[index]

        if name == "":
            index += 1
            continue
        # if blank separator

        if index + 1 >= len(lines) or not lines[index + 1].isdigit():
            raise ValueError(
                "Macro '{}' has no valid line count".format(name))
        # if line count missing

        line_count = int(lines[index + 1])
        start = index + 2
        end = start + line_count

        if end > len(lines):
            raise ValueError(
                "Macro '{}' is truncated: expected {} lines, found {}".format(
                    name, line_count, len(lines) - start))
        # if truncated

        macros.append((name, lines[start:end]))
        index = end
    # while lines left

    return macros
# _parse_macros


def _parse_tool_args(tool_args):
    """Return the tool's input, output and extension arguments, or None.

    Returns None when a path is missing, which the tool has already reported.
    """
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("-i", "--input_hmi")
    parser.add_argument("-o", "--output_dir")
    parser.add_argument("-f", "--file_ext", default=".txt")
    args, _ = parser.parse_known_args(tool_args)

    if not args.input_hmi or not args.output_dir:
        return None
    # if paths missing

    # Same normalisation as the tool.
    if not args.file_ext.startswith("."):
        args.file_ext = "." + args.file_ext
    # if extension without dot

    return args
# _parse_tool_args


def _export_macros(args, raw):
    """Write the HMI macros to <output_dir>/Macros<ext>, when there are any."""
    data = _read_hmi_entry(raw, _MACROS_ENTRY)

    if data is None:
        return
    # if no macros entry

    macros = _parse_macros(data)

    if not macros:
        return
    # if no macros

    text = [_MACROS_TITLE]

    for name, code in macros:
        text.append(_INDENT + "Macro " + name)

        for line in code:
            text.append(_INDENT * 2 + line if line else "")
        # for line

        text.append("")
    # for macro

    out_path = Path(args.output_dir) / (_MACROS_TITLE + args.file_ext)

    # Same encoding and newline handling as the tool's own output.
    with _open_as_ascii(out_path, "w") as out_file:
        out_file.write("\n".join(text).rstrip("\n") + "\n")
    # with out_file
# _export_macros


def _read_settings(raw):
    """Return (model ID, orientation in degrees) from the settings entry.

    Raises ValueError when the entry is missing or the orientation unknown.
    Called before the tool runs, as its -d option clears the previous dump.
    """
    data = _read_hmi_entry(raw, _SETTINGS_ENTRY)

    if data is None:
        raise ValueError("No '{}' entry in the HMI file".format(
            _SETTINGS_ENTRY))
    # if no settings entry

    (model_id,) = struct.unpack_from("<I", data, _SETTINGS_MODEL_OFFSET)
    (orientation_raw,) = struct.unpack_from(
        "<B", data, _SETTINGS_ORIENTATION_OFFSET)

    if orientation_raw not in _ORIENTATIONS:
        raise ValueError(
            "Unknown display orientation: " + hex(orientation_raw))
    # if unknown orientation

    return model_id, _ORIENTATIONS[orientation_raw]
# _read_settings


def _export_settings(args, settings, models):
    """Write the model and display orientation to <output_dir>/main.HMI<ext>.

    settings comes from _read_settings. models is the tool's model table, so
    the names match its own output; the tool has already rejected unknown
    models before writing anything.
    """
    model_id, orientation = settings
    model = models[model_id]
    text = [
        _SETTINGS_ENTRY,
        _INDENT + "Model: " + model["short"] + " (" + model["long"] + ")",
        _INDENT + "Orientation: " + str(orientation),
    ]

    # The entry name contains a dot, so it cannot collide with a page name.
    out_path = Path(args.output_dir) / (_SETTINGS_ENTRY + args.file_ext)

    # Same encoding and newline handling as the tool's own output.
    with _open_as_ascii(out_path, "w") as out_file:
        out_file.write("\n".join(text) + "\n")
    # with out_file
# _export_settings


def main():
    """Register the codec and encoding default, then run the tool directly."""
    if len(sys.argv) < 2:
        print("ERROR: Path to Nextion2Text.py is required.", file=sys.stderr)
        return 2
    # if tool path missing

    global _input_hmi

    codecs.register(_lookup_ansi)

    tool = sys.argv[1]
    tool_args = sys.argv[2:]
    sys.argv = [tool] + tool_args  # Hide the shim from the tool's argparse

    args = _parse_tool_args(tool_args)

    if args is not None:
        with _real_open(args.input_hmi, "rb") as hmi_file:
            raw = hmi_file.read()
        # with hmi_file

        _input_hmi = (Path(args.input_hmi).resolve(), raw)
        settings = _read_settings(raw)
    # if paths given (otherwise the tool reports them missing)

    # runpy reads the tool's source through io.open_code, not builtins.open,
    # so patching here does not affect how the script itself is loaded.
    builtins.open = _open_as_ascii

    try:
        # Nextion2Text has no main guard; run_path executes it as __main__.
        # The returned globals expose the tool's model table.
        tool_globals = runpy.run_path(tool, run_name="__main__")
    finally:
        builtins.open = _real_open
    # try

    if args is None:
        return 0
    # if paths missing

    _export_macros(args, raw)
    _export_settings(args, settings, tool_globals["HMI"]._models)

    return 0
# main


if __name__ == "__main__":
    sys.exit(main())
# __main__
