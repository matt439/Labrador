# ArtAttack — Code Review

> Read-only review by 99 agents across ~28,000 lines / 197 first-party source files.
> `external/rapidjson` and `packages/` excluded as vendored third-party.
>
> **This document is the review as written, and is not updated as findings are
> fixed.** For what has since been remediated, see [IMPLEMENTED.md](IMPLEMENTED.md)
> — 31 of the 36 critical findings are fixed as of 2026-08-07.

**879 findings** — 40 critical · 233 high · 404 medium · 202 low.

Module findings passed through per-module verification agents that re-opened the cited lines; 14 candidates were rejected as inaccurate. Collapsing reports where multiple agents independently found the same defect gives **781 distinct issues** (36 critical, 205 high, 347 medium, 193 low).

Full detail for every finding: `ArtAttack-review-all-findings.md`. Raw data: `ArtAttack-review-findings.json`.

---

## Verification notes

Findings from the 12 module groups were re-checked by per-module verification agents that reopened the cited lines. The repo-wide sweeps and gap-fill agents were **not** put through that layer, so treat their findings as slightly lower confidence.

Independently spot-checked against the source after the run:

| Claim | Result |
|---|---|
| 1-byte JSON read buffer sized with `sizeof(unique_ptr)` | **Confirmed** — `make_unique<char>()` + `sizeof(read_buffer)` at `Save.cpp:16`, `SoundBank.cpp:154`, `SpriteSheet.cpp:74`, `LevelLoadedInfo.cpp:100`; `Save.cpp:23` then `.release()`s it |
| `Projectile::get_delete_timer()` returns `_timer` | **Confirmed** — `Projectile.cpp:220` and `:285` both return `_timer`, so `:278` tests `_timer > _timer` |
| `Weapon::stop_sounds()` throws for Sniper and Bucket | **Confirmed** — `Weapon.cpp:273` `default:` throws; only SPRAYER/ROLLER/MISTER are handled |
| `closest_pt_point_OBB` iterates 3 axes in 2D | **Confirmed** — `EricsonMath.cpp:774` `for (int i = 0; i < 3; i++)` |
| `RectangleRotated(Segment, float)` "always throws" | **Overstated.** `_points` has a 4-element default initialiser (`MattMath.h:1119`), so there is no throw or out-of-bounds read. The real defect is worse in a different way: **both** constructors (`MattMath.cpp:4194`, `:4209`) call `is_valid()` before `_points = calculate_points()`, so `edges_valid()` validates four zero-points instead of the actual rectangle. The validation is vacuous — it cannot reject an invalid rectangle. |

---

## Verdict

The strongest thing in this codebase is that it works at all: a split-screen multiplayer game with paint accumulation, level loading from JSON, a fixed-step simulation, deferred-context multithreaded rendering, and a unit-test project — assembled by someone who started it in year one of a degree. That ambition is genuine and the newest code (`RectangleRotated`, the OBB work, the test project) shows a visibly higher standard than the oldest. But the review found 40 critical and 233 high findings across 197 files, and they are not scattered: they cluster into a handful of decisions made once, early, and then copy-pasted. The single worst class of defect is memory corruption on the happy path — `auto read_buffer = std::make_unique<char>(); FileReadStream is(fp, read_buffer.get(), sizeof(read_buffer));` writes 8 bytes into a 1-byte allocation, and that exact line is duplicated in `Save.cpp:16-17`, `SpriteSheet.cpp`, `SoundBank.cpp` and `LevelLoadedInfo.cpp`, so every launch and every asset load overruns the heap. Sitting beside it: `Weapon::stop_sounds()` throws for two of the shipped weapons at the end of every round with no `catch` anywhere in the process, and `draw()` mutates `std::string` members on objects that `Level::draw_active_level` hands to sixteen thread-pool workers simultaneously.

Structurally, what holds this back is not bad C++ so much as the total absence of a boundary. `ICollisionGameObject` — a core engine interface — identifies objects with a 23-value enum that cross-multiplies object class × team × weapon. `IGameObject` names `DirectX::SpriteBatch`. `MenuPage`, the base class for all UI, owns deferred contexts and command lists. `ResourceLoader` hardcodes twelve asset filenames. There is no engine target at all: `ConfigurationType=Application` in all four configurations, 193 files flat in one directory, two filters. Nothing here can be linked by a second game, or by its own tests, which is why `MattMathTests` resorts to `#include`ing `.cpp` files.

The engine goal is reachable, but not by refactoring uniformly. `MattMath`, the collision layer, `Level`, `Player`, `Weapon` and the menu system all need to be split before they can be improved — the work is *extraction*, not *cleanup*. Three things want outright rewriting rather than repair: `CollisionTools` (a 40-iteration bisection that mutates geometry instead of producing a contact manifold), the `MatrixF`/`Matrix3x3F`/`Vector4F` sublibrary (entirely dead and entirely broken — the constructor never sizes its storage), and the paint/scoring rules currently living inside `Level` and `IPaintableGameObject`. Everything else is salvageable.

## The five things that matter most

1. **Fix the four copies of the 1-byte JSON read buffer, and the throwing/racing paths beside them.** This is undefined behaviour executed on every launch and every level load, hidden only by CRT allocation granularity — a Release build on a different allocator may behave differently tomorrow. Together with `Weapon::stop_sounds()` terminating the process at match end and the unguarded `ThreadPool::work_callback`, these are the findings that make the program unsound rather than merely unpleasant. **Scale: a day.** One shared `load_json(path)` helper that checks `fopen`, sizes its buffer, and inspects `HasParseError` deletes four findings and four future ones.

2. **Split the solution into an engine static library and a game application.** Nothing else on this list can be verified without it: today there is no compiler-enforced answer to "is this engine code?", which is precisely why `player_team`, `level_stage`, Splatoon team colours and a weapon catalogue drifted into interfaces, headers and the resource cache. Making `ArtAttack` a `StaticLibrary` plus a thin `Game` app immediately turns every layering violation into a link error you can work through. **Scale: a week to split, then months of incremental extraction — but the split itself is mechanical and pays out on day one.**

3. **Introduce a contact manifold and delete the bisection resolver.** `CollisionTools` has no normal, no penetration depth and no MTV, so "direction" is a centre-to-centre unit vector that callers quantise to eight compass points and throw on. The sign-inverted push-out, the swapped band cases, the inverted containment axis, the depth query that heap-clones a shape to answer a boolean, and the silent failure to separate deep overlaps are all downstream of that one missing type. **Scale: 1-2 weeks**, and it retires a dozen findings plus the empty `// TODO` that makes left ramps non-solid.

4. **Make `draw()` const and stop mutating simulation state from render threads.** `Player::draw`, `Weapon::draw`, `PaintTile::draw` and `TextDropShadow::draw` all follow "set members, then draw", while every worker iterates every object — an unsynchronised race on `std::string` members, sixteen threads wide. Pass draw parameters instead of storing them; the const-correctness follows automatically, as does the ability to reason about the parallel renderer at all. **Scale: 1-2 weeks**, touching every renderable but shallowly.

5. **Set `<LanguageStandard>` to C++20 and turn on `TreatWarningAsError`.** The project silently builds as C++14, which is why the codebase hand-rolls `clamp` and `lerp`, cannot use `std::optional` or `[[nodiscard]]`, and ships dynamically-initialised const aggregates in headers. `/W4` is on but nobody reads the output — there is a committed unreachable duplicated `return`. **Scale: an afternoon to flip, a week to clear the fallout**, and it makes the compiler an ally for every step above.

## Suggested order of attack

1. **Toolchain first (day 1).** `<LanguageStandard>`, `TreatWarningAsError`, and align the test project's `/fp:` and warning level with the game's — right now tests validate different arithmetic than ships (`FloatingPointModel=Fast` in all four game configs, `precise` in the tests). Every later change is safer with warnings failing the build.
2. **Stop the corruption and the crashes (week 1).** The shared JSON loader, `ResourceManager`'s four `operator[]`-in-a-dead-`catch` getters replaced with `.at()`, an exception barrier in `work_callback`, and the `stop_sounds` default case. Cheap, isolated, and they stop the ground moving under you.
3. **Split engine from game (weeks 2-3).** Static library + app. Do not chase the leaks yet — just get the boundary to exist and let the linker enumerate the violations.
4. **Fix the shape contracts, then the collision layer (weeks 3-6).** Document and unify `get_edges()` ordering — that alone fixes the live shipped bug where three of four paintable faces paint on the wrong side — repair `triangles_intersect`'s four-of-nine edge pairs, delete the dead `MatrixF` subsystem, then build the contact manifold on top. Collision must be trustworthy before `Level` is decomposed, because the decomposition will move that code.
5. **De-fang the lifetime story (weeks 6-8).** `OnDeviceRestored` should recreate GPU resources, not reconstruct `ThreadPool`, `ResourceManager` and the `unique_ptr<float>` dt that eleven classes hold by raw pointer; fix `Game`'s member-vs-base destruction order so `AudioEngine` outlives its WaveBanks. Do this before extracting more subsystems, so the extracted ones inherit a correct ownership model instead of the current one.
6. **Then decompose the god classes (ongoing).** `Level`, `Player`, `Weapon`, `MenuPage` — in that order. Each is a large job, but by this point the boundary exists, the collision code beneath them is correct, and lifetimes are stated, so the decomposition is a series of moves rather than a rewrite.

---

## Per-module results

| Module | Confirmed | Rejected by triage | Dominant themes |
|---|---:|---:|---|
| `math-vectors` | 51 | 3 | Dead-and-wrong public API: cross, sign, rotate_vector_by_ref, desaturate and the entire MatrixF/Matrix3x3F subsystem are all defective and all uncalled — shipped in the engine header waiting for a first caller; Value types fused with polymorphic behaviour: RectangleF/Circle/Triangle/Quad inherit Shape, costing a vptr, constexpr-ability, safe assignment and trivial copyability on the engine's most-copied type; Duplication instead of abstraction: four unitisation functions with two contradictory zero-length contracts, five copies of the camera transform, six retypings of the convex-vs-convex intersection algorithm — and the copies have already diverged into real bugs |
| `math-shapes` | 66 | 0 | Undefined interface contracts on Shape: get_edges() ordering, inflate() semantics, and the missing contains()/transform() are unspecified, so polymorphic callers get different geometry depending on the concrete type behind the pointer - the biggest blocker to MattMath being a reusable engine core, and it has already produced a live face-painting bug in StructurePaintable.; Validation theatre in Quad and RectangleRotated: constructors and setters throw aggressively, but the checks run before the state they validate exists (edges_valid), use absolute epsilons on size-dependent quantities, mutate before validating so a throw leaves the object corrupt, and are bypassed entirely by `= default` default constructors that produce objects failing the class's own is_valid().; Allocation-heavy hot paths: get_edges/get_points/get_triangles all return std::vector by value, RectangleRotated caches four fixed corners in a heap vector rebuilt on nine mutators, point-in-OBB round-trips through a validating Quad, and the OBB 'cheap AABB early-out' is more expensive than the test it guards - all inside an O(n^2) per-frame loop that CollisionTools reruns dozens of times per contact. |
| `math-collision` | 49 | 0 | No contact manifold: resolution is a 40-iteration bisection over mutated geometry instead of an analytic MTV, which is the root cause of the sign-inversion, band-swap, containment-axis, depth-by-simulation and silent-failure-to-separate bugs; 3D textbook code ported to 2D without adapting it: a 3-axis loop on a 2-axis type that throws unconditionally, hand-unrolled axis blocks with a zero-velocity hole, ~40% commented-out dead code, and four exported functions with zero callers; Game rules welded into engine types: collision identity is a class x team x weapon cross-product enum on the engine's own collision interface, ramp facing is a collision category, and DirectX::SpriteBatch is baked into the root game-object interface |
| `player` | 51 | 2 | Player is a god class with a non-virtual DrawObject diamond — no part of it is reusable without the Splatoon rules attached; Rendering mutates simulation state, and the parallel draw path makes that an unsynchronised data race on std::string members; Index-by-position contracts with unchecked operator[]: compacted input vectors and global-index-into-per-team spawn arrays |
| `weapons` | 38 | 3 | Throwing from teardown and lookup paths: get_sound_effect_instance_name(), get_sound_name() and WeaponBuilder all throw std::exception on a default case, and none of those paths has a catch - a missing sound becomes a terminated process; Mutable draw state on shared objects: the whole draw path is 'set members, then draw' rather than 'pass parameters', which is both the source of the multi-threaded data race and the reason draw cannot be const; Type tags standing in for polymorphism: wep_type is simultaneously the factory discriminator, a stored member, a menu option and an array bound, and the base class switches on it - so adding a weapon means editing the base |
| `projectiles` | 36 | 0 | Hand-copied state that should be derived: the projectile_type tag, the structure-type list, the update() prologue and the centring offset are each duplicated across five leaf classes, and each has already drifted at least once (mist tagged SPRAY, ramps missing from both copies of the structure filter, Rolling missing its offset and animation tick); Game rules welded into what should be engine code: player_team, a two-team switch with a throwing default, and a weapon-x-team cross-product collision_object_type enum are all compile-time dependencies of the base Projectile class, and the enum is re-expanded by hand in six files; Accessors that return the wrong member: get_delete_timer() returns _timer, get_col_rect_size() and get_base_size() return `size` instead of `col_rect_size` — all three hidden by identically-shaped names and by tuning data where the two fields happen to be equal |
| `level` | 57 | 2 | Data smuggled through geometry: four JSON booleans are encoded as Segments and decoded back via an undocumented edge ordering and exact float equality, producing the module's one live shipped gameplay bug; Enum tags used as a substitute for the type system: collision_object_type cross-multiplies class × team × weapon into 23 values, is re-derived by four duplicated predicate chains, and is recovered with a dynamic_cast behind a catch that can never fire; Engine and game welded together: paint scoring, a two-team model, match length, countdown assets and D3D11 deferred contexts all appear in what should be reusable engine types |
| `menus` | 60 | 0 | Input indexing: a compacted pad vector indexed as if it were a stable player-slot array, producing an unchecked OOB read in PauseMenu and an uncompletable team/weapon select; UI base class is a renderer: MenuPage owns D3D11 deferred contexts, command lists, sprite batches, thread pool and partitioner, so no UI can exist without DirectX — the single biggest blocker to the engine-reuse goal; Stringly-typed navigation: ~90 widget-name comparisons instead of a focus model, with the compared name snapshotted before the input loop and going stale after the first move |
| `rendering` | 46 | 0 | Unsafe C-idiom carryover in the asset loaders: sizeof on a smart pointer as a buffer size, unchecked fopen, release() instead of reset(), and zero JSON validation — a heap overflow on every sprite sheet and sound bank load; draw() mutates shared object state while the renderer runs it on N thread-pool workers over the same objects; nothing below IGameObject is const, and the const markers that do exist are laundered through unique_ptr members and a const getter returning a mutable ResourceManager*; Lookup failure is silent everywhere: map::operator[] in all four ResourceManager getters and in SpriteSheet::draw, each wrapped in unreachable error handling, so a typo'd asset name yields nullptr or an invisible zero-size sprite instead of a diagnosable load error |
| `platform` | 50 | 3 | Device-lost recovery is comprehensively broken: subsystems are reconstructed instead of GPU resources, deferred contexts are never recreated, and every cached raw pointer dangles afterwards; Ownership by raw pointer with no contract — deferred contexts, the parallel SpriteBatch* mirror, the heap frame-delta float, and the whole GameData service locator — makes lifetimes unanalysable and is the root of most findings here; Destruction order is load-bearing and undocumented: AudioEngine dies before its WaveBanks, the State tree dies after every service it borrows, and the Main.cpp globals are declared in reverse dependency order |
| `resources` | 53 | 0 | Copy-paste propagation of defects: the broken 1-byte JSON reader exists in 4 files, the operator[]-in-dead-catch getter in 4 places, and the resolution conversion table in 4 functions across 2 classes — every one of them a candidate for a single shared helper that does not exist; Error handling that looks present but is provably inert: catch blocks for exceptions the called code cannot throw (map::operator[], WaveBank::Play, WaveBank::CreateInstance), rapidjson asserts compiled out by NDEBUG, and bool failure returns consumed only to pick a std::cout message in a GUI app with no console; Silent nullptr instead of a loud failure: operator[] inserting nulls, reset_all_* keeping keys with null values, CreateInstance returning nullptr unchecked, and default constructors that leave dependencies null with no setter to repair them — every one converts a diagnosable lookup error into an access violation somewhere unrelated |
| `state-objects` | 47 | 1 | Lifetime and destruction order are correct only by coincidence — transition_to destroys the running state, Game destroys subsystems before the state tree that borrows them, and four menu member groups are declared in reverse dependency order; Game rules are welded into what should be engine interfaces: collision_object_type enumerates weapons×teams on ICollisionGameObject, IGameObject hardwires SpriteBatch and Camera, and TeamColour bakes exactly-two-teams into member names; Input identity is broken at the source: connection-compacted vectors are indexed by player number, producing an out-of-bounds read on controller disconnect and mis-attributed pause menus |

---

# Critical findings (36)

### 1. There is no engine target: one Application project, 193 files flat in one directory, 2 filters

`ArtAttack/ArtAttack.vcxproj:29, 36, 43, 51 (and ArtAttack.vcxproj.filters:1-12)` — **critical** · `design`

All four configurations set `<ConfigurationType>Application</ConfigurationType>` (lines 29, 36, 43, 51). The project emits an .exe and no .lib, so there is literally nothing another game could link against. All 193 first-party .h/.cpp files live in a single flat `ArtAttack/` directory, and `ArtAttack.vcxproj.filters` declares exactly two filters (`Common`, `Assets`) covering 6 of ~200 items — `Game.h`, `Level.h`, `Player.h`, `MattMath.h`, `weapon_consts.h` and `CollisionTools.h` are all siblings with no grouping.

The cost is already visible: `MattMathTests` cannot link the engine and instead `#include`s implementation .cpp files (MathTests.cpp:5-6 includes `..\ArtAttack\MattMath.cpp` and `EricsonMath.cpp`; CollisionToolsTests.cpp:3 includes `CollisionTools.cpp`). Only 3 of ~81 TUs are testable at all, precisely because there is no library boundary.

Every other finding in this sweep is downstream of this one: with no compilation boundary, nothing *stops* game concepts leaking into engine headers, so they all have.

**Fix:** Split into three MSBuild targets sharing a common `.props`: `MattMath` (StaticLibrary, zero deps), `ArtAttackEngine` (StaticLibrary, depends on MattMath + DirectXTK), `ArtAttackGame` (Application, depends on Engine). Mirror that on disk (see notes). The moment the engine is a static lib, a game include from an engine .cpp becomes a link/compile error instead of a silent coupling.

### 2. The 'engine' is ConfigurationType=Application, so it can never be linked by anything - including its own tests

`ArtAttack/ArtAttack.vcxproj:29,36,43,51` — **critical** · `design`

All four configurations declare <ConfigurationType>Application</ConfigurationType>. The project therefore emits no .lib, and there is no second project separating engine from game. The consequence is visible in the test project: MattMathTests/MattMathTests.vcxproj:173 declares a ProjectReference to ArtAttack.vcxproj, but because there is no library to link, MattMathTests/MathTests.cpp:5-6 does `#include "..\ArtAttack\MattMath.cpp"` and `#include "..\ArtAttack\EricsonMath.cpp"`, and MattMathTests/CollisionToolsTests.cpp:3 does `#include "..\ArtAttack\CollisionTools.cpp"`. The ProjectReference's only effect is to force a full game-EXE build before the tests run. Only 3 of the 81 translation units are reachable by tests at all, and the stated goal (a reusable 2D engine) is structurally impossible: there is nothing a second game could consume.

**Fix:** Split into three projects sharing one .props sheet: ArtAttackEngine (StaticLibrary - DeviceResources, StepTimer, MattMath, CollisionTools, ThreadPool, Partitioner, ResourceManager, SpriteSheet, State/StateContext, Drawer, ViewportManager), ArtAttackGame (Application - Level, Player, Weapon*, Projectile*, Menu*, TeamColour, collision_object_type), and Tests linking the engine lib. Delete the #include-a-.cpp hack once the lib exists.

### 3. Device-lost/restore destroys ResourceManager and the dt float, leaving every live drawable holding a freed pointer

`ArtAttack/DrawObject.h:36` — **critical** · `lifetime`

`DrawObject::_resource_manager` (DrawObject.h:36) and `Drawer::_resource_manager` / `Drawer::_dt` (Drawer.h:30-31) and `AnimationObject::_dt` (AnimationObject.h:49) are non-owning raw pointers captured at construction and never re-seated.

I read Game.cpp. `OnDeviceLost()` resets the sprite batches, resets `_states` (which is `CommonStates`, not the state machine) and calls `reset_all_textures/sprite_fonts/sounds`. It does NOT tear down `Game::_state` (Game.h:67, `std::unique_ptr<State>`), so the whole live object graph survives. `OnDeviceRestored()` then calls `create_device_dependent_resources()`, which at Game.cpp:238-245 does:
```cpp
this->_resource_manager = std::make_unique<ResourceManager>();  // old one destroyed
this->_dt = std::make_unique<float>(0.f);                       // old float destroyed
```
The `unique_ptr` assignments free the old objects. `GameData` is re-pointed; every already-constructed `Player`, `Visual`, `PaintTile`, `Text`, `TextDropShadow`, `InterfaceGameplay` and `DebugText` is not. The next `TextureObject::draw` → `SpriteSheetObject::get_sprite_sheet()` (SpriteSheetObject.cpp:33) dereferences freed memory, as does `Drawer::get_dt()` (Drawer.cpp:30).

The root cause is the design: nothing in `DrawObject`'s or `Drawer`'s API states that the pointer is non-owning, how long it must outlive the object, or who re-seats it. This is exactly the coupling that blocks the engine-reuse goal — every drawable holds a mutable back-pointer to a global service.

**Fix:** Short term: do not recreate `ResourceManager` and the dt float on device restore — keep the objects and only reload their GPU-side contents in place. Long term: delete `DrawObject::_resource_manager` entirely; drawables should hold resolved handles and receive whatever they need through the draw call, and frame time should be a `float` parameter to `update(float dt)` rather than a heap-allocated float whose address is threaded through ~26 headers.

### 4. closest_pt_point_OBB loops over 3 axes in 2D and throws on every call; zero tests

`ArtAttack/EricsonMath.cpp:769-786` — **critical** · `test-coverage` · independently reported by 4 agents

`closest_pt_point_OBB(p, b, q)` — the only OBB support routine, added with the recent RectangleRotated/OBB work — contains `for (int i = 0; i < 3; i++) { float dist = Vector2F::dot(d, b.get_axis(i)); ... }`. `RectangleRotated::get_axis` (ArtAttack/MattMath.cpp:4315-4327) is `if (axis==0) ... else if (axis==1) ... throw std::invalid_argument("Axis must be 0 or 1")`. The third iteration therefore throws unconditionally: the function can never return normally, for any input. It is declared in EricsonMath.h:141, has zero call sites in the engine, and has zero tests. A single test asserting the closest point on an axis-aligned OBB would have caught this on the day it was written. This is the clearest evidence that the RectangleRotated/OBB work shipped without behavioural coverage of its own primitives.

**Fix:** Change the loop bound to 2. Add a test class for the OBB primitives: closest point for a point inside the box (returns p), outside along +x axis, outside past a corner (clamps on both axes), and on a 45-degree-rotated box where the answer differs from the AABB answer.

### 5. Device restore rebuilds SpriteBatches on the 16 deferred contexts of the destroyed device

`ArtAttack/Game.cpp:282-287, 52, 229` — **critical** · `correctness`

`create_deferred_contexts` is called exactly once, from `Game::initialize` (Game.cpp:52) — grep confirms no other call site. `DeviceResources::HandleDeviceLost` (DeviceResources.cpp:418-453) does `m_d3dContext.Reset(); ... m_d3dDevice.Reset(); m_dxgiFactory.Reset(); CreateDeviceResources(); CreateWindowSizeDependentResources();` and then calls `OnDeviceRestored`. `Game::OnDeviceRestored` (Game.cpp:282-287) calls `create_device_dependent_resources()`, which at Game.cpp:226-232 does `std::make_unique<SpriteBatch>(this->_device_resources->get_deferred_context(i))` for i in 0..15 — i.e. it builds brand-new SpriteBatches, and `_states = make_unique<CommonStates>(device)` at line 252 on the NEW device, on top of the OLD deferred contexts, which still belong to the removed device. `DeviceResources::_deferred_contexts` (DeviceResources.h:128) is never cleared or recreated on device loss.

Every frame after a restore, `Game::clear()` (Game.cpp:136-149) then does `deferred_context->OMSetRenderTargets(1, &renderTarget, nullptr)` with the NEW device's RTV on an OLD device's context, and `GameLevel::draw` (GameStates.cpp:328) / `MenuPage::draw_mobjects_in_viewports` (MenuPage.cpp:161) call `immediate_context->ExecuteCommandList(...)` on the new immediate context with command lists produced by the old device. Cross-device resource use is invalid in D3D11.

In Debug this trips the info-queue break installed at DeviceResources.cpp:238-239 (`SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true)`) — a hard debug break. In Release it is silent corruption: black or garbage frames for the rest of the session. This is reachable from the two normal device-loss paths, `Present` (DeviceResources.cpp:485-494) and `ResizeBuffers` (DeviceResources.cpp:290-304).

**Fix:** Move deferred-context creation into `DeviceResources::CreateDeviceResources()` (so it is redone by `HandleDeviceLost`), release the old ones first, and store them as `Microsoft::WRL::ComPtr<ID3D11DeviceContext>` so the reference is dropped when the device is torn down. Longer term, a `TryEnableDeviceLossHandling`-style test path or at least a manual `DXGIGetDebugInterface`-triggered device removal should be exercised once — this code has clearly never been run.

### 6. OnDeviceRestored reconstructs every subsystem, turning the entire live object graph into dangling pointers

`ArtAttack/Game.cpp:218-261, 282-287` — **critical** · `resource-lifetime`

`Game::OnDeviceRestored()` (Game.cpp:282-287) unconditionally re-runs `create_device_dependent_resources()`, which is not a GPU-resource hook at all — it reassigns `_thread_pool` (:236), `_resource_manager` (:239), `_resource_loader` (:241), `_dt = std::make_unique<float>(0.f)` (:244), `_viewport_manager` (:247), `_states` (:252), `_partitioner` (:255), each assignment destroying the previous object, then republishes the new addresses into `GameData`.

Nothing in the live world re-reads GameData. Raw pointers are snapshotted once at construction: `Level.h:118 ViewportManager*`, `:119 ResourceManager*`, `:140 const float* _dt`, `:141 ID3D11SamplerState* _sampler_state` (obtained from the `CommonStates` that `OnDeviceLost` destroys at Game.cpp:276), `:146 ThreadPool*`, `:147 const Partitioner*`. Eleven headers cache `const float* _dt` (AnimationObject.h, AnimatedSprite.h, Drawer.h, LevelBuilder.h, Level.h, LevelObjectBuilder.h, PaintTile.h, Player.h, Projectile.h, StructurePaintable.h, Weapon.h). A single TDR/driver-update during gameplay therefore frees the float that every one of them reads on the next update, plus the thread pool the render path is mid-flight in.

**Fix:** Split into (a) device-dependent GPU objects recreated on restore (SpriteBatches, CommonStates, textures) and (b) device-independent services created exactly once in `initialize()` and never reassigned (ThreadPool, ResourceManager registry, ViewportManager, Partitioner, frame delta). Services that must be recreated should be repaired in place (`_resource_manager->recreate_gpu_resources(device)`) so their identity is stable, or reached through an accessor rather than a snapshotted raw pointer.

### 7. Device-lost recovery destroys the ResourceManager, ThreadPool and dt that every live object holds by raw pointer

`ArtAttack/Game.cpp:218-287` — **critical** · `correctness`

`Game::create_device_dependent_resources()` is called from two places: `Game::initialize()` (Game.cpp:54) and `Game::OnDeviceRestored()` (Game.cpp:284). It unconditionally re-creates the owning `unique_ptr` members:

```cpp
this->_resource_manager = std::make_unique<ResourceManager>();   // :239
this->_resource_loader  = std::make_unique<ResourceLoader>(...); // :241
this->_dt               = std::make_unique<float>(0.f);          // :244
this->_viewport_manager = std::make_unique<ViewportManager>(...);// :247
this->_thread_pool      = std::make_unique<ThreadPool>(...);     // :236
this->_partitioner      = std::make_unique<Partitioner>();       // :255
this->_resource_loader->load_all_resources();                    // :258
```

Every live game object captured raw pointers to the *old* instances at construction time: `Level::_sound_bank` and `_resource_manager` (Level.cpp:59), `Player::_sound_bank` (Player.cpp:44), `StructurePaintable::_sound_bank` (StructurePaintable.cpp:34), `Weapon::_sound_bank` (Weapon.cpp:33), `DrawObject::_resource_manager` on every drawable, and the `const float* _dt` threaded through ~26 headers. After a device-lost/restore (driver reset, TDR, Alt-Tab on some drivers) every one of those pointers dangles — guaranteed use-after-free on the next frame if a level is in progress.

`OnDeviceLost` (Game.cpp:269-280) also calls `reset_all_textures/sprite_fonts/sounds` on a ResourceManager that is about to be thrown away entirely, so that work is pointless, and `load_all_resources()` re-reads every DDS and JSON from disk from scratch.

**Fix:** Split device-dependent GPU resource creation from the service objects. `ResourceManager`, `ThreadPool`, `Partitioner`, `_dt` and `ViewportManager` should be created once in `initialize()` and never re-created; only the D3D objects (SpriteBatches, CommonStates, textures, SpriteFonts) should be torn down and rebuilt on device loss, with the ResourceManager re-populating its existing map entries in place so the borrowed pointers stay valid.

### 8. OnDeviceLost/OnDeviceRestored replace every shared service, leaving all live game objects with dangling pointers

`ArtAttack/Game.cpp:269-287` — **critical** · `memory-safety`

`Game::OnDeviceRestored` (Game.cpp:282-287) calls `create_device_dependent_resources()`, which does not "recreate device-dependent resources" — it reconstructs the entire service graph: `_thread_pool` (Game.cpp:236), `_resource_manager` (239), `_resource_loader` (241), `_dt = std::make_unique<float>(0.f)` (244), `_viewport_manager` (247), `_states` (252), `_partitioner` (255), each republished into `GameData` (237, 240, 243, 245, 250, 253, 256). `GameData` is only a bag of raw pointers (GameData.h:49-61); every consumer copied those pointers into its own members at construction and is never notified. Confirmed consumers: `Level::_resolution_manager/_viewport_manager/_resource_manager` (Level.h:117-119), `Level::_dt` (Level.h:140), `Level::_thread_pool/_partitioner` (Level.h:146-147), `Player::_dt` (Player.h:170). After one device restore mid-match, `Level::update` dereferences six freed objects.

Worse, the crash arrives before the restore: `Game::OnDeviceLost` (Game.cpp:277-279) calls `_resource_manager->reset_all_sounds()`, which is `sound_bank.second->reset_all_instances(); sound_bank.second.reset();` (ResourceManager.cpp:119-126) — it destroys every `SoundBank`. `Player::_sound_bank` (Player.h:168) and `Level::_sound_bank` (Level.h:107) are raw pointers into those destroyed banks, and the next `play_wave` (SoundBank.cpp:16-21) is a use-after-free. Audio is not a D3D device resource at all — tearing down sound on D3D device loss is gratuitous.

**Fix:** Split `create_device_dependent_resources` into (a) genuinely device-dependent GPU objects (SpriteBatches, CommonStates, textures/fonts) and (b) one-time services (`_dt`, thread pool, partitioner, resource manager, viewport manager) that must survive device loss; only (a) may be re-run from `OnDeviceRestored`. Stop resetting sounds on device loss. Frame time should be a `float` passed as an argument, not a heap `float*` handed out to 26 headers.

### 9. OnDeviceRestored reallocates the shared frame-time float, dangling every const float* _dt in the engine

`ArtAttack/Game.cpp:282-287` — **critical** · `memory-safety`

`Game::OnDeviceRestored()` calls `create_device_dependent_resources()`, which at ArtAttack/Game.cpp:244-245 does:

```
this->_dt = std::make_unique<float>(0.f);
this->_data->set_dt(this->_dt.get());
```

Assigning a new `unique_ptr` frees the old float. Every live object took a copy of the OLD raw pointer at construction time and keeps it forever: `Level::_dt` (ArtAttack/Level.h:140), `Player::_dt` (ArtAttack/Player.h:170), `Weapon::_dt` (ArtAttack/Weapon.h:123), `Projectile::_dt` (ArtAttack/Projectile.h:79), `PaintTile::_dt` (ArtAttack/PaintTile.h:82), `StructurePaintable::_dt` (ArtAttack/StructurePaintable.h:57), `AnimationObject::_dt` (ArtAttack/AnimationObject.h:49), `AnimatedSprite::_dt` (ArtAttack/AnimatedSprite.h:42), `Drawer::_dt` (ArtAttack/Drawer.h:31), `LevelBuilder`/`LevelObjectBuilder` (:32/:31). After a device-lost/restore (driver update, TDR, GPU hot-swap, remote-desktop transition) every `*this->_dt` read — e.g. `Player::get_dt()` ArtAttack/Player.cpp:692-694, `AnimationObject.cpp:88` — is a use-after-free that then multiplies velocities by garbage.

The same function also replaces `_thread_pool`, `_resource_manager`, `_resource_loader`, `_viewport_manager`, `_partitioner` and republishes them into `GameData`, while the live `Level` holds raw pointers to the old ones (ArtAttack/Level.h:117-119, 146-147) — so device restore during gameplay is a use-after-free across the whole object graph, not just for `_dt`.

**Fix:** Split `create_device_dependent_resources()` into genuinely device-dependent GPU objects (SpriteBatch, CommonStates, textures) and device-INdependent services (`_dt`, ThreadPool, Partitioner, ResourceManager instance) so restore only recreates the former. Better still: stop passing frame time as a heap-allocated `float*` at all — pass `float dt` down through `update(float dt)`, which removes 11 dangling-pointer members and the whole aliasing problem in one change.

### 10. AudioEngine is destroyed before the ResourceManager that owns its WaveBanks and SoundEffectInstances

`ArtAttack/Game.h:65-74` — **critical** · `resource-lifetime` · independently reported by 2 agents

Members destruct in reverse declaration order. `_resource_manager` is declared at Game.h:65 and `_audio_engine` at Game.h:74, so on every shutdown `_audio_engine` dies FIRST and `_resource_manager` second. `ResourceManager::_sound_banks` (ResourceManager.h:53) is `std::map<std::string, std::unique_ptr<SoundBank>>`; `SoundBank` owns `std::unique_ptr<DirectX::WaveBank> _wave_bank` (SoundBank.h:39) and `std::map<std::string, std::unique_ptr<DirectX::SoundEffectInstance>>` (SoundBank.h:41). DirectXTK requires the AudioEngine to outlive every WaveBank / SoundEffectInstance — their destructors unregister themselves from the engine. So `g_game.reset()` (Main.cpp:226) runs ~WaveBank and ~SoundEffectInstance against a freed AudioEngine on every normal exit. `Game::~Game()` (Game.cpp:23-29) calling `_audio_engine->Suspend()` does not help: the destructor body runs BEFORE member destruction, not after.

**Fix:** Declare `_audio_engine` first (before `_resource_manager`/`_resource_loader`) so it dies last, and better, stop relying on declaration order: add an explicit `Game::shutdown()` that does `_resource_manager.reset(); _resource_loader.reset();` and only then `_audio_engine.reset()`, with a comment stating the DirectXTK constraint.

### 11. Gamepad input vector is compacted by connection state, then indexed by player number — out-of-bounds read on controller disconnect

`ArtAttack/GameStates.cpp:141-157, 185` — **critical** · `correctness`

`GameLevel::update` does `std::vector<PlayerInputData> player_inputs = this->_player_input->update_and_get_player_inputs();` (:141-142) and passes it unvalidated to `_level->update(player_inputs)` (:185).

`PlayerInput::update_and_get_player_inputs` (PlayerInput.cpp:119-138) only pushes an entry for pads that are currently connected:
```cpp
for (int i = 0; i < 4; i++) {
    if (current[i].connected) { ... result.push_back(input); }
    this->_prev_inputs[i] = current[i];
}
```
But `Level::update_level_logic` (Level.cpp:189-192) indexes it positionally by player object:
```cpp
int player_index = 0;
for (const auto& object : *this->_player_objects) {
    object->set_player_input(player_inputs[player_index]);
```
In a 3-player match, unplugging pad 0 shrinks the vector to 2 while `_player_objects` still holds 3 → `player_inputs[2]` is an unchecked `std::vector::operator[]` past the end. Before it goes out of bounds it also silently remaps every surviving player's input to the wrong Player object.

The same defect flows through `check_for_pause_input` (:348-359), which returns an index into the compacted vector; that index is handed to `PauseMenuData` as the player number (:156) and then used as an unchecked subscript into `MenuInput`'s separately-compacted vector — `inputs[player_num]` at PauseMenu.cpp:53, 54, 59, 82, 98, 222, 229, 256 (MenuInput::update_and_get_menu_inputs compacts identically, MenuInput.cpp:131-150). A pad disconnecting while the pause menu is open is a second out-of-bounds read.

`PlayerInputData` already carries a `connection` field for exactly this, but PlayerInput.cpp:111 hardcodes `result.connection = connection_state::CONNECTED;` and DISCONNECTED is never written.

**Fix:** Return a fixed 4-element array (or a vector always sized to the max player count) indexed by pad slot, with `connection` correctly set for absent pads, so slot index == player identity is an invariant across both input types and across frames. Then `check_for_pause_input` returns a real player number and every `inputs[n]` is safe. Until then use `.at()` in Level::update_level_logic and PauseMenu so the failure is loud.

### 12. End-menu "Restart" replays the match with the wrong viewport layout and no split-screen dividers

`ArtAttack/GameStates.cpp:276-283` — **critical** · `correctness`

`Level::draw_zoom_out_level_component` permanently mutates the shared ViewportManager at the end of every match:

    // Level.cpp:619 (inside a `const` draw helper, run on a thread-pool worker)
    this->_viewport_manager->set_layout(screen_layout::ONE_PLAYER);

Grep confirms `ViewportManager::set_layout` has exactly two call sites: `GameStates.cpp:123` (`GameLevel::init`) and `Level.cpp:619`. The end-menu RESTART branch rebuilds the level but never restores the layout:

    // GameStates.cpp:276-283
    case end_menu_action::RESTART:
    {
        this->_level->stop_music();
        this->_level = std::move(this->_level_builder->build_level(this->_settings));
        this->_state = game_level_state::FIRST_UPDATE;
        break;
    }

`GameLevel::init()` is not re-run (it is only called by `StateContext::transition_to`), so match 2 of a 2/3/4-player game renders every player's camera into one fullscreen viewport stacked on top of each other. Worse, `LevelBuilder::build_level` builds the split-screen dividers *from the current layout* (`LevelBuilder.cpp:55-56` -> `LevelObjectBuilder.cpp:232-233` -> `ViewportManager::get_viewport_dividers`), and `ViewportManager.cpp:129-132` returns an empty vector for ONE_PLAYER — so the rebuilt level also has zero viewport dividers baked in permanently. Both symptoms persist for every subsequent restart. `PlayerBuilder::build_players` is likewise handed the stale `_viewport_manager` (LevelBuilder.cpp:50-53).

**Fix:** Two separate fixes. (a) Stop mutating global render state from a draw path: `draw_zoom_out_level_component` should compute the fullscreen viewport locally (`ViewportManager::get_fullscreen_d3d11_viewport()` already exists) and apply it to the deferred context, instead of calling `set_layout`. Note the method is declared `const` (Level.h:173) yet mutates the manager — that const is a lie. (b) Make rematch go through one code path: extract a `GameLevel::build_and_enter_level()` that calls `set_layout(_settings.get_screen_layout())` and then `build_level`, and call it from `init()`, from `pause_menu_action::RESTART` (GameStates.cpp:200-205) and from `end_menu_action::RESTART`.

### 13. The core collision interface identifies objects with an enum of this game's content

`ArtAttack/ICollisionGameObject.h:5, 14 (with collision_object_type.h:4-88)` — **critical** · `design`

`ICollisionGameObject` — the engine's fundamental collidable abstraction — includes `collision_object_type.h` (line 5) and exposes `virtual collision_object_type get_collision_object_type() const = 0;` (line 14). That enum (collision_object_type.h:4-29) is a flat cross-product of class x team x weapon: `PLAYER_TEAM_A`, `PLAYER_TEAM_B_DEAD`, `PROJECTILE_SPRAY_TEAM_A`, `PROJECTILE_MIST_TEAM_B`, `PAINT_TILE`, `STRUCTURE_RAMP_LEFT`, ... The header then ships 8 free predicates (`is_team_a_projectile`, `is_dead_player`, ...) at lines 31-88 that hardcode the same content.

Consequence: every collidable in any future game must declare itself as one of *this* game's 23 types. Adding a weapon means editing an engine header and recompiling every TU that touches collision. Adding a third team is a combinatorial edit. It also forces the same if/else identity ladder to be rewritten in every object (Player.cpp:186-244, PaintTile.cpp:114-140, StructurePaintable.cpp:64-102, Level.cpp:238, Level.cpp:713-715).

This is the single most pervasive engine/game inversion in the repo: the game's content model *is* the engine's type system.

**Fix:** Replace the enum with data the engine does not interpret: a `CollisionLayer`/`CollisionMask` bitset plus an opaque game-owned tag (e.g. `uint32_t user_type` or a `void*`/`std::any` payload). The engine filters pairs by layer/mask; the *game* decides what `PROJECTILE_SPRAY_TEAM_A` means by casting/inspecting its own tag in its own `on_collision`. Move `collision_object_type.h` out of the engine entirely into game/ as `game_collision_tags.h`.

### 14. player_inputs is compacted to connected pads but indexed by player-object index (OOB read)

`ArtAttack/Level.cpp:188-193` — **critical** · `correctness` · independently reported by 2 agents

`PlayerInput::update_and_get_player_inputs()` (ArtAttack/PlayerInput.cpp:126-137) builds its result with `if (current[i].connected) { ... result.push_back(input); }` — the vector is COMPACTED, so its indices are "nth connected pad", not "player number". `Level::update_level_logic` then does:

```
int player_index = 0;
for (const auto& object : *this->_player_objects)
{
    object->set_player_input(player_inputs[player_index]);
```

`_player_objects` is fixed at level-build time. The moment any pad disconnects, `player_inputs.size() < _player_objects->size()` and `operator[]` reads past the end — and the players that survive are driven by the WRONG controller (unplug pad 0 in a 2-player match and player 0 is now steered by pad 1). If *all* pads go away the vector is empty and `player_inputs[0]` is an out-of-bounds read on an empty vector on the very first iteration.

That last case is not hypothetical and is reachable without touching any hardware: minimising the window sends WM_SIZE/SIZE_MINIMIZED (ArtAttack/Main.cpp:281-291) → `Game::on_suspending()` (ArtAttack/Game.cpp:171-176) → `_gamepad->Suspend()`, while the main loop keeps calling `tick()` (ArtAttack/Main.cpp:213-224) → `GameLevel::update()` ACTIVE branch → `_player_input->update_and_get_player_inputs()` on a suspended GamePad, which reports every pad disconnected. So minimising during a match walks straight into the empty-vector index.

The same index-confusion exists in the pause menu: `GameLevel::check_for_pause_input` (ArtAttack/GameStates.cpp:348-358) returns `i`, an index into the compacted `player_inputs`, which is stored as `player_num` and later used as a direct subscript into a *different* compacted vector — `inputs[player_num]` at ArtAttack/PauseMenu.cpp:48-53 and 217-222. Note that every other menu iterates `for (i = 0; i < inputs.size(); i++)` correctly (ArtAttack/MainMenu.cpp:49-51, ArtAttack/EndMenu.cpp:32, ArtAttack/ResultsMenu.cpp:266) — only the gameplay and pause paths get it wrong.

**Fix:** Stop compacting. Return a fixed `std::array<PlayerInputData, 4>` (or a vector sized to the player count) indexed by player slot, and use the already-present but currently dead `PlayerInputData::connection` field (ArtAttack/PlayerInputData.h:19; `calculate_player_input` unconditionally writes `connection_state::CONNECTED` at ArtAttack/PlayerInput.cpp:111, so DISCONNECTED is never produced) to mark absent pads. Then `Level::update_level_logic` looks up by `player->get_player_num()` and treats a disconnected pad as neutral input / auto-pause instead of indexing off the end.

### 15. Level indexes the connection-compacted input vector by player ordinal - OOB read on any pad dropout

`ArtAttack/Level.cpp:189-192` — **critical** · `memory-safety`

`Level::update_level_logic` walks `*this->_player_objects` with a running counter and uses it as a subscript into the input vector:

```cpp
int player_index = 0;
for (const auto& object : *this->_player_objects)
{
    object->set_player_input(player_inputs[player_index]);   // Level.cpp:192
    ...
    player_index++;
}
```

`_player_objects->size()` is fixed at level build time and equals the menu's player count (`PlayerBuilder::build_players`, PlayerBuilder.cpp:27, iterates `settings.get_player_settings()`, whose size is forced to `_player_count` by `MenuLevelSettings::set_player_count`, MenuLevelSettings.cpp:9-25).

`player_inputs.size()` is NOT the player count. `PlayerInput::update_and_get_player_inputs` (PlayerInput.cpp:126-137) only `push_back`s entries for slots that reported `IsConnected()`, so its size is the number of *currently connected* pads.

Mid-match trace (2 players, pads in XInput slots 0 and 1, player 2's pad unplugged):
1. PlayerInput.cpp:124 `get_raw_input(1)` -> `pad.IsConnected()` false -> `current[1].connected` stays false (PlayerInput.h:25 default).
2. PlayerInput.cpp:129 skips the push_back, so `result.size() == 1`.
3. GameStates.cpp:141-142 hands that 1-element vector to `Level::update`, GameStates.cpp:185.
4. Level.cpp:192 with `player_index == 1` reads `player_inputs[1]` on a size-1 vector - out of bounds. `std::vector::operator[]` is unchecked in Release; in Debug MSVC's `_ITERATOR_DEBUG_LEVEL` assert fires and the process aborts.

Observable effect in Release: player 2 is fed ~48 bytes of whatever follows the vector buffer, reinterpreted as `PlayerInputData`. `x_movement` becomes a garbage float that is multiplied straight into velocity (`X_INITIAL_VELOCITY * x_input`, Player.cpp:769; `X_ACCELERATION * dt * x_input`, Player.cpp:774) - so the abandoned player teleports or accelerates unboundedly, which then trips `throw std::exception("Player out of bounds")` at Level.cpp:312. Nothing catches it; the app terminates.

The *all* pads case is worse and does not need an unplug: if the vector is empty, `player_inputs[0]` is dereferencing a null/garbage `_Myfirst` pointer.

Note `Level.h:143` declares `std::vector<PlayerInputData> _player_inputs` which is never read or written anywhere - a vestige of the design that would have avoided this.

**Fix:** Stop compacting. Have `PlayerInput::update_and_get_player_inputs()` return a fixed 4-element (`GamePad::MAX_PLAYER_COUNT`) array/vector where disconnected slots carry `connection_state::DISCONNECTED` and zeroed axes - the `connection` field in `PlayerInputData` already exists for exactly this and is currently dead. Then store the owning XInput slot on `Player` (it already has `_player_num`, Player.cpp:1077) and look up `player_inputs[player->get_pad_slot()]`, treating DISCONNECTED as "no input this frame" rather than as a shorter vector. As a stopgap, guard the loop with `if (player_index >= player_inputs.size()) { object->set_player_input(PlayerInputData()); }`.

### 16. Every render worker draws every object, and draw() mutates the object — guaranteed data race

`ArtAttack/Level.cpp:491-592` — **critical** · `concurrency` · independently reported by 2 agents

`Level::draw_active_level` (Level.cpp:375-400) partitions *players* across thread-pool tasks:

```
auto partitioned_players = this->_partitioner->partition(this->_player_objects->size(), num_threads);
for (int i = 0; i < partitioned_players.size(); i++)
    this->_thread_pool->add_task([this, i, partitioned_players, ...]{ this->draw_player_view_level(...); });
```

But inside `draw_player_view_level` each task iterates the *entire* world, not its slice (Level.cpp:516-540):

```
for (auto& object : *this->_non_collision_objects) { ... object->draw(sprite_batches->at(i), camera); }
for (auto& object : *this->_collision_objects)     { ... object->draw(sprite_batches->at(i), camera); }
for (auto& object : *this->_player_objects)        { ... object->draw(sprite_batches->at(i), camera); }
```

So in a 2-4 player match, 2-4 threads call `draw()` on the *same* object concurrently. `IGameObject::draw` is non-const (IGameObject.h:11-12) and the implementations write member state:

- `Player::draw` (Player.cpp:54-86): `this->_animation_state = new_animation_state;`, `TextureObject::set_sprite_sheet_name(info.sprite_sheet);`, `TextureObject::set_element_name(info.uniform_texture);`, `set_animation_strip_and_reset(...)` (which resets `_frame_index`/`_time_elapsed`, AnimationObject.cpp:108-112), `set_frame_time(...)`, `TextureObject::set_effects(effects)`, `AnimationObject::set_effects(effects)`.
- `Weapon::draw` (Weapon.cpp:61-64, 71-76): `set_element_name(...)`, `set_origin(...)`, `set_effects(...)`, `set_draw_rotation(...)`.
- `PaintTile::draw` (PaintTile.cpp:34-53): `this->set_colour(...)`, `this->_splash.set_colour(...)`, `this->_splash.set_rectangle_center(...)` — and `StructurePaintable::draw` (StructurePaintable.cpp:45-52) loops every tile, from every thread.

`set_element_name`/`set_sprite_sheet_name` assign `std::string` members (SpriteSheetObject.h:25-26). Concurrent `std::string::operator=` on the same object is not merely a torn value — it is concurrent free/allocate on the same control block, i.e. heap corruption. This is the single most serious defect found in the sweep and it fires on every multiplayer frame. Note the codebase contains **zero** uses of `std::mutex`, `std::atomic`, or any synchronisation primitive (grepped across all first-party sources).

**Fix:** Split the render pass into a lock-free shape: (1) make `draw` a `const` operation on immutable per-frame state — move all the animation/effect/name mutation out of `draw` and into `update`, which runs single-threaded; (2) have each task record only its own slice, i.e. per-viewport visibility should not require re-walking global containers from N threads mutating shared objects. Longer term, produce an immutable per-frame render packet list during update and let workers consume it read-only.

### 17. draw_zoom_out_level_component runs on a worker thread but mutates ViewportManager and touches the IMMEDIATE device context

`ArtAttack/Level.cpp:595-651` — **critical** · `concurrency` · independently reported by 3 agents

`draw_zoom_out_level` pushes the whole zoom-out draw onto the pool:

```cpp
void Level::draw_zoom_out_level(...) const
{
    int num_threads = this->_thread_pool->get_max_num_threads();   // :599  never used (C4189 under /W4)
    this->_thread_pool->add_task([this, deferred_contexts, command_lists, sprite_batches]()
        { this->draw_zoom_out_level_component(deferred_contexts, command_lists, sprite_batches); });  // :601
    this->_thread_pool->wait_for_tasks_to_complete();               // :606
}
```

The callback body then does, **on a background thread**:

```cpp
this->_viewport_manager->set_layout(screen_layout::ONE_PLAYER);  // :619
this->_viewport_manager->apply_player_viewport(0);               // :620
```

Two separate defects:

1. `ViewportManager::set_layout` (ViewportManager.cpp:79-82) is a **non-const mutator** writing the shared `_layout` member that the main thread reads from `get_all_viewports`, `get_player_viewport`, `get_viewport_dividers`. It is reached from a `const` member function through the non-const raw pointer `ViewportManager* _viewport_manager` (Level.h:118) — the `const` on `draw_zoom_out_level` is decoration only. It is also a permanent side effect hidden inside a draw call: the layout is clobbered to ONE_PLAYER and never restored.

2. `apply_player_viewport(int)` — the one-argument overload (ViewportManager.cpp:63-70) — does:
```cpp
auto context = this->_device_resources->GetD3DDeviceContext();  // the IMMEDIATE context
context->RSSetViewports(1, &vp);
this->_sprite_batch->SetViewport(vp);                            // sprite batch #0, owned by deferred ctx 0
```
So a worker thread issues a command on the immediate context and reconfigures SpriteBatch 0 while the main thread is inside `Game::render`/`Present`. `ID3D11DeviceContext` is explicitly not thread-safe; the whole point of the 16 deferred contexts (Game.cpp:52, DeviceResources.cpp:716-730) is that workers must never touch the immediate one. The three-argument overload that takes an explicit context exists (ViewportManager.cpp:53-61) and is what the rest of the worker code correctly uses (Level.cpp:506, MenuPage.cpp:107).

Submitting exactly one task and then immediately blocking on it also means this path pays a full thread-pool round trip to gain zero parallelism.

**Fix:** Do not run this on the pool at all — call `draw_zoom_out_level_component` directly on the calling thread and delete the unused `num_threads`. Hoist `set_layout` out of the draw path into `Level::update` (draw must not change simulation/presentation state). Replace `apply_player_viewport(0)` with the three-arg overload `apply_player_viewport(0, deferred_contexts->at(0), sprite_batches->at(0))` so the immediate context is never touched off the main thread, and delete the one-argument overload so it cannot be reached again.

### 18. `Level` is both the only scene abstraction and the complete paint-battle ruleset

`ArtAttack/Level.h:22-52, 54-80, 128-137 (impl Level.cpp:706-735)` — **critical** · `design`

There is no engine-level `Scene`/`World` type; `Level` is it, and it is entirely this game. Its constructor (Level.h:58-80) takes `const TeamColour&`, `level_stage stage`, `const std::vector<Vector2F>& team_a_spawns`, `const std::vector<Vector2F>& team_b_spawns`. Its members include `TeamColour _team_colours` (:128), `level_stage _stage = level_stage::KING_OF_THE_HILL` (:129), `_team_a_spawns`/`_team_b_spawns` (:136-137). `level_consts` (:31-52) hardcodes a 240-second match timer, a 3-second countdown, the countdown font `gill_sans_mt_bold_144`, and the sound cue names `"smash_countdown"`/`"slide_whistle_up"`.

`level_state` (:22-29) — `START_COUNTDOWN/ACTIVE/ZOOM_OUT/OVERVIEW/FINISHED` — is this game's match flow, not a generic scene lifecycle. And `Level::get_level_end_info()` (Level.cpp:706-735) is the win condition: it walks `_collision_objects`, filters `type == STRUCTURE_PAINTABLE`, `dynamic_cast<IPaintableGameObject*>`, and sums `paint.team_a`/`paint.team_b`.

So the generic responsibilities that *should* be engine (own the object lists, step them, run broad/narrow phase, dispatch collisions, cull, draw through viewports, delete dead objects) are welded to the game responsibilities (two teams, spawns, paint scoring, match timer, zoom-out celebration).

**Fix:** Extract an engine `Scene` owning `std::vector<std::unique_ptr<IGameObject>>` + the collision/update/draw pipeline, with virtual (or callback) hooks `on_pre_update/on_post_collision/is_finished/get_result`. Then `PaintBattleScene : Scene` in game/ holds `TeamColour`, spawns, the match timer and `get_level_end_info()`. The engine must not know the word 'team'.

### 19. Level JSON parsed through a 1-byte heap buffer declared as 8 bytes — heap overflow on every level load

`ArtAttack/LevelLoadedInfo.cpp:96-110` — **critical** · `memory-safety` · independently reported by 2 agents

```cpp
FILE* fp = fopen(json_path, "rb");                            // unchecked
std::unique_ptr<char> readBuffer = std::make_unique<char>();  // ONE char
FileReadStream is(fp, readBuffer.get(), sizeof(readBuffer));  // sizeof(unique_ptr) == 8
Document d;
d.ParseStream(is);                                            // return value discarded
fclose(fp);
readBuffer.release();                                          // leaks the allocation
```
Four independent defects. (1) `make_unique<char>()` allocates one byte; `sizeof(readBuffer)` is the size of the `unique_ptr` object, 8 on x64. I read the vendored rapidjson (external/rapidjson/include/rapidjson/filereadstream.h:66-81): `Read()` does `readCount_ = std::fread(buffer_, 1, bufferSize_, fp_)` and, on the final short read, `buffer_[readCount_] = '\0'` — so it writes up to 8 bytes, plus a terminator at offset up to 8, into a 1-byte allocation. king_of_the_hill.json is 21,955 bytes, so the refill loop runs ~2,700 times per load, each time overwriting the allocator's block header. (2) `readBuffer.release()` relinquishes ownership without deleting. (3) `fopen` is unchecked; `RAPIDJSON_ASSERT(fp_ != 0)` compiles out under NDEBUG and `fclose(nullptr)` is UB. (4) `d.HasParseError()` is never queried, so a malformed file yields a null Document and every later `_json_doc["level_name"]` (line 17) etc. is UB in Release.

Candidates 3, 19, 58 and 72 all report this; merged. The same `sizeof(unique_ptr)` idiom also exists at SpriteSheet.cpp:74-81 (I confirmed `read_buffer.release()` there), and reviewers cite Save.cpp and SoundBank.cpp — treat it as one shared helper to fix.

**Fix:** Write one `rapidjson::Document parse_json_file(const std::string& path)` helper that checks `fopen`, uses a real `std::vector<char> buffer(65536)`, closes the FILE* with an RAII guard, and throws a message naming the path and `GetParseError()`/`GetErrorOffset()` on failure. Replace all four copy-pasted loaders with it.

### 20. StructurePaintable stores a reference to a builder local; it dangles for the object's entire lifetime

`ArtAttack/LevelObjectBuilder.cpp:107-142` — **critical** · `memory-safety`

```cpp
std::vector<Segment> paintable_edges;                 // :107  function-local
if (json.HasMember("paintable_faces")) { ... paintable_edges.push_back(...); }
return std::make_unique<StructurePaintable>(..., paintable_edges, ...);  // :140
```

`StructurePaintable` binds that argument to a **reference member**: `const std::vector<MattMath::Segment>& _paintable_edges;` (StructurePaintable.h:56). `build_collision_object` returns immediately afterwards, destroying the vector, so the member dangles for the whole life of the level. `StructurePaintable` reads it during paint-tile generation (StructurePaintable.cpp:189-193). 24 objects across the three shipping levels are `StructurePaintable`.

Secondary: that reference member also implicitly deletes the `StructurePaintable() = default;` declared on line 28 of the same header.

**Fix:** Change the member to `std::vector<MattMath::Segment> _paintable_edges;` (by value) and move the argument in. If the edges are only needed during construction (they appear to be — tiles are generated in the ctor), take the parameter and drop the member entirely.

### 21. RectangleRotated(Segment, thickness) constructor always throws; no test constructs it

`ArtAttack/MattMath.cpp:4201-4215` — **critical** · `test-coverage` · independently reported by 2 agents

The constructor does `this->_x_axis = center_line.get_direction();` then `this->_y_axis = Vector2F::normal(this->_x_axis);` and then `if (!this->is_valid()) throw std::invalid_argument(...)`. But `Segment::get_direction()` (ArtAttack/MattMath.cpp:4063-4066) returns `point_1 - point_0` — un-normalised — while `axes_valid()` (4522-4537) requires `are_equal(_x_axis.length(), 1.0f, EPSILON)`. So this constructor throws for every segment whose length is not within 1e-4 of 1.0. Note the sibling 4-arg constructor at 4186 does call `_x_axis.normalize()`, so the two constructors disagree. Every RectangleRotated test in MattMathTests/MathTests.cpp (lines 356-403, 491-515, 629-662, 736-765, 783-830, 831-873) uses only the 4-arg constructor; the Segment overload has zero tests. This is the overload intended for paint trails / thick line segments, i.e. the reason OBBs were added.

**Fix:** Normalise in the Segment ctor (`_x_axis = Vector2F::unit_vector(center_line.get_direction())`) and add tests: construct from a segment of length 10 with thickness 2, assert `get_half_extents()`, `get_angle()`, the four corner points, and that a zero-length segment throws (`Assert::ExpectException<std::invalid_argument>`).

### 22. PauseMenu subscripts the menu-input vector with an unvalidated player index - guaranteed OOB if any pad drops

`ArtAttack/PauseMenu.cpp:46-113` — **critical** · `memory-safety` · independently reported by 3 agents

`PauseMenuInitial::update` does:

```cpp
std::vector<ProcessedMenuInput> inputs = this->get_menu_inputs();
int player_num = this->get_pause_menu_data()->get_player_num();
...
if (inputs[player_num].action == menu_input_action::BACK ||   // PauseMenu.cpp:53
    inputs[player_num].action == menu_input_action::PAUSE)
```

There is no size check anywhere in the function - `inputs[player_num]` is subscripted at lines 53, 54, 59, 82 and 98, and again in `PauseMenuConfirmation::update` at lines 222, 229, 256 and 257.

`player_num` is not a stable pad identity: it is the *compacted index* returned by `GameLevel::check_for_pause_input` (GameStates.cpp:348-358, `return i;` where `i` indexes the already-compacted `player_inputs` vector), stored into `PauseMenuData` at GameStates.cpp:156.

This is the only unguarded menu-input subscript in the codebase - every other page loops `for (i < inputs.size())` (MainMenu.cpp:50, EndMenu.cpp:32, ResultsMenu.cpp:266) or bounds itself with `i < num_inputs && i < player_count` (MainMenu.cpp:1160, 1443, 1790).

Three concrete ways to fire it:
- Player 3 of 3 pauses (`player_num == 2`), then any one pad is unplugged: `MenuInput::update_and_get_menu_inputs` (MenuInput.cpp:139-148) returns 2 entries, `inputs[2]` is OOB.
- Every pad is unplugged (or all batteries die) while paused: `inputs` is empty and `inputs[0]` dereferences the empty vector's null data pointer.
- Alt-tab: `Game::on_deactivated` (Game.cpp:165-169) calls `GamePad::Suspend()` while the main loop (Main.cpp:213-224) keeps calling `tick()` unconditionally, so the pause menu keeps polling a suspended gamepad.

Secondary consequence even without the OOB: the pause menu is only exitable by the pad at that compacted index, so if that specific pad dies the match cannot be resumed, restarted or quit at all.

**Fix:** Guard every access: `if (player_num < 0 || static_cast<size_t>(player_num) >= inputs.size()) return;` at minimum. Properly, carry the XInput slot (not a compacted index) in `PauseMenuData`, index a fixed-size input array by that slot, and add an explicit "controller disconnected - reconnect or press START on any pad" state so the menu never becomes unresponsive.

### 23. Player::draw mutates object state and runs concurrently on the same Player from several thread-pool threads

`ArtAttack/Player.cpp:47-90` — **critical** · `concurrency`

Merges 3 candidates (all reported this). Verified end to end. `Player::draw(SpriteBatch*, const Camera&)` is non-const and writes: `this->_animation_state = new_animation_state;` (:59), `TextureObject::set_sprite_sheet_name(info.sprite_sheet)` (:62), `TextureObject::set_element_name(...)` (:63), `set_animation_strip_and_reset(...)` (:65, which also resets `_frame_index`/`_time_elapsed`), `set_frame_time(...)`/`set_frame_time_to_default()` (:66-73), and unconditionally `TextureObject::set_effects(effects)` (:83) and `AnimationObject::set_effects(effects)` (:86).

The concurrency is real, and it is worse than the candidates described — `Level::draw_active_level` (Level.cpp:382-397) partitions the PLAYER list, not the viewport list: `_partitioner->partition(this->_player_objects->size(), num_threads)` with `NUM_THREADS_MAX = 16` (Game.h:14). `Partitioner::partition` (Partitioner.cpp:10) caps the loop at `i < num_elements`, so an N-player match produces exactly N partitions and N concurrent `ThreadPool::add_task` submissions. `ThreadPool::add_task` uses the real Win32 pool (`SubmitThreadpoolWork`, ThreadPool.cpp:47). Each task then runs `draw_player_view_level`, which loops over EVERY player (Level.cpp:534-540: `for (auto& object : *this->_player_objects) { ... object->draw(sprite_batches->at(i), camera); }`).

So in any 2+ player match, `Player::draw` for the same instance executes on N threads simultaneously. `SpriteSheetObject::_sheet_name` and `_element_name` are `std::string` (SpriteSheetObject.h:25-26); concurrent unsynchronised assignment to a `std::string` is a torn write on the heap pointer / SSO buffer, i.e. potential heap corruption, not merely a stale frame.

Separately, even single-threaded this is a design defect: clip selection belongs in `update()` (called once per frame), not `draw()` (called once per viewport). A player who is off-screen in every viewport never changes animation clip at all, because `Level` guards the call with `is_visible_in_viewport`.

**Fix:** Move the whole `calculate_animation_state()` / strip-swap / effects block into `Player::update()`, and make both `draw` overrides `const` so the compiler enforces purity. Long term `IGameObject::draw` should be const across the engine — a parallel command-list recorder is only safe if the record path is read-only.

### 24. get_delete_timer() returns _timer, so the lifetime check is `_timer > _timer` and no projectile ever expires

`ArtAttack/Projectile.cpp:278-288` — **critical** · `correctness` · independently reported by 2 agents

Verified verbatim. `float Projectile::get_delete_timer() const { return this->_timer; }` (Projectile.cpp:285-288) returns the elapsed timer instead of `_details.delete_timer`. The sole caller is `if (this->get_timer() > this->get_delete_timer())` at Projectile.cpp:278, and `get_timer()` (line 220-223) also returns `_timer` — so the test is a strict self-comparison and is never true. Grep confirms `ProjectileDetails::delete_timer` (projectile_details.h:12) is written at projectile_consts.h:19,32,53,66,79,100 and read nowhere.

Consequences I traced end to end:
- The only removal path left is `Projectile::on_collision` (Projectile.cpp:120-154), which sets deletion on a structure or enemy-player hit.
- `DETAILS_JET` and `DETAILS_MIST` have `gravity = 0.0f, wind_resistance = 0.0f` (projectile_consts.h:50-51, 76-77), so a missed jet/mist shot flies dead straight forever.
- `Level::update_level_logic` responds to anything leaving the level with `throw std::exception("Collision object out of bounds")` (Level.cpp:315-321). I grepped every `catch` in ArtAttack/*.cpp: the only handlers are in Level.cpp:727 (bad_cast), ResourceLoader, ResourceManager and SoundBank. There is no handler on the update path, so a missed shot terminates the process.
- Survivors accumulate in `_collision_objects`, which is walked object-vs-object in a full O(n^2) double loop every frame (Level.cpp:259-293).

Secondary defect confirmed in the same block: the expiry test at line 278 runs before `alter_timer(dt)` at line 282, so even once fixed a projectile survives one frame past its deadline.

Merged from 5 duplicate candidate reports (all naming Projectile.cpp:278-288).

**Fix:** `return this->get_details().delete_timer;`, and move the test after `alter_timer(dt)`. Rename the pair to `get_age()` / `get_lifetime()` (or collapse both into `bool has_expired() const`) so the self-comparison becomes unwritable. Separately, `Level::is_object_out_of_bounds` must mark transient objects for deletion, not throw — an escaped projectile is not an exceptional condition, and today it is a guaranteed process kill.

### 25. Save-file parse writes an 8-byte read into a 1-byte heap allocation and never checks anything

`ArtAttack/Save.cpp:12-32` — **critical** · `memory-safety`

NOTE: outside the platform file list, but this is the first thing `wWinMain` does (Main.cpp:99) so it is a platform-startup defect.

```
FILE* fp = fopen(json_path, "rb");             // NULL never checked
auto read_buffer = std::make_unique<char>();    // ONE byte
FileReadStream is(fp, read_buffer.get(), sizeof(read_buffer)); // sizeof(unique_ptr)==8, not the buffer size
Document d; d.ParseStream(is);                  // HasParseError() never checked
fclose(fp);
read_buffer.release();                          // leaks the allocation
auto resolution = Vector2I(d["resolution"]["x"].GetInt(), ...); // unchecked member access
```
rapidjson is told the buffer is 8 bytes when it is 1 — a guaranteed 7-byte heap overflow on every launch. `read_buffer.release()` then leaks it. A truncated or hand-edited `save_data.json` makes `d["resolution"]` assert/UB. `check_if_save_file_exists()` (Save.cpp:33-42) makes the NULL `fp` unlikely but not impossible (file deleted between the check and the open, or unreadable).

**Fix:** `auto read_buffer = std::make_unique<char[]>(N);` and pass `N`. Check `fp != nullptr` and `d.HasParseError()`, guard every member access with `HasMember`/`IsObject`, drop the `release()`, and fall back to default `SaveData` on any failure instead of propagating.

### 26. Every JSON file is parsed through a 1-byte heap buffer that rapidjson is told is 8 bytes

`ArtAttack/Save.cpp:14-23` — **critical** · `memory-safety` · independently reported by 2 agents

All four JSON loaders in the codebase share this exact block:

```cpp
FILE* fp = fopen(json_path, "rb");
auto read_buffer = std::make_unique<char>();          // allocates ONE char
FileReadStream is(fp, read_buffer.get(), sizeof(read_buffer));  // sizeof(unique_ptr) == 8 (x64)
```

`std::make_unique<char>()` allocates a single byte. `sizeof(read_buffer)` is `sizeof(std::unique_ptr<char>)` — 8 on x64, 4 on Win32 — not the buffer size. rapidjson's `FileReadStream::Read()` (external/rapidjson/include/rapidjson/filereadstream.h:66-80) then does `std::fread(buffer_, 1, bufferSize_, fp_)` followed by `buffer_[readCount_] = '\0'`, i.e. it writes up to 9 bytes into a 1-byte allocation, repeatedly, for the entire length of the file. This is a heap buffer overflow on every asset load in the program. It only appears to work because malloc's minimum bucket happens to absorb it.

Secondarily, `read_buffer.release()` (Save.cpp:23) relinquishes ownership *without* deleting — a leak on every call; the author almost certainly meant nothing at all here.

Identical sites: Save.cpp:14-23, LevelLoadedInfo.cpp:98-107, SoundBank.cpp:152-161, SpriteSheet.cpp:72-81. All four must be fixed; there is no shared helper.

**Fix:** Replace all four copies with one shared helper, e.g. `rapidjson::Document load_json_document(const std::string& path)`, that reads the whole file into a `std::vector<char>` / `std::string` and uses `rapidjson::Document::Parse(text.c_str())` (or `FileReadStream` with a real `std::array<char, 65536>`). Delete the `release()` calls.

### 27. fopen result never checked in any of the four JSON loaders; NULL FILE* reaches fread in Release builds

`ArtAttack/Save.cpp:14-26` — **critical** · `robustness`

`FILE* fp = fopen(json_path, "rb");` is used with no null check at Save.cpp:14, LevelLoadedInfo.cpp:98, SoundBank.cpp:152 and SpriteSheet.cpp:72. The only thing standing between a missing file and `std::fread(buf, 1, 8, nullptr)` is `RAPIDJSON_ASSERT(fp_ != 0)` in FileReadStream's constructor — and `RAPIDJSON_ASSERT` is `#define RAPIDJSON_ASSERT(x) assert(x)` (external/rapidjson/include/rapidjson/rapidjson.h:437), which is compiled out under NDEBUG. Release builds therefore call `fread` and then `fclose` on a null FILE*, invoking the MSVC invalid-parameter handler and aborting with no message.

The most likely trigger is not exotic: `Save::load_save_file()` (Save.cpp:43-56) writes a default save when the file is missing, but `write_save_file` (Save.cpp:60) will itself fail if the `save/` *directory* doesn't exist (fopen "wb" does not create directories). It returns false, prints "Save was unsuccessful" to a console that does not exist (see separate finding), and `load_save_file` then unconditionally proceeds to `load_from_json` on the still-missing file. A deployed build without a `save/` folder hard-crashes before the window is created (Main.cpp:99 calls `g_save->load_save_file()` with no try/catch).

**Fix:** Check `fp` and throw a typed exception carrying the path, or return an empty/defaulted result. Wrap the whole startup asset load in Main.cpp in a try/catch that shows a `MessageBoxW` naming the missing file instead of aborting silently.

### 28. rapidjson read buffer is a 1-byte allocation sized with sizeof(unique_ptr) — heap overrun on every JSON load, then leaked

`ArtAttack/SoundBank.cpp:152-161` — **critical** · `memory-safety`

Merged from 4 near-identical candidates. Verbatim at SoundBank.cpp:152-161:

    FILE* fp = fopen(json_path, "rb");
    auto read_buffer = std::make_unique<char>();
    FileReadStream is(fp, read_buffer.get(), sizeof(read_buffer));
    ...
    fclose(fp);
    read_buffer.release();

Three compounded defects, all verified:
1. `std::make_unique<char>()` allocates exactly ONE char. `std::make_unique<char[]>(N)` was intended.
2. `sizeof(read_buffer)` is `sizeof(std::unique_ptr<char>)` == 8 on x64 (4 on Win32), not the buffer size. I read external/rapidjson/include/rapidjson/filereadstream.h: the ctor asserts `bufferSize >= 4` (line 46, passes), then `Read()` does `readCount_ = std::fread(buffer_, 1, bufferSize_, fp_)` (line 71) — 8 bytes into a 1-byte heap block, repeatedly for the whole file — and on the final short read also writes `buffer_[readCount_] = '\0'` (line 76), index up to 7. Heap corruption on every asset load.
3. `read_buffer.release()` relinquishes ownership WITHOUT deleting, so the block leaks. The author likely meant `reset()`, or nothing at all.

All four sites verified line by line: ArtAttack/SoundBank.cpp:152-161, ArtAttack/Save.cpp:14-23, ArtAttack/SpriteSheet.cpp:72-81, ArtAttack/LevelLoadedInfo.cpp:96-110 (the last uses the name `readBuffer`). Secondary effect: the file is consumed 8 bytes at a time, so a 22 KB level JSON costs ~2,700 fread calls.

**Fix:** Delete all four copies and write ONE helper — `rapidjson::Document parse_json_file(const char* path)` — that opens with a checked/RAII FILE*, uses `char buf[65536]` (or `std::vector<char>`) with its real size, and returns the document. Drop every `.release()`.

### 29. load_from_json freads into a 1-byte allocation using sizeof(unique_ptr) as the buffer size, leaks it, and never checks fopen

`ArtAttack/SpriteSheet.cpp:70-90` — **critical** · `memory-safety`

Merged from four independent reports; all four describe the same six lines and all four are correct.

```cpp
FILE* fp = fopen(json_path, "rb");            // 72: result never checked
auto read_buffer = std::make_unique<char>();  // 74: allocates exactly ONE char
FileReadStream is(fp, read_buffer.get(), sizeof(read_buffer)); // 75: sizeof(unique_ptr<char>) == 8
Document d; d.ParseStream(is);
fclose(fp);
read_buffer.release();                        // 81: drops ownership without deleting
```
I read the vendored stream to confirm the consequence. external/rapidjson/include/rapidjson/filereadstream.h:66-81 `Read()` does `readCount_ = std::fread(buffer_, 1, bufferSize_, fp_);` and then, when `readCount_ < bufferSize_`, `buffer_[readCount_] = '\0';`. With bufferSize_==8 and a 1-byte allocation that is a 7-byte heap overflow on every refill, repeated for the whole file. It also forces the parser to refill 8 bytes at a time.

`release()` abandons the allocation (author meant `reset()`, which was itself unnecessary). `fopen` is unchecked and rapidjson's `RAPIDJSON_ASSERT(fp_ != 0)` compiles out under NDEBUG, so a missing file gives `fread(nullptr)` then `fclose(nullptr)`.

The identical five lines exist verbatim in ArtAttack/SoundBank.cpp:150-161 (I read it; same `make_unique<char>()` + `sizeof(read_buffer)` + `release()`), so this is two sites, not one. Live at startup via ResourceLoader.

**Fix:** Use a real buffer and its real size, check the open, RAII the handle:
```cpp
std::vector<char> read_buffer(64 * 1024);
FILE* fp = nullptr;
if (fopen_s(&fp, json_path, "rb") != 0 || !fp)
    throw std::runtime_error(std::string("cannot open ") + json_path);
std::unique_ptr<FILE, decltype(&fclose)> guard(fp, &fclose);
rapidjson::FileReadStream is(fp, read_buffer.data(), read_buffer.size());
rapidjson::Document d;
if (d.ParseStream(is).HasParseError()) throw std::runtime_error(...);
```
Fix SoundBank.cpp:150-161 the same way. For a 14KB file, `Document::Parse` on a slurped string is simpler than streaming at all. Grep the tree for `sizeof(` applied to any smart pointer.

### 30. SpriteSheet::draw calls std::map::operator[] on a shared sheet from up to 16 worker threads

`ArtAttack/SpriteSheet.cpp:92-132` — **critical** · `concurrency`

Two of the four `SpriteSheet::draw` overloads are non-const and index the frame map with `operator[]`:

```cpp
void SpriteSheet::draw(SpriteBatch* sprite_batch, const std::string& frame_name, ...)  // :92, no trailing const
{
    sprite_batch->Draw(this->_texture, position.get_xm_vector(),
        this->_sprite_frames[frame_name].get_source_rectangle(),   // :105
        ...);
}
// identical at :114-132, line :126
```

`_sprite_frames` is `std::map<std::string, SpriteFrame>` (SpriteSheet.h:61) and `std::map::operator[]` **default-constructs and inserts** when the key is absent — a tree mutation. `SpriteSheet` objects are owned once by `ResourceManager` and handed out as raw pointers to every `SpriteSheetObject`, so a single sheet is shared by every player, projectile and tile.

Those objects' `draw()` calls all funnel into this from inside the thread-pool tasks submitted at Level.cpp:388 and MenuPage.cpp:142. Concurrent `operator[]` on one `std::map` from N threads is undefined behaviour even when every key already exists (it is a non-const member function on shared state), and a genuine red-black-tree corruption the first time any lookup misses — e.g. a typo'd frame name in a JSON sheet, which today silently inserts an empty `SpriteFrame` instead of failing.

Note the other two overloads (:134, :156) are correctly `const` and take a pre-resolved `RECT*`, which proves the const version is achievable.

**Fix:** Make both string-keyed overloads `const` and replace `_sprite_frames[frame_name]` with `auto it = _sprite_frames.find(frame_name); if (it == _sprite_frames.end()) throw/log;`. That removes the mutation, makes the missing-frame case loud instead of silent, and is a prerequisite for any multithreaded draw. Longer term, resolve frame names to indices/handles once at load time so the render path does no string lookups at all.

### 31. Resource lookup mutates shared std::maps and is called per-draw from thread-pool worker threads

`ArtAttack/SpriteSheetObject.cpp:31-35` — **critical** · `correctness`

`Level::draw_active_level` (Level.cpp:386-399) dispatches `draw_player_view_level` onto `ThreadPool` workers, one per player partition, each recording into its own deferred context. Those tasks call `object->draw(...)` on shared game objects, which reaches:

```cpp
SpriteSheet* SpriteSheetObject::get_sprite_sheet() const {
    return this->get_resource_manager()->get_sprite_sheet(this->get_sprite_sheet_name());
}
```

`ResourceManager::get_sprite_sheet` is `_sprite_sheets[name]` (ResourceManager.cpp:51) — a **non-const, potentially-inserting** `std::map` operation on an object shared by all worker threads. The draw then calls `SpriteSheet::draw`, whose body is `this->_sprite_frames[frame_name].get_source_rectangle()` (SpriteSheet.cpp:105 and :126) — a second mutating `operator[]` on a second shared map, from N threads at once.

Even when no insertion occurs, concurrent invocation of a non-const `std::map` member from multiple threads is a data race by [res.on.data.races]. When an insertion *does* occur (any unknown sheet or frame name) the red-black tree is rebalanced under other threads' iterators. `ResourceManager::get_sprite_font` (:31) and `get_sound_bank` (:95) have the same shape and are reached from the same draw path (TextObject.cpp:30,50; DebugText.cpp:106).

**Fix:** Make every ResourceManager and SpriteSheet lookup `const` and non-mutating (`.at()` / `find()`), which removes the race entirely for read-only lookups. Better still for a per-frame hot path: resolve the `SpriteSheet*` and the `SpriteFrame` once at object construction and store them, instead of doing two string-keyed map lookups per sprite per frame.

### 32. Paintable face selection maps edge indices to the wrong faces — 3 of 4 faces paint on the wrong side

`ArtAttack/StructurePaintable.cpp:188-213` — **critical** · `correctness` · independently reported by 2 agents

generate_paint_tiles() recovers which faces are paintable by matching `this->get_shape()->get_edges()` against `_paintable_edges` and mapping the positional index: `if (i==0) top_edge=true; else if (i==1) right_edge=true; else if (i==2) bottom_edge=true; else if (i==3) left_edge=true;`.

MattMath.cpp:808-818 returns `{ get_top_edge(), get_bottom_edge(), get_left_edge(), get_right_edge() }` — top, bottom, left, right. So the actual mapping is: JSON top→top (correct), JSON bottom→right face, JSON left→bottom face, JSON right→left face.

I verified this against the shipped level data (all three files parsed). Live consequences:
- king_of_the_hill.json `castle_ceiling` (1650x25, bottom only): should get ~412 tiles on its underside, actually gets ~6 on its right end. The castle ceiling is unpaintable.
- `castle_wall_left` (25x750, right only) and `castle_wall_right` (25x750, left only): tiles land on the opposite face, i.e. the outside of the castle instead of the inside.
- `castle_floor_left` (top+right) → generates top+left; `castle_floor_right` (left+top) → generates bottom+top.
- close_quarters.json `lower_paint` (3000x50, top+bottom) → generates top+right, losing ~738 tiles.
- turbulence.json `central_divider_ledge_paint` (left+top+right) → generates bottom+top+left.
All-four-face structures (mountain_platform_left/right, upper_top_paint, etc.) are unaffected because the set is symmetric — which is why this has gone unnoticed.

The root cause is the round trip itself: LevelObjectBuilder.cpp:107-130 encodes four JSON booleans as `Segment`s, and this code decodes them back into booleans via an undocumented positional ordering. Candidates 1, 43, 57 and 76 all report this; merged here.

**Fix:** Delete the encode/decode entirely. Pass the four booleans (or a `paintable_faces` bitmask) straight from LevelObjectBuilder into StructurePaintable — the commented-out `PaintableFaces` struct at StructurePaintable.h:17-23 and LevelObjectBuilder.cpp:101-105 already did exactly this. If edge matching must stay, compare against the named accessors (`rect.get_top_edge()` etc.) rather than positional indices, and add a unit test pinning `RectangleF::get_edges()` ordering.

### 33. No exception barrier in the thread-pool callback — any task exception terminates the process and leaks the task

`ArtAttack/ThreadPool.cpp:60-65` — **critical** · `robustness` · independently reported by 3 agents

```cpp
void CALLBACK ThreadPool::work_callback(PTP_CALLBACK_INSTANCE instance, PVOID parameter, PTP_WORK work)
{
    std::function<void()>* task = static_cast<std::function<void()>*>(parameter);
    (*task)();
    delete task;
}
```

There is no try/catch. An exception escaping a Win32 thread-pool callback is unhandled: the process is terminated, `delete task` never runs, and the caller in `wait_for_tasks_to_complete` has no way to observe the failure (there are no futures, no error channel, `add_task` returns void).

The tasks *do* throw, on ordinary paths:
- `Level::draw_player_view_level` — `throw std::exception("Deferred context not created")` (Level.cpp:500) and `throw std::exception("Failed to finish command list")` (Level.cpp:590).
- `Level::draw_zoom_out_level_component` — Level.cpp:616, 649.
- `MenuPage::draw_mobject_in_viewports` — MenuPage.cpp:96, 120.
- Every `->at(i)` in those functions (Level.cpp:498, 503-506, 587; MenuPage.cpp:176) throws `std::out_of_range`.
- `SpriteSheet::get_animation_strip` uses `_animation_strips.at(name)` (SpriteSheet.cpp:17) — a missing animation name in a JSON asset crashes the process from a worker with no diagnostic.

Also note `instance` and `work` are unnamed-unused parameters under /W4 (C4100).

**Fix:** Wrap the invocation: `try { (*task)(); } catch (...) { /* capture into a per-task std::exception_ptr */ }` and store the `std::unique_ptr<std::function<void()>>` so the delete is exception-safe. Change `add_task` to return a handle (or `std::future<void>`) and have `wait_for_tasks_to_complete` rethrow the first captured exception on the calling thread.

### 34. Weapon::draw mutates shared per-object state and is executed concurrently on several render threads

`ArtAttack/Weapon.cpp:36-82` — **critical** · `memory-safety`

`Weapon::draw` is non-const and writes four members of its own base subobjects before issuing the batch call: `TextureObject::set_element_name(get_details().frame_name)` (Weapon.cpp:61, assigns `SpriteSheetObject::_element_name`, a `std::string` - SpriteSheetObject.h:26), `DrawObject::set_origin`, `set_effects`, `set_draw_rotation` (:62-64), and again in the debug branch (:71-78). `WeaponRoller::draw` additionally writes `_colour` (WeaponRoller.cpp:90-97).

That object is shared across threads. `Level::draw_active_level` (Level.cpp:379-399) partitions the players and submits one `ThreadPool` task per partition; `Partitioner::partition` (Partitioner.cpp:4-22) yields one partition per player while players <= threads, and `ThreadPool` is a real Win32 pool with `SetThreadpoolThreadMaximum(16)` (ThreadPool.cpp:15-18). Each task runs `Level::draw_player_view_level`, which at Level.cpp:534-540 loops over **all** `_player_objects` - so with N players, player 0's `Player::draw` (Player.cpp:47-89) and therefore `this->_primary->draw(...)` is entered by N threads simultaneously. Unsynchronised concurrent write/write on a `std::string` is UB and can tear the pointer/size/capacity triple.

Downstream is worse: `SpriteSheetObject::get_sprite_sheet()` reaches `ResourceManager::get_sprite_sheet`, which is `this->_sprite_sheets[name]` - `std::map::operator[]`, a mutating call - from all those threads at once (ResourceManager.cpp:46-58).

**Fix:** Make drawing a pure read. Compute element name / origin / effects / rotation / colour into locals and pass them as arguments to a `const` `TextureObject::draw(...)`, then declare `Weapon::draw` and the whole `IGameObject::draw` chain `const` so the compiler enforces it. Separately, change `ResourceManager::get_*` and `SpriteSheet::draw` to use `find()`/`at()` so lookups cannot mutate the container. Resolving the `SpriteSheet*` and `SpriteFrame` once at construction removes both the race and the per-frame lookups.

### 35. stop_sounds() throws for Sniper and Bucket, terminating the process at the end of every round

`ArtAttack/Weapon.cpp:252-277` — **critical** · `correctness` · independently reported by 2 agents

`Weapon::stop_sounds() const` (Weapon.cpp:252-257) is non-virtual and unconditionally calls `get_sound_effect_instance_name()` (Weapon.cpp:258-277), whose switch handles only SPRAYER, ROLLER and MISTER and ends in `default: throw std::exception("Weapon::get_sound_effect_instance_name() - invalid weapon type")`. SNIPER and BUCKET are buildable (WeaponBuilder.cpp:22, :34) and selectable in the weapon-select menu (MainMenu.cpp:1589, :1601). They dodge the throw during play only because WeaponSniper.cpp:28-35 and WeaponBucket.cpp:28-35 override `handle_shoot_sound` to use `play_wave`; neither overrides `stop_sounds`, which is not virtual anyway. The path is unconditional and hit every match: Level.cpp:135 `this->stop_player_sounds();` on the ACTIVE->ZOOM_OUT transition -> Level.cpp:323-329 loops every player -> Player.cpp:1048-1050 -> `_primary->stop_sounds()`. There is no try/catch anywhere on the tick path (grep of Main.cpp and Game.cpp finds none), so the exception reaches std::terminate. Merged from four independent reports.

**Fix:** Stop keying sound identity off `wep_type`. Resolve the looping instance name (or nullptr/empty for wave-based weapons) once in the Weapon constructor from data in `WeaponDetails`, and make `stop_sounds()` a no-op when there is none. A teardown path must never throw. Adding an explicit SNIPER/BUCKET case plus `virtual stop_sounds()` is the minimum patch, but the data-driven fix removes the whole class of bug.

### 36. Match end throws an unhandled exception if any player picked Sniper or Bucket

`ArtAttack/Weapon.cpp:252-277` — **critical** · `correctness` · independently reported by 2 agents

`Weapon::stop_sounds() const` (252-257) calls `get_sound_effect_instance_name()` (258-277), whose switch only handles `SPRAYER`, `ROLLER`, `MISTER` and ends in `default: throw std::exception("Weapon::get_sound_effect_instance_name() - invalid weapon type");` (273-274).

Can Sniper/Bucket actually be equipped? Yes, three independent ways:
- `MainMenuWeaponSelect::cycle_weapons` (MainMenu.cpp:1524-1554) cycles `0..MAX_PRIM_WEP` where `MAX_PRIM_WEP = RANDOM_PRIMARY = 5` (wep_type.h:4-18), i.e. SPRAYER(0), SNIPER(1), ROLLER(2), MISTER(3), BUCKET(4), RANDOM(5). `update_weapon_select_visuals` (MainMenu.cpp:1585-1608) draws icons for all of them, so they are visibly selectable.
- `get_random_weapon()` (MainMenu.cpp:1519-1523) is `rand() % 5` → picks SNIPER or BUCKET with probability 2/5.
- `WeaponBuilder::build_weapon` (WeaponBuilder.cpp:22-25, 34-37) happily constructs `WeaponSniper` / `WeaponBucket`.

Why it does NOT throw every frame (this matters — an earlier reading implied a broader crash): both classes override the *virtual* `handle_shoot_sound` (WeaponSniper.h:22 / WeaponSniper.cpp:28-35, WeaponBucket.h:23 / WeaponBucket.cpp:28-35) and use `play_wave` directly, bypassing `get_sound_effect_instance_name()`. But `stop_sounds` is declared **non-virtual** at Weapon.h:44, so it is not overridable and always lands in the base version.

REACHABILITY: LIVE, and it fires at exactly the 'match ends' step of a normal session. Shortest chain:
Game::tick (Game.cpp:75) → Game::update → StateContext::update → GameLevel::update → Level::update (Level.cpp:126-135, `_timer <= 0` → ZOOM_OUT) → Level::stop_player_sounds (Level.cpp:323-328) → Player::stop_sounds (Player.cpp:1048-1051) → Weapon::stop_sounds (Weapon.cpp:252) → throw at Weapon.cpp:273.

There is no try/catch anywhere on that path (see the separate finding on Main.cpp:211-224), so this is `std::terminate`.

**Fix:** Two fixes, both needed. (1) Make the sound-name lookup total: give SNIPER and BUCKET entries, or better, put the `SoundEffectInstanceWeaponDetails` on `WeaponDetails` so it is data, not a switch — the switch at Weapon.cpp:264-275 duplicates knowledge the subclass already has. (2) `stop_sounds` must be `virtual` and overridden alongside `handle_shoot_sound` in WeaponSniper/WeaponBucket (which have no looping sound to stop, so their override is empty). The real defect is that one half of a two-method protocol was made virtual and the other half wasn't.


---

## Architecture, design and the engine/game split

The stated goal is a reusable 2D engine with a Splatoon-like game as the proof. Structurally, that split does not exist anywhere: there is one MSVC project, one flat directory of 193 files, and no compilation boundary that could stop game concepts leaking into engine code — so they have, comprehensively. The game's team model, weapon roster, map names and asset filenames appear inside the collision interface, the resource cache, the input layer, the level file format and the colour palette. Underneath that, the object model inherits its rendering backend rather than composing it, so `DirectX::SpriteBatch` is part of the type identity of every game object and every UI widget. The engine is also missing most of the abstractions it repeatedly hand-rolls: no contact manifold, no broad phase, no view, no state stack, no asset handle, no audio bus. The good news is that the ordering is clean — fixing the build target makes the leakage visible, and most of the rest are mechanical once there is a wall to enforce them.

### The missing boundary

**There is no engine target, so nothing can consume the engine — including the tests.** `ArtAttack/ArtAttack.vcxproj:29,36,43,51` set `ConfigurationType=Application` in all four configurations, so no `.lib` is emitted. `MattMathTests/MattMathTests.vcxproj:173` declares a `ProjectReference` that links nothing; `MathTests.cpp:5-6` and `CollisionToolsTests.cpp:3` therefore `#include` implementation `.cpp` files directly, and the two test TUs implicitly link to each other (CollisionTools gets its MattMath symbols from the *other* test file). Only 3 of ~81 TUs are testable. **Fix:** split into `MattMath` (StaticLibrary, zero deps), `ArtAttackEngine` (StaticLibrary), `ArtAttackGame` (Application), sharing one `.props`. This is the prerequisite for every other item in this section.

**The directory layout hides the problem.** `ArtAttack.vcxproj.filters:3-11` declares two filters covering 6 of ~200 items; `DeviceResources.cpp`, `MattMath.cpp`, `Player.cpp` and `MainMenu.cpp` are siblings. Mirror the three-target split on disk (`engine/math`, `engine/render`, `engine/core`, `game/…`) and regenerate the filters from paths.

**No `<LanguageStandard>` is set in either project**, so MSVC v145 silently builds as C++14 (`ArtAttack/ArtAttack.vcxproj:88-170`). That single omission blocks `std::optional` (which would delete six out-param overload pairs in `MattMath.h:99-136` and nine bool+out-param signatures in `EricsonMath.h:93-142`), `inline constexpr` (which would delete the per-TU dynamic-initialisation problem across `Colour.h`, `weapon_consts.h`, `player_consts.h`, `directory_consts.h` and four menu constant namespaces), `std::variant`, `std::clamp` and `[[nodiscard]]`. `TreatWarningAsError` is also unset, so /W4 is decorative. Set `stdcpp17` or `stdcpp20` first — several other fixes depend on it.

### Game rules welded into engine interfaces

These share one root cause: with no boundary, the shortest path to a feature was to put game vocabulary in the shared type. Fix them together.

| Site | What leaks | Fix |
|---|---|---|
| `ICollisionGameObject.h:14` + `collision_object_type.h:4-88` | A 23-value enum cross-multiplying class × team × weapon (`PROJECTILE_MIST_TEAM_B`), plus 8 game predicates, is the collision interface | `{layer, mask}` bitfields the engine filters on, plus an opaque game-owned tag |
| `IPaintableGameObject.h:4-15` | `PaintTotal{team_a, team_b}` — two-team paint scoring as a core interface | Move to `game/paint/`; the game holds its own surface list |
| `ResourceManager.h:7,32-35,52` | Level cache keyed on `level_stage{KING_OF_THE_HILL,…}` while all four sibling caches use `std::string` | Key by string; delete `level_stage.h` from the engine |
| `PlayerInputData.h:9-22` + `PlayerInput.cpp:27-32` | Raw input layer emits `jump_pressed`/`primary_shoot`, hardwired to XInput button positions, no keyboard, no rebinding | `DeviceState` + game-registered `ActionMap` |
| `LevelLoadedInfo.h:17-18,36-37` | The level *file format* has room for exactly two teams | Named spawn groups: `get_spawn_group(name)` |
| `Colour.h:309-313` | Five `TEAM_*` constants in the engine palette, reached by every drawable via `DrawObject.h:4` | Move next to `TeamColour.h` |
| `ViewportManager.h:10-16` | Split-screen dividers hardcode `"sprite_sheet_1"` / `"pixel"` | `DividerStyle` passed in by the game |
| `DebugText.h:7,29` | Debug overlay `#include`s `Player.h` and takes `const Player*` | `add_line(label, value)`; game pushes its own lines |
| `Main.cpp:6,208` | The platform entry point includes `GameStates.h` and constructs `GameMenu` | `run_game(HINSTANCE, unique_ptr<IApplication>)` |

Two more of the same shape: `ResourceLoader.cpp:17-51` *is* the asset manifest — twelve filenames in C++, loaded serially on the main thread while the ThreadPool sits idle 22 lines away; and `MattMath.h:6,767-806` drags `<d3d11.h>` into 68 of 81 TUs for one `Viewport` struct that exists to convert to `D3D11_VIEWPORT` (and is type-punned via `reinterpret_cast` at `MattMath.cpp:3215`).

### The renderer is inherited, not injected

**`IGameObject.h:11-12` puts `DirectX::SpriteBatch*` in the root interface of every object in the engine, including the entire UI widget tree.** There is no renderer abstraction anywhere: raw `SpriteBatch*` threads through `Mh.h:23-24`, `MenuPage.h:38-50`, `Level.h:83-85`, `GameData.h:39`. `Level`'s public draw API is three raw pointers to vectors of D3D11 objects (`Level.h:83-85`), `MenuPage` is a command-list orchestrator (`MenuPage.h:38-53`), and `GameStates.cpp:303-346` — a *State*, a flow-control object — hand-loops `ExecuteCommandList`/`Release`. Nothing below `IGameObject` can be tested headlessly, which is exactly why `MattMathTests` covers only MattMath and CollisionTools. **Fix:** an engine `IRenderer`/`DrawList` (`draw_sprite(texture, src, dst, tint, rotation, origin, depth)`) with a DirectXTK implementation; `draw(IRenderer&) const`. Collapse the two `draw` overloads while you are there — the camera-less one is satisfied everywhere by forwarding `Camera::DEFAULT_CAMERA` (`Player.cpp:91-95`, `Structure.cpp:35-38`, all five projectiles). Move culling out of objects too: `is_visible_in_viewport` is the same one-liner in nine classes and is outright broken in three (`ProjectileRolling.cpp:44-47` returns `false`; `Mh.cpp:162-165` is unconditional infinite recursion). Objects should expose `get_bounds()`; the scene tests once.

### Inheritance used where composition belongs

**`Player.h:63-64` is a non-virtual diamond**: `AnimationObject` and `TextureObject` both derive `SpriteSheetObject → DrawObject` non-virtually, so every `Player` physically contains **two** complete sets of `_resource_manager`, `_colour`, `_draw_rotation`, `_origin`, `_effects`, `_layer_depth`, `_sheet_name`, `_element_name` — deliberately initialised to different values (`Player.cpp:29,31`) and kept in sync by hand (`Player.cpp:83,86`). `Player*` → `DrawObject*` is ambiguous, so a Player can never be handled generically as a drawable. Same shape in `Projectile.h:19`, `Structure.h:7`, `Visual.h:8`. **Fix:** one `Sprite` value type held by composition; an entity *has* sprites.

Other instances of the same mistake, all fixable independently:

- **`MovingObject.h:16-57`** — 27 protected virtual accessors over three fields, with exactly one override in the whole codebase (`Player.h:104`), whose body is `return MovingObject::get_velocity();` — access laundering. Make it a plain `struct Kinematics` member.
- **`MenuData.h:7`** — the game's UI state classes *inherit* the engine's 13-pointer service locator, three levels deep, each snapshotting all 13 pointers via a hand-written copy constructor (`GameData.cpp:4-20`) that must be edited whenever a service is added. `GameData` has no virtual destructor yet hands out `GameData*` (`GameData.cpp:122`). Compose; rename to `EngineServices`; narrow it.
- **`SoundBankObject.h:7-36`** — a string plus a pointer plus twelve one-line forwarders, publicly inherited by four unrelated menu page classes with a public non-virtual destructor.
- **`Text.cpp:22-37`** — a whole class whose entire body is four forwards that exist only to widen `protected` to `public`. `using TextObject::set_text;` does it with no code. Same for `MWidget` (`Mh.h:57-72`), which re-declares four already-pure virtuals to add one setter that all three leaves forward away.
- **`MattMath.h:184,68-89`** — `RectangleF`, the engine's universal value type and AABB, inherits the polymorphic `Shape`. It therefore carries a vptr, cannot be `constexpr`, is 24 bytes instead of 16, is not trivially copyable, and is returned *by value* from every `get_bounding_box()` call in the collision inner loop. `Shape` also leaves copy-assignment implicitly public, so base-to-base assignment silently no-ops. Split the math primitive from the collision hierarchy; as a one-line stopgap, `protected: Shape(const Shape&) = default;`.

### Missing abstractions the code hand-rolls

Each of these is a place where the absent engine concept forced game code to reimplement it, usually several times:

- **No contact manifold.** `CollisionTools.cpp:32-69` resolves collisions by 40-iteration bisection that *mutates geometry*, re-running full narrow phase (with `dynamic_cast` and a heap allocation for triangles) each iteration; `calculate_object_collision_depth` clones the shape to measure penetration. "Collision direction" is a centre-to-centre unit vector (`CollisionTools.cpp:109-134`) that callers quantise to eight compass points (`Player.cpp:250`) and dispatch through an eight-branch ladder ending in `throw` (`Player.cpp:490-526`). A real SAT/MTV normal deletes `BRACKET_ITERATIONS`, both bracket functions, the ramp enum tags, `RAMP_GRAVITY_MULTIPLIER` and the eight `on_*_collision` methods, and makes arbitrary slopes work by construction.
- **No broad phase.** `Level::update_level_logic` (`Level.cpp:180-293`) is three nested loops testing every pair *twice*, single-threaded, while `Partitioner` — which despite its name does no spatial partitioning at all — parallelises only drawing. Meanwhile `is_colliding` is on the object interface, so the same 32-line narrow-phase body is copy-pasted into seven classes (`Player.cpp:148-185`, `PaintTile.cpp:84-109`, all five projectiles), and the relation is asymmetric: `Structure::is_colliding` unconditionally returns `false` (`Structure.cpp:43-46`), so structure collisions work only because the caller happens to dispatch both orderings.
- **No View.** `Level.cpp:491-593` uses one index `i` as player index, deferred-context index, sprite-batch index and command-list index simultaneously; the camera, viewport and HUD all come from `_player_objects->at(i)`. No spectator view, minimap, or render-to-texture is expressible, which is why the zoom-out path is a separate duplicated function.
- **No state stack.** `StateContext.h:7-17` holds one state with replace-only semantics, so `GameLevel` hand-rolls overlays three times with three parallel `unique_ptr<StateContext>` + action enum + data-class triples (`GameStates.h:60-69`). `transition_to` also destroys the running state from inside its own `update()` (`StateContext.cpp:20-25`) — a self-delete at eleven call sites, safe only because none touches a member afterwards. Add push/pop and make the swap deferred to the end of `update()`.
- **No asset handle.** Every draw re-resolves assets by string through two `std::map` lookups (`SpriteSheetObject.cpp:31-35` → `SpriteSheet.cpp:105`), per object per viewport per frame. `AssetHandle<T>` resolved once at construction.
- **No mixer.** `SoundBank` has no master or category volume (`SetMasterVolume` appears nowhere), music is a looping sound effect started by eight menu pages and stopped by one, and voices must be pre-declared by name — hence 24 hand-written `sprayer_a0…mister_b3` entries in `sound_bank_1.json`, the 2×4 struct in `WeaponDetails.h:21-33` and its 36-line nested-switch accessor.
- **No game mode.** `level_mode` is stored on `MenuLevelSettings` and read by nobody (`MenuLevelSettings.h:18` has zero callers); the single paint-coverage ruleset is spread across `Level::update`, `Level::get_level_end_info` and `LevelEndInfo`.

### Content that should be data is C++

Levels, sprite sheets and sound banks already load through rapidjson; entities do not. Five projectile classes (`ProjectileSpray.cpp` et al., ~440 lines) are byte-identical apart from which `DETAILS_*` constant they forward, and five weapon classes likewise; `weapon_consts.h:14-168` and `projectile_consts.h` hold all tuning as positional aggregates in headers, so a balance change is a full rebuild. `LevelObjectBuilder.cpp:15-200` is a closed if/else over four hardcoded type strings with `throw` at the end — the one part of the engine a content-driven game most needs to extend cannot be extended without editing it. **Fix:** load `ProjectileDefinition`/`WeaponDefinition` from JSON into a registry; make the object builder a `map<string, factory_fn>` the *game* registers into. Ten files and two switch statements disappear.

### Ceremony and dead architecture

Six stateless all-static classes are heap-allocated and threaded through the object graph to call what should be free functions: `Partitioner` (four layers of plumbing, `Game.h:75` → `GameData` → `LevelBuilder` → `Level`), `CollisionTools` (plus two global-scope tuning constants in a widely-included header), `WeaponBuilder` (`PlayerBuilder.cpp:8-11` allocates one to call a `static`), `ProjectileBuilder` (one allocation per weapon per player), `TeamColourTools`, `CameraTools`. Delete the classes; use namespaces.

Genuinely dead: the six `StructureRamp*` files are 100% commented out yet still compiled (`ArtAttack.vcxproj:252-254,348-350`) while ramps ship as tagged `Structure`s; `AnimatedSprite` is an uninstantiated fork of `AnimationObject` carrying both of its bugs; `dimension.h`, `direction.h` and `direction_lock.h` have zero includers and `direction.h`'s global enum collides with `MattMath::direction`; `menu_element.h` is the typed widget-identity system the ~90 string comparisons in `MainMenu.cpp` should have used, abandoned half-built. Roughly 2,000 lines (7% of the codebase) are commented-out prior implementations sitting beside their replacements — `Level.cpp:402-490` and `Player.cpp:391-483` are old bodies of functions live in the same file, which is actively misleading.

### If you only do three things

1. **Split the build into `MattMath` / `ArtAttackEngine` / `ArtAttackGame` static libs + app, mirror it on disk, and set `<LanguageStandard>`.** Nothing else in this section can be enforced without a wall, and the tests cannot stop `#include`-ing `.cpp` files until there is a lib.
2. **Delete `collision_object_type` from `ICollisionGameObject` in favour of layer/mask + an opaque game tag, and delete `IPaintableGameObject` and `level_stage` from the engine.** That one change removes the largest single body of engine/game inversion and collapses the four hand-rewritten type ladders in `Player`, `PaintTile`, `StructurePaintable` and `Level`.
3. **Introduce `IRenderer` and change `IGameObject::draw` to `draw(IRenderer&) const`, then break the `Player` diamond by composing a `Sprite` instead of inheriting two of them.** Together these make the object layer renderer-agnostic and headlessly testable, which is the actual definition of "reusable" here.

---

## Correctness, memory safety and concurrency

This is the section with the genuine crashes in it. Four separate JSON loaders overflow a one-byte heap allocation on every asset load; the parallel render path has every worker thread writing to the same `std::string` members of the same `Player` and `PaintTile` objects; two normal-play paths (`Weapon::stop_sounds`, `Level`'s out-of-bounds projectile check) throw uncaught exceptions and terminate the process; and the two input vectors are compacted by controller connection but indexed by player ordinal, so unplugging a pad reads off the end of a vector. Underneath the individual bugs sit three structural causes worth naming once: **`draw()` is a mutating operation**, which makes the thread pool unsound by construction; **raw non-owning pointers are snapshotted at construction and never re-seated**, which makes device-loss recovery a use-after-free of the entire scene graph; and **geometry and enums are used to smuggle data that should be plain fields**, which is where the silent wrong-behaviour bugs cluster. Encouragingly, a large fraction of the math-library defects are in dead code — but they are shipped in the engine's public headers, so they are traps rather than bugs.

### Memory safety

**Every JSON file is parsed through a 1-byte buffer declared as 8 bytes.** `ArtAttack/Save.cpp:14-23` — and identically `SpriteSheet.cpp:72-81`, `SoundBank.cpp:152-161`, `LevelLoadedInfo.cpp:96-110`. `std::make_unique<char>()` allocates one byte; `sizeof(read_buffer)` is `sizeof(unique_ptr)` = 8. rapidjson's `FileReadStream::Read` does `fread(buffer_, 1, bufferSize_, fp_)` then `buffer_[readCount_] = '\0'` — a 7-byte overrun per refill, thousands of times per launch (king_of_the_hill.json is ~22 KB). `read_buffer.release()` then leaks the block, so the corrupted allocation never reaches `free()` and the debug heap check never fires. `fopen` is unchecked and `HasParseError()` is never queried in any of the four. **Fix:** one shared `rapidjson::Document parse_json_file(const std::string&)` helper with a real `std::vector<char>` buffer, a checked open, an RAII `FILE*` guard, and a throw naming the path and `GetParseError()`. Then grep the tree for `sizeof(` applied to a smart pointer.

**A level-lifetime object holds a reference to a builder local.** `ArtAttack/StructurePaintable.h:56` declares `const std::vector<Segment>& _paintable_edges;`, bound at `StructurePaintable.cpp:30` to the function-local built at `LevelObjectBuilder.cpp:107` and destroyed when `build_collision_object` returns. It survives only because the sole reads happen in `generate_paint_tiles()`, called from the constructor body. The reference member also silently deletes the `= default` constructor declared at `StructurePaintable.h:28`. **Fix:** pass it as a parameter to `generate_paint_tiles` and store nothing.

**Compacted input vectors indexed by player ordinal.** `ArtAttack/PlayerInput.cpp:126-137` only `push_back`s entries for connected pads, but `Level.cpp:189-192` subscripts by loop position and `PauseMenu.cpp:53/54/59/82/98/222/229/256` subscripts by a stored `player_num` that came from a *different* compacted vector (`GameStates.cpp:348-358`). Unplug a pad mid-match and you get both a silent identity shift (pad 1 now drives player 0) and an unchecked read past the end; drop all pads and you index an empty vector. This does not need hardware — `Game::on_deactivated` suspends the gamepad on every alt-tab (`Game.cpp:165-169`) while `Main.cpp:222` keeps ticking unconditionally. `PlayerInputData::connection` exists for exactly this and is hardcoded to `CONNECTED` at `PlayerInput.cpp:111`. **Fix:** return a fixed 4-slot array indexed by pad slot with the connection flag set honestly, look up by `player->get_player_num()`, and treat DISCONNECTED as neutral input.

**Same class of bug in spawn selection:** `PlayerBuilder.cpp:26-37` indexes two *per-team* spawn arrays with one *global* counter, so team A takes spawns 0 and 2 and never 1 and 3, with unchecked `operator[]` against unvalidated JSON. Separate team counters plus a size check up front.

### Concurrency

**`draw()` mutates shared objects, and every worker draws every object.** `Level::draw_active_level` (`Level.cpp:386-397`) partitions *players* into tasks, but each task's `draw_player_view_level` loops over all of `_non_collision_objects`, `_collision_objects` and `_player_objects` (`Level.cpp:516-540`). With N players, N threads enter `Player::draw` on the same instance. `Player::draw` (`Player.cpp:47-90`) assigns `_animation_state`, `set_sprite_sheet_name`, `set_element_name`, `set_animation_strip_and_reset`, `set_effects`. Concurrent unsynchronised assignment to a `std::string` is heap corruption, not a stale frame. Same shape in `Weapon::draw` (`Weapon.cpp:36-82`), `PaintTile::draw` (`PaintTile.cpp:34-53`, thousands of tiles per structure) and `TextDropShadow::draw` (`TextDropShadow.cpp:28-48`, which save/mutate/restores its own base state). There is not one `std::mutex` or `std::atomic` in the first-party tree. **Fix:** move every state transition into `update()` — the animation-state block in particular belongs there anyway, since an off-screen player currently never changes clip — then make `IGameObject::draw` and everything it reaches `const` so the compiler enforces it.

**Mutating map lookups on the render hot path.** `ResourceManager::get_texture/get_sprite_font/get_sprite_sheet/get_sound_bank` (`ResourceManager.cpp:7-102`) all use `map::operator[]` inside a `catch (std::out_of_range&)` that can never fire — `operator[]` default-inserts, it does not throw. So the intended diagnostic is dead, a misspelled name silently returns `nullptr` into `SpriteBatch::Draw`, and the lookup is a non-const tree mutation invoked from N threads. `SpriteSheet::draw` repeats it at `SpriteSheet.cpp:105` and `:126`. `get_level_info` (`:66-78`) uses `.at()` and is the only const one — the right idiom is ten lines away. **Fix:** `.at()` or `find()` + explicit throw, mark all five `const`, and resolve sheet/frame/font handles once at construction so the render path touches no string-keyed map.

**A worker thread drives the immediate D3D context.** `Level::draw_zoom_out_level_component` runs inside a pool task (`Level.cpp:601`) and calls `apply_player_viewport(0)`, whose one-argument overload (`ViewportManager.cpp:63-70`) does `GetD3DDeviceContext()->RSSetViewports(...)` on the *immediate* context and reconfigures SpriteBatch 0, which belongs to deferred context 0. `ID3D11DeviceContext` is not free-threaded. The same task also calls `set_layout(ONE_PLAYER)` (`Level.cpp:619`) — a permanent mutation of shared presentation state from a `const` draw function, never restored, which is why restarting after a match renders every player fullscreen. **Fix:** delete the one-argument overload and `ViewportManager::_sprite_batch`; use the three-argument overload the rest of the draw code already uses; hoist `set_layout` into the state transition.

### Lifetime and shutdown

These share one root cause: **`create_device_dependent_resources` is not a device-resource function.** `Game.cpp:218-261` reallocates `_thread_pool`, `_resource_manager`, `_resource_loader`, `_dt`, `_viewport_manager`, `_states` and `_partitioner`, and `OnDeviceRestored` (`:282-287`) re-runs it wholesale while `OnDeviceLost` leaves the state tree alive. Every live object snapshotted the old pointers: `Level.h:107/118/119/140/141/146/147`, `Player.h:168/170`, `Weapon.h:47/123/124`, and `const float* _dt` in eleven headers. `OnDeviceLost` additionally destroys every `SoundBank` (`ResourceManager.cpp:119-126`), so the dangle starts before the restore. **Fix:** split one-time services from GPU resources; only the latter may be recreated. And stop passing frame time as `const float*` — pass `float dt` to `update(float)`, which deletes eleven dangling members outright.

| Ordering defect | Site | Effect |
|---|---|---|
| `_audio_engine` declared after `_resource_manager` | `Game.h:65, 74` | WaveBank/SoundEffectInstance destructors run against a freed AudioEngine on every clean exit |
| `Game` *inherits* `StateContext` | `Game.h:16` | Base subobject destroyed last, so the whole level/player graph is torn down after every service it borrows |
| Menu data declared after the `StateContext` pointing at it | `GameStates.h:24-29, 55-70` | Pages outlive their `MenuData`/`MenuInput`; latent only because destructors are `= default` |
| `MContainer` children declared after the container | `Mh.h:31-55`, `EndMenu.h:64-72` | Raw non-owning child pointers dangle through teardown |

**Deferred contexts leak and are never rebuilt.** `DeviceResources.cpp:716-730` stores 16 raw `ID3D11DeviceContext*` with no `Release()` anywhere and `~DeviceResources() = default`; `HandleDeviceLost` (`:418-453`) never recreates them, so `OnDeviceRestored` builds new SpriteBatches on contexts belonging to a destroyed device. Use `ComPtr` and move creation into `CreateDeviceResources()`.

**`transition_to` destroys the running state from inside its own `update()`.** `StateContext.cpp:20-25` move-assigns into `_state`, i.e. `delete this` at ~30 call sites, then runs the new state's `init()` under a dead stack frame. `PauseMenu.cpp:70-80` and `MainMenu.cpp:165-168` do not `return` afterwards. **Fix:** park the new state in a `_pending` member and apply it after `_state->update()` returns.

### Crashes on ordinary paths

- **`Weapon::stop_sounds()` throws for Sniper and Bucket** — `Weapon.cpp:252-277`. Non-virtual, and `get_sound_effect_instance_name()` only handles SPRAYER/ROLLER/MISTER before `default: throw`. Reached unconditionally from `Level.cpp:135` at the end of every match; no `catch` anywhere on the tick path. Resolve the instance name from `WeaponDetails` at construction and make teardown non-throwing.
- **Escaped projectiles kill the process** — `Level.cpp:315-321` throws `"Collision object out of bounds"`. Combined with `Projectile::get_delete_timer()` returning `_timer` (`Projectile.cpp:285-288`), so the expiry test is `_timer > _timer` and *nothing ever expires*, a single missed jet/mist shot (both have zero gravity and drag) terminates the game. Return `_details.delete_timer`; mark out-of-bounds objects for deletion instead of throwing.
- **`get_deferred_context` is `noexcept` and throws** — `DeviceResources.cpp:737-745`: guaranteed `std::terminate`, reachable from `MenuPage.cpp:175` which indexes by object rather than partition.
- **`throw new std::exception`** in two provably unreachable branches — `MenuInput.cpp:60-66, 118-122`. Throws a pointer nothing can catch. Delete both.

### Wrong results

**Paintable faces are permuted.** `RectangleF::get_edges()` returns `{top, bottom, left, right}` (`MattMath.cpp:808-818`) while `StructurePaintable.cpp:188-213` decodes the index as `{top, right, bottom, left}`. Three of four faces paint on the wrong side in shipped content: king_of_the_hill's `castle_ceiling` gets 6 tiles on its right end instead of 412 underneath; `lower_paint` in close_quarters loses ~738 tiles. This changes the win condition. The root cause is round-tripping four booleans through `Segment` equality (compared with exact float `==` under `/fp:fast`, `StructurePaintable.cpp:193`); the `PaintableFaces` struct that would fix it is sitting commented out at `StructurePaintable.h:17-23`. Adjacent bands also overlap by `THICKNESS²` at every corner (`:219-296`), double-counting area and roughly doubling corner hit scores.

**`triangles_intersect` tests 4 of 9 edge pairs.** `MattMath.cpp:393-404`: `for (int i = 0; i < 2; i++)` against `b_edges[0]` and `[1]` only. `a_edges[2]` and `b_edges[2]` are never tested, so the function is asymmetric in its arguments and misses real crossings. It underpins `triangle_quad_intersect`, `quads_intersect` and `quad_rectangle_rotated_intersect`. Replace with a full double loop, or better one SAT routine to replace the six hand-written near-duplicates at `193-229, 260-298, 381-407, 426-445, 453-490, 619-661`.

**Collision resolution.** Three distinct bugs in `CollisionTools.cpp`: the band cases at `:176-187` resolve on the wrong axis (`left && right && !top && !bottom` means the only separating axis is vertical, but it calls the horizontal comparator) — the thin-platform fast-fall case, which shoves the player sideways instead of landing them; `calculate_containing_collision_direction` at `:91-107` picks the axis opposite to the one its own comment describes; and the bisection push-out at `:32-69` can displace at most ~2× the collider's own bounding box (Σ(2/3)ⁱ = 2), its only failure return is unreachable, and `resolve_object_collision` discards it and returns `true` unconditionally at `:260`. Derive the push from the actual penetration (MTV) rather than the mover's size, and propagate failure.

**Projectiles pass through every ramp.** `Projectile.cpp:98-101` and `:124-127` hand-write a three-entry structure whitelist that omits `STRUCTURE_RAMP_LEFT/RIGHT`; the correct predicate `is_structure()` already exists at `collision_object_type.h:75-82` and is what `Player` uses. king_of_the_hill.json:271 ships a ramp. Meanwhile `Player::on_structure_ramp_collision`'s left-ramp arm is an empty `// TODO` (`Player.cpp:274-277`) that `LevelObjectBuilder` will happily construct.

**Type tag copy-paste.** `ProjectileMist.cpp:19-22` passes `SPRAY` as its `projectile_type`, so every mist particle reports `PROJECTILE_SPRAY_*`, takes `SPRAY_DAMAGE`, and makes all `PROJECTILE_MIST_*` branches dead. Invisible today only because both damage constants happen to be `0.01f`. Related: `Projectile::get_player_damage()` has zero callers — the entire per-projectile damage table in `projectile_consts.h` is inert while `Player.cpp:645-690` hand-writes a divergent one with the opposite sign convention.

**Dead players keep firing.** `Player::update_weapon_and_get_projectiles()` (`Player.cpp:742-750`) has no `player_state` check, so a dead player holding the trigger spawns projectiles from the corpse for the full 3-second respawn, with the looping shoot sound still playing — and that loop is only ever stopped from inside the per-frame update (`Weapon.cpp:236-251`), so it survives pause and outlives the weapon on every quit path.

**Geometry primitives that are simply wrong** (all currently dead code, all public API): `rotate_vector_by_ref` clobbers `x` before computing `y` (`MattMath.cpp:1800-1807`); `Vector2F::cross` returns the two unsubtracted terms (`:1720-1723`); `MattMath::sign` truncates a float determinant to `int`, returning "collinear" for any area < 1 (`:86-91`); `Colour::desaturate` is a byte-identical copy of `saturate` (`:2385-2400`); `MatrixF(int,int)` never sizes `_elements`, so every matrix factory writes out of bounds on an empty vector (`:2478-2513`); `Triangle::get_angle_N` returns the supplement of the interior angle (`:3512-3527`) and `find_hypotenuse` returns a vertex index used as an edge index (`:3665-3684`); `closest_pt_point_OBB` loops three axes of a 2D OBB and throws on `i == 2` (`EricsonMath.cpp:769-786`); `RectangleRotated(Segment, thickness)` never normalises its axes and throws for any segment whose length isn't 1.0 (`MattMath.cpp:4201-4215`). Delete what you don't need — `MatrixF`/`Matrix3x3F`/`Vector4F` have zero users — and unit-test what you keep.

**Numerical hazards on live paths:** `Vector2F::normalized()`/`normalize()` have no zero guard while the sibling `to_unit_vector()` does, and `Triangle::inflate`/`Quad::inflate`/`RectangleRotated::set_x_axis` all use the unguarded pair — a NaN vertex silently stops the shape colliding, and `/fp:fast` is on in all four configs so the compiler may assume it never happens. `angle_between` has no `acos` domain clamp (`:1808-1811`). `calculate_gradient` divides by Δx with no zero check (`:3594-3603`). `Circle::inflate` permits a negative radius, after which four different circle tests disagree (`:3346-3349`). And `Player::_health` is clamped only at the top, so a dead player's negative health reaches `InterfaceGameplay.cpp:123-142` and produces an inverted destination `RECT`.

### If you only do three things

1. **Fix the four JSON loaders** (`Save.cpp:14`, `SpriteSheet.cpp:72`, `SoundBank.cpp:152`, `LevelLoadedInfo.cpp:96`) with one shared helper. It is a heap overrun on every launch and the cheapest fix in the report.
2. **Make `draw()` const, top to bottom.** Move the animation-state, effects, name and colour mutation out of `Player::draw`, `Weapon::draw`, `PaintTile::draw` and `TextDropShadow::draw` into `update()`, and switch `ResourceManager`/`SpriteSheet` lookups to `.at()`/`find()`. Until that lands the parallel renderer is corrupting memory on every multiplayer frame.
3. **Make the input vectors slot-indexed instead of compacted**, and fix `Weapon::stop_sounds` and `Projectile::get_delete_timer`. Those three together remove the out-of-bounds reads and both of the guaranteed process terminations on ordinary play.

---

## Modern C++, API quality and build configuration

This is the theme where the codebase is furthest behind, and also the one where the fixes are cheapest per unit of value. The project runs a 2026 toolset (`v145`) against a 2014 language standard, because no `<LanguageStandard>` element exists in either `.vcxproj` — a grep across all 197 first-party files finds zero uses of `std::optional`, `std::string_view`, `if constexpr`, `std::clamp`, `[[fallthrough]]` or `[[nodiscard]]`, so nothing depends on staying there. `/W4` is on and demonstrably unread: an unreachable duplicated `return` sits in a hot accessor. The API surface has a deeper problem than any individual defect — `const` carries no information anywhere in the render or simulation path, four "getters" mutate the containers they read, one `noexcept` function throws, and failure is routinely encoded as a magic value (`-1`, `{-1,-1}`, `FLT_MIN`, a zero rectangle at the origin) rather than in the type. Fixing the build settings first is what unblocks most of the rest.

### Build configuration

**Set `<LanguageStandard>` — one line unblocks a dozen other fixes.** `ArtAttack/ArtAttack.vcxproj:88-170` sets `WarningLevel=Level4`, `ConformanceMode=true` and `FloatingPointModel=Fast` in all four configurations but never sets the standard, so MSVC defaults to `/std:c++14`. That is why `MattMath::clamp`/`clamp_ref` are hand-written twice (`MattMath.cpp:34-85`), why `player_consts.h:14-15` uses `FLT_MIN` as an "unset" sentinel instead of `std::optional<float>`, why ~800 header constants can't be `inline constexpr`, and why the deliberate fall-through at `GameStates.cpp:137-138` can't be marked. Add `<LanguageStandard>stdcpp20</LanguageStandard>` (or `stdcpp17`) — in a shared props sheet, not four times.

**The unit tests compile the code under test with different codegen than ships.** `MattMathTests/MattMathTests.vcxproj:96-156` sets no `FloatingPointModel` (so `/fp:precise`), no `ConformanceMode`, `Level3` instead of `Level4`, and no `/arch:SSE2` on Win32 — while `ArtAttack.vcxproj:94,116,139,165` sets `/fp:fast` everywhere. Because the tests textually include the implementation (`MathTests.cpp:5` → `..\ArtAttack\MattMath.cpp`) and assert to `EPSILON_F = 1e-6`, a green suite does not tell you the shipping build agrees. Move `FloatingPointModel`, `ConformanceMode`, `WarningLevel`, `EnableEnhancedInstructionSet` and `LanguageStandard` into one `Common.props` imported by both projects — or better, link a real engine `.lib` so `MattMath.cpp` is compiled once in the whole solution.

**Four hand-copied `ItemDefinitionGroup` blocks that have already drifted.** `ArtAttack.vcxproj:86-180`: `EnableEnhancedInstructionSet` appears only in the two Win32 configs (`:95,:140`); `GuardEHContMetadata` only in Release|x64 (`:168`); `WIN32` is defined in the x64 preprocessor lists (`:115,:164`); the include dirs, `/Zc:__cplusplus` and a stale link list (`comdlg32`, `advapi32`, `shell32`, `oleaut32` — none used) are written out four times. There is also no `<MultiProcessorCompilation>`, so 81 TUs build serially. Consolidate into a props sheet and leave only `Optimization`/`UseDebugLibraries` per config.

**Turn `TreatWarningAsError` on after clearing the backlog.** It is absent from both projects, and the backlog is small and specific: `GameData.cpp:142-146` (duplicated unreachable `return`, C4702), `Player.cpp:271` (dead local that also shadows the member function `is_on_ramp()`, C4189), `Player.cpp:1218` (unhandled enumerator `DROPPING_DOWN`, C4062), `DeviceResources.cpp:740`, `MainMenu.cpp:50`, `Level.cpp:386`, `EndMenu.cpp:32`, `MenuPage.cpp:140`, `ResultsMenu.cpp:266` (all C4018), plus C4100 clusters in `Weapon.cpp:188-192` and `ProjectileRolling.cpp:36-47`. Also enable `/w14263 /w14266` — they would have caught the hidden-virtual bug below.

**The built exe only runs under the debugger.** `directory_consts.h:9-13` uses `./textures/`, `./levels/`, `./fonts/`, `./sounds/`, but `ArtAttack.vcxproj:383-395` has no `<Content>` items, no `CopyToOutputDirectory` and no post-build copy; it works because VS sets the working directory to `$(ProjectDir)`. Add a content group with `PreserveNewest` and verify by launching from the output folder.

**Precompiled header carries 13 unused DirectXTK headers.** `ArtAttack/pch.h:59-77` includes `Model.h`, `Effects.h`, `PostProcess.h`, `PrimitiveBatch.h`, `GeometricPrimitive.h`, `ScreenGrab.h`, `BufferHelpers.h`, `VertexTypes.h`, `WICTextureLoader.h`, `Keyboard.h`, `Mouse.h`, `GraphicsMemory.h`, `DirectXHelpers.h` — none referenced anywhere in first-party code, and the two heaviest (`Model`, `Effects`) are 3D-only in a 2D sprite game. Every TU pays. Meanwhile `MattMathTests/pch.h` is an empty wizard stub with `PrecompiledHeader=Use` set in all four configs. Cut the former, and either populate or disable the latter. The test project also hardcodes `..\packages\directxtk_desktop_2019.2023.10.31.1\include` with no `packages.config` of its own, and its `<ProjectName>UnitTests</ProjectName>` matches neither the directory nor the file.

### API contracts that are false

**`get_deferred_context` is `noexcept` and throws.** `ArtAttack/DeviceResources.h:88` and `.cpp:737-745` — throwing from `noexcept` is an unconditional `std::terminate`, so the one defensive check in the function hard-kills the process instead of reporting a bad index. It is reachable: `MenuPage.cpp:175` indexes the 16-context pool by *widget* index. Drop `noexcept`, take `std::size_t`, and delete the manual check in favour of `at()`.

**Four resource "getters" use `map::operator[]` inside a `catch (std::out_of_range)` that can never fire.** `ResourceManager.cpp:7-19, 27-38, 46-58, 91-102`, plus `SpriteSheet.cpp:105,126`. `operator[]` default-inserts and never throws, so the diagnostic is dead code, a misspelled asset silently inserts a null entry and returns `nullptr` into `SpriteBatch::Draw`, and the lookup is a *write*, which is why all four are non-const. `get_level_info` (`:66-78`) already does it correctly with `.at()`. Use `.at()`/`find()`, mark them `const`, return `const T*`.

**That single choice is what makes `const` meaningless project-wide.** Because the getters are non-const, `DrawObject::get_resource_manager() const` (`DrawObject.h:20`) hands out a mutable pointer, so every `const` on the draw path is a lie. The same pattern recurs deliberately: `SoundBank.h:20-35` (const methods that start and stop playback, via `get_sound_effect_instance() const` returning a non-const pointer), `Player.cpp:641-644, 742-750, 1048-1051` (const members that advance shoot timers, spend ammo and spawn projectiles — which is what lets `Level::update_level_logic(...) const` at `Level.cpp:180` drive a whole simulation frame), `ResultsMenu.cpp:214-252`, and `ResourceLoader.h:15-54` where `load_texture` is const and `load_sprite_font` is not, for no reason. Fix the getters first, then let the compiler propagate the truth outward.

**Failure encoded as magic values.** `ResolutionManager.cpp:47-63` returns `{-1,-1}` for an unknown resolution while its three siblings fall back to 1280x720 — and that `-1` flows into `CreateWindowExW` and `SetWindow`. `player_consts.h:14-15` uses `FLT_MIN` as "unset" and compares it with exact `!=` under `/fp:fast` (`Player.cpp:66`). `ResultsMenu.h:104` returns `int` `-1` where the caller only tests presence. `RectangleF::intersection` (`MattMath.cpp:1099-1113`) returns a zero rect at the origin for "no overlap", which `CollisionTools.cpp:214-217` consumes unchecked. `CollisionTools.cpp:109-134` overloads `Vector2F::ZERO` as both "no collision" and a legal direction. Once on C++17, these are `std::optional`; until then, at least make the sentinels unrepresentable as valid values.

**Out-parameters where a return value belongs, each needing a hand-written discarding twin.** `MattMath.h:100,124,133,170-171` — `rectangle_circle_intersect`, `circle_triangle_intersect`, `circle_segment_intersect`, `segments_intersect` each require a second overload that declares a dummy local purely to throw it away (`MattMath.cpp:312-317, 342-346, 576-581`). Same shape in `clamp_ref` (`MattMath.h:52-54`), `Camera::calculate_view_rectangle(RectangleF&)` (`:1023`), `SoundBank::clamp_levels(float&,float&,float&)` (`SoundBank.cpp:174-179`), and `MenuPage.h:38-41`, which deposits a COM object's sole reference into a caller's container slot via `ID3D11CommandList*&`. Return a small struct (or `optional`) and delete the twins.

**`MenuPage::get_menu_inputs() const` consumes global edge state.** `MenuPage.h:53` → `MenuInput.cpp:147` overwrites `_prev_inputs`. A `const`, `get_`-prefixed accessor whose second call in a frame silently swallows every button press. Split into a non-const `update()` called once per frame and a genuinely const `get_inputs()` returning a reference.

### Type safety left on the table

| Gap | Site | Consequence |
|---|---|---|
| Missing `explicit` | `MattMath.h:409, 926-928, 1016` | `circle.intersects(5.0f)` compiles and means the point (5,5); a `RectangleF` implicitly becomes a `Quad` via a conversion that throws |
| Unscoped `enum projectile_type` | `Projectile.h:10-17` | `SPRAY`/`JET`/`BALL`/`MIST`/`ROLLING` leak into the global namespace of nearly every TU via `Weapon.h`→`Player.h`→`Level.h`, convert to `int`, and defeat `/W4` exhaustiveness checks |
| `MAX` sentinel aliasing the last enumerator | `TeamColour.h:17-23` | `rand() % MAX` can never return `BLUE_YELLOW`; the same spelling means *one-past-the-end* in `wep_type`/`level_stage`, so one constant has two readings |
| Unlabelled `bool` parameters | `Weapon.h:52` / `Weapon.cpp:229-251` | The two arguments are bound in the order **opposite** to their parameter names; a tidy-up swap is silent. Also `MTextDropShadow`'s bare `true` in slot 9 of 14 (`MainMenu.cpp:1942-1951`) and `play_effect(name, true, …)` |
| Overload pair that narrows and is ambiguous | `Partitioner.h:11-15` | `int`/`size_t` pair; the `size_t` body is an unchecked `static_cast<int>`, and any *other* integer type fails to compile as ambiguous |

**Constructors of 13-16 positional parameters, with the parameter-object struct already written and unused.** `Player.h:67-83` (16 params, including two adjacent `wep_type` enums separated only by a weapon pointer, plus four pass-through sprite defaults no caller ever supplies); `Weapon.h:17-29` and `:148-161` (13/14, with five trailing parameters never supplied by `WeaponBuilder` and re-declared verbatim across all five concrete weapon headers — and `rotation` never actually initialises `Weapon::_rotation`); `TextDropShadow.h:10-22` (four consecutive interchangeable floats). `InterfaceGameplay.h:37-45` already declares a `struct InterfaceDraw` for exactly this and is referenced at precisely one place: its own declaration. Introduce `DrawStyle`/`ShadowStyle`/`WeaponSpawnContext` value types and delete the dead trailing parameters.

### Modern C++ mechanics

**`X(const X&) = default;` on 17 types suppresses every move operation.** `MattMath.h:192, 360, 408, 516, 575, 594, 662, 687, 753, 778, 814, 847, 898, 923, 988, 1013, 1050`. A user-declared copy constructor — `= default` counts — kills the implicit move constructor *and* move assignment. The lines buy nothing (the implicit copy already existed) and cost real allocations on the two types with heap members: `RectangleRotated`'s `std::vector<Point2F> _points` (`:1119-1120`) and `MatrixF`'s `std::vector<float>` (`:723`). Delete all 17 lines; rule of zero.

**No `[[nodiscard]]` anywhere in the project** (grep: zero hits). The highest-value targets are the const/mutator pairs deliberately given near-identical names — `normalized()` vs `normalize()` (`MattMath.h:445-446`), `clamped()` vs `clamp()` (`:450-451`), `intersection()` sitting inside a block of `void` mutators (`:249`) — plus `Shape::clone()` (`:85`), every `intersects`/`contains`/`is_valid` predicate, `WeaponBuilder::build_weapon`, and `Weapon::update_and_get_projectiles` (discarding it silently drops the projectiles the player just fired).

**Header constants are per-TU dynamically-initialised objects with a real cross-header ordering dependency.** `projectile_consts.h:6-107` mixes `const` and `static const` for the same kind of value, and because `ProjectileDetails` holds two `std::string` members none of the eight blobs is a literal type — 12 string constructions per TU at static-init time, in a header that reaches most of the game. `weapon_consts.h:40,74,90,118,152` then initialises *its* namespace-scope statics by reading `projectile_consts::DETAILS_SPRAY.size.x`, and `DebugText.h:14` initialises from `Vector2F::ZERO`, which is dynamically initialised in `MattMath.cpp:1907`. Both work only because declaration order happens to save them. Same shape in `InterfaceGameplay.h:12-34`. Give the value types `constexpr` constructors, use `std::string_view` for asset names, and make every blob `inline constexpr`.

**Assorted mechanics worth a single sweep.** `MattMath.h:676-682` declares Vector3F's seven free operators `static` in a header, giving them internal linkage — the first TU outside `MattMath.cpp` to write `v3a + v3b` fails to link. `MenuInput.cpp:64,122` uses `throw new std::exception(...)`, which throws a *pointer* no `catch (const std::exception&)` can match, and leaks — the only two such sites in the tree. `EricsonMath.cpp:292-295` and a dozen sibling lines return `0`/`1` from `bool` functions. `GameStates.cpp:125,202,279` and `Game.cpp:224-232` wrap `std::move` around prvalues, defeating elision. `MattMath.cpp:3313-3317, 3405-3426, 3700-3776, 4100-4105` assign members in constructor bodies over their NSDMIs while neighbouring constructors use init lists correctly. `SpriteSheet.h:62` wraps a plain value type in `unique_ptr` inside a map, one line below a map that gets it right. `EricsonMath.h:119-120` takes one `Point2F` by value and its neighbour by const reference. And despite `<algorithm>` being in `pch.h:42`, the first-party tree contains **zero** uses of `std::find`, `remove_if`, `any_of`, `accumulate`, `count_if` or `sort` — hence the hand-rolled swap-erase at `Level.cpp:296-305` that relies on `unique_ptr` self-move-assignment and `size_t` wraparound.

### Inheritance API hazards the compiler could catch

**`TextDropShadow::draw` hides rather than overrides.** `TextDropShadow.h:24-25` declares both overloads without the trailing `const` that `TextObject.h:21-23` has, and without `override`. Cv-qualification is part of the signature, so these start two new virtual slots: any call through a `Text*`/`TextObject*` renders the text with no shadow, silently. It only works today because every call site names the concrete type. Add `const` and `override`, then apply `override` mechanically across the render hierarchy.

**Two unrelated virtuals sharing one slot.** `AnimationObject.h:21` declares `protected virtual void update()` with the same signature as `IGameObject::update()` (`IGameObject.h:10`), so `Player::update()` and `Projectile::update()` (`Projectile.h:38`) override both at once. The only thing preventing infinite recursion is the explicit `AnimationObject::` qualification at `ProjectileSpray.cpp:42`, `ProjectileJet.cpp:40`, `ProjectileMist.cpp:42`, `ProjectileBall.cpp:40` — one dropped token away — and `ProjectileRolling` simply forgot the call, which is why its animation never advances. Rename it `advance_animation()` and drop the `virtual`.

**Other slots worth closing.** `Weapon.h:31-33` hides all five inherited `TextureObject::draw` overloads, forcing explicit qualification at `Weapon.cpp:66,80` and `WeaponRoller.cpp:99,113` — which also defeats virtual dispatch; add `using TextureObject::draw;`. `MattMath.h:895-914` (`TriangleRightAxisAligned`) never overrides `clone()`, so every level ramp is stored sliced to a plain `Triangle`; enforce it with a CRTP `ShapeClonable<Derived>` mixin so a new leaf cannot forget. `MattMath.h:1110-1111` declares `RectangleRotated::operator==`/`!=` with no definition anywhere — a latent LNK2019 for the first caller. `Game.h:22-23` explicitly defaults move operations on a class that publishes `this` to three external observers; delete them. And `IGameObject`, `ICollisionGameObject`, `State` and `MovingObject` each declare a virtual destructor and stop, which suppresses move but leaves *copy* — so `a = b;` on two `MovingObject&`s compiles and slices half the physics state.

### Header hygiene

**Three includes have the wrong case and only compile because NTFS is case-insensitive:** `TeamColour.h:5` and `InterfaceGameplay.h:7` (`"colour.h"` for `Colour.h`), `Level.cpp:2` (`"level.h"`). These are hard build breaks on a case-sensitive filesystem, which is exactly what CI for a "reusable engine" would use.

**Several engine headers are not self-contained** — they compile only behind this project's PCH. `PlayerInput.h:31,38` names `DirectX::GamePad` with the only `#include "GamePad.h"` in the tree being `pch.h:64`; `SpriteSheet.h` names `ID3D11ShaderResourceView`, `SpriteBatch`, `RECT` and `unique_ptr` with none included; `ResourceManager.h`, `MenuPage.h` and `PlayerBuilder.h` are the same. Add a build step that compiles each header standalone.

**Transitive weight, all removable by forward declaration.** `Player.h:14` includes `CollisionTools.h` and never uses it; `Player.h:13` pulls `SoundBank.h` (and therefore `<Audio.h>` and `rapidjson/document.h`) for a raw pointer member; `Weapon.h:11` pulls `ProjectileBuilder.h`, which includes all five concrete projectile headers, into every TU that reaches `Player.h`; `ThreadPool.h:7` includes `<iostream>` and uses no stream, reaching nearly every TU via `GameData.h`. `Weapon.h:104` also gets `rotation_origin` through a four-level transitive chain with no direct include. Separately, `PlayerInputData.h:1-2` squats on the guard `PLAYER_INPUT_H`, which `PlayerInput.h:41`'s own `#endif` comment claims — "tidying" that comment would make `PlayerInputData` silently vanish. Switch first-party headers to `#pragma once`.

### If you only do three things

1. **Add `<LanguageStandard>stdcpp20</LanguageStandard>` and `<TreatWarningAsError>` to a shared `Common.props` imported by both `.vcxproj` files**, replacing the four drifted config blocks — this fixes the test/game codegen mismatch and unblocks `std::clamp`, `optional`, `string_view`, `inline constexpr` and `[[nodiscard]]` for every other fix in this section.
2. **Make resource lookups honest**: `.at()`/`find()` instead of `map::operator[]` in `ResourceManager.cpp` and `SpriteSheet.cpp`, delete the four unreachable catch blocks, mark the getters `const`, and drop `noexcept` from `DeviceResources::get_deferred_context`. That restores `const` as a real signal all the way up the draw path.
3. **Sweep `MattMath.h`**: delete the 17 `X(const X&) = default;` lines, add `explicit` to every single-argument constructor, and add `[[nodiscard]]` to every const accessor and factory — starting with the `normalize`/`normalized` and `clamp`/`clamped` pairs where the diagnostic maps directly onto a real class of bug.

---

## Performance, robustness, data pipeline and tests

The engine currently has no containment. Roughly 58 `throw std::exception("literal")` sites act as ordinary control flow, several of them on the normal per-frame path, and there is not a single `try`/`catch` between them and `wWinMain` — some throw from inside Win32 thread-pool callbacks, where no handler could ever catch them. Every JSON loader reads untrusted files with unchecked `fopen`, unchecked `ParseStream` and raw `operator[]`, so in Release (NDEBUG kills RapidJSON's asserts) a missing key is undefined behaviour rather than an error message. The hot paths are dominated by work that should not exist: string-keyed `std::map` lookups per sprite per viewport, heap allocations inside the narrow phase, an O(n²) collision loop with no broad phase, and thousands of full polymorphic game objects used as paint pixels. Numerically, the math library divides without guards, validates after mutating, and compares floats with an absolute epsilon smaller than one ULP at level scale — under `/fp:fast`. And the test suite covers the leaves while skipping the trunk: two tests for eleven `CollisionTools` functions, zero for the `Shape` dispatch layer, zero exception-path tests anywhere, and two brand-new OBB functions that throw on *every* call and shipped anyway.

### 1. Failure handling — the multiplier on everything else

**Exceptions escaping thread-pool callbacks terminate the process.** `ArtAttack/ThreadPool.cpp:60-65` — `work_callback` does `(*task)(); delete task;` with no `try`/`catch`. Task bodies throw routinely: `Level.cpp:500`, `:590`, `:616`, `:649`, `MenuPage.cpp:96`, `:120`, plus every `->at(i)` in those functions, plus `DeviceResources::get_deferred_context` (`DeviceResources.cpp:737`) which is declared `noexcept` and unconditionally throws. An escaping exception is `std::terminate` on a worker with no diagnostic, and the task leaks. Fix: capture into a `std::exception_ptr` on the work item, own the closure with `unique_ptr`, and rethrow from `wait_for_tasks_to_complete()` on the calling thread.

**No top-level handler at all.** `ArtAttack/Main.cpp:211-224` — the message loop calls `g_game->tick()` bare; `Main.cpp` contains no `try` or `catch`. `GameLevel::init` calls `build_level` unguarded (`GameStates.cpp:125`). Wrap the loop body and show a `MessageBoxW` with `e.what()`. Also replace `std::exception("literal")` (an MSVC extension) with `std::runtime_error`.

**Leaving the play area is a fatal error.** `ArtAttack/Level.cpp:307-321` throws `"Player out of bounds"` / `"Collision object out of bounds"` for any object whose AABB stops intersecting the level rect — an entirely ordinary event for a fast projectile. The engine already has `get_for_deletion` plus a sweep at `Level.cpp:296-305`, and `_team_a_spawns`/`_team_b_spawns` (`Level.h:136-137`) are stored and never read. Mark projectiles for deletion, respawn players, and fold the test into the existing sweep so the list is walked once.

### 2. Data pipeline — every failure is silent or fatal, never diagnostic

These share one root cause: **assets and level data are addressed by unvalidated strings and read with unchecked accessors.**

| Site | Defect |
|---|---|
| `Save.cpp:14`, `LevelLoadedInfo.cpp:98`, `SoundBank.cpp:152`, `SpriteSheet.cpp:72` | `fopen` result never null-checked; `RAPIDJSON_ASSERT` is compiled out under NDEBUG, so Release does `fread(buf,1,8,nullptr)` then `fclose(nullptr)` |
| same four + `LevelObjectBuilder.cpp:19-199` | `ParseStream` result discarded; repo-wide grep for `HasParseError`/`IsObject()`/`IsString` returns **zero** first-party hits; `HasMember` appears 7 times total |
| `ResourceManager.cpp:7-19, 27-38, 46-58, 91-102` | four getters use `map::operator[]` inside a `catch (std::out_of_range)` that **can never fire** — a miss returns `nullptr`, pollutes the map, and is dereferenced unchecked (`Weapon.cpp:32`, `Player.cpp:44`, `AnimationObject.cpp:21`) |
| `SpriteSheet.cpp:105, 126` | `_sprite_frames[frame_name]` inserts a default `{0,0,0,0}` rect — a misspelled frame draws an invisible sprite forever, while `get_animation_strip` uses `.at()` and throws for the same class of error |
| `SoundBank.cpp:110-141` | `WaveBank::CreateInstance` returns nullptr on a miss (verified in `Audio.h:336-339`); the `catch` is dead, a null instance is stored under a valid key and crashes later at play time |
| `Colour.h:315-466` | `colour_from_name` returns WHITE for any unknown name and omits the five `TEAM_*` colours defined 6 lines above — the one JSON field that fails silently |
| `Save.cpp:43-56` | `load_save_file` detects that writing the default save failed, prints to a `std::cout` that has no console in a `/SUBSYSTEM:WINDOWS` app, then parses the missing file anyway |

**Fix once, centrally:** one shared loader that null-checks `fopen`, checks `HasParseError()` and reports `GetParseErrorCode()`/`GetErrorOffset()`, plus `require_float(v,"x")`/`require_string(v,"type")` helpers that check `HasMember` **and** type and throw naming file, object and field. Then resolve every asset name to a handle at load time so `SpriteSheet`/`SoundBank`/font misses fail at startup, not mid-frame.

**Repo hygiene blocking a clean clone.** `.gitignore:365-368` ignores `*.wav`/`*.xwb`/`*.mp3`; `git ls-files ArtAttack/sounds` shows no wave bank is tracked, so a fresh clone cannot start a level. Also: `ArtAttack/save/save_data.json` is tracked and rewritten every run (dirty tree after every launch); three tool EXEs are committed; `ArtAttack/levels/unused/level_0.json` and `test_1.json` are unloadable dead content; and `TeamColour.h:5` / `InterfaceGameplay.h:7` spell `#include "colour.h"` in lowercase, which blocks any case-sensitive build of a supposedly portable engine.

### 3. Performance — the four things that actually cost frames

**Renderables store resource *names*, so every draw is 2-5 string-keyed tree walks.** Root cause at `ArtAttack/SpriteSheetObject.cpp:31-35`; amplified by `AnimationObject::get_animation_strip` (`AnimationObject.cpp:24-28`, called from `update()`:87, `draw()`:33 and `get_source_rectangle()`:141), `TextObject.cpp:29-30`, `SoundBankObject.cpp:14-21` (all twelve wrappers re-resolve the bank), and `Weapon.cpp:236-251` (a map lookup plus `SetVolume/SetPitch/SetPan/Play` **every tick**, and `Stop(true)` on an idle voice every tick). Ironically the dead `AnimatedSprite` class already caches the pointers. Resolve `SpriteSheet*`/`const AnimationStrip*`/`SoundEffectInstance*` once at construction; touch audio only on state transitions.

**Paint tiles are full polymorphic game objects, never culled, scanned linearly.** `StructurePaintable.cpp:45-62` draws every tile once the parent structure passes culling, and `PaintTile::is_visible_in_viewport` (`PaintTile.cpp:149-152`) has zero callers. Shipped counts: turbulence 5,574 tiles, king_of_the_hill 3,348, with `boundary_floor` alone at 1,500 — times up to 4 viewports, times 2 sprites each. `update()` (`:37-43`) ticks all of them every frame, and `on_collision` (`:89-101`) linear-scans all 1,500 per projectile hit on a *regular grid* whose index range is two divisions. Replace `std::vector<PaintTile>` with `{origin, tile_w, tile_h, nx, ny, std::vector<uint8_t> team}` — ~300 bytes/cell down to 1 — and keep a small active-splash list.

**Collision is O(n²) with no broad phase and redundant everywhere.** `Level.cpp:221-293` has three nested loops; loop 3 visits every *ordered* pair (no `j > i`), and player↔object pairs are traversed twice. On top of that: `Shape::get_edges/get_points/get_triangles` return `std::vector` by value (`MattMath.h:87, 865, 946, 961`) so one quad-vs-OBB query fans out to dozens of malloc/free pairs; `RectangleRotated::contains` round-trips through a validating `Quad` (`MattMath.cpp:611-617, 4450-4455`) — 3 allocations and 2 barycentric solves where 2 dot products suffice, *and it can throw*; the "cheap AABB early-out" at `MattMath.cpp:623` and `:457` is more expensive than the test it guards; circle tests take `sqrtf` (`:300-304, 339, 348`) though `distance_squared` exists; `calculate_object_collision_depth` (`CollisionTools.cpp:270-281`) heap-clones a polymorphic shape to produce one boolean; and `CollisionTools.cpp:35` and `:58` compute a `collider_aabb` that is never read — the one at `:58` inside a 40-iteration loop. Fix order: unique pairs + layer mask + a flat per-frame AABB array; then fixed-size edge/point returns; then the direct OBB `contains`.

**Weapons emit one projectile per tick and nothing caps the count.** `weapon_consts.h:41` sets sprayer `shoot_interval = 0.0f` and `:91` roller `0.0001f`, so DPS is a function of tick rate, and 5-second-lived projectiles feed straight into the quadratic loop above. Also `Weapon.cpp:202-227` resets the timer to zero (discarding overshoot, so the real interval is always `interval + dt`) and gates on `>` so the first update never fires. Use a real interval, `>=`, and `alter_shoot_timer(-interval)`.

**Graphics/threading waste.** `Game::clear()` (`Game.cpp:124-152`) records `OMSetRenderTargets`+`RSSetViewports` into all 16 deferred contexts every frame, but `Partitioner` caps partitions at the element count (≤4 players), so 12 contexts are never `FinishCommandList`ed — their command buffers grow unbounded for the whole session. Bind render target and viewport inside the recording function instead. Related: `Level::draw_zoom_out_level` (`:595-607`) dispatches one task and immediately joins (and computes an unused `num_threads` at `:599`); `MenuPage.cpp:124-164` spins up the pool, a partitioner and a 16-entry command-list vector to draw two widgets; `ThreadPool::add_task` copies the `std::function` twice (`ThreadPool.cpp:38-41` — `std::move(task)` is free); `Level.cpp:388` captures the whole partition vector into each closure; `ExecuteCommandList(..., TRUE)` in the level path vs `FALSE` in the menu path (`GameStates.cpp:328` vs `MenuPage.cpp:117`) though `clear()` discards the saved state anyway. And `MenuPage.cpp:142-143` captures caller stack locals **by reference** into tasks, which becomes a use-after-free the moment `add_task` throws mid-loop, because `wait_for_tasks_to_complete()` is a plain statement, not a destructor.

**Startup cost.** `Colour.h:10-313` defines 149 `const std::string` and 153 `const Colour` at namespace scope — internal linkage, so every including TU gets its own dynamically-initialised copy, each parsing hex at runtime via `set_from_hex` (`MattMath.cpp:2359-2384`). A single bad literal throws from a namespace-scope initialiser and terminates before `main` with no attributable message. Same pattern in `TeamColour.h:25-42` and ~780 namespace-scope const objects across 55 headers. Make `Colour`'s constructor `constexpr` and use `inline constexpr` — which requires setting `<LanguageStandard>` in the vcxproj, currently unset (so you are on the C++14 default).

### 4. Numerical robustness in MattMath

- **Unguarded divisions.** `normalized()`/`normalize()` (`MattMath.cpp:1724-1734`) divide by length while the identical `to_unit_vector()`/`unit_vector()` (`:1774-1787, 1852-1863`) guard — two contracts, no documentation. `angle_between` (`:1808-1811`) feeds an unclamped quotient to `acos`. `EricsonMath.cpp:729-743` (`barycentric`), `:756-767` (`closest_pt_point_segment`) and `:348` divide by degenerate denominators; the downstream comparisons are all false for NaN, so a degenerate triangle silently claims every point is outside it. `Partitioner.cpp:4-22` divides by `num_partitions` with no validation. `StructurePaintable.cpp:108-116` divides by a truncated tile count that is 0 for any structure under 4 units, and applies `fabs` to the count but not the divisor. Guard each and return a documented fallback. Note `/fp:fast` is set in all four configs (`ArtAttack.vcxproj:94,116,139,165`), which licenses the compiler to assume no NaNs — so "silently false" isn't even reproducible across optimisation levels.
- **Validate-after-mutate.** All six `RectangleRotated` setters (`MattMath.cpp:4295-4413`) and all four `Quad::set_point_N` (`:3883-3921`) write the member, then validate, then throw — leaving `_points` inconsistent with the extents, with no exception guarantee at all. `set_x_axis(ZERO)` writes NaN and *then* throws. Compute into a local, validate, commit.
- **Invalid-by-construction defaults.** `Quad() = default` (`MattMath.h:922`) and `RectangleRotated() = default` (`:1049`) bypass every invariant; a default-constructed OBB makes the const query `contains()` throw from inside the collision loop. Delete them or give them valid states.
- **Missing invariants and tolerances.** `RectangleF` has no width/height ≥ 0 check and `from_top_left_bottom_right` (`:1122-1128`) builds inverted rects straight from level JSON (`LevelObjectBuilder.cpp:45-51, 88-94`) — every predicate then silently returns the wrong answer. `EPSILON = 0.0001f` (`MattMath.h:46`) is absolute, and one float ULP at the levels' 6000-unit coordinates is ~4.9e-4, so `are_equal` is harsher than "equal after one rounding step"; a second `EPSILON` 100× tighter is visible in the same TU (`EricsonMath.h:75`). Add a relative comparison and name the constants for what they measure.

### 5. Tests — the new code shipped without coverage of its own primitives

**Two OBB functions cannot succeed for any input, and no test noticed.** `EricsonMath.cpp:769-786` — `closest_pt_point_OBB` loops `i < 3` over a 2D box, and `RectangleRotated::get_axis` (`MattMath.cpp:4315-4327`) throws for anything but 0 or 1, so the third iteration throws unconditionally. `MattMath.cpp:4201-4215` — `RectangleRotated(const Segment&, float)` assigns `_x_axis = center_line.get_direction()` **un-normalised** while the sibling constructor at `:4191` normalises, so `axes_valid()` rejects any centre line not of length 1 and the constructor always throws. That is the constructor for thick line segments, i.e. the reason OBBs were added. Both have zero tests. Related: `is_valid()` is a no-op during construction (`:4194` validates before `_points` is assigned at `:4199`, so `edges_valid()` compares four zero-length edges and always passes).

**`CollisionTools` has 2 tests for 11 functions, and the untested branches are wrong.** `MattMathTests/CollisionToolsTests.cpp:15-99`. `resolve_object_AABB_collision` (`CollisionTools.cpp:209-229`) uses signed `std::max` on the direction — for `(-0.9, -0.1)` the divisor is `-0.1` and the push-out becomes 9× the overlap; the two existing tests use the only two inputs for which that is harmless. `calculate_containing_collision_direction` (`:91-107`) has its `<` inverted. `calculate_object_collision_direction_by_edge` (12 branches) and `bracket_object_collision_generic` (used for every non-rect resolution) have zero tests. And the one ramp regression test — the exact case the recent commits were about — is **commented out** at `CollisionToolsTests.cpp:82-98` rather than fixed.

**Untested trunk and untested failure modes.** `Shape::intersects` / `AABB_intersects` and the four free wrappers (`MattMath.cpp:119-175`) — the layer every in-game collision actually goes through — have no tests; the suite calls the concrete free functions directly. `grep ExpectException MattMathTests/` returns zero hits, and every shape in every test is well-formed, so none of the divide-by-zero paths above is pinned. `Camera` (`MattMath.h:1007-1045`), the whole matrix library, `TriangleRightAxisAligned`, `Colour`, `Vector2I`, `RectangleI` and `Viewport` have no coverage of any kind — including `RectangleF::intersection`, which every rect-vs-rect push-out depends on. `Colour::saturate` and `Colour::desaturate` (`MattMath.cpp:2385-2404`) are character-for-character identical; no Colour test exists to notice. `intersect_moving_AABB_AABB` (`EricsonMath.cpp:535-640`) — 105 lines of swept collision, half of it commented-out 3D source — has no callers and no tests: either test it and adopt it to fix tunnelling, or delete it.

**Highest-value additions, in order:** (a) a `Shape*` matrix test over all 25 ordered type pairs asserting symmetry — a strong oracle needing no hand-derived expected values; (b) a table-driven `CollisionTools` test over shape-pair × 8 approach directions asserting the resolved shape no longer intersects and the sign/magnitude of `amount`; (c) a `DegenerateInputTests` class, one test per primitive, pinning throw-or-value for zero-length segments, collinear triangles, zero-radius circles and negative-extent rectangles.

### 6. Build guardrails that would have caught much of this

`/W4` is on in all four configs but `TreatWarningAsError` appears in neither vcxproj, so real warnings sit in shipped code: unused locals `collider_aabb` (`CollisionTools.cpp:35, 58`) and `num_threads` (`Level.cpp:599`) are C4189; `int i < vec.size()` (`Level.cpp:386`, `GameStates.cpp:321`, `StructurePaintable.cpp:189`) is C4018; the four dead `catch (const std::out_of_range& e)` blocks in `SoundBank.cpp` bind an unused `e` (C4101); the missing `default:` at `MainMenu.cpp:1264-1288` (whose two sibling functions both have one) is C4062. Turn on `/we4189 /we4100 /we4018 /we4062` at minimum. Set `<LanguageStandard>` explicitly (C++17 unlocks `inline constexpr`, `std::string_view`, `std::filesystem`, `std::optional` — all of which fix findings above). Reconsider `/fp:fast` while the library still has unguarded NaN paths. And trim `pch.h:59-77`, which force-includes 13 unused DirectXTK headers (`Model.h`, `Effects.h`, `PostProcess.h`…) into all 81 TUs while omitting `<string>`/`<vector>`/`<map>`, which 34/28 of them actually use.

### If you only do three things

1. **Add the two exception barriers**: a `try`/`catch` in `ThreadPool::work_callback` that captures an `exception_ptr` and rethrows from `wait_for_tasks_to_complete()`, and a `try`/`catch` around the message loop in `Main.cpp`. Then make out-of-bounds objects despawn instead of throw (`Level.cpp:307-321`). This turns a dozen silent process kills into diagnosable errors.
2. **Fix the data pipeline at the boundary**: one loader that null-checks `fopen`, checks `HasParseError()`, and validates every field's presence and type with a message naming the file and field; change all four `ResourceManager` getters from `operator[]` to `find()` + a throw naming the missing key; make `SpriteSheet::draw` do the same. Then commit or document how to obtain the audio assets, because the repo currently does not run from a clean clone.
3. **Stop the per-frame string lookups and cull the paint tiles**: cache `SpriteSheet*`/`AnimationStrip*`/`SoundEffectInstance*` at construction, and replace the `std::vector<PaintTile>` grid with a flat team-id array plus an active-splash list. Those two changes remove tens of thousands of tree walks and virtual calls per frame on the shipped levels — more than everything else in this section combined.

---

## High-severity index (205)

1. `.gitignore:365-368` — All audio assets are gitignored, so a fresh clone cannot run the game
2. `ArtAttack/AnimatedSprite.h:1-49` — AnimatedSprite is entirely dead code and a line-for-line fork of AnimationObject — including both of its bugs
3. `ArtAttack/ArtAttack.vcxproj:86-180` — No <LanguageStandard> anywhere: the whole engine is stuck on /std:c++14
4. `ArtAttack/ArtAttack.vcxproj:91,113,134,160` — /W4 with no TreatWarningAsError, and the test project silently compiles at /W3 with -permissive- off
5. `ArtAttack/CameraTools.cpp:74-91` — Camera scroll border clamps min above max, inverting the dead zone at the shipped default resolution
6. `ArtAttack/collision_object_type.h:4-29` — Collision identity is a cross-product of game class x team x weapon, welded into the engine's collision interface
7. `ArtAttack/collision_object_type.h:31-73` — Collision-type predicates are written and never called, while call sites re-implement them as shadowing locals
8. `ArtAttack/CollisionTools.cpp:32-69, 249-260` — Bisection push-out can silently fail to separate, and resolve_object_collision reports success unconditionally
9. `ArtAttack/CollisionTools.cpp:32-69, 244-254, 270-281` — Collision response is an iterative bisection that mutates geometry; there is no contact/manifold concept anywhere
10. `ArtAttack/CollisionTools.cpp:109-134` — "Collision direction" is a centre-to-centre unit vector, not a surface normal, and callers snap it to eight compass points
11. `ArtAttack/CollisionTools.cpp:176-187` — Band cases in calculate_object_collision_direction_by_edge resolve on the wrong axis
12. `ArtAttack/Colour.h:1-469` — Colour.h contains no Colour type - only `namespace colour_consts` - and breaks the file-naming rule it sits next to
13. `ArtAttack/Colour.h:309-313` — Splatoon team colours are baked into the engine's colour palette header
14. `ArtAttack/Colour.h:10-313` — 302 internal-linkage objects defined in a header, every Colour constructed by parsing a hex string at static-init time
15. `ArtAttack/Colour.h:10-157,160-307,315-466` — Colour.h duplicates 302 dynamically-initialised const objects and a 152-line function into each of 58 TUs
16. `ArtAttack/DebugText.h:4-33` — DebugText, an engine-level overlay, includes Player.h and hardcodes knowledge of ~18 game-specific fields
17. `ArtAttack/DeviceResources.cpp:418-453, 716-730` — HandleDeviceLost never touches the deferred-context pool, so restore builds SpriteBatches on contexts of a destroyed device
18. `ArtAttack/DeviceResources.cpp:716-745` — Deferred contexts are raw AddRef'd COM pointers that are never released — 16 leaked contexts, rule-of-5 violation on vendor code
19. `ArtAttack/DeviceResources.cpp:737-745` — get_deferred_context is declared noexcept but throws std::out_of_range — the bounds check is a guaranteed std::terminate
20. `ArtAttack/DeviceResources.h:86-88` — Raw D3D11 contexts and command lists are the public draw API, and the record/execute/Release protocol is copy-pasted
21. `ArtAttack/DeviceResources.h:88` — get_deferred_context is declared noexcept but throws — guaranteed std::terminate, called from a worker thread
22. `ArtAttack/DrawObject.h:1-43` — The render hierarchy is inheritance where composition belongs, and Player ends up with two complete copies of all render state
23. `ArtAttack/DrawObject.h:1-6` — Including DrawObject.h drags RapidJSON, XAudio2, SoundBank and the game's level_stage enum into every renderable
24. `ArtAttack/DrawObject.h:20` — const accessors hand out a mutable ResourceManager*, so the entire const-marked draw path performs writes to shared containers
25. `ArtAttack/Game.cpp:75-98` — Controller polling lives inside the fixed-step update, so sampling rate is welded to 60 Hz and varies 0..6 per displayed frame
26. `ArtAttack/Game.cpp:75-98, 157-184` — Game keeps simulating, presenting and playing audio while unfocused; while minimised it spins at unbounded frame rate
27. `ArtAttack/Game.cpp:93-97` — Audio device loss is explicitly caught and discarded: game is permanently silent after any default-device change
28. `ArtAttack/Game.cpp:124-152` — clear() records into all 16 deferred contexts every frame; 12 of them are never flushed, so their command buffers grow unbounded
29. `ArtAttack/Game.cpp:178-184` — ResetElapsedTime() exists at exactly one site; every real stall path (level load, device restore, drag) is missing it
30. `ArtAttack/Game.cpp:264-267, 197-205` — create_window_size_dependent_resources is an empty stub, so any window resize desyncs viewports from the backbuffer
31. `ArtAttack/Game.cpp:218-261, 282-287` — OnDeviceRestored rebuilds ThreadPool / ResourceManager / ResourceLoader / _dt, leaving every cached raw pointer in the live scene dangling
32. `ArtAttack/Game.cpp:218-261` — Device restore re-creates every engine service while live objects hold stale raw pointers to the old ones
33. `ArtAttack/Game.cpp:269-280` — OnDeviceLost frees SpriteBatches and CommonStates but leaves the raw-pointer mirrors GameData published
34. `ArtAttack/Game.h:16-76` — Game is a god object: platform host, device-lost sink, root state machine and owner of twelve unrelated subsystems
35. `ArtAttack/Game.h:16, 59-76` — Game frees every service before its StateContext base destroys the State that points at them
36. `ArtAttack/Game.h:16, 59-76` — Game inherits StateContext, so subsystems die before the live state tree — and the audio engine dies before the sounds it owns
37. `ArtAttack/Game.h:16-76` — The root State is owned by a base class, so on app exit it outlives every service it borrows; Game::_state is dead code
38. `ArtAttack/Game.h:16-76` — Destruction order is inverted: the live State outlives every Game member, and the ThreadPool is destroyed after the SpriteBatches its tasks use
39. `ArtAttack/Game.h:66` — Frame delta is a heap-allocated unique_ptr<float> whose raw pointer is threaded through eleven classes
40. `ArtAttack/GameData.h:13-62` — GameData is a 13-pointer service locator that is also inherited as a base by five game-specific data bags, with no virtual destructor
41. `ArtAttack/GameStates.cpp:105-127` — No exception handler exists anywhere on the path; a malformed level file or an out-of-bounds object terminates the process
42. `ArtAttack/GameStates.cpp:113-127` — GameLevel::init captures device-dependent raw pointers for the whole life of the level and never refreshes them on device restore
43. `ArtAttack/GameStates.cpp:128-142` — FIRST_UPDATE/SECOND_UPDATE is a hand-rolled workaround for the missing timer reset, and it under-shoots
44. `ArtAttack/GameStates.cpp:303-346` — GameLevel::draw does raw D3D11 command-list submission — GPU backend code inside a flow-control State
45. `ArtAttack/GameStates.h:1-12` — The state layer is bidirectionally coupled to every concrete screen; GameStates.h compiles the whole game
46. `ArtAttack/GameStates.h:55-70` — GameLevel declares every menu's data object after the state that points at it, so data dies first
47. `ArtAttack/ICollisionGameObject.h:12-13` — is_colliding lives on the interface, so every object implements the whole narrow phase — and the relation is not symmetric
48. `ArtAttack/ICollisionGameObject.h:14` — Splatoon rules are welded into the engine's core collision interface via collision_object_type
49. `ArtAttack/ICollisionGameObject.h:16` — Boolean accessors split between `get_x()` and `is_x()` with no rule; the worst offender is on a core engine interface
50. `ArtAttack/IGameObject.h:1-16` — 40 headers are not self-contained: they name DirectX/Win32 types with no include, and compile only because /Yu force-injects pch.h
51. `ArtAttack/IGameObject.h:11-12` — IGameObject hardwires DirectX::SpriteBatch and MattMath::Camera into the root interface of every object in the engine
52. `ArtAttack/IGameObject.h:10-13` — Every game object in the engine is compiled against DirectXTK's SpriteBatch and MattMath::Camera
53. `ArtAttack/InterfaceGameplay.cpp:15-47` — No rule about who owns SpriteBatch Begin/End — four incompatible conventions across the rendering code
54. `ArtAttack/InterfaceGameplay.cpp:123-142` — Health bar fill width is never clamped and player health is only clamped at the top, so a dead player renders an inverted destination RECT
55. `ArtAttack/InterfaceGameplay.h:10-79` — InterfaceGameplay is a Splatoon HUD welded into what is presented as an engine class
56. `ArtAttack/IPaintableGameObject.h:1-17` — An interface named `IPaintableGameObject` with a hardcoded two-team score sits alongside the engine's core interfaces
57. `ArtAttack/IPaintableGameObject.h:4-15` — Splatoon paint scoring and a fixed two-team model are baked into an engine-level interface
58. `ArtAttack/Level.cpp:61-64` — Quit-to-main-menu from the pause menu leaves weapon loop sounds playing forever
59. `ArtAttack/Level.cpp:221-249` — "Still on the ground" is inferred from any structure overlap, so wall and jump-through contact suppresses on_no_collision()
60. `ArtAttack/Level.cpp:221-293` — O(n²) collision inlined into Level with no broad phase, and object-object pairs visited in both orderings
61. `ArtAttack/Level.cpp:307-321` — An object leaving the play area throws an unhandled exception instead of being despawned
62. `ArtAttack/Level.cpp:307-321` — Out-of-bounds objects throw instead of being despawned, turning a routine event into a process abort
63. `ArtAttack/Level.cpp:491-593` — Split-screen is hardwired: render-slot index == player index, and there is no View concept
64. `ArtAttack/Level.cpp:496-540` — Multithreaded draw calls non-const draw() on shared objects — concurrent writes to the same PaintTile
65. `ArtAttack/Level.cpp:498-501` — Exceptions thrown inside thread-pool draw tasks escape a Win32 callback and terminate the process
66. `ArtAttack/Level.cpp:619-620` — Zoom-out draw permanently forces ONE_PLAYER layout on the shared ViewportManager; Restart then renders every player fullscreen
67. `ArtAttack/Level.h:54-179` — Level is a god class: object storage, simulation, collision, camera, HUD, audio, match rules and the D3D11 render backend
68. `ArtAttack/Level.h:58-80` — 23-parameter constructor, five of whose members are written and never read
69. `ArtAttack/Level.h:83-85` — Level's public draw API is three raw pointers to vectors of D3D11 objects
70. `ArtAttack/Level.h:107-119` — Level caches raw SoundBank* and ResourceManager* that device-loss destroys and replaces
71. `ArtAttack/Level.h:116, 136-137, 143` — Level has four write-only members, including a complete spawn-point pipeline that terminates in nothing
72. `ArtAttack/LevelEndInfo.cpp:96-128` — winning_ratio divides winner by loser (not by total): yields >100%, negative, inf and NaN percentages
73. `ArtAttack/LevelLoadedInfo.cpp:15-94` — No JSON access anywhere is guarded: zero HasParseError checks, zero type checks, ~60 raw operator[] + GetFloat/GetString
74. `ArtAttack/LevelLoadedInfo.h:5,34` — rapidjson/document.h is parsed by 60 of 81 TUs because a JSON Document is stored by value in a header
75. `ArtAttack/LevelLoadedInfo.h:17-18, 36-37` — The level file format itself only has room for exactly two teams
76. `ArtAttack/LevelObjectBuilder.cpp:15-180, 182-200` — Level loading is a closed if/else over four hardcoded type strings — no registration point
77. `ArtAttack/LevelObjectBuilder.cpp:19-199` — Every level JSON field is read with unchecked rapidjson accessors — missing or mistyped fields are UB in Release
78. `ArtAttack/Main.cpp:6, 208 (and 27, 123)` — The platform entry point includes the game's state header and constructs `GameMenu` directly
79. `ArtAttack/Main.cpp:28, 123` — App identity is two renames stale: window titled "Colour Wars", class "ChromaClashWindowClass"
80. `ArtAttack/Main.cpp:84-231` — wWinMain has no try/catch, so any asset, save or D3D failure aborts with no diagnostic
81. `ArtAttack/Main.cpp:146-165, 202` — AdjustWindowRect and GetClientRect results are both computed and discarded; the backbuffer is sized from the save file
82. `ArtAttack/Main.cpp:156, 189-196` — Borderless-fullscreen window is WS_EX_TOPMOST and never drops topmost when deactivated
83. `ArtAttack/Main.cpp:211-224` — No exception handler anywhere on the frame loop, while ~58 sites throw as ordinary control flow
84. `ArtAttack/MainMenu.cpp:36-46` — draw() is byte-identical in all twelve page classes, as are the container members and the init() scaling tail
85. `ArtAttack/MainMenu.cpp:47-61` — Zero controllers connected makes the entire application inert and unexitable from within - there is no keyboard or mouse path anywhere
86. `ArtAttack/MainMenu.cpp:131-203` — Menu navigation and activation are implemented as ~90 string comparisons against widget names
87. `ArtAttack/MainMenu.cpp:167, 334-340, 427-451` — Window and display-mode management lives inside a menu screen, duplicating Main.cpp and bypassing ExitGame()
88. `ArtAttack/MainMenu.cpp:318-353, 427-451` — Options 'Apply' has a dead SetWindowPos, discards the chosen resolution in fullscreen, and sizes the window rect instead of the client rect
89. `ArtAttack/MainMenu.cpp:318-352, 427-451` — Options apply path: dead SetWindowPos, no re-derivation of render size, and fullscreen guarantees a viewport/backbuffer mismatch
90. `ArtAttack/MainMenu.cpp:427-451` — A menu page performs raw Win32 window management and duplicates the window-creation logic from Main.cpp
91. `ArtAttack/MainMenu.cpp:939-950, 1160, 1236-1246` — Choosing more players than connected controllers makes team/weapon select impossible to complete
92. `ArtAttack/MainMenu.cpp:1154-1246` — Team select cannot be completed when the chosen player count exceeds the number of connected pads
93. `ArtAttack/MainMenu.cpp:1443-1463` — No navigation stack: every page hardcodes its predecessor as a concrete sibling class
94. `ArtAttack/MainMenu.cpp:1816-1820` — "Random" stage and "Random" weapon are resolved once and baked into MenuLevelSettings, so rematch is never random
95. `ArtAttack/MainMenu.cpp:1577-1626, 1736-1755, 1852-1883` — Game content (weapon, stage and mode catalogues) is hardcoded in switch statements and header string constants
96. `ArtAttack/MattMath.cpp:119-175` — The Shape dispatch layer every in-game collision goes through has zero tests
97. `ArtAttack/MattMath.cpp:381-407` — triangles_intersect only tests 4 of the 9 edge pairs — two triangles can cross and be reported as disjoint
98. `ArtAttack/MattMath.cpp:611-617, 4265-4268, 4450-4455` — Point-in-OBB does 3 heap allocations, a full Quad validity check and 2 barycentric solves instead of 2 dot products
99. `ArtAttack/MattMath.cpp:619-661` — RectangleRotated exposes a full OBB interface (axes, half-extents, indexed accessors) that no intersection routine uses; the OBB path is brute-force point/edge enumeration instead of SAT
100. `ArtAttack/MattMath.cpp:808-818` — RectangleF::get_edges() returns {top, bottom, left, right} while its only consumer indexes it as clockwise
101. `ArtAttack/MattMath.cpp:808-818` — Shape::get_edges() has no documented ordering and RectangleF's differs from every other shape's; StructurePaintable indexes it as if it didn't
102. `ArtAttack/MattMath.cpp:2142-2183, 2476-3050` — An entire unused linear-algebra sublibrary (MatrixF / Matrix3x3F / Vector4F) lives in the core math header
103. `ArtAttack/MattMath.cpp:2478-2513, 2995-3048` — MatrixF(int, int) never sizes _elements, so every matrix factory writes out of bounds on an empty vector
104. `ArtAttack/MattMath.cpp:3512-3527` — Triangle::get_angle_N returns the supplement of the interior angle
105. `ArtAttack/MattMath.cpp:3633-3652, 3665-3684` — find_hypotenuse returns a VERTEX index that both callers use as an EDGE index, so get_hypotenuse() returns a leg
106. `ArtAttack/MattMath.cpp:4156-4169` — calculate_camera_from_view_rectangle is not the inverse of calculate_view_rectangle; its parameters are named backwards and its divisor is unguarded
107. `ArtAttack/MattMath.cpp:4186-4200, 4464-4480, 4539-4565` — RectangleRotated::is_valid() is a no-op during construction and no test exercises the invalid path
108. `ArtAttack/MattMath.cpp:4201-4215` — RectangleRotated(Segment, thickness) never normalises its axes, so it throws for every segment whose length != 1
109. `ArtAttack/MattMath.h:1-1139` — MattMath.h is a god header spanning pure math, collision, colour, D3D11 and camera — and hard-depends on d3d11.h
110. `ArtAttack/MattMath.h:1-1139` — MattMath is a god module: shapes, colour, a general dynamic matrix, viewport and camera in one 1139-line header / 4568-line TU
111. `ArtAttack/MattMath.h:4-8, 767-806` — The geometry library #includes d3d11.h and DirectXTK, hard-binding a renderer-agnostic module to D3D11
112. `ArtAttack/MattMath.h:68-89, 184-301` — RectangleF — the engine's universal value type and AABB — inherits polymorphic Shape
113. `ArtAttack/MattMath.h:68-89` — Shape is an O(N^2) fat interface: adding a primitive forces edits to every existing shape
114. `ArtAttack/MattMath.h:87` — Shape::get_edges/get_points/get_triangles return std::vector by value, forcing a heap allocation per narrow-phase call
115. `ArtAttack/MattMath.h:88` — Shape::inflate(float) is one virtual with three mutually incompatible geometric meanings and three different failure policies
116. `ArtAttack/MattMath.h:684-765, 1007-1045, 586-645, 354-400, 508-565, 767-806, 895-914` — Nine public types in MattMath.h have literally zero tests, including Camera and the entire matrix library
117. `ArtAttack/MattMath.h:922, 1049` — Quad() and RectangleRotated() = default bypass every invariant, and a default-constructed OBB makes contains() throw from inside the collision loop
118. `ArtAttack/MattMath.h:1080-1082` — set_x_axis / set_y_axis can never actually rotate a RectangleRotated -- they validate the new axis against the unchanged other axis
119. `ArtAttack/MenuData.h:7-17` — Menu data classes inherit a copy of the whole GameData service locator, and publish results by writing through raw out-pointers
120. `ArtAttack/MenuInput.cpp:131-150` — MenuInput returns a compacted vector, so the input index silently stops matching the player index
121. `ArtAttack/MenuPage.cpp:94-121, 140-150` — Menu drawing throws from inside Win32 thread-pool callbacks, which terminates the process
122. `ArtAttack/MenuPage.cpp:99-115` — Menu pages draw once per viewport of a stale screen_layout, tiling the main menu after a multiplayer quit
123. `ArtAttack/MenuPage.h:38-53` — MenuPage — the base class for all UI — is a D3D11 command-list orchestrator
124. `ArtAttack/Mh.cpp:162-165` — MTexture::is_visible_in_viewport calls itself — unconditional infinite recursion
125. `ArtAttack/Mh.h:1-167` — `Mh.h` and its `M`-prefixed classes are unguessable; the prefix even yields the word "mobject" in API names
126. `ArtAttack/Mh.h:31-55` — MContainer stores raw non-owning children with no ownership contract, and every menu declares containers before the widgets they point at
127. `ArtAttack/MovingObject.h:33-47, 55` — `_dx` is a 2D vector, producing the nonsense accessors `get_dx_x()` / `set_dx_x()` / `get_dx_magnitude()`
128. `ArtAttack/PaintTile.cpp:34-53` — Splash animation is drawn forever after it finishes, permanently doubling sprite count and leaving a 24x24 team-coloured blob on every painted tile
129. `ArtAttack/pch.h:42-77` — PCH includes 13 DirectXTK headers that are never used, and omits <string>/<vector>/<map> which almost everything uses
130. `ArtAttack/player_consts.h:65-69` — Two divergent projectile-damage tables; the ProjectileDetails one is dead code
131. `ArtAttack/Player.cpp:44` — Player caches a raw SoundBank* that Game::OnDeviceLost destroys underneath it
132. `ArtAttack/Player.cpp:47-90` — draw() is a mutating operation on shared objects, and the same object is drawn concurrently by every viewport thread
133. `ArtAttack/Player.cpp:148-184` — The same 32-line is_colliding narrow-phase boilerplate is copy-pasted into 7 classes
134. `ArtAttack/Player.cpp:274-277` — STRUCTURE_RAMP_LEFT collision branch is an empty // TODO — left ramps are silently non-solid
135. `ArtAttack/Player.cpp:696-750` — Dead players keep firing: the weapon is never gated on player_state
136. `ArtAttack/Player.h:63-64` — Player inherits SpriteSheetObject/DrawObject twice (non-virtual diamond) — two divergent copies of all draw state
137. `ArtAttack/Player.h:63-252` — Player is a god class: simulation, rendering, audio, camera, input, weapon and game rules in one type
138. `ArtAttack/PlayerBuilder.cpp:23-37, 64` — Spawn points chosen with the global player index into per-team spawn arrays; unchecked operator[] on JSON data
139. `ArtAttack/PlayerBuilder.cpp:26-60` — A player left on 'no team' in team select spawns at team B's point and then throws during level build
140. `ArtAttack/PlayerInput.cpp:119-138` — update_and_get_player_inputs returns a positionally compacted vector that Level indexes by player ordinal
141. `ArtAttack/PlayerInput.cpp:119-138` — Input vectors are compacted by connection state, destroying the pad-slot to player mapping; the connection_state field that exists to prevent this is dead
142. `ArtAttack/PlayerInput.h:34` — Edge-detection history is per-consumer and only advances while that consumer is the active state
143. `ArtAttack/PlayerInputData.h:7-23 (with PlayerInput.cpp:20-36, 119-137)` — The input layer emits this game's verbs directly, with a fixed XInput button map and no keyboard
144. `ArtAttack/Projectile.cpp:93-119` — Projectile's hand-rolled structure filter omits both ramp types, so projectiles fly through every ramp in the shipped level
145. `ArtAttack/Projectile.cpp:93-119` — Each object hardcodes its own slice of the collision matrix, and the resulting relation is asymmetric
146. `ArtAttack/Projectile.cpp:120-154` — A projectile's only hit response is a bool; its actual effect is implemented by whatever it hits
147. `ArtAttack/Projectile.cpp:155-195` — collision_object_type is a team x weapon cross-product enum, re-expanded by hand in six places
148. `ArtAttack/Projectile.h:4, 24-36, 70-79` — Splatoon team rules are baked into the projectile base class, blocking the engine-reuse goal
149. `ArtAttack/Projectile.h:10-16` — `enum projectile_type` is the codebase's only unscoped enum and dumps SPRAY/JET/ROLLING/MIST/BALL into the global namespace
150. `ArtAttack/ProjectileBall.h:22-31` — The collision shape lives in the leaves, so five subclasses exist only to re-declare the same RectangleF
151. `ArtAttack/ProjectileBuilder.cpp:23-113` — Projectiles are class-per-variant behind a switch factory, while the rest of the engine is already data-driven
152. `ArtAttack/ProjectileMist.cpp:19-22` — ProjectileMist constructs its base with `SPRAY` as its projectile_type
153. `ArtAttack/ProjectileRolling.cpp:29-47` — ProjectileRolling never applies its displacement and never advances its animation — the roller emits stationary invisible hitboxes
154. `ArtAttack/ProjectileSpray.cpp:8-91` — Five Projectile subclasses are ~90% byte-identical; the whole family should be one class + a data table
155. `ArtAttack/ProjectileSpray.cpp:56-87` — The same 32-line is_colliding() is copy-pasted into all five projectile leaves (and again into Player and PaintTile)
156. `ArtAttack/ResourceLoader.cpp:17-51` — The engine's resource loader hardcodes this one game's entire asset manifest, including its game-mode enum
157. `ArtAttack/ResourceLoader.cpp:17-51` — The content manifest is hardcoded C++ inside the engine's loader; adding a level means editing engine code
158. `ArtAttack/ResourceManager.cpp:7-102` — All four ResourceManager getters use map::operator[] wrapped in a catch that can never fire, so a missing asset returns nullptr and every render call site dereferences it
159. `ArtAttack/ResourceManager.cpp:7-58` — Resource "getters" use map::operator[] — mutating lookup from render threads, dead catch blocks, silent nullptr on typo
160. `ArtAttack/ResourceManager.cpp:46-58` — Mutating map lookups run concurrently on thread-pool workers during parallel draw — genuine data race
161. `ArtAttack/ResourceManager.cpp:104-126` — reset_all_* null out the mapped values but keep the keys, turning "released" into a silent nullptr, and dangle every cached SoundBank*
162. `ArtAttack/ResourceManager.cpp:104-126` — reset_all_* nulls map entries instead of erasing them, leaving booby-trapped keys that lookups happily return
163. `ArtAttack/ResourceManager.h:7, 32-35, 52` — The game-mode enum `level_stage` is baked into the engine's asset cache as a map key type
164. `ArtAttack/ResourceManager.h:7, 32-34, 52 (with ResourceLoader.cpp:17-51)` — The resource cache is keyed on this game's three map names, and the asset manifest is compiled C++
165. `ArtAttack/ResultsMenu.cpp:225-261` — Results fill bars are sized in 1920x1080 design units after the container was already scaled
166. `ArtAttack/SoundBank.cpp:110-141` — A wave name typo in sound_bank_1.json stores a null SoundEffectInstance that crashes later at play time
167. `ArtAttack/SoundBank.cpp:150-172` — fopen result unchecked and ParseStream result never inspected — a missing or malformed asset is UB in Release, not an error
168. `ArtAttack/sounds/sound_bank_1/cmd.txt:1-8` — Three third-party tool EXEs and their cmd.txt notes are committed, while the audio assets they produce are gitignored — the app cannot run from a clone
169. `ArtAttack/SpriteSheet.cpp:92-132` — Both name-keyed draw overloads look frames up with map::operator[], so a typo draws nothing silently and the overloads cannot be const
170. `ArtAttack/SpriteSheet.cpp:105` — A misspelled frame_name in level JSON silently draws a 0x0 sprite instead of failing
171. `ArtAttack/SpriteSheet.h:10-69` — SpriteSheet is three classes in one: atlas data, JSON loader, and sprite renderer
172. `ArtAttack/SpriteSheet.h:63` — SpriteSheet stores a borrowed ID3D11ShaderResourceView* with no AddRef that ResourceManager releases underneath it
173. `ArtAttack/SpriteSheetObject.cpp:31-35` — Renderables store resource names, so every draw costs 2-3 string-keyed std::map lookups through a chain of virtual getters
174. `ArtAttack/StateContext.cpp:20-25` — transition_to destroys the running state from inside its own update(); correctness rests on an unwritten 'return immediately' rule
175. `ArtAttack/StateContext.cpp:20-25` — transition_to destroys the State whose update() is still on the stack — `delete this` at ~30 call sites
176. `ArtAttack/StateContext.h:7-17` — StateContext holds one state with no stack, forcing GameLevel to hand-roll overlay machinery three times
177. `ArtAttack/StructurePaintable.cpp:45-52` — Paint tiles are never viewport-culled — thousands of sprites submitted per viewport per frame
178. `ArtAttack/StructurePaintable.cpp:188-213` — Paintable-face flags are permuted: edge index map disagrees with RectangleF::get_edges() ordering
179. `ArtAttack/StructurePaintable.h:56` — StructurePaintable stores a reference member bound to a caller's local vector — dangles for the object's whole life
180. `ArtAttack/TeamColour.h:5` — Three `#include` directives use the wrong case and only compile because of Windows' case-insensitive filesystem
181. `ArtAttack/TextDropShadow.cpp:28-48` — TextDropShadow::draw renders by mutating and restoring its own base state, on an object shared across all render threads
182. `ArtAttack/TextDropShadow.h:24-25` — TextDropShadow::draw hides rather than overrides TextObject::draw — the const qualifier differs and no override keyword catches it
183. `ArtAttack/ThreadPool.cpp:38-58` — add_task is not exception-safe and there is no RAII drain, so a mid-loop throw abandons tasks pointing at the caller's stack
184. `ArtAttack/ThreadPool.h:9-31` — ThreadPool owns raw Win32 handles and declares a destructor but does not delete copy — the implicit copy ctor double-closes every handle
185. `ArtAttack/ThreadPool.h:9-31` — ThreadPool owns raw OS handles with a user destructor but implicitly-defaulted copy/move — double-free waiting to happen
186. `ArtAttack/ViewportManager.cpp:63-70` — apply_player_viewport(int) mutates the immediate context from a thread-pool worker and configures a batch belonging to a different context
187. `ArtAttack/ViewportManager.cpp:90-104` — get_all_viewports() asks for viewport index 4 in a 3-player layout, so every menu draws a spurious 4th fullscreen copy
188. `ArtAttack/ViewportManager.cpp:154-245` — calculate_viewport's layout cases fall through — an out-of-range player index silently gets another layout's geometry
189. `ArtAttack/ViewportManager.h:10-16 (consumed at LevelObjectBuilder.cpp:236-241)` — The engine's split-screen viewport manager hardcodes this game's texture atlas and frame names
190. `ArtAttack/weapon_consts.h:14-168` — All weapon tuning is a compile-time C++ table in a header while the rest of the content pipeline is data-driven JSON
191. `ArtAttack/weapon_consts.h:41, 91` — Fire rate is quantised to the tick rate (shoot_interval 0.0f / 0.0001f) and nothing caps live projectiles feeding an all-pairs collision loop
192. `ArtAttack/Weapon.cpp:32-34` — _sound_bank comes from a lookup that silently returns nullptr on a bad name and is then dereferenced unguarded every frame
193. `ArtAttack/Weapon.cpp:188-235` — Dead players keep firing, burning ammo and looping the shoot sound from the corpse
194. `ArtAttack/Weapon.cpp:236-257` — Looping shoot sound is only ever stopped from inside the per-frame update, so it survives pause and outlives the weapon
195. `ArtAttack/Weapon.cpp:258-277` — The base class switches on wep_type to name sounds, so Weapon knows the full list of its own subclasses
196. `ArtAttack/Weapon.h:14-143` — Weapon is a god class welded to DirectX and to Splatoon's team/player concepts - the core obstacle to engine reuse
197. `MattMathTests/CollisionToolsTests.cpp:15-99` — 2 tests cover 2 of CollisionTools' 11 functions; every buggy branch is untested
198. `MattMathTests/CollisionToolsTests.cpp:82-98` — The one regression test for ramp (rect-vs-triangle) collision is commented out, in the module being actively fixed
199. `MattMathTests/MathTests.cpp:1-876` — No degenerate, NaN, or exception-path testing anywhere in the suite
200. `MattMathTests/MathTests.cpp:5-6` — Tests #include .cpp files, and the two test TUs are implicitly linked to each other
201. `MattMathTests/MathTests.cpp:14-16, 335, 393, 658, 762, 816, 866` — Three ad-hoc epsilon constants tuned until the tests went green, at resolutions finer than the library's own EPSILON
202. `MattMathTests/MathTests.cpp:34, 39, 44, 86, 107, 131, 229, 437, 523` — Geometric results compared with exact float equality via Vector2F::operator==
203. `MattMathTests/MathTests.cpp:51-59` — FLT_EPSILON boundary cases are arithmetic no-ops: "just inside" and "just outside" are the same point
204. `MattMathTests/MathTests.cpp:278-281` — Two negative boundary cases build their input and then assert nothing
205. `MattMathTests/MattMathTests.vcxproj:91-158` — Tests compile the code under test with different codegen than the game ships with
