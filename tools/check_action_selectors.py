"""Check reviewed Action commit selectors and explicit migration guards, without network access.

Reads literal uses/with fields in repository workflows, composite actions and
the two starter recipes. This is a bounded source check, not a YAML evaluator,
runner qualification, settings change or hosted Action execution.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import json
from pathlib import Path
import re
import sys


@dataclass(frozen=True)
class ActionUse:
    path: str
    line: int
    selector: str
    inputs: dict[str, str]
    duplicate_inputs: tuple[str, ...]


def scalar(text: str) -> str:
    value = text.split("#", 1)[0].strip()
    if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
        value = value[1:-1]
    return value


def action_uses(text: str, path: str) -> list[ActionUse]:
    lines = text.splitlines()
    uses = []
    block_indent = None
    for index, line in enumerate(lines):
        stripped = line.lstrip(" ")
        indent = len(line) - len(stripped)
        if block_indent is not None:
            if not stripped or indent > block_indent:
                continue
            block_indent = None
        if re.fullmatch(r"(?:- )?[\w-]+:\s*[|>][0-9+-]*\s*(?:#.*)?", stripped):
            block_indent = indent + (2 if stripped.startswith("- ") else 0)
            continue
        match = re.fullmatch(r"(?P<dash>- )?uses:\s*(?P<value>.+)", stripped)
        if match is None:
            continue
        step_indent = indent if match["dash"] else indent - 2
        property_indent = step_indent + 2
        inputs: dict[str, str] = {}
        duplicates = []
        in_with = False
        with_count = 0
        for following in lines[index + 1:]:
            content = following.lstrip(" ")
            depth = len(following) - len(content)
            if not content or content.startswith("#"):
                continue
            if depth <= step_indent:
                break
            if depth == property_indent:
                in_with = scalar(content) == "with:"
                if in_with:
                    with_count += 1
                    if with_count > 1:
                        duplicates.append("with mapping")
                continue
            if in_with and depth == property_indent + 2:
                field = re.fullmatch(r"([\w-]+):\s*(.*)", content)
                if field:
                    key = field[1]
                    if key in inputs:
                        duplicates.append(key)
                    inputs[key] = scalar(field[2])
        uses.append(ActionUse(path, index + 1, scalar(match["value"]), inputs, tuple(duplicates)))
    return uses


def source_paths(root: Path) -> list[Path]:
    files = set()
    for folder in (root / ".github/workflows", root / ".github/actions"):
        if folder.is_dir():
            files.update(folder.rglob("*.yml"))
            files.update(folder.rglob("*.yaml"))
    for name in ("ci.yml", "browser-ci.yml"):
        path = root / "website/src/walkthrough" / name
        if path.is_file():
            files.add(path)
    return sorted(files)


def checked_inventory(value: dict) -> dict:
    if not isinstance(value, dict) or value.get("schema_version") != 1 or not isinstance(value.get("actions"), dict) or not value["actions"]:
        raise ValueError("expected nonempty schema_version 1 action inventory")
    if not re.fullmatch(r"\d+\.\d+\.\d+", str(value.get("minimum_runner", ""))):
        raise ValueError("inventory must record the reviewed minimum runner version")
    for name, entry in value["actions"].items():
        if not re.fullmatch(r"[\w-]+/[\w-]+", name) or not isinstance(entry, dict):
            raise ValueError(f"invalid action inventory entry {name!r}")
        if not re.fullmatch(r"[0-9a-f]{40}", str(entry.get("commit", ""))):
            raise ValueError(f"{name}: expected a full immutable Action commit")
        if not re.fullmatch(r"v\d+\.\d+\.\d+", str(entry.get("version", ""))) or entry.get("runtime") != "node24" or entry.get("module_format") != "esm":
            raise ValueError(f"{name}: missing reviewed release/runtime facts")
        if entry.get("release_url") != f"https://github.com/{name}/releases/tag/{entry['version']}" \
                or entry.get("metadata_url") != f"https://raw.githubusercontent.com/{name}/{entry['commit']}/action.yml" \
                or not re.fullmatch(r"[0-9a-f]{64}", str(entry.get("metadata_sha256", ""))):
            raise ValueError(f"{name}: reviewed source identity URLs/digest disagree")
        if not isinstance(entry.get("guard_inputs"), dict) or not all(isinstance(key, str) and isinstance(item, str) for key, item in entry["guard_inputs"].items()):
            raise ValueError(f"{name}: invalid input guards")
        if not isinstance(entry.get("subpaths"), list) or not entry["subpaths"] or not all(isinstance(item, str) for item in entry["subpaths"]):
            raise ValueError(f"{name}: invalid reviewed sub-action paths")
        if any(item and not re.fullmatch(r"[\w-]+(?:/[\w-]+)*", item) for item in entry["subpaths"]):
            raise ValueError(f"{name}: invalid reviewed sub-action path spelling")
    return value["actions"]


def audit(root: Path, inventory: dict | None = None) -> tuple[list[str], list[ActionUse]]:
    root = root.resolve()
    if inventory is None:
        inventory = json.loads((root / ".github/action-versions.json").read_text(encoding="utf-8"))
    entries = checked_inventory(inventory)
    files = source_paths(root)
    if not files or not (root / ".github/workflows").is_dir():
        raise ValueError("no repository workflows found; pass the Labrador checkout root")
    uses = [use for path in files for use in action_uses(path.read_text(encoding="utf-8"), path.relative_to(root).as_posix())]
    findings = []
    selected = set()
    for use in uses:
        location = f"{use.path}:{use.line}"
        if use.selector.startswith("./"):
            local = (root / use.selector).resolve()
            metadata = (local / "action.yml").resolve()
            if not local.is_relative_to(root / ".github/actions") or not metadata.is_relative_to(root / ".github/actions") or not metadata.is_file():
                findings.append(f"{location}: local Action must resolve inside .github/actions with action.yml")
            continue
        match = re.fullmatch(r"([\w-]+/[\w-]+)(?:/([\w/-]+))?@([^\s]+)", use.selector)
        if match is None or match[1] not in entries:
            findings.append(f"{location}: unreviewed remote Action {use.selector!r}")
            continue
        name, subpath, ref = match[1], match[2] or "", match[3]
        entry = entries[name]
        selected.add(name)
        if subpath not in entry["subpaths"]:
            findings.append(f"{location}: unreviewed {name} sub-action {subpath!r}")
        if ref != entry["commit"]:
            findings.append(f"{location}: {name} must use reviewed {entry['version']} commit {entry['commit']}, found {ref}")
        if use.duplicate_inputs:
            findings.append(f"{location}: duplicate with inputs: {', '.join(use.duplicate_inputs)}")
        for key, expected in entry["guard_inputs"].items():
            if use.inputs.get(key) != expected:
                findings.append(f"{location}: {name} requires explicit {key}: {expected}, found {use.inputs.get(key)!r}")
    for name in entries.keys() - selected:
        findings.append(f"inventory: reviewed Action {name} has no source use")
    return findings, uses


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--repo", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args(argv)
    try:
        findings, uses = audit(args.repo)
    except (OSError, ValueError, TypeError) as error:
        print(f"check_action_selectors: {error}", file=sys.stderr)
        return 2
    if findings:
        print("\n".join(findings), file=sys.stderr)
        return 1
    remote = sum(not use.selector.startswith("./") for use in uses)
    print(f"Action selectors: {remote} remote uses match reviewed commits/guards; {len(uses) - remote} repository-owned uses stay local.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
