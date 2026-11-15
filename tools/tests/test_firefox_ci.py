"""Keep the software-rendered Firefox CI lane from silently shrinking."""
import pathlib
import unittest


class FirefoxCiTests(unittest.TestCase):
    def test_wasm_enables_software_and_runs_all_firefox_entries(self):
        root = pathlib.Path(__file__).resolve().parents[2]
        workflow = (root / ".github/workflows/ci.yml").read_text(encoding="utf-8")
        wasm = workflow.split("\n  wasm:", 1)[1].split("\n  safari:", 1)[0]
        self.assertIn("cmake --preset wasm-release -DLABRADOR_FIREFOX_SOFTWARE_WEBGL=ON", wasm)
        self.assertIn('ctest --preset wasm-release -R "^Firefox" --no-tests=error -V', wasm)
        self.assertNotIn('^FirefoxWebWindowTests$', wasm)


if __name__ == "__main__":
    unittest.main()
