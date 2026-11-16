"""Compare a named immutable release's starter/docs rebuild; never publish.

Planning is the default. --execute downloads the recorded originals, invokes
the named clean checkout's own packaging scripts and preserves a hash report.
No checkout, tag, line ending, manifest or remote asset is rewritten.
"""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import signal
import subprocess
import sys
import time
from urllib.request import Request, urlopen
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parent))
import git_local  # noqa: E402

API = "https://api.github.com/repos/matt439/Labrador"
DOWNLOAD = "https://github.com/matt439/Labrador/releases/download"
TAG = re.compile(r"v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)")
SHA = re.compile(r"[0-9a-f]{64}")
MAX_ASSET_BYTES = 256 * 1024 * 1024
REPOSITORY = Path(__file__).resolve().parents[1]


class Refusal(RuntimeError):
    """A required identity, input or command could not be established."""


def digest(path: Path) -> dict:
    with path.open("rb") as stream:
        sha = hashlib.file_digest(stream, "sha256").hexdigest()
    return {"bytes": path.stat().st_size, "sha256": sha}


def names(tag: str) -> tuple[str, str]:
    if not TAG.fullmatch(tag):
        raise Refusal("Use an explicit stable release tag, such as v0.3.0.")
    return f"labrador-starter-{tag}.zip", f"labrador-docs-{tag}.tar.gz"


def local_source(source: Path, tag: str) -> dict:
    names(tag)
    # Git may print a long Windows path when the caller used an 8.3 alias.
    # Compare filesystem identities while still requiring the actual root.
    source = source.resolve()
    def git(*arguments):
        return git_local.git(source, *arguments, text=True).stdout.strip()
    if Path(git("rev-parse", "--show-toplevel")).resolve() != source:
        raise Refusal("--source must be the release checkout's own root.")
    ref = f"refs/tags/{tag}"
    if git("cat-file", "-t", ref) != "tag":
        raise Refusal("The release requires an annotated tag.")
    header = git("cat-file", "-p", ref).split("\n\n", 1)[0].splitlines()
    if f"tag {tag}" not in header or "type commit" not in header:
        raise Refusal("The tag object must name this tag and a commit directly.")
    commit = git("rev-parse", "HEAD^{commit}")
    if git("rev-parse", f"{ref}^{{commit}}") != commit:
        raise Refusal("The release checkout's HEAD is not the requested tag.")
    if git("status", "--porcelain=v1", "--untracked-files=all", "--ignore-submodules=none"):
        raise Refusal("Release source must be clean, including untracked files.")
    if any(line and not line.startswith("H ") for line in git("ls-files", "-v").splitlines()):
        raise Refusal("Release source must have no hidden index flags.")
    if json.loads(git("show", f"{commit}:vcpkg.json"))["version-string"] != tag[1:]:
        raise Refusal("The release tag disagrees with vcpkg.json.")
    required = ["tools/consumer_check/starter_zip.py", "website/package-lock.json",
                "website/scripts/snapshot-source.mjs", "website/scripts/build-snapshot.mjs",
                "website/scripts/pack-snapshot.mjs"]
    for name in required:
        if not (source / name).is_file():
            raise Refusal(f"Missing release input: {name}")
    return {"tag": tag, "commit": commit, "tag_object": git("rev-parse", ref),
            "commit_date": git("show", "-s", "--format=%cI", commit),
            "inputs": {name: digest(source / name) for name in required}}


def release_assets(release: dict, ref: dict, annotated: dict, source: dict,
                   manifest: dict) -> list[dict]:
    tag = source["tag"]
    if not all(isinstance(value, dict) for value in (release, ref, annotated, manifest)):
        raise Refusal("Release, tag and manifest metadata must be JSON objects.")
    ref_object, tag_object = ref.get("object"), annotated.get("object")
    asset_list, releases = release.get("assets"), manifest.get("releases")
    if not isinstance(ref_object, dict) or not isinstance(tag_object, dict) \
            or not isinstance(asset_list, list) or not all(isinstance(asset, dict) for asset in asset_list) \
            or not isinstance(releases, list) or not all(isinstance(entry, dict) for entry in releases):
        raise Refusal("Missing or malformed tag objects, asset list or release manifest.")
    if release.get("tag_name") != tag or release.get("immutable") is not True \
            or release.get("draft") is not False or release.get("prerelease") is not False:
        raise Refusal("Require this tag's published, immutable, non-prerelease GitHub release.")
    if (ref.get("ref") != f"refs/tags/{tag}" or ref_object.get("type") != "tag"
            or ref_object.get("sha") != source["tag_object"]):
        raise Refusal("GitHub's annotated tag reference differs from the local tag object.")
    if annotated.get("sha") != source["tag_object"] or annotated.get("tag") != tag \
            or tag_object.get("type") != "commit" or tag_object.get("sha") != source["commit"]:
        raise Refusal("GitHub's tag object differs from the checked-out source commit.")
    selected = []
    for name in names(tag):
        matches = [asset for asset in asset_list if asset.get("name") == name]
        if len(matches) != 1:
            raise Refusal(f"Require exactly one release asset named {name}.")
        asset = matches[0]
        size = asset.get("size")
        sha = asset.get("digest", "")
        url = f"{DOWNLOAD}/{tag}/{name}"
        if type(size) is not int or not 0 < size <= MAX_ASSET_BYTES \
                or not isinstance(sha, str) or not sha.startswith("sha256:") \
                or not SHA.fullmatch(sha[7:]) or asset.get("state") != "uploaded" \
                or asset.get("browser_download_url") != url:
            raise Refusal(f"Missing or invalid size, SHA-256, state or canonical URL for {name}.")
        selected.append({"name": name, "bytes": size, "sha256": sha[7:],
                         "url": url, "asset_id": asset.get("id")})
    docs = [entry for entry in releases if entry.get("tag") == tag]
    if len(docs) != 1 or any(docs[0].get(key) != value for key, value in {
            "commit": source["commit"], "asset": selected[1]["name"],
            "bytes": selected[1]["bytes"], "sha256": selected[1]["sha256"]}.items()):
        raise Refusal("The recorded documentation identity disagrees with GitHub/source.")
    return selected


def compare(expected: dict, original: Path, rebuilt: Path) -> dict:
    if not original.is_file() or not rebuilt.is_file():
        raise Refusal(f"Missing original or rebuilt input for {expected['name']}.")
    actual = digest(original)
    recorded = {key: expected[key] for key in ("bytes", "sha256")}
    if actual != recorded:
        raise Refusal(f"Downloaded original does not match the release identity: {expected['name']}")
    candidate = digest(rebuilt)
    return {"name": expected["name"], "recorded": recorded, "original": actual,
            "rebuilt": candidate, "status": "match" if candidate == actual else "mismatch",
            "rule": "exact complete-file SHA-256 and byte count; no normalization"}


def download(url: str, destination: Path, limit: int) -> None:
    request = Request(url, headers={"User-Agent": "Labrador-release-rebuild",
                                   "Accept": "application/vnd.github+json" if url.startswith(API) else "application/octet-stream"})
    total = 0
    deadline = time.monotonic() + 120
    with urlopen(request, timeout=30) as response, destination.open("xb") as output:
        while data := response.read1(64 * 1024):
            total += len(data)
            if total > limit or time.monotonic() > deadline:
                raise Refusal("Download exceeded its recorded size/time bound; partial evidence retained.")
            output.write(data)


def command(argv: list[str], cwd: Path, output: Path, label: str, records: list,
            timeout: int) -> str:
    started = time.monotonic()
    row = {"argv": argv, "cwd": str(cwd), "timeout_seconds": timeout}
    records.append(row)
    process = subprocess.Popen(argv, cwd=cwd, env=git_local.environment(),
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               start_new_session=os.name != "nt")
    row["pid"] = process.pid
    try:
        stdout, stderr = process.communicate(timeout=timeout)
    except subprocess.TimeoutExpired as failure:
        row.update({"status": "timeout", "elapsed_seconds": time.monotonic() - started})
        # npm launches Astro as a child. Stop this owned tree, not just its
        # parent, so a timed-out command cannot leave the build running.
        if process.poll() is None:
            try:
                if os.name == "nt":
                    stopped = subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"],
                                             capture_output=True, timeout=15, check=False)
                    row["tree_cleanup_exit_code"] = stopped.returncode
                else:
                    os.killpg(process.pid, signal.SIGKILL)
            except (OSError, subprocess.TimeoutExpired) as cleanup:
                row["tree_cleanup_error"] = str(cleanup)
            if process.poll() is None:
                process.kill()
        try:
            stdout, stderr = process.communicate(timeout=15)
        except subprocess.TimeoutExpired:
            stdout, stderr = failure.stdout or b"", failure.stderr or b""
            row["tree_cleanup"] = "incomplete: output pipes still open after cleanup"
        (output / f"{label}.stdout.log").write_bytes(stdout)
        (output / f"{label}.stderr.log").write_bytes(stderr)
        raise Refusal(f"{label} timed out; logs retained.") from failure
    row.update({"exit_code": process.returncode, "elapsed_seconds": time.monotonic() - started})
    (output / f"{label}.stdout.log").write_bytes(stdout)
    (output / f"{label}.stderr.log").write_bytes(stderr)
    if process.returncode != 0:
        raise Refusal(f"{label} failed ({process.returncode}); logs retained.")
    return stdout.decode("utf-8", errors="replace").strip()


def execute(source: Path, tag: str, output: Path, convention: str, manifest: dict,
            *, fetch=download, run=command) -> dict:
    if output == source or source in output.parents:
        raise Refusal("Evidence must be outside the release checkout.")
    output.mkdir(parents=True, exist_ok=False)
    report = {"schema_version": 1, "started_utc": datetime.now(timezone.utc).isoformat(),
              "driver": {"path": str(Path(__file__).resolve()), **digest(Path(__file__))},
              "source_directory": str(source), "checkout_convention": convention,
              "comparison_rule": "exact complete-file bytes; no archive/content/line-ending normalization",
              "status": "incomplete", "commands": [], "comparisons": []}
    try:
        identity = local_source(source, tag)
        report["source"] = identity
        if any(os.environ.get(key) is not None for key in
               ("LABRADOR_COMMIT", "LABRADOR_COMMIT_DATE", "LABRADOR_DOCS_TAG")):
            raise Refusal("Unset LABRADOR_COMMIT, LABRADOR_COMMIT_DATE and LABRADOR_DOCS_TAG.")
        node = shutil.which("node")
        npm = shutil.which("npm")
        npm_cli = Path(npm).parent / "node_modules/npm/bin/npm-cli.js" if npm else Path()
        if not node or not npm or not npm_cli.is_file():
            raise Refusal("Need Node and npm's adjacent node_modules/npm/bin/npm-cli.js (standard Windows install).")
        report["toolchain"] = {"platform": platform.platform(), "python": sys.version,
                               "python_executable": sys.executable, "zlib_compile": zlib.ZLIB_VERSION,
                               "zlib_runtime": zlib.ZLIB_RUNTIME_VERSION, "node_executable": node,
                               "npm_cli": str(npm_cli)}
        def call(argv, cwd, label, timeout=60):
            return run([str(arg) for arg in argv], cwd, output, label, report["commands"], timeout)
        for label, argv in (("node-version", [node, "--version"]),
                            ("node-components", [node, "-p", "JSON.stringify(process.versions)"]),
                            ("npm-version", [node, npm_cli, "--version"]),
                            ("git-version", ["git", "--version"])):
            report["toolchain"][label] = call(argv, source, label)
        eol = git_local.git(source, "ls-files", "--eol", "-z").stdout
        (output / "checkout-eol.txt").write_bytes(eol.replace(b"\0", b"\n"))
        report["checkout_eol_sha256"] = digest(output / "checkout-eol.txt")
        report["recorded_docs_manifest"] = manifest
        metadata = {}
        for label, url in (("release", f"{API}/releases/tags/{tag}"),
                           ("tag-ref", f"{API}/git/ref/tags/{tag}"),
                           ("tag-object", f"{API}/git/tags/{identity['tag_object']}")):
            target = output / f"{label}.json"
            fetch(url, target, 4 * 1024 * 1024)
            metadata[label] = json.loads(target.read_bytes())
        assets = release_assets(metadata["release"], metadata["tag-ref"], metadata["tag-object"], identity, manifest)
        report["release_id"] = metadata["release"].get("id")
        report["assets"] = assets
        originals, rebuilt = output / "originals", output / "rebuilt"
        originals.mkdir()
        rebuilt.mkdir()
        for asset in assets:
            fetch(asset["url"], originals / asset["name"], asset["bytes"])
            if digest(originals / asset["name"]) != {key: asset[key] for key in ("bytes", "sha256")}:
                raise Refusal(f"Downloaded original failed verification: {asset['name']}")
        website = source / "website"
        packed_docs = website / "release-assets" / assets[1]["name"]
        if packed_docs.exists() or (website / "dist/snapshot").exists():
            raise Refusal("Use a fresh release checkout: candidate docs output already exists.")
        source_gate = json.loads(call([node, website / "scripts/snapshot-source.mjs", tag], website, "source-before"))
        if not isinstance(source_gate, dict) or source_gate.get("commit") != identity["commit"] \
                or source_gate.get("tag") != tag:
            raise Refusal("The release's own snapshot source gate disagrees with verified identity.")
        call([sys.executable, source / "tools/consumer_check/starter_zip.py", rebuilt,
              "--release", tag], source, "starter", 180)
        call([node, npm_cli, "ci", "--no-audit", "--no-fund"], website, "npm-ci", 300)
        call([node, npm_cli, "run", "snapshot:build", "--", tag], website, "docs-build", 300)
        call([node, npm_cli, "run", "snapshot:pack", "--", tag], website, "docs-pack", 180)
        shutil.copyfile(packed_docs, rebuilt / assets[1]["name"])
        if local_source(source, tag) != identity:
            raise Refusal("Release source identity changed during the rebuild.")
        source_after = json.loads(call([node, website / "scripts/snapshot-source.mjs", tag], website, "source-after"))
        if source_after != source_gate:
            raise Refusal("The release's snapshot source record changed during the rebuild.")
        report["comparisons"] = [compare(asset, originals / asset["name"], rebuilt / asset["name"])
                                 for asset in assets]
        report["status"] = "match" if all(row["status"] == "match" for row in report["comparisons"]) else "mismatch"
    except (Refusal, git_local.GitError, OSError, ValueError, KeyError) as failure:
        report["error"] = str(failure)
    finally:
        report["finished_utc"] = datetime.now(timezone.utc).isoformat()
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return report


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--release", required=True)
    parser.add_argument("--source", required=True, type=Path, help="fresh clean checkout of the annotated release tag")
    parser.add_argument("--output", required=True, type=Path, help="new evidence directory outside that checkout")
    parser.add_argument("--convention", required=True, help="how this checkout's line endings were selected; recorded, never applied")
    parser.add_argument("--manifest", type=Path, default=REPOSITORY / "website/published-versions.json")
    parser.add_argument("--execute", action="store_true", help="explicitly download and rebuild; otherwise print the plan")
    args = parser.parse_args(argv)
    try:
        source, output = args.source.resolve(), args.output.resolve()
        identity = local_source(source, args.release)
        if not args.execute:
            print(json.dumps({"mode": "plan; nothing downloaded or built", "source": identity,
                              "output": str(output), "convention": args.convention,
                              "assets": names(args.release), "command_timeouts_seconds": [180, 300, 300, 180]}, indent=2))
            return 0
        report = execute(source, args.release, output, args.convention,
                         json.loads(args.manifest.read_text(encoding="utf-8")))
        print(f"{report['status']}: {output / 'report.json'}")
        if report.get("error"):
            print(report["error"], file=sys.stderr)
        return {"match": 0, "mismatch": 1, "incomplete": 2}[report["status"]]
    except (Refusal, git_local.GitError, OSError, ValueError, KeyError) as failure:
        print(f"release_rebuild: {failure}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
