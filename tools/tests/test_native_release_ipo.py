"""Compiler-free checks of the native Release IPO policy's scope and refusals.

The capability probe is a stand-in here; native build qualification runs the
real compiler/linker probe and checks the generated compile/link commands.
"""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


REPOSITORY = Path(__file__).resolve().parents[2]
MODULE = (REPOSITORY / "cmake/native_release_ipo.cmake").as_posix()
CMAKE = shutil.which("cmake")


@unittest.skipUnless(CMAKE, "CMake is required for the compiler-free policy fixtures")
class NativeReleaseIpoTests(unittest.TestCase):
    def configure(self, *, enabled="ON", compiler="MSVC", windows=True,
                  browser=False, supported=True, inherited=None,
                  standalone=False, configuration="Release", compiler_flags=""):
        temporary = tempfile.TemporaryDirectory(prefix="labrador-ipo-policy-")
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        (root / "child").mkdir()
        (root / "modules").mkdir()
        (root / "modules/CheckIPOSupported.cmake").write_text(
            "function(check_ipo_supported)\n"
            "  cmake_parse_arguments(PROBE \"\" \"RESULT;OUTPUT\" \"LANGUAGES\" ${ARGN})\n"
            "  if(NOT PROBE_LANGUAGES STREQUAL \"CXX\")\n"
            "    message(FATAL_ERROR \"Probe must select only CXX\")\n"
            "  endif()\n"
            f"  set(${{PROBE_RESULT}} {'TRUE' if supported else 'FALSE'} PARENT_SCOPE)\n"
            "  set(${PROBE_OUTPUT} \"unsupported fixture linker\" PARENT_SCOPE)\n"
            "endfunction()\n", encoding="utf-8")
        (root / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.26)\n"
            "project(IpoPolicyFixture LANGUAGES NONE)\n"
            "list(PREPEND CMAKE_MODULE_PATH \"${CMAKE_CURRENT_SOURCE_DIR}/modules\")\n"
            f"set(CMAKE_CXX_COMPILER_ID {compiler})\n"
            f"set(CMAKE_BUILD_TYPE {configuration})\n"
            f'set(CMAKE_CXX_FLAGS "{compiler_flags}")\n'
            f"set(WIN32 {'TRUE' if windows else 'FALSE'})\n"
            f"set(EMSCRIPTEN {'TRUE' if browser else 'FALSE'})\n"
            + (f"set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE {inherited})\n"
               if inherited is not None else "")
            + (f'include("{MODULE}")\n'
               'file(WRITE "${CMAKE_BINARY_DIR}/child.txt" '
               '"${CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE}")\n'
               if standalone else "add_subdirectory(child)\n")
            + "file(WRITE \"${CMAKE_BINARY_DIR}/parent.txt\" "
            "\"${CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE}\")\n", encoding="utf-8")
        (root / "child/CMakeLists.txt").write_text(
            'project(LabradorFixture LANGUAGES NONE)\n'
            f'include("{MODULE}")\n'
            'file(WRITE "${CMAKE_BINARY_DIR}/child.txt" '
            '"${CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE}")\n', encoding="utf-8")
        selection = [f"-DLABRADOR_NATIVE_RELEASE_IPO={enabled}"] if enabled else []
        result = subprocess.run(
            [CMAKE, "-S", str(root), "-B", str(root / "build"), *selection],
            capture_output=True, text=True, timeout=30)
        return result, root / "build"

    def test_only_standalone_native_msvc_release_enables_the_default(self):
        result, build = self.configure(enabled=None, standalone=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual((build / "child.txt").read_text(), "ON")
        for options in ({"standalone": False}, {"standalone": True, "configuration": "Debug"},
                        {"standalone": True, "compiler": "Clang"},
                        {"standalone": True, "browser": True},
                        {"standalone": True, "windows": False}):
            with self.subTest(options=options):
                result, build = self.configure(enabled=None, **options)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual((build / "child.txt").read_text(), "")

    def test_standalone_default_has_an_explicit_opt_out(self):
        result, build = self.configure(enabled="OFF", standalone=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual((build / "child.txt").read_text(), "")

    def test_debug_asan_default_does_not_probe_ipo(self):
        result, build = self.configure(enabled=None, standalone=True,
                                       configuration="Debug", supported=False,
                                       compiler_flags="/fsanitize=address /Zi")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual((build / "child.txt").read_text(), "")

    def test_enabled_policy_stays_inside_engine_directory(self):
        result, build = self.configure()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual((build / "child.txt").read_text(), "ON")
        self.assertEqual((build / "parent.txt").read_text(), "")

    def test_opt_out_preserves_the_callers_explicit_policy(self):
        for inherited in (None, "OFF", "ON"):
            with self.subTest(inherited=inherited):
                result, build = self.configure(enabled="OFF", inherited=inherited)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                expected = inherited or ""
                self.assertEqual((build / "child.txt").read_text(), expected)
                self.assertEqual((build / "parent.txt").read_text(), expected)

    def test_other_compilers_platforms_and_browser_are_refused_when_enabled(self):
        for options in ({"compiler": "Clang"}, {"windows": False}, {"browser": True}):
            with self.subTest(options=options):
                result, _ = self.configure(**options)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("requires native Windows MSVC", result.stdout + result.stderr)

    def test_opt_out_does_not_change_browser_or_clang_policy(self):
        result, build = self.configure(enabled="OFF", compiler="Clang", browser=True)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual((build / "child.txt").read_text(), "")

    def test_failed_capability_probe_names_reason_and_opt_out(self):
        result, _ = self.configure(supported=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("unsupported fixture linker", result.stdout + result.stderr)
        self.assertIn("LABRADOR_NATIVE_RELEASE_IPO=OFF", result.stdout + result.stderr)
