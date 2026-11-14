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
the agent brief; CI's lint job fails a commit of a pull request that neither
touches this file, nor says `not consumer-visible`, nor is the commit this file
names as the exception to it (`tools/check_changes_rule.py`). Whether the classification is right is
review's and the release audit's.

An entry is written for someone who did not make the change and is moving a
pin across it. It says what kind of change it is. A source break names the old
spelling, the new spelling, and the diff in this repository that made the same
change to a sample, which is the migration to copy. A behaviour change says
what a game sees before and after, and what to do if that matters to it.

An entry added in the commit it describes cannot know that commit's hash, so it
is headed by the date and the commit's subject; `git log --grep` with the
subject finds the hash. The backfill below was written after the fact and is
headed by hash.

An entry that answers an engine request links the issue it answers, so a pin
move across it shows which workaround it retires;
[CONTRIBUTING.md](CONTRIBUTING.md#how-a-request-is-closed) says how the
request is closed.

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

### 2026-11-14 - Port the post stage to Direct3D 12, OpenGL and Vulkan

**Behaviour change.** Direct3D 12, OpenGL and Vulkan implement the post stage and no longer refuse a non-empty chain: with the identity chain set, the processed views draw into an engine-owned scene target the size and format of the back buffer (on Vulkan, of the output image present copies), rebuilt on resize and on device or context loss, and `submit()` draws it through the material before any overlay view, also with no views. Every backend now behaves as Direct3D 11 does, and with no chain nothing changes.

### 2026-11-14 - Add a post stage: an identity chain and an overlay boundary

**New API.** `Renderer::set_post_effects(std::span<const PostEffect>)` sets, outside a frame, the materials `submit()` draws the frame's processed views through before any overlay view; `Renderer::begin_overlay()` declares, inside a frame, the first view drawn over that output unprocessed; `Renderer::set_post_values(PostEffect, const PostValues&)` sets an effect's values. `engine/render/post_effect.h` declares `PostEffect`, whose one value is `identity`, and `PostValues`. A chain is at most one effect. Direct3D 11 and null implement it; Direct3D 12, OpenGL and Vulkan refuse a non-empty chain with `std::logic_error` until they do, and all five implement `begin_overlay`, which with no chain only declares a view. `begin_overlay` needs one view of capacity beyond the frame's own. With no chain, nothing a game draws changes. [docs/effects.md](docs/effects.md) is the design.

### 2026-11-13 - Classify an unflagged Basic Render Driver as a software device

**Behaviour change.** The Direct3D 11 and 12 backends' `Renderer::device_info().kind` is `RenderDeviceKind::software` for an adapter DXGI does not flag as software when its vendor and device numbers (`0x1414`, `0x8c`) or its name identify the Microsoft Basic Render Driver; it was `hardware` there. Adapter selection is unchanged, so a release build still starts on such an adapter and now reports it as `software`. ([#125](https://github.com/matt439/Labrador/issues/125))

### 2026-11-13 - Count allocations across warmed frames outside the backends (REV#126)

**Behaviour change.** `ThreadPool` keeps the slot and Win32 work object it builds for each submission and reuses them after `wait_for_tasks_to_complete`. Once a pool has held as many tasks between two waits as it is given now, `add_task` allocates nothing for a callable that `std::function` stores in place, and a warmed fanned-out `Scene::draw` makes no heap allocation. A task's callable, and its captures, are now destroyed by the wait that follows its submission, on the waiting thread. They used to be destroyed on the worker right after the task ran. The pool's work objects stay open until the pool is destroyed.

### 2026-11-13 - Size the web drawing buffer to the snapped device-pixel box (REV#97)

**Behaviour change.** In the browser, the drawing buffer is the canvas's `device-pixel-content-box` where the browser reports one within a pixel of the CSS size times `devicePixelRatio`. This keeps a canvas at a fractional device-pixel position from being resampled and blurred. `on_window_size_changed` can therefore report one pixel more or less than before. A browser that reports no such box keeps the rounded product.

### 2026-11-13 - Refuse a 32-bit Windows compiler at configure and add an LNK4272 entry

**Behaviour change.** Configure now stops on a Windows build with a 32-bit compiler, such as one from the plain Developer Command Prompt, with `Labrador needs the 64-bit compiler, and this build folder has a 32-bit one`, naming the x64 Native Tools Command Prompt and the build folder to delete. Before this, a game configured and built as 32-bit without any message, and its next build from the x64 prompt failed to link with `LNK4272`. The check is in Labrador's top-level `CMakeLists.txt`, so a project that adds Labrador gets it with no change of its own. Browser builds are unaffected. (REV#106)

### 2026-11-13 - Refuse a NaN audio level above the seam (REV#125)

**Behaviour change.** `SoundBank::play_wave`, `play_effect`, `set_effect_volume`, `set_effect_pitch`, `set_effect_pan` and `AudioResources::set_group_volume` now throw `std::invalid_argument` naming the call and the level for a NaN volume, pitch or pan, before the handle is checked. Before, `clamp` passed NaN through to the device, where Web Audio threw a `TypeError` inside the frame and XAudio2 received it as a level. An infinity still folds into range. (REV#125)

### 2026-11-13 - Refuse non-finite input and a zero or non-finite scale in Camera

**Behaviour change.** `Camera::frame` now throws `std::invalid_argument` for a NaN or infinite world-rectangle position or extent, a NaN or infinite viewport extent, and world/viewport sizes so far apart that the scale would underflow to zero or overflow to infinity. Before this, those inputs returned a NaN camera or one with a zero or infinite scale. `Camera::visible_rectangle` and `Camera::world_position` now refuse a NaN or infinite scale as well as a zero one. A negative scale is still accepted unchanged.

### 2026-11-13 - Refuse an invalid circle in every circle predicate

**Behaviour change.** `circles_intersect`, `circle_triangle_intersect`, `circle_quad_intersect`, `circle_segment_intersect`, `circle_point_intersect`, `test_circle_circle` and `test_circle_triangle` now return false for a circle with a negative radius or a nonfinite centre or radius, as `test_circle_AABB` already did. Before, they squared the radius, so a radius of -3 acted as 3. A rejected circle leaves the point out-parameter unchanged. The new `Circle::is_valid()` states the rule: the centre and radius are finite and the radius is not negative. A zero radius is a point and is valid.

### 2026-11-10 - Sleep between steps and draw nothing while minimised

**Behaviour change.** On Windows, a minimised window no longer draws, and its loop sleeps between fixed steps. Before this change the loop ticked as fast as it could while minimised and rendered every tick into a window with no area. Now `update` still runs at `ApplicationOptions::target_fps`, as `engine/core/state.h` says it does for a state that was not told of the background, but no state's `draw` is called, no frame is presented and `Application::frame_timings().frame` does not advance until the window is restored. The sleep ends when the system's timer says, so a wake can be late and the steps then owed run together in one tick, as they do after any slow frame; the count of updates is kept. A game that advances something from `draw`, or waits in `update` for `frame_timings().frame` to move, sees it stand still while minimised and should move that work into `update` or pause in `on_deactivated`. A window created hidden with `ApplicationOptions::visible = false` is not minimised and draws as before. `WindowNotify` gains `tick_minimized()`, with a default that calls `tick()` and asks for no wait, so a type that implements `WindowNotify` compiles and behaves as it did. The browser build is unchanged.

### 2026-11-10 - Throw from load_manifest and run before initialize (REV#16)

**Behaviour change.** Before `initialize()`, `Application::load_manifest` throws `std::logic_error` reading `Application::load_manifest was called before Application::initialize().` and `Application::run` throws `std::logic_error` reading `Application::run was called before Application::initialize().` Before, each dereferenced a null member and ended in an access violation with no message. `run()` now throws before it enters the first state, so that state's `init()` is not called. A game that calls `initialize()`, then `load_manifest(...)`, then `run(...)` sees no change. A game that meets either message has lost or reordered its `app.initialize();` line.

### 2026-11-05 - Name formats, not file counts, in texture refusals

**Behaviour change: two texture refusal messages.** Under OpenGL, a texture in a format the backend cannot upload (b4g4r4a4_unorm in practice) is still refused by `add_texture_asset` (and so `load_texture_asset`) with a `std::runtime_error`. The message now names the format as well as the texture, as the Direct3D and Vulkan refusals already did: `Texture '<name>' is b4g4r4a4_unorm, a pixel format this backend cannot upload (texture_format.h). ...`, where it read `Texture '<name>' is in a pixel format this backend cannot upload ...`. Under Vulkan, a block-compressed texture on a device without `textureCompressionBC` is still refused at the same point, and its message no longer counts how many of the engine's `.dds` files are block compressed. The exception types, when they are thrown and the texture name in each message are unchanged. A game that matched the old wording should match the texture name or the format name instead.

### 2026-11-05 - Refuse a ScreenResolution outside the four presets

**Behaviour change.** `ResolutionManager::set_resolution(ScreenResolution)` and `ResolutionManager::convert_resolution_to_string` now throw `std::invalid_argument` for a value that is not one of the four presets. Only a cast can produce such a value. Before this change, `set_resolution` stored it with a size of -1 by -1, which reached `ViewportManager` and everything that divides by the size, and `convert_resolution_to_string` named it "-1x-1". `set_resolution` now throws before it changes either the label or the size. A game that casts an integer to `ScreenResolution` should keep the integer between 0 and `ScreenResolution::max`, which is the range ColourWars' option cycling already uses.

### 2026-11-05 - Share the render seam's checks and say each backend fact once (hygiene H15)

**Behaviour change: the Renderer::view out-of-range message.** On every backend, `Renderer::view(index)` with an index outside the frame's views still throws `std::out_of_range`. The message now reads `Renderer::view(<index>) - this frame has <count> views.`; the old trailing sentence `Asking for one nobody set up used to answer with a fullscreen pane and draw a whole extra pass.` is gone. Under Vulkan, a texture refused for a `TextureFormat` value outside the enum, which only a cast produces, now names it `TextureFormat <n>` instead of `r8g8b8a8_unorm`, as Direct3D already did. The exception types and every other result are unchanged. A game that matched the old text should match the `Renderer::view(` prefix, or check the index against the view count before asking.

### 2026-11-05 - Rename Shape::AABB_intersects to bounding_boxes_intersect

**Source break: `Shape::AABB_intersects` is now `Shape::bounding_boxes_intersect(const Shape&)`, and the pointer overload is removed.** To migrate, replace `a.AABB_intersects(b)` with `a.bounding_boxes_intersect(b)`, and replace `a->AABB_intersects(b_pointer)` with `a->bounding_boxes_intersect(*b_pointer)`. The answer is unchanged. No sample called it, and the engine's one caller, the contact pass, is updated.

### 2026-11-05 - Test a point against a Quad as one convex polygon, and pre-check by boxes

**Added: `Quad::points()`, returning the four points as `const std::array<Point2F, 4>&`, the same type `RectangleRotated::points()` returns.** `quad_point_intersect` now tests the quad as one convex polygon with `point_in_convex_polygon`, with the boundary counted as inside, as the other quad and rotated-rectangle routines already did. Before, it tested the two triangles either side of a diagonal. The outer edges are evaluated exactly as before. The only possible difference is a rounding-level change for points within floating-point error of the internal diagonal, which the old split could misplace. `rectangles_rotated_intersect` and `triangle_rectangle_rotated_intersect` now pre-check with a bounding-box test where they ran a full shape-against-box intersection. Their answers are unchanged.

### 2026-11-05 - Move Partitioner from collision to core, beside ThreadPool

**Source break: `Partitioner` moved from `engine/collision/partitioner.h` to `engine/core/partitioner.h`.** The class is unchanged. It splits work for the thread pool and is not part of collision, so it now sits beside `ThreadPool`, and its reference page moved from the collision module to core. Change `#include "engine/collision/partitioner.h"` to `#include "engine/core/partitioner.h"`. Code that reaches it only through `application.h` or `app.partitioner()` needs no change, and no sample includes the header directly. ColourWars includes the old path in five files, among them `game/objects/level_builder.h`, `scripts/acceptance/debug_console/cpu_level.h` and `scripts/acceptance/paint/jump_through_checks.h`; each changes its include when it takes this pin.

### 2026-11-05 - Say what Partitioner is for, and spell out its counts

**Changed: `Partitioner::partition`'s parameters are `element_count` and `partition_count`.** They were `num_elements` and `num_partitions`. Callers are unaffected because only parameter names changed. The three `std::invalid_argument` messages that name the parameters now use the new names, for example `Partitioner::partition: partition_count must be at least 1.` The class also has a comment saying what it is for.

### 2026-11-05 - Name the gamepad button bit once, beside GamepadState

**Added: `gamepad_button_bit(GamepadButton)` in `engine/input/gamepad.h`.** A `constexpr` function that returns the bit a button occupies in `GamepadState::buttons`. Both pad backends and `GamepadState::is_down` now use it, and a game that builds a `GamepadState` for its own edge tests can call it instead of writing the shift itself. No existing behaviour changes.

### 2026-11-05 - Include only what three public headers use

**Changed: `engine/assets/resource_loader.h` forward-declares the four services it borrows.** It names `AudioDevice`, `AudioResources`, `RenderResources` and `Renderer` only as pointers, so it now declares them instead of including `audio_device.h`, `audio_resources.h`, `render_resources.h` and `renderer.h`. A file that includes only `resource_loader.h` and uses those types, or the registry, font, sprite sheet, sound bank and sound group declarations they brought with them, now includes the header it uses. `application.h` still includes all four, so a game that includes `application.h` sees no difference. `application.h` no longer includes `<vector>` and `widget.h` no longer includes `camera.h`; both still arrive through other headers, and `widget.h` now includes `<utility>` for the `std::pair` it uses.

### 2026-11-05 - Answer the H3 review: name the texture in Direct3D format refusals

**Behaviour change, Direct3D only.** On Direct3D 12, a GPU wait that fails while the device is still there now throws `labrador::com_exception` with the HRESULT of the call that failed (the fence signal, `SetEventOnCompletion` or the wait itself), for example `The wait for the GPU to finish failed (HRESULT 8007000E).`. Before, it always reported `E_FAIL` (`80004005`). This applies to texture loading, `Renderer::read_back_buffer`, device resource creation and the wait before a swap-chain resize. A removed device is still reported by its removal reason. Separately, on Direct3D 11 and 12, `add_texture_asset` given a `TextureData` whose `format` is a value outside `TextureFormat` (only a cast can produce one) now throws `std::runtime_error` reading `Texture '<name>' is in a format this backend has no Direct3D 11 format for (TextureFormat <n>).` (or `Direct3D 12`), as the other backends do. Before, it threw `std::invalid_argument` without the texture's name. A game that matched `80004005` should match the real code instead, and one that caught `std::invalid_argument` there should catch `std::runtime_error`.

### 2026-11-05 - Return UiContainer::children by const reference and test the UI setters

**Retyped public name.** `UiContainer::children()` was a non-const member returning a copy of the list; it is now `const std::vector<std::pair<std::string, UiObject*>>& children() const`. A range-for or an `auto` copy compiles unchanged and a const container can now be read. Code binding the result to a non-const `std::vector<...>&`, or modifying the returned list, must take a copy first (`std::vector<std::pair<std::string, UiObject*>> list = row.children();`). The reference is invalidated by `add_child` and `remove_child`. No sample used it.

### 2026-11-05 - Remove unused render API and test what stays

**Source break.** Render members that nothing called outside tests are removed: the two static `Camera::calculate_view_rectangle(world, camera)` and `(world, camera, view)` overloads (write `camera.calculate_view_rectangle(world)`; the member in-place form stays); `ResolutionManager::set_resolution(const std::string&)`, which turned any unknown name into 720p (use the `ScreenResolution` overload); `Colour::brighten` (add to `r`, `g` and `b` yourself, or `+=` a colour); the `SpriteFrame(const Vector2F& position, const Vector2F& size, origin)` constructor (use `SpriteFrame(RectangleI(...), origin)`); `sprite_quad_bounds` (use `sprite_world_bounds`, which at zero units per pixel is the box around the untruncated quad); and the protected `SpriteSheetObject::set_sprite_sheet(const std::string&)` (pass `render_resources()->resolve_sprite_sheet(name)` to the handle form). `Colour::clamp_colours` is now private; every public constructor and operator already clamps. No sample used any of them, so there is no sample migration diff.

### 2026-11-05 - Remove unused math API and test the clamp and lerp contracts

**Source break.** Public math members that nothing called or tested are removed, with no replacement: `sq_dist_point_AABB` (ericson_math.h), `Vector2F::clamp` and `Vector2F::clamped`, the static `Vector2F::angle(const Vector2F&)` (use the member `v.angle()`), `Vector2F::to_unit_vector` and `Vector2F::unit_vector` (use `normalized()`/`normalize()`, which return zero for a zero vector instead of inventing (1, 0)), the `Vector2F::lerp(a, b, const Vector2F& t)` overload (the float-t `Vector2F::lerp` stays), and both `RectangleF::set_position_from_top_right` overloads (set `rectangle.x = right - rectangle.width; rectangle.y = top;`). `Quad::triangle_0`/`triangle_1` are now private (use `triangles()[0]`/`[1]`) and `Triangle::calculate_gradient` is protected. No sample used any of them, so there is no sample migration diff.

### 2026-11-05 - Refuse a pad backend at configure where its API does not exist

**Changed: configure refuses a pad backend on the wrong platform.** Setting `LABRADOR_INPUT_BACKEND=xinput` in a browser (Emscripten) build, or `LABRADOR_INPUT_BACKEND=web` in a native build, now stops configure with a message naming the variable and the value it must have. The render and audio backends were already refused this way. Before, the first error came later and named something else: an unnamed `find_package(directxtk)` failure, or a missing Emscripten header at compile time. The defaults are unchanged, so a project that never sets the variable sees no difference.

### 2026-11-05 - Release GL bootstrap, shader and program objects on every failure

**Behaviour change: one OpenGL start-up message.** When `SetPixelFormat` refuses the pixel format chosen for the window, the OpenGL renderer now throws `SetPixelFormat refused the OpenGL pixel format chosen for this window, with Windows error <n>. ...` instead of repeating the message for a window with no suitable format. Nothing else a game sees changes.

### 2026-11-05 - Name the failed operation at every Direct3D 12 ThrowIfFailed

**Behaviour change: Direct3D 12 failure messages.** Every Direct3D 12 failure the engine reports through `com_exception` now names the operation before the code, such as `The swap chain could not be resized (HRESULT 887A0001).`, where it read `Failure with HRESULT of 887A0001`. The exception type and the HRESULT are unchanged. A game that matched the old text should match the HRESULT digits instead.

### 2026-11-05 - Name the texture, format and operation in Direct3D 11 failures

**Behaviour change: Direct3D 11 failure messages.** When a Direct3D 11 device refuses a texture, for example an optional b4g4r4a4_unorm, `add_texture_asset` (and so `load_texture_asset`) now throws `std::runtime_error` reading `Texture '<name>' is <format> at <w>x<h>, and this device would not create its texture (HRESULT 80070057).` instead of `Failure with HRESULT of 80070057`, as the other backends already did. Every other engine-written Direct3D 11 failure now starts with the operation that failed, such as `The sprite index buffer could not be created (HRESULT 887A0005).`; the type is still `labrador::com_exception`, a `std::runtime_error`. Device creation failures from the upstream device_resources.cpp keep the old wording. A game that matched the old text should match the HRESULT digits instead.

### 2026-11-05 - Drop const from ResourceLoader's reload and load helpers

**Source break.** `ResourceLoader::reload_device_resources()` is no longer
`const`, since it rebuilds every device resource the manifest named: call it
on a non-const loader. No sample needed the change.

### 2026-11-05 - Refuse an unset viewport index, clamp NaN colour channels, skip trig unrotated

**Behaviour change.** `ViewportManager::player_viewport` throws
`std::out_of_range` for an index outside the current layout's viewports (one
for one player, two for two, four for three and four) instead of returning a
fullscreen viewport. Check the index against the layout before asking. A
colour channel that computes to NaN, such as dividing by a colour with a zero
channel, now clamps to 0.

### 2026-11-05 - State the UI module's loud failures at their declarations

**Behaviour change and source break.** `UiContainer::add_child(nullptr)`
throws `std::invalid_argument` instead of dereferencing null.
`FocusGroup::set_focused` checks the slot before the widget, so an
out-of-range slot throws `std::out_of_range` even for a widget the group does
not hold; it returned false. `FocusGroup::refresh_style()` is no longer
`const`, since it writes every held widget's colour: call it on a non-const
group. No sample needed the change.

### 2026-11-05 - Partition into a caller-owned vector so a split-screen frame allocates nothing

**Source break and contract change.** `Partitioner::partition` fills a
caller-owned vector instead of returning one: `ranges =
partitioner.partition(n, k);` becomes `partitioner.partition(n, k, ranges);`.
The vector is cleared first and its capacity reused; a rejected argument
leaves it unchanged. No sample calls it. `Scene::draw` keeps its ranges in a
buffer reused across frames and must not be entered by two threads at once on
the same scene, which its setting of the renderer's view count already
required.

### 2026-11-05 - Give the broad-phase grid defined answers for nonfinite and huge boxes

**Behaviour change.** `BroadPhase` gives defined results for bounding boxes
with NaN, infinite, inside-out or very large fields, matching the all-pairs
sweep: a NaN box forms no pair, an infinite or inside-out box is tested
against every object, and cell coordinates clamp to the 32-bit range. These
inputs were undefined behaviour. Finite, ordinary boxes form the same pairs
as before.

### 2026-11-05 - Bound what a hand-edited JSON file can ask of the loaders

**Behaviour change.** Hand-edited JSON is bounded:

- The asset manifest, sprite-sheet and sound-bank loaders refuse a repeated
  key in any fixed-schema object with `std::runtime_error` naming the file, the object
  and the key. The first value used to win.
- `JsonValue::number` throws `std::runtime_error` for a number outside
  `float`'s range instead of returning infinity.
- `read_sprite_sheet` refuses an animation strip whose `frame_count` exceeds
  `max_texture_dimension`, or is positive with a first frame of zero width or
  height. Such a count could ask for gigabytes.
- Deeply nested JSON is refused with the parse error instead of overflowing
  the stack.

### 2026-11-05 - Refuse an oversized PNG header before decoding, and test the binary readers' refusals

**Behaviour change.** `decode_png` and `read_png_file` refuse a PNG wider or
taller than `max_texture_dimension` (16384, declared in
`engine/render/texture_data.h`), or with a zero side, with
`std::runtime_error` naming the file, before decoding. Such a file used to
decode into a texture no device could take, or fail with `std::bad_alloc`.

### 2026-11-05 - Check ThreadPool's counts and read_file_bytes' failures by name

**Behaviour change and source break.** `read_file_bytes` throws
`std::runtime_error`, not `std::out_of_range`, for a path that exists but
cannot be read: a directory, a locked or unreadable file, or one too large to
address. `std::out_of_range` now means only that nothing is there.
`ThreadPool` throws `std::invalid_argument` unless `1 <= min_num_threads <=
max_num_threads`, and `std::runtime_error` if Windows cannot create the
minimum.

`ByteReader` no longer accepts a temporary buffer, which it would have read
after the buffer was destroyed. `ByteReader reader(read_file_bytes(path),
path);` becomes `const std::vector<unsigned char> bytes =
read_file_bytes(path); ByteReader reader(bytes, path);`. No sample needed the
change.

### 2026-11-05 - Keep handler exceptions inside the Win32 window, and make a closed window inert

**Behaviour change.** An exception thrown by a `WindowNotify` handler on
Windows closes the window and comes out of `pump_until_quit`, or out of the
`Window` constructor, instead of unwinding through user32, including one
thrown from a message sent during construction, `resize_client` or a window
drag. A constructor that throws leaves no window and no registered class.
Once a window has closed, every `Window` call does nothing on both platforms;
on Windows `resize_client`, the full-screen calls and the cursor calls acted
on a destroyed window before.

### 2026-11-05 - Close Application's COM apartment last, and answer calls before initialize

**Behaviour change.** Before `initialize()`, `Application::window()` answers
null and `quit()` does nothing; `set_resolution` and `set_fullscreen` record
the request in the options, and the window opens with it. Each of these
dereferenced a null window before. COM on Windows is now uninitialised after
the audio device, renderer and window are destroyed, not before.

### 2026-11-05 - Make a suspended XInput reader answer every slot disconnected

**Behaviour change.** A suspended `GamepadReader` answers every slot
disconnected on Windows, as it already did in a browser. XInput kept
reporting connected pads with live buttons while the game was in the
background. A game that read pads while suspended now sees none.

### 2026-11-05 - Make the three audio backends agree on one-shots, bad indices and bank opens

**Behaviour change.** The three audio backends now agree:

- A one-shot (`SoundBank::play_wave`) asked for while the game is suspended
  is dropped on every backend; XAudio2 queued it on the stopped engine. The
  null backend does not record it.
- XAudio2 `play_wave` and `create_voice` with a wave index the bank does not
  hold, such as a `WaveHandle` resolved from another bank, throw
  `std::out_of_range` as null and Web Audio do, instead of playing nothing.
- XAudio2 refuses a streaming wave bank (xwbtool `-s`) at load with
  `std::runtime_error` naming the file, instead of failing during play.
  Re-pack it in-memory. Web Audio still plays one.
- Opening a wave bank lets `std::bad_alloc` propagate instead of turning it
  into the error that substitutes a silent optional bank. XAudio2's `Failed to
  open wave bank` message now ends with the reader's own words.
- Null `play_voice` follows DirectXTK's Play: a paused voice resumes keeping
  its loop flag, a playing voice is unchanged, and only a stopped voice takes
  the requested loop, so `is_effect_looping` matches the other backends.

### 2026-11-05 - Refuse unknown keys in a sound bank definition

**Behaviour change.** A sound bank definition (`.json`) refuses a key other
than `waves`, `create_effect_instance_for_each_wave` and
`sound_effect_instances`, and an effect instance refuses a key other than
`name` and `wave`, with `std::runtime_error` naming the file, the object and
the key, as manifests and sprite sheets already did. A misspelled field used
to be ignored. Remove or correct the field.

### 2026-11-04 - Load textures from PNG files

**Addition, and a dependency every game now names.** A `texture` or
`sprite_sheet` asset is read from `name.png` when there is no `name.dds`, so a
picture from a paint program draws with no converter. Any PNG colour type
comes out as 8-bit RGBA with each colour channel premultiplied by its alpha,
rounded to nearest; gamma and colour-profile chunks are not applied. A name
with both files is refused with `std::runtime_error` naming both, and a name
with neither is `std::out_of_range` reading `No file at <name>.dds or
<name>.png.`, which named only the `.dds` before. `engine/render/png_file.h`
declares `read_png_file` and `decode_png`.

The engine now links `lodepng` on every platform, so a game's own `vcpkg.json`
must name it: configure fails at `find_package(lodepng)` without it. Add
`"lodepng"` to its `dependencies`, as the minimal template's change list
says. Its zlib licence asks nothing of a release, so no notice file is added.

### 2026-11-04 - Open and resize the window at any client size

**Addition.** `ApplicationOptions::client_size`, a `std::optional<Vector2I>`,
opens the window at any client size, and `Application::set_resolution(const
Vector2I&)` resizes it to one, beside the four 16:9 presets, which were all a
game could ask for. Either is refused with `std::invalid_argument` when an
extent is below `min_window_width` or `min_window_height`; `validate()` names
the field, and a refused `set_resolution` changes nothing. A size past the
monitor's comes back clamped and the layout follows it. Leaving full screen
returns to the size last asked for, and `set_resolution(ScreenResolution)`
clears it. `ResolutionManager::resolution()` keeps the preset last asked for
unless the size is one of the four. A game that sets neither sees no change.

### 2026-11-04 - Collide circles in the narrow phase

**Behaviour change.** `narrow_phase` measured only the convex polygons and
threw `std::invalid_argument` for a `Circle`, so a `CollisionObject` whose
`shape()` was a circle compiled and then threw from `Scene::resolve` or
`find_contacts` at the first pair whose bounds overlapped. It now measures a
circle against a circle and against each polygon type, under the same
`Manifold` contract: a unit normal from the first shape towards the second, a
positive penetration, touching is no contact, and a nonfinite centre or
radius, or a radius that is not greater than zero, reports no contact. A game
that kept circles out of collision to avoid the throw can now give an object
a `Circle` shape; one that caught the exception sees contacts instead. A
shape whose `shape_type()` is `none` still throws.

## v0.3.0 (2026-11-03)

Of the commits in

```
git log --format=%h v0.2.0..v0.3.0 -- 'engine/*.h' 'engine/*.cpp' engine/CMakeLists.txt cmake/settings.cmake vcpkg.json
```

every one that has no entry below says `not consumer-visible` in its message
except `b7ad202`, which splits the Win32 window procedure into one handler per
message group and keeps each message's handling and result.

### 2026-10-29 - Keep a Win32 window shown through full screen

**Behaviour change, Windows only.** `Window::enter_fullscreen` and
`leave_fullscreen` replaced the window style without `WS_VISIBLE`, so a
shown window counted as hidden afterwards. Leaving full screen then skipped
its show: the window stayed on screen at the monitor's corner with a white
client, and GL drew white in full screen. Both calls now keep the window's
visibility, so `Application::set_fullscreen(false)` returns a framed window
at the requested client size that draws. Hidden windows still stay hidden.
No migration is needed.

### 2026-10-29 - Release a browser game's services after it quits

**Behaviour change, browser builds only.** After `Application::quit()`, or an
exception the web window catches from a frame or input handler and reports
through `Module.onError`, the web window calls the new
`WindowNotify::on_closed()` on a later browser task. `Application` then
destroys its states and every service except the closed window, in the order
its destructor uses. Previously a finished browser game kept its states, its
Web Audio listeners and `AudioContext`, its WebGL context and its whole module
heap for the life of the page. A page that offered another game leaked one
instance per game. The page's `onQuit` and `onError` are still called
synchronously, before the release. State destructors now run in the browser
after a quit, as they do on Windows. A game whose state destructor assumed it
never runs there should check that it only releases what it owns. Nothing may
call into the `Application` or its services after `onQuit`. A Win32 window
never calls the hook, and a `WindowNotify` that does not override it is
unaffected.

### 2026-10-29 - Refuse non-finite sprite-font file metrics

**Behaviour change.** `read_sprite_font_file` requires stored glyph
`x_offset`, `y_offset`, `x_advance` and `line_spacing` to be finite. It now
rejects NaN and either infinity with `std::runtime_error` naming the file
and field, plus the glyph index for glyph metrics. Previously these bits
were accepted and could reach layout. Replace non-finite values in affected
content with the intended finite metrics; every finite value, including
negative values and signed zero, retains its bits. Programmatically
constructed fonts and the generic byte reader retain their existing behavior.

### 2026-10-29 - Compute animation-strip origins before narrowing

**Behaviour change.** `AnimationStrip` computes the complete horizontal
frame origin in wide arithmetic, preserving representable results even
when an intermediate index-times-width product exceeds int. A cached x
outside int range throws `std::out_of_range` during construction; authored
sheet loading reports that failure with its file and strip context.
Previously the multiplication or addition could overflow before caching.
Ordinary strips, timing, zero-count behavior and representable signed
geometry are unchanged; no content migration is required.

### 2026-10-29 - Refuse unrepresentable sprite-font glyph extents

**Behaviour change.** `read_sprite_font_file` rejects glyph rectangle edge
differences outside the engine's int range with a file-named error before
signed arithmetic can overflow. Previously opposite signed endpoints could
overflow while converting the file's edges into a position and size.
Representable geometry and valid fonts are unchanged; no content migration
is required.

### 2026-10-29 - Select UTF-8 source and narrow literals under MSVC

**Compile-option change.** `labrador_settings` now passes `/utf-8` to MSVC.
Source files and ordinary narrow literals use UTF-8 instead of the machine's
ambient code page. Save game source as UTF-8; convert files saved in another
encoding before rebuilding, and review boundaries that expected legacy
narrow-literal bytes. ASCII source needs no conversion. The minimal template's
change list records this for copied projects. Font coverage and the existing
text-language limits are unchanged.

### 2026-10-29 - Preserve an effect's group after failed assignment

**Behaviour change.** If `SoundBank::set_effect_group` cannot store an effect's
first notification membership, its previous group remains unchanged and the
call can be retried. Previously playback could use the failed assignment
while later group-volume updates missed the effect. Successful assignment,
reassignment and notification order are preserved. Valid caller code needs
no migration.

### 2026-10-29 - Keep failed first sound-bank registration retryable

**Behaviour change.** If registering a new bank name through
`AudioResources::add_sound_bank` throws, the name remains absent and can be
retried. Previously a failure storing its notification name could leave the
bank readable while later group-volume changes skipped it. Existing banks
and successful first-add order are preserved, as are replacement and null
emptying through stable handles. Valid caller code needs no migration.

### 2026-10-29 - Keep large-period step fractions below one after rounding

**Behaviour change.** `step_position` now answers the nearest float below one
when a valid large-period ratio rounds to one. Its stated fraction range is
preserved across valid uint64 tick periods, including values above double's
exact-integer range. Ordinary Application periods, ratios already below one,
invalid-input zeros and step counts are unchanged. No caller migration or
timer/FPS policy change is required.

### 2026-10-29 - Name negative authored animation counts before sizing a strip

**Behaviour change.** `read_sprite_sheet` rejects a negative animation-strip
`frame_count` before construction with `std::runtime_error` naming the source,
indexed strip, name and field value. Previously the signed value became an
oversized container-capacity request whose error lacked that content context.
Use zero for an empty strip or a positive authored count. Nonnegative strips
keep their rectangles, timing and looping; the direct `AnimationStrip` API is
unchanged. Valid content and caller code need no migration.

### 2026-10-29 - Reject raw NUL bytes throughout JSON input

**Behaviour change.** `read_json_file` and `parse_json` reject a literal NUL byte
anywhere in the input with `std::runtime_error` naming the caller, source and
byte offset. Previously, a valid JSON root followed by NUL could load while
ignoring that byte and the remaining input. Valid JSON needs no migration:
escaped `\u0000` inside strings and member names is still decoded, trailing
whitespace is still accepted, and `parse_json` reads only the supplied view.
Remove raw zero bytes from malformed content, or encode intended string NULs
with the JSON `\u0000` escape.

### 2026-10-29 - Keep sound-group insertion retryable after allocation failure

**Behaviour change.** If storing a newly added sound group's volume throws,
`AudioResources::add_sound_group` now removes that group's just-inserted name.
Previously the name could remain mapped to a slot that did not exist, reject
a retry as a duplicate and later alias another group's volume. Failed additions
now leave the name absent and retryable, with existing groups and volumes
unchanged. Successful additions and mixing behavior are unchanged. No caller
migration is needed.

### 2026-10-29 - Reject unknown manifest and sprite-sheet keys

**Behaviour change.** The manifest and sprite-sheet loaders previously ignored
extra object members, including misspelt optional fields such as `optionl` and
`orgin`. They now reject unsupported keys at their fixed document, group,
frame, geometry, origin, animation-strip and first-frame objects, naming the
file, object path/index and complete unexpected key. Correct the spelling or
remove the unsupported field when moving a pin; the checked-in minimal
manifest already uses the supported schema. Game-registered kinds and resource
names remain unrestricted values. The generic JSON reader still permits
game-owned dictionaries/metadata; `JsonValue::keys()` adds copied member-name
enumeration without changing its parse or duplicate-member behavior.

### 2026-10-28 - Publish D3D12 replacement textures before their descriptors

**Behaviour change, Direct3D 12 only.** Reloading a texture now completes
wrapper allocation and registry publication before writing its reused
shader-visible descriptor. If that preparation throws, an existing texture
keeps its object and descriptor together. Upload waits and slot reuse are
unchanged, and no caller migration is needed. This adds no general rollback
guarantee for allocating a new name. A refused wrapper allocation is
regression-tested on a real D3D12 device.

### 2026-10-28 - Preserve Vulkan temporary ownership during failures

**Behaviour change, Vulkan only.** A failed texture-wrapper allocation now
releases the prepared image, memory and view. Pixel-shader creation failure
releases the already-created vertex module. Back-buffer read-back retains the
device that allocated its temporary buffer, so recovery and recovery exceptions
clean up through that original device; a lost frame still throws instead of
returning pixels. No caller migration is needed. Each failure path is
regression-tested on a real Vulkan device.

Controller vibration (`Gamepads::vibrate` and `stop_vibration`, with
`Vibration`, `VibrationResult` and `check_vibration` in `gamepad.h`) adds names
only, so it is a note rather than an entry. A request is bounded to
`max_vibration_seconds`, is stopped by the engine when its time passes, its pad
leaves or the reader is suspended or destroyed, and is never replayed on
resume. `submitted` means the pad API took the request, not that a motor
turned: no physical output has been checked on either backend.

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
