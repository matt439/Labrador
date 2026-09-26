# September 26 review fixes

All 23 findings are implemented and verified by **Fix all September 26 review
findings**, the commit introducing this file, based on `cbe3dad`.
[STATUS.md](STATUS.md) is the live ledger. The original
[review](README.md), [supplement](SUPPLEMENT.md), and reproduction evidence
remain historical and unchanged.

## Contract changes to review

- **Contacts expire at `end_tick()`.** Read the contact span before the next
  `resolve()`, `end_tick()`, or Scene destruction. Copied participant pointers
  have the same lifetime; copy needed geometry rather than retaining pointers.
- **Culling is camera-aware.** `bounds()` supplies unsnapped world geometry;
  `cull_bounds(units_per_pixel)` adds any allowance needed for view-pixel
  quantization. Existing GameObject subclasses inherit `bounds()` as their
  default. Visual, UI textures/containers, and particles implement the
  relevant allowance; Scene passes the inverse camera scale.
- **Scenes compose in submission order.** A Scene reuses the final declared
  renderer slot for its first view and appends its remaining views. Earlier
  draws remain intact; an empty Scene changes nothing. A fullscreen overlay
  needs no additional slot; several multiview scenes need capacity for
  `1 + sum(scene.view_count() - 1)`. Overlay callback indices remain local.
- **Content paths are UTF-8.** Filesystem and XAudio2 boundaries convert to
  native paths explicitly. JSON string accessors preserve their full lengths;
  identifier/path accessors reject embedded NULs with field context. Native
  path helpers also refuse embedded NULs.
- **The tunnelling budget has narrower guarantees.** Its arithmetic/API is
  unchanged. Safety requires fixed-size axis-aligned boxes translating along
  a shared coordinate axis, with strict overlap on the other axis. General
  or diagonal projected extents are a heuristic, not a safety proof.
  PHILOSOPHY T3 records this limitation.
- **Timing diagnoses state their uncertainty.** A cadence signature alone
  does not establish presentation pacing. The backend result key is now
  `presentation_wait_candidate_repetitions`, replacing
  `presentation_locked_repetitions`; phase costs remain available.

## Implementation and regression map

The named tests are added or strengthened regressions, not the full test roster.
Execution status is recorded below rather than inferred from their presence.

| Finding | Implemented behavior | Regression evidence |
|---|---|---|
| R26-01 | Scoped deferral restores the enclosing callback state, including on exceptions; an existing drain cannot reenter. | [state_context_tests.cpp](../../../tests/core/state_context_tests.cpp): “nested activation preserves update deferral before and after notification”, init/suspend/resume/result variants, and “throwing callbacks restore deferral without draining during unwinding”. |
| R26-02 | Clear the complete contact snapshot before participant retirement, including pairs whose objects survive. | [scene_tests.cpp](../../../tests/scene/scene_tests.cpp): “end_tick expires contacts whether or not their participants retire”. |
| R26-03 | Cull against unsnapped geometry plus camera-dependent quantization allowance, including rotated distant pivots. | [cull_tests.cpp](../../../tests/scene/cull_tests.cpp): “fractional world sprites survive translated zoomed camera culling”; [sprite_geometry_tests.cpp](../../../tests/render/sprite_geometry_tests.cpp): “world cull bounds contain camera-snapped corners with distant pivots”. |
| R26-04 | UI texture bounds use current frame, origin, rotation, position, and size; container unions propagate the culling allowance. Layout-only widgets without resources retain their rectangle bounds. | [cull_tests.cpp](../../../tests/scene/cull_tests.cpp): “rotated UI textures and container unions follow current geometry”; existing UiTests cover a default derived widget without render resources. |
| R26-05 | Preserve OBB axes and every represented nonzero polygon edge; use double intermediates for normalization and degeneracy checks. | [narrow_phase_tests.cpp](../../../tests/collision/narrow_phase_tests.cpp): “thin rotated rectangles retain both separating axes” and “tiny nonzero polygon edges still contribute separating axes”; [contacts_tests.cpp](../../../tests/collision/contacts_tests.cpp): “overlapping AABBs do not manufacture contact between disjoint thin polygons”. |
| R26-06 | A rotated-shape Visual keeps the shape angle plus caller rotation about its center; frame/caller origins are additional source-texel offsets. | [visual_tests.cpp](../../../tests/render/visual_tests.cpp): “rotated shape visuals preserve their center and orientation”. Also [cull_tests.cpp](../../../tests/scene/cull_tests.cpp): "rotated shape visuals emit their centered geometry with local origins", checking all recorded corners at cardinal/oblique angles, added rotation, and authored/caller origins. |
| R26-07 | Translate keyboard scan positions and the extended bit independently of layout-dependent character virtual keys; retain layout-aware text input. | [window_tests.cpp](../../../tests/app/window_tests.cpp): “keyboard messages map physical positions independently of their characters”, including AZERTY/QWERTZ and numpad/navigation distinctions. |
| R26-08 | Capture loss cancels held buttons and advances an input epoch, suppressing synthetic release edges while preserving focus. Normal last-button release remains ordinary. | [window_tests.cpp](../../../tests/app/window_tests.cpp): “capture cancellation and transfer clear buttons without a normal release”. |
| R26-09 | Validate positive dimensions, representable row strides, size multiplication, and accumulated offsets before accepting DDS/spritefont layouts. Errors retain the source name. | [dds_file_tests.cpp](../../../tests/render/dds_file_tests.cpp): layout/offset overflow and malformed large dimensions; [sprite_font_file_tests.cpp](../../../tests/render/sprite_font_file_tests.cpp): “spritefont rejects overflowed atlas layouts with the source name”. |
| R26-10 | Compare all image extent components with the queried Vulkan format limits before `vkCreateImage`. | [vulkan_texture_tests.cpp](../../../tests/render/vulkan_texture_tests.cpp): “Vulkan refuses queried image extent overflow before creating an image”, exercising width and height one above the device limit. |
| R26-11 | The benchmark depends on an always-run content-copy target, independently of relinking. | [test_benchmark_content.py](../../../tools/tests/test_benchmark_content.py): `test_incremental_target_refreshes_and_restores_deployed_content`, executing the production copy block for manifest, font, and texture changes/deletions. |
| R26-12 | Require a non-whitespace ConsoleUser locally and verify the AMI tag; establish worker shutdown ownership immediately after declared instance identity checks. | [test_cloud_performance.py](../../../tools/tests/test_cloud_performance.py): local template refusal, AMI tag mismatch, and `test_worker_early_refusal_shuts_down_only_after_identity_matches`; [worker_failure_fixture.ps1](../../../tools/tests/worker_failure_fixture.ps1) stubs metadata, cloud I/O, CPU failure, and shutdown, including upload failure. |
| R26-13 | Reject nonfinite SAT geometry before reduction, invalid resolver unit vectors/penetration, and malformed circle/AABB inputs consistently across overloads. | [narrow_phase_tests.cpp](../../../tests/collision/narrow_phase_tests.cpp): contained nonfinite geometry; [resolve_tests.cpp](../../../tests/collision/resolve_tests.cpp): nonfinite unit components; [ericson_math_tests.cpp](../../../tests/math/ericson_math_tests.cpp): “circle AABB overloads consistently reject contained nonfinite geometry”. |
| R26-14 | Particle bounds include each current shrinking quad, with an additional view-pixel culling allowance; an empty field stays empty. | [particle_draw_tests.cpp](../../../tests/linesweeper/particle_draw_tests.cpp): “particle bounds contain visible edges and survive an edge-view cull”, checking recorded corners, Scene culling, and expiry; updated [view_tests.cpp](../../../tests/linesweeper/view_tests.cpp). |
| R26-15 | Construct JSON strings using explicit lengths; loaders request NUL-free identifiers and paths. | [json_tests.cpp](../../../tests/assets/json_tests.cpp): “JSON strings retain embedded NUL while identifier access rejects it”; [asset_manifest_loader_tests.cpp](../../../tests/assets/asset_manifest_loader_tests.cpp): contextual identifier/directory rejection. |
| R26-16 | Preserve prior Scene views and append composition through the final slot, retaining the guard against dropping recorded views. | [fanout_tests.cpp](../../../tests/scene/fanout_tests.cpp): actual StateContext split-gameplay/fullscreen-overlay composition across repeated frames; [pixel_tests.cpp](../../../tests/render/pixel_tests.cpp): “a fullscreen scene overlays both split viewports after their drawing”, asserting every pixel. |
| R26-17 | Pin the executing action through focus-group mutation without copying it per activation; explicit Button copies retain independent mutable state. PHILOSOPHY T11 documents this bounded invocation-ownership exception. | [focus_tests.cpp](../../../tests/ui/focus_tests.cpp): “an action survives rebuilding its focus group”, vector-growth persistence, and independent Button-copy state. |
| R26-18 | Encode executable/native paths as UTF-8 and convert explicitly for JSON, binary assets, and wave-bank opens. | [content_root_tests.cpp](../../../tests/app/content_root_tests.cpp): “UTF-8 manifest and binary content load under non-ANSI directory names”; [file_path_tests.cpp](../../../tests/core/file_path_tests.cpp): roundtrip/NUL refusal; relocated AppTests execution described below. |
| R26-19 | Track power suspension separately from minimization; modern suspend/resume notifications produce one transition when their combined state changes. | [window_tests.cpp](../../../tests/app/window_tests.cpp): “modern power notifications are idempotent and independent of minimize”. |
| R26-20 | Narrow the displacement-budget contract to its supported geometry and trajectory assumptions; preserve the diagonal counterexample. | [tunnelling_tests.cpp](../../../tests/collision/tunnelling_tests.cpp): “projected extents cannot rule out a diagonal corner crossing”, with before/middle/after intersection checks and the supported axis-parallel case. |
| R26-21 | Validate GL formats before allocation and hold scoped texture ownership through upload and resource-table insertion. | [gl_texture_tests.cpp](../../../tests/render/gl_texture_tests.cpp): “GL texture refusal leaves no allocated object and permits retry”, checking repeated refusals preserve the sentinel binding and a valid retry succeeds. |
| R26-22 | Report the cadence signature separately from work-limited, ambiguous, or presentation-wait candidate classification using measured phase means. | [test_cloud_performance.py](../../../tools/tests/test_cloud_performance.py): `test_work_and_mixed_cost_cadence_signatures_do_not_claim_presentation_lock`, plus wait-dominated, alternating, and legacy-tail cases. |
| R26-23 | Reject zero-tick frame-rate options and timer targets; invalid seconds cannot undergo an out-of-range integer conversion or replace the old period. | [application_options_tests.cpp](../../../tests/app/application_options_tests.cpp): “accepted frame rates always produce an advancing timer period”. |

## Validation

All builds and native test runs were serialized. Each preset was rebuilt before
CTest; final null runs include the recorded rotated-geometry regression.

| Backend | Debug | Release |
|---|---|---|
| D3D11 | 14/14 | 14/14 |
| D3D12 | 14/14 | 14/14 |
| OpenGL | 14/14 | 14/14 |
| Vulkan | 14/14 | 14/14 |
| Null render + null audio | 13/13 | 13/13 |

These are **138 passing CTest entries across ten configurations**, including
pixel/golden comparisons in all eight rasterising configurations. No golden
image was regenerated. The new scene-composition case checks all 4,096 pixels
against an analytic expected image. Commands were `cmake --build --preset
<preset>` followed by `ctest --preset <preset> --output-on-failure`; build and
CTest logs are under `out/review-fixes-2026-09-26/`.

Additional checks:

- **Vulkan synchronization validation:** the Debug `RenderPixelTests` ran from
  its test content directory with `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation`,
  the shell-visible settings from the Vulkan review section 2 (`validate_sync`
  and best-practices checks enabled), and a loader trace. **44 cases / 4,523
  assertions passed**, including both oversized-atlas refusals. The trace
  confirms Khronos layer insertion. There were **zero validation errors,
  VUID reports, or synchronization hazards**. Best-practice checks emitted
  1,124 performance warnings about small dedicated buffer/image allocations;
  those are not validation errors or a new throughput measurement. Settings
  and output are retained as `vk_layer_settings.txt` and
  `vulkan-sync-validation.log` in the same output directory.
- **Unicode installation:** the same AppTests executable and DLLs ran from an
  installation directory containing `漢`: **22 cases / 136 assertions passed**,
  including executable-directory discovery and real UTF-8 manifest/binary
  loading. Output: `unicode-app-tests.log`.
- **Offline tooling:** `python -m unittest discover -s tools/tests -v`, with
  bundled CMake/Ninja supplied through `CMAKE_COMMAND`/`NINJA_COMMAND`, passed
  **50 tests with no skips**. Worker platform/cloud effects were stubbed.
- **Historical timing captures:** read-only analysis of `g6f-reference-003`
  and `004` remains complete. The five prior cadence cases remain
  presentation-wait candidates: 003 D3D11 repetition 1 and Vulkan repetition
  1; 004 D3D11 repetitions 1/2 and D3D12 repetition 5. Mean update plus
  record/submit is 1.39-2.53 ms; mean begin plus present is 14.13-15.28 ms.
  No retained evidence or historical report was rewritten.
- **Source checks:** `git diff --check` passed. Build include/citation checks
  completed, retaining the existing ambiguous-basename notices.

The initial focused UI run exposed a default-widget regression introduced
while changing bounds; the resource-free fallback was repaired and all final
UI runs pass. D3D12 Debug's incrementally linked benchmark initially faulted
at address zero. Its objects had rebuilt; preserving the executable/PDB/ILK,
removing only the generated executable and ILK, and relinking unchanged source
resolved it. The full preset then passed. The failing link artifacts remain
under `out/review-fixes-2026-09-26/benchmark-crash/`; this supports a generated
incremental-link-state issue, not a source-level numerical diagnosis.

Message-level input/power tests do not establish physical keyboard-layout or
actual sleep/wake behavior. Unicode wave-bank conversion has no `.xwb` playback
fixture. No new hardware-performance claim or cloud launch is part of these
fixes.
