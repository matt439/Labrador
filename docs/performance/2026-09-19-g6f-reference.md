# The first EC2 reference run — `g6f-reference-003`, 2026-09-19

**Investigation correction, 2026-09-19.** The original report mistook Vulkan's
frame-slot timeline wait for image acquisition, inferred a swapchain image count
that was never recorded, and converted a median interval into average Hz. Those
interpretations are corrected below; the retained samples and percentile tables
are unchanged. Section 7 records the resulting fixes and their verification.

**What this is.** The first complete run of the lane under
[`tools/cloud_performance/`](../../tools/cloud_performance/README.md):
`LineSweeperFrameBench` on one declared `g6f.2xlarge`, five repetitions of
sixty seconds at 60 Hz for each of the four rasterising backends, with the
host's identity and every raw frame retained. It is a **reference profile**
for that host and nothing else. The README says so and PHILOSOPHY's low tier
is a Radeon configuration this document does not touch; the box in
[`backend-equivalence-2/STATUS.md`](../review/backend-equivalence-2/STATUS.md)
that asks for a p99 on the named low tier stays open. What this run gives
the tree that it did not have is a measured p99 on hardware that is not the
desktop the engine was written on, and a record of what it took to get one.

**Why the raw samples matter more than the table.** The tool's aggregate for
two backends says p99 ≈ 17 ms and ≈ 32 ms. Both numbers are true and both are
misleading: they come from one repetition each, in which every frame — or
exactly every second frame — spent that time inside renderer calls, and the
other four repetitions of the same backend show nothing of the kind. §3 gives the
aggregate as the artefact states it and §4 gives the repetitions, because the
second table is the one a reader should reason from.

## 1. Identity

| | |
|---|---|
| Run | `g6f-reference-003`; stack `labrador-performance-g6f-reference-003`, ap-southeast-2 |
| Source | commit `7b72313`, clean; bundle `d352629e…`, release manifest `6fa73f0c…`, config `aae76b13…`, worker `cc57a06f…` |
| Image | `ami-071dc9daa2faeaea7` (`labrador-performance-runner-v1-20260919-0305`): Windows Server 2022 Datacenter 20348, NVIDIA GRID 19.5 / driver 582.53, VC++ 14.44 runtime, AWS.Tools 5.0.302; console auto-logon user `labrador-bench`; the Nitro basic display adapter disabled |
| Instance | `i-06a54363af83973e8`, `g6f.2xlarge`, ap-southeast-2a: AMD EPYC 7R13 at 2.65 GHz, **4 cores, SMT off**, 30.8 GiB; **one quarter of an NVIDIA L4** presented as `NVIDIA L4-6Q` (PCI `10DE:27B8`); one display, 2560×1600 at 59 Hz, High performance power plan |
| Sessions | worker as `SYSTEM` in session 0; each benchmark process in session 1 (console) as `labrador-bench`, started by a scheduled task with an interactive logon type |
| Workload | `full_well_top_out`: 1280×720 client window, 9,600 live particles, 1,800 warm-up frames then 3,600 retained frames per repetition, paced at 60 Hz; five repetitions per backend, d3d11 → d3d12 → gl → vulkan |
| When | host captured 05:03:39Z; first frame 05:03:49Z; last 05:34:50Z; `success.json` 05:35:08Z (15:03–15:35 AEST) |
| Evidence | `s3://labrador-performance-077207386906-apse2/labrador-performance/runs/g6f-reference-003/` — 64 files under `output/`, hashed in `success.json`; local copy `out/cloud/g6f-reference-003/`, analysis `out/cloud/g6f-reference-003-analysis.json` (`complete: true`, 18,000 samples per backend) |
| Cost | about US$0.60 of compute for the run itself; the day's three builders, three verifications and two failed runs (§6) came to roughly US$6 |

## 2. What each column measures

The benchmark's loop is in
[`bench/linesweeper_frame_bench.cpp`](../../bench/linesweeper_frame_bench.cpp);
every number below is `std::chrono::steady_clock` wall time on the CPU,
including any wait the call performs, and nothing is a GPU timestamp.

- **update** — `Scene::update` and `end_tick`: the game's own work, the
  particle field included.
- **begin** — `Renderer::begin_frame`. On Vulkan this waits for the prior
  submission using the current frame slot, through `vkWaitSemaphores`; on
  D3D12 it is the fence wait for the frame's allocator; on D3D11 and GL it
  does nearly nothing. This phase does **not** acquire a Vulkan swapchain image.
- **record + submit** — `Scene::draw` and `Renderer::submit`: the draw
  walk and the command recording, which is the cost this workload exists to
  measure.
- **present** — `Renderer::end_frame`: `Present` with sync interval one on
  both Direct3D backends, `SwapBuffers` under the driver's default on GL,
  image acquisition, command submission and `vkQueuePresentKHR` in FIFO on
  Vulkan. An acquired image may still require a GPU semaphore wait after the
  acquisition call returns; these CPU timings cannot measure that wait directly.
- **whole frame** — frame start to the end of present.
- **interval** — this frame's start minus the last one's: what the
  software pacer actually delivered. It asked for 16.67 ms and delivered
  **about 17.00 ms at the median in nineteen repetitions**, with
  a p99 near 18.0 ms; Vulkan-001 alternates long and short intervals instead.
  Every repetition's **mean** is 16.6664–16.6669 ms, approximately 60 Hz. The
  pacer uses absolute deadlines and catches up after a late frame, so the
  reciprocal of the median is not its average rate. The 0.33 ms median
  difference measures interval unevenness, not sustained rate loss or an
  independently measured sleep overshoot. No number here measures scan-out.

Percentiles are nearest-rank, as the tool computes them.

## 3. The aggregate, as the tool states it

18,000 frames per backend, all five repetitions pooled. Milliseconds.

| backend | update p99 | begin p50 / p99 | record + submit p50 / p99 | present p50 / p99 | **whole p50 / p95 / p99 / max** |
|---|---|---|---|---|---|
| d3d11 | 0.38 | 0.00 / 0.01 | 2.14 / 2.85 | 0.04 / 14.90 | **2.72 / 17.00 / 17.04 / 17.45** |
| d3d12 | 0.37 | 0.09 / 0.19 | 1.28 / 1.93 | 0.11 / 0.14 | **1.68 / 2.38 / 2.48 / 2.85** |
| gl | 0.37 | 0.00 / 0.00 | 1.49 / 2.19 | 0.02 / 4.54 | **2.11 / 6.07 / 6.12 / 14.98** |
| vulkan | 0.36 | 0.03 / 30.67 | 1.24 / 2.07 | 0.07 / 0.13 | **1.60 / 31.97 / 32.42 / 33.44** |

## 4. The repetitions

Whole frame, p50 / p99 in milliseconds, with the share of frames over 10 ms
and which phase held them. Each repetition is a separate process, started
by its own scheduled task, 92–96 seconds apart including dispatch overhead.

| rep | d3d11 | d3d12 | gl | vulkan |
|---|---|---|---|---|
| 001 | **16.98 / 17.12** — 100 % over 10 ms, all in present (14.10 / 14.97) | 1.80 / 2.49 | 2.05 / 2.54 | **1.45 / 32.85** — 50 % over 10 ms, all in begin (0.03 / 30.76) |
| 002 | 2.31 / 3.05 | 1.66 / 2.43 | 1.68 / 2.52 | 1.62 / 2.48 |
| 003 | 2.46 / 3.20 | 1.72 / 2.48 | 3.06 / 3.15 — present 0.98 / 1.66 | 1.64 / 2.54 |
| 004 | 2.52 / 3.11 | 1.66 / 2.53 | 5.11 / 6.14 — present 3.51 / 4.65 | 1.59 / 2.45 |
| 005 | 2.42 / 3.17 | 1.67 / 2.42 | 1.84 / 2.49 | 1.54 / 2.46 |

Update and record + submit are comparatively stable across the repetitions
— update p99 never above 0.60 ms, record + submit p50 within 0.15 ms of its
backend's pooled value. The large changes occur in present and, for Vulkan,
begin. CPU phase timings locate those changes but do not distinguish GPU work,
GPU semaphore readiness, driver scheduling and presentation backpressure.

**What the frames below 10 ms cost.** Taking every frame
under 10 ms: d3d11 2.43 / 3.14, d3d12 1.68 / 2.48, gl 2.11 / 6.12, vulkan
1.55 / 2.48 (p50 / p99 ms). A short local smoke of the same workload on the
desktop this engine is written on, an RTX 5080, puts the median near 1.5 ms;
a quarter of an L4 behind four EPYC cores is within a millisecond of that
at the median, and D3D11 is the slowest of the four to record and submit by
roughly 0.8 ms, which reads as the immediate context's per-draw state cost.
These conditional percentiles exclude the long frames; they cannot establish
whole-run headroom against a 16.67 ms budget.

**Two repetitions differ in kind, not degree, and both are a backend's
first.** d3d11-001 spent 14.1 ms at the median in `Present`: the whole
repetition ran near the display's cadence. The old artifact does not measure
pacer sleep separately. Its next four repetitions never waited that long.
vulkan-001 blocked waiting for its frame slot's previous submission for
~30.7 ms on exactly every second frame, and the frame after each stall
ran immediately (interval p50 1.45 ms) because the pacer was behind; its
next four repetitions have a p99 of 2.5 ms. d3d12's and gl's first
repetitions show nothing of the sort, so "first process after boot" is not
the explanation on its own — d3d11-001 was the first benchmark process on
the machine, but vulkan-001 started twenty-three minutes and fifteen
processes later. What the two share is being the first process of their
API on the host. Whether the presentation engine, DWM or the vGPU's display
path holds a different mode for a first swap chain, and releases it after
the first process exits, is a question this run poses and cannot answer;
§5 says what would. Vulkan-001's entire present phase, which includes the
CPU acquisition call, never exceeds 0.118 ms. The 30 ms stall therefore cannot
be a blocking CPU acquisition call in these samples.

**GL's swap is the one that moves between repetitions.** Three isolated
frames exceed 10 ms (one each in repetitions 001, 003 and 004), and
`SwapBuffers` cost 0.02 ms in two repetitions, ~1 ms in one and
~3.5 ms in another, which is the difference between a swap that returns
and one that waits part of a refresh. The benchmark used for this run inherited
the driver's default swap interval on GL rather than setting one. Section 7
records the fix for future captures.

**D3D12 is clean in all five**, p99 2.42–2.53 ms, present 0.10–0.14 ms:
neither its begin nor present call shows the large waits seen elsewhere.

## 5. What this run asks, and what would answer it

These questions came from the first run. The corrected attribution and the
code changes in section 7 are verified locally; the host-specific cause remains
unresolved.

1. **Is the first-process presentation mode repeatable?** Run the lane
   again on the same image. If d3d11-001 and vulkan-001 misbehave the same
   way and 002–005 do not, first-use host or driver state becomes a stronger
   hypothesis. Keep the first repetition: discarding it after inspecting the
   result would hide the observed condition. Any process preconditioning
   belongs in a separately declared experiment with its own retained evidence.
   If it does not recur, this run alone cannot identify the cause.
2. **What delays Vulkan's frame-slot completion?** The two-slot submission
   ring is known; the actual swapchain image count was not retained and cannot
   be inferred from the alternation. The stall waits on a timeline value for a
   submission that also depends on the acquired-image semaphore. GPU tracing
   or a controlled repeat is needed to distinguish that dependency from GPU
   scheduling and execution. `engine/render/vulkan/device_resources.h` now
   states these separate waits and why FIFO gives no one-refresh latency bound.
3. **Set GL's swap interval.** Fixed in section 7: the GL backend asks for
   interval one and the benchmark records the request and queried driver state.
4. **What is the 0.33 ms?** The original report called this `sleep_until`
   overshoot, but it is only median interval minus target. A `timeBeginPeriod`
   request or a spin for the last millisecond might reduce jitter but neither guarantees
   a wake-up deadline. The raw means show no sustained 0.33 ms overshoot;
   measure lateness against the deadline before assigning a sleep error.
   Section 7 records the chosen wait and the new retained timing fields.
5. **This host is a virtual display on a fractional GPU.** Its reported
   display rate is 59 Hz while the benchmark requests 60 Hz. Fractional-vGPU
   scheduling, contention and any driver frame-rate limit are possible
   influences, but other tenant occupancy and an active limiter were not
   measured. The run provides a reference for this declared host; it cannot
   establish presentation behavior on a player's machine.

## 6. How it was produced

The lane is documented in its README; what follows is what this run added
to that document, in the order it was learned, because each item was a
failed attempt first.

- **The benchmark cannot run where the worker runs.** SSM executes the
  worker as `SYSTEM` in session 0, whose window station has no display. A
  1280×720 window comes back clamped there; with that worked around, DXGI
  refuses a swap chain with `DXGI_ERROR_NOT_CURRENTLY_AVAILABLE`, so neither
  Direct3D backend can start, while GL and Vulkan start and present into
  nothing at ~32 ms a frame. The worker now starts each repetition in the
  console session, where the display, DWM and a logged-on user are
  (commit `a7e7978`).
- **The image needs one display.** With the Nitro basic adapter enabled its
  phantom 1024×768 monitor is primary: every Direct3D frame crossed adapters
  through DWM at ~125 ms and WGL bound the GDI fallback instead of the NVIDIA
  ICD. Disabled, all four backends measured 2–3 ms.
- **Sysprep strips the auto-logon, and a stale password survives it.** The
  first image came back with nobody on the console; the second logged on
  with the wrong password because Winlogon prefers a registry
  `DefaultPassword` over the LSA secret the answer file writes. The third
  image is the one above.
- **Two runs never started the worker.** `launch` handed
  `AWS-RunRemoteScript` an `s3://` URI, which that document rejects, and
  nothing before the agent on the runner parses the value; the fix then
  existed as a function nothing called. Both idle runners were stopped by
  hand. Commits `fccaa2a` and `7b72313`, the second with a test on the
  dispatch call itself.

To repeat the run: build and stage the four Release benches as the README
shows, `bundle`, put the two hashes and a fresh deadline in a copy of the
run configuration that names `ami-071dc9daa2faeaea7` and its five tags,
`render` and dry-run `launch`, then `launch --execute`, `collect`, `analyze`
and `stop --execute`. The image-building steps are not in the tree; the
README's recipe is what a script under `out/cloud/image-builder/` on the
machine that built this image reproduced three times, and turning it into
`tools/cloud_performance/build_image.py` is the obvious next addition.

## 7. Investigation and fixes after the first run

The retained evidence passes the revised analyzer without alteration: all
72,000 samples remain present and every pooled percentile is unchanged.
`out/cloud/g6f-reference-003-reanalysis.json` adds per-repetition mean cadence,
budget exceedances, counts of long begin/present calls and transitions between
long and short calls, and ranges of phase percentiles across repetitions.
Vulkan-001 has 1,800 long begin calls and 3,599 transitions in 3,600 samples:
exact alternation. The realized mean rates across all twenty repetitions are
59.9991–60.0009 Hz. No first repetition is discarded.

The GL backend now requires `WGL_EXT_swap_control`, explicitly requests interval
one, and records both the request and `wglGetSwapIntervalEXT`'s result. Missing
extension support or a failed setter is a device-creation error. Every backend's
device record names its presentation mode; intervals without an applicable
query remain absent. These values record API state, not the compositor's actual
behavior, as the [WGL extension contract](https://registry.khronos.org/OpenGL/extensions/EXT/WGL_EXT_swap_control.txt)
defines it.

The benchmark replaces `sleep_until` with a
[high-resolution Windows waitable timer](https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-createwaitabletimerexw),
requiring Windows 10 1803 or later. Absolute deadlines and immediate catch-up
remain the policy. It uses neither a spin loop nor a timer-resolution request.
Each frame now retains pacing wait and start lateness outside `whole_frame_ns`;
the analyzer verifies these fields when present and labels old captures as
legacy. The scheduler and renderer may still make a frame late. In short local
null-backend captures (120 warm-up, 300 retained frames), interval p99 changed
from 30.57 ms with the previous sleep to 17.30 ms with the timer. That is a
desktop smoke observation, not an EC2 result or a guaranteed latency bound.

Vulkan's synchronization algorithm is unchanged. Its header now describes
the frame-slot timeline wait separately from acquisition and the acquired-image
semaphore. [Vulkan's acquisition contract](https://docs.vulkan.org/spec/latest/chapters/VK_KHR_surface/wsi.html#_acquiring_presentable_images)
allows the call to return before an image is ready; the later GPU wait can
delay submission completion and therefore frame-slot reuse. The CPU samples
locate the wait but do not establish why completion was delayed. A new bounded
EC2 run or GPU trace is still needed to resolve the first-process behavior.

Verification of the changed code: all five Release builds and all 69 CTest
entries pass, including the four hardware pixel suites; the 25 offline cloud
tests pass. Each new Release benchmark completed 120 warm-up and 300 retained
frames on the local desktop (RTX 5080 for raster backends); GL reported requested
and stored interval one. Logs and JSON are under `out/validation/ec2-fixes/`.
The rebuilt Vulkan pixel suite (42 cases, 417 assertions) and a further 240
presented benchmark frames also pass with Khronos synchronization validation
enabled, with no validation errors or synchronization hazards. Only existing
small dedicated-allocation performance warnings remain.
No new EC2 instance was launched, and the original bundle and evidence remain
unchanged. The session-0, display-adapter, auto-logon and SSM dispatch fixes in
section 6 were already present before this investigation.
