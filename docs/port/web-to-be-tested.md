# The web port — what is still to be tested

Everything in `web.md`'s milestones that can be written and checked on this
desktop has been, in headless Chrome (and headless Firefox for LineSweeper's
drawing), under every preset, with the pixel tests on the RTX 5080, WARP and
SwiftShader. What follows needs a person, hardware this machine does not have,
a browser it does not run, or a push. Tick a box in the commit that checks it,
and amend `web.md` §9 with the result.

## Needs a push (CI)

- [x] The wasm job is green on the GitHub runner: the pixel pages on its
      SwiftShader, `RenderPixelTestsWithoutS3tc`, `WebWindowTests` and the six
      `*PageTests`. Run 38130728123, PR #1, every step passed, with the five
      there were then; run 38339603954, on master at `fbb753d`, passed all six,
      `ComponentsPageTests` among them.
- [ ] The null-backends step runs even when a page step fails, and the job
      still goes red. Not yet seen: nothing has failed since the change.
- [x] The x64 jobs' `RenderPixelTests` pass against the 17 regenerated text
      goldens and the three new frames on the runner's Direct3D adapter
      (`x64-release` and `x64-debug-d3d12`, same run).
- [x] The wasm job uploads the `linesweeper-web` artifact and its `BUILD.json`
      names the commit CI built, clean. On a pull request that is GitHub's
      merge commit for it rather than the branch head, since that is what the
      job checks out.
- [ ] Optional: one run with `ALLOWED_CHANNEL_DRIFT` at 0 to measure the
      runner's adapter, so the allowance can come down from 8.

## Needs Safari (M3's done-when)

- [ ] LineSweeper, local_multiplayer, minimal, collision, the audio sample and
      the components gallery in desktop Safari: drawing, keys, quit, context loss and restore. The 2013
      MacBook Air is the candidate; note its Safari ceiling before moving off
      legacy wasm exceptions, which needs Safari 18.4.
- [ ] Safari masks `RENDERER` as "WebKit WebGL", as assumed, so the device
      strings are unmasked through the extension.

## Needs real displays (R5)

- [ ] Frame pacing of the fixed 60 Hz step under `requestAnimationFrame` on a
      60 Hz display and on a high-refresh one: judder or none.
      [R5 method and results template](pacing-method.md#2-r5-raf-delivery-and-observed-judder):
      console rAF timestamps, raw JSON and nearest-rank p50/p99; separately
      record visible irregular holds/jumps during comparable motion. Callback
      bins alone are not visible-judder or scan-out evidence. The console
      procedure still needs its first browser run; no display result is added.
- [ ] The mattharding.dev stage at 1x, 1.25x, 1.5x and 2x and with browser
      zoom: the HUD text is readable when the game is drawn smaller than
      1280x720.

## Needs a gamepad (M4)

- [ ] A standard-mapping pad plays LineSweeper in Chrome, Firefox and Safari.
- [ ] Unplug and replug with more than four pads seen, to check slot
      compaction against real browser indices.
- [ ] A tab switch or minimise suspends and resumes the pad (visibility was
      faked headlessly).
- [ ] Windows: alt-tab, minimise and restore with a pad and keyboard; activation
      behaves as before.

## Needs a touchscreen (M7a)

- [ ] Windows: two or more fingers on `MinimalSample.exe` each show a marker
      with its own id; a tap never also clicks; losing the foreground mid-gesture
      cancels every point. LineSweeper's touch buttons work through `WM_POINTER`.
- [ ] A pen still drives the mouse.
- [ ] Firefox and Safari touch on the web path (only Chrome's emulation has
      driven it).

## Needs real phones (M7b)

- [ ] A current iPhone in Safari and a mid-range Android phone in Chrome play
      LineSweeper's development page: portrait layout, safe-area insets, no
      scroll or zoom under a finger, no full-screen button on iOS, rotation,
      two fingers held at once, button size by thumb.
- [ ] Without S3TC the CPU-decoded atlas draws legibly and its load time and
      memory are acceptable.
- [ ] R6: time the 9,600-particle top-out frame on a phone.
      [R6 fixture requirements and results template](pacing-method.md#3-r6-prerequisites-for-an-exact-9600-particle-browser-frame)
      name the instrument: completed-frame CPU phases from
      `Application::frame_timings()`, paired with rAF intervals and the same
      observed-judder record as R5. The opt-in exact fixture/export is written;
      browser runtime acceptance is blocked by Chrome GPU access denial.
      Headless ParticleField counts, ordinary played top-out and native bench
      timings cannot supply this phone result. No phone measurement is added.
- [ ] Rotating with the pause menu open re-arranges the match underneath only
      on resume (known and documented; check it reads acceptably).

## Needs ears (Web Audio)

- [ ] The audio sample in a real browser through speakers: ping, chime, the
      drone, pan left and right, pitch, volume; the held keys.
- [ ] The first key, click or tap unlocks audio in Firefox, Safari, iOS Safari
      and Android Chrome, and the sound triggered by that same key is heard.
- [ ] Backgrounding the tab pauses and resumes the context without leaving it
      stuck suspended.
- [ ] An ADPCM bank written by Microsoft's own tools is read, and XAudio2 plays
      the test fixture `ping_adpcm.xwb`.

## The live demo (M5), checked 2026-10-13

The demo is live at <https://mattharding.dev/demo/linesweeper/>. The owner
deploys it from the PersonalWebsite repository's `main` branch with
`npm run deploy`, after `npm run sync-demo` has copied a bundle in; its
`BUILD.json` names the Labrador commit it was built from, `c369e8b` (tagged
`v0.1.0`), clean, `Release` under Emscripten 6.0.12. The module folder is
`m/cd7d8cfd9043/`, the first twelve hex digits of the SHA-256 of
`linesweeper.js` followed by `linesweeper.wasm`, which recomputing it from the
served files reproduces; the previous deploy's folder, `m/4544f56a476f/`, now
answers 404. Each box below was checked against that deploy on 2026-10-13
from this desktop, in headless Chrome 155, and a later deploy needs them run
again.

- [x] `curl -I` with `Accept-Encoding: br` against the page and the live
      `.wasm`: `application/wasm`, immutable `Cache-Control` under
      `m/<hash>/`, `no-cache` on the page, and `Content-Encoding: br` served
      for both. Headless Chrome, which also offers zstd, was answered with
      `Content-Encoding: zstd` on the same three files, and so was `curl`
      offering `gzip, deflate, br, zstd`. Neither response carries a
      `Cross-Origin-*` header or a `Content-Security-Policy`.

      ```
      $ curl -sI -H "Accept-Encoding: br" https://mattharding.dev/demo/linesweeper/
      HTTP/1.1 200 OK
      Content-Type: text/html
      CF-Cache-Status: HIT
      Cache-Control: no-cache
      Content-Encoding: br
      Server: cloudflare

      $ curl -sI -H "Accept-Encoding: br" https://mattharding.dev/demo/linesweeper/m/cd7d8cfd9043/linesweeper.wasm
      HTTP/1.1 200 OK
      Content-Type: application/wasm
      CF-Cache-Status: HIT
      Cache-Control: public, max-age=31536000, immutable
      ETag: W/"b3392f4638e74bf2397a4316206924be"
      Content-Encoding: br
      Server: cloudflare
      ```

      The loader, `m/cd7d8cfd9043/linesweeper.js`, answers the same way with
      `Content-Type: text/javascript`. The lines left out above are the date,
      `Connection`, Cloudflare's reporting headers, `CF-RAY` and `alt-svc`.
      The rest of `/demo/linesweeper/` - `poster.png`, `NOTICE.txt`,
      `BUILD.json` and `linesweeper.mp4` - is served `Cache-Control: public,
      max-age=0, must-revalidate` with an ETag, which revalidates on every use
      as `no-cache` does; the two text files under Brotli, the image and the
      video as they are. The video answers a byte range with `206 Partial
      Content` and `Accept-Ranges: bytes`.
- [x] What is served is what `BUILD.json` records. Fetched with
      `Accept-Encoding: identity`, the loader, the module, the poster and the
      notices are 118,982, 403,759, 15,060 and 92,118 bytes, and each one's
      SHA-256 is the one `BUILD.json` gives it (`c14723d3...`, `16567dcc...`,
      `c4e0c99b...`, `4c8c80d6...`). The Brotli responses decode to the same
      bytes. Cloudflare compresses them itself, and not to the same bytes every
      time: across fetches on 2026-10-13 the loader crossed the wire at
      32,180-32,250 bytes and the module at 150,445-150,670, about 32 KB and
      150 KB, against the 27,283 and 125,384 `BUILD.json` records at quality
      11. Its `index.html` is the
      bundle's example page, which the site does not serve: the site's own
      page stands at `/demo/linesweeper/`, and `index.html` there redirects.
- [ ] Play the live page in desktop Chrome, Firefox and Safari: click to start,
      play, quit, play again. **Chrome done, headless**: driven over the
      DevTools protocol, nothing but the page, its poster, its fonts, the
      site's favicon and Cloudflare's analytics beacon and its report was
      fetched before the click; the click fetched the loader and the module,
      the game
      drew and took arrow keys, Space and C (the frames were looked at) and
      Up and X, whose two quarter turns left its S piece looking as it was;
      Escape, Down, Down and Enter reached `onQuit` and the
      page's "Thanks for playing.", and "Play again" mounted a fresh canvas,
      fetched the module again from the browser's cache, started from an
      empty board, and played and quit the same way, with no console message
      and no browser warning throughout. **Firefox waits on BDP-06** (the
      page runner speaks only CDP) and **Safari on SM-09**.
- [x] The live demo plays the calm top-out (BDP-23). `c369e8b` contains
      `8c69272`, the commit that calmed it, and in headless Chrome a third
      game, Space pressed until the well filled, showed the stack on every
      frame before the top-out and none from the frame the banner appeared:
      the well behind the banner held only dim sparks, and screenshots taken
      over the next second showed them fading to a well that looked empty. This is the
      drawing looked at, not the flash count, which was measured on the
      desktop build (`samples/linesweeper/README.md`, *The bursts against WCAG
      2.3.1*) and not on this page. A topped-out match takes no Escape, as
      `play_state.cpp` says; R restarted it, and Escape, Down, Down and Enter
      then reached `onQuit`.
- [ ] A real phone gets the recording and fetches no wasm; a tablet with a
      keyboard gets the Play button back after a key. **Emulated half done**:
      under Chrome's touch emulation (5 touch points, a 390x844 mobile
      viewport), the page matched `(pointer: coarse)` and not
      `(any-pointer: fine)`, showed the recording, paused at its poster with
      the browser's controls, and its status line, and the network log held
      no request for `linesweeper.js`, `linesweeper.wasm` or
      `linesweeper.mp4`; a key press then removed the video and brought back
      "Play LineSweeper", still without fetching any of them. This is
      emulation, not a phone: the real-phone half waits on **SM-13** (an
      iPhone; the Android half is SM-44).
- [x] A machine without WebGL2 shows the `onError` message. Headless Chrome
      started with `--disable-webgl2`, Play clicked: the page fetched the loader
      and the module, and its status line read "LineSweeper stopped: This
      browser would not give the canvas a WebGL2 context. WebGL2 is in every
      current desktop browser; it can be turned off, and a GPU the browser has
      blocked takes it away." with the button relabelled "Try again". The
      engine's own `console.error` of the same startup failure was the only
      console message.
- [ ] A real context loss (GPU reset, or a tab hidden while it is lost) shows
      and clears the page's message. **Blocked on BDP-06**, which takes a real
      tab switch and a real loss; `RenderContextLossTests` covers the engine
      half with a forced loss. Not run against this deploy.
- [ ] Decide whether 4.3 MB of video is acceptable on phones, or swap in a
      smaller encode with `npm run sync-demo -- <bundle> --video <mp4>`.
      **The owner's decision, open as BDP-20.** What the emulated phone run
      saw: the site's page now sets the recording to `preload="none"` with
      controls, so it neither plays nor downloads until it is asked to, and
      the run fetched none of it (the file is 4,305,947 bytes). A tap on the
      video under emulation did not start it in headless Chrome, so what a
      real tap fetches has not been seen.

## Firefox sample checks (WEB#4)

- [ ] Run minimal, collision, audio, local_multiplayer and components against
      current Release outputs with the separate
      [Firefox BiDi runner](../../tests/render/web/run_firefox.mjs).
- [ ] Port and run `WebWindowTests`' input and shell-CSS checks. Chromium's
      CDP-specific DPR, touch cancellation and fullscreen activation checks
      have not been translated to Firefox BiDi by this increment.

The runner uses an owned temporary profile, loopback-only serving and default
Firefox security settings. It requires continuing real WebGL clear/draw
submissions, trusted key actions and the page's existing `Module.onQuit`;
warnings, errors and `Module.onError` fail it. Artifact byte lengths/hashes,
session/browser capabilities and raw WebGL strings are logged. Submission
counts do not prove pixel correctness, audible audio or a physical adapter.
No browser run is claimed by the 18 offline transport/control fixtures; with
Chromium's six existing fixtures, `BrowserProtocolTests` passes 24 cases under
Node without a browser or device.

**Current runtime gate, 2026-10-26.** Fresh `wasm-release` linking failed while
replacing `out/build/wasm-release/samples/minimal/MinimalSample.js` with
`PermissionError: [Errno 13] Permission denied`. That write is stopped. The
failed link left a 379,758-byte wasm dated October 26 beside the older
118,834-byte JS and 1,177-byte HTML dated October 24. That mismatched output
set was not run. Firefox has not launched for this increment, and all five
current-page runs remain unverified. The minimal output's denial is
established by that sample build. The temporary console-link diagnostic setting
was removed after the failure. No SDK source or configuration was edited.

A separate, original `WebWindowTests` target build then exited 1 while
replacing `out/build/wasm-release/tests/app/WebWindowTests.js`, also with
`PermissionError: [Errno 13] Permission denied`. Its task-owned process 17320
completed at `2026-10-26T21:49:57.9424754Z`; stdout, stderr and the process
receipt are retained under `out/overnight-2026-10-26/firefox-webwindow-build*`.
The failed link left a 48,038-byte wasm dated October 26 beside the older
36,559-byte JS and 1,193-byte HTML dated October 24. This mismatched set was
not run, and this output action is stopped. No Firefox launch, alternate
output, file deletion, permission change or SDK change followed the denial.
The separate input/CSS port and its trusted cancellation question remain
unverified; source planning alone does not complete those browser checks.

After building the five current pages, run each sample name through the same
command (`minimal`, `collision`, `audio`, `local_multiplayer`, `components`):

```text
cmake --build --preset wasm-release --target MinimalSample CollisionSample AudioSample LocalMultiplayerSample ComponentsGallery --parallel 1
node tests/render/web/run_firefox.mjs --browser "C:\Program Files\Mozilla Firefox\firefox.exe" --build-dir out/build/wasm-release --sample minimal
```

Record the build's source identity and each actual outcome before ticking a
box. Old generated outputs are not evidence of the current checkout.
LineSweeper's earlier Firefox run remains the separate `web.md` §9 M3 record.
