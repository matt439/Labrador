## What and why

<!-- One paragraph per commit is enough; the commit messages carry the detail. -->

## Configurations CI cannot run

<!--
Not required: CI's green is enough to merge, and this is run on a real GPU
before each release. If you ran it - expected when this changes golden
images, welcome when it changes engine/render/ or engine/app/ - paste the
output of

    python tools/ci_gap_report.py --execute

run on the PR's head from a shell with vcvars (and emsdk_env for wasm-debug,
whose inventory list needs a Chromium and a Firefox).
It configures, builds and tests x64-debug-gl and x64-debug-vulkan (whose
RenderPixelTests, MinimalSmokeTests and CollisionSmokeTests CI runs only on
software rasterisers),
the four release presets CI does not build, and wasm-debug. A preset left out
with --preset stays in the
table as "not run"; say why. The tool lists each preset's device; fill in
its "Driver:" line, which no test prints.
Otherwise write "Not run."
-->

## Manual checks

<!--
Checks no command performs, each with its result or "not done": Vulkan
validate_sync after engine/render/vulkan/ changes, Golden-Reviewed lines for
changed images, regenerated LineSweeper captures, listening, physical pads,
phones or display pacing. AGENTS.md's verification boundaries own them.
-->
