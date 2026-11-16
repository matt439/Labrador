# Generating a grid sprite sheet

`tools/grid_sheet.py` writes the existing sprite-sheet JSON from explicit
image dimensions and a uniform grid. It uses Python's standard library only;
Python 3.10 or newer runs the command. It opens no image,
changes no pixels and installs nothing. The image still has to be supplied
and loaded by the game.

From a game project's folder, with the tool in its engine checkout:

```bat
python external\labrador\tools\grid_sheet.py --image 128 32 --grid 4 1 --frame red 0 0 --strip cycle 0 0 4 0.25 true --output game\content\textures\lamp.json
```

The output folder must already exist. The command validates every input and
then replaces the named output file. A refused definition leaves an existing
file untouched; replacement is made from a completed temporary file in the
same folder. An unwritable destination names the path and exits
with failure.

The tool is new development tooling, absent from the `v0.3.0` starter. Its
output uses the original schema and also works with that release: a copy of
this one Python file can run outside an engine checkout. Keeping an older
starter does not require an engine change to use generated metadata. The
[animation guide](../website/src/content/docs/docs/guides/your-animation.mdx)
retains manual JSON authoring alongside the command.

## Inputs

| Flag | Meaning |
| --- | --- |
| `--image WIDTH HEIGHT` | The actual image's dimensions in texels, supplied by the author. Each is a positive signed 32-bit integer. No image reader verifies them here; the real asset loader checks the resulting rectangles against the loaded texture. |
| `--grid COLUMNS ROWS` | Positive cell counts. The image dimensions must divide evenly, giving positive integral cell sizes. The tool never enumerates the whole grid. |
| `--frame NAME COLUMN ROW` | One cell, named for sprite-frame lookup. Repeat the flag for more frames. Column and row are zero-based and nonnegative. |
| `--strip NAME COLUMN ROW COUNT SECONDS LOOPING` | A first cell and a horizontal sequence of `COUNT` cells. Repeat for more strips. `LOOPING` is exactly `true` or `false`. A strip never wraps onto another row. |
| `--output PATH` | The JSON file to replace after validation. Output is UTF-8 with LF newlines and one final newline. |

Frame flags retain their order in `sprite_frames`; strip flags retain theirs
in `animation_strips`. Keys have a fixed order, with no timestamp, source path
or machine information. Identical inputs produce identical bytes. Names must
be nonempty UTF-8 without NUL and unique within their table; a frame and a
strip may have the same name because their lookups are separate.

All emitted integer fields fit the loader's signed 32-bit range. Arithmetic
uses Python's unbounded integers before checking the result. Nonempty strips
are limited to 16,384 frames, matching `max_texture_dimension`, and their first
and last rectangles must fit the image. Those endpoints bound every frame.

Zero-count strips remain empty, matching the loader: their unused rectangle
may lie outside the image, but its coordinates must still fit in `int`.
`SECONDS` accepts the loader's finite float-range values, including zero or
negative values. A playback component decides what timing it can use;
`AnimationPreview` and the guide's looping player need a nonempty strip with
positive frame time. This metadata command does not select playback policy.

Generated cells have positive size and no rotation or authored pivot. Existing
hand-authored JSON is unchanged: signed sizes, zero-area individual frames and
optional origins keep the loader's own contract. Packing, margins, gaps,
nonuniform frames and animation sequences are outside this uniform-grid tool.

## Reproduce the delivered animation metadata

From the engine repository root:

```bat
if not exist out mkdir out
python tools\grid_sheet.py --image 64 16 --grid 4 1 --strip pulse_loop 0 0 4 0.25 true --strip pulse_once 0 0 4 0.25 false --output out\animation_preview.json
```

This writes the same canonical JSON as
`samples/components/content/textures/animation_preview.json`: a 64 by 16
image, four 16 by 16 cells, one looping strip and one ending strip. No art is
regenerated. Text comparisons normalize checkout CRLF to LF.

`tools/tests/test_grid_sheet.py` checks the actual command output, deterministic
bytes, boundary/refusal cases and that delivered example. The generated
`tests/assets/grid_sheet/grid.json` passes through the real reader in
`AssetsTests`; null render also loads the existing delivered atlas through
`ResourceLoader` and records its selected UVs. `ComponentsTests` loads the
copied example and checks its loop and one-shot playback. These tests run in
native-null and wasm-null configurations; they make no visual acceptance claim.
