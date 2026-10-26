# Browser pacing and phone top-out measurement method

This is SM-08's method preparation for R5/R6. It contains no new display or
phone result. R5 has a console procedure and an offline reducer; R6 still
needs the exact browser fixture and engine-frame export described below.
SM-08's code-instrument acceptance also requires the `wasm-release` flag run
and `LineSweeperPageTests`. Those browser checks have not run for this method:
Chrome's GPU process was denied access (`0xC0000022`) in this work session.
Neither headless cadence nor the native frame bench closes the real-device
boxes in [web-to-be-tested.md](web-to-be-tested.md).

## 1. Record the run before collecting

Use a fresh run id and save the source commit, dirty state, Release build,
Emscripten/browser versions, page URL and bundle `BUILD.json` or artifact
hash. Record the device/OS, physical display and selected refresh mode,
whether that rate is nominal or independently measured, VRR and power-saving
settings, browser zoom, viewport, canvas CSS/drawing-buffer dimensions and
DPR. Record other active workloads, power connection and thermal state;
repeat after an interruption rather than concealing it. Keep each display,
browser, orientation and power mode as a separate run.

For desktop R5 use the Release bundle page that is being judged. Use a real
60 Hz mode and a real mode above 60 Hz, with the browser wholly on that
display. A browser's callback rate does not identify the physical refresh
mode. For the phone use the Release development page, which permits touch
play; the host page's phone gate stays in place. Serving and remote inspection
are SM-14's separate owner session. Do not replace the GPU, reduce the canvas
or enable device emulation and call it that phone's result.

The browser's rAF is the cadence clock. Do not add a repeating timer or
software frame pacer, force a read-back, finish the GPU or change presentation
policy. [The ratchet record](../performance/2026-09-19-g6f-ratchet.md),
sections 3 and 5, explains why wait placement and work cost must stay distinct.
Keep normal WebGL submission and the sample's fixed 60 Hz updates.

## 2. R5: rAF delivery and observed judder

The instrument is the browser console on the page under inspection. Undock
the inspector before collecting so opening it does not resize the canvas.
After Play and the first frame, run the reviewed snippet below, filling in
the run identity first. Click the canvas during the five-second warmup, then
play for at least two uninterrupted minutes. It buffers timestamps without
per-frame console/DOM writes and prints one JSON object at the end. Save that
object as `<run-id>.json`; the output also remains in `labradorPacing` for the
console's own copy/save command. Browser console copy commands differ, so
record how the JSON was saved and verify that it parses before discarding the
page. Do not include console prompts or truncated previews in the file.

```js
(() => {
  const run = {
    id: "FILL unique run id", source_commit: "FILL built commit", source_dirty: false,
    build: "FILL wasm-release, Emscripten, BUILD.json/artifact hash",
    page_url: location.href, device: "FILL actual host and OS",
    browser: navigator.userAgent, display: "FILL actual display, mode, VRR and power",
    refresh_hz: 0, refresh_source: "FILL selected mode source, nominal/measured",
    workload: "played match; describe the inputs and result in the run record",
    viewport_css: [innerWidth, innerHeight], dpr: devicePixelRatio,
    canvas_css: null, drawing_buffer: null
  };
  if (!(run.refresh_hz > 0) || JSON.stringify(run).includes("FILL")) {
    throw new Error("Fill the physical run identity and refresh mode first.");
  }
  if (typeof stopLabradorPacing === "function") stopLabradorPacing();
  const canvas = document.querySelector("canvas");
  if (!canvas) throw new Error("Start the game before collecting.");
  const box = canvas.getBoundingClientRect();
  run.canvas_css = [box.width, box.height];
  run.drawing_buffer = [canvas.width, canvas.height];
  const capacity = 131072;
  const times = new Float64Array(capacity);
  const segments = new Uint32Array(capacity);
  const phases = new Uint8Array(capacity);
  const visible = new Uint8Array(capacity);
  const events = [];
  let count = 0, segment = 0, start = null, measured = null, request = 0, done = false;
  const targets = [[document, "visibilitychange"], [window, "resize"],
    [canvas, "webglcontextlost"], [canvas, "webglcontextrestored"]];
  function eventOccurred(event) {
    ++segment;
    events.push({ event: event.type, timestamp_ms: performance.now(),
      visibility: document.visibilityState });
  }
  function finish(status) {
    if (done) return;
    done = true;
    cancelAnimationFrame(request);
    for (const [target, event] of targets) target.removeEventListener(event, eventOccurred);
    window.labradorPacing = {
      schema: "labrador-web-pacing-v1", instrument: "requestAnimationFrame",
      status, run, events,
      samples: Array.from({ length: count }, (_, frame) => ({ frame,
        timestamp_ms: times[frame], segment: segments[frame],
        phase: phases[frame] ? "measure" : "warmup", visible: !!visible[frame] }))
    };
    console.log(JSON.stringify(window.labradorPacing));
  }
  function frame(timestamp) {
    if (start === null) start = timestamp;
    const measuring = timestamp - start >= 5000;
    if (measuring && measured === null) measured = timestamp;
    times[count] = timestamp;
    segments[count] = segment;
    visible[count] = document.visibilityState === "visible" ? 1 : 0;
    phases[count++] = measuring ? 1 : 0;
    if (measured !== null && timestamp - measured >= 120000) finish("complete");
    else if (count === capacity) finish("truncated");
    else request = requestAnimationFrame(frame);
  }
  for (const [target, event] of targets) target.addEventListener(event, eventOccurred);
  window.stopLabradorPacing = () => finish("truncated");
  request = requestAnimationFrame(frame);
})()
```

This observer measures rAF delivery timestamps, not the engine's completion
time or display scan-out. Its buffering has overhead, so record that the
instrument was enabled and compare an otherwise identical uninstrumented
visual play-through. No browser execution of this snippet is claimed here.
It is a manually started console procedure; no development-page query flag
has been implemented by this document.

Run the [offline reducer](../../tools/web_pacing/README.md), retaining its
report and the input. It uses nearest-rank p50/p99, retains every excluded
row and bins callbacks against the recorded display period. Warmup, the
measurement boundary, hide/resume, resize and context transitions do not enter
the same interval population. A callback with either timestamp's page hidden
is also excluded, including callbacks delivered while the page stays hidden.
If a run lacks a continuous two minutes, redo
it. Inspect sequence gaps or non-positive timestamps. Do not trim late
callbacks because the play looked smooth, combine different refresh modes
or average repeated runs' p99 values.

For this procedure **observed judder** means a person sees an irregular
hold/jump in motion during otherwise comparable falling/particle motion,
beyond the regular fixed-step motion. Record approximate time, action,
whether it repeats and whether the long callback is nearby; distinguish a
pause, hard drop, top-out transition or intentional discrete rule step.
`late_callback` (above 1.5 declared display periods) is a diagnostic marker,
not proof of visible judder. A steady rAF stream can still draw unevenly when
the fixed step supplies zero or several updates. Absence of a visible defect
is an observation for that person/run, not a universal smoothness guarantee.
Save a native browser performance trace for a suspect window when available,
with profiler settings and overhead recorded; do not silently substitute its
FPS chart for the saved interval samples.

Repeat three runs per browser/display mode and retain them individually.
Put measured results in [web.md](web.md)'s R5 record only after the real
display session; report both modes even if one finds a defect.

| Run id / raw JSON / report / optional trace | Source/build | Browser/device/display and refresh source | Buffer/CSS/DPR/zoom/power | Eligible count / contiguous ms | p50 / p99 / max ms | Late callback count | Observed judder and timestamps | Outcome/limits |
|---|---|---|---|---|---|---|---|---|
| Pending real run | | | | | | | | |

## 3. R6: prerequisites for an exact 9,600-particle browser frame

**This instrument and fixture are not implemented yet.** Playing a match to
top-out does not fill every visible cell, and cannot establish 9,600 live
particles. Neither the preceding rAF observer nor the Windows-only
`LineSweeperFrameBench` times that browser CPU frame. Keep R6 and SM-08's
remaining instrument acceptance open until the following are concrete and
checked. The existing bench's `build_workload()` in
[linesweeper_frame_bench.cpp](../../bench/linesweeper_frame_bench.cpp)
is the construction reference, not a transferable timing result.

The browser fixture must use the current sample's BoardView, ParticleField
and TopOutBanner, their ordering, texture, camera and the built presentation
constants. Fill all 200 visible cells with the bench's pattern. Let the field
observe that pre-top-out world, then mark it topped out and advance its tick.
At the first rendered burst, assert exactly 9,600 live particles and zero
dropped; save those counts and the constructed world/source identity. A
different count is a refused fixture, not an approximate pass. Record actual
buffer dimensions and touch-control visibility. A frozen zero-time bench
frame and an evolving first burst are separate workloads and must have
separate identifiers; the fixture must say which one it draws.

Use the existing public `Application::frame_timings()` to export the actual
completed frame's id, update count, interval and five CPU phase durations.
Read it outside the fixed-step update path so a frame with zero updates on a
high-refresh display is not silently lost. Deduplicate by frame id and
report any skipped ids. Associate the top-out marker and live/dropped counts
with the frame that was drawn, accounting for the snapshot being the last
completed frame. Preserve first/reset intervals of zero as discontinuities.
Buffer output and log per-frame rows together after capture, rather than
printing in the measured loop. The flag must be opt-in on the development
page; ordinary play and the bundle's phone gate remain ordinary play.

Report the burst frame's update, begin, draw, submit and end times separately,
plus draw + submit, their sum, its start interval and the following start
interval. A long following interval can reveal delayed scheduling; CPU
phases cannot establish GPU time, scan-out or which component caused it.
Keep browser-clock coarsening (including zero-valued short phases) visible.
Capture the first evolving burst and its decay with normal updates; if a
frozen sustained 9,600-particle capture is also used, retain it separately.
Use at least five fresh triggers per device/orientation, not five samples
from one frozen frame. Retain warmup and every interrupted run with reasons.

Before taking phone results, the opt-in `wasm-release` development page must
log those per-frame intervals and the marked top-out CPU time in headless
Chrome, and `LineSweeperPageTests` must still pass (SM-08 acceptance). Add
headless assertions for the exact live/dropped counts and correct frame
association; source compilation or a synthetic reducer test cannot establish
that. Browser execution currently remains blocked by the recorded GPU
access denial. Real iPhone Safari data, remote-inspector access and a real
Android run are further independent gates; none is implied by a later
headless pass.

For the eventual real-phone session, use the same run identity and observed
judder definition as R5, document portrait/landscape, thermal/power state,
first-burst counts and profiler overhead. A visual stall or a large callback
interval is evidence to inspect, not a substituted CPU duration. Store raw
engine-frame rows, rAF rows and any trace together. Phone results go in
[web-to-be-tested.md](web-to-be-tested.md)'s R6 record and retain the platform
limits required by SM-13/SM-44.

| Run/trigger id and raw files | Source/fixture id and evolving/frozen mode | Phone/OS/browser/orientation/power | Buffer/CSS/DPR | Frame id / updates / live / dropped | Update / begin / draw / submit / end / draw+submit / sum ms | This / next interval ms | Observed judder / trace | Outcome/limits |
|---|---|---|---|---|---|---|---|---|
| Pending fixture and real run | | | | | | | | |

## 4. Verification boundary

`node --test tools/web_pacing/analyse.test.mjs` covers the reducer's
nearest-rank arithmetic, classification thresholds, raw-row conservation,
warmup/discontinuity/gap exclusions, continuous-span check and CLI errors on
synthetic data. These are offline analysis tests. The console snippet has
not been executed in a browser, the query-flag engine instrument and exact
browser fixture remain unimplemented, and no real display/phone result is
recorded here. This preparation does not close SM-08, BDP-07, R5 or R6.
