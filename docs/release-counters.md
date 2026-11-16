# Manual aggregate release counters

`tools/release_counters.py` records a small dated snapshot when the owner
explicitly invokes collection. It adds no tracking, analytics service,
scheduled task or CI hook. It does not publish anything. The
[November 12 v0.3.0 record](review/2026-11-10/2026-11-12-github-counters.md)
remains unchanged; its figures describe that dated observation.

The repository, published release tag and requested traffic window are
explicit arguments. This example uses `v0.4.0`; use it only after that release
is published, or substitute the published tag being recorded. From the
checkout root, inspect the plan first:

```text
python tools/release_counters.py --repository matt439/Labrador --release v0.4.0 --window latest-14-days
```

The default performs no account reads or file writes. GitHub CLI, already
used by the release procedure, supplies GET-only API access when collection
is requested. The tool uses existing authentication, disables interactive
prompting and requests no login or credential changes; it pins the REST API
version and github.com host. Requests pin `2022-11-28`; future migrations follow
[GitHub's API versioning contract](https://docs.github.com/en/rest/about-the-rest-api/api-versions).
The public snapshot requires a confirmed public repository and published
release. It records only the two exact starter/documentation asset names.

## Collect public asset counters

```text
python tools/release_counters.py --repository matt439/Labrador --release v0.4.0 --window latest-14-days --collect
```

The new snapshot directory is printed under ignored
`out/release-counters/`. `public.json` and `public.md` contain the selected
assets' availability and cumulative counts. JSON also records upload IDs,
created/updated metadata timestamps, release publication time, capture time,
API provenance and a hash of the public observations. Missing assets are
`not_present` with no count; an observed zero remains zero. Existing output
directories are refused instead of overwriting a prior observation. An
explicit `--output-dir <new-directory>` chooses another local destination.

The [release-assets API](https://docs.github.com/en/rest/releases/assets#list-release-assets)
supplies an asset's `download_count` and upload identity without a reporting-window
parameter. These are cumulative observations for that upload at capture,
not downloads during the traffic window. Replacing an asset can reset its
counter, so retain its ID and dates. Pagination uses GitHub CLI's
[`--paginate --slurp`](https://cli.github.com/manual/gh_api) and the collector
does not download asset bytes.

## Separately request owner-private traffic

Repository views and uniques are owner-private API data even when the
repository is public. This step runs only when the owner explicitly chooses
it; implementing the collector or cutting a release does not invoke it:

```text
python tools/release_counters.py --repository matt439/Labrador --release v0.4.0 --window latest-14-days --collect --include-traffic
```

`traffic-private.json` and `traffic-private.md` hold the separate aggregate
result and access limits. The replay input is named `capture-private.json`;
it is private too. Public reports contain no traffic counts, access-denial
details or hashes derived from private inputs. No author/uploader identity,
referrer, popular path, visitor identifier or clone series is retained.
Keep private files in ignored/local storage. Sharing any traffic observation
is a separate owner choice; this tool never inserts it into repository docs.

GitHub's [traffic API](https://docs.github.com/en/rest/metrics/traffic#get-page-views)
provides its latest fourteen-day window, with daily UTC buckets, for
repositories the caller can write to. Fine-grained tokens need read access
to the repository Administration permission. The tool does not elevate
access. A refused or malformed result is `unavailable`, not zero; the public
asset snapshot can still be retained. Requested traffic failures return exit
1, complete requested observations return 0, and invalid inputs, public
request failures or local prerequisites return 2.

The returned bucket dates describe only the buckets received; they do not
prove complete coverage of a requested start/end range. The API's own example
has fifteen UTC dates, so the collector does not impose a fourteen-row limit
or invent missing days. Provider aggregate uniques are retained directly;
adding daily uniques would count repeat visitors again. Traffic covers the
whole repository, with the release tag recorded only as context.

## Replay retained observations without account access

The collector retains sanitized `capture-public.json` or
`capture-private.json` inputs alongside the reports. To replay one, give its
same repository, tag and window and a new output directory. Replace
`C:/path/to/capture-public.json` below with the retained file's actual path:

```text
python tools/release_counters.py --repository matt439/Labrador --release v0.4.0 --window latest-14-days --replay "C:/path/to/capture-public.json" --output-dir out/release-counters/replay-public
```

Add `--include-traffic` only when replaying the private companion intentionally.
Without it, retained private traffic is omitted from all new output. Replay
makes no API calls, preserves the observation times and is labelled as
replay in both reports and retained inputs. It proves reproducible parsing
and formatting, not that an edited input is authentic or that old counts
are current. The hashes identify sanitized observations; they are not signatures.

Views, uniques and downloads are observations, not a number of people using
the engine. Automation, repeat requests and the owner can contribute; the
preserved historical record also explains CI-driven clones and website-driven
documentation downloads. The collector makes no adoption or demand inference.

## Qualification, 2026-11-17

Seventeen offline fixtures cover publication/privacy scope, paginated
assets, empty/missing/zero data, overlapping daily uniques, fifteen bucket
dates, access failure, discarded identities, independent public hashes,
prior-output preservation and deterministic replay. The actual CLI also
replayed a fixture with `gh` absent from PATH. All thirteen local lint stages
passed, including 514 Python tests with one environment-dependent skip;
independent source review found no remaining actionable defect. Synthetic
replay artifacts and logs are retained under `out/counter-validation/` in
the qualification checkout. No live public or private
account collection was invoked for implementation; account-read qualification
occurs only with an explicit later owner invocation. The combined website
gate remains part of integration.
