# The consumer check

The optional [Release-symbols procedure](../../website/src/content/docs/docs/guides/release-symbols.mdx)
is separate from the ordinary starter. Its copied recipe preserves native
MSVC Release optimization, runtime and IPO choices; compiler-free fixtures
are `test_release_symbols_recipe.py`. The standalone
[symbol probe](symbol_probe/README.md) verifies the actual local PDB/image,
preserved source checksum, a real one-shot breakpoint and own-child smoke
exit. `retain_symbols.py` captures the explicitly selected pair and working
source bytes privately, excluding generated/VCS directories without changing
line endings. It records hashes, not a claim that arbitrary selected PDBs
match; run the probe separately. Its fixtures are `test_retain_symbols.py`.
The [dated consumer qualification](../../docs/performance/2026-11-17-release-symbols.md)
records the default/full-IPO proofs and installed-folder check. No probe or
private capture is added to the normal installed game.

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
where nothing runs it.

The assembled project also gets the walkthrough's optional `labrador.vsconfig`
beside its presets. The manual project route needs no such file: it selects
the same Visual Studio prerequisites by hand. Assembly and CMake never run an
installer or change vcpkg's tool selection. The configuration matches the
five-entry selection and bootstrapper command qualified on Visual Studio
Community 2026 18.12.0 in the
[isolated installation record](../../docs/review/2026-11-10-first-run/2026-11-13-visual-studio-install.md).
`test_consumer_check_starter.py` checks that selection, the command and the
assembled/ZIP bytes. The website's configuration download reads the same
canonical file; after a site build, compare `website/dist/labrador.vsconfig`
with `website/src/walkthrough/labrador.vsconfig` before claiming payload
verification. A later installer is not qualified by the older install record.

`starter_zip.py` writes the same project, engine included, to a ZIP: the
starter ZIP each release carries as `labrador-starter-vX.Y.Z.zip`, which
*Get the starter project*
(`website/src/content/docs/docs/get-started/starter.mdx`) has a reader
download, extract and build without Git. Every entry is under one top folder,
`mygame/`, so extracting it makes one folder; beside the project it holds
`README.txt`, which names the Labrador version and commit and points at Get
Started. Entries are sorted, carry the time of the tree's last commit and
nothing else of the machine, so the same tree, checked out with the same line
endings, gives the same bytes: entries are read from the working tree, so a
CRLF checkout and an LF one differ
([the `v0.3.0` rebuild](../../docs/website-versioning.md#rebuild-comparison-2026-11-13)
matched the release's ZIP from a CRLF checkout). With
`--release vX.Y.Z` it is named for that release and refuses a tree that is not
exactly the tag: `vcpkg.json`'s `version-string` must match, `HEAD` must be
the tag's commit, and nothing may be modified or untracked. Without it, it is
`labrador-starter-dev.zip` and its README says it is not a release.
The release ZIP also stamps `cmake/release_identity.txt` with its verified
commit and exact tag, so `version()` and CMake identify the release after
extraction without Git. Its source revision remains `unknown`; the stamp is
package identity, not a check of later manual edits. Plain unstamped source
copies report a development version.
CONTRIBUTING.md's *Cutting a release* attaches one to each release.

The consumer owns its top-level manifest; adding the engine as a subdirectory
does not install the engine's manifest. Selected package failures name the
missing CMake package, its port/feature obligation and the engine manifest to
compare, while retaining CMake's package reason, considered configurations
and versions, and directory/search hints. A declared-but-missing
port points to toolchain/triplet/discovery repair rather than a duplicate
entry. A different working baseline is accepted. Optional materials need host
tools only when a material is selected. The shared starter CMake file uses
`labrador_find_package` for doctest when the selected engine provides it,
and ordinary `find_package(doctest CONFIG REQUIRED)` otherwise. This keeps
the development walkthrough's file usable with the released engine it pins.

On 2026-11-17 that same file configured successfully in both a real Git-route
project with the engine submodule at `v0.3.0` (`0f4f5033`) and a development
assembly copied from `f21917f5`. CMake traces selected ordinary discovery in
the former and the helper in the latter; the copied game CMake files had
identical bytes. These are configure/generate checks, not new build or device
results. The offline dependency fixtures execute the shared lookup with the
helper both present and absent and require the imported doctest target.

CI's `consumer` job (`.github/workflows/ci.yml`) makes that ZIP on every pull
request and every push CI builds, from that run's own checkout, extracts it
to `C:\dev\mygame` with Windows' own `tar`, and keeps it as the run's
`labrador-starter` artifact. It then does the configure and build of the
page's step 7 in the project as it came out of the ZIP and checks that the
project's tests are the game's and not the engine's: the project's root
enables testing, so every `add_test` in the tree is in the list `ctest -N`
prints, and that list must be exactly `MyGameTests` and `MyGameSmokeTests`.
An engine test, or an engine sample's, makes the count wrong. Then it runs
them: the rules test, and the start-up check, which opens the game hidden,
draws a few frames and quits, and needs a graphics device.

To run it as CI does, from an x64 Native Tools Command Prompt with `VCPKG_ROOT`
set, into a folder whose path is short (CI uses `C:\dev\mygame`, the page's
own; the prerequisites page says why a long one fails):

```bat
python tools\consumer_check\starter_zip.py C:\dev
cd /d C:\dev
tar -xf labrador-starter-dev.zip
cd /d C:\dev\mygame
cmake --preset x64-debug
cmake --build --preset x64-debug
ctest -N --test-dir out\build\x64-debug
ctest --preset x64-debug
```

`python tools\consumer_check\assemble.py C:\dev\mygame` makes the same folder
without the ZIP or its `README.txt`.

Both assembly routes place the maintained `website/src/walkthrough/game-README.md`
at `game/README.md` and adapt standalone comments through `game-comments.json`.
The root ZIP instructions distinguish `game/` from the engine's look-alike
`external/labrador/samples/minimal/`. The latter keeps the full original
source, README and change list; assembly changes no engine files. Offline
fixtures check comments, code-string preservation, line endings and ZIP
contents, and compare the website's source-anchor model with real assembly.

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
fails unless the installed game's embedded manifest declares PerMonitorV2
first (`tools/check_dpi_manifest.py`, which reads the file and runs
nothing), and
the smoke test must exit 0; if it does not, `startup.txt` holds the reason
the game wrote to standard error, which reaches the file only through
`start`'s `/b`. Install into a folder that does not exist yet, or
an empty one: the install never deletes a file.

**Under another name.** After the release checks, `rename_check.py` makes
the project of *Rename your game*
(`website/src/content/docs/docs/guides/your-name.mdx`). It reads three things
out of the guide itself, so that the line a reader pastes and the line checked
are the same text: the name in the `set "NAME=..."` line, step 2's PowerShell
replacement line, and the count the guide says that line prints. It copies the
starter without its `out` folder, runs the line through `cmd` in the copy with
`NAME` set, and fails unless the line prints the guide's count, prints
`Replaced MyGame 0 times.` when run again, leaves `MyGame` in neither
`CMakeLists.txt` nor in `CMakePresets.json` or the workflow, puts the new name
where the old one was as many times as the count, and leaves the name in
`vcpkg.json` in lower case. Then it does the guide's step 5 on `x64-debug`,
without opening the game: configure, build, and `ctest`, failing unless the
tests listed are the renamed game's two and no other, `game\<name>.exe` is
there, `CMakeCache.txt` has the new project name, and no file or folder in
the build folder is named `MyGame`. To run it as CI does, in the same prompt,
after the first block above:

```bat
python <engine>\tools\consumer_check\rename_check.py C:\dev\hopper --from C:\dev\mygame
```

Without `--from` it assembles its own starter from the engine tree, as
`assemble.py` does, so this one line is the whole check from nothing:

```bat
python tools\consumer_check\rename_check.py C:\dev\hopper
```

It must end `renamed to SkyHopper by the guide's own line, and x64-debug built
and passed SkyHopperTests and SkyHopperSmokeTests.` With `--no-build` it stops
after the line and the check of the files, which takes a few seconds and
needs no compiler. The folder must be new or empty, and as short as
`C:\dev\mygame`. It does not make the guide's edits by hand, the window's
title, the error box and the page's title, and it renames neither the release
folder nor the browser build; the guide's *What was checked* records those
runs. The offline tests (`tools/tests/test_consumer_check_rename.py`, in CI's
`lint` job) run the guide's line on the walkthrough's build files on Windows
and hold the guide's count to them, so a guide or walkthrough change that
breaks the rename fails before anything is built. A guide whose step 2 no
longer has exactly one such line, name or count is refused, naming the script
to change with it.

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

CI's `wasm-consumer` job makes and extracts the same starter ZIP and builds the
page's browser preset, from *Build for the browser*
(`website/src/content/docs/docs/guides/browser.mdx`). To run that as it does,
from a prompt where Emscripten's `emsdk_env.bat` has also been run, so that
`EMSDK` and `EMSDK_NODE` are set, and with the engine's own `wasm-release`
build present for the Chrome its configure found (or name any Chromium
yourself):

```bat
python tools\consumer_check\starter_zip.py C:\dev
cd /d C:\dev
tar -xf labrador-starter-dev.zip
cd /d C:\dev\mygame
cmake --preset wasm-release
cmake --build --preset wasm-release
ctest --preset wasm-release
"%EMSDK_NODE%" <engine>\tests\render\web\run_page.mjs --browser <chrome.exe> --page out\build\wasm-release\game\MyGame.html --quit-keys Escape,Enter --timeout 60
```

ctest runs `MyGameTests` alone, under Node, and the last command must end
`Module.onQuit was called` and exit 0.

**The optional browser CI recipe.**
`website/src/walkthrough/browser-ci.yml` is copied only when a game opts into
the guide `website/src/content/docs/docs/guides/browser-ci.mdx`; it is absent
from `assemble.py`'s `PLACED` list and leaves the native workflow alone.
Its packaging step invokes the consumer's full embedded
`external/labrador/tools/web_bundle/bundle.mjs`, with the runtime files,
licences and SDK-version input named in that step. It writes
`out/web-release/index.html`, `MyGame.js`, `MyGame.wasm`, `NOTICE.txt` and
`BUILD.json`. Run the page command above with
`--page out/web-release/index.html` to qualify that packaged copy; the starter
still quits with `Escape,Enter`. Keep the full embedded engine because the
page runner imports sibling modules and the website server helper.

`python -m unittest tools.tests.test_consumer_browser_ci` runs the offline
workflow and real-command packaging fixtures, including missing-input
failures. On 2026-11-16 the assembled consumer's Release build and Node test
passed locally with Emscripten 6.0.12, and its packaged page quit in Chrome
157.0.8103.78 on ANGLE/D3D11/RTX 5080. Its Git identity was unknown because
the assembled copy was an archive, not a repository. These local results do
not qualify this new workflow's hosted SDK/cache or artifact-upload steps.
No deploy step or publication credential is supplied. The bundler's
`--verify` mode belongs to LineSweeper and does not accept this package.

**With a game-authored shader.** After those baseline checks, both consumer
jobs run `material_fixture.py` on the assembled copy. It writes a game-owned
HLSL pixel function, calls `labrador_add_material` from the project's root
after `game/` exists, and registers and selects its `MaterialSource` after
`app.initialize()` in both entry points. The function samples the scene with
a parameter set by the game. The native smoke test and browser page then draw
through it. Pixel semantics are asserted separately by `RenderPixelTests`;
these consumer runs check the generated header, paths, build tools and
runtime handover in a project outside the engine's own build.

Each job then runs the fixture's `--check-invalid-shader <build-directory>`
mode. It replaces the copied shader with malformed HLSL, requires the build
to fail with that shader's file, line and column in a compiler diagnostic,
then restores the original bytes and rebuilds the material, including when
the diagnostic check fails. A missing compiler alone cannot pass this check.

The fixture changes the assembled copy, so use it after the release-folder
and rename checks. It refuses a changed starter or a second application
before writing. To run the native pass from that project's folder:

```bat
python <engine>\tools\consumer_check\material_fixture.py .
cmake --preset x64-debug
cmake --build --preset x64-debug
ctest --preset x64-debug
python <engine>\tools\consumer_check\material_fixture.py . --check-invalid-shader out\build\x64-debug
```

For a separate browser starter, run the script with `--host-tools`, then the
browser configure, build, ctest and page commands above again. That option
adds the consumed engine's optional `materials` host dependencies to this
fixture's manifest: dxc with SPIR-V support and spirv-cross. The default
starter's manifest and build remain unchanged. An existing tool installation
can instead be named by the shader guide's CMake cache overrides.

`--engine <path>` consumes another engine tree than the checkout either script
is in. `assemble.py`'s folder must be new or empty; delete it to assemble
again. `tar` overwrites the files of a `C:\dev\mygame` that is there already
but deletes none, so delete the folder before extracting a ZIP again.

**Scripted pad input.** After those baseline checks (and, natively, the rename/release checks), the
existing `consumer` and `wasm-consumer` jobs opt their assembled copy into
`scripted_pad_fixture.py`. Its one `ScriptedPadConsumerTests` target links the
same configured LabradorEngine as the game and constructs a normal reader
beside a scripted source. It drives real Gamepads snapshots, edges,
suspension, vibration records and teardown with no physical pad output.
The source lives in `scripted_pad.cpp`; nothing is added to the distributed
starter's own two tests or to the repository's ctest inventory.

To add this optional fixture after the baseline checks:

```bat
python <engine>\tools\consumer_check\scripted_pad_fixture.py C:\dev\mygame
cd /d C:\dev\mygame
cmake --preset x64-debug
cmake --build --preset x64-debug --target ScriptedPadConsumerTests
ctest --preset x64-debug -R "^ScriptedPadConsumerTests$" --no-tests=error --output-on-failure
```

Use the browser preset and Emscripten environment for the same test under
Node. This fixture qualifies the consumer construction boundary; it does
not qualify physical mapping or rumble.

## Optional native command helpers

The assembled project also carries `build.cmd`, `test.cmd`, `play.cmd` and
their shared `tools/starter_commands.ps1`. From an ordinary Command Prompt
whose current folder is the project, `build.cmd` configures/builds,
`test.cmd` configures/builds/tests, and `play.cmd` configures/builds/plays.
`-Preset x64-release` chooses Release; `play.cmd -SmokeTest` starts the hidden
startup check. The manual commands above remain the underlying route.

The existing consumer CI job exercises all three helpers before changing the
project for its extra fixtures. `tools/tests/test_starter_helpers.py` uses
isolated native executable stand-ins to pin Visual Studio discovery,
non-default tool paths, project paths with spaces and shell characters,
environment activation, both output streams, renamed-game selection and
each failure's actual exit code. It installs nothing. Real ordinary-prompt
and Sandbox evidence is recorded with OCA-97 in BACKLOG.md.

## Compiled guide pilot

`guide_check.py` applies the numbered main procedures in **Add a rule and a
test**, **Add a screen to your game**, and **Add a collision object** to a copy
of an extracted development starter. From the same developer prompt as the
ordinary consumer check, with an existing extracted starter at `C:\dev\mygame`:

```bat
python tools\consumer_check\guide_check.py C:\dev\guide-pilot --from C:\dev\mygame --check-broken
```

The destination must be new or empty. The input starter is read only. The
copy uses `x64-debug` with null render/audio overrides: first the unchanged
starter's two tests, then each independent finished guide example. The rule
uses its guide's own tests. Screen tests press Enter and Escape against the
actual title state; collision tests drive the actual HelloState and inspect
its recorded wall stopping, sliding and contact tint. Each example also runs
the game's smoke test. The native consumer job runs this after its baseline,
release and rename checks, before its other fixtures, in the same required
job. Those checks and the distributed starter remain separate.

The complete new files are read from the guides' titled C++ blocks. Build
list replacements, insertions and entry-point edits use their numbered
procedure's code blocks and SourceFile excerpts; missing or ambiguous
selectors fail with the guide's name. Optional changes after the procedure
are excluded. `--check-broken` misspells the generated bounds rule's rectangle
constructor, requires a compiler failure naming that source and typo,
restores its exact bytes and rebuilds and retests it. An unbuilt source or a
missing tool cannot pass that proof. Offline tooling tests cover recipe
refusals, leaked engine tests, command failure and source restoration.

`out/guide-pilot/BUILD.json` in the copy records each configure/build/inventory/
test command, exit status and elapsed seconds, input guide and tool hashes,
generated source hashes and the number of checked selectors. Its sibling
folders preserve the finished changed sources and full command logs. CI
keeps that folder as the `compiled-guide-pilot` artifact, including failures.
The copy's source bytes are restored on exit; its build cache still describes
the last example, so configure and build before using it again.

Local installed packages may be reused explicitly with repeatable
`--configure-arg=-Dname=value` arguments. CI uses the normal vcpkg manifest
and its existing binary cache. The initial pilot has no browser build,
visual or physical-input acceptance, does not qualify the released v0.3.0
starter, and does not check every intermediate or optional edit. Its measured
cost and expansion boundary are in the
[2026-11-17 record](../../docs/review/2026-11-12-work-audit/2026-11-17-guide-pilot.md).
