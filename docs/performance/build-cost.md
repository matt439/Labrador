# Reading compilation and build costs

`tools/build_cost.py` reports named local captures offline. It records the
slow translation units, folder sums and header fan-out from explicit
compilation outputs, an isolated Ninja log window and a dependency export.
It adds no compiler profiling flags, CI warning, timing threshold or schedule.

## Capture one build

Use a configured **single-configuration Ninja** build, in its existing
developer environment. Enable CMake's compilation database explicitly:

```text
cmake --preset x64-debug-null -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
python tools/build_cost.py record --execute --build out/build/x64-debug-null --target LabradorMathTests --parallel 2 --cache-context "incremental outputs; installed dependencies warm; no compiler cache; OS cache and other load unmeasured" --output out/build-cost/math-incremental
python tools/build_cost.py report out/build-cost/math-incremental
python tools/build_cost.py report out/build-cost/math-incremental --json
```

`record` requires explicit `--execute`, runs exactly one `cmake --build`, and
refuses an existing output directory. It does not configure, install
dependencies, touch sources, retry failed builds or change compiler options.
Only one build may use the selected build directory during the capture,
and its sources must stay unchanged.
The command's stdout/stderr and exit status remain in the capture directory.

`--clean-first` requests CMake's ordinary clean before that build; it removes
build outputs in the chosen build directory. Use a separate build folder
when retaining existing binaries matters. It does not empty vcpkg, compiler
or operating-system caches. State those separately in `--cache-context`,
including competing work or unknown cache state. This description is the
maintainer's declared context, not a measured cache-hit or CPU-load counter.

The receipt retains source HEAD, dirty patch hash and untracked file hashes;
configured flags and dependency selections; compiler/linker/archiver/tool paths
and executable hashes;
CMake's compiler-identification record; host/processor facts; target,
parallelism, command and cache context. A narrow environment whitelist also
retains option/search-path/cache overrides such as MSVC's `CL`, `_CL_`,
`INCLUDE` and `LIB`, and corresponding common clang/Emscripten/cache variables.
MSVC [takes additional options from its environment](https://learn.microsoft.com/en-us/cpp/build/reference/cl-environment-variables?view=msvc-170),
so command text alone is insufficient provenance. This is an explicit whitelist,
not the whole environment. Paths and full compiler commands
are local evidence, so review their contents when choosing what to publish.

Before/after compilation databases, CMake cache, Ninja build/rule files and
log snapshots are retained with SHA-256 hashes. The reader verifies hashes
and unchanged configuration. A lost log prefix after recompaction or
truncation is refused; start a new capture after that housekeeping. Timing
resets in a cumulative `.ninja_log` do not reliably delimit its builds.

## Read the quantities

- **Whole build wall seconds** measure the explicit CMake build subprocess.
  They exclude this tool's provenance probes and dependency export, and the
  developer environment setup already performed by the caller. A clean
  request includes CMake's clean step. Configuration, package installation
  and tests are outside this interval.
- **Compiler invocation edge elapsed** comes from Ninja start/end records
  for outputs explicitly listed by `compile_commands.json`. It includes
  invocation/dependency-extraction overhead and any configured compiler
  launcher/cache. A cache hit can avoid running the compiler itself; this
  tool records neither cache-hit counters nor time inside compiler phases.
  Its sum overlaps when compiles
  run in parallel and is neither build wall time nor measured process CPU.
  CPU work is explicitly **unmeasured**.
- **Recorded edge span** runs from the first retained edge's start to the
  last end. It omits work outside those records and is not the whole build.
- **Link/post-link** and archive interval groups are separate. A CMake link
  edge can include wrappers and post-link copies. Equal time/hash records
  are grouped for multi-output edges; unclassified groups are not proven
  distinct commands. Common output suffixes identify these artifact groups;
  the retained Ninja graph gives their exact commands. Ambiguous
  compiler-output groups are refused.
- **Header fan-out** counts distinct timed object outputs whose valid
  recorded dependencies contain the header. This is an include relationship,
  not measured header parsing cost. Missing/stale dependency blocks and
  unmapped object outputs are named. No-op captures have no timed compiler
  objects, so their dependency coverage is empty. Paths are normalised
  lexically; filesystem aliases such as short Windows paths are not resolved.

The log parser accepts Ninja v5/v6/v7's five fields. Command hashes remain
opaque; compilation-database commands are not assumed hash-equivalent to
Ninja's expanded commands. Failed compiler commands do not enter that log;
a failed build's receipt and stdout/stderr remain necessary evidence.
[Ninja's log implementation](https://github.com/ninja-build/ninja/blob/v1.13.2/src/build_log.cc)
and [builder](https://github.com/ninja-build/ninja/blob/v1.13.2/src/build.cc)
define these boundaries.

## Compare named captures

```text
python tools/build_cost.py compare out/build-cost/math-second out/build-cost/math-first
```

This compares the first positional capture with the named baseline in the
second position. It reports deltas/ratios only when source, machine,
configuration/tools/environment, targets, parallelism, declared cache context, clean
selection and actual recorded output workload match, both builds succeeded,
and source snapshots match before/after each build. It lists differing
context otherwise. A zero baseline gives no ratio. This is a bounded
comparison aid, without a pass/fail performance threshold. Matching recorded
machine facts do not measure RAM/clock state, OS cache state or competing
load. Executable hashes do not inventory every toolchain DLL or SDK header.

The dated [IPO CPU comparison](2026-11-13-ipo-update-phase.md) and native
Release qualification already record whole-build/relink observations.
The latter's retained clean-build wall times include developer-environment
setup, unlike this tool's interval. Its current Ninja logs also contain later
incremental builds, so those logs cannot reconstruct clean-run per-source
costs. Keep the original wall/link observations in their own scope instead
of interpreting cumulative log rows as a clean-build baseline.

## Local qualification

The [dated local capture record](2026-11-17-build-cost.md) accompanies the implementation. It uses a
bounded headless math-test target to qualify parsing, dependency coverage,
comparison and no-op handling; it establishes no whole-engine clean-build
budget or performance promise for another machine.
