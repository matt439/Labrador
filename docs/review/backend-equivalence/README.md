# Do the three render backends draw the same picture?

> Read-only audit by 199 agents, 2026-08-19, against the tree at `57b65b3`.
> `engine/render/d3d11/` (2,145 lines), `engine/render/gl/` (1,522) and
> `engine/render/null/` (669), held against the seam in
> [renderer.h](../../../engine/render/renderer.h) and the shared pixel arithmetic
> in `sprite_geometry`, `font`, `dds_file`, `sprite_font_file`, `camera`,
> `viewport`, `colour`, `texture_data` and `resource_factory`.
>
> Nothing was built and nothing was run — no `cmake`, no `ctest`, no device. Every
> claim here comes from reading source, and the arithmetic was done by hand.
>
> **This document is the audit as written, and is not updated as findings are
> fixed.** It is also the first review in `docs/review/` written after the repo
> split, so unlike [round 1](../README.md) and [round 2](../round-2/README.md) it
> cites no `game/` path and its line numbers were current at `57b65b3`.

**71 candidate divergences raised, 61 refuted, 10 confirmed — which collapse to
three defects, one of them live.**

| Document | What it holds |
|---|---|
| **README.md** (this file) | Method, the verdict, the three defects, and what was spot-checked |
| [INVENTORY.md](INVENTORY.md) | The 29 axes and what each backend actually does — the reference half |
| [TEST-GAP.md](TEST-GAP.md) | What CI structurally cannot catch, and 19 proposed `CONTRACT` cases |
| [DRIFT.md](DRIFT.md) | Claims in `renderer.h` and its siblings that the code no longer matches |
| [GAPS.md](GAPS.md) | What the audit did not examine, and what it capped off |

---

## Why this audit and not another

`tests/render/pixel_tests.cpp` is the executable pixel contract and it is good —
28 `CONTRACT` cases pinning every term the seam names. But it runs against
**whichever backend the preset configured**, never two in one process, and it is
not built at all under `x64-debug-null` ([tests/render/CMakeLists.txt:62](../../../tests/render/CMakeLists.txt)).
`LABRADOR_RENDER_BACKEND` picks one at configure time and asking for another is a
missing symbol (T5), so **no "backend A agrees with backend B" statement is
expressible in-process**. d3d11 and gl are each held to the same assertions
independently and never compared to each other. Any divergence those 28 cases do
not happen to exercise is invisible to CI by construction. That is the gap this
audit exists to fill, and it is a gap no assertion inside the present harness can
close — see [TEST-GAP.md](TEST-GAP.md).

## How it was done

| Phase | Agents | What it did |
|---|---|---|
| **Contract** | 5 | Extracted **294 falsifiable points** from the shared headers, the design docs and the two test files, and recorded which of the 28 `CONTRACT` cases pins each. Everything downstream was judged against that written ruler rather than against an agent's priors. |
| **Hunt** | 29 | One agent per equivalence axis, each holding **all three backends at once** for that axis. The axis list is the table in [INVENTORY.md](INVENTORY.md). |
| **Red team** | 18 | A second, adversarially framed pass over the 18 highest-risk axes: *assume the first pass missed something and a divergence is present.* |
| **Verification** | 2 per candidate | Every high or medium candidate attacked by two agents under distinct lenses — **does the code actually say this** (re-open every quoted line and read the enclosing function) and **can any caller tell** (a pixel, a recorded sprite, an exception, a lifetime — and is it already documented as intentional, or already pinned by a test). Default verdict REFUTED. Both lenses had to fail to refute. |
| **Critique** | 4 | Completeness, test-gap, documentation drift, and a severity/dedup pass. |
| **Synthesis** | 1 | The ranking below. |

**The verification layer did the work.** 61 of 71 candidates died there, and the
reasons are worth more than the survivors: the difference was inert (a depth range
with no depth buffer), out of domain (a `Viewport` no caller can construct),
unreachable on real content (a mip chain no `.dds` in either repository carries —
the verifier read `dwMipMapCount` at header offset 28 out of all 45), or already
documented with reasons the code still matches. Three of the refutations are
sharper than the claims they killed and are kept below.

## The verdict

The invariant holds. `NOTHING A BACKEND DOES DECIDES WHERE A PIXEL GOES` is true of every term the 28 CONTRACT cases pin and of every term they do not: all three backends call `build_sprite_quad`/`build_glyph_quad` with byte-identical arguments (`d3d11/renderer.cpp:304-310`, `gl/renderer.cpp:197-202`, `null/renderer.cpp:85-91`), both real backends derive the pixels-to-clip constant from the same `Viewport::pixel_rect()` integers that feed the rasteriser rectangle, both build the same `(0,1,2),(1,3,2)` index buffer, both specify the same eight premultiplied blend terms, and the GLSL is a true transliteration of the HLSL. **There is exactly one place where a backend decides where a pixel goes, and it is one line.** `engine/render/gl/renderer.cpp:511` places every pane against a back-buffer height the backend caches rather than one it queries, and the shell guarantees that cache goes stale during any window drag. That is a live bug on the GL preset today. The other two confirmed defects — a D3D11 deferred context that survives an abandoned frame, and three different answers to `add_texture_asset` before `create_device` — are latent: real, invisible to CI, and not reachable from any shipping client. [INVENTORY.md](INVENTORY.md), which records what the three do differently and legitimately, is the larger deliverable.

---

## The three defects

Ranked by whether a wrong pixel reaches a screen today. Only the first one does.

---

### A. **LIVE** — GL anchors every pane to a cached back-buffer height; D3D11 needs no height at all

*(Reported by six axes as six findings — clip-transform ×2, viewport-origin ×2, clear, resize. One line, one cache, one fix.)*

**d3d11** — `to_d3d_viewport` copies `pixel_rect().y` straight into `TopLeftY` (`d3d11/renderer.cpp:44-51`). D3D11 measures down from the render target's top-left, and the render target is a swap chain the backend created at exactly the size it was told. **The back-buffer height appears nowhere in the placement.** When the window and the swap chain disagree, `DXGI_SCALING_STRETCH` (`d3d11/device_resources.cpp:284`) presents a complete, top-anchored, merely-scaled frame.

**gl** — must convert to a bottom-left origin, and does it against `this->height`:

```cpp
glViewport(static_cast<GLint>(pixels.x),
    static_cast<GLint>(this->height - (pixels.y + pixels.height)),
    ...                                            // gl/renderer.cpp:510-513
```

`Impl::height` is a plain `int` written only by `create_device` and `window_size_changed` (`gl/renderer.cpp:580-581`). The thing it is supposed to describe — the WGL default framebuffer — is the window's live client area, and the backend's own comment says so: *"NOTHING IS REBUILT, WHICH IS THE WHOLE DIFFERENCE… the default framebuffer of a WGL context follows its window on its own"* (`gl/renderer.cpp:574-578`). The same stale number also stamps every view's default pane (`gl/renderer.cpp:606-611`), sizes the unscissored `glClear` (`:600-603`), and sizes `read_back_buffer` (`:692-694`).

**null** — stamps the same stale `Viewport` on every `RecordedSprite` (`null/renderer.cpp:27, :196-198`) and cannot express the condition at all.

**The shell produces the desync on purpose, twice.**

1. *Drag-resize.* Default style is `WS_OVERLAPPEDWINDOW` (`window.cpp:174`), class is `CS_HREDRAW | CS_VREDRAW` (`:159`) so every drag step repaints, `WM_PAINT` deliberately calls `notify_->tick()` — a full `render()` — while `in_sizemove_` is set (`window.cpp:347-349`), and **every `WM_SIZE` for the duration of the drag is discarded** by the `else if (self && !self->in_sizemove_)` guard at `window.cpp:394`.
2. *Restore-from-minimise.* The `else if (self && self->minimized_)` branch (`window.cpp:385-393`) consumes that `WM_SIZE` to clear the flag and calls `on_resuming()` only — it never forwards the size. A window minimised while windowed and restored maximised is wrong **for the rest of the run**.

**Observable effect.** Window at 800×600, dragged to 800×700, `impl_->height` still 600. `glClear` blackens the whole real 700-row framebuffer; every pane is then placed by `600 - (y + h)` counted from the bottom. The top pane lands at top-down rows 100..399, the bottom pane at 400..699 — **the entire frame, every pane, every sprite, every glyph, slid down by exactly 100 rows under a 100-row black band.** Drag the other way and the top rows of the frame are cropped off the top of the window. D3D11, same drag: the picture stretches smoothly and stays anchored top-left. The divergence is purely vertical — `gl/renderer.cpp:510` has no `width -` term — which is the signature of this term specifically and not of a general resize bug.

`read_back_buffer` diverges the same way, and asymmetrically: on a **grow**, GL's draw and GL's readback are both bottom-anchored by the same wrong height so the error cancels exactly and the image is correct; on a **shrink**, `glReadPixels(0, 0, width, height)` (`gl/renderer.cpp:700-702`) reads rows outside the framebuffer, which the GL spec leaves undefined, where D3D11's staging copy is sized from the back buffer's own `D3D11_TEXTURE2D_DESC` (`d3d11/renderer.cpp:830-832`) and is always exact.

**Coverage: none.** `grep -rn "window_size_changed\|back_buffer_size" tests/ bench/ samples/` returns **nothing** — not one call anywhere outside the engine. `RenderPixelTests` creates one fixed 64×64 `WS_POPUP` window whose client rect equals `create_device`'s arguments by construction (`pixel_tests.cpp:110-158`), so `impl_->height` can never be stale and the two viewport cases (`:928`, `:960`) exercise the flip only in the one configuration where the cache is right.

**Fix (one expression).** Have `Impl::replay` take its flip height from the window's live client rect — the backend already holds the `HWND` (`gl/backend.h:143`) — instead of from the cached `Impl::height`. *Honest caveat:* the pane rectangles are still sized from the stale number, so this turns a displaced frame into a correctly top-anchored letterboxed one — much milder than D3D11's stretch, and the version a player would not notice.

---

### B. **LATENT** — `begin_frame` discards an unsubmitted frame on GL and null, and defers it to the next frame on D3D11

**gl / null** — a view's recording *is* the frame. `View::reset()` drops it outright: `vertices.clear(); runs.clear();` (`gl/renderer.cpp:144-145`), `sprites.clear();` (`null/renderer.cpp:46`). A frame begun and never submitted leaves nothing behind.

**d3d11** — `View::reset()` clears only the CPU staging batch and then `this->bound = false;` (`d3d11/renderer.cpp:228-232`). It deliberately touches no context; `begin_frame`'s own comment says *"Nothing below this line touches a context"* (`:678-681`). The only thing in the backend that drains a deferred context is `FinishCommandList` (`:245`), reached solely from `View::finish()`, which `submit()` calls behind `if (!view.bound) { continue; }` (`:775-778`). So the moment `begin_frame` clears `bound`, a context holding an abandoned frame's commands becomes undrainable — and it is not lost either: the next frame's `set_view_count` sets `bound` back to true, and that frame's `submit()` gets **one** command list whose head is the stale frame's `DrawIndexed` calls, executed over the fresh clear.

`View::draw` only appends to the CPU batch, so a trivial single-texture frame strands nothing but the harmless bind. Anything that flushed — a texture change, a filter change, a `set_viewport`, or a full 2048-sprite batch (`d3d11/renderer.cpp:107-113`) — leaves real geometry behind.

**Observable effect.** A client that catches an exception out of its draw walk and keeps running sees, on D3D11 only, the aborted frame's already-flushed sprites composited over the *next* frame — a ghost pane one frame late. On GL and null the aborted frame simply never happened.

**Why it is latent, not live.** `Application::render()` (`application.cpp:280-300`) is straight-line with no `try`; the only escape is an exception, and both samples catch at top level in `wWinMain` and exit (`samples/minimal/main.cpp:48`, `samples/linesweeper/main.cpp:49`), so the process dies rather than drawing frame N+1. `renderer.h:306` also states the protocol the trigger violates (*"Called once per frame, between begin_frame and end_frame"*). It needs a client that catches a draw-path throw and continues.

One route makes it likelier than a caller error, and no axis named it: `Renderer::Impl::texture_size` contains a `ThrowIfFailed` (`d3d11/renderer.cpp:387`) and runs on every worker for every sprite; `ThreadPool::work_callback` stores the exception and `wait_for_tasks_to_complete` rethrows it on the joining thread (`core/thread_pool.cpp:68-93`), so a **device event** detected from a worker propagates out of `Scene::draw` mid-frame with `view_count` set, `touched` set and no `submit()`. That is a driver-triggered version of exactly this state, and it exists on D3D11 alone.

**Coverage: none.** Both harnesses always pair begin with submit (`pixel_tests.cpp:188-203`, `null_tests.cpp:83-101`).

**Fix.** Have `begin_frame` finish and release the command list of any view still flagged `bound` before it clears the flag, so no context can hold commands no `submit()` will drain.

---

### C. **LATENT, and diagnostic rather than pixel** — `add_texture_asset` before `create_device` gives three answers

| backend | behaviour | evidence |
|---|---|---|
| gl | throws `std::runtime_error` naming the texture | `gl/texture_factory.cpp:90-94` — an explicit `gl_context == nullptr` guard |
| d3d11 | **access violation** — `device_of()` is a bare `m_d3dDevice.Get()`, null before `CreateDeviceResources` | `d3d11/texture_factory.cpp:32`, `:92`, `device_resources.h:86` |
| null | **succeeds**, and hands back a resolvable, correctly-sized handle | `null/texture_factory.cpp:30-33` (`std::ignore = renderer;`) |

Nothing catchable on D3D11 — the crash address is inside the D3D runtime, not the loader. The sharp end is the third row: **null is the most permissive of the three about the one ordering rule the seam never states**, and null is the configuration CI runs completely. The rule appears nowhere on `renderer.h` for `add_texture_asset` (only obliquely, on `set_resources`), so the GL guard is the only place it is written down, inside the backend that enforces it.

**Why it is latent.** `Application::initialize` calls `create_device` (`application.cpp:121`) before `create_services` builds `RenderResources` and the loader, and `ResourceLoader` is the only in-tree caller. It costs a hand-rolled harness or a reordered shell to hit, and no wrong pixel is possible from it.

**Coverage: none.** Both harnesses create the device first (`pixel_tests.cpp:157`, `null_tests.cpp:68`).

**Fix.** Give D3D11's `add_texture_asset` the null-device guard GL already has, and put the create_device-before-load rule on the seam so null can be held to it too.

---

### Refutations worth keeping

Three of the 61 are more interesting than the claims they killed.

- **The choice of diagonal is unobservable, permanently.** `build_quad` emits `position + R(rotation) * ((CORNERS[i] - origin_ratio) * size)` — always a parallelogram — and attributes across a parallelogram are exactly affine, so `(0,1,2),(1,3,2)` and `(0,1,3),(0,3,2)` interpolate identically for every quad this engine can produce. Not "invisible until a rotated quad appears": invisible full stop. Meanwhile an *invalid* triangulation is already caught — `(0,1,2),(0,2,3)` leaves an uncovered wedge that the sample at `(6,2)` in `"texel (0,0) draws at the top left"` sits inside. Close this one; do not schedule it.
- **Mip sampling is armed, not broken.** D3D11 asks for `MIN_MAG_MIP_POINT`/`_LINEAR` with `MaxLOD = D3D11_FLOAT32_MAX` (`d3d11/renderer.cpp:474-481`); GL's samplers use `GL_NEAREST`/`GL_LINEAR` (`gl/renderer.cpp:439-450`), the two non-mipmapped minification filters, and sampler-object state overrides the texture object's. But I read `dwMipMapCount` at header offset 28 out of **every** `.dds` in both repositories — 2 here, 43 in ColourWars — and every one is 0 or 1, while `sprite_font_file.cpp:132` pushes exactly one level unconditionally. With `MipLevels == 1` D3D11 clamps LOD to 0 and the two are bit-identical. The claimed GL memory waste is also zero: there is no chain to upload. `dds_file.cpp:210-227` walks chains and both factories upload them, so **the first mipped `.dds` anyone exports splits the two backends with no code change.**
- **Per-backend format refusal is the contract, not a violation of it.** `resource_factory.h:73-81` states it: *"Throws `std::runtime_error` naming `name` and the format if the device will not take it, which is the answer a backend that cannot upload block compression owes rather than a blank texture."* GL refusing `b4g4r4a4_unorm` by name (`gl/texture_factory.cpp:73-78`) meets that floor. What does *not* meet it is D3D11's device-refusal message, which is `"Failure with HRESULT of %08X"` and nothing else (`core/throw_if_failed.h:36`) — and that is already pinned as such by `tests/core/throw_if_failed_tests.cpp`.

---


---

## Spot-checks

The load-bearing half of defect A was re-opened by hand after the run, outside the
agent layer, because it is the only finding that puts a wrong pixel on a screen.

| Claim | Result |
|---|---|
| `gl/renderer.cpp:511` flips y against a cached `this->height` | **Confirmed** — verbatim, and the comment three lines above declares it "the only place in the backend where that conversion happens" |
| `Impl::height` is written only by `create_device` and `window_size_changed` | **Confirmed** — `gl/renderer.cpp:553` and `:581`, and nowhere else; the same field also sizes the `glClear` (`:601`), the default pane (`:610`), `back_buffer_size` (`:687`) and `read_back_buffer` (`:693`) |
| `WM_PAINT` renders a full frame during a drag | **Confirmed** — `window.cpp:344-350`, `notify_->tick()` under `in_sizemove_` |
| every `WM_SIZE` during a drag is discarded | **Confirmed** — `window.cpp:394`, `else if (self && !self->in_sizemove_)` |
| restore-from-minimise consumes its `WM_SIZE` without forwarding a size | **Confirmed** — `window.cpp:385-393` calls `on_resuming()` only |
| `renderer.h:376` — "three translation units" | **Confirmed wrong** — d3d11 has four (`device_resources.cpp`), gl has four (`gl_functions.cpp`); only null has three |
| `renderer.h:379` — "the third file is thirty lines" | **Confirmed wrong** — `wc -l`: 101, 162, 35 |

**One correction, and it lowers a severity.** The audit did not open
`WM_EXITSIZEMOVE`. It should have: `window.cpp:408-417` calls `GetClientRect` and
forwards the real client size the moment the drag ends. So the displacement
defect A describes is **transient — it lasts for the duration of the drag and
self-corrects on mouse-release**, rather than persisting. It is still wrong on
every frame the user sees while dragging, which is precisely when they are
looking at the window, and the black band and the 100-row slide are real. But
"the whole frame is displaced until something else resizes the window" is only
true of the second trigger, the restore-from-minimise path, and only when the
size actually changed across the minimise. Read defect A with that scoped.

## What this audit capped off

Verification was capped at the two highest-severity candidates per axis, so
**16 high-or-medium candidates were raised and never verified** — concentrated in
`factory-split` (3), `seam-hygiene` (3), `error-parity` (2) and `pixel-logic-leak`
(2), with one each in `format-mapping`, `upload-mechanics`, `batch-breaks`,
`resize`, `gl-entrypoints` and `threading`. Those are unexamined, not cleared.
The rest of what was not covered is in [GAPS.md](GAPS.md).
