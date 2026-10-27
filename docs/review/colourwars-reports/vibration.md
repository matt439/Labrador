# Controller vibration: report 25

**Decision, 2026-10-27.** Accept, narrowed to controller output transport.
Implementation is [PI-21](../../product/BACKLOG.md#pi-21). This answer adds no
public API and does not claim that a controller has vibrated. The report is
answered by accepting a bounded mechanism and giving its implementation an
owner, rather than leaving a platform workaround pending.

## What the engine has now

Rechecked at `1b3120b0424e005a904db090a6695c678bb44d3f`:

- [Gamepads](../../../engine/input/gamepads.h) owns current and previous
  input snapshots and exposes polling and queries, with four numbered slots.
  It has no output operation.
- [GamepadReader](../../../engine/input/gamepad_reader.h) exposes sampling,
  reading, suspension and resumption. Both its
  [XInput implementation](../../../engine/input/xinput/gamepad_reader.cpp)
  and its [browser implementation](../../../engine/input/web/gamepad_reader.cpp)
  implement those operations without a vibration output path.
- [Application::gamepads()](../../../engine/app/application.h) already lends
  the game a mutable `Gamepads*`. Output can be reached through that public
  service without exposing a platform handle or requiring another reader.
- The [input tests](../../../tests/input/CMakeLists.txt) cover input values
  and translation, and the [local multiplayer sample](../../../samples/local_multiplayer/README.md)
  demonstrates input routing. Neither supplies vibration coverage or physical
  output evidence.

A caller that needs vibration currently supplies its own platform transport.
This source finding establishes the missing operation, not the behaviour or
capabilities of any physical controller or browser.

## Why it belongs in the engine

[PHILOSOPHY](../../design/PHILOSOPHY.md), T1, separates device mechanism from
the game's decisions. Sending strengths to a connected slot is mechanism.
Which event requests output, its strength and timing, player-to-slot routing,
preferences, accessibility choices, envelopes and mixing concurrent effects
are game policy. Keeping those decisions in the game does not require the
game to own the platform call as well.

T9 puts platform input APIs at the bought edge.
[ARCHITECTURE](../../design/ARCHITECTURE.md#the-tree), *The tree*,
keeps platform calls inside backend folders behind engine-owned interfaces.
A narrow output operation follows that boundary. It does not introduce an
action map, effect library, scheduler, callback system or game event type.

## Implementation boundary

PI-21 supplies slot-addressed output with two independent normalised motor
strengths and an explicit stop. It must settle the operation's lifetime and
result semantics across native and browser backends before publishing its
signature. In particular, a portable contract must not assume that a browser
can hold actuator levels indefinitely. A bounded request or a clearly reported
unsupported capability is preferable to promising output the backend cannot
provide. Accepted submission and completed physical output must not be
described as the same result.

The implementation and its public contract must keep these requirements:

- Slot validation follows `Gamepads`' checked-access convention. Strengths
  must be finite and within `[0, 1]`; invalid values fail loudly rather than
  being silently clamped. Any duration introduced by the chosen contract also
  needs explicit finite bounds and validation.
- A missing controller or unsupported actuator is expected absence, as T6
  distinguishes it from a broken caller contract. The result must let the
  game distinguish unavailable output from an accepted request; ignoring a
  request must not report success.
- Output addresses the same slot the input service reports. A disconnected
  slot must not leave a request waiting to vibrate a later occupant.
- Explicit stop, reader suspension and reader destruction cancel outstanding
  engine requests and attempt to stop the device. Resume and reconnect do not
  replay old requests. Backend teardown makes a best-effort stop without
  throwing, consistent with T6. The game's
  own menus and transitions still decide when to stop an effect while the
  reader remains active.
- Platform types and calls remain in `engine/input/xinput/` or
  `engine/input/web/`. A game does not create a second platform reader, and no
  output request changes the input snapshots or edge calculation.

## Acceptance and evidence

The implementing change ships deterministic headless tests for validation,
slot routing, unavailable output, explicit stop, cancellation and stale-request
suppression. Those tests need an observable transport seam or backend fixture;
testing only the argument helper does not establish that cancellation reaches
the transport. Native and wasm builds must both compile and check their
selected paths. The public reference and CHANGES.md describe the shipped
contract and any unsupported capability in the same change.

Physical acceptance is separate: record the controller, platform, build and
what was felt for each motor and stop, including backgrounding, disconnect,
reconnect and shutdown. Browser output requires its own recorded checks; a
native result does not establish it. No such session is supplied by this
decision or by headless tests. PI-21 must state those limits wherever the
feature is documented.
