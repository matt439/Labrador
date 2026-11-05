"""The build retry retries a locked-file failure once and nothing else; no build is run."""

from __future__ import annotations

import contextlib
import io
from pathlib import Path
import unittest

from tools.tests.tool_fixtures import load_tool

REPOSITORY = Path(__file__).resolve().parents[2]
retry = load_tool("tools/ci_build_retry.py")

# The shape of job 125693580862's log: the edge, then the bare text.
LOCKED_LOG = ("FAILED: samples/local_multiplayer/LocalMultiplayerSample.exe\n"
              "C:\\Windows\\system32\\cmd.exe /C \"cd . && vs_link_exe ... && vcpkg.exe z-applocal ...\"\n"
              "Access is denied.\n")
LINK_ERROR = "engine.lib(x.obj) : error LNK2019: unresolved external symbol\n"


def runs(*results):
    calls = []
    answers = iter(results)

    def run(command):
        calls.append(list(command))
        return next(answers)
    return run, calls


def main(run) -> tuple[int, str]:
    output = io.StringIO()
    with contextlib.redirect_stdout(output):
        code = retry.main(["cmake", "--build", "--preset", "x64-debug"], run=run)
    return code, output.getvalue()


class BuildRetry(unittest.TestCase):
    def test_a_passing_build_runs_once(self):
        run, calls = runs((0, "ok\n"))
        self.assertEqual(main(run)[0], 0)
        self.assertEqual(len(calls), 1)

    def test_a_locked_output_is_built_once_more(self):
        for message in retry.LOCKED:
            with self.subTest(message=message):
                run, calls = runs((1, LOCKED_LOG.replace("Access is denied.", message)), (0, ""))
                code, text = main(run)
                self.assertEqual(code, 0)
                self.assertEqual(len(calls), 2)
                self.assertIn("::warning::", text)

    def test_a_second_failure_is_the_result(self):
        run, calls = runs((1, LOCKED_LOG), (1, LOCKED_LOG))
        self.assertEqual(main(run)[0], 1)
        self.assertEqual(len(calls), 2)

    def test_any_other_failure_is_not_retried(self):
        for output in (LINK_ERROR, "error C2065: 'x': undeclared identifier\n",
                       "a message that says Access is denied. in passing\n"):
            with self.subTest(output=output):
                run, calls = runs((1, output))
                self.assertEqual(main(run)[0], 1)
                self.assertEqual(len(calls), 1)

    def test_every_native_build_goes_through_it(self):
        workflow = (REPOSITORY / ".github/workflows/ci.yml").read_text(encoding="utf-8")
        builds = [line.strip() for line in workflow.splitlines()
                  if "cmake --build" in line and "wasm" not in line]
        self.assertEqual(len(builds), 4, builds)
        wrapper = 'python "%GITHUB_WORKSPACE%\\tools\\ci_build_retry.py" cmake --build'
        for line in builds:
            self.assertTrue(line.startswith(wrapper), line)


if __name__ == "__main__":
    unittest.main()
