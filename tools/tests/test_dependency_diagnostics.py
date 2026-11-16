"""Configure-only consumer dependency failures without a compiler, network or vcpkg install."""

from __future__ import annotations

import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

REPOSITORY = Path(__file__).resolve().parents[2]
PACKAGES = {"lodepng": "lodepng", "directxtk": "Microsoft::DirectXTK",
            "xaudio2redist": "Microsoft::XAudio2Redist", "doctest": "doctest::doctest"}


class DependencyDiagnosticsTests(unittest.TestCase):
    def setUp(self):
        self.cmake = os.environ.get("CMAKE_COMMAND") or shutil.which("cmake")
        if not self.cmake:
            self.skipTest("CMake is required for configure-only dependency fixtures")
        temporary = tempfile.TemporaryDirectory(prefix="labrador-dependencies-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.prefix = self.root / "packages"
        self.consumer = self.root / "consumer"
        (self.consumer / "engine").mkdir(parents=True)
        (self.consumer / "game").mkdir()
        for package, target in PACKAGES.items():
            directory = self.prefix / "share" / package
            directory.mkdir(parents=True)
            (directory / f"{package}Config.cmake").write_text(
                f'if(NOT TARGET {target})\nadd_library({target} INTERFACE IMPORTED)\nendif()\n',
                encoding="utf-8")
        (self.consumer / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.26)\nproject(DependencyConsumer NONE)\n'
            'set(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH FALSE)\n'
            'set(CMAKE_FIND_USE_CMAKE_ENVIRONMENT_PATH FALSE)\n'
            'set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH FALSE)\n'
            'set(CMAKE_FIND_USE_PACKAGE_REGISTRY FALSE)\n'
            'set(CMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY FALSE)\n'
            'add_subdirectory(engine)\nadd_subdirectory(game)\n', encoding="utf-8")
        (self.consumer / "engine/CMakeLists.txt").write_text(
            f'include("{REPOSITORY.as_posix()}/cmake/dependencies.cmake")\n'
            'labrador_find_package(lodepng)\n'
            'if(NOT PORTABLE)\n'
            'labrador_find_package(xaudio2redist PORT directxtk FEATURE xaudio2redist)\n'
            'labrador_find_package(directxtk PORT directxtk FEATURE xaudio2redist)\n'
            'endif()\n', encoding="utf-8")
        (self.consumer / "game/CMakeLists.txt").write_text(
            'if(NOT SKIP_GAME_TESTS)\nlabrador_find_package(doctest)\nendif()\n', encoding="utf-8")
        self.manifest([])

    def manifest(self, dependencies, **fields):
        (self.consumer / "vcpkg.json").write_text(json.dumps({"name": "consumer",
            "version-string": "1.0.0", "builtin-baseline": "a" * 40,
            "dependencies": dependencies, **fields}), encoding="utf-8")

    def configure(self, *arguments):
        build = self.root / f"build-{len(list(self.root.glob('build-*')))}"
        result = subprocess.run([self.cmake, "-S", str(self.consumer), "-B", str(build),
                                 f"-DCMAKE_PREFIX_PATH={self.prefix.as_posix()}", *arguments],
                                capture_output=True, text=True, timeout=30, check=False)
        return result.returncode, result.stdout + result.stderr

    def omit(self, package):
        (self.prefix / "share" / package / f"{package}Config.cmake").unlink()

    def test_a_working_consumer_with_a_different_baseline_is_not_rejected(self):
        self.manifest(["lodepng", {"name": "directxtk", "features": ["xaudio2redist"]}, "doctest"])
        code, output = self.configure()
        self.assertEqual(code, 0, output)
        self.assertNotIn("could not discover", output)

    def test_shared_walkthrough_uses_the_helper_only_when_the_engine_provides_it(self):
        shared = (REPOSITORY / "website/src/walkthrough/game-CMakeLists.txt").read_text(encoding="utf-8")
        start = shared.index("if(COMMAND labrador_find_package)")
        end = shared.index("\nadd_executable(MyGameTests", start)
        (self.consumer / "game/CMakeLists.txt").write_text(
            shared[start:end] + '\nif(NOT TARGET doctest::doctest)\n'
            'message(FATAL_ERROR "The walkthrough needs doctest::doctest")\nendif()\n',
            encoding="utf-8")
        root = self.consumer / "CMakeLists.txt"
        source = root.read_text(encoding="utf-8")
        source = source.replace('add_subdirectory(engine)\n',
            'if(WITH_DEPENDENCY_HELPER)\n'
            f'include("{REPOSITORY.as_posix()}/cmake/dependencies.cmake")\nendif()\n')
        root.write_text(source, encoding="utf-8")
        helper_call = r"\):\s+labrador_find_package\(doctest\s*\)"
        ordinary_call = r"\):\s+find_package\(doctest CONFIG REQUIRED\s*\)"
        for enabled in (False, True):
            with self.subTest(helper=enabled):
                code, output = self.configure(
                    f"-DWITH_DEPENDENCY_HELPER={'ON' if enabled else 'OFF'}",
                    f"--trace-source={self.consumer.as_posix()}/game/CMakeLists.txt")
                self.assertEqual(code, 0, output)
                self.assertRegex(output, helper_call if enabled else ordinary_call)
                self.assertNotRegex(output, ordinary_call if enabled else helper_call)

    def test_each_missing_package_names_the_consumer_obligation_and_retains_cmake_detail(self):
        for package in PACKAGES:
            with self.subTest(package=package):
                original = self.prefix / "share" / package / f"{package}Config.cmake"
                hidden = original.with_suffix(".hidden")
                original.rename(hidden)
                try:
                    code, output = self.configure()
                finally:
                    hidden.rename(original)
                self.assertNotEqual(code, 0, output)
                self.assertIn(f"CMake package '{package}'", output)
                self.assertIn(str(self.consumer / "vcpkg.json").replace("\\", "/"), output)
                self.assertIn(REPOSITORY.as_posix() + "/vcpkg.json", output)
                self.assertIn(f"{package}Config.cmake", output)
                self.assertIn("does not install its own manifest", output)
                if package in {"directxtk", "xaudio2redist"}:
                    self.assertIn('"name": "directxtk"', output)
                    self.assertIn('"features": ["xaudio2redist"]', output)

    def test_declared_but_undiscovered_package_points_at_toolchain_and_triplets(self):
        self.omit("lodepng")
        self.manifest(["lodepng"])
        code, output = self.configure()
        self.assertNotEqual(code, 0)
        self.assertIn("already declares lodepng", output)
        self.assertIn("CMAKE_TOOLCHAIN_FILE", output)
        self.assertIn("selected triplets", output)
        self.assertNotIn('add "lodepng"', output)
        self.assertIn("lodepngConfig.cmake", output)

    def test_a_found_but_broken_package_keeps_its_original_reason(self):
        self.manifest(["lodepng"])
        (self.prefix / "share/lodepng/lodepngConfig.cmake").write_text(
            'set(lodepng_FOUND FALSE)\nset(lodepng_NOT_FOUND_MESSAGE "Fixture dependency is broken")\n',
            encoding="utf-8")
        code, output = self.configure()
        self.assertNotEqual(code, 0)
        self.assertIn("already declares lodepng", output)
        self.assertIn("Fixture dependency is broken", output)

    def test_a_partial_import_cannot_turn_a_failed_lookup_into_success(self):
        self.manifest(["lodepng"])
        (self.prefix / "share/lodepng/lodepngConfig.cmake").write_text(
            'if(TARGET lodepng)\nreturn()\nendif()\n'
            'add_library(lodepng INTERFACE IMPORTED)\n'
            'set(lodepng_FOUND FALSE)\nset(lodepng_NOT_FOUND_MESSAGE "Incomplete imported package")\n',
            encoding="utf-8")
        code, output = self.configure()
        self.assertNotEqual(code, 0, output)
        self.assertIn("Incomplete imported package", output)

    def test_feature_and_default_feature_declarations_are_recognised(self):
        self.omit("lodepng")
        for selected in (True, False):
            with self.subTest(explicitly_selected=selected):
                self.manifest([], features={"pictures": {"description": "pictures", "dependencies": ["lodepng"]}},
                              **({} if selected else {"default-features": ["pictures"]}))
                code, output = self.configure(*(["-DVCPKG_MANIFEST_FEATURES=pictures"] if selected else []))
                self.assertNotEqual(code, 0)
                self.assertIn("already declares lodepng", output)

    def test_portable_selection_does_not_require_windows_packages_or_optional_material_tools(self):
        self.omit("directxtk")
        self.omit("xaudio2redist")
        code, output = self.configure("-DPORTABLE=ON")
        self.assertEqual(code, 0, output)
        self.assertNotIn("directxtk", output)
        self.assertNotIn("directx-dxc", output)

    def test_disabled_game_tests_do_not_require_doctest(self):
        self.omit("doctest")
        code, output = self.configure("-DSKIP_GAME_TESTS=ON")
        self.assertEqual(code, 0, output)

    def test_invalid_manifest_shape_does_not_hide_cmake_discovery_errors(self):
        self.omit("lodepng")
        self.manifest({}, **{"default-features": "not-an-array"})
        code, output = self.configure()
        self.assertNotEqual(code, 0)
        self.assertIn("lodepngConfig.cmake", output)
        self.assertNotIn("string sub-command JSON", output)

    def test_broken_toolchain_is_not_misdiagnosed_as_a_manifest_entry(self):
        code, output = self.configure(f"-DCMAKE_TOOLCHAIN_FILE={self.root.as_posix()}/missing.cmake")
        self.assertNotEqual(code, 0)
        self.assertIn("Could not find toolchain file", output)
        self.assertNotIn("Labrador could not discover", output)


if __name__ == "__main__":
    unittest.main()
