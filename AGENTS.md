# Labrador — notes for AI coding agents

## Start here

Read [README.md](README.md), then **NOW** in [TODO.md](TODO.md) and the owner
of the selected item. [BACKLOG.md](docs/product/BACKLOG.md) orders the stories;
tick its box in the commit that closes one. TODO.md is the open-work index.

The authoritative design documents are
[PHILOSOPHY.md](docs/design/PHILOSOPHY.md) (T1–T12 and review vocabulary),
[ARCHITECTURE.md](docs/design/ARCHITECTURE.md) (targets, modules and dependency
direction), and [CONVENTIONS.md](docs/design/CONVENTIONS.md) (naming and style).

This is the standalone C++20 engine repository. ColourWars is a separate
client that consumes it as a submodule; nothing here depends on it.
The six samples are `minimal` (the new-project template, with rules tests and
`--smoke-test`), `collision`, `audio`, `local_multiplayer`, `components`
(copyable option-row and slider code, not an engine library), and `linesweeper`.
Read [LineSweeper's README](samples/linesweeper/README.md) before changing it.
PHILOSOPHY's UI section decides what belongs in components.

## Build and test

```text
cmake --preset x64-debug
cmake --build --preset x64-debug
ctest --preset x64-debug
```

Ninja builds out of source in `out/build/<preset>/`. Visual Studio's vcvars
puts CMake, Ninja and the Windows SDK tools on PATH. `VCPKG_ROOT` is required.
Direct3D also requires `fxc` on PATH. Vulkan requires `VULKAN_SDK`: its headers,
import library and **its own** `Bin/dxc.exe` with SPIR-V support, not the Windows
SDK's identically named compiler. [compile_shaders.cmake](cmake/compile_shaders.cmake)
explains the lookup and errors. The three APIs compile one `sprite.hlsl`, and
the post stage's `post_frame.hlsli` with its materials, at their profiles; GL
compiles hand-written GLSL for both at device creation.

The browser presets require Emscripten and `EMSDK`; on this machine it is
`C:\Tools\emsdk`, version 6.0.12, matching CI. Run `emsdk_env.bat` as well as
vcvars. Emscripten's Node runs headless tests. Browser build details and open
acceptance terms are in [docs/port/web.md](docs/port/web.md), especially §9,
and what each milestone measured in [web-as-built.md](docs/port/web-as-built.md).

Five render backends are selected by `LABRADOR_RENDER_BACKEND`: `d3d11`,
`d3d12`, `gl`, `vulkan`, `null`. Three audio backends are independently selected
by `LABRADOR_AUDIO_BACKEND`: `xaudio2`, `webaudio`, `null`. Two pad backends
are selected by `LABRADOR_INPUT_BACKEND`: `xinput`, `web`. Keys, mouse and
touch are fed by the window. Browser defaults are GL/WebGL2, Web Audio and
web input; configure refuses browser Direct3D/Vulkan/XAudio2/XInput and native
Web Audio and web input.

| Preset pair | Render / audio | ctest entries and device requirements |
|---|---|---|
| `x64-debug`, `x64-release` | D3D11 / XAudio2 | 22; debug permits WARP fallback |
| `x64-debug-d3d12`, `x64-release-d3d12` | D3D12 / XAudio2 | 23; debug permits WARP fallback |
| `x64-debug-gl`, `x64-release-gl` | desktop GL / XAudio2 | 22; real GL driver |
| `x64-debug-vulkan`, `x64-release-vulkan` | Vulkan / XAudio2 | 22; SDK and driver |
| `x64-debug-null`, `x64-release-null` | null / null | 23; no graphics/audio API; no RenderPixelTests |
| `wasm-debug`, `wasm-release` | WebGL2 / Web Audio | 36 Debug / 37 Release with Chromium, 8 more with Firefox, 19 without; Node plus browser pages |

`LABRADOR_TEST_BROWSER` names a Chromium. Only when configure finds one are
`RenderPixelTests`, `RenderPixelTestsWithoutS3tc`, `RenderPixelTestsThroughPost`,
`RenderContextLossTests`, `RenderContextLossWithPostTests`, `WebWindowTests`,
all six sample `*PageTests`,
`LineSweeperReducedMotionPageTests`, `LineSweeperPalettePageTests`,
`ComponentsPointerPageTests`,
`MinimalLoadFailurePageTests` and
`LineSweeperBundlePageTests` registered. `MinimalLoadFailurePageTests` answers
the template page's `.wasm` with 404 and asserts its failure line and the
loader's console error; a failed loader script or `.data` file is untested. The bundle test loads `web-dist/linesweeper/index.html`, plays and
replays, checks the touch gate, restores a context after a forced loss and a
GPU process crash, switches tabs and asserts failure UI.
It requires a rasterizing backend. `LineSweeperHostPageTests` drives the
host site's own page the same way, registered only when `LABRADOR_HOST_SITE`
names a host checkout's `public/`; CI has no such checkout. The second pixel run hides
S3TC to test CPU block decoding. The page runner
[run_page.mjs](tests/render/web/run_page.mjs) serves the page and uses Node's
WebSocket and DevTools; sample pages must draw and call `Module.onQuit`, with
console errors, browser warnings and `Module.onError` failing the test.
Release also registers `LineSweeperPacingPageTests` for the development-page
instrument's frame protocol and exact fixture counts; it makes no physical
pacing claim and does not run in the null-render browser configuration.
`BrowserProtocolTests` holds Chromium and Firefox command replies and crash
teardown to fake WebSockets under Node, with no browser or device.
`LABRADOR_TEST_FIREFOX` names a Firefox. Where configure finds one and the
render backend rasterizes, eight `Firefox*` entries run the six sample pages,
`WebWindowTests`' sequence and the bundle page's desktop play, forced context
loss, quit and replay over WebDriver BiDi
([run_firefox.mjs](tests/render/web/run_firefox.mjs)). CI's image has a
Firefox but no WebGL2 in it (no GPU, and no software WebGL by default), so CI
runs only `FirefoxWebWindowTests`. The drawing entries need a GPU, or
`-DLABRADOR_FIREFOX_SOFTWARE_WEBGL=ON`, which puts Firefox on WARP and which
CI does not yet pass ([web-to-be-tested.md](docs/port/web-to-be-tested.md)).
CI's `safari` job, on macos-latest, runs the six sample pages and the bundle
page from the `wasm` job's artifacts in desktop Safari through
[run_safari.mjs](tests/render/web/run_safari.mjs) and safaridriver; it is not
a ctest entry. Locally, `--driver geckodriver --browser-name firefox` checks
the runner, not Safari.

The faster CI workflow is documented in [CONTRIBUTING.md](CONTRIBUTING.md).
Changes confined to `.md` files or `docs/` run the standalone CMake lint
checks below plus the offline Python tooling and Node flash, pacing and
bundle-verification suites; build jobs report without building. The website
still runs its type check, tests and build. Code changes use the affected configurations;
CI's full matrix remains required. Match focused local checks to the change.

## Verification boundaries

`python tools/verify_changes.py --area render` prints the render verification
plan; `--path engine/render/gl/renderer.cpp --format json` selects it by path.
`--area` takes `render`, `audio`, `input` or `app` and repeats for a union;
`--path` selects the areas of the `engine/` folders it names.
The optional `--preflight` inspects tooling and caches without running phases.
Explicit `--execute` runs the sweep only when its mixed runtime is authorized;
it checks all prerequisites first, validates caches and test registrations,
and stops at the first command or result failure. [Verification](docs/verification.md)
owns the command contract, logs and remaining real-sweep qualification.
The current mapping covers four engine folders:

| Changed area | Planned selections and additional obligations |
|---|---|
| `engine/render/`, including backend folders | Five native Debug render presets below, `wasm-debug` GL and CI's `wasm-release-null` override; Release and device evidence stay explicit. |
| `engine/render/gl/` | The same matrix; wasm compiles `web_context.cpp` and shared GL under clang, with pixel/no-S3TC/context page acceptance. |
| `engine/render/vulkan/` | The same matrix plus the synchronization review's `validate_sync` procedure below. |
| `engine/audio/` | `x64-debug` (XAudio2, silent bank), `x64-debug-null` (recorded waves, `AudioSmokeTests`), `wasm-debug` (Node `AudioTests`, `AudioSampleTests`, pages) and `wasm-release-null`; the XAudio2 endpoint smoke and listening stay unrun. |
| `engine/input/` | The same four tracks with `InputTests` and the browser pages; real pads and touchscreens stay unrun. |
| `engine/app/` | All seven render tracks, with `AppTests`, native `MinimalSmokeTests` and `CollisionSmokeTests` and native-null `LocalMultiplayerSmokeTests`; the window reaches every backend. |
| Other engine folders, samples, website and tooling | Not modeled by the planner; follow their current owners and affected-configuration checks. |

`python tools/ci_gap_report.py --execute` builds and tests what CI cannot
(GL/Vulkan pixel and smoke tests on a real driver, four release presets,
`wasm-debug`) and prints
the PR template's section; its test fails if `ci.yml` drifts from its list.
A preset CI never builds fails unless it registers exactly its inventory list.
It is a release step, not a per-PR one: a pull request without a GPU writes
"Not run" ([CONTRIBUTING](CONTRIBUTING.md#before-opening-a-pull-request)).

The five native selections are `x64-debug`, `x64-debug-d3d12`, `x64-debug-gl`,
`x64-debug-vulkan` and `x64-debug-null`. CI's wasm-null selection is an explicit
override build directory, not a named preset. Emitted commands and detected
paths are planning information; they do not establish a build or test result.

- A change in `engine/render/` is checked against all five backends; a change
  in `gl/` also under a wasm preset, which alone compiles `web_context.cpp` and
  compiles GL with clang. Re-run the Vulkan review's §2 synchronization check
  after a change in `engine/render/vulkan/`: use its `vk_layer_settings.txt`
  with `validate_sync` so layer output reaches the shell, not only
  `OutputDebugStringA`. [Vulkan review](docs/review/vulkan/README.md).
- Desktop `RenderPixelTests` creates a hidden window and device;
  `MinimalSmokeTests` starts a hidden sample, draws 30 frames and quits, and
  `CollisionSmokeTests` drives the collision sample's scripted route the same
  way. All three need a device. Under null render `RenderPixelTests` is not
  registered; the two smoke tests and `LocalMultiplayerSmokeTests` record
  draws instead, and null audio adds `AudioSmokeTests`. Native null also
  registers `MinimalStateTests`, the template's states driven by synthetic
  keys in a hidden Application, which the testing guide shows. CI's GL job draws on Mesa's llvmpipe and its Vulkan job on
  Chrome's SwiftShader, and fails if `RenderPixelTests` names another device;
  neither is a real driver, which `ci_gap_report.py` still covers. A passing
  release Direct3D test does not imply WARP: release has no WARP fallback.
  `renderer_seam_tests.cpp` is headless and runs with every render backend.
- Pixel tests compare read-back with the shared PNGs in `tests/render/golden/`.
  Regenerate with `LABRADOR_GOLDEN_DUMP=1` and **review every changed image**.
  After looking at each image, add an exact `Golden-Reviewed: <repo-relative-path>`
  line for it to the message of the commit that changes it. No globs or aggregate
  declaration in a later commit: the CI checker requires every changed PNG's
  own commit to carry its line. Renaming between golden PNG paths requires both
  paths; deletion or movement out requires the old path after reviewing removal.
  The checker verifies declarations, not visual judgment.
  [The procedure](CONTRIBUTING.md#golden-image-review-records)
  defines the local command and range coverage.
  `Harness::end_not_comparable` marks frames with no golden: three resize cases
  and the analytic composition/viewport cases, which assert every pixel;
  `allowed_channel_drift` in `golden_image.cpp` states the measured allowance.
  Text cases decode their BC2 atlas on the CPU to avoid device rounding; a
  separate case checks device BC2 decoding. Null read-back throws; null tests
  instead assert submitted sprites, order, textures and views.
- Null render configurations run scene fan-out/culling, real `UiText`, particle
  draws, multiplayer draws and `fanout_bench_null.cpp`. CI's separate
  `wasm-release-null` build selects null render/audio to compile and run those
  paths under clang. `application_loop_tests.cpp` runs only on native null:
  browser `Application::run()` never returns, including under wasm-null.
- `LineSweeperTests` links only the rules library and `labrador_settings`;
  `LineSweeperViewTests` links the engine; its portable cases create no device. Particles,
  compaction, event reconstruction, layouts and touch hit tests run headlessly.
  Null render also loads the sample font and records BoardView's sprites, so
  palette colours are asserted there; how the presentation looks is looked at.
  Native null render/audio also exercise the real play/pause states with a
  hidden window, synthetic input and activation, and no application loop or
  graphics/audio API device.
- Check `engine/audio/` changes beyond the default preset. `AudioTests` is
  always headless: XAudio2 presets use `SoundBank::silent()` and bank-reader
  tests; native null adds recorded wave/level/order assertions; wasm uses
  `webaudio_tests.cpp` under Node with no Web Audio, and holds a wave's loop
  region to a recording stand-in there; no XAudio2 test plays a region and null
  records none. The shared `.xwb` reader
  checks PCM/MS-ADPCM against original WAVs and refuses xWMA/XMA/compact entries.
  XAudio2 keeps DirectXTK's reader. `samples/audio/README.md` owns authoring and
  playback; XAudio2 smoke needs an endpoint. Browser `AudioSampleTests` tests
  lifecycle under Node instead of a command-line smoke. Nobody has listened to
  this backend's output; an AnalyserNode measurement is not listening evidence.
- Browser samples are `out/build/wasm-*/samples/<sample>/*.html`; each sample's
  `main_web.cpp` and CMake file show its build. `Application::run()` unwinds the
  stack before the first browser frame: keep the Application static, as the
  LineSweeper entry point does. WebGL context loss and restore notify
  `DeviceNotify` together at `begin_frame`; optional host callbacks report both
  ends from frames. Pads follow page visibility, keys canvas focus. Web Audio
  waits for the first key/click/touch; a pad press is not one, so expect
  pad-only play to be silent. `WebWindowTests` drives real input through
  the shell CSS; its visibility event is synthetic.
- Do not claim real hardware acceptance from emulation. Safari, real pads and
  touchscreens, real-phone layout/memory and display pacing have open checks in
  [web-to-be-tested.md](docs/port/web-to-be-tested.md) and TODO.md. Chrome and
  Firefox have drawn LineSweeper; M3 still owes Safari and measured pacing.
- `LineSweeperFrameBench` is a Release hardware measurement, not ctest. It
  records fixed 9,600-particle top-out, frame-start intervals, phases and actual
  device; it does not measure scan-out. Read
  [the ratchet document](docs/performance/2026-09-19-g6f-ratchet.md) before
  interpreting either EC2 run: software pacing and synchronized present can
  relocate a whole refresh wait; `record + submit` measures work. EC2's L4 is
  not the unmeasured Radeon low tier. Cloud launch/stop verbs default to dry-run.
  A local run follows [the local procedure](docs/performance/local-frame-bench.md).
- `LineSweeperCapture` is also outside ctest. A presentation change requires
  regenerating its website images on `x64-release` and looking at every changed
  image before committing. Its README owns repeatability and whole-pixel text.
  The `linesweeper_web_dist` target packages the embeddable ES module, wasm,
  example, notices and `BUILD.json`; deploy belongs to the owner. The live demo
  is <https://mattharding.dev/demo/linesweeper/>; web.md §7 owns its checks.

## Website coupling

Build `website/` separately: `npm ci`, then `npm run gate`, CI's whole gate:
check, tests and build. It needs Node, not CMake; [website/README.md](website/README.md)
is its guide, [website-plan.md](docs/website-plan.md) its decisions and
[website-inventory.md](docs/website-inventory.md) its documentation sources.

The site reads design documents, public headers, vcpkg.json, sample/test
excerpts and header include sites directly from this checkout. Edits outside
`website/` can therefore fail its build. Fix any quoting page in the same commit
when changing the first/last text of an excerpt, linked paths or declarations.
Public header comments are published word for word: contract or rationale,
never history. The reference reader rejects unsupported shapes such as block
comments or a declaration opening its body on the same line. Every new public
header must be published or explicitly left out in `website/src/lib/published.ts`.
`website/published-versions.json` is append-only canonical text; once it lists
a release, `npm run build` downloads that snapshot from its GitHub release.
The website workflow runs on every pull request and on pushes to master and
`review-*` branches; both workflows skip a master push whose tree its merged
pull request already passed. Citation checking skips `website/`.

## Build checks

- `labrador_settings` applies `/W4 /WX /permissive- /sdl /fp:precise`, or under
  clang `-Wall -Wextra -Wpedantic -Werror -ffp-contract=off`: zero suppressions.
  Floating-point options protect exact zero comparisons, tolerances and NaNs.
  Clang also checks implicit copy assignment beside a defaulted copy constructor
  (declare both) and derived `draw` hiding overloads (use `using`).
- [check_engine_includes.cmake](cmake/check_engine_includes.cmake) forbids engine
  includes of game headers and **any** cross-folder include into a backend,
  including from headers and the shell. The wall covers render, audio, input
  and app platform folders. A consumer's include roots do not weaken it.
  Literal forward-slash includes normalize dot components, and a backend owns
  its whole subtree. The [fixture script](cmake/check_engine_include_fixtures.cmake)
  tests permitted and refused spellings without a compiler; ARCHITECTURE states
  the lexical scope and limits.
- [check_linesweeper_layers.cmake](cmake/check_linesweeper_layers.cmake) refuses
  literal engine includes in LineSweeper's rules and its tick declaration in
  presentation. It runs only where the sample is added; the companion fixture
  script proves failures without a compiler. The sample README states its limits.
- [check_doc_citations.cmake](cmake/check_doc_citations.cmake) checks document,
  numbered-section and T-number existence in comments on top-level builds.
  It proves name closure, not that a citation supports its claim.
  `engine/render/SEAM.md` and `engine/app/APPLICATION.md` are rationale;
  `renderer.h` and `application.h` are caller contract.
- [check_agent_brief.cmake](cmake/check_agent_brief.cmake) requires CLAUDE.md's
  first line to be `@AGENTS.md`. Amend only this brief; CLAUDE imports it.
  It also derives the preset table's ctest counts from the inventory below and
  names the row, the stated count and the derived count when they differ. It
  checks every native count, both wasm counts with Chromium and both Firefox
  counts. The 19 without a browser have no inventory source, and
  `wasm-release-null` has no row: a standalone run lists them as not checked.
  The [fixture script](cmake/check_agent_brief_fixtures.cmake)
  proves the failures. README.md's counts are not checked.
- Sources are enumerated explicitly in engine/test/sample CMakeLists.txt;
  add each new `.cpp`, or it is silently absent from the build.
- No preset sets a ctest timeout, so each entry carries its own:
  `labrador_add_test_suite` gives 60 s unless given `TIMEOUT`, sized from the
  CI durations its comment records; page entries are bounded by their
  runner's `--timeout`; every other entry sets `TIMEOUT` except the template's
  rules-only `MinimalTests`, whose copies are the game's own.
- [ctest_inventory.json](tests/ctest_inventory.json) lists the ctest entries
  each configuration must register: CI fails on any difference in the eight it
  runs, and `ci_gap_report.py` in the four other native Release presets and
  `wasm-debug`, whose list needs a Chromium and a Firefox. Adding,
  removing or renaming an entry changes it in the same commit. It sees entries,
  not the doctest cases inside them. Its `chromium_registered` list names the
  entries configure adds only with a Chromium, for the verification planner.
- CI's `consumer` makes Get Started's starter ZIP from this checkout
  (`starter_zip.py`, the release asset), extracts it with Windows' `tar`,
  builds it, verifies only
  the game's two tests are registered, runs them, installs a Release folder,
  checks `website/src/walkthrough/release-files.txt` and starts the installed
  game. Last, `rename_check.py` runs the Rename your game guide's own
  replacement line, read from the `.mdx`, on a copy, then builds and tests it
  on `x64-debug` and fails if its build files, its tests or a file in its
  build folder still name `MyGame`. `wasm-consumer` builds the same project as
  a browser page and quits it in Chrome. Follow
  [consumer_check/README.md](tools/consumer_check/README.md) for local checks;
  standalone tests cannot prove consumer-relative paths or dependencies. A clean-machine Sandbox check is separate from CI's runtime.

`python tools/lint_local.py --base origin/master` runs CI's whole `lint` job
in its order and stops at the first failure: the per-commit golden-review and
CHANGES.md checks over the range (without `--base`, HEAD alone), the CMake
lint scripts, the Node flash, pacing and bundle tests, and the Python suite.
`--list` prints the commands. Its test fails when the lint job's step names,
order or commands differ from it; the prose lists of steps are not checked.
The CMake lint scripts alone (no configure required):

```text
cmake -DREPO_DIR=C:/Projects/Labrador -P cmake/check_doc_citations.cmake
cmake -DREPO_DIR=C:/Projects/Labrador -P cmake/check_agent_brief.cmake
cmake -DREPO_DIR=C:/Projects/Labrador -P cmake/check_agent_brief_fixtures.cmake
cmake -DENGINE_DIR=C:/Projects/Labrador/engine -P cmake/check_engine_includes.cmake
cmake -DREPO_DIR=C:/Projects/Labrador -P cmake/check_engine_include_fixtures.cmake
cmake -DREPO_DIR=C:/Projects/Labrador -P cmake/check_linesweeper_layers.cmake
cmake -DREPO_DIR=C:/Projects/Labrador -P cmake/check_linesweeper_layer_fixtures.cmake
cmake -DREPO_DIR=C:/Projects/Labrador -P cmake/check_source_revision.cmake
cmake -DREPO_DIR=C:/Projects/Labrador -P cmake/check_whitespace.cmake
```

## Changes and landing

[CONTRIBUTING.md](CONTRIBUTING.md) owns the contribution and landing procedure:
branch from `master`, one self-contained commit with source, tests and every
document it makes false, then a PR and rebase-merge on green. Master is protected,
merges are linear, and required checks are strict. Update a branch with
`git rebase origin/master`, never by merging master into it: the golden-review
check in `lint` refuses merge commits. Adding, removing or renaming
a CI job or matrix entry requires the owner to update branch protection and its
required-check list.

For the owner's work with agents, accumulate self-contained commits on a local
work branch across tasks. Create the branch from `master` when no work branch
is already in use, then keep using it. Commit completed, validated work without
waiting for a separate commit request. The owner will request a PR when ready;
open one PR for the accumulated work when asked. Do not create a branch or PR
per commit. Push or open a PR only when the owner asks.

A consumer-visible change goes in [CHANGES.md](CHANGES.md), newest first under
`## Unreleased`. Touching `engine/**/*.h`, `engine/CMakeLists.txt`,
`cmake/settings.cmake` or `vcpkg.json` requires an entry or `not consumer-visible`
in the commit message, which the lint job checks per commit
(`tools/check_changes_rule.py`). The rule includes silent behaviour and dependency/flag
changes. A source break names old/new spellings and the sample migration diff.
Keep history there, never in header comments. Releases are owner-cut tags by
CONTRIBUTING's release procedure, with entries under the tag carrying them.

A vcpkg dependency change updates
`website/src/components/ReleaseNotices.astro` in the same commit. A change a
copy of the minimal template would want adds a line to
[samples/minimal/README.md](samples/minimal/README.md)'s change list.

## Implementation rules

- Dependencies point toward math. `core` is shared; only `app` may depend on
  everything. Nothing points back at `app` or `assets`. Modules are folders,
  not build targets: enforce the architecture through includes and review.
- Include from the repository root (`engine/render/renderer.h`); own header
  first, then engine, external, standard. Use `#pragma once`, no include guards.
- PascalCase types; snake_case everything else; SCREAMING_SNAKE macros alone.
  Private members end in `_`; public struct fields are bare. Accessors are
  nouns (`bounds()`). No `I`/`M`/`F` prefixes, `m_`, screaming constants or
  namespace-scope `using namespace` in headers. Follow all of CONVENTIONS,
  including explicit types except unspellable lambda types.
- `update()` writes; `draw()` is const all the way down and receives its inputs
  as parameters. Several views can call draw on the same object concurrently.
  Native/wasm null tests cover the fan-out; its benchmark compares one thread.
- Platform code stays behind render/audio/input/app seams. The platform selects
  the window implementation; `app/window.h` stays neutral. Renderer is concrete
  with one build-selected implementation, not a virtual call per sprite (T8).
  Backend translation units are `renderer.cpp`, `render_resources.cpp`,
  `texture_factory.cpp`, plus device resources where needed; D3D12's
  `sprite_pipeline.cpp` owns root signature and PSO only. GL alone selects
  one of `wgl_context.cpp`/`web_context.cpp` for its shared drawing path (web D1).
  Shared `engine/render/render_resources.cpp` owns engine data and text queries;
  backend resource code touches textures. All backend-owned data stays inside
  its folder. The shell passes a `void*` window handle through the seam.
- Glyph walking, DDS/font parsing and quad arithmetic are shared headless code;
  backends supply devices, textures, buffers, shaders, blend/rasterizer/filter
  state and API coordinate translations. D3D12 fences and Vulkan timelines own
  frames in flight behind the seam. Vulkan draws into an engine-owned image
  and blits at present, allowing a frame that is never presented. The backend
  headers and `engine/render/SEAM.md` own these decisions.
- A new public primitive ships with behavioural tests in the same commit.
  Benchmarks assert complexity class, not wall-clock: quadrupling object count
  must preserve the expected scaling independent of the machine.

## Reading records

Design documents describe the destination in the present tense of the target.
A change that fights them amends them in the same commit, with its reason.
`docs/product/` is the dated 2026-10-13 product record; only BACKLOG.md stays
current. The outside-adopter persona is an owner decision, not observed demand.

[docs/review/INDEX.md](docs/review/INDEX.md) maps every review to its live ledger.
`docs/review/` holds historical reviews: do not rewrite findings as fixes land
or trust their line numbers as current. The first two rounds' `game/...` paths
refer to ColourWars. Backend-equivalence, backend-equivalence-2, D3D12 and
Vulkan reviews postdate the split. Read their STATUS.md/README.md for current
disposition. `rtcd/` is a candidate list, not a plan; round-2/PLAN.md owns it.
Unverified findings are questions to check before acting: D3D12 §6.5
(`REV#3`) and Vulkan §6.2 (`REV#5`) remain in TODO.md.
Deleting a false sentence does not itself answer the question it got wrong.

`docs/review/colourwars-reports/` is the engine's decision on client reports,
not a review of this tree. Keep its README table current in the commit
answering a report; describe workarounds generically, as its README requires.
The moved brief's narrative and relocation map are preserved in
[the dated record](docs/2026-10-26-agent-brief-history.md).

## Scope

Bindings and rebinding belong to each game; there is no engine action map
(T1). A client evaluator request is declined in the input report. Online play,
3D, an editor and a scripting layer are permanently out. All five planned
render backends exist; no sixth is planned. Vulkan's reach toward Android,
Linux and Apple platforms is a port path, not a support claim; see
[android.md](docs/port/android.md).
