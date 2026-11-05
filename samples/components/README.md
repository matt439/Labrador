# Components

Small pieces a game copies into its own tree, rather than links. Each is a
header and a source file, small enough to read in one sitting, with its tests
beside it. The gallery shows menu rows and two animation previews on one screen.

| Component | Files | What it is |
| --- | --- | --- |
| Option row | `option_row.h`, `option_row.cpp` | A label and a value picked from a fixed list with left and right: a difficulty, a window mode. Wraps or stops at the ends. |
| Slider | `slider.h`, `slider.cpp` | A label, a bar and a number for an integer in a range: a volume, a sensitivity. Clamps, never wraps. A pointer's x maps to a value for dragging. |
| Pointer capture | `pointer_capture.h`, `pointer_capture.cpp` | One contact at a time, from the mouse or one finger, followed from press to release or cancellation, and a hit test over a page's targets. |
| Nine-slice | `nine_slice.h`, `nine_slice.cpp` | One small image drawn at any size with its corners kept: a panel, a button background, a speech bubble. Nine `draw_sprite` calls and no engine change. |
| Animation preview | `animation_preview.h`, `animation_preview.cpp` | Game-owned elapsed time, looping, one-shot completion and restart over a named animation strip. Drawing reads the current frame without advancing it. |

## Taking one

1. Copy the component's `.h` and `.cpp` into your game.
2. Change the namespace and the include paths to your game's, and list the
   `.cpp` in your `CMakeLists.txt`.
3. Change anything you like. The copy is yours: nothing here promises to stay
   compatible with it, and nothing in the engine knows it exists.

The tests (`*_tests.cpp`, with their in-memory content helpers) come too if you
want them. Widget and timing cases need no window, device or files. The
animation reader case reads the delivered DDS/JSON pair; the build stages it
beside the native test executable or embeds it in the Node module. Native-null
cases also record what the nine-slice, the bar, the slider and the previews
draw, and load the actual manifest's sprite-sheet entry, without creating a
window or graphics/audio API device.
When both native render and audio backends are null, two gallery integration
cases use a hidden window and synthetic input: one checks the real overlay
and R restart binding, the other the pointer rules. They run no Application
loop or API device.

## Why these are copied and not linked

PHILOSOPHY's UI section has the rule. A compound widget that every game uses
unchanged is mechanism and belongs in `engine/ui/`; one that every game edits
is policy and belongs to the game (T1). Nobody knows which a new compound is
until a few games have used it. So it starts here, where taking it costs a
copy and changing it costs nothing, and moves into the engine only once every
client is shown to have copied it unchanged.

The option row is the first evidence of that kind. ColourWars wrote its own,
for its settings and debug menus, and this one is the same shape with the
game's styling taken out.

## How a page uses them

A component does not read input. Which keys mean left is a binding, and
bindings belong to the page. For menu rows, a page:

- adds each row to a `FocusGroup`, which treats it as one target because an
  option row or slider is a `UiContainer`;
- moves the cursor with up and down through `FocusGroup::move`;
- sends left and right to whichever component is focused, through its
  `adjust()`. The gallery finds that component by comparing pointers
  (`GalleryState::adjust_focused`), because the page built every row and
  already holds each one;
- reacts to a change in the callback it handed the constructor.

For a mouse or a touchscreen, the gallery's `update_pointer` adds the rest,
and all of it is page policy rather than a component's:

- `PointerCapture` follows one contact: the first finger down, or the mouse's
  left button when no finger is near. A second finger is ignored, a mouse
  press in the tick a finger lifts or the tick after is ignored, and a
  contact that goes away without being let go - the window lost focus, the
  platform cancelled it - ends cancelled, so nothing completes.
- Each row is a band as tall as the row spacing, from just left of the label
  to past the slider's number, and `hit_target` finds the first enabled one
  under the point, through the camera the page draws with
  (`Camera::world_position`). A disabled band is passed over.
- A press focuses the row under it, as LineSweeper's pause menu does. On a
  slider's track, or within 20 units of either end, it also starts a drag:
  the slider takes `value_at(x)` through `drag_to`, follows the contact past
  either end of the track and clamps there, and keeps the value it reached
  when the contact ends, however it ends.
- A release completes a click or a tap only over the row that was pressed.
  On an option row's value, the left half steps left and the right half
  right; on its label it only focused. Quit runs its action.
- A tick in which the keys or the pad moved the cursor, changed a row or
  chose one cancels the contact, so the two never both act in one tick.

`adjust()` returns whether the value moved, which is the signal a cursor sound
hangs on. Focus colours the label, and the slider's fill as well. The value
keeps a colour of its own, so the player can tell what is selected from what
the cursor is on.

The nine-slice is not a row: it has no value and takes no focus, and a page
adds it to a scene before whatever it frames, because a scene draws in the
order things were added.

The animation preview is a value rather than a widget. The gallery owns two
values, steps them once in `update()` and draws them through the scene's const
view overlay, which already has the fitted camera and viewport. They take no
focus and do not change the settings row order.

## Playing an animation strip

Resolve a paired sheet/strip handle with
`RenderResources::resolve_sheet_strip("animation_preview", "pulse_loop")`, then
construct an `AnimationPreview` with that pair and the resources it belongs to.
The resource table must outlive the preview, and the sheet/strip definition
must stay unchanged. Texture reloads preserve the pair; the preview resolves
the sheet again when it draws, without holding a device texture pointer.

`pulse_loop` and `pulse_once` use the same four 16x16 frames at 0.25 seconds per
frame. The loop wraps every second; the one-shot finishes after the last
frame's full duration and holds it. `restart()` returns either to frame zero.
Negative or nonfinite time steps throw before changing the state, even after
completion; zero changes nothing. A step crossing many frames takes bounded
work. Repeated draws, including draws into two views, read the same frame.

The checked-in `content/textures/animation_preview.dds` and adjacent
`animation_preview.json` are one sprite-sheet asset with the same basename.
The manifest lists it once as `sprite_sheet`: the loader reads both the
texture and strip definitions. The original geometric artwork and generator
are MIT-licensed; keep `animation_preview.LICENSE.txt` when copying them.
[The source record](source/README.md) describes the pixels and regeneration
command. No image tool or Python is required to build the checked-in content.

The [Sprites and text guide](https://labradorengine.com/docs/guides/sprites-and-text/#sprites-from-a-sheet)
quotes the named resolution, sample clock and public `frame_rect` draw path.
Timing tests use in-memory sheets, reader tests inspect the checked-in pair,
and native-null cases record frame UVs through the real named loader
route and a texture release/reload. Those checks produce no pixels and do not
establish real device-loss recovery or how the previews look.
The hidden-window native-null case additionally drives GalleryState's clock,
overlay, point filter and R press/hold behavior with the delivered manifest.
A second hidden-window case feeds the gallery synthetic mouse, touch and key
events and checks its pointer rules from the status lines: a click on a label
only chooses, each half of a value steps its way, a release off the pressed
row does nothing, a slider is grabbed on or just before its track and clamps
past its end, a key drops a drag, and a tap on Quit closes the window.

## The gallery

From the repository root in the x64 Native Tools Command Prompt. In a starter
project the repository root is `external\labrador`, and
[Run the samples](https://labradorengine.com/docs/guides/samples/) has every
step with its time, output and failures:

```bat
cmake --preset x64-debug
cmake --build --preset x64-debug --target ComponentsGallery ComponentsTests
.\out\build\x64-debug\samples\components\ComponentsGallery.exe
ctest --preset x64-debug -R "^ComponentsTests$"
```

| Control | Action |
| --- | --- |
| Up, Down, D-pad or left stick | Choose a row |
| Left, Right, D-pad or left stick | Change it |
| Click or tap a row | Choose it; on a value, the left half steps back and the right half on |
| Drag a slider's track | Set it, clamped at either end |
| Enter, Space or gamepad A on Quit | Quit |
| R or gamepad X | Restart both animation previews |
| Escape or gamepad B | Quit |

The Window row offers **Windowed** and **Full screen**, the two modes
Application can request. On Windows, full screen is already borderless; it is
not a third mode. Its callback calls `Application::set_fullscreen`, and both
initialization and every update use `fullscreen()` and `set_index()` to follow
the current report without firing the callback again. A browser can defer or
refuse the request, or leave full screen externally, so the status line says
which mode was requested rather than claiming it has been entered.

Difficulty and the two volume sliders demonstrate callbacks. The gallery
loads no sound bank and their status text does not change playback. The
[Settings and saves guide](https://labradorengine.com/docs/guides/settings-and-saves/#apply-settings-to-the-running-game)
shows the compiling window callback and how a game's copied slider scales a
percentage into the existing sound groups, with wave/effect assignments on its
SoundBank. No settings are saved by this gallery.

`ComponentsTests` includes a real two-choice row following external fullscreen
snapshots without calling its callback. It tests that synchronization recipe,
not GalleryState's polling or a platform fullscreen transition.

It draws its bars from the engine's own white texture, `white_texture_name`,
its panel from `content/textures/panel.dds`, a 16-texel premultiplied frame
with 6-texel corners, its previews from the animation atlas, and its text from
the minimal sample's font, which the
build copies beside it from `../minimal/content/fonts/`. That folder and,
on Windows, the template's DPI awareness declaration
`../minimal/dpi_aware.manifest` are the two paths outside this one the
gallery uses, and the components themselves use none. A copy of the gallery
puts the `.spritefont` in its own `content/fonts/` and points the three lines
of `CMakeLists.txt` that reach into it there, and takes the manifest into its
own folder and repoints its one path, or removes that path from its
`add_executable` call and lets Windows stretch the window on a scaled
display. Under a `wasm-*` preset it is a page, `ComponentsGallery.html`, with
the same content packed into the module.
`ComponentsPageTests` is configured to load the browser build, move a slider
and an option row, restart the previews with R, and quit. Its window-row
adjustment requests full screen. The sequence sends Escape twice because the
browser can consume the first press to leave full screen before the gallery
receives its quit key. The key sequence alone does not establish fullscreen
or animation acceptance; the animated frames, the restart and the window row
are looked at on screen, natively and in a browser.
`ComponentsPointerPageTests` drives the same page with a real mouse and
Chromium's touch emulation through the canvas's CSS box
(`web/pointer_input.mjs`): it drags the music slider from the track's middle
past its end, lets go of a press on another row, taps the left half of the
difficulty's value and checks it stepped once, and taps Quit. It reads each
change from the console line `main_web.cpp` writes for every status. Emulated
touch is not a touchscreen: a real phone's tap is an open check.

## What a component is not

It is not an engine API. It is not versioned, and it does not depend on
another component. Once it would need to depend on one, it has grown into a
framework, and it should be split or argued into the engine instead.

A gameplay piece such as a character controller, or a camera that follows,
can only be a component, never engine code. Its rules belong to a genre, and
the engine already supplies the collision, input and views it is built from.
