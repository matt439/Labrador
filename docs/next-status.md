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

Written against `94076f1`.

---

## The table

| Item | State | Commit |
|---|---|---|
| **2.1** The parallel path has never executed | **discharged** | `90ce3d0` |
| **2.2** `samples/minimal` cannot be quit without a controller | **fixed** | `d3de8f6` |
| **3.1a** Render bench, engine arithmetic | **landed** | `a9b806c` |
| **3.1b** `Scene::draw` fan-out under null | **landed** | `90ce3d0` |
| **3.2** The LineSweeper particle field | **landed** | `f5bd513` |
| **3.3** `engine/ui/` has no client, and no `Direction` producer | **both fixed** | `f567fe7`, `c2411a0` |
| **3.4a** `tests/audio/` — the cheap evidence | **landed**, and it found one defect | `708114b` |
| **3.4b** The audio seam | **landed**, and it was not blocked on `.xwb` after all | `94076f1` |
| **3.5** Sprite sheets discard `origin` and `rotated` | **both keys answered** | `169a3c0` |
| **5** The InputMap refusal | **measured, and inconclusive** | `d3de8f6` |
| **6** The four decisions `next.md` does not make | two made — the third, then the first; two still unmade | `f411e24`, *this commit* |
| **7** The nine drifted claims | **all nine fixed**, and three more found | `dea5fe0` |

**Every work item in the survey has landed** — the whole spine
2.2 → 3.1a → 3.3 → 3.4a → 3.4b, all three branches off it (3.1b, 3.2, 3.5),
and §7. What is left is §6: two unmade decisions, and one measurement that
a decision has now made takeable.

**3.4b is the one that came out differently from the way the survey ranked it**,
and the difference is worth the sentence. It was filed as *weeks*, at the bottom
of the spine, "blocked on the `.xwb` container question". It was not. The
container question is real and is still unmade — it is §6's fourth decision —
but the seam does not have to answer it, because `next.md` §6 named the other
option in its own words and that option turned out to be the whole of the work:
*"§3.4 either moves the content format too **or is cut above it with the format
decision deferred**"*. Cut above it, the item is days rather than weeks, and
nothing about it needed hardware this desktop does not have.

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

### 3.3 — the module with no client, and the header that lied

**Both findings fixed, in two commits.** `f567fe7` gave `engine/ui/` a client;
`c2411a0` made `navigation.h`'s sentence about `engine/input/` true.

**The first was answered by a pause screen, and nothing in `engine/` changed
to make it work.** That is the outcome §3.3 predicted and it is recorded as a
result rather than assumed: the widget set, the focus group, the navigation
walk and the state stack were all used exactly as they shipped. Two things the
engine already had turned out to be load-bearing and had never been run —
`State::covers_screen()` returning false, which `state_context.h` names a pause
menu as the worked example for twice, and `StateContext::pop` being queued
rather than immediate, which is the only reason a button's action may pop the
state it lives inside.

**A stub is not a client, and that is the part worth keeping.** The module's
only previous exercise was four `StubWidget`s, which report whatever bounds the
test asks for. `nearest_in_direction` is arithmetic over `bounds()`, so the
three rows navigate and wrap only because a `UiText` reports what the font
measured. No test could have established that.

**The second finding cost an engine change and was worth waiting for one
client.** `Direction` moved to `engine/input/direction.h` — the sentence had
the dependency the right way round and ARCHITECTURE's table already allowed
`ui → input`, an edge nothing had ever stood on. Beside it went the two pieces
a client keeps writing: `stick_direction` for the quadrant test and
`DirectionRepeat` for the hold-to-repeat, plus `pad_direction` to collapse a
stick and a d-pad into one answer.

**The order was the point.** The pause screen's first version translated two
keys and two d-pad buttons itself, in nine obvious lines, and that was cheap
*because both are edge devices* — an edge needs neither a deadzone nor a
repeat. Adding the stick is what makes the other two thirds appear. The
mechanism landed with the client that needed it rather than ahead of one, which
is T1's shape rather than a violation of it, and §3.3 called that shape
correctly in advance.

**One thing it caught in passing.** `ARCHITECTURE.md`'s `tests/` tree still
said `tests/linesweeper/` was one target linking no engine, which `f5bd513` had
already made false — CLAUDE.md and README were amended then and that one was
missed. Fixed in `c2411a0`. §7's lesson holds: the counts that drift are the
ones nothing checks.

### 3.4a — the list the audio seam was waiting on

**Landed, and the answer is larger than the question** — `708114b`. §3.4a asked
for the written list of which of `SoundBank`'s fourteen public methods
`silent()` makes unreachable, on the ground that nobody should spend weeks on
§3.4b before it exists. The list is the header of
`tests/audio/sound_bank_tests.cpp`. What it turned out to rest on is a harder
fact than the item anticipated.

**An audible `SoundBank` is not untested here. It is unconstructible.** The
other constructor takes a `std::unique_ptr<DirectX::WaveBank>`; DirectXTK's
`WaveBank` has exactly one constructor, an `AudioEngine*` and a path to an
`.xwb`; `find . -name "*.xwb"` is empty, because the one the shipped manifest
names is built from source audio that cannot be distributed; and
`DirectX::SoundEffectInstance` has no public constructor at all, only a move, so
the instance registry cannot be filled by hand either. `silent()` is therefore
not one of two banks a test may choose between. It is the only bank this
repository can build, and that is not a property of the tests.

That is `PHILOSOPHY.md:632-637` almost word for word — *a seam with only the
platform's own implementation behind it still requires the platform in order to
construct anything*. So the finding is not that `silent()` is a poor headless
implementation. It is that **`silent()` is not a headless implementation at
all**: it is a null `WaveBank` pointer inside the platform's own class, checked
at the top of every method. It can decline to do things. It cannot record them
the way `render/null/` records a draw, which is why no test in this tree can
assert that a sound was played.

**The list, and the number that comes out of it.** Fourteen public methods
counting the named constructor, so thirteen instance methods. A silent bank
leaves **five** answering something — `audible`, the two resolvers, and
`effect_state`/`is_effect_looping`, both of which answer for a handle they never
issued — and **eight** with no observable behaviour whatever, where "it did not
throw" is the whole of what a test can assert.

**What the eight cost is the part worth carrying into §3.4b.** Every line the
`audible()` check skips is engine code that has never executed in this
repository, and the level clamp is on five of the eight. That clamp is
arithmetic this module owns — folding a volume into `[0,1]` is the same kind of
engine-side decision as the glyph walk in `render/font.h` — and it sits *below*
the check for the platform rather than above it. A seam drawn where `audible()`
is checked today would put it on the platform's side of the wall. That is one
concrete thing §3.4b now knows that it did not, and it is the term the item was
actually buying.

**Two promises a silent bank cannot keep, both deliberate and both now pinned.**
Resolving is this class's entire T6 guarantee — a misspelt wave throws at load —
and a silent bank has no name table, so `"bang"` and `"bagn"` are not merely
both accepted, they are the same answer. `sound_bank.h:41-48` states that trade
out loud already. The second is `play_wave`'s unresolved-handle throw, which is
also below the check: the one mistake the class exists to catch loudly is caught
in a build with audio and goes unmentioned in a build without it.
`sound_bank.cpp:35-38` says a silent bank must not reject anything, so both are
the rule rather than an oversight.

**And a defect, found by constructing the type rather than by reading it.** A
default-constructed `SoundBankObject` dereferenced a null `AudioResources*` on
every call, and nothing could ever repair it — `set_sound_bank` changes which
bank, not which table — so an object built that way was permanently unusable
and crashed rather than saying so. It **was confirmed as a `SIGSEGV` before it
was fixed**, by backing the guard out and running the case, rather than argued
from reading the pointer. It now throws `std::logic_error` naming the problem,
which is what `Registry` already does instead of answering `nullptr` (T6), and
the constructor takes the same guard.

Nothing in this tree inherits `SoundBankObject`, and per `PHILOSOPHY.md:612-621`
that settles nothing — which is exactly why the answer is a loud throw and not
a deletion. It is also the argument for the whole item: this cost an afternoon,
a grep could not have found it, and it was sitting in a module that had never
had a test compiled against it.

### 3.4b — the audio seam

**Landed, and the block it was filed under was not the one that mattered.**
§3.4b sat at the bottom of the spine marked *weeks* and *blocked on `.xwb`*.
The `.xwb` block is real and is untouched; what was wrong was the belief that
the seam had to wait for it. `next.md` §6 had already written the alternative —
cut the seam above the container and defer the format — and once that is the
plan, none of the remaining work needs a wave bank, a sound card or a second
platform.

**What the cut is.** `engine/audio/audio_device.h`: a concrete class chosen at
build time by `LABRADOR_AUDIO_BACKEND`, sixteen methods wide, with
`audio/xaudio2/` behind it and `audio/null/` beside it. Above the seam is
everything this engine decides — which name means which wave, which handles are
valid, and the level clamp. Below it is what an audio API does: open a
container, find a name in it, build a voice, and start, stop, adjust or report
one. `open_wave_bank` takes a directory and a bank *name*, never a file name,
which is where the container decision is deferred to: the extension and the
reader are the backend's.

**The one thing that had to cross the seam, and it was not obvious in advance.**
A backend with no container has no wave-name table, and a backend that accepts
every name is `silent()` again — the exact failure the item exists to fix. So
the seam takes the definition's list of wave names at `open_wave_bank`. That is
principled rather than a workaround, and the precedent is one folder over:
`render/null/` is handed a `TextureData` the *engine* decoded out of a `.dds`
and keeps only the width and the height. The engine parses its own content; a
backend keeps the minimum it needs to answer questions. On `xaudio2/` the list
is checked against the container and a wave the definition names but the `.xwb`
lacks throws at open, naming both — earlier and stricter than before, where the
same content bug surfaced at `CreateInstance` or not at all.

**What it bought, in the numbers §3.4a produced.** All of them.

| §3.4a found | now |
|---|---|
| 8 of 13 instance methods with no observable behaviour | all 8 assert what they did, in `tests/audio/null_tests.cpp` |
| 5 sites of level clamping never executed | all 5 execute, and the clamped value is read back off the device |
| `stop_effect`'s `immediate` provably inert | both spellings arrive, as different calls |
| an audible `SoundBank` unconstructible | constructible in one preset, over a device that records |

**And one thing the reorder fixed that a recording backend could not.** The
clamp and the handle checks moved *above* the test for whether a bank has
content, which is where §3.4a argued they belonged — the clamp is engine
arithmetic and a seam drawn at the old check put it on the platform's side of
the wall. For the clamp that is a claim about which side of the wall the code
is on and changes no behaviour; **for the unresolved-handle throw it changes
real behaviour**, and it is the one promise `silent()` used to break that it
does not have to. A handle nobody resolved is now refused by every bank in
every build, where before "the one mistake this class exists to catch loudly"
was caught with audio present and unmentioned without it.

**Three things fell out that the item did not ask for.**

- **`Microsoft::DirectXTK` is `PRIVATE` on `LabradorEngine`.** It was `PUBLIC`,
  and not as a precaution: four public engine headers named `DirectX::` types,
  so `<Audio.h>` was on the compile line of every sample, every test and every
  downstream game whether it made a noise or not. The last of the four went
  with this seam. Two `find_package` calls came out of each sample's
  `CMakeLists.txt` — including the new-project template's, which is the one
  every project on this engine starts from.
- **`samples/minimal` had `using namespace DirectX;` in two state files**, using
  nothing from it. The template told every copier to open a Microsoft namespace
  it did not need, and only stopped compiling when the header stopped arriving.
- **`check_engine_includes.cmake` now captures the module as well as the
  backend.** The rule was written for `render/` and audio is the module where
  the thing it exists to prevent had already happened, unwatched. The change is
  one capture group, and a third module with backend folders needs no edit.

**Where the tests live, and why the split is the honest one.** The parse is
device-free and runs in all six configurations —
`tests/assets/sound_bank_loader_tests.cpp`, new, and the loader had no test at
all before because reading the JSON used to require constructing a
`DirectX::WaveBank` first. What a bank *played* needs a device, so it is
`tests/audio/null_tests.cpp`, compiled only under `x64-debug-null`, exactly as
`tests/render/null_tests.cpp` is. `AudioTests` still constructs no device in
the five presets that build `audio/xaudio2/`.

**What did NOT change, and is the honest limit of the item.** There is still no
`.xwb` in this tree and nothing here has ever played a sound. §6's fourth
decision — write the container format down, an `xwb_file.h` beside
`dds_file.h` — is exactly as unmade as it was, and it is now the whole of the
audio question rather than half of it. It is also blocked on something a
decision cannot supply: you cannot write and check a format reader with no file
of that format. `docs/port/android.md` §3.2 carries the same conclusion from
the port's side, and its own finding — that audio was the one place the
second-platform claim was provably false — is discharged.

**One behaviour change a reader might trip over.** The definition is parsed
before the container is opened, because a backend with no container answers out
of the parsed wave list. So a bank whose `.xwb` is missing *and* whose JSON is
malformed now reports the malformed JSON, where it used to report the missing
file. The JSON is in every clone and the container is not, so that is the
better of the two answers (T6) — but it is a change, and it is recorded in
`sound_bank_loader.h` as well as here.

### 3.5 — the two keys the loader read and dropped

**Both answered, in one commit** — `169a3c0`. `rotated` is refused at load by
name; `origin` is honoured, added to whatever origin the caller passed. §3.5
predicted exactly that split and gave the reason for it, and nothing found
while doing it argued against either half.

**What the survey's grep claimed was true.** `origin_` and `rotated_` were two
members, two constructors that set them, and nothing else — no accessor, no
reader, no draw path. Checked again before the change.

**The measured part, and it is the reason this was cheap.** Nothing in *this*
tree has a sprite sheet at all: neither sample declares the `sprite_sheet`
asset kind, so the whole path is client-only code and no golden image, no
sample and no other test reaches it. The client does have one, and it was read
rather than guessed at — `game/content/textures/sprite_sheet_1.json`, **41
frames and 9 strips, of which exactly one frame mentions either key**, and it
sets `"origin": {0.0, 0.0}` and `"rotated": false`. Both the identity. So the
refusal rejects nothing the only client owns and the composition moves nothing
it draws. The two keys look like a packer template filled in once and
abandoned, which is consistent with nobody ever noticing they went nowhere.

**The member was deleted, and that needed an argument rather than a grep.**
§5 refuses deletion of anything that merely looks unused here, on
`PHILOSOPHY.md:590-599` — zero callers here means zero callers *here*, and
live client API has been deleted twice on that mistake. `SpriteFrame::rotated_`
is outside that rule for a reason narrower than "nothing calls it": it had **no
accessor**, so no client could observe it, and a constructor argument that is
provably ignored is not behaviour anybody can depend on. Removing it is a
compile error for a caller that passed one — which is the loud answer — rather
than a silent change of what a program does. §3.5 asked for it in its own
words: rotation "should be argued on its own, not smuggled in by a member that
already exists".

**The origin composes rather than replaces, and the reason is a missing
sentinel.** A sheet's pivot and a caller's origin are the same quantity in the
same units, so `SpriteSheet::draw` adds them. The alternative — the frame's
unless the caller gave one — cannot be written: a default argument cannot tell
a caller that said nothing from one that asked for the top-left corner, so the
rule would hang on whether a `Vector2F` happened to be zero. Addition also
makes the change invisible to every frame with no authored pivot, which is
almost all of them.

**Two test files, because neither can hold the whole finding.**
`tests/assets/sprite_sheet_loader_tests.cpp` is new, runs in all five
configurations and creates no device — `read_sprite_sheet` takes a
`TextureHandle`, and a handle is an index. It pins the parse and the refusal
message, including that the message names the offending frame and **not** the
one before it. The composition is not readable anywhere after the call — it is
one argument to `build_sprite_quad` and then four corner positions — so it is
pinned in `tests/render/null_tests.cpp`, where a quad can be read off a
recording on a machine with no GPU.

---

## The four decisions in §6, restated with what is now known

1. **The reference machine. Made, and it is half of what the bar asks for** —
   *this commit*. `PHILOSOPHY.md`, Performance now defines "the low tier", the
   term four backend comments were sizing real decisions against while no
   document said what it meant. It is the Radeon Graphics adapter integrated
   into this desktop's Ryzen 9000 package (PCI `1002:13C0`), at 1280x720, with
   the process held to four cores.

   **A configuration rather than a purchased part, and that was the decision.**
   `samples/linesweeper/README.md` asks for a named part, a named resolution
   and a measured p99, which reads as a demand for hardware. There is none to
   name: the only machines here are this desktop and a Raspberry Pi 3B+, and
   the Pi is not too low but on the wrong axis — Windows-only tree, a GL
   backend that creates its context through WGL, VideoCore IV topping out at
   OpenGL 2.1, and no Vulkan driver until the Pi 4. Naming hardware nobody can
   boot would have left every number where it was while reading as decided, so
   the answer is the weakest thing in the box that is still a real driver. It
   is not WARP for the reason `PHILOSOPHY.md` now gives.

   **What it buys, and the half it does not.** All four rasterising backends
   run on it, so all four "low tier" sites have a referent. But it names a GPU,
   and §3.1a's 35.4 ns and §3.1b's 250-object crossing are both CPU-side —
   capping the core count changes how many workers the fan-out gets, never how
   fast one is. So those two stay properties of a fast desktop and `f411e24`'s
   refusal to bake a threshold into `scene.cpp` is untouched. The four claims
   also split: the two 256 KB vertex pages are memory budgets a written spec
   settles, and the two about pipeline depth need the p99. **No p99 has been
   taken**, so nothing here is a floor yet and both documents say so.

   **The candidate that would replace it, and what it is blocked on.** A 2013
   MacBook Air is the one genuinely low-tier *whole* machine in reach — a
   dual-core Haswell fixes exactly the CPU gap above. Under Boot Camp it runs
   D3D11 (HD 5000 is feature level 11_0 against a 10_0 floor) and GL 3.3 core
   (Intel's Haswell driver exposes 4.3), and it runs neither D3D12 nor Vulkan,
   because Intel's Windows drivers for both start at Skylake and Haswell has no
   driver at all — which strands precisely the two pipeline-depth claims. It
   would be a named part with a takeable p99, so it is the successor rather
   than a rival; it is blocked on whether the machine still boots Windows, on
   an EOL Windows 10, and it has a second and better job — see decision 4's
   neighbour below.
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
4. **The `.xwb` container.** Still unmade — and it is now the *only* thing left
   in the audio question rather than the thing that gated it. §3.4a walked into
   it from one side and §3.4b from the other, and between them they moved it
   twice.

   **First, it stopped gating the seam.** §3.4b was filed as weeks and blocked
   on this decision; it was neither, because §6 itself had written the other
   option — cut the seam above the container and defer the format — and that
   option cost days. `AudioDevice::open_wave_bank` takes a directory and a bank
   name, so the extension, the reader and the bytes belong to a backend and no
   line above the seam knows what an `.xwb` is.

   **Second, one of the two questions attached to it has been answered, and not
   by deciding anything.** §3.4a asked *"can this repository ever have a test
   that plays anything"* and recorded the answer as no. It is now yes:
   `audio/null/` records what it was asked to play, so `tests/audio/null_tests.cpp`
   asserts which wave, out of which bank, at which levels, in what order. What
   it does not do is make a sound, and nothing here ever will.

   **What is left is the original question and it is unmoved.** Does the
   content format move to a second platform, or does this engine write it down
   — an `xwb_file.h` beside `dds_file.h`, which is the precedent
   `docs/port/android.md` §3.2 argues from and the same shape as the reader
   that already replaced `CreateDDSTextureFromFile`. **It is blocked on
   something a decision cannot supply**: there is no `.xwb` in this tree to
   write a reader against, for the same undistributable-source-audio reason the
   manifest marks the bank optional. That is worth stating plainly, because it
   is the only item in this document whose blocker is a missing file rather
   than a missing judgement.

**A fifth question, raised by none of the above and now on the table.** The
plan is for this engine to reach macOS. Four documents already say Vulkan gets
there through MoltenVK and that "Metal eventually" is then a build target
rather than a sixth backend — `docs/port/android.md` §5 — but that is stated as
a conditional and its own §1 files the condition as **the one unmeasured claim
in the document that would cost real work if it is wrong**. A plan makes a
conditional load-bearing. There is no `docs/port/macos.md`, and `CLAUDE.md`
still reads "a sixth is not planned". Worth noting here because the claim is a
*capability* question — does the subset MoltenVK omits touch a seam that draws
textured quads with one blend state and two samplers — so it needs a Mac and
not a fast one, and the 2013 Air under decision 1 is one. That is a better use
of it than being a low-tier D3D11 box, and Boot Camp does not make the two
exclusive.

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

- **§3.5's price tag is for the feature its own body refuses.** The heading
  reads *days* and the spine entry reads "3.5 refuse `rotated` (independent,
  days)" — but the paragraph between them argues that the in-doctrine answer is
  one branch in the loader rather than the packer-rotation feature, and it is
  right. Refusing, deleting the member, honouring `origin` and writing both test
  files came to an afternoon. The estimate priced the option the item talks
  itself out of, which is a mis-estimate rather than a wrong claim, and it is
  the only reason this sat at the bottom of §4's spine.

- **§3.4a's question presupposes a bank that cannot exist.** It asks for "which
  of `SoundBank`'s fourteen public methods `silent()` makes unreachable", which
  reads as a comparison: these are reachable on a real bank and not on this one.
  There is no real bank to compare against in this repository and there cannot
  be one, for the reason recorded above. The list still exists and is still the
  product — what a silent bank leaves observable is a question with an answer —
  but it had to be taken against the class's source rather than against a second
  bank, and the fact that made that necessary is worth more than the list. Its
  price tag held exactly, which §3.5's did not: hours, and it was an afternoon.

- **§3.4b was blocked on nothing, and the survey said so itself two sections
  later.** Its heading reads *weeks*, its spine entry reads "blocked on .xwb,
  see port/android.md", and §2's overview calls it "the one item that was
  always going to need something this desktop cannot supply". §6's fourth
  decision then names the escape in one clause — the item is "cut above [the
  container] with the format decision deferred" — and that is the whole of what
  it took. Cut there, nothing in the seam knows what an `.xwb` is, and the item
  came to days on this desktop with no new dependency.

  **This is a worse mis-estimate than §3.5's and a different kind.** §3.5
  priced the option its own body talks itself out of, which is one paragraph
  disagreeing with its own heading. Here two sections of the same document
  disagree, the pessimistic one is the heading and the spine, and the effect
  was to rank the item last: everything else was worked through first partly
  *because* this was believed to need hardware. What made the difference is
  that §3.4a was done first and reported honestly, and the reason it is not a
  reason to distrust the survey is that the survey wrote both halves down. A
  sweep that records the escape it does not take is one a reader can correct.

  **What was right, and it was the important half.** The `.xwb` question is
  real, it is untouched, and it is now the entire remaining audio question —
  see decision 4 above. What the survey got wrong was which side of it the seam
  was on.

Nothing else in the survey has failed a check yet. §1 says every citation was
verified by reading the file, and the eight items worked through so far found
five exceptions between them — two wrong, two merely expensive, one that asked
a question with a missing premise.
