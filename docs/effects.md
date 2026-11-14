# Render targets and a post stage: what they cost, and in what order

> Scoping only, 2026-11-14, read against the tree at `509c00b9`. **Nothing here
> has been built.** Every claim about this repository was checked by reading it
> and is cited by file and line. Claims about the client, ColourWars, cite its
> own checkout at `3fa5bf54`, read and never edited. Claims about graphics APIs, compilers
> and GPUs are general knowledge, marked **[GK]** where they carry weight.
> A few claims about `dxc`, `spirv-cross` and `fxc` were observed by running
> them on a throwaway shader outside the tree; those are marked **[ran]**.
> §1 says which is which, as [port/web.md](port/web.md) §1 does.

The goal is a CRT monitor that a game can turn on: curvature, scanlines that
widen with brightness, a shadow mask, a vignette and colour fringing, applied
to the frame before it is presented. Getting there takes three mechanisms the
engine does not have: a **render target** (`GAP#3`), a **post stage** that
draws the whole frame through a shader, and a **shader authoring path** that
writes one pixel function once and builds it for five backends. This document
answers three questions: **what each costs, what in this tree already makes it
cheap, and which decisions it forces that the tree has not made.** The
decisions are in §12, and each changes something a design document states, a
public header spells, or a rule `AGENTS.md` gives.

Sizes mean what they mean in [port/android.md](port/android.md):
**hours** = an afternoon, **days** = 2-4 days, **week** = 5 working days,
**weeks** = 2 or more. TODO.md's S/M/L/XL grades are coarser; this record
maps its total onto them in §13.

**Contents**

1. [What was checked, and what was not](#1-what-was-checked-and-what-was-not)
2. [The client and the claim](#2-the-client-and-the-claim)
3. [Where the renderer stands](#3-where-the-renderer-stands)
4. [Render targets (GAP#3), backend by backend](#4-render-targets-gap3-backend-by-backend)
5. [The post stage and its fixed material frame](#5-the-post-stage-and-its-fixed-material-frame)
6. [Shader authoring and cross-compilation](#6-shader-authoring-and-cross-compilation)
7. [The CRT effect, and bloom after it](#7-the-crt-effect-and-bloom-after-it)
8. [Caller API sketch](#8-caller-api-sketch)
9. [Lighting and depth, later](#9-lighting-and-depth-later)
10. [Testing and verification](#10-testing-and-verification)
11. [Performance on the low tier](#11-performance-on-the-low-tier)
12. [Decisions](#12-decisions)
13. [Milestones, and what to spike first](#13-milestones-and-what-to-spike-first)
14. [Out of scope, on purpose](#14-out-of-scope-on-purpose)
15. [The documents this amends](#15-the-documents-this-amends)

---

## 1. What was checked, and what was not

**Read in this tree:** the seam (`engine/render/renderer.h`,
`engine/render/SEAM.md`), each backend's renderer and device resources, the
shared shader and its build, the GL loader and contexts, `Application`'s
restore path, `Scene::draw`, the pixel-test harness and comparator, the ctest
inventory, and the documents §15 lists.

**Read in ColourWars, at `C:\Projects\ColourWars` (`3fa5bf54`), read-only:**
its backstory, opening, level drawing, polish plans, TODO and gap audit, cited
as `ColourWars path:line`.
Whether a record here may cite the client by path is E14: the
colourwars-reports folder describes clients only generically
(`docs/review/colourwars-reports/README.md:20-26`).

**Run, outside the tree [ran]:** the Vulkan SDK 1.4.357.0's `dxc` and
`spirv-cross`, and the Windows SDK 10.0.26100.0 `fxc`, on `sprite.hlsl` and
on a CRT sketch shaped like §7's, and on an identity material in §5.4's
frame. No preset was configured and no GPU drew a
post pass.

**Not checked:** whether the GitHub-release `dxc` that vcpkg's `directx-dxc`
installs emits SPIR-V (the Windows SDK copy can lack it,
`cmake/compile_shaders.cmake:84-85`); whether the `directx-dxc` and
`spirv-cross` ports exist at the tree's vcpkg baseline (the local,
Visual Studio-bundled vcpkg has no registry checkout); whether vcpkg host
tools are reachable from a `wasm32-emscripten` configure; any GPU time on any
device; device removal on D3D11 or D3D12, which no ctest forces today.

**The lint scripts do not check this file.** `check_doc_citations.cmake`
scans only comments in source files under `engine/`, `samples/`, `tests/`,
`bench/` and `tools/`, and `check_whitespace.cmake` scans only those folders'
sources. A clean run of either says nothing about the citations here, which
were checked by reading the source by hand.

**Easy misreadings of the tree, corrected.** Each looks true from a quick read:

1. *"D3D11 binds the back buffer into its deferred contexts at begin_frame."*
   `begin_frame` binds the immediate context only: "THE BINDING IS NOT DONE
   HERE" (`engine/render/d3d11/renderer.cpp:805-812`). Views bind when
   `set_view_count` covers them (`:850-869`); D3D12 likewise
   (`engine/render/d3d12/renderer.cpp:1045-1065`). The *frame's* target is
   fixed by `begin_frame`, where the clear goes; a *view's* by
   `set_view_count`.
2. *"The back buffer is always sRGB."* That comment
   (`engine/render/d3d11/device_resources.h:40-41`) is about colour space; the
   format is `B8G8R8A8_UNORM` (`engine/render/d3d11/backend.h:39`) and any
   `_SRGB` is stripped (`engine/render/d3d11/device_resources.cpp:53-58`).
3. *"miniature_view.cpp draws the arena on the monitor glass."* No such file.
   The opening draws the arena into back-buffer panes on the glass (ColourWars
   `game/objects/match_opening.cpp:296-316`); the miniature is a mode switch
   (`game/objects/level.cpp:770-777`).

---

## 2. The client and the claim

### 2.1 What ColourWars draws

Paths in this subsection are ColourWars'. It ships on Direct3D 11
(`README.md:25`) at 720p, 1080p and QHD (`docs/polish-plan.md:316`). Every
match opens on "a computer monitor's bezel ... probably a beige CRT" and ends
the opening with "**The camera pushes through the glass,** and split screen
begins" (`docs/backstory.md:43-65`). The glass is a fixed fraction of the
screen (`game/content/presentation.json:1205`); the push lasts 0.8 s
(`game/content/tuning.json:88`); `intro_screen()` lerps the glass to the
window (`game/objects/match_opening.cpp:207-213`). Each human's view lerps from
the picture on the glass to their pane (`match_opening.cpp:296-316`), drawn
between a "behind" list (the casing) and a "front" list (tint, sticky notes,
lip, close button, picker, cursor, silhouette) (`match_opening.cpp:326-398`).

For context only: the polish plan's dated note that "simple characters and
largely flat arena geometry speak different visual languages"
(`docs/polish-plan.md:139-141`) is about art consistency, and its remedy is
art, not a post stage. It is not evidence that post-processing fixes a flat
look.

The client's one engine ask in this area is a different, parked one: the
`ENG#PARKED` row lists "colour transforms (was `GAP#F10`)" (`TODO.md:89`),
from the gap audit's F10, "Nothing between `submit()` and the presented back
buffer", which found a greyscale pause or a daltonization filter impossible
for a client "at any call count" (`docs/review/gap-audit.md:391`). A post
stage would also meet that ask, with a colour matrix as a material.

### 2.2 What sprites cannot do

The engine draws premultiplied sprites through one blend
(`engine/render/renderer.h:202-205`; SEAM §5). Each sprite is an affine quad
sampled from a texture (`engine/render/sprite_geometry.h`). That is enough for
a constant darkening vignette and a full-screen colour grade, as LineSweeper's
README argues when it declines a `BlendMode`
(`samples/linesweeper/README.md:632-640`). It is not enough for:

| Effect | Why one blend over quads cannot draw it |
|---|---|
| Barrel curvature | Moves where every pixel of the composed frame lands. A quad cannot warp its texture, and the frame is not a texture. |
| Scanlines modulated by brightness | A dark-row overlay under the premultiplied blend computes `dst * (1 - a)`: a constant factor. A beam that widens on bright pixels needs the frame's own colour per pixel. |
| Shadow mask | Same as scanlines: a constant multiply is reachable, a mask that keeps highlights is not. |
| Chromatic aberration | Resamples each channel of the composed frame at a different position. |
| Bloom | Blurs the frame's bright parts and adds them back. |
| Phosphor persistence | Reads the previous presented frame. |

Every row needs the composed frame as a texture, which is `GAP#3`, and a pixel
function run over it, which is the post stage.

### 2.3 Why this answers T1 and the survey's "Not now"

T1 gives *how* to the engine and *what* to the game, and extracts mechanisms
"from a real client game in hand" (PHILOSOPHY T1). Here:

- **Mechanism (engine):** render targets, the post stage, the material frame,
  and the build step that turns one HLSL function into five backends' bytes.
- **Policy (game):** whether the CRT is on, its region, its strengths, and how
  Reduce effects changes it; the client already has that preference
  (ColourWars `docs/polish-reduced-effects.md:3-21`).
- **In between:** the CRT material. It is a look with tuned constants, which
  T1 calls policy, and PHILOSOPHY's UI section tries a compound first as a
  copied component, moving it into the engine only once "every client ends up
  copying unchanged" (`docs/design/PHILOSOPHY.md:634-642`). It is also the
  material that proves the frame on five backends. E3 weighs the two.

The survey put "Custom shaders and post-processing (XL, against T8's one
pipeline)" under *Not now*, "Each waits for a client that needs it (T1)"
(`docs/survey/2026-10-27.md:61-66`); the decision record deferred GAP#3 "until
a client asks" (`docs/2026-11-04-decision-recommendations.md:386-392`); TODO
holds it as a DECISION (`TODO.md:130`).

**The trigger has fired (E16, answered 2026-11-14).** ColourWars has one
developer, who is also the engine's owner, and wants the CRT look for it; that
is the client in hand T1 asks for. No client document asked first, and none
needs to: a client's documents record its author's decisions, they do not
outrank them. The parked colour-transform ask above is a second use for the
same stage. The dated records that deferred this are **not edited** ("The
comparison as written, not updated as items land",
`docs/survey/2026-10-27.md:5`); the answer is recorded in TODO and BACKLOG
(§15). T8's objection is answered in §5.3.

---

## 3. Where the renderer stands

| Fact | Where |
|---|---|
| One shader, `sprite.hlsl`, with `b0` (`pixels_to_clip`), `t0`, `s0` | `engine/render/sprite.hlsl:77-83` |
| D3D11 compiles it at `vs/ps_4_0_level_9_1`, D3D12 at `5_1`, both with fxc | `engine/CMakeLists.txt:171-178`, `:195-202` |
| Vulkan compiles it with the SDK's dxc, shifts `b+0 t+1 s+2` | `cmake/compile_shaders.cmake:117-121` |
| GL compiles a hand-written GLSL copy at device creation | `engine/render/gl/sprite_shader.h:18-34`; `gl/renderer.cpp:154-173` |
| The GL loader has no FBO or `glUniform4fv` entry points; 1.1 calls such as `glDrawElements` link directly and are never loaded | `engine/render/gl/gl_functions.h:86-128`, `:115-116`; `gl/renderer.cpp:558`; `gl/wgl_context.cpp:30-35` |
| One premultiplied blend for every draw | `engine/render/renderer.h:202-205`; SEAM §5 |
| Views are panes of one back buffer, drawn in view order | `renderer.h:135`, `:396-407` |
| D3D11 binds views in `set_view_count`, not `begin_frame` | `d3d11/renderer.cpp:805-812`, `:850-869` |
| D3D12 `submit()` returns early when no view recorded | `d3d12/renderer.cpp:1117-1120` |
| Vulkan draws into an engine-owned `B8G8R8A8_UNORM` image without `SAMPLED` usage, and copies it at present | `vulkan/device_resources.cpp:1013-1015`, `:1847-1910` |
| Vulkan `submit` records every view in one render pass | `vulkan/renderer.cpp:993-1016`, `:1172-1182` |
| `read_back_buffer` is legal only between `submit()` and `end_frame()` | `renderer.h:420-458` |
| After a restore, Application re-adds `engine:white` and replays the manifest, nothing else | `engine/app/application.cpp:695-713` |
| An overlay scene's first view reuses the last declared view | `engine/scene/scene.h:270-276` |
| Default `view_capacity` is 4 | `engine/app/application.h:103` |
| `engine/math` has no four-component vector | `engine/math/` listing |

Two shape everything below: **post must run inside `submit()`**, or read-back,
the pixel tests and `LineSweeperCapture` miss it; and **GL is the expensive
path**, with no framebuffer object and no offline shader format.

---

## 4. Render targets (GAP#3), backend by backend

Two kinds of target share one implementation:

- the **scene target**: engine-owned, back-buffer sized, the input to the post
  chain, plus a small pool of intermediates for multi-pass effects;
- **game targets**: `GAP#3` proper, a texture a view draws into and a later
  view samples as a sprite (minimaps, transitions, integer-scaled pixel art).

### 4.1 The shared contract

1. **8-bit UNORM:** BGRA on Direct3D and Vulkan, `GL_RGBA8` on GL. It matches
   every back buffer, so identity is byte-identical (§10.1). Float is E7.
2. **Premultiplied contents**, because targets are drawn under the one blend;
   drawing one with a white tint is correct.
3. **Cleared every frame it is bound:** the scene to opaque black as today
   (`d3d11/renderer.cpp:99`), game targets to `Colour(0, 0, 0, 0)`. Contents
   across frames are not promised (E5). **With a chain set, `begin_frame` and
   the resize restart also clear the back buffer (Vulkan: the output image)
   on every backend**, one extra full clear a frame. Without it, a frame begun
   and never submitted, then presented, would show the previous frame's post
   output, breaking "A FRAME BEGUN AND NEVER SUBMITTED CONTRIBUTES NOTHING TO
   THE NEXT ONE" (`renderer.h:366-373`) and the resize restart's "cleared as
   begin_frame would clear it" (`renderer.h:336`). Today D3D11 clears only the
   one `render_target` (`d3d11/renderer.cpp:793-796`), D3D12 one target
   (`d3d12/renderer.cpp:553-561`), and Vulkan present copies the output image
   whatever it holds (`vulkan/device_resources.cpp:1847-1910`). A test begins
   a frame, skips `submit()`, presents and reads, chain on, on every
   rasterising backend.
4. **Top row first on every backend**, so a target is sampled with the
   engine's top-left UVs and shared `sprite_geometry` learns no flip. Direct3D
   and Vulkan already do this; GL needs §4.4's convention.
5. **View order is the only ordering** (`renderer.h:396-407`). A view drawing
   a target precedes any view sampling it; a view sampling its own target is
   refused with `std::logic_error` (**[GK]** D3D11 would sample black). The
   check runs where a batch's texture changes, which already flushes, and at
   `submit()`, never per `draw_sprite` (T8).
6. **Overlay scenes.** `Scene::draw` reuses the last declared view
   (`scene.h:270-276`), so target-bound views are declared below the scene's,
   and `Scene::draw` refuses to reuse one.
7. **Loss.** A target is in no manifest (`application.cpp:695-713`), and the
   renderer cannot re-register it: it holds only `const RenderResources*`
   (`renderer.h:364`). On loss `Application::on_device_lost` calls
   `release_device_resources()`, which drops every texture payload
   (`application.cpp:680-691`; `render_resources.h:85-110`), and
   `on_device_restored` re-adds only `engine:white` and the manifest
   (`application.cpp:695-713`). So the *scene* target and intermediates, which
   the renderer owns, are rebuilt by the renderer; a *game* target needs a
   stated mechanism (E6): either `RenderResources` keeps each target's
   description and a factory call that `on_device_restored` makes beside
   `add_white_texture` recreates them empty, or the Renderer owns game
   targets. On D3D12 each recreated target takes a descriptor slot from the
   bump counter's fixed 256 (`render_resources.h:96-110`), which a device
   loss resets. Only the game redraws a target's contents.
8. **Created outside a frame**, keeping allocation out of the loop (T8).

### 4.2 Direct3D 11

**Scene target.** Owned by `Renderer::Impl`, not the DirectXTK-derived
`DeviceResources`: a `B8G8R8A8_UNORM` texture with `RENDER_TARGET |
SHADER_RESOURCE`, its RTV and SRV, beside the existing state
(`d3d11/backend.h:268-284`). One re-entrant creation function is called from
`create_device` (after `renderer.cpp:663`), from `OnDeviceRestored`
(`:623-633`), which runs after the swap chain is rebuilt
(`device_resources.cpp:384-392`), and from `window_size_changed` when rebuilt
(`:747-752`); `OnDeviceLost` (`:589-621`) resets it. Every back-buffer bind
becomes `Impl::frame_target()`: `begin_frame` (`:792-798`), `set_view_count`
(`:861-869`), the resize restart (`:756-772`). Deferred contexts and fan-out
are unchanged.

**Overlay split.** `submit()`'s execute loop (`renderer.cpp:907-921`) splits
at the overlay index: the processed views' command lists execute, then the
post pass, then the overlay views' lists. Each list was recorded against its
own target at `set_view_count`, so only the order of execution changes.

**Post pass.** Inside that loop at the overlay index, on the immediate
context, after the processed views' `ExecuteCommandList`s: bind the back buffer, null input
layout, post VS/PS, map the constants, bind `t0` and the existing samplers
(`renderer.cpp:486-524`), a null blend state **[GK: no blending]**,
`Draw(3, 0)`, then unbind `t0`. **[GK]** `ExecuteCommandList(..., FALSE)`
resets immediate state, so the pass sets everything, as `flush()` already does
(`renderer.cpp:184-189`). `read_back_buffer` (`:931-993`) is unchanged.

**Game targets.** `D3D11Texture` (`backend.h:41-67`) gains an optional RTV;
`add_render_target` in `d3d11/texture_factory.cpp` registers in the same
registry, so `draw_sprite` is unchanged. A target-bound `View::bind` records
its clear into the deferred context. **Multi-pass:** a fixed pool at scales 1,
1/2, 1/4; unbind every PS SRV before binding an output. **[GK]** Binding an
RTV that is still an SRV nulls the SRV silently. (§7.3 replaces this pool with
a 1/2-to-1/16 pyramid if bloom is built.)

**Size:** scene, pass, second clear and overlay split *days*; game targets
*days*; multi-pass *hours*.

### 4.3 Direct3D 12

**Scene target.** A committed texture with `ALLOW_RENDER_TARGET` and an
optimised clear matching `open_frame`'s black (`d3d12/renderer.cpp:555-561`),
owned by `Renderer::Impl` because `handle_device_lost` rebuilds window-size
resources *before* `on_device_restored` recreates the shader-visible heap
(`d3d12/device_resources.cpp:511-516`). RTVs go in a new Impl-owned heap of
fixed capacity that throws when full, as `allocate_texture_slot` does
(`renderer.cpp:488-500`). The SRV heap (256 names, `backend.h:217-263`) gains
a reserved tail. `transition_back_buffer` (`renderer.cpp:608-628`)
generalises to any tracked resource. One scene texture serves both frames in
flight: one queue, and barriers order the reads **[GK]**.

**Post pass.** `PostPipeline` beside `SpritePipeline` in
`d3d12/sprite_pipeline.{h,cpp}`, which still owns "root signature and PSO
only" (`AGENTS.md`, Implementation rules): root constants at `b0`, visible to
both stages (44 DWORDs; a root CBV instead if E8 grows the block past the
64-DWORD root signature **[GK]**), an SRV
table `t0..t3`, static samplers, no input layout, blending off, built in
`create_device_dependent_resources` (`renderer.cpp:634`). Unused SRV slots
in the `t0..t3` table get null descriptors, which tier-1 resource binding
requires **[GK]**. The `lists.empty()` early return (`:1117-1120`) becomes
"skip the views, still run the chain". Scissor rects are mandatory
(`renderer.cpp:69-82`).

**Overlay split.** `submit()` today gathers every view's list into one
`ExecuteCommandLists` and then `signal_frame` (`renderer.cpp:1100-1124`).
With a chain it submits, in order, the processed views' lists, then
`frame_list` carrying the post pass and its transitions, then the overlay
views' lists, in one `ExecuteCommandLists` or three, with `signal_frame`
after all of them.

**Game targets** rest in `PIXEL_SHADER_RESOURCE`, as uploads do
(`texture_factory.cpp:256-258`). A target-bound `View::begin`
(`d3d12/renderer.cpp:269-331`) records `PSR -> RT` and the clear;
`View::close` (`d3d12/renderer.cpp:333-344`) records `RT -> PSR`. Each list
restores the resting state, which holds under parallel recording, unexecuted
views and `abandon_recording` (`d3d12/renderer.cpp:520-544`).

**Size:** scene, pass, second clear and overlay split *week*; game targets
*days*; multi-pass *days*.

### 4.4 GL and WebGL2

**Loader.** Add `glGenFramebuffers`, `glBindFramebuffer`,
`glFramebufferTexture2D`, `glCheckFramebufferStatus`, `glDeleteFramebuffers`,
`glIsFramebuffer` and `glUniform4fv` (`gl_functions.h:86-128`), all core in
GL 3.3 and ES 3.0 **[GK]**. `glDrawArrays` is not added: it is GL 1.1, called
directly and linked from `opengl32.lib` as `glDrawElements` already is
(`gl/renderer.cpp:558`), and some drivers answer null for 1.1 entry points
through `wglGetProcAddress` (`gl/wgl_context.cpp:30-35`).

**Scene target.** `GL_RGBA8`, renderable and filterable in WebGL2 without an
extension **[GK]**, sized to `drawable_size()`, which GL reads live
(`gl/wgl_context.cpp:229` on the desktop; in a browser the CSS box times
devicePixelRatio, `gl/web_context.cpp:246-261`), and nothing is rebuilt on a
resize (`gl/renderer.cpp:633-638`). With no resize hook, `begin_frame`
compares sizes and reallocates. `clear_and_reset_views`
(`renderer.cpp:295-317`) clears the scene FBO and framebuffer 0 to the
opaque black it uses today (`renderer.cpp:304-307`), §4.1 item 3's shared
term, so a frame begun and never submitted contributes nothing
(`renderer.h:366-373`). Read-back must bind `GL_READ_FRAMEBUFFER` 0 before
its `glReadBuffer(GL_BACK)` (`renderer.cpp:812-818`); no `glBindFramebuffer`
exists in `engine/render/gl` today.

**The y convention, decided once.** Today the one flip is
`glViewport(x, drawable_height - (y + h), ...)` (`gl/renderer.cpp:531-545`).
**The scene FBO is drawn exactly as framebuffer 0 is today**: the same
viewport flip and the same `pixels_to_clip`, so sprite rasterisation into it
is unchanged by construction and GL's implementation-defined edge and
tie-break rules **[GK]** cannot move a row. The scene texture is therefore
bottom row first, and the frame's engine-owned sampling helpers
(`scene_point`, `scene_linear`, §5.4) flip v by the frame's `output_flip`
term. That is engine code in the frame, not shared sprite UV code, so SEAM §5
holds. **Game targets** are drawn top row first, transform
`(2/w, +2/h, -1, -1)`, viewport `glViewport(x, y, w, h)`, so a sprite samples
them with the engine's top-left UVs; winding flips, harmless with culling off
(`renderer.cpp:431-432`). They get new goldens anyway (§10.2).

**Post pass.** An attribute-less VAO (core profile draws nothing without one,
`renderer.cpp:368-371`) and `glDrawArrays(GL_TRIANGLES, 0, 3)`; unbind the
scene texture before its FBO is bound again (**[GK]** a feedback loop is an
error in WebGL2). A pass into framebuffer 0 and a pass into a top-row-first
intermediate (bloom, §7.3) differ in y, so the vertex stage reads
`output_flip` from the frame (§5.4). **Overlay split:** after the post pass,
framebuffer 0 stays bound and the overlay views draw into it as views do
today. **Context loss:** FBOs, targets and programs join `restore_device`'s
forget list (`renderer.cpp:443-461`) and `create_gl_resources`
(`renderer.cpp:319-441`), with destructors guarded like `GlTexture`
(`gl/render_resources.cpp:23-40`). Open: whether a restored context can reuse
a lost name, which already affects `GlTexture`.

**Size:** loader, scene, pass and overlay split *week*; game targets *days*;
multi-pass *days*. GL is the largest share.

### 4.5 Vulkan

**Scene target.** Vulkan already draws into its own image. The colour target
becomes a `TrackedImage` (image, memory, view, framebuffer, extent, `layout`,
`layout_submitted`), `transition_colour` takes any of them, and
`stage_of`/`access_of` already know `SHADER_READ_ONLY_OPTIMAL`
(`vulkan/device_resources.cpp:66-104`). `execute` and `abandon_commands`
commit or reset **every** tracked image (`:1646-1651`, `:1702-1749`); missing
one is the hazard that comment describes. The **scene** is a new image with
`COLOR_ATTACHMENT | SAMPLED | TRANSFER_DST`, which the clear
(`vulkan/renderer.cpp:661-698`) and the views' pass target. The existing image
stays the **output**, without `SAMPLED` (`device_resources.cpp:1013-1015`):
it is what present copies and read-back reads, and nothing samples it
(persistence reads its own history image, §7.3). Images follow the engine's extent, not the
swapchain's (`device_resources.cpp:988-1003`), so swapchain rebuilds are
untouched.

**Post pass.** Render passes, not dynamic rendering: the floor is 1.2 with
only `VK_KHR_swapchain` (`device_resources.cpp:41`, `:727-749`). One more
render pass on the output image with `loadOp DONT_CARE`; a post descriptor
set layout (§6.3) and a small per-frame pool; the frame's cbuffer as a
uniform buffer per frame in flight, since 176 bytes already exceeds the
128-byte minimum push-constant size **[GK]**; one pipeline per material with
no vertex input, rebuilt on restore (`renderer.cpp:700-720`). Every post
pass sets the same negative-height viewport sprites use
(`vulkan/renderer.cpp:1053-1067`), because the post vertex entry emits
Direct3D-convention clip y. **The producer carries the dependency:** the
views' pass today has only a `TRANSFER_READ` outgoing dependency
(`device_resources.cpp:917-924`); once it targets the scene, its outgoing
dependency becomes `FRAGMENT_SHADER`/`SHADER_READ`, or the scene gets an
explicit `transition_colour` to `SHADER_READ_ONLY_OPTIMAL` with the matching
stage and access.

**Overlay split.** `submit` records every view inline in one render pass on
one image (`renderer.cpp:993-1016`). With a chain it records the processed
views in the scene pass, then one pass instance on the output image that
draws the post triangle and then the overlay views inline. The output pass's
`DONT_CARE` load is right because post writes every pixel; the overlay draws
follow in the same subpass, so no extra dependency is needed between them.

**Game targets** start in `SHADER_READ_ONLY`, which every descriptor write
assumes (`renderer.cpp:1118-1119`). Target-bound views get their own pass
instances before the scene pass. **Verification** is the `validate_sync`
procedure (`docs/review/vulkan/README.md:151-175`).

**Size:** scene, pass, second clear and overlay split *week*; game targets
*days*; multi-pass *hours*.

### 4.6 Null

Null records, it does not rasterise (`engine/render/null/recording.h:18-19`).
It gains `RecordedTarget{id, width, height}`, a `target` field on
`RecordedSprite` (`:26-33`), and `recorded_passes(renderer)`: an ordered list
of `{material, parameters, inputs, output, size}` filled by `submit()` and
cleared by `begin_frame()`, under the validity rules `recording.h:35-42`
states. This is the seam's headless implementation PHILOSOPHY requires
(*Tests and toolchain*). **Size:** *days*.

---

## 5. The post stage and its fixed material frame

### 5.1 Where it runs

Inside `submit()`, once per frame, after every processed view has executed
and before any overlay view (§5.2). So
`read_back_buffer` returns the post output, keeping the promise that a capture
is the frame the stack would draw (`renderer.h:441-449`); "a second call adds
nothing" (`renderer.h:405-407`) holds; and a zero-view frame still runs the
chain and presents black.

### 5.2 Processed views and overlay views

A whole-back-buffer effect would curve the beige bezel, the sticky notes and
the HUD. Two mechanisms keep them flat:

1. **An overlay boundary.** `begin_overlay()`, called inside a frame, declares
   one new view. It and every later view draw into the back buffer *after* the
   chain, unprocessed. Views declared before it draw into the scene target.
   This fits D3D11's bind-at-declaration (§1, correction 1): the boundary is
   forward-only, and no already-declared view changes target. It declares a
   fresh view itself, so `Scene::draw`'s reuse of the last view
   (`scene.h:270-276`) lands in the overlay layer, not the processed one.
   With an empty chain, `begin_overlay()` only declares a view, so game code is
   the same with the effect on or off. Each backend's section in §4 says how
   its `submit()` splits at the boundary.
2. **A screen region.** Each built-in effect takes a rectangle in back-buffer
   pixels. Outside it, pixels pass through the chain unchanged. The opening's
   casing, drawn before the arena, is outside the glass and stays flat without
   moving to the overlay.

At submit the order becomes: processed views into the scene target, the chain
into the back buffer, overlay views into the back buffer.

### 5.3 Wiring time, and the T8 argument

The chain's *shape* (materials, intermediates, scales) is set outside a frame,
`std::logic_error` inside one, and built then and on restore. Its *values* may
change any time before `submit()`, which uploads them.

T8 says "Flexibility that survives lives at load and wiring time, never in the
loop" (PHILOSOPHY T8). Per frame, the cost with an empty chain is one branch,
`frame_target = chain_empty ? back_buffer : scene`, and every existing golden
and timing is unchanged. With a chain, it is one full-screen pass per material:
a whole-frame cost, stated in §11, never a per-sprite one. The sprite path
keeps its one shader and one blend; `sprite.hlsl`'s "IT DOES ALMOST NOTHING,
AND THAT IS THE DESIGN" (`sprite.hlsl:39`) stays true.

**Why this does not reopen the BlendMode decline.** LineSweeper declined a
`BlendMode` because "a change of `set_filter` flushes the view's batch" and a
blend selector would inherit one blend state per draw call
(`samples/linesweeper/README.md:632-640`). A post pass is not a draw-list
state. It is one pipeline per frame, chosen at wiring time, and draws no
sprites. Its writes are opaque (blending off), which SEAM §5 must say in one
sentence: the premultiplied blend governs draw lists; full-screen passes
replace. The README's route for a darkening vignette and a colour grade, one
blend and a full-screen sprite, stays right when nothing else needs the frame.

### 5.4 The fixed frame

The engine owns `engine/render/post_frame.hlsli`. It declares every register,
the vertex entry and the pixel entry. A material is one function:

```hlsl
// engine/render/post_frame.hlsli - the engine's half of every material.
// A material declares no register, reads no SV_Position, and defines
// exactly one function: float4 material(MaterialInput input).

cbuffer PostFrame : register(b0)
{
    float4 output_size;            // xy: output pixels; zw: 1 / output pixels
    float4 scene_size;             // xy: scene pixels;  zw: 1 / scene pixels
    float4 frame_terms;            // x: the caller's seconds; y: frame index;
                                   // z: v sign of every input (-1 on GL, else +1); w: unused
    float4 material_values[8];     // the material's parameters, in order
};                                 // 3 + 8 = 11 float4s, 176 bytes, 44 DWORDs

Texture2D<float4> input_0 : register(t0);   // the scene, or the previous pass
Texture2D<float4> input_1 : register(t1);   // the scene, for a pass that needs both
Texture2D<float4> input_2 : register(t2);   // reserved: the history target (§7.3)
Texture2D<float4> input_3 : register(t3);   // reserved: an engine-owned mask or LUT
SamplerState point_sampler : register(s0);
SamplerState linear_sampler : register(s1);

struct MaterialInput
{
    float2 uv;            // 0..1 across the output, (0, 0) at the top left
    float2 output_pixel;  // uv * output_size.xy: top-down output pixels
};

float4 material_value(int index) { return material_values[index]; }

// Top-down uv to the texture's own v. GL's engine-owned post textures are
// drawn exactly as framebuffer 0 is, bottom row first (§4.4); this is the one
// place that knows, and it is engine code, not shared sprite UV code.
float2 input_uv(float2 uv)       { return float2(uv.x, 0.5 + (uv.y - 0.5) * frame_terms.z); }

// A material samples only through these helpers.
float4 scene_point(float2 uv)    { return input_0.SampleLevel(point_sampler, input_uv(uv), 0); }
float4 scene_linear(float2 uv)   { return input_0.SampleLevel(linear_sampler, input_uv(uv), 0); }
float4 second_linear(float2 uv)  { return input_1.SampleLevel(linear_sampler, input_uv(uv), 0); }

struct PostVaryings
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

// One triangle covering the output, from the vertex index alone.
PostVaryings post_vertex_main(uint vertex_index : SV_VertexID)
{
    PostVaryings result;
    const float2 corner = float2((vertex_index << 1) & 2, vertex_index & 2);
    result.uv = corner;
    result.position = float4(corner.x * 2.0 - 1.0, 1.0 - corner.y * 2.0, 0.0, 1.0);
    return result;
}

float4 material(MaterialInput input);   // defined by the material file

float4 post_pixel_main(PostVaryings varyings) : SV_Target
{
    MaterialInput input;
    input.uv = varyings.uv;
    input.output_pixel = varyings.uv * output_size.xy;
    return material(input);
}
```

Rules the frame enforces:

- **One cbuffer of `float4`s:** one Vulkan uniform buffer per frame in
  flight, one flattened `vec4[]` and one `glUniform4fv` on GL (§6.2), one
  root-constant block on D3D12, one dynamic buffer on D3D11. At 11 `float4`s
  it is 44 DWORDs, under D3D12's 64-DWORD root signature **[GK]**; E8's 16
  would be 19 `float4`s, 76 DWORDs, and move D3D12 to a root CBV. Vulkan's
  176 bytes already exceed the 128-byte minimum push-constant size **[GK]**,
  so it is a uniform buffer from the start. The count is E8, the frame's
  overall size E18.
- **No `SV_Position` in a material.** **[GK]** GLSL ES 3.00 cannot declare
  `origin_upper_left`, so `gl_FragCoord` counts from the bottom on WebGL2.
  `output_pixel` is top-down on every backend, which keeps SEAM §5's "nothing
  a backend does decides where a pixel goes" true for post.
- **Y is the frame's alone.** The vertex entry emits Direct3D-convention clip
  y; Vulkan inverts it with the negative viewport sprites already use (§4.5),
  and GL draws every engine-owned post texture as it draws framebuffer 0
  (§4.4), so the vertex stage reads no uniform. The one GL term is the input
  v sign, `frame_terms.z`, which `input_uv` applies. A material never sees it,
  provided it samples only through the frame's helpers.
- **Colour encoding.** Targets hold sRGB-encoded values in UNORM (§1,
  correction 2). Whether the CRT and bloom compute in that encoding or in
  linear light is E17; under E17 (b) the frame gains `to_linear` and
  `to_encoded` helpers.
- **Registers are the engine's**, so "Binding policy stays out of the shader"
  (`compile_shaders.cmake:84-90`) stays true.

CMake wraps a material in a three-line build-tree file (`#include` the frame,
the material, the entry), so an error names the **material's** file and line
[ran: `crt_material.hlsl(8,28)`], as T6 asks.

---

## 6. Shader authoring and cross-compilation

### 6.1 One source, five backends

| Backend | Build step | Bytes the backend embeds |
|---|---|---|
| D3D11 | fxc `/T vs_4_0`, `/T ps_4_0` | DXBC byte array, as today |
| D3D12 | fxc `/T vs_5_1`, `/T ps_5_1` | DXBC byte array |
| Vulkan | SDK dxc `-spirv -T vs_6_0/ps_6_0`, post shifts (§6.3) | SPIR-V byte array |
| GL, WebGL2 | dxc `-spirv`, then `spirv-cross` to `#version 330` or `--es --version 300`, then a `cmake -P` embed script | GLSL text header |
| null | nothing | none; it records the material handle |

Only one GL dialect is needed per build: `EMSCRIPTEN` selects ES 3.00,
otherwise 330, as `wgl_context.cpp` and `web_context.cpp` are already
exclusive (the `if(EMSCRIPTEN)` branch, `engine/CMakeLists.txt:210-230`), each
with its own `#version` prelude (`gl/wgl_context.cpp:223`;
`gl/web_context.cpp:118-121`).

**D3D11 profile.** [ran] `vs_4_0_level_9_1` rejects the triangle (`X4555:
invalid type used for 'SV_VertexID'`); `vs_4_0` accepts it, and the 10_0 floor
(`d3d11/device_resources.cpp:64-72`; `sprite.hlsl:55-60`) allows it. **[GK]**
9_1's instruction limits would not hold the CRT anyway (E10).

### 6.2 What the GLSL looks like [ran]

Running `sprite.hlsl` and the CRT sketch through dxc and spirv-cross showed
what the engine's embed step must fix, once, never in a game's file:

1. **Uniforms.** A cbuffer becomes a std140 block; `--flatten-ubo` makes it
   `uniform highp vec4 type_PostFrame[11];` for §5.4's frame, one
   `glUniform4fv`, no UBO objects to restore after a loss. This is why the
   frame is one array of `float4`s.
2. **Samplers, and what a material leaves out.** Texture and sampler pairs
   become combined `sampler2D`s named `SPIRV_Cross_Combined<texture><sampler>`,
   and the CLI cannot rename them. **dxc and spirv-cross drop every resource a
   material does not use** [ran: an identity material that reads only
   `input_0` through `point_sampler` emits one
   `SPIRV_Cross_Combinedinput_0point_sampler` and no `type_PostFrame` at all;
   its vertex stage declares no uniforms]. So the embed script renames each
   combined sampler from a fixed engine table, **fails the build on any name
   not in the table** (T6), and accepts missing ones; at link time the engine
   skips any uniform or sampler whose location is -1 (`glUniform*` at location
   -1 is already a no-op **[GK]**). `[[vk::combinedImageSampler]]` under
   `#ifdef __spirv__` also works but puts binding policy in the shader.
3. **Varyings.** The stages emit `out_var_TEXCOORD0` and `in_var_TEXCOORD0`;
   GL 3.3 and ES 3.00 link by name **[GK]**. A fixed
   `--rename-interface-variable` list on both stages fixes it.
4. **Precision.** The ES default is written `mediump` though every
   declaration is `highp`. The script rewrites it and fails on any `mediump`,
   holding `web_context.cpp:105-117`'s rule.
5. **`#version`** is stripped, so the context prelude still supplies it
   (`sprite_shader.h:26-34`).
6. **Early return** becomes `do { ... } while (false)`; `SV_VertexID` becomes
   `gl_VertexID`. Both valid.
7. **Arithmetic is unchanged:** for `sprite.hlsl` the emitted expressions are
   the hand-written ones in the same order.
8. **fxc `ps_5_1` alone** warns `X4000` on the early-return idiom.
   `compile_hlsl` passes no `/WX` (`compile_shaders.cmake:62-68`) (E11).

Running `glslangValidator` on the emitted text at build time is optional
hardening that moves the last browser-only risk (`sprite_shader.h:23-24`) to
the build.

### 6.3 Vulkan bindings

The sprite shifts send `b0, t0, s0` to bindings 0, 1, 2
(`compile_shaders.cmake:117-121`); under them the frame's `t1` collides with
`s0`. `compile_hlsl_to_spirv` takes its shifts as arguments: the sprite call
keeps `0/1/2`, post uses `b+0, t+1, s+5`, giving `b0` = 0, `t0..t3` = 1..4,
`s0..s1` = 5..6. The frame declares every register, so one post set layout
serves every material. GL ignores bindings and binds by name.

### 6.4 The game's path, and the tools

```cmake
# game/CMakeLists.txt, after add_subdirectory(external/labrador)
labrador_add_material(MyGame crt_glow
    SOURCE  shaders/crt_glow.hlsl       # defines float4 material(MaterialInput input)
    DEPENDS shaders/common.hlsli)       # fxc writes no depfile
```

It writes `<build>/generated/materials/crt_glow.h` with an
`inline constexpr labrador::MaterialSource` holding the bytes and their kind,
so game C++ is backend-neutral. First, a scoping bug: `LABRADOR_GENERATED_ROOT`
is `set()` in the engine root's scope (`compile_shaders.cmake:11`), and
**[GK]** a function reads its caller's scope, which for the starter is `game/`
(`website/src/walkthrough/root-CMakeLists.txt`, line 11). It moves to a global
property.

| Build | Tools today | With materials |
|---|---|---|
| D3D11, D3D12 | fxc | fxc |
| Vulkan | SDK dxc | SDK dxc |
| GL desktop, wasm | none; `compile_shaders.cmake` is never reached on `gl` (`docs/port/web.md:539`) | dxc with SPIR-V, spirv-cross |

The starter has `x64-*` and `wasm-*` presets and no Vulkan SDK
(`website/src/walkthrough/CMakePresets.json:19-45`). Three ways to supply the
tools (E2):

1. **vcpkg host ports** `directx-dxc` and `spirv-cross` **[GK: both are vcpkg
   ports; not checked at the tree's baseline `d92484ed`, `vcpkg.json:15`]**: no
   SDK and no CI install step. Open: whether both exist at that baseline;
   SPIR-V in the release dxc; reachability under the Emscripten triplet;
   whether the consumer manifest copies `"host": true`
   (`docs/dependency-updates.md:73-79`).
2. **The Vulkan SDK on GL and wasm builds:** simplest for the engine
   (`compile_shaders.cmake:94-99`), but a large install for every starter
   browser user against the near-beginner floor (`docs/beginner-guides.md:3-11`),
   and a copy of the `windows` job's SDK install step
   (`.github/workflows/ci.yml:393-431`) in the `wasm`, `wasm-null` and
   `wasm-consumer` jobs, which have none today.
3. **Checked-in generated GLSL** for engine materials: against the spirit of
   `compile_shaders.cmake:3-6`, which keeps generated shader output out of the
   tree ("avoiding checked-in bytecode that can drift from it"; it says
   nothing of GLSL text, so extending it is a decision), and no help to game
   materials.

Host tools ship no code into a game; `ReleaseNotices.astro` still requires
both ports listed as not shipped (`website/src/components/ReleaseNotices.astro:98-116`).
**[GK]** SPIRV-Cross is Apache-2.0, DXC is NCSA.

### 6.5 What PHILOSOPHY has to say about it

- **T5** is the case *for*: GLSL errors today appear at device creation, in a
  browser (AGENTS.md). Generated GLSL moves them to the build.
- **T9** reserves bought code for "the platform and format edges". dxc and
  spirv-cross are build tools at the shader-format edge, beside fxc and the
  SDK dxc (`compile_shaders.cmake:41`, `:94`); they link nothing and hand over
  no engine. A bought effects library would, and is out.
- **T10 and the scripting-layer non-goal.** The non-goal bans a second
  language, "visual or textual, for a game's logic to live in"
  (`docs/design/PHILOSOPHY.md:885-886`). Game-authored HLSL is a second
  textual language a beginner must learn. It is argued as presentation data,
  like an atlas: it computes a colour from the frame and cannot reach game
  state. That is an argument, not a settled reading; the owner accepts it or
  not (E1).
- **T12** forbids "a code generator standing between the source and the
  compiler". A generated `crt_glow.h` holding an `inline constexpr
  labrador::MaterialSource` is exactly such a generator. The precedent is the
  tree's own: `compile_shaders.cmake` already generates the byte headers the
  backends include (`compile_shaders.cmake:3-7`). Whether that precedent
  scopes T12 to C++ a reader meets, or T12 needs amending, is the owner's
  question beside E1, not settled here.
- **Teachability** (PHILOSOPHY, *The public face*): a CRT settings struct
  passes easily; a game material needs a guide, and the introductory C++
  series, whose acceptance test is that a feature can be explained in it
  (`docs/design/PHILOSOPHY.md:786-790`), would need an HLSL chapter at P6.
  One reason game materials come later.

### 6.6 The sprite shader, and the fallback

Moving `sprite.hlsl` to the generated path would retire the by-eye pair
(`sprite_shader.h:12-16`), but would put the tools on every GL and wasm build
and rename what `gl/renderer.cpp:335-357` binds. Not with materials; later,
alone, only with **zero** changed PNGs on `x64-debug-gl` and `wasm-debug`.

**Cross-compiling is needed only for game materials.** Under E1 (b) there are
only engine materials until P6, and one hand-transliterated GLSL copy of the
CRT beside `sprite_shader.h`, under the existing by-eye pair rule
(`sprite_shader.h:12-16`), would cover GL and wasm. Whether the CRT waits for
the toolchain is E19, a sequencing question, not a consequence of the design.

If cross-compilation is declined, the engine ships built-in materials only,
hand-transliterated beside `sprite_shader.h`; games set parameters. Dual-source
game materials fail the beginner floor, and run-time spirv-cross is rejected
against the wasm size budget (REV#53) and T9.

---

## 7. The CRT effect, and bloom after it

### 7.1 The pixel function

This is the shape that compiled under fxc at `ps_4_0` and `ps_5_1` and under
dxc at `ps_6_0` [ran, on a sketch of the same structure, before the region
clamp, the per-region scanline count and the compensation gain were added;
those revisions have not been compiled]; the constants are starting values
for the spike to tune by looking, not settled ones.

```hlsl
// engine/render/materials/crt.hlsl - an engine material.
// The frame (post_frame.hlsli) declares every register; this file declares none.
//
// material_value(0) = screen region in output uv: x, y, width, height
// material_value(1) = curvature, scanlines per region height, scanline strength, mask strength
// material_value(2) = vignette, aberration (output px at the edge), brightness compensation, unused

float2 barrel(float2 centred, float curvature)   // centred: -1..1 across the region
{
    return centred * (1.0 + curvature * dot(centred, centred));
}

// Every tap stays inside the region, half a texel in from its edge, so neither
// the fringe nor the bilinear footprint reaches the casing drawn around it.
float2 clamp_to_region(float2 uv, float4 region)
{
    const float2 half_texel = 0.5 * scene_size.zw;
    return clamp(uv, region.xy + half_texel, region.xy + region.zw - half_texel);
}

float3 shadow_mask(float2 output_pixel, float strength)
{
    // An aperture grille three output pixels wide: R, G, B columns. The
    // weights 1.75 / 0.25 / 0.25 average 0.75 per channel; brightness
    // compensation (below) restores it rather than letting saturate clip.
    const float phase = frac(output_pixel.x / 3.0) * 3.0;
    const float3 grille = float3(phase < 1.0, phase >= 1.0 && phase < 2.0, phase >= 2.0);
    return lerp(float3(1.0, 1.0, 1.0), grille * 1.5 + 0.25, strength);
}

float4 material(MaterialInput input)
{
    const float4 region = material_value(0);
    const float4 shape = material_value(1);
    const float4 finish = material_value(2);

    // Outside the glass, the pixel passes through untouched.
    const float2 local = (input.uv - region.xy) / region.zw;
    if (any(local < 0.0) || any(local > 1.0))
    {
        return scene_point(input.uv);
    }

    // Curvature: where on the flat picture this point of the curved glass looks.
    const float2 curved = barrel(local * 2.0 - 1.0, shape.x);
    if (any(abs(curved) > 1.0))
    {
        return float4(0.0, 0.0, 0.0, 1.0);   // past the tube's edge
    }
    const float2 source = region.xy + (curved * 0.5 + 0.5) * region.zw;

    // Chromatic aberration: red and blue pulled apart, more toward the edge.
    const float2 fringe = curved * finish.y * output_size.zw;
    float3 colour;
    colour.r = scene_linear(clamp_to_region(source + fringe, region)).r;
    colour.g = scene_linear(clamp_to_region(source, region)).g;
    colour.b = scene_linear(clamp_to_region(source - fringe, region)).b;

    // Scanlines follow the curved picture, so they bend with it. The count is
    // per region height, so the lines scale with the glass during the push
    // instead of changing number. The beam widens with brightness.
    const float brightness = dot(colour, float3(0.299, 0.587, 0.114));
    const float row = frac((source.y - region.y) / region.w * shape.y) - 0.5;
    const float beam_width = lerp(0.15, 0.45, brightness);
    const float beam = exp(-0.5 * (row * row) / (beam_width * beam_width));
    colour *= lerp(1.0, beam * 1.25, shape.z);

    // The shadow mask sits on the glass, so it is keyed to output pixels.
    colour *= shadow_mask(input.output_pixel, shape.w);

    // Vignette: darker toward the curved edge.
    colour *= saturate(1.0 - finish.x * 0.5 * dot(curved, curved));

    // Brightness compensation: a gain the game sets (1 is none) to restore
    // what the mask and the beam take away; saturate still clips above white.
    colour *= finish.z;

    return float4(saturate(colour), 1.0);
}
```

Notes on it:

- **Alpha is 1:** the scene is cleared opaque and the WebGL context has
  `alpha=false` (`gl/web_context.cpp:293-321`).
- **Scanlines per region height, mask in output pixels.** Scanlines belong to
  the picture and curve with it; the mask belongs to the glass. A fixed count
  (for example 240 lines) scales with the glass, so the 0.8 s push does not
  change the number of lines and the pattern does not shimmer. A Gaussian
  beam point-evaluated once per output pixel is barely above Nyquist at 3
  pixels a line and forms moiré bands once curvature bends the rows **[GK:
  the classic CRT-shader failure]**; validation refuses a count that gives
  fewer than 4 output pixels a line at the current region height, and
  integrating the beam over the pixel footprint is the spike's alternative.
  X2 looks at the push for moiré.
- **Region clamp.** Every tap is clamped half a texel inside the region. The
  sampler clamps only to the texture's edge (`d3d11/renderer.cpp:489-491`),
  and outside the region in the ColourWars opening is the beige casing.
- **Brightness.** At full mask strength the grille averages 0.75 and the beam
  averages well under 1, so the picture dims by roughly 40-50% with clipped
  highlights unless the game raises the compensation gain. A gain is chosen
  over normalising the mask and beam to an average of 1, because normalising
  raises highlights past white and saturate clips them; the spike checks this
  by looking.
- **Colour encoding.** Every term here, the luma weights included, operates
  on sRGB-encoded values unless E17 picks linear. **[GK]** In gamma space,
  scanline darkening is much stronger than the physical effect.
- **All strengths zero.** When every strength is zero and the gain is 1, the
  engine draws the identity material in the CRT's place (the views have
  already drawn into the scene target, so the pass cannot simply be skipped),
  and the frame is then the chain-off frame under §10.1's anchor (§10.2);
  rebuilding `source` through float arithmetic and `scene_linear` would
  otherwise not be exact on every device **[GK]**.
- **Region** is given in back-buffer pixels (§8) and divided at upload. One
  region per material (E9).
- **A CPU twin**, `crt_source_position(settings, output_pixel)`, restates
  `barrel` and the region in C++ for pointer mapping and for tests (§10.2).
  Curvature decides where a pixel goes, which SEAM §5 gives only to engine
  code: shared HLSL and shared C++, never a backend.

### 7.2 Reduce effects and flashing

Every strength is a parameter and zero is off; Reduce effects and the client's
WCAG flash review (ColourWars `docs/polish-plan.md:1847-1856`) are the game's
policy (T1), as LineSweeper's flash review was its own
(`samples/linesweeper/README.md:795`). No v1 term animates. A flicker would use
the caller's time, never an engine clock: "a draw that reads a clock or a
counter is outside the pure-read premise" (`renderer.h:450-457`).

### 7.3 Bloom, as a later multi-pass effect

| Pass | Input | Output | Scale |
|---|---|---|---|
| 1. Bright pass, downsample | scene (`input_0`) | `level_1` | 1/2 |
| 2. Downsample chain | `level_n` | `level_n+1` | 1/4, 1/8, 1/16 |
| 3. Upsample and add | `level_n+1`, `level_n` | `level_n` | 1/8 back up to 1/2 |
| 4. CRT, bloom folded in | scene (`input_0`), `level_1` (`input_1`, `second_linear`) | back buffer | 1 |

Each pass is a material in the same frame; the renderer owns the
intermediates, a pyramid from 1/2 to 1/16 rather than §4.2's original 1, 1/2,
1/4 pool. A single half-resolution separable blur gives too small a radius at
1080p and QHD, which the client ships (ColourWars `docs/polish-plan.md:316`),
for a wide phosphor glow **[GK]**. There is no separate full-resolution
composite: the CRT pass reads the bloom as `input_1`, which saves the one
full-resolution write that was most of bloom's cost. Each 8-bit write
re-quantises and bands in dark gradients **[GK]**, so pyramid writes carry an
ordered dither, or E7 accepts the banding explicitly. On 8-bit UNORM nothing is
brighter than white, acceptable for a 90s phosphor (float is E7). The blur
computes in whichever encoding E17 picks; in gamma space halos darken and
fringe **[GK]**. Bloom stays opt-in.

**Persistence** needs an engine-owned **history target** on every backend,
written by a copy of the output at `end_frame`, never the output image
itself: reading the Vulkan output while the post pass writes it is a feedback
loop, and on Direct3D flip-model swap chains and WebGL's framebuffer 0 the
presented buffer cannot be read on a later frame **[GK]**. The history target
is cleared on a resize and on a loss and ping-pongs with a second copy if a
pass must read and write it. The term is `max(current, previous * decay)`
with `decay` computed from the caller's frame time (for example
`pow(decay_per_second, dt)`), so persistence looks the same at 60 Hz and
144 Hz. To keep "a frame submitted and never presented contributes nothing to
the next" (`renderer.h:369-378`), history advances only at `end_frame`.

---

## 8. Caller API sketch

Per CONVENTIONS: PascalCase types, snake_case functions, noun accessors, no
`auto`. Names are proposals.

### 8.1 The post stage

```cpp
// engine/render/post_effect.h, in namespace labrador
// Built-in materials; a chain is a list of these.
enum class PostEffect
{
	crt
};

// Validated when set (std::invalid_argument on a negative strength, a NaN, a
// scanline count giving fewer than 4 output pixels a line, or an empty
// screen). Every strength defaults to zero, and zero turns that term off.
struct CrtSettings
{
	RectangleF screen;              // back-buffer pixels; outside it, pixels pass through
	float curvature = 0.0f;         // 0 is flat
	float scanline_count = 240.0f;  // scanlines across the screen's height
	float scanline_strength = 0.0f; // 0..1; dark rows thin more than bright ones
	float mask_strength = 0.0f;     // 0..1
	float vignette = 0.0f;          // 0..1
	float aberration = 0.0f;        // back-buffer pixels of red/blue split at the edge
	float brightness = 1.0f;        // gain restoring what the mask and beam take; 1 is none
};

// Where the pixel at output_pixel was taken from: the CRT's curvature on the CPU.
Vector2F crt_source_position(const CrtSettings& settings, const Vector2F& output_pixel);
```

```cpp
// Renderer, additions
	// Outside a frame; std::logic_error inside one. An empty span removes the
	// chain and returns the frame to drawing straight into the back buffer.
	void set_post_effects(std::span<const PostEffect> effects);

	// From the thread that owns the frame, outside any Scene::draw callback;
	// the value current at submit() is drawn. Views can draw concurrently, so
	// a call from inside the fan-out would be a data race, and is refused with
	// std::logic_error where the renderer can tell.
	void set_crt_settings(const CrtSettings& settings);

	// Inside a frame, once: declares one new view. It and every view declared
	// after it draw over the chain's output, unprocessed. With no chain it only
	// declares the view. A second call in one frame throws std::logic_error.
	void begin_overlay();
```

`set_post_effects` is the whole wiring for v1. `begin_overlay` needs one spare
view; `set_post_effects` reserves it, so `view_capacity` (`application.h:103`)
counts only the game's own views, and a frame that would exceed capacity
throws a T6 message naming the fix. The null seam tests assert the
off-thread refusal.

**The alternative to one setter per look (E3).** `set_crt_settings` makes
the concrete renderer seam carry one effect's settings type, and bloom or a
colour matrix would each add another method. A generic
`void set_post_values(PostEffect effect, const PostValues& values)`, with
`PostValues` a plain struct of named `float` fields in groups of four (not
`std::array<Colour, 8>`, which would put non-colours in a colour type), and
`CrtSettings` packed into it by a free function or by component code, keeps
the seam at one method. A four-component vector in `engine/math` is the other
route, and the owner's to approve.

### 8.2 Monitor glass: the ColourWars opening

**Illustrative only.** This is a shape, not the client's code: the client's
author writes the real one, if at all (§13 P3). Names below are invented
where the client's differ.

```cpp
// Once when the match is wired (outside a frame)
const PostEffect crt_only[] = { PostEffect::crt };
renderer.set_post_effects(crt_only);

// Each frame, on the thread that owns it
CrtSettings crt = presentation.crt;                // strengths read from data (T7)
crt.screen = this->monitor_->picture(screen);      // the picture rectangle the code has
if (reduce_effects)
{
	crt = presentation.crt_reduced_effects;          // also data, the game's policy
	crt.screen = this->monitor_->picture(screen);
}
renderer.set_crt_settings(crt);

draw_behind(renderer);          // the casing: outside the screen, so untouched
draw_arena_panes(renderer);     // the arena, on the glass: curved
draw_program_chrome(renderer);  // preview tint, picker, close button: curved

renderer.begin_overlay();
draw_front_details(renderer);   // lip and sticky notes: flat, above the picture
draw_cursor(renderer);          // scripted, so it needs no pointer mapping: flat
draw_silhouette(renderer);      // flat
```

The region follows what the code computes: `intro_screen()` lerps the glass
to the window through `screen_for_glass` (`match_opening.cpp:207-213`), and
the picture is `monitor_->picture(screen)` (`match_opening.cpp:362`). During
the 0.8 s push the picture grows toward the window. What the client's author
would decide:

1. **Reordering.** Today the close button and picker draw after
   `draw_front_details` (`match_opening.cpp:379-387`); in the sketch they
   draw under the sticky notes. The cursor also draws after the front details
   today (`match_opening.cpp:396`), and "The cursor and person draw in front"
   (ColourWars `docs/polish-monitor.md:26`), so the sketch puts it in the
   overlay, flat, rather than curved under the notes.
2. **Notes.** `docs/polish-monitor.md:22-26` describes the notes staying
   attached to the bezel "including during the countdown push and ending
   pullback"; in the overlay they keep doing so.
3. **The other paths.** The reveal (`match_opening.cpp:343-347`) and the
   countdown front list (`match_opening.cpp:348-356`) also need placing on
   one side of the overlay boundary, which the sketch does not show.
4. **The CRT stays on in gameplay, subtler** (E9 (d)): the settings lerp from
   the opening's look to gameplay's across the 0.8 s push, and the region
   grows with the picture until it is the window or the panes (§8.3).

### 8.3 Keeping the HUD flat in gameplay

ColourWars draws its HUD into the same per-pane views as the world (ColourWars
`game/objects/level_draw.cpp:27-32`, `:66-130`). Either it accepts a curved
HUD, at no cost, or after `scene.draw` it calls `begin_overlay()` and declares
one view per pane for the HUD: 4 + 1 + 3 = 8 views in four-player, of which
the overlay's first is the one `set_post_effects` reserves, with pointer hits
mapped through `crt_source_position`. E9 (d) keeps a subtler CRT on in
gameplay, so this applies, but how much of it does depends on the experiment:
gameplay terms that do not move pixels (scanlines, mask, vignette, a slight
fringe with no curvature) leave a HUD drawn in the panes where it was, and
only curvature makes the overlay views and pointer mapping necessary.

**The experiment needs fast iteration.** The client reads its strengths from
data (T7), so trying a look is editing values, not rebuilding. The gameplay
starting point to try first is zero curvature, light scanlines and mask, a
faint vignette and fringe, and brightness compensation to keep paint colours
readable; the opening's look is the same terms turned up with curvature.

### 8.4 Game render targets (GAP#3)

```cpp
// engine/render/resource_factory.h, beside add_texture_asset. Outside a frame.
// Registers name as a texture draw_sprite samples like any other, holding
// premultiplied colour. Recreated empty after a loss; only the game redraws it.
void add_render_target(const Renderer& renderer, RenderResources& resources,
	const std::string& name, int width, int height);

// Renderer: inside a frame, before set_view_count covers index. The view draws
// into the target, cleared to Colour(0, 0, 0, 0), at the target's size.
void set_view_target(int index, TextureHandle target);
```

A minimap is view 0 targeting `"minimap"` and a later view drawing it as a
sprite. Integer-scaled pixel art is the game drawn into a small target and
drawn once, point-filtered, at an integer scale. Where the handle lives is E6.

Game materials (§6.4) add `MaterialHandle add_material(const MaterialSource&)`
outside a frame and a values block of `PostValues` (§8.1), not
`std::array<Colour, 8>`: `engine/math` has no `float4`, and `Colour` would
put non-colours in a colour type against T4's clarity.

`add_render_target`'s "recreated empty after a loss" depends on E6's
mechanism (§4.1 item 7): under E6 (a) `Application::on_device_restored` makes
the call that recreates targets, beside `add_white_texture`
(`application.cpp:695-713`).

---

## 9. Lighting and depth, later

A separate stage, scoped only so the post stage does not foreclose it.

- **Lighting:** a light accumulation target drawn from light sprites
  (additive, which the premultiplied blend already gives with zero alpha),
  then a material multiplying the scene by it. Normal-mapped sprites need a
  second texture per sprite and a second sprite shader, which ends "one
  shader, one blend" for draw lists, not just post. That half waits for a
  client, as the survey's "2D lights and shadows" does
  (`docs/survey/2026-10-27.md:61-66`).
- **What post gives it:** targets, the frame, chains and a second input.
- **Depth:** "3D. Permanent." (PHILOSOPHY, *Non-goals*) bounds it, and "order
  is the depth" (PHILOSOPHY, *Simulation and rendering*) stands; no backend
  reads `layer_depth`. A height term for shadows would be a sprite attribute
  or a target, that stage's decision.

**Size:** weeks; not scheduled.

---

## 10. Testing and verification

### 10.1 The anchor: identity is byte-identical

An identity material (`return scene_point(input.uv);`) on an 8-bit UNORM scene
target, point-sampled at texel centres, must reproduce **every existing golden
byte for byte** on every rasterising backend, with no new PNG. This proves the
redirect, the GL y convention, the full-screen triangle's coverage and
read-back in one run. The orientation goldens
(`contract-the-destination-is-in-pixels-y-runs-down-origin-top-left.png`,
`contract-texel-0-0-draws-at-the-top-left-of-the-destination.png`) catch a
flipped copy.

- **`RenderPixelTestsThroughPost`:** the same executable with every `Harness`
  routed through identity, held to the unchanged goldens. Native selects it by
  a ctest `ENVIRONMENT` property, read as `dumping()` reads
  `LABRADOR_GOLDEN_DUMP` (`tests/render/golden_image.cpp:320-327`); the browser
  needs a page argument `run_page.mjs` lacks (`:154-156`). Its own entry, so "a
  failure names which path it was" (`tests/render/CMakeLists.txt:312-313`).
- **In process:** chain off and identity on, `snapshot()`
  (`pixel_test_harness.h:108-111`) equal with **zero** drift.
- **On GL** the scene FBO is drawn exactly as framebuffer 0 (§4.4), so this
  anchor does not depend on GL's implementation-defined edge rules **[GK]**;
  only game targets, drawn top row first, could move an edge, and they have
  their own goldens.

### 10.2 Effects are asserted analytically, not by golden

`allowed_channel_drift = 1` is "the only inexact statement in the pixel
contract", and the rule is to find the term, not raise the number
(`golden_image.cpp:385-389`). Linear filtering and `exp` differ by device
**[GK]**, so curved and blurred frames use `end_not_comparable` with every
assertion stated, as the composition and viewport cases already do
(`view_pixel_tests.cpp:21-40`, `:175`, `:203`, `:524`):

| Case | Assertion |
|---|---|
| Region | Every pixel outside `screen` equals the chain-off frame exactly. |
| All strengths zero | The engine draws identity in the CRT's place (§7.1); the frame equals the chain-off frame exactly, and null records the identity material. |
| Never submitted | A frame begun, not submitted and presented, chain on, contributes nothing: the back buffer is cleared (§4.1 item 3). |
| Curvature | Input stripes encode their column in two channels (red high byte, green low), and a test material samples the curvature through `scene_point`, so no filtering interpolates the code. Corners are black; the centre stays fixed within 1 px; decoded columns are monotone along each row; `src(x) + src(w-1-x) = w-1` within 1; each decoded column is within 1 texel of `crt_source_position`. |
| Region clamp | A saturated colour drawn just outside `screen` appears in no pixel inside the curved picture, with aberration at its maximum. |
| Scanlines | On flat grey bands: rows repeat with the period within 1; the dark-to-bright row ratio is larger on the darker band. |
| Mask | On white: period 3 per column, the expected channel dominant in each phase. |
| Vignette | Centre is the maximum, corners the minimum, monotone along diagonals, mirror symmetric within 1. A frame submitted twice equals one submitted once: the first rasterising check of "a second call adds nothing". |
| Aberration | A centred white line shows no split; lines at plus and minus a quarter width split outward symmetrically. |
| Overlay | Overlay views are unprocessed: a stripe pattern drawn after `begin_overlay` reads back exactly. |
| Render target | Draw into a target, draw it 1:1 point-filtered: equal to drawing directly, in process and against a golden; alpha of a transparent-cleared target asserted. |
| Split screen | `check_red_and_blue_frame` holds through identity; with curvature, the vertical pane boundary stays on the centre column. |

Each analytic case amends the three places that list the exceptions:
`pixel_tests.cpp:55-62`, `pixel_test_harness.h:83-88` and AGENTS.md's
pixel-test bullet. A second comparator with its own measured allowance would
amend `golden_image.cpp:385-388` and is E12.

### 10.3 Headless, loss and sync

- **Seam** (`tests/render/renderer_seam_tests.cpp`, all five backends):
  invalid settings throw `std::invalid_argument`; `set_post_effects` inside a
  frame, a second `begin_overlay` and a view sampling its own target throw
  `std::logic_error`; a zero-sized target is refused.
- **Null** (`null_tests.cpp`): one chain per frame whatever the view count; a
  second `submit()` adds none (twin of `:747-755`); an unsubmitted frame
  records none; a resize resizes the scene target. A benchmark asserts post's
  CPU cost is O(1) in sprites and views.
- **CPU twin** `crt_source_position` against a table of points, and a fixture
  script proving a broken material fails the build and names its file.
- **WebGL context loss:** `RenderContextLossWithPostTests`, a post-on twin of
  `tests/render/web/context_loss_tests.cpp:300-415`.
- **Resize:** the three resize cases and the presentation-lifecycle cases with
  the chain on. GL's drag-resize subcase relies on `back_buffer_size` answering
  from the window (`pixel_tests.cpp:255-298`), still true while the scene is
  back-buffer sized.
- **Vulkan `validate_sync`** with the chain on and off, begun-never-submitted,
  submitted-never-presented, mid-frame resize, minimise and restore; the
  D3D12 debug layer over the same (`pixel_tests.cpp:381-385`). Real drivers
  through `ci_gap_report.py --execute`, since CI rasterises GL on llvmpipe and
  Vulkan on SwiftShader.

### 10.4 Inventory and counts

The two entries change, in P1's commit: `tests/ctest_inventory.json`, both
in each configuration's entry list and, for the browser twins of
`RenderPixelTestsThroughPost` and `RenderContextLossWithPostTests`, in its
`chromium_registered` list (`tests/ctest_inventory.json:198`), which the
planner reads; `tools/verify_changes.py`'s GL page acceptance;
README.md's counts, in the same commit though `check_agent_brief.cmake`
does not check them; and the
AGENTS.md table that `cmake/check_agent_brief.cmake` checks: the D3D11, GL and
Vulkan pairs 21 to 22, D3D12 22 to 23, null unchanged at 23, wasm with
Chromium 34/35 to 36/37, wasm without a browser unchanged at 19. A golden that
does change carries its own `Golden-Reviewed:` line.

---

## 11. Performance on the low tier

The low tier is the Ryzen 9000's integrated Radeon, two RDNA 2 compute units
on shared memory, at 1280x720 (PHILOSOPHY, *The target*), never measured.
Everything below is **[GK] estimate**.

| Configuration | Pixels per pass | Traffic per frame | GPU time |
|---|---|---|---|
| CRT, 720p | 921,600 | ~7-15 MB (3 taps, 1 write) | ~0.1-0.5 ms |
| CRT and bloom, 720p | 1/2 to 1/16 pyramid, then the CRT at 1 | ~10-18 MB (no full-resolution composite; one extra tap in the CRT) | ~0.3-0.8 ms |
| CRT, phone 390x844 at DPR 3 | 2,962,440 (3.2x) | ~25-45 MB | unknown; tilers pay per target switch |

- **The empty chain is free:** one branch a frame.
- **Vulkan already copies** its image at present
  (`vulkan/device_resources.cpp:1847-1910`); folding the last pass into it is
  a later optimisation. Direct3D and GL gain a whole pass.
- **Memory:** a scene target is 3.7 MB at 720p, 8.3 MB at 1080p; bloom's
  1/2-to-1/16 pyramid adds about 1.2 MB at 720p; persistence's history target
  adds one more full-size target.
- **A second clear** of the back buffer each frame while a chain is set
  (§4.1 item 3), one full-screen write.
- **Phones** argue for a capped scene resolution (E13).
- **Measuring it.** `LineSweeperFrameBench` measures "neither GPU execution nor
  display scan-out" (PHILOSOPHY, *The target*); GPU time needs timestamps
  (`docs/performance/2026-09-19-g6f-ratchet.md:311`). A bench-only timestamp
  instrument (**[GK]** D3D11 queries, D3D12 query heaps,
  `vkCmdWriteTimestamp`, `GL_TIME_ELAPSED`) runs chain off and on, by
  `docs/performance/local-frame-bench.md`.
- **Proposed budget, the owner's:** CRT at most 1 ms and bloom at most 2 ms at
  720p on the low tier. Until measured: "sized against, not proven on".

---

## 12. Decisions

Each changes something a design document or a public header states. None is
made by writing it down here. An open row has a recommendation and needs a yes
before the milestone that depends on it.

### Open

| | Decision | Options | Recommendation | Answer | What it amends |
|---|---|---|---|---|---|
| **E1** | Who writes post shaders, and how T10 and T12 read | (a) engine-written built-ins only; (b) built-ins first, game materials later; (c) game materials from the start | **(b).** The first tranche ships only engine materials, which keeps T10, T12, the teachability bar and the starter's tools out of the first decision. At the game-material milestone the owner also answers whether game HLSL is presentation data under T10, and whether `compile_shaders.cmake`'s generated headers scope T12 or T12 is amended (§6.5) | | PHILOSOPHY T10 and *Non-goals* (scripting layer), T12, the introductory series' scope, only at the game-material milestone |
| **E2** | How GL and wasm get dxc and spirv-cross | (a) vcpkg host ports; (b) Vulkan SDK required; (c) checked-in GLSL for engine materials, hand-written fallback (§6.6) | **(a)**, after a spike shows both ports exist at the baseline, the release dxc emits SPIR-V and the tools are reachable under Emscripten; (c)-style hand-written GLSL for the CRT if the spike fails. Needed before game materials, and before the CRT only if E19 says so | | `vcpkg.json`, `ReleaseNotices.astro`, `NOTICE`, AGENTS build section, ARCHITECTURE *The build*, `prerequisites.mdx`, `troubleshooting.mdx`, the starter's files (§15), CHANGES.md |
| **E3** | Is the CRT engine-owned, and how the seam carries its values | (a) an engine material, every term off by default, with `set_crt_settings`; (b) sample code a game copies, like `samples/components`; (c) client-owned once game materials exist; (d) an engine material whose values travel through a generic `set_post_values(effect, PostValues)`, with `CrtSettings` packed by a free function or component code (§8.1) | **(d)**: the CRT proves the frame on five backends, but its tuned look is policy (T1), and PHILOSOPHY moves a compound into the engine only once every client copies it unchanged; one generic setter keeps the concrete seam from growing one method per look | | PHILOSOPHY's UI components paragraph (`PHILOSOPHY.md:634-642`) and T1 under (a) or (d); ARCHITECTURE *The tree*; published.ts |
| **E4** | Where the post pass runs | (a) inside `submit()`, between processed and overlay views; (b) `end_frame()` | **(a)**: read-back, captures and pixel tests see the frame that is presented | | `renderer.h` submit and read-back comments; SEAM §6 |
| **E5** | Game target contents across frames | (a) cleared every frame it is bound; (b) a "not cleared" mode, lossy on device loss | **(a)** for v1; (b) when a paint layer needs it | | `renderer.h`; SEAM §6 |
| **E6** | Where a game target lives, and who recreates it after a loss | (a) `RenderResources` keeps each target's description, and a factory call that `Application::on_device_restored` makes beside `add_white_texture` recreates them empty; (b) owned by the Renderer, with `draw_sprite` resolving them some other way | **(a)**: `draw_sprite` resolves it like any texture. The renderer holds only `const RenderResources*` (`renderer.h:364`), so the restore belongs to Application, whose replay already adds what no manifest names (`application.cpp:695-713`). Each recreated target costs a D3D12 descriptor slot (`render_resources.h:96-110`) | | `resource_factory.h`, `render_resources.h`, `application.cpp` restore and its comment |
| **E7** | Target format, and banding | (a) 8-bit UNORM only, with ordered dither on bloom's pyramid writes; (b) 8-bit UNORM, banding accepted; (c) optional float for bloom | **(a)**: identity stays byte-identical, half the memory, no WebGL extension, and the dither answers re-quantisation | | SEAM §5 |
| **E8** | Material parameter block | count of `float4`s: 8, 16 | **8**: the CRT needs 3. Doubling later is a frame change, not an API break, but at 16 the frame is 76 DWORDs and D3D12 moves from root constants to a root CBV (§5.4) | | `post_frame.hlsli`; D3D12's `PostPipeline` under 16 |
| **E9** | Whether the CRT stays on in gameplay, and its regions | (a) off in gameplay, faded during the push; (b) on per pane, several regions per material; (c) on across the whole window, one region; (d) on in both, pronounced in the opening and subtle in gameplay, the gameplay look found by experiment | Curving four panes as one tube bears on the client's exit condition that aim sweeps stay "stable and readable" (ColourWars `docs/polish-plan.md:314-316`) | **(d)**, 2026-11-14, by the client's author. The opening is the pronounced look; gameplay gets subtler filters whose terms and regions are tuned by trying them (§8.3). Consequences: every term is independently zero-able and read from data, the settings lerp from the opening's to gameplay's across the push, and whether gameplay curves per pane, across the window, or not at all is left to the experiment, so v1 must allow one region per material either way | `CrtSettings`; §8.3 |
| **E10** | D3D11 post profile | (a) `vs/ps_4_0` for post only; (b) a vertex buffer to stay at 9_1 | **(a)**: the 10_0 floor already allows it, and 9_1 limits would not hold the CRT | | `windows-release.mdx:289-290`; ARCHITECTURE shared-shader paragraph |
| **E11** | `/WX` on material compiles | (a) off, as `compile_hlsl` today; (b) on | **(a)**: `ps_5_1` alone warns on the early-return idiom | | `compile_shaders.cmake` |
| **E12** | How effect frames are compared | (a) analytic `end_not_comparable` cases only; (b) a second comparator with a measured allowance | **(a)** | | `golden_image.cpp:385-388` only under (b) |
| **E13** | Reduced internal resolution | (a) scene target always back-buffer sized; (b) a cap, for phones and pixel art | **(a)** for v1; (b) with GAP#3's integer scaling | | SEAM §6 `back_buffer_size`; `pixel_tests.cpp:255-298` reasoning |
| **E14** | Citing the client here | (a) by path and line, as this draft does, pinned to a ColourWars revision; (b) generically, as the colourwars-reports folder does | the owner's; this draft uses (a) because the client is public | | this file |
| **E15** | Where the work is tracked | (a) under `GAP#3`; (b) a new row family for the post stage, CRT, bloom and lighting | **(b)**, with GAP#3 re-pointed here: they are separate yeses. Waits on E16 | | `TODO.md:130`, BACKLOG.md |
| **E16** | Has T1's trigger fired | (a) the engine owner's request, made as ColourWars' author, is enough under T1; (b) it needs a ColourWars report through the colourwars-reports process; (c) the trigger is the parked colour-transform ask (ColourWars `TODO.md:89`, `docs/review/gap-audit.md:391`), making the first built-in material a colour matrix rather than a look | the owner's | **(a)**, 2026-11-14. ColourWars' sole developer wants the look; what that developer says is the client's demand, and its documents count for little beside it. No report is filed | TODO.md, BACKLOG.md |
| **E17** | Colour encoding of post maths | (a) gamma-space maths on the stored sRGB-encoded values, chosen deliberately as part of the look; (b) decode to linear in the frame's helpers and re-encode before the write, a few ALU ops a tap | **(b)** for bloom's blur at least, where gamma-space halos darken and fringe **[GK]**; the CRT's terms decided by looking at X2. Separate from §14's out-of-scope sRGB output format | | `post_frame.hlsli`; SEAM §5 |
| **E18** | The frame's size | (a) the minimum the CRT needs: one input, one sampler, the header and three material `float4`s; (b) reserved headroom: four inputs, two samplers, eight `float4`s, an intermediate pool | the owner's. T1 rules out speculative frameworks ("generalise only as far as real clients demand", `PHILOSOPHY.md:45-47`); growing the frame later is an internal change, as E8 argues for the parameter count. §5.4 sketches (b) | | `post_frame.hlsli` |
| **E19** | Sequencing the CRT against the toolchain | (a) the CRT with hand-written GLSL in P2, moving X3, E2 and the toolchain to P6 with game materials; (b) a D3D11-and-null preview the client can try from a work-branch pin after X1 and X2, other backends refusing a chain with a T6 message until P1, against AGENTS' rule that every render change is checked on five backends; (c) as first drafted, the CRT cross-compiled in P2 | **(a)**. Calendar time to the first frame ColourWars can show: about 7-9 weeks to a release tag under (a); about 2 weeks to a work-branch pin under (b), not landable on master until P1; about 8-10 weeks under (c). Each adds the client's pin move | | §13; AGENTS verification boundaries under (b) |

Anything else this plan meets that would change a design document comes back
here as a new row before it is built.

---

## 13. Milestones, and what to spike first

### Spikes, before any milestone is committed to

- **X1 - identity post on D3D11 and null.** *Days.* A scene target, the
  frame's vertex entry, an identity material at `vs/ps_4_0`, `begin_overlay`,
  and null's `recorded_passes`. Done when `RenderPixelTests` on `x64-debug`
  passes through identity with **zero changed PNGs**, on WARP and on the
  desktop GPU, and §10.3's null cases pass on `x64-debug-null`.
- **X2 - the CRT on D3D11.** *Days.* §7.1's material through fxc, the region,
  `crt_source_position` and §10.2's analytic cases. Looked at on screen in a
  throwaway build of the opening's shape (a bezel sprite, a picture, a push
  that lerps from the pronounced look to a subtle gameplay one, E9 (d)), with
  the strengths read from a file so looks can be tried without rebuilding;
  not in ColourWars.
- **X3 - the cross-compile path.** *Days.* `post_frame.hlsli` and the CRT
  through vcpkg `directx-dxc` and `spirv-cross` under `x64-debug-gl` and
  `wasm-debug`; the embed script's renames and highp rewrite; whether the
  release dxc emits SPIR-V; whether the host tools are found under Emscripten.
  Its answer settles E2.

### Milestones

| | Milestone | Size | Done when |
|---|---|---|---|
| **P0** | Spikes X1-X3 (X3 moves to P6 under E19 (a)) | week | As above; E1, E2, E4, E16 and E19 answered |
| **P1** | Scene target, identity chain and overlay split on all five backends | weeks | `RenderPixelTestsThroughPost` green on the eight native rasterising presets and `wasm-debug`/`wasm-release` with no changed PNG (which needs `run_page.mjs` to take a page argument it lacks today); the never-submitted case of §4.1 item 3 green with the chain on; `validate_sync` and the D3D12 debug-layer sweeps clean; context-loss twin green; the inventory, `verify_changes.py`, README.md, AGENTS and SEAM amendments of §10.4 and §15 in the same commit |
| **P2** | The CRT, built in | weeks | §10.2 analytic cases green on every rasterising backend; looked at on D3D11, GL, Vulkan and in Chrome, including the push for moiré; the CRT drawn once on the browser pixel page. Under E19 (c) also E2's tooling: host ports under the Emscripten triplet, CI's GL and wasm jobs, the consumer jobs and `ReleaseNotices.astro` |
| **P3** | An engine release a client can adopt | days | A tagged release carrying P1 and P2, and a sample or test reproducing the opening's shape (a bezel, a picture, a push, flat notes over it). ColourWars' adoption is the client's own follow-up, or a colourwars-reports row if the client files a report; it is not tracked here |
| **P4** | Game render targets (GAP#3) | weeks | §8.4 on five backends; target refusals; restore recreates empty targets |
| **P5** | Multi-pass chains and bloom | week | §7.3's chain; bloom's analytic halo cases; a GPU-time measurement against the budget |
| **P6** | Game materials | week | `labrador_add_material` from the starter on `x64-debug` and `wasm-debug`; the consumer jobs build one; a guide to writing one |
| **P7** | Persistence | week | An engine-owned history target on every backend, copied at `end_frame`, cleared on resize and loss; decay from the caller's frame time, the same at 60 Hz and 144 Hz; the never-presented golden unchanged |

P1 is roughly 5-7 weeks: the per-backend sizes in §4 alone add up to about
4 weeks for the scene target and pass, and P1 adds the overlay split on four
backends, the second clear, the browser and context-loss twins, the
validate_sync and debug-layer sweeps, possible GL triage and the amendments.
GL and Vulkan are the larger shares. P2 is about 2 weeks; P0-P3 roughly 8-10
weeks; P0-P7 roughly 4 months. In TODO's scale P0-P3 is **XL**; GAP#3 alone
stays **L**.

### Risks, in the order to retire them

1. **An incomplete redirect** (a bind, clear or restore path): X1's run.
2. **Tools that do not reach the browser build:** X3, before P1.
3. **A GL edge tie-break in game targets**, drawn top row first; the scene
   FBO is drawn as framebuffer 0 is, so identity does not carry this risk
   (§4.4). P4's target goldens on llvmpipe, a real driver and Chrome.
4. **Vulkan synchronization** (`docs/review/vulkan/README.md:151-175`): P1's
   `validate_sync` sweep.
5. **Low-tier cost**, unmeasured until the bench runs on that part.

---

## 14. Out of scope, on purpose

- **Per-draw materials or blend modes.** The BlendMode decline stands (§5.3).
- **A second authored shading language.** HLSL only, as web.md declined WebGPU
  for "a third shading language (WGSL)" (`docs/port/web.md:1187-1188`).
- **Multisampled targets**, which the survey lists after GAP#3
  (`docs/survey/2026-11-10.md:527`); **sRGB or HDR output**.
- **A post-processing library**, bought or vendored (T9).
- **3D, z-buffers and depth testing.** Permanent (PHILOSOPHY, *Non-goals*).

---

## 15. The documents this amends

Each amendment lands in the commit that earns it, under the rule that
`docs/design/` changes in the same commit as the change that conflicts with
it. **None is made in advance.**

| Document | What changes | Earned by |
|---|---|---|
| PHILOSOPHY, *Targets and layout* | A backend's inventory gains a render target and a post pipeline; "a shader" becomes the sprite shader and the post frame; where a pixel goes is still engine code, now in shared HLSL too | P1 |
| PHILOSOPHY T9 | dxc and spirv-cross as build tools at the shader-format edge | P2 (if E2 is a or b) |
| PHILOSOPHY T12 | Scoped to C++ a reader meets, citing the generated shader headers as precedent, or amended: the owner's answer under E1 (§6.5) | P6 (P2 under E19 (c)) |
| PHILOSOPHY T10, *Non-goals* | A game material is presentation data, not logic, or T10 amended | P6 |
| ARCHITECTURE, *The tree* and the shared-shader paragraph | `sprite.hlsl` stops being "the only shader"; `post_frame.hlsli` and materials; the profiles and bindings per backend | P1, P2 |
| ARCHITECTURE, backend translation units and *The build* | `PostPipeline` in D3D12's `sprite_pipeline.cpp`; vcpkg host tools | P1, P2 |
| `engine/render/SEAM.md` §4, §5, §6, §7 | Second shader; full-screen passes replace while draw lists blend; GL's top-row-first targets; what `begin_frame` resets; read-back is post-post; analytic comparison for effects | P1, P2 |
| `engine/render/renderer.h` | `set_post_effects`, `set_crt_settings` or `set_post_values` (E3), `begin_overlay`; the one-blend comment scoped to draw lists; submit's and read-back's comments | P1 |
| `engine/render/post_effect.h`, `resource_factory.h` | New public names | P1, P4 |
| `cmake/compile_shaders.cmake` | Shifts as arguments; GLSL output; the generated root as a global property; `labrador_add_material` | P1, P2, P6 |
| AGENTS.md | "one `sprite.hlsl` at three profiles; GL compiles GLSL at device creation"; tools per preset; backend TU list; preset counts; pixel-test exception list | each milestone |
| `tests/ctest_inventory.json` (entry lists and `chromium_registered`), `tools/verify_changes.py`, README.md's counts | Two new entries and their browser twins (§10.4) | P1 |
| `website/src/lib/published.ts` | Each new header published or excluded | each header's commit |
| CHANGES.md, `ReleaseNotices.astro`, `NOTICE` | New dependencies; the `begin_overlay` capacity note | P1, P2 |
| `windows-release.mdx`, `prerequisites.mdx`, `troubleshooting.mdx`, `sprites-and-text.mdx` | Post profile; tools; blend scope | P2, P6 |
| `samples/minimal/README.md` change list | Only if the template builds materials | P6 |
| `TODO.md:130`, BACKLOG.md | GAP#3 re-pointed here; new rows per E15; stories before work (`TODO.md:124`) | the owner's yes |
| PHILOSOPHY, UI section's components paragraph, and T1 | Whether a tuned look ships in the engine before clients copy it unchanged (E3) | P2, under E3 (a) or (d) |
| `docs/port/web.md` | "`compile_shaders.cmake` is never reached on `gl`" (`docs/port/web.md:539`) and the WebGL2 shader-path decisions around it | P6 under E19 (a); P2 under (c) |
| `docs/verification.md` | The planner contract `tools/verify_changes.py` implements: the new entries and GL page acceptance | P1 |
| `engine/app/application.cpp` restore comment; `engine/app/APPLICATION.md` | What `on_device_restored` re-adds, under E6 (a); APPLICATION.md states no restore order today, so it gains one only if the restore gains a step worth stating there | P4 |
| The starter: `website/src/walkthrough/vcpkg.json`, `CMakePresets.json`, `root-CMakeLists.txt`; `tools/consumer_check/README.md`; `starter_zip.py` and `rename_check.py` expectations | Host ports and the generated-root property. A starter that writes no material needs no host tools under E19 (a); under P6 a wasm starter that writes one does | P6 (P2 under E19 (c)) |
| **Not amended** | `docs/survey/2026-10-27.md`, `docs/survey/2026-11-10.md`, `docs/2026-11-04-decision-recommendations.md`, `docs/feature-candidates.md`: dated records, cited as the trigger having fired | - |
