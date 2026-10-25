"""Check a release folder of the Get Started project against its file list.

The folder is what `cmake --install out/build/x64-release --prefix <folder>`
stages from the project assemble.py makes. The list is
website/src/walkthrough/release-files.txt, which the Ship a Windows release
guide shows a reader: every file the install puts there, and nothing else.
This fails, naming each one, if a listed file is missing or a file the list
does not name is present. Windows paths ignore case, so the comparison does.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

LIST = Path("website/src/walkthrough/release-files.txt")


def listed_files(listing: str) -> set[str]:
    """The paths a release-files list names: the first word of each line that
    is neither blank nor a comment, with forward slashes, in lower case."""
    names = set()
    for line in listing.splitlines():
        stripped = line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        names.add(stripped.split()[0].replace("\\", "/").lower())
    return names


def installed_files(folder: Path) -> set[str]:
    return {path.relative_to(folder).as_posix().lower() for path in folder.rglob("*") if path.is_file()}


def differences(listed: set[str], installed: set[str]) -> list[str]:
    return [f"missing: {name}" for name in sorted(listed - installed)] + [
        f"not in the list: {name}" for name in sorted(installed - listed)
    ]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("folder", type=Path, help="the folder cmake --install staged")
    parser.add_argument(
        "--engine",
        type=Path,
        default=Path(__file__).resolve().parents[2],
        help="the engine tree whose list to read (default: the checkout this script is in)",
    )
    arguments = parser.parse_args()
    folder = arguments.folder.resolve()
    if not folder.is_dir():
        print(f"{folder} is not a folder.", file=sys.stderr)
        return 1
    listed = listed_files((arguments.engine / LIST).read_text(encoding="utf-8"))
    problems = differences(listed, installed_files(folder))
    for problem in problems:
        print(problem, file=sys.stderr)
    if problems:
        print(f"{folder} does not match {LIST.as_posix()}.", file=sys.stderr)
        return 1
    print(f"{folder}: the {len(listed)} files {LIST.as_posix()} lists, and no others.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
