"""Hold tools/consumer_check/starter_zip.py, and assemble.py's project, to a small engine tree."""

from __future__ import annotations

import contextlib
import io
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

from tools.tests.tool_fixtures import load_tool

REPOSITORY = Path(__file__).resolve().parents[2]


assemble = load_tool("tools/consumer_check/assemble.py")
starter_zip = load_tool("tools/consumer_check/starter_zip.py")

# A commit made at a fixed time, so the ZIP's entry times are known.
COMMIT_SECONDS = 1767225600  # 2026-01-01 00:00:00 UTC
FIXTURE = {
    "CMakeLists.txt": b"project(Labrador)\n",
    "LICENSE": b"licence\n",
    "vcpkg.json": json.dumps({
        "name": "labrador",
        "version-string": "0.3.0",
        "dependencies": ["directxtk", {"name": "lodepng", "platform": "windows"}, "doctest"],
        "builtin-baseline": "0123456789abcdef0123456789abcdef01234567",
    }).encode("utf-8"),
    "engine/core/thing.h": b"#pragma once\n",
    "samples/minimal/main.cpp": b'#include "samples/minimal/states/hello.h"\r\n#include "engine/app/application.h"\r\nint main() {}\r\n',
    "samples/minimal/states/hello.h": b'#pragma once\n#include "samples/minimal/rules/motion.h"\n',
    "samples/minimal/rules/motion.h": b"#pragma once\n",
    "samples/minimal/CMakeLists.txt": b"the sample's own build\n",
    "samples/minimal/README.md": b"# Minimal sample\nThe original template change list.\n",
    "samples/minimal/content/greeting.json": b'{"text": "samples/minimal/"}\n',
    "website/src/walkthrough/root-CMakeLists.txt": b"root\n",
    "website/src/walkthrough/game-CMakeLists.txt": b"game\n",
    "website/src/walkthrough/game-README.md": b"# MyGame\nEdit game/.\n",
    "website/src/walkthrough/game-comments.json": b"{}\n",
    "website/src/walkthrough/CMakePresets.json": b"{}\n",
    "website/src/walkthrough/gitignore.txt": b"out/\n",
    "website/src/walkthrough/ci.yml": b"name: ci\n",
    "website/src/walkthrough/labrador.vsconfig": b'{"version":"1.0","components":["Fixture.Cpp"]}\n',
    "website/src/walkthrough/helpers/build.cmd": b"build helper\n",
    "website/src/walkthrough/helpers/test.cmd": b"test helper\n",
    "website/src/walkthrough/helpers/play.cmd": b"play helper\n",
    "website/src/walkthrough/helpers/starter_commands.ps1": b"shared discovery\n",
    ".gitignore": b"out/\n",
}


def git(tree: Path, *arguments: str, **environment: str) -> None:
    subprocess.run(
        ["git", "-C", str(tree), "-c", "user.name=Fixture", "-c", "user.email=fixture@example.com",
         "-c", "core.autocrlf=false", *arguments],
        check=True, capture_output=True, env={**os.environ, **environment},
    )


@unittest.skipUnless(shutil.which("git"), "git is required to list the engine tree")
class StarterZipTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="labrador-starter-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.engine = self.root / "engine-tree"
        for name, content in FIXTURE.items():
            (self.engine / name).parent.mkdir(parents=True, exist_ok=True)
            (self.engine / name).write_bytes(content)
        git(self.engine, "init", "--quiet")
        git(self.engine, "add", ".")
        when = f"@{COMMIT_SECONDS} +0000"
        git(self.engine, "commit", "--quiet", "-m", "fixture", GIT_AUTHOR_DATE=when, GIT_COMMITTER_DATE=when)
        # Ignored, so neither the engine copy nor the ZIP holds it.
        (self.engine / "out").mkdir()
        (self.engine / "out/build.log").write_bytes(b"ignored\n")

    def quietly(self, call, *arguments):
        with contextlib.redirect_stdout(io.StringIO()):
            return call(*arguments)

    def write(self, name: str, release: str | None = None) -> Path:
        destination = self.root / name
        entries = self.quietly(starter_zip.starter_files, self.engine, release)
        starter_zip.write_zip(destination, entries, starter_zip.commit_time(self.engine))
        return destination

    def test_project_files_are_the_project_without_the_engine(self):
        files = self.quietly(assemble.project_files, self.engine)
        self.assertEqual(sorted(files), [
            ".github/workflows/ci.yml", ".gitignore", "CMakeLists.txt", "CMakePresets.json",
            "build.cmd",
            "game/CMakeLists.txt",
            "game/README.md",
            "game/content/greeting.json",
            "game/main.cpp",
            "game/rules/motion.h",
            "game/states/hello.h",
            "labrador.vsconfig",
            "play.cmd",
            "test.cmd",
            "tools/starter_commands.ps1",
            "vcpkg.json",
        ])
        self.assertEqual(files["game/CMakeLists.txt"], b"game\n")
        self.assertEqual(files["game/README.md"], FIXTURE["website/src/walkthrough/game-README.md"])
        self.assertEqual(files["CMakeLists.txt"], b"root\n")
        self.assertEqual(files["tools/starter_commands.ps1"], b"shared discovery\n")

    def test_optional_installer_config_is_copied_unchanged_without_changing_presets(self):
        files = self.quietly(assemble.project_files, self.engine)
        self.assertEqual(files["labrador.vsconfig"], FIXTURE["website/src/walkthrough/labrador.vsconfig"])
        self.assertEqual(files["CMakePresets.json"], FIXTURE["website/src/walkthrough/CMakePresets.json"])
        with zipfile.ZipFile(self.write("starter.zip")) as archive:
            self.assertEqual(archive.read("mygame/labrador.vsconfig"), files["labrador.vsconfig"])

    def test_includes_are_edited_and_line_endings_kept(self):
        files = self.quietly(assemble.project_files, self.engine)
        self.assertEqual(
            files["game/main.cpp"],
            b'#include "game/states/hello.h"\r\n#include "engine/app/application.h"\r\nint main() {}\r\n',
        )
        self.assertEqual(files["game/states/hello.h"], b'#pragma once\n#include "game/rules/motion.h"\n')
        # Only .h and .cpp files are edited.
        self.assertEqual(files["game/content/greeting.json"], FIXTURE["samples/minimal/content/greeting.json"])

    def test_a_template_with_no_include_to_edit_is_refused(self):
        (self.engine / "samples/minimal/main.cpp").write_bytes(b"int main() {}\n")
        (self.engine / "samples/minimal/states/hello.h").write_bytes(b"#pragma once\n")
        with self.assertRaises(SystemExit):
            self.quietly(assemble.project_files, self.engine)

    def test_comment_edits_keep_code_strings_indentation_and_line_endings(self):
        before = b'\t// MinimalRules\r\nconst char* title = "MinimalRules";\r\n// unlisted\r\n'
        after = assemble.edited_comments(before, {"// MinimalRules": "// MyGameRules"})
        self.assertEqual(after, b'\t// MyGameRules\r\nconst char* title = "MinimalRules";\r\n// unlisted\r\n')
        html = b'<!--\n  MinimalSample.html\n-->\n<script>"MinimalSample.html"</script>\n'
        self.assertEqual(assemble.edited_comments(html, {"MinimalSample.html": "MyGame.html"}, html=True),
                         b'<!--\n  MyGame.html\n-->\n<script>"MinimalSample.html"</script>\n')
        for replacement in ('code();', '// comment\ncode();', '// continuation\\'):
            with self.subTest(replacement=replacement), self.assertRaises(assemble.Refusal):
                assemble.edited_comments(b'// old\n', {"// old": replacement})
        with self.assertRaises(assemble.Refusal):
            assemble.edited_comments(b'<!--\nold\n-->\n', {"old": "--> <script>"}, html=True)

    def test_manifest_carries_every_engine_package(self):
        manifest = json.loads(self.quietly(assemble.project_files, self.engine)["vcpkg.json"])
        self.assertEqual(manifest, {
            "name": "mygame",
            "version-string": "0.1.0",
            "dependencies": ["directxtk", {"name": "lodepng", "platform": "windows"}, "doctest"],
            "builtin-baseline": "0123456789abcdef0123456789abcdef01234567",
        })

    def test_zip_has_one_top_folder_holding_project_engine_and_readme(self):
        with zipfile.ZipFile(self.write("starter.zip")) as archive:
            names = archive.namelist()
            self.assertEqual({name.split("/")[0] for name in names}, {"mygame"})
            engine = sorted(name[len("mygame/external/labrador/"):] for name in names
                            if name.startswith("mygame/external/labrador/"))
            self.assertEqual(engine, sorted(FIXTURE))
            self.assertNotIn("mygame/external/labrador/out/build.log", names)
            self.assertIn("mygame/README.txt", names)
            self.assertEqual(archive.read("mygame/game/main.cpp").splitlines()[0], b'#include "game/states/hello.h"')
            self.assertEqual(archive.read("mygame/external/labrador/samples/minimal/main.cpp"),
                             FIXTURE["samples/minimal/main.cpp"])
            self.assertEqual(archive.read("mygame/external/labrador/samples/minimal/README.md"),
                             FIXTURE["samples/minimal/README.md"])
            self.assertEqual(archive.read("mygame/game/README.md"),
                             FIXTURE["website/src/walkthrough/game-README.md"])
            self.assertEqual(json.loads(archive.read("mygame/vcpkg.json"))["name"], "mygame")
            self.assertEqual(names, sorted(names))

    def test_same_tree_gives_the_same_bytes_and_entries_carry_the_commit_time(self):
        first = self.write("first.zip").read_bytes()
        second = self.write("second.zip").read_bytes()
        self.assertEqual(first, second)
        with zipfile.ZipFile(self.root / "first.zip") as archive:
            self.assertEqual({info.date_time for info in archive.infolist()}, {(2026, 1, 1, 0, 0, 0)})
            self.assertEqual({info.compress_type for info in archive.infolist()}, {zipfile.ZIP_DEFLATED})

    def test_readme_names_the_release_or_says_it_is_not_one(self):
        with zipfile.ZipFile(self.write("release.zip", "v0.3.0")) as archive:
            text = archive.read("mygame/README.txt").decode("ascii")
        self.assertIn("Labrador version: v0.3.0\r\n", text)
        self.assertIn(r"game\states\hello_state.cpp", text)
        self.assertIn(r"external\labrador\samples\minimal\README.md", text)
        self.assertIn("    https://labradorengine.com/versions/v0.3.0/docs/get-started/\r\n", text)
        self.assertLess(text.index("/versions/v0.3.0/"), text.index(f"    {starter_zip.GET_STARTED}\r\n"))
        self.assertTrue(all(len(line) <= 80 for line in text.splitlines() if not line.startswith("Labrador ")))
        with zipfile.ZipFile(self.write("dev.zip")) as archive:
            text = archive.read("mygame/README.txt").decode("ascii")
        self.assertIn("not a release (its vcpkg.json says 0.3.0)", text)
        self.assertNotIn("uncommitted", text)  # out/ is ignored
        self.assertIn(f"    {starter_zip.GET_STARTED}\r\n", text)
        self.assertNotIn("/versions/", text)
        self.assertTrue(all(len(line) <= 80 for line in text.splitlines() if not line.startswith("Labrador ")))
        (self.engine / "engine/core/thing.h").write_bytes(b"#pragma once\n// edited\n")
        with zipfile.ZipFile(self.write("edited.zip")) as archive:
            self.assertIn(", with uncommitted changes", archive.read("mygame/README.txt").decode("ascii"))

    def test_release_must_be_the_tagged_clean_tree_at_its_version(self):
        with self.assertRaisesRegex(SystemExit, "no tag"):
            starter_zip.check_release(self.engine, "v0.3.0")
        git(self.engine, "tag", "v0.3.0")
        starter_zip.check_release(self.engine, "v0.3.0")
        with self.assertRaisesRegex(SystemExit, "version-string"):
            starter_zip.check_release(self.engine, "v0.4.0")
        with self.assertRaisesRegex(SystemExit, "not a release tag"):
            starter_zip.check_release(self.engine, "0.3.0")
        (self.engine / "new.txt").write_bytes(b"untracked\n")
        with self.assertRaisesRegex(SystemExit, "clean checkout"):
            starter_zip.check_release(self.engine, "v0.3.0")

    def test_command_line_names_the_zip_by_release(self):
        git(self.engine, "tag", "v0.3.0")
        script = REPOSITORY / "tools/consumer_check/starter_zip.py"
        output = self.root / "assets"
        result = subprocess.run(
            [sys.executable, str(script), str(output), "--engine", str(self.engine), "--release", "v0.3.0"],
            capture_output=True, text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue((output / "labrador-starter-v0.3.0.zip").is_file())
        self.assertIn("sha256", result.stdout)
        with zipfile.ZipFile(output / "labrador-starter-v0.3.0.zip") as archive:
            identity = archive.read("mygame/external/labrador/cmake/release_identity.txt").decode("ascii")
        self.assertRegex(identity, r"^commit=[0-9a-f]{40}\nrefs=tag: v0\.3\.0\n$")

    def test_assemble_writes_the_same_project_as_the_zip_holds(self):
        project = self.root / "mygame"
        self.quietly(assemble.assemble, self.engine, project)
        on_disk = sorted(path.relative_to(project).as_posix() for path in project.rglob("*") if path.is_file())
        with zipfile.ZipFile(self.write("starter.zip")) as archive:
            zipped = sorted(name[len("mygame/"):] for name in archive.namelist())
            for name in on_disk:
                self.assertEqual((project / name).read_bytes(), archive.read("mygame/" + name), name)
        self.assertEqual(sorted(on_disk + ["README.txt"]), zipped)


@unittest.skipUnless(shutil.which("git"), "git is required to list the actual checkout")
class AssembledSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with contextlib.redirect_stdout(io.StringIO()):
            cls.project = assemble.project_files(REPOSITORY)

    def test_every_comment_rule_matches_the_current_template(self):
        rules = json.loads((REPOSITORY / assemble.WALKTHROUGH / "game-comments.json").read_text(encoding="utf-8"))
        for relative, replacements in rules.items():
            original = (REPOSITORY / "samples/minimal" / relative).read_text(encoding="utf-8")
            lines = [line.lstrip(" \t") for line in original.splitlines()]
            edited = self.project["game/" + relative].decode("utf-8")
            edited_lines = [line.lstrip(" \t") for line in edited.splitlines()]
            for before, after in replacements.items():
                with self.subTest(file=relative, comment=before):
                    self.assertEqual(lines.count(before), 1)
                    self.assertNotIn(before, edited_lines)
                    self.assertIn(after, edited_lines)

    def test_game_owned_comments_name_the_assembled_targets_and_paths(self):
        self.assertIn("game/CMakeLists.txt, MyGameSmokeTests", self.project["game/main.cpp"].decode("utf-8"))
        self.assertIn("MyGameRules", self.project["game/rules/motion.h"].decode("utf-8"))
        self.assertIn("game/CMakeLists.txt, MyGameTests", self.project["game/tests/motion_tests.cpp"].decode("utf-8"))
        self.assertIn("MyGame.html beside MyGame.js and MyGame.wasm", self.project["game/web/shell.html"].decode("utf-8"))
        self.assertEqual(self.project["game/README.md"],
                         (REPOSITORY / assemble.WALKTHROUGH / "game-README.md").read_bytes())

    @unittest.skipUnless(shutil.which("node"), "Node is required to compare the website's source model")
    def test_guide_anchor_sources_match_the_real_assembled_project(self):
        helper = (REPOSITORY / "website/tests/helpers/starter-sources.mjs").as_uri()
        script = f'import {{ starterSources }} from {json.dumps(helper)}; process.stdout.write(JSON.stringify([...starterSources(process.argv[1])]));'
        result = subprocess.run(["node", "--input-type=module", "-e", script, str(REPOSITORY)],
                                capture_output=True, text=True, encoding="utf-8", check=True)
        modeled = dict(json.loads(result.stdout))
        expected = {name: data.decode("utf-8").replace("\r\n", "\n") for name, data in self.project.items()
                    if Path(name).suffix in {".h", ".cpp", ".html", ".json", ".md", ".txt", ".manifest", ".yml"}
                    or name == ".gitignore" or name.endswith("CMakeLists.txt")}
        self.assertEqual(modeled, expected)


@unittest.skipUnless(shutil.which("git"), "git is required to ask which paths it ignores")
class StarterGitignoreTests(unittest.TestCase):
    """The .gitignore the project gets keeps the build and Visual Studio's own folder out of `git add .`."""

    def test_project_gitignore_ignores_build_and_visual_studio_folders_only(self):
        with tempfile.TemporaryDirectory(prefix="labrador-gitignore-") as name:
            project = Path(name)
            target = assemble.PLACED["gitignore.txt"]
            shutil.copyfile(REPOSITORY / assemble.WALKTHROUGH / "gitignore.txt", project / target)
            git(project, "init", "--quiet")
            ignored = ["out/build/x64-debug/game/MyGame.exe", ".vs/slnx.sqlite", ".vs/mygame/v18/.wsuo"]
            kept = ["game/main.cpp", "game/states/hello_state.cpp", "CMakePresets.json", "vcpkg.json",
                    "labrador.vsconfig", "external/labrador/engine/core/state.h", ".github/workflows/ci.yml", ".gitignore"]
            result = subprocess.run(
                ["git", "-C", str(project), "check-ignore", "--no-index", "--stdin"],
                input="".join(path + "\n" for path in ignored + kept).encode("utf-8"), capture_output=True,
            )
            self.assertEqual(result.stdout.decode("utf-8").split(), ignored, result.stderr)


class InstallerSelectionTests(unittest.TestCase):
    def test_optional_configuration_and_command_match_the_isolated_install_record(self):
        record = (REPOSITORY / "docs/review/2026-11-10-first-run/2026-11-13-visual-studio-install.md").read_text(encoding="utf-8")
        recorded = re.search(r"```json\n(.*?)\n```", record, re.DOTALL)
        self.assertIsNotNone(recorded)
        actual = (REPOSITORY / assemble.WALKTHROUGH / "labrador.vsconfig").read_text(encoding="utf-8")
        self.assertEqual(json.loads(actual), json.loads(recorded.group(1)))
        command = "VisualStudioSetup.exe --passive --wait --norestart --config labrador.vsconfig"
        guide = (REPOSITORY / "website/src/content/docs/docs/get-started/prerequisites.mdx").read_text(encoding="utf-8")
        self.assertIn(command, record)
        self.assertIn('start "" /wait ' + command, guide)

    @unittest.skipUnless(sys.platform == "win32" and Path(sys.executable).with_name("pythonw.exe").is_file(),
                         "the Windows GUI stand-in requires pythonw.exe")
    def test_documented_shell_wrapper_waits_for_gui_exit_and_preserves_installer_arguments(self):
        guide = (REPOSITORY / "website/src/content/docs/docs/get-started/prerequisites.mdx").read_text(encoding="utf-8")
        command = next(line.strip() for line in guide.splitlines() if line.strip().startswith('start "" /wait VisualStudioSetup.exe'))
        with tempfile.TemporaryDirectory(prefix="labrador-setup-wait-") as folder:
            root = Path(folder)
            standin = root / "standin.py"
            receipt = root / "receipt.json"
            standin.write_text(
                "import json,sys,time\nfrom pathlib import Path\ntime.sleep(0.1)\n"
                "Path(sys.argv[1]).write_text(json.dumps(sys.argv[2:]))\nsys.exit(37)\n", encoding="utf-8")
            executable = Path(sys.executable).with_name("pythonw.exe")
            wrapped = command.replace("VisualStudioSetup.exe",
                                      '"%LABRADOR_SETUP_TEST_EXE%" "%LABRADOR_SETUP_TEST_SCRIPT%" "%LABRADOR_SETUP_TEST_RECEIPT%"', 1)
            script = root / "wait.cmd"
            script.write_text('@echo off\n' + wrapped + '\necho RESULT:%errorlevel%\n', encoding="ascii")
            environment = {**os.environ, "LABRADOR_SETUP_TEST_EXE": str(executable),
                           "LABRADOR_SETUP_TEST_SCRIPT": str(standin), "LABRADOR_SETUP_TEST_RECEIPT": str(receipt)}
            run = subprocess.run(["cmd", "/d", "/c", str(script)], capture_output=True, text=True,
                                 timeout=10, check=True, env=environment)
            self.assertIn("RESULT:37", run.stdout)
            self.assertEqual(json.loads(receipt.read_text()),
                             ["--passive", "--wait", "--norestart", "--config", "labrador.vsconfig"])


if __name__ == "__main__":
    unittest.main()
