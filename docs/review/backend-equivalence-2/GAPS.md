# What this sweep missed, capped off, and could not do

> Read-only sweep by 350 agents, 2026-08-28, against the tree at `ca5b2e3`.
> Part of the [five-backend equivalence sweep](README.md). Not updated as
> findings are fixed.

The 2026-08-19 audit's equivalent document turned out to be the most useful thing
in its folder, because it said where to start. This is the attempt to be as
useful, and it is written before anybody has acted on the findings rather than
after.

---

## What was capped, and by what

**The red team never ran.** The prior audit's second adversarially-framed pass
over its 18 highest-risk axes earned its keep — it is where several of its
sharpest refutations came from. This sweep's equivalent phase was written and
scheduled and did not execute: the run that carried it was interrupted four
times, and each interruption re-keyed its cache and re-ran work rather than
advancing. The axis results here therefore rest on **one hunting pass plus
two-lens verification**, with no agent ever instructed to assume a divergence was
present and go looking for it.

That is the largest hole in this document, it is a hole in the same place the
prior audit put its best work, and it is cheap to close.

**Verification stopped at 47 of the 113 drift findings.** All 39 unchecked ones
are medium, and they concentrate: `gl/` has 10 and the four test files have 10.
[DRIFT.md](DRIFT.md) marks every one. Given 44 of the 47 that *were* checked
survived, the prior on these is the opposite of the axis candidates — most are
probably real.

**The 85 low-severity axis candidates were never verified.** Here the prior runs
the other way: 61 of the 65 higher-ranked candidates died, so most of these are
inert too. They are unexamined, not cleared.

## What the method could not reach

- **Nothing was run.** Same limit the last audit had and the same consequence:
  where this document says five backends agree, that is an argument about source,
  not a measurement of five frame buffers. The difference from 2026-08-19 is that
  the mechanism which *would* measure it now exists — the golden image set — and
  four rasterising backends are already held to it byte for byte. What the
  goldens do not cover is what this sweep could not either.
- **Three findings are settled by one run each, and none was taken.** README §5
  finding 1 is settled by reading one CI job log for `x64-release`. Findings 4
  and the Vulkan half of finding 7 are settled by `RenderPixelTests` under the
  Khronos validation layer with `validate_sync` on — the sweep
  [docs/review/vulkan/](../vulkan/) §2 describes, which is not what the layers
  check by default and which that document says to re-run after any change under
  `engine/render/vulkan/`. This sweep changed nothing, so it did not owe that
  run; the findings above do.
- **The axes were chosen before the reading**, again. Twenty-nine came from the
  prior audit and eleven from the two new backends' own vocabulary, so an
  invariant the seam never names and neither API introduces had no agent assigned
  to it.
- **Decomposition by render axis has a blind spot at the subsystem boundary.**
  The prior audit got defect A's severity wrong by one grade for exactly this
  reason — six agents analysed the GL renderer and none read
  `engine/app/window.cpp` end to end. This sweep instructed its agents to leave
  the folder where an axis had an outside producer, but only the red team was
  going to be *required* to, and the red team did not run.

## What this sweep got wrong

**One correction, found while writing this up.** The README's first draft stated
four of the five backend line counts from memory rather than from `wc -l`, and
three of the four were wrong. They are corrected in place. It is a small thing
and it is exactly the failure mode `dea5fe0`'s commit message named — *"counts
written into prose drift fastest precisely when someone is looking at them"* —
committed in a document whose largest single category of finding is
miscounted backends. The lesson generalises and it is the same one: prefer a
count that is computed to a count that is written, and where a count must be
written, compute it in the same breath.

## Where the next pass should start

Roughly in order of how likely it is to change something.

1. **Read one CI log.** README §5 finding 1 asserts that `x64-release` runs
   `RenderPixelTests` in the configuration where the D3D11 WARP fallback is
   compiled out, on a runner with no GPU, and that four statements in the tree
   about that job cannot all be true. Either the job is passing for a reason
   nobody has written down, or it is not running what everyone thinks. Both
   outcomes are worth an amendment, and neither costs an afternoon.
2. **Run the red team.** Fourteen axes were selected for it and the list is in
   the sweep's own script: `viewport-origin`, `negative-viewport`,
   `owned-back-buffer`, `barriers-and-layouts`, `frame-ordering`, `readback`,
   `resize`, `clear`, `blend-state`, `sampler-state`, `error-parity`,
   `pixel-logic-leak`, `upload-sync`, `frames-in-flight`.
3. **Finish verifying the 39.** They are cheap, they are enumerated, and the
   evidence says most are real.
4. **The two API-behaviour questions the prior audit named are still open, and
   there is now a third specification.** The rasteriser fill rule against
   deliberately fractional edges — `build_scaled_quad` truncates nothing, so
   every glyph quad has them — is specified exactly by D3D, left
   implementation-dependent by GL 3.3, and specified again by Vulkan. The golden
   set makes this quieter than it was but does not close it, because
   `ALLOWED_CHANNEL_DRIFT` is 8 per channel and a tie-break disagreement on one
   edge row can hide under that. The same holds for blend-result clamping, where
   GL still never calls `DescribePixelFormat` to learn what framebuffer format it
   got.
5. **`markers` is the one axis where the sweep produced a recommendation rather
   than a finding**, and it needs a decision rather than more reading. See
   [LEDGER.md](LEDGER.md), items M1 to M8: one backend forwards, four discard,
   all four argue the discard in place, `set_marker` has zero callers tree-wide,
   and no configuration covers any of it. The 2026-08-26 survey listed this as
   open and it is still open.
6. **The low tier now has a referent and still has no number.** The survey called
   it the largest unmade decision in the tree; `c8176ef` named the machine and
   said which half is measured. LEDGER items R1 to R11 record what that left
   behind: the GPU half is named, the CPU half is neither named nor measured, no
   p99 exists anywhere in the tree, and one of the five sites the survey counted
   was never amended and is now in direct tension with PHILOSOPHY. That is a
   measurement, not a sweep, and it is the one item here that needs hardware
   rather than reading.
