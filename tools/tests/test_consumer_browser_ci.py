"""Exercise the optional copied browser workflow's real packaging command."""

from __future__ import annotations

import hashlib
import json
import re
import shlex
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from tools.tests.tool_fixtures import load_tool

REPOSITORY = Path(__file__).resolve().parents[2]
WORKFLOW = REPOSITORY / "website/src/walkthrough/browser-ci.yml"
assemble = load_tool("tools/consumer_check/assemble.py")


def package_command(text: str) -> list[str]:
    block = text.split("      - name: Package the browser runtime and notices\n", 1)[1]
    block = block.split("\n      - name:", 1)[0]
    command = block[block.index('"%EMSDK_NODE%"'):].strip()
    command = command.removesuffix(" || exit /b 1")
    command = command.replace("^\n", " ")
    command = command.replace("%engine%", "external/labrador")
    command = command.replace("%emscripten%", "fixture-sdk/upstream/emscripten")
    return shlex.split(command)


class WorkflowContractTests(unittest.TestCase):
    def test_opt_in_workflow_has_no_publication_permissions_or_step(self):
        text = WORKFLOW.read_text(encoding="utf-8")
        self.assertRegex(text, r"(?m)^permissions:\n  contents: read\n\n")
        self.assertEqual(text.count("permissions:"), 1)
        for forbidden in ("secrets.", "pages:", "id-token:", "contents: write", "git push",
                          "deploy-pages", "configure-pages", "upload-pages-artifact"):
            with self.subTest(forbidden=forbidden):
                self.assertNotIn(forbidden, text)
        self.assertNotIn("browser-ci.yml", assemble.PLACED)
        native = (REPOSITORY / "website/src/walkthrough/ci.yml").read_text(encoding="utf-8")
        self.assertNotIn("wasm", native)

    def test_artifact_is_the_checked_package_and_missing_files_fail(self):
        text = WORKFLOW.read_text(encoding="utf-8")
        command = package_command(text)
        output = command[command.index("--out") + 1]
        self.assertEqual(output, "out/web-release")
        self.assertIn(f"--page {output}/index.html --quit-keys Escape,Enter --timeout 60", text)
        self.assertIn(f"          path: {output}/\n          if-no-files-found: error\n", text)
        checked = text.split("      - name: Draw and quit the packaged page\n", 1)[1]
        checked = checked.split("\n      - name:", 1)[0]
        self.assertNotIn("continue-on-error", checked)
        self.assertNotIn("if: always()", text)
        self.assertNotIn("--verify", text)
        page = REPOSITORY / "tests/render/web/run_page.mjs"
        self.assertTrue(page.is_file())
        pin = re.search(r"(?m)^  EMSDK_VERSION: (\S+)$", text).group(1)
        engine_ci = (REPOSITORY / ".github/workflows/ci.yml").read_text(encoding="utf-8")
        self.assertIn(f"  EMSDK_VERSION: {pin}\n", engine_ci)


@unittest.skipUnless(shutil.which("node"), "Node runs the browser bundler")
class BrowserPackageTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="labrador-browser-ci-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.command = package_command(WORKFLOW.read_text(encoding="utf-8"))
        self.command[0] = shutil.which("node")
        tool = self.root / self.command[1]
        tool.parent.mkdir(parents=True)
        shutil.copyfile(REPOSITORY / "tools/web_bundle/bundle.mjs", tool)
        self.sources: list[Path] = []
        self.runtime: dict[str, bytes] = {}
        self.licences: list[str] = []
        for index in range(2, len(self.command), 2):
            flag, value = self.command[index:index + 2]
            if flag in {"--file", "--licence"}:
                name, filename = value.split("=", 1)
                content = f"fixture {name}\n".encode()
                if flag == "--file":
                    self.runtime[name] = content
                else:
                    self.licences.append(name)
            elif flag == "--notice":
                filename = value
                content = b"fixture notice preamble\n"
            else:
                continue
            path = self.root / filename
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(content)
            self.sources.append(path)
        version = self.root / "fixture-sdk/upstream/emscripten/emscripten-version.txt"
        version.write_text("6.0.12\n", encoding="utf-8")
        self.sources.append(version)

    def run_package(self):
        return subprocess.run(self.command, cwd=self.root, capture_output=True, text=True, timeout=20)

    def test_runtime_names_hashes_and_full_notices_survive_packaging(self):
        result = self.run_package()
        self.assertEqual(result.returncode, 0, result.stderr)
        output = self.root / "out/web-release"
        self.assertEqual(sorted(path.name for path in output.iterdir()),
                         ["BUILD.json", "MyGame.js", "MyGame.wasm", "NOTICE.txt", "index.html"])
        build = json.loads((output / "BUILD.json").read_text(encoding="utf-8"))
        self.assertEqual(build["build_type"], "Release")
        self.assertEqual(build["emscripten"], "6.0.12")
        for name, content in self.runtime.items():
            self.assertEqual((output / name).read_bytes(), content)
            self.assertEqual(build["files"][name]["sha256"], hashlib.sha256(content).hexdigest())
        notices = (output / "NOTICE.txt").read_text(encoding="utf-8")
        for name in self.licences:
            self.assertIn(f"fixture {name}\n", notices)

    def test_each_missing_runtime_notice_or_sdk_input_refuses_a_complete_package(self):
        for source in self.sources:
            with self.subTest(source=source.relative_to(self.root)):
                self.assertEqual(self.run_package().returncode, 0)
                content = source.read_bytes()
                source.unlink()
                try:
                    # The bundler empties its dedicated output each time. Failure must
                    # not leave an earlier success record for an upload to mistake.
                    result = self.run_package()
                    self.assertNotEqual(result.returncode, 0)
                    self.assertFalse((self.root / "out/web-release/BUILD.json").exists())
                finally:
                    source.write_bytes(content)


if __name__ == "__main__":
    unittest.main()
