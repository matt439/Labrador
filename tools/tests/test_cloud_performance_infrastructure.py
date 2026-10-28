"""Offline infrastructure tests using synthetic data and local fixtures."""

from __future__ import annotations

import copy
import json
import os
import shutil
import subprocess
import tempfile
import unittest
from datetime import datetime, timezone
from pathlib import Path
from unittest.mock import Mock

from tools.cloud_performance import __main__ as cli
from tools.cloud_performance import infrastructure
from tools.tests.cloud_performance_fixtures import config, write_json


class InfrastructureTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which("powershell"), "Windows PowerShell is required")
    def test_worker_early_refusal_shuts_down_only_after_identity_matches(self):
        for wrong_identity, upload_failure in ((False, False), (True, False), (False, True)):
            with self.subTest(wrong_identity=wrong_identity, upload_failure=upload_failure):
                with tempfile.TemporaryDirectory() as temporary:
                    root = Path(temporary)
                    write_json(root / "declaration.json", config())
                    commands = [
                        shutil.which("powershell"), "-NoProfile", "-NonInteractive", "-File",
                        str(Path(__file__).with_name("worker_failure_fixture.ps1")),
                        "-FixtureRoot", str(root), "-WorkerSource",
                        str(Path(__file__).parents[1] / "cloud_performance" / "worker.ps1"),
                    ]
                    if wrong_identity:
                        commands.append("-WrongIdentity")
                    if upload_failure:
                        commands.append("-UploadFailure")
                    # Let Windows PowerShell rebuild its own module paths;
                    # a pwsh parent can supply incompatible PowerShell 7 modules.
                    env = {key: value for key, value in os.environ.items()
                           if key.casefold() != "psmodulepath"}
                    process = subprocess.run(commands, env=env, capture_output=True, timeout=30)
                    output = (process.stdout + process.stderr).decode("utf-8", errors="replace")
                    self.assertNotEqual(process.returncode, 0, output)
                    shutdown = root / "shutdown.json"
                    self.assertEqual(shutdown.exists(), not wrong_identity, output)
                    if shutdown.exists():
                        self.assertEqual(json.loads(shutdown.read_text(encoding="utf-8-sig")),
                                         ["/s", "/t", 0, "/f"])
                    if not upload_failure:
                        terminal = json.loads((root / "work/evidence/failure.json").read_text(
                            encoding="utf-8-sig"))
                        self.assertEqual(terminal["status"], "failure")
                        self.assertIn("AMI differs" if wrong_identity else "early processor refusal",
                                      terminal["error"])

    def test_ami_must_have_the_declared_console_user(self):
        document = config()
        tags = document["expected_ami_tags"]
        for actual in (None, "other-user", " "):
            with self.subTest(actual=actual):
                image_tags = dict(tags)
                if actual is None:
                    del image_tags["ConsoleUser"]
                else:
                    image_tags["ConsoleUser"] = actual
                ec2 = Mock()
                ec2.describe_images.return_value = {"Images": [{
                    "State": "available", "Architecture": "x86_64", "Platform": "windows",
                    "RootDeviceName": document["root_device_name"],
                    "Tags": [{"Key": key, "Value": value} for key, value in image_tags.items()],
                }]}
                with self.assertRaisesRegex(ValueError, "ConsoleUser"):
                    cli._verify_ami(ec2, document)
                ec2.describe_instance_type_offerings.assert_not_called()

    def test_terminated_instance_purge_is_recognized(self):
        error = RuntimeError("gone")
        error.response = {"Error": {"Code": "InvalidInstanceID.NotFound"}}

        self.assertTrue(cli._missing_instance(error))

    def test_worker_uses_single_backslash_path_normalization(self):
        worker = (Path(__file__).parents[1] / "cloud_performance" / "worker.ps1").read_text()

        self.assertIn(r".TrimStart('\').Replace('\', '/')", worker)
        self.assertIn(r"$Relative.Replace('/', '\')", worker)
        self.assertNotIn(r".TrimStart('\\').Replace('\\', '/')", worker)

    def test_template_is_one_on_demand_terminated_instance(self):
        now = datetime(2030, 1, 1, tzinfo=timezone.utc)
        rendered = infrastructure.template(config(now), now=now)
        resources = rendered["Resources"]
        instances = [value for value in resources.values() if value["Type"] == "AWS::EC2::Instance"]
        self.assertEqual(len(instances), 1)
        self.assertFalse(any(value["Type"] in {
            "AWS::AutoScaling::AutoScalingGroup", "AWS::EC2::LaunchTemplate"
        } for value in resources.values()))
        properties = instances[0]["Properties"]
        self.assertNotIn("InstanceMarketOptions", properties)
        self.assertEqual(properties["CpuOptions"], {"CoreCount": 4, "ThreadsPerCore": 1})
        self.assertEqual(properties["InstanceInitiatedShutdownBehavior"], "terminate")
        self.assertEqual(properties["MetadataOptions"]["HttpTokens"], "required")
        self.assertEqual(properties["MetadataOptions"]["HttpPutResponseHopLimit"], 1)
        root = properties["BlockDeviceMappings"][0]["Ebs"]
        self.assertTrue(root["Encrypted"])
        self.assertTrue(root["DeleteOnTermination"])
        group = resources["RunnerSecurityGroup"]["Properties"]
        self.assertNotIn("SecurityGroupIngress", group)

    def test_template_scopes_evidence_and_has_independent_watchdog(self):
        now = datetime(2030, 1, 1, tzinfo=timezone.utc)
        rendered = infrastructure.template(config(now), now=now)
        resources = rendered["Resources"]
        statements = resources["RunnerRole"]["Properties"]["Policies"][0][
            "PolicyDocument"]["Statement"]
        encoded = json.dumps(statements)
        self.assertIn("labrador-performance/runs/reference-001/*", encoded)
        self.assertNotIn('"Resource": "*"', encoded)
        code = resources["Watchdog"]["Properties"]["Code"]["ZipFile"]
        compile(code, "watchdog", "exec")
        self.assertIn("terminate_instances", code)
        self.assertEqual(resources["WatchdogTick"]["Properties"]["ScheduleExpression"],
                         "rate(1 minute)")

    def test_execute_requires_explicit_authorization(self):
        now = datetime(2030, 1, 1, tzinfo=timezone.utc)
        with self.assertRaisesRegex(ValueError, "authorization"):
            infrastructure.template(config(now), now=now, require_authorization=True)

    def test_destructive_target_requires_exact_stack_identity(self):
        class Paginator:
            def __init__(self, resources):
                self.resources = resources

            def paginate(self, **kwargs):
                self.stack_name = kwargs["StackName"]
                return [{"StackResourceSummaries": self.resources}]

        class CloudFormation:
            def __init__(self, stack, resources):
                self.stack = stack
                self.paginator = Paginator(resources)

            def describe_stacks(self, **kwargs):
                self.requested = kwargs["StackName"]
                return {"Stacks": [self.stack]}

            def get_paginator(self, name):
                self.asserted_name = name
                return self.paginator

        stack = {
            "StackName": "labrador-performance-reference-001",
            "StackId": "stack-id",
            "Tags": [{"Key": "Project", "Value": "Labrador"},
                     {"Key": "RunId", "Value": "reference-001"}],
            "Outputs": [{"OutputKey": "RunId", "OutputValue": "reference-001"},
                        {"OutputKey": "RunnerInstanceId", "OutputValue": "i-runner"}],
        }
        resources = [{"LogicalResourceId": "Runner", "ResourceType": "AWS::EC2::Instance",
                      "PhysicalResourceId": "i-runner"}]
        _, instance = cli._validated_runner(
            CloudFormation(copy.deepcopy(stack), copy.deepcopy(resources)), stack["StackName"])
        self.assertEqual(instance, "i-runner")
        wrong = copy.deepcopy(stack)
        wrong["Tags"][0]["Value"] = "AnotherProject"
        with self.assertRaisesRegex(ValueError, "identity"):
            cli._validated_runner(CloudFormation(wrong, resources), stack["StackName"])
        with self.assertRaisesRegex(ValueError, "namespace"):
            cli._validated_runner(CloudFormation(stack, resources), "unrelated-stack")


if __name__ == "__main__":
    unittest.main()
