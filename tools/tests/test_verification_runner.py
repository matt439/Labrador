"""Fake-command sweep tests; configured tools and subprocesses never execute."""

from __future__ import annotations

import contextlib
import copy
import importlib.util
import io
import json
from pathlib import Path
import unittest
from unittest import mock
import xml.etree.ElementTree as ElementTree

import test_verify_changes as fixtures

planner = fixtures.planner
SPEC = importlib.util.spec_from_file_location("verification_runner", planner.REPOSITORY / "tools/verification_runner.py")
sweep = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(sweep)


class RunnerTests(unittest.TestCase):
    def setUp(self):
        fixture = fixtures.PlanTests(methodName="runTest")
        fixture.setUp()
        self.addCleanup(fixture.doCleanups)
        self.fixture = fixture
        self.root = fixture.root
        self.environment, self.tools, self.browser = fixture.fake_tooling()
        self.plan = fixture.plan()
        fixture.write_caches(self.plan, self.environment, self.browser)
        self.trace = []
        self.output = self.root / "reports with spaces" / "run"
        for name in ("run", "Popen"):
            patch = mock.patch.object(sweep.subprocess, name, side_effect=AssertionError("real subprocess forbidden"))
            patch.start()
            self.addCleanup(patch.stop)

    def inspect(self, plan, **options):
        return self.fixture.inspect(plan, self.environment, self.tools, **options)

    def cache(self, name):
        return self.root / "out/build" / name / "CMakeCache.txt"

    def names(self, configuration):
        return list(dict.fromkeys(configuration["required_test_names"]
                                  + configuration["required_browser_test_names"]
                                  + configuration["conditional_browser_test_names"] + ["AdditionalTests"]))

    def registrations(self, configuration):
        return {"kind": "ctestInfo", "version": {"major": 1, "minor": 0},
                "tests": [{"name": name, "command": ["C:/fixture tools/test.exe", "argument with spaces"]}
                          for name in self.names(configuration)]}

    def junit(self, names):
        document = ElementTree.Element("testsuite", tests=str(len(names)), failures="0", errors="0", skipped="0")
        for name in names:
            case = ElementTree.SubElement(document, "testcase", name=name, status="run")
            ElementTree.SubElement(case, "system-out").text = "fixture output only"
        return ElementTree.tostring(document, encoding="unicode")

    def fake_runner(self, argv, *, cwd, stdout, stderr):
        if argv[0] == "cmake":
            phase = "build" if "--build" in argv else "configure"
        else:
            phase = "registration" if "--show-only=json-v1" in argv else "test"
        if "--preset" in argv:
            name = argv[argv.index("--preset") + 1]
            if name == "wasm-release":
                name = "wasm-release-null"
        else:
            selector = "--build" if phase == "build" else "--test-dir"
            name = Path(argv[argv.index(selector) + 1]).name
        self.trace.append((name, phase, list(argv), cwd))
        configuration = next(item for item in self.plan["configurations"] if item["id"] == name)
        stderr.write("fixture diagnostic\n")
        if phase == "registration":
            stdout.write(json.dumps(self.registrations(configuration)))
        else:
            stdout.write("fixture command output\n")
        if phase == "test":
            path = Path(argv[argv.index("--output-junit") + 1])
            self.assertFalse(path.exists(), "result path must be fresh")
            path.write_text(self.junit(self.names(configuration)), encoding="utf-8")
        return 0

    def execute(self, runner=None, inspector=None, output=None):
        return sweep.execute_plan(self.plan, inspector=inspector or self.inspect, cache_reader=planner.cache_values,
                                  runner=runner or self.fake_runner, output_dir=output or self.output)

    def assert_failure(self, report, phase, name="x64-debug", commands=None):
        self.assertEqual(report["result"], "failed")
        self.assertEqual(report["exit_code"], 1)
        self.assertEqual(report["failure"]["phase"], phase)
        self.assertEqual(report["failure"]["configuration"], name)
        if commands is not None:
            self.assertEqual(len(self.trace), commands)

    def test_all_seven_tracks_run_serially_with_exact_selectors_and_fresh_logs(self):
        before = copy.deepcopy(self.plan)
        report = self.execute()
        self.assertEqual((report["result"], report["exit_code"], report["failure"]), ("passed", 0, None))
        self.assertEqual(self.plan, before)
        self.assertEqual([(name, phase) for name, phase, _, _ in self.trace],
                         [(name, phase) for name in planner.CONFIGURATIONS
                          for phase in ("configure", "build", "registration", "test")])
        for name, phase, argv, cwd in self.trace:
            self.assertEqual(cwd, str(self.root))
            if phase in {"build", "test"}:
                self.assertEqual(argv[argv.index("--parallel") + 1], "2" if phase == "build" else "1")
            if phase == "test":
                self.assertIn("--stop-on-failure", argv)
            if phase == "registration":
                self.assertEqual(argv[-1], "--show-only=json-v1")
                self.assertNotIn("--parallel", argv)
            if name == "wasm-release-null" and phase != "configure":
                self.assertNotIn("--preset", argv)
                self.assertIn("out/build/wasm-release-null", argv)
        self.assertEqual(report["source_revision"], "uncollected")
        self.assertEqual(report["device_identity"], "uncollected")
        self.assertTrue(all(item["status"] == "not-run" for item in report["manual_obligations"]))
        self.assertNotIn("registration-before-future-execution", [item["id"] for item in report["manual_obligations"]])
        self.assertEqual(json.loads(Path(report["report_path"]).read_text()), report)
        for configuration in report["configurations"]:
            self.assertEqual({item["result"] for item in configuration["validations"].values()}, {"passed"})
            self.assertTrue(all(Path(step["stdout_path"]).is_file() for step in configuration["steps"]))

    def test_missing_late_prerequisite_blocks_every_command_before_output_creation(self):
        def inspect(plan, **options):
            return [{"id": "EMSDK", "configurations": ["wasm-debug"], "blocking": True,
                     "detail": "fixture missing late prerequisite"}]
        report = self.execute(inspector=inspect)
        self.assert_failure(report, "preflight", None, 0)
        self.assertFalse(self.output.exists())
        self.assertTrue(all(step["result"] == "not-run" for item in report["configurations"] for step in item["steps"]))

    def test_preflight_read_failure_is_reported_without_a_command(self):
        report = self.execute(inspector=mock.Mock(side_effect=OSError("fixture denied cache read")))
        self.assert_failure(report, "preflight", None, 0)
        self.assertIn("fixture denied", report["failure"]["detail"])

    def test_existing_output_directory_is_never_overwritten(self):
        self.output.mkdir(parents=True)
        sentinel = self.output / "report.json"
        sentinel.write_text("existing owner report")
        report = self.execute()
        self.assert_failure(report, "output-directory", None, 0)
        self.assertEqual(sentinel.read_text(), "existing owner report")

    def test_relative_output_directory_anchors_to_checkout(self):
        report = self.execute(output="out/relative-report")
        self.assertEqual(report["output_dir"], str((self.root / "out/relative-report").resolve()))

    def test_default_output_directory_is_unique_under_ignored_out(self):
        reports = [sweep.execute_plan(self.plan, inspector=self.inspect, cache_reader=planner.cache_values,
                                      runner=self.fake_runner) for _ in range(2)]
        self.assertNotEqual(reports[0]["output_dir"], reports[1]["output_dir"])
        self.assertTrue(all(Path(item["output_dir"]).parent == self.root / "out/verification" for item in reports))

    def test_first_command_failure_stops_each_phase_at_early_and_late_tracks(self):
        for target in ("x64-debug", "wasm-release-null"):
            for target_phase in ("configure", "build", "registration", "test"):
                with self.subTest(target=target, phase=target_phase):
                    self.trace = []
                    def run(argv, **options):
                        result = self.fake_runner(argv, **options)
                        return 17 if self.trace[-1][:2] == (target, target_phase) else result
                    output = self.root / "failures" / (target + target_phase)
                    report = self.execute(runner=run, output=output)
                    self.assert_failure(report, target_phase, target)
                    self.assertEqual(self.trace[-1][:2], (target, target_phase))
                    following = [item for item in report["configurations"]
                                 if planner.CONFIGURATIONS.index(item["id"]) > planner.CONFIGURATIONS.index(target)]
                    self.assertTrue(all(step["result"] == "not-run" for item in following for step in item["steps"]))

    def test_launch_error_is_logged_and_later_phases_remain_unrun(self):
        def run(argv, **options):
            self.fake_runner(argv, **options)
            raise OSError("fixture executable unavailable")
        report = self.execute(runner=run)
        self.assert_failure(report, "configure", commands=1)
        self.assertEqual(report["configurations"][0]["steps"][1]["result"], "not-run")
        self.assertTrue(Path(report["report_path"]).is_file())

    def test_interrupt_has_exit_130_and_no_descendant_teardown_claim(self):
        def run(argv, **options):
            self.fake_runner(argv, **options)
            raise KeyboardInterrupt()
        report = self.execute(runner=run)
        self.assertEqual((report["result"], report["exit_code"]), ("interrupted", 130))
        self.assertIn("not inspected", report["failure"]["detail"])
        self.assertEqual(len(self.trace), 1)
        self.assertEqual(report["configurations"][0]["steps"][0]["result"], "interrupted")

    def test_zero_configure_exit_still_requires_new_cache_to_exist(self):
        self.cache("x64-debug").unlink()
        report = self.execute()
        self.assert_failure(report, "cache", commands=1)
        self.assertIn("cache", report["failure"]["detail"])
        self.assertEqual(report["configurations"][0]["steps"][0]["result"], "passed")

    def test_cache_axes_are_rechecked_after_configure_before_build(self):
        def run(argv, **options):
            result = self.fake_runner(argv, **options)
            path = self.cache("x64-debug")
            path.write_text(path.read_text().replace("BACKEND:STRING=d3d11", "BACKEND:STRING=gl"))
            return result
        report = self.execute(runner=run)
        self.assert_failure(report, "cache", commands=1)
        self.assertIn("LABRADOR_RENDER_BACKEND", report["failure"]["detail"])

    def test_explicit_browser_cache_must_match_requested_override(self):
        self.plan = self.fixture.plan(browser="C:/different/chrome.exe")
        report = self.execute()
        self.assert_failure(report, "preflight", None, 0)
        self.assertIn("LABRADOR_TEST_BROWSER", report["failure"]["detail"])

    def test_browser_cache_is_rechecked_after_configure(self):
        self.plan = self.fixture.plan(browser=self.browser)
        def run(argv, **options):
            result = self.fake_runner(argv, **options)
            name, phase = self.trace[-1][:2]
            if name == "wasm-debug" and phase == "configure":
                path = self.cache(name)
                path.write_text(path.read_text().replace(self.browser, self.tools["node"]))
            return result
        report = self.execute(runner=run)
        self.assert_failure(report, "cache", "wasm-debug")
        self.assertEqual(self.trace[-1][:2], ("wasm-debug", "configure"))
        self.assertIn("LABRADOR_TEST_BROWSER", report["failure"]["detail"])

    def test_build_automatic_reconfigure_cannot_change_backend_before_tests(self):
        def run(argv, **options):
            result = self.fake_runner(argv, **options)
            if self.trace[-1][1] == "build":
                path = self.cache("x64-debug")
                path.write_text(path.read_text().replace("BACKEND:STRING=d3d11", "BACKEND:STRING=gl"))
            return result
        report = self.execute(runner=run)
        self.assert_failure(report, "post-build-cache", commands=2)
        self.assertIn("LABRADOR_RENDER_BACKEND", report["failure"]["detail"])
        self.assertEqual(report["configurations"][0]["steps"][1]["result"], "passed")

    def test_registration_errors_stop_before_test_command(self):
        data = self.registrations(self.plan["configurations"][0])
        variants = ["warning before JSON\n" + json.dumps(data), "{}", "{",
                    json.dumps({**data, "tests": []}),
                    json.dumps({**data, "tests": data["tests"][1:]}),
                    json.dumps({**data, "tests": data["tests"] + [data["tests"][0]]}),
                    json.dumps({**data, "kind": "other"}),
                    json.dumps({**data, "version": {"major": True, "minor": False}}),
                    json.dumps({**data, "version": {"major": 2, "minor": 0}})]
        for index, text in enumerate(variants):
            with self.subTest(index=index):
                self.trace = []
                def run(argv, **options):
                    result = self.fake_runner(argv, **options)
                    if self.trace[-1][1] == "registration":
                        options["stdout"].seek(0)
                        options["stdout"].truncate()
                        options["stdout"].write(text)
                    return result
                report = self.execute(runner=run, output=self.root / "bad-registration" / str(index))
                self.assert_failure(report, "registration", commands=3)

    def test_registration_rejects_missing_commands_duplicate_properties_and_disabled_tests(self):
        configuration = self.plan["configurations"][0]
        mutations = [lambda test: test.pop("command"), lambda test: test.update(command=[]),
                     lambda test: test.update(command=[17]),
                     lambda test: test.update(properties=[{"name": "DISABLED", "value": True}]),
                     lambda test: test.update(properties=[{"name": "DISABLED", "value": "ON"}]),
                     lambda test: test.update(properties=[{"name": "DISABLED", "value": False},
                                                         {"name": "DISABLED", "value": False}]),
                     lambda test: test.update(properties=[{"name": "DISABLED"}]),
                     lambda test: test.update(properties={"DISABLED": False})]
        for mutation in mutations:
            with self.subTest(mutation=mutation):
                data = self.registrations(configuration)
                mutation(data["tests"][0])
                with self.assertRaises(sweep.VerificationError):
                    sweep.registration_inventory(json.dumps(data), configuration, selected_browser=False)

    def test_registration_false_constants_do_not_disable_active_tests(self):
        configuration = self.plan["configurations"][0]
        for value in (False, 0, 0.0, "FALSE", "OFF", "NO", "N", "0", "IGNORE", "NOTFOUND", "x-NOTFOUND", ""):
            with self.subTest(value=value):
                data = self.registrations(configuration)
                data["tests"][0]["properties"] = [{"name": "DISABLED", "value": value}]
                self.assertEqual(len(sweep.registration_inventory(json.dumps(data), configuration, selected_browser=False)),
                                 len(data["tests"]))
        for value in (True, 1, -3, "TRUE", "ON", "YES", "Y", "1", "unmodeled", [], None):
            with self.subTest(value=value), self.assertRaises(sweep.VerificationError):
                data = self.registrations(configuration)
                data["tests"][0]["properties"] = [{"name": "DISABLED", "value": value}]
                sweep.registration_inventory(json.dumps(data), configuration, selected_browser=False)

    def test_duplicate_json_keys_and_nonfinite_constants_are_not_accepted(self):
        configuration = self.plan["configurations"][0]
        for text in ('{"kind":"ctestInfo","kind":"ctestInfo"}', '{"tests":NaN}'):
            with self.subTest(text=text), self.assertRaises(sweep.VerificationError):
                sweep.registration_inventory(text, configuration, selected_browser=False)

    def test_null_render_forbids_all_rasterizer_pages(self):
        for configuration in (self.plan["configurations"][4], self.plan["configurations"][-1]):
            for name in configuration["forbidden_test_names"]:
                with self.subTest(configuration=configuration["id"], name=name):
                    data = self.registrations(configuration)
                    data["tests"].append({"name": name, "command": ["fixture"]})
                    with self.assertRaisesRegex(sweep.VerificationError, "forbidden"):
                        sweep.registration_inventory(json.dumps(data), configuration, selected_browser=True)

    def test_selected_or_partially_registered_null_browser_requires_complete_conditional_pages(self):
        configuration = self.plan["configurations"][-1]
        data = self.registrations(configuration)
        conditional = set(configuration["conditional_browser_test_names"])
        data["tests"] = [item for item in data["tests"] if item["name"] not in conditional]
        self.assertTrue(sweep.registration_inventory(json.dumps(data), configuration, selected_browser=False))
        with self.assertRaisesRegex(sweep.VerificationError, "missing required"):
            sweep.registration_inventory(json.dumps(data), configuration, selected_browser=True)
        data["tests"].append({"name": next(iter(conditional)), "command": ["fixture"]})
        with self.assertRaisesRegex(sweep.VerificationError, "missing required"):
            sweep.registration_inventory(json.dumps(data), configuration, selected_browser=False)

    def test_missing_required_gl_browser_page_is_not_covered_by_other_tests(self):
        configuration = self.plan["configurations"][-2]
        data = self.registrations(configuration)
        data["tests"] = [item for item in data["tests"] if item["name"] != "LineSweeperBundlePageTests"]
        with self.assertRaisesRegex(sweep.VerificationError, "LineSweeperBundlePageTests"):
            sweep.registration_inventory(json.dumps(data), configuration, selected_browser=True)

    def test_zero_exit_runtime_result_errors_stop_before_next_configuration(self):
        names = self.names(self.plan["configurations"][0])
        variants = ["not XML", "<unknown/>", "<testsuite/>", self.junit(names[1:]),
                    self.junit(names + [names[0]]), self.junit(names + ["UnexpectedTests"]),
                    self.junit(names).replace('status="run"', 'status="notrun"', 1),
                    self.junit(names).replace('status="run"', 'status="unknown"', 1),
                    self.junit(names).replace('<system-out>', '<skipped message="fixture skip"/><system-out>', 1),
                    self.junit(names).replace('<system-out>', '<failure message="fixture fail"/><system-out>', 1),
                    self.junit(names).replace('<system-out>', '<error message="fixture error"/><system-out>', 1),
                    self.junit(names).replace('failures="0"', 'failures="1"'),
                    self.junit(names).replace('skipped="0"', 'skipped="1"'),
                    self.junit(names).replace('errors="0"', 'errors="1"'),
                    self.junit(names).replace('tests="' + str(len(names)) + '"', 'tests="999"')]
        for index, text in enumerate(variants):
            with self.subTest(index=index):
                self.trace = []
                def run(argv, **options):
                    result = self.fake_runner(argv, **options)
                    if self.trace[-1][1] == "test":
                        Path(argv[argv.index("--output-junit") + 1]).write_text(text)
                    return result
                report = self.execute(runner=run, output=self.root / "bad-junit" / str(index))
                self.assert_failure(report, "test-results", commands=4)
                self.assertEqual(report["configurations"][0]["steps"][2]["result"], "passed")

    def test_missing_new_junit_never_reads_stale_success(self):
        stale = self.root / "out/build/x64-debug/junit.xml"
        stale.write_text(self.junit(self.names(self.plan["configurations"][0])))
        def run(argv, **options):
            result = self.fake_runner(argv, **options)
            if self.trace[-1][1] == "test":
                Path(argv[argv.index("--output-junit") + 1]).unlink()
            return result
        report = self.execute(runner=run)
        self.assert_failure(report, "test-results", commands=4)
        self.assertTrue(stale.is_file())

    def test_single_suite_container_and_standard_absent_case_status_are_supported(self):
        names = {"FirstTests", "SecondTests"}
        xml = '<testsuites tests="2" failures="0">' + self.junit(sorted(names)) + '</testsuites>'
        self.assertEqual({item["name"] for item in sweep.junit_results(xml, names)}, names)
        self.assertEqual(len(sweep.junit_results(self.junit(sorted(names)).replace(' status="run"', ''), names)), 2)

    def test_multiple_suites_and_unknown_xml_children_are_rejected(self):
        for xml in ('<testsuites><testsuite/><testsuite/></testsuites>',
                    '<testsuite><unknown/></testsuite>',
                    '<testsuite><testcase name="OnlyTests"><unknown/></testcase></testsuite>'):
            with self.subTest(xml=xml), self.assertRaises(sweep.VerificationError):
                sweep.junit_results(xml, {"OnlyTests"})

    def test_report_write_denial_is_visible_without_hiding_first_command_failure(self):
        original_write = Path.write_text
        def deny_report(path, *arguments, **options):
            if path.name == "report.json":
                raise PermissionError("fixture report write denied")
            return original_write(path, *arguments, **options)
        def run(argv, **options):
            self.fake_runner(argv, **options)
            return 7
        with mock.patch.object(Path, "write_text", deny_report):
            report = self.execute(runner=run)
        self.assert_failure(report, "configure", commands=1)
        self.assertIsNone(report["report_path"])
        self.assertEqual(report["intended_report_path"], str(self.output / "report.json"))
        self.assertIn("fixture report write denied", report["report_write_error"])
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            code = planner.main(["--area", "render", "--execute"], root=self.root,
                                executor=lambda *arguments, **options: report)
        self.assertEqual(code, 1)
        self.assertIn("Report: not written", output.getvalue())
        self.assertIn("Report write failed: fixture report write denied", output.getvalue())

    def test_cli_explicit_execution_uses_injected_executor_without_printing_command_stdout(self):
        def execute(plan, **options):
            return self.execute()
        output, errors = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors):
            code = planner.main(["--area", "render", "--execute", "--format", "json"],
                                root=self.root, inspector=self.inspect, executor=execute)
        report = json.loads(output.getvalue())
        self.assertEqual((code, errors.getvalue()), (0, ""))
        self.assertEqual(report["mode"], "execute")
        self.assertNotIn("fixture command output", output.getvalue())

    def test_default_cli_does_not_call_injected_executor_and_output_requires_execute(self):
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(planner.main(["--area", "render"], root=self.root,
                                           executor=mock.Mock(side_effect=AssertionError("executor"))), 0)
            self.assertEqual(planner.main(["--area", "render", "--output-dir", "new"], root=self.root,
                                           executor=mock.Mock(side_effect=AssertionError("executor"))), 2)
            self.assertEqual(planner.main(["--area", "render", "--execute", "--preflight"], root=self.root,
                                           executor=mock.Mock(side_effect=AssertionError("executor"))), 2)


if __name__ == "__main__":
    unittest.main()
