"""Run Git against an explicit checkout, with nothing ambient steering it.

Every tool here that reads Git goes through git(). Ambient GIT_* variables,
global and system configuration, grafts and replacement refs cannot route a
call to another checkout, object store or history, and Git never pages, takes
optional locks, fetches a missing object lazily or prompts for credentials.
An explicit `git fetch` still reaches its remote. Standard library only.
"""

from __future__ import annotations

import os
from pathlib import Path
import subprocess


class GitError(RuntimeError):
    """Git could not run, or exited with a status the caller did not accept."""


def environment() -> dict[str, str]:
    """The process environment without GIT_* variables, plus the isolation."""
    result = {name: value for name, value in os.environ.items() if not name.upper().startswith("GIT_")}
    result.update({"GIT_CONFIG_NOSYSTEM": "1", "GIT_CONFIG_GLOBAL": os.devnull,
                   "GIT_GRAFT_FILE": os.devnull, "GIT_TERMINAL_PROMPT": "0"})
    return result


def git(repository: Path, *arguments: str, accepted: tuple[int, ...] = (0,), text: bool = False,
        timeout: float = 30, error: type[Exception] = GitError) -> subprocess.CompletedProcess:
    """Run `git -C repository arguments...`, raising `error` when Git cannot run
    or exits with a status outside `accepted`. Output is bytes unless `text`."""
    try:
        result = subprocess.run(
            ["git", "--no-pager", "--no-optional-locks", "--no-lazy-fetch", "--no-replace-objects",
             "-C", str(repository), *arguments],
            capture_output=True, env=environment(), shell=False, timeout=timeout, check=False,
            text=text, encoding="utf-8" if text else None)
    except (OSError, subprocess.TimeoutExpired) as failure:
        raise error(f"cannot read local Git history: {failure}") from failure
    if result.returncode not in accepted:
        detail = result.stderr if text else result.stderr.decode("utf-8", errors="replace")
        raise error(f"git {arguments[0]} failed: {detail.strip() or 'no diagnostic'}")
    return result


def tree_files(root: Path) -> list[str]:
    """Repository-relative names of the tracked and untracked-but-not-ignored
    files under root, as a commit of the working tree would hold them."""
    listed = git(root, "ls-files", "-z", "--cached", "--others", "--exclude-standard").stdout
    return [name.decode("utf-8") for name in listed.split(b"\0") if name]
