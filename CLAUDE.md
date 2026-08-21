# Labrador — notes for Claude

A 2D game engine in C++20, Windows-only, built with CMake + vcpkg. This
repository is the **engine half of a split**: the client that drives it,
ColourWars, lives in its own repository and consumes this one as a submodule.
Nothing here depends on it — this tree builds, tests and benchmarks standalone,
and `samples/` holds the only two clients in it: `minimal` (~380 lines), the
new-project template you copy, and `linesweeper`, a falling-block game you read
— whose own [README](samples/linesweeper/README.md) records its design
decisions and is the first thing to read before changing it.

Start from [README.md](README.md). The three design documents are authoritative
on intent: [PHILOSOPHY.md](docs/design/PHILOSOPHY.md) (the trade-offs T1–T12,
and the review vocabulary — "T3: take the simpler model" is a complete comment),
[ARCHITECTURE.md](docs/design/ARCHITECTURE.md) (targets, tree, module table),
[CONVENTIONS.md](docs/design/CONVENTIONS.md) (naming).

## Build, test, run

```
cmake --preset x64-debug          # or x64-release
cmake --build --preset x64-debug
ctest --preset x64-debug
```

`VCPKG_ROOT` must be set — `CMakePresets.json` reads it for the toolchain file
and configuring fails without it. For the two Direct3D backends `fxc` must also
be on `PATH`, because they compile [engine/render/sprite.hlsl](engine/render/sprite.hlsl)
at build time into a byte array
([cmake/compile_shaders.cmake](cmake/compile_shaders.cmake)) — one source, two
profiles, two generated headers; the GL backend compiles its GLSL at device
creation and needs no tool. Both come with the Visual Studio install.
**The Vulkan preset is the one exception to that**, and it is the only thing in
this repository that has to be installed: it needs the
[Vulkan SDK](https://vulkan.lunarg.com/) — `VULKAN_SDK` set — for its headers,
its import library and a `dxc` that can emit SPIR-V from the same `sprite.hlsl`.
There are two `dxc.exe` on a normal machine and only one of them has a SPIR-V
backend compiled in; `compile_shaders.cmake` says which, why it is looked for in
`$VULKAN_SDK/Bin` and nowhere else, and what the error looks like when the wrong
one is found. Ninja generator, out-of-source in `out/build/<preset>/`.

**There are five render backends**, chosen by `LABRADOR_RENDER_BACKEND` at
configure time — so asking for one that was not built is a missing symbol at
link (T5). A change to anything in `engine/render/` should be checked against
all five; CI builds all five.

| Preset | Backend | ctest |
|---|---|---|
| `x64-debug`, `x64-release` | `render/d3d11/` | 12 entries; WARP fallback in debug |
| `x64-debug-d3d12` | `render/d3d12/` — the one where the engine owns the fence | 12 entries; WARP fallback in debug |
| `x64-debug-gl` | `render/gl/` — GL 3.3 core via WGL, same Win32 window | 12 entries; needs a real driver |
| `x64-debug-vulkan` | `render/vulkan/` — the one that reaches other platforms | 12 entries; needs a driver and the Vulkan SDK |
| `x64-debug-null` | `render/null/` — no graphics API; records draws | 11 entries; `RenderPixelTests` is not built |

`RenderPixelTests` is the pixel contract and needs a device. The null backend's
`read_back_buffer` throws saying so, and [tests/render/null_tests.cpp](tests/render/null_tests.cpp)
— compiled only in that configuration — asserts the other half: which sprites a
frame submitted, in what order, from which texture, into which view. Twelve ctest entries: `MattMathTests`, `CoreTests`,
`CollisionTests`, `SceneTests`, `RenderTests`, `RenderPixelTests`,
`InputTests`, `UiTests`, `AssetsTests`, `AppTests`, `LineSweeperTests`
(doctest) and `Benchmarks`. `RenderPixelTests` is the only one that creates a
device — a hidden window and a WARP fallback in debug under `x64-debug` and
`x64-debug-d3d12`, a WGL context under `x64-debug-gl`, a `VkDevice` under
`x64-debug-vulkan` — and asserts on the
pixels
`Renderer::read_back_buffer` hands back. It is the only test that rasterises
anything, and the executable statement of the pixel contract every backend with
a rasteriser has to reproduce. It runs against one backend at a time — but it no
longer follows that they are never compared: every frame it reads back is also
checked byte for byte against a PNG of it in
[tests/render/golden/](tests/render/golden/), which is one set of images that
all four rasterising backends are held to. CI checks two of them, because
Direct3D falls back to WARP on a GPU-less runner where OpenGL falls back to GDI
1.1 and Vulkan has no in-box fallback at all — its software implementations are
installed rather than shipped. Regenerate with `LABRADOR_GOLDEN_DUMP=1`
and **review every image it changes** — a regeneration that is not looked at
turns the contract into a recording of whatever the code does now.
Two terms sit outside the images and both say so where they are decided:
`Harness::end_not_comparable` in [pixel_tests.cpp](tests/render/pixel_tests.cpp)
holds the three frames that are not 64x64 — one whose size the seam makes
backend-specific, two that resize to 32x32 mid-frame — and
`ALLOWED_CHANNEL_DRIFT` in
[golden_image.cpp](tests/render/golden_image.cpp) is the per-channel allowance
that lets one set serve both a hardware adapter and the WARP one CI has, with
the measurement that set it. What runs in all five configurations is
[tests/render/renderer_seam_tests.cpp](tests/render/renderer_seam_tests.cpp) —
everything the seam answers without a device. The
samples land at `out/build/x64-debug/samples/minimal/MinimalSample.exe` and
`out/build/x64-debug/samples/linesweeper/LineSweeperSample.exe`.

`LineSweeperTests` links no engine at all — the sample's rules are a static
library that links only `labrador_settings` — so the whole falling-block game
runs there with no window and no device. A rule is asserted rather than played.

## What will fail the build

- **`/W4 /WX /permissive- /sdl /fp:precise`**, with **zero suppressions**. One
  `INTERFACE` target, `labrador_settings` in [cmake/settings.cmake](cmake/settings.cmake),
  carries it; every real target links it. `/fp:precise` is load-bearing, not
  inherited — exact `operator==` against `Vector2F::ZERO`, tolerance
  assumptions and NaN propagation all depend on it.
- **An engine file including a game header.** Standalone that is now the
  compiler's own error, but [cmake/check_engine_includes.cmake](cmake/check_engine_includes.cmake)
  greps for it on every build anyway, because the compiler only enforces it
  when this repository is built standalone.
- **A file outside `engine/render/<backend>/` including *any* header in that
  folder.** Second pass in the same script. It guards the folder rather than
  one filename in it, and it reads headers as well as `.cpp` files, because
  `device_resources.h` beside `backend.h` is how the backend escaped last
  time — so naming `engine/render/gl/gl_functions.h` from `engine/app/` fails
  the build exactly as naming `backend.h` does.
- **Adding a source file without listing it.** Sources are enumerated
  explicitly in [engine/CMakeLists.txt](engine/CMakeLists.txt) and each test
  folder's own `CMakeLists.txt` — no globbing. A new `.cpp` that nobody lists
  silently is not compiled.

## Rules that are not checked, and matter anyway

- **Dependencies point one way, toward `math`.** The module table is
  ARCHITECTURE's; `core` is the only module everything may lean on, `app` is
  the only one allowed to depend on everything, and nothing may point back at
  `app` or at `assets`. A module is a folder inside `engine/`, not a build
  target — the walls are include discipline plus review.
- **Includes are written from the repository root**: `#include
  "engine/render/renderer.h"`. Own header first, then engine, then external,
  then standard library. `#pragma once`, never include guards.
- **Naming**: PascalCase types, snake_case everything else, SCREAMING_SNAKE
  for macros alone. Trailing underscore on private members (`frame_time_`),
  bare on public struct fields. Accessors are the noun (`bounds()`, never
  `get_bounds()`). Never an `I`/`M`/`F` type prefix, never `m_`, never
  SCREAMING constants, never `using namespace` in a header.
- **`update()` writes, `draw()` is `const` all the way down** and takes what it
  needs as parameters. The parallelism axis is views, not objects — several
  workers enter `draw()` on the *same* object at once, so the pure read is
  load-bearing, not a convenience.
- **Platform code lives behind seams**: `render/d3d11/`, `input/xinput/`.
  `Renderer` is a concrete class with one implementation selected at build
  time, not an abstract base — T8 does not permit a virtual call per sprite.
  Every backend has the same three translation units — `renderer.cpp`,
  `render_resources.cpp`, `texture_factory.cpp` — and at most one more, being
  the part of an API that is not about drawing: `d3d11/`, `d3d12/` and
  `vulkan/` each add `device_resources.cpp`, `gl/` adds `gl_functions.cpp` and
  compiles its GLSL at device creation, `null/` adds nothing. The HLSL three of
  the five compile is **not** in any of their folders — it is
  `render/sprite.hlsl`, one file at three profiles through two compilers, and
  that file says why. Note the name collision, because it is
  deliberate: [engine/render/render_resources.cpp](engine/render/render_resources.cpp)
  is a shared file, and the backend one beside it holds only the calls that
  touch a texture. Two of the three resource tables hold engine data and are
  members of `RenderResources` itself, so their methods — `measure_text` and
  `first_unrenderable` among them — are compiled once rather than once per
  backend. Everything a backend owns lives in
  `render/<backend>/`. Nothing outside that folder includes anything from it,
  the shell included: it hands its window handle to `create_device` as a
  `void*`. `check_engine_includes.cmake` fails the build for a file that
  reaches across, headers included.
- **Nothing a backend does decides where a pixel goes.** The glyph walk
  ([render/font.h](engine/render/font.h)), both file readers
  ([dds_file.h](engine/render/dds_file.h),
  [sprite_font_file.h](engine/render/sprite_font_file.h)) and the quad
  arithmetic ([sprite_geometry.h](engine/render/sprite_geometry.h)) are engine
  code, tested headlessly, shared by every backend. A backend supplies a
  device, a texture from bytes, a vertex buffer, a shader, and whatever its API
  spells the blend, the rasteriser state and the two filters as — five state
  objects on `d3d11/`, one pipeline state object and two samplers on `d3d12/`,
  two sampler objects and some `glEnable` on `gl/`, one pipeline and two
  samplers on `vulkan/`, none at all on `null/`. The
  one term a backend still decides is where a pane sits in the buffer, and the
  three answers to it are the map of the folder: Direct3D measures down from
  the top, GL up from the bottom, and Vulkan hands the rasteriser a negative
  viewport height so that one shader serves all three. `gl/backend.h` and
  `vulkan/renderer.cpp` each say what theirs costs. What `d3d12/` decides that
  no other backend does is **when** rather than where: frames in flight,
  fence-gated allocator reuse, a per-frame vertex ring and an upload the load
  path waits for. None of it reaches the seam, which is why that backend
  exists — `d3d12/backend.h` states the claim, and `vulkan/` is the second
  answer to it, a timeline semaphore being an `ID3D12Fence` spelt differently.
  What `vulkan/` decides that no other backend does is what a **back buffer**
  is: the frame is drawn into an image the engine owns and blitted into a
  swapchain image at present, because the seam permits a frame that is never
  presented and a swapchain image does not. `vulkan/device_resources.h` carries
  that argument and it is the largest decision in the port.
- **A new public primitive ships with behavioural tests in the same commit.**
  Benchmarks assert on **complexity class**, not wall-clock — a phase linear in
  the object count must stay linear when the count quadruples, whatever the
  machine.

## Reading the documents correctly

- `docs/design/` is written in **the present tense of the target**. It
  describes the destination, deliberately says nothing about the current
  codebase, and **changes by amendment in the same commit as the change that
  fights it** — a change that contradicts a philosophy means either the change
  is wrong or the philosophy is, and if the philosophy is, say so there with
  the reason. Several commits in the history do nothing else.
- `docs/review/` is **historical**. It is the review as written and is not
  updated as findings are fixed. Both full rounds were written while the
  paint-shooter was still in this tree, so any finding citing `game/...` refers
  to code that now lives in ColourWars — do not go looking for those files
  here, and do not treat their line numbers as current. `docs/review/rtcd/` is
  a candidate list mined from Ericson's *Real-Time Collision Detection*, not a
  plan; the decisions live in `docs/review/round-2/PLAN.md`.
  `docs/review/backend-equivalence/`, `docs/review/d3d12/` and
  `docs/review/vulkan/` are the three exceptions to the `game/` caveat, because
  all three postdate the split. The first
  holds the three render backends that existed when it was written against one
  contract, and its `DRIFT.md` is a live list of comments in `engine/render/`
  that the code no longer matches. The second reviews the fourth backend
  against the seam it was written to test. **Its three must-fix findings, all
  seven should-fix, every minor and the one item it left unresolved have been
  applied** — nine commits, each naming the section it answers, `6ae4a15`
  through `06f0b5f`. What has *not* been applied is section 6: nine findings
  that ranked below the review's own verification budget and were never checked,
  which are questions rather than findings. Two of them were counts a reader can
  check and went in with the rest; the other seven are behaviour and need
  verifying before anybody acts on them. The document itself still reads as it
  was written, line numbers included, so do not expect them to match.
- **The third reviews the fifth backend, and none of it has been applied yet.**
  `docs/review/vulkan/` holds `engine/render/vulkan/` against the same seam:
  three must-fix, fifteen should-fix, eight minor, and sixty-five findings that
  ranked below its verification budget and were never checked, which are its
  section 7 and are questions rather than findings. It is the first review in
  this tree that **ran** anything — its section 2 is `RenderPixelTests` under
  the Khronos validation layer with `validate_sync` on, which is not what the
  layers check by default and is not what this backend was written against.
  That sweep found two synchronisation hazards on the present path and both are
  findings below it, so **re-run it after any change under
  `engine/render/vulkan/`**: section 2 carries the `vk_layer_settings.txt` that
  makes the layer's output visible to a shell, which the backend's own messenger
  (it writes to `OutputDebugStringA`) does not.

## Known-absent, on purpose

An action-mapping layer over the input devices — neither client has a rebinding
screen, so a binding table would be the speculative framework T1 rules out.
That is the whole list now; every render backend that was planned has landed.

This section used to end "a fifth is not planned and is not refused either —
Vulkan was weighed against D3D12 and lost on cost, not on principle — but the
seam claim a fourth backend existed to test is now tested, so a fifth would
need a reason of its own." **It found one, and it was not a seam claim.**
`render/vulkan/` is here because it is the single API that reaches Android,
Linux and, through MoltenVK, the Apple platforms — [the Android
port](docs/port/android.md) is where that is argued, and it is the only item
on that document's spine that runs on hardware this repository already has. It
did test something the other four could not: Vulkan is the first API behind
this seam where the *presentation engine* says the window changed, rather than
Win32, and `renderer.h`'s resize contract turned out to have the right shape
for it. A sixth is not planned. Metal would be a build target rather than a
backend if MoltenVK holds, which is the one unmeasured claim that port rests
on.
Also permanently out of scope: online play, 3D, an editor, and a scripting
layer.
