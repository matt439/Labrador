"""Release/development identity through isolated Git and compiler-free CMake consumers."""

from __future__ import annotations

import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest import mock

from tools.tests.tool_fixtures import isolate_repository, isolated_git_environment

REPOSITORY = Path(__file__).resolve().parents[2]


class VersionTests(unittest.TestCase):
    def setUp(self):
        self.cmake = shutil.which("cmake")
        if not self.cmake or not shutil.which("git"):
            self.skipTest("CMake and Git are required for offline version fixtures")
        temporary = tempfile.TemporaryDirectory(prefix="labrador-version-")
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        self.source = root / "engine"
        self.source.mkdir()
        self.consumer = root / "consumer"
        self.consumer.mkdir()
        environment, hooks = isolated_git_environment(root, "Version fixture")
        isolation = mock.patch.dict(os.environ, environment, clear=True)
        isolation.start()
        self.addCleanup(isolation.stop)
        for relative in ("cmake/version.cmake", "cmake/source_revision.cmake",
                         "cmake/version_values.h.in", "cmake/release_identity.txt", ".gitattributes"):
            target = self.source / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(REPOSITORY / relative, target)
        (self.source / "vcpkg.json").write_text('{"version-string":"0.3.0"}\n', encoding="utf-8")
        (self.source / "CMakeLists.txt").write_text(
            'include(cmake/version.cmake)\nlabrador_set_version("${CMAKE_CURRENT_SOURCE_DIR}")\n'
            'project(Labrador VERSION "${LABRADOR_VERSION}" LANGUAGES NONE)\n'
            'configure_file(cmake/version_values.h.in "${CMAKE_CURRENT_BINARY_DIR}/values.h" @ONLY)\n',
            encoding="utf-8")
        self.git("init", "-q", "--initial-branch=fixture")
        isolate_repository(self.git, hooks)
        self.commit()

    def git(self, *arguments):
        result = subprocess.run(["git", "--no-pager", "-C", str(self.source), *arguments],
                                capture_output=True, timeout=30, check=False)
        self.assertEqual(result.returncode, 0, result.stderr.decode("utf-8", errors="replace"))
        return result.stdout.decode("utf-8").strip()

    def commit(self):
        self.git("add", "--all")
        self.git("commit", "-q", "-m", "fixture", "--no-gpg-sign")
        return self.git("rev-parse", "HEAD")

    def configure(self, source=None, *, extra=(), success=True):
        source = source or self.source
        (self.consumer / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.26)\nproject(VersionConsumer NONE)\n'
            f'add_subdirectory("{source.as_posix()}" engine)\n'
            'file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/facts.json" '
            '"{\\"version\\":\\"${LABRADOR_VERSION}\\",'
            '\\"string\\":\\"${LABRADOR_VERSION_STRING}\\",'
            '\\"major\\":${LABRADOR_VERSION_MAJOR},'
            '\\"minor\\":${LABRADOR_VERSION_MINOR},'
            '\\"patch\\":${LABRADOR_VERSION_PATCH},'
            '\\"development\\":\\"${LABRADOR_VERSION_IS_DEVELOPMENT}\\",'
            '\\"revision\\":\\"${LABRADOR_SOURCE_REVISION}\\"}")\n', encoding="utf-8")
        result = subprocess.run([self.cmake, "-S", str(self.consumer), "-B", str(self.consumer / "build"),
                                 *extra], capture_output=True, timeout=30, check=False)
        output = result.stdout.decode("utf-8", errors="replace") + result.stderr.decode("utf-8", errors="replace")
        if not success:
            self.assertNotEqual(result.returncode, 0, output)
            return output
        self.assertEqual(result.returncode, 0, output)
        return json.loads((self.consumer / "build/facts.json").read_text(encoding="utf-8"))

    def test_exact_clean_tag_is_release_but_dirty_or_between_tags_is_development(self):
        original = self.git("rev-parse", "HEAD")
        facts = self.configure()
        self.assertEqual(facts, {"version": "0.3.0", "string": "0.3.0-dev", "major": 0,
                                 "minor": 3, "patch": 0, "development": "ON", "revision": original})
        self.git("tag", "v0.3.0")
        self.assertEqual(self.configure()["string"], "0.3.0")
        (self.source / "tracked.txt").write_text("modified\n", encoding="utf-8")
        # Untracked files do not change source_revision's dirty contract.
        self.assertEqual(self.configure()["development"], "OFF")
        self.commit()
        self.assertEqual(self.configure()["string"], "0.3.0-dev")
        self.git("tag", "-f", "v0.3.0")
        self.assertEqual(self.configure()["development"], "OFF")
        (self.source / "tracked.txt").write_text("changed again\n", encoding="utf-8")
        facts = self.configure()
        self.assertEqual(facts["development"], "ON")
        self.assertTrue(facts["revision"].endswith("-dirty"))

    def test_a_different_exact_tag_does_not_claim_the_manifest_release(self):
        self.git("tag", "v0.2.0")
        self.assertEqual(self.configure()["string"], "0.3.0-dev")

    def test_tagged_archive_keeps_package_release_but_source_revision_is_unknown(self):
        self.git("tag", "v0.3.0")
        archive = self.source.parent / "release.tar"
        self.git("archive", "--format=tar", f"--output={archive}", "HEAD")
        unpacked = self.source / "nested-archive"
        unpacked.mkdir()
        # Extract only the fixture's own archive, not any external payload.
        shutil.unpack_archive(archive, unpacked, format="tar", filter="data")
        facts = self.configure(unpacked)
        self.assertEqual((facts["string"], facts["development"], facts["revision"]),
                         ("0.3.0", "OFF", "unknown"))

    def test_unstamped_or_wrong_tag_archive_is_development(self):
        unpacked = self.source / "plain-copy"
        shutil.copytree(self.source / "cmake", unpacked / "cmake")
        for name in ("CMakeLists.txt", "vcpkg.json"):
            shutil.copyfile(self.source / name, unpacked / name)
        self.assertEqual(self.configure(unpacked)["string"], "0.3.0-dev")
        identity = unpacked / "cmake/release_identity.txt"
        identity.write_text("commit=" + "a" * 40 + "\nrefs=tag: v0.3.1\n", encoding="ascii")
        self.assertEqual(self.configure(unpacked)["development"], "ON")
        identity.write_text("commit=" + "a" * 40 + "\nrefs=tag: v0.3.0-other\n", encoding="ascii")
        self.assertEqual(self.configure(unpacked)["development"], "ON")
        identity.write_text("commit=" + "a" * 40 + "\nrefs=tag: v0.3.0,other\n", encoding="ascii")
        self.assertEqual(self.configure(unpacked)["development"], "ON")
        identity.write_text("commit=unknown\nrefs=tag: v0.3.0\n", encoding="ascii")
        self.assertEqual(self.configure(unpacked)["development"], "ON")

    def test_missing_git_is_development_even_after_a_successful_release_configure(self):
        self.git("tag", "v0.3.0")
        self.assertEqual(self.configure()["development"], "OFF")
        facts = self.configure(extra=("-DCMAKE_DISABLE_FIND_PACKAGE_Git=ON",))
        self.assertEqual((facts["string"], facts["revision"]), ("0.3.0-dev", "unknown"))

    def test_numeric_version_reconfigures_without_stale_values_and_rejects_malformed_input(self):
        self.configure()
        manifest = self.source / "vcpkg.json"
        manifest.write_text('{"version-string":"1.22.333"}\n', encoding="utf-8")
        facts = self.configure()
        self.assertEqual((facts["major"], facts["minor"], facts["patch"], facts["string"]),
                         (1, 22, 333, "1.22.333-dev"))
        for invalid in ("01.2.3", "1.2", "1.2.3-dev", "1.2.3.4", "1.2.4294967296"):
            manifest.write_text(json.dumps({"version-string": invalid}), encoding="utf-8")
            self.assertIn("version", self.configure(success=False))


if __name__ == "__main__":
    unittest.main()
