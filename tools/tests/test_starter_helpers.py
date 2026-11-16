"""The assembled native helpers through fake, independently executed Windows tools."""

from __future__ import annotations

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import textwrap
import unittest

REPOSITORY = Path(__file__).resolve().parents[2]
HELPERS = REPOSITORY / "website/src/walkthrough/helpers"
POWERSHELL = shutil.which("powershell")

FAKE_TOOL = r'''
using System;
using System.IO;
using System.Reflection;
class FakeTool {
    static int Main(string[] args) {
        Console.OutputEncoding = new System.Text.UTF8Encoding(false);
        string file = Assembly.GetExecutingAssembly().Location;
        string name = Path.GetFileNameWithoutExtension(file).ToLowerInvariant();
        if (name == "vswhere") {
            Console.Write(File.ReadAllText(Path.ChangeExtension(file, ".json")));
            return Int32.Parse(Environment.GetEnvironmentVariable("FAKE_DISCOVERY_EXIT") ?? "0");
        }
        string phase = name == "cmake" ? (args.Length > 0 && args[0] == "--build" ? "build" : "configure") :
                       name == "ctest" ? "test" : "play";
        string log = Environment.GetEnvironmentVariable("FAKE_COMMAND_LOG");
        bool first = !File.Exists(log);
        File.AppendAllText(log,
            phase + "|" + String.Join(" ", args) + "|" + Environment.CurrentDirectory + "|" +
            Environment.GetEnvironmentVariable("TEST_ENV_MARKER") + "\n");
        string receipt = Environment.GetEnvironmentVariable("FAKE_ENV_RECEIPT");
        if (receipt != null) File.WriteAllText(receipt, Environment.GetEnvironmentVariable("FAKE_PRIVATE_VALUE"));
        Console.Out.WriteLine("stdout: " + phase);
        Console.Error.WriteLine("stderr: " + phase);
        string firstExit = Environment.GetEnvironmentVariable("FAKE_FIRST_EXIT");
        if (first && firstExit != null) return Int32.Parse(firstExit);
        if (Environment.GetEnvironmentVariable("FAKE_LARGE_OUTPUT") == "1") {
            for (int index = 0; index < 512; ++index) {
                Console.Out.WriteLine("out " + index + new String('a', 200));
                Console.Error.WriteLine("err " + index + new String('b', 200));
            }
        }
        return Int32.Parse(Environment.GetEnvironmentVariable("FAKE_" + phase.ToUpperInvariant() + "_EXIT") ?? "0");
    }
}
'''


@unittest.skipUnless(sys.platform == "win32" and POWERSHELL, "Windows PowerShell exercises cmd/native process semantics")
class StarterHelperTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.compiled = tempfile.TemporaryDirectory(prefix="labrador-helper-tool-")
        cls.binary = Path(cls.compiled.name) / "tool.exe"
        source = Path(cls.compiled.name) / "tool.cs"
        source.write_text(FAKE_TOOL, encoding="utf-8")
        compile_script = Path(cls.compiled.name) / "compile.ps1"
        compile_script.write_text(
            "$ErrorActionPreference = 'Stop'\n"
            "Add-Type -Path $args[0] -OutputAssembly $args[1] -OutputType ConsoleApplication\n", encoding="utf-8")
        result = subprocess.run([POWERSHELL, "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass",
                                 "-File", str(compile_script), str(source), str(cls.binary)], capture_output=True, timeout=60)
        if result.returncode:
            cls.compiled.cleanup()
            raise AssertionError(result.stderr.decode(errors="replace"))

    @classmethod
    def tearDownClass(cls):
        cls.compiled.cleanup()

    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="labrador-helper-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.project = self.root / "Project & %literal% with spaces"
        self.project.mkdir()
        self.installation = self.root / "Non default VS & %literal%"
        self.program_files = self.root / "Program Files (x86)"
        self.vswhere = self.program_files / "Microsoft Visual Studio/Installer/vswhere.exe"
        self.copy_tool(self.vswhere)
        self.discovery = self.vswhere.with_suffix(".json")
        self.discovery.write_text(json.dumps([{"installationPath": str(self.installation),
                                               "installationVersion": "18.1.2.3"}]), encoding="utf-8")
        for name in ("cmake", "ctest"):
            self.copy_tool(self.installation / f"Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/{name}.exe")
        self.copy_tool(self.installation / "Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe")
        self.vcvars = self.installation / "VC/Auxiliary/Build/vcvars64.bat"
        self.vcvars.parent.mkdir(parents=True)
        self.vcvars.write_text('@echo off\nset "TEST_ENV_MARKER=activated %%literal%% & equals=value"\n'
                               'if defined FAKE_VCVARS_EXIT exit /b %FAKE_VCVARS_EXIT%\nexit /b 0\n', encoding="ascii")
        toolchain = self.installation / "VC/vcpkg/scripts/buildsystems/vcpkg.cmake"
        toolchain.parent.mkdir(parents=True)
        toolchain.write_text("# fixture\n", encoding="utf-8")
        (self.project / "game").mkdir()
        (self.project / "game/CMakeLists.txt").write_text("# fixture\n", encoding="utf-8")
        (self.project / "CMakeLists.txt").write_text("project(RenamedGame)\n", encoding="utf-8")
        (self.project / "CMakePresets.json").write_text("{}\n", encoding="utf-8")
        for preset in ("x64-debug", "x64-release"):
            build = self.project / f"out/build/{preset}"
            build.mkdir(parents=True)
            (build / "CMakeCache.txt").write_text("CMAKE_PROJECT_NAME:STATIC=RenamedGame\n", encoding="utf-8")
            self.copy_tool(build / "game/RenamedGame.exe")
        for helper in ("build.cmd", "test.cmd", "play.cmd"):
            shutil.copy2(HELPERS / helper, self.project / helper)
        (self.project / "tools").mkdir()
        shutil.copy2(HELPERS / "starter_commands.ps1", self.project / "tools/starter_commands.ps1")
        self.log = self.root / "commands.txt"
        self.environment = {**os.environ, "ProgramFiles(x86)": str(self.program_files), "VCPKG_ROOT": "",
                            "FAKE_COMMAND_LOG": str(self.log), "TEST_ENV_MARKER": "not activated"}
        for name in list(self.environment):
            if name.startswith("FAKE_") and name != "FAKE_COMMAND_LOG":
                del self.environment[name]

    def copy_tool(self, path):
        path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(self.binary, path)

    def run_helper(self, helper, *arguments, **environment):
        return subprocess.run([os.environ["COMSPEC"], "/d", "/c", helper + ".cmd", *arguments],
                              cwd=self.project, env={**self.environment, **environment},
                              capture_output=True, text=True, encoding="utf-8", errors="replace",
                              stdin=subprocess.DEVNULL, timeout=60)

    def phases(self):
        return [] if not self.log.exists() else [line.split("|", 1)[0] for line in self.log.read_text(encoding="utf-8").splitlines()]

    def test_build_uses_shared_discovery_x64_environment_and_literal_project_path(self):
        result = self.run_helper("build")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(self.phases(), ["configure", "build"])
        calls = self.log.read_text(encoding="utf-8")
        self.assertIn(str(self.project), calls)
        self.assertIn("activated %literal% & equals=value", calls)
        self.assertIn("> cmake --preset x64-debug", result.stdout)
        self.assertIn("> cmake --build --preset x64-debug", result.stdout)
        self.assertIn("stdout: configure", result.stdout)
        self.assertIn("stderr: configure", result.stderr)

    def test_release_tests_and_renamed_game_play_use_the_selected_preset(self):
        result = self.run_helper("test", "-Preset", "x64-release")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(self.phases(), ["configure", "build", "test"])
        self.assertIn("> ctest --preset x64-release", result.stdout)
        result = self.run_helper("play", "-SmokeTest")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(self.phases()[-3:], ["configure", "build", "play"])
        self.assertIn("RenamedGame.exe --smoke-test", result.stdout)
        self.assertIn("play|--smoke-test|", self.log.read_text(encoding="utf-8"))

    def test_each_tool_failure_keeps_its_real_exit_and_stops_the_plan(self):
        for phase, code, helper, expected in (("CONFIGURE", 17, "test", ["configure"]),
                                             ("BUILD", 23, "test", ["configure", "build"]),
                                             ("TEST", 29, "test", ["configure", "build", "test"]),
                                             ("PLAY", 31, "play", ["configure", "build", "play"])):
            with self.subTest(phase=phase):
                self.log.unlink(missing_ok=True)
                result = self.run_helper(helper, **{f"FAKE_{phase}_EXIT": str(code)})
                self.assertEqual(result.returncode, code, result.stdout + result.stderr)
                self.assertEqual(self.phases(), expected)

    def test_missing_discovery_installation_tools_and_vcpkg_fail_without_retry(self):
        self.discovery.write_text("[]", encoding="utf-8")
        result = self.run_helper("build")
        self.assertEqual(result.returncode, 2)
        self.assertIn("No suitable Visual Studio", result.stderr)
        self.assertEqual(self.phases(), [])
        self.discovery.write_text("invalid", encoding="utf-8")
        result = self.run_helper("build")
        self.assertEqual(result.returncode, 2)
        self.assertIn("discovery returned invalid data", result.stderr)
        self.vswhere.unlink()
        result = self.run_helper("build")
        self.assertEqual(result.returncode, 2)
        self.assertIn("vswhere.exe", result.stderr)
        self.assertEqual(self.phases(), [])

    def test_discovery_and_vcvars_failures_keep_their_real_exit_codes(self):
        result = self.run_helper("build", FAKE_DISCOVERY_EXIT="19")
        self.assertEqual(result.returncode, 19)
        self.assertIn("discovery failed (exit 19)", result.stderr)
        result = self.run_helper("build", FAKE_VCVARS_EXIT="21")
        self.assertEqual(result.returncode, 21, result.stdout + result.stderr)
        self.assertIn("environment setup failed (exit 21)", result.stderr)
        self.assertEqual(self.phases(), [])

    def test_explicit_invalid_vcpkg_does_not_fall_back_to_the_bundled_copy(self):
        result = self.run_helper("build", VCPKG_ROOT=str(self.root / "missing vcpkg"))
        self.assertEqual(result.returncode, 2)
        self.assertIn("Correct VCPKG_ROOT", result.stderr)
        self.assertEqual(self.phases(), [])

    def test_missing_cmake_is_named_before_any_build_command_runs(self):
        (self.installation / "Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe").unlink()
        result = self.run_helper("build")
        self.assertEqual(result.returncode, 2)
        self.assertIn("missing CMake", result.stderr)
        self.assertIn("C++ CMake tools for Windows", result.stderr)
        self.assertEqual(self.phases(), [])

    def test_environment_values_are_preserved_without_dumping_them(self):
        receipt = self.root / "private-value.txt"
        value = 'fixture-only\npercent% & equals= "quoted"'
        result = self.run_helper("build", FAKE_ENV_RECEIPT=str(receipt), FAKE_PRIVATE_VALUE=value)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(receipt.read_text(encoding="utf-8"), value)
        self.assertNotIn("fixture-only", result.stdout + result.stderr)

    def test_unicode_discovery_is_decoded_as_utf8_in_an_oem_prompt(self):
        renamed = self.root / "Non default VS Caf\u00e9"
        self.installation.rename(renamed)
        self.discovery.write_text(json.dumps([{"installationPath": str(renamed),
                                               "installationVersion": "18.1.2.3"}], ensure_ascii=False), encoding="utf-8")
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = subprocess.SW_HIDE
        result = subprocess.run([os.environ["COMSPEC"], "/d", "/c", "chcp 437 >nul && build.cmd"],
                                cwd=self.project, env=self.environment, capture_output=True,
                                stdin=subprocess.DEVNULL, timeout=60, startupinfo=startup,
                                creationflags=subprocess.CREATE_NEW_CONSOLE)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(self.phases(), ["configure", "build"])

    def test_unsupported_presets_and_smoke_action_fail_before_running_tools(self):
        result = self.run_helper("build", "-Preset", "wasm-debug")
        self.assertNotEqual(result.returncode, 0)
        result = self.run_helper("test", "-SmokeTest")
        self.assertEqual(result.returncode, 2)
        self.assertEqual(self.phases(), [])

    def test_both_output_pipes_are_drained_without_losing_failure_output(self):
        result = self.run_helper("build", FAKE_LARGE_OUTPUT="1", FAKE_CONFIGURE_EXIT="27")
        self.assertEqual(result.returncode, 27)
        self.assertIn("out 511", result.stdout)
        self.assertIn("err 511", result.stderr)
        self.assertEqual(self.phases(), ["configure"])

    def test_the_actual_ci_step_stops_after_a_negative_windows_crash_code(self):
        workflow = (REPOSITORY / ".github/workflows/ci.yml").read_text(encoding="utf-8")
        step = workflow.split("      - name: Run the starter's optional native command helpers\n", 1)[1]
        block = step.split("        run: |\n", 1)[1].split("\n\n", 1)[0]
        (self.project / "helper-ci.cmd").write_text("@echo off\n" + textwrap.dedent(block) + "\n", encoding="ascii")
        crash = -1073741819
        result = self.run_helper("helper-ci", FAKE_FIRST_EXIT=str(crash))
        self.assertEqual(result.returncode & 0xffffffff, crash & 0xffffffff, result.stdout + result.stderr)
        self.assertEqual(self.phases(), ["configure"])


if __name__ == "__main__":
    unittest.main()
