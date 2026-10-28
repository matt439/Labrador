# OCA-41: source literal encoding

Selected 2026-10-29 after SM-67 from the existing ready Later work. BACKLOG's
L21 puts OCA-41 before the string-table and wrapping examples; its language
limits and atlas-authoring prerequisites are already documented. At selection,
the shared MSVC settings had no explicit source/execution character encoding,
while the repository's EditorConfig required UTF-8.

## Scope and reservation

Add `/utf-8` to `labrador_settings` for MSVC, explaining that it sets both
source decoding and ordinary narrow-literal execution encoding. Record the
consumer-visible option change, the copied-template note and source-encoding
guidance. Font coverage, shaping, supplementary-plane behavior, `char8_t`
policy and rendering remain unchanged.

[STORIES.md, OCA-41](product/STORIES.md#oca-41) reserves the first non-ASCII
committed test source for the owner and explicitly permits a recorded
scratch-consumer fallback. Use that fallback: no non-ASCII C++ test file or
new generator framework enters the tracked tree. The existing text-encoding
tests use escapes and test conversion; they do not prove compiler decoding.

The ignored scratch consumer includes this checkout's actual
`cmake/settings.cmake` and links `labrador_settings`. Its CMake adds no manual
encoding option. Its BOM-free UTF-8 C++ source contains actual raw non-ASCII
bytes in both a wide and an ordinary narrow literal. Expected values are six
BMP units `0063 0061 0066 00E9 0020 3042` and nine UTF-8 bytes
`63 61 66 C3 A9 20 E3 81 82`. It prints numeric values and returns failure on
a mismatch in every build type. This is no glyph-drawing or broader Unicode
acceptance claim.

## Qualification plan

- Guard original/candidate and unchanged text/font/test hashes before edits.
- Optionally observe the unmodified settings with the same scratch source;
  a passing baseline is possible and no negative is required. Do not force
  a code page, locale or compiler override to manufacture one.
- Configure/build the scratch target through installed CMake/Ninja and
  vcvars, inspect the actual compile command for inherited `/utf-8`, then
  run the console probe and record its exact code units, bytes and exit code.
- Build the explicit engine/Core/Assets/Audio/Ui/Render targets under the
  five installed native Debug configurations plus Release null, serializing
  presets with two compiler jobs. These targets compile/link, run source
  checks and copy content; they launch no application or graphics API device.
  Follow with anchored headless CoreTests in default Debug and Release null.
- Run all eight standalone checks and website check/tests/build, then review
  the rendered guide paragraph and its unchanged language-limit link.

## Recorded implementation and checks

The shared option, copied-template note and guide paragraph are implemented.
Original/candidate and the five unchanged text/font/test hashes were guarded
before applying the three source changes. The actual scratch source is
BOM-free UTF-8 with two raw literal spellings; its SHA-256 is
`0150607f086c56cdaa5e72a8db3c54f4bcea7deef77e85313c1e3a7d41212fe0`.

With unmodified settings, MSVC 19.51.36256.0 configures and builds the consumer
successfully without compiler warnings, but its explicit code-unit check
returns 1:

```text
wide_length=9
wide_units=0063 0061 0066 00C3 00A9 0020 00E3 0081 201A
narrow_length=9
narrow_bytes=63 61 66 C3 A9 20 E3 81 82
utf8_literal_match=0
```

After the option is applied, CMake rebuilds that same source through the
actual settings target. Its real compile command contains exactly one
inherited `/utf-8`; scratch CMake supplies no manual encoding option.
Configure/build, command inspection and the console probe succeed:

```text
wide_length=6
wide_units=0063 0061 0066 00E9 0020 3042
narrow_length=9
narrow_bytes=63 61 66 C3 A9 20 E3 81 82
utf8_literal_match=1
```

The original narrow spelling already retained these nine bytes on this
machine; the wide units demonstrate the source-decoding failure. The new
option makes both encodings explicit. No locale, code page or global
compiler setting was changed, and no forced negative was needed.

All six planned native configurations build the explicit six-target set
successfully, including its math dependency, with `/W4 /WX` and no compiler
warning diagnostics. All 769 generated C++ object rules for those seven
targets carry `/utf-8`. Default Debug and Release null CoreTests each pass
84 cases/4,989 assertions with zero skips. No other runtime suites, sample
startup, device test or pixel test were run for this slice.

All eight standalone checks pass, including 49 engine include fixtures.
Website verification reports zero type diagnostics, 160 passing tests,
145 pages, 436 generated files and zero release snapshots. The rendered
guide contains the encoding paragraph and its existing language-limit link.
Text conversion, font contracts and existing conversion tests are unchanged.

## Remaining acceptance

OCA-41 remains unchecked: wasm/Node is at the standing Emscripten activation
stop and full all-presets CI remains required. Native Release configurations
other than Release null were not locally built. The explicit scratch
fallback leaves the owner's reservation on committed literal source intact;
it does not require resolving that choice to make this source progress.
No browser, generated game module, graphics/audio API device, endpoint,
listening or rendered-font acceptance is claimed. The compiler check covers
the stated BMP sequence, not supplementary-plane or script rendering.
