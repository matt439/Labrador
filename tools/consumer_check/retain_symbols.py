"""Retain an explicitly selected EXE/PDB pair and the source tree's actual bytes privately.

This records hashes; the symbol probe separately verifies PDB/source matching.
It never installs, uploads or replaces a retained build. Working-tree bytes,
rather than git archive, preserve the line endings recorded in the PDB.
"""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import zipfile

EXCLUDED = {".git", ".vs", "out", "node_modules", "vcpkg_installed", "__pycache__"}


def digest(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def source_files(root: Path):
    def fail_scan(error: OSError) -> None:
        raise error

    for folder, directories, names in os.walk(root, followlinks=False, onerror=fail_scan):
        directories[:] = sorted(name for name in directories if name not in EXCLUDED)
        for name in directories + sorted(names):
            path = Path(folder) / name
            if name in EXCLUDED:
                continue
            attributes = getattr(path.lstat(), "st_file_attributes", 0)
            reparse_point = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0)
            if path.is_symlink() or attributes & reparse_point:
                raise ValueError(f"Source capture does not follow links: {path}")
        for name in sorted(names):
            if name not in EXCLUDED:
                path = Path(folder) / name
                if not path.is_file():
                    raise ValueError(f"Source capture needs regular files: {path}")
                yield path


def retain(exe: Path, pdb: Path, source: Path, destination: Path, receipt: Path | None = None) -> dict:
    exe, pdb, source, destination = (path.resolve() for path in (exe, pdb, source, destination))
    if not exe.is_file() or not pdb.is_file() or not source.is_dir():
        raise ValueError("EXE, PDB and source directory must exist.")
    if exe.suffix.lower() != ".exe" or pdb.name.lower() != exe.with_suffix(".pdb").name.lower():
        raise ValueError("Select the executable and its same-basename full PDB.")
    if destination == source:
        raise ValueError("The private destination cannot replace the source tree.")
    if source in destination.parents and destination.relative_to(source).parts[0] != "out":
        raise ValueError("A destination inside the source tree must be under its excluded out/ folder.")
    stage = destination.with_name(destination.name + ".partial")
    if destination.exists() or stage.exists():
        raise ValueError(f"Retained output already exists: {destination} or {stage}")
    if receipt is not None and not receipt.is_file():
        raise ValueError(f"No symbol-probe receipt at {receipt}")
    files = list(source_files(source))
    destination.parent.mkdir(parents=True, exist_ok=True)
    stage.mkdir()
    captured = {}
    with zipfile.ZipFile(stage / "source.zip", "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path in files:
            relative = path.relative_to(source).as_posix()
            value = hashlib.sha256()
            with path.open("rb") as stream, archive.open(relative, "w", force_zip64=True) as entry:
                for block in iter(lambda: stream.read(1024 * 1024), b""):
                    entry.write(block)
                    value.update(block)
            captured[relative] = value.hexdigest()
    pairs = {}
    for name, path in (("exe", exe), ("pdb", pdb)):
        copied = stage / path.name
        shutil.copyfile(path, copied)
        pairs[name] = {"file": path.name, "bytes": copied.stat().st_size, "sha256": digest(copied)}
    receipt_record = None
    if receipt is not None:
        copied_receipt = stage / "symbol-check.txt"
        shutil.copyfile(receipt, copied_receipt)
        receipt_record = {"file": copied_receipt.name, "sha256": digest(copied_receipt)}
    record = {"format": 1, "captured_utc": datetime.now(timezone.utc).isoformat(),
              "source_root": str(source), **pairs, "receipt": receipt_record,
              "source": {"archive": "source.zip", "sha256": digest(stage / "source.zip"),
                         "files": captured}}
    (stage / "identity.json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    # Both paths are siblings under the explicitly selected destination parent.
    # No old archive is deleted; an interrupted capture stays .partial.
    if stage.parent != destination.parent:
        raise ValueError("Private staging and destination must share a parent.")
    stage.rename(destination)
    return record


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--pdb", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--receipt", type=Path)
    arguments = parser.parse_args()
    try:
        record = retain(arguments.exe, arguments.pdb, arguments.source_root, arguments.out, arguments.receipt)
    except (OSError, ValueError, zipfile.BadZipFile) as error:
        parser.exit(1, f"retain_symbols: {error}\n")
    print(f"Retained {record['exe']['sha256']} and its selected PDB in {arguments.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
