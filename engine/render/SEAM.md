# The renderer seam

Why [`renderer.h`](renderer.h) has the shape it does. That file states the
contract a **caller** is held to; this one states what a **backend** is, what it
may never decide, and what holds the five to one another.

It sits here rather than in `docs/design/` because those documents are written
in the present tense of the target and deliberately say nothing about the
current codebase, and all of this is about the current codebase. It is beside
the code for the same reason a backend's `backend.h` is: a reader in this folder
should trip over it.

**It changes by amendment in the same commit as the change that fights it.**
That is the rule `docs/design/` keeps, and prose one directory away from its
subject needs it more, not less — the twenty-eight claims in
`docs/review/backend-equivalence/DRIFT.md` were believed to be a live list for
nine months and were not.

---

## 1. What the seam is for

Two things, and only two: **headless tests**, and **an eventual second
platform**. Before this file, nine of the engine's translation units were
untestable on one include — `<SpriteBatch.h>` at the top of
`engine/core/game_object.h` — which is why `tests/` had an assets, a core, a
math, a collision and a ui folder and no render one.

Neither purpose needs two backends live in one process. That is the whole
justification for the next section.

**Both purposes are filled, and neither is a claim any more.** `gl/` is OpenGL
3.3 core and passes `RenderPixelTests`; `null/` has no graphics API at all and
records what it was asked to draw, which is what lets a test assert which
sprites a frame submitted on a machine with no driver. Writing either changed
nothing above the seam.

**And a third nobody had written down**, which `d3d12/` is the answer to. Every
API behind this seam hid the CPU/GPU boundary until that one: D3D11 renames a
mapped buffer for you and tracks what is still in flight, OpenGL's driver does
the same, and the null backend has no GPU to be out of step with. So nothing had
ever asked whether these methods still describe a frame when the *engine* owns
the fence — when a command allocator may not be reused until the GPU says so, a
vertex page written this frame is still being read next frame, and a texture
upload is a copy somebody has to wait for.

They do, and not one signature moved. What the port added is one line inside a
backend's `begin_frame`, waiting on a fence before anything resets an allocator,
which no caller can see.

## 2. Concrete class, not an abstract base

`Renderer` is a class with one implementation chosen at build time, not an
interface with a vtable.

- **T8.** A customisation point that taxes the frame loop is a customisation
  point that goes. A virtual `draw_sprite` is that tax in the module that draws
  thousands of sprites per frame, and it is a *new* tax: the per-sprite cost is
  one out-of-line call that builds four vertices and appends them to a batch,
  and a direct call through this seam is the same shape. An indirect branch
  through a vtable, in that loop, is not.
- **T5.** A compile-time choice fails at link, not at run time. Asking for a
  backend that was not built is a missing symbol.
- If a real client ever needs runtime selection, promoting a concrete class to
  an interface is mechanical and no call site changes. That option is held, not
  spent — the same escalation ARCHITECTURE.md describes under *Modules*.

## 3. How one header serves five backends

`Renderer` holds a pimpl and `DrawList` holds a raw pointer to per-view state
the backend owns; each backend defines `Renderer::Impl` and `DrawList::View` in
its own translation unit under `engine/render/<backend>/`. `DrawList` stays
trivially copyable, so passing one costs nothing, and the per-draw cost is a
single out-of-line call.

**What is deliberately absent** from `renderer.h`: no `ID3D11*` type, no
`DirectX::` type, no batch object, no sampler-state pointer, no device accessor.
Creating a texture from a file is a resource factory's job, not a renderer's,
and `RenderResources` already speaks in handles — so only the handle's payload
type changes when the backend does.

DirectXTK is no longer on the render path at all. It remains bought for audio
and for the gamepad reader, which are seams of their own.

## 4. The shape of a backend

Three translation units every backend has — `renderer.cpp`,
`render_resources.cpp` and `texture_factory.cpp` — plus whatever it needs to
build its shader, plus at most one more for the API itself. Only `null/` stops
at three: `d3d11/`, `d3d12/` and `vulkan/` each add `device_resources.cpp` and
`gl/` adds `gl_functions.cpp`, and all four are the same kind of file — the part
of an API that is not about drawing.

**The third file is where they diverge most**, and is the honest measure of what
a port owes for content. It turns already-decoded bytes into a texture, and how
much code that takes is **inversely proportional to how much the API will accept
unchanged**. Stated as an ordering and not as line counts, which nothing checks
and which read as authoritative long after they stop being true.

`null/` is shortest: it stores the bytes and stops. Then `d3d11/`, handed them
and copying them itself; then `gl/`; then `d3d12/`, which wants a resource, a
staging buffer, a footprint per mip level, a copy on a command list, a barrier
and a wait — the argument for the fourth backend in one file. **`vulkan/` is the
longest, and the extra is not the copy**: that one takes the engine's own tightly
packed bytes, where D3D12 pads every row to 256. It is that nothing in that API
owns anything — an allocation to bind to the image, a memory type chosen for it
by hand, and a handler putting all three back on every path that can throw,
because there is no `ComPtr`.

Path-building and file-reading are in `engine/render/resource_factory.cpp`,
written once for everybody.

**The second file is where they diverge least.** `render_resources.cpp` is a
constructor, a destructor, two moves and two lookups. The rest is
`engine/render/render_resources.cpp` — a shared file with a backend sibling's
name and no backend in it — because two of the three resource tables hold engine
data and only the third was ever hiding anything.

**The shader is not a backend's at all.** `engine/render/sprite.hlsl` is one
file compiled three times, at a profile each backend picks and into a byte array
each keeps to itself, because the source is character for character the same and
a second copy of it could silently disagree with the first. What a backend owns
is the profile and how it binds `b0` — a constant buffer on one, four root
constants on the next, a uniform buffer at a shifted descriptor binding on the
third. **One difference reaches the shader**: the declaration order of
`VertexIn`'s three members is an ABI term on the Vulkan backend, because `dxc`
assigns SPIR-V locations in declaration order and a Vulkan pipeline binds
attributes by number rather than by semantic. Two of the three go through `fxc`
into DXBC and one through the Vulkan SDK's `dxc` into SPIR-V — a second compiler
for one unchanged source, not a second source.

## 5. What is on which side of the line

**Nothing a backend does decides where a pixel goes.** Every decision that shows
on screen is the engine's: which glyph goes where ([`font.h`](font.h)), what a
`.dds` and a `.spritefont` say ([`dds_file.h`](dds_file.h),
[`sprite_font_file.h`](sprite_font_file.h)), and where a sprite's four corners
land and what they sample ([`sprite_geometry.h`](sprite_geometry.h)).

What a backend supplies is a device, a texture from bytes, a vertex buffer, a
shader that multiplies each vertex by one constant, and the states that make the
blend premultiplied. That is what lets the four backends with a rasteriser pass
the same assertions — and what makes the file asserting them say "the renderer",
never "this renderer".

The one term a backend still decides is **where a pane sits in the buffer**, and
the three answers are the map of the folder: Direct3D measures down from the
top, GL up from the bottom, and Vulkan hands the rasteriser a negative viewport
height so that one shader serves all three.

## 6. Terms `renderer.h` defers here

### Why the camera is on the `DrawList`

Every drawable used to take one and do the conversion itself, and the whole of
that conversion was nine lines across three files — every other `draw()` in the
engine carried a `Camera` parameter only to hand it down. Twenty override sites
lost a parameter.

### Why level zero and never between levels

A mip level is chosen per pixel from screen-space derivatives, and both APIs let
an implementation approximate that computation. A chain would therefore put
"which texel" in the same class as the things §5 says a backend never decides.
Chains are read and uploaded; nothing samples from them.

### What each backend resets in `begin_frame`

The five have four different things to reset, which is why "a frame begun and
never submitted contributes nothing to the next one" is worth stating:

- **`gl/`, `null/`, `vulkan/`** hold a frame in vectors — dropping it is
  clearing the vector.
- **`d3d11/`** holds it in a deferred context, which keeps what was recorded
  until something takes the command list away, so it has to drain as well as
  forget.
- **`d3d12/`** holds an open command list, which cannot be reset and whose
  allocator cannot be reset under it, so it has to be closed before its memory
  can be reused.
- **`vulkan/`** is the fourth kind: clearing its vectors is not the whole of it
  — the command pool and the descriptor pools are reset too, and the tracked
  image layout goes back to what the last submit left, since the barriers that
  moved it were in what was thrown away.

### What `submit` costs

Three of the five replay a vector. **`d3d11/`** runs a protocol — record,
`FinishCommandList`, `ExecuteCommandList`, `Release` — that was hand-written in
four places, each of which had to pre-size a vector, pre-fill it with null and
`Release` every non-null entry: three caller obligations stated nowhere, and two
of the four call sites already disagreed about `RestoreContextState`. There is
one copy now and it is not the caller's. **`d3d12/`** hands the finished lists to
its queue as one array in view order, in a single `ExecuteCommandLists` — the
one submit shape a fourth backend actually introduced. The fifth introduced
none: `vulkan/` replays vectors like `gl/`, because a `VkCommandPool` may not be
used from two threads at once and a view's vertices are already built on the CPU
before any backend sees them.

### Why `read_back_buffer` exists

It is what makes the first purpose in §1 real. Until it, there was no way for a
test to observe a single thing the renderer had drawn — so every term of the
pixel contract (what the blend equation is, which way y runs, what `origin` is
measured in, what happens to a fractional destination) was decided by whichever
library the backend happened to call and written down nowhere. A seam whose
output nothing can read cannot be held to a contract, however many backends fill
it.

It answers RGBA regardless of what the backend stores. Three of the four
rasterisers' buffers are BGRA and all three swap on the way out — both Direct3D
ones and the Vulkan one, whose colour target is `B8G8R8A8_UNORM` for exactly
that reason. The D3D12 one additionally unpads a row pitch its API rounds up to
256 bytes; the GL one is asked for as RGBA and only flipped, because GL reads
from the bottom. One swap and no flip on three, one flip and no swap on the
fourth.

## 7. What holds the five to one another

**Four runs and one set of images.** An assertion holds *one* backend to a
relationship, and hand-copied implementations can get the same relationship
wrong in the same direction without any run noticing. So every frame a case
reads back is also compared byte for byte against a PNG of it in
[`tests/render/golden/`](../../tests/render/golden/), and those images are what
hold the backends to each other rather than each to a sentence. Fifty-seven
frames, four of which fill more than one view — the machinery the backends share
least. On one machine's GPU, `d3d11`, `d3d12`, `gl` and `vulkan` reproduce all
fifty-seven exactly.

**The running of it is still separate processes.** `LABRADOR_RENDER_BACKEND`
picks a backend at configure time (T5), so the checked-in set is what passes
between them. Two of the four happen on the build machine as well, which is the
reason the fourth backend is a Direct3D one: a runner has no GPU and Direct3D
gets a device there anyway, where OpenGL falls back to GDI 1.1 and Vulkan has no
in-box fallback at all.

Two terms sit outside the images, both stated where they are decided:
`Harness::end_not_comparable` holds the three frames that are not 64x64, and
`ALLOWED_CHANNEL_DRIFT` in `golden_image.cpp` is the per-channel allowance that
lets one set serve two adapters.

[`tests/render/renderer_seam_tests.cpp`](../../tests/render/renderer_seam_tests.cpp)
is the part that runs in all five configurations, being everything the seam
answers without a device.

## 8. Settled: `AssetKind::reload_device` stays on the loader

It was the one open question at the foot of `renderer.h`. What was open: a
public `std::function` on the asset loader that exists only because a device can
be lost, which two of the five backends never call — a WGL context is not lost,
and the null one has nothing to lose — so a caller writes a rebuild path for a
hazard its configuration may not have.

**Not on `DeviceNotify`, which already carries the half it can carry.** That
interface is the *event*; the shell hears it and calls the loader. What it
cannot carry is *what to rebuild*. A rebuild has to refill the slots the old
resources sat in rather than make new ones, because a drawable holds a handle, a
handle names a slot and a slot belongs to a name — and the only thing that knows
the names is the manifest, which is what the loader keeps. Move the rebuild to
`DeviceNotify` and a game keeps a second list beside the manifest.

**Not nowhere.** A device is lost on three of the five, so the hazard is most of
this seam's configurations and not a Direct3D 11 peculiarity.

**And the premise that made it look misplaced is the part that was wrong.** "A
hazard its configuration may not have" is a statement about a build, not about a
caller. `LABRADOR_RENDER_BACKEND` picks a backend at configure time over one
game source, so the same `register_kind` call compiles into a D3D11 build and
into a GL one; what varies is which build ever *runs* the path, never which
source has to write it. A seam whose caller contract changed per backend is the
thing this seam exists to prevent — and `gl/renderer.cpp` and `null/renderer.cpp`
both drop a frame on a resize that destroys nothing of theirs, for exactly that
reason.

The criterion a caller applies is written on `AssetKind` itself — *does the GPU
hold this asset*, never *can this backend lose a device*. What a restore does is
pinned by `tests/assets/resource_loader_tests.cpp`, which needs no device and so
runs in all five configurations.
