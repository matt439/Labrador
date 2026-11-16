"""Build-cost semantics against retained, compiler-free Ninja/dep fixtures."""
from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.tests.tool_fixtures import load_tool

cost = load_tool("tools/build_cost.py")


def context():
    source = {"head": "a" * 40, "dirty": False}
    return {"build_directory": "C:/build", "source_directory": "C:/src", "source": source,
            "source_after": copy.deepcopy(source), "machine": {"cpu": "fixture"},
            "configuration": {"type": "Debug"}, "tools": {"compiler": "fixture"},
            "build_environment": {"CL": None, "_CL_": None},
            "targets": ["Tests"], "parallelism": 2, "cache_context": "clean outputs; warm dependencies",
            "clean_first": False, "exit_code": 0, "build_wall_seconds": 0.2}


def database():
    return json.dumps([
        {"directory": "C:/build", "file": "C:/src/engine/a.cpp", "output": "a.obj", "command": "cl /Foa.obj a.cpp"},
        {"directory": "C:/build", "file": "C:/src/engine/b.cpp", "output": "b.obj", "arguments": ["cl", "b.cpp"]},
    ])


def log(*rows, version=7):
    return f"# ninja log v{version}\n" + "".join("\t".join(map(str, row)) + "\n" for row in rows)


class BuildCostTests(unittest.TestCase):
    def test_parallel_compiler_sums_are_not_wall_or_cpu_and_multiple_outputs_do_not_double_them(self):
        recorded = log((0, 100, 0, "a.obj", "aa"), (0, 100, 0, "a.pdb", "aa"),
                       (20, 120, 0, "b.obj", "bb"), (130, 160, 0, "game.exe", "cc"))
        report = cost.analyze(recorded, database(), "", context())
        self.assertEqual(report["compiler_invocation_edge_elapsed_ms_sum"], 200)
        self.assertEqual(report["recorded_edge_span_ms"], 160)
        self.assertEqual(report["context"]["build_wall_seconds"], 0.2)
        self.assertIsNone(report["compiler_process_cpu_seconds"])
        self.assertEqual(len(report["compilations"]), 2)
        self.assertEqual(report["compilations"][0]["outputs"], ["c:/build/a.obj", "c:/build/a.pdb"])
        self.assertEqual(report["folders"], [{"folder": "engine", "elapsed_ms_sum": 200}])
        self.assertEqual(report["other_interval_groups"][0]["kind"], "link/post-link")

    def test_header_fanout_preserves_windows_colons_spaces_and_deduplicates_objects(self):
        deps = ("a.obj: #deps 3, deps mtime 123 (VALID)\n"
                "    C:\\src\\engine\\a.cpp\n    C:\\src\\shared.h\n    C:\\SDK Name\\vector\n\n"
                "b.obj: #deps 2, deps mtime 456 (VALID)\n"
                "    C:\\src\\engine\\b.cpp\n    C:\\src\\shared.h\n\n")
        report = cost.analyze(log((0, 5, 0, "a.obj", "aa"), (1, 7, 0, "b.obj", "bb")), database(), deps, context())
        self.assertEqual(report["header_fanout"], [{"header": "shared.h", "object_count": 2},
                                                 {"header": "c:/sdk name/vector", "object_count": 1}])
        self.assertEqual(report["dependency_coverage"]["missing_or_stale"], [])

    def test_stale_and_missing_dependencies_are_explicit_and_never_inflate_fanout(self):
        deps = "a.obj: #deps 1, deps mtime 1 (STALE)\n    C:/src/old.h\n\n"
        report = cost.analyze(log((0, 5, 0, "a.obj", "aa"), (1, 7, 0, "b.obj", "bb")), database(), deps, context())
        self.assertEqual(report["header_fanout"], [])
        self.assertEqual([row["state"] for row in report["dependency_coverage"]["missing_or_stale"]], ["stale", "missing"])

    def test_append_selection_drops_all_prior_builds_and_accepts_an_empty_noop(self):
        before = log((0, 20, 0, "a.obj", "aa")).encode()
        after = before + b"0\t30\t0\tb.obj\tbb\n"
        self.assertEqual(cost.parse_log(cost.appended_log(before, after))[0]["output"], "b.obj")
        self.assertEqual(cost.parse_log(cost.appended_log(before, before)), [])
        self.assertEqual(cost.parse_log(cost.appended_log(b"", before))[0]["end_ms"], 20)
        for changed in (log((0, 20, 0, "b.obj", "aa")).encode(), b"", b"# ninja log v7\n"):
            with self.subTest(changed=changed), self.assertRaises(ValueError):
                cost.appended_log(before, changed)

    def test_malformed_and_partial_logs_and_dependency_blocks_fail_loudly(self):
        for text in ("", "# ninja log v9\n", log((20, 10, 0, "a.obj", "aa")),
                     "# ninja log v7\n0\t1\t0\ta.obj\taa", "# ninja log v7\nbroken\n"):
            with self.subTest(text=text), self.assertRaises(ValueError):
                cost.parse_log(text)
        for text in ("a.obj: #deps 2, deps mtime 0 (VALID)\n    only.h\n\n", "unrecognised\n"):
            with self.subTest(text=text), self.assertRaises(ValueError):
                cost.parse_dependencies(text, "C:/build")
        for text in ("a.obj: #deps 1, deps mtime 0 (VALID)\n    \n\n",
                     "a.obj: #deps 0, deps mtime 0 (STALE)\n\nA.OBJ: #deps 0, deps mtime 0 (VALID)\n\n"):
            with self.subTest(text=text), self.assertRaises(ValueError):
                cost.parse_dependencies(text, "C:/build")
        for version in (5, 6, 7):
            self.assertEqual(cost.parse_log(log((1, 2, -1, "a.obj", "f"), version=version))[0]["end_ms"], 2)

    def test_ambiguous_edges_and_repeated_output_intervals_are_refused(self):
        for recorded in (log((0, 10, 0, "a.obj", "aa"), (0, 10, 0, "b.obj", "aa")),
                         log((0, 10, 0, "a.obj", "aa"), (0, 20, 0, "a.obj", "aa"))):
            with self.subTest(recorded=recorded), self.assertRaises(ValueError):
                cost.analyze(recorded, database(), "", context())

    def test_repeated_sources_in_different_objects_remain_separate_compilations(self):
        entries = json.loads(database())
        entries[1]["file"] = entries[0]["file"]
        report = cost.analyze(log((0, 10, 0, "a.obj", "aa"), (5, 25, 0, "b.obj", "bb")),
                              json.dumps(entries), "", context())
        self.assertEqual(len(report["compilations"]), 2)
        self.assertEqual(report["compiler_invocation_edge_elapsed_ms_sum"], 30)

    def test_output_flags_are_not_guessed_and_unmapped_object_rows_are_named(self):
        with self.assertRaisesRegex(ValueError, "explicit"):
            cost.parse_compilation_database('[{"directory":"/b", "file":"/s/a.cpp", "command":"cl /Foa.obj"}]')
        report = cost.analyze(log((0, 1, 0, "not-in-compdb.obj", "aa")), database(), "", context())
        self.assertEqual(report["other_interval_groups"][0]["kind"], "unmapped-object")
        self.assertEqual(report["compilations"], [])

    def test_comparison_checks_context_source_changes_failures_and_actual_workload(self):
        first = cost.analyze(log((0, 10, 0, "a.obj", "aa")), database(), "", context())
        second = copy.deepcopy(first)
        second["context"]["build_wall_seconds"] = 0.3
        self.assertTrue(cost.compare(first, second)["comparable"])
        self.assertAlmostEqual(cost.compare(first, second)["deltas"]["build_wall_seconds"]["ratio"], 1.5)
        for field in ("source", "machine", "configuration", "tools", "build_environment", "targets", "parallelism", "cache_context", "clean_first"):
            changed = copy.deepcopy(second)
            changed["context"][field] = "different"
            self.assertFalse(cost.compare(first, changed)["comparable"], field)
        second["context"]["source_after"] = {"head": "b" * 40}
        self.assertFalse(cost.compare(first, second)["comparable"])
        second = copy.deepcopy(first)
        second["compilations"] = []
        self.assertIn("recorded output workload", cost.compare(first, second)["context_differences"])
        second = copy.deepcopy(first)
        second["context"]["exit_code"] = 1
        self.assertFalse(cost.compare(first, second)["comparable"])
        second["context"]["exit_code"] = False
        self.assertFalse(cost.compare(first, second)["comparable"])
        for field in ("machine", "configuration", "tools"):
            changed = copy.deepcopy(first)
            changed["context"][field] = {}
            self.assertFalse(cost.compare(changed, changed)["comparable"])
        changed = copy.deepcopy(first)
        changed["context"]["build_environment"]["_CL_"] = "/O2"
        self.assertIn("build_environment", cost.compare(first, changed)["context_differences"])

    def test_empty_compiler_commands_do_not_count_as_compiler_invocations(self):
        for command in ("", "   ", [], [""]):
            entries = json.loads(database())
            entries[0]["command"] = command
            with self.subTest(command=command), self.assertRaises(ValueError):
                cost.parse_compilation_database(json.dumps(entries))

    def test_empty_or_missing_wall_and_zero_denominators_are_not_invented(self):
        info = context()
        info["build_wall_seconds"] = None
        report = cost.analyze(log(), database(), "", info)
        self.assertEqual(report["recorded_edge_span_ms"], 0)
        self.assertIsNone(report["compiler_process_cpu_seconds"])
        self.assertNotIn("build_wall_seconds", cost.compare(report, report)["deltas"])
        self.assertIsNone(cost.compare(report, report)["deltas"]["compiler_invocation_edge_elapsed_ms_sum"]["ratio"])
        for value in (-1, float("nan"), True):
            info["build_wall_seconds"] = value
            with self.assertRaises(ValueError):
                cost.analyze(log(), database(), "", info)

    def test_capture_hashes_and_configuration_are_verified_before_reporting(self):
        with tempfile.TemporaryDirectory() as name:
            folder = Path(name)
            data = {filename: b"same" for filename in cost.INPUTS}
            data.update({"ninja.before.log": b"", "ninja.after.log": log().encode(),
                         "compile_commands.before.json": database().encode(),
                         "compile_commands.after.json": database().encode(), "dependencies.txt": b""})
            def write():
                for filename, content in data.items():
                    (folder / filename).write_bytes(content)
                (folder / "receipt.json").write_text(json.dumps({"schema_version": 1, "context": context(),
                      "sha256": {filename: cost.digest(content) for filename, content in data.items()}}))
            write()
            self.assertEqual(cost.load_report(folder)["compilations"], [])
            (folder / "ninja.after.log").write_bytes(b"tampered")
            with self.assertRaisesRegex(ValueError, "hash mismatch"):
                cost.load_report(folder)
            data["rules.after.ninja"] = b"changed flags"
            write()
            with self.assertRaisesRegex(ValueError, "changed during"):
                cost.load_report(folder)

    def test_recording_without_execute_never_starts_a_process_or_creates_output(self):
        with patch.object(cost.subprocess, "run") as run:
            self.assertEqual(cost.main(["record", "--build", "absent", "--output", "absent",
                                       "--target", "Tests", "--cache-context", "unknown"]), 2)
            run.assert_not_called()


if __name__ == "__main__":
    unittest.main()
