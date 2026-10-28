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
makes the count wrong. Then it runs them: the rules test, and the start-up
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

The job then makes the release folder of *Ship a Windows release*
(`website/src/content/docs/docs/guides/windows-release.mdx`), from the same
project, and checks it: every file
`website/src/walkthrough/release-files.txt` lists and no other, and the game
started from there with `--smoke-test`. To run that as it does, in the same
prompt:

```bat
cmake --preset x64-release
cmake --build --preset x64-release
cmake --install out\build\x64-release --prefix C:\dev\mygame-release
python <engine>\tools\consumer_check\check_release.py C:\dev\mygame-release
start /wait /b "" C:\dev\mygame-release\MyGame.exe --smoke-test 2> startup.txt
echo %errorlevel%
```

`check_release.py` names each file that is missing or not in the list, and
the smoke test must exit 0; if it does not, `startup.txt` holds the reason
the game wrote to standard error, which reaches the file only through
`start`'s `/b`. Install into a folder that does not exist yet, or
an empty one: the install never deletes a file.

**On a clean machine.** `sandbox.py` writes a Windows Sandbox configuration
for a release folder, for the check the runner cannot do, because it has
Visual Studio and the Visual C++ redistributable installed:

```bat
python <engine>\tools\consumer_check\sandbox.py C:\dev\mygame-release C:\dev\mygame-sandbox
C:\dev\mygame-sandbox\clean-machine.wsb
```

Opening the `.wsb` starts a sandbox with the release folder mapped read-only
and the results folder writable. At logon it runs `run.cmd`, which starts the
game with `--smoke-test` and writes `exit-code.txt`, `stderr.txt` and
`machine.txt` (the Windows version, whether the three runtime DLLs are
installed in the sandbox's `System32`, and its display adapters) to the
results folder, then shuts the sandbox down. If it stays open, the screen
shows why: a loader message about a missing DLL stops the script there. The
sandbox's default virtual GPU presents the host's own adapter through a
paravirtual driver, so a release build draws on it; with `--vgpu Disable`
only Windows' software adapter remains, which a release build refuses with
`No Direct3D hardware device found`. So a run proves that the folder loads
and its runtime is complete on a clean Windows; whether it draws on a player's
GPU and driver needs a real machine. `run.cmd` takes a release folder as its argument
to try it on the host first. Windows Sandbox needs Windows Pro or Enterprise,
and an administrator to enable the *Windows Sandbox* optional feature.

CI's `wasm-consumer` job assembles the same project and builds the page's browser
preset, from *Build for the browser*
(`website/src/content/docs/docs/guides/browser.mdx`). To run that as it does,
from a prompt where Emscripten's `emsdk_env.bat` has also been run, so that
`EMSDK` and `EMSDK_NODE` are set, and with the engine's own `wasm-release`
build present for the Chrome its configure found (or name any Chromium
yourself):

```bat
python tools\consumer_check\assemble.py C:\dev\mygame
cd /d C:\dev\mygame
cmake --preset wasm-release
cmake --build --preset wasm-release
ctest --preset wasm-release
"%EMSDK_NODE%" <engine>\tests\render\web\run_page.mjs --browser <chrome.exe> --page out\build\wasm-release\game\MyGame.html --quit-keys Escape,Enter --timeout 60
```

ctest runs `MyGameTests` alone, under Node, and the last command must end
`Module.onQuit was called` and exit 0.

`--engine <path>` consumes another engine tree than the checkout the script is
in. The folder must be new or empty; delete it to assemble again.
