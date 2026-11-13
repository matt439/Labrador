"""Explicit verification-sweep execution; callers supply the repository planner's checks.

No toolchain activation, retries, cache repair or hardware acceptance is inferred.
The supported CTest JSON/JUnit subset is documented in docs/verification.md.
"""

from __future__ import annotations

import copy
import json
import math
from pathlib import Path
import re
import subprocess
import sys
import uuid
import xml.etree.ElementTree as ElementTree

sys.path.insert(0, str(Path(__file__).resolve().parent))
import git_local  # noqa: E402


class VerificationError(ValueError):
    """A command's selected configurations or recorded results are incomplete."""


def cmake_boolean(value) -> bool:
    if isinstance(value, bool):
        return value
    if isinstance(value, int) or isinstance(value, float) and math.isfinite(value):
        return value != 0
    if isinstance(value, str):
        upper = value.upper()
        if upper in {"", "0", "OFF", "NO", "FALSE", "N", "IGNORE", "NOTFOUND"} or upper.endswith("-NOTFOUND"):
            return False
        if upper in {"1", "ON", "YES", "TRUE", "Y"}:
            return True
    raise VerificationError(f"unsupported DISABLED value: {value!r}")


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise VerificationError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def registration_inventory(text: str, configuration: dict, *, selected_browser: bool) -> list[dict]:
    def bad_constant(value):
        raise VerificationError(f"invalid JSON constant: {value}")

    try:
        data = json.loads(text, object_pairs_hook=unique_object, parse_constant=bad_constant)
    except json.JSONDecodeError as error:
        raise VerificationError(f"invalid CTest registration JSON: {error}") from error
    version = data.get("version") if isinstance(data, dict) else None
    if (not isinstance(data, dict) or data.get("kind") != "ctestInfo"
            or not isinstance(version, dict) or set(version) != {"major", "minor"}
            or any(type(value) is not int for value in version.values())
            or version != {"major": 1, "minor": 0}):
        raise VerificationError("unsupported CTest registration kind/version; require ctestInfo JSON 1.0")
    tests = data.get("tests")
    if not isinstance(tests, list) or not tests:
        raise VerificationError("CTest registered no selected tests")
    inventory = []
    names = set()
    for test in tests:
        if not isinstance(test, dict):
            raise VerificationError("invalid CTest registration entry")
        name, command = test.get("name"), test.get("command")
        if not isinstance(name, str) or not name.strip() or name in names:
            raise VerificationError(f"empty or duplicate registered test name: {name!r}")
        if (not isinstance(command, list) or not command or not isinstance(command[0], str)
                or not command[0].strip() or any(not isinstance(argument, str) for argument in command)):
            raise VerificationError(f"registered test has no executable argv: {name}")
        names.add(name)
        properties = test.get("properties", [])
        if not isinstance(properties, list):
            raise VerificationError(f"invalid properties for {name}")
        values = {}
        for item in properties:
            if (not isinstance(item, dict) or not isinstance(item.get("name"), str)
                    or not item["name"].strip() or item["name"] in values or "value" not in item):
                raise VerificationError(f"invalid or duplicate property for {name}")
            values[item["name"]] = item["value"]
        if cmake_boolean(values.get("DISABLED", False)):
            raise VerificationError(f"selected test is disabled: {name}")
        inventory.append({"name": name, "command": command, "properties": properties})
    required = set(configuration["required_test_names"] + configuration["required_browser_test_names"])
    conditional = set(configuration["conditional_browser_test_names"])
    if selected_browser or names & conditional:
        required |= conditional
    missing, forbidden = required - names, names & set(configuration["forbidden_test_names"])
    if missing:
        raise VerificationError("missing required registrations: " + ", ".join(sorted(missing)))
    if forbidden:
        raise VerificationError("forbidden registrations: " + ", ".join(sorted(forbidden)))
    return inventory


JUNIT_OUTPUT = {"system-out", "system-err", "properties"}
JUNIT_PROBLEMS = {"failure", "error", "skipped"}


def junit_cases(text: str, *, error: type[Exception] = VerificationError) -> tuple[list[dict], list]:
    """Read ctest's --output-junit document without judging its outcomes.

    The shape is one testsuite, optionally inside a single testsuites
    container, holding testcase elements with nonempty unique names. Each case
    is {"name", "status", "problem", "output"}, where problem is the (tag,
    reason) of its first failure, error or skipped child, or None, and output
    joins its system-out text. The aggregates are the
    elements whose count attributes describe the suite. Anything else raises
    `error`, so both readers here refuse the same unknown shapes."""
    try:
        document = ElementTree.fromstring(text)
    except ElementTree.ParseError as failure:
        raise error(f"invalid JUnit XML: {failure}") from failure
    if document.tag == "testsuite":
        suite, aggregates = document, [document]
    elif (document.tag == "testsuites" and len(document) == 1
          and document[0].tag == "testsuite"):
        suite, aggregates = document[0], [document, document[0]]
    else:
        raise error("unsupported JUnit shape; require one testsuite")
    cases = []
    seen = set()
    for case in suite:
        if case.tag in JUNIT_OUTPUT:
            continue
        if case.tag != "testcase":
            raise error(f"unsupported JUnit suite child: {case.tag}")
        name = case.get("name")
        if not name or not name.strip() or name in seen:
            raise error(f"empty or duplicate JUnit testcase: {name!r}")
        seen.add(name)
        problem = None
        output = []
        for child in case:
            if child.tag in JUNIT_PROBLEMS:
                if problem is None:
                    problem = (child.tag, child.get("message") or (child.text or "").strip())
            elif child.tag not in JUNIT_OUTPUT:
                raise error(f"unsupported JUnit testcase child: {child.tag}")
            elif child.tag == "system-out" and child.text:
                output.append(child.text)
        cases.append({"name": name, "status": case.get("status"), "problem": problem,
                      "output": "\n".join(output)})
    return cases, aggregates


def junit_results(text: str, names: set[str]) -> list[dict]:
    cases, aggregates = junit_cases(text)
    results = []
    seen = set()
    for case in cases:
        name, status = case["name"], case["status"]
        seen.add(name)
        if status is not None and status not in {"run", "passed"}:
            raise VerificationError(f"JUnit testcase did not run successfully: {name} status={status!r}")
        if case["problem"]:
            tag, reason = case["problem"]
            raise VerificationError(f"JUnit {tag}: {name}: {reason}")
        results.append({"name": name, "result": "passed"})
    if not seen or seen != names:
        raise VerificationError(f"JUnit inventory differs: missing={sorted(names - seen)}, extra={sorted(seen - names)}")
    for aggregate in aggregates:
        for key in ("tests", "failures", "errors", "skipped", "disabled"):
            value = aggregate.get(key)
            if value is None:
                continue
            wanted = len(seen) if key == "tests" else 0
            if not value.isdecimal() or int(value) != wanted:
                raise VerificationError(f"JUnit aggregate {key}={value!r} contradicts successful inventory")
    return results


# RenderPixelTests prints the device it drew on as a doctest MESSAGE, natively
# and through the browser page runner alike (tests/render/pixel_tests.cpp).
DEVICE_LINE = re.compile(r"MESSAGE: Drawing on (.+)$")
DEVICE_TEST = "RenderPixelTests"
# CTest keeps 1024 bytes of a passing test's output by default, which can cut
# the device line off; JUnit's system-out holds only what CTest kept.
PASSED_OUTPUT_LIMIT = ["--test-output-size-passed", "1048576"]


def device_names(cases: list[dict]) -> list[str]:
    """The distinct devices the cases' output says they drew on, in order."""
    names = []
    for case in cases:
        for line in case["output"].splitlines():
            found = DEVICE_LINE.search(line.rstrip())
            if found and found.group(1).strip() not in names:
                names.append(found.group(1).strip())
    return names


def device_identity(cases: list[dict]) -> dict:
    """What a configuration's JUnit says drew: the device names, or why none."""
    names = device_names(cases)
    if names:
        return {"result": "collected", "devices": names}
    if any(case["name"] == DEVICE_TEST for case in cases):
        return {"result": "not-printed", "devices": [],
                "detail": f"{DEVICE_TEST} ran but its output has no 'Drawing on' line"}
    return {"result": "no-device-test", "devices": [],
            "detail": f"{DEVICE_TEST} is not registered in this configuration"}


def source_revision(root: Path, runner=None, *, error: type[Exception] = VerificationError) -> tuple[str, int]:
    """The checkout's HEAD and how many entries `git status` lists."""
    run = runner or (lambda argv: git_local.git(root, *argv[1:], text=True, error=error))
    head = run(["git", "rev-parse", "HEAD"])
    status = run(["git", "status", "--porcelain", "--untracked-files=normal"])
    if head.returncode != 0 or status.returncode != 0:
        raise error("cannot read the checkout's HEAD and status")
    commit = head.stdout.strip()
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise error(f"unexpected HEAD: {commit!r}")
    changed = [line for line in status.stdout.splitlines() if line.strip()]
    return commit, len(changed)


def run_command(argv: list[str], *, cwd: str, stdout, stderr) -> int:
    return subprocess.run(argv, cwd=cwd, stdout=stdout, stderr=stderr,
                          shell=False, check=False).returncode


def execute_plan(plan: dict, *, inspector, cache_reader, output_dir: str | Path | None = None,
                 runner=None, revision_reader=None) -> dict:
    """Execute generated argv serially; fake runners can supply all command results.

    revision_reader(root) returns (HEAD, uncommitted entry count) and defaults
    to reading Git. Each configuration's device identity stays "uncollected"
    until its JUnit has been read."""
    report = copy.deepcopy(plan)
    report.update({"mode": "execute", "result": "not-run", "exit_code": 1,
                   "failure": None, "report_path": None, "output_dir": None,
                   "source_revision": {"result": "uncollected"},
                   "device_identity": {configuration["id"]: {"result": "uncollected", "devices": []}
                                       for configuration in report["configurations"]},
                   "success_scope": "modeled commands and result checks only; manual acceptance is separate"})
    report["manual_obligations"] = [item for item in report["manual_obligations"]
                                    if item["id"] != "registration-before-future-execution"]
    root = Path(report["configurations"][0]["steps"][0]["cwd"])
    run = runner or run_command
    read_revision = revision_reader or source_revision
    current = None
    active = None
    phase = "source-revision"
    for configuration in report["configurations"]:
        configuration["validations"] = {name: {"result": "not-run"}
                                         for name in ("cache", "post-build-cache", "registration", "test-results")}
    try:
        commit, changed = read_revision(root)
        report["source_revision"] = {"result": "collected", "commit": commit, "uncommitted_changes": changed}
        phase = "preflight"
        checks = inspector(report)
        report["preflight"] = {"enabled": True, "checks": checks}
        blocking = [item for item in checks if item["blocking"]]
        if blocking:
            raise VerificationError(blocking[0]["id"] + ": " + blocking[0]["detail"])
        phase = "output-directory"
        target = Path(output_dir) if output_dir is not None else Path("out/verification") / uuid.uuid4().hex
        target = (root / target).resolve()
        target.mkdir(parents=True, exist_ok=False)
        report["output_dir"] = str(target)
        report["report_path"] = str(target / "report.json")

        def command(step):
            step["stdout_path"] = str(target / (current["id"] + "-" + step["phase"] + ".stdout.log"))
            step["stderr_path"] = str(target / (current["id"] + "-" + step["phase"] + ".stderr.log"))
            step["result"] = "running"
            with Path(step["stdout_path"]).open("x", encoding="utf-8") as stdout, \
                    Path(step["stderr_path"]).open("x", encoding="utf-8") as stderr:
                step["returncode"] = run(list(step["argv"]), cwd=step["cwd"], stdout=stdout, stderr=stderr)
            if step["returncode"] != 0:
                raise VerificationError(f"command exited {step['returncode']}; see {step['stderr_path']}")
            step["result"] = "passed"

        def validate_cache(validation):
            validation["result"] = "running"
            checks = inspector(report, required_cache_ids=(current["id"],))
            validation["checks"] = [item for item in checks if current["id"] in item["configurations"]]
            blocking = [item for item in validation["checks"] if item["blocking"]]
            if blocking:
                raise VerificationError(blocking[0]["id"] + ": " + blocking[0]["detail"])
            cache = root / current["binary_dir"] / "CMakeCache.txt"
            values = cache_reader(cache.read_text(encoding="utf-8"))
            validation["result"] = "passed"
            return values

        for current in report["configurations"]:
            configure, build, test = current["steps"]
            phase, active = "configure", configure
            command(configure)
            phase, active = "cache", current["validations"]["cache"]
            validate_cache(active)
            phase, active = "build", build
            command(build)
            phase, active = "post-build-cache", current["validations"]["post-build-cache"]
            values = validate_cache(active)
            selected = values.get("LABRADOR_TEST_BROWSER", "")
            selected_browser = bool(selected and not selected.endswith("-NOTFOUND"))
            phase, active = "registration", current["validations"]["registration"]
            active.update({"phase": phase, "argv": test["argv"][:3] + ["--show-only=json-v1"],
                           "cwd": test["cwd"], "effects": ["registration-query", "build-tree-reads"]})
            command(active)
            inventory = registration_inventory(Path(active["stdout_path"]).read_text(encoding="utf-8"),
                                               current, selected_browser=selected_browser)
            active["inventory"] = inventory
            names = {item["name"] for item in inventory}
            phase, active = "test", test
            junit = target / (current["id"] + "-junit.xml")
            test["argv"] += PASSED_OUTPUT_LIMIT + ["--output-junit", str(junit)]
            command(test)
            phase, active = "test-results", current["validations"]["test-results"]
            active.update({"result": "running", "junit_path": str(junit)})
            junit_text = junit.read_text(encoding="utf-8")
            # The device is recorded before the results are judged, so a run
            # whose JUnit fails validation still says what drew.
            report["device_identity"][current["id"]] = device_identity(junit_cases(junit_text)[0])
            active["tests"] = junit_results(junit_text, names)
            active["result"] = "passed"
        phase, active, current = "source-revision", None, None
        after, _ = read_revision(root)
        if after != commit:
            raise VerificationError(f"HEAD moved from {commit} to {after} during the sweep")
        report.update({"result": "passed", "exit_code": 0})
    except (OSError, UnicodeError, ValueError, KeyboardInterrupt) as error:
        interrupted = isinstance(error, KeyboardInterrupt)
        status = "interrupted" if interrupted else "failed"
        if active is not None:
            active["result"] = status
        detail = ("Interrupted; descendant process state was not inspected." if interrupted else str(error))
        report.update({"result": status, "exit_code": 130 if interrupted else 1,
                       "failure": {"configuration": current["id"] if current else None,
                                   "phase": phase, "detail": detail}})
    if report["report_path"]:
        try:
            Path(report["report_path"]).write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        except OSError as error:
            report["intended_report_path"] = report["report_path"]
            report["report_path"] = None
            report["report_write_error"] = str(error)
            if report["failure"] is None:
                report.update({"result": "failed", "exit_code": 1,
                               "failure": {"configuration": None, "phase": "report", "detail": str(error)}})
    return report
