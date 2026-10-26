"""Check the production Vulkan header minimum through CMake's real finder.

The synthetic SDK contains only version metadata and an empty import library.
It cannot compile or link Vulkan code; these fixtures configure LANGUAGES NONE.
"""

from __future__ import annotations

import os
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


class VulkanSdkVersionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cmake = os.environ.get("CMAKE_COMMAND") or shutil.which("cmake")
        cls.ninja = os.environ.get("NINJA_COMMAND") or shutil.which("ninja")
        if cls.cmake and not cls.ninja:
            # Visual Studio ships Ninja beside its CMake installation, even
            # when only CMake's bin directory has been added to PATH.
            cmake_path = Path(shutil.which(cls.cmake) or cls.cmake).resolve()
            paired_ninja = cmake_path.parent.parent.parent / "Ninja" / "ninja.exe"
            if paired_ninja.is_file():
                cls.ninja = str(paired_ninja)
        if not cls.cmake or not cls.ninja:
            raise unittest.SkipTest(
                "CMake and Ninja are required (or CMAKE_COMMAND/NINJA_COMMAND)")

        repository = Path(__file__).resolve().parents[2]
        source = (repository / "engine/CMakeLists.txt").read_text(encoding="utf-8")
        requests = re.findall(
            r"(?m)^[ \t]*(find_package\([ \t]*Vulkan\b[^)\r\n]*\))"
            r"[ \t]*(?:#[^\r\n]*)?$", source)
        if len(requests) != 1:
            raise RuntimeError("Expected one single-line production find_package(Vulkan ...) call")
        # Use the actual request: removing or raising its minimum must change
        # these outcomes, rather than leave an independently hardcoded test green.
        cls.request = requests[0]

    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="labrador-vulkan-version-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def configure(self, version: str, *, modern_macro: bool = False,
                  request: str | None = None) -> subprocess.CompletedProcess[str]:
        major, minor, patch = version.split(".")
        sdk = self.root / "sdk"
        include = sdk / "include"
        headers = include / "vulkan"
        headers.mkdir(parents=True)
        (headers / "vulkan.h").write_text("", encoding="utf-8")
        complete = (f"VK_MAKE_API_VERSION(0, {major}, {minor}, VK_HEADER_VERSION)"
                    if modern_macro else
                    f"VK_MAKE_VERSION({major}, {minor}, VK_HEADER_VERSION)")
        # FindVulkan reads these definitions as text; VK_MAKE_* deliberately
        # needs no implementation because no C preprocessor or compiler runs.
        (headers / "vulkan_core.h").write_text(
            f"#define VK_HEADER_VERSION {patch}\n"
            f"#define VK_HEADER_VERSION_COMPLETE {complete}\n", encoding="utf-8")
        library = sdk / "lib" / "vulkan-1.lib"
        library.parent.mkdir()
        library.write_bytes(b"")

        self.include = include.as_posix()
        self.library = library.as_posix()
        self.build = self.root / "build"
        (self.root / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.26)\n"
            "project(VulkanVersionFixture LANGUAGES NONE)\n"
            # Force the built-in module, not an installed config or a mock
            # FindVulkan. Explicit cache paths supply every required SDK input.
            'set(CMAKE_MODULE_PATH "")\n'
            "set(CMAKE_FIND_PACKAGE_PREFER_CONFIG FALSE)\n"
            "set(CMAKE_FIND_USE_PACKAGE_ROOT_PATH FALSE)\n"
            "set(CMAKE_FIND_USE_CMAKE_PATH FALSE)\n"
            "set(CMAKE_FIND_USE_CMAKE_ENVIRONMENT_PATH FALSE)\n"
            "set(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH FALSE)\n"
            "set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH FALSE)\n"
            "set(CMAKE_FIND_USE_INSTALL_PREFIX FALSE)\n"
            + (self.request if request is None else request) + "\n"
            # Do not make a missing target fatal here. If REQUIRED is removed
            # from production, an unsuitable version must configure successfully
            # and make the rejection test fail, rather than fail on our own guard.
            'set(found "no")\n'
            'set(location "missing")\n'
            'set(includes "missing")\n'
            "if(Vulkan_FOUND AND TARGET Vulkan::Vulkan)\n"
            '  set(found "yes")\n'
            "  get_target_property(location Vulkan::Vulkan IMPORTED_LOCATION)\n"
            "  get_target_property(includes Vulkan::Vulkan INTERFACE_INCLUDE_DIRECTORIES)\n"
            "endif()\n"
            'file(WRITE "${CMAKE_BINARY_DIR}/found.txt" '
            '"${found}\n${Vulkan_VERSION}\n${location}\n${includes}\n")\n',
            encoding="utf-8")

        environment = os.environ.copy()
        for name in ("CMAKE_TOOLCHAIN_FILE", "CMAKE_PROJECT_TOP_LEVEL_INCLUDES",
                     "CMAKE_PREFIX_PATH", "CMAKE_INCLUDE_PATH", "CMAKE_LIBRARY_PATH",
                     "CMAKE_PROGRAM_PATH", "Vulkan_ROOT", "VULKAN_ROOT"):
            environment.pop(name, None)
        environment["VULKAN_SDK"] = str(sdk)
        return subprocess.run(
            [self.cmake, "-S", str(self.root), "-B", str(self.build), "-G", "Ninja",
             "-DCMAKE_MAKE_PROGRAM:FILEPATH=" + self.ninja,
             "-DCMAKE_TOOLCHAIN_FILE:FILEPATH=",
             "-DCMAKE_PROJECT_TOP_LEVEL_INCLUDES:STRING=",
             "-DVulkan_INCLUDE_DIR:PATH=" + self.include,
             "-DVulkan_LIBRARY:FILEPATH=" + self.library],
            env=environment, capture_output=True, text=True,
            encoding="utf-8", errors="replace", timeout=30)

    def assert_found(self, result: subprocess.CompletedProcess[str], version: str):
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(
            (self.build / "found.txt").read_text(encoding="utf-8").splitlines(),
            ["yes", version, self.library, self.include])

    def test_older_headers_fail_with_the_required_version_diagnostic(self):
        result = self.configure("1.1.130")
        output = " ".join((result.stdout + result.stderr).split())
        self.assertNotEqual(result.returncode, 0, output)
        self.assertIn("Could NOT find Vulkan", output)
        self.assertIn('Found unsuitable version "1.1.130"', output)
        self.assertRegex(output, r'(?:minimum required is|at least) "1\.2"')
        self.assertNotIn("missing: Vulkan_LIBRARY", output)
        self.assertNotIn("missing: Vulkan_INCLUDE_DIR", output)

    def test_minimum_headers_are_accepted(self):
        self.assert_found(self.configure("1.2.0"), "1.2.0")

    def test_newer_headers_with_current_macro_shape_are_accepted(self):
        self.assert_found(self.configure("1.4.357", modern_macro=True), "1.4.357")

    def test_unversioned_control_accepts_the_same_older_headers(self):
        self.assert_found(
            self.configure("1.1.130", request="find_package(Vulkan REQUIRED)"),
            "1.1.130")


if __name__ == "__main__":
    unittest.main()
