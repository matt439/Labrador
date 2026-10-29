"""Assemble the project Get Started's "Start your own project" page builds.

The page is website/src/content/docs/docs/get-started/your-project.mdx. This
does its steps 3 to 6 to an empty folder, taking everything from one engine
tree, which it copies to external/labrador where step 2 adds a submodule:
samples/minimal copied to game/, every include the page lists edited, the
files in website/src/walkthrough/ put where its README says, and the
vcpkg.json the page derives from the engine's own. It does not run git
init or git submodule add, and it builds nothing; README.md beside this says
what to run next. starter_zip.py beside this writes the same project, engine
included, to the starter ZIP a release carries.

The include edits and the manifest are derived here by the rules the page's
two components use (website/src/components/IncludeChanges.astro and
ConsumerManifest.astro). A change to either rule is made in both.
"""

from __future__ import annotations

import argparse
import io
import json
import shutil
import subprocess
import sys
from pathlib import Path, PurePosixPath

ENGINE = Path("external/labrador")
SAMPLE = PurePosixPath("samples/minimal")
INCLUDE_FROM = '#include "samples/minimal/'
INCLUDE_TO = '#include "game/'

# Walkthrough file -> where the page puts it (website/src/walkthrough/README.md).
WALKTHROUGH = PurePosixPath("website/src/walkthrough")
PLACED = {
    "root-CMakeLists.txt": "CMakeLists.txt",
    "game-CMakeLists.txt": "game/CMakeLists.txt",
    "CMakePresets.json": "CMakePresets.json",
    "gitignore.txt": ".gitignore",
    "ci.yml": ".github/workflows/ci.yml",
}

# The page's <ConsumerManifest name="mygame" />, and the package only the
# engine's own tests use, which a consumer does not build. doctest stays: the
# game's own tests use it.
PROJECT_NAME = "mygame"
TEST_ONLY = {"lodepng"}


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


def edited_includes(source: bytes) -> tuple[bytes, list[str]]:
    """The source with each line starting INCLUDE_FROM rewritten, and those lines as edited.

    Line endings are kept as they are, whichever they are.
    """
    lines = io.StringIO(source.decode("utf-8"), newline="").readlines()
    edited = [INCLUDE_TO + line[len(INCLUDE_FROM):] if line.startswith(INCLUDE_FROM) else line for line in lines]
    changes = [after.strip() for before, after in zip(lines, edited) if before != after]
    return "".join(edited).encode("utf-8"), changes


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


def project_files(engine: Path, files: list[Path] | None = None) -> dict[str, bytes]:
    """Every file of the page's project except the engine, by its path in the project.

    The page's steps 3 to 6: the template under game/ with its includes edited,
    the walkthrough files where the page puts them, and the derived vcpkg.json.
    Each path is relative to the project's root and spelt with forward slashes.
    files is the engine tree's listing, tree_files(engine) when not given.
    """
    if files is None:
        files = tree_files(engine)
    project: dict[str, bytes] = {}
    changed = 0
    for relative in files:
        posix = PurePosixPath(relative.as_posix())
        if SAMPLE not in posix.parents:
            continue
        placed = (PurePosixPath("game") / posix.relative_to(SAMPLE)).as_posix()
        content = (engine / relative).read_bytes()
        if posix.suffix in {".h", ".cpp"}:
            content, changes = edited_includes(content)
            changed += len(changes)
            for change in changes:
                print(f"include: {placed}: {change}")
        project[placed] = content
    if changed == 0:
        raise SystemExit(f"No line in {SAMPLE.as_posix()} starts with {INCLUDE_FROM}; the Get Started page needs rewriting.")

    for name, placed in PLACED.items():
        project[placed] = (engine / WALKTHROUGH / name).read_bytes()
        print(f"walkthrough: {name} -> {placed}")

    manifest = derived_manifest(engine / "vcpkg.json")
    project["vcpkg.json"] = (json.dumps(manifest, indent=2) + "\n").encode("utf-8")
    print(f"vcpkg.json: {', '.join(d if isinstance(d, str) else d['name'] for d in manifest['dependencies'])}")
    return dict(sorted(project.items()))


def assemble(engine: Path, project: Path) -> None:
    if project.exists() and any(project.iterdir()):
        raise SystemExit(f"{project} is not empty; assemble into a new folder.")
    copy_engine(engine, project / ENGINE)
    for placed, content in project_files(engine).items():
        target = project / placed
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(content)


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
