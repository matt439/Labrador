# LineSweeper — design decisions

A falling-block game, and the sample that shows what a whole game on this
engine looks like. This file records the decisions behind it and the reasons,
so that a reader meeting the code cold does not have to reconstruct them, and
so that a decision cannot be quietly reversed without meeting its argument.

It is deliberately not a design *philosophy*. `docs/design/` holds those, there
are three of them, and this is not a fourth: everything here is a choice about
one sample, made under those three. Where a decision below is really an engine
commitment, it says so and names the document it belongs in.

**Status: playable, and both halves are here.** All three layers exist.
`rules/` is the whole game — the
seven-bag deal, the gravity curve, the wall kicks, lock delay, hold, hard drop,
T-spins and scoring — and `tests/linesweeper/` plays it with no window, no
device and no engine linked into the binary. `presentation/` draws it,
`states/` turns a keyboard and a pad into it, and pressing R or Start after a
top-out restarts the match with one assignment.

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
for MattMath being one: when a wall has to be load-bearing rather than a
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
`StubWidget`s in a test. `docs/next.md` section 3.3 is the finding.

**A stub is not a client, and the difference showed up immediately.** A stub
reports whatever bounds the test asks for; a `UiText` reports the box the font
measured, and directional navigation is arithmetic over exactly those boxes.
The three rows navigate and wrap because the measurements are real, which is a
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
autolayout engine, so the three rows are three y-coordinates and a spacing
constant. The one thing not done by hand is centring: each string is measured
once in `init()` through `RenderResources::measure_text`. The HUD in
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
the stack happens to be under them, and no blend mode makes that legible. The
quad is premultiplied black at 88%, so an eighth of the stack shows through and
it reads as a banner over the well rather than a hole in it. `faded()` in
`palette.h` is the two multiplications a premultiplied tint costs a caller, and
is the only place in the sample that knows the blend equation.

### The particle field

This section used to be called *Particles are not in the first version* and
said the sample demonstrated the value-semantics half of what it is for and not
the data-layout half — "a 276-byte match, a restart that is an assignment and a
replay that is a `memcmp` are all here; ten thousand particles laid out for the
cache are not. Deferred, not dropped." **They are here now**, and the five
decisions behind them are below. `docs/next.md` section 3.2 is the item.

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

So the board between the lock and the clear is reconstructed, out of the two
pure queries `world.h` declares for exactly this kind of reader: `shadow()`
gives where a hard drop would have put the falling piece, which is where it
locked in every case but one — gravity and soft drop lock a piece that is
already resting, and a hard drop locks it at the shadow by definition. The
exception is a piece that moved sideways or rotated on the very tick it locked,
and the failure mode there is that no row comes back full and no sparks are
thrown. **A missed burst on a rare frame is a cost worth paying; a burst on the
wrong row is not.**

**The exact answer was priced and refused, by the same assert that priced a
rule out once already.** A field on `World` naming the rows that went would
cost four bytes, not one: 277 pads to 280, and `sizeof(World) == 276` and
`has_unique_object_representations_v` both fire. *The padding assert priced a
rule out* records that trade being made for a rule. This is it made again for
an effect, and an effect has even less claim on the value than the lock-delay
low-water mark did.

#### What it costs, measured against what was predicted

`bench/render_bench.cpp` puts the engine's quad arithmetic at **35.4 ns a
sprite** and flat from a thousand sprites to sixty-five thousand
(`docs/next-status.md`, section 3.1a). Ten thousand particles is therefore
about **354 microseconds** of arithmetic in a 16.7 ms frame — a fiftieth of the
budget — which is why the field has no spatial index, no sort and no second
submission path. That prediction is what section 3.2 was told to wait for
section 3.1 to produce, and it is the reason the cost of this feature was known
before a line of it was written.

It remains a prediction about arithmetic rather than a frame time, and the
reference machine is still unnamed, so no number here is a floor. See *Still
open*.

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

## Still open

- **The reference machine — named now, and still unmeasured.** The
  hardware-floor claim this sample is eventually meant to prove needs a named
  part, a named resolution and a measured p99. Two of the three have landed:
  `PHILOSOPHY.md`, Performance names the configuration, and the resolution is
  the 1280x720 `main.cpp` already asks for. The p99 has not been taken, and it
  is the half that carries the claim. So what changed is the shape of this gap
  rather than its size — until that measurement exists, no number in this
  repository should be described as a floor.
- **Whether the layer rule gets a build check.** It is now checkable and
  unchecked, which is the worst of the three states, and `presentation/`
  has five files in it rather than three.
  `cmake/check_engine_includes.cmake` is the model and the rule is two greps:
  nothing in `rules/` may include `engine/`, and nothing in `presentation/` may
  include `rules/tick.h`. Both hold today by review. The argument against
  writing it is that a sample's build check is a sample's build check — someone
  who copies the tree and deletes it gets folders and no discipline, which the
  layer decision above already says out loud.

- **Whether the input map belongs in the sample or the engine — now priced,
  and still open.** `states/` has two tables, `{Key, button}` and
  `{GamepadButton, button}`, nine entries each, and that is the whole input
  layer. The engine deliberately has no action-mapping layer (CLAUDE.md,
  Known-absent), and one sample with no rebinding screen is not the second
  client that would justify one.

  `docs/next.md` §5 asked for the second device to be priced before the
  question was argued, and for the threshold to be named **first**, on the
  grounds that a measurement whose threshold is chosen afterwards can only
  agree with whoever ran it. So it was: under 25 lines confirms the refusal;
  60+, or an engine header having to change, overturns it; between them is
  inconclusive.

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
