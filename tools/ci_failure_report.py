"""Report observed CI step failures in an explicit run-creation cohort, using GET requests only.

Live reads require a repository, UTC window and latest/all attempt choice.
--offline replays a captured response set without GitHub CLI or network access.
Reports keep cancelled/incomplete observations and missing data separate; they
do not establish flakiness, expected matrix coverage or a failure rate.
"""

from __future__ import annotations

import argparse
from collections import defaultdict
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from html import escape
import json
from pathlib import Path
import re
import subprocess
import sys
from typing import Any, Callable
from urllib.parse import urlencode

api_version = "2026-03-10"
capture_schema = 1
failed = frozenset({"failure", "timed_out", "startup_failure"})
passing = frozenset({"success", "skipped", "neutral"})
repository_name = re.compile(r"[A-Za-z0-9][A-Za-z0-9_.-]*/[A-Za-z0-9][A-Za-z0-9_.-]*\Z")
run_fields = ("id", "name", "workflow_id", "path", "head_branch", "head_sha", "event",
              "status", "conclusion", "run_attempt", "created_at", "updated_at", "run_started_at", "html_url")
job_fields = ("id", "name", "run_id", "run_attempt", "status", "conclusion", "started_at", "completed_at", "html_url")
step_fields = ("number", "name", "status", "conclusion", "started_at", "completed_at")


class QueryError(RuntimeError):
    """One unavailable API response, retained in a partial report."""


def timestamp(value: str) -> datetime:
    if not isinstance(value, str):
        raise ValueError("timestamp must be a string with a timezone")
    result = datetime.fromisoformat(value.replace("Z", "+00:00"))
    if result.tzinfo is None:
        raise ValueError("timestamps need a timezone, for example 2026-11-16T00:00:00Z")
    return result.astimezone(timezone.utc)


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


@dataclass(frozen=True)
class Scope:
    repository: str
    since: str
    until: str
    attempts: str
    workflow_path: str | None = None
    branch: str | None = None
    page_size: int = 100
    max_run_pages: int = 5
    max_job_pages: int = 5
    max_runs: int = 50
    max_attempts: int = 5
    max_requests: int = 100

    def __post_init__(self):
        if not repository_name.fullmatch(self.repository):
            raise ValueError("repository must be an explicit owner/name on github.com")
        start = timestamp(self.since)
        end = timestamp(self.until)
        if start >= end:
            raise ValueError("since must be before until")
        object.__setattr__(self, "since", start.isoformat().replace("+00:00", "Z"))
        object.__setattr__(self, "until", end.isoformat().replace("+00:00", "Z"))
        if self.attempts not in {"latest", "all"}:
            raise ValueError("attempts must be latest or all")
        for name in ("workflow_path", "branch"):
            value = getattr(self, name)
            if value is not None and (not isinstance(value, str) or not value.strip()):
                raise ValueError(f"{name} must be nonempty when supplied")
        for name in ("page_size", "max_run_pages", "max_job_pages", "max_runs", "max_attempts", "max_requests"):
            if type(getattr(self, name)) is not int or getattr(self, name) < 1:
                raise ValueError(f"{name} must be a positive integer")
        if self.page_size > 100:
            raise ValueError("page_size cannot exceed GitHub's 100-entry maximum")


class CaptureApi:
    def __init__(self, get: Callable[[str], Any], maximum: int):
        self.get = get
        self.maximum = maximum
        self.executed = 0
        self.responses: dict[str, dict] = {}

    def __call__(self, path: str) -> Any:
        if path not in self.responses:
            if self.executed >= self.maximum:
                self.responses[path] = {"error": "request budget reached; GET not executed", "executed": False}
            else:
                self.executed += 1
                try:
                    self.responses[path] = {"data": self.get(path), "executed": True}
                except (QueryError, OSError, ValueError, subprocess.TimeoutExpired) as error:
                    self.responses[path] = {"error": str(error), "executed": True}
        result = self.responses[path]
        if "error" in result:
            raise QueryError(result["error"])
        return result["data"]


def gh_get(path: str) -> Any:
    """The only transport: fixed-host, explicit GET, no shell or credentials in argv."""
    run = subprocess.run(
        ["gh", "api", "--hostname", "github.com", "--method", "GET", "-H",
         "Accept: application/vnd.github+json", "-H", f"X-GitHub-Api-Version: {api_version}", path],
        stdin=subprocess.DEVNULL, capture_output=True, text=True, timeout=45,
    )
    if run.returncode:
        raise QueryError(f"gh api exited {run.returncode}: {run.stderr.strip() or run.stdout.strip()}")
    try:
        return json.loads(run.stdout)
    except ValueError as error:
        raise QueryError(f"gh api returned invalid JSON: {error}") from error


def positive_id(value: Any) -> bool:
    return type(value) is int and value > 0


def selected_fields(value: dict, names: tuple[str, ...]) -> dict:
    return {name: value.get(name) for name in names}


def collect(scope: Scope, get: Callable[[str], Any]) -> dict:
    errors: list[dict] = []
    gaps: list[dict] = []
    paths: list[str] = []

    def fields(value: dict, names: tuple[str, ...], source: dict) -> dict:
        result = selected_fields(value, names)
        for name, item in result.items():
            numeric = name in {"id", "run_id", "run_attempt", "workflow_id", "number"}
            if item is not None and not (positive_id(item) if numeric else isinstance(item, str)):
                result[name] = None
                gaps.append({**source, "message": f"invalid {name} field"})
        return result

    def query(path: str) -> dict | None:
        paths.append(path)
        try:
            answer = get(path)
            if not isinstance(answer, dict):
                raise QueryError("response is not a JSON object")
            return answer
        except (QueryError, OSError, ValueError) as error:
            errors.append({"path": path, "message": str(error)})
            return None

    def pages(base: str, field: str, maximum: int) -> list[dict]:
        items: list[dict] = []
        expected = None
        for page in range(1, maximum + 1):
            separator = "&" if "?" in base else "?"
            path = f"{base}{separator}per_page={scope.page_size}&page={page}"
            body = query(path)
            if body is None:
                break
            listed = body.get(field)
            total = body.get("total_count")
            if not isinstance(listed, list) or type(total) is not int or total < 0:
                errors.append({"path": path, "message": f"missing/invalid {field} or total_count"})
                break
            if expected is not None and expected != total:
                errors.append({"path": path, "message": f"total_count changed during pagination: {expected} to {total}"})
            expected = total
            if not all(isinstance(item, dict) for item in listed):
                errors.append({"path": path, "message": f"{field} contains a non-object entry"})
                break
            items.extend(listed)
            if len(listed) > scope.page_size or len(items) > total:
                errors.append({"path": path, "message": "page contains more entries than advertised"})
            if field == "workflow_runs" and total > 1000 and page == 1:
                errors.append({"path": path, "message": "GitHub's filtered-run search caps results at 1000"})
            if len(items) >= total:
                break
            if len(listed) < scope.page_size:
                errors.append({"path": path, "message": f"short page leaves {len(items)} of {total} advertised entries"})
                break
            if page == maximum or (field == "workflow_runs" and len(items) >= 1000):
                errors.append({"path": path, "message": f"pagination limit reached after {len(items)} of {total} advertised entries"})
                break
        return items

    base = f"/repos/{scope.repository}/actions/runs"
    parameters = {"created": f"{scope.since}..{scope.until}"}
    if scope.branch is not None:
        parameters["branch"] = scope.branch
    observed = pages(base + "?" + urlencode(parameters), "workflow_runs", scope.max_run_pages)
    runs: list[dict] = []
    seen_runs: set[int] = set()
    matching = []
    for run in observed:
        identifier = run.get("id")
        if not positive_id(identifier):
            errors.append({"path": base, "message": "run with missing/invalid id omitted"})
            continue
        if identifier in seen_runs:
            errors.append({"path": base, "message": f"duplicate run id {identifier}; counted once"})
            continue
        seen_runs.add(identifier)
        try:
            created = timestamp(run["created_at"])
        except (KeyError, TypeError, ValueError):
            errors.append({"path": base, "run_id": identifier, "message": "run creation time missing/invalid; omitted"})
            continue
        if not (timestamp(scope.since) <= created < timestamp(scope.until)):
            continue
        if scope.branch is not None and run.get("head_branch") != scope.branch:
            if run.get("head_branch") is None:
                gaps.append({"run_id": identifier, "message": "head_branch missing; branch filter could not be evaluated"})
            continue
        workflow = run.get("path")
        if scope.workflow_path is not None and (not isinstance(workflow, str) or workflow.split("@", 1)[0] != scope.workflow_path):
            if not isinstance(workflow, str):
                gaps.append({"run_id": identifier, "message": "workflow path missing/invalid; workflow filter could not be evaluated"})
            continue
        matching.append(run)
    if len(matching) > scope.max_runs:
        errors.append({"path": base, "message": f"run limit selects {scope.max_runs} of {len(matching)} observed matches in API order"})

    attempts: list[dict] = []
    for run in matching[:scope.max_runs]:
        identifier = run["id"]
        latest = run.get("run_attempt")
        source_run = fields(run, run_fields, {"run_id": identifier})
        source_run["selected_attempts"] = []
        runs.append(source_run)
        if not positive_id(latest):
            gaps.append({"run_id": identifier, "message": "latest run_attempt missing/invalid; no attempt guessed"})
            continue
        first = latest if scope.attempts == "latest" else max(1, latest - scope.max_attempts + 1)
        source_run["prior_attempts_not_selected"] = first - 1
        if scope.attempts == "all" and first > 1:
            errors.append({"run_id": identifier, "message": f"attempt limit retains {first}..{latest}; {first - 1} earlier attempts omitted"})
        for number in range(first, latest + 1):
            source_run["selected_attempts"].append(number)
            path = f"{base}/{identifier}/attempts/{number}"
            metadata = query(path)
            if metadata is None:
                continue
            if not positive_id(metadata.get("id")) or not positive_id(metadata.get("run_attempt")) or metadata["id"] != identifier or metadata["run_attempt"] != number:
                errors.append({"path": path, "message": "attempt identity mismatch; jobs not attributed"})
                continue
            source_attempt = {"run_id": identifier, "attempt": number}
            attempt = fields(metadata, run_fields, source_attempt)
            head_repository = metadata.get("head_repository")
            attempt["head_repository"] = head_repository.get("full_name") if isinstance(head_repository, dict) else None
            if attempt["head_repository"] is not None and not isinstance(attempt["head_repository"], str):
                attempt["head_repository"] = None
                gaps.append({**source_attempt, "message": "invalid head_repository name"})
            attempt["latest_at_listing"] = number == latest
            attempt["jobs"] = []
            attempts.append(attempt)
            for field in ("workflow_id", "path", "name", "head_branch", "head_sha", "event", "status"):
                if attempt[field] is None:
                    gaps.append({"run_id": identifier, "attempt": number, "message": f"missing attempt {field}"})
            if attempt["status"] == "completed" and attempt["conclusion"] is None:
                gaps.append({**source_attempt, "message": "completed attempt conclusion missing"})
            jobs = pages(path + "/jobs", "jobs", scope.max_job_pages)
            if not jobs:
                gaps.append({"run_id": identifier, "attempt": number, "message": "no jobs returned; not evidence of zero failed steps"})
            seen_jobs: set[int] = set()
            for raw in jobs:
                job_id = raw.get("id")
                if not positive_id(job_id) or job_id in seen_jobs:
                    errors.append({"path": path + "/jobs", "message": f"missing/duplicate job id {job_id!r}; omitted"})
                    continue
                seen_jobs.add(job_id)
                job_run = raw.get("run_id", identifier)
                job_attempt = raw.get("run_attempt", number)
                if not positive_id(job_run) or not positive_id(job_attempt) or job_run != identifier or job_attempt != number:
                    gaps.append({"run_id": identifier, "attempt": number, "job_id": job_id,
                                 "message": "job belongs to another run/attempt; not counted here"})
                    continue
                source_job = {"run_id": identifier, "attempt": number, "job_id": job_id}
                job = fields(raw, job_fields, source_job)
                job["steps"] = None
                attempt["jobs"].append(job)
                for field in ("name", "status"):
                    if job[field] is None:
                        gaps.append({**source_job, "message": f"missing job {field}"})
                if job["status"] == "completed" and job["conclusion"] is None:
                    gaps.append({**source_job, "message": "completed job conclusion missing"})
                raw_steps = raw.get("steps")
                if not isinstance(raw_steps, list) or not all(isinstance(step, dict) for step in raw_steps):
                    gaps.append({"run_id": identifier, "attempt": number, "job_id": job_id, "message": "steps missing/invalid"})
                    continue
                job["steps"] = []
                seen_steps: set[int] = set()
                for index, raw_step in enumerate(raw_steps):
                    step = fields(raw_step, step_fields, {**source_job, "step_index": index})
                    step["source_index"] = index
                    if step["number"] is not None and step["number"] in seen_steps:
                        errors.append({**source_job, "message": f"duplicate step number {step['number']}; counted once"})
                        continue
                    if step["number"] is not None:
                        seen_steps.add(step["number"])
                    if step["number"] is None or step["name"] is None or step["status"] is None:
                        gaps.append({**source_job, "step_index": index, "message": "step identity/status incomplete"})
                    if step["status"] == "completed" and step["conclusion"] is None:
                        gaps.append({**source_job, "step_index": index, "message": "completed step conclusion missing"})
                    job["steps"].append(step)
                if job["conclusion"] in failed and not any(step["conclusion"] in failed for step in job["steps"]):
                    gaps.append({"run_id": identifier, "attempt": number, "job_id": job_id,
                                 "message": "failed job has no reported failed step; no step invented"})

    groups: dict[tuple, list[dict]] = defaultdict(list)
    outcomes: dict[tuple, list[dict]] = defaultdict(list)
    for attempt in attempts:
        identity = (attempt["workflow_id"], attempt["path"], attempt["name"], attempt["head_repository"],
                    attempt["head_branch"], attempt["event"], attempt["run_attempt"])
        for job in attempt["jobs"]:
            source = {"run_id": attempt["id"], "attempt": attempt["run_attempt"], "job_id": job["id"],
                      "run_url": attempt["html_url"], "job_url": job["html_url"]}
            outcomes[identity + (job["name"], job["status"], job["conclusion"])].append(source)
            for step in job["steps"] or []:
                context = "completed"
                if (attempt["status"] != "completed" or job["status"] != "completed" or step["status"] != "completed"
                        or attempt["conclusion"] is None or job["conclusion"] is None or step["conclusion"] is None):
                    context = "incomplete"
                elif "cancelled" in (attempt["conclusion"], job["conclusion"], step["conclusion"]):
                    context = "cancelled"
                if step["conclusion"] in passing:
                    # Job outcomes retain cancelled/incomplete contexts without
                    # treating their already successful steps as problem steps.
                    continue
                kind = "failure" if context == "completed" and step["conclusion"] in failed else context
                if kind == "completed":
                    kind = "other"
                key = identity + (kind, job["name"], job["conclusion"], step["name"], step["number"], step["conclusion"])
                groups[key].append({**source, "step_number": step["number"], "step_index": step["source_index"]})

    labels = ("workflow_id", "workflow_path", "workflow_name", "head_repository", "head_branch", "event", "attempt")
    def grouped(values: dict, extra: tuple[str, ...]) -> list[dict]:
        return [{**dict(zip(labels + extra, key)), "count": len(sources), "sources": sources}
                for key, sources in sorted(values.items(), key=lambda item: json.dumps(item[0]))]

    return {
        "schema_version": 1, "repository": scope.repository, "scope": asdict(scope),
        "query_complete": not errors, "errors": errors, "data_gaps": gaps,
        "request_paths": paths, "observed_run_count": len(observed), "selected_runs": runs, "attempts": attempts,
        "step_groups": grouped(groups, ("context", "job_name", "job_conclusion", "step_name", "step_number", "step_conclusion")),
        "job_outcomes": grouped(outcomes, ("job_name", "job_status", "job_conclusion")),
        "limits": [
            "The half-open window selects original run created_at, not retry execution dates.",
            "Latest means the attempt number observed when runs were listed; later retries are not polled.",
            "Partial reruns contain returned job observations, not an expected whole-matrix denominator.",
            "head_branch is GitHub's value, not a reconstructed full ref; source repository, event and SHA remain available.",
            "Sequential API reads are not an atomic snapshot. Deleted or expired data may be unavailable.",
            "Counts describe observations only; no failure rate, cause or flakiness is inferred.",
        ],
    }


def markdown(report: dict) -> str:
    def cell(value: Any) -> str:
        if value is None:
            return "(missing)"
        return escape(str(value), quote=False).replace("\\", "\\\\").replace("|", "\\|").replace("`", "\\`").replace("\r", "").replace("\n", "\\n")

    scope = report["scope"]
    source = report.get("source", {})
    lines = [f"CI observations: {scope['repository']}", "",
             f"Original run creation: [{scope['since']}, {scope['until']}); attempts: {scope['attempts']}.",
             f"Workflow filter: {scope['workflow_path'] or '(all)'}; head_branch filter: {scope['branch'] or '(all)' }.",
             f"Capture: {source.get('started_at', '(not supplied)')} to {source.get('finished_at', '(not supplied)')}; "
             f"mode: {source.get('mode', '(not supplied)')}; API: {source.get('api_version', '(not supplied)')}.",
             f"Caps: {scope['max_runs']} runs, {scope['page_size']} entries/page, {scope['max_run_pages']} run pages, "
             f"{scope['max_job_pages']} job pages/attempt, {scope['max_attempts']} attempts/run, {scope['max_requests']} GET requests.",
             f"Query complete: {report['query_complete']}; data gaps: {len(report['data_gaps'])}.", "",
             "Selected original runs", "",
             "| run | workflow | head_branch | event | latest at listing | selected attempts | earlier attempts excluded | created |",
             "|---:|---|---|---|---:|---|---:|---|"]
    for run in report["selected_runs"]:
        values = (run["id"], run["path"], run["head_branch"], run["event"], run["run_attempt"],
                  run["selected_attempts"], run.get("prior_attempts_not_selected"), run["created_at"])
        lines.append("| " + " | ".join(cell(value) for value in values) + " |")
    lines += ["",
             "Observed non-passing steps", "",
             "| workflow | head repository | head_branch | event | attempt | context | job | step | conclusion | observations |",
             "|---|---|---|---|---:|---|---|---|---|---:|"]
    for group in report["step_groups"]:
        lines.append("| " + " | ".join(cell(group[key]) for key in
                     ("workflow_path", "head_repository", "head_branch", "event", "attempt", "context", "job_name", "step_name", "step_conclusion", "count")) + " |")
    lines += ["", "Job outcomes (including cancellation, incomplete execution and jobs without steps)", "",
              "| workflow | head repository | head_branch | event | attempt | job | status | conclusion | observations |", "|---|---|---|---|---:|---|---|---|---:|"]
    for group in report["job_outcomes"]:
        lines.append("| " + " | ".join(cell(group[key]) for key in
                     ("workflow_path", "head_repository", "head_branch", "event", "attempt", "job_name", "job_status", "job_conclusion", "count")) + " |")
    lines += ["", "Source attempts", ""]
    for attempt in report["attempts"]:
        lines.append(f"- run {attempt['id']}, attempt {attempt['run_attempt']}: {cell(attempt['status'])}/"
                     f"{cell(attempt['conclusion'])}, {cell(attempt['head_sha'])}, {cell(attempt['html_url'])}")
    lines += ["", "Coverage and interpretation", ""]
    lines.extend("- " + note for note in report["limits"])
    lines.extend("- " + cell(note) for note in report["errors"] + report["data_gaps"])
    lines += ["- Exact request paths, job IDs and step sources are retained in JSON output and captured responses."]
    return "\n".join(lines) + "\n"


def main(argv: list[str] | None = None, get: Callable[[str], Any] = gh_get) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--offline", type=Path, help="captured responses; makes no network request")
    parser.add_argument("--repository")
    parser.add_argument("--since", help="inclusive original-run creation timestamp, with timezone")
    parser.add_argument("--until", help="exclusive original-run creation timestamp, with timezone")
    parser.add_argument("--attempts", choices=("latest", "all"))
    parser.add_argument("--workflow-path", help="optional exact workflow file path")
    parser.add_argument("--branch", help="optional exact GitHub head_branch value")
    for name in ("page-size", "max-run-pages", "max-job-pages", "max-runs", "max-attempts", "max-requests"):
        parser.add_argument("--" + name, type=int)
    parser.add_argument("--capture", type=Path, help="save GET responses locally for offline replay")
    parser.add_argument("--format", choices=("markdown", "json"), default="markdown")
    parser.add_argument("--output", type=Path, help="write the report here instead of stdout")
    args = parser.parse_args(argv)
    scope_names = tuple(Scope.__dataclass_fields__)
    try:
        if args.offline is not None:
            if args.capture is not None or any(getattr(args, name) is not None for name in scope_names):
                parser.error("--offline uses the captured scope; do not supply live-query flags")
            capture = json.loads(args.offline.read_text(encoding="utf-8"))
            if not isinstance(capture, dict) or capture.get("schema_version") != capture_schema or not isinstance(capture.get("responses"), dict):
                raise ValueError("unsupported/malformed response capture")
            scope = Scope(**capture["scope"])
            responses = capture["responses"]
            def replay(path: str) -> Any:
                entry = responses.get(path)
                if not isinstance(entry, dict):
                    raise QueryError("response absent from offline capture; no live fallback")
                if "error" in entry:
                    raise QueryError(str(entry["error"]))
                if "data" not in entry:
                    raise QueryError("captured response has neither data nor error")
                return entry["data"]
            report = collect(scope, replay)
            report["source"] = {"mode": "offline", "started_at": capture.get("started_at"), "finished_at": capture.get("finished_at"),
                                "api_version": capture.get("api_version")}
        else:
            values = {name: getattr(args, name) for name in scope_names if getattr(args, name) is not None}
            if not all(name in values for name in ("repository", "since", "until", "attempts")):
                parser.error("live reads require --repository, --since, --until and --attempts")
            scope = Scope(**values)
            api = CaptureApi(get, scope.max_requests)
            started = utc_now()
            report = collect(scope, api)
            finished = utc_now()
            report["source"] = {"mode": "live", "started_at": started, "finished_at": finished, "api_version": api_version}
            if args.capture is not None:
                capture = {"schema_version": capture_schema, "api_version": api_version,
                           "started_at": started, "finished_at": finished, "scope": asdict(scope), "responses": api.responses}
                args.capture.parent.mkdir(parents=True, exist_ok=True)
                args.capture.write_text(json.dumps(capture, indent=2) + "\n", encoding="utf-8")
        output = json.dumps(report, indent=2) + "\n" if args.format == "json" else markdown(report)
        if args.output is None:
            print(output, end="")
        else:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(output, encoding="utf-8")
        return 0 if report["query_complete"] else 1
    except (OSError, ValueError, KeyError, TypeError, subprocess.TimeoutExpired) as error:
        print(f"ci_failure_report: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
