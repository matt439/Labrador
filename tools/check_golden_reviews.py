"""Check per-commit Golden-Reviewed declarations, not whether anyone looked.

Reads local Git objects only. It never fetches, writes history, decodes images
or runs a renderer. Invoke from the repository with --commit REV, or with
--base BASE --head HEAD. Ranges check each introduced commit independently.
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import subprocess
import sys

MARKER = "Golden-Reviewed: "
GOLDEN_DIRECTORY = "tests/render/golden/"


class CheckError(RuntimeError):
    """The requested Git scope could not be checked completely."""


def git(repository: Path, *arguments: str) -> bytes:
    environment = os.environ.copy()
    environment["GIT_OPTIONAL_LOCKS"] = "0"
    environment["GIT_TERMINAL_PROMPT"] = "0"
    try:
        result = subprocess.run(
            # Require local objects and the actual identities, even in a
            # partial clone or a checkout with replacement refs installed.
            ["git", "--no-pager", "--no-lazy-fetch", "--no-replace-objects",
             "-C", str(repository), *arguments],
            capture_output=True, env=environment, timeout=30, check=False)
    except (OSError, subprocess.TimeoutExpired) as error:
        raise CheckError(f"cannot read local Git history: {error}") from error
    if result.returncode:
        detail = result.stderr.decode("utf-8", errors="replace").strip()
        raise CheckError(f"git {arguments[0]} failed: {detail or 'no diagnostic'}")
    return result.stdout


def resolve_commit(repository: Path, revision: str) -> str:
    if not revision or revision.startswith("-"):
        raise CheckError("a commit revision must be nonempty and cannot start with '-'")
    value = git(repository, "rev-parse", "--verify", "--end-of-options",
                revision + "^{commit}").decode("ascii").strip()
    if not re.fullmatch(r"(?:[0-9a-f]{40}|[0-9a-f]{64})", value):
        raise CheckError(f"Git returned an invalid commit identity for {revision!r}")
    return value


def scope_commits(repository: Path, *, commit: str | None = None,
                  base: str | None = None, head: str | None = None) -> tuple[str, list[str]]:
    if git(repository, "rev-parse", "--is-shallow-repository").strip() != b"false":
        raise CheckError("shallow history cannot prove this scope; provide full local history (no fetch attempted)")
    if commit is not None:
        resolved = resolve_commit(repository, commit)
        return f"single commit {resolved}; ancestors are not checked", [resolved]
    resolved_base = resolve_commit(repository, base or "")
    resolved_head = resolve_commit(repository, head or "")
    commits = git(repository, "rev-list", "--reverse", "--topo-order",
                  resolved_base + ".." + resolved_head, "--").decode("ascii").splitlines()
    return f"introduced commits in {resolved_base}..{resolved_head}", commits


def review_declarations(message: str) -> set[str]:
    # Preserve path spaces and exact spelling. Only line endings are normalized.
    return {line.removesuffix("\r")[len(MARKER):]
            for line in message.split("\n") if line.startswith(MARKER)}


def changed_goldens(repository: Path, commit: str, parents: list[str]) -> set[str]:
    if len(parents) > 1:
        raise CheckError(f"{commit} is a merge commit; merge diffs are unsupported, not treated as empty")
    revisions = [parents[0], commit] if parents else ["--root", commit]
    raw = git(repository, "diff-tree", "--no-commit-id", "--name-only", "-r",
              "--no-renames", "--no-ext-diff", "--no-textconv", "-z",
              *revisions, "--", GOLDEN_DIRECTORY)
    paths = {path.decode("utf-8", errors="surrogateescape")
             for path in raw.split(b"\0") if path}
    paths = {path for path in paths if path.startswith(GOLDEN_DIRECTORY) and path.endswith(".png")}
    for path in paths:
        if "\n" in path or "\r" in path:
            raise CheckError(f"golden path cannot be represented by one review line: {path!r}")
    return paths


def missing_reviews(repository: Path, commits: list[str]) -> tuple[list[tuple[str, list[str]]], int]:
    missing = []
    changed_count = 0
    for commit in commits:
        # Read the actual headers, even at a history boundary. Never mistake a
        # missing parent for a root by relying on a truncated revision walk.
        contents = git(repository, "cat-file", "commit", commit)
        headers, separator, message = contents.partition(b"\n\n")
        if not separator:
            raise CheckError(f"{commit} has no commit-message separator")
        parents = [line[len(b"parent "):].decode("ascii")
                   for line in headers.split(b"\n") if line.startswith(b"parent ")]
        changed = changed_goldens(repository, commit, parents)
        changed_count += len(changed)
        declared = review_declarations(message.decode("utf-8", errors="surrogateescape"))
        omitted = sorted(changed - declared)
        if omitted:
            missing.append((commit, omitted))
    return missing, changed_count


def main(arguments: list[str] | None = None, *, repository: Path | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    scope = parser.add_mutually_exclusive_group(required=True)
    scope.add_argument("--commit", help="check exactly this commit, not its ancestors")
    scope.add_argument("--base", help="exclude commits reachable from this base")
    parser.add_argument("--head", help="check introduced commits reachable from this head")
    options = parser.parse_args(arguments)
    if options.base is not None and options.head is None:
        parser.error("--base requires --head")
    if options.commit is not None and options.head is not None:
        parser.error("--head requires --base and cannot accompany --commit")
    try:
        location = repository if repository is not None else Path.cwd()
        # diff-tree pathspecs are relative to its working directory. Normalize
        # first so running this script from tools/ cannot silently find no PNGs.
        root = git(location, "rev-parse", "--show-toplevel").decode("utf-8", errors="surrogateescape")
        location = Path(root.removesuffix("\n").removesuffix("\r"))
        label, commits = scope_commits(location, commit=options.commit,
                                       base=options.base, head=options.head)
        print("Golden review scope: " + label)
        missing, changed_count = missing_reviews(location, commits)
    except CheckError as error:
        print(f"Golden review check could not complete: {error}", file=sys.stderr)
        return 2
    if missing:
        for commit, paths in missing:
            print(f"{commit} lacks exact Golden-Reviewed declarations:", file=sys.stderr)
            for path in paths:
                print("  " + repr(path), file=sys.stderr)
        return 1
    print(f"Declarations cover {changed_count} changed golden paths across {len(commits)} commits. "
          "This checks declarations, not visual review.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
