# Labrador: GPT-6 review, 2026-09-17

Reviewed commit: `9dea4516e9a985d37f98433e07655461fd9d7ec2`.

This is a historical review snapshot. Findings describe that revision and are
not updated as fixes land. Future implementation work should reference the
finding IDs below. Source line numbers refer to the reviewed revision.

## Assessment

Keep the architecture. The module boundaries, null backends, common pixel
contract, and independently testable game rules are useful foundations. The
highest-value next work is lifecycle and integration correctness.

The recurring gap is between a component's tested behavior and its caller's
assumptions: drawing and culling disagree about bounds; explicit stack clearing
works while ordinary destruction does not; menu confirmation becomes gameplay
input. Several comments describe safeguards that these combinations do not
provide. Regression tests should exercise the claimed invariant through its
consumer, rather than only asserting the helper's present implementation.

## Scope and verification

The review combined source inspection, the repository's design documents and
current review status, full builds/tests, and focused C++ probes. Parallel lanes
covered core/math/collision/scene, rendering, and build/sample integration;
application, input, assets, audio, and UI were inspected alongside them.

Validation used the existing configured Windows build trees:

| Preset | CTest result |
|---|---|
| `x64-debug-null` | 13/13 passed |
| `x64-debug` | 14/14 passed |
| `x64-release` | 14/14 passed |
| `x64-debug-d3d12` | 14/14 passed |
| `x64-debug-gl` | 14/14 passed |
| `x64-debug-vulkan` | 14/14 passed |

That is 83 successful test-entry runs, with shared suites repeated across
configurations, not 83 distinct test cases. Pixel/golden comparisons ran on all
four rasterizing backends. No golden images were regenerated. No engine, sample,
or test source was changed during the review.

Focused probes demonstrated failures that those suites do not cover. Their
inputs and observations are recorded below. Local probe sources and runners
remain under `out/review-gpt6/`, `out/review-core/`,
`out/review-gpt6-render/`, and `out/review_delivery/`; these are ignored local
artifacts, not checked-in regression tests or prerequisites for this document.

Limits: this was not exhaustive coverage of every source path. Interactive
playtesting, actual audio playback/recovery, long-running GPU stress, and a
low-tier performance measurement were not performed. The Vulkan run was the
normal test preset, not a new synchronization-validation campaign. Menu findings
use source inspection plus headless probes, rather than a played UI sequence.

## Findings

Priority denotes suggested implementation order: P1 is a high-priority lifetime
hazard, P2 is a correctness or integration defect, and P3 is a presentation defect.
Every finding below was open at the reviewed revision.

| ID | Priority | Finding |
|---|---|---|
| G6-01 | P1 | Window destruction leaves a native window with dangling callback data |
| G6-02 | P2 | Ordinary state-stack destruction runs bottom-first |
| G6-03 | P2 | Math constants remain dependent on static initialization order |
| G6-04 | P2 | Transformed sprites and labels can be culled while visible |
| G6-05 | P2 | Segment queries miss polygon crossings through corners |
| G6-06 | P2 | Restoring a minimized window can discard its new dimensions |
| G6-07 | P2 | Pause confirmation and cancellation leak into gameplay |
| G6-08 | P2 | Pause initialization does not suppress an already-held direction |
| G6-09 | P2 | Sample asset loading depends on the working directory |
| G6-10 | P2 | Targeted builds bypass the architecture checks |
| G6-11 | P3 | Particle reconstruction can identify the wrong cleared row |

### G6-01. Window destruction leaves dangling callback data

**Source:** [window.cpp:218-222](../../../engine/app/window.cpp#L218),
[minimal error handling](../../../samples/minimal/main.cpp#L38).

`Window::~Window()` is defaulted. Its rationale assumes the message loop has
returned after `WM_DESTROY`. A failure during initialization or manifest loading,
or an exception escaping the loop, can unwind the owner while the native window
still exists. `GWLP_USERDATA` still points to the destroyed `Window`.

**Reproduction:** construct a hidden `Window`, retain its `HWND` and user-data
value, then leave scope without calling `close()` or running the pump. The probe
reported `IsWindow=1` and `stale_userdata=1`. It detached the user data before
cleanup and deliberately did not dereference the dangling pointer.

**Impact:** subsequent window messages can dispatch through freed C++ state. The
samples' catch blocks display a message box after `Application` has unwound,
providing a concrete path back into message processing. The dangling native
window was reproduced; a crash was not deliberately induced.

**Suggested fix and regression:** make native-window ownership and cleanup
idempotent. Detach callbacks and destroy any surviving window before its owner
and services become unavailable. Cover normal close, startup failure after
window creation, and an exception escaping the loop. Simply adding a late
`DestroyWindow` call must account for the callbacks destruction itself can send.

### G6-02. Ordinary state-stack destruction runs bottom-first

**Source:** [state_context.cpp:9](../../../engine/core/state_context.cpp#L9),
[clear()](../../../engine/core/state_context.cpp#L80).

The defaulted destructor lets the vector destroy frames in its implementation's
element order. On this toolchain, a scoped context containing states 1, 2, 3
destroyed them as `1 2 3`. The dependency order required by the stack is
`3 2 1`: an upper state may borrow resources owned by a lower one.

**Impact:** standalone or nested contexts can destroy a borrowed resource before
the state using it finishes teardown. `Application::~Application()` explicitly
calls `clear()`, so its top-level stack is already protected; this finding does
not claim every application shutdown fails.

**Suggested fix and regression:** call `clear()` from the context destructor,
retaining Application's earlier clear to protect its services. Test ordinary
scope exit with dependent states, not only an explicit call to `clear()`.

### G6-03. Math constants depend on static initialization order

**Source:** [vector2f.cpp:221](../../../engine/math/vector2f.cpp#L221),
[matrix3x2f.cpp:12](../../../engine/math/matrix3x2f.cpp#L12).

The constants invoke out-of-line, non-`constexpr` constructors. Replacing computed
arguments with literals does not make those objects constant-initialized.

**Reproduction:** a separate client translation unit defines namespace-scope
copies of `Vector2F::DIRECTION_RIGHT`, `DIRECTION_UP_RIGHT`, and
`Matrix3x2F::identity`. A Debug probe linked with the client initializer first
observed:

```text
direction at startup: (0,0), after startup: (1,0)
diagonal at startup: (0,0), after startup: (0.707107,-0.707107)
identity at startup: (0,0), after startup: (1,1)
```

The identity output prints its two diagonal elements. This demonstrates one
permitted initialization ordering; failure need not occur under every link order
or optimization setting.

**Impact:** client globals can permanently capture zero directions or a zero
transform without an error. Use `constexpr` constructors where appropriate and
enforce constant initialization of the definitions with `constinit`. Retain a
cross-translation-unit regression or equivalent compile-time guarantee.

### G6-04. Transformed sprites and labels can be culled while visible

**Source:** [Visual::bounds](../../../engine/render/visual.cpp#L48),
[TextObject::text_bounds_at](../../../engine/render/text_object.cpp#L98),
[Scene culling](../../../engine/scene/scene.cpp#L236).

`Visual` reports the destination rectangle without accounting for rotation or
origin. Text bounds account for origin and scale but omit rotation. Those boxes
do not satisfy the `GameObject` contract of enclosing the drawn world extent.

**Reproduction:** a null-backend probe compared direct draw vertices with actual
`Scene` submissions into a viewport spanning x=0-90:

| Object | Reported x extent | Drawn x extent | Scene submissions |
|---|---|---|---|
| Visual at x=100, width 20, source width/origin both 8 texels | 100-120 | 80-100 | 0 |
| Visual at (100,20), size 20x20, rotation pi | 100-120 | 80-100 | 0 |
| Label at (100,20), measured size 20x20, rotation pi | 100-120 | 80-100 | 0 |

**Impact:** visible objects disappear at camera boundaries on every backend.
Derive conservative transformed bounds, including authored sprite-frame origins
and caller origins. Regress through `Scene::draw`, checking that visible geometry
is submitted. Rendering changes should receive the repository's five-backend
verification.

### G6-05. Segment queries miss crossings through polygon corners

**Source:** [segment_rectangle_rotated_intersect](../../../engine/math/intersects.cpp#L470),
[triangle_segment_intersect](../../../engine/math/intersects.cpp#L302).

The polygon routines test endpoint containment, then use a segment predicate
that accepts only proper edge crossings. A crossing exactly at an edge endpoint
is excluded.

**Reproduction:** the segment `(-5,-5) -> (15,15)` crosses the interior of the
square `(0,0), (10,0), (10,10), (0,10)` through opposite vertices. Both
`quad_segment_intersect` and `segment_rectangle_rotated_intersect` return false.
This is an interior traversal, not merely a disputed touching boundary.

**Impact:** public math queries used for selection or visibility can miss an
intersection. Scene collision uses an independent SAT route and is not implicated
by this probe. Use an appropriate closed polygon intersection/clipping operation;
preserve the separately documented semantics of the proper-crossing primitive.
Test opposite-corner traversal and reversed segment direction.

### G6-06. Restore can discard the new window dimensions

**Source:** [WM_SIZE handling](../../../engine/app/window.cpp#L399).

The branch clearing `minimized_` calls `on_resuming()` and excludes the adjacent
branch that delivers `on_window_size_changed()`.

**Reproduction:** send a hidden window `WM_SIZE/SIZE_MINIMIZED`, followed by
`WM_SIZE/SIZE_MAXIMIZED` carrying 1024x768. The observer reports
`suspends=1 resumes=1 resize_callbacks=0`. This was a message-dispatch probe,
not a manual desktop minimize/maximize exercise.

**Impact:** restoring into a different size can leave the layout manager and
rendering dimensions stale. Resume handling should also deliver the valid restored
client size. Add a window-message regression asserting both notifications.

### G6-07. Pause input leaks into resumed gameplay

**Source:** [pause confirmation](../../../samples/linesweeper/states/pause_state.cpp#L203),
[gameplay bindings](../../../samples/linesweeper/states/play_state.cpp#L49),
[simulation input](../../../samples/linesweeper/states/play_state.cpp#L279).

Space confirms Resume and also means hard drop. The pause screen observes a
press; gameplay subsequently observes the still-held key. `World::input` retains
the last simulation input from before the pause; the resumed tick copies that
into `previous_input`, so this is a fresh gameplay action.

**Reproduction:** pause with Space previously up, confirm Resume with Space, and
keep it held through the next gameplay tick. Source inspection establishes the
input route; a headless rules probe of that resumed input changed
`piece=5 score=0` to `piece=0 score=38`. Gamepad A confirmation similarly reaches
clockwise rotation; B cancellation reaches anticlockwise rotation. These
controller paths were inspected in source, not exercised with a physical
controller.

**Suggested fix and regression:** gate gameplay buttons consumed by the menu until
release, with a deliberate policy for restart as well as resume. Test the state
transition and subsequent input, retaining held-input semantics inside the
deterministic simulation.

### G6-08. Pause initialization does not suppress held navigation

**Source:** [pause initialization](../../../samples/linesweeper/states/pause_state.cpp#L172),
[DirectionRepeat](../../../engine/input/direction.cpp#L68).

The sample calls `repeat_.reset()` to prevent an already-held direction moving the
cursor when the menu opens. Reset actually clears the remembered direction, so
the next held direction is treated as a new press. The helper follows its own
tested semantics; the sample's assumption is wrong.

**Reproduction:** `reset(); update(Direction::down, 1.0f / 60)` immediately returns
`Direction::down`. Holding Down while opening the menu can move selection from
Resume to Restart on its first update.

**Suggested fix and regression:** explicitly suppress the opening direction until
neutral, or establish an intentional repeat-state initialization policy. Test
menu entry with a direction already held.

### G6-09. Sample asset paths depend on the working directory

**Source:** [minimal/main.cpp:44](../../../samples/minimal/main.cpp#L44),
[linesweeper/main.cpp:45](../../../samples/linesweeper/main.cpp#L45).

Both samples load `./manifest.json`. The build copies content beside the
executable, but neither entry point resolves an executable-relative content root.
The README's claim that the sample runs from anywhere is therefore false.

**Reproduction:** the actual manifest loader throws
`read_json_file - cannot open './manifest.json'` from the repository directory and
succeeds from the sample executable directory. The probe exercised the loader,
not an interactive launch and dismissal of its error dialog.

**Suggested fix and regression:** define the sample's content-root policy and
resolve both the manifest and its asset directories consistently. Test launch
from an unrelated working directory; finding G6-01 also makes startup failure
cleanup part of that test's value.

### G6-10. Targeted builds bypass the architecture checks

**Source:** [check_engine_includes target](../../../CMakeLists.txt#L19),
[check_doc_citations target](../../../CMakeLists.txt#L46).

Both are `ALL` custom targets, but actual library/application targets do not
depend on them. `ALL` schedules a check in the default build, not in every
target-specific build.

**Reproduction:** `ninja -C out/build/x64-debug-null -n LabradorEngine` reports no
work, while the corresponding default-build dry run schedules both checks.

**Impact:** IDE target builds and consumer game-target builds skip the advertised
include boundary check. Normal full builds and the current CI matrix still run
it. Make the applicable checks dependencies of real targets, preserving the
existing top-level-only scope of citation checking. Verify with a small external
consumer targeting its game, including a deliberately forbidden include.

### G6-11. Particles can identify the wrong cleared row

**Source:** [landing reconstruction](../../../samples/linesweeper/presentation/particles.cpp#L90),
[clear handling](../../../samples/linesweeper/presentation/particles.cpp#L242).

The particle field observes a line-count increase, then reconstructs the locked
board from the previous piece's predicted landing. Rotation or movement on the
locking tick can make the actual landing different.

**Reproduction:** start a horizontal I at `(3,5)`. Coordinates are zero-based and
include the two hidden spawn rows. Fill row 20 except columns 3-6; fill row 21
except column 5, leaving all other cells empty. Rotate clockwise and hard-drop
in one tick. The vertical I clears row 21, but the reconstructed horizontal
landing causes 220
particles to emit from row 20. Observed particle bounds were approximately
`y=576.22..603.98`; the actual cleared row occupies `y=604..632`.

This extends the already documented reconstruction limitation from a missed
burst to a burst at the wrong row. Simulation and scoring remain correct.

**Suggested fix and regression:** expose actual cleared rows as a small tick
result separate from persistent `World`, or revise reconstruction to preserve
the actual locking event. Test combined rotation and hard drop. Returning a
small value does not require an event bus or a larger replay state.

## Feature and verification opportunities

These recommendations are separate from the confirmed findings. Existing
documented gaps are identified as such.

1. **A tested external-project starter.** Supply working top-level CMake/presets,
   dependency setup, and content-root handling. Add a CI consumer that uses
   `add_subdirectory` and builds its own target. This tests the adoption path as
   well as the standalone engine and supports the intended beginner-facing API.
2. **Presentation lifecycle coverage.** The
   [pixel harness](../../../tests/render/pixel_tests.cpp#L342) normally omits
   `end_frame()`. Its [presentation case](../../../tests/render/pixel_tests.cpp#L2077)
   reads back first, waiting for GPU work. Add bounded consecutive presented
   frames, resize, and teardown with work outstanding, using D3D12 diagnostics
   and Vulkan synchronization validation. The frames-in-flight coverage gap was
   already documented by earlier reviews.
3. **Replay controls in LineSweeper.** Deterministic rules already provide the
   core mechanism. Recording and replaying inputs, with an explicit rules/file
   version, would make gameplay failures reproducible and demonstrate that
   design to engine users.
4. **Audible sample content and output-device recovery.** A generated or otherwise
   redistributable sound would permit actual loading/playback checks. The
   [XAudio2 backend](../../../engine/audio/xaudio2/audio_device.cpp#L235)
   deliberately ignores `AudioEngine::Update()` failure today. Playback and
   device recovery are useful follow-ups; the null tests cannot establish them.
5. **Frame-time diagnostics and the low-tier measurement.** A lightweight display
   of frame-time distribution, submitted sprites, and dropped particles would
   make performance observable. Complete the already-open low-tier p99 measurement
   in [the backend review status](../backend-equivalence-2/STATUS.md); successful
   complexity benchmarks do not supply that measurement.
6. **Reproducible content preparation.** Document or script DDS and spritefont
   generation so a new client can replace sample assets without reverse-engineering
   their binaries. This is an onboarding improvement, not a call for an editor.

## Suggested implementation sequence

1. Repair native-window and state-stack lifetimes, then make math constants
   reliably initialized: G6-01 through G6-03.
2. Fix the geometric and window-size contracts: G6-04 through G6-06. Pair the
   culling fix with Scene integration tests and the required render matrix.
3. Repair menu transitions, content paths, and target dependencies: G6-07 through
   G6-10. Add sample/consumer smoke coverage around these actual entry points.
4. Resolve the particle event contract, G6-11, then choose feature work from
   demonstrated client needs. GPU lifecycle coverage and the low-tier measurement
   give stronger evidence for existing claims before expanding the engine.

Implementation validation should preserve the existing tests and add the missing
behavioral cases. A passing matrix at the reviewed revision establishes the
baseline; it does not refute the focused reproductions above.
