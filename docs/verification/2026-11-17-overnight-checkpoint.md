# Overnight integration checkpoint, 2026-11-17

The first accepted batch, BACKLOG L66 to L83, is complete. This record closes
the combined-qualification gates for OCA-87, OCA-88, SM-77, OCA-90, OCA-92,
PI-22 and PI-23, and the later ISO-key story OCA-100. Their implementation,
consumer fixtures and specific contracts remain in their individual owners.
It does not qualify later changes merely because they share the work branch.

## Sources and results

All runs used the Windows desktop, Visual Studio Community 2026 installation
`18.9.12120.119`, its `14.51.36231` MSVC toolset and the configured vcpkg
toolchain. The isolated Sandbox installer's 18.12 version is a separate
environment, not the desktop toolchain. Native rasterizing tests used the RTX 5080;
the null configurations use no graphics/audio device. Browser builds used
Emscripten 6.0.12 and its Node 24.19.0. Chrome was 157.0.8103.78; the local
Firefox runs used 160.0. These are local results, separate from hosted
Firefox 159.0.1 evidence elsewhere.

| Source | Selection | Result |
|---|---|---|
| `59c69c30c1a065a396430ffea3137a3a304d93c7` | Five native Debug presets | 112/112: D3D11 22, D3D12 23, GL 22, Vulkan 22, null 23. The Vulkan result is the fresh serialized run described below; null is the separate continuation. |
| `59c69c30c1a065a396430ffea3137a3a304d93c7` | Five native Release presets, explicit native IPO on | 112/112, complete configure/build/test for each preset. No tests filtered or excluded. |
| `ad711c050cea427a1ec64a1d084483628c7b483c` | `wasm-debug`, GL/Web Audio | 45/45, complete build and serial CTest, including Chromium and eight Firefox entries. |
| `ad711c050cea427a1ec64a1d084483628c7b483c` | CI's `wasm-release-null` override | 31/31, complete optimized build and serial CTest. |
| `ad711c050cea427a1ec64a1d084483628c7b483c` | `lint_local.py --base f5c17f32` | All 13 phases; 497 Python tests, no skips with Git's bundled OpenSSL on the child PATH. |
| `ad711c050cea427a1ec64a1d084483628c7b483c` | Website `npm run gate` | Check, 201 tests, build, internal links and release-snapshot assembly passed; 186 pages. |
| `75131afd3aab93a1e471fd3f1354a6e6956725dd` | Isolated ASan / ordinary native null | 24/24 and 23/23; [instrumentation and detection evidence](../verification-asan.md#combined-source-qualification-2026-11-17). |

The changes between the native epoch `59c69c30` and browser epoch `ad711c05`
add Emscripten-only JavaScript dependency metadata and its Node probe, update
its inventory/brief counts, and clarify documentation. They do not change
native runtime behavior. Native results retain their actual measured source;
no native rerun at the browser epoch is claimed.

OCA-87's appearance evidence is the separately retained
[all-rasteriser windowed/full-screen record](../review/colourwars-reports/window-19/2026-11-17/README.md).
Its eight visible client captures and eight redraws were inspected. Automated
geometry/page results do not establish scan-out or exclude transients shorter
than the capture interval. OCA-100's mapping evidence is synthetic; no physical
ISO keyboard is qualified. Listening, real pads/touchscreens, phone behavior,
display pacing and other owner-attended acceptance remain open in TODO.

## Initial failures and corrections

The original `verify_changes.py` execution at `59c69c30` stopped when Vulkan
RenderPixelTests reached its 120-second bound while other GPU work was active.
That overlap does not establish a cause. A serialized diagnostic completed
all 91 cases and 101,108 assertions within the same bound; a later post-stage
run also reached its bound. An investigation-only 240-second diagnostic
completed in 45.65 seconds. Finally, a fresh standard full Vulkan run passed
22/22 in 78.86 seconds, with pixel/post tests taking 29.99/31.77 seconds.
Tracked 120-second timeouts and test selection were unchanged. Original failed
reports remain retained rather than being relabelled as passing sweeps.

At `5cff44b9`, the required Vulkan `validate_sync` pixel run passed 91 cases
and 101,108 assertions with no validation error or synchronization hazard.
Its layer settings sent output to the shell. Two known small-dedicated-allocation
warning kinds remain in the log. No Vulkan implementation changed through
this checkpoint; later Vulkan changes require a new synchronization check.

Headless Chrome initially denied fullscreen despite document focus and user
activation. Bringing the page to the foreground fixed the driver interaction.
The engine also now owns rejection handling for its fullscreen Promises,
without taking over page-owned requests or unrelated errors. Intermittent
Windows native occlusion stalled animation frames; the headless runner now
disables that calculation on Windows while retaining the actual tab-visibility
tests. Two Firefox session-creation timeouts passed fresh retries; the final
complete 45-entry run then passed without filtering.

Optimized Emscripten linking exposed stack helpers whose dependency metadata
was being read too late. The engine now supplies exact early JavaScript
library dependencies, with incremental link tracking. The full optimized null
build and shipping ES module pass; undefined-symbol checks remain enabled.
The new Node probe verifies the optimized dependency path. These fixes and
the fresh complete browser runs supersede the earlier failures, whose logs
are preserved.

## Retained local receipts

The closure-only documentation changes also passed a fresh full 13-phase lint
run (497 Python tests, no skips) and website gate (201 tests, 186 pages).
Their logs are `out/overnight/checkpoint-doc-lint.log` and
`out/overnight/checkpoint-doc-website.log`; no extra runtime rerun is implied.

The ignored `out/` files are local evidence, not release artifacts or files
promised to another checkout. The planner report records the failed first
execution; continuation JUnit and reports supply the later passing results.

- `out/overnight/integrated-sweep.json` and
  `out/verification/overnight-integrated-59c69c30/`: first native execution.
- `out/overnight/portable-integrated-59c69c30/`: native-null continuation and
  the original optimized-link failure.
- `out/overnight/vulkan-final-serialized.log` and `.xml`: fresh full Vulkan run.
- `out/overnight/vulkan-sync.log` and `vk-layer/vk_layer_settings.txt`:
  synchronization run and settings.
- `out/overnight/release-integrated-59c69c30/`: per-preset logs, JUnit and report.
- `out/overnight/browser-fixed-ad711c05/`: both complete browser runs, commands,
  exit codes and JUnit.
- `out/overnight/fixed-browser-lint.log` and `fixed-browser-website.log`:
  complete final gates.

SHA-256 values below identify the receipts at this checkpoint. The sanitizer
record links its own source-specific receipts and dependency hashes.

| Local receipt | SHA-256 |
|---|---|
| `out/overnight/integrated-sweep.json` | `21e0884f919bacfd603bedd22ae36b26677e55fc065001dc6da949304613aa05` |
| `out/overnight/release-integrated-59c69c30/report.json` | `2d861a97ca06b5654d3412be0d20c0162e52008136a15731efb19a545921138e` |
| `out/overnight/vulkan-final-serialized.xml` | `d099b05e562a77df76827d31a9832a5791ec57d30562f45ce91f220498dce5c7` |
| `out/overnight/vulkan-sync.log` | `a43392c71b78b1a14c5a364630c94292942ffda481c1574b0b1fc1af8294153f` |
| `out/overnight/browser-fixed-ad711c05/report.json` | `ce23febe9431386910d9e926c21acc94b9120b0142afbf5b3a40af960be6e9b1` |
| `out/overnight/browser-fixed-ad711c05/debug.xml` | `e72805f41d527142f62df805e1596693f3b47a63d37acc03c7123189300411c6` |
| `out/overnight/browser-fixed-ad711c05/null.xml` | `279fdc0efaaab257cd572f2f3142fef526ebac5f8295eeaad469f0c202b02565` |
| `out/overnight/fixed-browser-lint.log` | `db8cc29745277872d8b883a2b85fef814ab0c60e4a3432dadbc69c562c8a59ed` |
| `out/overnight/fixed-browser-website.log` | `9fa05ab82e6bd917b91ec414435d87cbe9366eded62d2db1e725d6558c7c59ae` |

No Labrador push, PR, deployment, release tag or repository-setting change was
performed for this checkpoint. Prepared schedules still require publication
and a first hosted result; local validation does not supply either.
