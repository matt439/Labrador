# Copied held touch controls, 2026-11-17

OCA-114's implementation is game-owned `TouchButton` and `TouchStick` values
in `samples/components`. Neither reads a device, draws, depends on the other
component or selects an action binding. Their headers define accepted
geometry, stable ownership, half-open activation, outside movement, end and
cancel. The stick clamps radially to length at most one, including extreme
finite inputs. Changing geometry cancels; identical geometry preserves it.

The existing ComponentsGallery contains an internal TouchDemoState page,
entered by F2 or its separate heading at world rectangle `(880,170,280,45)`.
It leaves the original five settings rows, then Binding and Name, in their
existing focus order and 1280 by 720 layout. Name editing, Binding capture
and their exit tick guard entry. Escape, scripted pad B or Back returns;
the original Gallery's quit route remains available outside the child page.

The page owns fixed regions, reverse-draw-order initial target precedence,
fresh-press acceptance and bindings: stick moves the marker, A colours it,
B enlarges it. A busy first hit rejects another finger instead of falling
through. It converts client pixels through its drawn Camera/Viewport before
passing positions to either value. A left mouse button previews one control,
and touch takes priority over that preview. No engine action map, input API,
seventh sample or settings persistence was added.

Focus/capture epochs, suspension and changed camera/viewport cancel all
values. The page then waits for all contacts and the left mouse button to
clear. Return also drops the Gallery's old pointer capture and held-direction
repeat. Cursor cues use the composed cursor API; resume restores the Gallery
cue before its first draw. The game canvas has `touch-action: none`, matching
LineSweeper: otherwise Chromium starts a pan gesture and cancels all fingers
when the stick moves. This changes only that canvas's gesture handling.

Touch snapshots retain no final-up location. A normal Back release therefore
uses the owner's last sampled inside position. A sampled outside move
prevents completion; movement and lift entirely between polls cannot revise
the prior sampled hold. A specific fixture preserves that bounded policy.
Held controls themselves always become neutral on end/cancel.

[The sample instructions](../samples/components/TOUCH.md) give the complete
copy/test recipe and point to the actual page routing source. The new sample
files are absent from the v0.3 starter, but the two copied values use v0.3
math. Updating an engine pin does not replace a game's copies. Copying the
whole development Gallery also requires its newer pointer-hit and cursor
APIs. The Input and components guides make those boundaries explicit.

## Local evidence

The source base was composed Gallery `04a14210`, then cursor feature
`62eb2c95` applied locally as `cfa9ca15`. The latter's additive CHANGES and
browser-window fixture conflicts retained both sides. The cursor helper's
old sample `hit_target` call was adapted to OCA-106's `hit_pointer_target`.
The commit containing this record supplies the touch sources and fixtures.
Builds used MSVC 19.51.36256 and Emscripten 6.0.12 with `--parallel 2`.

- Native Debug null render/audio: built ComponentsGallery and ComponentsTests;
  the existing test entry passed, **118 cases / 5,807 assertions**. Portable
  values account for 14 cases / 1,912 assertions. Hidden Application cases
  cover simultaneous contacts, rejected contacts, outside motion, independent
  cancel, focus loss/return between polls, resize quarantine, mouse takeover,
  Back release/cancel, modal entry guards, return quarantine and repeated
  const drawing. Pads came from ScriptedGamepadSource.
- Optimized wasm Release WebGL2/Web Audio: built ComponentsGallery and
  ComponentsTests. The portable Node entry passed, **85 cases / 3,333
  assertions**. Chromium's existing pointer page passed its extended route:
  simultaneous contacts, stable ownership, outside stick clamping, button
  re-entry, partial release without adoption, individual synthetic
  pointercancel, same-turn blur/focus, resize/fresh-coordinate conversion,
  mouse preview and F2/click/Back/Escape return. Active stick display values
  allow two percentage points for integer client pixels and display rounding;
  cancelled neutral values remain exact.
- The existing main Gallery, text and rebinding Chromium pages and ordinary
  Firefox Gallery page passed on the composed sample: all six Components
  entries passed. Firefox's ordinary route does not exercise this new touch
  script. The composed WebWindowTests target compiled, and its Chromium and
  Firefox entries both passed (2/2).
- A pristine verified v0.3 starter ZIP was extracted into
  `C:\dev\touch114-release\mygame`. The sample files came from the development
  source folder (they are absent from that released ZIP). The documented copies, include
  rewrites and root CMake test block were applied directly; both native Debug
  null and wasm Debug null compiled MyGameTouchTests and passed its one entry
  (14 cases / 1,912 assertions). No game was played in that copy.
- Browser capture route passed and its neutral/three-contact images were
  looked at: the controls, Back, explanatory text and neutral/held marker
  states fit and are legible. Chrome 157 used ANGLE D3D11 on the RTX 5080.
  These are local ignored captures, not golden or published website images.
- `npm ci` and the complete website gate passed: 201 tests, 190 pages, one
  v0.3 release snapshot. Full local lint passed steps 1–12, including tracked
  source enumeration. Step 13 ran 588 offline Python tests with two skips
  and one composed-baseline failure:
  `test_weekly_configurations.WeeklyTests.test_documented_weekly_counts_are_the_authoritative_inventory_counts`.
  The weekly document still says wasm Debug has 44 entries; the composed
  inventory derived 48. This implementation adds no test entries. At that
  checkpoint, aggregate inventory reconciliation and fresh lint were still
  pending.

Receipts and logs live under the isolated worktree's ignored
`out/touch-controls/`: `native-final`, `wasm-components-final`,
`pointer-final`, `wasm-window-final`, `copy-proof`, `capture`, `site-gate` and `lint`.
No ctest entry, CI job or required context was added; the pointer page's
extended route took about ten seconds locally, within its existing timeout.

## Integration checks, 2026-11-20

The retained change was applied to `acafccea`, the v0.4.0 checkout with the
TODO readiness audit. The merged component source list includes the existing
shape-outline sources and tests. The native Debug null gallery and its tests
built with MSVC 19.51.36256, and `ComponentsTests` passed 125 cases and 7,173
assertions. The fixtures exercised ownership, outside motion, normal release,
cancellation and return to the existing settings page.

The wasm Release gallery and WebWindow sample built with Emscripten 6.0.12.
All eight Components and WebWindow entries passed. The Node component entry
passed 89 cases and 4,470 assertions. Chrome 157.0.8103.80 ran the extended
pointer route on ANGLE D3D11 with the RTX 5080. It exercised three concurrent
contacts, outside drag and re-entry, release, individual cancellation and
focus/resize cancellation. Firefox 160.0.1 ran the ordinary gallery and
WebWindow routes. The November 17 copy/test proof remains the v0.3 evidence.

The website gate passed 218 tests, built 196 pages and assembled both release
snapshots. The new local logs are under `out/touch-qualification/` in the
`Lab-touch` worktree.

## Acceptance still outside this implementation

Real touchscreen glass, pen, native injected/physical touch feel, a real
phone's layout/memory and phone browser behavior were not observed. WEB#5 and
SM-13 stay open independently. Chromium CDP contacts and deliberately
dispatched DOM cancellation/focus events qualify their stated synthetic
paths, not real hardware. No live device or browser settings were changed,
and no push, PR, hosted CI dispatch or deployment was performed. The root
integration sweep remains distinct from these sample-focused local gates.
