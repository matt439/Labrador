# Strict asset keys: PI-20 implementation and checks

[Report 12's decision](review/colourwars-reports/content.md#12-json-from-memory-key-enumeration-source-spans)
accepts enumeration and strict engine loaders, and declines source spans.
This change adds `JsonValue::keys()` and one assets-private validation helper;
it adds no schema framework, parser, source offsets or writing API.

## Reader and loader boundaries

`keys()` copies complete decoded member names in stored order, including
duplicates, empty names and embedded NUL bytes. The copied strings outlive
their document. The `JsonValue` remains borrowed and wrong object types use
its existing source/context exception. Generic JSON permits game-owned
dictionaries and metadata; enumeration neither validates nor rewrites them.

`engine/assets/json_object_keys.h` is private to the two loaders and explicitly
excluded from website publication. It compares whole string lengths/bytes
with each fixed field list and reports the file, object path/index and unknown
name. ASCII control bytes and quote/backslash delimiters are escaped in that
name so a NUL does not truncate the diagnostic. RapidJSON remains private to
`json.cpp` and its existing parse path is unchanged.

| Fixed object | Accepted fields |
|---|---|
| Manifest | `assets` |
| Manifest group | `kind`, `directory`, `names`, `optional` |
| Sprite sheet | `sprite_frames`, `animation_strips` |
| Sprite frame | `name`, `position`, `size`, `origin`, `rotated` |
| Frame position | `x`, `y` |
| Frame size | `w`, `h` |
| Frame origin | `x`, `y` |
| Animation strip | `name`, `first_frame`, `frame_count`, `frame_time`, `looping` |
| Strip first frame | `x`, `y`, `w`, `h` |

Existing required fields/types and optional defaults stay the same. `rotated:
true` is still refused and `false` accepted. Kinds, resource/frame/animation
names and dictionaries in a game's own reader remain data. This does not extend
strictness to sound-bank definitions, saves or arbitrary JSON. It changes no
duplicate-member policy, numeric/domain limits or rotation support.

## Evidence and outstanding qualification

The existing `AssetsTests` entry gains twelve behavioral cases:

- Seven reader cases cover empty/nested/order, full decoded Unicode/empty/NUL
  names, duplicates, independent copies/lifetime, root/nested wrong types and
  unrestricted game dictionaries/metadata. Source uses ASCII escapes.
- Three manifest cases cover unknown document/group keys, `optionl`, indexed
  errors and complete NUL/control/empty key labels, while preserving optional
  true/false/default, custom kinds and arbitrary resource-name values.
- Two sprite cases cover every fixed object, `orgin`/`rotatd`, useful indexed
  diagnostics and a complete valid frame/animation strip's resolved values.

All **66 cases and 444 assertions** pass in `x64-debug`, `x64-debug-null` and
`x64-release-null`. Builds use at most two compiler jobs, existing installed
dependencies and disabled manifest installation. Release/null checks exercise
the checked failures with `NDEBUG`; no graphics/audio device is created by
these parser/loader cases. Independent read-only source review finds no issue
with schemas, lifetime, full key lengths, diagnostics, scope or registration.

The fresh copied minimal consumer builds against this source with preinstalled
dependencies and null render/audio, registers exactly its two tests and passes
both. This proves the unchanged template manifest and consumer include/target
shape; it adds no Release/device/Sandbox evidence. All standalone checks pass.
The website check reports zero diagnostics, all 160 tests pass, and the
143-page/431-file build validates its internal links and new reference output.
Logs use the `pi20-` prefix under ignored `out/loop-2026-10-28-2114/`.

**Still open:** wasm compilation and Node execution remain at the existing
Emscripten environment-write/generated-output stops; neither route was retried.
Native checks do not establish clang/wasm qualification, and CI's required
matrix remains outstanding. Keep PI-20's backlog box unchecked pending that
qualification. No browser, GPU, listening, deployment or private-client result
is implied by these checks.
