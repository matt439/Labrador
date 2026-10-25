"""Hold tools/consumer_check/check_release.py to the list it reads and to a folder."""

from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "check_release", REPOSITORY / "tools/consumer_check/check_release.py"
)
check_release = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(check_release)


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


if __name__ == "__main__":
    unittest.main()
