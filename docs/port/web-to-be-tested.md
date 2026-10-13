# The web port — what is still to be tested

Everything in `web.md`'s milestones that can be written and checked on this
desktop has been, in headless Chrome (and headless Firefox for LineSweeper's
drawing), under every preset, with the pixel tests on the RTX 5080, WARP and
SwiftShader. What follows needs a person, hardware this machine does not have,
a browser it does not run, or a push. Tick a box in the commit that checks it,
and amend `web.md` §9 with the result.

## Needs a push (CI)

- [x] The wasm job is green on the GitHub runner: the pixel pages on its
      SwiftShader, `RenderPixelTestsWithoutS3tc`, `WebWindowTests` and the five
      `*PageTests`. Run 38130728123, PR #1, every step passed.
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
`BUILD.json` names the Labrador commit it was built from, `e7888bd`, clean, at
`wasm-release`. The module folder is `m/4544f56a476f/`. Each box below was
checked against that URL on 2026-10-13 from this desktop, and a later deploy
needs them run again.

- [x] `curl -I` with `Accept-Encoding: br` against the page and the live
      `.wasm`: `application/wasm`, immutable `Cache-Control` under
      `m/<hash>/`, `no-cache` on the page, and `Content-Encoding: br` served
      for both. Headless Chrome, which also offers zstd, was answered with
      `Content-Encoding: zstd` on the same three files.

      ```
      $ curl -sI -H "Accept-Encoding: br" https://mattharding.dev/demo/linesweeper/
      HTTP/1.1 200 OK
      Content-Type: text/html
      CF-Cache-Status: HIT
      Cache-Control: no-cache
      Content-Encoding: br
      Server: cloudflare

      $ curl -sI -H "Accept-Encoding: br" https://mattharding.dev/demo/linesweeper/m/4544f56a476f/linesweeper.wasm
      HTTP/1.1 200 OK
      Content-Type: application/wasm
      CF-Cache-Status: HIT
      Cache-Control: public, max-age=31536000, immutable
      ETag: W/"ff680cab2f7adde2c53dc7d8ea8b38f3"
      Content-Encoding: br
      Server: cloudflare
      ```

      The loader, `m/4544f56a476f/linesweeper.js`, answers the same way with
      `Content-Type: text/javascript`. The lines left out above are the date,
      `Connection`, Cloudflare's reporting headers, `CF-RAY` and `alt-svc`.
- [ ] Play the live page in desktop Chrome, Firefox and Safari: click to start,
      play, quit, play again. **Chrome done, headless**: driven over the
      DevTools protocol, nothing but the page, its poster, its fonts, the
      site's favicon and Cloudflare's analytics beacon and its report was
      fetched before the click; the click fetched the loader and the module,
      the game
      drew and took arrow keys, rotate and Space (the frame was looked at),
      Escape, Down, Down and Enter reached `onQuit` and the page's "Thanks for
      playing.", and "Play again" mounted a fresh canvas that played and quit
      the same way, with no console message and no browser warning
      throughout. **Firefox waits on BDP-06** (the page runner speaks only
      CDP) and **Safari on SM-09**.
- [ ] A real phone gets the recording and fetches no wasm; a tablet with a
      keyboard gets the Play button back after a key. **Emulated half done**:
      under Chrome's touch emulation (5 touch points, a 390x844 mobile
      viewport), the page matched `(pointer: coarse)` and not
      `(any-pointer: fine)`, showed the recording with its status line, and
      the network log held no request for `linesweeper.js` or
      `linesweeper.wasm`; a key press then removed the video and brought back
      "Play LineSweeper", still without fetching either. This is emulation, not
      a phone: the real-phone half waits on **SM-13** (an iPhone; the Android
      half is SM-44).
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
      half with a forced loss.
- [ ] Decide whether 4.3 MB of video is acceptable on phones, or swap in a
      smaller encode with `npm run sync-demo -- <bundle> --video <mp4>`.
      **The owner's decision, open as BDP-20.** What the emulated phone run
      saw: the recording autoplays, and the browser began fetching it at once
      (`linesweeper.mp4`, answered 206 with a byte range).

## Firefox, which the page runner cannot drive

- [ ] `WebWindowTests` and the sample page tests in Firefox (the runner speaks
      CDP, Firefox only WebDriver BiDi): the minimal, collision, audio and
      local_multiplayer pages by hand, and the shell CSS.
