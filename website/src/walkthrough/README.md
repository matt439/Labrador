# The Get Started project files

The files a reader creates in *Start your own project*
(`src/content/docs/docs/get-started/your-project.mdx`), kept as files rather
than typed into the page so that the page shows exactly what was built.

| File | Goes to |
| --- | --- |
| `root-CMakeLists.txt` | `CMakeLists.txt` |
| `game-CMakeLists.txt` | `game/CMakeLists.txt` |
| `CMakePresets.json` | `CMakePresets.json` |
| `gitignore.txt` | `.gitignore` |
| `ci.yml` | `.github/workflows/ci.yml`, from *Check your game in CI* (`src/content/docs/docs/guides/ci.mdx`) |

The `vcpkg.json` is not here because the page derives it from Labrador's own
(`src/components/ConsumerManifest.astro`), and the include edits are not
listed here because the page reads them from the sample
(`src/components/IncludeChanges.astro`): every line in the copied `.h` and
`.cpp` files that starts `#include "samples/minimal/`, `main_web.cpp`'s
included although the Windows build does not compile it.

**What checks them.** CI's `consumer` job assembles the project from these
files on every pull request and every push CI runs on, with
`tools/consumer_check/assemble.py`: Labrador at `external/labrador` from that
run's own checkout, `samples/minimal` copied to `game/`, every include the
page lists edited, these files put in place and the derived `vcpkg.json`
written. It then builds the `x64-debug` preset, checks that the project's
ctest lists exactly the game's two tests, `MyGameTests` and
`MyGameSmokeTests`, and so none of the engine's, and runs them. It does not
use Git or clone the public repository, and nothing runs `ci.yml`: its steps
are the job's own, which is what the guide says.

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

Change one, and the `consumer` job builds it on the pull request; follow the
page by hand again when a change touches a step the job does not do.
