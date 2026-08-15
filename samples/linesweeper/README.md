# LineSweeper — design decisions

A falling-block game, and the sample that shows what a whole game on this
engine looks like. This file records the decisions behind it and the reasons,
so that a reader meeting the code cold does not have to reconstruct them, and
so that a decision cannot be quietly reversed without meeting its argument.

It is deliberately not a design *philosophy*. `docs/design/` holds those, there
are three of them, and this is not a fourth: everything here is a choice about
one sample, made under those three. Where a decision below is really an engine
commitment, it says so and names the document it belongs in.

**Status: playable.** All three layers exist. `rules/` is the whole game — the
seven-bag deal, the gravity curve, the wall kicks, lock delay, hold, hard drop,
T-spins and scoring — and `tests/linesweeper/` plays it with no window, no
device and no engine linked into the binary. `presentation/` draws it,
`states/` turns a keyboard into it, and pressing R after a top-out restarts the
match with one assignment.

What is not here is the second half of what this sample is for: the particle
field, the glow and the instrument panel. See *Particles are not in the first
version*, below.

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

`LineSweeperRules` links `artattack_settings` — an INTERFACE target carrying
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
them. It reads `held()` for all nine bindings, packs them into a byte, and lets
`tick()` derive every press by comparing that byte against the previous one.

Using the engine's edges would work, once. What it would cost is the property
the rules layer exists for: an edge computed outside the simulation depends on
which frames the window had the keyboard, so a recorded hard drop could replay
as two or as none. Deriving edges *inside* `tick()` means the recording carries
them, and a `std::vector<std::uint8_t>` is a complete match.

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

### Particles are not in the first version

The first version is the game: rules, plain drawing, the state flow, the tests.
No particle field, no glow, no instrument panel.

That means what ships today demonstrates the *value-semantics* half of what
this sample is for and not the *data-layout* half. A 276-byte match, a restart
that is an assignment and a replay that is a `memcmp` are all here; ten thousand
particles laid out for the cache are not. Deferred, not dropped — the particle
field is where the second half lands, and it is the thing a modern
falling-block game's identity is actually made of, which is why this genre was
chosen over the alternatives.

## Still open

- **The reference machine.** The hardware-floor claim this sample is eventually
  meant to prove needs a named part, a named resolution and a measured p99. No
  such measurement exists. Until one does, no number in this repository should
  be described as a floor.
- **Whether the layer rule gets a build check.** It is now checkable and
  unchecked, which is the worst of the three states.
  `cmake/check_engine_includes.cmake` is the model and the rule is two greps:
  nothing in `rules/` may include `engine/`, and nothing in `presentation/` may
  include `rules/tick.h`. Both hold today by review. The argument against
  writing it is that a sample's build check is a sample's build check — someone
  who copies the tree and deletes it gets folders and no discipline, which the
  layer decision above already says out loud.

- **Whether the input map belongs in the sample or the engine.** `states/` has
  a nine-entry table of `{Key, button}` pairs and that is the whole input
  layer. The engine deliberately has no action-mapping layer (CLAUDE.md,
  Known-absent), and one sample with no rebinding screen is not the second
  client that would justify one. Recorded so that "add an InputMap" has to meet
  an argument.
