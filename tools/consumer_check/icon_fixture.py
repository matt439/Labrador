"""Copy test-only icon input into an assembled starter for its optional recipe.

This changes no engine or game code and builds nothing. Configure the game
with GAME_ICON_FILE naming the returned path, then build/install/check/smoke.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import sys


ICON = Path("tests/app/window_icon_fixture.ico")
PLACED = Path("game/icon fixture/consumer.test.ico")


def prepare(project: Path, engine: Path) -> Path:
    project = project.resolve()
    for relative in ("CMakeLists.txt", "game/CMakeLists.txt", "game/main.cpp", "game/game_icon.rc"):
        if not (project / relative).is_file():
            raise ValueError(f"{project / relative} is missing; assemble the current starter first.")
    cmake = (project / "game/CMakeLists.txt").read_text(encoding="utf-8")
    main = (project / "game/main.cpp").read_text(encoding="utf-8")
    if ("GAME_ICON_FILE" not in cmake or "GAME_HAS_ICON_RESOURCE" not in main
            or 'options.window_icon_resource = L"GAME_ICON";' not in main):
        raise ValueError("The assembled game lacks the optional icon recipe; use the current starter.")
    output = project / PLACED
    if not output.resolve().is_relative_to(project):
        raise ValueError("The icon fixture destination must stay inside the assembled project.")
    if output.exists():
        raise ValueError(f"{output} already exists; use a fresh fixture project.")
    contents = (engine / ICON).read_bytes()
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(contents)
    return output


def check_configured(project: Path, build: Path) -> None:
    """Require the opt-in macro on the actual game's compiled native entry."""
    database = build / "compile_commands.json"
    entries = json.loads(database.read_text(encoding="utf-8"))
    expected = (project / "game/main.cpp").resolve()
    for entry in entries:
        source = Path(entry["file"])
        if not source.is_absolute():
            source = Path(entry["directory"]) / source
        if source.resolve() != expected:
            continue
        command = entry.get("command", " ".join(entry.get("arguments", [])))
        if not re.search(r'(?:^|\s)"?[/-]DGAME_HAS_ICON_RESOURCE(?:=[^"\s]+)?"?(?=\s|$)', command):
            raise ValueError(f"{database}: game/main.cpp is missing GAME_HAS_ICON_RESOURCE")
        print("The native game entry is compiled with its explicit icon request enabled.")
        return
    raise ValueError(f"{database}: no native game/main.cpp compile entry")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("project", type=Path)
    parser.add_argument("--engine", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--check-configured", type=Path, metavar="BUILD_DIR")
    arguments = parser.parse_args()
    if arguments.check_configured:
        build = arguments.check_configured
        if not build.is_absolute():
            build = arguments.project / build
        check_configured(arguments.project.resolve(), build.resolve())
        return 0
    path = prepare(arguments.project, arguments.engine.resolve())
    print(f"Test-only icon input: {path}")
    print(f"Configure with -DGAME_ICON_FILE={path.as_posix()}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(f"icon fixture: {error}", file=sys.stderr)
        sys.exit(2)
