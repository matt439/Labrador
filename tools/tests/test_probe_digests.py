"""The D3D12 probes' source digests follow an edit without an explicit reconfigure."""

from __future__ import annotations

import hashlib
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


class ProbeDigestTests(unittest.TestCase):
    def test_editing_a_measured_source_reconfigures_its_digest(self):
        cmake = os.environ.get("CMAKE_COMMAND") or shutil.which("cmake")
        ninja = os.environ.get("NINJA_COMMAND") or shutil.which("ninja")
        if not cmake or not ninja:
            self.skipTest("CMake and Ninja are required (or CMAKE_COMMAND/NINJA_COMMAND)")
        repository = Path(__file__).resolve().parents[2]
        source = (repository / "bench/CMakeLists.txt").read_text(encoding="utf-8")
        # The production digest block, from its comment to the second digest.
        start = source.index("# The probes print these digests")
        last = "descriptor_device_sha256)"
        end = source.index(last, start) + len(last)
        block = source[start:end] + "\n"
        self.assertIn("CMAKE_CONFIGURE_DEPENDS", block)
        with tempfile.TemporaryDirectory(prefix="labrador-digests-") as temporary:
            root = Path(temporary)
            bench = root / "bench"
            d3d12 = root / "engine/render/d3d12"
            bench.mkdir()
            d3d12.mkdir(parents=True)
            sources = [d3d12 / "renderer.cpp", d3d12 / "device_resources.cpp"]
            for path in sources:
                path.write_bytes(b"version one")
            (bench / "CMakeLists.txt").write_text(
                "cmake_minimum_required(VERSION 3.26)\n"
                "project(ProbeDigestFixture LANGUAGES NONE)\n" + block +
                'file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/digests.txt"\n'
                '    "${descriptor_renderer_sha256} ${descriptor_device_sha256}")\n'
                "add_custom_target(probe ALL)\n", encoding="utf-8")
            build = root / "build"

            def run(*arguments: str) -> None:
                result = subprocess.run([cmake, *arguments], capture_output=True, timeout=30)
                self.assertEqual(result.returncode, 0,
                                 (result.stdout + result.stderr).decode("utf-8", errors="replace"))

            def expected() -> str:
                return " ".join(hashlib.sha256(path.read_bytes()).hexdigest() for path in sources)

            run("-S", str(bench), "-B", str(build), "-G", "Ninja", "-DCMAKE_MAKE_PROGRAM=" + ninja)
            self.assertEqual((build / "digests.txt").read_text(encoding="utf-8"), expected())
            for path in sources:
                path.write_bytes(b"version two")
                # A build alone, as after an edit, with no configure asked for.
                run("--build", str(build))
                self.assertEqual((build / "digests.txt").read_text(encoding="utf-8"), expected())


if __name__ == "__main__":
    unittest.main()
