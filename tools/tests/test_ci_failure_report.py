"""CI observations from captured API shapes; every request is offline."""

from __future__ import annotations

import json
from pathlib import Path
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch
from urllib.parse import urlencode

from tools.tests.tool_fixtures import load_tool

reporter = load_tool("tools/ci_failure_report.py")
base = "/repos/owner/repo/actions/runs"
start = "2026-11-01T00:00:00Z"
end = "2026-11-02T00:00:00Z"


def scope(**changes):
    return reporter.Scope(**{"repository": "owner/repo", "since": start, "until": end,
                             "attempts": "latest", **changes})


def listing_path(selection, page=1):
    parameters = {"created": f"{selection.since}..{selection.until}"}
    if selection.branch is not None:
        parameters["branch"] = selection.branch
    return f"{base}?{urlencode(parameters)}&per_page={selection.page_size}&page={page}"


def attempt_path(identifier=10, number=1):
    return f"{base}/{identifier}/attempts/{number}"


def jobs_path(selection, identifier=10, number=1, page=1):
    return f"{attempt_path(identifier, number)}/jobs?per_page={selection.page_size}&page={page}"


def run(identifier=10, attempt=1, **changes):
    return {"id": identifier, "run_attempt": attempt, "name": "CI", "workflow_id": 7,
            "path": ".github/workflows/ci.yml", "head_branch": "work-in-progress", "head_sha": "a" * 40,
            "head_repository": {"full_name": "owner/repo"}, "event": "pull_request", "status": "completed",
            "conclusion": "failure", "created_at": "2026-11-01T12:00:00Z", "updated_at": "2026-11-03T12:00:00Z",
            "html_url": f"https://github.invalid/runs/{identifier}", **changes}


def step(number=1, **changes):
    return {"number": number, "name": "Test pages", "status": "completed", "conclusion": "failure", **changes}


def job(identifier=100, run_id=10, attempt=1, **changes):
    return {"id": identifier, "run_id": run_id, "run_attempt": attempt, "name": "wasm", "status": "completed",
            "conclusion": "failure", "html_url": f"https://github.invalid/jobs/{identifier}", "steps": [step()], **changes}


class Api:
    def __init__(self, responses):
        self.responses = responses
        self.calls = []

    def __call__(self, path):
        self.calls.append(path)
        if path not in self.responses:
            raise AssertionError(f"unexpected GET: {path}")
        value = self.responses[path]
        if isinstance(value, Exception):
            raise value
        return value


def fixture(selection=None, listed=None, metadata=None, jobs=None):
    selection = selection or scope()
    listed = [run()] if listed is None else listed
    metadata = {(item["id"], item["run_attempt"]): item for item in listed} if metadata is None else metadata
    jobs = {(10, 1): [job()]} if jobs is None else jobs
    responses = {listing_path(selection): {"total_count": len(listed), "workflow_runs": listed}}
    for (identifier, number), value in metadata.items():
        responses[attempt_path(identifier, number)] = value
    for (identifier, number), values in jobs.items():
        responses[jobs_path(selection, identifier, number)] = {"total_count": len(values), "jobs": values}
    return selection, Api(responses)


class CiFailureReportTests(unittest.TestCase):
    def test_completed_failed_step_retains_workflow_ref_attempt_and_sources(self):
        selection, api = fixture()
        result = reporter.collect(selection, api)
        self.assertTrue(result["query_complete"])
        group, = result["step_groups"]
        self.assertEqual((group["workflow_id"], group["workflow_path"], group["head_branch"], group["event"],
                          group["attempt"], group["job_name"], group["step_name"], group["context"], group["count"]),
                         (7, ".github/workflows/ci.yml", "work-in-progress", "pull_request", 1, "wasm", "Test pages", "failure", 1))
        self.assertEqual(group["sources"][0]["run_id"], 10)
        self.assertEqual(group["sources"][0]["job_id"], 100)
        self.assertEqual(result["request_paths"], api.calls)

    def test_latest_and_all_attempts_do_not_merge_a_failed_attempt_with_a_successful_retry(self):
        earlier = run(attempt=1)
        later = run(attempt=2, conclusion="success")
        for mode in ("latest", "all"):
            with self.subTest(mode=mode):
                selection = scope(attempts=mode)
                _, api = fixture(selection, [later], {(10, 1): earlier, (10, 2): later},
                                 {(10, 1): [job()], (10, 2): [job(200, attempt=2, conclusion="success", steps=[step(conclusion="success")])]})
                result = reporter.collect(selection, api)
                self.assertTrue(result["query_complete"])
                self.assertEqual([item["run_attempt"] for item in result["attempts"]], [2] if mode == "latest" else [1, 2])
                self.assertEqual(result["selected_runs"][0]["prior_attempts_not_selected"], 1 if mode == "latest" else 0)
                self.assertEqual(len(result["step_groups"]), 0 if mode == "latest" else 1)
                self.assertNotIn(base + "/10/jobs", " ".join(api.calls))

    def test_failed_step_in_a_successful_job_is_still_an_observed_failure(self):
        selection, api = fixture(listed=[run(conclusion="success")], jobs={(10, 1): [job(conclusion="success")]})
        result = reporter.collect(selection, api)
        group, = result["step_groups"]
        self.assertEqual(group["context"], "failure")
        self.assertEqual(group["job_conclusion"], "success")
        self.assertEqual(result["attempts"][0]["conclusion"], "success")

    def test_cancelled_and_incomplete_contexts_preserve_failed_steps_without_merging_counts(self):
        for status, conclusion, context in (("completed", "cancelled", "cancelled"), ("in_progress", None, "incomplete")):
            with self.subTest(context=context):
                selection, api = fixture(listed=[run(status=status, conclusion=conclusion)])
                result = reporter.collect(selection, api)
                group, = result["step_groups"]
                self.assertEqual(group["context"], context)
                self.assertEqual(group["step_conclusion"], "failure")
                self.assertEqual(result["attempts"][0]["status"], status)
                self.assertEqual(result["attempts"][0]["conclusion"], conclusion)

    def test_missing_steps_cancelled_jobs_and_no_jobs_remain_explicit(self):
        selection, api = fixture(jobs={(10, 1): [job(conclusion="cancelled", steps=None)]})
        result = reporter.collect(selection, api)
        self.assertFalse(result["step_groups"])
        self.assertEqual(result["job_outcomes"][0]["job_conclusion"], "cancelled")
        self.assertTrue(any("steps missing" in item["message"] for item in result["data_gaps"]))
        selection, api = fixture(jobs={(10, 1): []})
        result = reporter.collect(selection, api)
        self.assertTrue(any("no jobs returned" in item["message"] for item in result["data_gaps"]))
        self.assertEqual(len(result["attempts"]), 1)

    def test_failed_job_without_failed_step_does_not_invent_a_step(self):
        selection, api = fixture(jobs={(10, 1): [job(steps=[step(conclusion="success")])]})
        result = reporter.collect(selection, api)
        self.assertFalse(result["step_groups"])
        self.assertEqual(result["job_outcomes"][0]["job_conclusion"], "failure")
        self.assertTrue(any("no reported failed step" in item["message"] for item in result["data_gaps"]))

    def test_missing_conclusions_and_filter_attribution_remain_unknown(self):
        selection, api = fixture(listed=[run(conclusion=None)])
        result = reporter.collect(selection, api)
        self.assertEqual(result["step_groups"][0]["context"], "incomplete")
        self.assertTrue(any("completed attempt conclusion missing" in item["message"] for item in result["data_gaps"]))
        selection = scope(workflow_path=".github/workflows/ci.yml")
        _, api = fixture(selection, [run(path=None)], metadata={}, jobs={})
        result = reporter.collect(selection, api)
        self.assertFalse(result["selected_runs"])
        self.assertTrue(any("filter could not be evaluated" in item["message"] for item in result["data_gaps"]))

    def test_window_uses_original_creation_and_is_half_open_even_when_retry_runs_later(self):
        selection = scope()
        listed = [run(), run(11, created_at=end), run(12, created_at="2026-10-31T23:59:59Z")]
        _, api = fixture(selection, listed, {(10, 1): run(run_started_at="2026-11-03T12:00:00Z")})
        result = reporter.collect(selection, api)
        self.assertEqual([item["id"] for item in result["selected_runs"]], [10])
        self.assertEqual(result["attempts"][0]["run_started_at"], "2026-11-03T12:00:00Z")

    def test_exact_workflow_and_branch_filters_keep_event_and_sha(self):
        selection = scope(branch="review", workflow_path=".github/workflows/ci.yml")
        chosen = run(head_branch="review", path=".github/workflows/ci.yml@review")
        listed = [chosen, run(11, head_branch="master"), run(12, head_branch="review", path=".github/workflows/site.yml")]
        _, api = fixture(selection, listed, {(10, 1): chosen})
        result = reporter.collect(selection, api)
        self.assertEqual([item["id"] for item in result["selected_runs"]], [10])
        self.assertEqual(result["attempts"][0]["head_sha"], "a" * 40)

    def test_workflow_ids_and_source_repositories_prevent_ambiguous_ref_aggregation(self):
        selection = scope()
        listed = [run(), run(11, workflow_id=8, head_repository={"full_name": "fork/repo"})]
        _, api = fixture(selection, listed, jobs={(10, 1): [job()], (11, 1): [job(110, run_id=11)]})
        result = reporter.collect(selection, api)
        self.assertEqual(len(result["step_groups"]), 2)
        self.assertEqual({item["head_repository"] for item in result["step_groups"]}, {"owner/repo", "fork/repo"})

    def test_pages_are_followed_and_duplicates_are_not_counted_twice(self):
        selection = scope(page_size=1)
        _, api = fixture(selection, [run()], jobs={(10, 1): [job()]})
        api.responses[listing_path(selection)] = {"total_count": 2, "workflow_runs": [run()]}
        api.responses[listing_path(selection, 2)] = {"total_count": 2, "workflow_runs": [run()]}
        api.responses[jobs_path(selection)] = {"total_count": 2, "jobs": [job()]}
        api.responses[jobs_path(selection, page=2)] = {"total_count": 2, "jobs": [job()]}
        result = reporter.collect(selection, api)
        self.assertFalse(result["query_complete"])
        self.assertEqual(result["step_groups"][0]["count"], 1)
        self.assertTrue(any("duplicate run" in item["message"] for item in result["errors"]))
        self.assertTrue(any("duplicate job" in item["message"] for item in result["errors"]))

    def test_changed_totals_and_page_caps_are_partial_coverage(self):
        selection = scope(page_size=1, max_run_pages=2)
        _, api = fixture(selection, [run()])
        api.responses[listing_path(selection)] = {"total_count": 2, "workflow_runs": [run()]}
        api.responses[listing_path(selection, 2)] = {"total_count": 3, "workflow_runs": [run(11)]}
        api.responses[attempt_path(11)] = run(11)
        api.responses[jobs_path(selection, 11)] = {"total_count": 0, "jobs": []}
        result = reporter.collect(selection, api)
        self.assertFalse(result["query_complete"])
        self.assertTrue(any("total_count changed" in item["message"] for item in result["errors"]))
        self.assertTrue(any("pagination limit" in item["message"] for item in result["errors"]))

    def test_job_page_failure_keeps_observed_sources_and_exposes_missing_tail(self):
        selection = scope(page_size=1)
        _, api = fixture(selection)
        api.responses[jobs_path(selection)] = {"total_count": 2, "jobs": [job()]}
        api.responses[jobs_path(selection, page=2)] = reporter.QueryError("HTTP 403 read denied")
        result = reporter.collect(selection, api)
        self.assertFalse(result["query_complete"])
        self.assertEqual(result["step_groups"][0]["count"], 1)
        self.assertEqual(result["step_groups"][0]["sources"][0]["job_id"], 100)
        self.assertTrue(any("403" in item["message"] for item in result["errors"]))

    def test_search_cap_and_selected_run_cap_are_visible(self):
        selection = scope(page_size=1, max_run_pages=1)
        _, api = fixture(selection)
        api.responses[listing_path(selection)] = {"total_count": 1001, "workflow_runs": [run()]}
        result = reporter.collect(selection, api)
        self.assertFalse(result["query_complete"])
        self.assertTrue(any("caps results at 1000" in item["message"] for item in result["errors"]))
        selection = scope(max_runs=1)
        _, api = fixture(selection, [run(), run(11)], {(10, 1): run()})
        result = reporter.collect(selection, api)
        self.assertEqual([item["id"] for item in result["selected_runs"]], [10])
        self.assertTrue(any("run limit" in item["message"] for item in result["errors"]))

    def test_all_attempt_cap_keeps_latest_attempts_and_names_omitted_prior_attempts(self):
        selection = scope(attempts="all", max_attempts=2)
        _, api = fixture(selection, [run(attempt=4)], {(10, 3): run(attempt=3), (10, 4): run(attempt=4)},
                         {(10, 3): [job(attempt=3)], (10, 4): [job(400, attempt=4)]})
        result = reporter.collect(selection, api)
        self.assertEqual([item["run_attempt"] for item in result["attempts"]], [3, 4])
        self.assertEqual(result["selected_runs"][0]["prior_attempts_not_selected"], 2)
        self.assertFalse(result["query_complete"])

    def test_unavailable_prior_attempt_is_not_a_failure_or_success(self):
        selection = scope(attempts="all")
        _, api = fixture(selection, [run(attempt=2)], {(10, 1): reporter.QueryError("HTTP 404 unavailable"), (10, 2): run(attempt=2)},
                         {(10, 2): [job(200, attempt=2)]})
        result = reporter.collect(selection, api)
        self.assertFalse(result["query_complete"])
        self.assertEqual([item["run_attempt"] for item in result["attempts"]], [2])
        self.assertIn("404", result["errors"][0]["message"])

    def test_identity_mismatches_are_not_attributed_to_requested_attempt(self):
        selection, api = fixture(metadata={(10, 1): run(attempt=2)})
        result = reporter.collect(selection, api)
        self.assertFalse(result["query_complete"])
        self.assertFalse(result["attempts"])
        selection, api = fixture(jobs={(10, 1): [job(attempt=2)]})
        result = reporter.collect(selection, api)
        self.assertFalse(result["step_groups"])
        self.assertTrue(any("another run/attempt" in item["message"] for item in result["data_gaps"]))

    def test_missing_attempt_and_optional_job_attempt_are_distinguished(self):
        missing = run()
        missing.pop("run_attempt")
        selection, api = fixture(listed=[missing], metadata={}, jobs={})
        result = reporter.collect(selection, api)
        self.assertFalse(result["attempts"])
        self.assertTrue(any("no attempt guessed" in item["message"] for item in result["data_gaps"]))
        supplied = job()
        supplied.pop("run_attempt")
        selection, api = fixture(jobs={(10, 1): [supplied]})
        self.assertEqual(reporter.collect(selection, api)["step_groups"][0]["count"], 1)

    def test_request_budget_and_auth_errors_remain_reported_data(self):
        selection, api = fixture()
        captured = reporter.CaptureApi(api, 1)
        result = reporter.collect(selection, captured)
        self.assertFalse(result["query_complete"])
        self.assertEqual(len(api.calls), 1)
        self.assertTrue(any("budget reached" in item["message"] for item in result["errors"]))
        failing = Api({listing_path(selection): reporter.QueryError("HTTP 401 Bad credentials")})
        result = reporter.collect(selection, failing)
        self.assertFalse(result["query_complete"])
        self.assertFalse(result["step_groups"])
        self.assertIn("401", result["errors"][0]["message"])

    def test_timeout_is_captured_without_discarding_the_partial_report(self):
        selection = scope()
        def timeout(path):
            raise subprocess.TimeoutExpired(["gh", "api", path], 45)
        result = reporter.collect(selection, reporter.CaptureApi(timeout, 1))
        self.assertFalse(result["query_complete"])
        self.assertIn("timed out", result["errors"][0]["message"])

    def test_missing_and_duplicate_step_numbers_cannot_silently_duplicate_sources(self):
        selection, api = fixture(jobs={(10, 1): [job(steps=[step(), step(), step(None, name="Unknown step")])]})
        result = reporter.collect(selection, api)
        self.assertFalse(result["query_complete"])
        self.assertEqual(sum(item["count"] for item in result["step_groups"]), 2)
        self.assertTrue(result["data_gaps"])

    def test_known_step_numbers_are_deduplicated_even_with_other_identity_fields_missing(self):
        selection, api = fixture(jobs={(10, 1): [job(steps=[
            step(1, name=None), step(1, name="Build"), step(2, status=None), step(2, name="Check")])]})
        result = reporter.collect(selection, api)
        self.assertFalse(result["query_complete"])
        self.assertEqual(sum(item["count"] for item in result["step_groups"]), 2)
        sources = [source["step_number"] for group in result["step_groups"] for source in group["sources"]]
        self.assertEqual(sorted(sources), [1, 2])
        self.assertEqual(sum("duplicate step number" in item["message"] for item in result["errors"]), 2)

    def test_markdown_names_are_literal_and_scope_and_ids_are_visible(self):
        selection, api = fixture(jobs={(10, 1): [job(steps=[step(name="<script>|`name`\nnext")])]})
        report = reporter.collect(selection, api)
        text = reporter.markdown(report)
        self.assertIn("&lt;script&gt;\\|\\`name\\`\\nnext", text)
        self.assertIn("run 10, attempt 1", text)
        self.assertIn(start, text)
        self.assertIn("Caps:", text)

    def test_scope_rejects_ambiguous_dates_and_normalizes_offsets(self):
        for changes in ({"since": "2026-11-01"}, {"until": start}, {"repository": "owner/repo?bad"}, {"page_size": 101}):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                scope(**changes)
        selection = scope(since="2026-11-01T10:00:00+10:00")
        self.assertEqual(selection.since, start)

    def test_live_capture_replays_offline_without_any_transport_fallback(self):
        selection, api = fixture()
        with tempfile.TemporaryDirectory(prefix="labrador-ci-report-") as folder:
            root = Path(folder)
            capture = root / "responses.json"
            live = root / "live.json"
            offline = root / "offline.json"
            arguments = ["--repository", selection.repository, "--since", start, "--until", end,
                         "--attempts", "latest", "--capture", str(capture), "--format", "json", "--output", str(live)]
            self.assertEqual(reporter.main(arguments, get=api), 0)
            def forbidden(path):
                self.fail("offline replay attempted a live request: " + path)
            self.assertEqual(reporter.main(["--offline", str(capture), "--format", "json", "--output", str(offline)], get=forbidden), 0)
            first = json.loads(live.read_text())
            second = json.loads(offline.read_text())
            self.assertEqual(first["step_groups"], second["step_groups"])
            self.assertEqual(first["request_paths"], second["request_paths"])
            captured = json.loads(capture.read_text())
            captured["responses"].pop(attempt_path())
            capture.write_text(json.dumps(captured))
            self.assertEqual(reporter.main(["--offline", str(capture), "--format", "json", "--output", str(offline)], get=forbidden), 1)
            self.assertIn("no live fallback", json.loads(offline.read_text())["errors"][0]["message"])

    def test_transport_uses_fixed_host_get_and_never_a_shell_or_write_verb(self):
        process = SimpleNamespace(returncode=0, stdout='{"total_count":0,"workflow_runs":[]}', stderr="")
        with patch.object(reporter.subprocess, "run", return_value=process) as invoke:
            reporter.gh_get(base)
        arguments = invoke.call_args.args[0]
        self.assertEqual(arguments[:7], ["gh", "api", "--hostname", "github.com", "--method", "GET", "-H"])
        self.assertNotIn("shell", invoke.call_args.kwargs)
        self.assertEqual(arguments[-1], base)


if __name__ == "__main__":
    unittest.main()
