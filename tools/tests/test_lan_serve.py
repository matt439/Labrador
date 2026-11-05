"""lan_serve.py on loopback: headers, types, HTTPS and its arguments."""

from __future__ import annotations

import contextlib
import io
from pathlib import Path
import shutil
import ssl
import subprocess
import tempfile
import threading
import unittest
import urllib.request

from tools.tests.tool_fixtures import load_tool

REPOSITORY = Path(__file__).resolve().parents[2]
lan_serve = load_tool("tools/lan_serve.py")


class ServeTests(unittest.TestCase):
    def setUp(self):
        self.folder = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.folder, True)
        (self.folder / "Page.html").write_text("<p>page</p>")
        (self.folder / "Page.wasm").write_bytes(b"\0asm\1\0\0\0")
        (self.folder / "Page.js").write_text("export {};")

    def serve(self, cert=None, key=None):
        server = lan_serve.make_server(self.folder, 0, cert, key, host="127.0.0.1")
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        self.addCleanup(server.server_close)
        self.addCleanup(server.shutdown)
        return server.server_address[1]

    def test_wasm_type_and_no_store_over_http(self):
        port = self.serve()
        with urllib.request.urlopen(f"http://127.0.0.1:{port}/Page.wasm") as response:
            self.assertEqual(response.headers["Content-Type"], "application/wasm")
            self.assertEqual(response.headers["Cache-Control"], "no-store")
            self.assertEqual(response.read(), b"\0asm\1\0\0\0")
        with urllib.request.urlopen(f"http://127.0.0.1:{port}/Page.js") as response:
            self.assertEqual(response.headers["Content-Type"], "text/javascript; charset=utf-8")

    @unittest.skipUnless(shutil.which("openssl"), "openssl makes the test certificate")
    def test_https_with_a_certificate(self):
        cert, key = self.folder / "cert.pem", self.folder / "key.pem"
        subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
                        "-subj", "/CN=127.0.0.1", "-keyout", str(key), "-out", str(cert)],
                       check=True, capture_output=True)
        port = self.serve(cert, key)
        context = ssl.create_default_context(cafile=str(cert))
        context.check_hostname = False
        with urllib.request.urlopen(f"https://127.0.0.1:{port}/Page.html", context=context) as response:
            self.assertEqual(response.read(), b"<p>page</p>")

    def test_cert_and_key_go_together(self):
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as raised:
            lan_serve.main([str(self.folder), "--cert", "cert.pem"])
        self.assertEqual(raised.exception.code, 2)

    def test_lan_addresses_leave_out_loopback(self):
        self.assertFalse(any(address.startswith("127.") for address in lan_serve.lan_addresses()))


if __name__ == "__main__":
    unittest.main()
