# Keyboard rebinding — 2026-11-17

OCA-110 qualification on Windows 11, from detached base `5cff44b9` plus
OCA-109 feature `bbd9baa4` and its report-fixture follow-up `45c712c4`, followed
by the rebinding changes in the commit containing this record. Local
cherry-pick identities were `ad3c51d4` and `f1663063`. The adjacent
`receipt.json` records the exact source/output/image bytes and the read-only
browser runner dependency. Bindings, duplicate refusal and protected keys
are copied game policy; no engine action map or gamepad rebinding was added.

## Automated checks

From `C:\Projects\Labrador-agent-rebinding`, after the installed Visual Studio
2026 x64 environment and `VCPKG_ROOT` were loaded:

```bat
cmake --preset x64-debug-null
cmake --build --preset x64-debug-null --target ComponentsGallery ComponentsTests --parallel 2
ctest --preset x64-debug-null -R "^ComponentsTests$" --output-on-failure
cmake --preset x64-debug
cmake --build --preset x64-debug --target ComponentsGallery ComponentsTests --parallel 2
ctest --preset x64-debug -R "^ComponentsTests$" --output-on-failure
```

Both builds passed with MSVC 14.51.36231 and CMake 4.3.1-msvc1. The directly
run native-null suite passed 90 cases / 1,765 assertions; the D3D11 suite
passed 72 / 1,429. The retained logs are direct executable invocations from
their content-staged build directories. Null gallery cases drive the real
GalleryState in a hidden native window with separated synthetic key/mouse
snapshots and scripted pad snapshots. They run no Application loop or API
device. They cover opener/all-held release, pad A opener and B cancellation,
duplicate/protected refusal, accepted keys, hold/commit suppression,
cancel/reset, focus loss/deactivation, usable navigation/quit afterwards,
queued pointer releases against Escape and commit, and simultaneous
Enter/action-key/fresh-pointer input on both Binding and Name. That last
regression reproduced the entry/exit action leak before the priority fix.
The gallery follows its explicit reading order for Up/Down, so Name remains
keyboard reachable beside Binding. Portable tests also
cover multiple-key ambiguity, exact invalid tokens and atomic table loads.

After `C:\Tools\emsdk\emsdk_env.bat` (Emscripten 6.0.12, Node 24.19.0):

```bat
cmake --preset wasm-debug -DVCPKG_MANIFEST_FEATURES=materials
cmake --build --preset wasm-debug --target ComponentsGallery ComponentsTests --parallel 2
C:\Tools\emsdk\node\24.19.0_64bit\node.exe out\build\wasm-debug\samples\components\ComponentsTests.js
```

The portable wasm suite passed 72 cases / 1,429 assertions. The materials
feature supplies this checkout's standalone GL test authoring tools; the
rebinding example adds no dependency or tool requirement.

Desktop Chrome 157.0.8103.78 passed the new rebinding route and the unchanged
text/pointer routes, reporting ANGLE on NVIDIA GeForce RTX 5080 through
D3D11. The command was:

```bat
node C:\Projects\Labrador\tests\render\web\run_page.mjs --browser "C:\Program Files\Google\Chrome\Application\chrome.exe" --page out/build/wasm-debug/samples/components/ComponentsGallery.html --input samples/components/web/rebinding_input.mjs --timeout 90 --browser-warnings fail
```

For the regressions, substitute `text_field_input.mjs` and `pointer_input.mjs`.
The runner was the read-only current root version, recorded by its own file
hash and last-changing commit in the receipt. Its foreground-tab/isolated
profile fixes are qualification dependencies; this story does not copy
current root files into its source changes. The new route uses real canvas
key/focus/pointer events separated by rendered frames: held Enter,
duplicate D refusal, protected Down refusal, T commit without also firing,
old/new shortcut behavior, cancellation/reset, actual canvas blur, keyboard
navigation from Binding into Name, Name's
ownership of R/D while editing, outside-row cancellation and original Quit.

The website gate passed: check reported zero errors/warnings/hints, the Node
tests passed, and 181 pages built with valid internal links. The isolated
gate borrowed the already integrated OCA-93 `GuideVersionNote.astro`
(`95108c33`); it is a dependency, not part of this story's diff. Compiler-free
lint steps 3–11 passed. The full offline suite ran 396 tests with three skips
and one known base failure: `test_ci_build_retry` expects five native CI
builds, while the scripted-pad consumer adds a sixth direct invocation.
The integration branch already owns correction `85c9fb63`; this story does
not duplicate that unrelated workflow change. The per-commit declarations
are checked on this story commit separately. Source files are explicitly
enumerated in CMake.

## Visible native/browser checks

Computer Use inspected the owned native D3D11 gallery. A click opened the
binding capture row, and a click on Difficulty cancelled capture, restored
`Restart: r` and left the old row focused. Clicking the original Quit row
closed the owned window. The helper/status lines and neighbouring Name field
fit inside the panel. Its short injected D/Escape taps produced no observed
keyboard edge, so these screenshots do not qualify native injected-key
capture. The separated native-null snapshots and real browser events above
carry that behavioral evidence; no physical keyboard or pad was exercised.

| Image | Observation |
| --- | --- |
| [native-capture.png](native-capture.png) | Focused capture row says `Press one key`; Name and the separate status areas fit. |
| [native-cancelled.png](native-cancelled.png) | An outside press restores the old assignment and says `Binding capture cancelled: another row.` |
| [browser-duplicate.png](browser-duplicate.png) | Duplicate D is refused visibly; capture waits for release and the Name column remains separate. |

The native images are unedited per-window screenshots, including the title
bar, at 1282×752. The browser image is an unedited DevTools page capture at
764×485, with the scaled 1280×720 canvas. They are inspection evidence rather
than golden comparisons. Owned native windows and the browser runner's
disposable profiles were closed after the checks. Persistence, keyboard-layout
labels, physical pads/touchscreens and mobile text input remain outside this
example's qualification.
