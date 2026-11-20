"""Add a game-authored post shader to an assembled starter, without building it.

The ordinary consumer checks run first. This opt-in fixture then changes the
assembled copy alone, so the shipped starter keeps its original files and tools.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path


INCLUDE_ANCHOR = '#include "engine/app/application.h"'
INITIALIZE_ANCHOR = "\t\tapp.initialize();"
NATIVE_SMOKE_OPTIONS_ANCHOR = "\t\toptions.visible = !smoke_test;"
NATIVE_SMOKE_OPTIONS = """
		if (smoke_test)
		{
			// Bound the post work and present immediately on software runners.
			options.client_size = Vector2I{ 640, 360 };
			options.vsync = false;
			std::fputs("material smoke: starting 640x360 device setup\\n", stderr);
		}
"""
NATIVE_SMOKE_INITIALIZED = """
		if (smoke_test)
		{
			std::fputs("material smoke: application initialized\\n", stderr);
		}
"""
NATIVE_SMOKE_STATE_ANCHOR = "\t\t\tfirst_state = std::make_unique<SmokeTest>(&app, std::move(first_state));"
NATIVE_SMOKE_STATE_NOTE = '\t\t\tstd::fputs("material smoke: material selected, entering frame loop\\n", stderr);'
MATERIAL_INCLUDE = '#include "materials/consumer_material.h"'
SETUP = """
		Renderer& material_renderer = *app.renderer();
		const MaterialHandle material =
			material_renderer.add_material(consumer_material);
		material_renderer.set_post_material(material);
		PostValues material_values;
		material_values.values[0] = 1.0f;
		material_renderer.set_post_values(material, material_values);
"""
SHADER = """// The game owns this function; the engine supplies its frame and entry point.
float4 material(MaterialInput input)
{
	return scene_point(input.uv) * material_value(0).x;
}
"""
CMAKE = """
# The consumer's own shader, added after game/ has declared MyGame.
labrador_add_material(MyGame consumer_material
    SOURCE game/shaders/consumer_material.hlsl)
"""
INVALID_SHADER = b"float4 material(MaterialInput input)\n{\n\treturn undeclared_material_function(input.uv);\n}\n"
DIAGNOSTIC_LOCATION = re.compile(r"consumer_material\.hlsl(?:\(\d+,\d+(?:-\d+)?\)|:\d+:\d+:?)")


def prepare(project: Path, host_tools: bool = False) -> None:
    """Validate every edit before writing the fixture to the assembled project."""
    root_cmake = project / "CMakeLists.txt"
    cmake = root_cmake.read_text(encoding="utf-8")
    if cmake.count("add_subdirectory(game)") != 1:
        raise ValueError("The project must declare game/ once; assemble a fresh starter first.")
    if "labrador_add_material(MyGame consumer_material" in cmake:
        raise ValueError("This project already has the material fixture; use a fresh starter.")

    edits: dict[Path, str] = {root_cmake: cmake + CMAKE}
    for entry in ("main.cpp", "main_web.cpp"):
        path = project / "game" / entry
        text = path.read_text(encoding="utf-8")
        if text.count(INCLUDE_ANCHOR) != 1 or text.count(INITIALIZE_ANCHOR) != 1:
            raise ValueError(f"{path} needs one application include and one app.initialize(); update the material fixture with the starter.")
        setup = SETUP
        if entry == "main.cpp":
            if (text.count(NATIVE_SMOKE_OPTIONS_ANCHOR) != 1 or
                    text.count(NATIVE_SMOKE_STATE_ANCHOR) != 1):
                raise ValueError(f"{path} needs the native smoke options and state; update the material fixture with the starter.")
            text = text.replace(NATIVE_SMOKE_OPTIONS_ANCHOR,
                                NATIVE_SMOKE_OPTIONS_ANCHOR + "\n" + NATIVE_SMOKE_OPTIONS.rstrip("\n"))
            text = text.replace(NATIVE_SMOKE_STATE_ANCHOR,
                                NATIVE_SMOKE_STATE_NOTE + "\n" + NATIVE_SMOKE_STATE_ANCHOR)
            setup = NATIVE_SMOKE_INITIALIZED + SETUP
        edits[path] = text.replace(INCLUDE_ANCHOR,
                                   INCLUDE_ANCHOR + "\n" + MATERIAL_INCLUDE).replace(
            INITIALIZE_ANCHOR, INITIALIZE_ANCHOR + "\n" + setup.rstrip("\n"))

    shader = project / "game/shaders/consumer_material.hlsl"
    if shader.exists():
        raise ValueError(f"{shader} already exists; use a fresh starter.")
    edits[shader] = SHADER

    if host_tools:
        manifest_path = project / "vcpkg.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        engine_manifest_path = project / "external/labrador/vcpkg.json"
        engine_manifest = json.loads(engine_manifest_path.read_text(encoding="utf-8"))
        try:
            dependencies = engine_manifest["features"]["materials"]["dependencies"]
        except KeyError as error:
            raise ValueError("The embedded engine manifest has no materials feature.") from error
        manifest["dependencies"].extend(dependencies)
        edits[manifest_path] = json.dumps(manifest, indent=2) + "\n"

    for path, text in edits.items():
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8", newline="\n")


def check_invalid_shader(project: Path, build: Path, run=subprocess.run) -> None:
    """Require a source-located build error, then restore and rebuild the shader."""
    shader = project / "game/shaders/consumer_material.hlsl"
    original = shader.read_bytes()
    build = build if build.is_absolute() else project / build
    command = ["cmake", "--build", str(build), "--target", "labrador_material_consumer_material"]

    def compile_material():
        return run(command, cwd=project, stdout=subprocess.PIPE,
                   stderr=subprocess.STDOUT, text=True, encoding="utf-8",
                   errors="replace", check=False, timeout=120)

    try:
        shader.write_bytes(INVALID_SHADER)
        rejected = compile_material()
        print(rejected.stdout)
        if rejected.returncode == 0:
            raise ValueError("The malformed game shader unexpectedly built successfully.")
        if not DIAGNOSTIC_LOCATION.search(rejected.stdout):
            raise ValueError("The malformed shader failed without naming consumer_material.hlsl and its line and column.")
    finally:
        shader.write_bytes(original)
        restored = compile_material()
        if restored.returncode != 0:
            print(restored.stdout)
            raise ValueError("The original game shader was restored, but rebuilding it failed.")
    print("Malformed game shader failed with its source location; the original shader was restored and rebuilt.")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("project", type=Path, help="an assembled MyGame starter whose baseline checks have passed")
    parser.add_argument("--host-tools", action="store_true", help="add the engine's host shader-tool dependencies for GL/WebGL2")
    parser.add_argument("--check-invalid-shader", metavar="BUILD_DIR", type=Path,
                        help="check compiler diagnostics in an existing configured fixture, then restore and rebuild it")
    arguments = parser.parse_args()
    if arguments.check_invalid_shader:
        if arguments.host_tools:
            parser.error("--host-tools prepares a fixture and cannot be combined with --check-invalid-shader")
        check_invalid_shader(arguments.project.resolve(), arguments.check_invalid_shader)
    else:
        prepare(arguments.project.resolve(), arguments.host_tools)
        print("Added consumer_material to both game entry points; configure, build and run the game to check it.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(f"material fixture: {error}", file=sys.stderr)
        sys.exit(2)
