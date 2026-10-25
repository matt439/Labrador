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
- the files in `website/src/walkthrough/` put where its README says;
- `vcpkg.json` derived from the engine's own by the rule
  `website/src/components/ConsumerManifest.astro` uses, named `mygame`.

It does not run `git init` or `git submodule add`, so the engine is a copy
rather than a submodule, and it builds nothing. It also puts the example
workflow the *Check your game in CI* guide gives at `.github/workflows/ci.yml`,
where nothing runs it. CI's `consumer` job (`.github/workflows/ci.yml`) runs
the script on every pull request and every push CI runs on, with the engine
taken from that run's own checkout. It then does the configure and build of
the page's step 7 and checks that the project's tests are the game's and not
the engine's: the project's root enables testing, so every `add_test` in the
tree is in the list `ctest -N` prints, and that list must be exactly
`MyGameTests` and `MyGameSmokeTests`. An engine test, or an engine sample's,
makes the count wrong. Last, it runs them: the rules test, and the start-up
check, which opens the game hidden, draws a few frames and quits, and needs a
graphics device.

To run it as CI does, from an x64 Native Tools Command Prompt with `VCPKG_ROOT`
set, into a folder whose path is short (CI uses `C:\dev\mygame`, the page's
own; the prerequisites page says why a long one fails):

```bat
python tools\consumer_check\assemble.py C:\dev\mygame
cd /d C:\dev\mygame
cmake --preset x64-debug
cmake --build --preset x64-debug
ctest -N --test-dir out\build\x64-debug
ctest --preset x64-debug
```

The listing must end `Total Tests: 2` and name `MyGameTests` and
`MyGameSmokeTests`, and the last command must pass both.

`--engine <path>` consumes another engine tree than the checkout the script is
in. The folder must be new or empty; delete it to assemble again.
