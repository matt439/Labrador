# MyGame

This is your game's source folder. Make your game changes here, under `game/`
in the project. `external/labrador/` contains the complete engine source,
including its original `samples/minimal/`; edit `game/states/hello_state.cpp`
when following a guide about your first screen.

The initial game moves a greeting with the arrow keys, WASD or a pad's left
stick. Escape or a pad's B button opens the quit question; Enter or A confirms
and Escape or B cancels. A touch shows a marker at its position.

| File, from the project root | What you change there |
| --- | --- |
| `game/main.cpp` | Windows startup, window options, content and the first screen. |
| `game/main_web.cpp` | Browser startup using the same screens and content. |
| `game/states/hello_state.h` and `.cpp` | The moving greeting, input and drawing. |
| `game/states/confirm_state.h` and `.cpp` | The question drawn over the first screen. |
| `game/rules/motion.h` and `.cpp` | Movement values and rules that take plain inputs. |
| `game/tests/motion_tests.cpp` | Expected results for those rules. |
| `game/content/manifest.json` and `game/content/fonts/` | The content the game loads. |
| `game/web/shell.html` | The browser page, its title, canvas and status text. |
| `game/CMakeLists.txt` | The game's source lists, targets, tests and release files. |

Run these commands from the project root, the folder above `game/`, in the
x64 Native Tools Command Prompt after installing the prerequisites:

```bat
cmake --preset x64-debug
cmake --build --preset x64-debug
ctest --preset x64-debug
out\build\x64-debug\game\MyGame.exe
```

The program is the `MyGame` target. `MyGameRules` holds its rules and links
`LabradorMath`; `MyGameTests` runs those rules with no window or device.
On Windows, `MyGameSmokeTests` also starts the game hidden, draws its first
frames and quits. A browser build runs `MyGameTests` under Node and produces
`out/build/wasm-debug/game/MyGame.html` or the corresponding `wasm-release`
page. The browser guide explains the tools and how to serve that page.

Useful guides on the development documentation site:

- [Read your starter's C++](https://labradorengine.com/docs/concepts/read-your-code/).
- [Make a first change](https://labradorengine.com/docs/get-started/first-change/).
- [Rename your game](https://labradorengine.com/docs/guides/your-name/), including the initial window and page titles. Update this README's names when you rename it.
- [Test your game](https://labradorengine.com/docs/guides/testing/).
- [Build for the browser](https://labradorengine.com/docs/guides/browser/).
- [Ship a Windows release](https://labradorengine.com/docs/guides/windows-release/).

A release ZIP's root `README.txt` links that release's own documentation.
The development guides above can describe features newer than your engine;
use the release's pages or follow their update notes.

The engine's [original minimal-template README and change list](../external/labrador/samples/minimal/README.md)
remain available in full. They describe `MinimalSample`, `MinimalRules` and
the other targets inside the engine repository. This game's `CMakeLists.txt`,
README and copy-specific comments are adapted during starter assembly; its
rules and startup behavior come from that template. Updating the engine
does not overwrite your `game/` changes. Follow [Update your starter](https://labradorengine.com/docs/guides/starter-update/)
or [Move your engine pin](https://labradorengine.com/docs/guides/move-pin/) for
your project's engine arrangement.
