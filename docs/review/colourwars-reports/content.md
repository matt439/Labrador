# Audio, JSON and content staging: reports 6, 12, 15

Three of ColourWars' reports ask the engine for something it already claims
and has not yet reached. PHILOSOPHY's boundary gives the engine "Audio
playback and mixing" and "Asset loading, registries, JSON parsing"
(docs/design/PHILOSOPHY.md:450-451), and ARCHITECTURE says a game's
`CMakeLists.txt` "puts the content beside the program"
(docs/design/ARCHITECTURE.md:566-570). Report 6 is the mixing the engine does
not yet do, report 12 the parsing it will only do from a file, and report 15
the content staging the template teaches. None of them touches a render
backend or `app/window.h`, so they are three independent pull requests that
can land in any order. Two threads join them. Reports 6 and 15 both rest on
the optional sound bank: `SoundBank::silent()` replaces a bank the manifest
marks optional only when its file is absent at run time, which a stale staged
copy defeats (15), and a silent bank has
to take group calls the way it takes every other call, with the handle checked
above the content check (6). Both cite the *WHAT IT COSTS* paragraph at
engine/audio/sound_bank.h:85-92 and do not restate it. The key-enumeration
story split out of report 12 has its strongest caller inside the engine: the
manifest loader's `optional` flag, the one report 15's recipe relies on,
cannot reject a misspelt key. The suggested order is 12's `parse_json` first
(smallest, no open measurement), then 15's documentation, then 6, which has
the most to settle. Line numbers are as of `0c15f34`. This is a decision
record written ahead of PI-09 filing the reports and PI-10 closing them
(docs/product/STORIES.md:1594, :1614). It is not an implementation; the
README's table says what has been built since.

PI-10's third acceptance line asks that an implemented answer land with "an
entry in the changes list" (docs/product/STORIES.md:1624). CHANGES.md's
definition, which every commit is held to, says a new public name is not
consumer-visible and is not listed (CHANGES.md:12-14). Both engine changes
below add names only, so the two rules conflict. This record settles it in
CHANGES.md's favour: the commit message says `not consumer-visible`, and the
PI-10 record names that commit as the answer. PI-10's line should be amended
to "an entry in the changes list where CHANGES.md's definition asks for one"
when PI-10 is closed. PI-10's trade-off line also cites the public-face
passage as `PHILOSOPHY.md:752-757`. At this commit that passage is
docs/design/PHILOSOPHY.md:792-798 ("a predicate a game needs belongs in the
engine, at whatever count"), and the citations below use those lines.

## 6. Named sound groups

**Decision.** Accept, narrowed. Add named sound groups whose volume
`SoundBank` multiplies in above the seam. A wave or effect is assigned its
group once, at load. Changing a group's volume re-applies it to that group's
retained voices. A one-shot already playing keeps the level it launched with.
There is no master bus, fade, ducking, listener or voice cap.

**What the engine has now.** Every level is chosen per call by the emitter,
and the engine mixes nothing.

- The seam has per-call and per-voice volume only: `play_wave`,
  `play_voice` and `set_voice_volume` among the per-voice verbs, and no gain
  or group verb anywhere in the class (engine/audio/audio_device.h:127-142).
  It has `wave_index` but no wave count (engine/audio/audio_device.h:119).
- A one-shot has no handle "because it has nothing anybody can do to it
  afterwards" (engine/audio/audio_device.h:84-85), and `SoundBank::play_wave`
  returns `void` (engine/audio/sound_bank.h:125-126).
- `play_effect` and `set_effect_volume` act on one named persistent effect
  (engine/audio/sound_bank.h:131-136, engine/audio/sound_bank.cpp:96-107,
  :138-148). Both are `const`.
- The clamp is engine arithmetic above the content check, and the order
  inside each method is part of the contract: clamp, then handle check, then
  content check (engine/audio/sound_bank.h:32-38,
  engine/audio/sound_bank.cpp:70-94, docs/design/ARCHITECTURE.md:492-496).
- `SoundBank::SoundEffect` holds only the voice handle
  (engine/audio/sound_bank.h:56-59). Its only construction is positional, in
  `build_sound_bank` (engine/assets/sound_bank_loader.cpp:122-125).
- `SoundBank`'s constructor takes a device, a wave-bank handle and the effect
  registry, and nothing that says how many waves there are
  (engine/audio/sound_bank.h:69-71).
- A silent bank has an empty effect registry, answers `EffectHandle(0)` for
  every effect name and `WaveHandle(0)` for every wave name
  (engine/audio/sound_bank.cpp:25-29, :36-45, :60-68). It has no record to
  store a per-effect group on, and one wave index for every wave.
- `AudioResources` is only a bank registry (engine/audio/audio_resources.h:14-42),
  and it never releases a bank before its destructor
  (engine/audio/audio_resources.h:34-38). `Registry` has no iteration
  (engine/core/registry.h). `Registry::get` hands back a non-const pointer
  from a const method (engine/core/registry.h:98).
- `Application` exposes `audio_resources()` publicly and keeps the device
  private (engine/app/application.h:237, :282).
- The null backend records each requested volume
  (engine/audio/null/recording.h:70-104). It records requests, not loudness
  (engine/audio/null/recording.h:36-43).
- Waves and effects are resolved from the same bank, and nothing in a bank or
  its definition records a category, so a per-bank multiplier could not
  separate music from effects in one bank.

**Why.** The design documents already give the engine this mechanism. The
boundary row is "Audio playback and mixing | Which sounds play, and when"
(docs/design/PHILOSOPHY.md:450), the tree says `audio/` is "playback, mixing"
(docs/design/ARCHITECTURE.md:187), and the boundary diagram repeats it
(docs/design/ARCHITECTURE.md:605). T9 says where the arithmetic goes: "Build
the mixer, buy the audio backend" (docs/design/PHILOSOPHY.md:165-166). The
documents describe the target, so this change moves the code toward them and
does not widen them.

PI-10 and the gap register frame report 6 as an "audio bus against T1", a
settled position a decline would have to cite
(docs/product/STORIES.md:1623, docs/product/GAP-REGISTER.md:1504). No design
document settles it. Neither `docs/design/` nor `engine/audio/` uses the word
bus, submix or ducking, and the documents point the other way. What T1
settles is the scope: "Mechanisms are extracted from a real client game in
hand, and generalise only as far as real clients demand"
(docs/design/PHILOSOPHY.md:45-47). A gain per named group, assigned by the
game, is that mechanism. Master volume, fades, ducking, a listener and voice
caps have no exhibit and stay out. Which group a sound belongs to is the
game's choice, and so are its authored levels.

The evidence is one client with the workaround in hand: it multiplies a saved
user gain into every emitter by hand and re-applies it to each retained effect
after a preference change or a resume. The engine's own samples are not
second callers. `samples/audio/sound_board.cpp:58-62` re-pushes its single
`volume_` to the drone, but ping, chime and drone all take the same level
(samples/audio/sound_board.cpp:27, :32, :37-38), so nothing there multiplies an
authored level by a group gain. The components gallery's "Music volume" and
"Effects volume" sliders only set status text
(samples/components/gallery_state.cpp:103-121); they show that a game wants
the control, and nothing more. The count does not decide this anyway. The
public face says the question is whether a client would have to write the
mechanism itself (docs/design/PHILOSOPHY.md:792-798), and every game with a
settings screen would.

T3 puts the gain above the seam rather than in backend submix voices.
DirectXTK gives no handle to an in-flight one-shot, while Web Audio could
route one through a per-group `GainNode`. A gain below the seam would
therefore behave differently on `xaudio2` and `webaudio`, which is the drift
the backend-equivalence sweeps exist to catch. As arithmetic in `SoundBank`
the rule is identical on all three backends, can be asserted under
`audio/null/`, and matches what the audio guide already promises: "One-shots
take their levels when launched, so new settings affect the next one-shot"
(website/src/content/docs/docs/guides/audio.mdx:111-112). T8 is met: a play
costs one indexed load and one multiply more, and the re-application loop
runs only when a settings value changes, never per frame.

**The change.** Files: `engine/audio/audio_resources.h/.cpp`,
`engine/audio/sound_bank.h/.cpp`, optionally forwarding in
`engine/audio/sound_bank_object.h/.cpp`, and the tests. Nothing in `xaudio2/`,
`webaudio/` or `null/` changes, and `audio_device.h` does not change.

(1) `AudioResources` owns the group table, because a group spans banks and
`AudioResources` is the one cross-bank object the shell already exposes.

```cpp
// audio_resources.h
struct SoundGroup;   // a tag, never defined, like SoundBank::Wave
using SoundGroupHandle = Handle<SoundGroup>;

// Load-time. Throws std::invalid_argument naming the group if one of that
// name was already added.
SoundGroupHandle add_sound_group(const std::string& group_name);

// Load-time. Throws std::out_of_range naming the group if none was added.
SoundGroupHandle resolve_sound_group(const std::string& group_name) const;

// Clamped to [0,1]. Re-applies to every retained effect in the group,
// across every bank this holds; a one-shot already playing keeps its level.
// Throws std::out_of_range through an unresolved handle.
void set_group_volume(SoundGroupHandle group, float volume);
float group_volume(SoundGroupHandle group) const;
```

The table is a private vector of `{ name, volume }` with a name-to-index map.
`NameTable` (engine/core/name_table.h) does not fit as it stands, because it
replaces a repeated name silently and is const after loading. Because
`Registry` cannot be iterated, `AudioResources` also keeps a
`std::vector<SoundBank*>` of the banks it added. Those pointers are stable
because no bank is released before the destructor
(engine/audio/audio_resources.h:34-38). `add_sound_bank` lends the bank a
`const` pointer to the group table. `AudioResources` owns both the table and
the banks, so the loan is lexical, and the member declaration says so
(docs/design/PHILOSOPHY.md:515-516).

(2) `SoundBank` takes its assignment at load time, so emitter call sites do
not change.

```cpp
// sound_bank.h
void set_wave_group(WaveHandle wave, AudioResources::SoundGroupHandle group);
void set_effect_group(EffectHandle effect, AudioResources::SoundGroupHandle group);
```

`sound_bank.h` cannot include `audio_resources.h`, which includes it. So
`SoundGroup` and `SoundGroupHandle` live in a small header of their own under
`engine/audio/` (for example `sound_group.h`), which both include. That header
is new and public, so it goes into `website/src/lib/published.ts` in the same
commit.

- Waves: a private `std::vector<SoundGroupHandle> wave_groups_` grown in
  `set_wave_group` to the handle's index. `play_wave` reads the entry when the
  index is in range and treats the wave as ungrouped otherwise. That needs no
  wave count, so the constructor and `build_sound_bank` stay as they are. If
  the implementer would rather size it up front, the count has to come from
  `definition.waves.size()` through a new constructor argument fed from
  engine/assets/sound_bank_loader.cpp:128-129. That would change a public
  constructor, which the lazy vector avoids.
- Effects: `SoundEffect` gains `SoundGroupHandle group;` and
  `float volume = 1.0f;`, the last local level requested through
  `play_effect` or `set_effect_volume` after clamping. Both need default
  member initialisers, so that the positional `SoundEffect{ voice }` at
  engine/assets/sound_bank_loader.cpp:124 still compiles. The other choice is
  a private table parallel to the registry, which leaves the public struct
  alone but needs an effect count the bank does not have.
- The effective level is `clamp(local) * group_volume(group)`. An ungrouped
  sound has gain 1.0, so every existing call behaves exactly as before.
- Order inside each method: clamp, then the play handle check, then the
  group handle check, then the content check. A group handle with no table
  entry throws `std::out_of_range` above the content check, as every other
  handle does (engine/audio/sound_bank.h:94-98, :130).
- Silent bank: `set_wave_group` and `set_effect_group` check the group handle
  and then do nothing. A silent bank has no record to store an assignment on
  and only one wave index for every wave, so the header must say "validates
  and discards", not "stores".
- A bank that was never added to an `AudioResources`, such as one built in a
  test or by a client that drives `AudioDevice` directly as the audio guide
  allows (website/src/content/docs/docs/guides/audio.mdx:117-119), has no
  group table. `set_*_group` on it throws `std::logic_error` saying the bank
  belongs to no `AudioResources`. Its ungrouped plays are unaffected.
- `play_effect` and `set_effect_volume` stay `const`, because existing callers
  hold a `const SoundBank&`. Each writes the record's stored local level
  through `Registry::get`'s non-const pointer (engine/core/registry.h:98), and
  the header says so. It also says that audio calls are `update()`-side,
  because `draw()` is entered concurrently on the same object under the view
  fan-out.

(3) The header contracts to write, as contract only, because published headers
are reproduced word for word on the website. A group volume affects one-shots
launched after the change and retained effects at once. A one-shot already in
flight keeps its level, said in those words. A paused effect takes the new
level now, through `set_voice_volume`, which is already a legal seam call on a
paused voice, and plays at it when resumed. Fades, ducking, authored levels
and which group a sound belongs to are the game's (T1).

(4) Documents. Add a paragraph on groups to the audio guide's *Levels and
lifecycle* section (website/src/content/docs/docs/guides/audio.mdx:103-115),
next to the one-shot sentence at :111-112, which states the same in-flight
contract and should keep doing so. The guide quotes `samples/audio/sound_board.cpp`
from `void SoundBoard::set_volume` to `}`, so if SoundBoard moves to a single
group to exercise the feature, the quoted method has to keep that shape or
the guide has to change in the same commit. Wiring the gallery's two sliders
to real groups is optional, and would make the gallery a caller.

Platforms: no render backend and no window is involved. It must build clean
on MSVC (`x64-debug`, which compiles `xaudio2/`, and a `-null` preset) and on
clang `-Werror` (`wasm-debug`, `wasm-release`). `cmake/settings.cmake:42`
passes `-Wextra`, which enables `-Wmissing-field-initializers`. Recent clang
does not warn about a field that has a default member initialiser, but only a
`wasm-*` build will show whether the positional `SoundEffect{ voice }` stays
quiet.

**Tests.** In the same commit:

- `tests/audio/sound_bank_tests.cpp` and `tests/audio/audio_resources_tests.cpp`
  (`AudioTests`, every configuration): a duplicate `add_sound_group` throws
  naming the group; `resolve_sound_group` throws naming a missing group;
  `set_group_volume` clamps; an unresolved group handle throws above the
  content check, including on `SoundBank::silent()`; `set_*_group` on a
  silent bank validates and stores nothing; `set_*_group` on a bank with no
  `AudioResources` throws `logic_error`.
- `tests/audio/null_tests.cpp` (`AudioTests` under `x64-debug-null`,
  `x64-release-null`, and CI's null-configured `wasm-release`): `play_wave` of
  a grouped wave records local times group; an ungrouped wave records the
  local level unchanged; `set_group_volume` records one `set_voice_volume` per
  playing or paused effect in that group across two banks, in bank-then-effect
  order, and none for other groups; `play_effect` and `set_effect_volume`
  after a group change record the product; a paused effect's level is pushed
  again.
- `tests/audio/webaudio_tests.cpp` (`AudioTests` under `wasm-debug` and
  `wasm-release`, in Node): the same calls reach the device without throwing.
- `AudioSampleTests` stays green, and `RenderPixelTests` is unaffected.

**Consumer-visible.** Not consumer-visible under CHANGES.md:4-14. The change
adds new names, and an ungrouped sound keeps gain 1.0, so behaviour does not
change silently. The commit message says `not consumer-visible`. Two parts of
that judgement need checking first. `SoundEffect` is a public nested struct,
and adding fields with initialisers keeps `SoundEffect{ voice }` compiling. A
new `sound_group.h` is a new header, not a moved one. The template does not
use audio, so `samples/minimal/README.md` needs no line.

**What the client could delete.** The user-gain multiplication at every
emitter, and the code that re-applies the user gain to each retained effect
after a preference change or a resume. Both become one `set_group_volume` per
group. Client policy that stays: storing and persisting the preference,
deciding which sounds are music and which are effects (now a handful of
`set_*_group` calls at load), authored levels, music fades and pause ducking.
Ducking can keep multiplying its own factor into `set_effect_volume`, which
composes with the group gain.

**Open questions.**

- Should `set_group_volume` also push to stopped effects? This record says
  no, because a stopped effect takes its level at its next `play_effect`, but
  whether every backend accepts `set_voice_volume` on a voice that has never
  played is not checked.
- A group handle from a different `AudioResources` cannot be told apart from
  a valid index. The header should say that a handle belongs to the
  `AudioResources` that issued it.
- Group assignment could later be data in the bank definition, since T7 says
  data decides "which". Start in code, as the report asks, and move to data
  only if a second client asks for it.
- A master volume (`AudioEngine::SetMasterVolume`, a destination `GainNode`)
  would also reach in-flight one-shots, but it is below the seam and nobody
  has asked for it. Leave it out.
- The client has also raised a listener model and a voice budget. They are
  unfiled and this decision does not cover them.
- Nothing here measures loudness. "Louder" is asserted only as the recorded
  arithmetic.

## 12. JSON from memory, key enumeration, source spans

**Decision.** Split three ways. `parse_json(text, source_name)` is
accepted and answered with a commit, sharing one private parse path with
`read_json_file`. Key enumeration is accepted as a separate story, argued from
the engine's own loaders, which cannot reject a misspelt key. Keeping raw
source spans is declined.

**What the engine has now.**

- The only public way to construct a `JsonDocument` is
  `read_json_file(const char* path)` (engine/assets/json.h:16). The constructor
  is private and that function is its only friend (engine/assets/json.h:110-113).
- `read_json_file` reads the file into a local string, records the path as
  the diagnostic source, and parses (engine/assets/json.cpp:36-77; the source
  is set at :65 and `Parse(text.c_str(), text.size())` runs at :66). That call
  is `Parse`, not `ParseInsitu`, so the document does not depend on the text
  buffer after the parse.
- Diagnostics name the source and the key or index context:
  `where()` prints `'<source_path>': <context>` (engine/assets/json.cpp:87-92,
  engine/assets/json.h:56-60). `JsonValue` keeps a `const std::string*` into
  the document's `Impl` (engine/assets/json.h:82-83), and
  `JsonDocument::source_path()` is described as "The path this was read from,
  as it was given to read_json_file" (engine/assets/json.h:106-107). The
  `JsonValue` comment says accessors throw "naming the file"
  (engine/assets/json.h:20-21).
- Arrays have `size()` and `at()` (engine/assets/json.h:48-50). Objects have
  only keyed access and `has()` (engine/assets/json.h:34-46), with no way to
  list their members.
- `tests/assets/json_tests.cpp:13-15` writes temporary files only to reach
  the parser, and says so: "there is no in-memory door in, and adding one for
  the tests would be a second parse path to keep honest".
- Engine loaders read optional keys through `has()`, so a misspelt key
  silently takes the default: `optional` in the manifest
  (engine/assets/asset_manifest_loader.cpp:32-33), and `origin` and `rotated`
  in sprite sheets (engine/assets/sprite_sheet_loader.cpp:45, :71). The sound
  bank's `create_effect_instance_for_each_wave` is read as required through
  `boolean()` (engine/assets/sound_bank_loader.cpp:60), so a misspelling there
  already throws. It is not an unknown-key case.

**Why.** In-memory parsing is accepted. The boundary gives the engine "Asset
loading, registries, JSON parsing" (docs/design/PHILOSOPHY.md:451), and T9
buys rapidjson for the format edge and builds the checked wrapper on it
(docs/design/PHILOSOPHY.md:157-159). Bytes the caller already holds are input
to the same mechanism. They are not a new policy, and `parse_json` still only
reads, so ARCHITECTURE's "`assets` only reads" (docs/design/ARCHITECTURE.md:404)
is not touched. STORIES frames report 12 as "JSON write against the read-only
asset asymmetry" (docs/product/STORIES.md:1623). That framing fits the spans
half and does not fit this one. T6 asks for failures "with names and paths"
(docs/design/PHILOSOPHY.md:111-112), meaning the right ones. A diagnostic that
names a scratch file the client wrote only to reach the parser points at the
wrong place. The objection in `json_tests.cpp`, that this adds a second parse
path, is answered by construction: `read_json_file` becomes file IO plus the
same private parse function `parse_json` calls.

Key enumeration is accepted and split into its own story. Recording it as
"needs more evidence" would not do, because that is not one of PI-10's three
outcomes (docs/product/STORIES.md:1622). And the evidence is already in the
tree. Engine loaders that cannot refuse a misspelt optional key are a T6
defect of the engine's own: "Loud is for broken contracts"
(docs/design/PHILOSOPHY.md:121-122). The public face says "a predicate a game
needs belongs in the engine, at whatever count"
(docs/design/PHILOSOPHY.md:792-798). Rejecting unknown keys in the engine's
loaders is the story, and enumeration is its mechanism. The client's
consumed-key check is a second caller of the mechanism, not the reason for it.

Source spans and source-preserving splicing are declined. They rest on
ARCHITECTURE: "`assets` only reads" (docs/design/ARCHITECTURE.md:404). They
also rest on PHILOSOPHY's "There is no hot-reload machinery"
(docs/design/PHILOSOPHY.md:815) and its permanent non-goal "An editor"
(docs/design/PHILOSOPHY.md:832). rapidjson's DOM keeps no value offsets, so
spans would need a second parser or SAX bookkeeping. T9 reserves the bought
edge for rapidjson (docs/design/PHILOSOPHY.md:157-159), and T1 rules out a
speculative framework (docs/design/PHILOSOPHY.md:45-47). Preserving and
splicing source text is a debug tool's policy, and the client is the right
place for it. No document's stated reason has gone false, so the decline
amends nothing. The client treats enumeration and spans as separate, unfiled
asks. PI-09 files the `parse_json` slice, and this record holds the
other two answers so that a later filing finds them already decided.

**The change.** Only `parse_json` is built in this commit.

```cpp
// engine/assets/json.h, beside read_json_file; add #include <string_view>

// Parses `text` as JSON. `source_name` is what every diagnostic from the
// document names in place of a path - a content filename, say. The document
// copies what it needs; neither argument is borrowed.
//
// Throws std::invalid_argument for an empty source_name, and
// std::runtime_error naming source_name if the text does not parse.
JsonDocument parse_json(std::string_view text, std::string source_name);
```

- Make it a second friend of `JsonDocument` (engine/assets/json.h:110).
- In `json.cpp`, add one function in the anonymous namespace that takes the
  bytes, their length, the source string and the caller's message prefix,
  fills an `Impl` and throws on a parse error. Spell the types out:
  `std::unique_ptr<JsonDocument::Impl>`, not `auto`.
- `read_json_file` keeps its messages byte for byte
  (`read_json_file - '<path>' is not valid JSON: ... (offset N)`,
  engine/assets/json.cpp:69-74), so existing assertions do not move.
  `parse_json` reads `parse_json - '<name>' is not valid JSON: ...`.
- The empty-name `invalid_argument` is a rule `read_json_file` does not have,
  since an empty path fails at open as a `runtime_error`. The header states it.
- Change the comments that would read false on the published reference page:
  `source_path()` (engine/assets/json.h:106) becomes "the path given to
  read_json_file, or the name given to parse_json"; `JsonValue`'s "naming the
  file" (:20) and the `source_path_` member's "only ever read to name the
  file" (:82) become "the source". Do not rename `source_path()`. That would
  be a source break for nothing, and the comment covers the T4 cost.
- Rewrite the `TempJson` comment at tests/assets/json_tests.cpp:13-15 in the
  same commit, or it becomes a false statement. `TempJson` stays for
  `read_json_file`'s file-specific cases (missing file, null path).
- Update the hand-written API list in the assets guide's *Reading JSON*
  section with one line (website/src/content/docs/docs/guides/assets.mdx:49).
- Platforms: no render or audio backend and no window. It must build on MSVC
  and on clang `-Werror`, and `AssetsTests` runs under Node on `wasm-*`.

The enumeration story, to be written as its own BACKLOG entry and not built
here, covers a `JsonValue` accessor for an object's member names (a noun:
`keys()`, or `member_count()` with `member_name(index)`), and unknown-key
rejection in `asset_manifest_loader.cpp` and `sprite_sheet_loader.cpp`.

**Tests.** `tests/assets/json_tests.cpp` (`AssetsTests`, on `x64-debug`, one
`-null` preset and `wasm-debug` under Node):

- `parse_json` of valid text gives the same root values as `read_json_file`
  of the same bytes.
- The document survives the destruction or overwriting of the caller's text
  and name buffers.
- `where()` and a typed-accessor failure name `source_name` and the key or
  index context.
- Invalid text throws `runtime_error` naming `source_name`, with rapidjson's
  message and offset.
- An empty `source_name` throws `invalid_argument`.
- A moved-from document and the document moved into both keep `source_path()`
  valid for the `JsonValue`s taken out of them.
- `parse_json` and `read_json_file` agree on the same bytes for a leading
  UTF-8 BOM, for text with a NUL after the root followed by junk, and for a NUL
  inside a string. This pins agreement between the two, not a particular
  outcome. rapidjson's length-taking parse reads through a stream that answers
  `'\0'` at the end, and its trailing-content check tests for `'\0'`, so an
  embedded NUL after the root may well be accepted. Whatever it does,
  `read_json_file` already does it today.
- `read_json_file`'s existing message assertions pass unchanged.

**Consumer-visible.** Not consumer-visible: one new public function in an
existing header, and `read_json_file`'s behaviour and messages are
unchanged. The commit message says `not consumer-visible`. `json.h` is already
in `published.ts`, so the reference page regenerates from the header. The
later enumeration story is a different matter. Unknown-key rejection turns a
silent default into a throw, which is a silent behaviour change under
CHANGES.md:10-12 and needs an entry there. If the minimal sample's manifest
could be affected, it also needs a line in `samples/minimal/README.md`.

**What the client could delete.** Its debug tool's temporary file, written
only to reach `read_json_file`, along with the read-back, the clean-up and the
rewriting of the scratch path into the logical filename in its diagnostics.
Client policy that stays: its validation and adoption rules, its
source-preserving document and splice, save and conflict handling, and its
consumed-key check. That check needs enumeration and also needs the client's
own knowledge of which fields it accepts.

**Open questions.**

- Should `parse_json` take a `std::string` so that a caller can move the text
  in? Not needed: the parse copies, and `string_view` is the simpler
  signature.
- For the enumeration story: should unknown-key rejection go in each loader,
  or in one helper the loaders share? Decide that before designing the
  accessor, because it decides whether the accessor is a list of names or a
  "reject everything except these" check.
- rapidjson's behaviour on embedded NULs could not be confirmed from its
  source on this machine. The agreement test makes the question moot for
  `parse_json`, but whoever implements this should read
  `reader.h`'s root-not-singular check once.

## 15. Content staging

**Decision.** Document only. The engine gets no CMake staging helper: the
template's always-run copy target is the documented recipe. The template's
comment and the assets guide gain the one fact they are missing, that
`copy_directory_if_different` never deletes, together with a short
list-and-remove recipe. PI-11's runtime-install question is a separate one and
stays undecided.

**What the engine has now.**

- `engine/app/content_root.h` owns resolving content relative to the
  executable: `executable_directory()` (engine/app/content_root.h:41) and
  `resolved_under()` (:46).
- The template's build file says "A release a player installs needs install
  rules as well, which this file does not have" (samples/minimal/CMakeLists.txt:6-7).
  Its always-run content target is explained at :49-53 and spelled at :54-67,
  with `copy_if_different` for the manifest and `copy_directory_if_different`
  for `fonts/`. Its browser branch embeds the content directory with
  `--embed-file` (:34) and names the inputs in `LINK_DEPENDS` (:28-32).
- No tracked CMake has `install()`, `export()`, CPack or a shared
  content-staging function. `cmake/` holds settings, shaders and the checks.
- All six samples spell their own always-run target:
  samples/minimal/CMakeLists.txt:54, samples/audio/CMakeLists.txt:46,
  samples/collision/CMakeLists.txt:44, samples/components/CMakeLists.txt:35,
  samples/linesweeper/CMakeLists.txt:167 and
  samples/local_multiplayer/CMakeLists.txt:51. None uses file lists, an
  optional asset or removal of stale files. The one sound-bank manifest in
  the samples marks its bank `"optional": false`
  (samples/audio/content/manifest.json:12).
- The hazard is real. `copy_directory_if_different` adds and updates and
  never removes, so a file deleted from `content/` stays in the build folder.
  An optional bank that is absent at run time is replaced by
  `SoundBank::silent()` (engine/assets/resource_loader.cpp:138-193, the
  substitution at :187), so a stale mirror hides the absence during
  development. The same applies to required content. A file the manifest
  still names but that has been deleted from `content/` keeps loading in
  development and fails only in a clean build or a release. That hides a T6
  failure as well.
- The content-root rule has already moved once, as a silent behaviour change
  (CHANGES.md:303-304, `92966b0`).

**Why.** This rests on ARCHITECTURE, *A game project*: "There is no project
file format, no code generation, no module manifest — the `CMakeLists.txt`
names the sources, links the engine and the shared settings, and puts the
content beside the program ... Starting a game is copying `samples/minimal/`,
renaming the target, and owning everything inside"
(docs/design/ARCHITECTURE.md:566-574). The staging target is part of what the
template hands over to be owned. It is not engine API. It also rests on T1's
"Not a licence for: speculative frameworks"
(docs/design/PHILOSOPHY.md:45-47). The mechanics the report wants extracted
are explicit file lists, failure on a missing required file and pruning of
missing optional ones, and most of the client's version is path-safety
checking of its own build tree. A generic function would have to either
impose that policy or drop it. T3 and T4 favour a recipe of about ten lines
that a reader can see over a CMake function whose argument grammar they have
to learn.

This decline does not rest on how many clients ask. PI-10's trade-off line
forbids that ground (docs/product/STORIES.md:1629;
docs/design/PHILOSOPHY.md:792-798), and the question it puts instead is
whether a client would have to write the mechanism itself. The answer has two
parts. Where the content must land is engine knowledge, but the template
states it in one comment (samples/minimal/CMakeLists.txt:49-51), and
CHANGES.md records it when it moves. On the install side, PI-11 already
carries the requirement that files land "at the path the engine's
content-root resolution reads" (docs/product/STORIES.md:1644), and that is
where the engine-knowledge argument belongs. Which files are required, which
are optional and how carefully paths are checked is the game's policy, and a
recipe is enough for a game to write it.

The report does expose a gap the engine owns. The template teaches a pattern
that quietly defeats the engine's own optional-bank rule
(engine/audio/sound_bank.h:85-92) and hides missing required content, which
works against T6 (docs/design/PHILOSOPHY.md:121-122). That is a defect in
documentation the engine ships, and the fix is a sentence and a recipe. The
template's comment is incomplete rather than false, so no design document
needs amending.

**The change.** One commit, documentation only.

(1) `samples/minimal/CMakeLists.txt`: extend the comment at :49-53 with two
sentences. Leave the lines `add_custom_target(sample_content ALL)` (:54) and
`add_dependencies(MinimalSample sample_content)` (:67) exactly as they are,
because the assets guide quotes the file from the first to the second
(website/src/content/docs/docs/guides/assets.mdx:37). Suggested text:

```cmake
# copy_directory_if_different adds and updates but never removes: a file
# deleted from content/ stays in the build folder until it is cleaned, so a
# required file goes on loading here and is missing only from a clean build,
# and an optional bank is never replaced by the silent one. A game with
# content it may delete lists its files instead (the assets guide, "Optional
# content and stale copies").
```

(2) In `website/src/content/docs/docs/guides/assets.mdx`, add a short
subsection, *Optional content and stale copies*, after
*Getting content beside the executable* (:31-40). It holds the recipe as
prose and a snippet, not as a tracked file. The checks have to run on every
build, not at configure time, so the always-run target calls a script with
`cmake -P`, and an `if(EXISTS)` in the `CMakeLists.txt` itself would be
evaluated only once:

```cmake
# stage_content.cmake, run as:
#   cmake -DSOURCE=... -DDEST=... "-DREQUIRED=a;b" "-DOPTIONAL=c" -P stage_content.cmake
cmake_minimum_required(VERSION 3.26)  # a script sets no policies; IN_LIST needs CMP0057
foreach(file IN LISTS REQUIRED OPTIONAL)
    if(EXISTS "${SOURCE}/${file}")
        get_filename_component(parent "${DEST}/${file}" DIRECTORY)
        file(MAKE_DIRECTORY "${parent}")
        file(COPY_FILE "${SOURCE}/${file}" "${DEST}/${file}" ONLY_IF_DIFFERENT)
    elseif(file IN_LIST REQUIRED)
        message(FATAL_ERROR "required content '${file}' is missing from ${SOURCE}")
    else()
        file(REMOVE "${DEST}/${file}")
    endif()
endforeach()
```

The subsection also says that removal is limited to the files the list
names, and that the browser branch's `--embed-file` is the web equivalent,
where nothing goes stale because the module is relinked from the directory as
it stands. `file(COPY_FILE ... ONLY_IF_DIFFERENT)` needs CMake 3.21, and the
tree requires 3.26 (CMakeLists.txt:1).

(3) `samples/minimal/README.md`: one line at the top of the change list
(after :29) saying the build file now warns that the content mirror never
removes, and pointing to the guide's recipe. A copy taken earlier would want
that.

(4) PI-10's record: report 15's development-staging half is answered as
document only, citing ARCHITECTURE *A game project* and T1. PI-11 stays
conditional on its own evidence and is neither accepted nor dropped by this.

**Tests.** No behavioural test, because no mechanism ships.

- The website build (`npm run build`, `.github/workflows/website.yml`) must
  stay green, because `assets.mdx` quotes `samples/minimal/CMakeLists.txt` by
  its first and last lines.
- CI's `consumer` job (`tools/consumer_check/assemble.py`) builds the
  template, which behaves the same as before.
- `check_doc_citations.cmake` scans only `.h` and `.cpp` files, so a CMake
  comment is not checked. Make sure by reading it that the comment names the
  guide section correctly.
- If the recipe is ever made a tracked file, it needs a configure-time test.
  That is a reason to keep it as prose until a sample has an optional asset
  to exercise it.

**Consumer-visible.** None. No engine header, `engine/CMakeLists.txt`,
`cmake/settings.cmake` or `vcpkg.json` changes, so CHANGES.md gets no entry.
The `samples/minimal/README.md` change-list line in (3) is still owed, because
the template's build file changes.

**What the client could delete.** Nothing directly. Its stricter staging
function, with explicit lists, pruning and path-safety checks, is the version
that carries policy, and it stays. What changes is that upstream now records
that function as legitimately the client's, and the template stops teaching
the stale-mirror hazard to the next game. Release packaging stays the client's
and is PI-11's subject.

**Open questions.**

- When to reopen. The reopen condition is not a count of callers. It is a
  second move of the content-root rule, or PI-11 being accepted. In either
  case, staging and install should be designed together, because an install
  function would take the same file lists and a staging function would come
  almost for free. A possible spelling then is
  `labrador_stage_content(TARGET ... SOURCE_DIR ... FILES ... OPTIONAL_FILES ...)`
  in `cmake/`.
- The recipe has no in-tree caller and can drift. Whether one sample should
  adopt it (`audio`, if its bank were ever made optional) is for whoever next
  changes that sample.
- Does a decline that also ships a recipe meet PI-10's "declined with a cited
  PHILOSOPHY T-number or ARCHITECTURE section" (docs/product/STORIES.md:1622)?
  It does: the helper is declined, the citation is ARCHITECTURE *A game
  project* and T1, and the recipe is the answer that goes with the decline.
