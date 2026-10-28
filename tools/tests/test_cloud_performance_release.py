"""Offline release tests using synthetic data and local fixtures."""

from __future__ import annotations

import subprocess
import tempfile
import unittest
import zipfile
from pathlib import Path

from tools.cloud_performance import release
from tools.tests.cloud_performance_fixtures import config


class ReleaseTests(unittest.TestCase):
    def test_bundle_records_dirty_source_and_detects_tamper(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            subprocess.run(["git", "init"], cwd=root, check=True, stdout=subprocess.DEVNULL)
            (root / "engine").mkdir()
            (root / "engine" / "tracked.cpp").write_text("tracked", encoding="utf-8")
            (root / "CMakeLists.txt").write_text("project(Test)", encoding="utf-8")
            subprocess.run(["git", "add", "."], cwd=root, check=True)
            subprocess.run(["git", "-c", "user.name=Test", "-c", "user.email=test@example.com",
                            "commit", "-m", "base"], cwd=root, check=True,
                           stdout=subprocess.DEVNULL)
            (root / "engine" / "dirty.cpp").write_text("uncommitted", encoding="utf-8")
            payload = root / "payload"
            payload.mkdir()
            (payload / "LineSweeperFrameBench.exe").write_bytes(b"binary")
            archive = root / "out" / "bundle.zip"
            result = release.bundle(payload, archive, root=root)
            manifest = release.verify(archive, result["archive_sha256"])
            self.assertTrue(manifest["git"]["dirty"])
            self.assertIn("source/engine/dirty.cpp", manifest["files"])
            self.assertIn("payload/LineSweeperFrameBench.exe", manifest["files"])
            release.verify_workspace_snapshot(manifest, root)
            (root / "engine" / "tracked.cpp").write_text("changed", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "source drift"):
                release.verify_workspace_snapshot(manifest, root)
            tampered = root / "tampered.zip"
            with zipfile.ZipFile(archive) as source, zipfile.ZipFile(tampered, "w") as output:
                for member in source.infolist():
                    data = source.read(member.filename)
                    output.writestr(member.filename, b"changed" if member.filename.endswith(".exe") else data)
            with self.assertRaisesRegex(ValueError, "drift"):
                release.verify(tampered)

    def test_archive_traversal_and_output_overwrite_are_refused(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive = root / "bad.zip"
            with zipfile.ZipFile(archive, "w") as output:
                output.writestr("../escape", b"bad")
            with self.assertRaisesRegex(ValueError, "root"):
                release.verify(archive)
            payload = root / "payload"
            payload.mkdir()
            (payload / "operator.pem").write_text("secret", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "credentials"):
                release.payload_files(payload)

    def test_declared_backend_requires_executable_and_both_assets(self):
        backend = config()["workload"]["backends"]
        manifest = {"files": {
            "payload/d3d11/LineSweeperFrameBench.exe": {},
        }}
        with self.assertRaisesRegex(ValueError, "runtime input"):
            release.verify_benchmark_payload(manifest, backend)

        manifest["files"].update({
            "payload/d3d11/fonts/jetbrains_mono_bold_16.spritefont": {},
            "payload/d3d11/textures/white.dds": {},
        })
        release.verify_benchmark_payload(manifest, backend)


if __name__ == "__main__":
    unittest.main()
