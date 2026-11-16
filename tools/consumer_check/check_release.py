"""Check a release folder of the Get Started project against its file list.

The folder is what `cmake --install out/build/x64-release --prefix <folder>`
stages from the project assemble.py makes. The list is
website/src/walkthrough/release-files.txt, which the Ship a Windows release
guide shows a reader: every file the install puts there, and nothing else.
This fails, naming each one, if a listed file is missing or a file the list
does not name is present. Windows paths ignore case, so the comparison does.
It also reads the installed game's embedded manifest and fails unless it
declares PerMonitorV2 first (tools/check_dpi_manifest.py), the declaration
the project's dpi_aware.manifest makes.

Optional --icon-resource NAME also checks that exact named RT_GROUP_ICON,
its first-language group metadata and each referenced RT_ICON payload. It
does not decode images, run the program or prove Windows can load the icon.
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import check_dpi_manifest  # noqa: E402

LIST = Path("website/src/walkthrough/release-files.txt")
RT_ICON = 3
RT_GROUP_ICON = 14


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


def icon_image_count(data: bytes, name: str) -> int:
    """Check named icon-group metadata and payload references, without decoding images."""
    context = f"icon resource {name!r}"
    image = check_dpi_manifest.PortableExecutable(data)
    group = image.resource(RT_GROUP_ICON, name)
    if group is None:
        raise check_dpi_manifest.ManifestError(f"embeds no named RT_GROUP_ICON {name!r}")
    if len(group) < 6:
        raise check_dpi_manifest.ManifestError(f"{context} has a truncated group header")
    reserved, resource_type, count = struct.unpack_from("<HHH", group)
    if reserved != 0 or resource_type != 1:
        raise check_dpi_manifest.ManifestError(f"{context} has an invalid icon-group header")
    if count == 0:
        raise check_dpi_manifest.ManifestError(f"{context} contains no image entries")
    if len(group) < 6 + 14 * count:
        raise check_dpi_manifest.ManifestError(f"{context} has truncated image entries for count {count}")
    for index in range(count):
        # Width/height zero encodes 256. Plane/depth fields are descriptive;
        # checking the referenced image encoding belongs to Windows' loader.
        _, _, _, entry_reserved, _, _, size, image_id = struct.unpack_from("<BBBBHHIH", group, 6 + 14 * index)
        if entry_reserved != 0 or size == 0:
            raise check_dpi_manifest.ManifestError(f"{context} image entry {index} has invalid reserved/size fields")
        payload = image.resource(RT_ICON, image_id)
        if payload is None:
            raise check_dpi_manifest.ManifestError(f"{context} image entry {index} references missing RT_ICON {image_id}")
        if not payload:
            raise check_dpi_manifest.ManifestError(f"{context} image entry {index} references empty RT_ICON {image_id}")
        if len(payload) != size:
            raise check_dpi_manifest.ManifestError(
                f"{context} image entry {index} declares {size} bytes for RT_ICON {image_id}, found {len(payload)}")
    return count


def check_icon_paths(folder: Path, name: str, report=print, complain=None) -> int:
    """Check the GUI programs whose DPI declarations check_paths already verified."""
    complain = complain or (lambda line: print(line, file=sys.stderr))
    found, _, unreadable = check_dpi_manifest.programs(folder)
    failed = False
    for program, reason in unreadable:
        complain(f"{program}: cannot read icon metadata: {reason}")
        failed = True
    if not found:
        complain(f"{folder}: no Windows-subsystem program found for icon metadata")
        failed = True
    for program in found:
        try:
            count = icon_image_count(program.read_bytes(), name)
        except (check_dpi_manifest.ManifestError, OSError) as error:
            complain(f"{program}: {error}")
            failed = True
        else:
            label = "payload" if count == 1 else "payloads"
            report(f"{program}: icon resource {name!r}, {count} referenced image {label} (metadata only)")
    return 1 if failed else 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("folder", type=Path, help="the folder cmake --install staged")
    parser.add_argument("--icon-resource", metavar="NAME",
                        help="also verify this exact named icon group and its embedded payloads")
    parser.add_argument(
        "--engine",
        type=Path,
        default=Path(__file__).resolve().parents[2],
        help="the engine tree whose list to read (default: the checkout this script is in)",
    )
    arguments = parser.parse_args()
    if arguments.icon_resource is not None and (not arguments.icon_resource or "\0" in arguments.icon_resource):
        parser.error("--icon-resource needs a nonempty resource name without NUL")
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
    status = check_dpi_manifest.check_paths([folder])
    if status != 0 or arguments.icon_resource is None:
        return status
    return check_icon_paths(folder, arguments.icon_resource)


if __name__ == "__main__":
    sys.exit(main())
