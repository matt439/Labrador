# Code hygiene

This is the current owner for source organisation and comment-audit work.
[BACKLOG.md](product/BACKLOG.md#later) owns the stories and their order;
[TODO.md](../TODO.md#code-hygiene) indexes the open work. The older
[file-length audit](file-length-audit.md) remains its dated record, including
the 2026-10-26 decision to keep StateContext together.

## Rules for the work

Follow [CONVENTIONS](design/CONVENTIONS.md#files): length prompts inspection;
a split needs a responsibility boundary. A table, a cohesive algorithm or a
state machine may be long and still belong together. Record a keep verdict
when extraction would scatter ownership. Do not introduce runtime dispatch,
public API, allocation or a helper framework merely to shorten a file.

For comments, apply [the three categories](design/CONVENTIONS.md#comments):
keep caller contracts at declarations; move substantial shared rationale to
an adjacent owner document; remove archaeology and implementation restatement.
Check claims against implementation and cited passages. Citation lint checks
that names resolve, not that the prose is true. There is no comment-reduction
quota. Preserve third-party licences, attribution and vendor-derived text.

Keep behaviour changes and mechanical movement in separate changes. Enumerate
new sources in CMake. Preserve test names, assertions, fixtures and registration;
a split that silently drops a test is a regression. Public header prose is
published verbatim, and source excerpts are read by the website: update the
affected pages in the same change. Historical reviews are not rewritten.

## Current evidence

Measured on 2026-10-28 at `79b28fc`, before the documentation-only roadmap
edits. Counts include comments and blank lines and cover tracked first-party
files; external libraries, generated files and build output are excluded.

| File | Lines | Initial disposition |
|---|---:|---|
| `tests/render/pixel_tests.cpp` | 2,645 | Extract the test harness and group independent contract cases |
| `engine/render/vulkan/device_resources.cpp` | 2,166 | Review the existing device/buffer ownership boundary |
| `engine/render/d3d12/renderer.cpp` | 1,456 | Review pipeline construction separately from the frame state machine |
| `engine/render/vulkan/renderer.cpp` | 1,403 | Review alongside D3D12; no automatic matching split |
| `engine/app/win32/window.cpp` | 1,393 | Extract backend-private input translation |
| `tools/tests/test_cloud_performance.py` | 1,297 | Group tests by the production module they exercise |
| `website/src/lib/reference.ts` | 898 | Separate indexing, prose links and page assembly |
| `engine/audio/webaudio/audio_device.cpp` | 817 | Review the JS transport/C++ ownership boundary |
| `samples/linesweeper/rules/tick.cpp` | 747 | Keep the ordered world transition together |
| `tests/core/state_context_tests.cpp` | 997 | Keep the stack-lifetime cases together, per the existing review |

These counts are an audit snapshot, not a build limit or a recurring task.
Refresh a target's evidence when taking up its story.

## Maintenance lane

These are ready maintenance stories, independent of new feature selection and
of Next's physical acceptance. Work in the backlog's maintenance order: comment
batches, test organisation, Win32 translation, website organisation, then
backend responsibility reviews. A responsibility review can finish with a
justified keep verdict; an extraction not settled by the review becomes a
separate bounded story before implementation.

| Story | Work | Size |
|---|---|---|
| `SM-45` | Public-header contracts and oversized rationale | M |
| `SM-46` | Backend recording and synchronization comments | S |
| `SM-47` | Implementation and build-comment archaeology | S |
| `SM-48` | Sample teaching comments and current architecture | S |
| `SM-49` | Pixel-test harness and contract-case organisation | M |
| `SM-51` | Cloud tooling tests by production responsibility | S |
| `SM-50` | Win32 physical-key and text translation helpers | S |
| `SM-52` | Website reference indexing, linking and rendering | M |
| `SM-53` | Vulkan device/buffer ownership review | M |
| `SM-54` | D3D12/Vulkan renderer construction review | M |
| `SM-55` | Web Audio transport and C++ lifetime review | S |
| `SM-56` | Vulkan exceptional cleanup at publication and recovery | S |
| `SM-57` | D3D12 replacement texture publication | S |
| `SM-58` | D3D12 root-signature/PSO value owner, after SM-57 | M |

## SM-45. Public-header contracts and rationale

Start with `engine/input/keyboard.h`, `engine/input/gamepad.h`,
`engine/core/game_object.h`, `engine/core/state_context.h`, `engine/ui/widget.h`
and `engine/math/scalar.h`. They mix caller rules with substantial design
explanation. Large prose alone is not a defect: retain information the caller
needs to use the declaration correctly.

**Done when:** each header has a reviewed inventory of preconditions, lifetime,
threading, exceptions and observable limits; those contracts remain at the
declaration. Remove chronology and repeated implementation descriptions, and
move only rationale that has outgrown the declaration. Review cited passages
for support, rather than only existence. Review the generated reference pages,
run citation/whitespace checks and website check/tests/build, and confirm the
diff changes no declarations, executable tokens or contract meaning. A false
contract needing a behavioural decision is filed separately with evidence.

**Closed 2026-10-28:** the [six-header contract inventory](code-hygiene-public-headers.md)
records preconditions, lifetime, threading, errors, limits and checked citation
support. The comments preserve caller obligations and clarify inaccurate
descriptions of existing behavior; no declaration or executable token changes.
All five standalone CMake checks, website check, 157 website tests and the
143-page website build pass. The six rendered reference pages were inspected
for the retained contracts and declaration/member structure. No new behavior
or platform qualification is claimed.

## SM-46. Backend recording and synchronization comments

Targets: `engine/render/null/recording.h`, `engine/audio/null/recording.h`,
`engine/render/d3d12/device_resources.h` and `engine/render/d3d12/backend.h`.
Keep the backend include boundary, recording limitations and resource-lifetime
rules at their declarations; remove prior-code narratives and method counts.

**Done when:** surviving comments accurately state recording contents and
limits, handle lifetime, submit/readback/present ordering and synchronization.
Remove obsolete method counts and chronology. Keep real ownership constraints
and the distinction between recorded commands and pixels, sound or listening.
Route evidence gaps to existing review ledgers without declaring them fixed.
Confirm a comments-only diff and run citation/whitespace and affected website
checks; no new GPU or listening claim comes from this audit.

**Source audit, 2026-10-28:** the four headers describe the current code without
the include-leak history, partial-conversion narrative, SoundBank method census
or previous synchronization comment's incorrect proof. Reviewed inventory:

| Header | Current implementation evidence and retained constraints |
|---|---|
| `engine/render/null/recording.h` | `null/renderer.cpp` gathers declared views in ascending order and preserves each view's sprite order in `submit()`. `begin_frame()` clears the recording; `end_frame()` leaves it intact. The renderer owns the vector. Corners come through shared sprite geometry, and read-back throws instead of producing pixels. `tests/render/CMakeLists.txt` omits null's pixel suite. |
| `engine/audio/null/recording.h` | `null/audio_device.cpp` records bank/wave identity on every call, including voice calls, plus their applicable parameters. The comments now include those populated fields. Calls append to a device-owned vector; bank names are stored in a vector of vectors, so opening another bank may invalidate a borrowed name-list reference. Voice state changes follow explicit calls without a clock, natural completion or audible output. Bank opening stores names without reading the container. |
| `engine/render/d3d12/device_resources.h` | `device_resources.cpp` records monotonically increasing queue signals against frame indices and waits for those targets before slot reuse. `present()` obtains the next index from the swap chain. The throwing and non-throwing waits share signal/completion operations; absent queue/fence/event resources make the non-throwing form return true without waiting. `renderer.cpp` restores the prior back-buffer state after read-back and transitions to PRESENT at frame end. |
| `engine/render/d3d12/backend.h` | `renderer.cpp` queues finished view lists in view order, signals after execution, waits before allocator reuse, and retains vertex pages until device recreation or destruction. Renderer-owned clear, transitions, read-back and index-upload commands use the frame list. `texture_factory.cpp` uploads on that list, waits before staging-buffer release and reuses an existing texture name's descriptor slot. Heap recreation resets slot allocation. |

The include restriction is checked against
`cmake/check_engine_includes.cmake`: it scans engine sources, while null tests
and benchmarks may deliberately inspect their configured backend. The website
reference excludes backend folders (`website/src/lib/published.ts` and
`reference.ts`); no source excerpt in `website/src/` names these four headers.

**Closed 2026-10-28:** comparison against the parent revision, stripping comments
and whitespace, preserves every executable token in all four headers. All five
standalone CMake checks pass. Website check reports zero errors or warnings,
all 157 website tests pass, and the 143-page build completes with valid internal
links; Vite reports upstream `astro:head-inject` directive warnings. These
backend headers have no generated reference page or direct website excerpt.
This source audit runs no device, browser, audio or performance experiment. Existing runtime
evidence and its limits remain in [the review index](review/INDEX.md) and
[the D3D12 status](review/d3d12/STATUS.md); low-tier pipeline-latency qualification
remains with [the ratchet decision](performance/2026-09-19-g6f-ratchet.md#5-what-to-change-and-what-not-to).

## SM-47. Implementation and build-comment archaeology

Targets: `engine/scene/scene.cpp`, `engine/math/rectangle_rotated.cpp`,
`engine/input/xinput/xinput_gamepad.h`, `cmake/check_doc_citations.cmake` and
`cmake/compile_shaders.cmake`. Retain current culling and comparison rules,
deadzone policy, citation-checker limits and backend compiler lookup constraints.

**Done when:** comments describe current culling transforms, authoritative
comparison fields, deadzone policy and tool constraints. Preserve the reason
for non-obvious choices and the citation check's name-closure limit. Remove
former-code narratives and fragile census claims; leave diagnostic strings and
vendor text intact. Review a comments-only diff, run the standalone lints and
offline tooling, and check website excerpts that consume touched code.

**Closed 2026-10-28:** the [implementation/build inventory](code-hygiene-implementation-comments.md)
records the checked call paths and constraints. Executable tokens and
diagnostic strings are unchanged. All five standalone CMake checks and 64
permitted offline Python cases pass; previously stopped PowerShell worker and
trace fixtures were not invoked. Website check, all 157 website tests and the
143-page build pass. No runtime or hardware evidence is added.

## SM-48. Sample teaching comments

Targets: LineSweeper's `states/pause_state.h`, `states/play_state.h`,
`presentation/board_view.h` and `presentation/top_out_banner.h`, together with
their implementation comments.
Read the sample README first and retain its rules/presentation/state boundary.

**Done when:** prose teaches current suspension, result delivery, layout,
ordering and culling rather than past repository structure. Verify every
named call path against the implementation and keep useful contract details.
Coordinate PlayState's enforcement wording with SRL-19; do not claim its build
check exists before it lands. Review a comments-only diff, check citations and
build the website so its source excerpts stay valid. No captures are required
for comments-only work.

**Closed 2026-10-28:** the [sample teaching inventory](code-hygiene-sample-comments.md)
records the reviewed suspension, lifetime, result, layout, ordering and culling
paths. All eight files preserve executable tokens; citation/whitespace checks,
website check, 157 website tests and the 143-page build pass. Excerpt delimiters
are unchanged. A separate conservative-banner-extent defect is filed as
[SRL-21](product/BACKLOG.md#srl-21); no behavior or pixels changed in this audit.

## SM-49. Pixel-test organisation

The pixel contract covers sprites, text, views, resource lifetime and
readback. Keep its test-only harness separate from the contract groups and
device setup and teardown owned in one place; avoid mutable global fixtures.

**Done when:** compare the before/after inventory of cases, subcases and
assertions; all remain registered under the same test entry and names. Golden
names derive from those names, so keep names and PNG bytes unchanged. Enumerate
every moved source explicitly. Run the applicable rasterising backend tests
and wasm GL, including the no-S3TC run; retain null's deliberate omission of
RenderPixelTests. Use the required render verification matrix and report any
unrun device checks. This is movement, not a golden-regeneration exercise.

**Source organisation, 2026-10-29:** `pixel_test_harness.h/.cpp` owns the
per-case window, resource table, renderer and readback, with the same member
order and fresh content/device setup. Immutable pixel values are shared;
mutable golden naming stays solely in `golden_image.cpp`. The case groups are
frame/device lifecycle (`pixel_tests.cpp`), sprites (`sprite_pixel_tests.cpp`),
text (`text_pixel_tests.cpp`), views/composition (`view_pixel_tests.cpp`) and
resource upload/reload (`resource_pixel_tests.cpp`). The existing
RenderPixelTests target enumerates these sources; platform windows, backend
additions and the null backend's deliberate omission remain separate.

The source audit at `cbb2b23` and after the movement preserves all 50 complete
case blocks byte for byte, 12 subcase calls, 238 assertion sites, three CAPTURE
diagnostics and 26 helper/member executable bodies. Harness declarations,
field order/initializers and the Emscripten branch are unchanged after namespace
qualification. All 73 protected golden/content/platform-fixture files retain
their bytes. Case-source order can change doctest's default listing order and
diagnostic file/line locations; golden identity still uses case/subcase names
and each context's readback ordinal.

All five native Debug RenderTests targets build and their headless CTest entry
passes; all four rasterising RenderPixelTests targets compile. CTest names and
commands remain identical: 20/21/20/20/19 entries for D3D11/D3D12/GL/Vulkan/null.
A separate source review found no ownership or registration defect. All seven
standalone checks pass, as do the website check, 160 tests and 143-page build.
Native pixel execution and wasm GL with/without S3TC remain unrun at the stopped
device and Emscripten gates. No pixel executable was invoked, including for a
case listing, and no golden was regenerated. SM-49 remains open for those
runtime qualifications.

## SM-50. Win32 input translation

`engine/app/win32/window.cpp` owns HWND creation, DPI, sizing, capture and
dispatch. Scan-position translation and UTF-16 assembly belong to the private
`keyboard_input.h/.cpp` helper, with assembly state owned by each window.
Keep dispatch, focus/suspension cleanup, modifier ownership and HWND lifetime
together.

**Done when:** the neutral window API and observable input stay unchanged.
Preserve the existing hidden-window AppTests for physical keys, modifiers,
characters and focus loss. Check paired and malformed UTF-16 input with the
existing replacement policy; moving code must not silently change that policy.
Use focused headless cases where the extraction exposes a translation seam,
plus native builds and AppTests. List the new sources and run the include wall.
Physical keyboard-layout acceptance remains separately identified.

**Closed 2026-10-28:** the window retains dispatch, modifier ownership, focus
cleanup and `DefWindowProc` ordering. The key table's executable body is
unchanged after parameter-type normalization; the helper uses full-width
virtual-key/message values.

Malformed UTF-16 keeps its existing policy: a high surrogate waits and a new
high replaces it; a following low combines; another BMP unit discards the
pending high and emits that unit. An unpaired low reaches Keyboard's U+FFFD
replacement. Focus/suspension does not implicitly flush pending assembly.
This extraction adds no new IME or physical-layout support claim.

Nine helper cases and two hidden-window integration cases join the existing
AppTests entry. Debug-null AppTests builds and passes, including the prior
window, modifier, text, focus and application cases. A separate read-only
review found no behavior or registration change. Standalone checks pass;
the helper header is private to the Win32 folder and is not a public API.

## SM-51. Cloud tooling test organisation

`tools/tests/test_cloud_performance_{config,infrastructure,release,analysis,dispatch}.py`
groups cases by the production responsibility each exercises. Shared
`config()` and `write_json()` fixtures live in `cloud_performance_fixtures.py`,
with no discovery side effects or production imports through a test file.

**Done when:** unittest discovery lists the same test methods and expected
skips before and after, the full offline suite passes, and each new module can
run on its own. Preserve temporary-directory isolation, mocked cloud calls,
terminal error assertions and cleanup. Do not launch cloud work or change the
tool's CLI, schedule or infrastructure as part of moving tests.

**Source implemented 2026-10-28:** AST comparison preserves all 41 methods,
complete class/helper bodies and skip decorators: Config 7, Infrastructure 8,
Release 3, Analysis 21 and Dispatch 2. Discovery finds each once. The monolith
is removed; subject modules import only the helpers/globals they need.
Historical review probes and evidence retain their recorded module paths and
are reproduced at their recorded revisions, rather than rewritten as fixes land.

All 71 permitted offline tooling cases pass. Config, Release, Analysis and
Dispatch also pass as separate module invocations. The worker case and nine
trace-process cases remain unexecuted under the existing PowerShell stop;
their skip metadata is unchanged, not altered to turn a refusal into a pass.
The full suite and Infrastructure's full standalone run therefore remain
open, and the story is not closed. No cloud work or network call was made.

## SM-52. Website reference organisation

`website/src/lib/reference.ts` contains header enumeration/symbol indexing,
explanation and includer indexes, prose linking, declaration layout and page
assembly. Separate those responsibilities behind the current exported facade,
with one clear owner for caches and invalidation. Reuse the existing header
parser and published-header policy; do not create a second parser.

**Done when:** generated routes, anchors, reference prose, source links and
guide return links are unchanged, with an acyclic module dependency graph.
Compare generated output from the same engine source revision; account for
revision stamps rather than rewriting fixtures to conceal differences. Run
the website type check, tests and build. Preserve `forgetIndexes` behavior and
validate fresh and reused caches through the existing reference tests.

**Closed 2026-10-28.** The exported facade now composes a sole index/reset
owner, a cache-free prose formatter and a page/layout assembler. Shared types
and a lazy layout callback keep imports acyclic and page bodies deferred.
[The dated parity record](2026-10-28-reference-organisation.md) preserves the
frozen source revision, unchanged output and proof-scaffold cleanup: all 93
reference pages, 10 module pages and 431 built files match exactly. The website
check, 160 tests and final 143-page build pass. Three cache regression tests
cover cold/warm/reset output, call order, borrowed maps and inventory changes.

## SM-53. Vulkan device/buffer ownership review

Review `engine/render/vulkan/device_resources.{h,cpp}`. The existing
`VulkanDevice`/`VulkanBuffer` responsibilities include shared device lifetime,
memory selection and buffer allocation/cleanup; renderer and upload code use
them as well. Assess extracting that existing owner without dividing the
commands/execute/abandon/present timeline and layout state machine.

**Done when:** record owners, callers, destruction order, failure cleanup and
the dependency graph at a proposed cut. Give a justified keep or split verdict.
For a split, scope its source changes, update the documented backend file
shape where necessary, and create its implementation story. That story must
preserve allocation/lifetime behavior and require the five-backend checks,
Vulkan upload/pending-submission tests and the review's `validate_sync` run.
The review itself makes no device-validation claim.

**Closed 2026-10-28.** The [dated source review](review/vulkan/2026-10-28-device-ownership.md)
records the device/buffer ownership graph, destruction and recovery order,
partial-allocation cleanup and caller-held waits. Keep the existing file
placement: moving buffer helpers would not simplify the coordinated frame and
generation lifetime. Two independently confirmed exception paths need SM-56:
texture-wrapper allocation outside raw-handle cleanup and read-back cleanup
using a replacement device if recovery throws. No runtime check was run.

## SM-54. Renderer construction review

Review `engine/render/d3d12/renderer.cpp` and
`engine/render/vulkan/renderer.cpp`, especially pipeline/static-resource
construction versus frame recording. D3D12's resource creation combines root
signature/PSO construction, descriptors/samplers and static-index upload.
Choose any extraction by coherent resource ownership, not matching file sizes.

**Done when:** record a per-backend keep or split verdict, including allocation
and destruction dependencies. Keep DrawList/Renderer frame recording,
abandonment, submission and resource lifetime coordinated. Do not scatter a
class's member bodies into arbitrary files or expand the public seam. A chosen
implementation gets its own story and render-matrix, pixel, allocation,
device-loss and synchronization checks. Preserve vendor-derived bodies.

**Closed 2026-10-28.** The [construction review](review/2026-10-28-renderer-construction.md)
selects a narrow D3D12 root-signature/PSO value owner (SM-58), with descriptors,
index upload, waits, frames and loss callbacks retained by the renderer. Vulkan
keeps its existing cohort. The separate confirmed vertex-shader cleanup gap is
in SM-56; the D3D12 replacement-descriptor publication gap is SM-57. Ownership,
partial-construction behavior and reset ordering are recorded separately from
each implementation's device qualification. The source review ran no device.

## SM-55. Web Audio transport review

`engine/audio/webaudio/audio_device.cpp` installs/bridges
`Module.labradorAudio` and also owns C++ bank lookup, handles and reported
voice state. Review whether a backend-private transport boundary simplifies
ownership without duplicating state between JavaScript and C++.

**Done when:** record callbacks, gesture-unlock flow, node/voice lifetime,
suspension and failure handling with a keep or split verdict. Scope any
extraction as a separate story, including the wasm source list. Its acceptance
must include browser audio lifecycle/unlock and observable transport behavior:
Node AudioTests create no AudioContext and cannot prove the moved graph works.
Native/null and Node state/handle checks still apply. Listening remains WEB#3;
this review neither closes it nor adds a new playback feature.

**Closed 2026-10-28.** The [transport review](review/2026-10-28-webaudio-transport.md)
records context, bank, node, gesture-listener, playhead and callback ownership;
the C++ handle/state and JS index protocol; suspension, close and failure
boundaries. Keep the current file: the named bridge already separates the
transport, and another translation unit adds declarations without simplifying
lifetime. No extraction or new feature is selected. Browser graph, unlock,
teardown and WEB#3 listening acceptance remain separate and unexecuted here.

## SM-56. Vulkan exceptional cleanup

The source-confirmed gaps in the [device review](review/vulkan/2026-10-28-device-ownership.md)
and [construction review](review/2026-10-28-renderer-construction.md) justify
three bounded fixes, separate from any file move:

- Keep raw texture image/memory/view cleanup active through successful
  `VulkanTexture` wrapper allocation, then transfer ownership exactly once.
  Registry publication already receives unique ownership; do not double-free
  on its insertion errors.
- Retain the read-back buffer's original shared device generation. Cleanup
  must use that device on success, normal recovered loss and a recovery
  exception after a replacement device exists. Never read lost-frame pixels,
  and preserve the original exception.
- If pixel-shader creation throws after vertex-shader creation succeeds,
  release the vertex module through its creating device and rethrow. Keep
  the existing pipeline API-result cleanup ordering.

**Done when:** each failure path has focused deterministic coverage of its
ownership/cleanup ordering, ordinary texture reload/read-back behavior remains
covered, and all five native render backends build and pass their applicable
checks. Repeat Vulkan upload/pending-submission regressions and shell-visible
`validate_sync` using the review's layer settings. Add a consumer-visible
behavior entry. Source implementation may land with qualification explicitly
open if a required operation remains stopped; that does not tick this story.
Do not add a general owner framework, alter the renderer seam or retry stopped
device operations as part of this scope.

**Source progress 2026-10-28.** Texture wrapper allocation is now the final
operation inside the raw-handle cleanup block; registry publication receives
its completed unique owner outside that catch. Read-back retains the original
`shared_ptr<VulkanDevice>` and uses its device for every local-buffer release,
without reading a lost frame. Pixel-shader creation now has a narrow catch
that releases the successful vertex module and rethrows.

All five native Debug render targets build, including all four pixel binaries,
with manifest installation disabled and existing local dependencies; each
backend's headless `RenderTests` passes. The new pixel case in
`tests/render/vulkan/shader_failure_tests.cpp` forwards ordinary loader calls,
injects a second-module API failure and checks the original error and vertex
destruction device/count. It compiles but has **not run**: its construction
creates a real device. It neither simulates genuine device loss nor exhausts
memory. All standalone lint/fixture checks pass. Independent read-only review
finds no ownership/exception or test-forwarding issue. The website check,
160 tests and 143-page build pass.

**Still open:** deterministic wrapper-allocation and recovery-exception
regressions are not yet implemented. The new shader case, pixel/resource,
upload/pending-submission and `validate_sync` runs remain at the existing
stopped GPU gate, which was not retried. Builds/headless arithmetic do not
qualify those error paths. Keep this story unchecked. Local build logs use
the `sm56-` prefix under `out/loop-2026-10-28-2114/`.

## SM-57. D3D12 replacement texture publication

The [construction review](review/2026-10-28-renderer-construction.md#separate-confirmed-issue-d3d12-replacement-descriptor-publication)
shows that reloading a name overwrites its shader-visible slot before the
replacement wrapper is allocated. Allocation failure retains the old registry
object but leaves its descriptor naming the new resource that unwinds.

**Done when:** complete throwing preparation/publication before overwriting the
existing descriptor; a failed replacement retains the old object and descriptor.
Preserve the existing wait before shader-visible writes and descriptor-slot
reuse/limit behavior. A deterministic allocation/publication-failure regression
and same-name reload cases pin that ordering. Run the native render matrix,
D3D12 pixel/resource and applicable device-loss/descriptor-lifetime checks, and
add a behavior entry. Keep stopped device checks open rather than implying
source inspection or a null backend exercised a D3D12 descriptor. This is not
a general new-name allocator rollback guarantee or the SM-58 extraction.

**Source progress 2026-10-28.** The loader obtains the existing/new slot and
CPU descriptor address, completes wrapper allocation and registry publication,
then performs the descriptor write. The local resource `ComPtr` remains alive
through that write. A replacement allocation failure cannot overwrite its old
descriptor; existing-name registry replacement is the non-allocating unique
owner assignment. The upload wait, reused slot and new-name allocation policy
are unchanged.

**Closed 2026-10-29.** `tests/render/d3d12/texture_replacement_tests.cpp`
adds "D3D12 failed same-name replacement wrapper keeps the old texture and
descriptor" to D3D12 `RenderPixelTests`. It locates the `D3d12Texture`
allocation in an identical successful same-name reload through the Debug CRT
hook in `tests/render/allocation_scope.h` (one attempt of that size), then
refuses exactly that attempt in a differently coloured reload. The load throws
`std::bad_alloc`; the registry still holds the same object, resource and slot,
and a full-target draw reads the old texels through the shader-visible
descriptor. A following successful reload reuses the slot and draws the new
texels. Reintroducing the descriptor write before wrapper allocation makes the
case crash in its post-failure draw. The refusal hook exists only in Debug
builds against the DLL CRT; Release compiles the case out.

At `b96bad6` with this and SM-56's regressions, on the RTX 5080 (NVIDIA
616.92), all five native Debug presets built and `ctest --preset <preset>
--output-on-failure` passed: `x64-debug` 20/20, `x64-debug-d3d12` 21/21, `x64-debug-gl` 20/20,
`x64-debug-vulkan` 20/20 and `x64-debug-null` 19/19. D3D12 `RenderPixelTests`
ran 58 cases and 50,242 assertions, including the 300-load slot-reuse contract,
format/BC uploads, presentation diagnostics and pending-slot regressions;
`D3D12SynchronizationTests` ran 3 cases. No existing case induces genuine
D3D12 device removal, so none was run; this story's descriptor-lifetime checks
are the slot-reuse contract and the new case. All standalone lint and fixture
checks pass. Logs are `out/ctest-<preset>.log` in the working tree.

## SM-58. D3D12 sprite-pipeline value owner

After SM-57, implement the [reviewed cut](review/2026-10-28-renderer-construction.md#bounded-implementation-story-to-file):
one backend-private `SpritePipeline` value owns the root signature and PSO,
with borrowed device/target-format inputs. Move their description and shader
array includes together. Preserve direct member `ComPtr` assignments, partial
construction and diagnostics; reset PSO before root at the existing place in
the renderer's loss callback. No heap allocation or public interface is added.

**Done when:** frame/view lists, heaps/slots, index upload, waits and callbacks
remain with their coordinating owners; source enumeration and architecture/seam
file-shape prose match the cut; descriptor/shader/diagnostic and release-order
comparison demonstrates mechanical movement. Preserve test names and golden
bytes, and run five-backend builds/checks, D3D12 pixel/resource, warmed
submission-allocation and applicable resize/loss/pending-slot regressions. Keep
vendor-derived helpers untouched. Device qualification is separate from source
movement; actual stopped actions remain stopped.
