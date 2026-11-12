"""Plan render verification, or explicitly execute its first-failure sweep.

Default planning reads repository rules without tooling discovery. --preflight
explicitly adds read-only tooling-environment, tool-path and cache inspection.
--execute runs the modeled commands in the existing environment; see
docs/verification.md for runtime effects and separate acceptance obligations.
"""

from __future__ import annotations

import argparse
import copy
import json
import os
from pathlib import Path, PurePosixPath, PureWindowsPath
import posixpath
import re
import shutil
import sys

REPOSITORY = Path(__file__).resolve().parents[1]
NATIVE = ("x64-debug", "x64-debug-d3d12", "x64-debug-gl", "x64-debug-vulkan", "x64-debug-null")
NATIVE_PRESETS = (*NATIVE, *(name.replace("-debug", "-release") for name in NATIVE))
NATIVE_HOST_CONDITION = {"type": "equals", "lhs": "${hostSystemName}", "rhs": "Windows"}
# Each planned configuration and the render backend it selects, in sweep order.
RENDER_BACKENDS = {"x64-debug": "d3d11", "x64-debug-d3d12": "d3d12", "x64-debug-gl": "gl",
                   "x64-debug-vulkan": "vulkan", "x64-debug-null": "null",
                   "wasm-debug": "gl", "wasm-release-null": "null"}
CONFIGURATIONS = tuple(RENDER_BACKENDS)
BACKENDS = tuple(RENDER_BACKENDS.values())
D3D_PRESETS = tuple(name for name, render in RENDER_BACKENDS.items() if render in {"d3d11", "d3d12"})
WASM_CONFIGURATIONS = tuple(name for name in CONFIGURATIONS if name.startswith("wasm-"))
ENVIRONMENT_KEYS = ("PATH", "PATHEXT", "VCPKG_ROOT", "VULKAN_SDK", "EMSDK", "EMSDK_NODE", "MSYSTEM")
# A cmd started from Git Bash inherits MSYSTEM; emsdk_env.bat then prints bash
# exports instead of setting the environment, and emcc is not on PATH.
MSYSTEM_DETAIL = ("MSYSTEM={value!r} is inherited, so emsdk_env.bat leaves emcc off PATH; "
                  "run `set MSYSTEM=` before emsdk_env.bat")
SOURCE_FILES = ("CMakePresets.json", "engine/CMakeLists.txt", "cmake/compile_shaders.cmake",
                ".github/workflows/ci.yml", "CMakeLists.txt", "tests/ctest_inventory.json")
NULL_CONFIGURE = ["cmake", "--preset", "wasm-release", "-B", "out/build/wasm-release-null",
                  "-DLABRADOR_RENDER_BACKEND=null", "-DLABRADOR_AUDIO_BACKEND=null"]
VCPKG_TOOLCHAIN = "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
WASM_TOOLCHAIN = "$env{EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake"
CACHE_KEYS = {"CMAKE_HOME_DIRECTORY", "CMAKE_CACHEFILE_DIR", "CMAKE_GENERATOR", "CMAKE_BUILD_TYPE",
              "CMAKE_TOOLCHAIN_FILE", "VCPKG_CHAINLOAD_TOOLCHAIN_FILE", "VCPKG_TARGET_TRIPLET",
              "LABRADOR_RENDER_BACKEND", "LABRADOR_AUDIO_BACKEND", "LABRADOR_INPUT_BACKEND",
              "VCPKG_MANIFEST_INSTALL", "LABRADOR_TEST_BROWSER", "CMAKE_CROSSCOMPILING_EMULATOR",
              "FXC_EXECUTABLE", "DXC_SPIRV_EXECUTABLE", "Vulkan_INCLUDE_DIR", "Vulkan_LIBRARY"}


class PlanError(ValueError):
    """The current repository/selector is outside the planner's modeled rules."""


def render_paths(paths: list[str]) -> list[str]:
    normalized = set()
    for original in paths:
        value = original.replace("\\", "/")
        parts = PurePosixPath(value).parts
        if (not value or PureWindowsPath(original).drive or value.startswith("/")
                or ".." in parts):
            raise PlanError(f"use a repo-relative render path without traversal: {original!r}")
        value = PurePosixPath(value).as_posix()
        if value != "engine/render" and not value.startswith("engine/render/"):
            raise PlanError(f"area not modeled: {original!r}; only engine/render and its descendants are supported")
        normalized.add(value)
    return sorted(normalized)


def preset_index(document: dict, section: str) -> dict[str, dict]:
    entries = document.get(section)
    if not isinstance(entries, list):
        raise PlanError(f"CMakePresets.json has no {section} list")
    result = {}
    for entry in entries:
        if not isinstance(entry, dict) or not isinstance(entry.get("name"), str) or not entry["name"]:
            raise PlanError(f"invalid named entry in {section}")
        if entry["name"] in result:
            raise PlanError(f"duplicate {section} name: {entry['name']}")
        result[entry["name"]] = entry
    return result


def configure_preset(index: dict[str, dict], name: str, parents: tuple[str, ...] = ()) -> dict:
    if name in parents:
        raise PlanError(f"preset inheritance cycle: {' -> '.join((*parents, name))}")
    if name not in index:
        raise PlanError(f"missing configure preset: {name}")
    entry = index[name]
    allowed = {"name", "displayName", "description", "hidden", "inherits", "generator", "binaryDir",
               "installDir", "toolchainFile", "cacheVariables", "architecture", "condition"}
    if set(entry) - allowed:
        raise PlanError(f"unsupported configure preset fields in {name}: {sorted(set(entry) - allowed)}")
    if "hidden" in entry and not isinstance(entry["hidden"], bool):
        raise PlanError(f"hidden must be a boolean in {name}")
    if "condition" in entry and entry["condition"] != NATIVE_HOST_CONDITION:
        raise PlanError(f"only the exact Windows host condition is modeled: {name}")
    parent = entry.get("inherits")
    if parent is not None and not isinstance(parent, str):
        raise PlanError(f"only single-parent preset inheritance is modeled: {name}")
    result = configure_preset(index, parent, (*parents, name)) if parent else {}
    result = copy.deepcopy(result)
    cache = entry.get("cacheVariables", {})
    if not isinstance(cache, dict) or any(not isinstance(value, (str, int, bool)) for value in cache.values()):
        raise PlanError(f"only scalar cacheVariables are modeled: {name}")
    inherited_cache = result.get("cacheVariables", {})
    result.update(entry)
    result["cacheVariables"] = {**inherited_cache, **cache}
    result["hidden"] = entry.get("hidden", False)
    for field in ("binaryDir", "installDir", "toolchainFile"):
        value = result.get(field)
        if value is not None and (not isinstance(value, str) or
                                  re.sub(r"\$\{(?:sourceDir|presetName)\}|\$env\{(?:VCPKG_ROOT|EMSDK)\}", "", value).find("$") >= 0):
            raise PlanError(f"unsupported {field} expression in {name}")
    return result


def test_preset_output(index: dict[str, dict], name: str, parents: tuple[str, ...] = ()) -> dict:
    """The output a test preset resolves to through single-parent inheritance from hidden output-only presets."""
    if name in parents:
        raise PlanError(f"test preset inheritance cycle: {' -> '.join((*parents, name))}")
    if name not in index:
        raise PlanError(f"missing test preset: {name}")
    entry = index[name]
    if parents and (not entry.get("hidden") or set(entry) - {"name", "hidden", "output", "inherits"}):
        raise PlanError(f"only hidden output-only test presets can be inherited: {name}")
    parent = entry.get("inherits")
    if parent is not None and not isinstance(parent, str):
        raise PlanError(f"only single-parent test preset inheritance is modeled: {name}")
    result = test_preset_output(index, parent, (*parents, name)) if parent else {}
    if "output" in entry:
        output = entry["output"]
        if (not isinstance(output, dict) or set(output) - {"outputOnFailure"}
                or any(not isinstance(value, bool) for value in output.values())):
            raise PlanError(f"unsupported testPresets output shape for {name}")
        result = {**result, **output}
    return result


def read_rules(root: Path) -> tuple[dict, dict[str, dict], list[str]]:
    try:
        sources = {name: (root / name).read_text(encoding="utf-8") for name in SOURCE_FILES}
        document = json.loads(sources["CMakePresets.json"])
        inventory = json.loads(sources["tests/ctest_inventory.json"])
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise PlanError(f"cannot read repository verification rules: {error}") from error
    if not isinstance(document, dict) or document.get("version") != 6 or "include" in document:
        raise PlanError("only the repository's version-6, local CMakePresets.json is modeled")
    index = preset_index(document, "configurePresets")
    for name in NATIVE_PRESETS:
        if configure_preset(index, name).get("condition") != NATIVE_HOST_CONDITION:
            raise PlanError(f"{name} must inherit the Windows host condition")
    for name in ("wasm-debug", "wasm-release"):
        if "condition" in configure_preset(index, name):
            raise PlanError(f"{name} must have no host condition")
    for section in ("buildPresets", "testPresets"):
        linked = preset_index(document, section)
        for name in (*NATIVE, "wasm-debug", "wasm-release"):
            if name not in linked or linked[name].get("configurePreset") != name:
                raise PlanError(f"{section} must link {name} to its matching configure preset")
            allowed = {"name", "displayName", "description", "configurePreset"}
            if section == "testPresets":
                allowed |= {"output", "inherits"}
            if set(linked[name]) - allowed:
                raise PlanError(f"unsupported {section} behavior for {name}")
            if section == "testPresets":
                test_preset_output(linked, name)
    engine = re.sub(r"(?m)#.*$", "", sources["engine/CMakeLists.txt"])
    for axis, browser, native in (("render", "gl", "d3d11"), ("audio", "webaudio", "xaudio2"),
                                  ("input", "web", "xinput")):
        variable = "labrador_default_" + axis + "_backend"
        pattern = (r'if\(EMSCRIPTEN\)\s*set\(' + variable + r' "' + browser +
                   r'"\)\s*else\(\)\s*set\(' + variable + r' "' + native + r'"\)\s*endif\(\)')
        if not re.search(pattern, engine) or not re.search(
                r'set\(LABRADOR_' + axis.upper() + r'_BACKEND "\$\{' + variable + r'\}" CACHE STRING', engine):
            raise PlanError(f"engine {axis} defaults changed; review this planner's expectations")
    if " ".join(NULL_CONFIGURE) not in sources[".github/workflows/ci.yml"]:
        raise PlanError("CI wasm-null override changed; review the planned configure/build/test directory")
    shader = re.search(r"find_program\(DXC_SPIRV_EXECUTABLE\b(.*?)\n\s*\)",
                       sources["cmake/compile_shaders.cmake"], re.S)
    if not shader or 'HINTS "$ENV{VULKAN_SDK}/Bin"' not in shader[1] or "NO_DEFAULT_PATH" not in shader[1]:
        raise PlanError("Vulkan SDK-only dxc lookup changed; review the preflight rule")
    pages = list(dict.fromkeys(re.findall(r"\blabrador_sample_page_test\(\s*([A-Z]\w+)", sources["CMakeLists.txt"])))
    if not pages or "ComponentsPageTests" not in pages or "LineSweeperBundlePageTests" not in sources["CMakeLists.txt"]:
        raise PlanError("browser page source rules changed; review required registrations")
    chromium = inventory.get("chromium_registered") if isinstance(inventory, dict) else None
    release = inventory.get("some_configurations", {}) if isinstance(inventory, dict) else {}
    if (not isinstance(chromium, list) or not chromium or not isinstance(release, dict)
            or any(not isinstance(name, str) or "wasm-release" not in release.get(name, ()) for name in chromium)
            or len(set(chromium)) != len(chromium)):
        raise PlanError("tests/ctest_inventory.json's chromium_registered must list wasm-release entries once each")
    if set(pages) - set(chromium) or "LineSweeperBundlePageTests" not in chromium:
        raise PlanError("a CMake sample page is missing from chromium_registered; review required registrations")
    null_chromium = [name for name in chromium if "wasm-release-null" in release[name]]
    guards = []
    guarded = set()
    for command in re.finditer(r"\b(if|elseif|else|endif|add_test|labrador_page_test)\s*\(([^)]*)\)",
                               re.sub(r"(?m)#.*$", "", sources["CMakeLists.txt"])):
        kind, arguments = command[1], command[2].strip()
        if kind == "if":
            guards.append(arguments)
        elif kind in {"else", "elseif"} and guards:
            guards[-1] = "alternative branch"
        elif kind == "endif" and guards:
            guards.pop()
        elif kind in {"add_test", "labrador_page_test"} and 'NOT LABRADOR_RENDER_BACKEND STREQUAL "null"' in guards:
            match = re.match(r"(?:NAME\s+)?(LineSweeper(?:Bundle|Pacing)PageTests)\b", arguments)
            if match:
                guarded.add(match[1])
    if guarded != {"LineSweeperBundlePageTests", "LineSweeperPacingPageTests"}:
        raise PlanError("null bundle/pacing exclusions changed; review forbidden registrations")
    return document, index, chromium, null_chromium


def build_plan(root: Path, *, paths: list[str] | None = None, browser: str | None = None) -> dict:
    selected = render_paths(paths or [])
    if browser is not None:
        browser = browser.replace("\\", "/")
        if not (PureWindowsPath(browser).is_absolute() or PurePosixPath(browser).is_absolute()):
            raise PlanError("--browser must be an explicit absolute path; it is not discovered or launched")
    _, presets, chromium, null_chromium = read_rules(root)
    configurations = []
    for name, render in RENDER_BACKENDS.items():
        wasm = name.startswith("wasm-")
        preset_name = "wasm-release" if name == "wasm-release-null" else name
        preset = configure_preset(presets, preset_name)
        if preset["hidden"]:
            raise PlanError(f"selected configure preset is hidden and cannot be invoked: {preset_name}")
        cache = preset["cacheVariables"]
        binary = preset.get("binaryDir", "").replace("${sourceDir}", str(root)).replace("${presetName}", preset_name)
        if (preset.get("generator") != "Ninja" or preset.get("toolchainFile") != VCPKG_TOOLCHAIN
                or Path(binary) != root / "out/build" / preset_name):
            raise PlanError(f"generator/binaryDir/toolchain changed for {preset_name}")
        if not wasm and preset.get("architecture") != {"value": "x64", "strategy": "external"}:
            raise PlanError(f"native external x64 architecture changed for {preset_name}")
        expected_type = "Release" if preset_name == "wasm-release" else "Debug"
        base_render = "gl" if wasm else render
        base_audio = "webaudio" if wasm else ("null" if render == "null" else "xaudio2")
        expected = {"build_type": expected_type, "render": render,
                    "audio": "null" if name == "wasm-release-null" else base_audio,
                    "input": "web" if wasm else "xinput"}
        for key, value in (("CMAKE_BUILD_TYPE", expected_type),
                           ("LABRADOR_RENDER_BACKEND", base_render),
                           ("LABRADOR_AUDIO_BACKEND", base_audio),
                           ("LABRADOR_INPUT_BACKEND", expected["input"])):
            default = {"LABRADOR_RENDER_BACKEND": "gl" if wasm else "d3d11",
                       "LABRADOR_AUDIO_BACKEND": "webaudio" if wasm else "xaudio2",
                       "LABRADOR_INPUT_BACKEND": "web" if wasm else "xinput"}.get(key)
            if cache.get(key, default) != value:
                raise PlanError(f"{preset_name} no longer selects expected {key}={value}")
        if wasm and (cache.get("VCPKG_CHAINLOAD_TOOLCHAIN_FILE") != WASM_TOOLCHAIN
                     or cache.get("VCPKG_TARGET_TRIPLET") != "wasm32-emscripten"):
            raise PlanError(f"wasm toolchain/triplet changed for {preset_name}")
        configure = list(NULL_CONFIGURE) if name == "wasm-release-null" else ["cmake", "--preset", name]
        if wasm and browser:
            configure.append("-DLABRADOR_TEST_BROWSER:FILEPATH=" + browser)
        binary_dir = "out/build/" + name
        build = ["cmake", "--build", binary_dir] if name == "wasm-release-null" else ["cmake", "--build", "--preset", name]
        test = ["ctest", "--test-dir", binary_dir] if name == "wasm-release-null" else ["ctest", "--preset", name]
        build += ["--parallel", "2"]
        test += ["--parallel", "1", "--stop-on-failure", "--output-on-failure", "--no-tests=error"]
        runtime = (["generated-modules", "browser-pages-when-registered"] if wasm else
                   ["native-processes", "hidden-windows"] + ([] if render == "null" else ["graphics-device", "sample-drawing"]))
        if not wasm and expected["audio"] == "xaudio2":
            runtime.append("audio-backend-initialization")
        if wasm and expected["audio"] == "webaudio":
            runtime.append("browser-audio-initialization-when-registered")
        steps = [{"phase": phase, "argv": argv, "cwd": str(root), "effects": effects, "result": "not-run"}
                 for phase, argv, effects in (
                     ("configure", configure, ["build-tree-writes", "possible-dependency-network"]),
                     ("build", build, ["build-tree-writes", "automatic-reconfigure",
                                       "possible-dependency-network"] + (["SDK-cache-use"] if wasm else [])),
                     ("test", test, runtime))]
        required = ["RenderTests", "SceneTests", "AudioTests"]
        if not wasm and render != "null":
            required += ["RenderPixelTests", "MinimalSmokeTests", "CollisionSmokeTests"]
        if render == "d3d12":
            required.append("D3D12SynchronizationTests")
        browser_tests = list(chromium) if wasm and render == "gl" else []
        configurations.append({"id": name, "configure_preset": preset_name, "binary_dir": binary_dir,
                               "expected": expected, "toolchain": VCPKG_TOOLCHAIN,
                               "chainload": WASM_TOOLCHAIN if wasm else "",
                               "triplet": "wasm32-emscripten" if wasm else None,
                               "browser_override": browser if wasm else None, "steps": steps,
                               "required_test_names": required,
                               "required_browser_test_names": browser_tests,
                               "conditional_browser_test_names":
                                   list(null_chromium) if wasm and render == "null" else [],
                               "forbidden_test_names":
                                   ["RenderPixelTests", "RenderPixelTestsWithoutS3tc", "RenderContextLossTests",
                                    "LineSweeperBundlePageTests", "LineSweeperPacingPageTests"]
                                   if render == "null" else []})
    vulkan = any(path == "engine/render/vulkan" or path.startswith("engine/render/vulkan/") for path in selected)
    sync = "required" if vulkan else "conditional"
    manual = [
        {"id": "release-configurations", "condition": "supplemental per-change evidence", "status": "not-run",
         "source": "docs/product/STORIES.md#sm-18",
         "detail": "x64-release, x64-release-d3d12, x64-release-gl, x64-release-vulkan, x64-release-null, wasm-release"},
        {"id": "vulkan-synchronization", "condition": sync, "status": "not-run",
         "source": "docs/review/vulkan/README.md#2-what-was-run-and-what-the-layers-said",
         "detail": "For Vulkan source changes use validate_sync layer settings and stdout output; device execution is separate."},
        {"id": "registration-before-future-execution", "condition": "before any future test execution", "status": "not-run",
         "source": "AGENTS.md",
         "detail": "Reject zero selected tests and missing required/browser registrations; "
                   "--no-tests=error alone cannot detect omitted pages."},
        {"id": "other-verification", "condition": "affected contracts", "status": "not-run", "source": "AGENTS.md",
         "detail": "Website/lints/consumer packaging, opt-in probe builds, captures, listening, "
                   "Safari/pads/touch/phone and display pacing are separate; no hardware evidence is inferred."}]
    return {"schema_version": 1, "mode": "plan", "scope": {"area": "render", "paths": selected},
            "source_rules": ["AGENTS.md", "docs/product/STORIES.md#sm-17", "docs/product/STORIES.md#sm-18", *SOURCE_FILES],
            "configurations": configurations, "manual_obligations": manual,
            "not_modeled": ["other areas", "Release sweep execution", "physical acceptance"],
            "preflight": {"enabled": False, "checks": [
                {"id": "tooling-and-caches", "configurations": list(CONFIGURATIONS), "category": "tooling",
                 "status": "not-checked", "detail": "Use --preflight for read-only inspection; no commands execute.",
                 "blocking": False}]}}


def cache_values(text: str) -> dict[str, str]:
    result = {}
    for line in text.splitlines():
        match = re.fullmatch(r"([^/#][^:]*):[^=]+=(.*)", line)
        if match and match[1] in CACHE_KEYS:
            if match[1] in result:
                raise PlanError(f"duplicate whitelisted cache key: {match[1]}")
            result[match[1]] = match[2]
    return result


def normalized_path(value: str, windows: bool) -> str:
    value = posixpath.normpath(value.replace("\\", "/"))
    return value.casefold() if windows else value


def inspect_environment(plan: dict, *, environment: dict[str, str] | None = None, host: str | None = None,
                        find=None, is_file=None, read_text=None, required_cache_ids=()) -> list[dict]:
    if environment is None:
        environment = {key: os.environ.get(key, "") for key in ENVIRONMENT_KEYS}
    host = sys.platform if host is None else host
    find = find or (lambda name: shutil.which(name, path=environment.get("PATH", "")))
    is_file = is_file or Path.is_file
    read_text = read_text or (lambda path: path.read_text(encoding="utf-8"))
    root = Path(plan["configurations"][0]["steps"][0]["cwd"])
    checks = []
    windows = host == "win32"

    def add(identifier, ids, category, status, detail, blocking=None):
        checks.append({"id": identifier, "configurations": list(ids), "category": category,
                       "status": status, "detail": detail,
                       "blocking": status in {"missing", "mismatch", "unsupported-host"} if blocking is None else blocking})

    def file_check(identifier, ids, path):
        present = bool(path) and is_file(Path(path))
        add(identifier, ids, "tooling", "detected" if present else "missing",
            (str(path) + "; present only, unlaunched") if present else f"missing {identifier}: {path or 'no selected path'}")

    add("host", CONFIGURATIONS, "host", "detected" if windows else "unsupported-host",
        "Modeled whole sweep requires an existing Windows toolchain environment; other host setups are not qualified.")
    for tool, ids in (("cmake", CONFIGURATIONS), ("ctest", CONFIGURATIONS), ("ninja", CONFIGURATIONS),
                      ("cl", NATIVE), ("fxc", D3D_PRESETS)):
        path = find(tool)
        add(tool, ids, "tooling", "detected" if path else "missing",
            f"{path}; detected only, version/capability unverified" if path else f"missing PATH tool: {tool}")
    requirements = (("VCPKG_ROOT", "scripts/buildsystems/vcpkg.cmake", CONFIGURATIONS),
                    ("VULKAN_SDK", "Include/vulkan/vulkan.h", ("x64-debug-vulkan",)),
                    ("VULKAN_SDK", "Lib/vulkan-1.lib", ("x64-debug-vulkan",)),
                    ("VULKAN_SDK", "Bin/dxc.exe", ("x64-debug-vulkan",)),
                    ("EMSDK", "upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake",
                     WASM_CONFIGURATIONS),
                    ("EMSDK", "upstream/emscripten/emcc.py", WASM_CONFIGURATIONS))
    for variable, relative, ids in requirements:
        base = environment.get(variable, "")
        file_check(variable + "/" + relative, ids, Path(base) / relative if base else None)
    planned_wasm = [item["id"] for item in plan["configurations"] if item["id"].startswith("wasm-")]
    if planned_wasm and environment.get("MSYSTEM"):
        add("MSYSTEM", planned_wasm, "tooling", "mismatch", MSYSTEM_DETAIL.format(value=environment["MSYSTEM"]))
    for configuration in plan["configurations"]:
        name = configuration["id"]
        ids = [name]
        wasm = name.startswith("wasm-")
        cache_path = root / configuration["binary_dir"] / "CMakeCache.txt"
        values = {}
        if is_file(cache_path):
            try:
                values = cache_values(read_text(cache_path))
            except (OSError, UnicodeError, PlanError) as error:
                add(name + ":cache", ids, "cache", "missing", f"cannot inspect existing cache: {error}")
            else:
                expected = {"CMAKE_HOME_DIRECTORY": str(root), "CMAKE_CACHEFILE_DIR": str(cache_path.parent),
                            "CMAKE_GENERATOR": "Ninja", "CMAKE_BUILD_TYPE": configuration["expected"]["build_type"],
                            **{"LABRADOR_" + axis.upper() + "_BACKEND": configuration["expected"][axis]
                               for axis in ("render", "audio", "input")}}
                if environment.get("VCPKG_ROOT"):
                    expected["CMAKE_TOOLCHAIN_FILE"] = str(Path(environment["VCPKG_ROOT"]) / "scripts/buildsystems/vcpkg.cmake")
                if wasm:
                    expected["VCPKG_TARGET_TRIPLET"] = "wasm32-emscripten"
                    if configuration["browser_override"]:
                        expected["LABRADOR_TEST_BROWSER"] = configuration["browser_override"]
                    if environment.get("EMSDK"):
                        expected["VCPKG_CHAINLOAD_TOOLCHAIN_FILE"] = str(
                            Path(environment["EMSDK"]) / "upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake")
                elif values.get("VCPKG_CHAINLOAD_TOOLCHAIN_FILE"):
                    expected["VCPKG_CHAINLOAD_TOOLCHAIN_FILE"] = ""
                if name == "x64-debug-vulkan" and environment.get("VULKAN_SDK"):
                    sdk = Path(environment["VULKAN_SDK"])
                    expected.update({"DXC_SPIRV_EXECUTABLE": str(sdk / "Bin/dxc.exe"),
                                     "Vulkan_INCLUDE_DIR": str(sdk / "Include"), "Vulkan_LIBRARY": str(sdk / "Lib/vulkan-1.lib")})
                for key, wanted in expected.items():
                    actual = values.get(key)
                    path_axis = key.endswith(("DIRECTORY", "DIR", "FILE")) or key in {
                        "DXC_SPIRV_EXECUTABLE", "Vulkan_INCLUDE_DIR", "Vulkan_LIBRARY", "LABRADOR_TEST_BROWSER"}
                    equal = actual is not None and (
                        normalized_path(actual, windows) == normalized_path(wanted, windows) if path_axis
                        else actual == wanted)
                    add(name + ":" + key, ids, "cache", "detected" if equal else "mismatch",
                        f"{key} agrees with plan" if equal else
                        f"{key} does not match plan: actual={actual!r}, expected={wanted!r}; cache left unchanged")
                add(name + ":manifest-install", ids, "cache", "not-checked",
                    f"VCPKG_MANIFEST_INSTALL={values.get('VCPKG_MANIFEST_INSTALL', 'unspecified')}; "
                    "configure/build may download or reconfigure regardless of current installed files", False)
                if name in D3D_PRESETS:
                    selected_fxc = values.get("FXC_EXECUTABLE", "")
                    file_check(name + ":cached-FXC_EXECUTABLE", ids,
                               selected_fxc if selected_fxc and not selected_fxc.endswith("-NOTFOUND") else None)
        else:
            required = name in required_cache_ids
            add(name + ":cache", ids, "cache", "missing" if required else "not-checked",
                "Configured cache is missing." if required else "No existing cache; configuration has not run.", required)
        if not wasm:
            continue
        cached_selection = values.get("CMAKE_CROSSCOMPILING_EMULATOR", "").split(";", 1)[0].strip('"')
        cached_node = cached_selection
        if cached_node and not (PureWindowsPath(cached_node).is_absolute() or PurePosixPath(cached_node).is_absolute()):
            cached_node = find(cached_node) or ""
        if cached_selection and not cached_node:
            add(name + ":cached-Emscripten-Node", ids, "cache", "missing",
                f"Cached emulator command {cached_selection!r} was not found on PATH; cache left unchanged.")
        node = environment.get("EMSDK_NODE", "") or cached_node
        if node and not (PureWindowsPath(node).is_absolute() or PurePosixPath(node).is_absolute()):
            node = find(node) or ""
        file_check(name + ":selected-Emscripten-Node", ids, node or None)
        if (cached_node and environment.get("EMSDK_NODE")
                and normalized_path(cached_node, windows) != normalized_path(environment["EMSDK_NODE"], windows)):
            add(name + ":Node-selection", ids, "cache", "mismatch",
                "Cached emulator and EMSDK_NODE disagree; neither is activated or changed.")
        browser = configuration["browser_override"] or values.get("LABRADOR_TEST_BROWSER", "")
        if browser.endswith("-NOTFOUND"):
            browser = ""
        if configuration["required_browser_test_names"] or browser:
            file_check(name + ":Chromium-registration-basis", ids, browser or None)
        if not configuration["required_browser_test_names"]:
            add(name + ":conditional-browser-pages", ids, "runtime", "not-checked",
                "Full wasm-null CTest can run DOM/sample pages if Chromium is registered; generated modules also remain unrun.", False)
    return checks


def format_text(plan: dict) -> str:
    lines = ["Render verification plan only; every phase is not-run.", "argv arrays are data, not shell-ready command strings."]
    for configuration in plan["configurations"]:
        lines.append(f"\n{configuration['id']}: {json.dumps(configuration['expected'], sort_keys=True)}")
        for step in configuration["steps"]:
            lines.append(f"  {step['phase']} [{step['result']}] cwd={json.dumps(step['cwd'])}")
            lines.append("    argv=" + json.dumps(step["argv"]))
            lines.append("    effects=" + ", ".join(step["effects"]))
        for field in ("required_test_names", "required_browser_test_names", "conditional_browser_test_names", "forbidden_test_names"):
            lines.append("  " + field + "=" + json.dumps(configuration[field]))
    lines.append("\nRead-only preflight:")
    for check in plan["preflight"]["checks"]:
        lines.append(f"  {check['id']}: {check['status']} - {check['detail']}")
    lines.append("\nManual obligations (not-run):")
    for item in plan["manual_obligations"]:
        lines.append(f"  {item['id']} ({item['condition']}): {item['detail']} [{item['source']}]")
    lines.append("Not modeled: " + ", ".join(plan["not_modeled"]))
    return "\n".join(lines)


def main(arguments: list[str] | None = None, *, root: Path | None = None, inspector=None, executor=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=lambda prog: argparse.HelpFormatter(prog, width=88))
    selection = parser.add_mutually_exclusive_group(required=True)
    selection.add_argument("--area", choices=["render"])
    selection.add_argument("--path", action="append", help="repo-relative logical render selector; repeat for a union")
    parser.add_argument("--format", choices=["text", "json"], default="text")
    behavior = parser.add_mutually_exclusive_group()
    behavior.add_argument("--preflight", action="store_true", help="inspect tooling environment/paths/caches without running tools")
    parser.add_argument("--browser", help="absolute Chromium path to include in planned wasm configure argv")
    behavior.add_argument("--execute", action="store_true", help="explicitly run the seven-track sweep; stops on first failure")
    parser.add_argument("--output-dir", help="new directory for execution logs/report; valid only with --execute")
    try:
        options = parser.parse_args(arguments)
    except SystemExit as error:
        return int(error.code)
    try:
        if options.output_dir is not None and (not options.execute or not options.output_dir.strip()):
            raise PlanError("--output-dir requires --execute and a nonempty new directory")
        plan = build_plan(root if root is not None else REPOSITORY, paths=options.path, browser=options.browser)
        if options.execute:
            if executor is None:
                from verification_runner import execute_plan
                executor = execute_plan
            report = executor(plan, inspector=inspector or inspect_environment,
                              cache_reader=cache_values, output_dir=options.output_dir)
            if options.format == "json":
                print(json.dumps(report, indent=2, sort_keys=True))
            else:
                print(f"Command sweep: {report['result']}; manual acceptance remains separate.")
                if report["failure"]:
                    failure = report["failure"]
                    print(f"{failure['configuration'] or 'sweep'}/{failure['phase']}: {failure['detail']}")
                print("Report: " + str(report.get("report_path") or "not written"))
                if report.get("report_write_error"):
                    print("Report write failed: " + report["report_write_error"])
            return report["exit_code"]
        if options.preflight:
            checks = (inspector or inspect_environment)(plan)
            plan["preflight"] = {"enabled": True, "checks": checks}
    except (PlanError, OSError) as error:
        print(f"Cannot form verification plan: {error}", file=sys.stderr)
        return 2
    print(json.dumps(plan, indent=2, sort_keys=True) if options.format == "json" else format_text(plan))
    return 1 if any(check["blocking"] for check in plan["preflight"]["checks"]) else 0


if __name__ == "__main__":
    raise SystemExit(main())
