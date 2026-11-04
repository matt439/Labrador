"""Inventory comparison with a fake ctest, and its drift against ci.yml; no tool is run."""

from __future__ import annotations

import contextlib
import importlib.util
import io
import json
from pathlib import Path
import re
import subprocess
import sys
import unittest

REPOSITORY = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("check_ctest_inventory",
                                              REPOSITORY / "tools/check_ctest_inventory.py")
inventory_check = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = inventory_check
SPEC.loader.exec_module(inventory_check)

INVENTORY = json.loads(inventory_check.INVENTORY.read_text(encoding="utf-8"))


def ctest(names: list[str], code: int = 0):
    text = json.dumps({"tests": [{"name": name} for name in names]})
    return lambda argv: subprocess.CompletedProcess(argv, code, text, "refused")


def check(runner) -> tuple[int, str]:
    output = io.StringIO()
    with contextlib.redirect_stdout(output), contextlib.redirect_stderr(output):
        code = inventory_check.main(["--configuration", "x64-debug-null", "--build-dir", "out"],
                                    runner=runner)
    return code, output.getvalue()


class ExpectedTests(unittest.TestCase):
    def test_a_configuration_gets_the_shared_tests_and_its_own(self):
        inventory = {"configurations": ["a", "b"], "every_configuration": ["Shared"],
                     "some_configurations": {"OnlyA": ["a"]}}
        self.assertEqual(inventory_check.expected_tests(inventory, "a"), {"Shared", "OnlyA"})
        self.assertEqual(inventory_check.expected_tests(inventory, "b"), {"Shared"})

    def test_an_unknown_name_is_refused_rather_than_expecting_nothing(self):
        inventory = {"configurations": ["a"], "every_configuration": [],
                     "some_configurations": {"Test": ["typo"]}}
        with self.assertRaises(inventory_check.InventoryError):
            inventory_check.expected_tests(inventory, "b")
        with self.assertRaises(inventory_check.InventoryError):
            inventory_check.expected_tests(inventory, "a")


class Main(unittest.TestCase):
    expected = sorted(inventory_check.expected_tests(INVENTORY, "x64-debug-null"))

    def test_the_exact_set_passes(self):
        code, text = check(ctest(self.expected))
        self.assertEqual(code, 0, text)

    def test_a_dropped_test_fails_and_is_named(self):
        code, text = check(ctest(self.expected[1:]))
        self.assertEqual(code, 1)
        self.assertIn(f"missing: {self.expected[0]}", text)

    def test_an_unlisted_test_fails_and_is_named(self):
        code, text = check(ctest(self.expected + ["BrandNewTests"]))
        self.assertEqual(code, 1)
        self.assertIn("not in the inventory: BrandNewTests", text)

    def test_ctest_failing_or_answering_nonsense_is_not_a_pass(self):
        self.assertEqual(check(ctest(self.expected, code=1))[0], 2)
        self.assertEqual(check(lambda argv: subprocess.CompletedProcess(argv, 0, "not json", ""))[0], 2)


class Drift(unittest.TestCase):
    def test_every_configuration_ci_tests_is_checked_and_listed(self):
        workflow = (REPOSITORY / ".github/workflows/ci.yml").read_text(encoding="utf-8")
        matrix = set(re.findall(r"^\s+- preset: (\S+)$", workflow, re.MULTILINE))
        self.assertEqual(set(INVENTORY["configurations"]), matrix | {"wasm-release", "wasm-release-null"})
        checked = set(re.findall(r"check_ctest_inventory\.py --configuration (\S+)", workflow))
        # The windows job passes its matrix preset, which reads as "${{" here.
        self.assertEqual(checked, {"${{", "wasm-release", "wasm-release-null"})


if __name__ == "__main__":
    unittest.main()
