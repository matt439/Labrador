# Website content inventory

**4 October 2026 · at `383b14c`**

What exists that the website's documentation can be built from, module by
module; what needs adapting; and what has to be written new. This is the
inventory [the website plan](website-plan.md) asks for before the first
release's coverage is agreed. Like a survey, it is dated and is not updated as
items land.

## How it was taken

Every public header was read: the `.h` files directly in each of the ten module
folders, leaving out the backend folders (`render/{d3d11,d3d12,gl,vulkan,null}/`,
`audio/{xaudio2,null}/`, `input/xinput/`), which are not API. Counts of lines
were measured. **Whether a header contains history was judged by reading**,
against the three kinds of comment in [CONVENTIONS](design/CONVENTIONS.md)
(*Comments*): a sentence about what the code or the comment used to be. A phrase
search does not find it reliably - "this replaced", "used to" and "finding #"
together match 6 headers - and four quoted examples were checked against the
source before this was written.

## The headers

94 headers, 9,631 lines: 4,913 comment lines against 3,697 others, which
include braces and includes. History of the code was found in 54 headers, and
a clause or a pointer to it in 11 more.

| Module | Headers | Comment : other | With history | Used by `minimal` | Used by LineSweeper |
| --- | --- | --- | --- | --- | --- |
| math | 16 | 668 : 636 | 8, +2 minor | `Vector2F`, `RectangleF`, `Matrix3x2F` | `Vector2F`, `RectangleF`, `RectangleI` |
| core | 11 | 540 : 520 | 6 | `State`, `StateContext` | `State`, `StateContext`, `GameObject` |
| collision | 9 | 375 : 151 | 7 | - | - |
| render | 34 | 1,434 : 1,289 | 18, +5 minor | `Label`, `Text`, `Colour`, `Viewport` | `DrawList`, `RenderResources`, `Label`, `Colour`, `Viewport` |
| scene | 1 | 176 : 80 | 1 | `Scene` | `Scene` |
| input | 6 | 545 : 297 | 3, +2 minor | `Keyboard`, `Gamepads` | `Keyboard`, `Gamepads`, `Direction`, `DirectionRepeat` |
| audio | 4 | 254 : 151 | 3 | - | - |
| ui | 4 | 289 : 223 | 3, +1 minor | - | `FocusGroup`, the widgets, navigation (the pause menu) |
| assets | 6 | 235 : 153 | 2, +1 minor | the manifest, through `content/` | the manifest, through `content/` |
| app | 3 | 397 : 197 | 3 | `Application` | `Application` |

Three things follow from the table.

- **The contracts are written; the reference is not ready to publish them as
  they stand.** Two thirds of the headers would put the history of the code in
  front of a reader looking for its contract. The website's reference
  prototype shows exactly that on `scene.h`, and its introduction page
  (`website/src/content/docs/docs/reference/index.mdx`) sets out the decision
  this forces.
- **Collision and audio have no client in this repository.** Neither sample
  uses them, so a guide to either needs a worked example written first.
  Outside the tests, collision is used only by `bench/scene_bench.cpp`, and
  audio only inside the engine (the shell and the asset loaders).
- **Split-screen has no public client.** Both samples set `view_capacity = 1`
  and add one view. Multiple views are exercised only by
  `tests/scene/fanout_tests.cpp`, `tests/render/pixel_tests.cpp` and
  `bench/fanout_bench_null.cpp`. A local-multiplayer guide waits on the public
  demonstration the plan already lists as needing to be chosen or made.

Two public types are used nowhere outside their own files, by any sample, test
or engine code: `MovingObject` (`engine/core/moving_object.h`) and
`RotationOrigin` (`engine/render/rotation_origin.h`). Thirteen headers in all
are included by no sample and no test, though most of their types are reached
through other headers.

## What can be adapted

Prose that already explains something a reader needs, and where it is. Header
line numbers are at `383b14c`.

| Topic | Concepts or Guides page | Existing material |
| --- | --- | --- |
| Starting a project | Get Started (written) | `README.md` *Building* and *Using Labrador in a project*; `samples/minimal/` |
| What a game is to the engine | Concepts | PHILOSOPHY *The object model*, *Structural types*, *Services and lifetimes* |
| States and the stack | Concepts | `core/state.h:38-93`; `core/state_context.h:42-49, 103-124`, with a typed-result example; both samples |
| The tick: update, collision, end of tick, draw | Concepts | `scene/scene.h:187-250`; `samples/minimal/states/hello_state.cpp:85-107` |
| Drawing, views and cameras | Concepts | `render/renderer.h:109-198` (the draw list), `223-362` (the frame); `render/camera.h`; `render/camera_tools.h` |
| Names, handles and resources | Concepts | `core/registry.h:24-48`; `core/handle.h:5-15`; `render/render_resources.h:16-39` |
| Sprites and text | Guide | `render/sprite_sheet.h:15-102`; `render/font.h:11-48`; `render/label.h:13-27`; LineSweeper's README, *The whole screen is one white texel* |
| Input | Guide | `input/keyboard.h:9-47, 149-178`; `input/gamepad.h:9-16, 47-154`; `input/direction.h:39-152`; LineSweeper's README, *Input is read as held* |
| Menus | Guide | `ui/navigation.h:28-70`; `ui/focus.h:56-80, 128-145`; `ui/widget.h`; LineSweeper's pause screen |
| Assets and the manifest | Guide | `assets/asset_manifest_loader.h:7-29`, with an example; `assets/resource_loader.h:14-55`; `assets/json.h:18-30` |
| Collision | Guide | `collision/collision_layer.h:7-39`; `collision/contacts.h:13-79`; `collision/resolve.h:7-89`; `collision/narrow_phase.h:40-86`. **No sample.** |
| Audio | Guide | `audio/sound_bank.h:11-31, 75-134`; `audio/audio_device.h:26-80`. **No sample.** |
| The renderer seam | Design, for backend authors | `engine/render/SEAM.md` |

## What exists that is not for the website

- **Historical:** `docs/review/` (101 files), `docs/survey/` (2),
  `docs/performance/` (4), `docs/repo-split.md`, `docs/file-length-audit.md`.
  Evidence and record; linked to, never published as documentation.
- **Planning, partly historical:** `docs/port/android.md` and
  `docs/port/content-probe.md`.
- **Contributor and agent instructions:** `CLAUDE.md`, `AGENTS.md`,
  `tools/cloud_performance/README.md`.

## Media

**There is none.** No screenshot, GIF, video or logo is tracked anywhere in the
repository. The only images are the 57 test frames in `tests/render/golden/`,
64x64 each and not showcase material. The site's home page has no picture of
a game for that reason.

Capturing LineSweeper by posting key messages to a running window was tried
while building the site, and does not work reliably: the frame capture
itself is sound (`PrintWindow` with `PW_RENDERFULLCONTENT` reads the Direct3D
frame from a window that is not in front), but scripted play stops registering
input partway through a run, for a reason not found. A capture that plays a
recorded input script through the rules - which LineSweeper's replay test shows
are deterministic - would give exact, repeatable images, and is the
recommended way to make them.

## Proposed coverage for the first release

| Section | Proposal |
| --- | --- |
| Get Started | As built: five pages, checked against a real build. |
| Concepts | Five pages: what a game is to the engine; states; the tick; drawing, views and cameras; names, handles and resources. All from the material above. |
| Guides | Sprites and text, input, menus, and assets, each from a sample that already does it. Collision, audio and local multiplayer each wait on a worked example. |
| API Reference | Decide first whether header history is published or removed; then publish module by module, starting with `core` and `scene`. |
| Design | As built: the three documents, read from the repository. |
| Troubleshooting | Start from the three failures Get Started already covers, and grow it from reports. |
