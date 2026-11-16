# Cross-origin bundle regression, 2026-11-17

`SM-93` retains the bounded part of the November 13 iframe rehearsal as
`LineSweeperIframePageTests`. Configure registers it when a Chromium is found
in a rasterizing browser build. It is absent from browser-null builds and is
listed in `ctest_inventory.json` and the verification planner's exclusions.

## The retained check

`tests/render/web/iframe_input.mjs` drives the packaged
`web-dist/linesweeper/index.html` through `run_page.mjs`. The game server binds
`127.0.0.1` and a second disposable server serves the embedding page as
`localhost`, on independently allocated ports. Their sites and origins differ.
The host has an ordinary focusable button and one iframe. Both servers and the
browser profile belong to the run; no external site or existing browser profile
is used.

The two iframe cases have no `allow` attribute and
`allow="fullscreen; gamepad; autoplay"`. The runner pauses each out-of-process
frame until its console, browser-log, exception and crash monitoring and the
existing factory-observation wrapper are installed. That wrapper calls the real
packaged factory; its query-marked import fetches the original loader and wasm.
An unrelated warning, error or exception in either document fails the run.

The assertions cover:

- No game instance before Play. A genuine top-page mouse click starts the
  module, which draws frames. The host's focused element is its iframe, and
  the child's is its canvas.
- Host-focused key input reaches the host and never the child. Clicking the
  canvas returns input to the game. The actual blur pause is proved by walking
  directly from that pause menu to QUIT, without sending another pause key.
- QUIT reaches `onQuit`, exposes the exact end status and Play again, and replay
  creates a fresh canvas and initialized game that also quits.
- Without fullscreen delegation, `fullscreenEnabled` is false and the example
  page hides its Full screen button. With delegation, a real button click makes
  the stage the child's fullscreen element and the iframe the host's.
  Programmatic `document.exitFullscreen()` is followed in both documents before
  further input. This is not evidence about the browser's Escape handling.
- The host cannot read the child's document and a host-origin localStorage
  marker is absent in the game origin. This checks origin separation, not a
  general storage-partition or persistence contract.
- An intentionally refused loader request gets the page's exact connection
  advice and Try again, without constructing a game. Only that URL's exact
  404/import diagnostics are excused; unrelated complaints still fail.

The fixture uses a stated 1280 by 1000 desktop viewport and real input routing.
It does not synthesize DOM focus/visibility events, emulate page focus, enable
Keyboard Lock or change browser permissions. The fullscreen exit waits for
both documents and subsequent animation frames before another hit test.

## Local qualification

The initial retained run used the clean Debug bundle from
`ad711c050cea427a1ec64a1d084483628c7b483c`, Emscripten 6.0.12. Its `BUILD.json`
and all six packaged files were copied unchanged into the isolated test output.
The bundle's SHA-256 entries were checked against those copied files.
Node was 24.19.0. Headless Chrome was 157.0.8103.78 (DevTools 1.3), and both
iframe cases reported ANGLE on the NVIDIA GeForce RTX 5080 through Direct3D11.
The frame was a separate `iframe` DevTools target, not same-origin DOM access.
The same iframe sequence also passed with explicit ANGLE/SwiftShader browser
arguments; its renderer reported the SwiftShader Subzero Vulkan device. This
software run exercises the automated route without claiming physical-device
acceptance.

The existing bundle page, new iframe page and complete browser-protocol entry
passed together, 3/3, under their existing bounds. Protocol coverage was 154/154;
planner/CI-gap fixtures were 65/65. Fresh Debug GL, Release GL and Release-null
configurations matched their inventories exactly: 46, 47 and 31 entries at this
base, with the iframe entry absent under null. The website gate passed all 201
tests and built 186 pages with valid internal links. The first twelve lint
phases passed; the full Python discovery is reserved for combined-root
qualification to avoid parallel Git-fixture contention, and the story remains
open for that final gate.

Raw local receipts are retained under the worktree's `out/sm93/`: final browser
output, a negative control, runner fixtures, registration inventories and the
website/lint results. The negative control adds an unrelated child console
error and a cleanup failure: the runner exits 1 and retains both diagnostics.
Protocol fixtures also hold child exceptions, browser-log failures and crashes
to the same failure path as the top document.

Early harness runs caught coordinates outside the browser viewport after
scrolling and a click before fullscreen-exit layout settled. The fixture now
uses actual element geometry, checks the host focus explicitly, states its
viewport and waits for the exit in both documents. No game or engine behavior,
timeout or error policy was relaxed to make those runs pass.

## Boundaries

The [November 13 rehearsal](../review/2026-11-10/2026-11-13-cross-origin-iframe.md)
remains unchanged. This regression qualifies the local headless Chromium route,
not every Chromium version or external host. Firefox and Safari iframe runs,
headed fullscreen and Escape, HTTPS hosting, physical pads, phones and audible
audio are unrun here. LineSweeper has no audio; the delegated gamepad/autoplay
tokens do not prove input or listening acceptance. itch.io setup and a guide,
external-host publication and precompressed `.br` distribution remain deferred.
