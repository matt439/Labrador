# The web port as built: what each milestone measured

[web.md](web.md) owns the plan, its decisions, the host page's contract, each
milestone's done-when and the summary of where the port stands;
[web-to-be-tested.md](web-to-be-tested.md) owns the checks still open. This
file records what each landing measured, in the order the work landed, each
entry as it was written when it did. An entry is amended only by a dated
addition to the milestone it concerns, never rewritten. D-, P-, S-, M- and
R-labels refer to [web.md §8](web.md#8-decisions)'s decisions and
[web.md §9](web.md#9-milestones-and-what-to-spike-first)'s spikes, milestones
and risks; "its row" is a milestone's row in web.md's
[milestone table](web.md#milestones).

**S1, S2, M1 and M2 are done. S3 was not run as a throwaway; it was absorbed
into M3, whose code has landed and which is met in two of the three browsers
its row names.** Safari has not been checked and R5 has not been measured, so
M3 is not called done. What they measured, against the guesses this plan
made:

- **S1 was hours, not a milestone.** Emscripten 6.0.12, every portable `.cpp`
  in `engine/` and LineSweeper under `-Wall -Wextra -Wpedantic`: 61 distinct
  warning sites of seven kinds, 49 of them one pattern (a definition qualified
  with its own namespace inside that namespace's block). The full build found
  more of the second-largest kind than the sweep did, because clang reports an
  implicit copy assignment only where one is used: thirteen types in all had a
  defaulted copy constructor and no declared assignment. Two
  `-Wtautological-overlap-compare` hits in `ericson_math.cpp` were the NaN
  rejections, deliberate and correct, and are `std::isnan` now. One
  `-Wunused-private-field` was a real dead member in LineSweeper's board view.
  Two errors were real portability bugs rather than platform files:
  `_wfopen_s` in the JSON reader and `std::exception("...")`, which only MSVC
  accepts.
- **S2 passed.** `LineSweeperTests` replays its scripted ticks and compares
  worlds with `std::memcmp`, and all of it holds under clang on WebAssembly, so
  R3 is retired for the rules. All thirteen ctest entries pass under Node.
- **Three things nobody predicted**, all fixed where they belong. wasm32 has a
  32-bit `size_t`, and one `texture_level` test asserted a 4 GiB level was
  representable, which it is only on 64-bit. Emscripten's stack is 64 KiB
  against Windows' 1 MiB, and the particle field's tests and the JSON reader's
  buffer were sized against the larger one, so the browser build links with
  the same 1 MiB. And `content_root.cpp` joined paths with a literal `'\\'`,
  which is not a separator on a POSIX file system.
- **D4 was measured as it asked.** MSVC's `steady_clock` is QPC converted to
  nanoseconds. Over 600 frames of 16.7 ms, every delta agreed with a direct QPC
  call to within 2 of StepTimer's 100 ns ticks, which is the gap between the
  two reads.
- **`widen` has one implementation, checked against the call it replaced.** A
  million random byte strings weighted towards the boundary bytes agree with
  `MultiByteToWideChar(CP_UTF8)` exactly. That needed one departure from the
  Unicode Standard's maximal subparts, which Windows makes and which the
  first draft did not: a continuation byte refused by the narrowed range after
  E0, ED, F0 or F4 goes with its lead.

**M1 departed from [its row in web.md's milestones](web.md#milestones) in two places, and both have closed.** The
`wasm-*` presets built `render/null/` rather than `gl/`, because `gl/` had only
a WGL context, until M3 gave it a second one and switched them to `gl/`. And
`AppTests` was not in the browser build at all rather than split, because
`application.h` included `<Windows.h>` until D2 landed in M2.
`input/web/gamepad_reader.cpp` exists a milestone early, because the
input-backend variable needed something to select.

**M4's code has landed, and a pad in a person's hands has not played it.**
`sample()` asks the browser once a frame rather than once a slot, slots are
compacted and stable, a non-standard pad is absent, and the canvas losing the
keyboard no longer suspends the pads: on the web they follow the page's
visibility ([web.md §3.4](web.md#34-input--the-pad-seam-holds-and-the-fed-devices-stay-fed)). That last one was a bug a pad-only player could not get out
of, because the canvas blurring suspended the reader and nothing a pad does
gives the canvas focus back. `Application` now keeps what the window has said
as three facts - foreground, shown, has the keyboard - and derives the pads,
the keys and the states' activation from them. On Windows the third never
changes; the one difference there is that a window restored without being
activated no longer takes the keyboard, which it never had. The fullscreen
flag follows the browser too. Checked in headless Chrome against
LineSweeper's page, with a script standing in for the Gamepad API: a standard
pad at index 5, with the canvas blurred, moved the piece through slot 0 and
opened the pause menu with Start, where `master` did nothing; with the page
faked hidden it did nothing, and shown again it played; keys did nothing with
the canvas blurred and played once it had focus. A request granted inside a
click set the flag and the canvas full screen, `document.exitFullscreen()`
cleared it, a `fullscreenerror` cleared it, and a page with
`fullscreenEnabled` false refused on the spot. Headless Chrome does not leave
full screen on a dispatched Escape, so Escape itself is a person's check, as
are a physical pad on the standard mapping, Firefox, and Safari.

Closing the render half moved some coverage. The test sources gated on the
null backend - `RenderTests`' `null_tests.cpp` and `null_resource_tests.cpp`,
`SceneTests`' `fanout_tests.cpp` and `cull_tests.cpp`, `LineSweeperViewTests`'
`particle_draw_tests.cpp`, `LocalMultiplayerTests`' `draw_tests.cpp` and the
null benchmark - drop out of executables that still exist, so the `wasm-*`
presets still counted 14 ctest entries and compile none of those files
*(17 since M6 registers `RenderPixelTests`, `RenderContextLossTests` and
`WebWindowTests` where a Chromium is found; below)*. MSVC
builds them under the two `-null` presets as before, and CI's `wasm-null` job
configures `wasm-release` a second time with `LABRADOR_RENDER_BACKEND=null`
and runs that build's tests too, so the second compiler still sees them.

**M2 met its row: LineSweeper reaches its frames in a browser and takes keys,
and the Windows samples behave as they did.** D2 and D3 are as decided:
`initialize()` takes nothing, `window()` is a `void*`, `app/window.h` is
neutral and `app/win32/` and `app/web/` each define its `Impl`. What it found:

- **[web.md §4](web.md#4-the-proposed-web-platform-layer)'s frame loop premise was false.** Simulating an infinite loop unwinds
  the stack by throwing, and with `-fwasm-exceptions` the unwinding runs
  destructors. A probe with a local in `main` printed its destructor before
  the first tick and then ticked three times on the destroyed object; the
  same probe under Emscripten's JavaScript exceptions did not, which is
  presumably where the documented behaviour comes from. So a web `main`
  keeps its `Application` in a static (`samples/linesweeper/main_web.cpp`),
  `Application::run` says so, and the web window reports the mistake rather
  than letting the loop call into freed memory: built with the local and
  loaded in Chrome, the page's `onError` receives the reason.
- **An exception out of a frame has no caller to reach**, because main's
  catch is no longer on the stack. The web window catches at its edge - the
  frame and every html5 callback - stops the loop and reports to stderr and
  `Module.onError`. That brought `onQuit` and `onError` forward from M5, which
  keeps the module packaging, the host page and the deploy.
- **D2 met three callers that hid their window**, and D6 is how they still
  do. D7 is the other choice it made.
- **The drawing buffer is measured, not observed**: CSS box times
  `devicePixelRatio`, once a frame before the tick, rather than a
  `ResizeObserver`. It is one code path, and it sees a zoom and a move to a
  denser display, which no layout event announces. The page must size the
  canvas in CSS, since a canvas at its default layout is as big as its buffer.
- **The keyboard keeps every unmodified key but Tab and the function keys**,
  so the arrows and Space do not scroll the page and Firefox's find bar does
  not open, and gives every chord back to the browser. Text is keydown's
  `key` when it is one code point and no command modifier is down; AltGr is
  text. The mouse is pressed on the canvas and followed on the page's window,
  which is `SetCapture`'s analogue. The wheel is the game's only while the
  canvas has focus, so a visitor scrolling past the demo is not trapped.
- **`AppTests` is in the browser build**: 14 ctest entries under Node, with
  the DOM translation pinned in place of the Win32 window cases. Two of
  `content_root_tests.cpp`'s cases do not ask Node to resolve a temporary
  path, because under `NODERAWFS` a Windows host's temporary directory is a
  drive path and no POSIX build counts it absolute.
- **Checked**: `x64-debug`, `-null`, `-gl`, `-d3d12`, `-vulkan`,
  `x64-release`, `wasm-debug` and `wasm-release`, every ctest entry green;
  headless Chrome loading the build's page, counting animation frames,
  following a resize at ratio 1 and 2 (the buffer followed; the layout did not
  hear of it, which M3 found and fixed, below), and taking Escape, Down, Down, Enter
  through the pause menu to the page's `onQuit`, after which the loop stops;
  and the Win32 LineSweeper and minimal samples on screen at 1280x720 client
  pixels, the pause menu opening on Escape and both exiting 0.

What M2 could not show was a frame, because the browser build drew with null.
Its size number was taken without a renderer: at `wasm-release` the module was
343 KB and its loader 85 KB, 109 KB and 20 KB under Brotli.

**M3 draws LineSweeper in Chrome, Firefox and Safari, and R5 is owed.** D1 is as
decided: `gl/` holds the three translation units every backend has and two
context files, `wgl_context.cpp` (the WGL loader and context, which was
`gl_functions.cpp`) and `web_context.cpp`, and `engine/CMakeLists.txt`
compiles exactly one of the two, so a build compiles four. `Renderer::Impl`
holds the context, and the context supplies what the two disagree about: the
shader prelude, whether S3TC is there, whether the context is a software one,
and the vendor and renderer names. What it met and where it departed:

- **The three GLES gaps are closed on both contexts with one path**, not per
  context, so that the browser's drawing path is the one `RenderPixelTests`
  checks on the desktop. Each run re-points the three `glVertexAttribPointer`
  offsets at its first vertex and calls `glDrawElements`, so
  `glDrawElementsBaseVertex` is gone from both and the loader's list is one
  shorter. `b8g8r8a8` is swizzled to RGBA on the CPU at upload, so `GL_BGRA`
  is gone. S3TC is asked of each context under its own name:
  `GL_EXT_texture_compression_s3tc` on WGL, `WEBGL_compressed_texture_s3tc` in
  a browser.
- **highp, not the mediump [web.md §3.3](web.md#33-the-gl-backend-against-webgl2) first proposed.** A GLES fragment shader has no
  default float precision, and at mediump, which may be half precision, an
  atlas coordinate cannot name every texel of a large atlas; a fragment
  sampler defaults to lowp. So the ES prelude declares `precision highp float`
  and `precision highp sampler2D`, which GLES 3.0 requires a fragment shader to
  support.
- **S3's two questions do not arise.** The context is made with
  `enableExtensionsByDefault = false`, and the extensions the backend uses are
  enabled by name through `emscripten_webgl_enable_extension`, so the list is
  the code's rather than Emscripten's. `has_gl_extension` is not asked in a
  browser, and what Emscripten's `glGetString` would call S3TC does not
  matter. `WEBGL_compressed_texture_s3tc` was present in both desktop browsers
  checked. The vendor and renderer names are the context's own where they
  name an adapter, and are unmasked through `WEBGL_debug_renderer_info` only
  where the plain `RENDERER` is the generic `WebKit WebGL` (Chrome and
  Safari); Firefox names a sanitised adapter there and deprecates the
  extension, so it is never asked for it.
- **Presentation is the browser's.** `present()` is `SwapBuffers` on WGL and
  nothing in a browser, which composites when the animation-frame callback
  returns. `device_info.present_mode` is `"browser_animation_frame"`, with no
  swap interval to ask for. A lost WebGL2 context is not reported: nothing
  listens for `webglcontextlost`, `DeviceNotify` is not called, and restoring
  it stays in M6. *(M6 has since restored it; below.)*
- **It exposed an M2 defect, fixed in the same change.** M2's frame measured
  the canvas's buffer and told nobody, which null could not show. Once WebGL
  drew, the layout stayed at its old size after a page resize, a zoom or a
  change of `devicePixelRatio`, and in Firefox a first buffer of 1361x766 left
  the board laid out for 1280x720. The web window now reports a measured
  change through `on_window_size_changed` before the tick, so the layout and
  the renderer hear of a new buffer before anything is drawn into it.
- **`RenderPixelTests` is not built for the browser**, because Node has no
  WebGL; running it in a headless browser is M6. The coverage this moved is
  in the paragraph on M1's departures above. *(M6 has since built it as a
  page; below.)*
- **Checked**: `x64-debug-gl` and `x64-release-gl`, 17 of 17 ctest entries,
  with `RenderPixelTests` byte-identical to the golden PNGs, so the swizzle and
  the re-pointed attributes draw the same pixels; `x64-debug`, 17 of 17;
  `wasm-debug` and `wasm-release`, 14 of 14 under Node. Headless Chrome (ANGLE
  over Direct3D 11 on the machine's RTX 5080, with no flags) and headless
  Firefox 157 (driven over WebDriver BiDi; its renderer string is generic, so
  its adapter was not read) both draw LineSweeper: the board, pieces, ghost, NEXT and HOLD,
  legible text from the BC2 font atlas, and the pause menu. Gameplay keys
  change the frame, Escape, Down, Down, Enter reaches `Module.onQuit`, and
  neither console shows an error. Firefox warns that legacy wasm exception
  handling's `try` is deprecated in favour of `try_table`. It warned that
  `WEBGL_debug_renderer_info` is deprecated too, until the extension was
  enabled only where `RENDERER` is masked: with Firefox's console sent to
  stdout, that warning appears at creation and again after a rehearsed
  loss and restore when the extension is forced, and not at all without it,
  while the device's renderer still reads as a named adapter in both
  browsers - ANGLE on the RTX 5080 in Chrome, and in Firefox its sanitised
  bucket, `ANGLE (NVIDIA, NVIDIA GeForce GTX 980 ...), or similar`. The Win32 GL
  LineSweeper and minimal samples were looked at on screen: no channel-swap
  tint, legible text, the pause menu, and exit 0.
- **Safari.** CI's `safari` job drives the six sample pages and the bundle page
  in desktop Safari through safaridriver on GitHub's hosted macOS runner
  ([run_safari.mjs](../../tests/render/web/run_safari.mjs)). Its first run,
  on 2026-11-05 in Safari 26.6.2, drew every page, heard a forced context loss
  and restoration and drew again, took each page's keys and quit, and played,
  quit and replayed the bundle; `RENDERER` read `WebKit WebGL` and the
  extension unmasked `Apple GPU`. The
  [Safari section](web-to-be-tested.md#safari-sample-checks-web1) of the
  to-be-tested list has the run and what it does not prove. The owner's
  current MacBook Air runs macOS 27.0.1 and Safari 27.0.1 (About Safari, read
  by the owner on 2026-11-03), above the 18.4 that dropping legacy wasm
  exceptions needs; the live demo loaded and was played briefly there that day.
- **Owed before M3 is done.** The owner's informal play of the live demo on
  the Air (SM-09). And R5: headless Chrome gave 181
  animation-frame callbacks in 3 s, a median of 16.7 ms between them (14.7 to
  16.9), which is a headless measurement and not the judder measurement on a
  60 Hz and a high-refresh display that R5 asks for.
- **Size, with `gl/` drawing**, at `wasm-release`: the module is 356,750 bytes
  (113,085 under Brotli) and its loader 107,466 (25,187), so WebGL adds about
  4 KB to the module and 5 KB to the loader under Brotli. The module is below
  [web.md §7.1](web.md#71-files)'s estimate and the loader inside it.

**WEB#4 follow-up, 2026-10-28:** a separate Firefox BiDi runner now handles
minimal, collision, audio, local_multiplayer and components, and
`WebWindowTests`' input sequence, with its offline transport, control and
input-translation cases in the existing `BrowserProtocolTests` entry. The
[method and results](web-to-be-tested.md#firefox-sample-checks-web4) own
what the runs prove. The offline cases are runner verification, not a Firefox
rendering, listening or device result. On 2026-10-28 all five pages, built at
commit `0fcf3a7`, passed that runner in headless Firefox 158.0.1, and so did
`WebWindowTests`' 39 steps, with BiDi's own input in place of the DevTools
protocol's. Touch cancellation is the one step BiDi cannot send.

**WEB#10, 2026-11-05:** the runner adds LineSweeper's development page and
its bundle page's play, forced context loss, quit and replay, and configure
registers all eight runs as `Firefox*` ctest entries where it finds a
Firefox. All eight passed in headless Firefox 159.0.1 on the desktop. CI's
wasm job runs only the window's entry, because the runner image's Firefox
makes no WebGL2 context without a GPU; the
[record](web-to-be-tested.md#firefox-sample-checks-web4) gives the counts and
what the bundle run leaves to Chromium.

**M6's first item is done: `RenderPixelTests` runs in headless Chrome
against the same golden set, and on this machine it is byte-identical to
it.** All 45 cases and 4,532 assertions pass in `wasm-debug` and
`wasm-release`, drawn by ANGLE over Direct3D 11 on the RTX 5080, and run once
more with `allowed_channel_drift` at zero, not one frame differed by a single
level. So the ES dialect, the highp prelude and the browser's S3TC decode
meet the pixel contract exactly, not just within the allowance. A
deliberately wrong blend factor fails the blend and text cases and leaves
their frames in `golden-actual/` beside the page, as on Windows. What it took:

- **The page is the test.** `tests/render/web/shell.html` holds no canvas and
  settles `window.labradorExit` with `main`'s exit code (`EXIT_RUNTIME`).
  `tests/render/web/run_page.mjs` is ctest's command: it serves the folder on
  a loopback port, starts Chrome headless with a profile of its own, drives it
  over the DevTools protocol with Node's own `WebSocket` (no package to
  install or pin), relays the console, waits on the exit, copies
  `/golden-actual` out of the page's memory, and prints the WebGL2 renderer
  it drew with. Every case runs inside one synchronous `main()`, which is why
  `read_back_buffer` sees a frame that a canvas without
  `preserveDrawingBuffer` discards when the page composites.
- **One canvas per case, not one per run.** A canvas keeps one WebGL context
  for its whole life, so a second device on it would inherit the first's
  state where each desktop case starts on a context nobody has touched.
  `tests/render/test_window.h` is the seam, with a Win32 and a web file
  compiled as the engine's platform folders are. Destroying an Emscripten
  context only forgets its handle, so the web window gives each one back
  through `WEBGL_lose_context` before removing its canvas. Without that,
  Chrome evicts the oldest live context past sixteen and warns once per case.
- **The golden set is read through lodepng on every platform** ([web.md §8](web.md#8-decisions), D8).
  A regeneration now leaves an image whose pixels did not move untouched, so
  the codec change rewrote nothing: `LABRADOR_GOLDEN_DUMP=1` under
  `x64-debug-gl` left all 57 files byte-identical. A regeneration cannot run
  in a browser, which cannot see the shell's environment and keeps what it
  writes in memory, so the set is regenerated on the desktop and the browser
  is held to it.
- **Absent rather than skipped without a browser.** The entry is registered
  only where configure finds a Chromium (`LABRADOR_TEST_BROWSER`), so the
  `wasm-*` presets count 25 entries here, with the context-loss page and the
  window page below, the no-S3TC run of this one (M7b), `AudioSampleTests`
  (M6), `CollisionSampleTests` and the five sample pages below, and 16 on a
  machine without one.
- **The device-info case has a browser branch**: `present_mode` is
  `browser_animation_frame` with no swap interval, where WGL reports one.
- **CI's Chrome draws with SwiftShader**, which its log names: "ANGLE
  (Google, Vulkan 1.3.0 (SwiftShader Device (Subzero) (0x0000C0DE)),
  SwiftShader driver)". It did not stay inside the allowance at first: two
  text frames missed by 9 against 8, in green alone, because SwiftShader
  rounds the interpolated colours of the font's BC2 atlas differently from
  the GPU the set was made on, and so did WARP, by 7. Measured with the
  allowance at zero, that rounding was all SwiftShader needed one for, and
  all of WARP's but one level on two blend frames. So `RenderPixelTests` decodes the atlas itself before a
  device sees it, and a new case holds the device's own BC2 decode to a block
  every decoder must agree on. Forced onto SwiftShader
  (`--use-angle=swiftshader`), Chrome on this machine now writes all 58
  frames byte for byte, as ANGLE over the RTX 5080 does; WARP
  (`--use-angle=d3d11-warp`) differs on two blend frames by 1.
  `golden_image.cpp` holds the measurement. The null backends now build and
  test in CI's separate `wasm-null` job, which runs whatever the page steps
  say: a deliberately failed sample page on a throwaway branch left
  `wasm-null` running all 28 of its tests and turned both jobs red
  ([web-to-be-tested.md](web-to-be-tested.md)).

**M6's second item is done: a lost WebGL2 context is restored, and the shell
hears it.** `RenderContextLossTests` pins it in headless Chrome, and
LineSweeper was played through one: the board, the pieces and the text drew
before the loss, the canvas was blank while it was lost, and after the
restore all of it drew again from the reloaded textures and font atlas, keys
still played, and neither `onError` nor a WebGL warning appeared. It departs
from [web.md §4](web.md#4-the-proposed-web-platform-layer)'s sketch in two places:

- **No overlay, and no report to the shell at the moment of loss.** A
  browser puts frames between the loss and the restore, sometimes many, and
  no backend before this one ever did: d3d11, d3d12 and vulkan each lose, rebuild and restore
  inside the call that found the loss. A shell that released its textures
  on the loss event would throw from the next draw that named one. So
  `DeviceNotify` hears both halves together, `on_device_lost` then
  `on_device_restored`, from the `begin_frame` after the browser hands the
  context back, and the gap is `gl/`'s to carry: frames are recorded and
  dropped, `read_back_buffer` throws rather than hand back an empty 0x0 frame, and no GL
  call is made, because a browser warns about every one. The caller
  contract is the one every other backend already gives, which is why
  nothing above the seam changed.
- **The lost callback asks for the context back, and nothing listens for
  the restore.** Returning true from it is what makes Emscripten call
  `preventDefault`, without which a browser never restores. Whether the
  context is lost is something it can be asked at any moment, so
  `begin_frame` asks. Calling into the shell from the event itself would
  run engine code between frames, where an exception has no catch to reach.

What a restore has to do was settled by asking Chrome, not by reading about
it. While a context is lost, `create*` hands out objects rather than null,
so nothing throws, and the drawing buffer reads 0x0. After the restore, an
object from before it is not the new context's: `isTexture` answers false
quietly and `deleteTexture` raises `INVALID_OPERATION` with a console
warning. So the renderer forgets its own names rather than deleting them,
`GlTexture` deletes only a name `glIsTexture` owns, and the extensions are
enabled again on the same handle. The device strings are asked of the
context itself rather than through `glGetString`, which Emscripten answers from
a cache it keeps for the whole page. A context lost again while it is being
rebuilt is waited out a second time, with the shell told nothing until a
rebuild holds. Keeping the handle is also what keeps the
window's input: destroying a context in Emscripten removes every handler on
its canvas. `RenderContextLossTests` fails on any warning the browser logs
(`run_page.mjs --browser-warnings fail`), because the stale delete's error
is drained by the next upload and the warning is the only place it is still
visible. With the `glIsTexture` guard taken out, it fails on exactly that.

**The host page hears both ends of a loss**, which the shell by design does
not: `Module.onContextLost` from the first `begin_frame` after the lost
event, and `Module.onContextRestored` once the rebuild holds and the shell has
heard both of its halves ([web.md §7.3](web.md#73-mounting-it-start-on-click)). A context that is never restored otherwise
left a blank canvas and no word, and a page holds nothing a frame names, so
telling it early costs the caller contract nothing. Both are called from a
frame, because a page's hook is the page's code and the event is between
frames; a loss and a restore that fall between the same two frames still
reach the page as two calls. A tab in the background draws no frames, so it
hears of the loss when it is shown. `RenderContextLossTests` installs both
hooks and checks that each is called once, in order, inside a frame, with
`onContextRestored` by the end of the frame that restored the shell and
nothing after; calling the first from the lost callback instead fails it.
LineSweeper's development page puts a message in its status line on the
first and takes it back on the second. Played in headless Chrome at
`wasm-debug` and `wasm-release` through a `WEBGL_lose_context` loss and
restore, the message was up over the blank canvas, then gone with the board
drawing again and keys moving the piece.

**`local_multiplayer` is the second sample in a browser** (M6), and it
needed no engine change. It has a `main_web.cpp` beside `main.cpp`, chosen
in its `CMakeLists.txt` as LineSweeper's is, and a development page. The
three things the Windows build copies beside its executable are embedded at
the same relative paths, so one manifest serves both builds. The page keeps
`max_threads = min(2, default_thread_count())`. A browser reports
`navigator.hardwareConcurrency` there, so on any machine with two cores the
partitioner cuts the two views into two slices. Each slice runs inside
`add_task` in turn, which is [web.md §3.6](web.md#36-threads--single-threaded-and-linesweeper-uses-none)'s serial pool doing the fan-out it was
written for. A `wasm-release` module is 314 KB and its loader 105 KB. It was
played in headless Chrome at `wasm-debug` and `wasm-release`. Both panes drew
both players, and WASD and the arrows moved one camera each in the same
frames. Arrow keys did not scroll the page. A `WEBGL_lose_context` loss and
restore came back drawing. A resize of the CSS box changed the drawing
buffer, and unlike LineSweeper this sample also re-laid out both panes,
because it rebuilds its views every tick. Escape reached `Module.onQuit`.
Headless Firefox has driven this page through the
[BiDi runner](web-to-be-tested.md#firefox-sample-checks-web4) (draw calls,
keys, quit; no pixels looked at). Safari has not been checked for this
sample. Neither has a physical pad, for the same reason M4 is open.

**M7b's texture item is done: a context without S3TC decodes the blocks on
the CPU instead of refusing them.** `engine/render/block_decode.h` turns BC1,
BC2 and BC3 into r8g8b8a8, every level, with each interpolated colour and
alpha rounded to nearest, and `gl/texture_factory.cpp` calls it when the
context reports no S3TC, on either context, and uploads the result as it
would an uncompressed file. The other backends keep native BC. It is engine
code because how a fringe texel rounds is the term the SwiftShader
measurement above found devices disagreeing on, so every device that falls
back draws the same pixels. The cost is memory: four times the bytes for BC2
and BC3, eight for BC1, on the devices with the least. What was checked:

- **Headlessly, in `RenderTests`**, against blocks built by hand with every
  expected texel worked out beside it: BC1's four-colour mode and its
  three-colour-and-transparent mode (equal endpoints included), BC2's
  explicit alpha under a colour block that would be three-colour as BC1,
  BC3's eight-alpha and six-alpha modes, a 5x3 level, a four-level mip chain,
  the shipped font atlas, and the four refusals.
- **The pixel tests' harness now draws its text from this decoder**, in place
  of the test-only one the SwiftShader fix wrote. They round the same way:
  `LABRADOR_GOLDEN_DUMP=1` under `x64-debug` moved none of the 58 images.
- **`RenderPixelTestsWithoutS3tc`** is a second ctest entry in the `wasm-*`
  presets: the same page and golden set, with `run_page.mjs
  --hide-extension WEBGL_compressed_texture_s3tc` making every context the
  page creates answer as a phone's browser does. All 45 cases pass against
  the native upload's images. The run fails if a context made afterwards
  still offers the extension. Swapping red and blue in the decoded upload
  fails its BC2 case and leaves the native entry green, so the decoded path
  is the one it draws. There is no desktop switch: WGL always has S3TC, and
  the GL path below the context is the same one.
- **LineSweeper, with the extension hidden**, in headless Chrome at
  `wasm-debug`: the board, the pieces, every label from the BC2 atlas and the
  pause menu drew legibly, keys moved the piece, and the only console error
  was the development server's missing favicon. No phone has run it.

**The one drawing difference `gl/` had from the other backends was not
`gl/`'s.** `tools/linesweeper_capture/README.md` recorded GL drawing two
centred text rows differently, the pause menu's RESTART and the top-out
banner, by up to 240 in a channel. Both were centred by halving an odd
measured width, so both sat exactly half a pixel off a whole one, where every
pixel centre is a texel edge and the point filter's choice is the
rasteriser's rounding. None of the four backends drew them cleanly. Moving
them a quarter pixel made GL and D3D11 agree byte for byte, which settled the
cause. The sample now rounds both positions down. All four backends write the
same four capture images, and the two website images that moved were looked
at. `renderer.h` tells a caller to do the same. A new pixel case,
"point-sampled text off a whole pixel lands on the nearest one", pins
quarter and three-quarter positions in both axes against a draw on the
nearest whole pixel. That makes sixty golden frames.
Measured at zero allowance, all sixty are byte for byte on this machine's GPU
through D3D11, D3D12, GL, Vulkan and Chrome's ANGLE, and on SwiftShader. WARP
still differs only on the two blend frames, by 1. The same case at x + 0.5
fails on both D3D11 and GL, which is why it pins neither.

**The audio container is decided and read** (D9). `engine/audio/xwb_file.h` reads an XACT 3 wave bank into
16-bit interleaved samples per wave, with names, rates and loop regions:
PCM at 8 and 16 bits, and MS-ADPCM decoded by the engine. `AudioTests` reads
`samples/audio`'s real bank and matches each wave to its source WAV sample
for sample, in every configuration and under Node, where NODERAWFS reads the
copies beside the test. An ADPCM fixture made from one of those WAVs by
FFmpeg's encoder decodes within the codec's error of its source, and to the
same samples Windows' own MS-ADPCM codec (msacm32) and FFmpeg's decoder
produce: those two were compared in full outside the tree, mono, stereo and
a clipping encode, and a hash of the first pins it in the test. Every refusal
has a hand-built bank and a named throw.

**M6's last item is done: `audio/webaudio/` plays it.** It is the seam's third
backend, one translation unit like the other two, and the default under
Emscripten, so both `wasm-*` presets build it; CI's second wasm
configuration now asks for null audio as well as the null renderer, so
`AudioTests`' recording cases still compile under clang. Each wave becomes an
`AudioBuffer` at load. A voice is a source node through a gain and a stereo
panner, with DirectXTK's pitch (two to the power of the pitch) and DirectXTK's
state transitions kept on the C++ side; pause, which Web Audio does not have,
is a recorded position and a new node. The `AudioContext` is made inside the
first key, click or touch the page receives, by capture listeners on `window`
that belong to the backend rather than to the engine's window, so the seam
grows no verb that only a browser needs; a device that opens no bank never
makes one, and LineSweeper, which has no sound, makes none. Before that
gesture a one-shot is dropped and a voice that was asked to play starts when
it comes, which `audio_device.h` states as the caller's contract. Under Node
there is no Web Audio: banks are read and checked and voices change state,
and nothing sounds, which is what `AudioTests` runs there as
`webaudio_tests.cpp`. In headless Chrome, at `wasm-debug` and `wasm-release`,
a scratch page tapped the context's output with an `AnalyserNode` and drove
the backend: no context before a synthetic key press and a `running` one
after it; the looping drone's 220 Hz present while playing and absent while
paused; a pitch of +1 moving it to 440 Hz; a pan of -1 lowering the level by
the square root of two; a non-immediate stop letting the pass finish about
0.7 s later and the state then reading stopped; a paused chime resuming on the
note it was paused in, at half rate; a one-shot audible after the gesture; and
the context suspended and resumed with the device. Nobody has listened to it.
Firefox has run it only under the audio sample's page, where nothing measured
its output (below), and Safari and a phone have not run it.

**`samples/audio` is the third sample in a browser**, on `audio/webaudio/`,
and it needed no engine change either. It has a `main_web.cpp` and a
development page, chosen in its `CMakeLists.txt` as the other two are, and its
manifest, its bank and the font are embedded at the paths the Windows build
copies them to; at 70 KB the bank is far below the megabyte at which [web.md §5](web.md#5-build-integration) would
fetch content instead. The page says that nothing sounds until the first key,
click or touch, and so does the sample's own screen, where the Windows build
names XAudio2. `--smoke-test` stays a Windows switch, because a page has no
command line. Its lifecycle, which is the packaged manifest's required bank
loaded through the engine's loader, both one-shots, then the drone played,
paused, resumed and stopped, is an `AudioSampleTests` case compiled for
`webaudio`, and `AudioSampleTests` now runs under Node in both `wasm-*`
presets, reading the content where it is checked in through NODERAWFS. That
makes 18 entries there. In headless Chrome, at `wasm-debug` and
`wasm-release`, keys 1, 2, 3, Space, S and Escape were sent to the page, with
an `AnalyserNode` on the context's output. There was no context before the
first key and a `running` one after it. The first key's ping was audible, so
the gesture that allows sound is not spent on it. Both one-shots peaked, the
drone sounded while playing and was silent while paused and after S, and the
screen read playing, paused and stopped in turn. Escape reached
`Module.onQuit`. The console logged nothing but the scratch server's missing
favicon. Nobody has listened to it. Headless Firefox has driven the page
through the [BiDi runner](web-to-be-tested.md#firefox-sample-checks-web4)
(draw calls, keys, quit; no pixels, no listening), and Safari and a phone
have not run it.

**M7a's code has landed: touch is the third fed device, and no glass has
touched it.** `input/touch.h` holds up to ten points, each with an engine id
that is never reused, so a platform id handed to the next finger between two
frames reads as a release and a press. A point down and up between two polls
is still seen for one frame, because a tap is shorter than a frame often
enough to matter. Edges need the device live on both frames. Cancel and focus
loss remove points without a release, and an eleventh finger is not seen for
its whole contact. Touch follows the foreground and the page's visibility, as
the pads do, not the canvas's keyboard focus, because a finger reaches the
canvas only by touching it. `app/win32/` handles `WM_POINTER` for `PT_TOUCH`
without `DefWindowProc`, so Windows makes no mouse messages of it, and also
drops any mouse message carrying the touch signature. A pen falls through and
stays the mouse. `app/web/` listens to pointer events on the canvas through
`EM_JS`, because the html5 API has none, rather than taking its touch
callbacks: a pointer event is one contact with one id, the shape `WM_POINTER`
already has. It prevents a touch's default, which
stops the browser making mouse events of it, and focuses the canvas on a
touch as a click does. The development pages of LineSweeper, `local_multiplayer`
and `minimal` set `touch-action: none`.
`minimal` gained a browser build for this, and draws each point's id where
the point is. In headless Chrome with touch emulation, two fingers drew two
markers with two ids, one followed its finger, a tap shorter than a frame
drew its own id, and none produced a `mousedown`, `mouseup` or `mousemove`.
The tap still fires `click`, which no engine listener hears. On Windows, touch
injected into a visible engine window from its own process arrived as two
points with client coordinates and no mouse message. Injection from another
process did not reach the window, so `minimal`'s markers have not been seen
on Windows. A real Windows touchscreen and a real phone are the checks still
owed. A full-screen request deferred to the next input event is not made from
a touch, because Emscripten runs deferred requests only from its own
listeners. That belongs to M7b, with the rest of the phone.

**M7b's sample half has landed: LineSweeper lays itself out upright and
draws its own buttons, and no phone has played it.** It is sample code and
nothing in `engine/` changed. The board still draws in one 1280x720 space,
and `presentation/arrangement.h` picks a camera each tick from the buffer's
aspect. Landscape frames the whole screen, and at exactly 1280x720 the camera
is the identity, so the capture tool's images did not move by a byte.
Portrait frames the board block, which is nearly square, across the buffer's
width, with a band under it for the controls. `presentation/touch_controls.h`
draws eight buttons from the first touch onwards and hit-tests every point
against them each frame. What a button means is a third `{Control, button}`
table in `play_state.cpp` feeding the same byte as the keys (P2), so a held
thumb gets the rules' own delay and repeat. Pause is a tap on PAUSE, and
restart after a top-out is a tap anywhere off the buttons. The pause menu
finds the row under a new finger and activates it, because `ui/` does not
depend on `input`. The development page drops the forced 16:9 when the
viewport is upright, sets `viewport-fit=cover` with the safe-area insets as
padding, and offers a full-screen button only where `requestFullscreen`
exists, so not on iPhone Safari. It lets a phone play: the poster rule in
[web.md §7.4](web.md#74-what-a-phone-sees-on-the-host-page) is the host page's. `LineSweeperViewTests` pins the arrangement and the
hit test headlessly. In headless Chrome at a 390x844 portrait viewport and an
844x390 landscape one, both with touch emulated, at `wasm-debug` and
`wasm-release`, the frames showed the portrait layout, the buttons appearing
on the first touch, CW rotating the piece, DROP dropping it, a held LEFT
repeating to the wall, HOLD holding, PAUSE opening the menu, a tapped RESUME
resuming, a tapped QUIT reaching `Module.onQuit`, and a tap on the board
after a top-out restarting. The Win32 sample was looked at in landscape,
unchanged, and in a tall window, where it takes the portrait layout. Still
owed: a real iPhone and a real Android phone, which the S3TC-less texture
path (above) has not met either,
and the top-out's frame time on a phone (R6).

**The web window's state has a test of its own: `WebWindowTests`, a page
driven by real input.** `AppTests` reaches `dom_events.h` under Node and
nothing that needs a DOM, so focus, the capture analogue, the size measured
before each tick, touch and full screen had been checked only by loading a
sample and looking. `tests/app/web/window_page.cpp` takes its canvas with a
real `Window` and logs every `WindowNotify` call, and a frame that runs with a
buffer nobody was told about is logged as a failure. `run_page.mjs` gained
`--input`, a script it runs against the page with the page's DevTools session
in hand, so the input is the browser's own dispatch rather than an event the
page made: `window_input.mjs` clicks, drags off the canvas, presses keys and
chords, turns the wheel, moves focus, resizes the canvas in CSS, sets a
device pixel ratio of two, puts two emulated fingers down, goes full screen
with and without a gesture and closes the window, comparing the whole log
after each step. Visibility is the exception, and the script says so: no CDP
call tried - another tab activated, the lifecycle states, a minimised window
- changes `visibilityState` in a headless browser, so the document is told it
is hidden and the event is dispatched. The browser lookup moved to the root
`CMakeLists.txt`, because this page is built in `tests/app` and under the null
render backend too, where the render pages are not. Breaking the wheel's
focus rule, reporting a size after the tick and dropping a touch's
`preventDefault` each failed it, at the step that names the rule. Under ctest it
runs in Chrome only; Firefox, which speaks WebDriver BiDi rather than CDP,
runs the same sequence by hand (the WEB#4 follow-up above).

**`collision` runs in a browser too**, so every sample now has a browser
build. It has a `main_web.cpp` and a development page in the shape of the
others, and needed no engine change. Its state moved out of `main.cpp` into
`collision_state.cpp` so that both entry points run it. The smoke test and
the WIC capture stay in `main.cpp`, Windows-only, because a page has no
command line and WIC is Windows'. `CollisionSampleTests` reads no files and
now runs under Node, which is the twentieth entry. `minimal`'s browser build,
which M7a gave it, is the new-project template's: its `CMakeLists.txt` builds
the page under `EMSCRIPTEN` and returns before the Windows half, so a project
copied from it gets both. At `wasm-release` the collision module is 338 KB,
with a 105 KB loader. The page was driven in headless Chrome at `wasm-debug`
and `wasm-release`, and the frames were looked at. The collision player
walked through the zero-mask rectangle, stopped at the solid wall, and went
around it into the trigger, which counted one entry and turned the player
green. Escape reached `Module.onQuit`. The only console error was the scratch
server's missing favicon. Headless Firefox has driven the page through the
[BiDi runner](web-to-be-tested.md#firefox-sample-checks-web4) (draw calls,
keys, quit; no pixels looked at). Safari and a pad have not been tried on it.

**Each sample's page is a ctest entry**, because until then nothing loaded
what a browser build ships, and a content path the module could not find
would first have shown on the live site. `LineSweeperPageTests`,
`LocalMultiplayerPageTests`, `MinimalPageTests`, `CollisionPageTests` and
`AudioPageTests` are registered in the root `CMakeLists.txt` wherever
`RenderPixelTests` is, and
CI's wasm job runs them as a step of their own. `run_page.mjs` gained a second
kind of page for them: with `--quit-keys` it wraps the callbacks the page
hands its `Module` before the loader reads them, waits for the runtime and
60 animation frames, presses each key for 150 ms (Escape, Down, Down, Enter
for LineSweeper; Escape, Enter for minimal; Escape for local_multiplayer and
collision; S then Escape for the sound board, where S is the gesture that
lets Web Audio start and Escape, which HTML does not count as one, is only
the way out), and
passes when `Module.onQuit` is called. A console error or warning, an
uncaught exception, a warning or an error the browser logs, or a call to
`Module.onError` fails it. The runner answers `/favicon.ico` itself, because
the browser asks every origin for one. It also does not count ANGLE's
"Performance" driver messages: over SwiftShader, Chrome logs "GPU stall due to
ReadPixels" four times for a sample whose code reads nothing back. Checked at
`wasm-debug` and `wasm-release` on the RTX 5080, and at `wasm-debug` again with
`--disable-gpu`, which draws with SwiftShader; all of them pass. They fail when
they should, too: LineSweeper at `wasm-debug` and minimal at `wasm-release`,
each with its content embedded one folder too deep, failed on the startup
error that reached `Module.onError`, and LineSweeper given Escape, Down,
Enter, which is RESTART, timed out without a quit. With the null renderer,
as CI's second wasm configure builds it, they pass too.

**M5's half in this repository is built, and the demo is live**: its
done-when is a live URL, the deploy is the owner's, in the site's own
repository, and it is at <https://mattharding.dev/demo/linesweeper/>,
checked on 2026-10-13 (below). The host is mattharding.dev, whose convention is a demo page of
its own at `/demo/<id>/`, so [web.md §7](web.md#7-what-the-host-page-gets) was rewritten for it: the demo is a plain
page of its own rather than an Astro component, and the bundle's `index.html`
is a working example of it, which the site's own page follows. The build links LineSweeper a second time as an ES module,
`linesweeper.js` with the factory `createLineSweeper` and `linesweeper.wasm`,
from the same objects as the development page, and `linesweeper_web_dist`
bundles it with the page, a poster, `NOTICE.txt` and `BUILD.json` into
`web-dist/linesweeper/` on every build; CI's wasm job uploads `wasm-release`'s
after the page tests pass. Nothing in the engine changed for it: every piece
of the engine's JavaScript reads `Module` and `specialHTMLTargets`, and under
`-sMODULARIZE` both are the instance's own. The second mount is where that was
proved rather than read. The bundle's page was driven in headless Chrome at
`wasm-debug` and `wasm-release`: before the click the page fetched itself and
the poster and nothing else; a click mounted the game on a new canvas, keys
moved, rotated, dropped and held pieces, Escape, Down, Down and Enter reached
`onQuit`, and "Play again" mounted a second instance on a fresh canvas, which
started from an empty board, played the same keys and quit the same way. The
console was clean throughout, and the frames were looked at. The first run
of that check found the one defect: the overlay holding "Play again" sat
under the old canvas, so the second click reached the dead game rather than
the button. With touch emulated, the page showed the keyboard line and
fetched no module. At `wasm-release` at `c369e8b` the module is 403,759 bytes
and the loader 118,982, 125,384 and 27,283 under Brotli at quality 11 - 153 KB
for a visitor to play at that quality, which the live host does not use, as
below. The module links the Web Audio backend, the touch device and
the CPU block decoder whether or not LineSweeper calls them.

## Packaged-page verification, 2026-10-27

The local `wasm-release` build passes all 31 CTest entries in Chrome
156.0.8078.48, using ANGLE on the RTX 5080 through D3D11.
`LineSweeperBundlePageTests` loads `web-dist/linesweeper/index.html` and
verifies real factory frames, quit/replay, touch gating, context loss/restore,
truncated-wasm startup failure and the captured configuration's `onError`
recovery UI. The earlier 2026-10-26 GPU startup denial no longer blocks this
local check. This closes BDP-04's runtime acceptance.

The [PR #59 wasm job](https://github.com/matt439/Labrador/actions/runs/40419996949/job/121911813526)
at initial head `0fffdf1` also succeeds. Its page-test output reports
`LineSweeperBundlePageTests` passed in 6.62 seconds and all eight page entries
passed. The CI browser is Chrome 156.0.8078.48 using Google's SwiftShader
Vulkan renderer, distinct from the local RTX 5080 run. This observes the
required CI entry and closes BDP-03. Neither run verifies the hosted page,
Firefox, physical touch or display pacing.

**BDP-24, 2026-10-27.** The packaged host page now has a visible accessibility
note referenced by Play and each fresh canvas through `aria-describedby`.
The [sample README](../../samples/linesweeper/README.md#accessibility) states
the same support and limitations and identifies LineSweeper as owner of the
planned palette work. After rebuilding `linesweeper_web_dist` with
`wasm-release`, `LineSweeperBundlePageTests` passes in 3.87 seconds in local
Chrome 156.0.8078.48 on the RTX 5080. It checks the note before play and on
first/replay canvases, and sends Tab through browser input to verify that
focus leaves the game for a page link. Captures at desktop viewport widths
1280 and 720 pixels were reviewed: the note wraps without horizontal overflow.
All four standalone CMake checks, website type checking, 21 website tests and
the 146-page website build pass. These checks do not establish physical-pad,
real-phone or screen-reader acceptance, and the changed page has not been
deployed to the live demo.

**The live page was checked on 2026-10-13**, deployed from the site's `main`
with `npm run deploy` from a bundle built at `c369e8b`, tagged `v0.1.0`, which
holds the calm top-out. `curl -I` with `Accept-Encoding: br` showed
`application/wasm` and `public, max-age=31536000, immutable` on the module
under `m/cd7d8cfd9043/`, `no-cache` on the page, and `Content-Encoding: br` on
both, and the four bundle files the site serves - all but the example
`index.html` - had the size and SHA-256 its `BUILD.json` records: a 403,759-byte module and a 118,982-byte loader,
which Cloudflare's own Brotli sends as about 150 KB and 32 KB, varying by a
few hundred bytes from one response to the next: about 183 KB for a visitor to
play, rather than the bundle's 153 KB, and the same again on 2026-10-25. In headless
Chrome it played, quit and played again with a clean console, and a match
played to a top-out drew no stack behind the banner, only dim sparks; with
`--disable-webgl2` a click on Play reached `onError` and the page showed the
engine's message; and under touch emulation it showed the recording, paused
and unfetched, and fetched no module.
[web-to-be-tested.md](web-to-be-tested.md) has the runs. Not checked: Firefox
and Safari on this page, a real context loss, a real phone seeing the
recording, and the top-out's flash count on this page rather than the
desktop build.
