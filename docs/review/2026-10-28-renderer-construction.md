# SM-54 renderer construction review

Source review dated 2026-10-28 at frozen revision
`8048e2448cb813adf1ac2f916ce8cb2ef4058308` on `local-work`.
Scope: pipeline/static-resource construction, its callers, allocation and
destruction dependencies. This is a source-only ownership review; no
implementation, build registration or public declaration changes, and no
runtime qualification, follow from its verdicts.

Sources: [D3D12 renderer](../../engine/render/d3d12/renderer.cpp),
[D3D12 owners](../../engine/render/d3d12/backend.h),
[D3D12 texture publication](../../engine/render/d3d12/texture_factory.cpp),
[Vulkan renderer](../../engine/render/vulkan/renderer.cpp) and
[Vulkan owners](../../engine/render/vulkan/backend.h). Line numbers below name
the reviewed revision; [Code hygiene](../code-hygiene.md) owns current follow-ups.

## Decisions

| Backend | Verdict | Responsibility boundary |
|---|---|---|
| D3D12 | Split one narrow cohort in a separate implementation story | Root-signature and graphics-PSO creation/ownership can form an embedded backend-private `SpritePipeline` value. Keep descriptor heaps, slot allocation, view/frame allocators and command lists, static-index upload and all frame/device-loss coordination with `Renderer::Impl`. |
| Vulkan | Keep current placement | Pipeline, layouts, samplers and static index resource remain a device-lifetime cohort coordinated by `Renderer::Impl` and `DeviceResources`. Moving only construction member bodies would scatter ownership. A separate confirmed temporary-shader cleanup defect needs a small fix, not a pipeline framework. |

The two backends do not need matching files or types. CONVENTIONS, Files,
requires a responsibility boundary rather than a line-count target. T4 favors
a direct owner with explicit inputs; T8 rules out added frame-path dispatch or
allocation. ARCHITECTURE, Modules, and `engine/render/SEAM.md` sections 3-5
keep backend types private and preserve the neutral renderer interface.

## D3D12: isolate root signature and PSO ownership

### Current owners and dependencies

| Owner / source | Responsibility and consumers |
|---|---|
| `d3d12/backend.h:233-234` | `Renderer::Impl` owns `ComPtr<ID3D12RootSignature>` and `ComPtr<ID3D12PipelineState>` next to its other device resources. |
| `d3d12/renderer.cpp:655-824` | Builds the root signature, serializes it with an error-text diagnostic, describes the vertex/shader/blend/raster/depth state, and creates the PSO. Inputs are an `ID3D12Device*`, the selected back-buffer format and compiled shader arrays. This region neither submits a list nor touches a frame allocator. |
| `d3d12/renderer.cpp:302-359` | `View::begin()` resets its list with the shared PSO, sets the shared root signature and descriptor heaps, and binds the shared index buffer. It remains the consumer of borrowed pipeline pointers. |
| `d3d12/renderer.cpp:826-881` | Creates shader-visible texture/sampler heaps and sampler descriptors. Texture-slot state is used by `texture_factory.cpp`, descriptor-address helpers and draw flushing; it is outside the chosen pipeline cut. |
| `d3d12/renderer.cpp:883-1006` | Creates frame/view command objects and uploads the immutable index data through the renderer's frame list, queue signal and `wait_for_gpu()`. This is frame/resource coordination, not pure pipeline description. |
| `d3d12/renderer.cpp:1070-1096`, `:1048-1057` | Initial device creation and `on_device_restored()` call the same construction path. Views themselves retain identity across loss so borrowed DrawLists still name the same view objects. |

The useful cut is smaller than `create_device_dependent_resources()`.
Moving that whole member body to `pipeline.cpp` would carry allocator resets,
view reconstruction and index-upload submissions into a file that claims to
own only a pipeline. Those operations stay together.

### Allocation, destruction and failure constraints

`Renderer::Impl::~Impl()` (`renderer.cpp:481-499`) waits through the existing
non-throwing device operation before member destruction. `device_resources`
is declared before its dependent resources and dies last. The proposed value
occupies the current root-signature/PSO member position, so member destruction
still releases the PSO before its root signature and before DeviceResources.

`on_device_lost()` (`renderer.cpp:1008-1046`) explicitly releases view lists,
allocators and vertex pages, then frame lists/allocators, indices, the PSO,
the root signature and descriptor heaps before notifying the client. A pipeline
owner's reset must preserve the PSO-before-root order and its place in that
coordinator. It does not perform waits or independently decide when destruction
is safe. DeviceResources keeps responsibility for the queue/fence and loss
sequence.

Serialization blobs and successful device objects already use ComPtr. If a
later operation fails, initialized root/PSO members remain owned and are
released by the existing owner/reset/destruction path. The extraction should
keep that partial-construction model and diagnostic text, rather than silently
turning it into a transaction with different replacement/error ordering.
The separate descriptor-publication defect below does not belong to this
construction extraction.

### Bounded implementation story to file

Create `engine/render/d3d12/sprite_pipeline.h/.cpp` with one embedded private
owner of the two ComPtrs, declared root first and PSO second. Its create
operation takes only the borrowed device and target format and assigns directly
to those members using the current ReleaseAndGetAddressOf calls. Do not eagerly
reset both or return a transactional temporary: serialization failure leaves any
preexisting pair as today, while successful root creation followed by failed
PSO creation retains that root until the existing teardown. Its explicit reset
releases pipeline state then root signature.
Keep read-only access to the two objects local to backend callers. Move the
compiled shader-array includes and root/PSO description code with that owner.
Do not add a heap allocation, virtual interface, frame callback, public renderer
type or general pipeline/configuration framework.

`Renderer::Impl::create_device_dependent_resources()` calls this owner at its
current position, then performs its existing descriptor, command-object and
index-upload work. `View::begin()` and initial view-list creation use the
owner's borrowed objects; loss/reset/destructor orchestration stays in
renderer.cpp. No batching, transform, sampler, vertex ABI, format or blend
choice changes. Source paths outside this folder continue to use the same
renderer seam.

Enumerate the new source in the D3D12 CMake branch. Amend ARCHITECTURE's backend
file-shape paragraph and SEAM section 4 in that implementation commit so a
backend-private resource owner is documented, without weakening the include
wall or relocating caller contracts. Mark the source-only organization change
`not consumer-visible` if it preserves the contract; do not mix a behavior fix
into the mechanical extraction.

Acceptance for that story: compare API descriptors, shader inputs, diagnostics,
ownership/release ordering and existing case inventory; run the required native
five-backend checks, D3D12 pixel/resource tests, warmed submission-allocation
regression and applicable resize/device-loss/pending-slot probes. Preserve
golden bytes and test names. The existing GPU startup stop means this source
review does not qualify those runs or authorize retrying them.

## Vulkan: keep construction in its current lifetime coordinator

### Current ownership and call graph

`vulkan/backend.h:174-179` holds the descriptor-set layout, pipeline layout,
pipeline, point/linear samplers and index buffer in `Renderer::Impl`.
`renderer.cpp:261-608` builds them. The pipeline consumes the compatible render
pass from `DeviceResources::render_pass()` (`renderer.cpp:470`); dimensions are
dynamic viewport/scissor state, not another pipeline variant.

The resource dependency graph is:

```text
DeviceResources -> device generation, render pass, upload/frame command state
Renderer::Impl  -> set layout -> pipeline layout -> graphics pipeline
              -> point/linear samplers
              -> immutable index VulkanBuffer
CPU view runs  -> Renderer::submit -> frame descriptors/uniforms/vertices
                                  -> shared pipeline, layouts, samplers, indices
```

The CPU views record sprite runs independently. `submit()` (`renderer.cpp:949`)
collects them, sizes frame-owned buffers, records the pass and binds the shared
pipeline/index buffer (`:1044`), then allocates/writes descriptors against the
shared layout (`:1137`). Descriptor pools, command recording/abandonment, image
layouts and timeline targets therefore remain with the existing frame owner.

Index construction uses `begin_upload()` and `end_upload()` (`:570`, `:599`).
Its staging resource has explicit success/catch cleanup (`:565-607`), and the
upload follows the same DeviceResources queue/timeline path as content upload.
This work must not migrate into a supposedly independent pipeline constructor.

### Lifetime and keep rationale

`Renderer::Impl::~Impl()` (`renderer.cpp:231-251`) waits before
`destroy_device_dependent_resources()` (`:610-650`) destroys indices, pipeline,
pipeline layout, set layout and samplers. DeviceResources is the first member
and is destroyed after its clients. `DeviceResources::handle_device_lost()`
(`device_resources.cpp:1385-1423`) waits, invokes the old-resource callback,
destroys/recreates its device objects, then invokes restoration.
`on_device_lost()`/`on_device_restored()` (`renderer.cpp:713-733`) participate
in that order. Resize (`:791`) rebuilds size-dependent state and reopens
recording without rebuilding this pipeline cohort.

A matching `SpritePipeline` wrapper here would need an explicit raw-handle
device-affinity and partial-failure policy, plus coordinated layout/sampler
release. The current group already has one lifetime coordinator and one draw
consumer. Introducing a second ownership policy merely to mirror D3D12 is not
an established simplification, especially while separately tracked exceptional
cleanup gaps are being addressed. Keep the files and shared handles here.
Revisit a distinct owner only when another real pipeline/lifetime consumer
provides a boundary, rather than starting with an arbitrary method split.

No extraction story is needed for this keep verdict. The existing renderer
seam remains unchanged. No Vulkan API state or ownership cleanup is changed
by accepting the source-organization decision.

## Separate confirmed issue: temporary Vulkan vertex shader cleanup

This finding is independent of both [SM-53 gaps](vulkan/2026-10-28-device-ownership.md)
(texture publication cleanup and read-back cleanup against a replacement
device). Their bounded implementation story is SM-56.

At `vulkan/renderer.cpp:327`, vertex `create_shader()` can succeed and return a
raw local VkShaderModule. Pixel `create_shader()` is called at `:330` before
the destroy calls at `:476-477`. If the second call throws, stack unwinding
cannot release the vertex module: neither an owning local nor an Impl member
holds it, and `destroy_device_dependent_resources()` has no shader-module
field to inspect. `create_shader()` (`:106-129`) can throw from its aligned
word-vector allocation or from a failed API result. The successful vertex
module then misses the engine's explicit cleanup path.

Both initial creation and restoration call this construction method. During
restoration the old cohort has already been released, and the leaked local
module belongs to the new device generation. This is a source-established
exception-path cleanup gap; it is not a reproduced allocation failure, a driver
failure, a measured process leak or a physical-device acceptance claim.

Bounded fix: wrap only the second create_shader call in a catch that destroys
the successful vertex module through its creating VkDevice and rethrows the
original exception. Once the pixel module succeeds, the intervening local
structure setup is nonthrowing; the existing code stores the pipeline API
result, destroys both modules, then checks that result. Preserve that ordering
and all descriptor/frame/upload behavior. Record this finding with the planned
Vulkan exceptional-cleanup story (SM-56); no Vulkan ownership framework or D3D12
extraction belongs in that fix.

That fix needs a deterministic second-module failure regression that observes
the already-created vertex module being destroyed through the same device,
plus the required render matrix and Vulkan shell-visible `validate_sync` run.
If those device checks remain stopped, its implementation and qualification
must stay separately reported. This review executes none of them.

## Separate confirmed issue: D3D12 replacement descriptor publication

At `d3d12/texture_factory.cpp:326-329`, an already loaded name selects its
existing descriptor slot. `CreateShaderResourceView()` overwrites that slot
at `:336-337`; only afterward does `make_unique<D3d12Texture>()` allocate the
replacement wrapper (`:339-341`). If that allocation throws, the call to
`add_texture()` never occurs. `render_resources.cpp:25-29` and
`core/registry.h:74-83` leave the original registry object in place until that
call replaces it, but its slot has already been changed to name the new local
resource. Unwinding releases that new local ComPtr. The registry and descriptor
publication no longer agree after this exception.

This is a source-established wrapper-allocation failure gap on a replacement
name, not an injected failure or observed corrupt frame. It is independent of
the root/PSO extraction and the Vulkan cleanup findings. Its bounded fix is
SM-57, with the D3D12 owner extraction separate as SM-58.

The fix should finish the potentially throwing replacement-wrapper allocation
before overwriting the existing descriptor, retaining the old registry object
and descriptor if allocation fails. Pin that ordering with a deterministic
failure regression and verify the existing same-name reload/descriptor-limit
cases. Do not broaden the claim to every new-name registry insertion or a
general allocator rollback guarantee without separate evidence. The required
render/device qualification remains open under the current stopped gates.

## Evidence limits and next record updates

Reviewed sources: the two renderer.cpp/backend.h pairs, both device-resource
implementations, texture-factory consumers, CMake source/shader registration,
current SM-54 acceptance, renderer seam, architecture and conventions. The
D3D12 current STATUS and existing review index keep their prior runtime claims;
historical reviews and line numbers are not rewritten by this review.

Independent read-only review agreed on the narrow D3D12 cut and Vulkan keep
boundary, and independently confirmed the temporary-shader cleanup gap.
The review creates no claim about performance, allocations during a live frame,
pipeline/profile compatibility on another device, real loss/recovery, queue
synchronization validation, scan-out or supported hardware. No build, test,
browser, GPU startup, generated game JavaScript, PowerShell fixture, network
request, credential read, service activation or repository commit was run.

The current [maintenance stories](../code-hygiene.md) are SM-56 for the three
independently confirmed Vulkan cleanup gaps, SM-57 for replacement-descriptor
publication and SM-58 for the narrow D3D12 owner extraction. The source review
closes with these verdicts; implementation stories retain their own checks
and all unrelated hardware gates remain open.
