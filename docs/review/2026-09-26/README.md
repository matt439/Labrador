# Labrador project review — 2026-09-26

Reviewed revision: `03c3b96f58345191a340b7db664401510a09c1d4`.

This is a broad review of the engine, its two samples, tests, build, and
performance tooling. Findings describe that revision and remain historical.
[STATUS.md](STATUS.md) is the implementation ledger: update it with evidence
when a finding is fixed, rejected, or deliberately deferred.

## Assessment

The engine's overall structure is worth keeping. The engine/client split,
compile-time backend selection, shared geometry, null recording backends,
strict compiler settings, and golden images provide useful and complementary
constraints. This review does not justify replacing them with a larger framework.

The highest-priority defect is state callback reentrancy: the deferred-operation
mechanism can destroy a state while that state's callback is still running.
The next group concerns relationships between individually tested components:
contact data outliving scene participants, bounds disagreeing with rendered
geometry, window messages disagreeing with the input contract, and build outputs
disagreeing with current content. Several of these survive every existing test.

The useful next investment is integration regressions at those boundaries,
followed by the corresponding small repairs. More isolated accessor tests would
not catch the failures demonstrated here. The design documents' preference for
simple, explicit machinery remains a good guide; it needs stronger evidence at
the places where two such mechanisms meet.

## Scope and verification

Four review lanes covered rendering; core/math/collision/scene;
samples/build/performance tools; and application/assets/audio/input/UI.
Suspicions were checked against current contracts and earlier review fixes.
Focused native probes exercised the real library and public entry points;
independent checks challenged the lifetime, cloud, and input interpretations.
This is substantial targeted coverage, not a claim that every line or failure
mode was exhaustively verified.

The initial working tree was clean. Production source, existing tests, golden
images, and old review documents were not edited. New files in this directory
are the review and its reproduction material. The review implements none of
the proposed fixes.

| Configuration/check | Result |
|---|---|
| `x64-debug-null` build + CTest | 13/13 passed |
| `x64-debug` build + CTest | 14/14 passed |
| `x64-debug-d3d12` build + CTest | 14/14 passed |
| `x64-debug-gl` build + CTest | 14/14 passed |
| `x64-debug-vulkan` build + CTest | 14/14 passed |
| `x64-release` build + CTest | 14/14 passed |
| `python -m unittest discover -s tools/tests -v` | 45 tests passed |
| Focused native probes | Results below; compiled against current source/libraries |
| Isolated incremental-content CMake fixture | Confirmed stale and missing output behavior |
| Offline cloud declaration probe | Confirmed missing `ConsoleUser` accepted |
| Vulkan oversized-texture validation probe | Confirmed `extent-02252`; validation stopped the invalid call |

All four rasterising Debug configurations ran their pixel/golden tests. These
are 83 passing CTest entries across six configurations, not 83 distinct test
cases. The other four Release configurations were not rebuilt in this review.
The include and citation checks completed; the citation checker also emitted
its existing ambiguous-basename notices. Those notices are not semantic proof
that comments remain accurate.

The retained probe sources are under [probes/](probes/); execution guidance and
logs are described in [EVIDENCE.md](EVIDENCE.md). No live cloud resources were
created. No consumer low-tier performance measurement, hardware audio playback,
device-removal campaign, interactive sample playthrough, or prolonged
frames-in-flight stress run was performed. The test timings above are test-run
evidence, not performance qualification.

## Findings

P1 denotes the first repair to make; P2 denotes a correctness or integration
defect; P3 denotes a smaller presentation/content defect. Priority is not a
claim that every trigger occurs in the shipped samples. Malformed-input and
unused-public-API cases are explicitly identified below.

| ID | Priority | Finding |
|---|---|---|
| R26-01 | P1 | Nested activation can destroy a state inside its own callback |
| R26-02 | P2 | Scene exposes contacts after destroying their participants |
| R26-03 | P2 | World-space pixel snapping causes false sprite culls under a camera |
| R26-04 | P2 | UI texture bounds omit rotation and origin |
| R26-05 | P2 | Thin valid polygons lose required separating axes |
| R26-06 | P2 | Rotated-rectangle Visual construction discards orientation |
| R26-07 | P2 | Keyboard translation violates the physical-position contract |
| R26-08 | P2 | Capture loss can leave mouse buttons permanently held |
| R26-09 | P2 | Texture layout overflow defeats file-size validation |
| R26-10 | P2 | Vulkan does not check queried image extent limits |
| R26-11 | P2 | Benchmark content is stale after asset-only rebuilds |
| R26-12 | P2 | Cloud preflight accepts a missing mandatory console user |
| R26-13 | P2 | NaN geometry bypasses promised rejection/checks |
| R26-14 | P3 | Particle bounds omit visible quad edges |
| R26-15 | P3 | JSON string accessors silently truncate embedded NULs |

### R26-01 — Nested activation can destroy a state inside its own callback

**Source:** [state_context.cpp](../../../engine/core/state_context.cpp), lines
159–174; outer update deferral at lines 32–38.

`update()` defers stack operations while invoking the current state. A nested
`notify_activation()` uses the same boolean, then unconditionally clears it and
drains pending operations. A transition/pop can therefore destroy the state
before its outer `update()` returns. Even a nested notification that queues
nothing leaves the outer callback unprotected against its next operation.
The same composition needs checking during `init`, suspend/resume, and result
callbacks inside an existing drain.

The probe observes destruction through separately owned state and prints:

```text
update enters
old state destroyed
update still running; state already destroyed=1
```

It does not dereference a destroyed object. This proves the premature
destruction that makes ordinary subsequent member access unsafe.

The application can enter this composition through synchronous Win32 window
operations and its activation forwarding. The public state API permits stack
changes from activation handlers. A current sample crash was not reproduced:
the supplied samples do not install the probe's stack-changing deactivation
handler. This is a supported API composition defect.

**Repair:** preserve nested deferral scopes and drain only at the outermost safe
boundary. Use a scope guard/depth model that also handles exceptions and nested
drains. Test activation inside both update and init, transitions issued before
and after that notification, and destruction order after every callback returns.

### R26-02 — Scene exposes contacts after destroying their participants

**Source:** [scene.cpp](../../../engine/scene/scene.cpp), lines 97–100 and
120–134; [scene.h](../../../engine/scene/scene.h), the resolve/contacts contract.

`Contact` retains raw participant pointers. `end_tick()` destroys retired
colliders but leaves `contacts_` intact until the next `resolve()`. A fresh
`contacts()` call after the normal resolve/end-tick sequence returns those
stale pointers. A contact overlay drawn afterward could dereference them.

The native probe generated one contact, retired a participant, observed its
destructor, and still read a contact count of one. It did not dereference the
stale pointer. No shipped engine/sample caller currently uses this accessor
after retirement; external ColourWars consumers were outside this review.

**Repair:** define the lifetime explicitly. The minimal option clears contacts
before retirement and expires borrowed spans and copied participant pointers at
`end_tick`; pruning retired
pairs preserves surviving-pair inspection. A post-tick view of retired contacts
requires copied inspection geometry/data. Test both retired and surviving-pair
semantics rather than silently changing what debug clients can inspect.

### R26-03 — World-space snapping causes false sprite culls under a camera

**Source:** [visual.cpp](../../../engine/render/visual.cpp), lines 56–65;
[sprite_geometry.cpp](../../../engine/render/sprite_geometry.cpp), lines 216–240;
consumer [scene.cpp](../../../engine/scene/scene.cpp), lines 219–238.

`Visual` caches bounds by snapping destination edges in world space. Every
backend transforms the destination through the camera first, then snaps in view
pixels. Those operations do not commute.

A destination `(0.1,0.1,0.8,0.8)` has cached bounds `(0,0,0,0)`. A camera at
`(0.2,0.2)` with scale 10 makes the actual corners `(-1,-1)` through `(7,7)`.
The world cull rejects it although part of the sprite is visible. Both the cull
result and emitted null-backend vertices were checked.

The G6-04 repair correctly added origin/rotation to bounds; its world-snapping
assumption introduces this remaining camera interaction. This does not mean
the earlier repair never landed. Its integer/identity-camera tests miss it.

**Repair:** use a conservative world envelope with a justified treatment of
view-space quantization, or compute camera-aware cull bounds. Test through
Scene with fractional destinations, translated/zoomed cameras, rotation, and
authored origins. Simply deleting truncation from one helper needs separate
proof for pivoted edge cases.

### R26-04 — UI texture bounds omit rotation and origin

**Source:** [widget.cpp](../../../engine/ui/widget.cpp), lines 181–185;
`UiTexture` construction/draw in the same file.

The public constructor accepts sprite rotation and origin, and drawing applies
them. `bounds()` returns the unchanged destination rectangle. `UiObject` is a
`GameObject`, so Scene uses that rectangle for culling; UiContainer propagates
it through its union, and focus navigation also reads it.

A `(10,10,20,10)` texture rotated 90 degrees about its default top-left pivot
draws approximately `x=0..10,y=10..30` while reporting `x=10..30,y=10..20`.
The native probe recorded those four corners, then drew through an edge view
`(0,12)..(9,20)`: Scene submitted zero sprites. This verifies the culling
consequence as well as the wrong reported box.

**Repair:** return a conservative drawn extent that accounts for the texture's
authored/caller origin and rotation, and maintain it across geometry/frame
setters. Resolve this alongside R26-03 so UI does not inherit the same
camera-space mistake. Test culling and container unions with transformed UI.

### R26-05 — Thin valid polygons lose required separating axes

**Source:** [narrow_phase.cpp](../../../engine/collision/narrow_phase.cpp),
lines 69–85 and 152–170.

`fill_axes()` skips edges shorter than 0.001 world units, including edges of
nondegenerate shapes. Two thin OBBs can then contribute only the same one of
the two required axes, yielding a false contact along the omitted direction.

The direct query reports contact for axis-aligned thin rectangles separated
by 98 units. More significantly, a diagonal case also survives BroadPhase:
axes `(sqrt(.5),sqrt(.5))` and `(-sqrt(.5),sqrt(.5))`, half extents
`(.0001,1)`, centers zero and `y_axis * 2.0001`. The native full-pipeline probe
reported overlapping AABBs, a positive polygon gap `9.98974e-05`, and one
contact. This is finite valid geometry, although smaller than shipped content.

**Repair:** use RectangleRotated's validated orthonormal axes directly; preserve
required nonzero normals for other supported polygons with stable normalization.
Do not silently discard geometry the public contract accepts. Test both direct
queries and overlapping-AABB/disjoint-polygon cases through `find_contacts`.

### R26-06 — Rotated-rectangle Visual construction discards orientation

**Source:** [visual.cpp](../../../engine/render/visual.cpp), lines 27–40;
[rectangle_rotated.cpp](../../../engine/math/rectangle_rotated.cpp), lines 280–282.

The RectangleRotated overload converts the shape to its unrotated rectangle
and never transfers its axes/angle. Its separate rotation argument defaults to
zero. A shape centered at `(50,50)`, axes `(0,1),(-1,0)`, half extents `(20,10)`
should occupy `(40,30,20,40)`; the actual Visual occupies `(30,40,40,20)`.
Recorded vertices confirm the lost orientation.

This is a public-API defect; no supplied sample uses this overload. Its contract
does not disclose that orientation is intentionally discarded.

**Repair:** define how shape orientation combines with optional sprite rotation
and authored origin, then preserve the supplied geometry. Copying only the angle
is insufficient because the shape rotates about its center while the default
sprite pivot is top-left. Test non-square shapes at cardinal and oblique angles.

### R26-07 — Keyboard translation violates the physical-position contract

**Source:** [window.cpp](../../../engine/app/window.cpp), lines 20–38 and
561–574; [keyboard.h](../../../engine/input/keyboard.h), the `Key` contract.

`Key` promises US-layout physical positions independent of the active layout.
The translator reads only `wParam`'s virtual-key code and ignores the scan code
in `lParam`. A French-layout message for the physical US-W position
(`scan=0x11`, `VK_Z`) becomes `Key::z`, not `Key::w`. The hidden-window message
probe confirms that result.

Windows maps scan codes through the active layout before producing virtual-key
codes; they are not interchangeable physical identifiers. See Microsoft's
[keyboard input overview](https://learn.microsoft.com/en-us/windows/win32/inputdev/about-keyboard-input).
This probe injected the documented message representation; it did not change
the user's active keyboard layout or replay physical keystrokes.

**Repair:** translate physical keys from scan/extended-key information while
keeping typed characters on WM_CHAR. Test AZERTY/QWERTZ message cases, numpad
distinctions, and extended keys. Alternatively change the public contract and
client bindings deliberately; leaving the current promise is incorrect.

### R26-08 — Capture loss can leave mouse buttons permanently held

**Source:** [window.cpp](../../../engine/app/window.cpp), lines 698–707;
[mouse.cpp](../../../engine/input/mouse.cpp), button/focus state handling.

WM_CAPTURECHANGED resets the window's button counter but does not release the
Mouse's held bits or tell it capture was lost. Capture can move without
WM_ACTIVATEAPP deactivation, for example to another window/dialog in the same
application. The eventual physical button-up can go elsewhere.

A hidden-window probe sent left-down, acquired capture, then called
`ReleaseCapture()`. After the real capture-change notification and a poll,
capture was absent but `held(left)==true` and focus remained true. Microsoft's
[capture notification contract](https://learn.microsoft.com/en-us/windows/win32/inputdev/wm-capturechanged)
does not imply application deactivation.

**Repair:** explicitly reconcile/cancel button state when capture is lost.
Keep the normal release-after-last-button-up case harmless. Test same-app
capture transfer and cancellation independently from application focus loss.

### R26-09 — Texture layout overflow defeats file-size validation

**Source:** [texture_data.cpp](../../../engine/render/texture_data.cpp),
lines 47–59; DDS entry point [dds_file.cpp](../../../engine/render/dds_file.cpp),
lines 194–201 and 251–272.

Stride is calculated using signed `int` multiplication before conversion to
`size_t`; compressed layouts also use unchecked `width+3`/`height+3`.
A 132-byte DDS declaring RGBA width `0x40000001`, height 1, and four payload
bytes is accepted by the real reader with `stride=4,size=4`. Signed overflow
has already occurred before the payload check. The spritefont reader shares
the arithmetic helper.

This is malformed-content robustness, not a demonstrated remote attack or a
failure of the shipped assets. A backend's later size refusal does not make
the decoder's undefined arithmetic valid.

**Repair:** checked layout arithmetic in an adequate unsigned type, explicit
representability limits for integer fields, and checked accumulated sizes.
Test real DDS/spritefont inputs at each overflow boundary with named errors.

### R26-10 — Vulkan does not check queried image extent limits

**Source:** [texture_factory.cpp](../../../engine/render/vulkan/texture_factory.cpp),
lines 177–201.

The factory queries `vkGetPhysicalDeviceImageFormatProperties` but checks only
`maxMipLevels`. The query receives the format/type/usage tuple, not the requested
dimensions. Width and height are then passed unchecked to `vkCreateImage`.
A valid one-row atlas slightly wider than `maxExtent.width` reaches invalid
Vulkan usage without needing the malformed layout in R26-09.

Khronos requires image extents to fit the returned creation limits:
[VkImageCreateInfo, extent-02252/02253](https://docs.vulkan.org/refpages/latest/refpages/source/VkImageCreateInfo.html).
Driver error handling after invalid usage is not the validation contract.

The bounded native probe matched the selected NVIDIA GeForce RTX 5080 to its
queried RGBA limit of 32,768 pixels. An atlas of 32,769 by 1 pixels (131,076
bytes) produced `VUID-VkImageCreateInfo-extent-02252`. The layer's FAIL action
stopped the invalid call before the driver; the factory threw only afterward
with `VkResult -1000011001`.

**Repair:** compare all requested extent components with the queried limits
before image creation and preserve the asset name/dimensions in the error.
Exercise an oversized, small-payload texture with validation enabled.
This validates the missing check without relying on a driver's response to
invalid usage. See EVIDENCE for the exact probe setup.

### R26-11 — Benchmark content is stale after asset-only rebuilds

**Source:** [bench/CMakeLists.txt](../../../bench/CMakeLists.txt), lines 66–75.

The frame benchmark copies font/texture assets only in POST_BUILD. They are
not dependencies of the executable link edge. Changing only an asset and
rebuilding the benchmark leaves old bytes beside it; deleting a deployed asset
does not restore it when nothing needs relinking. The sample's always-run
content target already handles this correctly.

Generated Ninja dependencies confirm the actual target's gap. An isolated
fixture using the same pattern built version-one content, changed only its
source to version two, and rebuilt: deployed content stayed version one.
Deleting that fixture's deployed asset and rebuilding left it missing.
No real project asset was changed or removed for this check.

The documented cloud staging procedure copies these deployed assets. Hashing
source and payload separately preserves the mismatch rather than preventing
it. Unchanged-content runs are unaffected.

**Repair:** a dependent content target or correctly declared copy outputs and
dependencies. Test content-only changes and missing deployed outputs, not just
clean builds.

### R26-12 — Cloud preflight accepts a missing mandatory console user

**Source:** [common.py](../../../tools/cloud_performance/common.py), lines
188–197; [worker.ps1](../../../tools/cloud_performance/worker.ps1), lines
169–197 and 501 onward.

`ConsoleUser` is mandatory in the worker but absent from the locally required
AMI tags. Omitting it passes configuration validation and authorized template
generation, which can create the paid instance before the worker refuses it.
An offline probe generated an EC2 Runner template with `ConsoleUser=None`.

That refusal occurs before `$declaredInstance` becomes true, so the worker's
normal immediate shutdown path is skipped; the deadline watchdog remains.
The example configuration and both retained runs include the tag. This finding
does not invalidate those runs or claim that a cloud instance was launched
during this review.

**Repair:** require a non-whitespace ConsoleUser locally and verify its AMI tag.
Set shutdown ownership after instance identity is established, before later
environment checks that can fail. Test omissions/whitespace as local refusals
and stub early worker failures to check cleanup after ownership is known.

### R26-13 — NaN geometry bypasses promised rejection/checks

**Source:** [narrow_phase.cpp](../../../engine/collision/narrow_phase.cpp),
lines 183–216; [resolve.cpp](../../../engine/collision/resolve.cpp), lines
18–31; circle/AABB routines in
[ericson_math.cpp](../../../engine/math/ericson_math.cpp).

Three related probes expose a boundary-validation gap:

- A rectangle `(5,5,NaN,10)` tested against `(0,0,20,20)` produces a contact with
  finite penetration 5. `signed_area != 0` accepts NaN; min/max reductions can
  discard poisoned later vertices.
- `separation_along((NaN,1),1,(0,1))` passes the promised unit-vector check
  and returns nonfinite movement because its rejecting comparison is false.
- A NaN-centered circle returns true from boolean rectangle-circle intersection
  and false from the overload that also returns a closest point.

These require invalid numeric data and rank below finite-shape defects. They
are grouped rather than counted as three normal-gameplay failures.

**Repair:** choose where nonfinite geometry is rejected, use positive acceptance
tests in validators, and align overload semantics. Test contained poisoned
shapes and each coordinate/extent position; existing boundary-only cases can
pass for unrelated geometric reasons.

### R26-14 — Particle bounds omit visible quad edges

**Source:** [particles.cpp](../../../samples/linesweeper/presentation/particles.cpp),
lines 175–200 and 383–394.

Bounds take extrema of particle centers; drawing emits finite squares around
them. The native probe measured bounds `x=500.969..527.041` and actual vertices
`x=497..530`. A Scene view spanning `x=497..498` intersected drawn geometry yet
submitted zero sprites. The ordinary fullscreen symptom is a disappearing
edge of the last outgoing particles, so this is P3.

**Repair:** include each currently rendered half-size and conservatively handle
edge quantization. Test every emitted quad against bounds and retain a visible
particle in an edge view. Keep emission-center reconstruction tests distinct
from rendered-extent tests.

### R26-15 — JSON string accessors silently truncate embedded NULs

**Source:** [json.cpp](../../../engine/assets/json.cpp), lines 169 and 249.

Both string accessors construct `std::string` from `GetString()` without
`GetStringLength()`. Valid JSON `"asset\u0000other"` therefore returns five
bytes rather than eleven. This silently changes data and can make different
asset identifiers alias. The actual reader/accessor probe confirmed it.

**Repair:** preserve the parsed string length in the generic accessor; reject
embedded NUL explicitly with contextual errors where identifiers/file paths
cannot support it. Test both object and array string access. The same probe
also showed `1e100` narrowing to infinity; finite-number rejection is a separate
contract decision, not implied solely by this string defect.

## Lower-priority hardening and unverified candidates

These are retained for follow-up without inflating the main defect list.

- **Modifier aggregation:** the translator maps left/right Shift, Control and
  Alt to a single bit. A synthetic down/down/up sequence clears it while the
  other side remains logically down. The Shift-specific Windows behavior also
  requires care: DirectXTK contains a simultaneous-Shift workaround. Verify
  real Control/Alt message behavior before claiming a reproduced hardware bug;
  side-aware internal bookkeeping may be needed even with one public bit.
- **Table insertion exception safety:** `name_table.h:46–47` and
  `registry.h:73–75` publish the map index before storage append. A throwing
  custom element probe left the failed name resolving to the next element.
  Current production element moves do not throw, so their trigger is allocation
  failure. Use transactional insertion/rollback when addressing this P3 risk.
- **Exceptional cleanup:** a partially submitted Scene draw needs a completion
  guard if later task submission throws. GL texture creation and Vulkan buffer
  creation also have raw-handle windows before later throwing steps. These were
  source-inspected, without OOM/native-resource failure injection; no routine
  frame crash or measured leak is claimed.
- **Font bearings:** negative glyph y offsets may place ink outside reported
  text bounds. No real supported font plus complete Scene reproduction was
  established, so this remains a candidate.

## Architecture, maintenance, and test priorities

1. **Keep the existing seams and repair lifetime composition first.** The state
   model does not need a new event framework to preserve nested deferral.
   Scene contact inspection does need an explicit borrowing boundary.
2. **Treat bounds as an integration contract.** Check actual emitted geometry
   through Scene under several cameras. Visual, UiTexture, and particles each
   demonstrate why plausible local boxes are not sufficient.
3. **Bring offline tooling tests into CI.** `tools/tests` has meaningful
   declaration, evidence, and trace-cleanup checks, but neither CTest nor the
   current CI workflow runs them. Their passing manual run is not a continuing
   regression gate.
4. **Strengthen numeric and asset boundaries.** Add compact malformed-file and
   nonfinite-geometry cases at public entry points. The tested DDS overflow and
   thin-shape cases warrant behavioral tests rather than implementation mirrors.
5. **Reconcile documentation when touching code.** The design documents remain
   useful. Some local comments overstate enforcement or preserve obsolete
   history. For example, `core/game_object.h:4` includes `render/camera.h` despite
   its math-only dependency discussion, although the header needs no Camera.
   The backend include check does not enforce the full module dependency table.
   Remove that unused dependency as a small separate cleanup; do not mistake
   green citation/include checks for a complete architecture audit.

The public API review did not establish a need for action mapping, an editor,
scripting, online play, a sixth renderer, or a new ownership framework. Those
would need real client requirements under T1. Physical-key correctness is
independent of whether an action-mapping layer is ever added.

## Historical findings and remaining evidence limits

The old window destruction, state destruction ordering, static math constant,
segment/corner intersection, sample pause-input, and particle row reconstruction
repairs are present. They were not copied into this review as fresh defects.
Likewise, inspected D3D12 live-device wait refusal and Vulkan submitted-layout
tracking match their prior fixes; no new device-loss release-order defect was
confirmed.

The performance ratchet is already explained by
[the current analysis](../../performance/2026-09-19-g6f-ratchet.md). Its two-clock
limitation is not new evidence against renderer throughput. The declared
consumer low tier remains unmeasured, and old EC2 results do not fill that gap.

Important coverage still absent after this review: prolonged rendering without
per-frame readback synchronization, actual device removal, live
minimize/restore/drag sequences, physical input across layouts and device changes,
real XAudio2 playback/recovery with a wave bank, and the named low-tier p99.
The existing live Application layout/cache integration gap also remains.
Passing the current test matrix does not settle these questions.
