"""Fail-closed analysis of downloaded cloud performance evidence."""

from __future__ import annotations

import hashlib
import json
import math
import re
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from .common import canonical, deadline, digest, safe_relative, validate_config

PHASES = (
    "update_ns", "begin_ns", "record_submit_ns", "present_ns", "whole_frame_ns",
    "scheduled_interval_ns",
)
FRAME_PHASES = PHASES[:4]
PACING_PHASES = ("pacing_wait_ns", "start_lateness_ns")
PRESENTATION_FIELDS = (
    "present_mode", "requested_swap_interval", "reported_swap_interval",
)
DEVICE_FIELDS = ("api", "device_name", "vendor_id", "device_id", "kind")


def _json(path: Path) -> Any:
    with path.open("r", encoding="utf-8-sig") as stream:
        return json.load(stream)


def _withheld(reason: str, **evidence: Any) -> dict[str, Any]:
    return {"schema_version": 1, "complete": False, "analysis_withheld": reason, **evidence}


def _integer(value: Any, name: str, *, minimum: int = 0) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < minimum:
        raise ValueError(f"{name} must be an integer >= {minimum}")
    return value


def _vendor_id(value: Any) -> int:
    if isinstance(value, str):
        return int(value, 0)
    return _integer(value, "device.vendor_id")


def _percentile(values: list[int], fraction: float) -> int:
    ordered = sorted(values)
    return ordered[max(0, math.ceil(fraction * len(ordered)) - 1)]


def _summary(values: list[int]) -> dict[str, int]:
    return {
        "count": len(values),
        "min": min(values),
        "p50": _percentile(values, 0.50),
        "p95": _percentile(values, 0.95),
        "p99": _percentile(values, 0.99),
        "max": max(values),
    }


def _long_call_pattern(flags: list[bool]) -> dict[str, Any]:
    """Locate slow episodes without losing their onset to a pooled percentile."""
    indices = [index for index, flagged in enumerate(flags) if flagged]
    longest = alternating = 0
    longest_start = alternating_start = None
    consecutive = changing = 0
    for index, flagged in enumerate(flags):
        consecutive = consecutive + 1 if flagged else 0
        if consecutive > longest:
            longest, longest_start = consecutive, index - consecutive + 1
        changing = (changing + 1 if index and flagged != flags[index - 1] else 1)
        if changing > 1 and changing > alternating:
            alternating, alternating_start = changing, index - changing + 1
    return {
        "count": len(indices),
        "first_sample": indices[0] if indices else None,
        "last_sample": indices[-1] if indices else None,
        "longest_consecutive_count": longest,
        "longest_consecutive_start_sample": longest_start,
        "longest_alternating_count": alternating,
        "longest_alternating_start_sample": alternating_start,
    }


def _diagnostics(phases: dict[str, list[int]], target_frame_ns: int) -> dict[str, Any]:
    """Describe cadence and long calls without inferring why a call took time."""
    intervals = phases["scheduled_interval_ns"]
    count = len(intervals)
    interval_total = sum(intervals)
    threshold = target_frame_ns // 2
    long_begin = [value > threshold for value in phases["begin_ns"]]
    long_present = [value > threshold for value in phases["present_ns"]]
    long_calls = [begin or present for begin, present in zip(long_begin, long_present)]
    over_budget = sum(value > target_frame_ns for value in phases["whole_frame_ns"])
    diagnostics = {
        "cadence": {
            "mean_interval_ns": interval_total / count,
            "realized_hz": 1_000_000_000 * count / interval_total if interval_total else None,
            "over_target_count": sum(value > target_frame_ns for value in intervals),
            "under_half_target_count": sum(2 * value < target_frame_ns for value in intervals),
            "over_one_and_half_target_count": sum(
                2 * value > 3 * target_frame_ns for value in intervals),
        },
        "whole_frame_over_target_count": over_budget,
        "whole_frame_over_target_fraction": over_budget / count,
        "whole_frame_over_target": _long_call_pattern([
            value > target_frame_ns for value in phases["whole_frame_ns"]]),
        "long_begin_or_present": {
            "threshold_ns": threshold,
            "begin_count": sum(long_begin),
            "present_count": sum(long_present),
            "either_count": sum(long_calls),
            # A count of N/2 alone cannot distinguish alternating long/short
            # calls from one contiguous slow half. Do not join repetitions.
            "adjacent_transition_count": sum(
                left != right for left, right in zip(long_calls, long_calls[1:])),
            "begin": _long_call_pattern(long_begin),
            "present": _long_call_pattern(long_present),
            "either": _long_call_pattern(long_calls),
        },
    }
    if "start_lateness_ns" in phases:
        diagnostics["start_lateness_over_target"] = _long_call_pattern([
            value > target_frame_ns for value in phases["start_lateness_ns"]])
    return diagnostics


def _repetition_ranges(observations: list[dict[str, Any]]) -> dict[str, Any]:
    return {
        phase: {
            percentile: {
                "min": min(item["summary"][phase][percentile] for item in observations),
                "max": max(item["summary"][phase][percentile] for item in observations),
            }
            for percentile in ("p50", "p99")
        }
        for phase in observations[0]["summary"]
    }


def _validate_presentation(device: dict[str, Any], backend_name: str) -> bool:
    recorded = any(field in device for field in PRESENTATION_FIELDS)
    if not recorded:
        return False
    if not all(field in device for field in PRESENTATION_FIELDS):
        raise ValueError("benchmark presentation metadata is incomplete")
    expected = {
        "d3d11": ("dxgi_sync_interval", 1, None),
        "d3d12": ("dxgi_sync_interval", 1, None),
        "gl": ("wgl_swap_interval", 1, 1),
        "vulkan": ("fifo", None, None),
    }[backend_name]
    actual = tuple(device[field] for field in PRESENTATION_FIELDS)
    if (actual != expected or any(isinstance(value, bool) for value in actual)
            or any(value is not None and not isinstance(value, int)
                   for value in actual[1:])):
        raise ValueError("benchmark presentation policy differs from its backend contract")
    return True


def _utc(value: Any, name: str) -> datetime:
    if not isinstance(value, str):
        raise TypeError(f"{name} must be a UTC timestamp")
    try:
        parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError as exc:
        raise ValueError(f"{name} must be a UTC timestamp") from exc
    if parsed.tzinfo is None or parsed.utcoffset() != timezone.utc.utcoffset(parsed):
        raise ValueError(f"{name} must be a UTC timestamp")
    return parsed.astimezone(timezone.utc)


def _gpu_identity(rows: Any) -> list[str]:
    if not isinstance(rows, list) or not rows:
        raise ValueError("nvidia-smi identity is absent")
    identity = []
    for row in rows:
        fields = [field.strip() for field in str(row).split(",")]
        if len(fields) < 3 or any(not field for field in fields[:3]):
            raise ValueError("nvidia-smi identity is malformed")
        identity.append("|".join(fields[:3]))
    return identity


def _validate_evidence_files(marker_path: Path, marker: dict[str, Any]) -> None:
    manifest = marker.get("evidence_files")
    if not isinstance(manifest, dict) or not manifest:
        raise ValueError("terminal evidence manifest is absent")
    root = marker_path.parent
    actual = {
        path.relative_to(root).as_posix()
        for path in root.rglob("*")
        if path.is_file() and path.name not in {"success.json", "failure.json"}
    }
    if actual != set(manifest):
        raise ValueError("downloaded evidence files differ from the terminal manifest")
    for relative, metadata in manifest.items():
        safe = safe_relative(relative, field="evidence path")
        if not isinstance(metadata, dict):
            raise TypeError(f"evidence metadata is invalid: {relative}")
        path = root.joinpath(*safe.parts)
        if (metadata.get("bytes") != path.stat().st_size
                or metadata.get("sha256") != digest(path)):
            raise ValueError(f"downloaded evidence drift: {relative}")


def _validate_identity(root: Path, marker: dict[str, Any],
                       config: dict[str, Any]) -> tuple[dict[str, Any], dict[str, Any]]:
    host_files = list(root.rglob("host.json"))
    release_files = list(root.rglob("release.json"))
    launch_files = list(root.rglob("launch.json"))
    template_files = list(root.rglob("template.json"))
    if (len(host_files) != 1 or len(release_files) != 1 or len(launch_files) != 1
            or len(template_files) != 1):
        raise ValueError(
            "host, release, launch and template identity must each exist exactly once")
    host = _json(host_files[0])
    release = _json(release_files[0])
    launch = _json(launch_files[0])
    if not all(isinstance(value, dict) for value in (marker, host, release, launch)):
        raise ValueError("host, release, launch and terminal marker must be objects")
    if host.get("schema_version") != 1 or launch.get("schema_version") != 1:
        raise ValueError("host or launch identity has the wrong schema")
    if (release.get("schema_version") != 1
            or release.get("scope") != "labrador-cloud-performance-payload"):
        raise ValueError("release identity has the wrong schema or scope")
    release_sha = hashlib.sha256(canonical(release)).hexdigest()
    config_sha = hashlib.sha256(canonical(config)).hexdigest()
    expected = {
        "run_id": config["run_id"],
        "bundle_sha256": config["bundle"]["sha256"],
        "release_sha256": config["bundle"]["release_sha256"],
        "config_sha256": config_sha,
        "template_sha256": digest(template_files[0]),
    }
    for name, value in expected.items():
        for label, record in (("marker", marker), ("launch", launch)):
            if record.get(name) != value:
                raise ValueError(f"{label} {name} differs from the declaration")
        if name != "run_id" and host.get(name) != value:
            raise ValueError(f"host {name} differs from the declaration")
    if release_sha != expected["release_sha256"]:
        raise ValueError("downloaded release manifest hash differs from the declaration")
    if config["authorization"]["allow_launch"] is not True:
        raise ValueError("completed evidence was not launch-authorized")
    run_deadline = deadline(config)
    if (_utc(marker.get("completed_utc"), "terminal completed_utc") >= run_deadline
            or _utc(host.get("completed_utc"), "host completed_utc") >= run_deadline):
        raise ValueError("run completed at or after its absolute deadline")
    worker_hashes = {
        host.get("worker_sha256"), marker.get("worker_sha256"), launch.get("worker_sha256")
    }
    if len(worker_hashes) != 1 or None in worker_hashes:
        raise ValueError("worker identity differs between launch, host and terminal marker")
    release_files = release.get("files")
    if not isinstance(release_files, dict):
        raise TypeError("release file table must be an object")
    bundled_worker_record = release_files.get("source/tools/cloud_performance/worker.ps1")
    if not isinstance(bundled_worker_record, dict):
        raise TypeError("release worker identity is absent")
    bundled_worker = bundled_worker_record.get("sha256")
    if bundled_worker != next(iter(worker_hashes)):
        raise ValueError("executed worker differs from the release source snapshot")
    identity = host.get("instance")
    if not isinstance(identity, dict):
        raise TypeError("host instance identity must be an object")
    if host.get("run_id") != config["run_id"]:
        raise ValueError("host run identity differs from the declaration")
    for field, declared in (("instanceType", config["instance_type"]),
                            ("imageId", config["ami_id"]),
                            ("region", config["region"]),
                            ("availabilityZone", config["availability_zone"])):
        if identity.get(field) != declared:
            raise ValueError(f"host instance {field} differs from the declaration")
    if identity.get("instanceId") != launch.get("instance_id"):
        raise ValueError("host instance ID differs from the launched instance")
    cpu = host.get("cpu_options")
    if not isinstance(cpu, dict):
        raise TypeError("host CPU options must be an object")
    if (cpu.get("core_count") != config["cpu_options"]["core_count"]
            or cpu.get("logical_processors") != (
                config["cpu_options"]["core_count"]
                * config["cpu_options"]["threads_per_core"])):
        raise ValueError("host CPU topology differs from the declaration")
    before_gpu = _gpu_identity(host.get("nvidia_smi_before"))
    after_gpu = _gpu_identity(host.get("nvidia_smi_after"))
    if before_gpu != after_gpu or host.get("nvidia_smi_identity") != after_gpu:
        raise ValueError("host GPU or driver identity changed during measurement")
    driver_versions = {identity.rsplit("|", 1)[-1] for identity in before_gpu}
    if driver_versions != {config["expected_ami_tags"]["NvidiaDriver"]}:
        raise ValueError("host NVIDIA driver differs from the AMI declaration")
    runtime = host.get("visual_cpp_runtime")
    runtime_names = {
        str(item.get("name", "")).upper() for item in runtime if isinstance(item, dict)
    } if isinstance(runtime, list) else set()
    if runtime_names != {"MSVCP140.DLL", "VCRUNTIME140.DLL", "VCRUNTIME140_1.DLL"}:
        raise ValueError("host Visual C++ runtime identity is incomplete")
    if "8c5e7fda-e8bf-4a96-9a85-a6e23a8c635c" not in str(host.get("power_plan", "")).lower():
        raise ValueError("host did not attest the High performance Windows power plan")
    video = host.get("video")
    if not isinstance(video, list):
        raise TypeError("host video inventory is missing")
    display_matches = []
    for adapter in video:
        if not isinstance(adapter, dict) or "NVIDIA" not in str(adapter.get("Name", "")).upper():
            continue
        try:
            wide_enough = int(adapter.get("CurrentHorizontalResolution")) >= config["workload"]["width"]
            tall_enough = int(adapter.get("CurrentVerticalResolution")) >= config["workload"]["height"]
            refresh_matches = abs(
                int(adapter.get("CurrentRefreshRate")) - config["workload"]["refresh_hz"]
            ) <= 1
        except (TypeError, ValueError):
            continue
        if wide_enough and tall_enough and refresh_matches:
            display_matches.append(adapter)
    if not display_matches:
        raise ValueError("host inventory has no NVIDIA display at the declared mode")
    if (not isinstance(host.get("cpu"), list) or not host["cpu"]
            or not isinstance(host.get("os"), dict)
            or not host["os"].get("build_number")
            or not isinstance(host.get("session"), dict)
            or host["session"].get("id") != 0
            or str(host["session"].get("user", "")).upper() != "SYSTEM"):
        raise ValueError("host CPU, OS or session identity is incomplete")
    if str(host["os"]["build_number"]) != config["expected_ami_tags"]["WindowsBuild"]:
        raise ValueError("host Windows build differs from the AMI declaration")
    aws_identity = host.get("aws_identity")
    if not isinstance(aws_identity, dict):
        raise TypeError("host AWS identity must be an object")
    role_name = str(aws_identity.get("role_name", ""))
    account_id = str(aws_identity.get("account_id", ""))
    caller_arn = str(aws_identity.get("caller_arn", ""))
    if (not role_name or account_id != str(identity.get("accountId", ""))
            or f":sts::{account_id}:assumed-role/{role_name}/" not in caller_arn):
        raise ValueError("host AWS calls did not use the instance profile role")
    git = release.get("git")
    if not isinstance(git, dict):
        raise TypeError("release Git identity must be an object")
    if (not isinstance(git.get("commit"), str)
            or not re.fullmatch(r"[0-9a-f]{40}", git["commit"])
            or not isinstance(git.get("dirty"), bool)):
        raise ValueError("release Git identity is incomplete")
    return host, release


def _validate_result(result: dict[str, Any], backend: dict[str, Any],
                     config: dict[str, Any]) -> dict[str, list[int]]:
    if not isinstance(result, dict):
        raise TypeError("benchmark result must be an object")
    if (result.get("schema_version") != 1
            or result.get("scope") != "linesweeper_frame_benchmark"
            or result.get("status") != "complete"):
        raise ValueError("benchmark result has the wrong schema, scope or status")
    if result.get("run") != config["run_id"]:
        raise ValueError("benchmark run differs from the declaration")
    if result.get("release_sha256") != config["bundle"]["release_sha256"]:
        raise ValueError("benchmark release differs from the declaration")
    started = _utc(result.get("started_utc"), "benchmark started_utc")
    finished = _utc(result.get("finished_utc"), "benchmark finished_utc")
    if finished < started or finished >= deadline(config):
        raise ValueError("benchmark timestamps cross the declared deadline")
    build = result.get("build")
    if not isinstance(build, dict):
        raise TypeError("benchmark build identity must be an object")
    if (build.get("configuration") != "release"
            or build.get("render_backend") != backend["name"]
            or result.get("measurement_class") != "hardware_raster"):
        raise ValueError("benchmark was not a Release hardware-raster measurement")
    device = result.get("render_device")
    if not isinstance(device, dict):
        raise TypeError("benchmark render_device must be an object")
    if device.get("kind") != "hardware":
        raise ValueError("benchmark backend or device kind differs from the declaration")
    presentation_recorded = _validate_presentation(device, backend["name"])
    if _vendor_id(device.get("vendor_id")) != backend["expected_vendor_id"]:
        raise ValueError("benchmark used an unexpected GPU vendor")
    if (re.search(backend["expected_device"], str(device.get("device_name", "")),
                  re.IGNORECASE) is None):
        raise ValueError("benchmark render device differs from the declaration")
    workload = result.get("workload")
    if not isinstance(workload, dict):
        raise TypeError("benchmark workload must be an object")
    expected = config["workload"]
    values = {
        "live_particles": expected["live_particles"],
        "warmup_frames": expected["warmup_frames"],
        "sample_frames": expected["sample_frames"],
        "refresh_hz": expected["refresh_hz"],
    }
    target_frame_ns = (1_000_000_000 + expected["refresh_hz"] // 2) // expected["refresh_hz"]
    resolution = workload.get("resolution", {})
    if (workload.get("name") != "full_well_top_out"
            or resolution != {"width": expected["width"], "height": expected["height"]}
            or workload.get("target_frame_ns") != target_frame_ns
            or any(workload.get(key) != value for key, value in values.items())):
        raise ValueError("benchmark workload differs from the declaration")
    timing = result.get("timing")
    if not isinstance(timing, dict):
        raise TypeError("benchmark timing must be an object")
    if (timing.get("clock") != "std::chrono::steady_clock"
            or timing.get("unit") != "nanoseconds"
            or timing.get("summary_method") != "nearest-rank"
            or timing.get("interval_scope") != (
                "software-paced frame-start interval; not display scan-out")):
        raise ValueError("benchmark timing contract differs")
    samples = result.get("samples")
    if not isinstance(samples, list) or len(samples) != expected["sample_frames"]:
        raise ValueError("raw benchmark sample count differs from the declaration")
    reported = result.get("summary")
    if not isinstance(reported, dict):
        raise TypeError("benchmark summary must be an object")
    pacing_recorded = (
        any(key in timing for key in ("pacer", "deadline_policy"))
        or any(phase in reported for phase in PACING_PHASES)
        or any(isinstance(sample, dict) and any(phase in sample for phase in PACING_PHASES)
               for sample in samples)
    )
    if pacing_recorded and (
            timing.get("pacer") != "win32_high_resolution_waitable_timer"
            or timing.get("deadline_policy") != "absolute_catch_up"):
        raise ValueError("benchmark pacing metadata is incomplete or unknown")
    # Both groups entered the producer together. Accept the old contract only
    # when neither is present, rather than silently downgrading a partial one.
    if pacing_recorded != presentation_recorded:
        raise ValueError("benchmark pacing and presentation metadata must be recorded together")
    phase_names = PHASES + PACING_PHASES if pacing_recorded else PHASES
    phases: dict[str, list[int]] = {phase: [] for phase in phase_names}
    for ordinal, sample in enumerate(samples):
        if not isinstance(sample, dict) or sample.get("ordinal") != ordinal:
            raise ValueError("benchmark sample ordinals are not contiguous")
        for phase in phase_names:
            phases[phase].append(_integer(sample.get(phase), f"sample.{phase}"))
        if sample["whole_frame_ns"] != sum(sample[phase] for phase in FRAME_PHASES):
            raise ValueError("benchmark whole-frame duration differs from its phase sum")
    if timing.get("sample_count") != len(samples):
        raise ValueError("timing sample_count differs from raw samples")
    if timing.get("scheduled_interval_ns") != phases["scheduled_interval_ns"]:
        raise ValueError("timing scheduled_interval_ns differs from raw scheduled intervals")
    for phase, samples_for_phase in phases.items():
        if reported.get(phase) != _summary(samples_for_phase):
            raise ValueError(f"benchmark {phase} summary differs from raw samples")
    return phases


def analyze(folder: str | Path) -> dict[str, Any]:
    root = Path(folder)
    successes = list(root.rglob("success.json"))
    failures = list(root.rglob("failure.json"))
    if failures:
        details = []
        for path in failures:
            try:
                details.append(_json(path))
            except (OSError, ValueError):
                details.append({"unreadable": str(path)})
        return _withheld("worker_failed", failures=details)
    if len(successes) != 1:
        return _withheld("terminal_success_marker_missing_or_ambiguous",
                         success_markers=len(successes))
    try:
        marker = _json(successes[0])
    except (OSError, ValueError) as exc:
        return _withheld("invalid_success_marker", error=str(exc))
    if (not isinstance(marker, dict) or marker.get("status") != "success"
            or not isinstance(marker.get("run_id"), str)):
        return _withheld("invalid_success_marker")
    try:
        _validate_evidence_files(successes[0], marker)
    except (OSError, TypeError, ValueError) as exc:
        return _withheld("evidence_manifest_invalid", error=str(exc))
    configs = list(root.rglob("config.json"))
    if len(configs) != 1:
        return _withheld("configuration_missing_or_ambiguous", config_files=len(configs))
    try:
        config = validate_config(_json(configs[0]), check_time=False)
    except (OSError, TypeError, ValueError, KeyError) as exc:
        return _withheld("configuration_invalid", error=str(exc))
    if marker["run_id"] != config["run_id"]:
        return _withheld("terminal_marker_run_mismatch")
    try:
        host, release = _validate_identity(root, marker, config)
    except (OSError, ValueError, KeyError, TypeError) as exc:
        return _withheld("host_or_release_identity_invalid", error=str(exc))
    result_files = sorted(root.rglob("result-*.json"))
    expected_count = (len(config["workload"]["backends"])
                      * config["workload"]["repetitions"])
    if len(result_files) != expected_count:
        return _withheld("fixed_sample_incomplete", expected_results=expected_count,
                         observed_results=len(result_files))
    declared = {item["name"]: item for item in config["workload"]["backends"]}
    observations: dict[str, list[dict[str, Any]]] = {name: [] for name in declared}
    pooled: dict[str, dict[str, list[int]]] = {
        name: {phase: [] for phase in PHASES} for name in declared
    }
    devices: dict[str, tuple[Any, ...]] = {}
    measurement_identity: tuple[bool, bool] | None = None
    seen: set[tuple[str, int]] = set()
    refresh_hz = config["workload"]["refresh_hz"]
    target_frame_ns = (1_000_000_000 + refresh_hz // 2) // refresh_hz
    try:
        for path in result_files:
            result = _json(path)
            if not isinstance(result, dict):
                raise TypeError("benchmark result must be an object")
            build = result.get("build")
            if not isinstance(build, dict):
                raise TypeError("benchmark build identity must be an object")
            backend_name = build.get("render_backend")
            if backend_name not in declared:
                raise ValueError("result names an undeclared backend")
            match = re.fullmatch(r"result-([a-z0-9-]+)-(\d{3})\.json", path.name)
            if match is None or match.group(1) != backend_name:
                raise ValueError("result filename does not bind backend and repetition")
            repetition = int(match.group(2))
            identity = (backend_name, repetition)
            if (repetition < 1 or repetition > config["workload"]["repetitions"]
                    or identity in seen):
                raise ValueError("result repetition is duplicate or outside the declaration")
            seen.add(identity)
            phases = _validate_result(result, declared[backend_name], config)
            device = result["render_device"]
            recorded = ("pacing_wait_ns" in phases,
                        all(field in device for field in PRESENTATION_FIELDS))
            if measurement_identity is not None and measurement_identity != recorded:
                raise ValueError("recorded and legacy measurement policies are mixed in one run")
            measurement_identity = recorded
            device_fields = DEVICE_FIELDS + (PRESENTATION_FIELDS if recorded[1] else ())
            device_identity = tuple(device.get(key) for key in device_fields)
            if backend_name in devices and devices[backend_name] != device_identity:
                raise ValueError("render device identity changed between repetitions")
            devices[backend_name] = device_identity
            observations[backend_name].append({
                "repetition": repetition,
                "source": path.relative_to(root).as_posix(),
                "started_utc": result["started_utc"],
                "finished_utc": result["finished_utc"],
                "sample_count": len(phases["whole_frame_ns"]),
                "render_device": device,
                "pacing_measurement": (
                    "recorded" if recorded[0] else "unrecorded_legacy"),
                "presentation_measurement": (
                    "recorded" if recorded[1] else "unrecorded_legacy"),
                "timing": {key: value for key, value in result["timing"].items()
                           if key != "scheduled_interval_ns"},
                "summary": {phase: _summary(values) for phase, values in phases.items()},
                **_diagnostics(phases, target_frame_ns),
            })
            for phase, values in phases.items():
                pooled[backend_name].setdefault(phase, []).extend(values)
        expected_seen = {
            (name, repetition)
            for name in declared
            for repetition in range(1, config["workload"]["repetitions"] + 1)
        }
        if seen != expected_seen:
            raise ValueError("fixed repetition matrix is incomplete")
    except (OSError, ValueError, KeyError, TypeError) as exc:
        return _withheld("result_validation_failed", error=str(exc))
    summaries: dict[str, Any] = {}
    for name, phases in pooled.items():
        first = observations[name][0]
        device_fields = DEVICE_FIELDS + (
            PRESENTATION_FIELDS if first["presentation_measurement"] == "recorded" else ())
        summaries[name] = {
            "render_device": dict(zip(device_fields, devices[name])),
            "pacing_measurement": first["pacing_measurement"],
            "presentation_measurement": first["presentation_measurement"],
            "sample_count": len(phases["whole_frame_ns"]),
            "target_frame_ns": target_frame_ns,
            "over_target_scheduled_count": sum(
                value > target_frame_ns for value in phases["scheduled_interval_ns"]),
            "summary": {phase: _summary(values) for phase, values in phases.items()},
            "repetition_ranges": _repetition_ranges(observations[name]),
            "repetitions": sorted(observations[name], key=lambda item: item["repetition"]),
        }
    return {
        "schema_version": 1,
        "complete": True,
        "interpretation": {
            "pooled_summary": (
                "All declared samples and repetitions are retained. Pooled percentiles can "
                "hide differences between processes; inspect repetitions and repetition_ranges."),
            "long_begin_or_present": (
                "CPU-observed begin/present durations exceeding half the target frame period. "
                "These include work and waits; their cause and GPU time are not measured. "
                "Episode sample indices are zero-based within each retained repetition. "
                "Alternating spans include both short and long samples; ties retain the first span."),
            "cadence": (
                "Frame-start intervals, not display scan-out. Realized Hz uses the mean "
                "interval; a median alone can hide alternating short and long intervals."),
        },
        "run_id": config["run_id"],
        "bundle_sha256": config["bundle"]["sha256"],
        "release_sha256": config["bundle"]["release_sha256"],
        "git": release["git"],
        "workload": config["workload"],
        "host": host,
        "backends": summaries,
    }
