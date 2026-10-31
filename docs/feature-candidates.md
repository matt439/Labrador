# Feature candidates

These proposals supplement the [2026-10-27 comparison](survey/2026-10-27.md).
They are based on this checkout, not on claims about other engines. Their
current selection state is indexed in [TODO.md](../TODO.md); none is an
implementation commitment. Select one, then give it a story and a place in
[BACKLOG.md](product/BACKLOG.md) before starting it.

`IDEA#1` to `IDEA#6` came from a read of the samples and guides. `IDEA#7` to
`IDEA#19` come from the engine review of 2026-10-31, which read every public
header and sample README against the planned rows in TODO.md, BACKLOG.md's
horizons and Won't list, the comparison and its *Not now* list, and kept only
what none of them names.

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

Among the review's additions, `IDEA#10` and `IDEA#7` remove limits every
game meets and are graded `should`; the rest are `could` and wait for a
client's case, as T1 asks. `IDEA#14` is the owner's call before it is a slice,
because the pacing records rest on the constant it would turn into a setting.

| id | proposal | priority | size | prerequisite |
|---|---|---|---|---|
| `IDEA#1` | A single-line typed-text component | could | M | Select its client and supported text scope; coordinate with OCA-42 |
| `IDEA#2` | A game-owned keyboard rebinding screen | could | M | Select the sample and its duplicate-binding policy |
| `IDEA#3` | Versioned saves with recovery | could | M | Select the example's save format and recovery policy; builds on OCA-53 |
| `IDEA#4` | A bounded, dead-zone camera-follow component | could | S | Select the local-multiplayer example's follow behavior |
| `IDEA#5` | A grid sprite-sheet authoring command | could | S | Select the input description; builds on the existing sheet schema |
| `IDEA#6` | Pointer interaction in the components gallery | could | S | OCA-50's guide first, or explicitly amend that story to include this extension |
| `IDEA#7` | Circle collision in the narrow phase | should | S | Select the shape pairs; the closest-point axis is named in narrow_phase.h |
| `IDEA#8` | Scene queries: segment hit and region overlap | could | M | Select the result shape and mask filter; exact tests, not swept motion |
| `IDEA#9` | Contact begin and end edges | could | S | A second client needing the edge; the collision sample derives it in a few lines |
| `IDEA#10` | Arbitrary client sizes | should | S | Select whether monitor-mode enumeration comes with it or later |
| `IDEA#11` | Sounds from WAV files and from memory | could | M | Select the seam verb's shape; compressed formats stay with GAP#2 |
| `IDEA#12` | Animation sequences with named frames and durations | could | M | Select the JSON form; the existing strip form stays valid |
| `IDEA#13` | A pointer hit test over a focus group's bounds | could | S | Keep OCA-50's rule that pointer policy stays in the page |
| `IDEA#14` | Present interval and render-rate control | could | M | Owner decision: the pacing records assume today's constant interval |
| `IDEA#15` | Display refresh rate in the device report | could | S | BDP-07 and SM-08 name the need; none for a game yet |
| `IDEA#16` | Window title at run time, and a diagnosed window icon | could | S | Select the icon's default name and whether absence is loud |
| `IDEA#17` | Cursor shapes | could | S | OCA-50's pointer menu or a client wanting a hand cursor |
| `IDEA#18` | A side-by-side two-player layout | could | XS | A client or guide wanting left and right panes |
| `IDEA#19` | Rectangle intersection and shortest-arc angle helpers | could | XS | Could ride GAP#8's easing story |

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

## IDEA#7. Circle collision in the narrow phase

**Present evidence:** [narrow_phase.h](../engine/collision/narrow_phase.h)
measures the convex polygons and throws `std::invalid_argument` for a
`Circle`, saying that adding one is "one more axis, the centre-to-closest-point
one". [circle.h](../engine/math/circle.h) and [intersects.h](../engine/math/intersects.h)
already supply the shape and every yes/no predicate against it, so a
`CollisionObject` whose `shape()` is a circle compiles today and throws from
the sweep at the first candidate pair whose boxes overlap. `GAP#6` is about
drawing circles, not colliding them. Balls, round bodies and radial triggers
are a whole genre's first shape.

**Proposed slice:** circle against each polygon shape, where the polygon's
closest point to the centre supplies the extra axis, and circle against
circle, under the same [Manifold](../engine/collision/manifold.h) contract:
a unit normal from `a` towards `b`, a positive penetration, touching is no
contact, and nonfinite input reports none. The broad phase is unchanged; a
circle's bounding box already exists. Any pair still unsupported keeps the
throw.

**Done when:** narrow-phase tests cover containment, grazing, nonfinite
coordinates, each polygon type and agreement with intersects.h's predicates on
randomised pairs; `find_contacts` tests pass with mixed shapes; the collision
sample or its tests hold one round body; the collision guide states the
supported pairs.

## IDEA#8. Scene queries: segment hit and region overlap

**Present evidence:** [scene.h](../engine/scene/scene.h) exposes
`collision_objects()` and nothing about where they are; its `BroadPhase` is a
private member and [broad_phase.h](../engine/collision/broad_phase.h) answers
only pair lists. [intersects.h](../engine/math/intersects.h) holds every
segment-against-shape predicate, so a game asking "what does this shot hit
first" writes the loop and a switch over `shape_type()` itself, and a game
asking "who is inside this blast" walks every object for every blast.
PHILOSOPHY's public-face rule holds that a predicate a game needs belongs in
the engine at whatever caller count.

**Proposed slice:** two const queries over the live collision objects, as free
functions or Scene members: the nearest object whose shape a segment meets,
with the hit point, the distance along the segment and a mask filter; and
every object whose bounding box, or shape, overlaps a rectangle, filtered the
same way, into a caller-owned vector. Exact shape tests against a segment, not
swept motion: T3 and [tunnelling.h](../engine/collision/tunnelling.h) stay as
they are. Allocation-free once warm; results borrow until the next
`end_tick()`, as `contacts()` does.

**Done when:** headless tests over hand-built scenes cover each shape type,
the mask filter, equal distances, a segment starting inside a shape, a
degenerate segment and an empty scene; a `bench/` case holds the region
query's scaling against the broad phase's on quadrupled counts; the collision
guide shows one hitscan and one radius query.

## IDEA#9. Contact begin and end edges

**Present evidence:** the engine reports overlaps per tick as a value list
([contacts.h](../engine/collision/contacts.h)). The collision sample derives
trigger entry by keeping last tick's overlap beside this tick's in
[arena.cpp](../samples/collision/arena.cpp), and its README calls that game
policy rather than an engine event subscription; LineSweeper's particle field
diffs whole worlds for the same reason. Both are a few lines, which is why the
prerequisite is a second client that needs the edge rather than the pattern.

**Proposed slice:** a small value type given each tick's contact span that
answers which pairs began, continued and ended, keyed on the participant
pointers and valid between `end_tick()` calls. No callbacks and no bus; what an
edge means stays the game's (T1).

**Done when:** tests cover begin, continue and end across ticks, a pair that
separates and touches again, a participant retired mid-tick and an
allocation-free steady state; the collision sample's trigger counter uses it,
or the proposal records that the sample pattern is enough and closes.

## IDEA#10. Arbitrary client sizes

**Present evidence:** [application.h](../engine/app/application.h)'s
`set_resolution` and `ApplicationOptions::resolution` accept only the four
16:9 presets in [screen_resolution.h](../engine/render/screen_resolution.h).
[resolution_manager.h](../engine/render/resolution_manager.h) already lays out
against any size through `set_resolution_exactly`, and
[window.h](../engine/app/window.h)'s `resize_client` takes a `Vector2I`, but a
game can reach neither. So a game cannot open at 1366x768 or 1600x900, cannot
offer its monitor's modes in an options menu, and cannot choose a 16:10 or 4:3
size, while the low tier PHILOSOPHY names is a laptop-class part.

**Proposed slice:** a `Vector2I` overload of `set_resolution` and a size form
in `ApplicationOptions`, validated against the minimum window size and
refused by name below it. The enum stays as the label for the four presets,
and `ResolutionManager::resolution()` keeps its stated meaning, the preset last
asked for. Enumerating the monitor's modes is a separate, later question.

**Done when:** hidden-window tests read back the client size for several
arbitrary sizes, including one below the minimum, refused, and one past the
monitor, clamped as `resize_client` documents; the hidden-window and browser
size reports are unchanged; the window guide's resolution section shows a
custom size; CHANGES.md records the addition.

## IDEA#11. Sounds from WAV files and from memory

**Present evidence:** the only way sound enters is
[audio_device.h](../engine/audio/audio_device.h)'s `open_wave_bank`, which
opens an `.xwb` packed by Microsoft's XWBTool. The engine owns an `.xwb` reader
([xwb_file.h](../engine/audio/xwb_file.h)) and no RIFF reader, so a reader's
first sound needs a downloaded tool, which is what `GAP#4` removes for images.
`GAP#2` is streamed compressed music, a different need. Nothing lets a game
hand a backend samples it produced itself, the audio counterpart of `GAP#7`.

**Proposed slice:** a headless RIFF/PCM reader beside xwb_file.h producing the
same `WaveData`; one new seam verb that opens a bank from in-memory
`WaveBankData`, implemented on XAudio2, Web Audio and null; and a `wave` asset
kind that builds a one-wave bank from a `.wav`. Compressed formats stay with
`GAP#2`. The memory cost is stated as xwb_file.h states it.

**Done when:** the reader round-trips [the audio sample's checked-in WAVs](../samples/audio/source/)
against the bank XWBTool made from them; refusals name truncated, non-PCM and
odd-block files; `AudioTests` on all three backends play a memory-built bank
and record the same levels; the audio guide's first sound needs no tool;
`ReleaseNotices` changes only if a dependency is added.

## IDEA#12. Animation sequences with named frames and durations

**Present evidence:** [animation_strip.h](../engine/render/animation_strip.h)
models a strip as a first frame, a count, one frame time and a looping flag:
horizontal, contiguous and uniformly timed. [sprite_sheet.h](../engine/render/sprite_sheet.h)
says a per-frame pivot on an animation "is asking for a schema this loader
does not have". Aseprite and TexturePacker export sequences of named frames
with their own durations, and neither `GAP#4` nor `IDEA#5` widens the schema.

**Proposed slice:** a second strip form in the sheet JSON, a list of frame
names each with a duration, resolved to frame handles at load and read by
index per frame; `AnimationStrip` gains a frame accessor that returns a
`SpriteFrame`, so authored pivots apply, and a per-frame time. The existing
form stays valid, and the components' animation preview reads either.

**Done when:** loader tests cover both forms in one sheet, an unknown frame
name, zero and negative durations and unknown keys; the animation preview
plays a non-uniform sequence; the sprites guide's animation section shows both
forms; CHANGES.md records the schema addition.

## IDEA#13. A pointer hit test over a focus group's bounds

**Present evidence:** [navigation.h](../engine/ui/navigation.h) is arithmetic
over widget bounds and [focus.h](../engine/ui/focus.h) holds the reachable set.
A point-in-bounds walk over that same set is written by hand for touch in
LineSweeper's [pause_state.cpp](../samples/linesweeper/states/pause_state.cpp),
and `IDEA#6` would have the gallery write it again for the mouse. Two copies is
the test the components README sets for promotion. OCA-50 and `IDEA#6` keep
pointer policy in the page, which pointer, capture and precedence, and so
does this.

**Proposed slice:** a const query, `FocusGroup::widget_at(point)` or a free
function over rectangles as `nearest_in_direction` has, answering the enabled
widget whose bounds contain the point, with the same candidate-order tie rule
and the same skip of degenerate boxes; optionally a per-call padding, because
LineSweeper widens its rows to a thumb's height.

**Done when:** tests cover hits, misses, overlaps resolved by order, disabled
and degenerate widgets and padding; LineSweeper's pause menu and the gallery
call it; the menus guide quotes it.

## IDEA#14. Present interval and render-rate control

**Present evidence:** every rasterising backend requests a swap interval of
one, or Vulkan's FIFO, and [renderer.h](../engine/render/renderer.h)'s
`RenderDeviceInfo` reports it read-only; [application.h](../engine/app/application.h)
says rendering is not capped by the fixed step. A game cannot drop vsync for a
latency test, cap drawing while unfocused or hold a laptop to a lower rate.
[The ratchet document](performance/2026-09-19-g6f-ratchet.md) and
[the pacing method](port/pacing-method.md) rest on the interval being a
constant, which is why this is the owner's call before it is a slice.

**Proposed slice:** a requested interval in `ApplicationOptions`, vsync on or
off, honoured by the four rasterising backends and reported as today; and an
optional render-rate cap the loop applies while the window is not in the
foreground, skipping frames and never fixed steps. The benchmark keeps an
interval of one and records what it asked for. A browser has no interval to
request and keeps its animation frame.

**Done when:** each backend reports the requested interval; a hidden-window
test shows the cap skipping frames while `frame_timings()` and the step count
stay truthful; the bench's recorded present mode is unchanged; the ratchet
document states which settings its numbers assume.

## IDEA#15. Display refresh rate in the device report

**Present evidence:** nothing in `engine/` or `bench/` queries a refresh rate.
[The pacing method](port/pacing-method.md) and LineSweeper's README leave
display identity to an operator, and `BDP-07` needs a 60 Hz display and a
high-refresh display told apart.

**Proposed slice:** an optional refresh rate on `RenderDeviceInfo`, read from
the window's monitor at device creation and after a monitor change, absent in
a browser and on the null backend; the frame bench records it beside the
device name.

**Done when:** the field is filled on the four rasterising backends on the
owner's desktop and absent on null and wasm; the bench's result files carry
it; the pacing method cites it in place of the operator's note.

## IDEA#16. Window title at run time, and a diagnosed window icon

**Present evidence:** `window_title` is read once from `ApplicationOptions`
and nothing changes it afterwards. The Win32 window loads an icon resource
named `IDI_ICON` ([window.cpp](../engine/app/win32/window.cpp)) and silently
takes Windows' default when no such resource exists; the round-2 review
recorded that as the quiet degradation T6 forbids, no disposition followed,
[the minimal template](../samples/minimal/CMakeLists.txt) has no resource
file, and [the Windows release guide](../website/src/content/docs/docs/guides/windows-release.mdx)
does not mention an icon. A shipped game carries the stock icon.

**Proposed slice:** `Application::set_window_title`; the icon resource's name
in `ApplicationOptions` with a stated default and either a loud diagnostic or a
documented silence when it is absent; a resource file in the minimal template
and a step in the release guide. In a browser the page owns both, and the calls
do nothing there.

**Done when:** a hidden-window test reads the title back; the template's
installed release shows its icon in Explorer and the taskbar on the owner's
desktop; the release guide's step is followed in a copied project.

## IDEA#17. Cursor shapes

**Present evidence:** the Win32 window sets the arrow or hides the pointer
([window.cpp](../engine/app/win32/window.cpp)), and the browser sets the
canvas's CSS cursor to default or none. A pointer menu (OCA-50) that wants a
hand over a row, or a drag that wants a move cursor, cannot ask.

**Proposed slice:** a small engine enum, arrow, hand, text, crosshair, move
and not-allowed, on `Application::set_cursor`, mapped to system cursors on
Win32 and CSS cursor names in a browser. A custom image stays a hidden pointer
plus a sprite, which already works.

**Done when:** each value is set and read back in a hidden-window test and a
page test; the pointer-menu guide uses the hand.

## IDEA#18. A side-by-side two-player layout

**Present evidence:** [viewport_manager.cpp](../engine/render/viewport_manager.cpp)
stacks the two-player layout top and bottom, as the local-multiplayer sample
shows; a game wanting left and right panes builds its own viewports.

**Proposed slice:** a side-by-side two-player layout, or an orientation on the
existing one, with its divider; `all_viewports` and `viewport_dividers` follow.

**Done when:** viewport tests cover the new layout on odd widths, each edge
then the difference; the local-multiplayer sample or guide shows the switch.

## IDEA#19. Rectangle intersection and shortest-arc angle helpers

**Present evidence:** [rectanglef.h](../engine/math/rectanglef.h) has
`union_of` and `intersects` but no intersection rectangle, which `GAP#5`'s
clip rectangle and any overlap-area code will want. [vector2f.h](../engine/math/vector2f.h)
has `angle_between` but nothing wraps an angle to a range or interpolates along
the shorter arc, a turret-and-steering hand-roll.

**Proposed slice:** `RectangleF::intersection_of`, returning the overlap or
an empty rectangle; `wrap_angle` and `lerp_angle` beside `lerp` in
[scalar.h](../engine/math/scalar.h), with their ranges and signs stated.
Nothing else; this could ride `GAP#8`'s easing story.

**Done when:** behavioural tests pin empty, touching and contained rectangles,
negative and large angles and the half-turn tie, and state how nonfinite
input is handled.
