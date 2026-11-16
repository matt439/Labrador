"""Aggregate counter parsing, privacy boundaries and replay with offline API fixtures."""

from __future__ import annotations

import contextlib
import copy
from datetime import datetime, timedelta, timezone
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

from tools.tests.tool_fixtures import load_tool

counters = load_tool("tools/release_counters.py")
REPOSITORY = Path(__file__).resolve().parents[2]
NAME = "fixture/engine"
TAG = "v0.4.0"
NOW = "2026-11-17T00:00:00Z"
SECRET = "DO_NOT_PERSIST_IDENTITY_OR_TOKEN"
ARGS = ["--repository", NAME, "--release", TAG, "--window", counters.WINDOW]


def asset(name: str, identifier: int, downloads: int) -> dict:
    return {"name": name, "id": identifier, "download_count": downloads, "state": "uploaded",
            "created_at": "2026-11-01T00:00:00Z", "updated_at": "2026-11-02T00:00:00Z",
            "uploader": {"login": SECRET}}


def fixtures() -> dict:
    first = datetime(2026, 11, 1, tzinfo=timezone.utc)
    # The provider's documented example also has fifteen UTC bucket dates.
    # Aggregate uniques are not the sum of these overlapping daily uniques.
    return {
        f"repos/{NAME}": {"full_name": NAME, "private": False, "owner": {"login": SECRET}},
        f"repos/{NAME}/releases/tags/{TAG}": {"id": 7, "tag_name": TAG, "draft": False,
            "published_at": "2026-11-03T12:00:00Z", "author": {"login": SECRET}},
        f"repos/{NAME}/releases/7/assets?per_page=100": [
            [asset(f"labrador-docs-{TAG}.tar.gz", 70, 123)],
            [asset(f"labrador-starter-{TAG}.zip", 71, 0), {"name": "unselected.txt", "uploader": SECRET}]],
        f"repos/{NAME}/traffic/views?per=day": {"count": 100, "uniques": 7, "views": [
            {"timestamp": (first + timedelta(days=index)).isoformat().replace("+00:00", "Z"),
             "count": 10, "uniques": 6, "referrer": SECRET} for index in range(15)],
            "referrers": SECRET},
    }


class ApiFixture:
    def __init__(self):
        self.data = fixtures()
        self.calls = []

    def __call__(self, endpoint, *, paginate=False):
        self.calls.append((endpoint, paginate))
        value = self.data[endpoint]
        if isinstance(value, Exception):
            raise value
        return copy.deepcopy(value)


def capture(*, traffic=True) -> dict:
    return counters.collect(NAME, TAG, include_traffic=traffic, api=ApiFixture(), now=lambda: NOW)


class ParsingTests(unittest.TestCase):
    def test_fifteen_buckets_keep_provider_totals_and_do_not_sum_daily_uniques(self):
        result = capture()
        self.assertEqual(result["traffic"]["data"], {"count": 100, "uniques": 7,
            "returned_bucket_dates": [f"2026-11-{day:02}T00:00:00Z" for day in range(1, 16)]})
        self.assertNotIn(SECRET, counters.canonical_json(result))

    def test_paginated_assets_distinguish_observed_zero_from_absence_and_retain_identity(self):
        api = ApiFixture()
        result = counters.collect(NAME, TAG, include_traffic=False, api=api, now=lambda: NOW)
        starter = result["assets"]["starter"]
        self.assertEqual((starter["status"], starter["download_count"], starter["id"]), ("observed", 0, 71))
        self.assertEqual(starter["created_at"], "2026-11-01T00:00:00Z")
        self.assertEqual(starter["updated_at"], "2026-11-02T00:00:00Z")
        self.assertTrue(api.calls[-1][1], "asset collection must paginate")
        api.data[f"repos/{NAME}/releases/7/assets?per_page=100"] = [[]]
        missing = counters.collect(NAME, TAG, include_traffic=False, api=api, now=lambda: NOW)
        self.assertEqual(missing["assets"]["starter"], {"status": "not_present",
                         "name": f"labrador-starter-{TAG}.zip", "download_count": None})

    def test_private_repositories_unpublished_releases_and_scope_mismatches_stop_collection(self):
        for endpoint, field, value in ((f"repos/{NAME}", "private", True),
                (f"repos/{NAME}", "full_name", "other/repository"),
                (f"repos/{NAME}/releases/tags/{TAG}", "draft", True),
                (f"repos/{NAME}/releases/tags/{TAG}", "published_at", None),
                (f"repos/{NAME}/releases/tags/{TAG}", "tag_name", "v0.3.0")):
            with self.subTest(field=field):
                api = ApiFixture()
                api.data[endpoint][field] = value
                with self.assertRaises(counters.SnapshotError):
                    counters.collect(NAME, TAG, include_traffic=True, api=api, now=lambda: NOW)
                self.assertFalse(any("traffic/" in path for path, _ in api.calls))

    def test_bad_counts_duplicate_assets_and_incomplete_uploads_are_not_observations(self):
        for field, value in (("download_count", True), ("download_count", -1),
                             ("id", 0), ("created_at", "yesterday"), ("state", "starter")):
            with self.subTest(field=field):
                pages = fixtures()[f"repos/{NAME}/releases/7/assets?per_page=100"]
                pages[1][0][field] = value
                with self.assertRaises(counters.SnapshotError):
                    counters.assets_from_pages(pages, TAG)
        pages = fixtures()[f"repos/{NAME}/releases/7/assets?per_page=100"]
        pages.append([copy.deepcopy(pages[1][0])])
        with self.assertRaisesRegex(counters.SnapshotError, "duplicate"):
            counters.assets_from_pages(pages, TAG)
        with self.assertRaises(counters.SnapshotError):
            counters.assets_from_pages([], TAG)

    def test_empty_traffic_is_observed_zero_and_unknown_access_is_unavailable(self):
        api = ApiFixture()
        api.data[f"repos/{NAME}/traffic/views?per=day"] = {"count": 0, "uniques": 0, "views": []}
        observed = counters.collect(NAME, TAG, include_traffic=True, api=api, now=lambda: NOW)
        self.assertEqual(observed["traffic"]["status"], "observed")
        self.assertEqual(observed["traffic"]["data"]["returned_bucket_dates"], [])
        api.data[f"repos/{NAME}/traffic/views?per=day"] = counters.FetchError("http_error", 403)
        unavailable = counters.collect(NAME, TAG, include_traffic=True, api=api, now=lambda: NOW)
        self.assertEqual(unavailable["traffic"]["status"], "unavailable")
        self.assertNotIn("data", unavailable["traffic"])

    def test_malformed_traffic_cannot_become_a_zero_or_adoption_count(self):
        api = ApiFixture()
        api.data[f"repos/{NAME}/traffic/views?per=day"]["uniques"] = True
        result = counters.collect(NAME, TAG, include_traffic=True, api=api, now=lambda: NOW)
        self.assertEqual((result["traffic"]["status"], result["traffic"]["reason"]),
                         ("unavailable", "invalid_payload"))
        self.assertNotIn("data", result["traffic"])

    def test_fractional_or_non_midnight_dates_are_not_daily_bucket_boundaries(self):
        for date in ("2026-11-01T00:00:00.123Z", "2026-11-01T01:00:00Z", "2026-11-01T00:00:00+10:00"):
            with self.subTest(timestamp=date), self.assertRaises(counters.SnapshotError):
                counters.traffic_totals(1, 1, [date])

    def test_public_reports_and_hashes_are_independent_of_private_traffic(self):
        first = capture()
        first["owner_private_metadata"] = SECRET
        second = copy.deepcopy(first)
        second["traffic"] = {"status": "unavailable", "reason": "http_error", "http_status": 403,
                             "captured_at": NOW}
        public, private = counters.reports(first, replay=False)
        denied_public, denied_private = counters.reports(second, replay=False)
        self.assertEqual(public, denied_public)
        self.assertNotIn(SECRET, counters.canonical_json(public))
        self.assertNotEqual(private["inputs_sha256"], denied_private["inputs_sha256"])
        self.assertEqual(public["download_window"], "asset_lifetime_to_capture")
        self.assertEqual(public["source"]["api_host"], "api.github.com")
        for name in ("traffic", "window", "reason", "http_status", "uniques"):
            self.assertNotIn(name, public)
        public_text, _ = counters.markdown(public, private)
        self.assertNotIn("Provider totals", public_text)
        self.assertNotIn("HTTP 403", public_text)


class CommandTests(unittest.TestCase):
    def run_main(self, arguments, *, api=None):
        output, errors = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors):
            code = counters.main(ARGS + arguments, api=api or mock.Mock(side_effect=AssertionError("unexpected account read")),
                                 now=lambda: NOW)
        return code, output.getvalue(), errors.getvalue()

    def test_plan_and_replay_perform_no_account_reads_and_replay_is_deterministic(self):
        self.assertEqual(self.run_main([])[0], 0)
        self.assertEqual(self.run_main(["--include-traffic"])[0], 0)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "capture.json"
            source.write_text(counters.canonical_json(capture()), encoding="utf-8")
            for name in ("first", "second"):
                code, _, errors = self.run_main(["--replay", str(source), "--include-traffic", "--output-dir", str(root / name)])
                self.assertEqual(code, 0, errors)
            for path in (root / "first").iterdir():
                self.assertEqual(path.read_bytes(), (root / "second" / path.name).read_bytes())
            self.assertIn("Replay", (root / "first/public.md").read_text())
            self.assertEqual(json.loads((root / "first/public.json").read_text())["mode"], "replay")
            self.assertEqual(json.loads((root / "first/capture-private.json").read_text())["retained_by"], "offline_replay")

    def test_real_cli_replays_without_gh_on_path(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "capture.json"
            source.write_text(counters.canonical_json(capture()), encoding="utf-8")
            result = subprocess.run([sys.executable, str(REPOSITORY / "tools/release_counters.py"), *ARGS,
                                     "--replay", str(source), "--include-traffic", "--output-dir", str(root / "snapshot")],
                                    env={**os.environ, "PATH": ""}, capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertEqual(json.loads((root / "snapshot/public.json").read_text())["mode"], "replay")

    def test_public_collection_never_requests_traffic_or_persists_identities(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "snapshot"
            api = ApiFixture()
            code, _, errors = self.run_main(["--collect", "--output-dir", str(output)], api=api)
            self.assertEqual(code, 0, errors)
            self.assertEqual(len(api.calls), 3)
            self.assertFalse(any("traffic/" in endpoint for endpoint, _ in api.calls))
            self.assertEqual({path.name for path in output.iterdir()}, {"public.json", "public.md", "capture-public.json"})
            self.assertTrue(all(SECRET not in path.read_text() for path in output.iterdir()))

    def test_private_denial_writes_a_separate_unavailable_companion_without_contaminating_public_data(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "snapshot"
            api = ApiFixture()
            api.data[f"repos/{NAME}/traffic/views?per=day"] = counters.FetchError("http_error", 403)
            code, _, errors = self.run_main(["--collect", "--include-traffic", "--output-dir", str(output)], api=api)
            self.assertEqual(code, 1, errors)
            self.assertTrue((output / "capture-private.json").exists())
            private = json.loads((output / "traffic-private.json").read_text())
            public = json.loads((output / "public.json").read_text())
            self.assertEqual(private["status"], "unavailable")
            self.assertNotIn("data", private)
            self.assertNotIn("http_status", public)
            self.assertNotIn("traffic", public)

    def test_replay_without_private_opt_in_drops_a_retained_private_result(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "capture.json"
            source.write_text(counters.canonical_json(capture()), encoding="utf-8")
            code, _, errors = self.run_main(["--replay", str(source), "--output-dir", str(root / "public-only")])
            self.assertEqual(code, 0, errors)
            persisted = json.loads((root / "public-only/capture-public.json").read_text())
            self.assertNotIn("traffic", persisted)
            self.assertFalse((root / "public-only/traffic-private.json").exists())

    def test_existing_destinations_are_preserved_before_any_account_read(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            prior = root / "prior.json"
            prior.write_text("retain me", encoding="utf-8")
            code, _, errors = self.run_main(["--collect", "--include-traffic", "--output-dir", str(root)])
            self.assertEqual(code, 2)
            self.assertIn("already exists", errors)
            self.assertEqual(prior.read_text(), "retain me")

    def test_bad_replay_scope_and_unknown_schema_fail_without_output(self):
        for field, value in (("schema_version", True), ("repository", "other/repo"),
                             ("traffic_window", "last-year"), ("repository_public", False),
                             ("api_version", "2022-13-99")):
            with self.subTest(field=field), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                raw = capture()
                raw[field] = value
                source = root / "bad.json"
                source.write_text(counters.canonical_json(raw), encoding="utf-8")
                code, _, _ = self.run_main(["--replay", str(source), "--output-dir", str(root / "snapshot")])
                self.assertEqual(code, 2)
                self.assertFalse((root / "snapshot").exists())

    def test_get_only_transport_uses_explicit_github_host_and_pagination_without_leaking_errors(self):
        commands = []
        payloads = fixtures()

        def runner(command, **kwargs):
            commands.append(command)
            self.assertEqual(command[command.index("--method") + 1], "GET")
            self.assertEqual(command[command.index("--hostname") + 1], "github.com")
            self.assertNotIn(SECRET, " ".join(command))
            self.assertNotIn("GH_DEBUG", kwargs["env"])
            self.assertEqual(kwargs["env"]["GH_PROMPT_DISABLED"], "1")
            return subprocess.CompletedProcess(command, 0, json.dumps(payloads[command[-1]]), "")

        def api(endpoint, *, paginate=False):
            return counters.gh_api(endpoint, paginate=paginate, runner=runner)

        with mock.patch.dict(os.environ, {"GH_TOKEN": SECRET, "GH_DEBUG": "api"}):
            result = counters.collect(NAME, TAG, include_traffic=True, api=api, now=lambda: NOW)
        self.assertEqual(len(commands), 4)
        self.assertIn("--paginate", commands[2])
        self.assertIn("--slurp", commands[2])
        self.assertNotIn(SECRET, counters.canonical_json(result))
        with self.assertRaisesRegex(counters.FetchError, r"^http_error \(HTTP 403\)$"):
            counters.gh_api("repos/fixture/engine", runner=lambda command, **kwargs:
                subprocess.CompletedProcess(command, 1, SECRET, "gh: " + SECRET + " (HTTP 403)"))

    def test_no_workflow_invokes_collection(self):
        for workflow in (REPOSITORY / ".github/workflows").glob("*.yml"):
            self.assertNotIn("release_counters.py", workflow.read_text(encoding="utf-8"), workflow.name)


if __name__ == "__main__":
    unittest.main()
