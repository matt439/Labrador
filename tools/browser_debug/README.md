# Browser-only C++ debugging probe

A deliberately wrong calculation for the browser debugging guide. It links no
engine and does not change the starter or a Release build. Portable rules bugs
belong in native tests and a native debugger first; this probe teaches the
browser tools for a problem that only reproduces there.

Qualified on Windows with Emscripten **6.0.12**, Chrome for Testing
**151.0.7922.34** and official C/C++ DevTools Support (DWARF)
**0.2.5854.1**. [The dated evidence](../../docs/browser-debugging/2026-11-17/README.md)
records the source breakpoint, named locals, step and corrected result.

## Build a separate Debug page

From the repository root, after `emsdk_env.bat`:

```bat
mkdir out\browser-debug-probe
copy tools\browser_debug\browser_fault.cpp out\browser-debug-probe\
copy tools\browser_debug\index.html out\browser-debug-probe\
cd out\browser-debug-probe
em++ browser_fault.cpp -g -O0 -fdebug-compilation-dir=. -sERROR_ON_WASM_CHANGES_AFTER_LINK=1 -sEXIT_RUNTIME=0 -o browser_fault.js
python -m http.server 8765 --bind 127.0.0.1
```

Keep that prompt serving. The source stays beside its wasm so the debugger
can fetch it through the same loopback server. The command compiles and links
with full `-g` DWARF; `-O0` keeps the calculation straightforward to step.
`-fdebug-compilation-dir=.` makes the source path relative to the served wasm,
without granting access to local files. A source map alone does not supply
this route's named C++ locals.

## Prepare the browser tools separately

Unpack the [official Chrome for Testing Windows x64 build](https://storage.googleapis.com/chrome-for-testing-public/151.0.7922.34/win64/chrome-win64.zip)
to a tools folder, for example `C:\dev\cft`. The release is listed in
[Google's availability manifest](https://googlechromelabs.github.io/chrome-for-testing/known-good-versions-with-downloads.json).
This is a separate testing browser, not a replacement for normal Chrome.

In a second prompt at the repository root:

```bat
python tools\browser_debug\prepare_extension.py --out out\browser-debug-probe\dwarf
set "debug_profile=%CD%\out\browser-debug-probe\profile"
set "debug_extension=%CD%\out\browser-debug-probe\dwarf"
"C:\dev\cft\chrome-win64\chrome.exe" --user-data-dir="%debug_profile%" --load-extension="%debug_extension%" --disable-extensions-except="%debug_extension%" --no-first-run --no-default-browser-check --auto-open-devtools-for-tabs http://127.0.0.1:8765/index.html
```

The helper downloads only the official Web Store item
[`pdcpmagijalfljmkmjngeonclgbbannb`](https://chromewebstore.google.com/detail/cc%2B%2B-devtools-support-dwa/pdcpmagijalfljmkmjngeonclgbbannb),
checks its qualified version and SHA-256, and unpacks it into the requested
empty folder. It does not install a system extension or change an existing
browser profile. If the official download changes, it stops before extraction;
the new bundle needs qualification rather than silently inheriting this result.

Use Chrome for Testing for this command. Regular Chrome and Edge removed the
extension-loading command-line flags; their ordinary profiles are not part of
this recipe. The extension is beta software; the tested pair above is the claim,
not every Chrome/extension combination. [Chrome's C++ debugging guide](https://developer.chrome.com/docs/devtools/wasm)
describes the DWARF extension, and [Playwright's extension guide](https://playwright.dev/docs/chrome-extensions)
explains the testing-browser boundary.

## Follow the fault

1. In DevTools choose **Sources**, expand the loopback server and open
   `browser_fault.cpp`. A source file containing `browser_score` must appear;
   wasm instructions alone do not establish this setup.
2. Click the gutter beside the `total_points` assignment, line 7 in this probe.
   Click **Calculate score for two players** on the page. The debugger pauses
   in `browser_score`, with `player_count: 2` and `points_per_player: 10` in
   **Scope**. `total_points` has not been initialized yet; its displayed storage
   value before the assignment is not a C++ result.
3. Click **Step over** or press **F10** with DevTools focused. The highlighted
   source line advances to the `printf`, and **Scope** shows `total_points: 12`.
   The intended total is 20: the calculation adds instead of multiplying.
4. Resume with **F8**. Change `+` to `*` in the copied source, run the same
   `em++` command again, and hard reload with **Ctrl+Shift+R**. The same source
   breakpoint and step now show `total_points: 20`; resuming displays actual 20.

**Console** shows the two startup lines: stdout as ordinary output and stderr
as the deliberately red diagnostic. A missing wasm prevents `main` from
running: a hard reload with only `browser_fault.wasm` temporarily withheld
instead shows HTTP 404, loader/abort messages and the page's failure line.
Restore the file and reload after that exercise. The fixture's intentional
stderr line is not a passing no-console-errors page test.

Close the testing browser and stop the loopback server with Ctrl+C when done.
Keep all of this in the separate ignored folder. Do not ship the probe,
extension, profile, source files or DWARF as part of the normal game bundle.
