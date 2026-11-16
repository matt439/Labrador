"""Private symbol capture keeps exact source bytes and never replaces receipts."""

from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import zipfile

from tools.tests.tool_fixtures import load_tool

capture = load_tool("tools/consumer_check/retain_symbols.py")


class RetainSymbolsTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="labrador-retained-symbols-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.source = self.root / "project"
        (self.source / "game").mkdir(parents=True)
        (self.source / "game/main.cpp").write_bytes(b"int main() { return 0; }\r\n")
        (self.source / "external/labrador").mkdir(parents=True)
        (self.source / "external/labrador/.git").write_text("gitdir: private metadata", encoding="utf-8")
        (self.source / "external/labrador/engine.h").write_bytes(b"// engine\n")
        binaries = self.source / "out/build/game"
        binaries.mkdir(parents=True)
        self.exe, self.pdb = binaries / "MyGame.exe", binaries / "MyGame.pdb"
        self.exe.write_bytes(b"fixture executable")
        self.pdb.write_bytes(b"fixture symbols")
        self.destination = self.source / "out/private-symbols/build-one"

    def retain(self, destination=None):
        return capture.retain(self.exe, self.pdb, self.source, destination or self.destination)

    def test_capture_preserves_crlf_source_and_hashes_without_build_or_git_data(self):
        result = self.retain()
        with zipfile.ZipFile(self.destination / "source.zip") as archive:
            self.assertEqual(sorted(archive.namelist()), ["external/labrador/engine.h", "game/main.cpp"])
            source = archive.read("game/main.cpp")
            self.assertEqual(source, b"int main() { return 0; }\r\n")
        self.assertEqual(result["source"]["files"]["game/main.cpp"], hashlib.sha256(source).hexdigest())
        self.assertEqual(result["exe"]["sha256"], hashlib.sha256(self.exe.read_bytes()).hexdigest())
        self.assertEqual((self.destination / "MyGame.pdb").read_bytes(), self.pdb.read_bytes())
        self.assertEqual(json.loads((self.destination / "identity.json").read_text()), result)

    def test_a_retained_pair_is_never_replaced(self):
        self.retain()
        original = (self.destination / "identity.json").read_bytes()
        self.exe.write_bytes(b"a later build")
        with self.assertRaisesRegex(ValueError, "already exists"):
            self.retain()
        self.assertEqual((self.destination / "identity.json").read_bytes(), original)

    def test_an_unexcluded_destination_inside_sources_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "excluded out/"):
            self.retain(self.source / "private")
        self.assertFalse((self.source / "private").exists())

    def test_missing_or_misnamed_symbols_are_rejected_before_output(self):
        wrong = self.pdb.with_name("AnotherGame.pdb")
        wrong.write_bytes(b"other symbols")
        with self.assertRaisesRegex(ValueError, "same-basename"):
            capture.retain(self.exe, wrong, self.source, self.destination)
        self.pdb.unlink()
        with self.assertRaisesRegex(ValueError, "must exist"):
            self.retain()
        self.assertFalse(self.destination.exists())

    def test_source_links_are_refused_instead_of_capturing_another_tree(self):
        other = self.root / "outside.cpp"
        other.write_text("outside source", encoding="utf-8")
        try:
            (self.source / "game/linked.cpp").symlink_to(other)
        except OSError as error:
            self.skipTest(f"This host cannot create a fixture symlink: {error}")
        with self.assertRaisesRegex(ValueError, "does not follow links"):
            self.retain()
        self.assertFalse(self.destination.exists())

    def test_directory_scan_failure_cannot_produce_a_completed_capture(self):
        def denied_scan(root, *, followlinks, onerror):
            onerror(PermissionError("Fixture source subtree cannot be read"))
            return iter(())

        with patch.object(capture.os, "walk", side_effect=denied_scan):
            with self.assertRaisesRegex(PermissionError, "cannot be read"):
                self.retain()
        self.assertFalse(self.destination.exists())

    def test_the_selected_probe_receipt_is_copied_and_hashed(self):
        receipt = self.root / "probe.txt"
        receipt.write_bytes(b"symbol probe passed\r\n")
        result = capture.retain(self.exe, self.pdb, self.source, self.destination, receipt)
        self.assertEqual(result["receipt"]["sha256"], hashlib.sha256(receipt.read_bytes()).hexdigest())
        self.assertEqual((self.destination / "symbol-check.txt").read_bytes(), receipt.read_bytes())

    @unittest.skipUnless(os.name == "nt", "Windows junction fixture")
    def test_windows_junction_is_refused_without_requiring_symlink_privilege(self):
        outside = self.root / "outside-tree"
        outside.mkdir()
        junction = self.source / "game/junction"
        self.assertTrue(outside.resolve().is_relative_to(self.root.resolve()))
        result = subprocess.run(["cmd.exe", "/d", "/c", "mklink", "/J", str(junction), str(outside)],
                                capture_output=True, text=True, timeout=10, check=False)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        with self.assertRaisesRegex(ValueError, "does not follow links"):
            self.retain()
        self.assertFalse(self.destination.exists())


if __name__ == "__main__":
    unittest.main()
