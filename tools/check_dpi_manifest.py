"""Check that Windows programs embed a manifest declaring PerMonitorV2 first.

samples/minimal/dpi_aware.manifest is listed with each sample's sources and
the Get Started project's, and the linker merges it into the manifest it
embeds as resource 1 of type RT_MANIFEST. This reads that resource from the
built file and fails, naming the program, if it is absent or its
dpiAwareness does not start with PerMonitorV2. It reads the file; it does not
run the program or prove what Windows does with the declaration on a scaled
display.

  python tools/check_dpi_manifest.py PATH [PATH ...]

A PATH that is a file is checked whatever its subsystem. A PATH that is a
folder is searched for .exe files, and each Windows-subsystem (GUI) program
in it is checked; console programs, such as test runners, are counted and
skipped. A folder holding no GUI program fails, so a moved build cannot
pass by checking nothing. Standard library only.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import struct
import sys
import xml.etree.ElementTree as ElementTree

RT_MANIFEST = 24
PROGRAM_MANIFEST_ID = 1
SUBSYSTEM_WINDOWS_GUI = 2
RESOURCE_DIRECTORY = 2
WINDOWS_SETTINGS_2016 = "http://schemas.microsoft.com/SMI/2016/WindowsSettings"
REQUIRED = "PerMonitorV2"


class ManifestError(ValueError):
    """The file is not a program whose manifest can be read, or says otherwise."""


class PortableExecutable:
    """The parts of a PE image this check reads: the subsystem and resources."""

    def __init__(self, data: bytes):
        self.data = data
        if len(data) < 0x40 or data[:2] != b"MZ":
            raise ManifestError("not a PE image (no MZ header)")
        header = self.unpack("<I", 0x3C)[0]
        if data[header:header + 4] != b"PE\0\0":
            raise ManifestError("not a PE image (no PE signature)")
        sections, optional_size = self.unpack("<H", header + 6)[0], self.unpack("<H", header + 20)[0]
        optional = header + 24
        magic = self.unpack("<H", optional)[0]
        if magic == 0x10B:
            directories_at = optional + 92
        elif magic == 0x20B:
            directories_at = optional + 108
        else:
            raise ManifestError(f"unknown optional header magic {magic:#x}")
        self.subsystem = self.unpack("<H", optional + 68)[0]
        count = self.unpack("<I", directories_at)[0]
        self.resource_rva = 0
        if count > RESOURCE_DIRECTORY:
            self.resource_rva = self.unpack("<I", directories_at + 4 + 8 * RESOURCE_DIRECTORY)[0]
        table = optional + optional_size
        self.sections = []
        for index in range(sections):
            start = table + 40 * index
            virtual_size, virtual_address, raw_size, raw_pointer = self.unpack("<IIII", start + 8)
            self.sections.append((virtual_address, max(virtual_size, raw_size), raw_pointer, raw_size))

    def unpack(self, layout: str, offset: int) -> tuple:
        size = struct.calcsize(layout)
        if offset < 0 or offset + size > len(self.data):
            raise ManifestError("truncated PE image")
        return struct.unpack_from(layout, self.data, offset)

    def offset(self, rva: int) -> int:
        for virtual_address, size, raw_pointer, raw_size in self.sections:
            if virtual_address <= rva < virtual_address + size:
                if rva - virtual_address >= raw_size:
                    raise ManifestError("resource data lies outside its section's file bytes")
                return raw_pointer + rva - virtual_address
        raise ManifestError(f"RVA {rva:#x} is in no section")

    def resource(self, type_id: int, name_id: int) -> bytes | None:
        """The first language's bytes of resource type_id/name_id, or None."""
        if not self.resource_rva:
            return None
        root = self.offset(self.resource_rva)

        def find(directory: int, wanted: int | None) -> int | None:
            named, numbered = self.unpack("<HH", directory + 12)
            for index in range(named + numbered):
                identifier, target = self.unpack("<II", directory + 16 + 8 * index)
                is_name = identifier & 0x80000000
                if wanted is None or (not is_name and identifier == wanted):
                    return target
            return None

        level = root
        for wanted in (type_id, name_id, None):
            target = find(level, wanted)
            if target is None:
                return None
            if wanted is None and not target & 0x80000000:
                level = root + target
                break
            if not target & 0x80000000:
                raise ManifestError("resource tree ends before its language level")
            level = root + (target & 0x7FFFFFFF)
        else:
            raise ManifestError("resource tree is deeper than type, name and language")
        data_rva, size = self.unpack("<II", level)
        start = self.offset(data_rva)
        if start + size > len(self.data):
            raise ManifestError("truncated manifest resource")
        return self.data[start:start + size]


def dpi_awareness(manifest: bytes) -> str | None:
    try:
        root = ElementTree.fromstring(manifest)
    except ElementTree.ParseError as error:
        raise ManifestError(f"the embedded manifest is not XML: {error}") from error
    element = root.find(f".//{{{WINDOWS_SETTINGS_2016}}}dpiAwareness")
    return None if element is None or element.text is None else element.text


def check_program(data: bytes) -> str:
    """The declared dpiAwareness, or a ManifestError saying what is wrong."""
    image = PortableExecutable(data)
    manifest = image.resource(RT_MANIFEST, PROGRAM_MANIFEST_ID)
    if manifest is None:
        raise ManifestError("embeds no program manifest (RT_MANIFEST resource 1)")
    declared = dpi_awareness(manifest)
    if declared is None:
        raise ManifestError("its manifest declares no dpiAwareness")
    first = declared.split(",")[0].strip()
    if first.lower() != REQUIRED.lower():
        raise ManifestError(f"its dpiAwareness is {declared.strip()!r}, which does not start with {REQUIRED}")
    return declared.strip()


def programs(path: Path) -> tuple[list[Path], int]:
    """The GUI programs a folder holds, and how many console ones it skipped."""
    found, skipped = [], 0
    for candidate in sorted(path.rglob("*.exe")):
        try:
            subsystem = PortableExecutable(candidate.read_bytes()).subsystem
        except ManifestError:
            subsystem = None
        if subsystem == SUBSYSTEM_WINDOWS_GUI:
            found.append(candidate)
        else:
            skipped += 1
    return found, skipped


def check_paths(paths: list[Path], report=print, complain=None) -> int:
    complain = complain or (lambda line: print(line, file=sys.stderr))
    failed = False
    for path in paths:
        if path.is_dir():
            found, skipped = programs(path)
            if not found:
                complain(f"{path}: no Windows-subsystem program found to check")
                failed = True
                continue
            report(f"{path}: {len(found)} Windows programs, {skipped} console programs skipped")
        elif path.is_file():
            found = [path]
        else:
            complain(f"{path}: no such file or folder")
            failed = True
            continue
        for program in found:
            try:
                declared = check_program(program.read_bytes())
            except (ManifestError, OSError) as error:
                complain(f"{program}: {error}")
                failed = True
            else:
                report(f"{program}: dpiAwareness {declared}")
    return 1 if failed else 0


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("paths", nargs="+", type=Path, help="a program, or a folder of them")
    options = parser.parse_args(arguments)
    return check_paths(options.paths)


if __name__ == "__main__":
    raise SystemExit(main())
