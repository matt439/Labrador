# Native process memory and first-frame observations

SM-87 adds optional reports to `LineSweeperFrameBench` and the minimal
sample's native smoke path. They describe one executable, configuration,
workload and desktop session. They impose no timing or memory threshold and
provide no leak, low-tier, GPU-memory or display-pacing verdict.

## Meaning and endpoints

| Field | Meaning |
|---|---|
| `peak_working_set_bytes` | Windows' process-lifetime peak resident working set through the single memory query. It includes resident shared pages and is not private committed memory. |
| `private_bytes_at_exit_endpoint` | `PROCESS_MEMORY_COUNTERS_EX::PrivateUsage`, the process's private committed bytes at the named live-process endpoint. It is neither a resident peak nor a measurement after process exit. |
| `process_creation_utc_100ns` | `GetProcessTimes`' creation timestamp, in 100 ns units since 1601-01-01 UTC. It precedes the C++ entry point and includes loader/startup time. |
| `first_completed_frame_utc_100ns` | `std::chrono::system_clock::now()` immediately after the first successful `Renderer::end_frame`, converted from the Unix epoch to the same FILETIME epoch. |
| `process_start_to_first_completed_frame_100ns` | The difference of those two UTC timestamps. Multiply by 0.0001 for milliseconds. UTC adjustments can distort it; an endpoint before process creation is explicitly unavailable. |

The memory fields use the documented
[PROCESS_MEMORY_COUNTERS_EX counters](https://learn.microsoft.com/en-us/windows/win32/api/psapi/ns-psapi-process_memory_counters_ex).
[GetProcessTimes](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getprocesstimes)
supplies process creation, rather than kernel/user CPU execution time. The
separate memory-sample timestamp, taken immediately after the counter query, uses
[GetSystemTimePreciseAsFileTime](https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getsystemtimepreciseasfiletime).
These are UTC wall-clock endpoints, not monotonic interval measurements.
Neither `end_frame` returning nor this timestamp establishes GPU completion,
compositor visibility or scan-out.

`FrameTimings::first_completed_frame_utc` remains absent until the first
frame is published. The clock is read at that frame's `end_frame` boundary,
then committed only with its successful publication. A frame that throws
publishes nothing. The first published endpoint, including an unavailable
one in a synthetic fixture, is retained across later frames, clock resets
and suspension/resume. No memory or process API is polled per frame.

The reports state the memory endpoint explicitly:

- Benchmark: after the frame loop, before benchmark destruction and result
  assembly. Resources and the retained sample buffer are still owned.
- Minimal smoke: after `Application::run` returns, before Application
  destruction and report assembly. States/resources still exist. A failed
  startup or run reports its exception-unwind endpoint instead and retains
  an unavailable first frame where none was published.

Null numeric values mean unavailable or not requested. `memory_status` and
`startup_status` distinguish those states, unsupported platforms, failed
Windows queries and backwards UTC endpoints; Windows error codes are retained.
Zero is a valid observed duration. A process id of zero accompanies unrequested or unsupported observations, not a sampled native process. Browser observations are unsupported;
browser frame intervals are not used to invent a native process-start time.

## Opt in

From the native toolchain environment described in
[the local frame-benchmark procedure](local-frame-bench.md), prepare a Release
build with the optional smoke reporting enabled:

```text
cmake --preset x64-release -DLABRADOR_SAMPLE_PROCESS_OBSERVATIONS=ON
cmake --build --preset x64-release --target MinimalSample LineSweeperFrameBench
```

Run `out/build/x64-release/samples/minimal/MinimalSample.exe --smoke-test`
with stdout and stderr retained. It remains hidden and uses its ordinary
smoke workload; stdout gains one JSON record. The configure-time source
revision, configuration, completed-frame count and actual render device/API
are recorded. Reporting is off by default; it adds no player-facing UI.
Copies may opt in with the same two target definitions from the sample's
CMake file, using their own source identity. A default starter needs no
extra source, package or tool. A loader failure or failure before the reporter is constructed cannot emit JSON; a missing report is missing evidence, never zero startup time.

For the benchmark, add `--process-observations on` to an explicitly selected
run. The default is `off`. Its existing release SHA-256, source revision,
workload, device and timing data remain in the record. The separate
`window_display` object from SM-86 describes nominal configured display
metadata; it does not change either startup endpoint or the pacing verdict.
Follow the full local procedure for a performance experiment. A two-frame
instrumentation check is useful only for checking collection and format.

Retain the executable's SHA-256, compiler/build settings, source identity,
machine/OS/driver, window visibility and exact arguments with each report.
Compare only the same workload and endpoints. The memory snapshot precedes
the allocations needed to serialize its own report; peak working set still
includes earlier initialization and any preceding workload allocations.

## Verification

AppTests hold the first-publication, failure, reset/resume and unknown
endpoint contracts with synthetic clocks and native-null lifecycle cases.
The observation helper tests UTC conversion, unknown/failed/backwards
endpoints, zero elapsed time, JSON escaping and the platform's process-creation
source. LineSweeperViewTests parse known and unavailable process-report JSON
without a graphics device. These are correctness checks without numeric
performance limits. A dated bounded collection is recorded separately below.

The local/cloud reducer preserves each repetition's source/build identity
and optional process observations. It checks numeric/null shape and observed
startup arithmetic, and retains unknown legacy metadata as null. These
fields are neither pooled into frame-phase summaries nor used to choose a
pacing target, device identity or performance verdict.

## Bounded collection, 2026-11-17

[The raw reports and receipts](2026-11-17-native-process-observations.json)
retain the executable hashes, LF-normalized code hashes and exact arguments.
This was a loaded Windows 11 desktop (build 26300), Ryzen 9 9950X3D, RTX 5080,
Windows driver 32.0.16.1692. The measured branch was `5cff44b9` plus this
instrumentation, MSVC 19.51.36231 x64 Release `/O2 /Ob2` without IPO. It does
not qualify the later integrated IPO/display-metadata changes or a release
candidate. No affinity hold or otherwise controlled performance session was
applied.

| Collection | Peak resident working set, bytes | Private committed bytes at named exit endpoint | Process creation to first completed CPU frame, ms |
|---|---:|---:|---:|
| Hidden minimal smoke, 30 completed frames | 49,741,824 | 63,832,064 | 173.2501 |
| Shown frame bench, one warm-up and one retained frame | 48,857,088 | 65,830,912 | 152.2634 |

The normal smoke's creation timestamp matched an independently launched
`Process.StartTime` converted to FILETIME exactly. Both elapsed values equal
their retained raw UTC endpoint differences. Their precision is a storage
unit, not a claim of timing accuracy. The missing-manifest fixture exited 1,
reported zero completed frames and null first-frame/elapsed values with its
exception-unwind endpoint; the copied manifest was restored immediately.
Source/device identity remained explicit, with device details unknown for
that unpublished startup.

These short runs check collection and failure reporting. They provide no
startup baseline, pacing evidence, leak result or low-tier acceptance.
Native-null and wasm GL/Node AppTests pass, as do Release D3D11
LineSweeperViewTests and the offline analysis tests. The combined app matrix,
website gate and hosted CI remain separate integration checks.

## Combined qualification, 2026-11-17

The combined seven-track app sweep, local lint and website gate subsequently
passed at `fadd1a63`, closing SM-87's mechanism/integration scope.
[The PR record](../verification/2026-11-17-pr-current.md) retains that evidence.
The bounded collection above keeps its source, build and workload; this adds
no performance baseline, low-tier, leak or physical-pacing verdict. Hosted
CI retains its separate PR result.
