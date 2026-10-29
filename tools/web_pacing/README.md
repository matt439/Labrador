# Browser pacing reduction

`analyse.mjs` reduces one saved rAF delivery-timestamp capture. It needs Node
alone and reads no clock, browser, display or network. It is analysis code;
the opt-in development-page instrument is `page.js` plus the sample's
`web/pacing_observer.cpp` and `web/pacing_state.cpp`.

Follow [the measurement method](../../docs/port/pacing-method.md) to obtain
raw rows and record the real run identity. From the repository root:

```text
node --test tools/web_pacing/*.test.mjs
node tools/web_pacing/analyse.mjs out/web-pacing/run.json > out/web-pacing/run-report.json
```

The report includes the original capture unchanged, a decision for every raw
row, exclusion counts, classifications and nearest-rank p50/p99 in
milliseconds. Keep the input and report together. A malformed input exits 2;
a valid capture with no eligible intervals has null percentiles. Neither
exit 0 nor `continuous_two_minutes` is a hardware or visible-judder pass.

## Capture format

The format is `labrador-web-pacing-v1`. Required fields are:

```json
{
  "schema": "labrador-web-pacing-v1",
  "instrument": "requestAnimationFrame",
  "status": "complete",
  "run": {
    "id": "operator-assigned unique run id",
    "source_commit": "full built source commit",
    "source_dirty": false,
    "build": "wasm-release; Emscripten version; BUILD.json or artifact hash",
    "page_url": "actual measured URL",
    "device": "actual host or phone and OS version",
    "browser": "actual browser and version",
    "display": "actual display and selected mode; VRR/power settings",
    "refresh_hz": 60,
    "refresh_source": "where the selected mode was read; nominal or measured",
    "workload": "played match; input and duration recorded separately"
  },
  "events": [],
  "samples": [
    { "frame": 0, "timestamp_ms": 100, "segment": 0, "phase": "warmup", "visible": true },
    { "frame": 1, "timestamp_ms": 117, "segment": 0, "phase": "measure", "visible": true },
    { "frame": 2, "timestamp_ms": 134, "segment": 0, "phase": "measure", "visible": true }
  ]
}
```

This is a schema example, not a measurement. `status` is `complete` or
`truncated`. Frame ids count observer callbacks, not rendered engine frames.
Each row records `visible` from the page's visibility state, even if a browser
delivers a callback while hidden. Increment `segment` on hide/resume, resize,
context loss/restore or a deliberate break. `events` retains the event type,
time and details. Additional metadata
(canvas dimensions, DPR, zoom, instrument version, thermal state, observed
judder and trace filenames) is preserved in `raw`. The method's collector
adds the page dimensions and event records automatically.

## Arithmetic and limits

An interval is the current timestamp minus the preceding row's timestamp.
The first row, warmup, warmup-to-measure boundary, either endpoint hidden,
changed segment, sequence gap and non-positive interval are excluded, in that
order; the raw row and its interval remain in the report. Exclusion reasons
are not a device verdict.
Unexpected clock reversal or sequence gaps require inspection and a new run,
even if other rows have percentiles. A hidden interval is not a slow frame.

The reference period is `1000 / run.refresh_hz`. Eligible intervals below
half of it are `short`, above 1.5 times it are `late_callback`, and both
threshold equalities are `within_one_period`. These are declared diagnostic
bins, not a perceptual threshold or proof of missed scan-outs. The nearest
positive period count is also reported; percentiles use the original
durations, never those rounded counts. The fixed 60 Hz simulation does not
supply the reference period on a high-refresh display.

For sorted values `x`, nearest rank is `x[ceil(q * N) - 1]`. No interpolation,
averaging of run percentiles or cross-display pooling occurs. A run is marked
`continuous_two_minutes` only when status is complete and at least 120,000 ms
of adjacent eligible intervals remain in one segment. This checks the saved
rows' span; it does not establish that a person played, that the declared
refresh mode stayed selected or that every callback produced a visible frame.

The ten tests use synthetic timestamps and a temporary input file. They hold
these arithmetic, conservation and CLI contracts; none simulates hardware
acceptance. The tool does not currently import browser trace formats or
reduce `Application::frame_timings()` CPU phases.

## Opt-in development-page capture

The Release development page accepts `?pacing=play`, `?pacing=topout` and
`?pacing=topout-frozen`. `page.js` is linked as pre-js only into that page,
with the observer/fixture C++ also excluded from the packaged ES module.
Without a flag it leaves Module and callbacks
unchanged. The sample's separate rAF observer reads completed
`Application::frame_timings()` snapshots outside the fixed-step loop; the
page deduplicates ids, retains gaps and buffers every observer timestamp.
No per-frame console write, software pacer, read-back or GPU finish is added.

`topout` builds all 200 visible cells with the native benchmark's pattern,
triggers an exact first burst after 300 fixed warmup steps and evolves after
that first burst frame completes. `topout-frozen` keeps the particles at
birth. The actual ParticleField must have 9,600 live particles and zero
dropped; a missing marked frame or wrong count cannot produce a complete
fixture receipt. These modes have distinct workload ids and the explicit
`benchmark_board_fixed_hints_no_touch_v1` profile: fixed landscape hint
labels, responsive camera and no touch controls, even for a portrait buffer.
Buffer orientation/geometry and control visibility are recorded separately.
The first burst is held at zero time until its target frame completes; it is
not a measured peak from ordinary portrait touch play or a timing inherited
from the Windows benchmark.

One `labrador-pacing <JSON>` line is emitted at completion; the complete
object remains in `Module.labradorPacingResult`. It uses schema
`labrador-web-engine-pacing-v1`: engine rows have raw CPU durations in
nanoseconds and their completed frame id; `top_out` identifies the marked
frame and its following interval. Save its nested `raf_capture` separately
for `analyse.mjs`. Complete operator identity through
`Module.labradorPacingSetIdentity({...})` during warmup, as the method shows;
physical refresh/device fields remain unrecorded until supplied. Geometry
comes from the first completed frame and later changes, not the initial
canvas attributes. Source/build stamps are supplied at configure time and
must be matched to the actual artifact/hash by the operator.
At observer startup the actual renderer also stamps `render_backend`,
`render_api` and `render_device_name` once. These raw strings are immutable
and cannot be replaced by operator metadata. Null, unknown and masked API
descriptions remain as reported; they imply no physical hardware kind or
display refresh rate. The browser driver requires backend `gl`, so a Release
null-render build cannot count as rendered browser acceptance.

The normal duration is two minutes after five seconds of warmup for `play`,
or three seconds after the first burst for the fixture. Query parameter
`pacing_duration_ms` accepts 100-600000 ms for focused checks; short captures
are not R5 evidence. Buffers hold 65,536 observer/engine entries for play or
8,192 for a fixture; capacity is recorded and exhaustion truncates. Export
allocations/printing happen after collection and may stall later frames.
`Module.labradorPacingStop()` exports a truncated run; reload for a fresh
fixture. Preserve all rows and exclusions, including first/reset engine
intervals of zero.

Twelve `page.test.mjs` cases run the actual pre-js against synthetic
snapshots/event targets under Node, without a browser or Application/device.
They verify the protocol, not that any fixture was rendered. The three C++
fixture cases in `LineSweeperViewTests` exercise real ParticleField emissions
headlessly. Source compilation and these tests do not replace the
Release query-flag runtime run and ordinary `LineSweeperPageTests`. One
additional Node case checks the driver's preserved initialization callback
and original-URL readiness without reading generated JS or using a context.

The driver command is:

```text
node tests/render/web/run_page.mjs --browser <chromium-path> --page out/build/wasm-release/samples/linesweeper/LineSweeperSample.html --input tools/web_pacing/page_input.mjs --timeout 40
```

Release rasterizing builds also register `LineSweeperPacingPageTests` when
configure finds Chromium. Run it with
`ctest --preset wasm-release -R '^(LineSweeperPacingPageTests|LineSweeperPageTests)$' --output-on-failure`;
[the method](../../docs/port/pacing-method.md#4-verification-boundary) records
its headless acceptance.

The driver sets real query parameters before the module starts, runs evolving
and frozen modes, and checks actual WebGL frames, completed ids, CPU phase
rows and the exact marked counts. No real display or phone evidence is
supplied by this tool.
