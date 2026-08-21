# The Vulkan backend, and the one instrument that had not been pointed at it

> Review by 177 agents across 30 lenses, 2026-08-21, against the tree at
> `4d91b91` — the single commit "Add the Vulkan backend, which is the first one
> that is about a platform". `engine/render/vulkan/` — 4,378 lines in six files
> — plus the seam in [renderer.h](../../../engine/render/renderer.h) and
> [render_resources.h](../../../engine/render/render_resources.h), the shared
> [sprite.hlsl](../../../engine/render/sprite.hlsl) at a third profile, the
> build ([engine/CMakeLists.txt](../../../engine/CMakeLists.txt),
> [compile_shaders.cmake](../../../cmake/compile_shaders.cmake),
> [CMakePresets.json](../../../CMakePresets.json),
> [ci.yml](../../../.github/workflows/ci.yml)), the tests, and the six documents
> the same commit amended.
>
> **Unlike the [Direct3D 12 review](../d3d12/README.md), part of this one was
> run.** The 177 agents read source and nothing else — no build, no `ctest`, no
> device, and every finding below §2 was written from reading. But this backend
> is the one whose own commit message says "a Vulkan mistake is usually silent,
> so *the tests are green* is a weaker statement here than on the Direct3D
> backends", so the suite was built and executed against it, and
> `RenderPixelTests` was re-run under the Khronos validation layer with
> **synchronization validation** enabled — which is not what the layers check by
> default and not what this backend was developed against. **§2 is what that
> said.** It confirms two of the findings below from outside the reading, and it
> is the reason §3.2 is a must-fix rather than an argument.
>
> **This document is the review as written, and is not updated as findings are
> fixed.** Like the [backend equivalence audit](../backend-equivalence/README.md)
> and the D3D12 review it postdates the repo split, so it cites no `game/` path,
> and its line numbers were current at `4d91b91`.

**167 candidates raised, 107 distinct after triage, 42 verified, 34 confirmed
and 8 refuted; 16 more raised by gap probes and confirmed, 3 refuted. Three must
fix.** Sixty-five ranked below the verification budget and were never checked —
they are questions in §7, not findings.

| Document | What it holds |
|---|---|
| **README.md** (this file) | The verdict, what the layers said, the findings merged and argued, what was refuted, and where the coverage is thin |
| [all-findings.md](all-findings.md) | Every finding as the run produced it — evidence, failure scenario, fix — with both refuters' reasoning printed underneath, and the refutations that killed the rest |

---

## How it was done

Thirty lenses ran independently over the same tree, each told to go deep on one
axis and not to dilute it: timeline semaphores and frame pacing, command buffer
and pool lifecycle, image layouts and barriers, the swapchain, device memory and
alignment, descriptor sets, pipeline and render pass state, object lifetimes,
the texture upload, the read-back path, `VkResult` handling and device loss,
instance and device creation, threading, seam conformance, equivalence against
`d3d12/`, equivalence against `gl/` and `null/`, pixel-divergence risk, the
fallout on the four backends that already worked, include discipline, the build,
the shader at three profiles, CI, the new test case, test coverage, the comments
inside the folder, the documents the commit amended, the commit message itself,
conventions, simplification, and degenerate inputs. Each was given the three
design documents, `CLAUDE.md`, the four sibling backends, and the standard that
a comment the code does not match is a defect — which is
[DRIFT.md](../backend-equivalence/DRIFT.md)'s, and which this backend's unusually
argumentative comments make load-bearing.

**One lens died and is not in the count.** `include-discipline` — the walls, and
the second pass of
[check_engine_includes.cmake](../../../cmake/check_engine_includes.cmake) that
enforces them — lost its connection mid-response and returned nothing. That axis
is unreviewed, and §9 says so again where it will be read.

Their 167 raw findings went through one triage pass per area — five areas, each
told to drop non-findings, merge duplicates and rank what was left — which
produced 107 distinct and dropped 18. The top of each area's list, 42 findings,
then went to **two adversarial refuters each, both instructed to default to
refuted**: one told to read the code and try to reach the failure from a real
entry point, one told to state the rule being invoked — a term of the seam, a
repository convention, or a clause of the Vulkan specification — precisely
enough to check that it says what the finding needs, on the grounds that much
plausible-sounding Vulkan is folklore. Where the two disagreed an adjudicator
read the code and decided. Thirty-four survived; the eight that did not are in
§8.

Three completeness critics then read what the lenses had found, what they said
they could not check, and what was never verified, and named nine probes. The
probes raised sixteen findings that survived the same two refuters and three
that did not. **Three probes independently rediscovered §3.1**, which four
lenses had already reached from four different areas — seven routes to one
branch is the strongest agreement this run produced.

---

## 1. Verdict

**The port is sound in its largest decision and wrong at its two edges.**
Drawing the frame into an image the engine owns and blitting it into a swapchain
image at present is the right call, and the seam absorbed it without a new term:
`read_back_buffer`, the frame shape, the pipeline, a descriptor set per run and
the texture path all stayed underneath `renderer.h` exactly as `d3d12/` did, and
the resize contract really did turn out to have the shape a presentation engine
needs. Nothing in this review argues with the design.

What is wrong is not spread evenly. It sits on the two boundaries where this
backend had to say something no other backend had to say — **the window's edge**
and **the frame's edge** — and in both places the code emits a weaker guarantee
than the comment beside it legislates. `create_swapchain`'s minimised branch
never releases the swapchain it declined to replace, so both of `present()`'s
minimised guards are dead and the state the comment calls temporary has no exit.
`abandon_commands` forgets the colour target's layout on every ordinary frame,
not just on the abandoned one it was written for, so the frame's opening barrier
carries an empty first scope and nothing orders the clear after the previous
frame's present. Both are covered by careful, correct-sounding prose. Neither
is covered by a test.

Behind them is a quieter class: three places where a memory or execution
dependency is simply absent — the read-back's host read, the acquired image's
transition, the index upload — each legal-looking, each invisible on the one
driver this has ever run against. **The validation layers found one of those,
and the frame's edge above, the moment they were asked the right question**
(§2) — which is the finding under the findings: the instrument this backend's
own commit message credits with catching everything it caught was never run
with synchronization validation on.

Then a documentation layer that went stale inside the commit that wrote it,
including the sentence in
[vulkan/device_resources.h:39](../../../engine/render/vulkan/device_resources.h)
that carries the port's largest decision and that this same commit falsified
with a test case it added in the same diff.

None of this is a reason not to ship a backend no shipping configuration selects
yet. All of it is a reason not to believe it works: CI runs `-E
RenderPixelTests` on this preset, so nothing here has ever rasterised anywhere
but one developer's machine, and the whole suite calls `present()` exactly once
— on the line this commit added.

**The one thing to do first is not on the list below.** It is to get
`RenderPixelTests` running against this backend somewhere automated with the
layers on, because the layers are the only instrument that has ever found a
defect in this folder, and this commit's message says so itself.

---

## 2. What was run, and what the layers said

The suite was built and run against this backend at `4d91b91`:

```
ctest --preset x64-debug-vulkan
100% tests passed, 0 tests failed out of 12
```

Twelve of twelve, `RenderPixelTests` included — 34 cases, 336 assertions, all
fifty golden images. That is the state the commit describes and it is accurate.

`RenderPixelTests` was then re-run with a `vk_layer_settings.txt` beside the
executable turning on what the default configuration does not check:

```
khronos_validation.debug_action = VK_DBG_LAYER_ACTION_LOG_MSG
khronos_validation.report_flags = error,warn,perf
khronos_validation.log_filename = stdout
khronos_validation.validate_sync = true
khronos_validation.validate_best_practices = true
```

`log_filename = stdout` is not optional for this: the backend's own messenger
(`device_resources.cpp:123`) forwards to `OutputDebugStringA`, which a shell run
cannot see. **Two errors, both hazards, both on the present path:**

```
Validation Error: [ SYNC-HAZARD-WRITE-AFTER-READ ]
vkQueueSubmit(): WRITE_AFTER_READ hazard detected. vkCmdPipelineBarrier writes
to VkImage 0x1a…, which was previously accessed by vkAcquireNextImageKHR.
… but layout transition does not synchronize with these stages.

Validation Error: [ SYNC-HAZARD-WRITE-AFTER-READ ]
vkQueueSubmit(): WRITE_AFTER_READ hazard detected. vkCmdPipelineBarrier writes
to VkImage 0x15…, which was previously read by vkCmdCopyImage.
No sufficient synchronization is present to ensure that a layout transition does
not conflict with a prior read (VK_ACCESS_2_TRANSFER_READ_BIT) at
VK_PIPELINE_STAGE_2_COPY_BIT.
```

The first is §4.4 — the acquired image's transition issued from `TOP_OF_PIPE`
while the submit waits the acquire semaphore at `TRANSFER`. The second is
**§3.2** — the next frame's opening barrier on the colour image, unordered
against the previous frame's present copy of it, which is precisely what
forgetting the layout produces.

**Both were attributed by filter.** Running only `-tc="CONTRACT: a frame may be
read back and then presented"` produces both; running the other 33 cases
(`-tce=` the same name) produces neither. Every assertion passes in both runs.
So the single case this commit added is the only thing under `ctest` that has
ever executed the present path — the two samples drive it on every frame they
draw, but no test did until this one — and it is silently wrong on it, which is
the exact shape the commit message predicts for this API and then does not check
for.

The rest of the sweep is clean: no other validation errors, and the only
best-practice warnings are `small-dedicated-allocation` on the per-resource
`vkAllocateMemory` calls, which is a known cost of not having a suballocator and
is not a finding.

**What this does not establish.** Synchronization validation sees only paths
that execute, and this suite never minimises, never rebuilds from `present()`'s
out-of-date branch (`device_resources.cpp:1513`, `:1634`), never loses a device
and never runs two frames deep. The two resize cases do rebuild the colour
target and the swapchain through `create_window_size_dependent_resources` —
§3.2's other caller included — and the sweep was clean on them. It also cannot
see a CPU read of mapped memory at all, so §4.3 is untouched by a clean run. A
silent sweep of the other cases is not evidence that they are correct; it is
evidence that this layer, on this driver, reported no hazard in what they
execute.

---

## 3. Must fix

### 3.1 The minimised swapchain: a guard that cannot fire, and a state with no exit — `engine/render/vulkan/device_resources.cpp:1059` and `:1495`

Seven findings reached this branch independently — C1, C2, C3 and C13 in
[all-findings.md](all-findings.md), from four of the five triage areas, plus
G1, G2 and G3, raised by three separate gap probes. C16 and G4 are the second
half of it and are §4.2.

`create_swapchain`'s only early return sets `swapchain_extent_` and returns:

```cpp
if (extent.width == 0u || extent.height == 0u)
{
    // A minimised window. There is nothing to present into and there
    // will be again; present() answers this by doing nothing rather
    // than by refusing, because a shell that keeps drawing while
    // minimised is the ordinary case and not a mistake.
    this->swapchain_extent_ = extent;
    return;
}
```

It sits above `VkSwapchainKHR previous = this->swapchain_;` (`:1110`) and above
the `destroy_swapchain()` at `:1140`, and `destroy_swapchain` is the only place
`swapchain_` is ever assigned `VK_NULL_HANDLE`. So after one successful
creation, a zero extent cannot produce the state `present()` tests for — and
`present()` tests for it twice, at `:1495` and again at `:1523` after
`rebuild_swapchain()`. The second is unreachable outright; the first is live
only where no swapchain exists yet — before the first creation, or after
`handle_device_lost` destroyed it and the recreate found a zero extent.

The failure the refuters walked: minimise a sample.
[window.cpp:387-397](../../../engine/app/window.cpp) turns
`WM_SIZE`/`SIZE_MINIMIZED` into `on_suspending` and forwards no size;
`Application::render` has no suspend gate, so the shell keeps drawing — the
client `:1061` describes. `vkAcquireNextImageKHR` returns
`VK_ERROR_OUT_OF_DATE_KHR`, `rebuild_swapchain()` re-enters the branch above and
changes nothing, the guard at `:1523` is false, the retry acquires from the same
out-of-date swapchain, and `check_vk` at `:1542` throws out of
`Renderer::end_frame` — through `Application::render` and the message pump, with
no handler above `main`. Both refuters noted the same qualification and it is
kept here: that the WSI reports `OUT_OF_DATE` while the client rect is `0x0` is
usual rather than guaranteed. On an implementation that answers `VK_SUCCESS` or
`VK_SUBOPTIMAL_KHR` instead — `:1540` lets `SUBOPTIMAL` past — the outcome is not
a throw but a degenerate blit, because `swapchain_extent_` is now `{0,0}` while
`colour_extent_` is clamped to at least 1, so `:1568`'s equality test sends the
frame down `vkCmdBlitImage` with `dstOffsets[1] = {0,0,1}`. The object is
inconsistent either way and the fix is the same either way.

**The second half is that there is no way back.** `create_swapchain` has exactly
two callers: `rebuild_swapchain` (`:1183`), reachable only from *past*
`present()`'s null guard, and `create_window_size_dependent_resources`
(`:1240`), reached from `create_device`, `handle_device_lost`, or a genuine size
change. `window.cpp`'s restore branch sets `minimized_ = false` and calls
`on_resuming` without ever calling `on_window_size_changed`. So a null swapchain
— the state `:1061` promises "there will be again" — is terminal. Fixing the
first half without this one produces a window that stops drawing when restored
instead of dying when minimised.

**Fix.** Call `destroy_swapchain()` immediately before the early return at
`:1066` — both callers have already run `vkDeviceWaitIdle` (`:1182`, `:1234`),
so it is safe there — and give `present()`'s null-swapchain branch a retry: it
already knows how to ask, and `vkGetPhysicalDeviceSurfaceCapabilitiesKHR` is
what tells it the window came back.

**Why it is a must-fix and not a should-fix.** It is not the throw, which is
driver-conditional. It is that the minimise path's entire written behaviour —
two guards and the comment that names them as the answer — describes code that
cannot run, in the one backend of the five where minimise is not free.

---

### 3.2 `abandon_commands` forgets the layout on every frame, not the abandoned one — `engine/render/vulkan/device_resources.cpp:1488`

Two lenses reached this from opposite ends, one blaming `stage_of(UNDEFINED)`
and one blaming the reset itself; they are one defect. **The validation layers
confirmed it (§2).**

The line is `this->colour_layout_ = VK_IMAGE_LAYOUT_UNDEFINED;`, under
twenty-two lines of comment arguing that it is not tidiness: "The barriers that moved the
colour target into a layout were IN the commands this just threw away, so the
member saying where it is has stopped being true." That premise is true for one
of the two callers. `abandon_commands` is called from `:1237` (the resize, which
then destroys the target anyway) and from `Renderer::Impl::abandon_recording`
(`renderer.cpp:658`), whose only caller is `renderer.cpp:840` — inside
`Renderer::begin_frame`. **Every frame.** And in an ordinary frame nothing was
thrown away: `present()` left `colour_layout_` at `TRANSFER_SRC_OPTIMAL` and
`execute()` submitted the barrier that put it there, so the member is *true* at
the top of the next frame and is discarded regardless.

`transition_colour` (`:1708`) then builds the frame's first barrier as
`image_barrier(colour_image_, colour_layout_, layout)` with
`stage_of(colour_layout_)` as `srcStageMask` — and `stage_of(UNDEFINED)` is
`TOP_OF_PIPE`, which is an empty first synchronisation scope. With
`FRAME_COUNT == 2`, one shared `colour_image_`, and `wait_for_frame` waiting per
frame index, there is nothing left to order frame N+1's clear against frame N's
present-time read of the same image. That is the hazard the layer reports in
§2, in its own words: *"vkCmdPipelineBarrier writes to VkImage 0x15…, which was
previously read by vkCmdCopyImage. No sufficient synchronization is present to
ensure that a layout transition does not conflict with a prior read
(VK_ACCESS_2_TRANSFER_READ_BIT) at VK_PIPELINE_STAGE_2_COPY_BIT."*

The comment closes by explaining why the D3D12 backend cannot have this bug —
its `open_frame` executes the clear immediately — and that part is true. What it
misses is that its own caller list makes the premise false for the path that
runs every frame.

**Fix.** Reset the tracked layout where the tracking actually stops being true:
in the resize caller at `:1237`, and on any path that discards recorded
barriers, rather than unconditionally inside `abandon_commands`. The
alternative — keeping the reset and giving `transition_colour` a real
`srcStageMask` — silences the layer but leaves the member lying about the image,
which is the thing the comment says it will not do.

---

### 3.3 `read_back_buffer` discards `execute()`'s device-lost answer, then uses the device it destroyed — `engine/render/vulkan/renderer.cpp:1221`

Raised by six lenses and, separately, by two gap probes.

[device_resources.h:287](../../../engine/render/vulkan/device_resources.h) is
explicit about what the return value is for: "ANSWERS FALSE WHEN THE DEVICE WAS
LOST, having rebuilt everything on the way out. That is not a courtesy."
`present()` honours it at `:1607`. `read_back_buffer` calls it bare:

```cpp
device_resources.execute();
device_resources.wait_for_gpu();
…
const unsigned char* bytes = static_cast<const unsigned char*>(readback.mapped);
…
destroy_vulkan_buffer(device_resources.device(), readback);
```

If that `execute()` handled a loss, `handle_device_lost` has already destroyed
the `VkDevice` and built a replacement. `readback.mapped` then points into freed
memory, and `destroy_vulkan_buffer` frees a `VkBuffer` and a `VkDeviceMemory`
belonging to the destroyed device *against its replacement*. `VulkanBuffer` is
the reason this is worse than it looks: unlike `VulkanTexture`, which the commit
message rightly makes a point of giving a `shared_ptr` to its device, a
`VulkanBuffer` is a bare aggregate that owns handles and knows nothing about
where they came from.

**Fix.** `if (!device_resources.execute()) { … }` — the read cannot be
completed, and the seam already has an answer for a frame that produced nothing.
Whatever that branch does, it must not touch `readback` afterwards.

---

## 4. Should fix

### 4.1 `timeline_value_` is advanced before the submit that would signal it — `device_resources.cpp:1382`, `:1872`

`execute()` does `timeline_value_++` and *then* `vkQueueSubmit`, with
`check_vk` for everything except `VK_ERROR_DEVICE_LOST`. `wait_for_gpu` and
`try_wait_for_gpu` both wait on that member with `UINT64_MAX`. So any submit
failure that is not a device loss leaves the counter naming a value nothing will
ever signal, and the next wait blocks for ever. `end_upload` (`:1872`) has the
same shape and no `DEVICE_LOST` branch at all. D3D12 does not have this: it
bumps its fence value, calls `Signal`, and returns the failure *before* writing
`frame_fences_[frame_index_]`, and its waits target that array rather than the
raw counter — so the transliteration lost the one line that made the pattern
safe.

### 4.2 Nothing recreates a null swapchain — `device_resources.cpp:1495`

The second half of §3.1, ranked separately by the lenses and as a must-fix by
one probe. Kept here as its own item because it needs its own fix.

### 4.3 The read-back maps and reads with no `TRANSFER_WRITE → HOST_READ` dependency — `renderer.cpp:1224`

After `vkCmdCopyImageToBuffer` and a timeline wait, the host reads
`readback.mapped` directly. A semaphore wait is an execution dependency; it is
not the availability operation the host domain needs, and `HOST_COHERENT` makes
writes visible without making them available. There is no
`VK_PIPELINE_STAGE_HOST_BIT`, no `VK_ACCESS_HOST_READ_BIT` and no
`vkInvalidateMappedMemoryRanges` anywhere in the folder. **This is the one the
sweep in §2 cannot see**, because the layer cannot observe a CPU load, and it is
the path every golden image goes through.

### 4.4 The acquired image's transition is issued from `TOP_OF_PIPE` — `device_resources.cpp:1554`

The barrier moving the swapchain image `UNDEFINED → TRANSFER_DST_OPTIMAL` is one
of four in the folder written by hand rather than through `transition_colour`
— the others are `TRANSFER_DST → PRESENT_SRC` at `:1603` and the two in
`texture_factory.cpp` at `:283` and `:297` — and the only one on an image the
presentation engine touched last. Its `srcStageMask` is `TOP_OF_PIPE` while the
submit waits the acquire semaphore at `TRANSFER`. A semaphore wait's second scope is
the batch's commands at the waited stages *and later*; `TOP_OF_PIPE` is earlier
than all of them, so the transition — which from `UNDEFINED` may discard
contents and re-initialise compression metadata — is not ordered after the
presentation engine finished with the image. **Confirmed by the layer (§2)**,
which names the fix: include the wait stage in `srcStageMask`.

### 4.5 Every `VulkanBuffer` local leaks its buffer and memory on any throw — `texture_factory.cpp:230`

`VulkanBuffer` has no destructor, and the staging copy is created inside the
`try` but named nowhere in the `catch` at `:320` — which frees the view, the
memory and the image and rethrows, stranding a `VkBuffer` and a still-mapped
allocation the full size of the texture. There are five throwing calls between
its creation and its release. The same shape appears with no handler at all at
`renderer.cpp:582` (the index buffer's staging copy) and `renderer.cpp:1184`
(the read-back buffer). The block's own comment states the rule it breaks:
"every failure between the two has to put it back. There is no ComPtr in this
API and no destructor to lean on."

### 4.6 `handle_device_lost` destroys before it waits, and the comment that would have caught it is false — `renderer.cpp:271`

`~Impl` says "The wait is inside `destroy_device_dependent_resources`";
that function contains no wait of any kind. It matters because the *other*
caller has none either: `handle_device_lost` calls `notify_->on_device_lost()`
as its **first** statement (`device_resources.cpp:1275`), which destroys the
pipeline, both samplers, the layouts and the index buffer — five lines before
its own `vkDeviceWaitIdle` (`:1280`). And one route into `handle_device_lost`
is an out-of-memory answer from `try_wait_for_gpu`, not a real loss.

### 4.7 `commands()`'s stated contract is the inverse of this backend — `device_resources.h:275`

"It is not a way to add to what an earlier entry point recorded — every entry
point executes before it returns, so there is never an open buffer between
them." Both `begin_frame` and `submit` return with `recording_ == true`;
`execute()` is called only from `present()` and `read_back_buffer`. The same
folder says so outright at `device_resources.cpp:1485`. The paragraph is D3D12's
protocol transplanted onto a backend that deliberately does not use it — and it
denies the exact hazard class the validation layers caught in this same commit.

### 4.8 The premise of the port's largest decision is false, and this commit made it so — `device_resources.h:39`

"tests/render/pixel_tests.cpp draws fifty-one frames and presents none of them"
is the load-bearing sentence of the engine-owned-colour-target argument. At HEAD
the file says fifty-three, and `pixel_tests.cpp:1767` — added by this commit —
presents one. The same sentence is repeated at
[docs/port/android.md:343](../../port/android.md). The decision it argues for is
right; the fact it argues from is not, and it is the sentence a future reader
will check first.

### 4.9 `render_resources.h`'s lifetime constraint was re-counted onto the wrong side — `render_resources.h:121`

The paragraph is where the `RenderResources`-outlives-`Renderer` rule is
stated — `renderer.h:357`, `application.h:241`, `vulkan/backend.h:78` and
`vulkan/render_resources.cpp:30` all point back at it. This commit changed its
counts from four to five while leaving the hazard at one backend, so it now
reads "it costs
nothing on four of the five backends". Vulkan has exactly D3D12's hazard for
exactly D3D12's reason: `~VulkanTexture` waits for nothing, and the only wait on
the shutdown path is `~Renderer::Impl`. The constraint is load-bearing in two
configurations of five, not one, and `application.h:248` repeats the old count.
This is the paragraph the D3D12 review's §2.3 fix wrote; it was renumbered
rather than re-derived.

### 4.10 The counts the seam header keeps about itself — `renderer.h:490`, `:580`, `tests/render/golden_image.h:14`

Three separate stale-count findings, kept together because they are one habit.
`renderer.h:490` still says a device "can be lost on both of them, which is half
the backends rather than one", fifty lines above a note this same commit updated
to "three of the five". `renderer.h:580`'s `texture_factory.cpp` line-count
list, offered as "the honest measure of what a port owes for content", is stale
on D3D12 by 46 lines (310, not 264) and omits Vulkan, while `:587` asserts
Vulkan's is "shorter than D3D12 anyway" — at 356 lines it is the longest of the
five.
`tests/render/golden_image.h:14` — the file `renderer.h:635` cites as carrying
the golden set's whole argument — still says three rasterising backends, "the
HLSL at two profiles", and "three runs, three configurations": four counts that
were right at `HEAD~1` and that this commit falsified without opening the file.

### 4.11 The resize path inherits half of D3D12's device-lost recovery — `device_resources.cpp:1217`

The comment justifying answer-rather-than-throw points at "the device-lost
branch a few lines below", and one is there: `if (!this->try_wait_for_gpu())`
at `:1223`, which calls `handle_device_lost` and returns. What did not come
across is the *second* recovery the sentence it was copied from depends on.
[d3d12/device_resources.cpp:315](../../../engine/render/d3d12/device_resources.cpp)
closes with "Same recovery, reached from the wait as well as from
ResizeBuffers", and `:339` is that other end — `DXGI_ERROR_DEVICE_REMOVED` out
of `ResizeBuffers`, answered rather than thrown. Here everything after the wait
goes through `check_vk`, `vkCreateSwapchainKHR` at `:1141` included, so a
removal that surfaces from the rebuild rather than from the wait throws out of a
window procedure — the exact outcome the paragraph exists to prevent, and the
one the D3D12 review's §2.2 was about.

### 4.12 The 1.2 floor's platform argument contradicts its own numbers — `device_resources.cpp:29`

The comment that sets the version floor concludes that a phone from 2019 clears
it, from premises stating that Android shipped Vulkan 1.1 in Android 10 — a
device `select_physical_device` (`:554`) then refuses by name. This is the
number that decides which phones this backend can run on, argued wrongly in the
file that sets it, in the port whose stated reason for existing is Android.

### 4.13 Five comments point at a `replay()` that exists only in `gl/` — `backend.h:67`

Including the two that name where the y flip is decided. The real sites are
inside `Renderer::submit` (`renderer.cpp:1039`, `:1082`). A reader following
either lands in another backend's folder — which
`check_engine_includes.cmake` forbids the code from doing and cannot stop a
comment from doing.

### 4.14 `frame_begun` is justified by the opposite of what happens here — `backend.h:193`

"asking the command buffer whether there is one answers no" — but `begin_frame`
ends in `open_frame`, whose first act reaches `commands()` and begins the
buffer. The D3D12 clause that made the sentence true was dropped in
transliteration.

### 4.15 The CI step that installs the SDK cannot fail — `.github/workflows/ci.yml:135`

`Start-Process -Wait` without `-PassThru` discards the installer's exit code,
and `VULKAN_SDK` is exported unconditionally on the next line. A failed install
is a green step, and the two consumers that then fail — `find_package(Vulkan)`
and `compile_hlsl_to_spirv`'s deliberately-named `VULKAN_SDK` error — both blame
a cause that is not the cause, which is the opposite of what T6 asked those
messages to do. The same section's justification (`:65`) says that without the
loader "none of the eleven other test executables in this preset would start at
all"; `MattMathTests` and `LineSweeperTests` link no engine, so it is nine.

---

## 5. Minor

- **`engine/CMakeLists.txt:184`** — `Vulkan::Vulkan` is linked `PUBLIC`, putting
  the SDK's include directory on the command line of every sample, test and
  downstream client. It is the first backend whose link line carries a header
  path, and it is a hole `check_engine_includes.cmake` cannot see.
- **`device_resources.cpp:1089`** — the surface format falls back to
  `surface_formats[0]` unchallenged while `present()` chooses `vkCmdCopyImage`
  over `vkCmdBlitImage` on extent alone, so a surface offering RGBA but not BGRA
  would present every frame with red and blue swapped while `read_back_buffer`
  and every golden image stayed clean. Latent on Win32; live on the platform
  this backend exists for. Both refuters killed it, splitting only on what the
  residue is worth — see §6.
- **`texture_factory.cpp:320`** — the `catch` frees the image and its memory
  while the upload batch copying into them may still be executing, because the
  only thing establishing completion is `end_upload`'s post-submit wait, which
  is a throwing call.
- **`sprite.hlsl:68`** — still says the negative y term is "the single line in
  this engine where that is decided" and that clip space's y runs up. The third
  reader this commit added has clip y running down and decides it a second time
  in `renderer.cpp:1051`; the two cancel.
- **`sprite.hlsl:78`** — `VertexIn`'s member declaration order is now a silent
  Vulkan ABI term (dxc assigns SPIR-V locations in declaration order;
  `renderer.cpp:386` binds by `offsetof` with no semantic anywhere), stated only
  inside the backend, in a file whose own header says no backend difference
  reaches it.
- **`renderer.h:473`** — "Presenting discards the back buffer's contents" is
  stated as a flat term of the seam and is false on the backend this commit
  added, whose entire point is that the engine owns the image. `pixel_tests.cpp:1751`,
  added here, already says the term is backend-specific.
- **`d3d12/device_resources.cpp:50`** — "ONE OF THE TWO DESTRUCTORS IN
  `engine/render/` THAT HAS TO SYNCHRONISE WITH A GPU" now enumerates a set of
  four. The Vulkan file's own copy at `:372` dropped the count rather than
  widen the sentence that names the set.
- **`vulkan/render_resources.cpp:94`** — "NO `descriptor_slot` HERE, WHICH IS
  THE ONE LINE THAT DIFFERS FROM THE D3D12 FILE" is false four ways: the missing
  slot is fourteen lines, and this file also carries a 35-line
  `~VulkanTexture`, a `size()`, and an `engine/math/vector2f.h` include with the
  `using namespace mattmath;` it brings, none of which the D3D12 file has.
  `texture_factory.cpp:349` cites it as authority.

---

## 6. Unresolved

**6.1 The severity of the minimise throw.** §3.1's dead guard and false comment
are structural and certain. The *crash* needs `vkAcquireNextImageKHR` or
`vkQueuePresentKHR` to answer `OUT_OF_DATE` on a minimised Win32 surface, which
is ordinary but not guaranteed, and nobody ran it. Minimising a sample under the
layers would settle it in a minute and was not done here because this review's
run budget went to the present path instead.

**6.2 The format fallback (§5) is two findings wearing one line.** Both
refuters refuted it — one at `not-a-finding`, one at `minor` — and both granted
the copy-versus-blit mechanism while putting its only real platform, Android,
outside anything this tree builds. What neither settled, and what this review
does not decide, is whether a defect that is latent on every configuration this
repository compiles counts against the port that exists for the platform where
it is live. The same family holds `compositeAlpha` (`:1123`), a bare constant
never checked against `supportedCompositeAlpha` in the one function that
validates every other surface property it uses, and `preTransform` (`:1122`),
which takes `currentTransform` from the capabilities probe and is then never
applied by the blit that presents.

**6.3 The index buffer's missing barrier.** `renderer.cpp:596` uploads the
sprite index buffer with no `TRANSFER_WRITE → INDEX_READ` dependency. Whether
`end_upload`'s full host stall substitutes for it is the open question; the
texture path emits a `TRANSFER → FRAGMENT_SHADER` barrier at
`texture_factory.cpp:297` despite the identical stall, and that asymmetry is
the real signal. It was
ranked below the verification budget, so it is in §7 rather than §4 — but of the
sixty-five there, this is the one to check first.

**6.4 How thinly `FRAME_COUNT == 2` is reached.** `frame_index_` advances only
inside `present()` (`device_resources.cpp:1502`, `:1526`, `:1648`), and the
suite presents exactly once — `pixel_tests.cpp:1767`, in the case this commit
added — so the ring's second half is entered by that case's second frame and by
nothing else, and never wraps. That single crossing is the state §3.2's hazard
needs, and it is why §2 caught it there and nowhere else. But one frame on slot
1, behind the full `wait_for_gpu` the read-back before it performs, is not two
frames in flight: nothing in this tree records frame N+1 while frame N is still
executing, so the mechanism the frame count exists for is untested even now.
D3D12's equivalent gap was accepted in that review only because it was
disclosed; nothing in `engine/render/vulkan/` discloses this one.

---

## 7. Not verified

Sixty-five findings were ranked, listed and never checked — no refuter read
them, no adjudicator saw them, and the severity on each is the raising agent's
own. They are enumerated at the end of
[all-findings.md](all-findings.md#never-verified--ranked-below-the-budget).

They are questions, not findings. The D3D12 review's §6 is the precedent for
what happens to a list like this: two of its nine were counts a reader could
check and went in with the fixes; the other seven were behaviour and are still
open. The same split applies here — roughly a third of these sixty-five are
counts and comment claims that can be settled by opening the file, and the rest
need a device.

---

## 8. Refuted

Eleven findings were raised, ranked, and then killed — nine by both of their
refuters, one by the adjudicator its split went to, and one (R11) on a split
that was never adjudicated and is filed here on the second refuter's reading.
They are in [all-findings.md](all-findings.md) with the reasoning, so that
nobody spends the budget re-deriving them. The four worth knowing about:

- **A second `OUT_OF_DATE` from the retry acquire throws out of `end_frame`.**
  Refuted as written — it is the same defect as §3.1 approached from the
  consequence rather than the cause, and the fix there answers it.
- **The Vulkan SDK is fetched from an unpinned "latest" URL.** Real, and
  deliberate: refuted because CI pins nothing else either, and a floating SDK is
  the same bet the Windows SDK on the runner already represents.
- **The debug messenger discards severity and only prints.** Refuted: the
  callback is documented as forwarding, `VK_FALSE` is correct, and the argument
  that it should abort mistakes a development instrument for a test.
- **`same_viewport` duplicates `Viewport::operator==`.** Refuted after an
  adjudicator: the two do compare the same six fields, but
  `Viewport::operator==` has no callers anywhere in the repository — its only
  caller is `operator!=`, which has none — so there is nothing for this copy to
  fall behind, and replacing an inlined compare with a cross-TU call per sprite
  is the trade T8 governs.

Three more refutations killed findings against the *new test case*'s comments —
two that "the other three had nothing to answer" overstates, one that "the only
observable half" is false. Five of those six refuters found the comment
defensible as written; on R11 the sixth confirmed it at `minor`, and that split
is the one nothing adjudicated. They are worth reading before anyone edits that
block.

---

## 9. Coverage

**Where this review is thin, in the order that matters.**

1. **Include discipline was not reviewed.** The lens assigned to the walls —
   the one axis in this repository that is a build failure rather than a
   judgement — died mid-run and returned nothing. Nothing here checks that
   `engine/render/vulkan/` is not reached from outside itself, that
   `check_engine_includes.cmake`'s second pass still catches what it claims
   with a fifth folder present, or that `vulkan.h` does not drag `windows.h`
   somewhere new.
2. **No pixel was compared.** No golden image was regenerated, decoded or
   diffed. That the Vulkan backend reproduces the fifty images rests on the run
   in §2 passing, which is a weaker statement than a human looking at them —
   and the two new PNGs in this commit were generated on D3D11 and reviewed
   there.
3. **The Android-facing findings are the ones this review can least test.**
   Format selection, `preTransform`, `compositeAlpha`, the 1.2 floor: these are
   why the backend exists, and no Android target exists in the tree, so every
   one of them is reasoning about a surface nobody here has.
4. **Sixty-five findings were never verified** (§7), and several lenses —
   threading, descriptors, the shader, instance and device creation — produced
   nothing that survived refutation. This review cannot tell you whether those
   axes are clean or whether the budget ran out before they were checked.
5. **The specification adjudication behind §4.3 and §4.4 was done from
   knowledge of the synchronisation chapter, not from the text.** Both are
   well-known idioms and §2 independently confirms the second, but a Khronos
   reader should confirm the exact wording before either fix is designed.
6. **The sweep in §2 is one machine, one driver, one run**, on the only paths
   thirty-four test cases happen to execute. It is worth more than reading
   alone; it is worth much less than CI running it.
