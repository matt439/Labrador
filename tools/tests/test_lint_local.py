"""tools/lint_local.py against ci.yml's lint job, and its scope and stopping rules."""

from __future__ import annotations

import contextlib
import io
from pathlib import Path
import re
import shlex
import subprocess
import unittest

from tools.tests.tool_fixtures import load_tool

REPOSITORY = Path(__file__).resolve().parents[2]
lint = load_tool("tools/lint_local.py")
WORKSPACE = "${{ github.workspace }}"
# Lines of a run block that only carry an earlier command's failure.
CARRIERS = {"if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }", "exit $LASTEXITCODE"}


# Step keys the parser understands; any other key fails the parse, so a step
# gaining `if:`, `shell:` or `working-directory:` is a deliberate update here.
STEP_KEYS = {"name", "uses", "run", "env", "with", "id"}


def ci_lint_steps(workflow: str) -> list[dict]:
    """ci.yml's lint job as [{name, uses, keys, lines}], each run line stripped.

    A `run: |` block ends at the first non-blank line indented eight spaces
    or less, so a comment or a key after it is never taken as a command.
    """
    job = re.search(r"^  lint:\n(.*?)(?=^  \S)", workflow, re.MULTILINE | re.DOTALL)
    if job is None:
        raise AssertionError("no lint job in ci.yml")
    steps_text = job.group(1).split("\n    steps:\n", 1)[1]
    steps = []
    for chunk in re.split(r"^      - ", steps_text, flags=re.MULTILINE)[1:]:
        lines = chunk.splitlines()
        lines[0] = "        " + lines[0]
        name = uses = ""
        keys: set[str] = set()
        run: list[str] = []
        in_run = False
        for line in lines:
            if not line.strip():
                continue
            indent = len(line) - len(line.lstrip(" "))
            if in_run and indent > 8:
                run.append(line.strip())
                continue
            in_run = False
            if line.lstrip().startswith("#") or indent > 8:
                continue
            key = re.match(r"^        ([A-Za-z_-]+): ?(.*)$", line)
            if key is None:
                if indent < 8:
                    continue
                raise AssertionError(f"unparsed lint step line: {line!r}")
            if key.group(1) not in STEP_KEYS:
                raise AssertionError(f"lint step key {key.group(1)!r} is not modeled by lint_local.py")
            keys.add(key.group(1))
            if key.group(1) == "name":
                name = key.group(2)
            elif key.group(1) == "uses":
                uses = key.group(2)
            elif key.group(1) == "run":
                if key.group(2) == "|":
                    in_run = True
                else:
                    run.append(key.group(2).strip())
        steps.append({"name": name, "uses": uses, "keys": keys, "lines": run})
    return steps


def tokens(line: str) -> tuple[str, ...]:
    return tuple(shlex.split(line.replace(WORKSPACE, "{repo}")))


class DriftTests(unittest.TestCase):
    def test_steps_are_ci_yml_lint_job_in_order(self):
        workflow = (REPOSITORY / ".github/workflows/ci.yml").read_text(encoding="utf-8")
        ci_steps = [step for step in ci_lint_steps(workflow) if not step["uses"]]
        self.assertEqual([step["name"] for step in ci_steps], [step.name for step in lint.STEPS])
        for ci_step, step in zip(ci_steps, lint.STEPS):
            with self.subTest(step=step.name):
                if step.scoped:
                    # CI chooses among PR, push and head-only scopes; each
                    # branch must run this one checker and nothing else.
                    invoked = {tokens(line)[1] for line in ci_step["lines"]
                               if line.startswith("python ")}
                    self.assertEqual(invoked, {step.scoped})
                    self.assertFalse([line for line in ci_step["lines"]
                                      if re.match(r"^(cmake|node|npm|npx)\b", line)])
                    # Each branch passes the scope by these flags, nothing else.
                    flags = {token for line in ci_step["lines"] if line.startswith("python ")
                             for token in tokens(line)[2::2]}
                    self.assertEqual(flags, {"--base", "--head", "--commit"})
                else:
                    # No env, with or id: lint_local runs the command bare.
                    self.assertEqual(ci_step["keys"], {"name", "run"})
                    ci_commands = tuple(tokens(line) for line in ci_step["lines"]
                                        if line not in CARRIERS)
                    self.assertEqual(ci_commands, step.commands)

    def test_parser_sees_a_changed_command(self):
        workflow = (REPOSITORY / ".github/workflows/ci.yml").read_text(encoding="utf-8")
        changed = workflow.replace("cmake/check_whitespace.cmake", "cmake/check_other.cmake", 1)
        step = next(step for step in ci_lint_steps(changed) if step["name"].startswith("Check indentation"))
        self.assertIn("cmake/check_other.cmake", step["lines"][0])

    def test_parser_refuses_an_unmodeled_step_key(self):
        workflow = (REPOSITORY / ".github/workflows/ci.yml").read_text(encoding="utf-8")
        changed = workflow.replace(
            "        run: cmake \"-DREPO_DIR=${{ github.workspace }}\" -P cmake/check_whitespace.cmake",
            "        working-directory: engine\n"
            "        run: cmake \"-DREPO_DIR=${{ github.workspace }}\" -P cmake/check_whitespace.cmake", 1)
        self.assertNotEqual(changed, workflow)
        with self.assertRaisesRegex(AssertionError, "working-directory"):
            ci_lint_steps(changed)

    def test_run_block_ends_before_a_following_comment(self):
        workflow = (REPOSITORY / ".github/workflows/ci.yml").read_text(encoding="utf-8")
        golden = next(step for step in ci_lint_steps(workflow) if "golden" in step["name"])
        self.assertEqual(golden["lines"][-1], "exit $LASTEXITCODE")


class ScopeTests(unittest.TestCase):
    def test_scoped_step_checks_head_only_without_base(self):
        step = lint.STEPS[0]
        self.assertEqual(lint.commands(step, None, "HEAD", python="py"),
                         [["py", "tools/check_golden_reviews.py", "--commit", "HEAD"]])

    def test_scoped_step_checks_range_with_base(self):
        step = lint.STEPS[1]
        self.assertEqual(lint.commands(step, "origin/master", "HEAD", python="py"),
                         [["py", "tools/check_changes_rule.py", "--base", "origin/master", "--head", "HEAD"]])

    def test_placeholders_are_filled(self):
        brief = next(step for step in lint.STEPS if "engine include" in step.name)
        filled = lint.commands(brief, None, "HEAD", repo=Path("C:/repo"), python="py")
        self.assertEqual(filled[0], ["cmake", "-DENGINE_DIR=C:/repo/engine", "-P",
                                     "cmake/check_engine_includes.cmake"])
        last = lint.commands(lint.STEPS[-1], None, "HEAD", python="py")
        self.assertEqual(last, [["py", "-m", "unittest", "discover", "tools/tests"]])


class RunTests(unittest.TestCase):
    def run_main(self, argv, codes):
        calls = []

        def run(command, cwd):
            calls.append(command)
            return subprocess.CompletedProcess(command, codes.get(len(calls), 0))

        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            code = lint.main(argv, run=run)
        return code, calls

    def test_runs_every_command_when_all_pass(self):
        code, calls = self.run_main([], {})
        self.assertEqual(code, 0)
        self.assertEqual(len(calls), sum(len(lint.commands(step, None, "HEAD")) for step in lint.STEPS))

    def test_stops_at_first_failure_with_its_code(self):
        code, calls = self.run_main(["--base", "origin/master"], {3: 7})
        self.assertEqual(code, 7)
        self.assertEqual(len(calls), 3)
        self.assertIn("--base", calls[0])

    def test_list_runs_nothing(self):
        code, calls = self.run_main(["--list"], {})
        self.assertEqual((code, calls), (0, []))

    def test_missing_program_is_reported(self):
        def run(command, cwd):
            raise FileNotFoundError(command[0])

        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()) as err:
            code = lint.main([], run=run)
        self.assertEqual(code, 2)
        self.assertIn("not on PATH", err.getvalue())


if __name__ == "__main__":
    unittest.main()
