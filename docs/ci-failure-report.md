# A manual CI observation report

`tools/ci_failure_report.py` reads GitHub Actions metadata and groups observed
non-passing steps by workflow, source repository, `head_branch`, event, attempt,
job and step. It is a maintainer command, not a workflow or a polling service.
It uses GitHub CLI's existing authentication and explicitly issues `GET`
requests to github.com. It never reruns or cancels jobs, changes settings,
or writes issues, comments or notifications. Optional output and capture
files are local.

## Choose the cohort and attempt scope

Run from the repository root, with Python and an already configured `gh` on
PATH. All four scope flags are required for a live invocation:

```bat
python tools\ci_failure_report.py --repository matt439/Labrador --since 2026-11-01T00:00:00Z --until 2026-11-08T00:00:00Z --attempts all --workflow-path .github/workflows/ci.yml --capture out\ci-report\responses.json --output out\ci-report\report.md
```

The timestamps select a half-open interval of the **original workflow run's
`created_at`**: `since` is included and `until` is excluded. A date without a
timezone is refused. Offsets are accepted and normalized to UTC. GitHub's
inclusive creation query is checked against that half-open interval locally.
A retry of one of those runs may have executed after `until`; it remains in
the selected run's attempt history. Conversely, a retry inside the window
of a run originally created before `since` is outside this cohort.

`--attempts latest` queries only the attempt number observed when the runs
were listed and records how many earlier attempts it excludes. It does not
poll for a newer retry. `--attempts all` queries the selected run's attempts
separately, up to the stated cap, and names any omitted earlier attempts.
Every attempt uses the
[attempt-specific metadata](https://docs.github.com/en/rest/actions/workflow-runs#get-a-workflow-run-attempt)
and [job endpoints](https://docs.github.com/en/rest/actions/workflow-jobs#list-jobs-for-a-workflow-run-attempt).
The default run-jobs view, which can hide prior executions, is not used.

`--workflow-path` is an optional exact workflow filename, such as
`.github/workflows/ci.yml`. A `@ref` suffix reported by GitHub does not prevent
that file filter matching. `--branch` is an optional exact `head_branch`
filter. `head_branch` is the value GitHub supplied, which can be missing;
it is not proof of a full `refs/heads/...` or `refs/tags/...` spelling.
Workflow ID/path/name, source repository, event, head SHA, run IDs and
attempt numbers remain in JSON output so similarly named refs are separable.

## Read the result

The Markdown report includes selected original runs and their attempt
choices, grouped non-passing steps, job outcomes, source attempts and coverage
notes. `--format json` retains normalized run/job/step observations, exact
request paths and each group's source run ID, attempt, job ID and step
number/index. Missing names or IDs are marked as missing; no label is guessed.

- `failure` groups contain failed/timed-out/startup-failed step observations
  whose attempt, job and step have completed with available conclusions.
  A failed step in a successful `continue-on-error` job remains observable;
  the successful job and attempt conclusions are retained.
- `cancelled` and `incomplete` contexts stay separate. A failed step observed
  before a run was cancelled retains its raw failure conclusion in that
  cancelled context. Unfinished work is not a completed failure.
- Job outcomes retain cancellations, queued/running jobs, successful jobs
  and jobs with unavailable step lists. A failed job with no reported failed
  step produces a data gap, not an invented step failure.

Groups count returned **step observations**, not failing workflows or
distinct commits. Repeated failures in different attempts remain different
observations. A partial rerun can execute only selected jobs: GitHub's
[rerun behaviour](https://docs.github.com/en/actions/how-tos/manage-workflow-runs/re-run-workflows-and-jobs)
does not provide an expected whole-matrix denominator for each attempt.
The report therefore computes no failure rate and does not call anything
flaky or identify a cause. A work-in-progress ref and a master failure remain
different observations.

`query_complete` flags request, page, run/attempt identity, duplicate-source
and cap errors. It does **not** mean every job has finished or that every
step or attribution field is available: inspect `data_gaps`, raw statuses
and the cancelled/incomplete contexts too. Reads are sequential, not an
atomic snapshot. Duplicated run/job/known step identifiers are counted once
and flag partial coverage; changed page totals are also exposed. A present
job attempt that disagrees with the requested attempt is omitted with a gap.

## Limits and unavailable data

All limits are recorded in every report and can be set on the command line:

| flag | default | meaning |
|---|---:|---|
| `--page-size` | 100 | entries per runs/jobs page; GitHub permits at most 100 |
| `--max-run-pages` | 5 | pages of original-run listings |
| `--max-job-pages` | 5 | job pages for each selected attempt |
| `--max-runs` | 50 | matching runs selected in the API's returned order |
| `--max-attempts` | 5 | under `all`, retain this many most recent attempt numbers per run |
| `--max-requests` | 100 | logical `gh api` GET invocations; each has a 45-second timeout |

A cap produces an explicit partial-coverage note and exit `1`; it is not
silently described as a complete window. GitHub's
[filtered workflow-run search](https://docs.github.com/en/rest/actions/workflow-runs#list-workflow-runs-for-a-repository)
also caps a search at 1,000 results. Narrow the creation window or use another
explicit cohort when a limit is reached. A smaller page size does not expand
the result window. Filters are recorded, and unavailable attribution needed
to evaluate one is retained as a data gap rather than assumed to match.

Authentication, API denials, missing/expired prior attempts, incomplete
pages, changed totals and missing captured responses are printed with their
request scope. The report keeps any observations already collected. Exit
`0` means the query completed, exit `1` means partial query coverage, and exit
`2` is invalid invocation/capture or a local file error. None is a verdict on
whether the observed CI passed.

## Keep and replay responses offline

`--capture` saves a JSON response set with its scope, start/end UTC timestamps
and API version. It records API bodies and errors, not authentication headers
or credentials supplied to `gh`. API bodies can include repository metadata
beyond the normalized report; the command does not publish either file.

```bat
python tools\ci_failure_report.py --offline out\ci-report\responses.json --format json --output out\ci-report\replayed.json
```

Offline mode uses the captured scope and refuses live-query flags. A missing
response is a recorded coverage error; there is no live fallback and `gh` is
not needed. Capture schema version `1` is an object containing `scope`,
`started_at`, `finished_at`, `api_version` and `responses`, whose keys are the
exact GET paths and whose entries hold `data` or `error`. This also permits
small captured-response fixtures. Unexpected shapes and unavailable fields
remain errors/gaps instead of zero failures.

## Qualification, 17 November 2026

Offline fixtures cover successful retries after failed attempts, latest/all
scope, partial attempt history, cancellations, unfinished jobs, missing steps,
continued-on-error failures, identity mismatches, duplicate identifiers,
pagination changes/caps, request limits, auth/timeout errors and pure offline
replay. These are observation/reporting tests; they run no CI job.

One bounded public read of `matt439/Labrador` selected the original creation
window `[2026-11-15T08:24:00Z, 2026-11-15T08:25:00Z)`, workflow
`.github/workflows/ci.yml`, `all` attempts, and caps of two runs, one runs
page, one jobs page, two attempts and eight logical requests. The report
used three GETs, from `2026-11-16T18:33:58Z` to `18:34:01Z`, selected
[run 43694319140, attempt 1](https://github.com/matt439/Labrador/actions/runs/43694319140/attempts/1),
and reported complete query coverage with no data gaps. It observed one
failed step on `work/firefox-readback`: job `132751418432` (`wasm`), step
`13`, `Run all Firefox pages on software WebGL`. An independent public jobs
response agreed on that job ID, step number, name and conclusion. Offline
replay reproduced the live JSON apart from the source-mode label.

This qualifies that bounded read and replay, not all CI history or the cause
of that failure. No run, issue, notification, schedule or setting was changed.
