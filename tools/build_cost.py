"""Record one explicit Ninja build, or report/compare its retained costs offline."""
from __future__ import annotations

import argparse
from collections import defaultdict
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import posixpath
import re
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parent))
import git_local  # noqa: E402

SCHEMA = 1
INPUTS = ("ninja.before.log", "ninja.after.log", "compile_commands.before.json",
          "compile_commands.after.json", "dependencies.txt", "cache.before.txt", "cache.after.txt",
          "build.before.ninja", "build.after.ninja", "rules.before.ninja", "rules.after.ninja")
BUILD_ENVIRONMENT = ("PATH", "CL", "_CL_", "LINK", "_LINK_", "INCLUDE", "LIB", "LIBPATH", "CFLAGS", "CXXFLAGS", "CPPFLAGS",
                     "LDFLAGS", "CPATH", "C_INCLUDE_PATH", "CPLUS_INCLUDE_PATH", "LIBRARY_PATH", "COMPILER_PATH",
                     "GCC_EXEC_PREFIX", "EMCC_CFLAGS", "EMCC_FORCE_STDLIBS", "EMMAKEN_CFLAGS", "EM_CACHE",
                     "CCACHE_DIR", "CCACHE_DISABLE", "CCACHE_BASEDIR", "CCACHE_COMPILERCHECK", "CCACHE_PREFIX",
                     "CCACHE_CPP2", "CCACHE_SLOPPINESS", "SCCACHE_DIR", "SCCACHE_DISABLE")
TOOL_CACHE_KEYS = {"compiler": "CMAKE_CXX_COMPILER", "linker": "CMAKE_LINKER", "archiver": "CMAKE_AR",
                   "ranlib": "CMAKE_RANLIB", "manifest_tool": "CMAKE_MT", "resource_compiler": "CMAKE_RC_COMPILER"}


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def key(path: str, directory: str = "") -> str:
    path, directory = path.replace("\\", "/"), directory.replace("\\", "/")
    absolute = path.startswith("/") or re.match(r"^[A-Za-z]:/", path)
    value = posixpath.normpath(path if absolute else posixpath.join(directory, path))
    return value.casefold() if re.match(r"^[A-Za-z]:/", value) else value


def display(path: str, root: str) -> str:
    root = key(root).rstrip("/") + "/"
    return path[len(root):] if path.startswith(root) else path


def parse_log(text: str) -> list[dict]:
    lines = text.splitlines()
    if not lines or not text.endswith("\n") or not re.fullmatch(r"# ninja log v[567]", lines[0]):
        raise ValueError("Expected a Ninja v5, v6 or v7 log header")
    rows = []
    for number, line in enumerate(lines[1:], 2):
        fields = line.split("\t")
        if len(fields) != 5:
            raise ValueError(f"Ninja log line {number}: expected five TAB-separated fields")
        start, end, mtime, output, command_hash = fields
        if (not start.isdecimal() or not end.isdecimal() or int(end) < int(start)
                or not re.fullmatch(r"-?\d+", mtime) or not output
                or not re.fullmatch(r"[0-9a-fA-F]{1,16}", command_hash)):
            raise ValueError(f"Ninja log line {number}: invalid interval, output or opaque command hash")
        rows.append({"start_ms": int(start), "end_ms": int(end), "output": output,
                     "command_hash": command_hash.lower()})
    return rows


def appended_log(before: bytes, after: bytes) -> str:
    parse_log(after.decode("utf-8"))
    if not before:
        return after.decode("utf-8")
    parse_log(before.decode("utf-8"))
    if not before.endswith(b"\n") or not after.startswith(before):
        raise ValueError("Ninja log prefix was lost: truncation/recompaction or a concurrent build prevents selecting this run")
    header = after.split(b"\n", 1)[0] + b"\n"
    return (header + after[len(before):]).decode("utf-8")


def parse_compilation_database(text: str) -> dict[str, dict]:
    entries = json.loads(text)
    if not isinstance(entries, list):
        raise ValueError("Compilation database must be a JSON array")
    result = {}
    for entry in entries:
        if not isinstance(entry, dict) or any(not isinstance(entry.get(name), str) or not entry[name]
                                              for name in ("directory", "file", "output")):
            raise ValueError("Each compilation needs explicit directory, file and output strings; output flags are not guessed")
        command = entry.get("arguments", entry.get("command"))
        if not (isinstance(command, str) and command.strip()) and not (
                isinstance(command, list) and command and all(isinstance(arg, str) for arg in command)
                and command[0].strip()):
            raise ValueError("Each compilation needs command text or an arguments array")
        output = key(entry["output"], entry["directory"])
        value = {"source": key(entry["file"], entry["directory"]), "command": command,
                 "directory": entry["directory"]}
        if output in result and result[output] != value:
            raise ValueError(f"Conflicting compilation database entries for {output}")
        result[output] = value
    return result


def parse_dependencies(text: str, directory: str) -> dict[str, dict]:
    result = {}
    current = None
    for line in text.splitlines() + [""]:
        if not line:
            if current is not None:
                if len(current["paths"]) != current["count"]:
                    raise ValueError("Ninja dependency block count differs from its listed paths")
                if current["output"] in result:
                    raise ValueError("Duplicate Ninja dependency output block")
                result[current["output"]] = current
                current = None
            continue
        if line.startswith("    ") and current is not None:
            if not line[4:].strip():
                raise ValueError("Empty Ninja dependency path")
            current["paths"].append(key(line[4:], directory))
            continue
        match = re.fullmatch(r"(.+): #deps (\d+), deps mtime -?\d+ \((VALID|STALE)\)", line)
        if match is None or current is not None:
            raise ValueError(f"Unrecognised Ninja dependency line: {line}")
        current = {"output": key(match[1], directory), "count": int(match[2]),
                   "state": match[3], "paths": []}
    return result


def analyze(log: str, compilation_database: str, dependencies: str, context: dict) -> dict:
    commands = parse_compilation_database(compilation_database)
    deps = parse_dependencies(dependencies, context["build_directory"]) if dependencies else {}
    log_rows = parse_log(log)
    groups = defaultdict(list)
    for row in log_rows:
        groups[(row["start_ms"], row["end_ms"], row["command_hash"])].append(row)
    compiles, other, folders, fanout, missing = [], [], defaultdict(int), defaultdict(set), []
    seen_outputs = set()
    for (start, end, command_hash), rows in groups.items():
        outputs = sorted({key(row["output"], context["build_directory"]) for row in rows})
        mapped = [output for output in outputs if output in commands]
        if len(mapped) > 1:
            raise ValueError("An identical interval/hash maps to multiple compilation outputs; edge identity is ambiguous")
        if not mapped:
            kind = "link/post-link" if any(Path(output).suffix.lower() in
                   {".exe", ".dll", ".so", ".dylib", ".wasm", ".js", ".html"} for output in outputs) else (
                   "archive" if any(Path(output).suffix.lower() in {".lib", ".a"} for output in outputs) else (
                   "unmapped-object" if any(Path(output).suffix.lower() in {".obj", ".o"} for output in outputs) else "other"))
            other.append({"kind": kind, "outputs": outputs, "elapsed_ms": end - start,
                          "start_ms": start, "end_ms": end, "log_command_hash": command_hash})
            continue
        output = mapped[0]
        if output in seen_outputs:
            raise ValueError("A compilation output has multiple intervals; this is not an isolated single-build window")
        seen_outputs.add(output)
        command = commands[output]
        source = display(command["source"], context["source_directory"])
        compiles.append({"source": source, "output": output, "outputs": outputs,
                         "elapsed_ms": end - start, "start_ms": start, "end_ms": end,
                         "command": command["command"], "directory": command["directory"],
                         "log_command_hash": command_hash})
        folders[posixpath.dirname(source) or "."] += end - start
        dependency = deps.get(output)
        if dependency is None or dependency["state"] != "VALID":
            missing.append({"output": output, "state": "missing" if dependency is None else "stale"})
        else:
            for header in set(dependency["paths"]) - {command["source"]}:
                fanout[display(header, context["source_directory"])].add(output)
    wall = context.get("build_wall_seconds")
    if wall is not None and (isinstance(wall, bool) or not isinstance(wall, (int, float))
                             or not math.isfinite(wall) or wall < 0):
        raise ValueError("Whole-build wall seconds must be finite and nonnegative, or absent")
    return {"schema_version": SCHEMA, "context": context,
            "compiler_process_cpu_seconds": None,
            "compiler_invocation_edge_elapsed_ms_sum": sum(row["elapsed_ms"] for row in compiles),
            "recorded_edge_span_ms": max((row["end_ms"] for row in log_rows), default=0)
                                     - min((row["start_ms"] for row in log_rows), default=0),
            "compilations": sorted(compiles, key=lambda row: (-row["elapsed_ms"], row["source"], row["output"])),
            "folders": sorted(({"folder": folder, "elapsed_ms_sum": cost} for folder, cost in folders.items()),
                              key=lambda row: (-row["elapsed_ms_sum"], row["folder"])),
            "other_interval_groups": other,
            "header_fanout": sorted(({"header": header, "object_count": len(outputs)} for header, outputs in fanout.items()),
                                    key=lambda row: (-row["object_count"], row["header"])),
            "dependency_coverage": {"timed_objects": len({row["output"] for row in compiles}),
                                    "missing_or_stale": missing},
            "limits": ["Elapsed compiler edges overlap and include launch/dependency-extraction overhead; CPU time is unmeasured.",
                       "Unmapped intervals are grouped by time and opaque hash, not proven distinct commands.",
                       "Link/post-link edges may include wrappers and copy commands; failed commands are absent from Ninja logs.",
                       "Header fan-out counts measured objects in the valid dependency snapshot, not header parsing time."]}


def compare(first: dict, second: dict) -> dict:
    fields = ("source", "machine", "configuration", "tools", "build_environment", "targets", "parallelism", "cache_context", "clean_first")
    def known(value, name):
        if name == "clean_first":
            return isinstance(value, bool)
        if name == "parallelism":
            return type(value) is int and value > 0
        if name == "targets":
            return isinstance(value, list) and bool(value) and all(isinstance(item, str) and item for item in value)
        if name == "cache_context":
            return isinstance(value, str) and bool(value.strip())
        return isinstance(value, dict) and bool(value)
    different = [name for name in fields if not known(first["context"].get(name), name)
                 or not known(second["context"].get(name), name)
                 or first["context"].get(name) != second["context"].get(name)]
    if any(not isinstance(item["context"].get("source"), dict) or not item["context"]["source"].get("head")
           for item in (first, second)):
        different.append("known source")
    workload = lambda report: sorted([row["outputs"] for row in report["compilations"]]
                                    + [row["outputs"] for row in report["other_interval_groups"]])
    if workload(first) != workload(second):
        different.append("recorded output workload")
    if any(type(item["context"].get("exit_code")) is not int or item["context"]["exit_code"] != 0
           for item in (first, second)):
        different.append("successful build")
    if first["context"].get("source_after") != first["context"].get("source") or second["context"].get("source_after") != second["context"].get("source"):
        different.append("unchanged source during build")
    result = {"comparable": not different, "context_differences": different, "deltas": {}}
    if not different:
        for name, before, after in (
                ("build_wall_seconds", first["context"].get("build_wall_seconds"), second["context"].get("build_wall_seconds")),
                ("compiler_invocation_edge_elapsed_ms_sum", first["compiler_invocation_edge_elapsed_ms_sum"],
                 second["compiler_invocation_edge_elapsed_ms_sum"])):
            if before is not None and after is not None:
                result["deltas"][name] = {"before": before, "after": after, "difference": after - before,
                                           "ratio": after / before if before else None}
    return result


def cache_values(text: str) -> dict[str, str]:
    result = {}
    for line in text.splitlines():
        match = re.fullmatch(r"([^/#][^:]*):[^=]*=(.*)", line)
        if match:
            result[match[1]] = match[2]
    return result


def source_identity(root: Path) -> dict:
    try:
        head = git_local.git(root, "rev-parse", "HEAD", text=True).stdout.strip()
        patch = git_local.git(root, "diff", "--binary", "HEAD").stdout
        untracked = git_local.git(root, "ls-files", "--others", "--exclude-standard", "-z").stdout
        additions = {name.decode("utf-8"): digest((root / name.decode("utf-8")).read_bytes())
                     for name in untracked.split(b"\0") if name and (root / name.decode("utf-8")).is_file()}
        return {"head": head, "dirty": bool(patch or additions), "patch_sha256": digest(patch), "untracked": additions}
    except git_local.GitError as error:
        return {"unknown": str(error)}


def record(arguments) -> int:
    if not arguments.execute:
        raise ValueError("Recording runs one cmake --build; pass --execute explicitly")
    build = arguments.build.resolve()
    output = arguments.output.resolve()
    if output.exists():
        raise ValueError(f"Refusing to overwrite capture directory {output}")
    cache = (build / "CMakeCache.txt").read_bytes()
    values = cache_values(cache.decode("utf-8"))
    if values.get("CMAKE_GENERATOR") != "Ninja":
        raise ValueError("Recording requires the single-configuration Ninja generator")
    compdb = (build / "compile_commands.json").read_bytes()
    parse_compilation_database(compdb.decode("utf-8"))
    source = Path(values["CMAKE_HOME_DIRECTORY"]).resolve()
    cmake, ninja = values["CMAKE_COMMAND"], values["CMAKE_MAKE_PROGRAM"]
    tools = {}
    for name, executable in (("cmake", cmake), ("ninja", ninja)):
        probe = subprocess.run([executable, "--version"], capture_output=True, check=True, text=True)
        tools[name] = {"path": executable, "version": probe.stdout.strip(), "sha256": digest(Path(executable).read_bytes())}
    for name, variable in TOOL_CACHE_KEYS.items():
        if values.get(variable):
            executable = Path(values[variable])
            tools[name] = {"path": str(executable), "sha256": digest(executable.read_bytes()) if executable.is_file() else None}
    infos = list((build / "CMakeFiles").glob("*/CMakeCXXCompiler.cmake"))
    tools["configured_compiler_information"] = {path.parent.name: path.read_text(encoding="utf-8") for path in infos}
    graph = {"build": (build / "build.ninja").read_bytes(),
             "rules": (build / "CMakeFiles/rules.ninja").read_bytes()}
    command = [cmake, "--build", str(build), "--parallel", str(arguments.parallel), "--target", *arguments.target]
    if arguments.clean_first:
        command.append("--clean-first")
    context = {"source_directory": str(source), "build_directory": str(build), "source": source_identity(source),
               "tools": tools, "configuration": {name: value for name, value in values.items()
                   if name.startswith(("CMAKE_CXX_FLAGS", "CMAKE_EXE_LINKER_FLAGS", "LABRADOR_", "VCPKG_"))
                   or name in {"CMAKE_BUILD_TYPE", "CMAKE_CXX_COMPILER_LAUNCHER", "CMAKE_TOOLCHAIN_FILE"}},
               "machine": {"platform": platform.platform(), "processor": platform.processor(), "logical_cpus": os.cpu_count()},
               "build_environment": {name: os.environ.get(name) for name in BUILD_ENVIRONMENT},
               "targets": arguments.target, "parallelism": arguments.parallel, "cache_context": arguments.cache_context,
               "clean_first": arguments.clean_first, "command": command,
               "started_utc": datetime.now(timezone.utc).isoformat()}
    context["configuration"]["build_graph_sha256"] = {name: digest(data) for name, data in graph.items()}
    output.mkdir(parents=True)
    (output / "cache.before.txt").write_bytes(cache)
    (output / "compile_commands.before.json").write_bytes(compdb)
    before = (build / ".ninja_log").read_bytes() if (build / ".ninja_log").exists() else b""
    (output / "ninja.before.log").write_bytes(before)
    for name, data in graph.items():
        (output / f"{name}.before.ninja").write_bytes(data)
    started = time.perf_counter()
    with (output / "stdout.log").open("wb") as stdout, (output / "stderr.log").open("wb") as stderr:
        completed = subprocess.run(command, cwd=source, stdout=stdout, stderr=stderr, check=False)
    context.update(build_wall_seconds=time.perf_counter() - started, exit_code=completed.returncode,
                   source_after=source_identity(source))
    for original, saved in ((".ninja_log", "ninja.after.log"), ("compile_commands.json", "compile_commands.after.json"),
                            ("CMakeCache.txt", "cache.after.txt"), ("build.ninja", "build.after.ninja"),
                            ("CMakeFiles/rules.ninja", "rules.after.ninja")):
        path = build / original
        (output / saved).write_bytes(path.read_bytes() if path.exists() else b"")
    dependencies = subprocess.run([ninja, "-t", "deps"], cwd=build, capture_output=True, check=False)
    (output / "dependencies.txt").write_bytes(dependencies.stdout if dependencies.returncode == 0 else b"")
    context["dependency_export_error"] = dependencies.stderr.decode("utf-8", errors="replace") if dependencies.returncode else None
    receipt = {"schema_version": SCHEMA, "context": context,
               "sha256": {name: digest((output / name).read_bytes()) for name in INPUTS}}
    (output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    report = load_report(output)
    print_summary(report)
    print(f"Capture: {output}")
    return completed.returncode


def load_report(folder: Path) -> dict:
    receipt = json.loads((folder / "receipt.json").read_text(encoding="utf-8"))
    if receipt.get("schema_version") != SCHEMA:
        raise ValueError("Unsupported build-cost receipt schema")
    context = receipt.get("context")
    if not isinstance(context, dict) or any(not isinstance(context.get(name), str) or not context[name]
                                           for name in ("build_directory", "source_directory")):
        raise ValueError("Capture needs source/build directory strings in its context")
    for name in ("source", "machine", "tools", "configuration"):
        if name in context and not isinstance(context[name], dict):
            raise ValueError(f"Capture context {name} must be an object")
    data = {name: (folder / name).read_bytes() for name in INPUTS}
    if any(receipt.get("sha256", {}).get(name) != digest(value) for name, value in data.items()):
        raise ValueError("Capture input hash mismatch")
    if any(data[f"{name}.before{ending}"] != data[f"{name}.after{ending}"] for name, ending in
           (("compile_commands", ".json"), ("cache", ".txt"), ("build", ".ninja"), ("rules", ".ninja"))):
        raise ValueError("Configuration/build graph changed during the build; configure first and capture again")
    log = appended_log(data["ninja.before.log"], data["ninja.after.log"])
    return analyze(log, data["compile_commands.after.json"].decode("utf-8"),
                   data["dependencies.txt"].decode("utf-8"), receipt["context"])


def print_summary(report: dict, top: int = 10) -> None:
    context = report["context"]
    source = context.get("source", {})
    print(f"Source: {source.get('head', 'unknown')}; dirty: {source.get('dirty', 'unknown')}; patch: {source.get('patch_sha256', 'unrecorded')}")
    print(f"Targets: {context.get('targets', 'unrecorded')}; parallelism: {context.get('parallelism', 'unrecorded')}; clean-first: {context.get('clean_first', 'unrecorded')}")
    print(f"Declared cache/load context: {context.get('cache_context', 'unrecorded')}")
    print(f"Machine: {json.dumps(context.get('machine', {}), sort_keys=True)}")
    configuration = context.get("configuration", {})
    print(f"Configuration: {configuration.get('CMAKE_BUILD_TYPE', 'unrecorded')}; renderer: {configuration.get('LABRADOR_RENDER_BACKEND', 'unrecorded')}; audio: {configuration.get('LABRADOR_AUDIO_BACKEND', 'unrecorded')}")
    tools = context.get("tools", {})
    print(f"Compiler identity: {json.dumps(tools.get('compiler', {}), sort_keys=True)}")
    print(f"Ninja: {tools.get('ninja', {}).get('version', 'unrecorded')}; CMake: {tools.get('cmake', {}).get('version', 'unrecorded').splitlines()[0]}")
    print(f"Build argv: {json.dumps(context.get('command'))}")
    print(f"Whole cmake build wall: {context.get('build_wall_seconds', 'unrecorded')} s; exit: {context.get('exit_code', 'unrecorded')}")
    print(f"Compiler invocation edges: {len(report['compilations'])}; summed elapsed: {report['compiler_invocation_edge_elapsed_ms_sum']} ms; CPU: unmeasured")
    print(f"Recorded edge span: {report['recorded_edge_span_ms']} ms (not whole-build wall)")
    print("Slow translation units (compiler-edge elapsed ms):")
    for row in report["compilations"][:top]:
        print(f"  {row['elapsed_ms']:8}  {row['source']}")
    print("Folder sums (overlapping compiler-edge elapsed ms):")
    for row in report["folders"][:top]:
        print(f"  {row['elapsed_ms_sum']:8}  {row['folder']}")
    for kind in ("link/post-link", "archive", "unmapped-object", "other"):
        rows = [row for row in report["other_interval_groups"] if row["kind"] == kind]
        print(f"{kind} interval groups: {len(rows)}; grouped elapsed: {sum(row['elapsed_ms'] for row in rows)} ms")
    coverage = report["dependency_coverage"]
    print(f"Dependency coverage: {coverage['timed_objects'] - len(coverage['missing_or_stale'])}/{coverage['timed_objects']} valid timed objects")
    print("Project header fan-out (object count, not parsing time):")
    project_headers = [row for row in report["header_fanout"]
                       if not row["header"].startswith("/") and not re.match(r"^[a-zA-Z]:/", row["header"])]
    for row in project_headers[:top]:
        print(f"  {row['object_count']:8}  {row['header']}")


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="action", required=True)
    capture = commands.add_parser("record", help="opt into one explicit local build, retaining its inputs/output")
    capture.add_argument("--build", required=True, type=Path)
    capture.add_argument("--output", required=True, type=Path)
    capture.add_argument("--target", action="append", required=True)
    capture.add_argument("--parallel", type=int, default=2)
    capture.add_argument("--cache-context", required=True, help="declared output/dependency/compiler/OS cache state and other load")
    capture.add_argument("--clean-first", action="store_true")
    capture.add_argument("--execute", action="store_true")
    for name in ("report", "compare"):
        command = commands.add_parser(name)
        command.add_argument("capture", type=Path)
        if name == "compare":
            command.add_argument("baseline", type=Path)
        command.add_argument("--json", action="store_true")
    arguments = parser.parse_args(argv)
    try:
        if arguments.action == "record":
            if arguments.parallel < 1 or not arguments.cache_context.strip():
                raise ValueError("Parallelism must be positive and cache context must be stated")
            return record(arguments)
        report = load_report(arguments.capture)
        if arguments.action == "compare":
            result = compare(load_report(arguments.baseline), report)
            print(json.dumps(result, indent=2))
        elif arguments.json:
            print(json.dumps(report, indent=2))
        else:
            print_summary(report)
        return 0
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
        print(f"build_cost: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
