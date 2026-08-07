# ArtAttack — Engine Design Philosophies

ArtAttack is a 2D game engine intended, eventually, for developers other
than its author. The paint-shooter built on it is its first client and its
proof; a documented API, a sample game, teaching materials, and speed are
what will make it worth picking up. Its style is the sharpest break from
the major engines: value-first, stack-first C++ with no garbage collection
(T11). When engine and game pull in different directions, the
engine wins: the game is a client, not the owner.

This document describes the destination. It deliberately says nothing about
the current codebase — no findings, no history; [docs/review/](../review/)
holds all of that. Everything here is written in the present tense of the
target: when code moves, it moves toward this.

Two kinds of content:

- **The trade-offs (T1–T11)** — pairs of things we want that conflict, each
  with a chosen side. The right-hand side is always genuinely valuable, so
  each entry also names the price we accept and the point past which leaning
  further becomes malpractice.
- **The target** — what each part of the engine looks like when it arrives,
  so that any piece of code being moved has a known shape to move into.

**How to use it:** it is the tie-breaker when two designs both work, and it
is review vocabulary — "T3: take the simpler model" is a complete comment.
It changes by amendment, not erosion: a change that fights a philosophy means
either the change is wrong or the philosophy is, and if the philosophy is,
this file is edited in the same PR with the reason.

---

## The trade-offs

### T1. Mechanism over policy

The engine supplies *how*; the game decides *what*. When a feature can ship
in an hour by putting the game's rule inside engine code, or in a day by
adding a general mechanism plus a game-side rule, take the day. Other
people's games will never be expressible in this game's vocabulary.

**The price:** features cost more up front, and engine APIs will feel
over-general for a game that needs exactly one behaviour.

**Not a licence for:** speculative frameworks. Mechanisms are extracted from
a real client game in hand, and generalise only as far as real clients
demand.

### T2. Correctness over speed

A sound design beats a fast one, every time they truly conflict. A slow
frame is a bug you can profile; corrupted memory is unfalsifiable. Speed is
this engine's headline (T8) — which is exactly why it must never be bought
with undefined behaviour: a fast engine nobody can trust is worthless.

**The price:** checked access where an invariant can't be proven at the
boundary, copies where sharing would race, and no optimisation that costs a
lifetime or aliasing guarantee.

**Not a licence for:** ignoring shape. The structures that make code fast —
handles, allocation-free loops, a broad phase — are designed in from the
start; correctness and speed are only rarely in genuine conflict.

### T3. Simplicity over accuracy

The simulation is believable, not physical. Models are chosen because their
behaviour can be predicted, debugged and explained, not because they mirror
reality. If the player can't feel the difference, the simpler model is the
correct one — and it is usually the faster one too.

**The price:** real ceilings, accepted. Projectile speeds get capped so
nothing tunnels, instead of building continuous collision detection. Paint
area is quantised to tiles. There is no stacking physics and no rotational
dynamics.

**Not a licence for:** vagueness. A simple model still has exact,
documented, tested semantics. When code starts simulating to answer a
question, step back and find the closed form.

### T4. Clarity over cleverness

Written for the next reader — who, once the engine has users, is a stranger
seeing the API cold. Boring constructs, explicit control flow, names that
say what they hold. A clever line is a tax levied on every future visit.

**The price:** more lines, and passing up genuinely elegant tricks.

**Not a licence for:** copy-paste in the name of readability — duplication
is the opposite of clarity at scale — nor for refusing an abstraction that
makes structure explicit.

### T5. Compile-time over run-time

Push every error to the earliest stage that can catch it: the type system,
then a link error, then a load-time throw, then checked access — and never
silence. The build is the cheapest reviewer and the only tireless one.

**The price:** more targets, more types, more ceremony — and the wall
sometimes says no when a hack would have worked today.

**Not a licence for:** enforcement cleverness. The walls are boring: static
libraries, `const`, real types instead of tag enums, warnings-as-errors.
Template metaprogramming that proves things costs more clarity (T4) than it
buys safety.

### T6. Loud failure over graceful degradation

When the engine can't do what was asked, it says so immediately, with names
and paths — it does not limp. A missing texture is a launch-time error
naming the file, not an invisible sprite. Quiet failure is the most
expensive kind, and doubly so for a user who didn't write the engine.

**The price:** development builds stop dead where a shipped commercial
engine would soldier on.

**Not a licence for:** throwing on the way out — teardown stays silent —
nor for treating expected absence as failure. A disconnected controller is a
state (neutral input), not an error. Loud is for broken contracts, not for
the world being the world.

### T7. Data over code

Content is data files; code is machinery. Adding a weapon or a level-object
type edits JSON and a registration line, and rebuilds nothing.

**The price:** schema and validation work, and two places to look when
authoring content.

**Not a licence for:** logic smuggled into data — data says *which* and
*how much*; code says *how* — nor for data lookups at frame time. Data is
read, validated and resolved to handles at load; the frame loop never sees
it (T8).

### T8. Performance over customisability

The direct path is the default; indirection must pay for itself. Being fast
— high frame rates, large object counts — is part of this engine's identity
and a reason someone would choose it over a bigger engine. A customisation
point that taxes the frame loop is a customisation point that goes.

**The price:** less pluggability. Adding variety sometimes means writing
code where a registry would have let data do it, and some fast paths stay
hardcoded.

**Not a licence for:** welding game policy into engine code — the boundary
(T1) is compile-time structure and costs zero frames, so speed never
justifies breaching it — nor for buying throughput with unsoundness (T2).
Flexibility that survives lives at load and wiring time, never in the loop.

### T9. Built over bought

The math, collision, threading and state machinery are hand-rolled on
purpose: an engine offered to others must be one its author understands
completely and can defend line by line. Third-party code is reserved for the
platform and format edges — graphics API, input, audio backend, JSON parsing
— where reimplementation teaches nothing.

**The price:** bugs a library solved years ago, and a standing obligation
that built code meet library standard — contracts, tests, documentation —
or it was not worth building.

**Not a licence for:** rebuilding the platform edge. Build the mixer, buy
the audio backend.

### T10. One language over a scripting layer

Everyone who writes behaviour for an ArtAttack game writes C++. There is no
Blueprint, no Verse, no Lua — no second language dividing the people who
build a game into programmers and designers. The premise behind those
layers — that C++ is too hard for designers — is rejected here: C++ is as
hard as its API makes it, and simple things written against a well-shaped
API are no harder than in any scripting language. That claim is a burden
the engine accepts: the game-facing API must live in the simple subset of
the language and stay teachable to a beginner.

**The price:** no visual scripting, and contributors who don't know C++
must learn it — so the engine ships the on-ramp (see The public face).
Behaviour changes cost a compile, which makes build times an engine
responsibility and pushes all tuning through data (T7), where a change
costs a restart, not a rebuild.

**Not a licence for:** an expert-only API. If writing game code demands
template metaprogramming, allocator knowledge or lifetime puzzles, T10 has
failed on its own terms — the fix is simplifying the API, never adding a
scripting language on top. Nor for logic smuggled into JSON (T7): a
homemade DSL in data files is a second language through the back door.

### T11. The stack over the heap

Objects are values. They live on the stack or inline in the containers that
own them, are copied and moved explicitly, and die deterministically when
their owner does. There is no garbage collector, and there never will be.
Inheritance and virtual dispatch are tools of last resort, not the default
modelling grammar — the web of heap-allocated, pointer-linked, GC-managed
objects at the centre of the major engines is the single loudest thing this
engine is *not*. Value-first C++ is simpler to teach (T10), deterministic
to destroy (lifetimes are lexical, not collected), and fast by construction
(T8: contiguous, cache-friendly, allocation-free).

**The price:** designs that lean on shared object webs — observers,
self-reference, polymorphic bags — must be rethought as indices, variants
or per-type storage, and sometimes that rethink is real work. Occasionally
you copy where a pointer would have felt free.

**Not a licence for:** banning the heap. Containers own heap memory and
that is fine — the rule is that every allocation has one owner whose
lifetime is lexical, not that allocation is sin. Nor for imposing this
style on the engine's users: T11 governs the engine's own code and
everything it ships, while game code meets the engine at small interfaces
and chooses its own grammar behind them — full OOP or full performance
(see The object model).

---

Performance is a headline value, not a constraint: the engine does not stop
at "fast enough for the paint-shooter", because its ambition is to be worth
choosing for games that don't exist yet. Correctness (T2) is the only thing
that outranks it.

---

## The target

### Targets and layout

Three build targets, one dependency direction, one shared `.props` for
compiler settings:

| Target | Type | Directory | Depends on |
|---|---|---|---|
| `MattMath` | static library | `engine/math/` | nothing |
| `ArtAttackEngine` | static library | `engine/` | MattMath, platform SDKs |
| `ArtAttackGame` | application | `game/` | ArtAttackEngine |
| tests | applications | `tests/` | the libraries they test |

The disk layout mirrors the targets — `engine/math/`, `engine/core/`,
`engine/render/`, `engine/collision/`, `engine/input/`, `engine/audio/`,
`game/…` — and every file picks its home the day it is created. An engine
file including a game header fails to compile; that is the feature (T5).
Tests link libraries, never `#include` implementation files.

Platform-specific code — rendering backend, input devices, audio backend,
windowing — lives at the edge behind engine-owned interfaces, so that a
second platform is an addition, not a rewrite. Cross-platform is an eventual
goal, not a current work item: today there is one backend (D3D11/DirectXTK,
XInput), kept behind seams that don't presume it is the only one.

### The boundary

The engine provides mechanism; the game provides policy (T1):

| The engine provides | The game provides |
|---|---|
| Scene container, game loop, fixed-step timing | The match: rounds, win conditions, scoring |
| Collision detection and response | What collides with what, and what a hit means |
| Renderer interface, cameras, viewports, split-screen | What is drawn: sprites, HUD, menus content |
| Input devices and action mapping | Which actions exist and their bindings |
| Audio playback and mixing | Which sounds play, and when |
| Asset loading, registries, JSON parsing | The content: definitions, levels, tuning data |
| State machinery | The states: menu flow, gameplay flow |

The engine routes on data it does not interpret: collision layers and masks,
opaque game-owned tags, string keys, named spawn groups. Engine headers
contain no game nouns — no team, weapon, paint, menu or asset name. The
litmus test for any file: if a game built on the engine — yours or a
stranger's — would need to edit it, it is game code.

### The object model

- The engine imposes no modelling grammar. Game code meets the engine
  through small interfaces — `GameObject` (update, draw, bounds),
  `CollisionObject` (shape, layers, response) — and what stands behind an
  interface is the game's business: a class hierarchy, a plain struct, or
  a batch. Mechanism over policy applies to code style too (T1).
- Both ends of the spectrum are first-class. A game can be written in full
  OOP — one registered object per entity, hierarchies as deep as its
  author likes — or for full performance: one registered object standing
  for thousands of values updated in a tight loop. A paint-tile grid is
  one `GameObject`, not ten thousand. Interface granularity is the user's
  performance dial.
- The engine's own internals, and everything it ships — the sample game,
  the tutorials — are value-first (T11): concrete types, contiguous
  storage, interfaces implemented at batch granularity where N is large.
- There is no garbage collector and no ambient object graph. Ownership is
  explicit and lexical (T11); whether the scene owns a registered object
  or borrows it is stated in the API, never assumed.
- No reflection. With one language (T10) and no editor, there is nothing
  for it to serve; spawning from data goes through the factory registry
  (see Content and assets).

### Structural types

One rule decides what form a concept takes:

- **An interface** where the engine calls into unknown game code: `State`
  (flow), `GameObject` (entities), `CollisionObject` (collision response).
- **A concrete class** where the engine provides machinery the game
  drives: the application shell — window, device, services, main loop, the
  state stack; the game constructs it and hands it a first state — and the
  `Scene` (registration, update/draw orchestration, culling, collision
  dispatch, cameras and views, spawn groups), widgets, registries.
- **No type at all** where the concept is policy: there is no engine
  `Player`, `Level`, `Match` or `Team`. The engine-worthy parts of
  "player" already exist as input slots and views; everything else about
  one is the game's. A game's level is a game-side class that owns a
  `Scene` and adds the rules.

A game, to the engine, is a set of states plus content. There is no
`IGame` to implement.

Structure comes from example, not enforcement. The sample game shows the
canonical shape — states, a level class owning a `Scene`, entities
implementing the interfaces — and code that many games want but the engine
must not contain, a traditional player class chief among them, lives in
the sample as starter code to **copy into your game and own**, never as
engine API to depend on.

### Services and lifetimes

- Every resource has exactly one owner. A non-owning pointer is a documented
  loan: the member declaration says who owns the object and why it outlives
  the holder.
- Services are created once, at initialisation, and never reseated. Device
  loss recreates GPU objects in place; service identity is stable across it.
- Frame time is a parameter — `update(float dt)` — not shared state.
- Construction and destruction order are designed. If an ordering is
  load-bearing, it is either designed away or stated where it lives.

### Simulation and rendering

- `update()` writes; `draw()` is `const` all the way down and receives what
  it needs as parameters. All state changes — animation selection included —
  happen in `update()`.
- Objects draw through a renderer interface
  (`draw_sprite(texture, src, dst, tint, rotation, origin, depth)`). The
  D3D11/DirectXTK backend is one implementation behind it — the same seam
  serves headless testing today and a second platform later.
- Parallel rendering is sound because drawing is a pure read; render workers
  own disjoint slices of the scene.
- Objects expose bounds; the scene culls. Visibility is not each object's
  job.

### UI

- The engine ships a small widget set — labels, images, buttons, containers
  — with the two things every controller game needs solved once: focus, and
  navigation between widgets (stick/d-pad movement, per-viewport focus for
  split-screen).
- Layout is manual: the game positions widgets explicitly. There is no
  autolayout engine — the widget machine stays small, cheap and teachable
  (T4, T10).
- Widgets draw through the renderer interface like everything else, so
  menus are headlessly testable. Styling and content belong to the game.
- Menus are C++ against the widget API (T10), with their text, assets and
  tuning in data where it is data (T7).

### Collision

- Math primitives carry documented contracts — edge ordering, winding,
  zero-length behaviour, what throws — each pinned by behavioural tests in
  the commit that creates them (T3: simple, but exact).
- A broad phase prunes pairs; the narrow phase produces a contact manifold —
  normal and penetration depth — and resolution is analytic (MTV), not
  iterative search.
- Collidability is layer/mask filtering plus an opaque game tag. The engine
  decides *whether* things collide; the game decides what it *means*.

### Content and assets

- Definitions — weapons, projectiles, level-object types — are JSON records
  loaded into registries. The object builder is a `map<string, factory>` the
  game registers into; unknown types fail naming the type and file (T6).
- The asset manifest is data the loader walks, not filenames in source.
- Names resolve to handles once, at load. Per-frame code touches no
  string-keyed map (T7, T8).
- Tuning changes rebuild nothing.
- The division of labour is final: C++ says *how*, data says *which* and
  *how much*. There is no scripting layer between them (T10).

### Performance

- Throughput is the measure: frame rate and object count, on the parallel
  paths the engine already commits to (multi-core update and render).
- Per-frame code performs no heap allocation and no string-keyed lookups;
  fixed-size geometry returns fixed-size containers; data the frame iterates
  is laid out for iteration.
- Benchmarks pin throughput the way tests pin behaviour: representative
  scenes with large object counts, run alongside the test suite. A
  throughput regression is a defect, not a curiosity.
- Beyond the designed shape, optimisation follows a profile — measured, not
  guessed.

### The public face

- Every public engine header is documented: the contract, the intent, and
  where it isn't obvious, a usage example. A stranger starts from the
  headers, not from reading the implementation.
- A minimal sample game lives beside the paint-shooter. It is the standing
  answer to "how do I start a project on this engine" — and the permanent
  second client that keeps the boundary honest (T1).
- An "Introduction to C++" video series accompanies the engine (T10): the
  answer to "C++ is too hard" is teaching it, not wrapping it. The
  game-facing API is the series' subject matter — and its acceptance test:
  a feature that can't be explained in an introductory series has an API
  that is too hard.
- Once the engine/game split lands and the API settles, releases are
  versioned and breaking changes are deliberate, batched and documented —
  not incidental.

### Tests and toolchain

- Everything below the platform edge is testable headlessly, because the
  boundary (targets) and the seams (renderer interface, parameterised draw)
  make it linkable and constructible without a window.
- A new public primitive ships with behavioural tests in the same commit.
- Warnings are errors with zero suppressions; the language standard is
  current; `const`, `noexcept` and `[[nodiscard]]` carry information the
  compiler enforces (T5).
- Build time is iteration time (T10): every behaviour change is a compile.
  Compile speed is a maintained property — light headers, disciplined
  includes — and a build-time regression is a defect.
- There is no hot-reload machinery. The loop is edit, build, run — kept
  honest by fast builds, and by data (T7) carrying everything that
  shouldn't need a rebuild.

---

## Non-goals, today

Named so their absence is deliberate, and revisited only explicitly:

- **Online play. Permanent, not provisional.** ArtAttack is a
  local-multiplayer engine — split-screen, shared screen, one machine.
  Being excellent at couch multiplayer is the niche; no design tax is paid
  for netcode, replication, or rollback determinism, ever.
- **3D.** MattMath is a 2D library; the dimension is a design constant, not
  a parameter.
- **An editor or general tooling.** Content is data (T7); data files plus a
  text editor are the toolchain for now.
- **Framework maximalism.** Composition where it pays, not an ECS for its
  own sake (T4).
- **Every genre.** The engine serves 2D sprite-based games and generalises
  when a real client game demands it — the paint-shooter and the sample game
  first, users' games later — never on speculation (T1).
