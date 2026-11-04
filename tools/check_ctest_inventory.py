"""Fail when a build's registered ctest entries differ from tests/ctest_inventory.json.

ctest passes with whatever is registered, so a CMake change that drops a test
stays green. The inventory is the list each CI configuration must register;
this compares it with `ctest --show-only=json-v1`. It sees entries, not the
doctest cases inside one.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess
import sys

REPOSITORY = Path(__file__).resolve().parents[1]
INVENTORY = REPOSITORY / "tests/ctest_inventory.json"


class InventoryError(ValueError):
    """The inventory or ctest's answer cannot be compared honestly."""


def expected_tests(inventory: dict, configuration: str) -> set[str]:
    if configuration not in inventory["configurations"]:
        raise InventoryError(f"{configuration} is not in {INVENTORY.name}'s configurations")
    expected = set(inventory["every_configuration"])
    for name, configurations in inventory["some_configurations"].items():
        unknown = set(configurations) - set(inventory["configurations"])
        if unknown:
            raise InventoryError(f"{name} names unknown configurations: {sorted(unknown)}")
        if configuration in configurations:
            expected.add(name)
    return expected


def registered_tests(text: str) -> set[str]:
    try:
        return {test["name"] for test in json.loads(text)["tests"]}
    except (ValueError, KeyError, TypeError) as error:
        raise InventoryError(f"ctest's JSON cannot be read: {error}") from error


def differences(expected: set[str], registered: set[str]) -> list[str]:
    lines = [f"missing: {name}" for name in sorted(expected - registered)]
    lines += [f"not in the inventory: {name}" for name in sorted(registered - expected)]
    return lines


def main(arguments: list[str] | None = None, *, runner=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--configuration", required=True, help="a name from the inventory")
    parser.add_argument("--build-dir", required=True, help="the configured build directory")
    options = parser.parse_args(arguments)
    run = runner or (lambda argv: subprocess.run(argv, capture_output=True, text=True, check=False))
    try:
        inventory = json.loads(INVENTORY.read_text(encoding="utf-8"))
        expected = expected_tests(inventory, options.configuration)
        result = run(["ctest", "--test-dir", options.build_dir, "--show-only=json-v1"])
        if result.returncode != 0:
            raise InventoryError(f"ctest exited {result.returncode}: {result.stderr.strip()}")
        problems = differences(expected, registered_tests(result.stdout))
    except (OSError, InventoryError) as error:
        print(f"check_ctest_inventory: {error}", file=sys.stderr)
        return 2
    if problems:
        print(f"{options.configuration} does not register what tests/ctest_inventory.json says:")
        for line in problems:
            print(f"  {line}")
        print("A test added or removed on purpose changes the inventory in the same commit.")
        return 1
    print(f"{options.configuration}: all {len(expected)} expected tests are registered.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
