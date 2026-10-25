# The consumer check

`assemble.py` builds, in an empty folder, the project that Get Started's
*Start your own project* page
(`website/src/content/docs/docs/get-started/your-project.mdx`) tells a reader
to make, from one engine tree:

- the engine at `external/labrador`: every file `git ls-files` lists in that
  tree, tracked or new, as it is on disk, so uncommitted work is included and
  `out/` is not;
- `samples/minimal` copied to `game/`;
- every include the page lists edited: each line starting
  `#include "samples/minimal/` in the copied `.h` and `.cpp` files, by the rule
  `website/src/components/IncludeChanges.astro` uses, `main_web.cpp`'s included
  although the Windows build does not compile it;
- the four files in `website/src/walkthrough/` put where its README says;
- `vcpkg.json` derived from the engine's own by the rule
  `website/src/components/ConsumerManifest.astro` uses, named `mygame`.

It does not run `git init` or `git submodule add`, so the engine is a copy
rather than a submodule, and it builds nothing. CI's `consumer` job
(`.github/workflows/ci.yml`) runs it on every pull request and every push CI
runs on, with the engine taken from that run's own checkout. It then does the
configure and build of the page's step 7, without running the game, which the
minimal sample cannot yet quit by itself (OCA-30), and checks that the
engine's tests stay out of the project: `ctest` lists none, and no
`CTestTestfile.cmake` is anywhere in the build tree outside
`vcpkg_installed`, because the consumer's own root never enables testing and
the count alone could not fail. `vcpkg_installed` is left out because it is
vcpkg's: a package vcpkg builds from source rather than restores from its
binary cache leaves its own build tree under `vcpkg_installed\vcpkg\blds`, and
DirectXMath's and DirectXTK's hold `CTestTestfile.cmake` files of their own.

To run it as CI does, from an x64 Native Tools Command Prompt with `VCPKG_ROOT`
set, into a folder whose path is short (CI uses `C:\dev\mygame`, the page's
own; the prerequisites page says why a long one fails):

```bat
python tools\consumer_check\assemble.py C:\dev\mygame
cd /d C:\dev\mygame
cmake --preset x64-debug
cmake --build --preset x64-debug
ctest -N --test-dir out\build\x64-debug | findstr /B /C:"Total Tests: 0"
dir /s /b out\build\x64-debug\CTestTestfile.cmake 2>nul | findstr /V /I /C:"vcpkg_installed"
```

The last command must print nothing.

`--engine <path>` consumes another engine tree than the checkout the script is
in. The folder must be new or empty; delete it to assemble again.
