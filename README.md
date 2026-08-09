# ArtAttack

A 2D game engine in C++, and a split-screen paint-shooter built on it to prove
it works.

The engine is the point. The game exists because an engine with no client is a
library of guesses — it is the thing that keeps saying "this API is awkward"
loudly enough to be worth fixing.

```
engine/    ~17k lines   the engine: ten modules with a fixed dependency direction
game/      ~13k lines   the paint-shooter — first client, and on its way to its own repository
samples/   ~330 lines   the minimal sample: the answer to "how do I start a project on this"
tests/     197 cases    doctest, eight targets, run by ctest
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

**Windows only.** The renderer's one backend is Direct3D 11, and the audio
backend is XAudio2. The seam for a second backend exists
(`engine/render/renderer.h`) and nothing has been written behind it.

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
cmake --preset x64-debug          # or x64-release
cmake --build --preset x64-debug
ctest --preset x64-debug
```

The presets put the build in `out/build/<preset>/`. The game runs from its own
output directory, which the build fills with `game/content/` — so
`out/build/x64-debug/game/ArtAttackGame.exe` runs from anywhere.

If `cmake` is not on your `PATH`, the copy inside Visual Studio is at
`<VS install>\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\`, and
`cl.exe` needs a `vcvars64.bat` shell.

## Audio

**A fresh clone has no sound, and that is expected.** The game runs; it is
silent, and it says so once on stderr.

The wave bank is built from source `.wav` files that are not in this
repository. Two reasons: four of the twenty-four waves are commercial music
tracks nobody can redistribute, and the sources are 148 MB against a built bank
of 154 MB, past GitHub's per-file limit. See [NOTICE](NOTICE).

What *is* here is the recipe:

- `game/content/sounds/sound_bank_1.json` — the bank's definition, and the
  authoritative list of the waves it expects.
- `game/content/sounds/sound_bank_1/XWBTool.exe` — Microsoft's wave-bank
  compiler.
- `cmake/build_wave_bank.cmake` — run from the build, it compiles the bank
  whenever every source wave named by the definition is present, and does
  nothing when they are not.

To get audio, put a `.wav` for each name in `sound_bank_1.json`'s `waves` array
into `game/content/sounds/sound_bank_1/` and build. The bank is compiled for
you.

Absent the bank, `game/content/manifest.json` marks it `"optional": true`, the
loader substitutes a silent sound bank, and every play is a no-op. Nothing else
in the game changes.

## The engine

Ten modules, each depending only on modules above it in the table
([ARCHITECTURE](docs/design/ARCHITECTURE.md) has the full version):

| Module | What it is |
|---|---|
| `math` | Vectors, shapes, intersection. Depends on nothing and links nothing. |
| `core` | Game objects, handles, registries, the state stack, the thread pool. |
| `collision` | Layers and masks, a broad phase, a narrow phase, analytic resolution. |
| `render` | The renderer seam, cameras, viewports, sprites, text. D3D11 lives in `render/d3d11/`. |
| `scene` | One object list, one collision sweep, one per-view render fan-out. |
| `input` | Gamepads, deadzones, press edges. XInput lives in `input/xinput/`. |
| `audio` | Sound banks and effect instances. |
| `ui` | Widgets, focus, directional navigation. |
| `assets` | The manifest, and checked JSON. |
| `app` | The window, the frame loop, and the services a game is handed. |

`engine/` may not include from `game/`, and the build fails if it does —
`cmake/check_engine_includes.cmake` runs on every build. That grep is a
stand-in for a compiler error, and becomes one when the game moves to its own
repository.

## Tests and benchmarks

```
ctest --preset x64-debug
```

197 test cases and about 4,000 assertions across eight targets, plus the
benchmark. Tests are doctest; there is no test for `game/`, because it is an
executable with no target to link against and it is on its way out of this
repository.

The benchmark (`bench/`) reports throughput and asserts on **complexity class**
rather than on wall-clock — a phase that is linear in the object count must
stay linear when the count quadruples, whatever the machine. An absolute
threshold would either fail on a slow box or pass on a fast one after a real
regression. Run `ArtAttackBench` directly to see the table.

## Status

The engine is usable and the game runs. Two rounds of full source review have
been carried out and worked through; `docs/review/` has both, including what
each was wrong about.

Not done, and known: a second render backend (the seam is cut, nothing is
behind it); a null backend for headless render tests; an action-mapping layer
over the input devices; and the repository split that moves `game/` out and
consumes the engine as a submodule.

## Licence

MIT — see [LICENSE](LICENSE). [NOTICE](NOTICE) records the third-party code
carried here (Microsoft's `DeviceResources` and `StepTimer`, Christer Ericson's
collision routines, rapidjson) and the content that is deliberately not
distributed.
