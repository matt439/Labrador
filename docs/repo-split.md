# The repository split — a verified procedure

> **Done.** Executed 2026-08-09. This repository is the engine side; the
> paint-shooter is `matt439/ColourWars` and consumes this one as a submodule at
> `external/labrador`. The document is kept as the record of how it was done.
>
> Four things differed from the plan below, all of them found while running it:
>
> - **The names.** `Labrador` for the engine and `ColourWars` for the game, not
>   the `artattack-engine` / `ArtAttack` this document assumes. Read the paths
>   accordingly.
> - **§6 is wrong about CI.** `.github/workflows/ci.yml` could not go to the
>   engine "unchanged" — its last step launches `ArtAttackGame.exe`, which does
>   not exist here. The step moved to ColourWars; the benchmark step stayed.
> - **§2 is wrong about NOTICE.** Its "content that is deliberately not
>   distributed" sections describe `game/content/`, so they belonged with the
>   game. Each repository now has a NOTICE covering what it actually ships.
> - **History does not reach 2022-12-30 on this side.** §5 claims it does.
>   `engine/` did not exist as a directory until the restructure, so
>   `--path engine` starts at 2023-12-19; the older history is under `game/`
>   paths and went to ColourWars, which does reach back to the beginning.
>
> §4's two breakages both happened exactly as described.

The last item on `docs/review/round-2/PLAN.md`'s §6, and the only one that
changes anything outside this working tree. It is written out rather than
performed because it creates repositories and rewrites this one: the commands
below are yours to run.

**Everything here has been rehearsed.** Both repositories were laid out in a
scratch tree, configured, built and tested — the engine side runs 9/9 under
`ctest`, and the game side produces a working `ArtAttackGame.exe` against the
engine as a submodule. The two things that broke during that rehearsal are in
§4, because they will break for you too if you skip the edits in §3.

**Contents**

1. [What the split is for](#1-what-the-split-is-for)
2. [The partition](#2-the-partition)
3. [The five edits](#3-the-five-edits)
4. [What the rehearsal found](#4-what-the-rehearsal-found)
5. [The commands](#5-the-commands)
6. [Afterwards](#6-afterwards)

---

## 1. What the split is for

`docs/design/ARCHITECTURE.md` has always said an engine file including a game
header "fails to compile, and that is the feature (T5)". It has never been
true. Both `engine/` and `game/` sit under one root, and includes are written
from the repository root (CONVENTIONS), so the engine's own include root *must*
be the directory that also holds `game/`. No include path admits
`"engine/render/renderer.h"` and refuses `"game/objects/level.h"` from there.

E1 did the prerequisite: the include roots are `CMAKE_CURRENT_SOURCE_DIR`-relative,
so the engine configures and builds under a foreign top-level project. That
turned the split from a build rewrite into a move.

**The rehearsal confirms the payoff is real.** With `game/` in its own
repository, an engine file that includes a game header fails like this:

```
engine\scene\scene.cpp(2): fatal error C1083: Cannot open include file:
'game/objects/level.h': No such file or directory
```

`cmake/check_engine_includes.cmake` stays anyway — see §4.

## 2. The partition

Two repositories. Names below are what the rest of this document assumes;
change them and change the paths with them.

### `artattack-engine` — the engine, its sample, its tests, its benchmarks

```
engine/                 119 files
samples/                  8
tests/                   37
bench/                    5
external/rapidjson/           vendored, with its own licence
cmake/settings.cmake          compiler strictness, one place
cmake/check_engine_includes.cmake
docs/design/                  PHILOSOPHY, ARCHITECTURE, CONVENTIONS
docs/review/                  both rounds — see the note below
CMakeLists.txt                new, §3.1
CMakePresets.json             unchanged
vcpkg.json                    unchanged (directxtk, doctest)
LICENSE  NOTICE  README.md
.gitattributes  .gitignore
.github/workflows/ci.yml
```

### `ArtAttack` — the paint-shooter

```
game/                   258 files
external/rapidjson/           its own copy: it parses its own save file
cmake/seed_save.cmake
cmake/build_wave_bank.cmake
CMakeLists.txt                new, §3.2
CMakePresets.json             unchanged
vcpkg.json                    unchanged
LICENSE  README.md            its own
.gitattributes  .gitignore
external/artattack-engine/    the submodule
```

**`docs/review/` is a judgement call and this document does not make it for
you.** Both reviews are overwhelmingly about the engine, and PLAN.md is the
document this procedure is the last item of — which argues for the engine
repository. Against: the reviews also contain every finding filed against
`game/`, and a game repository with no record of why its code looks like this
loses something. Splitting the reviews themselves is the one option worth
refusing; they are one argument each.

**`cmake/settings.cmake` goes to the engine only.** Not both. See §4.1.

## 3. The five edits

### 3.1 The engine's new root `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.26)

project(ArtAttackEngineProject LANGUAGES CXX)

include(cmake/settings.cmake)

add_custom_target(check_engine_includes ALL
    COMMAND "${CMAKE_COMMAND}"
        "-DENGINE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/engine"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_engine_includes.cmake"
    COMMENT "Checking that no engine file includes a game header"
    VERBATIM
)

add_subdirectory(engine)

# A client consuming this as a submodule wants the library and nothing else:
# its ctest should not run the engine's tests, and it should certainly not
# build the engine's sample.
if(PROJECT_IS_TOP_LEVEL)
    enable_testing()

    add_subdirectory(samples/minimal)
    add_subdirectory(bench)
    add_subdirectory(tests/assets)
    add_subdirectory(tests/collision)
    add_subdirectory(tests/core)
    add_subdirectory(tests/input)
    add_subdirectory(tests/math)
    add_subdirectory(tests/render)
    add_subdirectory(tests/scene)
    add_subdirectory(tests/ui)
endif()
```

`PROJECT_IS_TOP_LEVEL` is the whole of what makes the engine consumable. Without
it, adding the engine as a subdirectory drags in doctest, ten test targets and
a sample executable, and puts them in the client's `ctest`.

### 3.2 The game's new root `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.26)

project(ArtAttack LANGUAGES CXX RC)

# First: it defines artattack_settings, which game/ links.
add_subdirectory(external/artattack-engine)

add_subdirectory(game)
```

### 3.3 `game/CMakeLists.txt` gains its own include root

```cmake
target_include_directories(ArtAttackGame PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
)
```

Without it, every `#include "game/pch.h"` in the game fails. See §4.2.

### 3.4 The game does not get a `cmake/settings.cmake`

Delete it from the game side, and do not `include()` it from the game's root.
See §4.1.

### 3.5 `.gitmodules`, in the game repository

```
[submodule "external/artattack-engine"]
    path = external/artattack-engine
    url = https://github.com/matt439/artattack-engine.git
```

## 4. What the rehearsal found

Two things broke. Both are in §3; this is why.

### 4.1 `artattack_settings` gets defined twice

If the game keeps its own `cmake/settings.cmake` and includes it, and the
engine submodule includes its copy, CMake fails:

```
add_library cannot create target "artattack_settings" because another
target with the same name already exists.
```

The fix is not a guard. ARCHITECTURE says compiler strictness lives in exactly
one place (T5), and two copies across two repositories is precisely the second
place that target exists to prevent — one of them would drift, and the
difference would be a warning level. The engine defines it; the game links it.

### 4.2 The game's own include root disappears

`game/main.cpp` opens with `#include "game/pch.h"`, which resolved because the
engine published the old repository root and that root held `game/`. After the
split the engine publishes its own root, which does not:

```
game\main.cpp(1): fatal error C1083: Cannot open include file:
'game/pch.h': No such file or directory
```

This is not a regression — it is §1's whole point arriving, and it lands on the
game because the game is the thing that moved. One line (§3.3) gives the game
back its own root, and the engine's still refuses `game/`.

### 4.3 Keep `check_engine_includes` anyway

It is tempting to delete the grep now that the compiler enforces it. Don't. The
compiler only enforces it when the engine is built *standalone*; a client
building the engine as a subdirectory does so from a tree that has a `game/` in
it, and that client's own include roots are not the engine's business. The grep
holds there. Belt, braces, and it costs one build step.

## 5. The commands

`git filter-repo` preserves history for each side. It is not bundled with git:
`pip install git-filter-repo`. The alternative — a fresh repository with one
commit — throws away a history that runs back to 2022-12-30 across three
repository names, and is not recommended.

Run from a scratch directory, not from your working tree.

```bash
# --- the engine -----------------------------------------------------------
git clone https://github.com/matt439/ArtAttack.git artattack-engine
cd artattack-engine
git filter-repo \
    --path engine --path samples --path tests --path bench \
    --path external/rapidjson \
    --path cmake/settings.cmake --path cmake/check_engine_includes.cmake \
    --path docs --path .github \
    --path CMakePresets.json --path vcpkg.json \
    --path LICENSE --path NOTICE --path README.md \
    --path .gitattributes --path .gitignore

# apply §3.1, then:
git add -A && git commit -m "Stand alone: the engine, its sample, its tests"
cd ..

# --- the game -------------------------------------------------------------
git clone https://github.com/matt439/ArtAttack.git ArtAttack-game
cd ArtAttack-game
git filter-repo \
    --path game \
    --path external/rapidjson \
    --path cmake/seed_save.cmake --path cmake/build_wave_bank.cmake \
    --path CMakePresets.json --path vcpkg.json \
    --path .gitattributes --path .gitignore

# apply §3.2, §3.3, §3.4, then:
git submodule add https://github.com/matt439/artattack-engine.git \
    external/artattack-engine
git add -A && git commit -m "Consume the engine as a submodule"
```

Then, before pushing anything:

```bash
# the engine, standalone
cd artattack-engine
cmake --preset x64-debug && cmake --build --preset x64-debug \
    && ctest --preset x64-debug        # expect 9/9

# the game, against the submodule
cd ../ArtAttack-game
cmake --preset x64-debug && cmake --build --preset x64-debug
```

Both were run in the rehearsal and both pass.

## 6. Afterwards

- **`docs/design/ARCHITECTURE.md`** — the paragraph that currently explains why
  the grep stands in for a compiler error becomes the paragraph explaining that
  it is now both. E1's note in PLAN.md §3 (E1, item 4) says the same thing from
  the other side.
- **`README.md`** on each side. The engine's loses the "the game" framing; the
  game's is new and should say which engine revision it pins.
- **`docs/design/PHILOSOPHY.md`** T11's escape clause was answered in E3 with
  "`game/` is a client, not shipped code — the repo split is pending and
  `game/` leaves this repository when it happens." When it has happened, that
  sentence stops being a promise.
- **CI on both sides.** `.github/workflows/ci.yml` goes with the engine
  unchanged. The game needs its own, with `submodules: recursive` on the
  checkout step.
- **The old repository.** Archive it rather than delete it: every issue, link
  and clone that points at it should keep resolving.
