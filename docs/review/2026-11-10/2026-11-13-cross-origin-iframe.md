# The bundle in a cross-origin iframe, driven 2026-11-13

The first step of `REV#72`: the LineSweeper `web-dist` bundle driven once
inside a cross-origin iframe, to record what fails. Nothing in the engine or
the page failed. The differences from top level are the browser's: keys reach
the frame only after a click into it, full screen needs the embedder's
`allow`, and storage is partitioned. Whether to add an iframe page test, an
itch.io guide or `.br` output stays the owner's call.

## What was run

- The bundle: `out/build/wasm-release/web-dist/linesweeper/` at `1dc8533c`,
  `BUILD.json` not dirty, Emscripten 6.0.12, unmodified. For the audio unlock,
  the audio sample's development page `samples/audio/AudioSample.html` from
  the same build, because LineSweeper has no audio.
- Headless Chrome 157.0.8103.78 (`--headless=new`, a fresh profile, default
  autoplay policy), driven over the DevTools protocol from Node 24.14.0. The
  frames drew on the RTX 5080 (`ANGLE (NVIDIA, NVIDIA GeForce RTX 5080 ...
  Direct3D11 ...)`, read inside the frame), not SwiftShader.
- The game origin was `http://127.0.0.1:9545` and the embedding page
  `http://localhost:9546`, which are different sites, each served by its own
  Node `http` server. The embedding page holds a button and
  `<iframe src="http://127.0.0.1:9545/bundle/index.html" width=1000 height=760>`,
  once with no `allow` and once with `allow="fullscreen; gamepad; autoplay"`.
  The same steps ran at top level as the baseline. Chrome put each frame in
  its own process: the frame arrived as a separate `iframe` target through
  `Target.setAutoAttach`, and probes and hooks ran in that session.
- Input went where a person's goes. `Input.dispatchMouseEvent` and
  `Input.dispatchKeyEvent` were sent to the top page at viewport coordinates,
  and Chrome routed them to the focused frame. Each key level was held for
  130 ms. A capture `keydown` listener in each document counted what arrived
  where, and a `clear` hook counted frames drawn.
- Three runs. Two covered the bundle and the audio sample, one with
  `Emulation.setFocusEmulationEnabled` and one without it; a third, with focus
  emulation, covered only the bundle. The bundle table below therefore rests
  on three runs and the audio table on two. Every outcome matched across the
  runs that covered it. The script was scratch and is not committed. The
  commands above are its substance.

## What happened

| Check | Top level | Frame, no `allow` | Frame, `allow` list |
|---|---|---|---|
| Play clicked, game draws | yes | yes | yes |
| A key before any click reaches | the page | the embedding page, not the frame | the embedding page, not the frame |
| Focus after clicking Play | canvas | canvas; the embedder's `activeElement` is the iframe | same |
| Escape, Down x3, Enter reaches QUIT and "Thanks for playing." | yes | yes | yes |
| Click outside the game, then the QUIT walk | the keys are heard by the document, not the canvas; the game paused itself | 0 keys reach the frame, all 5 go to the embedding page; the game paused itself | same |
| Click the canvas, then Down x3, Enter | QUIT | QUIT | QUIT |
| `document.fullscreenEnabled` in the game document | true | false | true |
| The page's Full screen button | shown; the stage went full screen | hidden by the page, no error | shown; the frame's `fullscreenElement` was the stage and the embedder's the iframe, 0 `fullscreenerror` |
| `featurePolicy` fullscreen / gamepad / autoplay | true / true / true | false / true / false | true / true / true |
| `navigator.getGamepads()` | 8 empty slots | 8 empty slots, no throw | 8 empty slots, no throw |
| `localStorage` and `indexedDB` | work | work; the top-level origin's value is not visible | work; sees the value the previous frame wrote |
| `document.hasStorageAccess()` | true | true | true |
| Console errors, warnings, exceptions | the test server's missing `favicon.ico` only | none | none |

The audio sample in the same three placements:

| Check | Top level | Frame, no `allow` | Frame, `allow` list |
|---|---|---|---|
| An `AudioContext` made with no gesture | `suspended` | `suspended` | `suspended` |
| Focus at load (the page calls `canvas.focus()`) | canvas | canvas; the embedder's `activeElement` is the iframe | same |
| The engine's context after the first key, `1`, in the frame | `running` | `running` | `running` |
| Buffer sources started by two presses of `1` | 2 | 2 | 2 |
| Escape quits | yes | yes | yes |

## Reading it

- The engine needs nothing changed to run in a cross-origin frame in Chrome.
  The bundle page's own `document.fullscreenEnabled` check hides the Full
  screen button where the embedder gives no `fullscreen`,
  so a host without it gets a game that plays in the page. The engine's
  `options.fullscreen` path in `engine/app/web/window.cpp` was not exercised,
  because LineSweeper does not ask for full screen at start.
- The bundle's keyboard needs a click into the frame first, because its page
  focuses nothing at load. The bundle's Play button is that click. A frame whose game starts without one, as the development shells
  do, depends on whether the browser lets the frame's `canvas.focus()` take
  focus. Headless Chrome did, with or without focus emulation, but that is
  not a headed or a Safari/Firefox result.
- When focus leaves the frame, keys go to the embedding page and LineSweeper
  pauses, the same as clicking outside the canvas at top level. A click on the
  canvas gives the keyboard back.
- `allow="autoplay"` was not needed. A gesture inside the frame itself let the
  engine's context run under Chrome's default policy, which was in force
  (a context made with no gesture stayed `suspended`). Nobody listened.
- Chrome 157 reports `gamepad` as allowed without delegation. No pad was
  connected or pressed, so pad input inside a frame is unchecked.
- Storage is partitioned by the top-level site. The engine and LineSweeper
  store nothing in the browser, so this changes nothing today; a game that
  saves through `localStorage` would see a separate store when embedded.
  `document.hasStorageAccess()` answering true everywhere does not contradict
  this: it reports access to unpartitioned cookies, not whether
  `localStorage` and `indexedDB` are partitioned.

## What this is not

It is headless Chrome on a desktop GPU, on two local origins over plain http
on loopback, which Chrome treats as secure contexts; a real host's HTTPS
frame on a public domain was not used. It is not
itch.io: its game host, its iframe attributes and its `.br` serving were not
used, and no upload was made, which needs the owner's account. Firefox, Safari,
a headed window's real full screen and Escape, a physical pad, a phone and
listening are all unchecked. Other agents ran on the machine at the same time;
no timing is claimed.
