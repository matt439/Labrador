"""Write a Windows Sandbox configuration that runs a release folder's smoke test.

Windows Sandbox is a clean Windows with nothing installed - no Visual Studio
and no Visual C++ redistributable - which is what a player's machine may be.
This writes two files into a results folder: clean-machine.wsb, which maps the
release folder into the sandbox read-only and the results folder writable, and
run.cmd, which the sandbox runs at logon. run.cmd starts the game with
--smoke-test and records its exit code, what it wrote to stderr, whether the
redistributable is installed in the sandbox, the Windows version, and the
display adapters, then shuts the sandbox down. Open clean-machine.wsb to run
it; README.md beside this says what the results mean.

A configuration names absolute host paths, so it is written for one machine's
folders rather than checked in.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
from xml.sax.saxutils import escape

SANDBOX_DESKTOP = r"C:\Users\WDAGUtilityAccount\Desktop"
RELEASE_IN_SANDBOX = SANDBOX_DESKTOP + r"\release"
RESULTS_IN_SANDBOX = SANDBOX_DESKTOP + r"\results"

# The release folder is the first argument, so the same script can be tried on
# the host against any folder; in the sandbox it is the mapped one.
RUN_CMD = r"""@echo off
rem Written by tools\consumer_check\sandbox.py. Windows Sandbox runs it at
rem logon; on the host, name a release folder as its argument.
setlocal
set "release=%~1"
if "%release%"=="" set "release={release}"
set "results=%~dp0"
set "program={program}"

del /q "%results%exit-code.txt" "%results%stderr.txt" "%results%machine.txt" 2>nul

"%release%\%program%" --smoke-test 2> "%results%stderr.txt"
> "%results%exit-code.txt" echo %errorlevel%

(
    ver
    echo.
    for %%f in (vcruntime140.dll vcruntime140_1.dll msvcp140.dll) do (
        if exist "%SystemRoot%\System32\%%f" (echo %%f is installed in System32) else (echo %%f is not installed in System32)
    )
    echo.
    powershell -NoProfile -Command "Get-CimInstance Win32_VideoController | Format-List Name, DriverVersion, AdapterCompatibility"
) > "%results%machine.txt" 2>&1

if "%~1"=="" shutdown /s /t 0
"""

WSB = """<Configuration>
  <VGpu>{vgpu}</VGpu>
  <Networking>Disable</Networking>
  <MappedFolders>
    <MappedFolder>
      <HostFolder>{release}</HostFolder>
      <SandboxFolder>{release_in_sandbox}</SandboxFolder>
      <ReadOnly>true</ReadOnly>
    </MappedFolder>
    <MappedFolder>
      <HostFolder>{results}</HostFolder>
      <SandboxFolder>{results_in_sandbox}</SandboxFolder>
      <ReadOnly>false</ReadOnly>
    </MappedFolder>
  </MappedFolders>
  <LogonCommand>
    <Command>cmd /c {results_in_sandbox}\\run.cmd</Command>
  </LogonCommand>
</Configuration>
"""


def write(release: Path, results: Path, program: str, vgpu: str) -> None:
    if not (release / program).is_file():
        raise SystemExit(f"{release} has no {program}; stage the release with cmake --install first.")
    results.mkdir(parents=True, exist_ok=True)
    (results / "run.cmd").write_text(
        RUN_CMD.format(release=RELEASE_IN_SANDBOX, program=program).replace("\n", "\r\n"),
        encoding="ascii",
        newline="",
    )
    (results / "clean-machine.wsb").write_text(
        WSB.format(
            vgpu=vgpu,
            release=escape(str(release)),
            results=escape(str(results)),
            release_in_sandbox=RELEASE_IN_SANDBOX,
            results_in_sandbox=RESULTS_IN_SANDBOX,
        ),
        encoding="utf-8",
    )
    print(f"wrote {results / 'clean-machine.wsb'} and {results / 'run.cmd'}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("release", type=Path, help="the folder cmake --install staged")
    parser.add_argument("results", type=Path, help="a folder for the configuration and what the run records")
    parser.add_argument("--program", default="MyGame.exe", help="the game's executable (default: MyGame.exe)")
    parser.add_argument(
        "--vgpu",
        choices=["Default", "Enable", "Disable"],
        default="Default",
        help="the sandbox's virtual GPU; Disable leaves only the software adapter (default: Default)",
    )
    arguments = parser.parse_args()
    write(arguments.release.resolve(), arguments.results.resolve(), arguments.program, arguments.vgpu)
    return 0


if __name__ == "__main__":
    sys.exit(main())
