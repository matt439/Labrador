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

The separate [2026-10-28 construction review](../2026-10-28-renderer-construction.md)
selects the root-signature/PSO value owner as **SM-58**, preserving partial
construction and release order. It also confirms **SM-57**: replacement wrapper
allocation can fail after overwriting an existing texture descriptor, retaining
the old registry object with a changed slot. [Code hygiene](../../code-hygiene.md)
owns those implementation stories and their error-path/device qualification.
The source review adds no runtime evidence and changes none of this ledger's
historical section-6 dispositions.

## Section 6 adjudication, 2026-10-26

| Item | Disposition | Current evidence and remaining limit |
|---|---|---|
| 1: duplicated view-opening loops | Confirmed maintenance observation; no behavior defect found | `Renderer::Impl::open_frame()` and `Renderer::set_view_count()` in [renderer.cpp](../../../engine/render/d3d12/renderer.cpp) both obtain the render target and viewport and call `View::begin()` for the declared views. `begin()` leaves a recording view alone. [pixel_tests.cpp](../../../tests/render/pixel_tests.cpp) has the mid-frame resize case and the resize-before-first-view-count case, so both restart shapes have behavioral assertions. No refactor follows merely from counting the loops. |
| 2: heap-start queries during flush | Measured on the named runtime; retain the current implementation | The [descriptor-query record](../../performance/2026-10-26-d3d12-descriptor-queries.md) separates ordinary getter-loop timings from an instrumented census of the real frozen 9,600-particle workload. Every one of 132 frames makes 12 sampler, 12 texture and two back-buffer queries. Median isolated loops cost about 1.4 ns per getter on this RTX 5080 configuration, including loop/checksum work. This does not identify a useful caching change here; no frame-time gain or result on other devices is claimed. |
| 3: command-list vector in submit | Measured allocation removed; no timing gain claimed | The first `Renderer::submit()` formerly constructed and reserved a temporary array. The warmed Debug CRT regression below reproduced its allocations. The backend now reserves scratch capacity during device creation, clears entries before collecting and after the queue call, and reuses capacity. Declared-view ordering and closing all recording views are unchanged. |
| 4: duplicated buffer description | Confirmed maintenance observation; no behavior defect found | The staging-buffer description in [texture_factory.cpp](../../../engine/render/d3d12/texture_factory.cpp) matches `buffer_description()` in renderer.cpp: buffer dimension, supplied byte width, height/depth/mips/sample count of one, unknown format, row-major layout and no flags. Fields omitted from the staging spelling are zero-initialized, matching the helper. The duplication is inside one backend; it is not a seam leak. |
| 5: D3D11 same-size factory refresh | Bypassed branch confirmed; loss of every refresh path refuted | [D3D11 renderer.cpp](../../../engine/render/d3d11/renderer.cpp)'s size guard returns before its sole call to `DeviceResources::WindowSizeChanged()`, so that method's same-size `UpdateColorSpace()` branch is not reached through the engine. [device_resources.cpp](../../../engine/render/d3d11/device_resources.cpp)'s `Present()` still checks `IsCurrent()` and calls `UpdateColorSpace()`, which recreates a stale factory. Creation and actual resizes also call it. The current comment describes these paths; no monitor-change session or driver behavior is claimed. |
| 6: minimum profiles and identical arithmetic | Minimum claim disproved for the tested D3D12 pipeline; arithmetic claim narrowed | The creation-only experiment below accepts full 4_0 and 5_0 as well as the selected 5_1 on the named device. The build still selects 5_1. The shared golden checks compare rendered output with a per-channel allowance of 8 in [golden_image.cpp](../../../tests/render/golden_image.cpp); they do not prove identical compiler arithmetic. Current shader/build comments state the selected targets and verification boundary. |
| 9: complete upload/read-back and multi-mip footprints | Verified for the patterned RGBA/BC chains below | The new backend-specific regression uses the real texture factory, copies every GPU subresource using queried footprints and compares all meaningful row bytes, including lower levels and BC tails. Public sampling still selects level zero; this is upload-byte verification rather than lower-LOD rasterization or a claim about arbitrary formats and dimensions. |

The existing tests named above are source evidence of coverage, not new test
runs performed for this adjudication. Items 1 and 4 require no behavior change
on the evidence here. Item 2's bounded measurements support retaining its
queries; the record states the method and limits. Item 5's remaining uncertainty
is a real display-change session. Items 3 and 9
have the executed, bounded follow-ups below; a level-zero draw alone was not
used to close the mip-upload question.

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

## Submission allocation verification, 2026-10-26

[d3d12/submit_allocation_tests.cpp](../../../tests/render/d3d12/submit_allocation_tests.cpp)
calibrates a scoped MSVC Debug DLL CRT allocation hook, warms four frames,
then measures only `submit()` with declared view counts 4, 3, 1 and 4. The
baseline made two normal-block allocation attempts totaling 48 requested
bytes on each measured call. The retained array makes zero in that scope.
Assertions, read-back and presentation remain outside the measured scope;
the previous hook is restored. This observes that thread's Debug CRT, not
every driver/system heap, a Release allocator or a timing improvement.
Analytic images independently check ascending view order and empty views.

## Multi-mip byte verification, 2026-10-26

[d3d12/texture_upload_tests.cpp](../../../tests/render/d3d12/texture_upload_tests.cpp)
uploads patterned RGBA 17x9 and BC1/2/3 20x12 textures through the actual
factory. Each has five levels through 1x1, including 2x1 BC tails. A narrow
backend-private borrowed-resource accessor permits GPU subresource read-back
without changing the public sampler contract. Queried footprints determine
all placements and row pitches; the test checks every meaningful row byte
against its source while excluding padding, then restores shader-readable
state. It waits before mapping or releasing the read-back resource and checks
available debug-queue warning/error/corruption messages.

Native Debug D3D12 `RenderTests` passes 163 cases; `RenderPixelTests` passes
all 54 cases and 49,749 assertions, with no debug-unavailable warning. The
allocation regression also passes in this run. No new golden image is
generated. These results establish the tested chains' uploaded bytes on the
named desktop configuration, not BC decode accuracy, nonzero sampler LOD,
every format/dimension/device or real device-removal recovery.
