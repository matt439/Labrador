# Extended review evidence

These results concern checkout `0cef6540eb1653557cb5b8db78929a613454729c`.
The conclusions are in [SUPPLEMENT.md](SUPPLEMENT.md), and implementation
status is in [STATUS.md](STATUS.md). No finding was fixed by producing these
diagnostics. A zero probe exit code means the diagnostic completed.

## New probes

| Finding/check | Retained source | Output |
|---|---|---|
| R26-16, scene overlay | [overlay_probe.cpp](extension/probes/overlay_probe.cpp) | [overlay](extension/evidence/overlay.txt) |
| R26-17/19, callback lifetime and power messages | [clients_probe.cpp](extension/probes/clients_probe.cpp) | [clients](extension/evidence/clients.txt) |
| R26-18, Unicode content discovery | [content_path_probe.cpp](extension/probes/content_path_probe.cpp) | [ASCII](extension/evidence/content-ascii.txt), [Unicode](extension/evidence/content-unicode.txt), [exit codes](extension/evidence/content-exits.txt) |
| R26-20/23 and nested-init R26-01 extension | [core_extension_probe.cpp](extension/probes/core_extension_probe.cpp) | [core extension](extension/evidence/core-extension.txt) |
| R26-21, ordinary GL refusal | [gl_refusal_probe.cpp](extension/probes/gl_refusal_probe.cpp) | [GL refusal](extension/evidence/gl-refusal.txt) |
| R26-22, synthetic analyzer counterexample | [presentation_lock_probe.py](extension/probes/presentation_lock_probe.py) | [presentation lock](extension/evidence/presentation-lock.txt) |
| Vulkan no-readback lifecycle | [stream_probe.cpp](extension/probes/stream_probe.cpp), [layer settings](extension/probes/vk_layer_settings.txt) | [first run](extension/evidence/vulkan-stream.txt), [second run with loader trace](extension/evidence/vulkan-stream-loader.txt) |

The native sources are byte-for-byte copies of those compiled during this
pass. The retained Python diagnostic changes only root discovery and fixture
placement: it creates temporary evidence rather than writing beside itself.
It was rerun after that adaptation. Its fixture uses existing test helpers;
it is synthetic, entirely offline, and creates no cloud resources.

Logs were decoded from their original UTF-8 or Windows PowerShell UTF-16
encoding and stored as UTF-8. Trailing spaces/tabs were removed from the
tooling test and Vulkan loader logs for the commit whitespace check; the
original captures remain under ignored `out/review-2026-09-26-extension/`.
Their contents were otherwise preserved. The
tooling test log retains PowerShell's `NativeCommandError` wrapper around the
first line written to stderr; the process returned zero and unittest reports
45 tests and `OK`. This wrapper is not a failing test.

## Fresh matrix

Every row was built and then tested sequentially. Build logs remain locally
under ignored `out/review-2026-09-26-extension/build-<preset>.log`.

| Preset | CTest log | Result |
|---|---|---|
| x64-debug | [log](extension/evidence/ctest-x64-debug.txt) | 14/14 |
| x64-release | [log](extension/evidence/ctest-x64-release.txt) | 14/14 |
| x64-debug-d3d12 | [log](extension/evidence/ctest-x64-debug-d3d12.txt) | 14/14 |
| x64-release-d3d12 | [log](extension/evidence/ctest-x64-release-d3d12.txt) | 14/14 |
| x64-debug-gl | [log](extension/evidence/ctest-x64-debug-gl.txt) | 14/14 |
| x64-release-gl | [log](extension/evidence/ctest-x64-release-gl.txt) | 14/14 |
| x64-debug-vulkan | [log](extension/evidence/ctest-x64-debug-vulkan.txt) | 14/14 |
| x64-release-vulkan | [log](extension/evidence/ctest-x64-release-vulkan.txt) | 14/14 |
| x64-debug-null | [log](extension/evidence/ctest-x64-debug-null.txt) | 13/13 |
| x64-release-null | [log](extension/evidence/ctest-x64-release-null.txt) | 13/13 |

Offline tooling: [45 passing tests](extension/evidence/tooling-tests.txt).

## Original findings rechecked

Original sources and recipes remain in [EVIDENCE.md](EVIDENCE.md).

| Findings | Fresh evidence |
|---|---|
| R26-01 | [core](extension/evidence/core-recheck.txt), plus nested-init extension above |
| R26-02/05 | [collision](extension/evidence/collision-recheck.txt) |
| R26-03/06/09 | [render](extension/evidence/render-recheck.txt) |
| R26-04 | [UI](extension/evidence/ui-recheck.txt) |
| R26-07/08/15 | [window/JSON](extension/evidence/app-recheck.txt) |
| R26-12 | [offline console-user declaration](extension/evidence/console-user-recheck.txt) |
| R26-13 | [math](extension/evidence/math-recheck.txt) |
| R26-14 | [particles](extension/evidence/particle-recheck.txt) |

R26-10 and R26-11 were source-rechecked against the original retained
reproductions. Their Vulkan invalid call and incremental-copy fixture were
not rerun. No production source changed between those reproductions and this
checkout.

## Reproduce the new native diagnostics

Use an x64 Visual Studio developer PowerShell at the repository root. Build
the Debug null, GL, and Vulkan presets first, sequentially. The commands below
compile only the diagnostic sources into a fresh ignored output directory.
They require the existing vcpkg Debug DirectXTK library/DLL for the core probe,
which calls `ApplicationOptions::validate()`, and the Vulkan SDK for the stream
probe. Preserve each executable's exit status when capturing output.

```powershell
$reviewSources = 'docs/review/2026-09-26/extension/probes'
$reviewOutput = 'out/review-extension-replay'
if (Test-Path -LiteralPath $reviewOutput) { throw 'Choose a fresh output directory' }
New-Item -ItemType Directory -Path $reviewOutput | Out-Null
$nullLibraries = @(
    'out/build/x64-debug-null/engine/LabradorEngine.lib',
    'out/build/x64-debug-null/engine/math/MattMath.lib',
    'user32.lib'
)
foreach ($probe in @('overlay_probe', 'clients_probe', 'content_path_probe')) {
    & cl /nologo /std:c++20 /EHsc /MDd /W4 /WX /fp:precise /I. `
        "$reviewSources/$probe.cpp" @nullLibraries `
        "/Fo$reviewOutput/" "/Fe$reviewOutput/$probe.exe"
    if ($LASTEXITCODE) { throw "Compile failed: $probe" }
}
& cl /nologo /std:c++20 /EHsc /MDd /W4 /WX /fp:precise /I. `
    "$reviewSources/core_extension_probe.cpp" @nullLibraries ole32.lib `
    out/build/x64-debug-null/vcpkg_installed/x64-windows/debug/lib/DirectXTK.lib `
    "/Fo$reviewOutput/" "/Fe$reviewOutput/core_extension_probe.exe"
if ($LASTEXITCODE) { throw 'Core probe compile failed' }
$previousReviewPath = $env:PATH
try {
    $debugRuntime = (Resolve-Path 'out/build/x64-debug-null/vcpkg_installed/x64-windows/debug/bin').Path
    $env:PATH = "$debugRuntime;$env:PATH"
    & "$reviewOutput/core_extension_probe.exe"
    if ($LASTEXITCODE) { throw 'Core diagnostic failed to run' }
} finally { $env:PATH = $previousReviewPath }
& "$reviewOutput/overlay_probe.exe"
& "$reviewOutput/clients_probe.exe"
```

The core diagnostic deliberately throws after three zero-step callbacks to
avoid hanging. Lifetime diagnostics observe external flags/weak pointers;
they do not dereference destroyed states or captures. The power diagnostic
injects messages only into its own hidden window, not the desktop.

For the Unicode-path diagnostic, run the same bytes from two locations:

```powershell
'{"probe":"ascii-control"}' | Set-Content "$reviewOutput/manifest.json" -Encoding ASCII
& "$reviewOutput/content_path_probe.exe"
$asciiExit = $LASTEXITCODE
$unicodeDirectory = Join-Path $reviewOutput ('unicode-' + [char]0x6F22)
New-Item -ItemType Directory -Path $unicodeDirectory | Out-Null
Copy-Item -LiteralPath "$reviewOutput/content_path_probe.exe", "$reviewOutput/manifest.json" `
    -Destination $unicodeDirectory
& (Join-Path $unicodeDirectory 'content_path_probe.exe')
$unicodeExit = $LASTEXITCODE
"control exit=$asciiExit unicode exit=$unicodeExit"
```

On this ACP-1252 machine the control returns 0 and Unicode returns 1. On a
machine whose active narrow code page represents the selected character, use
the recorded code page to interpret the result; do not claim universal failure.

GL and Vulkan compilation:

```powershell
& cl /nologo /std:c++20 /EHsc /MDd /W4 /WX /fp:precise /I. `
    "$reviewSources/gl_refusal_probe.cpp" `
    out/build/x64-debug-gl/engine/LabradorEngine.lib `
    out/build/x64-debug-gl/engine/math/MattMath.lib user32.lib gdi32.lib opengl32.lib `
    "/Fo$reviewOutput/" "/Fe$reviewOutput/gl_refusal_probe.exe"
if ($LASTEXITCODE) { throw 'GL diagnostic compile failed' }
& "$reviewOutput/gl_refusal_probe.exe"

& cl /nologo /std:c++20 /EHsc /MDd /W4 /WX /fp:precise /I. `
    "$reviewSources/stream_probe.cpp" `
    out/build/x64-debug-vulkan/engine/LabradorEngine.lib `
    out/build/x64-debug-vulkan/engine/math/MattMath.lib `
    "$env:VULKAN_SDK/Lib/vulkan-1.lib" user32.lib gdi32.lib `
    "/Fo$reviewOutput/" "/Fe$reviewOutput/stream_probe.exe"
if ($LASTEXITCODE) { throw 'Vulkan diagnostic compile failed' }
$previousReviewLayers = $env:VK_INSTANCE_LAYERS
$previousReviewSettings = $env:VK_LAYER_SETTINGS_PATH
$previousReviewLoader = $env:VK_LOADER_DEBUG
try {
    $env:VK_INSTANCE_LAYERS = 'VK_LAYER_KHRONOS_validation'
    $env:VK_LAYER_SETTINGS_PATH = (Resolve-Path "$reviewSources/vk_layer_settings.txt").Path
    $env:VK_LOADER_DEBUG = 'all'
    & "$reviewOutput/stream_probe.exe"
} finally {
    $env:VK_INSTANCE_LAYERS = $previousReviewLayers
    $env:VK_LAYER_SETTINGS_PATH = $previousReviewSettings
    $env:VK_LOADER_DEBUG = $previousReviewLoader
}
```

The GL diagnostic deletes the leaked objects after observing them. Vulkan
loader warnings that environment variables added a layer are expected;
validation errors are not. The stored trace shows the validation DLL loaded
and inserted into the instance and device stacks. No device-loss or actual
concurrent-frame claim follows from this bounded run.

## Reproduce the offline diagnostic and tooling checks

```powershell
python docs/review/2026-09-26/extension/probes/presentation_lock_probe.py
python -m docs.review.2026-09-26.probes.console_user_probe
python -m unittest discover -s tools/tests -v
```

The analyzer diagnostic retains no temporary cloud evidence after it exits;
the output states its synthetic work duration and the actual analyzer verdict.
The console-user diagnostic only validates a declaration and renders a template
in memory. Neither command launches or contacts AWS.
