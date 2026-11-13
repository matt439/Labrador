"""Check that a release commit is ready to tag, and print its release notes.

CONTRIBUTING.md's *Cutting a release* owns the procedure; this checks the
parts of its steps 1 and 2 that can be read from the tree at --head:

- the release is named vX.Y.Z, and vcpkg.json's version-string is X.Y.Z;
- no tag of that name names another commit;
- --base is a vX.Y.Z tag that the head contains, and before 1.0 the number
  moves as *The number* says: the second number for a release with a
  CHANGES.md entry, the third for one with none;
- CHANGES.md has no `## Unreleased`, its newest release heading is
  `## vX.Y.Z (date)` dated with the head's author date, and the release
  heading after it is the base's;
- every entry in samples/minimal/README.md's change list names a commit the
  head contains (an entry without one is given the commit `git log -S` finds);
- check_changes_rule.py passes over base..head.

Each mismatch is a failure line naming what is wrong, and the exit status is
1. Commits in the range that change an engine .cpp file without touching
CHANGES.md are listed for reading: whether one is a behaviour change a game
would see, and whether an entry or a declaration is right, is the audit's
reading, not this tool's.

--notes prints step 5's notes body on stdout, once every check passes, and
the report on stderr, so `> notes.md` captures the notes alone.

Reads local Git objects only. It tags, pushes, uploads, fetches and writes
nothing; the rebuild, signing and upload steps stay the owner's.
"""

from __future__ import annotations

import argparse
import datetime
import json
from pathlib import Path
import re
import sys
from typing import TextIO

sys.path.insert(0, str(Path(__file__).resolve().parent))
import check_changes_rule as changes_rule  # noqa: E402
import check_golden_reviews as golden  # noqa: E402

CheckError = golden.CheckError

REPOSITORY_URL = "https://github.com/matt439/Labrador"
STARTER_URL = "https://labradorengine.com/docs/get-started/starter/"
VERSIONS_URL = "https://labradorengine.com/versions/"
VERSION = re.compile(r"v(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)")
RELEASE_HEADING = re.compile(r"## (v\d+\.\d+\.\d+) \((\d{4}-\d{2}-\d{2})\)")
DATED_ENTRY = re.compile(r"### (\d{4}-\d{2}-\d{2})\b")
MINIMAL_README = "samples/minimal/README.md"
MINIMAL_LIST_HEADING = "## A copy is yours"
MINIMAL_ENTRY = re.compile(r"- \*\*(\d{4}-\d{2}-\d{2})(?:, `([0-9a-f]{7,64})`)?\.\*\*\s*(.*)")
PUBLIC_FACE_LEAD = "Releases are versioned, and before 1.0 a release promises a record, not compatibility."
PUBLIC_FACE_SECOND = re.compile(r"A 0\.x tag promises one thing about the tag before it.*?\.(?=\s|$)")
SETTLE_LEAD = re.compile(r"\*\*(The API settles after .*?\.)\*\*")
# PHILOSOPHY's settle criterion: twelve weeks and two 0.x tags without an entry.
SETTLE_WEEKS = 12
SETTLE_TAGS = 2


def parse_version(name: str) -> tuple[int, int, int] | None:
    match = VERSION.fullmatch(name)
    return None if match is None else (int(match[1]), int(match[2]), int(match[3]))


def version_name(version: tuple[int, int, int]) -> str:
    return "v{}.{}.{}".format(*version)


def anchor(heading: str) -> str:
    """GitHub's fragment for a Markdown heading's text."""
    return re.sub(r"[^\w\- ]", "", heading.strip().lower()).replace(" ", "-")


def release_sections(changes: str) -> tuple[bool, list[tuple[str, str, list[str]]]]:
    """Whether `## Unreleased` is present, and each release heading under
    `## Entries` newest first: its tag, its date and its dated entries' dates."""
    unreleased = False
    sections: list[tuple[str, str, list[str]]] = []
    in_entries = False
    current: list[str] | None = None
    for line in changes.splitlines():
        if line.startswith("## "):
            current = None
            if line.strip() == "## Entries":
                in_entries = True
                continue
            if not in_entries:
                continue
            if line.strip() == "## Unreleased":
                unreleased = True
                continue
            match = RELEASE_HEADING.fullmatch(line.strip())
            if match is not None:
                current = []
                sections.append((match[1], match[2], current))
            continue
        if current is not None:
            entry = DATED_ENTRY.match(line)
            if entry is not None:
                current.append(entry[1])
    return unreleased, sections


def check_version_string(vcpkg: str, release: tuple[int, int, int]) -> list[str]:
    try:
        value = json.loads(vcpkg).get("version-string")
    except (ValueError, AttributeError):
        return ["vcpkg.json at the head is not a JSON object"]
    expected = "{}.{}.{}".format(*release)
    if value != expected:
        return [f"vcpkg.json's version-string is {value!r}; the release commit sets it to {expected!r} (step 2)"]
    return []


def check_changes(changes: str, release: str, base: str, head_date: str) -> tuple[list[str], int | None]:
    """Failures in CHANGES.md's headings, and the release's entry count."""
    failures: list[str] = []
    unreleased, sections = release_sections(changes)
    if unreleased:
        failures.append("CHANGES.md still has `## Unreleased`; the release commit renames it to "
                        f"`## {release} (date)` (step 2)")
    if not sections or sections[0][0] != release:
        newest = sections[0][0] if sections else "none"
        failures.append(f"CHANGES.md's newest release heading is {newest}, not `## {release} (date)`")
        return failures, None
    _, date, entries = sections[0]
    if date != head_date:
        failures.append(f"CHANGES.md's `## {release} ({date})` is not dated with the head commit's "
                        f"author date, {head_date} (CHANGES.md, *Entries*)")
    previous = sections[1][0] if len(sections) > 1 else "none"
    if previous != base:
        failures.append(f"CHANGES.md's release heading after {release} is {previous}, not the base {base}")
    return failures, len(entries)


def check_number(release: tuple[int, int, int], base: tuple[int, int, int], entries: int | None) -> list[str]:
    name, base_name = version_name(release), version_name(base)
    if release <= base:
        return [f"{name} does not come after the base {base_name}"]
    if release[0] >= 1 or entries is None:
        # From 1.0 the first number moves for a break, which only reading tells,
        # and 1.0 itself follows the settle criterion, not the batch.
        return []
    if entries:
        expected = (base[0], base[1] + 1, 0)
        reason = f"carries {entries} CHANGES.md entr{'y' if entries == 1 else 'ies'}"
    else:
        expected = (base[0], base[1], base[2] + 1)
        reason = "carries no CHANGES.md entry"
    if release != expected:
        return [f"{name} {reason}, so before 1.0 it is {version_name(expected)} after {base_name} "
                "(CONTRIBUTING.md, *The number*)"]
    return []


def minimal_entries(readme: str) -> list[tuple[str, str | None, str]]:
    """Each change-list entry's date, commit id (or None) and first line of text."""
    entries: list[tuple[str, str | None, str]] = []
    inside = False
    for line in readme.splitlines():
        if line.startswith("## "):
            inside = line.strip() == MINIMAL_LIST_HEADING
            continue
        if inside:
            match = MINIMAL_ENTRY.match(line)
            if match is not None:
                entries.append((match[1], match[2], match[3].strip()))
    return entries


def public_face_sentences(philosophy: str) -> tuple[str, str, str] | None:
    """The bold lead, the 0.x sentence and the settle criterion's lead, word for word."""
    flat = " ".join(line.strip() for line in philosophy.splitlines())
    lead = f"**{PUBLIC_FACE_LEAD}**"
    start = flat.find(lead)
    if start < 0:
        return None
    second = PUBLIC_FACE_SECOND.search(flat, start)
    settle = SETTLE_LEAD.search(flat, start)
    if second is None or settle is None:
        return None
    return lead, second[0], settle[1]


def supported_revisions(security: str) -> str | None:
    lines = security.splitlines()
    try:
        start = lines.index("## Supported revisions") + 1
    except ValueError:
        return None
    paragraph: list[str] = []
    for line in lines[start:]:
        if not line.strip():
            if paragraph:
                break
            continue
        if line.startswith("#"):
            break
        paragraph.append(line.strip())
    return " ".join(paragraph) or None


def settle_status(sections: list[tuple[str, str, list[str]]], release: tuple[int, int, int]) -> str:
    if release[0] >= 1:
        return "The criterion has run: this is a tag from 1.0 on."
    since: list[str] = []
    for tag, _, entries in sections:
        if entries:
            newest = max(entries)
            break
        since.append(tag)
    else:
        return "Unmet: CHANGES.md has no dated entry to count from."
    release_date = datetime.date.fromisoformat(sections[0][1])
    weeks = (release_date - datetime.date.fromisoformat(newest)).days // 7
    if not since:
        return (f"Unmet: CHANGES.md's newest entry is dated {newest}, under this release, "
                "so no tag without an entry has been cut since it.")
    tags = ", ".join(reversed(since))
    count = "one 0.x tag" if len(since) == 1 else f"{len(since)} 0.x tags"
    verdict = "Both have run" if weeks >= SETTLE_WEEKS and len(since) >= SETTLE_TAGS else "Unmet"
    return (f"{verdict}: CHANGES.md's newest entry is dated {newest}, {weeks} "
            f"week{'' if weeks == 1 else 's'} before this release, and {count} ({tags}) "
            "with no entry have been cut since it.")


def notes(release: str, changes: str, philosophy: str, security: str) -> tuple[str | None, list[str]]:
    failures: list[str] = []
    sentences = public_face_sentences(philosophy)
    if sentences is None:
        failures.append("PHILOSOPHY.md's *The public face* no longer holds the release sentences "
                        "step 5 quotes word for word")
    revisions = supported_revisions(security)
    if revisions is None:
        failures.append("SECURITY.md has no paragraph under `## Supported revisions`")
    _, sections = release_sections(changes)
    if not sections or sections[0][0] != release:
        failures.append(f"CHANGES.md's newest release heading is not {release}, so the notes cannot link it")
    if failures:
        return None, failures
    lead, second, settle = sentences
    heading = f"{release} ({sections[0][1]})"
    blob = f"{REPOSITORY_URL}/blob/{release}"
    body = [
        f"**Download `labrador-starter-{release}.zip`, then follow "
        f"[Get the starter project]({STARTER_URL}).**",
        f"{lead} {second}",
        f"What changed: [CHANGES.md at {release}]({blob}/CHANGES.md#{anchor(heading)}).",
        f"{settle} {settle_status(sections, parse_version(release))}",
        f"Supported revisions: {revisions}",
        f"What a release promises: [PHILOSOPHY.md at {release}]({blob}/docs/design/PHILOSOPHY.md#the-public-face).",
        f"`labrador-starter-{release}.zip` is the Get Started project with this release inside it, "
        f"built without Git: <{STARTER_URL}>.",
        f"`labrador-docs-{release}.tar.gz` is the website's frozen copy of this release's documentation, "
        f"published at <{VERSIONS_URL}{release}/>; a reader does not need to download it.",
    ]
    return "\n\n".join(body) + "\n", []


def show(location: Path, head: str, path: str) -> str:
    return golden.git(location, "show", f"{head}:{path}").decode("utf-8", errors="replace")


def tag_commit(location: Path, name: str) -> str | None:
    result = golden.git_local.git(location, "rev-parse", "--verify", "--quiet", "--end-of-options",
                                  f"refs/tags/{name}^{{commit}}", accepted=(0, 1), error=CheckError)
    value = result.stdout.decode("ascii").strip()
    return value or None


def contains(location: Path, ancestor: str, head: str) -> bool:
    result = golden.git_local.git(location, "merge-base", "--is-ancestor", ancestor, head,
                                  accepted=(0, 1), error=CheckError)
    return result.returncode == 0


def adding_commit(location: Path, head: str, text: str) -> str | None:
    """The oldest commit in the head's history that changes how often the
    entry's opening text occurs in the README, so an entry older than the base
    is found too."""
    snippet = text[:60]
    if not snippet:
        return None
    found = golden.git(location, "log", "--reverse", "--format=%h", "-S", snippet,
                       head, "--", MINIMAL_README).decode("ascii").split()
    return found[0] if found else None


def check_minimal(location: Path, readme: str, head: str) -> list[str]:
    failures: list[str] = []
    for date, commit, text in minimal_entries(readme):
        summary = text if len(text) <= 60 else text[:57] + "..."
        if commit is None:
            found = adding_commit(location, head, text)
            hint = f"; `git log -S` finds {found}" if found else ""
            failures.append(f"{MINIMAL_README}'s change-list entry {date} ({summary}) has no commit id{hint} (step 2)")
            continue
        result = golden.git_local.git(location, "rev-parse", "--verify", "--quiet", "--end-of-options",
                                      f"{commit}^{{commit}}", accepted=(0, 1), error=CheckError)
        resolved = result.stdout.decode("ascii").strip()
        if not resolved or not contains(location, resolved, head):
            failures.append(f"{MINIMAL_README}'s change-list entry {date}, `{commit}`, names no commit the head contains")
    return failures


def audit(location: Path, commits: list[str], exceptions: set[str]) -> tuple[list[str], int, list[tuple[str, str]]]:
    """check_changes_rule.py's failures over the commits, how many touch a path
    the rule names, and the commits that change an engine .cpp file without
    touching CHANGES.md, which are listed for reading."""
    unclassified, governed = changes_rule.unclassified(location, commits, exceptions)
    failures = [f"{commit[:7]} ({subject}) touches {', '.join(rule)} without touching "
                "CHANGES.md or saying `not consumer-visible` (check_changes_rule.py; step 1)"
                for commit, subject, rule in unclassified]
    reading: list[tuple[str, str]] = []
    for commit in commits:
        parents, message_bytes = golden.commit_parts(location, commit)
        paths = golden.changed_paths(location, commit, parents)
        if changes_rule.CHANGES not in paths and any(path.startswith("engine/") and path.endswith(".cpp")
                                                     for path in paths):
            subject = message_bytes.decode("utf-8", errors="replace").split("\n", 1)[0]
            reading.append((commit[:7], subject))
    return failures, governed, reading


def main(arguments: list[str] | None = None, *, repository: Path | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--release", required=True, help="the tag to cut, vX.Y.Z")
    parser.add_argument("--base", required=True, help="the newest tag before it, vX.Y.Z")
    parser.add_argument("--head", default="HEAD", help="the release commit (default HEAD)")
    parser.add_argument("--notes", action="store_true",
                        help="print step 5's notes body on stdout once every check passes")
    options = parser.parse_args(arguments)
    report: TextIO = sys.stderr if options.notes else sys.stdout
    release, base = parse_version(options.release), parse_version(options.base)
    if release is None or base is None:
        parser.error("--release and --base are tag names of the form vX.Y.Z")
    failures: list[str] = []
    try:
        location = repository if repository is not None else Path.cwd()
        root = golden.git(location, "rev-parse", "--show-toplevel").decode("utf-8", errors="surrogateescape")
        location = Path(root.removesuffix("\n").removesuffix("\r"))
        head = golden.resolve_commit(location, options.head)
        base_commit = tag_commit(location, options.base)
        if base_commit is None:
            raise CheckError(f"no tag {options.base} in the local repository (no fetch attempted)")
        if not contains(location, base_commit, head):
            raise CheckError(f"the head {head} does not contain the base tag {options.base}")
        print(f"Release check: {options.release} at {head}, after {options.base}", file=report)
        existing = tag_commit(location, options.release)
        if existing is not None and existing != head:
            failures.append(f"the tag {options.release} already names {existing}, not the head")
        head_date = golden.git(location, "log", "-1", "--format=%as", head).decode("ascii").strip()
        failures += check_version_string(show(location, head, "vcpkg.json"), release)
        changes = show(location, head, changes_rule.CHANGES)
        heading_failures, entries = check_changes(changes, options.release, options.base, head_date)
        failures += heading_failures
        failures += check_number(release, base, entries)
        failures += check_minimal(location, show(location, head, MINIMAL_README), head)
        _, commits = golden.scope_commits(location, base=base_commit, head=head)
        audit_failures, governed, reading = audit(location, commits, changes_rule.recorded_exceptions(changes))
        failures += audit_failures
        body, note_failures = (notes(options.release, changes, show(location, head, "docs/design/PHILOSOPHY.md"),
                                     show(location, head, "SECURITY.md"))
                               if options.notes else (None, []))
        failures += note_failures
    except CheckError as error:
        print(f"Release check could not complete: {error}", file=sys.stderr)
        return 2
    if reading:
        print(f"{len(reading)} commit(s) change an engine .cpp file without touching CHANGES.md; "
              "read each for a behaviour change a game would see (step 1):", file=report)
        for commit, subject in reading:
            print(f"  {commit} {subject}", file=report)
    report.flush()
    if failures:
        for failure in failures:
            print(failure, file=sys.stderr)
        print(f"{len(failures)} mismatch(es); {options.release} is not ready to tag.", file=sys.stderr)
        return 1
    print(f"{len(commits)} commits, {governed} touching a path CHANGES.md's rule names, and "
          f"{entries} CHANGES.md entr{'y' if entries == 1 else 'ies'}; every check passes. "
          "Steps 3 to 5 (push runs, ci_gap_report.py, tag, draft, assets) remain.", file=report)
    if body is not None:
        sys.stdout.write(body)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
