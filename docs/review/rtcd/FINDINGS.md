# RTCD findings, by engine area

> Mined from Christer Ericson, *Real-Time Collision Detection* (2005), pages 1-551.
> Method, counts and the ranked shortlist: [README.md](README.md).
> What this sweep missed: [GAPS.md](GAPS.md).

---

## The state of play

Ericson's book is already in the building. `engine/math/ericson_math.cpp` carries fifteen live ports with the source credited at the top, and the ones that matter are correct: `closest_pt_point_triangle` is the full seven-region Voronoi form, `closest_pt_point_OBB` loops `i < 2` with a comment saying why, `intersect_moving_AABB_AABB` is x/y slabs only, and `test_triangle_AABB` was left commented out rather than half-ported. The file's last line is a bare `// 132` — mining stopped at page 132 and the file says so. This document is what pages 133 through 551 have to offer.

Three facts govern everything below, and two of them contradict the raw material.

**`engine/collision/` has no consumer outside `tests/`.** `grep` for the module across `game/` returns only `partitioner.h` (from `level.h:19` and `level_builder.h:15`). `Level::update_level_logic` (`game/objects/level.cpp:201-360`) runs three hand-written sweeps against the game's own `game/objects/collision_object.h`, and the six copied `is_colliding` blocks short-circuit to `true` on the AABB test whenever the other shape is a rectangle. So the ~10⁶ pair tests per frame are real, but they are not in `find_contacts` and not in `narrow_phase`. Every performance argument here is about what the engine path will cost *after* C2 ports the game onto it. That makes now the right time to fix it and the wrong time to claim a regression.

**There is no benchmark harness.** `PHILOSOPHY.md:423-425` already promises one — "Benchmarks pin throughput the way tests pin behaviour... A throughput regression is a defect, not a curiosity" — and `tests/` holds six correctness folders and no timing target. `cmake/settings.cmake` sets `/W4 /WX /permissive- /sdl` and no optimisation flags, no `/GL`, no `CMAKE_INTERPROCEDURAL_OPTIMIZATION`. T8 is a headline value with no instrument, which means most of the speed claims in this document are well-argued guesses. The mining agents' own arithmetic about where the cost lies was wrong by three orders of magnitude, which is itself the argument for building the instrument.

**Most of the numeric surface was never designed, only inherited.** `mattmath::EPSILON = 1e-4` is smaller than one float ULP anywhere above 1024, and every level runs to 6200. Two range tests are written in the form Ericson names as the NaN trap. Four ported routines divide with no guard. There is not one `assert` call in `engine/` or `game/`.

Verdicts read: **adopt** (take it as the book gives it), **adapt** (the idea is right, the 2D value-first form differs materially), **already-have** (the engine arrived there independently), **archive** (correct, translated, no caller yet — recorded with its trigger), **reject** (declined, with the reason recorded so it is not re-proposed).

---

## The broad phase

`find_contacts` (`engine/collision/contacts.cpp:15-54`) is a literal `for i / for j = i + 1` double loop, and `contacts.h:40-46` says so in its own words and names itself as the insertion point: *"the signature does not change when it arrives, because 'which pairs are worth measuring' is exactly the question the two cheap filters above already answer badly."* `Partitioner` splits index ranges for the thread pool and does no spatial work despite the name. I grepped `engine/`, `game/`, `tests/` and `samples/` for grid, quadtree, octree, BVH, sweep, Morton and broad_phase: nothing but prose.

Chapters 6 and 7 are written for irregular primitive soups in unbounded worlds, and this engine is none of that. All three levels declare `out_of_bounds` as exactly `{-100, -100, 6200, 6200}`. Eighteen, thirty-three and thirty-four collision objects are authored per level and every one is static — `Structure::update` is empty (`game/objects/structure.cpp:28-30`) and `Structure::is_colliding` returns `false` unconditionally (`:39-42`). Against that sit 600-1000 transient projectiles which, per `Projectile::is_matching_collision_object_type` (`game/objects/projectile.cpp:90-115`), collide with structures and the opposing player team and never with each other. There are no ray queries anywhere.

### Fill a flat extent array once per frame, before enumerating any pairs — adopt

Ericson's sweep operates over a standalone contiguous array of boxes read as plain data — `gAABBArray[j]->min[gSortAxis]` — never re-derived from the underlying geometry during the sort or the sweep (7.5.2, pp.336-337). The general rule he states by example, and again for BVH leaves (6.3.4, pp.260-261), is that whatever the pair loop needs per object gets materialised once per object, not once per pair. 13.5.1 (pp.531-535) calls the same move *linearisation*.

Count the current inner loop. Per candidate pair it calls `b->for_deletion()`, `a->layer()`, `a->mask()`, `b->layer()`, `b->mask()` — five virtual calls — before `layers_collide` can reject it, and `layers_collide` (`engine/collision/collision_layer.h:40-44`) is a `constexpr` test on two `uint32`s. Four more follow if the pair survives: `shape()` twice, and `Shape::AABB_intersects` (`engine/math/matt_math.cpp:122-125`) calling virtual `bounding_box()` on both sides, each materialising a 24-byte polymorphic `RectangleF` of which eight bytes are a vptr the broad phase will never dereference. In this workload nearly every pair is projectile-against-projectile and dies on the mask, so almost the entire cost is virtual dispatch guarding a bitwise AND.

A per-frame `struct { float min_x, min_y, max_x, max_y; std::uint32_t layer, mask, index; }` array, built in one pass and reused across frames, drops that to *n* virtual calls and turns the inner loop into integer and float compares on cache-resident data. It changes no contract, forces no ordering decision, and it is the substrate any sweep, grid or SIMD path would later want. **This is the cheapest large win in the module and it should land before anything else here.**

*Philosophy:* T8 and T11 exactly — contiguous values, no dispatch on the hot path, and CONVENTIONS already says a virtual call per item is a T8 tax. Mild T4 tension: for the duration of one function there are two representations of the same objects, which has to be said in `contacts.h` or it reads as duplication.

### Sweep that array on one axis instead of enumerating all pairs — adapt

Sort ascending by box minimum along one axis, then for each entry scan forward and stop the instant `min[j] > max[i]`, because no later box can reach back (7.5.2, pp.336-337). The forward scan replaces the textbook active-interval set, so nothing is inserted, erased or allocated during the sweep. 10.4 (pp.423-425), stripped of its depth buffer, is the same two-pass ordered prune.

The honest counterweight first: the sweep's stated worst case is objects clustered on more than one axis, and that is this game's steady state — the sprayer has `shoot_interval = 0.0f` (`game/objects/weapon_consts.h:44`) so it emits one projectile per player per frame from one muzzle, and the mister emits five per 0.1 s. Hundreds of projectiles sit in a tight ball for several frames. But those pairs are all vetoed by the mask, and after the extent array a vetoed pair costs one AND and a compare on data already in the cache line. What the sweep prunes is exactly the pairs that would otherwise reach `bounding_box()` and `narrow_phase` — the pairs that cost anything. That is the argument the header owes, rather than a benchmark it cannot yet cite.

Do not build the sorted doubly-linked-list variant of 7.5.1. `game/objects/level.cpp:340-349` swap-and-pops the whole collision vector every frame, so neither object identity nor array position survives between frames for an incremental structure to exploit.

### Sort on the minimum and break on the maximum, or containment is silently lost — adopt

Ericson warns (7.5, p.333) that the obvious optimisation — report only boxes whose endpoints lie between the query box's min and max — is wrong, because a much larger box enclosing the query has both endpoints outside that span and is never seen. The array sweep is immune only because it sorts by minimum and each box scans forward from itself: the enclosing box has the smaller min, appears earlier, and finds the small box during its own scan.

This is not hypothetical. The level JSON contains a 6200×100 ceiling, a 100×7000 boundary wall and a 6000×6000 backdrop; anything inside one of those is a containment case, and containment is precisely what `narrow_phase` was rewritten to get right — its own header lists "one shape wholly containing the other" among the four defects it replaces, and `tests/collision/narrow_phase_tests.cpp:84` pins it. A broad phase that drops those pairs would undo the fix from the layer above without failing a single collision test; it would just stop reporting. State the cost honestly too: a map-spanning box makes its own forward scan run most of the array, so the sweep is O(n) for that one object. With a handful of such structures that is the correct price.

A pinning test — a small box wholly inside a very large one, at both array positions — belongs in the same commit, per the A2/C2 precedent that the specific defect being prevented gets its own test.

### Pair-once is free today, and the existing test pins contact *position* — adopt

In any partition where one object can occupy several cells, the same pair is enumerated once per shared cell (7.7, p.341), and Ericson's point is that the real hazard is duplicated collision *response*, not wasted narrow-phase work. His fix where the key is a pair is to force an ordering before hashing (12.2.2, pp.479-480) — `if (vj > vk) Swap(vj, vk)` — so (A,B) and (B,A) land in one bucket.

Two things follow, and the second is the one to settle before writing a line. First, pair-once is currently a *structural* property of `j = i + 1`, and any multi-cell index destroys it; `dispatch_contacts` (`contacts.cpp:57-72`) fires `on_contact` once per participant per Contact, so a duplicated pair means a tile painted twice with its sound played twice, or a player separated twice in one frame. Second: `tests/collision/contacts_tests.cpp:99-102` asserts that `contacts[0]` names (a,b), `contacts[1]` names (a,c) and `contacts[2]` names (b,c). That is positional. A correct, non-duplicating sort-and-sweep breaks those three lines and reads as a regression when it is not. Either the assertions become order-independent and `contacts.h` states that contact order is unspecified — the honest and cheaper answer — or the broad phase is obliged to preserve index order.

*Philosophy:* T2 over T8. Single dispatch is already a documented contract (`collision_object.h:38-46`, `contacts.h:30-31`), and an optimisation must not quietly weaken a contract other code was written against.

### Choose the sort axis from centre variance, computed in the same frame — adapt

Accumulate per-axis sum and sum-of-squares of AABB centres, form `s2 - s*s/n`, sort on the axis with the larger value (7.5.1 tail, p.338). Ericson computes it during one sweep and applies it on the next, through a file-scope `gSortAxis`, and warns that objects resting on a floor cluster on the vertical axis (7.5, p.330 and Figure 7.20).

Two adaptations, both improvements on the source. `find_contacts` is a free function over a span with no frame-to-frame memory and the engine is committed to multi-core work, so the book's mutable global is unavailable — and unnecessary, because the extent array is being built anyway: accumulate the variance in that same pass and sort on the better axis *this* frame. That deletes the state, the staleness and the global. Second, the 3D advice ("track x and z, drop y") has no 2D escape hatch: there are only two axes, and gravity guarantees y clusters. So x is the correct default for a side-view platformer and the variance is the guard, not the mechanism. The axis type already exists as `mattmath::Dimension` (`engine/math/dimension.h`).

### Record why a uniform grid is unsizeable for this content — adopt

Cells must be at least as large as the largest indexed object for the neighbour rules to hold; too coarse and the grid degenerates to all-pairs, too fine and insertion cost explodes for large objects, and when sizes vary greatly you get both at once (7.1.1, pp.286-287) — which is why hierarchical grids exist (7.2, pp.300-301).

Measured, not assumed: collidable extents inside the 6200×6200 world run from 15×15 projectiles and a 52×120 player up to 6200×100, 100×7000 and 6000×6000. Sizing cells to the largest object gives a one-cell grid; sizing to the projectile puts the ceiling into hundreds of cells and the wall into hundreds more. That is Ericson's case 4 in its pure form, and it is a fact about the authored content rather than a tuning oversight. The escape routes are two tiers — real machinery for 18-34 static objects — or a structure with no cell size at all (7.5, p.329). That is the argument for the sweep, and it belongs in the header as what was rejected and why, in the house style `contacts.h` and `narrow_phase.h` already set.

### Never build the index over an object's surface parts — adopt

Hierarchies built over an object's surface primitives detect surface *crossings* only; if one object lies entirely inside another, no surface primitives intersect and the query reports no collision (6.4.2, p.263, restated for sphere trees at 6.4.4, p.264).

This is preventative, and the temptation is concrete: the appealing version of the paint-tile fix below is to treat a structure's four tile faces as its collision proxy, because they are already generated, already uniform and already indexable. That would silently reintroduce the exact containment hole `narrow_phase` was written to close. The rule for any index here is one line in the header, in the same voice as `narrow_phase.h`'s containment note: build it over whole-object `bounds()` rectangles, never over edges, faces or surface samples. `GameObject::bounds()` (`engine/core/game_object.h:67-75`) is already the right input and its own comment says so.

### Partition objects; never split shapes; store indices, never fragments — adopt

`BuildBSPTree` calls `SplitPolygon` and pushes a piece down each side, and Ericson quantifies the price: worst case each split introduces n more polygons, giving O(n²) output (8.3 listing p.356; 8.3.2, p.361). Even where splitting is done he recommends tagging each fragment with a reference to the original and outputting the *reference* in the leaf, which dodges slivers and T-junctions (8.3.4, pp.371-372).

This engine could not do the splitting even if it wanted to: there is no clipping, splitting or classification code anywhere in `engine/`, `ShapeType` is a closed set of five with no general convex polygon, and splitting the one `TriangleRightAxisAligned` ramp by a vertical line produces a triangle and a quadrilateral — the shape *type* changes under the split. Recording the decision (any index partitions objects one-leaf-each, or duplicates whole references into overlapping cells with the dedup rule above, and never touches a `mattmath::Shape`) deletes Ericson's entire 8.3.3-8.3.5 robustness section from the engine's future. That is several days of the hardest floating-point code in the book, removed by a paragraph. The corollary: internal nodes carry no objects, so a query pays nothing for passing through a divider.

### The static/dynamic asymmetry is real, and it fights the header's own promise — adapt

When two distinct groups are tested against each other, only one is inserted into the structure and the other is used purely as query objects (7.1.7, pp.299-300); static world data gets a build-once contiguous representation (7.1.4, pp.290-291).

The asymmetry here is sharp and verified: 18/33/34 authored objects, all static for the life of the match, against 600-1000 projectiles appended at runtime (`level.cpp:243`) and swap-and-popped every frame (`:340-349`). The honest problem is that `find_contacts` takes one `std::span<CollisionObject* const>`, `contacts.h:41-45` promises "the signature does not change when it arrives", and layer/mask does not encode static-versus-dynamic. Either `CollisionObject` gains a question it does not have or `find_contacts` gains a second span — and amending the header is a smaller cost than pretending the current signature already supports this (PHILOSOPHY:27-29 requires exactly that amendment in the same PR). Note also that once the mask veto costs one integer test, this buys much less than it appears to. Take it only if the benchmark asks.

Worth deflating one number: with 34 statics against 600-1000 projectiles, skipping the static×static block saves ~578 pair tests out of roughly half a million. The value of a static/dynamic split is the half a future index can build *once*, not the pairs it skips.

### Ericson's hashed uniform grid, in index-array form, as the alternative candidate — adapt

If a cell-based structure is wanted anyway, 12.1 (pp.469-474) gives a complete one that is already T11-shaped: `index = 0x8da6b343 * cell_x + 0xd8163841 * cell_y; return index % NUM_BUCKETS;` over unbounded, possibly negative cell coordinates, with `int first[NUM_BUCKETS]` (head, -1 for empty) and `int next[MAX_ITEMS]` — three stores per insert, no allocation, no world bound, no perfect hashing (collisions are harmless because every candidate is distance-tested anyway).

Two caveats bind here. Ericson explicitly disqualifies the simpler `|x| + |y|` sort key because it "has degenerate behavior when vertices are aligned to a regular grid" — and that is this engine's *normal* case, not a corner case: every coordinate in every level JSON is a multiple of 5, most of 25 or 50, so `(x+y)`, `(x^y)` and `(x*31+y)` keys all cluster badly and the two multiplicative primes are load-bearing. That deserves a comment, because the simpler key looks obviously fine. Second, the cell-size rule becomes "cell size at least the largest indexed AABB extent", which is exactly why `boundary_floor` must not share the index — so this lands as two indices, not one.

### Compute the overlapped paint-tile range by division instead of scanning all 1500 — adapt

Ericson's group-versus-object framing (7.6, p.340 and Figure 7.22; 9.1, p.385) is that the outer bound gates entry and something inside it must gate which primitives get measured. Where the contained primitives are laid out regularly, that gate is arithmetic (7.1, pp.285-287): floor of `(query_min - origin)/pitch` to floor of `(query_max - origin)/pitch`, clamped.

`StructurePaintable::on_collision` (`game/objects/structure_paintable.cpp:78-90`) loops every element of `paint_tiles_` calling `PaintTile::is_colliding` for every offensive-projectile contact. Tile counts computed from the level JSON: 1898 / 4116 / 5230 per level, and one structure holds exactly 1500. The tiles already *are* a uniform 1D grid per face, so the inverse mapping is one division and the scan visits one to four tiles. This is the single largest measured per-frame cost in the tree and the fix is entirely in `game/`.

Two corrections that decide whether the rewrite works. `generate_paint_tiles` (`:93-181`) builds **four separate 1-D strips** — top, bottom, left, right, each guarded by `faces_` — concatenated blind into one flat vector, so the mapping needs per-strip base offsets, not one 2-D division. And the pitch is **not** `paint_tile_consts::WIDTH = 4.0f`: `num_paint_tiles_x = static_cast<int>(std::fabs(width / WIDTH))` and `paint_tile_width = rectangle().width / num_paint_tiles_x`, so the stride is per-structure and only `THICKNESS = 12.0f` is constant. The derivation must reuse the generator's `left + paint_tile_width * i` form rather than an accumulating add, or the two drift by float error. That per-count derivation is deliberate and uncommented — it avoids a remainder sliver — and would be "simplified" away by the next reader; it deserves a line regardless.

The same four runs make `StructurePaintable::draw` (`:44-51`, every tile, per view, no per-tile cull, ~5230 × 4 views) and `update` (`:36-42`, every tile every frame) cullable.

### Build the benchmark before the broad phase, and count Nv and Np rather than milliseconds — adopt

Ericson's cost model (6.1.2, pp.237-238) is `T = Nv·Cv + Np·Cp + Nu·Cu + Co` — bounding-volume pairs tested, primitive pairs tested, and their unit costs — and he names the coupling: tightening the volume lowers Nv and Np but raises Cv and Cp, so the terms cannot be minimised independently. 7.2.1 (pp.302-303) adds that any tuning ratio "is application specific."

`Nv` maps exactly onto the `AABB_intersects` calls at `contacts.cpp:39` and `Np` onto the `narrow_phase` calls at `:44`. Those are *pinnable integers* on a fixed scene where wall-clock is not. Build 200 boxes resting in a row on one floor line (the shape of a real level) plus a tight cluster of 500 (the shape of a sprayer burst), and assert `Np` is under a number. `tests/collision/CMakeLists.txt` shows the pattern: an executable linking `ArtAttackEngine`, doctest and `artattack_settings`, registered with `add_test`, runnable by `ctest` with no IDE. Counters in the hot loop cost something, so they belong behind a compile-time switch — which is the T5 answer.

Ericson's own reference-algorithm discipline (2.7.1, pp.20-21) applies in the same commit and is free right now: the all-pairs loop *is* the brute-force oracle, and it is gone the moment the loop is edited. Lift it into the test target before replacing it, and pin the invariant that governs any accelerator — a filter may over-report freely and must never drop a pair whose shapes overlap (10.2, p.419; 10.4, pp.423-424; 4.4.1, p.106).

---

## The narrow phase and manifolds

`engine/collision/narrow_phase.cpp` is SAT over both shapes' edge normals with a minimum-overlap manifold and a first-separating-axis early out, returning `std::optional<Manifold>` over a value type. Ericson endorses all three decisions point for point in 5.2.1 (pp.156-158): test the face normals of both objects, exit on the first separating axis, keep the least overlap as the contact normal with its overlap as the penetration estimate. The book also independently rejects the 40-iteration bisection resolver C2 deleted, naming the parallel-slide worst case — an object moving alongside a surface just outside the termination distance — which is a platformer's steady state.

What follows is mostly finishing sentences the file already started.

### State that the face-normal axis set is *complete* in 2D — adopt

Ericson notes twice, in two different sections, that the simple test "are all vertices of A outside some face of B, or vice versa" is exact in 2D and fails only in 3D (4.4.1, pp.101-102), where two boxes can meet edge-to-edge with perpendicular edges and lie outside no single face of either. The nine `u_i × u_j` rows of Table 4.1 exist purely to catch that configuration. 4.6.5 (pp.121-122) calls the face-normals-only convex hull test "approximate" and "conservative" — describing, in 3D, exactly what this engine's narrow phase does. In 2D the cross of two edge directions is a scalar, not an axis, so that family does not exist and the caveat does not transfer. 5.2.10 (pp.172-173) makes the same point from the other side: nine of the eleven triangle-triangle axes are edge-edge crosses.

`narrow_phase.h:14-48` spends thirty lines on what the SAT replaces and why the old resolver's four defects were all the same defect. It never states that the axis set is necessary and sufficient. Adding the completeness claim and its 2D-only reason costs three lines and buys three things: it forecloses a future reader adding cross-product axes by analogy with published 3D SAT code; it closes the question of whether a GJK or proximity path is ever needed for accuracy (it is not); and it is the single clearest instance in the book of 3D machinery getting strictly *cheaper*, not merely different, when the dimension is fixed at 2 — PHILOSOPHY's claim that 2D is a design constant rather than a restriction, with a citation behind it.

The same paragraph owes the *precondition* the theorem rests on: SAT is a theorem about convex sets (3.8, p.64; 4.6, p.115; 9 intro, pp.383-384), and `Quad` does not currently enforce convexity. Either that sentence ships with the Quad fix below, or it says plainly that Quad does not enforce it yet.

### Test each distinct edge normal once — a rectangle contributes two axes, not four — adopt

Ericson counts one candidate axis per pair of parallel faces, not one per face: a 3D box has six faces and three axes (4.4 p.101, Table 4.1), and the k-DOP representation stores k/2 *slabs* for the same reason (4.6.2, pp.117-118). He also orders axes so consecutive ones are as perpendicular as possible (4.6.3, p.119), because the early out is wasted on near-duplicate directions.

`polygon_from` (`narrow_phase.cpp:38-49`) emits a rectangle's corners TL, TR, BR, BL, so its four edge normals are `(0,1)`, `(-1,0)`, `(0,-1)`, `(1,0)` — two directions, each tested twice. The redundancy is exact, not approximate: projecting on `-n` negates both interval endpoints bit-for-bit, so `forwards` and `backwards` in `test_axis` (`:116-160`) swap, the branch at `:147` flips with them, and the returned normal and penetration are identical — the duplicate can never win the strict `<` at `:204`. Better still, the axes are known without arithmetic: an axis-aligned box's are the literals, and a `rectangle_rotated`'s are its own `x_axis()`/`y_axis()`, already unit and validated by `axes_valid` (`matt_math.cpp:3584-3598`). Rect-vs-rect drops from 8 axis tests and 8 `std::sqrt` calls to 4 and none. The dedup must key off `shape_type()`, not an `i+2` index trick, because `mattmath::Quad` is a general quad whose opposite edges need not be parallel.

The existing pinning tests, including the 41×25 separation sweep at `narrow_phase_tests.cpp:157`, should pass unchanged. If they do not, the reduction was written wrong.

### Give the axis-aligned rectangle pair an analytic fast path — adopt

Ericson's tightness/cost trade-off has a degenerate best case: when the bounding volume fits the object exactly, the BV test is not a filter, it is the answer (4.1, pp.75-77). He applies the same reasoning structurally in 4.4.1 (pp.105-106): when the application constrains an OBB's orientation, the separating axes are known ahead of the test and the setup arithmetic vanishes. And 3.11 (pp.70-72) gives the closed form outright — the Minkowski difference of two AABBs is an AABB `(a.min - b.max, a.max - b.min)`, and they intersect iff it contains the origin, so the penetration is the smallest of `{a.right - b.left, b.right - a.left, a.bottom - b.top, b.bottom - a.top}` with the normal read off from which one won.

`RectangleF::bounding_box()` returns `*this` (`matt_math.cpp:687-690`), so the AABB filter at `contacts.cpp:39` has already proved the overlap *exactly* for the pair type that is essentially all of the game's collision traffic. Today `narrow_phase` then pays 64 dot products and 8 square roots to rediscover four constants. The fast path is not an approximation — for two AABBs, `test_axis`'s `min(forwards, backwards)` on axis `(1,0)` is exactly `min(b.right - a.left, a.right - b.left)`.

The sharpest correction in this whole exercise sits here: **the game code `narrow_phase` was written to replace already has this shortcut.** `paint_tile.cpp:70-78` and `player.cpp:175-184` both read "if the other object is a rectangle, then we have a collision since the AABB check passed". The engine regressed against its own game on the pair type that matters most.

*Philosophy:* T8 on the dominant path with zero allocation, and T5 in that the axes become literals. The cost is T4 — a second code path for a case the general one handles — bought off by a randomised equivalence assertion against the general path, which belongs in the same commit rather than as an optional extra. Land the distinct-axis reduction first: it is strictly less work for a bit-identical result and needs no second representation.

*Reject the related proposal* to defer normalisation generally by comparing `penetration²` cross-multiplied by squared axis lengths (5.2.3's scale-invariance note, pp.161-164). It is less readable, it changes tie-breaking at the margin, and there is no benchmark to arbitrate. The rectangle special case gets the same win with none of the risk.

### Stop routing rotated rectangles through `Quad` — it allocates three times, revalidates twice, and can throw — adopt

Ericson derives the box projection radius `r = e0|u0·n| + e1|u1·n|` precisely so a box is never enumerated as corners (5.2.3, pp.161-164), and lands on centre + orthonormal axes + half-extents as the preferred OBB representation for exactly that reason (4.4, p.101). He rejects his own lower-flop Plücker formulation one section earlier (5.3.4, pp.187-188) because "the extra memory accesses incurred are likely to make it slower" — the T11 argument, made by someone else in 2005. And 9.5.6 (p.408) states the storage rule outright: transform the direction handed to the support mapping, not the vertices, because "the first approach requires extra storage to hold the transformed vertices."

Follow what `narrow_phase.cpp:69-79` actually does for `ShapeType::rectangle_rotated`. It builds `mattmath::Quad quad(static_cast<const RectangleRotated&>(shape))`. That constructor (`matt_math.cpp:2801`) calls `RectangleRotated::quad()` (`:3511`) → `calculate_points()` (`:3543-3560`), which returns a `std::vector<Point2F>` **by value** — allocation one. The inner `Quad(const std::vector<Point2F>&)` runs `is_valid()` (`:2886-2903`), which calls `edges()` — allocation two, a `std::vector<Segment>` — and six segment-segment tests. The outer constructor then runs `is_valid()` **again**: allocation three, six more tests. Three heap allocations, twelve segment-segment predicates and two throw-capable validity passes, per shape per pair per frame, **inside the function whose own comment at `:16-20` says it carries polygons in an array rather than asking `Shape::edges()` because that would be "a heap allocation per shape per pair per frame, on the one path PHILOSOPHY's performance section names outright."** The file's stated invariant is already false for one of its four supported shape types.

There is also a live throw: a default-constructed `RectangleRotated` has zero extents, all four corners coincide, `segments_intersect`'s `a == b` shortcut (`matt_math.cpp:544-549`) reports every edge pair as intersecting, `is_valid` returns false, and the `Quad` constructor throws `std::invalid_argument` out of the middle of `narrow_phase` — which documents throwing only for circles and untyped shapes.

Minimum fix: read `center()`, `x_axis()`, `y_axis()` and `half_extents()` and write the four corners straight into `Polygon::points`. Better fix: the extent-radius projection, which needs no corners at all. Latent today — nothing in `game/` is a `RectangleRotated` — and worth taking now because it is small and because the same `Quad(RectangleRotated)` path is reached from `rectangles_rotated_intersect` and `point_rectangle_rotated_intersect`, which the math tests do exercise.

### Guard degenerate edges by length, not by testing `normalized()` against exact zero — adopt

When a candidate separating axis comes out near-null, all projections collapse toward zero and the separation inequality becomes `0 > 0`; in floating point the accumulated error on each side is random in sign, so a near-null axis can be misread as separating (4.4.2, p.106 — Ericson biases the radius side of the comparison). 11.2.3 (pp.439-440) names the underlying mechanism: subtracting nearly equal quantities leaves a result dominated by rounding error, and testing *that* against exact zero does not detect it.

`narrow_phase.cpp:188-192` computes `Vector2F(-edge.y, edge.x).normalized()` and skips the axis if `axis == Vector2F::ZERO`. `Vector2F::normalized()` (`matt_math.cpp:1363-1371`) returns zero only when `length == 0.0f` *exactly*, and `Vector2F::operator==` (`:1249-1252`) is exact float equality — so the guard is a tag check rather than a numeric one, and an edge of length 1e-7 passes it and contributes a unit vector whose direction was computed from a subtraction that lost most of its bits. Worse, the comment at `:184-187` explicitly credits `normalized()`'s zero-in-zero-out contract as "how a degenerate edge is skipped", so the comment claims a guarantee the code does not provide, and the guard silently breaks if the deferred value-semantics pass ever unifies `normalized()` with `to_unit_vector()`'s `(1,0)` contract (`matt_math.h:444-447` warns the two differ deliberately). `edge.length_squared() <= threshold`, with the constant named for the quantity it measures, removes both problems.

Do **not** put an epsilon on the `forwards <= 0.0f || backwards <= 0.0f` comparison. `manifold.h`'s contract and a pinned test fix touching-is-not-overlap at exactly zero, and Ericson's bias is on the radius side of the inequality, not on the boundary of contact.

*Philosophy:* T2 over T8 — `narrow_phase.h` already names silently missing collisions as the worse failure, which is why it throws on circles. T4 cost: this would be a third tolerance in a subsystem where `EPSILON` and `SEGMENT_PARALLEL_EPSILON` already disagree by two orders of magnitude, so it must be named and its quantity stated.

### State the least-penetration tie-break instead of inheriting whichever axis was tried first — adapt

Ericson stops mid-algorithm in `PointFarthestFromEdge` (3.9.2, p.68) to insist an extremal search resolve ties by a stated secondary rule — "to break ties between two points equally far along the perpendicular, the one projecting farthest along AB is selected" — because an arbitrary tie-break makes the result depend on input ordering.

`narrow_phase.cpp:204` uses strict `<`, so the first axis tried wins an exact tie, and "first" is a function of which polygon the caller passed as `a` — inside `find_contacts`, index order in the objects vector. There is no test asserting that `narrow_phase(a, b)` and `narrow_phase(b, a)` agree up to the sign of the normal.

Deflate the urgency, though: two axis-aligned rectangles overlapping corner-on-corner with equal x and y overlap are **not** order-dependent, because `polygon_from` emits the same four cardinal axes in the same order for both and `test_axis` is exactly symmetric under axis negation. Order-dependence is real only where the two polygons contribute *different* axis lists — rectangle versus triangle, i.e. the one ramp. Rank this as hygiene plus a missing symmetry test, not as a live grid bug. A stated rule (keep on `test.penetration < least.penetration - tie_epsilon`; on a tie prefer the more vertical normal, so a corner landing is a landing) costs one float compare per axis.

*Philosophy:* T2 as determinism — the same two shapes in the same positions must resolve the same way regardless of spawn order, and multi-core update makes order-dependence worse rather than better.

### Add `slide()` to `resolve.h` — remove the into-surface component instead of deleting the axis — adopt

Any vector decomposes uniquely against a direction into a component along it and a component perpendicular to it (3.3.3, pp.40-41, Figure 3.5b): for unit `u`, `p = (u·v)u` and `q = v - p`. With a unit contact normal in hand, the tangential part of a velocity is `v - normal * dot(v, normal)`. No trig, no square root, no branch chain, and in 2D the perpendicular is unambiguous.

`Player::on_structure_ramp_collision` (`game/objects/player.cpp:254-380`, reached from `on_collision` at `:245-247`) answers every ramp contact with `CollisionTools::resolve_object_collision(..., DIRECTION_DOWN)` followed by `set_velocity_y(0.0f)` — in five separate branches. That deletes the whole vertical velocity rather than only the component into the surface, so a player running up a slope loses their climb rate and a player landing on one stops dead instead of sliding. The resolution is wrong too, not just the velocity: every branch snaps the player against the ramp's *bounding box*, not its hypotenuse.

This is the piece that lets C2 **delete** `on_structure_ramp_collision` and `on_structure_collision`'s eight cardinal handlers (`player.cpp:419-587`) rather than wrap them: `narrow_phase` already returns the ramp's slope normal analytically and `narrow_phase_tests.cpp:123` pins it ("a ramp resolves along its slope, not along an axis"). The contract the house style requires: return the velocity unchanged when `dot(v, normal)` is non-positive, because a body already moving away from a surface is not being stopped by it.

*Philosophy:* T3 exactly — the simplest believable model of a slope is "you keep your tangential speed", and that is also the physical one, so simplicity costs nothing. It continues the committed shape of `resolve.h`: the engine offers the arithmetic and never performs it.

### `separation_along` guards the divisor's magnitude, not the output's — adopt

Ericson's discipline (11.7, p.463; 11.6, p.462) is that the acceptance test belongs on the quantity you actually care about, and that a routine must have a documented answer for every input it can be handed.

`engine/collision/resolve.cpp:14-24` reads `const float along = Vector2F::dot(normal, axis); if (std::abs(along) <= mattmath::EPSILON) return Vector2F::ZERO; return axis * (-penetration / along);`, with `EPSILON = 1e-4`. So `|along| = 1.0001e-4` passes the guard and returns a vector of magnitude ~10⁴ × penetration along `axis`. For a one-pixel overlap that is a ten-thousand-pixel teleport, silently, from the primitive whose whole job is to be the safe arithmetic the engine offers in place of the resolver it deleted. Either guard the output magnitude, or state in the header the maximum separation the primitive will ever return. `resolve.h` also documents that `axis` must be unit length and never checks it. **This is the sharpest single defect in the new collision code, and it is three lines.**

### Reuse the AABB filter's intervals for the rect-rect case — adapt

With an early out on the first separating axis, the *order* of candidate axes is a real performance parameter and should be chosen deliberately (5.2.9, p.169 — Ericson cites Akenine-Möller's measured 3-1-2 ordering for box-vs-triangle); axes already known not to separate should not be tested at all (5.2.1, p.158).

The AABB filter at `contacts.cpp:39` has already proved the world x and y axes non-separating for every pair `narrow_phase` ever sees. For two axis-aligned rectangles those are the *only* axes that exist, so no axis can produce the early out — and the comment at `narrow_phase.cpp:197-200`, which claims the early out makes the common case cost one projection pair rather than all seven, overstates itself for exactly the pair type the engine is built around. The filter also computed the two overlap intervals and threw them away, and `narrow_phase` recomputes them. Passing the depths through, or fusing the filter into `narrow_phase`, makes the rect-rect path nearly free. This does not violate `contacts.h`'s promise: that promise is about the outer signature, and nothing stops the filter and the narrow phase sharing intervals inside it.

### Translate the pair to a common reference point before projecting — adapt

Ericson's accuracy argument for local-space tests (4.2.2, p.81) comes with an escape hatch that needs no local spaces: subtract a shared reference point from all vertices before projecting, so the intervals are unchanged (the subtraction cancels) but computed near zero. 5.2.9 (p.170) says the same in the pair form — "the computations are simplified by moving one object — the symmetrical one, if present — to the origin."

Quantify it honestly, because the raw candidates did not. Level extents run to 6200 units and a float ulp there is 4.9e-4; the interval subtractions in `test_axis` are already exact by Sterbenz, so the error is only in the rounding of each dot product — about one ulp at 6200 units, roughly 5e-4 pixels. The smallest object in the game is a 4-unit paint tile and the player is 52×120, so a thousandth of a pixel is something T3 says the player cannot feel. Take the six subtractions for free while `polygon_from` is open for the axis reduction. Do not sell it as correctness, and do not open the file for it alone. Note where the concern is already absent by luck: `test_AABB_AABB` compares stored coordinates without forming differences, so the broad-phase filter is immune.

### Decide by measurement whether a resting contact needs a skin — adapt

Ericson's thick plane (11.3.2, pp.444-445, Figure 11.5 — drawn in 2D, and the text says so) exists because an exact intersection point lands on either side of a boundary about half the time; the remedy is a boundary with a radius larger than the maximum deviation, with the radius determined *empirically*. 11.5.3 (pp.459-460) adds that a constructed point should be biased to the safe side: "the actual position of Q is of less importance than Q lying on the correct side."

`separation()` (`resolve.cpp:9-12`) returns exactly `normal * -penetration`, which in exact arithmetic places the shape precisely touching — and touching is the state `narrow_phase.cpp:140` declares non-overlapping, with `manifold.h:21-23` stating penetration is always `> 0`. Meanwhile `Player::on_no_collision` (`player.cpp:1209-1220`) demotes `on_ground` to `in_air` after a single contactless frame, and jumping is gated on the ground state. The distance at stake is sub-pixel and nobody can feel it — which is precisely T3's argument that the simpler model wins — but the boolean it flips is a jump input, which *is* felt.

That is why this is adapt and not adopt: **do not add a skin speculatively.** Run the world-coordinate sweep below first. It either shows the current zero skin is justified and pins that fact, or it produces the smallest offset that restores separation, which is the number. If a skin is warranted it belongs in `resolve.h` as a named, measured budget next to the arithmetic — not as a knob.

The one-way platform sits on the same seam: `player.cpp:402` tests `prev_rectangle_.bottom() <= other_rect.top()`, two world-scale floats compared exactly, normally true only because the previous frame snapped `bottom()` to exactly that value. `prev_rectangle_` is the only previous-position state in the tree, and the engine-side version of the rule — a contact filter over a `Manifold` and a previous AABB — deletes it.

### `intersect_moving_AABB_AABB` reports a hit for boxes that can never meet — adopt

The book's listing (5.5.8, pp.230-232) only does work in the `v[i] < 0` and `v[i] > 0` branches, so an axis with exactly zero relative velocity contributes nothing — even when the projections on it are disjoint and, having no relative motion, can never meet. The listing never states the case; the sphere version two sections earlier (5.5.5, p.224) does have the corresponding early-outs.

Concrete repro against `engine/math/ericson_math.cpp:230-319`: `a = {0,0,1,1}`, `b = {5,10,1,1}`, `va = (0,0)`, `vb = (0,-20)`. `v.x == 0` so the x block (`:251-278`) is skipped entirely, the y block yields `tfirst = 0.45` and `tlast = 0.55`, and the function returns true for boxes five units apart in x forever. In 2D one dead axis is *half* the test, and axis-aligned levels make `v.x == 0` or `v.y == 0` the common case. The fix is four lines: per axis, before the sign branches, reject if the relative velocity is zero and the projections are disjoint.

The function has zero callers and zero tests anywhere in the worktree. So the honest alternative is deletion — CCD is declined by T3, and an untested, uncalled swept routine reads as if CCD were half-built. If it stays, it owes the contract the review already closed against `partitioner.h`: that `tlast` is initialised to 1 so the result is clamped to the step, that `tfirst == tlast == 0` means already overlapping, and that both are untouched on the early-out returns. Deleting it does not reopen CCD; it *records* that CCD was closed.

### The declined CCD ceiling rests on a speed cap that is neither derived nor enforced — adapt

This is the one place the book legitimately pushes back on a declared decision, and it pushes in T3's favour rather than against it.

Ericson opens the dynamic chapter (5.5, pp.214-215) by naming tunneling and giving the rule that avoids it without any swept test: the per-step displacement must not exceed the combined extent of mover and target along the direction of travel. 11.3.4 (p.448) states the same inequality as the fat-object rule — a test tolerates a gap `e` as long as `r > e/2` — and 2.4.3 (pp.16-17) says the time step must be short enough that movement is less than the spatial extents.

`PHILOSOPHY` T3 states "Projectile speeds get capped so nothing tunnels, instead of building continuous collision detection." Two independent derivations show the cap is not doing that work:

- **Gravity.** `game/objects/projectile_consts.h:15,:28` sets gravity 2500 px/s²; `projectile.cpp:226-238` clamps only at `MAX_VELOCITY = {5000, 5000}` (`projectile_consts.h:11`). Spray is 15×15 with a five-second life, and all three levels contain 300×10 platforms. The combined extent is 25 px, exceeded at 1500 px/s, which free fall reaches in 0.6 s — well inside the projectile's life, and the clamp does not bite until 83 px/step. Spray falling onto thin ledges passes through them, visible as paint that will not stick.
- **Muzzle speed.** `game/objects/weapon_consts.h:79` sets `DETAILS_SNIPER starting_vel_length = 2000.0f` and nothing clamps it. At the fixed 1/60 s step (`engine/app/application.h:39`) that is 33.33 px per step against a budget of `2h + t = 20 + 10 = 30` for the 20×20 jet against a 10-px platform.

(The two are consistent: `MAX_VELOCITY` clamps the *gravity integration*, and no cap at all applies to authored muzzle speeds. Neither number is derived from geometry.)

There is a second, tighter case with the same cause: paint tiles are 12 px thick, so a projectile whose leading edge lands 32-33.3 px inside a face overlaps the structure but not the tile band, `StructurePaintable::on_collision` finds no tile, and the shot registers as a hit that paints nothing.

**The fix is a named `max_step_displacement` constant living next to the geometry it protects, plus a load-time test asserting that every weapon's muzzle speed and every projectile's terminal speed times the fixed step stays under the smallest collidable extent.** That is T3's own rule — a simple model still has exact, documented, tested semantics — applied to the cap T3 already chose. It is not CCD: nothing computes a time of impact. If the cap cannot be met without making the sniper feel wrong, *then* a swept test becomes the question, and per the docs rule the PHILOSOPHY sentence gets amended in the same PR.

### Moving AABB against AABB is a segment against the Minkowski-inflated box — adapt

Two reductions the book uses repeatedly (5.5.5, Figure 5.35 p.225; 5.5.7, pp.228-230): subtract one velocity so only one object moves, then grow the stationary object by the mover and shrink the mover to a point. For a sphere the grown shape has rounded corners, so Ericson must expand to an AABB and repair the corner regions with capsules.

In 2D box-against-box needs no repair at all: the Minkowski sum of two AABBs is an AABB, so a moving AABB against a static one is exactly a segment against the static box inflated by the mover's half-extents. The whole capsule apparatus of 5.5.7 is an artefact of the sphere. `RectangleF::inflate(const Vector2F&)` exists (`matt_math.h:254`) and `test_segment_AABB` (`ericson_math.cpp:323-354`) is the book's SAT form correctly reduced to 2D — two face-normal rows plus the single surviving cross row, the x and y cross rows commented out. Four lines composing two existing primitives.

It answers *whether*, not *when*, so its use is a debug assert or a validation test for the speed cap above, not a resolver. Fix `test_segment_AABB`'s signature while there: `p0` is by const reference and `p1` by value (`ericson_math.h:114-115`). Note the caller inherits `SEGMENT_PARALLEL_EPSILON` at 1e-6, two orders tighter than `mattmath::EPSILON` and documented as a different quantity.

### The ramp is a textbook t-junction against the floor it stands on — adapt

A t-vertex is a vertex of one polygon lying in the interior of an edge of a polygon it does not belong to (12.3, pp.484-487). Ericson ranks three repairs: vertex collapse (preferred — it removes a vertex and can make a triangle degenerate enough to delete outright), edge cracking, and vertex snapping.

Verified against `game/content/levels/king_of_the_hill.json`: `mountain_level_1_ramp_east`'s base vertices are (300, 6000) and (1700, 6000); `boundary_floor` spans x 0..6000 with its top edge at y = 6000. Both base vertices lie strictly in the interior of that edge, and the ramp's base is collinear with the floor's surface for 1400 units. That is the classic internal-edge configuration: a body straddling the seam can take a horizontal MTV from one collider and a vertical one from the other in the same frame, and `Level::update_level_logic` dispatches the two independently.

The cheapest repairs are the two Ericson ranks first, and both are **content edits**: move the ramp's base to coincide with a floor rectangle corner, or split `boundary_floor` at x = 300 and x = 1700. The expensive repair — a runtime internal-edge filter carrying neighbour normals — is machinery this engine has no room for, and it would put a second input on `narrow_phase`'s currently clean `(Shape&, Shape&) -> optional<Manifold>` value signature. Latent until C2 wires `Level` to `find_contacts`; the diagnosis and the ranking of repairs are the deliverable.

### The circle axis is centre-to-nearest-vertex, with a containment fallback — archive

`narrow_phase.h:44-48` names the gap and the fix in one sentence: *"Adding them is one more axis, the centre-to-closest-point one."* The book supplies both missing halves precisely. A circle is convex and symmetric, so it projects onto any unit axis as `[dot(C,L) - r, dot(C,L) + r]` (3.8, pp.62-64 — the support mapping `S_C(d) = O + r·d/|d|`; 5.2.1, pp.156-157, Figure 5.14, which is literally a circle against an oriented rectangle). And 5.2.5 (p.166, Figure 5.18) warns, with a figure, that per-face slab tests alone do *not* decide circle-vs-box: a circle can sit outside a corner without lying fully outside either face meeting there. That is why the extra axis is mandatory rather than an optimisation.

The header's sentence is right but under-specified in two ways that would each cost a debugging session. The axis must come from the closest point on the polygon's **boundary**, not the closest point *in* the polygon: `closest_pt_point_triangle` returns `p` itself for an interior point, so using it yields a zero axis for exactly the deep-containment case that matters. The canonical minimal axis is **centre-to-nearest-vertex**, because a closest point lying on an edge produces an axis parallel to that edge's normal, which is already in the set. And when the centre is inside the polygon the direction is undefined and the face normals alone give the right minimum-translation answer — a documented fallback, not a divide by zero.

Archive rather than adopt, with a named trigger: nothing in the tree is a collidable circle, `narrow_phase` throws for them, and the header says outright that this is the deliberate choice because silently missing collisions is the worse failure. Recording the recipe means the header's promise has a specific implementation behind it. `project()` (`:91-102`) is also one line from being a support function — it computes min and max of the same dot products and discards which index produced each — so if contact *positions* are ever wanted (impact sparks, paint placed where the projectile hit rather than at its centre), the supporting vertex along ±`manifold.normal` is the primitive and the loop already does the arithmetic. Reject the book's hill-climbing acceleration over a vertex-adjacency structure outright: a pointer-linked graph fighting T11, meaningless at n ≤ 4.

### The engine cannot answer *where* a segment hit — and the book's ray-AABB listing has a sign error — archive

A box is the intersection of slabs; substituting `R(t) = P + t·d` into each keeps the farthest entry `tmin` and nearest exit `tmax`, exiting as soon as `tmin > tmax`, with `tmin` the entry parameter on success (5.3.3, pp.179-183). 5.3.8 (pp.198-201) generalises it to any convex body as an intersection of half-planes, giving the exact overlap interval rather than a boolean. Three details carry the robustness: the parallel branch must reject by containment rather than divide (0/0 gives NaN, and NaN defeats every subsequent comparison silently); the query type — line, ray or segment — is one interval initialiser; and the direction is left unnormalised so `t` comes out on [0,1].

No ray, line or half-plane primitive exists: `engine/math/shape_type.h` lists five shapes plus none, and `Segment` is not a `Shape`. The interval-clip skeleton is already in the file, though — `intersect_moving_AABB_AABB` is `tfirst`/`tlast` with per-axis raise and lower and the `if (tfirst > tlast) return 0` early out.

Genuinely useful the first time anything wants a hitscan weapon, a line of sight, an aim line or a debug pick — none of which exist (`WeaponSniper` spawns a projectile like every other weapon). **Two notes for whoever writes it.** The printed listing on p.181 reads `if (t2 > tmax) tmax = t2;` and must be `<` — `tmax` is the nearest exit, and with the printed form the early out never fires on the far plane and the routine manufactures hits. This engine ports Ericson verbatim, including a still-commented 3D test with the book's ellipsis in it, so it *will* inherit the erratum. And guard the parallel case with a tolerance on the denominator, not `denom == 0.0f`; the book itself is sloppier here than its own advice one section later.

Reject the justification some candidates offered for building it now — the 1500-tile paint scan is a game-side indexing problem wearing a geometry costume, and `test_segment_AABB`'s 2D reduction is already correct in the tree, so the segment-vs-box *boolean* is not the gap. The `t` value is.

### Lift one allocation-free SAT core into `engine/math` — adapt

Section 5.2 (pp.156-158) observes that a boolean test is strictly less general than a distance computation and therefore much cheaper, and gives SAT as the general tool. 9.1 (pp.383-385) is the chapter's account of the O(n²) boundary method — vertex containment both ways, then edge crossings — and exists precisely to be beaten.

`engine/math/matt_math.cpp` implements the convex-vs-convex question eight separate ways, all by containment passes plus edge loops over `std::vector`: `rectangle_triangle_intersect` (`:172-208`), `rectangle_quad_intersect` (`:210-224`), `triangles_intersect` (`:360-386`), `triangle_quad_intersect` (`:388-403`), `quads_intersect` (`:471-490`), `quad_rectangle_rotated_intersect` (`:524-540`), `rectangles_rotated_intersect` (`:598-640`), `segment_rectangle_rotated_intersect` (`:562-588`). One `bool polygons_overlap(...)` over a fixed-size stack polygon replaces all eight, allocates nothing, and is provably complete in 2D. It would delete `Quad::triangles()`, the triangulation helpers, and roughly a dozen hand-written pair functions.

The layering constraint is real: ARCHITECTURE has `math` depending on nothing and `collision` depending on `math`, so the `Polygon`/`project`/`test_axis` machinery in `narrow_phase.cpp`'s anonymous namespace has to move *down* into `engine/math`, with `narrow_phase` becoming its caller. That is the right direction of travel anyway. The honest counterweight: this is a wide change to a heavily-pinned public surface (`tests/math/math_tests.cpp` is 1013 lines of predicate assertions) for code the game barely runs, and at least one pinned answer — the exclusive segment-segment contract — would have to be settled first. File it against the deferred value-semantics pass, not as its own task. The argument is T11 and T4, not measured throughput.

---

## Shapes and bounding volumes

The engine got the bounding-volume ladder rung right, and right for the reason the book gives. `mattmath::RectangleF` is exactly Ericson's min-width AABB struct (4.2.1, pp.79-80) — `{Point min; float d[2];}` — the representation his §4.2 says is correct for a world where everything translates and nothing rotates, and everything in this game translates and nothing rotates. `RectangleF::bounding_box()` returns `*this`, so for the shape that is 99% of the content the bound *is* the geometry. `Triangle`/`Quad`/`RectangleRotated::bounding_box()` are the textbook extremal-point sweep over 3-4 vertices, which the book says is optimal at that vertex count and explicitly says convex-hull preprocessing cannot improve. `RectangleF::union_of` (`matt_math.cpp:1027-1034`) is literally `AABBEnclosingAABBs` (6.5.1, p.267).

What the engine does not do is treat a bounding volume as *a thing you compute once and reuse*, and it does not enforce the convexity that its whole collision path assumes.

### `Quad::is_valid()` must test convexity — and fixing it is a prerequisite for a change already in the review backlog — adopt

For a quadrilateral ABCD, convexity is exactly equivalent to the two diagonals AC and BD crossing on their interiors (3.7.1, pp.59-62 — `IsConvexQuad`, with Figure 3.18's adversarial corpus). In 2D each `Cross` collapses to the scalar perp-dot, so it is one interior-only segment test, or equivalently four same-sign signed areas around the boundary. Four multiply-subtract pairs, no branches, no allocation, and it establishes the winding as a side effect. Ericson's judgement about *where* it belongs is separate and equally useful (3.7.1, p.59): "all faces should be verified as convex, either at tool time or during runtime (perhaps in a debug build)" — convexity is a property of authored data, checked once when the data enters.

`Quad::is_valid()` (`matt_math.cpp:2886-2903`) allocates a `std::vector<Segment>` via `edges()` and runs six pairwise segment tests, checking only that the quad is **simple**. Four of those six pairs are adjacent edges sharing an endpoint, and they report "no intersection" only because `test_2D_segment_segment` uses strict `a1 * a2 < 0.0f`, which makes a shared endpoint a zero product. So it rejects bowties and accepts **darts** — and SAT (`narrow_phase`) and the fixed-diagonal fan (`Quad::triangles()`, `:3018`) are both decision procedures only for convex shapes. A dart passes all five constructors and all four setters and yields a confident, silently wrong manifold with a penetration depth a resolver will act on.

**And it is scheduled to detonate.** `docs/review/round-2/all-findings.md:5261-5326` already carries a remedy that closes `segments_intersect`'s open boundary with the contract "collinear overlap intersects; touching intersects". The moment that lands, every `Quad` constructor in the tree throws. The convexity rewrite removes the coupling entirely, is strictly stronger, is one test instead of six, and deletes a heap allocation from every `Quad` construction and mutation.

*Philosophy:* T2 (the constructor enforces a weaker invariant than its only consumers require), T11 and T8 (a `std::vector` allocation inside a value type's constructor and every setter), T4 ("the diagonals cross" is a shorter sentence than "no two of six edge pairs meet"). Fights nothing. The contract obligation the project already owns applies: the header must state what coincident or collinear vertices answer, and Figure 3.18 supplies the exact test inputs.

*Scope caveat:* `Quad` and `RectangleRotated` are never constructed outside `engine/math` and tests, so this is an **engine-API correctness fix, not a frame-time fix**. Follow Ericson's placement advice too: translation cannot destroy convexity, so `offset()`/`set_position` — the per-frame calls — need no check at all; only the vertex setters do, and those are authored-data paths. Validate in the constructor unconditionally, assert-only in the mutators. That is the T5/T8 resolution that keeps T2 intact.

The same measurement applies to `RectangleRotated::is_valid()` (`:3525-3540`): `edges_valid` (`:3601-3632`) allocates a `std::vector<Segment>` and then does eight `normalized()` calls plus four `Segment::length()` calls — **twelve square roots and one heap allocation per validation**, on every mutation.

### `Triangle` has no validity check, and a collinear triangle produces a phantom manifold — adapt

Three points are collinear exactly when their signed area is zero (3.5, p.54); in 2D that is one `signed_2D_tri_area` call against a tolerance, no cross product and no square root. The book states noncollinearity as a precondition three separate times in the same slice (3.4 p.46, 3.5 p.54, 3.6 p.55).

`mattmath::Triangle` (`matt_math.h:770-821`) has four constructors and no validation at all, unlike `Quad` and `RectangleRotated` which both throw. `narrow_phase` guards only `polygon.count < 3` (`:168`) and the all-edges-degenerate case (`:212`). A **collinear** triangle whose three edges are each individually nonzero passes both: its edge perpendiculars normalise to real unit axes, it projects to a zero-width interval on its own edge normals, and the function returns a `Manifold` with a nonzero normal and a positive penetration for a shape with no interior. `narrow_phase.h`'s own stated rule is that silently missing collisions is the worse failure — silently *inventing* one is the same failure.

The tolerance must be relative, because an area scales with the square of the input scale, which is the one thing none of the engine's five current tolerance sites agrees on; the precedent to copy is `RectangleRotated::edges_valid`'s `EPSILON * std::max(1.0f, length)`. Effort is M rather than S for a reason the raw candidates missed: `TriangleRightAxisAligned` derives from `Triangle` and `level_object_builder.cpp:147-153` constructs one straight from JSON floats, so a throwing constructor means deciding what happens to a malformed level file. Failing loudly at load is the right answer, but it is a decision this item owes.

### `Player::bounds()` reports a box 37.7× the player, on the object that queries most — adopt

A bound is measured from the geometry, never guessed. Ericson's loose-AABB strategy (4.2.3, pp.82-83) computes the radius as the distance to the farthest vertex; 4.4.3 (p.107) argues that the quality of a bound matters at all; 4.2.6 (pp.86-87) removes the reason to be loose in the first place; 6.1 (pp.235-236) states that each node should be of minimal volume because pruning power is proportional to tightness; and 8.1 (p.350) attaches the failure mode to the O(log n) claim in the same breath — "degenerate situations may cause all n objects to be tested — for example, if the query object is very large."

`game/objects/player.cpp:864-871` copies `rectangle_` and calls `inflate(Vector2F(200.0f, 200.0f))` "to take in the weapon, which is drawn from `Player::draw` but is not inside the player's own rectangle" — a *drawing* reason. `RectangleF::inflate` subtracts the amount from the origin and adds twice the amount to each extent, so 52×120 (`player_consts.h:20`) becomes **452×520 — 235,040 square units against 6,240, a factor of 37.7.**

Three things make this worth doing now rather than later. It is one of the very few items in this document with a *live* per-frame payoff: `bounds()` is what the per-view draw cull reads today (`level.cpp:443, 451, 459`, three loops per view, four views). It is what `GameObject::bounds()` (`engine/core/game_object.h:57-75`) declares a broad phase will index, and what C1 Scene's "objects expose bounds; the scene culls" design depends on — a scene can only be as good as the extents it is given. And the direction matters: it errs *outward*, which is the safe direction (13.4.2, p.530 — a coarse volume must be rounded outward so it stays conservative), so the dangerous change is a future *tightening* that silently drops contacts with nothing to catch it. The precondition — `bounds()` must contain `shape()` — belongs in `game_object.h` next to the sentence that already invites a broad phase to index it.

The fix is the union of the player's rectangle with the weapon's drawn rectangle, and every input exists: `Weapon::draw_pos()` (`weapon.cpp:123-137`), `Weapon::rotation()`, `details().size` (max 100×50, `weapon_consts.h:92`). The true union is on the order of 270×160. `RectangleF::union_of` already exists and `UiContainer::bounds()` (`engine/ui/widget.cpp:96-121`) is the in-tree precedent, empty-child guard included.

### `Shape::inflate` is three different operations under one name — adopt

Inflating a convex shape by `r` is the Minkowski sum with a disc (3.11, pp.70-71, Figure 3.26; 4.5, pp.113-114 — the sphere-swept volume `R = { x : dist(x, inner) <= r }`). Every edge moves outward by exactly `r` along its own normal, and vertices move along their angle bisectors by `r/sin(θ/2)` — a sharp vertex moves much further than `r`.

The score in the tree is three right, two wrong. `RectangleF::inflate` (`matt_math.cpp:860-877`) offsets each face outward by `amount` — the Minkowski sum with a box, conservative and correct. `Circle::inflate` (`:2401-2404`) adds to the radius — correct for a disc. `Triangle::inflate` (`:2516-2531`) and `Quad::inflate` (`:2867-2884`) push each vertex `amount` along the centroid-to-vertex direction, which displaces the edge's supporting line by only `amount · (u·n)` — strictly less than `amount` for any non-equilateral shape, and badly less for an obtuse corner: a 45° ramp corner gets its edges moved out by roughly 0.4×`amount`. Both bodies also normalise `(points[i] - center)` and then multiply back by its length, reconstructing the point they already had, and `Vector2F::normalize()` zeroes a zero-length vector, so a vertex on the centroid collapses *inward*.

Neither override is ever called — the only `inflate` call sites are `player.cpp:869` on a `RectangleF`, `inflate_to_size` in `projectile_mist.cpp:37` / `projectile_spray.cpp:37` (also `RectangleF`), and two `RectangleRotated::inflate` calls in tests. So deletion is free and repair is optional. What is not optional is the header sentence, because `RectangleF::inflate` is the one overload with a live caller: state that `inflate` produces the polygonal outer offset — every boundary moved out by `amount` along its own normal, corner rounding ignored per T3 — and that the result must always contain the input. If a correct polygon `inflate` is ever wanted, the bisector form is what to write.

### Derive `RectangleRotated`'s AABB from centre/axes/extents; delete the vector cache and the redundant `y_axis_` — adapt

The AABB wrapping an oriented box needs no corners (4.2.6, pp.86-87, Arvo90's `UpdateAABB`): each new half-extent is a sum of `|m[i][j]|·r[j]`, which in 2D with unit axes `u`, `v` and half-extents `(rx, ry)` is `hx = |u.x|·rx + |v.x|·ry`, `hy = |u.y|·rx + |v.y|·ry` — four abs, four multiplies, two adds, no branches, **no matrix type**. Ericson's memory economy (4.4, p.101 — store two of three 3D axes and cross-product the third) is sharper in 2D, where the second axis is `perp(x_axis) = (-x.y, x.x)`: one negation and a swap, so storing it is pure redundancy. And 4.2.2 (pp.81-82) endorses caching a transformed bound for the duration of a step while naming the cost honestly — but caching trades storage for recomputation, never storage *plus an allocation*.

`matt_math.h:1040-1047` stores `center_`, `x_axis_`, `y_axis_`, `hw_extents_` — 8 floats where 6 suffice — plus `std::vector<Point2F> points_`, a heap-allocated 4-point cache **inside a value type**, reassigned in both constructors, in `offset()` (`:3327`), in `inflate()` (`:3360`) and in every setter. A geometry value that heap-allocates and frees every time it moves cannot be copied cheaply, stored contiguously, or passed by value — the three things T11 asks of a shape. The type costs roughly 88 bytes and a `malloc` where ~24 bytes of pure value would do. Replace `points_` with `Point2F points_[4]`; `is_valid()` already fixes the count at four.

Deriving `y_axis_` additionally makes `axes_valid()` half redundant and `edges_valid()`'s orthogonality check vacuous by construction — T5, an invariant made unrepresentable rather than runtime-validated. Mild T4 tension on the `|dot|` identity, since the corner min/max is more obviously correct at a glance; it needs a comment and a pinning test against the corner form at several angles. Entirely latent (nothing in `game/` is a `RectangleRotated`), so this is already-filed technical debt on a per-frame-eligible type, not reclaimed frame time.

### Rewrite `rectangles_rotated_intersect` as the four-axis 2D SAT — adapt

Two OBBs are separated iff for some axis L, `|dot(T,L)| > rA + rB` — a fixed, branch-light sequence with an early out, needing no vertex enumeration, no edge-pair enumeration and no containment special case, because a contained box still fails to be separated on every axis (4.4.1, pp.102-105). In 2D exactly four tests survive.

`matt_math.cpp:598-640` instead pretests with `a.intersects(b.bounding_box())` — itself a full rotated-rect-vs-rectangle test, so the pretest is not cheap either — then runs 8 `contains()` calls, each routing through `point_rectangle_rotated_intersect` → `quad()` → `calculate_points()` (a heap vector) → `Quad(points)` → `is_valid()` → `edges()` (another heap vector) → six segment-segment tests. It then builds two more `std::vector<Segment>` and runs 16 more crossing tests. Roughly **18 heap allocations and 60+ segment-segment predicates** for a query that four axis tests and ~12 dot products decide exactly. It also replaces a two-case test — containment OR edge crossing — whose union is correct only by inspection, with one algorithm that has a single correctness argument, and it is the natural donor for an OBB projection helper `narrow_phase` can share.

Ranked at 2 purely because nothing calls it.

### Test point-in-convex-polygon as "inside every edge half-plane" — adopt

A convex polygon is the intersection of its edge halfplanes (3.7, pp.58-59, Figure 3.16; 3.6, pp.54-55 for the constant-normal form), so a point is inside iff it is on the inside of every edge. Ericson puts the same conjunction first in the boundary algorithm (9.1, p.385, step 1) because it costs one signed distance per face and is most likely to terminate the query early. Four sign tests for a quad; no allocation, no division.

`quad_point_intersect` (`matt_math.cpp:509-521`) instead builds a `std::vector<Triangle>` via `Quad::triangles()` and runs up to two full barycentric solves with two divisions each. `point_rectangle_rotated_intersect` (`:590-596`) builds a whole `Quad` first — itself two heap allocations and six segment tests — and then does the same. A free `bool point_in_convex_polygon(std::span<const Point2F>, const Point2F&)` in `engine/math` gives one definition of *inside* for `Quad`, `RectangleRotated` and `Triangle`, and unlike the triangle-fan it does not mis-answer for the concave case `Quad::is_valid` currently lets through.

Name the tension: the inside-sign must be derived from the polygon's own signed area rather than from `Quad`'s documented "clockwise from top left" winding, because nothing enforces that comment. **Reject** the scope creep two candidates proposed — adding a `struct Line { Vector2F n; float d; }` value type. Under the project's own rules that owes a documented contract, a degenerate answer for `Line(a,a)`, and tests, for a type with two callers neither of which the game exercises. Leave it until something wants signed distances rather than signs.

### Say that `Shape::center()` is the vertex mean, and index any future broad phase on `bounds().center()` — adapt

Ericson warns explicitly (4.3.2, p.89) that using the geometric centre of the points instead of the bounding-box midpoint "can give extremely bad bounds for nonuniformly distributed points" — up to twice the needed radius — because the mean is dragged toward wherever the vertices cluster. The 2D case is easier to hit: a right axis-aligned triangle has two vertices sharing the right-angle corner's coordinates, so its vertex mean sits at one third of the bounding box in each direction rather than at the midpoint.

`Triangle::center()` (`matt_math.cpp:2643-2646`) returns `(p0+p1+p2)/3` and `Quad::center()` (`:3081-3084`) the mean of four corners; `matt_math.h:87` declares `virtual Point2F center() const` with no comment saying which centre it is. `CollisionTools::calculate_containing_collision_direction` (`collision_tools.cpp:102-118`), `shape_shape_collision_direction` (`:120-127`) and the four two-edge tie-breaks at `:185-197` all decide which way to push by comparing centres — and `TriangleRightAxisAligned` ramps are the only live non-rectangle collidable, so the biased centre sits precisely on the one path that reads it.

Do **not** fix the resolver in place; that machinery is superseded by the manifold and re-fixing it is on the rejected list. The live payload is three lines of rule for code being written next: `matt_math.h` says which centre it is; a broad phase keys off `bounds().center()` and never `shape()->center()`, or objects get bucketed off-centre by up to a third of their extent; and it is a second, independent reason the SAT rewrite was right, because `narrow_phase` compares no centres at all.

### Give `union_of` an empty answer, and give it a test — adapt

The AABB merge (6.5.1, p.267) has no concept of an empty volume, because in a bottom-up build every node already bounds at least one primitive. Any code folding a merge over a variable-length list needs a starting value, and the only correct one is the inverted-empty box (min +∞, max −∞), for which merge is the identity.

The engine has already been bitten by this and wrote the lesson down in the wrong place. `UiContainer::bounds()` (`engine/ui/widget.cpp:96-121`) carries a comment recording that `RectangleF::ZERO` is a real point at the world origin, so an empty nested container dragged its parent's box back to (0,0) — a parent of one label at (900,400) reporting a box from the origin. The fix was a `bool any` flag plus skipping degenerate children **in the caller**, so every future caller must rediscover it, and any bottom-up fold in a future index hits the same trap. `union_of` also has no test at all. A `RectangleF::EMPTY` (or an `empty()` factory) that `union_of` treats as the identity fixes the operation and deletes the `any` dance.

### The SSV pattern is the design target for the deferred value-semantics pass — adapt

Every sphere-swept-volume overlap test is the same shape (4.5.1, pp.114-115): compute the distance between the inner structures, compare against the sum of the radii, squared to avoid the root — and Ericson stresses the distance computation does not require the inner structures to be of the same type, so the family closes under mixing for free. N shape types over a shared distance kernel costs N distance functions, not N² intersection functions. In 2D the kernel set is tiny: point-point, point-segment, segment-segment, point-convex-polygon.

`Shape` (`matt_math.h:69-90`) declares seven pure virtual `intersects` overloads, one per concrete type, and `matt_math.cpp` implements 27 free `x_y_intersect` functions to fill the matrix; `Shape::intersects(const Shape*)` dispatches by `switch(other->shape_type())` plus a `dynamic_cast` per call, with `default: throw` that `ShapeType::none` falls into. Adding one shape type means an eighth pure virtual on all six existing types plus a new row and column. The commented-out `//RectangleF(const mattmath::Segment& center_line, float thickness);` at `matt_math.h:203` shows somebody has already wanted a stadium.

This is a **design note governing the deferred pass, not a feature to build.** The narrow phase already found this answer for polygons independently — one `Polygon` value and one SAT rather than a bespoke routine per polygon pair — and SSV is the same move generalised to the shapes that are not polygons. It explicitly does *not* justify adding `Capsule` to `ShapeType`: nothing wants to be a stadium, and doing it under the current matrix costs an eighth virtual on six types.

---

## Math primitives and predicates

Chapter 3 exposes not a missing algorithm but a missing *name*. One scalar — the 2D cross, the perp-dot, ORIENT2D, twice the signed triangle area — is the primitive from which sidedness, winding, convexity, collinearity, point-in-triangle and polygon area all fall out, and the engine has written it three times: once correctly under a triangle-shaped name that nothing else reaches for, and twice broken.

### Give MattMath one correct 2D cross product; delete the two broken encodings — adopt

`perp_dot(u,v) = u.x*v.y - u.y*v.x`. Its sign is the winding of the pair, its magnitude is the signed parallelogram area, and it is zero exactly when the two are parallel; applied to points, `ORIENT2D(a,b,c) = perp_dot(a-c, b-c)` is twice the signed area of `abc` and its sign is "which side of directed line ab does c lie on" (3.1.6.1, pp.32-33; 3.1.3, pp.27-28; 3.3.5, pp.41-43 — there is no 3D cross product in 2D, the analogue is the scalar). Ericson defines it once as `Cross2D` and builds sidedness, winding, area and point-in-triangle out of it (5.4.2, p.205).

- `Vector2F::cross` (`matt_math.cpp:1359-1362`) returns `Vector2F(x * other.y, y * other.x)` — the two products of the determinant, **never subtracted**. Not the cross product, not the perp-dot, not the component-wise product, not any quantity with a geometric meaning. The return type is plausible, so a caller reaching for it gets nonsense with no compile error.
- `mattmath::sign` (`matt_math.cpp:65-70`, declared `matt_math.h:57`) computes the correct expression and then wraps it in `static_cast<int>`, so any triple whose doubled signed area lies in (−1, 1) reports 0 — *collinear* — which is precisely the input a sidedness predicate exists to arbitrate. The name lies too: it returns a truncated area, not a sign.
- `signed_2D_tri_area` (`ericson_math.cpp:358-362`) is the correct one, buried under a triangle-shaped name whose comment says only "Returns 2 times the signed triangle area", so nothing else reaches for it.

Both broken ones have zero callers anywhere in the repository, which makes this two deletions. Add `static float Vector2F::cross(const Vector2F&, const Vector2F&)` beside `dot()`, express `signed_2D_tri_area` as `cross(a-c, b-c)`, and say in `ericson_math.h` that this is ORIENT2D and that its sign is a sidedness test. Keep the name `cross` rather than introducing `perp_dot` — the book itself calls the 2D scalar the pseudo cross product and derives it from the 3D cross — but the header **must** say it returns a scalar, or the next reader assumes the old broken signature. Four other findings in this document become available the moment it has a name.

### Delete `Vector2F::rotate_vector_by_ref` — it reads its own half-written output — adopt

Every entry of a matrix product is formed from the *original* entries of both operands (3.1.1, pp.25-26); Ericson's own `TransformPoint` (13.6, pp.537-538) computes every component into locals before storing precisely so input and output may alias.

`matt_math.cpp:1449-1456` reads:

```
vec.x = vec.x * cos_angle - vec.y * sin_angle;
vec.y = vec.x * sin_angle + vec.y * cos_angle;
```

The second line reads the `x` it just overwrote. Zero callers. The tree contains one correct pattern twice — `Vector2F::rotate` (`:1406-1416`, two temporaries) and the static `rotate_vector` (`:1441-1448`) — and one broken copy, all three public on the same value type for one concept, and the broken one has the longest name and is therefore the most likely to be picked by someone skimming the header. Delete it; `rotate()` already is the by-reference version. This does not resurrect the matrix subsystem; it is one broken 2×2 product on a value type.

### Rewrite `test_point_triangle` as three signed-area sign tests, which makes `barycentric` dead — adopt

Containment needs only the *signs* of the three sub-triangle areas relative to the sign of the whole, so the division by the total area drops out completely (3.4, pp.49-52 — Figure 3.10's seven regions are decided by the signs of u, v, w alone; 5.4.2, pp.203-206 gives the 2D `Cross2D` form and the `SameSign` variant for unknown winding). Six multiplies, three subtracts, no division, winding-independent, and it cannot produce a NaN. The book's projection-plane-selection step vanishes in 2D.

`barycentric` (`ericson_math.cpp:399-413`) divides by `denom = d00*d11 - d01*d01` with no guard. For a collinear or coincident-vertex triangle that is zero, `v` and `w` come back inf or NaN, and `test_point_triangle`'s `v >= 0.0f && w >= 0.0f` (`:416-422`) then returns false through NaN comparisons — safe by luck, not by design, and the failure mode is a ramp that silently stops being solid. The sign form needs no validity check at all: a degenerate triangle simply contains nothing.

`triangle_point_intersect` (`matt_math.cpp:426-430`) is called four times per player-vs-ramp test from `rectangle_triangle_intersect` — the one non-rect-rect pair the game actually runs. **Use the `SameSign` form, not a CCW-only rewrite:** nothing in the tree constrains triangle winding (`level_object_builder.cpp:147-153` reads six floats out of JSON in author order), and the current barycentric implementation is accidentally winding-agnostic, so a CCW-only version would be a regression.

Settle the competing claims while here. Two candidates asserted `barycentric` must stay "because `closest_pt_point_triangle` needs the weights". That is false — `closest_pt_point_triangle` (`:46-90`) computes its own `va`, `vb`, `vc` inline and never calls it. `barycentric`'s only caller is `test_point_triangle`, whose only caller is `triangle_point_intersect`. Taking the sign form leaves `barycentric` with zero callers, and the honest move is deletion along with its unguarded division. That also settles the proposal to hoist `d00`/`d01`/`d11` per triangle: there is nothing left to hoist.

### Replace `closest_pt_point_segment` with the book's deferred-divide form — adopt

The book gives `ClosestPtPointSegment` twice. The p.128 form computes `t = dot(c-a, ab) / dot(ab, ab)` and then clamps; the p.129 form defers the divide — compute the unnormalised numerator, return `a` with `t=0` if it is ≤ 0, compute `denom`, return `b` with `t=1` if the numerator ≥ `denom`, and only divide in the interior case where `denom` is provably positive (5.1.2, pp.128-129). The book sells it as saving a division; the consequence that matters is that the division is only ever *reached* when it is safe.

`ericson_math.cpp:426-437` is the p.128 form verbatim — line 431 divides unconditionally. For `a == b` both dot products are zero, `t = 0.0f/0.0f = NaN`, and neither `if (t < 0.0f)` nor `if (t > 1.0f)` fires for NaN, so `d` comes back `(NaN, NaN)`. `circle_segment_intersect` (`matt_math.cpp:312-319`) then evaluates `NaN <= radius`, which is false: a silently missed intersection. The p.129 body returns `a` with `t=0` for that input and is cheaper in both clamped cases. Reachable through `circle_rectangle_rotated_intersect` (`:332-354`), which feeds it edges pulled from `RectangleRotated::edges()`, and through `Circle::intersects(Segment)` (`:2437`).

### Guard the divisions the book states preconditions for and never repeats — adopt

Every closest-point and projection formula in chapter 3 divides by a quantity the book declares nonzero in a sentence it does not repeat at the call site: the length (3.3.1, p.38 — "a **nonzero** vector v can be made unit"), `u·u`, `|u||v|`, or the coefficient determinant (3.1.4, pp.29-31 — Cramer has a unique solution iff the determinant is nonzero). 11.5.2 (pp.457-458) adds the operational rule: hoist divisions out of decisions, and `denom == 0` is its own case that must be answered.

The engine already learned this lesson once and wrote it down. `Vector2F::normalized()` (`matt_math.cpp:1363-1371`) returns zero for zero input, with a comment saying that dividing by the length "produced NaN, which then propagated silently through velocities and shape validation", and `narrow_phase.cpp:188-192` depends on that contract. The discipline was never carried into the ported functions:

| Site | Divides by | Consequence |
|---|---|---|
| `ericson_math.cpp:431` `closest_pt_point_segment` | `dot(ab, ab)` | NaN out-params; clamps at `:433-434` do not catch NaN |
| `ericson_math.cpp:409-411` `barycentric` | Gram determinant | inf/NaN weights on a degenerate triangle |
| `matt_math.cpp:1457-1460` `angle_between` | `a.length() * b.length()` | NaN, then `acos` with no domain clamp |

Do these **after** the two items above, not before: `barycentric` and `angle_between` are both removed for free by other findings, so guarding them is guarding code you are about to delete. `closest_pt_point_segment` is the surviving standalone work and it is real, reachable and worth doing on its own. Ericson's own trick makes it branch-free: `if (!(t > 0.0f)) t = 0.0f;` sends NaN to the `a` endpoint, a defined answer, with no branch added.

### Delete the `acos` chain: `angle_between` → `Triangle::angle_*` → `find_hypotenuse` → `hypotenuse()` — adapt

Perpendicularity is the sign of an inner product, not an angle comparison (3.3.3, pp.39-40 and Figure 3.4 — the dot product is positive for acute, negative for obtuse, zero for perpendicular, "an extremely useful property... frequently used in various geometric tests"). The `acos` form costs two square roots, a division and a transcendental, and the quotient can drift outside [−1,1] in float and return NaN.

`find_hypotenuse` (`matt_math.cpp:2719-2738`) spends three `acos` and six `sqrt` answering "which corner is the right angle", a question that is three dot products — and because `angle_between` has no domain clamp, `are_equal(NaN, PI_OVER_2)` is false for all three, `find_hypotenuse` returns −1, and `hypotenuse()` throws **"Triangle is not a right triangle" on a triangle that is one**. The correct house pattern already exists twice in the same file, at `axes_valid` (`:3584-3599`) and `edges_valid` (`:3601-3632`), both of which test a dot against zero and never call `acos`.

The decisive fact is that `hypotenuse()` and `hypotenuse_gradient()` have **zero callers anywhere, including inside `matt_math.cpp` itself** — the entire `TriangleRightAxisAligned` public surface beyond its constructors is dead, and the game's ramp handling never touches it. So the correct action is deletion of the chain, leaving only `RectangleRotated::rotation()` (`:3523`), which is either guarded or rewritten as `atan2` of the x axis and needs no `acos` at all.

### Compare squared distances against squared radii — adopt

Every circle test in chapter 5 ends the same way: form `v`, return `dot(v,v) <= r*r`. Squaring is order-preserving on non-negative quantities, so the root is pure waste (5.2.5, p.165 — "both distance and radius can be squared before the comparison is made without changing the result of the test"; repeated 5.2.7 p.168 and 5.3.2 p.179, where the boolean `TestRaySphere` never reaches a `sqrt` at all).

`circles_intersect` (`matt_math.cpp:279-283`) and `circle_point_intersect` (`:327-330`) both use `Vector2F::distance`; `circle_segment_intersect` (`:312-319`) uses it after `closest_pt_point_segment`, and `circle_rectangle_rotated_intersect` calls that once per edge — four roots per query. Meanwhile `test_circle_circle` (`ericson_math.cpp:36-44`) is the correct squared form and has **zero callers anywhere, including the tests**. Two implementations of one predicate, the public one slower and rounding differently at the grazing boundary — which is a correctness hazard, not just duplication, because two functions answering the same question can disagree.

### Spend `closest_pt_point_OBB` — circle-vs-oriented-box is two clamps and a squared compare — adopt

`TestSphereOBB` is `TestSphereAABB` with `ClosestPtPointAABB` swapped for `ClosestPtPointOBB`: clamp the centre's projection onto each box axis to that axis's half-extent, compare the squared distance against the squared radius (5.2.6, pp.166-167). Four lines. The better form that never materialises the closest point is also given (5.1.4.1, pp.133-134).

`closest_pt_point_OBB` (`ericson_math.cpp:439-457`) already implements the clamping form correctly with `for (int i = 0; i < 2; i++)`, is tested at `math_tests.cpp:146-210`, and has **no production caller**. Meanwhile `circle_rectangle_rotated_intersect` next door does a bounding-box reject, a `contains()` test, then `rect_rotated.edges()` returning a `std::vector<Segment>` by value, then up to four `circle_segment_intersect` calls each taking a `sqrt`. The replacement is two lines. The book's precondition — orthonormal axes — is genuinely enforced here: `RectangleRotated`'s constructors normalise (`matt_math.cpp:3232-3233`) and `axes_valid()` re-checks unit length and perpendicularity. Cheapest item in this document: no new primitive, no new test infrastructure, one function body.

### Add an n-gon signed area (2D Newell collapses to the shoelace sum) — adopt

Deriving a polygon's normal from one pair of edges is not robust — near-collinear edges cancel — so Ericson sums over all edges (12.4.2, pp.491-495, Newell's method). In 2D the x and y components vanish identically and only the z term survives, which is exactly twice the shoelace signed area: `signed_area(std::span<const Point2F>)` gives area, winding and degeneracy in one pass, five lines, span in, float out, no allocation. Ericson also notes the quad case reduces to the cross product of the diagonals, so `2 · area(ABCD) = perp_dot(C − A, B − D)` — cheaper than the triangle case, and using all four vertices.

This is the primitive the convexity check, the winding enforcement and any future polygon collider all sit on. The diagonal identity is worth adopting alongside the `Quad::is_valid()` convexity clause: between them they replace the six segment-intersection tests and the `std::vector<Segment>`. The free rider Ericson supplies at the same time — "largest signed area is the outer boundary; enforce winding by reversing the vertex list" — is one line, and it is the honest answer to `Quad` demanding clockwise-from-top-left ordering by documentation only.

### Compare the signs of two determinants; do not multiply them — adapt

These determinants are consulted only for their sign (3.1.6, p.32 — "the sign of a determinant plays a special role... often used as topological predicates"). Multiplying two signed areas to test whether they differ squares the dynamic range for no benefit; `(a1 < 0.0f) != (a2 < 0.0f)` is the same test, exact for every finite input, one multiply cheaper. Ericson recommends the sign-bit form explicitly for integers (5.1.9.1, pp.152-153).

`ericson_math.cpp:375` and `:382` use bare `a1 * a2 < 0.0f` and `a3 * a4 < 0.0f` inside `test_2D_segment_segment`. Level coordinates run to a few thousand units, so each area is order 1e6 and the product order 1e12 — not an overflow, but mantissa bits spent on a comparison that never needed them, and at the other end two genuinely tiny opposite-signed areas can underflow the product to exactly `0.0f` and be read as "not crossing". The sign convention must be chosen deliberately at the same time, because it also decides whether an exactly-zero area counts as left or right. This is coupled to the review backlog's plan to close the `segments_intersect` boundary — whichever way that lands, these two comparisons are where it lands, so do them as one change.

### `test_2D_segment_segment` is a proper-intersection test by design: say so, and delete the `a == b` patch — adapt

The book settles what looks like an open question. After the listing, Ericson states (5.1.9.1, pp.152-153) that with collinear points either signed area may be zero, and gives the explicit guard a *proper*-intersection test needs. So returning false for touching endpoints and collinear overlap is the algorithm working as designed, not a bug — and it happens to agree with `manifold.h`'s "shapes that merely touch do not overlap", so the engine's policy is consistent.

The actual defect is that `mattmath` exposes an exclusive predicate in a library where every other predicate is closed, states no contract, and then patches one instance of the excluded case with exact float equality: `segments_intersect` (`matt_math.cpp:542-553`) wraps it with `if (a == b) { t = 0.0f; p = a.point_0; return true; }`, and `Segment::operator==` is exact, so it catches bit-identical segments and not the reversed spelling. Two edits: write the exclusive contract into `ericson_math.h`, and delete the `a == b` special case. Building a collinear-overlap branch instead would fight T3 and put `engine/math` at odds with `engine/collision`'s touching rule.

The consumers that inherit the exclusion are worth listing so the contract change is scoped: `triangle_segment_intersect` (`:405-424`), `triangles_intersect` (`:360-386`), `segment_rectangle_rotated_intersect` (`:562-588`), `rectangles_rotated_intersect` (`:598-640`), and `Quad::is_valid` (`:2886`).

### `triangles_intersect` tests four of the nine edge pairs — and the ground checks disagree about whether that is a bug — adapt

Ericson states flatly that the edge-piercing method needs up to six edge-triangle tests with no shortcut below that (5.2.10, pp.172-173), and that closest points between two triangles need nine edge-edge tests (5.1.11, p.155).

`matt_math.cpp:360-386` runs a six-way containment pass and then `for (int i = 0; i < 2; i++) { if (segments_intersect(a_edges[i], b_edges[0]) || segments_intersect(a_edges[i], b_edges[1])) }` — the pairs (a0,b0), (a0,b1), (a1,b0), (a1,b1). `a_edges[2]` and `b_edges[2]` are never touched.

**Two ground checks reached opposite verdicts and this document will not paper over it.** Two independent checks concluded the truncation is *sufficient*, one by proof and one by fuzzing (400,000 and 500,000 random triangle pairs respectively, zero disagreements against the exhaustive 3×3 form). The proof: given the containment pass has already failed, every vertex of the overlap region is a crossing, its boundary alternates arcs of A and B, and an edge intersected with a convex body is connected — so at least two distinct edges of A participate, hence at least one of a0/a1; that edge enters and leaves B through two *distinct* edges of B, hence at least one of b0/b1. A third check argued the enumeration is provably incomplete and offered a "chord case" counterexample with crossings only at (a2,b0), (a2,b1), (a0,b2), (a1,b2) — but that configuration has a0 crossing only one edge of B, which a chord of a convex polygon cannot do unless an endpoint is inside B, which the containment pass excludes. **The weight of evidence is that the loop is sufficient**, and the third check itself correctly demolished the Star-of-David counterexample an earlier slice had offered.

The actionable finding is the same either way, and it is a T4 finding, not a T2 one: the argument is nowhere written, it silently depends on the containment pass running first, and it breaks the moment someone generalises the loop to polygons or reorders the function. **Do not file it as a correctness defect.** Either restore the full 3×3 form or write the invariant into a comment with a hexagram pinning test — and settle the disagreement by committing the fuzz comparison that two agents have already run. Moot if the shared SAT core lands.

### `closest_pt_point_triangle`'s seven branches are a Voronoi region walk — already-have

The Voronoi region of a feature is the set of points closer to it than to any other feature, and a triangle partitions its plane into exactly seven such regions with boundaries assigned to one side so the classification is total (3.10, pp.69-70, Figure 3.24; the 2D case is certified explicitly at 5.1.3, p.130, one of the few places the book states a routine works in both two and three dimensions). For a convex n-gon in 2D the count is 2n+1.

`ericson_math.cpp:46-90` is the full seven-region form, each early return already annotated with its barycentric coordinates. Nothing to change; one comment to add. The branchy shape is not clumsiness, it is a region classification, and someone tempted to "simplify" it into a min-over-three-edges loop would lose the interior case and not notice — because the interior case is exactly the one where every edge projection is wrong. `sq_dist_point_AABB` and `closest_pt_point_AABB` (`:116-174`) are the same idea in clamp form, and `closest_pt_point_OBB` (`:439-457`) is decompose-clamp-reconstruct over an orthonormal basis, which is the concrete reason deleting `MatrixF` cost the engine nothing on the OBB path: it inverts the box frame with two dot products against the axes rather than by solving a system. That is worth stating in the header as *why* `RectangleRotated`'s axes are an invariant orthonormal pair.

### Write the contracts `ericson_math.h` has never had, and clear out the half-mined 3D leftovers — adopt

The book demonstrates by placing `IntersectMovingSpherePlane` and `IntersectMovingSphereAABB` a page apart with opposite t-range conventions (5.5.3 p.221 vs 5.5.7 p.229) that whether a routine clamps its own output or leaves it to the caller is a **contract choice that must be stated**. 5.6 (pp.232-233) says the derivation is the deliverable, not the listing.

`ericson_math.h` declares fifteen functions and states a precondition, a degenerate answer or a checked-ness claim for **none** of them. The round-2 review closed "the engine's only pure primitive has no documented contract and no test" against `partitioner.h`, which makes a header comment stating preconditions, whether they are checked, and the degenerate answer an obligation for a public primitive. Every closest-point and test function in this header owes one.

Three cleanups ride along. `ericson_math.h:12-72` is a commented-out block of superseded scaffolding (`Vector`/`Point`/`AABB`/`Sphere`/`OBB` plus a dozen PascalCase declarations). `ericson_math.h:104-106` and `.cpp:189-225` hold `test_triangle_AABB` commented out, verbatim in 3D — `.z`, `Cross`, `Plane p`, `TestAABBPlane` — with the book's literal `...` placeholder mid-body; in 2D that test is not *needed* at all, it is SAT over three triangle normals and two box normals, which `narrow_phase` already does, and a file whose library declares the dimension a design constant should not carry a 3D test with an ellipsis in it, because the next reader cannot tell "not needed" from "not done". Replace `// 132` with a note saying which sections were mined and which were deliberately skipped. And `ericson_math.cpp:356-357` carries the book's y-up winding comment ("positive if abc is ccw") into a y-down screen library, pinned in the same wording at `math_tests.cpp:57-70` — fix both. Record while there that `narrow_phase` is immune to the winding ambiguity **by construction**, because `test_axis` derives the normal's orientation from the signed projections at `:147-158` rather than from vertex order; that is a better choice than the book's own `ComputePlane` convention, and it should be stated in `narrow_phase.h` so nobody "fixes" `polygon_from`'s corner ordering.

---

## Numerical robustness and tolerances

This is the half of the module that was never designed, only inherited. `PHILOSOPHY.md` promises "documented contracts — edge ordering, winding, zero-length behaviour, what throws" and says nothing about tolerance policy, world-coordinate bounds, or non-finite input; `grep` for float/epsilon/NaN/precision returns one unrelated line.

### The epsilon inventory, and the range each is valid over — adapt

An absolute tolerance is only meaningful once the magnitude of the quantity it applies to is known: float spacing doubles with the exponent, and machine epsilon is a *relative* bound defined at 1.0 (11.2/11.2.1, pp.429-435, Figure 11.2 and Table 11.1 — there are 1,048,575 floats between 10 and 11 and 1,023 between 10000 and 10001). The combined form is `|x-y| <= eps * max(|x|,|y|,1)` (11.3.1, pp.442-443). Ericson's wider rule (11.6 p.462, 11.7 p.463) is that a tolerance is a property of a specific quantity in specific units, and that one shared number across unrelated quantities is the same error as several formulas for one value; 8.4.3 (p.377) adds the ordering constraint — the query epsilon must exceed the build epsilon, or a primitive placed on one side within build tolerance is missed by a strict query; 12.4.1 (p.491) makes the merge tolerance strictly smaller than the on-line one.

| Constant | Where | Value | Quantity it measures |
|---|---|---|---|
| `EPSILON` | `matt_math.h:47` | 1e-4 | unstated — used for unit lengths, dot products of unit vectors, near-zero velocity components |
| (unnamed) | `matt_math.h:62`, `are_equal(Vector2F,…)` default | 1e-4 | same, but hardcoded instead of naming `EPSILON` — a CONVENTIONS violation today |
| `SEGMENT_PARALLEL_EPSILON` | `ericson_math.cpp:16`, anon ns | 1e-6 | slab-test denominator; one caller; its own comment concedes the two "were never the same quantity" |
| `EPSILON * max(1.0f, length)` | `matt_math.cpp:3630-3633` | relative | opposite-edge lengths in `RectangleRotated::edges_valid` |
| (none) | `narrow_phase.cpp:140, 189, 212` | exact 0 | separation boundary and degenerate axis |

**The arithmetic, which sharpens the point.** Shipping levels span y in [−100, 7000] and x in [−100, 6100], every value a multiple of 5. In [4096, 8192) one ULP is 2⁻¹¹ = 4.883e-4, so `5000.0f + EPSILON == 5000.0f` **exactly** — the engine's general tolerance is one fifth of a representable step at the coordinates the game runs at. Every collision test in `tests/` runs at coordinates 0-800, where the ULP is 6.1e-5 and `EPSILON` is ~1.6 ULPs. The tests exercise a range where the tolerance is meaningful; the game runs at one where it is not.

Nothing is broken today, and that should be said clearly: every live `are_equal` call site in `engine/` passes a quantity of order one — velocities (`matt_math.cpp:1303-1318`), unit-vector lengths and dots of unit vectors (`:3423-3437`, `:3587-3594`, `:3619`), a dot of unit vectors in `resolve.cpp:18`. That is exactly the domain an absolute epsilon is valid for. **The defect is that nothing says so**, so the next call site that passes a world position gets exact float equality without noticing.

The deliverable is four sentences in PHILOSOPHY's Collision section, mirrored on `matt_math.h` next to `EPSILON`: what the tolerance policy is, the maximum world coordinate the engine claims correctness at, whether non-finite input is a precondition violation or a handled case, and what a degenerate shape means. Scope it to `resolve.h`, `narrow_phase.h` and `matt_math.h`; leave `collision_tools.cpp` alone, since it is slated for deletion.

One correction to the obvious remedy: the relative helper is the *fallback*, not the pattern. `edges_valid` already solves the same problem better twelve lines earlier (`matt_math.cpp:3615-3619`) by **normalising both edge directions before dotting them**, so the compared quantity is dimensionless and an absolute epsilon is correct. Normalise-the-operands is the pattern to name.

### Two range tests let NaN through as "overlapping with everything" — adopt

Every comparison involving NaN is false, so a test written as "reject if separated" falls through to the accept branch on NaN. Ericson names it and gives the rewrite (11.2.2, pp.436-437): make the branch that *asserts* overlap be reached only when comparisons actually succeed.

`ericson_math.cpp:30-31`, `test_AABB_AABB`: `if (a_max.x < b_min.x || a_min.x > b_max.x) return 0;` twice, then `return 1;`. That routine is `rectangles_intersect` → `RectangleF::intersects` → `Shape::AABB_intersects` → the cheap filter at `contacts.cpp:39`, **and** the whole of `Level::is_object_out_of_bounds` (`level.cpp:574-579`), used at `:333` to flag deletion and at `:355` to throw "Player out of bounds". `narrow_phase.cpp:140` repeats the shape: `if (forwards <= 0.0f || backwards <= 0.0f) return AxisTest{};` falls through on NaN to `separated = false` with a NaN penetration.

So one NaN coordinate produces an object that is never culled out of bounds and generates a contact against everything in the level, dispatching a NaN normal into every `on_contact`, which propagates into positions. The alternative failure — a miss — is strictly better. The rewrite is the same instruction count. `test_point_triangle` (`:421`) is already in the safe form, twelve lines below the unsafe one, which is the neatest possible demonstration that this was never a policy.

There is a third variant no candidate caught: `project()` (`narrow_phase.cpp:94-101`) seeds min/max from `points[0]` and folds with `std::min`/`std::max`, which are not NaN-symmetric — a NaN at index 1..3 is silently *dropped* from the hull and the projection is computed as if the polygon had fewer vertices, while a NaN at index 0 poisons both. Same defect, two different behaviours depending on vertex order.

There is currently no way to notice any of this: `grep` for `isnan`/`isfinite` across `engine/`, `game/` and `tests/` returns nothing, and neither `narrow_phase_tests.cpp` nor `contacts_tests.cpp` has a single non-finite input case.

### The collision tests all run within 100 units of the origin; the game runs at 6000 — adopt

An algorithm verified near the origin is not verified far from it, because a separation of length L applied at coordinate X is a no-op whenever L falls below half a ULP at X (11.2, pp.429-431). Ericson's prescription for finding any tolerance is empirical (11.3.2, p.444): sweep the full input range and grow the tolerance until every case classifies correctly. 11.5.1 (p.456, Figure 11.11) and 11.6 (p.463) both say to perform the deciding arithmetic centred on the origin.

`narrow_phase_tests.cpp:157-175` sweeps a 20×80 rectangle against a fixed 80×20 over integral x in [−100,100] and y in [−60,60] and REQUIREs that one application of `separation()` ends the overlap. At |x| ≤ 100 a ULP is ~7.6e-6; at 6000 it is 4.88e-4, 64 times coarser, so a penetration below ~2.4e-4 cannot be resolved by translation at all — `position + separation == position` and the contact regenerates every frame forever.

**This is one loop parameter on a test that already exists, and it settles two other items on this list empirically instead of by argument.** The honest expectation is that the rect-rect sweep still passes exactly: every authored level coordinate is integral, integers up to 2²⁴ are exact in float, and for axis-aligned boxes the SAT axes are exactly (0,±1)/(±1,0) so the dot is a coordinate copy and the subtraction obeys Sterbenz. The ramp case, whose axis is (0.7071, 0.7071), is where error will show. Either result is worth pinning, and if it fails, the smallest offset that restores separation *is* the contact skin the engine needs. Re-run the pinning fixtures translated by (+6000, +5000).

### Adopt `assert` for internal invariants, with a stated rule about when to throw instead — adapt

Ericson's baseline debugging practice is liberal `assert` (2.7.1, p.21), and 11.6 (p.462) applies it to geometry specifically — assert that computed points lie on the thick plane, that a reflected velocity points away from it — with his own caveat that an assert on a float quantity needs the same tolerance discipline as the code it guards.

There is not one `assert` call in `engine/` or `game/`. `manifold.h:18-23` states two invariants and checks neither ("Unit length, pointing from the first shape towards the second"; "Always greater than zero") — both are one line at `narrow_phase.cpp:218`. `resolve.h` documents that `separation_along`'s axis must be unit length and never checks it; `narrow_phase.cpp:114-115` carries a comment saying the axis must be unit or the comparison is meaningless, and never checks it. The only precondition machinery in the tree is a local `require` lambda in `ApplicationOptions::validate` that throws.

The engine's loudness is uniformly exception-shaped, which is defensible for contracts crossing the API boundary but *cannot* be used on the per-frame path — a throw cannot be compiled out, so a per-pair invariant check would be a permanent T8 tax and therefore simply is not written. That leaves the invariants unchecked in both builds. Adopting `assert` needs one stated rule, not a habit: **throw for broken contracts crossing the public API, assert for internal invariants on the frame path.** T12-clean — `<cassert>`, not a macro inventing syntax.

### Write down the boundary convention: a closed filter feeding an open decider — adopt

Ericson's overlap tests are deliberately non-strict at the boundary (4.3.1 p.88, `TestSphereSphere` uses `<=`; 4.2.1 pp.79-80, `TestAABBAABB` rejects only on strict separation) — exactly-touching volumes report as intersecting, which is the conservative direction for a *filter*: it can produce a false positive but never a false negative. He keeps the inexact 3D face-only test on the same ground (4.4.1 p.102, p.106 — "conservative in that it never fails to detect a collision"; Bergen97's 6-7% false positives are acceptable because the exact test cleans them up), and 11.3.3 (pp.446-447) states the composition rule: a cheap pre-test and an exact test must not disagree at the boundary.

The engine currently gives four different unstated answers to "what does exactly touching mean", and only one is written down:

| Predicate | Boundary | Documented? |
|---|---|---|
| `test_AABB_AABB` (`ericson_math.cpp:30-31`) | closed — touching intersects | no |
| `test_circle_circle` (`:36-44`) | closed (`<=`) | no |
| `test_2D_segment_segment` (`:375, :382`) | open — strict `a1*a2 < 0` | no |
| `test_axis` (`narrow_phase.cpp:140`) | open — `<= 0.0f` | **yes**, `manifold.h` + pinned test |
| `test_point_triangle` | closed | no |

The composition at `contacts.cpp:33-49` is therefore correct — a closed filter running into an open decider — but by accident. In a tile-built level, colliders placed exactly flush is the common case, not the curiosity, and an undocumented tie-break is a defect waiting for a level designer.

The forward-looking half matters more: **the broad phase's correctness criterion is not a benchmark number, it is that it may over-report pairs freely and must never drop a pair whose shapes overlap.** Nothing currently stops someone writing a strict comparison inside it, and the resulting bug is a collision that goes missing on exactly-touching geometry. One line in `contacts.h` beside the AABB filter and one over the predicate block in `matt_math.h`. It fights T3 only if it tempts someone toward exact predicates or adaptive arithmetic from chapters 11-12, and it should not: the deliverable is a header comment and tests, not a robustness library.

### Pin the floating-point model in the build, and fix the `FLT_MIN` sentinel — adopt

IEEE-754's real contribution is the strict correctly-rounded accuracy requirement (11.2.1, pp.434-435); every epsilon and every `== 0.0f` guard in the tree rests on it. Table 11.1 (p.433) lists 2⁻¹²⁶ as "smallest positive" — an ordinary finite number, not an absent value.

`cmake/settings.cmake` sets `/W4 /WX /permissive- /sdl` and **no `/fp:` option at all**, so the build gets MSVC's default `/fp:precise` — the correct model, arrived at by omission, and the file's own header comment says its values were "carried over from the solution this build replaced". Several guards in the tree are sound only under exactly-rounded arithmetic: `normalized()`'s `if (length == 0.0f)`, `narrow_phase.cpp:189` and `:212` comparing against `Vector2F::ZERO` through an exact `operator==`, and every `are_equal` tolerance. Adding `/fp:precise` explicitly with a one-line comment means the day someone reaches for `/fp:fast` for T8 reasons they have to *delete a stated decision* rather than fill a blank — and `/fp:fast` was in all four configurations of the solution this build replaced, so that day has precedent. Directly serves the docs rule that a change fighting a decision must edit the decision in the same PR; you cannot fight a decision that was never written down.

Separately, `game/objects/player_consts.h:13-14` declares `float frame_time = FLT_MIN;` with the comment "if frame_time is FLT_MIN, then the frame_time will be set to the default", used at five construction sites and tested at `player.cpp:68` with `if (info.frame_time != FLT_MIN)`. That is the exact comparison the chapter spends most of its length warning about. `std::optional<float>` makes the sentinel unrepresentable, is a value, and costs nothing. It is game-side, but it matters to the engine as a pattern: nothing in `engine/` should acquire it.

### The `FLT_EPSILON` boundary tests are no-ops — adopt

Machine epsilon is a relative bound defined at 1.0, so `value ± FLT_EPSILON` does not produce a neighbouring float above magnitude 1 (11.2.1, pp.434-435).

`tests/math/math_tests.cpp:48`, `:53` and `:326-332` build "just inside / just outside" pairs as `10.0f ± FLT_EPSILON`. At 10.0f a ULP is 9.54e-7 and `FLT_EPSILON` is 1.19e-7, so both sides of every one of those boundaries are the bit-identical float `10.0f`. `std::nextafter` says "the adjacent float", which is what they mean. Free correction.

### Porting hygiene: transcribe the derivation, not the listing — adopt

This chapter's listings carry verified errata, and the failure mode is that none of them fails loudly:

| Erratum | Book | Symptom if transcribed |
|---|---|---|
| `RemoveObjectFromHGrid`: `occupiedLevelsMask &= (1 << obj->level)` where the intent is `&= ~(...)` | 7.2.1, p.303 | stops testing most levels after the first removal |
| Grid traversal's `tx`/`ty` initialisation branches opposite to the derivation two pages earlier | 7.4.2, pp.325-327 | wrong cells visited for every non-axis-aligned segment |
| `InsertObject`'s straddle test as printed | 7.3.2, p.312 | every object pinned at the root — a broad phase slower than the loop it replaced |
| `if (t2 > tmax) tmax = t2;` should be `<` | 5.3.3, p.181 | the far-plane early out never fires; the routine manufactures hits |

`ericson_math.cpp` sets the house method: a faithful transcription crediting the book by name, with 3D remnants left visible, an unexplained epsilon whose own comment admits it, and a page marker as the last line. That method is right, but it transcribes bugs as faithfully as algorithms. The rule to carry: **transcribe the derivation, not the listing, and pin the specific wrong answer with a test** — which is the precedent A2 and C2 already set. This is the concrete cost of T9 made visible: hand-rolling from a book means owning the book's bugs, and the only defence is that transcription is not verification.

### The 2²⁴ world-coordinate budget, as a load-time check — adopt

Every world coordinate in the tree is an integral value under 7000, three orders inside float's exact-integer range of 2²⁴. That budget is the reason `narrow_phase`'s "shapes that only touch do not overlap" test can compare against exact `0.0f` and be *right* rather than lucky. It deserves a sentence in ARCHITECTURE plus a bound check in the level loader, so a level authored at 10⁶ units is a load-time error rather than a mystery jitter report. It is the one durable residue of the extended-precision material rejected below — knowing how much precision a decision has left.

---

## Data layout and performance

Chapter 13 is unsafe to apply here until there is a benchmark, and the benchmark is item one above. What follows is the subset whose payoff does not depend on measurement, plus the archive of what does.

### Build the benchmark, and turn on link-time codegen — adopt

Ericson closes with "constant factors matter!" (13.9, p.551) and states every technique in chapters 11 and 13 as a *measured* quantity, printing assembly and counting cycles; he is explicit that several of them can go either way on a given architecture. He also names the abstraction penalty (13.6.3, p.543): cross-translation-unit calls to one-line functions defeat the optimizer, which cannot hoist, cannot prove absence of aliasing, and must reload through the reference.

`Vector2F::dot`, `normalized` and `operator-` are all out-of-line in `matt_math.cpp`, which builds into the `MattMath` static library, while `narrow_phase.cpp` builds into `ArtAttackEngine` — so `project()` makes a real cross-library call per point per axis. Two source-free fixes: set `INTERPROCEDURAL_OPTIMIZATION_RELEASE`, and move the genuine one-line `Vector2F` members into `matt_math.h` as `constexpr`. The second is better because it also makes them usable in constant expressions (T5).

### Give the value-semantics pass the targets the book names — adapt

Ericson's structure rules (13.3.1, pp.518-519): decrease the overall size of the structure, padding counts as waste, use offsets instead of pointers, do not store data that can be easily computed from already stored values. 13.5 (pp.531-532): software caches are arrays of fixed-size elements. 13.2 (p.516): avoid creeping featurism — one function per case that actually occurs.

`narrow_phase.cpp:16-27` is the precedent and its comment states the reason in the book's own terms; `PHILOSOPHY:421-422` states it outright: "Per-frame code performs no heap allocation... fixed-size geometry returns fixed-size containers." The targets, in order of cheapness:

| Target | Where | Cost |
|---|---|---|
| `RectangleRotated::points_` | `matt_math.h:1046` | a `std::vector` cache of four points inside a value type, rebuilt in every mutator; **strictly free to fix — nothing in the game is one** |
| `Shape::edges()` returning `std::vector<Segment>` | `matt_math.h:88` | a heap allocation per call, called *inside* intersection loops (`matt_math.cpp:377-378`, `:196`) |
| `Partitioner::partition` returning `std::vector<std::pair<int,int>>` | `partitioner.h:28` | by value, per frame, from the draw path |
| `RectangleF`'s vptr | `matt_math.h:185` | `Shape` has a virtual destructor and fifteen pure virtuals, so `sizeof(RectangleF)` is 24 for four floats; losing it is what makes "four AABBs per cache line" true instead of 2.67 |
| `Shape::intersects(const Shape*)` | `matt_math.cpp:97-113` | a five-way `switch` of `dynamic_cast` into a 28-path predicate matrix, of which the game exercises two |
| `Shape::clone()` | `matt_math.h` | `make_unique` per call; `calculate_object_collision_depth` (`collision_tools.cpp:286`) clones per query |

Real tension with T2 and blast radius on the last three: `RectangleF` is the return type of `bounds()`, `bounding_box()`, `intersection()` and `union_of()`, and the predicate matrix is the tested surface in `tests/math/math_tests.cpp` — so deleting it means deleting tests, which needs `narrow_phase` equivalents first. Field *reordering*, the book's headline 40% trick, does not pay here: the waste is fields that should not exist, not gaps between them.

### Replace the hand-copied `||` chains with `constexpr` bit masks, and use the layer/mask machinery the engine already ships — adopt

Set membership over a small enum is one AND against a compile-time constant, not a chain of compares and branches (13.8, pp.549-551 — `SmallPrime`'s five `||` comparisons become a bit table plus a shift, against ~20 instructions of pipeline refill per mispredicted branch; 13.3.1 p.519 recommends bit fields over Booleans; 13.7.3/13.8 recommend bitwise `|`/`&` over short-circuit forms where both sides are cheap).

`game/objects/collision_object_type.h:41-53` holds seven such predicates over a 22-enumerator enum — `is_player`, `is_dead_player`, `is_projectile` (ten terms), `is_team_a_projectile`, `is_team_b_projectile`, `is_structure`, `is_structure_ramp`. The ten-term projectile chain is then hand-copied at `structure_paintable.cpp:57-67` (inside `on_collision`, per contact per frame), `level.cpp:554-564`, `paint_tile.cpp:97-111` and `player.cpp:212-231` — five hand-maintained copies of "is this an offensive projectile". Meanwhile `engine/collision/collision_layer.h` already provides `CollisionLayer`/`CollisionMask` as `std::uint32_t` with a `constexpr layers_collide`, plus `CollisionTag` "so a response can recover what it hit without the engine having to know the vocabulary" — and the game uses none of it.

Ericson's own `mask = (x < 32)` does not save the shift from UB when the enumerator index reaches 32, so the honest guard here is a `static_assert` on the enumerator count rather than copying his mask. While there: `layers_collide` uses short-circuit `&&`, which is one avoidable branch on the same path; `((a_layer & b_mask) != 0u) & ((b_layer & a_mask) != 0u)` is branchless and stays `constexpr` — with a one-line comment saying why, or it reads as a typo. CONVENTIONS forbids a `consts` namespace, so the bit assignments live as snake_case constants next to `CollisionObjectType`.

### Split the paint tile hot from the cold — adapt

Beyond the index-range fix above, `game/objects/paint_tile.h:54-84` stores a 24-byte polymorphic `RectangleF`, a 32-byte `TeamColour` identical for every tile in the level, and an embedded `PaintTileSplash` (itself an `AnimationObject` plus a `GameObject` vptr plus another `RectangleF`) — and `StructurePaintable::update` calls `update` on every one of them every frame. After the splash animation finishes, every one of those calls reads a paused flag and returns having pulled the whole tile array through L1. The per-tile rectangle is also redundant: `origin + i * stride` from data the parent already holds (13.3.1 p.519; 13.3.2 pp.522-523 — express positions relative to the parent volume's origin). The derived rectangle is bit-identical to the stored one, so there is no accuracy trade at all.

*Tension with T4:* a paint tile stops being a thing you can hold and becomes an index into arrays owned by `StructurePaintable`, which has to be paid for with a documented contract on the owner.

### Write the C1 Scene's tree walks as an explicit index stack from the start — adopt

Ericson gives `MarkAllNeighbors()` recursively for brevity and immediately says not to write it that way (12.3 preamble, p.484): "a mesh of several thousand faces... the program stack is unlikely to cope." An explicit stack also makes it easy to avoid revisiting the node you came from and gives control over what is stacked. His traversal listings are `while (!node->IsLeaf())` loops stepping an index (8.4.1, pp.374-375).

`engine/ui/widget.cpp:96-110` unions children recursively, and `draw`/`update` recurse the same way. UI depth is small, so this is not a bug report — it is a constraint to apply *before* C1 rather than retrofit, because the Scene's bounds, cull and draw walks run over the thousands of objects a level holds (~5230 paint tiles). An explicit index stack over a contiguous node array is both the T11 shape and the robust one, and it lets the cull skip a whole subtree by index with no call overhead. The file already carries the scar: `UiTexture::bounds()` used to call itself, and `/W4` had been reporting C4717 the whole time unnoticed (`widget.cpp:163`).

### The build-with-pointers, ship-flat licence — adopt

Worth recording as a general rule because it settles a recurring argument: Ericson grants the escape hatch himself (8.3, p.358) — "the BSP tree used during tree construction should be designed for easy manipulation using regular pointers... the BSP tree used at runtime should be designed to use little memory... in a cache-efficient format." Build with pointers in an anonymous namespace, ship a flat `std::vector<Node>` with int32 child indices and leaves holding a `[first, count)` range. That is what makes any future tree admissible under T11, and it means the T11 objection to the tree families below is *not* the reason they are rejected.

### The cache-aware tail — archive

| Technique | Book | 2D translation, recorded so it is not re-derived | Why not now |
|---|---|---|---|
| k-d tree nodes packed 15 to a cache line by stealing mantissa bits | 13.4.1, pp.525-529 | a 2-d tree needs one axis bit: `00 = x`, `01 = y`, `11 = leaf` | reads a float through the inactive member of a union and casts node pointers through `int32` — UB, 32-bit-only, and would fail `/W4 /WX`; there is no tree |
| Compact AABB tree, 11-byte nodes | 13.4.2, pp.529-530 | four sides instead of six → five bytes | 34 static AABBs is 544 bytes; a linear sweep beats any traversal |
| SIMD, four AABB tests in 11 instructions | 13.7, pp.543-547 | 8 instructions with the Z rows removed | a 4× constant on a loop still O(n²) is worth far less than the O(n log n) the same SoA layout unlocks — layout, then sweep, then measure |
| Prefetching | 13.3.3, pp.523-525 | — | the book's own conclusion is prefetch *after* linearising |
| `restrict` | 13.6.2, pp.540-542 | — | spelled `__restrict` on MSVC, so a blanket policy is a dialect (T12); a wrong annotation miscompiles undetectably (T2). Keep in reserve for one short SoA kernel whose disjointness is provable by inspection |
| Predictive linearization caching | 13.5.2, pp.535-536 | — | its own exclusion clause names why: respawn is instant teleportation |

13.4.3 (p.530) supplies the deciding argument for the whole group: cache-oblivious beats cache-aware when the target hardware is unknown. This ships to unknown Windows desktop hardware, so a layout with a hardcoded 64 in it is wrong on the next machine, and the flat contiguous array is right on all of them.

### Do not rewrite `test_AABB_AABB` in the min-width form — reject

Ericson gives three AABB overlap tests, one per storage representation, so you write the test in whatever you store rather than converting first (4.2.1, pp.79-80) — and calls the min-width form "the least appealing". Beside it he offers implementing `Abs` by stripping the float's sign bit, and deciding integer interval overlap by deliberately forcing unsigned underflow.

The rewrite buys nothing. `top_left()` returns `Vector2F(x, y)` — zero arithmetic — and `bottom_right()` returns `Vector2F(x+width, y+height)` — two adds; both are inline and both temporaries vanish in any release build. Min-max costs one add per comparison, min-width one subtract. Equal op counts, on the hottest predicate in the engine, against a pinning test (`math_tests.cpp:248-272`, covering touching, just-not-touching, containment both ways and self-intersection) that would then need re-reasoning. The integer and sign-bit tricks are rejected outright under T4 and T12: the world is float throughout, `std::fabs` is one instruction on any modern target, and the unsigned-underflow form is a type-punning riddle needing a paragraph of comment.

What the passage is genuinely worth: it vindicates the storage choice. `RectangleF` **is** Ericson's min-width struct, and `RectangleF::offset` (`matt_math.cpp:908-917`) touches two fields where a min-max box would touch four — the right trade for a translate-only world. Record that so a future "switch to min-max for the faster test" refactor is recognised as a regression on the update path. The one useful residue of the surrounding text is the SIMD sentence, which argues for the 4-float POD box in the linearisation item.

There is a T2-shaped question hiding underneath, worth one line in `matt_math.h`: the min-width form assumes `width >= 0` where the min-max form degrades gracefully on an inverted box; `RectangleF` has no such invariant, and `from_top_left_bottom_right` (`matt_math.cpp:1035-1041`) can build an inverted rectangle straight from level JSON — which the AABB prefilter would then reject for every pair, silently.

---

## What the book confirms the engine already got right

These are not compliments; they are foreclosures. Each one is a place where the field's standard reference independently reached the decision already made here, which means the decision does not need revisiting and the *reason* belongs in the header so a future reader does not undo it.

| Decision in the tree | Book's independent agreement |
|---|---|
| SAT over both shapes' edge normals, minimum-overlap axis, first-separating-axis early out (`narrow_phase.cpp:163-219`) | 5.2.1, pp.156-158 — all three decisions, point for point |
| The edge-normal axis set with no cross-product family | 4.4.1, p.102 and 4.6.5, pp.121-122 — exact in 2D; the nine 3D axes exist for a configuration with no 2D counterpart |
| `RectangleF` as `{x, y, width, height}` | 4.2.1, pp.79-80 — the min-width struct, the right choice for a translate-only world; `offset` touches two fields, not four |
| `bounding_box()` as an extremal sweep over 3-4 vertices | 4.4.3 — optimal at that vertex count; hull preprocessing cannot improve it |
| `Manifold` and `Contact` as values; `std::vector<Contact>&` cleared and reused | 13.5, pp.531-532; 13.3.1, pp.518-519 — arrays of fixed-size elements, zero steady-state allocation |
| `Polygon { Vector2F points[4]; int count; }` on the stack, with the reason in the comment | 13.3.1, p.519 — do not store what you can compute, and do not allocate to reach it |
| `Handle<T>` (`engine/core/handle.h`) | 13.3.1, p.519 — use offsets instead of pointers |
| No collidable point or segment type — only fat objects collide | 11.3.4, p.448 — the whole recommendation, arrived at independently |
| `test_2D_segment_segment` puts every division in the construction and none in the decision | 11.5.2, pp.457-458 — exactly as prescribed |
| The degenerate-axis guard's *intent* | 10.3, pp.421-422 and Figure 10.5 — closes the 2D analogue (the mechanism still needs the length test above) |
| `closest_pt_point_triangle` as the full seven-region form | 3.10, pp.69-70 — Voronoi region classification, and the interior case a "simplification" would lose |
| `closest_pt_point_OBB` inverting the frame with two dot products | 4.4, p.101 — the concrete reason deleting `MatrixF` cost nothing on the OBB path |
| `narrow_phase` deriving normal orientation from signed projections, not vertex winding | Better than the book's own `ComputePlane` convention, and immune to the y-down ambiguity that `ericson_math.cpp:356` still carries |
| The bisection resolver deleted in favour of the analytic manifold | The book independently rejects the same family, naming the parallel-slide worst case; `RitterIterative`'s `NUM_ITER = 8` and Jacobi's `MAX_ITERATIONS = 50` carry the same smell. **The rule to keep: when a proposed geometry routine carries an iteration-count constant, look for the closed form first** |
| Layer/mask/tag on `CollisionObject` | 12.4, p.487 — Ericson's merge criteria arrive at the same list of properties a collider must carry (see the rejected merge pass below) |
| Collision geometry separate from render geometry (`Structure::collision_shape_`, `structure.h:37`) | 2.2.2, pp.11-12 |
| The closed AABB filter feeding the open SAT decider | 4.4.1, p.106 — the composition is safe in exactly the sanctioned direction (undocumented; see above) |
| `bounds()` as one structure serving both cull and pairing (`game_object.h:60-71`) | 7.6's preamble, p.338 — an explicit collision partition is not always necessary when the render structure can serve both. **This is a constraint on C1 Scene: build the cull index as a shared query structure, or the broad phase will build a second one over the same rectangles** |
| `RectangleF::union_of` + `UiContainer::bounds()`'s empty-child guard | 6.5.1, p.267 — the O(1) grow, with the degenerate case learned the hard way |
| `EPSILON` applied only to dimensionless and near-zero quantities | 11.3.1 — an absolute epsilon is valid exactly there. Currently true by luck, not by contract |
| `Vector2F::normalized()`'s zero-in-zero-out contract, with the bug it fixed written down | 11.5.2 — the lesson, learned once and documented; it simply was never carried into the ported routines |
| `mattmath::Dimension`, `partitioner.h`'s contract, `contacts.h`'s "what this is not" paragraph | The house documentation style the book's own prose keeps demanding |

One more that is worth stating as a design result rather than a coincidence. The engine's bounding volume is the AABB by contract, and of Ericson's five desirable BV properties (4.1, p.77) the only one the AABB fails is "easy to rotate and transform" — and **the engine never rotates a collidable.** That is the justification the `bounds()` comment does not currently state, and it is one line.

---

## What the book offers that this engine should decline

Two general points before the list. First, the rejections here are almost never about *dimension*. The 3D-only material is a small, cleanly separable set; most of what is declined is declined on **scale** (n ≤ 4 vertices, 34 static objects) or on **workload shape** (nothing is occluded, nothing is a mesh, nothing casts a ray). Second, several of these recast perfectly well to index arrays — the book asks for the recast itself — so T11 answers the objection rather than settling it. Recording the *actual* reason is what stops each one being rediscovered.

### The convexity-algorithm zoo — reject

| Technique | Book | Why not |
|---|---|---|
| V-Clip | 9.2.1, pp.386-388 | a pointer-linked feature lattice with a five-state transition table and an O(n) brute-force escape from a local minimum; at n ≤ 4 the walk is longer than the brute force it replaces |
| Dobkin-Kirkpatrick hierarchy | 9.3, pp.388-391 | O(log n) over nested polytopes; on a quad the hierarchy is one level deep |
| Hill climbing over vertex adjacency | 9.5, pp.404-407; 4.2.5, pp.84-85 | needs a per-vertex adjacency list and frame-to-frame walk state to beat a four-element linear scan over contiguous floats |
| GJK | 9.5, pp.399-407 | terminates with **separation distance**; `Manifold` is a normal plus a penetration depth, so it needs EPA bolted on. Cheap to build here (the 2D simplex is a point, segment or triangle, and both minimum-norm routines are already ported) — which is exactly why the rejection must be recorded rather than left to be re-litigated |
| Chung-Wang | 9.6, pp.410-412 | returns a boolean; ~2× faster than separating-axis GJK per Bergen, but the convergence proof is flawed and this is the correctness-critical path |
| Virtual `support()` on `Shape` | 9.5, pp.400-401 | CONVENTIONS: seams are concrete classes selected at build time, not vtables. Adding a virtual to the one path `narrow_phase` deliberately routes around is a regression, not a consolidation. Take the *theorem* — the edge-normal set is complete — and decline the abstraction |

The V-Clip patent (US 6,054,997) has expired, so the objection is engineering, not legal. Revisit only if a general convex-polygon `ShapeType` with dozens of vertices ever arrives, which nothing in PHILOSOPHY gives reason to expect.

### Spatial structures other than a sorted array — reject

BVH (6.2.1, pp.240-241), quadtree (7.3.1, p.309), k-d tree (7.3.7, pp.319-321), hgrid (7.2.1, p.302), grid-of-lists (7.1.2, p.287), and the array recastings the book itself recommends (6.6.1-6.6.3, pp.270-274 — children at `4i+1..4i+4`, preorder with one right link, 16-bit indices; 7.2.1's own preference for arrays over lists).

Not rejected for being pointer-linked. Rejected because at n ≈ 1000 in a fixed 6200×6200 square with no ray queries, a sorted array does the same job with no build, no depth bound, no cell size, no straddling policy and no dedup pass. A quadtree spends most of its depth on empty interior nodes over 18-34 static objects, which is Ericson's own argument at 7.2 (p.301) against tree tops. A BVH costs a rebuild every frame because the mover set turns over completely. An hgrid is the right answer to a harder problem than this one.

**BSP specifically** (8.1 p.351; 8.3.1 pp.357-358; 8.5 pp.381-382): the versatility is bought with a full plane equation per node where axis-aligned alternatives store one split coordinate, and the static set it would index is 18/33/34 objects with every coordinate a multiple of 5 and every shape but one axis-aligned — a general 2D line is three floats and makes every insertion a projection, where an axis-aligned split is one float plus a `Dimension` and makes it a comparison. Decisively, the cost is not the static set at all; it is the 600-1000 live projectiles, and a BSP is an offline structure over static geometry that cannot hold them. Nothing in the docs picks a structure or a filename — `ARCHITECTURE.md:136` is an example of *include-path style*, not a planned `broad_phase.h`, and `ARCHITECTURE:86` and `PHILOSOPHY:399` promise only that a broad phase prunes pairs. The filename is unclaimed.

**Cells and portals** (7.6, pp.338-340, Figure 7.21): a 2D sprite compositor has no occlusion — nothing is hidden behind anything — so there is no visible set to shrink, and the levels are single open arenas. The 2D analogue is real and buys nothing. The paragraph immediately *before* the section is worth more than the section, and it is already in this document's confirmations list.

### The point-set fitting family — reject

| Technique | Book | Why not |
|---|---|---|
| Ritter's two-pass approximate sphere | 4.3.2, pp.89-91 | nothing in the engine has a point cloud: `Polygon::MAX_POINTS` is 4, `Quad` and `RectangleRotated` hold four points, `Triangle` three. At four or fewer points the exact minimum enclosing circle is a closed form, not an iteration |
| `RitterIterative`, `NUM_ITER = 8` | 4.3.4, pp.98-99 | structurally what C2 just deleted: `BRACKET_ITERATIONS = 40` / `ITERATION_POWER = 1.5f` in `collision_tools.cpp`, a fixed-count loop with no convergence proof, replaced by the analytic manifold |
| Welzl's randomised exact minimum sphere | 4.3.5, pp.99-101 | double recursion with O(n) stack depth (Ericson warns about overflow himself), and it mutates its input by random swaps, so it cannot take a `const span` |
| Hill climbing for extremal vertices; coherence-based k-DOP realignment | 4.2.5, pp.84-85; 4.6.4, p.120 | worse than brute force at three or four vertices, where the exhaustive sweep is three comparisons and the whole vertex set fits in a cache line; needs pointer-linked adjacency and frame-to-frame walk state |
| Covariance matrix + `SymSchur2` + Jacobi (`MAX_ITERATIONS = 50`) | 4.3.3, pp.92-97 | in 2D the covariance is 2×2 symmetric — three numbers — and the eigenproblem has a closed form: `theta = 0.5f * std::atan2(2*c01, c00 - c11)`. Forty-five lines of Jacobi replaced by one, exactly |

The Welzl item leaves one free, general residue worth remembering: a point already inside a bound cannot change it, so a containment test gates any refit — with the T4 warning that a cached bound plus a staleness rule is exactly the hidden-invariant pattern the codebase is shedding.

The covariance rejection is the one to write down most carefully, because it **pre-answers the argument that would reopen a closed door.** `MatrixF` was deleted with "do not fix or resurrect" attached, and "we need PCA to fit an oriented bound" is precisely the request that would look like a reason to bring it back. In 2D it is three float accumulators and one `atan2`. Ericson's own warning at 4.4.3 (pp.108-109) is the second half: "all methods for computing bounding volumes based on weighting vertex positions should ideally be avoided", because the defining features of a minimum bounding volume are provably independent of interior and clustered vertices. So if something ever needs to bound a *group*, the answer is the extremal-point fold (`RectangleF::union_of`), not a statistic over the members' positions — do not bound a `StructurePaintable` by the mean and spread of its 1500 tile centres, and do not centre a camera on averaged player positions rather than their extent. The deviation-form advice (subtract the mean before accumulating products, because single-precision cancellation is real at 6200-unit coordinates) is a precondition on any variance the engine ever does accumulate, including the sweep-axis case above.

### The 2D 8-DOP — reject

A k-DOP is a slab-intersection volume whose normals are a fixed global set shared by every object (4.6.2, pp.117-118 — Figure 4.13 is itself a 2D worked example, so nothing needs translating). Four axes — (1,0), (0,1), (1,1), (1,−1) — give an octagon: an AABB with its corners sliced off, 8 floats against 4, and 4 interval comparisons against 2.

For an axis-aligned rectangle the two diagonal slabs are **fully determined by the AABB** and prune nothing the AABB filter has not already pruned. Every collidable in the entire content set is a `RectangleF` except one `TriangleRightAxisAligned` ramp whose AABB is twice its area. The win is one object in one level, against a doubled bound on every object in every level. T3 decides it: the simpler model is indistinguishable on this content, so the simpler model is correct. Revisit only if the engine gains many non-axis-aligned colliders — destructible terrain, rotated platforms.

The related struct-of-arrays proposal (precompute per-object shared-axis intervals so pairwise testing is pure interval comparison) is the right data-structure answer at the wrong time: the AABB filter in `find_contacts` already *is* that idea on two axes, and adding diagonals to axis-aligned content prunes nothing.

### Mesh topology and vertex welding — reject

Winged-edge, half-edge and winged-triangle (12.2, pp.474-477), the edge-to-face adjacency hash (12.2.2, pp.479-481), connected components (12.2.3, pp.481-483), and vertex welding (12.1, pp.467-474).

Welding has nothing to weld: every coordinate in every shipping level is an exact multiple of 5, and enumerating all static pairs found **zero interpenetrations and zero near-misses** — the content is authored as exact integers. The topology structures are pointer-linked node graphs answering a traversal question this engine does not have: there is no mesh, no editor, no 3D, and the collision world is 34 disjoint convex boxes rather than a planar subdivision. The winged-triangle's bit-packing of 2-bit edge indices into the low bits of triangle pointers is a T12 and T4 violation on top. And the edge-to-face table does not translate as cleanly as it looks: two axis-aligned boxes rarely share a *whole* edge (a small ledge against a tall wall shares part of one), so the 2D version must key on the supporting line and do 1D interval overlap within a bucket — a real complication, for the one t-junction that exists.

The one durable residue is not a structure. The static-geometry invariant (no two static colliders interpenetrate) is currently true and unwritten; a one-nested-loop check over 34 rectangles at load would turn a lucky property into a guaranteed one and fail loudly the day someone drags a box three units into a wall in the JSON.

### The collinear-merge pass — reject, and note what stopped it

Ericson makes face merging a build step to cut collision cost, and lists the criteria beyond geometric co-planarity that two faces must meet: same general direction, **same associated surface properties** (he names footstep sounds and friction), and at least one genuinely shared boundary edge (12.4, pp.487-489). Co-planar alone is never sufficient.

Enumerated across all three shipping levels: exactly-touching static pairs 16 / 38 / 18, interpenetrating pairs 0, and **four** geometrically mergeable pairs — all in `king_of_the_hill`, and every one with a differing collision type. `castle_entrance` is `STRUCTURE_JUMP_THROUGH` between two `STRUCTURE` floors; `castle_facade_floor_left/right` sit on `STRUCTURE_PAINTABLE` walls. Merging any of them would be a gameplay bug: a one-way platform becomes solid, or a paintable surface loses its tiles. So the pass has zero work to do on shipping content, **and the rule Ericson states is exactly what would have stopped someone writing it.** (The `turbulence` candidate — `central_divider` plus two ledges, all `STRUCTURE`, sharing a top edge at y = 5025 — is covered anyway: `central_divider_ledge_paint` sits directly on top of all three, so the walking surface there is already a single collider.) The correct reading is the cheaper one: fewer, larger rectangles in the JSON would get the same result with no code at all.

The engine's layer/mask/tag design is independently validated by the book arriving at the same list of properties a collider must carry.

### Coherence caches — reject

Two forms, rejected on the same structural ground. Caching the axis that separated a pair last frame and testing it first (5.2.1, p.158; 3.10, p.70 for the region-walk version), and maintaining pairwise distances so a collision is ruled out until combined maximum movement exceeds them (5.1 intro, pp.125-126).

Both need persistent state keyed by *pair*, and `Contact` is deliberately a value in a cleared-and-reused vector with no identity across frames — a pair cache means an associative structure on the heap, which is the shape T11 and T8 both push back on, and it would have to survive multi-core update. Both also buy little here for a specific reason: `contacts.cpp:39` already rejects non-overlapping pairs with an AABB test before `narrow_phase` runs, so `narrow_phase` almost never sees the far-apart pairs a cached axis exists to reject cheaply, and the pairs it *does* see mostly overlap — for which there is no separating axis to cache. The distance cache fails on the same arithmetic, and the objects that dominate the count are projectiles whose cached distance would expire within a frame or two. Above all, both are redundant: the pairs that would benefit are exactly the pairs a broad phase never enumerates, and `contacts.h` already names the broad phase as the sanctioned insertion point.

### GPU-assisted collision, interval arithmetic, and extended-precision integer geometry — reject

Chapter 10 in full (pp.413-426) does collision detection by rasterising geometry into depth and stencil buffers and reading the answer back through occlusion queries. In 2D the "image" of a scene along a view direction is a 1D line, so the convex test collapses into exactly the separating-axis projection `test_axis` already computes — **sampled where the engine is exact**, with a device round-trip where the engine has none, and the chapter concedes the whole family is approximate with both false positives and false negatives.

Interval arithmetic with outward rounding (11.4-11.4.2, pp.448-452) is "up to a magnitude slower" by the book's own admission — a straight T8 violation on the per-pair path — and adds a second numeric type to a library whose identity is that `Vector2F` is a value. Its two genuinely useful instances are already ported anyway: `sq_dist_point_AABB` + `test_circle_AABB` (`ericson_math.cpp:93-143`) is 11.4.2's worked example term for term, and `test_AABB_AABB` is interval separation on two axes.

The extended-precision integer machinery (11.5.1-11.5.3, pp.453-461 — 64-bit add/multiply overflow idioms, a bit-budget calculus, 16×16 partial products, Michelucci's continued-fraction rational comparison) solves a precision problem this engine does not have: every world coordinate is an integral value under 7000, three orders inside float's exact-integer range. Ericson himself closes 11.5.3 saying "this integer approach is more complicated than a floating-point approach using tolerances." Union type-punning and 16-bit partial products are a dialect, not the language (T12).

The one residue worth keeping is the bit-budget *habit* — knowing how much precision a decision has left — which is captured concretely by the 2²⁴ world-coordinate budget above.

### Ray, segment and cell-traversal material — archive, with the trigger named

Amanatides-Woo grid traversal with the common `sqrt(dx²+dy²)` factor cancelled (7.4.2, pp.324-328), locational codes and Morton order (7.3.3-7.3.5, pp.313-318), line picks (7.4, p.322).

`ericson_math.h:114-123` already ports `test_segment_AABB` and `test_2D_segment_segment`, and their only callers are `matt_math.cpp:228` and `:551` — the predicate matrix calling itself — plus the math tests. Nothing in `game/` casts a ray or a segment, and `mattmath::Segment` is not a `ShapeType`, so it can never be a collidable. **Trigger: a hitscan weapon** (`WeaponSniper` currently spawns a projectile like every other weapon) **or a line-of-sight test.** Three things to carry to that day. The walk must be **4-connected** in 2D — this is a correctness rule, not an optimisation, and the diagonal-stepping Bresenham-style line everyone already knows is the wrong one, because it skips cells. Ericson's in-cell hit verification is unnecessary here and will stay unnecessary as long as `narrow_phase` accepts only convex polygons of at most four vertices — *convexity*, not dimension, is what carries that argument. And do not write Morton codes speculatively: four lines of hex constants with no caller is a T4 loss with no T8 gain, and a flat row-major index is simpler and faster for a bounded world.

### Pair de-duplication mechanisms — archive

Needed only if a multi-cell index lands; the sweep emits each pair once by construction, which is the finding.

| Mechanism | Book | Verdict |
|---|---|---|
| Triangular bit array, one bit per unordered pair | 7.7.1, pp.341-342 | the only mechanism that dedups *pairs* rather than object *visits* — but at n ≈ 1000 it is a 61 KB memset per frame plus an index formula (`min*(2n - min - 3)/2 + max - 1`) nobody can read without the derivation |
| Time stamping / mailboxes | 7.7.2, pp.342-344 | the right shape if a multi-cell index arrives, and it also fits the per-view cull (`level.cpp:441-463`): four bytes per object, no clearing pass, and the overflow question closes by using `std::uint64_t` rather than by machinery. `if (stamp[i] == current) continue;` needs no comment |
| Amortized clearing | 7.7.3, pp.344-346, Table 7.2 | **hard reject** — it exists solely to rescue an 8-bit counter, and a wider counter deletes the whole section. A technique whose correctness needs a six-row table to demonstrate, whose failure mode is silently dropped collisions, is not worth 4 KB |

Keep the distinction straight: stamps dedup object *visits per query*; only the bit array dedups *pairs*. Conflating them would break the single-dispatch contract.

### Minimum-area bounding rectangle, rotating calipers, and convex hulls — archive

A minimum-area enclosing rectangle always has a side collinear with a hull edge (4.4.4, pp.110-112, Freeman75), so the continuous orientation search reduces to n candidate orientations; rotating calipers does the same in O(n) given the hull. Ericson's `MinAreaRect(Point2D pt[], int numPts, Point2D &c, Vector2D u[2])` listing is literally 2D source and returns a centre plus two axes — i.e. a `mattmath::RectangleRotated` — so it drops in unmodified. **The precondition that must travel with it is that the input is the convex hull**; over a raw non-convex point set the collinearity premise fails and the answer is a local best.

No caller. `RectangleRotated` is never a collidable and nothing fits a bound to a point set. Plausible future customers are thin and all load-time: a tight oriented collider around a sprite's opaque region, or replacing the hand-authored `zoom_out_finish_bounds` in the level JSON with a bound computed from geometry. Worth knowing it exists chiefly so nobody reaches for iterative angle sampling — which is what the book does in 3D (4.4.5) and would be a T4 violation in 2D where the exact answer is one pass. Honest effort is two functions, not one, because the hull is missing too.

**If a hull is ever needed**, the answer is Andrew's monotone chain, not Quickhull. The book notes it is easy to write in situ over an array, which makes it the rare exact fit for T11; 3D Quickhull and hill-climbing support search are pointer-linked vertex-adjacency graphs, fighting T11 and pointless at n ≤ 4. Record that choice so Quickhull's dimensional generality — a non-goal here — does not win the argument later.

### Ear clipping and Hertel-Mehlhorn — archive, as the sanctioned answer to a concave level shape

Every simple non-triangle polygon has at least two non-overlapping ears, so repeated ear removal triangulates any simple polygon into exactly n−2 triangles (12.5.1, pp.496-499); Hertel-Mehlhorn then deletes every diagonal whose removal creates no reflex vertex, giving a convex decomposition provably within 4× of minimal, and Ericson's better formulation folds that deletion into the triangulation so no diagonal list and no mesh adjacency is needed (12.5.2, pp.500-502). 8.2.3 (pp.354-355, Figure 8.5) adds: cut at the reflex vertex first.

Ericson's own listing uses `int prev[]` and `int next[]` as a doubly linked list built from two fixed arrays — **zero pointers, zero nodes, zero allocation.** It needs no T11 recasting at all, which makes it a good exhibit for the next listing that arrives full of node pointers.

No consumer today, and the building blocks are present (`signed_2D_tri_area`, `test_point_triangle`). Its value is a recorded decision plus a header line: **the moment a level wants a concave platform — an L-ledge, a U-pit — the answer sanctioned by both the book and the existing code is to cut it into convex pieces at authoring or load time and register each as its own `CollisionObject`, not to add a concave branch to the hottest geometric routine in the engine.** `narrow_phase.h` documents which shape types are supported but not what to do when the level wants one that is not, so today that answer would be discovered by someone writing a concave case into JSON and watching it behave wrongly rather than throw.

### Multi-contact separation as a 2-variable LP — archive

Seidel solves a d-variable LP with m half-space constraints in expected O(d! m) (9.4.1.2, pp.396-398); at d = 2 the factor is 2, the recursion is exactly one level deep (a 1D interval intersection), and Ericson says outright it is "quite efficient for small d". Fourier-Motzkin (9.4.1.1, pp.394-396) eliminates one variable at a time; at d = 2 there is one elimination round and no blow-up, and the book's worked example (Figure 9.7) is two 2D triangles.

Two recorded answers, neither to be built now. **First:** `resolve.h` cannot express simultaneous resolution, so a player wedged in a corner receives two translations that partly undo each other and the order they arrive in decides the result. "Find the smallest translation satisfying every contact constraint at once" removes the ordering dependence, is fixed-size and stack-only, and is the known answer *the day corner wedging is a reported bug*. **Second:** Fourier-Motzkin gives an independent boolean oracle for `narrow_phase` that shares no code and no reasoning with SAT — convert both polygons to edge half-planes, eliminate x, compare against `has_value()` — and it yields a *witness point* inside the overlap, which is a stronger assertion than a boolean when a test fails. Test-only, so its exponential worst case and its allocations never touch a frame.

*Philosophy:* the LP serves T2 (the answer stops depending on iteration order) and T9, but fights T3 and T4 hard — a randomized LP solver is cleverness, and a paint shooter's player may never feel the difference. Ericson's related judgement is worth banking with it: for a pure feasibility question the objective is arbitrary, so the boolean form needs no objective and only the "which translation" form does — at which point the objective is a gameplay opinion (prefer up, prefer smallest), which is exactly the opinion `resolve.h` refuses to hold.

### The 3D-only material, rejected by name so it is not mined again — reject

| Material | Book | One-line reason |
|---|---|---|
| ORIENT3D, INSPHERE | 3.1.6.2, 3.1.6.4, p.34 | 3D; the 2D sibling is `signed_2D_tri_area`, already in the tree |
| Scalar triple product | 3.3.7, p.45 | parallelepiped volume; its only transferable content — zero determinant means linear dependence — is the perp-dot in 2D |
| Lagrange/Jacobi identities, the five-multiply cross rewrite | 3.3.6, pp.43-44 | the book counts 13 operations against 9 and declines to claim a win without hardware evidence. **Keep the discipline, not the rewrite** — counting total operations before claiming a performance win is the standard the missing benchmark harness implies |
| Three planes meeting in a point; the dihedral angle | 3.6, p.55 | no 2D content |
| INCIRCLE2D | 3.1.6.3, p.34 | genuinely 2D, but it is the core predicate of Delaunay triangulation; nothing here triangulates or remeshes, `Quad::triangles()` splits along a fixed diagonal with no quality criterion and wants no better one |
| Generalised barycentric / mean value coordinates | 3.4, p.52 | genuinely 2D, but interpolates a continuous field over a convex n-gon; `ShapeType` is a closed set of five, `MAX_POINTS` is 4, and the paint model is discrete — up to 1500 uniform tiles each fully one colour, so there is no continuous quantity to interpolate |
| Polyhedra, 2-manifolds, the d-simplex, Euler's V+F−E=2 | 3.7 | 3D combinatorics |
| 3D Quickhull, hill-climbing support search | 3.9 | pointer-linked vertex-adjacency graphs; pointless at n ≤ 4 (see the monotone-chain note above) |

The mining agents handled this material correctly and did not smuggle any of it in as a 2D technique, which is the failure mode worth checking for.

---

## Sequencing

The dependencies are real, and taking them out of order wastes work.

**1 — Instruments, before anything measurable.** The benchmark harness and the reference-algorithm oracle (2.7.1, pp.20-21; 6.1.2, pp.237-238). Both are cheapest right now: the all-pairs loop *is* the oracle and it disappears the moment the loop is edited, and the first benchmark run establishes a baseline rather than catching a regression, because the game is not on this path yet. Set `INTERPROCEDURAL_OPTIMIZATION_RELEASE` and pin `/fp:precise` in the same commit — both are free and source-free.

**2 — Corrections that cost nothing and are on the clock.** Delete `Vector2F::cross`, `mattmath::sign`, `rotate_vector_by_ref`, and the `hypotenuse()` chain — all dead, all wrong, all deletions. Fix `separation_along`'s guard. Fix the two NaN range-test forms. Fix `Quad::is_valid()` to test convexity **before** the review backlog's `segments_intersect` boundary change lands, or every `Quad` constructor in the tree throws. Fix `intersect_moving_AABB_AABB` or delete it. Point `circles_intersect` at `test_circle_circle` and `circle_rectangle_rotated_intersect` at `closest_pt_point_OBB`. Fix `Player::bounds()` — it is one of the very few items with a live per-frame payoff today, and it must land before any index is benchmarked, or the measurement will say the index does not help.

**3 — Contracts, while the modules are open.** The numeric policy paragraph in PHILOSOPHY and `matt_math.h`; the axis-set completeness claim and its convexity precondition in `narrow_phase.h`; the boundary-direction rule in `contacts.h`; the fifteen missing contracts in `ericson_math.h`, plus clearing the commented 3D `test_triangle_AABB`, the `// 132` marker and the y-up winding comment. These are prerequisites that make several later arguments decidable rather than arguable, and they discharge an obligation the review already closed against `partitioner.h`.

**4 — Measure the range the game actually runs at.** Re-run the `narrow_phase` sweep translated by (+6000, +5000). One loop parameter, and it settles the resting-contact skin question and the local-origin question empirically instead of by argument. Add the tunnelling budget constant and its load-time assertion in the same pass — that is the item that makes PHILOSOPHY's declined-CCD position honest, and it is a constant and a test, not an algorithm.

**5 — The engine collision path, in this order.** Distinct-axis reduction (provably inert, bit-identical). Then the rect-rect fast path with its equivalence test. Then `polygon_from`'s OBB route off `Quad`. Then the flat extent array. Then the sweep, with the sort-on-minimum rule and the contact-ordering decision in the same commit. Each earlier step makes the next one cheaper or safer to measure.

**6 — Game-side, and it is the largest measured win in the tree.** The paint-tile index range in `StructurePaintable::on_collision`, with the four per-face `(begin, count, origin, stride)` tuples that `generate_paint_tiles` already implies. It needs no engine primitive and it is hours of work.

**7 — The deferred value-semantics pass, now unblocked.** Its targets are named above and its design brief is the SSV pattern: fewer virtuals, values not hierarchies, fixed-size inner primitives, and one distance kernel set rather than an N² intersection matrix. Start with `RectangleRotated::points_`, which is free.

Everything else in this document is either archived with its trigger written down, or rejected with the reason recorded so that the next reader of these chapters stops there rather than re-deriving the same conclusion.
