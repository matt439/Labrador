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
and configuring fails without it. Ninja generator, out-of-source in
`out/build/<preset>/`. Twelve ctest entries: `MattMathTests`, `CoreTests`,
`CollisionTests`, `SceneTests`, `RenderTests`, `RenderPixelTests`,
`InputTests`, `UiTests`, `AssetsTests`, `AppTests`, `LineSweeperTests`
(doctest) and `Benchmarks`. `RenderPixelTests` is the only one that creates a
Direct3D device — a hidden window, a WARP fallback in debug, and assertions on
the pixels `Renderer::read_back_buffer` hands back. It is the only test of
anything this engine draws, and the executable statement of the pixel contract
a second backend has to reproduce. The
samples land at `out/build/x64-debug/samples/minimal/ArtAttackSample.exe` and
`out/build/x64-debug/samples/linesweeper/LineSweeperSample.exe`.

`LineSweeperTests` links no engine at all — the sample's rules are a static
library that links only `artattack_settings` — so the whole falling-block game
runs there with no window and no device. A rule is asserted rather than played.

## What will fail the build

- **`/W4 /WX /permissive- /sdl /fp:precise`**, with **zero suppressions**. One
  `INTERFACE` target, `artattack_settings` in [cmake/settings.cmake](cmake/settings.cmake),
  carries it; every real target links it. `/fp:precise` is load-bearing, not
  inherited — exact `operator==` against `Vector2F::ZERO`, tolerance
  assumptions and NaN propagation all depend on it.
- **An engine file including a game header.** Standalone that is now the
  compiler's own error, but [cmake/check_engine_includes.cmake](cmake/check_engine_includes.cmake)
  greps for it on every build anyway, because the compiler only enforces it
  when this repository is built standalone.
- **A file outside `engine/render/<backend>/` including that backend's
  `backend.h`.** Second pass in the same script, and it reads headers as well
  as `.cpp` files — a header is how the backend escaped last time.
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
  A backend is three translation units — `renderer.cpp`,
  `render_resources.cpp`, `resource_factory.cpp` — and all three live in
  `render/<backend>/`. Nothing outside that folder includes the backend
  header, the shell included: it hands its window handle to `create_device`
  as a `void*`. `check_engine_includes.cmake` fails the build for a file that
  reaches across, headers included.
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
  updated as findings are fixed. Both rounds were written while the
  paint-shooter was still in this tree, so any finding citing `game/...` refers
  to code that now lives in ColourWars — do not go looking for those files
  here, and do not treat their line numbers as current. `docs/review/rtcd/` is
  a candidate list mined from Ericson's *Real-Time Collision Detection*, not a
  plan; the decisions live in `docs/review/round-2/PLAN.md`.

## Known-absent, on purpose

A second render backend (the seam is cut, nothing is behind it — though the
pixel contract it must reproduce is now pinned by `RenderPixelTests`), a null backend
for headless render tests, and an action-mapping layer over the input devices —
neither client has a rebinding screen, so a binding table would be the
speculative framework T1 rules out. Also permanently out of scope: online play,
3D, an editor, and a scripting layer.
