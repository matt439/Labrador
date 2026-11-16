"""Compile the rules, screens and collision guides in a copy of an extracted starter.

The native-null pilot applies each guide independently, reading its actual code
blocks and source excerpts. It preserves the input starter and leaves generated
sources, command logs and a timing receipt under the copy's out/guide-pilot/.
It does not qualify browser pages, visual presentation or intermediate edits.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import time
from typing import Callable

sys.path.insert(0, str(Path(__file__).resolve().parent))
import rename_check  # noqa: E402
from guide_examples import Guide, prepare_rule  # noqa: E402
import screen_guide  # noqa: E402
import collision_guide  # noqa: E402


PRESET = "x64-debug"
RECIPES: tuple[tuple[str, Callable], ...] = (
    ("your-rule", prepare_rule),
    ("your-screen", screen_guide.prepare),
    ("your-collision", collision_guide.prepare),
)


def project_sources(project: Path) -> dict[Path, bytes]:
    return {path.relative_to(project): path.read_bytes()
            for path in [project / "CMakeLists.txt", *(project / "game").rglob("*")]
            if path.is_file()}


def restore(project: Path, original: dict[Path, bytes]) -> None:
    """Restore only this disposable copy's known source files, retaining builds."""
    for relative in set(project_sources(project)) - set(original):
        target = project / relative
        if not target.resolve().is_relative_to(project.resolve()):
            raise ValueError(f"refusing source cleanup outside {project}: {target}")
        target.unlink()
    for relative, contents in original.items():
        (project / relative).write_bytes(contents)


def add_state_checks(project: Path, engine: Path, name: str) -> None:
    scenario = "SCREEN" if name == "your-screen" else "COLLISION"
    extra = "title_state.cpp" if scenario == "SCREEN" else "block.cpp"
    source = project / "game/tests/guide_state_tests.cpp"
    source.write_bytes((engine / "tools/consumer_check/guide_state_tests.cpp").read_bytes())
    cmake = f'''
# The compiled-guide pilot's tests; not part of the distributed starter.
find_package(doctest CONFIG REQUIRED)
add_executable(GuideStateTests
    game/tests/guide_state_tests.cpp
    game/states/confirm_state.cpp
    game/states/hello_state.cpp
    game/states/{extra}
)
target_link_libraries(GuideStateTests PRIVATE
    MyGameRules LabradorEngine doctest::doctest labrador_settings
)
target_compile_definitions(GuideStateTests PRIVATE
    GUIDE_{scenario}=1
    GAME_MANIFEST="${{CMAKE_CURRENT_SOURCE_DIR}}/game/content/manifest.json"
)
add_test(NAME GuideStateTests COMMAND GuideStateTests)
set_tests_properties(GuideStateTests PROPERTIES TIMEOUT 60)
'''
    root = project / "CMakeLists.txt"
    root.write_text(root.read_text(encoding="utf-8") + cmake, encoding="utf-8", newline="\n")


class Runner:
    def __init__(self, project: Path, configure: list[str], run=subprocess.run):
        self.project = project
        self.configure = configure
        self.run = run
        self.receipt: list[dict] = []
        self.directory = project / "out/guide-pilot"
        self.directory.mkdir(parents=True, exist_ok=True)

    def command(self, phase: str, argv: list[str], *, broken: bool = False) -> dict:
        started = time.perf_counter()
        result = self.run(argv, cwd=self.project, capture_output=True, text=True, errors="replace")
        elapsed = time.perf_counter() - started
        output = result.stdout + result.stderr
        log = self.directory / f"{phase}.log"
        log.write_text(output, encoding="utf-8")
        entry = {"phase": phase, "argv": argv, "seconds": round(elapsed, 3),
                 "exit_code": result.returncode, "log": log.name}
        self.receipt.append(entry)
        print(f"{phase}: exit {result.returncode}, {elapsed:.3f} s ({log})", flush=True)
        if broken:
            if result.returncode == 0 or "bounds.cpp" not in output or "SM84_BROKEN_GUIDE" not in output:
                raise ValueError("deliberately broken guide example did not fail compilation with its source diagnostic")
        elif result.returncode != 0:
            print(output, file=sys.stderr)
            raise ValueError(f"{phase} failed; see {log}")
        return entry

    def build_and_test(self, name: str, state_checks: bool) -> None:
        self.command(f"{name}-configure", ["cmake", "--preset", PRESET,
                     "-DLABRADOR_RENDER_BACKEND=null", "-DLABRADOR_AUDIO_BACKEND=null", *self.configure])
        self.command(f"{name}-build", ["cmake", "--build", "--preset", PRESET, "--parallel", "2"])
        # The engine's tests must remain absent from the consumer's inventory.
        listing = self.command(f"{name}-inventory", ["ctest", "--test-dir",
                               f"out/build/{PRESET}", "--show-only=json-v1"])
        inventory = json.loads((self.directory / listing["log"]).read_text(encoding="utf-8"))
        actual = sorted(test["name"] for test in inventory["tests"])
        expected = sorted(["MyGameTests", "MyGameSmokeTests"] + (["GuideStateTests"] if state_checks else []))
        if actual != expected:
            raise ValueError(f"{name}: consumer test inventory {actual}, expected {expected}")
        self.command(f"{name}-tests", ["ctest", "--preset", PRESET,
                     "--no-tests=error", "--output-on-failure"])

    def check_broken(self) -> None:
        source = self.project / "game/rules/bounds.cpp"
        original = source.read_bytes()
        try:
            if original.count(b"return RectangleF(") != 1:
                raise ValueError("the rule's constructor expression changed; update the negative proof with it")
            source.write_bytes(original.replace(b"return RectangleF(", b"return SM84_BROKEN_GUIDE(", 1))
            self.command("your-rule-broken-build", ["cmake", "--build", "--preset", PRESET,
                         "--target", "MyGameRules", "--parallel", "2"], broken=True)
        finally:
            source.write_bytes(original)
            self.command("your-rule-restored-build", ["cmake", "--build", "--preset", PRESET,
                         "--parallel", "2"])
        self.command("your-rule-restored-tests", ["ctest", "--preset", PRESET,
                     "--no-tests=error", "--output-on-failure"])


def pilot(project: Path, engine: Path, configure: list[str], *, check_broken: bool,
          run=subprocess.run) -> dict:
    original = project_sources(project)
    runner = Runner(project, configure, run)
    report = {"schema_version": 1, "platform": "native-null", "preset": PRESET,
              "scope": "numbered main steps; independent examples; no browser or visual acceptance",
              "examples": [], "commands": runner.receipt,
              "tool_sha256": {name: hashlib.sha256((engine / "tools/consumer_check" / name).read_bytes()).hexdigest()
                              for name in ("guide_check.py", "guide_examples.py", "screen_guide.py",
                                           "collision_guide.py", "guide_state_tests.cpp")}}
    try:
        runner.build_and_test("baseline", False)
        for name, prepare in RECIPES:
            restore(project, original)
            guide = Guide(name, engine)
            prepare(project, guide)
            changed = {path: contents for path, contents in project_sources(project).items()
                       if original.get(path) != contents}
            sources = {}
            for path, contents in changed.items():
                snapshot = runner.directory / name / path
                snapshot.parent.mkdir(parents=True, exist_ok=True)
                snapshot.write_bytes(contents)
                sources[path.as_posix()] = hashlib.sha256(contents).hexdigest()
            report["examples"].append({"guide": guide.path.as_posix(),
                "guide_sha256": hashlib.sha256((engine / guide.path).read_bytes()).hexdigest(),
                "recipe_selections": len(guide.selections), "source_sha256": sources})
            state_checks = name != "your-rule"
            if state_checks:
                add_state_checks(project, engine, name)
            runner.build_and_test(name, state_checks)
            if name == "your-rule" and check_broken:
                runner.check_broken()
        report["result"] = "passed"
        return report
    finally:
        restore(project, original)
        # Restoring source bytes does not pretend the build cache is the baseline.
        # Every subsequent use must configure/build, as the ordinary guide says.
        report.setdefault("result", "failed")
        (runner.directory / "BUILD.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("project", type=Path, help="new or empty disposable copy on a short path")
    parser.add_argument("--from", dest="starter", type=Path, required=True,
                        help="the extracted starter; read only and never modified")
    parser.add_argument("--engine", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--configure-arg", action="append", default=[],
                        help="additional CMake configure argument; use --configure-arg=-Dname=value")
    parser.add_argument("--check-broken", action="store_true",
                        help="also require the intentionally malformed rule source to fail compilation")
    options = parser.parse_args(arguments)
    if sys.platform != "win32":
        parser.error("the initial compiled-guide pilot runs native-null on Windows")
    project, engine = options.project.resolve(), options.engine.resolve()
    if project.exists() and any(project.iterdir()):
        parser.error(f"{project} must be new or empty")
    rename_check.copy_starter(options.starter.resolve(), project)
    report = pilot(project, engine, options.configure_arg, check_broken=options.check_broken)
    print(f"Three-guide pilot passed; {len(report['commands'])} commands recorded in "
          f"{project / 'out/guide-pilot/BUILD.json'}", flush=True)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError) as failure:
        print(f"compiled-guide pilot: {failure}", file=sys.stderr)
        sys.exit(1)
