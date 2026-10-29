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
height. The guide gives no numeric character-region command; the command
used for acceptance is recorded below.

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

## Matching-atlas and browser acceptance

Recorded 2026-10-29 at `1b5ecee`; the recipe is `7fe2068`. MakeSpriteFont.exe
from DirectX Tool Kit's `may2026` release (Microsoft-signed, SHA-256
`7c1127265365a4d7ded5d01381607491308cc9f429fe5cf90f68c33bc8d2b4f0`) was used
from a scratch folder and is not in the repository. JetBrains Mono is not
installed on the machine, so the atlas is the installed Courier New Bold:

```bat
MakeSpriteFont "Courier New" menu_bold_16.spritefont /FontSize:16 /FontStyle:Bold /CharacterRegion:0x20-0x7e /CharacterRegion:0xe9
```

It captures 96 glyphs, printable ASCII plus U+00E9, into a BC2
(CompressedMono) atlas, SHA-256
`017b1596e3b5ab7da107b1c75d23184b0564abae6eac1534a800ffcbb024277c`. Its
debug sprite sheet was inspected visually and contains U+00E9.

`tools/consumer_check/assemble.py` assembled the Get Started project in
scratch. Following the guide, the project adds `content/strings/fr.json`
with the JSON-escaped title, the atlas as `menu_bold_16`, and the strings
copy, install and `LINK_DEPENDS` lines. It copies the helper verbatim from
the compiled test. Initialization resolves the table under
`executable_directory()`, draws the returned `Text` in the scene overlay and
shows the coverage results in a label.

- Native `x64-debug`, D3D11 on an RTX 5080: the build passes and so do
  exactly the project's two tests. The window capture shows `Café` with its
  accent. Standard error and the label report four units,
  `can_render(title)` true and `can_render(U+03A9)` false at unit 0. The
  table was then rewritten with the raw UTF-8 bytes `C3 A9` instead of the
  `é` escape, rebuilt and run again: the same four units and coverage
  results.
- Browser `wasm-release`, Chrome 156 WebGL2 through ANGLE D3D11: the clang
  build and the Node rules test pass. A `run_page.mjs` input run with strict
  complaints captured the same title and status line. A
  `--quit-keys Escape,Enter` run drew 60 frames and reached `Module.onQuit`
  with no console or browser complaint.
- Under Node, `StringTableTextRecipeTests` passes 3 cases and 28 assertions
  in wasm-release and wasm-debug, and 4 and 38 in wasm-release-null. Complete
  AssetsTests pass 79/637, 79/637 and 80/647 with zero skips.
- CI run 40836037123 on `b96bad6`, which contains the recipe, passed all
  12 jobs.

This shows one precomposed Latin accent drawing from an authored atlas. The
language limits are unchanged. Safari, phones and other GPUs were not run.
