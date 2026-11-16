# A scriptable pad source beside the configured backend: issue #122

**Decision, 2026-11-16: accept the pad request through the revised
[SM-78](../../product/BACKLOG.md#sm-78).** The
[public issue](https://github.com/matt439/Labrador/issues/122) requires a
consumer's tests to script Gamepads while linking the same configured engine
library as the game. The earlier configured-null scope alone does not meet
that requirement. Implementation and qualification remain open; this record
does not post a tracker response or close the issue.

## The accepted pad scope

- Make a scripted/recording source available beside the normal configured
  reader in the same LabradorEngine library. Select it explicitly at
  construction, preserving normal reader defaults and the existing snapshot
  and slot contracts.
- Replace neither backend symbols at link time nor process-global pad state.
  State RAII ownership and any borrowed source lifetime: the source must
  remain valid for the reader and Gamepads that use it, including moves and
  teardown.
- A scripted source records suspension/resume and vibration/stop requests,
  supplies chosen slot snapshots and has explicit supported transport outcomes.
  Scripted execution never polls physical pads or sends physical vibration.
  Suspension, reconnect edges, expiry and teardown retain the shipping seam's
  documented behaviour.
- Keep any configured-null coverage complementary. Revised SM-78 owns its
  relationship to the same-library source; merely building a second engine
  with null input does not answer this request.

The current [GamepadReader](../../../engine/input/gamepad_reader.h) is concrete
with one configured implementation, while
[Gamepads](../../../engine/input/gamepads.h) borrows that concrete reader.
That is the missing construction seam. The real client need justifies a
bounded change to that seam; it selects no game action map, global override,
replay file format or per-sprite indirection.

## Verification and migration

A consumer fixture must link the normal configured engine and construct both
normal and scripted readers without a second engine build or duplicate symbol
definitions. Its scripted path covers snapshot/edge behaviour, slot changes,
suspension, recorded output/refusal and teardown, asserting that physical
input/output transports are untouched. Document the replacement for the
link-order workaround and the source lifetime; update public contracts,
CHANGES.md and the affected input/app/consumer checks together.

These are planned checks. Scripted results do not qualify real controller
mapping or rumble; the physical-pad acceptance remains separately owned.

## Registry rider

**Decline name forgetting and slot recycling in the current Registry.** A
name owns its slot for that registry's lifetime so resolved handles survive
resource release and refill. Erasing/reusing slots would change that contract
and could make an old index handle silently name a different resource.

The existing [Registry::add](../../../engine/core/registry.h) can replace a
stored owning value with empty `Storage{}`. That releases the registry's
ownership of the payload; `contains(name)` then answers false and `get()`
throws until the name is refilled. The name/slot mapping remains deliberately
retained. Existing raw resource pointers are borrowed and must not be used
after their payload is released or replaced.

A game-owned catalogue may use a fresh registry lifetime only after retiring
every old handle and borrow, or while retaining the original catalogue owner
for the old handles. [Handle](../../../engine/core/handle.h) is an index and
carries no registry identity or generation: an old handle must not be passed
to a replacement registry. A map/catalogue that needs actual key erasure can
own that policy outside this durable resource table.

Reopen generation-aware removal only with explicit resource ownership,
stale-handle behaviour and catalogue-lifetime requirements. The present
decision adds no remove/clear API and changes no existing handle semantics.

No build, physical-device run or new client acceptance was performed for this
decision. The public record retains the engine-focused disclosure boundary.
