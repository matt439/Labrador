# Workload identity and cadence pools, 2026-10-26

This closes PERF#1, PERF#2 and PERF#3: comparability and report contracts
for `LineSweeperFrameBench` and the cloud analyzer. It adds no hardware measurement. The retained samples and tables
for `g6f-reference-003` and `g6f-reference-004` are unchanged; read
[the ratchet record](2026-09-19-g6f-ratchet.md) before those tables.

## 1. The same workload name no longer means the same rendered frame

`bench/linesweeper_frame_bench.cpp` fills all 200 visible cells, adds the
sample's own presentation, sets `topped_out`, and performs a zero-time
update. That materializes 48 particles per cell, exactly 9,600, which remain
frozen for the measured frames. `bench/linesweeper_frame_result.cpp` still
writes `workload.name = "full_well_top_out"`; the analyzer requires that name.

Commit `8c6927203a73bb0acd94f7976a626ad186366aa2`, on 2026-10-13, changed the
presentation the benchmark uses:

- `BoardView::draw_stack()` returns for a topped-out world. The filled cells
  remain in the rules state and generate sparks, but their stack is not drawn.
- `ParticleField` starts top-out sparks at `shatter_heat = 0.55` instead of
  life 1. Its requested quad side is `shatter_size * (0.35 + 0.65 * life)`,
  so the nominal frozen side is about 4.245 pixels instead of 6. Rasterized
  edges are quantized and depend on the particle's position. The tint is
  cooler too.
  Speed and lifetime also changed; the zero-time frozen capture does not
  advance those particles through their lifetime.

The live count, viewport size and run length therefore do not describe all
the work that changed. A later capture must not be compared with 003/004 as
the same rendered workload solely because those fields and the name match.
Use the recorded source commit and immutable release hash to identify which
presentation was built, as well as the declared host and timing policies.
No speedup or regression is inferred here from the source change.

The workload name is retained deliberately here: revising it requires a
coordinated declaration/producer/analyzer migration that still accepts
retained evidence. This records the boundary explicitly rather than silently
renaming the producer or reclassifying historical results. A future workload
revision must name its migration and comparison boundary together.

## 2. Separate cadence populations without dropping samples

The analyzer's existing `presentation_lock` record classifies the tail of
each repetition. Its `cadence_signature` is an observation, not proof of a
presentation wait. The verdict is `not_locked`, `presentation_wait_candidate`,
`work_limited` or `ambiguous_cadence`; the backend's candidate-id list is
`presentation_wait_candidate_repetitions`. The ratchet record's section 5
uses those names and distinguishes the signature from the mechanism
independently investigated in runs 003/004.

Each backend now also reports `cadence_pools`, keyed by the verdicts present
in that backend's repetitions. A pool contains `repetitions`, `sample_count`
and `summary` for each recorded phase, computed by pooling raw values rather
than averaging the repetitions' percentiles. Every repetition belongs to
exactly one pool and contributes every sample, including samples before a
tail's `tail_first_sample`. An absent verdict has no empty pool; unrecorded
legacy phases are not synthesized.

This separates populations with different tail classifications while
preserving the existing all-repetition `summary`, total counts, candidate
list, per-repetition diagnostics and ranges. A pool is not a declaration that
every frame had its tail's state: `not_locked` also admits split cadence,
and a candidate remains an inference about CPU-observed durations. Inspect
the raw samples and onset before attributing a cause or comparing processes.

The owner decision `SM-03`, which p99 column judges the pipeline-depth claim,
remains open. This change selects no column, changes no pacing policy and
replaces no low-tier hardware acceptance.

## 3. Verification

The 21 offline `AnalysisTests` pass. Synthetic evidence covers distinct
cadence pools, raw-sample percentile calculation, conservation of all samples
including pre-tail prefixes, legacy missing phases and preservation of the
existing all-repetition aggregates. This is analyzer verification, not a new
benchmark or a reinterpretation of the retained hardware measurements.
