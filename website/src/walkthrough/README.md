# The Get Started project files

The files a reader creates in *Start your own project*
(`src/content/docs/docs/get-started/your-project.mdx`), kept as files rather
than typed into the page so that the page shows exactly what was built. The
starter ZIP each release carries, which *Get the starter project*
(`src/content/docs/docs/get-started/starter.mdx`) builds, holds the same
files in the same places, made by `tools/consumer_check/starter_zip.py`. *Build
for the browser* (`src/content/docs/docs/guides/browser.mdx`) quotes the same
files: the `wasm` presets in `CMakePresets.json` and the `if(EMSCRIPTEN)`
block in `game-CMakeLists.txt`.

| File | Goes to |
| --- | --- |
| `root-CMakeLists.txt` | `CMakeLists.txt` |
| `game-CMakeLists.txt` | `game/CMakeLists.txt` |
| `CMakePresets.json` | `CMakePresets.json` |
| `gitignore.txt` | `.gitignore` |
| `ci.yml` | `.github/workflows/ci.yml`, from *Check your game in CI* (`src/content/docs/docs/guides/ci.mdx`) |

`release-files.txt` goes nowhere: it is every file `cmake --install` puts in the
project's release folder, which *Ship a Windows release*
(`src/content/docs/docs/guides/windows-release.mdx`) shows as a table and
`tools/consumer_check/check_release.py` holds an installed folder to.

The `vcpkg.json` is not here because the page derives it from Labrador's own
(`src/components/ConsumerManifest.astro`), and the include edits are not
listed here because the page reads them from the sample
(`src/components/IncludeChanges.astro`): every line in the copied `.h` and
`.cpp` files that starts `#include "samples/minimal/`, `main_web.cpp`'s
included, which only the `wasm` presets compile.

**What checks them.** CI's `consumer` job makes a starter ZIP of the project
from these files on every pull request and every push CI builds, with
`tools/consumer_check/starter_zip.py`, which takes the project from
`assemble.py`'s rules: Labrador at `external/labrador` from that run's own
checkout, `samples/minimal` copied to `game/`, every include the page lists
edited, these files put in place and the derived `vcpkg.json` written. It
extracts the ZIP with Windows' `tar` to `C:\dev\mygame`, keeps it as the run's
`labrador-starter` artifact, and then builds the `x64-debug` preset, checks that the project's
ctest lists exactly the game's two tests, `MyGameTests` and
`MyGameSmokeTests`, and so none of the engine's, and runs them. Then it builds
the `x64-release` preset, installs it into an empty folder, checks that the
folder holds exactly what `release-files.txt` lists, and starts the
installed game with `--smoke-test` from there. It does not
use Git or clone the public repository, and does not run `ci.yml`: its steps
are the job's own, which is what the guide says. `ci.yml` itself ran
on 26 October 2026, with the steps it has now, in
[matt439/labrador-mygame](https://github.com/matt439/labrador-mygame), the
walkthrough project at `v0.2.0`, twice and green, the second run restoring the
first's vcpkg cache; *Check your game in CI* links both runs. CI's `wasm-consumer` job extracts a ZIP of the
same project and builds its `wasm-release` preset with Emscripten, runs
`MyGameTests` under Node, and loads `MyGame.html` in headless Chrome with
`tests/render/web/run_page.mjs`, which presses Escape and Enter and passes when
the page reports that the game quit with nothing in the console.

**What was checked by hand.** On 4 October 2026, in the run commit `68b392d`
records, the whole page was followed in the x64 Native Tools Command Prompt:
Labrador added as a submodule from the public repository, the minimal sample
copied, its includes edited (four then; `main_web.cpp` came later), these
files put in place, and the project built, and rebuilt after the first change.
Step 2's checkout of a release tag came later than that run and was not part
of it, and the `consumer` job does not run it either.

**The first build from a cold cache.** On 25 October 2026 Get Started's steps
2 to 5 were followed on a fast desktop with Visual Studio 2026 Community
(version 18) and Git already installed, from a fresh clone of
`https://github.com/matt439/Labrador.git` at `6be26c9` into `C:\fte\Labrador`,
with `VCPKG_DEFAULT_BINARY_CACHE`, `VCPKG_DOWNLOADS` and
`X_VCPKG_REGISTRIES_CACHE` each pointed at an empty folder. Nothing failed.

| Step | Wall time |
| --- | --- |
| `cmake --preset x64-debug` | 2 min 17 s, of which the vcpkg install of 7 packages was 1.2 min: directxtk 43 s, directxmath 9.8 s, lodepng 7.1 s, xaudio2redist 6.6 s, doctest 3.6 s |
| `cmake --build --preset x64-debug` | 20.5 s, 293 steps |
| `ctest --preset x64-debug` | 24.4 s, 18 of 18 passed |
| The project of *Start your own project*, at the `v0.1.0` tag: configure | 12.5 s, its 5 packages restored from the cache in 109 ms |
| The same: build | 9.2 s |
| The same: rebuild after *Make a first change* | 5.8 s |

vcpkg also downloaded a CMake 4.3.2 of its own (51 MB), finding Visual
Studio's 4.3.1 not suitable, and a PowerShell 7.6.1 (114 MB), and fetched
137 MB of registry; about 330 MB was downloaded in all. Labrador's `out\`
folder was 1.3 GB afterwards. From the clone to the project's own window took
about six minutes. How long Visual Studio and Git take to install was not
measured. The template has had its tests since, so a project made from the
page now restores 6 packages rather than 5, doctest being the sixth, and
Labrador's ctest has 20 entries.

**The browser build, by hand.** On 25 October 2026 *Build for the browser*
was followed against the project assembled from the tree at `76e939e`
with that page's change: Emscripten 6.0.12 installed by the page's `emsdk`
commands into an empty `C:\fte\emsdk`, on a machine that already had
Python 3.14 on its `PATH`, after which `wasm-debug` configured
with it; then, with the machine's usual `emsdk`, both `wasm` presets built,
`MyGameTests` passed under Node, and `MyGame.html` was loaded in headless
Chrome, drew the greeting and quit on Escape and Enter with no console
message. The page's server command was checked to send `.wasm` as
`application/wasm`. With `EMSDK` unset, configure stopped with the error the
page quotes, and with `LABRADOR_RENDER_BACKEND=d3d11` or `vulkan` it stopped
with the engine's refusal. The page's first step, installing Python on a
machine or account without it, was not followed.

**The release folder, by hand.** On 25 October 2026 *Ship a Windows release*
was followed against the project assembled from the tree at `846ccb8` with
that page's change, on the desktop above: `x64-release` configured, built
and installed into an empty `C:\fte\mg8-release`, which held the 20 files
`release-files.txt` lists and no others; `MyGame.exe --smoke-test` exited 0
from there, and the running game had loaded `DirectXTK.dll`,
`XAudio2_9Redist.dll`, `MSVCP140.dll`, `VCRUNTIME140.dll` and
`VCRUNTIME140_1.dll` from that folder rather than from Windows, which also
has the runtime installed. The same project configured with
`LABRADOR_RENDER_BACKEND=gl` installed the same files, only the program
differing, and that program, copied beside the first as `MyGame-gl.exe`,
also passed its smoke test.

**On a clean Windows.** On 26 October 2026 the project assembled from the
tree at `2fdb322` was built and installed the same way, and the folder held
the same 20 files. The configuration `tools/consumer_check/sandbox.py` wrote
ran it in Windows Sandbox, Windows 10.0.26100 with none of the three runtime
DLLs in its `System32` and a virtual GPU in front of the desktop's: the
smoke test exited 0 with nothing on standard error.

**The starter ZIP, without Git.** On 30 October 2026 a ZIP that
`starter_zip.py` made from the tree with that change was 8.8 MB,
1,102 files, three quarters of the compressed size being the engine's `docs/`
and `website/` folders, which a submodule checkout at a tag holds too. Made
twice, it was the same bytes. Windows' `tar` and its shell ZIP handling,
which **Extract All** uses, each extracted it to one `mygame` folder. In the
x64 Native Tools Command Prompt of Visual Studio 2026, with Git for Windows
taken off `PATH` and `ProgramFiles` pointed at an empty folder, since vcpkg
also looks for `Program Files\Git\cmd\git.exe` directly, and with vcpkg's
download, binary and registry caches empty, the extracted project configured
in 2 minutes 8 seconds: vcpkg said `A suitable version of git was not
found`, downloaded PortableGit 2.54.0 (59 MB) and used it to fetch its
registry, and about 400 MB was downloaded in all. The build took 7 seconds
and both tests passed. In Windows Sandbox, where Git was never installed,
Visual Studio's own vcpkg, mapped in, downloaded the same PortableGit and
fetched the registry, then stopped only for want of Visual Studio. The
commands of *Get the starter project*'s second project, *Move a starter
project to a newer Labrador* and *Put your project in Git* (with `v0.2.0`)
were run as those pages give them, between two projects from the same ZIP.

Change one, and the `consumer` job builds it on the pull request; follow the
page by hand again when a change touches a step the job does not do.
