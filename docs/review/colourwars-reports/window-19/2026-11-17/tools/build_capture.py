"""Link an out-only measurement entry point to an already-built native preset."""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("preset", choices=["x64-debug", "x64-debug-d3d12", "x64-debug-gl", "x64-debug-vulkan"])
    parser.add_argument("--root", type=Path, default=Path(r"C:\Projects\Labrador"))
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    output = here / args.preset
    output.mkdir(exist_ok=True)
    build = args.root / "out" / "build" / args.preset
    ninja = (build / "build.ninja").read_text(encoding="utf-8")
    section = re.search(r"^build samples\\minimal\\MinimalSample.exe:.*?(?=\n\n)", ninja, re.MULTILINE | re.DOTALL).group(0)
    libraries = re.search(r"^  LINK_LIBRARIES = (.+)$", section, re.MULTILINE).group(1)
    libraries = [token.strip('"').replace("$:", ":").replace("$ ", " ") for token in re.findall(r'"[^"]*"|\S+', libraries)]
    libraries = [str(build / token) if "\\" in token and not Path(token).is_absolute() else token for token in libraries]
    inputs = [
        here / "capture_main.cpp",
        args.root / "samples/minimal/states/hello_state.cpp",
        args.root / "samples/minimal/states/confirm_state.cpp",
        args.root / "tools/linesweeper_capture/png_file.cpp",
    ]
    commands = []
    objects = []
    for source in inputs:
        object_path = output / (source.stem + ".obj")
        command = ["cl.exe", "/nologo", "/c", "/std:c++20", "/MDd", "/EHsc", "/Zi", "/Od", "/W4", "/WX", "/permissive-", "/utf-8", "/fp:precise", "/DUNICODE", "/D_UNICODE", "/DWIN32", "/D_WINDOWS", "/DNOMINMAX", "/DWIN32_LEAN_AND_MEAN", "/DCAPTURE_FULLSCREEN=0", "/I" + str(args.root), "/Fo" + str(object_path), "/Fd" + str(output / "compile.pdb"), str(source)]
        commands.append(command)
        subprocess.run(command, check=True)
        objects.append(str(object_path))
    executable = output / "RevealCaptureWindowed.exe"
    command = ["link.exe", "/nologo", "/DEBUG", "/INCREMENTAL:NO", "/SUBSYSTEM:WINDOWS", "/MANIFEST:EMBED", "/MANIFESTINPUT:" + str(args.root / "samples/minimal/dpi_aware.manifest"), "/OUT:" + str(executable), "/PDB:" + str(output / "capture.pdb"), *objects, *libraries, "windowscodecs.lib", "dwmapi.lib"]
    commands.append(command)
    subprocess.run(command, check=True)
    fullscreen_object = str(output / "capture_main_fullscreen.obj")
    fullscreen_compile = [argument.replace("/DCAPTURE_FULLSCREEN=0", "/DCAPTURE_FULLSCREEN=1").replace("/Fo" + objects[0], "/Fo" + fullscreen_object) for argument in commands[0]]
    commands.append(fullscreen_compile)
    subprocess.run(fullscreen_compile, check=True)
    fullscreen_executable = output / "RevealCaptureFullscreen.exe"
    fullscreen_link = [argument.replace("/OUT:" + str(executable), "/OUT:" + str(fullscreen_executable)).replace("/PDB:" + str(output / "capture.pdb"), "/PDB:" + str(output / "capture-fullscreen.pdb")) for argument in command]
    fullscreen_link = [fullscreen_object if argument == objects[0] else argument for argument in fullscreen_link]
    commands.append(fullscreen_link)
    subprocess.run(fullscreen_link, check=True)
    content = build / "samples/minimal"
    shutil.copy2(content / "manifest.json", output / "manifest.json")
    shutil.copytree(content / "fonts", output / "fonts", dirs_exist_ok=True)
    for dll in content.glob("*.dll"):
        shutil.copy2(dll, output / dll.name)
    hashes = {str(path): sha256(path) for path in inputs}
    for library in libraries:
        path = Path(library)
        if path.is_absolute():
            hashes[str(path)] = sha256(path)
    hashes[str(executable)] = sha256(executable)
    hashes[str(fullscreen_executable)] = sha256(fullscreen_executable)
    head = subprocess.check_output(["git", "-C", str(args.root), "rev-parse", "HEAD"], text=True).strip()
    (output / "build-receipt.json").write_text(json.dumps({
        "created_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "preset": args.preset,
        "root_head_at_link": head,
        "commands": commands,
        "sha256": hashes,
        "limitations": ["Instrumented sample entry point; engine library linked unchanged.", "client-redraw.png is an engine framebuffer redraw, not displayed scan-out.", "CUA window snapshots are point-in-time and do not rule out sub-snapshot compositor flashes."]
    }, indent=2), encoding="utf-8")
    print(executable)


if __name__ == "__main__":
    main()
