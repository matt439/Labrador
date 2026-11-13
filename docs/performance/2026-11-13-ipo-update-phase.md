# LineSweeperFrameBench with IPO off and on, 2026-11-13

The measurement step of `REV#36`
([finding](../review/2026-11-10/FINDINGS.md#rev36)): `Vector2F`'s operators
are defined out of line in `engine/math/vector2f.cpp`, and no preset enables
interprocedural optimisation, so the finding asked what the frame bench's
update phase costs with IPO off and on before anyone decides. This record
supplies the number; whether to enable IPO, inline `Vector2F` in its header,
or do neither is the owner's decision and is not made here.

On this desktop, under MSVC, IPO cut the median `update_ns` from 136.6 µs to
25.9 µs per frame, about 5.3 times less, and the median `record_submit_ns`
from 632.8 µs to 378.0 µs. The four repetitions of each build do not overlap.
The executable is 3.0 % smaller. The frame interval does not change, because
presentation paces both builds and both finish well inside a refresh.

## What was run

- Source: `144641914d5a` with no local change. Two build folders configured
  from the `x64-release` preset (D3D11 / XAudio2) with only this difference on
  the command line:

  ```text
  cmake --preset x64-release -B out/build/ipo-off -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF
  cmake --preset x64-release -B out/build/ipo-on  -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
  cmake --build out/build/ipo-off --target LineSweeperFrameBench --parallel 8
  cmake --build out/build/ipo-on  --target LineSweeperFrameBench --parallel 8
  ```

  The ON folder's `build.ninja` carries `/GL` on its compiles and `/LTCG` on
  the static-library and executable links (331 and 28 occurrences); the OFF
  folder's carries neither. The vcpkg dependencies are built by their own
  triplet and are the same in both. MSVC 14.51.36231 (Visual Studio 18).
- Machine: the RTX 5080 desktop (Windows driver version string
  `32.0.16.1692`; 2560x1440, reported 59 Hz; Windows 11 Pro 10.0.26300; Ryzen 9
  9950X3D, 16 cores, 32 logical processors).
- Each run followed [the local procedure](local-frame-bench.md): 1,800 warmup
  frames, 3,600 retained, `--pacing presentation`, the four-core hold
  (`held to affinity 0xFF` read back from every bench process), and the
  executable's SHA-256 as `--release-hash`. Every result is `complete`,
  `hardware_raster` on the RTX 5080, D3D11 with `dxgi_sync_interval` 1. The
  procedure's block measures D3D12 and Vulkan; this comparison used D3D11
  because the question is CPU work in the update phase, which is the same code
  on every backend.
- Interleaved: off, on; on, off; off, on; on, off, so neither build always ran
  first or with the machine in one state.
- Results stay under ignored `out/`:
  `out/performance/local/2026-11-13/local-144641914d5a-d3d11-ipo{off,on}-{1..4}.json`
  in the worktree that ran them, with `machine-144641914d5a.txt` beside them.
  OFF's executable hashed `b3438c7f…d64ef`, ON's `12efb073…c4aed4`.

The update phase is `Scene::update(0.0f)` and `end_tick()` over the frozen
full-well top-out, 9,600 live particles. With a zero step nothing moves or
dies, but `ParticleField::update` still runs its whole loop, which calls
`Vector2F::operator*`, `operator+=` and `operator+` per particle
(`samples/linesweeper/presentation/particles.cpp`, the integration and the
bounds lead). Those calls cross from the bench executable into
`LabradorMath.lib`, which is what IPO can inline.

## Results

Nearest-rank values from each run's `summary`, in microseconds:

| Run | `update_ns` min / p50 / p95 / p99 | `record_submit_ns` min / p50 / p99 | `scheduled_interval_ns` p50 / p99 (ms) |
|---|---|---|---|
| off-1 | 126.7 / 138.2 / 483.3 / 1345.1 | 411.2 / 677.0 / 3145.9 | 16.679 / 19.149 |
| off-2 | 123.9 / 135.6 / 268.6 / 338.1 | 383.1 / 599.6 / 1066.2 | 16.679 / 17.011 |
| off-3 | 125.9 / 137.5 / 401.0 / 903.9 | 402.8 / 665.9 / 2677.2 | 16.681 / 18.553 |
| off-4 | 120.1 / 133.3 / 260.6 / 295.7 | 368.3 / 575.6 / 958.5 | 16.680 / 16.938 |
| on-1 | 18.5 / 26.7 / 71.0 / 362.4 | 228.6 / 381.9 / 1631.2 | 16.680 / 18.559 |
| on-2 | 20.4 / 27.6 / 126.3 / 353.7 | 236.3 / 401.3 / 1571.3 | 16.680 / 18.063 |
| on-3 | 19.4 / 24.7 / 41.0 / 114.4 | 232.1 / 372.0 / 581.8 | 16.681 / 16.901 |
| on-4 | 19.6 / 25.1 / 37.8 / 188.9 | 241.5 / 374.1 / 750.1 | 16.680 / 17.333 |

| Build | Median of the four `update_ns` p50s (range) | Median of the four `record_submit_ns` p50s (range) |
|---|---|---|
| IPO off | 136.6 (133.3 to 138.2) | 632.8 (575.6 to 677.0) |
| IPO on | 25.9 (24.7 to 27.6) | 378.0 (372.0 to 401.3) |

The minima tell the same story as the medians (120 to 127 µs against 18 to
20 µs), so the difference is not a tail effect.

| Cost | IPO off | IPO on |
|---|---|---|
| `LineSweeperFrameBench.exe` | 290,816 bytes | 282,112 bytes (−8,704, −3.0 %) |
| Rebuild of the bench after touching `vector2f.cpp`, one sample each | 2.4 s | 3.2 s |

## What this does and does not establish

- **The delta is IPO's whole effect, not `Vector2F`'s alone.** `/GL` and
  `/LTCG` allow inlining across every translation unit in the link, including
  the scene, the D3D11 renderer and the sample's own draw code; that is why
  `record_submit_ns` also fell. How much of the update-phase saving moving
  `Vector2F`'s operators into its header would recover without IPO was not
  measured; it is the other option the finding names, and a separate build.
- **It is CPU time on this desktop, not a frame-rate result.** Both builds
  hold the 16.68 ms interval; on the RTX 5080 desktop the update phase is under
  1 % of a 60 Hz frame either way. Nothing here was run on
  [PHILOSOPHY's low tier](../design/PHILOSOPHY.md#performance), and the ratio
  is not evidence of its size there.
- **The machine was loaded.** Other agents were building and testing
  throughout; Windows' processor load read 26 % to 99 % around the runs, and
  the four-core hold shares those cores with whatever else Windows placed on
  them. The p95 and p99 columns move from run to run for that reason (off-1's
  `update_ns` p99 is 1.3 ms, off-4's 0.3 ms) and are not compared. The p50
  and minimum separations are large beside that spread and held in both
  orders of the interleave.
- **MSVC only.** The browser build compiles with clang under Emscripten,
  where IPO means `-flto`; it was not built or timed.
- The rebuild timings are one touch each on the same loaded machine; a clean
  build's link-time cost was not measured.
- Enabling IPO for the presets or for `labrador_settings` is a
  `cmake/settings.cmake` or preset change, consumer-visible, with a
  CHANGES.md entry, as the finding says.
