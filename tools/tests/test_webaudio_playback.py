"""Run the real-playback observer's offline fixtures; no browser is launched."""

import pathlib
import shutil
import subprocess
import unittest


class WebAudioPlaybackTests(unittest.TestCase):
    def test_observer_rejects_incomplete_pipeline_evidence(self):
        root = pathlib.Path(__file__).resolve().parents[2]
        node = shutil.which("node")
        self.assertIsNotNone(node, "Node is required by the offline CI gate")
        result = subprocess.run(
            [node, "--test", "tests/audio/webaudio/playback_input.test.mjs"],
            cwd=root, capture_output=True, text=True, timeout=30,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
