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
        # Use the production copy function and the benchmark's call to it; only
        # replace the file directory expression so an uncompiled fixture target
        # can own it.
        sample = (repository / "samples/linesweeper/CMakeLists.txt").read_text(encoding="utf-8")
        start = sample.index("function(linesweeper_add_content_copy")
        end = sample.index("endfunction()", start) + len("endfunction()")
        function = sample[start:end].replace("$<TARGET_FILE_DIR:${executable}>",
                                             "${CMAKE_CURRENT_BINARY_DIR}") + "\n"
        self.assertNotIn("TARGET_FILE_DIR", function)
        source = (repository / "bench/CMakeLists.txt").read_text(encoding="utf-8")
        start = source.index("# The benchmark resolves the same real font")
        call = "linesweeper_add_content_copy(linesweeper_frame_bench_content"
        end = source.index(")", source.index(call, start)) + 1
        commands = source[start:end] + "\n"
        with tempfile.TemporaryDirectory(prefix="labrador-content-") as temporary:
            root = Path(temporary)
            bench = root / "bench"
            content = root / "samples/linesweeper/content"
            bench.mkdir()
            # Defined in the sample's folder, as the real one is, so its
            # content path resolves against that folder.
            content.mkdir(parents=True)
            (content.parent / "content_copy.cmake").write_text(function, encoding="utf-8")
            assets = ("manifest.json", "fonts/test.spritefont", "textures/white.dds")
            for asset in assets:
                path = content / asset
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b"version one")
            (bench / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.26)\n"
                "project(BenchmarkContentFixture LANGUAGES NONE)\n"
                "include(../samples/linesweeper/content_copy.cmake)\n"
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
