# Components

Small pieces a game copies into its own tree, rather than links. Each is a
header and a source file, small enough to read in one sitting, with its tests
beside it. The gallery shows all of them on one settings screen.

| Component | Files | What it is |
| --- | --- | --- |
| Option row | `option_row.h`, `option_row.cpp` | A label and a value picked from a fixed list with left and right: a difficulty, a window mode. Wraps or stops at the ends. |
| Slider | `slider.h`, `slider.cpp` | A label, a bar and a number for an integer in a range: a volume, a sensitivity. Clamps, never wraps. |

## Taking one

1. Copy the component's `.h` and `.cpp` into your game.
2. Change the namespace and the include paths to your game's, and list the
   `.cpp` in your `CMakeLists.txt`.
3. Change anything you like. The copy is yours: nothing here promises to stay
   compatible with it, and nothing in the engine knows it exists.

The tests (`*_tests.cpp`, with `test_content.h`) come too if you want them.
They need no window, no device and no content on disk.

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
bindings belong to the page. So a page:

- adds each component to a `FocusGroup`, which treats the whole row as one
  target, because a component is a `UiContainer`;
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

## The gallery

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug --target ComponentsGallery ComponentsTests
.\out\build\x64-debug\samples\components\ComponentsGallery.exe
ctest --preset x64-debug -R '^ComponentsTests$'
```

| Control | Action |
| --- | --- |
| Up, Down, D-pad or left stick | Choose a row |
| Left, Right, D-pad or left stick | Change it |
| Enter, Space or gamepad A on Quit | Quit |
| Escape or gamepad B | Quit |

It reuses the minimal sample's font, which the build copies beside it, and draws
its bars from the engine's own white texture, `white_texture_name`. Under a `wasm-*` preset it is a page,
`ComponentsGallery.html`, with the same content packed into the module.
`ComponentsPageTests` loads it in headless Chrome, moves a slider and an
option row, and quits.

## What a component is not

It is not an engine API. It is not versioned, and it does not depend on
another component. Once it would need to depend on one, it has grown into a
framework, and it should be split or argued into the engine instead.

A gameplay piece such as a character controller, or a camera that follows,
can only be a component, never engine code. Its rules belong to a genre, and
the engine already supplies the collision, input and views it is built from.
