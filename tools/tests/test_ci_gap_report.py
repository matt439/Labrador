"""CI-gap list drift, JUnit grouping and fake-command execution; no tool is run."""

from __future__ import annotations

import contextlib
import importlib.util
import io
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

REPOSITORY = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("ci_gap_report", REPOSITORY / "tools/ci_gap_report.py")
report = importlib.util.module_from_spec(SPEC)
# Dataclass creation looks its module up while the module is executing.
sys.modules[SPEC.name] = report
SPEC.loader.exec_module(report)

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
            skip = re.search(r"^\s*skip_tests: '(.*)'$", found.group(3), re.MULTILINE)
            matrix.append((found.group(2), skip.group(1) if skip else ""))
        self.assertTrue(matrix, "no matrix entries found in ci.yml")
        ci_presets = {preset for preset, _ in matrix}
        # Jobs outside the matrix name their preset literally.
        ci_presets |= set(re.findall(r"ctest --preset ([A-Za-z0-9-]+)", workflow))
        presets = json.loads((REPOSITORY / "CMakePresets.json").read_text(encoding="utf-8"))
        tested = {preset["name"] for preset in presets["testPresets"]}

        gaps = {gap.preset: gap for gap in report.GAPS}
        whole = {name for name, gap in gaps.items() if not gap.ci_skips}
        self.assertEqual(whole, tested - ci_presets)
        skipped = {}
        for preset, skip in matrix:
            if skip:
                found = re.fullmatch(r'-E "\^\(([A-Za-z0-9|]+)\)\$"', skip)
                self.assertIsNotNone(found, f"unrecognised skip_tests for {preset}: {skip}")
                skipped[preset] = tuple(found.group(1).split("|"))
        self.assertEqual(skipped, {name: gap.ci_skips for name, gap in gaps.items() if gap.ci_skips})
        for gap in report.GAPS:
            self.assertTrue(set(gap.ci_skips) <= set(gap.required), gap.preset)


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

    def test_required_tests_must_have_run_and_passed(self):
        gl = report.GAPS[0]
        result, detail = report.judge(gl, report.junit_summary(junit(["CoreTests", "RenderPixelTests"])))
        self.assertEqual(result, "failed")
        self.assertIn("not registered: MinimalSmokeTests", detail)
        result, detail = report.judge(gl, report.junit_summary(
            junit(["CoreTests", "RenderPixelTests", "MinimalSmokeTests"])))
        self.assertEqual((result, detail),
                         ("passed", "3/3 passed; RenderPixelTests and MinimalSmokeTests passed"))
        # Without a Chromium, configure registers 19 of wasm-debug's 32 tests.
        wasm = report.GAPS[-1]
        unregistered = [f"Headless{index}" for index in range(19)]
        result, detail = report.judge(wasm, report.junit_summary(junit(unregistered)))
        self.assertEqual(result, "failed")
        self.assertIn("19/19 passed; not registered: RenderPixelTests,", detail)
        result, detail = report.judge(wasm, report.junit_summary(
            junit(unregistered + list(report.BROWSER_TESTS))))
        self.assertEqual((result, detail), ("passed", "32/32 passed; including 13 required tests"))
        result, detail = report.judge(report.Gap("x64-release-null"), report.junit_summary(CTEST_JUNIT))
        self.assertEqual(result, "failed")
        self.assertEqual(detail, "1/6 passed; failed: Bad, Slow; skipped: Missing, Skipper; disabled: Off")


class ExecuteTests(unittest.TestCase):
    def fake(self, *, configure_fails=(), ctest_code=0, documents=None):
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

    def test_a_failed_preset_does_not_stop_the_next(self):
        run, calls = self.fake(configure_fails={"x64-release-gl"})
        gaps = [report.Gap("x64-release-gl"), report.Gap("x64-release-null")]
        with tempfile.TemporaryDirectory() as directory:
            outcomes = report.execute(gaps, Path(directory), Path(directory) / "out", runner=run)
            self.assertTrue((Path(directory) / "out/x64-release-null-junit.xml").is_file())
        self.assertEqual([outcome.result for outcome in outcomes], ["failed", "passed"])
        self.assertIn("`cmake --preset` exited 3", outcomes[0].detail)
        self.assertEqual(len(calls), 4)

    def test_nonzero_ctest_with_clean_junit_is_a_failure(self):
        run, _ = self.fake(ctest_code=8)
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
        run, calls = self.fake()
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
        self.assertIn("| `x64-debug-gl` | RenderPixelTests, MinimalSmokeTests | not run: not selected |", text)
        self.assertEqual(text.count("\n| `"), len(report.GAPS))

    def test_execute_records_commit_dirty_tree_and_exit_code(self):
        run, _ = ExecuteTests().fake(documents={"x64-release-null": CTEST_JUNIT,
                                                "wasm-debug": junit(list(report.BROWSER_TESTS))})
        with tempfile.TemporaryDirectory() as directory:
            code, text, stderr = self.main(
                ["--execute", "--preset", "wasm-debug", "--preset", "x64-release-null",
                 "--output-dir", "evidence"], root=Path(directory), runner=run,
                git_runner=git_runner(["b" * 40, "b" * 40], dirty=" M engine/a.cpp\n?? b.txt\n"))
            self.assertTrue((Path(directory) / "evidence/wasm-debug.log").is_file())
        self.assertEqual(code, 1)
        self.assertIn("at `" + "b" * 40 + "`, working tree with 2 uncommitted change(s)", text)
        self.assertIn("| `wasm-debug` | whole preset | passed: 13/13 passed; including 13 required tests |", text)
        self.assertIn("| `x64-release-null` | whole preset | failed: 1/6 passed;", text)
        self.assertIn("Logs and JUnit:", stderr)

    def test_head_moving_during_the_run_fails(self):
        run, _ = ExecuteTests().fake()
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
