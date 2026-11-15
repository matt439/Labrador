"""Keep Safari readback artifact and execution wiring paired."""
import pathlib
import unittest


class SafariPixelsCiTests(unittest.TestCase):
    def test_pixel_page_is_uploaded_and_run_by_real_safari(self):
        root = pathlib.Path(__file__).resolve().parents[2]
        workflow = (root / ".github/workflows/ci.yml").read_text(encoding="utf-8")
        wasm, safari = workflow.split("\n  safari:", 1)
        safari = safari.split("\n  wasm-null:", 1)[0]
        for extension in ("html", "js", "wasm"):
            self.assertIn("tests/render/RenderPixelTests." + extension, wasm)
        self.assertIn("name: wasm-pixel-page", wasm)
        self.assertIn("name: wasm-pixel-page", safari)
        self.assertIn('--test-page "$RUNNER_TEMP/pixel/RenderPixelTests.html"', safari)
        self.assertNotIn("--browser-name firefox", safari)
