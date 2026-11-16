import importlib.util
from pathlib import Path
import tempfile
import unittest


SPEC = importlib.util.spec_from_file_location(
    "scripted_pad_fixture", Path(__file__).resolve().parents[1] /
    "consumer_check/scripted_pad_fixture.py")
FIXTURE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(FIXTURE)


class ScriptedPadFixtureTests(unittest.TestCase):
    def test_refusals_preserve_existing_project_and_success_is_explicit(self):
        with tempfile.TemporaryDirectory() as directory:
            project = Path(directory)
            root = project / "CMakeLists.txt"
            original = "add_subdirectory(game)\n"
            root.write_text(original, encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "Missing consumer fixture"):
                FIXTURE.prepare(project)
            self.assertEqual(root.read_text(encoding="utf-8"), original)
            source = project / "external/labrador/tools/consumer_check/scripted_pad.cpp"
            source.parent.mkdir(parents=True)
            source.write_text("int main() {}\n", encoding="utf-8")
            FIXTURE.prepare(project)
            expected = root.read_text(encoding="utf-8")
            self.assertIn("LabradorEngine labrador_settings", expected)
            self.assertIn('"-sENVIRONMENT=node"', expected)
            with self.assertRaisesRegex(ValueError, "already present"):
                FIXTURE.prepare(project)
            self.assertEqual(root.read_text(encoding="utf-8"), expected)

    def test_non_starter_is_refused_before_writing(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory) / "CMakeLists.txt"
            root.write_text("project(Other)\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "Expected one"):
                FIXTURE.prepare(Path(directory))
            self.assertEqual(root.read_text(encoding="utf-8"), "project(Other)\n")
