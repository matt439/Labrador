# ArtAttack — Code Review, Round 2

> Read-only review by 100 agents, 2026-08-08. 430 tracked files, 259 first-party
> sources across `engine/`, `game/`, `samples/`, `tests/`, `cmake/` and the root
> build files.
>
> `external/rapidjson/` and the vcpkg-built DirectXTK/XAudio2 sources were **not**
> reviewed. They were opened as evidence — to settle what `RAPIDJSON_ASSERT`
> expands to, what `SpriteFont::Impl::ConvertUTF8` does, what
> `WaveBank::CreateInstance` returns on a miss — and every such citation names the
> vendored file and line so you can check it.
>
> The configured build tree under `out/` was also used as read-only evidence:
> compile flags from `build.ninja` and `CMakeCache.txt`, object timestamps, and
> `LastTest.log`. That is how several claims about what `/W4 /WX` does and does not
> diagnose were settled empirically rather than argued.
>
> **This document is the review as written, and is not updated as findings are
> fixed.** Round 1 is at [`../README.md`](../README.md); what it fixed is at
> [`../IMPLEMENTED.md`](../IMPLEMENTED.md). Full detail for every finding in this
> round: [`all-findings.md`](all-findings.md).

**901 findings across 56 groups — 13 critical · 165 high · 402 medium · 283 low.**

(The severity tally is taken over the 52 module and sweep reports. The four gap
reports contribute the remaining 38 findings to the 901 total; they are counted in
the group table below.)

40 of the most consequential claims were then attacked by eight adversarial
verifiers under distinct lenses — is it already fixed, is the mechanism real, is
the cost estimate honest, is the tenet cited correctly, would a steelman defend it.
**Thirteen came down one severity grade. None was refuted.**

---

## What was reviewed, and how

Three passes, in this order:

| Pass | Agents | What it did |
|---|---|---|
| **Module groups** | 32 | One group per coherent slice of the tree (`math-header`, `render-text`, `game-weapon`, …). Each read its assigned files in full plus everything they reach into. |
| **Module verification** | 32 | A second agent per group re-opened **every cited line**, re-derived every arithmetic claim, and re-ran every "zero callers" grep. It could reject, re-grade, correct or add. |
| **Repo-wide sweeps** | 20 | One question each across the whole tree: naming, includes, namespaces, value semantics, loud failure, threading, lifetimes, build, testability, docs accuracy, content data, and nine more. **Self-verified only.** |
| **Gap fill** | 4 | Coverage (what was never opened), structure (four end-to-end vertical slices), roadmap (what order to do the work in), adversary (audit of the review itself). |
| **Adversarial verification** | 8 | 40 load-bearing claims, each attacked under a named lens by an agent whose job was to kill it. |

The module verification pass was not decorative. It **rejected 21 claims outright
and added 111 findings the first agent had walked past**, several of them the
sharpest thing in their group. The added findings are in the 901.

### Confidence: two halves, two standards

This matters more than any single finding, so it is stated plainly.

- **The 32 module reports were verified by a second agent.** Treat their line
  numbers as checked. Where the verifier corrected a number, the corrected number
  is what appears here.
- **The 20 sweep reports were not.** They are one agent's reading, self-checked.
  They are where 7 of the 13 criticals live, and they grade critical-or-high at
  **31%** against the verified half's **15%** (`gap-adversary`). That gap is a
  property of who filed the finding, not of the code. Discount sweep severities by
  roughly one grade unless the finding also appears in a verified module report or
  in the adversarial list below.
- **The 4 gap reports were also self-verified**, but three of the four spend most
  of their length checking the other 52 rather than the code.

Two further honesty notes the review owes you:

- The triage pass's fifty duplicate clusters index a 178-finding list **that exists
  in no file**, so the review's own dedupe index is unusable as delivered. The
  cluster *titles* are still correct and are what the theme sections below are
  organised around.
- Splitting one 5,700-line type across a header agent and three implementation
  agents (`math-header`, `math-impl-a/b/c`) guaranteed duplicate reports of the same
  member. Nine such duplicates exist, three of them carrying **contradictory
  severities** (`gap-adversary`). Where that happened, this document states one
  grade and says which.

### The review checking itself

Rejected outright by the module verifiers — the sharpest ones, so you can see the
shape of what got through and what did not:

| Rejected claim | Verdict |
|---|---|
| "`State::context()` is dead in every game; `StateContext` offers a state nothing to call" (`samples-minimal`) | **False.** `this->context()->transition_to(...)` appears 20+ times across `game/states/`. The finding's entire evidence base collapsed; dropped. |
| "`set_dx()` has zero callers in the entire tree" (`core-objects`) | **False.** `game/objects/player.cpp:1033` calls it. Three further counts in the same finding were wrong (nine accessors → twelve, six-of-nine dead → seven-of-twelve). |
| "C++ requires `RegistryStorage` be specialised in the primary template's namespace, so CONVENTIONS closes the extension point to game code" (`core-registry`) | **False.** C++17 relaxed [temp.expl.spec]/2; this is C++20. A client specialises from its own scope. Regraded to a documentation gap, low. |
| "The fan-out's soundness rests on the captured loop counter `i` being unique" (`core-threading`) | **False.** `level.cpp:514-531` and `menu_page.cpp:175-181` index by *element*, not task. Rewritten around the real invariant, which is narrower and more fragile. |
| "`app` depending on `collision` inverts ARCHITECTURE's module table" (`collision-input`) | **False.** `ARCHITECTURE.md:138` gives `app` "everything", explicitly. |
| "`engine/render/` at least has the `d3d11/` folder reserved for this" (`audio`) | **False.** `engine/render/` is flat on disk; `d3d11/` exists only in ARCHITECTURE's tree diagram. Correcting it strengthened the finding. |
| "Projectiles pass through ramps without painting" (`game-projectile`) | **Half false.** `level_object_builder.cpp:130-163` builds `StructureRamp` as a plain `Structure`, so there is no paint to lose. The tunnelling half survives. |
| "`player_consts.h` uses `std::string` and `FLT_MIN` with no include, compiling only because of the PCH" (`game-player`) | **False.** Both arrive transitively through `colour.h` and `SimpleMath.h`. Include-what-you-use, not a PCH dependency. |
| "The saved resolution is not honoured because the process is DPI-unaware" (`game-shell`) | **Wrong mechanism.** The swap chain does render at the saved size and is then composited scaled. The real second cause — `CreateWindowExW` given the resolution as the *outer* rect with no `AdjustWindowRect` anywhere — was filed separately. |
| "`#pragma region` violates ARCHITECTURE's 'Nothing IDE-specific is committed'" (`game-main-menu`) | **Wrong citation.** That sentence governs build files, and the engine itself uses `#pragma region` in `widget.cpp` and `matt_math.cpp`. |
| "`enum class` would let `/W4` check switch exhaustiveness" (`game-projectile`) | **False.** Scoping changes nothing about MSVC's diagnostics; the `default:` label is what suppresses C4062 either way. |
| "The warm-up absorbs the huge `dt` produced by `build_and_enter_level()`" (`game-states-flow`) | **False.** `StepTimer` is fixed-step (`application.cpp:71`), so `dt` is always exactly 1/60. The real effect is up to six catch-up updates, which the warm-up does not prevent. |

Three claims were *raised and then killed by the agent that raised them*, by
compiling the exact shape with this tree's own `cl.exe`: `int i < vec.size()` under
`/W4 /WX` produces no diagnostic (checked in four separate groups); the
`#pragma warning(disable:4061)` in `device_resources.cpp` is inert because C4061 is
off under `/W4`; and the game's duplicate `NOMINMAX` define is a would-be C4005 hidden by
CMake's PCH wrapper `#pragma system_header` plus `-external:W0`. Those are recorded
because the negative result is worth as much as the finding would have been.

The **adversarial pass refuted nothing** in 40 attempts. Thirteen claims came down
one grade, each for a stated reason: the OBB `edges()` allocation is real but no
shipped level contains a rotated rectangle (`high → medium`); `CoUninitialize`
before COM member release is a contract violation with no COM-activated object
behind it (`high → medium`); `transition_to`'s self-destruction has zero live
instances across all ten call sites (`high → medium`); `player_viewport(4)` is a
visible rendering defect, not corruption (`critical → high`); the cached sampler
needs a device loss to fire (`critical → high`).

### Where the review disagrees with itself

| Disagreement | Which side this document takes |
|---|---|
| Unchecked rapidjson reads: `assets` and both sweeps say **critical**; `audio`'s verifier explicitly rejected critical for the same defect class, "gated behind a malformed content file" | **Critical.** The adversarial pass re-derived the whole chain from `rapidjson.h:437` through `document.h:2107-2109` to `nullptr`, and confirmed `/DNDEBUG` in `out/build/x64-release/CMakeCache.txt:46`. `sprite_sheet_1.json` is 14,165 hand-maintained bytes with 50 `"name"` keys; one dropped key is an ordinary authoring mistake, not a malformed file. |
| `MatrixF`'s unsized storage: filed **critical** twice (`math-header`, `math-impl-b`), regraded **high** by both verifiers | **High.** A default-constructed `std::vector`'s `data()` is null, so `identity(3)` access-violates deterministically rather than corrupting the heap, and it has zero callers anywhere. Both reports independently reach the same remedy: **delete** `MatrixF`, `Matrix3x3F` and `Vector4F` rather than fix them. |
| `/fp:fast`: `math-impl-a` files its silent removal as a finding; `math-impl-b`'s verifier stripped a consequence that assumed it was *set* | **Both are right about the code and it is gone.** `cmake/settings.cmake` carries only `/W4 /WX /permissive- /sdl`; `tests.md` and `sweep-build` confirm it appears in no CMake file, preset or `.vcxproj`. The finding that survives is documentary: `cmake/settings.cmake` says its values are "carried over from the solution this build replaced", and `/fp:fast` and `/Zc:__cplusplus` were in all four old configurations and are in none of the new ones. Dropping `/fp:fast` is almost certainly correct; doing it under a comment asserting the opposite is not. |
| T11 is cited 20 times against `game/` code | **Those citations are wrong as written.** `PHILOSOPHY.md:207-211` hands game code its own grammar explicitly. The findings themselves mostly survive on a *different* citation: `PHILOSOPHY.md:311-313` ("A paint-tile grid is one `GameObject`, not ten thousand") is the engine's own worked example, and it is inverted in `game/objects/structure_paintable.h:59`. Cite that, not T11. |
| The engine/game include wall: five sweeps propose per-target include roots | **The remedy does not work.** `CONVENTIONS.md:110-113` requires every include be spelled from the repository root, so any include root that lets an engine file see `engine/` also lets it see `game/`. The wall arrives with the repository split, or as a grep step over `engine/` sources for `#include "game/`. Nothing else available today enforces it. |
| Coverage: `gap-coverage` measures 24 of 259 first-party sources cited nowhere; `gap-adversary` measures 11 of 242 tracked files never named | **Both, by different methods, say the same thing.** File coverage is near-complete; 21 of the 24 uncited files are the forwarding-constructor leaves and two-line enum headers the weapon, projectile and menu groups already characterised as a class. The gap in this review is unasked questions, not unread files. |

Four whole question categories are absent from all 84 first-pass and verification
reports and were filled only by `gap-coverage`: **continuous integration** (there is none, and
`ARCHITECTURE.md:63-65` says `ctest` runs "in CI"), **compile-cost and PCH hygiene**,
**second-`Application`/multi-window state**, and **input-device identity across an
unplug**. `PHILOSOPHY.md:420-422` also makes benchmarks an obligation equal to tests;
30 findings assert per-frame cost and not one of them mentions that the benchmark
suite does not exist.

---

## The headline

The CMake conversion landed, and with it the first slice of the split. Two static
libraries build, `samples/minimal` is a real target, `ctest` runs three suites,
`/W4 /WX` reaches all six targets through one `INTERFACE` target with zero
suppressions, and the const-`draw()` pass is real and compiler-enforced —
`engine/core/i_game_object.h:41-42` is `virtual void draw(...) const = 0` and every
one of the 20 overrides honours it. Round 1's fixes survived the restructure: `#4`,
`#24`, `#25`-`#29`, `#31`, `#33`, `#35`/`#36` and `#5` were spot-checked in place and
none regressed.

What did not land is any **seam**. `engine/` now has the folder names
`ARCHITECTURE.md:81-93` prescribes and almost none of the contents:
`engine/collision/` holds `partitioner.{h,cpp}` — a thread-range splitter — and no
collision; `engine/input/` holds `connection_state.h`, a two-enumerator enum no
engine translation unit includes; `engine/ui/` holds `widget.{h,cpp}` with no button,
no focus and no navigation, which is the whole of what
`ARCHITECTURE.md:90` says the module is for; and `engine/render/` has no renderer,
because `engine/core/i_game_object.h:4` is `#include <SpriteBatch.h>` and
`DirectX::SpriteBatch*` is the first parameter of `draw`. The platform edge **is**
the API rather than one implementation behind it.

Three load-bearing structural claims are contradicted by the very build files meant
to enforce them:

- `engine/CMakeLists.txt:46-49` and `engine/math/CMakeLists.txt:14` both publish
  `${CMAKE_SOURCE_DIR}` as a `PUBLIC` include directory, so an engine file including
  a game header compiles — and, for the header-only game enums, links. `ARCHITECTURE.md:14-16`,
  `PHILOSOPHY.md:271-272` and `CONVENTIONS.md:110-113` each say it fails to compile,
  and one of them calls that "the feature (T5)".
- `MattMath`, the module `ARCHITECTURE.md:130` says depends on **nothing**, links
  `Microsoft::DirectXTK PUBLIC` (`engine/math/CMakeLists.txt:15-18`) and its public
  header includes `<d3d11.h>` (`engine/math/matt_math.h:3-5`) — and
  `engine/math/CMakeLists.txt:1-2` cites `docs/design/ARCHITECTURE.md` as its
  authority for a statement that document does not make.
- No test target links `ArtAttackEngine`. `tests/assets/CMakeLists.txt:6-19`
  recompiles two engine `.cpp` files into itself; `tests/core/CMakeLists.txt:12-15`
  links no library at all. **Thirty of the engine's thirty-two translation units are
  compiled into nothing that runs.**

The safety work that did land stops one call short of the thing it was protecting.
`draw()` is const all the way down through ArtAttack's own types, but
`engine/core/registry.h:98` and `:117` are `Resource* get(...) const` — const
members handing back mutable pointers — and the live race is one level deeper,
inside DirectXTK: every text draw passes `const char*`, which converts through a
scratch buffer owned by the **shared** `SpriteFont`
(`Src/SpriteFont.cpp:365-391`), while `game/objects/level.cpp:404-415` fans out one
worker per player and each worker draws the whole HUD with that player's own
numbers. `../IMPLEMENTED.md:46,70` records `#16`'s data race as fixed and only its
redundancy as outstanding. **That is the one status line in that document I would
not trust.**

Beneath the architecture the game carries a thick layer of ordinary visible defects.
A camera scroll border whose minimum exceeds its maximum at the default resolution,
oscillating 140 px every frame in split-screen (`engine/render/camera_tools.cpp:91-108`).
A three-player menu that draws its fourth pane with a fullscreen camera and leaves
its top-right quadrant empty (`engine/render/viewport_manager.cpp:79-93,143-234`).
Projectiles that pass through the one shipped ramp
(`game/objects/projectile.cpp:96-99,122-125`). A weapon fire loop that keeps playing
under the main menu (`game/objects/weapon.cpp:242-268`). A wave bank that is
gitignored, so no fresh clone can start the game (`.gitignore:365-368`). Together
they say the proof-of-engine client has not been played against its own edges
recently.

**The single thing most worth doing next is cutting the renderer seam**: an
engine-owned `draw_sprite`/`draw_text` interface with the D3D11/DirectXTK backend
behind it. The evidence that it is the right one is that four separate outstanding
items all queue behind it — `Scene` extraction from `Level`, headless tests, the
sample as a usable template, a second backend; it is the only change that deletes
`<SpriteBatch.h>` from `engine/core/` and makes `ARCHITECTURE.md:128-138`'s module
table true rather than aspirational; nine of thirty-two engine TUs are untestable on
that one include; and it is exactly where the one remaining piece of *live*
undefined behaviour lives, so the seam and the race share a fix site.

Do the `SpriteFont` fix first — hold text as `std::wstring` and call the wide
overloads, an afternoon's work — so the race is not still shipping while the seam is
being designed.

---

# Critical findings (13)

The 13 filed criticals collapse to **nine distinct defects**. Where several groups
found the same thing independently, it is said. Where the adversarial pass moved a
grade, that is said too.

### 1. The shared `SpriteFont` UTF-8 buffer is raced by the per-view render workers

`engine/render/text_object.cpp:36-45, 53-62, 74-83, 116` · `game/objects/interface_gameplay.cpp:156-202` · `game/objects/debug_text.cpp:106-117` — **critical** · `threading` · found independently by `render-text` and `game-hud`; survived adversarial attack at critical

Every text draw in the tree passes a `const char*`. That binds to
`SpriteFont::DrawString(SpriteBatch*, char const*, ...)` — verified by overload
resolution, not assumed: `Vector2F::xm_vector()` returns `XMFLOAT2` and
`Colour::xm_vector()` returns `XMVECTOR` (`engine/math/matt_math.h:413, 650`), which
selects the UTF-8 overload declared at `Src/SpriteFont.cpp:581`. Its body at `:583` —
like the three sibling narrow overloads at `:589`, `:595`, `:601` and `MeasureString`
at `:607` — funnels through `SpriteFont::Impl::ConvertUTF8`
(`Src/SpriteFont.cpp:365-391`), which writes `utfBuffer`, a
`std::unique_ptr<wchar_t[]>` member of the `Impl` at line 67. The method is `const`
and mutates through `pImpl`, which is why neither the compiler nor the comment at
`engine/core/i_game_object.h:10-15` can see it.

`game/objects/level.cpp:404-415` submits one thread-pool task per player partition;
`engine/collision/partitioner.cpp:5-23` with `max_threads = 16`
(`engine/app/application.h:46`, `game/main.cpp:38`) gives one worker per player. Each
worker reaches `level.cpp:599-604` → `draw_countdown_text` at `:759-767`, drawing the
single `countdown_text_` (`level.h:122`) — twice, because `text_drop_shadow.cpp:28-40`
draws shadow then text. `RenderResources::sprite_font()` returns a borrowed pointer
into one `Registry<DirectX::SpriteFont>` (`render_resources.h:61, 90`). So N threads
convert into one buffer during the countdown of every 2+ player match — and, through
`draw_gameplay_interface` at `level.cpp:578` → `interface_gameplay.cpp:156-202`, on
every frame of one.

**Three failures, in increasing order of frequency.** (1) First-allocation double
free: two threads both see `!utfBuffer`, both `reset(new wchar_t[1024])`; the second
`reset` deletes the array the first is about to read. (2) Torn conversion: both
threads `MultiByteToWideChar` into the same array; `ForEachGlyph` → `FindGlyph` then
`throw std::runtime_error("Character not in font")` (`Src/SpriteFont.cpp:262`) from a
worker thread, rethrown on the join and fatal. (3) Resize use-after-free, needing a
>1024-character string — real but unreachable with this content.

The exposure is asymmetric and was checked: `TextObject`'s constructor calls
`remeasure()` (`engine/render/text_object.cpp:25, 112-118`), which routes through the
same `ConvertUTF8`, so the two `gill_sans` fonts are warmed single-threaded and race
on **content** only — two dead players see each other's respawn digits
(`interface_gameplay.cpp:186-201`), two players with the overlay on interleave each
other's debug lines. `courier_new_bold_16` is named only by `debug_text.h:10` and
`samples/minimal`, so its buffer is genuinely still null on first concurrent draw.
`Application::OnDeviceLost` calls `reset_all_sprite_fonts()` (`application.cpp:377`)
and nothing re-measures afterwards, so the cold-buffer branch returns after every
device restore.

**Remedy.** Keep a `std::wstring` beside `text_`, refreshed in `set_text`/`set_font`
alongside `remeasure()`, and call the `wchar_t const*` overloads, which never touch
`Impl::utfBuffer`. That also removes a `MultiByteToWideChar` from `remeasure()`.
`debug_text.cpp` and `interface_gameplay.cpp` need the same treatment and both build
their strings with `std::to_string(...).c_str()` per draw, so they want the value
cached in `update()` anyway. Then correct `interface_gameplay.h:51-57`, which asserts
the opposite as fact.

### 2. Sprite-sheet and sound-bank JSON is read through accessors whose checks vanish in Release

`engine/assets/sprite_sheet_loader.cpp:11-67` · `engine/assets/sound_bank_loader.cpp:14-82` — **critical** · `correctness` · T6, T2 · found independently by `assets`, `sweep-content-data` and `sweep-loud-failure`; survived adversarial attack at critical

`sprite_sheet_loader.cpp:14-23` reads `frame["name"].GetString()`,
`frame["position"]["x"].GetInt()` and five more with no presence or type check;
`:62-66` does the same for `doc["sprite_frames"]` and `doc["animation_strips"]`.
`sound_bank_loader.cpp:53-54` and `:73-78` are identical in shape, including
`doc["create_effect_instance_for_each_wave"].GetBool()`.

The chain was re-derived in the vendored source rather than asserted.
`external/rapidjson/include/rapidjson/rapidjson.h:435-437` is
`#define RAPIDJSON_ASSERT(x) assert(x)`; nothing in `CMakeLists.txt`,
`CMakePresets.json` or `cmake/settings.cmake` redefines it, and
`out/build/x64-release/CMakeCache.txt:46` is
`CMAKE_CXX_FLAGS_RELEASE:STRING=/O2 /Ob2 /DNDEBUG`. `document.h:1226-1238` does
`RAPIDJSON_ASSERT(false)` then returns a placement-new'd `GenericValue`;
`document.h:690` value-initialises `data_` with `kNullFlag`; `document.h:1853`
returns `DataString(data_)`; `document.h:2107-2109` takes the non-inline branch over
zeroed bytes and `RAPIDJSON_GETPOINTER` yields **`nullptr`**. So
`std::string name = frame["name"].GetString();` on a frame missing `"name"` is
`std::string(nullptr)` — `strlen` on null.

The reachable path is ordinary. `game/content/manifest.json` names `sprite_sheet_1`;
`resource_loader.cpp:143-153` opens `./textures/sprite_sheet_1.json`; that file is
14,165 bytes of hand-maintained records with 50 `"name"` keys. The non-crashing cases
are no better: `GetInt()` on a value written `0.5` returns whatever sits in
`data_.n.i.i`, a silently wrong source rectangle; `GetFloat()` returns `0.0`;
`GetBool()` returns `false`; and `doc["sprite_frames"]` on a document that parsed as
an array walks `GetMembersPointer()` over array storage — 32-byte `Member` strides
across 16-byte elements, an out-of-bounds read followed by `StringEqual` on garbage.

This is precisely the hazard the sibling file in the same folder names, in a comment
at `asset_manifest_loader.cpp:12-16`, and the fix already exists twenty lines away at
`asset_manifest_loader.cpp:23-54`. Worse, `sprite_sheet_loader.h:14-15` tells the
reader "whatever rapidjson throws if the document is the wrong shape" — rapidjson
throws nothing here, so that sentence is false.

**Remedy.** Move `fail`/`require_member`/`require_array`/`require_string` out of
`asset_manifest_loader.cpp`'s anonymous namespace into a shared internal header, add
`require_int`/`require_float`/`require_bool`, and route every read in both loaders
through them so a bad field comes back as a sentence naming the file, the record
index and the key. `tests/assets/asset_manifest_loader_tests.cpp:198-217` already
pins that message shape.

### 3. The point-clamp sampler is cached in two places, and device loss frees it

`game/objects/level.h:149` · `game/objects/level_builder.h:39` — **critical** as filed, **high** after adversarial review · `lifetime` · found independently by `game-level` and `sweep-lifetimes`

`GameLevel::init` reads `this->data()->common_states()->PointClamp()` once
(`game/states/game_states.cpp:114-122`, the call at `:119`) and stores the raw
`ID3D11SamplerState*` in **two** members. `Application::OnDeviceLost` releases the
`CommonStates` at `engine/app/application.cpp:374`; `OnDeviceRestored` builds a new
one at `:169`. Nothing re-seats either member.

It is consumed on every render worker: `level.cpp:531`
(`Begin(SpriteSortMode_Deferred, nullptr, this->sampler_state_)`), `level.cpp:585`
forwarded into `interface_gameplay.cpp:39` for a second `Begin`, and `level.cpp:650`
on the zoom-out path. `SpriteBatch` stores it and calls `PSSetSamplers` at `End()`,
so this is a use-after-free reaching the driver.

`../IMPLEMENTED.md:156` records the `Level` half as a known survivor. The
`LevelBuilder` half is the one that makes it permanent: the builder is constructed
once and outlives every level it builds (`level_builder.cpp:21`, `:77`), and
`build_and_enter_level()` (`game_states.cpp:131-137`) is the restart path, reached
from `PauseMenuAction::restart` (`:213`) and `EndMenuAction::restart` (`:289`). So the
one action a player takes after a graphics hiccup — restart — constructs a *fresh*
`Level` and hands it a pointer released before that `Level` existed.

The contract is written down and violated. `game/game_data.h:53-55` says
`CommonStates` objects "change identity across [a device loss] and must be read
through this each time they are needed". `MenuPage` obeys it —
`menu_page.cpp:185-188` re-fetches per draw. `GameLevel::init` does not.

**Remedy.** Delete `sampler_state_` from both classes and both constructors; read
`common_states()->PointClamp()` at the top of `draw`, exactly as `MenuPage` already
does. Better, hoist sampler choice behind the renderer interface so no game object
names a D3D state object at all.

**Grade note:** the adversarial pass dropped this to high because firing it needs a
TDR, driver update or RDP transition. I agree with high for planning purposes; the
`LevelBuilder` half is new information and the fix is two deletions, so it belongs in
the first week regardless.

### 4. There is no renderer seam: D3D11 is the engine's public draw API

`engine/render/device_resources.h:56-100` (with `engine/core/i_game_object.h:4, 41-42`) — **critical** · `boundary` · T1, ARCHITECTURE/The tree, PHILOSOPHY/Simulation and rendering

`ARCHITECTURE.md:84-85` places the D3D11 backend in `engine/render/d3d11/` behind an
engine-owned interface. That folder does not exist. `engine/render/` holds 38 flat
files, there is no `renderer.h`, and no renderer type exists. Instead this header
publishes seventeen raw D3D/DXGI accessors, and the `ID3D11DeviceContext*` they hand
out is the drawing vocabulary of both client games.

The raw context type has propagated into eight game-side signatures
(`game/objects/level.h:80,164,168,177,181`, `game/states/menu_page.h:38,48`) and into
`engine/render/viewport_manager.h:35,38`, so the
`record → FinishCommandList → ExecuteCommandList → Release` protocol is hand-written
in four places (`level.cpp:606`, `:670`, `menu_page.cpp:120`,
`samples/minimal/states/hello_state.cpp:78`), each with its own `->Release()`.
`tests/` holds `assets`, `core` and `math` and no `render`, because nothing above this
file is constructible without a window — which contradicts `PHILOSOPHY.md:445-447`
in as many words. DirectXTK's only `SpriteBatch` constructor dereferences its
`ID3D11DeviceContext*` argument immediately (`Src/SpriteBatch.cpp:35-39, 377, 390`),
so a null or fake context is a crash rather than a test double. That one fact gates
14 engine types.

**The seam is much smaller than the class suggests.** Counting external callers for
all seventeen accessors, only five have one: `GetD3DDevice`
(`application.cpp:142,154`), `GetD3DDeviceContext` (`application.cpp:285`,
`game_states.cpp:328`, `menu_page.cpp:155`, `hello_state.cpp:83` — three of the four
do nothing but `ExecuteCommandList`), `GetRenderTargetView` (`application.cpp:288`),
`GetScreenViewport` (`application.cpp:293`) and `GetOutputSize`
(`application.cpp:350`). The other twelve have no caller anywhere in the repository.

**Remedy.** Add `engine/render/renderer.h` with: a per-worker draw-list handle
replacing `deferred_context(i)`; `submit()` (the finish/execute/release protocol,
written once); `begin_frame`/`end_frame` absorbing `Application::clear()` and
`Present()`; `draw_sprite(...)` as `PHILOSOPHY.md:367-370` already specifies;
`draw_text(...)` taking wide strings (see finding 1); `back_buffer_size()`; and
`begin_marker`/`end_marker` for the PIX pair. Move this file to
`engine/render/d3d11/` as its one implementation and port `samples/minimal/` first —
it is 85 lines and it is the acceptance test for whether the seam is real.
`GetD3DDevice` is not a renderer concern at all but a resource-factory one, and
`RenderResources` already speaks `Handle<ID3D11ShaderResourceView>`
(`engine/render/render_resources.h:38`), so only the handle's payload type changes.
**Decide explicitly in ARCHITECTURE whether "interface" means a vtable or a
compile-time-selected concrete type**: T8 rules out a virtual call per sprite, so the
document should say which kind of seam it means.

### 5. The new-project template makes the game author write D3D11

`samples/minimal/states/hello_state.cpp:62-85` — **critical** · `boundary` · T10 ("Not a licence for: an expert-only API"), PHILOSOPHY/Simulation and rendering

The sample `ARCHITECTURE.md:176-178` calls "the template a new project copies" opens
a deferred context, records a command list, executes it on the immediate context and
releases a COM object by hand — eight lines of graphics-backend code in the smallest
possible game.

Every `SpriteBatch::Begin`/`End` pair in the repository is in `game/` or `samples/`;
the engine never opens a batch. So drawing is not a service the engine provides, it
is a protocol every client reimplements — and the two existing implementations have
**already diverged**: `level.cpp:606` and `:670` pass `FinishCommandList(TRUE, …)`
while `menu_page.cpp:120` passes `FALSE`; `game_states.cpp:337` passes
`ExecuteCommandList(…, TRUE)` while `menu_page.cpp:164` passes `FALSE`. Neither
divergence is commented anywhere. A copy-pasted new project also inherits a raw-COM
leak on any early return added later.

`samples/minimal` is the mechanism `PHILOSOPHY.md:431-433` names as the thing that
keeps the boundary honest. It currently exercises four of roughly fifty public engine
types and touches none of the breached APIs — no `ViewportManager`, no `CameraTools`,
no `ScreenLayout` — which is precisely why the boundary breaches in this review went
uncaught by it.

**Remedy.** Give the engine the render-frame mechanism, so a state's `draw()` names
what to draw and nothing about how it reaches the GPU. The sample should then contain
no `ID3D11*` identifier at all. It also needs a `project()` preamble and
`${CMAKE_CURRENT_SOURCE_DIR}/..` in place of `${CMAKE_SOURCE_DIR}` before it can
configure outside this repository (`samples/minimal/CMakeLists.txt:1-44`).

### 6. There is no `Scene`: the engine ships an interface and nothing that holds one

`game/objects/level.cpp:181-340` — **critical** · `boundary` · T1, PHILOSOPHY/Structural types

`PHILOSOPHY.md:330-334` names `Scene` as a concrete engine class owning
"registration, update/draw orchestration, culling, collision dispatch, cameras and
views, spawn groups". `grep -r "Scene" engine/` returns nothing. Every one of those
responsibilities is a private method of the game's `Level`: collision dispatch at
`level.cpp:233-261` and `:289-304` (every ordered pair tested twice, no broad phase),
swap-and-pop retirement at `:320-329`, culling at `:534-540`, per-view fan-out at
`:393-418`.

`engine/core/i_game_object.h:44-56` documents `bounds()` as existing so "the caller
build an index once and query it". There is no caller in the engine to build that
index, so the comment describes an intent no engine code holds. A stranger writing a
game gets `IGameObject` (update/draw/bounds) and `Application` (window, device,
services, one state) and nothing in between; to draw two objects they must write the
container, the update loop, the cull test, the pair loop, the deletion pass and the
fan-out themselves — which is what `samples/minimal` demonstrably does, badly.

`Level` today owns eleven members and nine methods that are `Scene`'s
(`game/objects/level.h:92-102, 124-155, 164-183`). Three things block a clean lift
and are worth knowing before starting: the view count comes from
`player_objects_->size()` (`level.cpp:400-401`), each view's viewport and camera are
read off `Player` (`level.cpp:521-526`), and the one mid-frame insertion point is a
single `push_back` at `level.cpp:223` inside `update_weapon_and_get_projectiles`.
`camera_tools_` (`level.h:124`) and `camera_bounds_` (`level.h:141`) are read only
inside the player loop at `level.cpp:81-85` and `:210-214` — those two members are the
weakest joint in the split and the seam has to either hand them back to the ruleset
or move the follow-camera into the view list.

**Remedy.** `engine/core/scene.h` owning the object list, the cull query, the pair
dispatch and the deletion pass, with `Level` becoming a game class that owns a `Scene`
and adds the paint-battle rules. **But not first — see finding 7.**

### 7. Cut the seam before extracting `Scene`; every method that becomes `Scene` carries D3D11 in its own signature

`game/objects/level.h:164-183` — **critical** · `design` · T1

All five private draw methods that finding 6 assigns to `Scene` take three raw
pointers to vectors of D3D11/DirectXTK objects:
`std::vector<ID3D11DeviceContext*>*`, `std::vector<ID3D11CommandList*>*` and
`std::vector<DirectX::SpriteBatch*>*`. One of them is an **output** parameter whose
caller must pre-size the vector, pre-fill it with `nullptr` and `Release` every
non-null entry — three obligations stated nowhere in the tree.

Extracting `Scene` before the seam exists therefore writes those three types into
`engine/core/scene.h`, in the module `ARCHITECTURE.md:131` gives exactly one
dependency, and guarantees the file is written twice. Today's breach in that module
is one include (`engine/core/i_game_object.h:4`); a `Scene` extracted now makes it
three types in a parameter list, in the file a stranger reads second.

The two are also one design, not two. The parallelism axis is views
(`engine/core/i_game_object.h:10-15`), and `Scene` cannot be built at all until it
owns an explicit view list, because the number of render tasks (`level.cpp:401`), each
view's viewport (`:523`) and each view's camera (`:526`) are all read off `Player`.
Whoever designs the view list is designing the seam's unit of work.

**Remedy.** Design the per-view draw target and `Scene`'s view list as one interface;
implement the seam first so `Scene`'s draw path can be written in engine vocabulary;
only then move the eleven members and nine methods out of `Level`.

### 8. `engine/collision/` contains no collision

`engine/collision/partitioner.h:1-18` — **critical** · `boundary` · ARCHITECTURE/The tree

The entire module is a class with a default constructor and two overloads of
`partition(int num_elements, int num_partitions)` — a thread-range splitter with no
collision concept in it, filed under a folder `ARCHITECTURE.md:86` describes as
"broad phase, narrow phase, manifolds, resolution".

Where the collision actually is: the **interface** is
`game/objects/i_collision_game_object.h:7-21`, in `game/`, in the global namespace;
the **narrow phase** is `engine/math/collision_tools.cpp` and `engine/math/matt_math.cpp`
under `mattmath::`, inside the library that is supposed to depend on nothing and sits
*upstream* of collision in the module table; the **dispatch** is
`game/objects/level.cpp:233-305`.

This is new information beyond outstanding finding `#13`. `#13` says the collision
interface carries a game enum; the post-split state is worse — the interface is not
in the engine at all, so there is no engine type for `#13`'s enum to be removed
*from*. Two concrete consequences: `samples/minimal` cannot use collision, because
`ICollisionGameObject` is in `game/` and the sample links only `ArtAttackEngine`, as
its own build file says in as many words (`samples/minimal/CMakeLists.txt:16`); and
the broad phase `PHILOSOPHY.md:395-396` promises has nowhere to live, which is why
`Level` runs an all-pairs loop.

One more thing worth knowing before anyone budgets this: `Partitioner::partition` has
**never once partitioned**. Every call in the repository passes `n <= k`, so it always
returns `n` singleton ranges — the engine's only collision module computes
`i -> {i, i+1}` and heap-allocates a vector to do it.

**Remedy.** Create `engine/collision/collision_object.h` with `shape()`, layer/mask
and an opaque game tag; move `Partitioner` to `engine/core/` beside `ThreadPool`; let
`#13` be answered by the tag rather than by a type that does not exist yet.

### 9. The repository has no front door: no README, no licence, no stated prerequisites

`docs/design/ARCHITECTURE.md:75-109` — **critical** · `docs` · PHILOSOPHY/The public face

`git ls-files | grep -i 'readme\|licen'` returns exactly two paths, both irrelevant:
`docs/review/README.md` (the previous review) and `external/rapidjson/README.md`.
There is no licence file of any kind. The tree ARCHITECTURE declares authoritative
lists neither, so the destination as written has no front door either.

The one hard prerequisite is stated only as a variable expansion, in
`CMakePresets.json:11`: `"toolchainFile": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"`.
A stranger cloning this gets a tree with no entry point and a configure step that
fails with a CMake toolchain error naming a variable nobody told them to set — and
`PHILOSOPHY.md:3-5` opens by saying the engine is "intended, eventually, for
developers other than its author".

Compounding it: `game/content/sounds/sound_bank_1.xwb` is gitignored
(`.gitignore:365-368`; `git check-ignore -v` confirms rule `:367`), and so are the 24
`.wav` sources the `XWBTool` recipe in `game/content/sounds/sound_bank_1/cmd.txt`
would need. `manifest.json:32-37` declares the sound bank and
`resource_loader.cpp:167-181` rethrows when the `WaveBank` constructor fails, so a
fresh clone builds a game that dies at startup behind a message box. There is no
README to say so because there is no README.

**Remedy.** A root `README.md` with prerequisites (vcpkg + `VCPKG_ROOT`, Ninja, MSVC,
Windows SDK), the two preset commands, the wave-bank build step, and links to the
three design docs; a `LICENSE`; and add both to ARCHITECTURE's tree. This is the
cheapest finding in the review and it gates every other one for anybody but you.

---

# Themes

Organised by defect, not by module. Each entry names the sites; each ends with the
fix.

## A. The seam that is not there, and the four things queued behind it

**`DirectX::SpriteBatch*` is the engine's draw vocabulary.** It appears in the member
signatures of **14 engine types**, entering through `engine/core/i_game_object.h:4`.
`engine/render/d3d11/` and `engine/input/xinput/` — the two folders the
cross-platform story rests on (`ARCHITECTURE.md:85, 88`) — have never been created.
`engine/render/device_resources.h` is what re-exports every D3D and DXGI type into
the engine's public surface, and `engine/app/application.h` re-exports
`DirectX::SpriteBatch*`, `CommonStates*`, `GamePad*` and a raw `HWND` on top.

Four outstanding items are downstream of this one change, and doing any of them first
produces code the seam deletes:

| Blocked item | Why the seam is its prerequisite |
|---|---|
| `Scene` extraction (finding 7) | All five candidate methods carry three D3D11 vector parameters in their own signatures (`level.h:164-183`) |
| Headless tests | 9 of 32 engine TUs are untestable on the `<SpriteBatch.h>` include alone; `tests/` has no `render/` folder as a direct result |
| The sample as a template | `hello_state.cpp:62-85` is eight lines of backend in a 24-line `draw()` |
| A second backend | `PHILOSOPHY.md:275-279` calls it "an addition, not a rewrite"; today it is a 20-site signature edit |

**The interface-signature edits are one pass, not four.** The `I` prefix removal,
`SpriteBatch*` → `Renderer&`, `SpriteEffects` → an engine enum, and
`State::update(float dt)` / `State::draw(Renderer&) const` all rewrite the same
declarations across the same 20 overrides, 13 headers, 17 game headers and 45 state
classes. Sequencing them separately multiplies the cost by four and the merge pain by
more.

**Fix:** finding 4, then finding 7, then finding 6.

## B. Folders named for modules that do not contain them

`ARCHITECTURE.md:81-93` is described in its own text (`:7`) as "the authoritative
roster of modules and folders". Four of nine `engine/` folders hold something other
than what it says, and three rows of the module table at `:128-138` are false.

| Folder | What ARCHITECTURE says | What is there |
|---|---|---|
| `engine/collision/` | broad phase, narrow phase, manifolds, resolution | `partitioner.{h,cpp}` — 57 lines of thread-range splitting |
| `engine/input/` | devices, action mapping | `connection_state.h` — one enum, 10 lines, included by no engine TU |
| `engine/ui/` | widgets, focus, controller navigation | `widget.{h,cpp}` — six `M*` types; a case-insensitive grep of `engine/` for *button*, *focus* or *navigat* returns zero hits |
| `engine/render/d3d11/` | the backend, behind the interface | does not exist |
| `engine/core/` | game loop, timing, **Scene**, states, services, registries | no `Scene`; `grep -r Scene engine/` returns nothing |
| `engine/math/` | depends on nothing | includes `<d3d11.h>`, links DirectXTK `PUBLIC`, and holds `Viewport`, `Camera` and `Colour` |

The mechanism the engine promises is written in `game/`, and in the input case
written twice: `game/objects/player_input.{h,cpp}` and `game/states/menu_input.{h,cpp}`
are two hand-rolled pad-polling-plus-edge-detection layers that have already diverged
on dead-zone policy (`DEAD_ZONE_CIRCULAR` vs `DEAD_ZONE_NONE`).

**Fix:** these are the module-content half of findings 4, 6 and 8; the folders are
promises, and the honest interim move is to say so in ARCHITECTURE (see theme N)
rather than leave the roster claiming to be authoritative.

## C. The walls are documentation, not build structure

- **The engine/game include wall does not exist.** `engine/CMakeLists.txt:46-49` and
  `engine/math/CMakeLists.txt:14` both publish `${CMAKE_SOURCE_DIR}` `PUBLIC`. An
  engine `.cpp` including `game/objects/collision_object_type.h` compiles and links.
  No engine file does today, so the wall is held by discipline alone — which is
  exactly the mechanism that would have prevented outstanding `#13` recurring.
- **`MattMath` is not the dependency-free library both documents describe.**
  `matt_math.h:3-5` includes `SimpleMath.h`, `shape_type.h` and `<d3d11.h>`;
  `engine/math/CMakeLists.txt:15-18` links DirectXTK `PUBLIC` and `artattack_settings`
  `PRIVATE`, so a stranger linking `MattMath` alone inherits `windows.h` **without**
  `NOMINMAX` (`cmake/settings.cmake:16-19` is where `NOMINMAX` lives). The `<d3d11.h>`
  half is cheap to cut: the entire D3D surface in the module is four declarations
  (`matt_math.h:794, 797, 798, 807`), and the only non-math callers are
  `viewport_manager.cpp:24` and `:65`.
- **`${CMAKE_SOURCE_DIR}` appears ten times across five build files** —
  `engine/CMakeLists.txt:47,48`, `engine/math/CMakeLists.txt:14`,
  `game/CMakeLists.txt:65,105`, `tests/assets/CMakeLists.txt:9,10,13,14`,
  `tests/core/CMakeLists.txt:11` — which is exactly the variable that stops working
  the day this repository becomes the submodule the roadmap says it will be.
- **The engine's compile-time contracts are `PRIVATE`.** `artattack_settings` carries
  `NOMINMAX`, `WIN32_LEAN_AND_MEAN` and `cxx_std_20` and is linked `PRIVATE` into both
  libraries, while `engine/app/application.h` includes `<Windows.h>` unconditionally.
  Those guarantees hold inside this repository and evaporate for the stranger.

**Fix:** `${CMAKE_CURRENT_SOURCE_DIR}/..` everywhere; move `NOMINMAX`,
`WIN32_LEAN_AND_MEAN` and `cxx_std_20` to `INTERFACE`; add a grep step over `engine/`
for `#include "game/` until the repository split makes it structural. Do not attempt
per-target include roots — `CONVENTIONS.md:110-113` makes them impossible (see "Where
the review disagrees with itself").

## D. `const` stops at the first pointer

The project bought a compiler-enforced pure-read `draw()` in `ca4d228`, and it is
real for ArtAttack's own types. It stops at every shared service:

| Site | What escapes |
|---|---|
| `engine/core/registry.h:98, 117` | `Resource* get(...) const` — const member, mutable pointer |
| `engine/render/render_resources.h:57-78` | `sprite_sheet()`, `texture()`, `sprite_font()` const, all returning `T*` |
| `engine/render/sprite_sheet.h:92` | `set_texture()` / `reset_all_textures()` compile inside a `draw() const` |
| `engine/audio/sound_bank_object.cpp:15-18` | `SoundBank* sound_bank() const` — every mutating audio call is const |
| `engine/assets/resource_loader.cpp:155-162` | a `const` member calling `set_texture(...)`; `reload_device_resources()` is const while rebuilding every GPU resource |
| `engine/core/state_context.h:17` | `update() const` drives every mutation in the program, because `unique_ptr` launders constness |
| `engine/render/drawer.h` | three methods documented as const because "none of the three reads a member" — which is what `static` says, and says stronger |

And one level past all of them sits the live race (finding 1), inside a `const`
method of a library type. `PHILOSOPHY.md:371-372` states the safety property as fact —
"render workers own disjoint slices of the scene" — which does not describe this
renderer: `level.cpp:514-558` has every worker iterate every object.
`../IMPLEMENTED.md:88-93` already records that correction. The document has not been
amended.

**Fix:** `const T*` from the const accessors, with a named non-const overload where a
loader genuinely needs one; measured cost is about 20 files. Then amend
`PHILOSOPHY.md:371-372` to say what makes it sound — a pure-read `draw()` — rather
than something that is not true.

## E. The recent passes stopped at `State`

`dt`-as-a-parameter, `const draw()`, parameterised draw and `bounds()` all landed on
`IGameObject` and none of them on `State`, in the same folder:

- `engine/core/state.h` is comment-free. `State::update()` and `State::draw()` take
  nothing, so `dt` remains a heap `float` behind `Application::dt()`
  (`application.h:131-134`) — the exact design `engine/core/i_game_object.h:27-33`
  spends six lines explaining why it removed.
- `State::draw()` is non-const, so `MenuPage`'s three draw helpers are non-const while
  running on sixteen workers, while `Level`'s equivalents are const.
- `c6696af`'s commit message argues for this deliberately ("states can already reach
  `dt` through the application"). That rationale is what `PHILOSOPHY.md:358` forbids by
  name, and it is recorded nowhere a reader would look.
- The documented state "stack" (`ARCHITECTURE.md:92-93`, `PHILOSOPHY.md:332`) is a
  one-slot holder (`state_context.h:17`). Its one client grew four nested
  `StateContext`s and three heap-allocated enums as return channels
  (`game/states/game_states.h`).
- `samples/minimal` teaches the deleted pattern back: `hello_state.cpp` models two
  labels as `unique_ptr<Text>` and its comments defend "dt is a pointer" as if it were
  the design.

**Fix:** `State::update(float dt)` and `State::draw(Renderer&) const` in the same pass
as the seam (theme A); push/pop on `StateContext` with the transition deferred to the
end of `update()`, which also closes the `transition_to` hazard in theme G and two
live menu bugs.

## F. Loud where somebody wrote it, silent everywhere else

T6 is implemented once per author sitting, and the newest code is far better than the
code beside it.

- `engine/assets/asset_manifest_loader.cpp` is exemplary — every read checked, errors
  naming file, group and key, a test pinning the message
  (`tests/assets/asset_manifest_loader_tests.cpp:198-217`). Fifteen lines away,
  `sprite_sheet_loader.cpp` and `sound_bank_loader.cpp` are undefined behaviour in
  Release (finding 2).
- `engine/assets/resource_loader.cpp:108-122` reports every texture failure as a bare
  `"Failure with HRESULT of 80070002"` with no file name, while `load_sprite_font`
  eleven lines below wraps DirectXTK's exception specifically to name the font and the
  path — and then **discards `e.what()`**, so DirectXTK's four distinct "this file is
  corrupt" throws all arrive as "not found".
- `engine/assets/sound_bank_loader.cpp:24-35` catches `std::out_of_range` for a wave
  name. DirectXTK's `WaveBank::CreateInstance(const char*)` returns an empty
  `unique_ptr` and throws nothing (`Audio/WaveBank.cpp:356-362`, with the comment
  explaining why at `:361`), so the catch is **dead**. `Registry::add` stores the null
  happily, `Registry::contains` then reports the name absent, and the failure arrives
  at first playback as `"'x' has been released."`. Both `sound_bank_loader.h:14-16`
  and `sound_bank.h:13-17` promise a throw naming the wave.
- `engine/math/colour.h:314` is a 148-branch `if`-chain ending in a silent `WHITE`
  fallback, in a tree that solves exactly this two folders away with
  `NameTable::resolve`, which throws naming the element and the kind.
- `engine/render/viewport_manager.cpp:143-234` has four copy-pasted layout blocks and
  **no `case` terminates**, so an out-of-range player index falls through to another
  layout's geometry and finally to the fullscreen `default:`. `all_viewports()` at
  `:90` actively depends on it by asking for `player_viewport(4)`.
- `engine/render/resolution_manager.cpp:153-176`: three unused `set_resolution`
  overloads answer any unrecognised size by silently selecting 720p — the exact mode
  at which the camera bug fires.
- `engine/app/application.cpp:257-262` discards `AudioEngine::Update()`'s bool.
  DirectXTK sets `mCriticalError`, calls `SetSilentMode()` and never self-heals
  (`Audio/AudioEngine.cpp:713-724`); nothing in the tree calls `IsCriticalError` or
  `Reset`. Unplug a headset and audio is gone for the session with no message.
  `git log -S` traces the line to the shell extraction, which transcribed DirectXTK's
  template placeholder `// more about this below...` into a `std::ignore`.
- `engine/core/registry.h:63-76`: `add` cannot distinguish a first insert from a
  replace, so duplicate manifest names — including the cross-kind `texture`/`sprite_sheet`
  collision the loader makes easy — collapse silently. Because `SpriteSheet::texture_`
  (`sprite_sheet.h:92`) is a bare pointer nobody re-seats, that collision is a
  use-after-free, not a wrong sprite.
- **`ApplicationOptions` is validated nowhere.** `max_threads = 0` reaches
  `engine/collision/partitioner.cpp:8` as an integer divide-by-zero on the first menu
  draw — an SEH exception, so `game/main.cpp:93`'s `catch (const std::exception&)`
  never reports it. `target_fps` and `min_threads` are equally unchecked. This is the
  one struct a stranger fills in first.

**Fix:** one shared `require_*` header for JSON; `NameTable`-style throws for colour
names; `case` terminators plus a throw for out-of-range viewport indices; validate
`ApplicationOptions` in `initialize()` with messages naming the field; delete the dead
catch and check what DirectXTK returned.

## G. Lifetime and teardown are the dominant hazard class, and it is all ordering

Round 1's memory-safety hits are genuinely gone: the draw paths are pure const reads,
`Registry`/`NameTable`/`Handle` bounds-check before indexing, the JSON loaders no
longer read through 1-byte buffers, and the thread pool has an exception barrier. No
live data race outside finding 1, and no reachable heap corruption. What is left is
destruction and transfer order:

- **`Application` inherits `StateContext`** (`application.h:71`), and C++ destroys
  member subobjects before base subobjects — so the base holding the entire live game
  tree (`state_context.h:17`) is destroyed **after** every service the tree borrows.
  `application.h:113-114` promises the opposite in writing. It is latent only by
  accident: every destructor in `game/` is `= default`, and `Weapon::sound_bank_`
  (`weapon.h:55`) and `Level::sampler_state_` (`level.h:149`) are already borrowed
  pointers into members that die first. One ordinary line — a `stop_sounds()` in a
  destructor, and `Player::stop_sounds()`/`Weapon::stop_sounds()` already exist — turns
  it into a use-after-free at exit.
- **`~Application` calls `CoUninitialize()` in its body** (`application.cpp:21-31`),
  i.e. before any COM object its members own is released, on every normal exit. Graded
  medium after review: none of the objects involved is `CoCreateInstance`-activated, so
  it is a contract violation with no demonstrated fault.
- **`transition_to` destroys the state whose `update()` is on the stack**
  (`state_context.cpp:21-26`), then dereferences the new one twice with no guard, while
  `update()`/`draw()` at `:5-20` both guard for null. All ten call sites were walked:
  none touches `*this` afterwards, so it is correct by luck. The safety at
  `pause_menu.cpp:87-98` rests on the *outer* `else if` chain — under plain `if`s a
  simultaneous proceed+up is a live heap use-after-free. Neither `state_context.h` (19
  lines) nor `state.h` documents any of it.
- **The window and its class outlive the `Application` whose address is in their user
  data.** `~Application` never unregisters the class, never nulls `window_`, and never
  clears `GWLP_USERDATA`. On the `load_manifest` failure path a modal `MessageBox`
  pumps messages into a window pointing at freed memory.
- **49 of 58 non-owning pointer members carry no ownership comment.** The nine that do
  — `Registry::kind_`, `TextObject::font_`, `SoundBank`'s two, `audio_engine_` dying
  last — are all correct. The defects cluster exactly where no comment exists, or where
  the comment is wrong, and the two confidently-wrong ones are both the engine's front
  door (`application.h:113-114`, `application.h:126-129` vs `common_states()` fifteen
  lines later).

**Fix:** make `Application` *own* a `StateContext` rather than inherit one; move
`CoUninitialize` behind a member that dies last, or into an RAII type declared first;
park the new state in `pending_` and swap it in after `state_->update()` returns; add
the four teardown calls the header implies; and state the loan on every non-owning
member as `PHILOSOPHY.md:353-355` requires.

## H. The ordinary defects — the client has not been played against its own edges

Every one of these is reachable from a shipped path with shipped content.

| Defect | Site | What you see |
|---|---|---|
| Camera scroll border min > max | `engine/render/camera_tools.cpp:91-108` with `matt_math.cpp:50-60` | At 1280x720 split-screen the dead zone inverts: a motionless player oscillates the camera 140 px vertically **every frame** (two-player) or 160 px horizontally (four-player). `clamp_ref` takes the min branch first, so the max is never consulted. One-player at 720p is clean. |
| `player_viewport(4)` + no `case` exits | `engine/render/viewport_manager.cpp:79-93, 143-234` | A three-player menu draws its fourth pane with the fullscreen viewport into player 1's quadrant, leaves the top-right empty, and renders two of four divider arms 1 px instead of 2 |
| Viewport layout never restored | `game/states/game_states.cpp:131-137` | `set_layout` has exactly one call site; none of the seven level exits restores it, so after a 4-player match **every main-menu page is drawn four times** for the rest of the session |
| Projectiles pass through ramps | `game/objects/projectile.cpp:96-99, 122-125` | The hand-written "is a structure" test omits `structure_ramp_left/right`, which `collision_object_type.h:73-80`'s `is_structure()` lists. The pair is never tested at all. One ramp ships (`king_of_the_hill.json:260`) |
| Weapon fire loop outlives the level | `game/objects/weapon.cpp:242-268` | `stop_player_sounds` has one call site (`level.cpp:136`); all seven menu exits call only `stop_music`, and the `SoundBank` outlives the `Level` by the whole application — so quitting mid-fire leaves the sprayer looping over the main menu for the rest of the process |
| Dead players keep firing | `game/objects/player.cpp:776-785` | No `state_` check. Input still arrives, ammo still drains (then `respawn()` at `:1035` wipes the evidence), the fire loop keeps playing for the full 3-second respawn — all while `draw` early-outs at `:78-83` so nothing visible is shooting |
| Held button delivered to the menu that just opened | `game/states/menu_input.cpp:42-48, 145-152` | `prev_inputs_` advances only on frames where its owner is the active state. Press A to jump, press Start, release A → the pause menu confirms "resume". Symmetric twin in `PlayerInput`: the first jump after resume is swallowed |
| Deadzone applied twice | `game/objects/player_input.cpp:21, 87-94` with `player_input.h:11` | `GetState(…, DEAD_ZONE_CIRCULAR)` already removes the hardware deadzone and rescales to [0,1] explicitly "to remove the deadzone discontinuity"; the code then re-imposes `STICK_DEADZONE = 0.5f` on the raw x component with no rescale. No analog movement below half speed, and a fully-deflected stick at 60° is dead |
| Diffusion size snaps 40% | `game/objects/projectile.cpp:59-87` | At `end_time` the terminal branch uses `end_scale` while the ramp converges on `1 + end_scale`, so a spray's collider and sprite jump 37.5 px → 22.5 px in one frame and spend the remaining four fifths of a five-second life at the smaller size |
| Window sized as the outer rect | `engine/app/application.cpp:59-61, 96-125, 215-224` | `AdjustWindowRect` appears nowhere in the repository. `ShowWindow` runs before `SetWindow`, and `WindowSizeChanged` early-returns on `!m_window`, so the corrective `WM_SIZE` is dropped permanently. A 1280x720 request gives a 1264x681 client, stretched non-uniformly (0.9875 x 0.9458) for the whole session |
| `srand` is never called | `game/states/main_menu.cpp` | The "random" team palette and both `rand()` sites produce identical results on every launch |
| Four players, one pad | `game/states/main_menu.cpp:1132-1235` | Only a connected pad can produce `proceed`, and `all_players_confirmed()` scans every player slot, so "4 Players" with one pad can never advance. Two presses of B escape it |
| No fresh clone can start | `.gitignore:365-368` | `sound_bank_1.xwb` and all 24 `.wav` sources are ignored; `load_manifest` throws at startup behind a message box |

**Fix:** these are independent of the seam and of each other. Most are one to five
lines. They are the cheapest evidence that the engine's first client works.

## I. Value types that are not values, and the dead half that is the broken half

- **`mattmath::Shape` is a polymorphic base with fourteen pure virtuals and a
  `clone()`** (`matt_math.h:69-90`), so `RectangleF` — the type
  `IGameObject::bounds()` returns **by value** on the culling path — is 24 bytes, not
  trivially copyable, and cannot be `constexpr`. `level.cpp:534/543/552` calls
  `object->bounds().intersects(camera_view)` for every object, per view, per frame;
  `collision_tools.cpp:286` heap-allocates `collider->clone()` inside
  `calculate_object_collision_depth`, called four times per player collision.
  `Shape::intersects` dereferences an unchecked `dynamic_cast` — UB for any
  user-implemented `Shape`, and `TriangleRightAxisAligned` never overrides `clone()`,
  so every ramp built at `level_object_builder.cpp:147` is stored **sliced** through
  `structure.cpp:23`.
- **`Shape::edges()` returns `std::vector` by value** (`matt_math.h:88`), which
  `PHILOSOPHY.md:418` forbids by name ("fixed-size geometry returns fixed-size
  containers"). Graded medium after review: no shipped level contains a rotated
  rectangle, so the live instance is `Triangle::edges()` on the rect-vs-triangle path,
  a handful of allocations per frame in one of three levels.
- **`PaintTile` is thousands of individually polymorphic objects** —
  4,116 in `king_of_the_hill.json`, 5,230 in `turbulence.json`, 1,898 in
  `close_quarters.json`, re-derived from the shipped data with the builder's own
  arithmetic (`structure_paintable.cpp:97-105`). `PHILOSOPHY.md:311-313` uses this
  exact case as its worked example of what not to do. The interface tax is paid and
  none of the flexibility is bought: no pointer of either interface type ever points at
  one (`structure_paintable.h:59` holds them by value), so four of `PaintTile`'s eight
  interface members and three of `PaintTileSplash`'s four are unreachable.
- **`DrawObject` and `MovingObject` declare 36 virtual accessors between them** for
  which the whole repository contains two overrides, one of which calls the base.
  Thirteen of `MovingObject`'s 27 accessors have zero callers and the rotation triple
  is write-only — the drawn rotation is `DrawObject::draw_rotation_`.
- **The heap is used to give a value a stable address**: `unique_ptr<float>` for `dt`,
  `make_unique<bool>`, three `make_unique<SomeEnum>` as return channels, and
  `unique_ptr<vector<unique_ptr<T>>>` for `Level`'s object lists. Stateless helpers
  (`Partitioner`, `CameraTools`, `TeamColourTools`, `ProjectileBuilder`,
  `WeaponBuilder`) are heap-allocated to call static functions.
- **The dead surface is where the memory bugs live.** `MatrixF(int,int)`
  (`matt_math.cpp:2473-2477`) never sizes `elements_`, and `set_element`'s guards test
  `rows_`/`columns_` rather than `elements_.size()` — so the whole ~574-line matrix
  subsystem has never executed. `Vector4F`, `cross`, `sign`, `normal`,
  `rotate_vector_by_ref`, `d3d_viewport_ptr`, `hypotenuse` and the `sm_*` converters
  all have zero callers, and four of the five unsound or wrong-answer bodies in the
  file are in that set. `Triangle::angle_0/1/2()` return π minus the interior angle,
  and `hypotenuse()` indexes edges with a vertex index — the second survives only
  because π/2 is its own supplement. `ericson_math.cpp` carries ~422 commented-out
  lines against 349 live ones, plus a page marker at `:788` and four U+FFFD bytes from
  a mis-decoded round trip.

**Fix:** the value-semantics work is large and low-urgency, with one exception —
**delete `MatrixF`, `Matrix3x3F` and `Vector4F` and the seven zero-caller free
functions now**, before somebody calls one. Give `TriangleRightAxisAligned` a
`clone()` override today; it is one method and it fixes every ramp in every level.

## J. Content is code, and code is content

`PHILOSOPHY.md:123-124` names "adding a weapon" as T7's worked example. Adding one
today edits **nine files** and rebuilds a third of the game:

`weapon_type.h` (the enumerator must precede `random_primary`, which
`main_menu.cpp:1499,1506` uses as a bound), `weapon_consts.h` (172 lines of
positional aggregates), `weapon_builder.cpp:17-41`, `weapon.cpp:290-303`'s
`resolve_loop_sound` switch, `game/CMakeLists.txt`, `main_menu.h:82-87`'s
descriptions, two switches in `main_menu.cpp`, and `sound_bank_1.json` — plus
`weapon_<new>.h/.cpp`. Measured transitive cost: `weapon_consts.h` reaches 14 of the
game's 46 translation units through `weapon.h` → `player.h` → `level.h` →
`level_builder.h`.

The roster is data wearing five class names: `weapon_sprayer.cpp` is a pure forward of
two `DETAILS_*` constants with an empty body; two subclasses hold the same override
byte for byte; only `WeaponRoller` carries a behavioural delta. Same shape in the five
projectile classes, where adding a type edits eleven places.

Adjacent to it: sixteen `*_consts` namespaces of `SCREAMING_SNAKE` values in headers,
`307` such declarations outside `colour.h` and `301` inside it, including CONVENTIONS'
own example constant `max_velocity` spelled `MAX_VELOCITY`. `engine/math/colour.h` is
a 466-line data table compiled into **59 of 91 first-party TUs** as 301
dynamically-initialised, internal-linkage objects — about 8,700 `std::string`
constructions and 26,000 `std::stoi` calls before `main` — where 301
`inline constexpr` values would cost nothing at either. Because `Colour` has no
`constexpr` constructor, the inline `colour_from_name` odr-uses those objects, which
makes it ill-formed-NDR today.

And the data has drifted from the code it maps to: `dcd8f6f` renamed
`CollisionObjectType`'s enumerators to snake_case and stopped at the `.cpp` boundary,
so the level JSON still says `"STRUCTURE_JUMP_THROUGH"` and file and loader no longer
read continuously — the one thing `CONVENTIONS.md:118-123` exists to guarantee.

**Fix:** `WeaponDefinition`/`ProjectileDefinition` as JSON records in a registry, with
`ResourceLoader::register_kind` — which is already exactly the
`map<string, factory>` shape `PHILOSOPHY.md:404-405` prescribes, already in the tree,
already taught "level" by `game/main.cpp:60`, and used by nothing else.
`LevelObjectBuilder` is a hardcoded `if`-chain ten metres away from it.

## K. Nothing links the product

- **No test executable links `ArtAttackEngine`.** `tests/assets/CMakeLists.txt:6-19`
  compiles `asset_manifest_loader.cpp` and `json_loader.cpp` as its own sources and
  re-declares the rapidjson include path `engine/CMakeLists.txt:46-49` already exports;
  `tests/core/CMakeLists.txt:12-15` links no library; `tests/math/CMakeLists.txt:8-12`
  links `MattMath`. `PHILOSOPHY.md:273` says "Tests link libraries"; `:267` and
  `ARCHITECTURE.md:35` both draw the arrow. **30 of 32 engine TUs have zero coverage.**
- **The stated reason is false.** `tests/assets/CMakeLists.txt:3-5` says linking the
  engine drags in "everything the engine drags in". `ArtAttackEngine` is `STATIC`, so
  linking it pulls only the objects whose symbols are referenced, and `MattMath` is
  already `PUBLIC Microsoft::DirectXTK`, so `MattMathTests` already links DirectXTK.
  The three-line build change is the cheapest enablement available in this review and
  the most likely to be deferred as tidying.
- **A third of the engine is testable today and simply untested.** `Partitioner` takes
  two ints; `ResolutionManager` and `CameraTools` take nothing; `ThreadPool` takes two
  ints; `ViewportManager`'s geometry is pure arithmetic (its `DeviceResources*` member
  is never read by any member function). Every type this walk identified as
  free-to-test-and-untested contained a defect a short test would have caught —
  `Partitioner`'s unchecked divisor, `CameraTools`'s inverted clamp,
  `ViewportManager`'s fall-through.
- **The coverage that exists is thinner than it reads.** One empty `TEST_CASE`; two
  blocks that build the input and forget the `CHECK`; a commented-out case; three
  "boundary" pairs built with `value ± FLT_EPSILON` at coordinate `10.0f`, which is one
  eighth of a ULP and rounds back to exactly `10.0f`, so both sides of the boundary are
  the same input; eleven error-path assertions that check only `std::runtime_error`,
  which an unwritable fixture also produces; and a bisection assertion that pins the
  current search's literal outputs including its iteration count.
- **The two-policy zero-length vector contract** that `../IMPLEMENTED.md` records as a
  NaN fix, that `PHILOSOPHY.md:392-394` names by example, and that live code reaches
  through `MovingObject::velocity_normalized`, `Triangle::inflate` and every
  `RectangleRotated` axis setter, has **zero coverage**: `normalize()`, `normalized()`
  and `to_unit_vector()` are not called once anywhere in `tests/`.
- `../IMPLEMENTED.md:21` says "38/38 unit tests pass". That is exactly the math
  binary's count and under-reports the current 61 by 23.

**Fix:** link `ArtAttackEngine` from `tests/core` (three lines), then write
`tests/core/partitioner_tests.cpp`, `tests/render/resolution_manager_tests.cpp`,
`tests/render/camera_tools_tests.cpp` and `tests/render/viewport_manager_tests.cpp`.
None of these is seam-blocked; they cover pure arithmetic that survives the seam
untouched, and two of them contain live bugs from theme H.

## L. The vocabulary did not move with the files

`PHILOSOPHY.md:296-299`: "Engine headers contain no game nouns… if a game built on
the engine would need to edit it, it is game code." The engine is mostly clean —
`engine/core/`, `engine/assets/`, `engine/audio/`, `engine/app/` and
`engine/math/matt_math.*` have no game nouns at all, and `ResourceLoader::register_kind`
is a textbook T1 mechanism. The breaches cluster, and every one is a *policy* leak
wearing a noun:

| Site | The noun | Why it is policy |
|---|---|---|
| `engine/math/colour.h:308-312` | `TEAM_BLUE`…`TEAM_GREEN` | The paint-shooter's team palette, in the module that depends on nothing, in the lowest layer of the build |
| `engine/render/viewport_manager.h:18-20` | `"sprite_sheet_1"`, `"pixel"`, `DIVIDER_COLOUR` | Engine constants naming a game texture atlas the sample's manifest does not declare — plus a style decision |
| `engine/render/camera_tools.cpp:10-22` | scroll margins in pixels | One game's camera feel, and arithmetically wrong for the engine's own split-screen (theme H) |
| `engine/render/screen_layout.h`, `screen_resolution.h` | four layouts, four resolutions | Closed enums plus a switch: a stranger's first change to the engine is editing an enum in two files, and `game/save.cpp` already duplicates the table byte for byte |
| `engine/core/i_game_object.h:36-56` | `Level`, `Player`, `ICollisionGameObject` | Engine header comments documenting engine contracts in terms of game classes the engine target cannot even include |

`samples/minimal` touches none of the breached APIs, which is why the second client is
not currently catching any of this.

**Fix:** parameters, not renames. A `DividerStyle` the game passes in; camera margins
as a struct the game supplies; `ScreenResolution` as a `Vector2I` with a named-preset
table on the game side; the `TEAM_*` colours next to `game/objects/team_colour.h`.

## M. Naming, dialect and the second language inside the engine

Four conventions passes have genuinely landed, and every mechanical rule is at or near
100%: zero include guards, zero `get_` accessors, zero `using namespace` in any
header, zero non-lowercase filenames, 28 of 29 enums already `enum class`. **Every rule
requiring a judgement call is close to 0%.**

- **The two prefix families CONVENTIONS bans by name** (`:127-128`): `I*` — 4 types,
  135 occurrences, 37 files, one of which is the document's own worked counter-example
  `IGameObject` — and `M*` — 6 widget types, 323 occurrences, 14 files, plus 82
  lowercased `mobject` spellings in function and variable names.
- **Three DirectXTK/Microsoft template files were relocated into `engine/` and wrapped
  in `namespace artattack` without being translated**: `device_resources.{h,cpp}`,
  `step_timer.h`, `throw_if_failed.h`. Over two hundred `m_` members, PascalCase
  methods, `Get*` accessors, `c_`-prefixed constants, `ppAdapter` Hungarian — and the
  Windows SDK's `interface` macro used as a class-key on `IDeviceNotify`, which
  `Application` publicly inherits. `git show 3ed124a` opens two of them with
  `namespace DX`; the copyright notice is absent. (The two sweeps that counted the
  `m_` members disagree, 229 against 212, which is itself a symptom: nobody owns these
  files.) Nothing in the tree marks them as vendored, so a stranger learning the
  conventions from the source meets two dialects inside one namespace and cannot tell
  which is the engine's.
- **SCREAMING_SNAKE is the codebase's default for constants** — 600+ declarations,
  including two SCREAMING *typedefs* (`AABB`, `OBB`) — which spends the one signal
  `CONVENTIONS.md:130` reserves so that "a screaming name always signals preprocessor
  danger".
- **Namespace discipline is inverted.** The engine claims the global namespace in
  exactly one file (`engine/math/colour.h`), while 99 of 115 game and sample files
  claim it for everything they declare, and 35 of 46 game `.cpp` files hoist three to
  five namespaces including `using namespace artattack;` — which erases the only
  textual marker of which side of the boundary a name is on. `samples/minimal`, written
  after CONVENTIONS existed and described by its own build file as the new-project
  template, reproduces the pattern exactly.
- **The sharpest single instance:** `engine/render/viewport_manager.h:15-21` and
  `engine/render/camera_tools.cpp:10-22` each carry a comment citing "CONVENTIONS,
  Constants and enumerators" **by name**, and then spell their constants in the exact
  SCREAMING form that same section reserves for macros. The placement half of the rule
  was applied deliberately and the casing half was not, in code written specifically to
  comply.
- `std::exception(const char*)` — an MSVC-only extension — is used **47 times**,
  including inside the render module, in a codebase whose stated eventual goal is a
  second platform.

**Fix:** do the `I`/`M` rename in the same pass as the seam's signature edits (theme
A) — it is the same 20 overrides and 13 headers. Mark the three vendored files as
vendored, with attribution, and exempt them explicitly rather than silently. The
SCREAMING sweep is mechanical and can wait; `colour.h` cannot, because
`inline constexpr` deletes a whole class of cost with it.

## N. Where the design document is the thing that is wrong

Filed against the documents, not the code.

| Document | Statement | Reality |
|---|---|---|
| `PHILOSOPHY.md:371-372` | "render workers own disjoint slices of the scene" | `level.cpp:514-558` has every worker draw every object; the axis is views. `../IMPLEMENTED.md:88-93` already records the correction and the document has not been amended, which is the one process promise `PHILOSOPHY.md:27-29` makes about itself |
| `PHILOSOPHY.md:415-416` | "the parallel paths the engine already commits to (multi-core update and render)" | The only two `add_task` call sites in the repository are draw paths (`level.cpp:406`, `menu_page.cpp:145`); `update_level_logic` is a serial loop |
| `ARCHITECTURE.md:81-93`, `:128-138` | "the authoritative roster of modules and folders" | Four folders hold something else, three module-table rows are false, two backend folders have never existed (theme B) |
| `ARCHITECTURE.md:105` | `external/` holds rapidjson **and DirectXTK** | vcpkg has bought DirectXTK since the conversion; `external/` holds rapidjson alone |
| `ARCHITECTURE.md:63-65` | `ctest` runs "in CI" | There is no CI configuration of any kind in the repository |
| `PHILOSOPHY.md:420-422` | benchmarks "run alongside the test suite"; a throughput regression "is a defect" | No benchmark suite exists, and no reviewer in 84 reports noticed the obligation |
| `PHILOSOPHY.md:452-454` | compile speed is "a maintained property" | `game/pch.h` force-includes twenty headers into all 46 game TUs, sixteen of them unreferenced, including the entire DirectXTK 3D model pipeline in a project whose first non-goal is 3D; the 32 engine TUs have no PCH at all |
| `engine/math/CMakeLists.txt:1-2` | cites `ARCHITECTURE.md` as authority for DirectXTK-in-math | ARCHITECTURE says the opposite (`:130`, `:133`) |
| `cmake/settings.cmake` | its values are "carried over from the solution this build replaced" | `/fp:fast` and `/Zc:__cplusplus` were in all four old configurations and are in none of the new ones |
| `../IMPLEMENTED.md:46, 68-93` | `#16` — the data race is fixed, only the redundancy remains | The narrow defect (a `draw()` assigning its own members) genuinely is fixed and compiler-enforced. The contract it depends on is broken one call deeper, inside DirectXTK (finding 1). The status line is misleading in scope |
| `../IMPLEMENTED.md:31-32` | `#1` and `#2` Outstanding | Both landed; the document is dated today and still reports them outstanding |
| `sound_bank_loader.h:14-16`, `sound_bank.h:13-17`, `sprite_sheet_loader.h:14-15` | headers promising specific throws | None of the three exceptions can occur (theme F) |

`PHILOSOPHY.md:11-14` and `ARCHITECTURE.md:7-8` both say they describe the destination
and not the current codebase, which correctly disarms most description mismatches. It
does **not** disarm these: a false safety property, a false module roster presented as
authoritative, a build comment citing a document that says the opposite, and a status
document that is out of date on both directions of its own subject.

**Fix:** amend `PHILOSOPHY.md:371-372` to state the real invariant; mark
`ARCHITECTURE.md`'s tree entries that do not yet exist; fix the two false lines in
`ARCHITECTURE.md` (`:63-65`, `:105`); correct `engine/math/CMakeLists.txt:1-2` and
`cmake/settings.cmake`'s comment; add a scope note to `../IMPLEMENTED.md`'s `#16`
entry and update `#1`/`#2`.

## O. Everything the repository is not

Four categories no module or sweep asked about, filled only at the end:

- **No CI.** No `.github/`, no pipeline of any kind, against `ARCHITECTURE.md:63-65`.
- **No `.editorconfig`, no `.clang-format`.** `.gitignore` and `.gitattributes` are the
  unmodified Visual Studio templates, and one of their rules (`*.wav` / `*.xwb`) has
  quietly become load-bearing in the worst possible way (finding 9).
- **The whole of `game/content/` is byte-compared and mirrored into every preset's
  output on every build**, driven by `game/CMakeLists.txt:91-97`: 178 files and 336 MB
  staged where `game/content/manifest.json` names 14 files and 160 MB — including
  `texconv.exe`, `MakeSpriteFont.exe`, `XWBTool.exe`, 355 KB of Publisher level
  sketches and four unloadable levels. `PHILOSOPHY.md:454` calls a build-time
  regression a defect.
- **`game/pch.h` pins `_WIN32_WINNT` to `0x0601`** while an engine TU compiles at
  `0x0A00` — measured with `#pragma message` — which also blocks the DPI remedy.

**Fix:** a `.github/workflows/ci.yml` that configures both presets and runs `ctest`;
prune the ignore templates; stage what the manifest names rather than the folder.

---

# What to do first, in dependency order

Eleven of the findings in this review **delete themselves** in steps 3-5, and three of
them have remedies as written that are pure throwaway — including installing a
destructor on a class that should be deleted, and fixing `#13` in a game header that
has to move first. The order below is built to avoid paying for those.

### Step 0 — this week, all independent, none blocked by anything

| Task | Site | Cost |
|---|---|---|
| Hold text as `std::wstring`, call the wide `DrawString` overloads | `text_object.cpp:36-45, 53-62, 74-83, 116`, `interface_gameplay.cpp:156-202`, `debug_text.cpp:106-117` | an afternoon; ends the only live UB |
| Delete both `sampler_state_` members; read `PointClamp()` per draw | `level.h:149`, `level_builder.h:39` | two deletions plus an accessor |
| Move `require_*` into a shared header; route both loaders through it | `asset_manifest_loader.cpp:23-54` → `sprite_sheet_loader.cpp`, `sound_bank_loader.cpp` | half a day |
| Root `README.md` + `LICENSE`; un-ignore the wave bank or document its build step | `.gitignore:365-368`, `ARCHITECTURE.md:75-109` | an hour, and it is what makes the rest of this repository usable by anyone |
| Delete `MatrixF`, `Matrix3x3F`, `Vector4F` and the seven zero-caller free functions | `matt_math.h:692-773`, `matt_math.cpp:2471-3045` | ~600 lines out, zero callers |
| Give `TriangleRightAxisAligned` a `clone()` override | `matt_math.h:908` | one method; every level ramp stops being sliced |

### Step 1 — the client's visible defects (week 1)

All of theme H. Every entry is one to five lines, none touches a signature the seam
will rewrite, and together they are the difference between a proof that works and a
proof that has not been run. Start with the camera clamp
(`camera_tools.cpp:91-108`), the viewport switch (`viewport_manager.cpp:143-234`) and
the layout restore (`game_states.cpp:131-137`), because those three are visible in
every multiplayer session.

Also validate `ApplicationOptions` in `Application::initialize()` with messages naming
the field — it is the struct a stranger fills in first, and `max_threads = 0` is
currently an SEH divide-by-zero two modules away with no C++ handler.

### Step 2 — the regression net (a day, and it must come before step 3)

Link `ArtAttackEngine` from `tests/core` (three lines; the recorded reason not to is
false), then write tests for the five engine types that need no device:
`Partitioner`, `ResolutionManager`, `CameraTools`, `ViewportManager`, `ThreadPool` —
plus `normalize()`/`normalized()`/`to_unit_vector()`, whose two-policy contract is
load-bearing and entirely uncovered. This is the only thing standing between the next
two steps and a silent regression, and none of it is seam-blocked.

### Step 3 — the renderer seam (findings 4, 5)

`engine/render/renderer.h` with the surface listed in finding 4; `device_resources`
moved to `engine/render/d3d11/`; `samples/minimal` ported first as the acceptance test.
Do the interface-signature edits in one pass with it: `I` prefix removal,
`SpriteBatch*` → `Renderer&`, `SpriteEffects` → an engine enum,
`State::update(float dt)` and `State::draw(Renderer&) const`. Decide and record in
ARCHITECTURE whether the seam is a vtable or a compile-time-selected type.

### Step 4 — `Scene` (findings 6, 7)

Eleven members and nine methods out of `Level`, with the view list designed as part of
step 3. Expect `camera_tools_` and `camera_bounds_` to be the contested joint.

### Step 5 — `CollisionObject` (finding 8, and outstanding `#13`)

`engine/collision/collision_object.h` with `shape()`, layer/mask and an opaque game
tag; `Partitioner` to `engine/core/`. Then `CollisionObjectType`'s 22 enumerators
become a layer plus a game tag: 155 occurrences across 14 files, all in `game/`, zero
in `engine/`, `samples/` or `tests/` — so it is a large but entirely local edit, and
doing it *before* the interface moves means doing it twice.

### Running in parallel, blocked by nothing above

The state stack (`StateContext` push/pop plus deferred transition — closes two live
menu bugs), widget focus and navigation, and the input module all touch files steps
3-5 never open. So does the document work in theme N, and it should be done by whoever
next edits each document rather than batched.

### Deliberately deferred

`colour.h` → `inline constexpr` (do it whenever, it is self-contained and deletes a
measurable startup cost); the SCREAMING_SNAKE sweep; the weapon/projectile registry
(theme J) — it is the right destination and it wants the factory registry that
`ResourceLoader::register_kind` already is, but it is a bigger job than it looks and
nothing else queues behind it.

### Two open questions, answered from the evidence

- **The renderer seam's contract** is already jointly specified by six findings across
  five groups: a per-view unit of work, per-draw depth, the sampler chosen inside,
  RAII command lists, an engine `Font` with `measure()`, and a wide-string text entry
  point. There is nothing left to decide except vtable-vs-concrete.
- **Event-based logic**, the second standing design question, has no case in this
  codebase that needs it. Every candidate resolves to something simpler: collision
  response is a returned value, the narrow phase wants a contact manifest, a child
  state's result wants the state stack. The absence of any case requiring a sender that
  does not know its receivers is itself the answer, and it agrees with
  `PHILOSOPHY.md:200-203`, which names observer webs as the thing to rethink.

---

# Per-group results

`M` = module group (verified by a second agent) · `S` = repo-wide sweep
(self-verified) · `G` = gap-fill (self-verified). "Rej." and "Add." are claims
rejected and findings added by the verification pass.

| Group | | Findings | Rej. | Add. | Sharpest thing in it |
|---|:-:|--:|--:|--:|---|
| `math-header` | M | 27 | 5 | 5 | 27 headers include `matt_math.h`, so its 20 types, zero `explicit`, zero `[[nodiscard]]` and zero `noexcept` reach the whole project |
| `math-impl-a` | M | 15 | 0 | 5 | `segments_intersect` is the one open-boundary primitive in a closed library, and its `a==b` fast path misses the reverse spelling |
| `math-impl-b` | M | 15 | 4 | 5 | `shape_shape_collision_direction` returns ZERO for intersecting shapes, laundered into `DIRECTION_RIGHT`, making `player.cpp:299`'s throw dead code |
| `math-impl-c` | M | 26 | 0 | 7 | `angle_0/1/2()` return the exterior angle; `RectangleRotated` cannot be rotated at all through its API |
| `math-ericson` | M | 18 | 0 | 4 | Swept AABB returns a collision window for boxes with zero relative velocity; ~422 of 929 lines are commented-out 3D source |
| `math-collision-tools` | M | 11 | 0 | 1 | All four correctness defects the last review recorded in the resolver are still live, none covered by a test |
| `math-colour` | M | 11 | 0 | 3 | 301 dynamically-initialised constants in 59 TUs; `colour_from_name` is ill-formed NDR and falls back to white |
| `core-registry` | M | 15 | 0 | 3 | `Registry::get()` is const and lends mutable access; `add` cannot tell an insert from a replace |
| `core-objects` | M | 15 | 0 | 4 | `engine/core` names a vendor type in a public signature — the one line that fails the module contract |
| `core-threading` | M | 19 | 4 | 2 | `~Application` calls `CoUninitialize()` before any COM member is released, on every exit |
| `collision-input` | M | 12 | 2 | 2 | `partition()` has never once partitioned: every call passes `n <= k` |
| `render-device` | M | 15 | 0 | 3 | **Critical:** D3D11 is the public draw API; twelve of seventeen accessors have no caller |
| `render-viewports` | M | 16 | 0 | 3 | Camera dead-zone inversion at the default resolution; `ScreenResolution` snaps unknown sizes to 720p |
| `render-primitives` | M | 17 | 0 | 4 | `Visual`'s `RectangleRotated` constructor discards the angle, and the setter meant to apply it is an empty TODO with no callers |
| `render-sprites` | M | 16 | 0 | 2 | Handle discipline is complete on the draw path and absent on the update path; `RECT` is the storage of both leaf types |
| `render-text` | M | 14 | 0 | 1 | **Critical:** the shared `SpriteFont` UTF-8 buffer race |
| `ui-widget` | M | 16 | 0 | 3 | Rescale policy in the base class already overflows the team fill bars 1.5x at 1280x720 |
| `assets` | M | 17 | 0 | 4 | **Critical:** unchecked rapidjson reads; range validation absent everywhere presence validation exists |
| `audio` | M | 14 | 1 | 1 | `engine/audio/` contains zero references to `AudioEngine`; the device lives naked in `Application` |
| `app-shell` | M | 16 | 1 | 3 | Two competing notions of "the size"; the window is sized as an outer rect, permanently |
| `game-player` | M | 26 | 0 | 6 | The left-stick deadzone is applied twice; `respawn()` restores half of what death changed |
| `game-weapon` | M | 20 | 0 | 5 | A sixth weapon edits nine files; the fire loop's voice has no owner |
| `game-projectile` | M | 17 | 0 | 3 | The structure filter is written twice by hand and both copies omit both ramp types |
| `game-level` | M | 18 | 1 | 5 | **Critical:** the sampler cached in two objects; a 21-parameter constructor with four adjacent `RectangleF`s |
| `game-world-objects` | M | 14 | 0 | 2 | 4,116 polymorphic paint tiles in one level — `PHILOSOPHY`'s own worked example, inverted |
| `game-hud` | M | 16 | 0 | 4 | **Critical:** the `SpriteFont` race from the game side; `rand()` is never seeded |
| `game-main-menu` | M | 19 | 0 | 3 | 51 string comparisons standing in for a focus model; two of the likeliest presses are silent no-ops |
| `game-menu-framework` | M | 18 | 1 | 2 | One index is both a player number and a viewport subscript — the same defect class fixed three times last round |
| `game-states-flow` | M | 19 | 0 | 3 | Pausing never silences a weapon's loop; the two input pumps are alternated, so edges diff across modes |
| `game-shell` | M | 27 | 0 | 4 | Every failure signal `save.cpp` produces is dropped, including `fclose`'s |
| `samples-minimal` | M | 16 | 1 | 4 | **Critical (via sweep):** the template teaches D3D11; the hint string leaves the surface on a title-bar drag |
| `tests` | M | 20 | 0 | 5 | No binary links `ArtAttackEngine`; `value ± FLT_EPSILON` at 10.0f is the same float |
| `sweep-game-nouns-in-engine` | S | 7 | — | — | Every vocabulary leak follows a policy leak; all of them are in `engine/render/` |
| `sweep-mechanism-vs-policy` | S | 15 | — | — | **Three criticals:** no `Scene`, no collision in `engine/collision/`, the sample writes D3D11 |
| `sweep-module-graph` | S | 13 | — | — | No cycles; every violation is outward into DirectXTK, D3D11 or Win32 |
| `sweep-naming` | S | 21 | — | — | Mechanical rules ~100%, judgement rules ~0%; two files cite CONVENTIONS and break the rule they cite |
| `sweep-files-includes` | S | 19 | — | — | The rules were applied file by file and never to the seams between files |
| `sweep-namespaces` | S | 11 | — | — | The wrap stopped at the `engine/` directory boundary; `samples/minimal` inherited the old habits |
| `sweep-value-semantics` | S | 16 | — | — | `Handle`/`Registry`/`NameTable` are exactly right, inside a codebase whose default grammar is still inheritance and heap |
| `sweep-compile-time-guardrails` | S | 19 | — | — | `[[nodiscard]]` appears zero times; almost every converting constructor in `matt_math.h` is implicit and lossy |
| `sweep-loud-failure` | S | 15 | — | — | **Critical:** two incompatible conventions for reading content data, in the same folder, from the same era |
| `sweep-clarity-duplication` | S | 19 | — | — | Where a helper exists the call sites retype its body — one shadows the free function with a local of the same name |
| `sweep-frame-loop-cost` | S | 15 | — | — | `draw` cannot receive the cull rectangle, so an object standing for N things draws all N or none |
| `sweep-memory-safety` | S | 11 | — | — | Round 1's hits are genuinely gone; what remains is three instances of one unexamined destruction-order question |
| `sweep-threading` | S | 14 | — | — | The engine ships the pool and none of the renderer, so both clients wrote one and they have diverged |
| `sweep-lifetimes` | S | 11 | — | — | **Critical:** the sampler cached twice; 49 of 58 non-owning pointer members are undocumented |
| `sweep-build` | S | 18 | — | — | Honest CMake building a repository that is not; `${CMAKE_SOURCE_DIR}` ten times |
| `sweep-testability` | S | 15 | — | — | 30 of 32 engine TUs uncovered; every free-to-test-and-untested type contained a defect |
| `sweep-public-face` | S | 18 | — | — | **Critical:** no README, no licence, no prerequisites; the sample exercises four of ~50 public types |
| `sweep-dialect` | S | 17 | — | — | The macro clause of T12 is almost clean; the dialect is made of everything that reads like a macro and is not |
| `sweep-docs-accuracy` | S | 20 | — | — | The remediation is real and survived; the documents describing it have not kept pace |
| `sweep-content-data` | S | 14 | — | — | **Critical:** one hardened door and two open ones; 336 MB staged where the manifest names 160 |
| `gap-coverage` | G | 7 | — | — | The source has been read hard and the repository around it has not: no CI, no PCH hygiene, no ignore-file review |
| `gap-structure` | G | 6 | — | — | Every defect is a value crossing a seam with two meanings; the end-of-match camera frames 64% of the arena |
| `gap-roadmap` | G | 13 | — | — | **Critical:** the seam is a hard prerequisite for `Scene`; eleven findings delete themselves in steps 3-5 |
| `gap-adversary` | G | 12 | — | — | The review has two halves with two standards of evidence and nothing on the page said which |
| **Total** | | **901** | **21** | **111** | |
