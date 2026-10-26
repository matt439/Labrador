"""Exercise the actual downloader with fake responses, without network access."""

from __future__ import annotations

from datetime import datetime, timedelta, timezone
from email.message import Message
from email.utils import format_datetime
import io
from pathlib import Path
import re
import socket
import ssl
import tempfile
import unittest
from unittest.mock import call, patch
from urllib.error import HTTPError, URLError

from tools import ci_download


URL = "https://example.invalid/pinned-input.exe"


class Response(io.BytesIO):
    def __init__(self, data: bytes, *, length: int | None = None):
        super().__init__(data)
        self.headers = Message()
        self.headers["Content-Length"] = str(len(data) if length is None else length)


class FailingResponse(Response):
    def __init__(self, failure: Exception):
        super().__init__(b"partial", length=100)
        self.failure = failure
        self.reads = 0

    def read(self, size=-1):
        self.reads += 1
        if self.reads == 1:
            return b"partial"
        raise self.failure


def http_error(status: int, retry_after: str | None = None) -> HTTPError:
    headers = Message()
    if retry_after is not None:
        headers["Retry-After"] = retry_after
    return HTTPError(URL, status, "synthetic response", headers, io.BytesIO(b"error"))


class CiDownloadTests(unittest.TestCase):
    def test_sdk_workflow_checks_download_exit_then_digest_before_installing(self):
        workflow = (Path(__file__).resolve().parents[2] /
                    ".github/workflows/ci.yml").read_text(encoding="utf-8")
        match = re.search(
            r"(?ms)^      - name: Install the Vulkan SDK\n.*?(?=^      - name:|\Z)",
            workflow)
        self.assertIsNotNone(match)
        step = match.group()
        download = re.search(
            r'(?m)^\s*python tools/ci_download\.py --url "\$uri" --output "\$installer"\s*$',
            step)
        guard = re.search(r"(?ms)^\s*if \(\$LASTEXITCODE -ne 0\) \{\s*throw [^\n]+\n\s*\}", step)
        digest = re.search(r"(?m)^\s*if \(\(Get-FileHash \$installer -Algorithm SHA256\)", step)
        install = re.search(r"(?m)^\s*\$process = Start-Process -FilePath \$installer", step)
        for operation in (download, guard, digest, install):
            self.assertIsNotNone(operation)
        self.assertLess(download.start(), guard.start())
        self.assertLess(guard.end(), digest.start())
        self.assertLess(digest.start(), install.start())
        self.assertIn("$env:VULKAN_SDK_VERSION", step)
        self.assertIn("$env:VULKAN_SDK_SHA256", step)
        self.assertNotRegex(step, r"(?m)^\s*(?:for|foreach|while|do|catch)\b")
        self.urlopen.assert_not_called()
        self.sleep.assert_not_called()

    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="labrador-ci-download-")
        self.addCleanup(temporary.cleanup)
        self.directory = Path(temporary.name)
        self.output = self.directory / "input.exe"
        self.output.write_bytes(b"existing input")
        # Every test replaces the only network entry point before invoking
        # production code. An unspecified fake fails rather than networking.
        opener = patch.object(ci_download, "urlopen", side_effect=AssertionError("Unexpected request"))
        sleeper = patch.object(ci_download.time, "sleep")
        self.urlopen = opener.start()
        self.sleep = sleeper.start()
        self.addCleanup(opener.stop)
        self.addCleanup(sleeper.stop)

    def assert_preserved(self):
        self.assertEqual(self.output.read_bytes(), b"existing input")
        self.assertEqual(list(self.directory.iterdir()), [self.output])

    def test_success_replaces_existing_destination_after_transfer(self):
        response = Response(b"complete input")
        self.urlopen.side_effect = [response]
        ci_download.download(URL, self.output)
        self.assertEqual(self.output.read_bytes(), b"complete input")
        self.assertTrue(response.closed)
        self.assertEqual(list(self.directory.iterdir()), [self.output])
        self.urlopen.assert_called_once_with(URL, timeout=60)
        self.sleep.assert_not_called()

    def test_each_allowed_status_can_retry_and_succeed(self):
        for status in (408, 429, 500, 502, 503, 504):
            with self.subTest(status=status):
                self.urlopen.reset_mock()
                self.sleep.reset_mock()
                error = http_error(status)
                response = Response(b"complete input")
                self.urlopen.side_effect = [error, response]
                ci_download.download(URL, self.output)
                self.assertEqual(self.urlopen.call_count, 2)
                self.sleep.assert_called_once_with(1.0)
                self.assertEqual(self.output.read_bytes(), b"complete input")

    def test_authentication_and_other_statuses_stop_immediately(self):
        for status in (304, 400, 401, 403, 404, 407, 418, 422, 451, 501, 505):
            with self.subTest(status=status):
                self.urlopen.reset_mock()
                self.sleep.reset_mock()
                error = http_error(status)
                self.urlopen.side_effect = [error, Response(b"must not be used")]
                with self.assertRaises(HTTPError) as raised:
                    ci_download.download(URL, self.output)
                self.assertIs(raised.exception, error)
                self.assertEqual(self.urlopen.call_count, 1)
                self.sleep.assert_not_called()
                self.assert_preserved()

    def test_authentication_failure_after_transient_error_stops(self):
        denied = http_error(403)
        self.urlopen.side_effect = [http_error(503), denied, Response(b"must not be used")]
        with self.assertRaises(HTTPError) as raised:
            ci_download.download(URL, self.output)
        self.assertIs(raised.exception, denied)
        self.assertEqual(self.urlopen.call_count, 2)
        self.sleep.assert_called_once_with(1.0)
        self.assert_preserved()

    def test_exhaustion_preserves_the_third_http_error(self):
        errors = [http_error(503) for _ in range(3)]
        self.urlopen.side_effect = errors + [Response(b"must not be used")]
        with self.assertRaises(HTTPError) as raised:
            ci_download.download(URL, self.output)
        self.assertIs(raised.exception, errors[-1])
        self.assertEqual(self.urlopen.call_count, 3)
        self.assertEqual(self.sleep.call_args_list, [call(1.0), call(2.0)])
        self.assert_preserved()

    def test_transport_certificate_socket_and_access_errors_do_not_retry(self):
        failures = [URLError("network failure"), URLError(PermissionError("socket denied")),
                    socket.timeout("timed out"), ssl.SSLError("certificate failure"),
                    PermissionError("access denied"), OSError("socket failure")]
        for error in failures:
            with self.subTest(error=repr(error)):
                self.urlopen.reset_mock()
                self.sleep.reset_mock()
                self.urlopen.side_effect = [error, Response(b"must not be used")]
                with self.assertRaises(type(error)) as raised:
                    ci_download.download(URL, self.output)
                self.assertIs(raised.exception, error)
                self.assertEqual(self.urlopen.call_count, 1)
                self.sleep.assert_not_called()
                self.assert_preserved()

    def test_retry_after_seconds_and_dates_are_respected(self):
        now = datetime(2026, 10, 26, 12, 0, 0, tzinfo=timezone.utc)
        cases = [("5", 5.0), ("0", 0.0), ("30", 30.0),
                 (format_datetime(now + timedelta(seconds=10), usegmt=True), 10.0),
                 (format_datetime(now - timedelta(seconds=10), usegmt=True), 0.0)]
        for header, expected in cases:
            with self.subTest(header=header), patch.object(ci_download, "datetime") as clock:
                clock.now.return_value = now
                self.urlopen.reset_mock()
                self.sleep.reset_mock()
                self.urlopen.side_effect = [http_error(429, header), Response(b"complete input")]
                ci_download.download(URL, self.output)
                self.assertEqual(self.urlopen.call_count, 2)
                self.sleep.assert_called_once_with(expected)

    def test_excessive_retry_after_stops_instead_of_retrying_early(self):
        now = datetime(2026, 10, 26, 12, 0, 0, tzinfo=timezone.utc)
        for header in ("31", "9" * 5000,
                       format_datetime(now + timedelta(seconds=31), usegmt=True)):
            with self.subTest(header=header[:60]), patch.object(ci_download, "datetime") as clock:
                clock.now.return_value = now
                self.urlopen.reset_mock()
                self.sleep.reset_mock()
                error = http_error(503, header)
                self.urlopen.side_effect = [error, Response(b"must not be used")]
                with self.assertRaises(HTTPError) as raised:
                    ci_download.download(URL, self.output)
                self.assertIs(raised.exception, error)
                self.assertIn("permitted wait", " ".join(error.__notes__))
                self.assertEqual(self.urlopen.call_count, 1)
                self.sleep.assert_not_called()
                self.assert_preserved()

    def test_invalid_retry_after_uses_bounded_backoff(self):
        self.urlopen.side_effect = [http_error(503, "not a date"),
                                   http_error(429, "-1"), Response(b"complete input")]
        ci_download.download(URL, self.output)
        self.assertEqual(self.urlopen.call_count, 3)
        self.assertEqual(self.sleep.call_args_list, [call(1.0), call(2.0)])

    def test_destination_creation_denial_is_not_retried(self):
        error = PermissionError("destination denied")
        response = Response(b"complete input")
        self.urlopen.side_effect = [response]
        with patch.object(ci_download.tempfile, "NamedTemporaryFile", side_effect=error):
            with self.assertRaises(PermissionError) as raised:
                ci_download.download(URL, self.output)
        self.assertIs(raised.exception, error)
        self.assertTrue(response.closed)
        self.assertEqual(self.urlopen.call_count, 1)
        self.sleep.assert_not_called()
        self.assert_preserved()

    def test_partial_response_failure_removes_owned_temporary_file(self):
        error = OSError("body read failed")
        response = FailingResponse(error)
        self.urlopen.side_effect = [response]
        with self.assertRaises(OSError) as raised:
            ci_download.download(URL, self.output)
        self.assertIs(raised.exception, error)
        self.assertTrue(response.closed)
        self.assertEqual(self.urlopen.call_count, 1)
        self.sleep.assert_not_called()
        self.assert_preserved()

    def test_short_declared_body_is_not_published_or_retried(self):
        self.urlopen.side_effect = [Response(b"short", length=100)]
        with self.assertRaisesRegex(OSError, "Incomplete download"):
            ci_download.download(URL, self.output)
        self.assertEqual(self.urlopen.call_count, 1)
        self.sleep.assert_not_called()
        self.assert_preserved()

    def test_replace_denial_removes_temporary_and_preserves_destination(self):
        error = PermissionError("replace denied")
        self.urlopen.side_effect = [Response(b"complete input")]
        with patch.object(ci_download.os, "replace", side_effect=error):
            with self.assertRaises(PermissionError) as raised:
                ci_download.download(URL, self.output)
        self.assertIs(raised.exception, error)
        self.assertEqual(self.urlopen.call_count, 1)
        self.sleep.assert_not_called()
        self.assert_preserved()

    def test_cleanup_denial_keeps_original_cause_and_reports_secondary_failure(self):
        error = OSError("body read failed")
        self.urlopen.side_effect = [FailingResponse(error)]
        with patch.object(Path, "unlink", side_effect=PermissionError("cleanup denied")):
            with self.assertRaises(OSError) as raised:
                ci_download.download(URL, self.output)
        self.assertIs(raised.exception, error)
        notes = " ".join(error.__notes__)
        self.assertIn("cleanup denied", notes)
        self.assertIn(".part", notes)
        self.assertEqual(self.output.read_bytes(), b"existing input")
        self.assertEqual(len(list(self.directory.glob("*.part"))), 1)
        self.assertEqual(self.urlopen.call_count, 1)
        self.sleep.assert_not_called()

    def test_http_response_close_denial_stops_immediately(self):
        error = http_error(503)
        denied = PermissionError("response close denied")
        self.urlopen.side_effect = [error, Response(b"must not be used")]
        with patch.object(error, "close", side_effect=denied):
            with self.assertRaises(PermissionError) as raised:
                ci_download.download(URL, self.output)
        error.close()
        self.assertIs(raised.exception, denied)
        self.assertEqual(self.urlopen.call_count, 1)
        self.sleep.assert_not_called()
        self.assert_preserved()

    def test_cli_reports_original_error_and_secondary_cleanup_note(self):
        error = OSError("original failure")
        error.add_note("secondary cleanup denied")
        with patch.object(ci_download, "download", side_effect=error), \
                patch.object(ci_download.sys, "stderr", new_callable=io.StringIO) as output:
            result = ci_download.main(["--url", URL, "--output", str(self.output)])
        self.assertEqual(result, 1)
        self.assertIn(URL, output.getvalue())
        self.assertIn(str(self.output), output.getvalue())
        self.assertIn("original failure", output.getvalue())
        self.assertIn("secondary cleanup denied", output.getvalue())
        self.urlopen.assert_not_called()


if __name__ == "__main__":
    unittest.main()
