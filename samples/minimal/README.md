# Minimal sample

The new-project template: a greeting that moves and circles, a confirm-to-quit
state, and a marker at every touch point. It is the folder a new game copies;
[Start your own project](https://labradorengine.com/docs/get-started/your-project/)
is the walkthrough.

## The font, and its licence

`content/fonts/courier_new_bold_16.spritefont` is the template's only font. It
is a glyph atlas rendered with Microsoft's `MakeSpriteFont` from Courier New, a
Windows system font whose copyright is Monotype's. Its redistribution terms are
not Labrador's to grant ([NOTICE](../../NOTICE)), and this repository does not
settle them: the licence description inside the font file permits using it "to
create, display, and print content" as the terms of the Microsoft product it
came with allow, and prohibits any other use, and nothing
here records whether an atlas shipped inside a game is such content. **Whether
your game may ship it is yours to check.** The collision, components,
local_multiplayer and audio samples embed the same atlas, and so does the hosted
LineSweeper demo. Replacing it with an openly licensed font is an open story,
[OCA-75](../../docs/product/STORIES.md#oca-75).

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
