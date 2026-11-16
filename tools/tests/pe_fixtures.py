"""Windows PE images built byte by byte, for the DPI manifest and release checks."""

from __future__ import annotations

from pathlib import Path
import struct

REPOSITORY = Path(__file__).resolve().parents[2]
TEMPLATE = (REPOSITORY / "samples/minimal/dpi_aware.manifest").read_bytes()
SECTION_RVA = 0x1000
SECTION_FILE = 0x200


def resource_section(entries: list[tuple[int | str, int | str, bytes]]) -> bytes:
    """A .rsrc body for (type, name, bytes) entries, one language each."""
    def order(identifier):
        return (0, identifier) if isinstance(identifier, str) else (1, identifier)

    types = sorted({type_id for type_id, _, _ in entries}, key=order)
    # Layout: root, type/name directories, language directories/data entries,
    # bounded UTF-16 names, then aligned opaque resource payloads.
    root_size = 16 + 8 * len(types)
    names_size = {t: 16 + 8 * sum(1 for e in entries if e[0] == t) for t in types}
    offset = root_size
    name_offsets = {}
    for resource_type in types:
        name_offsets[resource_type] = offset
        offset += names_size[resource_type]
    language_offsets, data_entry_offsets = [], []
    for _ in entries:
        language_offsets.append(offset)
        offset += 24
        data_entry_offsets.append(offset)
        offset += 16
    string_offsets, string_bytes = {}, {}
    identifiers = [*types, *(name for _, name, _ in entries)]
    for identifier in identifiers:
        if not isinstance(identifier, str) or identifier in string_offsets:
            continue
        encoded = identifier.encode("utf-16-le", errors="surrogatepass")
        string_offsets[identifier] = offset
        string_bytes[identifier] = struct.pack("<H", len(encoded) // 2) + encoded
        offset += len(string_bytes[identifier])
    offset = (offset + 7) & ~7
    data_offsets = []
    for _, _, payload in entries:
        data_offsets.append(offset)
        offset += (len(payload) + 7) & ~7
    body = bytearray(offset)

    def directory(at, ids_and_targets):
        ordered = sorted(ids_and_targets, key=lambda item: order(item[0]))
        named = sum(isinstance(identifier, str) for identifier, _ in ordered)
        struct.pack_into("<IIHHHH", body, at, 0, 0, 0, 0, named, len(ordered) - named)
        for index, (identifier, target) in enumerate(ordered):
            key = 0x80000000 | string_offsets[identifier] if isinstance(identifier, str) else identifier
            struct.pack_into("<II", body, at + 16 + 8 * index, key, target)

    directory(0, [(resource_type, 0x80000000 | name_offsets[resource_type]) for resource_type in types])
    for resource_type in types:
        directory(name_offsets[resource_type], [(entry[1], 0x80000000 | language_offsets[index])
                                               for index, entry in enumerate(entries) if entry[0] == resource_type])
    for index, (_, _, payload) in enumerate(entries):
        directory(language_offsets[index], [(0x409, data_entry_offsets[index])])
        struct.pack_into("<IIII", body, data_entry_offsets[index],
                         SECTION_RVA + data_offsets[index], len(payload), 0, 0)
        body[data_offsets[index]:data_offsets[index] + len(payload)] = payload
    for identifier, encoded in string_bytes.items():
        at = string_offsets[identifier]
        body[at:at + len(encoded)] = encoded
    return bytes(body)


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
