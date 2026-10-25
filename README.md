# Labrador

A 2D game engine in C++20, for Windows and a desktop browser through WebGL2.
It is a library your game links, built from source with CMake and vcpkg: the
game is ordinary C++ and its content is JSON and standard file formats.

No editor, no scripting language, no hot reload, 2D only and local play
only: [why](https://labradorengine.com/why/), and
[what Labrador is not](https://labradorengine.com/#what-labrador-is-not).

**Website and documentation: [labradorengine.com](https://labradorengine.com/).**
New here? [Get Started](https://labradorengine.com/docs/get-started/) takes an
empty Windows machine to a game of your own that builds against Labrador, in
five steps.

The engine is the point, but an engine with no client is a library of guesses.
Labrador has one: ColourWars, a split-screen paint-shooter that consumes this
repository as a submodule and is the thing that keeps saying "this API is
awkward" loudly enough to be worth fixing. It used to live in this tree. It
does not any more, and that is the point of the arrangement rather than an
accident of it — see [The wall](#the-wall).

*(ColourWars is a private repository, so its link is omitted rather than left
to 404. Nothing here depends on it: this repository builds, tests and
benchmarks standalone.)*

Six samples come with it, and they answer different questions.
`samples/minimal` is about 760 lines, its test and start-up check included,
and is the answer to "how do I start a project on this" — copy it. `samples/linesweeper` is a falling-block game and
is the answer to "what does a finished game look like on this engine" — read
it, and read [its README](samples/linesweeper/README.md) for the decisions
behind it. [Play LineSweeper in a desktop browser](https://mattharding.dev/demo/linesweeper/)
before building anything: the hosted demo starts on a click and plays with a
keyboard. Phones are a planned phase, so a phone gets a poster and a recording
of the game to tap and watch, not the game itself, until a real iPhone and a
real Android phone have played it.

`samples/collision`, `samples/audio` and `samples/local_multiplayer` are
focused examples for the website's guides: a collision arena, a playable
sound bank and two players sharing a world through separate views. Each has
its own README with controls and build commands.

`samples/components` is the answer to "do I have to write a settings menu
from scratch": an option row and a slider, each a header, a source file and
its tests, which a game copies into its own tree rather than links, and a
gallery that shows them running. [Its README](samples/components/README.md)
says why they are copied, and when one would move into the engine.

```
engine/    ~43k lines   the engine: ten modules with a fixed dependency direction
samples/                the starter template, LineSweeper, three focused guide examples and components to copy
tests/     ~28k lines   doctest, run by ctest with five more that sit beside their samples
bench/                  throughput, run by ctest alongside them
docs/                   the design documents, and the reviews that argued with them
website/                the public site, built with npm from this same checkout
```

- **[docs/design/PHILOSOPHY.md](docs/design/PHILOSOPHY.md)** — what this engine
  is and what it refuses to be. Start here if you want to know why anything is
  the way it is.
- **[docs/design/ARCHITECTURE.md](docs/design/ARCHITECTURE.md)** — the module
  table, the dependency rules, and how the build enforces them.
- **[docs/design/CONVENTIONS.md](docs/design/CONVENTIONS.md)** — naming, file
  layout, and what a comment is for.

## Building

**Windows, and a desktop browser.** The renderer has five backends, chosen by
`LABRADOR_RENDER_BACKEND` at configure time: Direct3D 11 (the default),
Direct3D 12, OpenGL 3.3 core, Vulkan, and a null one with no graphics API that
records what it was asked to draw. The first four are held to the same pixel
tests and the same checked-in images of every frame they draw. Audio is a seam
of its own: XAudio2, Web Audio in a browser, or a null one that records what it
was asked to play. The `wasm-debug` and `wasm-release` presets build the engine
and the samples for a browser with Emscripten, drawing through WebGL2 with the
OpenGL backend. All six samples run there, LineSweeper has also been checked in
Firefox, and Safari has not been checked yet. Phones are a planned phase, not a
platform.

Why each backend exists is in [ARCHITECTURE](docs/design/ARCHITECTURE.md),
*Modules*; where the browser build stands is in
[docs/port/web.md](docs/port/web.md), and Vulkan's route to other platforms in
[docs/port/android.md](docs/port/android.md).

### Prerequisites

- **Visual Studio 2022 or newer**, with the "Desktop development with C++"
  workload. Ninja and CMake come with it.
- **The Windows 10/11 SDK**, for `d3d11.h` and friends, and for `fxc` — the
  two Direct3D backends compile their shader at build time and the build fails
  without it. The OpenGL one needs no tool.
- **The [Vulkan SDK](https://vulkan.lunarg.com/)**, with `VULKAN_SDK` set, and
  **only for the two Vulkan presets**. It does not come with Visual Studio or
  with Windows. That backend compiles the same
  `sprite.hlsl` to SPIR-V, and the `dxc` that can do it is the SDK's rather
  than the Windows SDK's — `cmake/compile_shaders.cmake` says how to tell them
  apart, because both are called `dxc.exe` and only one of them will work.
- **[Emscripten](https://emscripten.org/)**, with `EMSDK` set, and **only for
  the two `wasm-*` presets**: the compiler, the toolchain file vcpkg chains to,
  and the Node the tests run under. It does not come with Visual Studio either,
  and Visual Studio is still what puts CMake and Ninja on the path for it. The
  pixel tests, the window's input test and the samples' pages there also
  want Chrome, or another
  Chromium named by `LABRADOR_TEST_BROWSER`, and are left out of `ctest` where
  there is none.
- **vcpkg**, with `VCPKG_ROOT` set. `CMakePresets.json` reads it for the
  toolchain file, and configuring fails without it. Visual Studio ships a copy
  at `<VS install>\VC\vcpkg` if you have no other.

Everything else — DirectX Tool Kit, doctest, lodepng — is in `vcpkg.json` and is fetched
at configure time. rapidjson is vendored in `external/`.

### Configure, build, test

```
cmake --preset x64-debug          # or x64-release, x64-debug-d3d12,
                                  # x64-release-d3d12, and the gl/vulkan/null
                                  # debug/release pairs
cmake --build --preset x64-debug
ctest --preset x64-debug
```

Each backend has a Debug and a Release preset: the unqualified pair selects
Direct3D 11, and `-d3d12`, `-gl`, `-vulkan` and `-null` select the other four.
The `-null` pair takes no graphics API and no audio API, so it runs end to end
on a machine with neither. A backend is chosen at compile time, so asking for
one that was not built is a missing symbol at link.

The presets put the build in `out/build/<preset>/`. The sample lands at
`out/build/x64-debug/samples/minimal/MinimalSample.exe` and runs from
anywhere — the build mirrors its content beside it. Under a `wasm-*` preset
it is `out/build/wasm-debug/samples/minimal/MinimalSample.html`, with its
content packed into the module beside it; serve that folder over HTTP and open
the page. LineSweeper also builds as an ES module for another site to host,
and the build bundles it into `out/build/wasm-release/web-dist/linesweeper/`:
the module, an example page that starts it on a click, the licence notices
for what it links, and a `BUILD.json` naming the commit it came from. CI keeps
that folder from every run ([web.md](docs/port/web.md), section 7).

If `cmake` is not on your `PATH`, the copy inside Visual Studio is at
`<VS install>\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\`, and
`cl.exe` needs a `vcvars64.bat` shell.

## Using Labrador in a project

Add it as a submodule and `add_subdirectory` it before the target that links
it:

```cmake
add_subdirectory(external/labrador)   # defines LabradorEngine, MattMath and labrador_settings

target_link_libraries(YourGame PRIVATE LabradorEngine labrador_settings)
target_include_directories(YourGame PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
```

Three things worth knowing:

- **You get the library and nothing else.** The tests, the benchmark and the
  samples are behind `PROJECT_IS_TOP_LEVEL`, so consuming Labrador does not put
  this repository's test targets into your `ctest`, or its executables into
  your build.
- **`labrador_settings` comes from here, and only from here.** It is the
  `/W4 /WX /permissive- /fp:precise` interface target every real target links
  (ARCHITECTURE, The build). Do not define your own copy — CMake will fail on
  the duplicate target name, and the fix is not a guard: two copies across two
  repositories is precisely the second place that target exists to prevent.
- **You need your own include root.** Includes are written from the repository
  root (CONVENTIONS), so your own `#include "yourgame/thing.h"` needs the
  directory above `yourgame/` on the include path. Labrador publishes its root,
  not yours.

`add_subdirectory` alone does not configure. A consuming project also
needs:

- **A top-level `vcpkg.json` naming the engine's dependencies.** vcpkg reads
  the manifest of the top-level project only, so Labrador's own
  [vcpkg.json](vcpkg.json) is not read when Labrador is a subdirectory, and
  the engine's `find_package(xaudio2redist)`, under the default audio
  backend, fails without one. Copy its dependencies and its
  `builtin-baseline`, leaving out `lodepng`, which only the engine's own tests
  use. `doctest` stays if your game's tests use it, as the minimal sample's
  do.
- **A configure preset that chains the vcpkg toolchain**: `toolchainFile` set
  to `$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake`, as every configure
  preset in this repository's [CMakePresets.json](CMakePresets.json) has.
- **`fxc` on `PATH`.** The default backend, Direct3D 11, and Direct3D 12
  compile the engine's shader at build time, so your configure looks for `fxc`
  too. The x64 Native Tools Command Prompt puts it there, as it does `cl`. The
  Vulkan backend needs the Vulkan SDK instead, as [Building](#building) says.

The website's walkthrough,
[Start your own project](https://labradorengine.com/docs/get-started/your-project/),
builds a project of this shape from an empty folder: Labrador as a submodule,
`samples/minimal` copied in as the game, and every file the project needs.
Those files are in [website/src/walkthrough/](website/src/walkthrough/), whose
README says how they were checked. `samples/minimal` is the starting point it
copies, not a consumer: it is built inside this repository, so it uses this
repository's `vcpkg.json` and presets and has no include root of its own. It
is also the shape of a browser build: `main_web.cpp` beside `main.cpp`, chosen
in its `CMakeLists.txt` under `EMSCRIPTEN`, and a page in `web/shell.html`.
`samples/linesweeper` has that shape too, and adds one more thing worth
copying: a game's simulation as a static library that links nothing, so the
whole game is playable from `ctest` with no window and no device.

## The engine

Ten modules, each depending only on modules above it in the table
([ARCHITECTURE](docs/design/ARCHITECTURE.md) has the full version):

| Module | What it is |
|---|---|
| `math` | Vectors, shapes, intersection, 2D affine transforms. Depends on nothing and links nothing. |
| `core` | Game objects, handles, registries, the state stack, the thread pool. |
| `collision` | Layers and masks, a broad phase, a narrow phase, analytic resolution. |
| `render` | The renderer seam, cameras, viewports, sprites, text. Each graphics API lives in a folder of its own — `render/d3d11/` and the four beside it. |
| `scene` | One object list, one collision sweep, one per-view render fan-out. |
| `input` | Keyboard, mouse, touch and gamepads; deadzones, press edges, menu directions. XInput lives in `input/xinput/` and the browser's Gamepad API in `input/web/`; the keyboard, the mouse and touch are fed by the window. |
| `audio` | Sound banks and effect instances, behind `AudioDevice`. XAudio2 lives in `audio/xaudio2/` and Web Audio in `audio/webaudio/`; `audio/null/` records what it was asked to play. |
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

Twenty entries on the Windows presets: eighteen doctest targets, the minimal
sample's start-up check and the benchmark. The `-null` presets have nineteen,
because the pixel tests need a device, and a `wasm-*` build has twenty-eight
where Chrome is found, the extra ones being pages run in headless Chrome, one
for each sample among them, and no start-up check, because a page has no
command line. How
many cases ran is what `ctest` prints.

The benchmark (`bench/`) asserts on **complexity class** rather than on
wall-clock: a phase that is linear in the object count must stay linear when
the count quadruples, whatever the machine. Run `LabradorBench` directly to see
the table.

`LineSweeperFrameBench` measures frame times on real hardware and is not a
`ctest` entry. Its runs, and how to read them, are in
[docs/performance/](docs/performance/); read
[`2026-09-19-g6f-ratchet.md`](docs/performance/2026-09-19-g6f-ratchet.md)
before any of their tables.

## Status

The engine is usable, and ColourWars, its client, runs on it. Releases are 0.x
tags, and [Changes](#changes) says what one promises. Open work is indexed in
[TODO.md](TODO.md), and [docs/product/BACKLOG.md](docs/product/BACKLOG.md) holds
its order.

`docs/review/` keeps every review of the engine as it was written: two full
rounds from before the split, and targeted reviews since, of the render
backends against one contract and of the whole project. Their findings are
not updated as they are fixed. Beside them are `rtcd/`, a candidate list
rather than a review, and `colourwars-reports/`, the engine's decisions on
the client's reports. Three files are kept current, and say where they
stand:
[backend-equivalence-2/STATUS.md](docs/review/backend-equivalence-2/STATUS.md),
[2026-09-26/STATUS.md](docs/review/2026-09-26/STATUS.md) and the table in
[colourwars-reports/README.md](docs/review/colourwars-reports/README.md).
Each other folder's README says what it covers and when.

Out of scope on purpose: online play, 3D, an editor and a scripting layer, for
good ([PHILOSOPHY](docs/design/PHILOSOPHY.md), *Non-goals*), and an
action-mapping layer over the input devices, because which actions exist and
how they are bound is the game's policy rather than the engine's mechanism
(PHILOSOPHY, *The boundary*, and T1).

## Contributing

A change lands as one self-contained commit on a branch, through a pull
request that is rebase-merged once CI is green; [CONTRIBUTING.md](CONTRIBUTING.md)
is the whole procedure.

## Changes

Releases are tags, `v0.1.0` the first, and [CHANGES.md](CHANGES.md) lists,
newest first and under the tag that carries each, every change a game built on
the engine can see - source breaks, `vcpkg.json` and compile-option changes,
and silent behaviour changes - with the commit and the migration. Read it
before moving a pin from one tag to the next. Before 1.0 a tag promises that
record rather than compatibility; [PHILOSOPHY](docs/design/PHILOSOPHY.md),
*The public face*, says what 1.0 waits for, and
[CONTRIBUTING.md](CONTRIBUTING.md) how a release is cut.

## Licence

MIT — see [LICENSE](LICENSE). [NOTICE](NOTICE) records the third-party code
carried here: Microsoft's `DeviceResources` and `StepTimer`, Christer Ericson's
collision routines, and rapidjson. It also records the one piece of content
that MIT does not cover: the samples' font atlas, rendered from JetBrains Mono
and licensed under the SIL Open Font License 1.1, whose text travels beside
every copy of it as `OFL.txt`.
