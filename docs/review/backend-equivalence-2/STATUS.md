# Status — what has been done about the five-backend sweep

> **This is the one document in this folder that IS updated as findings are
> fixed.** The other five are the sweep as written, frozen at `ca5b2e3`, and say
> so in their own headers. This one is the ledger, and it exists because the
> sweep found that the 2026-08-19 audit had been almost entirely worked through
> without anybody being able to tell — 26 of its 28 drift claims were already
> amended and no document recorded it. That is the failure this file prevents.
>
> The convention is `docs/survey/2026-08-26-status.md`'s, which was created nine
> days earlier for the same problem. Follow it: **tick the box in the same commit
> as the change**, and where a commit answers an item, name the item in the
> commit message so `git log --grep` finds it.

## How to use it

- `- [ ]` open, `- [x]` done. Add the commit SHA after the item when you tick it.
- **An item that turns out to be wrong is ticked with a reason, not deleted.**
  The sweep itself found one claim the prior audit got wrong and one it got half
  right; the same will be true here, and a struck item with an argument beside it
  is worth more than a missing line.
- §2 is grouped **by file rather than by severity**, because that is how the work
  is actually done — one pass over `gl/renderer.cpp` answers ten items, one over
  `renderer.h` answers nine, and 23 of the 46 files carry a single line. Severity
  is on each line.
- Items marked *(unchecked)* were raised but never put through the adversarial
  pass. **Verify before amending.** Three of the 47 that were checked came back
  refuted, so expect three or four of these to be wrong.

---

## Progress

| Section | Items | Done |
|---|---|---|
| 1. The eight findings | 8 | 8 |
| 2. Documentation drift | 110 | 110 |
| 3. Tests and checks that never landed | 34 | 30 |
| 4. Decisions to make | 4 | 3 |
| 5. Finishing the sweep | 6 | 5 |
| **Total** | **162** | **156** |

---

## 1. The eight findings

From [README.md](README.md) §5 — the candidates that survived two adversarial
lenses. None puts a wrong pixel on a screen. Ranked as the README ranks them.

- [x] **1. CI runs `RenderPixelTests` where the WARP fallback is compiled out**
      `.github/workflows/ci.yml:86-88` gives `x64-release` `skip_tests: ''`; `d3d11/device_resources.cpp:162-191` puts WARP in the `#else` of `#if defined(_DEBUG)`. Four statements in the tree about that job cannot all be true. **Settled by reading one job log** — do this first, it is the cheapest item in the folder and the only one that is not a question about the source.
      **DONE — the log says the premise is false. Run 33144174541, job `windows (x64-release, false)`: 14 of 14 pass, `RenderPixelTests` in 0.36s, with the WARP fallback compiled out. So the runner offers a DXGI adapter that is not flagged software and `D3D11CreateDevice` succeeds on it; which rasteriser that is, a log cannot say, because the two lines that would name it are `OutputDebugString` calls in `_DEBUG` builds. All four statements amended to what is checkable: `.github/workflows/ci.yml` (the evidence), `tests/render/CMakeLists.txt`, `tests/render/null_tests.cpp`, `tests/render/golden_image.cpp` — and `CLAUDE.md`, `README.md`, `renderer.h`, `d3d12/backend.h`, `docs/port/android.md` with it**

- [x] **2. D3D12 leaks a descriptor-heap slot per texture name on a non-device-loss release**
      `next_texture_slot` is a bump allocator with no free list, reset only at `d3d12/renderer.cpp:814` and `:994`. `RenderResources::release_device_resources()` empties the table without resetting it. Fixed heap of 256. Narrowed to an unstated precondition on public seam API — so the fix may be a sentence rather than code.
      **DONE — the sentence, not the free list. `render_resources.h` now states the precondition on `release_device_resources()` — it is the first half of a device loss — and says why a free list is the wrong answer (T1, T3): it would buy back 256 slots for a call nothing makes, at the price of bringing frames-in-flight bookkeeping to the seam. `d3d12/render_resources.cpp` says what a release without a device costs there. Answers §4's fourth decision**

- [x] **3. D3D12 `handle_device_lost` can tear down with a live device still executing**
      Of its three call sites, `d3d12/device_resources.cpp:320-324` — the ordinary `WM_EXITSIZEMOVE` resize path where `try_wait_for_gpu()` answered false — reaches `:444-449`, whose first statement is `notify_->on_device_lost()`, with no GPU wait.
      **DONE — the branch now asks the device. `create_window_size_dependent_resources` calls `GetDeviceRemovedReason()` before recovering: a removal takes `handle_device_lost` as before, and a wait that failed for any other reason — `E_OUTOFMEMORY` out of `Signal` or `SetEventOnCompletion`, with the GPU still executing — is `ThrowIfFailed(E_FAIL)`, the same answer `wait_for_gpu` gives to the same question**

- [x] **4. Vulkan `abandon_commands` discards tracked image layout on a submitted-but-unpresented frame**
      `vulkan/device_resources.cpp:1564`, `:1627-1630`. The residual half of the Vulkan review's C4/C5, on the branch the applied fix deliberately kept. **Verify under the validation layer with `validate_sync` on** before changing anything — see [docs/review/vulkan/](../vulkan/) §2.
      **DONE — `colour_layout_submitted_` replaces the `discarded` flag. The flag asked whether a command buffer was open; the question is whether the transitions in it had run. `execute()` mirrors the layout on every successful submit and `abandon_commands` assigns the mirror back unconditionally — right for a frame thrown away, right for one that submitted and recorded again, and a no-op in the case the flag was written for. Verified under the Khronos validation layer with `validate_sync = true` and `validate_best_practices = true`: 34 cases, 336 assertions, zero errors and zero SYNC-HAZARDs, and all fifty goldens byte-identical**

- [x] **5. The folder wall's module is a hard-coded `(render|audio)` alternation**
      `cmake/check_engine_includes.cmake:97` and `:103`. So `engine/input/xinput/` — the third platform seam ARCHITECTURE names — is outside the wall, and `CLAUDE.md:149-159` repeats the false consequence clause in the section listing what fails the build. Documentation-accuracy finding with no behavioural component; the fix is either two literals or one sentence.
      **DONE — the module is captured now, and the claim is checked rather than made. Both regexes take any `[A-Za-z0-9_]+`, so `engine/input/xinput/` is inside the wall; the include must be the engine's own spelling — rooted at `engine/`, or relative with `../` — because a bare three-component path is not by itself an engine one (`<rapidjson/error/en.h>`). Tested against four spellings, including the `../../engine/audio/null/` form the old regex also missed. `CLAUDE.md` says what was true for nine days, and the header block at `:1-15` — the same file's other drift item — now says the game-header rule does fail standalone**

- [x] **6. Two of the three terms a draw call is keyed on have never rasterised**
      No submitted frame in `tests/render/pixel_tests.cpp` changes texture or filter mid-list — the only two-texture list is deliberately never submitted. Only the viewport break has ever reached a device. Both stamps are asserted on null. This is TEST-GAP's **B8** by another route.
      **DONE — both terms rasterise now, in `3022ca0`. `tests/render/pixel_tests.cpp` gained "a texture change keeps call order, it does not group runs" - three interleaved runs whose overlaps say GREEN if a backend grouped by texture - and "a filter change mid-list applies to what follows it", which needed the first texture in this file with an edge in it, because point and linear differ only between texels. All four rasterisers write both images byte for byte, the filtered edge included**

- [x] **7. Re-loading a texture name inside an open frame is undefined and differs across five**
      `resource_factory.h:79-87` states only the `create_device` ordering rule and is silent on the `begin_frame`/`submit` interval. The named contract at `pixel_tests.cpp:1782` pins re-load only *outside* an open frame. Four rasterising backends hold a non-owning reference until `submit()`; null does not.
      **DONE — written onto `resource_factory.h` beside the `create_device` ordering rule, which is where the other half of the same rule already lived. It names what each rasteriser holds and does not own until `submit()`, says the null backend answers differently, and says the supported thing: load content between frames**

- [x] **8. D3D12's texture factory discards its `HRESULT`**
      `d3d12/texture_factory.cpp:159-175`. `E_OUTOFMEMORY`, `E_INVALIDARG`, `DXGI_ERROR_DEVICE_REMOVED` and an unsupported format all arrive as one sentence naming only the format and the dimensions. T6, and the only one of the five factories whose message both asserts a cause and carries no code.
      **DONE — the message carries it. `hresult_name(created)` in the same position Vulkan's factory names its `VkResult`, and the comment names the four answers that used to arrive as one sentence**

---

## 2. Documentation drift

All 110 claims from [DRIFT.md](DRIFT.md) that the adversarial pass did not
refute, grouped by file. **66 are marked *(unchecked)* — verify those before
amending.** The rule this list exists to serve is CLAUDE.md's: a document changes
in the same commit as the change that fights it. These have no such commit coming,
because the code they describe is already correct — so they need a commit of their
own, which is what `dea5fe0` was for the last batch.

**ALL 110 AMENDED IN ONE PASS, 2026-08-28** — the commit is "Amend every comment
in engine/render/ that five backends made untrue". Each was located by text
rather than by the line numbers above, read against the code beneath it at HEAD,
and rewritten to what the code says now; the ones marked *(unchecked)* were
verified first, and the verification is why four of them are amended differently
from what this list predicted:

- `gl/renderer.cpp:33-34` — `same_viewport` is not what closes a run, and the
  amendment says what does (`set_viewport`'s own `close_run`) and why the term
  is kept anyway.
- `golden_image.cpp:98-102` — the case shaped like that has ONE subcase, so the
  reset it describes has never yet been needed; the amendment says so.
- `sprite_geometry.cpp:126-127` — the worked example was unreachable, so the
  example is replaced rather than the count corrected.
- `d3d11/backend.h:81` — the by-name lookup is not the loader's, so the
  amendment says what it is for instead of restating the claim.

Three items were answered by a code change rather than a comment and are ticked
in §1 as well: the two `check_engine_includes.cmake` items (finding 5) and
`d3d12/texture_factory.cpp`'s HRESULT (finding 8).

### `engine/render/gl/renderer.cpp` — 10

- [x] **`:691-693`** *(high, stale-backend-arithmetic)* — It vanishes on four. d3d12/renderer.cpp:1070 and vulkan/renderer.cpp:829 both carry "A FRAME IN PROGRESS IS RESTARTED, NOT REFUSED", and null/renderer.cpp:193 restarts for this paragraph's own argument and says so in the corrected form: "vanish on the other fo
- [x] **`:230-232`** *(medium, stale-backend-arithmetic)* *(unchecked)* — It is identical to all four others', and the tree already carries the corrected sentence: engine/render/vulkan/renderer.cpp:244 reads "IDENTICAL TO EVERY OTHER BACKEND'S, LINE FOR LINE BAR THE TYPES", verbatim otherwise. I compared the four DrawList::draw_text
- [x] **`:769-772`** *(medium, stale-backend-arithmetic)* *(unchecked)* — Two backends have the harder version, and the corrected wording is one folder over. engine/render/vulkan/renderer.cpp:924-926: "The two backends that record into a command list have a harder version of this problem, but the answer the seam gives is the same, s
- [x] **`:69-72`** *(medium, stale-backend-arithmetic)* *(unchecked)* — Three backends compile the shader at build time now, from the one shared source. engine/CMakeLists.txt:123-130 runs compile_hlsl at vs/ps_4_0_level_9_1 for d3d11, :145-152 at vs/ps_5_1 for d3d12, and :202-209 runs compile_hlsl_to_spirv at vs/ps_6_0 for vulkan 
- [x] **`:858-860`** *(medium, stale-backend-arithmetic)* *(unchecked)* — Three other buffers are BGRA and all three swap. d3d11/renderer.cpp:985 ("B and R swap on the way out"), d3d12/renderer.cpp:1336 ("B and R swap on the way out: the back buffer is B8G8R8A8 and the seam…"), vulkan/renderer.cpp:1315 ("B AND R SWAP ON THE WAY OUT:
- [x] **`:671-675`** *(medium, stale-backend-arithmetic)* *(unchecked)* — Three answer true, and the shell's own file already says so in the plural. engine/app/window.cpp:371-373: "then made gl answer 'nothing changed' to the WM_EXITSIZEMOVE below - the one message that ends a resize, which the other backends answer true to." d3d11/
- [x] **`:873-875`** *(medium, stale-backend-arithmetic)* *(unchecked)* — One of five forwards the markers and four do not, which makes the no-op the ordinary case rather than this backend's exception. d3d11/renderer.cpp:1005-1017 calls PIXBeginEvent/PIXEndEvent/PIXSetMarker; d3d12/renderer.cpp:1365,:1374, vulkan/renderer.cpp:1373,:
- [x] **`:96-100`** *(medium, stale-backend-arithmetic)* *(unchecked)* — The header twelve files' worth of argument above it was amended to retract exactly this, and this copy was left. engine/render/gl/backend.h:24-25: "WHAT THIS BACKEND DOES DIFFERENTLY FROM D3D11, AND IT IS NOT ONE THING. This heading used to claim it was", foll
- [x] **`:537-539`** *(low, stale-backend-arithmetic)* *(unchecked)* — It is the same two words in every rasteriser's descriptor: d3d11/renderer.cpp:464-468 (D3D11_BLEND_ONE / INV_SRC_ALPHA), d3d12/renderer.cpp:735-739, vulkan/renderer.cpp:472-476 (VK_BLEND_FACTOR_ONE / ONE_MINUS_SRC_ALPHA), and gl's own glBlendFuncSeparate at :5
- [x] **`:33-34`** *(low, dead-path)* *(unchecked)* — same_viewport never closes a run, because it cannot return false where it is asked. Its only call site is the join predicate at renderer.cpp:110, guarded by `this->open_valid`; and View::viewport is written in exactly three places, each of which has already in

### `engine/render/renderer.h` — 9

- [x] **`:610-613`** *(high, behaviour)* — engine/render/sprite.hlsl says the opposite twice, in the file this sentence is about. sprite.hlsl:31-36: "ONE difference reaches / this file and it is worth knowing before that sentence is read as none: the / declaration order of VertexIn's three members is a
- [x] **`:357-360`** *(high, stale-backend-arithmetic)* — The file this sentence sends the reader to says two, and says so deliberately. engine/render/render_resources.h:112-123: "CONSTRAINT: A RenderResources OUTLIVES THE Renderer IT WAS FILLED / AGAINST. This class holds whatever a backend calls a texture, and on /
- [x] **`:367-371`** *(medium, stale-backend-arithmetic)* — Three of the five hold a frame in vectors, not two. gl/backend.h:122-123 (`std::vector<SpriteVertex> vertices; std::vector<Run> runs;`), null/backend.h:80 (`std::vector<RecordedSprite> sprites;`) and vulkan/backend.h:152-153 (the same two vectors) — and each `
- [x] **`:390-394`** *(medium, stale-backend-arithmetic)* — Three, not two: gl, null and vulkan each strand a vector. All five throws exist and are byte-comparable (d3d11/renderer.cpp:863, d3d12/renderer.cpp:1183, gl/renderer.cpp:777, null/renderer.cpp:273, vulkan/renderer.cpp:931), so the "All five throw it" clause be
- [x] **`:284-290`** *(medium, count)* *(unchecked)* — There were four. engine/render/null/ landed at f1b952e on 2026-08-15; the commit that wrote this rule is d31a804 on 2026-08-20 22:16, five days later, and its diff touches d3d11, d3d12, gl, renderer.h and pixel_tests.cpp and not null. At that commit the null b
- [x] **`:36-38`** *(medium, cross-reference)* *(unchecked)* — docs/design/ARCHITECTURE.md:122-126 is now the directory tree — line 122 is "│   │   │                   decision that shows on screen", 123-125 the sprite.hlsl row, 126 the d3d11/ row. The passage cited sits at ARCHITECTURE.md:260-269 ("Promoting a concrete c
- [x] **`:298-300`** *(low, cross-reference)* *(unchecked)* — resize_client has exactly one caller in the tree — application.cpp:220, inside Application::set_resolution. Application::set_fullscreen (application.cpp:224-246) calls Window::enter_fullscreen() or Window::leave_fullscreen(...) instead, which do their own SetW
- [x] **`:460-465`** *(low, stale-backend-arithmetic)* *(unchecked)* — The list accounts for three of the four rasterisers and stops. Vulkan's cost is a B/R swap and nothing else, and its own file spells it out: vulkan/renderer.cpp:1315-1322, "B AND R SWAP ON THE WAY OUT: the colour target is B8G8R8A8 / and the seam promises RGBA
- [x] **`:646-649`** *(low, count)* *(unchecked)* — Fifty is right — tests/render/golden/ holds exactly 50 PNGs and pixel_tests.cpp:62-64 agrees ("Fifty-three frames; fifty of them are 64x64 on every backend"). Four fill more than one view, not six. pixel_tests.cpp:1300 says "THE FOUR CASES BELOW ARE THE FIRST 

### `tests/render/pixel_tests.cpp` — 8

- [x] **`:1414-1417`** *(high, stale-backend-arithmetic)* — Written at ccd8ba4 (2026-08-20), when d3d11, gl and null were the whole tree; d3d12 (3dda092) and vulkan (4d91b91) landed after and neither correction commit touched it. At HEAD there are five, and they reset in two places, not three: gl/renderer.cpp:743-749, 
- [x] **`:1305-1310`** *(medium, stale-backend-arithmetic)* *(unchecked)* — This paragraph was deliberately amended for the fourth backend (e92a982 authored lines 1303-1312) and then not amended for the fifth. engine/render/vulkan/ has per-view multi-view machinery of its own - a vector of runs per view, replayed in view order - and e
- [x] **`:1636-1637, and the same sentence again at :1705-1706`** *(medium, stale-backend-arithmetic)* *(unchecked)* — The next sentence in the same paragraph was amended for the fifth backend and this one was not: line 1641 is 4d91b91's "would leave GL drawing into 64x64 while the other three moved to 32x32", which is right - d3d11, d3d12 and vulkan all answer the size they w
- [x] **`:651-652`** *(medium, stale-backend-arithmetic)* *(unchecked)* — Written at 6af3e61 (2026-08-20), before 3dda092 and 4d91b91. contract-a-positive-rotation-turns-the-sprite-clockwise-on-screen.png is one of the fifty images in tests/render/golden/, and tests/render/golden_image.h:14-15 says the set is what "the four rasteris
- [x] **`:50-53`** *(medium, dead-path)* *(unchecked)* — Rotation is pinned, by a case added at 6af3e61 which the header was never amended for: pixel_tests.cpp:612 TEST_CASE("CONTRACT: a positive rotation turns the sprite clockwise on screen"), whose own comment at :638-641 says "Where it was before the turn, and wh
- [x] **`:1503-1505`** *(medium, cross-reference)* *(unchecked)* — Harness::read_frame calls it on every frame: pixel_tests.cpp:497, "this->buffer_ = this->renderer_.back_buffer_size();", added by 6103b2f (2026-08-20) a day after f22c968 wrote this comment, so that at() could address the three frames whose buffer is not 64x64
- [x] **`:301-302`** *(low, count)* *(unchecked)* — Two cases do. The minified-draw case builds two_level_texture() at :678-681 (68fe4dc, which wrote this comment) and "CONTRACT: re-loading a name reuses its slot, however many times" builds three hundred flat_texture()s at :1805-1814 (bdf752d, which did not ame
- [x] **`:1215-1217`** *(low, cross-reference)* *(unchecked)* — engine/scene/scene.cpp:211 is still the only production caller, but a test reaches it now: tests/scene/fanout_tests.cpp calls scene.draw(this->renderer_) at :91 and asserts on the RecordedSprites that come back, and Scene::draw_views (scene.cpp:203-212) issues

### `engine/render/d3d11/renderer.cpp` — 6

- [x] **`:73-76`** *(high, behaviour)* — The OpenGL backend's framebuffer origin IS at the bottom and it writes these four numbers unchanged: `engine/render/gl/renderer.cpp:602-605` is `glUniform4f(this->transform_uniform, pane_width > 0.0f ? 2.0f / pane_width : 0.0f, pane_height > 0.0f ? -2.0f / pan
- [x] **`:39`** *(medium, stale-backend-arithmetic)* — Two-backend arithmetic in a tree with four rasterisers. "The other one" was gl/, and it is still the one that has no float viewport (gl/renderer.cpp:575-576, and the glViewport call at :595 takes GLint/GLsizei). But d3d12/ takes a D3D12_VIEWPORT whose extents 
- [x] **`:264-266`** *(medium, stale-backend-arithmetic)* — Three of the five handle device loss now. `engine/render/d3d12/device_resources.cpp:444` defines `DeviceResources::handle_device_lost()` and it is reached from three sites (`:322`, `:347`, `:488`); `engine/render/vulkan/device_resources.cpp:1358` defines the s
- [x] **`:37-39`** *(medium, stale-backend-arithmetic)* *(unchecked)* — Two of the four other backends could keep it as well, and both say so in their own files. `engine/render/d3d12/renderer.cpp:37-43`: "D3D12_VIEWPORT is D3D11_VIEWPORT with different spelling, and the truncation is the same deliberate one: the extents are FLOAT 
- [x] **`:130-135`** *(medium, behaviour)* *(unchecked)* — There are two DISCARD cases, not one. After the wrap test at `:137-141` the code asks again: `if (this->buffer_position == 0) { how = D3D11_MAP_WRITE_DISCARD; }` (`:142-145`). `reset()` sets `buffer_position = 0` (`:230`) and `begin_frame` calls it on every vi
- [x] **`:409-413`** *(low, count)* *(unchecked)* — The body makes three: `texture->GetResource(...)` (`:415`), the `QueryInterface` inside `resource.As(&texture_2d)` (`:418`), and `texture_2d->GetDesc(...)` (`:421`) — plus the two `Release`s when the ComPtrs leave scope. `engine/render/d3d12/backend.h:89` repe

### `engine/CMakeLists.txt` — 5

- [x] **`:119-122`** *(medium, stale-backend-arithmetic)* *(unchecked)* — Three compile it now. The same file is compiled at `vs/ps_4_0_level_9_1` here (`:123-130`), at `vs/ps_5_1` for d3d12 (`:145-152`), and at `vs/ps_6_0` through the Vulkan SDK's dxc for vulkan (`:203-210`). The vulkan block itself says "The same source as the thr
- [x] **`:119-122 (claim at :120-121)`** *(medium, stale-backend-arithmetic)* — Three do. This file compiles sprite.hlsl three times over: d3d11 at vs/ps_4_0_level_9_1 (:123-130), d3d12 at 5_1 (:145-152) and vulkan at 6_0 through dxc (:202-209). The file it points at says so in its second paragraph — sprite.hlsl:4 "Three backends compile 
- [x] **`:96-97`** *(medium, stale-backend-arithmetic)* — "Either" counts two folders; there are five — render/d3d11/, render/d3d12/, render/gl/, render/vulkan/, render/null/, enumerated in the cache STRINGS three lines below at :100-101. git log -L 96,97 dates the sentence to 091c8f4, the commit that added the secon
- [x] **`:196`** *(medium, stale-backend-arithmetic)* *(unchecked)* — Read positionally — which is how this file uses "above" everywhere else (:140 "the backend above", :166 "the two that compile HLSL above") — the three backends above the vulkan block are d3d11, d3d12 and gl, and gl compiles no HLSL at all: :162-166, twenty lin
- [x] **`:3-5 (claim at :5)`** *(low, other)* *(unchecked)* — There is no game in this repository to migrate from. ARCHITECTURE.md:178 records that game/ is its own repository now and consumes this one as a submodule; CLAUDE.md opens "This repository is the engine half of a split". Nothing in docs/design/, CLAUDE.md or R

### `tests/render/null_tests.cpp` — 5

- [x] **`:317-319`** *(high, behaviour)* — Both halves are false at HEAD. pixel_tests.cpp calls set_viewport at :1235, :1271, :1276, :1353, :1407, :1442 and :1539 - an entire section headed "--- the viewport ---" at :1210-1223 exists to drive it through a rasteriser (added by b1a837e, after 126eded wro
- [x] **`:322-324`** *(medium, cross-reference)* — pixel_tests.cpp:690 and :703 both call DrawList::set_filter, so it is odr-used in the four configurations this file is NOT compiled in. A grep of the tree for set_filter outside engine/render/ returns only these two files. The sentence was amended by the Vulka
- [x] **`:444-445`** *(medium, stale-backend-arithmetic)* *(unchecked)* — The arithmetic does not close in either direction. "four" was written by the Vulkan commit (4d91b91 authored line 444) while "the third" was left from cc02678 - two plus one accounts for three of a stated four, and there are five backends. The tree's own curre
- [x] **`:32-33`** *(medium, count)* *(unchecked)* — There are fourteen ctest entries at HEAD, not twelve: add_test appears fourteen times across bench/CMakeLists.txt:40 and tests/*/CMakeLists.txt (MattMathTests, CoreTests, CollisionTests, SceneTests, RenderTests, RenderPixelTests, InputTests, UiTests, AssetsTes
- [x] **`:30-32`** *(medium, behaviour)* *(unchecked)* — WARP is available on a build machine, which is the entire reason CI does not skip RenderPixelTests everywhere. .github/workflows/ci.yml:33-35: "A runner has no GPU. Direct3D falls back to WARP, an in-box, fully conformant software rasteriser" - and ci.yml:41-4

### `engine/render/d3d11/backend.h` — 4

- [x] **`:201-204`** *(medium, stale-backend-arithmetic)* — It is a std::vector on only three of the other four. `engine/render/gl/backend.h:122-123` and `engine/render/vulkan/backend.h:152-153` hold `std::vector<SpriteVertex> vertices; std::vector<Run> runs;`, and `engine/render/null/backend.h:80` holds `std::vector<R
- [x] **`:27-28`** *(low, cross-reference)* *(unchecked)* — No shader header includes this one, or anything else. The two generated headers are fxc `/Fh` output — `out/build/x64-debug/generated/engine/render/d3d11/sprite_vertex_shader.h` opens with `#if 0` and a disassembly comment and holds a byte array, nothing more 
- [x] **`:81`** *(low, dead-path)* *(unchecked)* — The loader does not use it. `add_texture_asset` (`d3d11/texture_factory.cpp:113`) calls `add_texture` and reads nothing back; what the load path wants afterwards is a handle, and it gets one from `RenderResources::resolve_texture` (`d3d11/render_resources.cpp:
- [x] **`:253-256`** *(low, count)* *(unchecked)* — Five state objects follow the comment with no break: `blend`, `depth`, `rasterizer` (`:257-259`) and then `point_sampler`, `linear_sampler` (`:260-261`), all five made in `create_device_dependent_resources` (`renderer.cpp:462-533`) and all five bound per flush

### `engine/render/gl/backend.h` — 4

- [x] **`:148-150`** *(high, stale-backend-arithmetic)* — Three of the five need it now. engine/render/d3d12/backend.h:264 declares `class Renderer::Impl final : public D3DDeviceNotify` and engine/render/d3d12/device_resources.cpp:448 calls `this->notify_->on_device_lost();`; engine/render/vulkan/backend.h:179 declar
- [x] **`:54-55`** *(high, stale-backend-arithmetic)* — A hand-written owner is now the majority case, and one backend says in its own header that COM's free reference is precisely what it does not get. engine/render/vulkan/backend.h:76-84: "AND A REFERENCE TO THE DEVICE, WHICH IS THE ONE FIELD NO OTHER BACKEND'S T
- [x] **`:192-195`** *(medium, stale-backend-arithmetic)* *(unchecked)* — Four backends are "the other backend" now, and the stated reason is wrong for one of them. engine/render/vulkan/backend.h:60-62 lists first among its own differences that "the frame is drawn into an image this engine owns and blitted into a swapchain image at 
- [x] **`:20-22`** *(medium, cross-reference)* *(unchecked)* — sprite_shader.h is not a client of this header and cannot be: engine/render/gl/sprite_shader.h contains no #include directive at all across its 84 lines - it is `#pragma once`, comment, then two raw-string GLSL constants. The arrow runs the other way, engine/r

### `cmake/check_engine_includes.cmake` — 3

- [x] **`:82-84 (restated inline at :100-102; the code is at :97 and :103)`** *(high, behaviour)* — The module is listed, and only the backend name has that property. Line 97: REGEX "...(engine/)?(render\|audio)/[A-Za-z0-9_]+/[A-Za-z0-9_]+\\.h[\">]" and line 103: string(REGEX MATCH "(render\|audio)/([A-Za-z0-9_]+)/...") — the backend is a wildcard `[A-Za-z0-
- [x] **`:1-15 (claim at :2; see also :7-8 and :14)`** *(high, behaviour)* — It does fail. This whole header block is written in the pre-split present tense and was never amended, while every other statement of the same rule in the tree was. :7-8 says "the engine's own include root has to be the directory above engine/ - and in this tr
- [x] **`:42`** *(low, cross-reference)* *(unchecked)* — ARCHITECTURE.md has no section of that name and never has: its headings at HEAD are The targets, The build, The tree, Modules, A game project, The boundary, and at 781b157 — the commit that wrote this message — they were the same six. The paragraph this wall i

### `engine/render/d3d12/backend.h` — 3

- [x] **`:34-35`** *(medium, stale-backend-arithmetic)* *(unchecked)* — Four other backends since 2026-08-21, and the enumeration that follows presents three as the whole field: ":38-40, "D3D11 renames a mapped buffer for you and tracks what is still in flight; OpenGL's driver does the same behind glBufferSubData; the null backend
- [x] **`:87-89`** *(low, count)* *(unchecked)* — It is three, not two. engine/render/d3d11/renderer.cpp:414-421 does `texture->GetResource(...)`, then `resource.As(&texture_2d)` - a QueryInterface, which is as virtual as the other two - then `texture_2d->GetDesc(&description)`, with the AddRef/Release traffi
- [x] **`:324-326`** *(low, other)* *(unchecked)* — The census is one short. The index buffer's own upload goes on this list too: renderer.cpp:938-952 takes `open_frame_list()`, records `CopyBufferRegion` into the DEFAULT-heap index buffer, records the COPY_DEST -> INDEX_BUFFER barrier, and calls `execute_frame

### `engine/render/d3d12/texture_factory.cpp` — 3

- [x] **`:81-82`** *(high, stale-backend-arithmetic)* — Two errors in one clause. There are five texture_factory.cpp, not three - every backend has one (CLAUDE.md, "Every backend has the same three translation units"), and renderer.h:588-591 measures all five. And this is not the longest of them: vulkan/texture_fac
- [x] **`:95-98`** *(high, behaviour)* — Two backends wait on a GPU inside the load now, not one. engine/render/vulkan/texture_factory.cpp:309-311 records the copy and the barrier and then calls `device_resources.end_upload(commands)` - "Ends, submits and waits, which is what the staging buffer's lif
- [x] **`:180-182`** *(medium, count)* *(unchecked)* — The tree measured this and corrected the other two sites. docs/port/content-probe.md:27-38 parsed every header in both trees and found 43 .dds in all - 41 block-compressed, two uncompressed - and §6 at :253-256 calls the old figures "one correction ... owed to

### `engine/render/font.h` — 3

- [x] **`:39-40`** *(high, behaviour)* — It accumulates. `for_each_glyph` mutates the running pen with it — `x += glyph.x_offset;` (engine/render/font.h:172) — and then adds the step on top of the already-offset x — `x += static_cast<float>(glyph.subrect.width) + glyph.x_advance;` (engine/render/font
- [x] **`:66`** *(high, behaviour)* — The first half is right and the second is not: `x += glyph.x_offset;` (engine/render/font.h:172) writes the bearing into the pen variable itself, and nothing removes it before `x += static_cast<float>(glyph.subrect.width) + glyph.x_advance;` (font.h:188) advan
- [x] **`:133`** *(medium, cross-reference)* — Only one of the two above shares it. `measure` does — `this->for_each_glyph(text, ...)` (engine/render/font.cpp:105) — and every draw_text does, one per backend (d3d11/renderer.cpp:375, d3d12/renderer.cpp:422, gl/renderer.cpp:234, vulkan/renderer.cpp:248, null

### `engine/render/texture_object.h` — 3

- [x] **`:60-62`** *(high, behaviour)* — `TextureObject::draw_with` reads one per-draw member on its last line: `texture_object.cpp:87` passes `this->layer_depth()` into `SpriteSheet::draw`, and `draw_with`'s parameter list (`texture_object.h:69-75`) has no layer_depth to displace it. The member is `
- [x] **`:64-66`** *(medium, cross-reference)* — There is no `Level` and no `draw_active_level` in this repository — `grep -rn "draw_active_level"` over engine/, samples/ and tests/ returns nothing, and `grep -rn "class Level"` nothing. The only other hits are docs/review/all-findings.md, where it is `ArtAtt
- [x] **`:49`** *(low, cross-reference)* *(unchecked)* — `ThreadPool` has no member `wait`. Its public surface is `add_task`, `wait_for_tasks_to_complete`, `min_num_threads`, `max_num_threads` (`engine/core/thread_pool.h:49-58`), and the rethrow is documented on the second of those — `:51-54` "Blocks until every sub

### `tests/render/golden_image.cpp` — 3

- [x] **`:387-393`** *(medium, count)* *(unchecked)* — The set was 47 images when the measurement was taken (git ls-tree at 6af3e61 returns 47) and is 50 now, so 19 + 28 no longer sums to the set. Three images were added after the measurement and none of the numbers moved: contract-a-frame-may-be-read-back-and-the
- [x] **`:254`** *(medium, count)* *(unchecked)* — The check-mode run compares one frame per golden image, and there are fifty: tests/render/golden/ holds 50 PNGs, and pixel_tests.cpp:62-69 states the same census from the other end - "Fifty-three frames; fifty of them are 64x64 ... The other three are not". ch
- [x] **`:98-102`** *(low, dead-path)* *(unchecked)* — Exactly one case in pixel_tests.cpp reads a frame outside its subcases - "CONTRACT: read_back_buffer hands back exactly back_buffer_size", end() at :1507 with its single SUBCASE at :1515 - and it has one subcase, not two, so doctest runs its body once. Travers

### `CMakeLists.txt` — 2

- [x] **`:31-33 (claim at :32)`** *(medium, count)* *(unchecked)* — Thirteen test targets. The add_subdirectory list at :40-50 reaches AppTests, AssetsTests, AudioTests, CollisionTests, CoreTests, InputTests, LineSweeperTests, LineSweeperViewTests, MattMathTests, RenderTests, RenderPixelTests, SceneTests and UiTests — tests/li
- [x] **`:3`** *(low, count)* *(unchecked)* — Two samples, both added by this file: add_subdirectory(samples/minimal) at :37 and add_subdirectory(samples/linesweeper) at :38. The sibling sentence twenty-eight lines down was corrected to the plural by a56d198 — "it should certainly not build the engine's s

### `engine/render/d3d12/device_resources.h` — 2

- [x] **`:125-127`** *(medium, stale-backend-arithmetic)* *(unchecked)* — Four other backends now, and Vulkan has the identical rule declared in the identical place: engine/render/vulkan/device_resources.h:346-349, "Blocks until everything submitted against THIS frame index has finished. Called once at the top of a frame, before any
- [x] **`:10-14`** *(low, stale-backend-arithmetic)* *(unchecked)* — The paragraph it cites was amended and this echo of it was not. engine/render/renderer.h:583-586 now reads "Only null stops at three: d3d11, d3d12 and vulkan each add device_resources.cpp and gl adds gl_functions.cpp, and all four are the same kind of file, wh

### `engine/render/d3d12/renderer.cpp` — 2

- [x] **`:903-904`** *(high, count)* — It is the second longest. `engine/render/vulkan/texture_factory.cpp` is 378 lines to this one's 310 (235 code lines to 185), and the seam file says so in capitals: engine/render/renderer.h:588-596 lists "115 lines on d3d11, 310 on d3d12, 168 on gl, 48 on null 
- [x] **`:265-270`** *(medium, stale-backend-arithmetic)* — There are four other backends, and the fourth asks the same thing. engine/render/vulkan/renderer.cpp:874-878 opens begin_frame with "BEFORE ANYTHING RESETS A COMMAND POOL OR OVERWRITES A VERTEX BUFFER, which is the whole of what this backend adds to the seam's

### `engine/render/null/renderer.cpp` — 2

- [x] **`:33-35`** *(medium, stale-backend-arithmetic)* — Four other backends do it, and one of them says so in the same words. d3d12/renderer.cpp:145-147: "ONE TEXTURE PER DRAW CALL, so a run of sprites sharing one is one call and a change is a flush. That is the whole of the batching, and it is the same on every ba
- [x] **`:327-331`** *(medium, stale-backend-arithmetic)* *(unchecked)* — The second site of the same stale sum, in the function a reader reaches by hitting the throw. Four backends rasterise at HEAD (d3d11, d3d12, gl, vulkan) and all four reproduce the golden set, so the count is a fifth implementation agreeing with four hardware f

### `engine/render/null/texture_factory.cpp` — 2

- [x] **`:31-33`** *(high, stale-backend-arithmetic)* — Four other backends refuse it, and the enumeration of what is null misses one API. d3d11/texture_factory.cpp:80 `if (device_of(renderer) == nullptr)`; d3d12/texture_factory.cpp:110 the same; gl/texture_factory.cpp:90 `if (renderer.impl()->gl_context == nullptr
- [x] **`:37-38`** *(high, stale-backend-arithmetic)* — Five backends keep it, not three. The guard exists in all five texture factories (d3d11:80, d3d12:110, gl:90, vulkan:138, null:39), and this same folder already says so two files away: backend.h:100-105 was amended to "the other four backends" and "the stricte

### `engine/render/render_resources.h` — 2

- [x] **`:131-133`** *(medium, count)* *(unchecked)* — Four test files hold both and keep the order, each citing this paragraph: tests/render/pixel_tests.cpp:502-509, tests/render/null_tests.cpp:117-123, tests/render/renderer_seam_tests.cpp:56-61 and tests/scene/fanout_tests.cpp:101-104 ("The table before the rend
- [x] **`:212-214, repeated at :223 ("The throw is gone - a missing glyph draws a question mark now")`** *(low, behaviour)* *(unchecked)* — It installs one on every font that offers a candidate, and nothing on a font that offers none. engine/render/resource_factory.cpp:53-69 tries the file's own choice, then U'?', then U' ', and returns having set nothing if the font contains neither - its own hea

### `engine/render/sprite_geometry.h` — 2

- [x] **`:26-27`** *(high, stale-backend-arithmetic)* — There are five. The count here has always been of backends and not of rasterisers: `git log -L 22,42:engine/render/sprite_geometry.h` shows it read "Three backends cannot disagree ... because only one of them decides" while the three were d3d11, gl and null, a
- [x] **`:29-34`** *(high, stale-backend-arithmetic)* — There are three APIs behind this seam now and three answers, not two. engine/render/vulkan/renderer.cpp:1087-1095 is the third: "pane's BOTTOM edge and whose height is negative inverts y", `pane.height = -static_cast<float>(pixels.height);`. CLAUDE.md already 

### `engine/render/vulkan/device_resources.cpp` — 2

- [x] **`:2127`** *(medium, count)* — Three other sites in the same folder have exactly that shape, and two of them say so in the same idiom. engine/render/vulkan/renderer.cpp:1240-1244: "A VulkanBuffer HAS NO DESTRUCTOR AND THERE ARE SIX THROWING CALLS BELOW IT, which is the whole of why this blo
- [x] **`:1735`** *(medium, count)* — Of the five other vkCmdPipelineBarrier calls in the folder that name their stages by hand, exactly ONE says TOP_OF_PIPE. texture_factory.cpp:291 - `vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, ...)`. The oth

### `engine/render/vulkan/texture_factory.cpp` — 2

- [x] **`:111`** *(high, count)* — This file is 378 lines; engine/render/d3d12/texture_factory.cpp is 310, gl 168, d3d11 115, null 48 (wc -l at ca5b2e3). Vulkan's is the LONGEST of the five, not the second. The seam says so in the same words and with the same numbers: engine/render/renderer.h:5
- [x] **`:57`** *(low, stale-backend-arithmetic)* *(unchecked)* — Two backends refuse it, not three, and this is the second. engine/render/gl/texture_factory.cpp:72-79 throws on it unconditionally in source_format's default. Both Direct3D backends accept it: engine/render/d3d11/texture_factory.cpp:46-47 and engine/render/d3d

### `engine/render/animation_strip.h` — 1

- [x] **`:16-17`** *(medium, behaviour)* *(unchecked)* — The declaration it sits above returns by const reference: `const mattmath::RectangleI& frame_rect(int frame_index) const;` (`animation_strip.h:19`), and the body hands back a reference into the strip's own cache — `return this->frame_rects_[static_cast<size_t>

### `engine/render/camera_tools.cpp` — 1

- [x] **`:111`** *(medium, cross-reference)* *(unchecked)* — mattmath::clamp_ref does not exist. engine/math/scalar.h:87-88: "clamp_ref, the in-place form of each, is gone: it had no caller in the tree and could not have acquired a useful one, for the reason above." A grep across engine/ finds the identifier at exactly 

### `engine/render/colour.cpp` — 1

- [x] **`:360`** *(low, count)* *(unchecked)* — There were 148. The deleted function is in 9128495's diff of engine/math/colour.h: `inline mattmath::Colour colour_from_name(const std::string& name)` opens with `if (name == "ALICE_BLUE")` and ends with `if (name == "YELLOW_GREEN")` followed by an uncondition

### `engine/render/d3d11/texture_factory.cpp` — 1

- [x] **`:78-79`** *(medium, stale-backend-arithmetic)* *(unchecked)* — It is kept in five places now, one per backend: `d3d11/texture_factory.cpp:80`, `d3d12/texture_factory.cpp:110`, `gl/texture_factory.cpp:90`, `vulkan/texture_factory.cpp:138` and `null/texture_factory.cpp:40`, each throwing the same named runtime_error. The nu

### `engine/render/d3d12/device_resources.cpp` — 1

- [x] **`:261-262`** *(low, cross-reference)* *(unchecked)* — resource_factory.h argues nothing of the kind. Read in full it describes the split (":14-20, "Loading a texture is: work out the path, read the file, put the result in the table"), the file-extension rule (:36-41), the two throws (:46-50) and the after-create_

### `engine/render/dds_file.h` — 1

- [x] **`:20-25`** *(low, behaviour)* *(unchecked)* — Four of the seven are named throws, not each of them. dds_file.cpp names cube maps (:206-211), volume textures (:212-216), the DX10 extended header (:91-97) and formats outside texture_format.h (:98-100, :115-125) — and nothing else in the file throws for a me

### `engine/render/gl/gl_functions.h` — 1

- [x] **`:27-31`** *(medium, other)* *(unchecked)* — There is a third exception, declared eighteen lines below the sentence: `const GLenum GL_BGRA_ = 0x80E1;` (gl_functions.h:46). BGRA is not an accepted external format in GLES 3.0 - the token exists there only as BGRA_EXT via EXT_texture_format_BGRA8888 - and i

### `engine/render/label.h` — 1

- [x] **`:52-53`** *(low, behaviour)* *(unchecked)* — A scale change takes no measurement. `TextObject::set_scale` stores and nothing more (`text_object.cpp:106-109`), and `Text::set_scale` only forwards to it (`text.cpp:32-35`); the two setters that remeasure are `set_text` and `set_font` (`text_object.cpp:73-82

### `engine/render/null/recording.h` — 1

- [x] **`:63-68`** *(medium, stale-backend-arithmetic)* — There are four rasterising backends now, not two: d3d11, d3d12, gl and vulkan, which is the same four CLAUDE.md says the golden set holds - "one set of images that all four rasterising backends are held to". So a rasteriser here would be the fifth implementati

### `engine/render/null/render_resources.cpp` — 1

- [x] **`:50-51`** *(low, stale-backend-arithmetic)* *(unchecked)* — Four backends have a device and each keeps its own texture table with its own resource type: `ID3D11ShaderResourceView` (d3d11/render_resources.cpp:37), `D3d12Texture` (d3d12:48), `GlTexture` (gl:52), `VulkanTexture` (vulkan:81). The trailing clause is the onl

### `engine/render/render_resources.cpp` — 1

- [x] **`:23-26`** *(high, stale-backend-arithmetic)* — It is five times. Every one of those five members is defined once per backend folder: d3d11/render_resources.cpp:63,68,69,70,73,83; d3d12/render_resources.cpp:79,84,85,86,89,96; gl/render_resources.cpp:65,70,71,72,75,82; vulkan/render_resources.cpp:112,117,118

### `engine/render/resolution_manager.h` — 1

- [x] **`:40`** *(medium, behaviour)* — A dragged window edge does not arrive as WM_SIZE. engine/app/window.cpp:409 gates the WM_SIZE delivery on `!self->in_sizemove_`, and WM_ENTERSIZEMOVE sets that flag (:418), so every WM_SIZE Windows sends inside the modal drag loop is dropped. The size arrives 

### `engine/render/resource_factory.h` — 1

- [x] **`:17-20`** *(medium, count)* *(unchecked)* — Thirty describes the smallest of the five and nothing else. Measured at HEAD, add_texture_asset runs null/texture_factory.cpp:25-47 (23 lines), d3d11:68-114 (47), gl:82-167 (86), d3d12:102-309 (208) and vulkan:130-377 (248). The two large ones say why in their

### `engine/render/sprite_font_file.cpp` — 1

- [x] **`:23-27`** *(medium, count)* *(unchecked)* — There is no second place, on either reading of "two". Grepping DXGI over engine/ leaves exactly one file outside engine/render/<backend>/ holding a DXGI format number, and it is this one (sprite_font_file.cpp:28-30); every other hit is prose (texture_format.h:

### `engine/render/sprite_font_file.h` — 1

- [x] **`:22-25`** *(medium, cross-reference)* — The caller has not been backend code since 2889f12. `read_sprite_font_file` is called from engine/render/resource_factory.cpp:87, which is in the unconditional source list (engine/CMakeLists.txt:50) and is therefore compiled once for the whole build rather tha

### `engine/render/sprite_geometry.cpp` — 1

- [x] **`:126-127`** *(medium, behaviour)* *(unchecked)* — Both orders give 8 for that rectangle, so the example demonstrates nothing and the 7 is unreachable. RectangleF::right() is x + width (engine/math/rectanglef.cpp:58-61), so the code's order is trunc(18.9) - trunc(10.9) = 18 - 10 = 8, and the rejected order is 

### `engine/render/sprite_sheet.cpp` — 1

- [x] **`:17-18`** *(medium, stale-backend-arithmetic)* — `DrawList::draw_sprite` — the seam call `destination_from` feeds (`sprite_sheet.cpp:117-119`) — is implemented five times, once per backend: `d3d11/renderer.cpp:311`, `d3d12/renderer.cpp:366`, `gl/renderer.cpp:180`, `null/renderer.cpp:67`, `vulkan/renderer.cpp

### `engine/render/sprite_vertex.h` — 1

- [x] **`:11-13`** *(medium, stale-backend-arithmetic)* — Four backends have a vertex buffer now, and the fifth mechanism is a third one, not one of the two named. engine/render/vulkan/renderer.cpp:388-409 binds by explicit location - "The three fields of SpriteVertex, located by offsetof rather than by ... The locat

### `engine/render/text_encoding.h` — 1

- [x] **`:26-29`** *(low, behaviour)* *(unchecked)* — It installs one on every font that has a candidate, and on no other. `install_stand_in` tries the file's own choice, then '?', then ' ', and falls off the end doing nothing if the font has none of the three (engine/render/resource_factory.cpp:53-69) — which re

### `engine/render/text_object.cpp` — 1

- [x] **`:86-87`** *(medium, behaviour)* — No backend answers it. `RenderResources::measure_text` is compiled once, engine-side, at `render_resources.cpp:77-81`, and forwards to `Font::measure` (`font.h:117`); `grep -rn measure engine/render/*/render_resources.cpp engine/render/*/backend.h` finds no de

### `engine/render/viewport_manager.h` — 1

- [x] **`:28`** *(high, dead-path)* — No member of ViewportManager takes a context, and none touches D3D. The complete public interface is the constructor (:30), set_layout/layout (:32-33), player_viewport (:40), all_viewports (:42), viewport_dividers (:51) and fullscreen_viewport (:53); the priva

### `engine/render/viewport.h` — 1

- [x] **`:16`** *(medium, cross-reference)* *(unchecked)* — D3D11_VIEWPORT spells them MinDepth and MaxDepth. From the SDK this tree builds against, Windows Kits/10/Include/10.0.26100.0/um/d3d11.h:1128-1136: `FLOAT TopLeftX; FLOAT TopLeftY; FLOAT Width; FLOAT Height; FLOAT MinDepth; FLOAT MaxDepth;`. None of the six is

### `tests/render/renderer_seam_tests.cpp` — 1

- [x] **`:19-20`** *(high, cross-reference)* — The golden set is exactly that arrangement, and the commit that added it is titled "Hold two backends to one set of images, which nothing did" (6103b2f, 2026-08-20 - one day after 1753b73 wrote this sentence). tests/render/golden_image.h:21-27: "every frame th

---

## 3. Tests and checks that never landed

From [LEDGER.md](LEDGER.md). These are the 34 adjudicated items whose answer
was that nothing was built. Several are deliberate and should be ticked with a
reason rather than implemented — the ledger says which, and TEST-GAP's own
"if only seven land" line is the prior audit's attempt at the same triage.


**TEST-GAP.md Vehicle A (A1–A5): tests/render/renderer_seam_tests.cpp**

- [x] **A1** CONTRACT: a view capacity below one is refused, and it is invalid_argument
      Low. The invariant holds in all five and the refusal is one line at the top of each create_device, so nothing is broken today. What is missing is the ratchet: the check exists five times by hand-copy, and the only assertion in the tree is against a different f
- [x] **A2** CONTRACT: a marker is legal before there is a device
      Low in practice, non-zero in principle. The only marker call site in the tree, application.cpp:293/:296, sits between begin_frame and submit — after create_device by construction — so no shipping path reaches the null deref. The cost is a seam call with an unw
- [x] **A4** CONTRACT: window_size_changed before there is a device rebuilds nothing and says so
      Live but latent, and the honest severity is lower than the audit implied — because the return value has no reader. `engine/app/application.cpp:410` is `std::ignore = this->renderer_->window_size_changed(width, height);`, so the shell discards the "re-run the l
- [x] **A5** CONTRACT: a renderer with no device has no views
      Lowest of the five, and arguably the case that least deserved to land. Nothing diverges, nothing plausibly will: the answer falls out of two default-initialised members rather than out of a hand-written guard per backend, so there is no copy to drift. The only
      **DONE — `tests/render/renderer_seam_tests.cpp`, and the window is null because the refusal happens before anything touches it - so the case runs in all five configurations, which is where a statement about all five has to be made**
      **DONE — and it is the executable half of the markers decision. `renderer.h` declares markers advisory and legal before `create_device`; `d3d11/renderer.cpp` gained the three guards that make that true, its `ID3DUserDefinedAnnotation` being made with the device and dereferenced unguarded before it. Answers M4 and M5 in the same commit, which is what the ledger asked**
      **DONE — and the divergence it names is closed rather than described: all five backends early-out `false` before there is a device, each saying so in the same words, and the seam states it on `window_size_changed`**
      **DONE — three lines against two default-initialised members, and it pins that `view()` refuses rather than answering with a fullscreen pane**

**TEST-GAP.md Vehicle B — the harness prerequisite and B1..B7, against tests/render/pixel_tests.cpp at ca5b2e3**

- [x] **B2** A viewport confines a sprite bigger than its pane
      No golden covers it and none can: a golden records what a case drew, so a draw nobody made has no image. The nearest coverage is the four BLACK checks at :1249-1253 in "a viewport offsets the pane and scales the whole of it", and those catch a pane offset appl
- [x] **B6** Submit is once per frame, and a second one adds nothing (said to FAIL on gl)
      The behaviour is still divergent and is now written down in two places that contradict each other, which is the cheapest half of this to fix because it is prose. engine/render/vulkan/renderer.cpp:1195-1197 explicitly legalises the thing the seam describes as h
- [x] **B7** A batch longer than one vertex buffer wraps and keeps its order (2050 sprites)
      No golden covers it and no golden could, because a golden records a case and there is no case. A 2050-sprite frame at 64x64 would also be a poor golden subject; the assertion this needs is ordering and completeness (which sprite is on top after the wrap), not 
      **DONE — "a viewport confines a sprite bigger than its pane" - a sprite twice the pane in both axes, starting outside it. The case that already existed fills its pane exactly, so a backend that offset a pane and never clipped it passed; this one cannot. Byte-identical on all four rasterisers**
      **DONE — hardened rather than declared undefined, which is the decision §4 asked for. A second `submit()` is a no-op on all five - one flag per Impl, set by submit and cleared by begin_frame - `renderer.h` says so and says why "undefined" was refused, and `vulkan/renderer.cpp`'s contradicting paragraph now says what its render pass's finalLayout actually buys**
      **DONE — "a batch longer than one vertex buffer keeps its order", 2050 sprites with the last one somewhere else in another colour. What it asserts is order and completeness across a page boundary rather than a picture, which is what the ledger said it should**

**TEST-GAP.md Vehicle B, cases B8-B14, plus the "if only seven land" closing line**

- [x] **B8** a texture change keeps call order, it does not group runs
      The case is cheap and is now MORE valuable than in August 2026, because two of the five backends gained an explicit run vector since: gl `DrawList::View::runs` (engine/render/gl/renderer.cpp:137) and vulkan `View::runs` (engine/render/vulkan/renderer.cpp:150),
- [x] **B11** a source rectangle outside the texture clamps to the edge texel
      Cheap to write, and the argument for it is now stronger than in August 2026 because the number of independent copies of the decision doubled. What it costs to leave: four backends each hard-code CLAMP in their own vocabulary with no statement holding them toge
- [ ] **B12** every format the seam names is uploaded, or refused by name
      The lever is proven now — three hand-built TextureData helpers exist and one of them already carries a mip chain — so the remaining work is a loop over the six enumerators with valid block-compressed bytes and a `CHECK_THROWS`/`CHECK_NOTHROW` per backend expec
      **DONE — landed as finding 6 - see §1. The case is "a texture change keeps call order, it does not group runs"**
      **DONE — "a source rectangle outside the texture clamps to the edge texel", both ends: four texels out of a two-texel texture, and a negative source origin. A wrapping sampler answers RED where this asserts GREEN. Four backends hard-code CLAMP in four vocabularies and nothing held them together until this**

**TEST-GAP.md Vehicle C (five proposed null_tests cases + the release_device_resources grep) and the three structural claims about what CI cannot catch**

- [x] **C3** Vehicle C: null_tests case mirroring B6 — submit is once per frame, and a second one adds nothing
      The exact state TEST-GAP existed to stop, one backend wider. TEST-GAP found a 2-1 split and asked for a decision — harden or declare undefined. No decision was made: engine/render/renderer.h:425 "Called once per frame, between begin_frame and end_frame" is unc
- [x] **C5** Vehicle C: a released-handle/reload case
      release_device_resources() empties the texture table and leaves every TextureHandle live — engine/render/renderer.h:357-361 and engine/render/render_resources.h:95-110 both state that a handle resolved before a loss draws the right thing after the reload refil
- [x] **C6** TEST-GAP's grep: release_device_resources() is public seam API that nothing in tests/ calls on any backend — re-run at HEAD
      One entry point of the seam, five implementations, one production caller, zero assertions — unchanged since 2026-08-19 while the backend count went from three to five. The lever TEST-GAP identified for this class also landed in the meantime and is unused for i
- [x] **D-A** Drift found while adjudicating S1: renderer_seam_tests.cpp still says the two device-side test files never hold two backends to one statement
      The file a reader consults for "what does the seam answer identically everywhere" tells them no other file compares backends, in a tree where the mechanism that does is 550 lines away and was added specifically to end that state. The narrow half of the claim i
      **DONE — `tests/render/null_tests.cpp`, "submit is once per frame, and a second one adds nothing" - the one configuration that can assert it, because "adds nothing" is a statement about a recording. B6 above carries the decision it pins**
      **DONE — `null_tests.cpp`, "a handle resolved before release_device_resources draws after the reload" - release, resolve, reload through the real load path, then draw with the handle taken before any of it**
      **DONE — the same case. `release_device_resources()` had five implementations, one production caller and zero assertions anywhere in `tests/`; it has one now, and the precondition finding 2 wrote onto the seam is what it exercises**
      **DONE — amended in the drift pass. `renderer_seam_tests.cpp` now says what the golden set is and what the two mechanisms answer differently - configurations against processes**

**GAPS.md "Where the next pass should start", items 1-4, against the five-backend tree (HEAD ca5b2e3)**

- [ ] **GAPS-4** sprite_sheet.cpp destination_from written a second time and truncating where a glyph does not; is there a sprite_sheet_tests.cpp?
      Two things, and they are small but neither is zero. First, an object placed by position-and-scale snaps to whole pixels while text placed identically does not, and a client hitting it sees a sprite jitter against a caption that does not — the engine states bot

**GAPS.md "Where the next pass should start", items 5-8, adjudicated against the five-backend tree at ca5b2e3**

- [x] **G5a** text_encoding.cpp includes <Windows.h> and sits in the unconditional source list, so it is compiled into the null configuration
      Nothing to do, and doing it would be net-negative today. Removing <Windows.h> here means writing a UTF-8 to UTF-16 decoder by hand or moving the render API to char16_t, which android.md:492-497 prices as touching font.h, text_object.h and both file readers, fo
- [x] **G5b** widen() is the only producer of surrogate pairs while font.h walks per UTF-16 code unit with a bare static_cast<char32_t>; that crossing was never run
      Two CHECKs in tests/render/font_tests.cpp, no device: `font.first_unrenderable(widen("\xF0\x9F\x8E\xAE")) == 0` and a measure_text on the same string equal to two stand-in advances. That is the whole of it — RenderTests already links the engine and already bui
- [ ] **G6a** ViewportManager::fullscreen_viewport() sizes from resolution_manager_->resolution_vec(), not Renderer::back_buffer_size(); two caches kept in step onl
      Nothing structural is owed. The remaining hole is a test that a live Application keeps the two in step, which needs a real HWND and is therefore an AppTests entry that does not currently exist (see G6b). If you wanted to close it without a window, the cheapest
- [x] **G7a** draw_object.cpp set_draw_rotation_by_rectangle_rotated is a setter with an empty body and a TODO, silently discarding its argument
      Two lines and a test. `set_draw_rotation(rect.angle())` plus, if the pivot is wanted, `set_origin` at the rect's centre; then a Visual-level assertion in the null configuration, where a quad's four corners are readable (the machinery null_tests.cpp:553-636 alr
- [x] **G7c** rotation_origin.h has zero users anywhere
      Nothing, and this should come off any list of open items rather than being carried forward. It is a design-document refusal, not an unfixed defect: the one action GAPS implied (deletion) is the action PHILOSOPHY.md:671-680 forbids on exactly this evidence. If 
- [ ] **G8a** Twelve .cpp under engine/render/ with neither an axis nor a test file - how many have a test now?
      Ten files, ~700 lines of engine code, still with no executable statement of what they do. The cheap subset is large: DrawObject, Label, Text, TextObject and Visual are accessor-and-composition classes that need no device and would test in RenderTests. The thre
      **DONE — nothing to do, and doing it would be net-negative today, which is the ledger's own answer. `<Windows.h>` in `text_encoding.cpp` buys the UTF-8 to UTF-16 conversion; removing it means hand-writing a decoder or moving the render API to `char16_t`, which `docs/port/android.md` prices at `font.h`, `text_object.h` and both file readers. Ticked as decided rather than done**
      **DONE — `tests/render/font_tests.cpp`, "a surrogate pair is two code units, and the walk sees two of them". The crossing between `widen()` and the pen walk had never run; it does now, and what it pins is the limitation - two stand-ins for one character - rather than a fix, because no atlas this engine loads holds either half**
      **DONE — `set_draw_rotation_by_rectangle_rotated` had an empty body and a TODO, so it took a `RectangleRotated` and discarded it. It sets the rotation from `atan2` of the rectangle's x axis and leaves the origin alone, and `tests/render/draw_object_tests.cpp` - a new file, which also answers part of G8a - states the whole of `DrawObject`'s surface around it**
      **DONE — nothing, and it comes off the list rather than forward. `rotation_origin.h` having no users is a design-document refusal (PHILOSOPHY.md), not an unfixed defect: the action GAPS implied is the action that document forbids on exactly this evidence**

**DEFECT A — "GL anchors every pane to a cached back-buffer height; D3D11 needs no height at all" (docs/review/backend-equivalence/README.md:76-107, the**

- [x] **A-b** (b) Is window.cpp unchanged? — yes in every respect the audit cited; the shell still produces the desync twice
      The shell half of defect A is untouched and still produces both desyncs. What it costs has changed completely, because no backend now mis-places a pane from it: during a drag, d3d11/d3d12 stretch at Present (`DXGI_SCALING_STRETCH`, `d3d11/device_resources.cpp:
- [x] **A-f** NEW — the sentence defect A's fix amended, engine/render/sprite_geometry.h, is stale at five backends: it counts four and enumerates two of three answ
      A reader taking this file at its word gets a count that is off by one and a two-API framing of a three-answer term, in the one paragraph in the tree written specifically to record defect A's resolution. `docs/review/backend-equivalence/DRIFT.md:37` filed exact
- [x] **A-g** NEW — engine/render/renderer.h's back_buffer_size paragraph, the seam text that legislates defect A's term, enumerates three of five backends
      Low. Nothing behaves wrongly and no reader is told anything false — they are told about three of five backends in the paragraph that specifies the term this whole defect turns on, and have to find the other two at their own sites. It is the same shape as the t
- [x] **A-h** NEW — the shell's restore-from-minimise hole now yields a different wrong picture per backend, because Application::on_window_moved feeds back_buffer_
      Low and strictly milder than what defect A cost. Before the fix GL displaced the whole frame downward under a black band in this state; now it top-anchors and letterboxes, which the fix's commit message anticipated and scoped out as "a layout question and not 
      **DONE — nothing owed. The shell half of defect A is untouched and still produces both desyncs, and no backend now mis-places a pane from it - which is what the fix bought. Ticked as adjudicated**
      **DONE — amended in the drift pass: `sprite_geometry.h` counts five backends and enumerates all three answers to the pane term**
      **DONE — amended in the drift pass, with the same paragraph**
      **DONE — left as the shell layout question the fix's commit message scoped it as, and it is milder than what defect A cost: a restore-from-minimise now letterboxes where it used to displace the frame under a black band. Ticked as adjudicated rather than fixed - it needs `Application::on_window_moved`, not a backend**

**Defect B (docs/review/backend-equivalence/README.md §B) — begin_frame and the frame begun but never submitted, re-adjudicated at HEAD ca5b2e3 against **

- [x] **B-residual-drift** THE ONE THING STILL OPEN: the seam paragraph that specifies defect B's rule counts four of the five backends, and so does the test comment that pins i
      Two sentences and one test comment. Confirming costs the three greps above; the fix is a two-word edit in engine/render/renderer.h:370 plus a clause naming Vulkan's pool-and-layout reset, and a one-word edit at tests/render/null_tests.cpp:444. Cheap now, and c
      **DONE — both sentences and the test comment amended in the drift pass, and `renderer.h` now names four kinds of thing to drop rather than three, Vulkan's pool-and-layout reset being the fourth**

**DEFECT C — add_texture_asset before create_device (backend-equivalence audit, 2026-08-19, 57b65b3) and its TEST-GAP vehicle A3**

- [x] **C-d-A1245** The A-vehicle carries one of its five proposed cases: A1, A2, A4 and A5 did not land anywhere
      The vehicle is built, unconditional, and cheap to extend — each of A1/A4/A5 is three to eight lines against an object that needs no device, and the file's own :22-25 states the admission rule ("anything a Renderer answers before create_device, and anything tha
      **DONE — all four landed - A1, A2, A4 and A5 are in this file above**

**survey 2026-08-26 §6 — the two render decisions (reference machine; markers on the seam), adjudicated at HEAD ca5b2e3**

- [x] **R7** SITE 5 of 5 — engine/render/d3d12/device_resources.cpp:20-30: the site the survey counted but never named, never amended, and now in direct tension wi
      A self-contradiction inside `engine/render/`, and it is exactly the class this half of the sweep is for. Before `c8176ef`, "the low tier" was undefined and could be read as feature-level-10-era hardware, on which the sentence is true. After `c8176ef` the term 
- [x] **R9** samples/linesweeper/README.md contradicts itself about the reference machine, 18 lines apart, at HEAD
      One sentence, and it is the sentence a reader hits first — it sits in the measured-cost section that reports the 35.4 ns figure, so the stale half is the half doing rhetorical work. The conclusion both sentences reach ("no number here is a floor") is unchanged
- [x] **M1** WHETHER MARKERS STAY ON THE SEAM — the decision was not made, and neither branch of it was taken
      The seam is the only file a caller may read — `cmake/check_engine_includes.cmake` forbids any file outside `engine/render/<backend>/` from including anything in it — and it is the one file that does not say what a marker does. Every honest answer in this tree 
- [x] **M3** RECOMMENDATION — keep the three methods; amend renderer.h:490-502 to declare markers advisory. T6, PHILOSOPHY.md:106-119.
      One sentence in one file, against a five-file deletion that would also have to rewrite `engine/app/application.cpp:293,296` — the engine's own frame annotation, which is a real caller, not a speculative one. Suggested content, matching what the four folders al
- [x] **M4** The amendment has to answer TEST-GAP A2 in the same commit — neither half of that 2026-08-19 either/or was ever done
      The prior sweep REFUTED this as a divergence and I concur: d3d11's `begin_frame` faults in the same state through a co-lifetime pointer (`d3d11/renderer.cpp:780` returns a null context, `:789` dereferences it), so markers add no hazard the seam does not alread
- [x] **M5** Zero marker coverage in all five configurations, and the vehicle for it exists and holds exactly one case
      The seam's one advisory-capability claim would be unexecutable the moment it is written, unless A2 goes in with it. Everything else about markers has drifted precisely because nothing pins it (M6). The fix is small: the file compiles in all five configurations
- [x] **M6** gl/renderer.cpp:875 still says 'the other backend' — written at two backends, false at five
      Nothing executable. A reader of `engine/render/gl/` concludes markers are a two-backend affair in which GL is the outlier; at HEAD GL is in the majority of four and d3d11 is the outlier. Cost of the fix is one word. Listed here because it is the same class of 
      **DONE — the fifth site is named and amended. "Not a shipping target on the low tier" was true while the low tier was undefined and is not now: the named configuration is feature level 12_2. What the floor excludes is hardware older than it, which is a claim about the successor machine**
      **DONE — one sentence in `samples/linesweeper/README.md`. The reference machine is named; the p99 is what is still missing, and *Still open* eighteen lines down already said so**
      **DONE — decided: markers stay. `renderer.h` declares them advisory - a backend may forward them and may do nothing, and a caller may not tell which - which is the branch the sweep recommended and the one T6's own carve-out points at**
      **DONE — the amendment is on `renderer.h` beside the three declarations, and it says both halves: advisory, and legal before `create_device` and outside a frame**
      **DONE — answered in the same commit, and by the three lines rather than by a precondition: d3d11 guards on the device, so all five discard a marker before there is one**
      **DONE — `renderer_seam_tests.cpp` holds the marker case now, in all five configurations. There is nothing to observe and that is the assertion - the calls are reachable, in any order, before a device and outside a frame**
      **DONE — amended in the drift pass: one backend forwards, four discard, and gl is in the majority of four**

---

## 4. Decisions to make

These are not work items. Each is a question that has to be answered before the
code or the document that depends on it can be written, and each has been open
long enough that the sweep found it twice.

- [x] **Do markers stay on the seam?** `application.cpp` calls `begin_marker` on
      the frame path every frame; one backend forwards it and four discard it, and
      all four argue the discard in place. `set_marker` has zero callers
      tree-wide. No configuration covers any of it. The 2026-08-26 survey listed
      this as open and it still is. The sweep recommends keeping the three methods
      and amending `renderer.h:490-502` to declare markers advisory (T6), and says
      the amendment has to answer TEST-GAP **A2** in the same commit — neither half
      of that 2026-08-19 either/or was ever done. See [LEDGER.md](LEDGER.md) M1–M8.
- [ ] **What is the low tier's CPU half, and what is its p99?** `c8176ef` named
      the machine and said which half is measured. The GPU half is named; the CPU
      half is neither named nor measured, and **no p99 exists anywhere in the
      tree**. One of the five sites the survey counted was never amended and is now
      in direct tension with PHILOSOPHY. `samples/linesweeper/README.md`
      contradicts itself about the reference machine, 18 lines apart. This is a
      measurement rather than a sweep and it is the one item here that needs
      hardware. See [LEDGER.md](LEDGER.md) R1–R11.
- [x] **B13 — does a minified draw sample level 0 or the chain?** TEST-GAP called
      it blocked on a decision in 2026-08-19 and proposed declaring level 0. The
      ledger records what the five backends answer now. Decide it, then the test.
- [x] **Does `release_device_resources()` have a stated precondition, or does
      D3D12 gain a free list?** Finding 2 above is narrowed to an unstated
      precondition on public seam API, so the fix is a sentence *or* code and the
      choice is the decision.
      **DONE — a stated precondition. It is the device-loss half of a pair, and `render_resources.h` says so, says what a call outside that pair costs on D3D12, and says why the free list is the wrong trade**
      **DONE — they stay, and `renderer.h` declares them advisory: a backend may forward a marker and may do nothing, and a caller may not tell which from anything it can observe. TEST-GAP's A2 is answered in the same commit and by code rather than by a precondition - d3d11 guards its three forwards on the device, so a marker before `create_device` does nothing on all five - and `renderer_seam_tests.cpp` holds every configuration to it. `set_marker` keeps its zero callers: it is the odd one of the three to delete alone, and the pair above it has a real caller in `application.cpp`**
      **DONE — already decided, and decided the way TEST-GAP proposed. `renderer.h` states it beside `set_filter` - level zero always, under either filter - each rasteriser says it in its own vocabulary (`MaxLOD`, `GL_TEXTURE_MAX_LEVEL`, `maxLod`), and `pixel_tests.cpp`'s "a minified draw samples level zero, not the chain" pins it under both filters with a two-level texture built for the question. Ticked as decided-and-tested rather than open**

---

## 5. Finishing the sweep

From [GAPS.md](GAPS.md). The sweep is incomplete in ways it can name, and these
close it.

- [x] **Read one CI log** for `x64-release` and settle finding 1. Cheapest item in
      the folder.
      **DONE — read. Finding 1 above carries what it said and what was amended
      because of it.**
- [x] **Run the red team.** **DONE — all fourteen axes, 61 candidates raised,
      one survived two lenses, and it was a defect in this file rather than in
      the engine.** Each agent was handed one specific assumption of the first
      pass to attack and told to assume a divergence was present, because the
      hunt and the verification passes are both conservative: the hunt tends to
      conclude the backends agree, and verification can only subtract from what
      the hunt raised. So this is the strongest statement the sweep can make —
      **the invariant was attacked on the fourteen axes most likely to break it
      and it held.**

      One near-miss is worth keeping. The `viewport-origin` agent found that the
      argument the sweep used to *kill* its own zero-extent candidate was wrong:
      `Application::on_window_moved` round-trips `back_buffer_size()` into
      `window_size_changed()`, which is a fixpoint for the two D3D backends
      because theirs is a cache and is **not** one for GL, whose
      `back_buffer_size()` is a live `GetClientRect`; and `window.cpp`'s
      `WM_MOVE` is gated on `!in_sizemove_` with no `minimized_` guard where the
      `WM_SIZE` handler below it has one. Both verifiers refuted the candidate on
      reachability, so it is not a finding — but the reasoning that dismissed it
      the first time was wrong, and it is recorded here rather than lost. It was
      found by reading `engine/app/window.cpp` end to end, which is the file the
      2026-08-19 audit failed to open and the lesson its `GAPS.md` drew.
- [x] **Verify the 66 unchecked drift findings**, concentrated in `gl/` and the test
      files.
- [x] **Re-run `RenderPixelTests` under the Khronos validation layer with
      `validate_sync` on**, per [docs/review/vulkan/](../vulkan/) §2. It settles
      finding 4 and the Vulkan half of finding 7.
      **DONE — run, at the changed backend, with `validate_sync` and best
      practices on: 34 cases, 336 assertions, no errors, no SYNC-HAZARD, only
      `small-dedicated-allocation` performance warnings — and the fifty goldens
      byte-identical afterwards. It settles finding 4. The Vulkan half of
      finding 7 is prose rather than a layer question and is answered on
      `resource_factory.h`.**
- [ ] **The rasteriser fill rule against fractional edges**, still open from the
      prior audit and now with a third specification. `build_scaled_quad` truncates
      nothing so every glyph quad has fractional edges; D3D mandates top-left
      exactly, GL 3.3 leaves the tie-break implementation-dependent, Vulkan
      specifies it again. The golden set makes this quieter but does not close it,
      because `ALLOWED_CHANNEL_DRIFT` is 8 per channel and a one-row tie-break
      disagreement hides under that.
- [x] **Blend-result clamping, and GL never learning its framebuffer format.** GL
      asks `ChoosePixelFormat` and never calls `DescribePixelFormat`. `Colour`
      clamps per channel independently, so a legal tint of `(1,1,1,0.5)` gives
      `src.rgb > src.a`. Also still open from the prior audit.

      **DONE — verified as part of amending them, which is the only way that was ever going to happen: each was read against the code beneath it before a word was changed, and four came back needing a different amendment from the one the sweep predicted. §2 above names those four**
      **DONE — both halves, and one of them was already fixed. GL DOES learn its framebuffer format now - `gl/renderer.cpp` calls `DescribePixelFormat` after `ChoosePixelFormat` and refuses by name unless all four channels are eight bits, which is the demand D3D makes by naming a format. The clamping half is now a pixel case: "a tint with more colour than alpha saturates, it is not clamped to it" draws `(1,1,1,0.5)` over black and over an opaque ground, and all four rasterisers write it byte for byte - so no backend clamps rgb down to alpha on the way in, and the 8-bit UNORM write is where the clamp happens**

