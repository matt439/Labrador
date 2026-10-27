# Browser pacing and phone top-out measurement method

This is SM-08's method and source preparation for R5/R6. It contains no new
display or phone result. R5 has a console procedure and an offline reducer;
the opt-in development-page engine-frame exporter and exact R6 fixture are
implemented in source, with browser runtime acceptance still open.
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

Playing a match to top-out does not fill every visible cell and cannot
establish 9,600 live particles. The preceding console observer measures only
rAF delivery. For completed-frame CPU phases use the **Release development
page**, with one of these query flags:

| Flag | Workload |
|---|---|
| `?pacing=play` | Ordinary PlayState, observed for five seconds of warmup and then two minutes |
| `?pacing=topout` | Exact first burst after 300 fixed warmup steps, followed by three seconds of evolving decay |
| `?pacing=topout-frozen` | The same first burst, held at zero elapsed time for the following capture |

Append `&pacing_id=<unique-id>` to name the run. The optional
`&pacing_duration_ms=<100..600000>` changes the retained duration after
warmup/the first burst; short values are for protocol checks, not two-minute
R5 evidence. Reload for each fresh trigger. The flag is consumed by
[page.js](../../tools/web_pacing/page.js), which is pre-js on the development
page alone, with its observer/fixture C++ compiled only into that target.
The packaged module excludes both the script and profiler C++, so adding the
query string to the bundle page does not enable a fixture or lift its phone
gate. There is no new normal-player UI.

During warmup fill the physical identity from the console, for example:

```js
Module.labradorPacingSetIdentity({
  id: "operator-assigned-run-id", device: "actual phone/host and OS",
  display: "actual display/mode/VRR", refresh_hz: 60,
  refresh_source: "where the selected mode was read; nominal or measured",
  emscripten: "actual build version", zoom: "actual zoom",
  power: "actual power mode", thermal: "initial thermal state",
  artifact_hash: "hash of the measured wasm/artifact", notes: "other active workloads"
});
```

Those example values are instructions to replace, not a run record. The
instrument supplies the configure-time source revision/dirty stamp and build
type; unknown stamps remain unknown. Match the stamp to the built artifact,
and retain its hash. At startup `app.renderer()->device_info()` stamps the
raw `render_backend`, `render_api` and `render_device_name` once; operator
fields cannot replace these. Null, unknown or masked strings remain raw
descriptions, with no physical-hardware or display inference. The draft
browser driver requires `gl` rather than accepting any Release preset.
Physical device/display/refresh fields remain explicitly
unrecorded until an operator supplies them. Geometry is sampled after the
first completed frame and on changes, with initial/final geometry and
discontinuity events retained; the pre-initialization 300x150 canvas is not
used as the measured drawing-buffer size.

The [shared fixture](../../samples/linesweeper/web/pacing_fixture.cpp)
reproduces `build_workload()` in
[linesweeper_frame_bench.cpp](../../bench/linesweeper_frame_bench.cpp): all
200 visible cells, the same colour pattern, queue and HUD values. The
[fixture state](../../samples/linesweeper/web/pacing_state.cpp) registers
BoardView, ParticleField, TopOutBanner and the two hint labels in the same
order, using the current texture/presentation constants and responsive
camera. Its explicit `benchmark_board_fixed_hints_no_touch_v1` profile keeps
the fixed landscape hint labels and omits touch controls, even with a portrait
buffer. This differs from ordinary portrait touch play; it does not measure
touch-button drawing or prove the cost of that complete played-phone UI.
Viewport/CSS/buffer geometry, buffer orientation and control visibility are
recorded separately. It marks top-out on update, advances the world
tick and calls the scene with zero elapsed time. The actual ParticleField
must report 9,600 live particles and zero dropped, or the fixture throws.

The state holds those birth positions through any catch-up updates until the
first completed burst frame. This is a zero-time birth held for that frame,
not a measured peak taken from an ordinary evolving played match. Later
updates evolve normally in `topout`;
`topout-frozen` continues with zero elapsed time. Their workload ids are
`web_full_well_top_out_first_burst_v1` and
`web_full_well_top_out_frozen_v1`. This construction matches the native
benchmark's workload; its timing does not transfer from Windows, and an
evolving burst is not the same workload as a frozen capture.

[The observer](../../samples/linesweeper/web/pacing_observer.cpp) runs in a
separate Emscripten rAF callback outside fixed-step update/draw. It reads
`Application::frame_timings()` by value, including frames with zero updates.
The page deduplicates completed ids, retains all observer callbacks and logs
gaps rather than inventing missed frames. The fixture marks the next
completed frame id before its first burst; an export that did not observe
that exact marked frame is `incomplete_fixture`, even if a later frame has
9,600 particles. First/reset intervals of zero remain raw data.

At completion one `labrador-pacing <JSON>` console line is emitted and the
same object is in `Module.labradorPacingResult`. Save that full object before
reload. `Module.labradorPacingStop()` exports an interrupted capture with
status `truncated`; it does not turn a short run into a pass. Per-frame
buffers are allocated before measurement: 8,192 entries for a fixture and
65,536 for ordinary play. `buffer_capacity_bytes` records their storage
(exported objects/JSON need additional memory after collection). Filling a
buffer truncates rather than overwriting rows. Compare an uninstrumented
visual run and record inspector/instrument overhead; the final JSON export
can itself stall the page after the retained measurement window.

The export's `samples` contain completed engine ids, update counts,
`interval_ns` and the five CPU durations as raw nanoseconds. Observer
timestamps are labelled separately from engine start intervals. `top_out`
names the exact marked snapshot, live/dropped counts, geometry, CPU total,
draw + submit and the next frame's interval. `raf_capture` is the separate
delivery-timestamp capture accepted by the offline reducer: save that nested
object to its own JSON file and complete the physical refresh identity before
reducing it. The reducer does not currently summarize engine phase rows.

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
Chrome, and `LineSweeperPageTests` must still pass (SM-08 acceptance). The
draft [page driver](../../tools/web_pacing/page_input.mjs) checks both fixture
flags, WebGL frames, exact counts, CPU phase rows and correct completed-frame
association. It has not run; source compilation and synthetic protocol tests
cannot establish its browser assertions. Browser execution remains blocked by the recorded GPU
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
| Pending browser acceptance and real run | | | | | | | | |

## 4. Verification boundary

`node --test tools/web_pacing/*.test.mjs` covers the reducer's
nearest-rank arithmetic, classification thresholds, raw-row conservation,
warmup/discontinuity/gap exclusions, continuous-span check and CLI errors on
synthetic data. Protocol tests also hold completed-id deduplication,
zero-update snapshots, exact marker association, missing/wrong-count refusal,
geometry changes, hidden callbacks, callback preservation and output receipts
without a browser. The draft driver's initialization hook is also tested
under Node without generated JS or a WebGL context.
`LineSweeperViewTests` exercises the actual ParticleField
against the shared 200-cell fixture with no device; a headless count is not
evidence that the browser rendered it. The console snippet and query-flag
instrument have not run in a browser, and no real display/phone result is
recorded here. This preparation does not close SM-08, BDP-07, R5 or R6.

Source verification on 2026-10-26: native-null `LineSweeperViewTests` passes,
including the three real-field fixture cases; all 23 offline Node cases pass.
The Release wasm objects compile for both entry variants, the fixture,
observer, state and fixture tests. The module entry has zero undefined pacing
references, while the development entry has three; the profiler sources and
pre-js are listed only on the development target. These are object/source
checks, not linked-page acceptance. A supporting read of the existing
`out/build/wasm-release/samples/linesweeper/LineSweeperSample.js` was access
denied and stopped. No generated-output link or browser run is claimed.

Release rasterizing browser builds now register `LineSweeperPacingPageTests`
for the driver above when Chromium is found. Registration was checked with
`ctest --preset wasm-release -N` (31 entries, 32 since
`LineSweeperReducedMotionPageTests`); the new test has not run here.
Run it and the ordinary `LineSweeperPageTests` once the recorded access gates
are resolved. Debug retains 31 entries with Chromium, and configurations with
no Chromium retain 19; the null-render browser configuration omits this
WebGL acceptance entry.
