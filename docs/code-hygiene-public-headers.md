# Public-header contract audit

SM-45's reviewed inventory for the six starting headers named by
[Code hygiene](code-hygiene.md#sm-45-public-header-contracts-and-rationale).
The source audit is dated 2026-10-28. It changes comments only; the declarations,
executable tokens and implementation remain unchanged. This record distinguishes
caller rules from implementation evidence and does not extend platform support.

The edited declarations retain the contracts a caller needs. Repeated design
arguments and historical claims were removed. No new rationale document beside
the engine is needed: [Architecture](design/ARCHITECTURE.md#modules),
[Philosophy](design/PHILOSOPHY.md) and the existing renderer seam already own
the dependency, policy and concurrent-drawing arguments.

## Keyboard

Header: [keyboard.h](../engine/input/keyboard.h).
Evidence: [keyboard.cpp](../engine/input/keyboard.cpp),
[Application](../engine/app/application.cpp), both window implementations and
[keyboard tests](../tests/input/keyboard_tests.cpp).

| Contract area | Reviewed rule |
|---|---|
| Preconditions | The shell feeds events and calls `poll()` once before readers each frame; games borrow `Application::keyboard()` rather than advancing it. Hand-written snapshots set `focused` for edge queries. Unmapped/bound keys are ignored by feeders and read false. |
| Lifetime | `state()` and `previous_state()` borrow object-owned snapshots whose values change on `poll()`. `typed()` borrows storage until the next poll or destruction; longer-lived text must be copied. |
| Threading | Feeding and polling mutate unsynchronized object storage. Application's event/update path provides their ordering; the header grants no concurrent mutation guarantee. Free edge functions read only their supplied snapshots. |
| Exceptions | No input-domain exception policy is added. Name lookup rejects unknown tokens with an empty optional, not an exception. Accumulated text uses `std::string`; the audit does not promise allocation-free input. |
| Observable limits | Keys represent positions, modifiers combine sides and numpad Enter shares `enter`. Edges require focus on both frames. A press and release between polls has no key edge; typed characters accumulate. Focus loss clears fed keys and pending text, visible at the next poll. Text filters C0 and DEL, replaces invalid Unicode scalar values with U+FFFD and encodes UTF-8. |

The comments keep per-key edge combination distinct from an edge of a combined
held level. They state that raw `KeyboardState::is_down()` tests the stored bit
without reading `focused`; `Keyboard::held()` adds the focus check. The text
channel's existence does not promise platform IME support. This avoids the old
generic claim that every platform's key-state path could deliver composed text.
The replacement rule agrees with
[text_encoding.h](../engine/render/text_encoding.h); T1 supports game-owned
binding policy and T3 supports the stated combined-modifier limit.

## Gamepad snapshots and vibration values

Header: [gamepad.h](../engine/input/gamepad.h).
Evidence: [gamepad.cpp](../engine/input/gamepad.cpp),
[gamepads.cpp](../engine/input/gamepads.cpp),
[vibration_output.h](../engine/input/vibration_output.h), the XInput/web reader
implementations, [gamepad tests](../tests/input/gamepad_tests.cpp) and
[deadzone tests](../tests/input/deadzone_tests.cpp).

| Contract area | Reviewed rule |
|---|---|
| Preconditions | Use named button enumerators and caller-selected trigger thresholds. Edge queries require both snapshots connected. Vibration strengths are finite in [0, 1] and duration is finite in (0, `max_vibration_seconds`]. |
| Lifetime | Snapshots and vibration requests are values with no borrowed backing storage. A new vibration request replaces the slot's previous request; the engine checks expiration while polling. |
| Threading | Free queries and validators use their input values and retain no state. This header introduces no device-threading or asynchronous completion guarantee. |
| Exceptions | `check_vibration()` throws `std::invalid_argument` naming an invalid field rather than clamping it. Pure queries/deadzone arithmetic add no explicit throws. No new exception specification is declared. |
| Observable limits | Stick coordinates use +y down with no backend deadzone. Raw button/trigger queries inspect stored values without checking occupancy. Trigger held uses strict `>`; equality is up. Deadzone mapping preserves sign/direction, includes its boundary in zero, passes through deadzones outside (0, 1), and does not clamp oversized inputs. `submitted` is API acceptance, not measured motor motion; `unavailable` queues nothing. |

The occupancy rule cannot establish pad identity. Replacement within one slot
between snapshots without an observed absence is invisible to both the device
edge rule and connection-edge queries. The comments retain this limitation
without suggesting that `just_connected()` can detect that unobserved event.
Suspension suppresses subsequent button edges only when the reader reports an
absent snapshot; the separate physical-pad acceptance in TODO remains open.
T1 supports game-owned binding, combination and vibration policy; T6 supports
invalid-request errors and expected unavailable output.

## Game objects

Header: [game_object.h](../engine/core/game_object.h).
Evidence: [Scene](../engine/scene/scene.cpp),
[renderer.h](../engine/render/renderer.h) and shared sprite geometry.

| Contract area | Reviewed rule |
|---|---|
| Preconditions | The caller supplies a valid per-view draw list with its camera and viewport set. `cull_bounds()` receives nonnegative world units per pixel and must conservatively cover drawn geometry. |
| Lifetime | The caller owns objects through their concrete use; the base has a virtual destructor. A draw list is borrowed until `Renderer::submit()`. Camera/viewport values read for a view must not be retained. |
| Threading | Updates are serial; several views may draw one object concurrently. Draw is a pure read of shared state with output in its supplied DrawList, keeping per-view tint/flip/offset in locals. Frame-wide choices belong in update. |
| Exceptions | The abstract interface sets no additional exception or recovery policy. No exception specification is changed. |
| Observable limits | `bounds()` reports unsnapped world geometry and is not a visibility flag. Scale-aware cull bounds account for view-pixel quantization while layout/collision keep unsnapped geometry. |

The architecture citation supports the forward declaration: core depends on
math rather than render. The renderer's existing contract supports borrowed
view state and the submission lifetime. Repeated arguments about the module
wall and drawing syntax were removed without moving caller obligations away
from their declarations.

## State context

Header: [state_context.h](../engine/core/state_context.h).
Evidence: [state_context.cpp](../engine/core/state_context.cpp), its inline
typed push/pop templates and
[state-context tests](../tests/core/state_context_tests.cpp).

| Contract area | Reviewed rule |
|---|---|
| Preconditions | Push/transition takes a non-null owned state. `clear()` is immediate and must not run within an executing state's update/init/activation callback. Owners clear states before borrowed services are destroyed. |
| Lifetime | The stack owns states by `unique_ptr`; covered states remain alive. Context is assigned before init. Pop destroys the removed state, resumes the uncovered state, then delivers the result. A frame's result callback survives replacing its state. Clear drops pending operations, destroys top first and delivers no results. |
| Threading | Operations within update/init/activation/drain are deferred through nested calls. This is reentrancy protection, not synchronization; no concurrent mutation guarantee is added. |
| Exceptions | Null push/transition throws `std::invalid_argument`; empty pop and missing/mismatched typed result throw `std::logic_error`. Callback exceptions remain the callback's responsibility. Clear is `noexcept`; a throwing state destructor terminates. |
| Observable limits | Only the top state updates. Drawing starts at the topmost covering state. Adjacent queued transitions coalesce; intervening push/pop retains order. Activation starts true, notifies top first on changes and does not retrospectively call a newly entered state. Clear preserves the last activation value. |

Two existing descriptions were inaccurate: queuing coalesces adjacent
transitions rather than all transitions in an update, and clear does not reset
activation to its initial value. The audit states the existing implementation;
it makes no behavioral choice or change. The structural-types citation supports
stack ownership, suspension and result delivery. The renderer citation supports
placing frame capture after update rather than in concurrent draw.

## UI objects and containers

Header: [widget.h](../engine/ui/widget.h).
Evidence: [widget.cpp](../engine/ui/widget.cpp),
[focus.h](../engine/ui/focus.h), shared geometry and the existing UI tests.

| Contract area | Reviewed rule |
|---|---|
| Preconditions | A child passed to `add_child()` is non-null and remains alive while operations can reach it. An overriding leaf draw checks `hidden()` itself. Games choose authored-space layout and the draw-list camera. |
| Lifetime | Containers borrow children and store names; removal erases the first match without deletion and is a no-op if absent. `children()` returns a list copy whose pointers remain borrowed. Container destruction drops pointers without dereferencing children, allowing child members of a derived container. |
| Threading | The inherited GameObject rule permits concurrent read-only drawing. Mutating the child list, visibility or colours is not given a concurrent-use guarantee. |
| Exceptions | No new exception or null-recovery policy is introduced. Non-null children are a precondition; construction/list copies may allocate, and resource lookup follows the composed Text/TextureObject contracts. |
| Observable limits | Children update in insertion order even when hidden; a hidden container suppresses drawing. Colour reaches hidden children and nested containers; uncolourable objects ignore it. Bounds skip children whose width or height is <= 0 and return zero if none contributes. This is no added validation promise for non-finite dimensions. Leaf classes remain derivable and deletable through the virtual base. |

The Philosophy UI section supports game-owned layout and copyable derived
widgets. The renderer reference supports camera mapping. Neither citation
requires recursive geometry mutation; the comments retain authored-space bounds
and per-view drawing.

## Scalar constants and arithmetic

Header: [scalar.h](../engine/math/scalar.h).
Evidence: [scalar.cpp](../engine/math/scalar.cpp),
[ericson_math.cpp](../engine/math/ericson_math.cpp),
[collision resolution](../engine/collision/resolve.cpp),
[RectangleRotated](../engine/math/rectangle_rotated.cpp) and
[polygon inflation](../engine/math/inflate.cpp).

| Contract area | Reviewed rule |
|---|---|
| Preconditions | Callers choose an absolute tolerance appropriate to their quantity and coordinate range. `clamp()` does not require an ordered range: its min-first behavior for inverted bounds remains explicit. `are_equal()` uses the caller's epsilon directly rather than validating or replacing it. |
| Lifetime | Constants are values; arithmetic functions return values and retain no input references or external storage. |
| Threading | Functions read supplied values, retain no state and share no mutable storage. The audit adds no threading policy or exception specification. |
| Exceptions | The implementation performs arithmetic/comparisons without allocation, validation throws or an explicit exception path. It does not add an error return for inverted bounds, nonpositive epsilon or non-finite floating-point input. |
| Observable limits | Angles use radians. `are_equal()` is strict absolute comparison, componentwise for vectors; a nonpositive epsilon matches nothing and NaN differences are unequal. `clamp()` tests the lower bound first, preserving its documented inverted-range answer. `lerp()` leaves t unclamped and extrapolates outside [0, 1]. `default_epsilon` does not grow with magnitude; at 5000 its addition rounds back to the same float. |

The header retains the distinct purposes and values of
`segment_parallel_epsilon`, resolution's squared-length unit guard and
`min_axis_alignment`. It also retains `RectangleRotated::edges_valid()`'s
relative scaling and the fact that polygon inflation's mitre branch can change
produced vertices. These code references were checked against their definitions,
rather than treated as an interchangeable family of tolerances.

The former assertion that no repository caller reaches
`inflate_convex_polygon()` was false:
[Triangle::inflate()](../engine/math/triangle.cpp) and
[Quad::inflate()](../engine/math/quad.cpp) call it. Removing that assertion
changes neither the inflation algorithm nor its acceptance. The removed test
census and long rationale established no broader precision or world-size
guarantee. The radians references agree with `Vector2F::unit_vec_from_angle()`
and `RectangleRotated::angle`; no degree-conversion API is introduced.
The removed direction for contributors adding new tolerances was design
guidance, not a primitive caller precondition; the header retains each existing
threshold's quantity and purpose rather than a universal ordering across
different quantities.

## Website and verification boundary

All six headers are explicitly published by
[published.ts](../website/src/lib/published.ts), so their public comments appear
in the generated reference. Their declarations and route/symbol names remain
unchanged. The edits preserve the reference reader's `//` comment blocks and
Allman declaration shapes in
[cpp-header.ts](../website/src/lib/cpp-header.ts). No direct `SourceFile` excerpt
in `website/src/` selects any of these headers, so no excerpt boundary text
requires updating.

**Closed 2026-10-28:** stripping comments and whitespace and comparing with
the parent revision preserves every executable token in all six headers.
All five standalone CMake checks pass. Website check reports zero errors or
warnings; all 157 website tests pass and the build produces 143 pages with
valid internal links. The build reports upstream Vite `astro:head-inject`
directive warnings.

The six generated reference pages were inspected as parsed HTML text. Key,
pad, object, state-stack, UI and scalar declarations still render with their
member sections; the retained focus/occupancy rules, vibration ranges,
pure-read drawing rule, immediate-clear distinction, child lifetime and
absolute-tolerance/extrapolation limits appear on their respective pages.
No device, browser, physical-input or performance acceptance follows from
this source audit.
