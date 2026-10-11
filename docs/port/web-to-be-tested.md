# The web port — what is still to be tested

Everything in `web.md`'s milestones that can be written and checked on this
desktop has been, in headless Chrome (and headless Firefox for LineSweeper's
drawing), under every preset, with the pixel tests on the RTX 5080, WARP and
SwiftShader. What follows needs a person, hardware this machine does not have,
a browser it does not run, or a push. Tick a box in the commit that checks it,
and amend `web.md` §9 with the result.

## Needs a push (CI)

- [ ] The wasm job is green on the GitHub runner: the pixel pages on its
      SwiftShader, `RenderPixelTestsWithoutS3tc`, `WebWindowTests` and the five
      `*PageTests` (the runner's focus and full-screen behaviour in headless mode
      may differ from this desktop's).
- [ ] The null-backends step runs even when a page step fails, and the job
      still goes red.
- [ ] The x64 jobs' `RenderPixelTests` pass against the 17 regenerated text
      goldens and the three new frames on the runner's Direct3D adapter.
- [ ] The wasm job uploads the `linesweeper-web` artifact and its `BUILD.json`
      names the pushed commit.
- [ ] Optional: one run with `ALLOWED_CHANNEL_DRIFT` at 0 to measure the
      runner's adapter, so the allowance can come down from 8.

## Needs Safari (M3's done-when)

- [ ] LineSweeper, local_multiplayer, minimal, collision and the audio sample
      in desktop Safari: drawing, keys, quit, context loss and restore. The 2013
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

## Needs the deploy (M5, the owner's)

- [ ] Deploy the PersonalWebsite branch `linesweeper-demo` with
      `npm run deploy`, then `curl -I` the live `.wasm`: `application/wasm`,
      immutable `Cache-Control` under `m/<hash>/`, `no-cache` on the page, and
      whether `Content-Encoding: br` is served.
- [ ] Play the live page in desktop Chrome, Firefox and Safari: click to start,
      play, quit, play again.
- [ ] A real phone gets the recording and fetches no wasm; a tablet with a
      keyboard gets the Play button back after a key.
- [ ] A machine without WebGL2 shows the `onError` message.
- [ ] A real context loss (GPU reset, or a tab hidden while it is lost) shows
      and clears the page's message.
- [ ] Decide whether 4.3 MB of video is acceptable on phones, or swap in a
      smaller encode with `npm run sync-demo -- <bundle> --video <mp4>`.

## Firefox, which the page runner cannot drive

- [ ] `WebWindowTests` and the sample page tests in Firefox (the runner speaks
      CDP, Firefox only WebDriver BiDi): the minimal, collision, audio and
      local_multiplayer pages by hand, and the shell CSS.
