# Browser WAV playback qualification

From the repository root, after building the actual Web Audio sample with
OCA-105's `ping_wav` asset:

```text
node tests/render/web/run_page.mjs --browser "C:/Program Files/Google/Chrome/Application/chrome.exe" --page out/build/wasm-debug/samples/audio/AudioSample.html --input tests/audio/webaudio/playback_input.mjs --browser-warnings fail --timeout 60
```

Use `--input` alone; do not add `--quit-keys`. This driver exports its own
sample-ready condition and installs observation hooks before navigation.
It forwards native AudioNode connections and AudioBufferSourceNode start/stop
calls without replacing the browser's audio implementation. It sends genuine
A, S and Escape keys through DevTools and requires the actual quit callback.

The `audio playback evidence` JSON line identifies the one-wave WAV bank's
AudioBuffer by object identity, separately from the XWB bank's equal-sounding
ping. It compares all 3,528 frames with the checked source WAV and records
the reference PCM SHA-256, rate/channels, native source start, connection
through gain and stereo pan to the real destination, gain/pan/playback rate,
running context, trusted natural-ended event without stop, observed context
elapsed time and quit. Unexpected page/browser complaints fail the run.

This is real backend/graph/processing evidence. It does not establish output
from a physical speaker, loudness, listening quality or endpoint recovery.
Elapsed context time is an observation, not an audio latency measurement.
No AnalyserNode result is substituted for listening.

The offline observer/control fixtures run with
`node --test tests/audio/webaudio/playback_input.test.mjs` and through the
existing Python tooling suite. They exercise the observation and rejection
logic using fixtures and supply no browser or playback acceptance evidence.

## Local result, 2026-11-17

The command above passed against the newly built Debug sample with Emscripten
6.0.12 and Chrome 157.0.8103.78 on Windows. The report retained at
`out/wav-qualification/browser-playback-qualified.log` recorded one WAV
bank/source, one channel at 22,050 Hz and 3,528 frames. Every sample matched the original
PCM, whose SHA-256 was
`48db9e5cb4cce6432db1f8c9d3f5c442b48a89e8c41e47231aebcb0de65c8658`.
The native source reached the real destination through gain
`0.4000000059604645`, pan 0 and playback rate 1; the context was running at
48,000 Hz. Its trusted natural end arrived with no stop call and
0.17066666666666666 seconds of observed context advancement, and the page
called `Module.onQuit` with no complaints. This retains the evidence boundary
above: output from speakers and listening were not observed.
