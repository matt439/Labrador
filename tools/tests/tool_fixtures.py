"""Fixtures shared by the tools/ tests: loading a tool, isolated Git repositories
and the render planner's synthetic checkout. Nothing here runs a configured tool.

The scripts in tools/ import one another by bare name (git_local,
check_golden_reviews, verification_runner, assemble) with their own folder on
sys.path. load_tool loads a script the same way and registers it under that
name, so a test and the tool that imports it share one module object. The
cloud_performance package is imported as the package it is.
"""

from __future__ import annotations

import importlib.util
import os
from pathlib import Path
import sys
import tempfile
from types import ModuleType
from typing import Callable
import unittest

REPOSITORY = Path(__file__).resolve().parents[2]


def load_tool(relative: str) -> ModuleType:
    """The script at a repository-relative path, imported under its file name."""
    path = (REPOSITORY / relative).resolve()
    existing = sys.modules.get(path.stem)
    if existing is not None and getattr(existing, "__file__", None) \
            and Path(existing.__file__).resolve() == path:
        return existing
    spec = importlib.util.spec_from_file_location(path.stem, path)
    module = importlib.util.module_from_spec(spec)
    # Registered before it runs: a dataclass looks its module up while the
    # module is executing.
    sys.modules[path.stem] = module
    try:
        spec.loader.exec_module(module)
    except BaseException:
        del sys.modules[path.stem]
        raise
    return module


def isolated_git_environment(root: Path, identity: str) -> tuple[dict[str, str], Path]:
    """An environment for fixture Git commands, and its empty hooks folder.

    No ambient GIT_* variable or system or global configuration reaches the
    fixture, so neither does a configured template; isolate_repository points
    hooks at the empty folder. The identity is fixed and Git never prompts, so
    a developer's signing, hooks or config cannot change what a commit is.
    """
    hooks = root / "empty-hooks"
    hooks.mkdir(exist_ok=True)
    config = root / "empty-gitconfig"
    config.write_text("", encoding="ascii")
    environment = {name: value for name, value in os.environ.items()
                   if not name.upper().startswith("GIT_")}
    environment.update({"GIT_CONFIG_NOSYSTEM": "1", "GIT_CONFIG_GLOBAL": str(config),
                        "GIT_AUTHOR_NAME": identity, "GIT_AUTHOR_EMAIL": "fixture@example.invalid",
                        "GIT_COMMITTER_NAME": identity, "GIT_COMMITTER_EMAIL": "fixture@example.invalid",
                        "GIT_TERMINAL_PROMPT": "0"})
    return environment, hooks


def isolate_repository(git: Callable[..., object], hooks: Path) -> None:
    """Pin a fixture repository's own configuration; git runs one command in it."""
    git("config", "core.autocrlf", "false")
    git("config", "core.hooksPath", str(hooks))
    git("config", "commit.gpgsign", "false")
    git("config", "gc.auto", "0")


class PlannerCheckout:
    """A temporary checkout holding only the render planner's source files,
    with synthetic tooling paths and caches that are never executed."""

    def __init__(self, test: unittest.TestCase, planner: ModuleType):
        temporary = tempfile.TemporaryDirectory(prefix="labrador-verification-plan-")
        test.addCleanup(temporary.cleanup)
        self.planner = planner
        self.root = Path(temporary.name)
        for name in planner.SOURCE_FILES:
            target = self.root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes((REPOSITORY / name).read_bytes())

    def plan(self, **options) -> dict:
        return self.planner.build_plan(self.root, **options)

    def fake_tooling(self) -> tuple[dict[str, str], dict[str, str], str]:
        root = self.root
        environment = {"VCPKG_ROOT": str(root / "vcpkg"), "VULKAN_SDK": str(root / "vulkan"),
                       "EMSDK": str(root / "emsdk"), "EMSDK_NODE": str(root / "emsdk/node/node.exe"),
                       "PATH": "synthetic-only", "PATHEXT": ".EXE"}
        required = [Path(environment["VCPKG_ROOT"]) / "scripts/buildsystems/vcpkg.cmake",
                    Path(environment["VULKAN_SDK"]) / "Include/vulkan/vulkan.h",
                    Path(environment["VULKAN_SDK"]) / "Lib/vulkan-1.lib",
                    Path(environment["VULKAN_SDK"]) / "Bin/dxc.exe",
                    Path(environment["EMSDK"]) / "upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake",
                    Path(environment["EMSDK"]) / "upstream/emscripten/emcc.py",
                    Path(environment["EMSDK_NODE"]), root / "browser/chrome.exe"]
        for path in required:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"fixture: never executed")
        tools = {name: str(root / "tools" / (name + ".exe"))
                 for name in ("cmake", "ctest", "ninja", "cl", "fxc", "node", "dxc")}
        for value in tools.values():
            path = Path(value)
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"fixture: never executed")
        return environment, tools, str(root / "browser/chrome.exe")

    def inspect(self, plan: dict, environment: dict[str, str], tools: dict[str, str], **options) -> list[dict]:
        return self.planner.inspect_environment(
            plan, environment=environment, host=options.pop("host", "win32"), find=tools.get,
            is_file=Path.is_file, read_text=lambda path: path.read_text(encoding="utf-8"), **options)

    def write_caches(self, plan: dict, environment: dict[str, str], browser: str) -> None:
        for configuration in plan["configurations"]:
            path = self.root / configuration["binary_dir"] / "CMakeCache.txt"
            values = {"CMAKE_HOME_DIRECTORY": str(self.root), "CMAKE_CACHEFILE_DIR": str(path.parent),
                      "CMAKE_GENERATOR": "Ninja", "CMAKE_BUILD_TYPE": configuration["expected"]["build_type"],
                      "CMAKE_TOOLCHAIN_FILE": str(Path(environment["VCPKG_ROOT"]) / "scripts/buildsystems/vcpkg.cmake"),
                      "VCPKG_MANIFEST_INSTALL": "ON",
                      **{"LABRADOR_" + axis.upper() + "_BACKEND": configuration["expected"][axis]
                         for axis in ("render", "audio", "input")}}
            if configuration["id"].startswith("wasm-"):
                values.update({"VCPKG_TARGET_TRIPLET": "wasm32-emscripten",
                               "VCPKG_CHAINLOAD_TOOLCHAIN_FILE": str(Path(environment["EMSDK"]) / "upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake"),
                               "CMAKE_CROSSCOMPILING_EMULATOR": environment["EMSDK_NODE"] + ";--fixture-flag",
                               "LABRADOR_TEST_BROWSER": browser})
            if configuration["id"] == "x64-debug-vulkan":
                values.update({"DXC_SPIRV_EXECUTABLE": str(Path(environment["VULKAN_SDK"]) / "Bin/dxc.exe"),
                               "Vulkan_INCLUDE_DIR": str(Path(environment["VULKAN_SDK"]) / "Include"),
                               "Vulkan_LIBRARY": str(Path(environment["VULKAN_SDK"]) / "Lib/vulkan-1.lib")})
            if configuration["id"] in self.planner.D3D_PRESETS:
                values["FXC_EXECUTABLE"] = str(self.root / "tools/fxc.exe")
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("\n".join(key + ":STRING=" + value for key, value in values.items()) + "\n",
                            encoding="utf-8")
