# The Direct3D device kind: issue #125

**Checked by reading, 2026-11-10. Not fixed, and not decided.**
[Issue #125](https://github.com/matt439/Labrador/issues/125) is a bug report:
on a machine with no GPU, a `d3d11` build's `Renderer::device_info()` names
the Microsoft Basic Render Driver, vendor `0x1414`, device `0x8c`, and gives
its kind as `RenderDeviceKind::hardware`. This file records what the code
does, how far the claim can be confirmed without such a machine, and the
change that would answer it. The issue stays open.

## What the code does

Rechecked at `043589ee4f2a8d0ec872b9b1223558103541b993`. Line numbers will
drift.

**One bit decides the kind in both Direct3D backends.**
`engine/render/d3d11/renderer.cpp:59-60` and
`engine/render/d3d12/renderer.cpp:40-41` are the same expression: `software`
when the adapter's `DXGI_ADAPTER_DESC1::Flags` has
`DXGI_ADAPTER_FLAG_SOFTWARE`, otherwise `hardware`. Neither reads the
adapter's name, vendor or device number to decide it, although both copy all
three into the record on the lines above. D3D11 reads the description from
the device's own adapter after creation (`:33-49`); D3D12 uses the
description `DeviceResources` kept when it chose the adapter
(`engine/render/d3d12/device_resources.cpp:242-245`).

**The same bit decides which adapter is taken.** D3D11's `IsUsableAdapter`
(`engine/render/d3d11/device_resources.cpp:76`) and D3D12's
`acceptable_adapter` (`engine/render/d3d12/device_resources.cpp:54`)
refuse an adapter only when that flag is set; D3D12 also requires that the
adapter can make a device at feature level 11_0. Both comments say the
refusal is of "the Basic Render Driver", but the code tests the flag, not the
driver. A release build throws `No Direct3D hardware device found`
(`d3d11/device_resources.cpp:168`) or
`No Direct3D 12 hardware device found` (`d3d12/device_resources.cpp:213`) only
when no adapter passes. A debug build falls back to WARP instead.

**So an adapter that is software and does not carry the flag is selected as
hardware and reported as hardware, in debug and release alike.** That
follows from the code alone. The release refusal of CPU rasterisers (T6, and
AGENTS.md's "release has no WARP fallback") is exactly as strong as the flag.

**The other two rasterising backends do not rest on one signal.** OpenGL
reports `software` when its context is a software one or when the vendor and
renderer strings contain any of seven names, one of which is
`microsoft basic render` (`engine/render/gl/renderer.cpp:47-63`, `:86-88`).
Vulkan reports `software` for `VK_PHYSICAL_DEVICE_TYPE_CPU` or one of four
names (`engine/render/vulkan/renderer.cpp:33-45`, `:65-67`). The issue's
point that one machine can answer differently per backend follows from this
difference.

**No test pins the Direct3D answer.** `RenderPixelTests`' case *the renderer
reports the rasterising device it selected* requires only that the kind is
not `null_device` (`tests/render/pixel_tests.cpp:95`); `tests/render/null_tests.cpp:72`
pins `null_device` for the null backend. Nothing asserts `hardware` or
`software` for a named Direct3D adapter, and the classification is not a
function a headless test could call: it is inline in each backend's
`selected_device`. CI checks the drawing device by name for the GL and Vulkan
jobs only; the Direct3D jobs' `software_device` is empty, so no CI log
carries the adapter those jobs drew on.

**This tree already holds half of the report's premise.**
`.github/workflows/ci.yml`'s comment on the Direct3D jobs reasons that
`x64-release` passes `RenderPixelTests` on a runner with no GPU, that a
release build has no WARP fallback, and that the runner therefore "offers a
DXGI adapter that GetHardwareAdapter accepts - one without
DXGI_ADAPTER_FLAG_SOFTWARE". It also says that a log cannot say which
rasteriser that is. The issue supplies the name. By contrast,
`tools/consumer_check/README.md` says that Windows Sandbox with its virtual
GPU disabled leaves "only Windows' software adapter", which a release build
does refuse. If both records are right, the flag is present on one
software-only machine and absent on another, which is the reason one bit is
not enough.

## Whether the claim holds

**It holds as far as reading can take it.** Given an adapter with the
reported description and without the flag, both backends return `hardware`:
there is no other branch. The engine's own CI comment establishes
independently that a GPU-less runner offers an unflagged adapter to a
release build.

**What was not established here:** that the runner's unflagged adapter is
the one named in the issue, with those two numbers. No machine with only the
Basic Render Driver was run for this record, and no CI log prints it. The
quickest confirmation needs no new code: run the existing test case alone in
a Direct3D CI job, as the GL and Vulkan jobs already do, and read its
`Drawing on` line.

## What it breaks

- [`renderer.h`](../../../engine/render/renderer.h) says the kind "lets a
  measurement refuse WARP or another CPU rasteriser", and
  [SEAM.md](../../../engine/render/SEAM.md) section 3 calls it a
  hardware/software/null classification. A CPU rasteriser reported as
  `hardware` makes both sentences false on that machine.
- `tools/cloud_performance/analysis.py` refuses a benchmark whose device
  kind is not `hardware`. It also compares the vendor number with the
  declared one, so that lane has a second guard. A consumer reading the kind
  alone has none.
- `tools/linesweeper_capture` and the D3D12 probes print the kind as a fact
  about the run.
- A release build starts on such a machine, where the engine's stated rule
  is that it refuses.

## The change that would answer it

Not made in the commit that adds this file. In two parts, because the second
is an owner decision and the first is not.

**1. Report the truth (a defect fix).** Move the classification out of the
two `selected_device` functions into one headless function in
`engine/render/`, beside `throw_if_failed.h`, which both Direct3D backends
may include without crossing the backend wall. It takes the three values a
`DXGI_ADAPTER_DESC1` supplies as plain integers and answers `software` when
the flag is set or when the vendor is `0x1414` and the device is `0x8c`, the
pair the issue reports for the Basic Render Driver. Taking integers, not the
DXGI struct, lets its test run in every preset. A new header under
`engine/` is published or left out in `website/src/lib/published.ts` in the
same commit.

- Tests in the same commit: the flagged case, the reported unflagged triple,
  and an ordinary vendor's adapter answering `hardware`.
- CI: give the Direct3D matrix entries the device-name check the GL and
  Vulkan entries have, and print the kind on the same line, so the job log
  states what drew and how it was classified. That turns the premise above
  from an inference into a record, and it is what would catch a regression.
- Documents: one sentence in `renderer.h` and SEAM.md section 3 saying what
  each kind promises (`software` means the API or the engine's list of known
  CPU rasterisers says so; `hardware` means neither did, which is not proof
  of a GPU), and the correction of the two "Basic Render Driver" comments in
  the selection code.
- CHANGES.md: a silent behaviour change. A game or tool that reads the kind
  sees `software` where it saw `hardware` on such a machine.

**2. Whether a release build still starts there (the owner's call, T6).**
If adapter selection uses the same function, a release build on a machine
whose only adapter is the unflagged Basic Render Driver throws
`No Direct3D hardware device found`, as it already does where the flag is
set. That is what the no-fallback rule says, and it is consistent. Its
costs are immediate: CI's two release Direct3D runs on a GitHub runner
(the `x64-release` matrix entry and the `consumer` job's installed game)
start on an unflagged adapter today and would stop, so CI's release device
tests would need a software path of their own or would stop running there.
CI does not run `x64-release-d3d12`; `tools/ci_gap_report.py` lists it among
the presets CI leaves out. And the
reporting client says its own release start-up check depends on the present
behaviour, while asking that it not be kept for its sake. Keeping selection
as it is and fixing only the report is also coherent: the engine then says
honestly that a release build may start on an unflagged CPU rasteriser, and
`renderer.h` must say so.

## Not verified

- The flag's state for the Basic Render Driver on any machine. This record
  reads the engine's code and two of its own documents; it ran no device.
- That `0x1414` and `0x8c` identify the Basic Render Driver on every Windows
  version. The pair is the issue's observation, and the fix above should
  cite Microsoft's documentation for it or fall back to the name, as the GL
  backend does.
- Whether a debug build's WARP fallback device reports the flag. It decides
  whether today's debug fallback is labelled `software`; the function above
  makes the answer the same either way.
