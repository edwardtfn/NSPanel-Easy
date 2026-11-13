#!/usr/bin/env python3
"""Fail when an ESPHome build uses too much of its app partition.

Run in the build job, right after the build: it measures the OTA image the
build produced against the smallest app partition of the build's partition
table, which is what ESP-IDF checks when flashing and what the build log
reports as "Flash". The image must fit an app partition with room to grow, as
the next OTA update is written to the other one.

  - WARN_PERCENT: a warning annotation, so growth is noticed early;
  - FAIL_PERCENT: an error annotation, and the script fails.

Builds whose name contains an excluded word (e.g. "Customizations", which tests
users' own additions) are reported but never fail.

With --output, the sizes are also written to a JSON file, which the build job
uploads as an artifact for the PR comment (see esphome_size_report.py). Besides
the flash usage, it holds the static RAM usage, read from the section headers of
the firmware's ELF file: the same sections PlatformIO adds up for its "RAM"
line (.dram0.data, .dram0.bss and .noinit).

Usage:
  esphome_size_guard.py --name NAME --search-dir DIR [--summary FILE] [--output FILE]

  --name NAME        build name, for the messages
  --search-dir DIR   directory holding the build output (the YAML file's directory)
  --summary FILE     append a Markdown line to FILE (e.g. $GITHUB_STEP_SUMMARY)
  --output FILE      write the sizes to FILE as JSON
"""

import argparse
import json
import struct
import sys
from pathlib import Path

WARN_PERCENT = 85.0
FAIL_PERCENT = 92.0
EXCLUDED = ("Customizations",)

IMAGE_NAMES = ("firmware.ota.bin", "firmware.bin")
TABLE_NAMES = ("partition-table.bin", "partitions.bin")
ENTRY_SIZE = 32
ENTRY_MAGIC = b"\xaa\x50"
APP_TYPE = 0x00
ELF_MAGIC = b"\x7fELF"
RAM_SECTIONS = (".dram0.data", ".dram0.bss", ".noinit")
BOOTLOADER_WORD = "bootloader"


def smallest_app_partition(table_path):
    """Size of the smallest app partition in an ESP-IDF partition table binary, or None."""
    data = table_path.read_bytes()
    sizes = []
    for start in range(0, len(data) - ENTRY_SIZE + 1, ENTRY_SIZE):
        entry = data[start:start + ENTRY_SIZE]
        if entry[:2] != ENTRY_MAGIC:
            break  # End of the table (0xFF padding or the MD5 entry)
        part_type = entry[2]
        _, size = struct.unpack_from("<II", entry, 4)
        if part_type == APP_TYPE and size > 0:
            sizes.append(size)
    return min(sizes) if sizes else None


def elf_section_sizes(elf_path):
    """Sizes of the sections of an ELF file by name, or {} when it cannot be read."""
    try:
        data = elf_path.read_bytes()
        if data[:4] != ELF_MAGIC or data[5] != 1:  # Little-endian only, as every ESP32
            return {}
        if data[4] == 1:  # ELF32: e_shoff at 32; sh_name, sh_offset and sh_size at 0, 16 and 20
            shoff, = struct.unpack_from("<I", data, 32)
            shentsize, shnum, shstrndx = struct.unpack_from("<HHH", data, 46)
            fields = ("<I", 0, 16, 20)
        elif data[4] == 2:  # ELF64: e_shoff at 40; sh_name, sh_offset and sh_size at 0, 24 and 32
            shoff, = struct.unpack_from("<Q", data, 40)
            shentsize, shnum, shstrndx = struct.unpack_from("<HHH", data, 58)
            fields = ("<Q", 0, 24, 32)
        else:
            return {}
        fmt, name_at, offset_at, size_at = fields

        def header(index):
            start = shoff + index * shentsize
            name, = struct.unpack_from("<I", data, start + name_at)
            offset, = struct.unpack_from(fmt, data, start + offset_at)
            size, = struct.unpack_from(fmt, data, start + size_at)
            return name, offset, size

        _, names_offset, names_size = header(shstrndx)
        names = data[names_offset:names_offset + names_size]
        sizes = {}
        for index in range(shnum):
            name, _, size = header(index)
            end = names.find(b"\x00", name)
            if end < 0:
                continue
            sizes[names[name:end].decode("ascii", errors="replace")] = size
        return sizes
    except (OSError, struct.error, IndexError):
        return {}


def static_ram(search_dir):
    """Static RAM usage of the newest firmware ELF under search_dir, in bytes, or None."""
    elfs = [path for path in Path(search_dir).rglob("*.elf") if BOOTLOADER_WORD not in str(path).lower()]
    if not elfs:
        return None
    sizes = elf_section_sizes(max(elfs, key=lambda path: path.stat().st_mtime))
    found = [sizes[name] for name in RAM_SECTIONS if name in sizes]
    return sum(found) if found else None


def find_build(search_dir):
    """(image, partition table) of the newest build under search_dir, or (None, None)."""
    images = [path for name in IMAGE_NAMES for path in Path(search_dir).rglob(name)]
    if not images:
        return None, None
    image = max(images, key=lambda path: path.stat().st_mtime)
    # The partition table of the same build: search upwards from the image
    for directory in [image.parent, *image.parents]:
        for name in TABLE_NAMES:
            tables = sorted(directory.rglob(name))
            if tables:
                return image, tables[0]
        if directory == Path(search_dir).resolve() or directory == Path(search_dir):
            break
    return image, None


def main():
    parser = argparse.ArgumentParser(description="Fail when an ESPHome build uses too much of its app partition.")
    parser.add_argument("--name", required=True)
    parser.add_argument("--search-dir", required=True)
    parser.add_argument("--summary", help="append a Markdown line to this file")
    parser.add_argument("--output", help="write the sizes to this file as JSON")
    args = parser.parse_args()

    excluded = any(word in args.name for word in EXCLUDED)
    image, table = find_build(args.search_dir)
    if image is None or table is None:
        print(f"::warning title=Build size::{args.name}: build output not found, size not checked")
        return 0
    partition = smallest_app_partition(table)
    if not partition:
        print(f"::warning title=Build size::{args.name}: no app partition in {table.name}, size not checked")
        return 0

    used = image.stat().st_size
    percent = 100 * used / partition
    if args.output:
        # Written before the checks below, so a build that is too large is reported too
        with open(args.output, "w", encoding="utf-8") as f:
            json.dump({"name": args.name, "flash_used": used, "flash_total": partition,
                       "ram_used": static_ram(args.search_dir)}, f)
    message = f"{args.name}: flash {percent:.1f}% ({used:,} of {partition:,} bytes of the app partition)"
    failed = False
    if excluded:
        print(f"{message}; not checked")
    elif percent >= FAIL_PERCENT:
        print(f"::error title=Build size::{message} is at or above {FAIL_PERCENT:.0f}%")
        failed = True
    elif percent >= WARN_PERCENT:
        print(f"::warning title=Build size::{message} is at or above {WARN_PERCENT:.0f}%")
    else:
        print(message)

    if args.summary:
        note = " (not checked)" if excluded else ""
        with open(args.summary, "a", encoding="utf-8") as f:
            f.write(f"**Build size**: flash {percent:.1f}% ({used:,} of {partition:,} bytes){note}. "
                    f"Warning at {WARN_PERCENT:.0f}%, failure at {FAIL_PERCENT:.0f}%.\n")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
