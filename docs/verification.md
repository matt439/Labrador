# Running a render verification sweep

`tools/verify_changes.py` prints the current render verification plan by default.
Every configure/build/test phase remains `not-run` unless `--execute` is explicit.
The executor stops at the first failed command or validation and names it.
The real sweep ran on 2026-10-29 ([qualification](#real-sweep-qualification-2026-10-29));
fake-command tests qualify the control flow. SM-18's per-change evidence
record follows it.

These examples run from the repository root:

```text
python tools/verify_changes.py --area render
python tools/verify_changes.py --path engine/render/gl/renderer.cpp --format json
python tools/verify_changes.py --path engine\render\vulkan --preflight
python tools/verify_changes.py --area render --preflight --browser "C:/Program Files/Chromium/chrome.exe"
```

From another directory, provide the absolute or relative path to this checkout's
script. It anchors the checkout to its own location.

`--area render` and repeatable `--path` selectors are mutually exclusive. The
exact `engine/render` root and its descendants are supported, including deleted
paths. Windows separators are normalized. Traversal, absolute paths and other
areas fail explicitly. Audio, documentation, sample and general engine mappings
are not modeled by this first tool; follow their owners in AGENTS.md.
Selectors are lexical: selected files are not opened and symlinks are not
resolved or checked.

Default planning reads repository source rules without tooling-environment
inspection, tool discovery, build-cache reads or Git/network contact. Standard
Python/CLI locale handling is incidental; it does not inspect the toolchain.
The checkout comes from the script's location, independent of the working
directory. Environment macros stay symbolic in the plan. Text output prints
JSON argv arrays and their cwd/effects; these are unambiguous data, including
paths with spaces, rather than shell-ready command strings. JSON output has
`schema_version: 1` and stable ordering.

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
matching build/test links, single-parent inheritance, backend defaults,
SDK-only dxc lookup and CI override. It models only the exact native-base
condition `${hostSystemName}` equals `Windows`: all ten native Debug/Release
presets inherit it, and both wasm presets have no host condition. Booleans,
null, compound predicates and other condition shapes fail explicitly.
Default planning validates that condition without inspecting the host and
still prints all seven tracks; preflight continues to require Windows for the
whole sweep. Future includes, condition or inheritance shapes and changed
source rules can require updating the model. It is not a general CMake
interpreter and does not activate user toolchain wrappers.

## Optional read-only preflight

`--preflight` inspects whitelisted environment values, tool/file paths and
existing planned CMakeCache.txt files. It executes no version probes, CMake,
CTest, compilers, SDK activation scripts, Node, browser or other subprocess.
It creates, deletes and changes no cache, build tree, environment or global file.

The checks cover the modeled Windows toolchain environment, CMake/CTest/Ninja/cl,
Direct3D fxc, VCPKG_ROOT's toolchain, Vulkan headers/import library and its own
Bin/dxc.exe, and EMSDK's toolchain/compiler entry file. A PATH dxc is no substitute
for the Vulkan SDK compiler. Emscripten's selected Node must come from the existing
EMSDK_NODE value or configured emulator; an arbitrary host Node on PATH does not
establish that selection. Presence checks do not establish versions/capabilities.

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
pixel/context/bundle/pacing entries. The executor inspects actual registrations,
rejects zero selected tests and absent required pages, then reports the selection.
`--no-tests=error` cannot detect a missing page while unrelated tests still exist.

Five native Release configurations and wasm-release GL remain separate unrun
obligations. Vulkan source paths activate the synchronization-validation reminder
with validate_sync and shell-visible layer output. Generic render selection
shows that requirement conditionally. Website/lints, consumer packaging, opt-in
probe builds and capture review keep their existing owners and remain unrun.

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

Execution uses the same seven configurations and argv selections as the plan.
It inspects prerequisites for every configuration before running the first
configure. A late missing SDK therefore prevents an earlier build from starting.
It does not activate vcvars/emsdk, fix caches, change environment values, probe
versions, download tools or retry commands. CMake and build tools can still
reconfigure and fetch dependencies themselves, as the plan's effects state.

For each configuration, in order, it:

1. Configures and requires the resulting cache to exist and agree with the
   planned axes, directories, shader compilers, toolchains and emulator/browser paths.
2. Builds with parallelism 2 and revalidates the cache because the build can
   reconfigure. It then asks CTest for its `--show-only=json-v1` inventory using
   the same preset or explicit null override directory.
3. Rejects malformed/empty inventories, duplicate names, missing executable argv,
   missing required registrations, forbidden null-render registrations and
   disabled selected tests. A selected wasm-null browser requires all its
   conditional page tests; any actually registered pages remain in the full run.
4. Runs the full CTest selection with parallelism 1, `--stop-on-failure` and a
   fresh JUnit path. Exit zero is insufficient: the report must cover every registered name
   exactly once, with no failed, errored, disabled or skipped suite. Missing,
   malformed or unsupported reports fail validation.

The first failure stops the sweep. Later phases remain `not-run`; no automatic
fallback, exclusion or retry can turn a partial run into a successful sweep.
Logs and the structured report go into a fresh output directory. By default
this is a unique directory under ignored `out/verification/`; `--output-dir`
selects a new directory and refuses an existing one. A failed initial preflight
creates no output directory; `--format json` still prints its complete report
to stdout. Command output goes to logs so JSON stdout remains usable. The report
records argv/cwd, exit codes, validation results and actual test names. It does
not collect a source revision,
driver or hardware identity and does not establish SM-18's per-change evidence.

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
wrote no build log. The report records the source revision and device
identity as `uncollected`.

The focused tooling tests use synthetic repository/cache/tool paths. They do
not run a configured build or test binary:

```text
python -m unittest discover -s tools/tests -p test_verify_changes.py
python -m unittest discover -s tools/tests -p test_verification_runner.py
```
