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
identifies the isolated backend-mapping commits. The current mappings and
test cases are described below.

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

The Direct3D, Vulkan and GL startup paths now share the category for
identified device or context refusals. The existing
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

## Implementation and isolated qualification, 2026-11-17

`RenderDeviceUnavailable` in `engine/render/device_unavailable.h` is the public
category. Its `std::runtime_error` base owns the reason, so old handlers remain
compatible and a narrower handler can run first without parsing English.
The [release guide](../../../website/src/content/docs/docs/guides/windows-release.mdx)
shows both handlers. The constructor, inheritance and lifetime of `what()`
are checked by portable consumer-shaped native and Node cases.

The category is assigned at these explicit startup branches:

| Backend | Typed requirement refusal | Ordinary controls |
| --- | --- | --- |
| Direct3D 11/12 | No acceptable adapter; `DXGI_ERROR_UNSUPPORTED` from device creation; an explicit WARP lookup's `DXGI_ERROR_NOT_FOUND` or `DXGI_ERROR_UNSUPPORTED` | Unexpected enumeration/probe errors, allocation/invalid-argument/SDK failures and recreation outside startup retain their COM/runtime category. Debug fallback does not reinterpret an earlier unrelated failure as capability absence. |
| WGL | Insufficient reported pixel channels/buffering; missing required startup extension or entry point; fresh WGL invalid-version/invalid-profile errors | Invalid HWND, pixel-format installation, bootstrap/make-current failures, interval setter errors and unknown native errors remain ordinary. A stale Windows error cannot turn an unreported failure into a capability refusal. |
| Vulkan | Instance reports missing extension/incompatible driver; zero physical devices; no candidate meeting required API/presentation/timeline capabilities | Instance/enumeration allocation failures, surface-query failures and startup shader creation failures retain their native reason and ordinary runtime category. Automatic device-loss recovery is not startup. |
| WebGL2 | The page has no `WebGL2RenderingContext` API | Missing/noncanvas targets are invalid arguments. Null context results and creation-error events remain ordinary runtime errors with native diagnostics: an actual canvas already bound to 2D can produce both. No additional context is allocated to diagnose a refusal. |
| null | None: it requires no graphics API/device | Existing headless behaviour and caller checks remain. |

This is deliberately not an exhaustive detector of every unavailable-browser
situation. Chrome 157.0.8103.78 actually emits
`webglcontextcreationerror` after a successful 2D canvas binding when WebGL2
is requested. Its event alone cannot justify device-absence classification;
status text is optional and platform-dependent under the
[WebGL specification](https://registry.khronos.org/webgl/specs/latest/2.0/#context-creation),
and [HTML's context modes](https://html.spec.whatwg.org/multipage/canvas.html#dom-canvas-getcontext-dev)
also return null for incompatible existing state. No English parsing is used.

Tests exercise the actual backend branches with scoped process-local API
forwarders. Direct3D's missing-list, real-WARP-only and unexpected enumeration
fixtures run its real selection predicates. Explicit synthetic hardware
metadata reaches only forced device-creation failures (and scripted capability
probes), so those cases run without a physical GPU and cannot report a fake
successful hardware device. Vulkan tests additionally invoke actual recovery;
WGL and browser tests retain shader/content/state exclusions. No host adapter,
policy or system setting is changed by the fixtures.

The focused native Debug checks pass on all five backends; native GL and both
Direct3D pixel suites pass, as do Vulkan pixel/through-post and sample smokes.
Vulkan's required `validate_sync` run reports zero validation errors and zero
synchronization hazards on the RTX 5080 with SDK 1.4.357.0; its known small
dedicated-allocation best-practice warnings remain visible. Wasm Debug GL
passes all six render/pixel/context-loss checks, optimized wasm-null passes
the render/scene/audio cases, and optimized GL passes all five pixel/context
checks and links/packages LineSweeper's shipping ES module. These are local
source/device checks, not a hosted CI, release or new-platform qualification.
The cross-backend verification procedure is in [verification](../../verification.md).

## Integration qualification, 2026-11-20

Direct3D rejects an invalid window before adapter selection. Vulkan tries
other candidates after a presentation-query failure and preserves the
original API failure if no suitable candidate succeeds. The
[combined qualification](../../verification/2026-11-20-todo-integration.md)
records the real startup-path fixtures, all seven render/application tracks
and Vulkan synchronization. SM-98's separate hosted software selection
checks and the public tracker response remain pending.
