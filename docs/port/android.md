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

This file was flat rather than a folder for the same reason `app/window.{h,cpp}`
is not in `app/win32/` yet ([ARCHITECTURE.md:203-204](../design/ARCHITECTURE.md#L203-L204)):
one document, and inventing the folder before there is a second is the
speculative structure T1 rules out. The condition it set — *"when the probe in
§3.1 produces its own file, this becomes `docs/port/`"* — was met the same day,
by [content-probe.md](content-probe.md). It is a folder now, and it became one
by the rule rather than in advance of it.

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

> **Amended 2026-08-21, and the paragraph below is left as written.** Two of
> those six files are gone and one the `grep` could not see has turned up, so
> the Windows surface outside the render backends is now **four files, none of
> them in `core/`**: `app/application.{h,cpp}`, `app/window.h`,
> `render/throw_if_failed.h`, `render/text_encoding.cpp`. That is §3.6's third
> bullet, discharged — see the amendment at the head of that section for what
> moved and why it was the only one of its four that could go first.
>
> **The blind spot is worth recording, because it is the method's and not this
> one file's.** [`core/step_timer.h`](../../engine/core/step_timer.h) uses
> `QueryPerformanceFrequency`, `QueryPerformanceCounter` and `LARGE_INTEGER`
> and includes nothing that declares any of them: it compiles because its only
> includer, `app/application.h`, has already pulled `<Windows.h>` in through
> `app/window.h` five lines above it. A search for the include cannot find a
> file that does not have one. So `core/` still has a Windows dependency after
> this, and the honest fix for it points the wrong way — making that file
> include what it uses would add a `<windows.h>` to `core/` on the day two of
> them left. What it actually wants is `std::chrono::steady_clock`, which is a
> change of substance to a file `NOTICE` carries as Microsoft's and a change to
> fixed-step timing besides, so it is named here rather than done.

**Checked, by reading this tree.** Every file and line cited below. The Windows
surface outside the render backends is **six files**: `app/application.{h,cpp}`,
`app/window.h`, `core/thread_pool.h`, `core/throw_if_failed.h`,
`render/text_encoding.cpp`. That is a `grep` result minus one false positive
worth keeping — `core/registry.h` matches on `wrl`, and it matches because
[`:14-17`](../../engine/core/registry.h#L14-L17) explains that the D3D-facing
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
[PHILOSOPHY.md:303-305](../design/PHILOSOPHY.md#L303-L305),
[ARCHITECTURE.md:194-197](../design/ARCHITECTURE.md#L194-L197).

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
  ([ARCHITECTURE.md:263](../design/ARCHITECTURE.md#L263)).
- **Where it holds but has not been demonstrated.** The window. ARCHITECTURE
  already commits to the exact move and to its cost — `app/window.{h,cpp}`
  drops into `app/win32/` "without renaming the class or touching a call site"
  ([:203-204](../design/ARCHITECTURE.md#L203-L204)). The port is what checks that
  sentence.
- ~~**Where it is false today.** `audio/`. See §3.2. This is the finding of this
  document.~~ **Answered 2026-08-27, and by this repository rather than by a
  port.** `engine/audio/audio_device.h` is the seam, `audio/xaudio2/` and
  `audio/null/` are behind it, and the include check that guards a backend
  folder covers `audio/` as well as `render/`. So this bullet is now the first
  kind — the claim holds outright — and what is left of §3.2 is one backend
  against a seam that already exists, which is what `render/` costs. The
  finding of this document has been paid; see the amendment at the head of
  §3.2 for what it turned out to be worth and what it did not settle.

---

## 3. The work items

Ordered by dependency in §4, not by size. Sizes here are per item in isolation.

### 3.1 The content probe — *hours* — **RUN, see [content-probe.md](content-probe.md)**

> **Answered 2026-08-21.** ETC2 fits the existing shape exactly — six one-line
> additions, no documented contract broken, and byte-for-byte the same size as
> BC3. ASTC would change the shape and is deferred. `.dds` **cannot** carry ETC2
> at any header version, because the container's format vocabulary is DXGI and
> DXGI has no ETC2 number — so the sprite sheet moves to a second container and
> the DX10-header question below is closed, not open. Source art exists for the
> one live sprite sheet, so it is a fresh encode rather than a transcode. Fonts
> are the awkward part and it is a tool problem. **The section below is left as
> written**, including the count it got from `texture_format.h` and which the
> probe found to be stale by two (it is 43 `.dds` and 41 block-compressed, not
> 45 and 43).

**Do this first, before a line of anything else.** It is the cheapest item on
the list and the only one that can invalidate the rest.

[`texture_format.h`](../../engine/render/texture_format.h) already states the
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

- [`dds_file.h:20-25`](../../engine/render/dds_file.h#L20-L25) names **the DX10
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

### 3.2 The audio seam that is not there — *week* — **BUILT, 2026-08-27**

> **Amended 2026-08-27. The paragraphs below are left as written**, the way §1
> and §3.1 are, because what they found was right and the record of a finding
> is worth more than a tidy page.
>
> **The seam exists.** `engine/audio/audio_device.h` is a concrete class chosen
> by `LABRADOR_AUDIO_BACKEND`, with `audio/xaudio2/` behind it and
> `audio/null/` — which records what it was asked to play — as the headless
> implementation `PHILOSOPHY.md:632-637` requires. All four public headers this
> section names have lost `<Audio.h>`; `SoundBank` is built from a handle
> rather than a `DirectX::WaveBank`; `EffectHandle` and `SoundState` are
> engine types; and `cmake/check_engine_includes.cmake` captures the module as
> well as the backend, so a file outside `engine/audio/<backend>/` naming a
> header inside it fails the build exactly as it does for `render/`.
>
> **It did not land for this document's reason, and that is worth recording.**
> It landed for `docs/survey/2026-08-26.md` §3.4b, whose argument is about
> testability rather than about a second platform: a seam with only the
> platform's own implementation behind it cannot be constructed without the
> platform, so eight of `SoundBank`'s thirteen instance methods and five sites
> of level clamping were code no test in this repository could reach. The
> port's requirement and the test's requirement turned out to be one piece of
> work, which is the cheapest way for an item on this spine to be discharged
> and was not predicted here.
>
> **What is left of this item, and it is the smaller half.** One backend
> against an existing seam — `audio/aaudio/` or `audio/opensles/`, sixteen
> methods, no engine change — which is now the same shape as §3.5 and can be
> costed the same way. Nothing above the seam has to move.
>
> **What is NOT settled is the sentence below about the container**, and it
> was deliberately left open rather than decided in passing. The seam is cut
> *above* the format: `open_wave_bank` takes a directory and a bank name,
> never a file name, so the extension and the reader belong to the backend —
> an `.xwb` on `xaudio2/`, and on `null/` the wave-name list the definition
> JSON supplies, because that JSON is content the engine parses and the
> container is not. So an Android backend still needs a container it can read,
> and the in-doctrine answer is still the one below: write the format down, an
> `xwb_file.h` beside `dds_file.h`. **That is blocked on something this
> repository does not have** — there is no `.xwb` in this tree to write a
> reader against, for the same undistributable-source-audio reason the shipped
> manifest marks the bank optional. `docs/survey/2026-08-26.md` §6's fourth
> decision is therefore still unmade, and it is now the whole of the audio
> question rather than half of it.

**This is the one place the second-platform claim is provably false today, and
it is false in a way a folder move does not fix.**

[`audio/sound_bank.h:3`](../../engine/audio/sound_bank.h#L3) includes `<Audio.h>` —
DirectXTK's, so XAudio2's — in a **public engine header**. It is not an
implementation detail that leaked: the public API is *spelled* in the library's
types. `SoundBank`'s constructor takes a `std::unique_ptr<DirectX::WaveBank>`
([:28-29](../../engine/audio/sound_bank.h#L28-L29)), and `EffectHandle` is
`Registry<DirectX::SoundEffectInstance>::handle`
([:25](../../engine/audio/sound_bank.h#L25)) — a `DirectX::` type in the handle a
game holds.

There is no `audio/xaudio2/`. The module table promises one in effect — *"`audio`
| core, math — the audio backend at its edge only"*
([ARCHITECTURE.md:264](../design/ARCHITECTURE.md#L264)) — and the folder listing
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

[ARCHITECTURE.md:208-226](../design/ARCHITECTURE.md#L208-L226) documents an
asymmetry it is careful to say is not an inconsistency. A gamepad is **read**:
`input/xinput/` asks for a complete snapshot whenever `Gamepads::poll` wants
one, and owes the window nothing. A keyboard and a mouse are **fed**: they exist
only as messages, the flow is `app → input` and never the reverse, and typed
text "cannot be rebuilt from device state at any price."

**On Android every device is fed.** Gamepads arrive as input events through the
same queue as touch. So `GamepadReader`
([`input/gamepad_reader.h`](../../engine/input/gamepad_reader.h), 61 lines) is a
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

> **Amended 2026-08-21, and the paragraph below is left as written.** *"The
> seam already has a shape for this and it is called `DeviceNotify`"* is wrong
> for this port, and the settlement it was cited in support of
> ([§4](#4-the-dependency-spine)) landed without it. `DeviceNotify` is the shape
> for a lost **device**. What an Android activity loses on
> backgrounding is its **surface**: under Vulkan — which
> [§3.5](#35-the-vulkan-backend) and [§6](#6-out-of-scope-on-purpose) commit
> this port to — the swapchain and the `VkSurfaceKHR` are destroyed while the
> `VkDevice`, every `VkImage` in it and therefore all of the content survive. So
> the routine Android event rebuilds a swapchain and never reaches the asset
> loader at all, and the two hazards read as one here only because Direct3D
> happens to lose both at once. This is knowledge rather than measurement (§1),
> and it is conditional on the Vulkan-first decision: a GLES context genuinely
> can take every GL object with it, which is the one fact that would put §6's
> rejected hedge back on the table.
>
> **What it costs instead is a call this seam cannot currently make.** A surface
> destroyed and recreated at the same size is not a resize, and
> `window_size_changed` is the only door in — where all four backends answer
> `false` and do nothing when the width and height match what they already have:
> [`d3d11/renderer.cpp:699-704`](../../engine/render/d3d11/renderer.cpp#L699-L704),
> [`d3d12/renderer.cpp:1063-1068`](../../engine/render/d3d12/renderer.cpp#L1063-L1068),
> [`gl/renderer.cpp:655-659`](../../engine/render/gl/renderer.cpp#L655-L659),
> [`null/renderer.cpp:172-175`](../../engine/render/null/renderer.cpp#L172-L175).
> A phone returning from the home screen at the size it left is exactly that
> call. The week below is the right size and this is one of the things in it.
> The term itself is the fifth backend's to state, so it is **not** written into
> `renderer.h` in advance, for the reason [§7](#7-the-documents-this-port-amends)
> gives.

> **Amended again 2026-08-21, by the fifth backend landing, and the amendment
> above is left as written.** Half of that finding is answered and half of it
> stands, and the split is worth being exact about because the paragraph above
> treats them as one thing.
>
> **The swapchain half needs no call at all.**
> [`engine/render/vulkan/device_resources.h`](../../engine/render/vulkan/device_resources.h)
> draws the frame into an image the engine owns and blits it into a swapchain
> image at present, for reasons that had nothing to do with Android — the seam
> permits a frame that is submitted and never presented, and a Win32 surface
> will not give a swapchain a size other than the window's. What falls out of it
> is that `VK_ERROR_OUT_OF_DATE_KHR` is handled entirely inside `present()`, so
> a swapchain going stale — from a rotation, from a compositor, from a resize
> nobody reported — never reaches `Renderer::window_size_changed` and never
> needed to.
>
> **The surface half stands, and is narrower than it was.** A backgrounded
> activity destroys the `ANativeWindow` itself, so the `VkSurfaceKHR` built from
> it has to be destroyed and a new one built from the handle the activity hands
> back. That is not something `present()` can discover: only the shell knows the
> new handle. So what the port still owes the seam is a way to say *here is a
> new window*, which is one call and not the resize term this paragraph was
> reaching for. It stays unwritten for the same reason as before — there is no
> platform here that makes it, and `renderer.h` should not gain a method for a
> caller that does not exist (T1).

`app/` is 1,800 lines: `window.{h,cpp}` at 906, `application.{h,cpp}` at 894.
The move itself is already specified ([§2](#2-the-claim-this-port-tests)) and
should cost hours. What costs a week is the lifecycle underneath it.

A Win32 program owns its window until it destroys it. An Android activity does
not: the surface can be destroyed and recreated under a running process, on
rotation, on backgrounding, on a phone call. **The seam already has a shape for
this and it is called `DeviceNotify`** —
[`renderer.h:239-245`](../../engine/render/renderer.h#L239-L245), `on_device_lost`
/ `on_device_restored`. Two of four backends currently use it and two never can
(a WGL context is not lost; the null backend has nothing to lose), which is the
open question `renderer.h:505-517` is still holding. Android is the platform
where surface loss is **routine rather than exceptional**, and that is the fact
that should settle where the hazard lives — see §4, because it argues for
settling it before the fifth backend rather than after.

The resize path is the other half and it is live: five commits in two days
(`d31a804`, `6ae4a15`, `f473505`, `30a0353`, `06da146`) are all fighting the
mid-frame-resize term, and
[`renderer.h:330-338`](../../engine/render/renderer.h#L330-L338) states a contract
written entirely in terms of a caller who already knows the size changed. See
§5 — this is the term Vulkan stresses.

### 3.5 The Vulkan backend — *weeks*

> **Done 2026-08-21, and it took a day rather than the weeks below.** The
> section is left as written, its estimate included, because an estimate that is
> quietly corrected teaches nobody anything. `render/vulkan/` is the four
> translation units it predicted; it passes `RenderPixelTests` and reproduces
> all fifty images in `tests/render/golden/` — the same set the other three
> rasterisers are held to — on the first run, and it draws
> `samples/linesweeper` on screen through a real driver at the right size and
> the right way up, resize included.
>
> **Where the estimate was wrong is the interesting part, and it is the sentence
> two paragraphs down.** *"A meaningful part of `d3d12/`'s cost was the
> synchronisation model, and that model is now written down and tested rather
> than being discovered."* That was the whole of it. A timeline semaphore is an
> `ID3D12Fence` with a different spelling, so the frame pacing, the wait before
> an allocator is reused and the stall on every upload transliterated from
> `d3d12/device_resources.cpp` line for line. What had to be *decided* rather
> than transliterated was one thing this document did not anticipate at all:
> **what a back buffer is.** A Vulkan swapchain image cannot be drawn into
> without being acquired and given back, and `tests/render/pixel_tests.cpp`
> draws fifty-three frames and presents exactly one of them — the case that
> walks the far end of `read_back_buffer`'s interval, after the read. The other
> fifty-two are consecutive and never present, which is more than any swapchain
> has images. The answer is in `vulkan/device_resources.h` and it is the
> largest decision in the port.
>
> **Three findings worth carrying forward.** The toolchain note below is
> right and understated: there are two `dxc.exe` on a normal Windows machine and
> the Windows SDK's — the one `PATH` finds first after `vcvars64` — lists every
> `-spirv` flag in its help and then answers *"SPIR-V CodeGen not available"*,
> so `cmake/compile_shaders.cmake` looks in `$VULKAN_SDK/Bin` and nowhere else.
> The register shifts that the same file applies are not optional: HLSL has a
> register space per resource kind and Vulkan has one binding number per
> descriptor set, so `b0`, `t0` and `s0` all arrive at set 0, binding 0 without
> them. And the validation layers found a defect nothing else could — a command
> buffer dropped without being submitted left this backend's image-layout
> tracking describing a transition that would never execute, on a case whose
> every assertion passed anyway; `DeviceResources::abandon_commands` says so
> where it is fixed.
>
> **§5's live claim is answered below**, in that section rather than here.

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

> **One of the four bullets is done, 2026-08-21, and the split is the useful
> part.** `windows.h` in `core/` was the only one answerable on this tree's own
> account, and its justification was never Android:
> [`core/registry.h:14-17`](../../engine/core/registry.h#L14-L17) had already
> refused to drag a platform header into this folder and said so in as many
> words, and these were the two files in the same folder not held to it.
>
> `throw_if_failed.h` is [`render/throw_if_failed.h`](../../engine/render/throw_if_failed.h)
> now — one file the two Direct3D backends share and neither owns, beside
> `sprite.hlsl`, which is the same arrangement for the same reason, because the
> rule that a backend's headers are its own leaves a copy in each folder as the
> only alternative. Its contract test moved with it into `RenderTests`, which
> is likewise built in every configuration, so the `static_assert` that was the
> point of it still fires where no Direct3D file is compiled at all.
> `thread_pool.h` keeps the Win32 thread pool and stops naming it: the platform
> types are behind a `ThreadPool::Impl` defined in the `.cpp`, the API is
> unchanged, and seven behavioural cases in `tests/core/thread_pool_tests.cpp`
> now hold a primitive that had none. `NOTICE` records the move, the file
> having been Microsoft's.
>
> **The other three wait, and §7's rule is why.** Clang equivalents for
> `/W4 /WX /permissive- /sdl /fp:precise`, an `arm64-v8a` preset, and
> `char16_t` on the render API are all correct for a build that does not exist.
> The last is the one to be careful with: `std::wstring` is not wrong today —
> a Windows code unit *is* 16 bits, which is exactly what `text_encoding.h`
> says the glyph table is keyed by — so changing it now would be rewriting
> working code in the future tense of a port, which is the mistake
> `texture_format.h` was corrected for. They land with the toolchain that needs
> them. **The section below is left as written**, its line numbers included.

Small, mechanical, and it blocks compiling anything.

- [`cmake/settings.cmake`](../../cmake/settings.cmake) is the single `INTERFACE`
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
  [`text_encoding.h:9-18`](../../engine/render/text_encoding.h#L9-L18) says the
  render module holds text as `std::wstring` because "a glyph table is keyed by
  code unit" and converting on the draw path is the alternative. On Windows a
  code unit is 16 bits. **On Android `wchar_t` is 32 bits**, so the glyph
  table's key changes width and the atlas's coverage assumption changes with it.
  This is not a blocker and it is not free; `char16_t` is the obvious answer and
  it touches `font.h`, `text_object.h` and both file readers.

---

## 4. The dependency spine

```
3.1 content probe ── DONE ──────┐   (hours; nothing depends on it, everything is shaped by it)
                                │
3.6 build + core ───────────────┤   (days; nothing compiles for the target without it)
                                │
        ┌───────────────────────┴──────────┬───────────────────────┐
        │                                  │                       │
3.5 Vulkan backend            3.1a KTX2 reader + ETC2      3.2 audio ── 3.3 input ── 3.4 shell
(verifiable on Windows, now)  (days; headless, testable    (all three need a device to finish)
                               now, blocks nothing)
```

**3.1a is what the probe spawned.** ETC2 itself is six one-line additions; the
container it has to arrive in is a second reader beside `dds_file.h`, and like
that one it is engine code with no device in it — so it joins the left branch,
not the right. Fonts ship uncompressed until the atlas sizes are revisited,
which is a content decision rather than a work item.

The shape that matters: **the left branch can start today and finish on this
desktop; the right branch cannot be finished without hardware.** That is why
Vulkan is a reasonable first *build* even though the content probe is the first
*decision*.

One ordering claim is worth arguing rather than asserting. **Settle
`AssetKind::reload_device` before the fifth backend, not after.**
[`renderer.h:505-517`](../../engine/render/renderer.h#L505-L517) holds it open and
records that a fourth backend "answered half of" it: device loss belongs to two
of four backends rather than to one, and what is left is which of three places
the hazard goes. Vulkan has `VK_ERROR_DEVICE_LOST` and lands on the same side,
making it three of five — and §3.4 says Android is where surface loss stops
being exceptional. A question that gets harder to answer with every backend
added to it, and whose answer changes when the platform arrives, should be
answered in the gap between the two. That gap is now.

> **Done 2026-08-21, and one of its two arguments did not survive the doing.**
> It stays on the loader. `DeviceNotify` carries the event and cannot carry what
> to rebuild, because a rebuild has to refill the slots the old resources sat in
> rather than make new ones, and the manifest is the only thing in this engine
> that knows their names.
> [`renderer.h:505-559`](../../engine/render/renderer.h#L505-L559) records it as
> settled rather than open — that file now has no open questions of its own —
> the criterion a caller applies is written on `AssetKind` itself (does the GPU
> hold this asset, never can this backend lose a device), and
> [`resource_loader_tests.cpp`](../../tests/assets/resource_loader_tests.cpp)
> pins what a restore does, in all four configurations, because none of it needs
> a device.
>
> The argument that did not survive is the second one. *"Whose answer changes
> when the platform arrives"* was true of a hazard Android does not deliver:
> what a backgrounded activity destroys is the surface, not the device, and the
> amendment at the head of §3.4 says why. The first argument was the honest one
> and was enough on its own — the question was answerable from the four backends
> already here, and each one added to it makes it longer to answer without
> making it different. **The paragraph above is left as written**, the claim it
> leans on included.

---

## 5. What Vulkan buys, and what it does not

**It buys the platforms, and probably all of them at once.** Vulkan is the
native modern API on Android and Linux, and MoltenVK is how most cross-platform
engines reach macOS and iOS without a Metal backend. For an engine that draws
textured quads with one blend state and two samplers, the subset MoltenVK does
not implement is not a subset this seam touches — see §1, this is the one
unmeasured claim in this document that would cost real work if it is wrong. **If
it holds, "Metal eventually" is a build target rather than a sixth backend.**

> **Answered 2026-08-21 and the paragraph below is left as written.** The
> contract at `renderer.h` has the right shape and did not move a line. The
> reason is not that the term was easy: it is that `render/vulkan/` does not
> draw into a swapchain image at all, so what the presentation engine declares
> stale is not what the seam calls the back buffer, and
> `VK_ERROR_OUT_OF_DATE_KHR` is answered inside `present()` without
> `window_size_changed` hearing about it. That was chosen for a different
> reason — see [§3.5](#35-the-vulkan-backend) — and this fell out of it, which
> is worth recording as luck rather than as foresight. What a backend that DID
> draw into swapchain images would have owed the seam is now an unasked
> question, and this document should not pretend it was asked.

**It buys one untested seam claim, and it is a live one.** Resize reaches the
renderer exactly one way today: `WM_SIZE` →
[`window.cpp:410`](../../engine/app/window.cpp#L410) →
[`application.cpp:411`](../../engine/app/application.cpp#L411) →
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
([`d3d12/backend.h`](../../engine/render/d3d12/backend.h) states the claim). Vulkan
has no in-box equivalent on a Windows runner — it is lavapipe or SwiftShader,
installed. That is a third CI install burden or a preset untested the way the GL
one is, and it should be decided rather than discovered.

> **Decided 2026-08-21: both, and for different reasons.**
> `.github/workflows/ci.yml` installs the Vulkan SDK on that preset and skips
> `RenderPixelTests` on it. The install is not optional and is not about the
> rasteriser — the build needs a `dxc` that emits SPIR-V, and the same installer
> supplies the loader without which none of the *other* eleven test executables
> in that preset would start at all. Rasterising is what was given up: a
> software ICD would be a second install of a thing whose only job is to
> disagree with the four backends that already rasterise the same images, and
> the reason the matrix exists is that a backend rots by failing to compile long
> before it rots by drawing the wrong thing.

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
| ~~`ARCHITECTURE.md:264`~~ | `audio`'s row gains the folder the sentence already implies (§3.2) — **done 2026-08-27**, and it gained two: `xaudio2/` and `null/`. The row now reads the way `render`'s does, naming the API and the folder it is confined to |
| ~~`PHILOSOPHY.md:303-305`~~ | "a second platform is an addition, not a rewrite" — amend with where it held and where it did not — **done 2026-08-27**, with the row above. It held for three of the four kinds of platform code that paragraph lists and did not hold for audio, which is what §2 above said and what the amendment now says in that document |
| ~~`PHILOSOPHY.md:454-462`~~ | "a second platform's" backend stops being hypothetical — **done 2026-08-21**, with §3.5. It is the one row the fifth backend earns on its own: the bullet said the seam owes a backend to "a headless one with no device, and a second platform's", and the second half was hypothetical until an API a second platform is actually reached through stood behind it. The rest of that document's backend arithmetic — four becoming five, three rasterisers becoming four — moved in the same commit, because those are counts rather than claims |
| ~~`renderer.h:505-517`~~ | `AssetKind::reload_device` — **settled 2026-08-21**, per §4. It stays on the loader; the note moved from STILL OPEN to SETTLED |

**None of these should be amended in advance**, and the rows already struck
through are not exceptions to that. It is the only line in the table that
amends nothing about Android: the question `renderer.h` held open was posed by
the four backends in this tree and was answerable from them, so settling it is
the tree answering its own question rather than a document describing a port
that has not happened. Every other row waits for the work that earns it — the
surface-loss term §3.4 names included, which is the fifth backend's to write and
not this document's to pre-empt.

A design document that describes a port that has not happened is the speculative
framework this tree refuses, and it would be the second time a comment in
`render/` was written in the future tense of a port. The first is on record and
was corrected: `texture_format.h`'s block-compression paragraph was written for
a GL backend that had not shipped, `docs/review/backend-equivalence/DRIFT.md`
caught it, and `a56d198` fixed it —
[`:34-42`](../../engine/render/texture_format.h#L34-L42) now says "that was
written before the port and the port has happened" in as many words, which is
why it is quotable in §3.1 at all.
