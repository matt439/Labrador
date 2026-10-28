"""Compare the host's committed LineSweeper BUILD.json with local Labrador master.

Reads local Git objects only. It never fetches, reads a live URL, inspects game
JavaScript, copies a bundle or deploys. Positive lag is informational (exit 0);
invalid metadata or incomplete/unrelated history fails (exit 2).
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys

REPOSITORY = Path(__file__).resolve().parents[1]
HOST_BUILD = "public/demo/linesweeper/BUILD.json"
MASTER = "refs/heads/master"


class CheckError(RuntimeError):
    """The supplied local sources cannot establish a clean revision comparison."""


def git(repository: Path, *arguments: str, accepted: tuple[int, ...] = (0,)) -> subprocess.CompletedProcess:
    # Ambient Git routing/configuration must not substitute another checkout,
    # object store, namespace or replacement history for the explicit roots.
    environment = {name: value for name, value in os.environ.items()
                   if not name.upper().startswith("GIT_")}
    environment.update({"GIT_CONFIG_NOSYSTEM": "1", "GIT_CONFIG_GLOBAL": os.devnull,
                        "GIT_GRAFT_FILE": os.devnull, "GIT_TERMINAL_PROMPT": "0"})
    try:
        result = subprocess.run(
            ["git", "--no-pager", "--no-optional-locks", "--no-lazy-fetch", "--no-replace-objects",
             "-C", str(repository), *arguments],
            capture_output=True, env=environment, shell=False, timeout=30, check=False)
    except (OSError, subprocess.TimeoutExpired) as error:
        raise CheckError(f"cannot read local Git objects: {error}") from error
    if result.returncode not in accepted:
        detail = result.stderr.decode("utf-8", errors="replace").strip()
        raise CheckError(f"git {arguments[0]} failed: {detail or 'no diagnostic'}")
    return result


def checkout_root(path: Path, label: str) -> Path:
    root = path.resolve()
    marker = root / ".git"
    if not root.is_dir() or not (marker.is_file() or marker.is_dir()):
        raise CheckError(f"{label} must be a checkout root with its own .git marker: {root}")
    actual = git(root, "rev-parse", "--show-toplevel").stdout.decode("utf-8").strip()
    if not actual or Path(actual).resolve() != root:
        raise CheckError(f"{label} Git top-level does not match the supplied checkout root: {root}")
    return root


def resolve_commit(repository: Path, revision: str) -> str:
    if not revision or revision.startswith("-"):
        raise CheckError("a revision must be nonempty and cannot start with '-'")
    value = git(repository, "rev-parse", "--verify", "--end-of-options",
                revision + "^{commit}").stdout.decode("ascii").strip()
    if not re.fullmatch(r"[0-9a-f]{40}", value):
        raise CheckError(f"Git did not return a full 40-hex commit for {revision!r}")
    return value


def unique_object(pairs: list[tuple[str, object]]) -> dict:
    result = {}
    for name, value in pairs:
        if name in result:
            raise CheckError(f"BUILD.json contains a duplicate key: {name}")
        result[name] = value
    return result


def invalid_constant(value: str) -> object:
    raise CheckError(f"BUILD.json contains a non-JSON constant: {value}")


def build_commit(contents: bytes) -> str:
    try:
        build = json.loads(contents.decode("utf-8"), object_pairs_hook=unique_object,
                           parse_constant=invalid_constant)
    except (ValueError, RecursionError) as error:
        raise CheckError(f"committed BUILD.json is invalid UTF-8 JSON: {error}") from error
    if not isinstance(build, dict):
        raise CheckError("committed BUILD.json must contain an object")
    commit = build.get("commit")
    if not isinstance(commit, str) or not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise CheckError("BUILD.json commit must be a full lowercase 40-hex Git commit, not unknown")
    if build.get("dirty") is not False:
        raise CheckError("BUILD.json dirty must be exactly false; its commit cannot identify uncommitted source changes")
    return commit


def commit_count(repository: Path, revision: str) -> int:
    count = git(repository, "rev-list", "--count", revision, "--").stdout.decode("ascii").strip()
    if not re.fullmatch(r"0|[1-9][0-9]*", count):
        raise CheckError("Git did not return a valid commit count")
    return int(count)


def compare(host_repository: Path, *, engine_repository: Path = REPOSITORY,
            engine_ref: str = MASTER) -> dict:
    if not engine_ref or engine_ref.startswith("-"):
        raise CheckError("--engine-ref must be nonempty and cannot start with '-'")
    engine = checkout_root(engine_repository, "Labrador")
    host = checkout_root(host_repository, "host repository")
    host_head = resolve_commit(host, "HEAD")
    engine_head = resolve_commit(engine, engine_ref)
    # Both tips are pinned before reading the blob or traversing the graph.
    commit = build_commit(git(host, "cat-file", "blob", f"{host_head}:{HOST_BUILD}").stdout)
    if git(engine, "rev-parse", "--is-shallow-repository").stdout.strip() != b"false":
        raise CheckError("Labrador history is shallow or unknown; complete local history is required (no fetch attempted)")
    if resolve_commit(engine, commit) != commit:
        raise CheckError("the recorded bundle identity did not resolve to that exact local commit")
    # Validate every parent reachable from this tip, including ancestors older
    # than the bundle boundary that a range-limited walk would not inspect.
    commit_count(engine, engine_head)
    ancestor = git(engine, "merge-base", "--is-ancestor", commit, engine_head, accepted=(0, 1))
    if ancestor.returncode == 1:
        raise CheckError(f"bundle commit {commit} is not an ancestor of local {engine_ref} ({engine_head}); "
                         "ahead or diverged history has no behind count")
    count = commit_count(engine, f"{commit}..{engine_head}")
    return {"host_repository": str(host), "host_commit": host_head, "host_path": HOST_BUILD,
            "bundle_commit": commit, "engine_repository": str(engine), "engine_ref": engine_ref,
            "engine_commit": engine_head, "behind": count}


def main(arguments: list[str] | None = None, *, engine_repository: Path | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host-repository", required=True, type=Path,
                        help="explicit local host checkout; read its committed HEAD BUILD.json")
    parser.add_argument("--engine-ref", default=MASTER,
                        help="local Labrador comparison ref (default: refs/heads/master); never fetched")
    try:
        options = parser.parse_args(arguments)
    except SystemExit as error:
        return int(error.code)
    try:
        result = compare(options.host_repository,
                         engine_repository=REPOSITORY if engine_repository is None else engine_repository,
                         engine_ref=options.engine_ref)
    except (CheckError, OSError, UnicodeError) as error:
        print(f"demo lag: cannot compare: {error}", file=sys.stderr)
        return 2
    label = "local master" if result["engine_ref"] == MASTER else "local engine reference"
    print(f"Host source: {result['host_repository']} @ {result['host_commit']}:{result['host_path']}")
    print(f"Recorded clean bundle commit: {result['bundle_commit']}")
    print(f"Labrador checkout: {result['engine_repository']}")
    print(f"Comparison: {label} {result['engine_ref']} @ {result['engine_commit']}")
    print(f"Behind comparison: {result['behind']} commits")
    print("Committed metadata only; the local reference was not refreshed. Live responses and bundle bytes were not checked.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
