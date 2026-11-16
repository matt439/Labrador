"""Run the game-owned persistence coordinator's offline regressions."""
import pathlib
import shutil
import subprocess
import unittest


class SaveRecoveryPersistTests(unittest.TestCase):
    def test_production_coordinator_lifecycle(self):
        root = pathlib.Path(__file__).resolve().parents[2]
        node = shutil.which("node")
        self.assertIsNotNone(node, "Node is required by the offline CI gate")
        result = subprocess.run(
            [node, "--test", "tools/save_check/recovery_persist.test.mjs"],
            cwd=root, capture_output=True, text=True, timeout=30,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
