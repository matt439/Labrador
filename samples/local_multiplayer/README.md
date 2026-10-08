# Local multiplayer sample

Two players explore one shared arena through two following cameras. Player 1
is cyan in the top pane; player 2 is orange in the bottom pane. Both players
appear in both panes whenever the camera can see them. Move toward the purple
cross to meet, or apart to watch each camera follow its own player.

From a Visual Studio developer shell with the prerequisites in the root
README:

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug --target LocalMultiplayerSample LocalMultiplayerTests
ctest --preset x64-debug -R LocalMultiplayerTests --output-on-failure
out\build\x64-debug\samples\local_multiplayer\LocalMultiplayerSample.exe
```

| Player | Keyboard | Optional controller | View |
| --- | --- | --- | --- |
| 1 | WASD | XInput slot 0, left stick | Top |
| 2 | Arrow keys | XInput slot 1, left stick | Bottom |

Escape or B on either controller quits. Missing controllers contribute neutral
input. Keyboard and stick input add, then the movement magnitude is capped, so
diagonals and using two devices together do not increase speed. Keys refer to
physical positions. Some keyboards cannot report every simultaneous key
combination; a controller provides an alternative in that case. Slots are
fixed for this demonstration; there is no joining or controller reassignment.

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

The build copies a manifest, the existing minimal sample's font, and
LineSweeper's white texture beside the executable. There are no downloads or
external game files to find, and startup works from any working directory. The
usual preset uses Direct3D 11; D3D12, GL and Vulkan use their corresponding
presets and prerequisites. The null preset records the draws without displaying
the arena.

## Verification

`LocalMultiplayerTests` covers separate keyboard/stick routing, simultaneous
movement, speed limits, world bounds and camera centring after a resize. Under
a null preset it also draws the actual arena through both serial and parallel
scenes, compares their recordings and checks that each player appears in both
views without changing positions.

`--smoke-test` opens a hidden window, renders 120 fixed ticks of independent
scripted movement and checks final positions and the view count before exiting.
It needs the selected renderer's device. It does not exercise physical input.
On rasterising presets it can capture the fifteenth frame for visual review:

```powershell
out\build\x64-debug\samples\local_multiplayer\LocalMultiplayerSample.exe --smoke-test
out\build\x64-debug\samples\local_multiplayer\LocalMultiplayerSample.exe --smoke-test --capture out\multiplayer.png
```

The capture uses the existing PNG writer from `tools/linesweeper_capture/` and
reads between `submit` and `end_frame`. Its parent directory must exist.
