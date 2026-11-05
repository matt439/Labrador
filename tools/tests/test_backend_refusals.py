"""Configure refuses each audio and input backend on a platform without its API."""

from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[2]


def refusal_block(axis: str, native: str) -> str:
    """engine/CMakeLists.txt's platform refusal for one axis, as written there."""
    source = (REPOSITORY / "engine/CMakeLists.txt").read_text(encoding="utf-8")
    start = source.index(f'if(LABRADOR_{axis}_BACKEND STREQUAL "{native}" AND EMSCRIPTEN)')
    end = source.index("endif()", start) + len("endif()")
    return source[start:end] + "\n"


class BackendRefusalTests(unittest.TestCase):
    def setUp(self):
        self.cmake = os.environ.get("CMAKE_COMMAND") or shutil.which("cmake")
        if not self.cmake:
            self.skipTest("CMake is required (or CMAKE_COMMAND)")
        temporary = tempfile.TemporaryDirectory(prefix="labrador-refusals-")
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)

    def configure(self, block: str, variable: str, value: str, browser: bool) -> subprocess.CompletedProcess:
        script = self.directory / "refusal.cmake"
        script.write_text(block, encoding="utf-8")
        arguments = [self.cmake, f"-D{variable}={value}"]
        if browser:
            arguments.append("-DEMSCRIPTEN=1")
        return subprocess.run([*arguments, "-P", str(script)], capture_output=True, timeout=30)

    def check_axis(self, axis: str, native: str, browser_api: str, neutral: list[str]):
        block = refusal_block(axis, native)
        variable = f"LABRADOR_{axis}_BACKEND"
        for value, browser, refused in [(native, True, True), (browser_api, False, True),
                                        (native, False, False), (browser_api, True, False),
                                        *[(name, browser, False) for name in neutral for browser in (True, False)]]:
            with self.subTest(value=value, browser=browser):
                result = self.configure(block, variable, value, browser)
                output = (result.stdout + result.stderr).decode("utf-8", errors="replace")
                if refused:
                    self.assertNotEqual(result.returncode, 0, output)
                    self.assertIn(f"{variable} is {value}", output)
                else:
                    self.assertEqual(result.returncode, 0, output)

    def test_input_backends_are_refused_where_their_api_does_not_exist(self):
        self.check_axis("INPUT", "xinput", "web", [])

    def test_audio_backends_are_refused_where_their_api_does_not_exist(self):
        self.check_axis("AUDIO", "xaudio2", "webaudio", ["null"])


if __name__ == "__main__":
    unittest.main()
