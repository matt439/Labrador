# Labrador — notes for AI coding agents

## Start here

Read [README.md](README.md), then **NOW** in [TODO.md](TODO.md) and the owner
document for the selected item. TODO is the open-work index.
[BACKLOG.md](docs/product/BACKLOG.md) owns story order and acceptance. Tick its
box in the commit that closes a story.

Labrador is the standalone C++20 engine. ColourWars is a separate client that
consumes it as a submodule. Read [LineSweeper's README](samples/linesweeper/README.md)
before changing it. The [components gallery](samples/components/README.md)
distributes copyable code. The UI section of
[PHILOSOPHY.md](docs/design/PHILOSOPHY.md) governs when a component belongs in
the engine. Game bindings and rebinding stay with the game. Online play,
3D, an editor and scripting are outside the project scope. The render backend
roster is complete at five. See
[the Android port plan](docs/port/android.md) for the platform boundary.

## Changes and landing

For the owner's work with agents, keep one local work branch across tasks.
Create it from `master` when no work branch is active. Commit completed,
validated work without waiting for a separate commit request. Push or open a PR
only when the owner asks. Open one PR for the accumulated work.

Follow [CONTRIBUTING.md](CONTRIBUTING.md) for the protected, linear `master`
workflow. Rebase with `git rebase origin/master` when updating the branch.
Remove every linked Git worktree used for the merged work immediately after
merging, without exception. Verify removal with `git worktree list` before
closing the task.
Include tests and every document the change makes inaccurate in the same
commit. Changes to required PR contexts also need the owner to update branch
protection. Weekly and sanitizer jobs stay outside required PR checks.

- Record consumer-visible changes under Unreleased in [CHANGES.md](CHANGES.md),
  newest first. For a touch to `engine/**/*.h`, `vcpkg.json` or a
  [listed consumer build input](CHANGES.md#the-rule), CI requires a change entry
  or `not consumer-visible` in that commit message. Include silent behaviour,
  flag and dependency changes. For a source break, name the old and new
  spellings and show the migration diff.
- A dependency change in `vcpkg.json` updates
  [ReleaseNotices.astro](website/src/components/ReleaseNotices.astro).
  Changes a copy of the starter would want get a dated entry in
  [samples/minimal/README.md](samples/minimal/README.md). Releases are owner-cut
  tags under the contribution procedure.
- Review every changed golden PNG. The commit that changes it needs the exact
  `Golden-Reviewed: <repo-relative-path>` line for each affected path.
  [The golden review procedure](CONTRIBUTING.md#golden-image-review-records)
  covers dumps, renames and deletions. A LineSweeper presentation change also
  requires regenerating and reviewing its website captures on `x64-release`.
  [Capture procedure](tools/linesweeper_capture/README.md)
- For ColourWars reports, update the
  [engine decision ledger](docs/review/colourwars-reports/README.md) in the
  commit that answers the report. Follow its rule for describing workarounds
  generically.

## Implementation rules

[PHILOSOPHY.md](docs/design/PHILOSOPHY.md) defines T1–T12,
[ARCHITECTURE.md](docs/design/ARCHITECTURE.md) owns targets and dependencies,
and [CONVENTIONS.md](docs/design/CONVENTIONS.md) owns code style. Design
documents describe the target. Amend a design document in the same commit as
a change that departs from it, with the reason.

- Dependencies point toward math. `core` is shared and `app` may depend on
  every module. Nothing in the engine points back at `app` or `assets`.
  Backend-owned code stays inside its folder and cross-folder includes into
  backends are forbidden. Keep platform interfaces neutral and use the
  build-selected implementations. The renderer is concrete (T8).
  [Renderer seam](engine/render/SEAM.md) ·
  [Application rationale](engine/app/APPLICATION.md)
- Include from the repository root, with the owning header first, then
  engine, external and standard headers. Use `#pragma once`, PascalCase types,
  snake_case names, SCREAMING_SNAKE macros, trailing underscores for private
  members and noun accessors. Declare explicit types except where the type
  cannot be spelled. Keep public header comments on caller contract or
  rationale. [Naming and style](docs/design/CONVENTIONS.md)
- `update()` writes. `draw()` is const through its callees and receives its
  inputs as parameters so several views can draw concurrently. Add
  behavioural tests when introducing a public primitive. Benchmarks assert
  complexity class rather than wall-clock duration.
- List each new `.cpp` in its owning CMake target. Keep warning checks intact.
  [Source enumeration](docs/source-enumeration.md) ·
  [Compiler settings](cmake/settings.cmake)

## Build and test

```text
cmake --preset x64-debug
cmake --build --preset x64-debug
ctest --preset x64-debug
```

Use Visual Studio's developer environment and set `VCPKG_ROOT`. Direct3D
needs `fxc`. Vulkan needs `VULKAN_SDK` and that SDK's SPIR-V-capable `dxc`.
Browser builds need `EMSDK` and `emsdk_env.bat`. This machine's SDK is under
`C:\Tools\emsdk`. Use the toolchain version pinned by CI. Standalone GL/WebGL2
tests compile authored material fixtures and need host `dxc` and `spirv-cross`.
Use `-DVCPKG_MANIFEST_FEATURES=materials` when those tools are supplied by
vcpkg. See [the build guide](README.md#building),
[the presets](CMakePresets.json), [shader tools](cmake/compile_shaders.cmake)
and [material authoring](docs/effects.md#64-the-games-path-and-the-tools).
[Native Release IPO](docs/performance/native-release-ipo.md) owns the
standalone policy and consumer boundary.

| Preset pair | Render / audio | ctest entries and device requirements |
|---|---|---|
| `x64-debug`, `x64-release` | D3D11 / XAudio2 | 22; Debug permits WARP fallback |
| `x64-debug-d3d12`, `x64-release-d3d12` | D3D12 / XAudio2 | 23; Debug permits WARP fallback |
| `x64-debug-gl`, `x64-release-gl` | desktop GL / XAudio2 | 22; needs a GL device |
| `x64-debug-vulkan`, `x64-release-vulkan` | Vulkan / XAudio2 | 22; needs a Vulkan device and SDK |
| `x64-debug-null`, `x64-release-null` | null / null | 23; no graphics/audio API |
| `x64-debug-null-asan` | null / null | 24; MSVC AddressSanitizer with a detection probe |
| `wasm-debug`, `wasm-release` | WebGL2 / Web Audio | 41 Debug / 42 Release with Chromium, 11 more with Firefox, 20 without either browser |

Use `LABRADOR_TEST_BROWSER` for Chromium and `LABRADOR_TEST_FIREFOX` for
Firefox. The registrations are independent. On rasterizing builds, the
eleven `Firefox*` entries cover six samples, the window, the bundle and
three pixel variants. Safari uses a separate hosted workflow job. See
[the browser build and host contract](docs/port/web.md) and
[the browser verification ledger](docs/port/web-to-be-tested.md).

The [CTest inventory](tests/ctest_inventory.json) is the source for the
checked counts and full CI and release-qualification registrations.
Change it with any registration change. The
[brief check](cmake/check_agent_brief.cmake) enforces the byte budget,
`CLAUDE.md`'s import and the preset counts it can derive. It lists omitted
counts such as browserless wasm and the CI-only `wasm-release-null`. Keep
agent guidance in this file and route detailed procedures and dated results
to their owners.

## Verification

Match local checks to the change using
[the contributor checklist](CONTRIBUTING.md#before-opening-a-pull-request).
For render, audio, input, app, collision or scene, run
`python tools/verify_changes.py --area <area>` or select changed engine
paths with `--path`. The plan includes the applicable suites, tracks and
manual obligations. `--preflight` inspects prerequisites. `--execute` runs
the mixed build, device and browser sweep with the authorization it needs.
The [verification guide](docs/verification.md) owns the matrix and execution
contract. Record what actually ran and what still needs qualification.

- For render changes check all five backends. GL changes also need a wasm
  preset. Vulkan changes also need the review's shell-visible
  synchronization procedure with `validate_sync`.
  [Vulkan review](docs/review/vulkan/README.md)
- Use [the release qualification procedure](docs/verification.md#release-and-real-driver-qualification)
  and the contributor checklist for `ci_gap_report.py` on release candidates
  and applicable PRs. CI's WARP, llvmpipe and SwiftShader provide software
  coverage. Real-driver tests and [weekly software coverage](docs/verification-weekly.md)
  have separate claims. Ordinary Direct3D Release refuses known software
  adapters. [Direct3D software-test policy](docs/render-software-tests.md)
- Null configurations record draw/audio calls and cover scene fan-out.
  Rasterizing pixel and native sample smoke checks create a hidden graphics
  device. The native XAudio2 smoke needs an output endpoint.
  [Verification obligations](docs/verification.md#reading-the-remaining-obligations) ·
  [Audio sample and listening checks](samples/audio/README.md)
- Keep browser `Application` static because `run()` unwinds the stack
  before the first browser frame. A pad press alone leaves Web Audio locked.
  Node tests, trusted browser input, software rasterizers and emulation
  establish distinct results. For
  attended Air/Safari play, physical pads and touch, real-phone acceptance,
  listening and display pacing, follow
  [the pending browser checks](docs/port/web-to-be-tested.md).
- `LineSweeperFrameBench` is a Release hardware measurement outside CTest.
  Read [the measurement decision](docs/performance/2026-09-19-g6f-ratchet.md#measurement-decision-sm-03)
  before interpreting results and follow
  [the local bench procedure](docs/performance/local-frame-bench.md).
  The packaged web bundle is built by `linesweeper_web_dist`.
  [Web host procedure](docs/port/web.md#7-what-the-host-page-gets)
- Use [the consumer checks](tools/consumer_check/README.md) for the starter
  ZIP, installed game and opt-in material fixtures. Standalone builds
  exercise different paths.

Run `python tools/lint_local.py --base origin/master` for the CI lint
sequence. `--list` prints its exact commands. For documentation-only changes
on a PR, CI runs lint and the website gate while build jobs report without
building. [CI procedure](CONTRIBUTING.md#the-procedure)

## Website and records

Run `npm ci` and `npm run gate` in `website/`. The website reads design
documents, headers, sources and assets from this checkout. Update pages that
quote a changed excerpt or declaration. Publish each new public header or
explicitly exclude it in
[website/src/lib/published.ts](website/src/lib/published.ts). Published
versions are append-only. [Website guide](website/README.md) ·
[Documentation sources](docs/website-inventory.md)

[docs/product/](docs/product/README.md) is the dated product record. Its
BACKLOG is maintained as work changes. [The review index](docs/review/INDEX.md)
routes to live status ledgers. Leave historical findings as written and
verify them against the current code before acting on them. The first two
review rounds' `game/...` paths refer to ColourWars.
