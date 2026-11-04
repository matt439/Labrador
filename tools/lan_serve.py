"""Serve a built browser page to a phone on the same network.

    python tools/lan_serve.py <folder> [--port 8443] [--cert cert.pem --key key.pem]

Binds every interface, so a phone on the same Wi-Fi reaches the page at this
machine's LAN address, and prints those addresses. With --cert and --key it
serves HTTPS, which a page needs for a secure context on a LAN address; without
them it serves plain HTTP. docs/port/lan-serve.md is the procedure, including
how to make a certificate the phone trusts.

Every response is sent with Cache-Control: no-store, so a rebuilt page is
fetched again on the next reload, and .wasm as application/wasm. Python's own
modules only. It is a development server for a network you trust: it serves
the folder's files to anyone who can reach the port, and nothing else.
"""

from __future__ import annotations

import argparse
import functools
import http.server
import socket
import ssl
import sys
from pathlib import Path

TYPES = {
    ".wasm": "application/wasm",
    ".js": "text/javascript",
    ".mjs": "text/javascript",
    ".html": "text/html",
    ".json": "application/json",
    ".data": "application/octet-stream",
}


class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {**http.server.SimpleHTTPRequestHandler.extensions_map, **TYPES}

    def end_headers(self) -> None:
        self.send_header("Cache-Control", "no-store")
        super().end_headers()


def lan_addresses() -> list[str]:
    """This machine's IPv4 addresses other than loopback, best effort."""
    found: set[str] = set()
    try:
        # No packet is sent: connecting a UDP socket only picks the route.
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
            probe.connect(("192.0.2.1", 9))
            found.add(probe.getsockname()[0])
    except OSError:
        pass
    try:
        for info in socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET):
            found.add(info[4][0])
    except OSError:
        pass
    return sorted(address for address in found if not address.startswith("127."))


def make_server(folder: Path, port: int, cert: Path | None, key: Path | None,
                host: str = "0.0.0.0") -> http.server.ThreadingHTTPServer:
    handler = functools.partial(Handler, directory=str(folder))
    server = http.server.ThreadingHTTPServer((host, port), handler)
    if cert is not None:
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(certfile=str(cert), keyfile=str(key))
        server.socket = context.wrap_socket(server.socket, server_side=True)
    return server


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Serve a built page to a phone on the LAN.")
    parser.add_argument("folder", type=Path, help="the folder holding the page")
    parser.add_argument("--port", type=int, default=None,
                        help="the port; 8443 with a certificate, 8000 without")
    parser.add_argument("--cert", type=Path, help="a PEM certificate for HTTPS")
    parser.add_argument("--key", type=Path, help="the certificate's PEM private key")
    options = parser.parse_args(argv)
    if (options.cert is None) != (options.key is None):
        parser.error("--cert and --key go together")
    if not options.folder.is_dir():
        parser.error(f"{options.folder} is not a folder")
    port = options.port if options.port is not None else (8443 if options.cert else 8000)
    scheme = "https" if options.cert else "http"
    server = make_server(options.folder, port, options.cert, options.key)
    pages = sorted(path.name for path in options.folder.glob("*.html"))
    print(f"lan_serve: serving {options.folder.resolve()} on port {port}")
    for address in lan_addresses() or ["<this machine's LAN address>"]:
        for page in pages or [""]:
            print(f"  {scheme}://{address}:{port}/{page}")
    if scheme == "http":
        print("lan_serve: plain HTTP on a LAN address is not a secure context; "
              "see docs/port/lan-serve.md for what that withholds")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
