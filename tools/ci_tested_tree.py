"""Say whether a push to master carries a tree a pull request already passed.

A pull request merges only once its branch is up to date with master and its
checks are green (CONTRIBUTING.md, required checks are strict), so the push
that merging makes usually lands a tree the pull request's run has already
built and tested. This finds that run, and the workflow skips its jobs when it
does. Every condition below must be proven from the repository or the API;
anything unproven, including an error, means build.

- the push is not forced and has a previous master to compare with;
- it changes no refresh path. A run on master is what saves the caches pull
  requests restore, so a change to what those caches are keyed on still runs;
- a pull request merged into master is associated with the pushed commit;
- that pull request's head has the pushed commit's tree, and contains the
  master the push replaced. A pull_request run tests the head merged into its
  base; with the base an ancestor of the head, that merge is the head's tree;
- every run on that head tested it against master. A run tests the base the
  pull request had when it started, and retargeting one starts no run, so its
  base must never have changed; and no other pull request may share the head,
  since runs are found by head and not by pull request. A closed pull
  request's runs no longer name it, so this is proven from the pull request;
- the newest run of this workflow for that head, from a pull_request event,
  succeeded.
"""

from __future__ import annotations

import argparse
from collections.abc import Callable, Iterable
from dataclasses import dataclass
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time
from typing import Any
from urllib.request import Request, urlopen


API = "https://api.github.com"
USER_AGENT = "LabradorCI/1.0 (+https://github.com/matt439/Labrador)"
# The commit's pull request association is written after the push event is
# sent, so an empty answer is asked again before it is believed.
ASSOCIATION_ATTEMPTS = 3
ASSOCIATION_WAIT_SECONDS = 10.0
PAGE = 100
SHA = re.compile(r"[0-9a-f]{40}")

Git = Callable[..., str]
Api = Callable[[str], Any]


@dataclass(frozen=True)
class Decision:
    tested: bool
    reason: str
    pull: int | None = None


def refreshes(path: str, refresh: Iterable[str]) -> bool:
    """A refresh entry ending in / names a directory; otherwise one file."""
    return any(path.startswith(entry) if entry.endswith("/") else path == entry
               for entry in refresh)


def newest_run(runs: list[dict[str, Any]], workflow_path: str) -> dict[str, Any] | None:
    matching = [run for run in runs
                if run.get("path") == workflow_path and run.get("event") == "pull_request"]
    return max(matching, key=lambda run: (run.get("created_at") or "", run.get("id") or 0),
               default=None)


def merged_pulls(api: Api, repository: str, head: str,
                 sleep: Callable[[float], None]) -> list[int]:
    for attempt in range(1, ASSOCIATION_ATTEMPTS + 1):
        pulls = api(f"/repos/{repository}/commits/{head}/pulls")
        numbers = [pull["number"] for pull in pulls
                   if pull.get("merged_at") and pull.get("base", {}).get("ref") == "master"]
        if numbers or attempt == ASSOCIATION_ATTEMPTS:
            return numbers
        sleep(ASSOCIATION_WAIT_SECONDS)
    return []


def tested_against_master(api: Api, repository: str, number: int, pull_head: str) -> bool:
    sharing = api(f"/repos/{repository}/commits/{pull_head}/pulls?per_page={PAGE}")
    if [pull["number"] for pull in sharing] != [number]:
        return False
    # A full page may hide more; unproven is untested.
    events = api(f"/repos/{repository}/issues/{number}/events?per_page={PAGE}")
    return len(events) < PAGE and all(event.get("event") != "base_ref_changed"
                                      for event in events)


def decide(*, repository: str, workflow_path: str, before: str, head: str, forced: bool,
           refresh: Iterable[str], git: Git, api: Api,
           sleep: Callable[[float], None] = time.sleep) -> Decision:
    if forced:
        return Decision(False, "a forced push replaces history no pull request tested")
    if not SHA.fullmatch(before) or not before.strip("0"):
        return Decision(False, "the push has no previous master to compare with")
    if not SHA.fullmatch(head):
        return Decision(False, f"{head!r} is not a commit hash")
    changed = git("diff", "--name-only", before, head).splitlines()
    refresh = tuple(refresh)
    refreshed = [path for path in changed if refreshes(path, refresh)]
    if refreshed:
        return Decision(False, "it changes " + ", ".join(refreshed)
                        + ", which master's caches are keyed on, so master saves them again")

    pulls = merged_pulls(api, repository, head, sleep)
    if not pulls:
        return Decision(False, "no pull request merged into master is associated with " + head)
    tree = git("rev-parse", head + "^{tree}").strip()
    for number in pulls:
        git("fetch", "--quiet", "--no-tags", "origin", f"refs/pull/{number}/head")
        pull_head = git("rev-parse", "FETCH_HEAD").strip()
        if git("rev-parse", pull_head + "^{tree}").strip() != tree:
            continue
        try:
            git("merge-base", "--is-ancestor", before, pull_head)
        except subprocess.CalledProcessError:
            continue
        if not tested_against_master(api, repository, number, pull_head):
            continue
        runs = api(f"/repos/{repository}/actions/runs?event=pull_request"
                   f"&head_sha={pull_head}&per_page={PAGE}")["workflow_runs"]
        run = newest_run(runs, workflow_path)
        if run is None or run.get("conclusion") != "success":
            continue
        return Decision(True, f"pull request #{number} passed this tree in {run['html_url']}",
                        number)
    return Decision(False, "no merged pull request passed this workflow on this tree: #"
                    + ", #".join(str(number) for number in pulls))


def run_git(*arguments: str) -> str:
    return subprocess.run(["git", *arguments], check=True, capture_output=True,
                          text=True, timeout=120).stdout


def github_api(token: str) -> Api:
    def get(path: str) -> Any:
        request = Request(API + path, headers={
            "Accept": "application/vnd.github+json",
            "Authorization": "Bearer " + token,
            "User-Agent": USER_AGENT,
            "X-GitHub-Api-Version": "2022-11-28",
        })
        with urlopen(request, timeout=30) as response:
            return json.load(response)
    return get


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--workflow-path", required=True,
                        help="the workflow file, as a run reports it: .github/workflows/ci.yml")
    parser.add_argument("--before", required=True)
    parser.add_argument("--head", required=True)
    parser.add_argument("--forced", default="false")
    parser.add_argument("--refresh", action="append", default=[],
                        help="a path whose change still builds; a trailing / names a directory")
    parser.add_argument("--output", type=Path, help="GITHUB_OUTPUT, given tested=true|false")
    arguments = parser.parse_args(argv)

    try:
        decision = decide(
            repository=os.environ["GITHUB_REPOSITORY"],
            workflow_path=arguments.workflow_path, before=arguments.before,
            head=arguments.head, forced=arguments.forced == "true",
            refresh=arguments.refresh, git=run_git,
            api=github_api(os.environ["GITHUB_TOKEN"]))
    except Exception as error:  # Unproven is untested: the workflow builds.
        decision = Decision(False, f"the check failed, so this run builds: {error!r}")
    if decision.tested:
        print("Already tested: " + decision.reason + ". Nothing is built again.")
    else:
        print("Building: " + decision.reason + ".")
    if arguments.output is not None:
        with arguments.output.open("a", encoding="utf-8") as output:
            output.write(f"tested={'true' if decision.tested else 'false'}\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
