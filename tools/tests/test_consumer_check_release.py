"""Hold tools/consumer_check/check_release.py to the list it reads and to a folder."""

from __future__ import annotations

import contextlib
import io
import struct
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from tools.tests import pe_fixtures as images
from tools.tests.tool_fixtures import load_tool

REPOSITORY = Path(__file__).resolve().parents[2]
check_release = load_tool("tools/consumer_check/check_release.py")


class CheckReleaseTests(unittest.TestCase):
    def test_list_is_paths_without_comments_or_descriptions(self):
        listing = "# a comment\n\nMyGame.exe  The game.\nfonts\\OFL.txt  The licence.\n"
        self.assertEqual(check_release.listed_files(listing), {"mygame.exe", "fonts/ofl.txt"})

    def test_walkthrough_list_names_the_program_and_its_dlls(self):
        listed = check_release.listed_files((REPOSITORY / check_release.LIST).read_text(encoding="utf-8"))
        for name in ("mygame.exe", "directxtk.dll", "xaudio2_9redist.dll", "vcruntime140.dll",
                     "vcruntime140_1.dll", "msvcp140.dll", "manifest.json"):
            self.assertIn(name, listed)

    def test_folder_matching_the_list_has_no_differences(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            (folder / "fonts").mkdir()
            (folder / "MyGame.exe").write_bytes(b"")
            (folder / "fonts/OFL.txt").write_bytes(b"")
            installed = check_release.installed_files(folder)
            self.assertEqual(check_release.differences({"mygame.exe", "fonts/ofl.txt"}, installed), [])

    def test_missing_and_unlisted_files_are_each_named(self):
        problems = check_release.differences({"mygame.exe", "manifest.json"}, {"mygame.exe", "mygame.pdb"})
        self.assertEqual(problems, ["missing: manifest.json", "not in the list: mygame.pdb"])


    def test_an_installed_game_without_the_dpi_declaration_fails(self):
        with tempfile.TemporaryDirectory() as temporary:
            engine, folder = Path(temporary, "engine"), Path(temporary, "release")
            listing = engine / check_release.LIST
            listing.parent.mkdir(parents=True)
            listing.write_text("MyGame.exe  The game.\n", encoding="utf-8")
            folder.mkdir()
            program = folder / "MyGame.exe"
            for data, expected in ((images.image([(24, 1, images.TEMPLATE)]), 0),
                                   (images.image([(3, 1, b"icon")]), 1)):
                with self.subTest(expected=expected):
                    program.write_bytes(data)
                    arguments = ["check_release.py", str(folder), "--engine", str(engine)]
                    with mock.patch("sys.argv", arguments), contextlib.redirect_stdout(io.StringIO()), \
                            contextlib.redirect_stderr(io.StringIO()) as errors:
                        self.assertEqual(check_release.main(), expected, errors.getvalue())

    def test_optional_icon_flag_checks_metadata_and_default_stays_unchanged(self):
        with tempfile.TemporaryDirectory() as temporary:
            engine, folder = Path(temporary, "engine"), Path(temporary, "release")
            listing = engine / check_release.LIST
            listing.parent.mkdir(parents=True)
            listing.write_text("MyGame.exe  The game.\n", encoding="utf-8")
            folder.mkdir()
            program = folder / "MyGame.exe"
            group = struct.pack("<HHHBBBBHHIH", 0, 1, 1, 16, 16, 0, 0, 1, 32, 8, 7)
            valid = images.image([(24, 1, images.TEMPLATE), (14, "GAME_ICON", group), (3, 7, b"12345678")])
            missing = images.image([(24, 1, images.TEMPLATE)])
            malformed = images.image([(24, 1, images.TEMPLATE), (14, "GAME_ICON", b"bad")])
            cases = [(valid, ["--icon-resource", "GAME_ICON"], 0, "referenced image payload"),
                     (missing, ["--icon-resource", "GAME_ICON"], 1, "no named RT_GROUP_ICON"),
                     (malformed, ["--icon-resource", "GAME_ICON"], 1, "truncated group header"),
                     (malformed, [], 0, "dpiAwareness"),
                     (missing, [], 0, "dpiAwareness")]
            for data, extra, expected, complaint in cases:
                with self.subTest(extra=extra, expected=expected):
                    program.write_bytes(data)
                    arguments = ["check_release.py", str(folder), "--engine", str(engine), *extra]
                    output, errors = io.StringIO(), io.StringIO()
                    with mock.patch("sys.argv", arguments), contextlib.redirect_stdout(output), \
                            contextlib.redirect_stderr(errors):
                        self.assertEqual(check_release.main(), expected, errors.getvalue())
                    self.assertIn(complaint, output.getvalue() + errors.getvalue())

    def test_empty_explicit_icon_name_is_not_silently_ignored(self):
        with mock.patch("sys.argv", ["check_release.py", ".", "--icon-resource", ""]), \
                contextlib.redirect_stderr(io.StringIO()) as errors:
            with self.assertRaises(SystemExit) as failure:
                check_release.main()
        self.assertEqual(failure.exception.code, 2)
        self.assertIn("nonempty resource name", errors.getvalue())


if __name__ == "__main__":
    unittest.main()
