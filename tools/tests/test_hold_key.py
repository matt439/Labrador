"""hold_key.ps1's window capture refuses a window it cannot read, without starting a game."""

from __future__ import annotations

import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[2] / "tools/input_drive/hold_key.ps1"


@unittest.skipUnless(sys.platform == "win32", "hold_key.ps1 drives Win32 windows")
class SaveClientTests(unittest.TestCase):
    def test_a_window_without_a_client_area_is_refused_by_name(self):
        powershell = shutil.which("powershell")
        if not powershell:
            self.skipTest("Windows PowerShell is required")
        source = SCRIPT.read_text(encoding="utf-8")
        # The script's own type definitions and Save-Client, and none of the
        # code that starts a game or sends a key.
        types = source[source.index("Add-Type -AssemblyName System.Drawing"):source.index("# Set-1 scan codes")]
        start = source.index("function Save-Client")
        function = source[start:source.index("\n}\n", start) + 3]
        with tempfile.TemporaryDirectory(prefix="labrador-hold-key-") as temporary:
            picture = Path(temporary) / "never.png"
            probe = Path(temporary) / "probe.ps1"
            probe.write_text(
                "$ErrorActionPreference = 'Stop'\n" + types + function +
                "try {\n"
                f"    Save-Client ([IntPtr]::Zero) '{picture}'\n"
                "    exit 3\n"
                "} catch {\n"
                "    Write-Output $_.Exception.Message\n"
                "    exit 0\n"
                "}\n", encoding="utf-8")
            result = subprocess.run([powershell, "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass",
                                     "-File", str(probe)], capture_output=True, timeout=120)
            output = (result.stdout + result.stderr).decode("utf-8", errors="replace")
            self.assertEqual(result.returncode, 0, output)
            self.assertIn("Could not read the game window's client area.", output)
            self.assertFalse(picture.exists())


if __name__ == "__main__":
    unittest.main()
