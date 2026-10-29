# Running LineSweeperFrameBench on a local machine

This is the desktop procedure for `LineSweeperFrameBench`, separate from the
EC2 lane in [tools/cloud_performance/README.md](../../tools/cloud_performance/README.md).
A run is a declared hardware experiment outside CTest
([PHILOSOPHY, Performance](../design/PHILOSOPHY.md#performance)): its result
belongs to one named machine, driver, build and display session, so the
procedure records those beside the JSON. Read
[the ratchet document](2026-09-19-g6f-ratchet.md) before reading any result,
and its [measurement decision](2026-09-19-g6f-ratchet.md#measurement-decision-sm-03)
for which column supports which claim.

## What a run takes

| Argument | Value here | Why |
|---|---|---|
| `--output` | `out/performance/local/<date>/<run>.json` | Required. Must end in `.json`; the bench refuses a path that exists, so evidence is never overwritten. |
| `--run` | `local-<commit>-<backend>-<repetition>` | Required. ASCII letters, digits, `.`, `_`, `-`. The JSON records no commit, so the identifier carries it. |
| `--release-hash` | SHA-256 of the `LineSweeperFrameBench.exe` that runs | Required, 64 hex characters, stored as `release_sha256`. Locally it binds the exact executable; the EC2 lane passes its bundle's release-manifest hash instead. |
| `--warmup` | `1800` | Frames drawn and discarded first (the default). |
| `--sample` | `3600` | Frames retained (the default): a minute at 60 Hz. |
| `--pacing` | `presentation` | Synchronised presentation paces the frames; the benchmark adds no limiter. This is the measurement decision's choice. `software` reproduces the ratchet's two-clock state and is for that investigation only. |

`--refresh` sets the software pacer's rate and does not affect timing under
presentation pacing, so it is not passed. Its default still lands in the JSON
as `workload.refresh_hz` 60 and `target_frame_ns`; under presentation pacing
those are not the display's rate, which the machine file records. The presets
are `x64-release-d3d12` and `x64-release-vulkan`, the pair SM-07's low-tier
capture selects; `x64-release` (D3D11) and `x64-release-gl` take the same steps. The bench
refuses a Debug build.

## Before running

- A clean checkout at the commit to measure. The script refuses a dirty tree.
- A Developer PowerShell for Visual Studio (x64), which puts CMake, Ninja and
  `VCPKG_ROOT` on the environment, with `VULKAN_SDK` set for the Vulkan preset.
- The display mode the run is meant to describe, set before starting. Close
  what else draws or computes. The bench opens a 1280x720 window. Resizing it,
  closing it, suspending the session or losing the device refuses the run;
  covering or moving it does not, but either can change presentation, so leave
  it uncovered and in place.
- On a machine with more than one adapter, select the one to measure first
  (for the Radeon, [SM-01](../product/STORIES.md#sm-01)); the JSON's
  `render_device` says which adapter the backend actually used.

## The run

From the repository root, paste this block whole:

```powershell
$ErrorActionPreference = 'Stop'
if (git status --porcelain) { throw 'Commit or stash first: the run id names the commit.' }
$commit = git rev-parse --short=12 HEAD
$repetition = 1
$out = "out/performance/local/$(Get-Date -Format yyyy-MM-dd)"
New-Item -ItemType Directory -Force $out | Out-Null

# The session the result belongs to: adapters, drivers, display mode and OS.
$machine = "$out/machine-$commit-$repetition.txt"
if (Test-Path -LiteralPath $machine) { throw "Raise `$repetition: $machine exists." }
Get-CimInstance Win32_VideoController |
    Format-List Name, DriverVersion, CurrentHorizontalResolution,
        CurrentVerticalResolution, CurrentRefreshRate |
    Out-File -Encoding utf8 $machine
Get-CimInstance Win32_OperatingSystem | Format-List Caption, Version |
    Out-File -Encoding utf8 -Append $machine

foreach ($preset in 'x64-release-d3d12', 'x64-release-vulkan') {
    cmake --preset $preset
    if ($LASTEXITCODE) { throw "$preset configure failed" }
    cmake --build --preset $preset --target LineSweeperFrameBench
    if ($LASTEXITCODE) { throw "$preset build failed" }

    $exe = "out/build/$preset/bench/LineSweeperFrameBench.exe"
    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $exe).Hash.ToLowerInvariant()
    $backend = $preset -replace '^x64-release-', ''
    $run = "local-$commit-$backend-$repetition"
    & $exe --output "$out/$run.json" --run $run --release-hash $hash `
        --warmup 1800 --sample 3600 --pacing presentation
    if ($LASTEXITCODE) { throw "$preset run failed" }
}
```

Each backend takes its build time plus 5,400 frames, 90 seconds at 60 Hz.
Each run prints `Wrote 3600 retained frames to <path> (<backend>, <class>)`.
A refusal prints the usage line and the reason, exits 1 and writes nothing.
Run the block again with `$repetition` raised for another repetition.

## What a run establishes

The JSON's `render_device` names the adapter, vendor and device the backend
used, and `present_mode` with the swap intervals names how it presented.
`DriverVersion` in the machine file is Windows' driver version string, not
the vendor's marketing number. Under presentation pacing the frame-start
interval measures cadence against the display, not scan-out. `update_ns` and
`record_submit_ns` are CPU-observed phases and say nothing about GPU completion
or latency; the measurement decision lists what each claim needs instead.
A run on a desktop GPU is not low-tier evidence: the low-tier box in
[backend-equivalence-2](../review/backend-equivalence-2/STATUS.md) names the
configuration that is. Results stay under ignored `out/`; a run quoted as
evidence names its JSON, machine file and commit.

## First run, 2026-10-29

Followed verbatim at `71b6af38c112` on the RTX 5080 desktop (Windows driver
version string `32.0.16.1692`, NVIDIA 616.92; 2560x1440, reported 59 Hz;
Windows 11 Pro 10.0.26200), with no other step. Both runs wrote `complete`
results of 3,600 retained frames under `out/performance/local/2026-10-29/`,
`hardware_raster` on the RTX 5080: D3D12 with `dxgi_sync_interval` 1, Vulkan
with `fifo`. Nearest-rank values, in milliseconds:

| Backend | `update_ns` p50 / p99 | `record_submit_ns` p50 / p99 | `scheduled_interval_ns` p50 / p99 / max | mean interval |
|---|---|---|---|---|
| D3D12 | 0.132 / 0.201 | 0.667 / 1.072 | 16.664 / 17.318 / 100.954 | 16.700 |
| Vulkan | 0.128 / 0.158 | 0.508 / 1.243 | 16.670 / 17.579 / 18.128 | 16.681 |

The D3D12 run has eight intervals over 20 ms: one of 101 ms at retained frame
146, five of 33.1 to 33.9 ms (frames 248, 1350, 1352, 2559 and 2561) and two of
20 and 21.5 ms. In every one, `update_ns` and `record_submit_ns` stayed under
0.8 ms; seven of the eight spent 10.9 to 15.8 ms in `begin_ns`, which includes
the frame-slot wait, and frame 146 shows no wait in any phase. The desktop
session was not otherwise controlled, and no cause is claimed. This run qualifies the
procedure; it is not low-tier evidence and supplies no claim the measurement
decision assigns to the Radeon configuration.
