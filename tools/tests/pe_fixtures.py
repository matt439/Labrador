"""Windows PE images built byte by byte, for the DPI manifest and release checks."""

from __future__ import annotations

from pathlib import Path
import struct

REPOSITORY = Path(__file__).resolve().parents[2]
TEMPLATE = (REPOSITORY / "samples/minimal/dpi_aware.manifest").read_bytes()
SECTION_RVA = 0x1000
SECTION_FILE = 0x200


def resource_section(entries: list[tuple[int, int, bytes]]) -> bytes:
    """A .rsrc body for (type, name, bytes) entries, one language each."""
    def directory(ids_and_targets):
        out = struct.pack("<IIHHHH", 0, 0, 0, 0, 0, len(ids_and_targets))
        for identifier, target in ids_and_targets:
            out += struct.pack("<II", identifier, target)
        return out

    types = sorted({type_id for type_id, _, _ in entries})
    # Layout: root, then per type a name directory, then per entry a language
    # directory and a data entry, then the data.
    root_size = 16 + 8 * len(types)
    names_size = {t: 16 + 8 * sum(1 for e in entries if e[0] == t) for t in types}
    offset = root_size
    name_offsets = {}
    for t in types:
        name_offsets[t] = offset
        offset += names_size[t]
    language_offsets, data_entry_offsets = [], []
    for _ in entries:
        language_offsets.append(offset)
        offset += 24
        data_entry_offsets.append(offset)
        offset += 16
    data_offsets = []
    for _, _, payload in entries:
        data_offsets.append(offset)
        offset += (len(payload) + 7) & ~7
    body = directory([(t, 0x80000000 | name_offsets[t]) for t in types])
    for t in types:
        body += directory([(e[1], 0x80000000 | language_offsets[i])
                           for i, e in enumerate(entries) if e[0] == t])
    for index, (_, _, payload) in enumerate(entries):
        body += directory([(0x409, data_entry_offsets[index])])
        body += struct.pack("<IIII", SECTION_RVA + data_offsets[index], len(payload), 0, 0)
    for _, _, payload in entries:
        body += payload + b"\0" * (((len(payload) + 7) & ~7) - len(payload))
    return body


def image(entries=None, subsystem=2, plus=True, resources=True) -> bytes:
    """A PE32(+) image with one section holding the resource tree."""
    section = resource_section(entries or []) if resources else b""
    raw_size = (len(section) + 0x1FF) & ~0x1FF
    optional_size = 240 if plus else 224
    header = b"MZ" + b"\0" * 0x3A + struct.pack("<I", 0x40)
    coff = b"PE\0\0" + struct.pack("<HHIIIHH", 0x8664 if plus else 0x14C, 1, 0, 0, 0, optional_size, 0x22)
    optional = bytearray(optional_size)
    struct.pack_into("<H", optional, 0, 0x20B if plus else 0x10B)
    struct.pack_into("<H", optional, 68, subsystem)
    directories = 112 if plus else 96
    struct.pack_into("<I", optional, directories - 4, 16)
    if resources:
        struct.pack_into("<II", optional, directories + 8 * 2, SECTION_RVA, len(section))
    table = struct.pack("<8sIIIIIIHHI", b".rsrc", len(section), SECTION_RVA, raw_size, SECTION_FILE,
                        0, 0, 0, 0, 0x40000040)
    head = header + coff + bytes(optional) + table
    return head + b"\0" * (SECTION_FILE - len(head)) + section + b"\0" * (raw_size - len(section))
