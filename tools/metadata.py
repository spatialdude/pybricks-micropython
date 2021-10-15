#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2019-2021 The Pybricks Authors

"""
Pybricks firmware metadata file generation tool.

Generates a .json file with information about a Pybricks firmware binary blob.

v1.2.0:

    metadata-version    "1.2.0"
    firmware-version    output of `git describe --tags --dirty`
    device-id           one of 0x40, 0x41, 0x80, 0x81
    checksum-type       one of "sum", "crc32"
    mpy-abi-version     number (MPY_VERSION)
    mpy-cross-options   array of string
    user-mpy-offset     number
    max-firmware-size   number
    hub-name-offset     number [v1.1.0]
    max-hub-name-size   number [v1.1.0]
    firmware-sha256     sha256 string of firmware.bin
"""

import argparse
import hashlib
import importlib
import io
import json
import os
import re
import sys
import typing

# Path to repo top-level dir.
TOP = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "micropython"))
sys.path.append(os.path.join(TOP, "py"))
sys.path.append(os.path.join(TOP, "tools"))
mpy_tool = importlib.import_module("mpy-tool")


# metadata file format version
VERSION = "1.1.0"

# hub-specific info
HUB_INFO = {
    "move_hub": {"device-id": 0x40, "checksum-type": "sum"},
    "city_hub": {"device-id": 0x41, "checksum-type": "sum"},
    "technic_hub": {"device-id": 0x80, "checksum-type": "sum"},
    "prime_hub": {"device-id": 0x81, "checksum-type": "crc32"},
    "essential_hub": {"device-id": 0x83, "checksum-type": "crc32"},
}


def generate(
    fw_version: str,
    hub_type: str,
    mpy_options: typing.List[str],
    map_file: io.FileIO,
    bin_file: io.FileIO,
    out_file: io.FileIO,
):
    metadata = {
        "metadata-version": VERSION,
        "firmware-version": fw_version,
        "mpy-abi-version": mpy_tool.config.MPY_VERSION,
        "mpy-cross-options": mpy_options,
        "firmware-sha256": hashlib.sha256(bin_file.read()).hexdigest(),
    }

    if hub_type not in HUB_INFO:
        print("Unknown hub type", file=sys.stderr)
        exit(1)

    metadata.update(HUB_INFO[hub_type])

    flash_origin = None  # Starting address of firmware area in flash memory
    flash_length = None  # Size of firmware area of flash memory
    name_start = None  # Starting address of custom hub name
    name_size = None  # Size reserved for custom hub name
    user_start = None  # Starting address of user .mpy file

    for line in map_file.readlines():
        match = re.match(r"^FLASH\s+(0x[0-9A-Fa-f]{8,16})\s+(0x[0-9A-Fa-f]{8,16})", line)
        if match:
            flash_origin = int(match[1], base=0)
            flash_length = int(match[2], base=0)
            continue

        match = re.match(r"^\.name\s+(0x[0-9A-Fa-f]{8,16})\s+(0x[0-9A-Fa-f]+)", line)
        if match:
            name_start = int(match[1], base=0)
            name_size = int(match[2], base=0)
            continue

        match = re.match(r"^\.user\s+(0x[0-9A-Fa-f]{8,16})", line)
        if match:
            user_start = int(match[1], base=0)
            continue

    if flash_origin is None:
        print("Failed to find 'FLASH' start address", file=sys.stderr)
        exit(1)

    if flash_length is None:
        print("Failed to find 'FLASH' length", file=sys.stderr)
        exit(1)

    if name_start is None:
        print("Failed to find '.name' start address", file=sys.stderr)
        exit(1)

    if user_start is None:
        print("Failed to find '.user' start address", file=sys.stderr)
        exit(1)

    metadata["user-mpy-offset"] = user_start - flash_origin
    metadata["max-firmware-size"] = flash_length
    metadata["hub-name-offset"] = name_start - flash_origin
    metadata["max-hub-name-size"] = name_size

    json.dump(metadata, out_file, indent=4, sort_keys=True)


if __name__ == "__main__":
    # want to ignore "-"" prefix so we can pass mpy-cross options but prefix_chars
    # cant be empty string so we use "+" as dummy value
    parser = argparse.ArgumentParser(description="Generate firmware metadata.", prefix_chars="+")
    parser.add_argument(
        "fw_version",
        metavar="<firmware-version>",
        type=str,
        help="Pybricks firmware version",
    )
    parser.add_argument(
        "hub_type",
        metavar="<hub-type>",
        choices=HUB_INFO.keys(),
        help="hub type/device ID",
    )
    parser.add_argument(
        "mpy_options",
        metavar="<mpy-cross-option>",
        nargs="*",
        type=str,
        help="mpy-cross option",
    )
    parser.add_argument(
        "map_file",
        metavar="<map-file>",
        type=argparse.FileType("r"),
        help="firmware linker map file name",
    )
    parser.add_argument(
        "bin_file",
        metavar="<bin-file>",
        type=argparse.FileType("rb"),
        help="firmware binary file name",
    )
    parser.add_argument(
        "out_file",
        metavar="<output-file>",
        type=argparse.FileType("w"),
        help="output file name",
    )

    args = parser.parse_args()
    generate(
        args.fw_version,
        args.hub_type,
        args.mpy_options,
        args.map_file,
        args.bin_file,
        args.out_file,
    )
