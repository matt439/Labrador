# The Direct3D 12 backend, held to the seam it was written to test

> Read-only review by 86 agents across 27 lenses, 2026-08-20, against the tree
> at `d31a804`. `engine/render/d3d12/` — 2,762 lines in six files — plus the
> seam in [renderer.h](../../../engine/render/renderer.h), the relocated
> [sprite.hlsl](../../../engine/render/sprite.hlsl), the build
> ([engine/CMakeLists.txt](../../../engine/CMakeLists.txt),
> [CMakePresets.json](../../../CMakePresets.json),
> [ci.yml](../../../.github/workflows/ci.yml)) and
> [pixel_tests.cpp](../../../tests/render/pixel_tests.cpp). The two commits that
> made this backend — `3dda092` and `d31a804` — read together, because the
> second reworked the frame loop of the first.
>
> Nothing was built and nothing was run — no `cmake`, no `ctest`, no device.
> Every claim here comes from reading source.
>
> **This document is the review as written, and is not updated as findings are
> fixed.** Like the [backend equivalence
> audit](../backend-equivalence/README.md) it postdates the repo split, so
> unlike [round 1](../README.md) and [round 2](../round-2/README.md) it cites no
> `game/` path, and its line numbers were current at `d31a804`.

**149 candidates raised, 29 distinct after triage, 15 confirmed and 5 refuted;
11 more raised by gap probes and confirmed. Three must fix.** Nine ranked below
the verification budget and were never checked — they are open questions in §6,
not findings.

| Document | What it holds |
|---|---|
| **README.md** (this file) | The verdict, the findings in full, what was refuted, and where the coverage is thin |
| [all-findings.md](all-findings.md) | Every finding as the agent that raised it wrote it — evidence, failure scenario, fix — with the refutations underneath |

---

## How it was done

Twenty-seven lenses ran independently over the same tree, each told to go deep
on one axis and not to dilute it by reviewing everything: fence and
frame-in-flight synchronisation, command list and allocator lifecycle, resource
state transitions, the descriptor heap, the vertex ring, root signature and
pipeline state, swap chain and resize, device removal, the texture upload path,
threading, `HRESULT` handling, lifetimes, backend equivalence, seam purity,
pixel-divergence risk, the resize commit on its own terms, conventions, claim
drift, build and CI, test coverage, simplification, efficiency, the shader at
two profiles, degenerate inputs, comment quality, arithmetic and narrowing, and
the shape of the design. Each was given the three design documents, `CLAUDE.md`,
the three backends beside this one, and the standard that a comment the code
does not match is a defect — which is
[DRIFT.md](../backend-equivalence/DRIFT.md)'s.

Their 149 raw findings went through one triage pass that merged duplicates and
dropped 23 as non-findings. The top 20 by rank then went to **two adversarial
refuters each, both instructed to default to refuted** — one reading the code,
one checking whether the rule being invoked was real — with an adjudicator where
the two disagreed. Fifteen survived; the five that did not are in §7, kept so
nobody spends the budget re-deriving them. Three completeness critics then
looked for what twenty-seven lenses had missed and named six probes, each
refuted again before it counted; eleven findings survived that pass, and two of
them independently rediscovered §2.1, which is the strongest signal in the run.

**What was capped.** Nine of the 29 triaged findings ranked below the
verification budget and were never checked (§6). Three further probes the
critics proposed were not run. 86 agents, 2,740 tool calls, no errors.

---

## 1. Verdict

The backend is sound and the seam claim it exists to test holds: frames in
flight, fence-gated allocator reuse, a per-frame vertex ring and a waiting
upload path all stayed under `renderer.h`, and the ring itself is correct — I
traced the two-frame fence sequence by hand and `wait_for_frame` on
`frame_fences_[frame_index_]` always covers the last submission against the
allocator about to be reset. Nothing here is a reason not to ship. The single
most important thing to fix is that `d31a804` did not finish its own job:
`window_size_changed` decides whether there is a frame to restart with
`frame_open()`, a predicate that is false for the whole interval between
`begin_frame()` and the first `set_view_count()`, so on that arrival the new
back buffer is bound and drawn with no `RENDER_TARGET` barrier and no clear —
and `d3d11` ships the same predicate with the same missing clear. Second and
third, both new to this backend: `wait_for_gpu()` throws, which turns a device
removal on the resize path into an exception out of a window procedure and makes
two destructors `std::terminate` sites; and the shell destroys `RenderResources`
before `Renderer`, so every texture's `ID3D12Resource` is released ahead of the
only GPU wait on the shutdown path.

---

## 2. Must fix

### 2.1 The restart gate is narrower than the interval the seam promises — `engine/render/d3d12/renderer.cpp:1076` (and `engine/render/d3d11/renderer.cpp:716`)

```cpp
const bool restart = impl.frame_open();
impl.abandon_recording();

const bool rebuilt =
    impl.device_resources.window_size_changed(width, height);

if (rebuilt && restart)
{
    impl.open_frame();
}
```

`frame_open()` (`renderer.cpp:488-503`) answers "is a command list open right
now" — `frame_list_open`, or any `view->recording`. That is not the interval
`renderer.h:281-315` legislates. `begin_frame()` leaves both arms false: it
calls `abandon_recording()` (`:1121`), sets `view_count = 0` (`:1127`), then
`open_frame()` (`:1129`) — whose view loop `for (int i = 0; i <
this->view_count; i++)` (`:552`) opens nothing and whose `execute_frame_list()`
(`:546`) clears `frame_list_open` at `:581`. So the instant `begin_frame()`
returns, the frame has already spent its `PRESENT → RENDER_TARGET` barrier and
its clear on the old buffer, and `frame_open()` says there is no frame.

**Failure scenario.** A direct client of the seam does what `renderer.h:281-282`
declares legal ("IT MAY ARRIVE IN THE MIDDLE OF A FRAME"):

```cpp
begin_frame(); window_size_changed(32,32); set_view_count(1);
view(0).draw_sprite(...); submit(); end_frame();
```

`restart` is false, so `open_frame()` is skipped. Meanwhile
`create_window_size_dependent_resources` has set `back_buffer_states_[i] =
D3D12_RESOURCE_STATE_PRESENT` (`device_resources.cpp:297`) and re-read
`frame_index_` (`:381`). `transition_back_buffer(RENDER_TARGET)` has exactly one
call site — `renderer.cpp:537`, inside `open_frame()` — so nothing transitions
the new buffer. `set_view_count` calls `View::begin`, whose whole target setup
is `OMSetRenderTargets(1, &render_target, FALSE, nullptr)` (`:287`), and
`submit()` executes those lists. A swap-chain back buffer carries
`ALLOW_RENDER_TARGET` and is not simultaneous-access, and `RENDER_TARGET` is not
in the promotable-from-COMMON set, so this is a genuine
`INVALID_SUBRESOURCE_STATE`. `device_resources.cpp:233-234` installs
`SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE)`, so under
`x64-debug-d3d12` it breaks into the debugger. Without the layer the two
always-true consequences remain: the write happens in a state the spec does not
allow (most desktop drivers tolerate it — COMMON and RENDER_TARGET share a
layout), and **the clear never happened**, so the frame composites over whatever
`ResizeBuffers` left. `end_frame()`'s `transition_back_buffer(PRESENT)` then
records nothing, because the tracked state already reads PRESENT (`:593-596`).

**Scope.** The missing clear is not d3d12-only. `d3d11/renderer.cpp:716` is
`restart = restart || view.bound;`, and `bound` is set only by `View::bind` from
`set_view_count` and cleared by `begin_frame`'s `reset()` — identical width,
identical skipped `ClearRenderTargetView` at `:736`. Only the missing barrier
and the tracked-state desync are d3d12's. `gl/renderer.cpp:703-716` clears and
resets unconditionally, and justifies it with "the other two hand back a cleared
buffer" — which on this path they do not, so this reopens the three-way
disagreement `d31a804` exists to close.

**Reachability, stated honestly.** Nothing in this tree reaches it.
`Application::render()` (`application.cpp:280-300`) is `begin_frame /
begin_marker / StateContext::draw / submit / end_marker / end_frame`,
`begin_marker` is a no-op on this backend (`renderer.cpp:1336-1339`), and
nothing in either gap pumps messages — see §3.7, which is the other end of this
rope. `Harness::begin()` calls `begin_frame()` and `set_view_count()` back to
back (`pixel_tests.cpp:260-265`), so the new contract case at `:1559` always has
a view recording. This is high because the seam declares the call legal at any
point and `pixel_tests.cpp` is exactly the kind of direct client that can make
it, not because a sample crashes today.

**Fix.** Track the frame instead of inferring it: a `bool frame_begun` on
`Renderer::Impl`, set in `begin_frame()`, cleared in `end_frame()`, used as
`restart`. `open_frame()` is already correct at `view_count == 0` — it resets
the frame allocator, transitions, clears and opens no views — so calling it
whenever `rebuilt` is true and a frame has begun is the whole repair. Keep the
call after `device_resources.window_size_changed`, as it is now:
`open_frame()`'s `frame_allocators[frame]->Reset()` (`:534`) is safe only
because `create_window_size_dependent_resources` has already called
`wait_for_gpu()` (`:288`). Apply the same predicate to d3d11's `restart`, and
add a `pixel_tests` case that resizes between `begin_frame()` and
`set_view_count()`.

*One disagreement among the reviewers, resolved:* the `submit()..end_frame()`
window is **not** the same defect. `restart` is false there too, but nothing is
drawn afterwards and the buffer really is in COMMON, which is what `Present`
wants. That window costs one uncleared presented frame — cosmetic, and fixed by
the same `frame_begun` flag.

---

### 2.2 `wait_for_gpu()` throws, so a device removal on the resize path skips the recovery branch fifteen lines below it, and both new destructors are `std::terminate` sites — `engine/render/d3d12/device_resources.cpp:292`, `:56`, `engine/render/d3d12/renderer.cpp:441`

```cpp
// Nothing may be released while the GPU is still reading it, and a back
// buffer is the thing most likely to be.
this->wait_for_gpu();
...
    const HRESULT hr = this->swap_chain_->ResizeBuffers(...);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
        this->handle_device_lost();
        return;
    }
```

`wait_for_gpu()` (`:487-497`) guards only on null `command_queue_` / `fence_` /
`fence_event_` — a device removal nulls none of them — then calls
`signal_frame()`, whose `ThrowIfFailed(this->command_queue_->Signal(...))` at
`:466` is the throw site. (`wait_for_frame`'s `SetEventOnCompletion` at `:482`
is not reached: the runtime signals fences to `UINT64_MAX` on removal, so the
early-out at `:477-480` fires — which is also why there is no hang.)

**Failure scenario A, the resize path.** A TDR, a driver update or an adapter
reset lands, and the resize is the first thing to touch D3D afterwards — the
ordinary case while a window is being dragged, since `WM_EXITSIZEMOVE` arrives
while the TDR is still clearing. `window.cpp:415 → application.cpp:411 →
renderer.cpp:1077 abandon_recording() → create_window_size_dependent_resources →
:292 throws`. `ResizeBuffers` never runs, so the `DXGI_ERROR_DEVICE_REMOVED`
branch at `:309-317` — the only device-loss recovery this backend has outside
`present()` — is skipped in exactly the ordering it was written for. The
exception unwinds through `Renderer::window_size_changed` into
`Window::window_proc`, which has no catch, and whose caller is `DispatchMessage`
— the condition `d31a804`'s own message and `renderer.h:315` call "nowhere to
catch". D3D11 recovers from the same hardware event, and does so deliberately:
`d3d11/renderer.cpp:258-268` refuses to check `FinishCommandList`'s HRESULT
because "the one way it fails here is a device this frame's exception was
probably reporting in the first place", and `d3d11/device_resources.cpp` does
only void calls before `ResizeBuffers` so that `ResizeBuffers` is what reports
the removal. This backend copies the first half of that argument
(`abandon_recording` uses `std::ignore = view->list->Close();` at
`renderer.cpp:518`) and then loses it to a fence wait the other API does not
have.

**Failure scenario B, teardown.** Both destructors are implicitly `noexcept` —
every member has a non-throwing destructor. On a clean exit after an undetected
removal, `~Impl` (`renderer.cpp:441`) throws `com_exception` out of a `noexcept`
destructor: abort instead of exit 0, and the GPU wait the destructor exists for
never runs. If the removal instead surfaces as a throw on the frame path,
unwinding destroys the local `Application` in `wWinMain` before the handler at
`samples/minimal/main.cpp:48-56` runs, so neither the stderr line nor the
MessageBox appears. The seam also already forbids this: `renderer.h:260-261`
declares `Renderer(Renderer&&) noexcept` and `operator=(Renderer&&) noexcept`,
and `renderer.cpp:1018-1019` defaults both over `unique_ptr<Impl>` — a
move-assignment destroys the old `Impl` inside a function the seam explicitly
marks `noexcept`.

**Why it is wrong.** `PHILOSOPHY.md:116` is the rule, by name: T6 is "**Not a
licence for:** throwing on the way out — *teardown stays silent*". `3dda092`
edited `PHILOSOPHY.md` and did not touch T6, so this fights an unamended
philosophy. Microsoft's own D3D12 `DeviceResources` — the file
`device_resources.h:16-23` says this one is deliberately not a transliteration
of — declares `WaitForGpu() noexcept` and `SUCCEEDED`-guards both calls for
exactly this reason; the port kept the destructor and dropped the guards. No
other backend has this exposure: `d3d11`'s is `~DeviceResources() = default`,
`gl`'s `~Impl` is `wglMakeCurrent` / `wglDeleteContext` / `ReleaseDC`.

**Fix.** Split it: keep the throwing `wait_for_gpu()` for the frame path, and
give the four stall paths (resize, load, read-back, shutdown) a `bool`-returning
form that takes the HRESULTs by value. On `DXGI_ERROR_DEVICE_REMOVED` /
`DEVICE_RESET`, `create_window_size_dependent_resources` should fall through to
`handle_device_lost(); return;` — restoring `:309-317` as the single recovery
point it was written to be — and the two destructors should simply return.

---

### 2.3 Every texture's `ID3D12Resource` is released before the GPU is idled — `engine/app/application.h:240` / `:244`

```cpp
// DECLARATION ORDER IS LOAD-BEARING BELOW THIS LINE.
...
std::unique_ptr<Renderer> renderer_ = nullptr;        // :240
...
std::unique_ptr<RenderResources> render_resources_ = nullptr;   // :244
```

Reverse-order destruction means `render_resources_` dies thirteen members before
`renderer_`. `d3d12/render_resources.cpp:60` is
`RenderResources::~RenderResources() = default;`, which destroys
`Registry<D3d12Texture>` and with it every `ComPtr<ID3D12Resource>`
(`backend.h:100` holds the sole reference — a descriptor is not a reference, and
`Renderer::Impl` keeps only raw pointers). The only wait is one object away, in
`~Impl` (`renderer.cpp:441`).

**Failure scenario.** `Application::render()` ends at `submit()` —
`ExecuteCommandLists` over view lists that name those textures through
`SetGraphicsRootDescriptorTable` (`renderer.cpp:235-236`) — and `end_frame() →
Present(1,0)`, which queues the flip and returns. `~Application`
(`application.cpp:62-86`) is `clear()`, `audio_engine_->Suspend()`,
`CoUninitialize()`; I grepped `engine/app/` for wait/Flush/drain/idle and there
is nothing. So on any normal exit of either sample under `x64-debug-d3d12`,
every texture resource drops to refcount 0 while the GPU may still be executing
that frame. D3D12 does not keep resources alive for in-flight command lists the
way the D3D11 runtime does: the debug layer reports `EXECUTION ERROR #921
OBJECT_DELETED_WHILE_STILL_IN_USE`, and `device_resources.cpp:232-235` breaks on
both CORRUPTION and ERROR.

**Why it is wrong.** The seam already states the requirement and the shell
already breaks it: `renderer.h:318` says of the notify "Borrowed; the shell owns
it and outlives the renderer", and `:320-321` says of the table "Borrowed, like
the notify above." `PHILOSOPHY.md:433-434` is the general rule — "Construction
and destruction order are designed. If an ordering is load-bearing, it is either
designed away or stated where it lives." `device_resources.cpp:49-53` states the
backend's own invariant ("dropping a command list the GPU is still reading is
exactly the bug the fence exists to prevent, **and a destructor is no
exception**") and it holds only for what `Renderer::Impl` owns. On d3d11 and gl
the same declaration order costs nothing, which is why it has survived.

**Fix.** Declare `render_resources_` before `renderer_` so the `~Impl` wait runs
first — an `ID3D12Resource` holds its own reference on the device, so releasing
textures afterwards is fine. Note the harness has the same ordering
(`pixel_tests.cpp:461-462`) and is saved only by accident, because every `end()`
routes through `read_back_buffer`, which drains at `renderer.cpp:1299`; it would
go live the first time a test submits a frame it does not read back. So the
ordering rule belongs written on the seam beside `release_device_resources`, not
only fixed in the shell.

---

## 3. Should fix

### 3.1 Re-loading a texture under an existing name permanently burns a descriptor slot — `engine/render/d3d12/texture_factory.cpp:250`

```cpp
const int slot = impl.allocate_texture_slot(name);
...
resources.impl()->add_texture(name,
    std::make_unique<D3d12Texture>(resource, slot, texture.width,
        texture.height));
```

Unconditional, with no lookup of the existing entry. `allocate_texture_slot`
(`renderer.cpp:457-468`) is a monotonic bump with no free list, reset only at
`renderer.cpp:812` and `:992` — i.e. only when a device loss remakes the heap.
`Registry::add` (`registry.h:60-76`) replaces the entry and destroys the old
`D3d12Texture`, whose `slot_` is a plain `int` with no destructor, so the
`ID3D12Resource` is reclaimed and the heap slot is not.

The tightest trigger is a single manifest at startup: `load_sprite_sheet`
(`resource_loader.cpp:128-138`) loads the sheet's texture *under the sheet's own
name* — "they are one asset with two files, and the manifest names it once" —
and `load_manifest` (`:76-94`) has no duplicate check, so a manifest listing
`{texture, "tiles"}` and `{sprite_sheet, "tiles"}` double-loads inside one walk.
A client loading a manifest per level burns one slot per texture per walk; at 43
live textures the sixth walk throws *"this backend's descriptor heap holds 256
textures and they are all taken"* while 43 are live. The other three backends
run that client indefinitely: d3d11 releases the SRV on replacement,
`GlTexture::~GlTexture` deletes the name, null holds two ints.

The comment side is wrong from the first re-add, before any ceiling:
`backend.h:252` says the capacity is "HOW MANY TEXTURES MAY BE LIVE AT ONCE",
and the counter measures loads since the last heap creation.
`render_resources.cpp:46-53` reasons about only one of the two ways a slot stops
being needed.

**Fix.** Look the existing `D3d12Texture` up and write the new SRV into the slot
it already holds. This is legal here and worth saying so, since the obvious
objection is overwriting a shader-visible descriptor under an in-flight list:
`texture_factory.cpp:245-246` already does `execute_frame_list();
wait_for_gpu();` immediately before the allocation, so the GPU is idle at line
250. Amend three sites in the same pass — `backend.h:252`,
`render_resources.cpp:46-53`, and the throw text at `renderer.cpp:461-465`,
which is the one a user reads.

Nothing in this tree reaches it (both sample manifests are duplicate-free; every
`Harness` is a fresh device), so this is latent — but it is a bounded-resource
divergence on a seam operation `registry.h:60-61` and `render_resources.h:77-78`
both document as first-class ("Re-adding a name reuses its slot").

### 3.2 The null backend never got the mid-frame-resize term — `engine/render/null/renderer.cpp:170`

```cpp
bool Renderer::window_size_changed(int width, int height)
{
    if (width == this->impl_->width && height == this->impl_->height)
    {
        return false;
    }
    this->impl_->width = width;
    this->impl_->height = height;
    return true;
}
```

`d31a804` touched d3d11, d3d12, gl and `renderer.h`, and not this file. Run the
sequence in `null_tests.cpp`'s own vocabulary — `begin_frame();
set_view_count(1); draw_sprite(A); window_size_changed(w/2,h/2); draw_sprite(B);
submit(); recorded_sprites()` — and null returns **both** sprites, both carrying
the pre-resize `Viewport`, where the other three return B alone at the new
full-window pane. That is precisely what `pixel_tests.cpp:1610-1616` now
asserts, and `tests/render/CMakeLists.txt:63` does not build `RenderPixelTests`
under this preset, so the only test of the new term is absent from the one
configuration whose output *is* the recording. `renderer_seam_tests.cpp` never
calls `window_size_changed`.

There is a harder consequence than a divergent recording. `View::reset()` clears
`touched` on all three real backends, and every restart path calls it; null's
does not, so `touched` survives — and `null/renderer.cpp:215-238` throws
`std::logic_error` when the count is lowered past a touched view. The seam's
return value means "re-run the layout", so a shell that does exactly what it is
told — re-running layout mid-frame from 2 views to 1 — gets an uncaught
`logic_error` under `x64-debug-null` and silence on the other three. That
inverts `null/backend.h:99-105`'s own claim that this backend "has to be the
strictest of the three and not the most permissive": here it is both the most
permissive and, on the count path, strict about a condition the seam says no
longer exists.

`gl/renderer.cpp:679-698` already argued this case against itself in writing —
"it does not, because the difference would be observable ... what a client can
rely on is the same sentence everywhere". The argument transfers verbatim.

**Fix.** Two steps, the ones gl got: `view->reset()` on every view, and re-stamp
`view->viewport` from the new width/height as `begin_frame` does. Leave
`view_count` alone per `renderer.h:309-310`, and **do not** clear
`impl_->recorded` — `submit()` rebuilds it wholesale (`:260`), and `:209-213`
promises the recording stays readable until the next `begin_frame`. Then pin it
in `null_tests.cpp`, the one configuration that can assert on what a frame
submitted.

### 3.3 `renderer.h` still counts three backends in eight places, and one paragraph asserts it was checked

`renderer.h` is the specification of the seam — `DRIFT.md`'s own preamble says
so — and `3dda092` renumbered `:40`, the `back_buffer_size` paragraph, `:467`,
`:482-497`, `:523` and the "WHAT IS STILL SEPARATE RUNS" heading while leaving
these:

| Line | Claim | Actual |
|---|---|---|
| `:340-347` | "the three backends have three different things to reset. Two of them hold a frame in a vector; the D3D11 one holds it in a deferred context" | Four backends, and d3d12's thing to drop — an open command list, which cannot be reset and whose allocator cannot be reset under it (`renderer.cpp:505-529`) — is not in the list |
| `:592-594` | "that paragraph already said the three backends had three different things to drop, which is why it needed no rewriting to take a fourth" | Checkable and false; the paragraph still reads "the three backends". The commit *message* writes the same sentence without the numeral, so the message is careful where the header is not |
| `:359-362` | "on two backends it is a vector, on the third a deferred context. All three throw it" | Four throw it (`d3d11:838`, `d3d12:1156`, `gl:777`, `null:233`), and d3d12's stranded recording is neither listed thing |
| `:375-376` | "two of the three replay a vector, and the D3D11 one executes a command list per view — a protocol (record, FinishCommandList, ExecuteCommandList, Release)" | d3d12 executes a command list per view too, in **one** `ExecuteCommandLists` over an array (`:1197-1236`), using none of that protocol — the one submit shape a fourth backend actually introduced is the one omitted |
| `:420-422` | "the D3D11 back buffer is BGRA and is swapped on the way out, the GL one is asked for as RGBA and only flipped" | Each clause is still true; the enumeration reads as exhaustive and now misses a third rasteriser that is also BGRA, also swaps, and additionally unpads a 256-byte row pitch |
| `:444-445` | "it is the only one of the three with such a wrapper to strip, because it is the **only one whose device can be lost**" | Contradicted 27 lines below by text the same commit added: "D3D12 loses a device the same way D3D11 does, so the hazard belongs to half the backends rather than to one". The strip clause survives on a narrow reading (d3d12's file was written fresh, so it never had vendored accessors to strip); the device-loss clause does not. This sentence is the *fix* for a previous DRIFT item — `git log -S` shows `a56d198`, "Correct thirty claims that stopped being true" — so the project has paid to correct it once already |
| `:524` | "pass the same assertions - 308 of them at the last count" | `d31a804` added five unconditional CHECKs (`pixel_tests.cpp:1593, 1607, 1608, 1615, 1616`) and a second call site for the `REQUIRE` in `Harness::resize_window`. Say "over three hundred", since the next clause already says the number is not the point |
| `:528` | "IT IS TWO RUNS AND ONE SET OF IMAGES" | Three; the same paragraph says so eleven lines below ("d3d11, d3d12 and gl reproduce all forty-seven exactly"), and the sibling heading *was* rewritten in the same commit |
| `:557` | "`renderer_seam_tests.cpp` is the part that runs in all three configurations" | Four — it is an unconditional source in `tests/render/CMakeLists.txt:3-17`. This is the one line a maintainer uses to decide where a device-free seam assertion must hold, and it under-counts |

Fix `d3d12/renderer.cpp:1115` in the same pass — "The three backends have three
different things to drop" followed by a colon list naming four — and
`d3d12/renderer.cpp:902`, "the third file of this backend is the longest of the
three", where the seam's own vocabulary at `:479-491` makes it the longest of
four.

### 3.4 `pixel_tests.cpp`'s frame census is wrong three ways, in the file that is the golden set's coverage statement — `tests/render/pixel_tests.cpp:62`

```
// argument. Forty-eight frames; forty-seven of them are 64x64 on every backend
// and identical across the two that rasterise. The forty-eighth is read out of
// a buffer the seam says is a different size per backend, and
// Harness::end_not_comparable is where that is written down.
```

`grep -c "harness\.end();"` returns 47; `end_not_comparable()` is called at
`:1497` and `:1603`; `tests/render/golden/` holds 47 images. So 49 frames, 47
golden-compared, two exempt. "The two that rasterise" is three since `3dda092` —
which never opened this file. And the stated reason no longer covers the second
exemption: the header says the exempt frame "is read out of a buffer the seam
says is a different size per backend", where `:305` says the new one "asks for a
32x32 buffer deliberately and gets it on every backend" — the opposite reason.

Three more in the same family: `:316-317` still says "Every other frame here is
64x64 on every backend", *inside the comment block `d31a804` rewrote*;
`:311-314` quotes `renderer.h` in quotation marks as "the D3D11 backend answers
the size it was told", a sentence `3dda092` rewrote to "both Direct3D backends
answer the size they were told", so the citation can no longer be checked
against its source; and `:340` says "Every case but one knows both are
`BUFFER_SIZE`". Outside this file: `CLAUDE.md:69-70` still says "the **one**
frame whose size the seam makes backend-specific" — and `3dda092` edited the
line directly above it — and `golden_image.h:14-15` / `:25` still say "the two
real backends" and "two runs, two configurations, one set of images".

This matters operationally: `CLAUDE.md` tells a contributor to regenerate with
`LABRADOR_GOLDEN_DUMP=1` and review every image it changes. A reviewer
reconciling 48 declared frames against 47 images looks for a missing image; at
49 with two exemptions the check cannot be performed at all.

`d31a804`'s message claims the amendment was made — "The comment on
`Harness::end_not_comparable` now covers both, rather than saying 'the one
frame'." True of that one comment; false of the file header, of `:316`, of
`:340`, of `CLAUDE.md` and of `golden_image.h`. `git log -L
60,66:tests/render/pixel_tests.cpp` shows four prior commits each amending this
sentence in the commit that changed the count; this is the first that did not.

### 3.5 `gl/sprite_shader.h:6` sends the reader to a path this commit deleted

```
// SIDE BY SIDE WITH engine/render/d3d11/sprite.hlsl, deliberately.
```

`3dda092` did `git rm engine/render/d3d11/sprite.hlsl` and added
`engine/render/sprite.hlsl`, and updated every other cross-reference —
`CLAUDE.md`, `ARCHITECTURE.md:121` and `:318`, `renderer.h:499`, all four
`compile_hlsl` calls. This one it missed, and it is one half of a pair:
`sprite.hlsl:20` points back at `render/gl/sprite_shader.h` correctly. This is
the only instruction anywhere to compare the two shader *sources* by eye — the
golden set catches behavioural divergence, not structural — and it opens
nothing. Lines 3-4 are stale in the same stroke: "a transliteration of the only
shader **the other one** has" no longer picks out a unique referent. So are
`:13` ("COMPILED AT RUN TIME, WHERE THE OTHER ONE IS COMPILED AT BUILD TIME" —
two others now, at different profiles) and `:36` ("the same constant **the other
backend** uploads" — d3d12 does not upload it, it passes four root constants).
Do not touch `DRIFT.md:39`, which cites the old path correctly as history.

### 3.6 GL compares `reported_` against a number the shell does not feed it — `engine/render/gl/renderer.cpp:655`

`gl`'s guard compares `impl_->reported_width/height`; `back_buffer_size()`
(`:826-834`) returns the **live** `GetClientRect`. Both Direct3D guards compare
exactly what their own `back_buffer_size()` returns, so
`Application::on_window_moved` (`application.cpp:381-386`) — which sources its
argument from `back_buffer_size()` — is a tautological self-comparison there and
always returns false. On gl it is not.

`window.cpp:359-364` forwards `WM_MOVE` with no gate at all, while `:394`
suppresses every `WM_SIZE` for the duration of a drag. Drag the **left or top**
edge and the origin moves, so `WM_MOVE` fires per step: each one calls
`window_size_changed` with the already-current client rect while `reported_` is
still the pre-drag size, so the guard falls through and the new `d31a804` block
at `:679-716` runs a `glViewport` + `glClear` + `view->reset()` per mouse-move
step. Then `WM_EXITSIZEMOVE` forwards `GetClientRect`, `reported_` already
equals it, and **gl returns `false` to the one message that ends a resize** —
where d3d11 and d3d12 both find the old output size, rebuild, and return `true`.
Which answer you get depends on which edge the user grabbed. Nothing breaks
today, because both call sites discard the bool and `application.cpp:409`
re-runs the layout unconditionally; the costs are the redundant per-message GPU
work and a seam term two backends answer differently.

It also falsifies the premise `d31a804` writes into
`d3d11/renderer.cpp:692-693`: "Application::on_window_moved calls this with the
size it already has on every move of the window" — true of a swap-chain backend,
not of gl.

**Fix.** Gate `WM_MOVE` on `!in_sizemove_` the way `WM_SIZE` already is, or have
`on_window_moved` pass the shell's own number
(`resolution_manager_->resolution_ivec()`) rather than asking the renderer what
it is drawing into. Do **not** "fix" gl to compare the live rect —
`gl/renderer.cpp:667-675` is the argument against exactly that, and it is right.

---

### 3.7 `renderer.h` justifies the new mid-frame-resize term with two delivery paths this tree cannot produce — `engine/render/renderer.h:293-299`

```cpp
// IT IS NOT A RULE THE CALLER COULD KEEP EVEN IF THIS FILE STATED ONE.
// A resize reaches the shell as a window message, and a window message
// can be delivered while the shell is inside a frame: engine/app/
// window.cpp renders from WM_PAINT, and a vsync Present is entitled to
// pump.
```

This paragraph is the sole written justification for the term the seam gained in
`d31a804`, and both of its supporting facts are checkable against files it names
by path. Neither holds.

**(a) "window.cpp renders from WM_PAINT"** — `window.cpp:344-350` renders from
`WM_PAINT` only under `if (self && self->in_sizemove_)`, and `:394` forwards
`WM_SIZE` only under `else if (self && !self->in_sizemove_)`. The conditions are
complements: in exactly the state where the shell renders from a message, it
throws every resize away. `renderer.h:392-394` says so itself — "the one state
where they need not is a drag-resize, during which the shell discards every
WM_SIZE and still asks for frames". The two paragraphs in one file cannot both
be right. (The mechanism is also inverted: `WM_PAINT` puts a frame inside a
message, not a message inside a frame.)

**(b) "a vsync Present is entitled to pump"** — the only `Present` is
`device_resources.cpp:443`, reached only from `DeviceResources::present()`,
whose only caller is `Renderer::end_frame` (`renderer.cpp:1136`).
`application.cpp` calls `submit()` at `:296` and `end_frame()` at `:299`. So
whatever window a pumping Present opens, it opens *after* `submit()` — never in
the interval the sentence names. Same on d3d11 (`device_resources.cpp:396`). The
only message retrieval in `engine/`, `tests/` and `samples/` is
`window.cpp:239-242`, in the `else` arm of `pump_until_quit`, with no frame on
the stack.

The false version has been copied into `pixel_tests.cpp:1563-1566`, so the copy
cannot be used to check the original. This is why §2.1 is a seam-reachability
defect rather than an observed crash: the gap is unreachable today *precisely
because* the paragraph's model is false, and the paragraph is what would let
someone believe it had been considered.

**Fix.** Amend to a mechanism this tree can produce. The honest one is
synchronous and in-stack rather than pumped: `Application::set_resolution`
(`application.cpp:213-221`) calls `Window::resize_client`, whose `SetWindowPos`
(`window.cpp:268`) sends `WM_SIZE` straight to the window procedure on the same
thread — so any caller changing resolution while a frame is open gets
`window_size_changed` inside that frame with no pump involved. If the
drag-resize argument is to stay, `window.cpp:394` has to be able to deliver it.
Either way the term itself is sound; it is the reasoning under it that needs
rewriting, and the copy in `pixel_tests.cpp` with it.

---

## 4. Minor

- `engine/render/d3d12/renderer.cpp:264` — "`Renderer::begin_frame` has already
  waited on the fence for this frame index" names one caller as the invariant;
  `d31a804` gave `View::begin` a second caller in `open_frame()`, reached from
  the resize path, where `frame_index_` was re-read after `ResizeBuffers` and
  may differ. The reset is safe, but because of `wait_for_gpu()` at
  `device_resources.cpp:292`, which this paragraph does not name.
  `backend.h:210-214` (`recording` "Deferred to set_view_count") and
  `backend.h:148-150` (page reuse) lean on the same unnamed wait — one omission,
  three sites.
- `engine/render/d3d12/backend.h:327` — "so a caller that wants to add to what
  `begin_frame` recorded can" describes a sequence no caller can produce:
  `open_frame()` executes and closes the list before returning, as does every
  other entry point. The method's real job is idempotence *within* one operation
  (transition + clear; transition + copy + transition), which it does correctly.
- `engine/render/d3d12/backend.h:22` — "Every client of it is in this folder -
  the four .cpp beside it". Three include it; `device_resources.cpp` does not
  and structurally is not one. `d3d11/backend.h:23-31` removed exactly this
  count "because the list went stale twice".
- `engine/render/d3d12/backend.h:106-108` — "Only the first of the three tables
  is this backend's" is contradicted six lines later at `:112-114` and by
  `render_resources.h:216-217`. Copied verbatim from `gl/backend.h:77-79`, which
  has the same self-contradiction; pre-existing, worth fixing in both.
- `engine/render/d3d12/render_resources.cpp:41` — "The same short list the other
  **two** backends keep". Three.
- `engine/render/d3d12/device_resources.cpp:49` — "THE ONE DESTRUCTOR IN
  `engine/render/` THAT HAS TO DO ANYTHING". Four have bodies;
  `renderer.cpp:431` is in the same folder, from the same commit, and does the
  same wait. It misdirects exactly the audit §2.2 requires. Narrow it to "the
  only one that has to synchronise with a GPU" and cross-reference the sibling.
- `engine/render/sprite.hlsl:13-17` — "WHAT A BACKEND DOES OWN IS THE PROFILE
  AND THE BINDING, and both are in `engine/CMakeLists.txt`". That file contains
  no register, root constant or constant buffer, and `:119-120` says so itself
  ("what each owns is the profile it asks for and the header the bytes land
  in"). The binding is at `d3d12/renderer.cpp:641-671` and
  `d3d11/renderer.cpp:180`; `d3d12/backend.h:57-59` already points correctly.
  Secondary, and pre-existing rather than new:
  `cmake/compile_shaders.cmake:14-16` says the byte array is "handed straight to
  `CreateVertexShader`", which D3D12 does not have — the bytes go into
  `D3D12_GRAPHICS_PIPELINE_STATE_DESC::VS` at `renderer.cpp:722-723`.
- `engine/render/d3d12/device_resources.h:106-109` — "Under signal-at-present
  that client would reset a command allocator the GPU was still reading from,
  **every frame**". `read_back_buffer` ends in `wait_for_gpu()`
  (`renderer.cpp:1299`), and every one of the harness's 49 frame terminations
  routes through it, so the hazard is live at zero frame boundaries in that
  file. Twelve lines below, the same header says `wait_for_gpu` "is what a load,
  a resize, a **read-back** and a shutdown use". The rule is right for a
  presenting client; the named justification is not. Same sentence is in the
  commit message.
- `engine/render/d3d12/device_resources.cpp:141-150` — the `D3D12CreateDevice`
  probe is justified by "picking it would mean a throw out of `create_device`
  rather than the WARP fallback below". In `x64-debug-d3d12`, the only preset
  that builds this backend, `NDEBUG` is undefined and the fallback at `:199` is
  a plain `if`, not the `else` of `if (adapter)` — so with or without the probe
  you get a WARP device and no throw. The real benefit is unwritten: the probe
  lets the loop keep looking past a D3D12-incapable adapter, so a machine with a
  capable second GPU uses it instead of dropping to WARP.
- `engine/render/d3d12/texture_factory.cpp:93-97` — "Loading already stalls on
  every backend (`resource_factory.h`)". That header says nothing about
  stalling, and d3d11 and gl do not stall here; d3d12 is the only one of four
  that blocks on a fence in the load path. The decision is right; the citation
  is dead and the comparison is backwards.

---

## 5. Unresolved

### 5.1 Severity of §2.3 (textures released before the GPU wait) — high or medium?

One reviewer: high — an unconditional violation of the one contract this backend
exists to honour, on every exit of both shipped samples, with no synchronisation
anywhere on the path. The other: medium — `Present(1, 0)` is a vsync present
that blocks the CPU, and by the time `WM_QUIT` is dequeued and thirteen members
(including a thread pool that joins its workers) have been destroyed, the GPU
has almost always drained; the spread is usually nothing, sometimes a debug
break at exit, rarely a page fault. It cannot produce a wrong pixel or affect
any test.

**My read: high, and the likelihood argument is correct.** The exposure is
widest under exactly the configuration the project puts this backend in — the
WARP fallback, where a 1280×720 frame is milliseconds of CPU rasterisation — and
`d31a804` itself is a commit that fixed a crash reproducing "roughly one run in
six", so this project already treats intermittent teardown UB as worth a commit.
The deciding factor is that the fix is moving one line and the rule is written
down twice (`renderer.h:320-321`, `PHILOSOPHY.md:433-434`).

### 5.2 `MipLevels` narrowed to `UINT16`, `subresources` to `UINT` — `texture_factory.cpp:121` / `:156`

One reviewer refuted it: `D3D12_REQ_MIP_LEVELS` is 15, so every count in [16,
65535] already produces the *named* throw at `:143`; the unbounded mip count
comes from `dds_file.cpp:213`, which is shared engine code neither commit
touched and all three level-consuming backends inherit; `texture_data.h:33-35`
puts that validation in the reader, not the backend; and the claimed "GPU fault
or device removal" is unproven, because
`ThrowIfFailed(CreateCommittedResource(...))` at `:181` stands in the way.
Another kept it at the bottom of low.

**My read: keep it, at the bottom of low, and correct the framing.** The
defensible core is narrow but real: the truncation is precisely what defeats
this backend's own guard. Absent it, a count ≥ 16 fails
`CreateCommittedResource` and hits the named throw; because the value is
truncated, a count ≥ 65536 makes creation *succeed* and the code then calls
`GetCopyableFootprints` with `NumSubresources` far outside its annotated range.
Compute the count once, reject one that will not fit, derive both values from it
— four lines. A bound in `dds_file.cpp` is the better half of the fix and covers
all four backends. Two of the finding's supporting arguments are wrong and
should not be repeated: the 65536 case is *not* worse (it is the same struct
passed to both calls), and d3d11 does not "fail cleanly" — it throws
`com_exception` with an eight-digit HRESULT.

### 5.3 Should the mid-frame-resize case be a golden image rather than `end_not_comparable`?

One reviewer: it is representable — `golden::check_frame` takes width and height
(`golden_image.h:39-40`), `Harness::end` already passes them,
`golden_image.cpp:146-165` slugs by case name — and the newest, least-shared
code path is now outside the only cross-backend comparison in the repository,
for a reason `:301-309` states as "neither is 64x64" while `:305` says the frame
is 32×32 on *every* backend. Another: `:301-309` documents the choice at the
point it is made, so this is taste.

**My read: the drift is a defect and the design choice is yours.** `:64-65`'s
stated reason ("read out of a buffer the seam says is a different size per
backend") does not cover the second exemption, and that must be fixed either
way. Whether the 32×32 frame joins the golden set is a call about how much of
the newest path you want held to an image; there is a real argument for it, and
it is not a review finding.

---

## 6. Not verified

These ranked below the agent-budget cap and **were not checked**. Listed as open
questions, not findings — each needs the same verification the confirmed list
got before anyone acts on it.

1. `renderer.cpp:550` vs `:1163-1177` — `open_frame()` and `set_view_count()`
   each carry their own copy of the "open the declared views" loop, over the
   same `view_count`, on the two paths `d31a804` exists to keep in step. A
   `Impl::open_views()` would fold them (T3).
2. `renderer.cpp:483` and `:444` — `texture_slot_gpu` and `sampler` re-query
   `GetGPUDescriptorHandleForHeapStart()` on every `flush()`, i.e. per draw
   call, while the two increment sizes beside them are cached members
   (`:801-806`). `backend.h:82-84` sets the bar this misses.
   `DeviceResources::back_buffer_view` does the same twice a frame.
3. `renderer.cpp:1209` — `submit()` constructs and reserves a fresh
   `std::vector<ID3D12CommandList*>` every frame; the only frame-path allocation
   any of the four backends makes. The file already has the pattern
   (`View::batch` is `clear()`ed to keep its capacity).
4. `texture_factory.cpp:166-184` — nineteen lines that reproduce field-for-field
   what `create_buffer` in `renderer.cpp:98-109` produces, duplicated only
   because that helper is in an anonymous namespace. Both files are inside the
   folder wall, so sharing it breaks no rule.
5. `d3d11/device_resources.cpp:535-538` — "Still called on three paths":
   `d31a804`'s duplicated size guard in `d3d11/renderer.cpp` made the "a resize
   that turns out not to be one" branch at `:341-346` unreachable, which is the
   branch that refreshed a stale DXGI factory when the window moves to another
   monitor. Needs checking against the actual `WindowSizeChanged` caller set.
6. `sprite.hlsl:30-35` — "at the lowest profile each backend accepts ... 5_1 for
   D3D12" (5_0 is lower and D3D12 accepts it), and "the two produce the same
   arithmetic - which `tests/render/golden/` is what actually checks" (the two
   profiles are never compared to each other, and `ALLOWED_CHANNEL_DRIFT = 8`
   per channel). `engine/CMakeLists.txt:138-140` repeats it.
7. `sprite_vertex.h:10-13` and `:23-24`, `texture_format.h:13-16`,
   `sprite_geometry.h:25-26`, `engine/CMakeLists.txt:160-163` — four
   shared-header claims that still count three backends after the fourth landed
   reading the same code. `texture_format.h` matters most: its `default:` in
   `d3d12/texture_factory.cpp` routes an unknown enumerator to `r8g8b8a8_unorm`.
8. `ARCHITECTURE.md:124` ("four states" — d3d11 creates five, and
   `CLAUDE.md:150` was amended by the same commit to say so),
   `ARCHITECTURE.md:110` ("the configurations: debug, release" — five presets),
   `PHILOSOPHY.md:332` (same "four state objects"), `CLAUDE.md:74` ("all three
   configurations").
9. The full texture-load and read-back paths beyond the lines named above,
   including whether `total_bytes` / footprint consistency holds for a multi-mip
   `.dds` — see §8.

---

## 7. Refuted

Kept so nobody spends budget re-deriving them.

- **"`frame_index_` never leaves 0, so the frames-in-flight ring is never
  executed in any ctest configuration."** Mechanically true — `end_frame` is
  called by no test on any backend, and `frame_index_` only advances in
  `move_to_next_frame` ← `present()` ← `end_frame()`. But it is a coverage gap,
  not a defect: the ring is correct (frame C's wait on `frame_fences_[0]` covers
  frame A's final signal), the fact is documented at
  `device_resources.cpp:322-330` at the point it happens, it is disclosed in the
  commit message, and `TEST-GAP.md` opens "Nothing here is a defect" for exactly
  this shape. Pre-existing since `39c187b`.
- **"`abandon_recording` discards a barrier without rolling the tracked
  back-buffer state back."** The tracker commits at record time, but no
  reachable path desyncs: `open_frame` executes the barrier via
  `execute_frame_list()` *before returning*, so `frame_list_open` is false for
  the whole draw walk; `read_back_buffer` is net-zero across a discard; and the
  only throw between barrier and execute is `ThrowIfFailed(Close())`, which
  needs a pre-existing defect or device removal — and on removal the next
  `begin_frame` throws first at the allocator `Reset`.
- **"`backend.h:74` claims a slot survives a heap recreation, and nothing
  enforces the DeviceNotify precondition."** `registry.h:39-44` defines
  "survives" as emptied-and-refilled-in-order, which is exactly what happens;
  `Application` installs itself as notify in its own constructor; and d3d11 has
  the identical no-notify hazard and ships green.
- **"GL drops the frame's drawing where the two Direct3D backends keep it."** No
  message pump exists between `begin_frame` and `submit` — the only
  `PeekMessage` is `window.cpp:239`, outside `tick()`, and `Present` sits inside
  `end_frame`, after `submit`. The gl restart always lands between frames, where
  it is idempotent. What survives of that observation is §3.6, which is a
  different claim.
- **"`UINT_MAX` at `renderer.cpp:745` without `<climits>`."** `<Windows.h>` does
  not supply it — `winnt.h:1467` says so in a comment — the MSVC STL does, via
  five headers already in the file's own block, and `gl/renderer.cpp:865` uses
  `std::memcpy` with no `<cstring>` through the same `<xutility>`.
  `CONVENTIONS.md:135` is an ordering rule, which this file obeys; there is no
  include-what-you-use rule in this project.

---

## 8. Coverage

**No build was run, no test was executed, no GPU was touched.** Every claim
above comes from reading the tree at `d31a804`. Nothing that depends on a
driver's actual behaviour — implicit promotion tolerance, what `Signal` returns
on a removed device, what `GetCopyableFootprints` writes when asked for
out-of-range subresources — was observed; those are argued from the spec and
from Microsoft's own reference implementation, and are flagged where it matters.

**What was examined.** Twenty-seven lenses, all but one pointed at
`engine/render/d3d12/`. Findings land on roughly 30 of the backend's ~96
function bodies: `View::begin` and `flush`, `abandon_recording`, `open_frame`,
`frame_open`, `Renderer::window_size_changed`, `submit`, the two destructors,
the fence quartet, `allocate_texture_slot`, `add_texture_asset`,
`RenderResources::release_device_resources`, and the PSO block of
`create_device_dependent_resources`. Everything else in the list is claim drift
in headers and documents. Independently spot-checked and **clean**: the five
state objects are field-for-field equal to d3d11's including `MaxLOD = 0.0f` (so
the mip term settled in `68fe4dc` holds), `MultisampleEnable`, `SrcBlend = ONE`,
`CullMode = NONE`; `MAX_PAGE_SPRITES * 4 = 8192` is safe for the `R16_UINT`
index buffer; `create_device_resources` resets both `fence_value_` and
`frame_fences_[]`, so a device loss cannot strand `wait_for_frame` on an
unreachable value; `frame_index_` is re-read from `GetCurrentBackBufferIndex()`
after `ResizeBuffers` rather than incremented, so the allocator ring cannot
desynchronise from the swap chain.

**Where it is thin.** Five contiguous regions carry no finding anywhere inside
them:

1. `device_resources.cpp:19-281` (263 lines) — `MIN_FEATURE_LEVEL`, the
   constructor, `create_factory`, `hardware_adapter`, `create_device_resources`.
   The largest un-examined block in the backend, almost entirely error and
   fallback path, and the only substantial code here with no house-written
   counterpart next door (d3d11's is Microsoft's vendored file).
2. The `frame_list` / `frame_allocators` construct and its five callers, plus
   `read_back_buffer` (`renderer.cpp:559-588`, `:1245-1335`,
   `texture_factory.cpp:214-248`). This construct exists in no other backend.
   Each caller was read in isolation; nobody enumerated the entry/exit state of
   `frame_list_open`, `frame_allocators[frame]` and `back_buffer_states_` across
   all five, and nobody traced the asset reload that runs re-entrantly inside
   `end_frame() → present() → handle_device_lost() → on_device_restored()`.
3. `renderer.cpp:790-958` — descriptor heaps, the two samplers, the per-view
   allocator/list loop, the index-buffer upload. Partly pinned by goldens.
4. `renderer.cpp:324-425` — the per-draw record path. Thinner than it looks; the
   one hook I expected to pay (`View::reset()` clearing camera and filter across
   the restart) is the same on all three real backends. The scissor rectangle at
   a negative-origin viewport is the only live question left.
5. `engine/app/` — the only caller of the seam that changed. One finding crossed
   into it (§2.3), and §3.7/§3.6 came out of finally reading it. **The premise
   of the entire `d31a804` fix went unexamined until the end**:
   `renderer.h:293-299` names two delivery mechanisms for a mid-frame resize,
   and neither exists here. `window.cpp:344-350` renders from `WM_PAINT` only
   when `in_sizemove_`, and `:394` forwards `WM_SIZE` only when `!in_sizemove_`
   — complementary conditions, as `renderer.h:392-394` says itself. And the only
   `Present` sits inside `end_frame`, after `submit`. The reachable in-stack
   path is a synchronous one the paragraph does not mention: `set_resolution →
   Window::resize_client → SetWindowPos`, which sends `WM_SIZE` on the same
   thread. The term itself is right and should stay; the reasoning under it
   needs rewriting, and it is why §2.1 is a seam-reachability defect rather than
   an observed crash.

**Untested code that ships green.** Nothing in `tests/` draws more than 2048
sprites into one view, so `View::flush()`'s take-another-page branch
(`renderer.cpp:180-183`) — one of the six decisions the commit message names,
and the one place this backend deliberately differs from d3d11's `MAP_DISCARD`
wrap — executes in no ctest configuration and appears in no golden image.
`texture_data.h:53` records that every `.dds` in this repository and its client
is single-level, so the per-mip footprint loop at `texture_factory.cpp:191-212`
never runs either. No test calls `end_frame`, so `present()`,
`move_to_next_frame()` and the second allocator/page/fence set are exercised
only by a real window — which both commits record under **Verified**, and which
is the right way to have checked them, but it is not ctest.

**One limit on the golden set worth naming**, since several findings lean on it:
the 47 images hold each backend to an image, not to each other, and
`ALLOWED_CHANNEL_DRIFT = 8` (`golden_image.cpp:410`) is per channel — so two
backends can differ from each other by up to 16 in a channel and both pass.
`renderer.h:552-555` states this correctly; `sprite.hlsl:34-35` does not.