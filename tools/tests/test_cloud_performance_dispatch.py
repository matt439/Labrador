"""Offline dispatch tests using synthetic data and local fixtures."""

from __future__ import annotations

import json
import unittest
from datetime import datetime, timezone

from tools.cloud_performance import __main__ as cli
from tools.cloud_performance.common import validate_config, worker_source_info
from tools.tests.cloud_performance_fixtures import config


class DispatchTests(unittest.TestCase):
    def test_launch_dispatches_the_worker_from_an_https_object_url(self):
        # The helper below is only worth anything if the dispatch uses it;
        # the second real launch failed exactly the way the first did because
        # it did not. This is the call the agent parses, parameter for
        # parameter.
        now = datetime(2030, 1, 1, tzinfo=timezone.utc)
        document = validate_config(config(now), now=now)
        document["artifact_bucket"] = "example-bucket"

        call = cli.worker_dispatch(document, "i-0123456789abcdef0", config_sha="a" * 64,
                                   worker_sha="b" * 64, template_sha="c" * 64, now=now)

        self.assertEqual(call["DocumentName"], "AWS-RunRemoteScript")
        self.assertEqual(call["InstanceIds"], ["i-0123456789abcdef0"])
        self.assertEqual(call["Parameters"]["sourceType"], ["S3"])
        self.assertEqual(call["Parameters"]["sourceInfo"],
                         [json.dumps(worker_source_info(document))])
        self.assertTrue(json.loads(call["Parameters"]["sourceInfo"][0])["path"]
                        .startswith("https://example-bucket.s3.ap-southeast-2.amazonaws.com/"))
        command_line = call["Parameters"]["commandLine"][0]
        self.assertIn("-File .\\worker.ps1 ", command_line)
        self.assertIn("-WorkerSHA256 '" + "b" * 64 + "'", command_line)
        self.assertIn("-OutputPrefix 'labrador-performance/runs/reference-001/output'", command_line)
        self.assertEqual(call["OutputS3KeyPrefix"], "labrador-performance/runs/reference-001/ssm")
        self.assertLessEqual(call["TimeoutSeconds"], 600)

    def test_worker_source_is_an_https_object_url_in_the_run_region(self):
        # AWS-RunRemoteScript's downloadContent rejects an s3:// URI as an
        # "invalid S3 path parameter", and the first thing that parses the
        # value is the agent on the runner - the first real launch idled for
        # an hour on it. Pin the form here, where a dry run can see it.
        document = config()
        document["artifact_bucket"] = "example-bucket"
        document["region"] = "ap-southeast-2"

        source = worker_source_info(document)

        self.assertEqual(source, {
            "path": "https://example-bucket.s3.ap-southeast-2.amazonaws.com/"
                    "labrador-performance/runs/reference-001/input/worker.ps1",
        })
        self.assertFalse(source["path"].startswith("s3://"))


if __name__ == "__main__":
    unittest.main()
