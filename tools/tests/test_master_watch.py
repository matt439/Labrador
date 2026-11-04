"""Master watch decisions and the gh calls they make, with a fake gh; nothing is called."""

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import sys
import unittest

REPOSITORY = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("master_watch", REPOSITORY / "tools/master_watch.py")
watch = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = watch
SPEC.loader.exec_module(watch)


def run(conclusion="failure", attempt=1, branch="master"):
    return {"id": 41770920634, "name": "build and test", "run_number": 900,
            "html_url": "https://github.invalid/run", "head_sha": "a" * 40,
            "head_branch": branch, "conclusion": conclusion, "run_attempt": attempt}


class FakeGh:
    def __init__(self, open_issues=(), views=()):
        self.calls = []
        self.open_issues = list(open_issues)
        self.views = list(views)

    def __call__(self, arguments):
        self.calls.append(list(arguments))
        if arguments[:2] == ["issue", "list"]:
            return json.dumps(self.open_issues)
        if arguments[:2] == ["run", "view"]:
            return json.dumps(self.views.pop(0))
        return ""


def act(run_, gh):
    sleeps = []
    action = watch.act(run_, gh, sleep=sleeps.append)
    return action, sleeps


TITLE = "master is red: build and test"


class Decide(unittest.TestCase):
    def test_each_outcome(self):
        self.assertEqual(watch.decide(run()).kind, "rerun")
        self.assertEqual(watch.decide(run(attempt=2)).kind, "report")
        self.assertEqual(watch.decide(run("timed_out", 3)).kind, "report")
        self.assertEqual(watch.decide(run("success")).kind, "close")
        self.assertEqual(watch.decide(run("cancelled")).kind, "none")
        self.assertEqual(watch.decide(run(branch="review-x")).kind, "none")


class Act(unittest.TestCase):
    def test_a_first_failure_reruns_only_the_failed_jobs_and_waits(self):
        gh = FakeGh(views=[{"status": "in_progress", "conclusion": None, "attempt": 2},
                           {"status": "completed", "conclusion": "success", "attempt": 2}])
        action, sleeps = act(run(), gh)
        self.assertEqual(gh.calls[0], ["run", "rerun", "41770920634", "--failed"])
        self.assertEqual(len(sleeps), 2)
        self.assertEqual(action.kind, "close")

    def test_a_rerun_that_fails_again_is_reported(self):
        gh = FakeGh(views=[{"status": "completed", "conclusion": "failure", "attempt": 1},
                           {"status": "completed", "conclusion": "failure", "attempt": 2}])
        action, _ = act(run(), gh)
        self.assertEqual(action.kind, "report")
        self.assertEqual(gh.calls[-1][:4], ["issue", "create", "--title", TITLE])

    def test_a_second_failure_opens_one_issue_then_comments_on_it(self):
        gh = FakeGh()
        act(run(attempt=2), gh)
        self.assertEqual(gh.calls[-1][:4], ["issue", "create", "--title", TITLE])
        gh = FakeGh([{"number": 12, "title": TITLE}, {"number": 13, "title": TITLE + " later"}])
        act(run(attempt=2), gh)
        self.assertEqual(gh.calls[-1][:3], ["issue", "comment", "12"])

    def test_green_closes_the_open_issue_and_nothing_else(self):
        gh = FakeGh([{"number": 12, "title": TITLE}, {"number": 14, "title": "master is red: website"}])
        act(run("success"), gh)
        self.assertEqual([call[:3] for call in gh.calls[1:]], [["issue", "close", "12"]])

    def test_green_with_nothing_open_does_nothing_more(self):
        gh = FakeGh()
        act(run("success"), gh)
        self.assertEqual(len(gh.calls), 1)

    def test_the_workflow_watches_both_workflows_by_name(self):
        text = (REPOSITORY / ".github/workflows/master-watch.yml").read_text(encoding="utf-8")
        names = set()
        for path in ("ci.yml", "website.yml"):
            first = (REPOSITORY / ".github/workflows" / path).read_text(encoding="utf-8")
            names.add(next(line.split(":", 1)[1].strip() for line in first.splitlines()
                           if line.startswith("name:")))
        for name in names:
            self.assertIn(f'"{name}"', text)


if __name__ == "__main__":
    unittest.main()
