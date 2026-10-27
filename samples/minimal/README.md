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
`CMakePresets.json` and `.gitignore`. Its tests come with it: `rules/` is
the game's rules in a library of their own, `tests/` runs them with no
window, and `--smoke-test` starts the game hidden and quits, both
registered with ctest
([Test your game](https://labradorengine.com/docs/guides/testing/)).
`main_web.cpp` and `web/shell.html` are the browser build, which the
walkthrough's Windows build does not compile;
[Build for the browser](https://labradorengine.com/docs/guides/browser/)
adds the presets that do.

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

- **2026-10-27.** The math library is `LabradorMath` and its types live in
  `labrador::`. `CMakeLists.txt` links `LabradorMath` where it linked
  `MattMath`, and the sources spell `labrador::Vector2F` and
  `using namespace labrador;` where they spelt `mattmath::`. A copy must make
  the same edits when it moves its engine pin; CHANGES.md has the commit.
- **2026-10-27.** Per-monitor DPI awareness. `dpi_aware.manifest` is new, and
  `CMakeLists.txt` lists it with the program's sources, so Windows no longer
  stretches the window and blurs its text on a scaled display. The window is
  then its requested size in physical pixels. A copy wanting the same adds the
  file and the line; the Windows release guide's *Scaled displays* section
  says what it changes.
- **2026-10-27.** Windows resolution changes resize the window without
  requesting activation. A hidden smoke-test window stays hidden and inactive;
  CHANGES.md's *Keep window resizing from requesting activation* entry is the
  engine side. No template source migration is needed.
- **2026-10-27.** Quitting during update skips the tick's render after its
  remaining fixed updates. The smoke test draws its requested 30 completed
  frames and closes without another present against a destroyed Vulkan
  surface. A copy relying on a final draw after `Application::quit()` moves
  that work before quitting; `frame_timings()` keeps the last completed frame.
- **2026-10-26.** XAudio2 retries unavailable default output while active.
  A copy adding sound keeps its bank and voice handles after recovery but
  explicitly plays persistent sounds again; lost one-shots are dropped.
- **2026-10-26.** A single-player pad can occupy any slot. `HelloState::update()`
  reads every slot's B edge and deadzoned left stick, and `ConfirmState` reads
  every slot's A/B edges, alongside the keyboard. Several pads share control:
  directions add and the rules clamp the sum. A copy that reads slot 0 alone
  wants both loops so a remaining controller in a higher slot can move the
  greeting, open the quit dialog and answer it. This is game policy, not a new
  engine input service; the input guide quotes the sample's loop.
- **2026-10-25.** A release folder. `CMakeLists.txt` now has install rules,
  so `cmake --install out/build/x64-release --prefix <folder>` stages what a
  player gets: the program, `DirectXTK.dll` and `xaudio2_9redist.dll`, the
  MSVC runtime copied from Visual Studio's redist folder, `manifest.json` and
  `fonts/`, and a `notices/` folder with the licences of what the program
  carries and the engine's `NOTICE`. Only CMake's list of the runtime's DLLs
  is copied; a copy that uses `<threads.h>` or C++/CX adds
  `vcruntime140_threads.dll` or `vccorlib140.dll` itself. Installing a debug build stops, because its runtime may not be
  redistributed. A copy wants the block below `add_dependencies`, with its
  own target name, and an `install(DIRECTORY ...)` line for each content
  folder it adds; *Ship a Windows release* on the website is the guide.
- **2026-10-25.** No code here changed, but a copy's browser build needs less:
  under Emscripten the engine now chooses the `gl` renderer by itself and
  refuses `d3d11`, `d3d12` and `vulkan` at configure, so a project's `wasm`
  preset sets only the vcpkg toolchain, Emscripten's toolchain file chained
  after it and the `wasm32-emscripten` triplet. The walkthrough's
  `CMakePresets.json` has those presets and its `game/CMakeLists.txt` the
  `if(EMSCRIPTEN)` block that builds `main_web.cpp` and `web/shell.html`;
  *Build for the browser* on the website is the guide. CHANGES.md's *Add a
  browser build to the Get Started project* entry is the engine side.
- **2026-10-25.** Tests, from the start. The motion arithmetic that was in
  `HelloState::update()` - the stick and the keys summed and clamped, the
  centre moved, the spin advanced and the greeting placed - is in
  `rules/motion.h` and `rules/motion.cpp` as `step_motion` and
  `greeting_position`, with the tuning constants beside them, and
  `HelloState` holds a `Motion` where it held `position_` and `spin_`.
  `CMakeLists.txt` builds the rules as `MinimalRules`, which links `LabradorMath`
  and not the engine, tests them in `MinimalTests` from
  `tests/motion_tests.cpp` with doctest, and registers
  `MinimalSample --smoke-test`, which `main.cpp` answers by running the first
  state hidden, quitting after 30 frames and writing a startup failure to the
  standard error stream rather than a message box. A copy that wants the same
  takes the three new files, the `CMakeLists.txt` lines, and from `main.cpp`
  the `asks_for_smoke_test` parse of `wWinMain`'s `command_line`,
  `options.visible = !smoke_test`, the `SmokeTest` class that wraps
  `first_state`, and the `catch` block that writes to the standard error
  stream and skips the message box under a smoke test, and `doctest` in its
  `vcpkg.json`; the walkthrough's `game/CMakeLists.txt`, root
  `CMakeLists.txt` (`enable_testing()`) and `CMakePresets.json` (test
  presets) show the project's side.
- **2026-10-25.** The hint in `states/hello_state.cpp` says "B or Escape asks
  to quit" rather than "quits", and `web/shell.html`'s status line says Escape
  asks and Enter says yes, because quitting pushes `ConfirmState` first. A
  copy that kept the confirmation wants the same words.
- **2026-10-24.** No code here changed, but the engine's table now holds a
  one-texel white texture under `white_texture_name`
  (`engine/render/resource_factory.h`), so a copy can draw a solid rectangle -
  a bar, a divider, a backdrop - with `draw_sprite` over source
  `RectangleI(0, 0, 1, 1)` and a tint, and no texture in `content/`.
- **2026-10-22.** No build step changed, but `CMakeLists.txt` now says what its
  content mirror does not do: `copy_directory_if_different` never removes, so a
  file deleted from `content/` goes on loading from the build folder until it
  is cleaned. A copy with content it may delete, or an optional sound bank,
  wants the list-and-remove recipe in the assets guide's *Optional content and
  stale copies*.
- **2026-10-22.** No code here changed, but what the window does did: on
  Windows, F10 and Alt pressed alone are keys and no longer stop the loop in
  the system menu. Alt+Space still opens it. CHANGES.md's *Let F10 and Alt
  alone be keys, not the system menu* entry is the engine side.
- **2026-10-13.** The font is `content/fonts/jetbrains_mono_bold_16.spritefont`,
  JetBrains Mono Bold under the SIL Open Font License 1.1 with that licence as
  `OFL.txt` beside it, in place of `courier_new_bold_16.spritefont`, whose
  redistribution terms are Monotype's and were never settled here. A copy
  still carrying the Courier atlas swaps it as *Using another font* below
  says, with `jetbrains_mono_bold_16` as the new name: `content/manifest.json`,
  the `font_name` constant in both states, the browser build's path in
  `CMakeLists.txt`, and `OFL.txt` copied with the atlas. The new glyphs are
  taller, so text placed near the edge of the frame is worth looking at again.
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

`content/fonts/jetbrains_mono_bold_16.spritefont` is the template's only font:
an atlas of the 95 printable ASCII characters, rendered at 16 points from
JetBrains Mono Bold. JetBrains Mono is licensed under the SIL Open Font
License 1.1, and `content/fonts/OFL.txt`, beside the atlas, is that licence
with the font's copyright line at its top. Your game may ship the atlas, sold
or not, provided `OFL.txt` goes with every copy of it and the font is not sold
by itself. The atlas stays under the OFL rather than your game's licence, and
[NOTICE](https://github.com/matt439/Labrador/blob/master/NOTICE) records it with the repository's other third-party
terms. `CMakeLists.txt` copies the whole `content/fonts` folder beside the
executable, and the browser build embeds it, so the licence travels with the
atlas with nothing more to do. The collision, components, local_multiplayer
and audio samples load the atlas from here, and LineSweeper carries a copy.

### How the atlas was built

| | |
|---|---|
| Font | JetBrains Mono 2.304, `fonts/ttf/JetBrainsMono-Bold.ttf` from `JetBrainsMono-2.304.zip` in [its release](https://github.com/JetBrains/JetBrainsMono/releases/tag/v2.304) |
| `JetBrainsMono-2.304.zip` | sha256 `6f6376c6ed2960ea8a963cd7387ec9d76e3f629125bc33d1fdcd7eb7012f7bbf` |
| `JetBrainsMono-Bold.ttf` | 277,828 bytes, sha256 `5590990c82e097397517f275f430af4546e1c45cff408bde4255dad142479dcb` |
| `OFL.txt`, from the same zip | 4,399 bytes, sha256 `30f0c136e3c88e422d0791acd97238870f9054a9729bc34cf2ff0d4ed8cac4ad` |
| Settings | `MakeSpriteFont`'s, from the DirectX Tool Kit `may2026` release, below |
| The atlas | 24,584 bytes, sha256 `4eed8bab40cd7c21856e9da166a42a956a63910c57051145571f13d25e7b95b1`: 95 glyphs, a line spacing of 28.16 pixels, a 128x168 BC2 texture, and every glyph steps the pen 13 pixels |

With `JetBrainsMono-Bold.ttf` installed, from this folder:

```bat
MakeSpriteFont "JetBrains Mono" content\fonts\jetbrains_mono_bold_16.spritefont /FontSize:16 /FontStyle:Bold
```

The bytes in this folder were written by `MakeSpriteFont` built from its
`may2026` source with one line changed: the font is loaded from
`JetBrainsMono-Bold.ttf` through a GDI+ private font collection rather than
found by name among the installed fonts, so nothing was installed to build it.
The same build, given Courier New Bold both ways, by name and from its file,
writes the Courier atlas this template carried before byte for byte, so the
command above, with the font installed, writes the checksum in the table.

### Using another font

Install a font you may redistribute in this form and build an atlas from it
with [MakeSpriteFont](https://github.com/microsoft/DirectXTK/wiki/MakeSpriteFont),
from this folder:

```bat
MakeSpriteFont "Your Font" content\fonts\your_font_16.spritefont /FontSize:16
```

Then change `jetbrains_mono_bold_16` to the new name in `content/manifest.json`,
in the `font_name` constant in `states/hello_state.cpp` and
`states/confirm_state.cpp`, and in the `.spritefont`
path the browser build lists in `CMakeLists.txt`. Delete the old atlas and its
`OFL.txt`, and put the new font's licence beside its atlas if the licence asks
to travel with it. The
[sprites and text guide](https://labradorengine.com/docs/guides/sprites-and-text/#making-textures-and-fonts)
says where to get the tool and what its output holds.
