# November 20 TODO integration verification

The combined runtime source was `cb5b53113d97d8da82d6d4c4a231f1e8fb7a34c7`.
The checkout was clean when verification started. The native hardware runs used
an NVIDIA GeForce RTX 5080. The browser checks used Emscripten 6.0.12,
Chrome 157.0.8103.80 and Firefox 160.0.1.

`verify_changes.py --area app --area render --area input --execute` passed
all seven configurations. The runner validated each build cache, exact CTest
registration, result and source revision.

| Configuration | Passed CTest entries |
| --- | ---: |
| `x64-debug` | 22/22 |
| `x64-debug-d3d12` | 23/23 |
| `x64-debug-gl` | 22/22 |
| `x64-debug-vulkan` | 22/22 |
| `x64-debug-null` | 23/23 |
| `wasm-debug` | 52/52 |
| `wasm-release-null` | 34/34 |
| Total | 198/198 |

The [starter desktop record](../review/2026-11-10-first-run/2026-11-17-starter-helpers.md#ordinary-prompt-desktop-run-2026-11-20)
retains its separate ordinary-prompt build, test and visible launch. The game
showed its quit confirmation and the launcher subsequently returned zero.
The final input was unobserved.

The separate Vulkan pixel run used the review's shell-visible `validate_sync`
and best-practices settings. It passed 96 cases and 101,180 assertions with
zero validation errors and zero synchronization hazards. The log retains the
best-practices warnings.

A separate `wasm-release` configuration registered all 53 expected entries
with Firefox software WebGL enabled. Its 11 selected entries passed. These
covered the Chromium and Firefox pixel variants and WebWindow routes, the
Chromium touch and rebinding routes, and the Node components suite.

The website gate passed 241 tests, checked internal links, built 196 pages and
assembled both published snapshots. Whole-branch lint passed all 13 steps,
including 669 Python tests with two fixture skips. The Action selector audit
matched all 66 remote uses and 12 repository-owned uses.

## Startup policy and consumer checks

At the startup implementation revision `96dd23d4`, default Release passed
nine Direct3D 11 and ten Direct3D 12 startup cases. The fixtures exercised
known-software refusal, a typed consumer catch with its native HRESULT, and
unrelated API and invalid-window controls. The separate explicit WARP Release
builds passed all 22 Direct3D 11 and 23 Direct3D 12 entries, including pixel
readback. Both reported Microsoft Basic Render Driver as software.

The assembled WARP Release starter matched all 1,615 compared source files,
passed its two tests, installed 20 files and kept the PerMonitorV2 manifest.
The installed executable exited zero from a separate working directory with
its software-device diagnostic. Its SHA-256 was
`8a8da876ee984aff2eafcce0b9e5ad80827ed70cee59d355f1c117afb00aeef7`.

OCA-120's caller handling is shown in the [Windows release guide](../../website/src/content/docs/docs/guides/windows-release.mdx).
[SM-98](../render-software-tests.md#hosted-qualification-2026-11-20) passed
hosted default refusal, WARP and installed-starter checks. [SM-92](../action-runtime-updates.md) and
[REV#151](../port/web-to-be-tested.md#browser-comparison-and-window-checks-2026-11-20)
also await hosted execution. [SM-97](../port/ios-simulator-pilot.md) awaits a
Simulator session on macOS. Physical input, listening and display acceptance
stay with their existing owner rows.

Ignored local receipts for the combined source are under
`out/verification/todo-20261120/` and `out/todo-20261120/`. The latter retains
`lint-retry.log`, `website.log`, `vulkan-sync.log` and `wasm-release-junit.xml`.
The focused startup receipts are in `out/startup-check/` in the startup
worktree. These paths identify local receipts for this run.
