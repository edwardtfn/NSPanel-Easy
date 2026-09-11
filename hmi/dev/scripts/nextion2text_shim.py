#!/usr/bin/env python3
"""
nextion2text_shim.py

Runs the pinned upstream Nextion2Text.py unmodified on any platform.

Nextion2Text was written for Windows and relies on two Windows behaviours:

1. It decodes HMI strings with the "ansi" codec, which exists only on Windows.
   There it is an alias for "mbcs", which calls MultiByteToWideChar without
   MB_ERR_INVALID_CHARS: on a Western European system that is Windows-1252 with
   the five undefined byte values (0x81, 0x8D, 0x8F, 0x90, 0x9D) passed through
   to the identical C1 control code points rather than rejected. Python's own
   cp1252 codec rejects those five bytes, and HMI files do contain them.

2. It opens its output files with no explicit encoding. On Windows the locale
   encoding is the same ANSI code page, so the decode above and the re-encode on
   write cancel out and the original HMI bytes end up in the file. Under a UTF-8
   locale they no longer cancel and every non-ASCII byte is doubled.

This shim supplies both, so the output is byte-identical to a local Windows run.
The upstream file is left untouched, so the pinned revision stays verifiable
against its source.

Usage:
    python3 nextion2text_shim.py <Nextion2Text.py> [tool arguments...]
"""

import builtins
import codecs
import encodings.cp1252
import runpy
import sys

# Byte values cp1252 leaves undefined but MultiByteToWideChar passes through.
_PASSTHROUGH_BYTES = (0x81, 0x8D, 0x8F, 0x90, 0x9D)


def _build_tables():
    """Return cp1252's tables with the undefined slots mapped to themselves."""
    table = list(encodings.cp1252.decoding_table)

    for byte in _PASSTHROUGH_BYTES:
        table[byte] = chr(byte)
    # for byte

    decoding_table = "".join(table)

    return decoding_table, codecs.charmap_build(decoding_table)
# _build_tables


_DECODING_TABLE, _ENCODING_TABLE = _build_tables()


class _AnsiCodec(codecs.Codec):
    """Stateless codec over the patched Windows-1252 tables."""

    def encode(self, input, errors="strict"):  # noqa: A002 - codecs API
        return codecs.charmap_encode(input, errors, _ENCODING_TABLE)
    # encode

    def decode(self, input, errors="strict"):  # noqa: A002 - codecs API
        return codecs.charmap_decode(input, errors, _DECODING_TABLE)
    # decode
# _AnsiCodec


class _AnsiIncrementalEncoder(codecs.IncrementalEncoder):
    """Incremental encoder; the mapping is stateless, so final is ignored."""

    def encode(self, input, final=False):  # noqa: A002 - codecs API
        return codecs.charmap_encode(input, self.errors, _ENCODING_TABLE)[0]
    # encode
# _AnsiIncrementalEncoder


class _AnsiIncrementalDecoder(codecs.IncrementalDecoder):
    """Incremental decoder; the mapping is stateless, so final is ignored."""

    def decode(self, input, final=False):  # noqa: A002 - codecs API
        return codecs.charmap_decode(input, self.errors, _DECODING_TABLE)[0]
    # decode
# _AnsiIncrementalDecoder


class _AnsiStreamWriter(_AnsiCodec, codecs.StreamWriter):
    pass
# _AnsiStreamWriter


class _AnsiStreamReader(_AnsiCodec, codecs.StreamReader):
    pass
# _AnsiStreamReader


_ANSI_CODEC_INFO = codecs.CodecInfo(
    name="ansi",
    encode=_AnsiCodec().encode,
    decode=_AnsiCodec().decode,
    incrementalencoder=_AnsiIncrementalEncoder,
    incrementaldecoder=_AnsiIncrementalDecoder,
    streamwriter=_AnsiStreamWriter,
    streamreader=_AnsiStreamReader,
)


def _lookup_ansi(name):
    """Resolve the Windows-only "ansi" codec on platforms that lack it."""
    if name.lower() == "ansi":
        return _ANSI_CODEC_INFO
    # if ansi

    return None
# _lookup_ansi


_real_open = builtins.open


def _open_as_ansi(file, mode="r", buffering=-1, encoding=None,
                  errors=None, newline=None, closefd=True, opener=None):
    """Default text-mode files to the ansi codec, as the Windows locale does."""
    if "b" not in mode and encoding is None:
        encoding = "ansi"
    # if text mode without explicit encoding

    return _real_open(file, mode, buffering, encoding,
                      errors, newline, closefd, opener)
# _open_as_ansi


def main():
    """Register the codec and encoding default, then run the tool directly."""
    if len(sys.argv) < 2:
        print("ERROR: Path to Nextion2Text.py is required.", file=sys.stderr)
        return 2
    # if tool path missing

    codecs.register(_lookup_ansi)

    # runpy reads the tool's source through io.open_code, not builtins.open,
    # so patching here does not affect how the script itself is loaded.
    builtins.open = _open_as_ansi

    tool = sys.argv[1]
    sys.argv = [tool] + sys.argv[2:]  # Hide the shim from the tool's argparse

    try:
        # Nextion2Text has no main guard; run_path executes it as __main__.
        runpy.run_path(tool, run_name="__main__")
    finally:
        builtins.open = _real_open
    # try

    return 0
# main


if __name__ == "__main__":
    sys.exit(main())
# __main__
