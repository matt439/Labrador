"""The DPI manifest check against PE images built here, byte by byte."""

from __future__ import annotations

from pathlib import Path
import struct
import tempfile
import unittest

from tools.tests.pe_fixtures import TEMPLATE, image
from tools.tests.tool_fixtures import load_tool

REPOSITORY = Path(__file__).resolve().parents[2]
checker = load_tool("tools/check_dpi_manifest.py")

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
        self.write("b/OtherTests.exe", image(subsystem=3))
        self.assertEqual(self.run_check(self.folder), 0, self.complaints)
        self.assertIn(f"{self.folder}: 2 Windows programs, 2 console programs skipped", self.lines)

    def test_one_bad_program_fails_the_folder_and_is_named(self):
        self.write("Game.exe", image([(24, 1, TEMPLATE)]))
        bad = self.write("Stretched.exe", image([(3, 1, b"icon")]))
        self.assertEqual(self.run_check(self.folder), 1)
        self.assertEqual(len(self.complaints), 1)
        self.assertTrue(self.complaints[0].startswith(str(bad)))

    def test_an_unparsable_program_in_a_folder_fails_and_is_named(self):
        self.write("Game.exe", image([(24, 1, TEMPLATE)]))
        truncated = self.write("Truncated.exe", image([(24, 1, TEMPLATE)])[:0x100])
        text = self.write("not_a_program.exe", b"text")
        self.assertEqual(self.run_check(self.folder), 1)
        self.assertEqual(sorted(complaint.split(": cannot be read as a program: ")[0]
                                for complaint in self.complaints),
                         sorted([str(truncated), str(text)]))
        self.assertIn(f"{self.folder}: 1 Windows programs, 0 console programs skipped", self.lines)

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
