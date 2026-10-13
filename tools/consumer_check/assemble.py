"""Assemble the project Get Started's "Start your own project" page builds.

The page is website/src/content/docs/docs/get-started/your-project.mdx. This
does its steps 3 to 6 to an empty folder, taking everything from one engine
tree, which it copies to external/labrador where step 2 adds a submodule:
samples/minimal copied to game/, every include the page lists edited, the
files in website/src/walkthrough/ put where its README says, and the
vcpkg.json the page derives from the engine's own. It does not run git
init or git submodule add, and it builds nothing; README.md beside this says
what to run next.

The include edits and the manifest are derived here by the rules the page's
two components use (website/src/components/IncludeChanges.astro and
ConsumerManifest.astro). A change to either rule is made in both.
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

ENGINE = Path("external/labrador")
SAMPLE = Path("samples/minimal")
INCLUDE_FROM = '#include "samples/minimal/'
INCLUDE_TO = '#include "game/'

# Walkthrough file -> where the page puts it (website/src/walkthrough/README.md).
WALKTHROUGH = Path("website/src/walkthrough")
PLACED = {
    "root-CMakeLists.txt": "CMakeLists.txt",
    "game-CMakeLists.txt": "game/CMakeLists.txt",
    "CMakePresets.json": "CMakePresets.json",
    "gitignore.txt": ".gitignore",
}

# The page's <ConsumerManifest name="mygame" />, and the packages only the
# engine's own tests use, which a consumer does not build.
PROJECT_NAME = "mygame"
TEST_ONLY = {"doctest", "lodepng"}


def tree_files(engine: Path) -> list[Path]:
    """The files a commit of the engine tree would hold, as they are on disk.

    Tracked and untracked-but-not-ignored, so a local run sees uncommitted work
    and never copies out/, node_modules/ or a worktree folder.
    """
    listing = subprocess.run(
        ["git", "-C", str(engine), "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
        check=True,
        capture_output=True,
    ).stdout.decode("utf-8")
    return sorted({Path(name) for name in listing.split("\0") if name and (engine / name).is_file()})


def copy_engine(engine: Path, destination: Path) -> None:
    files = tree_files(engine)
    for relative in files:
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(engine / relative, target)
    print(f"engine: {len(files)} files from {engine} to {destination}")


def edit_includes(game: Path) -> None:
    changed = 0
    for source in sorted(game.rglob("*")):
        if source.suffix not in {".h", ".cpp"}:
            continue
        with source.open(encoding="utf-8", newline="") as stream:
            lines = stream.readlines()
        edited = [INCLUDE_TO + line[len(INCLUDE_FROM):] if line.startswith(INCLUDE_FROM) else line for line in lines]
        for before, after in zip(lines, edited):
            if before != after:
                changed += 1
                print(f"include: {source.relative_to(game.parent).as_posix()}: {after.strip()}")
        with source.open("w", encoding="utf-8", newline="") as stream:
            stream.writelines(edited)
    if changed == 0:
        raise SystemExit(f"No line in {SAMPLE.as_posix()} starts with {INCLUDE_FROM}; the Get Started page needs rewriting.")


def derived_manifest(engine_manifest: Path) -> dict:
    engine = json.loads(engine_manifest.read_text(encoding="utf-8"))
    return {
        "name": PROJECT_NAME,
        "version-string": "0.1.0",
        "dependencies": [
            dependency
            for dependency in engine["dependencies"]
            if (dependency if isinstance(dependency, str) else dependency["name"]) not in TEST_ONLY
        ],
        "builtin-baseline": engine["builtin-baseline"],
    }


def assemble(engine: Path, project: Path) -> None:
    if project.exists() and any(project.iterdir()):
        raise SystemExit(f"{project} is not empty; assemble into a new folder.")
    labrador = project / ENGINE
    copy_engine(engine, labrador)

    shutil.copytree(labrador / SAMPLE, project / "game")
    edit_includes(project / "game")

    for name, placed in PLACED.items():
        shutil.copyfile(labrador / WALKTHROUGH / name, project / placed)
        print(f"walkthrough: {name} -> {placed}")

    manifest = derived_manifest(labrador / "vcpkg.json")
    (project / "vcpkg.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"vcpkg.json: {', '.join(d if isinstance(d, str) else d['name'] for d in manifest['dependencies'])}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("project", type=Path, help="the folder to assemble into; new or empty, on a short path")
    parser.add_argument(
        "--engine",
        type=Path,
        default=Path(__file__).resolve().parents[2],
        help="the engine tree to consume (default: the checkout this script is in)",
    )
    arguments = parser.parse_args()
    assemble(arguments.engine.resolve(), arguments.project.resolve())
    return 0


if __name__ == "__main__":
    sys.exit(main())
