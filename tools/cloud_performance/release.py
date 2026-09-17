"""Build and verify immutable benchmark-input archives."""

from __future__ import annotations

import hashlib
import json
import stat
import subprocess
import zipfile
from pathlib import Path
from typing import Any

from .common import SCHEMA_VERSION, canonical, digest, safe_relative

RELEVANT_ROOTS = {
    ".github", "bench", "cmake", "engine", "external", "samples", "tests", "tools",
}
RELEVANT_TOP_LEVEL = {
    "AGENTS.md", "CLAUDE.md", "CMakeLists.txt", "CMakePresets.json", "README.md", "vcpkg.json",
}
RELEVANT_DOCUMENTS = {
    "docs/design/ARCHITECTURE.md",
    "docs/design/CONVENTIONS.md",
    "docs/design/PHILOSOPHY.md",
    "docs/review/backend-equivalence-2/STATUS.md",
}
FIXED_ZIP_TIME = (1980, 1, 1, 0, 0, 0)
FORBIDDEN_SUFFIXES = {".key", ".pem", ".pfx"}
FORBIDDEN_NAMES = {".env", "credentials", "credentials.json"}


def _git(root: Path, *arguments: str) -> bytes:
    result = subprocess.run(
        ["git", "-C", str(root), *arguments],
        check=True,
        capture_output=True,
    )
    return result.stdout


def _relevant(relative: str) -> bool:
    path = safe_relative(relative, field="source path")
    return (path.as_posix() in RELEVANT_TOP_LEVEL
            or path.as_posix() in RELEVANT_DOCUMENTS
            or path.parts[0] in RELEVANT_ROOTS)


def _reject_sensitive(path: Path, relative: str) -> None:
    if path.suffix.lower() in FORBIDDEN_SUFFIXES or path.name.lower() in FORBIDDEN_NAMES:
        raise ValueError(f"credentials cannot enter a performance bundle: {relative}")


def source_files(root: str | Path) -> list[Path]:
    """Return tracked and untracked, non-ignored source inputs for this lane."""
    repository = Path(root).resolve()
    listed = _git(repository, "-c", "core.quotepath=false", "ls-files", "-z",
                  "--cached", "--others", "--exclude-standard", "--", ".")
    names = [name.decode("utf-8") for name in listed.split(b"\0") if name]
    files: list[Path] = []
    for name in names:
        normalized = name.replace("\\", "/")
        if not _relevant(normalized):
            continue
        path = repository / Path(*pure_path_parts(normalized))
        if path.is_symlink() or not path.is_file():
            raise ValueError(f"source input is not a regular file: {normalized}")
        _reject_sensitive(path, normalized)
        files.append(path)
    return sorted(files, key=lambda path: path.relative_to(repository).as_posix())


def pure_path_parts(value: str) -> tuple[str, ...]:
    """Avoid host interpretation after safe_relative has checked POSIX form."""
    return safe_relative(value).parts


def payload_files(root: str | Path) -> list[Path]:
    payload = Path(root).resolve()
    if not payload.is_dir():
        raise ValueError("payload must be an existing directory")
    files: list[Path] = []
    for path in payload.rglob("*"):
        if path.is_symlink():
            raise ValueError(f"payload contains a symbolic link: {path}")
        if path.is_file():
            relative = path.relative_to(payload).as_posix()
            safe_relative(relative, field="payload path")
            _reject_sensitive(path, relative)
            files.append(path)
    if not files:
        raise ValueError("payload directory is empty")
    return sorted(files, key=lambda path: path.relative_to(payload).as_posix())


def _metadata(path: Path) -> dict[str, Any]:
    return {"sha256": digest(path), "bytes": path.stat().st_size}


def _write_entry(archive: zipfile.ZipFile, name: str, data: bytes) -> None:
    safe_relative(name, field="archive member")
    info = zipfile.ZipInfo(name, FIXED_ZIP_TIME)
    info.compress_type = zipfile.ZIP_DEFLATED
    info.external_attr = (stat.S_IFREG | 0o644) << 16
    archive.writestr(info, data, compress_type=zipfile.ZIP_DEFLATED, compresslevel=9)


def bundle(payload: str | Path, output: str | Path, *, root: str | Path) -> dict[str, Any]:
    repository = Path(root).resolve()
    git_root = Path(_git(repository, "rev-parse", "--show-toplevel").decode("utf-8").strip()).resolve()
    if git_root != repository:
        raise ValueError("root must be the Git repository root")
    archive_path = Path(output).resolve()
    if archive_path.exists():
        raise FileExistsError(f"refusing to overwrite {archive_path}")
    archive_path.parent.mkdir(parents=True, exist_ok=True)
    payload_root = Path(payload).resolve()
    sources = source_files(repository)
    payloads = payload_files(payload_root)
    source_entries = {
        f"source/{path.relative_to(repository).as_posix()}": _metadata(path)
        for path in sources
    }
    payload_entries = {
        f"payload/{path.relative_to(payload_root).as_posix()}": _metadata(path)
        for path in payloads
    }
    status = _git(repository, "status", "--porcelain=v1", "-z", "--untracked-files=all")
    manifest = {
        "schema_version": SCHEMA_VERSION,
        "scope": "labrador-cloud-performance-payload",
        "git": {
            "commit": _git(repository, "rev-parse", "HEAD").decode("ascii").strip(),
            "dirty": bool(status),
            "status_sha256": hashlib.sha256(status).hexdigest(),
        },
        "files": source_entries | payload_entries,
    }
    with zipfile.ZipFile(archive_path, "x") as archive:
        _write_entry(archive, "release.json", canonical(manifest))
        for name, path in (
            [(name, repository / Path(*pure_path_parts(name.removeprefix("source/"))))
             for name in source_entries]
            + [(name, payload_root / Path(*pure_path_parts(name.removeprefix("payload/"))))
               for name in payload_entries]
        ):
            _write_entry(archive, name, path.read_bytes())
    result = {
        "archive": str(archive_path),
        "archive_sha256": digest(archive_path),
        "release_sha256": hashlib.sha256(canonical(manifest)).hexdigest(),
        "source_files": len(source_entries),
        "payload_files": len(payload_entries),
        "git": manifest["git"],
    }
    return result


def _member_is_regular(member: zipfile.ZipInfo) -> bool:
    mode = member.external_attr >> 16
    kind = stat.S_IFMT(mode)
    return kind in (0, stat.S_IFREG) and not member.is_dir()


def verify(path: str | Path, expected_archive_sha256: str | None = None) -> dict[str, Any]:
    archive_path = Path(path)
    if expected_archive_sha256 is not None and digest(archive_path) != expected_archive_sha256.lower():
        raise ValueError("bundle archive hash differs from declaration")
    with zipfile.ZipFile(archive_path, "r") as archive:
        members = archive.infolist()
        names: set[str] = set()
        for member in members:
            safe_relative(member.filename, field="archive member")
            if member.filename in names:
                raise ValueError(f"bundle contains duplicate member: {member.filename}")
            names.add(member.filename)
            if not _member_is_regular(member):
                raise ValueError(f"bundle member is not a regular file: {member.filename}")
        if "release.json" not in names:
            raise ValueError("bundle has no release.json")
        raw_manifest = archive.read("release.json")
        manifest = json.loads(raw_manifest)
        if raw_manifest != canonical(manifest):
            raise ValueError("release.json is not canonical JSON")
        if (manifest.get("schema_version") != SCHEMA_VERSION
                or manifest.get("scope") != "labrador-cloud-performance-payload"
                or not isinstance(manifest.get("files"), dict)):
            raise ValueError("bundle release manifest has the wrong schema or scope")
        expected_names = {"release.json", *manifest["files"]}
        if names != expected_names:
            raise ValueError("bundle members differ from release manifest")
        for name, metadata in manifest["files"].items():
            safe_relative(name, field="release member")
            content = archive.read(name)
            if (not isinstance(metadata, dict)
                    or metadata.get("bytes") != len(content)
                    or metadata.get("sha256") != hashlib.sha256(content).hexdigest()):
                raise ValueError(f"bundle member drift: {name}")
    return manifest


def verify_benchmark_payload(manifest: dict[str, Any], backends: list[dict[str, Any]]) -> None:
    """Require every declared executable and its two runtime assets."""
    files = manifest.get("files")
    if not isinstance(files, dict):
        raise TypeError("release manifest has no file table")
    required: set[str] = set()
    for backend in backends:
        executable = safe_relative(backend["executable"], field="backend executable")
        directory = executable.parent
        required.update({
            executable.as_posix(),
            (directory / "fonts" / "courier_new_bold_16.spritefont").as_posix(),
            (directory / "textures" / "white.dds").as_posix(),
        })
    missing = sorted(required - set(files))
    if missing:
        raise ValueError(f"release payload is missing declared runtime input: {missing[0]}")


def verify_workspace_snapshot(manifest: dict[str, Any], root: str | Path) -> None:
    """Refuse launch after any source byte or relevant path changed."""
    repository = Path(root).resolve()
    files = manifest.get("files")
    if not isinstance(files, dict):
        raise TypeError("release manifest has no file table")
    expected = {
        name: metadata for name, metadata in files.items() if name.startswith("source/")
    }
    current = {
        f"source/{path.relative_to(repository).as_posix()}": _metadata(path)
        for path in source_files(repository)
    }
    if current.keys() != expected.keys():
        raise ValueError("workspace source paths differ from the reviewed release snapshot")
    for name, metadata in current.items():
        if metadata != expected[name]:
            raise ValueError(f"workspace source drift after bundling: {name}")
