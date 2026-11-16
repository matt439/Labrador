"""Require safe execution and deterministic heap/container ASan reports from the instrumented probe."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import subprocess
import sys


def verify_probe(executable: Path, *, run=subprocess.run) -> list[str]:
    environment = os.environ.copy()
    environment["ASAN_OPTIONS"] = "detect_container_overflow=1"
    observations = []
    for mode, expected in (("--safe", None), ("--heap-overflow", "heap-buffer-overflow"),
                           ("--container-overflow", "container-overflow")):
        result = run([str(executable), mode], capture_output=True, text=True,
                     encoding="utf-8", errors="replace", timeout=20, env=environment)
        output = result.stdout + result.stderr
        if expected is None:
            if result.returncode != 0 or "AddressSanitizer:" in output:
                raise ValueError(f"safe probe failed ({result.returncode}): {output}")
            observations.append("safe probe exited zero")
        else:
            signature = f"AddressSanitizer: {expected}"
            if result.returncode == 0 or signature not in output:
                raise ValueError(f"{mode} did not fail with {signature} ({result.returncode}): {output}")
            observations.append(f"{mode}: detected {signature}, exit {result.returncode}")
    return observations


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    options = parser.parse_args(arguments)
    try:
        for observation in verify_probe(options.executable):
            print(observation)
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        print(f"asan_probe: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
