# Mining *Real-Time Collision Detection* for ArtAttack

> Christer Ericson, *Real-Time Collision Detection* (Morgan Kaufmann, 2005), read
> in full — printed pages 1–551 — by 50 agents on 2026-08-08, against the tree at
> `5101879`.
>
> **This is a candidate list, not a plan.** Nothing here has been decided; the
> verdicts are the sweep's opinion, argued against [PHILOSOPHY.md](../../design/PHILOSOPHY.md)
> and checked against the source. The engine's own decisions live in
> [round-2/PLAN.md](../round-2/PLAN.md).

**123 items** — 52 adopt · 46 adapt · 1 already-have · 11 archive · 13 reject.

Distilled from 657 raw candidates. The book was cut into 38 slices aligned to real
section boundaries, weighted by relevance rather than page count: chapter 5 (basic
primitive tests) got 8 agents, chapter 7 (spatial partitioning) 6, chapter 10
(GPU-assisted) 1. Three agents first surveyed `engine/collision`, `engine/math`,
the game's real call sites and the design docs; 38 mined one slice each; six merged
duplicates and re-opened every cited file to verify each *already present* and
*absent* claim; three wrote the synthesis below, the ranked shortlist, and the
account of what the sweep missed.

Why this book and not another: `engine/math/ericson_math.cpp` is already a partial
port of it, credited at the top — and its last line is a bare `// 132`, marking the
page where the original port stopped. This sweep is what pages 133–551 have to offer.

| Document | What it holds |
|---|---|
| **README.md** (this file) | Method, and the ranked shortlist |
| [FINDINGS.md](FINDINGS.md) | The findings, organised by engine area |
| [GAPS.md](GAPS.md) | What the sweep missed or got wrong, by its own critic |
| [all-findings.md](all-findings.md) | All 123 items in full, grouped by theme |

Every technique carries its book reference (section and printed pages) so it can be
looked up. Verdicts read: **adopt** (take it as the book gives it), **adapt** (the
idea is right, the form needs recasting for 2D or for value semantics),
**already-have**, **archive** (real, but not for this engine soon), **reject**.

**Two standing cautions.** The agents' performance arithmetic was wrong by three
orders of magnitude in at least one place, and there is no benchmark harness to
settle any of it — so treat every speed claim here as a well-argued guess until
something measures it. And `engine/collision/` still has no consumer outside
`tests/`, so these findings describe what the engine path will cost *after* the
game is ported onto it, not what it costs today.

---

## Spot-checks

The grounding pass re-opened every cited file, and the shortlist's load-bearing
claims were then independently checked against the source a third time:

| Claim | Result |
|---|---|
| `ericson_math.cpp` ends at page 132 of the book | **Confirmed** — last line before the closing brace is a bare `// 132` |
| `test_AABB_AABB` swallows NaN into "intersecting" | **Confirmed** — `ericson_math.cpp:30-33` rejects on separation and otherwise `return 1`; both comparisons are false for NaN, so the accept branch is reached by falling through |
| `Vector2F::cross` never subtracts | **Confirmed** — `matt_math.cpp:1361` returns `Vector2F(x * other.y, y * other.x)`, the determinant's two products as a vector. It is not a cross product in any dimension |
| `mattmath::sign` truncates a signed area to `int` | **Confirmed** — `matt_math.cpp:65-70` computes the determinant correctly, then `static_cast<int>` rounds toward zero, so every area under one square unit reports collinear. The name also promises −1/0/+1, which it does not return |
| `separation_along` guards the divisor, not the result | **Confirmed** — `resolve.cpp:17-23` returns `Vector2F::ZERO` when `\|dot\|` ≤ `EPSILON`; a dot just above 1e-4 passes and scales the translation by ~10⁴ |
| `Player::bounds()` inflates by 200 in each direction | **Confirmed** — `player.cpp:869`, `inflate(Vector2F(200.0f, 200.0f))` on a 52×120 collider |
| No `assert` anywhere in `engine/` or `game/` | **Confirmed** — the only two matches are the word inside comments (`asset_manifest_loader.cpp:12`, `contacts.h:16`) |

---

## Mining "Real-Time Collision Detection" for ArtAttack — ranked shortlist

| # | What | Book | Where it lands | Effort | Why it ranks here |
|---|------|------|----------------|--------|-------------------|
| 1 | `slide()` — remove the into-surface component of a velocity instead of zeroing the axis | 3.3.3, pp.40-41 (parallel/perpendicular decomposition) | New free function in `engine/collision/resolve.h`, beside `separation()`/`separation_along()`; retires `Player::on_structure_ramp_collision`'s five `set_velocity_y(0.0f)` branches | S | Two multiplies and a subtract, and it is the only item on this list that fixes behaviour a player can feel today (a slope deletes your climb rate). It is also the missing piece that lets C2 **delete** the eight cardinal handlers rather than wrap them. Cheap, live, unblocks a planned deletion. |
| 2 | One named 2D cross (`ORIENT2D` / perp-dot); delete `Vector2F::cross`, `mattmath::sign`, `rotate_vector_by_ref` | 3.1.6.1 pp.32-33; 5.4.2 p.205; 3.1.1 pp.25-26 | `matt_math.cpp:1359`, `:65`, `:1449`; give `signed_2D_tri_area` (`ericson_math.cpp:358`) a stated identity in the header | S | Three public functions with plausible names return wrong answers, all three have zero callers, so the fix is deletion plus one `static float cross()`. It is the keystone: rows 5 and 7 become one-liners the moment the scalar has a name. |
| 3 | Rewrite the two range tests that swallow NaN into the form where the accept branch must be *reached* | 11.2.2, pp.436-437 (`NumberInRange`) | `ericson_math.cpp:30-31` (`test_AABB_AABB`), `narrow_phase.cpp:140` (`test_axis`), plus `project()`'s non-symmetric `min`/`max` fold at `:94-101` | S | Same instruction count, and `test_AABB_AABB` is genuinely live via `Level::is_object_out_of_bounds`: one NaN coordinate today yields an object that is never culled and reports a contact against everything in the level. A miss is strictly the better failure. |
| 4 | A rectangle contributes **two** SAT axes, not four — and for an AABB they are compile-time literals | 5.2.9 p.169; 4.4.1 pp.105-106; 3.11 pp.70-72 (Minkowski box) | `narrow_phase.cpp:176-209`; `polygon_from` gains `Vector2F axes[4]; int axis_count` | S | Provably inert: projecting on `-n` swaps `forwards`/`backwards` bit-for-bit, so the duplicate can never win the strict `<`. Rect-vs-rect drops from 8 axis tests and 8 `sqrt` to 4 and 0 with no second code path to keep in agreement. This is every collision the game will run after C2. |
| 5 | Point-in-triangle as three signed-area sign tests (`SameSign` form); delete `barycentric()` | 5.4.2, pp.203-206; degenerate-denominator rule 5.1.8 p.147 | `ericson_math.cpp:399-422`; live via `rectangle_triangle_intersect` → `Triangle::contains`, four times per player-vs-ramp test | S | Removes an unguarded division and a silent NaN false-negative from the one non-rect-rect predicate the game actually runs, and `barycentric`'s only caller is the function being rewritten — so it goes with it. Must use `SameSign`: nothing constrains triangle winding. |
| 6 | `closest_pt_point_segment`: take the p.129 deferred-divide form | 5.1.2, pp.128-129 (the second listing supersedes the first) | `ericson_math.cpp:426-437`; reached from `circle_segment_intersect` and `circle_rectangle_rotated_intersect` | S | A zero-length segment currently returns `(NaN, NaN)`, both clamps fail silently, and the caller reads it as "no intersection". The book's own next page is cheaper *and* only divides where the divisor is provably positive. One function body plus a pinning test. |
| 7 | `Quad::is_valid()` must test convexity, not simplicity | 3.7.1, pp.59-62 (`IsConvexQuad`, the transverse-diagonal criterion) | `matt_math.cpp:2886`, called from five constructors and four setters; consumers are `narrow_phase` and `Quad::triangles()` | S | The constructor enforces a weaker invariant than both its consumers require — a dart passes and gets a confident wrong manifold. The replacement is one interior diagonal test instead of six segment tests, deletes a `std::vector` from a value type's constructor, and *pre-empts* the backlog change that closes `segments_intersect`'s boundary and would otherwise make every `Quad` constructor throw. |
| 8 | `separation_along` must guard the output magnitude, not the divisor | 11.7 p.463; 11.5.2 pp.457-458 | `resolve.cpp:14-24` | S | `dot` = 1.0001e-4 clears the `EPSILON` guard and returns a translation ~10⁴× the penetration — a silent 10,000-pixel teleport out of the primitive that exists to be the safe arithmetic. Three lines, in brand-new code, and the header names a unit-axis precondition it never checks. |
| 9 | Write the three contract paragraphs the collision module owes: the face-normal axis set is **complete in 2D**; convexity is the precondition; a closed filter feeding an open decider is the rule | 4.4.1 pp.101-102 ("works in 2D, does not work in 3D"); 3.8 p.64; 4.2.1 pp.79-80 with 4.3.1 p.88 | `narrow_phase.h:14-48`, `contacts.h:37-45`, `ericson_math.h:117` | S | Zero runtime cost, and it converts "believed correct" into "provably exact" with a citation — which forecloses a future reader importing cross-product axes from 3D SAT code and closes the GJK question permanently. Ship it with row 7 or the convexity sentence is a lie. |
| 10 | Derive, name and test the anti-tunnelling displacement bound the philosophy already claims | 5.5 pp.214-215 (displacement < combined extent); 2.4.3 pp.16-17 | `projectile_consts.h` / `weapon_consts.h`, a load-time assertion over level geometry, and an amendment to `PHILOSOPHY.md:71` | S/M | The declined-CCD position rests on a cap that does not exist: the sniper covers 33.3 px/step against a 30 px budget, and a spray blob under gravity passes it after 0.6 s of fall. The fix is a constant and a test, **not** a swept solver — this repairs the decision rather than reopening it. |
| 11 | `Player::bounds()` = union of player and weapon rectangles, not `inflate(200, 200)` | 6.1 pp.235-236 (minimal volume); 6.5.1 p.267 (`AABBEnclosingAABBs`) | `player.cpp:864-871`; `RectangleF::union_of` and `UiContainer::bounds()` are the in-tree precedents | S | 452×520 for a 52×120 collider — 37.7× by area, on the object that queries most. It costs per-view cull time *today* and becomes a straight candidate-pair multiplier the moment anything indexes `bounds()`, which `game_object.h` already invites. A union is shorter than the magic number. |
| 12 | Re-run the collision pinning tests translated to real world coordinates; write the tolerance ordering down | 11.2 pp.429-431 (Fig 11.2); 11.3.2 p.444 (determine tolerances empirically) | `narrow_phase_tests.cpp:157` (one loop parameter); `matt_math.h:47`, `resolve.h`, `narrow_phase.h` | S | Every collision test runs within 100 units of the origin; the game runs at 6200, where one ULP is 4.88e-4 and `EPSILON` (1e-4) is exact equality wearing a costume. This is the cheapest way to settle empirically whether the engine needs a contact skin, instead of adding one speculatively (T3). |
| 13 | A benchmark that counts **Nv and Np**, not milliseconds | 6.1.2 pp.237-238 (the cost model); 8.3.6 pp.373-374 (replay database) | `tests/collision/`, counting `AABB_intersects` at `contacts.cpp:39` and `narrow_phase` at `:44` | M | `PHILOSOPHY.md:424` already makes benchmarks an obligation equal to tests and nothing implements one, so T8 is currently unenforceable. The book's contribution is *what to count*: pair counts are pinnable integers where wall-clock is not. Gate rows 14 and 16 on it. |
| 14 | Fill one flat POD extent array per frame, then sweep it on one axis — sorting on the min and breaking on the max, with contact order declared unspecified | 7.5.2 pp.336-337; 7.5 p.333 (the containment warning); 7.7 pp.341-346 | Inside `find_contacts`, `contacts.cpp:15-54`; signature unchanged as `contacts.h:41-46` promises | M | Removes five virtual calls guarding a bitwise AND from every pair, and needs no cell size — which matters because the content's 466:1 extent spread makes a uniform grid unsizeable. The three details are non-negotiable and belong in the same commit: sort on min or containment silently dies; state ordering or `contacts_tests.cpp:99-102`'s positional assertions fail on a *correct* rewrite. |
| 15 | Project a rotated rectangle from centre/axes/extents; stop materialising a `Quad` in `polygon_from` | 5.2.3 pp.161-164 (`r = e₀\|u₀·n\| + e₁\|u₁·n\|`); 4.4 p.101 | `narrow_phase.cpp:69-79`; kills `RectangleRotated`'s `std::vector<Point2F>` cache too | S | Three heap allocations, twelve segment tests and two throw sites per shape per pair — inside the function whose own comment boasts that it avoids exactly that. Latent (nothing in the game is a `RectangleRotated`), but it is a file contradicting its own stated invariant. |
| 16 | Address the paint-tile strips by index range instead of scanning 1500 | 7.6 p.340; 4.1 pp.75-76 (a group bound and an object bound are different jobs) | `structure_paintable.cpp:78-90`, using four `(base, count, origin, pitch)` tuples `generate_paint_tiles` already computes and discards | M | The largest real per-frame cost in the tree, and pure arithmetic — no structure, no engine primitive. Ranked here and not higher precisely because it is game-side proof-work: the engine residue is one addressing helper in `engine/math`. Use `paint_tile_width`, not `WIDTH`, or the mapping drifts. |
| 17 | Write the closed doors down, by name and with reasons | 9.2-9.6 (GJK, V-Clip, Dobkin-Kirkpatrick, hill climbing, Chung-Wang); 8.3 (BSP); 4.6.2 (k-DOP); 4.3.3-4.3.5 (Ritter, Welzl, PCA); 12.1-12.2 (welding, mesh topology); ch.10 (image-space); 11.4-11.5 (interval and integer geometry) | One "what this is not" paragraph each in `narrow_phase.h`, `contacts.h`, `ericson_math.h` | S | These die on scale and on question, not on dimension: `MAX_POINTS = 4`, so a four-element scan beats every asymptotic structure, and GJK/Chung-Wang answer distance or a boolean when `Manifold` is defined as a normal plus a depth. Recording *why* is what stops PCA being the argument that resurrects `MatrixF`, and stops chapter 9 being re-mined. |

### The top three, and what I would do first

**Row 1 wins because it is the only book item that touches code the game is running and the only one that makes a planned deletion possible.** `Player::on_structure_ramp_collision` answers every slope contact by zeroing the whole vertical velocity in five branches, and resolves along the ramp's bounding box rather than its hypotenuse. Ericson's §3.3.3 decomposition is two multiplies and a subtract, it belongs in `resolve.h` next to `separation_along` which already speaks that vocabulary, and once it exists C2 can delete the ramp handler and the eight cardinal handlers instead of wrapping them — `narrow_phase` already returns the slope normal analytically and there is a pinned test for it. It also costs nothing philosophically: the simplest believable model of a slope is "you keep your tangential speed", which happens to also be the physical one, so T3 and T2 point the same way for once.

**Row 2 wins on leverage per keystroke.** The engine has written the 2D cross three times: once correctly, buried under a triangle-shaped name nothing reaches for, and twice broken — `Vector2F::cross` returns the two products of the determinant without ever subtracting them, and `mattmath::sign` truncates a signed area to `int`, so every sliver under one square unit reports collinear. Both are dead, which makes them free to delete and dangerous to leave, because each is the method the next reader reaches for by name while following this exact book. Give the scalar one home with `ORIENT2D` written in the header and rows 5 and 7 stop being rewrites and become one-liners.

**Row 3 wins because it is the cheapest catastrophic-failure fix on the list and it lands on live code.** `test_AABB_AABB` is written as "reject if separated, otherwise accept", so a NaN coordinate falls through to *intersecting* — and that routine is `Level::is_object_out_of_bounds`. One NaN today produces an object that is never culled and contacts everything in the level, dispatching a NaN normal into every response, which then propagates into positions. The safe form costs the same instructions. There is currently no way to notice: `grep` finds no `isfinite` anywhere and not one non-finite input case in `tests/`.

**I would do row 2 first, even though it ranks second.** The ranking is by value of the outcome; the work order is by dependency, and the cross-product commit is a prerequisite for two items above and below it while consisting almost entirely of deletions — the lowest-risk change on the page. Row 1 follows immediately, in the same week, because it is the one the player feels. Then rows 3, 5, 6 and 8 as a single "guard the divisions and the comparisons" pass while those files are already open, followed by rows 7 and 9 shipped together — the convexity sentence in `narrow_phase.h` is false until `Quad` enforces it, so they are one commit or neither.

**What I am deliberately holding back, and why the famous stuff is at the bottom.** The broad phase sits at 14, below the benchmark at 13, because `find_contacts` has no production caller and every throughput claim about it is currently a well-argued guess — the brute-force loop is the free reference oracle right now and gone the moment anyone edits it. CCD stays declined; row 10 implements the decision the philosophy already made rather than reopening it. And GJK is last, inside a paragraph explaining why it will stay last: at four vertices a linear scan over contiguous floats beats any support-mapping search, GJK terminates with separation distance and needs EPA bolted on to produce the penetration depth that `Manifold` exists to carry, and the completeness result in row 9 removes the only accuracy argument anyone could make for it. A one-axis sweep over a flat array is the technique this engine should take; the convexity zoo is the technique it should record refusing.
