# What linesweeper.wasm is made of, measured 2026-11-13

The measurement `REV#155` asks for: LineSweeper's shipped module built at
`wasm-release` at v0.1.0, v0.2.0, v0.3.0 and `14464191`, with its code bytes
attributed to engine module, sample, C++ and C runtime and dependency. No flag
in the tree changed. A size budget (`REV#53`) and the size flags (`REV#102`)
stay where they were; this record is the numbers they wait on.

**Most of the growth since v0.1.0 is PNG decoding, which LineSweeper never
uses.** The module grew by 84,047 bytes, and its code section by 72,991.
`engine/render/png_file.cpp` and lodepng together hold 38,349 bytes of code
and 2,147 of data at `14464191`, none of it present at v0.3.0. LineSweeper
embeds no PNG (its embedded files are a sprite font, its licence, the manifest
and `textures/white.dds`). The decoder is linked because
`engine/render/resource_factory.cpp` chooses `name.dds` or `name.png` when the
game runs, so every program that loads a texture by name carries it.

## Shipped sizes

Each tag's `linesweeper_web_dist` copies the `LineSweeperModule` link output
unchanged as `linesweeper.wasm` and its loader as `linesweeper.js`. Brotli is
Node's at quality 11, as `tools/web_bundle/bundle.mjs` computes it for
`BUILD.json`; gzip is level 9.

| Build | `linesweeper.wasm` | Brotli | gzip | `linesweeper.js` | Brotli |
|---|---:|---:|---:|---:|---:|
| v0.1.0 (`c369e8b9`) | 403,759 | 125,384 | 157,038 | 118,982 | 27,283 |
| v0.2.0 (`1e60fb26`) | 411,204 | 127,831 | 160,193 | 118,982 | 27,283 |
| v0.3.0 (`0f4f5033`) | 426,100 | 132,288 | 165,922 | 120,545 | 27,753 |
| `14464191` | 487,806 | 150,968 | 189,972 | 121,144 | 27,839 |
| v0.1.0 to `14464191` | +84,047 | +25,584 | +32,934 | +2,162 | +556 |

v0.1.0's 403,759 bytes and loader are the sizes `docs/port/web-as-built.md`
records for `c369e8b`, and v0.3.0's 426,100 and 132,288 are the figures the
LineSweeper bundle guide on the website quotes, so the rebuilds reproduce what shipped.
The owner's checkout at `1dc8533c` gives 487,799 bytes (151,122 under Brotli).

Where the module's bytes are, by section:

| Section | v0.1.0 | v0.2.0 | v0.3.0 | `14464191` |
|---|---:|---:|---:|---:|
| code | 330,451 | 337,774 | 350,985 | 403,442 |
| data | 69,862 | 69,982 | 71,498 | 80,671 |
| everything else (header, types, imports, function, element, export and small sections) | 3,446 | 3,448 | 3,617 | 3,693 |

## Code bytes by module

Bytes are each function's body in the shipped code section, after
`wasm-opt`, with its size field. Engine folders are the source folder of the
object that defined the function; a backend subfolder is shown on its own.

| Owner | v0.1.0 | v0.2.0 | v0.3.0 | `14464191` | v0.1.0 to `14464191` |
|---|---:|---:|---:|---:|---:|
| libc++ | 80,429 | 80,429 | 80,735 | 83,571 | +3,142 |
| engine/render | 35,834 | 36,850 | 39,437 | 65,230 | +29,396 |
| engine/assets | 40,808 | 40,890 | 46,203 | 50,391 | +9,583 |
| LineSweeper presentation, states and entry | 35,341 | 35,571 | 38,019 | 37,212 | +1,871 |
| libc | 31,369 | 31,369 | 31,382 | 31,392 | +23 |
| engine/app | 11,862 | 14,921 | 16,325 | 20,458 | +8,596 |
| engine/audio | 16,076 | 16,354 | 16,552 | 16,657 | +581 |
| engine/render/gl | 15,799 | 15,799 | 16,104 | 16,166 | +367 |
| lodepng (vcpkg) | 0 | 0 | 0 | 15,314 | +15,314 |
| engine/core | 12,382 | 12,291 | 12,625 | 14,046 | +1,664 |
| engine/scene | 5,786 | 10,849 | 10,599 | 10,844 | +5,058 |
| malloc (dlmalloc) | 9,337 | 9,337 | 9,337 | 9,337 | 0 |
| compiler-rt builtins | 6,930 | 6,930 | 6,930 | 6,930 | 0 |
| libc++abi and libunwind | 6,615 | 6,615 | 6,615 | 6,616 | +1 |
| engine/app/web | 5,630 | 5,630 | 6,190 | 6,256 | +626 |
| engine/audio/webaudio | 4,034 | 4,034 | 3,949 | 3,968 | -66 |
| engine/ui | 3,734 | 3,734 | 3,737 | 3,735 | +1 |
| LineSweeper rules | 1,956 | 1,956 | 1,956 | 1,972 | +16 |
| engine/input | 4,472 | 2,158 | 1,885 | 1,885 | -2,587 |
| engine/input/web | 538 | 538 | 722 | 722 | +184 |
| engine/math | 445 | 445 | 608 | 608 | +163 |
| engine/collision | 576 | 576 | 577 | 0 | -576 |
| linker-generated | 492 | 492 | 492 | 126 | -366 |
| **total** | **330,445** | **337,768** | **350,979** | **403,436** | **+72,991** |

Grouped, at `14464191`: the engine is 210,966 bytes of code (52%), the C++
runtime (libc++, libc++abi, libunwind) 90,187 (22%), libc, malloc and
compiler-rt 47,659 (12%), LineSweeper 39,184 (10%) and lodepng 15,314 (4%).
At v0.1.0 the engine was 157,976 (48%) and LineSweeper 37,297 (11%).

Two kinds of code sit inside the engine and sample rows rather than in a
dependency row, because they are templates instantiated in the object that
uses them. At `14464191`, functions named `std::...` defined in engine objects
total 34,336 bytes (29,716 at v0.1.0) and in LineSweeper 1,585 (1,283);
RapidJSON's reader, instantiated in `engine/assets/json.cpp`, is 7,926
(7,769). Both are counted in their engine row above as well.

## What changed between tags

From the object that defined each surviving function:

- **v0.1.0 to v0.2.0, +7,323 code.** `scene/scene.cpp` +5,063 and
  `app/application.cpp` +3,059; `input/touch.cpp` and `input/gamepads.cpp`
  (3,386 together) no longer define a surviving function, while
  `render/resource_factory.cpp` (1,544) and `core/file_path.cpp` (1,387)
  appear.
- **v0.2.0 to v0.3.0, +13,211 code.** LineSweeper's states and entry point
  move from the shared `LineSweeperWeb` objects into `LineSweeperModule`'s own
  (a rename in this table, +2,448 net with the new top-out banner);
  `render/sprite_geometry.cpp` appears (2,323) as `render_resources.cpp` and
  `text_object.cpp` leave the list; `assets/asset_manifest_loader.cpp` +1,695
  and `audio/audio_resources.cpp` (1,503) appear.
- **v0.3.0 to `14464191`, +52,457 code.** PNG reading: `labrador::decode_png`
  alone is 22,852 bytes, because `wasm-opt` inlined lodepng's decode path into
  it, and lodepng's surviving functions are 15,314 (zlib inflate 4,941,
  unfiltering 4,322, Huffman tables 2,102). Also `app/application.cpp` +4,132
  (`load_manifest` and `render` now stand alone), libc++'s filesystem
  `operations.o` +2,664 (new at this build; `resource_factory.cpp` asks `std::filesystem::exists` which file is there), `render/resource_factory.cpp`
  +1,693, `assets/json.cpp` +1,563 and `assets/sprite_sheet_loader.cpp`
  +1,490. `collision` leaves the module: its one surviving function at v0.3.0 was `Partitioner::partition` (577 bytes), which moved to core.

## Data bytes

Data is attributed from `wasm-ld`'s link map, which is the layout before
`wasm-opt`'s memory packing, so the totals (70,940 to 81,752) are a little
above the shipped data section (69,862 to 80,671). `.bss` is excluded.

| Owner | v0.1.0 | v0.2.0 | v0.3.0 | `14464191` |
|---|---:|---:|---:|---:|
| embedded files | 29,711 | 29,711 | 29,711 | 29,711 |
| merged string constants (linker) | 12,432 | 12,549 | 12,965 | 19,262 |
| libc++ | 8,914 | 8,914 | 8,914 | 9,218 |
| libc | 7,327 | 7,327 | 7,327 | 7,327 |
| engine (all folders) | 7,791 | 7,791 | 8,012 | 8,525 |
| LineSweeper | 3,825 | 3,825 | 4,678 | 4,622 |
| lodepng | 0 | 0 | 0 | 2,147 |
| libc++abi, libunwind, malloc | 940 | 940 | 940 | 940 |

The embedded files are the sprite font (24,625 bytes), its OFL licence text
(4,507), the asset manifest (375), `white.dds` (152) and Emscripten's file
table (52). Merged string constants are every object's literals after the
linker deduplicates them; the map does not say whose they are, and their
+6,830 since v0.1.0 is not split further here.

## Method

- Emscripten 6.0.12 from `C:\Tools\emsdk`, under vcvars, for every tag. Each
  commit was checked out with `git worktree add --detach` under this worktree's
  `out/`, configured with `cmake --preset wasm-release -B out/build/plain
  "-DCMAKE_EXE_LINKER_FLAGS="` and built with
  `cmake --build out/build/plain --target LineSweeperModule --parallel 8`. The
  shipped sizes are from that link. The worktrees were removed afterwards.
- The same build directory was then reconfigured with
  `"-DCMAKE_EXE_LINKER_FLAGS=--profiling-funcs -Wl,--Map=linesweeper.map"`,
  which relinks only. `--profiling-funcs` keeps a `name` section (113,710
  bytes at v0.1.0) and the map lists every input function with its object.
  For every tag the named and shipped modules have identical section sizes,
  the same number of functions and the same multiset of function-body sizes;
  their function order differs. The attribution therefore describes the
  shipped code.
- A Python reader, not committed, walked the named module's code section and
  `name` section and looked each final function name up in the map's `CODE`
  part. Every function matched by name. Archive members that share a file name
  (`render_resources.cpp`, `renderer.cpp` in `render/` and `render/gl/`) were
  told apart by which object file in the build tree defines the mangled symbol
  (`llvm-nm --defined-only`). A name defined in objects with different owners
  (260 bytes at the tags, 95 at `14464191`, `__cxx_global_array_dtor` and
  similar) counts under the first.
- Attribution is after optimisation. A function `wasm-opt` inlined counts in
  the function it landed in, so the split between `png_file.cpp` and lodepng
  is not meaningful and their sum is. Functions it merged count once.
- Not measured here: the loader's contents, the effect of any flag, and any
  download or start-up time.
