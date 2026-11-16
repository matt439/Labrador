# Browser Escape qualification attempt, 2026-11-17

**OCA-119 remains BLOCKED. No trusted click or Escape was sent, no full-screen
session was entered, and no game-key routing was observed.** The remaining
acceptance is a qualified trusted-key route or the owner's informal headed
browser observation. The story is not closed by a visible browser window,
an offline fixture or this record.

## Setup and exact stop

The isolated checkout started at `ad711c05`. The host was Windows 11 Pro
10.0.26300 (build 26300). An opt-in headed Chromium runner started Chrome
157.0.8103.78, DevTools 1.3, revision
`4b3d0dabb8250b6f152d2d85d79e7706e309d087`, with a fresh temporary profile,
loopback-only server and no `LABRADOR_TEST_BROWSER_ARGS` overrides. No user
profile or browser/system setting was used or changed.

A disposable qualification page was intended to request full screen on a
trusted click, record browser exit on Escape without calling
`document.exitFullscreen`, then continue to the unchanged components gallery,
whose Escape binding calls `Application::quit()`. Emscripten 6.0.12 compiled
that gallery under `wasm-debug`, with the actual GL/WebGL2 implementation and
two parallel compiler jobs. The run never reached the game page.

Computer Use initialized `@oai/sky` and listed exactly one task-owned browser
window, titled `OCA-119 — qualify browser Escape - Google Chrome`. The next
cell selected that returned window with `sky.get_window` and requested
`sky.get_window_state`. It returned this policy stop before any screenshot
was returned or input was issued:

> Computer Use has been stopped for this turn because it could not determine
> the current browser URL on Windows with enough confidence to enforce policy.
> Stop your work and send a final message noting why Computer Use ended.

No alternative OS-input route was attempted. The browser's main process was
stopped by its verified relationship to this exact runner, after which the
runner executed its normal server/profile cleanup path. The runner, browser and
direct browser children were verified gone. Other browser sessions were not
targeted. The resulting `Runtime.evaluate: the browser went away` and exit 1
are consequences of that cleanup, not a tested Escape failure.

The local receipt is `out/escape119-headed.log` in the retained
`Labrador-agent-browser-escape` worktree; `out/escape119-build.log` records the
gallery build. The three unqualified observer prototype files are retained
only under `out/oca119-unqualified-prototype/`. They are not maintained tests
or a qualified regression: their browser-state checks cannot by themselves
authenticate which input route caused an exit. No successful page-test result
or Escape behaviour is inferred from them.

SHA-256 identifies those retained local files:

| File under `out/` | SHA-256 |
|---|---|
| `escape119-headed.log` | `283ebfa92559141529c82841048649053c9efaec854ac7724a1e3b5229605fdd` |
| `oca119-unqualified-prototype/fullscreen_escape.html` | `a47a20b383ea45457c13db55b6537f85d94c74611648a28c4fc5a4ecf644ed8b` |
| `oca119-unqualified-prototype/fullscreen_escape.mjs` | `7af47a0c0f4e327937935d4e9b56c8d6621c6aca584ac05c50ce8939a3680256` |
| `oca119-unqualified-prototype/fullscreen_escape.test.mjs` | `f807c540cdbe6ad628bdc12692c7b0cce0e553397ef98d73c2a09511ecff3023` |

## Retained tooling and its limit

`tests/render/web/run_page.mjs` now accepts the explicit pair
`--browser-mode headed`; its default remains `headless`. Headed mode omits
`--headless=new` and the Windows headless occlusion workaround. Both modes
retain the runner-owned temporary profile, loopback server, finite deadline
and existing teardown. The driver receives the selected `browser_mode`, and
the terminal logs it. The mode does not change DevTools input into OS input.

For example, adding the option to an existing page check opens its test
window while keeping that check's original input source:

```text
node tests/render/web/run_page.mjs --browser <chrome.exe> --browser-mode headed --page out/build/wasm-debug/samples/components/ComponentsGallery.html --quit-keys ArrowDown,ArrowDown,ArrowLeft,ArrowUp,ArrowRight,KeyR,Escape,Escape --timeout 60
```

This command is a headed **DevTools-key** page check, not the missing
trusted-Escape observation, and was not run after the policy stop. Existing
default headless checks and full-screen flag/explicit-exit tests retain their
own meaning. The new option and its argument/profile fixtures are useful for
attended page inspection; the unqualified observer is deliberately excluded
from CI and the test inventory.

## Remaining acceptance

Before recording game routing, a human or a permitted headed trusted-key
route must first reproduce browser-owned full-screen exit with Escape on a
page that has no Escape handler and makes no explicit exit call. Then use
the same route in a running game with an Escape pause/quit binding and record
actual exit, key delivery and game response, with browser/version and source
identity. A page-level `isTrusted` flag alone does not establish that browser
action, and a DOM event or `document.exitFullscreen` cannot substitute.

No Keyboard Lock, engine binding policy, browser workaround or new required
CI job is selected. The guide records the missing observation rather than a
claim that the first or second Escape has a particular effect.

## Offline handoff validation

`ctest --preset wasm-debug -R "^BrowserProtocolTests$" --no-tests=error
--output-on-failure` passed its 149 Node fixtures, including the new headed
argument/profile checks, without opening a browser. The website gate passed
its type check, 201 tests, development build and link validation. All nine
standalone CMake lint commands in AGENTS.md passed, and the source-enumeration
check found all 380 tracked C++ sources declared. Receipts are
`out/escape119-offline.log`, `out/escape119-website.log` and
`out/escape119-cmake-lint.log` in the retained worktree. No new full Python
discovery or live page check was run for this offline handoff.
