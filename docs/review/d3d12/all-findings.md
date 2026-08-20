# Every finding, as its agent wrote it

> Part of the [Direct3D 12 backend review](README.md). Read-only, 2026-08-20,
> against the tree at `d31a804`. Not updated as findings are fixed.

The [README](README.md) is the review: merged, ranked and argued. This file is
the material underneath it — each finding in the words of the agent that raised
it, with the evidence it quoted and the fix it proposed, and for the refuted
ones the reasoning that killed them. Where the README merges two findings into
one section, both appear here separately.

Severities are the verified ones where a refuter corrected them.

---

## Confirmed — survived two adversarial refuters

15 findings. Both refuters were told to default to *refuted*; where they
disagreed an adjudicator read the code and decided.

### C1. window_size_changed gates the frame restart on frame_open(), which is false for the whole interval between begin_frame() and the first set_view_count() — the new back buffer is then rendered into while tracked and actually in PRESENT, and is never cleared

**`engine/render/d3d12/renderer.cpp:1076`** · high ·
`resource-state-correctness`

```
renderer.cpp:1076-1088
		const bool restart = impl.frame_open();
		impl.abandon_recording();

		const bool rebuilt =
			impl.device_resources.window_size_changed(width, height);

		if (rebuilt && restart)
		{
			impl.open_frame();
		}

and the predicate it turns on, renderer.cpp:488-503:
	bool Renderer::Impl::frame_open() const
	{
		if (this->frame_list_open) { return true; }
		for (const std::unique_ptr<DrawList::View>& view : this->views)
		{ if (view->recording) { return true; } }
		return false;
	}
```

**Why it is wrong.** renderer.h:281-315, added by d31a804, states as a term of
the seam that this call "MAY ARRIVE IN THE MIDDLE OF A FRAME, AND A FRAME IN
PROGRESS IS RESTARTED RATHER THAN REFUSED", that "The views the frame declared
are reopened against the new buffer, which is cleared as begin_frame would clear
it", and that a rule forbidding the call at some point in the frame is exactly
what T6 says to make impossible rather than document. frame_open() implements a
strictly narrower predicate — "is a command list open right now" — which is not
the interval begin_frame..end_frame. backend.h:308-311 describes it as "Whether
anything of a frame is open: a view recording, or the frame list holding a
barrier. What Renderer::window_size_changed asks before it decides whether there
is a frame to restart", but the frame_list_open half is dead at every call
boundary: open_frame (:537-546), end_frame (:1134-1135), read_back_buffer
(:1287-1296) and add_texture_asset (texture_factory.cpp:218-245) all execute the
list before returning, so no caller can observe it true. backend.h:325-328's "a
caller that wants to add to what begin_frame recorded can" describes the same
unreachable state.

It also reopens the three-way disagreement d31a804 exists to close:
gl/renderer.cpp:703-716 clears and resets every view unconditionally on any real
size change, and its comment justifies that by saying "the other two hand back a
cleared buffer" — which on this path they do not. d3d11/renderer.cpp:720 has the
identical blind spot (`restart = restart || view.bound`, and `bound` is set only
in set_view_count), though there the consequence is only the missing clear.

**Failure scenario.** begin_frame() leaves frame_open() false. It calls
abandon_recording() (renderer.cpp:1121, clearing every view->recording), then
sets impl.view_count = 0 (:1127), then open_frame() (:1129) — whose view loop
`for (int i = 0; i < this->view_count; i++)` (:552) opens nothing, and whose
execute_frame_list() (:546) sets frame_list_open = false (:581). So both arms of
frame_open() are false the instant begin_frame() returns, even though the frame
has already spent its PRESENT->RENDER_TARGET barrier and its clear on the old
buffer.

Call sequence, legal under the seam: begin_frame(); window_size_changed(32,32);
set_view_count(1); view(0).draw_sprite(...); submit(); end_frame().

restart is false, so open_frame() is skipped. Meanwhile
DeviceResources::create_window_size_dependent_resources has released every back
buffer and set back_buffer_states_[i] = D3D12_RESOURCE_STATE_PRESENT for both
indices (device_resources.cpp:294-298) and re-read frame_index_ (:381).
transition_back_buffer(RENDER_TARGET) has exactly one call site —
renderer.cpp:537, inside open_frame() — so nothing transitions the new buffer.
set_view_count then calls View::begin, which does OMSetRenderTargets(1,
&render_target, FALSE, nullptr) (:287) with no barrier, and submit() executes
those lists. A swap-chain back buffer carries ALLOW_RENDER_TARGET and is not
implicitly promoted out of COMMON to RENDER_TARGET, so this is a genuine
INVALID_SUBRESOURCE_STATE — and device_resources.cpp:232-236 installs
SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE), so a debug run under
x64-debug-d3d12 breaks into the debugger. Without the layer the render-target
write happens in the wrong state, and the clear never happened, so the frame
composites over whatever ResizeBuffers left. end_frame()'s
transition_back_buffer(PRESENT) then records nothing, because the tracked state
already reads PRESENT (:593-596) — the frame is presented having been written
with no barrier at all.

The same hole re-opens between submit() and end_frame(), where submit() has set
every view->recording = false (:1221): a resize there presents an uncleared,
never-transitioned buffer.

The new test cannot reach it: Harness::begin() calls begin_frame() and
set_view_count() back to back (pixel_tests.cpp:260-265), so a view is always
recording when the resize lands.

**Fix.** Track the frame explicitly instead of inferring it from open lists: a
`bool frame_begun` on Renderer::Impl, set in begin_frame() and cleared in
end_frame(), used as `restart`. open_frame() is already correct for view_count
== 0 — it resets the frame allocator, transitions, clears and opens no views —
so calling it whenever `rebuilt` is true and a frame has begun is the whole
repair. Apply the same predicate to d3d11's `restart` so all three rasterising
backends hand back a cleared buffer, and add a pixel_tests case that resizes
between begin_frame() and set_view_count().

**Corrected in verification.** Severity: "critical" overstates it; "high" is
right. The mechanism is confirmed, but (a) the process does not die the way the
pre-d31a804 bug did — in release the worst case is one frame composited over
undefined ResizeBuffers content plus a spec-illegal RT write that most desktop
drivers tolerate; (b) the debug-layer break needs the resize to land in a window
that neither RenderPixelTests nor Application::render() drives deliberately; (c)
the known-green d3d11 backend ships the identical `restart` predicate, which is
evidence this is a seam-wide correctness gap rather than a d3d12-only
catastrophe.

Three things the finding gets wrong or overstates, none fatal to the core claim:

1. The submit()..end_frame() window is NOT the same defect. The reporter says "a
   resize there presents an uncleared, never-transitioned buffer" and files it
   under the same state error. `restart` is indeed false there (submit sets
   every `view.recording = false` at :1221, and end_frame's `execute_frame_list`
   has not run yet but `frame_list_open` is already false), but nothing is drawn
   afterwards, and the buffer really is in COMMON which is exactly what Present
   requires. So that window yields one garbage/uncleared presented frame — a
   cosmetic defect — not an INVALID_SUBRESOURCE_STATE. Only the
   begin_frame..set_view_count window produces the state error.

2. The claim that backend.h:325-328 ("a caller that wants to add to what
   begin_frame recorded can") "describes the same unreachable state" is wrong.
   `open_frame_list()` returning an already-open list is exercised twice in the
   tree: inside `open_frame()` itself, where `transition_back_buffer` (:537)
   opens the list and records the barrier and the very next statement's
   `open_frame_list()->ClearRenderTargetView` (:544) adds to that same open
   list; and in `read_back_buffer`, where `transition_back_buffer(COPY_SOURCE)`
   (:1275) opens it, `CopyTextureRegion` (:1287) adds to it, and
   `transition_back_buffer(previous)` (:1295) adds again. That comment is
   honoured. What IS dead is only the `frame_list_open` clause of `frame_open()`
   at a public-API boundary — every entry point (`open_frame` :546, `end_frame`
   :1135, `read_back_buffer` :1296, `add_texture_asset` texture_factory.cpp:245,
   `create_device_dependent_resources` :953) executes the list before returning
   — which is enough to support the finding without the overreach.

3. Minor citation drift: `SetBreakOnSeverity(ERROR)` is at
   device_resources.cpp:233-234 inside the block at :226-238, not ":232-236";
   `back_buffer_states_[i] = PRESENT` is the single line :297 inside the loop
   :294-298. The substance is right.

One caveat on the proposed fix worth flagging: gating `open_frame()` on a new
`frame_begun` flag is correct, but note `open_frame()` also does
`frame_allocators[frame]->Reset()` (:534) — safe here only because
`create_window_size_dependent_resources` has already called `wait_for_gpu()`
(:288) before the flag is consulted. Anyone implementing it should keep the
`open_frame()` call after `device_resources.window_size_changed`, as the current
code already does. Severity should be high, not critical. Reachability is
narrower than the finding implies: in the shipped shell the two gaps are
begin_frame -> scene.cpp:166's set_view_count (application.cpp:292-296;
begin_marker is a no-op on this backend, renderer.cpp:1336-1339) and submit ->
end_frame, and nothing in either interval pumps messages, so the resize has to
be delivered by client code that does. No test and no CI path reaches it. The
damage also self-heals: the frame after the bad one calls open_frame, and its
COMMON->RENDER_TARGET barrier is valid because the runtime never left COMMON.
Cost is one garbage-composited, never-cleared frame plus a debug-layer ERROR
break, not a persistent corrupt state.

Scope correction: the missing clear is not d3d12-only. d3d11/renderer.cpp:720
has the identical predicate width and the same skipped clear, and both files
were rewritten by d31a804, so the finding should be filed against both Direct3D
backends with only the missing RENDER_TARGET barrier and the tracked-state
desync as d3d12-specific. The fix section already says this; the title and
severity do not.

Two citation slips, neither load-bearing: submit() does not assign
`view->recording = false` at renderer.cpp:1221 — :1221 is `view.close();`, and
close() (:298-309) clears the flag. And the back-buffer state reset is
device_resources.cpp:297 inside the FRAME_COUNT loop at :294-298, not the whole
range; SetBreakOnSeverity is :232-235, not :232-236.

One wording correction on the failure text: "the render-target write happens in
the wrong state" is undefined behaviour, not a guaranteed visible corruption —
on most desktop hardware COMMON and RENDER_TARGET share a layout and the write
lands. The defensible always-true consequences are (a) the debug-layer ERROR and
break under x64-debug-d3d12, and (b) the clear that never happened, so the frame
composites over whatever ResizeBuffers left.

---

### C2. Every texture's ID3D12Resource is released before the GPU is idled, because Application destroys RenderResources ahead of Renderer

**`engine/render/d3d12/render_resources.cpp:60`** · high ·
`gpu-resource-lifetime`

```
engine/render/d3d12/render_resources.cpp:60
	RenderResources::~RenderResources() = default;

which destroys Registry<D3d12Texture> and with it every ComPtr<ID3D12Resource> in D3d12Texture (backend.h:99), with no wait of any kind. The only wait is one object away, in Renderer::Impl::~Impl (renderer.cpp:431-442):
	// BEFORE ANY OF THE MEMBERS BELOW device_resources GO ... the command
	// lists, allocators and vertex pages declared after it would be released
	// while the GPU was still reading them.
	this->device_resources.wait_for_gpu();

engine/app/application.h, under "DECLARATION ORDER IS LOAD-BEARING BELOW THIS LINE" (:229):
	:240  std::unique_ptr<Renderer> renderer_ = nullptr;
	:244  std::unique_ptr<RenderResources> render_resources_ = nullptr;
```

**Why it is wrong.** device_resources.cpp:49-53 states the rule the backend is
built on — "dropping a command list the GPU is still reading is exactly the bug
the fence exists to prevent, and a destructor is no exception" — and
renderer.cpp:433-441 says the ~Impl wait covers "the command lists, allocators
and vertex pages". It covers only what Renderer::Impl owns. The textures every
draw call samples are owned by a different object that the shell destroys first,
so the guarantee has a hole exactly one object wide, and it is the object
holding the resources the GPU is actually reading. D3D11 and GL are immune
(deferred destruction and driver-managed names), so this is a hazard the new
backend introduced and did not close, and the ordering rule appears nowhere in
renderer.h or render_resources.h.

**Failure scenario.** Members destruct in reverse declaration order, so
render_resources_ (:244) dies BEFORE renderer_ (:240), and ~Application
(application.cpp:62-86) adds no drain — it calls clear(),
audio_engine_->Suspend() and CoUninitialize(). So on any normal exit of
MinimalSample or LineSweeperSample under x64-debug-d3d12: the last
Application::render() ran submit() (ExecuteCommandLists over view lists that
name those textures through SetGraphicsRootDescriptorTable,
renderer.cpp:235-236) and end_frame() -> Present(1,0), which queues the flip and
returns without draining the queue. WM_QUIT returns run(), ~Application runs,
and every texture resource drops to refcount 0 and its committed heap is freed
while the GPU may still be executing that frame. Only afterwards does ~Renderer
-> Renderer::Impl::~Impl call wait_for_gpu().

D3D12 does not keep resources alive for in-flight command lists the way the
D3D11 runtime does and descriptors are not references, so the result is a GPU
page fault / DXGI_ERROR_DEVICE_REMOVED at process exit, or under the debug layer
a resource-destroyed-while-referenced report at CORRUPTION severity, which
device_resources.cpp:232-233 breaks on. The window is about one frame wide,
which is why a slow exit looks clean.

**Fix.** Declare render_resources_ before renderer_ in engine/app/application.h
so the Renderer (and its ~Impl wait_for_gpu) is destroyed first — an
ID3D12Resource holds its own reference on the device, so releasing textures
after the renderer is fine — or reset renderer_ explicitly in ~Application ahead
of every device-backed table. Either way the ordering rule belongs beside
release_device_resources on the seam, since only this backend can be hurt by
getting it wrong.

**Corrected in verification.** Four things the finding gets wrong or
under-states, none of which touch the core claim.

1. The debug-layer detail is imprecise. The D3D12 report for this is EXECUTION
   ERROR #921, OBJECT_DELETED_WHILE_STILL_IN_USE, at severity ERROR — not
   CORRUPTION. The conclusion survives because device_resources.cpp:231-234 sets
   the break on BOTH: `SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION,
   TRUE)` and `SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE)`. The
   cited line range 232-233 straddles the two calls; 231-234 is the block.

2. The failure is timing-dependent, not deterministic, and the finding's "about
   one frame wide" undersells how much CPU work sits in the window. Between
   `Present(1,0)` returning and `~RenderResources` running, the process does:
   return from render/tick, drain to WM_QUIT, `clear()` on the whole state
   stack, and destroy six members including `thread_pool_` (which joins its
   workers) before it reaches `render_resources_`. On a hardware adapter drawing
   a two-sprite sample frame the GPU has almost always finished by then. The
   window is widest exactly where the project puts this backend in debug — the
   WARP fallback in `create_device_resources`, where a 1280x720 frame is
   milliseconds of CPU rasterisation. So expect an intermittent exit-time break
   under WARP rather than a reliable crash on a dev box with a GPU. I kept the
   severity at high anyway: it is an unconditional violation of the one contract
   this backend exists to honour, on every exit of both shipped samples, with no
   synchronisation of any kind on the path.

3. The finding misses that the fix it proposes is incomplete. Reordering
   `application.h` closes the shell, but tests/render/pixel_tests.cpp:461-462
   declares `Renderer renderer_; RenderResources resources_;` in the same wrong
   order. That harness is currently saved only by accident — `read_back_buffer`
   (renderer.cpp:1299) drains, and every `end()` calls it — so the hazard is
   latent there and would go live the first time a test submits a frame it does
   not read back. The reporter's own preferred remedy (state the rule on the
   seam beside `release_device_resources`) is the right one precisely because
   there are two holders, not one.

4. The finding treats this purely as a lifetime bug and misses that it is also
   comment drift by this project's own standard
   (docs/review/backend-equivalence/DRIFT.md). device_resources.cpp:47-49 claims
   to be "THE ONE DESTRUCTOR IN engine/render/ THAT HAS TO DO ANYTHING"; with
   this backend, `d3d12/render_resources.cpp:60` is a second destructor in
   engine/render/ that releases GPU memory, and it does nothing.
   renderer.cpp:433-441 likewise claims the `~Impl` wait is "why this destructor
   exists at all" while covering only the members Renderer::Impl owns. And
   docs/design/PHILOSOPHY.md:433-434 — "Construction and destruction order are
   designed. If an ordering is load-bearing, it is either designed away or
   stated where it lives" — is the project rule the change breaks, and is the
   vocabulary the fix should be argued in. Four corrections, one of which
   strengthens the finding:

1. "the ordering rule appears nowhere in renderer.h or render_resources.h" is
   WRONG, in the finding's favour. renderer.h:318 says of the notify "Borrowed;
   the shell owns it and outlives the renderer", and :320-321 says of the
   resources table "Borrowed, like the notify above." The seam already states
   that RenderResources outlives the Renderer; application.h:240/244 already
   breaks it. The fix is therefore restoring a stated term, not inventing one.

2. Severity attribution. The debug-layer message is EXECUTION ERROR #921
   OBJECT_DELETED_WHILE_STILL_IN_USE, i.e. it trips the ERROR break at
   device_resources.cpp:234-235, not only the CORRUPTION break at :232-233 the
   finding cites. Both are set, so a break fires either way — but "CORRUPTION
   severity" alone is imprecise.

3. Location. render_resources.cpp:60 is where the release happens, but that line
   is byte-identical in d3d11 (:68), gl (:70) and null, and is not itself wrong.
   The defect is the missing ordering constraint: application.h:240 vs :244,
   plus the absence of the requirement at renderer.h:320-321 /
   render_resources.h:110 where release_device_resources is specified. Anchoring
   the finding at d3d12/render_resources.cpp:60 invites the reply "that file
   does nothing the other three don't."

4. Likelihood, which the finding overstates by omission. present() is
   `Present(1, 0)` — a vsync present that blocks the CPU — so by the time
   WM_QUIT is dequeued the GPU has usually drained; the exposure is the last
   frame's GPU render time for a trivial scene. Consequence is a spread: usually
   nothing, sometimes a debug break at exit, rarely a page fault /
   DEVICE_REMOVED. It cannot produce a wrong pixel, corrupt a live frame, or
   affect any test. That, plus teardown-only and one backend of four, is why I
   put it at medium rather than the claimed high — while noting the project's
   own precedent (d31a804 fixed a crash that reproduced "roughly one run in
   six") treats intermittent teardown UB as worth a commit.

---

### C3. Re-loading a texture under a name already in the table permanently burns a descriptor slot, so a supported reload eventually throws "they are all taken" with a few dozen textures live

**`engine/render/d3d12/texture_factory.cpp:250`** · medium · `resource-leak`

```
texture_factory.cpp:248-263 — unconditional on every load, name already in the table or not:
		// The descriptor, which is what a draw call actually names. Claimed
		// last, so a texture the device refused never takes a slot.
		const int slot = impl.allocate_texture_slot(name);
		...
		resources.impl()->add_texture(name,
			std::make_unique<D3d12Texture>(resource, slot, texture.width,
				texture.height));

renderer.cpp:457-468 — a monotonic bump counter with no free list:
		if (this->next_texture_slot >= TEXTURE_CAPACITY) { throw ... }
		return this->next_texture_slot++;

engine/core/registry.h:60-76 — Registry::add replaces the entry for an existing name and destroys the old D3d12Texture, slot number included.
```

**Why it is wrong.** backend.h:252 states the invariant this allocator is
supposed to implement — "HOW MANY TEXTURES MAY BE LIVE AT ONCE" — and the
counter measures loads since the last heap creation, not live textures, so the
T6 refusal on the next line says something that did not happen.
render_resources.cpp:46-53 reasons about only one of the two ways a slot stops
being needed (a device loss, where the heap is remade) and describes a
reclamation that does not happen for the other (a name re-added, where it is
not). resource_factory.h:79-87 states exactly one ordering rule for
add_texture_asset and no rule about loading a name once, so a call sequence that
is free on three backends hits a hard bounded ceiling on the fourth — the
cross-backend divergence renderer.h exists to prevent.

**Failure scenario.** next_texture_slot is reset only in
create_device_dependent_resources (renderer.cpp:812) and on_device_lost
(renderer.cpp:992) — i.e. only when the heap is remade by a device loss. Every
other re-add of a name consumes a fresh slot and orphans the old one forever,
because D3d12Texture holds slot_ as a plain int with no destructor
(backend.h:85-104).

Reachable paths, all documented as supported: ResourceLoader::load_manifest has
no already-loaded guard (resource_loader.cpp:76-94) and resource_loader.h:74-77
explicitly contemplates more than one manifest ("A second manifest replaces the
first as the thing a restore replays, so a game with more than one loads them as
one"); ResourceLoader::reload_device_resources() is public and re-walks the same
manifest (resource_loader.cpp:96-108). A client that loads a manifest per level,
reloads one on a restart, or ships two manifests naming a shared texture, burns
one slot per texture per walk. With the ~45 textures CLAUDE.md says this engine
loads, the sixth walk throws "Texture 'x' does not fit: this backend's
descriptor heap holds 256 textures and they are all taken" while only 45 are
live — a message naming a limit that was never reached.

The same client runs indefinitely on the other three backends: d3d11 stores the
SRV in a ComPtr that is released on replacement, GlTexture::~GlTexture deletes
the GL name, null holds nothing. d3d12 is the only backend where a replacement
reclaims the resource but not the bounded heap slot.

**Fix.** Reuse the slot when the name is already in the table — look the
existing D3d12Texture up before allocating and write the new SRV into the slot
it already holds — or keep a free list that add_texture pushes the replaced
texture's slot onto. Then amend backend.h:252 and render_resources.cpp:46-53 so
both describe what the counter actually bounds.

**Corrected in verification.** Four things the finding gets wrong or overstates,
none of which kill it:

1. THE 45 FIGURE IS MIS-ATTRIBUTED. "the ~45 textures CLAUDE.md says this engine
   loads" — CLAUDE.md says nothing of the sort. The number comes from this
   backend's own comment at `engine/render/d3d12/texture_factory.cpp:153-155`
   ("43 of the 45 images loaded") and from
   `engine/render/texture_format.h:16-24` (41 bc3 + 2 b8g8r8a8 = 43);
   `d3d12/backend.h:256-257` says only "both clients together load a few dozen
   textures". The arithmetic ("the sixth walk throws") is right; the citation is
   not.

2. `reload_device_resources()` IS THE WEAKEST OF THE THREE PATHS, not a peer of
   the others. `resource_loader.h:85-86` documents it as "Rebuilds what lives on
   the GPU **after a device loss**", and its only in-tree caller is
   `application.cpp:439` inside `Application::on_device_restored`, which is
   reached only from `Renderer::Impl::on_device_restored`
   (renderer.cpp:1000-1002) → `create_device_dependent_resources` →
   `next_texture_slot = 0` (renderer.cpp:812). So the device-loss reload is safe
   by construction; that path only burns slots if a client calls the function
   standalone, outside its documented purpose. The genuinely unguarded paths are
   a second `load_manifest` repeating a name and a direct
   `load_texture_asset`/`add_texture_asset` for a name already in the table.

3. renderer.cpp:992 is not an independent second reset — `on_device_lost` and
   `create_device_dependent_resources` are two halves of the same heap-remake
   cycle, so there is effectively one reset condition, not two.

4. NO IN-TREE PATH TRIGGERS IT. Every `Harness` in
   `tests/render/pixel_tests.cpp` is a fresh local per test case (lines 471,
   483, 502, ... 1420), each creating its own device and its own counter, and
   `add_texture_asset(..., "two_level", ...)` at :631 uses a name the harness
   did not load. Both sample manifests are duplicate-free. So this is latent,
   not observed, and no test pins it.

Worth adding for the fix's sake: the stale descriptor left behind in the
orphaned slot describes a released `ID3D12Resource`, but no live `D3d12Texture`
holds that index, so nothing samples it. This is not a wrong-pixel or
use-after-free risk — the fix only needs to reclaim the index, not scrub the
descriptor.

SEVERITY: lowered high → medium. Nothing in this repository reaches it; it needs
an out-of-tree client to redundantly re-load the same names roughly five times
over; and the consequence is a clean named throw at load time rather than a
crash, corruption, wrong pixel or race. It stays above low because the bound is
small (256 against ~45 live), because the message actively misdiagnoses the
condition ("they are all taken" when 45 are), and because it is a genuine
cross-backend divergence on a seam operation the other three survive
indefinitely — which is exactly what `renderer.h` exists to prevent. Four
corrections, none fatal; two of them strengthen the finding.

1. MIS-CITATION, minor. "the ~45 textures CLAUDE.md says this engine loads" —
   CLAUDE.md says nothing about a texture count. The count is
   `engine/render/texture_format.h:16-24` (41 bc3 + 2 b8g8r8a8 = 43,
   corroborated in `docs/review/backend-equivalence/DRIFT.md`), and the "few
   dozen" phrasing is `engine/render/d3d12/backend.h:257-258`, i.e. the very
   comment the finding is attacking. The arithmetic is unaffected — 256/43 still
   puts the throw on the sixth walk.

2. THE WEAKEST OF THE THREE CITED PATHS IS LED WITH. `reload_device_resources()`
   is public (`resource_loader.h:96`) and reachable via
   `Application::resource_loader()` (`application.h:206`), but its only
   documented purpose is the device-loss replay, and on that path the allocator
   is reset first. Calling it outside a device loss is arguably misuse, and a
   skeptic will attack there. The finding should lead with the manifest paths
   instead.

3. A STRONGER, SINGLE-MANIFEST TRIGGER THE FINDING MISSED.
   `ResourceLoader::load_sprite_sheet`
   (`engine/assets/resource_loader.cpp:128-138`) loads the sheet's texture under
   the sheet's own name — "The sheet's texture is loaded under the sheet's own
   name: they are one asset with two files, and the manifest names it once."
   Nothing enforces "names it once": `load_manifest`
   (`resource_loader.cpp:76-94`) walks entries blindly with no duplicate check.
   A single manifest listing `{kind: "texture", name: "tiles"}` and `{kind:
   "sprite_sheet", name: "tiles"}` double-loads inside one walk, at startup, on
   the first run. That is a content-authoring mistake that costs a wasted slot
   on d3d12 and is completely free on the other three — a much tighter
   reproduction than two manifests.

4. THE PROPOSED FIX IS SOUND AND SHOULD SAY WHY, because the obvious objection
   is the D3D12 rule that a shader-visible descriptor must not be overwritten
   while an in-flight command list references it. It cannot be violated here:
   `texture_factory.cpp:245-246` already does `impl.execute_frame_list();
   impl.device_resources.wait_for_gpu();` immediately before the allocation, and
   `DeviceResources::wait_for_gpu` (`device_resources.cpp:487-497`) is a
   signal-then-wait that drains the single command queue. The GPU is idle at
   line 250, so writing a new SRV into an already-held slot is legal. Note this
   pre-empts the objection.

Also worth noting for whoever fixes it: the amendment must cover three sites,
not two — `backend.h:252` ("MAY BE LIVE AT ONCE"),
`d3d12/render_resources.cpp:46-53` (the reset that only covers one of the two
paths), and the throw text at `renderer.cpp:461-465` ("they are all taken"),
which is the one a user actually reads.

---

### C4. The null backend never got the mid-frame-resize term the same commit wrote into the seam, so a resize keeps every recorded sprite there and drops it on the other three

**`engine/render/null/renderer.cpp:170`** · medium · `seam-conformance`

```
engine/render/null/renderer.cpp:170-179 — the whole function, untouched by either commit:
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

Compare null/renderer.cpp:192-205, where begin_frame is the only thing that clears impl_->recorded, calls view->reset() and re-stamps view->viewport.
```

**Why it is wrong.** renderer.h:301-315 states the restart as an unconditional
term of the seam, not a per-backend habit: "Everything recorded into every view
this frame is dropped", "The views the frame declared are reopened against the
new buffer". The same commit spends a paragraph in gl/renderer.cpp:686-698
arguing that a backend which does not NEED the restart must do it anyway,
"because the difference would be observable ... what a client can rely on is the
same sentence everywhere" — an argument that applies verbatim to null, whose
recording IS the observable output. d31a804 touched d3d11, d3d12, gl and
renderer.h and never touched null/renderer.cpp, and CLAUDE.md's rule is that a
change to engine/render/ is checked against all four. It is untestable where it
stands: the only case for the new term lives in pixel_tests.cpp, which
tests/render/CMakeLists.txt:62-63 does not build under the null preset, and
renderer_seam_tests.cpp never mentions window_size_changed.

**Failure scenario.** Under x64-debug-null, in null_tests.cpp's own vocabulary:
begin_frame(); set_view_count(1); DrawList list = view(0); list.draw_sprite(A,
..., RectangleF(0,0,32,32), ...); renderer.window_size_changed(32, 32) ->
returns true; list.draw_sprite(B, ..., RectangleF(0,0,8,8), ...); submit();
recorded_sprites().

On null the result holds BOTH A and B, and both carry the pre-resize Viewport,
because nothing on this path clears View::sprites, calls DrawList::View::reset()
(null/renderer.cpp:42-48) or re-seeds view->viewport. On d3d11, d3d12 and gl the
same sequence yields B alone at the new full-window viewport — which is exactly
what tests/render/pixel_tests.cpp:1607-1616 now asserts ("WHAT WAS DRAWN BEFORE
THE RESIZE IS GONE"). Camera and TextureFilter also survive on null and are
reset to Camera::DEFAULT_CAMERA / TextureFilter::point on the other three. One
seam call, four backends, two different answers — and the divergent one is the
backend whose whole purpose is to make the seam assertable without a device.

**Fix.** Give null's window_size_changed the same three steps gl got on a real
size change: clear impl_->recorded, reset() every view, and re-stamp each view's
viewport from the new width/height exactly as begin_frame does (leaving
view_count alone, per renderer.h). Then pin it in tests/render/null_tests.cpp
beside "a frame that is never submitted contributes nothing to the next" — the
one configuration that can assert on what a frame submitted. renderer.h:284 also
still enumerates three answers where there are four.

**Corrected in verification.** Three corrections, one of which strengthens the
finding.

1. WRONG SUB-CLAIM (harmless to the core, but wrong): "begin_frame is the only
   thing that clears impl_->recorded". `Renderer::submit()` at
   null/renderer.cpp:260 opens with `this->impl_->recorded.clear();` before
   re-gathering from the views. So `recorded` is cleared in two places, and the
   reporter's suggested fix step "clear impl_->recorded" is not load-bearing —
   submit rebuilds that vector from `view.sprites` unconditionally. The state
   that actually survives the resize and produces the wrong answer is
   `View::sprites`, `View::viewport`, `View::camera`, `View::filter` and
   `View::touched`. The correct fix is the two steps gl got (reset() every view;
   re-stamp viewport from the new width/height), not three.

2. UNDERSTATED CONSEQUENCE — there is a harder failure than a divergent
   recording, and the reporter missed it. `View::reset()` clears `touched` on
   all three real backends (d3d11:231, gl:148, d3d12:319), and the restart path
   calls reset() on every view (d3d11:722, gl:712, d3d12:521 inside
   abandon_recording). Null's window_size_changed calls nothing, so `touched`
   survives. `Renderer::set_view_count` (null/renderer.cpp:215-238) throws
   `std::logic_error` when the count is lowered past a view whose `touched` is
   set. The seam's return value means "re-run the layout" — so a shell that does
   exactly what it is told, re-running layout mid-frame from 2 views to 1 after
   a resize, gets an uncaught `std::logic_error` under x64-debug-null and
   silence on the other three. That inverts null/backend.h:99-105's own claim
   that this backend "has to be the strictest of the three and not the most
   permissive": it is strict here about a condition the seam says no longer
   exists. This path requires a caller that calls set_view_count twice in a
   frame, which nothing in this tree currently does — hence it raises the
   ceiling of the defect without raising its current blast radius.

3. MARGINAL SUB-CLAIM: "renderer.h:284 also still enumerates three answers where
   there are four". Line 284 is exact — `// This used to say nothing, and three
   backends answered it three ways:` — but the paragraph is a historical account
   of the three answers that were dangerous or loud, and the same file counts to
   four correctly elsewhere (`:467` "two of the four backends", `:481` "one of
   the four"). Null did also answer, so the enumeration is incomplete, but this
   is the weakest part of the report and would not stand alone as a DRIFT entry.

SEVERITY: high is overstated; medium is right. Nothing renders wrong, no golden
image moves, no test currently fails, and null draws nothing, so there is no
user-visible breakage on any shipping path today — null_tests.cpp never calls
window_size_changed (grep across engine/render/null/, null_tests.cpp and
renderer_seam_tests.cpp returns exactly one hit: the definition at
null/renderer.cpp:170). The defect is a latent seam divergence in the one
configuration whose purpose is to make the seam assertable, plus the
reachable-in-principle spurious throw above. That is squarely a DRIFT-class
defect by this project's own standard, not a high. Three corrections, none fatal
to the finding.

1. THE PROPOSED FIX IS PARTLY WRONG AND WOULD INTRODUCE A NEW DIVERGENCE. "clear
   impl_->recorded" must NOT be done. `impl_->recorded` is cleared and rebuilt
   wholesale by `submit()` (null/renderer.cpp:265-274) and cleared by
   `begin_frame` (:194), so it is redundant for a frame in progress — and it
   actively contradicts null's documented promise at :209-213 that "The
   recording stays until the next begin_frame, so a caller that reads it after
   this rather than before gets the same answer." A WM_SIZE arriving after
   `end_frame` and before the next `begin_frame` is the common case
   (Application::on_window_moved fires it on every window move,
   application.cpp:384), and the proposed fix would wipe a recording the caller
   is entitled to read. The load-bearing steps are only the two gl got:
   `view->reset()` on every view, and re-stamp `view->viewport` from the new
   width/height as begin_frame does. `RecordedSprite` is stored by value, so
   resetting views never disturbs `recorded`.

2. LINE CITATIONS DRIFT SLIGHTLY. The gl argument the finding quotes begins at
   gl/renderer.cpp:679 ("AND THE FRAME IN PROGRESS IS RESTARTED, WHICH THIS
   BACKEND DOES NOT NEED AND DOES ANYWAY"), not :686; the quoted sentences sit
   at :691-698. In pixel_tests.cpp the "WHAT WAS DRAWN BEFORE THE RESIZE IS
   GONE" comment is :1610-1614 with the asserts at :1615-1616; :1607-1608 are
   the buffer-size checks.

3. THE SEVERITY IS OVERSTATED. Nothing crashes, no pixel is wrong in any
   rasterising configuration, and no test is red today. The blast radius is
   fidelity of the headless configuration: a client asserted correct on null
   could behave differently on the three backends that ship pixels. The
   project's own framing ("two backends disagreeing about the shell's signal is
   the same class of bug as two disagreeing about a pixel",
   gl/renderer.cpp:674-675) keeps it a genuine defect, but medium is the honest
   weight, not high.

The sub-claim about renderer.h:284 ("three backends answered it three ways",
enumerating gl/d3d11/d3d12) stands as drift: four backends answered it, in three
ways, and the same file counts four at :467 and :482-488. It is minor and shares
the finding's root cause rather than being separate.

---

### C5. Both new destructors call wait_for_gpu(), which throws on a removed device — shutdown after a TDR is std::terminate, and the wait the destructors exist for does not happen

**`engine/render/d3d12/device_resources.cpp:56`** · medium · `exception-safety`

```
device_resources.cpp:47-57
	DeviceResources::~DeviceResources()
	{
		// THE ONE DESTRUCTOR IN engine/render/ THAT HAS TO DO ANYTHING ...
		if (this->command_queue_ && this->fence_)
		{
			this->wait_for_gpu();
		}

wait_for_gpu -> signal_frame (device_resources.cpp:463-470):
		ThrowIfFailed(this->command_queue_->Signal(this->fence_.Get(),
			this->fence_value_));
wait_for_gpu -> wait_for_frame (device_resources.cpp:482):
		ThrowIfFailed(this->fence_->SetEventOnCompletion(target,
			this->fence_event_));

and the same call at renderer.cpp:441, inside Renderer::Impl::~Impl().
```

**Why it is wrong.** samples/minimal/main.cpp:50-51 states the rule this breaks:
"A broken contract stops the program dead with the reason on screen, never a
silent abort (PHILOSOPHY T6)." A device loss is not hypothetical for this
backend — it is the hazard the whole DeviceNotify/handle_device_lost path exists
for — so the one place it turns into an unreported abort is the destructor pair
added to make shutdown safe. No other backend has this exposure: d3d11's is
`~DeviceResources() = default`, gl's ~Impl only calls
wglMakeCurrent/wglDeleteContext/ReleaseDC. The backend already knows how to
spell a best-effort teardown call — abandon_recording uses `std::ignore =
view->list->Close();` (renderer.cpp:518) for exactly this reason.

**Failure scenario.** A destructor is implicitly noexcept, so any exception
escaping one calls std::terminate. After a device removal — a TDR, a driver
update, an eGPU unplug, or a hang during the wait_for_gpu on the load path at
texture_factory.cpp:246 — ID3D12CommandQueue::Signal returns
DXGI_ERROR_DEVICE_REMOVED. The guards do not help: only the device was removed,
so command_queue_, fence_ and fence_event_ are all still non-null and
wait_for_gpu's own early-out (device_resources.cpp:489-493) is not taken.

Two concrete routes. (1) The removal is detected nowhere — this backend only
repairs it in present() (:445-456) and around ResizeBuffers (:310-317) — the
player quits, and ~Renderer::Impl (renderer.cpp:441) throws com_exception
(engine/core/throw_if_failed.h:43-49) out of a noexcept destructor: abort
instead of exiting 0, and the GPU wait the destructor exists to perform never
runs, so the lists, allocators and mapped vertex pages below it are released
while the GPU may still hold them. (2) The removal surfaces as a throw on the
frame path — e.g. ThrowIfFailed(allocators[frame]->Reset()) at renderer.cpp:271
— which unwinds out of Application::render() and destroys the local Application
in wWinMain; the destructor throws while an exception is already in flight, so
the catch at samples/minimal/main.cpp:48-56 never runs and neither the stderr
line nor the MessageBox appears.

**Fix.** Give the teardown path a non-throwing wait: take the HRESULTs of Signal
and SetEventOnCompletion by value and return on failure (a removed device has
already dropped every GPU reference, so there is nothing left to wait for), or
split wait_for_gpu into the throwing frame/load/resize/read-back form and a
bool-returning one the two destructors call. Note also that
device_resources.cpp:49's "THE ONE DESTRUCTOR IN engine/render/ THAT HAS TO DO
ANYTHING" sends a reader auditing exactly this away from renderer.cpp:431, the
sibling with the same defect (see the d3d12-local claims finding).

**Corrected in verification.** Five corrections, none fatal to the core claim.

1. WRONG THROW SITE. device_resources.cpp:482 (`SetEventOnCompletion`) is NOT
   reachable after a device removal. `ID3D12Fence::GetCompletedValue` is
   documented to return UINT64_MAX once the device is removed, so the early-out
   at :477-480 (`if (GetCompletedValue() >= target) return;`) fires and :482 is
   skipped — which also means there is no INFINITE hang at :484. The single
   throw site in the teardown chain is the Signal at :466, so the whole finding
   rests on that one HRESULT.

2. CONTINGENT ON A RUNTIME BEHAVIOUR NOT PROVABLE HERE. If a given
   driver/runtime returns S_OK from Signal on a removed device, the destructor
   is harmless on that machine. The defect is still real as a code shape (a
   noexcept destructor calling a ThrowIfFailed path, against T6's explicit
   "teardown stays silent"), but the reporter states the abort as certain where
   it is "whenever Signal reports the removal".

3. THE SECOND HARM DOES NOT HAPPEN. "the lists, allocators and mapped vertex
   pages below it are released while the GPU may still hold them" is wrong:
   std::terminate aborts the process at the ~Impl boundary, so nothing below is
   released at all. And on a removed device the GPU holds nothing — the
   finding's own Fix says exactly that. The two consequences are alternatives;
   only the abort occurs.

4. ROUTE (1) IS NARROWER THAN STATED. present() does repair removal (:445-456),
   so a quiet quit after a TDR requires no Present between the removal and the
   quit — a TDR on the final frame with WM_CLOSE arriving first. Route (2) is
   the realistic one and should carry the finding.

5. THE STRONGEST CASE IS THE ONE NOT CITED: RenderPixelTests. That harness
   deliberately never presents (device_resources.h:102-109 and the commit
   message say so, because a flip-model Present discards what it reads back), so
   removal is detected NOWHERE in that configuration. read_back_buffer's
   `wait_for_gpu()` at renderer.cpp:1299 throws, unwinding destroys the
   harness's Renderer, ~Impl terminates the test process, and the doctest
   failure never prints — a CI abort with no reason, on the one preset CI
   rasterises twice.

Minor: the finding calls both destructors "guarded". renderer.cpp:441 has no
guard of its own at all — it calls wait_for_gpu unconditionally and relies
entirely on that function's :489-493 early-out.

Severity: high is a notch overstated. The trigger is a hardware fault, and in
the dominant route the process was already dying — the delta is a reported
exit-1 versus a silent abort, not a wrong pixel or corruption. Medium fits: a
genuine defect against an explicit PHILOSOPHY T6 clause, with a two-line fix, on
a path that only opens after a device loss. Four corrections, none fatal to the
finding.

1. WRONG RULE CITED (the important one). The finding sources the rule to
   samples/minimal/main.cpp:50-51. The authoritative source is
   docs/design/PHILOSOPHY.md:116 — T6's "**Not a licence for:** throwing on the
   way out — teardown stays silent" — which prohibits this construct by name.
   Add that CLAUDE.md's amendment rule is also broken: 3dda092 edited
   PHILOSOPHY.md but left T6 untouched, so the commit fights a philosophy
   without amending it.

2. MECHANISM: which call actually throws. The finding presents Signal (:466) and
   SetEventOnCompletion (:482) as equally likely throw sites. On a removed
   device the D3D12 runtime signals fences to UINT64_MAX so pending waits do not
   hang, which means wait_for_frame's `GetCompletedValue() >= target` early-out
   at :477-480 is taken and :482 is never reached — the concrete throw site is
   `Signal` at device_resources.cpp:466-467. (If a runtime does not auto-signal,
   :482 becomes the throw site instead. Either way exactly one of the two
   throws, so the finding is robust here — but the report should name Signal.)

3. OVERSTATED HARM. "the lists, allocators and mapped vertex pages below it are
   released while the GPU may still hold them" is weak in the device-removal
   case: once the device is removed it has stopped executing its work, which is
   exactly why a best-effort no-op wait is a safe fix. The substantive harm is
   the unreported `std::terminate`, not a dangling GPU reference. The
   dangling-reference harm is real only in the rarer sub-case where Signal
   succeeds on a live device and SetEventOnCompletion then fails.

4. MISSED, AND IT STRENGTHENS THE FINDING. There is a third route that does not
   need a destructor at all. The shared seam declares `Renderer(Renderer&&)
   noexcept;` and `Renderer& operator=(Renderer&&) noexcept;`
   (engine/render/renderer.h:260-261), and d3d12/renderer.cpp:1018-1019 defines
   both `= default` over `std::unique_ptr<Impl> impl_` (renderer.h:459). A
   move-assignment destroys the old Impl inside a function the seam EXPLICITLY
   marks noexcept — so the throwing ~Impl breaks a written noexcept promise on
   the shared seam, not merely an implicit one. Nothing in-tree move-assigns a
   Renderer today, so this is latent rather than live, but it means the seam
   itself already requires ~Impl to be non-throwing.

Also note the location line should name both sites, not one:
device_resources.cpp:56 and renderer.cpp:441.

---

### C6. renderer.h's begin_frame paragraph still counts three backends and enumerates three kinds of reset, while the same file and the commit message both assert it needed no rewriting to take a fourth

**`engine/render/renderer.h:340`** · low · `comment-drift`

```
renderer.h:338-344
		// A FRAME BEGUN AND NEVER SUBMITTED CONTRIBUTES NOTHING TO THE NEXT
		// ONE, which is a statement about what "resets" means and is worth
		// making because the three backends have three different things to
		// reset. Two of them hold a frame in a vector, where dropping it is
		// clearing the vector; the D3D11 one holds it in a deferred context,
		// which keeps what was recorded into it until something takes the
		// command list away, so it has to drain as well as forget.

renderer.h:591-594, added by 3dda092:
//    reset under it - and that paragraph already said the three backends had
//    three different things to drop, which is why it needed no rewriting to
//    take a fourth.
```

**Why it is wrong.** CLAUDE.md: a document a change fights is amended in the
same commit as the change. 3dda092 amended nine other count sentences in this
header (:40 "FOUR BACKENDS", :467 "two of the four backends", :482-489, :523)
and instead of amending this one added a paragraph 250 lines below asserting no
amendment was needed. That assertion is checkable and false, and the fourth kind
of reset is the term whose mishandling d31a804 records as "a dead process".
docs/review/backend-equivalence/DRIFT.md holds this exact class (":40, :255,
:328 — 'two backends' / 'a second backend' throughout. Three.") as a defect, and
renderer.h is "not a header with comments, it is the specification of the seam".

**Failure scenario.** Count the paragraph it points at: "the three backends",
then "Two of them hold a frame in a vector" (gl, null) plus "the D3D11 one holds
it in a deferred context" — three backends, and the enumeration closes. The
fourth thing a backend may have to drop, an open ID3D12GraphicsCommandList that
cannot be reset and whose allocator cannot be reset under it
(Renderer::Impl::abandon_recording, d3d12/renderer.cpp:505-529), appears nowhere
on the seam. A reader following d3d12/renderer.cpp:1113-1120 ("A FRAME BEGUN AND
NEVER SUBMITTED CONTRIBUTES NOTHING TO THE NEXT ONE (renderer.h)") to the seam
for the rule finds a closed list that does not contain their case, and a fifth
backend written against this paragraph would implement only the listed ones. The
local copy at d3d12/renderer.cpp:1115 repeats the wrong count in the same commit
— "The three backends have three different things to drop" — and then names four
backends' worth in its own list.

**Fix.** Rewrite :338-347 as four backends and four things: two clear a vector,
the D3D11 one drains a deferred context, and the D3D12 one closes a command list
that is still recording, because neither it nor its allocator can be reset while
it is open. Then delete or correct the justification at :591-594 and the copied
count at d3d12/renderer.cpp:1115.

**Corrected in verification.** 1. "The fourth thing a backend may have to drop
... appears nowhere on the seam" is false. renderer.h:589-592, in the same file
and the same commit, names it exactly: "a command list that is still open, which
cannot be reset and whose allocator cannot be reset under it". The defect is a
stale count plus an unamended illustration, not an absent term.

2. The title says the paragraph "enumerates three kinds of reset". It enumerates
   TWO kinds over three backends — a vector (two of them) and a deferred context
   (D3D11). The reporter's own Fix text gets this right ("two clear a vector,
   the D3D11 one drains a deferred context, and the D3D12 one closes a command
   list"), so the Fix's headline "four backends and four things" also miscounts:
   four backends, three kinds.

3. THE FINDING IS UNDER-SCOPED, and this weakens its narrative that :340 was
   uniquely skipped in favour of a self-justifying paragraph. The same
   member-comment block has at least two more count sentences neither commit
   amended, both now false for the same reason:
   - renderer.h:359-362 — "on two backends it is a vector nothing will replay,
     on the third a deferred context holding commands submit() will not reach.
     All three throw it". Four throw it; d3d12/renderer.cpp:1156 throws the same
     std::logic_error, and its stranded recording is a command list, not either
     listed thing.
   - renderer.h:375-376 — "two of the three replay a vector, and the D3D11 one
     executes a command list per view". d3d12/renderer.cpp:1197-1226 executes a
     command list per view too (in one ExecuteCommandLists, per its own comment
     at :1199-1202). Should be two of the four, and both Direct3D ones. The
     accurate description is that 3dda092 renumbered the top-of-file and
     epilogue prose and left the member-function comment block un-renumbered
     throughout — three instances, one work item — rather than that it singled
     this paragraph out.

4. d3d12/renderer.cpp:1115 is weaker evidence than the reporter thinks. "The
   three backends have three different things to drop: two clear a vector, the
   D3D11 one drains a deferred context, and this one has a command list that is
   still recording" admits a reading where "the three backends" means the three
   others and "this one" is deliberately set outside that count. It is at best
   ambiguous rather than plainly wrong; the header's copy has no such escape
   because it has no "this one".

5. "the term whose mishandling d31a804 records as 'a dead process'" is loose.
   The dead process was the RESIZE path — window_size_changed
   (d3d12/renderer.cpp:1065-1077) releasing the back buffer with lists still
   naming it — not begin_frame. Same abandon_recording term, different entry
   point. Three corrections; none touch the core.

1. OVERSTATED: "appears nowhere on the seam" is false as written. The fourth
   kind of reset *is* stated on the seam, at renderer.h:588-591 — "'begin_frame
   resets every view's recording' now covers a third kind of thing to reset - a
   command list that is still open, which cannot be reset and whose allocator
   cannot be reset under it". The defect is location, not absence: it sits 250
   lines below the declaration it governs, inside the file's closing essay,
   while the paragraph that states the rule *at* `begin_frame()` still
   enumerates three. Rephrase as "does not appear in the paragraph that states
   the rule at the declaration"; the version that says "nowhere" is refutable in
   one grep and will be refuted.

2. "amended nine other count sentences" is not exactly right — the 3dda092
   renderer.h diff amends about six (`:40`, `:357-362`, `:467`, `:482-497`,
   `:498-512`, `:523`). Drop the number and cite the diff; the substance is
   unaffected.

3. UNDER-SCOPED — the same commit left at least three more of this exact class,
   and a fix limited to `:338-347` would leave the sweep half done:
   - renderer.h:375 (submit): "two of the three replay a vector, and the D3D11
     one executes a command list per view". d3d12 executes a command list per
     view and is unaccounted for; same paragraph, same defect, same commit.
   - renderer.h:444: d3d11's device_resources is "the only one of the three with
     such a wrapper to strip, because it is the / only one whose device can be
     lost" — contradicted by :467-472 in the same file, amended by the same
     commit: "D3D12 loses a / device the same way D3D11 does".
   - d3d12/renderer.cpp:902: "the third file of this backend is the longest of
     the three" — longest of four (115/264/168/48, per the author's own list at
     renderer.h:487-489). Add these to the Fix alongside `:338-347`, `:592-594`
     and d3d12/renderer.cpp:1115.

On severity: medium is defensible and I would keep it, but note the ceiling —
DRIFT.md itself files this class as "a work item, not a fix". No runtime
consequence. What holds it at medium rather than low is that the same commit
added an explicit false assertion that no amendment was needed, and that the new
backend's own copy contradicts itself in a single sentence.

---

### C7. View::begin's fence invariant names begin_frame as the waiter, but d31a804 gave it a second caller where begin_frame waited on a different frame index

**`engine/render/d3d12/renderer.cpp:264`** · low · `comment-drift`

```
renderer.cpp:264-272
		// THE ALLOCATOR IS RESET HERE AND NOWHERE ELSE, and Renderer::begin_frame
		// has already waited on the fence for this frame index. That sentence is
		// the whole of what this API asks of the engine that the other two do
		// not: ... reusing it while the GPU is still reading last time's commands
		// is not an error anything reports - it is a frame drawn from two frames'
		// commands at once.
		ThrowIfFailed(this->allocators[static_cast<size_t>(frame)]->Reset());

and the wait the restart path actually depends on, device_resources.cpp:290-292:
		// Nothing may be released while the GPU is still reading it, and a back
		// buffer is the thing most likely to be.
		this->wait_for_gpu();
```

**Why it is wrong.** This repository holds comments in engine/render/ to the
same standard as the design documents and keeps a live list of the ones the code
has outgrown (DRIFT.md: "Each item is a claim that the code no longer matches,
with the line that contradicts it"). "Renderer::begin_frame has already waited
on the fence for this frame index" is such a claim, and d31a804 — the commit
that added the second caller and reworked the frame loop heavily — did not amend
it, nor the wait at device_resources.cpp:290-292 that silently became
load-bearing for two invariants instead of one.

**Failure scenario.** Before d31a804, DrawList::View::begin() had one caller,
Renderer::set_view_count, always downstream of begin_frame's wait_for_frame() on
the current index. d31a804 added a second: Renderer::Impl::open_frame()
(renderer.cpp:554), reached from window_size_changed (:1087). On that path the
index is not the one begin_frame waited on —
create_window_size_dependent_resources re-reads frame_index_ from the swap chain
after ResizeBuffers (device_resources.cpp:381-382), and a flip-model chain
resets it — so a frame that began at index 1, whose wait_for_frame() therefore
waited on frame_fences_[1], resets allocators[0] here. open_frame's own
frame_allocators[frame]->Reset() (renderer.cpp:534) has the same property and no
comment at all.

The reset is in fact safe, but only because of the wait_for_gpu() at
device_resources.cpp:292, which this paragraph does not mention and whose own
comment scopes it to releasing back buffers. Anyone who reorders the resize path
— moving that wait after ResizeBuffers, short-circuiting it when only the RTVs
change, or adding a third caller of open_frame — follows the rule as written,
removes the only guarantee on that path, and gets the failure this very comment
says nothing reports.

**Fix.** Name the invariant rather than one caller: the allocator for frame
index N may be reset only after a wait that covers everything signalled against
N, which is begin_frame's wait_for_frame() on the ordinary path and
create_window_size_dependent_resources' wait_for_gpu() on the restart path, and
the index may differ between the two. Extend the wait's own comment at
device_resources.cpp:290 to say it is also what makes those resets legal after
ResizeBuffers moves frame_index_.

**Corrected in verification.** Four corrections, one of which is why I dropped
medium to low:

1. SEVERITY. This is documentation-only. The code is safe on every reachable
   path, and the path is covered by a passing device test
   (pixel_tests.cpp:1559). More importantly, the reporter's "whose own comment
   scopes it to releasing back buffers... nothing mentions it" overstates the
   gap: renderer.cpp:1072-1075 already ties the two together at the restart path
   — "IT HAS TO HAPPEN BEFORE THE RESIZE AND NOT AFTER IT. A closed list is not
   the problem; an OPEN one is, because the resize waits for the GPU and then
   resets allocators that a recording list is still holding." A maintainer
   reordering that path is warned there that the wait precedes the allocator
   resets. What is genuinely missing is (i) the correction at the reset site
   itself and (ii) the fact that the index may differ from the one begin_frame
   waited on. That is a real drift item, but a low one.

2. That same sentence at :1073-1074 is itself slightly loose and worth fixing in
   the same pass: the resize (`create_window_size_dependent_resources`) does not
   reset any allocator; `open_frame()` at :1087 does, after it returns.

3. Line nit: the `frame_allocators[frame]->Reset()` statement is
   renderer.cpp:534-535, with the `->Reset()` text on 535; the finding cites 534
   (the `ThrowIfFailed(` line).

4. "open_frame's own frame_allocators[frame]->Reset() ... has no comment at all"
   is not a finding on its own — a missing comment is a "consider adding", which
   this project excludes. The defect is solely the false claim at :264-265.

WORTH ADDING, same family, same commit, which the finding missed:
backend.h:210-214 documents `recording` as "Deferred to set_view_count for the
reason the D3D11 backend defers its binding", and d31a804 gave it a second
setter in `open_frame` (renderer.cpp:552-556) — the identical
single-caller-named-as-the-invariant drift. And backend.h:148-150 ("page N of
frame 0 is written again only after the fence says frame 0's last submission
finished") leans on the same unnamed `wait_for_gpu`, because `View::begin`
resets `page`/`page_position` for the new index at renderer.cpp:294-295. If this
is written up, all three should be fixed together, since they are one omission:
the restart path's wait is what licenses every per-frame-index reuse after
`ResizeBuffers` moves `frame_index_`. Three corrections, one of which is why I
dropped the severity.

1. SEVERITY: medium → low. The finding's strongest sentence — that
   `wait_for_gpu` "silently became load-bearing" with nothing saying so —
   overstates it. `renderer.cpp:1072-1075`, added by the same commit, does state
   the dependency at the restart site: "IT HAS TO HAPPEN BEFORE THE RESIZE AND
   NOT AFTER IT. A closed list is not the problem; an OPEN one is, because **the
   resize waits for the GPU and then resets allocators** that a recording list
   is still holding." A maintainer reordering the resize path reads that
   comment, not `View::begin`'s. The drift is real but it is a stale
   *attribution* in one paragraph, not an undocumented invariant; DRIFT.md
   itself frames this class as "a work item, not a fix".

2. The finding asserts "a flip-model chain resets it [to 0]" as though DXGI
   documents it. `IDXGISwapChain::ResizeBuffers` does not formally specify the
   resulting `GetCurrentBackBufferIndex` value; the reliable statement is the
   weaker one, which is all the finding needs: the index is not guaranteed to be
   preserved, which is precisely why this code re-queries it at
   `device_resources.cpp:381-382` and why `move_to_next_frame` (`:499-507`) says
   the swap chain "is entitled to disagree". Phrase it as "may differ", not
   "resets it".

3. Line numbers: the comment block is `renderer.cpp:264-270` and the `Reset()`
   it guards is `:271`, not "264-272". Also, the parenthetical complaint that
   `open_frame`'s `frame_allocators[frame]->Reset()` at `:534-535` "has the same
   property and no comment at all" is not itself drift — an absent comment makes
   no claim — so it should be dropped from the finding rather than carried as a
   second instance.

The proposed fix is sound as written, and the cheapest correct form of it is to
replace "Renderer::begin_frame has already waited on the fence for this frame
index" with the invariant plus both waiters (`begin_frame`'s `wait_for_frame()`
on the ordinary path, `create_window_size_dependent_resources`' `wait_for_gpu()`
on the restart path, where the index may have moved), and to extend
`device_resources.cpp:290-291` past "a back buffer is the thing most likely to
be" to name the allocator resets it also licenses.

---

### C8. renderer.h says D3D11 is the only backend with a device wrapper and the only one whose device can be lost, twenty-seven lines above text the same commit added saying the opposite

**`engine/render/renderer.h:444`** · medium · `comment-drift`

```
renderer.h:443-445
		// (engine/render/d3d11/device_resources.h says which and why - it is the
		// only one of the three with such a wrapper to strip, because it is the
		// only one whose device can be lost).

renderer.h:471-474, added by 3dda092:
//    made worth asking, and a fourth has now answered half of: D3D12 loses a
//    device the same way D3D11 does, so the hazard belongs to half the
//    backends rather than to one
```

**Why it is wrong.** One commit, one file, two paragraphs, opposite claims about
the same fact — the internal-contradiction class DRIFT.md already files against
this header (":236-238, :296-304 ... contradicted by :399-406 in the same
file"). 3dda092 rewrote every neighbouring count sentence for the fourth backend
and left this one, so the seam's specification now states both "only one" and
"half of them".

**Failure scenario.** A reader auditing device loss — which renderer.h:235-247
(DeviceNotify) sends them here to do, and which the STILL OPEN item at :465-475
asks them to settle — reads the parenthetical, concludes the hazard is confined
to d3d11, and never opens engine/render/d3d12/device_resources.h (a second
wrapper of exactly the same kind), device_resources.cpp:410-437
(handle_device_lost), :445-456 (the DXGI_ERROR_DEVICE_REMOVED branch in present)
or d3d12/renderer.cpp:962-1008 (on_device_lost/on_device_restored). Any rule
they then write — a DeviceNotify obligation, a reload path, a test — is scoped
to one backend when it must cover two. Both halves of :444-445 are false: "one
of the three" (there are four) and "the only one whose device can be lost" (two
are).

**Fix.** Amend :443-445 to say d3d11/device_resources.h is the one with vendored
accessor wrappers to strip — that argument is d3d11's alone, its file being the
Microsoft sample — and drop "the only one whose device can be lost", which
:471-473 now settles; point at both device_resources.h files.

**Corrected in verification.** 1. OVERSTATED: "Both halves of :444-445 are
false". The wrapper half is more defensible than that.
engine/render/d3d12/device_resources.h:16-23 states the D3D12 wrapper "IS NOT
THE D3D11 FILE TRANSLITERATED ... This file is new, so it is written the way
CONVENTIONS says", and 3dda092's message says "there are two
device_resources.cpp now and only one of them is Microsoft's". So d3d12's
wrapper never had vendored accessors TO STRIP, and "the only one with such a
wrapper to strip" survives on a narrow reading. What is unambiguously false is
(i) the count "one of the three" - there are four backends, and every other
count in the same block was updated by the same commit, and (ii) the causal
clause "because it is the only one whose device can be lost". The reporter's
suggested Fix already draws exactly this distinction, so the remedy is right
even though the "both halves" framing is not.

2. OVERSTATED ROUTING: "which renderer.h:235-247 (DeviceNotify) sends them here
   to do". It does not. :235-247 is a bare DeviceNotify declaration with the
   comment "Not a graphics type: a game object that has to rebuild something
   after a device loss implements this and knows nothing about what was lost."
   It contains no cross-reference to :443 or to any device_resources.h. The
   reader reaches the false parenthetical by reading the seam specification end
   to end, and by the STILL OPEN item at :465-475 which does pose the
   device-loss question. The scenario is reachable; the stated navigation path
   is not literal.

3. ADDITIONAL DRIFT IN THE SAME PARAGRAPH, not mentioned in the finding and
   worth folding into the same amendment: :439-441 "Debug markers. The only
   capability of the backend's device wrapper that reaches the seam unchanged".
   On d3d12 the markers touch no device wrapper at all -
   d3d12/renderer.cpp:1330-1348 makes all three no-ops ("THE THREE MARKERS DO
   NOTHING ... the D3D12 equivalent is an opaque blob whose encoding belongs to
   WinPixEventRuntime, a library this repository would have to buy to write
   three no-op wrappers (T9)"), against d3d11/renderer.cpp:982
   `this->impl_->device_resources.PIXBeginEvent(name)`. That sentence was
   already loose for gl before these commits, so it is pre-existing rather than
   introduced, but 3dda092 widened it from one exception to two.

4. SEVERITY CONFIRMED at medium, not raised. It is documentation-only: no crash,
   wrong pixel, leak or race follows, and the correct behaviour is fully
   implemented on both Direct3D backends. It sits at the top of the
   comment-drift band rather than above it because the file is the seam's
   specification, the contradiction is intra-file and intra-commit, and it
   regresses a previously-fixed DRIFT.md entry. Three corrections, all narrowing
   rather than sinking the finding.

1. "Both halves of :444-445 are false" is stronger than the evidence supports
   for the strip clause. The "with such a wrapper to strip" argument has a
   defensible narrow reading that survives: d3d11/device_resources.h:77-84
   documents seventeen accessors trimmed to five precisely because that file is
   the vendored Microsoft sample ("There were seventeen. Twelve had no caller
   anywhere in the repository"), whereas d3d12/device_resources.h:17-23 says it
   was written fresh in this repo's conventions and so never had accessors to
   strip. Under that reading only the word "three" is false in the first half
   (there are four backends, and two now have a wrapper of that kind -
   d3d12/device_resources.h:11-14 says so itself: "The D3D11 folder has one of
   these"). The second half, "the only one whose device can be lost", has no
   such defence and is flatly false. The proposed fix is therefore exactly
   right: keep the vendored-accessor argument as d3d11's alone, drop the
   device-loss clause, point at both device_resources.h files.

2. The failure scenario overstates one cross-reference. renderer.h:235-247
   (DeviceNotify) names no backend and contains no pointer to :443 - it reads
   only "a game object that has to rebuild something after a device loss
   implements this and knows nothing about what was lost." A reader reaches :443
   by reading the header down or by grepping for device loss, not because
   DeviceNotify sends them. The scenario stands on the STILL OPEN item at
   :465-475 alone, which is the real neighbour.

3. "3dda092 rewrote every neighbouring count sentence for the fourth backend and
   left this one" is not accurate. It also left :340 ("the three backends have
   three different things to reset" - four backends now, and d3d12 adds a fourth
   kind, an open command list, which the commit message itself claims the
   paragraph already covered) and :375 ("two of the three replay a vector, and
   the D3D11 one executes a command list" - two of the four replay a vector, and
   both Direct3D backends execute command lists). This is a family of leftovers
   in one file, not a single miss; the finding is one member of it and should
   say so rather than claim uniqueness.

Line-number nit: handle_device_lost runs 410-436, not 410-437.

---

### C9. pixel_tests.cpp's frame census still says forty-eight frames, one exemption and two rasterising backends — three counts, each broken by one of the two commits, in the file that is the golden set's coverage statement

**`tests/render/pixel_tests.cpp:62`** · medium · `comment-drift`

```
pixel_tests.cpp:62-65
// argument. Forty-eight frames; forty-seven of them are 64x64 on every backend
// and identical across the two that rasterise. The forty-eighth is read out of
// a buffer the seam says is a different size per backend, and
// Harness::end_not_comparable is where that is written down.

against the same file's amended comment at :298-307 ("THERE ARE TWO OF THEM NOW AND THEY ARE EXEMPT FOR THE SAME REASON: neither is 64x64"), and CLAUDE.md:69-70, untouched by either commit: "`Harness::end_not_comparable` ... holds the one frame whose size the seam makes backend-specific".
```

**Why it is wrong.** This header block is the specification of what the golden
set covers, and d31a804's own message claims the amendment was made: "The
comment on Harness::end_not_comparable now covers both, rather than saying 'the
one frame'." That is true of one comment and false of the file header forty
lines above it, of :340, and of CLAUDE.md — one fact stated in four places, one
amended. DRIFT.md exists to catalogue exactly this, and the file in question is
the executable statement of the pixel contract.

**Failure scenario.** `grep -c "harness\.end();"` returns 47 and
`end_not_comparable()` is called at :1497 and :1603 — 49 frames against 47
images in tests/render/golden/, with two exempt, not 48 and one. "identical
across the two that rasterise" is three since 3dda092. And the stated reason no
longer covers the second exemption: the header says the exempt frame "is read
out of a buffer the seam says is a different size per backend", whereas :304-306
says the new one "asks for a 32x32 buffer deliberately and gets it on every
backend" — the opposite reason. Two more statements in the same file are stale
the same way: :312-317 quotes renderer.h's back_buffer_size verbatim as "the
D3D11 backend answers the size it was told", a sentence 3dda092 rewrote to "both
Direct3D backends answer the size they were told", so the citation no longer
matches its source and cannot be checked; and :340 says "Every case but one
knows both are BUFFER_SIZE", now two.

CLAUDE.md instructs a contributor to regenerate goldens with
LABRADOR_GOLDEN_DUMP=1 and "review every image it changes". A reviewer
reconciling 48 declared frames against 47 images concludes one image is missing
and looks for the wrong case; with 49 and two exemptions the check cannot be
performed at all.

Separately, the second exemption is weakly grounded: golden::check_frame takes
the frame's width and height as parameters (golden_image.h:39-40) and
Harness::end already passes them from buffer_, and the new case reads back
exactly once at 32x32 on every backend — so a 32x32 golden image is
representable, and the newest, least-shared code path is outside the only
cross-backend comparison in the repository for a reason the same paragraph
contradicts.

**Fix.** Amend :62-65 to forty-nine frames, forty-seven held to golden images
across the three backends that rasterise, and two exempt for two different
reasons; re-quote the current renderer.h:394-399 sentence at :312-317; change
:340 to "every case but two"; and amend CLAUDE.md:69-70 to "the two frames no
golden image can hold — one whose size the seam makes backend-specific, one that
changes size mid-frame". Consider calling end() rather than end_not_comparable()
in the mid-frame resize case, which would put the newest path back inside the
golden set.

**Corrected in verification.** Line numbers: the amended `end_not_comparable`
block runs :298-309, not :298-307 (trivial). renderer.h's current sentence is at
:393-399, not :394-399.

The finding understates the drift by one item. The block d31a804 itself amended
contradicts its own new paragraph seven lines later.
tests/render/pixel_tests.cpp:316-317, inside the hunk that commit added to:

// the other BY CONTRACT, and one file cannot be both. Every other frame // here
is 64x64 on every backend and is compared byte for byte.

"Every other frame here is 64x64 on every backend" is false as of the same
commit that wrote the paragraph above it, which says one of the two exempt
frames "asks for a 32x32 buffer deliberately and gets it on every backend". So
the amendment d31a804's message claims ("The comment on
Harness::end_not_comparable now covers both") is itself internally inconsistent
— a seventh stale statement, and the one closest to the change.

The finding slightly overstates one half of :62-63. "forty-seven of them are
64x64 on every backend" is still exactly right: 47 golden-compared frames remain
64x64. Only the total (48 -> 49), the ordinal ("The forty-eighth", now two), and
"the two that rasterise" (three) are wrong.

The "Separately" paragraph should not carry weight. The claim that the second
exemption is "weakly grounded" and the closing "Consider calling end() rather
than end_not_comparable()" are a design preference, and the brief excludes both
"consider adding" and "restating a limitation the code already documents at the
point it happens" — :301-309 documents the choice and its reason exactly where
it is made. It is technically true that golden::check_frame(int width, int
height, ...) at golden_image.h:39-40 takes the size and that Harness::end passes
buffer_.x/buffer_.y (:293-294), so a 32x32 golden is representable, and
golden_image.cpp:146-165 slugs by case name so a new size would just be a new
file. But that makes it a defensible alternative, not a defect. The core
comment-drift finding stands entirely without it. Four corrections, none fatal;
three of them strengthen the finding.

1. "CLAUDE.md:69-70, untouched by either commit" is imprecise about the file.
   `git show 3dda092 --stat` shows CLAUDE.md with 63 lines changed, and the diff
   amends the line immediately above the cited sentence ("both rasterising
   backends" -> "all three rasterising backends"). The accurate statement:
   3dda092 edited that very paragraph and stopped one line short, and d31a804 —
   the commit that created the second exempt frame — touched no documentation at
   all. That makes the omission more pointed, not less.

2. The finding cites :312-317 for the stale quote but misses the sharper defect
   inside it. :316-317 still reads "Every other frame here is 64x64 on every
   backend and is compared byte for byte" — fifteen lines below :301's new
   "THERE ARE TWO OF THEM NOW AND THEY ARE EXEMPT FOR THE SAME REASON: neither
   is 64x64", inside the same comment block d31a804 rewrote. That is a
   self-contradiction within one comment, not merely a stale citation, and it
   means d31a804's message claim ("now covers both") is only partly true even of
   the comment it names.

3. The same one-fact-in-many-places drift extends to a file the finding does not
   name: tests/render/golden_image.h:14-15 ("Every term the two real backends
   hand-copied from each other") and :25 ("two runs, two configurations, one set
   of images"), both stale since 3dda092 added a third rasteriser.
   golden_image.h is the file :61 sends the reader to for "the whole argument",
   so it belongs in the same fix.

4. Trim the last paragraph of the Fix. "Consider calling end() rather than
   end_not_comparable() in the mid-frame resize case" is a design suggestion,
   and :305-309 already documents the rationale at the point it happens ("a case
   that changes the size mid-frame has no image to be ... both of them assert on
   pixels they address themselves") — the review's own exclusions rule that out
   as a finding. What survives is only the drift: :64-65 states a reason ("read
   out of a buffer the seam says is a different size per backend") that does not
   cover the second exempt frame.

Exact targets for the fix: pixel_tests.cpp:62-65, :311-317, :340-342;
CLAUDE.md:69-70; golden_image.h:14-15 and :25; the sentence to re-quote is
renderer.h:395-399.

---

### C10. gl/sprite_shader.h sends the reader to engine/render/d3d11/sprite.hlsl, a path this commit deleted, in the one comment whose job is to keep the two shaders in step

**`engine/render/gl/sprite_shader.h:6`** · medium · `comment-drift`

```
gl/sprite_shader.h:3-11
// The only shader this backend has, and a transliteration of the only shader
// the other one has.
//
// SIDE BY SIDE WITH engine/render/d3d11/sprite.hlsl, deliberately. Both do one
// multiply-add to reach clip space, one texture fetch and one multiply, because
// every term of the pixel contract is settled on the CPU in
// engine/render/sprite_geometry.cpp before either of them runs. The two files
// differing in more than syntax would mean the contract had leaked into a
// shader, which is the failure this arrangement exists to prevent.
```

**Why it is wrong.** CLAUDE.md's rule is that a change amends the text it fights
in the same commit, and DRIFT.md already lists two dead in-tree paths of exactly
this shape (":344-345 — sends the reader to
engine/render/<backend>/device_resources.h, a file two of three folders do not
have. Same dead path at sprite_vertex.h:17-19"). This is live engine code, not a
historical review document, and it is the sole statement of the rule that the
GLSL and the HLSL must differ in nothing but syntax.

**Failure scenario.** `git show --stat 3dda092` shows
`engine/render/d3d11/sprite.hlsl | 75 --` deleted and `engine/render/sprite.hlsl
| 97 +++` added; `ls engine/render/d3d11/` confirms no .hlsl remains. The commit
updated every other cross-reference — CLAUDE.md, ARCHITECTURE.md:121 and :318,
renderer.h:499, and all four compile_hlsl calls in engine/CMakeLists.txt — but
not this one, and the reference is one half of a pair:
engine/render/sprite.hlsl:18-20 points back at "render/gl/sprite_shader.h says
what that costs". A reader following the instruction to open the two files side
by side, which is the only stated check that the GLSL has not diverged from the
HLSL, lands on a path that was git rm'd in the same commit. Lines 3-4 are wrong
a second way: "a transliteration of the only shader the other one has" — two
backends now compile that HLSL, at two profiles.

**Fix.** Point line 6 at engine/render/sprite.hlsl and reword lines 3-4 so "the
other one" becomes the two Direct3D backends that take HLSL — the same edit
sprite.hlsl:18-20 already makes in the other direction.

**Corrected in verification.** Two things to tighten, neither of which touches
the core claim.

(a) The secondary claim about lines 3-4 is slightly overstated as written. "the
only shader the other one has" — the *"only shader"* half actually survives,
because both Direct3D backends compile the same single file at two profiles, so
there is still exactly one HLSL shader. What breaks is *"the other one"*: there
are now two other backends with a shader, so the phrase no longer picks out a
unique referent.

(b) The finding under-scopes the reword. The same singular-backend error appears
twice more in the same header and the fix must cover them, or the file is only
half-corrected:
  - `sprite_shader.h:13` — `// COMPILED AT RUN TIME, WHERE THE OTHER ONE IS
    COMPILED AT BUILD TIME` (two "other ones" now, both at build time but at
    different profiles — 4_0_level_9_1 and 5_1).
  - `sprite_shader.h:36` — inside the GLSL string, `// same constant the other
    backend uploads, which is why RenderPixelTests can / // hold them to the
    same answers` — three rasterising backends are now held to one golden set,
    and d3d12 does not "upload" the constant at all, it passes it as four root
    constants (per 3dda092's own commit message: "four root constants where
    D3D11 has a per-view dynamic constant buffer").

Severity medium is right and I would not move it. It is comment-only with no
runtime effect, which caps it below high; but it is live engine code (not
`docs/review/`), it is the sole written statement of the GLSL/HLSL equivalence
rule, it was falsified by the commit under review rather than inherited, and
DRIFT.md:29 ranks a dead in-tree path of exactly this shape as a work item.
CLAUDE.md's amend-in-the-same-commit rule was applied to five other
cross-references in this very commit and missed only this one. Two
overstatements in the rationale, neither of which touches the defect:

1. "the only stated check that the GLSL has not diverged from the HLSL" is too
   strong. The mechanical check is the golden set, and
   `engine/render/sprite.hlsl:31-35` names it: "the two produce the same
   arithmetic - which `tests/render/golden/` is what actually checks", and
   CLAUDE.md confirms one set of 47 images holds all three rasterising backends.
   The side-by-side reading is the only *source-text* check (structure,
   comments, intent); behavioural divergence is caught by the images. Reword
   "the sole statement of the rule" to "the only instruction to compare the two
   sources by eye".

2. "all four compile_hlsl calls in engine/CMakeLists.txt" is accurate but worth
   splitting: two of the four (`:143-149`, the `5_1` pair) are new, and two
   (`:121-128`, the `4_0_level_9_1` pair) had their source path rewritten by
   this commit. Both readings support the point.

Also add to the fix, since a fixer will otherwise leave half the drift:
`DRIFT.md:39` still cites `engine/render/d3d11/sprite.hlsl:11-16` and must NOT
be updated — `docs/review/` is historical by CLAUDE.md and DRIFT.md's own header
pins it to `57b65b3`.

Severity: medium is defensible — sole surviving dead path in the tree, newly
introduced, in a file the GL preset actually compiles, and one half of a
bidirectional pair the same commit wrote the other half of. Low is arguable on
consequence alone, since there is no runtime, pixel or build effect.

---

### C11. Six more sentences of renderer.h — the seam's specification — still count three backends after the fourth landed, including submit()'s cost, set_view_count()'s throw, the assertion total and the seam-test configuration count

**`engine/render/renderer.h:375`** · medium · `comment-drift`

```
:375-376 — "two of the three replay a vector, and the D3D11 one executes a command list per view - a protocol (record, FinishCommandList, ExecuteCommandList, Release)"
:359-362 — "on two backends it is a vector nothing will replay, on the third a deferred context holding commands submit() will not reach. All three throw it"
:420-422 — "the D3D11 back buffer is BGRA and is swapped on the way out, the GL one is asked for as RGBA and only flipped"
:524 — "pass the same assertions - 308 of them at the last count"
:528 — "IT IS TWO RUNS AND ONE SET OF IMAGES"
:557 — "tests/render/renderer_seam_tests.cpp is the part that runs in all three configurations"
```

**Why it is wrong.** renderer.h is the specification of the seam and several of
its paragraphs are the only place a rule is written down (DRIFT.md's opening).
3dda092 renumbered :40, :467, :482-489, :523 and :539 in this same file and left
these, and d31a804 added assertions to the file :524 counts without amending it
— so the header now states two different backend counts in adjacent paragraphs.
Commit a56d198 ("Correct thirty claims that stopped being true, mostly by
counting") established these as defects here.

**Failure scenario.** Each is checkable and each is now false. :375 — d3d12's
submit() also executes a command list per view, and does it in ONE
ExecuteCommandLists over an array in view order (renderer.cpp:1197-1236) using
none of the FinishCommandList/ExecuteCommandList/Release protocol; the one shape
a fourth backend actually changed is the one this sentence omits. :359-362 —
four backends throw the std::logic_error (d3d11:838, d3d12:1156, gl:639,
null:233), and on d3d12 the stranded recording is a command list, not a vector
or a deferred context. :420-422 — the d3d12 read-back is also BGRA
(DeviceResources is constructed with DXGI_FORMAT_B8G8R8A8_UNORM, backend.h:262)
and also swaps, plus a 256-byte row-pitch unpad the other two do not have;
DRIFT.md already files this same paragraph and 3dda092 made it wronger. :524 —
d31a804 added five unconditional CHECK macros to pixel_tests.cpp (:1593, :1594,
:1595, :1615, :1616) plus a REQUIRE inside Harness::resize_window, so the run is
no longer 308. :528 — three backends rasterise and the paragraph itself says so
eleven lines below ("d3d11, d3d12 and gl reproduce all forty-seven exactly");
the sibling heading two paragraphs down was rewritten from "WHAT IS STILL TWO
RUNS" to "WHAT IS STILL SEPARATE RUNS" in the same commit and this one was left.
:557 — renderer_seam_tests.cpp is an unconditional source in
tests/render/CMakeLists.txt:3-17, so it builds and runs under d3d11, d3d12, gl
and null; a maintainer using this line to decide where a device-free seam
assertion must hold under-counts by one.

**Fix.** Update all six to four backends: name d3d12's single-call
ExecuteCommandLists beside d3d11's deferred context at :375 and :359, name the
three rasterising back-buffer formats or drop the enumeration at :420, say "over
three hundred" at :524 so a new case cannot falsify a sentence whose next clause
says the number is not the point, "THREE RUNS AND ONE SET OF IMAGES" at :528,
and "all four configurations" at :557.

**Corrected in verification.** Four inaccuracies in the evidence, none fatal:

1. Two of the five cited CHECK line numbers are wrong. Actual:
   pixel_tests.cpp:1593, 1607, 1608, 1615, 1616 — not 1594 and 1595.

2. "d31a804 added … plus a REQUIRE inside Harness::resize_window" is wrong about
   provenance. That REQUIRE pre-existed (git show
   3dda092:tests/render/pixel_tests.cpp:325, now :335) and is already exercised
   by the drag-resize case at :1486. d31a804 adds a second call site (:1592), so
   it executes one more time — the runtime total still rises, by five CHECKs
   plus one REQUIRE, but the macro was not added.

3. "DRIFT.md already files this same paragraph" is loose. DRIFT.md (dated
   2026-08-19 against 57b65b3) files ":327-330 — 'This backend's buffer is
   BGRA'", i.e. a per-backend sentence on a shared seam. That complaint was
   already fixed: the current :420-422 text names d3d11 and gl separately. What
   3dda092 did was leave the newer, corrected enumeration one backend short — a
   fresh instance, not an unfixed old one.

4. The title ("six sentences … still count three backends") does not fit item
   :524, which is a stale assertion total rather than a backend count; and
   :375's sentence was already imprecise before this change, since
   renderer.h:552-555 itself says null records and never replays, so "two of the
   three replay a vector" already miscounted null. The fourth backend makes it
   wrong on count as well as on shape.

Severity medium is right and I would not lower it. It is comment-only with no
runtime effect, but renderer.h is the seam specification, the header now states
two different backend counts in adjacent paragraphs, and :557 is the one line a
maintainer would use to decide where a device-free seam assertion has to hold —
it under-counts by one configuration. The suggested fix is sound; I would add
that :375 should also say what d3d12 does differently (one ExecuteCommandLists
over an array) rather than only appending it to d3d11's clause, since that is
the one submit() shape a fourth backend actually introduced. Line-number and
mechanism corrections, none of which change the verdict:

1. The gl throw is at engine/render/gl/renderer.cpp:777, not :639. The finding
   copied :639 from DRIFT.md, which was written against 57b65b3 and is
   explicitly historical.

2. The five new CHECKs are at pixel_tests.cpp:1593, 1607, 1608, 1615, 1616 — not
   :1593-1595. Only :1593 of the first group is right.

3. "plus a REQUIRE inside Harness::resize_window" mis-describes the mechanism.
   That REQUIRE (pixel_tests.cpp:335, `REQUIRE(SetWindowPos(...) != 0)`)
   pre-existed d31a804; what the commit added is a second CALL SITE at :1592.
   The effect is the same and slightly larger than stated — six more executed
   assertions per run, not five plus a newly written one.

4. :420-422 is the weakest of the six and should be described precisely. Each of
   its two clauses is still individually TRUE — d3d11 is BGRA and swaps, gl is
   RGBA and flips. The defect is an incomplete enumeration on a sentence that
   reads as exhaustive ("Whether that costs anything is the backend's business
   and is written down in the backend: …"), which is the class DRIFT.md files,
   not a false statement. Do not say "each is now false" of this one.

5. At :524 only the number is stale. "over 30 cases" survives at 31 cases, and
   the clause "and the number is not the point" is unaffected. I could not
   independently verify that 308 was accurate immediately before d31a804
   (doctest's executed count includes loop iterations and cannot be derived
   statically here) — what is provable is that unconditionally-executed
   assertions were added to the file this line counts, in a commit that edited
   renderer.h, so 308 cannot still be correct whatever it was.

6. The "Why wrong" line-number list is approximate. 3dda092's renderer.h diff
   amends :40 ("THREE BACKENDS" → "FOUR BACKENDS"), the back_buffer_size
   paragraph, the reload_device paragraph ("two of the three" → "two of the
   four"), the translation-unit paragraph, the ":524" rasteriser count and the
   "WHAT IS STILL SEPARATE RUNS" heading — it does not renumber ":467" or ":539"
   as such. The substance of the argument (the same commit corrected adjacent
   counts and skipped these) holds.

---

### C12. Five claims written by this commit inside engine/render/d3d12/ were false on the day they landed, including a client count the sibling header records as having gone stale twice and a "one destructor" claim its own folder contradicts

**`engine/render/d3d12/backend.h:22`** · low · `comment-drift`

```
backend.h:22-24 — "Every client of it is in this folder - the four .cpp beside it". Three include it: renderer.cpp:1, render_resources.cpp:1, texture_factory.cpp:4. device_resources.cpp:1 includes only device_resources.h and throw_if_failed.h, and cannot include backend.h — backend.h includes device_resources.h (backend.h:6), so the reverse would be a cycle.

backend.h:106-108 — "Where every named resource lives. Only the first of the three tables is this backend's; fonts and sheets are engine data and are here because the storage of a pimpl is the pimpl's", contradicted six lines later at :112-114 ("The font and sheet tables are RenderResources' own members") and by render_resources.h:214-215, where fonts_ and sprite_sheets_ actually live.

texture_factory.cpp:81-83 and renderer.cpp:901-902 — "the longest of the three". wc -l: renderer.cpp 1349, texture_factory.cpp 264, render_resources.cpp 76.

render_resources.cpp:41 — "The same short list the other two backends keep". Three others do.

device_resources.cpp:49-53 — "THE ONE DESTRUCTOR IN engine/render/ THAT HAS TO DO ANYTHING". Three others: d3d12/renderer.cpp:431 (added by the same commit, in the same folder, and the one that carries the GPU wait), gl/renderer.cpp:247, gl/render_resources.cpp:25.
```

**Why it is wrong.** d3d11/backend.h:23-31 states the rule this file was copied
from and its history in capitals: "EVERY CLIENT OF IT IS IN THIS FOLDER, and
that is the rule rather than a count ... This paragraph used to name its clients
individually and the list went stale twice." The new backend reintroduces the
count the sibling had just removed for going stale, and DRIFT.md already files
the identical false-client-list defect against all three existing backend
headers. The rest are checkable statements — wc -l, a file count, a destructor
census — that were wrong when written, against files in the same diff.

**Failure scenario.** Each sends a maintainer somewhere the code is not. A
reader deciding whether device_resources.cpp may reach for Renderer::Impl is
told it already does, and one deleting or renaming backend.h expects four
compile errors and gets three. A reader deciding where a new engine-data table
goes in a fifth backend is told by :107 that engine tables live behind the pimpl
wall — the arrangement render_resources.h:35-61 records as having been undone,
because it forced measure_text and first_unrenderable to be compiled once per
backend. And a reader auditing destructor safety across the four backends —
exactly the audit the throwing-destructor finding above demands — takes
device_resources.cpp:49 at face value, stops after that file, and misses
Renderer::Impl::~Impl 380 lines away in the same backend, which shares the
defect.

**Fix.** Drop the count at backend.h:22 ("Every client of it is in this folder"
is the rule and is true), delete the contradicted second clause of :107, say
"264 lines against the D3D11 backend's 115" rather than "the longest of the
three" in both places, say "the other three backends" at
render_resources.cpp:41, and reword device_resources.cpp:49 to what is actually
singular — the only destructor in engine/render/ that has to synchronise with a
GPU — cross-referencing Renderer::Impl::~Impl, which does the other half of the
same job.

**Corrected in verification.** Sub-claim 3 does not survive and should be
dropped from the finding. "The longest of the three" is not a claim about this
backend's own files: renderer.h:479-491, rewritten by this same commit, defines
"the third file" as texture_factory.cpp compared ACROSS backends and calls
d3d12's "THE LONGEST OF THEM", with the same reason (no initial-data parameter).
Under that reading the statement is true (264 vs gl 168, d3d11 115, null 48).
The only residual defect there is the count "three" where the seam header in the
same diff enumerates four third files — a much weaker item, and not the one
reported. The reporter's suggested fix for those two sites is therefore also
wrong.

Sub-claim 2 needs re-framing: the sentence is not original to this commit but a
verbatim copy of gl/backend.h:78-80 (d3d11/backend.h:53-59 is a worse variant),
all made stale by 3febbbc four commits earlier. It is still false in the new
file, but it is copy-propagated sibling drift rather than a new invention, and
the corrective at :113 sits six lines below it.

Line-number nits: fonts_/sprite_sheets_ are render_resources.h:216-217, not
214-215; the texture_factory.cpp sentence spans 81-82, not 81-83.

The failure narrative for sub-claim 1 overstates one step: backend.h:22 does not
tell a reader that device_resources.cpp "reaches for Renderer::Impl", only that
it includes backend.h. The real cost is the one the reporter states second — a
rename or deletion of backend.h is expected to break four files and breaks
three, and the file the header names as a client is the one file in the folder
that structurally cannot be one.

Severity "low" is right and is not understated. Nothing here changes runtime
behaviour; the sharpest consequence is sub-claim 5, which misdirects exactly the
destructor-safety audit the neighbouring finding demands, and which the same
commit's own renderer.cpp:431 contradicts. The finding survives, but four of its
supporting arguments need correcting:

1. backend.h:22 — the parenthetical reason is WRONG. "device_resources.cpp ...
   cannot include backend.h — backend.h includes device_resources.h
   (backend.h:6), so the reverse would be a cycle" is not a cycle: a .cpp
   including backend.h, which includes device_resources.h, compiles fine under
   #pragma once (d3d11 has the identical structure). The real reason is that
   device_resources.cpp does not need Renderer::Impl — it is the part of the API
   that is not about drawing. Also, the em-dash phrase is ambiguous: read as a
   gloss on "this folder" it is true (four .cpp do sit beside backend.h). This
   sub-claim should be dropped or demoted; it is the one a skeptic can beat.

2. The "why wrong" narrative misstates what the sibling did. a56d198 removed the
   INDIVIDUALLY NAMED clients ("All three named their clients individually when
   the rule is the folder"), not the count. d3d11/backend.h:26 still carries a
   count — "The three .cpp beside this file include it and so does the shader
   header" — so d3d12 did not reintroduce something the sibling had removed. And
   that surviving d3d11 clause is itself false: the generated shader headers are
   fxc /Fh byte arrays (cmake/compile_shaders.cmake:14-16, /Fh /Vn) and include
   nothing, so no shader header includes backend.h. The standard the finding
   holds up is imperfectly kept in the file it cites as the model.

3. backend.h:106-108 is not new drift. It is verbatim gl/backend.h:77-79, which
   ships green today and self-contradicts six lines later at :83-85 in exactly
   the same way; d3d11/backend.h:53-59 does the same in different words.
   Pre-existing in all backends and unfiled in DRIFT.md — the finding should say
   "copied a false sentence from gl", not "written false here".

4. "the longest of the three" — the finding's evidence (this backend's three wc
   -l) is only one of two readings. The reason clause "because this API has no
   initial-data parameter" points at a cross-backend comparison of
   texture_factory.cpp, where the substance is right (d3d12 264 > gl 168 > d3d11
   115 > null 48) and only the count word is wrong (four, not three). Report
   both readings, since the sentence is false under each.

Line numbers all check out: backend.h:22, :106, :112-114;
texture_factory.cpp:82; renderer.cpp:902; render_resources.cpp:41;
device_resources.cpp:49. The finding cites texture_factory.cpp:81-83 and
renderer.cpp:901-902 — the sentences begin at :81 and :901 and are correct as
ranges.

---

### C13. sprite.hlsl sends the reader to engine/CMakeLists.txt for the b0 binding, which is not there, and compile_shaders.cmake still says the byte array goes to CreateVertexShader, which D3D12 does not have

**`engine/render/sprite.hlsl:13`** · low · `comment-drift`

```
engine/render/sprite.hlsl:13-17
// WHAT A BACKEND DOES OWN IS THE PROFILE AND THE BINDING, and both are in
// engine/CMakeLists.txt where the backend is chosen: D3D11 compiles this at
// vs_4_0_level_9_1 and ps_4_0_level_9_1 and gives the vertex shader its b0
// through a constant buffer; D3D12 compiles it at 5_1 and gives it the same b0
// as four root constants.

cmake/compile_shaders.cmake:14-16
# WHAT COMES OUT is a C header holding a byte array, which the backend includes
# and hands straight to CreateVertexShader. No file is read at run time and
# nothing has to be deployed beside the executable.
```

**Why it is wrong.** DRIFT.md treats a comment that routes a reader to the wrong
file as a defect by name (its entry for renderer.h:344-345). Both files were
written or generalised by this commit — sprite.hlsl is new here and
compile_shaders.cmake went from one client to two — so these drifted on arrival
rather than over time, in the one file whose whole argument is that it is
shared.

**Failure scenario.** engine/CMakeLists.txt:101-152 holds four compile_hlsl
calls and nothing else about the shader — the profile, the entry point, the
symbol name and the output header. No line of it names a constant buffer, a root
constant, a register or a root signature; :118-121 contradicts the claim in its
own words ("what each owns is the profile it asks for and the header the bytes
land in"). A reader who wants to check that the root signature really covers the
whole float4 at `cbuffer ViewportTransform : register(b0)` follows the pointer
to a file that cannot answer it; the binding is at d3d12/renderer.cpp:641-671
(D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS, ShaderRegister 0, Num32BitValues 4)
and in d3d11's per-view dynamic constant buffer. d3d12/backend.h:57-59 gets it
right and names renderer.cpp, create_device_dependent_resources.

Separately, compile_shaders.cmake now serves two backends and only one of them
has CreateVertexShader: d3d12/renderer.cpp:722-723 puts the bytes into a
pipeline state description (`pipeline.VS = { SPRITE_VERTEX_SHADER,
sizeof(SPRITE_VERTEX_SHADER) };`) handed to CreateGraphicsPipelineState at :779.

**Fix.** Split the sprite.hlsl sentence: the profile is in engine/CMakeLists.txt
where the backend is chosen; the binding is in each backend's renderer.cpp,
create_device_dependent_resources — which is where backend.h:57-59 already
points. In compile_shaders.cmake, say the byte array goes to whatever that API
takes a compiled shader in: CreateVertexShader/CreatePixelShader on d3d11, a
D3D12_GRAPHICS_PIPELINE_STATE_DESC field on d3d12.

**Corrected in verification.** Three corrections, none fatal to the core claim.

1. The "Why wrong" mis-states compile_shaders.cmake's history. `git log
   --oneline -- cmake/compile_shaders.cmake` returns only `091c8f4` and
   `1370fc0` — neither commit under review touched that file. It did not "go
   from one client to two" by being generalised; its text is untouched and only
   its caller count changed, in `engine/CMakeLists.txt:143-150`. So "drifted on
   arrival" is true of `sprite.hlsl:13-17` (new text, this commit) but not of
   `compile_shaders.cmake:14-16`, which is pre-existing text this commit
   falsified without amending. That is still a finding under the same rule, but
   it is the weaker of the two halves and should be described as such.

2. "CreateVertexShader" was already loose before this commit, not made loose by
   it. Even with d3d11 as the only client, the function is generic over profile
   and entry point and produces two arrays, and the pixel one goes to
   `CreatePixelShader` (`d3d11/renderer.cpp:433`), never `CreateVertexShader`.
   The change under review moves it from "names one of two consumers" to "names
   a call one of its two backends does not have", which is a degree worse rather
   than a new kind of error.

3. Minor: the finding calls this one finding at `sprite.hlsl:13`. It is two
   independent drifts in two files with different histories and different
   strengths; the second would be better filed on its own line, the way DRIFT.md
   separates its "Adjacent, same drift" items.

Severity `low` is correct and I would not raise it. There is no runtime, pixel
or build consequence — the root signature at `d3d12/renderer.cpp:641-671` does
cover the whole `float4` (Num32BitValues 4 against one `float4 pixels_to_clip`
at b0), so the thing the misled reader would go to check is in fact correct;
only the route to checking it is broken. Three corrections, none fatal.

1. Citation range off by a line: the contradicting sentence "what each owns is
   the profile it asks for and the header the bytes land in" is at
   engine/CMakeLists.txt:119-120, not :118-121. The comment paragraph is
   117-120; line 121 is the `compile_hlsl(` call itself, not comment text.

2. "compile_shaders.cmake went from one client to two ... written or generalised
   by this commit" overstates authorship. `git log --oneline --
   cmake/compile_shaders.cmake` returns 091c8f4 and 1370fc0 - neither commit
   under review touched the file. 3dda092 made its text false by adding a
   second, PSO-based client without amending it, which is still the
   amend-in-the-same-commit obligation, but this half is weaker than the finding
   implies: "hands straight to CreateVertexShader" was already loose before
   D3D12 existed, because SPRITE_PIXEL_SHADER went to CreatePixelShader and
   SPRITE_VERTEX_SHADER also to CreateInputLayout (d3d11/renderer.cpp:433, 453).
   D3D12 widens a pre-existing imprecision into an outright falsehood; it does
   not create one. It is in scope - DRIFT.md:62 logs build-file comment drift by
   precedent ("CMakeLists.txt:31 - 'ten test targets and a sample executable':
   eleven and two") - but it is not the load-bearing half.

3. The sprite.hlsl half carries the finding alone and should be reported as the
   primary defect, with compile_shaders.cmake demoted to a secondary note
   flagged as pre-existing-and-widened rather than new. The proposed fix is
   right as written, and backend.h:57-59 already models the correct wording.

---

### C14. device_resources.h's justification for signalling the fence after every execute overstates the hazard by a factor of forty-nine, and no test fails if the two signal_frame calls are deleted

**`engine/render/d3d12/device_resources.h:101`** · low · `comment-drift`

```
device_resources.h:101-110
		// ONE COUNTER, SIGNALLED AFTER EVERY EXECUTE, rather than the sample's
		// signal-only-at-present. The difference matters because a client that
		// never presents is not hypothetical: tests/render/pixel_tests.cpp
		// draws, submits and reads the buffer back without ever presenting,
		// deliberately ... Under signal-at-present that client would
		// reset a command allocator the GPU was still reading from, every
		// frame
```

**Why it is wrong.** This project treats a comment claiming something the code
does not do as a defect and keeps a live list of the class in DRIFT.md. The
claim is checkable and overstated, and the reasoning it rests on — that the
named client would be broken without the rule — is what makes it read as an
executable justification when nothing executes it. The rule itself is still
right for a client that presents; only the argument for it is wrong.

**Failure scenario.** Renderer::read_back_buffer ends with
device_resources.wait_for_gpu() (renderer.cpp:1298-1299), and wait_for_gpu is
signal_frame() + wait_for_frame() (device_resources.cpp:487-497) — a full GPU
flush. The harness calls it at the end of every frame: read_frame() is submit();
read_back_buffer(...); back_buffer_size() (pixel_tests.cpp:453-458), reached by
all 47 end() calls and both end_not_comparable() calls. So on 49 of the 49
read-back frames in that file the GPU is provably idle before the next
begin_frame resets an allocator, with or without signal-after-execute. The
hazard the paragraph describes is live at one frame boundary in the file — the
"a frame that is never submitted contributes nothing" case, which calls begin()
twice with no read between — not "every frame".

The coverage consequence is the sharper one: delete
`this->device_resources.signal_frame();` from renderer.cpp:586
(execute_frame_list) and from :1235 (submit) and the entire suite stays green on
every preset, because read_back_buffer's own flush masks it. The headline design
decision of the commit has no test that fails when it is removed.

**Fix.** Correct the sentence to name the one frame boundary in that file where
the hazard is live and note that read_back_buffer's own wait_for_gpu covers the
rest. To pin the rule, the frames-in-flight case proposed above (rank 6) would
also cover it, since it drives begin_frame/submit/end_frame with no
read_back_buffer between.

**Corrected in verification.** FOUR CORRECTIONS, three of which strengthen the
finding.

1. The reporter's concession is too generous: the hazard is live at ZERO frame
   boundaries in pixel_tests.cpp, not one. At the "a frame that is never
   submitted contributes nothing" boundary (pixel_tests.cpp:1418-1440) the
   abandoned frame reached neither ExecuteCommandLists site — a texture-change
   flush only records a draw into the still-open view list — and the last prior
   submission was the Harness constructor's texture upload
   (texture_factory.cpp:245-246) and index-buffer upload (renderer.cpp:949-954),
   each followed immediately by wait_for_gpu. So nothing is in flight at that
   second begin() either. The title's "factor of forty-nine" is therefore the
   wrong metric; the accurate statement is that the hazard is unreachable from
   this file entirely.

2. The trailing clause is unsupported too, and the finding does not mention it:
   "the only symptom would be a debug-layer message the preset that runs those
   tests does not always have" (device_resources.h:108-109). With the GPU idle
   there would be no debug-layer message at all. This matters because commit
   3dda092 reports "the D3D12 debug layer was installed and live with
   break-on-error throughout, and raised nothing" — consistent with the hazard
   never having been live, not with it being masked by a missing layer.

3. The same false sentence is in the commit message of 3dda092 ("Under the usual
   signal-at-present scheme that client would reset a command allocator the GPU
   was still reading from, every frame"). That copy is immutable;
   device_resources.h:101-109 is the only fixable one.

4. Scope is correctly narrow and the fix should stay narrow: the nearby comment
   at renderer.cpp:264-270, which is where the allocator is actually reset,
   states the rule without the false justification — "Renderer::begin_frame has
   already waited on the fence for this frame index" — and is accurate. Only
   device_resources.h:101-109 needs amending.

ONE SOFT SPOT IN THE FINDING, worth stating: "the entire suite stays green on
every preset" with the two signals deleted is a reasoned inference, not an
executed experiment (I was instructed not to build). It rests on (a) no other
test creating a d3d12 device, (b) nothing asserting on fence state, (c)
wait_for_gpu being self-sufficient. All three verified by reading. Severity
stays low: the rule the comment defends is correct, the code is safe, and no
client behaviour is wrong — this is a DRIFT-class work item plus an
admitted-adjacent coverage gap (the commit already confesses a different one,
window_size_changed, in its "Not verified" paragraph). Four corrections, none
fatal.
1. Line cite: :101 is where the block starts; the false sentence is
   device_resources.h:106-109. Quote those lines, not the heading.
2. The strongest evidence is omitted: the same header contradicts itself at
   :118-121 (wait_for_gpu "is what a load, a resize, a read-back and a shutdown
   use"), and the seam makes that binding at renderer.h:435-436 ("It stalls on
   the GPU by construction"). The refutation "but the counterfactual would not
   have flushed either" is closed off by the fact that the sample scheme the
   comment names keeps a signal-and-wait WaitForGpu. Lead with that rather than
   with the arithmetic.
3. "the entire suite stays green" overstates by the same kind of step the
   comment does. The one live boundary (pixel_tests.cpp:1426 → :1440) is a race
   — an allocator reset while a clear list may still be executing — so the
   honest claim is "no test fails deterministically", not "provably green". The
   real coverage gap is that no test presents at all (no end_frame() call exists
   anywhere in tests/ or bench/), so frame_index_ never leaves 0,
   FRAME_COUNT=2's second allocator set and fence slot are never used, and the
   presenting clients (the samples) are where the rule actually earns its keep.
4. Scope: the same overstatement is the commit's own stated rationale (3dda092,
   "THE FENCE IS SIGNALLED AFTER EVERY EXECUTE" paragraph, lines 59-61). The
   amendment should correct the header; the finding should note the commit
   message is where the claim was first made. Severity low is correct: no
   runtime defect, the rule itself is right and load-bearing for a presenting
   client.

---

### C15. MipLevels is narrowed to 16 bits while the subresource count that indexes the same array is narrowed to 32, so a malformed .dds makes the two describe different resources

**`engine/render/d3d12/texture_factory.cpp:121`** · low · `correctness`

```
texture_factory.cpp:121
		description.MipLevels = static_cast<UINT16>(texture.levels.size());

texture_factory.cpp:152-158
		const UINT subresources = static_cast<UINT>(texture.levels.size());
		...
		device->GetCopyableFootprints(&description, 0, subresources, 0,
			footprints.data(), row_counts.data(), row_bytes.data(),
			&total_bytes);
```

**Why it is wrong.** description.MipLevels and subresources are two spellings of
one quantity — texture.levels.size() — and the file relies on them agreeing: the
footprint array, the copy loop and the resource's actual subresource count are
all indexed by the same `level`. Narrowing them to different widths breaks that
invariant silently, and it fights T6, which this file quotes twice: every other
malformed-.dds case in dds_file.cpp is a named throw and this one is a dead
process.

**Failure scenario.** read_dds_file takes dwMipMapCount straight out of the
header with no upper bound (engine/render/dds_file.cpp:156, 213) and pushes one
TextureLevel per declared level; the only guard is that the file actually
contains the bytes. A .dds declaring 65537 levels therefore reaches this
function with texture.levels.size() == 65537. description.MipLevels becomes 1
while subresources stays 65537, so GetCopyableFootprints is asked for 65537
subresources of a one-subresource resource and CopyTextureRegion is then
recorded with destination.SubresourceIndex = level for indices the resource does
not have — an out-of-range subresource copy, i.e. a GPU fault or device removal
rather than a named throw. Declaring exactly 65536 is worse: MipLevels becomes
0, which D3D12 reads as "generate the full chain", so the description handed to
GetCopyableFootprints is not the description of the resource that was created.
The d3d11 sibling casts the same expression with static_cast<UINT>
(d3d11/texture_factory.cpp:89) and so fails cleanly inside CreateTexture2D.

**Fix.** Compute the level count once, reject a count that will not fit UINT16
(or exceeds the chain the dimensions allow) with a named std::runtime_error in
the same voice as the other throws in this file, and derive both
description.MipLevels and subresources from that one checked value.

**Corrected in verification.** Three corrections; the finding survives all of
them, but two of its supporting arguments do not.

1. THE "65536 IS WORSE" MECHANISM IS FLATLY WRONG. The finding says "MipLevels
   becomes 0 ... so the description handed to GetCopyableFootprints is not the
   description of the resource that was created." It is literally the same
   struct: `&description` is passed to CreateCommittedResource at line 133 and
   to GetCopyableFootprints at line 162, with no intervening write (lines
   136-148 are only the FAILED check and its throw). Both calls therefore read
   MipLevels=0 and interpret it identically as "full chain". The 65536 case is
   not distinguishable from the 65537 case in the way claimed, and it is not
   "worse" — in both cases the single real defect is that NumSubresources
   exceeds the subresource count the description implies. Drop this paragraph.

2. THE STATED OUTCOME IS OVERSTATED AND UNPROVEN. "an out-of-range subresource
   copy, i.e. a GPU fault or device removal rather than a named throw" skips the
   two things that happen first. (a) On any debug build,
   device_resources.cpp:229-236 sets
   `info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE)` and
   `SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE)`, so the invalid
   GetCopyableFootprints breaks into the debugger long before any copy is
   recorded. (b) In release, `total_bytes` is whatever the runtime writes for an
   invalid call (commonly UINT64_MAX), and that value goes straight into
   `staging_description.Width` at line 170; the very next
   CreateCommittedResource (lines 179-182) is wrapped in ThrowIfFailed, so the
   overwhelmingly likely result is a thrown com_exception, not a GPU fault.
   CopyTextureRegion at line 220-230 is only reached if that staging allocation
   somehow succeeds. The finding should say "an unnamed HRESULT throw or a debug
   break, with an out-of-range CopyTextureRegion as the residual worst case" —
   that is still a defect against T6, but it is not device removal.

3. THE D3D11 CONTRAST IS OVERSTATED. "The d3d11 sibling ... so fails cleanly
   inside CreateTexture2D" — d3d11/texture_factory.cpp:105-106 wraps
   CreateTexture2D in `ThrowIfFailed`, which throws `com_exception`, whose
   message is `"Failure with HRESULT of %08X"`
   (engine/core/throw_if_failed.h:35-37). That is an eight-digit HRESULT —
   precisely what the finding's own "Why wrong" section invokes T6 against. So
   d3d11 is not the clean-failure counterexample the finding paints; the genuine
   difference is narrower and should be stated as such: d3d11 never truncates
   the value, so its description and its subresource array can never disagree
   about what resource they describe.

4. MINOR CITATION SLIP: the second evidence block is at lines 156 and 162-164,
   not 152-158.

The defensible core, after these edits: at levels.size() >= 65536 the UINT16 and
UINT narrowings of one quantity diverge, and since texture_data.h deliberately
refuses to bound the count, this backend is the only place that can — so it
should compute the count once, reject one that will not fit UINT16 with a named
runtime_error in the voice of the two throws already in this file, and derive
both values from it. Six corrections, each independently verified:

(a) "T6, which this file quotes twice" — FALSE.
`engine/render/d3d12/texture_factory.cpp` cites T6 exactly once, at line 65
("For the throw, because an eight-digit HRESULT is not an answer (T6).").
Confirmed by `grep -o "(T6[^)]*)"`; the finding's second hit is a `UINT64`
substring match.

(b) "every other malformed-.dds case in dds_file.cpp is a named throw and this
one is a dead process" — category error. This is not a `dds_file.cpp` case at
all: the reader *accepts* a 65537-level file (`dds_file.cpp:213`, `:232`). The
unbounded mip count is shared engine code that predates and is untouched by both
commits under review, and all three level-consuming backends inherit it
(`d3d11:89`, `gl:124`).

(c) Scope. Because `D3D12_REQ_MIP_LEVELS` is 15 (`d3d12.h:1243`), every level
count in [16, 65535] already produces the named throw at
`texture_factory.cpp:143`. Only counts ≥ 65536 with a low-16-bits value that is
a valid chain length reach the mismatch. The title's "a malformed .dds" should
read "a hand-crafted .dds declaring ≥ 65536 mip levels and carrying ~256KB of
padding levels to satisfy the reader's byte check".

(d) Mechanism. `ThrowIfFailed(device->CreateCommittedResource(...))` at `:181`
stands between `GetCopyableFootprints` and the `CopyTextureRegion` loop, with
`staging_description.Width = total_bytes` taken from the out-of-range call. The
likely observed outcome is an unnamed HRESULT throw at `:181`, not the claimed
GPU fault or device removal. `GetCopyableFootprints` returns void and its retail
behaviour outside `_In_range_(0,D3D12_REQ_SUBRESOURCES-FirstSubresource)` is
unspecified, so neither outcome can be asserted as fact.

(e) Fix location. `texture_data.h:33-35` and `:51-53` put chain-length
validation in the reader, not the backend ("the backend does nothing but pass
that to whatever its API calls a texture"; the only stated invariant is "At
least one"). A guard in `dds_file.cpp` fixes it once for all four backends; the
finding's proposed guard in `d3d12/texture_factory.cpp` fixes one of three and
leaves the other two as they are.

(f) Seam citation. The finding implies a promise the seam does not make.
`resource_factory.h:74-77` scopes the named throw to format refusal ("if the
device will not take it ... a backend that cannot upload block compression"),
and `:46-50` scopes `load_texture_asset`'s named throw to a file that "will not
decode". Neither covers a level count the reader accepted. The finding survives
but must be substantially rewritten; five of its supporting arguments are wrong.

(1) TITLE AND SCOPE. "a malformed .dds" is wrong — it must read "a .dds
declaring >= 65536 mip levels and carrying the padding levels to satisfy the
reader's byte check (~262KB)". D3D12_REQ_MIP_LEVELS is 15 (d3d12.h:1243), so
every count in [16, 65535] already produces the named throw at
texture_factory.cpp:143. Reviewer B established this and it is correct.

(2) DELETE THE "65536 IS WORSE" PARAGRAPH ENTIRELY. "MipLevels becomes 0 ... so
the description handed to GetCopyableFootprints is not the description of the
resource that was created" is false. It is literally the same struct:
`&description` goes to CreateCommittedResource at :133 and to
GetCopyableFootprints at :162 with no write between (:136-148 are only the
FAILED check and its throw). Both calls read MipLevels = 0 and resolve it the
same way. The 65536 case is not distinguishable from 65537 in the way claimed.
Reviewer A's correction 1 is right.

(3) CONSEQUENCE OVERSTATED. "a GPU fault or device removal" is unproven and
probably not what is observed. In debug, device_resources.cpp:229-236 sets
SetBreakOnSeverity(CORRUPTION/ERROR, TRUE), so the invalid call breaks into the
debugger first. In retail, `total_bytes` from the out-of-range
GetCopyableFootprints feeds `staging_description.Width` at :170 and the
ThrowIfFailed'd CreateCommittedResource at :179-182 is the likely stop. Correct
wording: "an unnamed HRESULT throw or a debug break, with an out-of-range
CopyTextureRegion as the residual worst case."

(4) THE D3D11 CONTRAST IS WRONG AS WRITTEN. "fails cleanly inside
CreateTexture2D" — d3d11/texture_factory.cpp:106-107 wraps CreateTexture2D in
ThrowIfFailed, which throws com_exception carrying "Failure with HRESULT of
%08X", the exact eight-digit HRESULT the finding invokes T6 against. The genuine
difference is narrower and must be stated as such: d3d11 never truncates, so its
description and its subresource array can never disagree about what resource
they describe.

(5) THE RULE CITATIONS DO NOT HOLD. "T6, which this file quotes twice" is false
— grep shows exactly one T6 citation, at :65, about format names; the other
apparent hits are UINT64 substrings. T6's text (PHILOSOPHY.md:106-119, "Loud is
for broken contracts, not for the world being the world") says nothing about
narrowing casts, and the seam scopes its named throws elsewhere:
resource_factory.h:74-77 to format refusal, :46-50 to a file that will not
decode. This file's own standard — named throw for the artist-facing failure,
ThrowIfFailed at :181 and :188 for shouldn't-happen device failures — is not
violated by an unnamed HRESULT here. Drop the T6 argument and rest the finding
on the internal inconsistency alone, which is enough.

(6) CITATION SLIP. The second evidence block is at lines 156 and 162-164, not
152-158.

(7) FIX. The finding's fix is half right. Reviewer B is correct that
texture_data.h:33-35 ("the backend does nothing but pass that to whatever its
API calls a texture") and :51-53 ("At least one") put chain-length validation in
the reader, and that a bound in dds_file.cpp fixes it once for all four
backends. But that does not excuse the local half: derive description.MipLevels
and subresources from one checked value in this function, because it is the
truncation — not the unbounded count — that lets a value past
CreateCommittedResource's own validation.

(8) ONE OVER-REASSURANCE IN REVIEWER A'S NOTE. A says "no CPU-side buffer
overrun ... footprints/row_counts/row_bytes are all sized from subresources".
True of those three vectors, but the memcpy at :194-201 writes to `staging_bytes
+ target.Offset + row * target.Footprint.RowPitch` into a staging buffer sized
from `total_bytes`, and both come from the same out-of-range call. Their mutual
consistency is unspecified, so CPU-side safety should not be asserted either
way.

---

## Confirmed by gap probe — raised after the critics, refuted again before counting

11 findings. Three completeness critics named six probes; these are what
survived a refuter on the far side of them. Several restate a finding above from
a different angle, which is why the README merges them.

### G1. "THE ONE DESTRUCTOR IN engine/render/ THAT HAS TO DO ANYTHING" is false: a sibling destructor in the same folder, added by the same commit, does the identical wait, and two GL destructors do real work as well

**`engine/render/d3d12/device_resources.cpp:49`** · medium · `comment-drift` ·
raised by the `gap:d3d12-device-bringup` lens

```
engine/render/d3d12/device_resources.cpp:47-57
	DeviceResources::~DeviceResources()
	{
		// THE ONE DESTRUCTOR IN engine/render/ THAT HAS TO DO ANYTHING, and it
		// is the same fact this whole file exists for. Releasing a D3D11 device
		// is enough because the runtime tracks what is still in flight; here,
		// dropping a command list the GPU is still reading is exactly the bug
		// the fence exists to prevent, and a destructor is no exception.
		if (this->command_queue_ && this->fence_)
		{
			this->wait_for_gpu();
		}

Falsified by, in the same folder and the same commit (3dda092), engine/render/d3d12/renderer.cpp:431-442:
	Renderer::Impl::~Impl()
	{
		// BEFORE ANY OF THE MEMBERS BELOW device_resources GO, which is why
		// this destructor exists at all. ... the command lists, allocators and
		// vertex pages declared after it would be released while the GPU was
		// still reading them.
		this->device_resources.wait_for_gpu();
	}

and by engine/render/gl/render_resources.cpp:25-31 (`glDeleteTextures(1, &this->name_);`) and engine/render/gl/renderer.cpp:247-258 (`wglMakeCurrent` / `wglDeleteContext` / `ReleaseDC`). `grep '::~'` over engine/render/ returns exactly four destructors with bodies; three of them contradict the claim.
```

**Why it is wrong.** docs/review/backend-equivalence/DRIFT.md sets the standard:
a comment in engine/render/ is held to the same authority as the design
documents, and a claim the tree contradicts is a defect, not a wording taste.
This one was contradicted by a file in the same commit at the moment it was
written.

**Failure scenario.** The claim is categorical and scoped to engine/render/, and
the comment's own elaboration fixes that scope across backends ("Releasing a
D3D11 device is enough because..."). Under the literal reading it is falsified
three times; under the charitable reading — "the one destructor that has to wait
on the fence" — it is still falsified by engine/render/d3d12/renderer.cpp:441,
which calls the very same `device_resources.wait_for_gpu()`, sits 380 lines away
in the same folder, and shipped in the same commit. Concrete consequence: a
maintainer auditing which destructors in engine/render/ actually need to exist,
and taking device_resources.cpp:49 at its word, concludes the GL ones are
removable — deleting GlTexture::~GlTexture leaks one GL texture object per
texture release, on every `release_all_textures()` and every device-loss reload,
with no test failing (RenderPixelTests never counts GL object names). This is
the same class DRIFT.md already catalogues at gl/backend.h:22-35 — "a second
file already claims to be the one difference".

**Fix.** Narrow it to what is actually distinctive and true: this is the only
DeviceResources in engine/render/ that has to wait on a fence before releasing
anything, and the wait exists for the same reason Renderer::Impl::~Impl in this
folder waits. Drop the uniqueness claim across engine/render/, which the sibling
in d3d12/ and both GL destructors break.

---

### G2. The D3D12CreateDevice pre-check is justified by a throw that no configuration this backend is built in produces — in the only preset that builds it, the outcome the comment says the probe averts is what happens with or without the probe

**`engine/render/d3d12/device_resources.cpp:144`** · low · `comment-drift` ·
raised by the `gap:d3d12-device-bringup` lens

```
engine/render/d3d12/device_resources.cpp:141-150
				// ASKED WHETHER IT CAN MAKE A DEVICE, WHICH THE D3D11 FILE DOES
				// NOT HAVE TO. There, every adapter DXGI enumerates supports
				// D3D11 at some level; here an adapter can be present and not
				// support Direct3D 12 at all, and picking it would mean a throw
				// out of create_device rather than the WARP fallback below.
				if (SUCCEEDED(D3D12CreateDevice(candidate.Get(),
					MIN_FEATURE_LEVEL, __uuidof(ID3D12Device), nullptr)))

against the structure it describes, :187-225:
		HRESULT hr = E_FAIL;
		if (adapter) { hr = D3D12CreateDevice(...); }
	#if defined(NDEBUG)
		else { throw std::runtime_error("No Direct3D 12 hardware device found"); }
	#else
		if (FAILED(hr)) { ...EnumWarpAdapter... }
	#endif
		ThrowIfFailed(hr);
```

**Why it is wrong.** This comment is the only written justification for a
device-capability probe the reference d3d11 file does not have, and it names an
effect the code does not produce. A maintainer weighing the probe's cost is told
it prevents a throw that cannot occur here, and is not told the multi-adapter
case that is the real reason to keep it — the same comment-drift class DRIFT.md
records for d3d11/renderer.cpp:238-243, where a flag that is still right carries
a reason that is gone.

**Failure scenario.** Take the machine the comment describes — one adapter,
DXGI_ADAPTER_FLAG_SOFTWARE clear, D3D11-capable, D3D12CreateDevice fails — and
build the only preset that selects this backend, x64-debug-d3d12
(CMakePresets.json:27-33; no release preset sets LABRADOR_RENDER_BACKEND=d3d12
and ci.yml's matrix lists x64-debug-d3d12 alone), so NDEBUG is undefined. WITH
the probe: hardware_adapter returns nullptr, hr stays E_FAIL, :199 `if
(FAILED(hr))` runs, WARP device, no throw. WITHOUT the probe: hardware_adapter
returns that adapter, :190 fails, and :199 is a plain `if`, not the `else` of
`if (adapter)`, so it runs anyway — WARP device, ThrowIfFailed(:225) does not
throw. Identical outcome; the stated consequence ("a throw out of
create_device") occurs in neither case. Renderer::create_device
(renderer.cpp:1021, calling create_device_resources at :1032) is the function
named, and it returns normally on both paths. In a hypothetical NDEBUG build the
throw does happen — but there `#if defined(NDEBUG)` at :193 compiles out the
very "WARP fallback below" the sentence contrasts against, so the contrast the
comment draws holds in no configuration that can be built. What the probe
genuinely buys is unwritten: it lets the loop keep looking past a
D3D12-incapable adapter, so a machine with a capable second GPU uses it instead
of silently dropping to WARP.

**Fix.** State the effect the probe actually has: without it the first
non-software adapter wins even when it cannot make a D3D12 device, so a machine
with a capable second GPU would drop to WARP in this preset (and fail device
creation in a release build) instead of using it.

---

### G3. A resize arriving after begin_frame() but before set_view_count() is not seen as a frame in progress, so the frame's views bind and draw into a back buffer that was never transitioned to RENDER_TARGET and never cleared

**`engine/render/d3d12/renderer.cpp:1076`** · high · `resource-state` · raised
by the `gap:d3d12-frame-list-sharing` lens

```
const bool restart = impl.frame_open();
		impl.abandon_recording();

		const bool rebuilt =
			impl.device_resources.window_size_changed(width, height);

		if (rebuilt && restart)
		{
			impl.open_frame();
		}
```

**Why it is wrong.** frame_open() answers "is any command list open right now",
but backend.h:308-311 declares it as the answer to a different question - "What
Renderer::window_size_changed asks before it decides whether there is a frame to
restart." Those two are the same question on d3d11, where a bound deferred
context is the frame, but not here, because open_frame() executes and closes the
frame list before begin_frame returns. renderer.h:301-316 makes restarting a
term of the seam for the whole interval in which a frame is in progress, not for
the sub-interval in which a view happens to be recording. And this is the one
backend that carries an explicit resource-state promise
(device_resources.h:141-152: "A barrier issued from the wrong assumed state is a
debug-layer error on a good day and a corrupt frame on a bad one, so the state
is a member and every transition goes through it"); here the tracked state and
the GPU's actual use of the resource part company. The d3d11 analogue
(d3d11/renderer.cpp:715-723) uses the same view-bound predicate and has the same
hole, but that API has no state tracking and no barriers, so the identical
sequence there costs one uncleared frame rather than a validation error - which
is why this is a d3d12 defect and not a shared limitation.

**Failure scenario.** Call sequence, all public seam methods, in an order
renderer.h:281-316 declares legal ("IT MAY ARRIVE IN THE MIDDLE OF A FRAME") and
whose last step renderer.h:278-279/309-310 actively invites ("the signal the
shell wants for 're-run the layout'"): begin_frame();
window_size_changed(w2,h2); set_view_count(1); view(0).draw_sprite(...);
submit(); end_frame().

After begin_frame() returns, nothing is open. open_frame() ends with
execute_frame_list() at renderer.cpp:546, which Closes the list and sets
frame_list_open = false; begin_frame set impl.view_count = 0 at :1127, so
open_frame's loop at :548-557 opened no view, and every view->recording is false
from abandon_recording at :1121. frame_open() (:488-503 - "frame_list_open, or
any view recording") therefore answers false, restart == false, and
impl.open_frame() at :1087 is skipped.

Meanwhile DeviceResources::create_window_size_dependent_resources has set
back_buffer_states_[i] = D3D12_RESOURCE_STATE_PRESENT for every i
(device_resources.cpp:294-298) and handed out freshly created back buffers,
which are genuinely in COMMON. set_view_count (:1163-1177) issues no barrier and
no clear - it only calls View::begin, whose whole target setup is
OMSetRenderTargets at :287. submit() (:1233) then executes those lists.

Result: the GPU writes a swap-chain back buffer as a render target while the
resource is in COMMON. D3D12 implicit promotion out of COMMON for a plain
(non-simultaneous-access) texture covers only COPY_SOURCE, COPY_DEST,
NON_PIXEL_SHADER_RESOURCE and PIXEL_SHADER_RESOURCE - RENDER_TARGET is not
promotable - so this is an INVALID_SUBRESOURCE_STATE execution error. This
backend's debug device sets SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR,
TRUE) at device_resources.cpp:234-235, so under _DEBUG with the debug layer
installed - the configuration the commit says it verified under - the process
breaks; without the layer the render-target writes are undefined. The buffer is
also never cleared, so the frame draws over whatever the new buffer contains.

The new test cannot see it: Harness::begin() calls begin_frame() then
set_view_count(views) (pixel_tests.cpp:262-263), so at the resize in the case at
:1592-1593 a view is recording, restart is true and open_frame() runs. That is
why "the D3D12 debug layer was installed and live with break-on-error
throughout, and raised nothing" holds and the defect still stands. The current
Win32 shell does not order its own calls this way (Application::render at
application.cpp:292-299 calls set_view_count from StateContext::draw immediately
after begin_frame, and wait_for_frame's WaitForSingleObjectEx is not alertable),
so this is reachable through the seam rather than observed in the sample - which
is exactly the reachability the seam's own paragraph says it cannot forbid.

**Fix.** Track the frame explicitly instead of inferring it from open lists: set
a frame_begun flag in Renderer::begin_frame, clear it in end_frame, and use it
(or frame_begun || frame_open()) as `restart` at :1076. open_frame() already is
the whole restart - it re-issues the PRESENT->RENDER_TARGET barrier and the
clear, and opens only the views the frame has declared, so it is correct with
view_count still 0.

---

### G4. renderer.h's "what resets means" paragraph still counts three backends and three things to reset, while a paragraph added 250 lines below asserts it was checked and needed no rewriting

**`engine/render/renderer.h:340`** · medium · `comment-drift` · raised by the
`gap:d3d12-frame-list-sharing` lens

```
:338-344  "A FRAME BEGUN AND NEVER SUBMITTED CONTRIBUTES NOTHING TO THE NEXT ONE, which is a statement about what \"resets\" means and is worth making because the three backends have three different things to reset. Two of them hold a frame in a vector, where dropping it is clearing the vector; the D3D11 one holds it in a deferred context ..."

:592-594  "and that paragraph already said the three backends had three different things to drop, which is why it needed no rewriting to take a fourth."
```

**Why it is wrong.** CLAUDE.md and PHILOSOPHY require documents to change by
amendment in the same commit as the change that fights them, and
docs/review/backend-equivalence/DRIFT.md holds comments in engine/render/ to
that standard explicitly ("renderer.h is not a header with comments, it is the
specification of the seam"). DRIFT.md already lists this exact class as a defect
- "`:40`, `:255`, `:328` - 'two backends' / 'a second backend' throughout.
Three." A fourth backend landing without amending the counting sentence is the
same finding, aggravated by a new paragraph in the same file that asserts the
sentence was checked and found sufficient.

**Failure scenario.** There are four backends. The paragraph at :338-347
accounts for three of them (a vector twice, a deferred context once) and never
reaches D3D12, whose thing to drop - a command list that is still recording,
which cannot be reset and whose allocator cannot be reset under it - is
implemented at engine/render/d3d12/renderer.cpp:505-529. The sentence at
:592-594 is checkably false against the file it points at: the paragraph still
reads "the three backends", so it did need rewriting. Commit 3dda092 rewrote
every other count in this same file in the same pass (:40 "THREE
BACKENDS"->"FOUR BACKENDS", :430 "two of the three backends"->"two of the four",
:484 "one of the three"->"one of the four", :523 "the two backends that have a
rasteriser"->"the three backends", :539 "d3d11 and gl"->"d3d11, d3d12 and gl"),
so this is the one it skipped while asserting it had been considered. Two
residues of the same miscount: :557 "tests/render/renderer_seam_tests.cpp is the
part that runs in all three configurations" - RenderTests carries
renderer_seam_tests.cpp in every configuration
(tests/render/CMakeLists.txt:3-17), so four now; and
engine/render/d3d12/renderer.cpp:1115, written fresh by this commit, says "The
three backends have three different things to drop" and then enumerates four
("two clear a vector, the D3D11 one drains a deferred context, and this one has
a command list that is still recording").

**Fix.** Amend :338-347 to four backends and name the D3D12 case (a command list
still recording, which must be closed before its allocator can be reset), then
rewrite :592-594 to point at that instead of claiming no rewrite was needed. Fix
"all three configurations" at :557 and the same miscount at
engine/render/d3d12/renderer.cpp:1115 in the same pass.

---

### G5. open_frame_list's stated contract - that a caller can add to what begin_frame recorded - describes a call sequence that cannot occur, because begin_frame executes and closes the frame list before it returns

**`engine/render/d3d12/backend.h:327`** · low · `comment-drift` · raised by the
`gap:d3d12-frame-list-sharing` lens

```
// Opens the frame list for recording, resetting it onto this frame's
		// allocator if it is closed. Returns the same list either way, so a
		// caller that wants to add to what begin_frame recorded can.
		ID3D12GraphicsCommandList* open_frame_list();
```

**Why it is wrong.** backend.h is where this backend's contracts are written
down (its own opening paragraph). A stated contract that no call sequence can
exercise is the defect DRIFT.md records for d3d11/renderer.cpp:238-243 - the
code is right, its stated reason is not. The real reason the method returns the
same list twice is idempotence within one operation (transition + clear, or
transition + copy + transition), not extensibility across frames.

**Failure scenario.** Renderer::Impl::open_frame() - "the second half of
begin_frame" (backend.h:320-323) - ends with this->execute_frame_list() at
renderer.cpp:546, which Closes the list, calls ExecuteCommandLists and sets
frame_list_open = false. Every other user pairs open with execute inside one
call and calls nothing out in between: create_device_dependent_resources (:936
open, :950 execute), end_frame (:1134 transition -> open, :1135 execute),
read_back_buffer (:1275/:1287 open, :1296 execute), add_texture_asset
(texture_factory.cpp:218 open, :245 execute). frame_list_open is therefore
provably false at every public entry point, every later open_frame_list() takes
the Reset branch at :561-568 - which discards, not extends - and no caller can
add to what begin_frame recorded. The clause is unexercisable by any input or
call order, and "Returns the same list either way" makes it a claim about the
reset branch specifically, where what begin_frame recorded has been both
executed and thrown away.

**Fix.** Say what is true: the list is opened and Reset on demand and stays open
until execute_frame_list closes it, so the two or three calls that make up one
operation share it. Drop the begin_frame clause.

---

### G6. renderer.h justifies the new mid-frame-resize term with two message paths this tree cannot produce, and contradicts its own drag-resize paragraph 98 lines later

**`engine/render/renderer.h:294`** · low · `comment-drift` · raised by the
`gap:resize-arrival-path` lens

```
// IT IS NOT A RULE THE CALLER COULD KEEP EVEN IF THIS FILE STATED ONE.
// A resize reaches the shell as a window message, and a window message
// can be delivered while the shell is inside a frame: engine/app/
// window.cpp renders from WM_PAINT, and a vsync Present is entitled to
// pump. So "do not call this between begin_frame and submit" would be a
// prohibition on something the caller does not control, which is the
// kind of rule T6 says to make impossible rather than to document.
```

**Why it is wrong.** renderer.h is the specification of the seam, not a header
with comments, and docs/review/backend-equivalence/DRIFT.md holds its paragraphs
to the same standard as the design documents — "Each item is a claim that the
code no longer matches, with the line that contradicts it", and it lists
self-contradiction inside one file as its own defect form (":236-238, :296-304
... contradicted by :399-406 in the same file"). This paragraph is the sole
written justification for a term the seam gained in d31a804, and both of its
supporting facts are checkable against files it names by path; one is foreclosed
by the branch conditions in that file and the other sits on the wrong side of
submit(). A reader who takes the paragraph at face value will believe the shell
can deliver a resize between begin_frame() and set_view_count() — where restart
is decided by impl.frame_open() (engine/render/d3d12/renderer.cpp:1076), which
is false in that interval because open_frame() closes the frame list through
execute_frame_list() (renderer.cpp:573-586) and view_count is 0, so no view is
recording. The same shape is in d3d11 (`restart = restart || view.bound`,
d3d11/renderer.cpp:715-722). That gap is unreachable today precisely because the
paragraph's model is false; the paragraph is what would let someone believe it
had been considered.

**Failure scenario.** A maintainer porting a fifth backend reads
renderer.h:292-299 to find out how a resize actually reaches window_size_changed
between begin_frame() and submit(), and follows the two mechanisms the paragraph
names. Neither exists in this tree.

(a) "window.cpp renders from WM_PAINT": engine/app/window.cpp:344-350 renders a
frame from WM_PAINT only under `if (self && self->in_sizemove_)`.
engine/app/window.cpp:394 forwards WM_SIZE to the renderer only under `else if
(self && !self->in_sizemove_)`. The two branch conditions are complements, so in
exactly the state where the shell renders from a message, the shell throws every
resize away. renderer.h says so itself at :392-394 — "the one state where they
need not is a drag-resize, during which the shell discards every WM_SIZE and
still asks for frames (engine/app/window.cpp)". The two paragraphs in the same
file cannot both be right. (The WM_PAINT mechanism is also inverted: it puts a
frame inside a message, not a message inside a frame.)

(b) "a vsync Present is entitled to pump": the only Present in this backend is
`swap_chain_->Present(1, 0)` at engine/render/d3d12/device_resources.cpp:443,
reached only from DeviceResources::present(), whose only caller in the tree is
Renderer::end_frame at engine/render/d3d12/renderer.cpp:1136.
engine/app/application.cpp calls submit() at :296 and end_frame() at :299. So
whatever window a pumping Present opens, it opens it after submit() has already
run — never in the interval the sentence names. The same holds on d3d11
(device_resources.cpp:396). And `PeekMessage`/`DispatchMessage` at
engine/app/window.cpp:239-242 is the only message retrieval in engine/, tests/
and samples/ (grep), and it runs from the `else` arm of pump_until_quit with no
frame on the stack.

The result: the only call to window_size_changed that lands mid-frame anywhere
in this repository is the deliberate one in tests/render/pixel_tests.cpp:1593,
and the false mechanism has been copied into that test's own comment at
pixel_tests.cpp:1563-1566 ("a window message can be delivered while the shell is
inside a frame"), so the copy cannot be used to check the original. Nothing
outside these comments and the commit message asserts that Present pumps.

**Fix.** Amend renderer.h:293-299 to a mechanism this tree can produce. The
honest one is synchronous and in-stack rather than pumped:
Application::set_resolution (application.cpp:213-221) calls
Window::resize_client, whose SetWindowPos (window.cpp:268) sends WM_SIZE
straight to the window procedure on the same thread, so any caller that changes
resolution while a frame is open gets window_size_changed inside that frame with
no pump involved. If the WM_SIZE-from-a-drag argument is to stay, the shell has
to be able to deliver it — window.cpp:394 currently cannot while in_sizemove_ is
set. Either way the term itself (restart, not refuse) is sound and
pixel_tests.cpp:1559-1615 exercises it; it is the reasoning under it that needs
rewriting, and the copies at pixel_tests.cpp:1563-1566 with it.

---

### G7. wait_for_gpu() throws on a removed device, so a device removal observed on the resize path becomes an exception out of the window procedure instead of reaching the recovery branch fifteen lines below it

**`engine/render/d3d12/device_resources.cpp:292`** · high ·
`device-loss-recovery` · raised by the `gap:device-loss-meets-resize` lens

```
// Nothing may be released while the GPU is still reading it, and a back
// buffer is the thing most likely to be.
this->wait_for_gpu();

...

void DeviceResources::wait_for_gpu()
{
	if (!this->command_queue_ || !this->fence_ ||
		this->fence_event_ == nullptr)
	{
		return;
	}

	this->signal_frame();
	this->wait_for_frame();
}

void DeviceResources::signal_frame()
{
	this->fence_value_++;
	ThrowIfFailed(this->command_queue_->Signal(this->fence_.Get(),
		this->fence_value_));
```

**Why it is wrong.** The identical hardware event on the shipped d3d11 backend
recovers, and it does so because that backend deliberately keeps device-removal
HRESULTs off the throw-away path. d3d11/renderer.cpp:258-268 refuses to check
FinishCommandList's HRESULT, in a comment that states this case verbatim: "the
one way FinishCommandList fails here is a device this frame's exception was
probably reporting in the first place ... Turning that into a throw out of
begin_frame would make an abandoned frame kill the process"; and
d3d11/device_resources.cpp:232-235 does only void calls before ResizeBuffers, so
the removal is reported by ResizeBuffers and handled at :253-266. The d3d12
backend copies the first half of that argument (abandon_recording uses
std::ignore = Close() at renderer.cpp:518 and :526) and then loses it to a fence
wait the other API does not have. Microsoft's D3D12 DeviceResources - the file
device_resources.h:16-23 says this one is deliberately not a transliteration of
- guards the same Signal with SUCCEEDED and marks WaitForGpu noexcept for
precisely this reason. That leaves one shell signal and one hardware condition
answered two ways by two backends, which is the class of divergence
docs/review/backend-equivalence exists to catch, and it contradicts
renderer.h:312-315 ("the caller sees one frame's drawing lost rather than an
exception it has nowhere to catch") - the sentence d31a804 added and cited T6
for. Nothing in d3d12/backend.h or at the call site documents the limitation.

**Failure scenario.** A TDR, a driver update or an adapter reset puts the device
in the removed state, and the resize path is the first thing to touch D3D
afterwards - the ordinary case while a window is being dragged, since a TDR
takes seconds to clear and WM_EXITSIZEMOVE lands inside it. window.cpp:415 calls
on_window_size_changed, application.cpp:411 calls Renderer::window_size_changed,
the size guard at d3d12/renderer.cpp:1059 passes, impl.abandon_recording() at
:1077 destroys the frame, and DeviceResources::window_size_changed enters
create_window_size_dependent_resources. Line 292 calls wait_for_gpu(), whose
only guard (489-493) is three null checks that all pass on a removed device, so
signal_frame() runs and ID3D12CommandQueue::Signal returns
DXGI_ERROR_DEVICE_REMOVED; ThrowIfFailed (engine/core/throw_if_failed.h:43-49)
turns it into a com_exception. (If Signal succeeds, ThrowIfFailed on
SetEventOnCompletion at 482 is the second site.) ResizeBuffers at 307 never
runs, so the DXGI_ERROR_DEVICE_REMOVED / DXGI_ERROR_DEVICE_RESET branch at
310-317 - the only device-loss recovery this backend has outside Present - is
skipped in exactly the ordering it was written for. The throw unwinds out of
Renderer::window_size_changed before the restart at :1087, through
Application::on_window_size_changed, into Window::window_proc (window.cpp:327),
which has no catch, and whose only caller is DispatchMessage in pump_until_quit
(window.cpp:242), which has none either - the exception has to cross the user32
kernel-callback boundary, which is the exact condition d31a804's message and
renderer.h:315 call "nowhere to catch". Even in the best case where it reaches
wWinMain's catch (samples/linesweeper/main.cpp:49), unwinding destroys the
Renderer, and both Renderer::Impl::~Impl (d3d12/renderer.cpp:441) and
DeviceResources::~DeviceResources (device_resources.cpp:56) call the same
non-noexcept wait_for_gpu() on the same removed device from an
implicitly-noexcept destructor: std::terminate. Either way the process dies and
the swap chain is never rebuilt.

**Fix.** Make wait_for_gpu() non-throwing on device removal, as its reference
implementation is: have signal_frame() return the HRESULT (or add a
wait_for_gpu_or_removed() for the four stall paths - resize, load, read-back,
shutdown), and on DXGI_ERROR_DEVICE_REMOVED / DXGI_ERROR_DEVICE_RESET fall
through to this->handle_device_lost(); return; from
create_window_size_dependent_resources rather than throwing. That restores the
branch at 310-317 as the single recovery point it was written to be, gives the
resize path the answer d3d11 already gives, and stops the two destructors at
renderer.cpp:441 and device_resources.cpp:56 from being std::terminate sites on
a machine that has just lost its device.

---

### G8. A resize arriving after begin_frame but before the first set_view_count is not seen as a frame in progress, so the rebuilt back buffer is bound and drawn as a render target with no RENDER_TARGET barrier

**`engine/render/d3d12/renderer.cpp:1076`** · medium · `sync-correctness` ·
raised by the `gap:frame-list-five-callers` lens

```
const bool restart = impl.frame_open();
		impl.abandon_recording();

		const bool rebuilt =
			impl.device_resources.window_size_changed(width, height);

		if (rebuilt && restart)
		{
			// Cleared and reopened against the buffer that now exists, so a
			// DrawList the caller is still holding draws into this frame
			// instead of into a resource that has gone.
			impl.open_frame();
		}
```

**Why it is wrong.** backend.h:308-310 says `frame_open()` reports "whether
anything of a frame is open", but it can only observe two transient conditions -
a recording view, or the frame list in the window between a
`transition_back_buffer` and the `execute_frame_list` that always immediately
follows it. It never observes "begin_frame has run", which is the question
`window_size_changed` is asking it. renderer.h:301-310 states as a term of the
seam that the views a frame declared are reopened against the new buffer "which
is cleared as begin_frame would clear it"; on this arrival point nothing is
reopened, nothing is cleared and, worse, nothing is transitioned. Separately,
D3D12 requires an explicit ResourceBarrier into
D3D12_RESOURCE_STATE_RENDER_TARGET before a resource in COMMON is bound as an
RTV.

**Failure scenario.** Verified by reading every barrier site in the folder:
`transition_back_buffer` (renderer.cpp:589, barrier recorded at :607) has
exactly three callers - `open_frame` (:537), `end_frame` (:1134) and
`read_back_buffer` (:1275, :1295). Nothing else transitions the back buffer.

After `Renderer::begin_frame()` returns, `Impl::frame_open()` (:488) is false:
`open_frame()`'s `execute_frame_list()` at :546 has set `frame_list_open =
false`, and `begin_frame` set `view_count = 0` at :1127 before calling
`open_frame()`, so `open_frame`'s `for (int i = 0; i < this->view_count; i++)`
loop at :553-556 opened no view and none has `recording == true`. The tracked
state of the buffer is nevertheless RENDER_TARGET, and it has been cleared - a
frame is in progress by every other measure.

A client of the seam then does what renderer.h:280-283 declares legal
("CONSTRAINT: IT MAY ARRIVE IN THE MIDDLE OF A FRAME"):

renderer.begin_frame(); renderer.window_size_changed(w2, h2); // legal per the
seam, any point renderer.set_view_count(1); renderer.view(0).draw_sprite(...);
renderer.submit(); renderer.end_frame();

`restart` is false at :1076, so `impl.open_frame()` at :1087 is skipped - while
`DeviceResources::create_window_size_dependent_resources` has already written
`back_buffer_states_[i] = D3D12_RESOURCE_STATE_PRESENT` for every i
(device_resources.cpp:297), released and re-created the buffers, and re-read
`frame_index_` from the resized swap chain (:381-382). `set_view_count(1)` then
calls `View::begin` (renderer.cpp:1175), whose `OMSetRenderTargets(1,
&render_target, FALSE, nullptr)` (:287) names the new RTV, and `submit()`
(:1197) executes those lists. `end_frame()`'s
`transition_back_buffer(D3D12_RESOURCE_STATE_PRESENT)` at :1134 finds `current
== state` and emits nothing either, so the resource is never corrected.

The back buffer is therefore written as a render target while both its tracked
state and its real state are COMMON/PRESENT. D3D12 implicit promotion for a
non-simultaneous-access texture covers only the COPY and SHADER_RESOURCE states;
RENDER_TARGET is not promotable from COMMON. In debug that is a debug-layer
ERROR that halts in the debugger, because `create_device_resources` sets
`info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE)`
(device_resources.cpp:230-235); in release it is undefined render-target writes.

Reachability, stated honestly: the shell cannot land there today -
`Application::render` (application.cpp:292-299) goes `begin_frame()`,
`begin_marker()` (a no-op, renderer.cpp:1336-1339), `StateContext::draw`, and
nothing between them pumps a window message. The trigger is a direct client of
renderer.h, which tests/render/pixel_tests.cpp is, and which the seam paragraph
exists precisely to keep legal. Hence medium, not high. The same `restart`
inference exists on d3d11 (renderer.cpp:716-723) and is harmless there, because
that API has no resource-state machine to get out of step; this consequence is
d3d12's alone.

**Fix.** Either track the frame explicitly - a `frame_begun_` flag set in
`Impl::open_frame()` and cleared in `Renderer::end_frame()`, OR'd into
`frame_open()` - or drop the `restart` term entirely and call
`impl.open_frame()` whenever `rebuilt` is true. The second is one word shorter
and costs nothing: a clear and a transition between frames is redundant but
correct, since the next `begin_frame`'s `transition_back_buffer` then finds the
state already RENDER_TARGET and emits nothing.

---

### G9. The null backend does not restart a frame a resize lands in, so a pre-resize draw survives into submit() there and every sprite carries the stale viewport

**`engine/render/null/renderer.cpp:170`** · medium · `seam-divergence` · raised
by the `gap:frame-list-five-callers` lens

```
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

**Why it is wrong.** renderer.h:304-310 states two sub-terms without
qualification: "Everything recorded into every view this frame is dropped" and
"The views the frame declared are reopened against the new buffer". Null does
neither, and null/backend.h:98-105 sets the standard it is failing: a backend
that "lets a client through a rule the configuration it ships on will stop it at
... is the opposite of what this one is for. It is what CI runs completely, so
it has to be the strictest of the three and not the most permissive." Here it is
the most permissive of the four. Having no back buffer is not an exemption -
that is the identical argument gl rejected in writing for itself.

**Failure scenario.** d31a804 amended renderer.h:280-316 to make "a frame in
progress is restarted rather than refused" a term of the seam, and changed d3d11
(renderer.cpp:704-749), d3d12 (renderer.cpp:1064-1088) and gl
(renderer.cpp:679-716) to honour it - gl explicitly throwing away work it did
not have to, on the recorded argument that a caller "would see its drawing
survive on one backend and vanish on two". engine/render/null/ was not touched
by either commit and is the fourth.

A headless client written in exactly the vocabulary of
tests/render/null_tests.cpp:

renderer.begin_frame(); renderer.set_view_count(1); DrawList list =
renderer.view(0); list.draw_sprite(A, ...); CHECK(renderer.window_size_changed(w
/ 2, h / 2)); // true; drops nothing list.draw_sprite(B, ...);
renderer.submit(); recorded_sprites(renderer);

On d3d11/d3d12/gl this yields B alone - that is what
tests/render/pixel_tests.cpp:1559-1617 asserts ("the first is gone and the
second is there"). On null it yields A and B: `submit()`
(null/renderer.cpp:264-275) gathers `view.sprites` untouched and nothing between
`begin_frame` and `submit` ever clears them.

The second half is the viewport, and it is wrong for BOTH sprites, not just the
pre-resize one. `RecordedSprite::viewport = this->viewport`
(null/renderer.cpp:27), and `view->viewport` is written only in `begin_frame`
(null/renderer.cpp:199-201) and in `DrawList::set_viewport`. gl rewrites every
view's viewport to the new drawable size in `window_size_changed`
(gl/renderer.cpp:711-716) and d3d12 reopens the declared views on
`screen_viewport()` (d3d12/renderer.cpp:551-556), so sprite B records the new
size there and the old size on null.

Nothing catches it: the only test of the new term is in pixel_tests.cpp, and
RenderPixelTests is not built against the null backend at all;
renderer_seam_tests.cpp, which does run in every configuration, never calls
`window_size_changed` (grep: no match in either that file or null_tests.cpp).
Yet null is the configuration CI runs end to end, and the file whose whole
purpose is "which sprites a frame submitted, in what order, from which texture,
into which view" is the one that would report the wrong answer.

**Fix.** In null's `window_size_changed`, after recording the new size, do for
every view what `begin_frame` does: `view->reset()` and `view->viewport =
Viewport(0.0f, 0.0f, width, height)`. Leave `view_count` alone, as
renderer.h:309-310 requires. Then put the assertion where the null configuration
will run it - null_tests.cpp, or renderer_seam_tests.cpp which runs in all five
- rather than only in the one test binary that is not built against this
backend.

---

### G10. View::begin's allocator-safety comment names begin_frame's per-index fence wait as the guarantee, which the resize-restart path the same commit added does not provide

**`engine/render/d3d12/renderer.cpp:264`** · low · `comment-drift` · raised by
the `gap:frame-list-five-callers` lens

```
// THE ALLOCATOR IS RESET HERE AND NOWHERE ELSE, and Renderer::begin_frame
		// has already waited on the fence for this frame index. That sentence is
		// the whole of what this API asks of the engine that the other two do
		// not: the memory a command list records into belongs to the allocator,
		// and reusing it while the GPU is still reading last time's commands is
		// not an error anything reports - it is a frame drawn from two frames'
		// commands at once.
		ThrowIfFailed(this->allocators[static_cast<size_t>(frame)]->Reset());
```

**Why it is wrong.** docs/review/backend-equivalence/DRIFT.md is the standing
statement that a comment claiming something the code does not do is a defect
here, and CLAUDE.md and PHILOSOPHY.md require the amendment in the same commit
as the change that fights it. The sentence was true for the single caller that
existed in 3dda092 and was not amended by the commit that added the second one.
backend.h:210-215 drifted the same way in the same commit: "Deferred to
set_view_count for the reason the D3D11 backend defers its binding: that is the
first moment anybody knows which views this frame has" now describes one of two
entry points into `begin`.

**Failure scenario.** `git show d31a804 -- engine/render/d3d12/renderer.cpp`
confirms `Impl::open_frame()` is new in that commit, and its loop at
renderer.cpp:553-556 is a second caller of `View::begin`, reached from
`Renderer::window_size_changed` at :1087. On that path `Renderer::begin_frame`'s
`wait_for_frame()` (:1111) waited on `frame_fences_[frame_index_]` for the index
that was current BEFORE the resize, and
`DeviceResources::create_window_size_dependent_resources` then re-reads
`frame_index_` from the rebuilt swap chain at device_resources.cpp:381-382 -
which after `ResizeBuffers` on a FRAME_COUNT==2 flip-model chain is 0 whatever
it was. A frame that began at index 1 therefore reaches `allocators[0]->Reset()`
with no `wait_for_frame` for index 0 having run at all.

The Reset is in fact safe, and I checked why:
`create_window_size_dependent_resources` calls `wait_for_gpu()` at
device_resources.cpp:292, which is `signal_frame()` + `wait_for_frame()`
(:487-495) - a full drain, strictly stronger than a per-index wait. But that is
not the guarantee this comment names, and this comment is the only place in the
folder where the allocator-reuse rule is written down.
device_resources.h:118-121 documents `wait_for_gpu` as "NOT A FRAME-PATH CALL:
it is what a load, a resize, a read-back and a shutdown use" - not as the thing
that makes `View::begin` legal. A reader who later moves or narrows that wait
(to release the back buffers only, say) breaks the restart path with nothing in
the tree pointing at it, and the symptom is the one this comment itself names:
"a frame drawn from two frames' commands at once", with no error reported.

**Fix.** Name both entry points and both guarantees: `begin_frame`'s
`wait_for_frame()` on the ordinary path, and
`create_window_size_dependent_resources`'s `wait_for_gpu()` on the restart path
- and add a line at device_resources.cpp:290-292 recording that the wait there
is load-bearing for `View::begin`'s allocator Reset, not only for releasing the
back buffers.

---

### G11. GL's rule that reported_ is compared against the shell's number, not the window's, is defeated by Application::on_window_moved feeding it back_buffer_size() — which on GL is the live client rect

**`engine/render/gl/renderer.cpp:669`** · low · `backend-divergence` · raised by
the `gap:who-delivers-a-midframe-resize` lens

```
// SO THE ONLY THING THIS WRITES IS THE ANSWER TO ITS OWN NEXT CALL.
// Nothing that places a pixel reads these two, and they are compared
// against the shell's number rather than the window's on purpose: the
// shell suppresses every WM_SIZE for the duration of a drag and then
// forwards GetClientRect on WM_EXITSIZEMOVE, so a backend that answered
// this against the live client rect would answer "nothing changed" to
// the one message that ends a resize, where the other backend answers
// true. Two backends disagreeing about the shell's signal is the same
// class of bug as two disagreeing about a pixel.
```

**Why it is wrong.** The comment at gl/renderer.cpp:667-675 is the seam rule for
this function in its own words, and d31a804's commit message leans on it as
authority ("That file already carries the argument in the function above this
one"). The caller routes the live client rect straight back in through
back_buffer_size(), which renderer.h:391-401 documents as backend-specific for
precisely this state, so GL is compared against the window after all. It also
falsifies the premise the commit under review states for its new Direct3D guards
— "Application::on_window_moved calls window_size_changed with the size it
already has on every move of the window" — which is true only of a swap-chain
backend; on GL it is called with a size the renderer has not been told, and
d31a804's new GL restart block does per-message GPU work on that supposedly-free
path.

**Failure scenario.** x64-debug-gl, window 1280x720, user drags the LEFT edge to
900x720. WM_ENTERSIZEMOVE sets in_sizemove_ (window.cpp:404) so every WM_SIZE of
the drag is dropped (window.cpp:394) — but WM_MOVE is forwarded with no gate at
all (window.cpp:359-364), and a left/top-edge drag moves the window origin, so
DefWindowProc's modal loop sends one WM_MOVE per step. Each reaches
Application::on_window_moved (application.cpp:381-386), which sources the size
from `this->renderer_->back_buffer_size()` (application.cpp:383) — on GL that is
the LIVE client rect (gl/renderer.cpp:826-834 -> Impl::drawable_size,
gl/renderer.cpp:826/267-276 GetClientRect), not the swap-chain size the Direct3D
backends answer. So window_size_changed(900,720) runs while reported_ is still
1280x720: the early return at gl/renderer.cpp:655-658 does NOT fire, reported_
is advanced to the live size at :676-677, and the block d31a804 newly added at
:679-716 runs a glViewport/glClearColor/glClear on the default framebuffer plus
view->reset() on every view — once per mouse-move step. Then WM_EXITSIZEMOVE
forwards GetClientRect = 900x720 (window.cpp:408-418); reported_ already equals
it, so gl/renderer.cpp:655-658 returns FALSE, where d3d11 (renderer.cpp guard
against GetOutputSize) and d3d12 (renderer.cpp:1058-1062 guard against
output_size()) both find 1280x720 != 900x720, rebuild, and return TRUE. GL
therefore silently answers "nothing was rebuilt" to the one message that ends a
resize — exactly the outcome the quoted comment says must not happen — and its
answer depends on which edge the user grabbed (drag the RIGHT or BOTTOM edge, no
WM_MOVE fires, and GL returns true like the others). The same divergence is
reachable off a drag: Application::set_fullscreen -> Window::enter_fullscreen
(window.cpp:271-278) maximises, which changes the origin, so WM_MOVE fires there
too. No user-visible breakage today because both call sites discard the bool and
application.cpp:408 re-runs the layout unconditionally — the cost is the
per-message clear+reset and a seam term two backends answer differently.

**Fix.** Either gate WM_MOVE on !in_sizemove_ the way WM_SIZE already is
(window.cpp:359-364), or have Application::on_window_moved pass the shell's own
number (resolution_manager_->resolution_ivec()) rather than asking the renderer
what it is drawing into. If neither, amend gl/renderer.cpp:667-675 to record
that the shell can and does hand this function the live client rect.

---

## Refuted — raised, then disproved

5 findings. Kept because a review that discards what it ruled out cannot be
checked, and because these are the ones most likely to be raised again.

### R1. Nothing in tests/ ever calls end_frame, so frame_index_ never leaves 0 and the whole second half of the frames-in-flight ring — the reason this backend exists — is never executed in any ctest configuration

**`tests/render/pixel_tests.cpp:277`**

**Why it was refuted.** I tried to break the mechanical claim and could not.
Every link in the chain checks out by reading:

1. `grep -rn end_frame` over the whole tree returns exactly one call site
   outside the backends: `engine/app/application.cpp:299`. In `tests/` the only
   hit is the comment at `tests/render/pixel_tests.cpp:277` ("Deliberately no
   end_frame()"), so the quoted evidence and the line number are correct.
   `Harness::end()` (:281) and `Harness::end_not_comparable()` (:314) both call
   only `read_frame()`; nothing else in the harness touches the frame loop.
2. `end_frame` is the only presenting entry point on the seam.
   `engine/render/renderer.h` declares 18 public members; `Renderer::end_frame`
   (d3d12/renderer.cpp:1132-1137) is the only one that calls
   `device_resources.present()`. `Renderer::submit()` (:1197-1235) ends at
   `ExecuteCommandLists` + `signal_frame()` — no present. `read_back_buffer`
   (:1245-1326) ends at `wait_for_gpu()` — no present.
3. `frame_index_` is assigned in exactly two places (`grep -n frame_index_`):
   `device_resources.cpp:381` inside `create_window_size_dependent_resources`,
   and `:505-507` inside `move_to_next_frame()`, which is reached only from
   `present()` (:461). Both assignments read `GetCurrentBackBufferIndex()`,
   which cannot advance without a Present, so with no Present anywhere in
   `tests/` the value is 0 for the life of every `Harness`. The new
   mid-frame-resize case (pixel_tests.cpp:1559-1617) goes through
   `Renderer::window_size_changed` → `DeviceResources::window_size_changed` →
   `create_window_size_dependent_resources`, which re-reads the index after
   `ResizeBuffers` and gets 0 again.
4. No other ctest entry reaches a device on x64-debug-d3d12: `tests/app/` is
   `application_options_tests.cpp` only (no `create_device`), `bench/` has no
   Renderer client except `scene_bench.cpp:147`, which uses the null backend's
   deviceless renderer, and `LineSweeperTests` links no engine.

So `view->allocators[1]`, `pages[1]`, `Impl::frame_allocators[1]`,
`back_buffers_[1]`, `back_buffer_states_[1]` and `frame_fences_[1]` are never
touched on the frame path in any ctest configuration, and
`move_to_next_frame`/`present` never run. The mutation argument holds trivially
for the indexing: with `frame` pinned at 0, substituting a literal 0 at
renderer.cpp:171-173, :262, :271-274 and :533-535 cannot change a single
assertion or a single golden byte.

I found no refutation on the "another backend does the same and is green" axis
either — d3d11 and gl have no frames-in-flight index, so there is no known-green
precedent for this being untested.

**And independently.** MECHANISM VERIFIED, RULE REFUTED. The factual half is
correct: frame_index_ is written only at
engine/render/d3d12/device_resources.cpp:381 (creation and post-ResizeBuffers,
both 0) and :506 (move_to_next_frame, reached only from present() at :460,
reached only from Renderer::end_frame() at renderer.cpp:1132-1137), and `grep
-rn end_frame tests/` returns only the pixel_tests.cpp:277 comment. So
frame_index_ is 0 for every frame of every ctest entry. That is not in dispute.
What fails is the rule.

(1) THE RULE'S OWN WORD EXCLUDES IT. PHILOSOPHY.md:589 and CLAUDE.md:162 say "A
new public primitive ships with behavioural tests in the same commit." The
per-frame-in-flight indexing is not public: frame_index_ is a private member of
DeviceResources, exposed only by an accessor in
engine/render/d3d12/device_resources.h:134, a header
cmake/check_engine_includes.cmake fails the build for anything outside the
folder naming. The rule's neighbours in that section ("Math primitives carry
documented contracts... each pinned by behavioural tests",
PHILOSOPHY.md:500-501; "A seam ships with its headless implementation, or it has
not shipped", :583-588) fix "primitive" as an engine/seam-level API, not
backend-private synchronisation state.

(2) THE FINDING MISREADS THE CLAIM IT QUOTES. backend.h:36-41 reads in full:
"frames in flight are a number this file picks..., a command allocator may not
be reset until the GPU has finished reading it, a vertex page written this frame
may still be being read next frame, and a texture upload is a copy the CPU must
wait for. Every one of those is below the seam and none of them reached a line
of it. That is the claim, and this backend is the test of it." The antecedent of
"the claim" is the sentence immediately before it - that all four facts stayed
below the seam - not "ctest exercises the ring". The backend's existence with an
unchanged renderer.h is that test, and PHILOSOPHY.md as amended by 3dda092 says
the same thing the same way: "render/d3d12/ owns all four and render/renderer.h
did not change." No document, comment or commit message anywhere claims the ring
is exercised by ctest.

(3) THE SEAM FORBIDS THE FIX'S SHAPE IN THAT HARNESS. renderer.h:430-431:
"BETWEEN submit() AND end_frame(). Presenting discards the back buffer's
contents, so after end_frame there is nothing left to read." The harness's
no-end_frame is compliance with the seam's own stated protocol, not an
oversight.

(4) THE LIMITATION IS DOCUMENTED AT THE POINT IT HAPPENS, IN THE CODE UNDER
REVIEW. engine/render/d3d12/device_resources.cpp:322-330, at swap-chain
creation: "Everything that follows from flip is therefore unconditional: no sRGB
back buffer format, and a present that discards what it presented, which is why
tests/render/pixel_tests.cpp never calls end_frame." The review brief lists
"restating a limitation the code already documents at the point it happens" as a
non-finding.

(5) THE COMMIT MESSAGE DOES NOT MISLEAD. 3dda092 states it directly: "THE FENCE
IS SIGNALLED AFTER EVERY EXECUTE, NOT ONLY AT PRESENT, and the reason is a
client that already exists. tests/render/pixel_tests.cpp draws, submits and
reads the buffer back without ever presenting - deliberately, because presenting
a flip-model swap chain discards what it wants to read." A reader is told in the
commit body that the pixel test never presents, and told that the fence design
was chosen so that such a client is correct anyway.

(6) PRE-EXISTING AND SUITE-WIDE, NOT INTRODUCED BY THESE COMMITS. `git log
-S"Deliberately no end_frame"` puts the cited comment in 39c187b, confirmed an
ancestor of 3dda092. end_frame is called by no test on any of the four backends;
the only caller in the tree is engine/app/application.cpp:299. If "tests must
call end_frame" were a rule, d3d11, gl and null - all shipped and green - break
it identically and have since 39c187b.

(7) PROJECT PRECEDENT PUTS THIS CLASS IN A GAP LIST, NOT A DEFECT LIST.
docs/review/backend-equivalence/TEST-GAP.md opens "Nothing here is a defect" and
catalogues exactly this shape: the vertex-buffer wrap no view ever reaches (B7),
and "release_device_resources() is public seam API and nothing in tests/ calls
it on any backend". DRIFT.md, the standard the finding invokes for comment
drift, is a list of comments the code contradicts - the pixel_tests.cpp:277
comment is not one: present does discard, begin_frame does clear and rebind on
the next pass (open_frame at renderer.cpp:531-556), so the frames that follow
are correct.

(8) CORRECTNESS NEVER DEPENDED ON THE INDEX ADVANCING, AND THE GATE ITSELF IS
EXERCISED. signal_frame (:463-470) records the fence into
frame_fences_[frame_index_] after every ExecuteCommandLists, and wait_for_frame
(:472-485) waits on the value for the slot about to be reset. At index 0 that is
the strictest form of the same gate - a full sync - so the wait-before-reset
mechanism at renderer.cpp:1111 and the allocator Reset at :272 execute on every
frame of every device configuration. Only the overlap the second slot buys is
unexercised.

(9) THE HEADLINE OVERSTATES. backend.h:26-47 gives two reasons the backend
exists; the second, "A SECOND RASTERISER CI CAN RUN", is exercised by every push
(x64-debug-d3d12 runs RenderPixelTests against WARP with no -E). And the
presenting half is verified by the method this project uses for it, recorded in
both Verified paragraphs: 3dda092 "LineSweeper draws correctly in a real
window"; d31a804 "the drag, the end of the drag and five consecutive resizes, on
each of the three backends... where the d3d12 sample used to crash roughly one
run in six" - a symptom only reachable with frame_index_ advancing.

The mutation argument (replace frame with literal 0 and the suite stays green)
is mechanically sound, and I checked it: with the mutation, frame N at
swap-chain index 0 would wait on frame_fences_[0] signalled two frames back
while resetting the allocator used in frame N-1, whose signal sits in
frame_fences_[1]. But mutation survival is not a standard this project has
adopted anywhere in CLAUDE.md, PHILOSOPHY.md, ARCHITECTURE.md or CONVENTIONS.md,
and TEST-GAP.md is the project's explicit statement that untested
backend-internal paths are gaps to propose against, not defects to report.

**And independently.** REFUTED. Reviewer B is right; Reviewer A verified the
mechanism, conceded both of the finding's load-bearing justifications, and then
returned refuted=false on the mechanism alone.

MECHANISM CONFIRMED (not in dispute). frame_index_ is assigned only at
device_resources.cpp:381 (creation and post-ResizeBuffers, both
GetCurrentBackBufferIndex() = 0) and :506 in move_to_next_frame(), reached only
from present() (:461) <- Renderer::end_frame() (renderer.cpp:1132-1136). `grep
-rn end_frame` gives one caller in the tree, engine/app/application.cpp:299; in
tests/ only the pixel_tests.cpp:277 comment. So frame_index_ is 0 for every
frame of every ctest entry on x64-debug-d3d12.

THE CHECK THAT DECIDES IT, WHICH NEITHER REVIEWER DID: the untested ring is
correct. I traced the fence sequence. Frame A: begin_frame -> wait_for_frame on
frame_fences_[0]; open_frame resets frame_allocators[0], clears,
execute_frame_list -> signal_frame writes fences[0]; View::begin resets
allocators[0]; submit -> ExecuteCommandLists + signal_frame -> fences[0];
end_frame -> execute_frame_list (fences[0] again) -> present -> frame_index_=1.
Frame C returns to index 0 and wait_for_frame targets fences[0], holding frame
A's FINAL signal, covering every list frame A executed, before
allocators[0]/pages[0] are reset. Two-frames-in-flight gating is right.
page/page_position reset at renderer.cpp:295/:318; pages[frame] is per-index and
only grows. The gap conceals no bug.

WHERE REVIEWER A FAILED. He conceded correction 3 ("The CLAUDE.md rule cited
does not apply") and correction 4 ("unfair to the commit message") - the
finding's entire normative basis - then let the verdict ride on the mechanism.
Mechanism plus no violated rule is a coverage gap, and
docs/review/backend-equivalence/TEST-GAP.md is the project's explicit statement
on those: it opens "Nothing here is a defect" and catalogues this exact shape
(B7, the vertex-buffer wrap no view reaches; and "release_device_resources() is
public seam API and nothing in tests/ calls it on any backend" - public seam
API, still filed as a gap, not a defect).

BOTH CONCESSIONS CHECK OUT. PHILOSOPHY.md:589 sits among seam-level statements
(":583-588", "A seam ships with its headless implementation, or it has not
shipped"); "public" is load-bearing and frame_index_ is private to a folder
check_engine_includes.cmake fails the build for anything outside naming.
backend.h:39-41's "That is the claim" attaches to the preceding sentence, "Every
one of those is below the seam and none of them reached a line of it" - which
3dda092 spells out as "renderer.h's claim that a platform is an addition". No
document claims ctest exercises the ring.

THREE FURTHER NAILS. (a) 3dda092's body discloses the fact - "pixel_tests.cpp
draws, submits and reads the buffer back without ever presenting - deliberately"
- and says the signal-after-every-execute design exists because of that client,
refuting "a reader is left believing the ring is covered". (b)
device_resources.cpp:322-330, inside the code under review, documents it at the
point it happens: "which is why tests/render/pixel_tests.cpp never calls
end_frame" - the brief's listed non-finding. (c) `git log -S"Deliberately no
end_frame"` puts pixel_tests.cpp:277 in 39c187b, which `git merge-base
--is-ancestor 39c187b 3dda092` confirms is an ancestor: wrong commit,
pre-existing, and no test on any of the four backends has ever called end_frame.

ERRORS IN THE FINDING'S EVIDENCE BEYOND WHAT A CAUGHT. "wait_for_frame
degenerates to waiting on the index it just signalled" inverts the mechanism:
pinned at 0, begin_frame's wait targets the PREVIOUS frame's signal - a full
sync, the strictest form of the gate. The gate at renderer.cpp:1111 and the
allocator Reset at :272 therefore run on every frame of every device
configuration; only the overlap the second slot buys is unexercised. "Never
written or read" is false for five of six named members
(frame_fences_[1]/back_buffer_states_[1] at :40-44, reset :294-298;
back_buffers_[1] at :371-379; both allocator arrays at renderer.cpp:846-851 and
:872-877) - only pages[1] is genuinely never populated. The proposed fix would
Present(1,0) on a window created and never shown (pixel_tests.cpp:112-116),
taking vsync throttling and DXGI_STATUS_OCCLUDED into CI.

WHAT SURVIVES: frame_index_ visits only 0 in every ctest configuration;
move_to_next_frame, present and the second allocator/page set are exercised only
by a real window - which both commits record under Verified ("LineSweeper draws
correctly in a real window"; "five consecutive resizes... where the d3d12 sample
used to crash roughly one run in six", a symptom only reachable with the index
advancing). That is a low-severity TEST-GAP.md entry phrased as a gap, not a
medium defect at pixel_tests.cpp:277.

---

### R2. transition_back_buffer commits the tracked back-buffer state when the barrier is recorded; abandon_recording discards that barrier without rolling the tracker back

**`engine/render/d3d12/renderer.cpp:608`**

**Why it was refuted.** The quoted evidence and every line number check out
(renderer.cpp:607-608 within the stated 606-609; abandon_recording 524-528;
Close at 580; device_resources.h:144-153; renderer.h:344-347). The mechanism
described — the tracker commits at record time and abandon_recording does not
roll it back — is a true statement about the code's shape. But the failure
scenario is not reachable from any entry point in renderer.h, for five
independent reasons.

1. THE SEAM SENTENCE THE FINDING LEANS ON DOES NOT PRODUCE THE DESYNC.
   renderer.h:344-347 blesses a throw "out of its own draw walk", i.e. between
   begin_frame and submit/end_frame. On that path the tracker is exactly right,
   because open_frame records the PRESENT->RENDER_TARGET barrier at :537 and
   SUBMITS IT IMMEDIATELY at :546 ("this->execute_frame_list();") before
   returning. begin_frame therefore leaves frame_list_open == false and the
   barrier already on the queue. When the next begin_frame calls
   abandon_recording, the `if (this->frame_list_open)` block at :524-528 does
   not execute at all; abandon_recording drops only the per-view lists, and grep
   for ResourceBarrier shows the only back-buffer barrier in the whole backend
   is :607 on the frame list (:948 is the index buffer, texture_factory.cpp:243
   is a texture). Case A's stated route is a misreading of the header it cites.

2. EVERY OTHER TRANSITION SITE IS NET-ZERO ACROSS AN ABANDON. read_back_buffer
   (:1274-1296) goes RENDER_TARGET -> COPY_SOURCE -> `previous` before
   executing, so a discarded list there leaves the tracker where it started.
   create_device_dependent_resources (:936-950) and texture_factory.cpp:218-245
   record no back-buffer barrier. Only open_frame and end_frame can leave the
   tracker ahead of reality.

3. AT THOSE TWO SITES THE SOLE THROW BETWEEN BARRIER AND EXECUTE IS
   ThrowIfFailed(Close()) AT :580. Nothing else qualifies. open_frame_list's
   ThrowIfFailed(frame_list->Reset(...)) at :564-566 is evaluated before
   ResourceBarrier and before set_back_buffer_state — the call order at :607-608
   guarantees that a failing Reset records no barrier AND advances no tracker,
   which is consistent. frame_allocators[frame]->Reset() at :534-535 is before
   the transition. back_buffer_view, ClearRenderTargetView and
   ExecuteCommandLists cannot throw. signal_frame's ThrowIfFailed(Signal(...)) —
   the one HRESULT that genuinely does return DXGI_ERROR_DEVICE_REMOVED in
   practice — is at :586, AFTER `frame_list_open = false` (:581) and after
   ExecuteCommandLists, so the barrier is already submitted and
   abandon_recording has nothing to discard.

4. SO THE TRIGGER IS "Close() fails on a list holding one valid barrier
   (end_frame) or one valid barrier plus one ClearRenderTargetView
   (open_frame)". Close returns an error when the runtime recorded an invalid
   API call, or on removal/OOM. Under correct operation those two lists contain
   nothing invalid, so the trigger needs an independent pre-existing defect or a
   driver failure — not a call sequence any client can produce.

5. ON THE ONE HARDWARE CONDITION THE FINDING NAMES, CASE B DOES NOT FOLLOW. If
   the device is removed, the very next begin_frame throws at :534-535
   (ID3D12CommandAllocator::Reset returns DXGI_ERROR_DEVICE_REMOVED) before it
   ever reaches transition_back_buffer, so no frame is drawn against the stale
   tracker. If removal instead surfaces at present()
   (device_resources.cpp:445-455) or inside ResizeBuffers (:310-316),
   handle_device_lost sets back_buffer_states_[i] = PRESENT for all i (:418-420)
   and resyncs. And the one path where abandon_recording IS routinely reached
   with a live frame — Renderer::window_size_changed at :1077 — is provably
   safe: create_window_size_dependent_resources resets back_buffer_states_[i] =
   PRESENT at device_resources.cpp:295-298 before open_frame re-transitions, and
   `rebuilt` is always true there because Renderer::window_size_changed already
   early-returned on an unchanged size with an equivalent comparison.

The comment-drift half of the argument also fails. device_resources.h:151-153
claims "every transition goes through it" — and every transition does go through
transition_back_buffer; the header makes no rollback promise.
abandon_recording's comment at :507-513 ("What it holds goes nowhere: on the
begin_frame path it belongs to a frame nobody submitted, and on the resize path
it names a back buffer that is about to stop existing") is true as written on
both named paths. Neither is a DRIFT.md-class defect.

Residual, stated honestly: transition_back_buffer is not exception-safe with
respect to a discarded list, and the fix the reporter proposes (advance the
tracker in execute_frame_list rather than at record time) would be strictly more
robust. But that is defensive hardening against a failure with no demonstrated
trigger, which is the shape T1 rules out, not a defect with a reachable failure
scenario.

**And independently.** The D3D12 semantics the finding leans on are correct (a
mismatched Transition.StateBefore is a real API violation, not merely a
validation warning, and RENDER_TARGET is not among the states a
non-simultaneous-access texture is implicitly promoted to from COMMON/PRESENT).
What fails is the CONTRACT: the rule it invokes does not exist as stated, and
the trigger is not reachable by any input, call sequence, or ordinary hardware
condition in this tree.

1) renderer.h:338-347 does not promise what the finding quotes it as promising.
The sentence is "A client reaches this by catching an exception out of its own
DRAW WALK and carrying on" — client code between begin_frame and submit. In that
window the d3d12 frame list is closed: open_frame (renderer.cpp:537-546) records
the RENDER_TARGET barrier and immediately calls execute_frame_list, so
frame_list_open is false for the whole draw walk and abandon_recording's `if
(this->frame_list_open)` branch (renderer.cpp:524) is never entered. Tracker and
resource agree throughout. That exact flow is already pinned by a test on the
device: tests/render/pixel_tests.cpp:1418 "CONTRACT: a frame that is never
submitted contributes nothing" (begin, sprite then text to force a flush, no
end, begin again, end) and tests/render/null_tests.cpp:425 — and d31a804's
message records all five presets green.

2) The tree rules out the "client catches and carries on" premise in its own
words. pixel_tests.cpp:1433-1437: "A client reaches this state by letting an
exception out of its draw walk and carrying on; both samples exit instead, so
nothing in the tree reaches it". `grep -rn "catch" engine/` yields only
assets/resource_loader.cpp:152 and core/thread_pool.cpp:90, neither in the frame
path; engine/app has no catch at all. Both samples catch at main and exit —
samples/minimal/main.cpp:48-55, "A broken contract stops the program dead with
the reason on screen, never a silent abort (PHILOSOPHY T6)". d31a804's own
message says the same of the sibling backend: "The throw d3d11 produced went
into a window procedure, where the shell has nowhere to catch it."

3) device_resources.h:144-153 says "the state is a member and every transition
goes through it" — which the code does literally: transition_back_buffer is the
only writer and the only barrier site for that resource (the only other
ResourceBarrier calls, renderer.cpp:948 and texture_factory.cpp:243, are on the
index buffer and on textures). The stronger promise the finding attributes to it
("advances when the barrier reaches the GPU, or is rolled back when it does
not") is the finding's paraphrase, not the comment's claim, so this is not
comment drift by the DRIFT.md standard.

4) The trigger reduces to exactly one statement:
ThrowIfFailed(this->frame_list->Close()) at renderer.cpp:580. Every other
statement between the ResourceBarrier (607) and ExecuteCommandLists (584) is
void-returning — set_back_buffer_state, ClearRenderTargetView, CopyTextureRegion
— and open_frame_list()'s Reset runs before the barrier is recorded, so a throw
there leaves the tracker untouched. Close fails only on a recording error
(E_OUTOFMEMORY) or device removal, both terminal here; and if execution did
continue, present() (device_resources.cpp:445-456) takes the device-lost path
and handle_device_lost resets every back_buffer_states_ entry to PRESENT
(:418-421), as does create_window_size_dependent_resources (:295-298).

5) abandon_recording's other call site, the resize path (renderer.cpp:1077), can
never desync, which the finding does not claim but which removes the only
non-exceptional route: window_size_changed rebuilds through
create_window_size_dependent_resources, which resets all FRAME_COUNT state
entries to PRESENT before open_frame runs again.

6) read_back_buffer (renderer.cpp:1273-1296) is already failure-consistent: it
saves `previous`, transitions to COPY_SOURCE, copies, transitions back to
`previous`, then executes — so a list dropped by a failed Close leaves the
tracker at `previous`, which is exactly where the resource still is.

7) No cross-backend comparison applies: d3d11, gl and null have no
resource-state model at all, so there is no shipped-green backend doing the same
thing — and equally no seam rule the d3d12 one is departing from.

---

### R3. backend.h claims a texture's slot number survives a heap recreation and nothing enforces the precondition that makes resetting the allocator to zero safe

**`engine/render/d3d12/backend.h:74`**

**Why it was refuted.** Quotes and line numbers all check out (backend.h:74-80,
:296-297; renderer.cpp:808-812, :990-994; pixel_tests.cpp:224-226 sets resources
but no notify). The finding still fails on both halves.

HALF (a) — "no slot survives anything" — refuted by the cross-reference the
comment itself makes. registry.h:39-44 states the criterion exactly: "the
pointer changes across a device restore and the slot does not", and :134-137
"Empties every slot and keeps every name, so handles survive the round trip
through a device loss." A d3d12 descriptor slot meets that criterion literally
in the shipped configuration: ResourceLoader::reload_device_resources walks
`manifest_.entries` in manifest order
(engine/assets/resource_loader.cpp:96-107), next_texture_slot restarts at 0
(renderer.cpp:812), so a texture gets back the same descriptor slot it held
before the loss, reached through a TextureHandle the registry kept valid. A
stored D3D12_GPU_DESCRIPTOR_HANDLE would not survive, because texture_heap is a
fresh heap from a fresh device (renderer.cpp:790-791). Every operative statement
in the paragraph is checkable and true: texture_slot_gpu reads
GetGPUDescriptorHandleForHeapStart() of the *current* heap
(renderer.cpp:479-486) and is called inside flush (renderer.cpp:235-236), so
"resolved at the draw" is literal; and a device loss does remake the heap
(renderer.cpp:990 then 790). d3d12/render_resources.cpp:46-53 does not
contradict the paragraph — it tells the same story from the table's side (names
kept, slots retaken from zero by the reload).

HALF (b) — the no-notify hazard — is real as an abstract contract gap but is not
this backend's, is not reachable from a shipped client, and is identical in the
known-green reference backend.
  1. The only shell is Application, which IS a DeviceNotify (application.h:115)
     and installs itself in its own constructor, before the device exists
     (application.cpp:51-60: `this->renderer_->set_device_notify(this);`), and
     unconditionally calls release_device_resources on loss
     (application.cpp:414-426). A texture can only reach the table through
     ResourceLoader, which Application owns — so there is no shipped
     configuration that has textures loaded and no notify.
  2. The one no-notify client is the pixel-test Harness, and it cannot reach the
     branch: it would need a real TDR/adapter removal mid-Present on a
     hidden-window WARP or hardware device, in a test that draws a handful of
     quads. If that ever happened the test fails, which is the correct outcome.
  3. d3d11 does exactly the same thing and ships green:
     Renderer::Impl::OnDeviceLost (d3d11/renderer.cpp:609-639) releases only
     what the renderer owns and forwards under the identical `if (this->notify
     != nullptr)` guard at :635-638; it never empties the texture table either.
     Without a notify its table keeps ID3D11ShaderResourceViews created on the
     destroyed device and binds them on the new device's context — a silently
     wrong frame from the same contract violation. The hazard belongs to the
     seam's DeviceNotify contract, not to the slot field, and renderer.h:463-475
     already books device loss as an open seam-level question ("STILL OPEN: …
     whether that belongs on the loader, on DeviceNotify, or nowhere").

THE PROPOSED FIX IS ALSO WRONG ON ITS OWN TERMS. Renderer::Impl holds `const
RenderResources* resources = nullptr;` (backend.h:296) — a const borrow, from
which the non-const release_device_resources() cannot be called — and
render_resources.h:105-109 argues explicitly that the call sits on the seam with
the shell as its one caller, "because its signature names nothing a backend
owns"; making Renderer tear the table down would fight that and put a
responsibility in d3d12 that d3d11 and gl leave with the shell.

Measured against DRIFT.md, whose every entry is a checkable count, filename or
mechanism mismatch (three TUs vs four, "thirty lines" vs 101, `backend.h` by
name vs the folder), this paragraph has no such mismatch to point at.

**And independently.** CONTRACT lens: neither rule the finding invokes supports
it.

(1) DRIFT.md's standard is real but not met.
docs/review/backend-equivalence/DRIFT.md:13-16 defines an item as "a claim that
the code no longer matches, with the line that contradicts it", and every entry
is a checkable factual mismatch (TU counts, line counts, dead file paths, "two
backends" vs three). The disputed paragraph's two concrete assertions are both
true of the code: a GPU descriptor handle is heap-relative, and the slot is
resolved at the draw — renderer.cpp:479-485 texture_slot_gpu adds
slot*texture_descriptor_size to GetGPUDescriptorHandleForHeapStart() at flush,
exactly as claimed. The clause the finding attacks, "it survives exactly as the
engine's own handles do", names its own definition: engine/core/registry.h:39-45
says "Releasing a resource - what a device loss does to textures and fonts -
empties the slot but keeps the mapping... the pointer changes across a device
restore and the slot does not", and registry.h:135-140 release_all "Empties
every slot and keeps every name, so handles survive the round trip through a
device loss." So the very sense of "survives" the comment borrows IS
emptied-and-refilled-from-zero. d3d12/render_resources.cpp:46-53 states the same
lifecycle ("the slots... are not returned one at a time, because the heap itself
is a device resource and Renderer::Impl remakes it whole... which resets the
allocator to zero for the reload to take again") and is consistent with, not
contradictory to, backend.h:74-80. The finding asserts a contradiction it does
not demonstrate.

(2) The T6 the finding cites does not exist as stated. PHILOSOPHY.md:106-119 T6
is "Loud failure over graceful degradation" — immediate errors with names and
paths, not "make caller obligations impossible rather than write them down".
grep for impossible/obligation/unrepresentable/precondition across PHILOSOPHY.md
returns only :158 and :466, neither relevant. The nearest in-tree extension,
renderer.h:297-299, is explicitly scoped to "a prohibition on something the
caller does not control"; installing a DeviceNotify is something the caller does
control, and render_resources.h:100-110 deliberately assigns
release_device_resources to the shell ("its one caller is the shell - which
learns from DeviceNotify that a device it has never heard of has gone away.
Making the shell say which *kinds* of resource a loss takes would be the seam
telling it what a texture is"). Placing the obligation on the client is the
argued seam design, not an unstated precondition.

(3) The no-notify hazard is seam-wide and pre-existing, not introduced by this
backend. d3d11/renderer.cpp:609-639 OnDeviceLost has the identical "if
(this->notify != nullptr)" guard and likewise never empties the texture table; a
no-notify d3d11 client after a device removal is left binding
ID3D11ShaderResourceViews from a dead device. gl/backend.h:143-145 and null
store notify the same way. So a shipped, green backend behaves identically at
the level the finding calls unenforced.

(4) The proposed fix is barred by the seam. renderer.h:333 is
set_resources(const RenderResources*), stored as "const RenderResources*
resources = nullptr" (d3d12/backend.h:296, mirrored at d3d11/backend.h:250),
while RenderResources::release_device_resources() is non-const —
Renderer::Impl::on_device_lost structurally cannot empty the table without
widening the seam to a mutable resource pointer, which would cut against draw()
being const all the way down.

(5) In-tree reachability is nil and correctly ordered.
engine/app/application.cpp:59 installs the shell as notify unconditionally in
the Application constructor, and :414-425 on_device_lost calls
release_device_resources(). DeviceResources::handle_device_lost
(d3d12/device_resources.cpp:410-436) calls notify_->on_device_lost() first, then
rebuilds, then on_device_restored -> create_device_dependent_resources ->
notify->on_device_restored -> reload; the allocator reset at renderer.cpp:812
therefore always follows the table being emptied. pixel_tests.cpp never sets a
notify but also never induces a device removal (no match for notify/device_lost
anywhere in tests/render/), so nothing is unpinned that was ever pinned.

What survives is at most a rigour quibble: on the only heap-remake path the
textures are destroyed anyway, so the counterfactual the rationale rules out (a
stale stored GPU handle) never bites in a shipped flow, and the analogy to
registry is imperfect in one direction registry guarantees by construction
(name->index) while d3d12 gets by reload order. That is a weaker-than-stated
rationale, not a claim the code fails to honour, and DRIFT.md's list contains
nothing of that kind.

---

### R4. The new size guard makes gl restart the frame on calls that are no-ops on both Direct3D backends, because gl compares a different number from the one the shell feeds it

**`engine/render/gl/renderer.cpp:655`**

**Why it was refuted.** The finding's mechanism checks out line for line, but
its failure scenario does not, and every consequence it claims is either
unreachable or invisible.

WHAT I CONFIRMED (all citations accurate). gl/renderer.cpp:653-659 compares
`impl_->reported_width/height`; gl/renderer.cpp:827-835 `back_buffer_size()`
returns `Impl::drawable_size()` (GetClientRect, :267-276) — a different
quantity. d3d11/renderer.cpp:697-702 and d3d12/renderer.cpp:1058-1063 compare
`GetOutputSize()`/`output_size()`, which is exactly what their
`back_buffer_size()` returns (d3d11:914-919), so `Application::on_window_moved`
(application.cpp:380-386) is a tautological self-comparison there and always
returns false. null/renderer.cpp:170-179 + :277-281 is the same self-comparison,
so gl is the odd one out of four. window.cpp:359-364 dispatches WM_MOVE
unconditionally; :394 suppresses WM_SIZE while `in_sizemove_`. So yes: dragging
the left or top edge does run gl's new body (703-718) per mouse step where the
Direct3D backends do nothing.

WHY IT IS NOT A DEFECT. (1) The claimed harm — "gl drops that frame's drawing" —
needs a WM_MOVE delivered between `begin_frame` and `submit`. There is exactly
one message pump in the entire engine, window.cpp:239 `PeekMessage`, and it
calls `tick()` only in the else-branch when the queue is empty;
`Application::render()` (application.cpp:280-300, the only span in which a frame
is open) contains no message-retrieval call, and `grep` for
PeekMessage/GetMessage/DispatchMessage/MsgWaitForMultipleObjects over all of
engine/ and samples/ returns nothing outside window.cpp. WM_MOVE is a *sent*,
not posted, message: USER32 delivers it inline from within SetWindowPos on the
calling thread. Every SetWindowPos in the engine (window.cpp:268, 276, 292)
carries SWP_NOMOVE and is called from `set_resolution`/`set_fullscreen` during
`update()`; the drag's SetWindowPos runs inside DefWindowProc's modal loop,
which is where WM_PAINT→`tick()` is dispatched *from*, never nested inside it.
So no WM_MOVE can arrive while gl has recorded-but-unsubmitted vertices. The
commit's own pump vector — "a vsync Present is entitled to pump" — delivers
queued messages, and this WM_MOVE is never queued. (2) In the only reachable
case (between frames) gl's body is a strict subset of `begin_frame`
(gl/renderer.cpp:733-751 does the identical glViewport + glClear +
`view->reset()` + full-drawable viewport stamp, plus `view_count = 0`). Anything
it does is redone at the top of the very next WM_PAINT tick, and the glClear
lands on a back buffer that is cleared again before it is ever presented.
Nothing that places a pixel reads `reported_*`. (3) The differing `true` return
has no consumer: both shell call sites discard it (`std::ignore`,
application.cpp:384 and :411), and `resolution_manager_->set_resolution_exactly`
at :409 runs unconditionally ahead of it. Only pixel_tests.cpp:1593 CHECKs the
value, on a path with no WM_MOVE. So the residual cost is one glClear and N
vector clears per mouse step, during a drag in which gl is already repainting
the whole window every step. That is wasted work, not a crash, wrong pixel,
race, hang or silent no-op.

**And independently.** REFUTED on two independent grounds: the failure is
unreachable, and the rule it invokes does not exist as stated. The mechanism it
describes is real, so this is a correct observation attached to a consequence
that cannot happen.

WHAT CHECKS OUT. Every factual premise about the comparands is true.
`engine/render/gl/renderer.cpp:655-656` compares `reported_width/height`;
`back_buffer_size()` at `gl/renderer.cpp:826-834` returns
`Impl::drawable_size()`, a live `GetClientRect` (`gl/renderer.cpp:267-276`).
Both Direct3D guards compare the exact quantity their own `back_buffer_size()`
returns — `d3d11/renderer.cpp:697` vs `:914-919`, `d3d12/renderer.cpp:1058` vs
`:1238-1243`, both `output_size()` — so `Application::on_window_moved`
(`application.cpp:381-386`) is a guaranteed self-comparison and returns false on
D3D. `window.cpp:359-364` dispatches WM_MOVE ungated; `window.cpp:392-396`
suppresses WM_SIZE while `in_sizemove_`. So on a left/top-edge drag the gl guard
does fall through where D3D's does not. That much is verified.

GROUND 1 — THE FAILURE IS UNREACHABLE. The finding's harm is "gl drops that
frame's drawing and the two Direct3D backends keep it", which requires a WM_MOVE
delivered between `begin_frame` and `submit`. It cannot be.
`Application::render()` (`application.cpp:280-300`) is straight-line
`begin_frame()` → `StateContext::draw` → `submit()` → `end_frame()`;
`StateContext::draw` (`state_context.cpp:29-49`) is a synchronous walk with no
pump and no fan-out; the only `PeekMessage`/`DispatchMessage` in the tree is
`window.cpp:239-242`, entirely outside `tick()`. The one pump the commit itself
names — "a vsync Present is entitled to pump" — lives in `end_frame`, i.e. after
`submit()`, and on gl `end_frame` is `SwapBuffers` (`gl/renderer.cpp:757`),
which does not dispatch queued messages at all. So on the very backend accused,
the restart always lands between `end_frame` and the next `begin_frame`, where
it is idempotent: `window_size_changed`'s new body (`gl/renderer.cpp:700-718`)
is the same `glViewport`/`glClearColor`/`glClear`/per-view `reset()` sequence as
`begin_frame` (`gl/renderer.cpp:735-753`), minus the `view_count = 0` the seam
deliberately excludes, and the buffer it clears is a back buffer the next
`begin_frame` clears again before anything is presented. Net observable cost of
the divergence: one redundant `glClear` per WM_MOVE and a `true` return that
`on_window_moved` discards with `std::ignore`. No pixel, no dropped frame.

GROUND 2 — THE RULE DOES NOT EXIST. The finding concedes its own premise:
"renderer.h:281-315 ... says nothing about when one happens". I confirmed it —
the CONSTRAINT block specifies only what a restart does (drop recordings, reopen
views, leave `view_count`), and was written to settle carry-on/throw/die when
the size *had* changed, not to settle the no-op case. PHILOSOPHY.md:309-336
states the equivalence claim in terms of pixels ("held to each other", enforced
by golden images; "nothing a backend does decides where a pixel goes") — not in
terms of when a call short-circuits. No test pins it: the new case at
`pixel_tests.cpp:1559-1616` resizes the real window first and then asserts
`CHECK(renderer.window_size_changed(...))` is true, exercising the fallthrough
on all three backends and never the guard.
`docs/review/backend-equivalence/DRIFT.md` sets the standard for this project's
comment-drift findings — a comment states X, a named line does not-X — and this
finding is not of that shape.

GROUND 3 — THE FIX IS REFUTED BY THE CODE IT CITES. Fix option 2 ("have gl
compare against the same quantity it hands back from back_buffer_size()") would
reintroduce the precise bug `gl/renderer.cpp:665-675` documents avoiding: the
shell forwards `GetClientRect` on WM_EXITSIZEMOVE, so a live-rect comparand
would answer "nothing changed" to the message that ends a resize. The finding
quotes that comment's closing sentence as its rule source without engaging with
the fact that the comment is the justification for the comparand it objects to.

GROUND 4 — NOT INTRODUCED BY THE CHANGE. `git show d31a804 --
engine/render/gl/renderer.cpp` shows the guard and the entire "compared against
the shell's number rather than the window's on purpose" paragraph as CONTEXT
lines; only the 40-line restart body is added. `Application::on_window_moved` is
untouched by both commits (last modified in 4dbb4a5). So the divergent comparand
predates the change under review.

---

### R5. UINT_MAX is used in the pipeline state description without including <climits>, compiling only through a Windows SDK transitive

**`engine/render/d3d12/renderer.cpp:745`**

**Why it was refuted.** The bare observation checks out, but three of the four
supports the finding rests on are wrong, and a shipped green backend does the
identical thing.

WHAT IS TRUE. `engine/render/d3d12/renderer.cpp:745` is exactly
`pipeline.SampleMask = UINT_MAX;` (git blame: added by 3dda092, so it is new
code in the change under review). `grep -rn UINT_MAX engine/` returns that one
line; `grep -rn "climits|limits.h" engine/ tests/ samples/` returns nothing. The
std block at :16-23 is quoted correctly — and is byte-identical to
d3d11/renderer.cpp:16-23, which is where it was copied from.

REFUTATION 1 — ANOTHER BACKEND DOES EXACTLY THIS AND IS GREEN.
`engine/render/gl/renderer.cpp:865` calls `std::memcpy(pixels.data() + y * width
* 4, source, width * 4);`. That file's standard block is lines 11-16: `<cstddef>
<memory> <stdexcept> <string> <tuple> <vector>` — no `<cstring>`, and
gl/backend.h adds only `<memory> <string> <vector>` plus gl_functions.h's
`<Windows.h>`/`<GL/gl.h>` (winnt.h's `<string.h>` gives `::memcpy`, not
`std::memcpy`). It is the same unwritten dependency on the same file: MSVC's
`<xutility>` line 11-12 is `#include <cstring>` / `#include <climits>`, and
`<xutility>` is what `<iterator>`, `<memory>`, `<string>` and `<vector>` drag
in. So `std::memcpy` in the GL backend and `UINT_MAX` in the D3D12 backend are
supplied by literally the same transitive header, and the GL one shipped, is
CI-built and passes RenderPixelTests. Verified against both toolsets on this box
(14.44.35207 and 14.51.36231): `xutility:12` is `#include <climits>` in each.

REFUTATION 2 — THE STATED MECHANISM IS WRONG. The finding says it "compiles
today only because engine/render/d3d12/device_resources.h:3 pulls in
<Windows.h>". `<Windows.h>` does not provide UINT_MAX. The Windows SDK says so
itself — winnt.h:1467 reads `// usually this would be * CHAR_BIT, but we don't
necessarily have #include <limits.h>`. Nothing in Windows.h's direct include
list
(winbase/wingdi/winuser/rpc/ole2/winsock/stralign/commdlg/mmsystem/shellapi/wincrypt/objbase,
all checked) includes limits.h or stdlib.h, and the only ucrt header that
includes `<limits.h>` is stdlib.h. The macro comes from the STL half alone.

REFUTATION 3 — THE TWO PROJECT RULES CITED DO NOT SAY WHAT THE FINDING SAYS.
CONVENTIONS.md:135 is the whole of the rule: "Include order: own header first —
proving every header compiles on its own — then engine headers, then external,
then standard library." The parenthetical is the rationale for
*own-header-first* (a .cpp including its own header first proves that HEADER is
self-sufficient). It is not a rule that a .cpp enumerate every standard header
it uses, and renderer.cpp is a .cpp that nothing includes. The project's
historical include findings (docs/review/all-findings.md:7422, 7856, 8267, 8540,
8984) are all about HEADERS not being self-contained, which is where the harm is
— and CLAUDE.md marks docs/review/ historical and about the pre-split game tree.
Second, check_engine_includes.cmake's second wall is quoted out of category:
lines 55-72 state it in capitals — "THE FOLDER, NOT ONE FILENAME IN IT" — it
stops files OUTSIDE engine/render/<backend>/ from naming headers INSIDE it. It
has nothing to do with standard-library macros arriving transitively, and
invoking it here is a category error.

REFUTATION 4 — THE FAILURE SCENARIO IS NOT REACHABLE. "Reorder those includes"
cannot break anything: the use is at line 745, after every include, so their
relative order is irrelevant to macro visibility. "Drop those includes" means
deleting `<vector>`/`<memory>`/`<string>` from a file that uses std::vector,
std::unique_ptr and std::string — the file fails a hundred other ways first.
That leaves "a future SDK/STL that stops exposing <climits> transitively", which
is the speculative future-proofing the review brief excludes; `<xutility>` is
the STL's own core header and has carried `#include <climits>` across both
toolsets present here. There is no runtime consequence at all, and CI builds the
one preset this file compiles in.

WHAT SURVIVES: an accurate, zero-risk hygiene nit on a new file. It is not a
defect with a failure scenario.

**And independently.** REFUTED — the rule invoked does not exist, the same
pattern is pervasive in shipped green code, and the stated compile mechanism is
factually wrong.

1) THE RULE IS NOT REAL AS STATED. docs/design/CONVENTIONS.md:129-136 is the
complete set of include rules: "#pragma once, not include guards" (:129),
"Include paths are written from the repository root" (:131), and "Include order:
own header first — proving every header compiles on its own — then engine
headers, then external, then standard library" (:135-136). That is an ORDERING
rule, and engine/render/d3d12/renderer.cpp:1-23 obeys it exactly (own header,
engine headers, generated shader headers, then the standard block). grep -rn -i
"include what you use|include-what-you-use|transitive|self-contained|iwyu" over
docs/design/, CLAUDE.md and docs/review/backend-equivalence/ returns NOTHING.
The finding's "Why wrong" supplies the missing premise itself — "an ordering
that presupposes a file lists the standard headers it uses" — but CONVENTIONS
attaches that rationale to *own header first*, and to *headers* compiling
standalone. renderer.cpp is a .cpp whose own header is first. No
include-what-you-use rule is written anywhere in this project.

2) THE IDENTICAL PATTERN IS PERVASIVE IN SHIPPED, GREEN CODE.
std::move/std::forward live in <utility>. Seventeen engine files use them with
no <utility> include, relying on exactly the same class of transitive reach:
  - engine/render/gl/render_resources.cpp — includes only <memory>, <string>;
    "this->textures.add(name, std::move(texture));" at :44
  - engine/render/null/render_resources.cpp — same two includes; std::move at
    :29
  - engine/render/render_resources.cpp — the SHARED engine file; <memory>,
    <string>, <string_view>; std::move at :33 and :39
  - engine/core/state_context.h — a HEADER, where the "compiles on its own"
    rationale actually bites; <any>, <functional>, <memory>, <stdexcept>,
    <vector>; std::move at :280-281 plus engine/app/application.cpp,
    assets/resource_loader.cpp, assets/sound_bank_loader.cpp,
    audio/audio_resources.cpp, audio/sound_bank.cpp, core/thread_pool.cpp,
    render/font.cpp, render/sprite_sheet.cpp, ui/button.cpp, ui/focus.cpp. Two
    other shipped backends and the shared seam-side file do the same thing. If
    this rule were real it would be a pre-existing engine-wide finding, not
    something the D3D12 backend introduced — and the finding does not claim
    that.

3) THE MECHANISM IS FACTUALLY WRONG. The finding says it "compiles today only
because engine/render/d3d12/device_resources.h:3 pulls in <Windows.h>".
<Windows.h> does not supply UINT_MAX. In the installed SDK (10.0.26100.0) the
ONLY definition is shared/intsafe.h:246 ("#define UINT_MAX 0xffffffff"), and
Windows.h does not include intsafe.h. um/winnt.h:1467 says so in its own
comment: "usually this would be * CHAR_BIT, but we don't necessarily have
#include <limits.h>". The actual provider is the MSVC STL, through FIVE
independent paths already in the file's own standard block (VC 14.51.36231):
<xutility>:12 includes <climits> directly, and <xmemory>:12 includes <limits>
which includes <climits> at :11. So <iterator>→<xutility>, <vector>→<xmemory>,
<memory>→<xmemory>, <string>→<xstring>→<xmemory>, and
<stdexcept>→<xstring>→<xmemory> each guarantee it on their own. The predicted
C2065 requires dropping ALL FIVE standard headers from a file that uses
std::vector, std::string, ComPtr and std::size — not "drop or reorder those
includes".

4) THE ANALOGY TO THE SECOND WALL IS A MISDESCRIPTION.
cmake/check_engine_includes.cmake:79-80 matches REGEX "^[ \t]*#[ \t]*include[
\t]*[\"<](\\.\\./)*(engine/)?render/[A-Za-z0-9_]+/[A-Za-z0-9_]+\\.h[\">]" — it
detects a file naming a header inside another backend's folder. It has nothing
to do with macro provenance or the <Windows.h> include graph; citing it as "the
accidental coupling the second wall exists to prevent" misstates what that check
does.

5) THE DRIFT STANDARD DOES NOT COVER THIS.
docs/review/backend-equivalence/DRIFT.md is a list of *comment/document claims
the code contradicts* (e.g. renderer.h:376-382 "three translation units" vs
four). Nothing in it concerns include hygiene, and there is no comment or
document claim about standard headers for renderer.cpp to contradict.

6) THE VALUE IS CORRECT AND CANONICAL.
D3D12_GRAPHICS_PIPELINE_STATE_DESC::SampleMask is a UINT; UINT_MAX = 0xffffffff
enables all samples and is what every Microsoft D3D12 sample and d3dx12 helper
writes. There is no D3D12 semantic issue, no validation-layer issue, and no
wrong pixel. It is also not a CONVENTIONS:151 "SCREAMING constants" violation —
UINT_MAX is a macro, which is the one thing screaming is reserved for.

No test in tests/render/ pins or could pin this; it is a compile-time question
and the file compiles in the one configuration it is built in (x64-debug-d3d12,
which CI builds).

---

## Never verified — ranked below the budget

9 findings survived triage but ranked below the top 20 and **were never put to a
refuter**. They are reproduced exactly as triage left them. Treat each as a
question, not a finding: the confirmed list above earned its place by surviving
two skeptics, and these have survived none.

### U1. open_frame() and set_view_count() each carry their own copy of the "open the declared views" block, on the two paths d31a804 exists to keep in step

**`engine/render/d3d12/renderer.cpp:550`** · low · `duplication`

```
open_frame(), renderer.cpp:547-556
		// Nothing on the ordinary path, where the frame has not said how many
		// views it has yet. Everything on the resize path, where it has.
		const D3D12_VIEWPORT viewport =
			this->device_resources.screen_viewport();
		for (int i = 0; i < this->view_count; i++)
		{
			this->views[static_cast<size_t>(i)]->begin(render_target,
				viewport);
		}

Renderer::set_view_count(), renderer.cpp:1163-1177
		this->impl_->view_count = count;
		...
		const D3D12_CPU_DESCRIPTOR_HANDLE render_target =
			this->impl_->device_resources.back_buffer_view();
		const D3D12_VIEWPORT viewport =
			this->impl_->device_resources.screen_viewport();

		for (int i = 0; i < count; i++)
		{
			this->impl_->views[static_cast<size_t>(i)]->begin(render_target,
				viewport);
		}
```

**Why it is wrong.** T3: the backend already has the seam for it —
Renderer::Impl is where the frame's non-view work lives, and backend.h:285-338
declares open_frame, open_frame_list, execute_frame_list and
transition_back_buffer for exactly this reason. The duplication is also new with
this backend rather than inherited: d3d11's two sites are genuinely different
code, because its restart path additionally clears and binds the immediate
context (d3d11/renderer.cpp:733-748).

**Failure scenario.** The two blocks are the same code over the same data —
set_view_count assigns view_count = count at :1163 before looping to `count`, so
the loop bound is this->view_count in both. One is the normal path (a frame
declaring its views), the other is the restart path. The next change to what
opening a view needs — begin() taking the scissor separately, a second
descriptor handle, or the restart path wanting a viewport that is not the full
screen — will be made at one site and the other will keep opening views the old
way. That is exactly the failure d31a804 was written to fix: the resize path and
the normal path disagreeing about the state a view is left in, with nothing able
to observe the difference except a real window being dragged, which 3dda092's
own message records as the only exercise ResizeBuffers has ever had.

**Fix.** Add `void Renderer::Impl::open_views();` beside open_frame() in
backend.h holding the six lines and reading this->view_count. open_frame() ends
with this->open_views(); set_view_count() ends with this->impl_->open_views();
after assigning view_count.

---

### U2. Both shader-visible descriptor heap start handles are re-queried from the runtime on every draw call, where the two increment sizes beside them are already cached members

**`engine/render/d3d12/renderer.cpp:483`** · low · `per-draw-work`

```
flush(), the per-draw-call path (renderer.cpp:235-238):
		this->list->SetGraphicsRootDescriptorTable(1,
			owner_impl.texture_slot_gpu(this->batch_texture->slot()));
		this->list->SetGraphicsRootDescriptorTable(2,
			owner_impl.sampler(this->filter));

renderer.cpp:479-486
	D3D12_GPU_DESCRIPTOR_HANDLE Renderer::Impl::texture_slot_gpu(int slot) const
	{
		D3D12_GPU_DESCRIPTOR_HANDLE handle =
			this->texture_heap->GetGPUDescriptorHandleForHeapStart();
		handle.ptr += static_cast<UINT64>(slot) * this->texture_descriptor_size;
		return handle;
	}

and the same shape in Renderer::Impl::sampler (renderer.cpp:444-450) against sampler_heap.
```

**Why it is wrong.** The file sets its own bar for this and then misses it.
backend.h:82-84 justifies carrying width and height inside D3d12Texture
specifically so a draw does not pay "the two virtual calls the D3D11 backend
makes", and renderer.cpp:217-225 states the rule for flush(): things "invariant
for the life of a list" go in begin(), "everything that varies per draw call is
here". A descriptor heap's start handle is invariant for the life of the heap,
which is longer than a list — and the two increment sizes derived from the same
heap creation are already cached as members at renderer.cpp:801-806 and reset
there on a device loss. PHILOSOPHY T8: a cost that taxes the frame loop goes,
and this engine is sized for the low tier.

**Failure scenario.** Every DrawList::View::flush() — one per texture change,
per filter change, per set_viewport and per page fill, i.e. one per draw call
the frame issues — makes two COM virtual calls into the D3D12 runtime purely to
recompute two values that last changed in create_device_dependent_resources. A
four-view frame in which each view alternates between a tile sheet and a font
atlas (a HUD over a board) issues tens of flushes per view; at 60 Hz that is
thousands of vtable calls per second, each returning a struct by value through
the MSVC hidden-return-pointer thunk, for two pointers that are constant for the
life of the heap. DeviceResources::back_buffer_view
(device_resources.cpp:510-517) re-queries GetCPUDescriptorHandleForHeapStart the
same way twice per frame.

**Fix.** Add texture_heap_start, texture_heap_cpu_start and sampler_heap_start
members to Renderer::Impl, fill them in create_device_dependent_resources
immediately after the two GetDescriptorHandleIncrementSize calls, and have
sampler(), texture_slot_gpu() and texture_slot_cpu() read the member. The same
one-line change applies to DeviceResources::back_buffer_view.

---

### U3. Renderer::submit() heap-allocates a fresh vector of command list pointers on every frame, the only frame-path allocation any of the four backends makes

**`engine/render/d3d12/renderer.cpp:1209`** · low · `frame-path-allocation`

```
renderer.cpp:1209-1210
		std::vector<ID3D12CommandList*> lists;
		lists.reserve(this->impl_->views.size());
```

**Why it is wrong.** PHILOSOPHY T2 names allocation-free loops among the
structures "designed in from the start", and T8 says a cost that taxes the frame
loop goes. The fix is a pattern this very file already uses and justifies:
DrawList::View::batch is a std::vector<SpriteVertex> that is clear()ed rather
than destroyed on every flush precisely so its capacity survives
(backend.h:186-189, renderer.cpp:245). The array of command lists is the same
shape of buffer and did not get the same treatment.

**Failure scenario.** Both statements run once per frame for the life of the
process: the vector is constructed empty, reserve() forces one operator new of
view_capacity * sizeof(void*), and the destructor one operator delete — 120
allocator round trips per second at 60 Hz, for a list whose maximum length was
fixed at create_device time and never changes. d3d11's submit executes lists one
at a time and allocates nothing (d3d11/renderer.cpp:896-911); gl replays vectors
it already owns and allocates nothing; null clears a member vector
(null/renderer.cpp:265). On a low-tier machine with a general-purpose allocator
this is a lock and a free-list walk inside the seam's hot call, and it scales
with view capacity rather than being fixed.

**Fix.** Make it a member of Renderer::Impl — std::vector<ID3D12CommandList*>
submit_lists; — reserved once in Renderer::create_device beside the
views.reserve at renderer.cpp:1035, and start submit() with
submit_lists.clear(). clear() keeps the capacity, so the loop and the
ExecuteCommandLists call below are unchanged and no frame after the first
allocates.

---

### U4. texture_factory.cpp hand-writes the upload-buffer description that create_buffer() already produces, because the helper is private to renderer.cpp

**`engine/render/d3d12/texture_factory.cpp:166`** · low · `duplication`

```
texture_factory.cpp:166-184
		const D3D12_HEAP_PROPERTIES upload_heap = { D3D12_HEAP_TYPE_UPLOAD,
			D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };

		D3D12_RESOURCE_DESC staging_description = {};
		staging_description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		staging_description.Width = total_bytes;
		staging_description.Height = 1;
		staging_description.DepthOrArraySize = 1;
		staging_description.MipLevels = 1;
		staging_description.Format = DXGI_FORMAT_UNKNOWN;
		staging_description.SampleDesc.Count = 1;
		staging_description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		staging_description.Flags = D3D12_RESOURCE_FLAG_NONE;

		ComPtr<ID3D12Resource> staging;
		ThrowIfFailed(device->CreateCommittedResource(&upload_heap, ...));
```

**Why it is wrong.** T3: the folder already owns the simpler model, and the
second copy exists only because create_buffer sits in renderer.cpp's anonymous
namespace where its sibling translation unit cannot reach it. Nothing here is a
seam concern — both files are inside engine/render/d3d12/, so sharing it pushes
no API detail upward and breaks no rule check_engine_includes.cmake enforces.
texture_factory.cpp:37-44 explains carefully why to_dxgi_format MUST be
duplicated (DXGI_FORMAT cannot be named outside the folder); that argument does
not cover this, which is what makes the contrast telling.

**Failure scenario.** These nineteen lines are field for field the object
create_buffer(device, D3D12_HEAP_TYPE_UPLOAD, total_bytes,
D3D12_RESOURCE_STATE_GENERIC_READ) produces in renderer.cpp:98-109 (Alignment 0,
Height 1, DepthOrArraySize 1, MipLevels 1, Format UNKNOWN, SampleDesc {1,0},
Layout ROW_MAJOR, Flags NONE), and the same is true of default_heap at
texture_factory.cpp:128-129. The next change to how this backend describes a
buffer — an explicit Alignment for a small-page driver, a D3D12_HEAP_FLAG, or
D3D12_HEAP_TYPE_CUSTOM on a UMA part — lands in create_buffer and silently
leaves the texture load path on the old description: every vertex page, the
index buffer and the readback buffer get the new one and every texture's staging
buffer gets the old one. No warning, no test, and the two are in different
translation units so nothing puts them side by side.

**Fix.** Move heap_properties, buffer_description and create_buffer out of
renderer.cpp's anonymous namespace into engine/render/d3d12/backend.h (or a
folder-private header beside it) and call create_buffer here and at
texture_factory.cpp:128.

---

### U5. The full-stall-per-texture paragraph cites resource_factory.h for a claim that file does not make, and states the opposite of what the other two backends do

**`engine/render/d3d12/texture_factory.cpp:94`** · low · `comment-drift`

```
texture_factory.cpp:93-97
	// A FULL STALL PER TEXTURE, AND THAT IS THE RIGHT ANSWER HERE RATHER THAN A
	// SHORTCUT. Loading already stalls on every backend (resource_factory.h),
	// this runs at load and never on the frame path, and the alternative -
	// keeping every upload buffer alive until some later fence - is a pool and
	// a lifetime rule for a path that reads files off a disk.
```

**Why it is wrong.** DRIFT.md calls out a cross-reference to a statement that is
not at the other end by name ("sends the reader to
engine/render/<backend>/device_resources.h, a file two of three folders do not
have"). The decision being defended — a full GPU stall per texture on the load
path — is real and defensible; it is the supporting citation that is dead, in a
comment written by this commit.

**Failure scenario.** A reader following the citation to justify the
wait_for_gpu() per texture at :246 opens engine/render/resource_factory.h and
finds nothing about stalling — the word does not appear in the file. What that
header states about add_texture_asset is the ordering rule ("AFTER
create_device", :79-87) and the throw contract, and nothing about the cost of
loading on any backend. The claim is also not true of the siblings in the way
the sentence implies: d3d11/texture_factory.cpp hands CreateTexture2D an array
of D3D11_SUBRESOURCE_DATA and returns without waiting on the GPU, and
gl/texture_factory.cpp calls glTexImage2D/glCompressedTexImage2D and returns.
d3d12 is the only one of the four that blocks on a fence here, which is the
opposite of what "Loading already stalls on every backend" tells the reader —
and a later reader can use that sentence to justify copying the pattern
somewhere it is not safe.

**Fix.** Drop the citation and state the argument on its own terms (this is the
load path, it already blocks on file I/O, and the alternative is an
upload-buffer pool with a lifetime rule), or make the comparison honest: d3d11
and gl do not stall here, and this backend does because it is the only one where
nobody else is tracking when the copy finished.

---

### U6. d31a804's duplicated size guard made DeviceResources::UpdateColorSpace unreachable on the path its own comment names, so the DXGI factory refresh on a monitor change is now one Present late

**`engine/render/d3d11/device_resources.cpp:535`** · low · `regression`

```
d3d11/device_resources.cpp:535-538
    // Still called on three paths, and the first one does the work: swap chain
    // creation, a resize that turns out not to be one, and a Present that finds
    // the factory stale.
    void DeviceResources::UpdateColorSpace()

and the branch that is now dead, device_resources.cpp:341-346:
        if (newRc.right == m_outputSize.right && newRc.bottom == m_outputSize.bottom)
        {
            UpdateColorSpace();

            return false;
        }
```

**Why it is wrong.** DRIFT.md's standard is that comments in engine/render/ are
held to the same amendment rule as the design documents, and this is a claim the
code no longer matches — in the file the commit message singles out as
untouchable ("that file is Microsoft's and carried with its own naming
(NOTICE)"), which is exactly why the guard was duplicated into renderer.cpp and
exactly why nothing recorded that one of the three paths went away.

**Failure scenario.** Renderer::window_size_changed (d3d11/renderer.cpp:697-702)
is now the only caller of DeviceResources::WindowSizeChanged in the tree (`grep
-rn WindowSizeChanged engine/` confirms), and d31a804 made it return before
calling it whenever the size matches. The "resize that turns out not to be one"
branch at device_resources.cpp:343 is therefore unreachable. That branch is what
Application::on_window_moved was reaching: dragging the window onto a second
monitor invalidates the cached DXGI factory (output information is cached on
it), and UpdateColorSpace's IsCurrent()/CreateFactory pair at :541-547 is what
notices and rebuilds it. The refresh now waits for the next Present that happens
to check IsCurrent (:419-422), so it is one frame late rather than immediate,
and the comment at :535-537 asserts a call site that can no longer be reached.

**Fix.** Either let the unchanged-size call reach WindowSizeChanged (returning
its false unmodified, and doing the frame teardown only afterwards), or note at
d3d11/renderer.cpp:690-696 that the short circuit costs the UpdateColorSpace
refresh and why the Present path is considered enough — and amend
device_resources.cpp:535-537 to say two paths.

---

### U7. sprite.hlsl claims 5_1 is the lowest profile Direct3D 12 accepts and that the golden set checks the two profiles produce the same arithmetic; 5_0 is lower and accepted, and the comparison allows 8 per channel

**`engine/render/sprite.hlsl:30`** · low · `comment-drift`

```
sprite.hlsl:30-35
// COMPILED AT BUILD TIME, at the lowest profile each backend accepts:
// 4_0_level_9_1 for D3D11, which are the lowest profiles that exist, and 5_1
// for D3D12, which has no shader model below 5. Both cost nothing to ask for:
// there are no loops, no branches and no integer arithmetic here, so a later
// profile would buy the shader nothing and the two produce the same arithmetic
// - which tests/render/golden/ is what actually checks.

engine/CMakeLists.txt:138-140 repeats it: "at the lowest profile this API has. Shader model 5 is where D3D12 starts ... and 5_1 is the one fxc emits for it."
```

**Why it is wrong.** The file's own framing is that every claim in it is
checkable — it corrects an earlier false claim about feature level 9.1 five
lines further down and cites the correction. "The lowest profile each backend
accepts" and "which tests/render/golden/ is what actually checks" are exactly
the kind this project holds to the code, and DRIFT.md's standard is that a claim
the tree does not support is a defect whether or not anything currently depends
on it. renderer.h:552-555 states the channel allowance correctly; this file does
not.

**Failure scenario.** fxc emits vs_5_0/ps_5_0 as readily as 5_1, and Direct3D 12
accepts shader model 5.0 DXBC — Microsoft's own D3D12 samples compile at
vs_5_0/ps_5_0 and hand the blob to CreateGraphicsPipelineState. So 5_1 is not
the floor; 5_0 is, and both halves of "the lowest profile each backend accepts"
and "5_1 is the one fxc emits for it" are false. The subordinate clause "which
has no shader model below 5" is the true fact the sentence is reaching for. A
maintainer who later wants the lower profile — 5_1's register-space binding
model buys this shader nothing, it having one cbuffer, one texture and one
sampler all in space0 — is told by two files that there is nothing below to ask
for, and the parallel with d3d11's genuinely-lowest 4_0_level_9_1 is what makes
the claim read as verified rather than approximate.

The second claim is also stated too strongly: tests/render/golden_image.cpp:410
is `constexpr int ALLOWED_CHANNEL_DRIFT = 8;` and check_frame counts only pixels
whose worst channel exceeds 8 (:501). The two profiles are never compared to
each other — each is compared to the checked-in image — so a per-channel
divergence inside that band passes on both runs while the two backends differ
from each other by up to 16. A shader-profile arithmetic difference of exactly
that size is what this sentence tells a reader is guarded.

**Fix.** Say what is true and what was chosen: shader model 5 is D3D12's floor,
5_0 and 5_1 both compile this file to the same arithmetic, and 5_1 is what this
build asks for. Qualify the golden claim the way renderer.h does — the set holds
both profiles to one image within the per-channel allowance golden_image.cpp
measures. Amend engine/CMakeLists.txt:138-140 in the same pass.

---

### U8. Three shared engine/render/ headers and the backend-selection block still count three backends after the fourth landed reading the same code

**`engine/render/sprite_vertex.h:10`** · low · `comment-drift`

```
sprite_vertex.h:10-13 — "Both real backends build every offset from offsetof and bind by semantic (d3d11) or by name (gl), so the three fields can be reordered here and both follow silently and correctly." d3d12/renderer.cpp:707-718 is a third consumer, building POSITION/COLOR/TEXCOORD offsets with offsetof and binding by semantic. :23-24's "the other two backends never make one" (a depth buffer) is short by one for the same reason.

texture_format.h:13-16 — "the backends that consume one are the whole of its traffic - two of the three do". d3d12/texture_factory.cpp:45-63 is a third, with its own to_dxgi_format switch and its own b4g4r4a4 refusal.

sprite_geometry.h:25-26 — "Three backends cannot disagree about where a sprite went, because only one of them decides." Four call build_sprite_quad/build_glyph_quad now (d3d12/renderer.cpp:376 and :417).

engine/CMakeLists.txt:160-163 — "it is the one place the two backends genuinely differ in kind rather than spelling", in the gl branch of a chain whose own cache string at :98 says four.
```

**Why it is wrong.** Every one of these was corrected by commit a56d198 after
the previous audit flagged it, and 3dda092 falsified all four again without
amendment. The project's rule is that a document a change fights is amended in
the same commit as the change, and the claims themselves are still true of the
code — it is only the counts that went stale, which is what makes them cheap to
fix and cheap to keep checking.

**Failure scenario.** Each is a list a maintainer uses as the set of call sites
to check. Someone reordering SpriteVertex on the strength of "both follow
silently and correctly" audits two of the three backends that consume the
struct. Someone adding an enumerator to TextureFormat misses the d3d12 switch —
whose `default:` label silently routes an unknown enumerator to r8g8b8a8_unorm.
And engine/CMakeLists.txt now says four backends at :98 and two at :163, in one
file; DRIFT.md already flagged that sentence before the fourth backend landed.

**Fix.** "All three real backends build every offset from offsetof and bind by
semantic (d3d11, d3d12) or by name (gl)" and "the other three backends never
make one"; "three of the four do ... and the null backend reads it never"; "Four
backends cannot disagree"; and "the one place the backends that have a shader
genuinely differ in kind rather than spelling" — two compile HLSL offline at two
profiles, one compiles GLSL at device creation.

---

### U9. Four design-document claims 3dda092 and d31a804 falsified while editing the very lines and paragraphs around them

**`docs/design/ARCHITECTURE.md:124`** · low · `comment-drift`

```
ARCHITECTURE.md:124 — "│   │   ├── d3d11/          the D3D11 backend: a device, buffers, four states". d3d11/renderer.cpp creates five state objects (CreateBlendState :474, CreateDepthStencilState :483, CreateRasterizerState :496, two CreateSamplerState :528, :532), and CLAUDE.md:150-156 was amended by the same commit to say "five state objects on `d3d11/`, one pipeline state object and two samplers on `d3d12/`". 3dda092 rewrote this exact line, removing "a shader compiled by fxc at build time" from it.

ARCHITECTURE.md:110 — "├── CMakePresets.json       the configurations: debug, release". Five configure presets now, four of which select a render backend and are the only way to build three of the four backends — while :325 on the same page says "There are four backends ... chosen by LABRADOR_RENDER_BACKEND at configure time".

PHILOSOPHY.md:332 — "A backend is now a device, a texture from bytes, a buffer, a shader and four state objects". 3dda092 rewrote the paragraph immediately below this line (the new "The fourth backend tests a different claim" block at :336-346) and fixed the identical count in CLAUDE.md, leaving the two documents a contributor is told to read first giving different answers to "what does a backend supply".

CLAUDE.md:74 — "What runs in all three configurations is tests/render/renderer_seam_tests.cpp". Four; that file is an unconditional source in tests/render/CMakeLists.txt:3-17. 3dda092 rewrote this same paragraph twice over ("There are four render backends" at :32, "all four" at :35, "all three rasterising backends" at :63) and left this count nine lines later.
```

**Why it is wrong.** docs/design/ is authoritative on intent and "changes by
amendment in the same commit as the change that fights it" (CLAUDE.md, "Reading
the documents correctly"). Editing the line is the moment the amendment rule
applies, and in three of these four cases the author was editing the line or the
paragraph. DRIFT.md had already filed the "four state objects" count against
CLAUDE.md's twin; fixing one of two copies in one commit is what leaves the pair
contradicting each other.

**Failure scenario.** A contributor reading ARCHITECTURE's tree for "where are
the configurations" is told there are two, and its only description of the file
that is the entry point to every backend the same page says exists. One
reconciling PHILOSOPHY against CLAUDE.md gets two different answers for the same
folder. One using CLAUDE.md:74 to decide where a device-free seam assertion runs
under-counts by one configuration.

**Fix.** ARCHITECTURE.md:124 "a device, buffers, five states"; :110 "the
configurations: debug, release, and one per render backend"; CLAUDE.md:74 "all
four configurations". For PHILOSOPHY.md:332, make it the property rather than
the number — "a device, a texture from bytes, a buffer, a shader, and whatever
its API spells the blend, the rasteriser state and the two filters as" — which
is the wording CLAUDE.md:150-156 already uses and which no fifth backend can
falsify.

---

## What the critics said about the coverage

**Critic 1.** FUNCTION INVENTORY. The six files hold ~96 function bodies:
renderer.cpp 52 (6 anonymous-namespace helpers, D3d12Texture::size, 6 View
methods, 5 DrawList methods, 15 Impl methods, 19 Renderer methods of which 3 are
=default), device_resources.cpp 20 + 8 inline accessors in its header,
texture_factory.cpp 4, render_resources.cpp 10, backend.h 2 inline.

WHAT THE 27 LENSES DEMONSTRABLY TOUCHED. Counting every file:line in the
confirmed, refuted and not-verified lists, the findings land on roughly 30 of
those 96: View::begin (264), View::flush/pages (vertex-ring), abandon_recording
(608), open_frame (550), texture_slot_gpu (483), allocate_texture_slot +
texture_slot_cpu (texture_factory 250), frame_open +
Renderer::window_size_changed (1076), submit (1209),
create_device_dependent_resources' PSO block only (745), the two destructors
(device_resources.cpp:56, renderer.cpp:431), add_texture_asset (94/121/166/250),
RenderResources::release_device_resources (render_resources.cpp:60), and the
fence quartet signal_frame/wait_for_frame/wait_for_gpu/move_to_next_frame via
the device_resources.h:101 finding. Everything else in the finding list is
comment/claim drift in renderer.h, backend.h, sprite.hlsl, pixel_tests.cpp,
ARCHITECTURE.md — documents, not code.

REGIONS NOBODY LOOKED AT. Five contiguous blocks carry no finding line number
anywhere inside them:
 1. device_resources.cpp:19-281 (263 lines) — MIN_FEATURE_LEVEL, the
    constructor, set_window, create_factory, hardware_adapter,
    create_device_resources. The single largest un-cited block in the backend,
    entirely error/fallback path, and the only substantial code in this folder
    with no house-written counterpart next door (d3d11's is Microsoft's vendored
    file). MERELY UNEXAMINED.
 2. renderer.cpp:559-588 + 1245-1335 + texture_factory.cpp:214-248 — the
    `frame_list` / `frame_allocators` construct and its four call sites, plus
    read_back_buffer (90 lines). This construct has no counterpart in any other
    backend at all; d3d11 uses the immediate context and gl has nothing. MERELY
    UNEXAMINED.
 3. renderer.cpp:790-958 (168 lines) — descriptor heaps, the two samplers, the
    per-view allocator/list loop, the index-buffer upload. Partly checkable
    against goldens (I confirmed MaxLOD parity is pinned by the mip test), so
    lower value. PARTLY UNINTERESTING.
 4. renderer.cpp:324-425 —
    set_viewport/set_camera/set_filter/draw_sprite/draw_text, and 33-120, the
    conversion helpers. I checked the one hook I expected to pay (View::reset
    resetting camera/filter across the resize restart) and all three backends do
    the same thing, so this is thinner than it looks. MOSTLY UNINTERESTING; the
    scissor rectangle at a negative-origin viewport is the only live question
    left and it is narrow.
 5. The shell-to-backend contract in engine/app/ — Application::tick/render
    (254-300, no re-entrancy guard) and window.cpp's
    WM_PAINT/WM_SIZE/WM_EXITSIZEMOVE routing (344-415). Only one finding ever
    crossed into engine/app/ (the RenderResources destruction order), and nobody
    asked which message path actually delivers window_size_changed between
    begin_frame and submit — which is the premise the entire d31a804 fix rests
    on. MERELY UNEXAMINED, and it questions the commit's own justification.

I confirmed two candidates dead before proposing them, so the probes below are
not padding: to_dxgi_format/format_name in texture_factory.cpp:45-77 are
character-identical to d3d11's and the duplication is argued in place; and
read_back_buffer's row-padding path IS exercised, because the mid-frame-resize
case at pixel_tests.cpp:1592 reads back a 32x32 buffer whose RowPitch is 256
against a width*4 of 128, where the 64x64 goldens hit the alignment exactly.

**Critic 2.** The 27 lenses covered each subsystem in isolation and did it well:
the fence discipline is actually sound (I traced the full two-frame ring by hand
— begin_frame's wait_for_frame on frame_fences_[frame_index_], signal_frame
after every ExecuteCommandLists, move_to_next_frame re-querying
GetCurrentBackBufferIndex — and frames 0/1/2 alternate correctly, so the
vertex-ring, cmdlist-lifecycle and fence-sync lenses were right to come back
with little). The confirmed list also already owns the two genuine defects
inside single files (the frame_open() gate at renderer.cpp:1076, the
texture-resource-before-GPU-idle ordering at render_resources.cpp:60) and the
large body of comment drift.

What none of them touched is the code path that only exists when two files are
on the stack at once, and there are three of those in this change:

(1) Device removal composed with the resize path.
DeviceResources::create_window_size_dependent_resources calls wait_for_gpu() at
device_resources.cpp:292 — which is signal_frame() →
ThrowIfFailed(command_queue_->Signal(...)) — BEFORE the ResizeBuffers whose
DXGI_ERROR_DEVICE_REMOVED branch sits twenty lines below at 310. The device-loss
lens looked at present() and the destructors; the resize lens looked at the
frame restart; nobody asked what happens when the two meet, and the answer looks
like a throw escaping Renderer::window_size_changed into a window procedure —
the exact failure mode d31a804's commit message presents as the thing it
removed.

(2) The frame list is a single recording target with five callers (open_frame,
transition_back_buffer, read_back_buffer, create_device_dependent_resources,
add_texture_asset in another file), and one of them — the asset reload — runs
re-entrantly inside end_frame() → present() → handle_device_lost() →
on_device_restored(). Every lens examined one caller. Nobody enumerated the
entry/exit state of frame_list_open, frame_allocators[frame] and
back_buffer_states_ across all five, or traced the reload that runs with
Renderer::end_frame still on the stack.

(3) The caller side of the new seam term. Every lens stayed inside
engine/render/. The seam's justification at renderer.h:293-299 names a specific
mechanism — "window.cpp renders from WM_PAINT, and a vsync Present is entitled
to pump" — and window.cpp gates WM_PAINT→tick() on in_sizemove_ (line 347) while
gating WM_SIZE→on_window_size_changed on !in_sizemove_ (line 394), which are
mutually exclusive. Whether the mechanism the seam cites can actually deliver a
resize mid-frame, and what frame phase the reachable deliveries land in, is
untested by anything.

Things I checked and am confident are NOT defects, so nobody should spend budget
re-deriving them: the frame-index/allocator/vertex-page triple never disagrees
(frame_index_ only moves in move_to_next_frame and
create_window_size_dependent_resources, and the resize path abandons recording
first and idles the GPU); open_frame_list resetting the list onto a
not-yet-reset allocator only appends, which is legal; the multi-threaded fan-out
in Scene::draw cannot collide with window_size_changed because
ThreadPool::wait_for_tasks_to_complete uses WaitForThreadpoolWorkCallbacks, a
non-alertable wait that does not pump; Impl::on_device_lost's field list is
fully covered by create_device_dependent_resources plus View::reset(); and
Registry::add reuses a name's slot, so handles survive a device restore.

**Critic 3.** COVERAGE IS DEEP BUT NARROW, AND THE NARROWNESS IS GEOGRAPHIC.
Twenty-six of the twenty-seven lenses point at `engine/render/d3d12/`. The one
that does not (backend-equivalence) produced the null-backend finding. The
result: of the 15 confirmed findings, exactly three are code defects inside
d3d12 (`renderer.cpp:1076`, `render_resources.cpp:60`,
`texture_factory.cpp:250`), one is a code defect in null, one is a shutdown
defect in d3d12's device_resources, and the remaining nine are comment/doc
drift. Zero code findings in `engine/render/d3d11/` or `engine/render/gl/` — the
two backends d31a804 also rewrote (+65 and +40 lines). Zero findings anywhere in
`engine/app/`, which is the only caller of the seam that changed.

THREE SPECIFIC BLIND SPOTS I CAN NAME.

(1) The confirmed high finding has an unreported structural twin in the DEFAULT
backend. `d3d12/renderer.cpp:1076` gates the restart on `impl.frame_open()`,
false between `begin_frame()` and the first `set_view_count()`.
`d3d11/renderer.cpp:716-728` does the same thing with `restart = restart ||
view.bound`, and `bound` is set only by `View::bind()` (line 210), called only
from `set_view_count` and from the restart block itself — while `begin_frame`
(line 811) clears it via `reset()` (line 224). Same interval, same false gate,
same consequence: `if (rebuilt && restart)` is skipped, so the
`ClearRenderTargetView` at line 736 never runs and the frame draws onto a back
buffer `ResizeBuffers` just handed back with undefined content. Nobody raised
it. gl takes a third path again (clears and resets unconditionally, and never
consults `view_count`).

(2) Nobody opened `engine/app/`. That matters more than it sounds, because
`Application::render()` (application.cpp:280-299) is `begin_frame →
StateContext::draw → submit → end_frame`, `update()` runs strictly before
`render()`, and the only in-frame call that can dispatch a window message is
`Present`, which lives inside `end_frame()` — after `submit()`, when
`frame_open()` is already false on every backend. If that is the complete set of
paths, then the mid-frame resize the whole of d31a804 exists for is unreachable
from this shell, and the commit's central argument ("a vsync Present is entitled
to pump messages", "IT IS NOT A RULE THE CALLER COULD KEEP") is wrong about
where Present sits — while the crash it fixed ("one run in six") had a different
cause that is still there. This is the "most serious problem is architectural"
possibility, and no lens was in a position to see it.

(3) Nothing in `tests/` draws more than 2048 sprites into one view (grep for
2048/MAX_PAGE/batch across `tests/render/*.cpp` returns two unrelated comments).
So `View::flush()`'s take-another-page branch at `d3d12/renderer.cpp:180-183` —
one of the six decisions the commit message names by name, and the one place
this backend deliberately differs from d3d11's MAP_DISCARD wrap — executes in no
ctest configuration and appears in no golden image. Likewise `texture_data.h:53`
records that every .dds in this repository and its client is single-level, so
the per-mip footprint loop in `texture_factory.cpp:191-212` is never executed
either. Both are defect-shaped code that ships green on every preset and on both
CI rasterisers.

WHAT THE REVIEW GOT RIGHT, checked independently. The state objects are
field-for-field equal to d3d11's, including `MaxLOD = 0.0f` (so the mip-level
term settled in 68fe4dc holds), `MultisampleEnable = TRUE`, `SrcBlend = ONE`,
`CullMode = NONE`, `DepthClipEnable = TRUE` — so the premultiplied-alpha and
minification terms cannot diverge on hardware. `MAX_PAGE_SPRITES * 4 = 8192` is
safe for the `R16_UINT` index buffer. `create_device_resources()` resets
**both** `fence_value_` and `frame_fences_[]` (device_resources.cpp:263-267), so
a device loss cannot strand `wait_for_frame()` on a value the new fence will
never reach — the obvious hang is not there. `frame_index_` is re-read from
`GetCurrentBackBufferIndex()` after `ResizeBuffers` (device_resources.cpp:381)
rather than derived by adding one, so the allocator ring cannot desynchronise
from the swap chain across a resize. The commit-message claims I spot-checked
hold: 47 golden PNGs exist, the "43 of the 45 images" figure is consistent with
`texture_format.h:35` and `gl/texture_factory.cpp:44` and is explicitly about
this repository plus its client, NOTICE does carry the d3d12 paragraph (line
48), and `Harness::end_not_comparable`'s own comment (pixel_tests.cpp:298-312)
genuinely was rewritten to cover both exempt frames — the stale census is the
separate file-header one already confirmed at line 62.

WHERE I THINK A REFUTATION MAY HAVE BEEN TOO QUICK. The dropped
`pixel_tests.cpp:277` finding (frame_index_ never leaves 0) is worth one
sentence of re-derivation by whoever runs probe 3: `frame_index_` changes only
in `move_to_next_frame()`, called only from `present()`, called only from
`end_frame()`, and pixel_tests.cpp:277 says in so many words "Deliberately no
end_frame()". I am not re-raising it, but probe 3 will touch the same machinery
and should say plainly whether the second half of the ring executes anywhere.
