"""Offline config tests using synthetic data and local fixtures."""

from __future__ import annotations

import json
import tempfile
import unittest
from datetime import datetime, timedelta, timezone
from pathlib import Path

from tools.cloud_performance import infrastructure
from tools.cloud_performance.common import load_config, validate_config
from tools.tests.cloud_performance_fixtures import config


class ConfigTests(unittest.TestCase):
    def test_console_user_is_required_before_authorized_template_generation(self):
        now = datetime(2030, 1, 1, tzinfo=timezone.utc)
        for value in (None, "", " ", "\t\r\n"):
            with self.subTest(value=value):
                document = config(now)
                document["authorization"]["allow_launch"] = True
                if value is None:
                    del document["expected_ami_tags"]["ConsoleUser"]
                else:
                    document["expected_ami_tags"]["ConsoleUser"] = value
                with self.assertRaises(ValueError):
                    validate_config(document, now=now)
                with self.assertRaises(ValueError):
                    infrastructure.template(document, require_authorization=True, now=now)

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


if __name__ == "__main__":
    unittest.main()
