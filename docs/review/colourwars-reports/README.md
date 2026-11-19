# ColourWars' reports: the engine's decisions

ColourWars, the engine's one real client, keeps a ledger of API
complaints against Labrador, each backed by the workaround it wrote. On
2026-10-22 fifteen were live and unfiled: 6 and 9 to 22. This folder is the
engine side's answer to each, decided ahead of their filing (`PI-09`) so that
`PI-10` ([STORIES.md](../../product/STORIES.md#pi-10)) has its thinking done
and an implementer can work from it without the ledger.

**The original five decision files were written before their changes were
implemented**, and the table below says what has been since. A file records a decision, its argument in
PHILOSOPHY's vocabulary, the shape of the change, the tests that ship with it,
the CHANGES.md entry it needs and what it lets the client delete. Each report
was assessed against the source at `0c15f34` and then argued against by a
second reviewer. Every verdict survived, and that review's corrections are
folded into the text rather than kept as a separate debate. Line numbers are
as of `0c15f34` and will drift, so recheck a citation before acting on it.

The client's workarounds are described generically on purpose. The ledger's
owner approved only minimal engine-focused disclosure, so no ColourWars path,
name or line appears here. The client's repository has been
public since 3 November 2026; that does not change this boundary, which is the
owner's decision rather than a visibility fact. A review on 5 November 2026
found no report here linking into the client's repository and left them so;
the README and the website's *Why Labrador?* page link the repository itself.

The owner reaffirmed this boundary on 27 October 2026 for `PI-09`'s public
filings. Recheck each report against current master and file an engine-focused
exhibit within that boundary, or withdraw it with a reason. This approval does
not broaden disclosure of the client's ledger.

## Filing recheck, 2026-10-27

A fresh read-only upstream check resolved `master` to
`febff245e7819c622c278426717c1fd45c15a03a`. All fifteen originals in the
decision table below have answers, and the client has closed each one. They
are withdrawn from the public filing queue because those answers already
exist: the table gives each report's individual disposition and implementation
or documentation subject. This includes report 19's request for evidence;
recording that answer did not supply the requested first-frame captures,
which window.md now records separately.
No issue is needed to ask for the same answer again.

Two newer drafts were rechecked at
`1b3120b0424e005a904db090a6695c678bb44d3f` and answered for `PI-09`,
separately from that completed batch:

| # | Engine-focused claim | Answer |
|---|---|---|
| [x] 23 | Resizing a hidden window requests activation. | [Accept and implement](resize.md): *Keep window resizing from requesting activation* adds `SWP_NOACTIVATE` to `resize_client`. The engine's hidden-window regression reproduces foreground activation before the fix in both windowed and fullscreen modes and passes afterwards. |
| [x] 25 | Controller vibration has no public output operation. | [Accept, narrowed](vibration.md): slot-addressed output transport belongs behind the input seam. Implementation is [PI-21](../../product/BACKLOG.md#pi-21): `Gamepads::vibrate` and `stop_vibration` are implemented with headless lifetime tests; timing, effects, preferences and player routing stay game policy. No physical output is claimed yet. |

Both are answered here without a duplicate public issue. Together with the
withdrawal of the original fifteen, this closes `PI-09`'s filing queue.
The records contain only public engine mechanisms and engine-owned evidence,
with no client exhibit content; the client's own ledger remains its own.

## Issues filed on the tracker, 2026-11-05

Four reports arrived as GitHub issues on 5 November 2026, after `PI-09`'s
queue closed, through the route [CONTRIBUTING.md](../../../CONTRIBUTING.md)
gives every consumer. They are numbered by issue, not by the client's ledger.
**The owner selected verdicts on 2026-11-16.** The pad source, typed renderer
refusal, mutable contact counterpart and remaining software-selection policy
have implementation owners below. The Registry name-removal rider is
declined with its lifetime rationale. Selection does not implement those
changes or file their tracker responses: the issue checkboxes stay unticked
until the implementation or recorded answer lands.

| | Issue | Engine-focused claim | Owner selection, 2026-11-16 |
|---|---|---|---|
| [ ] | [#122](https://github.com/matt439/Labrador/issues/122) | Request. A consumer's tests cannot put a chosen `GamepadState` into a slot or see what was asked of a pad without replacing `GamepadReader`'s members at link time; `LABRADOR_INPUT_BACKEND` has no headless choice. A rider asks for a way to remove a name from a `Registry`. | [SM-78](../../product/BACKLOG.md#sm-78) implements `ScriptedGamepadSource` beside the configured backend, selected through reader construction or Application options. [scripted-pad.md](scripted-pad.md) owns the source lifetime, records, qualification and declined Registry rider. Tracker response remains open; local implementation is not a published answer. |
| [ ] | [#123](https://github.com/matt439/Labrador/issues/123) | Request. A game can recognise "no usable graphics device" only by comparing an exception's message, and each render backend words its refusal differently. | [OCA-120](../../product/BACKLOG.md#oca-120) maps renderer startup refusals to the shipped `RenderDeviceUnavailable`, preserving native details, unrelated failure categories and runtime recovery. Ambiguous WebGL null/context-error cases remain ordinary; the category is not exhaustive. [device-unavailable.md](device-unavailable.md) owns the exact branches and receipts. Tracker response remains open. |
| [ ] | [#124](https://github.com/matt439/Labrador/issues/124) | Request. The contact callback's const counterpart prevents a response from changing it without a cast. | [PI-25](../../product/BACKLOG.md#pi-25) implements mutable `other` with deterministic dispatch, both halves once, one pre-response measurement and deferred lifetime rules. [contact.md](contact.md) records the source break, sample and guide migration, and local qualification. Tracker response remains open. |
| [ ] | [#125](https://github.com/matt439/Labrador/issues/125) | Bug. The Direct3D backends report a CPU rasteriser that lacks `DXGI_ADAPTER_FLAG_SOFTWARE` as `RenderDeviceKind::hardware`, and a release build starts on it. | [SM-98](../../product/BACKLOG.md#sm-98) shares the software classifier between selection and reporting, tests default Release refusal and explicitly selects WARP for software CI and the installed starter. [Decision and evidence](device-kind.md). Hosted qualification and tracker response remain pending. |

The workarounds behind #122 to #124 are described in the issues by the
client itself, on this repository's public tracker. The files here restate
only the engine-side claim, as the boundary above requires.

## The files

| File | Reports | What they share |
|---|---|---|
| [window.md](window.md) | 11, 16, 17, 19 | `engine/app/win32/window.cpp`. On Win32 the window is sized by the same call that shows it. |
| [loop.md](loop.md) | 9, 10, 14 | `Application`'s private loop. It stays closed, and only what a client cannot build from public calls is added. |
| [input.md](input.md) | 13, 21 | Both come from the client shipping the first rebinding screen. Names are mechanism, and evaluating bindings is policy. |
| [draw.md](draw.md) | 18, 20, 22 | None changes `GameObject::draw`'s signature or `Scene`'s dispatch. |
| [content.md](content.md) | 6, 12, 15 | Above the audio seam, in `assets/json`, and in the template's CMake. Three independent PRs. |
| [resize.md](resize.md) | 23 | The size-only Win32 path and its hidden-window activation regression. |
| [vibration.md](vibration.md) | 25 | A narrowed output-transport decision; PI-21 owns implementation. |
| [scripted-pad.md](scripted-pad.md) | Issue #122 and its Registry rider | Same-library scripted source beside the configured backend, with explicit borrowed lifetime and bounded records; name forgetting/recycling is declined under Registry's retained-slot contract. |
| [device-unavailable.md](device-unavailable.md) | Issue #123 | OCA-120 selects a typed renderer-startup/device-capability refusal without absorbing unrelated failures or audio policy. |
| [contact.md](contact.md) | Issue #124 | PI-25 implements mutable `other` with tested dispatch/lifetime rules and a documented source migration; the original draft remains dated context. |
| [device-kind.md](device-kind.md) | Issue #125 | Reporting is fixed; SM-98 selects default Release refusal of known software and an explicit software-test opt-in, with qualification open. |

## The decisions

This list covers the ledger's reports; the four tracker issues have their own
table above. This list is kept current. Tick a box in the commit that implements the report
or files its answer, and say there which commit or issue answered it.

| | # | Decision | Size | Answer |
|---|---|---|---|---|
| [x] | [16](window.md) | Accept | S | A hidden window stays hidden and inactive through `set_fullscreen(true/false)`: it is sized with `SetWindowPos` instead of `ShowWindow`. **Implement first**, because it is a contract break (`window.h`'s `visible`). Done: *Keep a hidden window hidden when it changes full screen*. Its first-size-report case found that a hidden windowed window makes no report before the constructor returns, and a hidden full-screen one reports its requested size; `window.h` now says so. |
| [x] | [11](window.md) | Accept, narrowed | S | The Win32 window always swallows a bare `SC_KEYMENU` (F10, or Alt alone), with no option. Alt+Space and Alt+F4 are unchanged. A silent behaviour change. Done: *Let F10 and Alt alone be keys, not the system menu*. No live case: a hidden window never enters the menu loop, so it could not fail. |
| [x] | [17](window.md) | Accept, narrowed | M | A per-monitor-aware process gets the window's own DPI in the frame arithmetic, and a DPI change keeps the client pixel size. No portable DPI API. Needs the owner to set a Windows floor first. Done: *Keep the client size in a per-monitor-aware process*, with the owner's floor of Windows 10 1607 and static imports (G105). |
| [x] | [19](window.md) | Accept, narrowed | — | Answer recorded in `cacf112`; withdrawn from filing as already answered. Captured 2026-11-13 on `samples/minimal`: a visible launch shows the white class brush for 225 to 305 ms before the first present on `x64-release`, and on all four rasterising backends; a hidden launch shown natively after the first frames shows none. Owner selected [OCA-87](../../product/BACKLOG.md#oca-87) on 2026-11-16: show normally visible windows after the first frame, preserve explicitly hidden windows, and qualify every native backend and full-screen startup. Implemented: *Reveal application windows after the first completed frame*. Native-null lifecycle and Application STARTUPINFO tests pass; the five native Debug tracks pass 112/112, and [eight dated native windowed/full-screen client captures](window-19/2026-11-17/README.md) qualify all four rasterisers on the RTX 5080. OCA-87 is closed after the remaining browser/app-matrix and website checks passed in the [combined checkpoint](../../verification/2026-11-17-overnight-checkpoint.md). No public `show()` is added. |
| [x] | [9](loop.md) | Document only | S | No observer hook on `Application`. A root `State` that owns a nested `StateContext` already sees every frame. Document and test that. Capture and routing are declined as game policy. Done: Document the root state that sees every frame |
| [x] | [10](loop.md) | Document only | S | No capture hook. Document the out-of-band frame (`begin_frame`, draw, `submit`, `read_back_buffer`, no `end_frame`), and state that a frame submitted and never presented is legal. Done: State that a frame submitted and never presented is legal |
| [x] | [14](loop.md) | Accept, narrowed | M | `Application::frame_timings()` returns the last completed frame's CPU phase times. GPU time, history and an overlay are declined; `REV#10` stays deferred. Done: Publish the last frame's CPU phase times from Application (story PI-19) |
| [x] | [13](input.md) | Accept | S | Add `key_name(Key)` and `key_from_name(string_view)`, where each token is the enumerator's spelling. A **source break** through ADL for a same-named unqualified call, which the engine's own `tests/app/web/window_page.cpp` has too. The owner chooses `Key::del`'s token and the name. Done: Name every key, so a game can save a binding and read it back (the owner chose `"del"` and kept `key_name`) |
| [x] | [21](input.md) | Decline | XS | Evaluating bindings is the game's action map, and the clients combine input sources by different rules. Replace the now-false "no client has a rebinding screen" reason wherever it is written. Done: Decline report 21 and re-take the reason the engine has no action map |
| [x] | [18](draw.md) | Document only | XS | No `Label::set_visible`. Point `label.h` at `UiText::set_hidden`, and warn that a zero-alpha tint still adds its RGB under the premultiplied blend. Done: "Point Label at UiText::set_hidden, and warn of the zero-alpha tint". |
| [x] | [20](draw.md) | Accept, narrowed | M | Add read-only `DrawList::camera()` and `viewport()`. Done: "Let a drawable read its view's camera and viewport from the DrawList". |
| [x] | [22](draw.md) | Accept, narrowed | M | Add paired `RenderResources::SheetFrame` and `SheetStrip`, which cannot be mismatched. Raw two-handle overloads, the `UiTexture` forms and `std::less<>` are declined. Done: "Re-point sprites and animations by a resolved sheet pair". |
| [x] | [6](content.md) | Accept, narrowed | M | Named sound groups whose volume `SoundBank` multiplies in above the seam. No master bus, fades, ducking, listener or voice cap. No design document settles "an audio bus against T1", so a T1 decline has no passage to cite. Done: "Mix named sound group volumes into every level a bank sends". |
| [x] | [12](content.md) | Split | S | `parse_json(text, source_name)` uses the file reader's parse path. PI-20 now implements copied `JsonValue::keys()` names and strict manifest/sprite-sheet fields, preserving arbitrary game-owned JSON. [Native checks and remaining wasm qualification](../../2026-10-29-strict-asset-keys.md) are separate; source spans remain declined. |
| [x] | [15](content.md) | Document only | XS | No CMake staging helper. The template comment and the assets guide say `copy_directory_if_different` never deletes, and give a list-and-remove recipe. `PI-11` stays separate. Done: "Say the template's content mirror never removes a file". |

Two corrections for whoever writes `PI-10`'s answers. First, the story frames
report 6 as a settled position against T1, but no design document settles it.
Second, its trade-offs forbid resting a decline on "only one client asks", so
report 15's decline rests on the template already being the recipe, not on
the caller count. window.md and content.md also note that STORIES.md cites the public-face
passage at PHILOSOPHY.md:752-757, which has since moved to :792-798; *Decline
report 21 and re-take the reason the engine has no action map* corrected
STORIES.md.
