"""The DPI manifest check against PE images built here, byte by byte."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

REPOSITORY = Path(__file__).resolve().parents[2]
SCRIPT = REPOSITORY / "tools/check_dpi_manifest.py"
SPEC = importlib.util.spec_from_file_location("check_dpi_manifest", SCRIPT)
checker = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(checker)

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


def manifest(awareness: str | None) -> bytes:
    settings = "" if awareness is None else (
        f'<dpiAwareness xmlns="{checker.WINDOWS_SETTINGS_2016}">{awareness}</dpiAwareness>')
    return (f'<?xml version="1.0" encoding="UTF-8" standalone="yes"?>'
            f'<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">'
            f'<application xmlns="urn:schemas-microsoft-com:asm.v3"><windowsSettings>{settings}'
            f'</windowsSettings></application></assembly>').encode("utf-8")


class ProgramTests(unittest.TestCase):
    def test_the_template_manifest_embedded_passes_in_both_image_formats(self):
        for plus in (True, False):
            with self.subTest(plus=plus):
                declared = checker.check_program(image([(24, 1, TEMPLATE)], plus=plus))
                self.assertTrue(declared.startswith("PerMonitorV2"))

    def test_the_manifest_is_found_beside_other_resources(self):
        data = image([(3, 1, b"\0" * 40), (16, 1, b"version"), (24, 1, manifest("PerMonitorV2"))])
        self.assertEqual(checker.check_program(data), "PerMonitorV2")

    def test_a_missing_or_wrong_declaration_fails_saying_which(self):
        cases = {
            "no program manifest": image([(3, 1, b"icon")]),
            "no program manifest ": image(resources=False),
            "declares no dpiAwareness": image([(24, 1, manifest(None))]),
            "'PerMonitor, PerMonitorV2'": image([(24, 1, manifest("PerMonitor, PerMonitorV2"))]),
            "'System'": image([(24, 1, manifest("System"))]),
            "not XML": image([(24, 1, b"<assembly")]),
        }
        for expected, data in cases.items():
            with self.subTest(expected=expected):
                with self.assertRaisesRegex(checker.ManifestError, expected.strip()):
                    checker.check_program(data)

    def test_an_isolation_manifest_of_another_id_is_not_the_programs(self):
        # Resource 2 is a DLL's isolation manifest; Windows reads 1 for a program.
        with self.assertRaisesRegex(checker.ManifestError, "no program manifest"):
            checker.check_program(image([(24, 2, TEMPLATE)]))

    def test_case_and_spacing_of_the_value_follow_windows(self):
        self.assertEqual(checker.check_program(image([(24, 1, manifest("  permonitorv2 ,PerMonitor "))])),
                         "permonitorv2 ,PerMonitor")

    def test_not_a_program_and_truncation_are_errors_not_passes(self):
        for data in (b"", b"MZ" + b"\0" * 10, b"MZ" + b"\0" * 0x3A + struct.pack("<I", 0x40) + b"NE\0\0",
                     image([(24, 1, TEMPLATE)])[:0x180]):
            with self.subTest(length=len(data)):
                with self.assertRaises(checker.ManifestError):
                    checker.check_program(data)


class FolderTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="labrador-dpi-")
        self.addCleanup(temporary.cleanup)
        self.folder = Path(temporary.name)
        self.lines, self.complaints = [], []

    def write(self, relative, data):
        path = self.folder / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        return path

    def run_check(self, *paths):
        return checker.check_paths(list(paths), report=self.lines.append, complain=self.complaints.append)

    def test_a_folder_checks_its_windows_programs_and_skips_console_ones(self):
        self.write("a/Game.exe", image([(24, 1, TEMPLATE)]))
        self.write("a/GameTests.exe", image(subsystem=3))
        self.write("b/Other.exe", image([(24, 1, TEMPLATE)]))
        self.write("b/not_a_program.exe", b"text")
        self.assertEqual(self.run_check(self.folder), 0, self.complaints)
        self.assertIn(f"{self.folder}: 2 Windows programs, 2 console programs skipped", self.lines)

    def test_one_bad_program_fails_the_folder_and_is_named(self):
        self.write("Game.exe", image([(24, 1, TEMPLATE)]))
        bad = self.write("Stretched.exe", image([(3, 1, b"icon")]))
        self.assertEqual(self.run_check(self.folder), 1)
        self.assertEqual(len(self.complaints), 1)
        self.assertTrue(self.complaints[0].startswith(str(bad)))

    def test_a_folder_with_no_program_and_a_missing_path_fail(self):
        self.write("Tests.exe", image(subsystem=3))
        self.assertEqual(self.run_check(self.folder, self.folder / "absent.exe"), 1)
        self.assertEqual(len(self.complaints), 2)

    def test_a_named_file_is_checked_whatever_its_subsystem(self):
        console = self.write("Tool.exe", image(subsystem=3))
        self.assertEqual(self.run_check(console), 1)
        self.assertIn("no program manifest", self.complaints[0])


if __name__ == "__main__":
    unittest.main()
