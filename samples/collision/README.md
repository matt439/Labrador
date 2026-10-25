# Collision sample

A controllable rectangle, solid walls, a trigger and a deliberately ignored
rectangle. It exercises the engine's `Scene` collision sweep and the game's
response in `Body::on_contact`, with no game-specific collision code in the
engine.

From the repository root, in the x64 Native Tools Command Prompt, which has
`VCPKG_ROOT` and the Windows SDK shader compiler available:

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
into the module. Serve that folder over HTTP and open the page.
`main_web.cpp` is its entry point and `web/shell.html` its page; the state
both entry points run is `collision_state.cpp`. The page has no smoke test or
capture, which need a command line and WIC, and `CollisionSampleTests` runs
under Node there instead.

| Control | Action |
| --- | --- |
| WASD, arrows or player one's left stick | Move |
| R or gamepad Y | Reset position and counters |
| Escape or gamepad B | Quit |

Walk right through the purple rectangle: its zero mask vetoes collision even
though the player's mask accepts its layer. The grey wall stops the player,
turning it gold while a contact is measured. Move diagonally against it to slide
along it. Walk around it and into the green trigger: contacts turn the player
green and count entries without separating it.

`Arena::step` runs update, resolve, reads the contacts, then ends the tick.
Contacts borrow their participants and expire at `end_tick`; the HUD retains
only their count. Trigger entry is game policy, computed from this step's
overlap and the previous step's overlap. It is not an engine event subscription.

The scene owns the bodies. Their `RectangleF` is both the collision shape and
the drawn rectangle, so moving it cannot leave an old collider behind.
`draw()` reads only. The player chooses `separation` and `slide` for walls;
walls themselves ignore callbacks. The engine supplies measurements, not an
automatic rigid-body solver.

The sample uses the application's 60 Hz fixed step and a speed of 240 world
units per second (four units per step), a 32-unit player and walls at least
24 units thick. The tests exercise wall sliding, mask rejection, trigger
entry/exit, the playable route and the outside corners. There is no continuous
collision detection, gravity, moving-body solver or guarantee for arbitrary
speeds and level layouts. Validate movement together with geometry when
adapting the sample.

`CollisionSampleTests` runs the same `Arena` and `Body` without a window,
graphics device or loaded content. It runs on every backend. To exercise the
application, manifest and drawing too, launch `CollisionSample.exe --smoke-test`.
It runs hidden for 315 fixed steps, follows a route into the wall and around it
to the trigger, checks the result and exits with code zero on success. This is
a device test, separate from CTest; use a rasterising preset to inspect pixels.
Add `--capture path.png` after `--smoke-test` to write its final frame through
the existing PNG writer. Capture is available on rasterising presets only.

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
  `main.cpp` keeps everything the capture needs behind `COLLISION_SAMPLE_CAPTURE`.
