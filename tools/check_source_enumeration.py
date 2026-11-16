"""Check tracked C++ source enumeration without configuring or compiling.

Reads tracked .cpp files under engine/, tests/, samples/ and bench/, and
explicit source declarations in tracked CMakeLists.txt files. Conditions are
not evaluated: all backend/platform branches count. See docs/source-enumeration.md
for the deliberately limited CMake syntax and what this check cannot prove.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import os
from pathlib import Path
import re
import subprocess
import sys

REPOSITORY = Path(__file__).resolve().parents[1]
ROOTS = ("engine/", "tests/", "samples/", "bench/")
VARIABLE = re.compile(r"\$\{([A-Za-z_][A-Za-z0-9_]*)\}")
CONFIGURATION = re.compile(r"\$<\$<CONFIG:[A-Za-z0-9_,]+>:(.+)>", re.DOTALL)


class EnumerationError(ValueError):
    pass


@dataclass(frozen=True)
class Token:
    value: str
    line: int
    kind: str = "word"


def tokens(text: str) -> list[Token]:
    """Lex the delimiters, quotes and comments needed by source declarations."""
    result = []
    index, line = 0, 1
    while index < len(text):
        start, at_line = index, line
        if text[index].isspace():
            line += text[index] == "\n"
            index += 1
            continue
        bracket = re.match(r"#?\[(=*)\[", text[index:])
        if bracket:
            end_token = "]" + bracket.group(1) + "]"
            end = text.find(end_token, index + bracket.end())
            if end < 0:
                raise EnumerationError(f"line {line}: unterminated bracket argument/comment")
            index = end + len(end_token)
            if text[start] != "#":
                result.append(Token(text[start + bracket.end():end], at_line, "bracket"))
            line += text[start:index].count("\n")
            continue
        if text[index] == "#":
            end = text.find("\n", index)
            index = len(text) if end < 0 else end
            continue
        if text[index] in "()":
            result.append(Token(text[index], line, "delimiter"))
            index += 1
            continue
        if text[index] == '"':
            index += 1
            value = ""
            while index < len(text) and text[index] != '"':
                if text[index] == "\\" and index + 1 < len(text):
                    # Keep escapes visible: source paths with escapes need an
                    # explicit supported form, rather than a guessed filename.
                    value += text[index:index + 2]
                    index += 2
                else:
                    value += text[index]
                    index += 1
            if index == len(text):
                raise EnumerationError(f"line {at_line}: unterminated quoted argument")
            index += 1
            result.append(Token(value, at_line))
            line += text[start:index].count("\n")
            continue
        while index < len(text) and not text[index].isspace() and text[index] not in '()"#':
            index += 1
        result.append(Token(text[start:index], at_line))
    return result


def commands(text: str):
    source = tokens(text)
    index = 0
    while index < len(source):
        name = source[index]
        if index + 1 >= len(source) or source[index + 1].kind != "delimiter" or source[index + 1].value != "(":
            raise EnumerationError(f"line {name.line}: expected a CMake command")
        index += 2
        depth, arguments = 1, []
        while index < len(source) and depth:
            token = source[index]
            if token.kind == "delimiter" and token.value == "(":
                depth += 1
            elif token.kind == "delimiter" and token.value == ")":
                depth -= 1
            if depth:
                arguments.append(token)
            index += 1
        if depth:
            raise EnumerationError(f"line {name.line}: unterminated {name.value} command")
        yield name.value.lower(), arguments, name.line


def declared_sources(root: Path, cmake: str) -> tuple[set[str], list[str]]:
    directory = root / Path(cmake).parent
    variables: dict[str, list[str]] = {}
    unsupported_variables: dict[str, str] = {}
    declared: set[str] = set()
    errors: list[str] = []
    function = ""
    function_kind = ""
    conditionals = []
    functions = []
    builtins = {name: directory.as_posix() for name in
                ("CMAKE_CURRENT_SOURCE_DIR", "CMAKE_CURRENT_LIST_DIR", "CMAKE_CURRENT_FUNCTION_LIST_DIR")}

    def snapshot():
        return ({name: list(values) for name, values in variables.items()}, dict(unsupported_variables))

    def expand(value: str, seen: tuple[str, ...] = ()) -> list[str]:
        if "\\" in value:
            raise EnumerationError(f"unsupported escaped source argument {value!r}; use forward-slash literal paths")
        configuration = CONFIGURATION.fullmatch(value)
        if configuration:
            return expand(configuration.group(1), seen)
        if re.fullmatch(r"\$<TARGET_OBJECTS:[A-Za-z_][A-Za-z0-9_]*>", value):
            return []
        if "$<" in value:
            raise EnumerationError(f"unsupported source generator expression {value!r}; enumerate its paths explicitly")
        match = VARIABLE.search(value)
        if match:
            name = match.group(1)
            if name in unsupported_variables:
                raise EnumerationError(f"source variable {name} uses {unsupported_variables[name]}; enumerate literal paths")
            if name in seen:
                raise EnumerationError(f"source variable cycle at {name}")
            if name in builtins:
                if name == "CMAKE_CURRENT_FUNCTION_LIST_DIR" and function_kind != "function":
                    raise EnumerationError("CMAKE_CURRENT_FUNCTION_LIST_DIR requires a function body")
                if function and name in {"CMAKE_CURRENT_SOURCE_DIR", "CMAKE_CURRENT_LIST_DIR"}:
                    raise EnumerationError(f"{name} in {function} depends on its caller; use CMAKE_CURRENT_FUNCTION_LIST_DIR")
                values = [builtins[name]]
            elif name in variables:
                values = [expanded for item in variables[name] for expanded in expand(item, (*seen, name))]
            elif function == "labrador_add_test_suite" and value == "${suite_SOURCES}":
                # Calls to this repository's wrapper are checked directly;
                # its forwarded parameter is not another local source list.
                return []
            else:
                raise EnumerationError(f"unresolved source variable {name}; use a local literal set/list or extend this checker")
            if match.group(0) != value and len(values) != 1:
                raise EnumerationError(f"source path {value!r} interpolates a list; enumerate complete paths")
            return [expanded for item in values for expanded in
                    expand(value[:match.start()] + item + value[match.end():], seen)]
        if "$" in value:
            raise EnumerationError(f"unsupported source expression {value!r}; enumerate literal paths")
        return [item for item in value.split(";") if item]

    def source_path(value: str) -> str:
        path = Path(value)
        if not path.is_absolute():
            if function:
                raise EnumerationError(f"relative source {value!r} in {function} depends on its caller; use CMAKE_CURRENT_FUNCTION_LIST_DIR")
            path = directory / path
        path = Path(os.path.normpath(path))
        try:
            return path.relative_to(root).as_posix()
        except ValueError as error:
            raise EnumerationError(f"source path leaves the checkout: {value!r}") from error

    def expand_arguments(arguments: list[Token]) -> list[str]:
        if any(token.kind == "bracket" for token in arguments):
            raise EnumerationError("bracket source arguments are unsupported; use quoted or unquoted literal paths")
        return [item for token in arguments for item in expand(token.value)]

    try:
        parsed = commands((root / cmake).read_text(encoding="utf-8-sig"))
        for name, arguments, line in parsed:
            values = [token.value for token in arguments]
            if name == "if":
                conditionals.append([snapshot(), [], False])
            elif name in {"elseif", "else"}:
                if not conditionals:
                    raise EnumerationError(f"line {line}: {name} without if")
                saved, branches, _ = conditionals[-1]
                branches.append(snapshot())
                variables, unsupported_variables = ({key: list(items) for key, items in saved[0].items()}, dict(saved[1]))
                conditionals[-1][2] = name == "else"
            elif name == "endif":
                if not conditionals:
                    raise EnumerationError(f"line {line}: endif without if")
                saved, branches, has_else = conditionals.pop()
                branches.append(snapshot())
                if not has_else:
                    branches.append(saved)
                variables, unsupported_variables = {}, {}
                for branch_variables, branch_unsupported in branches:
                    for variable, items in branch_variables.items():
                        combined = variables.setdefault(variable, [])
                        combined.extend(item for item in items if item not in combined)
                    unsupported_variables.update(branch_unsupported)
            elif name in {"function", "macro"}:
                functions.append((function, function_kind, snapshot()))
                function = values[0] if values else ""
                function_kind = name
            elif name in {"endfunction", "endmacro"}:
                if not functions:
                    raise EnumerationError(f"line {line}: {name} without function/macro")
                function, function_kind, (variables, unsupported_variables) = functions.pop()
            elif name == "set" and values:
                variable, assigned = values[0], values[1:]
                if "CACHE" in assigned or "PARENT_SCOPE" in assigned:
                    unsupported_variables[variable] = "a CACHE/PARENT_SCOPE assignment"
                else:
                    try:
                        # CMake expands values on assignment. Keeping the
                        # reference would make a later set change an earlier alias.
                        variables[variable] = expand_arguments(arguments[1:])
                        unsupported_variables.pop(variable, None)
                    except EnumerationError as error:
                        unsupported_variables[variable] = f"an unsupported set assignment ({error})"
            elif name == "unset" and values:
                variable = values[0]
                if len(values) != 1:
                    unsupported_variables[variable] = "a CACHE/PARENT_SCOPE unset"
                else:
                    variables[variable] = []
                    unsupported_variables.pop(variable, None)
            elif name == "list" and len(values) >= 2:
                operation, variable = values[:2]
                if operation in {"APPEND", "PREPEND"}:
                    try:
                        items = expand_arguments(arguments[2:])
                        existing = variables.get(variable, [])
                        variables[variable] = existing + items if operation == "APPEND" else items + existing
                    except EnumerationError as error:
                        unsupported_variables[variable] = f"an unsupported list assignment ({error})"
                elif operation == "REMOVE_ITEM":
                    try:
                        removed = expand_arguments(arguments[2:])
                        variables[variable] = [value for value in variables.get(variable, []) if value not in removed]
                    except EnumerationError as error:
                        unsupported_variables[variable] = f"an unsupported list removal ({error})"
                else:
                    unsupported_variables[variable] = f"list({operation})"
            elif name in {"file", "string", "math", "cmake_path", "get_filename_component", "get_property", "get_target_property"}:
                # These commands can replace an earlier local value. A stale
                # list must not survive an operation this model does not execute.
                operands = values[1:] if name in {"file", "string", "math", "cmake_path"} else values
                for value in operands:
                    if value in variables:
                        unsupported_variables[value] = f"an unsupported {name} operation"
                if name == "file" and len(values) >= 2 and values[0] in {"GLOB", "GLOB_RECURSE"}:
                    unsupported_variables[values[1]] = f"file({values[0]})"
            if name in {"add_library", "add_executable", "target_sources"}:
                sources = arguments[1:]
                if "FILE_SET" in values:
                    errors.append(f"{cmake}:{line}: source FILE_SET declarations are unsupported; extend the checker explicitly")
                    continue
                if any(token.value in {"ALIAS", "IMPORTED"} for token in sources):
                    continue
            elif name == "labrador_add_test_suite":
                if "SOURCES" not in values:
                    errors.append(f"{cmake}:{line}: labrador_add_test_suite needs an explicit SOURCES list")
                    continue
                start = values.index("SOURCES") + 1
                end = next((index for index in range(start, len(values)) if values[index] in {"LINKS", "TIMEOUT"}), len(values))
                sources = arguments[start:end]
            else:
                continue
            for token in sources:
                try:
                    for value in expand_arguments([token]):
                        if value.endswith(".cpp"):
                            declared.add(source_path(value))
                except EnumerationError as error:
                    errors.append(f"{cmake}:{token.line}: {error}")
    except (EnumerationError, OSError, UnicodeError) as error:
        errors.append(f"{cmake}: {error}")
    return declared, errors


def check(root: Path, tracked: list[str]) -> tuple[int, list[str]]:
    root = Path(os.path.abspath(root))
    required = {path for path in tracked if path.endswith(".cpp") and path.startswith(ROOTS)}
    cmake_files = sorted(path for path in tracked if Path(path).name == "CMakeLists.txt")
    declared: set[str] = set()
    errors: list[str] = []
    for cmake in cmake_files:
        sources, failures = declared_sources(root, cmake)
        declared.update(sources)
        errors.extend(failures)
    for path in sorted(required - declared):
        parent = Path(path).parent
        nearest = next((str(directory / "CMakeLists.txt").replace("\\", "/") for directory in
                        (parent, *parent.parents) if str(directory / "CMakeLists.txt").replace("\\", "/") in cmake_files), "CMakeLists.txt")
        errors.append(f"{path}: no explicit source declaration; add the correct relative path to {nearest} or another target's source list")
    return len(required), errors


def main(arguments: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=REPOSITORY, help="checkout to inspect (default: this tool's checkout)")
    options = parser.parse_args(arguments)
    try:
        located = subprocess.run(["git", "-C", str(options.repo), "rev-parse", "--show-toplevel"],
                                 capture_output=True, timeout=30, check=True)
        repository = Path(located.stdout.decode("utf-8", errors="surrogateescape").rstrip("\r\n"))
        listed = subprocess.run(["git", "-C", str(repository), "ls-files", "-z"],
                                capture_output=True, timeout=30, check=True)
        tracked = listed.stdout.decode("utf-8", errors="surrogateescape").split("\0")
        count, errors = check(repository, [path for path in tracked if path])
    except (OSError, subprocess.SubprocessError) as error:
        print(f"Source enumeration could not complete: {error}", file=sys.stderr)
        return 2
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print(f"All {count} tracked C++ sources under engine/tests/samples/bench have explicit CMake declarations.")
    print("All conditional branches count; this checks enumeration, not configured compilation or test execution.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
