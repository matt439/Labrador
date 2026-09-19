# The first EC2 reference run — `g6f-reference-003`, 2026-09-19

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
exactly every second frame — waited on the display path, and the other four
repetitions of the same backend show nothing of the kind. §3 gives the
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
| When | host captured 05:03:39Z; first frame 05:03:49Z; last 05:34:50Z; `success.json` 05:35:08Z (13:03–13:35 AEST) |
| Evidence | `s3://labrador-performance-077207386906-apse2/labrador-performance/runs/g6f-reference-003/` — 64 files under `output/`, hashed in `success.json`; local copy `out/cloud/g6f-reference-003/`, analysis `out/cloud/g6f-reference-003-analysis.json` (`complete: true`, 18,000 samples per backend) |
| Cost | about US$0.60 of compute for the run itself; the day's three builders, three verifications and two failed runs (§6) came to roughly US$6 |

## 2. What each column measures

The benchmark's loop is in
[`bench/linesweeper_frame_bench.cpp`](../../bench/linesweeper_frame_bench.cpp);
every number below is `std::chrono::steady_clock` wall time on the CPU,
including any wait the call performs, and nothing is a GPU timestamp.

- **update** — `Scene::update` and `end_tick`: the game's own work, the
  particle field included.
- **begin** — `Renderer::begin_frame`. On Vulkan this acquires the swap
  chain image and is where that backend waits; on D3D12 it is the fence wait
  for the frame's allocator; on D3D11 and GL it does nearly nothing.
- **record + submit** — `Scene::draw` and `Renderer::submit`: the draw
  walk and the command recording, which is the cost this workload exists to
  measure.
- **present** — `Renderer::end_frame`: `Present` with sync interval one on
  both Direct3D backends, `SwapBuffers` under the driver's default on GL,
  `vkQueuePresentKHR` in FIFO on Vulkan.
- **whole frame** — frame start to the end of present.
- **interval** — this frame's start minus the last one's: what the
  software pacer actually delivered. It asked for 16.67 ms and delivered
  **17.00 ms at the median on this host, on every backend** (58.8 Hz), with
  a p99 of 18.0 ms. The bench says of itself that this is a software
  deadline and not a claim about scan-out; that the four backends share the
  same realised interval is what keeps them comparable, and the 0.33 ms is a
  fact about `sleep_until` on this Windows Server, not about any of them.

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
by its own scheduled task, ninety seconds apart.

| rep | d3d11 | d3d12 | gl | vulkan |
|---|---|---|---|---|
| 001 | **16.98 / 17.12** — 100 % over 10 ms, all in present (14.10 / 14.97) | 1.80 / 2.49 | 2.05 / 2.54 | **1.45 / 32.85** — 50 % over 10 ms, all in begin (0.03 / 30.76) |
| 002 | 2.31 / 3.05 | 1.66 / 2.43 | 1.68 / 2.52 | 1.62 / 2.48 |
| 003 | 2.46 / 3.20 | 1.72 / 2.48 | 3.06 / 3.15 — present 0.98 / 1.66 | 1.64 / 2.54 |
| 004 | 2.52 / 3.11 | 1.66 / 2.53 | 5.11 / 6.14 — present 3.51 / 4.65 | 1.59 / 2.45 |
| 005 | 2.42 / 3.17 | 1.67 / 2.42 | 1.84 / 2.49 | 1.54 / 2.46 |

Update, begin and record + submit are stable across all twenty repetitions
— update p99 never above 0.60 ms, record + submit p50 within 0.15 ms of its
backend's pooled value — so the table is a table of presentation behaviour
and nothing else moved.

**What the four backends cost when nothing waits.** Taking every frame
under 10 ms: d3d11 2.43 / 3.14, d3d12 1.68 / 2.48, gl 2.11 / 6.12, vulkan
1.55 / 2.48 (p50 / p99 ms). A short local smoke of the same workload on the
desktop this engine is written on, an RTX 5080, puts the median near 1.5 ms;
a quarter of an L4 behind four EPYC cores is within a millisecond of that
at the median, and D3D11 is the slowest of the four to record and submit by
roughly 0.8 ms, which reads as the immediate context's per-draw state cost.
Against a 16.67 ms budget the p99 leaves a factor of five or more on three
backends and two and a half on GL, whose swap is the reason (below).

**Two repetitions differ in kind, not degree, and both are a backend's
first.** d3d11-001 spent 14.1 ms of every frame in `Present`: the whole
repetition ran at the display's cadence, and its interval column (16.98 /
17.12) says the pacer never had to sleep. Its next four repetitions never
waited at all. vulkan-001 blocked in the acquire for ~30.7 ms — two refresh
periods — on exactly every second frame, and the frame after each stall
ran immediately (interval p50 1.45 ms) because the pacer was behind; its
next four repetitions have a p99 of 2.5 ms. d3d12's and gl's first
repetitions show nothing of the sort, so "first process after boot" is not
the explanation on its own — d3d11-001 was the first benchmark process on
the machine, but vulkan-001 started twenty-three minutes and fifteen
processes later. What the two share is being the first process of their
API on the host. Whether the presentation engine, DWM or the vGPU's display
path holds a different mode for a first swap chain, and releases it after
the first process exits, is a question this run poses and cannot answer;
§5 says what would.

**GL's swap is the one that moves between repetitions.** No frame over
10 ms, but `SwapBuffers` cost 0.02 ms in two repetitions, ~1 ms in one and
~3.5 ms in another, which is the difference between a swap that returns
and one that waits part of a refresh. The bench inherits the driver's
default swap interval on GL rather than setting one, and that is the first
thing to fix before reading this column again.

**D3D12 is clean in all five**, p99 2.42–2.53 ms, present 0.10–0.14 ms:
the backend that owns its fence never waited on the display path here.

## 5. What this run asks, and what would answer it

These are questions rather than findings, in the sense the review folders
use: nothing below has been verified beyond what the tables above show.

1. **Is the first-process presentation mode repeatable?** Run the lane
   again on the same image. If d3d11-001 and vulkan-001 misbehave the same
   way and 002–005 do not, the cause is on the host and the analysis
   should either discard a backend's first repetition or the worker should
   run a throwaway process per backend before the retained ones. If it does
   not recur, it was a state of that boot.
2. **What does Vulkan's acquire wait on?** A 30 ms stall on every second
   frame is a two-image swap chain being handed back at half rate. The
   review of that backend recorded that nothing in the tree had run two
   frames in flight; this is the first run that has, on a virtual display,
   and `engine/render/vulkan/device_resources.h` should say what it expects
   from `vkAcquireNextImageKHR` under FIFO on a presentation engine that
   pauses.
3. **Set GL's swap interval.** The GL backend should ask for an interval
   rather than inherit one, and the bench should record what it got.
4. **What is the 0.33 ms?** `sleep_until` overshoots by a third of a
   millisecond on this host, consistently; a `timeBeginPeriod` request or
   a spin for the last millisecond would pin the pacer at 16.67 ms, at the
   cost of a core. Decide which the benchmark wants before comparing across
   hosts.
5. **This host is a virtual display on a shared GPU.** 59 Hz rather than
   60, a quarter of an L4 time-sliced with three other tenants, and a
   frame-rate limiter somewhere in the vGPU stack are all plausible actors
   in §4 and none of them exist on a player's machine. The run is worth
   more as a regression reference for the engine's own CPU cost than as a
   statement about presentation.

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
