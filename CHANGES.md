# Changes

What changed in the engine that a game built on it can see, newest first, with
the commit and the migration. A change is **consumer-visible** when it is any
of these: a source break, where code that compiled against the engine before
the change does not compile after it; a removed or retyped public name; a new
required package, or any change to the dependencies, features, overrides or
`builtin-baseline` in `vcpkg.json`; a moved header; a change to
the compile options or definitions `cmake/settings.cmake` gives
`labrador_settings`, and so gives the game; or a silent behaviour change, where
the game compiles as it did and does something different, such as
[92966b0](#92966b0)'s move of the content root. Everything else, a new public
name included, is not, and is not listed here; the commit history says what
was added. A new free function taking an engine type can still be a source
break: argument-dependent lookup finds it beside a game's own function of the
same name and signature, and the game's unqualified call becomes ambiguous.

## The rule

A commit that touches `engine/**/*.h`, `engine/CMakeLists.txt`,
`cmake/settings.cmake` or `vcpkg.json` either adds an entry to this file in
the same commit or says `not consumer-visible` in its commit message. The
merge procedure in [CONTRIBUTING.md](CONTRIBUTING.md) asks for it, and so does
the agent brief.

An entry is written for someone who did not make the change and is moving a
pin across it. It says what kind of change it is. A source break names the old
spelling, the new spelling, and the diff in this repository that made the same
change to a sample, which is the migration to copy. A behaviour change says
what a game sees before and after, and what to do if that matters to it.

An entry added in the commit it describes cannot know that commit's hash, so it
is headed by the date and the commit's subject; `git log --grep` with the
subject finds the hash. The backfill below was written after the fact and is
headed by hash.

The text of an entry stays in this file. A header comment states the contract
as it is now and never what it used to be
([CONVENTIONS](docs/design/CONVENTIONS.md), *Comments*), so a change described
here is not described again in the header it changed.

There is no deprecation machinery: a name is changed in one commit, every
caller in this repository is changed with it, and the entry here is the notice
([PHILOSOPHY](docs/design/PHILOSOPHY.md), T1).

## Entries

Entries are grouped under the release that carries them, newest first. A
release is a tag, `v` and the `version-string` in `vcpkg.json`, and its heading
is `## v0.y.z (date)`, dated with the author date of the commit the tag is
cut on, which a rebase-merge keeps (`git log -1 --format=%as v0.y.z` reads it
back). An entry added after the newest tag goes under `## Unreleased` above it,
and the commit that prepares the next tag renames that heading to the tag's
([CONTRIBUTING.md](CONTRIBUTING.md), *Cutting a release*). A release heading
with no entry under it carried no consumer-visible change. Moving a pin across
several tags means reading every release after the one pinned, up to and
including the one moved to.

An entry is a heading under a release that names the one commit it describes,
with that commit's subject and date in its first line, and the settle criterion
in PHILOSOPHY counts those dates. Prose under a release heading that is not
under an entry, such as the classification of a commit that needs none, is a
note: it is not an entry and restarts nothing.

From `v0.1.0` on, a change confined to a browser build is an entry like any
other. The backfill leaves such changes out because a game on `4cefb68` has no
browser build to break, and a game on a tag can have one.

`vcpkg.json`'s `version-string` changes at every release, and that change is
not consumer-visible: a game reads nothing from it, and a release is told by
its heading here.

## Unreleased

### 2026-10-27 - Name Direct3D adapters with a portable narrow()

**Behaviour change, Direct3D 11 and 12 only.** Creating a device used to throw
when the adapter reported an empty name or one that was not valid UTF-16. It
now reports the name as it is: empty stays empty, as on OpenGL and Vulkan, and
an unpaired surrogate becomes U+FFFD. The conversion is the new
`narrow()` in `engine/render/text_encoding.h`, `widen()`'s inverse. A game that
relied on the throw to refuse such an adapter can check
`device_info().device_name` itself.

### 2026-10-27 - Remove public members that nothing calls or tests

**Source break.** These members had no caller in the engine, the samples, the
tests or the benchmark, and no test, which a public primitive needs. They are
removed:

| Type | Removed |
|---|---|
| `RectangleF` | `set_position_x`, `set_position_y`, `set_position_x_from_right`, `set_position_y_from_bottom`, both `set_position_from_bottom_left` and both `set_position_from_bottom_right` overloads, both `from_top_left_bottom_right` overloads, `rectangle_i` |
| `RectangleI` | `set_left`, `set_top`, `set_right`, `set_bottom`, `set_top_left`, `set_bottom_right`, `set_top_left_and_bottom_right` |
| `Colour` | `saturate`, `darken`, `invert`, `make_opaque`, `make_transparent` |
| `Camera` | `calculate_intermediate_camera` |
| `UiContainer` | `remove_all_children` |
| `UiTexture` | `set_texture`, `set_position_from_top_right_origin` |
| `Button` | `has_action` |

`rectanglef.h` no longer declares `RectangleI`; include
`engine/math/rectanglei.h` for it. To migrate, set the fields directly
(`rectangle.x = ...`, `colour.a = 1.0f`), or keep a copy of the removed
function in the game. No sample used any of them, so there is no sample diff.

### 2026-10-27 - Spell constants in snake_case

**Source break.** Public constants follow the CONVENTIONS rule that only a
macro is SCREAMING_SNAKE:

| Old | New |
|---|---|
| `Vector2F::ZERO`, `Vector2I::ZERO` | `Vector2F::zero`, `Vector2I::zero` |
| `RectangleF::ZERO`, `RectangleI::ZERO` | `RectangleF::zero`, `RectangleI::zero` |
| `Vector2F::ONE` | `Vector2F::one` |
| `Vector2F::DIRECTION_RIGHT` and the other seven | `Vector2F::direction_right` and so on |
| `PI`, `PI_OVER_2` | `pi`, `pi_over_2` |
| `EPSILON` | `default_epsilon` |
| `MIN_AXIS_ALIGNMENT` (`engine/collision/resolve.h`) | `min_axis_alignment` |
| `Camera::DEFAULT_CAMERA` | `Camera::default_camera` |

`EPSILON` is not plain `epsilon` because that is the name of the parameter it
defaults. To migrate, replace each old spelling with the new one. In this
repository the same edits were made across every sample, including
`samples/minimal/states/hello_state.cpp` and
`samples/minimal/tests/motion_tests.cpp`.

### 2026-10-27 - Move the math library into namespace labrador as LabradorMath

**Source break.** The math types and functions (`Vector2F`, `Vector2I`,
`RectangleF`, `RectangleI`, `Circle`, `Segment`, `Triangle`, `Quad`,
`RectangleRotated`, `Matrix3x2F`, `Shape`, `are_equal`, `clamp` and the
rest of `engine/math/`) move from `namespace mattmath` to
`namespace labrador`, and the CMake target `MattMath` becomes
`LabradorMath`. The headers stay where they were.

To migrate, replace `mattmath::` with `labrador::`, replace
`using namespace mattmath;` with `using namespace labrador;` (or delete it
where `using namespace labrador;` is already there), and link `LabradorMath`
in place of `MattMath`. In this repository the same edits were made across
every sample, including `samples/minimal/CMakeLists.txt` and
`samples/minimal/rules/motion.h`. Code inside `namespace labrador` that called
a math free function with the same name as a member of the calling class must
now qualify it, as `Vector2F::angle_between` does with `labrador::clamp`.

### 2026-10-27 - Remove engine types nothing uses

**Source break.** These public types are removed, with their headers. No
sample, tool or engine code used them:

- `MovingObject` (`engine/core/moving_object.h`)
- `RotationOrigin` (`engine/render/rotation_origin.h`)
- `SoundBankObject` (`engine/audio/sound_bank_object.h`)
- `AnimationObject` (`engine/render/animation_object.h`)
- `CameraTools` (`engine/render/camera_tools.h`)
- `TextDropShadow` (`engine/render/text_drop_shadow.h`) and the widget built
  on it, `UiTextDropShadow` (`engine/ui/widget.h`)

There is no replacement. A game that used one should copy the class from the
previous release into its own source. To play an animation strip without
`AnimationObject`, resolve it with `SpriteSheet::resolve_animation_strip`, keep
the elapsed time in the game's own `update()`, and draw
`AnimationStrip::calculate_frame(index)` through the `SpriteSheet::draw`
overloads that take a source rectangle. `engine/ui/widget.h` no longer includes
`engine/render/text_drop_shadow.h`. It now includes `engine/render/text.h`
directly. No sample changed.

### 2026-10-27 - Keep window resizing from requesting activation

**Silent behaviour change, Windows only.** `Window::resize_client`, including
the path from `Application::set_resolution`, no longer requests activation.
A hidden window stays hidden and inactive when resized; a shown window is
resized without bringing it to the foreground. Previously the size request
could activate even a hidden window. No source migration is needed. A caller
that needs to activate a native window must request that separately.

### 2026-10-27 - Stop rendering after quit and cover Vulkan instance diagnostics

**Behaviour change.** When an update calls `Application::quit()`, the tick
finishes its remaining fixed updates and skips rendering. Previously it could
draw and present after the native window was destroyed or the browser canvas
was closed. On Vulkan the minimal sample's smoke test then failed while
querying the destroyed window's surface (`VK_ERROR_SURFACE_LOST_KHR`).
`frame_timings()` retains the last completed frame. No source migration is
needed; code must not rely on a final draw after calling `quit()`.

**Behaviour change, Debug Vulkan backend.** When the Khronos validation layer
is available, the engine registers its debugger diagnostic callback for
`vkCreateInstance` and `vkDestroyInstance` as well as the calls between them.
Warnings and errors from those two instance operations can now reach
`OutputDebugStringA`. The callback borrows no renderer or texture state. No
source migration is needed; independent validation-layer stdout logging is
still configured separately.

### 2026-10-27 - Require Vulkan 1.2 headers at configure time

**Build requirement clarified.** Configuring the Vulkan backend now requires
headers at least version 1.2, matching the API types and runtime device floor
it already uses. An older SDK fails during configure with a version diagnostic
instead of reaching compilation. The separate SPIR-V-capable SDK `dxc` lookup
and runtime physical-device checks remain required; a header-version pass
does not establish either one.

### 2026-10-27 - Reuse D3D12 submission scratch storage

**Behaviour change.** D3D12 reserves its command-list pointer array with the
configured view capacity during device creation and reuses that storage for
submission. It no longer creates a temporary array allocation on each drawing
frame. The array borrows the views' command lists, closes every recording view
and submits only declared views in order as before. No source migration is
needed; the change establishes allocation reuse, not a measured speedup.

### 2026-10-26 — Identify Vulkan instance-creation failures accurately

**Behaviour change, Vulkan backend.** If instance creation fails, the
`std::runtime_error` now names the failed operation and its `VkResult` instead
of diagnosing every result as a missing Vulkan loader. No source migration is
needed; code matching the old exception text must use the new diagnostic.

### 2026-10-26 - Remove Vulkan's temporary view-prefix allocation

**Allocation behavior.** Vulkan drawing no longer constructs a temporary
per-view vertex-prefix vector on each nonempty submission. Draw order,
skipped/empty view handling and submit exception cleanup remain unchanged.
A warmed Debug CRT regression verifies removal of the observed allocation
attempts; this does not establish driver/system heap behavior or a timing gain.

### 2026-10-26 - Close Vulkan render passes when submission throws

**Behaviour change on failure.** If Vulkan submission throws while recording
a render pass, including a descriptor-allocation failure, it closes that pass
before propagating the original exception. Follow-up read-back, presentation
or abandonment no longer encounters the pass left open by that failure.
Earlier draws are not rolled back, and the failed submission is not replayed.
This adds no atomic-frame, genuine out-of-memory or device-loss recovery
guarantee.

### 2026-10-26 - Retry unavailable XAudio2 output without restarting voices

**Behaviour change.** XAudio2 retries an unavailable default output once a
second while active, including a launch with no output. Suspension defers
recovery; resuming makes the next update eligible immediately. Banks and
persistent voice handles remain valid, but one-shots are dropped and voices
stop on device loss. Games explicitly play persistent sounds again after
recovery. Missing output remains silent; other reset failures propagate.

Six deterministic retry-policy cases pass without creating an audio graph;
native XAudio2 and null audio regressions pass. Real endpoint loss,
reappearance and audible playback still need a manual session.

### 2026-10-26 - Drain submitted scene work and release failed Vulkan uploads

**Behaviour change on failure.** If submitting a scene draw task throws, the
scene waits for tasks already submitted before propagating that failure, so
their borrowed scene, renderer and overlay remain alive. Vulkan buffer
construction releases its buffer and any successful allocation or mapping if
a later step fails; upload-command begin/end failures release their command
buffer before propagating the original error. Successful drawing and uploads
keep their existing behavior.

### 2026-10-26 - Retain aggregate modifier keys until both sides are released

**Behaviour change.** On Windows and in a browser, releasing one Shift,
Control or Alt key keeps the public aggregate key held while its other side
is down. Previously the first release could produce a premature key-up.
Per-window side tracking clears on focus or suspension loss, so a missed
release cannot keep the aggregate held after returning to the application.

### 2026-10-26 - Include overhanging glyph cells in text cull bounds

**Behaviour change.** Text bounds include glyph cells that extend outside the
font's layout rectangle, including negative vertical bearings and drawable
whitespace. A scene no longer culls visible ink at a view edge merely because
the layout rectangle is outside it. `Font::measure()` and text placement keep
their existing layout semantics; a game's spatial index can receive a larger
text rectangle for a font with overhanging cells.

### 2026-10-26 - Keep failed table insertions absent and retryable

**Behaviour change.** When storing a new entry throws, `NameTable::add` and
`Registry::add` remove that entry's name before propagating the exception.
Resolving the failed name therefore throws instead of naming a missing slot
or an unrelated entry added later. A loader can retry the name after handling
the failure. The underlying vector's exception guarantees still apply to the
element or storage type supplied by the caller.

### 2026-10-26 - Expose the engine source revision to consumer builds

**Behaviour change (browser bundles).** A source archive nested in another Git
checkout now records `commit: "unknown"` and `dirty: null` in `BUILD.json`
instead of borrowing the containing repository's identity. Build from a
Labrador checkout to record its commit. Consumer CMake projects can also read
`LABRADOR_SOURCE_REVISION` after adding Labrador: its configure-time commit
and optional `-dirty` suffix, or `unknown`. Reconfigure after source changes
before writing release build information; [README.md](README.md#name-the-engine-source-in-a-release)
shows the usage.

### 2026-10-26 — Make default disabled menu rows dimmer than unfocused rows

**Behaviour change.** `FocusStyle::disabled` defaults to `Colour::dim_gray`
(RGB 105) instead of `Colour::dark_gray` (RGB 169), so disabled rows are darker
than enabled unfocused rows (`Colour::gray`, RGB 128). Games explicitly setting
`FocusStyle::disabled` keep that colour. To retain the previous default, set
`style.disabled = Colour::dark_gray` explicitly.

## v0.2.0 (2026-10-25)

Of the commits in

```
git log --format=%h v0.1.0..v0.2.0 -- 'engine/*.h' 'engine/*.cpp' engine/CMakeLists.txt cmake/settings.cmake vcpkg.json
```

every one that has no entry below says `not consumer-visible` in its message
except `0c15f34`, which changes a comment in `engine/CMakeLists.txt` and
nothing it builds.

#### Add a browser build to the Get Started project

*Add a browser build to the Get Started project* (2026-10-25). Behaviour
change at configure, in a browser build only.

- **Under Emscripten, `LABRADOR_RENDER_BACKEND` defaults to `gl`.** It
  defaulted to `d3d11` on every platform, so a browser preset that did not set
  it compiled the Direct3D 11 backend and failed far from the cause. A game's
  browser preset no longer needs to name the backend; one that names `gl`, as
  this repository's own `wasm-*` presets do, is unaffected. Windows builds
  still default to `d3d11`.
- **`d3d11`, `d3d12` and `vulkan` are refused at configure in a browser
  build**, with a message saying it must be `gl` or `null`, as `xaudio2`
  already was for audio. Each of them failed later before, so no build that
  worked stops working.
#### Keep a debug build running where there is no audio endpoint

*Keep a debug build running where there is no audio endpoint* (2026-10-25).
Silent behaviour change.

- **A debug build no longer stops at start-up on a machine with no audio
  device.** On the XAudio2 backend the engine asks for XAudio2's debug layer
  only when a debugger is attached.
  - Old: every debug build turned the layer on, and DirectXTK sets it to break
    on any error, so with no default audio endpoint - a CI runner, a headless
    session - the game ended at a break instruction (exit code `0x80000003`)
    before its first frame. A release build carried on silent.
  - New: without a debugger a debug build carries on silent too, as a release
    build does; under a debugger the layer and its breaks are as before.
  - Nothing to migrate. A game that wants XAudio2's trace output runs under a
    debugger, which is where a break was ever useful.

#### Keep a scene's collision objects in the order they were added

*Keep a scene's collision objects in the order they were added* (2026-10-24).
Silent behaviour change.

- **A retirement no longer reorders the collision objects that remain.**
  `Scene::end_tick` removes retired collision objects with a stable erase, so
  `Scene::collision_objects()` and the order a scene draws them in are the
  order they were added.
  - Old: a retired object's slot took the last collision object, so after a
    retirement one object drew earlier than before - under objects it had
    been added after - and `collision_objects()` was documented as "no
    particular order".
  - New: the survivors keep their order, and `scene.h` states the whole draw
    order: plain objects as added, then collision objects as added, then the
    overlay, with a later scene over an earlier one.
  - A game that never retires a collision object sees no change. One that
    retires some sees the order it added them in, which is the order it most
    likely meant; a game that relied on the old reshuffle has nothing to
    migrate to, because that order was never promised.

#### Re-point sprites and animations by a resolved sheet pair

*Re-point sprites and animations by a resolved sheet pair* (2026-10-22).
Silent behaviour change.

- **A name setter that throws leaves the object as it was.**
  `TextureObject::set_frame(sheet_name, frame_name)`,
  `AnimationObject::set_animation_strip_and_reset(sheet, strip)` and, through
  the first, `UiTexture::set_texture` now resolve both names before they assign
  either.
  - Old: the sheet moved first, so a frame or strip name the new sheet did not
    have threw with the object holding the new sheet and the old element's
    slot, which reads as some other frame of that sheet or throws again at the
    next draw.
  - New: the throw leaves the sheet, the element, and an animation's frame
    index and frame time as they were. A game that caught that throw and drew
    on sees the old frame rather than a mismatched one.
  - The paired forms the same commit adds, `RenderResources::SheetFrame` and
    `SheetStrip` from `resolve_sheet_frame` and `resolve_sheet_strip`, are the
    handle path for re-pointing from per-frame code; the name forms stay.

#### Name every key, so a game can save a binding and read it back

*Name every key, so a game can save a binding and read it back*
(2026-10-22). Source break, through argument-dependent lookup, and a new file
format.

`engine/input/keyboard.h` gains `std::string_view key_name(Key)` and
`std::optional<Key> key_from_name(std::string_view)`. Each key's token is its
enumerator's spelling, `Key::del` included (`"del"`).

- **Source break, for a game with its own `key_name(labrador::Key)`** called
  unqualified. Argument-dependent lookup on the `labrador::Key` argument now
  finds `labrador::key_name` beside the game's own function, both take `Key`
  by value, and the call is ambiguous, so it no longer compiles. A game whose
  function has another name, or takes another type, is not affected.
  - Old: `key_name(key)`, calling the game's own function.
  - New: qualify the call (`::my_game::key_name(key)`), or delete the game's
    function and call `labrador::key_name(key)`, whose tokens are the
    enumerators' spellings.
  - Template: the same change to the browser window test page, which had its
    own `key_name` and called it unqualified; no sample had one:

    ```
    git show $(git log --format=%h \
        --grep="^Name every key, so a game can save a binding and read it back$" \
        | tail -1) -- tests/app/web/window_page.cpp
    ```
- **The tokens are a file format from this commit on.** A game that saves a
  token reads it back with `key_from_name`. Renaming an enumerator later is a
  source break and a format break in one change, and its entry here says both.

#### Windows 10 version 1607 is the floor

*Keep the client size in a per-monitor-aware process* (2026-10-22). A new
platform requirement, on Windows only.

- Old: no floor was stated. The window's static import of `GetPointerType`
  made it Windows 8 in fact.
- New: Windows 10 version 1607. The Win32 window imports
  `AdjustWindowRectExForDpi`, `GetDpiForWindow` and `GetDpiForSystem`
  statically, so a game built on the engine does not start on an earlier
  Windows: the loader refuses it, naming a missing `user32` entry point.
  Nothing is defined to enforce it at build time, and a game's own code may
  still name anything its SDK declares.
- A game that ships to Windows 8 or 8.1, or to Windows 10 before 1607, has to
  stay on an earlier engine. A game's store page or readme now has a minimum
  to state. A browser build is unaffected.

#### Keep the client size in a per-monitor-aware process

*Keep the client size in a per-monitor-aware process* (2026-10-22). Silent
behaviour change, on Windows only, and only for a process that declares
per-monitor DPI awareness, by manifest or by API.

- Old: the frame around the client area was measured at the system DPI. On a
  monitor whose DPI was not the system's the client came out off the size
  asked for, and a DPI change - the window moved to another monitor, or the
  scale setting changed - left the outer rect alone, so under v2, where the
  system scales the frame, the client shrank or grew by the difference.
  `WM_SIZE` adopted either quietly.
- New: the frame is measured at the window's own DPI, at creation, in
  `set_resolution` and coming back from full screen, so the client is the
  size asked for on any monitor. Under per-monitor awareness v2 a DPI change
  keeps the client size and changes only the frame; a full-screen window
  keeps its size.
- A per-monitor v1 process sees no change: its frame, which the system does
  not scale, is still measured at the system DPI, and a DPI change still
  leaves the window alone, which keeps its client pixels. The rect the system
  suggests to v1 keeps logical size instead, so it is not applied.
- An unaware or system-aware process, which is every sample and the template
  as they stand, sees no change either. No name is added: a game that wants the DPI
  asks the window handle itself.

#### Let F10 and Alt alone be keys, not the system menu

*Let F10 and Alt alone be keys, not the system menu* (2026-10-22). Silent
behaviour change, on Windows only.

- Old: pressing and releasing F10, or Alt on its own, sent the window into the
  system-menu loop. The game stopped ticking until Escape or another Alt, the
  keys pressed meanwhile went to a menu the player could not see, and nothing
  told the game.
- New: both are ordinary keys, read from `Keyboard` as `Key::f10` and
  `Key::alt`, and the game keeps running. Alt+Space still opens the system
  menu and Alt+F4 still closes the window.
- A keyboard-only player who reached Move and Size with bare Alt and the
  arrows now reaches them with Alt+Space. There is no option to restore the
  old behaviour. This is narrower than GLFW 3.4's Win32 window, which by
  default drops every keyboard menu activation, Alt+Space included.
- A game that eats bare F10 itself, with a message hook over the engine's
  pump, can delete it. A browser build is unaffected.

#### Keep a hidden window hidden when it changes full screen

*Keep a hidden window hidden when it changes full screen* (2026-10-22).
Silent behaviour change, on Windows only.

- Old: `Application::set_fullscreen(true)` or `set_fullscreen(false)` on an
  `Application` initialised with `visible = false` showed its window and
  activated it, and so did `Window::enter_fullscreen` and `leave_fullscreen`
  on a `Window` created hidden.
- New: the window stays hidden and inactive, and is resized as a shown one
  would be: to the monitor going full screen, and to the requested client size
  coming back. A hidden window going full screen is restored rather than
  maximised, which nothing in the engine reads.
- A game that relied on `set_fullscreen` to reveal a hidden window has to be
  visible from construction; the engine never shows a window created hidden.
  A shown window, including one a game showed itself, goes full screen as it
  did. A browser build is unaffected.

## v0.1.0 (2026-10-13)

The first release. It carries every consumer-visible change from ColourWars'
pin, `4cefb68`, to the tag, which is the backfill below and nothing after it.
Of the commits since the backfill's end that touch its paths,

```
git log --format=%h 65116c3..v0.1.0 -- engine cmake CMakeLists.txt vcpkg.json
```

`01f9e1a`, `6a713e9` and `97be55f` change comments in public headers and no
declaration, which each message says, and `f8584da` changes
`cmake/check_doc_citations.cmake`, which runs only in a top-level build.

`6a713e9` also changes what `read_sprite_font_file` throws for a `.spritefont`
whose glyph count needs more bytes than the file has left, and that is not
consumer-visible either. A well-formed file reads as it did. On Windows the
reader threw `std::runtime_error` naming the path before, and throws it now
with the count in the message as well. Only in a browser build did the type
change: a count of 2^27 or more wrapped the 32-bit product, passed the check,
and let `std::length_error` escape from the reserve after it, without the path;
it is now the same `std::runtime_error`. Only a malformed file reaches either
throw, and a change confined to a browser build is not consumer-visible to a
game moving from `4cefb68`, for the reason the
[list at the end](#not-consumer-visible) gives.

### Backfill: from 4cefb68 to 65116c3

ColourWars, the engine's first client, is pinned at `4cefb68` (2026-09-26).
This section covers every commit in

```
git log --format=%h 4cefb68..65116c3 -- engine cmake CMakeLists.txt vcpkg.json
```

which is 29 commits. Each was read as a diff. Six have an entry; the other
twenty-three are in the [list at the end](#not-consumer-visible), each with the
reason. `65116c3` itself, and every commit in the range that the command does
not print, touches none of those paths.

#### Moving a pin from 4cefb68, in short

For a Windows game that uses `Application`, which is the shape of every sample:

1. `app.initialize(instance, show_command)` becomes `app.initialize()`.
2. `wWinMain` stops naming the two parameters it no longer uses, or `/W4 /WX`
   stops the build at C4100.
3. A file that used a Win32 name through `engine/app/application.h` includes
   `<Windows.h>` itself.
4. A caller that passed `SW_HIDE` sets `ApplicationOptions::visible = false`.
5. A caller of `Application::window()` casts the `void*` to `HWND`.
6. Code that makes a `Window` directly drops its first two constructor
   arguments, and a class that implements `WindowNotify` itself adds four
   touch overrides.
7. Code that reads a `GamepadReader` directly calls `sample()` first each
   frame.
8. Nothing in the game's own `vcpkg.json` has to change.

The minimal sample's diff in `4175640` is items 1 to 3 in one file:
`git show 4175640 -- samples/minimal/main.cpp`. Two behaviour changes have no
compile error to announce them: touch on a Windows touchscreen
([eaa9b3f](#eaa9b3f)) and a window restored without being activated
([ea90387](#ea90387)).

<a id="eaa9b3f"></a>
#### eaa9b3f: touch is a device of its own

*Feed touch to the engine as the third fed device, from WM_POINTER and pointer
events* (2026-10-11). Source break and silent behaviour change.

- **Source break, for a class that implements `WindowNotify` itself** - one
  that constructs a `Window` directly rather than through `Application`. A game
  that uses `Application` is not affected. `WindowNotify` gains four pure
  virtual functions, so such a class no longer compiles until it declares them.
  - Old: no touch calls.
  - New: `on_touch_down(uint32_t point, int x, int y) const`,
    `on_touch_move(uint32_t point, int x, int y) const`,
    `on_touch_up(uint32_t point, int x, int y) const` and
    `on_touch_cancel(uint32_t point) const`, all `= 0`.
  - Template: `git show eaa9b3f -- bench/linesweeper_frame_bench.cpp`, four
    empty overrides.
- **Behaviour, on Windows: a touch is no longer a mouse click.** The Win32
  window now handles `WM_POINTERDOWN`, `WM_POINTERUPDATE` and `WM_POINTERUP`
  for touch contacts without passing them to `DefWindowProc`, and drops any
  mouse message Windows synthesises from a touch. Before, a tap on a
  touchscreen reached the game through `Mouse` as a left button, and Windows
  could turn a press and hold into a right button; after, it reaches the game
  only through
  `Application::touch()`. A game played with the mouse on a touchscreen reads
  `touch()` as well. A pen is still the mouse.

<a id="ea90387"></a>
#### ea90387: the pads follow the player, and the reader is sampled

*Let the web pads follow the page rather than the canvas, and the fullscreen
flag follow the browser* (2026-10-11). Two silent behaviour changes.

- **`GamepadReader::read(slot)` answers from the last `sample()`.**
  - Old: `read(slot)` asked XInput for that slot when called.
  - New: `sample()` takes a snapshot of every slot and `read(slot)` answers
    from it; before the first `sample()` every slot is disconnected.
  - `Gamepads::poll()` calls `sample()` itself, so a game that reads pads
    through `Application::gamepads()` sees no difference. Code that owns a
    `GamepadReader` and calls `read` directly calls `sample()` once a frame
    before it, as `git show ea90387 -- engine/input/gamepads.cpp` does.
- **Behaviour, on Windows: a window restored without being activated does not
  take the input.** `Application::on_resuming` used to resume the pads, focus
  the keyboard and the mouse and tell the state stack it was active, whatever
  the foreground was. Now the pads, the fed devices and the stack follow two
  facts together, in the foreground and shown, so a window restored behind
  another one stays inactive until it is activated. `State::on_activated` can
  therefore arrive later than it did, at the activation rather than at the
  restore.

<a id="e6b6019"></a>
#### e6b6019: gl/ decodes block-compressed textures it cannot sample

*Decode block-compressed textures on the CPU where a WebGL2 context has no
S3TC* (2026-10-11). Silent behaviour change, `gl` backend only.

- Old: on an OpenGL context without S3TC, loading a BC1, BC2 or BC3 texture
  threw `std::runtime_error` naming the missing extension.
- New: the texture is decoded on the CPU (`engine/render/block_decode.h`) and
  uploaded as RGBA. A game that caught that throw to fall back to other content
  no longer sees it. Every desktop driver the `gl` presets have met has S3TC,
  so on Windows this changes nothing in practice; it is a browser on a phone
  that takes the new path. `vulkan/` still refuses and names the feature.

<a id="e77e9b6"></a>
#### e77e9b6: vcpkg.json gains lodepng

*Run RenderPixelTests in headless Chrome, against the same golden set*
(2026-10-11). `vcpkg.json` change.

- Old: `"doctest"` was the last dependency.
- New: `"doctest", "lodepng"`.
- **A consumer needs neither.** lodepng is found only by
  `tests/render/CMakeLists.txt`, which a consumer does not build, exactly like
  doctest. The manifest the website gives a new project
  (`website/src/components/ConsumerManifest.astro`) leaves both out, and a
  game's own `vcpkg.json` does not add it. Of the manifest's two changes since
  `4cefb68`, this one and [1b16287](#1b16287)'s platform qualifier on
  directxtk, a consumer needs neither.

<a id="4175640"></a>
#### 4175640: initialize() takes no arguments, and the window is split by platform

*Split the shell's window by platform, and run LineSweeper in a browser*
(2026-10-10). Source break, removed and retyped public names, a moved source
file, and one behaviour that is the same by design. This is the entry a game
on `4cefb68` cannot skip. The migration templates are the samples' own diffs in
this commit:

```
git show 4175640 -- samples/minimal/main.cpp            # items 1-3 below
git show 4175640 -- samples/collision/main.cpp          # item 4, SW_HIDE
git show 4175640 -- tools/linesweeper_capture/main.cpp  # item 4, hidden, no wWinMain
git show 4175640 -- bench/linesweeper_frame_bench.cpp   # item 6, Window directly
```

1. **`Application::initialize` loses its arguments.**
   - Old: `void initialize(HINSTANCE instance, int show_command);`
   - New: `void initialize();`
   - Migration: `app.initialize(instance, show_command);` becomes
     `app.initialize();`.
2. **The unused-parameter error that follows.** Once `initialize` is not handed
   them, `wWinMain`'s `instance` and `show_command` are named and unused, which
   is warning C4100 under `/W4` and an error under `/WX`, both of which
   `labrador_settings` gives the game. Drop the names:
   - Old: `int WINAPI wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int show_command)`
   - New: `int WINAPI wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int)`
3. **`engine/app/application.h` and `engine/app/window.h` no longer include
   `<Windows.h>`.** A file that used `HWND`, `MessageBoxW`, `WINAPI` or any other
   Win32 name and got it through either header now fails to compile on that
   name. Include `<Windows.h>` in that file, as each sample's `main.cpp` now
   does. (`engine/render/throw_if_failed.h` still includes it.)
4. **`SW_HIDE` becomes `ApplicationOptions::visible`.**
   - Old: `app.initialize(instance, smoke_test ? SW_HIDE : show_command);`
   - New: `options.visible = !smoke_test;` before the `Application` is
     constructed, then `app.initialize();`. `WindowOptions::visible` is the same
     field for code that makes a `Window` itself. Both default to `true`.
5. **`Application::window()` and `Window::handle()` return `void*`.**
   - Old: `HWND window() const;` and `HWND handle() const;`
   - New: `void* window() const;` and `void* handle() const;`, an `HWND` on
     Windows, which is what `Renderer::create_device` already took.
   - Migration: `static_cast<HWND>(app.window())` where a Win32 call needs one.
6. **`Window`'s constructor, for code that makes one directly.**
   - Old: `Window(HINSTANCE instance, int show_command, const WindowOptions& options, WindowNotify* notify);`
   - New: `Window(const WindowOptions& options, WindowNotify* notify);`
   - Removed: the public static
     `Window::outer_size_for_client(const Vector2I& client_size, DWORD style, DWORD ex_style)`.
     It has no replacement in the public header; it is private to
     `engine/app/win32/window.cpp`, where the window uses it.
7. **The show command, which is the same by design.** The window used to be
   shown with the `show_command` it was handed. It is now shown with
   `SW_SHOWDEFAULT`, which takes the show state from the `STARTUPINFO` of the
   process that started the game, which is where `wWinMain`'s `show_command`
   came from. Launched normally, from a shortcut set to minimised or maximised,
   or by `Start-Process -WindowStyle Hidden`, the window is shown as it was. A
   game that passed a show command other than the one it was given - forcing
   `SW_SHOWMAXIMIZED`, say - no longer can; full screen is
   `ApplicationOptions::fullscreen`, as before.
8. **`engine/app/window.cpp` moved to `engine/app/win32/window.cpp`.** No header
   moved: `engine/app/window.h` is where it was, and a game includes nothing
   else. Only something that names the source file - a build script that lists
   engine sources, a patch, a debugger breakpoint by file - has to follow it.

<a id="1b16287"></a>
#### 1b16287: settings.cmake branches by compiler, and directxtk is Windows-only

*Build the engine for the browser, and run its tests under Node* (2026-10-10).
`vcpkg.json` change and a `cmake/settings.cmake` restructure that leaves an
MSVC build as it was.

- **`vcpkg.json`: `directxtk` gains `"platform": "windows"`.** A consumer needs
  no change for it: a Windows build installs directxtk exactly as before, and
  the qualifier only keeps it out of a browser build. A game that copies the
  engine's manifest may copy the qualifier or not.
- **`cmake/settings.cmake`**: the options `/W4 /WX /permissive- /sdl
  /fp:precise` now apply under `if(MSVC)`, the definitions `UNICODE _UNICODE
  WIN32 _WINDOWS NOMINMAX WIN32_LEAN_AND_MEAN` under `if(WIN32)`, and a clang
  branch gives `-Wall -Wextra -Wpedantic -Werror -ffp-contract=off`, with
  `-fwasm-exceptions` and a 1 MiB stack under Emscripten. A game built with
  MSVC gets exactly the options and definitions it got at `4cefb68`.
  `CMAKE_CXX_EXTENSIONS` is set `OFF` in the scope of the engine's root
  `CMakeLists.txt`, which a game that adds the engine with `add_subdirectory`
  does not inherit.
- The engine finds and links DirectXTK only when `LABRADOR_AUDIO_BACKEND` is
  `xaudio2` or `LABRADOR_INPUT_BACKEND` is `xinput`, which is the default on
  Windows; a new cache variable, `LABRADOR_INPUT_BACKEND`, defaults to `xinput`
  there. A Windows game configures as before.

<a id="92966b0"></a>
#### Before the pin: 92966b0, content resolves beside the executable

*Resolve a game's content beside its executable, not beside its working
directory* (2026-09-17). Silent behaviour change. It is older than `4cefb68`,
so a game pinned there already has it; it is here because no compiler reports
it and a game pinned before it meets it on the next move.

- Old: `Application::load_manifest("./manifest.json")`, and every relative
  `directory` inside the manifest, were resolved against the process's working
  directory.
- New: a relative manifest path is resolved against the directory the
  executable was loaded from, and a relative directory inside the manifest
  against the manifest's own directory. An absolute path is taken as it is.
  `engine/app/content_root.h` holds the rule.
- What breaks: a game launched from somewhere other than its build folder used
  to find, or fail to find, its content by the working directory, and now finds
  it beside the executable. A file the game reads itself, outside the manifest
  and before it, is still resolved by the C runtime against the working
  directory; resolve it the same way with
  `resolved_under(executable_directory(), path)` from
  `engine/app/content_root.h`.
- No sample's call changed: `"./manifest.json"` now means the file beside the
  game, and the samples' diffs in `92966b0` are comments saying so.

<a id="not-consumer-visible"></a>
#### Not consumer-visible

Each of these touches a path the backfill command lists, and none changes what
a game on Windows compiles against, links or sees. A change confined to a
browser build is listed here, because a game on `4cefb68` has no browser build
to break.

- `7c4c371` *Write the merge procedure into the tree, and make the agent brief
  one file*: adds `cmake/check_agent_brief.cmake` and a target inside
  `if(PROJECT_IS_TOP_LEVEL)`, which a consumer's build never enters.
- `0977112` *Answer the components review*: a comment in the root
  `CMakeLists.txt`, inside the top-level block.
- `d169ea9` *Let a Text say what it shows*: `Text` publishes `text()` and
  `colour()`, which were protected; nothing is removed or retyped.
- `66a63f9` *Add components*: the components sample and its page test, inside
  the top-level block.
- `4bdc205` *Load and quit each sample's page in headless Chrome*: page tests,
  inside the top-level block.
- `fbe2e6d` *Run the collision sample in a browser*: sample registration,
  inside the top-level block.
- `5ec2ac6` *Drive the web window with real input in headless Chrome*: the
  browser lookup and a page test, inside the top-level block.
- `fd323be` *Put the sound board in a browser*: sample registration, inside the
  top-level block.
- `1a3058d` *Play sound in a browser through Web Audio*: adds the `webaudio`
  backend, chosen only under Emscripten, and refuses it elsewhere;
  `audio_device.h` gains comments on the browser's gesture rule and no
  declaration. `xaudio2` stays the Windows default.
- `e34f65e` *Read the XACT wave bank in the engine*: adds
  `engine/audio/xwb_file.h`; `audio/xaudio2/` keeps DirectXTK's reader, and
  `audio_device.h`'s change is a comment.
- `5af01f6` *Put LineSweeper's centred text on a whole pixel*: comments in
  `renderer.h` and `sprite_geometry.h` that tell a caller to floor a centred
  point-sampled text position. What a backend draws is unchanged; the comment
  states something that was already true.
- `2b729b1` *Tell the host page when its WebGL2 context goes*: browser only;
  the WGL context's two new hooks do nothing.
- `3c06d82` *Ask WebGL for the debug renderer info only where RENDERER is
  masked*: browser only, and an include reordered in `gl/backend.h`.
- `857991b` *Decode the pixel tests' font atlas before a device does*: counts
  in `SEAM.md` and a comment in `null/renderer.cpp`.
- `dcd222d` *Run the local multiplayer sample in a browser*: sample
  registration, inside the top-level block.
- `f608f7e` *Restore a lost WebGL2 context*: browser only. On WGL,
  `GlTexture`'s destructor now asks `glIsTexture` before deleting, which every
  name a WGL context made answers true; nothing above the render seam changed.
- `ee40b6e` *Draw with WebGL2 in gl/*: `gl/gl_functions.cpp` becomes
  `gl/wgl_context.cpp`, a backend file nothing outside `render/gl/` may name;
  the desktop GL path's drawing changed shape and the golden images stayed
  byte-identical. `renderer.h`, `resource_loader.h` and `texture_format.h`
  changed comments only.
- `37b624d` *Take Windows out of the core*: on Windows, `widen` cuts ill-formed
  UTF-8 where `MultiByteToWideChar` did, `StepTimer` reads `steady_clock`,
  which MSVC implements with `QueryPerformanceCounter` and which was measured
  against it, `ThreadPool` is the Win32 pool as before, and the content root
  joins paths with the backslash it used. `step_timer.h` no longer throws
  `std::exception` from `QueryPerformanceCounter` and no longer needs
  `<Windows.h>` included before it; its public members are unchanged.
- `51f0065` *Clear the tree for clang*: thirteen math and render types declare
  the copy assignment they already had implicitly; `Visual` and `UiTexture`
  keep `TextureObject`'s positioned `draw` overloads hidden with a private
  using-declaration, as name hiding kept them before; the JSON reader opens the
  same files through `std::ifstream`; and `ViewportManager` throws
  `std::invalid_argument` where it threw `std::exception` with the same
  message, which every handler that caught the old type still catches.
- `08c6ca2` *Add runnable collision, audio and local multiplayer guides*:
  sample registration, inside the top-level block.
- `eac9721` *Add LineSweeperCapture*: the capture tool's registration, inside
  the top-level block, and `check_doc_citations.cmake`, which runs only in a
  top-level build.
- `c9d22d5` *Take the history out of the public headers and publish all of
  them*: comments only, in 76 headers and two source files; no declaration
  changed.
- `68b392d` *Build the website's representative slice*:
  `check_doc_citations.cmake` skips `website/`; top-level builds only.
