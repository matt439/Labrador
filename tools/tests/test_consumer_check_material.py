"""Check the opt-in shader fixture's edits and refusals without a compiler."""

from __future__ import annotations

import contextlib
import io
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

from tools.tests.tool_fixtures import load_tool


material_fixture = load_tool("tools/consumer_check/material_fixture.py")
REPOSITORY = Path(__file__).resolve().parents[2]


class ConsumerMaterialTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="labrador-material-consumer-")
        self.addCleanup(temporary.cleanup)
        self.project = Path(temporary.name)
        for source, destination in (
            ("website/src/walkthrough/root-CMakeLists.txt", "CMakeLists.txt"),
            ("samples/minimal/main.cpp", "game/main.cpp"),
            ("samples/minimal/main_web.cpp", "game/main_web.cpp"),
            ("vcpkg.json", "external/labrador/vcpkg.json"),
        ):
            path = self.project / destination
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes((REPOSITORY / source).read_bytes())
        (self.project / "vcpkg.json").write_text(
            json.dumps({"name": "mygame", "dependencies": ["doctest"]}), encoding="utf-8")

    def contents(self):
        return {path.relative_to(self.project): path.read_bytes()
                for path in self.project.rglob("*") if path.is_file()}

    def test_prepares_actual_native_and_web_entry_points_after_initialize(self):
        manifest = (self.project / "vcpkg.json").read_bytes()
        material_fixture.prepare(self.project)
        for entry in ("main.cpp", "main_web.cpp"):
            text = (self.project / "game" / entry).read_text(encoding="utf-8")
            self.assertEqual(text.count(material_fixture.MATERIAL_INCLUDE), 1)
            self.assertLess(text.index("app.initialize();"), text.index("add_material(consumer_material)"))
            self.assertLess(text.index("set_post_material(material)"), text.index("set_post_values(material, material_values)"))
            self.assertLess(text.index("add_material(consumer_material)"), text.index("app.run("))
        cmake = (self.project / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertLess(cmake.index("add_subdirectory(game)"), cmake.index("labrador_add_material"))
        self.assertTrue((self.project / "game/shaders/consumer_material.hlsl").is_file())
        self.assertEqual((self.project / "vcpkg.json").read_bytes(), manifest)

    def test_host_tools_come_from_the_consumed_engines_optional_feature(self):
        material_fixture.prepare(self.project, host_tools=True)
        actual = json.loads((self.project / "vcpkg.json").read_text(encoding="utf-8"))
        engine = json.loads((self.project / "external/labrador/vcpkg.json").read_text(encoding="utf-8"))
        self.assertEqual(actual["dependencies"], ["doctest", *engine["features"]["materials"]["dependencies"]])

    def test_missing_web_anchor_leaves_every_file_unchanged(self):
        path = self.project / "game/main_web.cpp"
        path.write_text("starter changed\n", encoding="utf-8")
        before = self.contents()
        with self.assertRaisesRegex(ValueError, "one application include"):
            material_fixture.prepare(self.project)
        self.assertEqual(self.contents(), before)

    def test_refuses_second_application_without_changing_files(self):
        material_fixture.prepare(self.project)
        before = self.contents()
        with self.assertRaisesRegex(ValueError, "already has the material fixture"):
            material_fixture.prepare(self.project)
        self.assertEqual(self.contents(), before)

    def check_diagnostic(self, output, returncode=1):
        material_fixture.prepare(self.project)
        shader = self.project / "game/shaders/consumer_material.hlsl"
        original = shader.read_bytes()
        calls = []

        def run(command, **options):
            calls.append(shader.read_bytes())
            return subprocess.CompletedProcess(command,
                                               returncode if len(calls) == 1 else 0,
                                               output if len(calls) == 1 else "restored")

        try:
            with contextlib.redirect_stdout(io.StringIO()):
                material_fixture.check_invalid_shader(self.project, Path("out/build/fixture"), run)
        finally:
            self.assertEqual(shader.read_bytes(), original)
            self.assertEqual(calls, [material_fixture.INVALID_SHADER, original])

    def test_accepts_native_source_location_and_restores_bytes(self):
        self.check_diagnostic("game/consumer_material.hlsl(3,9): error X3004: undeclared identifier")

    def test_accepts_dxc_source_location(self):
        self.check_diagnostic("game/consumer_material.hlsl:3:9: error: use of undeclared identifier")

    def test_unexpected_success_restores_and_rebuilds_before_refusing(self):
        with self.assertRaisesRegex(ValueError, "unexpectedly built"):
            self.check_diagnostic("built", returncode=0)

    def test_a_filename_without_a_diagnostic_location_does_not_pass(self):
        with self.assertRaisesRegex(ValueError, "without naming"):
            self.check_diagnostic("FAILED: consumer_material.hlsl, compiler executable absent")


if __name__ == "__main__":
    unittest.main()
