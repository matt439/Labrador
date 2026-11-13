"""Pure plan and synthetic-path/cache fixtures; no configured tool is executed."""

from __future__ import annotations

import contextlib
import io
import json
from pathlib import Path
import unittest
from unittest import mock

from tools.tests.tool_fixtures import PlannerCheckout, load_tool

REPOSITORY = Path(__file__).resolve().parents[2]
planner = load_tool("tools/verify_changes.py")


class PlanTests(unittest.TestCase):
    def setUp(self):
        self.checkout = PlannerCheckout(self, planner)
        self.root = self.checkout.root

    def plan(self, **options):
        return self.checkout.plan(**options)

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
        return self.checkout.fake_tooling()

    def inspect(self, plan, environment, tools, **options):
        return self.checkout.inspect(plan, environment, tools, **options)

    def write_caches(self, plan, environment, browser):
        self.checkout.write_caches(plan, environment, browser)

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
        for path in ("", "engine/rendered/file.cpp", "engine/audios", "engine/scene", "samples/audio", "/engine/render", "C:\\engine\\render",
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
        self.assertIn("ComponentsPointerPageTests", gl["required_browser_test_names"])
        self.assertIn("ComponentsPointerPageTests", plan["configurations"][-1]["conditional_browser_test_names"])
        self.assertNotIn("LineSweeperBundlePageTests", plan["configurations"][-1]["conditional_browser_test_names"])
        self.assertIn("D3D12SynchronizationTests", plan["configurations"][1]["required_test_names"])
        for configuration in plan["configurations"][:4]:
            for name in ("RenderPixelTests", "MinimalSmokeTests", "CollisionSmokeTests"):
                self.assertIn(name, configuration["required_test_names"], configuration["id"])
        self.assertIn("RenderPixelTests", plan["configurations"][4]["forbidden_test_names"])
        self.assertIn("WebWindowTests", plan["configurations"][-1]["conditional_browser_test_names"])
        self.assertEqual(next(item for item in plan["manual_obligations"] if item["id"] == "vulkan-synchronization")["condition"], "required")
        self.assertEqual(next(item for item in self.plan()["manual_obligations"] if item["id"] == "vulkan-synchronization")["condition"], "conditional")
        self.assertTrue(all(item["status"] == "not-run" for item in plan["manual_obligations"]))
        self.assertIn("audio-backend-initialization", plan["configurations"][0]["steps"][2]["effects"])
        self.assertNotIn("audio-backend-initialization", plan["configurations"][4]["steps"][2]["effects"])
        self.assertIn("browser-audio-initialization-when-registered", plan["configurations"][-2]["steps"][2]["effects"])
        self.assertNotIn("browser-audio-initialization-when-registered", plan["configurations"][-1]["steps"][2]["effects"])

    def test_each_area_selects_its_tracks_in_sweep_order(self):
        self.assertEqual(self.plan()["scope"], {"areas": ["render"], "paths": []})
        for area, tracks in planner.AREAS.items():
            with self.subTest(area=area):
                plan = self.plan(areas=[area])
                self.assertEqual(plan["scope"], {"areas": [area], "paths": []})
                self.assertEqual([item["id"] for item in plan["configurations"]], list(tracks))
                self.assertEqual(plan["preflight"]["checks"][0]["configurations"], list(tracks))
        self.assertEqual([item["id"] for item in self.plan(areas=["audio"])["configurations"]],
                         ["x64-debug", "x64-debug-null", "wasm-debug", "wasm-release-null"])
        self.assertEqual([item["id"] for item in self.plan(areas=["app"])["configurations"]], list(planner.CONFIGURATIONS))
        with self.assertRaises(planner.PlanError):
            self.plan(areas=["scene"])
        with self.assertRaises(planner.PlanError):
            self.plan(areas=["audio"], paths=["engine/audio"])

    def test_paths_select_the_union_of_their_areas(self):
        plan = self.plan(paths=["engine\\audio\\xaudio2\\deleted.cpp", "engine/input/web", "engine/audio"])
        self.assertEqual(plan["scope"], {"areas": ["audio", "input"],
                                         "paths": ["engine/audio", "engine/audio/xaudio2/deleted.cpp", "engine/input/web"]})
        self.assertEqual([item["id"] for item in plan["configurations"]], list(planner.AUDIO_INPUT_TRACKS))
        mixed = self.plan(paths=["engine/app/win32/window.cpp", "engine/audio/sound_bank.cpp"])
        self.assertEqual(mixed["scope"]["areas"], ["app", "audio"])
        self.assertEqual([item["id"] for item in mixed["configurations"]], list(planner.CONFIGURATIONS))
        by_area = self.plan(areas=["audio", "app", "audio"])
        self.assertEqual(by_area["scope"]["areas"], ["app", "audio"])
        self.assertEqual(mixed["configurations"], by_area["configurations"])

    def test_area_registrations_join_on_shared_tracks(self):
        def required(plan, name):
            return next(item for item in plan["configurations"] if item["id"] == name)["required_test_names"]
        audio = self.plan(areas=["audio"])
        self.assertEqual(required(audio, "x64-debug"), ["AudioTests", "AudioSampleTests"])
        self.assertEqual(required(audio, "x64-debug-null"), ["AudioTests", "AudioSampleTests", "AudioSmokeTests"])
        self.assertIn("AudioPageTests", next(item for item in audio["configurations"]
                                             if item["id"] == "wasm-debug")["required_browser_test_names"])
        self.assertEqual(required(self.plan(areas=["input"]), "wasm-release-null"), ["InputTests"])
        app = self.plan(areas=["app"])
        self.assertEqual(required(app, "x64-debug-null"),
                         ["AppTests", "MinimalSmokeTests", "CollisionSmokeTests", "LocalMultiplayerSmokeTests"])
        self.assertEqual(required(app, "wasm-debug"), ["AppTests"])
        self.assertNotIn("AppTests", required(self.plan(), "x64-debug"))
        union = self.plan(areas=["render", "app"])
        self.assertEqual(required(union, "x64-debug"),
                         ["AppTests", "MinimalSmokeTests", "CollisionSmokeTests",
                          "RenderTests", "SceneTests", "AudioTests", "RenderPixelTests"])
        for item in union["configurations"]:
            self.assertEqual(len(item["required_test_names"]), len(set(item["required_test_names"])))

    def test_required_tests_must_be_in_the_inventory_for_their_track(self):
        path = self.root / "tests/ctest_inventory.json"
        data = json.loads(path.read_text(encoding="utf-8"))
        data["some_configurations"]["AudioSmokeTests"] = ["x64-release-null"]
        path.write_text(json.dumps(data), encoding="utf-8")
        with self.assertRaisesRegex(planner.PlanError, "AudioSmokeTests for x64-debug-null"):
            self.plan(areas=["audio"])
        self.assertEqual(len(self.plan()["configurations"]), 7)

    def test_area_manual_obligations_name_what_the_sweep_cannot_show(self):
        def ids(plan):
            return [item["id"] for item in plan["manual_obligations"]]
        audio = self.plan(areas=["audio"])
        self.assertEqual(ids(audio), ["release-configurations", "xaudio2-endpoint-smoke", "listening",
                                      "registration-before-future-execution", "other-verification"])
        self.assertEqual(audio["manual_obligations"][0]["detail"], "x64-release, x64-release-null, wasm-release")
        self.assertIn("WEB#3", audio["manual_obligations"][2]["detail"])
        self.assertIn("physical-input", ids(self.plan(areas=["input"])))
        self.assertNotIn("listening", ids(self.plan(areas=["input"])))
        self.assertEqual(ids(self.plan(areas=["app"])), ids(self.plan()))
        self.assertTrue(all(item["status"] == "not-run" for item in audio["manual_obligations"]))
        text = planner.format_text(self.plan(areas=["audio", "input"]))
        self.assertTrue(text.startswith("Verification plan for audio, input only"))

    def test_preflight_inspects_only_the_planned_tracks(self):
        environment, tools, browser = self.fake_tooling()
        environment.pop("VULKAN_SDK")
        plan = self.plan(areas=["input"], browser=browser)
        checks = self.inspect(plan, environment, tools)
        self.assertFalse([check for check in checks if check["blocking"]])
        self.assertFalse([check for check in checks if "VULKAN_SDK" in check["id"]])
        planned = set(planner.AUDIO_INPUT_TRACKS)
        self.assertTrue(all(check["configurations"] and set(check["configurations"]) <= planned for check in checks))
        self.assertTrue(any(check["blocking"] for check in self.inspect(self.plan(browser=browser), environment, tools)))

    def test_cli_repeats_areas_for_a_union(self):
        code, output, errors = self.invoke(["--area", "audio", "--area", "input", "--format", "json"])
        self.assertEqual((code, errors), (0, ""))
        plan = json.loads(output)
        self.assertEqual(plan["schema_version"], 2)
        self.assertEqual(plan["scope"]["areas"], ["audio", "input"])
        self.assertEqual(len(plan["configurations"]), 4)

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

    def test_native_host_condition_is_inherited_without_filtering_default_planning(self):
        document = json.loads((self.root / "CMakePresets.json").read_text(encoding="utf-8"))
        index = planner.preset_index(document, "configurePresets")
        self.assertEqual({name for name in index if name.startswith("x64-")}, set(planner.NATIVE_PRESETS))
        self.assertEqual(len(planner.NATIVE_PRESETS), 10)
        for name in planner.NATIVE_PRESETS:
            self.assertEqual(planner.configure_preset(index, name)["condition"], planner.NATIVE_HOST_CONDITION)
        for name in ("wasm-debug", "wasm-release"):
            self.assertNotIn("condition", planner.configure_preset(index, name))
        with mock.patch.object(planner.sys, "platform", "linux"):
            plan = self.plan()
        self.assertEqual([item["id"] for item in plan["configurations"]], list(planner.CONFIGURATIONS))
        self.assertEqual(len([step for item in plan["configurations"] for step in item["steps"]]), 21)
        self.assertEqual({step["result"] for item in plan["configurations"] for step in item["steps"]}, {"not-run"})

    def test_copied_project_presets_share_the_native_gate_without_restricting_wasm(self):
        document = json.loads((REPOSITORY / "website/src/walkthrough/CMakePresets.json").read_text(encoding="utf-8"))
        index = planner.preset_index(document, "configurePresets")
        self.assertEqual({name for name in index if name.startswith("x64-")}, {"x64-debug", "x64-release"})
        for name in ("x64-debug", "x64-release"):
            self.assertEqual(planner.configure_preset(index, name)["condition"], planner.NATIVE_HOST_CONDITION)
        for name in ("wasm-debug", "wasm-release"):
            self.assertNotIn("condition", planner.configure_preset(index, name))

    def test_missing_native_gate_and_release_overrides_fail_before_forming_a_plan(self):
        original = (self.root / "CMakePresets.json").read_bytes()
        mutations = [lambda data: data["configurePresets"][0].pop("condition"),
                     lambda data: data["configurePresets"].remove(next(
                         item for item in data["configurePresets"] if item["name"] == "x64-release-vulkan"))]
        for change in mutations:
            with self.subTest(change=change):
                self.change_presets(change)
                with self.assertRaises(planner.PlanError):
                    self.plan()
                (self.root / "CMakePresets.json").write_bytes(original)
        for condition in (None, False, True, "Windows",
                          {"type": "equals", "lhs": "${hostSystemName}", "rhs": "Linux"},
                          {"type": "allOf", "conditions": [planner.NATIVE_HOST_CONDITION]},
                          {**planner.NATIVE_HOST_CONDITION, "extra": True}):
            with self.subTest(condition=condition):
                self.change_presets(lambda data: next(
                    item for item in data["configurePresets"] if item["name"] == "x64-release-vulkan"
                ).update(condition=condition))
                with self.assertRaisesRegex(planner.PlanError, "exact Windows host condition"):
                    self.plan()
                (self.root / "CMakePresets.json").write_bytes(original)

    def test_wasm_cannot_inherit_or_add_the_native_host_gate(self):
        original = (self.root / "CMakePresets.json").read_bytes()
        mutations = [lambda data: next(
                         item for item in data["configurePresets"] if item["name"] == "wasm-base"
                     ).update(inherits="base"),
                     lambda data: next(
                         item for item in data["configurePresets"] if item["name"] == "wasm-debug"
                     ).update(condition=dict(planner.NATIVE_HOST_CONDITION)),
                     lambda data: next(
                         item for item in data["configurePresets"] if item["name"] == "wasm-release"
                     ).update(condition=dict(planner.NATIVE_HOST_CONDITION))]
        for change in mutations:
            with self.subTest(change=change):
                self.change_presets(change)
                with self.assertRaisesRegex(planner.PlanError, "must have no host condition"):
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

    def test_test_presets_inherit_output_only_from_hidden_output_presets(self):
        document = json.loads((self.root / "CMakePresets.json").read_text(encoding="utf-8"))
        index = planner.preset_index(document, "testPresets")
        for name in (*planner.NATIVE, "wasm-debug", "wasm-release"):
            self.assertEqual(planner.test_preset_output(index, name), {"outputOnFailure": True})
        mutations = [lambda data: next(item for item in data["testPresets"] if item["name"] == "test-base").pop("hidden"),
                     lambda data: next(item for item in data["testPresets"] if item["name"] == "test-base").update(
                         filter={"exclude": {"name": "Pixel"}}),
                     lambda data: next(item for item in data["testPresets"] if item["name"] == "test-base").update(
                         inherits="x64-debug"),
                     lambda data: data["testPresets"][0].update(inherits="missing"),
                     lambda data: data["testPresets"][0].update(inherits=["test-base"])]
        original = (self.root / "CMakePresets.json").read_bytes()
        for change in mutations:
            with self.subTest(change=change):
                self.change_presets(change)
                with self.assertRaises(planner.PlanError):
                    self.plan()
                (self.root / "CMakePresets.json").write_bytes(original)

    def test_null_render_exclusions_follow_page_registrations(self):
        path = self.root / "CMakeLists.txt"
        original = path.read_bytes()
        source = original.decode("utf-8")
        self.assertIn("labrador_page_test(LineSweeperPacingPageTests", source)
        for registration in ("labrador_page_test(LineSweeperPacingPageTests",
                             "labrador_page_test(LineSweeperBundlePageTests"):
            with self.subTest(registration=registration):
                path.write_text(source.replace(registration, "labrador_page_test(LineSweeperOtherPageTests"),
                                encoding="utf-8")
                with self.assertRaisesRegex(planner.PlanError, "null bundle/pacing exclusions changed"):
                    self.plan()
                path.write_bytes(original)
        unguarded = source.replace('if(NOT LABRADOR_RENDER_BACKEND STREQUAL "null")',
                                   'if(LABRADOR_RENDER_BACKEND)')
        path.write_text(unguarded, encoding="utf-8")
        with self.assertRaisesRegex(planner.PlanError, "null bundle/pacing exclusions changed"):
            self.plan()
        path.write_bytes(original)

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

    def test_inherited_msystem_blocks_the_planned_wasm_tracks(self):
        environment, tools, browser = self.fake_tooling()
        plan = self.plan(browser=browser)
        self.write_caches(plan, environment, browser)
        self.assertFalse(any(item["id"] == "MSYSTEM" for item in self.inspect(plan, environment, tools)))
        environment["MSYSTEM"] = "MINGW64"
        checks = self.inspect(plan, environment, tools)
        check = next(item for item in checks if item["id"] == "MSYSTEM")
        self.assertEqual(check["configurations"], list(planner.WASM_CONFIGURATIONS))
        self.assertTrue(check["blocking"])
        self.assertIn("run `set MSYSTEM=` before emsdk_env.bat", check["detail"])
        self.assertEqual([item["id"] for item in checks if item["blocking"]], ["MSYSTEM"])
        native = dict(plan, configurations=[item for item in plan["configurations"]
                                            if not item["id"].startswith("wasm-")])
        self.assertFalse(any(item["id"] == "MSYSTEM" for item in self.inspect(native, environment, tools)))

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

    def test_an_unreadable_cache_is_reported_once_rather_than_as_every_mismatch(self):
        environment, tools, browser = self.fake_tooling()
        plan = self.plan()
        self.write_caches(plan, environment, browser)
        path = self.root / "out/build/x64-debug/CMakeCache.txt"
        path.write_text(path.read_text(encoding="utf-8") + "LABRADOR_RENDER_BACKEND:STRING=gl\n",
                        encoding="utf-8")
        checks = self.inspect(plan, environment, tools)
        own = [item for item in checks if item["configurations"] == ["x64-debug"]]
        self.assertEqual([(item["id"], item["status"]) for item in own], [("x64-debug:cache", "missing")])
        self.assertIn("duplicate whitelisted cache key", own[0]["detail"])
        self.assertTrue(own[0]["blocking"])

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
        for arguments in ([], ["--area", "scene"], ["--area", "render", "--path", "engine/render"],
                          ["--area", "audio", "--path", "engine/audio"]):
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
