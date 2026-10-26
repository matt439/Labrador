# Vulkan candidate adjudication

Live status for the frozen review's
[65 never-verified candidates](all-findings.md#never-verified--ranked-below-the-budget),
read alongside [README section 7](README.md#7-not-verified).
The original list has no item IDs: these ordinals preserve its existing order,
not new findings or severity rankings. Each original bullet is copied verbatim,
including its historical source location. Do not treat its old line numbers
as current or rewrite the frozen review as an item is answered.

## Scope and evidence

Ordinals **1-40** were checked against current source and test declarations on
2026-10-26. Sources were inspected in the working tree, including the current
buffer-cleanup implementation; "already resolved" records that implementation
rather than attributing it to a new commit. The initial adjudication was a
source reading. Dated follow-ups below separately record the actual regression
and injected-failure runs performed while resolving candidates. Source reading
alone supplies no GPU/browser, network or failure-injection acceptance; an error
path being present is not proof a real device has taken it.

"Confirmed" is limited to its described source condition. "Unmeasured" retains
a runtime/platform question rather than making it a reproduced defect. A
resolved stale comment or duplicate is not a new bug. Candidate 3's narrow
fix preserves the original error and adds no public fault-injection seam.

## Adjudicated candidates

### Candidate 01

Original, copied verbatim:

- **[should-fix]** `engine/render/vulkan/device_resources.cpp:1688` — try_wait_for_gpu cannot see a lost device whose outstanding work already finished, so the resize path's answer-rather-than-throw guard falls through into calls that throw out of a window procedure

**Already resolved for the original shortcut; device-removal recovery remains
unmeasured.** `DeviceResources::try_wait_for_gpu` in
[device_resources.cpp](../../../engine/render/vulkan/device_resources.cpp)
always passes a nonzero timeline value to `vkWaitSemaphores`. There is no
completed-value shortcut; its early return covers absent state or zero
submitted value. The old shortcut premise is absent, but actual removal after
completed work, including subsequent size-resource creation errors, still
needs real-device/fault evidence. No new recovery defect is established here.

### Candidate 02

Original, copied verbatim:

- **[should-fix]** `engine/render/vulkan/renderer.cpp:596` — The sprite index buffer is uploaded with no TRANSFER_WRITE -> INDEX_READ barrier, unlike the texture upload beside it and unlike the D3D12 backend it transliterates

**Already resolved.** `Renderer::Impl::create_device_dependent_resources` in
[renderer.cpp](../../../engine/render/vulkan/renderer.cpp) follows the index
copy with a whole-buffer TRANSFER_WRITE to INDEX_READ barrier, at TRANSFER to
VERTEX_INPUT stages, before `end_upload`. Do not file the missing barrier
again. Its presence is source evidence; executed validation is separate.

### Candidate 03

Original, copied verbatim:

- **[should-fix]** `engine/render/vulkan/device_resources.cpp:1869` — end_upload frees its command buffer on one of three failure paths and leaks it on the other two, into a transient pool nothing ever resets

**Fixed by `3096a7a`; source-reviewed, runtime failure injection untested.** At the start of this audit,
only the `vkEndCommandBuffer` failure still lacked cleanup in
`DeviceResources::end_upload` in
[device_resources.cpp](../../../engine/render/vulkan/device_resources.cpp).
Submit and wait failure cleanup already existed. The ending check now frees
the command buffer from `upload_pool_` and rethrows the original exception.
The matching guard in `begin_upload` frees its successfully allocated command
buffer if the following `vkBeginCommandBuffer` check throws, then rethrows.
Normal begin, submit, timeline advancement and waiting are unchanged. These
are bounded guards around begin/end recording, not a claim that all possible
upload failures were injected. No begin/end-recording failure was exercised;
source review and successful normal rendering do not establish those paths.

### Candidate 04

Original, copied verbatim:

- **[should-fix]** `engine/render/vulkan/device_resources.h:274` — commands()'s documented contract states the opposite of this backend's design: it says every entry point executes before it returns and no buffer is ever left open between them

**Already resolved.** `DeviceResources::commands` in
[device_resources.h](../../../engine/render/vulkan/device_resources.h)
explicitly distinguishes Vulkan from D3D12: begin/submit retain the open
recording; present/read-back execute it. The implementations of `commands`,
`execute`, `present` and `read_back_buffer` follow that division.

### Candidate 05

Original, copied verbatim:

- **[should-fix]** `engine/render/vulkan/backend.h:193` — The stated justification for Renderer::Impl::frame_begun is inverted on this backend: between begin_frame and the first set_view_count the command buffer IS recording

**Already resolved.** The `Renderer::Impl::frame_begun` comment in
[backend.h](../../../engine/render/vulkan/backend.h) says `open_frame` leaves
the buffer open and read-back can make recording false mid-frame, so the frame
interval is tracked separately. It no longer applies D3D12's recording-state
argument as Vulkan's justification.

### Candidate 06

Original, copied verbatim:

- **[should-fix]** `engine/render/vulkan/backend.h:67` — Five comments in engine/render/vulkan/ cite a function replay() that does not exist in this backend

**Already resolved.** No `replay(` references remain in Vulkan's `.cpp` or
`.h` files. The recording-shape description in
[backend.h](../../../engine/render/vulkan/backend.h) correctly names `submit()`
as the operation replaying the CPU recordings.

### Candidate 07

Original, copied verbatim:

- **[should-fix]** `engine/render/texture_format.h:42` — texture_format.h and device_resources.h claim the Vulkan backend refuses a BC-less device at device selection so the refusal names the content set; the refusal is at the first texture and names that one file

**Already resolved.** [texture_format.h](../../../engine/render/texture_format.h)
locates refusal in the texture factory and says device selection asks whether
the feature is available. `select_physical_device` records
`textureCompressionBC` in `owner_->block_compression`; `to_vk_format` in
[texture_factory.cpp](../../../engine/render/vulkan/texture_factory.cpp)
refuses a compressed texture by name when it is false. The code and current
contract agree on which operation refuses it.

### Candidate 08

Original, copied verbatim:

- **[should-fix]** `engine/render/vulkan/texture_factory.cpp:171` — The image-format probe is never asked about the extent, so an oversized texture reaches vkCreateImage as a valid-usage violation while the comment above it claims a size the device will not take is caught here by name

**Already resolved.** `add_texture_asset` in
[texture_factory.cpp](../../../engine/render/vulkan/texture_factory.cpp)
compares the requested extent with queried `limits.maxExtent` before
`vkCreateImage`, and also refuses zero extents.
[vulkan_texture_tests.cpp](../../../tests/render/vulkan_texture_tests.cpp)
contains `Vulkan refuses queried image extent overflow before creating an
image`, covering width/height overflow and the named error. Its source was
inspected here; no new test execution is claimed.

### Candidate 09

Original, copied verbatim:

- **[should-fix]** `engine/render/vulkan/device_resources.h:39` — device_resources.h's premise for the whole colour-target decision miscounts pixel_tests' frames and says none is presented, in the commit that added the case which presents one

**Already resolved.** The colour-target argument in
[device_resources.h](../../../engine/render/vulkan/device_resources.h)
says pixel tests present almost none of their frames, names the presenting
cases and avoids a numeric frame count. Its premise is the legal run of
unpresented frames, not absence of all presentation from the suite.

### Candidate 10

Original, copied verbatim:

- **[should-fix]** `engine/render/renderer.h:473` — renderer.h's read_back_buffer contract rests on a premise the fifth backend falsifies, and its swizzle enumeration silently omits that backend

**Already resolved.** The read-back contract in
[renderer.h](../../../engine/render/renderer.h) promises RGBA irrespective of
storage and delegates conversion details to each backend. Its interval refers
conditionally to backends where presentation discards the buffer; the false
universal premise and incomplete swizzle roster are absent.

### Candidate 11

Original, copied verbatim:

- **[should-fix]** `engine/render/renderer.h:586` — Two comments assert the Vulkan texture factory is shorter than D3D12's; it is the longest of the five by every measure

**Already resolved.** The length comparison is absent from
[renderer.h](../../../engine/render/renderer.h).
[SEAM.md](../../../engine/render/SEAM.md), section 4, and
[texture_factory.cpp](../../../engine/render/vulkan/texture_factory.cpp)
correctly call Vulkan longest and give its ownership/API rationale instead
of depending on a line-count table.

### Candidate 12

Original, copied verbatim:

- **[should-fix]** `engine/render/vulkan/device_resources.cpp:484` — create_instance's failure text names a missing loader for a failure only a machine with a working loader can reach, and the message written for the GPU-less build machine is never printed

**Reproduced and corrected diagnostic; synthetic return codes only.**
At baseline `78974a0`, `DeviceResources::create_instance` returned the same
"There is no usable Vulkan loader" diagnosis for injected
`VK_ERROR_OUT_OF_HOST_MEMORY`, `VK_ERROR_EXTENSION_NOT_PRESENT` and
`VK_ERROR_INCOMPATIBLE_DRIVER`. All three retained `std::runtime_error` and
the result name, but failed the operation-specific diagnostic assertion.
[device_resources.cpp](../../../engine/render/vulkan/device_resources.cpp)
now uses the existing `check_vk` helper to name instance creation and its
result without assigning an unsupported cause. The separate no-device
enumeration diagnostic is unchanged.

Follow-up on 2026-10-26:
[vulkan_initialization_tests.cpp](../../../tests/render/vulkan_initialization_tests.cpp)
uses executable-local forwarding to the installed DLL export. A successful
initialization proves one forwarded instance call. Each injected subcase
observes one call, one synthetic failure, no forwarding and an initially
null output left untouched. The scope ends before renderer destruction.
The baseline reports 15 passing assertions and three message failures;
the corrected test checks the exact message, exception type and call counts.
It forces no allocation exhaustion or device loss and adds no engine API.

All five native render checks and all four raster pixel suites pass. Vulkan
passes 163 headless render cases and 55 pixel cases with shell-visible
synchronization validation: zero errors/hazards and 741 advisory lines.
Candidate 16's injected cleanup and candidate 56's warmed Debug CRT
allocation regression remain passing; four CMake lints and 21 website
contract tests pass. This does not test actual OOM, missing-DLL startup,
retry/recovery or a loader/ICD failure matrix. The historical "never printed"
claim remains unverified.

### Candidate 13

Original, copied verbatim:

- **[should-fix]** `engine/render/vulkan/renderer.cpp:1153` — renderer.cpp:1153 claims the restored layout is what makes a second submit in one frame legal, but a second submit destroys the VkBuffers the first submit's recorded bindings and descriptors already name

**Already resolved.** `Renderer::submit` in
[renderer.cpp](../../../engine/render/vulkan/renderer.cpp) returns immediately
when `frame_submitted` is set, before reserving buffers. Its final-layout
comment explains that a second submit is a no-op and tracking serves
read-back. Repeated submit cannot reach the described buffer destruction.

### Candidate 14

Original, copied verbatim:

- **[should-fix]** `engine/render/renderer.h:370` — renderer.h's begin_frame paragraph still says two backends hold a frame in a vector and enumerates three kinds of reset, when the Vulkan backend makes it three and four

**Already resolved.** The `begin_frame` contract in
[renderer.h](../../../engine/render/renderer.h) delegates reset rationale to
[SEAM.md](../../../engine/render/SEAM.md), section 6. That section accounts for
five backends, GL/null/Vulkan vector recording and Vulkan's additional pool
and layout reset. The stale public-header roster is absent.

### Candidate 15

Original, copied verbatim:

- **[minor]** `engine/render/vulkan/device_resources.cpp:321` — create_vulkan_buffer leaks the VkBuffer and the VkDeviceMemory when vkBindBufferMemory or vkMapMemory fails, though the line above it cleans up carefully

**Already resolved in the current buffer-cleanup implementation.**
`create_vulkan_buffer` in
[device_resources.cpp](../../../engine/render/vulkan/device_resources.cpp)
wraps memory selection/allocation/binding/mapping in a `try`. Its catch calls
`destroy_vulkan_buffer(owner.device, result)` and rethrows. That helper unmaps
if necessary, destroys the buffer and frees memory; successful allocation and
mapping outputs enter `result` only after success. This is current source,
not a new fault-injection run or a newly unresolved leak.

### Candidate 16

Original, copied verbatim:

- **[minor]** `engine/render/vulkan/renderer.cpp:1000` — Renderer::submit() opens a render pass with no unwind path, so a throw from allocate_descriptor_set leaves the frame's buffer with an active pass that end_frame then records transfers into

**Reproduced and fixed render-pass unwind gap; synthetic failure only.**
At baseline `0e0934a`, the unchanged production `Renderer::submit` began a
render pass, then returned the original descriptor-allocation exception with
one begin and zero ends when the first or second allocation was made to fail.
[vulkan_submit_failure_tests.cpp](../../../tests/render/vulkan_submit_failure_tests.cpp)
observes the actual renderer calls through executable-local Vulkan forwarding
symbols. Its cleanup closes the defective baseline pass only after measuring
the failed assertions; it does not present unbalanced commands or force real
allocation exhaustion.

[renderer.cpp](../../../engine/render/vulkan/renderer.cpp) now ends the pass
in a catch and rethrows the original exception. Submission flags, buffers,
descriptors and layout policy are unchanged. Earlier draws are not rolled
back, and a failed submission is not replayed. Both injected subcases preserve
the exact exception and balance the pass, then complete read-back/presentation
and check the next frame's full analytic RGBA image without new goldens.

Follow-up on 2026-10-26: focused render checks pass on all five native
backends, including full pixel suites on the four rasterizers. Vulkan passes
163 headless render cases and 51 pixel cases with the review's shell-visible
synchronization and best-practices settings: zero validation errors or
synchronization hazards, with 718 advisory lines. This verifies injected API
failure cleanup and successful subsequent drawing on this run; it supplies
no genuine OOM, device-loss, atomic-frame or physical-platform acceptance.

### Candidate 17

Original, copied verbatim:

- **[should-fix]** `engine/render/vulkan/device_resources.cpp:29` — The 1.2 floor's platform argument is refuted by its own numbers: it cites Vulkan 1.1 on Android 10 and then calls the floor a phone from 2019, a device select_physical_device refuses by name

**Already resolved.** The API-floor rationale in
[device_resources.cpp](../../../engine/render/vulkan/device_resources.cpp)
explicitly says Android 10/2019 does not clear Vulkan 1.2 and names the extra
synchronization work required to support it. `select_physical_device` still
refuses older API versions by name. External platform support was not
reverified in this source-only adjudication.

### Candidate 18

Original, copied verbatim:

- **[minor]** `engine/render/vulkan/device_resources.cpp:544` — Device scoring ties a software CPU device with an integrated GPU, so the claimed parity with D3D12's adapter walk - which excludes software adapters outright - does not hold

**Confirmed source discrepancy; selection impact unmeasured.**
`select_physical_device` in
[device_resources.cpp](../../../engine/render/vulkan/device_resources.cpp)
gives discrete GPUs score 2 and everything else 1, retaining the first tie.
Its D3D12-parity comment overstates agreement: the adapter walk in
[d3d12/device_resources.cpp](../../../engine/render/d3d12/device_resources.cpp)
explicitly excludes software adapters. No mixed CPU/iGPU system was exercised
and this adjudication makes no selection-policy change.

### Candidate 19

Original, copied verbatim:

- **[minor]** `engine/render/vulkan/device_resources.cpp:1123` — compositeAlpha is hardcoded without checking supportedCompositeAlpha, in the one function that validates every other surface property it uses

**Unmeasured portability question; unchecked assignment confirmed.**
`create_swapchain` in
[device_resources.cpp](../../../engine/render/vulkan/device_resources.cpp)
sets `VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR` without inspecting
`supportedCompositeAlpha`. This is already the platform-surface question in
[review section 6](README.md#6-unresolved), tracked as **REV#5** in
[TODO.md](../../../TODO.md). Keep its existing Android re-entry gate; this is
not a duplicate Windows failure or evidence of tested Android surfaces.

### Candidate 20

Original, copied verbatim:

- **[minor]** `engine/render/vulkan/device_resources.cpp:490` — The debug messenger is created after vkCreateInstance and destroyed before vkDestroyInstance, so the validation layer is silent for exactly the two instance calls this file makes

**Refuted as an absolute silence claim; application callback gap confirmed.**
`create_instance` in
[device_resources.cpp](../../../engine/render/vulkan/device_resources.cpp)
creates the instance without a messenger create-info in `pNext` and creates
the application messenger afterward. `VulkanDevice` destroys that messenger
before instance destruction. The two calls lack this application's callback,
but [review section 2](README.md#2-what-was-run-and-what-the-layers-said)
configures independent validation-layer stdout logging. Callback absence does
not establish layer-wide silence. No instance-callback/logging comparison was
run for this candidate; candidate 12's synthetic return codes do not test
creation/destruction logging.

### Candidate 21

Original, copied verbatim:

- **[minor]** `engine/render/vulkan/device_resources.cpp:1122` — preTransform is set to currentTransform without the blit ever applying that transform, and docs/port/android.md claims rotation is already answered inside present()

**Unmeasured portability question; assignment confirmed.** `create_swapchain`
in [device_resources.cpp](../../../engine/render/vulkan/device_resources.cpp)
assigns `capabilities.currentTransform` to `preTransform`. The present copy/blit
uses image extents and offsets without an explicit orientation transform.
[android.md](../../port/android.md), section 3.5, currently makes the narrower
claim that `VK_ERROR_OUT_OF_DATE_KHR` is handled inside `present`; it does not
establish correct rotated pixels on an Android surface. Keep the existing
**REV#5** platform-surface gate with candidate 19. No rotated surface, Android
device or specification adjudication was exercised here, so this is not a
reproduced rotation defect.

### Candidate 22

Original, copied verbatim:

- **[minor]** `engine/render/vulkan/device_resources.h:139` — uniform_alignment is documented as the driver's required offset multiple, but the code stores max(minUniformBufferOffsetAlignment, 16) and nothing records that the 16 is what keeps transform slots from overlapping

**Corrected in the current comments; no allocation defect established.**
`VulkanDevice::uniform_alignment` in
[device_resources.h](../../../engine/render/vulkan/device_resources.h) now
describes the transform slot stride as the maximum of the driver's required
offset alignment and the four-float payload. The payload floor prevents
adjacent slots overlapping on a smaller-alignment device. This matches
`select_physical_device` in
[device_resources.cpp](../../../engine/render/vulkan/device_resources.cpp)
and `Renderer::submit`'s per-run stride and four-float write. No layout or
allocation behavior changed, and no small-alignment device was exercised.

### Candidate 23

Original, copied verbatim:

- **[minor]** `engine/render/vulkan/device_resources.cpp:802` — create_render_pass says the transfer clear costs one image barrier per frame; open_frame records two, and renderer.cpp's comment on the same decision says two

**Corrected in the current comments.** `DeviceResources::create_render_pass` in
[device_resources.cpp](../../../engine/render/vulkan/device_resources.cpp)
now names the two transitions around the clear: to TRANSFER_DST and back to
COLOR_ATTACHMENT. That matches `Renderer::Impl::open_frame` in
[renderer.cpp](../../../engine/render/vulkan/renderer.cpp), which calls
`transition_colour`, clears and transitions back. This corrects the rationale;
no barrier was removed and no new synchronization execution is claimed.

### Candidate 24

Original, copied verbatim:

- **[minor]** `engine/render/vulkan/renderer.cpp:1085` — The per-run descriptor-pool comment points at d3d12/render_resources.cpp for D3D12's fixed heap capacity, which that file does not carry

**Already resolved in current documentation.** Vulkan's `Renderer::submit`
comment still points at
[d3d12/render_resources.cpp](../../../engine/render/d3d12/render_resources.cpp),
and that file explicitly describes slots "out of 256", the
release/device-recreation rule and reuse on reloading an existing name.
It carries the referenced rationale even though the capacity constant is
declared elsewhere. There is no current missing-reference defect here.

### Candidate 25

Original, copied verbatim:

- **[minor]** `tests/render/golden_image.cpp:254` — golden_image.cpp still says a broken PNG round trip would fail 39 times over; there are fifty golden frames, two of them added by this commit

**Already resolved.** The codec rationale in
[golden_image.cpp](../../../tests/render/golden_image.cpp) now says a broken
round trip would fail once per image in the set, without a fixed count. The
codec implementation is shared lodepng encode/decode. Other counts describing
a particular recorded measurement are not a current golden-image roster and
were not remeasured here.

### Candidate 26

Original, copied verbatim:

- **[minor]** `engine/render/vulkan/texture_factory.cpp:56` — The b4g4r4a4 refusal says this is the third backend to refuse it; only GL refuses it, so this is the second - and the same block's "two of the six" is four of the six

**Corrected in the current comments.** `to_vk_format` in
[texture_factory.cpp](../../../engine/render/vulkan/texture_factory.cpp)
names two refusal reasons, avoiding an unconditional count of refused enum
entries: the block-compressed formats require a device feature and b4g4r4a4
requires a conversion. It also correctly calls Vulkan the second backend
refusing b4g4r4a4. The existing switch still refuses that format always and
BC1/BC2/BC3 when `block_compression` is false. No format support changed and
no new BC-less-device execution is claimed.

### Candidate 27

Original, copied verbatim:

- **[minor]** `engine/render/vulkan/device_resources.h:278` — DeviceResources::recording() has no caller anywhere in the tree, and its only documented use is a comment that describes behaviour it does not have

**Removed the unused internal accessor; the behavior rationale is resolved.**
`DeviceResources::recording()` was removed from
[device_resources.h](../../../engine/render/vulkan/device_resources.h)
after checking current engine, tests, samples, benchmarks, tools and website
sources for callers. Test harnesses' unrelated `recording()` methods are
different symbols and remain. The `frame_begun` comment in
[backend.h](../../../engine/render/vulkan/backend.h) explains why command-buffer
recording state cannot represent the frame interval. The internal recording
flag and separate frame tracking are unchanged. Candidate 40 is the same
accessor cleanup, not a second behavioral defect. This source check does not
establish new runtime lifecycle acceptance.

### Candidate 28

Original, copied verbatim:

- **[minor]** `engine/render/vulkan/backend.h:55` — vulkan/backend.h says the seam's parallelism axis is views "precisely because" a view's vertices are built on the CPU, which inverts the reason PHILOSOPHY.md and renderer.h give

**Corrected in the current rationale; no threading behavior changed.** The
recording-shape paragraph in
[backend.h](../../../engine/render/vulkan/backend.h) now distinguishes this
backend's CPU recording choice from the engine's parallelism axis. Each
worker records quads into its own view's vectors, using shared quad arithmetic,
and submit alone turns the recordings into GPU commands. The paragraph
connects that choice to independently writable recording targets, and view
parallelism to drawing's shared reads while update writes, matching
[PHILOSOPHY.md](../../design/PHILOSOPHY.md), Simulation and rendering and
Performance. No race, different parallelism axis or new concurrent execution
result is established by the comment correction.

### Candidate 29

Original, copied verbatim:

- **[minor]** `engine/render/vulkan/renderer.cpp:445` — The rasterisation-state comment justifies cullMode NONE with a clause that contradicts its own first sentence and sprite_geometry.cpp: a flip cannot change a sprite's winding

**Corrected the residual comment contradiction.** The rasterizer-state comment
in [renderer.cpp](../../../engine/render/vulkan/renderer.cpp) now relates any
face convention to the negative-height viewport, retaining the correct fact
that a sprite flip changes texture coordinates without changing corner
winding. `build_quad` in
[sprite_geometry.cpp](../../../engine/render/sprite_geometry.cpp) applies
`mirror_bits` only to texture-coordinate indexing, not positions. The cull mode
and pipeline state are unchanged; no new culling behavior is claimed.

### Candidate 30

Original, copied verbatim:

- **[should-fix]** `engine/render/renderer.h:370` — renderer.h's begin_frame and set_view_count paragraphs still count two vector-recording backends where there are three, so both enumerations account for four of five and the header now contradicts d3d12/renderer.cpp, which this commit did fix

**Already resolved; duplicate of candidate 14.** The current frame and view
contracts in [renderer.h](../../../engine/render/renderer.h) lack those
backend-count enumerations. Reset rationale is delegated to
[SEAM.md](../../../engine/render/SEAM.md), section 6, which names
GL/null/Vulkan vector recording and the five-backend reset map.

### Candidate 31

Original, copied verbatim:

- **[should-fix]** `engine/render/renderer.h:490` — renderer.h says a device can be lost on "half the backends" and cites a note in the same file that this commit rewrote to say three of five

**Already resolved.** `DeviceNotify` in
[renderer.h](../../../engine/render/renderer.h) explains the common caller
responsibility while saying the ways a device may be lost vary by backend,
and refers its rationale to [SEAM.md](../../../engine/render/SEAM.md),
section 8. The false "half" roster is absent. This is the resolved device-loss
roster question, not a newly open backend omission.

### Candidate 32

Original, copied verbatim:

- **[should-fix]** `engine/render/vulkan/device_resources.h:39` — vulkan/device_resources.h's frame count is stale and its "presents none of them" premise was falsified by the test case this same commit added - in the bullet the header calls the largest decision in the port

**Already resolved; duplicate of candidate 9.** The current colour-target
explanation in
[device_resources.h](../../../engine/render/vulkan/device_resources.h)
says pixel tests present almost none of their frames and identifies the
presenting cases. It no longer depends on a fixed frame count or complete
absence of presentation.

### Candidate 33

Original, copied verbatim:

- **[minor]** `engine/render/vulkan/backend.h:67` — Five comments in three files of engine/render/vulkan/ cite a replay() that does not exist in that folder - including the two that name where the y flip is decided

**Already resolved; duplicate of candidate 6.** No `replay(` reference remains
in the Vulkan `.h` or `.cpp` files. The recording description names `submit()`;
that method's viewport setup in
[renderer.cpp](../../../engine/render/vulkan/renderer.cpp) directly explains
and implements the y flip using a negative viewport height.

### Candidate 34

Original, copied verbatim:

- **[question]** `engine/render/vulkan/renderer.cpp:1058` — The scissor is clamped only at its near edges though the comment calls it "the intersection with the buffer", so a pane wider than the colour target rasterises outside the render area

**Comment corrected; visible clipping verified for the tested panes.**
The scissor setup in `Renderer::submit` in
[renderer.cpp](../../../engine/render/vulkan/renderer.cpp) clamps negative
near edges while retaining the pane's far extent. Its comment now distinguishes
that scissor from the render area/colour target that bounds visible output.
No pipeline, viewport or scissor arithmetic changed.

[pixel_tests.cpp](../../../tests/render/pixel_tests.cpp) now checks every pixel
for seven oversized/off-buffer placements and four zero/quantized-zero panes
followed by a valid pane. Markers retain local size and position while the
visible output is cropped. All four native rasterizers pass without new
goldens. Vulkan's full suite reports zero validation errors or synchronization
hazards and 769 performance advisories. This establishes the visible contract
on the tested configuration, not out-of-attachment writes or every driver.

### Candidate 35

Original, copied verbatim:

- **[should-fix]** `tests/render/golden_image.h:14` — tests/render/golden_image.h - the file renderer.h names as carrying the whole golden-image argument - still says three rasterising backends, HLSL at two profiles, and three runs

**Already resolved.** [golden_image.h](../../../tests/render/golden_image.h)
now names four rasterizing backends, HLSL at three profiles plus GLSL, and
four runs/four configurations. The old backend/profile/configuration counts
are absent from that argument.

### Candidate 36

Original, copied verbatim:

- **[minor]** `engine/render/renderer.h:285` — renderer.h's new clause "which was every backend there was when the rule was written" is false - four backends existed at d31a804, and the fourth answers this term too

**Already resolved.** That historical clause is absent from the current
[renderer.h](../../../engine/render/renderer.h). The `create_device` contract
states platform/device behavior directly without the quoted historical count.
The old commit chronology was not independently reconstructed here.

### Candidate 37

Original, copied verbatim:

- **[minor]** `engine/render/null/renderer.cpp:33` — The sweep over the four existing backends stopped short: four more cross-backend counts in engine/render/ still say two or three where the fifth backend makes it four or five

**Referenced roster resolved; the unnamed aggregate is not reconstructed.**
The specifically referenced statement in
[null/renderer.cpp](../../../engine/render/null/renderer.cpp) now says
"other four". Other inspected roster statements also account for Vulkan:
`null/texture_factory.cpp` names four device backends/all five implementations,
`gl/renderer.cpp` describes three HLSL consumers, and
`d3d12/render_resources.cpp` says "other four". The original bullet supplies
only one precise locator for its four-site aggregate. This adjudication does
not claim to reconstruct every unnamed site or audit all render comments.

### Candidate 38

Original, copied verbatim:

- **[minor]** `tests/render/null_tests.cpp:439` — null_tests.cpp's backend count was bumped from three to four by this commit, but there are five and the sentence enumerates only three - and it attributes to one backend a fix that two needed

**Already resolved.** The abandoned-frame explanation in
[null_tests.cpp](../../../tests/render/null_tests.cpp) identifies three of five
implementations as GL/null/Vulkan vector recording and distinguishes the two
command-list implementations. This settles the current count and attribution
statement without re-proving the historical sequence of fixes.

### Candidate 39

Original, copied verbatim:

- **[minor]** `engine/render/d3d12/renderer.cpp:266` — The D3D12 backend still claims engine-owned synchronisation is what "the other three" backends do not ask of the engine; the fifth backend asks the same thing, transliterated from that backend line for line

**Already resolved at the cited setup rationale.** The device-dependent setup rationale in
[d3d12/renderer.cpp](../../../engine/render/d3d12/renderer.cpp) explicitly says
Vulkan asks the same thing about a command pool.
[d3d12/backend.h](../../../engine/render/d3d12/backend.h) frames the original
justification historically and names Vulkan as its second answer. Neither
explanation claims D3D12 alone owns synchronization. Candidate 55 separately
covers the other header bullet, which is now qualified too.

### Candidate 40

Original, copied verbatim:

- **[minor]** `engine/render/vulkan/device_resources.h:278` — DeviceResources::recording() is an accessor with no caller anywhere in the tree, and it is the predicate the D3D12 review found to be the wrong question

**Removed with candidate 27; duplicate historical claim.** The unused
`DeviceResources::recording()` accessor is absent from
[device_resources.h](../../../engine/render/vulkan/device_resources.h)
after a current caller check, including the Vulkan auxiliary tests. Unrelated
test harness methods named `recording()` remain. The `frame_begun` rationale
in [backend.h](../../../engine/render/vulkan/backend.h) rejects command-buffer
recording state as the frame-interval question and keeps separate tracking.
Neither that state nor frame reset behavior changed, and no new runtime
lifecycle result is claimed by deleting this accessor.

## Continuation and remaining work

[CANDIDATES-41-65.md](CANDIDATES-41-65.md) adjudicates the remaining 25 originals.
All 65 now have a source disposition; unresolved runtime questions retain their
limits in the individual entries. The count is no longer an unexamined queue
or a count of defects. The [review index](../INDEX.md#vulkan-tracking) owns the
remaining work, including the separate REV#5 platform gate.
