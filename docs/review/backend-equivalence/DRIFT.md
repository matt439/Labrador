# Documentation drift in `engine/render/`

> Part of the [backend equivalence audit](README.md). Read-only, 2026-08-19,
> against the tree at `57b65b3`. Not updated as findings are fixed.

The design documents are authoritative on intent and change by amendment in the
same commit as the change that fights them
([CLAUDE.md](../../../CLAUDE.md), [PHILOSOPHY.md](../../design/PHILOSOPHY.md)).
The comments in `engine/render/` are held to the same standard — `renderer.h` is
not a header with comments, it is the specification of the seam, and several of
its paragraphs are the only place a rule is written down.

These did not amend. Each item is a claim that the code no longer matches, with
the line that contradicts it. **This is a work item, not a fix**: nothing in this
audit changed a document, because a review that quietly rewrites the thing it is
reviewing cannot be checked afterwards.

**`engine/render/renderer.h`**

- **`:376-382`** — *"The shape of a backend is three translation units — renderer.cpp, render_resources.cpp and texture_factory.cpp"*. d3d11 has **four** (`device_resources.cpp`), gl has **four** (`gl_functions.cpp`); only null has three (`engine/CMakeLists.txt:100-139`).
- **`:379-380`** — *"The third file is thirty lines"*. `wc -l`: d3d11 **101**, gl **162**, null 35.
- **`:378-379`** — *"whatever that backend needs to build its shader, which for one of the three is nothing"*. Two of the three. `engine/CMakeLists.txt:131` is explicit: `# No shader step.` for gl.
- **`:390-391`** — *"which is what lets two of them pass the same 128 assertions"*. 99 assertion-macro lines now (was 84 when written, in `091c8f4`); executed count is ~212 with the 64-iteration `CHECK_MESSAGE` loop at `:988-993`. Three commits added assertions without amending this line. The framing is also wrong: the two never run in one process and are never compared to each other.
- **`:327-330`** — *"RGBA REGARDLESS OF WHAT THE BACKEND STORES… **This backend's buffer is BGRA** and the conversion happens here."* A per-backend sentence on the shared seam, false in two of three configurations. GL reads `GL_RGBA` and does no swizzle; its own comment at `:705-709` says so.
- **`:282-283`** — *"begin_frame … resets every view's recording"*. True on gl and null. On d3d11 it resets the CPU batch only — see defect B. Null has no back buffer to clear at all.
- **`:288-291` and `:258-259`** — document only `std::out_of_range` and nothing. All three backends also throw `std::logic_error` for a lowered count past a drawn view (`d3d11:713`, `gl:639`, `null:230`, pinned by `null_tests.cpp:247`) and `std::invalid_argument` for `view_capacity < 1`.
- **`:314-339`** — `read_back_buffer` never says a backend may refuse; null throws `std::logic_error`.
- **`:236-238`, `:296-304`** — swap chain / `FinishCommandList` / `ExecuteCommandList` vocabulary on the shared seam, contradicted by `:399-406` in the same file.
- **`:344-345`** — sends the reader to `engine/render/<backend>/device_resources.h`, a file two of three folders do not have. Same dead path at `sprite_vertex.h:17-19`.
- **`:388-389`** — *"a shader that multiplies by two constants"*. One constant and one per-vertex attribute; `sprite.hlsl:6-8` gets it right. Repeated at `sprite_geometry.h:24`.
- **`:40`, `:255`, `:328`** — "two backends" / "a second backend" throughout. Three.

**`engine/render/sprite_vertex.h:10-14`** — *"THE FIELDS ARE IN BUFFER ORDER AND THE STRUCT IS THE LAYOUT… reordering them here silently changes what every shader reads. That is the only rule about this type."* Neither real backend depends on declaration order: both build every offset from `offsetof` (`d3d11/renderer.cpp:407-418`, `gl/renderer.cpp:426-434`) and bind by semantic and by name respectively. Reorder the three fields and both follow silently and correctly. The stale rule has been copied verbatim into both backends (`d3d11/renderer.cpp:405-406`, `gl/renderer.cpp:423-424`) — three copies of a rule nothing enforces. What is load-bearing is the field *set*, the types, and that it is one interleaved struct.

**`engine/render/texture_format.h:12-14`** — *"the two readers that produce one and **the one backend** that consumes one are the whole of its traffic."* Two consume it by different routes and disagree; the third reads it never. `:16-24` — the texture counts still check out exactly (41 bc3 + 2 b8g8r8a8 = 43); the **font** counts do not: 32 `.spritefont` files exist across the two repositories, not two. `:26-33` is written in the future tense of a port that shipped — GL now queries `GL_EXT_texture_compression_s3tc` thirty lines away (`gl/texture_factory.cpp:30-46`).

**`engine/render/sprite_geometry.h:25-26`** — *"Two backends cannot disagree about where a sprite went, because only one of them decides."* Three backends, and defect A is the live exception.

**`engine/render/d3d11/sprite.hlsl:11-16`** — *"device_resources.cpp accepts a device down to feature level 9.1, so a shader that needed 10.0 would turn a machine this engine is meant to run on into a device-creation failure"*. It does not: `device_resources.h:59` defaults `minFeatureLevel` to `D3D_FEATURE_LEVEL_10_0` and `backend.h:206` takes the default, so the 9_3/9_2/9_1 entries are truncated out of the array (`device_resources.cpp:127-136`). `device_resources.h:52-56` states the correction outright. The comment was false when written — `git log -S` puts the 10_0 default four commits earlier.

**`engine/render/gl/backend.h:17-20`** — *"its only client is engine/render/gl/texture_factory.cpp. A second would be a mistake."* Three, all in-folder. Same error in both siblings, and worse in d3d11's: `d3d11/backend.h:21-26` names **`engine/app/application.cpp`** — which no longer includes it and for which the folder wall would now **fail the build** — and **`engine/render/d3d11/resource_factory.cpp`**, a file that does not exist. `null/backend.h:17-18` propagates the error to all three with *"like every other backend's header"*.

**`engine/render/gl/backend.h:22-35`** — *"WHAT THIS BACKEND DOES DIFFERENTLY FROM THE OTHER ONE, **and it is one thing**."* Three defects in one heading: there are two others; the thing named (record-then-replay) is what null does too and so does not distinguish GL; and it is not one thing — device loss is stated 111 lines later in the same file (`:133-139`), and `engine/CMakeLists.txt:129-132` calls run-time shader compilation *"the one place the two backends genuinely differ in kind"*, i.e. a second file already claims to be the one difference. Add mip sampling, format refusal, the load-order guard, the no-rebuild resize, the dropped depth range, the unbounded vertex store and the no-op markers.

**`engine/render/null/recording.h`**

- **`:16-20`** — *"cmake/check_engine_includes.cmake fails the build for it **by name**"*. It has not matched by name since `fb633f2`; `check_engine_includes.cmake:55-67` says so in capitals — **THE FOLDER, NOT ONE FILENAME IN IT**. `recording.h` is a header in `engine/render/null/`, so it is now **inside the wall this paragraph declares itself outside of**; the exemption survives only because the check scans `engine/` and the sole outside includer is `tests/render/null_tests.cpp:3`.
- **`:40-43`** — *"RenderPixelTests is excluded from this configuration **by name**"*. It is not built. `tests/render/CMakeLists.txt:55-62` contradicts it word for word: *"NOT BUILT AT ALL AGAINST THE NULL BACKEND, rather than built and excluded."* `ci.yml` says the same. Two files amended, this one not.
- **`:28-33`** — *"This answers the first two completely"*. Neither. `bench/scene_bench.cpp:144-154` still duplicates the render cull and says why; `tests/ui/stub_widget.h:14-21` still stubs and says *"That is worth doing and **is not done**."* Both call sites were amended to record that they were not converted; the header claiming to have converted them was not.

**`CLAUDE.md`**

- **`:103-105`** — *"A backend is three translation units … plus one shader, and all four live in `render/<backend>/`."* Wrong for all three; null has no shader at all.
- **`:73-76`** — *"A file outside `engine/render/<backend>/` including that backend's `backend.h`."* The pass no longer keys on `backend.h`; it guards **every header in the folder**, precisely because `device_resources.h` was the escape route. As written it would let a reader believe naming `engine/render/gl/gl_functions.h` from `engine/app/` is legal.
- **`:114-116`** — *"four state objects"*. d3d11 creates five (blend, depth-stencil, rasteriser, two samplers); gl creates two sampler objects and no state objects; null creates none.
- **`:50-55`** — *"`RenderPixelTests` … is the only test of anything this engine draws"* contradicts `:44-47` in the same file, and under `x64-debug-gl` it creates a WGL context, not a Direct3D device.

**Adjacent, same drift, worth fixing in the same pass**

- `d3d11/renderer.cpp:238-243` — `FinishCommandList(FALSE)` is justified *"because begin_frame rebinds the render target and the viewport on every context at the top of the next frame."* `begin_frame:672-681` is the paragraph explaining that binding moved to `set_view_count`. The flag is still right; its stated reason is gone.
- `gl/renderer.cpp:388-390` — *"A run longer than this many is split **at replay**"*. It is split at record time in `View::draw` (`:106-121`); `replay()` neither splits nor clamps. `gl/backend.h:102-104` states it correctly.
- `CMakeLists.txt:31` — *"ten test targets and a sample executable"*: eleven and two.
