# Review 2 — the plan

The findings turned into ordered work. The narrative lives in `README.md`; the
full evidence lives in the appendix. This document answers one question: **what
do I do, in what order, and what am I allowed to skip.**

The order below is a dependency order, not a severity order. Several `high`
findings sit on the do-not-fix list in §4, because the code they live in is
deleted by something above them. A three-line build-file edit is item one,
because until it lands nothing else on the plan is checkable.

Sizes are rough and mean what they say: **hours** ≈ an afternoon, **days** ≈ 2–4
days, **week** ≈ 5 working days, **weeks** ≈ 2 or more.

**Contents**

1. [The two open questions, answered](#1-the-two-open-questions-answered)
2. [The dependency spine](#2-the-dependency-spine)
3. [The work items](#3-the-work-items)
4. [Do not fix these](#4-do-not-fix-these)
5. [Sequencing](#5-sequencing)

---

## 1. The two open questions, answered

Both are decidable from the evidence already collected. Neither is close.

### 1.1 The renderer seam: yes, cut it — and cut it *before* `Scene`

**Recommendation:** build `engine/render/renderer.h` with the D3D11/DirectXTK
implementation under `engine/render/d3d11/`, and do it before any part of
`Scene` is extracted from `Level`. Make it a **concrete class selected at build
time, not an abstract base with a vtable.**

**Why it comes first, and not after `Scene`.** Every method the review assigns
to the future engine `Scene` carries the backend in its own signature. All five
private draw methods at `game/objects/level.h:164-183` take
`std::vector<ID3D11DeviceContext*>*`, `std::vector<ID3D11CommandList*>*` and
`std::vector<DirectX::SpriteBatch*>*`, and so does the public one at
`game/objects/level.h:80-82`. Extracting `Scene` first writes those three types
into `engine/core/scene.h` — and one of them is an *output* parameter whose
caller must pre-size the vector, pre-fill it with null and `Release` every
non-null entry, three obligations stated nowhere in the tree. Today
`engine/core` breaches its own module rule (`docs/design/ARCHITECTURE.md:131`,
`core | math`) at exactly one line: `engine/core/i_game_object.h:4`,
`#include <SpriteBatch.h>`. Doing `Scene` first turns one include into three
types in a parameter list, in the file a stranger reads second. Then the file
gets written again when the seam lands.

**Why they are one design and not two.** The parallelism axis is *views* — the
comment at `engine/core/i_game_object.h:10-15` says so, and it is correct. But
the view count comes off the player list (`game/objects/level.cpp:401`), each
view's viewport comes off a `Player` (`game/objects/level.cpp:523`) and each
view's camera comes off the same `Player` (`game/objects/level.cpp:526`). So
`Scene` cannot be built at all until it owns an explicit view list — and the
D3D11 unit of a view (a deferred context, a sprite batch, a command list) is
exactly what the seam has to own. Whoever designs `Scene`'s view list is
designing the seam's unit of work. Doing them apart means designing the same
thing twice and then reconciling the answers.

**Why it is worth the weeks.** It is the only change that removes
`<SpriteBatch.h>` from `engine/core`; nine of `ArtAttackEngine`'s 32 translation
units are untestable on that one include, which is why `tests/` has an `assets`,
a `core` and a `math` folder and no `render`. It is where the
`record → FinishCommandList → ExecuteCommandList → Release` protocol currently
hand-written in four places (`game/objects/level.cpp:606`, `:670`,
`game/states/menu_page.cpp:120`,
`samples/minimal/states/hello_state.cpp:78-84`) becomes one function. And it is
the acceptance test for the sample: `samples/minimal/` is 85 lines and it
currently teaches Direct3D 11 rather than this engine.

**It is narrower than the classes make it look.** Of `DeviceResources`'
seventeen graphics accessors, twelve have no caller anywhere in the repository
and five have exactly one or two (`engine/render/device_resources.h:56-100`).
`ViewportManager` already splits down the middle:
`camera_adjusted_player_viewport_rect` (`engine/render/viewport_manager.cpp:27-45`)
is pure arithmetic that comes through untouched, while `apply_player_viewport`
(`:43-58`) is three lines of `RSSetViewports`/`SetViewport` that are purely
backend. `Viewport::d3d_viewport()` (`engine/math/matt_math.h:797`) has exactly
two live callers, both in `engine/render/viewport_manager.cpp` (`:24`, `:65`).

**Vtable or compile-time selection.** This question is asked exactly once in the
entire corpus and the triage's headline recommendation dropped it. Answer:
**concrete class.** T8 (`docs/design/PHILOSOPHY.md:136-148`) says a
customisation point that taxes the frame loop is a customisation point that
goes, and a virtual `draw_sprite` per sprite is that tax in the module that
draws thousands of paint tiles per frame. The seam's two actual purposes —
headless tests and an eventual second platform — are both served by build-time
selection: a null implementation linked into `tests/render/`, and a second
backend added as a sibling folder. Neither needs two backends live in one
process. T5 makes the compile-time choice a link error rather than a runtime
one. And if a real client ever *does* need runtime selection, promoting a
concrete class to an interface is mechanical and the call sites do not change —
that option is held, not spent, exactly like the module-to-library escalation at
`docs/design/ARCHITECTURE.md:122-126`. Write the decision into ARCHITECTURE's
module table, which currently says "renderer interface" without saying which
kind.

**Six constraints the interface must meet.** Each is filed separately somewhere
in the review; together they are a specification, and none is derivable from
"replace `SpriteBatch*` with `Renderer&`". A seam that misses any one gets cut
twice.

| # | Constraint | Where the evidence is |
|---|---|---|
| 1 | The unit of work is a **view**, not an object — workers do not own disjoint slices | `engine/core/i_game_object.h:10-15` |
| 2 | **Sort depth is per draw**, not per object — the same sprite in two viewports at two depths is currently inexpressible | `engine/render/texture_object.cpp:110` reads `layer_depth()` off the shared object inside the function built to take locals |
| 3 | **Sampler state lives inside the seam** — two objects cache `CommonStates::PointClamp()` across device loss | `game/objects/level.cpp:531`, `game/objects/level.h:149`, `game/objects/level_builder.h:39` |
| 4 | **Command-list lifetime is RAII and inside the seam** — four hand-written copies today | `game/objects/level.cpp:606-610`, `:670-674`, `game/states/menu_page.cpp:120`, `samples/minimal/states/hello_state.cpp:78-84` |
| 5 | **Font metrics are an engine type** — `TextObject`'s *constructor* measures, so text is unconstructible headlessly, not merely undrawable | `engine/render/text_object.cpp:112-118`, called from `:25` |
| 6 | **The text entry point is wide** (`std::wstring`), because the narrow overload races | DirectXTK `Src/SpriteFont.cpp:365-391`; done separately in A3 below |

**Cost of the alternative (Scene first, seam later):** `engine/core/scene.h`
written twice, and the second write is not a signature change — it is a redesign
of the file's central data structure, the view list. Plus a second full sweep of
20 `override` sites across 13 engine headers, 17 game headers and 45 state
classes, because the game-side overrides migrate once for `Scene` and again for
the seam.

**Definition of done, and it is a grep.** `samples/minimal/states/hello_state.cpp`
contains no `ID3D11*` identifier, no `<SpriteBatch.h>`, no `*this->app_->dt()`;
`samples/minimal/CMakeLists.txt:21-25` names no `Microsoft::DirectXTK`.

### 1.2 Event-based game logic: no. Close the question and write the decision down

**Recommendation:** build no event machinery. Record in
`docs/design/PHILOSOPHY.md` that results are returned, contacts are a list, and
suspend/resume is the state stack — and reopen only against a named case.

**The strongest form of the argument is the absence.** Across 84 group reports
and 178 clustered findings, there is no case where the fix requires a sender
that does not know its receivers. Split-screen local multiplayer, fixed step,
one scene, one process is precisely the domain where direct calls and per-frame
value lists are sufficient.

**All three things that look like they want a bus are already answered by items
on this plan.**

1. **Collision response.** `game/objects/i_collision_game_object.h:11-15`
   declares `on_collision(const ICollisionGameObject*)`, and the dispatch fires
   *both* participants' responses off *one* participant's predicate
   (`game/objects/level.cpp:245-248` and `:280-284`) while testing every pair
   twice. The callback pairing is already broken before any event machinery is
   added. PHILOSOPHY/Collision (`docs/design/PHILOSOPHY.md:395-397`) names the
   replacement and it is data, not an event: "the narrow phase produces a
   contact manifold — normal and penetration depth — and resolution is analytic
   (MTV)". A manifest of contacts the game iterates after `Scene::resolve()` is
   a value. → **item C2.**
2. **A child state reporting a result.** `game/states/game_states.h:28`, `:60`,
   `:64`, `:67` — a heap `bool` and three `unique_ptr<enum>` used as
   out-parameters. This is the one place the codebase actually reached for
   indirection, and what it reached for was a return value. → **item C3.**
3. **"Something happened to the level."** `game/objects/level.h:89`,
   `void stop_music() const`, called by hand at seven `GameLevel` exits; plus
   the weapon loop that keeps playing under the pause menu
   (`game/states/game_states.cpp:155-173`, `game/objects/weapon.cpp:242-259`).
   Both want a lifetime and a suspend hook. The engine already ships the
   mechanism with zero callers — `SoundBank::pause_effect` / `resume_effect` at
   `engine/audio/sound_bank.h:54-55`. → **item C3.**

**Cost of the alternative.** An event bus makes the receiver set dynamic, so the
frame's work stops being statically known: that is either an allocation per
emission or a preallocated queue plus a dispatch table, on the frame path, with
no benchmark to measure it against — the word "benchmark" appears zero times in
863 findings, against `docs/design/PHILOSOPHY.md:420-424`, which makes throughput
regressions defects. It makes the update order of the three cases above
*unstated* where today it is lexical. It makes headless testing harder, not
easier, because a test asserts on emissions rather than on returned values. And
T11 (`docs/design/PHILOSOPHY.md:190-198`) names the web of pointer-linked
observer objects as "the single loudest thing this engine is *not*", while T1's
"Not a licence for: speculative frameworks"
(`docs/design/PHILOSOPHY.md:45-48`) rules out building it before a client
demands it.

**Why closing it matters more than the answer.** An undecided event system is a
standing invitation to answer each of the three cases above with a subscription,
one at a time, during exactly the weeks when `Scene` and the state stack are
being written. The falsifiable version of the decision: if a genuine case
exists, it appears after C1–C3 land, and it will be specific enough to answer
with a narrow mechanism rather than a bus.

---

## 2. The dependency spine

```
A1 tests link the engine ─┐
A3 SpriteFont race        ├─ no dependencies; A1 is the net for everything below
A4 deferred transition_to │
A2 engine arithmetic bugs │
A5 delete, then rename ───┤
A6 include grep ──────────┘
                          │
                          ▼
                    B1  THE SEAM  ← the only item on the plan measured in weeks
                          │
          ┌───────────────┼───────────────┐
          ▼               ▼               ▼
     C1 Scene        C2 collision     C3 state stack
     (2 weeks)        (1–2 weeks)      (1 week)
          └───────────────┼───────────────┘
                          ▼
              E1 MattMath + include roots
              E2 tuning into data
              E3 document corrections

off the spine, parallel with everything from A5 onward:
     D1 ui focus/navigation ·  D2 input module  ·  D3 asset validation
     A7 README / licence / wave bank / CI
```

**What blocks what, and why:**

- **B1 blocks C1** because the five methods that become `Scene` name D3D11 in
  their own signatures (`game/objects/level.h:164-183`), and because `Scene`'s
  view list and the seam's unit of work are the same design (§1.1).
- **B1 blocks C3** only in the weak sense that C3 rewrites the flow of 45 state
  classes and B1 rewrites their signatures (`engine/core/state.h:12-14` →
  `update(float dt)` / `draw(Renderer&) const`). Doing C3 first means editing
  all 45 twice. C3 does **not** need `Scene`.
- **C1 and C3 do not block each other.** Their file sets are disjoint: C1 opens
  `game/objects/level.{h,cpp}`, `game/objects/level_builder.*` and the new
  `engine/core/scene.*`; C3 opens `engine/core/state_context.*` and
  `game/states/*`. The single point of contact is `GameLevel::draw()` calling
  `level_->draw(...)`.
- **A1 blocks nothing formally and gates everything in practice.** Today the
  automated coverage of `ArtAttackEngine` is zero translation units, and `ctest`
  stays green through any of B1, C1 or C3. A1 is three lines in two build files.
- **A5's rename half must precede B1**, not follow it. Otherwise B1's diff is a
  rename and a redesign entangled in the same hunks across 13 engine headers, 17
  game headers and 45 state classes.
- **E1 must follow B1**, which is counter-intuitive: "MattMath depends on
  nothing" is the most-cited violation in the review (seven findings), and it
  looks like a prerequisite. It is not. Nothing fails to compile or link because
  of `engine/math/matt_math.h:5`, `#include <d3d11.h>` — the SDK is present. What
  blocks a headless test is that `DirectX::SpriteBatch`'s only constructor
  dereferences its `ID3D11DeviceContext*`, so a drawable cannot be *driven*.
  MattMath's D3D dependency is a compile-time and portability cost, not a
  testability one — and `Viewport::d3d_viewport()`'s two live callers
  (`engine/render/viewport_manager.cpp:24`, `:65`) need somewhere to go, and that
  somewhere is `engine/render/d3d11/`, a folder B1 creates. Doing E1 first means
  inventing a temporary adapter home for a conversion that then moves again.
- **D1, D2 and D3 depend on nothing on the spine.** Widget focus and navigation
  are pure logic over `bounds()` (`engine/ui/widget.h:59-74` already exposes
  it); the input module is device polling, deadzones and edge detection; the
  loader guards are inside two `.cpp` files. These are the right work for
  whatever capacity exists beside B1.

---

## 3. The work items

### Phase A — the net and the subtractions

Nothing in Phase A requires a design decision. All of it survives every later
phase.

---

#### A1 — Make the tests link `ArtAttackEngine`, and add the two missing test folders

**What changes.** `tests/assets/CMakeLists.txt:6-11` compiles
`${CMAKE_SOURCE_DIR}/engine/assets/asset_manifest_loader.cpp` and
`json_loader.cpp` directly instead of linking the library; `tests/core` links
neither the engine nor anything else (`tests/core/CMakeLists.txt:6-15`). Add
`ArtAttackEngine` to `target_link_libraries` in both, delete the copied source
entries and the include directories they needed, and create `tests/render/` and
`tests/collision/`.

**The stated reason not to is false, and it is worth saying out loud.** The
comment at `tests/assets/CMakeLists.txt:3-5` says linking the engine drags in
"everything the engine drags in". A static library pulls only the objects whose
symbols are referenced: `asset_manifest_loader.obj` and `json_loader.obj`, and
it stops. What arrives beyond that is `Microsoft::DirectXTK`'s import library on
the link line, via `engine/CMakeLists.txt:50-53` — a name for the linker, not 32
translation units of object code.

**Files.** `tests/assets/CMakeLists.txt`, `tests/core/CMakeLists.txt`,
`tests/CMakeLists.txt`, new `tests/render/`, `tests/collision/`.

**Closes.** The "no test target links the engine" cluster —
`tests/assets/CMakeLists.txt:6-19` filed in five separate reports; the
`tests --> engine` edge in `docs/design/ARCHITECTURE.md:33-39` that no target
satisfies; ARCHITECTURE's test-folder tree at `:100-104`, which names three of
nine modules; "no test covers any file in this group" against `engine/core/`
(`tests/core/CMakeLists.txt:1`); "`engine/ui` has no tests"
(`engine/ui/widget.cpp:79-123`); "test the three pure functions in this module"
(`engine/render/viewport_manager.cpp:143-234`); "the engine's only pure
primitive has no documented contract and no test"
(`engine/collision/partitioner.h:12-16`).

**Size.** Hours for the link change. Days for the first tests, which are A2.

**Unblocks.** Everything. B1 is a rewrite of every draw signature in the engine,
C1 moves eleven members and nine methods out of a 766-line class, C3 rewrites
`game/states/`. This is the cheapest possible thing to do first and the only one
that makes the next three items falsifiable. It is also the item most likely to
be deferred, because it reads as tidying rather than as enablement.

---

#### A2 — The engine defects that are pure arithmetic, with tests

**What changes.** Five fixes in `engine/render/`, `engine/collision/`,
`engine/core/` and `engine/app/`, none of which touches a D3D type, all of which
come through B1 unchanged.

1. **The camera scroll border clamps a minimum above a maximum.**
   `engine/render/camera_tools.cpp:102-105` clamps each border into
   `[MIN_BORDER_*, viewport/2]` with the floors at `:19-22` (400/250), and
   `mattmath::clamp_ref` (`engine/math/matt_math.cpp:50-60`) takes the min
   branch first, so the max is never consulted. Two-player at 1280×720 gives a
   1280×360 viewport: `top` clamps up to 250 and `bottom` to 250 against a max
   of 180, so `top_edge = T + 250` sits *below* `bottom_edge = T + 110`
   (`camera_tools.cpp:36-37`) and a motionless player produces 140 px of
   vertical judder at 60 Hz, forever. Three- and four-player (640×360) break
   both axes. 1280×720 is the only supported resolution that breaks — and it is
   `ApplicationOptions`' default, `ResolutionManager`'s default, and what every
   unparseable save file coerces to.
2. **`all_viewports()` asks for viewport index 4.**
   `engine/render/viewport_manager.cpp:90`, in the three-player branch. Valid
   indices are 0–2; index 4 falls through `calculate_viewport`'s switch
   (`:143-234`), several of whose cases have no exit, and returns a fullscreen
   rectangle as the fourth pane.
3. **`Partitioner::partition` divides by an unchecked argument.**
   `engine/collision/partitioner.cpp:5-11`, first line, no precondition; reached
   from `game/objects/level.cpp:401` on every frame of every view.
   `ThreadPool` never validates its counts (`engine/core/thread_pool.cpp:5-29`)
   and `ApplicationOptions` is never validated at all
   (`engine/app/application.cpp:73-75`), so `max_threads = 0` is an integer
   divide-by-zero on the first frame and `target_fps = 0` hangs inside
   `StepTimer`.
4. **`ViewportManager` holds a `DeviceResources*` no member function reads**
   (`engine/render/viewport_manager.h:23-24`, `:55`). Dropping it is what makes
   the class constructible in a test today.
5. **Tests for all of the above**, in the folders A1 created: the four screen
   layouts, the partitioner's edge cases, and the border model. One line pins
   the worst of them: `calculate_camera_scroll_border({1280,360}).top + .bottom
   > 360`.

**Files.** `engine/render/camera_tools.{h,cpp}`,
`engine/render/viewport_manager.{h,cpp}`, `engine/collision/partitioner.{h,cpp}`,
`engine/core/thread_pool.cpp`, `engine/app/application.cpp`, `tests/render/`,
`tests/collision/`.

**Closes.** Two `high` findings in `render-viewports`, two in
`sweep-testability`, the `partition()` and `ApplicationOptions` validation
findings in four reports, plus the `calculate_viewport` fall-through in
`sweep-memory-safety`.

**Size.** Days.

**Unblocks.** Nothing structurally — and that is the point. This is the code
that is *already* testable and *already* wrong, and it is the only thing that
will check B1's 60-file diff.

---

#### A3 — Hold text as `std::wstring` and call the wide DirectXTK overloads

**What changes.** `TextObject` stores `std::string` and passes `const char*` to
`SpriteFont::DrawString` / `MeasureString`
(`engine/render/text_object.cpp:36-45`, `:53-62`, `:74-83`, `:116`). DirectXTK's
narrow overloads convert through `SpriteFont::Impl::ConvertUTF8`
(`Src/SpriteFont.cpp:365-391`), which lazily allocates and may *reallocate* a
`utfBuffer` owned by the shared `SpriteFont`, from a `const` method. Meanwhile
`Level::draw_active_level` (`game/objects/level.cpp:393-418`) fans out one
worker per player and each worker draws the whole HUD with that player's own
numbers. Switch the stored type and the call sites to wide.

**Files.** `engine/render/text_object.{h,cpp}`, `engine/render/text.{h,cpp}`,
`engine/render/text_drop_shadow.{h,cpp}`, `game/objects/interface_gameplay.cpp`,
plus the call sites that set text.

**Closes.** The one remaining piece of *live* undefined behaviour in the tree,
and it corrects `docs/review/IMPLEMENTED.md:46` and `:68-78`, which record
finding `#16`'s data race as fixed and only its redundancy as outstanding. That
is the one status line in that document not to trust: the const pass made
`draw()` a pure read of *ArtAttack's* types, and the race is one level deeper
inside a `const` DirectXTK method.

**Size.** Hours.

**Unblocks.** Nothing, and it is still near the top: the seam is weeks of work
and this should not still be shipping while it is being designed. It is also
constraint 6 of the six in §1.1, so doing it now removes it from B1's scope.

---

#### A4 — Make `transition_to` deferred

**What changes.** `StateContext::transition_to`
(`engine/core/state_context.cpp:21-26`) assigns over `state_`, destroying the
state whose `update()` is on the stack, then calls `set_context` and `init()` on
the incoming one. Park the incoming state in a `pending_` member and swap it in
inside `StateContext::update()` (`engine/core/state_context.cpp:5-12`) after
`state_->update()` returns. About five lines.

**Why now and not with C3.** Ten call sites do this from inside their own
`update()` — `game/states/game_states.cpp:80-85`, `:221-222`, `:262-263`,
`:272-273`, `:282-283`, `:299-300`, and
`game/states/pause_menu.cpp:87-89`, `:94-96`, `:242-244`, `:267-269`. All ten
are safe only because none of them touches a member afterwards, and
`engine/core/state_context.h` has no comment on any of its 19 lines saying so.
That is a property of forty-two call sites rather than of one function, and C3
is the item that rewrites every branch ending in a `transition_to`. Whoever does
C3 will be moving statements across a call whose danger is recorded nowhere.

**Files.** `engine/core/state_context.{h,cpp}`, new
`tests/core/state_context_tests.cpp`.

**Closes.** The `transition_to` cluster — four findings across three reports.

**Size.** Hours, plus the first `tests/core/` test that A1 made linkable.

**Unblocks.** C3, safely.

---

#### A5 — Delete first, then rename. One pass, before the seam

**What changes, part 1 — subtraction.** Every one of these has zero callers or
has never executed:

- `Drawer` (`engine/render/drawer.h:10-35`, `engine/render/drawer.cpp:13-57`,
  listed at `engine/CMakeLists.txt:32`) — dead engine API built on a camera
  model the engine replaced; all three of its helpers have zero callers, and its
  two subclasses inherit it to store one pointer.
- `MovingObject`'s 13 dead virtual accessors and the cached `dx_`
  (`engine/core/moving_object.h:7-59`, `:34-48`, `:56`) — 27 virtual protected
  accessors around twenty bytes with one real override between them.
- `MatrixF` — the constructor sets the dimensions and never sizes the storage
  (`engine/math/matt_math.*`), so the whole matrix subsystem has never executed.
  **Delete it rather than fix it**; nothing calls it, and `mattmath::divide` is
  not matrix division under any definition.
- `LevelEndInfo`'s six callerless accessors (`game/objects/level_end_info.cpp:80-128`);
  `winning_ratio()` at `:114-121` divides by `losing_score()`, which is `0.0f`
  in the ordinary shut-out case.
- `LevelMode` (`game/objects/level_mode.h:3-9`) — written once, never read.
- `MenuElement` and both `convert_*_to_element` functions in `game/states/`.
- `MainMenuData`'s callerless one-argument constructor
  (`game/states/main_menu_data.cpp:5-9`) and the setters that patch it into
  validity.
- The commented-out code: 229 lines in `engine/math/matt_math.cpp` for a
  subsystem never built, ~160 lines in `game/objects/player.cpp`, 112 lines in
  `game/objects/level.cpp:420-508`, the 3D transcription in
  `engine/math/ericson_math.cpp`.
- `game/content/save/save_data.txt` — a dead file from a previous save format.

**What changes, part 2 — the renames.** `IGameObject` → `GameObject`
(`engine/core/i_game_object.h:23` and the filename), `ICollisionGameObject` →
`CollisionObject`, `IPaintableGameObject`, `IDeviceNotify`; and the `M` family
in `engine/ui/widget.h:9,33,59,76,114,140` — `MObject`, `MWidget`, `MContainer`,
`MTexture` and the rest. The review counts 135 `I*` references and 325 `M*`
references. `docs/design/CONVENTIONS.md:32-37` names `GameObject`/`IGameObject`
by name as the
counter-example, and the `M` prefix is doubly wrong: it is a banned type prefix
*and* it stands for the game's word "menu" on an engine widget set.

**Files.** `engine/render/drawer.*` (deleted), `engine/core/moving_object.*`,
`engine/ui/widget.*`, `engine/core/i_game_object.h` (renamed), 13 engine
headers, 17 game headers, `engine/CMakeLists.txt`.

**Closes.** Cluster 24 (the `I`/`M` prefix quartet), the three `Drawer`
findings, the two `MovingObject` findings, the `MatrixF` pair, and roughly a
dozen dead-code findings scattered across the game reports.

**Size.** Days.

**Unblocks.** B1's readability. Renaming and redesigning the same declarations
in one commit produces a diff nobody can review, and the rename half is
zero-risk and mechanical. The deletion half shrinks B1's blast radius by two
files outright.

---

#### A6 — Add the engine→game include check

**What changes.** A five-line CMake custom target that fails the build if
`#include "game/` appears anywhere under `engine/`.

**Why now and why only the grep.** `docs/design/ARCHITECTURE.md:14-16` says an
engine file including a game header "fails to compile, and that is the feature
(T5)". It compiles and links: `engine/CMakeLists.txt:46-49` and
`engine/math/CMakeLists.txt:14` both publish `${CMAKE_SOURCE_DIR}` as a `PUBLIC`
include directory. The proper fix — engine sees only itself, game and sample
receive the root through the engine's `INTERFACE` — is E1, and it belongs
*after* the moves, because C2 moves `ICollisionGameObject` out of
`game/objects/` into `engine/collision/` and B1 moves the draw path across the
same boundary in the other direction. Those are exactly the weeks when an engine
file acquires a `#include "game/..."` by accident, and exactly the weeks when a
hard include-root change breaks the build for unrelated reasons.

**Files.** `cmake/`.

**Size.** An hour.

**Unblocks.** Nothing; it protects B1, C1 and C2 while they run.

---

#### A7 — The front door (off the spine, and cheap)

**What changes.** Four independent things a second person meets before any
header:

- **A README and a licence.** `git ls-files` finds `docs/review/README.md`,
  `external/rapidjson/README.md` and `external/rapidjson/license.txt` and
  nothing at the root. No licence also means the two adopted Microsoft files
  (`engine/render/device_resources.*`, `engine/core/step_timer.h:1-12`) and the
  transcribed Ericson routines (`engine/math/ericson_math.cpp`) ship with no
  licence text at all — a real distribution problem, separate from the "front
  door" one.
- **The build prerequisites.** `CMakePresets.json:11-12` requires
  `$env{VCPKG_ROOT}`, documented nowhere.
- **The wave bank.** `.gitignore:366-368` is `*.wav`, `*.xwb`, `*.mp3` — blanket
  globs, not path-scoped — and `game/content/sounds/sound_bank_1.xwb` is
  consequently untracked, so a fresh clone throws at startup on the file the
  shipped manifest names. Either commit it with a path-scoped exception or
  commit the pipeline that rebuilds it and say so in writing.
- **CI.** `docs/design/ARCHITECTURE.md:63-65` says `ctest` runs "on a
  contributor's machine or in CI"; there is no `.github/`, no
  `.gitlab-ci.yml`, no `azure-pipelines.yml`. One workflow that configures the
  release preset, builds and runs `ctest` on a clean checkout would have caught
  both of the previous two bullets on its first run. If CI is not wanted yet,
  strike the clause from ARCHITECTURE so the document stops describing
  infrastructure that does not exist.

**Size.** Hours, plus whatever the wave-bank decision costs.

**Unblocks.** A second person. Nothing technical.

---

### Phase B — the pivot

---

#### B1 — The renderer seam, and all four interface signature changes, in one pass

**What changes.**

1. **`engine/render/renderer.h`** — a concrete class (§1.1) with roughly this
   surface, which is what counting the callers of `DeviceResources`' seventeen
   accessors actually yields: a per-view draw-list handle (replacing
   `deferred_context(i)`), `submit()` (the
   finish/execute/release protocol, written once), `begin_frame`/`end_frame`
   (absorbing `Application::clear()` and `Present()`), `draw_sprite(...)` as
   `docs/design/PHILOSOPHY.md:367-370` already specifies, `draw_text(...)` and a
   `measure()` that does not need a device to construct, `back_buffer_size()`,
   and the PIX marker pair. `GetD3DDevice` is not a renderer concern at all — it
   is a resource-factory one, and `RenderResources` already speaks
   `Handle<ID3D11ShaderResourceView>` (`engine/render/render_resources.h:38`),
   so only the handle's payload type changes.
2. **`engine/render/d3d11/`** — `device_resources.*` moves here as the one
   implementation, owning the deferred contexts, the sprite batches, the sampler
   state and command-list lifetime.
3. **`engine/core/i_game_object.h:41-42`** — `DirectX::SpriteBatch*` becomes
   `Renderer&` (or the per-view draw target), and `#include <SpriteBatch.h>` at
   `:4` goes. This is the single line that makes ARCHITECTURE's module table
   true rather than aspirational.
4. **`DirectX::SpriteEffects` → an engine `SpriteFlip`** across the sprite
   chain.
5. **`engine/core/state.h:12-14`** — `virtual void update() = 0` becomes
   `update(float dt)`, `virtual void draw() = 0` becomes
   `draw(Renderer&) const`. With it go `Application`'s heap `float*`
   (`engine/app/application.h:131-134`, `:159`), `GameData::dt()`'s
   `const_cast`, and `Application::sprite_batches()`'s `const_cast`
   (`engine/app/application.cpp:429-432`).

**Why these five are one commit and not five.** They rewrite the same
declarations in the same files. `engine/ui/widget.h:69-73` takes three of them
on adjacent lines. Each done separately is a full sweep of 20 `override` sites
across 13 engine headers, 17 game headers and 45 state classes. `State::update(dt)`
in particular should **not** be done on its own authority as a
`docs/design/PHILOSOPHY.md:358` fix — it forces a second edit of all 45 state
classes when `draw(Renderer&)` arrives.

**Files.** All of `engine/render/` (38 files), new `engine/render/d3d11/`,
`engine/core/i_game_object.h`, `engine/core/state.h`, `engine/ui/widget.*`,
`engine/app/application.*`, `game/objects/level.{h,cpp}`,
`game/states/menu_page.{h,cpp}`, ~17 game headers, 45 state classes,
`samples/minimal/states/hello_state.{h,cpp}`, `samples/minimal/CMakeLists.txt`.

**Closes.** Cluster 1 (11 findings), cluster 2 (DirectXTK in audio and assets
APIs), cluster 19 (`draw() const` not enforced past the shared services), cluster
20, cluster 21 (per-view fan-out hand-written twice, already diverged), cluster
37 (the sample's hand-rolled command lists), cluster 38 (`State` takes no
parameters), plus the two `CommonStates::PointClamp()` caches
(`game/objects/level.h:149`, `game/objects/level_builder.h:39`) and the
`text_bounds()`/`Text`-forwarding findings in `render-text`.

**Size.** **2–3 weeks.** This is the only item on the plan measured in weeks and
the only one that should be.

**Unblocks.** C1, C3, headless tests for `engine/render/` and `engine/ui/`, the
sample as a usable template, a second backend, and E1.

**Order inside it.** Port `samples/minimal/` first — 85 lines, and it is the
acceptance test for whether the seam is real. Then `engine/ui/widget.*`, then
`game/states/menu_page.*`, then `Level`'s draw path last, because it is the
largest and the one C1 is about to move anyway.

---

### Phase C — three streams after the seam

C1 and C3 are genuinely concurrent; C2 shares `game/objects/` with C1 and should
follow or interleave with it.

---

#### C1 — `Scene` out of `Level`

**What changes.** A new `engine/core/scene.{h,cpp}` with the three things
`Level` does not have, then the move.

The three new things, each of which is why this is a design and not a
cut-and-paste:

1. **An explicit view list** — N × (viewport, camera), refilled by the game each
   tick. Today the view count is `player_objects_->size()`
   (`game/objects/level.cpp:401`), each viewport comes from
   `player_objects_->at(i)->player_num()` (`:521-523`) and each camera from
   `player_objects_->at(i)->camera()` (`:526`). Folding `player_objects_` into
   one registered-object list deletes the renderer's only source of view
   information, so the view list has to exist before the fold.
2. **`add()` with insertion deferred to end of tick** —
   `update_weapon_and_get_projectiles` (`game/objects/level.cpp:218-224`) is the
   only place objects enter the world mid-frame.
3. **A named post-resolution phase** —
   `update_weapon_position()` / `update_prev_rectangle()`
   (`game/objects/level.cpp:264-268`) is an unnamed post-resolution step only
   `Player` has. `Scene` either names that phase for everyone or the game runs
   it itself after `Scene::resolve()`.

**What moves.** Members: `non_collision_objects_` (`game/objects/level.h:92-93`),
`collision_objects_` (`:95-96`), `viewport_dividers_` (`:101-102`),
`camera_tools_` (`:124`), `resolution_manager_` (`:127`), `viewport_manager_`
(`:128`), `render_resources_` (`:129`), `out_of_bounds_` (`:140`),
`camera_bounds_` (`:141`), `thread_pool_` (`:154`), `partitioner_` (`:155`).
Methods: the two update loops (`level.cpp:185-188`, `:228-231`), the collision
dispatch (`:233-305`), the out-of-bounds sweep (`:310-317`), the swap-and-pop
compaction (`:320-329`), `is_object_out_of_bounds` (`:709-714`),
`draw_active_level` (`:393-418`), `draw_player_view_level` (`:509-612`),
`draw_zoom_out_level_component` (`:625-675`) and the three culls (`:536`,
`:545`, `:554`).

**What stays in `Level`.** `player_objects_` (`level.h:98-99`); the FSM —
`LevelState` (`:21-28`), `state_` (`:148`), `state()`/`set_state()` (`:84-85`),
the branch in `update` (`level.cpp:68-176`) and in `draw` (`:376-390`); the four
timers (`level.h:131-134`); the zoom-out camera and its bounds (`:136`,
`:142-143`); all audio (`:104-112`); all HUD (`:120`, `:122`, `:125`, `:152`);
scoring (`:138`); and `level_consts` (`:30-51`).

**Fix while you are in there.** `draw_zoom_out_level_component`
(`game/objects/level.cpp:652-661`) has no `bounds()` cull, while
`draw_player_view_level` twenty lines above (`:543-549`) does — and the
uncullled path is the one that runs for the whole post-match menu flow, 5,230
`PaintTile::draw` calls per frame on `turbulence`, single-threaded, entirely
behind an opaque results box. "Objects expose bounds; the scene culls"
(`docs/design/PHILOSOPHY.md:373-374`) is exactly what `Scene` is for.

**Files.** New `engine/core/scene.{h,cpp}`, `game/objects/level.{h,cpp}`,
`game/objects/level_builder.{h,cpp}`, `game/states/game_states.cpp`.

**Closes.** Finding `#18`, outstanding since the 2023 review. Clusters 30 and 50
(`Level`'s `unique_ptr<vector<unique_ptr<T>>>` object lists and its
heap-allocated scalars). Plus, by deletion, seven or eight `Level` findings
listed in §4.

**Size.** **2 weeks.**

**Definition of done.** `HelloState` registers its two text objects with a
`Scene` and does not loop over them itself.

---

#### C2 — `engine/collision/` gets collision

**What changes.** Today `engine/collision/` contains `partitioner.{h,cpp}` and
nothing else (`engine/CMakeLists.txt:22`) — a nineteen-line thread-range
splitter that is a *draw* scheduler. The narrow phase and the resolver are in
`engine/math/collision_tools.cpp`; the dispatch is in
`game/objects/level.cpp:233-305`; the interface is
`game/objects/i_collision_game_object.h:11-15`.

1. **`engine/collision/collision_object.h`** — the seam interface, with
   `CollisionObjectType` (`game/objects/i_collision_game_object.h:11-15`) split
   into engine-side layer/mask filtering plus an opaque game tag, exactly as
   `docs/design/PHILOSOPHY.md:398-400` specifies.
2. **Move the narrow phase** out of `engine/math/collision_tools.*` into
   `engine/collision/`, and give it the `{normal, penetration}` manifold
   PHILOSOPHY already names, with analytic (MTV) resolution.
3. **Fix the dispatch** — it fires both objects' responses off one participant's
   predicate (`game/objects/level.cpp:245-248`, `:280-284`) and tests every pair
   twice.

**Why the resolver's four live defects belong here and not earlier.** All four
disappear under the manifold design: the band case resolving on the axis that
cannot separate it (`engine/math/collision_tools.cpp:187-198` — the thin-platform
bug that shoves a player sideways off a platform it should have landed on), the
containment fallback picking the axis opposite to its own comment (`:105-117`),
the signed `std::max` over direction components (`:232-239`), and the bracket
whose failure is unreportable and whose result is discarded (`:53-56`,
`:262-271`, which welds a deeply-penetrating object in place silently, every
frame). Fixed in place they are four small edits plus four tests — all thrown
away by this item. See §4.

**Files.** New `engine/collision/collision_object.h`,
`engine/collision/narrow_phase.*`, `engine/collision/resolve.*`;
`engine/math/collision_tools.*` (mostly deleted);
`game/objects/i_collision_game_object.h` (deleted);
`game/objects/level.cpp`, `game/objects/player.cpp`,
`game/objects/structure_*.cpp`.

**Closes.** Finding `#13`, outstanding since 2023. Cluster 31. The four resolver
defects. The `on_collision` half of open question 2.

**Size.** **1–2 weeks.**

---

#### C3 — A real state stack

**What changes.** `StateContext` (`engine/core/state_context.h:8-18`) is a
single slot, so the one client has grown a stack out of nested contexts and heap
scalars. Give it `push`/`pop` and a typed result channel; then delete
`GameLevelState`, the three nested `StateContext`s, the three
`unique_ptr<enum>` out-parameters and the `unique_ptr<bool>`
(`game/states/game_states.h:28`, `:60`, `:64`, `:67`), and collapse the four
`*MenuData` classes into one composed context.

**Two bugs visible in the shipped game are its acceptance criteria.**

1. **The viewport layout is never restored.**
   `game/states/game_states.cpp:134` is the only writer of the layout in the
   entire repository, and it runs on entry only; the five exits at `:216-223`,
   `:256-265`, `:266-275`, `:276-285`, `:293-302` leave split-screen applied. So
   after a four-player match the main menu is drawn four times into
   quarter-screen viewports. A push/pop pair is what makes "restore what I
   replaced" structural instead of a call somebody has to remember at five
   sites.
2. **Pausing pauses the simulation and nothing else.** The frame a pause is
   detected (`game/states/game_states.cpp:155-158`), `Level::update` stops being
   called — and `Level::update` is the only thing that ever stops a looping
   weapon voice (`game/objects/weapon.cpp:242-259`). The level music
   (`game/objects/level.cpp:74`) plays at full volume under the pause menu, and
   a player who pauses mid-burst leaves a sustained tone running. The engine
   already ships the fix and it has zero callers:
   `SoundBank::pause_effect` / `resume_effect`
   (`engine/audio/sound_bank.h:54-55`). A stack gives the level the obvious
   place to hang "quiet down while something is above me", which is what both
   bugs actually want.

**Files.** `engine/core/state_context.{h,cpp}`, `game/states/game_states.{h,cpp}`,
`game/states/pause_menu.*`, `game/states/end_menu.*`,
`game/states/results_menu.*`, `game/states/*_menu_data.*`.

**Closes.** Clusters 13, 14 and 41. Three `high` findings in
`game-states-flow`. The "child state reporting a result" and "something happened
to the level" halves of open question 2.

**Size.** **1 week** — the smallest of the three structural items, and the one
that produces the most visible change in the shipped game.

**Definition of done.** The sample can show a second state above the first and
get a result back.

---

### Phase D — parallel, and blocked by nothing on the spine

These three touch files B1, C1 and C2 never open. They are the right work for a
second stream, or for a week when the seam is stalled on a decision.

---

#### D1 — `engine/ui/`: focus, navigation, and a button

**What changes.** `engine/ui/` currently ships six widget classes and none of
the three things `docs/design/PHILOSOPHY.md:378-381` says the module exists for.
`engine/ui/widget.h:59-74` already exposes `bounds()`, which is everything a
focus ring and a directional walk need. Focus is a widget pointer *per
viewport*; navigation is a nearest-neighbour walk over `bounds()`; a button is a
widget plus a "what a press means" callback the game supplies. None of the three
touches `draw()`, so none is blocked by B1 — though `engine/ui/widget.h` is
edited by both, so land the rename (A5) first and expect one merge.

Also here: take the resolution-rescale policy out of the widget base class
(`engine/ui/widget.h:22`, `:45-49`, `engine/ui/widget.cpp:79-92`), which is
already producing a visible overflow, and fix the two focus bugs that only exist
because focus is hand-written per screen — the focused widget snapshotted before
the input loop so one pad's move redirects another pad's press, and a button
held when a menu opens delivered to that menu as a fresh press.

**Files.** `engine/ui/`, `game/states/main_menu.cpp`, `game/states/menu_page.*`.

**Closes.** Clusters 33, 34 and 35, plus two `high` findings in
`game-menu-framework`.

**Size.** **1 week.**

**Why it is worth doing beside the seam rather than after it.** It is the
largest single reduction available anywhere in `game/`:
`game/states/main_menu.cpp` is 1,976 lines and a large fraction of it is a
hand-written per-screen adjacency table keyed by `std::string` widget names
compared every frame. And the *per-viewport* focus PHILOSOPHY promises for
split-screen exists nowhere, so a second game gets none of it.

---

#### D2 — `engine/input/`

> **Landed. The duplication was the symptom; the polling boundary was the
> defect.**
>
> **Two copies were never the real cost.** The item reads as a de-duplication
> — one module instead of `player_input.cpp` and `menu_input.cpp`. But each
> copy also owned its own previous-frame store, and each advanced it only on
> the frames its owner happened to be running: gameplay's while a match was up,
> the menu's while a menu was. An edge is "down now, up last frame", so both
> were wrong at every transition, and both had grown a `prime()` with a
> paragraph of comment describing a bug it had shipped. Four hand-written
> `prime()` calls in `game_states.cpp` were the running total. `Application`
> polls once per frame, before any state updates, and all four go — not because
> they were consolidated but because the thing they compensated for stopped
> existing.
>
> **The two copies had already disagreed about the physical stick.** One asked
> the pad API for a circular deadzone and the other for none, so the same
> stick position meant two different things depending on which was reading. The
> module asks for none and applies the deadzone once, in engine code, with the
> magnitude rescaled — which is also the filed double-deadzone fix, and the two
> turn out to be the same fix.
>
> **The sample is the acceptance test again, and it lost a member.**
> `ConfirmState` carried a `bool ready_` and a comment: the B that opened the
> question is still down on the frame it opens, so it had to be released before
> it counted. With a real press edge that member is gone — a state no longer
> has to notice the transition it was created by. `DirectX::GamePad` has left
> `application.h`, `game_data.h`, both sample states and the game's precompiled
> header; it survives in `engine/input/xinput/gamepad_reader.cpp` and nowhere
> else.
>
> **`ConnectionState` is deleted rather than used, and that is the honest
> answer.** It was a two-valued enum written in two files and read in none — a
> `bool` with ceremony. `Gamepads` reports `connected`, `just_connected` and
> `just_disconnected`, which is the mechanism a real player-to-pad binding
> needs; the binding itself is game policy this item does not have the standing
> to decide, so the weld is documented at `PlayerInputData::connected` rather
> than quietly half-fixed.
>
> **What is deliberately not built:** the action map. ARCHITECTURE gives this
> module "devices and action mapping" and this is the devices half. Neither
> client has a rebinding screen, so a data-driven binding table would be T1's
> speculative framework. `tests/input/` is new, and it is a folder that could
> not have existed before: the edge logic is free functions over two
> `GamepadState` values, so testing it needs no controller plugged into the
> machine running the tests.

**What changes.** `engine/input/` is one enum
(`engine/input/connection_state.h:1-10`) and the real input API is a raw
`DirectX::GamePad*` on `engine/app/application.h:17`, `:123`, `:158`. Device
polling, deadzones and press-edge detection are written twice in the game —
`game/objects/player_input.cpp:18-37`, `:120-146` and
`game/states/menu_input.cpp:18-36`, `:133-154` — and the two copies have already
drifted. Move the mechanism into `engine/input/`, with the XInput specifics in
`engine/input/xinput/`.

Fold in while you are there: the stick deadzone applied twice in `Player`, the
second time without rescaling; and the connect/disconnect edge, because
`ConnectionState` is assigned in two files and read in none. That deadness has a
runtime price — a player is welded to an XInput slot for the whole match
(`game/objects/level.cpp:191-207`), XInput user indices are not stable across a
replug, and a controller that comes back in a different slot silently drives a
different character with nothing on screen to say why.

**Files.** `engine/input/`, `game/objects/player_input.*`,
`game/states/menu_input.*`, `engine/app/application.h`.

**Closes.** Cluster 32, plus the replug finding and the deadzone finding.

**Size.** **1 week.**

---

#### D3 — Guard the asset loaders

> **Landed — and it was not an `engine/assets/` item, because the header it
> was filed against was the *game's* JSON entry point too.**
>
> **The remedy as filed does not reach the defect.** "Make the guarded readers
> the public API" reads as promoting three static functions out of
> `asset_manifest_loader.cpp`. It cannot be that: they take
> `const rapidjson::Value&`, so promoting them keeps rapidjson in a public
> header and `PRIVATE` stays a fiction. What the item actually wanted was a
> *type* — `JsonValue` and `JsonDocument` in `engine/assets/json.h`, whose only
> rapidjson is a `void*` recovered inside `json.cpp`. Every accessor checks the
> receiver's kind as well as the member's, and throws naming the file and the
> position in it: `'./levels/turbulence.json': collision_objects[17] has no
> 'colour'`. `tests/assets` dropping its own rapidjson include is the check
> that the leak is closed, and it is a build-file line rather than an assertion.
>
> **The scope was in `game/`, not `engine/`.** `json_loader.h` was included by
> five `.cpp` files and only three were the engine's; the loader whose reads
> were least checked and whose file is most hand-edited was
> `game/objects/level_object_builder.cpp`, with 55 unguarded accessor calls
> against level JSON. `LevelLoadedInfo` said so in its own comment — "the rest
> of this class still reaches into the document unchecked, that is the loaders'
> standing validation debt, and it is not paid here" — and that comment is what
> the item was really filed against. `save.cpp` keeps rapidjson, and always
> will: the save file is the one JSON this project writes, and `assets` only
> reads.
>
> **One thing was found by doing it rather than by the review.** The game's own
> rejections — an unknown colour name, an unknown object or collision type —
> named neither the file nor which object, and two of them threw
> `std::exception`, which is MSVC-only. `JsonValue::where()` exists for that: a
> caller with its own reason to reject what it read gets the same sentence
> pointing at the same place. Fixing the reads without fixing those would have
> left a level file that says which key is missing but not which object had the
> unknown colour.
>
> The three fold-ins landed as written. The dead `catch` was real: DirectXTK's
> `WaveBank::CreateInstance` answers null, so a definition naming a missing wave
> had been registering a null instance that failed later, elsewhere. One filed
> finding in these files is deliberately left: `resource_loader.cpp` reports
> "the content names something that is not there" as `out_of_range` twice and
> `runtime_error` twice, and picking one is an API decision rather than a guard.

**What changes.** `engine/assets/sprite_sheet_loader.cpp:11-67` and
`engine/assets/sound_bank_loader.cpp:14-82` read JSON through rapidjson's
asserting accessors, which `NDEBUG` disarms — so a hand-edited content file is a
null dereference in Release. The guarded readers already exist
(`engine/assets/asset_manifest_loader.cpp:10-55`) and are private to one `.cpp`;
the module's only public JSON API is a raw `rapidjson::Document`
(`engine/assets/json_loader.h:12`). Make the guarded readers the public API and
route both loaders through them.

Fold in: the dead `catch` in the wave-name check
(`engine/assets/sound_bank_loader.cpp:24-35` — DirectXTK returns null rather
than throwing, so the header's promise at `sound_bank_loader.h:14-16` is not
kept); and texture load failures that report an `HRESULT` with no file name
(`engine/assets/resource_loader.cpp:108-122`), unlike the font loader twenty
lines below them.

**Files.** `engine/assets/`, `tests/assets/`.

**Closes.** Clusters 15, 16 and 42.

**Size.** **Days.**

---

### Phase E — after the files stop moving

---

#### E1 — MattMath's real dependencies, and the module walls

> **Landed, in four commits — and two of the four items below were wrong about
> the code. Recorded here because the rest of this document is worth reading
> with them in mind.**
>
> **Item 1 was too small.** It budgeted for moving `d3d_viewport()` and the
> `RECT` conversions, and expected MattMath to keep linking DirectXTK because
> the library "currently wraps" it. Counting callers first said otherwise:
> across all forty-six DirectX-typed constructors, accessors and assignment
> operators in `matt_math.h`, thirty-nine had **zero callers anywhere**, and
> every one of the seven live ones was in a single file,
> `engine/render/d3d11/renderer.cpp`. Every DirectX-typed constructor
> duplicated a `Vector2F` or `RectangleF` one that already existed, so nothing
> lost an entry point. All forty-six are gone, five free functions in the
> backend replace them, and MattMath links nothing at all. The general lesson,
> and it applies to E2: **count the callers before budgeting the move.**
>
> **Item 4 cannot be done before the repo split, and the reason is
> structural.** "Engine sees only itself; game and sample receive the root
> through the engine's `INTERFACE`" is not reachable while `engine/` and
> `game/` are siblings: includes are written from the repository root
> (CONVENTIONS), so the engine's own include root *must* be the directory
> above `engine/` — which is the directory that also holds `game/`. No include
> path admits `"engine/render/renderer.h"` and refuses `"game/objects/level.h"`
> from there. What E1 did instead is the prerequisite: the include roots are
> `CMAKE_CURRENT_SOURCE_DIR`-relative, so the engine now configures and builds
> under a foreign top-level project (verified, not assumed), and the split is a
> move rather than a build rewrite. A6's grep stays as the wall until then, and
> ARCHITECTURE says so rather than describing a compiler error that does not
> happen.
>
> Items 2 and 3 landed as written. Three entries from §4 closed with them:
> `d3d_viewport_ptr()`'s `reinterpret_cast`, `DIVIDER_COLOUR`'s
> dynamic-initialisation order, and `colour_consts`' 301 internal-linkage
> objects per translation unit — the palette is `static const Colour` members
> defined once, constant-initialised.
>
> Two defects were found by the move rather than by the review.
> `Colour::set_from_hex` threw `std::invalid_argument` on a right-length string
> whose digits were not hex, straight past the `else` that promises opaque
> black; and `saturate`/`desaturate` had byte-identical bodies. Both are fixed,
> with tests, in `tests/render/colour_tests.cpp`.

**What changes.** Four things that are one build edit and one set of moves:

1. **Strip the backend conversions out of `mattmath::Viewport`.**
   `engine/math/matt_math.h:3-5` includes `SimpleMath.h` and `<d3d11.h>`;
   `engine/math/CMakeLists.txt:15-18` links `Microsoft::DirectXTK PUBLIC`, and
   its comment at `:1-2` cites ARCHITECTURE as its authority for doing so.
   `Viewport::d3d_viewport()` (`engine/math/matt_math.h:797`, defined at
   `engine/math/matt_math.cpp:3196`) has two live callers, both in
   `engine/render/viewport_manager.cpp` (`:24`, `:65`) — move it, the `RECT`
   conversions and `Viewport::d3d_viewport_ptr()`'s `reinterpret_cast` into
   `engine/render/d3d11/`, the folder B1 created.
2. **Move `Camera` (`engine/math/matt_math.h:1020-1057`), `Viewport`
   (`:775-811`) and `Colour` (`:594-649`) into `engine/render/`.** They are
   render-module types living in `math/`.
3. **Get the game nouns out of the engine.** The `TEAM_*` palette at
   `engine/math/colour.h:308-312` and the paint-shooter's sprite-sheet and frame
   names at `engine/render/viewport_manager.h:15-21` — including
   `DIVIDER_COLOUR` at `:21`, an inline variable initialised from a
   dynamically-initialised constant in another TU.
4. **Narrow the include roots.** `engine/CMakeLists.txt:46-49` and
   `engine/math/CMakeLists.txt:14` both publish `${CMAKE_SOURCE_DIR}` `PUBLIC`,
   so an engine file including a game header compiles and links. Engine sees
   only itself; game and sample receive the root through the engine's
   `INTERFACE`. Same edit: mark the rapidjson include `SYSTEM` and make it
   `PRIVATE` so it stops escaping into every client, and replace
   `CMAKE_SOURCE_DIR` with `CMAKE_CURRENT_SOURCE_DIR`-relative paths — the
   current form breaks the moment the engine is consumed as a submodule, which
   is the stated repo topology, and it is why `samples/minimal/` cannot
   configure standalone.

**Files.** `engine/math/matt_math.{h,cpp}`, `engine/math/colour.h`,
`engine/math/CMakeLists.txt`, `engine/CMakeLists.txt`, `engine/render/d3d11/`,
`engine/render/viewport_manager.h`, `samples/minimal/CMakeLists.txt`,
`tests/*/CMakeLists.txt`.

**Closes.** Clusters 3, 4, 5, 6, 7 and 8 — seventeen findings, and the
seven-finding "MattMath depends on nothing" cluster that is the most-cited
violation in the review.

**Size.** **1 week.**

---

#### E2 — Tuning into data

**What changes.** Weapon, projectile and player tuning are C++ aggregates in
headers, so a tuning change rebuilds a third of the game — against T7 and
`docs/design/PHILOSOPHY.md:409` ("Tuning changes rebuild nothing"). Roughly 593
SCREAMING constant declarations across the tree are the same defect wearing a
naming violation.

Two content bugs belong here because they are numbers in files, not code:

- **The end-of-match camera frames a third of the arena.**
  `game/content/levels/turbulence.json:74-80` gives `zoom_out_finish_bounds` a
  width of 3840 against a 6000-wide arena, in all three shipped levels; the
  height field is never read, because
  `Camera::calculate_camera_from_view_rectangle`
  (`engine/math/matt_math.cpp:4158-4160`) derives the vertical extent from the
  width and the aspect. Measured against the paintable structures, the shot that
  decides the match shows 82% of `close_quarters`, 13% of `king_of_the_hill` and
  14% of `turbulence` — every team A spawn and no team B spawn. Fixing the API
  alone leaves the three files wrong; fixing the three files alone leaves the
  next author's `height` silently discarded. Do both.
- **The countdown is centred against a guessed text size.**
  `game/objects/level.h:40-41` hardcodes a 400×600 box; the rendered text is
  212×445 for a digit and 752×445 for `"GO!"`, so the countdown sits 94 px left
  of centre and jumps 270 px right on its final beat, every match, at every
  resolution. `TextObject` has measured the real size since the handle pass and
  nothing reads it. This wants B1's `measure()` and the `text_bounds()` fix that
  goes with it.

**Files.** `game/objects/weapon_consts.h`, the projectile tuning aggregates,
`game/objects/player.h`'s constants, `game/content/levels/*.json`,
`game/objects/level.{h,cpp}`, `engine/math/matt_math.cpp`.

**Size.** **1 week.**

**Sequencing note.** E2 shares `game/objects/` with C1 and C2. Do it after them,
or accept the merges.

---

#### E3 — Correct the documents

**What changes.** Most of the "documentation inaccuracy" findings resolve
themselves: PHILOSOPHY and ARCHITECTURE describe the destination, and B1 through
E1 are the journey. Four do not, and these are the ones worth an edit:

1. **`docs/design/PHILOSOPHY.md:371-372`** — "render workers own disjoint slices
   of the scene". They do not, and they cannot: the parallelism axis is views,
   every worker draws every object, and the pure-read `const draw()` is the only
   thing making it sound. This is wrong about the *destination*, not about the
   present, and it is the tenet that governs the design of B1. Amend it.
2. **`docs/design/PHILOSOPHY.md:207-211`** — T11's escape clause says it governs
   "the engine's own code and everything it ships", and `:314-315` enumerates
   the shipped set as "the sample game, the tutorials". The paint-shooter is not
   in that list. Twenty findings in this review cite T11 against `game/` code, two of
   them `high`. Say explicitly whether `game/` is in or out; either answer is
   fine, the ambiguity is not.
3. **`docs/review/IMPLEMENTED.md:46`, `:68-78`** — finding `#16` is recorded as
   "the data race is fixed, the redundancy is not". The race is *not* fixed:
   `draw()` is a pure read of ArtAttack's own types, and the race is one level
   deeper, inside a `const` DirectXTK method. A3 is what fixes it. Correct the
   line now, because it is wrong today, and mark it fixed when A3 lands. This is
   the one status line in that document not to trust.
4. **`docs/design/ARCHITECTURE.md:33-39`** — the `tests --> engine` edge. A1
   makes it true; until then it is an edge no target satisfies.

Everything else in the docs-accuracy sweep — the module table at `:130-133`, the
tree at `:82-91`, `external/` at `:105`, the renderer-seam claims at
`:277-279` and `:367-370`, the `dt`-is-a-parameter claim at `:358` — becomes
accurate as the work lands. Amend those *in the PR that makes them true*, which
is what `docs/design/PHILOSOPHY.md:27-29` already asks for.

**Size.** **Hours**, spread across the other items.

---

## 4. Do not fix these

Every item below is a real, correctly-reported finding. Fixing any of them
directly is work that survives a few weeks and is then deleted. Three of them
are worse than neutral: their *remedies as written* install a property on code
that should not exist, or cannot be satisfied where they are filed.

| Finding, and where it lives | Deleted by | Note |
|---|---|---|
| The per-frame partition vector copied into every task lambda — `game/objects/level.cpp:401-410` | **B1** | The fan-out moves behind the seam |
| "Give `ThreadPool` a `parallel_for` to unify the two hand-written fan-outs" — `engine/core/thread_pool.h:20-29` | **B1** | The seam's per-view submit *is* the `parallel_for` |
| The `RestoreContextState` divergence — `TRUE` at `game/states/game_states.cpp:337` vs `FALSE` at `game/states/menu_page.cpp:164` | **B1** | One submit path afterwards |
| "Document `Level::draw`'s three caller obligations" — `game/objects/level.h:80-82` | **B1** | The parameters go |
| "Document `Level::sampler_state_` as a loan" — `game/objects/level.h:149` | **B1** | The member goes. **The caching *bug* is separate and real** — device loss frees the `CommonStates` in two places (`game/objects/level_builder.h:39` too). Take the one-line stopgap (read `PointClamp()` at draw time) in A2; do not document the loan |
| `MenuPage` indexes deferred contexts and sprite batches by widget ordinal, capping every menu at 16 objects — `game/states/menu_page.cpp:175-181` | **B1** | The view is the unit, not the widget |
| `MenuPage`'s non-const draw helpers running on 16 threads | **B1** | `State::draw(Renderer&) const` makes it a compile error |
| `Application::sprite_batches()`'s `const_cast` — `engine/app/application.cpp:429-432`; `GameData::dt()`'s `const_cast` | **B1** | Both members go |
| "Give `Drawer` a `protected` destructor" — `engine/render/drawer.h:10-14` | **A5** | **Actively misleading**: it installs a safety property on a class with zero callers for all three of its helpers. Delete the class |
| "Add `<cmath>` to `moving_object.cpp`" | **A5** | Both callers are among the 13 dead accessors |
| The `MatrixF` sizing bug — `engine/math/matt_math.*` | **A5** | Do not size the storage. Delete the subsystem; it has never executed and nothing calls it |
| `Level::update_level_logic` declared `const` and rewriting the world — `game/objects/level.h:161-162` | **C1** | The method splits across the boundary |
| `Level`'s 21 positional constructor parameters — `game/objects/level.h:57-77` | **C1** | Eleven of the members go to `Scene` |
| `Level`'s seven undocumented borrowed pointers — `game/objects/level.h:104`, `:127-129`, `:149`, `:154-155` | **C1** | Four of the seven move; document the three that remain, afterwards |
| `count_projectiles` re-implementing `is_projectile` — `game/objects/level.cpp:682-706` | **C1** | Rescans every collision object per viewport for one debug line |
| The four resolver defects — `engine/math/collision_tools.cpp:187-198`, `:105-117`, `:232-239`, `:53-56` + `:262-271` | **C2** | Four small edits and four tests, all thrown away by the `{normal, penetration}` manifold. The thin-platform bug is live and visible; if it is intolerable before C2, fix *only* `:187-198` and accept that one test is disposable |
| "Fix `#13`: the collision interface carries a game enum" — `game/objects/i_collision_game_object.h:11-15` | **C2** | **Cannot be fixed where it is filed.** The file is in the game, so there is no engine type for the enum to be removed *from* until C2 creates one. Doing it in place produces an opaque tag in a game header — the same finding in different clothes |
| The three `unique_ptr<enum>` out-params and the `unique_ptr<bool>` — `game/states/game_states.h:28`, `:60`, `:64`, `:67` | **C3** | The stack's typed return channel replaces all four |
| The four `*MenuData` classes and their `const GameData*` divergence | **C3** | One composed context |
| `GameLevelState`'s missing `break` and two `default`-less switches — `game/states/game_states.cpp:200-309` | **C3** | The enum goes |
| "Rebuild a sub-menu's context before its data" — `game/states/game_states.cpp:160-172` | **C3** | Latent today; the ordering disappears with the nested contexts |
| `MainMenu`'s per-frame string-keyed widget lookups and per-screen adjacency table — `game/states/main_menu.cpp` | **D1** | Focus and navigation replace the table |
| `MObject::name()` existing only for the game's per-frame string comparison — `engine/ui/widget.h:14`, `:29` | **D1** | Same |
| The individual `player_input.cpp` / `menu_input.cpp` divergence findings — `game/objects/player_input.cpp:18-37`, `game/states/menu_input.cpp:18-36` | **D2** | One module, not two reconciled copies |
| `Viewport::d3d_viewport_ptr()`'s `reinterpret_cast` — `engine/math/matt_math.h` | **E1** | The conversion leaves the library |
| `ViewportManager::DIVIDER_COLOUR`'s dynamic-initialisation order — `engine/render/viewport_manager.h:21` | **E1** | The constant leaves the engine |
| `colour_consts` being 301 internal-linkage objects per TU — `engine/math/colour.h` | **E1** | Same move; fix the linkage there, once |

**One more, of a different kind.** `Shape`'s polymorphic `clone()` on the
engine's most-copied value, and the fixed-size geometry returning `std::vector`
(`engine/math/matt_math.h`, filed `high` twice) — do not start this before C2.
C2 rewrites its heaviest caller, and a value-semantics pass over a type whose
call sites are about to change is the same work twice.

---

## 5. Sequencing

### If you have one week

The goal is not to start the seam. It is to have a regression net, no live
undefined behaviour, and the seam's contract written down before a single line of
it is implemented.

| Day | Item |
|---|---|
| 1 (morning) | **A1** — link `ArtAttackEngine` from `AssetsTests` and `CoreTests`; create `tests/render/` and `tests/collision/`. Three lines in two build files. Also **A6**, the include grep, which is an hour |
| 1 (afternoon) | **A3** — `std::wstring` text. The one live race, gone |
| 2 | **A4** — deferred `transition_to`, plus the first `tests/core/state_context_tests.cpp` |
| 2–3 | **A2** — camera border, `player_viewport(4)`, the unvalidated divisors, drop `ViewportManager`'s unread `DeviceResources*`, with tests in the folders A1 created |
| 4 | **A5's subtraction half only** — `Drawer`, `MovingObject`'s dead accessors, `MatrixF`, `MenuElement`, `LevelMode`, `LevelEndInfo`'s six, ~500 lines of commented-out code. No renames yet |
| 5 | Write `engine/render/renderer.h` as declarations and comments and *nothing else*: the six constraints from §1.1, the concrete-class decision, and the three sample greps that define done |

**Explicitly not this week:** `Scene`, MattMath's D3D dependency, the naming
sweep beyond deletions, tuning into JSON, and starting the seam's implementation
on a Friday.

**If a second person is anywhere near this repository, add A7** — half a day,
and it is the difference between a clone that runs and one that throws
`Failed to load wave bank` at startup.

### If you have a month

Four weeks. The goal is the seam landed and the sample proving it.

| Week | Item |
|---|---|
| 1 | The one-week plan above, **plus A5's rename half** — `IGameObject` → `GameObject`, the `M` family, `ICollisionGameObject` → `CollisionObject`. Mechanical, zero-risk, and it is what stops week 2's diff from being a rename and a redesign in the same hunks |
| 2–3.5 | **B1** — the seam, and all four signature changes, in one pass. Port `samples/minimal/` first: 85 lines, and it is the gate |
| 3.5–4 | **C3** — the state stack. It is the smallest structural item, it needs only B1, and it closes two bugs a player can see: the main menu drawn into four quarter-screen viewports after a four-player match, and the weapon loop and music playing under the pause menu |

**Do not start C1 (`Scene`) in month one.** It is two weeks on its own, and its
acceptance criteria depend on B1's view list having been settled *in practice* —
by the sample and by `MenuPage` — not just on paper.

**If there is a second stream that never opens `engine/render/`:** run **D1**
(ui focus and navigation) alongside weeks 2–3. It is the largest single
reduction available anywhere in `game/` — `game/states/main_menu.cpp` is 1,976
lines — and it delivers the per-viewport focus PHILOSOPHY promises for
split-screen and that exists nowhere today. **D2** (input) is the same shape and
the same size if D1 is taken.

### Month two, for orientation

**C1 and C2 in parallel**, then **E1** once the files have stopped moving —
because E1 is a build edit whose whole value is that it breaks the build when
something moves, and it should be landed at a moment when nothing is.

Then the plan is spent, and what is left — E2, E3, and the long tail of `low`
findings in the appendix — is maintenance rather than sequencing.
