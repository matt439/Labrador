# Browser C++ debugging qualification, 2026-11-17

OCA-101's browser route was followed on Windows using Emscripten **6.0.12**,
Chrome for Testing **151.0.7922.34** and the official **C/C++ DevTools Support
(DWARF) 0.2.5854.1** extension. The debugger displayed the original C++ source,
hit its line breakpoint, stepped to the next C++ line and showed named C++
parameters and locals. A wasm instruction listing was not used as acceptance.

## Observed results

| Evidence | Observation |
|---|---|
| [Before the calculation](paused-before.png), [accessibility text](paused-before.txt) | `browser_fault.cpp:7`, paused on a source breakpoint in `browser_score(int)`; `player_count: 2`, `points_per_player: 10`. `total_points` has not yet been initialized, so its displayed storage value is ignored. |
| [After Step over](paused-after.png), [accessibility text](paused-after.txt) | Source advances to `browser_fault.cpp:8`; named local `total_points: 12` reveals the deliberate addition fault. |
| [Corrected calculation](corrected.png), [accessibility text](corrected.txt) | Replacing only `+` with `*`, rebuilding and hard reloading yields the same C++ source step with `total_points: 20`. Resuming displays actual 20 on the page. |
| [Console](console.png), [accessibility text](console.txt) | C++ stdout appears as ordinary output, intentional stderr as a red diagnostic, and the corrected score as 20. The extension reports loading the module's DWARF symbols. |
| [Missing wasm](loader-failure.png), [accessibility text](loader-failure.txt) | With only the test wasm temporarily withheld, hard reload reports its HTTP 404, streaming/fetch failures and abort in Console; the page shows its failure line and never reaches C++ `main`. The wasm was restored afterward. |

All five images were inspected. They contain only the isolated probe and its
DevTools. Text files are matching accessibility excerpts with surrounding and
trailing whitespace removed; the images are unchanged. The source is the [small deliberately faulty fixture](../../../tools/browser_debug/browser_fault.cpp),
with its [page](../../../tools/browser_debug/index.html) and
[reproduction recipe](../../../tools/browser_debug/README.md). The second proof
uses the same source with that one operator corrected; the tracked exercise
retains the fault intentionally.

## Build and tools

The measured compile/link command, from the served folder, was:

```text
em++ browser_fault.cpp -g -O0 -fdebug-compilation-dir=. -sERROR_ON_WASM_CHANGES_AFTER_LINK=1 -sEXIT_RUNTIME=0 -o browser_fault.js
```

The local compiler reports Emscripten revision
`5488e0871d46d19a3209988190fa6a653126256f`, with clang 24.0.0git revision
`55ea1f4ebea23d0355d5cdb075f1e12f28ab82d0`. `llvm-dwarfdump` confirms DWARF 4,
`DW_AT_comp_dir (".")`, the original C++ file and the named parameter/local
records. The source is served beside the wasm over loopback HTTP; no local-file
access permission or path mapping was needed. The Debug flags are used at both
compilation and linking in this single invocation.

The verified module's source bytes match the tracked fixture. The copied
recipe was then run separately at port 8765: HTML, C++, JavaScript and wasm
all returned HTTP 200, with `application/wasm` for the module; its wasm hash
matches the one used in the source-breakpoint proof. The extension preparation
helper was also run: all 11 extracted files match the originally loaded bundle.
[verification.json](verification.json) retains command arguments, source/module,
browser and extension hashes, and the checked UI observations.

The browser was an existing official Chrome for Testing build, also listed in
[Google's version manifest](https://googlechromelabs.github.io/chrome-for-testing/known-good-versions-with-downloads.json).
The extension came from its official Web Store update endpoint; its CRX SHA-256
is `f4c7d9951929acccb602e3cb0e73ece394ecafc1e2094419887d95bf54bd8fce`.
The helper pins both the qualified version and bytes and refuses a changed
download before extraction. The extension is beta software; this record
qualifies the named pair rather than promising every browser or future version.

## Isolation and limits

- The browser used a fresh profile under the ignored test output directory.
  No existing Chrome or Edge profile or system setting was edited. The extension was loaded only for that disposable
  browser instance, and no extension code or browser binary is added to the repo.
- The Windows UI helper initially confused the installed Chrome app identity
  with Chrome for Testing. A separate portable copy, with its executable renamed
  `LabradorDebugBrowser.exe`, made its process-specific window targetable. Its
  SHA-256 equals the original executable byte for byte. That was an automation
  identity workaround, not a change to browser code or the debugging flags.
  The copied browser initially stayed on `about:blank`; navigating explicitly
  to the loopback probe loaded it. No sandbox flag or filesystem permission
  was changed.
- Breakpoint, Step over and Resume were operated in the actual Sources UI.
  Console and loader failures were inspected in the actual browser window.
  The screenshots record the original source and Scope values, not merely
  DWARF sections or JavaScript's WebAssembly locals.
- The probe links no engine and proves this small C++ debugging workflow.
  Complex game values, optimised locals and another browser/tool combination
  remain separate work. Portable rules defects still use native debugging first.
- This exercise changes no default Release flag or bundle input. Debug sources,
  DWARF, test profiles, extensions and this deliberately failing probe are
  developer artifacts, kept out of the normal distributable.
- The isolated browser window was closed, its process ended, and both test
  loopback servers were stopped. The applicable computer-use completion was
  recorded after the window inventory showed no remaining debugger window.

The setup follows [Chrome's C++ debugging guide](https://developer.chrome.com/docs/devtools/wasm),
[Emscripten's debugging guidance](https://emscripten.org/docs/porting/Debugging.html)
and the [official extension's relative-source test recipe](https://raw.githubusercontent.com/ChromeDevTools/devtools-frontend/main/extensions/cxx_debugging/e2e/tests/cpp_eval.yaml).
