"""Golden review declarations through isolated local Git objects, never images."""

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
SCRIPT = REPOSITORY / "tools/check_golden_reviews.py"
SPEC = importlib.util.spec_from_file_location("check_golden_reviews", SCRIPT)
checker = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(checker)
GOLDEN = "tests/render/golden/example.png"
OTHER = "tests/render/golden/other.png"


class DeclarationTests(unittest.TestCase):
    def test_only_exact_lines_count_and_path_spaces_are_preserved(self):
        message = (f"Golden-Reviewed: {GOLDEN}\r\n"
                   "Golden-Reviewed: tests/render/golden/with spaces.png\n"
                   f" Golden-Reviewed: {OTHER}\n"
                   f"golden-reviewed: {OTHER}\n"
                   f"The image {OTHER} was discussed.\n")
        self.assertEqual(checker.review_declarations(message),
                         {GOLDEN, "tests/render/golden/with spaces.png"})

    def test_duplicate_lines_do_not_hide_another_missing_path(self):
        self.assertEqual(checker.review_declarations(
            f"Golden-Reviewed: {GOLDEN}\nGolden-Reviewed: {GOLDEN}\n"), {GOLDEN})

    def test_nul_paths_do_not_depend_on_git_quoting(self):
        spaced = "tests/render/golden/with spaces.png"
        quoted = 'tests/render/golden/with"quote.png'
        unicode_name = "tests/render/golden/\u00e9.png"
        output = "\0".join([spaced, quoted, unicode_name, "tests/render/golden/notes.md", ""]).encode("utf-8")
        with mock.patch.object(checker, "git", return_value=output) as git:
            paths = checker.changed_goldens(Path("fixture"), "a" * 40, ["b" * 40])
        self.assertEqual(paths, {spaced, quoted, unicode_name})
        arguments = git.call_args.args[1:]
        for option in ("--no-renames", "--no-ext-diff", "--no-textconv", "-z"):
            self.assertIn(option, arguments)

    def test_a_path_with_a_line_break_fails_instead_of_truncating(self):
        with mock.patch.object(checker, "git", return_value=b"tests/render/golden/a\nb.png\0"):
            with self.assertRaisesRegex(checker.CheckError, "cannot be represented by one review line"):
                checker.changed_goldens(Path("fixture"), "a" * 40, [])

    def test_git_reads_use_argv_without_a_shell_and_disable_pagers(self):
        result = subprocess.CompletedProcess([], 0, b"read", b"")
        with mock.patch.object(checker.subprocess, "run", return_value=result) as run:
            self.assertEqual(checker.git(Path("path with spaces"), "cat-file", "commit", "a" * 40), b"read")
        self.assertEqual(run.call_args.args[0][:6],
                         ["git", "--no-pager", "--no-lazy-fetch", "--no-replace-objects",
                          "-C", "path with spaces"])
        self.assertNotIn("shell", run.call_args.kwargs)
        self.assertEqual(run.call_args.kwargs["env"]["GIT_OPTIONAL_LOCKS"], "0")
        self.assertEqual(run.call_args.kwargs["env"]["GIT_TERMINAL_PROMPT"], "0")

    def test_git_failure_and_missing_executable_are_actionable(self):
        result = subprocess.CompletedProcess([], 128, b"", b"missing object")
        with mock.patch.object(checker.subprocess, "run", return_value=result):
            with self.assertRaisesRegex(checker.CheckError, "git cat-file failed: missing object"):
                checker.git(Path("fixture"), "cat-file", "commit", "missing")
        with mock.patch.object(checker.subprocess, "run", side_effect=FileNotFoundError("git unavailable")):
            with self.assertRaisesRegex(checker.CheckError, "cannot read local Git history"):
                checker.git(Path("fixture"), "rev-parse", "HEAD")

    def test_revision_operands_are_resolved_after_end_of_options(self):
        revision = "topic; this is not a shell command"
        with mock.patch.object(checker, "git", return_value=("a" * 40 + "\n").encode()) as git:
            self.assertEqual(checker.resolve_commit(Path("fixture"), revision), "a" * 40)
        self.assertEqual(git.call_args.args[1:],
                         ("rev-parse", "--verify", "--end-of-options", revision + "^{commit}"))
        with mock.patch.object(checker, "git") as git:
            with self.assertRaises(checker.CheckError):
                checker.resolve_commit(Path("fixture"), "--help")
            git.assert_not_called()

    def test_shallow_history_is_explicitly_incomplete_without_fetch(self):
        with mock.patch.object(checker, "git", return_value=b"true\n") as git:
            with self.assertRaisesRegex(checker.CheckError, "shallow history.*no fetch attempted"):
                checker.scope_commits(Path("fixture"), commit="HEAD")
        self.assertEqual(git.call_count, 1)


class GitFixtureTests(unittest.TestCase):
    def setUp(self):
        if not shutil.which("git"):
            self.skipTest("Git is required for isolated commit fixtures")
        temporary = tempfile.TemporaryDirectory(prefix="labrador-golden-review-")
        self.addCleanup(temporary.cleanup)
        self.temporary = Path(temporary.name)
        self.repository = self.temporary / "repository"
        self.repository.mkdir()
        hooks = self.temporary / "empty-hooks"
        hooks.mkdir()
        config = self.temporary / "empty-gitconfig"
        config.write_text("", encoding="ascii")
        environment = {name: value for name, value in os.environ.items() if not name.startswith("GIT_")}
        environment.update({"GIT_CONFIG_NOSYSTEM": "1", "GIT_CONFIG_GLOBAL": str(config),
                            "GIT_AUTHOR_NAME": "Golden fixture", "GIT_AUTHOR_EMAIL": "fixture@example.invalid",
                            "GIT_COMMITTER_NAME": "Golden fixture", "GIT_COMMITTER_EMAIL": "fixture@example.invalid",
                            "GIT_TERMINAL_PROMPT": "0", "GIT_TEMPLATE_DIR": str(hooks)})
        isolation = mock.patch.dict(os.environ, environment, clear=True)
        isolation.start()
        self.addCleanup(isolation.stop)
        self.hooks = hooks
        self.git("init", "-q", "--initial-branch=fixture")
        self.git("config", "core.autocrlf", "false")
        self.git("config", "core.hooksPath", str(hooks))
        self.git("config", "commit.gpgsign", "false")
        self.base = self.commit("Initial fixture", {"README.md": b"fixture\n"})

    def git(self, *arguments, data=None):
        result = subprocess.run(["git", "--no-pager", "-C", str(self.repository), *arguments],
                                input=data, capture_output=True, timeout=30, check=False)
        if result.returncode:
            self.fail(result.stderr.decode("utf-8", errors="replace"))
        return result.stdout

    def commit(self, message, files=None, remove=()):
        for relative, contents in (files or {}).items():
            target = self.repository / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(contents)
        for relative in remove:
            (self.repository / relative).unlink()
        self.git("add", "--all")
        self.git("commit", "-q", "--allow-empty", "--no-verify", "--no-gpg-sign", "--cleanup=verbatim", "--file=-",
                 data=message.encode("utf-8"))
        return self.git("rev-parse", "HEAD").decode("ascii").strip()

    def run_checker(self, *arguments):
        output, errors = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors):
            result = checker.main(list(arguments), repository=self.repository)
        return result, output.getvalue(), errors.getvalue()

    def test_no_golden_changes_need_no_declarations(self):
        head = self.commit("Ordinary code", {"source.cpp": b"source\n", "elsewhere/image.png": b"fake image"})
        code, output, errors = self.run_checker("--base", self.base, "--head", head)
        self.assertEqual((code, errors), (0, ""))
        self.assertIn("0 changed golden paths across 1 commits", output)
        self.assertIn("not visual review", output)

    def test_missing_paths_are_named_per_committing_message(self):
        head = self.commit("Unreviewed", {GOLDEN: b"fake PNG one", OTHER: b"fake PNG two"})
        code, _, errors = self.run_checker("--commit", head)
        self.assertEqual(code, 1)
        self.assertIn(head, errors)
        self.assertIn(repr(GOLDEN), errors)
        self.assertIn(repr(OTHER), errors)

    def test_each_exact_line_covers_its_changed_path(self):
        head = self.commit(f"Reviewed\n\nGolden-Reviewed: {GOLDEN}\nGolden-Reviewed: {OTHER}\n",
                           {GOLDEN: b"fake PNG one", OTHER: b"fake PNG two"})
        code, output, errors = self.run_checker("--commit", head)
        self.assertEqual((code, errors), (0, ""))
        self.assertIn("2 changed golden paths", output)
        self.assertIn("ancestors are not checked", output)

    def test_replacement_refs_cannot_substitute_a_reviewed_message(self):
        original = self.commit("Unreviewed", {GOLDEN: b"fake PNG"})
        tree = self.git("rev-parse", original + "^{tree}").decode("ascii").strip()
        replacement = self.git("commit-tree", tree, "-p", self.base,
                               data=f"Replacement\n\nGolden-Reviewed: {GOLDEN}\n".encode())
        replacement = replacement.decode("ascii").strip()
        self.git("replace", original, replacement)
        # The fixture's ordinary view can see the substitute; the checker
        # must still inspect the actual commit that introduced the image.
        self.assertIn(b"Golden-Reviewed: ", self.git("cat-file", "commit", original))
        code, _, errors = self.run_checker("--commit", original)
        self.assertEqual(code, 1)
        self.assertIn(original, errors)
        self.assertIn(repr(GOLDEN), errors)

    def test_incidental_partial_glob_and_padded_lines_do_not_cover_a_path(self):
        messages = [f"Looked at {GOLDEN}", "Golden-Reviewed: example.png",
                    "Golden-Reviewed: tests/render/golden/*.png", f" Golden-Reviewed: {GOLDEN}",
                    f"Golden-Reviewed: {GOLDEN} extra", f"Golden-Reviewed: {GOLDEN} "]
        for message in messages:
            with self.subTest(message=message):
                head = self.commit(message, {GOLDEN: message.encode("utf-8")})
                self.assertEqual(self.run_checker("--commit", head)[0], 1)

    def test_later_aggregate_message_cannot_backfill_an_earlier_commit(self):
        unreviewed = self.commit("Forgotten", {GOLDEN: b"first"})
        head = self.commit(f"Later explanation\n\nGolden-Reviewed: {GOLDEN}")
        code, _, errors = self.run_checker("--base", self.base, "--head", head)
        self.assertEqual(code, 1)
        self.assertIn(unreviewed, errors)
        self.assertNotIn(head, errors)

    def test_the_same_image_changed_twice_needs_both_commits_declarations(self):
        first = self.commit(f"First\n\nGolden-Reviewed: {GOLDEN}", {GOLDEN: b"first"})
        head = self.commit("Second lacks its line", {GOLDEN: b"second"})
        code, _, errors = self.run_checker("--base", self.base, "--head", head)
        self.assertEqual(code, 1)
        self.assertIn(head, errors)
        self.assertNotIn(first, errors)

    def test_deletion_still_requires_the_old_path(self):
        self.commit(f"Add\n\nGolden-Reviewed: {GOLDEN}", {GOLDEN: b"fake PNG"})
        head = self.commit("Remove", remove=[GOLDEN])
        self.assertEqual(self.run_checker("--commit", head)[0], 1)
        reviewed = self.commit(f"Re-add\n\nGolden-Reviewed: {GOLDEN}", {GOLDEN: b"fake PNG"})
        removed = self.commit(f"Remove after review\n\nGolden-Reviewed: {GOLDEN}", remove=[GOLDEN])
        self.assertEqual(self.run_checker("--base", reviewed, "--head", removed)[0], 0)

    def test_rename_is_a_removal_and_addition_not_an_inferred_review(self):
        self.commit(f"Add\n\nGolden-Reviewed: {GOLDEN}", {GOLDEN: b"same fake PNG"})
        head = self.commit(f"Move\n\nGolden-Reviewed: {OTHER}", {OTHER: b"same fake PNG"}, remove=[GOLDEN])
        code, _, errors = self.run_checker("--commit", head)
        self.assertEqual(code, 1)
        self.assertIn(repr(GOLDEN), errors)
        self.assertNotIn(repr(OTHER), errors)

    def test_a_filename_with_spaces_is_matched_without_quoting(self):
        path = "tests/render/golden/a frame with spaces.png"
        head = self.commit(f"Reviewed\n\nGolden-Reviewed: {path}", {path: b"fake PNG"})
        self.assertEqual(self.run_checker("--commit", head)[0], 0)

    def test_moves_out_of_and_into_the_directory_require_only_the_protected_path(self):
        outside = "other-images/example.png"
        self.commit(f"Add\n\nGolden-Reviewed: {GOLDEN}", {GOLDEN: b"same fake PNG"})
        moved_out = self.commit(f"Move out\n\nGolden-Reviewed: {GOLDEN}",
                                {outside: b"same fake PNG"}, remove=[GOLDEN])
        self.assertEqual(self.run_checker("--commit", moved_out)[0], 0)
        moved_in = self.commit(f"Move in names only outside\n\nGolden-Reviewed: {outside}",
                               {GOLDEN: b"same fake PNG"}, remove=[outside])
        code, _, errors = self.run_checker("--commit", moved_in)
        self.assertEqual(code, 1)
        self.assertIn(repr(GOLDEN), errors)
        self.assertNotIn(repr(outside), errors)

    def test_root_commit_includes_its_added_goldens(self):
        root = self.temporary / "root-repository"
        root.mkdir()
        self.git("init", "-q", "--initial-branch=fixture", str(root))
        original = self.repository
        self.repository = root
        try:
            head = self.commit("Root without review", {GOLDEN: b"root fake PNG"})
            self.assertEqual(self.run_checker("--commit", head)[0], 1)
            self.git("commit", "--amend", "-q", "--no-verify", "--no-gpg-sign", "--file=-",
                     data=f"Root reviewed\n\nGolden-Reviewed: {GOLDEN}".encode())
            self.assertEqual(self.run_checker("--commit", "HEAD")[0], 0)
        finally:
            self.repository = original

    def test_merge_commit_fails_explicitly_even_without_golden_changes(self):
        self.git("checkout", "-q", "-b", "side")
        self.commit("Side", {"side.txt": b"side"})
        self.git("checkout", "-q", "fixture")
        self.commit("Main", {"main.txt": b"main"})
        self.git("merge", "-q", "--no-ff", "--no-verify", "--no-gpg-sign", "-m", "Fixture merge", "side")
        code, _, errors = self.run_checker("--commit", "HEAD")
        self.assertEqual(code, 2)
        self.assertIn("merge commit", errors)
        self.assertIn("not treated as empty", errors)

    def test_missing_history_is_not_replaced_with_a_head_only_check(self):
        head = self.commit("Ordinary", {"other.txt": b"other"})
        code, output, errors = self.run_checker("--base", "f" * 40, "--head", head)
        self.assertEqual(code, 2)
        self.assertIn("could not complete", errors)
        self.assertNotIn("Declarations cover", output)

    def test_empty_range_is_reported_as_zero_commits(self):
        code, output, errors = self.run_checker("--base", self.base, "--head", self.base)
        self.assertEqual((code, errors), (0, ""))
        self.assertIn("across 0 commits", output)

    def test_real_cli_runs_in_the_supplied_fixture_directory(self):
        head = self.commit(f"Reviewed\n\nGolden-Reviewed: {GOLDEN}", {GOLDEN: b"fake PNG"})
        result = subprocess.run([sys.executable, str(SCRIPT), "--commit", head],
                                cwd=self.repository, capture_output=True, text=True, timeout=30, check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("ancestors are not checked", result.stdout)

    def test_real_cli_from_a_nested_directory_cannot_hide_changed_goldens(self):
        head = self.commit("Missing declaration", {GOLDEN: b"fake PNG"})
        nested = self.repository / "tools"
        nested.mkdir()
        result = subprocess.run([sys.executable, str(SCRIPT), "--commit", head],
                                cwd=nested, capture_output=True, text=True, timeout=30, check=False)
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn(repr(GOLDEN), result.stderr)


class CliAndWorkflowTests(unittest.TestCase):
    def test_invalid_scope_combinations_fail_before_git(self):
        for arguments in ([], ["--base", "HEAD"], ["--head", "HEAD"],
                          ["--commit", "HEAD", "--head", "HEAD"],
                          ["--commit", "HEAD", "--base", "HEAD", "--head", "HEAD"]):
            with self.subTest(arguments=arguments), contextlib.redirect_stderr(io.StringIO()):
                with mock.patch.object(checker, "git") as git:
                    with self.assertRaises(SystemExit) as error:
                        checker.main(arguments)
                    self.assertEqual(error.exception.code, 2)
                    git.assert_not_called()

    def test_existing_lint_job_uses_full_history_and_event_scope_arguments(self):
        workflow = (REPOSITORY / ".github/workflows/ci.yml").read_text(encoding="utf-8")
        lint = workflow.split("\n  lint:\n", 1)[1].split("\n  windows:\n", 1)[0]
        self.assertIn("uses: actions/checkout@v4\n        with:\n          fetch-depth: 0", lint)
        self.assertIn("PR_BASE: ${{ github.event.pull_request.base.sha }}", lint)
        self.assertIn("PR_HEAD: ${{ github.event.pull_request.head.sha }}", lint)
        self.assertIn('--base "$env:PR_BASE" --head "$env:PR_HEAD"', lint)
        self.assertIn('--base "$env:EVENT_BEFORE" --head "$env:EVENT_HEAD"', lint)
        self.assertIn("$env:EVENT_BEFORE -notmatch '^0+$'", lint)
        self.assertIn("@('schedule', 'workflow_dispatch')", lint)
        self.assertIn("$env:EVENT_BEFORE -match '^0+$'", lint)
        self.assertIn('--commit "$env:EVENT_HEAD"', lint)
        self.assertIn("not ancestor/new-branch history", lint)
        self.assertIn("refusing to guess", lint)


if __name__ == "__main__":
    unittest.main()
