# Input: reports 13, 21

Both reports come from one event. ColourWars shipped the first rebinding
screen any client of this engine has had: a capture screen that lets a player
choose the keyboard key for one action. The two reports land on opposite sides
of one row of PHILOSOPHY's boundary table, "Input devices: pads, keyboard,
mouse and touch | Which actions exist, their bindings, and any rebinding"
(docs/design/PHILOSOPHY.md:449). Report 13 asks for the names of the values of
an enum the engine owns. Only the engine can keep that list complete, so it is
mechanism, and it is accepted. Report 21 asks the engine to evaluate a table of
bindings. The row gives that table to the game, and the clients already combine
their input sources by rules that disagree, so it is policy, and it is
declined. The decline still changes something: the reason the engine gives for
having no action map, that no client has a rebinding screen, is now false and
has to be replaced wherever it is written.

Implement 21 first. It is documentation only, extra small, and needs no owner
decision, and it is the commit that re-takes the stale reason. Report 13 is its
own small story under PI-10. It waits on two owner decisions (the token for
`Key::del`, and whether to keep the name `key_name` given the source break
described below), and neither report depends on the other. Line numbers are as
of `0c15f34`. This is a decision record written ahead of PI-09 filing the
reports and PI-10 answering them. It is not an implementation; the README's
table says what has been built since.

## 13. Names for keys

**Decision.** Accept. Add `std::string_view key_name(Key)` and
`std::optional<Key> key_from_name(std::string_view)` to
`engine/input/keyboard.h`. Each key's token is its enumerator's spelling, the
function covers every enumerator, and a test fails when a key is added without
a name.

**What the engine has now.** `Key` is a physical key position, not a character
(engine/input/keyboard.h:35-40). The enum is at engine/input/keyboard.h:48-118,
and `count` is documented as "the bound every loop over the set stops at"
(engine/input/keyboard.h:115-117). Typed text is a separate channel,
`Keyboard::typed()` (engine/input/keyboard.h:265-279). Nothing in
`engine/input/` turns a `Key` into a string or a string into a `Key`. Both
windows decode platform events into `Key`, one way only. The Win32 window
reads set-1 scan positions (engine/app/win32/window.cpp:14-17). The web window
decodes `KeyboardEvent.code` with `key_from_code`
(engine/app/web/dom_events.h:22, engine/app/web/dom_events.cpp:161). That
decoder maps many codes to one key on purpose, `ShiftLeft` and `ShiftRight` to
`shift` (engine/app/web/dom_events.cpp:46) and `Enter` and `NumpadEnter` to
`enter` (engine/app/web/dom_events.cpp:50). It is compiled only in the web
build (engine/CMakeLists.txt:95) and is not a published header. The modifier
collapse is documented as a deliberate simplification that is "additive to fix"
(engine/input/keyboard.h:42-47). The engine already has one lookup from a
string to a value: `colour_from_name` returns `std::optional` and leaves "the
caller decides whether that is a default or an error"
(engine/render/colour.h:244-248, engine/render/colour.cpp:518-529).

The client keeps two tables of its own. One has about eighty names for a
developer console and is built by arithmetic on the enum's order. The other
has the A to Z and Space tokens its saved keyboard preference uses. Where the
two tables overlap, their spellings agree.

**Why.** The public face settles it: "the argument has to be about whether a
client would have to write them itself — a predicate a game needs belongs in
the engine, at whatever count" (docs/design/PHILOSOPHY.md:792-798). Any game
that saves a binding, logs a key or has a console has to write this table. Only
the engine can keep it right, because the engine owns the enum, and keys will
be added to it: engine/input/keyboard.h:46-47 says the modifier split is
additive.

T1 (docs/design/PHILOSOPHY.md:35) holds. The mechanism is the name of a value
the engine owns. The policy stays with the client: which keys may be bound,
the label a player sees, case folding, aliases for old files, and any chord
grammar such as `ctrl+`. T12's price is that without reflection, serialisation
is "written by hand or absent" (docs/design/PHILOSOPHY.md:261-264). This
function is the hand-written half for one engine type.

The tokens follow the header, so a reader can predict every token from
keyboard.h without a table (T4, docs/design/PHILOSOPHY.md:83). For the same
reason the W3C `code` strings are rejected as the canonical form: `Key::shift`,
`Key::control` and `Key::alt` have no single W3C name, so W3C codes cannot
spell the engine's collapsed modifiers. That argument is weaker than it first
looks, and the owner should weigh it. The engine's only existing precedent,
`colour_from_name`, spells its content tokens in a vocabulary separate from the
identifiers: `"DARK_SLATE_GREY"` names `dark_slate_grey`
(engine/render/colour.h:244-245). The header's own comment on `del` also calls
it "the one name in the set that could not be spelt the obvious way"
(engine/input/keyboard.h:77-78). That is an argument for the token `delete`.

T7 and T8 (docs/design/PHILOSOPHY.md:124, :137) are not engaged. A name is
looked up when a file is loaded or a console line is read, never per frame.
T3 (docs/design/PHILOSOPHY.md:64): the codec is finite and exact, with no
localisation, no chords and no case folding. T5
(docs/design/PHILOSOPHY.md:95): a `switch` with no `default` lets clang's
`-Wswitch`, under the wasm presets' `-Wall -Werror`, reject a key without a
name at compile time. MSVC's equivalent warning, C4062, is off by default, so
on Windows the round-trip test is what enforces completeness.

**The change.** In `engine/input/keyboard.h`, after the enum and before
`KeyboardState`, add the two functions with a header comment that states the
contract. A sketch of the comment:

```cpp
// The token for a key in a file or a console: the enumerator's own spelling
// ("a", "digit_1", "numpad_1", "page_up", "left_bracket", "del"). Lower-case
// ASCII, unique per key, independent of layout and platform. Empty for
// Key::none, Key::count and any value outside the set. A name, not a label:
// what a player is shown is the game's. Read at load, not per frame (T7).
//
// The spelling is a file format. A key renamed here changes what every saved
// file means, and a key added keeps every existing token's meaning.
std::string_view key_name(Key key);

// The key a token names. Exact and case-sensitive: no trimming, no aliases,
// no chords. Nothing for anything else, "none" and "count" included; the
// caller decides whether that is a default or an error, as colour_from_name
// does.
std::optional<Key> key_from_name(std::string_view name);
```

Add `<optional>` to the header's includes. It already has `<string_view>`
(engine/input/keyboard.h:3-5). The comment is published on the website word for
word (keyboard.h is listed at website/src/lib/published.ts:185), so it has to
read as contract, not as history, and it must use `//` comments, never
`/* */`. The reference page picks the new functions up with no change to the
list.

Next to the modifier note at engine/input/keyboard.h:42-47, add the constraint
this creates. A future left and right split adds new keys (`left_shift` and so
on), and `shift` keeps meaning either one. Otherwise a game's saved `"shift"`
silently changes meaning.

In `engine/input/keyboard.cpp`, `key_name` is one `switch` with a case for
every enumerator, `none` and `count` included, since clang's `-Wswitch` wants
them too. There is no `default`, and a `return {};` after the switch covers
out-of-range values and keeps MSVC's C4715 quiet. `key_from_name` is derived
from that switch, so there is only one table:

```cpp
std::string_view key_name(Key key)
{
	switch (key)
	{
	case Key::none:
	case Key::count:
		return {};
	case Key::a: return "a";
	// ... every enumerator, spelt as declared ...
	case Key::del: return "del";
	// ...
	}
	return {};
}

std::optional<Key> key_from_name(std::string_view name)
{
	for (unsigned int index = static_cast<unsigned int>(Key::a);
		index < static_cast<unsigned int>(Key::count); ++index)
	{
		const Key key = static_cast<Key>(index);
		if (key_name(key) == name)
		{
			return key;
		}
	}
	return std::nullopt;
}
```

Spell every type out; the engine uses no `auto`. A lookup compares about a
hundred strings and runs off the frame path, with no virtual call and no
`std::function`.

**A source break that the original assessment missed, inside this tree as well
as in the client.** `tests/app/web/window_page.cpp:50` defines its own
`key_name(labrador::Key)` in an anonymous namespace at global scope and calls
it unqualified at `:134` and `:139`. That file includes keyboard.h through
`engine/app/window.h:3`. Once `labrador::key_name(Key)` exists,
argument-dependent lookup on the `labrador::Key` argument finds it alongside
the local function. Both take `Key` by value, return types play no part in
overload resolution, and so the call is ambiguous. `WebWindowTests` would stop
compiling under the `wasm-*` presets. The same commit deletes that local
function and calls `labrador::key_name`. Every key that
`tests/app/web/window_input.mjs` expects in the log (`a`, `left`, `right`,
`up`, `down`, `space`, `tab`, `control`) logs the same string as before. Keys
the local table did not name would now log their real token instead of
`other`, and `Key::none` would log an empty string instead of `none`. No
expectation in window_input.mjs depends on either. The client has exactly the
same shape: a function of the same name, taking `Key`, in its own namespace,
called unqualified. It breaks the same way at the pin move.

Platforms and backends: the module is platform-neutral, so the code is the
same in every preset. No render, audio or input backend changes. Neither
window implementation changes, and `dom_events.cpp` is untouched. The clang
build (`wasm-debug`) is where completeness is enforced at compile time, and it
is also the only build of `window_page.cpp`.

**Tests.** In `tests/input/keyboard_tests.cpp` (`InputTests`), in the same
commit:

- Every `Key` from `a` up to, but not including, `count` has a non-empty name
  made only of `[a-z0-9_]`.
- No two keys share a name.
- `key_from_name(key_name(k)) == k` for every key, and
  `key_name(*key_from_name(s)) == s` for every name.
- `key_name(Key::none)`, `key_name(Key::count)` and
  `key_name(static_cast<Key>(255))` are empty.
- `key_from_name` returns nothing for `""`, `"none"`, `"count"`, `"J"`,
  `"Space"`, `" j"`, `"j "`, `"ctrl+j"`, `"KeyJ"`, the rejected spelling of
  delete (`"delete"` or `"del"`, whichever is not chosen), and `"j\0"` with
  length 2.
- Pinned spellings for the cases a reader would guess wrong: `digit_1` against
  `numpad_1`, the delete key's token, `page_up`, `left_bracket`, `grave`.

None of these needs a device or a window. `InputTests` runs in all ten Windows
presets and, under Node, in both `wasm-*` presets. Check at least `x64-debug`
and `wasm-debug`, the latter with a Chromium configured so that
`WebWindowTests` is built and runs with the edited `window_page.cpp`. CI
covers the rest.

**Consumer-visible.** Yes, as a source break. The assessment called the change
not consumer-visible because it adds new names. That is wrong. CHANGES.md's
first clause, "a source break, where code that compiled against the engine
before the change does not compile after it" (CHANGES.md:4-6), comes before
"a new public name included, is not" (CHANGES.md:13). Code that already calls
its own `key_name(Key)` unqualified stops compiling, so the commit adds an
entry under `## Unreleased`. The entry gives the old spelling (an unqualified
call to a game's own `key_name(labrador::Key)`), the new one (qualify that
call, or delete the game's function and call `labrador::key_name`), and the
migration diff, which is this commit's own change to
`tests/app/web/window_page.cpp`, since no sample is affected. The entry also
says that tokens are now a file format: renaming an enumerator later is a
source break and a format break in one change, and its entry has to say both.
`samples/minimal/` does not change, so its README change list does not either.

The alternative is a name nobody uses yet, such as `key_token` and
`key_from_token`, which turns the change back into a pure addition. That costs
the parallel with `colour_from_name` and CONVENTIONS' plain noun accessor. The
owner chooses (see below).

**What the client could delete.** The name column of its saved-preference
table and the bodies of its two key/name conversion functions. Its saved
tokens are already the enumerator spellings, so no save file changes. It also
loses the enum-order arithmetic and the name list in its developer console
table. It keeps, as its own policy: which keys may be bound, the labels a
player sees, throwing on an unknown token, case folding, its chord parser and
chord names, and which keys are reserved. Its developer bindings file uses
shorter spellings for the number row, the numpad, the page keys, the brackets
and delete. So it either keeps a small alias table or migrates that file once.

**Open questions.**

- The token for `Key::del`. `del` keeps the rule exceptionless: every token is
  its identifier (T4, T12). `delete` reads better in a file, and the evidence
  for it is engine/input/keyboard.h:77-78, which calls `del` a keyword
  workaround, plus `colour_from_name`'s separate content vocabulary. Decide
  before the code lands, because the token is a file format from then on.
- Keep `key_name`, with a source-break entry and the `window_page.cpp` fix in
  the same commit, or pick a name with no collision. Whichever is chosen, the
  ColourWars pin bump that takes this commit has to fix its call in the same
  change.
- Whether CHANGES.md's opening paragraph should say outright that a new free
  function overloaded on an engine type can break a game's unqualified call
  through argument-dependent lookup. As written, the paragraph reads as if new
  names are always safe.
- `GamepadButton`, `MouseButton` and `GamepadTrigger` have the same gap. No
  client has asked for names for them, so under T1 none are built now. If one
  ever is, it mirrors this shape.
- `key_from_code` returning `Key::none` does not conflict with
  `key_from_name` returning nothing. The first is a platform event meaning
  "unmapped"; the second answers a question about a string.

## 21. Evaluating typed bindings

**Decision.** Decline, and amend every document that gives the old reason.
Evaluating a table of bindings is the action map, which PHILOSOPHY's boundary
table gives to the game. The engine already has the mechanism for each source,
and the clients combine sources by rules that disagree. Since a client now has
a rebinding screen, the stated reason for the absence ("no client has a
rebinding screen") is false and is replaced in the same commit that records
this decline.

**What the engine has now.** engine/input/gamepad.h:9-16 explains the absence:
the samples spell their bindings in code and "none has a rebinding screen".
It also quotes the boundary table as giving this module "input devices and
action mapping". That quotation has been stale since `734c836`, which amended
PHILOSOPHY but not the header that quotes it. The row now reads "Input devices:
pads, keyboard, mouse and touch | Which actions exist, their bindings, and any
rebinding" (docs/design/PHILOSOPHY.md:449). The same false reason is in
docs/design/ARCHITECTURE.md:176-179 and AGENTS.md:634-635.

Each source already has its own edge functions, and they are pure:

- Gamepad button and trigger edges, with a threshold the caller supplies
  (engine/input/gamepad.h:125-139).
- Keyboard edges, which need focus on both frames
  (engine/input/keyboard.h:133, :157-185).
- Mouse button edges, which are also suppressed across a change of
  `button_epoch` (engine/input/mouse.h:81, :109).

`pressed()` depends only on the two snapshots and on the key asked about
(engine/input/keyboard.cpp:86-97). So "changing a binding to a key that is
already held invents no press", the report's first requirement, is already a
property of the engine's edges.

The engine's own samples combine sources in both of the ways that matter.
LineSweeper's play state keeps separate key and pad binding tables
(samples/linesweeper/states/play_state.cpp:54, :86), argues for two tables
rather than one row per action (:66-75), and reads only `held()`, folding every
source into a byte so the rules derive edges from it and a replay carries its
own edges (:218-231). Its pause menu ORs each source's `pressed()`
(samples/linesweeper/states/pause_state.cpp:276-288), and so does the
components gallery (samples/components/gallery_state.cpp:201, :221-222). Other
samples do the same inline: samples/minimal/states/hello_state.cpp:136 ORs
edges, and samples/local_multiplayer/controls.cpp:13-26 binds four keys per
player in a local array and reads `held()`. ColourWars ORs per-source edges for
its game actions and uses a third rule, based on whether the action was held
the frame before, for its menus. That is two rules inside one client.

**Why.** The boundary table (PHILOSOPHY, The boundary,
docs/design/PHILOSOPHY.md:449) gives "which actions exist, their bindings, and
any rebinding" to the game. An evaluator "over typed physical bindings" needs
an engine binding type: a list of keys, mouse buttons, pad buttons and
triggers, each trigger with a threshold. That type is the binding table,
whatever it is called. T1 (docs/design/PHILOSOPHY.md:35) has the engine supply
how and the game what. The how is already here as per-source edge functions
over two snapshots, testable without a window and independent of any binding's
history.

What is left is how sources combine, and that is policy, as the samples above
show. ORing per-source edges fires once for each source that goes down: a key
pressed and then a pad button pressed while the key is still held count as two
presses. Edges rebuilt from the combined held level fire once, until every
source has been released. LineSweeper uses the second rule on purpose so that
a match stays a function of its recorded input. Its menus use the first rule.
The client uses a third. The report itself concedes that "both policies can be
useful; an extraction must document its semantics or expose source results for
the client to combine". Exposing the per-source results is what the engine
already does. An extraction would ship one rule and treat the others as wrong.

T3 (docs/design/PHILOSOPHY.md:64): what remains generic is a struct of three
fields and three ORs, which the samples write as one `||` expression. The
public-face test (docs/design/PHILOSOPHY.md:792-798) asks whether a client
would have to write it itself. A client writes `pressed(a) || pressed(b)` with
or without an engine type, because which sources join, and how, is the
client's to say. The decline does not rest on "only one client asks", which
that passage forbids. It rests on two clients answering the combination
question differently.

The measured evidence is LineSweeper's own design note
(samples/linesweeper/README.md:723-760). The threshold was named before the
measurement: 60 or more lines, or an engine header having to change, would
overturn the refusal. The pad then cost 26 lines with "no engine header
changed". Touch arrived later at about the same size, and "no row of the first
two tables changed". The cost is per device, not per binding.

The same passage of The public face also says "a decision resting on a
measurement has to be re-taken when the measurement moves, not re-cited"
(docs/design/PHILOSOPHY.md:797-798). The measurement has moved, because a
client now rebinds. Re-taken, the absence still holds, but its grounds are the
boundary table and the disagreement between combination rules, not the lack
of a client. If every sentence that gives the old reason is not amended, the
decline itself rests on a false sentence.

**The change.** Documentation only, with no API change. Every file below is
amended in one commit.

- `engine/input/gamepad.h:9-16`. Replace the paragraph. It must not quote a
  boundary row that no longer exists, and it must not give the old reason.
  The text is published (website/src/lib/published.ts:187), so it reads as
  contract. A sketch:

  ```cpp
  // WHAT IS DELIBERATELY ABSENT: an action map. PHILOSOPHY's boundary table
  // gives the engine the input devices and the game which actions exist,
  // their bindings and any rebinding, and a table from a named action to a
  // button is the game's half however it is spelt (T1). The engine's half is
  // the edge functions here and in keyboard.h and mouse.h: each compares two
  // snapshots of one source, so an edge depends only on what was down, never
  // on which binding asked, and changing a binding never invents a press or
  // loses one.
  //
  // COMBINING SOURCES IS THE GAME'S CHOICE, and the two obvious rules give
  // different answers. OR-ing each source's pressed() fires once per source
  // that goes down, so a key and then a button pressed for one action are two
  // presses. Rebuilding the edge from the combined held level fires once,
  // until every source is up again.
  ```

- `engine/input/keyboard.h`, near the edge functions (:149-185). Add one
  sentence saying that `pressed(now, before, a) || pressed(now, before, b)`
  combines independent edges, and that a game wanting an edge of the combined
  level computes held first. Or point to gamepad.h's paragraph instead of
  repeating it.
- `docs/design/ARCHITECTURE.md:176-179`, in the `input/` entry of the tree.
  Replace "neither client has a rebinding screen, so a binding table would be a
  speculative framework (T1)" with the boundary-table reason. Bindings and any
  rebinding are the game's (PHILOSOPHY, The boundary), and a client that
  rebinds keeps its own table (T1).
- `AGENTS.md:634-635`, Known-absent. Give the same reason, and add that a
  client now rebinds a key and still owns its table, and that report 21 was
  declined on those grounds. Change only AGENTS.md; CLAUDE.md imports it.
- `samples/linesweeper/README.md:726-728`. The note cites "AGENTS.md,
  Known-absent" and calls the question one of a "second client that would
  justify one". Point it at ARCHITECTURE's input entry and PHILOSOPHY's
  boundary instead, and say that this sample's `held()`-only reading is one of
  the two combination rules a shared layer would have to choose between. Keep
  the measurement paragraphs (:730-760) as they are.
- `docs/product/STORIES.md:3008`, SRL-06's acceptance criterion. SRL-06 is an
  open story (docs/product/BACKLOG.md:422), and its criterion currently
  requires the README to keep "the reason (neither client has a rebinding
  screen)". That would order a later agent to restore the false reason. Amend
  it to keep the re-taken reason. Leave the dated lines at
  docs/product/STORIES.md:136 and :2668 as they were written.
- `docs/port/android.md:285-291` and `:655`. The plan quotes the old reason
  word for word and expects touch and a pad to promote the absent item into a
  required one. Say that the decision was re-taken, on the boundary table,
  when answering report 21, and that the Android port re-argues it rather than
  inheriting either version. The same lines still say `CLAUDE.md` where they
  mean AGENTS.md, which `0c15f34` missed.
- `docs/port/web.md:791`, decision P2. "Until a second client needs one (T1)"
  sets a re-take trigger by count, which docs/design/PHILOSOPHY.md:792-798
  refuses. Restate it on the boundary table. P2's link to
  `play_state.cpp#L46-L54` has also drifted; the key table now starts at
  samples/linesweeper/states/play_state.cpp:54.
- `TODO.md:142`, PORT#2. It says the Android pad seam brings "the action
  mapping". Reword it to say the port re-takes report 21's decline, with
  android.md's argument recorded there, rather than presuming the layer.
- `docs/product/BACKLOG.md:398`, PI-10. Record report 21 as declined, citing
  docs/design/PHILOSOPHY.md:449 and T1. Tick the box only once every filed
  report is decided. While there, PI-10's own trade-off line cites
  PHILOSOPHY.md:752-757 for The public face, which is now :792-798
  (docs/product/STORIES.md:1629).

`website/src/content/docs/docs/guides/input.mdx:86` already states the absence
without the false reason and needs no change.

**Tests.** None are required, since behaviour does not change. What the
decline relies on is already pinned in `tests/input/keyboard_tests.cpp`
(`InputTests`): "an edge is down now and up last frame" (:119), "holding a key
is not pressing it again" (:131) and "an edge requires focus on both frames"
(:144), with the gamepad edge and trigger-threshold cases in
`tests/input/gamepad_tests.cpp`. One optional case pins the report's first
requirement directly: a key held across two focused frames gives
`pressed(now, before, key) == false`, whichever key the caller asked about the
frame before. It documents that edges carry no binding history. Both edited
headers are published, so the commit must pass the website build
(`.github/workflows/website.yml`) as well as the configure-time
`check_doc_citations.cmake`. Every T-number and document the new comments name
has to resolve. Building `x64-debug` and `wasm-debug` is enough to show the
headers still compile.

**Consumer-visible.** No. Only comments and documents change. The commit
touches `engine/**/*.h`, so its message says `not consumer-visible`. No name,
behaviour or compile option changes, and `samples/minimal/` is untouched.

**What the client could delete.** Nothing. It keeps its binding evaluator on
purpose, and this decline is the record it asked for. The way it merges
sources, how it samples each one, its two held-before rules, its trigger
thresholds and its binding tables are all its own policy. Once report 13
lands, only the token column of its saved-preference table changes, and that
belongs to report 13.

**Open questions.**

- If a third client, or a second rebindable action in ColourWars, turns out
  to need the same evaluator unchanged, the route is PHILOSOPHY's rule for UI
  compounds (docs/design/PHILOSOPHY.md:603-609). That would be a
  `samples/components/` piece that games copy, a per-action sample plus an OR
  merge with its tests, moved into the engine only if every copier keeps it
  unchanged. It would let the semantics be tried without an engine promise.
  Noted here, not proposed (T1).
- The Android port (TODO.md:142, PORT#2, deferred) is where this decision is
  next re-taken, against android.md's argument that two device shapes for the
  same actions make the layer necessary. It should be re-argued there, not
  quietly reversed.
- The stale quotation in gamepad.h survived the amendment `734c836` made to
  PHILOSOPHY. Decide whether the next edit to the boundary table should grep
  for quotations of it, and record that wherever such checks are kept.
