#!/usr/bin/env python3
"""Check linked production descriptors in the normal and diagnostic ELFs.

Run from the pinned ESP-IDF Python environment, which supplies pyelftools.
This checks compiled bytes; it does not establish physical enumeration.
"""

import argparse
from pathlib import Path

from elftools.elf.elffile import ELFFile


def symbol_bytes(path: Path, name: str) -> bytes:
    with path.open("rb") as stream:
        elf = ELFFile(stream)
        symbols = elf.get_section_by_name(".symtab").get_symbol_by_name(name)
        if not symbols or len(symbols) != 1:
            raise ValueError(f"{path.name}: require one linked {name}")
        symbol = symbols[0]
        section = elf.get_section(symbol["st_shndx"])
        start = symbol["st_value"] - section["sh_addr"]
        size = symbol["st_size"]
        data = section.data()[start:start + size]
        if not size or len(data) != size:
            raise ValueError("Invalid linked descriptor extent")
        return data


def check_configuration(data: bytes, diagnostic: bool) -> bytes:
    expected_count = 3 if diagnostic else 1
    if (len(data) < 9 or data[:2] != b"\x09\x02" or
            int.from_bytes(data[2:4], "little") != len(data) or
            data[4] != expected_count):
        raise ValueError("Configuration length/interface count mismatch")
    interfaces = []
    endpoints = []
    hid = None
    offset = 0
    while offset < len(data):
        length = data[offset]
        if length < 2 or offset + length > len(data):
            raise ValueError("Invalid descriptor boundary")
        descriptor = data[offset:offset + length]
        kind = descriptor[1]
        if kind == 4:
            if length != 9 or descriptor[3] != 0:
                raise ValueError("Unexpected interface layout")
            interfaces.append((descriptor[2], descriptor[5]))
        elif kind == 5:
            if length != 7:
                raise ValueError("Unexpected endpoint layout")
            endpoints.append(descriptor[2])
            if descriptor[2] == 0x81:
                # Preserve the normal mouse's interrupt endpoint and cadence.
                if descriptor != bytes.fromhex("07058103080008"):
                    raise ValueError("Mouse endpoint changed")
        elif kind == 0x21:
            if hid is not None:
                raise ValueError("Multiple HID descriptors")
            hid = descriptor
        offset += length
    expected_interfaces = [(0, 3), (1, 2), (2, 10)] if diagnostic else [(0, 3)]
    expected_endpoints = [0x81, 0x82, 0x03, 0x83] if diagnostic else [0x81]
    if interfaces != expected_interfaces or endpoints != expected_endpoints or hid is None:
        raise ValueError("Interface class/endpoint ownership mismatch")
    return hid


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--usb", type=Path, required=True)
    parser.add_argument("--diagnostic", type=Path, required=True)
    args = parser.parse_args()
    hid_descriptors = []
    reports = []
    for path, diagnostic in ((args.usb, False), (args.diagnostic, True)):
        hid_descriptors.append(check_configuration(
            symbol_bytes(path, "s_configuration_descriptor"), diagnostic))
        reports.append(symbol_bytes(path, "s_hid_report_descriptor"))
    if hid_descriptors[0] != hid_descriptors[1] or reports[0] != reports[1]:
        raise ValueError("Diagnostic mode changed the mouse report contract")
    if int.from_bytes(hid_descriptors[0][7:9], "little") != len(reports[0]):
        raise ValueError("HID descriptor report length mismatch")
    print("PASS: linked normal/diagnostic mouse bytes, interfaces and endpoints")


if __name__ == "__main__":
    main()
