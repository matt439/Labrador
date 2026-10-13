# Minimal sample

The new-project template: a greeting that moves and circles, a confirm-to-quit
state, and a marker at every touch point. It is the folder a new game copies;
[Start your own project](https://labradorengine.com/docs/get-started/your-project/)
is the walkthrough.

## Making it your project

The walkthrough covers adding the engine as a submodule, rewriting the includes,
which name `samples/minimal/...` here because they are written from the root of
the repository they sit in, the `CMakeLists.txt` that replaces this one, and
the project's own root build files: its `CMakeLists.txt`, `vcpkg.json`,
`CMakePresets.json` and `.gitignore`.

## A copy is yours

This folder is meant to be copied, not linked
([PHILOSOPHY.md, The public face](https://github.com/matt439/Labrador/blob/master/docs/design/PHILOSOPHY.md#the-public-face)).
Once copied it is your game's code. Nothing updates it, and like the
components, which that section says carry no compatibility promise, it carries
none: a later version of this folder can differ from yours in any line.

What changes in the engine, and what a game moving its engine pin has to do
about it, is recorded in
[CHANGES.md](https://github.com/matt439/Labrador/blob/master/CHANGES.md). The
list below is the other half, newest first: the changes to what the code here
does or how it builds, and the engine behaviour changes it depends on, that a
copy taken before them would want.

- **2026-10-11, `eaa9b3f`.** A browser build: `main_web.cpp`, `web/shell.html`
  and the `EMSCRIPTEN` branch at the top of `CMakeLists.txt`. `HelloState`
  draws a marker at every touch point. CHANGES.md's `eaa9b3f` entry is the
  engine side.
- **2026-10-10, `4175640`.** `Application::initialize()` takes no arguments, so
  `main.cpp` no longer passes `wWinMain`'s instance and show command, and
  includes `<Windows.h>` itself. A source break; CHANGES.md's `4175640` entry
  is the migration.
- **2026-09-17, `92966b0`.** No code here changed, but what it does did: a
  relative manifest path now resolves against the executable's directory
  rather than the working directory, so the copy opens its content however it
  is started. CHANGES.md's `92966b0` entry says what a game sees.
- **2026-08-27, `94076f1`.** `CMakeLists.txt` no longer calls `find_package` for
  `xaudio2redist` and `directxtk`, and the states no longer say
  `using namespace DirectX`.
- **2026-08-26, `d3de8f6`.** Both states read the keyboard as well as the pad,
  and neither wraps its input in `pads.connected(0)`, so a copy can be moved
  and quit on a machine with no pad.
- **2026-08-15, `6b5c0bd`.** The engine's name throughout: the namespace
  `artattack` is `labrador`, the targets `ArtAttackEngine` and
  `artattack_settings` are `LabradorEngine` and `labrador_settings`, and this
  sample's target is `MinimalSample`.
- **2026-08-15, `5c2fa54`.** `main.cpp` sets `options.view_capacity = 1`. The
  default is four, and every view above what a frame draws is recording state
  created at startup and never used.

Labrador adds a line here in the commit that makes such a change.

## The font, and its licence

`content/fonts/courier_new_bold_16.spritefont` is the template's only font. It
is a glyph atlas rendered with Microsoft's `MakeSpriteFont` from Courier New, a
Windows system font whose copyright is Monotype's. Its redistribution terms are
not Labrador's to grant
([NOTICE](https://github.com/matt439/Labrador/blob/master/NOTICE)), and this
repository does not settle them: the licence description inside the font file permits using it "to
create, display, and print content" as the terms of the Microsoft product it
came with allow, and prohibits any other use, and nothing
here records whether an atlas shipped inside a game is such content. **Whether
your game may ship it is yours to check.** The collision, components,
local_multiplayer and audio samples embed the same atlas, and so does the hosted
LineSweeper demo. Replacing it with an openly licensed font is an open story,
[OCA-75](https://github.com/matt439/Labrador/blob/master/docs/product/STORIES.md#oca-75).

To replace it before you ship, install a font you may redistribute in this form,
such as one under the SIL Open Font License, and build an atlas from it with
[MakeSpriteFont](https://github.com/microsoft/DirectXTK/wiki/MakeSpriteFont),
from this folder:

```powershell
MakeSpriteFont "Your Font" content\fonts\your_font_16.spritefont /FontSize:16
```

Then change `courier_new_bold_16` to the new name in `content/manifest.json`,
in the `font_name` constant in `states/hello_state.cpp` and
`states/confirm_state.cpp`, and in the `.spritefont`
path the browser build lists in `CMakeLists.txt`, and delete the old atlas. The
[sprites and text guide](https://labradorengine.com/docs/guides/sprites-and-text/#making-textures-and-fonts)
says where to get the tool and what its output holds.
