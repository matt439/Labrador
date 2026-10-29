"""Run the configurations CI cannot run and print a pull-request section's body.

The default prints the plan and runs nothing. --execute configures, builds and
tests each selected preset in turn, records ctest's JUnit per preset and keeps
going after a failure so the section covers every selected preset. It activates
no toolchain: run it from a shell where vcvars (and emsdk_env for wasm-debug)
are already set. It records the commit and working-tree state, not which device
drew; the person running it names the machine. docs/verification.md owns the
procedure.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass, field
from pathlib import Path
import re
import subprocess
import sys
import uuid
import xml.etree.ElementTree as ElementTree

REPOSITORY = Path(__file__).resolve().parents[1]


@dataclass(frozen=True)
class Gap:
    preset: str
    # Empty means CI never builds this preset, so every test is the gap.
    # Otherwise these tests are the ones CI's job for the preset excludes.
    ci_skips: tuple[str, ...] = ()
    # Tests that must have run and passed. Registration can quietly drop them:
    # a GL or Vulkan run must include what CI skips, and configure registers
    # wasm-debug's browser tests only when it finds a Chromium.
    required: tuple[str, ...] = ()


DEVICE_TESTS = ("RenderPixelTests", "MinimalSmokeTests")
BROWSER_TESTS = (
    "RenderPixelTests", "RenderPixelTestsWithoutS3tc", "RenderContextLossTests", "WebWindowTests",
    "MinimalPageTests", "CollisionPageTests", "AudioPageTests", "LocalMultiplayerPageTests",
    "ComponentsPageTests", "LineSweeperPageTests", "LineSweeperReducedMotionPageTests",
    "LineSweeperPalettePageTests", "LineSweeperBundlePageTests",
)

# What .github/workflows/ci.yml leaves out. tools/tests/test_ci_gap_report.py
# derives the same list from ci.yml and CMakePresets.json and fails on drift.
GAPS = (
    Gap("x64-debug-gl", DEVICE_TESTS, DEVICE_TESTS),
    Gap("x64-debug-vulkan", DEVICE_TESTS, DEVICE_TESTS),
    Gap("x64-release-d3d12"),
    Gap("x64-release-gl"),
    Gap("x64-release-vulkan"),
    Gap("x64-release-null"),
    Gap("wasm-debug", required=BROWSER_TESTS),
)


class ReportError(ValueError):
    """A result file or repository state cannot be summarised honestly."""


@dataclass
class Outcome:
    preset: str
    result: str = "not run"
    detail: str = ""
    counts: dict[str, int] = field(default_factory=dict)
    junit_path: str = ""


def commands(preset: str, junit: Path) -> list[list[str]]:
    return [["cmake", "--preset", preset],
            ["cmake", "--build", "--preset", preset],
            ["ctest", "--preset", preset, "--output-junit", str(junit)]]


def junit_summary(text: str) -> dict[str, list[str]]:
    """Group ctest's JUnit cases by outcome without rejecting failures."""
    try:
        document = ElementTree.fromstring(text)
    except ElementTree.ParseError as error:
        raise ReportError(f"invalid JUnit XML: {error}") from error
    if document.tag == "testsuites" and len(document) == 1:
        document = document[0]
    if document.tag != "testsuite":
        raise ReportError("unsupported JUnit shape; require one testsuite")
    groups: dict[str, list[str]] = {"passed": [], "failed": [], "skipped": [], "disabled": []}
    seen: set[str] = set()
    for case in document:
        if case.tag != "testcase":
            continue
        name = case.get("name") or ""
        if not name.strip() or name in seen:
            raise ReportError(f"empty or duplicate JUnit testcase: {name!r}")
        seen.add(name)
        status = case.get("status")
        children = {child.tag for child in case}
        if "failure" in children or "error" in children or status == "fail":
            groups["failed"].append(name)
        elif status == "disabled":
            groups["disabled"].append(name)
        elif "skipped" in children or status == "notrun":
            groups["skipped"].append(name)
        elif status in (None, "run", "passed"):
            groups["passed"].append(name)
        else:
            raise ReportError(f"unsupported JUnit status for {name}: {status!r}")
    if not seen:
        raise ReportError("JUnit names no test cases")
    return groups


def judge(gap: Gap, groups: dict[str, list[str]]) -> tuple[str, str]:
    total = sum(len(names) for names in groups.values())
    problems = []
    for key in ("failed", "skipped", "disabled"):
        if groups[key]:
            problems.append(f"{key}: {', '.join(groups[key])}")
    missing = [name for name in gap.required if name not in groups["passed"]
               and not any(name in groups[key] for key in ("failed", "skipped", "disabled"))]
    if missing:
        problems.append(f"not registered: {', '.join(missing)}")
    summary = f"{len(groups['passed'])}/{total} passed"
    if problems:
        return "failed", summary + "; " + "; ".join(problems)
    if gap.ci_skips:
        summary += "; " + " and ".join(gap.ci_skips) + " passed"
    elif gap.required:
        summary += f"; including {len(gap.required)} required tests"
    return "passed", summary


def git_state(root: Path, runner=None) -> tuple[str, int]:
    run = runner or (lambda argv: subprocess.run(argv, cwd=root, capture_output=True,
                                                 text=True, check=False, shell=False))
    head = run(["git", "rev-parse", "HEAD"])
    status = run(["git", "status", "--porcelain", "--untracked-files=normal"])
    if head.returncode != 0 or status.returncode != 0:
        raise ReportError("cannot read the checkout's HEAD and status")
    commit = head.stdout.strip()
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise ReportError(f"unexpected HEAD: {commit!r}")
    changed = [line for line in status.stdout.splitlines() if line.strip()]
    return commit, len(changed)


def run_command(argv: list[str], *, cwd: Path, log) -> int:
    try:
        return subprocess.run(argv, cwd=cwd, stdout=log, stderr=subprocess.STDOUT,
                              shell=False, check=False).returncode
    except OSError as error:
        # A tool missing from this shell fails the preset, not the whole record.
        log.write(f"cannot start {argv[0]}: {error}\n")
        return -1


def execute(gaps: list[Gap], root: Path, output_dir: Path, *, runner=None) -> list[Outcome]:
    run = runner or run_command
    output_dir.mkdir(parents=True, exist_ok=False)
    outcomes = []
    for gap in gaps:
        outcome = Outcome(gap.preset)
        junit = output_dir / f"{gap.preset}-junit.xml"
        log_path = output_dir / f"{gap.preset}.log"
        ctest_code = 0
        with log_path.open("x", encoding="utf-8") as log:
            for argv in commands(gap.preset, junit):
                log.write("> " + " ".join(argv) + "\n")
                log.flush()
                code = run(argv, cwd=root, log=log)
                if argv[0] == "ctest":
                    ctest_code = code
                # A failing ctest still writes its JUnit, which says which tests.
                if code != 0 and argv[0] != "ctest":
                    outcome.result = "failed"
                    outcome.detail = f"`{argv[0]} {argv[1]}` exited {code}; see {log_path.name}"
                    break
        if not outcome.detail:
            try:
                groups = junit_summary(junit.read_text(encoding="utf-8"))
                outcome.result, outcome.detail = judge(gap, groups)
                outcome.counts = {key: len(names) for key, names in groups.items()}
                outcome.junit_path = str(junit)
                if ctest_code != 0 and outcome.result == "passed":
                    outcome.result = "failed"
                    outcome.detail += f"; ctest exited {ctest_code}; see {log_path.name}"
            except (OSError, UnicodeError, ReportError) as error:
                outcome.result = "failed"
                outcome.detail = f"no usable JUnit ({error}); see {log_path.name}"
        outcomes.append(outcome)
    return outcomes


def markdown(outcomes: list[Outcome], *, commit: str, changed: int, executed: bool) -> str:
    scope = {gap.preset: (", ".join(gap.ci_skips) if gap.ci_skips else "whole preset") for gap in GAPS}
    if changed:
        tree = f"working tree with {changed} uncommitted change(s), so this describes that tree"
    else:
        tree = "clean working tree"
    lines = []
    if executed:
        lines += [f"`python tools/ci_gap_report.py --execute` at `{commit}`, {tree}.",
                  "Each preset was configured, built and tested in this run; results are ctest's JUnit.", ""]
    else:
        lines += [f"Nothing was run for `{commit}`; this is the plan.", ""]
    lines += ["| Preset | What CI misses | Result |", "|---|---|---|"]
    for outcome in outcomes:
        result = outcome.result if not outcome.detail else f"{outcome.result}: {outcome.detail}"
        lines.append(f"| `{outcome.preset}` | {scope[outcome.preset]} | {result} |")
    lines += ["", "Device: <!-- adapter, driver and whether a fallback rasteriser was possible -->", ""]
    return "\n".join(lines)


def main(arguments: list[str] | None = None, *, root: Path | None = None, runner=None,
         git_runner=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--preset", action="append", choices=[gap.preset for gap in GAPS],
                        help="run only this preset; repeat for several (default: all)")
    parser.add_argument("--execute", action="store_true",
                        help="configure, build and test the selected presets")
    parser.add_argument("--output-dir", help="new directory for logs and JUnit (with --execute)")
    options = parser.parse_args(arguments)
    if options.output_dir and not options.execute:
        parser.error("--output-dir requires --execute")
    root = root or REPOSITORY
    selected = [gap for gap in GAPS if not options.preset or gap.preset in options.preset]
    try:
        commit, changed = git_state(root, git_runner)
        if not options.execute:
            outcomes = [Outcome(gap.preset, "planned",
                                "; ".join(" ".join(argv) for argv in commands(gap.preset, Path("<junit>"))))
                        for gap in selected]
        else:
            target = Path(options.output_dir) if options.output_dir else \
                Path("out/ci-gap") / uuid.uuid4().hex
            target = (root / target).resolve()
            outcomes = execute(selected, root, target, runner=runner)
            after = git_state(root, git_runner)
            if after[0] != commit:
                raise ReportError(f"HEAD moved from {commit} to {after[0]} during the run")
            changed = max(changed, after[1])
        chosen = {outcome.preset for outcome in outcomes}
        outcomes += [Outcome(gap.preset, "not run", "not selected") for gap in GAPS
                     if gap.preset not in chosen]
        outcomes.sort(key=lambda outcome: [gap.preset for gap in GAPS].index(outcome.preset))
    except (OSError, ReportError) as error:
        print(f"ci_gap_report: {error}", file=sys.stderr)
        return 2
    sys.stdout.write(markdown(outcomes, commit=commit, changed=changed,
                              executed=options.execute))
    if options.execute:
        print(f"Logs and JUnit: {target}", file=sys.stderr)
        return 0 if all(outcome.result in ("passed", "not run") for outcome in outcomes) else 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
