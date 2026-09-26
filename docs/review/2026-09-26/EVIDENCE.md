# Review evidence and reproduction

All results concern source revision
`03c3b96f58345191a340b7db664401510a09c1d4`. The main report and status ledger
are the only conclusions; probe exit code zero means the diagnostic ran,
not that the engine passed a regression assertion.

## Retained results

| Finding/check | Probe source | Actual output |
|---|---|---|
| State deferral, table exception safety | [core_probe.cpp](probes/core_probe.cpp) | [core](evidence/core-probe.txt) |
| Window input, JSON | [app_probe.cpp](probes/app_probe.cpp) | [app](evidence/app-probe.txt) |
| Visual geometry, DDS overflow | [render_probe.cpp](probes/render_probe.cpp) | [render](evidence/render-probe.txt) |
| Direct collision/NaN queries | [math_probe.cpp](probes/math_probe.cpp) | [math](evidence/math-probe.txt) |
| Contact retirement, full thin-OBB pipeline | [collision_probe.cpp](probes/collision_probe.cpp) | [collision](evidence/collision-probe.txt) |
| Particle extent and Scene cull | [particle_bounds_probe.cpp](probes/particle_bounds_probe.cpp) | [particles](evidence/particle-probe.txt) |
| UI extent and Scene cull | [ui_bounds_probe.cpp](probes/ui_bounds_probe.cpp) | [UI](evidence/ui-probe.txt) |
| Vulkan extent validation | [vulkan_extent_probe.cpp](probes/vulkan_extent_probe.cpp) | [Vulkan](evidence/vulkan-extent.txt) |
| Missing cloud ConsoleUser | [console_user_probe.py](probes/console_user_probe.py) | [cloud declaration](evidence/console-user-probe.txt) |
| Incremental content copy | [fixture](probes/content-fixture/verify.ps1) | [content-copy fixture](evidence/content-fixture.txt) |

Raw CTest output: [null Debug](evidence/ctest-x64-debug-null.txt),
[D3D11 Debug](evidence/ctest-x64-debug.txt),
[D3D12 Debug](evidence/ctest-x64-debug-d3d12.txt),
[OpenGL Debug](evidence/ctest-x64-debug-gl.txt),
[Vulkan Debug](evidence/ctest-x64-debug-vulkan.txt),
[D3D11 Release](evidence/ctest-x64-release.txt).
The [tooling result](evidence/tooling-tests.txt) is explicitly transcribed from
tool output; that run did not retain its full stdout in a file.

The original build logs, diagnostics, and generated malformed assets remain
under ignored `out/review-2026-09-26/`. The retained sources were copied from
that directory. Particle/UI content paths were changed from this checkout's
absolute path to the equivalent repository-relative path, so they can be run
from another checkout root. This is the only source adaptation during copying.

The input probe injects messages into its own hidden window. It does not change
the desktop keyboard layout, send global input, or establish physical hardware
behavior for the modifier sequence. The table probe deliberately supplies a
throwing custom element. Contact/state lifetime probes observe external flags
without dereferencing freed objects.

The content fixture uses text bytes named `white.dds` solely to exercise copying;
it never passes them to a texture decoder. Its CMake target reproduces the
benchmark's POST_BUILD dependency shape, while inspection of the actual
generated Ninja edge establishes that the project has the same omission.

## Reproduce the native probes

Use a Visual Studio x64 developer PowerShell from the repository root, with
the normal vcpkg prerequisites. Build `x64-debug-null` first. Run native builds
and tests sequentially. The following writes only under ignored `out/`:

```powershell
$reviewProbes = 'docs/review/2026-09-26/probes'
$reviewOutput = 'out/review-2026-09-26'
New-Item -ItemType Directory -Force $reviewOutput | Out-Null
$nullLibraries = @(
    'out/build/x64-debug-null/engine/LabradorEngine.lib',
    'out/build/x64-debug-null/engine/math/MattMath.lib',
    'user32.lib'
)
foreach ($probe in @('core_probe', 'app_probe', 'render_probe',
                    'math_probe', 'collision_probe', 'ui_bounds_probe')) {
    & cl /nologo /std:c++20 /EHsc /MDd /W4 /WX /fp:precise /I. `
        "$reviewProbes/$probe.cpp" @nullLibraries `
        "/Fo$reviewOutput/" "/Fe$reviewOutput/$probe.exe"
    if ($LASTEXITCODE -ne 0) { throw "Compile failed: $probe" }
    & "$reviewOutput/$probe.exe"
    if ($LASTEXITCODE -ne 0) { throw "Probe failed: $probe" }
}
& cl /nologo /std:c++20 /EHsc /MDd /W4 /WX /fp:precise /I. `
    "$reviewProbes/particle_bounds_probe.cpp" `
    samples/linesweeper/presentation/particles.cpp `
    out/build/x64-debug-null/samples/linesweeper/LineSweeperRules.lib `
    @nullLibraries "/Fo$reviewOutput/" "/Fe$reviewOutput/particle_bounds_probe.exe"
if ($LASTEXITCODE -ne 0) { throw 'Particle probe compile failed' }
& "$reviewOutput/particle_bounds_probe.exe"
```

The original core probe compiled state.cpp/state_context.cpp directly;
likewise the original input/JSON probe compiled its implementation files
directly. As a delivery check, all seven retained non-Vulkan probe sources were
then compiled and executed with the consolidated library-linking recipe above.
They reproduced the reported results. The delivery log is retained locally at
`out/review-2026-09-26/delivery.log`.

## Vulkan extent probe

Build `x64-debug-vulkan`. The probe creates a hidden native window, asks the
public renderer which adapter it selected, matches that identity in an
independent Vulkan query, and requests exactly `maxExtent.width + 1` by 1
pixels. It refuses a CPU payload exceeding four megabytes.

Compile with the same settings as above, replacing the null libraries with
the Vulkan configuration's LabradorEngine/MattMath, adding
`/external:I"$env:VULKAN_SDK/Include" /external:W0`,
`"$env:VULKAN_SDK/Lib/vulkan-1.lib"`, `user32.lib`, and `gdi32.lib`.
Use [vk_layer_settings.txt](probes/vk_layer_settings.txt) for that child process:

```powershell
$previousLayerSettings = $env:VK_LAYER_SETTINGS_PATH
$previousInstanceLayers = $env:VK_INSTANCE_LAYERS
try {
    $env:VK_LAYER_SETTINGS_PATH = (Resolve-Path "$reviewProbes/vk_layer_settings.txt").Path
    $env:VK_INSTANCE_LAYERS = 'VK_LAYER_KHRONOS_validation'
    & "$reviewOutput/vulkan_extent_probe.exe"
} finally {
    $env:VK_LAYER_SETTINGS_PATH = $previousLayerSettings
    $env:VK_INSTANCE_LAYERS = $previousInstanceLayers
}
```

The installed SDK was 1.4.357.0. Its validation manifest documents
`VK_DBG_LAYER_ACTION_FAIL`: the layer returns validation failure before
dispatching an invalid command to the driver. The settings combine it with
shell-visible error logging. The actual result named `extent-02252` and then
the factory's `VkResult -1000011001` throw. This is an expected diagnostic
from the reproduction, not an unexpected failure in the baseline pixel suite.
It is not a synchronization-validation campaign.

## Offline tooling and content fixture

From the repository root:

```powershell
python -m docs.review.2026-09-26.probes.console_user_probe
python -m unittest discover -s tools/tests -v
```

The cloud probe only validates a synthetic declaration and renders a template
in memory. It makes no cloud API call. Its `allow_launch` field is test data,
not authorization to execute a deployment.

Copy the fixture to a **fresh ignored output directory**, then run its verifier
from the x64 developer environment:

```powershell
$fixtureOutput = 'out/review-2026-09-26/content-fixture-replay'
if (Test-Path -LiteralPath $fixtureOutput) { throw 'Use a fresh fixture output path' }
Copy-Item -LiteralPath "$reviewProbes/content-fixture" -Destination $fixtureOutput -Recurse
& "$fixtureOutput/verify.ps1"
```

The verifier edits/removes only its copied fixture inputs/outputs. It refuses
an existing build directory so original evidence is preserved. Expected
defect evidence is `source_only_edit_refreshed=false` and
`missing_deployed_asset_restored=false`.
