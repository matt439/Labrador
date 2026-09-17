"""Offline failure-boundary tests; no AWS calls, credentials or instances."""

from __future__ import annotations

import copy
import hashlib
import json
import subprocess
import tempfile
import unittest
import zipfile
from datetime import datetime, timedelta, timezone
from pathlib import Path

from tools.cloud_performance import __main__ as cli
from tools.cloud_performance import analysis, infrastructure, release
from tools.cloud_performance.common import canonical, load_config, validate_config


def config(now: datetime | None = None) -> dict:
    current = now or datetime.now(timezone.utc)
    return {
        "schema_version": 1,
        "run_id": "reference-001",
        "region": "ap-southeast-2",
        "availability_zone": "ap-southeast-2a",
        "ami_id": "ami-0123456789abcdef0",
        "instance_type": "g6f.2xlarge",
        "cpu_options": {"core_count": 4, "threads_per_core": 1},
        "vpc_id": "vpc-0123456789abcdef0",
        "subnet_id": "subnet-0123456789abcdef0",
        "artifact_bucket": "labrador-private-evidence",
        "artifact_prefix": "labrador-performance",
        "bundle": {
            "path": "out/cloud/payload.zip",
            "sha256": "a" * 64,
            "release_sha256": "b" * 64,
        },
        "expected_ami_tags": {
            "Project": "Labrador", "ImageRole": "performance-runner-v1",
            "WindowsBuild": "20348", "NvidiaDriver": "555.1",
        },
        "root_device_name": "/dev/sda1",
        "root_volume_gib": 100,
        "associate_public_ip": True,
        "authorization": {
            "allow_launch": False,
            "deadline_utc": (current + timedelta(hours=1)).isoformat(),
            "max_elapsed_hours": 2,
            "max_hourly_usd": 2,
            "max_compute_usd": 3,
        },
        "workload": {
            "width": 1280,
            "height": 720,
            "live_particles": 9600,
            "warmup_frames": 120,
            "sample_frames": 100,
            "refresh_hz": 60,
            "repetitions": 2,
            "backends": [{
                "name": "d3d11",
                "executable": "payload/d3d11/LineSweeperFrameBench.exe",
                "expected_device": "NVIDIA.*L4",
                "expected_vendor_id": 0x10DE,
            }],
        },
    }


def write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value), encoding="utf-8")


def evidence_manifest(root: Path) -> dict[str, dict[str, int | str]]:
    manifest = {}
    for path in sorted(root.rglob("*")):
        if not path.is_file() or path.name in {"success.json", "failure.json"}:
            continue
        content = path.read_bytes()
        manifest[path.relative_to(root).as_posix()] = {
            "bytes": len(content), "sha256": hashlib.sha256(content).hexdigest(),
        }
    return manifest


def refresh_terminal_manifest(root: Path) -> None:
    marker_path = root / "success.json"
    marker = json.loads(marker_path.read_text())
    marker["evidence_files"] = evidence_manifest(root)
    write_json(marker_path, marker)


def phase_summary(values: list[int]) -> dict[str, int]:
    ordered = sorted(values)

    def percentile(numerator: int, denominator: int) -> int:
        rank = max(1, (len(ordered) * numerator + denominator - 1) // denominator)
        return ordered[rank - 1]

    return {"count": len(values), "min": ordered[0], "p50": percentile(50, 100),
            "p95": percentile(95, 100), "p99": percentile(99, 100), "max": ordered[-1]}


class ConfigTests(unittest.TestCase):
    def test_valid_config_is_normalized(self):
        now = datetime(2030, 1, 1, tzinfo=timezone.utc)
        document = config(now)
        document["bundle"]["sha256"] = "A" * 64
        document["workload"]["backends"][0]["expected_vendor_id"] = 0
        actual = validate_config(document, now=now)
        self.assertEqual(actual["bundle"]["sha256"], "a" * 64)
        self.assertEqual(actual["cpu_options"], {"core_count": 4, "threads_per_core": 1})
        self.assertEqual(actual["workload"]["backends"][0]["expected_vendor_id"], 0)

    def test_windows_powershell_utf8_bom_is_accepted(self):
        now = datetime(2030, 1, 1, tzinfo=timezone.utc)
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "run.json"
            path.write_bytes(b"\xef\xbb\xbf" + json.dumps(config(now)).encode("utf-8"))

            actual = load_config(path, now=now)

        self.assertEqual(actual["run_id"], "reference-001")

    def test_external_and_ambiguous_paths_are_refused(self):
        now = datetime(2030, 1, 1, tzinfo=timezone.utc)
        for path in ("../payload.zip", "/payload.zip", "C:/payload.zip",
                     "out/payload.zip:stream", "out\\payload.zip"):
            document = config(now)
            document["bundle"]["path"] = path
            with self.subTest(path=path), self.assertRaisesRegex(ValueError, "path|root"):
                validate_config(document, now=now)

    def test_expired_deadline_and_excess_budget_are_refused(self):
        now = datetime(2030, 1, 1, tzinfo=timezone.utc)
        expired = config(now)
        expired["authorization"]["deadline_utc"] = (now - timedelta(seconds=1)).isoformat()
        with self.assertRaisesRegex(ValueError, "expired"):
            validate_config(expired, now=now)
        expensive = config(now)
        expensive["authorization"]["max_hourly_usd"] = 10
        expensive["authorization"]["max_compute_usd"] = 1
        with self.assertRaisesRegex(ValueError, "budget"):
            validate_config(expensive, now=now)

    def test_unknown_field_is_refused_in_versioned_schema(self):
        now = datetime(2030, 1, 1, tzinfo=timezone.utc)
        document = config(now)
        document["workload"]["sample_farmes"] = 100
        with self.assertRaisesRegex(ValueError, "unexpected workload field"):
            validate_config(document, now=now)

        missing_driver = config(now)
        del missing_driver["expected_ami_tags"]["NvidiaDriver"]
        with self.assertRaisesRegex(ValueError, "Windows build and driver"):
            validate_config(missing_driver, now=now)

    def test_cloud_workload_cannot_drift_from_the_native_reference(self):
        now = datetime(2030, 1, 1, tzinfo=timezone.utc)
        document = config(now)
        document["workload"]["width"] = 1920

        with self.assertRaisesRegex(ValueError, "fixed 1280x720"):
            validate_config(document, now=now)

        too_many = config(now)
        too_many["workload"]["sample_frames"] = 100_001
        with self.assertRaisesRegex(ValueError, "100000"):
            validate_config(too_many, now=now)

        too_short = config(now)
        too_short["authorization"]["deadline_utc"] = (
            now + timedelta(minutes=4)).isoformat()
        with self.assertRaisesRegex(ValueError, "frame matrix"):
            validate_config(too_short, now=now)

        headless = config(now)
        headless["workload"]["backends"][0]["name"] = "null"
        with self.assertRaisesRegex(ValueError, "raster backend"):
            validate_config(headless, now=now)

        gl = config(now)
        gl["workload"]["backends"][0]["name"] = "gl"
        with self.assertRaisesRegex(ValueError, "no PCI ID"):
            validate_config(gl, now=now)


class InfrastructureTests(unittest.TestCase):
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


class ReleaseTests(unittest.TestCase):
    def test_bundle_records_dirty_source_and_detects_tamper(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            subprocess.run(["git", "init"], cwd=root, check=True, stdout=subprocess.DEVNULL)
            (root / "engine").mkdir()
            (root / "engine" / "tracked.cpp").write_text("tracked", encoding="utf-8")
            (root / "CMakeLists.txt").write_text("project(Test)", encoding="utf-8")
            subprocess.run(["git", "add", "."], cwd=root, check=True)
            subprocess.run(["git", "-c", "user.name=Test", "-c", "user.email=test@example.com",
                            "commit", "-m", "base"], cwd=root, check=True,
                           stdout=subprocess.DEVNULL)
            (root / "engine" / "dirty.cpp").write_text("uncommitted", encoding="utf-8")
            payload = root / "payload"
            payload.mkdir()
            (payload / "LineSweeperFrameBench.exe").write_bytes(b"binary")
            archive = root / "out" / "bundle.zip"
            result = release.bundle(payload, archive, root=root)
            manifest = release.verify(archive, result["archive_sha256"])
            self.assertTrue(manifest["git"]["dirty"])
            self.assertIn("source/engine/dirty.cpp", manifest["files"])
            self.assertIn("payload/LineSweeperFrameBench.exe", manifest["files"])
            release.verify_workspace_snapshot(manifest, root)
            (root / "engine" / "tracked.cpp").write_text("changed", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "source drift"):
                release.verify_workspace_snapshot(manifest, root)
            tampered = root / "tampered.zip"
            with zipfile.ZipFile(archive) as source, zipfile.ZipFile(tampered, "w") as output:
                for member in source.infolist():
                    data = source.read(member.filename)
                    output.writestr(member.filename, b"changed" if member.filename.endswith(".exe") else data)
            with self.assertRaisesRegex(ValueError, "drift"):
                release.verify(tampered)

    def test_archive_traversal_and_output_overwrite_are_refused(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive = root / "bad.zip"
            with zipfile.ZipFile(archive, "w") as output:
                output.writestr("../escape", b"bad")
            with self.assertRaisesRegex(ValueError, "root"):
                release.verify(archive)
            payload = root / "payload"
            payload.mkdir()
            (payload / "operator.pem").write_text("secret", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "credentials"):
                release.payload_files(payload)

    def test_declared_backend_requires_executable_and_both_assets(self):
        backend = config()["workload"]["backends"]
        manifest = {"files": {
            "payload/d3d11/LineSweeperFrameBench.exe": {},
        }}
        with self.assertRaisesRegex(ValueError, "runtime input"):
            release.verify_benchmark_payload(manifest, backend)

        manifest["files"].update({
            "payload/d3d11/fonts/courier_new_bold_16.spritefont": {},
            "payload/d3d11/textures/white.dds": {},
        })
        release.verify_benchmark_payload(manifest, backend)


class AnalysisTests(unittest.TestCase):
    def evidence(self, root: Path) -> dict:
        now = datetime.now(timezone.utc)
        document = config(now)
        document["authorization"]["allow_launch"] = True
        worker_sha = "e" * 64
        release_document = {
            "schema_version": 1,
            "scope": "labrador-cloud-performance-payload",
            "git": {"commit": "c" * 40, "dirty": True, "status_sha256": "d" * 64},
            "files": {"source/tools/cloud_performance/worker.ps1": {
                "sha256": worker_sha, "bytes": 1,
            }},
        }
        document["bundle"]["release_sha256"] = hashlib.sha256(
            canonical(release_document)).hexdigest()
        config_sha = hashlib.sha256(canonical(validate_config(document))).hexdigest()
        shared = {
            "schema_version": 1,
            "run_id": document["run_id"],
            "bundle_sha256": document["bundle"]["sha256"],
            "release_sha256": document["bundle"]["release_sha256"],
            "config_sha256": config_sha,
            "worker_sha256": worker_sha,
        }
        write_json(root / "output" / "config.json", document)
        write_json(root / "output" / "release.json", release_document)
        template_bytes = canonical(infrastructure.template(
            document, require_authorization=True, check_time=False))
        template_path = root / "output" / "template.json"
        template_path.parent.mkdir(parents=True, exist_ok=True)
        template_path.write_bytes(template_bytes)
        template_sha = hashlib.sha256(template_bytes).hexdigest()
        shared["template_sha256"] = template_sha
        write_json(root / "ssm" / "launch.json", shared | {
            "stack_id": "stack", "instance_id": "i-test", "command_id": "command",
        })
        completed = (now + timedelta(minutes=1)).isoformat()
        gpu_row = "NVIDIA L4-6Q, 0x1234, 555.1, P0, 40, 1000, 5000"
        write_json(root / "output" / "host.json", {
            "schema_version": 1,
            "run_id": document["run_id"],
            "instance": {
                "instanceType": document["instance_type"], "imageId": document["ami_id"],
                "region": document["region"], "availabilityZone": document["availability_zone"],
                "instanceId": "i-test", "accountId": "123456789012",
            },
            "cpu_options": {"core_count": 4, "logical_processors": 4},
            "cpu": [{"Name": "AMD EPYC"}],
            "os": {"build_number": "20348"},
            "video": [{"Name": "NVIDIA L4-6Q", "CurrentHorizontalResolution": 1920,
                       "CurrentVerticalResolution": 1080, "CurrentRefreshRate": 60}],
            "session": {"id": 0, "user": "SYSTEM"},
            "aws_identity": {
                "role_name": "labrador-runner-role",
                "account_id": "123456789012",
                "caller_arn": "arn:aws:sts::123456789012:assumed-role/labrador-runner-role/i-test",
            },
            "visual_cpp_runtime": [
                {"name": "MSVCP140.dll", "version": "14.0"},
                {"name": "VCRUNTIME140.dll", "version": "14.0"},
                {"name": "VCRUNTIME140_1.dll", "version": "14.0"},
            ],
            "power_plan": "Power Scheme GUID: 8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c",
            "nvidia_smi_before": [gpu_row], "nvidia_smi_after": [gpu_row],
            "nvidia_smi_identity": ["NVIDIA L4-6Q|0x1234|555.1"],
            "completed_utc": completed,
            "bundle_sha256": document["bundle"]["sha256"],
            "release_sha256": document["bundle"]["release_sha256"],
            "config_sha256": config_sha, "worker_sha256": worker_sha,
            "template_sha256": template_sha,
        })
        write_json(root / "output" / "success.json", shared | {
            "status": "success", "completed_utc": completed,
        })
        samples = []
        for ordinal in range(document["workload"]["sample_frames"]):
            samples.append({
                "ordinal": ordinal,
                "update_ns": 5 + ordinal,
                "begin_ns": 10 + ordinal,
                "record_submit_ns": 20 + ordinal,
                "present_ns": 30 + ordinal,
                "whole_frame_ns": 60 + ordinal,
                "scheduled_interval_ns": 16_666_667 + ordinal,
            })
        summary = {phase: phase_summary([sample[phase] for sample in samples])
                   for phase in analysis.PHASES}
        for repetition in range(1, document["workload"]["repetitions"] + 1):
            write_json(root / "output" / "results" / f"result-d3d11-{repetition:03d}.json", {
                "schema_version": 1,
                "scope": "linesweeper_frame_benchmark",
                "status": "complete",
                "run": document["run_id"],
                "release_sha256": document["bundle"]["release_sha256"],
                "started_utc": now.isoformat(),
                "finished_utc": completed,
                "build": {"configuration": "release", "render_backend": "d3d11"},
                "measurement_class": "hardware_raster",
                "render_device": {"api": "Direct3D 11", "device_name": "NVIDIA L4-6Q",
                                  "vendor_id": 0x10DE, "device_id": 1, "kind": "hardware"},
                "workload": {"name": "full_well_top_out",
                             "resolution": {"width": 1280, "height": 720},
                             "refresh_hz": 60, "warmup_frames": 120, "sample_frames": 100,
                             "live_particles": 9600, "target_frame_ns": 16666667},
                "timing": {"clock": "std::chrono::steady_clock", "unit": "nanoseconds",
                           "summary_method": "nearest-rank",
                           "interval_scope": (
                               "software-paced frame-start interval; not display scan-out"),
                           "sample_count": 100,
                           "scheduled_interval_ns": [
                               sample["scheduled_interval_ns"] for sample in samples]},
                "samples": samples,
                "summary": summary,
            })
        evidence_root = root / "output"
        write_json(root / "output" / "success.json", shared | {
            "status": "success", "completed_utc": completed,
            "evidence_files": evidence_manifest(evidence_root),
        })
        return document

    def test_complete_evidence_reports_pooled_percentiles(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.evidence(root)
            report = analysis.analyze(root)
            self.assertTrue(report["complete"])
            self.assertEqual(report["backends"]["d3d11"]["sample_count"], 200)
            self.assertIn("p99", report["backends"]["d3d11"]["summary"]["whole_frame_ns"])

    def test_incomplete_or_identity_drift_withholds_analysis(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.evidence(root)
            (root / "output" / "results" / "result-d3d11-002.json").unlink()
            report = analysis.analyze(root)
            self.assertFalse(report["complete"])
            self.assertEqual(report["analysis_withheld"], "evidence_manifest_invalid")

            marker_path = root / "output" / "success.json"
            marker = json.loads(marker_path.read_text())
            marker["evidence_files"].pop("results/result-d3d11-002.json")
            write_json(marker_path, marker)
            report = analysis.analyze(root)
            self.assertFalse(report["complete"])
            self.assertEqual(report["analysis_withheld"], "fixed_sample_incomplete")
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.evidence(root)
            host = json.loads((root / "output" / "host.json").read_text())
            host["instance"]["imageId"] = "ami-deadbeef"
            write_json(root / "output" / "host.json", host)
            refresh_terminal_manifest(root / "output")
            report = analysis.analyze(root)
            self.assertFalse(report["complete"])
            self.assertEqual(report["analysis_withheld"], "host_or_release_identity_invalid")

    def test_nested_non_objects_are_withheld_instead_of_crashing(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.evidence(root)
            host_path = root / "output" / "host.json"
            host = json.loads(host_path.read_text())
            host["instance"] = []
            write_json(host_path, host)
            refresh_terminal_manifest(root / "output")

            report = analysis.analyze(root)

            self.assertFalse(report["complete"])
            self.assertEqual(report["analysis_withheld"], "host_or_release_identity_invalid")

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.evidence(root)
            result_path = root / "output" / "results" / "result-d3d11-001.json"
            result = json.loads(result_path.read_text())
            result["render_device"] = []
            write_json(result_path, result)
            refresh_terminal_manifest(root / "output")

            report = analysis.analyze(root)

            self.assertFalse(report["complete"])
            self.assertEqual(report["analysis_withheld"], "result_validation_failed")

    def test_session_gpu_and_deadline_identity_are_enforced(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.evidence(root)
            host_path = root / "output" / "host.json"
            host = json.loads(host_path.read_text())
            host["session"] = {"id": 2, "user": "Administrator"}
            write_json(host_path, host)
            refresh_terminal_manifest(root / "output")
            self.assertEqual(
                analysis.analyze(root)["analysis_withheld"],
                "host_or_release_identity_invalid",
            )

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.evidence(root)
            host_path = root / "output" / "host.json"
            host = json.loads(host_path.read_text())
            host["aws_identity"]["caller_arn"] = (
                "arn:aws:sts::123456789012:assumed-role/unrelated-role/i-test")
            write_json(host_path, host)
            refresh_terminal_manifest(root / "output")
            self.assertEqual(
                analysis.analyze(root)["analysis_withheld"],
                "host_or_release_identity_invalid",
            )

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.evidence(root)
            host_path = root / "output" / "host.json"
            host = json.loads(host_path.read_text())
            host["nvidia_smi_after"][0] = host["nvidia_smi_after"][0].replace(
                "555.1", "556.2")
            write_json(host_path, host)
            refresh_terminal_manifest(root / "output")
            self.assertEqual(
                analysis.analyze(root)["analysis_withheld"],
                "host_or_release_identity_invalid",
            )

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            document = self.evidence(root)
            marker_path = root / "output" / "success.json"
            marker = json.loads(marker_path.read_text())
            marker["completed_utc"] = document["authorization"]["deadline_utc"]
            write_json(marker_path, marker)
            self.assertEqual(
                analysis.analyze(root)["analysis_withheld"],
                "host_or_release_identity_invalid",
            )

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.evidence(root)
            host = json.loads((root / "output" / "host.json").read_text())
            host["video"][0]["CurrentRefreshRate"] = 30
            write_json(root / "output" / "host.json", host)
            refresh_terminal_manifest(root / "output")
            report = analysis.analyze(root)
            self.assertFalse(report["complete"])
            self.assertEqual(report["analysis_withheld"], "host_or_release_identity_invalid")


if __name__ == "__main__":
    unittest.main()
