# Collision sample

A controllable rectangle, solid walls, a trigger and a deliberately ignored
rectangle. It exercises the engine's `Scene` collision sweep and the game's
response in `Body::on_contact`, with no game-specific collision code in the
engine.

From the repository root, in a Visual Studio developer shell with `VCPKG_ROOT`
and the Windows SDK shader compiler available:

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug --target CollisionSample CollisionSampleTests
.\out\build\x64-debug\samples\collision\CollisionSample.exe
ctest --preset x64-debug -R '^CollisionSampleTests$'
```

The build reuses the minimal sample's font and LineSweeper's white texel, copying
both beside this executable with its own manifest. It can start from any working
directory. Resize the window and the camera fits the same arena into it.

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
