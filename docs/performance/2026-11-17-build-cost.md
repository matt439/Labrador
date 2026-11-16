# Build-cost reporting qualification, 2026-11-17

[SM-85](../product/BACKLOG.md#sm-85) adds the offline
[build-cost report](build-cost.md), with no timing threshold or recurring job.
This qualifies its retained-log interpretation on a bounded native target;
it is not a whole-engine clean-build budget.

## Inputs and commands

- Compiled source: `5cff44b9c6da777479229832abb40bfc0fc6e65c`. The working
  tree adds the reporting tool, its fixtures and procedure as untracked
  files; their hashes are in each receipt. No timed C++ source or build
  policy differs from that revision. Both source snapshots match within
  each capture.
- Windows 11 `10.0.26300`, AMD64 Family 26 Model 68 Stepping 0, 32 logical
  processors. Other agents build/test concurrently. OS file-cache state,
  clocks, RAM state and actual process CPU work are unmeasured.
- MSVC `19.51.36256.0`, tools folder `14.51.36231`; CMake
  `4.3.1-msvc1`; Ninja `1.13.2`. Receipts retain executable hashes for the
  compiler, linker, archiver, manifest/resource tools and CMake/Ninja, plus
  CMake's compiler-identification file and the relevant build environment.
  `CL`, `_CL_`, `LINK` and `_LINK_` are unset in these runs.
- `x64-debug-null`, `CMAKE_EXPORT_COMPILE_COMMANDS=ON`, installed vcpkg
  packages reused read-only from the main checkout with
  `-DVCPKG_MANIFEST_INSTALL=OFF`. No compiler launcher/cache is configured.
  Debug's `/Od`, `/Zi`, `/MDd`, `/RTC1` and `/fp:precise` remain unchanged.

From the VS x64 developer environment in the measurement worktree:

```text
cmake --preset x64-debug-null -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DVCPKG_MANIFEST_INSTALL=OFF -DVCPKG_INSTALLED_DIR=C:/Projects/Labrador/out/build/x64-debug/vcpkg_installed
python tools/build_cost.py record --execute --build out/build/x64-debug-null --target LabradorMathTests --parallel 2 --clean-first --cache-context "clean outputs; installed vcpkg warm; compiler launcher none; OS file cache unmeasured; other agents active" --output out/build-cost/math-env-clean-1
```

The second invocation uses the same arguments and context, with
`math-env-clean-2` as the output. A third uses `math-env-noop`, omits
`--clean-first`, and declares up-to-date outputs. Configuration and developer
environment setup are outside the measured CMake subprocess interval.

## Recorded observations

| Named capture | Build wall, s | Compiler-edge elapsed sum, ms | Recorded edge span, ms | Archive group, ms | Executable link/post-link group, ms |
|---|---:|---:|---:|---:|---:|
| `math-env-clean-1` | 6.033 | 7,112 | 5,855 | 47 | 423 |
| `math-env-clean-2` | 6.095 | 7,253 | 5,944 | 45 | 428 |
| `math-env-noop` | 1.488 | 0 | 1,425 | 0 | 0 |

Both clean-output builds record 29 compilation objects and valid dependencies
for all 29. The compiler-edge sums exceed build wall time because work
overlaps. Neither number is process CPU time. The no-op still runs normal
check targets, without compiling or linking anything.

The slowest unit in each clean capture is `tests/test_main.cpp`, 1,001 and
1,020 ms. Folder sums are respectively 3,075/3,125 ms for `tests/math`,
3,036/3,108 ms for `engine/math`, and 1,001/1,020 ms for `tests`. These sums
overlap in the build. Both dependency snapshots name `scalar.h` and
`vector2f.h` in 28 timed objects each; an independent count of the retained
`scalar.h` dependency lines is also 28. This is fan-out, not the time spent
parsing either header.

The comparison accepts the two clean-output captures' matching recorded
context and workload; its wall ratio is 1.0104 and compiler-edge-sum ratio
1.0198. One pair on this loaded desktop establishes no regression boundary
or stable speed difference. Clean versus no-op comparison is unqualified
because its declared cache context, clean selection and workload differ.
`LabradorMathTests` then passes 1/1.

Raw snapshots, dependency exports, compiler commands, contexts and logs stay
under `C:/Projects/Labrador-agent-build-cost/out/build-cost/`. They are local
measurement artifacts, not release assets. The source snapshot and input
hashes in each `receipt.json` identify its data independently of later edits.

## Existing IPO evidence retained in its scope

The [native Release IPO qualification](2026-11-16-native-release-ipo.md)
already records source `52a84842`, compiler/configuration, commands and cache
context. Its retained `qualification-verified.json` gives whole-build wall
observations of 123.557 s OFF and 142.730 s ON, including developer-environment
setup. `incremental.json` records the separate no-op/rebuild intervals.
`build-audit.json` retains the clean frame-benchmark link/post-link edges,
0.442 s OFF and 1.532 s ON. These records were read and agree with that dated
qualification; no new full-engine IPO run was made here.

Those build directories' current `.ninja_log` files contain subsequent
builds. A timing reset cannot recover a lost clean-build window after
recompaction. They are therefore not imported as per-source clean-build
baselines, and the 29-object Debug captures above are not compared with
their full Release workload.

## Offline and source checks

Thirteen compiler-free fixtures cover parallel sums versus span/wall/CPU,
multi-output records, append-window selection and recompaction loss,
malformed/partial rows, explicit compilation outputs, repeated sources,
missing/stale/duplicate dependencies, Windows paths and spaces, no-op and
zero-denominator comparisons, missing/changed provenance, ambient option
changes, hash/configuration mismatch, and refusal to build without
`--execute`. The website gate and repository lint follow the normal owner
procedure; the known pre-existing CI retry fixture is handled in its own fix.
