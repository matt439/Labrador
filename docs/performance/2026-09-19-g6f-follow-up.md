# EC2 follow-up after the pacing fixes — `g6f-reference-004`

Run 004 completed successfully on 2026-09-19 from clean commit `35d7178`.
All twenty repetitions and 72,000 retained frames passed evidence validation.
The changes improved typical frame-start interval tails and GL's pooled
whole-frame timing, but did not eliminate long renderer waits. D3D11 and Vulkan
repeat the first-repetition symptoms, and additional slow repetitions appear
in D3D11 and D3D12. The results do not support a cause confined to first use of
an API.

This is the same EC2 reference profile as
[run 003](2026-09-19-g6f-reference.md), not a measurement of the consumer
minimum in PHILOSOPHY. Both the pacer and GL swap-interval setting changed
between runs; this comparison does not isolate their individual effects.

## 1. Declaration and verification

| Item | Run 004 |
|---|---|
| Source | `35d71781ca156d89f7aae4529526d03a63da0bc9`, clean |
| Bundle SHA-256 | `6cac2c533e5dea5aea845b6fc3276302b93b9f5c7a9e38501609d0500c82808a` |
| Release manifest SHA-256 | `2042f5c6a338c17312aacdcd75d56d19b065d2f1a9e919d27ef63ad1cf1a9fea` |
| Instance | `i-0abe1eb84fb0489fb`, `g6f.2xlarge`, `ap-southeast-2a` |
| Image | `ami-071dc9daa2faeaea7`; Windows Server 2022 build 20348, NVIDIA 582.53 |
| CPU / GPU | AMD EPYC 7R13, four cores with SMT disabled; NVIDIA L4-6Q, one quarter L4 |
| Display / session | NVIDIA display 2560×1600 at reported 59 Hz; console session 1, `labrador-bench`; High performance power plan |
| Workload | 1280×720, 9,600 particles, 1,800 warm-up then 3,600 retained frames, nominal 60 Hz |
| Matrix | Five independent processes each, D3D11 → D3D12 → GL → Vulkan; no preconditioning and no discarded repetitions |
| Worker window | 06:34:51–07:06:07 UTC; success marker and SSM command both successful |
| Deadline | 07:27:11 UTC; completed before it |

The fresh build passed all five Release builds and all 69 CTest entries; all
25 offline cloud tests passed. The four raster benchmarks were staged directly
after those builds. AWS preflight checked the existing image, bucket, network,
instance offering, CPU topology, quota and quoted price; CloudFormation accepted
the rendered template before launch.

The analyzer verified the complete evidence manifest, declared host and
workload. A separate local comparison script also checked all raw samples,
phase sums and summaries, clean source identity, comparable host settings, and
the new metadata that the legacy-compatible analyzer does not require:

- All twenty captures use `win32_high_resolution_waitable_timer` and
  `absolute_catch_up`, with pacing wait and start lateness on every frame.
- D3D11 and D3D12 report `dxgi_sync_interval`, requested interval one.
- GL reports `wgl_swap_interval`, requested and reported intervals both one.
- Vulkan reports `fifo`; inapplicable interval fields are null.

These fields record API state, not actual scan-out behavior.

## 2. Every repetition

Whole-frame p99 in CPU-observed milliseconds, run 003 → run 004. The whole
frame includes renderer waits and excludes pacing wait. Read this table before
the pooled values: the processes differ substantially.

| Repetition | D3D11 | D3D12 | GL | Vulkan |
|---|---:|---:|---:|---:|
| 001 | 17.116 → **17.115** | 2.489 → 2.357 | 2.541 → 2.510 | 32.853 → **22.880** |
| 002 | 3.046 → **17.104** | 2.427 → 2.482 | 2.517 → 2.269 | 2.475 → 2.372 |
| 003 | 3.199 → 2.964 | 2.482 → 2.188 | 3.148 → 2.461 | 2.538 → 2.176 |
| 004 | 3.110 → 3.095 | 2.525 → 2.225 | 6.139 → 4.111 | 2.453 → 2.156 |
| 005 | 3.175 → 3.058 | 2.417 → **23.500** | 2.491 → 2.485 | 2.455 → 2.423 |

**D3D11's first repetition recurs almost unchanged.** Whole-frame median is
16.983 ms, present p99 is 14.881 ms, and all 3,600 present calls exceed
8.333333 ms. The second repetition now changes behavior within the retained
minute: zero-based sample 1216 has a long present, 1217 is short, and every
sample from 1218 through 3599 has a long present. That is 2,383 long presents
in total. Repetitions 003–005 return to whole-frame p99 2.964–3.095 ms.
Repetition 005 still contains one 18.930 ms frame, dominated by record/submit;
its low p99 does not mean every frame stayed within budget.

**D3D12's fifth repetition is newly slow.** The first four have whole-frame
p99 2.188–2.482 ms. The fifth has 3,494 begin calls above 8.333333 ms,
begin p99 22.061 ms, and whole-frame p99 23.500 ms. Its largest recorded pacing
wait is only 200 ns: the large measured cost is in `begin_frame`, which includes
the allocator's fence wait, rather than in the timer wait. This localizes the
CPU duration; it does not identify what delayed GPU completion.

**Vulkan's first repetition still stalls, with a different onset and size.**
No begin/present call exceeds 8.333333 ms through zero-based sample 1113.
Long begins then occur on exactly every even sample from 1114 through 3598:
1,243 calls and 2,486 long/short transitions. The alternation therefore occupies
the latter part of the retained minute, rather than all of it as in 003.
Begin p99 is 20.996 ms and whole-frame p99 22.880 ms, down from 30.761 and
32.853 ms. Repetitions 002–005 have whole-frame p99 2.156–2.423 ms.
The begin phase contains the frame-slot timeline wait; it is not image
acquisition. The samples still cannot distinguish GPU execution, dependencies,
driver scheduling or presentation backpressure.

**GL improves, while remaining variable.** All five captures confirm interval
one. Whole-frame p99 is 2.269–4.111 ms, with repetition 004 still the slowest
and present p99 2.596 ms. One present exceeds 8.333333 ms; no whole frame exceeds
the 16.666667 ms budget. This is an observation of the combined changes on a
new instance of the same profile, not isolated proof of the swap-interval fix.

## 3. Pacing and pooled values

The sixteen repetitions without sustained long begin/present episodes have
frame-start interval p99 17.018–17.056 ms. Many corresponding captures in 003
were near 18.0 ms. Their new start-lateness p99 is 0.906–1.033 ms. Median
intervals remain near 17 ms; across all twenty repetitions, mean cadence is
59.9904–60.0008 Hz. The old run already averaged approximately 60 Hz, so this
is improved interval tails in typical captures, not recovery of a lost average
frame rate.

In D3D11-001 the median start lateness is 24.035 ms and its p99 is 24.531 ms,
while pacing-wait p99 is only 100 ns. Together with D3D12-005's negligible
pacing wait, the new fields distinguish being late after renderer work from
time spent waiting in the pacer. Run 003 did not retain these fields, so no
old-versus-new lateness delta can be reconstructed.

All 18,000 frames per backend are included below. These pooled numbers must
not hide the process differences in section 2.

| Backend | Whole-frame p99, 003 → 004 (ms) | Interval p99, 003 → 004 (ms) | Frames over whole-frame budget, 003 → 004 |
|---|---:|---:|---:|
| D3D11 | 17.045 → 17.080 | 18.000 → 17.090 | 2,398 → 3,987 |
| D3D12 | 2.482 → 17.252 | 17.998 → 17.259 | 0 → 2,044 |
| GL | 6.117 → 3.143 | 17.028 → 17.027 | 0 → 0 |
| Vulkan | 32.419 → 22.334 | 32.419 → 22.335 | 1,800 → 1,139 |

The first-API-process observation from 003 recurs, but it is not a sufficient
explanation: D3D11-002 and D3D12-005 now also stall, and two episodes begin
partway through retained measurements. Another uninstrumented repeat cannot
by itself assign a cause. A separately declared run with GPU/ETW tracing or a
controlled presentation/pacing change would be a more discriminating next
experiment. No additional run or implementation change was made here.

## 4. Evidence and cleanup

- Raw evidence: `out/cloud/g6f-reference-004/` (67 downloaded objects).
- Validated analysis: `out/cloud/g6f-reference-004-analysis.json`.
- Build logs, declaration, bundle, authorization, preflight and launch records:
  `out/cloud/g6f-reference-004-preparation/`.
- Reproducible local comparison: `compare.py`, `comparison.json` and
  `comparison.md` in that preparation directory. The comparison retains all
  phase percentiles, per-repetition cadence, pacing and long-call diagnostics.
- Remote evidence: bucket `labrador-performance-077207386906-apse2`, prefix
  `labrador-performance/runs/g6f-reference-004/`.
- Exact run stack: `labrador-performance-g6f-reference-004`.

The instance terminated after successful upload. The exact run stack was deleted
after collection, verified at 07:10:09 UTC; `cleanup.json` in the preparation
directory records the final verification. A subsequent AWS check found no active
Labrador instances. The evidence in the existing S3 bucket is retained.
