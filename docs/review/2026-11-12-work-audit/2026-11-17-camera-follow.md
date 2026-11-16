# Local-multiplayer follow component, 2026-11-17

OCA-103 puts bounded dead-zone following in copyable sample code,
`samples/local_multiplayer/follow_camera.h` and `.cpp`. The engine's Camera
contract and player movement are unchanged. The source is based on
`5cff44b9` with this story's working changes.

## Policy and state

`PlayState` owns one FollowCamera value per player and updates both after
the arena's single movement tick, before copying cameras into the scene.
The first update starts with a centred view, then applies the zone and world
constraints. Later updates pan only by the excess beyond an inclusive zone.
Resizing preserves the previous world-space view centre before applying the
new zone and world bounds. World clamps take priority near an edge; an axis
no larger than the view is centred, including a zero-size world axis.

The helper uses unit scale, finite inputs and checked representability. A
refused update leaves all tracking state unchanged. The camera query returns
a value and writes nothing. Drawing remains const; no follow update is called
from a view worker. The sample chooses a central-half zone with a minimum
16-pixel inset, reserving its HUD outside the world viewport. Short panes keep
a positive world strip rather than painting unfit instructions over it.

## Automated results

| Configuration | Focused result |
| --- | --- |
| x64-debug, D3D11/XAudio2 | LocalMultiplayerTests 1/1; 120-tick hidden capture smoke exits 0 |
| x64-debug-null | LocalMultiplayerTests and LocalMultiplayerSmokeTests 2/2 |
| wasm-debug, WebGL2/Web Audio | LocalMultiplayerTests under Node 1/1 |
| wasm-release override, null render/audio | LocalMultiplayerTests under Node 1/1 |
| Chrome and Firefox sample pages | LocalMultiplayerPageTests and FirefoxLocalMultiplayerPageTests 2/2 |

The component's 14 cases pass 299 assertions: all inclusive zone edges/corners,
minimal one-pixel pans, stopped targets, asymmetric and point zones, each world
edge, offset/small/equal/zero worlds, resizing, independent copies and refusal
of invalid or unrepresentable inputs. The existing native serial/parallel
arena recording comparison now uses this helper. Wasm uses the serial pool;
neither recording establishes rasterization or physical input.

A native-null real-state case moves one player inside and beyond its zone,
checks that the other view does not move, then drives the other player to the
top edge below its HUD. It checks resize/repeated drawing and a short-pane
layout. The smoke route still ends at the same two player positions and two
views. No named ctest entry or inventory changed.

An additional one-off consumer check copied the pair into the existing
v0.3.0 starter used by the independent Random-guide verification, changed the
source's first include to `game/follow_camera.h`, and listed it in a consumer
target against that starter's unchanged engine. Configure/build/run pass;
minimal pan and centred small-world results match. This confirms the helper
uses the existing API rather than requiring an engine follow-camera feature.

## Presentation review

All three captures in the [receipt](2026-11-17-camera-follow.json) were viewed:

- `native.png`, 1280x720: the repeatable fifteenth smoke frame. Both players
  and the shared cross appear in both views, below readable fixed headers.
- `browser-follow.png`, 1280x720: after long independent right/up input. The
  cyan player is at its zone edge with the world panned; the orange player
  reaches the world's top, fully below the second HUD.
- `browser-small-world.png`, 2560x1440: desktop DPR-2 emulation makes the view
  wider than the 1800-unit world. Equal horizontal outside gutters are visible
  in both panes; each header remains fixed and the world is centred.

Chrome 157.0.8103.78 names ANGLE D3D11 on the RTX 5080. Firefox 160.0 passes
its headless page protocol and exposes a sanitized renderer string, which is
not physical device identification. The native capture is the configured
D3D11 Debug path, whose WARP fallback remains possible. DPR emulation does not
qualify a physical high-DPI display, phone, pacing or pad. No audible output,
Safari run or physical controller use was added by this story.

## Evidence and integration

The receipt retains source hashes, command arguments and exits, browser
versions and the reviewed PNG hashes/dimensions. Raw logs, captures and the
one-off browser/copy drivers remain under the isolated worktree's
`out/camera-follow/`. Installed native/wasm vcpkg packages were reused with
manifest installation disabled; this is not a clean dependency-install run.

The guide excerpts quote the maintained component and replace the undecided
proposal. Their version note uses the already selected GuideVersionNote
component from `c625a1c1`; that shared file was supplied temporarily for the
isolated website gate and is not duplicated in this commit. The gate passes
type checking, 200 tests and a 180-page build with valid internal links.
The source-enumeration declarations include both new `.cpp` files.
