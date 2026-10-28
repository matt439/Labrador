# Components

Small pieces a game copies into its own tree, rather than links. Each is a
header and a source file, small enough to read in one sitting, with its tests
beside it. The gallery shows menu rows and two animation previews on one screen.

| Component | Files | What it is |
| --- | --- | --- |
| Option row | `option_row.h`, `option_row.cpp` | A label and a value picked from a fixed list with left and right: a difficulty, a window mode. Wraps or stops at the ends. |
| Slider | `slider.h`, `slider.cpp` | A label, a bar and a number for an integer in a range: a volume, a sensitivity. Clamps, never wraps. |
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
cases also record drawings and load the actual manifest's sprite-sheet entry,
without creating a window or graphics/audio API device.
When both native render and audio backends are null, a gallery integration
case uses a hidden window and synthetic keyboard snapshots to check the real
overlay and R restart binding. It runs no Application loop or API device.

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

## The gallery

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
build copies beside it from `../minimal/content/fonts/`. That folder is the one path outside this one
the gallery uses, and the components themselves use none: a copy of the
gallery puts the `.spritefont` in its own `content/fonts/` and points the
three lines of `CMakeLists.txt` that reach into it there. Under a `wasm-*`
preset it is a page, `ComponentsGallery.html`, with the same content packed
into the module.
`ComponentsPageTests` is configured to load the browser build, move a slider
and an option row, restart the previews with R, and quit. Its window-row
adjustment requests full screen. The changed page path, fullscreen external
exit/re-sync, animated frames and restart still need a recorded browser run;
native gallery appearance also needs review. The key sequence alone does not
establish fullscreen or animation acceptance.

## What a component is not

It is not an engine API. It is not versioned, and it does not depend on
another component. Once it would need to depend on one, it has grown into a
framework, and it should be split or argued into the engine instead.

A gameplay piece such as a character controller, or a camera that follows,
can only be a component, never engine code. Its rules belong to a genre, and
the engine already supplies the collision, input and views it is built from.
