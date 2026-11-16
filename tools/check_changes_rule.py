"""Check CHANGES.md's rule for each commit, not whether a change is consumer-visible.

A commit that touches engine/**/*.h, vcpkg.json or an explicitly inventoried
consumer build input passes when it also touches CHANGES.md,
when its message says `not consumer-visible` (any case, a line break allowed
between the two words), or when CHANGES.md at the checked head names it as
the exception to its rule. Whether an entry is right, or whether a .cpp
change is a behaviour change, is the release audit's reading, not this.

Reads local Git objects only, with check_golden_reviews.py's scopes: --commit
REV, or --base BASE --head HEAD for each commit the range introduces.
The build-input inventory is RULE_PATHS below and in CHANGES.md, The rule.
Its expanded paths apply after f5c17f32; older commits keep the original path
coverage. The boundary does not classify those older changes as non-visible.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import check_golden_reviews as golden  # noqa: E402

ORIGINAL_RULE_PATHS = ("engine/CMakeLists.txt", "cmake/settings.cmake", "vcpkg.json")
RULE_PATHS = (*ORIGINAL_RULE_PATHS, "CMakeLists.txt", "engine/math/CMakeLists.txt",
              "cmake/source_revision.cmake", "cmake/compile_shaders.cmake",
              "cmake/version.cmake", "cmake/version_values.h.in", "cmake/release_identity.txt",
              "cmake/dependencies.cmake",
              "cmake/add_material.cmake", "cmake/embed_material.cmake",
              "cmake/check_engine_includes.cmake", "cmake/native_release_ipo.cmake")
# The approved scope before the coverage expansion. Preserve the policy those
# ancestors were reviewed under, without exempting their originally governed paths.
EXPANDED_COVERAGE_BASE = "f5c17f32f57205f3eea4142380845e382fcc27de"
CHANGES = "CHANGES.md"
DECLARATION = re.compile(r"not\s+consumer-visible", re.IGNORECASE)
# CHANGES.md's audit sentence for a release: "... says `not consumer-visible`
# in its message except `0c15f34`, which ...".
EXCEPTION = re.compile(r"`not consumer-visible`\s+in\s+its\s+message\s+except\s+`([0-9a-f]{7,64})`")

CheckError = golden.CheckError


def is_rule_path(path: str) -> bool:
    return path in RULE_PATHS or (path.startswith("engine/") and path.endswith(".h"))


def original_rule_path(path: str) -> bool:
    return path in ORIGINAL_RULE_PATHS or (path.startswith("engine/") and path.endswith(".h"))


def original_coverage_commits(repository: Path) -> set[str]:
    """The policy boundary's ancestors, when that exact boundary is present.

    Unrelated repositories, including fixtures, get the complete current rule.
    Reading refs first distinguishes an absent boundary from a failing Git read.
    """
    present = golden.git(repository, "rev-list", "--all", "HEAD").decode("ascii").splitlines()
    if EXPANDED_COVERAGE_BASE not in present:
        return set()
    return set(golden.git(repository, "rev-list", EXPANDED_COVERAGE_BASE).decode("ascii").splitlines())


def declares(message: str) -> bool:
    return DECLARATION.search(message) is not None


def recorded_exceptions(changes: str) -> set[str]:
    return set(EXCEPTION.findall(changes))


def is_exception(commit: str, exceptions: set[str]) -> bool:
    return any(commit.startswith(prefix) for prefix in exceptions)


def head_changes(repository: Path, head: str) -> str:
    try:
        return golden.git(repository, "show", f"{head}:{CHANGES}").decode("utf-8", errors="replace")
    except CheckError:
        return ""


def unclassified(repository: Path, commits: list[str], exceptions: set[str]) -> tuple[list[tuple[str, str, list[str]]], int]:
    failures = []
    governed = 0
    original_coverage = original_coverage_commits(repository)
    for commit in commits:
        parents, message_bytes = golden.commit_parts(repository, commit)
        paths = golden.changed_paths(repository, commit, parents)
        rule = sorted(path for path in paths if is_rule_path(path) and
                      (commit not in original_coverage or original_rule_path(path)))
        if not rule:
            continue
        governed += 1
        message = message_bytes.decode("utf-8", errors="replace")
        if CHANGES in paths or declares(message) or is_exception(commit, exceptions):
            continue
        failures.append((commit, message.split("\n", 1)[0], rule))
    return failures, governed


def main(arguments: list[str] | None = None, *, repository: Path | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
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
        root = golden.git(location, "rev-parse", "--show-toplevel").decode("utf-8", errors="surrogateescape")
        location = Path(root.removesuffix("\n").removesuffix("\r"))
        label, commits = golden.scope_commits(location, commit=options.commit,
                                              base=options.base, head=options.head)
        head = golden.resolve_commit(location, options.commit or options.head)
        exceptions = recorded_exceptions(head_changes(location, head))
        print("CHANGES.md rule scope: " + label)
        failures, governed = unclassified(location, commits, exceptions)
    except CheckError as error:
        print(f"CHANGES.md rule check could not complete: {error}", file=sys.stderr)
        return 2
    if failures:
        for commit, subject, paths in failures:
            print(f"{commit} ({subject}) touches {', '.join(paths)} without touching {CHANGES} "
                  "or saying `not consumer-visible` in its message", file=sys.stderr)
        print(f"Add an entry under ## Unreleased in {CHANGES}, or reword the commit's message "
              "(CONTRIBUTING.md, step 3).", file=sys.stderr)
        return 1
    print(f"{governed} of {len(commits)} commits touch a path the rule names; each touches {CHANGES}, "
          "says `not consumer-visible` or is a recorded exception. This checks the declaration, "
          "not whether it is right.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
