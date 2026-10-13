# Security policy

## Supported revision

`master`. The repository has no release tags yet, so the latest commit on
`master` is the only revision that receives fixes. Once the first tag is cut,
the latest tag and `master` are supported, and nothing older is.

Before reporting, check that the problem is still present on current `master`
rather than only on a revision your project pins.

## Reporting a vulnerability

Report it privately through GitHub's private vulnerability reporting: the
**Security** tab of [matt439/Labrador](https://github.com/matt439/Labrador),
then **Report a vulnerability**, which opens
<https://github.com/matt439/Labrador/security/advisories/new>. Do not open a
public issue for it.

Say which revision you tested, which file or input triggers it, and what
happens. A file that reproduces it is the most useful thing you can attach.

One person maintains this project, and no response time is promised. A
confirmed report is fixed on `master` and disclosed through a GitHub security
advisory once the fix has landed.

## What input the engine trusts

**Content files are trusted input.** The engine reads `.dds` textures,
`.spritefont` fonts, `.xwb` wave banks, an asset manifest (`manifest.json`) and
the sprite-sheet and sound-bank JSON a manifest names. It reads them to turn a
mistake in content a developer made into a clear error, not to defend a
process against a file written to attack it. If your game loads content from
somewhere you do not control - mods, user levels, files fetched at run time -
you are relying on parsers that have not been tested against hostile input,
and you should treat that content as you would untrusted code.

What the readers do, and what the tree has tests for:

- **A malformed file is a throw at load that names the file.** The three
  binary readers throw `std::out_of_range` naming the path when there is no
  file there, and every reader throws `std::runtime_error` naming the path and
  what is wrong with it otherwise, including the JSON reader for a file it
  cannot open (PHILOSOPHY T6). The tests in `tests/render/dds_file_tests.cpp`,
  `tests/render/sprite_font_file_tests.cpp`, `tests/audio/xwb_file_tests.cpp`,
  `tests/assets/json_tests.cpp` and `tests/assets/asset_manifest_loader_tests.cpp`
  feed the readers missing, truncated, mislabelled and inconsistent files and
  check that each is refused, and some of them that the message names the file
  or the entry.
- **The three binary readers cannot read past the end of the file.** The
  `.dds` and `.spritefont` readers read through `engine/core/byte_reader.h`,
  which checks every read against the bytes that are left, and the `.xwb`
  reader checks each region its header points at against the file's size
  before reading any of it. A count read from a file is checked against the
  bytes that have to hold it before a container is sized from it. Image
  dimensions, row strides and accumulated offsets are checked for overflow, and
  a `.dds` that claims more mip levels than its dimensions allow is refused.
- **`.xwb` banks are bounds-checked per entry** - each entry's play region must
  lie inside the bank's wave data and its loop inside the wave - and the
  MS-ADPCM decoder's step is clamped so a corrupt block cannot overflow it.
- **JSON reads are checked.** `engine/assets/json.h` wraps rapidjson so that a
  missing key or a value of the wrong type is a throw naming the file and where
  in it, rather than an assertion that a Release build has disabled.
  Identifiers and paths with an embedded NUL are refused.

What the readers do not do:

- **There is no fuzzing and no sanitizer configuration.** No fuzz target, no
  AddressSanitizer or UndefinedBehaviorSanitizer preset and no CI job that runs
  either exists in this repository. The tests above are hand-written cases, not
  a search for inputs nobody thought of.
- **Memory use is not capped.** Every reader loads the whole file into memory.
  An `.xwb` is decoded to 16-bit samples at load, which is about four times the
  file's size for MS-ADPCM, and nothing stops two entries naming the same wave
  data, so a small bank can describe a much larger decode.
- **JSON nesting depth is not limited.** The manifest and the files beside it
  are parsed by rapidjson's default, recursive parser, so a deeply nested
  document can exhaust the stack.
- **A manifest can name any file the process can read** that has the
  extension its kind adds. Each entry's `directory` and `name` are joined, with
  that extension, into a path that is opened as written; nothing confines it to
  a content folder, an absolute `directory` is taken as one, and `..` is not
  refused.
- **A glyph rectangle is not checked against its atlas**, nor a sprite-sheet
  frame against its texture. Either draws whatever the texture sampler returns
  for coordinates outside the image.
- **On Windows with the XAudio2 audio backend, `.xwb` banks are read by
  DirectXTK's `WaveBank`, not by the engine's reader**, so the checks above for
  `.xwb` apply only to the Web Audio backend. Textures are read by the engine's
  reader on every backend and then handed to the graphics driver, whose own
  handling of them is outside this repository.

## Scope

This policy covers the code in this repository: `engine/`, `samples/`,
`tools/`, `tests/`, `bench/` and the website source in `website/`. A
vulnerability in a dependency (see `vcpkg.json` and `external/`) is best
reported upstream; report it here as well if the way the engine uses it is
what makes it exploitable.
