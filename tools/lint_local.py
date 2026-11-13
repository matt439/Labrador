"""Run CI's lint job locally: the same steps, in ci.yml's order, stopping at the first failure.

No configure, compiler or network is needed: the steps are Python, CMake
script mode and Node, and cmake and node must be on PATH (vcvars puts CMake
there). Run from anywhere; commands run at the repository root.

The two per-commit checks (golden-image review declarations and CHANGES.md's
rule) need a scope. With --base BASE they check every commit BASE..HEAD
introduces, as CI does for a pull request; without it they check only the
--head commit (HEAD by default), as CI does for a scheduled run.

tools/tests/test_lint_local.py fails when STEPS and ci.yml's lint job differ.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
import subprocess
import sys

REPOSITORY = Path(__file__).resolve().parents[1]


@dataclass(frozen=True)
class Step:
    """One step of ci.yml's lint job, named as ci.yml names it.

    A scoped step runs one per-commit checker over the --base/--head or
    --commit scope. Any other step runs its commands in order; `{repo}`
    stands for the repository root, as `${{ github.workspace }}` does in CI,
    and `python` for this interpreter.
    """

    name: str
    commands: tuple[tuple[str, ...], ...] = ()
    scoped: str = ""


def _cmake(variable: str, value: str, script: str) -> tuple[str, ...]:
    return ("cmake", f"-D{variable}={value}", "-P", script)


STEPS: tuple[Step, ...] = (
    Step("Check per-commit golden review declarations", scoped="tools/check_golden_reviews.py"),
    Step("Check CHANGES.md's rule for each commit", scoped="tools/check_changes_rule.py"),
    Step("Check every document a comment cites resolves",
         (_cmake("REPO_DIR", "{repo}", "cmake/check_doc_citations.cmake"),)),
    Step("Check the agent brief's import, its ctest counts and failing fixtures",
         (_cmake("REPO_DIR", "{repo}", "cmake/check_agent_brief.cmake"),
          _cmake("REPO_DIR", "{repo}", "cmake/check_agent_brief_fixtures.cmake"))),
    Step("Check engine include boundaries and failing fixtures",
         (_cmake("ENGINE_DIR", "{repo}/engine", "cmake/check_engine_includes.cmake"),
          _cmake("REPO_DIR", "{repo}", "cmake/check_engine_include_fixtures.cmake"))),
    Step("Check LineSweeper include layers and failing fixtures",
         (_cmake("REPO_DIR", "{repo}", "cmake/check_linesweeper_layers.cmake"),
          _cmake("REPO_DIR", "{repo}", "cmake/check_linesweeper_layer_fixtures.cmake"))),
    Step("Check source revision and archive metadata without a compiler",
         (_cmake("REPO_DIR", "{repo}", "cmake/check_source_revision.cmake"),)),
    Step("Check indentation and trailing whitespace against .editorconfig",
         (_cmake("REPO_DIR", "{repo}", "cmake/check_whitespace.cmake"),)),
    Step("Check the flash definitions against synthetic frames",
         (("node", "--test", "tools/flash_check/flash_check.test.mjs"),)),
    Step("Check browser pacing reduction against synthetic runs",
         (("node", "--test", "tools/web_pacing/analyse.test.mjs", "tools/web_pacing/page.test.mjs",
           "tools/web_pacing/page_input.test.mjs"),)),
    Step("Check web bundle verification against offline fixtures",
         (("node", "--test", "tools/web_bundle/bundle.test.mjs", "tools/web_bundle/check_live.test.mjs",
           "tests/render/web/bundle_host.test.mjs"),)),
    Step("Run the offline tooling tests",
         (("python", "-m", "unittest", "discover", "tools/tests"),)),
)


def commands(step: Step, base: str | None, head: str, repo: Path = REPOSITORY,
             python: str = sys.executable) -> list[list[str]]:
    """The argument lists one step runs, with placeholders filled in."""
    if step.scoped:
        scope = ["--base", base, "--head", head] if base else ["--commit", head]
        return [[python, step.scoped, *scope]]
    root = repo.as_posix()
    filled = []
    for command in step.commands:
        argv = [argument.replace("{repo}", root) for argument in command]
        if argv[0] == "python":
            argv[0] = python
        filled.append(argv)
    return filled


def main(argv: list[str] | None = None, run=subprocess.run) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--base", help="check each commit BASE..HEAD introduces, e.g. origin/master")
    parser.add_argument("--head", default="HEAD", help="the head commit (default HEAD)")
    parser.add_argument("--list", action="store_true", help="print the commands without running them")
    options = parser.parse_args(argv)

    total = len(STEPS)
    for number, step in enumerate(STEPS, start=1):
        print(f"[{number}/{total}] {step.name}", flush=True)
        for command in commands(step, options.base, options.head):
            if options.list:
                print("  " + subprocess.list2cmdline(command))
                continue
            try:
                code = run(command, cwd=REPOSITORY).returncode
            except FileNotFoundError:
                print(f"lint_local: {command[0]} is not on PATH", file=sys.stderr)
                return 2
            if code != 0:
                print(f"lint_local: step {number} failed with exit code {code}: {step.name}",
                      file=sys.stderr)
                return code
    if not options.list:
        print(f"lint_local: all {total} steps passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
