# Investigation of run 004's renderer waits

This investigates [the follow-up](2026-09-19-g6f-follow-up.md), using its raw
evidence and the renderer source. **The long EC2 waits remain unresolved.**
The changes here fix evidence validation and reporting, and supply a controlled
diagnostic capture. They do not establish a GPU performance improvement.

**Resolved, 2026-09-19.** The waits are resolved in
[the ratchet document](2026-09-19-g6f-ratchet.md) without the trace this
document asks for: the benchmark's software pacer and the synchronised present
are two clocks at one rate on that host, and a stall moves the wait from the
first to the second for the rest of the process. The validation, reporting
and diagnostic-mode changes recorded below stand and were used there. The
remaining experiment below is superseded for that question; §5 of the ratchet
document says what to change instead.

## What the evidence establishes

The run's samples locate the sustained stalls in D3D11 `present` and
D3D12/Vulkan `begin`. Pacing waits are negligible during these episodes. Across
every adjacent retained sample in all twenty captures,
`interval - change_in_start_lateness` is exactly 16,666,666 ns: the software
deadline arithmetic is consistent. This does not establish that combining
software deadlines with synchronized presentation is harmless.

The renderer source does not support removing the waits:

- D3D12 records a fence after executing command lists, before `Present`, and
  waits for the actual reused back-buffer slot before resetting its allocator.
- Vulkan waits for its reused command slot. Its submission waits on the image
  acquisition semaphore at `TRANSFER`, then signals the slot timeline after
  completion. Image acquisition can return before that semaphore signals.
  Presentation delays can therefore surface later in `begin_frame`.
- Vulkan's acquire wait also gates the submission's early offscreen transfer
  clear, which can limit overlap. Splitting submissions is a possible
  optimization, but its effect on these stalls has not been measured.

These interpretations follow the [DXGI Present contract](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-present),
[D3D12 presentation behavior](https://learn.microsoft.com/en-us/windows/win32/direct3d12/swap-chains)
and [Vulkan WSI synchronization](https://docs.vulkan.org/spec/latest/chapters/VK_KHR_surface/wsi.html).
They explain possible dependencies, not which dependency delayed this EC2 GPU.
The D3D11 comment claiming every synchronized present sleeps until VSync has
been corrected to describe possible queue backpressure.

## Implemented changes

The cloud analyzer validates complete presentation metadata when present and
checks consistency across repetitions. Old evidence without those fields is
explicitly identified as legacy; partial metadata and mixed instrumentation
are rejected. Pacing measurements are retained in the pooled summaries and
repetition ranges. Per-repetition diagnostics expose long calls, their onset,
consecutive runs and alternating sequences, so a pooled p99 cannot silently
hide the process differences documented in run 004.

`LineSweeperFrameBench --pacing presentation` is an opt-in diagnostic that
removes only software deadline waits. The default remains `--pacing software`.
Rendering work per frame and renderer synchronization are unchanged. The
diagnostic labels its timing contract explicitly and omits software wait and
deadline-lateness fields rather than reporting invented zero measurements.
The null backend rejects this mode because it has no presentation mechanism.
In presentation mode `--refresh` sets the comparison budget only; the renderer
and host determine cadence, and equal frame counts need not take equal time.
Neither mode measures display scan-out.

The existing cloud reference declaration accepts software pacing only, and its
analyzer rejects presentation-driven diagnostic captures. A diagnostic run
must not be pooled with runs 003/004 or relabeled as an equivalent reference.

The local [trace wrapper](../../tools/cloud_performance/trace_benchmark.ps1)
captures one bounded benchmark process with Windows Performance Recorder's GPU
profile and retains the ETL, result, process identity and logs. The
[runner README](../../tools/cloud_performance/README.md) gives the invocation
and recording requirements.

## Remaining experiment

**Superseded for the renderer waits** — see the note at the head of this
document. What follows is as written.

Capture the affected EC2 profile with GPU/ETW tracing, retaining every
repetition. Compare separately declared software and presentation-driven
captures with the same renderer, host, workload, and warm-up; alternate order
to avoid confounding first-use effects. Correlate long begin/present calls
with queue execution, presentation and scheduling before changing frame-slot
counts, Vulkan submission structure or presentation defaults. Tracing itself
can perturb timing, so retain an untraced control as well.

No additional EC2 instance was launched during this investigation. A trace on
the affected host, followed by a targeted change and rerun, is still needed to
close the performance findings.

## Verification

All five Release builds and all 69 CTest entries passed. After the diagnostic
metadata was finalized, the benchmark and its result tests were rebuilt and
the result tests rerun on all five configurations. Both pacing modes completed
120-sample smoke captures on D3D11, D3D12, GL and Vulkan using the local RTX
5080; software pacing also completed on null, and presentation pacing on null
was correctly refused. These short local captures test the feature and are
not a reproduction of the EC2 performance experiment.

All 41 Python tests passed: 32 cloud analyzer/runner tests and nine trace-wrapper
process integration tests. The updated analyzer independently
accepted the complete original evidence for runs 003 and 004, retaining their
distinct legacy/current instrumentation labels. The deadline identity above
was checked on all 71,980 adjacent retained-sample pairs from run 004.

Build/test logs, smoke results and reanalysis are under
`out/validation/g6f-follow-up-fixes/`. Original cloud evidence was not modified.
The trace wrapper's tests use real PowerShell and harmless native subprocess
fixtures, including failure to write process evidence after starting a child;
cleanup still kills that child and attempts only the owned trace save. An actual
GPU ETW recording remains unverified in this non-elevated session.
