# What the three backends actually do, axis by axis

> Part of the [backend equivalence audit](README.md). Read-only, 2026-08-19,
> against the tree at `57b65b3`. Not updated as findings are fixed.

This is the reference half of the audit and the part with the longest useful
life. Every row is a real difference between the three backends that **no caller
can observe**, or that the seam explicitly delegates to a backend. The three
rows that are not — defects A, B and C — are written up in the
[README](README.md); they are marked here so the table stays complete.

The left-hand column is the axis list the hunt phase was decomposed into.

| Axis | d3d11 | gl | null | Verdict |
|---|---|---|---|---|
| **clip-transform** | `to_transform` reads the same `D3D11_VIEWPORT` `RSSetViewports` got (`renderer.cpp:81-91`) | same expression rebuilt per run from the same `pixel_rect()` (`renderer.cpp:503-520`) | no transform; stores the float `Viewport` (`renderer.cpp:52-55`) | agree bit-for-bit — **except defect A** |
| **viewport-origin** | `TopLeftY` absolute, height-free | y-flip against cached height | no conversion; float survives, pinned by `null_tests.cpp:351` | **defect A** |
| **scissor-clip** | `RSSetViewports` only; `ScissorEnable` FALSE by zero-init (`renderer.cpp:459`) | `glViewport` only; `GL_SCISSOR_TEST` never enabled | clips nothing | legitimate — with w=1 the clip volume's image *is* the pane on both; equivalent guarantees |
| **clear** | one `ClearRenderTargetView` on the immediate context (`:664`) | one `glClear`, unscissored, of the whole real framebuffer (`:603`) | `recorded.clear()` (`:191`) | legitimate; **defect B** rides here |
| **blend-state** | one immutable state object, re-bound **every flush** (`:186`) | three calls made **once per process** (`:461-464`) | none — nothing rasterises | legitimate: identical eight terms; nothing else in the process perturbs GL state (`opengl32` linked once, `engine/CMakeLists.txt:129`) |
| **sampler-state** | 2 `ID3D11SamplerState`, CLAMP, `MaxLOD` unbounded, mip filters | 2 sampler objects, `CLAMP_TO_EDGE`, no mip term | filter recorded per sprite (`:26`) | legitimate today; **armed** on the first mipped `.dds` |
| **depth-raster** | full desc re-bound per flush; `DepthEnable FALSE`, `CullMode NONE` | two `glDisable` calls once; eight other terms inherited from GL initial state | no rasteriser | legitimate — no DSV, no z, single sample, so every remaining term is inert |
| **vertex-layout** | `offsetof` + semantic binding, validated by `CreateInputLayout` | `offsetof` + `glBindAttribLocation` pre-link; only `GL_LINK_STATUS` checked | copies the struct whole (`:26-31`) | legitimate; D3D11's free cross-check is narrower than `sprite_vertex.h` claims (see [DRIFT.md](DRIFT.md)) |
| **index-topology** | 2048-sprite immutable IB; **start-index** addressing, `BaseVertexLocation = 0` (`:191-193`) | identical 2048-sprite IB; **base-vertex** addressing (`:525-528`) | no index buffer, no triangle | legitimate: same split points, same draw-call count, same order; addressing follows from the recording model |
| **batch-capacity** | 2048 bounds a fixed 256 KB per-view VB (`:531-537`) | 2048 bounds the shared IB; per-view vertex vector unbounded | unbounded `vector<RecordedSprite>` | legitimate — GL/null must hold a whole view because a worker may not touch the driver (`gl/backend.h:22-35`); on real content d3d11 is the heavier one (4 views × 256 KB eagerly) |
| **batch-breaks** | flush on texture / filter / viewport / cap / end-of-view | `close_run` on the same four | **no batching at all**, by design (`:33-37`) | legitimate; null is structurally blind to the axis |
| **format-mapping** | all six mapped; refusal delegated to the driver | five mapped; `b4g4r4a4` refused by name | reads `texture.format` never | legitimate — `resource_factory.h:73-81` authorises refusal by name; no file in either repo is 4444 |
| **upload-mechanics** | `SysMemPitch` per level; reads `offset/stride/size` | `GL_UNPACK_ALIGNMENT 1`; reads `offset/width/height/size` | reads none of it | legitimate: each half re-derives the other through its own API |
| **texture-size** | `GetResource`+`QI`+`GetDesc` **per draw** (`:376-393`) | cached `int` pair (`gl/backend.h:49-50`) | cached `int` pair | legitimate: `CreateTexture2D` stores what it was given, so all three return the same numbers; both sides document the trade-off |
| **readback** | staging copy at the driver's `RowPitch`, BGRA→RGBA swizzle | `glReadPixels(GL_RGBA)` + row flip, no swizzle | throws `std::logic_error` (`:290-293`) | legitimate and pinned (`null_tests.cpp:289`) |
| **frame-ordering** | deferred contexts, `FinishCommandList`/`ExecuteCommandList` | record to memory, replay on the context thread | copy vectors in view order | legitimate; **defect B** |
| **dropped-views** | `bound` flag; `submit` walks the capacity and drains every bound view | recording cleared at `begin_frame`; `submit` walks declared views only | same as gl | legitimate — d3d11's asymmetric loop is what stops idle views stranding commands (`backend.h:150-165`) |
| **error-parity** | 4 seam throws + `com_exception` from `View::bind` | same 4, byte-identical text | same 4, byte-identical text | legitimate; **defect C** is the exception |
| **device-loss** | full release/rebuild cycle, `DeviceNotify` fired | notify stored, never called (`:585-591`) | notify stored, never called (`:178-182`) | legitimate and declared (`gl/backend.h:133-137`, `renderer.h:363-372`) — but see the release-half note below |
| **resize** | rebuilds the swap chain eagerly; clamps to ≥1 (`device_resources.cpp:238-239`) | stores two ints, rebuilds nothing | stores two ints | **defect A**; the zero-extent clamp difference is unreachable (`resolution_manager.cpp:66-75` throws first) |
| **threading** | only backend that touches a driver from a worker; legal because `D3D11_CREATE_DEVICE_SINGLETHREADED` is set nowhere | recording touches no GL entry point; everything else pinned to the `create_device` thread | no affinity at all | legitimate and documented (`gl/backend.h:22-34`, `renderer.h:399-403`) |
| **shader-equivalence** | HLSL, fxc at build time, `vs_4_0_level_9_1` | GLSL 330 core, driver-compiled at device creation | no shader | legitimate: I diffed the two — same multiply-add, same fetch, same multiply, same constant |
| **gl-entrypoints** | loader-resolved import libs; nothing can be null | 36 hand-loaded entry points; `== nullptr` only | resolves nothing | legitimate: every name is core in the 3.3 context the driver already granted |
| **null-fidelity** | — | — | records handle, filter, viewport, four vertices; **no format, no batch break, no flush, no marker** | legitimate and declared (`recording.h:35-43`), but see [TEST-GAP.md](TEST-GAP.md) |
| **pixel-logic-leak** | 6 backend-owned pixel decisions | 6, two of them hand-copied duplicates of d3d11's | 1 (`texture_size`) | legitimate; duplication is the residual risk, not a present divergence |
| **factory-split** | 101 lines | 162 lines | 35 lines | legitimate; the seam's "thirty lines" claim is not ([DRIFT.md](DRIFT.md)) |
| **markers** | forwarded to `ID3DUserDefinedAnnotation`; **unguarded null deref before `create_device`** | no-ops, documented (`:717-723`) | no-ops | legitimate — the crash is the seam-wide use-before-`create_device` precondition, which `begin_frame` hits identically |
| **resource-tables** | `ComPtr` storage, borrowed raw pointer in, non-const pointee, extra by-name accessor | `unique_ptr<GlTexture>`, ownership transfer, const pointee | `unique_ptr<NullTexture>`, same shape as gl | legitimate; note GL's table entries **destruct by calling `glDeleteTextures`** (`gl/render_resources.cpp:25-31`) — a context/thread precondition on `release_device_resources` that no other backend has and the seam never states |
| **seam-hygiene** | 3 in-folder clients | 3 in-folder clients | 3 in-folder clients + `recording.h` for tests | clean — no backend type escapes any folder, verified in both directions |

Two inventory notes that are not defects but are worth carrying:

- **The release half of device loss is live on GL and one-way.** `release_device_resources()` is public seam API and fully wired on all three (`gl/render_resources.cpp:106-109` → `Registry::release_all()`), and on GL emptying a slot issues driver calls. But `on_device_restored` never fires there, so nothing reloads. Unreachable today — `Application::on_device_lost` is the only caller and only D3D11 fires it — but it is a public method with no path back on two of three backends.
- **The corrected seam-hygiene claim.** An earlier pass reported that the folder wall's regex misses three include spellings. It does not: `cmake/check_engine_includes.cmake:81` matches `[\"<](\.\./)*(engine/)?render/…` — both spellings and the relative form — and keys on the folder, not on `backend.h`. What *is* true is that the wall is invoked with `ENGINE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/engine` (`CMakeLists.txt:21`), so `samples/`, `tests/` and `bench/` are outside it — and the incident it was written for was in sample state files.
