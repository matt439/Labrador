"""Compare-only release rebuild fixtures: no network, npm or asset publication."""

from __future__ import annotations

import contextlib
import copy
import hashlib
import io
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

from tools.tests.tool_fixtures import isolate_repository, isolated_git_environment, load_tool, windows_short_path

rebuild = load_tool("tools/release_rebuild.py")
TAG = "v0.3.0"
SOURCE = {"tag": TAG, "commit": "a" * 40, "tag_object": "b" * 40}
CONTENT = (b"original zip\r\n", b"original documentation\n")


def metadata():
    assets = [{"name": name, "size": len(data), "digest": "sha256:" + hashlib.sha256(data).hexdigest(),
               "id": index + 10, "state": "uploaded", "browser_download_url": f"{rebuild.DOWNLOAD}/{TAG}/{name}"}
              for index, (name, data) in enumerate(zip(rebuild.names(TAG), CONTENT))]
    release = {"tag_name": TAG, "immutable": True, "draft": False, "prerelease": False,
               "id": 42, "assets": assets}
    ref = {"ref": f"refs/tags/{TAG}", "object": {"type": "tag", "sha": SOURCE["tag_object"]}}
    annotated = {"sha": SOURCE["tag_object"], "tag": TAG,
                 "object": {"type": "commit", "sha": SOURCE["commit"]}}
    manifest = {"releases": [{"tag": TAG, "commit": SOURCE["commit"], "asset": assets[1]["name"],
                              "bytes": assets[1]["size"], "sha256": assets[1]["digest"][7:]}]}
    return release, ref, annotated, manifest


class Identities(unittest.TestCase):
    def test_exact_immutable_asset_and_remote_source_identities(self):
        release, ref, annotated, manifest = metadata()
        selected = rebuild.release_assets(release, ref, annotated, SOURCE, manifest)
        self.assertEqual([asset["name"] for asset in selected], list(rebuild.names(TAG)))
        self.assertEqual(selected[0]["sha256"], hashlib.sha256(CONTENT[0]).hexdigest())

    def test_mutable_draft_prerelease_or_wrong_release_is_refused(self):
        for key, value in (("immutable", False), ("draft", True), ("prerelease", True), ("tag_name", "v0.3.1")):
            with self.subTest(key=key):
                release, ref, annotated, manifest = metadata()
                release[key] = value
                with self.assertRaises(rebuild.Refusal):
                    rebuild.release_assets(release, ref, annotated, SOURCE, manifest)

    def test_missing_duplicate_or_unverifiable_asset_is_refused(self):
        for change in (lambda r: r["assets"].pop(),
                       lambda r: r["assets"].append(copy.deepcopy(r["assets"][0])),
                       lambda r: r["assets"][0].update(digest=None),
                       lambda r: r["assets"][0].update(size=True),
                       lambda r: r["assets"][0].update(size=rebuild.MAX_ASSET_BYTES + 1),
                       lambda r: r["assets"][0].update(browser_download_url="https://example.invalid/asset")):
            release, ref, annotated, manifest = metadata()
            change(release)
            with self.assertRaises(rebuild.Refusal):
                rebuild.release_assets(release, ref, annotated, SOURCE, manifest)

    def test_tag_object_commit_and_manifest_must_all_agree(self):
        for index, change in ((1, lambda r: r["object"].update(sha="c" * 40)),
                              (1, lambda r: r["object"].update(type="commit")),
                              (2, lambda r: r["object"].update(sha="c" * 40)),
                              (2, lambda r: r.update(tag="v0.3.1")),
                              (3, lambda r: r["releases"][0].update(sha256="c" * 64))):
            values = list(metadata())
            change(values[index])
            with self.assertRaises(rebuild.Refusal):
                rebuild.release_assets(*values[:3], SOURCE, values[3])

    def test_explicit_stable_tag_cannot_be_a_path_or_option(self):
        for tag in ("", "--upload", "v0.3.0/other", "v0.3.0-dev", "v00.3.0"):
            with self.subTest(tag=tag), self.assertRaises(rebuild.Refusal):
                rebuild.names(tag)

    def test_malformed_metadata_is_a_refusal(self):
        for index, value in ((0, None), (1, []), (3, None)):
            values = list(metadata())
            values[index] = value
            with self.assertRaises(rebuild.Refusal):
                rebuild.release_assets(*values[:3], SOURCE, values[3])
        for index, key in ((0, "assets"), (1, "object"), (2, "object"), (3, "releases")):
            values = list(metadata())
            values[index][key] = None
            with self.assertRaises(rebuild.Refusal):
                rebuild.release_assets(*values[:3], SOURCE, values[3])


class Comparisons(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.original, self.rebuilt = self.root / "original", self.root / "rebuilt"
        self.original.write_bytes(CONTENT[0])
        self.rebuilt.write_bytes(CONTENT[0])
        self.asset = {"name": "asset.zip", **rebuild.digest(self.original)}

    def test_match_uses_complete_hash_and_bytes(self):
        self.assertEqual(rebuild.compare(self.asset, self.original, self.rebuilt)["status"], "match")

    def test_equal_size_mismatch_is_preserved_without_normalizing(self):
        different = CONTENT[0].replace(b"original", b"changed!")
        self.assertEqual(len(different), len(CONTENT[0]))
        self.rebuilt.write_bytes(different)
        self.assertEqual(rebuild.compare(self.asset, self.original, self.rebuilt)["status"], "mismatch")
        self.assertEqual(self.original.read_bytes(), CONTENT[0])
        self.assertEqual(self.rebuilt.read_bytes(), different)

    def test_missing_input_or_changed_original_is_not_a_rebuild_mismatch(self):
        self.rebuilt.unlink()
        with self.assertRaisesRegex(rebuild.Refusal, "Missing"):
            rebuild.compare(self.asset, self.original, self.rebuilt)
        self.rebuilt.write_bytes(CONTENT[0])
        self.original.write_bytes(b"wrong baseline")
        with self.assertRaisesRegex(rebuild.Refusal, "original does not match"):
            rebuild.compare(self.asset, self.original, self.rebuilt)


class Execution(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source, self.output = self.root / "source", self.root / "evidence"
        self.source.mkdir()
        self.node = self.root / "tools/node.exe"
        self.npm = self.root / "tools/npm.cmd"
        cli = self.npm.parent / "node_modules/npm/bin/npm-cli.js"
        cli.parent.mkdir(parents=True)
        cli.write_bytes(b"never executed")
        self.release, self.ref, self.annotated, self.manifest = metadata()
        self.failure = None
        self.candidate = CONTENT
        self.fetches = []

    def fetch(self, url, destination, limit):
        self.fetches.append(url)
        if url.endswith(f"releases/tags/{TAG}"):
            data = json.dumps(self.release).encode()
        elif url.endswith(f"git/ref/tags/{TAG}"):
            data = json.dumps(self.ref).encode()
        elif url.endswith(SOURCE["tag_object"]):
            data = json.dumps(self.annotated).encode()
        else:
            data = CONTENT[list(rebuild.names(TAG)).index(url.rsplit("/", 1)[1])]
        self.assertLessEqual(len(data), limit)
        with destination.open("xb") as stream:
            stream.write(data)

    def run_command(self, argv, cwd, output, label, records, timeout):
        records.append({"argv": argv, "cwd": str(cwd), "timeout_seconds": timeout})
        if label == self.failure:
            raise rebuild.Refusal("fixture command failure")
        if label.startswith("source-"):
            return json.dumps({"tag": TAG, "commit": SOURCE["commit"]})
        if label == "starter":
            (output / "rebuilt" / rebuild.names(TAG)[0]).write_bytes(self.candidate[0])
        if label == "docs-pack":
            destination = self.source / "website/release-assets" / rebuild.names(TAG)[1]
            destination.parent.mkdir(parents=True)
            destination.write_bytes(self.candidate[1])
        return "fixture version"

    def execute(self):
        find = lambda name: str(self.node if name == "node" else self.npm)
        with mock.patch.object(rebuild, "local_source", return_value=SOURCE), \
                mock.patch.object(rebuild.shutil, "which", side_effect=find), \
                mock.patch.object(rebuild.git_local, "git", return_value=subprocess.CompletedProcess([], 0, b"i/lf w/crlf fixture\0")):
            return rebuild.execute(self.source, TAG, self.output, "fixture CRLF", self.manifest,
                                   fetch=self.fetch, run=self.run_command)

    def test_complete_compare_preserves_both_assets_and_only_invokes_build_commands(self):
        report = self.execute()
        self.assertEqual(report["status"], "match", report)
        self.assertEqual(len(self.fetches), 5)
        for index, name in enumerate(rebuild.names(TAG)):
            self.assertEqual((self.output / "originals" / name).read_bytes(), CONTENT[index])
            self.assertEqual((self.output / "rebuilt" / name).read_bytes(), CONTENT[index])
        commands = report["commands"]
        self.assertTrue(any("starter_zip.py" in " ".join(row["argv"]) for row in commands))
        self.assertTrue(any("snapshot:pack" in row["argv"] for row in commands))
        self.assertFalse(any(any(word in " ".join(row["argv"]) for word in ("upload", "publish", "--clobber"))
                             for row in commands))
        with self.assertRaises(FileExistsError):
            self.execute()

    def test_mismatch_is_complete_and_both_differing_bytes_survive(self):
        self.candidate = (b"different zip", CONTENT[1])
        report = self.execute()
        self.assertEqual(report["status"], "mismatch")
        self.assertEqual(report["comparisons"][0]["status"], "mismatch")
        self.assertEqual((self.output / "originals" / rebuild.names(TAG)[0]).read_bytes(), CONTENT[0])
        self.assertEqual((self.output / "rebuilt" / rebuild.names(TAG)[0]).read_bytes(), self.candidate[0])

    def test_failed_build_keeps_originals_and_incomplete_report(self):
        self.failure = "docs-build"
        report = self.execute()
        self.assertEqual(report["status"], "incomplete")
        self.assertIn("fixture command failure", report["error"])
        self.assertTrue((self.output / "originals" / rebuild.names(TAG)[1]).is_file())
        self.assertEqual(json.loads((self.output / "report.json").read_text())["status"], "incomplete")

    def test_original_hash_failure_stops_before_packaging(self):
        self.release["assets"][0]["digest"] = "sha256:" + "0" * 64
        report = self.execute()
        self.assertEqual(report["status"], "incomplete")
        self.assertIn("original failed verification", report["error"])
        self.assertEqual((self.output / "originals" / rebuild.names(TAG)[0]).read_bytes(), CONTENT[0])
        self.assertFalse(any("starter_zip.py" in " ".join(row["argv"]) for row in report["commands"]))

    def test_missing_tool_and_nested_output_are_refused(self):
        with mock.patch.object(rebuild, "local_source", return_value=SOURCE), \
                mock.patch.object(rebuild.shutil, "which", return_value=None):
            report = rebuild.execute(self.source, TAG, self.output, "fixture", self.manifest,
                                     fetch=self.fetch, run=self.run_command)
        self.assertEqual(report["status"], "incomplete")
        self.assertEqual(self.fetches, [])
        with self.assertRaisesRegex(rebuild.Refusal, "outside"):
            rebuild.execute(self.source, TAG, self.source / "out", "fixture", self.manifest)

    def test_default_plan_does_not_create_evidence_or_execute(self):
        with mock.patch.object(rebuild, "local_source", return_value=SOURCE), \
                mock.patch.object(rebuild, "execute") as execute, contextlib.redirect_stdout(io.StringIO()):
            code = rebuild.main(["--release", TAG, "--source", str(self.source),
                                 "--output", str(self.output), "--convention", "fixture"])
        self.assertEqual(code, 0)
        execute.assert_not_called()
        self.assertFalse(self.output.exists())

    def test_timeout_retains_partial_stdout_and_stderr(self):
        self.output.mkdir()
        records = []
        process = mock.Mock(pid=12345)
        process.poll.return_value = None
        process.communicate.side_effect = [subprocess.TimeoutExpired(["fixture"], 1),
                                          (b"partial output", b"partial error")]
        with mock.patch.object(rebuild.subprocess, "Popen", return_value=process), \
                mock.patch.object(rebuild.subprocess, "run", return_value=subprocess.CompletedProcess([], 0)) as stop, \
                mock.patch.object(rebuild.os, "killpg", create=True) as kill:
            with self.assertRaisesRegex(rebuild.Refusal, "timed out"):
                rebuild.command(["fixture"], self.source, self.output, "build", records, 1)
            if rebuild.os.name == "nt":
                self.assertEqual(stop.call_args.args[0], ["taskkill", "/PID", "12345", "/T", "/F"])
            else:
                kill.assert_called_once_with(12345, rebuild.signal.SIGKILL)
        self.assertEqual(records[0]["status"], "timeout")
        self.assertEqual((self.output / "build.stdout.log").read_bytes(), b"partial output")
        self.assertEqual((self.output / "build.stderr.log").read_bytes(), b"partial error")

    def test_real_process_output_and_exit_status_are_retained(self):
        self.output.mkdir()
        records = []
        result = rebuild.command([sys.executable, "-c", "print('fixture output')"],
                                 self.source, self.output, "probe", records, 10)
        self.assertEqual(result, "fixture output")
        self.assertEqual(records[0]["exit_code"], 0)
        self.assertGreater(records[0]["pid"], 0)
        self.assertIn(b"fixture output", (self.output / "probe.stdout.log").read_bytes())

    def test_download_size_bound_keeps_partial_bytes(self):
        response = mock.MagicMock()
        response.__enter__.return_value = response
        response.read1.side_effect = [b"ab", b"cd", b""]
        destination = self.root / "partial-original"
        with mock.patch.object(rebuild, "urlopen", return_value=response), \
                self.assertRaisesRegex(rebuild.Refusal, "size/time bound"):
            rebuild.download(rebuild.DOWNLOAD + "/fixture", destination, 3)
        self.assertEqual(destination.read_bytes(), b"ab")


@unittest.skipUnless(shutil.which("git"), "Git source identity needs local objects")
class Source(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name).resolve()
        self.environment, hooks = isolated_git_environment(self.root, "rebuild-fixture")
        def git(*args):
            return subprocess.run(["git", "-C", str(self.root), *args], env=self.environment,
                                  check=True, capture_output=True, text=True).stdout.strip()
        self.git = git
        git("init", "-q")
        isolate_repository(git, hooks)
        git("config", "user.name", "Fixture")
        git("config", "user.email", "fixture@example.invalid")
        files = ["tools/consumer_check/starter_zip.py", "website/package-lock.json",
                 "website/scripts/snapshot-source.mjs", "website/scripts/build-snapshot.mjs",
                 "website/scripts/pack-snapshot.mjs"]
        for name in files:
            target = self.root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text("fixture\n", encoding="ascii")
        (self.root / "vcpkg.json").write_text('{"version-string":"0.3.0"}\n', encoding="ascii")
        git("add", "--all")
        git("commit", "-qm", "fixture")
        git("tag", "-a", TAG, "-m", "fixture")

    def test_clean_annotated_source_and_raw_input_digests(self):
        record = rebuild.local_source(self.root, TAG)
        self.assertEqual(record["commit"], self.git("rev-parse", "HEAD"))
        self.assertIn("website/package-lock.json", record["inputs"])
        self.assertEqual(record["inputs"]["website/package-lock.json"],
                         rebuild.digest(self.root / "website/package-lock.json"))

    def test_dirty_or_nonmatching_tag_source_cannot_run(self):
        (self.root / "website/package-lock.json").write_bytes(b"changed")
        with self.assertRaisesRegex(rebuild.Refusal, "clean"):
            rebuild.local_source(self.root, TAG)

    def test_a_nested_directory_is_not_the_release_root(self):
        with self.assertRaisesRegex(rebuild.Refusal, "own root"):
            rebuild.local_source(self.root / "website", TAG)

    def test_a_commit_after_the_tag_is_not_the_requested_release(self):
        (self.root / "website/package-lock.json").write_bytes(b"changed")
        self.git("add", "website/package-lock.json")
        self.git("commit", "-qm", "later source")
        with self.assertRaisesRegex(rebuild.Refusal, "HEAD is not"):
            rebuild.local_source(self.root, TAG)

    def test_a_real_short_alias_keeps_root_identity_and_source_guards(self):
        alias = windows_short_path(self.root)
        self.assertNotEqual(alias, self.root)
        self.assertEqual(rebuild.local_source(alias, TAG), rebuild.local_source(self.root, TAG))
        with self.assertRaisesRegex(rebuild.Refusal, "own root"):
            rebuild.local_source(alias / "website", TAG)
        (self.root / "website/package-lock.json").write_bytes(b"changed")
        with self.assertRaisesRegex(rebuild.Refusal, "clean"):
            rebuild.local_source(alias, TAG)


if __name__ == "__main__":
    unittest.main()
