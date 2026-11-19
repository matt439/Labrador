"""Schedule, software-device and inventory contracts for the non-required weekly lane."""

from __future__ import annotations

import json
from pathlib import Path
import re
import shlex
import unittest

from tools.tests.test_required_checks import documented_contexts, matrix_values, scalar, workflow_contexts
from tools.tests.tool_fixtures import load_tool

REPOSITORY = Path(__file__).resolve().parents[2]
report = load_tool("tools/ci_gap_report.py")
inventory_check = load_tool("tools/check_ctest_inventory.py")
INVENTORY = json.loads(inventory_check.INVENTORY.read_text(encoding="utf-8"))
WEEKLY = ".github/workflows/weekly-configurations.yml"
SHARED_TOOL_VALUES = ("VULKAN_SDK_VERSION", "VULKAN_SDK_SHA256", "MESA_VERSION", "MESA_SHA256",
                      "EMSDK_VERSION", "CCACHE_VERSION", "CCACHE_SHA256", "CCACHE_DIR",
                      "CCACHE_MAXSIZE", "LABRADOR_TEST_BROWSER_ARGS")


def top_block(text: str, key: str) -> str:
    found = re.search(rf"^{re.escape(key)}:\n((?:(?:[ #].*)?\n)*)", text, re.MULTILINE)
    if found is None:
        raise ValueError(f"missing {key} block")
    return found[1]


def jobs(text: str) -> dict[str, str]:
    return {found[1]: found[2] for found in re.finditer(
        r"^  ([A-Za-z0-9_-]+):\n((?:(?:(?:   |  #).*)?\n)*)", top_block(text, "jobs"), re.MULTILINE)}


def events(text: str) -> set[str]:
    return set(re.findall(r"^  ([A-Za-z0-9_]+):", top_block(text, "on"), re.MULTILINE))


def environment(text: str) -> dict[str, str]:
    return {found[1]: scalar(found[2]) for found in re.finditer(
        r"^  ([A-Za-z0-9_]+): (.+)$", top_block(text, "env"), re.MULTILINE)}


def full_test_command(body: str) -> str:
    commands = re.findall(r"^\s*ctest (.+)$", body, re.MULTILINE)
    if len(commands) != 1:
        raise ValueError("each weekly job must run one complete ctest suite")
    command = commands[0]
    tokens = shlex.split(re.sub(r"\$\{\{.*?\}\}", "expression", command))
    seen = set()
    index = 0
    values = {"--preset", "--parallel", "--output-junit", "--test-output-size-passed"}
    flags = {"--no-tests=error", "--output-on-failure", "-V"}
    while index < len(tokens):
        option = tokens[index]
        if option in seen or option not in values | flags:
            raise ValueError(f"unsupported or repeated weekly ctest option: {option}")
        seen.add(option)
        index += 1
        if option in values:
            if index == len(tokens) or tokens[index].startswith("-"):
                raise ValueError(f"weekly ctest option needs a value: {option}")
            index += 1
    for required in ("--preset", "--no-tests=error", "--output-junit", "--output-on-failure"):
        if required not in seen:
            raise ValueError(f"weekly ctest is missing {required}")
    return command


class ParserTests(unittest.TestCase):
    def test_test_selection_refuses_filters_or_a_second_partial_run(self):
        good = "        run: |\n          ctest --preset fixture --no-tests=error --output-junit result.xml --output-on-failure\n"
        self.assertIn("--preset fixture", full_test_command(good))
        for changed in (good.replace(" --preset", " -E Pixel --preset"),
                        good.replace(" --preset", " -LE GPU --preset"),
                        good.replace(" --preset", " --tests-regex Core --preset"),
                        good + "          ctest --preset fixture -R Pixel\n",
                        good.replace(" --preset fixture", " --preset fixture --preset other"),
                        good.replace(" --no-tests=error", "")):
            with self.subTest(command=changed), self.assertRaises(ValueError):
                full_test_command(changed)


class WeeklyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.weekly = (REPOSITORY / WEEKLY).read_text(encoding="utf-8")
        cls.primary = (REPOSITORY / ".github/workflows/ci.yml").read_text(encoding="utf-8")
        cls.sanitizer = (REPOSITORY / ".github/workflows/sanitizers.yml").read_text(encoding="utf-8")
        cls.jobs = jobs(cls.weekly)

    def test_schedule_is_the_existing_weekly_timing_and_cannot_report_on_a_pr_or_push(self):
        self.assertEqual(events(self.weekly), {"schedule", "workflow_dispatch"})
        cron = re.findall(r"^    - cron: '([^']+)'$", top_block(self.weekly, "on"), re.MULTILINE)
        self.assertEqual(cron, re.findall(r"^    - cron: '([^']+)'$", top_block(self.primary, "on"), re.MULTILINE))
        self.assertEqual(cron, ["17 3 * * 1"])
        self.assertNotIn("continue-on-error", self.weekly)
        self.assertEqual(set(self.jobs), {"native", "browser"})

    def test_exact_five_profiles_match_the_candidate_plan_and_existing_inventories(self):
        native = self.jobs["native"]
        presets = matrix_values(native, "preset")
        self.assertEqual(tuple(presets) + ("wasm-debug",), report.WEEKLY_PRESETS)
        self.assertEqual(presets, ["x64-release-d3d12", "x64-release-gl", "x64-release-vulkan", "x64-release-null"])
        self.assertIn("check_ctest_inventory.py --configuration ${{ matrix.preset }}", native.replace("\\", "/"))
        self.assertIn("check_ctest_inventory.py --configuration wasm-debug", self.jobs["browser"])
        for preset in report.WEEKLY_PRESETS:
            with self.subTest(preset=preset):
                expected = inventory_check.expected_tests(INVENTORY, preset)
                self.assertTrue(expected)
                gap = next(gap for gap in report.GAPS if gap.preset == preset)
                self.assertEqual(set(gap.expected), expected)
        browser = inventory_check.expected_tests(INVENTORY, "wasm-debug")
        self.assertEqual(len([name for name in browser if name.startswith("Firefox")]), 8)
        self.assertTrue(set(INVENTORY["chromium_registered"]) <= browser)

    def test_candidate_report_distinguishes_required_pr_gaps_from_weekly_coverage(self):
        outcomes = [report.Outcome(gap.preset) for gap in report.GAPS]
        markdown = report.markdown(outcomes, commit="fixture", changed=0, executed=False)
        self.assertIn("What required PR CI misses", markdown)
        self.assertEqual([gap.preset for gap in report.GAPS],
                         ["x64-debug-gl", "x64-debug-vulkan", "x64-debug",
                          "x64-debug-d3d12", "x64-release", *report.WEEKLY_PRESETS])

    def test_native_release_uses_qualified_ipo_material_tools_and_correct_software_devices(self):
        native = self.jobs["native"]
        self.assertIn("-DLABRADOR_NATIVE_RELEASE_IPO=ON", native)
        self.assertIn("matrix.preset == 'x64-release-gl' && '-DVCPKG_MANIFEST_FEATURES=materials'", native)
        self.assertEqual(matrix_values(native, "software_device"), ["Basic", "llvmpipe", "SwiftShader", ""])
        self.assertEqual(matrix_values(native, "vulkan_sdk"), ["false", "false", "true", "false"])
        self.assertIn("matrix.preset != 'x64-release-null'", native)
        self.assertIn('findstr /C:"Drawing on" device.txt | findstr /C:"${{ matrix.software_device }}"', native)
        self.assertIn("matrix.preset == 'x64-release-d3d12' && "
                      "'-DLABRADOR_D3D_SOFTWARE_TEST=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON'", native)
        self.assertIn("d3d_software_check.py --mode pixels", native)
        self.assertEqual(native.count("/tests/render/d3d-software.json"), 2)
        self.assertIn("VK_DRIVER_FILES", native)
        self.assertIn("HKLM:\\SOFTWARE\\Khronos\\Vulkan\\Drivers", native)
        self.assertIn("GALLIUM_DRIVER=llvmpipe", native)
        for directory in ("tests\\render", "samples\\minimal", "samples\\collision"):
            self.assertIn(f'Copy-Item "$mesa\\*.dll" "out\\build\\${{{{ matrix.preset }}}}\\{directory}"', native)

    def test_every_profile_runs_its_complete_registered_suite_and_keeps_results(self):
        native = full_test_command(self.jobs["native"])
        browser = full_test_command(self.jobs["browser"])
        self.assertIn("--preset ${{ matrix.preset }}", native)
        self.assertIn("--preset wasm-debug", browser)
        self.assertIn(" -V", browser)
        for body in self.jobs.values():
            self.assertIn("if: always()", body)
            self.assertIn("weekly-results.xml", body)
            self.assertIn("Testing/Temporary/LastTest.log", body)
            self.assertIn("--parallel 2", body)

    def test_browser_uses_material_tools_software_firefox_and_separate_debug_objects(self):
        browser = self.jobs["browser"]
        self.assertIn("cmake --preset wasm-debug -DLABRADOR_FIREFOX_SOFTWARE_WEBGL=ON -DVCPKG_MANIFEST_FEATURES=materials", browser)
        self.assertIn("objects: wasm-debug-weekly-gl", browser)
        self.assertNotIn("objects: wasm-debug-weekly-gl", self.primary)
        self.assertIn("set MSYSTEM=", browser)
        shared = environment(self.primary)
        for key in SHARED_TOOL_VALUES:
            with self.subTest(value=key):
                self.assertEqual(environment(self.weekly)[key], shared[key])

    def test_required_twenty_contexts_and_sanitizer_selection_are_unchanged(self):
        contexts = workflow_contexts(self.primary) + workflow_contexts(
            (REPOSITORY / ".github/workflows/website.yml").read_text(encoding="utf-8"))
        stated, listed, _ = documented_contexts((REPOSITORY / "CONTRIBUTING.md").read_text(encoding="utf-8"))
        self.assertEqual((stated, len(contexts)), (20, 20))
        self.assertEqual(set(listed), set(contexts))
        weekly = workflow_contexts(self.weekly)
        self.assertEqual(len(weekly), 5)
        self.assertFalse(set(weekly) & set(contexts))
        self.assertFalse(set(weekly) & set(workflow_contexts(self.sanitizer)))
        self.assertEqual(events(self.sanitizer), {"schedule", "workflow_dispatch"})
        self.assertIn("'47 3 * * 1'", self.sanitizer)
        self.assertNotIn("x64-debug-null-asan", self.weekly)
        self.assertEqual(set(re.findall(r"ctest --preset (\S+)", self.sanitizer)), {"x64-debug-null-asan"})
        self.assertIn("python tools/asan_receipt.py", self.sanitizer)

    def test_documented_weekly_counts_are_the_authoritative_inventory_counts(self):
        documentation = (REPOSITORY / "docs/verification-weekly.md").read_text(encoding="utf-8")
        rows = re.findall(r"^\| `([^`]+)` \| ([0-9]+) \|", documentation, re.MULTILINE)
        counts = {preset: int(count) for preset, count in rows}
        self.assertEqual(set(counts), set(report.WEEKLY_PRESETS))
        self.assertEqual(counts, {preset: len(inventory_check.expected_tests(INVENTORY, preset))
                                  for preset in report.WEEKLY_PRESETS})


if __name__ == "__main__":
    unittest.main()
