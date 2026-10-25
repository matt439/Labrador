"""Hold tools/consumer_check/sandbox.py's two files to the layout the sandbox reads."""

from __future__ import annotations

import contextlib
import importlib.util
import io
import tempfile
import unittest
import xml.etree.ElementTree as ElementTree
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "sandbox", REPOSITORY / "tools/consumer_check/sandbox.py"
)
sandbox = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(sandbox)


class SandboxTests(unittest.TestCase):
    def write(self, folder: Path, program: str = "MyGame.exe") -> Path:
        # An ampersand in a host path must reach the XML escaped.
        release = folder / "release & co"
        release.mkdir()
        (release / program).write_bytes(b"")
        results = folder / "results"
        with contextlib.redirect_stdout(io.StringIO()):
            sandbox.write(release, results, program, "Disable")
        return results

    def test_configuration_maps_the_release_read_only_and_the_results_writable(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            results = self.write(folder)
            root = ElementTree.parse(results / "clean-machine.wsb").getroot()
            self.assertEqual(root.findtext("VGpu"), "Disable")
            mapped = {
                entry.findtext("SandboxFolder"): (entry.findtext("HostFolder"), entry.findtext("ReadOnly"))
                for entry in root.iter("MappedFolder")
            }
            self.assertEqual(mapped[sandbox.RELEASE_IN_SANDBOX], (str(folder / "release & co"), "true"))
            self.assertEqual(mapped[sandbox.RESULTS_IN_SANDBOX], (str(results), "false"))
            self.assertEqual(
                root.findtext("LogonCommand/Command"), "cmd /c " + sandbox.RESULTS_IN_SANDBOX + "\\run.cmd"
            )

    def test_run_cmd_names_the_program_and_the_mapped_release(self):
        with tempfile.TemporaryDirectory() as temporary:
            results = self.write(Path(temporary), "Other.exe")
            run = (results / "run.cmd").read_bytes().decode("ascii")
            self.assertIn('set "program=Other.exe"', run)
            self.assertIn('set "release=' + sandbox.RELEASE_IN_SANDBOX + '"', run)
            self.assertIn("--smoke-test", run)
            self.assertNotIn("\n", run.replace("\r\n", ""))

    def test_a_folder_without_the_program_is_refused(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaises(SystemExit):
                sandbox.write(Path(temporary), Path(temporary) / "results", "MyGame.exe", "Default")


if __name__ == "__main__":
    unittest.main()
