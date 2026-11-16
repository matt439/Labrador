# Native MSVC Release interprocedural optimization

`LABRADOR_NATIVE_RELEASE_IPO` enables CMake's Release-only interprocedural
optimization (IPO) for the targets created inside Labrador's directory tree.
Fresh standalone native MSVC Release configurations enable it by default,
following [SM-81](../product/BACKLOG.md#sm-81)'s
[native qualification](2026-11-16-native-release-ipo.md). Explicit selection
is also supported:

```text
cmake --preset x64-release -DLABRADOR_NATIVE_RELEASE_IPO=ON
cmake --build --preset x64-release --parallel 2
ctest --preset x64-release
```

The default covers the five single-configuration native Release presets.
Debug, other configurations, browser, other compiler and subdirectory builds
keep their prior defaults. Existing CMake cache selections persist; configure
`ON` explicitly to move an existing cache that already selected `OFF`.

The option requires native Windows and the MSVC compiler. Configure checks
the compiler/linker's IPO support before creating the engine targets and
names the failed probe if it cannot enable it. Release uses `/GL` and `/LTCG`;
Debug retains its ordinary compilation. The precise floating-point options
remain in force. Browser and other compiler defaults are unchanged and are
not qualified by this option.

Use `-DLABRADOR_NATIVE_RELEASE_IPO=OFF` to opt out. This leaves any separately
chosen CMake IPO policy intact; remove a caller-supplied
`CMAKE_INTERPROCEDURAL_OPTIMIZATION` or configuration-specific override too
when comparing ordinary compilation with IPO.

## A game consuming Labrador

Labrador never sets a consumer's parent-directory IPO variable or target
properties. In a game, the option enables the engine libraries, while the
game owns whether its own objects and final executable use IPO. For a native
MSVC Release build, after defining the game's targets:

```cmake
set_property(TARGET MyGame MyGameRules MyGameTests
    PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
```

Configure `-DLABRADOR_NATIVE_RELEASE_IPO=ON` as well, so the engine's probe
qualifies the selected compiler/linker. The game's Debug and browser builds
should leave this selection off. A consumer that already has a global CMake
IPO policy keeps control of that policy; the Labrador option does not clear it.

## Evidence and qualification limits

The [2026-11-13 CPU-phase comparison](2026-11-13-ipo-update-phase.md) measured
one D3D11 frame-benchmark target with IPO off and on. It found lower CPU update
and record/submit work on the named desktop, and a smaller executable, with
a longer one-file rebuild. It did not qualify the complete native Release
configurations, consumer links, clean build costs or browser builds.

The [2026-11-16 qualification](2026-11-16-native-release-ipo.md) records all
five native Release suites, clean and incremental costs, consumer policy
boundaries and a new same-source CPU comparison. IPO increases some executable
sizes and relink costs. The opt-out remains supported.

Those are dated results for the named compiler, source and desktop. Later
source or toolchain changes still need their affected checks. CPU phases
establish neither FPS nor display pacing or performance on the unmeasured
Radeon tier. Browser IPO remains unqualified and its flags are unchanged.
