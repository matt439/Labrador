# Labrador

A 2D game engine in C++.

The engine is the point, but an engine with no client is a library of guesses.
Labrador has one: ColourWars, a split-screen paint-shooter that consumes this
repository as a submodule and is the thing that keeps saying "this API is
awkward" loudly enough to be worth fixing. It used to live in this tree. It
does not any more, and that is the point of the arrangement rather than an
accident of it — see [The wall](#the-wall).

*(ColourWars is a private repository, so its link is omitted rather than left
to 404. Nothing here depends on it: this repository builds, tests and
benchmarks standalone.)*

Two samples come with it, and they answer different questions.
`samples/minimal` is about 380 lines and is the answer to "how do I start a
project on this" — copy it. `samples/linesweeper` is a falling-block game and
is the answer to "what does a finished game look like on this engine" — read
it, and read [its README](samples/linesweeper/README.md) for the decisions
behind it.

```
engine/    ~17k lines   the engine: ten modules with a fixed dependency direction
samples/   ~2.8k lines  two clients: minimal, the template you copy; linesweeper, the game you read
tests/     361 cases    doctest, eleven targets, run by ctest
bench/                  throughput, run by ctest alongside them
docs/                   the design documents, and the reviews that argued with them
```

- **[docs/design/PHILOSOPHY.md](docs/design/PHILOSOPHY.md)** — what this engine
  is and what it refuses to be. Start here if you want to know why anything is
  the way it is.
- **[docs/design/ARCHITECTURE.md](docs/design/ARCHITECTURE.md)** — the module
  table, the dependency rules, and how the build enforces them.
- **[docs/design/CONVENTIONS.md](docs/design/CONVENTIONS.md)** — naming, file
  layout, and what a comment is for.

## Building

**Windows only.** The renderer has three backends, selected by
`LABRADOR_RENDER_BACKEND` at configure time: Direct3D 11, OpenGL 3.3 core, and
a null one with no graphics API that records what it was asked to draw. The
first two are held to the same pixel tests and both still run on Windows,
through the same Win32 window — the second exists to prove the seam carries a
backend, not to reach a new platform. The third is what makes drawing
assertable on a machine with no driver. The audio backend is XAudio2 and has no
second.

### Prerequisites

- **Visual Studio 2022 or newer**, with the "Desktop development with C++"
  workload. Ninja and CMake come with it.
- **The Windows 10/11 SDK**, for `d3d11.h` and friends.
- **vcpkg**, with `VCPKG_ROOT` set. `CMakePresets.json` reads it for the
  toolchain file, and configuring fails without it. Visual Studio ships a copy
  at `<VS install>\VC\vcpkg` if you have no other.

Everything else — DirectX Tool Kit, doctest — is in `vcpkg.json` and is fetched
at configure time. rapidjson is vendored in `external/`.

### Configure, build, test

```
cmake --preset x64-debug          # or x64-release, or x64-debug-gl
cmake --build --preset x64-debug
ctest --preset x64-debug
```

`x64-debug-gl` builds against the OpenGL backend and `x64-debug-null` against
one with no graphics API at all. They are separate configurations rather than a
runtime switch because the backend is chosen at compile time
(`LABRADOR_RENDER_BACKEND`), so asking for one that was not built is a missing
symbol at link rather than a failure on the first frame.

The presets put the build in `out/build/<preset>/`. The sample lands at
`out/build/x64-debug/samples/minimal/MinimalSample.exe` and runs from
anywhere — the build mirrors its content beside it.

If `cmake` is not on your `PATH`, the copy inside Visual Studio is at
`<VS install>\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\`, and
`cl.exe` needs a `vcvars64.bat` shell.

## Using Labrador in a project

Add it as a submodule and `add_subdirectory` it before the target that links
it:

```cmake
add_subdirectory(external/labrador)   # defines LabradorEngine and labrador_settings

target_link_libraries(YourGame PRIVATE LabradorEngine labrador_settings)
target_include_directories(YourGame PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
```

Three things worth knowing:

- **You get the library and nothing else.** The tests, the benchmark and the
  sample are behind `PROJECT_IS_TOP_LEVEL`, so consuming Labrador does not put
  ten doctest targets and a sample executable into your `ctest`.
- **`labrador_settings` comes from here, and only from here.** It is the
  `/W4 /WX /permissive- /fp:precise` interface target every real target links
  (ARCHITECTURE, The build). Do not define your own copy — CMake will fail on
  the duplicate target name, and the fix is not a guard: two copies across two
  repositories is precisely the second place that target exists to prevent.
- **You need your own include root.** Includes are written from the repository
  root (CONVENTIONS), so your own `#include "yourgame/thing.h"` needs the
  directory above `yourgame/` on the include path. Labrador publishes its root,
  not yours.

`samples/minimal` is a working example of all three, at about 380 lines.
`samples/linesweeper` is the same three plus a fourth thing worth copying: a
game's simulation as a static library that links nothing, so the whole game is
playable from `ctest` with no window and no device.

## The engine

Ten modules, each depending only on modules above it in the table
([ARCHITECTURE](docs/design/ARCHITECTURE.md) has the full version):

| Module | What it is |
|---|---|
| `math` | Vectors, shapes, intersection, 2D affine transforms. Depends on nothing and links nothing. |
| `core` | Game objects, handles, registries, the state stack, the thread pool. |
| `collision` | Layers and masks, a broad phase, a narrow phase, analytic resolution. |
| `render` | The renderer seam, cameras, viewports, sprites, text. D3D11 lives in `render/d3d11/`. |
| `scene` | One object list, one collision sweep, one per-view render fan-out. |
| `input` | Gamepads, deadzones, press edges. XInput lives in `input/xinput/`. |
| `audio` | Sound banks and effect instances. |
| `ui` | Widgets, focus, directional navigation. |
| `assets` | The manifest, and checked JSON. |
| `app` | The window, the frame loop, and the services a game is handed. |

### The wall

`engine/` may not include from a client's tree, and the build fails if it does.
Standalone, that is now the compiler's own error — there is no `game/` beside
`engine/` to resolve, and an engine file reaching for one gets

```
fatal error C1083: Cannot open include file: 'game/objects/level.h'
```

`cmake/check_engine_includes.cmake` runs on every build anyway, because the
compiler only enforces it standalone. Build Labrador as a subdirectory of a
project that *does* have a `game/` and the grep is the only thing left holding
the line.

## Tests and benchmarks

```
ctest --preset x64-debug
```

331 test cases and about 17,000 assertions across nine targets, plus the
benchmark.

The benchmark (`bench/`) reports throughput and asserts on **complexity class**
rather than on wall-clock — a phase that is linear in the object count must
stay linear when the count quadruples, whatever the machine. An absolute
threshold would either fail on a slow box or pass on a fast one after a real
regression. Run `LabradorBench` directly to see the table.

## Status

The engine is usable and its client runs. Two rounds of full source review have
been carried out and worked through; `docs/review/` has both, including what
each was wrong about. Those reviews were written while the paint-shooter was
still in this tree, so findings filed against `game/` refer to code that now
lives in ColourWars.

Since the split, one targeted audit: `docs/review/backend-equivalence/` holds
the three render backends against the one contract in `engine/render/renderer.h`
and asks what `RenderPixelTests` structurally cannot, since it only ever runs
against the backend the preset configured. The invariant holds — the three
agree on every term the pixel contract names — with one live exception on the
GL preset and a list of comments the code has outgrown.

Not done, and known: an action-mapping layer over the input devices. That is
the whole list — the second render backend and the null one both landed.

## Licence

MIT — see [LICENSE](LICENSE). [NOTICE](NOTICE) records the third-party code
carried here: Microsoft's `DeviceResources` and `StepTimer`, Christer Ericson's
collision routines, and rapidjson.
