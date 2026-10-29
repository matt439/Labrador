# The web port — what is still to be tested

Everything in `web.md`'s milestones that can be written and checked on this
desktop has been, in headless Chrome (and headless Firefox for LineSweeper's
drawing, the other five sample pages and `WebWindowTests`), under every preset, with the pixel tests on the RTX 5080, WARP and
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
- [ ] Optional: one run with `allowed_channel_drift` at 0 to measure the
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

**WEB#6 local access gate, 2026-10-26.** A read-only HTTPS HEAD to
`https://mattharding.dev/demo/linesweeper/` was denied at
`2026-10-26T22:44:55.3087718Z` with `System.Net.Http.HttpRequestException`:
"An attempt was made to access a socket in a way forbidden by its access
permissions. (mattharding.dev:443)". The endpoint check is stopped. No hosted
page was inspected or opened in a browser, and no scaling, zoom or HUD
legibility result was obtained. The exact operation, target, reason and time
are retained in `out/overnight-2026-10-26/web6-live-head.json`. Local HTTPS
access is needed before this hosted-page acceptance can proceed.

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
- [ ] Firefox and Safari touch on the web path. Chrome's emulation and
      Firefox's synthetic BiDi touches (the
      [Firefox section](#firefox-sample-checks-web4)) have driven it; a real
      finger, a cancel the browser makes and Safari have not.

## Needs real phones (M7b)

**Owner decision, 2026-10-27 (`BDP-10`):** recorded iPhone evidence (`SM-13`)
and the host page's phone layout (`BDP-09`) are sufficient to lift the phone
gate. It stays closed until both are complete. No Android phone is available;
`SM-44` is deferred optional evidence. Android remains unverified.

- [ ] A current iPhone in Safari plays LineSweeper's development page:
      portrait layout, safe-area insets, no scroll or zoom under a finger, no
      full-screen button on iOS, rotation, two fingers held at once, button
      size by thumb.
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
- [ ] Deferred, optional (`SM-44`): repeat the phone checks on a mid-range
      Android phone in Chrome when one is available. This does not gate
      `BDP-10`.

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
      emulation, not a phone: the required real-phone half waits on **SM-13**
      (an iPhone); Android's **SM-44** is deferred optional evidence.
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
- [x] **Owner decision, 2026-10-27 (`BDP-20`): keep the existing 4.3 MB
      on-demand recording** (4,305,947 bytes); no smaller encode is needed.
      The site's page sets it to `preload="none"` with controls, so it neither
      plays nor downloads until it is asked to, and the emulated phone run
      fetched none of it. A tap on the video under emulation did not start it
      in headless Chrome, so real-phone playback remains part of **SM-13**.

## Repeated play (BDP-18), measured 2026-10-29

The packaged page (`web-dist/linesweeper/index.html`, `wasm-release`) was
driven in headless Chrome 156 by `run_page.mjs` with a scratch input script.
The renderer was ANGLE on Direct3D 11 on this desktop's RTX 5080. Each of
twenty rounds clicked Play, waited for five drawn frames, quit through the
pause menu, forced a collection (`HeapProfiler.collectGarbage`) and read
`Runtime.getHeapUsage`, `Memory.getDOMCounters`, the WebGL2 contexts the page
made (held through `WeakRef`, so counting did not keep them) and each
instance's `WebAssembly.Memory`.

| Round | Before: live contexts | Before: module heaps alive | After: live contexts | After: module heaps alive |
|---|---|---|---|---|
| 1 | 1 | 1 (17.1 MB) | 1 | 1 (17.1 MB) |
| 5 | 5 | 5 (85.6 MB) | 1 | 1 |
| 16 | 16 | 16 | 1 | 1 |
| 17 | 16, and Chrome warned "Too many active WebGL contexts. Oldest context will be lost." | 17 | 1 | 1 |
| 20 | 16 | 20 | 1 | 1 |

Before the change, the JS heap grew by about 0.1 MB a round and event
listeners grew by nine a round. Every finished instance stayed reachable
through the Web Audio device's capture-phase gesture listeners on `window`.
The first warning was at round 17, and no round had an unprompted loss of the
running game's context. After the change, the JS heap stayed between 1.14 and
1.43 MB and listeners stayed at three. Play to the fifth drawn frame took
109 to 228 ms in both runs (median about 120 ms), with no upward trend. The
first round pays the module compile.

The fix is in two parts. `Application` releases a quit browser game's
services ([APPLICATION.md §7](../../engine/app/APPLICATION.md)), and the page
loses the previous game's context on Play again, so the browser's count does
not wait for a collection. `LineSweeperBundlePageTests` asserts five rounds
with at most one live context and one module heap after each. This is one
desktop Chrome. Phone memory remains `SM-13`'s.

## Firefox sample checks (WEB#4)

- [x] Run minimal, collision, audio, local_multiplayer and components against
      current Release outputs with the separate
      [Firefox BiDi runner](../../tests/render/web/run_firefox.mjs).
      **2026-10-28, commit `0fcf3a7`, clean checkout; all five passed.** The
      `wasm-release` build of the five targets had nothing to relink, so its
      outputs are this commit's. Headless Firefox 158.0.1 (build
      20261019185856) under Node 24.14.0, one runner process per page, each
      exiting 0 with Firefox exiting 0 and its temporary profile removed.
      Every page initialised, kept clearing and submitting `drawElements`
      across 60 more animation frames, took its trusted keys, reached
      `Module.onQuit`, and logged nothing to the tab's console and nothing to
      `Module.onError`. The runner's own totals at quit:

      | Page | `.wasm` bytes, SHA-256 prefix | Clears | `drawElements` | Elements |
      |---|---|---:|---:|---:|
      | minimal | 381,074, `f11355f5e79e` | 73 | 73 | 40,260 |
      | collision | 399,062, `4efec4d78381` | 61 | 122 | 66,246 |
      | audio | 458,489, `706a9cac15dd` | 73 | 73 | 131,400 |
      | local_multiplayer | 374,275, `4db15e0bbe34` | 61 | 366 | 81,984 |
      | components | 404,847, `9cbbc5016e76` | 130 | 780 | 152,832 |

      What these runs do not prove: pixels (nothing was read back or looked
      at), the adapter (`RENDERER` read Firefox's sanitised bucket,
      `ANGLE (NVIDIA, NVIDIA GeForce GTX 980 Direct3D11 vs_5_0 ps_5_0), or
      similar`, on a machine with an RTX 5080), sound (the audio page's S key
      went through the page, and nothing listened, which is `WEB#3`'s), and
      WebWindow input or shell CSS (the next box). Firefox's stderr carried,
      for every page, the legacy wasm exception handling warning that `try` is
      deprecated in favour of `try_table`. It is not a console entry of the
      tested tab, so the runner does not fail on it; it is the same warning
      [web.md](web.md) §9 records for LineSweeper, and it lasts as long as
      the build emits legacy exceptions.
- [x] Port and run `WebWindowTests`' input and shell-CSS checks.
      **2026-10-28, commit `0fcf3a7` with the port uncommitted; four
      consecutive runs passed all 39 steps.** The `wasm-release` page
      (`.wasm` 48,059 bytes, SHA-256 prefix `e5752ef68d16`) relinks to the
      same bytes with the port's comment change to its source. Headless Firefox 158.0.1 under Node 24.14.0; each run
      exited 0 with Firefox exiting 0 and its profile removed, and the tab
      logged nothing. `run_firefox.mjs --test WebWindowTests` runs the same
      sequence and expected logs as Chromium's `WebWindowTests` entry
      ([window_input.mjs](../../tests/app/web/window_input.mjs)), through
      BiDi: `input.performActions` for the mouse, the wheel, keys and touches,
      `browsingContext.setViewport` for the device pixel ratio, and
      `script.evaluate` with `userActivation` for the granted full-screen
      request. Focus, the capture analogue, the defaults the canvas takes,
      wheel notches, the measured box under the shell CSS and its CSS
      resizes, a ratio of two, touch ids and focus, full screen granted, left
      and refused, and the closed window all hold in Firefox. Where the two
      runs differ, and why:

      - **Touch cancellation is not translated.** BiDi has no action that
        makes the browser cancel a pointer, so the second finger lifts
        instead (its up, then its lost capture). A cancel on real glass is
        `WEB#5`'s.
      - **Control is a key.** BiDi holds Control by pressing it, so the chord
        step also expects Control's own `key_down` and `key_up`, and its
        keydown kept.
      - **The starting ratio is set.** This machine's Windows text size is
        110%, and headless Firefox starts at a device pixel ratio of 12/11
        (the 400x250 box measured 436x273). The runner sets a ratio of one
        before the page loads, and returns to one after the ratio-two step.
      - **Touch positions are compared within one client pixel.** Firefox
        puts a synthesised touch on a device pixel of its own window, which
        that text scaling still sizes, and the pointer event keeps the
        fraction: a touch sent at (120, 70) arrived at (120.083, 69.667) and
        floors to row 49. Mouse positions are whole and stay exact.
      - **Fingers move one per tick.** Two fingers moved in one BiDi tick gave
        one `touchmove` naming both and a `pointermove` for the first only;
        the window listens to pointer events.
      - Two steps changed for both browsers, because a WebDriver press is a
        move and then the button, and no move is sent to where the pointer
        already is: the click after Tab moves to the canvas first, and the
        position scaled at a ratio of two is reached from beside the canvas.
        Chromium's entry passed with both.

      What these runs do not prove: a browser hiding the page (visibility is
      synthetic in both browsers), a touch the browser cancels, a real
      touchscreen, pad or display, or pixels. Firefox's stderr carried the
      legacy wasm exception warning above and, in some runs, a
      `UrlClassifierListManager` error from Firefox's own modules; neither is
      a console entry of the tested tab.

The runner uses an owned temporary profile, loopback-only serving and default
Firefox security settings. For a sample it requires continuing real WebGL
clear/draw submissions, trusted key actions and the page's existing
`Module.onQuit`; for `WebWindowTests` it requires every step's whole log, and
fails if the shell settles its failure promise. Warnings and errors logged to
the tested tab, and `Module.onError`, fail either. Artifact byte
lengths/hashes, session/browser capabilities, for a sample raw WebGL strings, and Firefox's
last 16 KiB of stderr are logged. Submission counts do not prove pixel
correctness, audible audio or a physical adapter. No browser run is claimed by
the 24 offline BiDi transport, control and input-translation fixtures;
`BrowserProtocolTests` runs them under Node, without a browser or device,
with Chromium's six transport fixtures and three sample-key fixtures, 33 cases
in all.

After building the five current pages and `WebWindowTests`, run each sample
name through the same command (`minimal`, `collision`, `audio`,
`local_multiplayer`, `components`), and the window's page through `--test`:

```text
cmake --build --preset wasm-release --target MinimalSample CollisionSample AudioSample LocalMultiplayerSample ComponentsGallery WebWindowTests --parallel 1
node tests/render/web/run_firefox.mjs --browser "C:\Program Files\Mozilla Firefox\firefox.exe" --build-dir out/build/wasm-release --sample minimal
node tests/render/web/run_firefox.mjs --browser "C:\Program Files\Mozilla Firefox\firefox.exe" --build-dir out/build/wasm-release --test WebWindowTests
```

Firefox is not in CI and these runs are not ctest entries; Chromium's
`WebWindowTests` entry runs the same sequence wherever configure finds a
Chromium.

Record the build's source identity and each actual outcome before ticking a
box. Old generated outputs are not evidence of the current checkout.
LineSweeper's earlier Firefox run remains the separate `web.md` §9 M3 record.
