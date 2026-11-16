# A scriptable pad source beside the configured backend: issue #122

**Decision, 2026-11-16: accept the pad request through the revised
[SM-78](../../product/BACKLOG.md#sm-78).** The
[public issue](https://github.com/matt439/Labrador/issues/122) requires a
consumer's tests to script Gamepads while linking the same configured engine
library as the game. The earlier configured-null scope alone does not meet
that requirement. The additive construction API implements that seam; this
record does not post a tracker response or close the issue.

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

The concrete [GamepadReader](../../../engine/input/gamepad_reader.h) uses its
configured implementation by default, or borrows an explicit
[ScriptedGamepadSource](../../../engine/input/scripted_gamepad_source.h).
[Gamepads](../../../engine/input/gamepads.h) borrows the concrete reader in
either case. `ApplicationOptions::scripted_gamepad_source` selects the same
source for the shell. Scripted construction allocates no platform reader and
performs no physical polling or output; no game action map, global override,
replay file format or per-sprite indirection is added.

The immovable source owns the next snapshots by value and freezes their
sampled copies until the reader samples again. A disconnected sample is
neutral, and a script samples disconnection before reusing a slot. The source
must outlive its one borrowing reader, including moves and teardown, and the
Application when selected there. Moving a reader transfers the source loan;
existing Gamepads borrows still name the old reader object and must be rebuilt.

The script supplies finite, nondecreasing clock seconds, so expiry needs no
sleep. Records use a buffer reserved at source construction; a full buffer
sets `records_overflowed()` while the input/output lifecycle continues.
Fixtures must assert that flag is false before trusting a complete record.
This keeps sampling, suspension and teardown free of recording allocation.

## Verification and migration

A consumer fixture must link the normal configured engine and construct both
normal and scripted readers without a second engine build or duplicate symbol
definitions. Its scripted path covers snapshot/edge behaviour, slot changes,
suspension, recorded output/refusal and teardown, asserting that physical
input/output transports are untouched. Document the replacement for the
link-order workaround and the source lifetime; update public contracts,
CHANGES.md and the affected input/app/consumer checks together.

The [testing guide](../../../website/src/content/docs/docs/guides/testing.mdx#script-pad-input)
shows the replacement for link-order member substitution. Scripted results
do not qualify real controller mapping or rumble; the physical-pad acceptance
remains separately owned.

### Local qualification, 2026-11-16

`x64-debug-null` built and passed `InputTests` and `AppTests` with the
configured XInput input source still present. The scripted cases cover copied
samples, button/connection edges, independent sources, coexistence with a
normal reader, duplicate-borrow refusal, moves, controlled expiry, output
refusal, explicit stop, disconnect, suspension, bounded-record overflow and
teardown. A hidden native-null Application runs its real loop and receives
Win32 activation messages while its scripted source records polling, expiry,
suspension/resume and final stops.

`wasm-debug` built and passed the same two portable suites under Emscripten
6.0.12's Node. The web-specific trap makes `navigator.getGamepads` throw if it
is touched: scripted sampling, output, suspension, expiry and destruction
complete with zero calls. Existing web mapping/vibration tests remain in the
same InputTests executable.

A separate assembled consumer at `C:\dev\pad78` built and passed
`ScriptedPadConsumerTests` once with native `x64-debug` (normal XInput,
D3D11/XAudio2) and once with `wasm-debug` (normal web input, WebGL2/Web Audio).
The fixture links the consumer's one ordinary LabradorEngine and constructs
its shipping reader beside the script. No backend symbols are replaced.
`scripted_pad_fixture.py` adds the same optional proof to CI's existing native
and browser consumer jobs after their baseline checks; the shipped starter
and repository ctest inventory retain their existing entries.

The fixture's two offline preparation/refusal tests and the CMake citation,
include-wall and whitespace checks pass locally. The combined engine/browser
matrix, website gate and hosted CI are separate integration gates. No
physical-pad qualification or public tracker response is claimed here.
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
