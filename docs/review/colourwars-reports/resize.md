# Window resizing: report 23

**Decision, 2026-10-27.** Accept and implement. Resizing a Windows window
does not show or activate it. This closes the remaining size-only path that
could override `WindowOptions::visible = false`'s no-focus contract.

## Recheck and change

At `1b3120b0424e005a904db090a6695c678bb44d3f`,
[`Window::resize_client`](../../../engine/app/win32/window.cpp) passes
`SWP_NOMOVE | SWP_NOZORDER` to `SetWindowPos`. The hidden fullscreen paths
already include `SWP_NOACTIVATE`. The resize path now includes it too,
unconditionally: a request to change client size is not a request to
activate even a shown window. The browser path is unchanged.

[PHILOSOPHY](../../design/PHILOSOPHY.md), T2, puts the documented contract
first. T1 leaves the game's resolution and activation choices with the game;
the window mechanism must not add activation to a size request. This is a
silent Windows behaviour change, recorded in [CHANGES.md](../../../CHANGES.md),
with the current contract on [`resize_client`](../../../engine/app/window.h)
and a note for template copies in the minimal sample's README.

## Regression evidence

[`AppTests`](../../../tests/app/window_tests.cpp) creates a real hidden
window, in windowed and fullscreen subcases, and requests a 400 by 300 client
area. A scoped native window-procedure observer records the
`WM_WINDOWPOSCHANGING` flags before forwarding every message to the engine.
It leaves the engine's window user data alone and restores the procedure
before the window is destroyed. The assertions require `SWP_NOACTIVATE`,
continued invisibility, no foreground ownership or activation notification,
the requested client pixels and the real size notification.

The new test was run first against the old implementation on 2026-10-27.
Both subcases failed the flag check, acquired foreground ownership and
reported `activated` before `size 400x300`. This reproduces activation in
the engine's own test, without a private client exhibit. It does not claim
that the hidden window became visible. The same test passes after the flag
is added; the flags assertion also catches a regression on systems where
Windows happens to leave the hidden window inactive.

No public name or option is added. A client can remove a platform workaround
whose sole job is to prevent activation during an engine resolution change;
deciding when to change resolution or activate a shown window remains its own.

## Verification, 2026-10-27

- The targeted regression passes all 16 assertions after failing six before
  the fix, across the two subcases.
- Complete builds and CTest runs pass: `x64-debug` 20/20, `x64-release` 20/20
  and `x64-debug-null` 19/19, including the live application resize cases in
  the null configuration.
- The four standalone CMake checks pass. Offline tooling reports 75 Python
  tests run with three skips, and all 36 Node flash/pacing tests pass.
- The website passes `npm ci`, `npm run check`, all 21 tests and its
  146-page build with valid internal links.

These are local results. The full required CI matrix remains the landing gate.
