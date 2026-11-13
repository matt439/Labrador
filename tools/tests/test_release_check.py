"""release_check.py's release preflight through isolated local Git objects."""

from __future__ import annotations

import contextlib
import io
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

from tools.tests.tool_fixtures import isolate_repository, isolated_git_environment, load_tool

REPOSITORY = Path(__file__).resolve().parents[2]
release_check = load_tool("tools/release_check.py")

PHILOSOPHY = """# Philosophy

### The public face

- **Releases are versioned, and before 1.0 a release promises a record, not
  compatibility.** A release is a tag on `master`, and its notes link
  CHANGES.md. A 0.x tag promises one thing about the tag before it: every
  consumer-visible change between the two is an entry in CHANGES.md, with its
  migration where it has one. Breaking changes are deliberate.
- **The API settles after twelve weeks and two 0.x tags in which CHANGES.md
  gains no entry.** The weeks are counted on CHANGES.md's own dates.
"""

SECURITY = """# Security policy

## Supported revisions

The latest tag and `master` are supported; a fix ships as a new tag.

Before reporting, check current `master`.
"""

CHANGES_PREAMBLE = "# Changes\n\n## The rule\n\nText.\n\n## Entries\n\nHow entries work.\n\n"

MINIMAL = """# Minimal

## A copy is yours

The list.

{entries}
## The font, and its licence

- **2020-01-01.** Not a change-list entry.
"""


def changes(*sections: str) -> bytes:
    return (CHANGES_PREAMBLE + "\n".join(sections)).encode("utf-8")


def minimal(*entries: str) -> bytes:
    return MINIMAL.format(entries="".join(entry + "\n" for entry in entries)).encode("utf-8")


def vcpkg(version: str) -> bytes:
    return ('{\n  "name": "labrador",\n  "version-string": "' + version + '"\n}\n').encode("utf-8")


class TextTests(unittest.TestCase):
    def test_anchor_is_githubs_fragment_for_a_release_heading(self):
        self.assertEqual(release_check.anchor("v0.4.0 (2026-11-20)"), "v040-2026-11-20")

    def test_release_sections_read_headings_and_dated_entries_under_entries_only(self):
        text = changes("## Unreleased\n\n### 2026-11-13 - New\n\n",
                       "## v0.2.0 (2026-10-25)\n\nA note.\n\n### 2026-10-20 - A\n\n### 2026-10-19 - B\n\n",
                       "## v0.1.0 (2026-10-13)\n\n### Backfill: from a to b\n\n").decode("utf-8")
        unreleased, sections = release_check.release_sections(text)
        self.assertTrue(unreleased)
        self.assertEqual(sections, [("v0.2.0", "2026-10-25", ["2026-10-20", "2026-10-19"]),
                                    ("v0.1.0", "2026-10-13", [])])

    def test_the_number_moves_by_whether_the_release_has_an_entry(self):
        self.assertEqual(release_check.check_number((0, 4, 0), (0, 3, 0), 2), [])
        self.assertEqual(release_check.check_number((0, 3, 1), (0, 3, 0), 0), [])
        wrong = release_check.check_number((0, 3, 1), (0, 3, 0), 1)
        self.assertEqual(len(wrong), 1)
        self.assertIn("v0.4.0", wrong[0])
        self.assertIn("v0.3.1", release_check.check_number((0, 4, 0), (0, 3, 0), 0)[0])
        self.assertIn("does not come after", release_check.check_number((0, 3, 0), (0, 3, 0), 0)[0])
        self.assertEqual(release_check.check_number((1, 0, 0), (0, 9, 0), 3), [])
        self.assertEqual(release_check.check_number((2, 0, 0), (1, 4, 2), 3), [])

    def test_the_settle_status_counts_tags_without_an_entry_since_the_newest(self):
        sections = [("v0.6.0", "2026-12-30", []), ("v0.5.0", "2026-12-01", []),
                    ("v0.4.0", "2026-11-20", ["2026-11-15"])]
        status = release_check.settle_status(sections, (0, 6, 0))
        self.assertIn("2026-11-15, 6 weeks before this release", status)
        self.assertIn("2 0.x tags (v0.5.0, v0.6.0)", status)
        self.assertTrue(status.startswith("Unmet:"))
        own = release_check.settle_status(sections[2:], (0, 4, 0))
        self.assertIn("under this release", own)

    def test_the_settle_status_says_both_have_run_after_twelve_weeks_and_two_tags(self):
        sections = [("v0.6.0", "2027-02-07", []), ("v0.5.0", "2027-01-10", []),
                    ("v0.4.0", "2026-11-20", ["2026-11-15"])]
        status = release_check.settle_status(sections, (0, 6, 0))
        self.assertTrue(status.startswith("Both have run: CHANGES.md's newest entry is dated 2026-11-15, "
                                          "12 weeks before this release"), status)
        one_tag = release_check.settle_status(sections[1:], (0, 5, 0))
        self.assertTrue(one_tag.startswith("Unmet:"), one_tag)

    def test_the_minimal_change_list_is_the_one_under_a_copy_is_yours(self):
        readme = minimal("- **2026-11-10.** Something new,", "- **2026-10-31, `4bc1aa2`.** Older.").decode()
        self.assertEqual(release_check.minimal_entries(readme),
                         [("2026-11-10", None, "Something new,"), ("2026-10-31", "4bc1aa2", "Older.")])

    def test_this_repositorys_philosophy_and_security_still_hold_what_the_notes_quote(self):
        philosophy = (REPOSITORY / "docs/design/PHILOSOPHY.md").read_text(encoding="utf-8")
        security = (REPOSITORY / "SECURITY.md").read_text(encoding="utf-8")
        sentences = release_check.public_face_sentences(philosophy)
        self.assertIsNotNone(sentences, "CONTRIBUTING step 5 quotes The public face word for word")
        self.assertTrue(sentences[1].startswith("A 0.x tag promises one thing about the tag before it"))
        self.assertTrue(sentences[2].startswith("The API settles after twelve weeks"))
        self.assertTrue(release_check.supported_revisions(security).startswith("The latest tag"))


class GitFixtureTests(unittest.TestCase):
    def setUp(self):
        if not shutil.which("git"):
            self.skipTest("Git is required for isolated commit fixtures")
        temporary = tempfile.TemporaryDirectory(prefix="labrador-release-check-")
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        self.repository = root / "repository"
        self.repository.mkdir()
        environment, hooks = isolated_git_environment(root, "Release fixture")
        isolation = mock.patch.dict(os.environ, environment, clear=True)
        isolation.start()
        self.addCleanup(isolation.stop)
        self.git("init", "-q", "--initial-branch=fixture")
        isolate_repository(self.git, hooks)
        self.base = self.commit("First release", {
            "vcpkg.json": vcpkg("0.1.0"),
            "CHANGES.md": changes("## v0.1.0 (2026-10-13)\n\n### 2026-10-12 - Start\n\nText.\n"),
            "samples/minimal/README.md": minimal("- **2026-10-12, `0000000`.** Start."),
            "docs/design/PHILOSOPHY.md": PHILOSOPHY.encode("utf-8"),
            "SECURITY.md": SECURITY.encode("utf-8"),
        }, date="2026-10-13T12:00:00+00:00")
        # The first entry names the base itself; rewrite it now its id is known.
        self.base = self.commit("Name the first entry's commit", {
            "samples/minimal/README.md": minimal(f"- **2026-10-12, `{self.base[:7]}`.** Start."),
        }, date="2026-10-13T12:00:00+00:00")
        self.git("tag", "v0.1.0", self.base)
        self.change = self.commit("Rename a thing", {
            "engine/core/thing.h": b"#pragma once\n",
            "CHANGES.md": changes("## Unreleased\n\n### 2026-11-01 - Rename a thing\n\nText.\n",
                                  "## v0.1.0 (2026-10-13)\n\n### 2026-10-12 - Start\n\nText.\n"),
            "samples/minimal/README.md": minimal("- **2026-11-01.** The copy renames a thing.",
                                                 f"- **2026-10-12, `{self.base[:7]}`.** Start."),
        }, date="2026-11-01T12:00:00+00:00")

    def git(self, *arguments, data=None):
        result = subprocess.run(["git", "--no-pager", "-C", str(self.repository), *arguments],
                                input=data, capture_output=True, timeout=30, check=False)
        if result.returncode:
            self.fail(result.stderr.decode("utf-8", errors="replace"))
        return result.stdout

    def commit(self, message, files=None, date="2026-11-02T12:00:00+00:00"):
        for relative, contents in (files or {}).items():
            target = self.repository / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(contents)
        self.git("add", "--all")
        self.git("commit", "-q", "--allow-empty", "--no-verify", "--no-gpg-sign", "--cleanup=verbatim",
                 f"--date={date}", "--file=-", data=message.encode("utf-8"))
        return self.git("rev-parse", "HEAD").decode("ascii").strip()

    def prepare(self, release="0.2.0", date="2026-11-20", heading_date=None):
        return self.commit("Prepare v" + release + "\n\nnot consumer-visible: the version-string.", {
            "vcpkg.json": vcpkg(release),
            "CHANGES.md": changes(f"## v{release} ({heading_date or date})\n\n### 2026-11-01 - Rename a thing\n\nText.\n",
                                  "## v0.1.0 (2026-10-13)\n\n### 2026-10-12 - Start\n\nText.\n"),
            "samples/minimal/README.md": minimal(f"- **2026-11-01, `{self.change[:7]}`.** The copy renames a thing.",
                                                 f"- **2026-10-12, `{self.base[:7]}`.** Start."),
        }, date=date + "T12:00:00+00:00")

    def run_check(self, *arguments):
        output, errors = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors):
            result = release_check.main(list(arguments), repository=self.repository)
        return result, output.getvalue(), errors.getvalue()

    def test_a_prepared_release_passes_and_prints_its_notes(self):
        self.prepare()
        code, output, errors = self.run_check("--release", "v0.2.0", "--base", "v0.1.0", "--notes")
        self.assertEqual(code, 0, errors)
        self.assertIn("every check passes", errors)
        self.assertTrue(output.startswith("**Download `labrador-starter-v0.2.0.zip`, then follow "
                                          "[Get the starter project]"
                                          "(https://labradorengine.com/docs/get-started/starter/).**\n\n"
                                          "**Releases are versioned, and before 1.0 a release promises a record, "
                                          "not compatibility.** A 0.x tag promises one thing about the tag "
                                          "before it: every consumer-visible change between the two is an entry "
                                          "in CHANGES.md, with its migration where it has one.\n"))
        self.assertIn("`labrador-docs-v0.2.0.tar.gz` is the website's frozen copy", output)
        self.assertIn("<https://labradorengine.com/versions/v0.2.0/>; a reader does not need to download it.",
                      output)
        self.assertIn("https://github.com/matt439/Labrador/blob/v0.2.0/CHANGES.md#v020-2026-11-20", output)
        self.assertIn("The API settles after twelve weeks and two 0.x tags in which CHANGES.md gains no entry. "
                      "Unmet: CHANGES.md's newest entry is dated 2026-11-01, under this release", output)
        self.assertIn("Supported revisions: The latest tag and `master` are supported; a fix ships as a new tag.",
                      output)
        self.assertIn("blob/v0.2.0/docs/design/PHILOSOPHY.md#the-public-face", output)
        self.assertIn("`labrador-starter-v0.2.0.zip`", output)

    def test_an_unprepared_tree_fails_by_name_on_each_step_2_mismatch(self):
        code, output, errors = self.run_check("--release", "v0.2.0", "--base", "v0.1.0", "--notes")
        self.assertEqual(code, 1)
        self.assertEqual(output, "")
        self.assertIn("version-string is '0.1.0'", errors)
        self.assertIn("still has `## Unreleased`", errors)
        self.assertIn("2026-11-01 (The copy renames a thing.) has no commit id; `git log -S` finds "
                      + self.change[:7], errors)

    def test_a_heading_not_dated_with_the_heads_author_date_fails(self):
        self.prepare(heading_date="2026-11-19")
        code, _, errors = self.run_check("--release", "v0.2.0", "--base", "v0.1.0")
        self.assertEqual(code, 1)
        self.assertIn("author date, 2026-11-20", errors)

    def test_a_release_number_the_batch_does_not_allow_fails(self):
        self.prepare(release="0.1.1")
        code, _, errors = self.run_check("--release", "v0.1.1", "--base", "v0.1.0")
        self.assertEqual(code, 1)
        self.assertIn("so before 1.0 it is v0.2.0 after v0.1.0", errors)

    def test_a_tag_that_names_another_commit_fails(self):
        self.git("tag", "v0.2.0", self.change)
        self.prepare()
        code, _, errors = self.run_check("--release", "v0.2.0", "--base", "v0.1.0")
        self.assertEqual(code, 1)
        self.assertIn("already names " + self.change, errors)

    def test_an_undeclared_rule_path_fails_and_a_source_change_is_listed_for_reading(self):
        forgotten = self.commit("Forgotten flag", {"cmake/settings.cmake": b"flags\n"})
        source = self.commit("Change behaviour quietly", {"engine/core/thing.cpp": b"source\n"})
        self.prepare()
        code, output, errors = self.run_check("--release", "v0.2.0", "--base", "v0.1.0")
        self.assertEqual(code, 1)
        self.assertIn(forgotten[:7] + " (Forgotten flag) touches cmake/settings.cmake", errors)
        self.assertIn(source[:7] + " Change behaviour quietly", output)

    def test_a_change_list_id_the_head_does_not_contain_fails(self):
        self.prepare()
        self.commit("Name a missing commit", {
            "samples/minimal/README.md": minimal("- **2026-11-01, `abcdef1`.** The copy renames a thing.",
                                                 f"- **2026-10-12, `{self.base[:7]}`.** Start."),
        }, date="2026-11-20T13:00:00+00:00")
        code, _, errors = self.run_check("--release", "v0.2.0", "--base", "v0.1.0")
        self.assertEqual(code, 1)
        self.assertIn("2026-11-01, `abcdef1`, names no commit the head contains", errors)

    def test_an_entry_older_than_the_base_without_an_id_is_given_its_commit(self):
        older = self.commit("Add an older entry", {
            "samples/minimal/README.md": minimal("- **2026-10-05.** An entry from before the base.",
                                                 f"- **2026-10-12, `{self.base[:7]}`.** Start."),
        }, date="2026-10-13T12:00:00+00:00")
        self.git("tag", "-d", "v0.1.0")
        self.git("tag", "v0.1.0", older)
        self.base = older
        self.change = self.commit("Rename a thing again", {
            "CHANGES.md": changes("## Unreleased\n\n### 2026-11-01 - Rename a thing\n\nText.\n",
                                  "## v0.1.0 (2026-10-13)\n\n### 2026-10-12 - Start\n\nText.\n"),
        }, date="2026-11-01T12:00:00+00:00")
        code, _, errors = self.run_check("--release", "v0.2.0", "--base", "v0.1.0")
        self.assertEqual(code, 1)
        self.assertIn("2026-10-05 (An entry from before the base.) has no commit id; `git log -S` finds "
                      + older[:7], errors)

    def test_a_missing_base_tag_cannot_complete(self):
        code, _, errors = self.run_check("--release", "v0.2.0", "--base", "v0.0.9")
        self.assertEqual(code, 2)
        self.assertIn("no tag v0.0.9", errors)


if __name__ == "__main__":
    sys.exit(unittest.main())
