# Labrador — file-length audit and split plan

| | |
|---|---|
| Engine revision | `0702776fdcf376233d077eac1204b41b920297fa` |
| Date | 2026-08-11 |

Produced by auditing the eight longest first-party units in the tree against CONVENTIONS' own
standard for file length, which sets no line count on purpose. Each unit was mapped by one pass
and then handed to a separate adversarial pass whose brief was to kill the proposed split - to
stop motion being mistaken for improvement. Four of eight survived. The four that did not are
recorded with their reasoning in Findings and again in the last section, because that is the
evidence this plan was filtered rather than collected.

Line numbers are against the revision above. `external/rapidjson` is excluded throughout as
vendored; it holds the four largest files in the tree.

CONVENTIONS sets no line count on purpose, and this audit honours that: "A file is too big when a reader can no longer hold what it does in their head — and the cause is almost always a type that grew past one job, so the fix is usually to split the type, not the file." Measured against that standard rather than against a number, two of the eight audited units fail — `engine/math/matt_math.{h,cpp}`, where `Shape` grew a second job that forced eleven types into one header, and `tests/math/math_tests.cpp`, which is a pile rather than a structure. Two more have a real but smaller defect: `device_resources.cpp` carries thirty-seven lines of engine code inside seven hundred and forty lines of vendor code, and `application.cpp` is 58% Win32 windowing wearing the shell's name. The remaining four are long and coherent, and this plan leaves them alone.

Facts below were checked against the tree rather than taken from the audits. Where an audit and its adversarial review disagreed, the disagreement is settled in the finding and the ruling is stated.

## Census

First-party files over roughly 200 lines. The verdict column applies only to the audited units.

| Lines | File | Verdict |
|---:|---|---|
| 2783 | `engine/math/matt_math.cpp` | **restructure the type, then split** |
| 1863 | `tests/math/math_tests.cpp` | **split** |
| 892 | `engine/math/matt_math.h` | **restructure the type, then split** |
| 779 | `engine/render/d3d11/device_resources.cpp` | **shed the pool, keep the file** |
| 678 | `engine/app/application.cpp` | **shed the platform** |
| 551 | `tests/core/state_context_tests.cpp` | **keep** |
| 531 | `engine/render/colour.cpp` | **keep** (one-line include fix) |
| 474 | `engine/math/ericson_math.cpp` | **keep** |
| 430 | `engine/render/d3d11/renderer.cpp` | **keep** |
| 333 | `engine/render/renderer.h` | **keep** |
| 329 | `engine/collision/narrow_phase.cpp` | not audited |
| 321 | `engine/ui/widget.cpp` | not audited |
| 317 | `tests/scene/scene_tests.cpp` | not audited |
| 310 | `tests/assets/json_tests.cpp` | not audited |
| 281 | `tests/collision/narrow_phase_tests.cpp` | not audited |
| 276 | `engine/scene/scene.h` | not audited |
| 274 | `engine/core/state_context.cpp` | not audited |
| 273 | `engine/assets/json.cpp` | not audited |
| 271 | `tests/collision/broad_phase_tests.cpp` | not audited |
| 260 | `engine/render/colour.h` | **keep** (one-line include fix) |
| 254 | `tests/collision/contacts_tests.cpp` | not audited |
| 251 | `engine/core/state_context.h` | not audited |
| 250 | `engine/scene/scene.cpp` | not audited |
| 243 | `engine/app/application.h` | **shed the platform** |
| 234 | `tests/input/gamepad_tests.cpp` | not audited |
| 228 | `engine/collision/broad_phase.cpp` | not audited |
| 225 | `engine/math/ericson_math.h` | **keep** (one dead include) |
| 225 | `engine/assets/resource_loader.cpp` | not audited |
| 221 | `engine/render/resolution_manager.cpp` | not audited |

Five files sit above 500 lines. Three of them are being changed. The other two — `colour.cpp` and `state_context_tests.cpp` — are long for reasons CONVENTIONS explicitly protects, and the reasoning is in Findings.

## Findings

### 1. `engine/math/matt_math.{h,cpp}` — a type past one job, and eleven types in one header

3675 lines across the pair, and the largest first-party file in the tree. The diagnosis is CONVENTIONS' own, arrived at rather than assumed: `Shape` (`matt_math.h:121-204`) is doing two unrelated jobs, and the second one is what forces the shape of everything else.

The first job is the geometry interface the engine consumes: `bounding_box()`, `shape_type()`, `center()`, `offset()`, `inflate()`, `AABB_intersects()`. That is exactly and exhaustively what `collision/collision_object.h`, `collision/narrow_phase.cpp`, `collision/contacts.cpp` and `scene/scene.cpp` use. Six members.

The second job is a complete pairwise intersection table nailed to the vtable — seven pure-virtual `intersects` overloads at `matt_math.h:126-132`, plus two `dynamic_cast`-based dispatchers at `:133-134`. I confirmed those line numbers, and the forward-declaration block at `:17-26` that exists only so `Shape` can name every subclass. That second job produces the file's shape three times over: it forces the eleven-types-in-one-header violation, because every concrete shape must name every other at declaration time; it duplicates every predicate, since each of the five subclasses implements all seven overloads as one-line forwards to a free function that `matt_math.h:206-292` has declared all along; and it is very nearly dead. Every production call site of that 34-function table is `RectangleF` against `RectangleF`. `Shape::intersects(const Shape*)` and `(const Shape&)` have no caller anywhere, tests included. `inflate()` has none either — I grepped `.inflate(` across `engine/`, `samples/`, `bench/` and `tests/`, and the only non-test hit is the internal delegation at `matt_math.cpp:843`.

The header already argues this against itself, in the "NOT HERE: clone(), and edges()" note at `matt_math.h:185-203`, which I read in full. That note stopped one short. `intersects` is the third pure virtual with exactly the same problem, and it is 37 overrides rather than one.

**Verdict after challenge: act, with the challenge's corrections folded in.** The challenge did not dispute the diagnosis and I could not fault it either. What it disputed was execution detail, and on every disputed point I went to the code and the challenge was right:

- The audit's rewrite table for `intersects.cpp` claimed that `matt_math.cpp:301`, `:430` and `:620` "stay as `RectangleF::intersects(RectangleF)`". They do not. Line 301 is `circle.intersects(rect_rotated.bounding_box())` — a `Circle` receiver. Line 430 is `triangle.intersects(rect_rotated.bounding_box())`. Line 620 is `a.intersects(b.bounding_box())` with `a` a `const RectangleRotated&`. All three are members the surgery deletes; as written the file would not compile.
- The audit's enumeration of member call sites inside the free-intersect region missed `matt_math.cpp:456`, `rect_rotated.intersects(edge)`, inside `triangle_rectangle_rotated_intersect`.
- The audit said the file carries nine `#pragma region` markers. It carries thirteen. The argument is unaffected and slightly strengthened.
- `intersects.h` cannot forward-declare only: six of the declarations take `Point2F&` out-parameters and `Point2F` is a typedef of `Vector2F`, so the header must include `vector2f.h`.

**Target layout.** Fourteen source pairs in `engine/math/`, replacing two. `shape_type.h` already exists there as a header holding an enum and no class, which is the precedent for the two headers below that hold free functions.

| New pair | Holds | ~lines |
|---|---|---:|
| `scalar.{h,cpp}` | `PI`, `PI_OVER_2`, the 70-line `EPSILON` ordering note and the constant, `clamp` ×2, `are_equal(float)`, `to_radians`, `to_degrees`, `lerp` | 150 |
| `vector2f.{h,cpp}` | `Vector2F`, the `Point2F` typedef, eight free operators, `are_equal(Vector2F)` | 405 |
| `vector2i.{h,cpp}` | `Vector2I`, seven free operators | 150 |
| `rectanglei.{h,cpp}` | `RectangleI` | 215 |
| `shape.{h,cpp}` | `Shape`, minus the intersection table; the 46-line `inflate` contract and the NOT-HERE note stay | 110 |
| `rectanglef.{h,cpp}` | `RectangleF`, the `AABB` typedef | 390 |
| `circle.{h,cpp}` | `Circle` | 80 |
| `triangle.{h,cpp}` | `Triangle`, with `TriangleRightAxisAligned` riding along | 265 |
| `quad.{h,cpp}` | `Quad` | 300 |
| `segment.{h,cpp}` | `Segment` | 55 |
| `rectangle_rotated.{h,cpp}` | `RectangleRotated`, the `OBB` typedef | 515 |
| `inflate.{h,cpp}` | `mattmath::detail::inflate_convex_polygon` | 160 |
| `intersects.{h,cpp}` | the 33 surviving free predicates | 665 |
| `ericson_math.{h,cpp}` | unchanged, less one dead include | 698 |

Naming: a trailing type-tag letter is not a word and takes no underscore, so `vector2f.h` and `rectanglei.h`, but `rectangle_rotated.h` because "rotated" is a word.

I am keeping `inflate.h` rather than the challenge's suggested `convex_polygon.h`. The challenge is right that `inflate.h` reads as a verb, but a header called `convex_polygon.h` that contains no `ConvexPolygon` promises a type it does not have, which is the worse failure under the same rule. T3: the name that says exactly what is inside is the simpler one.

**The type surgery, stated once so nobody re-derives it per function.** `Shape` loses the seven pure virtuals and the two dispatchers; its vtable goes from twelve slots to five. Each of `RectangleF`, `Circle`, `Triangle`, `Quad` and `RectangleRotated` loses its seven overrides and their one-line bodies; `Segment` loses its two. Where a predicate exists as both a member and a free function, keep the free function — it has the body, the contract comment and the documented edge cases — and delete the member. Three exceptions, each because a production caller names the member: `RectangleF::intersects(const RectangleF&)` (`broad_phase.cpp:183,207`; `scene.cpp:73,229,238`; `bench/scene_bench.cpp:155`), which absorbs the body of `rectangles_intersect`; `RectangleF::contains(const RectangleF&)` (`camera_tools.cpp:66`); and `Shape::AABB_intersects` (`contacts.cpp:25`).

The audit wanted a fourth exception — keeping `RectangleRotated::contains(Point2F)` for its documented degenerate behaviour *and* keeping `point_rectangle_rotated_intersect`. That is two spellings of one predicate surviving a surgery whose whole point is to delete the second spelling, and the challenge is right to refuse it. **Ruling: delete the member, keep the free function, and move the 15-line rationale at `matt_math.cpp:598-612` with it intact.** One spelling per predicate, no exception that contradicts the rule.

The corrected rewrite table for the 26 member call sites inside `matt_math.cpp:108-658`:

- `:127`, `:195` — `RectangleF::intersects(RectangleF)`, survives unchanged
- `:133-135`, `:210-213` → `rectangle_point_intersect`
- `:141-144`, `:330`, `:402`, `:436-439`, `:509` → `triangle_point_intersect`
- `:201-204`, `:307`, `:445-447`, `:577`, `:626-637` → `point_rectangle_rotated_intersect`
- `:153`, `:222`, `:571` → `rectangle_segment_intersect`
- `:301` → `rectangle_circle_intersect(rect_rotated.bounding_box(), circle)`
- `:430` → `rectangle_triangle_intersect(rect_rotated.bounding_box(), triangle)`
- `:456` → `segment_rectangle_rotated_intersect(edge, rect_rotated)`
- `:620` → `rectangle_rotated_rectangle_intersect(b.bounding_box(), a)`

**CMakeLists edit.** `engine/math/CMakeLists.txt`, lines 7-10 are the whole `add_library` block; lines 1-5 (the comment) and 11-14 (include dirs, link) stay untouched. Replace with:

```cmake
add_library(MattMath STATIC
    circle.cpp
    ericson_math.cpp
    inflate.cpp
    intersects.cpp
    quad.cpp
    rectangle_rotated.cpp
    rectanglef.cpp
    rectanglei.cpp
    scalar.cpp
    segment.cpp
    shape.cpp
    triangle.cpp
    vector2f.cpp
    vector2i.cpp
)
```

Fourteen entries where there were two. `engine/CMakeLists.txt` does not change — line 1 is `add_subdirectory(math)` and no engine source is added, moved or renamed. `bench/CMakeLists.txt` and `samples/minimal/CMakeLists.txt` do not change.

**Include fallout: 26 files include `matt_math.h` directly** (27 `#include` lines, of which one is `matt_math.cpp`'s own header). Two of them name nothing from `mattmath` at all — `engine/render/colour.h:3` and `engine/audio/sound_bank.h:7` — and simply lose the line. I verified `colour.h`: its only mention of `mattmath` is in a comment at line 14. A further set of roughly 31 files name a `mattmath` type while including nothing, living off transitive includes.

There is no umbrella header. `matt_math.h` is deleted. PHILOSOPHY makes compile speed a maintained property and calls a build-time regression a defect; an umbrella preserves the exact defect — `sound_bank.h` would go on compiling eleven shape classes for nothing — and T3's simpler model is the one where a caller says what it uses.

**The cross-repository caveat is the one thing that can stop the work.** ColourWars consumes this repository as a submodule and cannot be read from here. If any ColourWars type derives from `mattmath::Shape` it implements all seven `intersects` overloads with `override`, and removing the base declarations is a hard compile error there, not a silent change. Before the surgery commit, grep the client for `: public mattmath::Shape`, `.intersects(` and `#include "engine/math/matt_math.h"`. This repository is clean — the only `: public Shape` sites are `matt_math.h:294, 605, 640, 709, 786`. If the client cannot be updated in the same submodule bump, keep `matt_math.h` for exactly one release as a sixteen-line umbrella carrying a comment saying it is scheduled for deletion and why.

**Risks.** Static-initialisation order widens by one translation unit: `Vector2F::ZERO` and the `DIRECTION_*` constants, `Vector2I::ZERO`, `RectangleF::ZERO` and `RectangleI::ZERO` are all dynamically initialised and today share one TU where declaration order is guaranteed. Nothing in the tree reads them from a namespace-scope initialiser, so the split does not create the hazard — but the 23-line note at `matt_math.cpp:1305-1327` must move to `vector2f.cpp` verbatim, because the engine has been bitten by this shape before. `inflate_convex_polygon` loses internal linkage when it is promoted to `mattmath::detail`; measured against zero production callers of `inflate()`, that costs nothing. Each new `.cpp` needs its own standard-library tier, which the audit omitted entirely: `<stdexcept>` for `triangle.cpp`, `quad.cpp` and `rectangle_rotated.cpp`; `<cmath>` for `scalar.cpp`, `vector2f.cpp` and `rectangle_rotated.cpp`; `<algorithm>` for `rectanglef.cpp` (`std::min`/`std::max` at `:963-976`, transitive today). And `quad.cpp` needs `ericson_math.h` for `strictly_opposite_sides`, declared at `ericson_math.h:112` and called at `matt_math.cpp:2092, 2099` — the audit's include list for that file was wrong.

`/fp:precise` is not disturbed: `EPSILON`'s value, every comparison, `Vector2F::operator==`'s exact test against `ZERO`, `RectangleRotated::edges_valid`'s relative tolerance and the NaN shape of `test_AABB_AABB` all move byte for byte. The one thing worth stating rather than assuming is that `mattmath::clamp`, called from `Vector2F::angle_between` at `matt_math.cpp:1246` to keep `acos` in domain, becomes a cross-TU call. `/fp:precise` forbids the reassociation that could make that differ, so run and diff `MattMathTests` on the surgery commit, before the filing commit, so any difference has one candidate cause.

`docs/review/rtcd/all-findings.md` cites `matt_math.cpp` and `matt_math.h` by line. CLAUDE.md marks `docs/review/` as historical and not updated as findings are fixed, so no amendment is owed — but one sentence in the commit message saying those citations now point at deleted files is cheaper than the confusion.

### 2. `tests/math/math_tests.cpp` — 69 test cases, one shared symbol, no structure

1863 lines and 69 `TEST_CASE`s, both verified. What decides this one is what the file does *not* contain: no fixture class, no anonymous namespace helper beyond one free predicate, no template, no friend, no shared mutable state. The only cross-cutting symbols are three float constants in `namespace MathTestConstants` at lines 13-18. Every case is self-contained. This is not a structure that would fragment if cut; it is a pile, and CONVENTIONS names this case in the same breath as the type-past-one-job case.

Two structural lies confirm the file has stopped organising anything. `namespace EricsonMathTests` opens at line 22 and closes at 788, and fifteen of the cases inside it test no Ericson function at all — `bounding_box_of` (`:233`), unary minus (`:267`), three `RectangleRotated` cases (`:281`, `:333`, `:404`), a `Quad` setter (`:311`), triangle angles and the hypotenuse (`:357`, `:383`), and `inflate` (`:432`). And `MattMathTests::EPSILON_F` at line 792 shadows `MathTestConstants::EPSILON_F` that the using-directive at line 20 already brought into scope — same value, harmless today, and a trap that exists only because the two declarations are 772 lines apart.

**Verdict after challenge: split, but take the challenge's fallback layout, not the audit's.** Three disputed facts, all of which I checked:

- The audit proposed pulling seven "oriented" rows out of the intersection table into `rectangle_rotated_tests.cpp`, on the grounds that they share a fixture and a tolerance regime. The challenge showed the tolerance argument is an artefact of the test data, not of the function under test — `math_tests.cpp:1196-1243` builds an axis-aligned rectangle first and uses the tight nudge, then rebuilds a diagonal one and only then needs the loose one. **Ruling: keep all 24 rows together.** I counted the `test_*_intersect` cases and there are exactly 24, running in `matt_math.h:206-292`'s own declaration order. That order is the one auditable property the file has, and it is what makes two absent rows visible: `circle_point_intersect` and `triangle_point_intersect` have zero calls anywhere in the file, which I confirmed by grep.
- The audit assigned `ericson_math_tests.cpp` "lines 22-789 verbatim" while simultaneously routing twelve cases from inside that range to six other files. Taken literally the split duplicates twelve test cases. The ranges below are by case, not by block, which removes the ambiguity.
- The audit's constant plan contradicted itself: it dissolved `MathTestConstants` while deferring the removal of the redundant `are_equal` third argument to a later commit, leaving `EPSILON_F_100` undeclared in two of the new files. `grep -c` returns 43 lines mentioning it — one declaration, 35 tolerance sites, seven nudge uses. **Ruling: drop the defaulted third argument in the motion commit.** `mattmath::EPSILON` is the literal `0.0001f` (`matt_math.h:100`) and is already the default of both `are_equal` overloads (`:112-114`), so it is bit-identical under `/fp:precise` and mechanically diffable. `EPSILON_F_2` has exactly two uses, at `:1175` and `:1526`, both inside the table — one file, not two.

**Target layout.** Nine test files plus the unchanged `test_main.cpp`, 68 cases (69 less the empty stub).

| New file | Cases, by opening line | ~lines |
|---|---|---:|
| `ericson_math_tests.cpp` | 26, 61, 84, 99, 129, 144, 161, 176, 185, 204, 492, 568, 602, 620, 642, 657, 679, 702, 731, 760 | 500 |
| `shape_intersect_tests.cpp` | 870, then the 24 rows 1036–1690 in header order | 750 |
| `rectangle_rotated_tests.cpp` | 281, 333, 404, 1734, 1770, 1834 | 180 |
| `inflate_tests.cpp` | 432, 928, 974 | 135 |
| `vector2_tests.cpp` | 267, 796, 809, 823, 838, 854, 903 | 130 |
| `triangle_tests.cpp` | 357, 383, 537 | 80 |
| `quad_tests.cpp` | 311, 1003 | 55 |
| `shape_tests.cpp` | 1791 | 45 |
| `rectanglef_tests.cpp` | 233 | 35 |

`TEST_CASE("test_test_segment_AABB")` at `:675-678` is deleted: the body is empty, it asserts nothing, and its subject is genuinely covered at `:591-600`. That is the one non-mechanical edit in a commit sold as motion, so say so in the message alongside the 69→68 count.

Two naming rulings. `shape_tests.cpp` holds the edges-ordering contract at `:1791`, which exercises four concrete shapes' `edges()` accessors — and `matt_math.h:185-203` records that `edges()` was deliberately taken *off* `Shape`. Open the file with one sentence saying its subject is the contracts every implementation states, so the mismatch reads as a decision rather than a filing error. `bounding_box_of` goes to `rectanglef_tests.cpp`, where it belongs, because it is a `RectangleF` static — the audit had it in `shape_tests.cpp` and the challenge caught it.

Every new file needs its own `namespace`/`TEST_SUITE` scaffolding plus the two file-scope using-directives at `:11` and `:20` — no inner line range compiles as a bare move. `namespace MathTestConstants`, `EricsonMathTests` and `MattMathTests` all disappear; all three are multi-word PascalCase, which CONVENTIONS ("Namespaces: one lowercase word") does not permit, and the newer files in the tree — `tests/render/colour_tests.cpp`, `camera_tests.cpp` — use `TEST_SUITE` with no wrapping namespace. That is the pattern to adopt.

**CMakeLists edit.** `tests/math/CMakeLists.txt`, replacing line 4. Line 1 (`find_package`), lines 7-11 (`target_link_libraries`) and line 13 (`add_test(NAME MattMathTests COMMAND MattMathTests)`) are untouched:

```cmake
add_executable(MattMathTests
    ericson_math_tests.cpp
    inflate_tests.cpp
    quad_tests.cpp
    rectangle_rotated_tests.cpp
    rectanglef_tests.cpp
    shape_intersect_tests.cpp
    shape_tests.cpp
    test_main.cpp
    triangle_tests.cpp
    vector2_tests.cpp
)
```

One executable in, one out; one `add_test` in, one out. The ctest count stays nine and `test_main.cpp` remains the only translation unit with a doctest main.

**Include fallout: zero.** Nothing in the tree includes `math_tests.cpp` and it declares nothing anyone else names. No shared test-helper header is created, deliberately: the only shared symbols are three floats, and a `tests/math/test_constants.h` collecting them is exactly the "consts namespace collecting strays" CONVENTIONS/Constants rules out, plus the speculative framework T1 declines. `tests/ui/stub_widget.h` is the tree's precedent for a genuine shared test helper — it is a type with behaviour, which is what earns a header.

Per-file includes follow `42f1f47`'s rule and list what each file names: `<cfloat>` only in `shape_intersect_tests.cpp` (`FLT_EPSILON` at `:1114-1120`); `<stdexcept>` in `rectangle_rotated_tests.cpp` and `quad_tests.cpp`; `<array>`, `<cfloat>`, `<cmath>`, `<limits>`, `<span>` in `ericson_math_tests.cpp`.

**Risks.** The nudge magnitudes are load-bearing and must not be collapsed: `:1175` and `:1526` need two ulps at 10.0f where the rest need one, and the diagonal rows need 1e-4. The `FLT_EPSILON` uses at `:52`, `:57`, `:1114-1120` move verbatim — `docs/review/round-2` has a live finding that `FLT_EPSILON` is smaller than one ulp at 10.0f and those boundary cases therefore do not straddle a boundary. That is a separate defect and must not be fixed inside a motion commit. Suite names change, which is cosmetic to the build (nothing filters on `--test-suite`) and material only to doctest's grouped output. All 69 case titles are distinct, so distributing them across ten translation units introduces no collision.

### 3. `engine/app/application.{h,cpp}` — the shell that is also a window

921 lines across the pair. Of the 678 in the `.cpp`, roughly 390 are Win32 windowing: `create_window` (122-185), the two frame-arithmetic overloads (187-213), the Win32 halves of `quit`/`set_resolution`/`set_fullscreen` (270-329), the `PeekMessage` pump inside `run` (254-267), and `window_proc` (518-677). The rest is what ARCHITECTURE assigns to the shell.

The verdict does not rest on length — 589 code lines across a unit is not a size crisis. It rests on PHILOSOPHY.md:298-299, which I read in the source: "Platform-specific code — rendering backend, input devices, audio backend, windowing — lives at the edge behind engine-owned interfaces." Three of those four have a seam in this tree. Windowing is the one item on that list with no seam at all, inlined into the type that also owns the services, the timer and the state stack. And the growth signal points at exactly that job: the most recent commit to touch the file, `0702776`, was net +81 lines and every one of them was windowing.

**Verdict after challenge: act, with corrections, and it needs a documentation amendment.** The challenge's refutations were substantive and I confirmed all of them:

- The audit said `WindowNotify` should be "modelled byte-for-byte on `DeviceNotify`" and that the handler bodies are "byte-identical". They cannot both be true. I read `application.h:213-219`: five of the seven handlers are const-qualified — `on_activated`, `on_deactivated`, `on_suspending`, `on_window_moved`, `on_display_change` — while `on_resuming` and `on_window_size_changed` are not. `DeviceNotify`'s virtuals are non-const. Copied byte-for-byte, `Application` cannot override them and stays abstract, and `samples/minimal/main.cpp:31` fails to construct it. **`WindowNotify` must declare the five const and the rest non-const, mirroring `application.h:213-219` exactly.**
- `window.cpp` needs `<tuple>`. `application.cpp:552`, inside `WM_PAINT`, is `std::ignore = BeginPaint(window, &paint);`, and `<tuple>` is included at line 11. The audit's include list for the new file was "its own header first, then `<stdexcept>`. Nothing else."
- The audit's cut rule — "everything that calls Win32 moves" — mis-predicts three of its own moves. `in_suspend` and `minimized` (`application.cpp:525-526`) call no Win32; they are boolean bookkeeping gating `gamepad_reader_->suspend()` and `audio_engine_->Suspend()`. **State the rule as "everything that knows a Win32 fact moves"**, and say in the commit message that those two flags move because they collapse two independent Windows suspend sources into one application-level signal.
- The audit declared `outer_size_for_client` the one headlessly testable thing in the unit and then put it in the private section. Make the pure static overload public, or the test paragraph is unactionable.

**Target layout.** Two new files, no type invented beyond the seam:

- `engine/app/window.h` (~135 lines) — `class WindowNotify` (eight pure virtuals, protected non-virtual destructor, const-qualification per above); `struct WindowOptions` (class name, title, client size, fullscreen, min width, min height, names copied verbatim from `ApplicationOptions` so no meaning shifts); `class Window` with `handle()`, `open()`, `exit_code()`, `dispatch_one()`, `close()`, `resize_client()`, `enter_fullscreen()`, `leave_fullscreen()`. Includes `engine/math/matt_math.h`, then `<Windows.h>`, then `<string>`. Never an `engine/render` header.
- `engine/app/window.cpp` (~345 lines) — the constructor absorbing `create_window`'s body, both `outer_size_for_client` overloads, `window_proc` with its twelve cases, the pump, and the Win32 halves of quit/resize/fullscreen. Includes its own header, then `<stdexcept>` and `<tuple>`.

`application.h` drops to ~230 and `application.cpp` to ~375. Every public declaration on `Application` is unchanged, so `samples/minimal` compiles unedited. The seven event handlers keep byte-identical bodies and become private `WindowNotify` overrides — a private override is still reachable through the base pointer.

**Where the cut runs is decided by a build rule, not by taste.** `application.cpp` reaches the D3D11 backend three times: `:234`, `:426` (`on_display_change` → `UpdateColorSpace`) and `:470`. `on_display_change` is a window-message handler. If it moved with the rest of the message translation, `window.cpp` would be a third file outside `render/d3d11/` including `backend.h`, which ARCHITECTURE forbids in as many words. All seven handlers therefore stay on `Application`. A reviewer should check that single fact first. Worth recording as a follow-up: a `Renderer::display_changed()` on the seam would delete the constraint entirely and make the handler placement a free choice.

**CMakeLists edit.** One line in `engine/CMakeLists.txt`, inserted after line 13 (`    app/application.cpp`) and before line 14 (`    assets/asset_manifest_loader.cpp`), preserving alphabetical order and the four-space indent:

```
    app/window.cpp
```

Nothing else. No test target, no benchmark, no sample source.

**Include fallout: one file.** `engine/app/application.h` gains `#include "engine/app/window.h"` at the head of its engine group. `application.h` is included by exactly four files — `application.cpp:1`, `samples/minimal/main.cpp:10`, `samples/minimal/states/hello_state.h:3`, `samples/minimal/states/confirm_state.h:3` — and none of them names anything that moves. Callers requiring an edit: zero.

**This step requires an ARCHITECTURE amendment in the same commit.** `docs/design/ARCHITECTURE.md:161-164` reads "Platform-specific code lives only in the backend subfolders (`render/d3d11/`, `input/xinput/`), behind engine-owned interfaces." A Win32 `Window` in `engine/app/` fights that sentence literally. Two honest resolutions: put the pair in `engine/app/win32/` with a platform-neutral declaration left behind, which is the letter of the rule and also the speculative framework T1 rules out given that a second platform is explicitly not a current work item; or amend line 162 to name the shell's window as the third case, on the ground that `app` is already the module allowed to depend on everything and PHILOSOPHY already lists windowing as platform code at the edge. I recommend the amendment and `engine/app/window.{h,cpp}`, because the escalation is cheap when it becomes real — the class keeps its name and every call site, and the pair moves down a folder. Whichever way it goes, `docs/design/` changes by amendment in the same commit as the change that fights it, and this is that change.

**Risks.** `notify_` must be assigned in the constructor's member-init list, before `CreateWindowExW`: `WM_CREATE` and the `WM_SIZE` that `ShowWindow` fires at `:183` both arrive inside the constructor, and the comment at `:172-182` makes that `WM_SIZE` load-bearing — it corrects the resolution manager to the client size the window really got before `create_device` reads it. A setter-based design silently reintroduces the fullscreen-at-launch bug. The three message flags need in-class default initialisers and must be declared before the HWND, because the creation-time `WM_SIZE` reads them; as function-local statics they are zero-initialised before `main` and this safety is currently free. `~Window` must not call `DestroyWindow` — by the time `~Application` runs, `WM_QUIT` has arrived and `WM_QUIT` came from `PostQuitMessage` inside `WM_DESTROY` at `:663-665`, so the handle is already gone. `dispatch_one()` must peek one message and tick only on an empty queue; draining the queue first is a defensible loop and a different one, and changing it inside a refactor is how a frame-pacing regression gets attributed to the wrong commit.

Nothing here touches floating point, so `/fp:precise` has zero exposure. No test changes: nothing constructs an `Application` today, `window_proc` has never been exercised by ctest, and there is no `tests/app` folder — adding one would make a tenth ctest entry, which the constraints forbid. That gap should be recorded in the commit rather than papered over, and verification is manual: launch `out/build/x64-debug/samples/minimal/ArtAttackSample.exe`, then exercise windowed launch at the requested client size, an edge drag, alt-tab, minimise/restore, and quit through `ConfirmState`.

### 4. `engine/render/d3d11/device_resources.{h,cpp}` — 95% vendor, 5% engine, in two dialects

779 lines in the `.cpp`, of which roughly 740 are Microsoft's. `NOTICE:13-28` records the pair as adopted from the DirectX Tool Kit samples and states the goal: "They remain recognisably Microsoft's." That is T9 working as designed, and the vendor body is a keep on the merits — it is one cyclic state machine with no acyclic cut anywhere in it. `CreateWindowSizeDependentResources` calls `HandleDeviceLost` at `:310` and `HandleDeviceLost` calls it back at `:473`; `Present` calls `HandleDeviceLost` at `:519`; `UpdateColorSpace` is called from three of those and calls `CreateFactory`, which `CreateDeviceResources` also calls. Every one reads and writes the same five members. Any cut yields a file defining member functions of a class it does not declare.

The defect is the last page. `DeviceResources` owns a pool of deferred contexts that exist for exactly one reason: to give each of `Renderer::Impl`'s views its own recording context. That is the renderer's per-view unit of work, not device lifetime. I verified the consequences: `deferred_context_count_` (`device_resources.h:149`) is a second copy of `Renderer::Impl::views.size()`; three members hold one list, one of them a heap-allocated `unique_ptr<vector<raw pointer>>` mirroring a `vector<ComPtr>`; and `deferred_contexts()` (`device_resources.h:102`, `cpp:763-766`) has zero callers anywhere — grep across `engine/`, `samples/`, `tests/` and `bench/` returns only `deferred_context(int)` at `renderer.cpp:231` and `create_deferred_contexts` at `renderer.cpp:286`.

The load-bearing ordering is also unwritten and cross-class: `HandleDeviceLost` must call `create_deferred_contexts` at `:468-471` after `CreateDeviceResources` and before `m_deviceNotify->OnDeviceRestored()` at `:475-478`, because `Renderer::Impl::create_device_dependent_resources` reads `deferred_context(i)`. PHILOSOPHY, Services and lifetimes: "If an ordering is load-bearing, it is either designed away or stated where it lives." It is neither.

**Verdict after challenge: act on the pool, and drop the pragma deletion.** The surgery is not "split `device_resources.cpp`" — it is that `DeviceResources` sheds the pool and `DrawList::View`, which already holds a borrowed `ID3D11DeviceContext*` next to the `SpriteBatch` that writes into it, takes ownership of its own context. Six members and three methods collapse into one member changing type from `ID3D11DeviceContext*` to `Microsoft::WRL::ComPtr<ID3D11DeviceContext>`. The rebuild ordering becomes two adjacent statements inside one loop in `Renderer::Impl::create_device_dependent_resources`, which is PHILOSOPHY's "designed away" rather than "stated where it lives" — the better of the two options it offers.

The audit also wanted to delete `device_resources.cpp:28`, `#pragma warning(disable : 4061)`, as inert vendor residue that makes the zero-suppressions promise "literally true". **Ruling: do not.** I read lines 20-30: line 28 sits immediately below `#ifdef __clang__` / `#pragma clang diagnostic ignored "-Wcovered-switch-default"` / `"-Wswitch-enum"`. It is the MSVC half of a vendor preamble pair, covering the same two switches. Deleting it manufactures a new undeclared divergence from upstream inside the one commit whose thesis is restoring a clean vendor file. Worse, the audit's stated fallback — "the honest answer is a `default:` label on the switch at `cpp:57-63`" — is a no-op: I read line 62 and that switch already has `default: return fmt;`, and so does the one at `:723-724`. C4061 fires in the presence of a `default:`; that is what distinguishes it from C4062. And the promise is already literally true: `cmake/settings.cmake` carries no `/wd` flag, and `docs/review/round-2/README.md:139` frames it as the settings target having zero suppressions.

**Target layout.** No new file. Four existing files change:

- `engine/render/d3d11/backend.h` — one member of `DrawList::View` changes type. `<wrl/client.h>` is already included at line 11.
- `engine/render/d3d11/renderer.cpp` — `create_device_dependent_resources` absorbs the creation loop; `OnDeviceLost` resets the ComPtr; `create_device:286` loses the `create_deferred_contexts` call. Lines 111, 125, 332 and 333 compile unchanged through `ComPtr::operator->`.
- `engine/render/d3d11/device_resources.h` — delete lines 97-104 (three declarations) and 144-149 (three members), and `<memory>`/`<vector>`, which have no other user.
- `engine/render/d3d11/device_resources.cpp` — delete 742-778, and the two weld blocks at 444-451 and 468-471 inside `HandleDeviceLost`. Replace `<memory>` and `<vector>` with `<algorithm>` **and `<iterator>`**: the audit spotted `std::max`/`std::min` at `:70` and `:286-287` but missed `std::size`, which I found at `:174`, `:257` and `:552`. Both are hygiene rather than hazard — `<stdexcept>` and `<system_error>` survive the edit and supply MSVC's `<xutility>` — so this should not be sold as the top risk.

Result: `device_resources.cpp` at ~730 lines and `device_resources.h` at ~137, both the DXTK original plus `namespace artattack` and the `D3DDeviceNotify` naming comment.

**CMakeLists edits: none.** No `.cpp` is created, renamed or deleted; `render/d3d11/device_resources.cpp` stays at `engine/CMakeLists.txt:39` and `render/d3d11/renderer.cpp` at `:41`.

**Include fallout: none outside the four files above.** `device_resources.h` is included by exactly one file in the repository, `engine/render/d3d11/backend.h:4`, plus its own `.cpp`. `backend.h` itself has **four** includers, not the three the audit stated — `engine/app/application.cpp:5`, `engine/assets/resource_loader.cpp:9`, `engine/render/d3d11/renderer.cpp:1` and `engine/render/d3d11/render_resources.cpp:1`. The fourth is inside the folder, so the two-file rule is untouched, but the number the audit rested its ODR argument on was wrong.

**One rationale claim must be softened.** The audit says the change makes NOTICE's fidelity claim "checkable by diff against upstream". It does not: `device_resources.cpp:7` includes `engine/core/throw_if_failed.h` in place of DXTK's own, and `device_resources.h:21` declares `D3DDeviceNotify`, renamed from upstream's `IDeviceNotify` — a rename the audit deliberately keeps. NOTICE:26-28 records only the folder move and the namespace. Claim instead that the file "remains recognisably Microsoft's, with the last engine-authored code removed", and amend NOTICE in the same commit to record the rename, the `throw_if_failed` re-home, and the departure of the pool. `docs/review/round-2/README.md:1000-1002` identifies `engine/core/throw_if_failed.h` as a third relocated Microsoft file that NOTICE also omits.

**Risks.** Context release moves from unconditional to conditional on a registered notify: `HandleDeviceLost` clears the pool at `:447` today whatever happens, and after the change the release lives in `Renderer::Impl::OnDeviceLost()`, which runs only if `m_deviceNotify != nullptr`. In this engine it is never null. Use `ReleaseAndGetAddressOf()` and say why in the code, not only in a risk list: `CreateWindowSizeDependentResources:310` can re-enter `HandleDeviceLost`, whose inner `OnDeviceRestored` already ran `create_device_dependent_resources()`, so the outer one runs it a second time over Views that already hold a live context. `GetAddressOf()` there leaks one context per view per double-restore. The exception type changes — `std::runtime_error("Failed to create deferred context.")` becomes `com_exception` from `ThrowIfFailed`, whose `what()` yields only a hex HRESULT — and `samples/minimal/main.cpp:41` still catches it, but the commit should own the change.

The device-lost path is being simplified without being executed. `docs/review/round-2/README.md:141` already observed of this code that it "has clearly never been run", and no null backend exists to test it headlessly. The simplification makes it more likely to be right; that is reasoning, not evidence, and the commit should say so.

### 5. `engine/render/colour.{h,cpp}` — keep, with a one-line dead-include fix

791 lines, of which 444 are one entry per CSS colour spelled three times because C++ requires it: 148 in-class declarations at `colour.h:97-244`, 148 out-of-class definitions at `colour.cpp:205-352`, and 148 name-table rows at `colour.cpp:368-515`. That is 58% of the unit as a table of constants, which is the case CONVENTIONS names by hand when it refuses a line count. No type has grown past one job — `Colour` is an RGBA value with clamping arithmetic, and the palette is 148 instances of it, not a second job. The growth signal is inverted: two commits ever, the second net −10 lines, over a W3C list that last changed in 2014.

The one clean cut — palette definitions and name table into a `colour_palette.cpp` — cannot touch the half a reader actually reads, because static data members must be declared inside the class. It would also produce a file holding no type, which breaks the naming rule it is meant to serve, and it separates three lists that must stay in lockstep where only two of the three mistakes are caught by a compiler. **Keep.**

One real defect, and it is a one-line delete each. `colour.h:3` includes `matt_math.h` and `colour.cpp:7` says `using namespace mattmath;`, and I confirmed by grep that the only remaining references to `mattmath` in the pair are inside prose comments at `colour.h:14` and the using-directive itself. `42f1f47` deleted the last real use — `operator=(const mattmath::Vector4F&)` — and its stated purpose was "stop leaning on the math header for the standard library"; it swept 60 files and missed the two lines in the file it had just emptied of math. This matters more than a usual dead include because `colour.h` is included by fourteen engine headers including `renderer.h`, so the dead line re-propagates the 892-line math header through nearly everything that touches a colour.

**The two lines must go together.** Deleting `colour.h:3` alone is a hard error: `colour.cpp:7`'s using-directive names a namespace that has not been declared. Both, one commit, then build — some of the fourteen dependent headers will need their own `#include`, and the number can only be established by compiling. This becomes free after the math split, since `colour.h` would drop the line anyway; fold it into that step.

### 6. `tests/core/state_context_tests.cpp` — keep

551 lines, 17 test cases. Long because `StateContext` has a genuinely wide behavioural surface — ten public entry points crossed with two orthogonal axes the header spends 80 lines explaining — not because unrelated things cohabit. No shared mutable fixture, no ordering dependency between cases, no helper calling another helper; every case constructs its own context and log in its first three lines, so the working set for any one case is about 100 lines.

The three candidate responsibilities inside `StateContext` are welded by one invariant living in one bool: `deferring_` is set by `update()` at `state_context.cpp:20`, re-set by the drain at `:187`, consulted by `queue()` at `:176`, and reset by `clear()` at `:102`. Lifting a `PendingQueue` out means that flag lives in two places — the speculative framework T1 rules out, bought with the indirection T3 says not to buy. The newest section, `clear()`, is the intersection of the other three rather than a stranger, so growth here has increased cohesion.

The growth clause is live and should be discharged with a tripwire rather than a pre-emptive cut: when the file next gains a section (roughly 650 lines), re-ask whether `StateContext` has by then grown a second job. If it has, split the type and the new type names the second test file for you — the only cut that leaves both names truthful. If it has not, the file is still one subject.

Two things surfaced while reading, both out of scope: `StateContext::push(std::unique_ptr<State>, std::function<void()>)` (`state_context.h:100`) has no test anywhere, and `docs/labrador-filing-list.md` still says "all 13 cases" against today's 17.

### 7. `engine/math/ericson_math.{h,cpp}` — keep, one dead include

699 lines, of which 318 are code — 44 in the header and 274 in the implementation. More than 43% of the unit is prose, and that prose is PHILOSOPHY/Collision being obeyed: "Math primitives carry documented contracts — edge ordering, winding, zero-length behaviour, what throws." The history runs the wrong way for a split: the `.cpp` has gone 787 → 474 lines, losing 40%, every drop a deletion.

The four groupings anyone would reach for are not independent. Four of the six intersection tests are two-line forwarders onto the closest-point group or the orientation group; `test_point_triangle` is two lines onto `point_in_convex_polygon`. Thirteen internal cross-references run across the candidate boundaries, and the most valuable single argument in the file — state a test in accepting form so a NaN input intersects nothing — is written once at `test_AABB_AABB` (`cpp:31-42`) and cited from `cpp:245`, `cpp:329` and `h:163`, two of which say "at the top of this file" verbatim. Three callers include this header; a four-header taxonomy serving three callers is T1's speculative framework.

One real fix: `ericson_math.h:8` includes `<cmath>` and the header uses nothing from it — I checked, the only `std::` names in the header are `std::span` at `:137` and `:165`, and the sole `std::abs` is in the implementation, which includes `<cmath>` itself. Fold the deletion into whatever commit next touches the file.

Recorded, not proposed: the file is named for an author rather than a type, which CONVENTIONS' naming rule does not sanction. `NOTICE:32` names both paths as the attribution for the book, so a rename is a NOTICE amendment. The organising principle really is provenance, and a neutral name would lose the fact a reader most needs on opening it. If this is ever changed it should be by amending CONVENTIONS to admit provenance as the one legitimate alternative to a type name.

### 8. `engine/render/renderer.h` + `engine/render/d3d11/renderer.cpp` — keep

763 lines, and small by content. The header is 333 lines carrying 94 lines of declaration; the rest is 203 lines of comment, five blocks of which are marked CONSTRAINT and each records a specific bug the shape makes inexpressible. The `.cpp` is 430 lines carrying 321 of code across six banner-marked sections, the longest function 27 lines with no branch deeper than one level.

The growth test answers itself: `renderer.cpp` was born complete at 394 lines in `a201fd0`, took +44/−8 when the type conversions arrived from MattMath as part of making that library depend on nothing, and +1/−1 since. One section gained in its life, by deliberate architectural transfer.

The cleanest available cut — `DrawList` from `Renderer` at `renderer.cpp:197` — is clean on linkage and wrong on the object graph. The sprite batch's lifecycle is created in `Impl::create_device_dependent_resources`, opened in `View::open_batch`, closed in `close_batch`, finished in `finish`, destroyed in `Impl::OnDeviceLost` and rebound in `Renderer::begin_frame`; a cut at 197 puts three of those on one side and three on the other. It would also break the header-to-implementation pairing `d3d11/` currently keeps exactly, and contradict `renderer.h:329-333`, which states that a second backend fills three translation units, not one.

Two riders, neither structural. `Renderer::set_marker` (`renderer.h:306`) has zero callers in this tree; it is public engine surface a private consumer may hold, and the tree-wide dead-surface sweep in `42f1f47` already declined to take it — it belongs in a dead-surface review. And `DrawList::draw_sprite` dereferences `view_->owner->resources->impl()` per sprite at `renderer.cpp:162` and `:186`, three pointer loads ahead of the registry lookup, on the one path T8 is written about; caching `RenderResources::Impl*` on `Renderer::Impl` would remove two of them. PHILOSOPHY says optimisation follows a profile, so it is reported and not acted on.

## Order of work

Six commits, in this order. Steps 1 and 2 must precede 3 and 4; steps 5 and 6 are independent of everything and of each other.

1. **Split `tests/math/math_tests.cpp` into nine files.** Judgement in the assignment of cases, mechanical thereafter. Large: 1863 lines redistributed, one CMakeLists edit, one deleted test case, 35 `are_equal` third arguments dropped. Depends on nothing. Do it first, contradicting `docs/review/round-2`'s "as `matt_math.h` itself is split": a test `.cpp` has no ODR, no include fan-in and no forward declarations, so the test split is free and independent, whereas the header split is expensive. Doing tests first gives the source split a per-subject harness that can be verified file by file, which is what that review's own companion finding asks for.

2. **The `matt_math` type surgery.** Three files touched — `matt_math.h`, `matt_math.cpp`, and whichever of the nine new test files hold the affected cases — net roughly −300 lines. No file moves. Judgement: the 26-site rewrite table above must be applied exactly, and the `RectangleRotated::contains` ruling honoured. Medium size, high care. **Gated on the ColourWars grep.** Run `MattMathTests` and diff the output against the pre-surgery run here, so that if `/fp:precise` behaviour differs there is one candidate cause.

3. **The `matt_math` filing.** Two files become fourteen pairs; `matt_math.{h,cpp}` are deleted. No behaviour change at all — every hunk is a move. Large but mechanical once step 2 has landed, with two judgement points: the standard-library tier for each new `.cpp`, and the include sweep across 26 direct includers. Touches `engine/math/CMakeLists.txt` only. Fold in the `colour.h:3` / `colour.cpp:7` deletion here, since `colour.h` loses the line anyway.

4. **Optional, gated on the client grep: delete what nothing calls.** `to_radians`, `to_degrees`, `clamp(int)`, `TriangleRightAxisAligned` (one caller, `math_tests.cpp:389`), and the pairwise predicates with no client — which would remove `intersects.{h,cpp}` and roughly 670 lines of tests outright. Judgement, and it cannot be decided from this repository. A deletion is much easier to argue about when the thing to delete is one file rather than a diagonal slice of two, which is a large part of the reason to file them together first.

5. **`DeviceResources` sheds the deferred-context pool.** Four existing files edited, no file created, no CMakeLists change. Small and almost entirely mechanical; the judgement is confined to `ReleaseAndGetAddressOf` and the `HandleDeviceLost` ordering. Amend NOTICE in the same commit. Independent of steps 1-4 — it touches no math header.

6. **`Application` sheds the platform into `engine/app/window.{h,cpp}`.** One CMakeLists line, one include added, zero callers edited. Medium size and substantially judgement: the const-qualification of `WindowNotify`, the construction-order contract, and the `on_display_change` placement forced by the backend-include rule. **Requires an ARCHITECTURE amendment in the same commit** (`ARCHITECTURE.md:161-164`). Independent of everything else.

On the documentation obligations across the whole plan: any split touching a public primitive ships with its behavioural tests in the same commit. Step 2 creates no primitive — `inflate_convex_polygon` is existing code with existing coverage, promoted from an anonymous namespace to `mattmath::detail` — so nothing new is owed there, and step 1 is itself the tests. Step 6 creates `Window`, which is not a public primitive in that sense: no client names it, it cannot be constructed without a real `HINSTANCE` and a message pump, and there is no `tests/app` folder because a tenth ctest entry is forbidden. The one headlessly testable piece is the static `outer_size_for_client`, which is why it must be public; if it is ever pinned, `tests/render/` can host it at the cost of one line in `tests/render/CMakeLists.txt` and no new ctest entry.

`docs/design/` changes by amendment in the same commit as a change that fights it. Step 6 needs one, as above. **Steps 1-5 need none.** ARCHITECTURE treats `math` as a folder, not a file — `ARCHITECTURE.md:189` says "`math` | nothing — and it links nothing", and `:114-115` describes the folder, neither of which fourteen sources contradicts. CONVENTIONS' "One primary type per header" governs headers that hold types; `scalar.h`, `intersects.h` and `inflate.h` hold none, and `engine/math/shape_type.h` already exists in that folder as a header holding an enum and no class. If a reviewer reads the rule as "exactly one", the amendment belongs in step 3's commit and should say that a header may instead hold a family of free functions named for what they compute, with `shape_type.h` and `ericson_math.h` as the existing precedents.

## Verification

Every step:

```
cmake --build --preset x64-debug
ctest --preset x64-debug
```

Nine entries must pass, by name: `MattMathTests`, `CoreTests`, `CollisionTests`, `SceneTests`, `RenderTests`, `InputTests`, `UiTests`, `AssetsTests`, `Benchmarks`. I verified there are exactly nine `add_test` calls in the tree and that `CMakeLists.txt:38-45` adds the eight test subdirectories plus `bench`. Run `x64-release` too before steps 2 and 3 land: `/W4 /WX` behaviour differs between configurations and a warning that only fires in release is still a build failure.

**The trap that makes a broken split look green.** Sources are enumerated explicitly and nothing globs. A `.cpp` that nobody lists is not compiled, and the tests still pass — for a test source, because its cases simply do not exist, and doctest reports success on the ones that do. Two checks catch it, and both should be run rather than assumed:

- **Case and assertion count.** `MattMathTests` reports 69 test cases and 404 assertions today. After step 1 it must report 68 (the empty stub at `:675` is deleted) with the assertion count unchanged at 404 — the deleted case contains no assertion, which is why it is deletable. Run the executable directly and read the summary line — `ctest` alone does not show it. If a file went unlisted, the case count drops silently below 68 and everything is still green.
- **Count the sources against the CMakeLists.** After step 1, `tests/math/` holds ten `.cpp` files and `tests/math/CMakeLists.txt` lists ten. After step 3, `engine/math/` holds fourteen `.cpp` files and `engine/math/CMakeLists.txt` lists fourteen. A `git status` showing an untracked `.cpp` in either folder is the same failure wearing a different hat.

For engine sources the failure is louder but not automatic: an unlisted `engine/math/*.cpp` produces a link error in `MattMathTests` for the symbols it should have defined — unless every symbol in it happens to be unreferenced, which after step 3 is true of nothing except possibly parts of `intersects.cpp`. Do not rely on the linker alone; count the files.

Step-specific:

- **Step 1**: diff each moved case body mechanically against the original. The commit is sold as motion plus 35 mechanical argument drops, so a reviewer must be able to confirm nothing else changed. Nothing in the tree filters on `--test-suite`, so the suite renames are cosmetic to the build.
- **Step 2**: `MattMathTests` output diffed against the pre-surgery run, before step 3, so a `/fp:precise` difference has one candidate cause. The `Vector2F::operator==` exact test against `ZERO` and the `signed_area` exact-equality assertions at `math_tests.cpp:70, 71, 77, 88, 96, 126, 141` are the sensitive ones.
- **Step 3**: every one of the fourteen new headers must compile standalone, which own-header-first proves in its `.cpp`. `intersects.h` and `inflate.h` are declaration-only and their own-header-first include proves almost nothing; put a one-line comment in each saying so, or a reader goes hunting for the missing include. Also confirm no `#include "engine/math/matt_math.h"` survives anywhere: `grep -rn 'matt_math.h' engine/ tests/ bench/ samples/` must return nothing.
- **Step 5**: `cmake/check_engine_includes.cmake` runs on every build and greps for game includes; it is unaffected. What it does *not* check is the two-file backend rule, so confirm by hand that `grep -rn 'd3d11/backend.h'` still returns exactly four files and that only two of them are outside `engine/render/d3d11/`. Then run `ArtAttackSample.exe` — if the contexts are not created, `begin_frame`'s `view->context->OMSetRenderTargets` at `renderer.cpp:332` null-dereferences on frame one.
- **Step 6**: manual, and named in the commit as manual. Launch `out/build/x64-debug/samples/minimal/ArtAttackSample.exe` and exercise the five paths the split touches: windowed launch at the requested client size, an edge drag (`WM_ENTERSIZEMOVE` / `WM_PAINT` tick / `WM_EXITSIZEMOVE`), alt-tab away and back, minimise and restore, and quit through `ConfirmState`.

## What this plan deliberately does not do

- **Split `device_resources.cpp`.** The vendor body has no acyclic cut; every candidate yields a translation unit defining member functions of a class it does not declare, and every cut costs the ability to diff against upstream.
- **Delete `device_resources.cpp:28`'s `#pragma warning(disable : 4061)`.** It is the MSVC half of a vendor preamble pair, both switches it covers already carry `default:` labels, and `cmake/settings.cmake` carries no suppression — the promise is already true.
- **Translate the vendor naming or delete the twelve dead `Get*` accessors** in `device_resources.h`. T9 and NOTICE say the file is Microsoft's; a rename sweep trades a diffable vendor file for a permanently forked one, and the dead accessors are a separate cosmetic commit with the same fidelity cost and none of this one's benefit.
- **Pull the seven oriented rows out of the intersection table** into `rectangle_rotated_tests.cpp`. Two of the three arguments for that seam are artefacts of the test data rather than of the functions under test, and the extraction destroys the header-declaration ordering that makes the two absent rows visible.
- **Keep a permanent `matt_math.h` umbrella.** It preserves the exact defect — `sound_bank.h` and `colour.h` would go on compiling eleven shape classes they name nothing from — forfeits the compile-time property PHILOSOPHY maintains, and hides the split from every future reader.
- **Split `engine/math/ericson_math.h` four ways.** Three callers, one of which uses a single function; the taxonomy is T1's speculative framework, and thirteen cross-references run across the candidate boundaries.
- **Split `engine/render/colour.cpp`'s palette into a second translation unit.** It cannot touch the header half a stranger actually reads, it would produce a file holding no type, and it separates three lists that must stay in lockstep where only two of the three mistakes produce a diagnostic.
- **Split `tests/core/state_context_tests.cpp`.** Seventeen cases against one type with one deferral flag; the newest section is the intersection of the other three, so growth has increased cohesion rather than accumulated strangers.
- **Split `renderer.h` or `renderer.cpp`.** 94 lines of declaration under 203 of recorded rationale, and a `DrawList`/`Renderer` cut would fragment the sprite batch's lifecycle at three points, break the folder's header-to-implementation pairing, and contradict a decision already written into the header.
- **Extract a `render_handles.h`** from `renderer.h:50-91`. Two of seven includers want only that block, but the file would hold no type, buy no compile time, and change no dependency direction.
- **Extract a `DeferredContextPool` type.** It relocates the pool concept instead of deleting it, keeps the count alive under another name, and creates a class whose whole body is one loop for one caller fifteen lines away. T3.
- **Fix the `FLT_EPSILON` boundary cases**, the two assertion-less blocks at `math_tests.cpp:1118-1120` and `:1682-1687`, or the missing `circle_point_intersect` and `triangle_point_intersect` rows. All are real coverage defects, all land in small files where the omission becomes obvious, and none belongs inside a motion commit.
- **Address `application.h` dragging `<Audio.h>` and `<d3d11_1.h>`** into every client TU through `resource_loader.h:6-7`. Real, filed, and a separate unit of work.
- **Resolve `RectangleF`'s vptr.** It derives from `Shape`, so it carries a vptr while being the value type `Camera`, `Viewport`, `Widget`, `SpriteSheet` and `Scene` hold by value. The alternative is a collider type wrapping a plain rectangle so that polymorphism is paid for only where `CollisionObject::shape()` needs it. That is a design change with real call-site consequences, not a filing question, and T3 says leave it until something forces it.