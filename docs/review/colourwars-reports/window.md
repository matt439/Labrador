# The Win32 window: reports 11, 16, 17, 19

Four of ColourWars' reports land in one file, `engine/app/win32/window.cpp`,
and three of them share one cause: on Win32 the window is sized by the same
call that shows it. The constructor and both full-screen paths size with
`ShowWindow` (`SW_SHOWMAXIMIZED`, `SW_SHOWNORMAL`), which is why a hidden window
comes up when it goes full screen (16) and why a visible window is on screen
before anything exists to draw into it (19). Report 17 is the frame arithmetic
under that sizing, and report 11 is the one message the window lets through to
`DefWindowProc` that stops the pump. Implement in this order. **16 first**: it
separates size from show with a file-local `monitor_rect` helper, makes
`enter_fullscreen` and `leave_fullscreen` testable headlessly for the first
time, and carries the interim sentence that answers 19. **17 second**, because
it changes the signature of `outer_size_for_client`, which 16's
`leave_fullscreen` path calls. **11 whenever**, since it is one independent
`WM_SYSCOMMAND` case. **19 waits** for the evidence named below, and if that
evidence asks for an engine-side show after the first present, it reuses 16's
helper. Each accepted change is its own self-contained commit with its own
CHANGES.md entry. All of them are Win32-only, none touches a render backend,
none adds a public name, and the web window already answers each question its
own way. Line numbers are as of `0c15f34`. This is a decision record written
ahead of PI-09's filing and PI-10's answers
([docs/product/STORIES.md:1614-1632](../../product/STORIES.md)), not an
implementation; the README's table says what has been built since.

PI-10's acceptance (docs/product/STORIES.md:1622-1624) asks that each report
close as answered with a commit, declined with a cited trade-off, or accepted
and split into its own story. Under that rule the four close as: 11 accepted,
narrowed; 16 accepted; 17 accepted, narrowed; 19 needs more evidence. 11, 16
and 17 are each larger than XS, so each becomes its own story (STORIES.md:1632).
PI-10's trade-offs cite the public-face passage as PHILOSOPHY.md:752-757
(STORIES.md:1629); at `0c15f34` that passage is
docs/design/PHILOSOPHY.md:792-798, which is where it is cited below.

## 11. F10 and Alt alone stop the game

**Decision.** Accept, narrowed. The Win32 window always swallows a bare
keyboard menu activation, `WM_SYSCOMMAND` with `SC_KEYMENU` and an `lParam` of
zero, which is what F10 and Alt pressed alone send. There is no option, and
Alt+Space, Alt+F4 and every key callback stay as they are.

**What the engine has now.** `WM_KEYDOWN`/`WM_SYSKEYDOWN` and the up pair are
forwarded to `on_key_down`/`on_key_up` (engine/app/win32/window.cpp:691-705)
and then break to `DefWindowProc` (engine/app/win32/window.cpp:950). That is
deliberate, and the source says so: "Alt is a key like any other and a game may
bind it, but Alt+F4 and F10 are the system's and stay the system's"
(engine/app/win32/window.cpp:681-685). Nothing in `engine/` handles
`WM_SYSCOMMAND` or `SC_KEYMENU`; `WM_MENUCHAR` only stops the beep
(engine/app/win32/window.cpp:941-944). F10 and Alt are translated and reach the
`Keyboard` (scan 0x44 to `Key::f10` at engine/app/win32/window.cpp:116, 0x38 to
`Key::alt` at :104), and tests/app/window_tests.cpp:323-324 pins `Key::alt`
through `WM_SYSKEYDOWN`. Neither `WindowOptions` (engine/app/window.h:120-143)
nor `ApplicationOptions` (engine/app/application.h:41-104) has a system-menu
option, and by its own comment the window never has a menu bar
(engine/app/win32/window.cpp:202-203). So releasing F10, or Alt on its own,
makes `DefWindowProc` enter a modal menu loop on the system menu. Inside it
`PeekMessage` no longer returns to `Window::pump_until_quit`, `tick()` stops
(the window's `WM_PAINT` ticks only under `in_sizemove`,
engine/app/win32/window.cpp:539-552), and the player's next keys go to a menu
they cannot see. The client's exhibit, driven with `SendMessage` against the
engine's window, recorded that F10 and Alt alone each produce exactly one
`SC_KEYMENU` with `lParam` zero and that a letter produces none; that is the
empirical basis for the predicate below. The engine's own product record
already carries this as owed: docs/product/JOURNEYS.md:172 names "F10 as a key"
as an ordinary need the engine fails, and docs/product/GAP-REGISTER.md:1493
names report 11 and the hook it keeps alive.

**Why.** The rule at engine/app/win32/window.cpp:681-685 is right about Alt+F4
and wrong about F10 and bare Alt. The same comment says "a game may bind it",
and `Key::f10` and `Key::alt` are public names; on a window with no menu bar a
bare `SC_KEYMENU` does nothing a player wants and freezes the loop the header
promises. That freeze is quiet: nothing tells the game it happened, which is
the failure T6 ranks most expensive (docs/design/PHILOSOPHY.md:109-122, "Quiet
failure is the most expensive kind", :113-114). The public-face test is
whether a client would have to write this itself
(docs/design/PHILOSOPHY.md:792-798), and it would, in the worst place: the
client runs a Debug-only message hook over the engine's own pump to eat bare
F10, which is platform message handling in game code. The window procedure
exists so that this is "in the engine and not copied into every project's
main.cpp" (engine/app/win32/window.cpp:507-509), and the seam it sits behind is
ARCHITECTURE's (docs/design/ARCHITECTURE.md:283-296).

The *option* the report asks for is declined under T1's "Not a licence for:
speculative frameworks. Mechanisms are extracted from a real client game in
hand" (docs/design/PHILOSOPHY.md:45-47) and T3 (docs/design/PHILOSOPHY.md:64-81).
Its other value is a game that freezes with no menu to show, nothing in either
repository would set it, and a knob only one side of which anyone wants is
policy dressed as mechanism. The strongest argument against the decline is that
swallowing unconditionally is itself a policy, and that outside precedent ships
an opt-in: GLFW 3.4, from memory and to be checked before it is quoted, drops
`SC_KEYMENU` by default and adds a `GLFW_WIN32_KEYBOARD_MENU` hint to restore
it. It does not win, because the restored behaviour is a stalled pump on a
window with no menu bar. The cut here is also narrower than GLFW's as
recalled: GLFW drops every `SC_KEYMENU`, Alt+Space included, and this keeps
Alt+Space, so the CHANGES entry must not claim parity with it.

What the narrowing keeps is everything that does belong to the system. Alt+F4
is `WM_SYSKEYDOWN VK_F4` to `DefWindowProc` to `SC_CLOSE`, which this does not
touch. Alt+Space sends `SC_KEYMENU` with `lParam` `' '` and still opens the
system menu, which is how a keyboard user reaches Move, Size and Close. Alt
plus a letter has no menu bar to match, and `WM_MENUCHAR` already handles it.
The web window is consistent and does not change: `browser_keeps_key`
(engine/app/web/dom_events.cpp:183-191) gives every function key and every Alt
chord back to the browser, because there they belong to the browser's own
menus, and engine/app/web/dom_events.h:38-46 already calls that the rule the
Win32 window keeps for Alt+F4.

**The change.** Three files and the source list.

- `engine/app/win32/system_command.h` and `system_command.cpp`, beside
  `pointer_input.h`/`.cpp` and for the same reason that pair gives at
  engine/app/win32/pointer_input.h:7-12: AppTests can pin the predicate without
  making Windows enter a menu loop. Being in a platform folder it is not a
  public header, so `website/src/lib/published.ts` is not concerned
  (website/src/lib/reference.ts:48-57 counts only headers directly in a module
  folder). The `.cpp` is listed in engine/CMakeLists.txt beside
  `app/win32/pointer_input.cpp` at :100, because sources are enumerated.

```cpp
// system_command.h
#pragma once

#include <cstdint>

namespace labrador
{
	// Whether a WM_SYSCOMMAND is the keyboard asking for the menu with no
	// key named: what F10, or Alt pressed and released alone, sends. The low
	// four bits of w_param are the system's and are masked off, as the
	// message's contract requires. Alt+Space names ' ' and is not this.
	bool is_bare_menu_activation(uint64_t w_param, intptr_t l_param);
}

// system_command.cpp
bool is_bare_menu_activation(uint64_t w_param, intptr_t l_param)
{
	return (w_param & 0xFFF0) == SC_KEYMENU && l_param == 0;
}
```

- `engine/app/win32/window.cpp`, `window_proc`:

```cpp
case WM_SYSCOMMAND:
	if (is_bare_menu_activation(w_param, l_param))
	{
		return 0;
	}
	break;
```

  The comment at engine/app/win32/window.cpp:681-685 is rewritten as contract
  and rationale, not as history (CONVENTIONS): Alt+F4, Alt+Space and Alt+Tab
  are the system's and fall through; F10 and Alt alone are keys and nothing
  else, because this window has no menu bar and the menu loop would only stall
  the pump. The new code spells its types; the `auto* self` at
  engine/app/win32/window.cpp:512 is the very example
  docs/design/CONVENTIONS.md:62-64 names as wrong, and fixing it in passing is
  the implementer's call.

- `engine/app/window.h`, on `on_key_down` (engine/app/window.h:77): one
  sentence that on Windows F10 and Alt alone are keys and nothing else, and that
  in a browser the function keys and Alt chords keep their browser defaults.
  This header is published word for word on the website
  (website/src/lib/published.ts:233), so the sentence is contract, uses no
  `/* */`, and the website build is run.

No new public name, no option, no change to `WindowNotify`. No render backend
is touched, and nothing is on the frame path: one switch case on a message a
player sends a few times a session (T8, docs/design/PHILOSOPHY.md:137-151, is
untouched). The Win32 window compiles only under MSVC, so clang's `-Werror`
does not see the new files; the window.h comment is compiled by the `wasm-*`
presets too and cannot affect them.

**Tests.** AppTests, `tests/app/window_tests.cpp` (listed at
tests/app/CMakeLists.txt:15), which every Win32 preset runs because the window
does not depend on the render backend: `x64-debug` is the minimum and CI's
Windows jobs cover the rest.

- A pure case for `is_bare_menu_activation`: `SC_KEYMENU` with and without low
  bits set and `lParam` 0 is true; `SC_KEYMENU` with `lParam` `' '` is false;
  `SC_CLOSE` and `SC_MAXIMIZE` with `lParam` 0 are false. This is also what
  covers Alt+Space, which is deliberately not run live, since `DefWindowProc`
  would open a modal menu.
- A live case on `hidden_options()` (tests/app/window_tests.cpp:96-104):
  `SendMessageW(WM_SYSCOMMAND, SC_KEYMENU, 0)` returns 0 and no
  `WM_ENTERMENULOOP` is seen.
- The existing keyboard case (tests/app/window_tests.cpp:291-327) gains
  `WM_SYSKEYDOWN`/`WM_SYSKEYUP` for `VK_F10` (scan 0x44) and for `VK_MENU`, and
  asserts that `Key::f10` and `Key::alt` arrive down then up.

Both live cases hang ctest on a regression, because a regressed window enters
the menu loop inside `SendMessageW` and never returns. Each needs an escape
that runs inside that loop: a `SetTimer` callback on the test thread, which the
menu loop's own dispatch runs, calling `EndMenu()` and recording that it had to.
Whether that reliably ends the loop has to be checked when the test is written;
if it does not, ship the pure case and the window-procedure return value only,
and leave the live cases out rather than ship a test that can hang. No wasm
change: the browser half is already pinned by
tests/app/dom_events_tests.cpp:132-157, which AppTests runs under the `wasm-*`
presets.

**Consumer-visible.** Yes, a silent behaviour change, and the commit also adds
an `engine/**/*.h`, so it needs a CHANGES.md entry under `## Unreleased`.
Before: on Windows, pressing and releasing F10, or Alt on its own, put the
window into the system-menu loop and the game stopped ticking until Escape or
another Alt. After: both are ordinary keys and the game keeps running;
Alt+Space still opens the system menu and Alt+F4 still closes. The entry says
two more things. A keyboard-only user who reached Move and Size with bare Alt
and the arrows now reaches them with Alt+Space. And this is not GLFW's
behaviour, which also drops Alt+Space. A game that relied on the old
behaviour has nothing to restore it, which is the point. No source break.
samples/minimal/README.md's change list gains a line, because the template's
window behaves differently.

**What the client could delete.** The Debug-only message hook over the
engine's pump that eats bare F10, and the hand-kept F10 press counter beside
it; F10 is then read from `Keyboard` like any other key. What stays client
policy is every binding choice, including which function keys it keeps for its
own debug tools, and any Alt+F4 handling of its own.

**Open questions.** (a) Whether `EndMenu()` from a timer callback escapes a
regressed menu loop, which decides whether the live cases ship. (b) What a bare
`SC_KEYMENU` does on the borderless full-screen window, which is `WS_POPUP`
without `WS_SYSMENU` (engine/app/win32/window.cpp:466); swallowing it is
harmless either way, but it has not been observed. (c) The exhibit drove
messages, not OS input, so a physical F10 and a physical bare Alt on a focused
window are still the attended check, on the owner's desktop. (d) Check the GLFW
precedent against its source before the CHANGES entry or a PR description
quotes it.

Answered by 11's commit: (a) is moot, because the live cases cannot fail.
Passed to `DefWindowProc`, `SC_KEYMENU` returned at once without a
`WM_ENTERMENULOOP` for a hidden window and for a shown, never-activated one,
so the commit ships the pure case and the F10 and Alt key edges, and the test
says why there is no live case. (d) GLFW 3.4's `win32_window.c` returns 0 for
every `SC_KEYMENU` unless the window's `keymenu` hint is set, which matches
the recollection above, and the CHANGES entry says this is narrower. (c)
stays the attended check.

## 16. A hidden window comes up when it changes full screen

**Decision.** Accept. A window created with `visible = false` stays hidden and
unactivated through `set_fullscreen(true)` as well as `set_fullscreen(false)`;
the Win32 window sizes a hidden window with `SetWindowPos` instead of
`ShowWindow`.

**What the engine has now.** Two public headers promise that a hidden window
puts nothing on screen and takes no focus: `ApplicationOptions::visible`
(engine/app/application.h:52-56) and `WindowOptions::visible`
(engine/app/window.h:134-139, "a whole shell that puts nothing on screen and
takes no focus"). `Application::set_fullscreen` (engine/app/application.cpp:248-270)
calls `Window::enter_fullscreen` or, unconditionally, `leave_fullscreen`
(:267-268). On Win32, `leave_fullscreen` (engine/app/win32/window.cpp:473-488)
calls `ShowWindow(handle, SW_SHOWNORMAL)` at :485, which shows and activates
the window, and `enter_fullscreen` (engine/app/win32/window.cpp:463-471) has
the same defect, `ShowWindow(handle, SW_SHOWMAXIMIZED)` at :470, which the
report did not name. `Window::Impl` (engine/app/win32/window.cpp:219-291) keeps
no visibility, and `options.visible` is read once, at
engine/app/win32/window.cpp:366-368. No test exercises a full-screen
transition, because every window test is hidden and the only way to exercise
one showed the window: `tests/app/window_tests.cpp` has no full-screen case,
and tests/render/win32/test_window.cpp:10 says "there is no ShowWindow call
anywhere in the tests". The web window is unaffected: `visible` is not read
there (engine/app/window.h:137-138), its enter and leave are the Fullscreen API
(engine/app/web/window.cpp:823-840), and the page owns visibility.

**Why.** This is a broken contract, not a convenience. T2 puts the documented
contract first (docs/design/PHILOSOPHY.md:49-61), and T6 is explicit that loud
is "for broken contracts" (docs/design/PHILOSOPHY.md:121-122): a hidden tool
that turns visible and takes the foreground mid-run is quiet failure seen by
someone other than the person who wrote the call. The fix is mechanism (T1,
docs/design/PHILOSOPHY.md:35-47): the game still decides when to go full
screen, and the engine only stops overriding a decision the game made at
construction. The case for a decline is that no hidden user in this tree
toggles full screen: the collision and local_multiplayer smoke tests
(samples/collision/main.cpp:67, samples/local_multiplayer/main.cpp:69) and
LineSweeperCapture (tools/linesweeper_capture/main.cpp:294) never call
`set_fullscreen`. It loses, because the count does not settle a contract
(docs/design/PHILOSOPHY.md:792-798), and because the client's hidden acceptance
client does drive the real settings path. The widening to `enter_fullscreen`
is part of the accept: fixing only the path the client measured would leave
the same contract broken eight lines above it. And it pays for itself in
tests, since the full-screen paths become headless AppTests cases, which
PHILOSOPHY's *Tests and toolchain* asks of everything below the platform edge
(docs/design/PHILOSOPHY.md:802-807).

**The change.** `engine/app/win32/window.cpp`, plus two header comments.

1. **No cached flag. Read `IsWindowVisible(handle)` at call time.** A
   `bool visible` in `Impl` was the first shape, but it can go stale: a client
   that shows a hidden window natively (the only later-show route there is
   until 19 is decided) would then get the hidden path silently, with no
   maximise and no show. The live query costs the same, holds no state and
   cannot drift (T3).
2. A file-local `monitor_rect(HWND)` in the anonymous namespace:
   `GetMonitorInfoW(MonitorFromWindow(handle, MONITOR_DEFAULTTONEAREST), ...)`,
   returning `rcMonitor`. It is the helper 19's engine-side shape would reuse.
3. `enter_fullscreen`: restyle as now. When shown, unchanged (the
   `SWP_FRAMECHANGED` `SetWindowPos`, then `SW_SHOWMAXIMIZED`). When hidden, one
   `SetWindowPos` to the monitor rect with `SWP_NOZORDER | SWP_NOACTIVATE |
   SWP_FRAMECHANGED` and no `SWP_SHOWWINDOW`.
4. `leave_fullscreen`: skip the `ShowWindow` when hidden, and add
   `SWP_NOACTIVATE` to its `SetWindowPos` when hidden.

```cpp
void Window::enter_fullscreen() const
{
	const HWND handle = this->impl_->handle;
	const bool shown = IsWindowVisible(handle) != FALSE;
	SetWindowLongPtr(handle, GWL_STYLE, WS_POPUP);
	SetWindowLongPtr(handle, GWL_EXSTYLE, WS_EX_TOPMOST);
	if (shown)
	{
		SetWindowPos(handle, HWND_TOP, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
		ShowWindow(handle, SW_SHOWMAXIMIZED);
		return;
	}

	// Sized as a shown window would be, and not shown.
	const RECT monitor = monitor_rect(handle);
	SetWindowPos(handle, HWND_TOP, monitor.left, monitor.top,
		monitor.right - monitor.left, monitor.bottom - monitor.top,
		SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}
```

The `WM_SIZE` the hidden path produces still reaches
`Application::on_window_size_changed` (engine/app/application.cpp:440-464), so
the layout invariant (engine/app/application.h:167-180) holds on both paths,
and the resize reaches all five render backends through
`Renderer::window_size_changed` as every other resize does. The two paths are
not identical in window state: the hidden one reports `SIZE_RESTORED` where the
shown one reports `SIZE_MAXIMIZED`, and `IsZoomed` differs. `WM_SIZE`
(engine/app/win32/window.cpp:575-617) treats both alike, so this is harmless
today, and the header must not claim more than the size.

Docs, both published on the website (website/src/lib/published.ts:231-233):
engine/app/window.h:259-267 gains "a hidden window stays hidden and inactive,
and is given the size a shown one would be"; and the `visible` comments at
engine/app/application.h:52-56 and engine/app/window.h:134-139 gain 19's
interim sentence (below). No signature changes, no T8 cost (these are
options-menu calls), no render backend changes.

Optional, and the owner's: hidden creation with `fullscreen = true` could use
the same helper, so a hidden full-screen window is monitor-sized as a shown one
is. Today it comes out at the requested preset. Do not take it until the
first-size-report case below has run, because whether `ShowWindow(SW_HIDE)` on
a never-shown window delivers the first `WM_SIZE` at all is unknown, and if it
does not, a monitor-rect `SetWindowPos` in the constructor would also be what
supplies the size report engine/app/window.h:172-181 calls load-bearing.

**Tests.** AppTests, `tests/app/window_tests.cpp`, on `hidden_options()` with
`min_window_width`/`min_window_height` lowered so `WM_GETMINMAXINFO`
(engine/app/win32/window.cpp:638-645) does not clamp the 64x64 client. All
Win32 presets; `x64-debug` is the minimum.

- "a hidden window stays hidden and inactive through enter_fullscreen":
  `IsWindowVisible` false, `GetForegroundWindow()` is not the handle, the style
  is `WS_POPUP`, and the last `size WxH` entry in `RecordingNotify::log` is the
  monitor rect's size.
- "leave_fullscreen restores the frame and the client size without showing":
  after the above, still not visible, style `WS_OVERLAPPEDWINDOW`,
  `GetClientRect` equals the size asked for, and so does the last size report.
- "an already-windowed leave_fullscreen is a resize and nothing else": the
  report's own case.
- "a hidden window reports its size before the constructor returns": construct
  on `hidden_options()` and check the log already holds a size entry. This
  settles 19's open question (a) and decides the optional change above. It is
  cheap and need not wait for 19.

The shown path keeps its behaviour and stays untested headlessly, because
showing a window is what the suite avoids; that gap is the same as today and
the test file says so. No wasm change.

**Consumer-visible.** Yes, a silent behaviour change: a CHANGES.md entry under
`## Unreleased`. Before: on Windows, `set_fullscreen(true)` or
`set_fullscreen(false)` on an Application initialised with `visible = false`
showed and activated its window. After: it stays hidden and inactive and is
resized as before. A game that relied on `set_fullscreen` to reveal a hidden
window has to be visible from construction. If the optional creation sizing is
taken, the entry also says a hidden full-screen window is now monitor-sized,
not preset-sized. No source break, no new name. No samples/minimal/README.md
line: the template is visible, and no sample calls `set_fullscreen`.

**What the client could delete.** The guard its hidden capture client wraps
around applying display settings: moving the window off screen, vetoing its
activation, hiding it again and re-arming input afterwards. That client then
calls the ordinary `set_fullscreen`, as the game does. When and whether to go
full screen stays the client's.

**Open questions.** (a) Whether `SetWindowPos` to the monitor rect on a hidden
`WS_POPUP` gives exactly the client size `SW_SHOWMAXIMIZED` gives a shown one;
for a frameless popup it should, and the first test pins the hidden half. (b)
What `monitor_rect` does when `GetMonitorInfoW` fails: T6 argues for a
`std::runtime_error` naming the step over a window left at the old size. (c)
`set_fullscreen(false)` on a shown, user-maximised window still restores it
through `SW_SHOWNORMAL`. That is out of scope here, but it is the same line and
a reviewer may ask. (d) The optional creation sizing is a second silent change,
and the owner may prefer it left out.

## 17. Client pixels in a per-monitor-aware process

**Decision.** Accept, narrowed. The Win32 window keeps its own client-pixel
promise in a per-monitor-aware process: frame arithmetic uses the window's DPI,
and a DPI change keeps the client pixel size the game chose
(`WM_GETDPISCALEDSIZE` and `WM_DPICHANGED`). The awareness default stays with
OCA-66, and no portable DPI query or notification is added.

**What the engine has now.** `outer_size_for_client`
(engine/app/win32/window.cpp:190-214) calls the DPI-unaware
`AdjustWindowRectEx` (:204). It sizes the window at creation (:327-328) and,
through `Impl::outer_size` (:490-496), in `resize_client` (:451-461) and
`leave_fullscreen` (:483). The window procedure (engine/app/win32/window.cpp:510-951)
has no `WM_DPICHANGED` or `WM_GETDPISCALEDSIZE` case, and neither
`WindowNotify` (engine/app/window.h:22-115) nor `Window`
(engine/app/window.h:166-277) exposes DPI. `Application` already adopts
whatever client size `WM_SIZE` reports, in both the layout and the renderer
(engine/app/application.cpp:440-464). The header gives the window this job and
nobody else: "Turning one into the other is this class's job and nobody
else's" (engine/app/window.h:128-129), and `set_resolution` promises the game
its client pixels, "frame included" (engine/app/application.h:182-186). No
engine target, sample or tool declares awareness or embeds a manifest; that is
G151's own finding (docs/product/GAP-REGISTER.md:2755-2766), and OCA-66 owns the
decision and the attended 150% run (docs/product/STORIES.md:1317-1333,
docs/product/BACKLOG.md:23). The web window already follows density: its
buffer is the CSS size times `devicePixelRatio`, measured each frame
(engine/app/web/window.cpp:158-164).

**Why.** Awareness is policy: the executable chooses it, by manifest, and that
is the client's or the template's (T1, docs/design/PHILOSOPHY.md:35-47). The
conversion is not policy. For a process that has opted into per-monitor
awareness, the engine does the job engine/app/window.h:128-129 gives it with
the one function such a process must not use, so the frame is computed at the
wrong scale, the client comes out off the preset, and `WM_SIZE` adopts the
off-preset size quietly (T6, docs/design/PHILOSOPHY.md:109-122). It does not
need two monitors. A per-monitor-v2 window gets its non-client area scaled by
the system, so even on one monitor a live change of the scale setting grows
the frame while the outer rect stays put, and the client shrinks with nothing
said. The client ships a per-monitor-v2 manifest today, so the client T1
requires is in hand (docs/design/PHILOSOPHY.md:45-47), though it claims no
measured pixel error. The fix costs nothing in the configurations the engine
ships: in an unaware or system-aware process `GetDpiForWindow` returns the DPI
`AdjustWindowRectEx` already assumed, and `WM_GETDPISCALEDSIZE` and
`WM_DPICHANGED` are sent only to per-monitor-aware windows. So it neither needs
nor prejudges OCA-66's decision.

The transition policy follows from the engine's resolution model, not from
taste. `ScreenResolution` and `set_resolution` are physical client pixels the
game chose, so a DPI change keeps them. Microsoft's default suggested rect
keeps logical size instead, which would change the game's resolution under it
on every monitor crossing and fight `set_resolution`'s contract; that is
declined under T1, because the game owns its resolution. Under per-monitor v2,
`WM_GETDPISCALEDSIZE` is where the choice is stated: the window answers the
outer size that keeps its client at the new DPI, so the rect `WM_DPICHANGED`
then suggests already agrees and is applied as Microsoft recommends, which is
also what keeps a drag across a monitor edge from oscillating. The portable DPI
query and notification are declined under T1's "Mechanisms are extracted from a
real client game in hand" (docs/design/PHILOSOPHY.md:45-47): the client says it
does not need one, and `GetDpiForWindow` on the HWND is its own Windows-only
business.

**The change.** `engine/app/win32/window.cpp`, plus two comments.

1. `outer_size_for_client(client_size, style, ex_style, dpi)` calls
   `AdjustWindowRectExForDpi(&rect, style, FALSE, ex_style, dpi)` and keeps its
   fall-back to the request on failure.
2. `Impl::outer_size` passes `GetDpiForWindow(handle)`.
3. Creation sizes at `GetDpiForSystem()`, since the window does not exist yet.
   Then, before the `ShowWindow` at engine/app/win32/window.cpp:366, if
   `GetDpiForWindow(handle)` differs, `SetWindowPos(outer_size(client_size))`
   with `SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE`, so a window that lands on
   a monitor of another density is corrected before its first size report.
4. Two new cases in `window_proc`, with **no full-screen special case**:

```cpp
case WM_GETDPISCALEDSIZE:
	if (self && l_param)
	{
		RECT client;
		GetClientRect(window, &client);
		const mattmath::Vector2I outer = outer_size_for_client(
			mattmath::Vector2I(client.right - client.left,
				client.bottom - client.top),
			static_cast<DWORD>(GetWindowLongPtrW(window, GWL_STYLE)),
			static_cast<DWORD>(GetWindowLongPtrW(window, GWL_EXSTYLE)),
			LOWORD(w_param));
		SIZE* const size = reinterpret_cast<SIZE*>(l_param);
		size->cx = outer.x;
		size->cy = outer.y;
		return TRUE;
	}
	break;

case WM_DPICHANGED:
	if (l_param)
	{
		const RECT* const suggested = reinterpret_cast<const RECT*>(l_param);
		SetWindowPos(window, nullptr, suggested->left, suggested->top,
			suggested->right - suggested->left,
			suggested->bottom - suggested->top,
			SWP_NOZORDER | SWP_NOACTIVATE);
	}
	return 0;
```

   An earlier shape returned `FALSE` from `WM_GETDPISCALEDSIZE` when full
   screen, and that is a bug: the system's suggested rect is then the current
   rect scaled by the DPI ratio, the handler above applies it, and a
   monitor-sized `WS_POPUP` becomes one and a half monitors at a 150% change.
   No branch is needed, because `WS_POPUP` has no frame: the answer is the
   current size, and the window stays monitor-sized (T3). The `WM_SIZE` that
   `SetWindowPos` produces reports the client as every other resize does; while
   the user drags, `in_sizemove` holds it until `WM_EXITSIZEMOVE`
   (engine/app/win32/window.cpp:619-636), so one late report is expected.
5. Comments: `outer_size_for_client`'s rationale says why the DPI is the
   window's; the `client_size` comment at engine/app/window.h:128-129 (published,
   website/src/lib/published.ts:233) gains "in the window's own DPI when the
   process is per-monitor aware, and a DPI change keeps it".

**The Windows floor decides how these are imported, and it is the one item
that needs the owner before code.** `AdjustWindowRectExForDpi`,
`GetDpiForWindow` and `GetDpiForSystem` are Windows 10 1607;
`WM_GETDPISCALEDSIZE` and per-monitor v2 are 1703. No document states the
engine's floor (G105, docs/product/GAP-REGISTER.md:1951), and nothing in
`cmake/` or `engine/` defines `_WIN32_WINNT`, so the declarations compile and a
static import would fail at load below 1607 with no build-time signal, which is
T6's failure (docs/design/PHILOSOPHY.md:109-122). The de facto floor is already
Windows 8, because engine/app/win32/window.cpp:866 statically imports
`GetPointerType`. engine/render/d3d11/device_resources.cpp:427's `WINBLUE`
guard is a debug-layer guard, and docs/design/ARCHITECTURE.md:458-460 is about
feature levels (d3d12 needs Windows 10 where d3d11 reaches 10.0), not an
app-layer floor. So the choice is Windows 8 or Windows 10 1607: keep 8 and
resolve the three functions once from `user32` with `GetProcAddress`, falling
back to `AdjustWindowRectEx`, which is exact wherever the per-DPI API is absent
because no process there can be per-monitor aware; or set the floor at Windows
10 and import statically (T3), recorded as G105's answer. Either way, no render
backend changes, and T8 is untouched: these messages arrive on a monitor
change, not per frame.

**Tests.** AppTests, `tests/app/window_tests.cpp`, Win32 presets. Each case
calls `SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)`
before creating its window and restores the previous context after, so the
rest of the suite is untouched. (The test may import that function statically;
it runs only on build machines.)

- "a per-monitor-aware window gets the client size it asked for":
  `GetClientRect` equals the request. On a 100% desktop this matches today; it
  pins the creation path.
- "WM_GETDPISCALEDSIZE answers the outer size that keeps the client": send it
  with `w_param` 144 and a `SIZE`, expect `TRUE` and the extent
  `AdjustWindowRectExForDpi` gives for the current client and style at 144;
  repeat at 120 and 192.
- "WM_DPICHANGED applies the suggested rect": a synthetic rect, then
  `GetWindowRect` equals it and one size report followed.
- "a full-screen window answers its current size": the inverse of the earlier
  shape's test, which would have pinned the bug. With 16 landed, this runs
  hidden.

Synthetic messages do not change the window's real DPI, which is why these
assert on outer geometry and not on client pixels at the new scale. The real
checks are attended hardware work recorded under OCA-66 (screenshot looked at,
date, commit): the cheapest is a single monitor whose scale setting is changed
while a per-monitor-v2 build runs, which needs no second display, and then the
mixed-DPI drag. No wasm change.

**Consumer-visible.** Yes, a silent behaviour change, scoped precisely in its
CHANGES.md entry. A process that declares per-monitor awareness, by manifest or
API, now gets the client size it asked for on any monitor and keeps it when the
window's DPI changes; before, the frame was sized at the system DPI and a DPI
change left the outer rect alone. Unaware and system-aware processes, which
includes every sample and the template as they stand, see no change. No source
break, no new name. If the owner sets a Windows floor, the floor is
consumer-visible on its own account and gets its own entry. No
samples/minimal/README.md line unless OCA-66 has made the template aware by
the time this lands, in which case the line says a copy keeps its resolution
across scale changes.

**What the client could delete.** Nothing today, and the report says so. It
keeps its per-monitor-v2 manifest and its startup awareness probe, which are
its policy. What it gains is that the engine's client-size promise holds on a
scaled or mixed-DPI setup it has opted into, so the workaround it would
otherwise write, re-adjusting the outer rect itself through the HWND, is never
written.

**Open questions.** (a) The floor, above: static import or `GetProcAddress`.
(b) Per-monitor v1: the system does not scale its non-client area and does not
send `WM_GETDPISCALEDSIZE`, so the suggested rect keeps logical size and a
strict client-pixel policy would have to re-adjust inside `WM_DPICHANGED` too.
Decide whether v1 is supported or documented as unsupported. (c) A DPI change
while minimised: `GetClientRect` is then empty, so `WM_GETDPISCALEDSIZE` should
probably return `FALSE` under `IsIconic`, and the restore path checked. (d) A
full-screen window moved to a monitor of another size (Win+Shift+arrow) keeps
the old monitor's size under this change, as it does today; whether full screen
should follow the monitor is a separate question. (e) None of this is a
measured pixel error, since the client's exhibit is a 96 DPI baseline with
arithmetic queries; the attended runs are the real acceptance. (f) This makes
OCA-66's "aware" answer cheaper, because the engine then holds its contract
either way.

Answered by 17's commit: (a) the owner set the floor at Windows 10 1607 with
static imports, recorded as G105's answer and given its own CHANGES entry.
(b) Per-monitor v1 keeps its client pixels and behaves as it did before: its
frame is measured at the system DPI, the DPI it is drawn at, and
`WM_DPICHANGED` goes to `DefWindowProc` rather than applying a rect that keeps
logical size. A test on a v1 thread context pins it.
(c) `WM_GETDPISCALEDSIZE` returns `FALSE` under `IsIconic`; the restore path
is unchecked, with (d) and (e).

## 19. Showing a window later

**Decision.** Needs more evidence. No public `Window::show()` or
`Application::show()` yet: the real question is what a default-visible launch
shows between `ShowWindow` and the first present, and the answer may be an
engine startup change rather than a new call. Meanwhile 16's commit states
that the engine never shows a window created hidden.

**What the engine has now.** `visible = false` already creates a hidden window
with its device and services (engine/app/application.cpp:177,
engine/app/win32/window.cpp:366-368, `SW_HIDE`). The visible branch asks for
`SW_SHOWDEFAULT` or `SW_SHOWMAXIMIZED` (engine/app/win32/window.cpp:363-368).
The window is shown inside the `Window` constructor ("Creates the window ...
and shows it", engine/app/window.h:169), before the device exists:
`Application::initialize` calls `create_window` (engine/app/application.cpp:119,
defined :169-185) and then `create_device` (:133). The constructor's `WM_SIZE`
is load-bearing for full-screen size at launch (engine/app/win32/window.cpp:351-362,
engine/app/window.h:172-181). Until the first present, a visible window is
painted with the class brush `COLOR_WINDOW + 1` (engine/app/win32/window.cpp:306),
because `WM_PAINT` outside a size-move only validates (:539-552). `Application`
and `Window` expose the native handle, documented only "as
Renderer::create_device takes it" (engine/app/application.h:264-266,
engine/app/window.h:206-213), and nothing shows a window later. The web window
has nothing here: the page owns visibility (engine/app/window.h:137-138).

**Why.** The report is candid that it has no failing sequence and no workaround
to delete: a native `ShowWindow` on `Application::window()` works, and the
client's own first-visible-frame question is still open on its side. A decline
on "nobody asks" is not available (docs/design/PHILOSOPHY.md:792-798), and the
public-face question has a real answer: a client wanting a later show would
write a `ShowWindow` on a handle the header documents for another purpose,
which is platform code in a game, the thing `app/win32/` is for
(docs/design/ARCHITECTURE.md:283-296). But accepting `show()` now would cut a
mechanism ahead of the client T1 requires (docs/design/PHILOSOPHY.md:45-47),
and its semantics are exactly what nobody has measured: whether it waits for
the first present, which show command it uses, whether it takes focus, whether
it re-runs full-screen sizing. The answer may not be a call at all. If the
problem is the white class brush between `ShowWindow` and the first present,
the engine-owned fix is to size without showing (16's `monitor_rect`), show
after the first frame, and stop painting the brush. That fixes every visible
game with no new name (T3, docs/design/PHILOSOPHY.md:64-81), where a `show()`
would leave every game to sequence it.

The cheapest closure would be document-only, a stated sequence for native
`ShowWindow` after `initialize`, and the client lists that as acceptable. It is
not taken, because documenting a sequence means choosing the show command, the
focus and the full-screen sizing without the first-frame evidence, which is the
same commitment as `show()` made in prose.

**The change.** Now, in 16's commit, one sentence in the `visible` comments at
engine/app/application.h:52-56 and engine/app/window.h:134-139: "The engine
never shows a window created hidden; a window shown by other means is outside
this contract." That is a statement about the engine, not a promise that the
window is never visible, so it neither blesses nor forbids a native show, and
with 16 reading `IsWindowVisible` rather than a cached flag a natively shown
window simply behaves as a shown one. The stronger wording first proposed,
"false holds for the Application's whole life", is not used: it collides with
the native route the client can use today.

Evidence wanted before a shape is chosen, each recorded with commit and date:

1. A capture of a default-visible launch of `samples/minimal` and of the
   client on the owner's desktop, on `x64-release` at least, recording what is
   on screen from `ShowWindow` to the first present: the class brush or not,
   and for how long.
2. The same with a hidden launch followed by a native `ShowWindow` after the
   first state is constructed: whether that gives a clean first visible frame
   or still shows a stale or white one before the first present.

If (1) shows a flash and (2) shows that hide-then-show cures it, the preferred
shape is engine-side and has no new name: `Window` sizes a full-screen window
with `monitor_rect` instead of `SW_SHOWMAXIMIZED`, `Application` shows the
window after its first `end_frame` when `ApplicationOptions::visible` is true,
and the class brush becomes null or black. Only if a client then needs a later
moment than the first frame does `Window::show()` become the shape: `const`, a
no-op in a browser, and its show command, focus and full-screen sizing stated
in engine/app/window.h, with tests.

**Tests.** None until a shape is chosen; the first-size-report case rides in
16's commit. If the engine-side shape lands: AppTests cannot observe a visible
window headlessly, so the test is on ordering. A hidden Application under the
`-null` presets records that no show happens before the first `end_frame`; the
null backend has no present, so the assertion is on an end-of-frame hook, not
on a present. What the first visible frame shows is an attended capture on the
four rasterising backends, not ctest.

**Consumer-visible.** Not yet; the sentence that rides on 16 is part of 16's
entry. A later engine-side show after the first present would be a silent
behaviour change for every visible game, with its own CHANGES.md entry and a
samples/minimal/README.md line.

**What the client could delete.** Nothing; the report says it has no later-show
workaround in production. If the engine-side shape lands, the client's open
startup question may close with no client code; if `Window::show()` lands
instead, it replaces whatever native `ShowWindow` the client would otherwise
write.

**Open questions.** (a) Whether `ShowWindow(SW_HIDE)` on a never-shown window
delivers the first `WM_SIZE` engine/app/window.h:172-181 relies on. The client's
control saw 1280x720, which equals the preset and so cannot tell; 16's
first-size-report case answers it. Answered by 16's commit: it does not. A
hidden windowed window makes no size report before the constructor returns,
and a hidden full-screen one reports its requested size from creation, not
the monitor's; both have the client size asked for above the system's
smallest window, and engine/app/window.h now states the exception. (b) A show after the first present on d3d12
and Vulkan happens after a present to a window that has never been visible, and
each backend's behaviour presenting to a hidden window, flip-model swap chains
especially, has to be checked before the engine-side shape is chosen. (c) The
white flash is inferred from the class brush and has not been observed; that is
evidence (1).
