# Recommendations for the owner's open feature calls, 2026-11-04

**A dated record, not a plan.** It gives options, their costs and one
recommendation for each `DECISION` row in [TODO.md](../TODO.md) that is a
call rather than a hardware session: `GAP#1` to `GAP#13` from
[the engine comparison](survey/2026-10-27.md), `IDEA#1` to `IDEA#19` from
[Feature candidates](feature-candidates.md), and what is left of `DOC#4`.
Nothing here is decided or implemented. A row the owner takes becomes a story
in [BACKLOG.md](product/BACKLOG.md) first, by that file's rule; a row the
owner declines leaves TODO.md with its reason and its revisit trigger
recorded in its owner, as Won't this time records its own.

The source was read at `be8eb71`. Claims about other libraries (SDL3,
GameInput, stb) are from knowledge of their current releases and were not
re-checked against their documentation, as the comparison's were not.

## The verdicts

Four verdicts are used. **Take** means write the story now. **Take, gated**
means write the story now with the gate as its first prerequisite.
**Defer** means status `DEFERRED` with the named condition. **Decline** means
the row closes with its reason and a revisit trigger.

| Row | Subject | Verdict | Suggested grade |
|---|---|---|---|
| `IDEA#7` | Circle collision | Take | should · S |
| `IDEA#10` | Arbitrary client sizes | Take | should · S |
| `IDEA#16` | Window title and a diagnosed icon | Take | should · S |
| `GAP#4` | PNG loading | Take | should · M |
| `IDEA#11` | WAV and in-memory sounds | Take, after `GAP#4` | could · M |
| `IDEA#15` | Refresh rate in the device report | Take, before `BDP-07`'s session | could · S |
| `IDEA#4` | Dead-zone camera follow | Take | could · S |
| `IDEA#5` | Grid-sheet authoring command | Take, after `GAP#4` | could · S |
| `GAP#6` | Debug drawing of collision shapes | Take a narrower slice, after `IDEA#7` | could · S |
| `GAP#1` | Non-Xbox pads on Windows | Take, gated on owning a non-Xbox pad | should · L |
| `IDEA#8` | Scene queries | Defer until `IDEA#7` lands | could · M |
| `GAP#2` | Streamed compressed music | Defer until a client ships a track | could · L |
| `GAP#7` | Run-time textures | Defer until a client asks | could · M |
| `GAP#3` | Render to texture | Defer until a client asks | could · L |
| `GAP#5` | Clip rectangle | Defer until a client scrolls | could · M |
| `IDEA#19` | Rectangle and angle helpers | Defer, ride `GAP#5` | could · XS |
| `GAP#13` | Relative mouse motion | Defer until a client asks | could · M |
| `GAP#9` | Camera rotation | Defer until a client asks | could · M |
| `IDEA#1` | Typed-text field | Defer until a client asks | could · M |
| `IDEA#2` | Rebinding screen | Defer until a client asks | could · M |
| `IDEA#3` | Versioned saves | Defer with `OCA-76` | could · M |
| `IDEA#12` | Named-frame animations | Defer until a client asks | could · M |
| `IDEA#6` | Gallery pointer interaction | Defer until a client asks | could · S |
| `IDEA#17` | Cursor shapes | Defer until a client asks | could · S |
| `IDEA#18` | Side-by-side split | Defer with `OCA-47` | could · XS |
| `IDEA#9` | Contact edges | Defer until a second client | could · S |
| `GAP#10` | Particle component | Defer with `SM-41` | could · S |
| `IDEA#13` | Focus-group hit test | Decline | |
| `IDEA#14` | Unfocused render-rate cap (vsync has landed) | Decline: close as answered | |
| `GAP#8` | Easing functions | Decline | |
| `GAP#11` | Tilemaps and Tiled | Decline | |
| `GAP#12` | TrueType at run time | Decline | |
| `DOC#4` | Site link and report links | Link once from *Why Labrador?*; leave reports as they are | |

If the owner takes all of them, a workable order is `IDEA#7`, `IDEA#10`,
`IDEA#16`, then `IDEA#15` before the 144 Hz session, then `GAP#4`, `IDEA#11`,
`IDEA#5`, `IDEA#4`, `GAP#6`'s slice, and `GAP#1` when a pad is to hand. The
first three are small, remove a trap or a hard limit, and need no owner
hardware beyond the hidden-window tests.

## What changed since the rows were written

Several rows rest on facts that have moved, and the recommendations use the
current ones.

- `OCA-40` closed. The audio guide's *Music, bank memory and the page
  download* states that three minutes of stereo 16-bit PCM is about 30 MiB of
  wave data and about 60 MiB of float buffers in a browser. `GAP#2` no longer
  waits on it.
- `OCA-43` closed, so `GAP#12`'s "fold it into `OCA-43`" has nothing to fold
  into.
- `OCA-50` closed, and its story records that an engine hit test over
  `bounds()` was weighed on 2026-10-24 and declined, because LineSweeper sizes
  tap targets by policy, not by bounds. `IDEA#13` proposes that same hit test.
- `lodepng` is already in `vcpkg.json`, for the golden-image tests. `GAP#4`
  can use a port already resolved, built and noticed by CI rather than a new
  dependency, though it would become a run-time dependency of games.
- The 2026-10-30 decision set the adopter's floor at someone with very little
  programming experience. The first-hour guides today send that reader to
  download `texconv`, `xwbtool` and MakeSpriteFont before a picture or a sound
  appears. `GAP#4` and `IDEA#11` each remove one of those.
- Neither LineSweeper nor any other sample in this tree has an easing curve.
  `GAP#8`'s premise, that the clients already hand-roll them, rests on the
  client alone.

## Collision

### IDEA#7. Circle collision in the narrow phase

A `CollisionObject` whose shape is a `Circle` compiles today and throws
`std::invalid_argument` from the sweep the first time its box overlaps
another. That is a trap rather than a missing feature: the type system
accepts what the engine refuses.

- **A. Implement circle against each polygon and circle against circle** under
  the current `Manifold` contract. Small, headless, and `narrow_phase.h`
  already names the axis. It removes the trap and serves the most common
  first shape in a 2D game.
- **B. Refuse a circle at construction** instead of at the sweep. Cheaper,
  and it makes the failure earlier (T6), but it keeps a hole in a shape set
  the math library already supports everywhere else.
- **C. Leave it.** The throw is loud, so T6 is met, but a beginner meets it
  mid-game rather than at their desk.

**Recommendation: A, should, S.** It is the cheapest row with the largest
effect, and every test it needs is headless.

### IDEA#8. Scene queries: segment hit and region overlap

The public-face rule holds that a predicate a game needs belongs in the
engine at whatever caller count, and every shooter asks "what does this shot
hit first".

- **A. Both queries now.** M, plus a scaling benchmark. Segment against
  circle needs `IDEA#7`'s shapes to be worth much.
- **B. The region query alone.** Smaller; it is the one a broad phase
  accelerates, and a blast radius is common.
- **C. Wait.** A game can loop over `collision_objects()` with the
  `intersects.h` predicates today.

**Recommendation: defer until `IDEA#7` lands, then take A as one story.**
Taking it first would ship a segment query that cannot hit the shape most
games use for bullets.

### IDEA#9. Contact begin and end edges

The collision sample derives trigger entry in a few lines, and the
proposal's own prerequisite is a second client that needs the edge.

- **A. Build the helper.** S, tidy, but no second caller exists.
- **B. Close it** with the record that the sample pattern is enough, which
  the proposal's *Done when* already allows.
- **C. Defer** until a second client derives the same edge.

**Recommendation: C.** B is defensible, but C keeps the trigger visible at
no cost.

### GAP#6. Circles, polygons and debug drawing

The usual need is seeing collision shapes while debugging, and that does not
need non-quad geometry: outlines are line segments, and lines already draw
from `engine:white`.

- **A. Filled circles and polygons in the renderer.** New geometry on five
  backends and new goldens, against T8's one sprite pipeline.
- **B. A copyable debug-draw component** in `samples/components` that
  outlines each collision shape from line quads, a circle as N segments.
  No engine change, no backend change.
- **C. Leave it.**

**Recommendation: B, could, S, after `IDEA#7`** so circles have something to
outline. Close the filled-geometry half with a revisit trigger of a client
that needs filled non-rectangular shapes.

## Window, display and pacing

### IDEA#10. Arbitrary client sizes

A game can request only four 16:9 presets, so it cannot open at 1366x768 or
1600x900, offer 16:10, or fill a laptop's native size, while the low tier
PHILOSOPHY names is a laptop-class part. `ResolutionManager` and
`Window::resize_client` already handle any size; only the public call is
missing.

- **A. A `Vector2I` overload and options field,** validated against the
  minimum, as proposed. S, additive, a CHANGES.md entry.
- **B. A and monitor-mode enumeration** for an options menu. Larger, and a
  browser has no modes to enumerate.
- **C. Leave it.**

**Recommendation: A, should, S.** Enumeration waits for a menu that lists
modes.

### IDEA#16. Window title at run time, and a diagnosed icon

The Win32 window loads `IDI_ICON` and silently takes Windows' default when the
resource is absent. The round-2 review recorded that as the quiet degradation
T6 forbids and nothing followed; the minimal template has no resource file
and the release guide does not mention an icon, so every shipped game carries
the stock icon.

- **A. All of it:** `set_window_title`, the icon's name in options with a
  diagnostic when it is absent, a resource file in the template, a release
  guide step.
- **B. The icon half only.** It is the half with a recorded T6 finding; the
  title setter has no caller.
- **C. Document the silence** in the release guide and change no code.

**Recommendation: A, should, S.** The title setter is a few lines beside the
icon work, and the template change is the part a beginner sees. The template
change needs a line in the minimal README's change list.

### IDEA#15. Display refresh rate in the device report

`BDP-07` is an owner session at 60 Hz and at 144 Hz, and today the operator
writes the display's rate into the record by hand.

- **A. Report it on `RenderDeviceInfo` and in the frame bench record,** as
  proposed.
- **B. Record it in the bench only,** read there, with no engine field.
- **C. Keep the operator's note.**

**Recommendation: A, could, S, landed before `BDP-07`'s session**, so the
records that session makes carry a measured rate rather than a typed one. B
is acceptable if the owner prefers no new public field without a game
caller.

### IDEA#14. Present interval and render-rate control

The interval half has landed since the row was written:
`Renderer::set_vsync` and `ApplicationOptions::vsync`, defaulting on, at a
client's request. What remains is the unfocused render-rate cap, and the
row's clause should shrink to that.

- **A. Add the unfocused cap.** It does not touch the benchmark, which runs
  focused, but a browser already throttles a hidden tab and vsync, still the
  default, already caps a native window.
- **B. Close the row** as answered by `set_vsync`, with the cap left for a
  client that asks.

**Recommendation: B.** The ratchet document should state that its numbers
assume the default of vsync on, if it does not already, which is a sentence
rather than a story.

### IDEA#17. Cursor shapes

`OCA-50` closed without a hand cursor, and no client has asked.

- **A. The six-value enum** on Win32 and the canvas. S.
- **B. Defer** until a pointer menu or a drag wants one.

**Recommendation: B.** A custom cursor already works as a hidden pointer plus
a sprite.

### IDEA#18. A side-by-side two-player layout

- **A. Add the layout.** XS.
- **B. Defer with `OCA-47`,** the three- and four-player layout story in Won't
  this time, which revisits layouts with `OCA-46`'s join screen.

**Recommendation: B.** It is cheap, but it is still a layout no caller asked
for, and it belongs with the story that next touches layouts.

### GAP#9. Camera rotation

`Camera` is translation and scale; rotation means growing culling's visible
rectangle and testing it on every backend's draw path.

- **A. Add rotation.** M.
- **B. Defer** until a client needs a rotating view.

**Recommendation: B.** Nothing in this tree or its guides wants one.

## Input

### GAP#1. PlayStation, Switch and generic pads on Windows

The niche is couch multiplayer, guests bring their own pads, and XInput reads
only Xbox-style ones. The browser backend already reads any pad the Gamepad
API maps. This is the comparison's highest-value row.

- **A. Build on Windows.Gaming.Input or GameInput.** Stays inside Microsoft's
  stack, but neither maps a PlayStation or Switch pad's buttons for you as far
  as is known; the engine would own a mapping table.
- **B. Buy SDL3's gamepad layer** as a third `LABRADOR_INPUT_BACKEND`, used
  only for pads, with no SDL window. T9 names input as a platform edge where
  buying is right, and SDL's mapping database and HID drivers are the work a
  one-person engine should not redo. Costs a large dependency, a
  ReleaseNotices entry and its licence text.
- **C. Keep XInput** and say in the guides which pads work.

Raising `Gamepads::max_count` above four is a separate question; the layouts
stop at four panes and nothing asks for more.

**Recommendation: B, should, L, gated on the owner having a non-Xbox pad.**
The owner has one XInput pad, so a story taken now could pass only headless
tests and claim nothing about the pads it exists for. Write the story now,
with buying or borrowing a DualSense or a Switch Pro pad as its first
prerequisite, and keep four slots. Until then, C's guide sentence is worth
landing on its own.

### GAP#13. Relative mouse motion

Win32 raw input and a browser pointer lock that is granted only inside an
input event. Its users are mouse-look games, outside the couch-multiplayer
niche.

**Recommendation: defer** until a client asks. Building it would also mean a
second "only inside an input event" path in the shell, beside full screen,
for no current caller.

## Images, sound and fonts

### GAP#4. PNG and other common image formats

A beginner today installs `texconv` and runs it with half a dozen options
before their first picture appears. The 2026-10-30 floor makes that the most
expensive step in the first hour.

- **A. Load PNG at run time** through `lodepng`, already in the manifest,
  premultiplied at load, beside DDS. A beginner drops a PNG in and it draws.
  Costs a run-time dependency (ReleaseNotices), uncompressed textures in
  memory, and decode time at load.
- **B. Convert PNG to DDS at build time** with a CMake step built on the same
  decoder. Run-time stays DDS-only; the beginner still needs nothing to
  download, but a build tool is added to every project.
- **C. Keep texconv** and the guide as it is.

**Recommendation: A, should, M.** It is the change a beginner notices most,
and it uses a decoder CI already builds. DDS stays the route for
block-compressed textures, and the guides can say when to switch.

### IDEA#11. Sounds from WAV files and from memory

The audio counterpart of `GAP#4`: today the first sound needs XWBTool.

- **A. A RIFF reader, a `wave` asset kind and an in-memory bank verb** on all
  three backends, as proposed. M, built rather than bought, since WAV is
  simple and the reader sits beside `xwb_file.h`.
- **B. The `.wav` asset kind alone,** with no public in-memory verb. Smaller
  seam change, but loses generated audio.
- **C. Keep XWBTool.**

**Recommendation: A, could, M, after `GAP#4`,** whose guide changes it should
mirror. The in-memory verb is also the seam a later `GAP#2` decoder would
feed.

### GAP#2. Streamed, compressed music

`OCA-40` put a number on the cost: about 60 MiB of browser buffers for three
minutes of stereo music, and a guide that recommends `--preload-file`.

- **A. Decode at load:** Ogg Vorbis into a bank, smaller download, same
  memory.
- **B. Stream one music voice:** a decoder bought natively (stb_vorbis is the
  usual choice) and the browser's own media element for the web, with null
  recording what was asked. L, three backends.
- **C. Keep PCM** and the guide's advice.

**Recommendation: defer until a sample or client ships a music track,** then
take B. No sample in this tree plays a long track (the audio sample's README
points to the guide's budget instead), so a streaming voice would ship with
only a synthetic test to show it works. `IDEA#11`'s seam verb should land
first so the decoder has somewhere to feed.

### GAP#12. TrueType and OpenType fonts

The row's plan was to fold this into `OCA-43`, which has closed. Building a
glyph atlas is engine work in T9's own words ("the glyph atlas standing on it
is the mixer"), so a run-time font library would be buying the engine.

- **A. Rasterise TrueType at run time.** L, a dependency, and against T9's
  reading.
- **B. A command-line tool that bakes a `.spritefont`** from a TTF on any
  platform, replacing MakeSpriteFont's Windows-only step. Keeps run time
  unchanged. M.
- **C. Close the row.**

**Recommendation: C, recording B as the revisit path** if MakeSpriteFont
becomes the beginner's last downloaded tool after `GAP#4` and `IDEA#11` land.
That is when B would earn its place.

### GAP#7. Textures made or updated from pixels at run time

`resource_factory.h` already makes a texture from decoded bytes, and
`GAP#4`'s loader will exercise that path for every PNG.

**Recommendation: defer until a client asks** (a minimap, a paint layer,
procedural noise). After `GAP#4` lands the public half is small, and the
story should then say whether updating an existing texture in place is in
scope, which is the expensive part on five backends.

### GAP#3. Render to texture

A target type, five backends and new goldens, for integer-scaled pixel art,
transitions and minimaps.

**Recommendation: defer until a client asks.** It is the largest rendering
row, and Vulkan's engine-owned image shows one backend could do it, but no
game in this tree or its guides needs it.

## Content authoring

### IDEA#5. A grid sprite-sheet authoring command

Beginners write frame rectangles by hand in the animation guide.

- **A. A standard-library command** that writes today's JSON from grid
  dimensions and names, reproducing `SRL-20`'s example. S.
- **B. Leave the arithmetic** in the guide.

**Recommendation: A, could, S, after `GAP#4`,** so the guide can go from a
PNG to an animating sprite without either tool.

### IDEA#12. Named-frame animations with durations

Aseprite and TexturePacker export this shape, but nobody in this tree has
asked for it, and it widens the schema the loader validates.

**Recommendation: defer** until a client or a guide reader brings an
exported sheet. `IDEA#5` covers the beginner's uniform strip.

### GAP#11. Tilemaps and Tiled import

**Recommendation: decline.** PHILOSOPHY's *Every genre* non-goal names this
case, and no client uses tiles. Revisit with a client that does.

## Gameplay and UI examples

### IDEA#4. A dead-zone camera-follow component

The beginner guide *A camera that follows the player* ends by naming this
row as a possible addition the owner has not decided on, so a reader who
finishes it is pointed at an open question.

- **A. The copyable component** in the local-multiplayer sample, as
  proposed. S, headless tests, no engine change.
- **B. Decline it** and change the guide's last paragraph to say following
stays each game's code, which it already teaches.

**Recommendation: A, could, S.** It is small, it stays out of the engine,
and either way the guide's paragraph changes in the same commit.

### GAP#8 and IDEA#19. Easing and small math helpers

T1 asks for easing to be extracted from what the clients hand-roll, and no
sample in this tree hand-rolls an easing curve. `IDEA#19`'s
`intersection_of` is wanted first by `GAP#5`'s clip rectangle.

**Recommendation: decline `GAP#8`** unless the owner can point to the
client's own copies, in which case the extraction is S and headless. **Defer
`IDEA#19`** to ride `GAP#5` (the rectangle) or a client turning toward a
target (the angles).

### GAP#5. A clip rectangle inside a view

**Recommendation: defer until a client scrolls a list or a panel.** One more
state on the draw list touches five backends and the goldens; LineSweeper's
menus fit on screen.

### GAP#10. LineSweeper's particle field as a component

**Recommendation: defer with `SM-41`,** whose trigger is the same: a second
client copying a component.

### IDEA#13. A pointer hit test over a focus group's bounds

This is the engine hit test `OCA-50` weighed and declined on 2026-10-24: the
one caller sizes its targets from row spacing and a minimum width, not from
bounds, so the target is policy (T1). The proposal's optional padding
narrows the gap but does not reproduce that rule.

**Recommendation: decline,** citing `OCA-50`'s decision. Revisit if
`IDEA#6` lands and a second caller hit-tests against bounds as they are.

### IDEA#6. Pointer interaction in the components gallery

**Recommendation: defer** until a client wants a pointer-driven slider. It is
`OCA-50`'s optional follow-up and has no caller.

### IDEA#1, IDEA#2 and IDEA#3. Typed text, rebinding and versioned saves

Each is a copyable example with a clear slice, and each names choosing its
client as its prerequisite. None has one: no sample enters a name, LineSweeper
has fixed bindings, and `OCA-76`, the engine half of saves, is itself waiting
for a second caller.

**Recommendation: defer all three.** `IDEA#1` and `IDEA#2` until a client or
sample wants name entry or rebinding; `IDEA#3` with `OCA-76`. If the owner
wants one new copyable capability anyway, `IDEA#2` is the strongest, because
rebinding is the accessibility choice the 2026-10-27 decision leaves to
games and a reader cannot easily get right alone.

## DOC#4. What is left after ColourWars went public

Two questions remain: whether the website links ColourWars, and whether the
report decisions should now link into its repository.

**The site link.**

- **A. One sentence on *Why Labrador?*,** in the README's terms: the engine
  is developed against a real client, ColourWars, with a link to its
  repository. It answers an evaluator's "has anyone built a game on this?"
  and uses no ColourWars art, which the website plan keeps a separate choice.
- **B. A showcase entry on Examples** with a screenshot. Stronger, but it is
  the asset choice the website plan defers, and a public showcase invites
  judgement of a game that is not the engine's.
- **C. No link.** The README already has one.

**Recommendation: A.** It costs a sentence and a website check, and it puts
the README's strongest argument where an evaluator reads.

**The report links.** The 2026-10-27 boundary allows only engine-focused
disclosure, with no client path, name or line, and the reports README says
the repository going public does not change that. No file in
`docs/review/colourwars-reports/` links into the client today.

- **A. Leave them as they are** and record that the review found nothing to
  change.
- **B. Link each report to its public client issue or ledger entry.** It
  widens the disclosure the owner deliberately bounded.

**Recommendation: A.** With the site sentence landed, `DOC#4` closes.
