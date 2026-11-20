# Explicit Direct3D software tests

Ordinary Direct3D 11 and 12 Release startup refuses every adapter the shared
`dxgi_device_kind` classifier identifies as software: the DXGI flag, the
Microsoft Basic Render Driver's vendor/device IDs, or its known name. An
unflagged known CPU rasteriser is therefore excluded from hardware selection
as well as reported as software. `hardware` still means only that those
signals did not identify software; it does not prove a physical GPU.

The feature floors remain Direct3D 11 feature level 10_0 and Direct3D 12
feature level 11_0. Debug's ordinary path remains hardware-first, then WARP
fallback. Failed shader/content/resource work is not a device-absence result.
The startup-only `RenderDeviceUnavailable` category and its narrower HRESULT
rules are owned by the separate
[device-unavailable contract](review/colourwars-reports/device-unavailable.md).

## Configuration and lifetime

`-DLABRADOR_D3D_SOFTWARE_TEST=ON` forces Microsoft WARP, including on a machine
with a usable hardware adapter. It is an explicit software-test profile,
defaulting OFF, and configure refuses it for non-Windows, browser, GL, Vulkan
or null configurations. It adds no runtime environment switch or public
Renderer option.

The option configures the engine library and applies to every program linked
to that configured library. An installed game retains that selection without
an environment variable or sidecar flag. A CMake cache retains ON until the
owner explicitly changes it; use a separate build directory for software
qualification. Ordinary starter presets and consumer defaults remain OFF.

In software-test builds the renderer reports its actual selected name and
kind through the diagnostic channel. With its ordinary sink, the installed
game's stderr contains a `Direct3D 11 software-test device:` or
`Direct3D 12 software-test device:` line. The name comes from the selected
adapter, and the kind comes from the shared classifier.

## Default refusal and opted-in drawing

The default Release fixtures run the actual backend startup path with scoped,
executable-local DXGI/D3D controls. Only real WARP is enumerated for the
software-only cases; unflagged ID/name controls isolate the known-software
signals. Explicitly synthetic hardware metadata is used only when creation
is forced to fail, so it is not reported as a successful hardware device.
Unavailable, allocation, invalid-argument and recovery cases retain their
different error categories.

The default-policy configuration is built separately from the opted-in pixel
configuration. Its selected fixture prefix must exist and execute:

```text
cmake --preset x64-release -B out/build/d3d11-default -DLABRADOR_D3D_SOFTWARE_TEST=OFF
cmake --build out/build/d3d11-default --target RenderPixelTests --parallel 2
python tools/d3d_software_check.py --mode default-refusal --executable out/build/d3d11-default/tests/render/RenderPixelTests.exe --report out/d3d11-default.json
```

Use `x64-release-d3d12` and another directory for the same Direct3D 12 check.
The wrapper runs `D3D STARTUP:*` with `--no-skip=true`, retains its JUnit and
refuses an empty, failed or misselected report. JUnit's `tests` attribute is
not treated as a case count: the wrapper checks the actual case elements.

For explicit software drawing, configure another directory with ON, build
and run its complete CTest suite, then record the selected rasteriser:

```text
cmake --preset x64-release -B out/build/d3d11-software -DLABRADOR_D3D_SOFTWARE_TEST=ON
cmake --build out/build/d3d11-software --parallel 2
ctest --test-dir out/build/d3d11-software --output-on-failure --no-tests=error
python tools/d3d_software_check.py --mode pixels --executable out/build/d3d11-software/tests/render/RenderPixelTests.exe --report out/d3d11-software.json
```

The full pixel suite still rasterises and compares the existing goldens. No
image allowance or test timeout is changed for this profile. The record step
requires both `Microsoft Basic Render` and kind `software`; successful exit
alone cannot qualify the requested device.

## CI and installed consumers

The existing Direct3D jobs explicitly select software-test ON. The Release
job and the existing Direct3D 12 job first build separate default-OFF Release
startup fixtures; their ordinary opted-in builds retain the complete pixel,
sample and test coverage. Required job names do not change.

The additional weekly Release D3D12 job also forces WARP and checks its
reported name and kind.

The existing native consumer job still builds the ordinary starter, checks its
test registration, and installs a Release folder. Its Release configure now
explicitly selects software-test ON. The installed game is started through
the same evidence checker:

```text
python <engine>/tools/d3d_software_check.py --mode installed --executable <installed>/MyGame.exe --report out/installed-software.json
```

Run that command from an unrelated working directory. It runs the installed
game with `--smoke-test` from that directory and requires a successful exit and
the actual software-test name/kind diagnostic, and identifies the executable
by SHA-256. It does not change the installed folder. Normal release packaging
and clean-machine validation still use the caller's chosen policy; a normal
default-OFF build refuses a software-only machine.

`ci_gap_report.py` now includes the three Direct3D configurations whose CI
jobs run WARP. Its Direct3D hardware configurations explicitly select OFF,
and a real-driver gap cannot pass from missing or software device evidence.
No software result supplies physical-GPU, display-pacing or driver acceptance.

## Local qualification, 2026-11-20

Default Release passed nine D3D11 and ten D3D12 startup cases. Explicit
WARP Release passed 22/22 D3D11 and 23/23 D3D12 CTest entries. Both suites
reported Microsoft Basic Render Driver as software. The assembled starter
passed both tests, installed successfully and ran its installed startup check
from a separate working directory with the same software-device diagnostic.
[The integration record](verification/2026-11-20-todo-integration.md) retains
the source and executable identities.

## Hosted qualification, 2026-11-20

PR #143 tested `79d65774`. The D3D11 and D3D12 jobs passed their separate
default Release refusal fixtures. Their explicit WARP configurations then
passed 22/22 and 23/23 CTest entries and both reported Microsoft Basic Render
Driver as software. The native consumer passed installed startup from an
unrelated directory with the same reported software device and both compile
policy entries verified.
[D3D11 job](https://github.com/matt439/Labrador/actions/runs/44652200557/job/136009629484),
[D3D12 job](https://github.com/matt439/Labrador/actions/runs/44652200557/job/136009629424),
[consumer job](https://github.com/matt439/Labrador/actions/runs/44652200557/job/136009629220).
