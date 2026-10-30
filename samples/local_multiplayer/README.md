# Local multiplayer sample

Two players join with their own input sources, choose READY independently,
then explore one shared arena through two following cameras. The first free
player slot is cyan in the top pane; the second is orange in the bottom pane. Both players
appear in both panes whenever the camera can see them. Move toward the purple
cross to meet, or apart to watch each camera follow its own player.

From the repository root, in the x64 Native Tools Command Prompt, with the
prerequisites in the root README. In a starter project the repository root is
`external\labrador`, and
[Run the samples](https://labradorengine.com/docs/guides/samples/) has every
step with its time, output and failures:

```bat
cmake --preset x64-debug
cmake --build --preset x64-debug --target LocalMultiplayerSample LocalMultiplayerTests
ctest --preset x64-debug -R LocalMultiplayerTests --output-on-failure
out\build\x64-debug\samples\local_multiplayer\LocalMultiplayerSample.exe
```

| Source | Join | Move / menu direction | Select | Leave menu |
| --- | --- | --- | --- | --- |
| WASD half | F | WASD / W and S | F | G |
| Arrow half | Enter | Arrows / Up and Down | Enter | Backspace |
| Any of four pad slots | Start | Left stick / stick or d-pad up and down | A | B |

Joining consumes its press; press Select again to choose READY, or move to
LEAVE. Both players must be ready before the arena starts. Each has a separate
FocusGroup slot, cursor marker and DirectionRepeat clock. Leaving or a
disconnect in the menu frees only that player's assignment and readiness.
Reconnect requires a fresh Start press. Simultaneous joins use the order
WASD, arrows, then pad slots 0 through 3, taking the first free player slot.

In the arena, Escape or B on an assigned controller quits. A player reads only
its assigned keyboard half or pad slot; unassigned devices do not steer it.
An absent assigned pad contributes neutral input. The movement magnitude is
capped, so diagonals do not increase speed. Keys refer to
physical positions. Some keyboards cannot report every simultaneous key
combination; a controller provides an alternative in that case.

Bindings remain attached to their observable pad slots during arena play.
A returning or replacement pad in that slot can control the player; the
engine does not establish physical identity between polls. The lobby handles
observed disconnects by removing the assignment. This sample adds no engine
player manager, rebinding layer or persistent controller identity.

`Arena` is the one `GameObject` holding both players. `Scene::update` runs once
per tick; drawing the same arena twice never steps it twice. The state rebuilds
the view list after movement using `ViewportManager`'s current window size.
Each `Camera` puts its player at the centre of its own pane. The camera can see
outside the arena near an edge; player positions remain inside it. Boundary
clamping is sample movement policy, not collision handling.

The scene borrows the shell's thread pool and partitioner to demonstrate
per-view fan-out. `Arena::draw` and the HUD callback read shared state and write
only their own `DrawList`. This tiny scene does not need parallelism for speed;
passing two null pointers to the scene constructor runs the same views serially.

The build copies a manifest and the existing minimal sample's font beside the
executable; the arena is drawn from the engine's own white texture,
`white_texture_name`. There are no downloads or
external game files to find, and startup works from any working directory. The
usual preset uses Direct3D 11; D3D12, GL and Vulkan use their corresponding
presets and prerequisites. The null preset records the draws without displaying
the arena.

## In a browser

The `wasm-debug` and `wasm-release` presets build the same arena from
`main_web.cpp`, with the content embedded in the module and
`web/shell.html` as the page. With Emscripten set up as the root README
describes:

```bat
cmake --preset wasm-release
cmake --build --preset wasm-release --target LocalMultiplayerSample
```

Serve `out\build\wasm-release\samples\local_multiplayer\` over HTTP and open
`LocalMultiplayerSample.html`. A browser will not compile a module read from
`file://`. The keyboard controls are the same. The pads come from the
browser's Gamepad API rather than XInput, and no pad has driven that path
yet. Escape stops the loop and the page says
so, because a page cannot close itself. The browser build has no threads, so
the two views fan out on the serial pool, one slice after the other. There is
no `--smoke-test` in this build. `LocalMultiplayerTests` runs under Node
instead.

## Verification

`LocalMultiplayerTests` covers separate keyboard/stick routing, simultaneous
movement, speed limits, world bounds and camera centring after a resize. Under
a null preset it also draws the actual arena through both serial and parallel
scenes, compares their recordings and checks that each player appears in both
views without changing positions.

The join reducer has 12 device-free cases covering source ownership, competing
joins, per-player choices and disconnects. Three more cases check joined
movement routing. Native null render/audio adds four real-menu cases with a
hidden Application and synthetic keyboard input: independent focus slots,
held-direction gating, resized recordings and the READY-to-arena transition.
These cases poll no physical pad and run no application loop or graphics/audio
API device. The Debug-null test and sample targets build, the test entry passes,
and the existing null smoke run exits 0. These recordings do not establish
appearance: the join screen is looked at on a desktop build and in a browser.
No physical pad has driven the menu.

`--smoke-test` bypasses the join menu, opens a hidden window and renders 120 fixed ticks of independent
scripted movement and checks final positions and the view count before exiting.
Rasterising presets need their selected renderer's device; null records the
draws without pixels. This check does not exercise the menu or physical input.
On rasterising presets it can capture the fifteenth frame for visual review:

```bat
out\build\x64-debug\samples\local_multiplayer\LocalMultiplayerSample.exe --smoke-test
out\build\x64-debug\samples\local_multiplayer\LocalMultiplayerSample.exe --smoke-test --capture out\multiplayer.png
```

The capture uses the existing PNG writer from `tools/linesweeper_capture/` and
reads between `submit` and `end_frame`. Its parent directory must exist.

## What it borrows from outside this folder

Everything else is in this folder or in the engine. A copy that leaves the
repository has these to deal with:

- `../minimal/content/fonts/`, the font atlas: copied beside the executable on
  Windows and embedded at `/fonts` in a browser. A copy puts the
  `.spritefont` in its own `content/fonts/` and points the three lines of
  `CMakeLists.txt` that reach into it there, or brings its own font and
  renames it in `content/manifest.json` and the state that resolves it.
- `../../tools/linesweeper_capture/png_file.h` and `png_file.cpp`, the PNG
  writer behind `--capture`, compiled into the Windows build on every backend
  but `null`; `main.cpp` includes the header from the repository root. A copy
  takes both files with it, or drops the capture by deleting the
  `if(NOT LABRADOR_RENDER_BACKEND STREQUAL "null")` block in `CMakeLists.txt`:
  `main.cpp` keeps everything the capture needs behind `LOCAL_MULTIPLAYER_CAPTURE`.
