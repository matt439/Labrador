"""Offline analysis tests using synthetic data and local fixtures."""

from __future__ import annotations

import copy
import hashlib
import json
import tempfile
import unittest
from datetime import datetime, timedelta, timezone
from pathlib import Path

from tools.cloud_performance import analysis, infrastructure
from tools.cloud_performance.common import canonical, validate_config
from tools.tests.cloud_performance_fixtures import config, write_json


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
                "whole_frame_ns": 65 + 4 * ordinal,
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

    def replace_samples(self, root: Path, repetition: int, samples: list[dict]) -> None:
        path = root / "output" / "results" / f"result-d3d11-{repetition:03d}.json"
        result = json.loads(path.read_text())
        result["samples"] = samples
        result["timing"]["scheduled_interval_ns"] = [
            sample["scheduled_interval_ns"] for sample in samples]
        result["summary"] = {
            phase: phase_summary([sample[phase] for sample in samples])
            for phase in analysis.PHASES + analysis.PACING_PHASES
            if phase in samples[0]
        }
        write_json(path, result)
        refresh_terminal_manifest(root / "output")

    def record_policies(self, root: Path) -> None:
        for path in (root / "output" / "results").glob("result-*.json"):
            result = json.loads(path.read_text())
            result["timing"].update({"pacer": "win32_high_resolution_waitable_timer",
                                     "deadline_policy": "absolute_catch_up"})
            result["render_device"].update({"present_mode": "dxgi_sync_interval",
                                            "requested_swap_interval": 1,
                                            "reported_swap_interval": None})
            for sample in result["samples"]:
                sample.update({"pacing_wait_ns": 1000, "start_lateness_ns": 100})
            for phase in analysis.PACING_PHASES:
                result["summary"][phase] = phase_summary(
                    [sample[phase] for sample in result["samples"]])
            write_json(path, result)
        refresh_terminal_manifest(root / "output")

    def test_complete_evidence_reports_pooled_percentiles(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.evidence(root)
            report = analysis.analyze(root)
            self.assertTrue(report["complete"])
            self.assertEqual(report["backends"]["d3d11"]["sample_count"], 200)
            self.assertIn("p99", report["backends"]["d3d11"]["summary"]["whole_frame_ns"])
            backend = report["backends"]["d3d11"]
            self.assertEqual(backend["presentation_measurement"], "unrecorded_legacy")
            self.assertEqual(backend["pacing_measurement"], "unrecorded_legacy")
            self.assertNotIn("present_mode", backend["render_device"])
            self.assertNotIn("start_lateness_ns", backend["summary"])
            pools = backend["cadence_pools"]
            self.assertEqual(set(pools), {"not_locked"})
            self.assertEqual(pools["not_locked"]["repetitions"], [1, 2])
            self.assertEqual(pools["not_locked"]["sample_count"], 200)
            self.assertEqual(pools["not_locked"]["summary"], backend["summary"])
            for phase in analysis.PACING_PHASES:
                self.assertNotIn(phase, pools["not_locked"]["summary"])

    def test_cadence_pool_percentiles_use_raw_samples_across_repetitions(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.evidence(root)
            for repetition, whole in ((1, 10), (2, 1000)):
                samples = [{
                    "ordinal": index, "update_ns": 1, "begin_ns": 2,
                    "record_submit_ns": 3, "present_ns": whole - 6,
                    "whole_frame_ns": whole, "scheduled_interval_ns": 16_666_667,
                } for index in range(100)]
                self.replace_samples(root, repetition, samples)

            report = analysis.analyze(root)

            self.assertTrue(report["complete"])
            backend = report["backends"]["d3d11"]
            pool = backend["cadence_pools"]["not_locked"]
            self.assertEqual(pool["repetitions"], [1, 2])
            self.assertEqual(pool["sample_count"], 200)
            # Averaging the repetitions' percentiles would produce 505.
            self.assertEqual(pool["summary"]["whole_frame_ns"]["p50"], 10)
            self.assertEqual(pool["summary"]["whole_frame_ns"]["p99"], 1000)
            self.assertEqual(pool["summary"], backend["summary"])

    def test_first_repetition_with_long_present_is_retained_and_exposed(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.evidence(root)
            samples = [{
                "ordinal": ordinal, "update_ns": 1_000_000, "begin_ns": 0,
                "record_submit_ns": 2_000_000, "present_ns": 14_000_000,
                "whole_frame_ns": 17_000_000, "scheduled_interval_ns": 17_000_000,
            } for ordinal in range(100)]
            self.replace_samples(root, 1, samples)

            report = analysis.analyze(root)

            self.assertTrue(report["complete"])
            backend = report["backends"]["d3d11"]
            first, second = backend["repetitions"]
            self.assertEqual(backend["sample_count"], 200)
            self.assertEqual(backend["summary"]["whole_frame_ns"]["p99"], 17_000_000)
            self.assertEqual([first["repetition"], second["repetition"]], [1, 2])
            self.assertEqual(first["sample_count"], 100)
            self.assertEqual(first["whole_frame_over_target_count"], 100)
            self.assertEqual(first["whole_frame_over_target_fraction"], 1.0)
            self.assertEqual(first["long_begin_or_present"]["present_count"], 100)
            self.assertEqual(first["long_begin_or_present"]["adjacent_transition_count"], 0)
            episode = first["long_begin_or_present"]["present"]
            self.assertEqual(episode["first_sample"], 0)
            self.assertEqual(episode["last_sample"], 99)
            self.assertEqual(episode["longest_consecutive_count"], 100)
            self.assertEqual(episode["longest_consecutive_start_sample"], 0)
            self.assertEqual(episode["longest_alternating_count"], 0)
            self.assertEqual(second["long_begin_or_present"]["either_count"], 0)
            self.assertEqual(backend["repetition_ranges"]["present_ns"]["p50"],
                             {"min": 79, "max": 14_000_000})
            self.assertEqual(first["source"], "output/results/result-d3d11-001.json")
            self.assertNotIn("scheduled_interval_ns", first["timing"])
            self.assertEqual(first["pacing_measurement"], "unrecorded_legacy")

    def test_alternating_begin_cost_does_not_turn_median_interval_into_rate(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.evidence(root)
            samples = [{
                "ordinal": ordinal, "update_ns": 200_000,
                "begin_ns": 30_000_000 if ordinal % 2 == 0 else 0,
                "record_submit_ns": 1_000_000, "present_ns": 100_000,
                "whole_frame_ns": 31_300_000 if ordinal % 2 == 0 else 1_300_000,
                "scheduled_interval_ns": 2_000_000 if ordinal % 2 == 0 else 31_333_334,
            } for ordinal in range(100)]
            self.replace_samples(root, 1, samples)

            report = analysis.analyze(root)

            self.assertTrue(report["complete"])
            first = report["backends"]["d3d11"]["repetitions"][0]
            self.assertEqual(first["summary"]["scheduled_interval_ns"]["p50"], 2_000_000)
            self.assertEqual(first["cadence"]["mean_interval_ns"], 16_666_667)
            self.assertAlmostEqual(first["cadence"]["realized_hz"], 60, places=4)
            self.assertEqual(first["cadence"]["under_half_target_count"], 50)
            self.assertEqual(first["cadence"]["over_one_and_half_target_count"], 50)
            self.assertEqual(first["whole_frame_over_target_count"], 50)
            self.assertEqual(first["long_begin_or_present"]["begin_count"], 50)
            self.assertEqual(first["long_begin_or_present"]["present_count"], 0)
            self.assertEqual(first["long_begin_or_present"]["adjacent_transition_count"], 99)

    def test_mid_run_alternation_and_sustained_episode_keep_their_onsets(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.evidence(root)
            for repetition in (1, 2):
                samples = []
                for ordinal in range(100):
                    slow = (ordinal >= 30 and ordinal % 2 == 0 if repetition == 1
                            else ordinal >= 20)
                    samples.append({
                        "ordinal": ordinal, "update_ns": 200_000,
                        "begin_ns": 20_000_000 if slow else 0,
                        "record_submit_ns": 1_000_000, "present_ns": 100_000,
                        "whole_frame_ns": 21_300_000 if slow else 1_300_000,
                        "scheduled_interval_ns": 16_666_667,
                    })
                self.replace_samples(root, repetition, samples)

            report = analysis.analyze(root)

            self.assertTrue(report["complete"])
            first, second = report["backends"]["d3d11"]["repetitions"]
            alternating = first["long_begin_or_present"]["begin"]
            self.assertEqual(alternating["count"], 35)
            self.assertEqual(alternating["first_sample"], 30)
            self.assertEqual(alternating["last_sample"], 98)
            self.assertEqual(alternating["longest_consecutive_count"], 1)
            self.assertEqual(alternating["longest_alternating_count"], 71)
            self.assertEqual(alternating["longest_alternating_start_sample"], 29)
            sustained = second["long_begin_or_present"]["begin"]
            self.assertEqual(sustained["count"], 80)
            self.assertEqual(sustained["first_sample"], 20)
            self.assertEqual(sustained["last_sample"], 99)
            self.assertEqual(sustained["longest_consecutive_count"], 80)
            self.assertEqual(sustained["longest_consecutive_start_sample"], 20)
            self.assertEqual(second["whole_frame_over_target"], sustained)
            self.assertIsNone(second["long_begin_or_present"]["present"]["first_sample"])

    def test_self_consistent_summary_cannot_hide_invalid_phase_arithmetic(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.evidence(root)
            path = root / "output" / "results" / "result-d3d11-001.json"
            samples = json.loads(path.read_text())["samples"]
            samples[0]["whole_frame_ns"] += 1
            self.replace_samples(root, 1, samples)

            report = analysis.analyze(root)

            self.assertFalse(report["complete"])
            self.assertEqual(report["analysis_withheld"], "result_validation_failed")
            self.assertIn("phase sum", report["error"])

    def test_recorded_pacing_and_present_policy_survive_analysis(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.evidence(root)
            self.record_policies(root)
            path = root / "output" / "results" / "result-d3d11-001.json"
            result = json.loads(path.read_text())

            report = analysis.analyze(root)

            self.assertTrue(report["complete"])
            first = report["backends"]["d3d11"]["repetitions"][0]
            self.assertEqual(first["pacing_measurement"], "recorded")
            self.assertEqual(first["summary"]["start_lateness_ns"]["p99"], 100)
            self.assertEqual(first["summary"]["pacing_wait_ns"]["p99"], 1000)
            self.assertEqual(first["timing"]["deadline_policy"], "absolute_catch_up")
            self.assertEqual(first["render_device"]["requested_swap_interval"], 1)
            self.assertIsNone(first["render_device"]["reported_swap_interval"])
            self.assertEqual(first["presentation_measurement"], "recorded")
            backend = report["backends"]["d3d11"]
            self.assertEqual(backend["summary"]["pacing_wait_ns"]["count"], 200)
            self.assertEqual(backend["summary"]["start_lateness_ns"]["p99"], 100)
            self.assertEqual(backend["repetition_ranges"]["start_lateness_ns"]["p99"],
                             {"min": 100, "max": 100})
            self.assertEqual(backend["render_device"]["present_mode"], "dxgi_sync_interval")
            self.assertEqual(first["start_lateness_over_target"]["count"], 0)

            del result["samples"][0]["start_lateness_ns"]
            write_json(path, result)
            refresh_terminal_manifest(root / "output")
            self.assertEqual(analysis.analyze(root)["analysis_withheld"],
                             "result_validation_failed")

            result["samples"][0]["start_lateness_ns"] = 100
            del result["timing"]["deadline_policy"]
            write_json(path, result)
            refresh_terminal_manifest(root / "output")
            self.assertEqual(analysis.analyze(root)["analysis_withheld"],
                             "result_validation_failed")

    def test_persistent_lateness_is_reported_separately_from_frame_work(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.evidence(root)
            self.record_policies(root)
            path = root / "output" / "results" / "result-d3d11-001.json"
            result = json.loads(path.read_text())
            for sample in result["samples"][20:]:
                sample["start_lateness_ns"] = 20_000_000
                sample["pacing_wait_ns"] = 0
            for phase in analysis.PACING_PHASES:
                result["summary"][phase] = phase_summary(
                    [sample[phase] for sample in result["samples"]])
            write_json(path, result)
            refresh_terminal_manifest(root / "output")

            report = analysis.analyze(root)

            self.assertTrue(report["complete"])
            backend = report["backends"]["d3d11"]
            first, second = backend["repetitions"]
            self.assertEqual(first["whole_frame_over_target_count"], 0)
            self.assertEqual(first["start_lateness_over_target"]["count"], 80)
            self.assertEqual(first["start_lateness_over_target"]["first_sample"], 20)
            self.assertEqual(first["start_lateness_over_target"]["longest_consecutive_count"], 80)
            self.assertEqual(second["start_lateness_over_target"]["count"], 0)
            self.assertEqual(backend["summary"]["start_lateness_ns"]["p99"], 20_000_000)
            self.assertEqual(backend["repetition_ranges"]["start_lateness_ns"]["p99"],
                             {"min": 100, "max": 20_000_000})

    def locked_samples(self, count: int, *, onset: int, whole_frame_ns: int = 16_664_000,
                       alternate: bool = False) -> list[dict]:
        """Free frames before the onset; after it the wait sits in present, not the pacer."""
        samples = []
        for ordinal in range(count):
            locked = ordinal >= onset
            if locked and alternate and ordinal % 2:
                present, wait = 1_000_000, 100
            elif locked and alternate:
                present, wait = 2 * whole_frame_ns - 4_000_000 - 3_000_000, 100
            elif locked:
                present, wait = whole_frame_ns - 3_000_000, 100
            else:
                present, wait = 40_000, 14_000_000
            samples.append({
                "ordinal": ordinal, "update_ns": 1_000_000, "begin_ns": 0,
                "record_submit_ns": 2_000_000, "present_ns": present,
                "whole_frame_ns": 3_000_000 + present,
                "scheduled_interval_ns": 16_666_667 if ordinal else 0,
                "pacing_wait_ns": wait, "start_lateness_ns": 24_000_000 if locked else 100,
            })
        return samples

    def test_wait_dominated_repetition_is_a_candidate_without_claiming_a_cause(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.evidence(root)
            self.record_policies(root)
            self.replace_samples(root, 1, self.locked_samples(100, onset=30))
            self.replace_samples(root, 2, self.locked_samples(100, onset=100))

            report = analysis.analyze(root)

            self.assertTrue(report["complete"])
            backend = report["backends"]["d3d11"]
            first, second = backend["repetitions"]
            lock = first["presentation_lock"]
            self.assertEqual(lock["verdict"], "presentation_wait_candidate")
            self.assertTrue(lock["cadence_signature"])
            self.assertEqual(lock["tail_mean_update_record_submit_ns"], 3_000_000)
            self.assertEqual(lock["tail_mean_begin_present_ns"], 13_664_000)
            self.assertEqual(lock["pacer_idle_source"], "pacing_wait_ns")
            self.assertEqual(lock["tail_first_sample"], 30)
            self.assertEqual(lock["tail_sample_count"], 70)
            self.assertEqual(lock["minimum_tail_sample_count"], 60)
            self.assertEqual(lock["pacer_idle_count"], 70)
            self.assertEqual(lock["tail_mean_whole_frame_ns"], 16_664_000)
            self.assertEqual(lock["tail_mean_interval_ns"], 16_666_667)
            self.assertEqual(lock["tail_start_lateness_ns"],
                             {"first": 24_000_000, "last": 24_000_000,
                              "min": 24_000_000, "max": 24_000_000})
            # Keep observed counts; phase timing supports an inference, not a
            # claim that the recorded whole-frame cost can be discarded.
            self.assertEqual(first["whole_frame_over_target_count"], 0)
            self.assertEqual(first["long_begin_or_present"]["present_count"], 70)
            self.assertEqual(second["presentation_lock"]["verdict"], "not_locked")
            self.assertEqual(second["presentation_lock"]["tail_sample_count"], 0)
            self.assertIsNone(second["presentation_lock"]["tail_first_sample"])
            self.assertNotIn("tail_start_lateness_ns", second["presentation_lock"])
            self.assertEqual(backend["presentation_wait_candidate_repetitions"], [1])
            self.assertIn("presentation_lock", report["interpretation"])
            pools = backend["cadence_pools"]
            self.assertEqual(set(pools), {"not_locked", "presentation_wait_candidate"})
            candidate = pools["presentation_wait_candidate"]
            other = pools["not_locked"]
            self.assertEqual(candidate["repetitions"], [1])
            self.assertEqual(other["repetitions"], [2])
            # Classification is by the tail, but all 100 samples survive:
            # the first 30 pre-onset frames are not silently discarded.
            self.assertEqual(candidate["sample_count"], 100)
            self.assertEqual(candidate["summary"]["whole_frame_ns"]["min"], 3_040_000)
            self.assertEqual(candidate["summary"]["whole_frame_ns"]["p99"], 16_664_000)
            self.assertEqual(other["summary"]["whole_frame_ns"]["p99"], 3_040_000)
            self.assertEqual(backend["summary"]["whole_frame_ns"]["p99"], 16_664_000)
            self.assertEqual(sum(pool["sample_count"] for pool in pools.values()),
                             backend["sample_count"])
            for phase in backend["summary"]:
                self.assertEqual(sum(pool["summary"][phase]["count"] for pool in pools.values()),
                                 backend["summary"][phase]["count"])
            self.assertIn("cadence_pools", report["interpretation"])

    def test_work_and_mixed_cost_cadence_signatures_do_not_claim_presentation_lock(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.evidence(root)
            self.record_policies(root)
            for repetition, update, record, begin, present in (
                    (1, 16_750_000, 100_000, 30_000, 20_000),
                    (2, 10_000_000, 100_000, 30_000, 6_770_000)):
                samples = [{
                    "ordinal": index, "update_ns": update, "record_submit_ns": record,
                    "begin_ns": begin, "present_ns": present,
                    "whole_frame_ns": 16_900_000,
                    "scheduled_interval_ns": 16_900_100,
                    "pacing_wait_ns": 100,
                    "start_lateness_ns": (index + 1) * (16_900_100 - 16_666_667),
                } for index in range(100)]
                self.replace_samples(root, repetition, samples)

            report = analysis.analyze(root)

            self.assertTrue(report["complete"])
            backend = report["backends"]["d3d11"]
            first, second = backend["repetitions"]
            self.assertEqual(first["presentation_lock"]["verdict"], "work_limited")
            self.assertEqual(first["presentation_lock"]["tail_mean_update_record_submit_ns"],
                             16_850_000)
            self.assertEqual(first["long_begin_or_present"]["either_count"], 0)
            self.assertEqual(first["whole_frame_over_target_count"], 100)
            self.assertEqual(second["presentation_lock"]["verdict"], "ambiguous_cadence")
            self.assertTrue(first["presentation_lock"]["cadence_signature"])
            self.assertTrue(second["presentation_lock"]["cadence_signature"])
            self.assertEqual(backend["presentation_wait_candidate_repetitions"], [])
            pools = backend["cadence_pools"]
            self.assertEqual(set(pools), {"work_limited", "ambiguous_cadence"})
            self.assertEqual(pools["work_limited"]["repetitions"], [1])
            self.assertEqual(pools["ambiguous_cadence"]["repetitions"], [2])
            self.assertEqual(pools["work_limited"]["summary"]["update_ns"]["p99"],
                             16_750_000)
            self.assertEqual(pools["ambiguous_cadence"]["summary"]["update_ns"]["p99"],
                             10_000_000)
            self.assertEqual(sum(pool["sample_count"] for pool in pools.values()),
                             backend["sample_count"])

    def test_alternating_lock_is_recognised_from_the_tail_mean(self):
        # Vulkan's two-slot ring against three FIFO images blocks every second
        # frame for two periods; the median whole frame is nowhere near the
        # period, the mean over the tail is exactly it.
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.evidence(root)
            self.record_policies(root)
            self.replace_samples(root, 1, self.locked_samples(100, onset=0, alternate=True))

            report = analysis.analyze(root)

            self.assertTrue(report["complete"])
            first = report["backends"]["d3d11"]["repetitions"][0]
            self.assertEqual(first["summary"]["whole_frame_ns"]["p50"], 4_000_000)
            self.assertEqual(first["presentation_lock"]["verdict"],
                             "presentation_wait_candidate")
            self.assertEqual(first["presentation_lock"]["tail_first_sample"], 0)
            self.assertEqual(first["presentation_lock"]["tail_mean_whole_frame_ns"], 16_664_000)
            self.assertEqual(first["long_begin_or_present"]["present_count"], 50)

    def test_catch_up_and_short_idle_tails_are_not_locks(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.evidence(root)
            self.record_policies(root)
            # An idle tail shorter than a second: the pacer stopped waiting
            # only for the last frames, which is a catch-up, not a lock.
            self.replace_samples(root, 1, self.locked_samples(100, onset=50))
            # An idle tail whose frames are far shorter than the period: a
            # late loop catching up, with nothing blocking it.
            samples = self.locked_samples(100, onset=0)
            for sample in samples:
                sample["present_ns"] = 40_000
                sample["whole_frame_ns"] = 3_040_000
            self.replace_samples(root, 2, samples)

            report = analysis.analyze(root)

            self.assertTrue(report["complete"])
            backend = report["backends"]["d3d11"]
            first, second = backend["repetitions"]
            self.assertEqual(first["presentation_lock"]["verdict"], "not_locked")
            self.assertEqual(first["presentation_lock"]["tail_sample_count"], 50)
            self.assertEqual(first["presentation_lock"]["tail_first_sample"], 50)
            self.assertEqual(second["presentation_lock"]["verdict"], "not_locked")
            self.assertEqual(second["presentation_lock"]["tail_sample_count"], 100)
            self.assertEqual(second["presentation_lock"]["tail_mean_whole_frame_ns"], 3_040_000)
            self.assertEqual(backend["presentation_wait_candidate_repetitions"], [])

    def test_legacy_lock_is_read_from_the_gap_and_cannot_see_the_first_sample(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.evidence(root)
            samples = self.locked_samples(100, onset=0)
            for sample in samples:
                for phase in analysis.PACING_PHASES:
                    del sample[phase]
            self.replace_samples(root, 1, samples)

            report = analysis.analyze(root)

            self.assertTrue(report["complete"])
            first = report["backends"]["d3d11"]["repetitions"][0]
            self.assertEqual(first["pacing_measurement"], "unrecorded_legacy")
            lock = first["presentation_lock"]
            self.assertEqual(lock["verdict"], "presentation_wait_candidate")
            self.assertEqual(lock["pacer_idle_source"], "frame_end_to_next_start")
            self.assertEqual(lock["tail_first_sample"], 1)
            self.assertEqual(lock["tail_sample_count"], 99)
            self.assertNotIn("tail_start_lateness_ns", lock)
            self.assertEqual(report["backends"]["d3d11"]["presentation_wait_candidate_repetitions"], [1])

    def test_partial_or_changed_presentation_metadata_is_refused(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.evidence(root)
            self.record_policies(root)
            path = root / "output" / "results" / "result-d3d11-002.json"
            original = json.loads(path.read_text())
            changes = [
                ("present_mode", "fifo"), ("requested_swap_interval", 0),
                ("requested_swap_interval", True), ("requested_swap_interval", 1.0),
                ("reported_swap_interval", 1),
            ]
            for key, value in changes:
                with self.subTest(field=key, value=value):
                    result = copy.deepcopy(original)
                    result["render_device"][key] = value
                    write_json(path, result)
                    refresh_terminal_manifest(root / "output")
                    report = analysis.analyze(root)
                    self.assertFalse(report["complete"])
                    self.assertIn("presentation", report["error"])
            for field in analysis.PRESENTATION_FIELDS:
                with self.subTest(missing=field):
                    result = copy.deepcopy(original)
                    del result["render_device"][field]
                    write_json(path, result)
                    refresh_terminal_manifest(root / "output")
                    report = analysis.analyze(root)
                    self.assertFalse(report["complete"])
                    self.assertIn("presentation metadata is incomplete", report["error"])

    def test_backend_presentation_contracts_are_checked(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            document = self.evidence(root)
            self.record_policies(root)
            original = json.loads((root / "output" / "results" /
                                   "result-d3d11-001.json").read_text())
            policies = {
                "d3d12": ("dxgi_sync_interval", 1, None),
                "gl": ("wgl_swap_interval", 1, 1),
                "vulkan": ("fifo", None, None),
            }
            for name, policy in policies.items():
                with self.subTest(backend=name):
                    result = copy.deepcopy(original)
                    result["build"]["render_backend"] = name
                    result["render_device"].update(dict(zip(analysis.PRESENTATION_FIELDS, policy)))
                    backend = document["workload"]["backends"][0] | {"name": name}
                    phases = analysis._validate_result(result, backend, document)
                    self.assertEqual(len(phases["pacing_wait_ns"]), 100)
                    result["render_device"]["reported_swap_interval"] = 0
                    with self.assertRaisesRegex(ValueError, "presentation policy"):
                        analysis._validate_result(result, backend, document)

    def test_legacy_and_recorded_policies_cannot_be_mixed_in_one_run(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.evidence(root)
            self.record_policies(root)
            path = root / "output" / "results" / "result-d3d11-002.json"
            result = json.loads(path.read_text())
            for field in analysis.PRESENTATION_FIELDS:
                del result["render_device"][field]
            del result["timing"]["pacer"]
            del result["timing"]["deadline_policy"]
            for phase in analysis.PACING_PHASES:
                del result["summary"][phase]
                for sample in result["samples"]:
                    del sample[phase]
            write_json(path, result)
            refresh_terminal_manifest(root / "output")

            report = analysis.analyze(root)

            self.assertFalse(report["complete"])
            self.assertIn("policies are mixed", report["error"])

    def test_omitting_a_whole_metadata_group_cannot_downgrade_the_run(self):
        for category in ("presentation", "pacing"):
            with self.subTest(category=category), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                self.evidence(root)
                self.record_policies(root)
                for path in (root / "output" / "results").glob("result-*.json"):
                    result = json.loads(path.read_text())
                    if category == "presentation":
                        for field in analysis.PRESENTATION_FIELDS:
                            del result["render_device"][field]
                    else:
                        del result["timing"]["pacer"]
                        del result["timing"]["deadline_policy"]
                        for phase in analysis.PACING_PHASES:
                            del result["summary"][phase]
                            for sample in result["samples"]:
                                del sample[phase]
                    write_json(path, result)
                refresh_terminal_manifest(root / "output")

                report = analysis.analyze(root)

                self.assertFalse(report["complete"])
                self.assertIn("must be recorded together", report["error"])

    def test_presentation_driven_diagnostic_cannot_pass_as_cloud_workload(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.evidence(root)
            path = root / "output" / "results" / "result-d3d11-001.json"
            result = json.loads(path.read_text())
            result["timing"].update({"pacer": "presentation_driven",
                                     "deadline_policy": "none"})
            write_json(path, result)
            refresh_terminal_manifest(root / "output")

            report = analysis.analyze(root)

            self.assertFalse(report["complete"])
            self.assertIn("pacing metadata", report["error"])

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
