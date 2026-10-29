# The web port — what it costs, and in what order

> Scoping only, 2026-10-10, read against the tree at `862e08b`. **Nothing here
> has been built, and no Emscripten toolchain was installed on the machine that
> wrote it.** Every claim about this repository was checked by reading it and is
> cited by file and line. Every claim about browsers or Emscripten is general
> knowledge, and is marked where it carries weight. §1 says which is which, as
> [android.md](android.md) §1 does.
>
> **Most of it has been built since**, and §9's *Where it stands* says what,
> milestone by milestone, with what each still owes. A `file:line` citation in
> the scoping text is to `862e08b`, the tree it was read against; the design
> documents are cited by section, because they have moved since and are where
> the port's amendments landed.

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
extension name that Emscripten's `glGetString` reports for S3TC. *(Moot after
M3: the web context enables `WEBGL_compressed_texture_s3tc` by name and never
reads the extension string; §9.)* The browser
facts below are general knowledge: S3TC coverage, timer resolution, Gamepad API
gating, and what COOP/COEP does to a page. They were not tested in a browser.

**Not checked, and not load-bearing.** Behaviour on phones. v1 is a desktop
demo (§10).

---

## 2. The claim this port tests, and how it differs from Android's

[ARCHITECTURE](../design/ARCHITECTURE.md#the-tree), in the platform-code
paragraph after the tree, promises that platform code sits behind
engine-owned interfaces, *"so a second platform is an addition, not a
rewrite"*. At `862e08b` it also named the window as the third case, which
*"moves down a folder without renaming the class or touching a call site"*.
[android.md](android.md) §2 recorded that this claim had never been tested,
and at `862e08b` it still had not: every configuration in `CMakePresets.json`
was Windows x64 under MSVC. The browser is now that test, and PHILOSOPHY's
porting paragraphs ([*Targets and layout*](../design/PHILOSOPHY.md#targets-and-layout))
record where it held and where it did not.

**The browser is a cheaper first test than Android, and it is not a smaller
one.** It is cheaper on three counts, each of which is the Android port's
hardest item becoming its easiest:

- **No new graphics API.** WebGL2 is OpenGL ES 3.0. The GL backend's shader was
  written for exactly that intersection
  ([`gl/sprite_shader.h:26-34`](../../engine/render/gl/sprite_shader.h#L26-L34)).
  Its loader says the only things outside it are one call, one token and the
  content's texture format
  (`gl/gl_functions.h` at `862e08b`).
  Android needed a fifth backend. The web needs about thirty lines of GL, in
  the right place (§3.3). *(Closed in M3: the three are answered once for both
  contexts, and that comment now says nothing is outside the intersection;
  §3.3 and §9.)*
- **The gamepad seam survives.** On Android every device is pushed, and
  [android.md](android.md) §3.3 found that `GamepadReader`'s pull shape does
  not hold there. The browser Gamepad API is polled:
  `navigator.getGamepads()` returns a snapshot on demand. That is exactly what
  `GamepadReader::sample()` asks for once a frame
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
| 3.5 | `audio/xaudio2/` | Playback | **Behind a seam**; the container is read by the engine (D9) |
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

> **Closed in M3, and the code named below is as it stood at `862e08b`.** The
> WGL code is `gl/wgl_context.cpp` (the old `gl_functions.cpp`, with context
> creation, `drawable_size` and the swap moved out of `renderer.cpp`), beside
> `gl/web_context.cpp`. All three gaps are answered on both contexts rather
> than only on the web: the base vertex by re-pointing the attribute offsets
> (`point_attributes` in `renderer.cpp`), BGRA by a CPU swizzle
> (`rgba_from_bgra` in `texture_factory.cpp`), and S3TC by asking the context,
> whose web half enables `WEBGL_compressed_texture_s3tc`. §9 has what M3
> measured. The text below is the analysis as written.

The WGL-specific code is confined, and there is not much of it:

- Context creation is `Impl::create_context(HWND)` with the 1.1 bootstrap in
  `gl/renderer.cpp`, the `HWND`/`HDC`/`HGLRC` members in `gl/backend.h`,
  `SwapBuffers` in `Renderer::end_frame` and `GetClientRect` in
  `Impl::drawable_size`.
- The loader is all of `gl_functions.cpp`, using `wglGetProcAddress`. Under
  Emscripten, GLES 3.0 entry points are linked directly, so the loader has
  nothing to do.

The GLES gaps are the three that `gl_functions.h` lists, and all three are
real on WebGL2:

1. **`glDrawElementsBaseVertex` does not exist in WebGL2**
   (`Impl::replay` in `renderer.cpp`). The base vertex is how one index
   buffer, built for `max_run_sprites`, serves every run. Without it, the
   replacement is to re-point the three `glVertexAttribPointer` offsets at the
   run's first vertex before each draw, where `create_context` points them
   once. That is three calls per run rather than one argument, and draws the
   same thing.
2. **WebGL2 has no BGRA upload** (`GL_BGRA` in `texture_factory.cpp`'s
   `source_format`). This is not hypothetical. LineSweeper's only texture,
   `content/textures/white.dds`, is a 1×1 `b8g8r8a8` (its header's red mask is
   `0x00ff0000`). Either swizzle to RGBA on upload, or re-encode the content.
3. **S3TC is an extension, and on the web it is a per-device lottery.**
   `texture_factory.cpp` asks for `GL_EXT_texture_compression_s3tc` and
   refuses by name without it.
   LineSweeper's font atlas is BC2. *General knowledge:*
   `WEBGL_compressed_texture_s3tc` is present in desktop Chrome, Edge,
   Firefox and Safari, and mostly absent on phones. So a desktop demo
   draws, and a phone would show the backend's own "no S3TC" throw.
   [content-probe.md](content-probe.md) is the long answer for phones. The
   short one for v1 is a desktop-only demo, with a CPU BC decode as a later
   fallback (§9, M7). *(Built in M7b: a context without the extension now
   uploads what `engine/render/block_decode.h` decodes, and §9 says how it
   was checked.)*

Shader: `#version 330 core` becomes `#version 300 es` plus
`precision highp float;` and `precision highp sampler2D;`. A GLES fragment
shader has no default float precision and its sampler defaults to lowp, and at
mediump, which may be half precision, an atlas coordinate cannot name every
texel of a 2048-texel atlas. highp is required of GLES 3.0 fragment shaders,
so it costs no reach ([`gl/web_context.cpp`](../../engine/render/gl/web_context.cpp),
`web_shader_prelude`). The shader body is otherwise the same text under both
contexts, as the header predicted; the other two differences are the base
vertex and the BGRA upload above, and both are answered on WGL as well.
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
`navigator.getGamepads()` once a frame, in `sample()`, which `Gamepads::poll`
calls before it reads any slot. It maps the W3C "standard" layout onto
`GamepadButton`. *General knowledge:* browsers hide pads until the page sees a
button press. That is the "slot occupied on both frames" rule `gamepad.h`
already enforces, so a pad appears with no phantom press. *(As built in M4:)*

- **A pad without the standard mapping is reported absent.** It takes no slot
  and a game sees no pad. Its buttons and axes are in whatever order its
  driver chose, so reading them by index would guess, and the engine does not
  guess. Nor does a standard pad short of the sixteen buttons and four axes
  the engine reads; the guide button is not one of them.
- **Slots are compacted.** The browser's index is its own count and can pass
  four, so a reconnected pad may come back as 5. Each connected standard pad
  takes the lowest free slot when it is first seen and keeps it while it stays
  connected, which is the rule XInput's user index keeps. A fifth waits until
  one of the four leaves.
- **The pads follow the page, not the canvas.** A click beside the canvas
  takes the keyboard, and a player on a pad has not left because of it. So
  `blur` and `focus` on the canvas reach `Application` as
  `on_keyboard_lost` and `on_keyboard_gained`, which release and restore the
  keys and the mouse and nothing else. The pads, and the states' activation,
  follow `visibilitychange`.
- **Both translations are pure functions** in `input/web/standard_gamepad.h`,
  pinned in `InputTests` under the `wasm-*` presets by filling the event
  Emscripten would fill, as `dom_events.h` is in `AppTests`.

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
Android one: one implementation of an existing seam. `open_wave_bank` takes
a bank name, and the only bank format in this tree is `.xwb`. The container
question that [android.md](android.md) §3.2 left open is decided (D9): the
engine reads the format itself, in
[`audio/xwb_file.h`](../../engine/audio/xwb_file.h) beside `dds_file.h`, and
hands a backend 16-bit PCM, which becomes an `AudioBuffer`'s floats in one
pass.
Content and manifests do not change. The backend itself was not v1, and M6
has built it (§9): `audio/webaudio/` makes each wave an `AudioBuffer` and is
the default audio backend under Emscripten, so the `wasm-*` presets build it
in place of `null`.

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
recommendation, not a fallback.** `local_multiplayer` does fan out two views,
on the serial pool, since M6 put it in a browser (§9), which is the
null-pointer path its README already described as equivalent: each slice runs
inside `add_task` in turn.

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
`HWND`. *(M2 hands over the canvas instead, and leaves the context to `gl/`:
D7. M3 built it that way: `gl/web_context.cpp` makes the context from the
canvas's `"!canvas"` target with these attributes, stated rather than left at
Emscripten's defaults, and the window never touches WebGL.)* The drawing buffer follows the canvas's CSS box times
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
here.*

> **Measured in M2, and false under `-fwasm-exceptions`, which T6 needs.** The
> stack is unwound, destructors included, before the first frame, so an
> `Application` that is a local in `main` is destroyed and then ticked. A web
> `main` keeps it in a static; §9 has the measurement and what the shell does
> about it. The rest of this list stands.

The consequences:

- **`run()` never returns on the web, and no destructor runs.** That matters to
  a state that saves on exit. LineSweeper has none. On a task after the
  window closes, `Application` releases its states and services as its
  destructor would ([APPLICATION.md §7](../../engine/app/APPLICATION.md)).
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
`blur` and `focus` call `on_keyboard_lost` and `on_keyboard_gained`, so held
keys are released when focus leaves, exactly as `set_input_focus` does for
alt-tab, and the pads go on reading (§3.4). `fullscreenchange`, and a refused
request's `fullscreenerror`, reach `on_fullscreen_changed`, so
`Application::fullscreen()` says what the browser did rather than what was
asked. The
`webglcontextlost` event goes to the renderer's `DeviceNotify`. The asset
reload path already exists for D3D and Vulkan and is settled in `renderer.h`.
That makes GL a fourth backend that can lose a device, which changes one
sentence in `gl/backend.h`. Wiring it is M6 work. In v1, a lost context
shows the error overlay. *(As built in M3, nothing listens for
`webglcontextlost`: `DeviceNotify` is not called, no overlay is shown, and the
canvas stays blank until the page is reloaded. M6 has since wired it, and not
as written here: there is no overlay, and `DeviceNotify` hears the loss only
once the context is back; §9.)*

**Assets.** Recommend **`--embed-file`** for LineSweeper. Its content is
24,584 bytes of font, the 4,399 bytes of its licence and 132 bytes of
texture. Embedding it in the `.wasm`
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
  `LABRADOR_INPUT_BACKEND=web`. *(M6 made the audio backend `webaudio`.)*
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

- **Expected to run under Node:** `LabradorMathTests`, `CoreTests` (serial pool),
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
  `allowed_channel_drift`. That is a measurement to make, not a premise. It is
  worth doing, because it is the only check that the ES dialect still meets
  the pixel contract. It is not on the critical path for a demo (M6).
  *(Done in M6, without Playwright: a page driven over the DevTools protocol
  by a Node script with no packages, and byte-identical to the set on this
  machine; §9.)*
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
| Content | 29.1 KB, BC2 + BGRA + the font's licence | small | small | large, private |
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

The host is **mattharding.dev**, the owner's personal site: Astro, served as
static assets by one Cloudflare Worker, from a repository of its own. Its
convention for a project's in-browser build is a page of its own, at
`public/demo/<id>/index.html`, which the project's page links to once the file
exists - for this demo `/demo/linesweeper/`, named for the sample it plays. So the host page is a plain
HTML page of the demo's own, not a component inside the site's pages: the
site needs no build step for it, and anything the demo ever needs from the
server can be scoped to `/demo/*`. This repository builds and publishes the
bundle, whose `index.html` is a working example of that page; the site keeps
a page of its own in its own styles, built on the same mount (7.3), and its
`npm run sync-demo` copies the rest of the bundle in beside it. **Putting it
on the site and deploying is the owner's**, in that repository: `npm run
deploy` from its `main` branch. The demo is live, at
<https://mattharding.dev/demo/linesweeper/>. Its live checks of 2026-10-13
were against a bundle built at `c369e8b` (`v0.1.0`)
([web-to-be-tested.md](web-to-be-tested.md) has the headers it served). On
2026-10-29 the host repository's committed `BUILD.json` (host commit
`d433560`) names `1e60fb2` (`v0.2.0`); the live page was not re-checked.

**When to re-sync.** After a change to LineSweeper's source or content, engine
code included in its browser build, the browser shell/mount, bundle/notices
inputs or wasm/toolchain/build settings, prepare and verify a fresh clean
Release bundle before asking the owner to re-sync the host. Until that sync,
the public demo does not represent those changes. Review the relevant diff:
a documentation-only commit can increase the revision distance without
changing the game bytes. A positive count alone is not a deploy instruction.

From this checkout's root, compare the host's committed bundle record with
the existing local Labrador `refs/heads/master`:

```text
python tools/check_demo_lag.py --host-repository C:/Projects/PersonalWebsite
```

Supply the path to the host checkout on the current machine. The command
pins its HEAD and reads only that commit's
`public/demo/linesweeper/BUILD.json`, ignoring an edited working-tree copy.
It names the checkout, host commit and blob path, the recorded bundle commit,
the resolved local comparison commit and the number of commits between them.
It does not fetch: local master can lag upstream. The count includes all
commits, rather than only changes relevant to wasm.

This is a committed-metadata check, not a request to the live site or proof
that production serves the same bytes. It requires a full 40-character commit,
`dirty` exactly `false` and complete local ancestor history; dirty/malformed metadata,
unknown revisions, shallow history and a non-ancestor fail explicitly.
A valid comparison, including a positive lag, exits 0; an invalid or unknown
comparison exits nonzero. Packaging verification in 7.1 remains separate.
No host checkout, bundle or deployment is changed.

### 7.1 Files

The build writes the bundle on every `wasm-*` build, as the
`linesweeper_web_dist` target, into `out/build/<preset>/web-dist/linesweeper/`,
and CI's wasm job uploads `wasm-release`'s as the artifact `linesweeper-web`.
`tools/web_bundle/bundle.mjs` assembles it, with the Node Emscripten's
toolchain already runs. Everything in it but `index.html` goes to
`public/demo/linesweeper/`, the module and its loader in a folder of their
own (7.2):

| File | What | `wasm-release` at `c369e8b`, measured | Brotli, quality 11 |
|---|---|---|---|
| `index.html` | An example of the demo's page: poster, button, the mount (7.3) | 7.8 KB | 2.5 KB |
| `linesweeper.js` | ES module loader and glue; default export `createLineSweeper` | 119 KB | 27 KB |
| `linesweeper.wasm` | engine + game + libc++ + 22 KB of content, embedded | 404 KB | 125 KB |
| `poster.png` | A frame of the game, from `LineSweeperCapture`'s website set | 15.1 KB | 10.9 KB |
| `NOTICE.txt` | What the module links that is not Labrador's, with each licence in full | 92 KB | 9.3 KB |
| `BUILD.json` | The commit, whether the checkout had uncommitted changes, the build type, the Emscripten version, and every file's size raw and under Brotli with its SHA-256 | | |

The module is linked with `-sMODULARIZE -sEXPORT_ES6
-sEXPORT_NAME=createLineSweeper -sENVIRONMENT=web` on top of what every
browser build takes (`-fwasm-exceptions`, a 1 MiB stack, a growing heap,
WebGL2 only). It is a second link of the same objects as the development
page's `LineSweeperSample.html`, which stays as it is, so the sample's page
test still loads what the module runs. **A visitor fetches 153 KB under
Brotli to play**: the loader and the module, after the click. That is the
bundle's own Brotli, at quality 11; mattharding.dev's Cloudflare compresses the
files itself and sends about 183 KB of them, as §9 records. The estimate
this section used to carry, from `x64-release-gl`'s 297 KB executable, was
0.6-1.2 MB for the module, and it came in under that.

`BUILD.json` is how a host tells which engine commit it serves, so a copy into
the site should carry it with the rest. `"dirty": true` means the build had
changes that commit does not hold, and such a bundle should not be deployed.

**`NOTICE.txt` is not optional.** The module statically links libc++,
libc++abi, libunwind, compiler-rt's builtins, Emscripten's musl-derived C
library and its JavaScript runtime, besides RapidJSON, and the page links to
the file. The licence texts are read from the Emscripten install that linked
the module, so they are the versions in it. The repository's `NOTICE` lists
the same set. The bundle also carries that attribution record and the full
Microsoft MIT text for the shared timer code, preserved in
`tools/web_bundle/Microsoft-LICENSE.txt`; the table's older measurement
predates these additional notice inputs.
For a game's copied template, the public
[browser guide](https://labradorengine.com/docs/guides/browser/#gather-the-browser-release-and-its-notices)
gives the corresponding release command using the same bundler.

**Verify before deploying.** From this repository's root, with Emscripten's
environment loaded, run:

```bat
"%EMSDK_NODE%" tools\web_bundle\bundle.mjs --verify out\build\wasm-release\web-dist\linesweeper
```

This read-only check exits 0 only when `BUILD.json` names a full Git commit,
`dirty` is exactly `false`, `build_type` is `Release`, and `emscripten` names
a numeric three-part version (with an optional suffix). It requires records
and regular files for `index.html`,
`linesweeper.js`, `linesweeper.wasm`, `poster.png` and `NOTICE.txt`. Every
recorded file must be directly in the bundle folder, with a matching SHA-256
and raw byte count, and a nonnegative integer Brotli byte count. The Brotli
size is a packaging estimate, not recomputed by another Node/Brotli version.
Every folder entry except `BUILD.json` must be recorded; stale files, subfolders
and symbolic links fail. Additional recorded files receive the same checks.
Missing or malformed metadata, unknown provenance and any failed rule
produce a diagnostic and a nonzero exit. The Debug
bundle fails the build-type rule. No Git checkout, compiler, browser or
network access is needed to verify a copied bundle.

The check verifies the packaging record and its bytes. The commit and build
type are recorded when bundling; they do not prove the sources or flags used
to compile an older module. Rebuild from the intended clean revision before
packaging, check the printed revision against the one intended for deploy,
and run the check on the folder that will actually be copied to the host.
The command verifies LineSweeper's file set; it is not a deploy command or
a verifier for an adopter's differently named game.

### 7.2 Headers the host must send

**Single-threaded, as built, so the list is short:**

- `.wasm` served as `Content-Type: application/wasm`, which
  `instantiateStreaming` requires. *General knowledge:* Workers Static Assets
  types files by extension and maps this one, and `curl -I` against the live
  module confirmed it on 2026-10-13 ([web-to-be-tested.md](web-to-be-tested.md)).
  It is not a header a page cannot start without, which this line used to
  say: Emscripten's loader falls back to fetching the module whole and
  compiling it from an `ArrayBuffer`, and logs the failed streaming compile
  and the fallback as two console errors. Served with `.wasm` as
  `application/octet-stream` on 2026-10-25, the Get Started project's page did
  exactly that in Chrome, then drew and played. A wrong type costs the
  streaming compile and a clean console, not the page. LineSweeper's ES module
  bundle loads through the same Emscripten fallback, but was not served wrong.
  The website's *Put your game on a web host* guide is the public statement of
  this list for an adopter's own page.
- **No COOP/COEP.** That is the point of §3.6.
- **If the site ever sends a Content-Security-Policy**, its `script-src`
  needs `'wasm-unsafe-eval'`, or the browser refuses to compile the module.
  That one does stop the page: on 2026-10-25 the Get Started project's page,
  served with `script-src 'self' 'unsafe-inline'`, aborted with a
  `CompileError` before its runtime was ready, and with `'wasm-unsafe-eval'`
  added it played and quit. The site sends none today; the Worker passes
  asset responses through untouched.
- **Caching, by a versioned folder rather than a header.** The bundle's files
  have fixed names, so the site's `sync-demo` puts `linesweeper.js` and
  `linesweeper.wasm` in `/demo/linesweeper/m/<hash>/`, the hash taken from
  their contents, and points the page at that folder. The Worker sets
  anything under `m/<hash>/` to `Cache-Control: public, max-age=31536000,
  immutable` and the page to `no-cache`, and leaves the rest - the poster,
  the notices, `BUILD.json`, the recording - with the asset store's own
  `public, max-age=0, must-revalidate`, each revalidated against its ETag, so a new build is a
  new path and the page that names it is never stale. `sync-demo` also
  refuses a bundle whose `BUILD.json` says `"dirty": true` and checks every
  file against the SHA-256 recorded there.

Any header the demo comes to need is set by the site's Worker,
`worker/index.js`, scoped to `/demo/*`, so the rest of the site is untouched.
**Only if pthreads are ever added** does that matter: the *page* then needs
`Cross-Origin-Opener-Policy: same-origin` and
`Cross-Origin-Embedder-Policy: require-corp`, which breaks any cross-origin
embed on the same page unless each one opts in.

### 7.3 Mounting it, start-on-click

The public [LineSweeper bundle contract](https://labradorengine.com/docs/guides/linesweeper-bundle/)
owns the factory, distributed files and mounting example for another host.
The [browser page contract](https://labradorengine.com/docs/guides/browser/#the-page-and-what-it-owes-the-module)
owns the generic `Module.canvas`, four optional callbacks and quit-before-removal rules.
Removing a running game's canvas does not stop its loop, WebGL2 context or
window listeners. The mounting example below applies that contract to the
LineSweeper bundle.

The page shows a poster and a button, and **nothing is downloaded until the
click**: the page's own load is the HTML and one 15 KB image. The click gives
`AudioContext` the user gesture it needs when audio arrives, and it is when
the canvas takes keyboard focus. The shape, from
`samples/linesweeper/web/index.html`, which says the rest:

```js
const factory = (await import("./linesweeper.js")).default;  // once
canvas = document.createElement("canvas");                     // every game
canvas.tabIndex = 0;
stage.append(canvas);
await factory({
  canvas,
  onQuit: () => { /* show "Play again" */ },
  onError: (message) => { /* show the reason */ },
  onContextLost: () => { /* say the graphics went, and are awaited */ },
  onContextRestored: () => { /* take that back */ },
});
canvas.focus();
```

**Each game is a new canvas and a new factory call.** A call is a whole
instance - its own memory, its own Module object and its own `Application`,
which lives in a static whose destructor never runs (§9). The engine's
JavaScript reads `Module.canvas` and the page's four callbacks from `Module`, and
the html5 target `"!canvas"` lives in the instance's own table, so all of it
resolves inside the instance the factory made. QUIT stops that instance's
loop and takes its listeners off its canvas, and on the next task the
instance releases its states, audio and renderer, the WebGL2 context
included. "Play again" makes a fresh canvas, calls the factory again, loses
the old canvas's context if the browser still counts it, and drops that
canvas from the page. The module is fetched
once more for the second instance, which the browser's cache answers.

The page sizes the canvas in CSS, at 16:9 and the column's width, and the
drawing buffer follows that box (§4). A visitor without WebGL2 gets the
engine's reason in the status line, through `onError`, which is T6 on a web
page; a loader or a module that cannot be fetched is caught by the page
itself, since the engine never runs. `onQuit` and `onError` are the engine's
two calls to the page about the game, and `onContextLost` and
`onContextRestored` its two about the canvas: the browser took the WebGL2
context away, and gave it back with the game drawing again. Each is optional,
read from `Module` when it is due, and the second pair is called once per
loss, in that order, each from a frame rather than from the browser's event.
Between them the canvas is blank, and a context that never comes back leaves
it blank for good, so what the page says in `onContextLost` is the only word
a visitor gets until the page acts.

**The instance the factory resolves has one function of LineSweeper's own,
`_linesweeper_quit()`.** It makes the quit the pause menu's QUIT row makes,
so `onQuit` follows synchronously and the instance releases itself. Before
the game is running it does nothing. The packaged page uses it when the
context has not returned after ten seconds: **Try again** quits the old game
and mounts a fresh one. A page without it, or a call that does not reach
`onQuit`, falls back to reloading the document. Removing a running canvas
never stops its game.

### 7.4 What a phone sees on the host page

Most visitors open the host site on a phone, so the page cannot just put an
error where the game should be. It decides **before downloading anything**:
if `matchMedia("(pointer: coarse)")` matches, it shows the poster with a line
saying the game asks for a keyboard or a controller for now, and no button,
and a key pressed on that device - a tablet with a keyboard - brings the
button back. The `.wasm` is never fetched on a phone, so a phone visitor pays
for the page and one image.

**The gate stays although the module plays by touch.** The module the bundle
ships links the touch controls and the portrait arrangement (M7b's sample
half, §9), and the development page lets a phone play; the bundle's page
holds them back until a recorded real iPhone pass (`SM-13`) is complete. The
host page's phone layout (`BDP-09`) is in place: `viewport-fit=cover` with the
safe-area insets as padding, a 16:9 stage limited by the viewport's height in
landscape, a portrait stage that takes the height the screen leaves, and a
full-screen button only where the element Fullscreen API exists. Under
emulation, a 390x844 viewport draws a canvas taller than wide and a 1280x720
one a 16:9 canvas, each wholly on screen. Lifting the gate (`BDP-10`) then
removes only the coarse-pointer gate.

**Owner decision, 2026-10-27 (`BDP-10`).** The iPhone's recorded evidence is
sufficient to lift the phone gate after `SM-13` and `BDP-09`; the gate stays
closed until then. No Android phone is available, so `SM-44` remains deferred
as optional future evidence, rather than a prerequisite. This decision adds
no Android support claim.

**On mattharding.dev a phone gets the poster and a recording**: the site's
page offers an on-demand recording of the game, `linesweeper.mp4` beside the
page, with `preload="none"` and playback controls. The owner accepted the
recording's size on 2026-10-27 (`BDP-20`, then 4.3 MB; the regeneration for
`BDP-21` is 2.5 MB); no smaller encode is needed.
It is `LineSweeperCapture`'s footage that the site copies in with
`sync-demo --video` rather than the bundle carrying it, and the Worker answers
byte ranges on it, which Safari needs to play a video. It still fetches no
module. [The live-demo ledger](web-to-be-tested.md#the-live-demo-m5-checked-2026-10-13)
records the emulated no-download check and the remaining real-phone check.

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
| **D2** | How `Application` stops spelling `HINSTANCE`/`HWND` | **`initialize()` takes no arguments.** The Win32 `Impl` gets `GetModuleHandleW(nullptr)` and `SW_SHOWDEFAULT` itself, and `window()` returns `void*`, as `create_device` takes it. One source break for every sample and ColourWars, batched as PHILOSOPHY asks | `application.h`; every sample's `main.cpp`; ColourWars, whose pin, `4cefb68`, is older than this break. Its migration, with the unused-parameter error it brings under `/W4 /WX` and the sample diffs to copy, is [CHANGES.md](../../CHANGES.md)'s entry for `4175640` |
| **D3** | The window move | **A neutral `app/window.h` holding a `Window::Impl`**, with the code in `app/win32/window.cpp` and `app/web/window.cpp`, because the include check rejects the move ARCHITECTURE describes (§3.2) | ARCHITECTURE, *The tree*: the platform-code paragraph |
| **D4** | The clock | **`std::chrono::steady_clock` on every platform**, measured against QPC on Windows before the change lands, so it is a checked swap and not an assumed one | `step_timer.h`, `NOTICE` |
| **D5** | v1's reach | **Desktop browsers first, and phones next, as a planned phase rather than a someday** (§9, M7). Most visitors open the host site on a phone, so v1 has to give a phone a good page, not only a refusal (§7.4) | `docs/website-plan.md`, *Web stack* |
| **P1** | How a touchscreen plays a game | **An engine touch device**, fed from the window like the keyboard and mouse, on **every** touchscreen rather than only phones: Win32 touch through `WM_POINTER` as well as the browser. The owner had wanted touch support regardless of the web port | `input/` gains a device; `WindowNotify` gains touch calls; `ARCHITECTURE.md`'s fed-devices paragraph |
| **P2** | Whether an action-mapping layer comes with touch | **Not yet.** Agreed by the owner. LineSweeper already maps keys onto its own buttons in a sample-level table ([`play_state.cpp:54-64`](../../samples/linesweeper/states/play_state.cpp#L54-L64)), and on-screen controls can feed the same buttons. The engine gains a device, and the binding layer stays in `AGENTS.md`'s known-absent list because PHILOSOPHY's boundary table gives bindings and any rebinding to the game (T1), not for want of a second client: a client that rebinds keeps its own table, which is how report 21 ([input.md](../review/colourwars-reports/input.md)) was declined on 2026-10-22. [android.md](android.md) §3.3 expected touch to force that layer; this plan argues that it does not | `AGENTS.md` known-absent list, only if the answer is yes |

### Made in M2 and M6, confirmed 2026-10-11

All three change a public header or a dependency, so by the rule below they
should have come back here before they were built. Each was the smallest
reading of a decided row that the code forced, and each is one commit to
reverse. The owner was asked on 2026-10-11, had no preference among the
alternatives, and left the call where it had been made, so they stand as
built; the reasoning in each row is why.

| | Decision | Answer as built, and confirmed | What it amends |
|---|---|---|---|
| **D6** | How a caller asks for a hidden window once `initialize()` takes no arguments (D2). Three did, by passing `SW_HIDE`: the collision and local-multiplayer smoke tests and `LineSweeperCapture` | **`ApplicationOptions::visible`**, copied into `WindowOptions::visible`, default true. Hidden is still a whole shell with a device and messages; a browser does not read it | `application.h`, `window.h`, three callers |
| **D7** | Who makes the WebGL2 context: the window (§4) or `gl/`'s web context TU (D1) | **`gl/`, built in M3.** `Window::handle()` is the canvas's html5 target, `"!canvas"`, as `create_device` takes an `HWND`, so the backend that draws makes its own context as WGL's TU does, and a build drawing with null makes none. `gl/web_context.cpp` makes it from that handle | `window.h`'s `handle()`; §4 |
| **D8** | How the browser build reads the golden PNGs, since WIC is Windows' own and a page has no codec it can call synchronously | **lodepng from vcpkg, on every platform**, replacing WIC in `golden_image.cpp`: one codec reading one set, rather than two codecs making two claims about it. It is the format edge T9 buys, and a test dependency only. RenderPixelTests links it and nothing the engine ships does. A regeneration leaves unchanged images alone, so the swap rewrote none of them. One commit to reverse, by restoring WIC on Windows behind the same two functions | `vcpkg.json`, `NOTICE`, `README.md`, the website's consumer manifest (which leaves it out, as it does doctest) |

### Decided, 2026-10-11

| | Decision | Answer | What it amends |
|---|---|---|---|
| **D9** | The audio container: what a backend with no `.xwb` reader of its own plays from (§3.5, [android.md](android.md) §3.2) | **Recommendation (a), an engine-owned reader**: `audio/xwb_file.h` beside `render/dds_file.h`, reading PCM at 8 and 16 bits and MS-ADPCM, decoding both to 16-bit samples; xWMA, XMA, compact entries and big-endian banks throw naming what they are. Content and manifests are unchanged, and `audio/xaudio2/` keeps DirectXTK's reader. The alternative, banks as folders of WAVs, would have changed every manifest and both clients' content for a format the engine can read in one file | `audio_device.h`'s container paragraph; android.md §3.2's amendment; `CLAUDE.md`'s audio paragraph; ARCHITECTURE's audio folder; PHILOSOPHY's porting paragraph |

### Made in M4 to M7, built, for the owner to see

Each of these changed a public header without coming back here first, which
the rule below asks for. None is a new direction: each is the smallest shape
a decided row or a milestone's done-when forced once it met a browser, and
each is one commit to reverse. They are recorded so that the owner sees them
as decisions rather than finds them as diffs.

| | Decision | Answer as built | What it amends |
|---|---|---|---|
| **D10** | What "the game is in the background" means on a page, where a canvas can lose the keyboard while the player is still looking at it with a pad in hand (M4) | **The pads, and touch, follow the page's visibility and the Win32 foreground, not the canvas's keyboard focus.** `WindowNotify` gains `on_keyboard_gained` and `on_keyboard_lost`, which release the keys held and suspend nothing; the Win32 window never calls them, because it has the keyboard exactly while it is active | `window.h`; `application.h`; `gamepad_reader.h`'s suspend comment |
| **D11** | Whether `Application::fullscreen()` answers what was asked or what the platform did, once a browser can leave full screen on Escape or refuse it (M4) | **What the platform did.** `WindowNotify::on_fullscreen_changed` reports it, so an options menu reading the flag shows the truth; a Win32 window changes it only when asked and never calls it | `window.h`; `application.h` |
| **D12** | How the pad seam meets an API that has to be asked for every pad at once (M4) | **`GamepadReader::sample()` takes one snapshot a frame, and `read()` answers each slot from it**, so the four slots of a frame are one moment; a pad keeps its slot while connected, and `input/web/` hands a new pad the lowest free one rather than the browser's own index | `gamepad_reader.h`; `gamepads.h`; both pad backends |
| **D13** | What a caller of the audio seam sees before a browser allows sound (M6) | **A one-shot is dropped and a persistent voice answers playing and starts when sound is allowed**, stated in `audio_device.h` as the caller's contract; the backend listens for the gesture itself, so the seam grows no verb only a browser needs, and a key that does not grant activation, Escape, does not count | `audio_device.h` |
| **D14** | Whether a device without S3TC refuses block-compressed textures or draws them (M7b) | **`gl/` decodes them on the CPU** through `engine/render/block_decode.h`, engine code so every device that falls back rounds the same way; Vulkan still refuses and names the feature. `texture_format.h` and `texture_data.h` say a backend may ask for the pixels, and is never handed them behind its back | `block_decode.h` (new); `texture_format.h`; `texture_data.h`; `SEAM.md` |
| **D15** | Where point-sampled text may be placed (found by the capture tool's GL difference) | **Off a whole pixel is fine, and exactly half way is not**: `renderer.h`'s `draw_text` tells a caller to floor a centred position, and LineSweeper does | `renderer.h`; `sprite_geometry.h`'s comment |

The touch device and its `WindowNotify` calls are P1, and the wave-bank reader
is D9, so neither has a row of its own here.

### Open

Nothing. Anything else this plan meets that would change a design document, a public header or a rule in `CLAUDE.md` comes back here as a new row before it is built.

---

## 9. Milestones, and what to spike first

### Where it stands, 2026-10-13

**Every milestone's code has landed, the demo is live, and what is left is
hardware and browsers.** [web-to-be-tested.md](web-to-be-tested.md) is that list as
boxes to tick. M1 and M2 are done. M3 is met in Chrome. In Firefox,
LineSweeper's development page has been checked in headless Firefox 157 -
its drawing, its keys and its way out (M3, below) - and headless Firefox 158
has run the other five samples' pages (draw calls submitted, trusted keys,
quit) and `WebWindowTests`' 39 steps over WebDriver BiDi, with no pixel read
back and nothing listened to; that list's
[Firefox section](web-to-be-tested.md#firefox-sample-checks-web4) has the
runs. M3 owes Safari and R5's pacing on real displays. M4's pad path has been driven by a
page script's fake pad and owes a physical one. M5 is done: the module, the
bundle, its page and CI's upload are built here, and the owner has deployed
the demo to <https://mattharding.dev/demo/linesweeper/>, from a bundle built
at `c369e8b` (`v0.1.0`), where on 2026-10-13 it played in headless Chrome,
calm top-out included, its files matched `BUILD.json` and `curl -I` showed the
§7.2 headers; Firefox and Safari on the live page are still to be checked. M6's items are all done. M7a's touch device
has been driven by Chrome's touch emulation and by touch injected into a
Win32 window, and owes a real touchscreen. M7b's controls, portrait layout,
page handling and texture path are built and emulated, and owe a real iPhone,
a real Android phone and a measured frame on one. All six samples run in a
browser, and every page the build makes is a ctest entry where configure
finds a Chromium. The paragraphs below are in the order the work landed, each
as it was written when it did.

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

**M1 departed from its row below in two places, and both have closed.** The
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
visibility (§3.4). That last one was a bug a pad-only player could not get out
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
null backend - `RenderTests`' `null_tests.cpp`, `SceneTests`'
`fanout_tests.cpp` and `cull_tests.cpp`, `LineSweeperViewTests`'
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

- **§4's frame loop premise was false.** Simulating an infinite loop unwinds
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

**M3 draws LineSweeper in Chrome and Firefox, and Safari is owed.** D1 is as
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
- **highp, not the mediump §3.3 first proposed.** A GLES fragment shader has no
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
- **Owed before M3 is done.** Safari, which no machine here runs; the owner's
  2013 MacBook Air is the candidate. And R5: headless Chrome gave 181
  animation-frame callbacks in 3 s, a median of 16.7 ms between them (14.7 to
  16.9), which is a headless measurement and not the judder measurement on a
  60 Hz and a high-refresh display that R5 asks for.
- **Size, with `gl/` drawing**, at `wasm-release`: the module is 356,750 bytes
  (113,085 under Brotli) and its loader 107,466 (25,187), so WebGL adds about
  4 KB to the module and 5 KB to the loader under Brotli. The module is below
  §7.1's estimate and the loader inside it.

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
- **The golden set is read through lodepng on every platform** (D8, below).
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
  `golden_image.cpp` holds the measurement, and the null-renderer step after
  this one in `ci.yml` now runs whatever the pixel step says.

**M6's second item is done: a lost WebGL2 context is restored, and the shell
hears it.** `RenderContextLossTests` pins it in headless Chrome, and
LineSweeper was played through one: the board, the pieces and the text drew
before the loss, the canvas was blank while it was lost, and after the
restore all of it drew again from the reloaded textures and font atlas, keys
still played, and neither `onError` nor a WebGL warning appeared. It departs
from §4's sketch in two places:

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
heard both of its halves (§7.3). A context that is never restored otherwise
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
`add_task` in turn, which is §3.6's serial pool doing the fan-out it was
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
copies them to; at 70 KB the bank is far below the megabyte at which §5 would
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
§7.4 is the host page's. `LineSweeperViewTests` pins the arrangement and the
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
its own at `/demo/<id>/`, so §7 was rewritten for it: the demo is a plain
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

### Packaged-page verification, 2026-10-27

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
  *(Not run as a throwaway: M3 absorbed it, and neither question arose, as
  "Where it stands" records.)*

### Milestones

| | Milestone | Size | Done when |
|---|---|---|---|
| **M0** | Spikes S1–S3 | days | Warning count known; LineSweeperTests green under Node; one quad in three browsers |
| **M1** | Build and core: presets, `settings.cmake` clang branch, vcpkg qualifier, input-backend variable, serial pool, clock (D4), `widen`, `executable_directory` | days (+ S1's clean-up) | `ctest --preset wasm-debug` runs the §5 list green; every Windows preset unchanged and green |
| **M2** | The shell: D2 and D3, `app/win32/` moved with no behaviour change, `app/web/window.cpp` (canvas, callbacks, main loop, lifecycle, resize) | week | LineSweeper reaches its first frame in a browser and takes keys; Windows samples behave as before (look at them, per the see-it-on-screen rule) |
| **M3** | WebGL2 in `gl/` (D1): context TU, ES shader, base vertex, BGRA swizzle, S3TC via WebGL | days | LineSweeper plays end to end in Chrome, Firefox and Safari on desktop; `RenderPixelTests` still green on `x64-debug-gl` |
| **M4** | Gamepad: `input/web/gamepad_reader.cpp`, sampled once a frame, compacted slots, pads following page visibility rather than canvas focus, and the fullscreen flag following the browser (code landed and driven by a page script's pad; a physical pad is the open check) | hours | A pad on the standard mapping plays LineSweeper |
| **M5** | Ship it: ES-module output and the bundle (done), `onQuit`/`onError` (done), the demo's own page and poster (done), CI job (done), deploy to mattharding.dev's `/demo/linesweeper/` with the §7.2 headers (done, the owner's; live and checked 2026-10-13) | days | The demo plays from the live URL, `curl -I` shows the headers, and CI uploads the bundle |
| **M6** | After v1, each separate: `RenderPixelTests` in headless Chromium (done); WebGL context-loss restore (done, and the host page told of both ends); `local_multiplayer` on the web (done); the web window driven by real input in headless Chromium, `WebWindowTests` (done); `audio/webaudio/` (done; its container is D9); `samples/audio` on the web (done); `collision` on the web (done); each sample's page a ctest entry (done). **All done** | days each; audio a week | Each its own commit, as usual |
| **M7a** | The touch device (P1): `input/touch.{h,cpp}`, fed from `app/win32/` (`WM_POINTER`) and `app/web/` (pointer events), with headless tests in `InputTests` (code landed; Chrome's emulation drives `minimal` and `WebWindowTests`, and a real Windows touchscreen is the open check) | week | A Windows touchscreen and Chrome's touch emulation both drive a test state; the input tests pin the edges |
| **M7b** | Phones for LineSweeper (D5): on-screen controls drawn by the game over M7a, textures without S3TC (done: `block_decode.h`), a portrait layout, `touch-action` and iOS viewport handling, and a measured frame on a real phone (controls, portrait layout and the development page's viewport handling landed; headless Chrome's emulation drives them, and real phones and the measured frame are open; the bundle's host page still asks a phone for a keyboard, §7.4) | weeks | LineSweeper plays on a current iPhone and a mid-range Android phone in their default browsers |

M7b includes both phone platforms as a broader target. The current phone gate
follows §7.4's owner decision: `SM-13` and `BDP-09` remain required, and
Android's `SM-44` remains deferred optional evidence.

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
    keyboard and mouse (ARCHITECTURE's fed-devices paragraphs, in
    [*The tree*](../design/ARCHITECTURE.md#the-tree)).
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
- **Textures, which were cheap for this sample, and are done.** The atlas
  is 24,584 bytes of BC2, and the choice was to ship it uncompressed or to
  decode BC1-BC3 to RGBA on the CPU at load when S3TC is missing. The second
  is built (`engine/render/block_decode.h`): engine code, tested headlessly,
  serving every no-S3TC device and every file rather than one atlas, which
  is the cheapest answer [content-probe.md](content-probe.md) did not need to
  give. "Where it stands" has how it was checked.
- **Layout, which is done.** Phones are held upright and LineSweeper was laid
  out for 16:9. Rather than moving any of `presentation/layout.h`'s
  positions, `presentation/arrangement.h` picks a camera each tick from the
  buffer's aspect: landscape frames the whole 1280x720 space, and portrait
  frames the board block across the width with a band under it for
  `presentation/touch_controls.h`. It is sample work, and nothing in
  `engine/` changed. "Where it stands" has how it was checked, and the
  sample's [README](../../samples/linesweeper/README.md), *A phone held
  upright*, has why.
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
  find on launch day. *Retired for content:* a context without S3TC decodes
  the blocks on the CPU (M7b), so a phone's browser draws the same art and
  text at four to eight times the texture memory. What is left of it is that
  memory, on a real phone, which nothing has measured.
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
- **Pthreads.** LineSweeper never fans out, and `local_multiplayer`, which
  does, runs its two views one after the other on the serial pool (§3.6). The
  cost of threads would land on the host page (§7.2).
- **Audio in v1.** LineSweeper has none (§3.5). M6 has since built
  `audio/webaudio/`, so a client with sound has it in a browser.
- **A browser-hosted editor or tooling.** Permanently out of scope, as before.

---

## 11. The documents this port amends

Each amendment lands in the commit that earns it, under the rule that
`docs/design/` changes in the same commit as the change that conflicts with
it. **None is made in advance.**

| Document | What changes | Earned by |
|---|---|---|
| `CLAUDE.md:3` | "Windows-only": amended in M3, to Windows and, through Emscripten and WebGL2, the browser | M3 |
| `CLAUDE.md`, preset table and ctest counts | Two `wasm-*` presets and their test list; amended again in M3, whose row names `render/gl/` and the null-gated sources it does not compile | M1 |
| `CLAUDE.md`, backend paragraph | D1's answer: amended in M3, naming `gl/`'s two context files as the one exception to "at most one more" | M3 |
| ARCHITECTURE, *The tree*: the platform-code paragraph | The window's move, as D3 decides it: amended in M2 | M2 |
| PHILOSOPHY, *Targets and layout*: the porting paragraphs | Where it held for a second compiler and a second loop: amended in M5's engine half, a paragraph after the audio one, with the S1 sweep, D2 and `Application` in a static | M5 |
| `docs/website-plan.md`, *Web stack* | Browser demos stop being "a separate technical project": amended in M5's engine half, naming mattharding.dev as the host | M5 |
| `cmake/settings.cmake` | The clang branch, argued as `/fp:precise` is | M1 |
| `gl/backend.h`, `gl/gl_functions.h` | The ES gaps stop being described as future: amended in M3, with D1's one backend and two contexts in `backend.h`'s header comment | M3 |
| `README.md`, the lead and Building | "Windows only" becomes Windows and a browser: amended in M3 | M3 |
| `engine/render/SEAM.md` §1, §3, §8 | `gl/`'s two contexts, and device loss on a WebGL2 context: amended in M3 | M3 |
| `ARCHITECTURE.md`, the tree and the backend paragraph | `gl/` as one backend with a context file per platform: amended in M3 | M3 |
| [android.md](android.md) §6 | An amendment, not a reversal, of *"a GLES backend as a hedge"* (D1): amended in M3 | M3 |
| ARCHITECTURE, *The tree*: the fed-devices paragraphs | The fed devices become three, keyboard, mouse and touch, and neither window lets a touch arrive again as the mouse: amended in M7a | M7a |
| ARCHITECTURE, the tree and *Modules*: the audio paragraph | `audio/webaudio/` beside `xaudio2/` and `null/`, and `xwb_file.h`: amended by D9 and M6 | M6 |
| PHILOSOPHY, the audio porting paragraph | The container answered as textures answered it, by the engine reading the format: amended by D9 | M6 |
| [android.md](android.md) §3.2 | The container question it left open is decided (D9): amended | M6 |
| `CLAUDE.md`, the audio paragraphs | Three audio backends, and what `AudioTests` checks under `wasm-*`: amended in M6 | M6 |
| PHILOSOPHY, *Targets and layout*: the targets table; ARCHITECTURE, *The tree* (`tools/web_bundle/`); and `NOTICE` | `LineSweeperModule`, and what the module links statically: amended in M5's engine half | M5 |
| `CLAUDE.md` and `README.md`, the samples | All five samples have a browser build and a page test: amended as each landed | M6, M7a |
| `CLAUDE.md`, Known-absent | Unchanged: P2 kept the action-mapping layer absent | — |
