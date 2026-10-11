#!/usr/bin/env python3
"""Parse the production HID descriptor and check exact button/X/Y bit layout."""
from __future__ import annotations

import re
import sys
from pathlib import Path


def main() -> None:
    source = Path(sys.argv[1]).read_text(encoding="utf-8")
    match = re.search(
        r"static const uint8_t s_hid_report_descriptor\[\]\s*=\s*\{(.*?)\};",
        source,
        flags=re.DOTALL,
    )
    assert match, "production descriptor missing"
    stripped = re.sub(r"/\*.*?\*/", "", match.group(1), flags=re.DOTALL)
    data = [int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]+", stripped)]
    assert len(data) == 50, len(data)
    assert data[2:6] == [0x09, 0x02, 0xA1, 0x01]
    size = count = page = bits = 0
    items = []
    pos = 0
    while pos < len(data):
        prefix = data[pos]
        pos += 1
        assert prefix != 0xFE
        n = (0, 1, 2, 4)[prefix & 3]
        assert pos + n <= len(data)
        value = int.from_bytes(bytes(data[pos:pos + n]), "little")
        pos += n
        kind, tag = (prefix >> 2) & 3, prefix >> 4
        if kind == 1 and tag == 0:
            page = value
        elif kind == 1 and tag == 7:
            size = value
        elif kind == 1 and tag == 9:
            count = value
        elif kind == 1 and tag == 8:
            raise AssertionError("Unexpected report ID")
        elif kind == 0 and tag == 8:
            assert size > 0 and count > 0
            items.append((bits, size * count, page, value))
            bits += size * count
    assert items == [
        (0, 2, 0x09, 0x02),
        (2, 6, 0x09, 0x03),
        (8, 16, 0x01, 0x06),
    ], items
    assert bits == 24
    print("HID descriptor: 2 button bits + 6 padding + 16 signed XY bits")


if __name__ == "__main__":
    main()
