# Running a verification sweep

The [November 17 integration checkpoint](verification/2026-11-17-overnight-checkpoint.md)
records the first completed overnight cohort, including source-specific native,
browser, Release, sanitizer and website results and retained initial failures.

The later [current PR qualification](verification/2026-11-17-pr-current.md)
records the complete seven-track sweep and Vulkan synchronization at
`fadd1a63`, with local lint/site receipts and explicit Release/ASan/consumer
and physical limits.

The separate opt-in [native-null AddressSanitizer qualification](verification-asan.md)
uses `x64-debug-null-asan`, its own dependency triplet and a full configured
suite with a deterministic detection probe. It is not an additional render
track selected by `verify_changes.py` or a replacement for real-driver checks.

`tools/verify_changes.py` prints the verification plan for the render, audio,
input, app, collision or scene area of the engine.
Every configure/build/test phase remains `not-run` unless `--execute` is explicit.
The executor stops at the first failed command or validation and names it.
The real sweep ran on 2026-10-29 ([qualification](#real-sweep-qualification-2026-10-29));
fake-command tests qualify the control flow. The per-change record of what
required PR CI does not fully qualify is [below](#release-and-real-driver-qualification).

These examples run from the repository root:

```text
python tools/verify_changes.py --area render
python tools/verify_changes.py --path engine/render/gl/renderer.cpp --format json
python tools/verify_changes.py --path engine\render\vulkan --preflight
python tools/verify_changes.py --area render --preflight --browser "C:/Program Files/Chromium/chrome.exe"
python tools/verify_changes.py --path engine/collision/contacts.cpp --path engine/scene/scene.h
python tools/verify_changes.py --area audio --area input
python tools/verify_changes.py --path engine/audio/xaudio2 --path engine/app/win32/window.cpp
```

From another directory, provide the absolute or relative path to this checkout's
script. It anchors the checkout to its own location.

Repeatable `--area` and repeatable `--path` selectors are mutually exclusive;
either repeated forms a union. The areas are `render`, `audio`, `input`, `app`,
`collision` and `scene`. A path selects the matching `engine/<area>` root,
including deleted paths. Windows
separators are normalized. Traversal, absolute paths and other folders fail
explicitly. Documentation, sample, website and other engine folders are not
modeled; follow their owners in AGENTS.md.
Selectors are lexical: selected files are not opened and symlinks are not
resolved or checked.

Default planning reads repository source rules without tooling-environment
inspection, tool discovery, build-cache reads or Git/network contact. Standard
Python/CLI locale handling is incidental; it does not inspect the toolchain.
The checkout comes from the script's location, independent of the working
directory. Environment macros stay symbolic in the plan. Text output prints
JSON argv arrays and their cwd/effects; these are unambiguous data, including
paths with spaces, rather than shell-ready command strings. JSON output has
`schema_version: 2`, a `scope` of the selected `areas` and `paths`, and stable ordering.

## The seven configurations

| Planned selection | Render / audio / input | Build type |
| --- | --- | --- |
| x64-debug | d3d11 / xaudio2 / xinput | Debug |
| x64-debug-d3d12 | d3d12 / xaudio2 / xinput | Debug |
| x64-debug-gl | gl / xaudio2 / xinput | Debug |
| x64-debug-vulkan | vulkan / xaudio2 / xinput | Debug |
| x64-debug-null | null / null / xinput | Debug |
| wasm-debug | gl / webaudio / web | Debug |
| wasm-release-null, CI override | null / null / web | Release |

Each area plans a subset of these tracks, in this order:

| Area | Tracks | Registrations it adds to each track | Manual obligations it adds |
| --- | --- | --- | --- |
| render | all seven | `RenderTests`, `SceneTests`, `AudioTests`; native rasterizing tracks add `RenderPixelTests`, `RenderPixelTestsThroughPost`, `MinimalSmokeTests` and `CollisionSmokeTests`, D3D12 adds `D3D12SynchronizationTests` | Vulkan synchronization validation, required for an `engine/render/vulkan` path |
| audio | x64-debug, x64-debug-null, wasm-debug, wasm-release-null | `AudioTests`, `AudioSampleTests`; native null adds `AudioSmokeTests` | the XAudio2 endpoint smoke and listening (`WEB#3`) |
| input | the same four | `InputTests` | real pads and touchscreens |
| app | all seven | `AppTests`; native tracks add `MinimalSmokeTests` and `CollisionSmokeTests`, native null adds `LocalMultiplayerSmokeTests` | Vulkan synchronization validation, conditional |
| collision | x64-debug, x64-debug-null, wasm-release-null | `CollisionTests`, `SceneTests`, `CollisionSampleTests`, `Benchmarks`; both native tracks add `CollisionSmokeTests` | none specific |
| scene | x64-debug, x64-debug-null, wasm-release-null | `SceneTests`, `CollisionSampleTests`, `Benchmarks`, `LocalMultiplayerTests`; both native tracks add `CollisionSmokeTests`, native null adds `LocalMultiplayerSmokeTests` | none specific |

Collision and scene share portable code. The D3D11 smoke drives the collision
sample's scripted route on a rasterizing device. Native null exercises the
sample's outline state and records the scene's per-view draws. Wasm null
compiles the recording, culling and fan-out cases and benchmarks under clang.
The multiplayer sample compares serial and parallel draws of the same players
across two views on both null tracks. The planner runs the full suite on each
selection.

For changes to the public contact callback, the [PI-25 integration
record](review/colourwars-reports/contact.md#integration-qualification-2026-11-20)
also covers the compiled collision guide and a reviewed Release capture.
Check edited guide steps with the [starter guide pilot](../tools/consumer_check/README.md#compiled-guide-pilot) and run `npm run gate`
in `website/` for affected published guides and header excerpts. These checks
run separately from the planner.

Audio and input have one backend on each side: XAudio2 or null and XInput
natively, Web Audio or null and web input in the browser. The default native
preset, native null (recorded waves, synthetic input with hidden windows) and
both browser tracks therefore cover them. The window and application loop
reach every render backend, so app keeps all seven. A union plans every track
any selected area needs, with every selected area's applicable registrations on each track. The browser
registrations below belong to the track, whichever area selects it. Each
required registration must be in `tests/ctest_inventory.json` for its track,
or planning fails and names the entry.

Named selections print preset configure/build/test argv, with build parallelism
2 and test parallelism 1. The null browser selection preserves CI's existing
override instead of adding another preset:

```text
cmake --preset wasm-release -B out/build/wasm-release-null -DLABRADOR_RENDER_BACKEND=null -DLABRADOR_AUDIO_BACKEND=null
cmake --build out/build/wasm-release-null --parallel 2
ctest --test-dir out/build/wasm-release-null --parallel 1 --output-on-failure --no-tests=error --stop-on-failure
```

Its build/test commands use the override directory, never the GL wasm-release
build/test preset. An explicit `--browser` adds a FILEPATH cache argument to
both planned wasm configure commands; planning neither writes that cache nor
launches the browser. It does not copy CI's GL/Vulkan pixel/smoke exclusions
into a full local sweep.

The restricted rule reader validates the current version-6 local preset shapes,
matching build/test links, single-parent inheritance (for a test preset,
only from hidden presets that hold nothing but its output), backend defaults,
SDK-only dxc lookup and CI override. It models only the exact native-base
condition `${hostSystemName}` equals `Windows`: all ten native Debug/Release
presets inherit it, and both wasm presets have no host condition. Booleans,
null, compound predicates and other condition shapes fail explicitly.
Default planning validates that condition without inspecting the host and
still prints every planned track; preflight continues to require Windows for the
whole sweep. Future includes, condition or inheritance shapes and changed
source rules can require updating the model. It is not a general CMake
interpreter and does not activate user toolchain wrappers.

## Optional read-only preflight

`--preflight` inspects whitelisted environment values, tool/file paths and
existing planned CMakeCache.txt files. It checks only what the planned tracks
need: an audio or input plan does not ask for `VULKAN_SDK`. It executes no version probes, CMake,
CTest, compilers, SDK activation scripts, Node, browser or other subprocess.
It creates, deletes and changes no cache, build tree, environment or global file.

The checks cover the modeled Windows toolchain environment, CMake/CTest/Ninja/cl,
Direct3D fxc, VCPKG_ROOT's toolchain, Vulkan headers/import library and its own
Bin/dxc.exe, and EMSDK's toolchain/compiler entry file. A PATH dxc is no substitute
for the Vulkan SDK compiler. Emscripten's selected Node must come from the existing
EMSDK_NODE value or configured emulator; an arbitrary host Node on PATH does not
establish that selection. Presence checks do not establish versions/capabilities.
A set `MSYSTEM` is a blocking mismatch for the planned wasm tracks: a cmd started
from Git Bash inherits it, `emsdk_env.bat` then prints bash exports instead of
setting the environment, and `emcc` is not on PATH. Run `set MSYSTEM=` before
`emsdk_env.bat`. Since `--execute` runs the same preflight first, it refuses too.

Existing cache checks compare source/build directories, generator, build type,
render/audio/input selections, toolchains and the explicit wasm triplet. A named
preset can preserve old cache overrides where it omits a backend value; x64-debug
does not prove that its existing cache is D3D11. Mismatches are reported without
resetting anything. A missing cache is unconfigured, not configured successfully.
Cached Direct3D FXC selections are checked for presence even when another fxc
exists on PATH; an intentionally selected valid cached compiler may differ.
Manifest-install state is a warning about possible downloads/reconfigure, never
permission to enable a fetch or claim a future build is network-free.

Chromium uses an explicit absolute `--browser` path or an existing configured
LABRADOR_TEST_BROWSER cache path. The tool does not duplicate CMake's program
search or mistake a same-named environment variable for a CMake selection. With
neither basis, page acceptance has a missing prerequisite; CMake might discover
one during a later authorized configure. A present file remains unlaunched and
does not establish protocol support or actual test registrations.
An existing selected wasm-null browser path is also checked, while no selected
null browser remains a conditional page obligation rather than a new requirement.

Only the selected environment/cache fields are reported. Unrelated cached
values and credentials are not included. `detected`, `missing`, `mismatch`,
`unsupported-host` and `not-checked` describe inspection; phase results remain
`not-run`. Non-Windows host setups are outside this Windows sweep preflight.

Planning/preflight exit codes:

- **0:** a valid plan was printed; when requested, no blocking static-preflight
  issue was found. No configure/build/test passed as a consequence.
- **1:** a complete plan was printed with missing/mismatched/unsupported static
  prerequisites. No command was attempted.
- **2:** selector, repository rule or preset input is invalid or unsupported.

## Reading the remaining obligations

Configure/build can write build trees, reconfigure and download dependencies;
wasm builds can use SDK caches. Native full CTest is mixed runtime: hidden
windows/processes, graphics devices/sample drawing on rasterizing backends,
and audio-backend initialization for XAudio2 selections. AudioSampleTests is
headless doctest coverage; endpoint smoke/listening remains separate.
D3D12SynchronizationTests is a device runtime suite. Null removes graphics/audio
APIs without removing every platform fixture or application loop.

Full wasm CTest executes generated modules and can run real DOM/sample pages.
Web Audio selections can initialize browser audio in those registered pages;
Node AudioTests and audio listening remain separate evidence.
Even wasm-null can register WebWindowTests and development sample pages when a
browser is found. It is not a Node-only or browser-free command. The plan names
required GL browser registrations, conditional null pages and forbidden null
pixel/context/bundle/iframe/pacing entries; the first two come from the inventory's
`chromium_registered` list. The executor inspects actual registrations,
rejects zero selected tests and absent required pages, then reports the selection.
`--no-tests=error` cannot detect a missing page while unrelated tests still exist.

The Release counterparts of the planned tracks, such as five native Release
configurations and wasm-release GL for render, remain separate unrun obligations. Vulkan source paths activate the synchronization-validation reminder
with validate_sync and shell-visible layer output. Generic render or app selection
shows that requirement conditionally. Audio selections name the XAudio2 endpoint
smoke and listening; input selections name real pads and touchscreens. Website/lints,
consumer packaging, opt-in probe builds and capture review keep their existing
owners and remain unrun.

Source/recording tests do not establish rasterization, listening, physical pads,
touchscreens, Safari, real-phone memory/layout or display pacing. File detection
and a printed plan provide none of that evidence. Existing denials/stops remain
in force; selecting execution does not authorize a stopped action.

## Explicit execution

Run this only in an already prepared toolchain environment and when the complete
mixed runtime sweep is authorized:

```text
python tools/verify_changes.py --area render --execute
python tools/verify_changes.py --path engine/render/gl/renderer.cpp --execute --format json --output-dir out/verification/render-change
```

Execution uses the same planned configurations and argv selections as the plan.
It inspects prerequisites for every configuration before running the first
configure. A late missing SDK therefore prevents an earlier build from starting.
It does not activate vcvars/emsdk, fix caches, change environment values, probe
versions, download tools or retry commands. CMake and build tools can still
reconfigure and fetch dependencies themselves, as the plan's effects state.

It first reads the checkout's HEAD and counts the entries `git status
--porcelain` lists; a checkout Git cannot read stops the sweep there, before
preflight. For each configuration, in order, it:

1. Configures and requires the resulting cache to exist and agree with the
   planned axes, directories, shader compilers, toolchains and emulator/browser paths.
2. Builds with parallelism 2 and revalidates the cache because the build can
   reconfigure. It then asks CTest for its `--show-only=json-v1` inventory using
   the same preset or explicit null override directory.
3. Rejects malformed/empty inventories, duplicate names, missing executable argv,
   missing required registrations, forbidden null-render registrations and
   disabled selected tests. A selected wasm-null browser requires all its
   conditional page tests; any actually registered pages remain in the full run.
4. Runs the full CTest selection with parallelism 1, `--stop-on-failure`,
   `--test-output-size-passed 1048576` and a fresh JUnit path. Exit zero is
   insufficient: the report must cover every registered name exactly once,
   with no failed, errored, disabled or skipped suite. Missing, malformed or
   unsupported reports fail validation.
5. Reads the device from the JUnit: `RenderPixelTests` prints
   `MESSAGE: Drawing on <device>`, natively and through the page runner.
   CTest keeps only 1024 bytes of a passing test's output by default, which
   can cut that line off, hence the larger limit.

After the last configuration it reads HEAD again and fails if it moved.

The first failure stops the sweep. Later phases remain `not-run`; no automatic
fallback, exclusion or retry can turn a partial run into a successful sweep.
Logs and the structured report go into a fresh output directory. By default
this is a unique directory under ignored `out/verification/`; `--output-dir`
selects a new directory and refuses an existing one. A failed initial preflight
creates no output directory; `--format json` still prints its complete report
to stdout. Command output goes to logs so JSON stdout remains usable. The report
records argv/cwd, exit codes, validation results and actual test names.
`source_revision` holds the commit and the uncommitted entry count.
`device_identity` maps each configuration to `collected` with the device names
its `RenderPixelTests` printed, `not-printed` when that test ran without the
line, `no-device-test` when the configuration registers no `RenderPixelTests`
(the null-render ones), or `uncollected` when the sweep stopped before reading
its JUnit. It does not record a driver version and does not establish SM-18's
per-change evidence.

Execution exits **0** only when all modeled commands and result checks pass,
**1** on a prerequisite, command, validation or output failure, and **130** on
an interrupt. An interrupt does not establish descendant-process teardown;
inspect any remaining jobs before retrying. Invalid arguments/rules still exit
**2** before execution. `--preflight` and `--execute` cannot be combined.

Registration JSON must have kind `ctestInfo`, integer version 1.0, a nonempty
test list, command argv and property lists without duplicate names. JUnit must
have one `testsuite`, optionally inside a single `testsuites` container. Case
status can be `run`, `passed` or absent; failure/error/skipped children and
contradictory aggregate counts fail. Unknown report shapes fail explicitly.
`ci_gap_report.py` reads JUnit through the same parser, so it refuses the
same shapes, but it groups failed, skipped and disabled cases for its report
rather than stopping at the first.
The synthetic
fixtures exercise failure and success paths without invoking a configured tool.
Release configurations and all manual acceptance remain outside the sweep.
In particular, command success does not complete Vulkan synchronization
validation or any visual, listening, physical-input, phone or display-pacing
check.

## Real sweep qualification (2026-10-29)

In one shell with vcvars, the Emscripten environment and `VULKAN_SDK`,
`--area render --preflight --browser <Chrome 156>` exited 0. Then
`--area render --execute --browser <Chrome 156> --output-dir
out/verify-2026-10-29` ran all seven tracks and reported `passed` with exit 0.
Configure, build and test passed for `x64-debug`, `x64-debug-d3d12`,
`x64-debug-gl`, `x64-debug-vulkan`, `x64-debug-null`, `wasm-debug` and the
`wasm-release-null` override, on an RTX 5080. The runner accepted this
machine's real CTest registration JSON and JUnit: 20, 21, 20, 20, 19, 32 and
28 test cases. In a shell with vcvars but no Emscripten environment, the same
`--execute` failed at preflight, naming the missing
`EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake`, and
wrote no build log. Its reports recorded the source revision and device
identity as `uncollected`, which the runner then wrote for every run.

The focused tooling tests use synthetic repository/cache/tool paths. They do
not run a configured build or test binary:

```text
python -m unittest discover -s tools/tests -p test_verify_changes.py
python -m unittest discover -s tools/tests -p test_verification_runner.py
```

## Chromium page activation (2026-11-17)

The page runner enables the DevTools Page domain and waits for
`Page.bringToFront` before navigation and input-driver preparation. This
activates the browser's view as well as the DOM focus emulation used by input
checks. Chromium's browser process separately checks the view's focus when
admitting a fullscreen request; see the
[Chrome 157 source at the measured revision](https://chromium.googlesource.com/chromium/src/+/4b3d0dabb8250b6f152d2d85d79e7706e309d087/content/browser/renderer_host/render_frame_host_impl.cc#9550).

On this Windows machine, Chrome `157.0.8103.78` (revision
`4b3d0dabb8250b6f152d2d85d79e7706e309d087`) rejected a bare canvas request
with `TypeError: not granted` in a fresh headless profile even when
`document.fullscreenEnabled`, the fullscreen permissions policy and transient
user activation were true. DOM focus emulation made `document.hasFocus()` true
and still received the refusal. Activating the target with `Page.bringToFront`
granted the same request; a trusted dispatched click also granted it after
activation. The probe had no engine or wasm module.

With only the runner change against the `5cff44b9` browser artifacts,
`WebWindowTests` passed all 53 steps, including granted fullscreen, leaving,
and a handled refusal. `ComponentsPageTests` passed its existing key sequence
through requesting fullscreen, leaving and `Module.onQuit`. Both named ANGLE
on the NVIDIA GeForce RTX 5080 through Direct3D11. The offline
`BrowserProtocolTests` command passed 146 tests under Emscripten's Node
`24.19.0`; its activation fixtures hold the two command replies before
navigation and propagate activation failure. Full browser sweeps and hosted
CI retain their own per-run results.

## Windows headless native occlusion (2026-11-17)

Windows headless Chromium processes use
`--disable-features=CalculateNativeWinOcclusion` by default. Linux and macOS
arguments are unchanged, and explicit `LABRADOR_TEST_BROWSER_ARGS` remain
appended in their original order. This is a disposable test-process profile;
actual tab visibility remains part of the browser tests.

Fresh activated Chrome 157.0.8103.78 profiles on this Windows host produced
these bare-page animation callbacks in successive eight-second probes:

| Profile | Callbacks | Largest interval |
| --- | ---: | ---: |
| Previous defaults | 426 | 194.7 ms |
| Native occlusion disabled | 450 | 20.2 ms |
| Previous defaults, repeat | 21 | 2702.5 ms |
| Native occlusion disabled, repeat | 445 | 72.8 ms |

Each probe was focused and `visible` at its start and finish and loaded no
engine or wasm. These software callback observations reproduce an intermittent
stall and support the scoped profile comparison; they do not isolate every
slow run's cause or measure physical display pacing.

At `28844a00`, passing the flag explicitly let
`LineSweeperBundlePageTests` complete its unchanged driver in 84.9822 s under
its existing 120 s limit, including repeated play, context restoration, GPU
process crashes, real tab switching and failure UI. Its earlier full-track run
had reached that limit. The same process profile passed the 30-entry
wasm-release-null track. Platform/argument fixtures keep the default Windows
scope and explicit argument precedence; the offline `BrowserProtocolTests`
command passes 148 tests. The new default profile also passed the focused
Chrome/Firefox window and Components page cases 3/3. Full combined reruns
retain their own receipts.

## Window-owned fullscreen refusal (2026-11-17)

The Fullscreen API rejects a refused request's Promise separately from its
`fullscreenerror` event. The installed Emscripten 6.0.12 entry and exit helpers
discard those Promises. The browser window invokes its own native requests,
including the prefixed fallback, and handles only their returned rejections.
Its callback uses the pinned SDK's existing activation queue with an explicit
`EM_JS_DEPS` dependency. Leaving and closing remove that callback by identity;
active-owner and request-generation checks cover already copied callbacks and
owned Promise settlement after closure. The current window's document listeners
still observe platform fullscreen events; those events carry no request ID and
cannot be attributed by Promise generation. Page methods and page-owned
Promise handling are unchanged.

Focused wasm-debug `WebWindowTests`, `FirefoxWebWindowTests` and
`ComponentsPageTests` passed 3/3 on Chrome 157.0.8103.78 and Firefox 160.0.
The shared window driver forces immediate, prefixed and deferred refusals,
checks actual status and continuing frames, cancels queued requests on leave
and close/reopen while retaining unrelated activation work, and rejects an
old in-flight request after its owner closed.
It also refuses an owned exit while actually fullscreen and verifies that a
page-owned rejected request still reaches `unhandledrejection`; its listener
acknowledges only that deliberate page fixture after observing the event.
The existing final `Module.onError` failure and teardown checks still pass.

An additional Debug wasm-null build passed `AppTests`, `WebWindowTests` and
`ComponentsPageTests` 3/3. Restoring the previous SDK entry path while keeping
the new tests made `WebWindowTests` fail at the first forced request: the
Promise was unhandled and no fullscreen-false callback arrived. Restoring
the owned path returned the focused suites to green. An out-only diagnostic
also made a genuine native request from an inactive browser view: one
fullscreen-false callback and a continuing tick arrived, with no unhandled
rejection. These are focused
browser/Node results; the combined full tracks and Release runs keep their
own receipts.

## Optimized browser stack dependencies (2026-11-17)

Pinned Emscripten 6.0.12 collects native callees of JavaScript library helpers
before native linking, then reads `EM_JS_DEPS` metadata afterwards. The window's
pointer bridge uses `stackSave`, `stackRestore` and `stringToUTF8OnStack`.
Debug assertion/exception tracing can retain their native operations
incidentally; optimized null-render ES modules exposed missing
`emscripten_stack_get_current` and `_emscripten_stack_restore` symbols.

The Emscripten-only engine dependency edge loads
`engine/app/web/window_dependencies.js` through `--js-library`. Its three
`__force` declarations use the SDK's existing implementations and dependency
expansion before native linking. They add no runtime/export setting override.
`INTERFACE_LINK_DEPENDS` makes the same file an incremental input of linked
executables. The file is explicitly covered by the consumer CHANGES rule.

`WebStackDependencyTests` builds a separate Node probe with private `-O3`,
`-g0`, `ASSERTIONS=0` and `EXCEPTION_STACK_TRACES=0` options even under Debug.
It links the engine's usage requirements without native engine references,
then checks a UTF-8 stack round trip and restored stack pointer. These options
are local to the probe; ordinary Debug diagnostics remain unchanged.

At `59c69c30` with this fix, the full wasm-release-null build, including
`LineSweeperModule`, passed and all 31 registered tests passed. The optimized
GL ES module and bundle also built; Release `LineSweeperBundlePageTests`,
`WebWindowTests` and the stack probe passed 3/3. The bundle completed in
94.07 s within its existing 120 s limit. The optimized probe also passed in
the Debug GL configuration.

A separate Release `add_subdirectory` consumer built and ran under Node with
spaces in both its project and engine-source paths. Removing only the metadata
link edge reproduced both exact missing stack symbols. Restoring the edge
and relinking returned the probe to green. Touching only the metadata file
caused a consumer relink with no C++ recompilation. The starter assembler's
ordinary source-copy set includes this file. These are local link/runtime
results; hosted CI retains its own qualification.

<a id="configurations-ci-cannot-run"></a>

## Release and real-driver qualification

Ordinary required PR CI runs `RenderPixelTests`, `MinimalSmokeTests` and
`CollisionSmokeTests` on the two Direct3D Debug presets and `x64-release` with
[explicit WARP selection](render-software-tests.md), and on `x64-debug-gl` and
`x64-debug-vulkan` with Mesa's llvmpipe and Chrome's SwiftShader. These five
configurations leave real-driver drawing to local qualification. Their
separate default-OFF Direct3D Release builds verify startup refusal. The matrix
also leaves out the four native Release presets other than `x64-release`
and `wasm-debug`. The [additional weekly workflow](verification-weekly.md)
runs those five as non-required software checks against their existing exact
inventories. That periodic result does not qualify a later candidate or a
real driver, so the local release/driver obligations remain unchanged.
It runs before each release on a real GPU, which CONTRIBUTING's *Cutting a
release* makes a step, and is not required of a pull request.
`tools/ci_gap_report.py` holds that list; its test derives the same list from
`ci.yml` and `CMakePresets.json` and fails when they drift apart, and the
inventory's test fails when its configurations are not CI's and these.

```text
python tools/ci_gap_report.py
python tools/ci_gap_report.py --execute
python tools/ci_gap_report.py --execute --preset wasm-debug --output-dir out/ci-gap/mine
```

The default prints the plan and runs nothing. `--execute` runs configure,
build and `ctest --test-output-size-passed 1048576 --output-junit` for each
selected preset in turn, writing a
log and JUnit per preset to a new directory under ignored `out/ci-gap/`. A
failed configure or build marks that preset failed and moves on to the next,
so the record covers every selected preset. Results come from JUnit, because
`LastTest.log` writes "Test Passed." for a test whose executable is missing
and `LastTestsFailed.log` survives later runs. Any failed, skipped or disabled
case, a nonzero ctest exit, or a required test that was never registered makes
the preset fail. In the five driver-only configurations, the required tests
are the pixel and smoke tests. Real-driver runs require a reported
hardware-kind device. Direct3D qualification explicitly configures
`LABRADOR_D3D_SOFTWARE_TEST=OFF`. Each whole candidate qualification must register
exactly its list in `tests/ctest_inventory.json`, the file CI holds its own
configurations to: an entry missing from the run or one the list does not
name fails the preset, naming the entry. `wasm-debug`'s list has the pages
configure registers only when it finds a Chromium and a Firefox, so the run
needs both: without them its headless tests would otherwise pass alone. A
configure given `LABRADOR_HOST_SITE` adds `LineSweeperHostPageTests`, which no
list names, and fails the same way. A tool missing from the shell fails its preset
and the run moves on. With `--execute` the exit code is 0 when every selected
preset passed and 1 when any failed. It is 2, with no section printed, when the checkout state cannot be
read, HEAD moved during the run, or the output directory exists or cannot be
made; argument errors also exit 2. The plan exits 0.

Its standard output is the body of the pull request template's
"Configurations outside required PR CI" section. It names the commit and how many
uncommitted changes the tree had, and lists each run preset's device as its
`RenderPixelTests` named it, read from the JUnit as the sweep reads it: "not
reached" where configure or build failed, and "no test here draws on a device"
for `x64-release-null`. The device line carries no driver version, so the
person pasting it fills in the template's "Driver:" line. It activates no
toolchain. Run it
from a cmd shell with vcvars, and with `emsdk_env.bat` for `wasm-debug`. A
cmd started from Git Bash inherits `MSYSTEM`, which makes `emsdk_env.bat`
print bash exports and leave the environment unset; clear it with
`set MSYSTEM=` first. With `wasm-debug` selected, `--execute` refuses to start
while `MSYSTEM` is set, exiting 2 before any preset runs. Passing the record
does not complete the manual checks listed above: those still go in the
template's own section.

The first run, on 2026-10-29 at `33975df` with the tool's own files
uncommitted, on the RTX 5080 (NVIDIA 616.92), passed `x64-debug-gl` and
`x64-debug-vulkan` 20/20 each with their pixel and smoke tests,
`x64-release-d3d12` 21/21, `x64-release-gl` 20/20, `x64-release-vulkan` 20/20
and `x64-release-null` 19/19. `wasm-debug` failed at configure, because the
shell had inherited `MSYSTEM` and `emcc` was not on PATH; the report named the
failing step and its log. Rerun with `MSYSTEM` cleared, it passed 32/32
including the Chromium page tests.
