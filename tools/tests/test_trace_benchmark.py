"""Exercise the real PowerShell wrapper with harmless native subprocess fixtures."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


@unittest.skipUnless(os.name == "nt", "Windows PowerShell process integration")
class TraceBenchmarkTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.compiler = Path(os.environ["WINDIR"]) / "Microsoft.NET/Framework64/v4.0.30319/csc.exe"
        if not cls.compiler.exists():
            raise unittest.SkipTest(".NET Framework C# compiler unavailable")
        cls.class_temp = tempfile.TemporaryDirectory(prefix="labrador-trace-fixture-")
        cls.fixture = Path(cls.class_temp.name) / "fixture.exe"
        source = Path(cls.class_temp.name) / "fixture.cs"
        source.write_text(r'''
using System;
using System.IO;
using System.Threading;
using System.Web.Script.Serialization;
using System.Collections.Generic;
class Fixture {
    static string Value(string[] args, string key) {
        return args[Array.IndexOf(args, key) + 1];
    }
    static int Main(string[] args) {
        var json = new JavaScriptSerializer();
        File.AppendAllText(Environment.GetEnvironmentVariable("TRACE_FIXTURE_LOG"),
            json.Serialize(args) + "\n");
        string mode = Environment.GetEnvironmentVariable("TRACE_FIXTURE_MODE");
        if (args[0] == "-status") { Console.WriteLine("Unrelated session present"); return 0; }
        if (args[0] == "-profiles") { Console.WriteLine("  GPU  GPU activity"); return 0; }
        if (args[0] == "-start") {
            if (mode == "record-failure") Directory.CreateDirectory(Path.Combine(
                Environment.GetEnvironmentVariable("TRACE_FIXTURE_OUTPUT"), "benchmark.process.json"));
            return mode == "start-failure" ? 7 : 0;
        }
        if (args[0] == "-stop") {
            if (mode == "stop-failure") return 9;
            File.WriteAllText(args[1], "fake trace"); return 0;
        }
        if (mode == "benchmark-failure") return 8;
        if (mode == "timeout" || mode == "record-failure") Thread.Sleep(10000);
        int count = Int32.Parse(Value(args, "--sample"));
        var samples = new object[count];
        for (int i = 0; i < count; ++i) samples[i] = new { index = i };
        var result = new {
            schema_version = 1, scope = "linesweeper_frame_benchmark", status = "complete",
            run = Value(args, "--run"), release_sha256 = Value(args, "--release-hash"),
            build = new { configuration = mode == "debug" ? "debug" : "release", render_backend = "vulkan" },
            measurement_class = "hardware_raster",
            workload = new { warmup_frames = Int32.Parse(Value(args, "--warmup")),
                sample_frames = count, refresh_hz = Int32.Parse(Value(args, "--refresh")) },
            timing = new { sample_count = count, pacer = Array.IndexOf(args, "--pacing") >= 0
                ? "presentation_driven" : "win32_high_resolution_waitable_timer" },
            samples = samples
        };
        File.WriteAllText(Value(args, "--output"), json.Serialize(result));
        Console.WriteLine("benchmark stdout"); Console.Error.WriteLine("benchmark stderr");
        return 0;
    }
}
''', encoding="utf-8")
        subprocess.run([str(cls.compiler), "/nologo", "/r:System.Web.Extensions.dll",
                        f"/out:{cls.fixture}", str(source)], check=True, capture_output=True)
        cls.script = Path(__file__).parents[1] / "cloud_performance/trace_benchmark.ps1"

    @classmethod
    def tearDownClass(cls):
        cls.class_temp.cleanup()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="labrador trace ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.bundle = self.root / "bundle"
        executable = self.bundle / "payload/vulkan/LineSweeperFrameBench.exe"
        executable.parent.mkdir(parents=True)
        shutil.copyfile(self.fixture, executable)
        member = executable.relative_to(self.bundle).as_posix()
        manifest = {"schema_version": 1, "scope": "labrador-cloud-performance-payload", "files": {
            member: {"bytes": executable.stat().st_size,
                     "sha256": hashlib.sha256(executable.read_bytes()).hexdigest()}}}
        release = self.bundle / "release.json"
        release.write_text(json.dumps(manifest), encoding="utf-8")
        self.release_hash = hashlib.sha256(release.read_bytes()).hexdigest()
        self.output = self.root / "capture output"
        self.log = self.root / "calls.jsonl"

    def run_capture(self, mode="success", extra=()):
        env = os.environ.copy()
        env.update(TRACE_FIXTURE_LOG=str(self.log), TRACE_FIXTURE_MODE=mode,
                   TRACE_FIXTURE_OUTPUT=str(self.output))
        return subprocess.run([
            "powershell.exe", "-NoProfile", "-NonInteractive", "-File", str(self.script),
            "-BundleRoot", str(self.bundle), "-ReleaseSHA256", self.release_hash,
            "-Backend", "vulkan", "-OutputDirectory", str(self.output),
            "-WprExecutable", str(self.fixture), "-Warmup", "1", "-Sample", "2",
            "-TimeoutSeconds", "1", *extra], env=env, capture_output=True, text=True, timeout=30)

    def calls(self):
        return [json.loads(line) for line in self.log.read_text().splitlines()] if self.log.exists() else []

    def capture(self):
        return json.loads((self.output / "capture.json").read_text(encoding="utf-8-sig"))

    def assert_owned_stop(self):
        calls = self.calls()
        starts = [call for call in calls if call[0] == "-start"]
        stops = [call for call in calls if call[0] == "-stop"]
        self.assertEqual(len(starts), 1)
        self.assertEqual(len(stops), 1)
        self.assertEqual(starts[0][-2:], stops[0][-2:])
        self.assertEqual(starts[0][-2], "-instancename")
        self.assertTrue(starts[0][-1].startswith("LabradorGpu-"))
        self.assertFalse(any("-cancel" in call for call in calls))

    def test_success_preserves_evidence_and_named_session(self):
        result = self.run_capture(extra=("-Pacing", "presentation"))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(self.capture()["status"], "complete")
        self.assert_owned_stop()
        request = json.loads((self.output / "request.json").read_text(encoding="utf-8-sig"))
        self.assertIn("--pacing", request["benchmark_arguments"])
        self.assertTrue((self.output / "gpu.etl").exists())
        self.assertTrue((self.output / "benchmark.stderr.log").read_text().strip())

    def test_start_failure_never_runs_benchmark_and_cleans_only_its_name(self):
        result = self.run_capture("start-failure")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual([call[0] for call in self.calls()], ["-status", "-profiles", "-start", "-stop"])
        self.assert_owned_stop()
        self.assertFalse(self.capture()["trace_started"])

    def test_benchmark_failure_still_saves_trace(self):
        result = self.run_capture("benchmark-failure")
        self.assertNotEqual(result.returncode, 0)
        self.assert_owned_stop()
        self.assertTrue(self.capture()["trace_saved"])
        self.assertEqual(self.capture()["benchmark_exit_code"], 8)

    def test_timeout_kills_only_benchmark_and_saves_trace(self):
        result = self.run_capture("timeout")
        self.assertNotEqual(result.returncode, 0)
        self.assert_owned_stop()
        self.assertTrue(self.capture()["trace_saved"])
        self.assertIn("deadline", self.capture()["failure"])

    def test_stop_failure_preserves_named_recovery(self):
        result = self.run_capture("stop-failure")
        self.assertNotEqual(result.returncode, 0)
        self.assert_owned_stop()
        self.assertFalse(self.capture()["trace_saved"])
        self.assertIn("stop exit code", self.capture()["failure"])

    def test_process_record_failure_kills_started_child_and_saves_trace(self):
        result = self.run_capture("record-failure")
        self.assertNotEqual(result.returncode, 0)
        self.assert_owned_stop()
        self.assertTrue(self.capture()["trace_saved"])
        self.assertIn("benchmark.process.json", self.capture()["failure"])
        env = os.environ.copy()
        env["TRACE_FIXTURE_EXE"] = str(self.bundle / "payload/vulkan/LineSweeperFrameBench.exe")
        remaining = subprocess.run([
            "powershell.exe", "-NoProfile", "-NonInteractive", "-Command",
            "@(Get-CimInstance Win32_Process | Where-Object { "
            "$_.ExecutablePath -eq $env:TRACE_FIXTURE_EXE }).Count"],
            env=env, capture_output=True, text=True, check=True, timeout=15)
        self.assertEqual(remaining.stdout.strip(), "0")

    def test_debug_capture_is_rejected_after_trace_saved(self):
        result = self.run_capture("debug")
        self.assertNotEqual(result.returncode, 0)
        self.assert_owned_stop()
        self.assertTrue(self.capture()["trace_saved"])
        self.assertIn("benchmark build", self.capture()["failure"])

    def test_changed_payload_is_rejected_before_any_process(self):
        executable = self.bundle / "payload/vulkan/LineSweeperFrameBench.exe"
        executable.write_bytes(executable.read_bytes() + b"changed")
        result = self.run_capture()
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.calls(), [])
        self.assertFalse(self.output.exists())

    def test_existing_directory_is_not_overwritten(self):
        self.output.mkdir()
        sentinel = self.output / "keep"
        sentinel.write_text("untouched")
        result = self.run_capture()
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.calls(), [])
        self.assertEqual(sentinel.read_text(), "untouched")


if __name__ == "__main__":
    unittest.main()
