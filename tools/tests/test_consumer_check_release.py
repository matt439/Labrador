"""Hold tools/consumer_check/check_release.py to the list it reads and to a folder."""

from __future__ import annotations

import contextlib
import io
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


if __name__ == "__main__":
    unittest.main()
