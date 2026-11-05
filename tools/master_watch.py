"""Act on a finished run of a workflow on master: rerun a failure once, then say so.

A merge to master is tested by its pull request, so a red run on master is
either a runner flake or something the pull request could not see. On a first
failed attempt this reruns the failed jobs, which tells a flake from a fault
on a tree already tested. A failure
that survives it opens one issue per workflow, or comments on the open one,
and GitHub notifies the repository's watchers. The next green run on master
closes it.

Run by .github/workflows/master-watch.yml with the workflow_run payload in
GITHUB_EVENT_PATH and GH_TOKEN set; it calls gh and nothing else. It waits
for its own rerun rather than hearing of it later, because a rerun started
with the workflow's token raises no workflow_run event of its own. The wait
is bounded inside the job's timeout, so a rerun that never finishes ends in a
printed outcome and a failed job rather than a killed one, and never in a
green job.
"""

from __future__ import annotations

from dataclasses import dataclass
import json
import os
import subprocess
import sys
import time
from typing import Callable, Sequence

FAILED = {"failure", "timed_out", "startup_failure"}
TITLE = "master is red: {workflow}"
# 80 polls 30 seconds apart end inside master-watch.yml's 45-minute timeout.
RERUN_POLLS = 80
RERUN_INTERVAL = 30.0


@dataclass(frozen=True)
class Action:
    kind: str  # "rerun", "report", "close", "expired" or "none"
    reason: str


def decide(run: dict) -> Action:
    conclusion = run.get("conclusion")
    if run.get("head_branch") != "master":
        return Action("none", "not a run on master")
    if conclusion == "success":
        return Action("close", "master is green")
    if conclusion not in FAILED:
        # A cancelled run is a superseded one, not a red one.
        return Action("none", f"concluded {conclusion}")
    if run.get("run_attempt", 1) == 1:
        return Action("rerun", "first failed attempt")
    return Action("report", f"attempt {run['run_attempt']} also failed")


def body(run: dict) -> str:
    return (f"[{run['name']} #{run['run_number']}]({run['html_url']}) failed on master at "
            f"`{run['head_sha'][:7]}`, attempt {run['run_attempt']}, after rerunning its failed jobs "
            f"once.\n\nA second failure on a tree its pull request already passed is either a "
            f"fault the pull request could not see or a flake that repeats. The next green run of "
            f"this workflow on master closes this issue.")


def wait_for_rerun(run: dict, gh: Callable[[Sequence[str]], str],
                   sleep: Callable[[float], None], interval: float = RERUN_INTERVAL,
                   polls: int = RERUN_POLLS) -> dict | None:
    """The run as it stands once the rerun has finished, or None if no later
    attempt has completed after `polls` views."""
    for _ in range(polls):
        sleep(interval)
        state = json.loads(gh(["run", "view", str(run["id"]), "--json", "status,conclusion,attempt"]))
        if state["status"] == "completed" and state["attempt"] > run.get("run_attempt", 1):
            return {**run, "conclusion": state["conclusion"], "run_attempt": state["attempt"]}
    return None


def act(run: dict, gh: Callable[[Sequence[str]], str],
        sleep: Callable[[float], None] = time.sleep) -> Action:
    action = decide(run)
    if action.kind == "rerun":
        gh(["run", "rerun", str(run["id"]), "--failed"])
        rerun = wait_for_rerun(run, gh, sleep)
        if rerun is None:
            return Action("expired", f"no rerun attempt completed after {RERUN_POLLS} polls")
        run = rerun
        action = decide(run)
    title = TITLE.format(workflow=run.get("name", "?"))
    if action.kind in ("report", "close"):
        found = json.loads(gh(["issue", "list", "--state", "open", "--search",
                               f'in:title "{title}"', "--json", "number,title"]) or "[]")
        existing = [issue["number"] for issue in found if issue["title"] == title]
        if action.kind == "report":
            if existing:
                gh(["issue", "comment", str(existing[0]), "--body", body(run)])
            else:
                gh(["issue", "create", "--title", title, "--body", body(run)])
        for number in existing if action.kind == "close" else ():
            gh(["issue", "close", str(number), "--comment",
                f"[{run['name']} #{run['run_number']}]({run['html_url']}) passed on master at "
                f"`{run['head_sha'][:7]}`."])
    return action


def gh_cli(arguments: Sequence[str]) -> str:
    return subprocess.run(["gh", *arguments], check=True, capture_output=True, text=True).stdout


def main(gh: Callable[[Sequence[str]], str] = gh_cli,
         sleep: Callable[[float], None] = time.sleep) -> int:
    with open(os.environ["GITHUB_EVENT_PATH"], encoding="utf-8") as event:
        run = json.load(event)["workflow_run"]
    action = act(run, gh, sleep)
    print(f"{run.get('name')} #{run.get('run_number')}: {action.kind} ({action.reason})")
    if action.kind == "expired":
        # Neither green nor red is known, so the job must not end green.
        print("The rerun did not finish inside this job's wait; its result is unknown "
              "and this job fails so that it is looked at.", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
