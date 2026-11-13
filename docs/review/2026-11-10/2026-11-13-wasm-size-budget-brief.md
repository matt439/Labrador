# A size budget for linesweeper.wasm: the brief, 2026-11-13

The first step of `REV#53`: the module's raw and Brotli sizes, its growth
from tag to tag, and three candidate budget figures, each with what it would
have refused in the recorded history and what it costs. Nothing was built for
this brief. Every size comes from
[the attribution record of 2026-11-13](../2026-11-12-work-audit/2026-11-13-wasm-size-attribution.md)
(`REV#155`) or from a record cited where it is used. No budget is added to
`tools/web_bundle/bundle.mjs`; which figure, if any, is the owner's call.
`REV#100`'s Brotli tolerance and `REV#102`'s flag experiments wait on that
call.

## Sizes and growth

`wasm-release`, Emscripten 6.0.12, `linesweeper.wasm` and `linesweeper.js` as
`linesweeper_web_dist` ships them. Brotli is Node's at quality 11, as
`bundle.mjs` writes it to `BUILD.json`. "To play" is the module plus the
loader under Brotli, what a visitor fetches after the click when the host
serves the bundle's own compression.

| Build | Date | Module | Brotli | Loader | Brotli | To play, Brotli |
|---|---|---:|---:|---:|---:|---:|
| v0.1.0 (`c369e8b9`) | 2026-10-13 | 403,759 | 125,384 | 118,982 | 27,283 | 152,667 |
| v0.2.0 (`1e60fb26`) | 2026-10-25 | 411,204 | 127,831 | 118,982 | 27,283 | 155,114 |
| v0.3.0 (`0f4f5033`) | 2026-11-03 | 426,100 | 132,288 | 120,545 | 27,753 | 160,041 |
| `14464191` | 2026-11-13 | 487,806 | 150,968 | 121,144 | 27,839 | 178,807 |

Two records of 2026-11-05 sit between the last two rows: a bundle module of
476,013 bytes (`docs/port/web-to-be-tested.md`, the Safari and Firefox runs)
and a local bundle reading 148,215 under Brotli (`REV#53`'s finding). They
may not be the same build, so they are not a row.

Growth of the module:

| Step | Raw | % | Brotli | % |
|---|---:|---:|---:|---:|
| v0.1.0 to v0.2.0 | +7,445 | +1.8 | +2,447 | +2.0 |
| v0.2.0 to v0.3.0 | +14,896 | +3.6 | +4,457 | +3.5 |
| v0.3.0 to `14464191` | +61,706 | +14.5 | +18,680 | +14.1 |
| v0.1.0 to `14464191` | +84,047 | +20.8 | +25,584 | +20.4 |

The loader grew by 2,162 bytes (556 under Brotli) over the same span, so the
module is where the growth is. Brotli stays between 30.9 % and 31.1 % of the
raw module at every tag, so a figure in one unit translates to the other.

Since v0.3.0, 40,496 bytes are PNG decoding (`png_file.cpp` and lodepng,
code and data), which LineSweeper never uses: `resource_factory.cpp` chooses
`name.dds` or `name.png` at run time, so the decoder is linked into every
program that loads a texture by name. Another 2,664 are libc++'s filesystem
operations, which that choice also brings in. The other 18,546 bytes of the
step are ordinary engine and sample work, most of it in `app/`, `assets/`
and the linker's merged string constants.

## What the size depends on

- **Toolchain.** CI's `wasm` job runs on `windows-latest` with Emscripten
  pinned at 6.0.12, and `bundle.mjs` compresses with the Node that emsdk
  carries, so both raw and Brotli sizes move only when the pin moves or the
  code does.
- **Host.** The same commit built on WSL 2 gave 487,687 bytes against
  487,799 on Windows
  ([the WSL build record](2026-11-13-wsl-wasm-build.md)); the 112 bytes were
  not traced. A budget checked in CI sees only CI's host.
- **Commit.** `1dc8533c` gives 487,799 and `14464191` 487,806: small commits
  move the module by a few bytes.
- **What a visitor fetches.** The host decides. mattharding.dev's Cloudflare
  compresses the files itself and sends about 183 KB of them
  (`docs/port/web.md` §7.1), a figure taken against an earlier bundle.

## Candidate figures

Each is stated on the raw module, which is what `BUILD.json` records exactly
and the one unit no compressor version moves; the Brotli equivalent at 31 %
is given beside it. The history column treats each figure as if `bundle.mjs`
had checked it from v0.1.0. Only the tagged builds and 2026-11-05 are
measured, so a step that a figure would have refused is located to a range of
commits, not to one commit, except where the attribution names it.

### A. A ceiling with headroom: 512,000 bytes (about 158,700 under Brotli)

Today's module is 24,194 bytes (5.0 %) under it.

- **History.** It refuses nothing recorded. The PNG step would have passed:
  426,100 plus 40,496 is well under it.
- **What it would catch.** One more dependency of lodepng's size, or about
  the ordinary growth since v0.3.0 again (18,546 bytes in ten days) plus a
  little.
- **Trade.** It costs nothing today and needs no file to maintain. It
  catches steps, not creep: at the rate of the last ten days the headroom
  lasts weeks, after which raising it is the only way past and the figure
  says nothing about any one change.

### B. A ratchet: the last accepted size plus 1 % (492,684 bytes today)

The accepted size lives in a tracked file, as `tests/ctest_inventory.json`
holds the ctest entries, and the commit that grows the module past the
tolerance updates the file and says why in its message. A commit that
shrinks it may lower the figure.

- **History.** v0.1.0 to v0.2.0 (+1.8 %) and v0.2.0 to v0.3.0 (+3.6 %)
  would each have needed at least one recorded raise somewhere in their
  range. `a6ae08ff` (2026-11-04, OCA-82, PNG loading) would have been
  refused on its own: PNG decoding alone is 9.5 % of v0.3.0's module.
- **What it would catch.** Any single change of about 4,900 bytes, the size
  of `app/application.cpp`'s growth since v0.3.0 (+4,132) plus a little,
  and every growth past that as a line in a commit message.
- **Trade.** Growth becomes a recorded decision, which is the point, but
  most engine commits touch the module, and a run of small commits can
  each pass while adding up; the file needs a raise every few such runs.
  The tolerance is far above the 112-byte host difference, so it does not
  fail on noise. It does not by itself say that any size is too large.

### C. Back below the PNG step: 450,000 bytes (about 139,500 under Brotli)

- **History.** It passes every tag to v0.3.0 and refuses 2026-11-05 and
  today. It would have refused `a6ae08ff`.
- **What it asks first.** Today's module fails it, so taking it means first
  keeping PNG decoding out of programs that load no PNG. Removing the
  40,496 bytes the attribution assigns to it gives about 447,310, and
  dropping the filesystem call too about 444,646. Both are arithmetic, not a
  build: `wasm-opt` inlines differently once the decoder is gone. Headroom
  after that is about 2,700 to 5,400 bytes.
- **Trade.** It turns the constrained-hardware position into a stated limit,
  and keeping the decoder out saves about 12,500 bytes of a visitor's
  Brotli download, but the budget becomes a code change before it is a
  check. How a game keeps the decoder
  out (a link-time choice instead of the run-time one) touches the asset
  path every program uses, and may be consumer-visible under the
  CHANGES.md rule. With that little headroom, the ratchet's question comes
  back within weeks.

The figures combine: C or A as the ceiling a release must stay under, and B
as the per-commit record beneath it.

## What follows from each

- **Where a check would sit.** `bundle.mjs` already reads every file's raw
  and Brotli size in CI's `wasm` job; a comparison there fails that job, as
  the finding proposes. No new job or ctest entry is needed.
- **`REV#100`.** A raw figure needs no tolerance for compressor drift. A
  Brotli figure needs one only if the emsdk pin moves, since the Node that
  compresses comes with it.
- **`REV#102`.** Closure, emmalloc, `-Os`/`-Oz` and LTO each give a delta
  to set against whichever figure is chosen. dlmalloc is 9,337 bytes of
  code, the most an allocator swap can remove; Closure acts on the loader
  only. Under C those experiments are a second route to the figure, besides
  keeping the PNG decoder out.
- **The loader.** It has grown 1.8 % since v0.1.0 and is about 16 % of the
  Brotli download to play. A budget can cover the module alone or the sum;
  the sum is what a visitor fetches.

## Not covered

No figure here was checked in a build, and no download, start-up or memory
time was measured. The per-commit sizes between tags are not measured, so
the history column for B locates raises to ranges. The other five sample
modules and the starter's page have no recorded series and are not
proposed for a budget here.
