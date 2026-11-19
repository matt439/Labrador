# A typed renderer-unavailable failure: issue #123

**Decision, 2026-11-16: accept as
[OCA-120](../../product/BACKLOG.md#oca-120).** The
[public request](https://github.com/matt439/Labrador/issues/123) identifies a
consumer that must distinguish a renderer's startup refusal without matching
English message text. One public renderer-unavailable exception category,
derived from `std::runtime_error`, is selected.

**Released in v0.4.0.** The [public header](../../../engine/render/device_unavailable.h)
defines `RenderDeviceUnavailable`. The [category tests](../../../tests/render/device_unavailable_tests.cpp)
throw it directly and check the narrow and base catches, retained message and
handling of an unrelated error. The released backends still use their existing
startup errors. The [November 17 handoff](../../2026-11-17-overnight-handoff.md#completed-isolated-scopes-awaiting-integration)
identifies the isolated backend-mapping commits. Integrating them, exercising
the backend refusal paths and verifying the consumer catch remain open.

## Accepted scope

- Use the same category across rendering backends for genuine startup/device
  capability unavailability: the selected backend cannot create a usable
  device or its required context/capabilities on the available system.
- Preserve useful backend messages and native error details. Existing
  `std::runtime_error` catches keep working; consumers may catch the narrower
  category and choose their own log reason and player-facing message.
- Do not relabel malformed assets, authored-shader errors, invalid arguments,
  programming errors or unrelated API failures as device absence. The category
  does not prove that a machine physically lacks a GPU: it describes the
  selected renderer's unmet requirements.
- Keep audio absence and existing runtime device-loss/recovery policy outside
  this startup classification slice. A backend not built remains a build/link
  concern rather than a new runtime backend-selection framework.

The released Direct3D, Vulkan and GL startup paths still need to emit the
common category for their device or context refusals. The existing
[COM exception helper](../../../engine/render/throw_if_failed.h) describes a
failed native call and is broader than this selected semantic failure. It
does not justify classifying every failed call as renderer unavailability.

## Verification and migration

The implementation identifies each backend's genuine unavailable-device or
capability branch and tests that it emits the common category with its useful
details retained. Controls keep bad assets, caller errors and shader failures
distinct. A consumer fixture demonstrates a typed catch without text matching,
and broad runtime-error catches remain compatible.

Coordinate [SM-98](../../product/BACKLOG.md#sm-98)'s ordinary Release
known-software refusal with this category, retaining default-refusal and
explicit software-test opt-in coverage. Update the public header/reference,
CHANGES.md, the release guide and affected backend/consumer checks together;
keep source, device and unrun acceptance evidence distinct.
