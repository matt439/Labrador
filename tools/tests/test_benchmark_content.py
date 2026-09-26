"""Exercise the benchmark's actual content target without compiling C++."""

from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


class BenchmarkContentTests(unittest.TestCase):
    def test_incremental_target_refreshes_and_restores_deployed_content(self):
        cmake = os.environ.get("CMAKE_COMMAND") or shutil.which("cmake")
        ninja = os.environ.get("NINJA_COMMAND") or shutil.which("ninja")
        if not cmake or not ninja:
            self.skipTest("CMake and Ninja are required (or CMAKE_COMMAND/NINJA_COMMAND)")
        repository = Path(__file__).resolve().parents[2]
        source = (repository / "bench/CMakeLists.txt").read_text(encoding="utf-8")
        # Use the production command/dependency block; only replace the file
        # directory expression so an uncompiled fixture target can own it.
        commands = source[source.index("# The benchmark resolves the same real font"):]
        commands = commands.replace("$<TARGET_FILE_DIR:LineSweeperFrameBench>",
                                    "${CMAKE_CURRENT_BINARY_DIR}")
        with tempfile.TemporaryDirectory(prefix="labrador-content-") as temporary:
            root = Path(temporary)
            bench = root / "bench"
            content = root / "samples/linesweeper/content"
            bench.mkdir()
            assets = ("manifest.json", "fonts/test.spritefont", "textures/white.dds")
            for asset in assets:
                path = content / asset
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"version one")
            (bench / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.26)\n"
                "project(BenchmarkContentFixture LANGUAGES NONE)\n"
                "add_custom_target(LineSweeperFrameBench)\n" + commands, encoding="utf-8")
            build = root / "build"

            def run(*arguments: str) -> None:
                result = subprocess.run([cmake, *arguments], capture_output=True, timeout=30)
                self.assertEqual(result.returncode, 0,
                                 (result.stdout + result.stderr).decode("utf-8", errors="replace"))

            def rebuild() -> None:
                run("--build", str(build), "--target", "LineSweeperFrameBench")

            run("-S", str(bench), "-B", str(build), "-G", "Ninja",
                "-DCMAKE_MAKE_PROGRAM=" + ninja)
            rebuild()
            for asset in assets:
                self.assertEqual((build / asset).read_bytes(), b"version one")
                (content / asset).write_bytes(b"version two")
            rebuild()
            for asset in assets:
                deployed = build / asset
                self.assertEqual(deployed.read_bytes(), b"version two")
                deployed.unlink()
            rebuild()
            for asset in assets:
                self.assertEqual((build / asset).read_bytes(), b"version two")


if __name__ == "__main__":
    unittest.main()
