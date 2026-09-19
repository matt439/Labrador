"""Shared validation and serialization with no AWS dependency."""

from __future__ import annotations

import copy
import hashlib
import json
import math
import re
from datetime import datetime, timezone
from pathlib import Path, PurePosixPath
from typing import Any

SCHEMA_VERSION = 1
WATCHDOG_SLACK_SECONDS = 120
RUN_STARTUP_SLACK_SECONDS = 300
RUN_ID = re.compile(r"[a-z0-9][a-z0-9-]{2,62}")
AWS_ID = {
    "ami_id": re.compile(r"ami-[0-9a-f]{8,17}"),
    "vpc_id": re.compile(r"vpc-[0-9a-f]{8,17}"),
    "subnet_id": re.compile(r"subnet-[0-9a-f]{8,17}"),
}
REGION = re.compile(r"[a-z]{2}(?:-[a-z0-9]+)+-[0-9]")
BUCKET = re.compile(r"(?=.{3,63}\Z)(?!xn--)(?!.*\.\.)(?!.*\.-)(?!.*-\.)[a-z0-9][a-z0-9.-]*[a-z0-9]")
HEX_SHA256 = re.compile(r"[0-9a-f]{64}")
BACKEND_NAME = re.compile(r"[a-z0-9][a-z0-9-]{0,31}")


def canonical(value: Any) -> bytes:
    """Return the one byte representation used for hashes and uploads."""
    return json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=True).encode("utf-8")


def digest(path: str | Path) -> str:
    hasher = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            hasher.update(block)
    return hasher.hexdigest()


def safe_relative(value: str, *, field: str = "path") -> PurePosixPath:
    """Validate a portable archive/object-relative path."""
    if not isinstance(value, str) or not value or "\\" in value:
        raise ValueError(f"{field} must be a non-empty forward-slash relative path")
    path = PurePosixPath(value)
    if (path.is_absolute() or any(":" in part for part in path.parts)
            or any(part in ("", ".", "..") for part in path.parts)):
        raise ValueError(f"{field} must remain inside its declared root")
    return path


def inside(root: str | Path, value: str, *, field: str = "path") -> Path:
    relative = safe_relative(value, field=field)
    base = Path(root).resolve()
    candidate = (base / Path(*relative.parts)).resolve()
    try:
        candidate.relative_to(base)
    except ValueError as exc:
        raise ValueError(f"{field} must remain inside its declared root") from exc
    return candidate


def _require_keys(value: dict[str, Any], keys: tuple[str, ...], scope: str) -> None:
    missing = [key for key in keys if key not in value]
    if missing:
        raise ValueError(f"missing {scope} field: {missing[0]}")


def _exact_keys(value: dict[str, Any], keys: tuple[str, ...], scope: str) -> None:
    _require_keys(value, keys, scope)
    unexpected = sorted(set(value) - set(keys))
    if unexpected:
        raise ValueError(f"unexpected {scope} field: {unexpected[0]}")


def _number(value: Any, field: str, *, minimum: float = 0, maximum: float | None = None) -> float:
    if (isinstance(value, bool) or not isinstance(value, (int, float))
            or not math.isfinite(value) or value < minimum):
        raise ValueError(f"{field} must be a number >= {minimum}")
    if maximum is not None and value > maximum:
        raise ValueError(f"{field} must be <= {maximum}")
    return float(value)


def _integer(value: Any, field: str, *, minimum: int = 0, maximum: int | None = None) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < minimum:
        raise ValueError(f"{field} must be an integer >= {minimum}")
    if maximum is not None and value > maximum:
        raise ValueError(f"{field} must be <= {maximum}")
    return value


def deadline(config: dict[str, Any]) -> datetime:
    raw = config["authorization"]["deadline_utc"]
    if not isinstance(raw, str):
        raise TypeError("authorization.deadline_utc must be an ISO-8601 string")
    try:
        parsed = datetime.fromisoformat(raw.replace("Z", "+00:00"))
    except ValueError as exc:
        raise ValueError("authorization.deadline_utc must be ISO-8601") from exc
    if parsed.tzinfo is None or parsed.utcoffset() != timezone.utc.utcoffset(parsed):
        raise ValueError("authorization.deadline_utc must be UTC and include its timezone")
    return parsed.astimezone(timezone.utc)


def run_prefix(config: dict[str, Any]) -> str:
    return f"{config['artifact_prefix']}/runs/{config['run_id']}"


def object_keys(config: dict[str, Any]) -> dict[str, str]:
    prefix = run_prefix(config)
    return {
        "bundle": f"{prefix}/input/payload.zip",
        "config": f"{prefix}/input/config.json",
        "worker": f"{prefix}/input/worker.ps1",
        "template": f"{prefix}/input/template.json",
        "launch": f"{prefix}/control/launch.json",
        "output": f"{prefix}/output",
        "ssm": f"{prefix}/ssm",
    }


def worker_source_info(config: dict[str, Any]) -> dict[str, str]:
    """The AWS-RunRemoteScript sourceInfo that fetches the worker.

    That document's downloadContent step takes an HTTPS URL of the object and
    rejects an s3:// URI as "invalid S3 path parameter" - which no dry run can
    see, because the first thing that parses the value is the agent on the
    runner. The regional virtual-hosted form keeps the request in the run's
    Region rather than bouncing through the global endpoint.
    """
    return {
        "path": f"https://{config['artifact_bucket']}.s3.{config['region']}.amazonaws.com/"
                f"{object_keys(config)['worker']}",
    }


def validate_config(document: dict[str, Any], *, now: datetime | None = None,
                    check_time: bool = True) -> dict[str, Any]:
    """Validate and return a normalized copy of a deployment document."""
    if not isinstance(document, dict):
        raise TypeError("configuration must be a JSON object")
    config = copy.deepcopy(document)
    _exact_keys(config, (
        "schema_version", "run_id", "region", "availability_zone", "ami_id",
        "instance_type", "cpu_options", "vpc_id", "subnet_id", "artifact_bucket",
        "artifact_prefix", "bundle", "expected_ami_tags", "root_device_name",
        "root_volume_gib", "associate_public_ip", "authorization", "workload",
    ), "deployment")
    if config["schema_version"] != SCHEMA_VERSION:
        raise ValueError(f"schema_version must be {SCHEMA_VERSION}")
    if not isinstance(config["run_id"], str) or not RUN_ID.fullmatch(config["run_id"]):
        raise ValueError("run_id must be 3-63 lowercase letters, digits or hyphens")
    if not isinstance(config["region"], str) or not REGION.fullmatch(config["region"]):
        raise ValueError("region is not a valid AWS region name")
    if (not isinstance(config["availability_zone"], str)
            or re.fullmatch(re.escape(config["region"]) + r"[a-z]",
                            config["availability_zone"]) is None):
        raise ValueError("availability_zone must belong to region")
    for field, pattern in AWS_ID.items():
        if not isinstance(config[field], str) or not pattern.fullmatch(config[field]):
            raise ValueError(f"{field} is not a valid AWS identifier")
    if not isinstance(config["instance_type"], str) or not re.fullmatch(r"[a-z0-9]+[a-z0-9.-]*", config["instance_type"]):
        raise ValueError("instance_type is invalid")
    cpu = config["cpu_options"]
    if not isinstance(cpu, dict):
        raise TypeError("cpu_options must be an object")
    _exact_keys(cpu, ("core_count", "threads_per_core"), "cpu_options")
    _integer(cpu["core_count"], "cpu_options.core_count", minimum=1, maximum=192)
    _integer(cpu["threads_per_core"], "cpu_options.threads_per_core", minimum=1, maximum=2)
    if not isinstance(config["artifact_bucket"], str) or not BUCKET.fullmatch(config["artifact_bucket"]):
        raise ValueError("artifact_bucket is not a valid S3 bucket name")
    prefix = safe_relative(config["artifact_prefix"], field="artifact_prefix")
    config["artifact_prefix"] = prefix.as_posix().rstrip("/")
    bundle = config["bundle"]
    if not isinstance(bundle, dict):
        raise TypeError("bundle must be an object")
    _exact_keys(bundle, ("path", "sha256", "release_sha256"), "bundle")
    bundle["path"] = safe_relative(bundle["path"], field="bundle.path").as_posix()
    if not isinstance(bundle["sha256"], str) or not HEX_SHA256.fullmatch(bundle["sha256"].lower()):
        raise ValueError("bundle.sha256 must be a SHA-256 hex digest")
    bundle["sha256"] = bundle["sha256"].lower()
    if (not isinstance(bundle["release_sha256"], str)
            or not HEX_SHA256.fullmatch(bundle["release_sha256"].lower())):
        raise ValueError("bundle.release_sha256 must be a SHA-256 hex digest")
    bundle["release_sha256"] = bundle["release_sha256"].lower()
    tags = config["expected_ami_tags"]
    if (not isinstance(tags, dict) or not tags
            or any(not isinstance(key, str) or not key or not isinstance(value, str) or not value
                   for key, value in tags.items())):
        raise ValueError("expected_ami_tags must be a non-empty string map")
    required_image_tags = {"Project", "ImageRole", "WindowsBuild", "NvidiaDriver"}
    if not required_image_tags <= set(tags):
        raise ValueError("expected_ami_tags must bind project, image role, Windows build and driver")
    if tags["Project"] != "Labrador" or not tags["WindowsBuild"].isdigit():
        raise ValueError("expected AMI project or Windows build is invalid")
    if (not isinstance(config["root_device_name"], str)
            or not config["root_device_name"].startswith("/dev/")):
        raise ValueError("root_device_name must be an EC2 device name")
    _integer(config["root_volume_gib"], "root_volume_gib", minimum=40, maximum=500)
    if not isinstance(config["associate_public_ip"], bool):
        raise TypeError("associate_public_ip must be boolean")

    authorization = config["authorization"]
    if not isinstance(authorization, dict):
        raise TypeError("authorization must be an object")
    _exact_keys(authorization, (
        "allow_launch", "deadline_utc", "max_elapsed_hours", "max_hourly_usd",
        "max_compute_usd",
    ), "authorization")
    if not isinstance(authorization["allow_launch"], bool):
        raise TypeError("authorization.allow_launch must be boolean")
    max_elapsed = _number(authorization["max_elapsed_hours"],
                          "authorization.max_elapsed_hours", minimum=0.05, maximum=24)
    max_hourly = _number(authorization["max_hourly_usd"],
                         "authorization.max_hourly_usd", minimum=0.01, maximum=100)
    max_compute = _number(authorization["max_compute_usd"],
                          "authorization.max_compute_usd", minimum=0.01, maximum=1000)
    if check_time:
        current = (now or datetime.now(timezone.utc)).astimezone(timezone.utc)
        remaining = (deadline(config) - current).total_seconds()
        if remaining <= 0:
            raise ValueError("authorization deadline has expired")
        if remaining > max_elapsed * 3600 + 1:
            raise ValueError("deadline exceeds authorization.max_elapsed_hours")
        worst_compute = max_hourly * (remaining + WATCHDOG_SLACK_SECONDS) / 3600
        if worst_compute > max_compute:
            raise ValueError("deadline and hourly ceiling exceed authorized compute budget")

    workload = config["workload"]
    if not isinstance(workload, dict):
        raise TypeError("workload must be an object")
    _exact_keys(workload, (
        "width", "height", "live_particles", "warmup_frames", "sample_frames",
        "refresh_hz", "repetitions", "backends",
    ), "workload")
    _integer(workload["width"], "workload.width", minimum=1, maximum=16384)
    _integer(workload["height"], "workload.height", minimum=1, maximum=16384)
    _integer(workload["live_particles"], "workload.live_particles", minimum=1, maximum=10_000_000)
    _integer(workload["warmup_frames"], "workload.warmup_frames", minimum=1, maximum=100_000)
    _integer(workload["sample_frames"], "workload.sample_frames", minimum=100, maximum=100_000)
    _integer(workload["refresh_hz"], "workload.refresh_hz", minimum=1, maximum=1000)
    _integer(workload["repetitions"], "workload.repetitions", minimum=1, maximum=100)
    fixed = {"width": 1280, "height": 720, "live_particles": 9600, "refresh_hz": 60}
    if any(workload[name] != value for name, value in fixed.items()):
        raise ValueError(
            "cloud workload must be the fixed 1280x720, 9,600-particle, 60 Hz reference")
    backends = workload["backends"]
    if not isinstance(backends, list) or not backends:
        raise ValueError("workload.backends must be a non-empty list")
    names: set[str] = set()
    raster_backends = {"d3d11", "d3d12", "gl", "vulkan"}
    for index, backend in enumerate(backends):
        scope = f"workload.backends[{index}]"
        if not isinstance(backend, dict):
            raise TypeError(f"{scope} must be an object")
        _exact_keys(backend, (
            "name", "executable", "expected_device", "expected_vendor_id",
        ), scope)
        if not isinstance(backend["name"], str) or not BACKEND_NAME.fullmatch(backend["name"]):
            raise ValueError(f"{scope}.name is invalid")
        if backend["name"] not in raster_backends:
            raise ValueError(f"{scope}.name is not a raster backend")
        if backend["name"] in names:
            raise ValueError("workload backend names must be unique")
        names.add(backend["name"])
        executable = safe_relative(backend["executable"], field=f"{scope}.executable")
        if executable.parts[0] != "payload" or executable.suffix.lower() != ".exe":
            raise ValueError(f"{scope}.executable must name a payload .exe")
        backend["executable"] = executable.as_posix()
        if not isinstance(backend["expected_device"], str) or not backend["expected_device"]:
            raise ValueError(f"{scope}.expected_device must be non-empty")
        _integer(backend["expected_vendor_id"], f"{scope}.expected_vendor_id",
                 minimum=0, maximum=0xFFFFFFFF)
        if backend["name"] == "gl" and backend["expected_vendor_id"] != 0:
            raise ValueError("OpenGL expected_vendor_id must be 0; that API exposes no PCI ID")
        try:
            re.compile(backend["expected_device"])
        except re.error as exc:
            raise ValueError(f"{scope}.expected_device must be a valid regex") from exc
    if check_time:
        current = (now or datetime.now(timezone.utc)).astimezone(timezone.utc)
        remaining = (deadline(config) - current).total_seconds()
        measured_seconds = (
            len(backends) * workload["repetitions"]
            * (workload["warmup_frames"] + workload["sample_frames"])
            / workload["refresh_hz"]
        )
        if remaining < measured_seconds + RUN_STARTUP_SLACK_SECONDS:
            raise ValueError("deadline cannot fit the declared frame matrix and startup allowance")
    return config


def load_config(path: str | Path, *, now: datetime | None = None,
                check_time: bool = True) -> dict[str, Any]:
    # Windows PowerShell 5.1 writes a UTF-8 BOM for `-Encoding UTF8`. Accept it
    # so an operator can prepare this Windows-only runner with the platform's
    # own shell without first learning a Python JSON-decoder distinction.
    with Path(path).open("r", encoding="utf-8-sig") as stream:
        document = json.load(stream)
    return validate_config(document, now=now, check_time=check_time)
