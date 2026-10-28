"""Pure plan and synthetic-path/cache fixtures; no configured tool is executed."""

from __future__ import annotations

import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock

REPOSITORY = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("verify_changes", REPOSITORY / "tools/verify_changes.py")
planner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(planner)


class PlanTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="labrador-verification-plan-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        for name in planner.SOURCE_FILES:
            target = self.root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes((REPOSITORY / name).read_bytes())

    def plan(self, **options):
        return planner.build_plan(self.root, **options)

    def change_presets(self, change):
        path = self.root / "CMakePresets.json"
        data = json.loads(path.read_text(encoding="utf-8"))
        change(data)
        path.write_text(json.dumps(data), encoding="utf-8")

    def invoke(self, arguments, inspector=None):
        output, errors = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(output), contextlib.redirect_stderr(errors):
            code = planner.main(arguments, root=self.root, inspector=inspector)
        return code, output.getvalue(), errors.getvalue()

    def fake_tooling(self):
        environment = {"VCPKG_ROOT": str(self.root / "vcpkg"), "VULKAN_SDK": str(self.root / "vulkan"),
                       "EMSDK": str(self.root / "emsdk"), "EMSDK_NODE": str(self.root / "emsdk/node/node.exe"),
                       "PATH": "synthetic-only", "PATHEXT": ".EXE"}
        required = [Path(environment["VCPKG_ROOT"]) / "scripts/buildsystems/vcpkg.cmake",
                    Path(environment["VULKAN_SDK"]) / "Include/vulkan/vulkan.h",
                    Path(environment["VULKAN_SDK"]) / "Lib/vulkan-1.lib",
                    Path(environment["VULKAN_SDK"]) / "Bin/dxc.exe",
                    Path(environment["EMSDK"]) / "upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake",
                    Path(environment["EMSDK"]) / "upstream/emscripten/emcc.py",
                    Path(environment["EMSDK_NODE"]), self.root / "browser/chrome.exe"]
        for path in required:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"fixture: never executed")
        tools = {name: str(self.root / "tools" / (name + ".exe"))
                 for name in ("cmake", "ctest", "ninja", "cl", "fxc", "node", "dxc")}
        for value in tools.values():
            path = Path(value)
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b"fixture: never executed")
        return environment, tools, str(self.root / "browser/chrome.exe")

    def inspect(self, plan, environment, tools, **options):
        return planner.inspect_environment(plan, environment=environment, host=options.pop("host", "win32"),
                                           find=tools.get, is_file=Path.is_file,
                                           read_text=lambda path: path.read_text(encoding="utf-8"), **options)

    def write_caches(self, plan, environment, browser):
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
            if configuration["id"] in planner.NATIVE[:2]:
                values["FXC_EXECUTABLE"] = str(self.root / "tools/fxc.exe")
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("\n".join(key + ":STRING=" + value for key, value in values.items()) + "\n",
                            encoding="utf-8")

    def test_exact_seven_tracks_have_distinct_expected_axes_and_unrun_phases(self):
        plan = self.plan()
        self.assertEqual([item["id"] for item in plan["configurations"]], list(planner.CONFIGURATIONS))
        self.assertEqual([item["expected"]["render"] for item in plan["configurations"]], list(planner.BACKENDS))
        self.assertEqual({step["result"] for item in plan["configurations"] for step in item["steps"]}, {"not-run"})
        for item in plan["configurations"]:
            self.assertEqual([step["phase"] for step in item["steps"]], ["configure", "build", "test"])
            self.assertTrue(all(step["cwd"] == str(self.root) for step in item["steps"]))
            self.assertIn("--no-tests=error", item["steps"][2]["argv"])

    def test_ci_null_override_never_uses_the_gl_build_or_test_preset(self):
        configuration = self.plan()["configurations"][-1]
        self.assertEqual(configuration["steps"][0]["argv"], planner.NULL_CONFIGURE)
        self.assertEqual(configuration["expected"],
                         {"build_type": "Release", "render": "null", "audio": "null", "input": "web"})
        self.assertEqual(configuration["steps"][1]["argv"],
                         ["cmake", "--build", "out/build/wasm-release-null", "--parallel", "2"])
        self.assertNotIn("--preset", configuration["steps"][2]["argv"])
        self.assertIn("browser-pages-when-registered", configuration["steps"][2]["effects"])

    def test_exact_render_root_windows_separators_deleted_paths_and_union(self):
        paths = ["engine/render", "engine\\render\\gl\\deleted.cpp", "engine/render/gl/deleted.cpp",
                 "engine/render/vulkan"]
        self.assertEqual(self.plan(paths=paths)["scope"]["paths"],
                         ["engine/render", "engine/render/gl/deleted.cpp", "engine/render/vulkan"])
        self.assertEqual(self.plan(paths=["engine/render/"])["scope"]["paths"], ["engine/render"])

    def test_outside_traversal_and_unmodeled_paths_are_rejected(self):
        for path in ("", "engine/rendered/file.cpp", "engine/audio", "/engine/render", "C:\\engine\\render",
                     "C:engine/render", "../engine/render", "engine/render/../assets", "\\\\host\\share\\engine\\render"):
            with self.subTest(path=path), self.assertRaises(planner.PlanError):
                self.plan(paths=[path])

    def test_default_planning_does_not_inspect_tooling_environment_tools_or_caches(self):
        class NoToolingEnvironment(dict):
            def get(self, key, default=None):
                if key in planner.ENVIRONMENT_KEYS:
                    raise AssertionError("tooling environment read")
                return super().get(key, default)

            def __getitem__(self, key):
                if key in planner.ENVIRONMENT_KEYS:
                    raise AssertionError("tooling environment read")
                return super().__getitem__(key)

        with mock.patch.object(planner, "inspect_environment", side_effect=AssertionError("real preflight")), \
                mock.patch.object(planner.shutil, "which", side_effect=AssertionError("tool discovery")), \
                mock.patch.object(planner.os, "environ", NoToolingEnvironment()) as environment, \
                mock.patch("subprocess.run", side_effect=AssertionError("process")), \
                mock.patch("subprocess.Popen", side_effect=AssertionError("process")), \
                mock.patch.object(planner.os, "system", side_effect=AssertionError("shell")):
            code, output, errors = self.invoke(["--area", "render", "--format", "json"])
        self.assertEqual((code, errors), (0, ""))
        self.assertFalse(json.loads(output)["preflight"]["enabled"])
        self.assertEqual(environment, {})
        self.assertIn("$env{VCPKG_ROOT}", output)
        self.assertIn("$env{EMSDK}", output)

    def test_nothing_writes_during_plan_or_synthetic_preflight(self):
        environment, tools, browser = self.fake_tooling()
        plan = self.plan(browser=browser)
        with mock.patch.object(Path, "write_text", side_effect=AssertionError("write")), \
                mock.patch.object(Path, "write_bytes", side_effect=AssertionError("write")), \
                mock.patch.object(Path, "unlink", side_effect=AssertionError("delete")), \
                mock.patch("subprocess.run", side_effect=AssertionError("process")), \
                mock.patch("subprocess.Popen", side_effect=AssertionError("process")):
            self.assertEqual(self.plan(), self.plan())
            checks = self.inspect(plan, environment, tools)
        self.assertFalse(any(check["blocking"] for check in checks))

    def test_text_and_json_keep_argv_with_spaces_unambiguous(self):
        browser = "C:/Program Files/Chromium/chrome.exe"
        plan = self.plan(browser=browser)
        text = planner.format_text(plan)
        for configuration in plan["configurations"]:
            for step in configuration["steps"]:
                self.assertIn("argv=" + json.dumps(step["argv"]), text)
        self.assertIn(json.dumps("-DLABRADOR_TEST_BROWSER:FILEPATH=" + browser), text)
        self.assertIn("not shell-ready", text)

    def test_json_is_stable_and_nested_working_directory_does_not_change_root(self):
        first = self.invoke(["--area", "render", "--format", "json"])
        self.assertEqual(first, self.invoke(["--area", "render", "--format", "json"]))
        with mock.patch.object(planner.Path, "cwd", side_effect=AssertionError("cwd selection")):
            self.assertEqual(first, self.invoke(["--area", "render", "--format", "json"]))

    def test_registration_constraints_and_manual_limits_remain_explicit(self):
        plan = self.plan(paths=["engine/render/vulkan/renderer.cpp"])
        gl = plan["configurations"][-2]
        for name in ("RenderPixelTests", "RenderPixelTestsWithoutS3tc", "RenderContextLossTests", "WebWindowTests",
                     "ComponentsPageTests", "LineSweeperBundlePageTests"):
            self.assertIn(name, gl["required_browser_test_names"])
        self.assertIn("D3D12SynchronizationTests", plan["configurations"][1]["required_test_names"])
        self.assertIn("RenderPixelTests", plan["configurations"][4]["forbidden_test_names"])
        self.assertIn("WebWindowTests", plan["configurations"][-1]["conditional_browser_test_names"])
        self.assertEqual(next(item for item in plan["manual_obligations"] if item["id"] == "vulkan-synchronization")["condition"], "required")
        self.assertEqual(next(item for item in self.plan()["manual_obligations"] if item["id"] == "vulkan-synchronization")["condition"], "conditional")
        self.assertTrue(all(item["status"] == "not-run" for item in plan["manual_obligations"]))
        self.assertIn("audio-backend-initialization", plan["configurations"][0]["steps"][2]["effects"])
        self.assertNotIn("audio-backend-initialization", plan["configurations"][4]["steps"][2]["effects"])
        self.assertIn("browser-audio-initialization-when-registered", plan["configurations"][-2]["steps"][2]["effects"])
        self.assertNotIn("browser-audio-initialization-when-registered", plan["configurations"][-1]["steps"][2]["effects"])

    def test_missing_parent_cycle_duplicate_and_unsupported_shapes_fail(self):
        mutations = [lambda data: data["configurePresets"][0].update(inherits="missing"),
                     lambda data: data["configurePresets"][0].update(inherits="x64-debug"),
                     lambda data: data["configurePresets"].append(data["configurePresets"][0]),
                     lambda data: data["configurePresets"][0].update(inherits=["another"]),
                     lambda data: data["configurePresets"][0].update(binaryDir="${fileDir}/build"),
                     lambda data: data["configurePresets"][0].update(condition=True),
                     lambda data: data.update(version=7), lambda data: data.update(include=["other.json"])]
        original = (self.root / "CMakePresets.json").read_bytes()
        for change in mutations:
            with self.subTest(change=change):
                self.change_presets(change)
                with self.assertRaises(planner.PlanError):
                    self.plan()
                (self.root / "CMakePresets.json").write_bytes(original)

    def test_preset_axes_and_build_test_links_cannot_silently_drift(self):
        self.change_presets(lambda data: data["configurePresets"][1]["cacheVariables"].update(LABRADOR_RENDER_BACKEND="gl"))
        with self.assertRaisesRegex(planner.PlanError, "expected LABRADOR_RENDER_BACKEND=d3d11"):
            self.plan()

    def test_missing_preset_and_wrong_test_link_are_actionable(self):
        self.change_presets(lambda data: data["testPresets"][0].update(configurePreset="x64-debug-null"))
        with self.assertRaisesRegex(planner.PlanError, "testPresets must link x64-debug"):
            self.plan()

    def test_hidden_selected_presets_and_unmodeled_output_shapes_are_rejected(self):
        mutations = [lambda data: data["configurePresets"][1].update(hidden=True),
                     lambda data: data["buildPresets"][0].update(output={"outputOnFailure": True}),
                     lambda data: data["testPresets"][0].update(output="verbose"),
                     lambda data: data["testPresets"][0].update(output={"outputOnFailure": "true"}),
                     lambda data: data["testPresets"][0].update(output={"verbosity": "verbose"})]
        original = (self.root / "CMakePresets.json").read_bytes()
        for change in mutations:
            with self.subTest(change=change):
                self.change_presets(change)
                with self.assertRaises(planner.PlanError):
                    self.plan()
                (self.root / "CMakePresets.json").write_bytes(original)

    def test_current_source_defaults_and_ci_override_are_pinned(self):
        path = self.root / "engine/CMakeLists.txt"
        path.write_text(path.read_text(encoding="utf-8").replace('set(labrador_default_render_backend "d3d11")',
                                                               'set(labrador_default_render_backend "gl")'), encoding="utf-8")
        with self.assertRaisesRegex(planner.PlanError, "render defaults changed"):
            self.plan()
        path.write_bytes((REPOSITORY / "engine/CMakeLists.txt").read_bytes())
        workflow = self.root / ".github/workflows/ci.yml"
        workflow.write_text(workflow.read_text(encoding="utf-8").replace("-DLABRADOR_AUDIO_BACKEND=null", "-DLABRADOR_AUDIO_BACKEND=webaudio"), encoding="utf-8")
        with self.assertRaisesRegex(planner.PlanError, "CI wasm-null override changed"):
            self.plan()

    def test_missing_environment_and_tools_report_all_prerequisites(self):
        checks = self.inspect(self.plan(), {}, {})
        identifiers = {check["id"] for check in checks if check["status"] == "missing"}
        self.assertIn("cmake", identifiers)
        self.assertIn("fxc", identifiers)
        self.assertIn("VULKAN_SDK/Bin/dxc.exe", identifiers)
        self.assertIn("VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake", identifiers)
        self.assertTrue(any(identifier.startswith("EMSDK/") for identifier in identifiers))
        self.assertIn("wasm-debug:Chromium-registration-basis", identifiers)

    def test_path_dxc_does_not_replace_the_sdk_owned_compiler(self):
        environment, tools, browser = self.fake_tooling()
        (Path(environment["VULKAN_SDK"]) / "Bin/dxc.exe").unlink()
        checks = self.inspect(self.plan(browser=browser), environment, tools)
        self.assertEqual(next(item for item in checks if item["id"] == "VULKAN_SDK/Bin/dxc.exe")["status"], "missing")

    def test_matching_caches_and_tool_paths_still_do_not_claim_execution(self):
        environment, tools, browser = self.fake_tooling()
        plan = self.plan()
        self.write_caches(plan, environment, browser)
        checks = self.inspect(plan, environment, tools)
        self.assertFalse(any(item["blocking"] for item in checks))
        self.assertTrue(any("may download" in item["detail"] for item in checks))
        self.assertEqual({step["result"] for item in plan["configurations"] for step in item["steps"]}, {"not-run"})

    def test_stale_default_backend_source_build_type_and_toolchain_are_reported(self):
        environment, tools, browser = self.fake_tooling()
        plan = self.plan()
        self.write_caches(plan, environment, browser)
        path = self.root / "out/build/x64-debug/CMakeCache.txt"
        before = path.read_text(encoding="utf-8")
        changed = before.replace("LABRADOR_RENDER_BACKEND:STRING=d3d11", "LABRADOR_RENDER_BACKEND:STRING=gl")
        changed = changed.replace("CMAKE_BUILD_TYPE:STRING=Debug", "CMAKE_BUILD_TYPE:STRING=Release")
        changed = changed.replace("CMAKE_HOME_DIRECTORY:STRING=" + str(self.root), "CMAKE_HOME_DIRECTORY:STRING=other-source")
        changed = changed.replace("vcpkg.cmake", "wrong.cmake")
        path.write_text(changed, encoding="utf-8")
        checks = self.inspect(plan, environment, tools)
        mismatches = {item["id"] for item in checks if item["status"] == "mismatch"}
        for axis in ("LABRADOR_RENDER_BACKEND", "CMAKE_BUILD_TYPE", "CMAKE_HOME_DIRECTORY", "CMAKE_TOOLCHAIN_FILE"):
            self.assertIn("x64-debug:" + axis, mismatches)
        self.assertEqual(path.read_text(encoding="utf-8"), changed)

    def test_a_cached_path_dxc_override_is_detected_even_with_a_valid_sdk(self):
        environment, tools, browser = self.fake_tooling()
        plan = self.plan()
        self.write_caches(plan, environment, browser)
        path = self.root / "out/build/x64-debug-vulkan/CMakeCache.txt"
        value = path.read_text(encoding="utf-8").replace(str(Path(environment["VULKAN_SDK"]) / "Bin/dxc.exe"), tools["dxc"])
        path.write_text(value, encoding="utf-8")
        checks = self.inspect(plan, environment, tools)
        self.assertEqual(next(item for item in checks if item["id"] == "x64-debug-vulkan:DXC_SPIRV_EXECUTABLE")["status"], "mismatch")

    def test_cached_fxc_selection_is_checked_instead_of_trusting_another_path_compiler(self):
        environment, tools, browser = self.fake_tooling()
        plan = self.plan()
        self.write_caches(plan, environment, browser)
        path = self.root / "out/build/x64-debug/CMakeCache.txt"
        missing = str(self.root / "missing/fxc.exe")
        path.write_text(path.read_text(encoding="utf-8").replace(tools["fxc"], missing), encoding="utf-8")
        checks = self.inspect(plan, environment, tools)
        self.assertEqual(next(item for item in checks if item["id"] == "x64-debug:cached-FXC_EXECUTABLE")["status"], "missing")
        alternative = self.root / "intentional/fxc.exe"
        alternative.parent.mkdir()
        alternative.write_bytes(b"fixture: never executed")
        path.write_text(path.read_text(encoding="utf-8").replace(missing, str(alternative)), encoding="utf-8")
        checks = self.inspect(plan, environment, tools)
        self.assertEqual(next(item for item in checks if item["id"] == "x64-debug:cached-FXC_EXECUTABLE")["status"], "detected")

    def test_selected_null_browser_is_checked_but_absent_null_browser_stays_conditional(self):
        environment, tools, browser = self.fake_tooling()
        plan = self.plan()
        self.write_caches(plan, environment, browser)
        path = self.root / "out/build/wasm-release-null/CMakeCache.txt"
        stale = str(self.root / "removed-browser.exe")
        path.write_text(path.read_text(encoding="utf-8").replace(browser, stale), encoding="utf-8")
        checks = self.inspect(plan, environment, tools)
        self.assertEqual(next(item for item in checks if item["id"] == "wasm-release-null:Chromium-registration-basis")["status"], "missing")
        path.write_text(path.read_text(encoding="utf-8").replace(stale, "LABRADOR_TEST_BROWSER-NOTFOUND"), encoding="utf-8")
        checks = self.inspect(plan, environment, tools)
        self.assertFalse(any(item["id"] == "wasm-release-null:Chromium-registration-basis" for item in checks))
        self.assertTrue(any(item["id"] == "wasm-release-null:conditional-browser-pages" for item in checks))

    def test_selected_node_comes_from_sdk_environment_or_configured_emulator_not_arbitrary_path(self):
        environment, tools, browser = self.fake_tooling()
        plan = self.plan(browser=browser)
        environment.pop("EMSDK_NODE")
        checks = self.inspect(plan, environment, tools)
        self.assertEqual(next(item for item in checks if item["id"] == "wasm-debug:selected-Emscripten-Node")["status"], "missing")

    def test_configured_emulator_selects_node_without_activating_sdk_environment(self):
        environment, tools, browser = self.fake_tooling()
        plan = self.plan()
        self.write_caches(plan, environment, browser)
        environment.pop("EMSDK_NODE")
        checks = self.inspect(plan, environment, tools)
        self.assertEqual(next(item for item in checks if item["id"] == "wasm-debug:selected-Emscripten-Node")["status"], "detected")

    def test_environment_node_disagreement_with_the_cached_emulator_is_explicit(self):
        environment, tools, browser = self.fake_tooling()
        plan = self.plan()
        self.write_caches(plan, environment, browser)
        other = self.root / "other-node.exe"
        other.write_bytes(b"fixture")
        environment["EMSDK_NODE"] = str(other)
        checks = self.inspect(plan, environment, tools)
        self.assertEqual(next(item for item in checks if item["id"] == "wasm-debug:Node-selection")["status"], "mismatch")

    def test_unresolved_cached_emulator_is_not_hidden_by_a_valid_environment_node(self):
        environment, tools, browser = self.fake_tooling()
        plan = self.plan()
        self.write_caches(plan, environment, browser)
        path = self.root / "out/build/wasm-debug/CMakeCache.txt"
        changed = path.read_text(encoding="utf-8").replace(environment["EMSDK_NODE"], "missing-node;--fixture")
        path.write_text(changed, encoding="utf-8")
        checks = self.inspect(plan, environment, tools)
        missing = next(item for item in checks if item["id"] == "wasm-debug:cached-Emscripten-Node")
        self.assertEqual(missing["status"], "missing")
        self.assertTrue(missing["blocking"])
        self.assertIn("missing-node", missing["detail"])
        self.assertEqual(next(item for item in checks if item["id"] == "wasm-debug:selected-Emscripten-Node")["status"], "detected")
        self.assertEqual(path.read_text(encoding="utf-8"), changed)

    def test_a_browser_environment_variable_is_not_cmake_discovery(self):
        environment, tools, browser = self.fake_tooling()
        environment["LABRADOR_TEST_BROWSER"] = browser
        checks = self.inspect(self.plan(), environment, tools)
        self.assertEqual(next(item for item in checks if item["id"] == "wasm-debug:Chromium-registration-basis")["status"], "missing")

    def test_generator_build_directory_audio_input_and_wasm_triplet_cache_axes_are_checked(self):
        environment, tools, browser = self.fake_tooling()
        plan = self.plan()
        self.write_caches(plan, environment, browser)
        path = self.root / "out/build/wasm-debug/CMakeCache.txt"
        changed = path.read_text(encoding="utf-8").replace("CMAKE_GENERATOR:STRING=Ninja", "CMAKE_GENERATOR:STRING=Other")
        changed = changed.replace("CMAKE_CACHEFILE_DIR:STRING=" + str(path.parent), "CMAKE_CACHEFILE_DIR:STRING=other-build")
        changed = changed.replace("LABRADOR_AUDIO_BACKEND:STRING=webaudio", "LABRADOR_AUDIO_BACKEND:STRING=null")
        changed = changed.replace("LABRADOR_INPUT_BACKEND:STRING=web", "LABRADOR_INPUT_BACKEND:STRING=xinput")
        changed = changed.replace("VCPKG_TARGET_TRIPLET:STRING=wasm32-emscripten", "VCPKG_TARGET_TRIPLET:STRING=x64-windows")
        path.write_text(changed, encoding="utf-8")
        checks = self.inspect(plan, environment, tools)
        mismatches = {item["id"] for item in checks if item["status"] == "mismatch"}
        for axis in ("CMAKE_GENERATOR", "CMAKE_CACHEFILE_DIR", "LABRADOR_AUDIO_BACKEND", "LABRADOR_INPUT_BACKEND", "VCPKG_TARGET_TRIPLET"):
            self.assertIn("wasm-debug:" + axis, mismatches)

    def test_cache_whitelist_does_not_expose_unrelated_values(self):
        parsed = planner.cache_values("SECRET_TOKEN:STRING=do-not-report\nLABRADOR_RENDER_BACKEND:STRING=gl\n")
        self.assertEqual(parsed, {"LABRADOR_RENDER_BACKEND": "gl"})
        with self.assertRaises(planner.PlanError):
            planner.cache_values("LABRADOR_RENDER_BACKEND:STRING=gl\nLABRADOR_RENDER_BACKEND:STRING=null\n")

    def test_nonwindows_host_is_unsupported_without_tool_or_device_probes(self):
        environment, tools, browser = self.fake_tooling()
        checks = self.inspect(self.plan(browser=browser), environment, tools, host="linux")
        self.assertEqual(checks[0]["status"], "unsupported-host")
        self.assertTrue(checks[0]["blocking"])

    def test_cli_preflight_exit_one_still_emits_complete_unrun_json(self):
        def synthetic_inspector(plan):
            return [{"id": "EMSDK", "configurations": ["wasm-debug"], "category": "tooling",
                     "status": "missing", "detail": "synthetic missing EMSDK", "blocking": True}]
        code, output, errors = self.invoke(["--area", "render", "--preflight", "--format", "json"], synthetic_inspector)
        self.assertEqual((code, errors), (1, ""))
        plan = json.loads(output)
        self.assertEqual(len(plan["configurations"]), 7)
        self.assertTrue(plan["preflight"]["enabled"])
        self.assertEqual({step["result"] for item in plan["configurations"] for step in item["steps"]}, {"not-run"})

    def test_invalid_cli_selector_or_browser_does_not_run_a_preflight(self):
        for arguments in ([], ["--area", "audio"], ["--area", "render", "--path", "engine/render"]):
            with self.subTest(arguments=arguments), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(planner.main(arguments, root=self.root,
                                             inspector=lambda _: self.fail("preflight called")), 2)
        for browser in ("", "relative/chrome.exe"):
            with self.subTest(browser=browser):
                code, _, errors = self.invoke(["--area", "render", "--browser", browser])
                self.assertEqual(code, 2)
                self.assertIn("absolute path", errors)


if __name__ == "__main__":
    unittest.main()
