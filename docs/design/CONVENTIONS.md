# Labrador — Naming & Code Conventions

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
| Namespaces | one lowercase word | `labrador`, `mattmath` |
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
- Types are spelled out too: `auto` does not stand in for a type that
  can be written. `Window* self`, not `auto* self`; `const
  std::vector<Segment> edges = triangle.edges();`, not `const auto edges
  = ...`. A declaration tells the next reader two things, the name and
  the type, and `auto` moves the second one out of the line and into a
  callee that is usually in another file (T4). The exception is the type
  that cannot be written: a lambda's type has no spelling, so a lambda
  held in a variable is held in an `auto`. The price is the occasional
  long declaration — an iterator, a nested `value_type` — and where one
  reads badly the fix is a named alias or a range-`for`, both of which
  say more than the `auto` did, not less. This restricts style, not
  vocabulary: `auto` is C++ that every C++ programmer knows, and
  declining a deduction invents no dialect (T12) the way a macro
  standing in for a keyword would.

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
  `mattmath::`; everything in `LabradorEngine` lives in `labrador::`.
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
  a return struct, an options bag, a typedef naming the same thing for a
  different reader — may ride along. A header may instead hold no type at
  all, and then it is named for what it computes rather than for what it
  holds: `shape_type.h` is an enum, `scalar.h` is the scalar vocabulary
  and the tolerance ordering, `intersects.h` is the pairwise predicates.
  That is the exception, not a second pattern to reach for — a header
  with no type and no single answer to "what is this for" is a bag, and
  the rule exists to stop bags.
- No huge files. Deliberately no line count: length is a symptom, not the
  defect, and the number that would be right for a table of constants is
  wrong for a class with intricate control flow. A file is too big when a
  reader can no longer hold what it does in their head — and the cause is
  almost always a type that grew past one job, so the fix is usually to
  split the type, not the file. Growth is the signal worth watching: a
  `.cpp` that gains a section a month is asking to become two. This is a
  guideline review raises, not a check the build fails, because a
  threshold crisp enough to enforce mechanically would be wrong often
  enough to need suppressing — and zero suppressions is a promise kept
  elsewhere (Tests and toolchain, PHILOSOPHY.md).
- `#pragma once`, not include guards: no name to get wrong, nothing to
  collide (T5).
- Include paths are written from the repository root, so every include
  states its module: `#include "engine/render/renderer.h"`. An engine
  file including `"game/..."` is thereby visible at a glance — and fails
  to build (see ARCHITECTURE.md).
- Include order: own header first — proving every header compiles on its
  own — then engine headers, then external, then standard library.
- Tests are named for their subject: `vector2_tests.cpp`.

## Comments

A comment states what the code cannot. There are exactly three things it can
be, and knowing which one is being written is most of the discipline:

- **A contract.** What a caller is held to: a precondition, an invariant, what
  is thrown and when, what a returned handle's lifetime is, which thread may
  call. This is the most valuable kind and it lives **at the declaration**,
  because the person about to violate it is reading the declaration and not
  anything else. It never moves to a document.
- **A rationale.** Why the code is this shape and not the obvious other one —
  the alternative that was weighed and lost, the constraint that forced a hand.
  Worth writing when the obvious reading is wrong; **cited rather than
  re-derived** when it has a name. "T3: take the simpler model" is a complete
  comment (PHILOSOPHY.md), and a paragraph re-arguing T8 next to a citation of
  T8 is a paragraph that can go stale against its own source.
- **Archaeology.** How the code came to be this way: what it used to be, which
  commit moved it, what a previous version of the comment wrongly said. **This
  is never a comment.** `git log -S` answers it precisely, on demand, and
  without rotting. A tree that writes it down accumulates a changelog in its
  headers that no build checks and no reader trusts.

The test is the tense. A sentence about what the code *is* earns its place; a
sentence about what it *was* belongs to the history, and a sentence narrating
what this very comment used to say has lost the thread twice.

- **Prose that outgrows the declaration it sits on becomes a document beside
  the code**, not a longer comment: `engine/render/SEAM.md` is the seam's
  charter and `renderer.h` cites it by section. Beside the code rather than in
  `docs/design/`, which is written in the present tense of the target and says
  nothing about the current tree — and **amended in the same commit as the
  change that fights it**, exactly as the design documents are. Prose one
  directory from its subject needs that rule more than prose in the same file,
  not less.
- **Growth is the signal, as it is for file length.** A header whose
  declarations have not changed in a month and whose comments have doubled is
  not better documented; it is accreting. The ratio worth watching is comment
  lines against the code they describe, and the fix is almost always that a
  rationale has outgrown its declaration and wants a document.
- Commented-out code is deleted. It is the purest archaeology and the version
  control system already has it.
- A comment that restates the line below it is noise: `// increment the
  counter` above `++count`. Naming (everything above) is what carries that
  load.

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
- Commented-out code, or a comment narrating what the code used to be.
- A comment re-deriving a trade-off it could cite by number.
- `auto` where the type can be written — a lambda variable is the only
  place it can't.
