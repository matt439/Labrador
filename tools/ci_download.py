"""Download a CI input with bounded retries for selected HTTP responses.

PowerShell's MaximumRetryCount also retries authentication failures. This
helper keeps the HTTP status policy explicit; SHA validation and installation
belong to its caller. The error notes used below require Python 3.11 or newer.
https://learn.microsoft.com/powershell/module/microsoft.powershell.utility/invoke-webrequest#-maximumretrycount
"""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
from email.utils import parsedate_to_datetime
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile
import time
import traceback
from urllib.error import HTTPError
from urllib.request import urlopen


RETRYABLE_HTTP_CODES = frozenset({408, 429, 500, 502, 503, 504})
MAX_ATTEMPTS = 3
REQUEST_TIMEOUT_SECONDS = 60
MAX_RETRY_AFTER_SECONDS = 30


def retry_delay(error: HTTPError, attempt: int) -> float | None:
    """Return the permitted wait, or None when the server asks for too long."""
    fallback = float(attempt)
    value = error.headers.get("Retry-After") if error.headers is not None else None
    if value is None:
        return fallback
    value = value.strip()
    if re.fullmatch(r"[0-9]+", value):
        digits = value.lstrip("0") or "0"
        # Avoid parsing an unbounded integer just to compare it with 30.
        if len(digits) > 2:
            return None
        delay = float(int(digits))
    else:
        try:
            target = parsedate_to_datetime(value)
            if target.tzinfo is None:
                return fallback
            delay = max(0.0, (target - datetime.now(timezone.utc)).total_seconds())
        except (ValueError, TypeError, OverflowError):
            return fallback
    return delay if delay <= MAX_RETRY_AFTER_SECONDS else None


def discard_temporary(path: Path, cause: Exception) -> None:
    try:
        path.unlink()
    except FileNotFoundError:
        pass
    except Exception as cleanup_error:
        # Keep the original failure and expose a secondary cleanup denial too.
        cause.add_note(f"Could not remove temporary download '{path}': {cleanup_error!r}")


def save_response(response, output: Path) -> None:
    temporary = None
    try:
        with response:
            with tempfile.NamedTemporaryFile(
                    mode="wb", dir=output.parent, prefix=f".{output.name}.",
                    suffix=".part", delete=False) as stream:
                temporary = Path(stream.name)
                shutil.copyfileobj(response, stream)
                length = response.headers.get("Content-Length")
                if length is not None and stream.tell() != int(length):
                    raise OSError(
                        f"Incomplete download: received {stream.tell()} bytes, expected {length}")
        # Replace only after both streams closed successfully, on the same
        # filesystem. A failed attempt never truncates an existing output.
        os.replace(temporary, output)
    except Exception as error:
        if temporary is not None:
            discard_temporary(temporary, error)
        raise


def download(url: str, output: Path) -> None:
    for attempt in range(1, MAX_ATTEMPTS + 1):
        try:
            response = urlopen(url, timeout=REQUEST_TIMEOUT_SECONDS)
        except HTTPError as error:
            # Close even the response we will rethrow. A close failure also
            # propagates immediately, with this HTTP error as its context.
            error.close()
            if error.code not in RETRYABLE_HTTP_CODES or attempt == MAX_ATTEMPTS:
                raise
            delay = retry_delay(error, attempt)
            if delay is None:
                error.add_note(
                    f"Retry-After exceeds the {MAX_RETRY_AFTER_SECONDS}-second permitted wait; stopping")
                raise
            print(f"HTTP {error.code}: retrying download after {delay:g}s "
                  f"(next attempt {attempt + 1}/{MAX_ATTEMPTS})", file=sys.stderr)
            time.sleep(delay)
            continue
        # Transport, certificate, socket and filesystem failures are outside
        # the retry handler, including failures while streaming a response.
        save_response(response, output)
        return


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", required=True)
    parser.add_argument("--output", required=True, type=Path)
    arguments = parser.parse_args(arguments)
    try:
        download(arguments.url, arguments.output)
    except Exception as error:
        print(f"Download '{arguments.url}' to '{arguments.output}' failed:", file=sys.stderr)
        # Include chained causes and secondary cleanup notes in the CI log.
        traceback.print_exception(error)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
