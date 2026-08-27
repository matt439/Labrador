# The survey's state — what has landed, and what it returned

**The companion to [next.md](next.md), and the only file of the two that is
edited.** That document opens by saying it is the survey as written, that it is
not updated as items land, and that its line numbers are the ones that were
true at `b4bcda0`. That rule is worth keeping — a survey rewritten as it is
worked through stops being a record of what somebody actually found — but it
leaves a reader with no way to tell an open item from one that was finished
last week, which is a different failure from stale line numbers and a worse
one. This file is that half.

Two rules for it:

- **A row per item, and the commit that closed it.** Nothing is deleted when it
  lands; an item that is done reads as done rather than disappearing, because
  the argument for doing it is still in `next.md` and somebody will want to know
  whether it was taken.
- **What an item returned lives here, not in `next.md`.** Three of the survey's
  items exist to produce a number or settle a question. Those answers are the
  product, and a commit message is not where anybody looks for them.

Written against `f5bd513`.

---

## The table

| Item | State | Commit |
|---|---|---|
| **2.1** The parallel path has never executed | **discharged** | `90ce3d0` |
| **2.2** `samples/minimal` cannot be quit without a controller | **fixed** | `d3de8f6` |
| **3.1a** Render bench, engine arithmetic | **landed** | `a9b806c` |
| **3.1b** `Scene::draw` fan-out under null | **landed** | `90ce3d0` |
| **3.2** The LineSweeper particle field | **landed** | `f5bd513` |
| **3.3** `engine/ui/` has no client, and no `Direction` producer | open | |
| **3.4a** `tests/audio/` — the cheap evidence | open | |
| **3.4b** The audio seam | open, and still blocked on `.xwb` | |
| **3.5** Sprite sheets discard `rotated` | open — independent of everything | |
| **5** The InputMap refusal | **measured, and inconclusive** | `d3de8f6` |
| **6** The four decisions `next.md` does not make | one made — the third; three still unmade | `f411e24` |
| **7** The nine drifted claims | **all nine fixed**, and three more found | `dea5fe0` |

§4's spine is finished down to its branch point and **3.2, the item it
pointed at, has landed**. What is left of §3 is two independent items and one
pair: **3.3** carries the sharper of its two findings, a header stating a false
fact about another module, and **3.5** is independent of everything and costs
days; **3.4a** sits one step behind 3.3 and **3.4b** behind that, still blocked
on `.xwb`.

---

## What the landed items returned

### 3.1a — the draw path's arithmetic

**35.4 ns a sprite**, for all four corners, x64-release on this desktop — and
flat, 35.4 at 1,024 sprites and 35.4 at 65,536, with the vertex buffer growing
from 128 KB to 8 MB underneath it. The phase does not become memory bound at
any count this engine will see.

That is the floor §3.2 and §5's bulk-submission refusal were both waiting on.
It is **not** a frame time: no `Renderer` is constructed, so the buffer map, the
upload and the draw call are outside it. It also prices the call boundaries
either side of the arithmetic, because both entry points and the `Vector2F`
operators they use are in other translation units and this build turns on no
cross-module optimisation — honest, because a frame reaches
`build_sprite_quad` across exactly those boundaries, but it makes the figure a
property of how the tree is linked as well as of what the function computes.

**The rotation branch, priced.** `sprite_geometry.cpp` skips a sine and a
cosine at an angle of zero and calls the saving "a real cost in a frame that
draws thousands" — its one claim about its own speed, and it had no number
behind it. It comes to **1.07× to 1.08×**, about three nanoseconds a sprite,
holding at every count. The claim survives and the branch is free, so nothing
changed; the sentence now points at the benchmark that measures it.

### 3.1b / 2.1 — the fan-out

**It runs.** Four views on four distinct threads, all off the calling thread —
which is the whole of §2.1's "has it ever executed", answered. It is reported
rather than asserted on, because `tests/core/thread_pool_tests.cpp` already set
that standard: a pool with a maximum of four threads is allowed to run four
tasks on one thread.

**And it is a net loss at the scale this tree draws.** Cutting the view list,
submitting a task per range and waiting costs the same at sixty objects as at
sixty thousand, so the fan-out starts behind and catches up. Release, null
backend, four panes over one arena:

| objects | fan-out against one thread |
|---|---|
| 64 | **0.45×** |
| 256 | 1.08× |
| 1,024 | 2.06× |
| 4,096 | 2.77× |

The crossing is around **250 objects**, and LineSweeper's well is 200 cells.
Debug reads friendlier — 0.96×, 1.51×, 3.32×, 3.86× — because an unoptimised
per-view body is more work for the workers to divide, and debug is what ctest
measures: `CMakePresets.json` has no release preset for the null backend, and
the release figures above came from a hand-configured build.

That is the evidence §6's third decision asked for and **it is deliberately not
the decision**. See below.

### 5 — the InputMap threshold

§5 refuses to argue the action-mapping question without a measurement and warns
that a threshold chosen afterwards can only agree with whoever ran it. So it
was named before a line was written: **under 25 lines confirms the refusal, 60+
or an engine-header change overturns it, between is inconclusive.**

The second device cost **26 lines** in LineSweeper — an include, a five-line
struct, an eleven-line table, a seven-line loop, one line to reach the pads and
one on the restart — of which nine are binding rows any design pays for
wherever the table lives. `samples/minimal` paid **27** with no table at all.

**Nothing overturned the refusal and nothing confirmed it.** One line over is
not a result to round down. What did settle: no engine header changed, and the
cost is per device rather than per binding, so a third device is a third table
rather than an edit to every existing row.

### 7 — the drifted claims

**All nine fixed, and three more found while counting** — `dea5fe0`. §7 declined
to fix any of them, on the rule that a claim is amended by the commit that
fights it. That rule cannot reach these, which is the whole finding: no future
commit collides with `vulkan/` being absent from `ARCHITECTURE.md`'s tree block,
or with `README.md` printing three different test-case counts. A rule that
produces a table it cannot close needs a commit whose only job is to close it.

The three the survey missed, all of them in the same shape as the nine:

- `README.md`'s `engine/` size said **~17k lines** and the tree is **~33k** —
  more than doubled, and 18.7k of it is `render/` across five backends. The
  `samples/` row beside it is still accurate, which is how the units were
  confirmed: they are raw line counts, not a code-only measure.
- **`gl_functions.h` says it declares forty-one entry points. It declares
  thirty-six** — and so did the loader's `std::runtime_error`, and so did
  `ARCHITECTURE.md`'s tree. Three copies, none checked.
- That error message is the only one of the twelve **a user can read**, which
  is why it is the one that stopped being written by hand. `gl_function_count()`
  is now `constexpr`, counted from the X-macro list, and the message builds the
  number rather than spelling it (T5). 36 was confirmed by a temporary
  `static_assert` that was **not** left in — a written 36 beside a computed one
  is the same defect wearing a seatbelt.

**Where a count was decoration it was deleted rather than corrected**, which is
what §7's own closing argument asks for: `README.md`'s test row now carries an
approximate size like the two rows above it, and its Tests section says what
`ctest` prints instead of naming two numbers that were wrong. **Where a count
carries an argument it was corrected and kept** — `renderer.h`'s
`texture_factory` comparison needs its line counts to make its point about which
API takes the least, so vulkan went 356 to 378.

`ARCHITECTURE.md`'s collision line lost its parenthetical rather than gaining a
correction, exactly as §7 proposed.

### 3.2 — the particle field

**Landed, and it needed nothing from the engine** — `f5bd513`. No blend mode,
no instancing verb, no backend state, no golden image and not one changed file
under `engine/`: ten thousand `draw_sprite` calls through the verb `renderer.h`
already had, from inside one `GameObject`. `PHILOSOPHY.md:397-412`'s central
claim now has an in-tree exercise, which was the whole argument for ranking a
sample feature this high.

**The number it was waiting for held.** §3.1a's 35.4 ns a sprite predicted
about 354 µs of quad arithmetic for ten thousand particles — a fiftieth of a
16.7 ms frame — and that prediction is why the field ships with no spatial
index, no sort and no second submission path. It stays a prediction about
arithmetic rather than a measured frame time, for the reason decision 1 below
gives.

**What it found that the survey did not predict.** Two things, both recorded in
the sample's own README:

- **A tick locks and clears together**, so a full row is never visible from
  outside the simulation. The obvious implementation — look for a full row in
  last frame's match — can never fire, and that was discovered by writing it
  and watching nothing happen. The board between the lock and the clear is
  reconstructed from `shadow()` and `piece_cells()` instead. The exact answer
  would cost `World` four bytes and both its size and padding asserts, so **the
  padding assert has now priced out a second thing** — a rule the first time,
  an effect the second.
- **The glow needed no atlas.** The sample's README predicted one. Under
  premultiplied alpha a tint with `a = 0` adds without attenuating, so a
  shrinking quad of pure addition reads as a spark and the content is still one
  white texel.

**And one thing it broke and fixed.** The top-out banner drew from inside
`BoardView`, and the field's largest burst is thrown on the exact frame those
words appear — so for the first second the one screen a player has to read was
unreadable. Object order is this sample's only depth, so the banner is its own
object now, registered last. That was caught by looking at the screen, which is
the only place it could have been caught.

**`LineSweeperViewTests` is new**, and is the reason `docs/design/` and
`CLAUDE.md` gained a thirteenth ctest entry in the same commit. It links the
engine and still creates no device, because the field takes a resolved texture
handle rather than the resource table — so the ten thousand particles, the
compaction and the row reconstruction all run headlessly. It pins the policy
and not the tuning.

---

## The four decisions in §6, restated with what is now known

1. **The reference machine.** Still unnamed, and now the most expensive gap in
   the tree rather than merely the largest. §6 predicted §3.1 would "produce
   numbers with nowhere to stand" and it did: 35.4 ns a sprite and a fan-out
   crossing at 250 objects are both properties of one desktop, and neither can
   be called a floor until a named part and a measured p99 exist. **§3.2 has
   now spent one of those numbers** — the particle field's whole cost argument
   rests on the 35.4 — which makes this the decision the most other work is
   quietly leaning on.
2. **Whether markers stay on the seam.** Untouched by any of this.
3. **Whether the fan-out is itself a T1 violation. Made, and the answer is
   no** — `f411e24`. §6 said: if §3.1b confirms it has never run and no client
   in this tree wants it, then `PHILOSOPHY.md:550-551` commits to a parallel
   path whose only client is in another repository and behind the math split.
   §3.1b confirmed the first half outright and gave the second half a number.
   The decision went the other way anyway, on the ground `PHILOSOPHY.md:590-599`
   already holds: the axis was extracted from a client that draws four panes,
   that client is in its own repository because that is what the split is for,
   and "no caller in this tree" is exactly the count that section refuses as an
   argument. The mechanism stays.

   **What was wrong was smaller, and is what the commit actually changed.**
   "Throughput is the measure … on the parallel path the engine commits to" read
   as though the parallel path were unconditionally the faster one, and below
   ~250 objects it is not. The Performance section now says the commitment is to
   the axis rather than to always taking it, and names the dial that already
   existed — `Scene`'s pool and partitioner are constructor parameters, and
   `nullptr` for either takes the serial path. **No threshold was added to
   `scene.cpp`**, and its early-out comment now says that is a decision: the
   crossing moves with the build and with the part, so a number there would be a
   machine-specific policy baked into mechanism, and decision 1 above is why no
   such number can be a floor yet.
4. **The `.xwb` container.** Untouched, and still what makes §3.4b weeks or
   months.

---

## What the survey itself got wrong

Recorded here because `next.md` reads as written, the way `docs/review/` does.

- **§2.1's grep.** It offers `grep -rn ThreadPool tests/ bench/` **is empty** as
  its evidence that nothing pairs a pool with a scene. It was not empty at
  `b4bcda0`: `tests/core/thread_pool_tests.cpp` existed and has seven cases.
  The conclusion it was supporting is still true and was checked again at
  `90ce3d0` — `tests/scene/scene_tests.cpp` names neither `ThreadPool` nor
  `Partitioner` and passes `nullptr` twice — but the grep as printed does not
  establish it. The finding stands; one line of its evidence does not.

- **§7's table is nine rows long and the count was twelve.** Not an error in
  any row — every one of the nine was still true when it was fixed — but the
  sweep that produced them stopped at the claims it had gone looking for.
  `README.md`'s `engine/` size and the three copies of "forty-one entry points"
  were all in files §7 was already reading. A table of drifted counts that is
  itself an undercount is the finding restated, and §7 predicted it in its own
  last paragraph.

Nothing else in the survey has failed a check yet. §1 says every citation was
verified by reading the file, and the five items worked through so far found
two exceptions between them.
