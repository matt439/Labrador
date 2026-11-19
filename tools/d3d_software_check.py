"""Check explicit Direct3D software evidence and the default-policy fixtures."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


STARTUP_PREFIX = "D3D STARTUP:"
DEFAULT_REQUIRED_CASES = {
    "D3D STARTUP: default Release refuses known software without device creation",
    "D3D STARTUP: selection skips known software and reaches the next hardware control",
}
DEVICE_FIELDS = r" ([^\r\n]+?) \((hardware|software|null)\)"
PIXEL_DEVICE = re.compile("Drawing on" + DEVICE_FIELDS)
INSTALLED_DEVICE = re.compile(r"Direct3D (?:11|12) software-test device:" + DEVICE_FIELDS)
IMPLEMENTATION = re.compile(r"/engine/render/d3d(?:11|12)/(?:renderer|device_resources)\.cpp$")
WRAPPER = re.compile(r'^\s*#\s*include\s+"engine/render/d3d(?:11|12)/(?:renderer|device_resources)\.cpp"', re.MULTILINE)
DEFINITION = re.compile(r'(?:^|\s)[-/]D(?:\s|")*LABRADOR_D3D_SOFTWARE_TEST(?:=([^\s"]+))?')


def startup_cases(path: Path) -> list[str]:
    root = ET.parse(path).getroot()
    cases = root.findall(".//testcase")
    names = [case.get("name", "") for case in cases]
    if not cases or any(not name.startswith(STARTUP_PREFIX) for name in names):
        raise ValueError("The default-policy report must contain nonempty D3D STARTUP cases only.")
    if not DEFAULT_REQUIRED_CASES <= set(names):
        raise ValueError("The report lacks the default Release refusal/selection controls; an opted-in or Debug binary is not default Release evidence.")
    if any(int(suite.get(kind, "0")) != 0 for suite in root.iter("testsuite")
           for kind in ("failures", "errors", "skipped")):
        raise ValueError("The default-policy report has a failed/skipped aggregate.")
    if any(case.find(kind) is not None for case in cases for kind in ("failure", "error", "skipped")):
        raise ValueError("A selected default-policy fixture failed or was skipped.")
    return names


def software_devices(output: str, *, installed: bool) -> list[str]:
    devices = (INSTALLED_DEVICE if installed else PIXEL_DEVICE).findall(output)
    if not devices:
        raise ValueError("No selected render device was reported.")
    if any(kind != "software" or "microsoft basic render" not in name.casefold()
           for name, kind in devices):
        raise ValueError("Every observed device must be Microsoft's Basic Render Driver classified software.")
    return [name for name, _ in devices]


def compile_policy(path: Path, *, enabled: bool) -> list[dict]:
    """Check every backend implementation copy, including excluded probes."""
    commands = json.loads(path.read_text(encoding="utf-8"))
    records = []
    found_device = False
    for entry in commands:
        source = Path(entry["file"])
        if not source.is_absolute():
            source = Path(entry["directory"]) / source
        if source.suffix.lower() not in (".cpp", ".cc", ".cxx"):
            continue
        normal = source.as_posix()
        implementation = IMPLEMENTATION.search(normal) is not None
        wrapper = source.is_file() and WRAPPER.search(source.read_text(encoding="utf-8")) is not None
        if not implementation and not wrapper:
            continue
        command = entry.get("command", " ".join(entry.get("arguments", [])))
        definitions = DEFINITION.findall(command)
        correct = definitions == ["1"] if enabled else not definitions
        if not correct:
            raise ValueError(f"Software-test definition disagrees with the requested profile: {source}: {command}")
        found_device |= implementation and normal.endswith("/device_resources.cpp")
        records.append({"file": str(source), "command": command})
    if not found_device:
        raise ValueError("The compile database contains no Direct3D device implementation.")
    return records


def check(mode: str, executable: Path, report: Path, timeout: float = 120,
          compile_commands: Path | None = None) -> dict:
    report = report.resolve()
    if report == executable.resolve():
        raise ValueError("The report must not overwrite the executable.")
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps({"mode": mode, "result": "not-completed"}) + "\n", encoding="utf-8")
    executable = executable.resolve(strict=True)
    wiring = compile_policy(compile_commands, enabled=mode != "default-refusal") if compile_commands else None
    with tempfile.TemporaryDirectory(prefix="labrador-d3d-evidence-") as temporary:
        xml_path = Path(temporary) / "startup.xml"
        arguments = (["--test-case=" + STARTUP_PREFIX + "*", "--no-skip=true",
                      "--reporters=junit", "--out=" + str(xml_path)]
                     if mode == "default-refusal" else
                     ["--test-case=the renderer reports the rasterising device it selected"]
                     if mode == "pixels" else ["--smoke-test"])
        command = [str(executable), *arguments]
        working_directory = Path.cwd() if mode == "installed" else executable.parent
        result = subprocess.run(command, cwd=working_directory, capture_output=True,
                                text=True, timeout=timeout)
        output = result.stdout + result.stderr
        print(output, end="")
        receipt = {"mode": mode, "command": command, "cwd": str(working_directory),
                   "exit": result.returncode,
                   "executable_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
                   "stdout": result.stdout, "stderr": result.stderr, "result": "failed"}
        if wiring is not None:
            receipt["implementation_copies"] = wiring
        if mode == "default-refusal" and xml_path.is_file():
            retained_xml = report.with_suffix(".xml")
            retained_xml.write_bytes(xml_path.read_bytes())
            receipt["junit"] = str(retained_xml)
        try:
            if result.returncode != 0:
                raise ValueError(f"The selected executable exited {result.returncode}.")
            if mode == "default-refusal":
                receipt["cases"] = startup_cases(xml_path)
            else:
                receipt["devices"] = software_devices(output, installed=mode == "installed")
            receipt["result"] = "passed"
        finally:
            report.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
        summary = {key: value for key, value in receipt.items()
                   if key not in ("stdout", "stderr", "implementation_copies")}
        if wiring is not None:
            summary["implementation_copy_count"] = len(wiring)
        print(json.dumps(summary, indent=2))
        return receipt


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=("default-refusal", "pixels", "installed"), required=True)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--compile-commands", type=Path,
                        help="verify the profile on backend sources and instrumented copies too")
    arguments = parser.parse_args()
    try:
        check(arguments.mode, arguments.executable, arguments.report,
              compile_commands=arguments.compile_commands)
    except (OSError, ValueError, ET.ParseError, subprocess.TimeoutExpired) as error:
        print(f"d3d_software_check: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
