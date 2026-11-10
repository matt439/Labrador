"""Hold tools/consumer_check/rename_check.py to the guide it reads and to small projects.

Nothing here configures or builds. On Windows one case runs the guide's own
PowerShell line, through cmd, on the walkthrough's build files.
"""

from __future__ import annotations

import contextlib
import io
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

from tools.tests.tool_fixtures import load_tool

REPOSITORY = Path(__file__).resolve().parents[2]
rename_check = load_tool("tools/consumer_check/rename_check.py")
assemble = load_tool("tools/consumer_check/assemble.py")

GUIDE = (REPOSITORY / rename_check.GUIDE).read_text(encoding="utf-8")

SMALL_GUIDE = """
1. Choose:

   ```bat
   set "NAME=SkyHopper"
   ```

2. Replace:

   ```bat
   powershell -NoProfile -Command "'Replaced MyGame ' + $n + ' times.'"
   ```

   ```text title="Output"
   Replaced MyGame 3 times.
   ```

   Run again, the line prints `Replaced MyGame 0 times.` and changes nothing.

   ```bat
   cmake --preset x64-debug
   ```
"""

LISTING = """Test project C:/dev/hopper/out/build/x64-debug
  Test #1: SkyHopperTests
  Test #2: SkyHopperSmokeTests

Total Tests: 2
"""


def write_project(root: Path, game_name: str) -> None:
    """A project with the three places a small starter names the game, and the two that never do."""
    files = {
        "CMakeLists.txt": f"project({game_name} LANGUAGES CXX)\n",
        "game/CMakeLists.txt": f"add_executable({game_name} WIN32 main.cpp)\nadd_test(NAME {game_name}Tests COMMAND {game_name}Tests)\n",
        "game/main.cpp": "int main() {}\n",
        "CMakePresets.json": "{}\n",
        ".github/workflows/ci.yml": "name: ci\n",
        "vcpkg.json": json.dumps({"name": game_name.lower(), "version-string": "0.1.0"}, indent=2) + "\n",
    }
    for name, content in files.items():
        (root / name).parent.mkdir(parents=True, exist_ok=True)
        (root / name).write_text(content, encoding="utf-8")


def write_build(folder: Path, name: str) -> None:
    (folder / "game").mkdir(parents=True)
    (folder / "game" / f"{name}.exe").write_bytes(b"")
    (folder / "CMakeCache.txt").write_text(f"CMAKE_GENERATOR:INTERNAL=Ninja\nCMAKE_PROJECT_NAME:STATIC={name}\n",
                                           encoding="utf-8")


class GuideReadingTests(unittest.TestCase):
    def test_fenced_lines_are_those_of_one_language(self):
        self.assertEqual(rename_check.fenced_lines(SMALL_GUIDE, "text"), ["Replaced MyGame 3 times."])
        self.assertEqual(len(rename_check.fenced_lines(SMALL_GUIDE, "bat")), 3)

    def test_name_line_and_count_are_read_from_the_blocks(self):
        self.assertEqual(rename_check.guide_name(SMALL_GUIDE), "SkyHopper")
        self.assertEqual(rename_check.guide_command(SMALL_GUIDE),
                         "powershell -NoProfile -Command \"'Replaced MyGame ' + $n + ' times.'\"")
        self.assertEqual(rename_check.guide_count(SMALL_GUIDE), 3)

    def test_a_guide_without_the_line_or_with_two_is_refused(self):
        without = SMALL_GUIDE.replace("powershell", "pwsh")
        twice = SMALL_GUIDE + '\n```bat\nset "NAME=Other"\n```\n'
        with self.assertRaises(rename_check.Refusal) as refused:
            rename_check.guide_command(without)
        self.assertIn("has 0 of", str(refused.exception))
        with self.assertRaises(rename_check.Refusal) as refused:
            rename_check.guide_name(twice)
        self.assertIn("has 2 of", str(refused.exception))

    def test_the_published_guide_has_one_name_one_line_and_one_count(self):
        name = rename_check.guide_name(GUIDE)
        command = rename_check.guide_command(GUIDE)
        self.assertRegex(name, r"^[A-Za-z][A-Za-z0-9]*$")
        self.assertNotEqual(name, rename_check.OLD_NAME)
        self.assertIn("$env:NAME", command)
        for relative in rename_check.RENAMED:
            self.assertIn(relative.replace("/", "\\"), command)
        self.assertIn(rename_check.MANIFEST, command)
        self.assertGreater(rename_check.guide_count(GUIDE), 0)

    def test_the_published_guide_builds_as_this_check_does(self):
        # build() runs these two commands and ctest; step 5 must still print them.
        lines = rename_check.fenced_lines(GUIDE, "bat")
        name = rename_check.guide_name(GUIDE)
        preset = rename_check.PRESET
        self.assertIn("rmdir /s /q out", lines)
        self.assertIn(f"cmake --preset {preset}", lines)
        self.assertIn(f"cmake --build --preset {preset} && ctest --preset {preset} && "
                      f"out\\build\\{preset}\\game\\{name}.exe", lines)

    def test_the_guides_count_is_the_walkthroughs(self):
        walkthrough = REPOSITORY / assemble.WALKTHROUGH
        placed = {to: source for source, to in assemble.PLACED.items()}
        count = sum((walkthrough / placed[relative]).read_text(encoding="utf-8").count(rename_check.OLD_NAME)
                    for relative in rename_check.RENAMED)
        self.assertEqual(count, rename_check.guide_count(GUIDE))
        for relative in rename_check.NEVER_NAMED:
            self.assertNotIn(rename_check.OLD_NAME, (walkthrough / placed[relative]).read_text(encoding="utf-8"))


class SourceProblemTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="labrador-rename-")
        self.addCleanup(temporary.cleanup)
        self.project = Path(temporary.name)

    def test_a_whole_rename_has_no_problems(self):
        write_project(self.project, "SkyHopper")
        self.assertEqual(rename_check.source_problems(self.project, "SkyHopper", 4), [])

    def test_a_line_left_behind_is_named_with_its_number(self):
        write_project(self.project, "SkyHopper")
        with (self.project / "game/CMakeLists.txt").open("a", encoding="utf-8") as file:
            file.write("install(TARGETS MyGame)\n")
        problems = rename_check.source_problems(self.project, "SkyHopper", 4)
        self.assertEqual(problems, ["game/CMakeLists.txt:3 still names MyGame: install(TARGETS MyGame)"])

    def test_a_file_that_should_never_name_the_game_is_checked(self):
        write_project(self.project, "SkyHopper")
        (self.project / "CMakePresets.json").write_text('{"name": "MyGame"}\n', encoding="utf-8")
        problems = rename_check.source_problems(self.project, "SkyHopper", 4)
        self.assertEqual(problems, ['CMakePresets.json:1 still names MyGame: {"name": "MyGame"}'])

    def test_a_count_that_is_not_the_lines_and_a_manifest_with_capitals_fail(self):
        write_project(self.project, "SkyHopper")
        (self.project / "vcpkg.json").write_text('{"name": "SkyHopper"}\n', encoding="utf-8")
        problems = rename_check.source_problems(self.project, "SkyHopper", 5)
        self.assertEqual(len(problems), 2)
        self.assertIn("4 times, not the 5", problems[0])
        self.assertIn('not "skyhopper"', problems[1])

    def test_rename_runs_the_guides_line_twice_with_the_guides_name(self):
        write_project(self.project, "MyGame")
        calls = []

        def run(command: str, project: Path, name: str) -> tuple[int, str]:
            calls.append((command, project, name))
            if len(calls) == 1:
                shutil.rmtree(project)
                write_project(project, name)
                return 0, "Replaced MyGame 3 times."
            return 0, "Replaced MyGame 0 times."

        with contextlib.redirect_stdout(io.StringIO()):
            name, problems = rename_check.rename(self.project, SMALL_GUIDE, run)
        self.assertEqual(name, "SkyHopper")
        # The fixture names the game four times where the small guide says three.
        self.assertEqual(len(problems), 1)
        self.assertIn("4 times, not the 3", problems[0])
        self.assertEqual(calls, [(rename_check.guide_command(SMALL_GUIDE), self.project, "SkyHopper")] * 2)

    def test_rename_stops_at_a_report_that_is_not_the_guides(self):
        write_project(self.project, "MyGame")
        with contextlib.redirect_stdout(io.StringIO()):
            _, problems = rename_check.rename(self.project, SMALL_GUIDE, lambda *_: (0, "Replaced MyGame 2 times."))
        self.assertEqual(len(problems), 1)
        self.assertIn("'Replaced MyGame 3 times.'", problems[0])

    def test_a_copy_of_a_starter_leaves_its_out_folder_and_keeps_deeper_ones(self):
        starter, copy = self.project / "mygame", self.project / "hopper"
        write_project(starter, "MyGame")
        for folder in ("out/build", "game/out"):
            (starter / folder).mkdir(parents=True)
            (starter / folder / "kept.txt").write_bytes(b"")
        rename_check.copy_starter(starter, copy)
        self.assertFalse((copy / "out").exists())
        self.assertTrue((copy / "game/out/kept.txt").is_file())
        self.assertTrue((copy / "game/main.cpp").is_file())
        with self.assertRaises(rename_check.Refusal):
            rename_check.copy_starter(self.project / "hopper" / "game" / "out", self.project / "other")

    @unittest.skipUnless(sys.platform == "win32" and shutil.which("powershell"),
                         "the guide's line is PowerShell run by cmd")
    def test_the_guides_own_line_renames_the_walkthroughs_build_files(self):
        walkthrough = REPOSITORY / assemble.WALKTHROUGH
        write_project(self.project, "MyGame")
        for source, placed in assemble.PLACED.items():
            shutil.copyfile(walkthrough / source, self.project / placed)
        with contextlib.redirect_stdout(io.StringIO()) as printed:
            name, problems = rename_check.rename(self.project, GUIDE)
        self.assertEqual(problems, [], printed.getvalue())
        self.assertEqual(name, rename_check.guide_name(GUIDE))
        self.assertEqual(printed.getvalue().splitlines(),
                         [rename_check.report(rename_check.guide_count(GUIDE)), rename_check.report(0)])
        self.assertIn(f"project({name}", (self.project / "CMakeLists.txt").read_text(encoding="utf-8"))

    @unittest.skipUnless(sys.platform == "win32" and shutil.which("powershell"),
                         "the guide's line is PowerShell run by cmd")
    def test_the_guides_own_line_refuses_a_folder_that_is_not_a_project(self):
        code, output = rename_check.run_line(rename_check.guide_command(GUIDE), self.project, "SkyHopper")
        self.assertEqual((code, output), (1, "This is not your project's folder. Nothing was changed."))


class BuildProblemTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="labrador-rename-build-")
        self.addCleanup(temporary.cleanup)
        self.build = Path(temporary.name) / "mygame" / "out" / "build" / "x64-debug"

    def test_listed_tests_are_the_names_ctest_prints(self):
        self.assertEqual(rename_check.listed_tests(LISTING), ["SkyHopperTests", "SkyHopperSmokeTests"])

    def test_a_renamed_build_in_a_folder_called_mygame_has_no_problems(self):
        write_build(self.build, "SkyHopper")
        # Windows matches *MyGame* against this name too; only the capitals count.
        (self.build / "mygame").mkdir()
        self.assertEqual(rename_check.build_problems(self.build, "SkyHopper", LISTING), [])

    def test_old_tests_programs_and_project_name_are_each_named(self):
        write_build(self.build, "MyGame")
        (self.build / "game" / "MyGameTests.exe").write_bytes(b"")
        problems = rename_check.build_problems(self.build, "SkyHopper", LISTING.replace("SkyHopper", "MyGame"))
        self.assertEqual(problems, [
            "ctest lists MyGameTests, MyGameSmokeTests, not SkyHopperTests and SkyHopperSmokeTests.",
            "game/SkyHopper.exe was not built.",
            "game/MyGame.exe is still named MyGame.",
            "game/MyGameTests.exe is still named MyGame.",
            "CMakeCache.txt has no line CMAKE_PROJECT_NAME:STATIC=SkyHopper.",
        ])

    def test_an_engine_test_in_the_list_fails(self):
        write_build(self.build, "SkyHopper")
        listing = LISTING.replace("\nTotal", "  Test #3: MathTests\n\nTotal")
        problems = rename_check.build_problems(self.build, "SkyHopper", listing)
        self.assertEqual(len(problems), 1)
        self.assertIn("MathTests", problems[0])


class RefusalTests(unittest.TestCase):
    @unittest.skipUnless(sys.platform == "win32", "elsewhere the check refuses before it looks at the folder")
    def test_a_folder_with_files_in_it_is_refused(self):
        with tempfile.TemporaryDirectory() as temporary:
            (Path(temporary) / "kept.txt").write_bytes(b"")
            with self.assertRaises(rename_check.Refusal) as refused:
                rename_check.main([temporary, "--no-build"])
            self.assertIn("is not empty", str(refused.exception))
            self.assertTrue((Path(temporary) / "kept.txt").is_file())


if __name__ == "__main__":
    unittest.main()
