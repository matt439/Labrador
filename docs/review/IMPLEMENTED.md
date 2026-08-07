# Review remediation status

Tracks what has been fixed from the review in [README.md](README.md), and what has not.
Last updated 2026-08-08.

**Critical findings: 31 of 36 fixed, 1 partial, 4 outstanding.**

The four outstanding ones are the structural items the review itself scopes at
weeks-to-months (`#1`, `#2`, `#13`, `#18`). Everything reachable without the
engine/game split has been done.

| Commit | Scope |
|---|---|
| `73da0a2` | Memory safety, throwing teardown paths, MattMath validation |
| `0c7eaff` | Input identity, paintable faces, device-lost recovery, draw path |
| `5b75b31` | All `/W4` warnings cleared, `TreatWarningAsError` enabled |
| `9a48f5d` | `draw()` made a pure read across every renderable |
| `ca4d228` | `draw()` made `const`, so that is enforced by the compiler |

Verified after each commit: `Debug|x64` and `Release|x64` build clean with
warnings as errors, 38/38 unit tests pass, and the game launches and runs.
(`Win32` is declared in the project files but not mapped in the `.sln`, so it
was already unbuildable and remains so.)

---

## Critical findings

| # | Finding | Status | Commit |
|--:|---|---|---|
| 1 | No engine target: one Application project, 193 files flat | **Outstanding** | — |
| 2 | `ConfigurationType=Application`, so nothing can link it | **Outstanding** | — |
| 3 | Device-lost destroys `ResourceManager` and the dt float | Fixed | `0c7eaff` |
| 4 | `closest_pt_point_OBB` loops 3 axes in 2D, throws on every call | Fixed | `73da0a2` |
| 5 | Device restore rebuilds SpriteBatches on the destroyed device's contexts | Fixed | `0c7eaff` |
| 6 | `OnDeviceRestored` reconstructs every subsystem | Fixed | `0c7eaff` |
| 7 | Device-lost destroys services held by raw pointer | Fixed | `0c7eaff` |
| 8 | `OnDeviceLost`/`Restored` replace every shared service | Fixed | `0c7eaff` |
| 9 | `OnDeviceRestored` reallocates the shared frame-time float | Fixed | `0c7eaff` |
| 10 | `AudioEngine` destroyed before its WaveBanks | Fixed | `0c7eaff` |
| 11 | Gamepad vector compacted by connection, indexed by player number | Fixed | `0c7eaff` |
| 12 | End-menu Restart replays with the wrong viewport layout | Fixed | `0c7eaff` |
| 13 | Collision interface identifies objects with this game's content enum | **Outstanding** | — |
| 14 | `player_inputs` compacted but indexed by player ordinal (OOB) | Fixed | `0c7eaff` |
| 15 | `Level` indexes the compacted input vector by ordinal | Fixed | `0c7eaff` |
| 16 | Every render worker draws every object, and `draw()` mutates it | **Partial** | `9a48f5d`, `ca4d228` |
| 17 | `draw_zoom_out_level_component` mutates ViewportManager, touches immediate context | Fixed | `0c7eaff` |
| 18 | `Level` is both the scene abstraction and the paint-battle ruleset | **Outstanding** | — |
| 19 | Level JSON parsed through a 1-byte heap buffer | Fixed | `73da0a2` |
| 20 | `StructurePaintable` stores a reference to a builder local | Fixed | `0c7eaff` |
| 21 | `RectangleRotated(Segment, thickness)` always throws | Fixed | `73da0a2` |
| 22 | `PauseMenu` subscripts the input vector unvalidated | Fixed | `0c7eaff` |
| 23 | `Player::draw` mutates state, runs on several threads | Fixed | `9a48f5d` |
| 24 | `get_delete_timer()` returns `_timer`, so nothing ever expires | Fixed | `73da0a2` |
| 25 | Save-file parse: 8-byte read into a 1-byte allocation | Fixed | `73da0a2` |
| 26 | Every JSON file parsed through a 1-byte buffer | Fixed | `73da0a2` |
| 27 | `fopen` result never checked in any of the four loaders | Fixed | `73da0a2` |
| 28 | `SoundBank` read buffer sized with `sizeof(unique_ptr)`, then leaked | Fixed | `73da0a2` |
| 29 | `SpriteSheet::load_from_json` same defect | Fixed | `73da0a2` |
| 30 | `SpriteSheet::draw` calls `map::operator[]` from 16 workers | Fixed | `73da0a2` |
| 31 | Resource lookup mutates shared maps from worker threads | Fixed | `73da0a2` |
| 32 | Paintable face selection maps edge indices to the wrong faces | Fixed | `0c7eaff` |
| 33 | No exception barrier in the thread-pool callback | Fixed | `73da0a2` |
| 34 | `Weapon::draw` mutates shared state on several render threads | Fixed | `9a48f5d` |
| 35 | `stop_sounds()` throws for Sniper and Bucket | Fixed | `73da0a2` |
| 36 | Match end throws unhandled if any player picked Sniper or Bucket | Fixed | `73da0a2` |

### Note on #16 (partial)

Two defects were merged under this finding. The **data race** is fixed, and as
of the const pass it is fixed in the sense that survives someone editing the
code: `IGameObject::draw` and every override of it are `const`, so a `draw()`
that assigns a member is now a compile error rather than something an audit has
to keep catching. `Weapon::draw`, `InterfaceGameplay::draw_gameplay_interface`
and the three `Drawer` helpers are const for the same reason — none of them is
an `IGameObject`, but all sit under the same per-view fan-out, and `Level`
holds them by `unique_ptr`, which does not pass its own constness on to what it
points at.

Making the signatures const also fixed a live dispatch bug it exposed:
`TextDropShadow::draw` was non-const while `TextObject::draw` was const, so it
never overrode anything — it *hid* the base. A `TextDropShadow` drawn through a
`Text&` silently lost its shadow.

The **redundancy** is not fixed: `draw_player_view_level` still walks the entire
world in every task rather than its own slice, so an N-player match does N times
the visibility work. That is wasted time, not undefined behaviour.

Worth recording for whoever writes `Scene`: the parallelism axis here is
**views, not objects**. Workers do not own disjoint slices — every worker draws
every object, so the pure-read contract is the *only* thing making this sound.
PHILOSOPHY's tenet still says workers own disjoint slices, which does not
describe this renderer.

---

## Also fixed, beyond the critical list

**Build guardrails (review's suggested step 1).** `LanguageStandard` was unset,
so the project silently built as C++14 — now C++20. `TreatWarningAsError` is on
with zero suppressions. The test project was aligned with the game's `/W4`,
`/fp:fast` and conformance mode; it had been validating `precise` arithmetic
while the game shipped `fast`.

**Bugs `/W4` was already reporting but nobody read** (`5b75b31`):

- `MTexture::is_visible_in_viewport` called itself — C4717 said "recursive on
  all control paths, function will cause runtime stack overflow".
- `GameData::get_thread_pool` had a committed duplicated `return` (C4702).
- `Level::get_level_end_info` caught `std::bad_cast` around a **pointer**
  `dynamic_cast`, which returns null rather than throwing. The catch could
  never fire and the null was dereferenced one line above it.
- `DeviceResources::get_deferred_context` was declared `noexcept` and throws
  (C4297).
- `Player::on_structure_ramp_collision` had an unread `is_on_ramp` (C4189)
  next to an empty `// TODO` for `STRUCTURE_RAMP_LEFT` — left ramps had no
  collision response at all and players fell through them. Implemented as the
  mirror of the `RAMP_RIGHT` case.

**MattMath defects the vacuous validation had been hiding.** Fixing
`RectangleRotated`'s validate-before-`_points` ordering (#21) turned two
previously-passing tests red, both correctly:

- `edges_valid()` compared an un-normalised dot product and raw edge lengths
  against an absolute epsilon, so it rejected large rectangles that were
  perfectly square.
- `normalize()`/`normalized()` divided by zero for a zero-length vector,
  producing NaN that propagated into velocities and shape validation. They now
  return zero, and the header states how that differs from
  `unit_vector()`/`to_unit_vector()`, which substitute `(1, 0)` — the review's
  "two contradictory zero-length contracts".

**Other:**

- `RectangleRotated`'s setters validate the candidate before committing, so a
  rejected argument no longer leaves the object holding the bad value and a
  stale corner cache.
- `ResourceManager::reset_all_*` erase entries instead of leaving keys mapped
  to null, which was indistinguishable from a loaded resource.
- A projectile leaving the level is marked for deletion instead of throwing
  `"Collision object out of bounds"`, which terminated the process.
- `WeaponRoller::draw` was a verbatim copy of `Weapon::draw` apart from one
  colour choice; replaced by a `get_draw_colour()` hook.
- `MenuPage::draw_range_of_mobjects_in_viewports` uses the deferred contexts
  passed to it instead of re-fetching the same ones through `GameData` from a
  worker thread.
- Tests added for the OBB primitives (closest point, axis-aligned and rotated),
  the `RectangleRotated(Segment)` constructor, degenerate-input rejection, and
  setter transactionality — the review noted the OBB work shipped with no
  behavioural coverage of its own primitives.

---

## Known remaining issues touched but not resolved

- `Level::_sampler_state` caches a pointer into the `CommonStates` that device
  loss destroys. The service graph no longer dangles on restore, but this one
  cached D3D pointer still does.
- `Player` leaving the level bounds still throws on the tick path with no
  handler above it. Unlike the projectile case this is a genuine simulation
  failure, so it was left loud rather than silently swallowed — but it is still
  a process kill.
- `Weapon`, `Projectile` and `Player` still throw from `default:` cases on
  `player_team` and `wep_type` switches. Individually reachable only through
  states the game does not currently produce, but the pattern is the same one
  that made `stop_sounds` fatal.
