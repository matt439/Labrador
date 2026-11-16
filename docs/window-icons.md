# Native window titles and optional icon resources

The game chooses its native title. `ApplicationOptions::window_title` sets
the initial text; `Application::set_window_title(...)` changes it later on
the application's thread. A direct window owner uses `Window::set_title`.
These native operations do not change a browser page's document title or
favicon. The host owns those in its HTML.

An icon is an optional resource compiled into the Windows executable.
`ApplicationOptions::window_icon_resource` and the corresponding WindowOptions
field select a named icon resource in that process executable. A supplied
name must load successfully; a missing explicit resource stops startup with
the requested name. It is a resource name, not a filename or a disk-image
lookup at run time.

With no explicit choice the existing default remains: try the executable's
named `IDI_ICON` resource and leave the class icon empty if it is absent,
letting Windows provide its ordinary fallback. The native backend borrows
the shared handles returned by LoadIconW; it does not destroy those shared
resources. A browser ignores the native icon choice.

## Add the icon you supply

This optional recipe needs the new window-icon API and starter build files;
the `v0.3.0` starter does not contain them. Keep your own project's title and
artwork. No user branding is supplied by the engine or this recipe.

Choose an `.ico` file you have the right to ship. Put it in your game's source
folder as `C:\dev\mygame\game\app.ico`. The starter includes `game_icon.rc`,
whose resource name is the literal string `GAME_ICON`. Its CMake option
supplies your file to the Windows SDK resource compiler. It adds no separate
installer or runtime image decoder.

Quit the game if it is running, so Windows can replace its executable. In the
x64 Native Tools Command Prompt, from `C:\dev\mygame`:

```bat
dir game\app.ico
cmake --preset x64-debug "-DGAME_ICON_FILE=C:/dev/mygame/game/app.ico"
cmake --build --preset x64-debug
ctest --preset x64-debug --output-on-failure
```

The path must be absolute and name an existing `.ico` file. Keep the quotes
when it has spaces. Semicolons are refused explicitly because CMake uses
them as list separators. A missing file fails configure naming
`GAME_ICON_FILE` and the path. An invalid icon file fails resource compilation;
correct or replace that source file, then build again.

When selected, CMake adds `game_icon.rc` and `GAME_HAS_ICON_RESOURCE` to the
native game target. `main.cpp` then explicitly requests `L"GAME_ICON"`.
Replacing `app.ico` rebuilds the resource because the file is an object
dependency. With an empty option, no icon resource or request is added.
The browser branch does not compile the native resource; its host still
chooses its own title and favicon.

Each build folder has its own choice. To turn the optional icon off in Debug:

```bat
cmake --preset x64-debug -DGAME_ICON_FILE=
cmake --build --preset x64-debug
```

## Build the release with the same choice

Repeat the option for the Release preset; its cache is separate from Debug.
Use a fresh output folder for installation:

```bat
cmake --preset x64-release "-DGAME_ICON_FILE=C:/dev/mygame/game/app.ico"
cmake --build --preset x64-release
ctest --preset x64-release --output-on-failure
cmake --install out/build/x64-release --prefix C:/dev/mygame-release
C:\dev\mygame-release\MyGame.exe --smoke-test
```

The icon is inside `MyGame.exe`. Neither `app.ico` nor `game_icon.rc` belongs
beside the installed executable; the existing release file list is unchanged.
The installed startup check makes the same explicit resource request, so it
fails if the selected resource is absent. Run the installed game normally to
inspect its Explorer and taskbar appearance on the target Windows desktop.
Automated resource loading and hidden-window checks do not establish how
its sizes look at different display scales.

The consumer fixture additionally checks that the native entry actually has
its opt-in compile definition and reads the installed named icon group and
referenced image payloads. A default startup succeeding alone cannot prove
that a requested icon was built into the program. Existing physical scaling
acceptance remains separate from these metadata and startup checks.

In the standalone engine sample, the equivalent native option is the same
`GAME_ICON_FILE`; run the preset from the repository root and select your own
absolute path. The test resource under `tests/app/` is only a one-pixel
file-format fixture and is not the starter's artwork.
