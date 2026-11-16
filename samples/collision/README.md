# Collision sample

A controllable rectangle, solid walls, a trigger and a deliberately ignored
rectangle. It exercises the engine's `Scene` collision sweep and the game's
response in `Body::on_contact`, with no game-specific collision code in the
engine.

From the repository root, in the x64 Native Tools Command Prompt, which has
`VCPKG_ROOT` and the Windows SDK shader compiler available. In a starter
project the repository root is `external\labrador`, and
[Run the samples](https://labradorengine.com/docs/guides/samples/) has every
step with its time, output and failures:

```bat
cmake --preset x64-debug
cmake --build --preset x64-debug --target CollisionSample CollisionSampleTests
.\out\build\x64-debug\samples\collision\CollisionSample.exe
ctest --preset x64-debug -R "^CollisionSampleTests$"
```

The build copies its manifest and the minimal sample's font beside this
executable; every shape is drawn from the engine's own white texture,
`white_texture_name`, so there is no texture to copy. It can start from any
working directory. Resize the window and the camera fits the same arena into it.

It also runs in a browser. Under the `wasm-debug` or `wasm-release` preset the
same build produces `CollisionSample.html` beside its `.js` and `.wasm` in
`out/build/<preset>/samples/collision/`, with the manifest and font packed
into the module. Serve that folder over HTTP and open the page; from the
engine's folder, in a prompt where `emsdk_env.bat` has run:

```bat
start "Sample server" "%EMSDK_PYTHON%" -m http.server 8000 --bind 127.0.0.1 --directory out\build\wasm-debug\samples\collision
```

then open `http://127.0.0.1:8000/CollisionSample.html`, and reload with
Ctrl+F5 after a rebuild.
[Build for the browser](https://labradorengine.com/docs/guides/browser/)
installs Emscripten and says what each step prints.
`main_web.cpp` is its entry point and `web/shell.html` its page; the state
both entry points run is `collision_state.cpp`. The page has no smoke test or
capture, which need a command line and WIC, and `CollisionSampleTests` runs
under Node there instead.

| Control | Action |
| --- | --- |
| WASD, arrows or any pad's left stick | Move |
| R or gamepad Y | Reset position and counters |
| O | Show or hide shape outlines |
| Escape or gamepad B | Quit |

Walk right through the purple rectangle: its zero mask vetoes collision even
though the player's mask accepts its layer. The grey wall stops the player,
turning it gold while a contact is measured. Move diagonally against it to slide
along it. Walk around it and into the green trigger: contacts turn the player
green and count entries without separating it.
Each entry leaves a small gold marker just above the entry position for half
a second. The marker stays there while the player moves on.
The HUD counts the markers still admitted to the scene.

`Arena::step` runs update, resolve, reads the contacts, then ends the tick.
Contacts borrow their participants and expire at `end_tick`; the HUD retains
only their count. Trigger entry is game policy, computed from this step's
overlap and the previous step's overlap. It is not an engine event subscription.

`Arena::spawn_entry_marker` adds one scene-owned `EntryMarker` after observing
an entry. The marker is pending until that step's `end_tick`, so the first
draw can show it before it receives its first update on the next tick. Its
zero layer/mask adds no contacts. The marker's game-owned timer records a
deletion flag after half a second; `Scene::end_tick` destroys it. Its
`set_for_deletion` also records a bounds-sweep request, although this arena
sets no bounds. Keep no pointer, reference or callback capture after that
retirement. Arena keeps only counts, and reset retires admitted markers before
clearing its counters.

The scene owns the bodies. Their `RectangleF` is both the collision shape and
the drawn rectangle, so moving it cannot leave an old collider behind.
`draw()` reads only. The player chooses `separation` and `slide` for walls;
walls themselves ignore callbacks. The engine supplies measurements, not an
automatic rigid-body solver.

O toggles a game-owned outline overlay, off by default. It walks the scene's
current collision objects, including mask-zero decoration, and hands their
unchanged shapes to the copyable `ShapeOutline`. The sample chooses yellow,
two world units of thickness and 32 circle segments. These are ordinary white
sprite quads; thickness scales with the camera and can quantize on small views.
There is no engine debug service or new renderer primitive. The component's
source and tests are documented in [Components](../components/README.md#drawing-collision-shape-outlines).

The sample uses the application's 60 Hz fixed step and a speed of 240 world
units per second (four units per step), a 32-unit player and walls at least
24 units thick. The tests exercise wall sliding, mask rejection, trigger
entry/exit, the playable route and the outside corners. They also add a round
post, a `Circle` the player treats as a wall. Met head on, the post stops the
player; met off centre, the player slides around it. There is no continuous
collision detection, gravity, moving-body solver or guarantee for arbitrary
speeds and level layouts. Validate movement together with geometry when
adapting the sample.

Every pad slot counts, as in the template: the system need not put the only
pad in slot 0. `pads.h` combines the slots, summing the sticks and taking Y
or B from any of them.

`CollisionSampleTests` runs the same `Arena`, `Body` and `EntryMarker`, and the pad
combination, without a window, graphics device or loaded content. It runs on
every backend. Null render adds a recording check: no marker before admission,
one after admission and none after retirement. Native-null render/audio also
creates a hidden shell, loads sample content and records the O binding and
const overlay. To exercise the
application, manifest and drawing too, launch `CollisionSample.exe --smoke-test`.
It runs hidden for 375 fixed steps: the original 315-step route into the wall
and around it to the trigger, then 60 stationary steps. It checks the route,
one marker spawn and its completed retirement, and exits zero on success.
`CollisionSmokeTests` runs it on every Windows preset, as `MinimalSmokeTests`
runs the template's; like that one it needs the selected renderer's device.
Use a rasterising preset to inspect pixels.
Add `--capture path.png` after `--smoke-test` to write the route's final frame,
before the stationary retirement wait, through the existing PNG writer.
Capture is available on rasterising presets only.
Add `--outlines` immediately after `--smoke-test` to run that same checked
route with outlines enabled, before an optional `--capture path.png`.

## What it borrows from outside this folder

Everything else is in this folder or in the engine. A copy that leaves the
repository has these to deal with:

- `../minimal/content/fonts/`, the font atlas: copied beside the executable on
  Windows and embedded at `/fonts` in a browser. A copy puts the
  `.spritefont` in its own `content/fonts/` and points the three lines of
  `CMakeLists.txt` that reach into it there, or brings its own font and
  renames it in `content/manifest.json` and the state that resolves it.
- `../components/shape_outline.h` and `shape_outline.cpp`, compiled directly
  into the sample's library. A copy takes those files and their tests as the
  components README describes, repoints the includes/source line, or removes
  the overlay and O binding. It does not link `ComponentsExample`.
- `../minimal/dpi_aware.manifest`, the template's per-monitor DPI awareness
  declaration, compiled into the Windows executable by the `elseif(WIN32)`
  `add_executable` in `CMakeLists.txt`. A copy takes the file into its own
  folder and repoints that one path, or removes that path from the
  `add_executable` call and lets Windows stretch the window on a scaled
  display.
- `../../tools/linesweeper_capture/png_file.h` and `png_file.cpp`, the PNG
  writer behind `--capture`, compiled into the Windows build on every backend
  but `null`; `main.cpp` includes the header from the repository root. A copy
  takes both files with it, or drops the capture by deleting the
  `if(NOT LABRADOR_RENDER_BACKEND STREQUAL "null")` block in `CMakeLists.txt`:
  `main.cpp` keeps everything the capture needs behind `COLLISION_SAMPLE_CAPTURE`.
