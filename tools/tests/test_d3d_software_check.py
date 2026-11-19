"""Refuse missing, empty, failed and mislabelled software qualification."""

import json
from pathlib import Path
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

from tools import d3d_software_check as checker


def default_xml(names=None):
    selected = sorted(checker.DEFAULT_REQUIRED_CASES) if names is None else names
    return '<testsuites><testsuite>' + ''.join(f'<testcase name="{name}"/>' for name in selected) + '</testsuite></testsuites>'


class SoftwareEvidenceTests(unittest.TestCase):
    def test_actual_name_and_software_kind_are_both_required(self):
        good = "fixture.cpp: MESSAGE: Drawing on Microsoft Basic Render Driver (software)\n"
        self.assertEqual(checker.software_devices(good, installed=False),
                         ["Microsoft Basic Render Driver"])
        for bad in ("", good.replace("software", "hardware"),
                    good.replace("Microsoft Basic Render Driver", "NVIDIA"),
                    good + "Drawing on NVIDIA (hardware)\n"):
            with self.subTest(output=bad), self.assertRaises(ValueError):
                checker.software_devices(bad, installed=False)
        with self.assertRaises(ValueError):
            checker.software_devices(good, installed=True)
        self.assertEqual(checker.software_devices(
            "Direct3D 12 software-test device: Microsoft Basic Render Driver (software)\n",
            installed=True), ["Microsoft Basic Render Driver"])

    def test_default_policy_requires_executed_selected_cases(self):
        with tempfile.TemporaryDirectory() as temporary:
            report = Path(temporary) / "report.xml"
            valid = default_xml()
            report.write_text(valid, encoding="utf-8")
            self.assertEqual(checker.startup_cases(report), sorted(checker.DEFAULT_REQUIRED_CASES))
            for invalid in ('<testsuites/>', valid.replace("D3D STARTUP:", "another case"),
                            valid.replace('/>', '><failure/></testcase>'),
                            valid.replace('/>', '><skipped/></testcase>'),
                            valid.replace('<testsuite>', '<testsuite failures="1">')):
                with self.subTest(xml=invalid):
                    report.write_text(invalid, encoding="utf-8")
                    with self.assertRaises(ValueError):
                        checker.startup_cases(report)
            for names in (["D3D STARTUP: an explicit software-test build creates the real WARP device"],
                          ["D3D STARTUP: unsupported device creation has a typed native diagnostic"],
                          [next(iter(checker.DEFAULT_REQUIRED_CASES))]):
                report.write_text(default_xml(names), encoding="utf-8")
                with self.assertRaisesRegex(ValueError, "default Release"):
                    checker.startup_cases(report)

    def test_every_instrumented_implementation_copy_receives_the_same_policy(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "engine/render/d3d12/device_resources.cpp"
            source.parent.mkdir(parents=True)
            source.write_text("// production implementation\n", encoding="utf-8")
            wrapper = root / "instrumented_device_resources.cpp"
            wrapper.write_text('#include "engine/render/d3d12/device_resources.cpp"\n', encoding="utf-8")
            commands = [{"file": str(source), "command": "cl -DLABRADOR_D3D_SOFTWARE_TEST=1 production.cpp"},
                        {"file": str(wrapper), "command": "cl -DLABRADOR_D3D_SOFTWARE_TEST=1 synchronization.cpp"},
                        {"file": str(wrapper), "command": "cl presented.cpp"}]
            database = root / "compile_commands.json"
            database.write_text(json.dumps(commands), encoding="utf-8")
            with self.assertRaises(ValueError):
                checker.compile_policy(database, enabled=True)
            commands[2]["command"] += " -DLABRADOR_D3D_SOFTWARE_TEST=1"
            database.write_text(json.dumps(commands), encoding="utf-8")
            self.assertEqual(len(checker.compile_policy(database, enabled=True)), 3)
            with self.assertRaises(ValueError):
                checker.compile_policy(database, enabled=False)
            for entry in commands:
                entry["command"] = "cl ordinary.cpp"
            database.write_text(json.dumps(commands), encoding="utf-8")
            self.assertEqual(len(checker.compile_policy(database, enabled=False)), 3)

    def test_invocation_keeps_arguments_separate_and_retains_failure_receipt(self):
        with tempfile.TemporaryDirectory(prefix="d3d evidence ") as temporary:
            root = Path(temporary)
            executable = root / "pixel fixture.exe"
            executable.write_bytes(b"offline executable stand-in")
            report = root / "report.json"

            def run(command, **options):
                self.assertEqual(command[1], "--test-case=D3D STARTUP:*")
                self.assertIn("--no-skip=true", command)
                self.assertEqual(options["cwd"], root)
                self.assertEqual(options["timeout"], 120)
                xml_path = Path(next(argument[6:] for argument in command if argument.startswith("--out=")))
                xml_path.write_text(default_xml(), encoding="utf-8")
                return SimpleNamespace(returncode=0, stdout="", stderr="fixture diagnostic\n")

            with mock.patch.object(checker.subprocess, "run", side_effect=run):
                receipt = checker.check("default-refusal", executable, report)
            self.assertEqual(receipt["result"], "passed")
            self.assertTrue(report.with_suffix(".xml").is_file())
            def fail_from_unrelated_directory(command, **options):
                self.assertEqual(command, [str(executable), "--smoke-test"])
                self.assertEqual(options["cwd"], Path.cwd())
                self.assertNotEqual(options["cwd"], root)
                return SimpleNamespace(returncode=7, stdout="original output", stderr="original cause")

            with mock.patch.object(checker.subprocess, "run", side_effect=fail_from_unrelated_directory):
                with self.assertRaisesRegex(ValueError, "exited 7"):
                    checker.check("installed", executable, report)
            failed = json.loads(report.read_text())
            self.assertEqual(failed["exit"], 7)
            self.assertEqual(failed["stderr"], "original cause")
            self.assertEqual(failed["result"], "failed")

    def test_timeout_is_not_a_software_result(self):
        with tempfile.TemporaryDirectory() as temporary:
            executable = Path(temporary) / "fixture.exe"
            executable.touch()
            with mock.patch.object(checker.subprocess, "run", side_effect=subprocess.TimeoutExpired("fixture", 120)):
                with self.assertRaises(subprocess.TimeoutExpired):
                    checker.check("installed", executable, Path(temporary) / "report.json")
            self.assertEqual(json.loads((Path(temporary) / "report.json").read_text())["result"],
                             "not-completed")
