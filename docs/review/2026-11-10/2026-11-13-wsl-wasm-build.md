# The wasm presets on WSL 2 Ubuntu, 2026-11-13

Run on 2026-11-13 for `REV#47`, by an agent on the owner's desktop. It is the
first build of a `wasm-*` preset on a non-Windows host: both presets
configured, built and passed their Node ctest selection. It is one run on
WSL 2, not a native Linux machine, not macOS, not a GitHub runner and not a
browser. Whether a CI job holds it stays the owner's call, since a new job
changes the required-check list.

## Host and versions

| Item | Version |
|---|---|
| Host | WSL 2 Ubuntu 26.04 LTS, kernel 6.6.87.2-microsoft-standard-WSL2, 32 threads, 47 GB, on Windows 11 |
| Source | `git clone /mnt/c/Projects/Labrador ~/labrador-wsl-check` at `1dc8533c`, in the Linux file system |
| Emscripten | emsdk at `708dcbcc` (2026-11-12), `./emsdk install 6.0.12` then `activate`: `emcc` 6.0.12 (`5488e087`), its own Node 24.19.0 |
| vcpkg | `microsoft/vcpkg` master `af2c387a` (2026-11-13), tool 2026-10-29; ports from the manifest's baseline `d92484ed` |
| Ports | doctest 2.5.3 and lodepng 2021-12-04 for `wasm32-emscripten`; vcpkg-cmake 2024-04-23 and vcpkg-cmake-config 2026-07-21 for the host |
| CMake, Ninja | Ubuntu's 4.2.3 and 1.13.2, already installed; vcpkg also fetched its own CMake 4.4.4 tarball |
| Compared with | Windows: Visual Studio's CMake 4.3.1-msvc1, its vcpkg tool 2026-05-27, Node 24.14.0, the same port versions |

## Steps

```text
git clone https://github.com/emscripten-core/emsdk.git ~/emsdk
cd ~/emsdk && ./emsdk install 6.0.12 && ./emsdk activate 6.0.12   # 27 s
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg               # 22 s
~/vcpkg/bootstrap-vcpkg.sh -disableMetrics
export VCPKG_ROOT=$HOME/vcpkg; source ~/emsdk/emsdk_env.sh
cd ~/labrador-wsl-check
cmake --preset wasm-release
cmake --build --preset wasm-release --parallel 8
ctest --preset wasm-release -j 4
```

`wasm-debug` ran the same three commands after it.

## What failed first, and the fix

The project needed no change. The two failures were vcpkg's host prerequisites
on a stock WSL Ubuntu, where `sudo` asked for a password the agent did not
have:

1. `bootstrap-vcpkg.sh` stopped with `Could not find zip`; `unzip` was also
   missing.
2. The first configure stopped inside vcpkg's doctest port:
   `vcpkg_fixup_pkgconfig` answered `Could not find pkg-config`. CMake then
   also printed `unable to find a build program corresponding to "Ninja"`,
   which was fallout of the failed toolchain step: Ninja was on the path, and
   the next configure found it.

Without root, `apt-get download zip unzip pkgconf pkgconf-bin libpkgconf7`
and `dpkg -x` into `~/.local/zipunzip`, with its `usr/bin` on `PATH` and its
library folder on `LD_LIBRARY_PATH`, supplied them. With root, vcpkg's own
message names the usual line, `sudo apt-get install curl zip unzip tar` and
`pkg-config`. Git, CMake, Ninja, curl and a host compiler were already present.

Every configure printed Emscripten's warning that CMake 4.2.3 `does not
support emscripten shared libraries`. Its range, 4.2.0 to 4.3.3, also covers
Visual Studio's 4.3.1 on Windows. Labrador builds no shared library.

## Results

| Preset | Configure | Build | ctest |
|---|---|---|---|
| `wasm-release` | 82 s, vcpkg install 34 s of it | 312 steps, 83 s, no warning in the log | 19 of 19 passed, 14 s |
| `wasm-debug` | 45 s, dependencies from vcpkg's binary cache | 107 s, no warning in the log | 19 of 19 passed, 14 s |

Timings are one run each, with other agents' work on the same machine, so they
are not a performance figure.

Configure found no Chromium and no Firefox and said so. The 19 registered
entries are the 18 `every_configuration` names in `tests/ctest_inventory.json`
plus `BrowserProtocolTests`, the count AGENTS.md gives without a browser. Every
test command ran emsdk's Linux Node. In `wasm-release` the doctest executables
reported 977 cases, all passing, with the same per-executable counts as the
Windows-built `wasm-release` tree in the main checkout (MinimalTests 3,
CollisionSampleTests 9, AudioSampleTests 4, ComponentsTests 52, AppTests 35,
AssetsTests 90, AudioTests 40, CollisionTests 60, CoreTests 114, InputTests
112, LineSweeperTests 25, LineSweeperViewTests 45, LocalMultiplayerTests 21,
LabradorMathTests 114, RenderTests 187, SceneTests 17, UiTests 49).
`BrowserProtocolTests` passed 120 of 120 `node --test` cases, and Benchmarks
printed `All benchmarks kept their complexity class.`

Both builds also packaged the LineSweeper web distribution. The release
`linesweeper.wasm` is 487,687 bytes against 487,799 from Windows, and the
`LineSweeperSample.wasm` 493,770 against 493,882. The 112-byte difference was
not traced; source paths embedded in the binary are a plausible cause.

## What this does not show

- No page ran: the 15 Chromium entries and 8 Firefox entries were built and
  not registered, so `run_page.mjs`' and `run_firefox.mjs`' browser discovery
  on Linux, and drawing, are untested from this host.
- One WSL 2 distribution with a hand-supplied `zip`, `unzip` and `pkg-config`
  is not a clean native Linux machine or `ubuntu-latest`, and macOS is
  unverified.
- No guide for a non-cmd shell was written; the browser guide's steps are
  still Windows `cmd`.
