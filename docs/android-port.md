# The Android port — what it costs, and in what order

> Scoping only, 2026-08-21, read against the tree at `f6e58be`. **Nothing here
> has been built.** Every claim about this repository was checked by reading it
> and is cited by file and line; every claim about Android is general knowledge
> and is marked where it carries weight. §1 exists to say which is which.

The question this answers is not "should Labrador run on Android" — that is
decided, and the reason is that mobile is where 2D games are. It is **what the
port actually costs, which parts are load-bearing, and what order they have to
happen in**, because the intuitive order is wrong: the renderer is the part
everyone thinks of and it is neither the largest item nor the riskiest.

This file is flat rather than a folder for the same reason `app/window.{h,cpp}`
is not in `app/win32/` yet ([ARCHITECTURE.md:203-204](design/ARCHITECTURE.md#L203-L204)):
there is one document, and inventing the folder before there is a second is the
speculative structure T1 rules out. When the probe in §3.1 produces its own
file, this becomes `docs/port/`.

Sizes mean what `docs/review/round-2/PLAN.md` made them mean: **hours** ≈ an
afternoon, **days** ≈ 2–4 days, **week** ≈ 5 working days, **weeks** ≈ 2 or
more.

**Contents**

1. [What was checked, and what was not](#1-what-was-checked-and-what-was-not)
2. [The claim this port tests](#2-the-claim-this-port-tests)
3. [The work items](#3-the-work-items)
4. [The dependency spine](#4-the-dependency-spine)
5. [What Vulkan buys, and what it does not](#5-what-vulkan-buys-and-what-it-does-not)
6. [Out of scope, on purpose](#6-out-of-scope-on-purpose)
7. [The documents this port amends](#7-the-documents-this-port-amends)

---

## 1. What was checked, and what was not

**Checked, by reading this tree.** Every file and line cited below. The Windows
surface outside the render backends is **six files**: `app/application.{h,cpp}`,
`app/window.h`, `core/thread_pool.h`, `core/throw_if_failed.h`,
`render/text_encoding.cpp`. That is a `grep` result minus one false positive
worth keeping — `core/registry.h` matches on `wrl`, and it matches because
[`:14-17`](../engine/core/registry.h#L14-L17) explains that the D3D-facing
specialisation lives where COM is already in scope "rather than dragging
`<wrl/client.h>` in here." `core` has refused this once already, deliberately,
which is the standard §3.6 holds the other two `core` files to. Module sizes are
`wc -l`. The audio finding in §3.2 is a read of `audio/sound_bank.h` and nothing
else — it did not need anything else.

**Not checked, and load-bearing.** Whether ETC2 or ASTC is the right target
format for the content that exists; what fraction of the install base a Vulkan
floor gives up against a GLES 3.0 one; whether MoltenVK's Vulkan subset covers
this seam (it almost certainly does — this engine draws textured quads with one
blend state — but "almost certainly" is not a measurement); and every number in
the *sizes* column, which are estimates from reading, not from doing. §3.1
exists because the first of those can be settled cheaply and changes the shape
of everything after it.

**Not checked, and not load-bearing.** Any of it on a device. There is no
device, no NDK toolchain and no emulator in this repository today, and the first
item on the spine deliberately does not need one.

---

## 2. The claim this port tests

Two documents make the same promise in the same words. **Platform-specific code
lives at the edge behind engine-owned interfaces, so that a second platform is
an addition, not a rewrite** —
[PHILOSOPHY.md:303-305](design/PHILOSOPHY.md#L303-L305),
[ARCHITECTURE.md:194-197](design/ARCHITECTURE.md#L194-L197).

That claim has never been cashed. Four render backends test one *half* of it —
that the render seam holds against four APIs, most recently one where the engine
owns synchronisation — but all four sit behind the same Win32 shell, poll the
same XInput, load the same DirectXTK audio and are compiled by the same MSVC
command line. **A second graphics API is not a second platform**, and the port
is the first thing that puts the whole sentence under load.

It is worth writing down now, before the work, what the answer looks like:

- **Where the claim holds outright.** `render/` — the seam was cut four times
  and a fifth costs a folder ([§3.5](#35-the-vulkan-backend)). `input/xinput/`
  — the folder exists and the module table forbids XInput outside it
  ([ARCHITECTURE.md:263](design/ARCHITECTURE.md#L263)).
- **Where it holds but has not been demonstrated.** The window. ARCHITECTURE
  already commits to the exact move and to its cost — `app/window.{h,cpp}`
  drops into `app/win32/` "without renaming the class or touching a call site"
  ([:203-204](design/ARCHITECTURE.md#L203-L204)). The port is what checks that
  sentence.
- **Where it is false today.** `audio/`. See §3.2. This is the finding of this
  document.

---

## 3. The work items

Ordered by dependency in §4, not by size. Sizes here are per item in isolation.

### 3.1 The content probe — *hours*

**Do this first, before a line of anything else.** It is the cheapest item on
the list and the only one that can invalidate the rest.

[`texture_format.h`](../engine/render/texture_format.h) already states the
problem, in a paragraph written for a different reason: of the 45 `.dds` files
across this repository and its client, **43 are BC3 and two are uncompressed**,
every font atlas is **BC2**, and the file says of block compression that it is
*"universally present on a desktop driver and absent from GLES 3.0 entirely."*
Android GPUs are the case that sentence was warning about. ETC2 is the
guaranteed floor on GLES 3.0 and on Android Vulkan; ASTC is the modern one; BC
is neither, on most of the market.

So on the day a Vulkan backend first runs on a phone, **there is no art and no
text.** That is not a backend problem. It is a format, a reader and a re-encode,
and all three are shared engine code tested headlessly.

Two things make it worse than a format addition, and both are already written
down as deliberate decisions:

- [`dds_file.h:20-25`](../engine/render/dds_file.h#L20-L25) names **the DX10
  extended header** among the things it deliberately does not read. That header
  is exactly how a `.dds` would carry ASTC. The reader's stated non-goal is on
  the critical path, which is the good case for a stated non-goal — it is a
  decision to revisit rather than a gap to discover.
- The font atlas is `MakeSpriteFont`'s output and is BC2 because that tool wrote
  it. A second atlas format is a second tool or a transcode step, not a flag.

**What the probe answers, and it is three questions.** Does a second compressed
format fit `TextureFormat` and `TextureData` as an enumerator plus a block size,
or does it change their shape? Does the content stay in `.dds` via the DX10
header, or move to a container (KTX2) — which is a second reader beside
`dds_file.h`, not a change to it? And is the re-encode good enough at the sizes
this content is actually drawn at? T9 buys format edges, so a transcoder is
purchasable; the reader is not, by the same rule that produced `dds_file.h` in
the first place.

### 3.2 The audio seam that is not there — *week*

**This is the one place the second-platform claim is provably false today, and
it is false in a way a folder move does not fix.**

[`audio/sound_bank.h:3`](../engine/audio/sound_bank.h#L3) includes `<Audio.h>` —
DirectXTK's, so XAudio2's — in a **public engine header**. It is not an
implementation detail that leaked: the public API is *spelled* in the library's
types. `SoundBank`'s constructor takes a `std::unique_ptr<DirectX::WaveBank>`
([:28-29](../engine/audio/sound_bank.h#L28-L29)), and `EffectHandle` is
`Registry<DirectX::SoundEffectInstance>::handle`
([:25](../engine/audio/sound_bank.h#L25)) — a `DirectX::` type in the handle a
game holds.

There is no `audio/xaudio2/`. The module table promises one in effect — *"`audio`
| core, math — the audio backend at its edge only"*
([ARCHITECTURE.md:264](design/ARCHITECTURE.md#L264)) — and the folder listing
promises nothing more specific than "playback, mixing" (`:153`). Compare
`render`, where the same table names the API *and the folder it is confined to*,
four times. Audio got the sentence without the structure.

The consequence, stated plainly: **`render/` gains a backend and `audio/` gets
rewritten.** The engine-side type has to stop being DirectXTK's before there is
anything for an Android backend to implement — and the `.xwb` wave bank is an
XACT container with no Android reader, so the content moves too. This is the
item most likely to be underestimated, because it is small (497 lines plus an
80-line loader) and looks like a port.

It is also the item with the clearest precedent. `dds_file.h` exists because
`CreateDDSTextureFromFile` put "what this engine's content actually is" inside a
library that exists for one API, and the fix was to write the format down. This
is the same shape, one module over.

### 3.3 Input — the asymmetry runs backwards — *week*

[ARCHITECTURE.md:208-226](design/ARCHITECTURE.md#L208-L226) documents an
asymmetry it is careful to say is not an inconsistency. A gamepad is **read**:
`input/xinput/` asks for a complete snapshot whenever `Gamepads::poll` wants
one, and owes the window nothing. A keyboard and a mouse are **fed**: they exist
only as messages, the flow is `app → input` and never the reverse, and typed
text "cannot be rebuilt from device state at any price."

**On Android every device is fed.** Gamepads arrive as input events through the
same queue as touch. So `GamepadReader`
([`input/gamepad_reader.h`](../engine/input/gamepad_reader.h), 61 lines) is a
*pull* interface on a platform that only pushes, and the one device the
architecture singles out as owing the window nothing is the one whose seam does
not survive. That is a re-cut, not a second implementation — and it is the
finding worth having early, because `xinput/` is otherwise the tidiest platform
folder in the tree.

Two more, both smaller than they look:

- **Mouse has no analogue.** 392 lines across `mouse.{h,cpp}`. Touch is not a
  mouse — no hover, no persistent position, N simultaneous points — and the
  wheel, which `ARCHITECTURE.md:217-218` already flags as a deltas-only device,
  has none at all. What saves this is that **`ui/` does not depend on `input`**:
  no file in `engine/ui/` includes anything from `engine/input/` (checked). The
  module table permits the dependency and the code declines it, so the widgets
  survive whatever replaces the pointer.
- **The known-absent item stops being speculative.** `CLAUDE.md` lists exactly
  one deliberate absence: an action-mapping layer, refused because "neither
  client has a rebinding screen, so a binding table would be the speculative
  framework T1 rules out." Touch and gamepad are two genuinely different device
  shapes for the same actions, on the same build. That is the real client T1
  asks for. **The port promotes the one known-absent item into a required one**,
  and that should be stated in the commit that lands it rather than discovered.

### 3.4 The shell — *week*

`app/` is 1,800 lines: `window.{h,cpp}` at 906, `application.{h,cpp}` at 894.
The move itself is already specified ([§2](#2-the-claim-this-port-tests)) and
should cost hours. What costs a week is the lifecycle underneath it.

A Win32 program owns its window until it destroys it. An Android activity does
not: the surface can be destroyed and recreated under a running process, on
rotation, on backgrounding, on a phone call. **The seam already has a shape for
this and it is called `DeviceNotify`** —
[`renderer.h:239-245`](../engine/render/renderer.h#L239-L245), `on_device_lost`
/ `on_device_restored`. Two of four backends currently use it and two never can
(a WGL context is not lost; the null backend has nothing to lose), which is the
open question `renderer.h:505-517` is still holding. Android is the platform
where surface loss is **routine rather than exceptional**, and that is the fact
that should settle where the hazard lives — see §4, because it argues for
settling it before the fifth backend rather than after.

The resize path is the other half and it is live: five commits in two days
(`d31a804`, `6ae4a15`, `f473505`, `30a0353`, `06da146`) are all fighting the
mid-frame-resize term, and
[`renderer.h:330-338`](../engine/render/renderer.h#L330-L338) states a contract
written entirely in terms of a caller who already knows the size changed. See
§5 — this is the term Vulkan stresses.

### 3.5 The Vulkan backend — *weeks*

A fifth folder under `render/`, the same four translation units the other three
non-null backends have. `d3d12/` is 3,010 lines and is the fair comparison;
Vulkan is the larger surface, but a meaningful part of `d3d12/`'s cost was the
synchronisation model, and that model is now written down and tested rather than
being discovered.

What it needs that this tree does not have: SPIR-V at build time. `fxc` came
free with the Visual Studio install and `CLAUDE.md` says so; `glslc` or `dxc`
does not, which changes a stated property of the toolchain in
`CMakePresets.json`, in `cmake/compile_shaders.cmake` and in CI. It is a tax,
not an obstacle, and it should be written down as one.

**This is the only item on the list that is verifiable in this repository
today** — against `RenderPixelTests`, against the same `tests/render/golden/`
images the other three rasterisers are held to, with validation layers and a
software ICD. Every other item needs a device and a toolchain that does not
exist yet. That is a strong argument for its position on the spine and a weak
one for its position in the *plan*: it is where the confidence is, not where the
risk is.

### 3.6 Build and core — *days*

Small, mechanical, and it blocks compiling anything.

- [`cmake/settings.cmake`](../cmake/settings.cmake) is the single `INTERFACE`
  target every real target links, and it is **entirely MSVC**: `/W4 /WX
  /permissive- /sdl /fp:precise`, plus `UNICODE _UNICODE WIN32 _WINDOWS NOMINMAX
  WIN32_LEAN_AND_MEAN` handed to every translation unit in the tree. The clang
  equivalents are known; `/fp:precise` is the one to get right rather than
  approximate, and the file's own comment says why — exact `operator==` against
  `Vector2F::ZERO`, tolerance assumptions, NaN propagation. `-ffp-model` or
  `-ffp-contract=off` is a decision, not a translation, and it should be argued
  in that file the way `/fp:precise` already is.
- `CMakePresets.json` binds Ninja, vcpkg and `x64` with `architecture.strategy`
  `external`. An `arm64-v8a` NDK preset is a second toolchain file and a second
  triplet.
- **`windows.h` in `core/`.** `core/thread_pool.h:3` and
  `core/throw_if_failed.h:3`. The second is worse than an include: it is
  `HRESULT`-based, which is a Direct3D concept sitting in the one module
  everything is allowed to lean on. It has to move regardless of Android, and
  the port is the occasion.
- **`std::wstring` on the render API.** `render/text_encoding.cpp` uses
  `MultiByteToWideChar`, which is a one-function replacement. The decision
  underneath it is not:
  [`text_encoding.h:9-18`](../engine/render/text_encoding.h#L9-L18) says the
  render module holds text as `std::wstring` because "a glyph table is keyed by
  code unit" and converting on the draw path is the alternative. On Windows a
  code unit is 16 bits. **On Android `wchar_t` is 32 bits**, so the glyph
  table's key changes width and the atlas's coverage assumption changes with it.
  This is not a blocker and it is not free; `char16_t` is the obvious answer and
  it touches `font.h`, `text_object.h` and both file readers.

---

## 4. The dependency spine

```
3.1 content probe ──────────────┐   (hours; nothing depends on it, everything is shaped by it)
                                │
3.6 build + core ───────────────┤   (days; nothing compiles for the target without it)
                                │
        ┌───────────────────────┴──────────────────┐
        │                                          │
3.5 Vulkan backend                        3.2 audio ── 3.3 input ── 3.4 shell
(verifiable on Windows, now)              (all three need a device to finish)
```

The shape that matters: **the left branch can start today and finish on this
desktop; the right branch cannot be finished without hardware.** That is why
Vulkan is a reasonable first *build* even though the content probe is the first
*decision*.

One ordering claim is worth arguing rather than asserting. **Settle
`AssetKind::reload_device` before the fifth backend, not after.**
[`renderer.h:505-517`](../engine/render/renderer.h#L505-L517) holds it open and
records that a fourth backend "answered half of" it: device loss belongs to two
of four backends rather than to one, and what is left is which of three places
the hazard goes. Vulkan has `VK_ERROR_DEVICE_LOST` and lands on the same side,
making it three of five — and §3.4 says Android is where surface loss stops
being exceptional. A question that gets harder to answer with every backend
added to it, and whose answer changes when the platform arrives, should be
answered in the gap between the two. That gap is now.

---

## 5. What Vulkan buys, and what it does not

**It buys the platforms, and probably all of them at once.** Vulkan is the
native modern API on Android and Linux, and MoltenVK is how most cross-platform
engines reach macOS and iOS without a Metal backend. For an engine that draws
textured quads with one blend state and two samplers, the subset MoltenVK does
not implement is not a subset this seam touches — see §1, this is the one
unmeasured claim in this document that would cost real work if it is wrong. **If
it holds, "Metal eventually" is a build target rather than a sixth backend.**

**It buys one untested seam claim, and it is a live one.** Resize reaches the
renderer exactly one way today: `WM_SIZE` →
[`window.cpp:410`](../engine/app/window.cpp#L410) →
[`application.cpp:411`](../engine/app/application.cpp#L411) →
`Renderer::window_size_changed`. The window tells the renderer; the renderer has
no path to originate it. All four current backends are content with that because
all four learn about a resize from Win32. **Vulkan is the first API where the
presentation engine tells you instead** — `VK_ERROR_OUT_OF_DATE_KHR` comes back
from acquire or from present, with no window message anywhere near it, on a
frame Win32 never flagged. Given that five commits in two days are fighting
this exact term, the fifth backend is the one that asks whether the contract at
`renderer.h:330-338` has the right shape.

**It does not buy a CI rasteriser for free.** `d3d12/` earned its place partly
on WARP: a GPU-less runner can rasterise Direct3D, and CI checks the pixel
contract twice because of it
([`d3d12/backend.h`](../engine/render/d3d12/backend.h) states the claim). Vulkan
has no in-box equivalent on a Windows runner — it is lavapipe or SwiftShader,
installed. That is a third CI install burden or a preset untested the way the GL
one is, and it should be decided rather than discovered.

**It does not buy Android.** Stated once more because it is the thing this
document exists to say: on the spine above, the Vulkan backend is one of five
items and the only one that runs on hardware this repository already has.

---

## 6. Out of scope, on purpose

Named so the absence is deliberate, in `PHILOSOPHY.md`'s own style:

- **iOS, in this pass.** MoltenVK makes it cheap *after* Android; doing both at
  once means two lifecycles and two content pipelines under one unproven claim.
- **A GLES backend as a hedge.** The engine has a GL 3.3 core backend and the
  temptation is to say GLES 3.0 is nearly that. It is not — profile, shading
  language version and, decisively, the texture formats in §3.1 — and a fifth
  and sixth backend to reach one platform is the cost Vulkan exists to avoid.
- **Touch-first UI, input latency, thermal budget, APK packaging, store
  compliance.** All real, none of them the port. This document ends at "the
  engine runs."
- **Everything already permanent.** Online play, 3D, an editor, a scripting
  layer — `PHILOSOPHY.md`, Non-goals. Android changes none of them.

---

## 7. The documents this port amends

Recorded here so the amendments happen in the commits that earn them, per the
rule that `docs/design/` changes in the same commit as the change that fights
it:

| Document | What changes |
|---|---|
| `CLAUDE.md:1` | "Windows-only" |
| `CLAUDE.md`, Known-absent | The action-mapping layer acquires its client (§3.3) |
| `ARCHITECTURE.md:194-204` | The window moves to `app/win32/`; the folder stops being speculative |
| `ARCHITECTURE.md:264` | `audio`'s row gains the folder the sentence already implies (§3.2) |
| `PHILOSOPHY.md:303-305` | "a second platform is an addition, not a rewrite" — amend with where it held and where it did not |
| `PHILOSOPHY.md:454-462` | "a second platform's" backend stops being hypothetical |
| `renderer.h:505-517` | `AssetKind::reload_device` — settled, per §4 |

**None of these should be amended in advance.** A design document that describes
a port that has not happened is the speculative framework this tree refuses, and
it would be the second time a comment in `render/` was written in the future
tense of a port. The first is on record and was corrected:
`texture_format.h`'s block-compression paragraph was written for a GL backend
that had not shipped, `docs/review/backend-equivalence/DRIFT.md` caught it, and
`a56d198` fixed it — [`:34-42`](../engine/render/texture_format.h#L34-L42) now
says "that was written before the port and the port has happened" in as many
words, which is why it is quotable in §3.1 at all.
