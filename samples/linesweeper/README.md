# LineSweeper — design decisions

A falling-block game, and the sample that shows what a whole game on this
engine looks like. This file records the decisions behind it and the reasons,
so that a reader meeting the code cold does not have to reconstruct them, and
so that a decision cannot be quietly reversed without meeting its argument.

It is deliberately not a design *philosophy*. `docs/design/` holds those, there
are three of them, and this is not a fourth: everything here is a choice about
one sample, made under those three. Where a decision below is really an engine
commitment, it says so and names the document it belongs in.

[Play LineSweeper in a desktop browser](https://mattharding.dev/demo/linesweeper/):
the hosted demo runs the module the `linesweeper_web_dist` target bundles,
starts on a click and plays with a keyboard. Phones are a planned phase, so a
phone gets a poster and a recording of the game to tap and watch, not the game
itself, until real iPhone checks and the host page's phone layout pass. Android
remains unverified.

`LineSweeperBundlePageTests` loads the packaged `web-dist/linesweeper/index.html`
in headless Chromium when configure finds one in a rasterizing browser build.
It clicks Play, observes rendered frames, loses and restores the WebGL context,
quits through the pause menu and plays a fresh instance. It also checks that
touch emulation hides Play and fetches no wasm until a key is seen, that a
truncated wasm response offers an exposed Try again button, and that the page's
captured `onError` does the same after frames have drawn. These are automated
browser checks, not real-phone or physical-controller acceptance. The driver
is `tests/render/web/bundle_input.mjs`; its injected factory wrapper delegates
to the packaged module, and only the truncated-response case changes its wasm.

**Status: playable, and both halves are here.** All three layers exist.
`rules/` is the whole game — the
seven-bag deal, the gravity curve, the wall kicks, lock delay, hold, hard drop,
T-spins and scoring — and `tests/linesweeper/` plays it with no window, no
device and no engine linked into the binary. `presentation/` draws it,
`states/` turns a keyboard, a pad and a touchscreen into it, and pressing R
  or Start after a top-out - or tapping off the buttons, on the glass - restarts
the match with one assignment. It lays itself out for a tall screen as well as
a wide one; see *A phone held upright*, below.

`presentation/particles.cpp` is the second half: ten thousand particles from
one registered object, which is the data-layout claim the value-semantics half
was carrying alone. What is still not here is the instrument panel. See
*The particle field*, below.

## What it is

Ten columns, twenty visible rows, two spawn rows above them. Seven four-cell
pieces on an integer gravity curve, with a visible next queue, a hold slot,
hard drop, a shadow showing where a hard drop lands, seven-bucket
randomisation, T-spins and back-to-back scoring.

Not included: perfect-clear bonus tables. Those exist to reward a technique
against opponents, and this game has none.

## Accessibility

Keyboard play is supported; controller input is implemented but physical-controller
checks remain open. The bundle's visible accessibility note describes both Play
and every game canvas through `aria-describedby`. Tab stays with the browser so
focus can leave the game. The page gives Play and the canvas visible focus
outlines; the pause menu distinguishes its focused row by brightness.
Rendered game state is not read out to screen readers. Controls are fixed by
sample design: actions, bindings and any rebinding belong to the game, and
Labrador deliberately has no engine action map (PHILOSOPHY, *The boundary*).
Touch controls exist, but the host page holds back touch play until real iPhone
and phone-layout checks pass. Android remains unverified.

The landing shadow's edge stands out from the empty well at 3:1 or more for
every piece, checked by a test (*The landing shadow is an outline*).
In a browser, `main_web.cpp` asks `prefers-reduced-motion` once at start and
logs which field it made; a player who has asked for reduced motion gets every
burst with `sparks_per_cell`'s reduced count, an eighth of each cell's sparks
rounded up (`particles.h`, `Motion`). The bursts keep their heat, so the flash
check below still bounds them. The desktop build does not read the system's
animation setting and always plays the full field. `LineSweeperViewTests`
holds the reduced counts, and `LineSweeperReducedMotionPageTests` runs the page
with the preference emulated.

The pause menu's COLOURS row selects one of four piece palettes: STANDARD and
one each for **Protanomaly / Protanopia**, **Deuteranomaly / Deuteranopia** and
**Tritanomaly / Tritanopia**. Enter, Space, A or a tap steps forward through
them, and Left or Right steps either way while the row has the cursor. The
pieces, the landing shadow, the hold slot, the next queue and the sparks all
change on the next frame, and the match does not. A selection lasts until the
program closes: nothing saves it, and nothing reads the system's colour
settings to choose one. The palettes were chosen by simulation, and no player
with a colour-vision deficiency has checked them yet
(*Colour-vision palettes*, below).

`LineSweeperBundlePageTests` checks that the note is visible and referenced
before play and on the first and replay canvases, and that Tab moves focus
from the game to a page link. These automated checks do not establish physical
controller, real-phone or screen-reader acceptance; the
[hardware checklist](../../docs/port/web-to-be-tested.md) owns those limits.

The example host page probes legacy WebAssembly exceptions and WebGL2 only
after Play, before importing the module. Unsupported browsers get advice
without a retry button; download, startup and runtime failures get distinct
player messages, with the original reason in the console. Source-policy
tests cover those paths without a browser. The updated packaged-page checks
still need runtime acceptance; [the source verification record](../../docs/port/2026-10-28-demo-host.md)
keeps that boundary explicit.

## The decisions

### It is a second sample, not a replacement for `samples/minimal`

`minimal` answers "how do I start a project on this engine" and is meant to be
copied — ARCHITECTURE says starting a game *is* copying it. LineSweeper answers
"what does a finished game look like" and is meant to be read. A sample large
enough to answer the second question is a bad answer to the first, and the
instruction to copy it stops being true the moment the file count goes up.

The practice this follows is near-universal in comparable libraries: raylib,
SDL3, SFML, Allegro 5, bgfx, Ogre3D and Urho3D all keep samples in the engine
repository, and Allegro splits them into `examples/` for one-feature programs
and `demos/` for complete games on exactly this line. The engines that moved
samples out — Godot, O3DE, Flax — moved them for **bytes**, not for code:
O3DE's sample projects were 6+ GB. This sample's content is a font and a JSON
file. If it ever grows an art budget into the megabytes, that is the signal to
revisit, and it is the only such signal in the field.

### The rules are a static library that links nothing

`LineSweeperRules` links `labrador_settings` — an INTERFACE target carrying
compiler flags and no libraries — so there is no way for it to reach an engine
symbol. An `#include "engine/..."` that resolves still fails at link.

That wall buys one property, and it is the one worth having: **the whole game
is playable inside the test suite**, with no window, no device and no renderer.
A rule can be asserted rather than played. It is also what makes the replay
test possible at all, since a headless test cannot construct an `Application`.

A static library rather than a folder for the same reason ARCHITECTURE gives
for LabradorMath being one: when a wall has to be load-bearing rather than a
convention, the escalation is cheap and this is it.

### Three layers, and the middle one may not see the rules' verbs

- `rules/` — the simulation. Includes nothing from `engine/`.
- `presentation/` — reads a `World` and draws it. May include `rules/world.h`;
  may **not** include `rules/tick.h`.
- `states/` — the only place that includes both, and where input becomes a
  rules verb.

The second rule is what makes "read the presentation without learning the
rules" true rather than aspirational, and it maps exactly onto the engine's own
`update()` writes / `draw()` reads split.

Price, stated plainly: this is a wall in *this* repository. Someone who copies
the tree and deletes the check gets folders and no discipline. It teaches by
existing; it does not travel.

### The match is one value, and four asserts say what that means

`World` is 276 bytes: five 32-bit counters, sixteen single-byte flags and
timers, the active piece, the queue, and 220 bytes of cells. It is trivially
copyable, trivially destructible, and has no padding bits.

Restarting is `world = World{}`. Snapshotting is `World snapshot = world;`.
Comparing two matches is `std::memcmp`. None of those needed a line of code
written for them, and that is the T11 demonstration this sample exists to make.

The fourth assert is the one worth reading:

```cpp
static_assert(std::has_unique_object_representations_v<World>);
```

It is the standard's own name for "no padding bits", which is what makes
`memcmp` a defined comparison rather than a hopeful read of indeterminate
bytes. It is *also* false for `float` and `double` — so the same line keeps the
simulation integer-only, and fires on the day somebody adds a float "just for
the lock timer", which would have made every replay depend on `/fp:precise` and
on this compiler. One trait, two invariants.

### Every duration is in ticks, and `main.cpp` pins the tick rate

Lock delay, the shift timer and the gravity curve are all counted in fixed
steps, never in seconds. That is what keeps a replay independent of how long a
frame took.

It also means `options.target_fps = 60` in `main.cpp` is load-bearing rather
than decorative. Nothing in the engine pins it — `ApplicationOptions::validate`
only checks it is positive — so a game that sets 120 runs at half speed with no
error anywhere. The line carries a comment saying so.

### The tuning numbers are compiled in, and CONVENTIONS says they should not be

> A header full of numeric constants is usually tuning data in the wrong place
> — *which* and *how much* belong in JSON (T7).

`rules/tables.h` is a header full of numeric constants: fifteen gravity rates,
four timings, three score tables and eighty wall-kick offsets. The exception it
claims is the wall the layer is built on. `rules/` links no engine, so it has no
JSON parser, no asset system and no file access — and even if it had, a rule set
that could be edited from disk would be a rule set a recording could not be
replayed against. A match is only reproducible against the tables it was played
on, so the tables change by commit.

That is a real cost and it is worth naming: retuning the shift delay is a
rebuild, not an edit. It buys a replay that is a `std::vector<std::uint8_t>` and
nothing else.

### The shapes are one formula; only the kicks are a transcription

A clockwise turn inside a box of side *n* takes the cell at (x, y) to
(n − 1 − y, x). Seven spawn rows and that line generate all twenty-eight
states, and four `static_assert`s check the result against published pictures.
The alternative — twenty-eight hand-typed matrices — is twenty-eight chances to
put one square in the wrong place, and nobody checks all of them.

It also turns the O piece's standing invariant into a structural fact rather
than an asserted one. O's box is **two** wide, and on a box of side two the
formula maps the four cells onto themselves; the obvious three-wide box slides
the piece a column every turn.

The kick table gets the opposite treatment, because it is data and not a rule:
it is written out in the convention the sources print it in, y positive
**upward**, so a reader can hold the file beside the published table and compare
rows without arithmetic. One `constexpr` function flips the sign of y on the way
into the tables the code reads, and two asserts fire if that flip is ever
deleted.

### The random source is a counter, so a snapshot restores the deal

`World::rng` is a SplitMix32 *position*, not a register: the field is
incremented by a constant and the bits come from mixing it. Two consequences,
and the second is the one that mattered.

A snapshot restores the piece that was coming next, for free, because the
position travelled with the copy. And zero is a legal seed — an xorshift, which
is the obvious thing to reach for, is stuck at zero forever from a zero state,
so `World{}` would have dealt no pieces at all. A default value that cannot
start a game is not the value semantics this sample is arguing for.

### The padding assert priced a rule out, and the rule changed

The guideline returns a piece's fifteen lock-delay resets when it reaches a row
lower than any it has occupied. That needs one byte of low-water mark, and one
byte is what `World` cannot afford: at 276 bytes the value has no padding at
all, and 277 would be padded up to 280 —
`has_unique_object_representations_v` fails, `memcmp` stops being a defined
comparison, and the replay test loses its footing. **New state in this
simulation costs four bytes or nothing.**

So the resets come back on any downward move instead. The difference is a player
who kicks a piece upward and lets it fall to farm resets, which costs them more
lock delay than it buys. That is an assert doing design work rather than
checking it, and it is the clearest thing in the sample about what a value
semantics commitment actually feels like from the inside.

### T-spins are in, and they pay for the kick tables

T-spins were initially cut: about seventy lines, plus the only cross-verb state
in the simulation (whether the last successful action was a rotation), to feed
scoring branches a solo player does not compete over.

They are in because a falling-block game shipping without them reads as
unfinished to anyone who plays the genre, and this is meant to be a real client
rather than a rules exercise. Two consequences worth recording:

- **Lock delay stops being optional.** The whole move is rotating the piece
  into the notch *after* it has landed, so with no lock-delay window T-spins are
  unperformable. The half-second-and-resets rule can no longer be trimmed on
  simplicity grounds without silently deleting the feature.
- **The kick tables earn their keep.** The SRS wall-kick table is the largest
  block of incidental complexity in the rules and the one part a reader must
  check line by line against a published source. Without T-spins it is
  conformance for its own sake and a simpler rotation system would be defensible
  on T3 grounds. With them, the kicks *are* the mechanism that puts the piece in
  the slot.

Back-to-back came back with them, because it is what makes a T-spin worth
setting up rather than merely worth more points.

### The whole screen is one white texel

There is no `draw_rect` on the renderer seam, and there should not be one: a
solid rectangle is a sprite, and the seam draws sprites. So the content grows a
132-byte `white.dds` — one opaque texel — and every block, panel, grid line and
banner backing on the screen is that texel with a tint on it.

That keeps "this sample's content is a font and a JSON file" nearly true, which
is the sentence the whole in-tree-samples argument rests on. It is also the
honest shape of the eventual answer: when the particle field lands, the glow is
an atlas, and an atlas is this file with more in it.

### The landing shadow is an outline

The shadow under a falling piece is the genre's landing aid, so it has to be
seen by a player with low vision. The target is WCAG 1.4.11's 3:1 for a graphic
the player needs: the shadow's visible edge against the empty well
(`well_colour`, 12, 14, 18), for all seven kinds.

A faded fill cannot meet that and still look unlike a piece. The shadow used to
keep 28% of the piece's colour, which is 1.28:1 for J and 2.02:1 at best for I.
J at full strength is only 3.62:1, so a fill reaching 3:1 would keep about 88%
of the piece and read as a second one. So each shadow cell is a two-pixel
outline in the piece's own full-strength colour around an inside that keeps
28%. The edges come out at 3.62:1 (J), 4.00 (T), 4.76 (Z), 8.46 (L), 9.33 (S),
12.63 (O) and 13.54 (I). The faint inside, a fill alpha below 1, is what keeps
it distinct from the opaque live piece of the same kind.

`palette.h` owns the well colour, the grid line and both shadow colours, and
`tests/linesweeper/palette_tests.cpp` checks every kind's edge in every palette
against the well, and against the one-pixel grid line drawn over it just
outside each cell, through the premultiplied blend equation. The grid line is
slightly lighter than the well (24, 26, 30 once blended), so it is the harder
of the two. A palette edit that drops a kind below 3:1 against either fails
`LineSweeperViewTests`. The edges against the well, then the grid line:

| Palette | Lowest against the well | Lowest against the grid line | Highest |
|---|---|---|---|
| Standard | 3.62 (J) | 3.26 (J) | 13.54 (I) |
| Protanomaly / Protanopia | 3.67 (T) | 3.31 (T) | 15.92 (O) |
| Deuteranomaly / Deuteranopia | 3.66 (S) | 3.30 (S) | 15.87 (O) |
| Tritanomaly / Tritanopia | 3.69 (T) | 3.32 (T) | 14.34 (I) |

Those tests check constants. `tests/linesweeper/board_view_tests.cpp` checks
the drawing under the null renderer: every piece, shadow edge and shadow fill
the board submits is the selected palette's colour for its kind. How the
outline looks is the captures', where it was looked at. Two limits remain. A
phone's scaled-down board draws the two-pixel edge at about one pixel. Nobody
has yet tested the shadow with low-vision players.

### Colour-vision palettes

The genre's seven colours are hard to tell apart for some players. Simulated
for protanopia, the standard J and T come out 2.8 apart by CIEDE2000 (method
below), where the closest standard pair for a viewer with typical colour vision
is 20.1 apart. So the sample has three more palettes, one for each group
BDP-25 names, chosen from the pause menu (*Accessibility*, above). A palette
changes how the pieces look and nothing else: it is not in `World`, not in a
recording and not an input to `tick()`. `PlayState` owns the selection and
lends it to the board and the particle field, and the pause menu is the one
place that writes it (`presentation/palette.h`).

| Kind | Standard | Protanomaly / Protanopia | Deuteranomaly / Deuteranopia | Tritanomaly / Tritanopia |
|---|---|---|---|---|
| I | 0, 240, 240 | 80, 255, 220 | 16, 252, 212 | 24, 248, 228 |
| J | 0, 96, 240 | 0, 116, 255 | 40, 116, 228 | 0, 140, 196 |
| L | 240, 152, 0 | 212, 132, 8 | 196, 144, 96 | 188, 100, 56 |
| O | 240, 208, 0 | 244, 240, 0 | 255, 236, 0 | 204, 224, 28 |
| S | 0, 208, 80 | 0, 132, 120 | 0, 126, 0 | 104, 116, 44 |
| T | 168, 48, 240 | 204, 24, 124 | 244, 140, 255 | 148, 40, 255 |
| Z | 240, 48, 48 | 244, 180, 224 | 188, 64, 120 | 244, 180, 220 |

**Method.** Each of the three was held to two limits that tests enforce:

- every kind's landing-shadow edge is at least 3:1 against the well and against
  the grid line, by WCAG's relative luminance (*The landing shadow is an
  outline*);
- no kind is a saturated red by the flash check's measure, linear
  `R / (R + G + B)` below 0.8. The highest are 0.74 (protan L and T), 0.68
  (deutan Z) and 0.75 (tritan L). The standard Z is 0.94, and the standard
  palette is left as it was.

Inside those limits, each palette was searched to keep its seven as far apart
as possible for its own group. Every pair was simulated and converted to
CIELAB, and the pair's difference measured by CIEDE2000. The search started
from hand-picked values, moved them locally, and then raised the darkest kinds
until their edges also cleared the grid line, not only the well. Three
simulations were used: Machado, Oliveira and Fernandes (2009) at severity 1.0
(dichromacy) and 0.6 (a moderate anomaly), and, for protan and deutan,
Viénot, Brettel and Mollon (1999). The closest pair in each palette:

| Palette | Typical colour vision | Own group, Machado 1.0 | Machado 0.6 | Viénot |
|---|---|---|---|---|
| Standard, simulated as protan | 20.1 (L/O) | 2.8 (J/T) | 5.4 (J/T) | 4.6 (J/T) |
| Standard, simulated as deutan | | 6.4 (J/T) | 4.2 (J/T) | 6.8 (J/T) |
| Standard, simulated as tritan | | 11.5 (I/S) | 20.2 (L/O) | - |
| Protanomaly / Protanopia | 32.2 (T/Z) | 22.8 (J/T) | 25.1 (L/O) | 22.5 (J/T) |
| Deuteranomaly / Deuteranopia | 27.6 (T/Z) | 20.6 (L/S) | 21.5 (L/S) | 20.7 (L/S) |
| Tritanomaly / Tritanopia | 33.7 (O/S) | 25.3 (I/J) | 29.3 (I/O) | - |

The three palettes were drawn by `LineSweeperCapture --palette`
([its README](../../tools/linesweeper_capture/README.md)) and every still was
looked at. The printed tick, score and lines matched the standard run's in
every palette. They were also checked against WCAG 2.3.1 (*The bursts against
WCAG 2.3.1*). Their captures are review material and are not on the website.

**Limits.**

- **Simulation is a development aid, not evidence that a player can tell the
  pieces apart.** The models describe an average observer of each type, and
  nobody with a colour-vision deficiency has played these palettes. Player
  validation remains separately identified, as
  [XAG 103](https://learn.microsoft.com/en-us/gaming/accessibility/xbox-accessibility-guidelines/103)
  asks.
- **Each palette is for its own group only.** Simulated as deutan, the protan
  palette's S and T are 4.7 apart. Simulated as protan, the tritan palette's L
  and S are 2.5 apart, and simulated as deutan its I and Z are 2.7 apart.
  Choosing the wrong palette can be worse than choosing the standard one.
- **Tritan rests on one model.** Viénot's method was applied to protan and
  deutan only, so there is no second simulation to cross-check the tritan
  palette.
- **Shape is still the main cue.** Every kind has its own shape in the well, the
  hold slot and the queue. The palettes make colour a better second cue; they
  do not make it the only one.
- **They depart from the genre's colours.** Z is not red in any of the three,
  and S and T change too. A player who knows the genre by colour will have to
  relearn which is which.
- **Only the pieces change.** The HUD, the menu, the frame and the top-out
  banner are the same in every palette. Nothing saves a selection or reads the
  system's colour settings. The browser page offers no way to choose one
  before play: that would widen the bundle's factory contract.

### Input is read as held, never as pressed

The engine's `Keyboard` computes press edges of its own, and `states/` ignores
them. It reads `held()` for every binding in both tables — nine keys and nine
pad buttons — ORs the lot into one byte, and lets `tick()` derive every press by
comparing that byte against the previous one. Which device set a bit is not
recorded and could not be: the byte is the whole of what the simulation sees,
which is also why a player may use both at once and nothing has to arbitrate.

Using the engine's edges would work, once. What it would cost is the property
the rules layer exists for: an edge computed outside the simulation depends on
which frames the window had the keyboard, so a recorded hard drop could replay
as two or as none. Deriving edges *inside* `tick()` means the recording carries
them, and a `std::vector<std::uint8_t>` is a complete match.

Restart is the exception that proves it. `update()` reads `pressed()` for R and
for Start, because restarting is not an input to `tick()` — it replaces the
value `tick()` runs on — so that edge falls outside the recording and is free to
be an edge. A held R would otherwise restart sixty times a second.

### There is a pause screen, and the game did not need one

The same screen protects a match when its controls disappear. Keyboard focus
loss pauses before the next rules tick; deactivation also pauses immediately,
so hiding and returning to a browser tab without an intervening frame cannot
resume a falling piece. Returning focus leaves the menu open until the player
chooses to resume. A covered match cannot stack another pause menu, and the
top-out screen stays a finished match. Pads still follow page visibility,
not canvas focus. Native null-render/null-audio tests exercise these state
transitions with a hidden window and synthetic input; the updated packaged
browser cases remain to be run.

**The objection first, because it is the real one.** T1 points the other way:
LineSweeper does not need a pause screen. Its README argues restart-as-one-
assignment as a virtue and the match is already interruptible by closing the
window. A menu built so that a module has a client is a menu built for the
engine's benefit, which is the thing this document is supposed to catch.

It is here anyway, and the defence is narrow enough to write down. `engine/ui/`
is roughly 1,350 lines across eight files and
`grep -rn "engine/ui/" samples/` was **empty** — the module's only client left
with the split. `PHILOSOPHY.md` calls the samples "the permanent second client
that keeps the boundary honest (T1)", and on this one module that client did
not exist. A promise a document makes on a module's behalf is not kept by four
`StubWidget`s in a test. `docs/survey/2026-08-26.md` section 3.3 is the finding.

**A stub is not a client, and the difference showed up immediately.** A stub
reports whatever bounds the test asks for; a `UiText` reports the box the font
measured, and directional navigation is arithmetic over exactly those boxes.
The rows navigate and wrap because the measurements are real, which is a
thing only a client can demonstrate.

**Nothing in `engine/` changed.** That is the outcome the survey predicted and
it is recorded as a result rather than assumed: the widget set, the focus
group, the navigation walk and the state stack were all used as they shipped.
Two things the engine already had turned out to be load-bearing and had never
been exercised:

- **`State::covers_screen()` returning false** is the whole of "keep drawing
  the match underneath". `state_context.h` names a pause menu as its worked
  example twice; this is the first one that exists.
- **`StateContext::pop` is queued, not immediate**, when called from inside a
  state's own update. That is what makes it safe for a button's action to pop
  the state — the action, the `FocusGroup` holding it and the state itself are
  all destroyed by that pop, and would be destroyed *during* the call if it
  were not deferred. `tests/core/state_context_tests.cpp` already pins it.

**The layout is manual and stayed manual.** `PHILOSOPHY.md` refuses an
autolayout engine, so the rows are y-coordinates and a spacing
constant. The one thing not done by hand is centring: each string is measured
once in `init()` through `RenderResources::measure_text`, and the COLOURS row
again each time it changes. The HUD in
`board_view.cpp` columnises by counting characters and says it only works
because the font is monospaced — true, and not a habit a menu with rows of
different lengths should inherit.

**The scrim is not a widget**, and that is the only place the widget set did
not fit. A full-screen dimming quad has no focus, no colour a cursor changes
and nothing to navigate to; making it a `UiWidget` would put a destination in
the walk that swallows every press. `UiTexture` is the leaf that would
otherwise fit and it takes a sheet name and a frame name, which this sample has
not got — one white texel and a font is the whole of its content. Fifteen lines
of `GameObject` in `pause_state.cpp` is cheaper than a sprite sheet in the
manifest.

**The stick works, and it is the half that took an engine change.**
`engine/ui/navigation.h` said `Direction` was "produced by the input module
from a stick or a d-pad", and
`grep -rn Direction engine/ | grep -v engine/ui/` was empty: **no such producer
existed**, which is a header stating a false fact about another module and a
defect independent of whether any game wants a menu.

The first version of this screen wrote the mapping itself, and it was cheap for
one reason only: it read the keyboard and the d-pad, which are edge devices, so
it needed neither a deadzone nor a repeat. Nine obvious lines. Adding the stick
is what makes the other two thirds appear, and that is where the mechanism
earned its place rather than being guessed at (T1).

What it costs here now is `pad_direction(gamepads()->state(0))` and one
`DirectionRepeat` member. The deadzone is radial and behind the call, the
quadrant test is behind it, and the repeat is a long delay then a short
interval — the third being the one every client writes wrong, in one of three
recognisable ways `engine/input/direction.h` lists. The keyboard is still read
here, and should be: which key means up is a binding, and a binding is the
game's.

### Additive blending needs no engine change

The engine looked as though it could not blend additively: it opened every batch
with a null blend state and the word "blend" appeared nowhere in
`engine/render/`. That reading was wrong, and the correction matters because the
whole visual pitch rests on it.

DirectXTK substituted `CommonStates::AlphaBlend()` for a null blend state, which
is `SrcBlend = ONE`, `DestBlend = INV_SRC_ALPHA`, `BlendOp = ADD` —
**premultiplied** alpha, not straight alpha. The blend state is the engine's own
now (`renderer.cpp`, one descriptor, and `RenderPixelTests` pins it), and it is
those same three values, deliberately. The equation is
`dst = src.rgb + dst.rgb * (1 - src.a)`, so a texel authored with `a = 0` and
`rgb > 0` adds with no attenuation. Glow is an atlas decision, not an API gap.
The same one state also expresses opaque drawing, a darkening vignette and a
full-screen colour grade.

A `BlendMode` on the seam is therefore **declined**, and not only as
unnecessary. `set_filter` closes and reopens the batch on every change, because
a sprite batch cannot swap state mid-`Begin`; a blend selector would inherit
exactly that, and a caller toggling it per particle would turn a handful of
draw calls into thousands. That is the frame-loop tax T8 refuses.

What is genuinely lost, rather than worked around: glowing **text**. `draw_text`
puts its glyphs in the same batch as everything else, and the tool that builds a
`.spritefont` will not write `a = 0` glyphs. The answer is a soft quad behind
the text, which reads better at this resolution anyway — but it is a workaround
and is named as one.

`board_view.cpp` already has the quad, and it earned its place for the second
reason before the first: the top-out banner is red words over whatever colour
the sparks the stack became happen to be under them, and no blend mode makes
that legible. The quad is premultiplied black at 88%, so an eighth of what is
under it shows through and it reads as a banner over the well rather than a
hole in it. `faded()` in
`palette.h` is the two multiplications a premultiplied tint costs a caller, and
is the only place in the sample that knows the blend equation.

### The particle field

This section used to be called *Particles are not in the first version* and
said the sample demonstrated the value-semantics half of what it is for and
not the data-layout half — "a 276-byte match, a restart that is an assignment
and a replay that is a `memcmp` are all here; ten thousand particles laid out
for the cache are not. Deferred, not dropped." **They are here now**, and the
five decisions behind them are below. `docs/survey/2026-08-26.md` section 3.2
is the item.

**One `GameObject`, ten thousand particles.** `PHILOSOPHY.md`, The object
model, says both ends of the spectrum are first-class and that a game may
register one object standing for thousands of values rather than one per
entity. Nothing in this repository stood at that end. `ParticleField` is one
registration, one `update()`, one `draw()` and one `bounds()` — three virtual
calls a frame whatever the count, against thirty thousand and ten thousand heap
allocations for the obvious shape.

**It asked the engine for nothing.** No blend mode, no instancing verb, no
particle system, no backend state and no golden image: ten thousand
`draw_sprite` calls through the verb `renderer.h` already had. An engine-side
particle system would have been the speculative framework T1 rules out — the
mechanism the engine owes a game here is a batched sprite draw, and it already
owned it. The only new engine-facing fact is that a sample can stand at that
end of the dial without the engine noticing.

**AoS, and thirty-two bytes.** The reflex for "laid out for the cache" is
structure-of-arrays, and it is the wrong reflex here: `update()` writes
position, velocity, life and decay and `draw()` reads position, life, size and
kind, so between them they touch every field of every live particle every
frame. Six streams would buy nothing. What was chosen instead is the size -
exactly two particles to a cache line, none straddling one, one allocation of
320 KB at construction and none after. Twenty-four bytes was reachable by
packing three floats into indices behind lookup tables and is declined on T3:
a sample meant to be *read* does not trade legibility for a fraction of a
microsecond. Dead particles are overwritten by the last live one, so the array
is a dense prefix and both loops stream.

**The glow needed no atlas, and this file predicted it would.** *Additive
blending needs no engine change*, above, closes by saying "when the particle
field lands, the glow is an atlas, and an atlas is this file with more in it."
That turned out to be wrong in the cheap direction. Under premultiplied alpha a
tint with `a = 0` adds without attenuating, so `glowing()` in `palette.h` is the
whole of it: a shrinking quad of pure addition reads as a spark at four pixels
across, overlapping ones saturate towards white, and the content is still one
white texel. The prediction was that soft radial dots were needed; at this size
they are not.

**Between two steps the sparks are carried on, and nothing else is.** The
match steps at 60 Hz and a 144 Hz display draws each step two or three times,
so anything drawn straight from the last update repeats a position and then
jumps. The field borrows the shell's `Application::step_position` and draws
each spark that share of a step further along its velocity; gravity's part of
less than one step is a quarter of a pixel. The board does not do this and must
not: a piece is in one cell or the next, and a piece drawn between cells is a
position the rules never held. The field carries on only when it was updated on
the latest step, because under the pause menu the match has stopped while the
step count goes on, and a field extrapolated there shimmers.
`LineSweeperViewTests` holds that rule; the null configuration's draw test holds
that a carried spark stays inside the bounds the cull reads. The capture tool
and the frame bench pass no step and draw what they drew before.

**It learns what happened by keeping last frame's match and looking.** There is
no event queue, no callback and not one line in `rules/` that knows the field
exists. The field holds a `World` by value, compares it with the live one every
frame, and reads the locks, the clears, the top-out and the restart out of the
difference. That is only affordable because a match is 276 trivially copyable
bytes — an object graph would have needed the bus. It is the same argument
*The match is one value* makes, arriving from the other side, and it is the
strongest thing in the sample about what value semantics actually buy.

#### The one place the diff is not enough, and what it cost

**A tick locks and clears together.** `tick.cpp` writes the piece into the
cells and calls `clear_lines` in the same step, so a full row is never visible
from outside the simulation — not in last frame's match, not in this frame's. A
field looking for one finds none, ever. That was found by writing the obvious
thing first and watching it never fire.

So the board between the lock and the clear is rebuilt: last frame's cells,
plus the piece as it locked. **Where the piece as it locked comes from is the
part this section got wrong once.** It was reconstructed, from `shadow()` of
last frame's match — where a hard drop would have put the falling piece, which
is where it locked in every case but one: a piece that moved sideways or
rotated on the very tick it locked. This section said the failure mode there
was that no row came back full and no sparks were thrown, and closed with "a
missed burst on a rare frame is a cost worth paying; a burst on the wrong row
is not." The second half of that sentence was the actual failure. A horizontal
I rotated and hard-dropped in one tick locks vertically and clears row 21; the
horizontal shadow completed row 20; two hundred and twenty sparks came out of a
row that was still on the board (`docs/review/gpt6/README.md`, G6-11).

**The exact answer was priced and refused, by the same assert that priced a
rule out once already — and then bought another way.** A field on `World`
naming the rows that went would cost four bytes, not one: 277 pads to 280, and
`sizeof(World) == 276` and `has_unique_object_representations_v` both fire.
*The padding assert priced a rule out* records that trade being made for a
rule. This is it made again for an effect, and an effect has even less claim on
the value than the lock-delay low-water mark did — so the value is untouched.
`tick()` returns the piece as it locked instead (`world.h`, `TickResult`, four
bytes, `Kind::none` on the ticks that lock nothing), the state keeps the
latest one beside its `World`, and the field borrows a pointer to each. A
return value costs the match nothing: it is not in the memcmp, not in the
replay, and a replay produces the same one on every tick because it is a
function of the same bytes. The field is still a reader and there is still no
bus; what changed is that the one fact the diff cannot recover is now handed to
it rather than guessed at.

#### What it costs, measured against what was predicted

`bench/render_bench.cpp` puts the engine's quad arithmetic at **35.4 ns a
sprite** and flat from a thousand sprites to sixty-five thousand
(`docs/survey/2026-08-26-status.md`, section 3.1a). Ten thousand particles is
therefore about **354 microseconds** of arithmetic in a 16.7 ms frame — a
fiftieth of the budget — which is why the field has no spatial index, no sort
and no second submission path. That prediction is what section 3.2 was told to
wait for section 3.1 to produce, and it is the reason the cost of this feature
was known before a line of it was written.

It remains a prediction about arithmetic rather than a frame time. The
reference machine is named now — `PHILOSOPHY.md`, Performance — and it is still
unmeasured, so no number here is a floor. See *Still open*, which says which
half of the claim landed and which did not.

#### The banner had to leave the board view

`board_view.h` argues that the well, the shadow, the hold slot, the preview and
the numbers are one object because they are one read of one value. The top-out
banner was the sixth and it is now its own file, because every draw in this
sample is at `layer_depth` 0 and object order is the only depth there is. The
top-out is the loudest burst the field throws — forty-eight particles for every
filled cell of the well — and it is thrown on exactly the frame those words
appear. Drawn from inside `BoardView` they were unreadable for the first second
of the one screen a player has to read; that was seen on screen, not reasoned
about. The split is bought by an ordering constraint rather than by tidiness,
which is the only thing that argument leaves room for.

### The bursts against WCAG 2.3.1

A burst is thousands of additive sparks thrown on one frame, and the hosted
page puts it in front of anybody who follows a link. So the line clears and
the top-out are checked against WCAG 2.3.1, *Three Flashes or Below
Threshold*, and **they are held to its strict reading: any pixels in the field
flickering at any time in the second count together.** That is the owner's
decision of 2026-10-13. On that reading the top-out as first written failed,
so the calmer one below is the default for every player, on the desktop and in
the browser, rather than an option for those who ask (BDP-22 is that option,
and now goes further than a default that is already safe). Of the three
top-out brightnesses measured, the owner looked at the calmest and chose the
middle one the same day: a top-out heat of 0.55.

**The reading.** WCAG counts the area of flashes "occurring concurrently" and
does not define the word. Read loosely, a field where sparks brighten some
pixels and darken others within the same few frames has made neither change,
because it has not brightened or darkened as a whole. Read strictly, every
pixel's own flashes count, whenever in the second they happen, and more than
a quarter of a field at more than three is a failure: that is the *pixel
bound* below. The strict reading is the one taken. WCAG's own definition
points the same way: it exempts balanced flashing only as "a fine, balanced,
pattern such as white noise or an alternating checkerboard pattern" with
elements under 0.1 degree, about four pixels at the assumed field, and a spark
is born larger than that.

**What failed.** With the stack left standing and every spark born at full
brightness, the top-out was 8 flashes a second by the pixel bound, in frames
675-734 of [`flash.txt`](../../tools/linesweeper_capture/flash.txt)'s run and
1961-2020 of `footage.txt`'s. For half a second after the shatter about a
fifth of the field brightened and a fifth darkened on every frame, as sparks
crossed it. The line clears passed at 2. Most of the top-out's flicker was
sparks crossing the stack: under additive blending a block near the top of the
sRGB range climbs steeply in linear light, so the same spark that barely
changes the dark well flashes a bright block. Dimming the sparks alone left
the top-out at 4.

**What changed: one branch and three numbers.**

- **A topped-out match draws no stack** (`board_view.cpp`, `draw_stack`). The
  field throws every cell of it as sparks on that frame, so the stack becomes
  the sparks and the well behind them is dark. The cells are still in the
  `World`; nothing in `rules/` changed, and a restart draws the empty well as
  before.
- **Each burst names the heat its sparks are born at** (`particles.h`,
  `lock_heat`, `clear_heat` and `shatter_heat`). A spark adds its colour times
  the square of what is left of its heat, so a heat is a brightness ceiling. A
  landing keeps 1, a clear is born at 0.75 and a top-out at 0.55, about three
  tenths of the light its sparks had. A spark's side shrinks with its heat as
  well, so a top-out spark is also born at about seven tenths of its old size.
  The
  top-out's sparks are also slower (160 pixels a second, from 420) and live
  longer (up to 2.2 seconds, from 1.6), so a spark that dim stays on screen
  long enough to read as the stack settling. The count did not change: a full
  well is still 9,600 sparks, the workload
  `LineSweeperFrameBench` freezes.

Heat is the term the result moves with. With the stack hidden, the top-out
counted 1, 1.5 and 2 flashes by the pixel bound over frames 640-988 at heats
of 0.5, 0.55 and 0.6, and halving the speed moved it by half a flash. 0.5
is the calmest of those and 0.6 the brightest inside the target of two; 0.55,
between them, is the one the owner chose, and the results below are for it.
The clears counted 2 at a heat of 1 and 1 at 0.75, which is also what the same
frames count with no particle drawn at all. `LineSweeperViewTests` pins each
heat to the value measured here, so a brighter burst fails there until this
check is run again.

**Method.** `LineSweeperCapture --sequence` wrote every 60 Hz presentation
update of two checked-in matches as a PNG, and
[`tools/flash_check/flash_check.mjs`](../../tools/flash_check/flash_check.mjs)
read them. `flash.txt` is the website's match as fast as a script can play it
- four tetrises, the second and third back to back, one single and the
top-out, then five seconds of the burst decaying: 989 frames, the shatter on
frame 687. [`footage.txt`](../../tools/linesweeper_capture/footage.txt) is the
same match at the website video's pace: 2,095 frames, the shatter on frame
1973. Both were drawn by the x64-release D3D11 build on an NVIDIA GeForce RTX
5080. The *Before this change* row and the run with no particle drawn are from
the tree of the commit that added this paragraph; every other figure below was
measured again on 2026-10-28, beside the colour-vision palettes' runs.

```bat
out\build\x64-release\tools\linesweeper_capture\LineSweeperCapture.exe out\flash tools\linesweeper_capture\flash.txt --sequence
node tools\flash_check\flash_check.mjs out\flash\frames
```

The analysis applies WCAG's relative-luminance formula to every pixel of
every frame. A general transition is a change of 0.1 or more where the darker
state is below 0.80, followed from each pixel's last extreme so a ramp counts
once; a red transition is a change of more than 20 in `(R - G - B) x 320` at a
saturated red, `R / (R + G + B) >= 0.8`, both on linear channels. That red
measure is the Understanding document's working definition. WCAG 2.2's own
note on saturated red uses a different measure, a difference of more than 0.2
in CIE 1976 UCS chromaticity, and that measure has not been run here. The area
is more than 25% of a 10 degree field, taken as a third of the frame each way
(427x240), as WCAG's 341x256 is of 1024x768 - which assumes the game fills a
screen seen from the distance that equivalence assumes. Every one-second
window is counted, at every position of the field. The tool reports the
concurrent count beside the pixel bound; the pixel bound is the verdict. The
file's header has the full definitions and
`node --test tools/flash_check/flash_check.test.mjs` holds them to synthetic
frames. The CI workflow's Windows `lint` job runs that suite on every push
CI handles and every pull request (`CI#2`), beside the offline tooling tests.

**Result**, the worst one-second window in each run, in flashes:

| | `flash.txt` | `footage.txt` |
|---|---|---|
| General, pixel bound | 1.5, the top-out (window ending at frame 699) | 1, a clear (ending at 360) |
| Red, pixel bound | 0 | 0 |
| General, concurrent | 0.5 | 0.5 |
| Top-out alone (`--frames 686-988`, `1960-2094`), pixel bound | 1.5 (ending at 702) | 1.5 (ending at 1988) |
| Line clears alone (`--frames 0-685`, `0-1972`), pixel bound | 1 | 1 |
| Widest change in one field | 29% falling at frame 689 | 29% falling at 1975 |
| Before this change, general, pixel bound | 8, the top-out (frames 675-734) | 8, the top-out (frames 1961-2020) |

A flash is two transitions, so 0.5 is a single change. Concurrently the
top-out is one darkening, as the stack goes and its sparks start no brighter
than it was; pixel by pixel its sparks then brighten and darken a quarter of a
field three times in the next second as they settle, which is the 1.5. The
clears are 1. Both runs pass, `flash.txt` with a margin of a flash and a half
against the threshold and `footage.txt` with two, and both are inside the
target of two this change was held to.

**The assumed screen, priced.** `--edge 6`, which lets a concurrent change span
six frames, leaves the pixel bound, the verdict, where it was: 1.5 in
`flash.txt` and 1 in `footage.txt`. The concurrent count becomes 1 in
`flash.txt`, a line clear (window ending at frame 427), and stays 0.5 in
`footage.txt`. `--field 213x120`, the game seen at twice the size - a
large monitor up close, a phone at reading distance - counts 2 by the pixel
bound in `footage.txt` (window ending at frame 366) and 3 in `flash.txt`
(ending at 373), with no red flash over 1. The top-out alone counts 2 there
in both runs (ending at 707 and 1993). The 3 is not the particles: the
same frames drawn with the field's `draw()` returning at once count 3 in the
same window. It is pieces spawning and stepping across the top of the well
with no wait between them, which `flash.txt` does and a player cannot; at the
video's pace the same match counts 2. The concurrent count at that field is
1.5 in `flash.txt`, where it was 2.5 before, and 1 in `footage.txt`.

**The colour-vision palettes** (*Colour-vision palettes*) change every spark's
colour, so both matches were drawn again in each of them with
`--sequence --palette protan`, `deutan` and `tritan`. That was the same
x64-release D3D11 build on the same RTX 5080, on 2026-10-28. Every run wrote
989 or 2,095 frames, and in every run the stack goes on frame 687 or 1973, as
in the standard palette. The pixel bound, the verdict, in flashes:

| | Protanomaly / Protanopia | Deuteranomaly / Deuteranopia | Tritanomaly / Tritanopia |
|---|---|---|---|
| `flash.txt`, general | 1.5 (ending at 696) | 1.5 (ending at 697) | 1.5 (ending at 245) |
| `flash.txt`, red | 0 | 0 | 0 |
| `footage.txt`, general | 1.5 (ending at 1998) | 1.5 (ending at 1999) | 1 (ending at 360) |
| `footage.txt`, red | 0 | 0 | 0 |
| Top-out alone (`flash.txt`, `footage.txt`) | 1.5, 1.5 | 1.5, 1.5 | 1.5, 1.5 |
| Line clears alone (`flash.txt`, `footage.txt`) | 1, 1 | 1, 1 | 1.5, 1 |
| General, concurrent (`flash.txt`, `footage.txt`) | 0.5, 0.5 | 0.5, 0.5 | 0.5, 0.5 |

All six pass, inside the target of two. The top-out alone counts 1.5 in every
palette, as it does in the standard one; the heats are unchanged. The protan
and deutan `footage.txt` runs reach 1.5 in a window of the top-out, where the
standard run's worst window is 1, in a clear. The tritan line clears in `flash.txt` reach 1.5, half a flash over the
standard palette, in the window ending at frame 245. No palette has a saturated
red, so no red flash is counted at all, and `palette_tests.cpp` keeps it that
way. At `--field 213x120` the colour palettes count 3 in `flash.txt` as the
standard one does, and 2.5 in `footage.txt`, where the standard one counts 2.
The CIE 1976 UCS measure of red has not been run for these palettes either
(*Still open*).

### A phone held upright

The layout was a set of positions in 1280x720 back-buffer pixels, and a phone
is held the other way up. Two things were possible: a second arrangement of the
five panels for a tall screen, or the same positions seen through a different
camera. **It is the camera**, and `presentation/arrangement.h` is the whole of
it.

Everything in `presentation/` still draws in that one 1280x720 space, which is
now *board space* rather than buffer pixels. Every tick the state works out an
`Arrangement` from the buffer the shell reports - taller than wide is portrait -
and hands the scene's one view a camera:

- **Landscape** frames all of 1280x720, fitted and centred. At exactly
  1280x720 the camera is a scale of one and no translation, both exact, so the
  window `main.cpp` opens draws byte for byte what it drew before; the capture
  tool's four images were regenerated against the change and did not move. A
  window or a canvas of any other size now scales the whole screen to fit, where
  it used to draw at 1:1 in the top-left corner and leave the rest black.
- **Portrait** frames only the board block - HOLD and the score, the well,
  NEXT - plus a band underneath it for the touch controls, fitted to the
  buffer's width. The block is 550 units wide and 590 tall, nearly square, so a
  tall screen takes it whole with room below for two rows of thumb-sized
  buttons. The cost of not having a second arrangement is a well a little
  narrower than it could be - about 45% of a phone's width rather than the 60%
  a dedicated layout might give it - and that is the T3 trade: one space and two
  cameras, against two sets of positions to keep in step with each other and
  with the particle field that throws sparks out of cells.

The keyboard and pad hints are hidden in portrait, where the line is wider than
the block. The pause menu works out the same arrangement from the same buffer,
so it sits over the well either way up. A buffer that changes shape while the
menu is open re-arranges the menu at once and the match under it when the menu
closes, because a suspended state is not updated and `draw()` is const; the
scrim covers the difference.

**The buttons are the game's, drawn over the board.**
`presentation/touch_controls.h` is eight rectangles, a hit test and one
`GameObject` that draws them, registered last so a spark is never over a
button. It appears the first frame a finger touches the screen and stays: a
keyboard player never sees it, and the hints that name keys go when it comes.
The engine has a touch device and no binding layer (`docs/port/web.md`, P2),
so what a finger on a rectangle means is a third table in `play_state.cpp`,
`{Control, button}`, beside the other two.

**Held, not tapped**, for the reason *Input is read as held* gives: each frame
asks which rectangle every finger is in and ORs those controls into the same
byte the keys and the pad feed, so a thumb resting on LEFT is a held arrow key
with the same delay and the same repeat, which `tick()` owns. A finger that
slides from one button to the next lets go of the first. Pause and restart are
edges, as Escape and R are, and are read from the points that went down this
frame: pause from the PAUSE button, restart from a tap anywhere that is not a
button - not a tap on one, or a player hammering DROP as the stack reached the
top would restart a match they had not finished looking at. The banner says
"TAP TO RESTART" once a finger has been seen.

**The menu reaches touch through the game**, the way it would reach the mouse:
`engine/ui/` does not depend on `input`, so `PauseState` asks the device where
a new finger is, finds the row, puts the cursor on it and activates it. A row
answers a band the width of the menu, or its measured width and a margin when
that is wider, and the height of the gap between rows, because its measured
text is sixteen units tall - on a phone, a target a thumb misses.

**What is checked and what is not.** The arrangement's arithmetic and the hit
test are pinned headlessly in `LineSweeperViewTests`
(`touch_controls_tests.cpp`): the identity at 1280x720, both orientations on
phone-shaped buffers, the controls on screen, apart and clear of the board,
every control found under its own centre and nothing in the gaps. What the
controls look like, and that a tap rotates, a tap drops, a held LEFT repeats,
PAUSE opens the menu and a tap on a row chooses it, was checked by looking at
frames from headless Chrome at a 390x844 portrait viewport and an 844x390
landscape one with touch emulated. **No real phone has played it**, and the
9,600-particle top-out has not been timed on one.

## Development-page pacing instrument

The Release development page accepts `?pacing=play` for completed-frame CPU
timings during ordinary play, `?pacing=topout` for an exact 200-cell first
burst followed by decay, and `?pacing=topout-frozen` for the held birth frame.
The flags are opt-in pre-js on this page; the packaged module and phone gate
do not load that script. Without a flag the first state remains PlayState.

The fixture uses the current BoardView, ParticleField and TopOutBanner and
the benchmark's well/HUD values. Its named profile keeps the fixed landscape
hint labels and omits touch controls even in portrait, so it is not a complete
ordinary played-phone UI measurement. Its actual field must contain exactly
9,600 live particles and zero dropped emissions before its marked frame.
The first birth is held at zero elapsed time through that frame; evolving
decay and frozen captures have separate workload identities.

`Application::frame_timings()` is read by a separate rAF observer outside
fixed updates and draws. Completed frame ids are deduplicated, zero-update
frames remain visible and missed marked frames are reported as incomplete.
The raw console export records CPU phase times, engine intervals, observer
timestamps, source/build and geometry; physical display/refresh identity
requires an operator. [The method](../../docs/port/pacing-method.md) and
[tool instructions](../../tools/web_pacing/README.md) own capture/export and
results templates.

Browser runtime acceptance is still open under the recorded Chrome GPU
access denial. Headless ParticleField counts and Node protocol tests prove
neither rendered browser counts nor real display/phone performance. The
ordinary `LineSweeperPageTests` and the instrumented Release driver still
need their actual browser run before SM-08 can close.

## Still open

- **The reference machine — named now, and still unmeasured.** The
  hardware-floor claims this sample is eventually meant to prove need the
  configuration named by `PHILOSOPHY.md`, Performance, at the 1280x720
  `main.cpp` already asks for, with separate measurements for each claim.
  [The measurement decision (SM-03)](../../docs/performance/2026-09-19-g6f-ratchet.md#measurement-decision-sm-03)
  assigns separate p99s to CPU update, CPU recording/submission and frame-start
  cadence. Those measurements have not been taken on the Radeon configuration,
  so no number here is a floor. `LineSweeperFrameBench` is the vehicle for
  taking them: the actual board, banner and 9,600-particle top-out through one
  selected backend, with raw intervals and renderer phases retained. New
  low-tier runs use `--pacing presentation` with synchronised presentation;
  their frame-start interval measures cadence rather than display scan-out.
  GPU execution, presentation and input-to-display latency need their own
  stated measurement endpoints. The pipeline-depth latency claim remains
  unverified by the benchmark's CPU phases. The EC2 runner beside it supplies
  reproducible reference profiles; the Radeon measurements remain open.
- **WCAG 2.2's saturated-red measure.** *The bursts against WCAG 2.3.1* counts
  red flashes by the Understanding document's working definition, and both
  runs count none by the pixel bound, in every palette. WCAG 2.2's own note
  measures a red change as more than 0.2 in CIE 1976 UCS chromaticity, and
  that has not been run.
- **Whether the colour-vision palettes serve their players.** They were chosen
  by simulation (*Colour-vision palettes*). Nobody with a colour-vision
  deficiency has played them, and that validation
  ([XAG 103](https://learn.microsoft.com/en-us/gaming/accessibility/xbox-accessibility-guidelines/103))
  has no story yet.
- **Whether the layer rule gets a build check.** It is now checkable and
  unchecked, which is the worst of the three states, and `presentation/`
  has grown well past the three files it held when the rule was written: every
  view, layout and control the sample has added since is in it, so each new
  file there widens what review alone is holding.
  `cmake/check_engine_includes.cmake` is the model and the rule is two greps:
  nothing in `rules/` may include `engine/`, and nothing in `presentation/` may
  include `rules/tick.h`. Both hold today by review. The argument against
  writing it is that a sample's build check is a sample's build check — someone
  who copies the tree and deletes it gets folders and no discipline, which the
  layer decision above already says out loud.

- **Whether the input map belongs in the sample or the engine — now priced,
  and still open.** `states/` has two tables, `{Key, button}` and
  `{GamepadButton, button}`, nine entries each, and that is the whole input
  layer. The engine deliberately has no action-mapping layer: PHILOSOPHY's
  boundary table gives bindings and any rebinding to the game, and
  ARCHITECTURE's `input/` entry keeps the table on the game's side (T1). This
  sample reads only `held()` and rebuilds its edges from the combined level,
  which is one of the two rules for combining sources that a shared layer
  would have to choose between; its pause menu ORs each source's `pressed()`,
  which is the other.

  `docs/survey/2026-08-26.md` §5 asked for the second device to be priced
  before the question was argued, and for the threshold to be named **first**,
  on the grounds that a measurement whose threshold is chosen afterwards can
  only agree with whoever ran it. So it was: under 25 lines confirms the
  refusal; 60+, or an engine header having to change, overturns it; between
  them is inconclusive.

  **The pad cost 26 lines here** — an include, a five-line struct, an
  eleven-line table, a seven-line loop, one line to reach the pads and one more
  on the restart. Hints are excluded, being what a one-device sample pays for
  too. Nine of the twenty-six are binding rows, which any design pays for
  wherever the table ends up living; the mechanism is the other seventeen.
  `samples/minimal` paid 27 for the same change with no table at all.

  **So nothing overturned the refusal and nothing confirmed it.** One line over
  a threshold is not a result to round down, and it is recorded rather than
  argued away. What the exercise did settle is worth more than the count: no
  engine header changed, and the cost is per *device* rather than per binding —
  a third device is a third table and a third loop, not an edit to every row of
  the first two. An `InputMap` would have to beat that, and "it would be
  tidier" is not beating it.

  **The third device arrived, and the prediction held.** Touch cost
  `play_state.cpp` a seven-row `{Control, button}` table, its five-line struct
  and an eleven-line loop - the same shape and about the same size as the
  pad's - and no row of the first two tables changed. What it cost beyond the
  mechanism is the part no `InputMap` would have saved: the rectangles and the
  hit test, which are layout rather than binding, and the two edges a finger
  produces that a key does not have to be found for.
