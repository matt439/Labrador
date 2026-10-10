# The web port — what it costs, and in what order

> Scoping only, 2026-10-10, read against the tree at `862e08b`. **Nothing here
> has been built, and no Emscripten toolchain was installed on the machine that
> wrote it.** Every claim about this repository was checked by reading it and is
> cited by file and line. Every claim about browsers or Emscripten is general
> knowledge, and is marked where it carries weight. §1 says which is which, as
> [android.md](android.md) §1 does.

The goal is a sample running in a browser tab, embedded as a playable demo
on a static Astro site served by Cloudflare. This document answers three
questions: **what the port costs, what in this tree already makes it cheap,
and which decisions it forces that the tree has not yet made.** Those
decisions are in §8, with the owner's answers where given. Every one changes something a
design document states, a public header spells, or a rule `CLAUDE.md` gives.

Sizes mean what they mean in [android.md](android.md):
**hours** ≈ an afternoon, **days** ≈ 2–4 days, **week** ≈ 5 working days,
**weeks** ≈ 2 or more.

**Contents**

1. [What was checked, and what was not](#1-what-was-checked-and-what-was-not)
2. [The claim this port tests, and how it differs from Android's](#2-the-claim-this-port-tests-and-how-it-differs-from-androids)
3. [The platform surface, seam by seam](#3-the-platform-surface-seam-by-seam)
4. [The proposed web platform layer](#4-the-proposed-web-platform-layer)
5. [Build integration](#5-build-integration)
6. [Which sample ships first](#6-which-sample-ships-first)
7. [What the host page gets](#7-what-the-host-page-gets)
8. [Decisions this plan does not make](#8-decisions-this-plan-does-not-make)
9. [Milestones, and what to spike first](#9-milestones-and-what-to-spike-first)
10. [Out of scope, on purpose](#10-out-of-scope-on-purpose)
11. [The documents this port amends](#11-the-documents-this-port-amends)

---

## 1. What was checked, and what was not

**Checked by reading this tree.** I searched every file outside
`engine/{render,audio,input}/<backend>/` for platform includes and platform
types (`<Windows.h>`, `<objbase.h>`, `HWND`, `HRESULT`, `QueryPerformance*`,
`MultiByteToWideChar`, the thread-pool API, `DirectX::`), then read each hit.
The DirectX hits in `font.h`, `dds_file.h`, `sprite_geometry.cpp`,
`sound_bank.h`, `audio_device.h` and `gamepad_reader.h` are all in comments.
The real surface is the list in §3. I also read the GL backend's WGL code and
the places where it leaves the GLES 3.0 intersection. LineSweeper's content was
checked byte by byte, because its formats decide what WebGL2 has to accept.

**Not checked, and load-bearing.** Nothing was compiled with Emscripten.
Three things are therefore guesses. The first is how many warnings clang
raises on a tree only MSVC has compiled, under `-Werror` with zero
suppressions (§9, S1). The second is the output size. The third is the
extension name that Emscripten's `glGetString` reports for S3TC. The browser
facts below are general knowledge: S3TC coverage, timer resolution, Gamepad API
gating, and what COOP/COEP does to a page. They were not tested in a browser.

**Not checked, and not load-bearing.** Behaviour on phones. v1 is a desktop
demo (§10).

---

## 2. The claim this port tests, and how it differs from Android's

[ARCHITECTURE.md:259-272](../design/ARCHITECTURE.md#L259-L272) promises that
platform code sits behind engine-owned interfaces, *"so a second platform is an
addition, not a rewrite"*. It also names the window as the third case, which
*"moves down a folder without renaming the class or touching a call site"*.
[android.md](android.md) §2 recorded that this claim has never been tested, and
it still has not. Every configuration in `CMakePresets.json` is Windows x64
under MSVC.

**The browser is a cheaper first test than Android, and it is not a smaller
one.** It is cheaper on three counts, each of which is the Android port's
hardest item becoming its easiest:

- **No new graphics API.** WebGL2 is OpenGL ES 3.0. The GL backend's shader was
  written for exactly that intersection
  ([`gl/sprite_shader.h:25-29`](../../engine/render/gl/sprite_shader.h#L25-L29)).
  Its loader says the only things outside it are one call, one token and the
  content's texture format
  ([`gl/gl_functions.h:26-32`](../../engine/render/gl/gl_functions.h#L26-L32)).
  Android needed a fifth backend. The web needs about thirty lines of GL, in
  the right place (§3.3).
- **The gamepad seam survives.** On Android every device is pushed, and
  [android.md](android.md) §3.3 found that `GamepadReader`'s pull shape does
  not hold there. The browser Gamepad API is polled:
  `navigator.getGamepads()` returns a snapshot on demand. That is exactly what
  `GamepadReader::read(slot)` asks for
  ([`input/gamepad_reader.h`](../../engine/input/gamepad_reader.h)).
- **There is no surface loss.** A WebGL context can be lost, but a tab moving
  to the background does not lose it. The lifecycle is visible/hidden plus
  focus/blur, which maps onto the four `WindowNotify` activation calls that
  already exist.

It is not smaller, because it is the first build with **a second compiler, a
second C++ standard library, and a frame loop the program does not own.** The
first two put `cmake/settings.cmake` under load. The third puts
`Application::run` under load, because it is documented as *"runs until the
window closes. Returns the process exit code"*
([`application.h:141-143`](../../engine/app/application.h#L141-L143)). In a
browser nothing blocks, and nothing returns.

---

## 3. The platform surface, seam by seam

Each item is graded by how cleanly it sits behind an abstraction that already
exists. **Behind a seam** means a new implementation and no change above the
seam. **Behind a pimpl** means the header is already neutral and only the
`.cpp` changes. **In the open** means a platform type appears in a public
signature.

| # | Where | What | Grade |
|---|---|---|---|
| 3.1 | `app/application.h` | `HINSTANCE`, `HWND`, `<Windows.h>`, `std::wstring` options | **In the open** |
| 3.2 | `app/window.{h,cpp}` | The Win32 window, message translation and pump | **In the open**, by design |
| 3.3 | `render/gl/` | WGL context, loader, three GLES gaps | Behind a seam, plus a dialect |
| 3.4 | `input/xinput/` | Pad reads | **Behind a seam** |
| 3.5 | `audio/xaudio2/` | Playback | **Behind a seam**, with an open container question |
| 3.6 | `core/thread_pool.cpp` | The Win32 thread pool | **Behind a pimpl** |
| 3.7 | `core/step_timer.h` | `QueryPerformanceCounter` | **In the open**, with no include |
| 3.8 | `app/content_root.cpp` | `GetModuleFileNameW` | Behind a neutral function |
| 3.9 | `render/text_encoding.cpp` | `MultiByteToWideChar`, 32-bit `wchar_t` | Behind a neutral function |
| 3.10 | `app/application.cpp` | `CoInitializeEx`, `XMVerifyCPUSupport` | Private, and Windows-only by nature |
| 3.11 | `cmake/settings.cmake`, `vcpkg.json`, `engine/CMakeLists.txt` | MSVC flags, DirectXTK, XInput | Build-only |
| 3.12 | `samples/*/main.cpp` | `wWinMain`, `MessageBoxA` | Per sample |

`render/throw_if_failed.h` is Windows code outside a backend folder. It is
shared by the two Direct3D backends and nothing else includes it, so a web
build never compiles it.

### 3.1 `Application`'s public signature — the one real architecture question

`Application::initialize(HINSTANCE instance, int show_command)`
([`application.h:128`](../../engine/app/application.h#L128)) and
`HWND Application::window() const` ([`:233`](../../engine/app/application.h#L233))
put Win32 types into the one class every client constructs, and
`<Windows.h>` into its header ([`:20`](../../engine/app/application.h#L20)).
Every sample's `wWinMain` passes those two arguments through. ColourWars does
too. The window's title and class name are `std::wstring`
([`:45-46`](../../engine/app/application.h#L45-L46)), which is harmless on the
web: a class name means nothing there and the title can be ignored.

This is the only place where the port cannot be written as "a new file
behind an existing seam". Any fix changes a public signature in the shell, and
`PHILOSOPHY` batches source breaks. **Decision D2 in §8.**

### 3.2 The window

`Window` is the platform class ARCHITECTURE names, and `WindowNotify` is the
neutral interface its owner implements. That half is already clean:
**`WindowNotify` has no Win32 type in it**
([`window.h:21-74`](../../engine/app/window.h#L21-L74)). Keys arrive as `Key`,
buttons as `MouseButton` and text as `char32_t`. `tick()` is a callback, so
the frame loop is already inverted. The engine does not run a `while` loop the
game sits inside. `Application::tick` is called by whatever drives the pump
([`application.cpp:261-265`](../../engine/app/application.cpp#L261-L265)), and
that is the shape `emscripten_set_main_loop` wants.

The other half is not clean. `window.h` includes `<Windows.h>` and spells
`HINSTANCE`, `HWND`, `DWORD`, `LRESULT` and `wchar_t` in its constructor, its
public statics and its private members
([`:127-128`](../../engine/app/window.h#L127-L128),
[`:157`](../../engine/app/window.h#L157),
[`:192-193`](../../engine/app/window.h#L192-L193),
[`:202-254`](../../engine/app/window.h#L202-L254)).

**ARCHITECTURE's promised move does not survive the include check as
written, and that finding is worth having before the work starts.**
ARCHITECTURE says the pair *"moves down a folder without … touching a call
site"*. But `check_engine_includes.cmake` fails the build for any file outside
`engine/<module>/<backend>/` that includes a header inside it, and it captures
**any** module, `app` included
([`:116-124`](../../cmake/check_engine_includes.cmake#L116-L124)). If
`window.h` moved to `app/win32/window.h`, `application.h` including it would
fail the build. That is the right outcome, because the header would be a
platform header. So the move that actually works is the one `GamepadReader`
and `ThreadPool` already use. `app/window.h` stays where it is, becomes
neutral, and holds a `struct Impl`. `app/win32/window.cpp` and
`app/web/window.cpp` each define that `Impl`. That keeps the class name and
every call site, adds one pointer hop on a path that runs once per message,
and amends one sentence of ARCHITECTURE. **Decision D3.**

`Window::outer_size_for_client` takes `DWORD` styles and is public because it
is the one testable part of the class
([`:183-193`](../../engine/app/window.h#L183-L193)). It belongs to the Win32
implementation, and its tests in `tests/app/window_tests.cpp` stay
Windows-only.

### 3.3 The GL backend, against WebGL2

The WGL-specific code is confined, and there is not much of it:

- Context creation is `Impl::create_context(HWND)` with the 1.1 bootstrap
  ([`gl/renderer.cpp:344-480`](../../engine/render/gl/renderer.cpp#L344-L480)),
  the `HWND`/`HDC`/`HGLRC` members in [`gl/backend.h`](../../engine/render/gl/backend.h),
  `SwapBuffers` ([`:853`](../../engine/render/gl/renderer.cpp#L853)) and
  `GetClientRect` in `drawable_size`
  ([`:336`](../../engine/render/gl/renderer.cpp#L336)).
- The loader is all of `gl_functions.cpp`, using `wglGetProcAddress`. Under
  Emscripten, GLES 3.0 entry points are linked directly, so the loader has
  nothing to do.

The GLES gaps are the three that `gl_functions.h:26-32` lists, and all three
are real on WebGL2:

1. **`glDrawElementsBaseVertex` does not exist in WebGL2**
   ([`renderer.cpp:680`](../../engine/render/gl/renderer.cpp#L680)). The base
   vertex is how one index buffer, built for `MAX_RUN_SPRITES`, serves every
   run. Without it, the replacement is to re-point the three
   `glVertexAttribPointer` offsets at the run's first vertex before each draw
   ([`:565-571`](../../engine/render/gl/renderer.cpp#L565-L571)). That is
   three calls per run rather than one argument, and draws the same thing.
2. **WebGL2 has no BGRA upload**
   ([`texture_factory.cpp:69`](../../engine/render/gl/texture_factory.cpp#L69)).
   This is not hypothetical. LineSweeper's only texture,
   `content/textures/white.dds`, is a 1×1 `b8g8r8a8` (its header's red mask is
   `0x00ff0000`). Either swizzle to RGBA on upload, or re-encode the content.
3. **S3TC is an extension, and on the web it is a per-device lottery.**
   [`texture_factory.cpp:17-53`](../../engine/render/gl/texture_factory.cpp#L17-L53)
   asks for `GL_EXT_texture_compression_s3tc` and refuses by name without it.
   LineSweeper's font atlas is BC2. *General knowledge:*
   `WEBGL_compressed_texture_s3tc` is present in desktop Chrome, Edge,
   Firefox and Safari, and mostly absent on phones. So a desktop demo
   draws, and a phone would show the backend's own "no S3TC" throw.
   [content-probe.md](content-probe.md) is the long answer for phones. The
   short one for v1 is a desktop-only demo, with a CPU BC decode as a later
   fallback (§9, M7).

Shader: `#version 330 core` becomes `#version 300 es` plus
`precision mediump float;`. Nothing else changes, as the header predicted.
Presentation: the browser composites at the end of each animation-frame
callback, so `SwapBuffers` becomes nothing, and the browser owns vsync.
`read_back_buffer`'s `glReadPixels` works in WebGL2, as long as it runs before
control returns to the browser.

**Where this code lives is Decision D1.** `CLAUDE.md` says *"A sixth is not
planned"*, and [android.md](android.md) §6 rejected *"a GLES backend as a
hedge"*. Its reasons were profile, shading language and texture formats. On
the web the first two are the one-line differences above, and the third is
the same S3TC question either way. So this plan argues for **one `gl/`
backend with a second context translation unit**, not a sixth folder. It
does not decide that.

### 3.4 Input — the pad seam holds, and the fed devices stay fed

`GamepadReader` is a concrete class with a pimpl, and
`input/xinput/gamepad_reader.cpp` is its only implementation. A web
implementation, `input/web/gamepad_reader.cpp`, samples
`emscripten_get_gamepad_status` for each slot inside `read(slot)`. It maps the
W3C "standard" layout onto `GamepadButton`, and answers disconnected for any
pad without a standard mapping. *General knowledge:* browsers hide pads until
the page sees a button press. That is the "slot occupied on both frames" rule
`gamepad.h` already enforces, so a pad appears with no phantom press.

The keyboard and mouse are fed, as on Win32. `app/web/window.cpp` registers
`emscripten_set_key{down,up}_callback` on the canvas and translates
`KeyboardEvent.code`, a physical position, into `Key`. That is the same
position-not-character rule LineSweeper's bindings rely on
([`play_state.cpp:36`](../../samples/linesweeper/states/play_state.cpp#L36)).
The keypress `key` field supplies `on_text`. Mouse callbacks take
canvas-relative coordinates scaled by the canvas's backing-store ratio. The
web window must call `preventDefault` on the keys the game uses. Otherwise the
arrows and space scroll the host page under the game.

**What is missing is in CMake, not code.** There is no
`LABRADOR_INPUT_BACKEND`. `input/xinput/gamepad_reader.cpp` is listed
unconditionally ([`engine/CMakeLists.txt:43`](../../engine/CMakeLists.txt#L43)).
It needs the same selection variable as render and audio, with `xinput` as
the default.

### 3.5 Audio — cut for v1

The seam exists ([`audio/audio_device.h`](../../engine/audio/audio_device.h)),
and `audio/null/` behind it already builds without a platform API. That is how
the `-null` presets build. **LineSweeper has no audio at all.**
[`pause_state.cpp:239`](../../samples/linesweeper/states/pause_state.cpp#L239)
says so in as many words. So v1 builds with `LABRADOR_AUDIO_BACKEND=null` and
costs nothing.

A Web Audio backend, `audio/webaudio/`, is the same shape of job as an
Android one: one implementation of an existing seam. It inherits the
container question that [android.md](android.md) §3.2 left open.
`open_wave_bank` takes a bank name, and the only bank format in this tree is
`.xwb`, read by DirectXTK. The web needs a reader that is not DirectXTK, so
either an engine `xwb_file.h` or a decision that banks are WAV folders. That
is the same unmade decision, and the web port should not make it as a side
effect. Not v1.

### 3.6 Threads — single-threaded, and LineSweeper uses none

The Win32 pool is behind `ThreadPool::Impl`
([`thread_pool.h`](../../engine/core/thread_pool.h), whose comment says why).
A serial `Impl` that runs each task inside `add_task`, records the first
exception and rethrows it from `wait_for_tasks_to_complete` meets every
promise the header makes. It should pass the seven cases in
`tests/core/thread_pool_tests.cpp`, because none of them asserts that two
tasks overlapped. That is a reading, not a run.

**LineSweeper never fans out.** Both of its states build
`Scene(nullptr, nullptr)`
([`play_state.cpp:104`](../../samples/linesweeper/states/play_state.cpp#L104),
[`pause_state.cpp:104`](../../samples/linesweeper/states/pause_state.cpp#L104)),
and `view_capacity = 1` ([`main.cpp:34`](../../samples/linesweeper/main.cpp#L34)).
Its pool is constructed and never used. Pthreads would buy this demo nothing.
They would also make the host page cross-origin isolated (§7.3), which is a
cost to the whole personal site, not to the demo. **Single-threaded is the
recommendation, not a fallback.** `local_multiplayer` would fan out two views
on the serial pool, which is the null-pointer path its README already
describes as equivalent.

### 3.7 The clock

[`core/step_timer.h`](../../engine/core/step_timer.h) calls
`QueryPerformanceCounter` without including anything that declares it.
[android.md](android.md) §1 recorded this, and the file is Microsoft's under
`NOTICE`. The portable answer is `std::chrono::steady_clock`. Emscripten backs
that with `performance.now()`, which browsers coarsen (*general knowledge:*
about 100 µs in Chrome and 1 ms in Firefox without cross-origin isolation). For
a 60 Hz fixed step clamped at 0.1 s
([`:41-42`](../../engine/core/step_timer.h#L41-L42),
[`:128-130`](../../engine/core/step_timer.h#L128-L130)), that is adequate.
Changing the Windows clock source is a change to fixed-step timing on every
preset. The alternative keeps QPC on Windows behind a two-line platform
split. **Decision D4.**

### 3.8 – 3.10 The small three

- `executable_directory()`
  ([`content_root.cpp:13-42`](../../engine/app/content_root.cpp#L13-L42)) has
  an obvious web answer: `"/"`, where the packed content is mounted. The
  function's contract survives as written.
- `widen()` ([`text_encoding.cpp:8-27`](../../engine/render/text_encoding.cpp#L8-L27))
  needs a portable UTF-8 decoder. **`wchar_t` is 32 bits under Emscripten**, as
  on Android ([android.md](android.md) §3.6). The decoder should emit UTF-16
  code units into the `wstring` anyway, so that glyph tables stay keyed the
  way `text_encoding.h` says. LineSweeper's text is ASCII, so v1 cannot see
  the difference. The `char16_t` question stays deferred, as android.md
  deferred it.
- `CoInitializeEx`, `CoUninitialize` and `XMVerifyCPUSupport`
  ([`application.cpp:93-104`](../../engine/app/application.cpp#L93-L104),
  [`:85-88`](../../engine/app/application.cpp#L85-L88)) are Win32 shell duties
  with no web counterpart. They move into the Win32 window `Impl`, or behind
  `#if`. **No `DirectX::` call survives above a backend.**

---

## 4. The proposed web platform layer

```
engine/app/window.h                 neutral; holds Window::Impl            (D3)
engine/app/win32/window.cpp         today's window.cpp, moved
engine/app/web/window.cpp           canvas, html5 callbacks, main loop
engine/render/gl/…                  + a web context TU, ES dialect          (D1)
engine/input/web/gamepad_reader.cpp Gamepad API
engine/audio/null/                  as it is
engine/core/thread_pool.cpp         + a serial Impl
```

**Window and canvas.** The web `Window` takes the canvas that Emscripten
treats as `Module.canvas`, so the page decides which canvas, and no CSS
selector is compiled in. It creates a WebGL2 context with
`emscripten_webgl_create_context`, using `majorVersion = 2`, `alpha = false`
(the engine's blend is premultiplied and the page must not show through),
`antialias = false` and `preserveDrawingBuffer = false`. It hands the context
to the renderer as the same `void*` that `create_device` already takes for an
`HWND`. The drawing buffer follows the canvas's CSS box times
`devicePixelRatio`, observed with a `ResizeObserver`. Each change goes through
`on_window_size_changed`, the door every Win32 resize already uses
([`application.cpp:395-419`](../../engine/app/application.cpp#L395-L419)).
`set_resolution` sets the canvas size. `set_fullscreen` asks the Fullscreen
API for the canvas's container, which a browser grants only during a user
gesture.

**The frame loop.** `pump_until_quit()` becomes
`emscripten_set_main_loop_arg(tick, this, 0, true)`. The `0` means
`requestAnimationFrame`. The `true` (*simulate infinite loop*) means `main`
never returns, and its stack is not unwound. That is why `Application`, a
local in `main`, stays alive. *Emscripten's documented behaviour, unverified
here.* The consequences:

- **`run()` never returns on the web, and no destructor runs.** That matters to
  a state that saves on exit. LineSweeper has none.
- **`quit()` cancels the main loop** and tells the page through a
  JavaScript callback. LineSweeper's pause menu has a QUIT choice
  ([`play_state.cpp:227`](../../samples/linesweeper/states/play_state.cpp#L227)).
  On the web it should show "Play again", and the page decides what that
  does.
- **Pacing is the browser's**, at display rate. `StepTimer` runs `update()` at a
  fixed 60 Hz and `render()` once per animation frame. On a 60 Hz display this
  is two 60 Hz clocks again, the pairing whose failure mode
  [`2026-09-19-g6f-ratchet.md`](../performance/2026-09-19-g6f-ratchet.md)
  describes. The ratchet itself needs a blocking present, and there is none
  here. The relative phase of the two clocks still decides whether a frame
  gets zero or two updates. Measure judder on 60 Hz and 144 Hz displays
  before calling the loop done (§9, risk R5).

**Lifecycle.** `visibilitychange` to hidden calls `on_suspending()`, and
visible calls `on_resuming()`, which already resets elapsed time. Canvas
`blur` and `focus` call `on_deactivated` and `on_activated`, so held keys are
released when focus leaves, exactly as `set_input_focus` does for alt-tab
([`application.cpp:309-324`](../../engine/app/application.cpp#L309-L324)). The
`webglcontextlost` event goes to the renderer's `DeviceNotify`. The asset
reload path already exists for D3D and Vulkan and is settled in `renderer.h`.
That makes GL a fourth backend that can lose a device, which changes one
sentence in `gl/backend.h`. Wiring it is M6 work. In v1, a lost context
shows the error overlay.

**Assets.** Recommend **`--embed-file`** for LineSweeper. Its content is
21,512 bytes of font and 132 bytes of texture. Embedding it in the `.wasm`
removes one request and one file. Switch to **`--preload-file`**, which adds a
separate `.data` file, once a sample's content passes about 1 MB. Both
mount the content at `/`, so `load_manifest("./manifest.json")` resolves
through 3.8 unchanged, and `std::filesystem` reads it from Emscripten's
in-memory file system. Fetching assets lazily over HTTP is not needed until
content is large. It would also mean an asynchronous loader, which T3 says to
avoid until something needs it.

**Errors.** T6 means the engine throws, and the samples catch at `main` and
show a message box ([`main.cpp:58`](../../samples/linesweeper/main.cpp#L58)).
Under Emscripten, C++ exceptions **are not catchable by default.** Build with
`-fwasm-exceptions` (native WebAssembly exception handling). The web
`main` then forwards `e.what()` to an overlay on the page.

---

## 5. Build integration

**Presets.** Add a hidden `wasm-base` alongside the x64 `base`, plus
`wasm-debug` and `wasm-release`:

- Toolchain: vcpkg's, with `VCPKG_CHAINLOAD_TOOLCHAIN_FILE` set to
  `$env{EMSDK}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake` and
  `VCPKG_TARGET_TRIPLET=wasm32-emscripten`. No `architecture` block.
- Cache: `LABRADOR_RENDER_BACKEND=gl`, `LABRADOR_AUDIO_BACKEND=null`, and a new
  `LABRADOR_INPUT_BACKEND=web`.
- `EMSDK` must be set, as `VCPKG_ROOT` and `VULKAN_SDK` are today. It is the
  second thing in the repository that has to be installed, after the Vulkan
  SDK.

**What changes so the Windows presets do not.** Every edit sits behind
`if(EMSCRIPTEN)` or a vcpkg platform qualifier:

- `vcpkg.json`: `directxtk` gains `"platform": "windows"`. Doctest is
  header-only and builds for any triplet.
- `engine/CMakeLists.txt`: `find_package(directxtk)` (`:13`) and its `PRIVATE`
  link (`:293-295`) become conditional, and the input backend is selected
  (3.4).
- `cmake/settings.cmake` gains a clang branch. **This is the item to argue,
  not translate.** The comment there explains why `/fp:precise` is stated
  rather than inherited. The clang equivalent is `-ffp-contract=off` and no
  `-ffast-math`, stated for the same reasons. Warnings become `-Wall -Wextra
  -Wpedantic -Werror` with zero suppressions, the same rule as `/W4 /WX`. The
  `UNICODE WIN32 _WINDOWS NOMINMAX WIN32_LEAN_AND_MEAN` definitions move under
  `if(WIN32)`.
- Samples other than LineSweeper, `bench/` and `tools/` are wrapped in
  `if(NOT EMSCRIPTEN)` until each has a web entry point.

**The two checks are unaffected.** `check_engine_includes.cmake` and
`check_doc_citations.cmake` run under `cmake -P` and read source text, so the
host and the target do not matter. The include check *helps*: once
`app/win32/` and `app/web/` exist, it enforces D3 for free.
`compile_shaders.cmake` is never reached on `gl`.

**Tests.** Emscripten's toolchain sets `CMAKE_CROSSCOMPILING_EMULATOR` to
Node, so `add_test` runs a doctest `.js` under Node without changing any test
file. Node 24 is already on this machine for the website.

- **Expected to run under Node:** `MattMathTests`, `CoreTests` (serial pool),
  `CollisionTests`, `SceneTests`, `RenderTests` (the seam tests need no
  device), `InputTests`, `UiTests`, `AssetsTests`, `AudioTests` (with its null
  recording cases), `LineSweeperTests` and `LineSweeperViewTests`.
  **`LineSweeperTests` under Node is the strongest single check in the port**:
  it is the game's rules, run on a second compiler and a second floating-point
  code generator, and asserted rather than played.
- **`AppTests` splits.** `application_options_tests.cpp` and the pure parts
  of `content_root_tests.cpp` run. `window_tests.cpp` is Win32 by construction
  and stays Windows-only.
- **`RenderPixelTests` cannot run under Node**, because Node has no WebGL2.
  It *can* run in a headless browser: build it for wasm, serve it, and drive
  Chromium with Playwright, comparing `read_back_buffer` against the same
  `tests/render/golden/` PNGs. *General knowledge:* Chrome's WebGL on Windows
  is ANGLE over Direct3D 11, so it plausibly lands inside
  `ALLOWED_CHANNEL_DRIFT`. That is a measurement to make, not a premise. It is
  worth doing, because it is the only check that the ES dialect still meets
  the pixel contract. It is not on the critical path for a demo (M6).
- **`Benchmarks`** is complexity-class rather than wall-clock, so it can run.
  `LineSweeperFrameBench` and `LineSweeperCapture` stay Windows-only.

**CI.** One more job in `.github/workflows/ci.yml`: set up emsdk, configure
`wasm-release`, build, run `ctest`, and upload the LineSweeper bundle as an
artifact. It adds no rasteriser and needs no GPU.

---

## 6. Which sample ships first

**LineSweeper**, without much of a contest:

| | LineSweeper | local_multiplayer | minimal | ColourWars |
|---|---|---|---|---|
| Players | 1 | 2, one per pane | 1 | 2+ |
| Audio | none | none | — | yes |
| Fans out | no | 2 views | — | yes |
| Content | 21.6 KB, BC2 + BGRA | small | small | large, private |
| Is a game | yes | a camera demo | a template | yes |

What LineSweeper needs beyond the platform layer is small:

- **A web entry point.** `main.cpp` is `wWinMain` with a `MessageBoxA`
  ([`:18`](../../samples/linesweeper/main.cpp#L18),
  [`:58`](../../samples/linesweeper/main.cpp#L58)). A `main_web.cpp` beside it,
  chosen in the sample's `CMakeLists.txt`, keeps the Windows file untouched.
  The `ApplicationOptions` block is identical, `target_fps = 60` included,
  which the rules depend on.
- **QUIT behaviour**, as in §4.
- **Nothing else.** It reads the keyboard and pad 0 only, so one visitor with
  one keyboard has the whole game.

`local_multiplayer` already plays on one keyboard, with WASD and the arrow
keys, but one visitor driving two cursors is a demonstration of the engine,
not a game. ColourWars is private and outside this repository. Bots or a
one-player mode would be its own design work, out of scope for a port.
`local_multiplayer` is the natural **second** web sample, because it is the
cheapest way to exercise two views and the serial pool fan-out in a browser.

---

## 7. What the host page gets

### 7.1 Files

Link with `-sMODULARIZE -sEXPORT_ES6 -sEXPORT_NAME=createLineSweeper
-sENVIRONMENT=web -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2
-sALLOW_MEMORY_GROWTH -fwasm-exceptions`, with content embedded:

| File | What | Expected size (**estimate**, §9 S3 measures it) |
|---|---|---|
| `linesweeper.wasm` | engine + game + libc++ + 22 KB of content | 0.6–1.2 MB raw; ~200–400 KB Brotli |
| `linesweeper.js` | ES module loader and glue | 60–150 KB raw; ~20–40 KB Brotli |
| `linesweeper.data` | only with `--preload-file` | absent in v1 |

The baseline for the estimate is `x64-release-gl`'s `LineSweeperSample.exe`
at 297 KB, which links the C runtime dynamically. The wasm build links libc++,
`<filesystem>` and exception support statically, which is most of the gap.

### 7.2 Headers the host must send

**Single-threaded, as recommended:**

- `.wasm` served as `Content-Type: application/wasm`. `instantiateStreaming`
  refuses anything else. *General knowledge:* Cloudflare already maps the
  extension. Confirm with `curl -I` after the first deploy.
- `Cache-Control: public, max-age=31536000, immutable`, on files under a
  versioned path such as `/demos/linesweeper/<build-hash>/`. The page refers
  to the path, so a new build is a new path and never a stale cache.
- **If the site sends a Content-Security-Policy**, its `script-src` needs
  `'wasm-unsafe-eval'`, or the browser refuses to compile the module.
- **No COOP/COEP.** That is the point of §3.6.

**Only if pthreads are ever added:** the *page* (the HTML, not the wasm)
needs `Cross-Origin-Opener-Policy: same-origin` and
`Cross-Origin-Embedder-Policy: require-corp`. That breaks any cross-origin
embed on the same page (video players, analytics iframes) unless each one
opts in. On Cloudflare this is a `_headers` rule scoped to the demo page's
path, so the rest of the site is untouched.

### 7.3 Mounting it, start-on-click

The page shows a poster image and a button. Nothing is downloaded until the
click. That keeps the personal site's own load cost at one image. It gives
`AudioContext` the user gesture it needs when audio arrives. And it is when
the canvas takes keyboard focus. `LineSweeperCapture` already produces
repeatable frames of this game for the Labrador site, and one of them is the
poster.

```html
<div class="labrador-demo">
  <canvas id="linesweeper" tabindex="0" hidden></canvas>
  <button id="play" style="background-image:url(/demos/linesweeper/poster.png)">
    Play LineSweeper
  </button>
  <p class="labrador-error" hidden></p>
</div>
<script type="module">
  const base = "/demos/linesweeper/BUILD_HASH/";
  const canvas = document.getElementById("linesweeper");
  const play = document.getElementById("play");
  play.addEventListener("click", async () => {
    play.disabled = true;
    const { default: createLineSweeper } = await import(base + "linesweeper.js");
    canvas.hidden = false;
    play.hidden = true;
    await createLineSweeper({
      canvas,
      locateFile: (path) => base + path,
      onQuit: () => { /* show "Play again" */ },
      onError: (message) => { /* fill .labrador-error */ },
    });
    canvas.focus();
  }, { once: true });
</script>
```

In Astro this is one `<LabradorDemo>` component that takes `base`, `poster`
and an aspect ratio. Its CSS sets `aspect-ratio: 16 / 9; width: 100%` on the
canvas, and the drawing buffer follows that box (§4). A visitor without
WebGL2 or S3TC gets the error paragraph naming what is missing, which is T6
on a web page. `onQuit` and `onError` are the only two calls from the engine
to the page. They are named here so that the web `Window` gets them from
`Module`, not from globals.

### 7.4 What a phone sees before M7

Most visitors open the host site on a phone, so the v1 page cannot just put
an error where the game should be. The component decides **before
downloading anything**: if `matchMedia("(pointer: coarse)")` matches and no
physical keyboard has been used, it shows the poster and a short muted loop
of the game, with a line saying the playable version needs a keyboard or a
controller for now. `LineSweeperCapture` already produces this game's
frames byte for byte, and the Labrador site already hosts a silent MP4, so
the footage costs nothing new. The `.wasm` is never fetched on a phone, so a
phone visitor pays for one image and one short video, and gets something to
watch rather than a refusal.

---

## 8. Decisions

Each of these changes something a design document or a public header states.
None of them is made by writing it down here. A decided row records the
owner's answer and when it was given. An open row has a recommendation and
needs a yes before the milestone that depends on it.

### Decided, 2026-10-10

| | Decision | Answer | What it amends |
|---|---|---|---|
| **D1** | WebGL2 inside `render/gl/` with a second context TU, or a sixth backend folder | **Inside `gl/`.** One backend, two context TUs (`wgl`/`web`), and the three GLES gaps closed for both. The cost is one more TU than the "at most one more" rule in `CLAUDE.md` allows, and that rule is amended rather than worked around | `CLAUDE.md` backend paragraph; `gl/backend.h` header comment; android.md §6 gets an amendment, not a reversal |
| **D2** | How `Application` stops spelling `HINSTANCE`/`HWND` | **`initialize()` takes no arguments.** The Win32 `Impl` gets `GetModuleHandleW(nullptr)` and `SW_SHOWDEFAULT` itself, and `window()` returns `void*`, as `create_device` takes it. One source break for every sample and ColourWars, batched as PHILOSOPHY asks | `application.h`; every sample's `main.cpp`; ColourWars, whose pin is already stuck behind the math split |
| **D3** | The window move | **A neutral `app/window.h` holding a `Window::Impl`**, with the code in `app/win32/window.cpp` and `app/web/window.cpp`, because the include check rejects the move ARCHITECTURE describes (§3.2) | `ARCHITECTURE.md:259-272` |
| **D4** | The clock | **`std::chrono::steady_clock` on every platform**, measured against QPC on Windows before the change lands, so it is a checked swap and not an assumed one | `step_timer.h`, `NOTICE` |
| **D5** | v1's reach | **Desktop browsers first, and phones next, as a planned phase rather than a someday** (§9, M7). Most visitors open the host site on a phone, so v1 has to give a phone a good page, not only a refusal (§7.4) | `docs/website-plan.md:90` |
| **P1** | How a touchscreen plays a game | **An engine touch device**, fed from the window like the keyboard and mouse, on **every** touchscreen rather than only phones: Win32 touch through `WM_POINTER` as well as the browser. The owner had wanted touch support regardless of the web port | `input/` gains a device; `WindowNotify` gains touch calls; `ARCHITECTURE.md`'s fed-devices paragraph |
| **P2** | Whether an action-mapping layer comes with touch | **Not yet.** Agreed by the owner. LineSweeper already maps keys onto its own buttons in a sample-level table ([`play_state.cpp:46-54`](../../samples/linesweeper/states/play_state.cpp#L46-L54)), and on-screen controls can feed the same buttons. The engine gains a device, and the binding layer stays in `CLAUDE.md`'s known-absent list until a second client needs one (T1). [android.md](android.md) §3.3 expected touch to force that layer; this plan argues that it does not | `CLAUDE.md` known-absent list, only if the answer is yes |

### Open

Nothing. Anything else this plan meets that would change a design document, a public header or a rule in `CLAUDE.md` comes back here as a new row before it is built.

---

## 9. Milestones, and what to spike first

### Spikes — *days*, before any milestone is committed to

Each is cheap and could change the plan:

- **S1 — clang under `-Werror`.** Install emsdk. Build `engine/math` and
  `engine/core` with the proposed clang flags and **count the warnings**. This
  is the largest unknown in the plan. Clang has never seen this tree, the rule
  is zero suppressions, and the count could be ten or three hundred. If it is
  large, the clean-up is its own milestone, and it improves the Windows build
  too.
- **S2 — `LineSweeperTests` under Node.** It links no engine, so it needs
  only S1's flags and a toolchain. A pass shows that the rules are the same
  game on a second compiler. It also gives the first real size number.
- **S3 — WebGL2 through `gl/renderer.cpp`.** A throwaway canvas, the web
  context, the ES dialect and the base-vertex replacement. One textured quad
  from a BC2 atlas in Chrome, Firefox and Safari. That settles the S3TC
  extension name Emscripten reports, and whether `has_gl_extension` sees it.

### Milestones

| | Milestone | Size | Done when |
|---|---|---|---|
| **M0** | Spikes S1–S3 | days | Warning count known; LineSweeperTests green under Node; one quad in three browsers |
| **M1** | Build and core: presets, `settings.cmake` clang branch, vcpkg qualifier, input-backend variable, serial pool, clock (D4), `widen`, `executable_directory` | days (+ S1's clean-up) | `ctest --preset wasm-debug` runs the §5 list green; every Windows preset unchanged and green |
| **M2** | The shell: D2 and D3, `app/win32/` moved with no behaviour change, `app/web/window.cpp` (canvas, callbacks, main loop, lifecycle, resize) | week | LineSweeper reaches its first frame in a browser and takes keys; Windows samples behave as before (look at them, per the see-it-on-screen rule) |
| **M3** | WebGL2 in `gl/` (D1): context TU, ES shader, base vertex, BGRA swizzle, S3TC via WebGL | days | LineSweeper plays end to end in Chrome, Firefox and Safari on desktop; `RenderPixelTests` still green on `x64-debug-gl` |
| **M4** | Gamepad: `input/web/gamepad_reader.cpp` | hours | A pad on the standard mapping plays LineSweeper |
| **M5** | Ship it: ES-module output, `onQuit`/`onError`, the Astro component, poster, CI job, deploy to the personal site with the §7.2 headers | days | The demo plays from the live URL, `curl -I` shows the headers, and CI uploads the bundle |
| **M6** | After v1, each separate: `RenderPixelTests` in headless Chromium; WebGL context-loss restore; `local_multiplayer` on the web; `audio/webaudio/` (blocked on the container decision) | days each; audio a week | Each its own commit, as usual |
| **M7a** | The touch device (P1): `input/touch.{h,cpp}`, fed from `app/win32/` (`WM_POINTER`) and `app/web/` (pointer events), with headless tests in `InputTests` | week | A Windows touchscreen and Chrome's touch emulation both drive a test state; the input tests pin the edges |
| **M7b** | Phones for LineSweeper (D5): on-screen controls drawn by the game over M7a, textures without S3TC, a portrait layout, `touch-action` and iOS viewport handling, and a measured frame on a real phone | weeks | LineSweeper plays on a current iPhone and a mid-range Android phone in their default browsers |

The critical path to the desktop demo is M0 → M1 → M2 → M3 → M5, roughly
**three to four weeks** of work on these definitions. The spread is mostly
S1. M7a can start in parallel with M3, because it needs only M2's window split and can be finished on Windows. M7b follows M5. M6's items are not on the way to phones, except
context-loss restore, which phones trigger more often than desktops.

### M7 in more detail, because it is where most visitors are

Four pieces, in order of how much they are unknown. The first is M7a; the rest are M7b:

- **Touch is an engine device (P1, decided), and it is the real work.** It
  is the third fed device, shaped like the other two so a game asks it the
  same questions. These are the design terms for M7a's first commit, not
  further decisions:
  - **Fed, not read.** Touch arrives as window messages on Windows and as
    events in a browser, so the flow is `app → input`, the same as the
    keyboard and mouse ([ARCHITECTURE.md:274-291](../design/ARCHITECTURE.md#L274-L291)).
    `WindowNotify` gains down, move, up and cancel calls carrying a point id
    and client-pixel coordinates. `input` still depends on `core` and `math`
    only.
  - **Several points, each with a stable id**, latched into frames like the
    keyboard, with `held`, `pressed` and `released` per point, and the same
    rule that an edge needs the device to be live on both frames.
  - **Focus loss cancels every point**, as `Mouse::cancel_buttons` does for a
    lost capture. A finger held on the glass during an app switch is never
    released into the game.
  - **One touch is one input.** Windows and browsers both synthesise mouse
    events from touch. The window drops the synthesised ones: on Win32 by
    handling `WM_POINTER` and ignoring mouse messages that carry the touch
    signature, and in a browser by using pointer events with `pointerType`.
    Without that, a tap on a widget is a touch and a click in the same frame.
  - **The game draws its controls.** LineSweeper's on-screen buttons are
    sample code. They hit-test touch points and press the same sample-level
    buttons its key table already feeds
    ([`play_state.cpp:46-54`](../../samples/linesweeper/states/play_state.cpp#L46-L54)),
    which is why P2 is "not yet". `ui/` still does not depend on
    `input`, so menus reach touch through the game, the way they reach the
    mouse.
  - **Testable without glass.** Like the keyboard, the device is fed, so
    `InputTests` can drive it headlessly. Chrome's device emulation drives
    the web path on a desktop. A real Windows touchscreen and a real phone
    are the only checks of the two windows' translation code. Neither was
    on hand when this was written.
- **Textures, which are cheap for this sample.** The atlas is 21,512 bytes of
  BC2. Either ship it uncompressed (about four times the size, still under
  100 KB) or decode BC2/BC3 to RGBA on the CPU at load when S3TC is missing.
  The decoder is engine code, testable headlessly, and serves every
  no-S3TC device. It is also the cheapest answer
  [content-probe.md](content-probe.md) did not need to give. *Days.*
- **Layout.** Phones are held upright and LineSweeper is laid out for 16:9.
  Its board is tall, so portrait may suit it, but the sample's
  `presentation/layout.h` has to arrange it. This is sample work, not engine
  work.
- **Phone browser quirks and cost.** iPhone Safari has no element Fullscreen
  API. The canvas needs `touch-action: none` so a swipe does not scroll the
  page. The 9,600-particle top-out has to be measured on a phone (R6), not
  assumed.

### Risks, in the order to retire them

- **R1 — the warning sweep (S1).** Unknown size. It blocks everything.
- **R2 — exceptions.** T6 makes throwing load-bearing. `-fwasm-exceptions`
  works in all current desktop browsers (*general knowledge*). Its effect on
  size is part of S2's number.
- **R3 — floating point.** WebAssembly arithmetic is IEEE single and double
  with no x87 and no FMA by default, so it should agree with MSVC
  `/fp:precise` on SSE2. `LineSweeperTests` under Node tests the claim in S2,
  not this paragraph.
- **R4 — S3TC reach.** Desktop first is a decision (D5), and phones are M7, not a gap to
  find on launch day.
- **R5 — pacing.** rAF against a fixed 60 Hz step (§4). Measure on a 60 Hz and
  a high-refresh display before M3 is called done.
- **R6 — the particle field on the low tier.** 9,600 particles a frame,
  single-threaded, in wasm, uploaded with `glBufferSubData`. Desktop is
  probably fine. The constrained hardware this engine positions itself for is
  not measured, and `LineSweeperFrameBench`'s numbers do not transfer.

---

## 10. Out of scope, on purpose

- **Phones in v1.** They are the next phase (M7), not out of scope. They
  are left out of v1 only so that desktop proves the port before touch
  controls change what a game's input is (D5).
- **WebGPU.** A sixth API and a third shading language (WGSL) for a demo
  WebGL2 already serves.
- **Pthreads.** Nothing in v1 fans out, and the cost lands on the host page
  (§7.2).
- **Audio in v1.** LineSweeper has none (§3.5).
- **A browser-hosted editor or tooling.** Permanently out of scope, as before.

---

## 11. The documents this port amends

Each amendment lands in the commit that earns it, under the rule that
`docs/design/` changes in the same commit as the change that conflicts with
it. **None is made in advance.**

| Document | What changes | Earned by |
|---|---|---|
| `CLAUDE.md:3` | "Windows-only" | M3 |
| `CLAUDE.md`, preset table and ctest counts | Two `wasm-*` presets and their test list | M1 |
| `CLAUDE.md`, backend paragraph | D1's answer | M3 |
| `ARCHITECTURE.md:259-272` | The window's move, as D3 decides it | M2 |
| `PHILOSOPHY.md:320-322` | Where it held for a second compiler and a second loop | M5 |
| `docs/website-plan.md:90` | Browser demos stop being "a separate technical project" | M5 |
| `cmake/settings.cmake` | The clang branch, argued as `/fp:precise` is | M1 |
| `gl/backend.h`, `gl/gl_functions.h` | The ES gaps stop being described as future | M3 |
| `ARCHITECTURE.md:274-291` | The fed devices become three: keyboard, mouse and touch | M7a |
| `CLAUDE.md`, Known-absent | Unchanged: P2 kept the action-mapping layer absent | — |
