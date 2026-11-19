"""Compiler-free checks of the explicit Direct3D software-test selection."""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


REPOSITORY = Path(__file__).resolve().parents[2]
MODULE = (REPOSITORY / "cmake/d3d_software_test.cmake").as_posix()
CMAKE = shutil.which("cmake")


@unittest.skipUnless(CMAKE, "CMake is required for the compiler-free policy fixtures")
class Direct3dSoftwareTestPolicyTests(unittest.TestCase):
    def configure(self, *, enabled=None, backend="d3d11", windows=True,
                  browser=False, child=False):
        temporary = tempfile.TemporaryDirectory(prefix="labrador-d3d-software-")
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        selection = (
            f"set(WIN32 {'TRUE' if windows else 'FALSE'})\n"
            f"set(EMSCRIPTEN {'TRUE' if browser else 'FALSE'})\n"
            f"set(LABRADOR_RENDER_BACKEND {backend})\n"
            f'include("{MODULE}")\n'
            'file(WRITE "${CMAKE_BINARY_DIR}/selection.txt" "${LABRADOR_D3D_SOFTWARE_TEST}")\n'
        )
        project = "cmake_minimum_required(VERSION 3.26)\nproject(SoftwarePolicy LANGUAGES NONE)\n"
        if child:
            (root / "engine").mkdir()
            (root / "engine/CMakeLists.txt").write_text(selection, encoding="utf-8")
            project += "add_subdirectory(engine)\n"
        else:
            project += selection
        (root / "CMakeLists.txt").write_text(project, encoding="utf-8")
        arguments = [f"-DLABRADOR_D3D_SOFTWARE_TEST={enabled}"] if enabled else []
        result = subprocess.run([CMAKE, "-S", str(root), "-B", str(root / "build"), *arguments],
                                capture_output=True, text=True, timeout=30)
        return result, root / "build"

    def test_every_backend_defaults_off(self):
        for backend in ("d3d11", "d3d12", "gl", "vulkan", "null"):
            with self.subTest(backend=backend):
                result, build = self.configure(backend=backend)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual((build / "selection.txt").read_text(), "OFF")

    def test_explicit_native_direct3d_selection_is_supported_for_consumers_too(self):
        for backend in ("d3d11", "d3d12"):
            for child in (False, True):
                with self.subTest(backend=backend, child=child):
                    result, build = self.configure(enabled="ON", backend=backend, child=child)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertEqual((build / "selection.txt").read_text(), "ON")

    def test_explicit_selection_refuses_every_other_platform_or_backend(self):
        for options in ({"windows": False}, {"browser": True}, {"backend": "gl"},
                        {"backend": "vulkan"}, {"backend": "null"}):
            with self.subTest(options=options):
                result, _ = self.configure(enabled="ON", **options)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("LABRADOR_D3D_SOFTWARE_TEST requires native Windows d3d11 or d3d12",
                              result.stdout + result.stderr)

    def test_an_explicit_cache_selection_survives_reconfigure(self):
        result, build = self.configure(enabled="ON")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        repeated = subprocess.run([CMAKE, "-S", str(build.parent), "-B", str(build)],
                                  capture_output=True, text=True, timeout=30)
        self.assertEqual(repeated.returncode, 0, repeated.stdout + repeated.stderr)
        self.assertEqual((build / "selection.txt").read_text(), "ON")
