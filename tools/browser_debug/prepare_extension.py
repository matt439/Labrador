"""Download the qualified official DWARF bundle into an explicitly chosen folder."""

import argparse
import hashlib
import io
import json
from pathlib import Path
import urllib.request
import zipfile


extension_id = "pdcpmagijalfljmkmjngeonclgbbannb"
qualified_version = "0.2.5854.1"
qualified_sha256 = "f4c7d9951929acccb602e3cb0e73ece394ecafc1e2094419887d95bf54bd8fce"
download_url = (
    "https://clients2.google.com/service/update2/crx?response=redirect"
    "&prodversion=151.0.0.0&acceptformat=crx2,crx3"
    f"&x=id%3D{extension_id}%26uc"
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    options = parser.parse_args()
    output = options.out.resolve()
    if output.exists() and any(output.iterdir()):
        parser.error("--out must be absent or empty; existing files are not replaced")

    with urllib.request.urlopen(download_url, timeout=60) as response:
        data = response.read(32 * 1024 * 1024 + 1)
    digest = hashlib.sha256(data).hexdigest()
    if digest != qualified_sha256:
        raise SystemExit(
            "The official bundle differs from the qualified SHA-256. "
            "Re-qualify its version before updating this recipe; nothing was extracted."
        )

    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        manifest = json.loads(archive.read("manifest.json"))
        if manifest["version"] != qualified_version:
            raise SystemExit("Unexpected DWARF extension version; nothing was extracted")
        for member in archive.infolist():
            target = (output / member.filename).resolve()
            if not target.is_relative_to(output):
                raise SystemExit("Archive member leaves --out; nothing was extracted")
        output.mkdir(parents=True, exist_ok=True)
        archive.extractall(output)
    print(f"Prepared official C/C++ DevTools Support (DWARF) {qualified_version}")
    print(f"SHA-256: {digest}")
    print(f"Unpacked folder: {output}")
    print("No browser profile or system setting was changed.")


if __name__ == "__main__":
    main()
