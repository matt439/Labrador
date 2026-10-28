# Planning a render verification sweep

`tools/verify_changes.py` prints the current render verification plan. It runs
no command in any mode. Every configure/build/test phase remains `not-run`.
This first SM-17 slice helps review a sweep; the story's executing command and
real sweep acceptance remain open. SM-18's per-change evidence record follows it.

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
ctest --test-dir out/build/wasm-release-null --parallel 1 --output-on-failure --no-tests=error
```

Its build/test commands use the override directory, never the GL wasm-release
build/test preset. An explicit `--browser` adds a FILEPATH cache argument to
both planned wasm configure commands; the tool neither writes that cache nor
launches the browser. It does not copy CI's GL/Vulkan pixel/smoke exclusions
into a full local sweep.

The restricted rule reader validates the current version-6 local preset shapes,
matching build/test links, single-parent inheritance, backend defaults,
SDK-only dxc lookup and CI override. Future includes, conditions, inheritance
shapes or changed source rules can require updating the model. It is not a
general CMake interpreter and does not activate user toolchain wrappers.

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

Exit codes:

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
pixel/context entries. A future runner must inspect actual registrations, reject
zero selected tests and absent required pages, then report the actual selection.
`--no-tests=error` cannot detect a missing page while unrelated tests still exist.

Five native Release configurations and wasm-release GL remain separate unrun
obligations. Vulkan source paths activate the synchronization-validation reminder
with validate_sync and shell-visible layer output. Generic render selection
shows that requirement conditionally. Website/lints, consumer packaging, opt-in
probe builds and capture review keep their existing owners and remain unrun.

Source/recording tests do not establish rasterization, listening, physical pads,
touchscreens, Safari, real-phone memory/layout or display pacing. File detection
and a printed plan provide none of that evidence. Existing denials/stops remain
in force; this tool has no execution flag that could override them.

The focused tooling tests use synthetic repository/cache/tool paths. They do
not run a configured build or test binary:

```text
python -m unittest discover -s tools/tests -p test_verify_changes.py
```
