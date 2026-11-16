"""Opt-in icon input stays in the assembled copy and preserves game sources."""

import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from tools.tests.tool_fixtures import load_tool


fixture = load_tool("tools/consumer_check/icon_fixture.py")
ROOT = Path(__file__).resolve().parents[2]


def project_files(project: Path) -> None:
    (project / "game").mkdir(parents=True)
    (project / "CMakeLists.txt").write_text("add_subdirectory(game)\n", encoding="utf-8")
    (project / "game/CMakeLists.txt").write_text("set(GAME_ICON_FILE)\n", encoding="utf-8")
    (project / "game/main.cpp").write_text(
        '#if defined(GAME_HAS_ICON_RESOURCE)\noptions.window_icon_resource = L"GAME_ICON";\n#endif\n', encoding="utf-8")
    (project / "game/game_icon.rc").write_text("GAME_ICON ICON GAME_ICON_FILE\n", encoding="utf-8")


class IconFixtureTests(unittest.TestCase):
    def test_copies_only_test_input_and_changes_no_code_or_embedded_engine(self):
        with tempfile.TemporaryDirectory() as directory:
            project = Path(directory) / "mygame"
            project_files(project)
            before = {path.relative_to(project): path.read_bytes() for path in project.rglob("*") if path.is_file()}
            copied = fixture.prepare(project, ROOT)
            self.assertEqual(copied, project / fixture.PLACED)
            self.assertEqual(copied.read_bytes(), (ROOT / fixture.ICON).read_bytes())
            after = {path.relative_to(project): path.read_bytes() for path in project.rglob("*") if path.is_file()}
            self.assertEqual(set(after) - set(before), {fixture.PLACED})
            for name, contents in before.items():
                self.assertEqual(after[name], contents)

    def test_refuses_an_old_or_incomplete_starter_before_writing(self):
        with tempfile.TemporaryDirectory() as directory:
            project = Path(directory) / "mygame"
            project_files(project)
            (project / "game/game_icon.rc").unlink()
            with self.assertRaisesRegex(ValueError, "current starter"):
                fixture.prepare(project, ROOT)
            self.assertFalse((project / fixture.PLACED).exists())

    def test_does_not_overwrite_an_existing_fixture_or_authored_file(self):
        with tempfile.TemporaryDirectory() as directory:
            project = Path(directory) / "mygame"
            project_files(project)
            copied = fixture.prepare(project, ROOT)
            copied.write_bytes(b"owner data")
            with self.assertRaisesRegex(ValueError, "already exists"):
                fixture.prepare(project, ROOT)
            self.assertEqual(copied.read_bytes(), b"owner data")

    def test_the_configured_entry_must_enable_the_explicit_request(self):
        with tempfile.TemporaryDirectory() as directory:
            project = Path(directory) / "mygame"
            project_files(project)
            build = project / "out"
            build.mkdir()
            for command, accepted in [("cl.exe -DGAME_HAS_ICON_RESOURCE=1 main.cpp", True),
                                      ('cl.exe "/DGAME_HAS_ICON_RESOURCE=1" main.cpp', True),
                                      ("cl.exe -DUNRELATED=1 main.cpp", False)]:
                with self.subTest(command=command):
                    (build / "compile_commands.json").write_text(json.dumps([{
                        "directory": str(project), "file": "game/main.cpp", "command": command}]), encoding="utf-8")
                    if accepted:
                        fixture.check_configured(project, build)
                    else:
                        with self.assertRaisesRegex(ValueError, "missing GAME_HAS_ICON_RESOURCE"):
                            fixture.check_configured(project, build)
            (build / "compile_commands.json").write_text("[]", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "no native game/main.cpp"):
                fixture.check_configured(project, build)


def icon_option_block(text: str) -> str:
    """The actual native opt-in block, including its nested validation checks."""
    lines = text.splitlines()
    start = lines.index('if(NOT "${GAME_ICON_FILE}" STREQUAL "")')
    depth = 0
    for end in range(start, len(lines)):
        line = lines[end].strip()
        if line.startswith("if("):
            depth += 1
        elif line.startswith("endif("):
            depth -= 1
            if depth == 0:
                return "\n".join(lines[start:end + 1]) + "\n"
    raise AssertionError("the optional icon block has no matching endif")


@unittest.skipUnless(shutil.which("cmake"), "CMake must be on PATH for the compiler-free recipe checks")
class IconRecipeValidationTests(unittest.TestCase):
    def test_actual_recipe_blocks_refuse_nonempty_bad_paths_and_accept_only_an_empty_opt_out(self):
        cmake = shutil.which("cmake")
        for relative in ("samples/minimal/CMakeLists.txt", "website/src/walkthrough/game-CMakeLists.txt"):
            with self.subTest(recipe=relative), tempfile.TemporaryDirectory() as directory:
                folder = Path(directory)
                wrong_type = folder / "not-an-icon.txt"
                wrong_type.write_text("fixture", encoding="utf-8")
                directory_icon = folder / "folder.ico"
                directory_icon.mkdir()
                block = icon_option_block((ROOT / relative).read_text(encoding="utf-8"))
                script = folder / "validate.cmake"
                script.write_text(block + 'message(STATUS "EMPTY_ICON_RECIPE")\n', encoding="utf-8")
                for value, complaint in [("OFF", "absolute path"), ("relative.ico", "absolute path"),
                                          (str(folder / "missing.ico"), "existing .ico file"),
                                          (str(wrong_type), "must name an .ico file"),
                                          (str(directory_icon), "existing .ico file")]:
                    result = subprocess.run([cmake, f"-DGAME_ICON_FILE={value}", "-P", str(script)],
                                            capture_output=True, text=True)
                    self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertIn("GAME_ICON_FILE", result.stdout + result.stderr)
                    self.assertIn(complaint, result.stdout + result.stderr)
                empty = subprocess.run([cmake, "-DGAME_ICON_FILE=", "-P", str(script)],
                                       capture_output=True, text=True)
                self.assertEqual(empty.returncode, 0, empty.stdout + empty.stderr)
                self.assertIn("EMPTY_ICON_RECIPE", empty.stdout)


if __name__ == "__main__":
    unittest.main()
