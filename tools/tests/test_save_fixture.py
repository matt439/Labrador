"""Optional save example copying never overwrites a reader's existing files."""
from pathlib import Path
import tempfile
import unittest

from tools.tests.tool_fixtures import load_tool

fixture = load_tool("tools/consumer_check/save_fixture.py")


class SaveFixtureTests(unittest.TestCase):
    def test_complete_source_is_checked_before_any_project_mutation(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            cmake = root / "CMakeLists.txt"
            original = "add_subdirectory(game)\n"
            cmake.write_text(original)
            with self.assertRaises(OSError):
                fixture.prepare(root)
            self.assertEqual(cmake.read_text(), original)
            self.assertFalse((root / "game/save_example").exists())

    def test_copied_sources_are_game_owned_and_native_browser_routes_are_explicit(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            (root / "CMakeLists.txt").write_text("add_subdirectory(game)\n")
            source = root / "external/labrador/tools/save_check"
            source.mkdir(parents=True)
            (root / "game").mkdir()
            for filename in fixture.FILES:
                (source / filename).write_text('#include "tools/save_check/recovery_settings.h"\n')
            fixture.prepare(root)
            copied = root / "game/save_example/recovery_settings.cpp"
            self.assertIn('#include "game/save_example/recovery_settings.h"', copied.read_text())
            self.assertIn('#include "tools/save_check/recovery_settings.h"', (source / "recovery_settings.cpp").read_text())
            result = (root / "CMakeLists.txt").read_text()
            self.assertIn("LabradorEngine labrador_settings", result)
            self.assertIn("-sENVIRONMENT=node -sNODERAWFS=1", result)
            self.assertIn("-sENVIRONMENT=web -sNODERAWFS=0", result)
            self.assertIn("recovery_persist.js", result)
            self.assertIn("add_test(NAME MyGameSaveTests", result)
            with self.assertRaises(ValueError):
                fixture.prepare(root)
            self.assertEqual((root / "CMakeLists.txt").read_text(), result)

    def test_existing_game_folder_and_nonstarter_are_preserved(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            cmake = root / "CMakeLists.txt"
            cmake.write_text("project(Other)\n")
            with self.assertRaises(ValueError):
                fixture.prepare(root)
            cmake.write_text("add_subdirectory(game)\n")
            owned = root / "game/save_example"
            owned.mkdir(parents=True)
            marker = owned / "reader.txt"
            marker.write_text("reader data")
            with self.assertRaises(ValueError):
                fixture.prepare(root)
            self.assertEqual(marker.read_text(), "reader data")


if __name__ == "__main__":
    unittest.main()
