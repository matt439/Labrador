"""Fresh synthetic declarations and JSON fixtures shared by cloud tooling tests."""

from __future__ import annotations

import json
from datetime import datetime, timedelta, timezone
from pathlib import Path


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
            "ConsoleUser": "benchmark",
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
