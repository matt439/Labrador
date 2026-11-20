# Native MSVC Release IPO qualification, 2026-11-16

[SM-81](../product/BACKLOG.md#sm-81) qualifies the five native Release presets
before enabling their standalone IPO default. This records local build,
runtime and CPU-phase observations; it is not browser IPO qualification or
low-tier, audible, display-pacing or real-pad acceptance.

## Source and environment

The controlled D3D11 comparison uses clean source
`52a84842f75f53861a6eb3f05948dfede474e463`: the opt-in IPO implementation
after the approved-decision baseline, with the deliberate-zero test fixture
repair described below. Both folders use the same source and installed
dependencies, differing only in `LABRADOR_NATIVE_RELEASE_IPO=OFF` or `ON`.
This is dated qualification of those inputs, not evidence for later engine
changes. The D3D12 configure started at `2431a746`; the Colour fixture repair
was applied before its test compiled and was then committed as `52a84842`.
GL, Vulkan and null configure at `52a84842`. `release-matrix.json` records the
driver's initial `2431a746` identity, not a per-phase source label. No D3D12
clean-build cost is used in the controlled comparison below.

- MSVC compiler identification: `19.51.36256.0`, from Visual Studio 18's
  `14.51.36231` tools folder. CMake's actual IPO compiler/linker probe passes.
- Windows 11 Pro `10.0.26300`; Ryzen 9 9950X3D, 16 cores / 32 logical
  processors; NVIDIA RTX 5080, driver `32.0.16.1692`.
- Display metadata: 2560x1440, nominal 59 Hz. That integer is not a measured
  refresh rate. The measured device reports hardware Direct3D 11.
- Existing vcpkg packages from the main checkout's
  `out/build/x64-debug/vcpkg_installed` are used read-only with
  `-DVCPKG_MANIFEST_INSTALL=OFF`; dependency rebuild time is excluded.
  Vulkan uses SDK `1.4.357.0`; GL material fixtures use that SDK's dxc and
  spirv-cross host tools explicitly.
- Other agents build and test during qualification. Build and CPU figures
  are observations on that loaded desktop, without a timing threshold.

The first complete ON build exposed MSVC warning C4723, promoted by `/WX`
to an LTCG failure, in Colour's tests of literal zero divisors. The public
contract intentionally clamps the resulting NaN/infinity. The fixture now
supplies the same exact zero through a volatile runtime load, retaining every
assertion and the product implementation. No warning is suppressed, test
excluded or arithmetic policy changed. The controlled clean comparison was
restarted after this repair.

## Build and runtime checks

Each native configuration runs configure, complete build with `--parallel 2`
and full CTest with `--parallel 1 --output-on-failure --no-tests=error`:

| Release selection | IPO | CTest result |
|---|---|---|
| D3D11 baseline | OFF | 22/22 |
| D3D11 | ON | 22/22 |
| D3D12 | ON | 23/23 |
| desktop GL | ON | 22/22 |
| Vulkan | ON | 22/22 |
| null render/audio | ON | 23/23 |

The rasterizing suites include pixel and through-post checks and the native
sample smoke tests. D3D12 includes its synchronization suite. No golden image
changes. The D3D11 generated build has `/GL` on its 306 C++ compile rules
under ON and none under OFF; all 306 retain `/fp:precise`. ON executable links
carry `/LTCG`. The capability probe is a prerequisite, not a substitute for
these builds and tests.

The D3D11 commands use the `x64-release` preset with separate
`out/build/qualification-{off,on}-verified` folders and explicit OFF/ON.
The other presets use `out/build/qualification-{d3d12,gl,vulkan,null}` with ON.
The common vcpkg arguments above are added to each configure. Full logs and
argv/exit/duration records are in the local receipt directory
`out/worktree-cleanup-2026-11-20/Labrador-agent-ipo/out/ipo/`, including
`qualification-verified.json`, `release-matrix.json` and their phase logs.

| D3D11 cost, seconds | OFF | ON |
|---|---:|---:|
| Clean complete build | 123.6 | 142.7 |
| No-op complete build | 2.0 | 1.8 |
| Complete rebuild after touching only `vector2f.cpp` | 8.1 | 38.5 |
| Frame-benchmark link/post-link step in the clean Ninja log | 0.442 | 1.532 |

These are one sample each. Clean timings include developer-environment setup;
the subsequent no-op/rebuild timings start inside that environment. No-op
still executes the normal check/content targets. The touch changes no source
bytes and its original timestamp is restored. The incremental build must
relink all dependent applications/tests, not only the frame benchmark.
`incremental.json` and the clean `build-audit.json` retain the detail.

| D3D11 executable, bytes | OFF | ON |
|---|---:|---:|
| `LineSweeperFrameBench.exe` | 308,224 | 299,520 |
| `MinimalSample.exe` | 367,616 | 370,176 |
| `LineSweeperSample.exe` | 413,696 | 414,720 |
| `RenderTests.exe` | 1,136,128 | 1,486,336 |

IPO does not uniformly reduce executable size. In particular, the test
executable is substantially larger; the benchmark's smaller size is not a
general binary-size promise. The supported opt-out retains ordinary native
Release compilation.

## Consumer boundary

An assembled starter from `2431a7465ec0158f44762ad238480b6a3cc4319a` is built
at `C:/dev/ipo81`. Its parent targets before and after
`add_subdirectory(external/labrador)` record their actual Release IPO
properties.

The later `52a84842` changes only a standalone Colour test; the compiled
consumer's engine/game sources are identical between those two commits.

All three modes build and pass exactly the game's two CTest entries:

| Consumer mode | Engine/math IPO | Parent before/after and game IPO |
|---|---|---|
| Ordinary default | unset | unset |
| Explicit engine option ON | ON | unset |
| Caller also selects `CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE=ON` | ON | ON |

The mixed case proves that an ordinary game links and runs against IPO-built
engine libraries without inheriting the engine's directory initializer. The
full case proves that an explicitly chosen parent policy is preserved. Its
installed Release folder passes `check_release.py` and the installed
`MyGame.exe --smoke-test`. `consumer.json` retains commands, property dumps
and results. These fixtures add no tests to the distributed starter.

After selecting the standalone default, fresh actual Release configuration
selects ON with `/GL` and `/LTCG`; Debug and wasm configurations select OFF
with none of `/GL`, `/LTCG` or `-flto`. A fresh consumer configuration with
the updated module still leaves every recorded IPO property unset.
`default-audit.json` retains those configuration checks. The compiler-free
fixtures additionally put `/fsanitize=address /Zi` in a Debug configuration
whose stand-in IPO probe would fail, proving that the default does not invoke
that probe. This is not sanitizer runtime or browser execution evidence.

## Same-source CPU-phase comparison

Four bounded runs use [the local procedure](local-frame-bench.md): 1,800
warmup frames, 3,600 retained frames, presentation pacing and the four-core
hold. The process affinity is read back as `0xFF` every time. Order is OFF-1,
ON-1, ON-2, OFF-2; all results are `complete`, `hardware_raster`, RTX 5080,
`dxgi_sync_interval` 1. This is the frozen 9,600-particle, zero-step top-out
fixture and the whole IPO effect, not Vector2F alone.

Nearest-rank observations in microseconds:

| Run | Update min / p50 / p99 | Record+submit min / p50 / p99 |
|---|---|---|
| OFF-1 | 119.5 / 240.1 / 255.8 | 360.2 / 535.1 / 907.8 |
| ON-1 | 17.7 / 36.7 / 44.6 | 216.5 / 394.2 / 567.2 |
| ON-2 | 17.8 / 36.8 / 43.9 | 214.4 / 385.7 / 608.8 |
| OFF-2 | 119.5 / 126.9 / 257.6 | 359.5 / 447.6 / 898.6 |

The OFF medians differ substantially on the loaded host. Both ON repetitions
show lower CPU work than both OFF repetitions, but these four runs do not
justify a stable speed-up factor. Frame-start p50s remain around 16.666 ms
under presentation pacing; CPU phases establish neither physical FPS nor
scan-out latency or the Radeon low tier.

Raw JSON and machine evidence are in the receipt directory's `measurements/`:
`local-52a84842-d3d11-ipo-{off,on}-{1,2}.json` and
`machine-52a84842.txt`. The exact measured benchmark executables are retained
in its `artifacts/` subdirectory, before incremental relinking, with SHA-256:

- OFF: `5224e9cabeeecd9bd32c475233cd47b9f01fb044bec02a4ca6e79e02f752a38e`
- ON: `a99d501d7c7419550a6373e0cc672c5c52d8c9f9d8ff90f847d4acf2b99e22fe`

The [policy](native-release-ipo.md) enables only the qualified native MSVC
standalone Release defaults. Browser/clang IPO, a consumer's global policy
and the existing device/owner-attended acceptance gates remain separate.
