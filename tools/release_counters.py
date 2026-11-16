"""Plan an owner-invoked aggregate release snapshot; collect only with --collect.

Public release-asset counts and explicitly requested owner-private traffic are
separate reports under ignored out/. --replay formats a sanitized capture
without account access. No workflow, publication or tracking hook is installed.
"""

from __future__ import annotations

import argparse
from collections.abc import Callable
from datetime import datetime, timedelta, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
from urllib.parse import quote

REPOSITORY = Path(__file__).resolve().parents[1]
API_VERSION = "2022-11-28"
WINDOW = "latest-14-days"
PUBLIC_CAPTURE_FIELDS = ("schema_version", "api_version", "repository", "repository_public",
                         "release", "public_captured_at", "assets")
PUBLIC_LIMITS = (
    "Downloads are cumulative for the identified asset at capture, not downloads within the traffic window.",
    "Downloads may include automation, CI, repeat requests and the owner; they do not count adopters.",
    "Asset IDs and metadata timestamps identify the upload; replacing an asset can reset its count.",
    "Documentation downloads are separate from starter downloads and may come from website builds.",
)
TRAFFIC_LIMITS = (
    "GitHub supplies its latest 14-day window; returned UTC bucket dates do not prove complete day coverage.",
    "Traffic covers the repository, not just this release or its downloaders.",
    "The provider's aggregate uniques are retained; daily uniques are never summed.",
    "Views and uniques are provider counters, not verified people or adopters. Unavailable access is not zero.",
)


class SnapshotError(ValueError):
    """The requested capture cannot be interpreted or written as specified."""


class FetchError(RuntimeError):
    def __init__(self, reason: str, http_status: int | None = None):
        self.reason = reason
        self.http_status = http_status
        super().__init__(reason + (f" (HTTP {http_status})" if http_status else ""))


def timestamp(value, where: str) -> str:
    try:
        parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
        if parsed.tzinfo is None or parsed.utcoffset() != timedelta(0):
            raise ValueError("not UTC")
    except (AttributeError, TypeError, ValueError) as error:
        raise SnapshotError(f"{where} must be a UTC timestamp") from error
    return parsed.isoformat().replace("+00:00", "Z")


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds").replace("+00:00", "Z")


def count(value, where: str, *, positive: bool = False) -> int:
    if type(value) is not int or value < (1 if positive else 0):
        raise SnapshotError(f"{where} must be a {'positive' if positive else 'nonnegative'} integer")
    return value


def repository_name(value: str) -> str:
    if not isinstance(value, str) or not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]*/[A-Za-z0-9][A-Za-z0-9_.-]*", value):
        raise SnapshotError("repository must be an explicit owner/name on github.com")
    return value


def release_tag(value: str) -> str:
    if not isinstance(value, str) or not re.fullmatch(r"v(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)", value):
        raise SnapshotError("release must be an explicit Labrador tag of the form vX.Y.Z")
    return value


def gh_api(endpoint: str, *, paginate: bool = False, runner=None):
    command = ["gh", "api", "--hostname", "github.com", "--method", "GET",
               "--header", "Accept: application/vnd.github+json",
               "--header", f"X-GitHub-Api-Version: {API_VERSION}"]
    if paginate:
        command += ["--paginate", "--slurp"]
    command.append(endpoint)
    environment = dict(os.environ, GH_PROMPT_DISABLED="1")
    environment.pop("GH_DEBUG", None)
    try:
        result = (runner or subprocess.run)(command, capture_output=True, text=True,
                    encoding="utf-8", env=environment, timeout=30, check=False)
    except subprocess.TimeoutExpired as error:
        raise FetchError("request_timeout") from error
    except OSError as error:
        raise FetchError("gh_unavailable") from error
    if result.returncode:
        status = re.search(r"HTTP\s+(\d{3})", result.stderr or "")
        raise FetchError("http_error", int(status[1]) if status else None)
    try:
        return json.loads(result.stdout)
    except (TypeError, ValueError) as error:
        raise FetchError("invalid_json") from error


def asset_names(tag: str) -> dict[str, str]:
    return {"starter": f"labrador-starter-{tag}.zip", "documentation": f"labrador-docs-{tag}.tar.gz"}


def assets_from_pages(pages, tag: str) -> dict:
    wanted = asset_names(tag)
    assets = {kind: {"status": "not_present", "name": name, "download_count": None}
              for kind, name in wanted.items()}
    if not isinstance(pages, list) or not pages or not all(isinstance(page, list) for page in pages):
        raise SnapshotError("release assets must be a complete paginated array")
    for page in pages:
        for value in page:
            if not isinstance(value, dict) or not isinstance(value.get("name"), str):
                raise SnapshotError("release asset needs a name")
            kind = next((kind for kind, name in wanted.items() if value["name"] == name), None)
            if kind is None:
                continue
            if assets[kind]["status"] != "not_present":
                raise SnapshotError(f"duplicate selected {kind} asset")
            if value.get("state") != "uploaded":
                raise SnapshotError(f"selected {kind} asset is not uploaded")
            assets[kind] = {"status": "observed", "name": wanted[kind],
                "id": count(value.get("id"), f"{kind}.id", positive=True),
                "download_count": count(value.get("download_count"), f"{kind}.download_count"),
                "created_at": timestamp(value.get("created_at"), f"{kind}.created_at"),
                "updated_at": timestamp(value.get("updated_at"), f"{kind}.updated_at")}
    return assets


def traffic_totals(view_count, unique_count, bucket_dates) -> dict:
    views = count(view_count, "traffic.count")
    uniques = count(unique_count, "traffic.uniques")
    if uniques > views:
        raise SnapshotError("traffic uniques cannot exceed views")
    dates = []
    if not isinstance(bucket_dates, list):
        raise SnapshotError("traffic needs its returned bucket dates")
    for value in bucket_dates:
        date = timestamp(value, "traffic.bucket.timestamp")
        if not date.endswith("T00:00:00Z"):
            raise SnapshotError("daily traffic bucket must start at UTC midnight")
        dates.append(date)
    if len(dates) != len(set(dates)):
        raise SnapshotError("traffic repeats a daily bucket")
    return {"count": views, "uniques": uniques, "returned_bucket_dates": sorted(dates)}


def traffic_data(value) -> dict:
    if not isinstance(value, dict) or not isinstance(value.get("views"), list):
        raise SnapshotError("traffic needs aggregate totals and returned daily buckets")
    dates = []
    for bucket in value["views"]:
        if not isinstance(bucket, dict):
            raise SnapshotError("traffic bucket must be an object")
        bucket_count = count(bucket.get("count"), "traffic.bucket.count")
        if count(bucket.get("uniques"), "traffic.bucket.uniques") > bucket_count:
            raise SnapshotError("bucket uniques cannot exceed its views")
        dates.append(bucket.get("timestamp"))
    return traffic_totals(value.get("count"), value.get("uniques"), dates)


def collect(repository: str, tag: str, *, include_traffic: bool, api=gh_api,
            now: Callable[[], str] = utc_now) -> dict:
    base = f"repos/{repository}"
    metadata = api(base)
    if not isinstance(metadata, dict) or metadata.get("private") is not False:
        raise SnapshotError("only a confirmed public repository can supply the public snapshot")
    canonical = repository_name(metadata.get("full_name"))
    if canonical.casefold() != repository.casefold():
        raise SnapshotError("repository response does not match the explicitly requested repository")
    release = api(f"{base}/releases/tags/{quote(tag, safe='')}")
    if not isinstance(release, dict) or release.get("draft") is not False or release.get("tag_name") != tag:
        raise SnapshotError("release response must identify the requested published release")
    release_id = count(release.get("id"), "release.id", positive=True)
    published_at = timestamp(release.get("published_at"), "release.published_at")
    assets = assets_from_pages(api(f"{base}/releases/{release_id}/assets?per_page=100", paginate=True), tag)
    capture = {"schema_version": 1, "api_version": API_VERSION, "repository": canonical,
        "repository_public": True, "release": {"id": release_id, "tag": tag, "published_at": published_at},
        "traffic_window": WINDOW, "public_captured_at": timestamp(now(), "public_captured_at"), "assets": assets}
    if include_traffic:
        try:
            data = traffic_data(api(f"{base}/traffic/views?per=day"))
            capture["traffic"] = {"status": "observed", "data": data}
        except FetchError as error:
            capture["traffic"] = {"status": "unavailable", "reason": error.reason, "http_status": error.http_status}
        except SnapshotError:
            capture["traffic"] = {"status": "unavailable", "reason": "invalid_payload", "http_status": 200}
        capture["traffic"]["captured_at"] = timestamp(now(), "traffic.captured_at")
    return capture


def sanitize_capture(raw, repository: str, tag: str, *, include_traffic: bool) -> dict:
    if not isinstance(raw, dict) or type(raw.get("schema_version")) is not int or raw["schema_version"] != 1:
        raise SnapshotError("capture needs schema_version 1")
    if raw.get("traffic_window") != WINDOW or raw.get("repository_public") is not True:
        raise SnapshotError("capture needs the explicit public-repository and latest-14-days scope")
    canonical = repository_name(raw.get("repository"))
    if canonical.casefold() != repository.casefold():
        raise SnapshotError("capture repository differs from the request")
    version = raw.get("api_version")
    if not isinstance(version, str) or not re.fullmatch(r"\d{4}-\d{2}-\d{2}", version):
        raise SnapshotError("capture needs its REST API version")
    try:
        datetime.fromisoformat(version)
    except ValueError as error:
        raise SnapshotError("capture REST API version must be a valid date") from error
    release = raw.get("release")
    if not isinstance(release, dict) or release.get("tag") != tag:
        raise SnapshotError("capture release differs from the request")
    selected = raw.get("assets")
    if not isinstance(selected, dict):
        raise SnapshotError("capture needs selected asset observations")
    pages = [[]]
    for kind, name in asset_names(tag).items():
        asset = selected.get(kind)
        if not isinstance(asset, dict) or asset.get("name") != name:
            raise SnapshotError(f"capture needs the named {kind} asset observation")
        if asset.get("status") == "observed":
            pages[0].append({**asset, "state": "uploaded"})
        elif asset.get("status") != "not_present" or asset.get("download_count") is not None:
            raise SnapshotError(f"capture has invalid {kind} availability")
    capture = {"schema_version": 1, "api_version": version, "repository": canonical,
        "repository_public": True, "release": {"id": count(release.get("id"), "release.id", positive=True),
            "tag": tag, "published_at": timestamp(release.get("published_at"), "release.published_at")},
        "traffic_window": WINDOW, "public_captured_at": timestamp(raw.get("public_captured_at"), "public_captured_at"),
        "assets": assets_from_pages(pages, tag)}
    if include_traffic:
        traffic = raw.get("traffic")
        if not isinstance(traffic, dict):
            raise SnapshotError("capture contains no explicitly requested traffic result")
        result = {"status": traffic.get("status"), "captured_at": timestamp(traffic.get("captured_at"), "traffic.captured_at")}
        if result["status"] == "observed":
            data = traffic.get("data")
            if not isinstance(data, dict) or not isinstance(data.get("returned_bucket_dates"), list):
                raise SnapshotError("capture needs the returned traffic bucket dates")
            result["data"] = traffic_totals(data.get("count"), data.get("uniques"), data["returned_bucket_dates"])
        elif result["status"] == "unavailable":
            reason = traffic.get("reason")
            if reason not in {"http_error", "gh_unavailable", "request_timeout", "invalid_json", "invalid_payload"}:
                raise SnapshotError("capture has an unknown traffic failure category")
            status = traffic.get("http_status")
            if status is not None and (type(status) is not int or not 100 <= status <= 599):
                raise SnapshotError("capture has an invalid HTTP status")
            result.update(reason=reason, http_status=status)
        else:
            raise SnapshotError("capture has an unknown traffic availability")
        capture["traffic"] = result
    return capture


def canonical_json(value) -> str:
    return json.dumps(value, sort_keys=True, indent=2, ensure_ascii=True) + "\n"


def reports(capture: dict, *, replay: bool) -> tuple[dict, dict | None]:
    public = {key: capture[key] for key in PUBLIC_CAPTURE_FIELDS}
    public_hash = hashlib.sha256(canonical_json(public).encode()).hexdigest()
    public["scope"] = "public release assets"
    public["mode"] = "replay" if replay else "collect"
    public["download_window"] = "asset_lifetime_to_capture"
    base = f"repos/{capture['repository']}"
    public["source"] = {"method": "GET", "api_host": "api.github.com", "repository": base,
        "release": f"{base}/releases/tags/{capture['release']['tag']}",
        "assets": f"{base}/releases/{capture['release']['id']}/assets?per_page=100", "all_asset_pages": True}
    public["inputs_sha256"] = public_hash
    public["limits"] = PUBLIC_LIMITS
    private = None
    if "traffic" in capture:
        private_inputs = {"repository": capture["repository"], "api_version": capture["api_version"], **capture["traffic"]}
        private = {"schema_version": 1, "scope": "owner-private repository traffic",
            "mode": public["mode"], "repository": capture["repository"], "release_tag_context": capture["release"]["tag"],
            "api_version": capture["api_version"], "window": WINDOW, **capture["traffic"],
            "source": {"method": "GET", "api_host": "api.github.com", "endpoint": f"{base}/traffic/views?per=day"}}
        private["inputs_sha256"] = hashlib.sha256(canonical_json(private_inputs).encode()).hexdigest()
        private["limits"] = TRAFFIC_LIMITS
    return public, private


def markdown(public: dict, private: dict | None) -> tuple[str, str | None]:
    mode = "Replay of retained observations; no account reads." if public["mode"] == "replay" else "Explicitly collected observations."
    lines = ["# Public release asset counters", "", f"Repository: `{public['repository']}`; release `{public['release']['tag']}`.",
        f"Captured at {public['public_captured_at']} (UTC). {mode}", "",
        "| Asset | Asset ID | Availability | Cumulative downloads |", "|---|---|---|---:|"]
    for asset in public["assets"].values():
        lines.append(f"| `{asset['name']}` | {asset.get('id', 'Unavailable')} | {asset['status']} | {asset['download_count'] if asset['download_count'] is not None else 'Unavailable'} |")
    lines += ["", "Asset created/updated UTC timestamps and API provenance are retained in public.json; these are not a download time series."]
    lines += ["", *PUBLIC_LIMITS, ""]
    traffic_text = None
    if private is not None:
        traffic_lines = ["# Owner-private repository traffic", "", "Keep this companion private unless the owner separately chooses to publish it.",
            f"Repository: `{private['repository']}`; release context `{private['release_tag_context']}`.",
            f"Captured at {private['captured_at']} (UTC). Requested window: {WINDOW}. {mode}", ""]
        if private["status"] == "observed":
            data = private["data"]
            dates = data["returned_bucket_dates"]
            traffic_lines += [f"Provider totals: {data['count']} views, {data['uniques']} uniques.",
                f"Returned UTC buckets: {len(dates)}; first {dates[0] if dates else 'unavailable'}, last {dates[-1] if dates else 'unavailable'}. These bounds do not establish complete coverage."]
        else:
            traffic_lines += [f"Unavailable: {private['reason']}" + (f" (HTTP {private['http_status']})." if private["http_status"] else ".")]
        traffic_lines += ["", *TRAFFIC_LIMITS, ""]
        traffic_text = "\n".join(traffic_lines)
    return "\n".join(lines), traffic_text


def write_snapshot(destination: Path, capture: dict, *, replay: bool) -> int:
    public, private = reports(capture, replay=replay)
    public_text, private_text = markdown(public, private)
    destination.mkdir(parents=True, exist_ok=False)
    (destination / "public.json").write_text(canonical_json(public), encoding="utf-8")
    (destination / "public.md").write_text(public_text, encoding="utf-8")
    visibility = "private" if private is not None else "public"
    retained = dict(capture, retained_by="offline_replay" if replay else "gh_collection")
    (destination / f"capture-{visibility}.json").write_text(canonical_json(retained), encoding="utf-8")
    if private is not None:
        (destination / "traffic-private.json").write_text(canonical_json(private), encoding="utf-8")
        (destination / "traffic-private.md").write_text(private_text, encoding="utf-8")
    return 1 if private is not None and private["status"] != "observed" else 0


def main(arguments: list[str] | None = None, *, api=gh_api, now=utc_now) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--repository", required=True, help="explicit owner/name on github.com")
    parser.add_argument("--release", required=True, help="published release tag, vX.Y.Z")
    parser.add_argument("--window", required=True, choices=[WINDOW], help="traffic's provider window; asset counts remain cumulative")
    action = parser.add_mutually_exclusive_group()
    action.add_argument("--collect", action="store_true", help="explicitly invoke GET-only GitHub collection")
    action.add_argument("--replay", type=Path, help="format a sanitized capture without account access")
    parser.add_argument("--include-traffic", action="store_true", help="explicitly request a separate owner-private views/uniques companion")
    parser.add_argument("--output-dir", type=Path, help="new output directory; default is ignored out/release-counters")
    options = parser.parse_args(arguments)
    try:
        repository = repository_name(options.repository)
        tag = release_tag(options.release)
        if not options.collect and options.replay is None:
            if options.output_dir is not None:
                raise SnapshotError("--output-dir needs --collect or --replay")
            print(f"Plan only: {repository}, {tag}, requested traffic window {WINDOW}. No account read or file write.")
            print("Public GETs: repository metadata, published release, paginated selected asset counts.")
            print("Owner-private traffic GET: " + ("explicitly selected; not run" if options.include_traffic else "not requested"))
            return 0
        if options.output_dir is not None and options.output_dir.exists():
            raise SnapshotError("output directory already exists; prior snapshots are preserved")
        raw = json.loads(options.replay.read_text(encoding="utf-8")) if options.replay else collect(
            repository, tag, include_traffic=options.include_traffic, api=api, now=now)
        capture = sanitize_capture(raw, repository, tag, include_traffic=options.include_traffic)
        stamp = capture["public_captured_at"].replace(":", "").replace("-", "")
        destination = options.output_dir or REPOSITORY / "out/release-counters" / repository.replace("/", "--") / f"{tag}-{stamp}"
        code = write_snapshot(destination, capture, replay=options.replay is not None)
        print(f"Local snapshot: {destination.resolve()}")
        if options.include_traffic:
            print("Owner-private traffic is separate; no publication was performed.")
        return code
    except (OSError, ValueError, FetchError) as error:
        print(f"release_counters: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
