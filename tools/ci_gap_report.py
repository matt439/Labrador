"""Run the configurations CI cannot run and print a pull-request section's body.

The default prints the plan and runs nothing. --execute configures, builds and
tests each selected preset in turn, records ctest's JUnit per preset and keeps
going after a failure so the section covers every selected preset. It activates
no toolchain: run it from a shell where vcvars (and emsdk_env for wasm-debug)
are already set. With wasm-debug selected it refuses to start while MSYSTEM
is set. It records the commit, the working-tree state and the device each
preset's RenderPixelTests says it drew on; the person running it adds the
driver version. docs/verification.md owns the procedure.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass, field
import json
import os
from pathlib import Path
import subprocess
import sys
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parent))
import check_ctest_inventory  # noqa: E402
import verification_runner  # noqa: E402

REPOSITORY = Path(__file__).resolve().parents[1]


@dataclass(frozen=True)
class Gap:
    preset: str
    # Empty means CI never builds this preset, so every test is the gap.
    # Otherwise these tests are the ones CI's job for the preset runs only on
    # a software rasteriser, so a real driver is the gap.
    real_driver: tuple[str, ...] = ()
    # Tests that must have run and passed. Registration can quietly drop them:
    # a GL or Vulkan run must include the device tests.
    required: tuple[str, ...] = ()
    # For a preset CI never builds, the exact set of entries ctest must
    # register, from tests/ctest_inventory.json: one missing or one extra
    # fails the preset. Configure registers wasm-debug's browser pages only
    # when it finds a Chromium and a Firefox, so without them its headless
    # tests would otherwise pass alone.
    expected: tuple[str, ...] = ()


DEVICE_TESTS = ("RenderPixelTests", "MinimalSmokeTests", "CollisionSmokeTests")
INVENTORY = json.loads((REPOSITORY / "tests/ctest_inventory.json").read_text(encoding="utf-8"))


def whole_preset(preset: str) -> Gap:
    return Gap(preset, expected=tuple(sorted(check_ctest_inventory.expected_tests(INVENTORY, preset))))


# What .github/workflows/ci.yml leaves out. tools/tests/test_ci_gap_report.py
# derives the same list from ci.yml and CMakePresets.json and fails on drift.
GAPS = (
    Gap("x64-debug-gl", DEVICE_TESTS, DEVICE_TESTS),
    Gap("x64-debug-vulkan", DEVICE_TESTS, DEVICE_TESTS),
    whole_preset("x64-release-d3d12"),
    whole_preset("x64-release-gl"),
    whole_preset("x64-release-vulkan"),
    whole_preset("x64-release-null"),
    whole_preset("wasm-debug"),
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
    # verification_runner.device_identity's record, or None where the run
    # never produced a readable JUnit.
    device: dict | None = None


def commands(preset: str, junit: Path) -> list[list[str]]:
    return [["cmake", "--preset", preset],
            ["cmake", "--build", "--preset", preset],
            ["ctest", "--preset", preset, *verification_runner.PASSED_OUTPUT_LIMIT,
             "--output-junit", str(junit)]]


def junit_summary(text: str) -> dict[str, list[str]]:
    """Group ctest's JUnit cases by outcome without rejecting failures.

    verification_runner.junit_cases reads the document, so this report and
    the render sweep accept the same shapes."""
    return group_cases(verification_runner.junit_cases(text, error=ReportError)[0])


def group_cases(cases: list[dict]) -> dict[str, list[str]]:
    groups: dict[str, list[str]] = {"passed": [], "failed": [], "skipped": [], "disabled": []}
    for case in cases:
        name, status = case["name"], case["status"]
        problem = case["problem"][0] if case["problem"] else None
        if problem in ("failure", "error") or status == "fail":
            groups["failed"].append(name)
        elif status == "disabled":
            groups["disabled"].append(name)
        elif problem == "skipped" or status == "notrun":
            groups["skipped"].append(name)
        elif status in (None, "run", "passed"):
            groups["passed"].append(name)
        else:
            raise ReportError(f"unsupported JUnit status for {name}: {status!r}")
    if not cases:
        raise ReportError("JUnit names no test cases")
    return groups


def judge(gap: Gap, groups: dict[str, list[str]]) -> tuple[str, str]:
    total = sum(len(names) for names in groups.values())
    registered = {name for names in groups.values() for name in names}
    problems = []
    for key in ("failed", "skipped", "disabled"):
        if groups[key]:
            problems.append(f"{key}: {', '.join(groups[key])}")
    wanted = list(gap.required) + [name for name in gap.expected if name not in gap.required]
    missing = [name for name in wanted if name not in registered]
    if missing:
        problems.append(f"not registered: {', '.join(missing)}")
    if gap.expected:
        extra = sorted(registered - set(gap.expected))
        if extra:
            problems.append(f"not in the inventory: {', '.join(extra)}")
    summary = f"{len(groups['passed'])}/{total} passed"
    if problems:
        return "failed", summary + "; " + "; ".join(problems)
    if gap.real_driver:
        names = list(gap.real_driver)
        spoken = names[0] if len(names) == 1 else ", ".join(names[:-1]) + " and " + names[-1]
        summary += "; " + spoken + " passed"
    elif gap.expected:
        summary += "; the inventory's entries exactly"
    return "passed", summary


def environment_problem(gaps: list[Gap], environment) -> str:
    """Why this shell cannot build a selected wasm preset, or an empty string.

    A cmd started from Git Bash inherits MSYSTEM; emsdk_env.bat then prints
    bash exports instead of setting the environment, and emcc is not on PATH."""
    wasm = [gap.preset for gap in gaps if gap.preset.startswith("wasm-")]
    value = environment.get("MSYSTEM", "")
    if not wasm or not value:
        return ""
    return (f"MSYSTEM={value!r} is inherited, so emsdk_env.bat leaves emcc off PATH for "
            f"{', '.join(wasm)}; run `set MSYSTEM=` before emsdk_env.bat")


def git_state(root: Path, runner=None) -> tuple[str, int]:
    return verification_runner.source_revision(root, runner, error=ReportError)


def run_command(argv: list[str], *, cwd: Path, log) -> int:
    try:
        return verification_runner.run_command(argv, cwd=str(cwd), stdout=log, stderr=subprocess.STDOUT)
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
                cases, _ = verification_runner.junit_cases(junit.read_text(encoding="utf-8"), error=ReportError)
                groups = group_cases(cases)
                outcome.device = verification_runner.device_identity(cases)
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


def device_text(outcome: Outcome) -> str:
    device = outcome.device
    if device is None:
        return "not reached"
    if device["result"] == "collected":
        return "; ".join(device["devices"])
    if device["result"] == "not-printed":
        return "RenderPixelTests printed no device line"
    return "no test here draws on a device"


def markdown(outcomes: list[Outcome], *, commit: str, changed: int, executed: bool) -> str:
    scope = {gap.preset: (", ".join(gap.real_driver) + " on a real driver" if gap.real_driver else "whole preset")
             for gap in GAPS}
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
    lines.append("")
    if executed:
        lines.append("Devices, as each preset's RenderPixelTests named them:")
        lines += [f"- `{outcome.preset}`: {device_text(outcome)}" for outcome in outcomes
                  if outcome.result != "not run"]
        lines.append("")
    lines += ["Driver: <!-- driver version; the tests do not print it -->", ""]
    return "\n".join(lines)


def main(arguments: list[str] | None = None, *, root: Path | None = None, runner=None,
         git_runner=None, environment=None) -> int:
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
            problem = environment_problem(selected, os.environ if environment is None else environment)
            if problem:
                raise ReportError(problem)
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
