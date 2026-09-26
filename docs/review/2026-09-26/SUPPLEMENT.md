# Labrador extended project review — 2026-09-26

Reviewed checkout: `0cef6540eb1653557cb5b8db78929a613454729c`.
That commit adds the original review; production source is unchanged from
`03c3b96f58345191a340b7db664401510a09c1d4`. This supplement records a second
review pass, its additional findings, and fresh validation. The original
[report](README.md) remains historical. [STATUS.md](STATUS.md) tracks all
findings; none was implemented during this review.

## Assessment

Keep the engine's overall architecture. Compile-time platform seams, shared
sprite geometry, null recording backends, the separate game rules library,
strict builds, and common golden images are useful constraints. The review
does not establish a reason for an ECS rewrite, another renderer, or a larger
framework.

The principal weakness is **composition of public contracts**. State callbacks
can outlive their owning state; UI actions can outlive their captured data;
two independently valid scenes cannot always share a frame; reported bounds
can exclude actual geometry. These are concrete failures that broad green
unit-test results do not exclude. Fixing their lifetime and integration
boundaries is more valuable than expanding the feature list.

The original 15 findings remain valid. Thirteen were reproduced again; R26-10
(Vulkan extent validation) and R26-11 (incremental content deployment) were
rechecked in unchanged source against their retained reproductions. Eight
additional entries below bring the ledger to **23 open findings: one P1,
18 P2, and four P3**. R26-21 promotes an earlier unverified cleanup candidate
with an ordinary refusal-path reproduction. Counts are an inventory, not a
measure of project quality or a claim of exhaustive coverage.

## Verification

Three independent subsystem lanes reviewed rendering; core/math/collision/scene;
and app/input/audio/assets/UI/samples. A fourth lane reviewed build, benchmark,
and cloud tooling, coordinated execution, challenged interpretations, and
assembled this report. Native builds and probe runs were serialized.

| Fresh build and CTest | Result |
|---|---|
| D3D11 Debug / Release | 14/14 each |
| D3D12 Debug / Release | 14/14 each |
| OpenGL Debug / Release | 14/14 each |
| Vulkan Debug / Release | 14/14 each |
| Null render + null audio Debug / Release | 13/13 each |
| Offline tooling unit tests | 45 passed |

These are **138 passing CTest entries across ten configurations**, including
pixel/golden checks in all eight rasterising configurations. They are not
138 distinct test cases. No golden image was regenerated. All configurations
were rebuilt before their tests; the four nondefault Release configurations
extend the original review's matrix.

The include/citation build checks completed. Existing ambiguous-basename
citation notices remain. A local developer-shell setup emitted a `vswhere`
lookup warning; it did not prevent compiler setup or any build. There were
no production-code edits, cloud launches, commits, or pushes.

Additional Vulkan evidence: two executions of 240 presented frames, two views,
200 sprite runs per frame, and no readbacks. The engine target was 64×64;
each view was 32×64, inside a hidden 128×128 outer window. Both completed
without validation diagnostics. Loader output confirms Khronos validation
was inserted for both instance and device; retained settings request
synchronization validation. This exercises a bounded submission/resource
lifecycle. It is neither performance qualification nor proof of simultaneous
GPU frames in flight.

Sources, exact outputs, and reproduction instructions are in
[EXTENDED_EVIDENCE.md](EXTENDED_EVIDENCE.md). Diagnostic exit code zero means
the probe completed; several probes intentionally demonstrate a defect.

## Additional findings

| ID | Priority | Finding |
|---|---|---|
| R26-16 | P2 | A fullscreen overlay Scene throws over split-screen gameplay |
| R26-17 | P2 | Rebuilding a focus group can destroy an executing action's captures |
| R26-18 | P2 | Content discovery fails for installation paths outside the ANSI code page |
| R26-19 | P2 | Modern suspend notifications bypass application suspend/resume hooks |
| R26-20 | P2 | The tunnelling helper promises safety it cannot establish for diagonal motion |
| R26-21 | P3 | OpenGL leaks texture objects on ordinary unsupported-format refusal |
| R26-22 | P2 | The timing analyzer attributes over-budget work to presentation pacing |
| R26-23 | P3 | A positive accepted frame-rate option can create a zero-step infinite loop |

### R26-16 — Fullscreen overlay over a split-screen Scene throws

**Source:** [scene.cpp](../../../engine/scene/scene.cpp), lines 163–166;
[state_context.cpp](../../../engine/core/state_context.cpp), lines 41–60;
[state.h](../../../engine/core/state.h), lines 36–50.

`StateContext::draw()` draws the underlying state followed by a noncovering
overlay. Every `Scene::draw()` independently calls `set_view_count()` with its
own count. Once a two-view gameplay Scene has recorded sprites, a one-view
fullscreen overlay tries to reduce that count and throws:

```text
Renderer::set_view_count(1) would drop view 1, which has already been drawn into this frame.
```

The null-backend probe uses the actual StateContext/Scene composition. Its
one-view base plus one-view overlay control records both sprites; its two-view
base plus the same overlay throws. All five backends contain the protective
view-count guard; only null was used for this focused reproduction. The
single-view LineSweeper pause menu does not exercise the failure.

**Repair:** define how scenes compose view ranges and submission order within
one frame. Preserve the guard against dropping recorded views; deleting it
would conceal lost drawing. Add a StateContext regression with split gameplay
and a fullscreen overlay, checking both base views and final overlay order.

### R26-17 — Focus-group mutation ends action lifetime before return

**Source:** [button.cpp](../../../engine/ui/button.cpp), lines 22–29;
[focus.cpp](../../../engine/ui/focus.cpp), lines 30, 42, and 101–117.

`FocusGroup::activate()` references a Button inside its vector, and
`Button::activate()` directly invokes its member `std::function`. An action
that clears and rebuilds the menu destroys that callable while it is running.
An `add()` that reallocates the vector can invalidate its storage too.

The probe keeps the group and borrowed widget alive, satisfying the documented
widget lifetime contract. A weak pointer, copied to a local before `clear()`,
observes that the action's captured shared object is destroyed while the action
is still executing. It does not access freed captures. Ordinary subsequent
access to a capture is unsafe. No shipped-sample crash is claimed.

Neither API forbids structural group mutation during an action, and rebuilding
menu choices after selection is a reasonable use. State transition deferral
does not protect this separate container.

**Repair:** preserve the action and its captured state until invocation ends,
or defer structural group mutation until that boundary. Blindly copying the
function before every invocation changes mutable-lambda semantics: changes to
the copy's captured state may be lost. Regression tests should check lifetime,
rebuild behavior, and persistent mutable action state.

### R26-18 — Unicode installation directories still break content loading

**Source:** [content_root.cpp](../../../engine/app/content_root.cpp),
lines 15–39, especially `executable.parent_path().string()` at line 38.

Getting the executable path through `GetModuleFileNameW` preserves its Unicode
characters, but immediately narrowing `std::filesystem::path` back to
`std::string` loses that guarantee. On this machine's Windows ANSI code page
1252, the same executable and manifest work in an ASCII directory and fail
when copied to a directory ending in `unicode-漢`:

```text
control exit=0 unicode exit=1
content discovery threw: No mapping for the Unicode character exists in the target multi-byte code page.
```

The failure precedes manifest loading. This affects normal installation/user
directory names, not malformed content. The sample and benchmark use this
helper. Machines using a UTF-8 ANSI code page were not tested.

**Repair:** choose and enforce one path representation end to end—native wide
filesystem paths, or explicit UTF-8 conversion paired with compatible file
opening. Changing only the discovery call leaves later path narrowing/opening
sites exposed. Keep an ASCII control and an actual Unicode-directory launch
in the regression evidence.

### R26-19 — Actual Windows suspend messages miss the lifecycle hooks

**Source:** [window.cpp](../../../engine/app/window.cpp), lines 522–546;
[application.cpp](../../../engine/app/application.cpp), lines 357–384.

The power-message handler responds to `PBT_APMQUERYSUSPEND` but omits
`PBT_APMSUSPEND`. The former is unsupported starting with Windows Vista;
the latter is the current suspend notification. The subsequent resume branch
only calls `on_resuming()` when the missing suspend path set `in_suspend_`.
See Microsoft's [query-suspend documentation](https://learn.microsoft.com/en-us/windows/win32/power/pbt-apmquerysuspend)
and [suspend notification](https://learn.microsoft.com/en-us/windows/win32/power/pbt-apmsuspend).

A hidden-window probe sends the modern suspend/resume pair through the real
window procedure and observes zero suspend and zero resume callbacks. The
obsolete query/resume pair gives one of each. This is message-path evidence;
the review did not put the user's computer to sleep.

Those callbacks own the application's audio suspension, gamepad/input
reconciliation, state activation notification, and timer reset. Other window
events may independently perform some of that work, but do not make this
power-notification path correct.

**Repair:** handle the supported suspend event and reconcile resume events with
minimized/suspended state. Test modern notification sequences and idempotence;
keep a real sleep/wake validation distinct from injected-message coverage.

### R26-20 — Projected extents are not a general no-tunnelling guarantee

**Source:** [tunnelling.h](../../../engine/collision/tunnelling.h),
lines 8–29 and 38–51.

The public helper calls the sum of extents projected along travel an exact
safe-displacement boundary. A diagonal corner crossing disproves that claim:
one unit square is fixed at `(0,0)`; another moves from `(-1.1,0.8)` to
`(-0.8,1.1)`. They are disjoint at both endpoints and overlap halfway.

```text
displacement=0.424264073 budget=2.82842708 can_tunnel=0
actual overlap before=0 middle=1 after=0
```

The probe uses finite ordinary coordinates and the real narrow phase.
Projected width describes the whole projection, not the contact interval
along a particular off-center trajectory. A shallow corner clip can be much
shorter. There is no production caller in this engine checkout; the demonstrated
defect is a false guarantee offered to clients.

**Repair:** narrow the helper's contract to the geometric assumptions under
which the bound holds, or explicitly present it as a heuristic. Preserve this
counterexample so a broad safety claim cannot return. This finding does not
require adding continuous collision detection or reversing T3.

### R26-21 — OpenGL's format refusal leaks an allocated texture object

**Source:** [GL texture_factory.cpp](../../../engine/render/gl/texture_factory.cpp),
allocation at lines 107–108, throwing format selection at line 149, cleanup
at line 158.

The factory allocates/binds before checking whether it can upload the format.
Engine-supported `b4g4r4a4_unorm` is deliberately unsupported by GL; that normal
refusal throws past cleanup. Three attempts leave three live texture objects,
including after `RenderResources::release_device_resources()`, because none
was transferred into the resource table. The probe queries each object and
then explicitly deletes it.

This promotes the original report's exceptional-cleanup candidate without
requiring allocation failure. It proves object/name leakage until context
destruction, not a measured amount of pixel-storage VRAM. It matters when a
caller catches a failed asset load and keeps the context alive.

**Repair:** perform predictable format validation before allocating, and retain
scoped ownership across all later throwing operations. Check refusal and retry
without accumulating objects. There is no requirement to add B4 format support.

### R26-22 — The analyzer confuses CPU-budget exhaustion with presentation lock

**Source:** [analysis.py](../../../tools/cloud_performance/analysis.py),
lines 88–123 and explanatory output at lines 656–663.

`_presentation_lock()` uses near-zero pacer wait and tail means within 2% of
the frame period. It does not examine where frame time was spent, yet labels
the result as presentation-locked and explains whole-frame time as the period
rather than the work.

An offline synthetic evidence fixture passes the complete analyzer with:

- update alone: 16.75 ms, already above the 16.666667 ms target;
- record/submit: 0.10 ms; begin: 0.03 ms; present: 0.02 ms;
- whole frame: 16.90 ms; frame-start interval: 16.9001 ms;
- negligible pacer wait and consistent increasing lateness.

Both repetitions are classified `locked_to_presentation_cadence`, although
update plus record/submit is 16.85 ms and no begin/present call exceeds the
existing long-call threshold. The full result still contains the correct raw
costs; it is the automatic causal interpretation that is unsound. This
synthetic counterexample does not establish that the historical captures were
misclassified or overturn their separately investigated two-clock ratchet.

**Repair:** distinguish an observed cadence signature from an established
presentation-wait explanation. Examine work and renderer phases, and retain
an ambiguous/work-limited classification when they do not support the claimed
cause. Test CPU-work-dominated and actual wait-dominated tails separately.

### R26-23 — Accepted extreme FPS produces a non-advancing fixed-step loop

**Source:** [application.cpp](../../../engine/app/application.cpp),
lines 36–37 and 147–148; [step_timer.h](../../../engine/core/step_timer.h),
lines 62–68 and 134–138.

`ApplicationOptions::validate()` accepts every positive integer frame rate.
At `target_fps=10000001`, conversion to the timer's 10,000,000 ticks/second
resolution truncates the target interval to zero. The fixed-step loop then
subtracts zero forever and continuously invokes updates with zero elapsed time.

The bounded probe accepts that real option, uses the real timer conversion and
Tick loop, observes three zero-tick callbacks, and throws deliberately to stop.
It does not hang the process. This is an extreme configuration-validation
boundary; the supplied samples' 60 FPS setting is unaffected. Earlier reviews
mentioned directly setting zero; this entry establishes a path through accepted
application options.

**Repair:** reject options whose converted timer interval is zero and enforce
the timer's own positive-period invariant. Test the representability boundary
with bounded callbacks rather than a test that can hang indefinitely.

## Repair order and architectural implications

1. **Lifetime boundaries:** R26-01 first, then R26-17 and R26-02. Preserve
   outermost callback deferral, action state, and explicit borrowing lifetimes.
2. **Frame composition and geometry:** R26-16, then R26-03/04/06/14 together
   around a documented culling/bounds contract. Use actual emitted geometry
   and nonidentity cameras in integration regressions.
3. **Application boundaries:** R26-18/19 and R26-07/08. Exercise Unicode paths,
   actual window notifications, and input-state cancellation separately from
   device snapshot tests.
4. **Numeric, collision, and content validation:** R26-05/09/10/13/15/20/23.
   Separate finite supported-geometry defects from malformed or extreme inputs.
5. **Tooling and cleanup:** R26-11/12/22/21. Fix content deployment and local
   launch refusal; keep performance interpretation tied to actual measured
   phases; make resource ownership survive refusals.

This is an ordering of review findings, not authorization to implement them.
Keep each closure small enough for its contract and regression evidence to be
reviewed independently. The ledger should close in the same commit as a fix,
or record a reasoned rejection/deferment and re-entry condition.

The most useful continuing test improvement is to exercise combinations:
StateContext with Scene, callbacks with owner mutation, geometry with cameras,
and Window messages with input/application state. The existing golden and null
tests complement that work. Offline tooling tests remain absent from CI and
should become a continuing gate; their successful manual run is only a snapshot.

Module dependency discipline remains partly manual. The known unused
`core/game_object.h` include of `render/camera.h` still contradicts the intended
core-to-math-only direction. The build's backend-include check does not enforce
the whole module table. This is a focused cleanup/enforcement decision, not
evidence that every module must become a separate library.

## Remaining limits

No exhaustive allocation-failure campaign, actual device removal, real
sleep/wake, interactive minimize/resize/drag campaign, physical keyboard-layout
test, XAudio2 wave-bank playback/recovery, or named consumer low-tier p99 was
performed. Vulkan's short no-readback probe narrows one evidence gap without
closing the prolonged lifecycle/stress question. The original unresolved
font-bearing and exception-cleanup candidates remain unverified except for the
specific GL refusal path above. ColourWars remains outside this checkout's
review scope.
