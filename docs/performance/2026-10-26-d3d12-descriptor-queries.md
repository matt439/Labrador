# D3D12 descriptor-query measurement, 2026-10-26

Retain the current descriptor queries on this measured runtime. The warmed
getter loops measured medians of 1.4386, 1.4399 and 1.4300 ns per operation;
the frozen LineSweeper workload invoked those getters 12, 12 and 2 times
per frame. These results do not justify adding a production cache. Broader
profiling remains open if another device, workload or frame budget makes
these calls material.

This answers the bounded measurement question in
[D3D12 review section 6, item 2](../review/d3d12/README.md#6-not-verified).
It does not establish a universal bottleneck, a whole-frame improvement,
a frame-time p99, or acceptance on PHILOSOPHY's Radeon low tier.

## Runtime and source identity

The raw timing receipt is
`out/overnight-2026-10-26/descriptor-query-raw.log`.
The final census receipts are
`out/overnight-2026-10-26/descriptor-counts-20261026-5.json` and its
`.descriptor-counts.csv` sidecar. These are retained local artifacts; the
tables below preserve the timing samples and census result in the repository.

| Recorded field | Value |
|---|---|
| Configuration | `x64-release-d3d12`, Release |
| Selected device | NVIDIA GeForce RTX 5080; hardware Direct3D 12 |
| Vendor / device IDs | `4318` / `11266` (`0x10de` / `0x2c02`) |
| Adapter LUID in raw probe | `00000000:00016ba0` |
| `_MSC_FULL_VER` | `195136256` |
| QPC frequency | `10000000` ticks/second |
| Configure-time source revision | `d2ee5b101375bead6aba3b322a77e0f2b7c6509b-dirty` |
| Census presentation request | DXGI sync interval 1; `--pacing presentation` |

The engine source hashes recorded by both probes were:

```text
engine/render/d3d12/renderer.cpp
  a89726256c72af3d7a3a9ca2494354fb4599a0834432c05ab734ffde7cc84d0c
engine/render/d3d12/device_resources.cpp
  d8492d7b3d2620172afcb29696160674d532fba4e4d5a431d852823c4aaadcb1
```

These are configure-time labels, including an explicit dirty-tree suffix,
not a claim of a clean checkout. Reconfigure after changing the measured
sources. The raw receipt does not record the CPU model or driver version;
the adapter LUID identifies this runtime instance rather than a portable
hardware identity. No affinity, power or global operating-system settings
were changed by either probe.

## Separate timing and invocation probes

[D3D12DescriptorQueryProbe](../../bench/d3d12_descriptor_query_probe.cpp)
links the normal engine and calls the unmodified
`Renderer::Impl::sampler(TextureFilter::point)`, `texture_slot_gpu(0)` and
`DeviceResources::back_buffer_view()` methods. A real hidden 320-by-200
`render_tests::TestWindow` supplies the device's HWND. Each method and each
volatile cached-handle baseline receives 100,000 warm-up iterations, then
seven samples of 1,000,000 iterations on one calling thread. QPC brackets
each aggregate loop; output occurs afterward. Unsigned checksums retain the
returned values and are checked against the corresponding handle sum.

The loops include loop control, accumulation and method-call work. The
volatile baselines force a cached-handle load each iteration; they are
controls for the experiment, not a proposed production cache. Calls from
this probe cross a translation-unit boundary, whereas production
`View::flush()` and the two GPU-handle helpers share a translation unit and
may be inlined differently. Subtracting the control therefore does not
isolate a guaranteed production optimization.

[D3D12DescriptorCountProbe](../../bench/d3d12_descriptor_count_main.cpp)
uses a separate
[renderer wrapper](../../bench/d3d12_descriptor_count_renderer.cpp). After
preloading declaration headers, it appends counters at the actual getter
call sites, resets after the frame wait and snapshots after present. Relaxed
atomic counters permit view fan-out; rows are allocated before the workload.
The wrapper reuses
[LineSweeperFrameBench's entry point](../../bench/linesweeper_frame_bench.cpp)
and normal content dependencies. Its hidden-window adapter queries the
actual client size and forwards that size to the benchmark's window notify.
It does not copy or replace the game workload.

The census is intrusive. Besides counters and hidden startup, this run
emitted eight progress messages at completed-frame counts
`1, 2, 4, 8, 16, 32, 64, 128`; it performs no per-frame CSV writing.
The JSON explicitly records
`measurement_class=instrumented_descriptor_counts_not_performance_acceptance`.
The reused entry point's stdout still prints `hardware_raster`; use the
JSON classification when interpreting the artifact. Neither its frame
timings nor its hidden presentation supply pacing or low-tier acceptance.

Both targets are D3D12-only, `EXCLUDE_FROM_ALL` and outside CTest. They have
no wall-clock pass thresholds. Timing and counting run in separate
executables so counter increments cannot contaminate the raw getter loops.

## Raw timing samples

All entries below are raw QPC ticks for 1,000,000 operations. Column names
are the exact method labels in the receipt.

| Repetition | sampler_point | sampler_cached_load | texture_slot_gpu_0 | texture_cached_load | back_buffer_view | target_cached_load |
|---|---|---|---|---|---|---|
| 0 | 14227 | 1747 | 14443 | 1748 | 14141 | 1766 |
| 1 | 14376 | 1767 | 14401 | 1773 | 14285 | 1805 |
| 2 | 14334 | 1799 | 14373 | 1777 | 14300 | 1767 |
| 3 | 14462 | 1930 | 14392 | 1770 | 14345 | 1775 |
| 4 | 14434 | 1775 | 14393 | 1796 | 14333 | 1779 |
| 5 | 14395 | 1777 | 14399 | 1763 | 14296 | 1784 |
| 6 | 14386 | 1772 | 14439 | 1772 | 14328 | 1782 |

At 10,000,000 ticks/second, one tick is 100 ns. Thus
`ns/operation = raw_ticks × 100 / 1000000 = raw_ticks / 10000`.
The median is the fourth sorted observation of seven; min and max are
observed endpoints, not confidence bounds.

| Method | Min ns/operation | Median ns/operation | Max ns/operation |
|---|---|---|---|
| `sampler_point` | 1.4227 | 1.4386 | 1.4462 |
| `sampler_cached_load` | 0.1747 | 0.1775 | 0.1930 |
| `texture_slot_gpu_0` | 1.4373 | 1.4399 | 1.4443 |
| `texture_cached_load` | 0.1748 | 0.1772 | 0.1796 |
| `back_buffer_view` | 1.4141 | 1.4300 | 1.4345 |
| `target_cached_load` | 0.1766 | 0.1779 | 0.1805 |

Each getter and its baseline returned the same checksum in every repetition:

| Pair | Unsigned checksum modulo 2^64 |
|---|---|
| Sampler | `13772419492883578880` |
| Texture slot | `11178347156350173184` |
| Back-buffer view | `1994071635376000000` |

## Actual workload invocation counts

The final run completed with exit 0: 12 warm-up frames followed by 120
retained frames. Parsing every CSV row gives 132 contiguous ordinals
`0..131`, each with exactly:

| Getter | Calls per frame | Total across all 132 frames |
|---|---|---|
| `sampler` | 12 | 1584 |
| `texture_slot_gpu` | 12 | 1584 |
| `back_buffer_view` | 2 | 264 |

The JSON identifies the real frozen `full_well_top_out` workload at
1280-by-720 with exactly 9,600 live particles. Its one-view scene draws the
board, particle field, top-out banner and keyboard/pad text in that order.
The census counts method invocations, not
the distribution of sampler filters or texture-slot arguments; the raw
microprobe specifically uses point filtering and slot zero.

The successful process receipt spans
`2026-10-26T21:42:38.5685047Z` to
`2026-10-26T21:43:12.7526362Z`: 34.1841315 seconds within its 50-second outer
limit. Earlier 30-second limits left incomplete runs. Those incomplete
runs did not establish a GPU hang or an access denial; 30 seconds was
insufficient for this completed execution.

Multiplying the independently measured counts by the method medians gives
this arithmetic model:

```text
12 × 1.4386 + 12 × 1.4399 + 2 × 1.4300 = 37.4020 ns/frame
12 × 0.1775 + 12 × 0.1772 + 2 × 0.1779 =  4.6122 ns/frame  (volatile controls)
```

This is a sum of medians from independent microloops, not a measured
per-frame median, measured saving or bound on saving. It ignores different
call-site optimization, cache lifetime/invalidation and any interactions
with the rest of the frame. It supports retaining these queries on this
named runtime; it does not replace a profile of production frame work.

Read [the ratchet record](2026-09-19-g6f-ratchet.md) before interpreting
frame benchmark phases: waits can move between begin, present and a pacer.
This census uses presentation-driven pacing, so the historical software-pacer
ratchet is not an established explanation for its waits. Their cause was
not investigated here, and none of its whole-frame or p99 values is used
to judge descriptor-query cost.

## Repeat the bounded experiment

From a PowerShell 7 x64 Visual Studio developer shell with `VCPKG_ROOT` set
and `fxc` available, configure fresh and build only the requested optional tools:

```powershell
cmake --preset x64-release-d3d12
cmake --build --preset x64-release-d3d12 --target D3D12DescriptorQueryProbe D3D12DescriptorCountProbe --parallel 2

$probeOutput = Join-Path $PWD 'out\descriptor-query-reproduction'
New-Item -ItemType Directory -Force -Path $probeOutput | Out-Null
function Invoke-DescriptorProbe($probePath, $probeArgs, $label, $limitMs) {
    $owned = Start-Process -FilePath $probePath -ArgumentList $probeArgs `
        -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $probeOutput "$label.stdout.log") `
        -RedirectStandardError (Join-Path $probeOutput "$label.stderr.log")
    $owned.Id | Set-Content (Join-Path $probeOutput "$label.pid.txt")
    if (-not $owned.WaitForExit($limitMs)) {
        try { $owned.Kill($true) }
        catch { throw "Termination uncertain for owned PID $($owned.Id): $_" }
        if (-not $owned.WaitForExit(5000)) {
            throw "Termination uncertain for owned PID $($owned.Id); inspect before retrying."
        }
        throw "Owned PID $($owned.Id) exceeded its execution limit and was terminated."
    }
    if ($owned.ExitCode -ne 0) { throw "$label exit: $($owned.ExitCode)" }
}
$queryProbe = (Resolve-Path .\out\build\x64-release-d3d12\bench\D3D12DescriptorQueryProbe.exe).Path
Invoke-DescriptorProbe $queryProbe @('1000000') 'raw' 30000

$countProbe = (Resolve-Path .\out\build\x64-release-d3d12\bench\D3D12DescriptorCountProbe.exe).Path
$countHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $countProbe).Hash
$countArgs = @('--output', ('"{0}"' -f (Join-Path $probeOutput 'counts.json')),
    '--run', 'descriptor-query-reproduction', '--release-hash', $countHash,
    '--warmup', '12', '--sample', '120', '--pacing', 'presentation')
Invoke-DescriptorProbe $countProbe $countArgs 'counts' 60000
```

The count command writes `counts.json.descriptor-counts.csv` after the
workload. Keep its JSON beside the CSV for device, workload and
instrumentation classification. Allow a bounded runtime comfortably longer
than the observed 34 seconds on this host; a deadline is an execution
limit, not a performance threshold. Reproduction on a different CPU,
driver or adapter needs its own retained raw samples and interpretation.

## Verification

Both optional targets compiled under the existing warning settings in
`x64-debug-d3d12` and `x64-release-d3d12`. Debug invocations refused measurement
with exit 1 and created no result JSON. The Release timing and final census
invocations above exited 0. The wrapper object defines the renderer methods,
and the two link commands contain the wrapper only in the census executable;
no forced archive extraction or duplicate-symbol linker option was used.
Nonzero paired census rows also guard against silently selecting the ordinary
renderer instead of its instrumented translation unit.

All four standalone CMake checks, the website check, its 21 contract tests and
its production build passed. No engine source, golden image, normal benchmark
or CTest registration changed. Hosted CI and other devices were not run.
