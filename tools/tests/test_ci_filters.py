"""Exercise the Windows workflow's exclusions through CTest without running tests."""

from __future__ import annotations

import json
import os
import re
import shlex
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


class CiFilterTests(unittest.TestCase):
    names = {
        "RenderPixelTests", "MinimalSmokeTests", "RenderTests", "SceneTests",
        "PrefixRenderPixelTests", "RenderPixelTestsSuffix",
        "PrefixRenderPixelTestsSuffix", "RenderPixelTestsWithoutS3tc",
        "PrefixMinimalSmokeTests", "MinimalSmokeTestsSuffix",
        "PrefixMinimalSmokeTestsSuffix",
    }
    excluded = {"RenderPixelTests", "MinimalSmokeTests"}

    @classmethod
    def setUpClass(cls):
        cmake = os.environ.get("CMAKE_COMMAND") or shutil.which("cmake")
        ninja = os.environ.get("NINJA_COMMAND") or shutil.which("ninja")
        ctest = os.environ.get("CTEST_COMMAND") or shutil.which("ctest")
        if not cmake or not ninja or not ctest:
            raise unittest.SkipTest("CMake, Ninja and CTest are required (or their *_COMMAND overrides)")
        cls.ctest = ctest
        temporary = tempfile.TemporaryDirectory(prefix="labrador-ci-filters-")
        cls.addClassCleanup(temporary.cleanup)
        root = Path(temporary.name)
        cls.build = root / "build"
        # Every command would fail if executed. Only CTest's selection is read.
        (root / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.26)\n"
            "project(CiFilterFixture LANGUAGES NONE)\n"
            "enable_testing()\n" + "".join(
                f'add_test(NAME {name} COMMAND "${{CMAKE_COMMAND}}" -E false)\n'
                for name in sorted(cls.names)
            ), encoding="utf-8")
        result = subprocess.run(
            [cmake, "-S", str(root), "-B", str(cls.build), "-G", "Ninja",
             "-DCMAKE_MAKE_PROGRAM=" + ninja],
            capture_output=True, text=True, timeout=30)
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)

    def selected(self, *options: str) -> set[str]:
        result = subprocess.run(
            [self.ctest, "--test-dir", str(self.build), "--show-only=json-v1", *options],
            capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return {test["name"] for test in json.loads(result.stdout)["tests"]}

    def test_unfiltered_fixture_registers_every_name(self):
        self.assertEqual(self.selected(), self.names)

    def test_windows_filters_exclude_only_the_two_exact_device_tests(self):
        repository = Path(__file__).resolve().parents[2]
        workflow = (repository / ".github/workflows/ci.yml").read_text(encoding="utf-8")
        for preset in ("x64-debug-gl", "x64-debug-vulkan"):
            with self.subTest(preset=preset):
                filters = re.findall(
                    rf"(?m)^\s+- preset: {re.escape(preset)}\s*\n\s+skip_tests: '([^']*)'\s*$",
                    workflow)
                self.assertEqual(len(filters), 1, "Expected one literal skip_tests entry for " + preset)
                options = shlex.split(filters[0])
                self.assertEqual(len(options), 2)
                self.assertEqual(options[0], "-E")
                self.assertEqual(self.selected(*options), self.names - self.excluded)


if __name__ == "__main__":
    unittest.main()
