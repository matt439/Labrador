## What and why

<!-- One paragraph per commit is enough; the commit messages carry the detail. -->

## Configurations CI cannot run

<!--
Required for a code change. Paste the output of

    python tools/ci_gap_report.py --execute

run on the PR's head from a shell with vcvars (and emsdk_env for wasm-debug).
It configures, builds and tests x64-debug-gl and x64-debug-vulkan (whose
RenderPixelTests and MinimalSmokeTests CI skips), the four release presets CI
does not build, and wasm-debug. A preset left out with --preset stays in the
table as "not run"; say why. Name the device below; the tool records none.
For a documentation-only change, write "Not applicable: no code changed."
-->

## Manual checks

<!--
Checks no command performs, each with its result or "not done": Vulkan
validate_sync after engine/render/vulkan/ changes, Golden-Reviewed lines for
changed images, regenerated LineSweeper captures, listening, physical pads,
phones or display pacing. AGENTS.md's verification boundaries own them.
-->
