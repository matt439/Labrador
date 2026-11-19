# The web port — what is still to be tested

This ledger combines dated results and open acceptance from `web.md`'s
milestones. Each result applies to its recorded revision, browser and device;
an earlier desktop or headless pass does not qualify later code or physical
hardware. What remains needs a person, hardware, another browser or a hosted
run. Tick a box in the commit that checks it,
add the result to [web-as-built.md](web-as-built.md) under its milestone, and
update `web.md` §9's status summary, milestone row or risk row.

## Approved acceptance actions, 2026-11-16

The independently scoped `SM-93` iframe regression is
[locally qualified](2026-11-17-cross-origin-iframe.md) in headless Chromium.
It leaves the physical, headed, other-browser and external-host acceptance
in this ledger open.

The owner approved the remaining acceptance scopes in the final decision
batch. Approval records what to do; it supplies no play, listening, display,
phone, driver or live-service result. [BACKLOG.md](../product/BACKLOG.md) owns
the story acceptance, and [TODO.md](../../TODO.md) indexes the next actions.

In TODO, `BLOCKED` on the hardware rows means approved work awaiting owner
attendance or a physical prerequisite. `WEB#12` is `WAITING` for a demo deployment.
Listable checks stay automated; the owner's contribution remains informal
play, listening and observations on the real devices.

`OCA-119` is also **BLOCKED** on a qualified trusted-key or informal human
headed-browser observation. The [2026-11-17 attempt](2026-11-17-browser-escape-attempt.md)
stopped at Computer Use's URL-policy gate before any trusted click or Escape.
No full-screen exit or game-key routing was observed. Headed launch and
offline runner fixtures do not satisfy that acceptance, and the existing
explicit-exit/full-screen-flag tests remain separate evidence.

| Approved session | Remaining evidence and boundary |
|---|---|
| Pad: `OCA-48`, `OCA-45`, `PI-21`, with `BDP-06` | Informal native and Chrome play with the owned XInput pad, including background-input behaviour and how vibration feels. Record date, source and device identity with observations. Automated mapping and slot tests remain their own evidence; this session does not imply a second pad or an observed unplug-slot-0 path. |
| MacBook Air: `SM-09` | Informal Safari play on the Air, with its actual OS/browser and played revision recorded. The earlier version ceiling and hosted Safari runs remain distinct from this unperformed acceptance. |
| iPhone and LAN: `SM-14`, then `SM-13` | Try and correct the LAN-serving draft in the real iPhone session; retain the phone's layout, touch and measurement limits. The phone gate stays closed until its existing acceptance is met; optional Android evidence remains separate. |
| Listening and recovery: `WEB#3`, `REV#8` | Listen through real output and observe endpoint loss/reappearance, retained-bank playback and suspension/resume. Wave recordings, fake endpoints and analyser values do not establish audible output or recovery on a physical endpoint. |
| Displays: `BDP-07`, `REV#3`, `OCA-66` | Record physical 60/144 Hz pacing observations, the real display-change case for the D3D11 same-size factory-refresh question, and template play at actual 150% Windows scaling. Callback timing, source inspection and emulated scale do not replace visible judder, driver behaviour or the real scaled display. |
| Radeon: `SM-01`, before `SM-07` | The already approved owner-attended BIOS enablement, AMD driver and display-path/adapter checks precede low-tier measurement. Record the actual driver and created Radeon device; a cross-adapter present or the RTX/L4 cannot supply this evidence. No current hardware state or new measurement is claimed here. |

On 2026-11-20 the [live documentation footer](https://labradorengine.com/docs/)
identified revision `f7b717c42199c1f5098a8c3fccbc17e970f1df67` of 2026-11-17.
`SITE#4` is **BLOCKED** on the owner's real-iPhone search observation. Record
what pressing Return does with an empty search and with a query, together with
the date, browser and revision shown on the phone.

On 2026-11-20 the hosted demo still identified the recorded `v0.3.0` revision
`0f4f5033b317a334d89e3cf14ab98684e0598ddd` in both
[BUILD.json](https://mattharding.dev/demo/linesweeper/BUILD.json) and
[SITE.json](https://mattharding.dev/demo/linesweeper/SITE.json). After the
next demo deployment, `WEB#12` runs `check_live.mjs` for the intended commit
and retains its dated output.

`OCA-79` closed on 2026-11-16: the unchanged current-release `v0.3.0` starter,
with plain engine files, passed its
[first push's workflow](https://github.com/matt439/labrador-starter-proof/actions/runs/43874327329)
in the dedicated public repository after account, target and payload review.
The guides retain that result and the remaining browser-form/sign-in limits.
This proof pushed no Labrador work branch, deployed no site and created no
recurring publication bot; [BACKLOG.md](../product/BACKLOG.md#oca-79) owns its receipt.

`SM-30` is `BLOCKED` on [read access to Workers Builds](../../website/README.md#sm-30-access-check-2026-11-16).
Once access is available, reconcile only differences from
`npm ci && npm run gate`, Node from `.node-version`, and no `NODE_VERSION`
override. The next normal production log must show Node 24 and the gate
before closure; no forced deployment or setting change occurs here.

Labrador's `PI-14` coordination story is closed as delegated to ColourWars.
Its candidate acceptance remains in that repository, with the previously
recorded candidate ZIP and Sandbox limitations preserved. Delegation does not
certify a current client release, physical pad, audible audio or Radeon run.

## Needs a push (CI)

- [x] The wasm job is green on the GitHub runner: the pixel pages on its
      SwiftShader, `RenderPixelTestsWithoutS3tc`, `WebWindowTests` and the six
      `*PageTests`. Run 38130728123, PR #1, every step passed, with the five
      there were then; run 38339603954, on master at `fbb753d`, passed all six,
      `ComponentsPageTests` among them.
- [x] The null backends run even when a page step fails, and the jobs go
      red. They are now a separate `wasm-null` job rather than a step after
      the pages. Procedure: on a throwaway `review-*` branch from `master`
      (pushes there run CI without a pull request), add
      `console.error('...')` as the first line of the `<script>` in
      `samples/minimal/web/shell.html`, push, read the run, then delete the
      branch; nothing merges. [Run 40868195679](https://github.com/matt439/Labrador/actions/runs/40868195679), 2026-10-29: in
      `wasm`, the sample-pages step failed on `MinimalPageTests` alone (9 of
      10 passed), the bundle upload was skipped and the job failed;
      `wasm-null` configured, built and ran all 28 tests, 27 passing and
      `MinimalPageTests` failing on the same error, since it registers the
      pages too; `wasm-consumer` failed on it as well, its project copying
      the template's page; every other job passed and the run failed.
- [x] The x64 jobs' `RenderPixelTests` pass against the 17 regenerated text
      goldens and the three new frames on the runner's Direct3D adapter
      (`x64-release` and `x64-debug-d3d12`, same run).
- [x] The wasm job uploads the `linesweeper-web` artifact and its `BUILD.json`
      names the commit CI built, clean. On a pull request that is GitHub's
      merge commit for it rather than the branch head, since that is what the
      job checks out.
- [x] One run with `allowed_channel_drift` at 0 measures the runner's
      adapters: run 41053232985 (2026-10-30) failed `RenderPixelTests` on
      five frames by 1 under all three Direct3D jobs and on three by 1 in
      Chrome 156's SwiftShader, and nothing moved by 2. The allowance is 1
      (`tests/render/golden_image.cpp`).

## Needs Safari (M3's done-when)

- [x] LineSweeper, local_multiplayer, minimal, collision, the audio sample and
      the components gallery in desktop Safari: drawing, keys, quit, context loss and restore.
      Moving off legacy wasm exceptions needs Safari 18.4 on every browser the
      demo claims. Checked 2026-11-05 in Safari 26.6.2 on GitHub's hosted
      `macos-26-arm64` runner; the [Safari section](#safari-sample-checks-web1)
      has the run. The owner's current MacBook Air (macOS 27.0.1, Safari
      27.0.1, 2026-11-03) has loaded and briefly played the live demo; play
      there stays informal (SM-09).
- [x] Safari masks `RENDERER` as "WebKit WebGL", as assumed, so the device
      strings are unmasked through the extension. The same run read
      `RENDERER` as `WebKit WebGL` and, through `WEBGL_debug_renderer_info`,
      `Apple GPU` from `Apple Inc.`.

*Amended 2026-11-03:* re-planned as an automated safaridriver run on
`macos-latest` (`WEB#1` in TODO.md) plus the owner's informal play on the Air.

## Needs real displays (R5)

- [ ] Frame pacing of the fixed 60 Hz step under `requestAnimationFrame` on a
      60 Hz display and on a high-refresh one: judder or none.
      [R5 method and results template](pacing-method.md#2-r5-raf-delivery-and-observed-judder):
      console rAF timestamps, raw JSON and nearest-rank p50/p99; separately
      record visible irregular holds/jumps during comparable motion. Callback
      bins alone are not visible-judder or scan-out evidence. The console
      procedure has run in headless Chrome; no display result is added.
- [x] The mattharding.dev stage at 1x, 1.25x, 1.5x and 2x and with browser
      zoom: the HUD text is readable when the game is drawn smaller than
      1280x720.

**WEB#6 emulated stage check, 2026-10-29.** Headless Chrome (ANGLE D3D11 on
the RTX 5080) loaded `https://mattharding.dev/demo/linesweeper/` ("Built from
Labrador 1e60fb2"), pressed Play, made eight hard drops and captured the stage
at one image pixel per device pixel through DevTools. Device scale and browser
zoom were emulated with `Emulation.setDeviceMetricsOverride`: zoom as device
scale equal to the zoom and a CSS viewport of 1280x900 divided by it. No
physical scaled display or real zoom control was used. The stage is the 52rem
column at 16:9, 800x450 CSS pixels from a 1280-wide window, and the drawing
buffer follows device pixels.

| Case | Drawing buffer | HUD |
|---|---|---|
| Scale 1, 1.25, 1.5, 2 | 800x450, 1000x563, 1200x675, 1600x900 | Labels, values and the controls line read cleanly |
| Zoom 150%, 200% | 1200x675, 1186x667 | Clean |
| Zoom 80%; 640-pixel window at scale 1 | 640x360; 593x334 | Readable; board grid lines thin unevenly and adjacent cells merge |
| Zoom 67% | 536x302 | Readable with effort; letters lose strokes (NEXT nearly reads WEXT) and the queued O piece draws as two bars |
| Zoom 50% | 400x225 | Not reliably readable: NEXT reads MEXT, the score digits deform and the controls line breaks up |

The page logged no console message, warning or exception. On 2026-10-29 the
owner checked the live demo at a real Windows display scale, and it reads
fine. The owner judged 50% browser zoom too small a setting to support, so
its illegible HUD needs no fix. Zoom down to 67% stays readable.

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
- [ ] The components gallery by finger, natively and in a browser: a tap on a
      row's value steps it once, a drag sets a slider and clamps past its
      ends, and a second finger does nothing. `ComponentsPointerPageTests`
      drives it with Chromium's touch emulation only.

## Needs real phones (M7b)

The explicitly invoked [iOS Simulator pilot](ios-simulator-pilot.md) has offline-qualified
tooling but no actual Simulator run yet. It is separate from the checks below
and cannot close them or lift the phone gate.

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
      observed-judder record as R5. The opt-in exact fixture/export passes
      in headless Chrome on the desktop.
      Headless ParticleField counts, ordinary played top-out, native bench
      timings and the throttled desktop rehearsal in
      [web-as-built.md](web-as-built.md#throttled-desktop-rehearsal-2026-11-13)
      cannot supply this phone result. No phone measurement is added.
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

*Amended 2026-11-03:* the host repository's committed `BUILD.json` has
since named a bundle built at `1e60fb2` (`v0.2.0`); the live page was not
re-checked against it, and neither were these boxes. Under the
owner's 2026-11-03 decision the server-side boxes become a script run after
each deploy, which records its dated output here in place of manual ticks
(`WEB#12` in TODO.md). Later on 2026-11-03 the owner deployed `v0.3.0`
(`0f4f503`): the live `BUILD.json` names it, and the served module, loader,
notices and poster match its SHA-256 values. These boxes were not re-run
against that deploy.

*Amended 2026-11-04:* the script is
[`tools/web_bundle/check_live.mjs`](../../tools/web_bundle/check_live.mjs).
After each deploy, from this checkout, run

```text
node tools/web_bundle/check_live.mjs --expect-commit <deployed commit> --out live-check.txt
```

and put its output in a dated block here, in place of the first two boxes
below. It checks the page's and the module folder's `Cache-Control` values,
the others' revalidation, the content types, Brotli where it is offered and
its decoded bytes, each served file's size and SHA-256 against the served
`BUILD.json`, the folder name, the commit `BUILD.json`, `SITE.json` and the
page's "Built from Labrador" line name, and that no COOP, COEP or
script-restricting Content-Security-Policy is sent. Its offline test,
`check_live.test.mjs`, runs in CI. It has not yet been run against the live
site: cloud sessions cannot reach `mattharding.dev`. On 2026-11-04 it passed
31 of 31 checks against host commit `6b57e7c` served on loopback through the
host's own `worker/index.js`, with a stand-in for the asset store and
Cloudflare's Brotli; that is the Worker's header logic, not Cloudflare's edge.
The Worker at that commit sends `Content-Security-Policy: frame-ancestors
'none'` with every response, which restricts no script; the 2026-10-13 headers
below predate it.

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
      and no browser warning throughout. **Firefox's half runs as
      `FirefoxLineSweeperBundlePageTests`** (the
      [Firefox record](#firefox-sample-checks-web4)), which plays, loses and
      restores the context, quits and plays again on the bundle page, and
      **Safari waits on SM-09**.
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
      and clears the page's message. `RenderContextLossTests` covers the
      engine half with a forced loss. **Automated in Chrome since 2026-11-04
      (`WEB#14`)**: `LineSweeperBundlePageTests` and
      `LineSweeperHostPageTests` crash Chrome's GPU process over DevTools
      (`Browser.crashGpuProcess`, what `chrome://gpucrash` does; that page
      cannot be navigated to from DevTools, which aborts it), and the page
      hears `onContextLost` with its waiting message and `onContextRestored`
      with its instructions back, and the game draws again. They then open a
      tab in front of the game (`Target.createTarget`), which hides it, stops
      its frames and pauses it, and activate the game's tab again, which
      shows it. Both are Chrome's own events, not ones the page makes, and
      both pass in headless Chrome 153 on SwiftShader against the `v0.3.0`
      module, where the GPU process restarts on the same software device.
      Not run against the live deploy, and not on a real GPU's driver reset.
      Firefox's entry plays the bundle page with a forced loss and restore;
      the GPU process crash and the tab switch run in Chromium only.

      **Headed on the desktop's GPU, 2026-11-13 (`REV#157`)**: the same
      driver, `bundle_input.mjs`, against the `wasm-release` bundle built at
      `1dc8533c` (`BUILD.json` clean), in a headed Chrome 157.0.8103.78 on
      Windows 11 build 26300 with an RTX 5080 on NVIDIA driver 616.92. The
      runner was a scratch copy of `run_page.mjs` that only dropped
      `--headless=new` and `--enable-unsafe-swiftshader` and fixed its ports;
      the copied driver logged, around the crash, the game's own context's
      `WEBGL_debug_renderer_info` strings and `SystemInfo.getProcessInfo`'s
      GPU process. Before every crash and after every restoration the game's
      context named `ANGLE (NVIDIA, NVIDIA GeForce RTX 5080 (0x00002C02)
      Direct3D11 vs_5_0 ps_5_0, D3D11)`, never SwiftShader, and the GPU
      process had a new id afterwards. Six `Browser.crashGpuProcess` crashes
      across six runs were all heard as `onContextLost` then
      `onContextRestored` with the page's two texts, and the game drew again;
      restoration arrived 1.8, 1.9, 2.8, 4.8, 4.8 and 5.8 s after the
      command, on a desktop where other agents' builds and browsers were
      running, so those are not a measure of Chrome's restart. With
      `--disable-features=CalculateNativeWinOcclusion` the whole driver
      passed three runs out of three (37 to 40 s each). Without it, two runs
      out of two failed at the tab switch: `Target.activateTarget` left the
      game's tab `hidden` for 60 s although DevTools reported it in the same
      window, in the normal state. The window was behind others the run did
      not own and was never brought to the front, and the same step passed
      once Chrome stopped computing occlusion, so Chrome hid a covered
      window's page; that is Chrome's visibility rule, not the page failing.
      `chrome://gpucrash` was also tried, in a second tab opened over
      DevTools and as a URL handed to the running browser on its command
      line: neither crashed the GPU process (the first tab's URL came back
      empty, the second opened a new-tab page), so it needs the address bar
      typed into, which this run did not do. This is a real GPU and driver
      under a GPU process crash, which ANGLE's Direct3D 11 device rebuilds on
      the same adapter; it is still not a driver reset (a Windows TDR) and
      not the live deploy, so the item stays open.
      **Windows headless profile, 2026-11-17:** the current runner also
      disables native-window occlusion tracking in its disposable Windows
      headless processes. A focused bundle run at `28844a00` completed this
      driver in 84.9822 s within its unchanged 120 s limit, including its real
      tab-switch assertions; the wasm-null track passed 30/30. Bare-page
      callback comparisons and their timing limits are recorded in
      [verification](../verification.md#windows-headless-native-occlusion-2026-11-17).
      This supplies software test-process evidence, while the real driver
      reset and deployment acceptance above remain open.
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
      [web-as-built.md](web-as-built.md)'s M3 entry records for LineSweeper,
      and it lasts as long as the build emits legacy exceptions.
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

- [x] Cover LineSweeper's development page and its bundle page, and
      register every run as a ctest entry where configure finds a Firefox
      (`WEB#10`, story SM-11). **2026-11-05, commit `11292fa` with the change
      uncommitted; all eight entries passed** under `ctest --preset
      wasm-release -R "^Firefox"`, in headless Firefox 159.0.1 (build
      20261102144711) under Node 24.14.0, each removing its profile. The
      development pages' totals at quit:

      | Page | `.wasm` bytes, SHA-256 prefix | Clears | `drawElements` | Elements |
      |---|---|---:|---:|---:|
      | linesweeper | 482,481, `bc3eb73fd899` | 135 | 686 | 158,598 |
      | minimal | 438,365, `0c766f25e541` | 81 | 81 | 45,684 |
      | collision | 458,067, `34bf9d5f7635` | 62 | 124 | 67,332 |
      | audio | 516,019, `2ed67de96843` | 81 | 81 | 145,800 |
      | local_multiplayer | 455,510, `533c728ab08a` | 62 | 124 | 139,128 |
      | components | 476,936, `68c1d01380b9` | 191 | 1,323 | 295,542 |

      `FirefoxWebWindowTests` passed all 41 steps (`.wasm` 48,986 bytes,
      `2c9245d2cb54`). The bundle entry (`linesweeper.wasm` 476,013 bytes,
      `aef5c7e08877`) passed `bundle_page.mjs`'s desktop instructions and its
      play, focus and Tab, forced `WEBGL_lose_context` loss and restore, QUIT,
      and replay on a fresh canvas that hears a synthetic visibility change:
      the steps and assertions `LineSweeperBundlePageTests` runs in Chromium.
      The runner serves the bundle folder itself, with
      `bundle_page.mjs`'s factory wrapper in place of the unqueried loader,
      and translates the steps' DevTools clicks, keys and navigations to
      BiDi. It does not run the touch and controller gates, the unsupported,
      download, truncated and runtime failure pages, a GPU process crash, a
      tab switch or repeated-play memory, which this runner does not yet
      drive over BiDi.

      Two of the earlier entries had drifted since 2026-10-28 and failed
      before this change: `components` stalled in full screen, because the
      gallery's keys had grown a restart and a second Escape that
      `ComponentsPageTests` sends and the Firefox list did not; and
      `WebWindowTests` failed on its own last step's deliberate error, which
      `window_input.mjs`'s `expected_complaint` now names to both runners.
      The first launch of the day exited at once and left a Firefox running
      the newer build on the runner's profile, which looks like a pending
      update applied on launch; that process was stopped by its PID, and
      every later run passed.

The runner uses an owned temporary profile, loopback-only serving and default
Firefox security settings. For a sample it requires continuing real WebGL
clear/draw submissions, trusted key actions and the page's existing
`Module.onQuit`; for `WebWindowTests` it requires every step's whole log, and
fails if the shell settles its failure promise, except the one error its
last step causes on purpose. For the bundle page it requires each of
`bundle_page.mjs`'s assertions. Warnings and errors logged to
the tested tab, and `Module.onError`, fail any of them. Artifact byte
lengths/hashes, session/browser capabilities, for a sample raw WebGL strings, and Firefox's
last 16 KiB of stderr are logged. Submission counts do not prove pixel
correctness, audible audio or a physical adapter. No browser run is claimed by
the 30 offline BiDi transport, control, input-translation and option fixtures;
`BrowserProtocolTests` runs them under Node, without a browser or device,
beside Chromium's transport, sample-key, input-bridge and bundle-host
fixtures, 78 cases in all.

Configure registers eight entries where it finds a Firefox
(`LABRADOR_TEST_FIREFOX`) and the render backend rasterizes:
`FirefoxLineSweeperPageTests`, `FirefoxMinimalPageTests`,
`FirefoxCollisionPageTests`, `FirefoxAudioPageTests`,
`FirefoxLocalMultiplayerPageTests`, `FirefoxComponentsPageTests`,
`FirefoxWebWindowTests` and `FirefoxLineSweeperBundlePageTests`. CI's wasm
job now opts into software WebGL and selects all eight entries. Without that
option, the runner image's Firefox gives a page no WebGL2 context: the runner has no GPU, and Firefox's driver blocklist
refuses the software adapter it has, so on 2026-11-05 (run 41944047596) each
drawing entry failed at startup with "Failed to create WebGL context" while
the window's entry passed. The drawing entries run wherever configure finds a
Firefox on a machine with a GPU, which remains a separate hardware check
from CI's software coverage.

Configuring with `-DLABRADOR_FIREFOX_SOFTWARE_WEBGL=ON` passes the runner's
opt-in `--software-webgl` to all eight entries. It writes three preferences
into the temporary profile's `user.js`: `webgl.force-enabled` past the
blocklist, `webgl.angle.force-warp` to put ANGLE's Direct3D 11 device on WARP,
Windows' software rasteriser, and `webgl.sanitize-unmasked-renderer` off, so
each sample's logged renderer names the device that drew. On 2026-11-13, with
Firefox 160 and `wasm-release` at `1dc8533c`:

- In Windows Sandbox with its virtual GPU disabled, whose only display adapter
  was the Microsoft Remote Display Adapter, `minimal` and `linesweeper` failed
  without the flag ("AllowWebgl2:false restricts context creation on this
  system"), as CI's did. `webgl.force-enabled` alone drew `minimal` on
  "ANGLE (Microsoft, Microsoft Basic Render Driver (0x0000008C) Direct3D11
  vs_5_0 ps_5_0)", which is WARP; the blocklist, not a missing device, was what
  refused. With all three preferences the six samples and the bundle page
  drew and passed on that device, and the window's sequence passed. Not every
  attempt passed: the first two Firefox launches after the Sandbox booted
  waited past the runner's 30-second BiDi endpoint limit, one launch failed to
  start its tab process, and `audio` failed two of its three completed runs
  with "InvalidStateError: Closed before resume completed" from
  `AudioSample.js`, a Web Audio context closed at quit before its resume
  settled, which no other `audio` run here has shown. The Sandbox is not CI's
  runner; whether that runner's Firefox reaches WARP the same way is shown
  only by a CI run with the option.
- On the desktop with its RTX 5080, Firefox draws `minimal` on its GPU without
  the flag (the logged string is Firefox's sanitized "NVIDIA GeForce GTX 980"
  bucket) and on the Microsoft Basic Render Driver with it, so
  `webgl.angle.force-warp` moves a machine with a GPU onto WARP. Configured
  with the option, all eight `Firefox*` entries passed through ctest there.

WARP is not the device a player's Firefox uses, and a submission count on it
proves no more than one on a GPU. CI now passes the option and selects
`-R "^Firefox"` with verbose output.
The sample runner refuses a non-WARP renderer in this mode. The
[hosted verification below](#hosted-browser-verification-2026-11-16) records
the published runs and the audio race's subsequent repair.

**Local integration check, 2026-11-15:** a fresh isolated `wasm-release` build
based on `d44101a0`, with the CI/runner changes in this work branch, registered
all 45 expected tests. All eight Firefox entries passed (41.11 s) on MattPC,
Firefox 160.0 build 20261104181552. Each of the six samples logged ANGLE's
Microsoft Basic Render Driver (0x0000008C), Direct3D11, and passed the new
fail-closed renderer check. Five additional consecutive FirefoxAudioPageTests
runs passed (22.69 s total); this did not reproduce the Sandbox resume race
and does not establish its absence on the hosted runner. No engine source or
shader changed. These are
local software-renderer results, not hosted CI, pixel, listening or device
acceptance.

To run them, or one page by hand (either
runner command also takes `--software-webgl`):

```text
ctest --preset wasm-release -R "^Firefox" --output-on-failure
node tests/render/web/run_firefox.mjs --browser "C:\Program Files\Mozilla Firefox\firefox.exe" --build-dir out/build/wasm-release --sample linesweeper
node tests/render/web/run_firefox.mjs --browser "C:\Program Files\Mozilla Firefox\firefox.exe" --bundle out/build/wasm-release/web-dist/linesweeper
```

## Safari sample checks (WEB#1)

- [x] Run the six sample pages and LineSweeper's bundle page in desktop Safari
      with the [Safari WebDriver runner](../../tests/render/web/run_safari.mjs),
      as CI's `safari` job does on every change that builds.
      **2026-11-05, CI run 41956008919 of pull request 120 (branch head
      `dee1830`); all seven passed.** Safari 26.6.2 under safaridriver on
      GitHub's hosted `macos-26-arm64` image 20261026.0455.2, Node 24.22.1,
      one session per page, from the `wasm` job's `wasm-sample-pages` and
      `linesweeper-web` artifacts. WebGL2 was available: `RENDERER` read
      `WebKit WebGL` and `WEBGL_debug_renderer_info` unmasked `Apple GPU`
      from `Apple Inc.`. Every sample page initialised, cleared and submitted
      `drawElements` across 60 more animation frames, heard a forced
      `WEBGL_lose_context` loss and its restoration, drew again on the restored
      context across 60 more frames, took its trusted keys, reached
      `Module.onQuit`, and made no console warning or error, uncaught error,
      unhandled rejection or `Module.onError`. The runner's own totals at quit:

      | Page | `.wasm` bytes, SHA-256 prefix | Clears | `drawElements` | Elements |
      |---|---|---:|---:|---:|
      | linesweeper | 482,462, `a341aee50ade` | 265 | 1,336 | 310,794 |
      | minimal | 438,365, `0c766f25e541` | 163 | 163 | 89,940 |
      | collision | 458,067, `34bf9d5f7635` | 135 | 270 | 146,610 |
      | audio | 516,025, `6113af6f1d81` | 158 | 158 | 150,732 |
      | local_multiplayer | 455,510, `533c728ab08a` | 135 | 270 | 302,940 |
      | components | 476,936, `68c1d01380b9` | 362 | 2,520 | 564,858 |

      The bundle page (`linesweeper.wasm` 476,013 bytes, `aef5c7e08877`) ran
      the same desktop play, Tab out of the game, forced context loss and
      restore, quit and replay that its Chromium and Firefox entries run.

      What these runs do not prove: pixels (nothing was read back or looked
      at), a physical Mac's GPU (the runner is a virtual machine whose WebGL2
      draws on a virtual Apple GPU), Safari 27 (the owner's Air runs 27.0.1;
      the runner's Safari is the image's), sound (`WEB#3`), a physical pad or
      display, WebWindow input or shell CSS, and a warning Safari prints itself
      without a console call: classic WebDriver has no console log, so the
      runner's hooks, which its server puts at the top of each page's
      `<head>`, hear only what the page says. The bundle's touch and
      controller gates, failure pages, real GPU loss, real tab switch and
      repeated-play memory run only in Chromium.

Record the build's source identity and each actual outcome before ticking a
box. Old generated outputs are not evidence of the current checkout.
LineSweeper's earlier Firefox run remains the separate M3 entry in
[web-as-built.md](web-as-built.md).


### Safari pixel exit-protocol wiring (2026-11-15)

The change shipped in `df60f96e` and `c28528ca` adds `run_safari.mjs --test-page`
for the existing `RenderPixelTests.html` page and carries that page, loader
and embedded-data wasm from the Windows build to the Safari job. The runner
requires the page's `labradorExit` promise to settle to integer zero without
an abort, emits the test's console output, and rejects page complaints.

The initial local verification used the runner's Node fixtures on Windows,
not Safari, WebKit emulation or an Apple GPU measurement. The
[hosted verification below](#hosted-browser-verification-2026-11-16) now records
actual Safari readback. Per-case passing drift records, Safari rejected-frame
extraction and its full WebWindow input sequence remain unimplemented or
unverified. REV#151 and the week-long REV#159 stability observation stay open.


### Pixel failure diagnostics (2026-11-15, local fixtures only)

The stacked `work/pixel-evidence` tooling change reuses `golden_image.cpp`'s
existing mismatch text. Safari's test-page runner emits a one-line JSON
`pixel diagnostics` record before checking complaints and the exit result,
while retaining the raw console lines. Each parsed failing frame carries the
pixel count, differing and out-of-tolerance counts, allowance, worst channel
delta and first failing coordinate. Repeated frames remain separate records;
truncated mismatch records are counted as unparsed rather than discarded.
An empty list is not a passing run or a claim of zero drift. Passing-frame
statistics are still not emitted by the comparator, and the exit protocol
and complaints remain the suite verdict. The parser is browser-neutral and
fixture-tested; this change does not run Safari, Firefox, WebKit emulation,
a renderer, or any stability observation. It adds no GPU acceptance evidence.


### Firefox baseline readback, local 2026-11-15

The BiDi runner now accepts `--build-dir <build> --test RenderPixelTests`
with optional `--software-webgl`. It waits for the existing page's
`labradorExit`, records the WebGL2 contexts created by the test itself,
checks their WARP renderer in software mode, and collects rejected PNGs
into `firefox-golden-actual` beside the page before rejecting failed exits.
It uses the shared mismatch diagnostic parser; absent mismatch records do
not substitute for the page exit verdict. No golden or tolerance changed.

The already-built wasm Release artifacts from the local LAB-1 build
(base `d44101a0`, harness commit `4179d934`) were reused without compiling.
Firefox 160.0 build 20261104181552 passed all 71 cases and 76,276 assertions
on Microsoft's Direct3D11 Basic Render Driver. No rejected PNG was emitted.
The raw run log, including artifact SHA-256s, is retained locally as
`labrador-firefox-pixels.log` in the task's temporary log directory.

This is baseline local WARP evidence, not physical GPU or hosted CI evidence.
Firefox's native stderr reported repeated `WebGL context was lost` warnings;
these were not delivered as BiDi console complaints. The runner preserves
that stderr and this result does not claim warning-free browser execution.
S3TC masking and through-post variants remain unimplemented for Firefox;
this mode has not been registered in CTest or CI. REV#151 remains open.


### Hosted Firefox follow-up, 2026-11-15

PR #136's first hosted run (43691323294) exposed two pending audio resumes
rejected during fast quit, plus a Minimal tab subprocess launch failure.
The next run (43692612875) coalesced the audio error to one, but showed that
checking the context state immediately was premature: Firefox can reject a
resume before its asynchronous close completes (Mozilla bug 2070740).
The backend now awaits its own close promise before accepting only that exact
cancellation, and preserves both errors if close also fails. Eight deferred
production-body fixtures and fresh wasm-debug AudioTests, AudioSampleTests
and the trusted S-to-Escape Firefox audio flow pass locally. This does not
establish the hosted result or listening acceptance.

The second hosted run also timed out while creating extra tabs for Minimal
and Collision. Both failed graceful shutdown; the launcher PID differed
from the actual browser PID, so launcher-only termination did not establish
child cleanup. The runner now reuses the sole initial about:blank context
of its newly owned session, verifying its canonical profile before controlling
it and refusing an unexpected startup context inventory.
On Windows, forced cleanup checks the live launcher's executable, unique
profile and unchanged creation time before terminating only that PID's tree.
A local forced-cleanup check verified the distinct launcher/browser and their
children were terminated. No sandbox preference, timeout, page assertion,
required test or failure verdict was relaxed. The initial local evidence is
Firefox 160; the final hosted Firefox 159 result is recorded below.


The third hosted run (43694319140) passed the audio page and both Minimal and
Collision page protocols. Those latter entries then failed cleanup: their
launchers exited normally while the ownership-query helper was still running,
but the runner waited for that helper and reported its timeout. Cleanup now
observes a graceful launcher exit during the query and cancels only the helper.
Its bounded deadline, ownership checks and failure propagation remain intact.

That run also exposed missing real document focus in the bundle, Components
and WebWindow sequences. Initial-tab activation before navigation was not proof
that the resulting document was focused. The harness activates after each
navigation (including bundle reloads), then waits for actual document.hasFocus
within its existing bounded execution. It never overrides that property or
dispatches synthetic focus. Fixtures cover delayed focus propagation and a
persistent-focus failure. The final hosted verification is recorded below.


A follow-up local focus experiment rejected startup-tab reuse as a solution:
the initial tab stayed unfocused after activate, navigation and native
window.focus, while a newly created foreground tab had real document focus.
The runner therefore creates a foreground test tab and closes the unused
startup tab, still verifying the owned profile and actual focus. No test uses
an unfocused startup document merely because rendering happened to work.


### Hosted browser verification, 2026-11-16

The published CI logs close REV#150's hosted execution gate. These three
independent runs passed all eight `Firefox*` entries with Firefox 159.0.1
(build 20261102144711) on Windows, after the intermediate audio-resume,
startup-tab, focus and cleanup failures described above:

| Run | Head | UTC date | Result |
|---|---|---|---|
| [43696022260](https://github.com/matt439/Labrador/actions/runs/43696022260/job/132756665669) | `7d3ff30c` | 2026-11-15 | 8/8 Firefox entries passed |
| [43798148301](https://github.com/matt439/Labrador/actions/runs/43798148301/job/133071656497) | `d776df5f` | 2026-11-16 | Scheduled master run, 8/8 passed |
| [43807435145](https://github.com/matt439/Labrador/actions/runs/43807435145/job/133101351900) | `61118c5a` | 2026-11-16 | Game-authored-material candidate, 8/8 passed |

Each run's six samples explicitly report WebGL 2.0 on
`ANGLE (Microsoft, Microsoft Basic Render Driver (0x0000008C) Direct3D11
vs_5_0 ps_5_0), or similar`, meeting the runner's WARP check. The bundle's
desktop play, forced context loss/restore, quit and replay and WebWindow's
input sequence passed. Each audio page passed; none of these three job logs
contains `Closed before resume completed`. That records the observed result,
not proof that no further race is possible. WARP, synthetic touch and the
input sequence do not establish physical-device or listening acceptance.

The same published workflow now reads pixels back in Safari 27.2 on the
hosted macOS runner. The test page itself reports
`Drawing on Apple Inc. / Apple GPU (hardware)`; this is the hosted device
identity, not acceptance on the owner's physical Mac:

| Run | Head | Pixel result |
|---|---|---|
| [43696022260](https://github.com/matt439/Labrador/actions/runs/43696022260/job/132758747254) | `7d3ff30c` | 71/71 cases, 76,276 assertions passed |
| [43807435145](https://github.com/matt439/Labrador/actions/runs/43807435145/job/133104077180) | `61118c5a` | 76/76 cases, 100,866 assertions passed |

Both pages exited zero without page complaints. The latter run's transferred
`RenderPixelTests.wasm` is 737,935 bytes, SHA-256
`152eddf4df5d3a1f5bfde30eb87219572306776a1cd980c293100e9d17d6db72`.
Its empty failure-diagnostics list is accompanied by the comparator's explicit
`passing_frame_details: "not emitted by comparator"`; no per-case drift
measurement can be inferred from it. The existing allowance of one channel
value was unchanged. REV#151 remains open for passing-frame drift reporting,
Safari rejected-frame extraction and WebWindow input, and Firefox's separate
pixel-coverage remainder recorded above.

REV#159's clean-week gate also remains open. A later
[failure in run 43550104143](https://github.com/matt439/Labrador/actions/runs/43550104143/job/132312634060)
at `edfe87e3`, 2026-11-14 03:19 UTC, left LineSweeper waiting for quit with
the canvas focused, the page visible and no page error. Its longest recorded
frame gap was 842 ms; a received ArrowDown was held for 598 ms and Enter
arrived, but `quit` stayed false. The failure must be investigated rather
than counting a week from the November 12 fix. Across all published job
attempts since that fix, twelve executed Safari jobs passed, one failed and
eleven were skipped. Seven executed jobs after the failure passed, most
recently [run 43809435219](https://github.com/matt439/Labrador/actions/runs/43809435219/job/133111393228)
at `2b0116cd`, finishing 2026-11-16 05:32 UTC. Skipped jobs supply no stability
evidence. These green jobs record successful sample and bundle runs, not a
clean week or a diagnosis of that failure.

### Safari sampled quit follow-up, 2026-11-16

The classic WebDriver runner now uses LineSweeper's existing development-only
sampled-input gate for its unchanged Escape, Down, Down, Down, Enter sequence.
Each phase still arrives through a genuine WebDriver key action. One real UI
update consumes that sampled level, and an acknowledged neutral release resets
menu repeat before the next down. The runner requires the actual pause-menu
rows 0, 1, 2 and 3, preserves the page's error policy and deadline, and requires
the genuine quit callback. It sends no synthetic DOM event and retries no
sampled key phase. Ordinary drawing and forced context loss/restoration precede
the gated sequence; the packaged bundle's play runs without this gate.

This addresses the duration-dependent quit sequence exposed by the November
14 failure without changing the game's repeat policy. The classic-WebDriver
fixtures feed trusted-action translations to the production handwritten
JavaScript bridge while running eighty catch-up updates before each reply;
they require the exact rows and final quit and reject a wrong row, missing
protocol or bridge error. This is local protocol evidence, not Safari or
physical-device acceptance. Hosted Safari qualification and a full clean week
after publication remain open as REV#159; sampled navigation does not qualify
ordinary held-key repeat or gameplay timing.

Local verification passed all 144 Node cases in the complete
`BrowserProtocolTests` command, including the classic-WebDriver fixtures,
and the citation and whitespace checks. No local geckodriver was available,
so the Firefox-through-classic-WebDriver runner check was not run. No new
Safari, hosted workflow or hardware session ran for this follow-up.

### Safari collision action-list follow-up, 2026-11-17

PR #140's Safari job `133662399035` at merge source `fdf171f4` passed the
pixel page (79 cases, 100,909 assertions), sampled LineSweeper and minimal.
Collision then received `+KeyO` but no `-KeyO` through three separate
WebDriver commands, with continued frames and canvas focus. This failure
does not supply clean-week stability evidence.

The collision runner now sends its unchanged O, O, Escape sequence as one
W3C action list with the existing pauses. It requires two actual,
release-separated O cycles before Escape and the first genuine `onQuit`.
The page hook preserves that first callback's key-log boundary; stale input,
an already-held test key, later events or a second quit cannot fill missing
edges. Escape release may follow quit. Held-key repeat is not counted as a
fresh press, and no collision key is retried. Sampled LineSweeper navigation,
context restoration, page complaints and deadlines retain their checks.

All 159 local Node browser-protocol fixtures pass, including delayed replies,
missing/reversed/wrong/stale edges, early quit and late error cases. This is
runner protocol evidence. Fresh hosted Safari execution and REV#159's
clean-week criterion remain open; no engine or sample source changed.

### Browser comparison and window checks, 2026-11-20

The browser comparator emits a record for each passing golden comparison with
the pixel count, differing pixel count, channel allowance and worst channel
delta. The Firefox and Safari runners include those records in their JSON
diagnostics alongside rejected comparisons. Analytic and size-only checks
still report through doctest. The allowance is one channel value in every
browser.

Local `wasm-release` built `RenderPixelTests` and `WebWindowTests` and
registered all 53 expected entries. The pixel module was 755,798 bytes
(`44dfda7e6cd460277856c3a4cc5df7605eb8c86b0bd3b85f731e52e47c8e981b`
SHA-256). Each browser variant passed 79 cases and 100,909 assertions:

| Local browser and renderer | Baseline | S3TC hidden | Through post |
|---|---|---|---|
| Chrome 157.0.8103.80, RTX 5080 through ANGLE D3D11 | 64 exact frames | 64 exact frames | 64 exact frames |
| Firefox 160.0.1, Microsoft Basic Render Driver D3D11 | 64 frames, 5 with drift of 1 | 64 frames, 5 with drift of 1 | 64 frames, 5 with drift of 1 |

Firefox's masked run observed S3TC absent on all 86 WebGL2 contexts.
Its other variants observed S3TC on all 86. Firefox had three BiDi
`browsingContext.getTree` timeouts before navigation across the initial
attempts. The table reports each passing run. Firefox native stderr also
contained context-loss messages outside the BiDi console.

For a local rejected-frame check, the classic WebDriver runner served a
disposable copy of the pixel page with one embedded golden substituted in
memory. Geckodriver 0.37.1 with Firefox 160.0.1 returned page exit 1 and
extracted one PNG before reporting failure. The record included 63 passing
goldens and a rejected comparison with 960 of 4,096 pixels past the allowance,
worst channel 255. The PNG decoded to the expected 64×64 opaque-black frame.
No checked-in golden was edited.

The classic WebDriver runner also passed all 57 WebWindow steps in that
local geckodriver configuration, including the fullscreen and cursor
subchecks. It delivers mouse, wheel, focus and keyboard input through
WebDriver, including key activation for fullscreen. Its DPR and touch
assertions use page fixtures. The touch assertions exercise the engine's
DOM listener. Desktop WebKit maps touch action sources to mouse sources, so
compatibility mouse generation is part of the real-touch qualification.
[WebKit automation source](https://github.com/WebKit/WebKit/blob/main/Source/WebKit/UIProcess/Automation/WebAutomationSession.cpp#L3266-L3275)
The window module SHA-256 was
`739a0381955c10b78253020911b69947d3603ede80ebbbccc693091383ac1424`.

The existing `wasm` job runs the three Firefox entries, with rejected frames
kept by variant. The existing `safari` job extracts rejected pixel frames
and drives the transferred WebWindow page. Hosted outcomes for the new
steps, Safari's passing-frame drift and any rejection artifact remain to be
recorded against the tested revision. REV#159 tracks the stability window.

### Hosted Safari stability, 2026-11-20

Reviewing each attempt after the collision action-list fix found five passing
Safari jobs, one failed job and two skipped jobs. The first master attempt at
`9ffe934e` passed pixel readback and all six sample pages. Collision logged
both O press/release cycles and Escape through quit. SafariDriver then closed
the connection on `POST /session` before the bundle page could navigate, with
empty driver output. The same source passed the bundle on attempt two.
[Failed attempt](https://github.com/matt439/Labrador/actions/runs/43981191139/job/133703585386),
[passing retry](https://github.com/matt439/Labrador/actions/runs/43981191139/job/133704706830).

The other four passes covered pixels, all six samples and the bundle. The
release master run skipped Safari on its first attempt after a Firefox failure.
The publication master run skipped it under the same-tree rule.
[First fix PR](https://github.com/matt439/Labrador/actions/runs/43974562599/job/133678425366),
[release PR](https://github.com/matt439/Labrador/actions/runs/44001464716/job/133770316079),
[release master retry](https://github.com/matt439/Labrador/actions/runs/44002989378/job/133779154106),
[publication PR](https://github.com/matt439/Labrador/actions/runs/44008804143/job/133793505060).

`REV#159` waits for a full clean week following the failed job. The earliest
review is 09:54 Brisbane time on November 24. Inspect every executed attempt
through the end of that window and investigate any recurrence during session
creation. Skipped jobs provide no stability evidence.
