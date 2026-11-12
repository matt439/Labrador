# Work audit of 2026-11-12: findings

Every new row of [the audit](README.md), grouped by the lens that found it, in id order within each group. Each states the gap with its evidence in the tree at `3fe88f64`, then what would close it. Line numbers describe that commit and are not updated.

## Engine correctness and tests

### REV#122: The circle predicates disagree on a negative or non-finite radius

<a id="rev122"></a>
*fix: no owner call · OPEN · should · S*

`test_circle_AABB` refuses a negative or non-finite radius (`engine/math/ericson_math.cpp:138-145`, tested at `tests/math/ericson_math_tests.cpp:599`). `test_circle_circle` (`ericson_math.cpp:50-51`), `test_circle_triangle` (`ericson_math.cpp:196-198`, reached by `circle_triangle_intersect` and `circle_quad_intersect`), `circle_segment_intersect` (`engine/math/intersects.cpp:175-184`) and `circle_point_intersect` (`intersects.cpp:193-197`) square the radius or the sum of radii, so two circles of radius -3 five apart report an overlap. `engine/collision/narrow_phase.h:82-84` states the opposite rule: a circle whose radius is not greater than zero encloses nothing. `Circle`'s constructors and `inflate` accept any radius (`engine/math/circle.cpp:8-16, 34-37`). Nothing under `tests/` calls `circle_point_intersect`.

*Suggested.* Give the four predicates the guard `test_circle_AABB` has, state the rule once in `intersects.h`, add a table case asserting each answers false for a negative radius and for a non-finite centre or radius, and add `circle_point_intersect` cases (boundary inclusive, outside, NaN). A behaviour change in a public header: CHANGES.md entry, and a check of the website excerpts.

### REV#123: narrow_phase's documented throw for a shape with no type is untested

<a id="rev123"></a>
*fix: no owner call · OPEN · could · XS*

The contract is at `engine/collision/narrow_phase.h:85-86` and the throw at `engine/collision/narrow_phase.cpp:154-157`. Under `tests/collision`, `CHECK_THROWS_AS` appears only in `resolve_tests.cpp`, and no test names `ShapeType::none`.

*Suggested.* A test-local `Shape` returning `ShapeType::none`, asserted against each real shape type in both argument positions, a circle included because circles dispatch before polygon conversion; optionally once through `find_contacts`.

### REV#124: Camera::frame accepts non-finite rectangles and can return the zero scale its header rules out

<a id="rev124"></a>
*fix: no owner call · OPEN · should · S*

`engine/render/camera.cpp:46-47` checks only `width <= 0.0f`. A NaN extent passes and yields a NaN camera; an infinite world width gives `viewport.width / inf == 0` at `camera.cpp:56-57`, contradicting `camera.h:61-63`. `visible_rectangle` and `world_position` test only `scale == 0.0f` (`camera.cpp:71, 85`). A negative scale gives a negative-extent rectangle that Scene's cull intersects with nothing (`engine/scene/scene.cpp:296, 305`), so the view goes blank with no signal. `tests/render/camera_tests.cpp:111-120` covers zero extents only.

*Suggested.* Refuse a non-finite world rectangle or viewport extent and a computed scale that is zero or non-finite, with the same `invalid_argument`; make `visible_rectangle` and `world_position` refuse a non-finite scale; add the cases. CHANGES.md entry. Split off negative scale (mirroring) as a separate question for the owner.

### REV#125: A NaN volume, pitch or pan passes the audio clamp and reaches the backends

<a id="rev125"></a>
*fix: no owner call · OPEN · should · S*

`labrador::clamp` (`engine/math/scalar.cpp:10-22`) returns NaN because both comparisons fail. `SoundBank::clamp_levels` (`engine/audio/sound_bank.cpp:335-339`) and `AudioResources::set_group_volume` (`engine/audio/audio_resources.cpp:101`) rely on it, so `audio_device.h:57-58` ('Levels arrive here already clamped') is false for NaN. The browser backend assigns the value to `AudioParam.value` (`engine/audio/webaudio/audio_device.cpp:393, 403, 412`), a WebIDL `float`, where NaN throws a `TypeError`; XAudio2 receives `SetVolume(NaN)`. No audio test uses NaN.

*Suggested.* Throw `std::invalid_argument` naming the parameter where the clamp lives, as `Random::chance` does (`engine/core/random.h:66`); AudioTests cases on null asserting the refusal and that nothing reaches the seam, and on Node that the Web Audio stand-in never sees NaN. Amend the invariant's wording; CHANGES.md entry.

### REV#126: Nothing asserts that a warmed frame allocates nothing

<a id="rev126"></a>
*fix: no owner call · OPEN · should · M*

`docs/design/PHILOSOPHY.md:702` says per-frame code performs no heap allocation. `engine/scene/scene.h:323-339`, `engine/collision/broad_phase.h:58-60, 111` and `engine/collision/contacts.h:34-36, 51-53` claim it. The Debug CRT counter `tests/render/allocation_scope.h` is used only under `tests/render/d3d12/` and `tests/render/vulkan/`. One allocation is plausible: `Scene::draw` submits a capturing lambda through `ThreadPool::add_task(std::function<void()>)` per range (`engine/scene/scene.cpp:249-253`, `engine/core/thread_pool.h:51`), a capture near MSVC's small-buffer size. LineSweeper rebuilds HUD strings with `std::to_wstring` when values change (`samples/linesweeper/presentation/board_view.cpp:75-90`).

*Suggested.* Move `allocation_scope.h` to a shared test folder; add cases to existing executables (SceneTests, a collision suite, LineSweeperViewTests) that warm N frames and count allocations over M more. Expect zero, or a listed and justified count. Any allocation found is fixed or stated as an exception in PHILOSOPHY in the same commit. The cases compile out where the hook is absent, as its users do today.

### REV#127: The text glyph walk and Font::wrap have no complexity rows in the benchmarks

<a id="rev127"></a>
*fix: no owner call · OPEN · could · S*

`bench/main.cpp:47-65` asserts linear scaling for Scene phases, cull, quad building and fan-out. `Font::for_each_glyph` is 'the one piece of this file the frame path walks' (`engine/render/font.h:170-176`); `Font::wrap` claims each unit is visited at most twice (`engine/render/font.cpp:186-189`) while rewinding to the last space. No file under `bench/` names `Font` or `wrap`.

*Suggested.* In `bench/render_bench.cpp`, a headless `Font` from synthetic glyphs as `font_tests` builds; rows for measuring an n-character string and wrapping n words at a width that breaks every word, on the existing ladder, added to `linear_phases`.

### REV#128: StepTimer::Tick has no deterministic test, and its snap compare can overflow

<a id="rev128"></a>
*fix: no owner call · OPEN · could · S*

`engine/core/step_timer.h:115` reads `Clock::now()` inside `Tick`; the clamp (`:42, 123-127`), snap (`:145`) and catch-up loop (`:153`) are reached only by `tests/app/application_options_tests.cpp:119-134`, which spins real ticks. `std::abs(static_cast<int64_t>(timeDelta - m_targetElapsedTicks))` reaches `INT64_MIN` when the target is near 2^63, which `SetTargetElapsedTicks` accepts. `FrameTimer` already reads no clock and its tests hand it time points (`tests/app/frame_timer_tests.cpp:10-15`). The hygiene review asked for a `tests/core` StepTimer test (`docs/review/2026-11-05-hygiene/FINDINGS.md:1750-1757`), which has not landed; this folds it in.

*Suggested.* Split `Tick` into the clock read and a pure advance; cases for a 59.94 Hz delta snapping to 60 Hz, a 1 s stall clamping to six updates, a 2.5-step delta carrying its remainder, variable mode and `ResetElapsedTime`; rewrite the compare without the signed cast. `step_timer.h` is not published, so the commit says not consumer-visible.

### REV#129: RectangleI and Vector2I have no behavioural tests

<a id="rev129"></a>
*fix: no owner call · OPEN · could · S*

`tests/math` mentions them only in `constant_initialisation_tests.cpp:34, 70`. Float constructors truncate with a raw `static_cast<int>` (`engine/math/rectanglei.cpp:17-29`, `engine/math/vector2i.cpp:10-11`); `RectangleI::contains(Vector2I)` includes the right and bottom edges (`rectanglei.cpp:74-80`). None of this is stated in a header.

*Suggested.* `tests/math/integer_geometry_tests.cpp` pinning today's behaviour, and header sentences stating it. No behaviour change.

### REV#130: Whether RectangleI and Vector2I refuse non-finite, out-of-range and zero-divisor input

<a id="rev130"></a>
*owner's decision · DECISION · could · S*

`engine/math/rectanglei.cpp:17-29` and `engine/math/vector2i.cpp:10-11` cast floats directly; `vector2i.cpp:39-56, 89-96` divide with no guard; `x + width` in `contains` can overflow. `docs/product/BACKLOG.md:1507` (SM-71, rank 3) leaves generic RectangleI arithmetic to a separate decision.

*Suggested.* The owner's call between a refusal (a behaviour change with a CHANGES.md entry) and a documented precondition. REV#129's tests land either way.

### REV#131: Two collision tests build a near-miss and never check it, and a test preamble's count is stale

<a id="rev131"></a>
*fix: no owner call · OPEN · could · XS*

The triangle at `tests/math/shape_intersect_tests.cpp:138-140` and the point at `:746-748` are built and never asserted on. `tests/render/null_tests.cpp:30-35` says CI runs thirteen of fourteen entries against the GL preset. `docs/code-hygiene.md:620-623` left both for a change of their own; nothing tracks them.

*Suggested.* Add the two assertions and replace the census with a pointer to `tests/ctest_inventory.json`.

## Adopters and the guides

### REV#132: Nothing checks the guides' 'search for' anchors, and one has already drifted

<a id="rev132"></a>
*fix: no owner call · OPEN · should · S*

`website/src/content/docs/docs/guides/your-components.mdx:306` quotes `The stick is already in this engine's convention`; `samples/minimal/states/hello_state.cpp:152` now reads `The stick is in this engine's convention` (changed by `c06ccf3e`); the v0.3.0 starter still matches, so the next starter ZIP breaks step 3. `SourceFile` excerpts fail the build on drift (`website/src/lib/excerpt.ts:1-7`); prose anchors are unchecked. The first-run review's `ide-x1` suggestion (`docs/review/2026-11-10-first-run/FINDINGS.md:583`) checks `SourceFile` anchors against the released starter, a different check.

*Suggested.* Correct the anchor; add `website/tests/guide-anchors.test.mjs` extracting every 'search for `...`' and 'find the line `...`' phrase and requiring it in `samples/minimal`, `website/src/walkthrough`, the copied `samples/components` files or a code block earlier on the page, failing with page and line. It runs in `npm run gate`.

### REV#133: Whether each code-editing guide's finished files are compiled

<a id="rev133"></a>
*owner's decision · DECISION · could · M*

Counted with grep: 40 fences in `your-rule.mdx`, 26 in `your-menu.mdx`, 20 each in `your-window.mdx` and `your-screen.mdx`, 18 each in `your-collision.mdx` and `your-settings.mdx`, with whole functions such as `TitleState::clicked_row` (`your-menu.mdx:465-490`). Each page's correctness rests on its 'What was checked' run (for example `your-components.mdx:642-675`). The consumer job builds the untouched starter and one rename. `docs/review/2026-11-10/FINDINGS.md:418` (REV#44) records one such snippet as not compiled source.

*Suggested.* A pilot on three guides: their finished files checked in under `tests/guides/`, overlaid on an extracted starter inside the consumer job, and a website test that every inline block appears in them. The owner weighs the upkeep.

### REV#134: The testing guide never drives a screen with keys from a compiled test

<a id="rev134"></a>
*fix: no owner call · OPEN · should · S*

`website/src/content/docs/docs/guides/testing.mdx:461` and `:464-482`; `your-components.mdx:508, 617-618` record bugs the start-up test misses because it presses no keys. `tests/linesweeper/focus_pause_tests.cpp:19-67` builds a hidden Application under null backends, feeds `Keyboard::on_key_down`/`on_key_up`, polls and asserts `StateContext` depth. Escape in HelloState pushes ConfirmState (`samples/minimal/states/hello_state.cpp:91, 140-141`).

*Suggested.* `tests/app/minimal_state_tests.cpp`, native null only: Escape pushes ConfirmState, a second pops it. Register it, update `tests/ctest_inventory.json` and the brief's derived null counts in the same commit, then a 'Press keys in a test' section quoting it. The starter's two tests stay as they are.

### REV#135: No page explains judder on a fast display or shows drawing between steps

<a id="rev135"></a>
*fix: no owner call · OPEN · could · S*

`website/src/content/docs/docs/concepts/tick.mdx:30`. `engine/core/step_position.h:10-14` calls drawing from the last step on a 120 or 144 Hz display judder; `Application::step_position()` (`engine/app/application.h:335-345`) exists for it. Only LineSweeper's particles use it. The starter sets the greeting's position in update (`samples/minimal/states/hello_state.cpp:190`). The only site match for `step_position` is an excerpt boundary at `concepts/resources.mdx:72`.

*Suggested.* Correct the sentence, add 'Drawing between steps' to the concept page, and a day-two section where the greeting keeps last step's centre and draws at the interpolated point, stating that a state not updated this frame (under ConfirmState) must not use the fraction. The template is unchanged.

### REV#136: A guide to random numbers in the starter, after REV#35

<a id="rev136"></a>
*owner's decision · DECISION · could · S*

`engine/core/random.h:11-16`; published at `website/src/lib/published.ts:127`; only `samples/linesweeper/presentation/particles.h` uses it. `testing.mdx:92-94` says a rule is handed its random numbers without showing how. `docs/review/2026-11-10/FINDINGS.md:418` (REV#44) lists the missing guide.

*Suggested.* After REV#35: a short guide seeding a Random in the state, drawing a position, passing it to a tested rule, and warning against `<random>`'s distributions with the header's reason.

### REV#137: Copying the components into a game leaves their tests behind

<a id="rev137"></a>
*fix: no owner call · OPEN · could · S*

`docs/design/PHILOSOPHY.md:780-781`; `samples/components/README.md:23-25`; `website/src/content/docs/docs/guides/your-components.mdx:57-61, 503-510, 617-618`. `samples/components/test_content.h:3-5` includes render headers, while `samples/minimal/CMakeLists.txt:36-44` links MyGameTests to maths only.

*Suggested.* An optional step copying `option_row_tests.cpp`, `slider_tests.cpp` and `test_content.h`, a `MyGameComponentTests` target on every preset, and the ctest count going from two to three, checked in a starter. Pages that state two tests (REV#117) must still hold.

### REV#138: Hand-written declaration listings in the concept and guide pages are unchecked

<a id="rev138"></a>
*fix: no owner call · OPEN · could · S*

The listings match today (`engine/render/renderer.h:200-232`, `render_resources.h:150-152`, `engine/assets/json.h:22-85`, `resource_loader.h:57-61`) but cannot be `SourceFile` excerpts as condensed. The site already parses those headers (`website/src/lib/cpp-header.ts`).

*Suggested.* Replace one-line declarations with excerpts; mark the remaining listings with their header and add a website test that each normalised line matches a parsed declaration, failing with page and line.

## Tooling, CI and release

### REV#139: The verification planner models render paths only

<a id="rev139"></a>
*fix: no owner call · OPEN · should · M*

`tools/verify_changes.py:473` (`choices=["render"]`) and `:61`; `docs/verification.md:26`. AGENTS.md's audio paragraph names concrete configurations: XAudio2 `x64-debug` with a silent bank, `x64-debug-null` with recorded waves and AudioSmokeTests, wasm with `webaudio_tests` under Node and AudioSampleTests, and `wasm-release-null`. SM-17's aim was one command per area.

*Suggested.* `--area audio|input|app` and matching `--path` prefixes selecting subsets of the seven tracks, reusing preflight and `--execute`; manual obligations (an XAudio2 endpoint, listening under `WEB#3`) named as unrun items; synthetic cases in `tools/tests/test_verify_changes.py`.

### REV#140: The configurations only ci_gap_report runs have no inventory, and the brief contradicts itself

<a id="rev140"></a>
*fix: no owner call · OPEN · should · S*

`tests/ctest_inventory.json` lists eight configurations. `tools/ci_gap_report.py:50-58` gives the Release gaps an empty `required` list and checks `wasm-debug` only against its browser tests. AGENTS.md's preset table says 34 for `wasm-debug` with Chromium and its build-check paragraph says 33; `check_agent_brief.cmake` checks neither.

*Suggested.* Inventory lists for the five configurations; `ci_gap_report` judges each against its list in both directions; `check_agent_brief.cmake` reads the new counts and the wrong figure is corrected; one failing fixture each. No CI job or schedule changes.

### REV#141: The release procedure has no preflight

<a id="rev141"></a>
*fix: no owner call · OPEN · should · M*

`CONTRIBUTING.md:223-376`: vcpkg.json's `version-string` (`:230`), CHANGES.md's `## Unreleased` (`CHANGES.md:81`) renamed to a dated heading, commit ids in `samples/minimal/README.md`'s change list (`:262`), notes quoting `PHILOSOPHY.md:791-793` and SECURITY.md's supported revisions (`:298-318`). TODO.md's NOW records that a listable owner check becomes automation.

*Suggested.* `release_check.py --release vX.Y.Z --base vPREV --head REV`, read-only and offline, failing by name on each mismatch and running `check_changes_rule` over the range; `--notes` prints step 5's body. It tags, uploads and fetches nothing; REV#70 keeps rebuild and signing.

### REV#142: Whether CHANGES.md's rule covers every build file a consumer's configure runs

<a id="rev142"></a>
*owner's decision · DECISION · could · XS*

`tools/check_changes_rule.py:24, 35`; `CHANGES.md:20-22`. A consumer adds the repository root (`website/src/walkthrough/root-CMakeLists.txt:11`), which runs `CMakeLists.txt:1-39` (minimum version, `source_revision.cmake`, `compile_shaders.cmake`, the include check) and builds LabradorMath. CONTRIBUTING's own words: a settings.cmake flag breaks a game as hard as a source break.

*Suggested.* Add those paths to `RULE_PATHS`, amend CHANGES.md's rule, AGENTS.md and the audit's path list, with test cases. More `not consumer-visible` lines are the cost.

### REV#143: No doctest suite has a ctest timeout

<a id="rev143"></a>
*fix: no owner call · OPEN · could · XS*

`tests/CMakeLists.txt:19`; `CMakeLists.txt:49` calls `enable_testing()` only; the test presets set only `outputOnFailure`. Smoke and page entries set timeouts (`samples/minimal/CMakeLists.txt:90`, `tests/render/CMakeLists.txt:163`). `engine/render/d3d12/device_resources.cpp:583` waits `INFINITE` and `engine/render/vulkan/device_resources.cpp:2031` `UINT64_MAX`; `ci.yml:248` sets 90 minutes.

*Suggested.* An optional TIMEOUT argument with a default set from the longest suite times in recent CI logs times a generous factor, applied with `set_tests_properties`; the measurement recorded beside it. Entry names do not change.

### REV#144: CONTRIBUTING's required-check list is held to the workflows by nothing

<a id="rev144"></a>
*fix: no owner call · OPEN · could · S*

`CONTRIBUTING.md:123-151` and `:209-213`. SM-21 closed by a one-time comparison (`docs/product/BACKLOG.md:119`). `tools/tests/test_ci_gap_report.py:50-58` already derives matrix entries from `ci.yml` with the standard library.

*Suggested.* `tools/tests/test_required_checks.py`, naming the missing or extra context and reminding that branch protection changes in the same PR. GitHub's live settings stay the manual `gh api` check.

### REV#145: No single local command runs the lint job, and its three copies drift

<a id="rev145"></a>
*fix: no owner call · OPEN · could · S*

AGENTS.md's nine `cmake -P` lines; `ci.yml:151-236` also runs `check_golden_reviews.py`, `check_changes_rule.py`, three `node --test` sets and the Python suite; `CONTRIBUTING.md:155-161` restates them. `CONTRIBUTING.md:465-467` against AGENTS.md and `website/README.md:21`.

*Suggested.* `tools/lint_local.py` running the lint job's steps in order with an optional `--base`, stopping at the first failure; an offline test that fails when it and `ci.yml`'s lint job differ; AGENTS.md and CONTRIBUTING point at it; CONTRIBUTING's `npm run build` corrected.

### REV#146: The verification preflight does not refuse an inherited MSYSTEM

<a id="rev146"></a>
*fix: no owner call · OPEN · could · XS*

`docs/verification.md:287-299` records the failure and the pass once MSYSTEM was cleared. `tools/verify_changes.py:33`'s `ENVIRONMENT_KEYS` omits it; `tools/ci_gap_report.py` has no such check.

*Suggested.* Read MSYSTEM; refuse in `--preflight` and `--execute`, and in `ci_gap_report` before `wasm-debug`, with 'run `set MSYSTEM=` before emsdk_env.bat'; fake-environment cases in both suites.

### REV#147: Whether build time is measured, as PHILOSOPHY's maintained property needs

<a id="rev147"></a>
*owner's decision · DECISION · could · S*

No tool, workflow or document reads `.ninja_log` or uses `-ftime-trace`. The only figure is a hand comment at `ci.yml:46-50`; OCA-12 observed one first build.

*Suggested.* `tools/build_time.py` (offline): total wall time, slowest translation units, per-folder sums, header fan-out from depfiles, `--compare` against a record. Baselines in `docs/performance/` once the owner says how often.

### REV#148: The verification runner records the revision and device as uncollected

<a id="rev148"></a>
*fix: no owner call · OPEN · could · S*

`tools/verification_runner.py:181`; `tools/tests/test_verification_runner.py:115-116`; `docs/verification.md:236-238, 285-287`.

*Suggested.* Read HEAD and the device line RenderPixelTests already prints; record 'uncollected' only where a run did not reach it; cases in the runner's tests; amend `docs/verification.md`.

### REV#149: v0.4.0 has no preparation row

<a id="rev149"></a>
*fix: no owner call · OPEN · should · S*

`CHANGES.md:81-325`; the cadence decision at `docs/product/BACKLOG.md:16`; `SRL-07` in TODO.md; `docs/verification.md:254-256` makes the gap report a release step.

*Suggested.* Run the gap report on this machine's GPU, walk CONTRIBUTING's release steps up to the tag, and hand the owner the notes and the report's section.

### REV#159: CI's Safari job fails intermittently on the components gallery page

<a id="rev159"></a>
*fix: no owner call · OPEN · should · S*

Failing safari jobs at `f55bd469` and `9adee0d9` (branches) and `e1fa9a8d` (master) on 2026-11-05, and twice at this audit's branch on 2026-11-12, all on image `macos-26-arm64` 20261102 with Safari 27.2; the same engine and sample code passed the job at `3067c67c` on 2026-11-10. Four ended 'Safari sample deadline expired. The keys did not quit the sample' with the canvas focused and the page visible; one ended 'Safari page timed out' before the keys. `tests/render/web/page_harness.mjs:22-23` presses eight keys for the gallery, the most of any sample, each held 150 ms (`page_harness.mjs:61-67`), and `engine/input/keyboard.h:150-151` states that a key pressed and released between two polls has no sampled edge, so a stalled frame can lose a key.

*Suggested.* Record the gallery's frame times and the keys it heard in the failing case (the runner can read them before giving up), find whether frames stall or a key is dropped, and fix the cause; holding a key until the page reports it is the runner-side answer if the frames are the cause. Retries are not the fix.

## Constrained hardware and the browser

### REV#150: CI's Firefox draws nothing because it has no software WebGL2

<a id="rev150"></a>
*fix: no owner call · OPEN · should · S*

`.github/workflows/ci.yml:699-714`; `docs/port/web-to-be-tested.md:505-516` (run 41944047596); `tests/render/web/run_firefox.mjs:1-5` sets no preferences by design. The same runner draws Direct3D on its software adapter.

*Suggested.* `--software-webgl` in `run_firefox.mjs` writing candidate preferences (names verified against current Firefox) into its temporary profile; a local proof that the renderer string names WARP; the CI step extended. Dropping the runner's 'no preferences' line is a stated reversal in the commit.

### REV#151: No browser but Chromium has read back a pixel

<a id="rev151"></a>
*fix: no owner call · OPEN · could · M*

`docs/port/web-to-be-tested.md:556-566`; `ci.yml:732-744` uploads no pixel page; `ci.yml:763-809` runs the sample and bundle pages; `docs/port/web.md:1023-1031` for Firefox. WebKit's WebGL goes through ANGLE on Metal, a rasteriser the drift allowance was not measured on.

*Suggested.* Upload the wasm-release pixel and window pages; a `--test` mode in `run_safari.mjs` waiting for the page's own result; a step in the existing job. A case over `allowed_channel_drift` is recorded and marked not comparable with its reason, never silently widened.

### REV#152: Whether LineSweeper runs in Safari on the iOS Simulator ahead of SM-13

<a id="rev152"></a>
*owner's decision · DECISION · could · M*

`website/acceptance/browsers.mjs:153`; `website.yml:186-191`; `tests/render/web/run_safari.mjs` opens desktop sessions only. `docs/port/web.md:1082, 1146-1150`; `docs/port/web-to-be-tested.md:123-148`.

*Suggested.* A `--simulator` option reusing the website's capability and tap retry, run as a dispatch or weekly non-required job, with what the Simulator does not prove recorded so SM-13 shrinks to the iPhone's part. BDP-10's gate stays on the real iPhone.

### REV#153: The slow-device case has no rehearsal under throttling

<a id="rev153"></a>
*fix: no owner call · OPEN · could · S*

`docs/port/web-to-be-tested.md:134-143`; `docs/port/web.md:1169-1172`. `tests/render/web/run_page.mjs` uses only media, focus and touch emulation. StepTimer clamps a delta at 0.1 s (`engine/core/step_timer.h:41-42, 123-126`), so a slow frame runs up to six catch-up updates, which `FrameTimings::updates` records (`engine/app/application.h:137-139`).

*Suggested.* `--cpu-throttle` and `--network` in `run_page.mjs`; the R6 fixture (`docs/port/pacing-method.md` section 3) at three rates; updates per frame, phase p50/p99 and time to first frame recorded in `web-as-built.md`.

### REV#154: No test reads the wasm heap's size

<a id="rev154"></a>
*fix: no owner call · OPEN · could · S*

`cmake/settings.cmake:85-92`; one 17.1 MB observation (`docs/port/web-to-be-tested.md:330-347`); `docs/port/web.md:1162-1166`. `tests/render/web/bundle_page.mjs:43-52` already holds each instance's memory weakly for leak counting, but reads no size.

*Suggested.* Record byteLength after warm-up and at quit for the six samples and at the R6 top-out, with and without S3TC, in `web-as-built.md`; consider a no-growth gate only once the figures show steady play holds still.

### REV#155: Nobody knows what linesweeper.wasm is made of

<a id="rev155"></a>
*fix: no owner call · OPEN · should · S*

`docs/port/web-as-built.md:238`; `docs/review/2026-11-10/FINDINGS.md:700` (the M3 figure predates MODULARIZE, so compare tags); a local `wasm-release` build on 2026-11-10 gave 487,245 bytes. Nothing in the tree uses `--profiling-funcs`, a names section or binaryen's metrics.

*Suggested.* Build LineSweeper at v0.1.0, v0.2.0, v0.3.0 and master with names kept in a separate build folder; group bytes by engine module, sample, libc++ and exceptions, and data; one dated table under `docs/performance/`.

### REV#156: The WebGL context leaves powerPreference unstated

<a id="rev156"></a>
*fix: no owner call · OPEN · could · XS*

`engine/render/gl/web_context.cpp:286-306`. `powerPreference` appears nowhere in the tree. It decides between integrated and discrete adapters, the low-tier question of `docs/design/PHILOSOPHY.md:713-723`.

*Suggested.* Set the default explicitly with the reason, or reword the comment, which justifies each term by pixels; record the adapter Chrome reports under each value. A change in `gl/` runs the render matrix including a wasm preset.

### REV#157: The bundle's context restore has never met a real driver reset

<a id="rev157"></a>
*fix: no owner call · OPEN · could · S*

`docs/port/web-to-be-tested.md:304-323`: not run against the live deploy, and not on a real GPU's driver reset. `WEB#12` covers `check_live.mjs` only.

*Suggested.* `LineSweeperBundlePageTests` headed on the RTX 5080, with the dated output under that section; the live-deploy half stays with `WEB#12`.

### REV#158: Report 19's first-frame evidence can be captured here

<a id="rev158"></a>
*fix: no owner call · OPEN · could · S*

`docs/review/colourwars-reports/README.md:103`; `docs/review/colourwars-reports/window.md:670-684` (evidence items 1 and 2 and the shape they unlock); `docs/review/INDEX.md` says the evidence is absent.

*Suggested.* Capture both launches frame by frame on this desktop under each Direct3D backend, record them in `window.md`, and return the question to the owner with the evidence.
