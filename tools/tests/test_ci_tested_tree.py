"""Exercise the master push check against real Git history and a fake API."""

from __future__ import annotations

import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest
import unittest.mock

from tools import ci_tested_tree as checker


REPOSITORY = Path(__file__).resolve().parents[2]
WORKFLOW = ".github/workflows/ci.yml"
OWNER = "owner/repo"


class History:
    """A clone of an origin holding master and one pull request's head."""

    def __init__(self, root: Path):
        self.origin = root / "origin.git"
        self.work = root / "work"
        subprocess.run(["git", "init", "--quiet", "--bare", "--initial-branch=master",
                        str(self.origin)], check=True)
        subprocess.run(["git", "init", "--quiet", "--initial-branch=master", str(self.work)],
                       check=True)
        self.git("remote", "add", "origin", str(self.origin))
        self.before = self.commit("engine.cpp", "one\n")

    def git(self, *arguments: str) -> str:
        environment = {**os.environ, "GIT_AUTHOR_NAME": "a", "GIT_AUTHOR_EMAIL": "a@a",
                       "GIT_COMMITTER_NAME": "a", "GIT_COMMITTER_EMAIL": "a@a"}
        return subprocess.run(["git", "-C", str(self.work), *arguments], check=True,
                              capture_output=True, text=True, env=environment).stdout

    def commit(self, path: str, text: str) -> str:
        (self.work / path).parent.mkdir(parents=True, exist_ok=True)
        (self.work / path).write_text(text, encoding="utf-8")
        self.git("add", path)
        self.git("commit", "--quiet", "-m", "change " + path)
        return self.git("rev-parse", "HEAD").strip()

    def pull_head(self, number: int, sha: str) -> None:
        self.git("push", "--quiet", "origin", f"+{sha}:refs/pull/{number}/head")

    def rebase_merge(self, pull_head: str) -> str:
        """Master after a rebase merge: the head's tree, under new commit hashes."""
        self.git("checkout", "--quiet", "--detach", self.before)
        self.git("cherry-pick", "--quiet", "--allow-empty", f"{self.before}..{pull_head}")
        self.git("commit", "--quiet", "--amend", "--no-edit", "--allow-empty",
                 "--date", "2000-01-01T00:00:00")
        return self.git("rev-parse", "HEAD").strip()


class FakeApi:
    def __init__(self, pulls, runs, association_delay: int = 0, sharing=None, events=None):
        self.pulls = pulls
        self.runs = runs
        self.association_delay = association_delay
        self.sharing = sharing
        self.events = [{"event": "merged"}, {"event": "closed"}] if events is None else events
        self.calls: list[str] = []

    def __call__(self, path: str):
        self.calls.append(path)
        if "/events" in path:
            return self.events
        if "/pulls" in path:
            if "per_page" in path:  # The pull request head's association.
                return self.pulls if self.sharing is None else self.sharing
            if self.association_delay:
                self.association_delay -= 1
                return []
            return self.pulls
        match = re.search(r"head_sha=([0-9a-f]{40})", path)
        return {"workflow_runs": self.runs.get(match.group(1), [])}


def merged(number: int, base: str = "master"):
    return {"number": number, "merged_at": "2026-10-30T00:00:00Z", "base": {"ref": base}}


def run(conclusion: str, created_at: str = "2026-10-30T00:00:00Z", *, path: str = WORKFLOW,
        event: str = "pull_request", identifier: int = 1):
    return {"path": path, "event": event, "conclusion": conclusion, "created_at": created_at,
            "id": identifier, "html_url": f"https://github.invalid/runs/{identifier}"}


class TestedTreeTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="labrador-tested-tree-")
        self.addCleanup(temporary.cleanup)
        self.history = History(Path(temporary.name))
        self.history.commit("engine.h", "two\n")
        self.history.commit("sample.cpp", "three\n")
        self.pull_head = self.history.git("rev-parse", "HEAD").strip()
        self.history.pull_head(7, self.pull_head)
        self.head = self.history.rebase_merge(self.pull_head)
        self.sleeps: list[float] = []

    def decide(self, api, *, before=None, head=None, forced=False, refresh=()):
        return checker.decide(
            repository=OWNER, workflow_path=WORKFLOW,
            before=self.history.before if before is None else before,
            head=self.head if head is None else head, forced=forced, refresh=refresh,
            git=self.history.git, api=api, sleep=self.sleeps.append)

    def passing(self, **overrides):
        return FakeApi(overrides.get("pulls", [merged(7)]),
                       overrides.get("runs", {self.pull_head: [run("success")]}))

    def test_rebase_merged_head_whose_run_passed_is_tested(self):
        self.assertNotEqual(self.head, self.pull_head)
        decision = self.decide(self.passing())
        self.assertTrue(decision.tested, decision.reason)
        self.assertEqual(decision.pull, 7)
        self.assertIn("#7", decision.reason)

    def test_newest_matching_run_decides(self):
        runs = [run("success", "2026-10-30T01:00:00Z", identifier=1),
                run("failure", "2026-10-30T02:00:00Z", identifier=2),
                run("success", "2026-10-30T03:00:00Z", path=".github/workflows/website.yml",
                    identifier=3),
                run("success", "2026-10-30T04:00:00Z", event="push", identifier=4)]
        decision = self.decide(self.passing(runs={self.pull_head: runs}))
        self.assertFalse(decision.tested, decision.reason)
        runs.append(run("success", "2026-10-30T05:00:00Z", identifier=5))
        self.assertTrue(self.decide(self.passing(runs={self.pull_head: runs})).tested)

    def test_unfinished_or_missing_run_builds(self):
        for runs in ([], [run(None)], [run("cancelled")], [run("skipped")]):
            with self.subTest(runs=runs):
                self.assertFalse(self.decide(self.passing(runs={self.pull_head: runs})).tested)

    def test_different_tree_builds(self):
        self.history.git("checkout", "--quiet", self.head)
        other = self.history.commit("engine.cpp", "changed after the run\n")
        decision = self.decide(self.passing(), head=other)
        self.assertFalse(decision.tested)
        self.assertIn("#7", decision.reason)

    def test_head_that_does_not_contain_the_replaced_master_builds(self):
        # The same tree, reached without the master the push replaced: its run
        # tested a merge with an older base, which is not this tree.
        self.history.git("checkout", "--quiet", "--orphan", "stale")
        self.history.git("commit", "--quiet", "-m", "same tree, other history")
        stale = self.history.git("rev-parse", "HEAD").strip()
        self.assertEqual(self.history.git("rev-parse", stale + "^{tree}"),
                         self.history.git("rev-parse", self.head + "^{tree}"))
        self.history.pull_head(7, stale)
        api = FakeApi([merged(7)], {stale: [run("success")]})
        self.assertFalse(self.decide(api).tested)

    def test_retargeted_pull_builds(self):
        # A run started against another base tested another tree, and
        # retargeting the pull request to master starts no new run.
        api = self.passing()
        api.events = [{"event": "base_ref_changed"}, {"event": "merged"}]
        self.assertFalse(self.decide(api).tested)
        api = self.passing()
        api.events = [{"event": "labeled"}] * checker.PAGE
        self.assertFalse(self.decide(api).tested)

    def test_head_shared_with_another_pull_builds(self):
        api = self.passing()
        api.sharing = [merged(7), {"number": 8, "merged_at": None, "base": {"ref": "review-x"}}]
        self.assertFalse(self.decide(api).tested)

    def test_unmerged_or_other_base_pull_builds_after_asking_again(self):
        for pulls in ([], [{"number": 7, "merged_at": None, "base": {"ref": "master"}}],
                      [merged(7, base="review-x")]):
            with self.subTest(pulls=pulls):
                self.sleeps.clear()
                decision = self.decide(self.passing(pulls=pulls))
                self.assertFalse(decision.tested)
                self.assertEqual(len(self.sleeps), checker.ASSOCIATION_ATTEMPTS - 1)

    def test_late_association_is_found(self):
        api = self.passing()
        api.association_delay = checker.ASSOCIATION_ATTEMPTS - 1
        self.assertTrue(self.decide(api).tested)

    def test_forced_new_or_malformed_push_builds_without_asking(self):
        for overrides in ({"forced": True}, {"before": "0" * 40}, {"before": ""},
                          {"before": "HEAD"}, {"head": "HEAD"}):
            with self.subTest(overrides=overrides):
                api = self.passing()
                self.assertFalse(self.decide(api, **overrides).tested)
                self.assertEqual(api.calls, [])

    def test_refresh_paths_build_without_asking(self):
        for refresh in (("engine.h",), ("sample.cpp", "vcpkg.json"), ("",), ("sample.cpp/",)):
            with self.subTest(refresh=refresh):
                api = self.passing()
                decision = self.decide(api, refresh=refresh)
                if refresh in (("",), ("sample.cpp/",)):
                    self.assertTrue(decision.tested, decision.reason)
                else:
                    self.assertFalse(decision.tested)
                    self.assertEqual(api.calls, [])
        self.history.git("checkout", "--quiet", self.head)
        cached = self.history.commit(".github/actions/emscripten/action.yml", "x\n")
        self.assertFalse(self.decide(self.passing(), head=cached, refresh=(".github/",)).tested)

    def test_main_reports_an_error_as_build(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "output"
            environment = {"GITHUB_REPOSITORY": OWNER}  # No token: the check cannot ask.
            with unittest.mock.patch.dict(os.environ, environment, clear=True):
                with unittest.mock.patch("sys.stdout"):
                    self.assertEqual(checker.main(
                        ["--workflow-path", WORKFLOW, "--before", "a" * 40,
                         "--head", "b" * 40, "--output", str(output)]), 0)
            self.assertEqual(output.read_text(encoding="utf-8"), "tested=false\n")


class WorkflowTests(unittest.TestCase):
    def test_each_workflow_names_its_own_file_and_gates_its_work(self):
        for name, gated in (("ci.yml", "needs.changes.outputs.tested != 'true'"),
                            ("website.yml", "steps.tested.outputs.tested != 'true'")):
            with self.subTest(workflow=name):
                text = (REPOSITORY / ".github/workflows" / name).read_text(encoding="utf-8")
                self.assertIn("python3 tools/ci_tested_tree.py", text)
                self.assertIn(f"--workflow-path .github/workflows/{name}", text)
                self.assertIn(gated, text)


if __name__ == "__main__":
    unittest.main()
