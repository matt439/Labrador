"""Write the starter ZIP: Get Started's project, with the engine inside it.

The ZIP holds one folder, mygame/, which is the project assemble.py makes:
the template under game/ with its includes edited, the walkthrough's build
files, the derived vcpkg.json, and at external/labrador every file of the
engine tree that assemble.py copies. A reader who extracts it can configure,
build, test and play without running Git. It adds README.txt, naming the
Labrador it holds and the Get Started page.

Each release carries one as labrador-starter-vX.Y.Z.zip (CONTRIBUTING.md,
Cutting a release); --release names the tag and refuses a tree that is not
exactly that tag. CI's consumer jobs build the project from a ZIP this makes
from their own checkout. The same tree gives the same bytes: entries are
sorted, every entry has the time of the tree's last commit, and nothing of the
machine that made it is recorded.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import sys
import time
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import assemble  # noqa: E402

TOP = "mygame"
GET_STARTED = "https://labradorengine.com/docs/get-started/"
TAG = re.compile(r"v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)")
# The earliest time a ZIP entry can hold.
EARLIEST = (1980, 1, 1, 0, 0, 0)


def git(engine: Path, *arguments: str) -> str:
    return subprocess.run(
        ["git", "-C", str(engine), *arguments], check=True, capture_output=True, text=True
    ).stdout.strip()


def commit_time(engine: Path) -> tuple[int, int, int, int, int, int]:
    """The committer time of the tree's HEAD, in UTC, as a ZIP entry holds it."""
    seconds = int(git(engine, "log", "-1", "--format=%ct"))
    return max(EARLIEST, tuple(time.gmtime(seconds)[:6]))


def version_string(engine: Path) -> str:
    return json.loads((engine / "vcpkg.json").read_text(encoding="utf-8"))["version-string"]


def check_release(engine: Path, tag: str) -> None:
    """Refuse to name a release that the tree is not, exactly."""
    if not TAG.fullmatch(tag):
        raise SystemExit(f"{tag} is not a release tag such as v0.3.0.")
    if f"v{version_string(engine)}" != tag:
        raise SystemExit(f"vcpkg.json's version-string is {version_string(engine)}, not {tag[1:]}.")
    try:
        tagged = git(engine, "rev-parse", "--verify", "--quiet", f"refs/tags/{tag}^{{commit}}")
    except subprocess.CalledProcessError:
        raise SystemExit(f"There is no tag {tag} in {engine}; fetch it with git fetch origin --tags.")
    if tagged != git(engine, "rev-parse", "HEAD"):
        raise SystemExit(f"HEAD is not {tag}; check it out with git checkout {tag}.")
    if git(engine, "status", "--porcelain", "--untracked-files=all"):
        raise SystemExit(f"{engine} has changes or new files; a release's ZIP is made from a clean checkout.")


def readme(engine: Path, release: str | None) -> bytes:
    commit = git(engine, "rev-parse", "HEAD")
    if release:
        version = release
    else:
        version = f"a development build, not a release (its vcpkg.json says {version_string(engine)})"
        if git(engine, "status", "--porcelain"):
            commit += ", with uncommitted changes"
    lines = [
        "Labrador starter project",
        "",
        "A game project of your own, made from Labrador's minimal sample,",
        r"with the engine inside it in external\labrador.",
        "",
        f"Labrador version: {version}",
        f"Labrador commit: {commit}",
        "",
        "How to build and run it, step by step, starting from a Windows",
        "machine with nothing installed:",
        "",
        f"    {GET_STARTED}",
        "",
        "In short, in the x64 Native Tools Command Prompt, from this folder:",
        "",
        "    cmake --preset x64-debug",
        "    cmake --build --preset x64-debug",
        "    ctest --preset x64-debug",
        r"    out\build\x64-debug\game\MyGame.exe",
        "",
        r"Labrador's licence is external\labrador\LICENSE. The game in the",
        "game folder is yours to change.",
    ]
    return ("\r\n".join(lines) + "\r\n").encode("ascii")


def entry(name: str, when: tuple[int, int, int, int, int, int]) -> zipfile.ZipInfo:
    info = zipfile.ZipInfo(name, date_time=when)
    info.create_system = 3
    info.external_attr = 0o100644 << 16
    info.compress_type = zipfile.ZIP_DEFLATED
    return info


def starter_files(engine: Path, release: str | None) -> dict[str, bytes | Path]:
    """Every entry of the ZIP below its top folder: bytes, or the engine file to read."""
    files = assemble.tree_files(engine)
    entries: dict[str, bytes | Path] = dict(assemble.project_files(engine, files))
    for relative in files:
        entries[(assemble.ENGINE / relative).as_posix()] = engine / relative
    entries["README.txt"] = readme(engine, release)
    print(f"engine: {len(files)} files from {engine}")
    return entries


def write_zip(destination: Path, entries: dict[str, bytes | Path], when: tuple[int, int, int, int, int, int]) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    partial = destination.with_name(destination.name + ".partial")
    with zipfile.ZipFile(partial, "w") as archive:
        for name in sorted(entries):
            content = entries[name]
            data = content.read_bytes() if isinstance(content, Path) else content
            archive.writestr(entry(f"{TOP}/{name}", when), data, compresslevel=9)
    partial.replace(destination)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("directory", type=Path, help="where to write the ZIP; made if missing")
    parser.add_argument(
        "--release",
        metavar="vX.Y.Z",
        help="the release tag the tree is checked out at; names the ZIP labrador-starter-vX.Y.Z.zip",
    )
    parser.add_argument(
        "--engine",
        type=Path,
        default=Path(__file__).resolve().parents[2],
        help="the engine tree to put in it (default: the checkout this script is in)",
    )
    arguments = parser.parse_args()
    engine = arguments.engine.resolve()
    if arguments.release:
        check_release(engine, arguments.release)
    name = f"labrador-starter-{arguments.release}.zip" if arguments.release else "labrador-starter-dev.zip"
    destination = arguments.directory.resolve() / name
    write_zip(destination, starter_files(engine, arguments.release), commit_time(engine))
    data = destination.read_bytes()
    print(f"{destination}: {len(data)} bytes, sha256 {hashlib.sha256(data).hexdigest()}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
