"""Check the native-null sanitizer's compile flags, static annotations and source-built DLL runtime."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys


def instrumentation(commands: list[dict], directives: str, imports: str, dependency_imports: str) -> dict:
    if not commands:
        raise ValueError("compile_commands.json contains no translation units")
    for entry in commands:
        command = entry["command"]
        if "/fsanitize=address" not in command or re.search(r"/RTC|_DISABLE_\w*ANNOTATION", command):
            raise ValueError(f"inconsistent sanitizer flags: {entry['file']}")
    for container in ("string", "vector", "optional"):
        if f"annotate_{container}=1" not in directives or f"annotate_{container}=0" in directives:
            raise ValueError(f"lodepng lacks consistent {container} annotations")
    if "/InferAsanLibs" not in directives:
        raise ValueError("lodepng lacks instrumented object directives")
    runtime = re.search(r"clang_rt\.asan(?:_dbg)?_dynamic-x86_64\.dll", imports, re.IGNORECASE)
    if runtime is None:
        raise ValueError("AudioTests imports no x64 ASan runtime")
    dependency_runtime = re.search(r"clang_rt\.asan(?:_dbg)?_dynamic-x86_64\.dll", dependency_imports, re.IGNORECASE)
    if dependency_runtime is None:
        raise ValueError("DirectXTK imports no x64 ASan runtime")
    return {"translation_units": len(commands), "compile_sanitizer": "/fsanitize=address",
            "lodepng_annotations": {name: 1 for name in ("string", "vector", "optional")},
            "audio_test_runtime": runtime.group(0), "directxtk_runtime": dependency_runtime.group(0)}


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", required=True, type=Path)
    options = parser.parse_args(arguments)
    build = options.build_dir.resolve()
    library = build / "vcpkg_installed/x64-windows-asan/debug/lib/lodepng.lib"
    audio = build / "tests/audio/AudioTests.exe"
    dependency = build / "vcpkg_installed/x64-windows-asan/debug/bin/DirectXTK.dll"
    try:
        commands = json.loads((build / "compile_commands.json").read_text(encoding="utf-8"))
        directives = subprocess.run(["dumpbin", "/directives", str(library)], check=True,
                                    capture_output=True, text=True).stdout
        imports = subprocess.run(["dumpbin", "/dependents", str(audio)], check=True,
                                 capture_output=True, text=True).stdout
        dependency_imports = subprocess.run(["dumpbin", "/dependents", str(dependency)], check=True,
                                           capture_output=True, text=True).stdout
        receipt = instrumentation(commands, directives, imports, dependency_imports)
        receipt["lodepng_sha256"] = hashlib.sha256(library.read_bytes()).hexdigest()
        receipt["directxtk_sha256"] = hashlib.sha256(dependency.read_bytes()).hexdigest()
        receipt["scope"] = "native-null MSVC Debug; dependencies not linked here are not qualified"
        print(json.dumps(receipt, indent=2))
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        print(f"asan_receipt: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
