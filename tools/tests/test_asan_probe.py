"""The sanitizer gate must reject a silent/uninstrumented or incorrectly failing probe."""

from pathlib import Path
import json
import re
import subprocess
import unittest

from tools.tests.tool_fixtures import load_tool


probe = load_tool("tools/asan_probe.py")
receipt = load_tool("tools/asan_receipt.py")


def replies(results):
    pending = iter(results)

    def run(arguments, **options):
        assert options["timeout"] == 20
        assert "detect_container_overflow=1" in options["env"]["ASAN_OPTIONS"]
        code, output = next(pending)
        return subprocess.CompletedProcess(arguments, code, "", output)
    return run


class DetectionTests(unittest.TestCase):
    def test_requires_safe_execution_and_both_expected_reports(self):
        observations = probe.verify_probe(Path("probe.exe"), run=replies([
            (0, ""), (1, "ERROR: AddressSanitizer: heap-buffer-overflow"),
            (1, "ERROR: AddressSanitizer: container-overflow"),
        ]))
        self.assertEqual(len(observations), 3)
        self.assertIn("container-overflow", observations[-1])

    def test_a_non_sanitizer_crash_or_zero_exit_cannot_pass(self):
        for failed in [(1, "missing runtime DLL"), (0, "AddressSanitizer: heap-buffer-overflow")]:
            with self.subTest(failed=failed), self.assertRaisesRegex(ValueError, "did not fail with"):
                probe.verify_probe(Path("probe.exe"), run=replies([(0, ""), failed]))

    def test_container_annotation_opt_out_cannot_pass(self):
        with self.assertRaisesRegex(ValueError, "container-overflow"):
            probe.verify_probe(Path("probe.exe"), run=replies([
                (0, ""), (1, "AddressSanitizer: heap-buffer-overflow"), (0, ""),
            ]))

    def test_broken_safe_probe_is_not_detection_evidence(self):
        for failure in [(1, ""), (0, "AddressSanitizer: heap-buffer-overflow")]:
            with self.subTest(failure=failure), self.assertRaisesRegex(ValueError, "safe probe failed"):
                probe.verify_probe(Path("probe.exe"), run=replies([failure]))


class InstrumentationTests(unittest.TestCase):
    def test_missing_flags_annotations_or_runtime_cannot_produce_a_receipt(self):
        commands = [{"file": "engine.cpp", "command": "cl /fsanitize=address /fp:precise"}]
        directives = "/InferAsanLibs annotate_string=1 annotate_vector=1 annotate_optional=1"
        imports = "clang_rt.asan_dbg_dynamic-x86_64.dll"
        self.assertEqual(receipt.instrumentation(commands, directives, imports, imports)["translation_units"], 1)
        for changed, flags, runtime in [
            ([], directives, imports),
            ([{"file": "wrong.cpp", "command": "cl /fp:precise"}], directives, imports),
            ([{"file": "wrong.cpp", "command": "cl /fsanitize=address /RTC1"}], directives, imports),
            ([{"file": "wrong.cpp", "command": "cl /fsanitize=address /D_DISABLE_STL_ANNOTATION"}], directives, imports),
            (commands, directives.replace("annotate_vector=1", "annotate_vector=0"), imports),
            (commands, directives, "ucrtbased.dll"),
        ]:
            with self.subTest(changed=changed, flags=flags, runtime=runtime), self.assertRaises(ValueError):
                receipt.instrumentation(changed, flags, runtime, imports)
        with self.assertRaisesRegex(ValueError, "DirectXTK"):
            receipt.instrumentation(commands, directives, imports, "ucrtbased.dll")


class ConfigurationTests(unittest.TestCase):
    root = Path(__file__).resolve().parents[2]

    def test_scheduled_lane_runs_all_registered_tests_and_preserves_failure_verdicts(self):
        workflow = (self.root / ".github/workflows/sanitizers.yml").read_text(encoding="utf-8")
        command = re.search(r"^\s+ctest --preset .*", workflow, re.MULTILINE).group(0)
        self.assertNotRegex(command, r"(?:^|\s)(?:-[RELI]|--(?:exclude|tests-regex|label-regex))\b")
        self.assertIn("--no-tests=error", command)
        self.assertNotIn("continue-on-error", workflow)
        self.assertNotIn("pull_request:", workflow)
        self.assertNotIn("push:", workflow)
        self.assertLess(workflow.index("cmake --build"), workflow.index("python tools/asan_receipt.py"))
        self.assertLess(workflow.index("python tools/asan_receipt.py"), workflow.index("ctest --preset"))

    def test_detection_probe_is_additional_to_the_entire_ordinary_null_inventory(self):
        inventory = json.loads((self.root / "tests/ctest_inventory.json").read_text(encoding="utf-8"))
        check = load_tool("tools/check_ctest_inventory.py")
        ordinary = check.expected_tests(inventory, "x64-debug-null")
        instrumented = check.expected_tests(inventory, "x64-debug-null-asan")
        self.assertEqual(instrumented, ordinary | {"AsanDetectionTests"})


if __name__ == "__main__":
    unittest.main()
