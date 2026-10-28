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

`tests/render/pixel_tests.cpp` holds a device/readback harness plus 50 cases
covering sprites, text, views, resource lifetime and readback. Extract a
test-only harness, then group cases by the contracts they exercise. Keep
device setup and teardown owned in one place; avoid mutable global fixtures.

**Done when:** compare the before/after inventory of cases, subcases and
assertions; all remain registered under the same test entry and names. Golden
names derive from those names, so keep names and PNG bytes unchanged. Enumerate
every moved source explicitly. Run the applicable rasterising backend tests
and wasm GL, including the no-S3TC run; retain null's deliberate omission of
RenderPixelTests. Use the required render verification matrix and report any
unrun device checks. This is movement, not a golden-regeneration exercise.

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
