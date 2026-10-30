"""Hold tools/consumer_check/starter_zip.py, and assemble.py's project, to a small engine tree."""

from __future__ import annotations

import contextlib
import importlib.util
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[2]


def load(name: str):
    spec = importlib.util.spec_from_file_location(name, REPOSITORY / f"tools/consumer_check/{name}.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


assemble = load("assemble")
starter_zip = load("starter_zip")

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
    "samples/minimal/content/greeting.json": b'{"text": "samples/minimal/"}\n',
    "website/src/walkthrough/root-CMakeLists.txt": b"root\n",
    "website/src/walkthrough/game-CMakeLists.txt": b"game\n",
    "website/src/walkthrough/CMakePresets.json": b"{}\n",
    "website/src/walkthrough/gitignore.txt": b"out/\n",
    "website/src/walkthrough/ci.yml": b"name: ci\n",
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
            "game/CMakeLists.txt", "game/content/greeting.json", "game/main.cpp",
            "game/rules/motion.h", "game/states/hello.h", "vcpkg.json",
        ])
        self.assertEqual(files["game/CMakeLists.txt"], b"game\n")
        self.assertEqual(files["CMakeLists.txt"], b"root\n")

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

    def test_manifest_is_derived_without_the_test_only_package(self):
        manifest = json.loads(self.quietly(assemble.project_files, self.engine)["vcpkg.json"])
        self.assertEqual(manifest, {
            "name": "mygame",
            "version-string": "0.1.0",
            "dependencies": ["directxtk", "doctest"],
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
        self.assertIn(starter_zip.GET_STARTED, text)
        with zipfile.ZipFile(self.write("dev.zip")) as archive:
            text = archive.read("mygame/README.txt").decode("ascii")
        self.assertIn("not a release (its vcpkg.json says 0.3.0)", text)
        self.assertNotIn("uncommitted", text)  # out/ is ignored
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

    def test_assemble_writes_the_same_project_as_the_zip_holds(self):
        project = self.root / "mygame"
        self.quietly(assemble.assemble, self.engine, project)
        on_disk = sorted(path.relative_to(project).as_posix() for path in project.rglob("*") if path.is_file())
        with zipfile.ZipFile(self.write("starter.zip")) as archive:
            zipped = sorted(name[len("mygame/"):] for name in archive.namelist())
            for name in on_disk:
                self.assertEqual((project / name).read_bytes(), archive.read("mygame/" + name), name)
        self.assertEqual(sorted(on_disk + ["README.txt"]), zipped)


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
                    "external/labrador/engine/core/state.h", ".github/workflows/ci.yml", ".gitignore"]
            result = subprocess.run(
                ["git", "-C", str(project), "check-ignore", "--no-index", "--stdin"],
                input="".join(path + "\n" for path in ignored + kept).encode("utf-8"), capture_output=True,
            )
            self.assertEqual(result.stdout.decode("utf-8").split(), ignored, result.stderr)


if __name__ == "__main__":
    unittest.main()
