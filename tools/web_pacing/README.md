# Browser pacing reduction

`analyse.mjs` reduces one saved rAF delivery-timestamp capture. It needs Node
alone and reads no clock, browser, display or network. It is analysis code,
not the missing engine-frame/9,600-particle browser instrument.

Follow [the measurement method](../../docs/port/pacing-method.md) to obtain
raw rows and record the real run identity. From the repository root:

```text
node --test tools/web_pacing/analyse.test.mjs
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
