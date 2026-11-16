"""The material build boundary: caller scopes, payloads and GLSL resources."""

from __future__ import annotations

import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest


REPOSITORY = Path(__file__).resolve().parents[2]


class MaterialBuildTests(unittest.TestCase):
    def setUp(self):
        self.cmake = os.environ.get("CMAKE_COMMAND") or shutil.which("cmake")
        if not self.cmake:
            self.skipTest("CMake is required (or CMAKE_COMMAND)")
        temporary = tempfile.TemporaryDirectory(prefix="labrador-material-")
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)

    def run_cmake(self, *arguments: str) -> subprocess.CompletedProcess:
        return subprocess.run([self.cmake, *arguments], capture_output=True,
                              text=True, timeout=60)

    def embed(self, kind: str, source: bytes) -> tuple[subprocess.CompletedProcess, Path]:
        shader = self.directory / "shader.bin"
        shader.write_bytes(source)
        output = self.directory / "generated/materials/example.h"
        result = self.run_cmake("-DNAME=example", f"-DKIND={kind}",
                                f"-DINPUT={shader}", f"-DOUTPUT={output}",
                                "-P", str(REPOSITORY / "cmake/embed_material.cmake"))
        return result, output

    def assert_success(self, result: subprocess.CompletedProcess):
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    @staticmethod
    def payload(header: Path) -> bytes:
        source = header.read_text(encoding="utf-8")
        return bytes(int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]{2})", source))

    def test_binary_shader_keeps_every_byte(self):
        source = bytes(range(256))
        result, header = self.embed("spirv", source)
        self.assert_success(result)
        self.assertEqual(self.payload(header), source)
        self.assertIn('MaterialSource example{MaterialKind::spirv, example_bytes, "example"}',
                      header.read_text(encoding="utf-8"))

    def test_glsl_uses_the_fixed_frame_names_and_high_precision(self):
        result, header = self.embed("glsl", b"""#version 300 es
precision mediump float;
uniform highp vec4 type_PostFrame[11];
uniform highp sampler2D SPIRV_Cross_Combinedinput_0point_sampler;
uniform highp sampler2D SPIRV_Cross_Combinedinput_0linear_sampler;
in highp vec2 post_uv;
void main() {}
""")
        self.assert_success(result)
        shader = self.payload(header)
        self.assertTrue(shader.startswith(b"\nprecision highp float;"))
        self.assertTrue(shader.endswith(b"\0"))
        self.assertIn(b"post_frame[11]", shader)
        self.assertIn(b"sampler2D input_0_point;", shader)
        self.assertIn(b"sampler2D input_0_linear;", shader)
        self.assertNotIn(b"SPIRV_Cross", shader)
        self.assertNotIn(b"#version", shader)

    def test_constant_glsl_with_no_uniforms_is_valid(self):
        result, header = self.embed("glsl", b"#version 330\nvoid main() {}\n")
        self.assert_success(result)
        self.assertEqual(self.payload(header), b"\nvoid main() {}\n\0")

    def test_extra_resources_fail_before_a_header_is_written(self):
        for resource in ("uniform sampler2D SPIRV_Cross_Combinedextra_sampler;",
                         "uniform vec4 another_block[2];",
                         "uniform vec4 type_PostFrame[12];"):
            with self.subTest(resource=resource):
                result, header = self.embed("glsl", resource.encode())
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("outside the post frame", result.stderr)
                self.assertFalse(header.exists())

    def test_unknown_kind_and_empty_binary_are_refused(self):
        for kind, source, message in (("bad", b"a", "Unknown material byte kind"),
                                      ("d3d11", b"", "Empty compiled shader")):
            with self.subTest(kind=kind):
                result, header = self.embed(kind, source)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(message, result.stderr)
                self.assertFalse(header.exists())

    def fixture(self, declaration: str):
        # A language-free caller makes this independent of compiler/SDK setup.
        # The helper is replaced only where the generated header attaches to a
        # C++ target; the actual configure/build commands and scopes run as-is.
        engine = self.directory / "engine"
        game = self.directory / "game"
        engine.mkdir()
        game.mkdir()
        (self.directory / "CMakeLists.txt").write_text("""cmake_minimum_required(VERSION 3.26)
project(MaterialFixture LANGUAGES NONE)
set(LABRADOR_RENDER_BACKEND null)
add_subdirectory(engine)
add_subdirectory(game)
""", encoding="utf-8")
        (engine / "CMakeLists.txt").write_text(
            f'include("{REPOSITORY.as_posix()}/cmake/compile_shaders.cmake")\n',
            encoding="utf-8")
        (game / "CMakeLists.txt").write_text("""add_custom_target(game)
function(labrador_add_generated_shader target generated_file)
    add_custom_target(material_${target} ALL DEPENDS "${generated_file}")
endfunction()
""" + declaration + "\n", encoding="utf-8")
        (game / "pixel.hlsl").write_text(
            "float4 material(MaterialInput input) { return scene_point(input.uv); }\n",
            encoding="utf-8")

    def test_consumer_sibling_scope_keeps_the_engine_generated_root(self):
        self.fixture("labrador_add_material(game example SOURCE pixel.hlsl)")
        build = self.directory / "build"
        # Ninja's path is optional: CMake's default generator works too.
        result = self.run_cmake("-S", str(self.directory), "-B", str(build))
        self.assert_success(result)
        result = self.run_cmake("--build", str(build))
        self.assert_success(result)
        header = build / "generated/materials/example.h"
        self.assertTrue(header.exists())
        self.assertIn('MaterialKind::null, {}, "example"', header.read_text(encoding="utf-8"))

    def test_missing_source_is_reported_at_configure(self):
        self.fixture("labrador_add_material(game example SOURCE missing.hlsl)")
        result = self.run_cmake("-S", str(self.directory), "-B", str(self.directory / "build"))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("source does not exist", result.stderr)

    def test_same_material_can_attach_to_two_targets(self):
        self.fixture("""add_custom_target(other_game)
labrador_add_material(game example SOURCE pixel.hlsl)
labrador_add_material(other_game example SOURCE pixel.hlsl)""")
        build = self.directory / "build"
        self.assert_success(self.run_cmake("-S", str(self.directory), "-B", str(build)))
        self.assert_success(self.run_cmake("--build", str(build), "--target", "other_game"))
        self.assertTrue((build / "generated/materials/example.h").exists())

    def test_same_name_cannot_silently_replace_a_material(self):
        self.fixture("""add_custom_target(other_game)
labrador_add_material(game example SOURCE pixel.hlsl)
labrador_add_material(other_game example SOURCE other.hlsl)""")
        (self.directory / "game/other.hlsl").write_text("different material", encoding="utf-8")
        result = self.run_cmake("-S", str(self.directory), "-B", str(self.directory / "build"))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("already names another source", result.stderr)

    def test_implicit_host_triplet_uses_host_architecture_not_wasm(self):
        script = self.directory / "host.cmake"
        script.write_text(f'''include("{REPOSITORY.as_posix()}/cmake/add_material.cmake")
set(ENV{{VCPKG_DEFAULT_HOST_TRIPLET}} "")
set(CMAKE_HOST_WIN32 TRUE)
set(CMAKE_HOST_SYSTEM_PROCESSOR AMD64)
set(VCPKG_TARGET_TRIPLET wasm32-emscripten)
labrador_material_host_triplet(host)
if(NOT host STREQUAL "x64-windows")
    message(FATAL_ERROR "Expected x64-windows, got ${{host}}")
endif()
set(ENV{{VCPKG_DEFAULT_HOST_TRIPLET}} "custom-host")
labrador_material_host_triplet(host)
if(NOT host STREQUAL "custom-host")
    message(FATAL_ERROR "Environment host selection was lost")
endif()
set(VCPKG_HOST_TRIPLET explicit-host)
labrador_material_host_triplet(host)
if(NOT host STREQUAL "explicit-host")
    message(FATAL_ERROR "Explicit host selection was lost")
endif()
''', encoding="utf-8")
        self.assert_success(self.run_cmake("-P", str(script)))


if __name__ == "__main__":
    unittest.main()
