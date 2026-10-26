# D3D12 review status

This is the live ledger for [the D3D12 review](README.md). The review and
[all-findings.md](all-findings.md) remain historical. Their line numbers name
the reviewed revision, not the current tree. [TODO.md](../../../TODO.md) owns
the open-work index; [the review index](../INDEX.md) maps reviews to ledgers.

The confirmed findings were answered by the follow-up series recorded in the
index. This ledger adjudicates the seven remaining questions from the review's
section 6: items 1-6 and 9. Its count-only items 7 and 8 are already resolved.
Source inspection can confirm duplication or a call site without establishing
a performance problem, a driver failure or a need to change the implementation.

## Section 6 adjudication, 2026-10-26

| Item | Disposition | Current evidence and remaining limit |
|---|---|---|
| 1: duplicated view-opening loops | Confirmed maintenance observation; no behavior defect found | `Renderer::Impl::open_frame()` and `Renderer::set_view_count()` in [renderer.cpp](../../../engine/render/d3d12/renderer.cpp) both obtain the render target and viewport and call `View::begin()` for the declared views. `begin()` leaves a recording view alone. [pixel_tests.cpp](../../../tests/render/pixel_tests.cpp) has the mid-frame resize case and the resize-before-first-view-count case, so both restart shapes have behavioral assertions. No refactor follows merely from counting the loops. |
| 2: heap-start queries during flush | Calls confirmed; cost unmeasured | `View::flush()` calls `texture_slot_gpu()` and `sampler()`, each of which asks its descriptor heap for its start handle. `DeviceResources::back_buffer_view()` also asks its heap. These calls remain in [renderer.cpp](../../../engine/render/d3d12/renderer.cpp) and [device_resources.cpp](../../../engine/render/d3d12/device_resources.cpp). Their presence does not measure their cost. A profile of the existing frame workload is needed before calling them a bottleneck. |
| 3: command-list vector in submit | Allocation confirmed; cost unmeasured | The first `Renderer::submit()` in a frame constructs an empty `std::vector<ID3D12CommandList*>` and reserves the configured view capacity before collecting lists. The vector is local, so its capacity is not retained. Later submits in that frame return at `frame_submitted`. No claim is made that this is the only allocation across the backends, or that its cost matters on the target machine. |
| 4: duplicated buffer description | Confirmed maintenance observation; no behavior defect found | The staging-buffer description in [texture_factory.cpp](../../../engine/render/d3d12/texture_factory.cpp) matches `buffer_description()` in renderer.cpp: buffer dimension, supplied byte width, height/depth/mips/sample count of one, unknown format, row-major layout and no flags. Fields omitted from the staging spelling are zero-initialized, matching the helper. The duplication is inside one backend; it is not a seam leak. |
| 5: D3D11 same-size factory refresh | Bypassed branch confirmed; loss of every refresh path refuted | [D3D11 renderer.cpp](../../../engine/render/d3d11/renderer.cpp)'s size guard returns before its sole call to `DeviceResources::WindowSizeChanged()`, so that method's same-size `UpdateColorSpace()` branch is not reached through the engine. [device_resources.cpp](../../../engine/render/d3d11/device_resources.cpp)'s `Present()` still checks `IsCurrent()` and calls `UpdateColorSpace()`, which recreates a stale factory. Creation and actual resizes also call it. The current comment describes these paths; no monitor-change session or driver behavior is claimed. |
| 6: minimum profiles and identical arithmetic | Minimum claim disproved for the tested D3D12 pipeline; arithmetic claim narrowed | The creation-only experiment below accepts full 4_0 and 5_0 as well as the selected 5_1 on the named device. The build still selects 5_1. The shared golden checks compare rendered output with a per-channel allowance of 8 in [golden_image.cpp](../../../tests/render/golden_image.cpp); they do not prove identical compiler arithmetic. Current shader/build comments state the selected targets and verification boundary. |
| 9: complete upload/read-back and multi-mip footprints | Partly covered; lower-level byte correctness remains open | The existing `two_level_texture()` and "CONTRACT: a minified draw samples level zero, not the chain" in pixel_tests.cpp upload two RGBA levels through the real factory, then assert level-zero output under both filters. The factory obtains every placement, row count, row-byte count and `total_bytes` from `GetCopyableFootprints`, and copies source rows into those placements. Renderer read-back also uses a queried footprint, waits, removes row padding and converts BGRA to RGBA; pixel cases observe that result. No source inconsistency was found, but those assertions do not inspect lower mip contents or the tail levels of a BC chain. |

The existing tests named above are source evidence of coverage, not new test
runs performed for this adjudication. Items 1 and 4 require no behavior change
on the evidence here. Items 2 and 3 need a measured cost before optimization.
Item 5's remaining uncertainty is a real display-change session. Item 9 needs
the deeper verification described below; it is not closed by a level-zero draw.

## Shader-profile creation experiment, 2026-10-26

A standalone native probe read the real `engine/render/sprite.hlsl` through
`D3DCompileFromFile`, with flags1 and flags2 both zero, for vertex/pixel target
pairs 4_0, 5_0 and 5_1. It constructed the current D3D12 root signature and
equivalent pipeline descriptors: four vertex root constants at b0, pixel
descriptor tables at t0 and s0, the real `SpriteVertex` input offsets,
premultiplied blending, no culling or depth, and one BGRA8 render target.
It included no backend-private header and changed no engine profile or code.

| Scope | Recorded value |
|---|---|
| Adapter | NVIDIA GeForce RTX 5080; vendor `10DE`, device `2C02`; software flags `0`; no WARP fallback |
| Adapter LUID | `00000000:00016BA0`, an identity for this session |
| Driver | `32.0.16.1692` |
| Requested feature level | `D3D_FEATURE_LEVEL_11_0` |
| Compiler DLL | `C:\Windows\System32\D3DCompiler_47.dll`, FileVersion `10.0.26100.9711` |
| Debug layer | Debug interface acquired and layer enabled; the probe printed no stored debug messages |
| Shader SHA-256 as tested | `203BB27C05A3980AF45946119F5349259A0D19C5272C29596D64997DF3D0AAEE` |
| Probe source SHA-256 | `6B6B00D928B9D528AB8DA626E0E5030E254ABA0936407BBCE79D0BB97A22D2F6` |

| Vertex / pixel profiles | Vertex compile HRESULT | Pixel compile HRESULT | Pipeline creation HRESULT |
|---|---|---|---|
| `vs_4_0` / `ps_4_0` | `S_OK` (`00000000`) | `S_OK` (`00000000`) | `S_OK` (`00000000`) |
| `vs_5_0` / `ps_5_0` | `S_OK` (`00000000`) | `S_OK` (`00000000`) | `S_OK` (`00000000`) |
| `vs_5_1` / `ps_5_1`, positive control | `S_OK` (`00000000`) | `S_OK` (`00000000`) | `S_OK` (`00000000`) |

The process exited zero and its 5_1 positive control passed. Local source and
raw output are under `out/overnight-2026-10-26/shader-profile-check/`; `out/`
is build output, so the table preserves the result independently of those
local receipts. The shader hash identifies the bytes before this change's
comment-only corrections.

Acceptance of full 4_0 disproves a universal 5_1 minimum for this shader and
pipeline. **Full `4_0` is not `4_0_level_9_1`**: the latter was not tested,
and no lowest accepted profile has been established. The engine retains its
5_1 build targets and its D3D12 feature-level floor of 11_0.

The probe creates pipelines only. It submits no draw or present, reads no
pixels and measures no performance. It establishes acceptance on this named
runtime/device/driver combination, not lower-profile raster equivalence,
support across all D3D12 devices, or an engine platform floor. The ordinary
golden suite continues to exercise the profiles the engine actually builds.

## Remaining multi-mip verification

The public seam deliberately samples level zero, so another minified draw
cannot check the bytes uploaded to the other levels. A useful focused test
would upload a patterned RGBA chain and a patterned BC chain, including tail
levels smaller than a block, then copy each GPU subresource to a read-back
buffer and compare its row bytes with the source after removing footprint
padding. Keep the resource access inside the D3D12 backend/test boundary.
Observe the debug layer too, but do not substitute an absence of validation
messages for byte comparisons. No such deeper read-back test was run here.
