"""Tracked source enumeration, including paths that a basename scan confuses."""

from __future__ import annotations

import contextlib
import io
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

from tools.tests.tool_fixtures import load_tool

checker = load_tool("tools/check_source_enumeration.py")


class SourceEnumerationTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="labrador-source-enumeration-")
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)

    def check(self, files):
        for relative, contents in files.items():
            path = self.root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(contents, encoding="utf-8")
        return checker.check(self.root, list(files))

    def test_an_orphan_names_its_own_directorys_cmake_file(self):
        count, errors = self.check({"engine/CMakeLists.txt": "add_library(E real.cpp)",
                                   "engine/real.cpp": "", "engine/forgotten.cpp": ""})
        self.assertEqual(count, 2)
        self.assertEqual(len(errors), 1)
        self.assertIn("engine/forgotten.cpp", errors[0])
        self.assertIn("engine/CMakeLists.txt", errors[0])

    def test_duplicate_basenames_are_distinct_sources(self):
        _, errors = self.check({"engine/CMakeLists.txt": "add_library(E first/main.cpp)",
                               "engine/first/main.cpp": "", "engine/second/main.cpp": ""})
        self.assertEqual(len(errors), 1)
        self.assertIn("engine/second/main.cpp", errors[0])

    def test_relative_quoted_builtin_paths_and_target_sources(self):
        self.assertEqual(self.check({
            "engine/CMakeLists.txt": 'add_library(E one.cpp)\ntarget_sources(E PRIVATE "../tests/../tests/two.cpp")',
            "engine/one.cpp": "", "tests/two.cpp": "",
            "samples/CMakeLists.txt": 'add_executable(S "${CMAKE_CURRENT_SOURCE_DIR}/three.cpp")',
            "samples/three.cpp": ""})[1], [])

    def test_every_conditional_backend_is_counted_without_host_selection(self):
        self.assertEqual(self.check({
            "engine/CMakeLists.txt": "if(WIN32)\nadd_library(E native.cpp)\nelseif(EMSCRIPTEN)\n"
                                    "add_library(E web.cpp)\nelse()\nadd_library(E other.cpp)\nendif()",
            "engine/native.cpp": "", "engine/web.cpp": "", "engine/other.cpp": ""})[1], [])

    def test_local_lists_and_path_variables_are_resolved_before_consumption(self):
        self.assertEqual(self.check({
            "tests/CMakeLists.txt": 'set(folder "${CMAKE_CURRENT_LIST_DIR}/sub")\nset(parts one.cpp)\n'
                                   'set(alias ${parts})\nset(parts two.cpp)\nlist(APPEND parts "${folder}/three.cpp")\n'
                                   'add_executable(T ${alias} ${parts})',
            "tests/one.cpp": "", "tests/two.cpp": "", "tests/sub/three.cpp": ""})[1], [])

    def test_unused_or_overwritten_lists_do_not_satisfy_a_declaration(self):
        _, errors = self.check({
            "engine/CMakeLists.txt": "set(unused hidden.cpp)\nset(parts overwritten.cpp)\nset(parts real.cpp)\nadd_library(E ${parts})",
            "engine/hidden.cpp": "", "engine/overwritten.cpp": "", "engine/real.cpp": ""})
        self.assertEqual(len(errors), 2)
        self.assertTrue(any("engine/hidden.cpp" in error for error in errors))
        self.assertTrue(any("engine/overwritten.cpp" in error for error in errors))

    def test_branch_list_alternatives_merge_and_removal_is_respected(self):
        _, errors = self.check({
            "bench/CMakeLists.txt": "set(parts shared.cpp removed.cpp)\nlist(REMOVE_ITEM parts removed.cpp)\n"
                                   "if(WIN32)\nlist(APPEND parts native.cpp)\nelse()\nlist(APPEND parts web.cpp)\nendif()\n"
                                   "add_executable(B ${parts})",
            "bench/shared.cpp": "", "bench/removed.cpp": "", "bench/native.cpp": "", "bench/web.cpp": ""})
        self.assertEqual(len(errors), 1)
        self.assertIn("bench/removed.cpp", errors[0])

    def test_removal_arguments_expand_aliases_and_unset_discards_stale_sources(self):
        _, errors = self.check({
            "engine/CMakeLists.txt": "set(parts removed.cpp kept.cpp)\nset(exclude removed.cpp)\n"
                                    "list(REMOVE_ITEM parts ${exclude})\nadd_library(E ${parts})\n"
                                    "set(cleared stale.cpp)\nunset(cleared)\nadd_library(F ${cleared})",
            "engine/removed.cpp": "", "engine/kept.cpp": "", "engine/stale.cpp": ""})
        self.assertEqual(len(errors), 2)
        self.assertTrue(any("engine/removed.cpp: no explicit source declaration" in error for error in errors))
        self.assertTrue(any("engine/stale.cpp: no explicit source declaration" in error for error in errors))

    def test_configuration_sources_and_repository_test_wrapper(self):
        self.assertEqual(self.check({
            "tests/CMakeLists.txt": 'function(labrador_add_test_suite name)\nadd_executable(${name} ${suite_SOURCES} '
                                   '"${CMAKE_CURRENT_FUNCTION_LIST_DIR}/test_main.cpp")\nendfunction()\n'
                                   'labrador_add_test_suite(T SOURCES ordinary.cpp LINKS E)\n'
                                   'target_sources(T PRIVATE "$<$<CONFIG:Debug>:${CMAKE_CURRENT_SOURCE_DIR}/debug.cpp>")',
            "tests/test_main.cpp": "", "tests/ordinary.cpp": "", "tests/debug.cpp": ""})[1], [])

    def test_comments_non_source_commands_and_untracked_files_do_not_count(self):
        files = {"engine/CMakeLists.txt": '# add_library(E commented.cpp)\n#[=[\nadd_library(E bracket.cpp)\n]=]\n'
                                         'message("add_library(E quoted.cpp)")\nmessage(")" "(")\nadd_library(E real.cpp)',
                 "engine/commented.cpp": "", "engine/bracket.cpp": "", "engine/quoted.cpp": "", "engine/real.cpp": ""}
        (self.root / "untracked.cpp").write_text("", encoding="utf-8")
        count, errors = self.check(files)
        self.assertEqual(count, 4)
        self.assertEqual(len(errors), 3)

    def test_unknown_source_variable_and_generator_expression_are_actionable(self):
        _, errors = self.check({"engine/CMakeLists.txt": 'target_sources(E PRIVATE ${computed} "$<BUILD_INTERFACE:one.cpp>")',
                               "engine/one.cpp": ""})
        self.assertTrue(any("engine/CMakeLists.txt:1: unresolved source variable computed" in error for error in errors))
        self.assertTrue(any("unsupported source generator expression" in error for error in errors))

    def test_source_glob_is_refused_as_an_unresolved_source_list(self):
        _, errors = self.check({"engine/CMakeLists.txt": 'file(GLOB parts "*.cpp")\nadd_library(E ${parts})',
                               "engine/one.cpp": ""})
        self.assertTrue(any("source variable parts uses file(GLOB)" in error for error in errors))
        self.assertTrue(any("engine/one.cpp: no explicit source declaration" in error for error in errors))

    def test_outside_model_variable_operations_do_not_reuse_stale_lists(self):
        for operation in ('file(GLOB parts "*.cpp")', 'string(APPEND parts other.cpp)'):
            with self.subTest(operation=operation):
                _, errors = self.check({"engine/CMakeLists.txt": f"set(parts one.cpp)\n{operation}\nadd_library(E ${{parts}})",
                                       "engine/one.cpp": ""})
                self.assertTrue(any("source variable parts uses" in error for error in errors))
                self.assertTrue(any("engine/one.cpp: no explicit source declaration" in error for error in errors))

    def test_context_dependent_directory_variables_are_not_guessed(self):
        for declaration in ('project(Nested)\nadd_library(E "${PROJECT_SOURCE_DIR}/engine/one.cpp")',
                            'add_library(E "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/one.cpp")'):
            with self.subTest(declaration=declaration):
                _, errors = self.check({"engine/CMakeLists.txt": declaration, "engine/one.cpp": ""})
                self.assertTrue(any("engine/one.cpp: no explicit source declaration" in error for error in errors))

    def test_repository_dot_components_are_normalized_without_following_links(self):
        files = {"engine/CMakeLists.txt": "add_library(E one.cpp)", "engine/one.cpp": ""}
        self.check(files)
        self.assertEqual(checker.check(self.root / "engine" / "..", list(files)), (1, []))

    def test_functions_do_not_leak_local_lists_or_guess_caller_relative_paths(self):
        _, errors = self.check({"engine/CMakeLists.txt": 'function(helper)\nset(parts one.cpp)\n'
                               'add_library(E "${CMAKE_CURRENT_SOURCE_DIR}/one.cpp")\nendfunction()\n'
                               'add_library(F ${parts})', "engine/one.cpp": ""})
        self.assertTrue(any("depends on its caller" in error for error in errors))
        self.assertTrue(any("unresolved source variable parts" in error for error in errors))

    def test_bracket_source_arguments_are_refused_without_expanding_their_contents(self):
        _, errors = self.check({"engine/CMakeLists.txt": "set(part one.cpp)\nadd_library(E [=[${part}]=])",
                               "engine/one.cpp": ""})
        self.assertTrue(any("bracket source arguments are unsupported" in error for error in errors))
        self.assertTrue(any("engine/one.cpp: no explicit source declaration" in error for error in errors))

    def test_unsupported_list_mutation_and_file_sets_are_named(self):
        _, errors = self.check({"engine/CMakeLists.txt": 'set(parts one.cpp)\nlist(TRANSFORM parts PREPEND sub/)\n'
                               'add_library(E ${parts})\ntarget_sources(E PRIVATE FILE_SET modules TYPE CXX_MODULES FILES two.cpp)',
                               "engine/one.cpp": "", "engine/two.cpp": ""})
        self.assertTrue(any("list(TRANSFORM)" in error for error in errors))
        self.assertTrue(any("FILE_SET declarations are unsupported" in error for error in errors))

    def test_source_outside_the_checkout_and_broken_syntax_are_refused(self):
        _, errors = self.check({"engine/CMakeLists.txt": "add_library(E ../../escape.cpp)"})
        self.assertTrue(any("source path leaves the checkout" in error for error in errors))
        _, errors = self.check({"engine/CMakeLists.txt": "add_library(E one.cpp"})
        self.assertTrue(any("unterminated add_library" in error for error in errors))

    def test_cpp_files_outside_the_selected_roots_are_not_required(self):
        self.assertEqual(self.check({"tools/fixture.cpp": "", "external/vendor.cpp": ""}), (0, []))

    def test_cli_reads_tracked_paths_and_returns_failure_statuses(self):
        self.check({"engine/CMakeLists.txt": "add_library(E one.cpp)", "engine/one.cpp": ""})
        located = subprocess.CompletedProcess([], 0, (self.root.as_posix() + "\n").encode(), b"")
        tracked = subprocess.CompletedProcess([], 0, b"engine/CMakeLists.txt\0engine/one.cpp\0", b"")
        with mock.patch.object(checker.subprocess, "run", side_effect=[located, tracked]) as run, contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(checker.main(["--repo", str(self.root / "engine")]), 0)
            self.assertEqual(run.call_args_list[1].args[0][2], str(self.root))
        missing = subprocess.CompletedProcess([], 0, tracked.stdout + b"engine/missing.cpp\0", b"")
        with mock.patch.object(checker.subprocess, "run", side_effect=[located, missing]), contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(checker.main(["--repo", str(self.root)]), 1)
        with mock.patch.object(checker.subprocess, "run", side_effect=OSError("git unavailable")), contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(checker.main(["--repo", str(self.root)]), 2)


if __name__ == "__main__":
    unittest.main()
