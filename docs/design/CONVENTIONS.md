# ArtAttack — Naming & Code Conventions

Names are the engine's first API. A stranger meets `Scene`, `draw_sprite`
and `dt` before they meet any documentation, so the scheme must be
learnable in one sitting (T10) and boring everywhere (T4). Like
[PHILOSOPHY.md](PHILOSOPHY.md), this document is written in the present
tense of the target: it describes the destination, says nothing about the
current codebase, and changes by amendment in the same PR as the change
that fights it.

Almost everything below falls out of one rule:

> **PascalCase names a type. snake_case names everything else.
> SCREAMING_SNAKE names a macro.**

| Thing | Form | Example |
|---|---|---|
| Types — classes, structs, enums, aliases | `PascalCase` | `AnimatedSprite`, `WeaponType`, `Handle` |
| Functions and methods | `snake_case` | `draw_sprite()`, `resolve()` |
| Variables and parameters | `snake_case` | `frame_time`, `dt` |
| Private data members | `snake_case_` (trailing underscore) | `frame_time_` |
| Public struct fields | `snake_case` (bare) | `origin`, `layer_depth` |
| Constants and enumerators | `snake_case` | `max_velocity`, `WeaponType::sprayer` |
| Namespaces | one lowercase word | `artattack`, `mattmath` |
| Macros (rare) | `SCREAMING_SNAKE` | `AA_ASSERT` |
| Files and directories | `snake_case` | `animated_sprite.h`, `engine/render/` |
| JSON keys, asset and registry names | `snake_case` | `frame_time`, `player_walk` |

## Types

- Named for their **role**, as nouns: `Scene`, `Renderer`, `AnimationStrip`.
- **No prefixes, ever.** Interfaces are just types — `GameObject`, not
  `IGameObject`; whether a type is abstract is visible in its header, not
  encoded in its name. Unreal's letter prefixes (`F`, `U`, `A`) serve a
  reflection system this engine does not have (see The object model in
  PHILOSOPHY.md); Hungarian notation encodes what the type system already
  enforces (T5).
- Acronyms are cased as words: `JsonLoader`, `UiButton`. Proper nouns and
  trademarks keep their own spelling: `D3D11Renderer`, `XInputDevice`.
- Template parameters are types, so they follow types: `T`, `VertexType`.

## Functions

- Actions are verbs: `update`, `resolve_collision`, `load_manifest`.
- Accessors are the noun, bare: `bounds()`, not `get_bounds()`. Setters
  take `set_`: `set_bounds()`. The asymmetry is deliberate — reads are
  frequent and should read like the thing itself.
- Predicates read as questions: `is_paused()`, `has_focus()`,
  `contains(point)`.

## Variables and members

- Private data members carry a trailing underscore: `frame_time_`. A
  leading underscore is legal at class scope but pattern-matches the
  reserved-identifier rules, which makes every careful reader stop to
  check (T4); the trailing form is unambiguous. Public struct fields are
  bare — they are the API.
- Booleans read as assertions: `paused_`, `is_visible`, `has_manifold`.
- Names are spelled out. The allowlist of short forms is small and
  closed: `dt`, `id`, `src`, `dst`, `min`, `max`. Everything else earns
  its letters — `weapon_type`, not `wep_type`.

## Constants and enumerators

- Constants are values (T11) and follow value naming: `max_velocity`,
  `gravity`, `Colour::white`. SCREAMING_SNAKE is reserved for macros
  alone, so that a screaming name always signals preprocessor danger and
  nothing else ever does.
- Enumerations are `enum class`; the type is PascalCase, the enumerators
  snake_case: `WeaponType::sprayer`, `ScreenLayer::hud`.
- A header full of numeric constants is usually tuning data in the wrong
  place — *which* and *how much* belong in JSON (T7). Constants that stay
  in code live next to the one thing they configure, not in a `consts`
  namespace collecting strays.

## Namespaces

- One namespace per library: everything in `MattMath` lives in
  `mattmath::`; everything in `ArtAttackEngine` lives in `artattack::`.
  No deeper nesting, with one exception: `detail` wraps internals that
  headers must expose but users must not touch.
- The engine never claims the global namespace. Game code owns its own
  namespace and never reopens the engine's.
- `using namespace` never appears at namespace scope in a header. In a
  `.cpp`, `using namespace mattmath;` is acceptable — math reads badly
  fully qualified — and others are used sparingly.

## Files

- Lowercase snake_case, named for the primary type they hold:
  `animated_sprite.h` holds `AnimatedSprite`. Lowercase because a
  wrong-case `#include` compiles on Windows and breaks on a
  case-sensitive filesystem — the all-lowercase rule deletes the trap
  before the second platform arrives.
- One primary type per header. Small types that exist only to serve it —
  a return struct, an options bag — may ride along.
- `#pragma once`, not include guards: no name to get wrong, nothing to
  collide (T5).
- Include paths are written from the repository root, so every include
  states its module: `#include "engine/render/renderer.h"`. An engine
  file including `"game/..."` is thereby visible at a glance — and fails
  to build (see ARCHITECTURE.md).
- Include order: own header first — proving every header compiles on its
  own — then engine headers, then external, then standard library.
- Tests are named for their subject: `vector2_tests.cpp`.

## Data

- JSON keys, asset names, registry keys, spawn-group names: snake_case,
  same as code-side variables, so a definition reads continuously from
  file to loader. Names are identifiers — resolved to handles once at
  load, never compared per frame (T7, T8).

## Never

- Hungarian notation, or any type information smuggled into a name (T5).
- `I`, `C`, `F`, `U` or any other type prefix.
- `m_` — the trailing underscore already does that job.
- SCREAMING constants — screaming means macro, nothing else.
- `get_` on accessors.
- `using namespace` in a header.
- Abbreviations off the allowlist.
