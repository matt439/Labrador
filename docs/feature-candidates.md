# Feature candidates

These proposals supplement the [2026-10-27 comparison](survey/2026-10-27.md).
They are based on this checkout, not on claims about other engines. Their
current selection state is indexed in [TODO.md](../TODO.md); none is an
implementation commitment. Select one, then give it a story and a place in
[BACKLOG.md](product/BACKLOG.md) before starting it.

The ready work is already substantial: browser contracts and bundle tooling,
couch joining, working settings, pointer menus, localisation and wrapping,
asset validation, and the new layer check and animation example. Those
stories should not wait for a decision on these proposals.

## Selection order

Start with typed text, rebinding or save recovery if the next goal is a new
copyable capability. Choose camera following for a gameplay example, or the
grid-sheet tool for content authoring. Each stays in tools or sample-owned
code; promoting a mechanism into the engine still needs the real-client
argument required by PHILOSOPHY T1.

| id | proposal | priority | size | prerequisite |
|---|---|---|---|---|
| `IDEA#1` | A single-line typed-text component | could | M | Select its client and supported text scope; coordinate with OCA-42 |
| `IDEA#2` | A game-owned keyboard rebinding screen | could | M | Select the sample and its duplicate-binding policy |
| `IDEA#3` | Versioned saves with recovery | could | M | Select the example's save format and recovery policy; builds on OCA-53 |
| `IDEA#4` | A bounded, dead-zone camera-follow component | could | S | Select the local-multiplayer example's follow behavior |
| `IDEA#5` | A grid sprite-sheet authoring command | could | S | Select the input description; builds on the existing sheet schema |
| `IDEA#6` | Pointer interaction in the components gallery | could | S | OCA-50's guide first, or explicitly amend that story to include this extension |

Sizes use BACKLOG's rough grades, not delivery dates. The acceptance below is
the proposed bound to carry into a selected story, not a final public API.

## IDEA#1. A single-line typed-text component

**Present evidence:** [Keyboard::typed](../engine/input/keyboard.h) supplies
UTF-8 text, while the [components gallery](../samples/components/README.md)
has no editable text example. Text rendering's language limits are stated in
[font.h](../engine/render/font.h); a text field does not remove them.

**Proposed slice:** a copyable name/label field in the gallery, driven by the
page's input wiring. Start with append, backspace, confirm and cancel. Define
a length/width limit and focus-loss behavior. Keep selection, clipboard,
mobile soft-keyboard support and a new IME composition API outside this slice.

**Done when:** headless tests hold UTF-8 code-point deletion, empty and full
fields, confirm/cancel, and input arriving while unfocused; the gallery shows
accepted and unsupported glyph behavior. Native and browser page runs exercise
the field with its documented limits. Coordinate shared text examples with
OCA-42; this is interaction, not another localisation story.

## IDEA#2. A game-owned keyboard rebinding screen

**Present evidence:** [keyboard.h](../engine/input/keyboard.h) exposes stable
`key_name`/`key_from_name` tokens, and [LineSweeper's state](../samples/linesweeper/states/play_state.cpp)
owns its binding tables. The engine supplies device state; games own actions
and rebinding. No engine action map is proposed.

**Proposed slice:** a copyable capture row and a small sample-owned binding
table. Choose replace, swap or refuse for duplicates. Include reset and cancel;
persistence is a follow-up unless explicitly selected with IDEA#3.

**Done when:** tests cover ignoring the opener until it is released, capture,
duplicates, cancel/reset, invalid loaded tokens and focus-loss cancellation.
The example still accepts navigation after every failed or cancelled capture.
A native/browser run exercises it; synthetic key input is identified as such.

## IDEA#3. Versioned saves with recovery

**Present evidence:** OCA-53 supplies the basic settings/save path and
[save_check](../tools/save_check/README.md) proves reload behavior. Its
[settings writer](../tools/save_check/settings.cpp) truncates a destination;
the [guide](../website/src/content/docs/docs/guides/settings-and-saves.mdx)
leaves a game's schema and error policy to that game.

**Proposed slice:** an example-owned versioned settings file with one
migration, temporary-write/replacement on Windows, and an explicit browser
flush result. Choose preservation and recovery behavior before coding. This
does not add an engine serializer or promise identical filesystem semantics.

**Done when:** fixtures cover missing/current/old/future versions, malformed
data, out-of-range fields, interrupted or failed writes preserving the prior
valid save, and explicit retry. Browser reload fixtures cover failed sync as
well as success. State each platform's durability limits and keep invalid or
future data intact until the selected policy permits replacement.

## IDEA#4. A bounded, dead-zone camera-follow component

**Present evidence:** [the multiplayer arena](../samples/local_multiplayer/arena.cpp)
already centres cameras directly. [Camera](../engine/render/camera.h) supplies
the transform, and [the components rule](../samples/components/README.md#what-a-component-is-not)
places following behavior in a component rather than the engine.

**Proposed slice:** a copyable alternative with a dead zone and a world-edge
clamp, demonstrated by the existing two-view sample. Settle behavior when the
world is smaller than the viewport. Smoothing is a separate choice; camera
rotation (`GAP#9`) is not a prerequisite.

**Done when:** headless tests cover a stationary target inside the zone,
crossing every edge, world clamps, resize, a small world and independent
player cameras. Review the native/browser sample and keep `draw()` const.

## IDEA#5. A grid sprite-sheet authoring command

**Present evidence:** the [sprites guide](../website/src/content/docs/docs/guides/sprites-and-text.mdx)
shows hand-authored frame/strip data, read by
[sprite_sheet_loader.cpp](../engine/assets/sprite_sheet_loader.cpp).
The animation example, SRL-20, gives an immediate output to reproduce.

**Proposed slice:** generate the existing unrotated JSON schema from explicit
image dimensions, grid dimensions, frame names and strip definitions. Keep
image packing, decoding, PNG loading (`GAP#4`) and a new runtime schema out of
the first version. This tool can use only the standard library.

**Done when:** identical inputs give identical JSON; duplicate names,
out-of-bounds frames, invalid strip extents and invalid dimensions fail with
useful errors. Round-trip generated fixtures through the actual loader and
reproduce the animation example without manual rectangle arithmetic.

## IDEA#6. Pointer interaction in the components gallery

**Present evidence:** OCA-50 already owns the pointer-menu guide and lists
gallery click support as an optional follow-up. The [gallery](../samples/components/gallery_state.cpp)
currently wires keyboard/pad navigation; [Slider](../samples/components/slider.h)
has no pointer interaction. This proposal makes that optional extension
selectable; it does not duplicate OCA-50's required guide work.

**Proposed slice:** click/tap an option row and drag a slider. The page owns
hit targets, coordinate conversion, capture and input precedence. Use the
existing camera transform, preserve keyboard/pad behavior and keep pointer
policy out of `engine/ui/`.

**Done when:** headless tests cover endpoint/midpoint values, clamping,
disabled rows, cancellation on lost focus, release outside the row, one active
touch id and avoiding a second activation from a synthetic mouse event.
The browser page test drives the gallery through its actual CSS geometry.
Identify emulated touch separately from a physical touchscreen session.
