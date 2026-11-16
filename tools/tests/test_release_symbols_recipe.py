"""Configure the optional game-owned symbols recipe without a compiler."""

from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

REPOSITORY = Path(__file__).resolve().parents[2]
RECIPE = REPOSITORY / "website/src/walkthrough/release-symbols.cmake"


class ReleaseSymbolsRecipeTests(unittest.TestCase):
    def setUp(self):
        self.cmake = os.environ.get("CMAKE_COMMAND") or shutil.which("cmake")
        if not self.cmake:
            self.skipTest("CMake is required for compiler-free recipe fixtures")
        temporary = tempfile.TemporaryDirectory(prefix="labrador-release-symbols-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        (self.root / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.26)\nproject(SymbolRecipe NONE)\n'
            'set(CMAKE_CXX_COMPILER_ID "${FIXTURE_COMPILER}")\n'
            'set(CMAKE_BUILD_TYPE "${FIXTURE_CONFIG}")\n'
            'set(WIN32 "${FIXTURE_WINDOWS}")\nset(EMSCRIPTEN "${FIXTURE_WEB}")\n'
            'set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Ob2 /DNDEBUG")\n'
            'set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)\n'
            'set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE "${FIXTURE_IPO}")\n'
            'add_compile_options(existing_compile)\nadd_link_options(existing_link)\n'
            f'include("{RECIPE.as_posix()}")\n'
            'get_directory_property(compile_options COMPILE_OPTIONS)\n'
            'get_directory_property(link_options LINK_OPTIONS)\n'
            'file(WRITE "${CMAKE_BINARY_DIR}/options.txt" '
            '"compile=${compile_options}\nlink=${link_options}\n'
            'release=${CMAKE_CXX_FLAGS_RELEASE}\nruntime=${CMAKE_MSVC_RUNTIME_LIBRARY}\n'
            'ipo=${CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE}\n")\n', encoding="utf-8")

    def configure(self, *extra):
        build = self.root / f"build-{len(list(self.root.glob('build-*')))}"
        command = [self.cmake, "-S", str(self.root), "-B", str(build),
                   "-DFIXTURE_COMPILER=MSVC", "-DFIXTURE_CONFIG=Release",
                   "-DFIXTURE_WINDOWS=ON", "-DFIXTURE_WEB=OFF", "-DFIXTURE_IPO=OFF", *extra]
        result = subprocess.run(command, capture_output=True, text=True, timeout=30, check=False)
        values = {}
        if (build / "options.txt").is_file():
            values = dict(line.split("=", 1) for line in
                          (build / "options.txt").read_text(encoding="utf-8").splitlines())
        return result.returncode, result.stdout + result.stderr, values

    def test_default_off_preserves_an_unrelated_platform_and_all_existing_choices(self):
        code, output, values = self.configure("-DFIXTURE_COMPILER=Clang", "-DFIXTURE_CONFIG=Debug",
                                              "-DFIXTURE_WINDOWS=OFF", "-DFIXTURE_WEB=ON",
                                              "-DFIXTURE_IPO=ON")
        self.assertEqual(code, 0, output)
        self.assertEqual(values, {"compile": "existing_compile", "link": "existing_link",
                                  "release": "/O2 /Ob2 /DNDEBUG", "runtime": "MultiThreadedDLL",
                                  "ipo": "ON"})

    def test_opt_in_keeps_optimisation_runtime_and_both_ipo_choices(self):
        for ipo in ("OFF", "ON"):
            with self.subTest(ipo=ipo):
                code, output, values = self.configure("-DMYGAME_RELEASE_SYMBOLS=ON", f"-DFIXTURE_IPO={ipo}")
                self.assertEqual(code, 0, output)
                self.assertEqual(values["compile"], "existing_compile;/Zi;/ZH:SHA_256")
                self.assertEqual(values["link"],
                                 "existing_link;/DEBUG:FULL;/OPT:REF;/OPT:ICF;/INCREMENTAL:NO")
                self.assertEqual(values["release"], "/O2 /Ob2 /DNDEBUG")
                self.assertEqual(values["runtime"], "MultiThreadedDLL")
                self.assertEqual(values["ipo"], ipo)

    def test_explicit_opt_in_refuses_debug_and_browser_compilation(self):
        for extra in (("-DFIXTURE_CONFIG=Debug",),
                      ("-DFIXTURE_COMPILER=Clang", "-DFIXTURE_WEB=ON")):
            with self.subTest(arguments=extra):
                code, output, values = self.configure("-DMYGAME_RELEASE_SYMBOLS=ON", *extra)
                self.assertNotEqual(code, 0)
                self.assertIn("native Windows MSVC Release", output)
                self.assertEqual(values, {})


if __name__ == "__main__":
    unittest.main()
