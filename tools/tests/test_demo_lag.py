"""Metadata/command checks and isolated local Git histories; no real host is read."""

from __future__ import annotations

import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

REPOSITORY = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("check_demo_lag", REPOSITORY / "tools/check_demo_lag.py")
checker = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(checker)


class ContractTests(unittest.TestCase):
    def test_metadata_identity_is_independent_of_bundle_integrity_verification(self):
        self.assertEqual(checker.build_commit(json.dumps({
            "commit": "a" * 40, "dirty": False, "build_type": "Debug"}).encode()), "a" * 40)

    def test_invalid_json_identity_and_dirty_states_fail(self):
        documents = [b"{", b"\xff", b"[]", b"null", b"42"]
        for commit in (None, "unknown", "abcdef0", "A" * 40, "a" * 64, "--help", 3):
            documents.append(json.dumps({"commit": commit, "dirty": False}).encode())
        for dirty in (None, True, 0, "false", []):
            documents.append(json.dumps({"commit": "a" * 40, "dirty": dirty}).encode())
        documents.extend([b'{"dirty":false}', ('{"commit":"' + "a" * 40 + '"}').encode()])
        for contents in documents:
            with self.subTest(contents=contents), self.assertRaises(checker.CheckError):
                checker.build_commit(contents)

    def test_duplicate_keys_and_non_json_constants_are_rejected(self):
        for contents in (
            '{"commit":"' + "a" * 40 + '","commit":"' + "b" * 40 + '","dirty":false}',
            '{"commit":"' + "a" * 40 + '","dirty":true,"dirty":false}',
            *('{"commit":"' + "a" * 40 + '","dirty":false,"extra":' + value + '}'
              for value in ("NaN", "Infinity", "-Infinity")),
        ):
            with self.subTest(contents=contents), self.assertRaises(checker.CheckError):
                checker.build_commit(contents.encode())

    def test_deep_and_oversized_json_parser_errors_have_the_cli_error_contract(self):
        prefix = ('{"commit":"' + "a" * 40 + '","dirty":false,"extra":').encode()
        limit = sys.get_int_max_str_digits()
        if limit:
            with self.assertRaisesRegex(checker.CheckError, "invalid UTF-8 JSON"):
                checker.build_commit(prefix + b"9" * (limit + 1) + b"}")
        # Decoder depth limits differ across Python builds; inject its documented
        # exception rather than assuming a nesting threshold or risking a stack.
        for error in (ValueError("integer digit limit"), RecursionError("JSON nesting limit")):
            with self.subTest(error=error), \
                    mock.patch.object(checker, "checkout_root", side_effect=[Path("engine"), Path("host")]), \
                    mock.patch.object(checker, "resolve_commit", side_effect=["b" * 40, "c" * 40]), \
                    mock.patch.object(checker, "git", return_value=subprocess.CompletedProcess([], 0, b"{}", b"")), \
                    mock.patch.object(checker.json, "loads", side_effect=error):
                output, errors = io.StringIO(), io.StringIO()
                with contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors):
                    self.assertEqual(checker.main(["--host-repository", "host"]), 2)
                self.assertEqual(output.getvalue(), "")
                self.assertIn("committed BUILD.json is invalid UTF-8 JSON", errors.getvalue())

    def test_git_has_no_shell_fetch_replacements_locks_or_inherited_routing(self):
        environment = {"PATH": "fixture-path", "GIT_DIR": "wrong", "git_work_tree": "wrong",
                       "GIT_OBJECT_DIRECTORY": "wrong", "GIT_ALTERNATE_OBJECT_DIRECTORIES": "wrong",
                       "GIT_CONFIG_COUNT": "1", "GIT_CONFIG_KEY_0": "core.worktree",
                       "GIT_CONFIG_VALUE_0": "wrong", "GIT_NAMESPACE": "wrong", "GIT_GRAFT_FILE": "wrong"}
        reply = subprocess.CompletedProcess([], 0, b"blob", b"")
        with mock.patch.object(checker.os, "environ", environment), \
                mock.patch.object(checker.subprocess, "run", return_value=reply) as run:
            checker.git(Path("path with spaces"), "cat-file", "blob", "a" * 40)
        self.assertEqual(run.call_args.args[0], ["git", "--no-pager", "--no-optional-locks",
                         "--no-lazy-fetch", "--no-replace-objects", "-C", "path with spaces",
                         "cat-file", "blob", "a" * 40])
        self.assertFalse(run.call_args.kwargs["shell"])
        actual = run.call_args.kwargs["env"]
        self.assertEqual(actual["PATH"], "fixture-path")
        self.assertEqual({name for name in actual if name.upper().startswith("GIT_")},
                         {"GIT_CONFIG_NOSYSTEM", "GIT_CONFIG_GLOBAL", "GIT_GRAFT_FILE", "GIT_TERMINAL_PROMPT"})
        self.assertEqual(actual["GIT_CONFIG_GLOBAL"], os.devnull)
        self.assertEqual(actual["GIT_GRAFT_FILE"], os.devnull)

    def test_git_errors_and_unavailable_processes_fail(self):
        for outcome in (subprocess.CompletedProcess([], 128, b"", b"missing local object"),
                        FileNotFoundError("git missing"), subprocess.TimeoutExpired("git", 30)):
            with self.subTest(outcome=outcome):
                options = {"side_effect": outcome} if isinstance(outcome, Exception) else {"return_value": outcome}
                with mock.patch.object(checker.subprocess, "run", **options), \
                        self.assertRaises(checker.CheckError):
                    checker.git(Path("fixture"), "rev-parse", "HEAD")

    def test_revision_operands_are_not_options_and_count_output_is_strict(self):
        with mock.patch.object(checker, "git") as git:
            for revision in ("", "--help"):
                with self.assertRaises(checker.CheckError):
                    checker.resolve_commit(Path("fixture"), revision)
                with self.assertRaises(checker.CheckError):
                    checker.compare(Path("fixture"), engine_ref=revision)
            git.assert_not_called()
        with mock.patch.object(checker, "git", return_value=subprocess.CompletedProcess([], 0, b"a" * 40, b"")) as git:
            checker.resolve_commit(Path("fixture"), "name; literal argv")
        self.assertEqual(git.call_args.args[1:],
                         ("rev-parse", "--verify", "--end-of-options", "name; literal argv^{commit}"))
        for output in (b"", b"-1\n", b"0 trailing\n", b"01", b"2\n3"):
            with self.subTest(output=output), mock.patch.object(checker, "git", return_value=
                    subprocess.CompletedProcess([], 0, output, b"")), self.assertRaises(checker.CheckError):
                checker.commit_count(Path("fixture"), "a" * 40)

    def test_both_tips_are_pinned_before_blob_and_graph_reads(self):
        engine, host = Path("engine"), Path("host")
        host_tip, engine_tip, build = "a" * 40, "b" * 40, "c" * 40
        replies = [json.dumps({"commit": build, "dirty": False}).encode(), b"false\n", b"8\n", b"", b"3\n"]
        with mock.patch.object(checker, "checkout_root", side_effect=[engine, host]), \
                mock.patch.object(checker, "resolve_commit", side_effect=[host_tip, engine_tip, build]) as resolve, \
                mock.patch.object(checker, "git", side_effect=[
                    subprocess.CompletedProcess([], 0, value, b"") for value in replies]) as git:
            result = checker.compare(host, engine_repository=engine)
        self.assertEqual(resolve.call_args_list, [mock.call(host, "HEAD"),
                         mock.call(engine, checker.MASTER), mock.call(engine, build)])
        self.assertEqual(git.call_args_list[0], mock.call(host, "cat-file", "blob", f"{host_tip}:{checker.HOST_BUILD}"))
        self.assertEqual(git.call_args_list[-1], mock.call(engine, "rev-list", "--count", f"{build}..{engine_tip}", "--"))
        self.assertEqual(result["behind"], 3)

    def test_cli_requires_explicit_host_without_reading_git(self):
        with mock.patch.object(checker, "git") as git, contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(checker.main([]), 2)
        git.assert_not_called()


class LocalHistoryTests(unittest.TestCase):
    def setUp(self):
        if not shutil.which("git"):
            self.skipTest("Git is required for isolated local-history fixtures")
        temporary = tempfile.TemporaryDirectory(prefix="labrador-demo-lag-")
        self.addCleanup(temporary.cleanup)
        self.folder = Path(temporary.name).resolve()
        self.engine = self.folder / "engine with spaces"
        self.host = self.folder / "host with spaces"
        hooks = self.folder / "empty-hooks"
        hooks.mkdir()
        config = self.folder / "empty-config"
        config.write_text("", encoding="utf-8")
        self.environment = {name: value for name, value in os.environ.items()
                            if not name.upper().startswith("GIT_")}
        self.environment.update({"GIT_CONFIG_NOSYSTEM": "1", "GIT_CONFIG_GLOBAL": str(config),
                                 "GIT_AUTHOR_NAME": "Lag fixture", "GIT_AUTHOR_EMAIL": "fixture@example.invalid",
                                 "GIT_COMMITTER_NAME": "Lag fixture", "GIT_COMMITTER_EMAIL": "fixture@example.invalid"})
        for repository in (self.engine, self.host):
            repository.mkdir()
            self.git(repository, "init", "-q", "-b", "master", "--object-format=sha1")
            self.git(repository, "config", "core.hooksPath", str(hooks))
            self.git(repository, "config", "commit.gpgsign", "false")
            self.git(repository, "config", "gc.auto", "0")
        self.base = self.commit(self.engine, "source.txt", "first\n")
        self.tip = self.commit(self.engine, "source.txt", "second\n")
        self.host_tip = self.record(self.base)

    def git(self, repository, *arguments, input=None):
        result = subprocess.run(["git", "--no-pager", "--no-lazy-fetch", "--no-replace-objects",
                                 "-C", str(repository), *arguments],
                                input=input, capture_output=True, env=self.environment, timeout=20, check=True)
        return result.stdout.decode("utf-8").strip()

    def commit(self, repository, name, contents):
        path = repository / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(contents, encoding="utf-8")
        self.git(repository, "add", "--", name)
        self.git(repository, "commit", "-q", "-m", "Isolated fixture")
        return self.git(repository, "rev-parse", "HEAD")

    def record(self, commit, dirty=False):
        return self.commit(self.host, checker.HOST_BUILD,
                           json.dumps({"commit": commit, "dirty": dirty}) + "\n")

    def compare(self, **options):
        return checker.compare(self.host, engine_repository=self.engine, **options)

    def invoke(self, arguments=()):
        output, errors = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors):
            code = checker.main(["--host-repository", str(self.host), *arguments], engine_repository=self.engine)
        return code, output.getvalue(), errors.getvalue()

    def test_ancestor_and_exact_tip_counts_are_informational(self):
        result = self.compare()
        self.assertEqual((result["host_commit"], result["bundle_commit"], result["engine_commit"], result["behind"]),
                         (self.host_tip, self.base, self.tip, 1))
        code, output, errors = self.invoke()
        self.assertEqual(code, 0, errors)
        for text in (str(self.host), self.host_tip, checker.HOST_BUILD, self.base, self.tip,
                     "local master refs/heads/master", "Behind comparison: 1 commits", "Live responses"):
            self.assertIn(text, output)
        self.record(self.tip)
        self.assertEqual(self.compare()["behind"], 0)

    def test_committed_blob_ignores_uncommitted_working_copy(self):
        (self.host / checker.HOST_BUILD).write_text("uncommitted invalid JSON", encoding="utf-8")
        self.assertEqual(self.compare()["bundle_commit"], self.base)

    def test_dirty_committed_metadata_is_not_a_clean_revision(self):
        self.record(self.base, dirty=True)
        code, output, errors = self.invoke()
        self.assertEqual(code, 2)
        self.assertEqual(output, "")
        self.assertIn("dirty must be exactly false", errors)

    def test_missing_blob_and_unknown_build_object_fail(self):
        self.git(self.host, "rm", "--", checker.HOST_BUILD)
        self.git(self.host, "commit", "-q", "-m", "Remove fixture metadata")
        with self.assertRaisesRegex(checker.CheckError, "git cat-file failed"):
            self.compare()
        self.record("f" * 40)
        with self.assertRaisesRegex(checker.CheckError, "git rev-parse failed"):
            self.compare()

    def test_archives_and_nested_inputs_cannot_borrow_an_enclosing_checkout(self):
        for label, engine, host in (("Labrador", self.engine / "archive", self.host),
                                   ("host repository", self.engine, self.host / "public")):
            engine.mkdir(parents=True, exist_ok=True)
            host.mkdir(parents=True, exist_ok=True)
            with self.subTest(label=label), self.assertRaisesRegex(checker.CheckError, label + ".*own .git"):
                checker.compare(host, engine_repository=engine)

    def test_actual_git_top_level_must_match_the_supplied_root(self):
        self.git(self.engine, "config", "core.worktree", str(self.host))
        with self.assertRaisesRegex(checker.CheckError, "top-level does not match"):
            self.compare()

    def test_linked_worktree_marker_file_is_supported(self):
        linked = self.folder / "linked engine"
        self.git(self.engine, "worktree", "add", "--detach", str(linked), self.tip)
        self.assertTrue((linked / ".git").is_file())
        self.assertEqual(checker.compare(self.host, engine_repository=linked)["behind"], 1)

    def test_shallow_history_fails_without_fetching(self):
        (self.engine / ".git/shallow").write_text(self.tip + "\n", encoding="ascii")
        with self.assertRaisesRegex(checker.CheckError, "shallow.*no fetch attempted"):
            self.compare()

    def test_missing_older_ancestor_is_not_hidden_by_a_zero_length_range(self):
        tree = self.git(self.engine, "rev-parse", self.tip + "^{tree}")
        contents = (f"tree {tree}\nparent {'f' * 40}\n"
                    "author Lag fixture <fixture@example.invalid> 1600000000 +0000\n"
                    "committer Lag fixture <fixture@example.invalid> 1600000000 +0000\n\n"
                    "Incomplete fixture history\n").encode()
        broken = self.git(self.engine, "hash-object", "-t", "commit", "-w", "--stdin", input=contents)
        self.git(self.engine, "update-ref", checker.MASTER, broken)
        self.record(broken)
        with self.assertRaisesRegex(checker.CheckError, "git rev-list failed"):
            self.compare()

    def test_ahead_and_diverged_metadata_have_no_fake_behind_count(self):
        self.record(self.tip)
        with self.assertRaisesRegex(checker.CheckError, "not an ancestor.*ahead or diverged"):
            self.compare(engine_ref=self.base)
        tree = self.git(self.engine, "rev-parse", self.tip + "^{tree}")
        side = self.git(self.engine, "commit-tree", tree, "-p", self.base, input=b"Diverged fixture\n")
        self.record(side)
        with self.assertRaisesRegex(checker.CheckError, "not an ancestor.*ahead or diverged"):
            self.compare()

    def test_missing_master_does_not_fall_back_to_head(self):
        self.git(self.engine, "update-ref", "refs/heads/work", self.tip)
        self.git(self.engine, "symbolic-ref", "HEAD", "refs/heads/work")
        self.git(self.engine, "update-ref", "-d", checker.MASTER)
        with self.assertRaisesRegex(checker.CheckError, "git rev-parse failed"):
            self.compare()

    def test_explicit_engine_reference_is_labeled_without_calling_it_master(self):
        code, output, errors = self.invoke(["--engine-ref", self.base])
        self.assertEqual(code, 0, errors)
        self.assertIn(f"Comparison: local engine reference {self.base} @ {self.base}", output)
        self.assertNotIn("local master", output)
        self.assertIn("Behind comparison: 0 commits", output)

    def test_inherited_git_environment_cannot_route_reads_to_the_host(self):
        with mock.patch.dict(os.environ, {"GIT_DIR": str(self.host / ".git"), "GIT_WORK_TREE": str(self.host),
                                        "GIT_NAMESPACE": "wrong", "GIT_CONFIG_COUNT": "1",
                                        "GIT_CONFIG_KEY_0": "core.worktree", "GIT_CONFIG_VALUE_0": str(self.host)}):
            self.assertEqual(self.compare()["behind"], 1)

    def test_replacement_refs_and_legacy_grafts_do_not_rewrite_identity(self):
        self.git(self.engine, "replace", self.base, self.tip)
        (self.engine / ".git/info/grafts").write_text(self.tip + "\n", encoding="ascii")
        self.assertEqual(self.compare()["behind"], 1)

    def test_nested_invocation_anchors_engine_at_script_root(self):
        nested = self.folder / "unrelated working folder"
        nested.mkdir()
        output = io.StringIO()
        with contextlib.chdir(nested), mock.patch.object(checker, "REPOSITORY", self.engine), \
                contextlib.redirect_stdout(output):
            self.assertEqual(checker.main(["--host-repository", str(self.host)]), 0)
        self.assertIn(f"Labrador checkout: {self.engine}", output.getvalue())

    def test_production_invokes_only_local_read_commands_with_pinned_operands(self):
        original = subprocess.run
        commands = []

        def record(command, **options):
            commands.append(command)
            return original(command, **options)

        with mock.patch.object(checker.subprocess, "run", side_effect=record):
            self.compare()
        operations = [command[7] for command in commands]
        self.assertEqual(set(operations), {"rev-parse", "cat-file", "merge-base", "rev-list"})
        self.assertTrue(all(command[:5] == ["git", "--no-pager", "--no-optional-locks",
                                           "--no-lazy-fetch", "--no-replace-objects"] for command in commands))
        self.assertIn(["cat-file", "blob", f"{self.host_tip}:{checker.HOST_BUILD}"],
                      [command[7:] for command in commands])


if __name__ == "__main__":
    unittest.main()
