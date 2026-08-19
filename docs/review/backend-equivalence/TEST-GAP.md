# The test gap

> Part of the [backend equivalence audit](README.md). Read-only, 2026-08-19,
> against the tree at `57b65b3`. Not updated as findings are fixed.

Nothing here is a defect. It is the answer to one question asked of every finding
and every legitimate difference in [INVENTORY.md](INVENTORY.md): **could
`RenderPixelTests` catch this, and does it?**

## What CI structurally cannot catch

`tests/render/pixel_tests.cpp` is 22 `TEST_CASE`s (21 labelled `CONTRACT:`) plus 7 `SUBCASE`s = the 28 cases, 99 assertion-macro lines. It runs against **whichever backend the preset configured**, never two in one process, never compared to each other, and `RenderPixelTests` is not built at all under `x64-debug-null` (`tests/render/CMakeLists.txt:62`). Three consequences:

1. **No "backend A agrees with backend B" statement is expressible in-process.** `LABRADOR_RENDER_BACKEND` picks one at configure time and asking for another is a missing symbol (T5). This covers every hand-copied duplicate: the pixels-to-clip constant (`d3d11/renderer.cpp:81-91` vs `gl/renderer.cpp:515-520`), the index loop (`:495-500` vs `:399-404`), the HLSL and its GLSL transliteration, and `draw_text`'s triplicated camera prologue. Each copy is separately correct; only a cross-backend diff sees drift. **The only mechanism that would catch it is a golden-image mode** — a `--dump <dir>` flag on the pixel harness writing each case's 64×64 read-back, one checked-in golden set, and a CI step that fails when the d3d11 and gl dumps differ. CI already builds all three. That is the fix for the whole class, and it is what would have caught the transform bug `pixel_tests.cpp:915-920` describes, in the configuration that did not have it.
2. **The harness is one view, one frame, one fixed window.** `create_device(window, 64, 64, 1)` (`:157-158`) and `set_view_count(1)` (`:191`) mean every multi-view path — D3D11's per-view deferred contexts, GL's per-view record-and-replay, view-order-versus-call-order — is unrasterised in every configuration. Every `begin()` pairs with an `end()`, so no abandoned frame exists. The `WS_POPUP` window's client rect equals `BUFFER_SIZE` forever, so defect A is unreachable by construction.
3. **The null recording is blind to the axes it would otherwise cover.** `RecordedSprite` (`recording.h:50-57`) carries no format, no batch boundary, no flush, no draw call, no triangle, no marker — and `null/texture_factory.cpp:25-34` reads neither `format` nor `levels` nor `pixels`. So the one configuration CI can run end to end cannot see any format, layout, mip, batching, blend or rasterisation term. No assertion changes that; it needs one paragraph beside the existing "AND WHAT THEY DO NOT" note (`null_tests.cpp:35-39`), which currently disclaims only colour and blending.

## The unused lever

`RenderTests` is built in **every** configuration, links `LabradorEngine`, and already constructs a `RenderResources` with no device (`render_resources_tests.cpp:16`). The `view_capacity < 1` check is the **first statement** of `create_device` on all three backends (`d3d11:617`, `gl:548`, `null:150`), before any window or device work. **A whole class of seam assertions runs in all three configurations on a machine with no GPU** and nothing uses it. Second unused lever: `add_texture_asset` is declared on `resource_factory.h`, names no backend type, and `TextureData`/`TextureLevel` are plain aggregates — **a test can build a texture by hand and hand it to a backend**, which unlocks format, stride and mip coverage with no new binary content.

## Proposed CONTRACT cases

**Vehicle A — new `tests/render/renderer_seam_tests.cpp`, added unconditionally to `RenderTests`.** No device needed; runs everywhere.

- **A1** `CONTRACT: a view capacity below one is refused, and it is invalid_argument` — `create_device(nullptr, 64, 64, 0)` and `(…, -1)` throw `std::invalid_argument`; `set_view_count(-1)` throws `std::out_of_range`. Passes on all three today; a lock.
- **A2** `CONTRACT: a marker is legal before there is a device` — **fails on d3d11 today** (`device_resources.h:92-105` derefs `m_d3dAnnotation` unguarded). Either the seam states markers need a device or d3d11 gains three lines.
- **A3** `CONTRACT: loading a texture before there is a device is refused, by name` — **defect C**. Passes on gl, AVs on d3d11, silently succeeds on null.
- **A4** `CONTRACT: window_size_changed before there is a device rebuilds nothing and says so` — d3d11 answers `false`, gl and null `true`. Needs a decision first; the call is guaranteed to happen in that state (`window.cpp:203-208` fires it from `ShowWindow` during window creation).
- **A5** `CONTRACT: a renderer with no device has no views` — `view(0)` and `set_view_count(1)` throw `out_of_range`.

**Vehicle B — `pixel_tests.cpp`.** Prerequisite, and itself a finding: raise the harness capacity from 1 to 4 and give `begin()` a view count.

- **B1** `read_back_buffer hands back exactly the back buffer, tightly packed` — `pixels.size() == w*h*4`. Never checked; `back_buffer_size()` is called by no test at all.
- **B2** `a viewport confines a sprite bigger than its pane` — all three existing `set_viewport` cases draw a destination *exactly* the pane's size, so no assertion has ever asked a sprite to overflow one, and confinement rests on two different API guarantees with no scissor anywhere.
- **B3** `views execute in view order, each under its own pane` — the ordering guarantee at `renderer.h:296-307` is the only promise `submit()` makes and on a device it is asserted by nothing.
- **B4** `no view state survives a frame boundary` — `null_tests.cpp:390` pins view-to-view; nothing pins frame-to-frame, and the three backends reset in three different places.
- **B5** `a frame that is begun and never submitted contributes nothing to the next` — **defect B**; must draw a sprite *and* text so the texture change forces a flush.
- **B6** `submit is once per frame, and a second one adds nothing` — passes on d3d11 and null, **fails on gl** (`replay()` never clears `runs`). Needs a decision: harden or declare undefined — three different undefined behaviours is what this audit exists to stop.
- **B7** `a batch longer than one vertex buffer wraps and keeps its order` — 2050 sprites. No view in either file has ever held more than three.
- **B8** `a texture change keeps call order, it does not group runs` — no case draws a sprite and text into one list.
- **B9** `a quarter turn turns the sprite clockwise on screen` — every `draw_sprite` in the file passes `0.0f` rotation.
- **B10** `the filter reaches the sampler, and linear is not point` — `TextureFilter::linear` reaches no device anywhere in the repository.
- **B11** `a source rectangle outside the texture clamps to the edge texel` — the seam names a filter and never an address mode; both backends chose clamp; nothing makes it a contract.
- **B12** `every format the seam names is uploaded, or refused by name` — hand-built `TextureData`, no new content. **41 of the 45 real images are bc3_unorm and no test in the repository has ever put one on a device.** Subcases for an unaligned stride and a two-level chain.
- **B13** `a minified draw samples <level 0 | the chain>` — **blocked on a decision.** d3d11 answers the coarse level, gl answers level 0. Cheapest resolution, given no content has a chain: declare level 0 and set d3d11's `MaxLOD` to 0.
- **B14** `after a resize the back buffer is the new size, corner to corner` — the closest CI can get to defect A, and it deliberately stops short.

**Vehicle C — `null_tests.cpp`,** mirroring B5, B4, B6, the dropped-view case, and a released-handle/reload case. `release_device_resources()` is public seam API and **nothing in `tests/` calls it on any backend.**

**If only seven land:** A3, A2, B5+C1, B6+C3, B12, B3, B4+C2. Decide B13 in the same commit even if the test lands later.

## What no assertion can reach

Defect A itself — the only honest observation point is the window's real pixels, outside the seam. Options: a `tests/app` case that pumps `WM_ENTERSIZEMOVE`/`WM_SIZE`/`WM_PAINT` and `PrintWindow`s the HWND; or **delete the term** by deriving the flip height from the drawable, which is what the six write-ups argue for. Also unreachable: the restore half of a device loss (no portable in-process trigger for `DXGI_ERROR_DEVICE_REMOVED`), threading (the fan-out is never entered — every caller takes the `count <= 1` early-out at `scene.cpp:171-176` — and a race that does not fire is not a passing test), and GL-internal hygiene (entry-point counts, the S3TC probe in process-global state, the texture name leaked on both format-refusal paths, `glDeleteSamplers` loaded and never called).
