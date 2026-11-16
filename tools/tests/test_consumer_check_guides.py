"""Pin guide-derived recipes, their refusals, inventories and failure restoration."""

from __future__ import annotations

import contextlib
import io
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

from tools.tests.tool_fixtures import load_tool


guide_check = load_tool("tools/consumer_check/guide_check.py")
examples = load_tool("tools/consumer_check/guide_examples.py")
assemble = load_tool("tools/consumer_check/assemble.py")
REPOSITORY = Path(__file__).resolve().parents[2]


class CompiledGuideTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="labrador-guide-consumer-")
        self.addCleanup(temporary.cleanup)
        self.project = Path(temporary.name)
        with contextlib.redirect_stdout(io.StringIO()):
            contents = assemble.project_files(REPOSITORY)
        for name, data in contents.items():
            path = self.project / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)

    def test_numbered_recipes_read_actual_guide_files_and_both_build_lists(self):
        original = guide_check.project_sources(self.project)
        for name, prepare in guide_check.RECIPES:
            with self.subTest(guide=name):
                guide_check.restore(self.project, original)
                guide = examples.Guide(name, REPOSITORY)
                prepare(self.project, guide)
                if name == "your-rule":
                    for title in (r"game\rules\bounds.h", r"game\rules\bounds.cpp",
                                  r"game\tests\bounds_tests.cpp"):
                        self.assertEqual((self.project / title.replace("\\", "/")).read_text(encoding="utf-8"),
                                         guide.file(title))
                    self.assertIn("centre_area(window_size", (self.project / "game/states/hello_state.cpp").read_text())
                else:
                    stem = "title_state" if name == "your-screen" else "block"
                    for extension in ("h", "cpp"):
                        self.assertEqual((self.project / f"game/states/{stem}.{extension}").read_text(encoding="utf-8"),
                                         guide.file(f"game\\states\\{stem}.{extension}"))
                    self.assertEqual((self.project / "game/CMakeLists.txt").read_text().count(f"states/{stem}.cpp"), 2)
                if name == "your-screen":
                    for entry in ("main.cpp", "main_web.cpp"):
                        self.assertIn("make_unique<TitleState>", (self.project / "game" / entry).read_text())

    def test_changed_collision_anchor_refuses_before_writing(self):
        source = self.project / "game/states/hello_state.cpp"
        source.write_text(source.read_text().replace("this->hint_ =", "this->changed_hint_ ="))
        before = guide_check.project_sources(self.project)
        with self.assertRaisesRegex(ValueError, "collision-guide anchor"):
            guide_check.collision_guide.prepare(self.project, examples.Guide("your-collision", REPOSITORY))
        self.assertEqual(guide_check.project_sources(self.project), before)

    def test_missing_or_duplicate_file_block_names_the_guide(self):
        guide = examples.Guide("your-rule", REPOSITORY)
        with self.assertRaisesRegex(ValueError, "your-rule.mdx.*found 0"):
            guide.file(r"game\rules\missing.h")
        guide.blocks.append(next(block for block in guide.blocks if block.title == r"game\rules\bounds.h"))
        with self.assertRaisesRegex(ValueError, "your-rule.mdx.*found 2"):
            guide.file(r"game\rules\bounds.h")

    def test_optional_rule_variants_do_not_replace_the_numbered_example(self):
        guide = examples.Guide("your-rule", REPOSITORY)
        self.assertGreater(guide.text.count("// Keep the greeting inside the window"), 1)
        self.assertEqual(guide.snippet("cpp", "// Keep the greeting inside the window").count("keep_inside"), 1)

    def fake_run(self, *, compiler_success=False, wrong_inventory=False, fail_build=False):
        def run(command, cwd, **options):
            del options
            project = Path(cwd)
            if "--show-only=json-v1" in command:
                names = ["MyGameTests", "MyGameSmokeTests"]
                if "add_executable(GuideStateTests" in (project / "CMakeLists.txt").read_text():
                    names.append("GuideStateTests")
                if wrong_inventory:
                    names.append("RenderTests")
                return subprocess.CompletedProcess(command, 0, json.dumps({"tests": [{"name": name} for name in names]}), "")
            if "--build" in command:
                rule = project / "game/rules/bounds.cpp"
                if rule.is_file() and "SM84_BROKEN_GUIDE" in rule.read_text():
                    return subprocess.CompletedProcess(command, 0 if compiler_success else 1,
                        "game/rules/bounds.cpp: error: SM84_BROKEN_GUIDE is undeclared", "")
                if fail_build and rule.is_file():
                    return subprocess.CompletedProcess(command, 1, "actual build failed", "")
            return subprocess.CompletedProcess(command, 0, "", "")
        return run

    def run_pilot(self, **options):
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            return guide_check.pilot(self.project, REPOSITORY, [], check_broken=True,
                                     run=self.fake_run(**options))

    def test_control_flow_restores_baseline_and_records_three_separate_examples(self):
        before = guide_check.project_sources(self.project)
        report = self.run_pilot()
        self.assertEqual(report["result"], "passed")
        self.assertEqual(len(report["examples"]), 3)
        self.assertEqual(guide_check.project_sources(self.project), before)
        self.assertEqual([step["exit_code"] for step in report["commands"] if "broken-build" in step["phase"]], [1])
        self.assertTrue((self.project / "out/guide-pilot/your-collision/game/states/block.cpp").is_file())

    def test_an_omitted_broken_source_cannot_pass_the_negative_proof(self):
        before = guide_check.project_sources(self.project)
        with self.assertRaisesRegex(ValueError, "did not fail compilation"):
            self.run_pilot(compiler_success=True)
        self.assertEqual(guide_check.project_sources(self.project), before)

    def test_engine_tests_leaking_into_the_consumer_inventory_fail(self):
        with self.assertRaisesRegex(ValueError, "consumer test inventory"):
            self.run_pilot(wrong_inventory=True)

    def test_a_failed_build_keeps_its_receipt_and_restores_sources(self):
        before = guide_check.project_sources(self.project)
        with self.assertRaisesRegex(ValueError, "your-rule-build failed"):
            self.run_pilot(fail_build=True)
        self.assertEqual(guide_check.project_sources(self.project), before)
        report = json.loads((self.project / "out/guide-pilot/BUILD.json").read_text())
        self.assertEqual(report["result"], "failed")
        self.assertEqual(report["commands"][-1]["exit_code"], 1)


if __name__ == "__main__":
    unittest.main()
