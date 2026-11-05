"""Run a build, and run it once more only if it failed on a locked output file.

Windows runners sometimes fail a link edge with a bare "Access is denied." or
"The process cannot access the file because it is being used by another
process." beside a different executable each time, for no cause yet found.
A second build relinks only the failed edge, and any other failure, compile
and link errors included, is returned at once without a retry, so this
cannot hide a defect.
"""

from __future__ import annotations

import subprocess
import sys
from typing import Callable, Sequence

LOCKED = ("Access is denied.",
          "The process cannot access the file because it is being used by another process.")


def locked_output(text: str) -> bool:
    return any(line.strip() in LOCKED for line in text.splitlines())


def stream(command: Sequence[str]) -> tuple[int, str]:
    """Run command, echoing its output as it comes, and return code and output."""
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               text=True, errors="replace")
    lines = []
    assert process.stdout is not None
    for line in process.stdout:
        sys.stdout.write(line)
        lines.append(line)
    return process.wait(), "".join(lines)


def main(arguments: Sequence[str] | None = None,
         *, run: Callable[[Sequence[str]], tuple[int, str]] = stream) -> int:
    command = list(sys.argv[1:] if arguments is None else arguments)
    if not command:
        print("usage: ci_build_retry.py <build command...>", file=sys.stderr)
        return 2
    code, output = run(command)
    if code == 0 or not locked_output(output):
        return code
    # GitHub's annotation syntax, so the retry shows on the run's summary.
    print("::warning::The build failed on a locked output file; building once more.", flush=True)
    return run(command)[0]


if __name__ == "__main__":
    sys.exit(main())
