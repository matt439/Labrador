# Native first-frame reveal qualification, 2026-11-17

Eight visible runs qualify OCA-87's native windowed/full-screen path at
`5cff44b9c6da777479229832abb40bfc0fc6e65c`. All four native rasterisers draw the
minimal sample on the NVIDIA GeForce RTX 5080. In each run, initialization,
manifest loading and the first draw are hidden; a subsequent completed-frame
observation is visible and in the foreground, with unchanged client and buffer
dimensions. Every retained visible client image and engine redraw was inspected:
the greeting and complete control hint appear on black, with no white class
brush in the recorded image. All eight runs returned from their application
loop after native close, and no capture window or process remains.

The captures ran at 00:01 to 00:03 AEST on 17 November 2026. JSON timestamps are
UTC. The window reports 96 DPI. Windowed clients are 1280 by 720; full-screen
clients are borderless and 2560 by 1440. No display or browser setting changed.

## Recorded pictures and devices

| Preset | Mode | Visible client | Engine redraw | First completed frame observed |
|---|---|---|---|---:|
| `x64-debug` | windowed | [image](x64-debug/windowed/client-visible.png) | [image](x64-debug/windowed/client-redraw.png) | 1 |
| `x64-debug` | full screen | [image](x64-debug/fullscreen/client-visible.png) | [image](x64-debug/fullscreen/client-redraw.png) | 2 |
| `x64-debug-d3d12` | windowed | [image](x64-debug-d3d12/windowed/client-visible.png) | [image](x64-debug-d3d12/windowed/client-redraw.png) | 1 |
| `x64-debug-d3d12` | full screen | [image](x64-debug-d3d12/fullscreen/client-visible.png) | [image](x64-debug-d3d12/fullscreen/client-redraw.png) | 1 |
| `x64-debug-gl` | windowed | [image](x64-debug-gl/windowed/client-visible.png) | [image](x64-debug-gl/windowed/client-redraw.png) | 1 |
| `x64-debug-gl` | full screen | [image](x64-debug-gl/fullscreen/client-visible.png) | [image](x64-debug-gl/fullscreen/client-redraw.png) | 1 |
| `x64-debug-vulkan` | windowed | [image](x64-debug-vulkan/windowed/client-visible.png) | [image](x64-debug-vulkan/windowed/client-redraw.png) | 1 |
| `x64-debug-vulkan` | full screen | [image](x64-debug-vulkan/fullscreen/client-visible.png) | [image](x64-debug-vulkan/fullscreen/client-redraw.png) | 2 |

Direct3D reports Direct3D 11 and Direct3D 12, respectively. GL reports
`OpenGL 3.3.0 NVIDIA 616.92` and
`NVIDIA Corporation / NVIDIA GeForce RTX 5080/PCIe/SSE2`; its numeric IDs are
zero because that seam has no portable numeric answer. Vulkan reports
`Vulkan 1.4.351`. Direct3D and Vulkan report vendor 4318 and device 11266.
Every device kind is hardware. [summary.json](summary.json) retains each API's
unmodified device description, observation times, image hashes and source
contract hashes.

## What was run

The archived [entry point](tools/capture_main.cpp) wraps the unchanged minimal
sample `HelloState` and `ConfirmState`. It logs its own window's visibility,
foreground state, client/buffer sizes and first draw, then keeps the application
alive for inspection. Its native engine, math and rules libraries are linked
unchanged from the validated root build; it does not rebuild those libraries.
Explicit `CAPTURE_FULLSCREEN=0/1` compile definitions select the two launch
modes. Each preset's `build-receipt.json` retains every compile/link command
and the SHA-256 of its sources, linked libraries and both executables.

The Computer Use skill's documented `@oai/sky` API launched each executable,
selected its single returned window, activated only that target, captured its
bounded window state, and closed it with Alt+F4. The inspection records are
each case's `snapshot.json`. The measured client origin and size crop the
bounded window snapshot to `client-visible.png`; no desktop, title bar or other
application is retained. An initial occluded snapshot and an ambiguous launch
preparation trial were discarded before the eight recorded cases.

Each `events.jsonl` contains initialization, first-draw begin/end, the first
completed frame observed from a later fixed update, activation/deactivation and
return from the loop. The first-draw end is hidden in all eight logs. The first
completed-frame observation is visible and foreground in every case; the client
and buffer dimensions equal those of the hidden first draw. The source's
`end_frame` then private `frame_completed` ordering supplies the reveal boundary;
the later fixed-update observation is not labelled as the exact first present.

To recreate the auxiliary executable, copy the three archived files in `tools/`
to an ignored work folder. In a Visual Studio x64 developer environment, run
`python build_capture.py x64-debug --root C:\Projects\Labrador` after building
that preset's `MinimalSample`; repeat with the other three preset names. The
script derives the preset's link libraries from its generated Ninja file and
writes only beside the copied tools. The recorded commands are the exact
commands of this run; the source-frame and CMake/public-contract hashes are in
`summary.json`. Compiled working-file byte hashes and canonical Git-blob hashes
are recorded separately, so line-ending conversion does not change their meaning.
Text-artifact hashes normalize CRLF to LF, matching canonical Git text; PNG,
library and executable hashes describe their unchanged bytes.

## Limits

- The visible images are point-in-time window captures several seconds after
  launch, after target activation. They are not a high-frequency compositor
  trace and cannot exclude an interval shorter than their sampling coverage.
  No scan-out, display pacing or universal absence of a transient is claimed.
- `client-redraw.png` is an engine-owned framebuffer redraw captured from the
  first later update that observes completed frames. It is submitted and read
  without presenting, as `Renderer::read_back_buffer` requires. It shows what
  that redraw drew, not a photograph of the display or the exact presented frame.
  The visible image can also contain the system pointer; the redraw cannot.
- Instrumentation and readback add work. Logged elapsed times are provenance,
  not startup or frame-performance measurements. The host was shared with
  other validation work; the separate IPO frame measurements had already ended.
- This qualifies the recorded native Debug desktop paths and device. It does
  not qualify another GPU, Release configuration, physical display behaviour,
  audible output or a client game's launch. Native-null tests separately pin
  failure, hidden startup, launcher show commands and maximized restore geometry.
- The five native Debug CTest selections pass 112/112 at the source frame.
  Remaining browser/app-matrix and final website verification are tracked in
  OCA-87; this visual record does not convert their results into passes.
