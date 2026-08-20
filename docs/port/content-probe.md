# The content probe — can the art land on the target

> Run 2026-08-21 against Labrador at `f6e58be` and ColourWars at its own working
> tree. This answers the three questions [android.md §3.1](android.md#31-the-content-probe--hours)
> asked and nothing else. **Every number here was measured** — DDS and
> `.spritefont` headers were parsed, not estimated — and §5 lists the two claims
> that are still knowledge rather than measurement.

**Verdict: the content is not the blocker, and the probe found the reason the
scoping document expected it to be one was itself out of date.** ETC2 costs six
one-line additions and zero bytes. The container has to change and `.dds` is not
available at any price. The fonts are the awkward part, and the awkwardness is a
tool, not a format.

**Contents**

1. [What the content actually is](#1-what-the-content-actually-is)
2. [Q1 — does a second format fit the shape](#2-q1--does-a-second-format-fit-the-shape)
3. [Q2 — does the content stay in `.dds`](#3-q2--does-the-content-stay-in-dds)
4. [Q3 — is the re-encode good enough](#4-q3--is-the-re-encode-good-enough)
5. [What is still unmeasured](#5-what-is-still-unmeasured)
6. [What this changes in the plan](#6-what-this-changes-in-the-plan)

---

## 1. What the content actually is

**`texture_format.h` is stale by two, and the shape of its claim is right.** It
says *"there are 45 .dds files … 43 block-compressed, all DXT4 or DXT5 and so all
bc3_unorm; and two uncompressed, both b8g8r8a8_unorm."* Parsing every header in
both trees, excluding build output and the vendored engine:

| | count |
|---|---|
| ColourWars `game/`, DXT4 | 14 |
| ColourWars `game/`, DXT5 | 27 |
| Labrador, uncompressed | 2 |
| **total** | **43** |

So it is **41 block-compressed and two uncompressed, 43 in all**. The
interesting half of the sentence — *all* DXT4 or DXT5, therefore all
`bc3_unorm` — is exactly right, and 14 + 27 is why. The count is wrong by two
and was wrong before this probe; `a56d198` and `e92a982` both corrected this
file and neither caught it.

**Its neighbour already disagrees with it, and its neighbour is right.**
[`texture_data.h:53`](../../engine/render/texture_data.h#L53) says *"all
forty-three .dds between them are single-level"* — 43, the measured total — while
`texture_format.h:20` says 45 four lines of code away, in the same folder, about
the same files. That is not drift against the code, which is what
`docs/review/backend-equivalence/DRIFT.md` collects; it is two engine headers
contradicting each other, which nothing in this tree currently looks for. The
single-level claim itself holds: every one of the 41 carries `mipMapCount` 1 and
Labrador's two carry 0.

**The far more useful number is that almost none of it is loaded.**
`game/content/manifest.json` names **one** sprite sheet and **seven** fonts. The
other 40 `.dds` live in a directory called `old textures` and are referenced by
nothing; 28 of the 35 `.spritefont` files are likewise unreferenced, which
`texture_format.h` already says of fonts (*"most of them loaded by nothing"*)
and does not say of textures.

**The whole live texture budget, measured:**

| Asset | Format | Pixels | Bytes |
|---|---|---|---|
| `sprite_sheet_1.dds` | BC3 (fourCC `DXT4`) | 1500x1512 | 2,268,000 |
| `gill_sans_ultra_bold_144` | BC2 | 2048x852 | 1,744,896 |
| `gill_sans_mt_bold_144` | BC2 | 2048x616 | 1,261,568 |
| `gill_sans_mt_bold_72` | BC2 | 1024x324 | 331,776 |
| `gill_sans_mt_bold_48` | BC2 | 512x300 | 153,600 |
| `gill_sans_mt_bold_36` | BC2 | 512x172 | 88,064 |
| `gill_sans_mt_bold_24` | BC2 | 256x168 | 43,008 |
| `courier_new_bold_16` | BC2 | 128x144 | 18,432 |
| **live total** | | | **5,909,344** (5.64 MB) |

Labrador's own content is `white.dds` (1x1, uncompressed, 4 bytes), `quad.dds`
(2x2, uncompressed, 16 bytes) and three copies of the 18 KB courier atlas. It is
noise, and it is *entirely uncompressed except the font* — which matters in §4,
because `RenderPixelTests` is what an Android build would want to run first and
the only block-compressed thing it touches is that atlas.

**Two files are 83% of the font budget.** The two 144-point atlases are 3.01 MB
of 3.64 MB. Any conversation about font compression on a phone is dominated by
whether a 144-point face at 2048 px belongs on one at all.

---

## 2. Q1 — does a second format fit the shape

**ETC2: yes, exactly, with no shape change at all. ASTC: no.**

The model is [`texture_data.h`](../../engine/render/texture_data.h) plus 60 lines
of [`texture_data.cpp`](../../engine/render/texture_data.cpp), and it is built on
one assumption stated in three places: **a compressed block is 4x4**.
`TextureLevel`'s comment says *"a 'row' is a row of 4x4 blocks"*,
`is_block_compressed`'s declared contract is *"whether `format` stores 4x4 blocks
rather than pixels"*, and `texture_level` writes the 4 twice:

```cpp
const int blocks_wide = (width + 3) / 4;
const int blocks_high = (height + 3) / 4;
```

**ETC2 is a 4x4-block format.** `COMPRESSED_RGBA8_ETC2_EAC` is 16 bytes per 4x4
block — bit-for-bit the same footprint arithmetic as BC2 and BC3, which
`unit_bytes` already returns 16 for. So the whole change is additive:

| File | Change |
|---|---|
| `texture_format.h` | one enumerator |
| `texture_data.cpp`, `unit_bytes` | one `case`, returning 16 |
| `texture_data.cpp`, `is_block_compressed` | one `||` clause |
| `texture_data.cpp`, `texture_level` | **none** — `(w+3)/4` is already right |
| `gl/texture_factory.cpp` | one `case` → `COMPRESSED_RGBA8_ETC2_EAC` |
| `d3d11/`, `d3d12/` `texture_factory.cpp` | one `case` each, throwing by name (T6) |
| `d3d12/texture_factory.cpp`, the name switch | one `case` |
| `null/` | none — it keeps a width and a height and throws the bytes away |

Six one-line additions and one deliberate no-op. **No documented contract
becomes false**, which is the test that matters in this tree: `TextureLevel`'s
paragraph and `is_block_compressed`'s "4x4" stay true of ETC2 word for word.

**ASTC breaks it.** ASTC is always 16 bytes per block, but the block is
4x4 through 12x12 — the footprint is a *property of the chosen variant*, not of
compression. `is_block_compressed` returning a bool that means "4x4" cannot
express that; it becomes a block width and a block height, `texture_level`'s two
constants become two variables, and `texture_data.h`'s specification paragraph —
which is the only written statement of this arithmetic in the tree — gets
rewritten. Then [`sprite_font_file.cpp:118-126`](../../engine/render/sprite_font_file.cpp#L118-L126),
which cross-checks a file's declared stride and rows against `texture_level`,
follows for free.

That is not expensive. It is just not *nothing*, and ETC2 is nothing.

---

## 3. Q2 — does the content stay in `.dds`

**No, and not for the reason the plan assumed.** [android.md §3.1](android.md#31-the-content-probe--hours)
framed this as a choice between carrying ASTC in a `.dds` DX10 header and moving
to KTX2, on the grounds that the DX10 header is what `dds_file.h` deliberately
does not read. The probe closes it: **the DX10 header's format field is a
`DXGI_FORMAT`, and DXGI has no ETC2 value at all.** The blocker is the
container's vocabulary, not its header. Implementing the DX10 header would buy
nothing here.

The same wall, one file over and harder.
[`sprite_font_file.cpp:28-46`](../../engine/render/sprite_font_file.cpp#L28-L46)
reads an atlas format as a DXGI number and says so in a comment that is careful
about exactly this — *"a file format that happens to be spelt in another API's
constants is still a file format"* — and accepts 28, 74 and 115 because
**MakeSpriteFont writes 28, 74 or 115.** A `.spritefont` cannot declare an ETC2
atlas, and the producer is a DirectXTK tool that could not write one if the
container allowed it.

So:

- **The sprite sheet moves container.** KTX2 is the format Khronos defines for
  exactly this, and a second reader beside `dds_file.h` is what T9 permits —
  format edges are the bought side of the line, but `dds_file.h` exists because
  the *reader* was the part worth building. The same judgement applies.
- **The fonts need a decision, not a reader** — see §4.
- **`.dds` stays** for the desktop backends. Nothing here argues for converting
  the Windows content; it argues for a second container beside it.

---

## 4. Q3 — is the re-encode good enough

**For the sprite sheet: yes, and it is not even a re-encode.**
`game/content/textures/` holds `sprite_sheet_1.dds` **and `sprite_sheet_1.png`
and `sprite_sheet_1.xcf`** — the layered GIMP source. So the ETC2 encode is a
fresh encode from source art, not a lossy-to-lossy transcode through BC3, which
was the risk this question existed to find. (The one file in the tree with no
source beside it is `sprite_sheet_1_BC3.dds`, and it is in `old textures` and
loaded by nothing.)

**And it costs nothing.** ETC2 RGBA8 and BC3 are both 16 bytes per 4x4 block:

| Encoding | Blocks | Bytes | vs BC3 |
|---|---|---|---|
| BC3 (today) | 375 x 378 | 2,268,000 | — |
| ETC2 RGBA8 | 375 x 378 | 2,268,000 | **identical** |
| ASTC 4x4 | 375 x 378 | 2,268,000 | identical |
| ASTC 8x8 | 188 x 189 | 568,512 | 4.0x smaller |

The BC3 row is not arithmetic, it is the file: 375 x 378 x 16 + a 128-byte header
is 2,268,128 bytes, which is `sprite_sheet_1.dds` to the byte. On an engine whose
positioning is the low tier, ASTC 8x8 saving 1.7 MB on one texture is a real
argument — and it is the argument that costs the §2 shape change. It should be
made on its own, later, with a device in hand.

**For the fonts: this is the awkward one, and it is a tool problem.** ETC2 RGBA8
is 1 byte per pixel and so is BC2, so an ETC2 atlas would also be byte-identical
— but §3 says no `.spritefont` can declare one. The options, measured:

| Option | Font bytes | Cost |
|---|---|---|
| BC2 today (Windows) | 3,641,344 | — |
| Uncompressed `r8g8b8a8` | 14,565,376 | **+10.9 MB**; MakeSpriteFont writes it today, zero engine change |
| ETC2 in a new container | 3,641,344 | a new atlas tool and a new reader |

**Uncompressed is the honest first move**, because it works today, changes no
engine code, and `r8g8b8a8_unorm` is already an accepted `TextureFormat` every
backend handles. +10.9 MB is not acceptable as a destination on a phone, but it
is entirely acceptable as the state in which the *first* Android frame gets
drawn — and it lets every other item on the port's spine proceed without waiting
on a font pipeline.

And the destination is probably not a new tool at all: **the two 144-point
atlases are 83% of that budget.** Regenerating the font set at sizes a phone
screen actually uses is a content decision that dominates the format decision,
and it is available before either.

---

## 5. What is still unmeasured

Two claims here are knowledge, not measurement, and both are cheap to settle
with a device:

1. **That ETC2 is universally available on the Android Vulkan floor.**
   `textureCompressionETC2` is a core `VkPhysicalDeviceFeatures` flag and is
   set by essentially every Android driver; ETC2 is also mandatory in GLES 3.0.
   This probe did not query a device, because there is no device.
2. **That an ETC2 encode of this particular sprite sheet looks acceptable.**
   Sprite art with hard alpha edges is the case block compression handles worst,
   and ETC2's alpha (EAC) is a different scheme from BC3's. Source art exists, so
   this is an encode and a look, not a research question — but it has not been
   done.

Everything else above is parsed from the files.

---

## 6. What this changes in the plan

- **§3.1 is answered and shrinks.** It was scoped *hours* and it turned out to
  be six one-line additions plus a container decision. The container work — a
  KTX2 reader — is real and belongs on the spine as its own item, roughly
  *days*, and it does not block the Vulkan backend.
- **The order does not change.** The probe was worth running first for exactly
  the reason §3.1 gave: had `.dds` been able to carry ETC2, the answer would have
  been a reader change; had ASTC been required, the answer would have been a
  shape change to the one piece of arithmetic every backend depends on. Neither
  is true, and now that is known rather than assumed.
- **Choose ETC2, not ASTC, for the first pass.** Zero shape change, zero size
  change, and it keeps `texture_data.h`'s specification true. Revisit ASTC 8x8
  when there is a device to measure the 4x saving on.
- **Fonts ship uncompressed for the first Android frame**, and the real fix is
  the atlas sizes rather than the atlas format.
- **One correction is owed to the engine, independent of the port.**
  `texture_format.h` says 45 `.dds` and 43 block-compressed; it is 43 and 41,
  and `texture_data.h:53` says 43 in the same folder. That is a two-line fix to
  a comment that is otherwise right, and it should go in on its own rather than
  riding the port — the port did not cause it and will not fix it.
