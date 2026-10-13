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
written. It then builds the `x64-debug` preset and checks that the engine's
tests stay out of it: `ctest` lists none and the build tree holds no
`CTestTestfile.cmake`. It does not run the game, use Git, or clone the public
repository.

**What was checked by hand.** On 4 October 2026, in the run commit `68b392d`
records, the whole page was followed in the x64 Native Tools Command Prompt:
Labrador added as a submodule from the public repository, the minimal sample
copied, its includes edited (four then; `main_web.cpp` came later), these
files put in place, and the project built, and rebuilt after the first change.

Change one, and the `consumer` job builds it on the pull request; follow the
page by hand again when a change touches a step the job does not do.
