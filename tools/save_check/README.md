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

On 2026-10-27 the compiled check passed in headless Chrome 156.0.8078.48.
The first C++ read returned `{}`; the compiled writer saved volume 37,
`FS.syncfs(false)` completed, and C++ read `{"version":1,"volume":37}` after
reload on the same origin and fresh task profile. The driver owns completion
across navigation and rejects unexpected console or browser complaints.

To check the Windows branch without writing to a user's application-data
folder, compile it without linking from the x64 Native Tools Command Prompt:

```bat
cl /nologo /std:c++20 /EHsc /W4 /WX /permissive- /c tools\save_check\settings.cpp /Foout\save-check\settings.obj
```

A Windows game using `settings_directory()` links `shell32.lib` and `ole32.lib`.

## Versioned recovery example (OCA-112)

The separate `recovery_*` files are the copyable advanced example in
[Recover a failed save](../../website/src/content/docs/docs/guides/save-recovery.mdx).
The basic writer above remains a small truncating example. The advanced store
uses Labrador's checked JSON reader and its own fixed numeric serializer;
schema and recovery are game code, not an engine service.

`settings.json` version 1 migrates `volume` to `effects_volume` in memory only.
Version 2 alternates `settings-a.json` and `settings-b.json` with a positive
generation. The exclusive `settings-pending.json` draft is published only to
the inactive slot. Until a successful acknowledgement the previous slot is
never rotated into the replacement role. Unknown/duplicate fields, invalid
values, malformed/future data and partial drafts block ordinary saves and
stay byte-identical. A validated interrupted draft can explicitly retry.
Fresh-copy recovery chooses a validated value or game defaults, leaves every
original intact and refuses an existing destination. Loading writes nothing.

One writer per directory/tab is required. Snapshot checks detect observed
external edits; they do not implement a concurrent transaction. Windows
checks exclusive creation, writes, draft flush/close and inactive publication
with ReplaceFileW or MoveFileExW. Replacement failure may alter the inactive
pathname, which is rescanned before retry. None of this promises power-loss
durability. The browser waits for restore before C++ main and awaits sync
before acknowledgement. Failed sync may still have persisted new bytes;
preservation of the old slot is not a rollback promise. Eviction and cleared
site data remain browser storage limits. No unload handler or backup runs.

From the engine checkout, create an **owned, new** assembled project:

```bat
python tools\consumer_check\assemble.py C:\dev\save-game
cd /d C:\dev\save-game
python external\labrador\tools\consumer_check\save_fixture.py .
cmake --preset x64-debug
cmake --build --preset x64-debug --target MyGameSaveTests
ctest --preset x64-debug -R "^MyGameSaveTests$" --no-tests=error --output-on-failure
```

The fixture copies the advanced files to `game/save_example`, rewrites their
own include paths and adds opt-in targets. It changes no ordinary game
source and refuses to overwrite a previous fixture. `MyGameSaveTests` uses
only exclusively created disposable paths: native system temporary paths,
and a verified owned child of the Node test's working directory under wasm
NODERAWFS. The test checks ownership again before recursive cleanup.

From an Emscripten environment in that project, also check the compiled
browser store and page (replace `<chrome.exe>` with a real Chromium path):

```bat
cmake --preset wasm-debug
cmake --build --preset wasm-debug --target MyGameSaveTests MyGameSavePage
ctest --preset wasm-debug -R "^MyGameSaveTests$" --no-tests=error --output-on-failure
"%EMSDK_NODE%" external\labrador\tests\render\web\run_page.mjs --browser "<chrome.exe>" --page out\build\wasm-debug\MyGameSavePage.html --input external\labrador\tools\save_check\recovery_reload.mjs --timeout 60
```

Use `wasm-release` for CI's Release configuration. Serve the same page over
HTTP for manual Apply, Retry and fresh-copy recovery controls. The demo's
single `/save/recovered` profile is selected by `?profile=recovered` only
after successful persistence. Choosing an existing recovered directory is
refused; this is not a general profile manager.

On 2026-11-17 an assembled starter passed MSVC native-null
`MyGameSaveTests`, `MyGameTests` and `MyGameSmokeTests` (3/3), and Emscripten
6.0.12 compiled/ran `MyGameSaveTests` under Node. Chrome 157.0.8103.78 in the
runner's fresh profile passed acknowledged volume 37/reload, a real aborted
IndexedDB read/write transaction/reload of 37, failed sync followed by retry
and reload of 52, future-version refusal and fresh-profile recovery retaining
original bytes. This is that transaction-abort case, not every quota/disk
failure or power-loss condition. Native fixtures denied a real inactive
replacement with a Windows sharing lock and retried after release; no live
user save was opened.

`recovery_persist.test.mjs` holds the browser glue's failure/queue/startup
contracts under Node VM. Its Python launcher and `test_save_fixture.py` run
in the ordinary offline tooling suite. The existing native and browser
consumer jobs add the compiled fixture after baseline/material checks;
there is no new required job or default starter test.
