"""CHANGES.md's per-commit rule through isolated local Git objects."""

from __future__ import annotations

import contextlib
import importlib.util
import io
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

REPOSITORY = Path(__file__).resolve().parents[2]
SCRIPT = REPOSITORY / "tools/check_changes_rule.py"
SPEC = importlib.util.spec_from_file_location("check_changes_rule", SCRIPT)
checker = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(checker)
HEADER = "engine/core/example.h"


class RuleTests(unittest.TestCase):
    def test_rule_paths_are_the_four_changes_md_names(self):
        for path in (HEADER, "engine/render/gl/deep/nested.h", "engine/CMakeLists.txt",
                     "cmake/settings.cmake", "vcpkg.json"):
            with self.subTest(path=path):
                self.assertTrue(checker.is_rule_path(path))
        for path in ("engine/core/example.cpp", "engine/render/SEAM.md", "samples/minimal/main.h",
                     "cmake/compile_shaders.cmake", "tests/CMakeLists.txt", "website/vcpkg.json",
                     "engine/core/example.hpp"):
            with self.subTest(path=path):
                self.assertFalse(checker.is_rule_path(path))

    def test_the_declaration_tolerates_case_and_a_wrapped_line_only(self):
        for message in ("Fix\n\nnot consumer-visible", "Not consumer-visible: comments.",
                        "only, not\nconsumer-visible.", "NOT  CONSUMER-VISIBLE"):
            with self.subTest(message=message):
                self.assertTrue(checker.declares(message))
        for message in ("consumer-visible", "not consumer visible", "notconsumer-visible",
                        "nothing consumer-visible here"):
            with self.subTest(message=message):
                self.assertFalse(checker.declares(message))

    def test_exceptions_are_read_from_changes_md_audit_sentences(self):
        changes = ("every one that has no entry below says `not consumer-visible` in its message\n"
                   "except `b7ad202`, which splits ...\n\n"
                   "every one that has no entry below says `not consumer-visible` in its message\n"
                   "except `0c15f34`, which changes a comment\n"
                   "A commit `deadbeef` mentioned elsewhere is no exception.\n")
        self.assertEqual(checker.recorded_exceptions(changes), {"b7ad202", "0c15f34"})
        self.assertTrue(checker.is_exception("0c15f34a7d2a6c16850e0d9dd67374d1bdd4226c", {"0c15f34"}))
        self.assertFalse(checker.is_exception("10c15f34" + "0" * 32, {"0c15f34"}))


class GitFixtureTests(unittest.TestCase):
    def setUp(self):
        if not shutil.which("git"):
            self.skipTest("Git is required for isolated commit fixtures")
        temporary = tempfile.TemporaryDirectory(prefix="labrador-changes-rule-")
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        self.repository = root / "repository"
        self.repository.mkdir()
        hooks = root / "empty-hooks"
        hooks.mkdir()
        config = root / "empty-gitconfig"
        config.write_text("", encoding="ascii")
        environment = {name: value for name, value in os.environ.items() if not name.startswith("GIT_")}
        environment.update({"GIT_CONFIG_NOSYSTEM": "1", "GIT_CONFIG_GLOBAL": str(config),
                            "GIT_AUTHOR_NAME": "Rule fixture", "GIT_AUTHOR_EMAIL": "fixture@example.invalid",
                            "GIT_COMMITTER_NAME": "Rule fixture", "GIT_COMMITTER_EMAIL": "fixture@example.invalid",
                            "GIT_TERMINAL_PROMPT": "0", "GIT_TEMPLATE_DIR": str(hooks)})
        isolation = mock.patch.dict(os.environ, environment, clear=True)
        isolation.start()
        self.addCleanup(isolation.stop)
        self.git("init", "-q", "--initial-branch=fixture")
        self.git("config", "core.autocrlf", "false")
        self.git("config", "core.hooksPath", str(hooks))
        self.git("config", "commit.gpgsign", "false")
        self.base = self.commit("Initial fixture", {"README.md": b"fixture\n", "CHANGES.md": b"# Changes\n"})

    def git(self, *arguments, data=None):
        result = subprocess.run(["git", "--no-pager", "-C", str(self.repository), *arguments],
                                input=data, capture_output=True, timeout=30, check=False)
        if result.returncode:
            self.fail(result.stderr.decode("utf-8", errors="replace"))
        return result.stdout

    def commit(self, message, files=None):
        for relative, contents in (files or {}).items():
            target = self.repository / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(contents)
        self.git("add", "--all")
        self.git("commit", "-q", "--allow-empty", "--no-verify", "--no-gpg-sign", "--cleanup=verbatim", "--file=-",
                 data=message.encode("utf-8"))
        return self.git("rev-parse", "HEAD").decode("ascii").strip()

    def run_checker(self, *arguments):
        output, errors = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors):
            result = checker.main(list(arguments), repository=self.repository)
        return result, output.getvalue(), errors.getvalue()

    def test_a_commit_outside_the_rule_needs_nothing(self):
        head = self.commit("Ordinary code", {"engine/core/example.cpp": b"source\n", "docs/a.md": b"a\n"})
        code, output, errors = self.run_checker("--commit", head)
        self.assertEqual((code, errors), (0, ""))
        self.assertIn("0 of 1 commits", output)

    def test_an_undeclared_header_change_fails_naming_commit_and_path(self):
        head = self.commit("Rename a thing", {HEADER: b"#pragma once\n"})
        code, _, errors = self.run_checker("--commit", head)
        self.assertEqual(code, 1)
        self.assertIn(head, errors)
        self.assertIn("Rename a thing", errors)
        self.assertIn(HEADER, errors)

    def test_an_entry_or_a_declaration_satisfies_the_rule(self):
        entry = self.commit("With an entry", {HEADER: b"one\n", "CHANGES.md": b"# Changes\n\nentry\n"})
        declared = self.commit("Comment only\n\nNot consumer-visible: a comment.", {HEADER: b"two\n"})
        settings = self.commit("Flags\n\nnot\nconsumer-visible", {"cmake/settings.cmake": b"x\n"})
        code, output, errors = self.run_checker("--base", self.base, "--head", settings)
        self.assertEqual((code, errors), (0, ""), (entry, declared))
        self.assertIn("3 of 3 commits", output)

    def test_each_commit_answers_for_itself_in_a_range(self):
        forgotten = self.commit("Forgotten", {"vcpkg.json": b"{}\n"})
        head = self.commit("Later entry\n\nnot consumer-visible", {"CHANGES.md": b"# Changes\n\nlate\n"})
        code, _, errors = self.run_checker("--base", self.base, "--head", head)
        self.assertEqual(code, 1)
        self.assertIn(forgotten, errors)
        self.assertNotIn(head, errors)

    def test_an_exception_counts_only_once_changes_md_at_the_head_records_it(self):
        forgotten = self.commit("Forgotten", {"engine/CMakeLists.txt": b"# comment\n"})
        self.assertEqual(self.run_checker("--commit", forgotten)[0], 1)
        recorded = ("# Changes\n\nevery one that has no entry below says `not consumer-visible` in its message\n"
                    f"except `{forgotten[:7]}`, which changes a comment.\n")
        head = self.commit("Record the exception", {"CHANGES.md": recorded.encode("utf-8")})
        code, output, errors = self.run_checker("--base", self.base, "--head", head)
        self.assertEqual((code, errors), (0, ""))
        self.assertIn("1 of 2 commits", output)

    def test_a_merge_commit_is_refused_rather_than_read_as_empty(self):
        self.git("checkout", "-q", "-b", "side")
        side = self.commit("Side", {"side.txt": b"side\n"})
        self.git("checkout", "-q", "fixture")
        self.commit("Main", {"main.txt": b"main\n"})
        self.git("merge", "-q", "--no-ff", "--no-edit", side)
        merge = self.git("rev-parse", "HEAD").decode("ascii").strip()
        code, _, errors = self.run_checker("--commit", merge)
        self.assertEqual(code, 2)
        self.assertIn("merge commit", errors)


class RepositoryHistoryTests(unittest.TestCase):
    def test_this_repository_meets_the_rule_since_its_first_release(self):
        if not shutil.which("git"):
            self.skipTest("Git is required to read this repository's history")
        listed = subprocess.run(["git", "-C", str(REPOSITORY), "rev-parse", "--verify", "--quiet", "v0.1.0^{commit}"],
                                capture_output=True, check=False)
        shallow = subprocess.run(["git", "-C", str(REPOSITORY), "rev-parse", "--is-shallow-repository"],
                                 capture_output=True, text=True, check=False)
        if listed.returncode or shallow.stdout.strip() != "false":
            self.skipTest("needs full local history with the v0.1.0 tag")
        parents = subprocess.run(["git", "-C", str(REPOSITORY), "rev-list", "--parents", "-n", "1", "HEAD"],
                                 capture_output=True, text=True, check=False).stdout.split()
        if len(parents) > 2:
            # A pull_request checkout is GitHub's merge of the head into its
            # base; the lint step checks the pull request's own commits.
            self.skipTest("HEAD is a merge commit, which the checker refuses")
        output, errors = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors):
            result = checker.main(["--base", "v0.1.0", "--head", "HEAD"], repository=REPOSITORY)
        self.assertEqual(result, 0, errors.getvalue())


if __name__ == "__main__":
    sys.exit(unittest.main())
