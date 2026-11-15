"""Run asynchronous production Web Audio lifecycle regressions in the offline gate."""
import pathlib
import shutil
import subprocess
import unittest


class WebAudioResumeTests(unittest.TestCase):
    def test_deferred_resume_lifecycle(self):
        root = pathlib.Path(__file__).resolve().parents[2]
        node = shutil.which("node")
        self.assertIsNotNone(node, "Node is required by the offline CI gate")
        result = subprocess.run(
            [node, "--test", "tests/audio/webaudio/resume_lifecycle.test.mjs"],
            cwd=root, capture_output=True, text=True, timeout=30,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
