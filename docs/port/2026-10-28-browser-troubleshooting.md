# Browser troubleshooting verification, 2026-10-28

OCA-24 and OCA-25 publish the browser failures a game author can meet. This
record owns the reproductions behind the troubleshooting page. Source:
`6365ae4`, with documentation edits only. The dated STORIES.md keeps its
original acceptance text; the current browser test totals are 19 without a
Chromium, 32 in Debug with one, and 33 in Release with one. Its old 17/27
comparison does not describe this tree.

## Tools and boundaries

Windows 11, CMake 4.3.1-msvc1, Emscripten 6.0.12 and Chrome
156.0.8078.48 (DevTools 1.3). Configure experiments used isolated folders under
`out/browser-troubleshooting/`; they did not reconfigure the existing wasm
builds. The missing-browser and test-count checks configure and enumerate
tests; they do not build or run those tests.

The file-URL and unavailable-WebGL2 cases loaded the existing Debug minimal
sample in separate headless Chrome profiles through DevTools. Each browser
was closed after collecting the page text and console messages. No remote
site was involved. These are failure reproductions, not hardware, listening,
Safari or phone acceptance.

## Missing EMSDK

With `VCPKG_ROOT` naming the installed vcpkg and `EMSDK` removed from the
configure process's environment:

```text
cmake --preset wasm-debug -B out/browser-troubleshooting/no-emsdk
```

Configure failed before selecting the browser compiler:

```text
include could not find requested file:

  /upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake
```

The later vcpkg, Ninja and compiler errors do not change that cause.

## Missing Chromium and current test totals

With `EMSDK=C:/Tools/emsdk`, a Ninja path supplied on the command line, and
the already-installed wasm dependencies reused without an install:

```text
cmake --preset wasm-debug -B out/browser-troubleshooting/no-chromium -DVCPKG_MANIFEST_INSTALL=OFF -DVCPKG_INSTALLED_DIR=C:/Projects/Labrador/out/build/wasm-debug/vcpkg_installed "-DCMAKE_IGNORE_PATH=C:/Program Files/Google/Chrome/Application;C:/Users/matsi/AppData/Local/Google/Chrome/Application" "-DCMAKE_MAKE_PROGRAM=C:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe"
ctest --test-dir out/browser-troubleshooting/no-chromium -N
```

Ignoring the two Chrome directories makes the real `find_program` lookup
miss on this machine. Configure printed:

```text
No Chromium found, so RenderPixelTests, RenderPixelTestsWithoutS3tc, RenderContextLossTests, WebWindowTests and the samples' pages are built and not registered with ctest. Set LABRADOR_TEST_BROWSER to run them.
```

ctest printed `Total Tests: 19`. Configuring into two further fresh folders,
without the ignore path and with
`-DLABRADOR_TEST_BROWSER=C:/Program Files/Google/Chrome/Application/chrome.exe`,
gave `Total Tests: 32` for `wasm-debug` and `Total Tests: 33` for
`wasm-release`. Release's extra entry is `LineSweeperPacingPageTests`.

## Opening the page from a file URL

Navigated Chrome to
`file:///C:/Projects/Labrador/out/build/wasm-debug/samples/minimal/MinimalSample.html`.
After five seconds the page text was still `Loading...`. Chrome logged the
blocked local-file fetch and Emscripten logged:

```text
Aborted(Assertion failed: readAsync does not work with file:// URLs)
Aborted(both async and sync fetching of the wasm failed)
```

The final failure was also an uncaught rejected promise. The troubleshooting
entry directs the author to the guide's loopback HTTP server.

## WebGL2 unavailable

Started a fresh headless Chrome with `--disable-webgl2`, served the same
minimal sample over loopback HTTP with `application/wasm` for its wasm file,
and navigated to it. The console and visible page text both reported:

```text
This browser would not give the canvas a WebGL2 context. WebGL2 is in every current desktop browser; it can be turned off, and a GPU the browser has blocked takes it away.
```

The console message had the `MinimalSample: ` prefix. This deliberately
disabled browser capability checks the actual engine error and template
failure UI; it does not diagnose a particular physical GPU or browser block.

## Browser audio before activation

Rebuilt `AudioSample`, `AudioTests` and `AudioSampleTests` under `wasm-debug`
with vcvars and `emsdk_env.bat`. `AudioTests`, `AudioSampleTests` and
`AudioPageTests` passed (3/3). The following observations used the rebuilt
`AudioSample.html`, served over loopback HTTP by `run_page.mjs`, with a
temporary input driver and a fresh headless Chrome profile for each run.
The driver required every console and browser complaint to fail the run;
both runs passed without one.

The first run observed the real sample's Web Audio calls and source starts:

1. Before any input, `navigator.userActivation.hasBeenActive` was false,
   the backend's context was null and the visible status began
   `Press a key or click the picture to allow sound.`
2. Synthetic `KeyboardEvent` down/up pairs for `Digit1` and `Digit3` let
   the sample request a ping and a drone loop without granting browser
   activation. Calls through `Module.labradorAudio.play_wave` and `play`
   confirmed that both requests reached the backend. Wrapping the real
   `AudioBufferSourceNode.prototype.start` recorded no source starts;
   the context stayed null and the loop was active with no source.
3. DevTools `Input.dispatchKeyEvent` delivered a real `KeyA` down/up pair.
   The context became `running`. After 1.5 seconds exactly one source had
   started: the one-second looping drone. No ping was replayed.
4. A real `Digit1` down/up pair made a second one-shot request and started
   a new non-looping source with the ping's 0.16-second buffer. Escape then
   quit the sample cleanly.

The second run delivered Escape as the first real key. Activation remained
false, the context stayed null, and the page reported
`Quit. Reload the page to play again.` Escape did not unlock audio.

This directly observes dropped one-shots and waiting loops through the
sample's input, C++ sound board and actual browser audio backend. It records
source scheduling, not a person listening to speakers. The separate physical
listening acceptance remains open as `WEB#3` in TODO.md.
