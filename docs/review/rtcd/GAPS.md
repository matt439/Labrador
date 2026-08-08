# What the sweep missed

> The completeness critic's account of the mining run, written against the slices
> on disk and the tree at `5101879`. It is deliberately unflattering.

---

## What this sweep missed, and what it got wrong

This section is the sweep's account of itself. Every claim below was re-checked against the slices on disk and against the worktree at `C:/VSProjects/ArtAttack/.claude/worktrees/rtcd-mining`. Where a gap is small I say so; three of them are not small.

---

### 1. Sections the book covers and no finding cites

Coverage was good in the chapters the themes were built around. The holes are concentrated in **slice-01 (Chapter 2, pp.1–22)**, which no theme owned properly, and in one section of **slice-21**.

#### 2.4.2 Sequential Versus Simultaneous Motion (slice-01, pp.15–16) — the largest single omission

Zero citations across six theme reports, and it is the section that describes what C2 actually changes.

The game today (`game/objects/level.cpp:204-324`) is **simultaneous movement, sequential resolution**: every object's `update(dt)` runs first, then a collision pass tests and resolves one pair at a time, mutating positions as it goes — `player->on_collision(other)` moves `rectangle_` immediately (via `CollisionTools::resolve_object_collision`, `player.cpp:292,312,321,330,352,361,369`), so the next pair in the same loop is measured against the already-corrected position.

`find_contacts` / `dispatch_contacts` is a different model. Every manifold is measured against one frozen snapshot, then all responses fire. Ericson names exactly this trade in 2.4.2, including the property the engine is about to give up: *"One of the benefits of the sequential movement model is that an object nonpenetration invariant is very easy to uphold."* Under the snapshot model a player wedged in a corner gets two separations computed from the same pre-resolution position, and they partly cancel — a bug the current code cannot have.

The convexity-geometry theme's LP archive item brushes against this (*"a player wedged in a corner receives two translations that partly undo each other"*) but attributes it to `resolve.h`'s expressiveness, not to the model change, and does not notice that today's sequential resolve is what prevents it. Every theme treats C2's migration as pure cleanup. It is a semantics change, and 2.4.2 is the section that prices it. Ericson also blesses the sequential model for games outright (*"For games... the problems introduced by a sequential movement model can often be ignored"*), so this should have produced at minimum a T3-shaped "already-have" finding and at most a warning attached to C2.

#### 7.1.5 Implicit Grids, bit-array form (slice-21, pp.291–293) — the broad phase never considered the one grid that fits

The broad-phase theme surveyed uniform grids, hgrids, quadtrees, k-d trees, BVHs, sort-and-sweep, and rejected the grid family on Ericson's case 4 — an object-size spread of 5×5 mist projectiles against a 6000×1000 `boundary_floor` makes one cell size unchoosable. Verified: extents run 300×10 up to 100×7000, 6000×1000. That rejection is correct **for the dense grid and the grid-of-lists**, which is what the reject item names.

It is not correct for the implicit grid. Ericson's second form (p.292) allocates `int32 rowBitArray[GRID_HEIGHT][N/32]` and `columnBitArray[GRID_WIDTH][N/32]`, sets one bit per overlapped row and one per overlapped column, and answers a query with `b = (r[i] | r[i+1] | ...) & (c[j] | c[j+1] | ...)`. Three properties the theme's own criteria were asking for and none of its candidates delivered together:

- A huge object costs `O(rows + cols)` bit-sets, not `O(rows × cols)` list insertions — the size spread that killed the dense grid does not apply.
- Two fixed contiguous arrays, no nodes, no lists, no allocation after the first frame. This is the T11 shape the theme said the tree families only reach after recasting.
- The candidate set arrives **as a bitmask**, so an object straddling four cells appears once by construction. That dissolves the entire "canonicalise or break the pinned contract" finding (broad-phase item 3 / convexity-geometry item 4) rather than fixing it.

It is not free: the query is `O(n/32)` words per object, so total work is `O(n²/32)` — a ~16× constant factor at n≈1000, not the `O(n log n)` the sweep offers, and at 1000 objects the row/column arrays are 128 bytes per entry. Whether it beats a one-axis sweep on this workload is exactly the question no benchmark exists to answer. But it belonged in the comparison, and the reject item ("The pointer-linked families — BVH, quadtree, k-d tree, hgrid, **grid-of-lists**") names the variant that loses and omits the one that might win.

#### 2.3 Types of Queries (slice-01, pp.13–14)

Uncited. Ericson's taxonomy — boolean test, intersection finding, contact manifold, penetration depth, separation distance, TOI — is the frame for deciding what `Manifold` should carry. `Manifold` is normal + depth and no contact **point**, and the engine has no distance query at all. That is defensible, but it is a decision, and the only place it appears in the whole document is a parenthetical inside a bounding-volumes archive item (*"if contact positions are ever wanted — impact sparks, paint placed where the projectile hit rather than at its centre"*). Given that painting-where-you-hit is a live gameplay behaviour, the query-type question deserved a paragraph. Minor, but it is the section that would have produced it.

#### 2.6 Robustness and 2.7 Ease of Implementation (slice-01, pp.19–20)

Both uncited, and 2.6 arbitrates a policy question the whole document skirts — see §3 below. 2.7's criterion *"how many tweaking variables are involved (such as numerical tolerances)"* is the book's own framing for the epsilon-family finding, and the robustness-perf theme reached that finding without it.

#### 5.5.1 Interval Halving (slice-17, pp.215–217) and 5.5.2 Moving SAT (p.226)

Both uncited. 5.5.2 is a genuine gap given that the speed-cap finding turns on whether a swept test is needed. 5.5.1 is the nearer miss: the narrow-phase theme's headline asserts *"The book also independently rejects the 40-iteration bisection resolver C2 deleted"* — an uncited claim about a section nobody mined, about code that is not deleted (§2 below).

#### Correctly thin, no action

`6.7` front tracking and `6.6.7` grouping queries (slice-20) — there is no hierarchy and no front to track. `5.1.6` tetrahedra, `5.4.4` three-plane intersection (3D). `5.1.10`, `5.3.6`, `7.1.6`, `7.3.6` loose quadtrees, `13.2` instruction cache, `10.1`, `10.5`. The thinness there is the book's dimensional bias, not the miners'.

---

### 2. Claims in the reports that are false

#### "No cap exists" — false, and it contradicts a sibling theme

robustness-perf: *"grep finds no cap: ... nothing clamps it in `game/objects/weapon.cpp` or `projectile.cpp`."* The clamp is in `projectile.cpp`, on **both** axes, at lines 231–237 (y) and 258–264 (x), against `projectile_consts::MAX_VELOCITY = {5000, 5000}`. The narrow-phase theme cites that same constant as "the cap" and then says *"wind_resistance at :241-256 touches x only"*, which reads as though there were no x clamp; there is one, immediately after.

The substantive conclusion survives and is the interesting one: the cap exists but is set at 5000 px/s, which is 83 px per fixed 1/60 s step, against a thinnest collidable of 300×10 present in all three levels (verified from the JSON). So the cap is real, is enforced, and **does not achieve the thing PHILOSOPHY says it achieves**. That is a better finding than "no cap exists" and it is a different one — it is a tuning defect, not a missing mechanism.

#### `triangles_intersect`'s four-of-nine loop — three themes, two verdicts, and the dissenter is wrong

- narrow-phase: sufficient, with a proof sketch.
- convexity-geometry: sufficient, brute-forced over 400k pairs.
- robustness-perf: *"provably incomplete by reading... The fix is two nested loops to 3"*, with a claimed counterexample.

robustness-perf is wrong, and its counterexample is geometrically impossible. The argument: if the containment pass at `matt_math.cpp:363-369` failed, no vertex of either triangle lies in the other, so every vertex of `A ∩ B` is a crossing. `∂A ∩ B` is a union of arcs, each contributed by one edge of A (an edge meets a convex set in a connected piece), and there are at least two such arcs, so **at least two of A's three edges participate** — hence at least one of `a_edges[0]`, `a_edges[1]`. A participating edge has both endpoints outside B, so it enters and leaves through **two distinct edges of B** — hence at least one of `b_edges[0]`, `b_edges[1]`. The four tested pairs cover it. Their proposed configuration has `a_edges[0]` crossing only `b_edges[2]`, which requires an endpoint of `a_edges[0]` inside B, which the containment pass already caught.

This matters because robustness-perf files it as `[adopt | value 3]` with a code change. Applying it would add work and a test pinning a case that cannot occur. The correct action is convexity-geometry's: leave it, comment it, delete it with the rest.

#### "The bisection resolution is deleted" — it is live game code today

The decisions preamble states *"Bisection resolution deleted"*, and every theme wrote against that. It is not deleted. `engine/math/collision_tools.cpp:13-14` still defines `BRACKET_ITERATIONS = 40` and `ITERATION_POWER = 1.5f`, and `resolve_object_collision` (`:255-265`) routes to `bracket_object_collision_generic` whenever either shape is not a rectangle. `Player::on_structure_ramp_collision` (`player.cpp:254`) calls `resolve_object_collision` with a `RectangleF` against a `TriangleRightAxisAligned`, so **every player-versus-ramp contact runs 40 iterations of `Shape::intersects` today**. The math-primitives theme's ramp finding treats the resolver as a dead-weight wrapper; it is the live cost of the only non-rect-rect pair in the game.

#### "Snaps the player against the ramp's bounding box, not its hypotenuse" — false

math-primitives offers this as a strengthening correction. `bracket_object_collision_generic` (`collision_tools.cpp:64-77`) tests `collider->intersects(collidee)` — the exact shape, not the bounding box. The resolve is shape-accurate; what is wrong is that it is 40 iterations of a geometric search where a manifold gives the answer in closed form, and that the step is a fraction of the *collider's own size* (`move_object_by_direction_relative_to_size`), which is a different defect from the one claimed.

#### "The second sweep is a duplicate of the first, reversed" — false, and it is a survey error the themes inherited

`Player::is_matching_collision_object_type` (`player.cpp:150-155`) returns `is_structure(other_type)` — a player's own `is_colliding` accepts **structures only**. `Structure::is_colliding` returns `false` unconditionally (`structure.cpp:39-42`) and `StructurePaintable` does not override it. So:

- sweep 1 (`level.cpp:254-281`) is **player → structure**, and nothing else;
- sweep 2 (`level.cpp:291-305`) is **projectile → player**, and nothing else.

They are disjoint, not duplicated. Sweep 2 is the only path by which a projectile damages a player. Deleting it as redundant — which "duplicate of the above, reversed" invites — removes all projectile damage.

#### Smaller factual slips

- math-primitives: `set_velocity_y(0.0f)` appears in **six** ramp branches (`player.cpp:297,317,326,335,357,374`), not five.
- convexity-geometry: *"every coordinate in every level JSON is a multiple of 5"* — false as stated. Spawn x's are 72/134/196/5752/5814/5876/5938 and decor rectangles include 4138, 4396, 4632, 4792, 1388. It is true of `collision_objects` rectangles, which is what the arguments actually need, so the conclusions stand; the stated premise does not. robustness-perf's narrower version (*"exactly one non-integer numeric value"* — `music_volume: 0.3`) is exactly right.
- convexity-geometry: "18/32/34 collision objects" — it is 18/33/34.

#### Claims I re-verified and found correct

`EPSILON = 1e-4` below one ULP above 1024 while levels run to 7000; the `FLT_EPSILON` test boundaries collapsing to bit-identical `10.0f` (`math_tests.cpp:48,53,326-332`); zero `assert` calls in `engine/` or `game/`; `separation_along` guarding the divisor at `1e-4` (`resolve.cpp:18`); `Vector2F::cross`, `mattmath::sign`, `rotate_vector_by_ref` all broken and all dead repo-wide; `intersect_moving_AABB_AABB`, `test_circle_circle`, `hypotenuse()` dead with no tests; `union_of` untested; `Player::bounds()` = 452×520 for a 52×120 collider (37.7×); paint-tile totals 1898/4116/5230 with a 1500-tile maximum; `edges_valid` at twelve square roots plus one allocation; the 41×25 sweep; the positional assertions at `contacts_tests.cpp:98-102`; no `/GL`, no IPO, no benchmark target; `engine/collision/` reached from `game/` only via `partitioner.h`.

---

### 3. Where the reports let the engine off easy

#### The engine throws from inside the frame loop, and the tree already contains the lesson

Three themes praise `narrow_phase`'s circle throw as principled (*"silently missing collisions is the worse failure"*), and a fourth pins it with a test. Nobody asks whether **throwing** is the right alternative to missing.

`level.cpp:326-329` carries this comment, in the same module, about a bug that was already fixed:

> *"A projectile that leaves the level is ordinary — it missed. Retire it with everything else being deleted this frame rather than throwing, **which terminated the process because nothing on the tick path catches**."*

The engine learned, wrote down, and fixed the fact that a throw on the tick path is a process kill. Then the newest primitive on the tick path was written to throw — `narrow_phase.cpp:80-86` throws `std::invalid_argument` for `ShapeType::circle` and for `ShapeType::none`; `polygon_from`'s rotated-rectangle branch can throw out of `Quad::is_valid()`; `Player::on_collision` (`player.cpp:249-252`) throws `std::exception("Invalid collision object type.")` from inside the collision loop; `Shape::intersects(const Shape*)` throws on `ShapeType::none`. Ericson 2.6 (slice-01, p.19) is the section that arbitrates: *"When faced with such problematic inputs, a robust program provides the expected results. A nonrobust program may in the same situations **crash** or get into infinite loops."* An uncaught throw from a per-frame query is a crash.

The robustness-perf assert finding gets close — it observes that *"a throw cannot be compiled out, so a per-pair invariant check would be a permanent T8 tax"* — but treats it as a reason the engine has no asserts, not as a reason the engine's existing throws are misplaced. The honest finding is a stated policy: throw at the API boundary, assert on the frame path, and return a documented degenerate answer where neither is available. Three of the four throw sites above are on the frame path.

#### The layer/mask migration silently disables projectile damage, or crashes

`layers_collide` is symmetric by design (`collision_layer.h:43`: `(a_layer & b_mask) && (b_layer & a_mask)`), and `contacts.h` states both directions must agree. The game's filters are **asymmetric**: `Player::is_matching_collision_object_type` accepts structures only, while `Player::on_collision` (`player.cpp:195-253`) handles structures *and* opposing-team projectiles and throws on anything else. The two functions disagree deliberately, because the projectile side drives that pair.

Nothing in the document notices. The survey documents `Projectile::is_matching_collision_object_type` as the authority on "pairs that really occur" and never reads the player's. Two consequences for C2, neither raised:

1. Derive masks from `is_matching_collision_object_type` — the obvious source, and the one the survey wrote down — and the player's mask omits projectiles, the symmetric AND vetoes every player/projectile pair, and **all projectile damage stops**. Silently: there are no tests for `Player` at all.
2. Derive masks at role granularity (PLAYER / PROJECTILE / STRUCTURE) instead of team granularity, and same-team overlaps are now enumerated, `Player::on_collision` falls to its `else throw`, and the process dies on the first friendly-fire frame.

This is a bigger migration hazard than the contact-ordering item that broad-phase ranked at value 5, and it is not in the document at all.

#### The boundary convention flips at C2, and it is a behaviour change, not a safe composition

bounding-volumes correctly identifies that `test_AABB_AABB` is closed (touching intersects) and `test_axis` is open (touching does not overlap), and files it as a *safe filter composition*. It is safe inside `find_contacts`. It is not neutral across the migration: the game's `is_colliding` short-circuits to `true` on the closed AABB test whenever the other shape is a rectangle (`player.cpp:175-184`, `paint_tile.cpp:70-78`), so **flush contacts are collisions today**. Under `find_contacts` they stop being contacts. In a level whose collision rectangles are all multiples of 5 and whose paint tiles are laid edge to edge by construction, exactly-flush is the common configuration. That is a gameplay-visible change nobody costed.

#### The tolerance family is larger than the epsilon findings admit

The reports enumerate `EPSILON`, `SEGMENT_PARALLEL_EPSILON`, the hardcoded `0.0001f` default, and `edges_valid`'s relative form. They omit `BRACKET_ITERATIONS = 40` and `ITERATION_POWER = 1.5f` from the tolerance census because they believed that code was gone. Those are two live tuning constants with no derivation, no stated convergence bound, and no test — the exact thing Ericson 2.7 counts as a design cost. The geometric series `Σ 1.5^-i` converges to 2, so the bracket can never move an object more than twice its own extent; nothing states that, and nothing checks that a ramp penetration stays inside it.

#### Two smaller ones

`Structure::bounds()` returns `sprite_rectangle_`, a *different member* from `collision_shape_` (`structure.cpp:36-38`, `structure.h:37`). They are equal today only because `level_object_builder.cpp:58-60` passes the same rectangle twice. The `bounds() ⊇ shape()` precondition that robustness-perf proposes writing into `game_object.h` on account of `Player` is unenforced for every `Structure` too, and there the type actively invites divergence. Minor today, real the moment a sprite and a collider differ.

`RectangleF` has no invariant — a negative width inverts `top_left()`/`bottom_right()`, makes `test_AABB_AABB` reject every pair, and makes `polygon_from` emit reversed winding so `narrow_phase` returns an *inverted* normal that pushes bodies together. This appears once, in a corrections aside inside another finding, and never as a finding. It is one `from_top_left_bottom_right` call away from level JSON.

---

### 4. Findings that are fashionable rather than justified

#### The flat extent array, counted three times at value 5

bounding-volumes ("Fill a POD AABB array once per frame"), broad-phase ("Build the frame's collision records into one flat array"), and robustness-perf ("Linearise find_contacts into a flat extent array") are **one change**, rated 5 three times. A reader scanning the document sees triple corroboration where there is a single idea. Worse, all three cite Ericson 2.4.1 — *"Reducing the cost associated with the pairwise test will only linearly affect runtime. To really speed up the process, the number of pairs tested must be reduced"* — and then propose the linear half first. broad-phase's defence (it is the substrate the sweep needs) is the only one that holds, and it holds only if the sweep follows. Rated honestly for a function with **no production caller** and **no benchmark**, against a PHILOSOPHY line that says optimisation follows a profile, this is one finding at value 3–4, not three at 5.

#### The rect-versus-rect analytic fast path

Rated 5 by bounding-volumes and 5 by math-primitives; narrow-phase rates the same territory at 5 for the *axis reduction* and 3 for the fast path, and is right. The axis reduction is provably inert — I re-derived it: negating an axis swaps `forwards` and `backwards` in `test_axis` and leaves the normal and penetration bit-identical, so a rectangle's four cardinal axes are two tests doing four tests' work. It needs no second code path and no equivalence test. The Minkowski special case adds a second implementation of one answer, on a path nothing calls, to save arithmetic nothing has measured. Two themes ranking the special case above the free reduction is the hot-loop reflex, not the evidence.

#### `signed_area` for an n-gon (convexity-geometry, value 4)

Justified as *"the primitive the convexity check, the winding enforcement, the ear-clipping ear test and any polygon collider all sit on."* Of those four: ear clipping is archived in the same report, polygon colliders do not exist and `ShapeType` is a closed set of five, winding is not enforced anywhere, and the convexity check wants four fixed cross products, not a `std::span` fold. `Polygon::MAX_POINTS = 4`. This is a general-case primitive for a codebase whose largest polygon is a quadrilateral, and by the document's own repeated standard — *"a caller-less primitive could not satisfy the contract obligation the review set via partitioner.h"* — it belongs in archive, not adopt. The other half of that finding (fix or delete `Vector2F::cross`) is sound and should not be dragged down with it.

#### `Player::bounds()`, counted four times

Four themes file it as four findings, with four different area ratios (19×, 9×, "400×400", 37.7×). It is one S-effort change. The duplication is honest — each theme found it independently — but the document should merge it, or a reader will conclude the sweep found four problems where it found one.

#### The contract-comment family

At least six findings across the themes reduce to "add a header comment": the completeness claim in `narrow_phase.h`, the ORIENT2D sentence on `signed_2D_tri_area`, the boundary-direction rule in `contacts.h`, the `center()`-is-the-vertex-mean note, the Voronoi comment, the `ericson_math.h` contracts. Each is individually defensible and cheap. Collectively they are the lowest-risk thing a mining exercise can recommend, and they inflate the count. They should be one work item.

---

### 5. What the sweep did well, briefly

The 3D material was translated or rejected by name and never smuggled in as 2D. Every "this is not a live cost" correction I re-checked was right, and the discipline of writing those corrections is what makes the rest of the document trustworthy. The three highest-value correctness items — `closest_pt_point_segment`'s NaN, `test_AABB_AABB`'s NaN-permissive form, and `separation_along`'s guard on the wrong quantity — are all real, all cheap, and all verified at the stated lines. And the sweep found something better than an algorithm: it found that `PHILOSOPHY.md:423-425` promises benchmarks, `PHILOSOPHY.md:71-72` promises a tunnel-proof speed cap, and `manifold.h` promises two invariants, and that none of the three is implemented. That is worth more than any technique in the book.
