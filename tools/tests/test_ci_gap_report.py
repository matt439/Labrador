"""CI-gap list drift, JUnit grouping and fake-command execution; no tool is run."""

from __future__ import annotations

import contextlib
import io
import json
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

from tools.tests.tool_fixtures import load_tool

REPOSITORY = Path(__file__).resolve().parents[2]
report = load_tool("tools/ci_gap_report.py")

# Shaped as CTest 4 writes it for one passing, failing, timed-out, missing,
# skipped and disabled test each. LastTest.log records "Test Passed." for the
# missing executable, which is why the tool reads JUnit.
CTEST_JUNIT = """<?xml version="1.0" encoding="UTF-8"?>
<testsuite name="(empty)" tests="6" failures="2" disabled="1" skipped="2" hostname="" time="1">
\t<testcase name="Good" classname="Good" time="0.02" status="run"><properties/><system-out></system-out></testcase>
\t<testcase name="Bad" classname="Bad" time="0.02" status="fail"><failure message="Failed"/><properties/></testcase>
\t<testcase name="Slow" classname="Slow" time="1.01" status="fail"><failure message="Timeout"/><properties/></testcase>
\t<testcase name="Missing" classname="Missing" time="0" status="notrun"><skipped message="Unable to find executable"/></testcase>
\t<testcase name="Skipper" classname="Skipper" time="0.02" status="notrun"><skipped message="SKIP_REGULAR_EXPRESSION_MATCHED"/></testcase>
\t<testcase name="Off" classname="Off" time="0" status="disabled"><properties/><system-out>Disabled</system-out></testcase>
</testsuite>
"""


def junit(names: list[str]) -> str:
    cases = "".join(f'<testcase name="{name}" status="run"/>' for name in names)
    return f'<testsuite tests="{len(names)}">{cases}</testsuite>'


def git_runner(heads: list[str], dirty: str = ""):
    calls = iter(heads)

    def run(argv):
        if argv[1] == "rev-parse":
            return subprocess.CompletedProcess(argv, 0, next(calls) + "\n", "")
        return subprocess.CompletedProcess(argv, 0, dirty, "")
    return run


class DriftTests(unittest.TestCase):
    def test_gaps_are_exactly_what_ci_leaves_out(self):
        workflow = (REPOSITORY / ".github/workflows/ci.yml").read_text(encoding="utf-8")
        # Each matrix entry runs from its "- preset:" line through the lines
        # indented as its keys are.
        matrix = []
        for found in re.finditer(r"^( *)- preset: (\S+)\n((?:\1  .*\n)*)", workflow, re.MULTILINE):
            device = re.search(r"^\s*software_device: (\S+)$", found.group(3), re.MULTILINE)
            matrix.append((found.group(2), device.group(1).strip("'") if device else ""))
        self.assertTrue(matrix, "no matrix entries found in ci.yml")
        ci_presets = {preset for preset, _ in matrix}
        # Jobs outside the matrix name their preset literally.
        ci_presets |= set(re.findall(r"ctest --preset ([A-Za-z0-9-]+)", workflow))
        presets = json.loads((REPOSITORY / "CMakePresets.json").read_text(encoding="utf-8"))
        tested = {preset["name"] for preset in presets["testPresets"] if not preset.get("hidden")}

        gaps = {gap.preset: gap for gap in report.GAPS}
        whole = {name for name, gap in gaps.items() if not gap.real_driver}
        self.assertEqual(whole, tested - ci_presets)
        software = {preset for preset, device in matrix if device}
        self.assertEqual(software, {name for name, gap in gaps.items() if gap.real_driver})
        for gap in report.GAPS:
            self.assertTrue(set(gap.real_driver) <= set(gap.required), gap.preset)
        # Nothing excludes a test any more; an exclusion would be a new gap.
        self.assertNotIn("skip_tests", workflow)


class JunitTests(unittest.TestCase):
    def test_ctest_outcomes_are_grouped_without_raising(self):
        groups = report.junit_summary(CTEST_JUNIT)
        self.assertEqual(groups, {"passed": ["Good"], "failed": ["Bad", "Slow"],
                                  "skipped": ["Missing", "Skipper"], "disabled": ["Off"]})

    def test_wrapped_suite_is_accepted_and_bad_documents_fail(self):
        self.assertEqual(report.junit_summary(f"<testsuites>{junit(['A'])}</testsuites>")["passed"], ["A"])
        for text in ("<", "<testsuites/>", "<other/>", "<testsuite/>",
                     '<testsuite><testcase name="A"/><testcase name="A"/></testsuite>',
                     '<testsuite><testcase name=" "/></testsuite>',
                     '<testsuite><testcase name="A" status="odd"/></testsuite>'):
            with self.subTest(text=text), self.assertRaises(report.ReportError):
                report.junit_summary(text)

    def test_unknown_children_fail_as_the_render_sweep_refuses_them(self):
        for text in ('<testsuite><testcase name="A"/><unknown/></testsuite>',
                     '<testsuite><testcase name="A"><unknown/></testcase></testsuite>'):
            with self.subTest(text=text), self.assertRaisesRegex(report.ReportError, "unsupported JUnit"):
                report.junit_summary(text)

    def test_required_tests_must_have_run_and_passed(self):
        gl = report.GAPS[0]
        result, detail = report.judge(gl, report.junit_summary(junit(["CoreTests", "RenderPixelTests"])))
        self.assertEqual(result, "failed")
        self.assertIn("not registered: MinimalSmokeTests, CollisionSmokeTests", detail)
        result, detail = report.judge(gl, report.junit_summary(
            junit(["CoreTests", "RenderPixelTests", "MinimalSmokeTests", "CollisionSmokeTests"])))
        self.assertEqual((result, detail),
                         ("passed", "4/4 passed; RenderPixelTests, MinimalSmokeTests and "
                                    "CollisionSmokeTests passed"))
        # Without a Chromium, configure registers 19 of wasm-debug's 34 tests.
        wasm = report.GAPS[-1]
        unregistered = [f"Headless{index}" for index in range(19)]
        result, detail = report.judge(wasm, report.junit_summary(junit(unregistered)))
        self.assertEqual(result, "failed")
        self.assertIn("19/19 passed; not registered: RenderPixelTests,", detail)
        result, detail = report.judge(wasm, report.junit_summary(
            junit(unregistered + list(report.BROWSER_TESTS))))
        self.assertEqual((result, detail), ("passed", "34/34 passed; including 15 required tests"))
        result, detail = report.judge(report.Gap("x64-release-null"), report.junit_summary(CTEST_JUNIT))
        self.assertEqual(result, "failed")
        self.assertEqual(detail, "1/6 passed; failed: Bad, Slow; skipped: Missing, Skipper; disabled: Off")


def fake_commands(*, configure_fails=(), ctest_code=0, documents=None):
    """A command runner that writes each preset's JUnit, and the argv it saw."""
    calls = []

    def run(argv, *, cwd, log):
        calls.append(argv)
        preset = argv[argv.index("--preset") + 1]
        if argv[0] == "cmake" and argv[1] == "--preset" and preset in configure_fails:
            return 3
        if argv[0] == "ctest":
            Path(argv[-1]).write_text((documents or {}).get(preset, junit(["CoreTests"])),
                                      encoding="utf-8")
            return ctest_code
        return 0
    return run, calls


class ExecuteTests(unittest.TestCase):
    def test_a_failed_preset_does_not_stop_the_next(self):
        run, calls = fake_commands(configure_fails={"x64-release-gl"})
        gaps = [report.Gap("x64-release-gl"), report.Gap("x64-release-null")]
        with tempfile.TemporaryDirectory() as directory:
            outcomes = report.execute(gaps, Path(directory), Path(directory) / "out", runner=run)
            self.assertTrue((Path(directory) / "out/x64-release-null-junit.xml").is_file())
        self.assertEqual([outcome.result for outcome in outcomes], ["failed", "passed"])
        self.assertIn("`cmake --preset` exited 3", outcomes[0].detail)
        self.assertEqual(len(calls), 4)

    def test_nonzero_ctest_with_clean_junit_is_a_failure(self):
        run, _ = fake_commands(ctest_code=8)
        with tempfile.TemporaryDirectory() as directory:
            outcomes = report.execute([report.Gap("x64-release-null")], Path(directory),
                                      Path(directory) / "out", runner=run)
        self.assertEqual(outcomes[0].result, "failed")
        self.assertIn("ctest exited 8", outcomes[0].detail)

    def test_a_tool_missing_from_the_shell_fails_only_its_preset(self):
        with tempfile.TemporaryDirectory() as directory:
            log_path = Path(directory) / "log"
            with log_path.open("w", encoding="utf-8") as log:
                code = report.run_command(["labrador-no-such-tool"], cwd=Path(directory), log=log)
            self.assertEqual(code, -1)
            self.assertIn("cannot start labrador-no-such-tool", log_path.read_text(encoding="utf-8"))

    def test_existing_output_directory_is_refused(self):
        run, calls = fake_commands()
        with tempfile.TemporaryDirectory() as directory, self.assertRaises(FileExistsError):
            report.execute([report.Gap("wasm-debug")], Path(directory), Path(directory), runner=run)
        self.assertEqual(calls, [])


class MainTests(unittest.TestCase):
    def main(self, arguments, **keywords):
        stdout, stderr = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            code = report.main(arguments, **keywords)
        return code, stdout.getvalue(), stderr.getvalue()

    def test_plan_runs_nothing_and_lists_every_gap(self):
        def refuse(*_, **__):
            raise AssertionError("the plan must not run a command")
        code, text, _ = self.main(["--preset", "wasm-debug"], root=REPOSITORY, runner=refuse,
                                  git_runner=git_runner(["a" * 40]))
        self.assertEqual(code, 0)
        self.assertIn("Nothing was run for `" + "a" * 40 + "`", text)
        self.assertIn("| `wasm-debug` | whole preset | planned: cmake --preset wasm-debug;", text)
        self.assertIn("| `x64-debug-gl` | RenderPixelTests, MinimalSmokeTests, CollisionSmokeTests on a real driver "
                      "| not run: not selected |", text)
        self.assertEqual(text.count("\n| `"), len(report.GAPS))

    def test_execute_records_commit_dirty_tree_and_exit_code(self):
        run, _ = fake_commands(documents={"x64-release-null": CTEST_JUNIT,
                                          "wasm-debug": junit(list(report.BROWSER_TESTS))})
        with tempfile.TemporaryDirectory() as directory:
            code, text, stderr = self.main(
                ["--execute", "--preset", "wasm-debug", "--preset", "x64-release-null",
                 "--output-dir", "evidence"], root=Path(directory), runner=run,
                git_runner=git_runner(["b" * 40, "b" * 40], dirty=" M engine/a.cpp\n?? b.txt\n"))
            self.assertTrue((Path(directory) / "evidence/wasm-debug.log").is_file())
        self.assertEqual(code, 1)
        self.assertIn("at `" + "b" * 40 + "`, working tree with 2 uncommitted change(s)", text)
        self.assertIn("| `wasm-debug` | whole preset | passed: 15/15 passed; including 15 required tests |", text)
        self.assertIn("| `x64-release-null` | whole preset | failed: 1/6 passed;", text)
        self.assertIn("Logs and JUnit:", stderr)

    def test_head_moving_during_the_run_fails(self):
        run, _ = fake_commands()
        with tempfile.TemporaryDirectory() as directory:
            code, text, stderr = self.main(["--execute", "--preset", "wasm-debug"], root=Path(directory),
                                           runner=run, git_runner=git_runner(["c" * 40, "d" * 40]))
        self.assertEqual((code, text), (2, ""))
        self.assertIn("HEAD moved", stderr)

    def test_output_dir_requires_execute(self):
        with self.assertRaises(SystemExit) as raised, contextlib.redirect_stderr(io.StringIO()):
            report.main(["--output-dir", "x"], root=REPOSITORY, git_runner=git_runner(["a" * 40]))
        self.assertEqual(raised.exception.code, 2)


if __name__ == "__main__":
    unittest.main()
