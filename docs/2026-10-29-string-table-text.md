# OCA-42: string-table text guide

Selected 2026-10-29 after OCA-41's source implementation and native literal
consumer check. Its stopped wasm/full-CI qualification remains open. This
source work follows the existing L21 story, using already available JSON,
text conversion, font coverage and drawable APIs rather than adding an
engine string-table facility. OCA-34 and OCA-43 already document atlas
authoring and the language limits.

## Scope

Extend the existing Sprites and text guide with one worked load-time path:
read a game-owned UTF-8 JSON table, take an owned string, widen once, check
the chosen font and return an owned `Text`. Quote the helper from a compiled
case through `SourceFile`, and link the Input guide's UTF-8 `typed()` passage
to it. Keep editing, language selection and key choice in the game.

Explain the existing ASCII-only template atlas, required character coverage,
MakeSpriteFont's existing `/CharacterRegion` option and one fixed rasterized
size per atlas. `FontSize` is in points, not a promise of identical pixel
height. Exact new numeric character-region command syntax has not been
checked against a local authoring tool and is not introduced here.

Keep source and runtime paths distinct: source `content/strings/fr.json`
is staged as `strings/fr.json` beside the executable or `/strings/fr.json`
in the browser package. Anchor the read to `executable_directory()` with
`resolved_under`; copying/installing the new folder and adding its browser
`LINK_DEPENDS` entry remain the game's build responsibility.

## Planned source and headless checks

- Register a new ASCII-only source in the existing AssetsTests target.
  Reuse its file fixture; create no test entry, engine API or framework.
- Exercise actual UTF-8 JSON bytes authored with C++ hex escapes and the
  equivalent JSON Unicode escape. Confirm four wide units and `Text`'s
  ownership after the JSON document and local conversion strings die.
- Check a missing accent's first code-unit index and an outside-atlas Greek
  letter even with a stand-in. These are glyph-membership checks, not strict
  UTF-8 validation, normalization, shaping or glyph-image judgments.
- Select the recording-only case with a PRIVATE AssetsTests definition for
  null render. A synthetic memory atlas can prove four submitted glyph
  sprites after load; it cannot establish real font appearance. Guard the
  null configuration before running it, and keep default cases API-free.
- Build/run complete native AssetsTests in default Debug, Debug null and
  Release null; run eight standalone checks and website check/tests/build.
  Verify the complete helper excerpt, Input link and packaging instructions.

## Recorded source and checks

The text section, Input link, helper source and existing test-target
registration are implemented. All three original files, the absent new
source path, four candidate hashes and fifteen unchanged engine/template/
fixture hashes were checked before edits. The new C++ file is ASCII without
a BOM; UTF-8 data is constructed with hex escapes and compared with JSON
Unicode escapes. The owner's source-literal reservation remains intact.

Complete native AssetsTests pass with zero skips:

| Configuration | Cases | Assertions |
|---|---|---|
| Default Debug | 76 | 585 |
| Debug null | 77 | 595 |
| Release null | 77 | 595 |

Three portable cases use synthetic glyph metadata and no renderer. They
check both JSON spellings, four owned wide units and measured bounds after
the helper's document/conversion strings and temporary file scope end. A
missing accent reports the source/title and UTF-16 unit 3; outside-atlas
U+03A9 reports unit 0 even when the font has a stand-in.

The fourth case is compiled only for null render through a PRIVATE
AssetsTests definition. Cache/backend guards and generated macro selection
were checked before execution: the default binary omits it. Null records
four sprites with the synthetic memory atlas and checks texture, first/last
positions and the accent's texture coordinate. These are rectangle
submissions, not real glyph artwork or graphics API device results.

All eight standalone checks pass, including 49 engine include fixtures.
Website verification reports zero type diagnostics, 160 passing tests,
145 pages, 436 generated files and no snapshots. The generated HTML contains
every line of the compiled helper, its complete return beyond the inner
validation block, the typed-text anchor and the path/packaging/language
guidance. SourceFile and include-site readers use the checkout's filesystem,
so the new source is available to them before Git staging.

The initialization and copy/install/relink instructions were checked against
the existing content-root API and template CMake files. They are not a full
installed-game or packaged-browser execution result. Engine rendering and
conversion source, font assets and existing language limits are unchanged;
no typing component or wrapping policy was introduced.

## Remaining acceptance

The story stays unchecked until its actual non-ASCII matching-atlas drawing
and remaining browser/wasm acceptance are recorded. Current device/browser
and Emscripten stops remain in force. No numeric authoring-command result,
real glyph artwork, graphics API device or browser qualification is claimed
by a metadata or null-recording check.
