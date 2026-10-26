# Settings guide persistence check

The guide *Settings and saves* quotes `settings.cpp` and `persist.js` directly.
The C++ has no engine dependency: Windows gets the per-user LocalAppData path
through the known-folder API, then writes JSON with the standard library.
The browser uses the same writer below an IDBFS mount.

From an Emscripten environment at the repository root:

```bat
mkdir out\save-check
em++ tools/save_check/settings.cpp -std=c++20 -Wall -Wextra -Wpedantic -Werror -fwasm-exceptions -O1 -lidbfs.js --pre-js tools/save_check/persist.js --shell-file tests/render/web/shell.html -sEXIT_RUNTIME=0 -o out/save-check/Settings.html
node tests/render/web/run_page.mjs --browser "C:\Program Files\Google\Chrome\Application\chrome.exe" --page out/save-check/Settings.html --input tools/save_check/reload.mjs --timeout 30
```

The existing test shell supplies the page runner's completion hooks. Its main
exit hook is unused because the runtime stays live for the exported writer.
The page runner creates a fresh browser profile, serves the page over HTTP
and fails on console errors. `reload.mjs` checks the empty first load,
writes volume 37 through the compiled C++ function, awaits `FS.syncfs(false)`,
reloads on the same origin/profile and checks the JSON read back by C++ main.
This tests browser persistence, not storage retention after the user clears
site data or a browser evicts it. It is not a real-device acceptance run.

To check the Windows branch without writing to a user's application-data
folder, compile it without linking from the x64 Native Tools Command Prompt:

```bat
cl /nologo /std:c++20 /EHsc /W4 /WX /permissive- /c tools\save_check\settings.cpp /Foout\save-check\settings.obj
```

A Windows game using `settings_directory()` links `shell32.lib` and `ole32.lib`.
