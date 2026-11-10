"""Rename a starter project as the Rename your game guide says, and build it.

The guide is website/src/content/docs/docs/guides/your-name.mdx. Its step 2
is one PowerShell line that replaces MyGame in the project's build files.
This reads that line, the name in the guide's `set "NAME=..."` line and the
count the guide prints out of the guide itself, so the line a reader pastes
and the line checked here are the same text. It runs the line through cmd in
a starter project, with NAME set as the guide sets it, and fails unless it
prints the guide's count, prints 0 when run again, and leaves MyGame in none
of the files the guide says it is gone from. Then it does the guide's step 5
on the x64-debug preset: configure, build and test in a project with no out
folder, failing unless the tests registered are the renamed game's two, the
program has the new name and nothing in the build folder is still named
MyGame.

The project is a copy of a starter already on disk (--from, without its out
folder), or one assemble.py makes. The line is PowerShell run by cmd, so
this runs on Windows only. Steps 3 and 4 of the guide, the window's title,
the error box and the page's title, are edits by hand and are not made.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Callable, Sequence

sys.path.insert(0, str(Path(__file__).resolve().parent))
import assemble  # noqa: E402

GUIDE = Path("website/src/content/docs/docs/guides/your-name.mdx")
OLD_NAME = "MyGame"
PRESET = "x64-debug"
# The files step 2's line rewrites, and the ones the guide says never named
# the game. None may hold MyGame once the line has run.
RENAMED = ("CMakeLists.txt", "game/CMakeLists.txt")
NEVER_NAMED = ("CMakePresets.json", ".github/workflows/ci.yml")
MANIFEST = "vcpkg.json"

SET_LINE = re.compile(r'set "NAME=([A-Za-z][A-Za-z0-9]*)"')
COUNT_LINE = re.compile(r"Replaced MyGame ([0-9]+) times\.")
LISTED_TEST = re.compile(r"\s*Test\s+#[0-9]+: (\S+)\s*")

Refusal = assemble.Refusal


def fenced_lines(guide: str, language: str) -> list[str]:
    """Every line inside the guide's fenced blocks of one language, stripped."""
    lines: list[str] = []
    inside: str | None = None
    for line in guide.splitlines():
        stripped = line.strip()
        if stripped.startswith("```"):
            # An opening fence names its language first: ```text title="Output".
            inside = (stripped[3:].split() or [""])[0] if inside is None else None
            continue
        if inside == language:
            lines.append(stripped)
    return lines


def only(found: list, what: str):
    if len(found) != 1:
        raise Refusal(f"{GUIDE.as_posix()} has {len(found)} of {what}, not one; "
                      "tools/consumer_check/rename_check.py reads the guide and needs changing with it.")
    return found[0]


def guide_name(guide: str) -> str:
    """The name in the guide's `set "NAME=..."` line."""
    return only([match.group(1) for match in map(SET_LINE.fullmatch, fenced_lines(guide, "bat")) if match],
                'a `set "NAME=..."` line in a bat block')


def guide_command(guide: str) -> str:
    """Step 2's replacement line, as the guide prints it."""
    return only([line for line in fenced_lines(guide, "bat")
                 if line.startswith("powershell ") and f"Replaced {OLD_NAME}" in line],
                "a PowerShell line that reports its replacements in a bat block")


def guide_count(guide: str) -> int:
    """The number of replacements the guide says a starter project gives."""
    return int(only([match.group(1) for match in map(COUNT_LINE.fullmatch, fenced_lines(guide, "text")) if match],
                    f"a `Replaced {OLD_NAME} N times.` line in a text block"))


def report(count: int) -> str:
    return f"Replaced {OLD_NAME} {count} times."


def run_line(command: str, project: Path, name: str) -> tuple[int, str]:
    """Run the guide's line as a reader does: by cmd, in the project's folder, with NAME set."""
    result = subprocess.run(command, shell=True, cwd=project, env={**os.environ, "NAME": name},
                            capture_output=True, text=True, errors="replace")
    return result.returncode, (result.stdout + result.stderr).strip()


def source_problems(project: Path, name: str, count: int) -> list[str]:
    """What the line left wrong in the project's files; empty when the rename is whole."""
    problems: list[str] = []
    renamed = 0
    for relative in RENAMED + NEVER_NAMED:
        path = project / relative
        if not path.is_file():
            if relative in RENAMED:
                problems.append(f"{relative} is missing.")
            continue
        text = path.read_text(encoding="utf-8")
        for number, line in enumerate(text.splitlines(), start=1):
            if OLD_NAME in line:
                problems.append(f"{relative}:{number} still names {OLD_NAME}: {line.strip()}")
        if relative in RENAMED:
            renamed += text.count(name)
    if renamed != count:
        problems.append(f"{' and '.join(RENAMED)} name {name} {renamed} times, not the {count} the line replaced.")
    manifest = project / MANIFEST
    manifest_name = json.loads(manifest.read_text(encoding="utf-8")).get("name") if manifest.is_file() else None
    if manifest_name != name.lower():
        problems.append(f'{MANIFEST} has "name": {json.dumps(manifest_name)}, not "{name.lower()}".')
    return problems


def rename(project: Path, guide: str,
           run: Callable[[str, Path, str], tuple[int, str]] = run_line) -> tuple[str, list[str]]:
    """Run the guide's line twice in the project. The name it gave, and every problem found."""
    name, command, count = guide_name(guide), guide_command(guide), guide_count(guide)
    problems: list[str] = []
    for expected in (count, 0):
        code, output = run(command, project, name)
        print(output, flush=True)
        if code != 0 or output != report(expected):
            problems.append(f"The guide's line exited {code} and printed {output!r}, not {report(expected)!r}.")
            return name, problems
    return name, source_problems(project, name, count)


def listed_tests(listing: str) -> list[str]:
    """The test names `ctest -N` prints, in its order."""
    return [match.group(1) for match in map(LISTED_TEST.fullmatch, listing.splitlines()) if match]


def build_problems(build: Path, name: str, listing: str) -> list[str]:
    """What a build folder still has of the old name; empty when step 5's result is the guide's."""
    problems: list[str] = []
    expected = [f"{name}Tests", f"{name}SmokeTests"]
    tests = listed_tests(listing)
    if sorted(tests) != sorted(expected):
        problems.append(f"ctest lists {', '.join(tests) or 'no tests'}, not {' and '.join(expected)}.")
    if not (build / "game" / f"{name}.exe").is_file():
        problems.append(f"game/{name}.exe was not built.")
    for path in sorted(build.rglob(f"*{OLD_NAME}*")):
        # Windows matches names without regard to case, and the project's own
        # folder may be called mygame.
        if OLD_NAME in path.name:
            problems.append(f"{path.relative_to(build).as_posix()} is still named {OLD_NAME}.")
    cache = build / "CMakeCache.txt"
    project_line = f"CMAKE_PROJECT_NAME:STATIC={name}"
    if not cache.is_file() or project_line not in cache.read_text(encoding="utf-8", errors="replace").splitlines():
        problems.append(f"CMakeCache.txt has no line {project_line}.")
    return problems


def copy_starter(starter: Path, project: Path) -> None:
    """Copy a starter project without its out folder, as step 5's rmdir leaves it."""
    if not (starter / "game" / "main.cpp").is_file():
        raise Refusal(f"{starter} is not a starter project.")
    shutil.copytree(starter, project, dirs_exist_ok=True,
                    ignore=lambda folder, names: ["out"] if Path(folder) == starter else [])


def step(command: Sequence[str], project: Path) -> None:
    print(f"> {' '.join(command)}", flush=True)
    if subprocess.run(list(command), cwd=project).returncode != 0:
        raise SystemExit(f"{' '.join(command)} failed in {project}.")


def build(project: Path, name: str) -> list[str]:
    """Step 5 without the game's window: configure, build, list the tests, check, run them."""
    retry = Path(__file__).resolve().parents[1] / "ci_build_retry.py"
    step(["cmake", "--preset", PRESET], project)
    step([sys.executable, str(retry), "cmake", "--build", "--preset", PRESET], project)
    folder = project / "out" / "build" / PRESET
    listing = subprocess.run(["ctest", "-N", "--test-dir", str(folder)], cwd=project,
                             capture_output=True, text=True, errors="replace")
    print(listing.stdout, flush=True)
    if listing.returncode != 0:
        raise SystemExit(f"ctest -N failed in {folder}: {listing.stderr.strip()}")
    problems = build_problems(folder, name, listing.stdout)
    if not problems:
        step(["ctest", "--preset", PRESET], project)
    return problems


def main(arguments: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("project", type=Path, help="the folder to rename a starter in; new or empty, on a short path")
    parser.add_argument("--from", dest="starter", type=Path,
                        help="a starter project to copy, without its out folder (default: assemble one)")
    parser.add_argument("--no-build", action="store_true",
                        help="stop after the guide's line and the check of the files it changed")
    parser.add_argument(
        "--engine",
        type=Path,
        default=Path(__file__).resolve().parents[2],
        help="the engine tree whose guide to read and, without --from, to consume "
             "(default: the checkout this script is in)",
    )
    options = parser.parse_args(arguments)
    if sys.platform != "win32":
        raise Refusal("The guide's line is PowerShell run by cmd; this check runs on Windows only.")
    engine, project = options.engine.resolve(), options.project.resolve()
    guide = (engine / GUIDE).read_text(encoding="utf-8")
    if project.exists() and any(project.iterdir()):
        raise Refusal(f"{project} is not empty; rename in a new folder.")
    if options.starter is not None:
        copy_starter(options.starter.resolve(), project)
    else:
        assemble.assemble(engine, project)

    name, problems = rename(project, guide)
    if not problems and not options.no_build:
        problems = build(project, name)
    for problem in problems:
        print(problem, file=sys.stderr)
    if problems:
        print(f"{project} is not the project {GUIDE.as_posix()} says the rename gives.", file=sys.stderr)
        return 1
    built = "" if options.no_build else f", and {PRESET} built and passed {name}Tests and {name}SmokeTests"
    print(f"{project}: renamed to {name} by the guide's own line{built}.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Refusal as refusal:
        print(refusal, file=sys.stderr)
        sys.exit(refusal.code)
