# SM-48: LineSweeper teaching-comment inventory

Source audit dated 2026-10-28, starting at `7838dda`. Scope is
`samples/linesweeper/states/{play_state,pause_state}.{h,cpp}` and
`samples/linesweeper/presentation/{board_view,top_out_banner}.{h,cpp}`.
This audit changes comments only. Declarations, executable tokens, text shown
to players, sample assets and test registration remain unchanged.

The sample's [README](../samples/linesweeper/README.md) owns its design.
`rules/` has no engine dependency; `presentation/` reads rule values and
queries; `states/` translates devices into rules input and owns menu policy.
PlayState's comment describes review enforcement and points to
[SRL-19](product/BACKLOG.md#srl-19), which still owns the standalone include
check. This audit neither implements nor claims that check.

## Reviewed inventory

| File | Retained or corrected teaching rules | Current source evidence |
|---|---|---|
| `states/play_state.h` | Application and services are borrowed; services and sample content must be ready before `init()`. World and palette stay at stable addresses, with the scene and its objects destroyed before those loans. Resume suppresses held controls until release; covered states must not stack pause menus on deactivation. | Constructor stores the application pointer. `init()` creates a scene borrowing state values. `on_suspend()`, `on_resume()` and `on_deactivated()` maintain coverage and pause policy. `ResumeGate::close_over()` and `pass()` suppress held rules bits. |
| `states/play_state.cpp` | Physical key bindings, slot-zero pad input and touch controls combine into one held-input byte. Pause and restart are state policy. Scene registration orders board, particles, banner, hints and controls. A rules tick runs once per fixed update, while one rendered frame may follow several updates. | `read_input()` ORs the three binding tables; `update()` checks focus/manual pause before `tick()`. `Application::tick()` supplies fixed updates through `StepTimer::Tick()`. `init()` commits pending scene objects before drawing. |
| `states/pause_state.h` | Application and palette must outlive the overlay. The palette changes during update, before const draws. The arrangement outlives its scene's scrim, and the scene outlives FocusGroup's widget loans. The match remains visible and suspended under the overlay. | Member declaration order, the false `covers_screen()` result and `StateContext`'s top-only update / overlay drawing implement these lifetimes and suspension rules. |
| `states/pause_state.cpp` | The first added row receives initial focus; later navigation uses measured bounds. Held arrow priority is up, down, left, right, then slot-zero pad direction. DirectionRepeat rejects directions already held when the menu opens. Touch hit-tests use board space and expanded targets. Game code reads input and invokes FocusGroup; UI objects do not read devices. | `FocusGroup::add()` selects its first button; `move()` calls `nearest_in_direction`. `held_direction()` implements the priority. `start_held()` precedes menu updates. `tapped_row()` transforms through the camera, uses the row gap as height and the larger of the minimum width or measured width plus margins. |
| `presentation/board_view.h` | World and palette are borrowed and read const; texture/font names resolve during construction. Update prepares three cached HUD strings, and draw reads them safely across views. Scene culls the reported panel extent even with one view. | Members store World/palette pointers and resource handles; `update()` handles score, lines and level. `Scene::draw_views()` applies `cull_bounds()` to every drawable. |
| `presentation/board_view.cpp` | Board-space well coordinates are shared with particles. Solid rectangles are tinted white-texel sprites; text uses its atlas. Literal namespace-scope colours avoid initialization dependencies. A top-out hides the stack behind its burst. Shadow edge width is in board units and makes no physical-phone-pixel guarantee. Preview readiness follows queue refill/consume logic. | `layout.h`, constructor resolution, `fill()`, `draw_stack()` and `draw_side_panels()` establish these paths. `rules/tick.cpp` refills before consuming queue entries; the constructor also supports the initial empty queue. |
| `presentation/top_out_banner.h` | Separate registration places the banner above particles. World/resource-table loans must outlive it. The cached text refreshes when either top-out state or chosen touch wording changes. Touch wording is a state choice, not device detection. | `PlayState::init()` registers it after ParticleField. Constructor calls `update(0)`; `update()` compares both cached flags; `set_touch()` records the wording choice. |
| `presentation/top_out_banner.cpp` | A premultiplied black panel attenuates the underlying scene and the text origin is floored in board space. Both cached message and measured size are prepared outside const draw. The quad-only bounds limitation is explicit. | `draw()` uses `faded(black, 0.88f)`, then `draw_text()` with the floored centred x-coordinate. `bounds()` returns the panel's rectangle, not the full text extent; the limitation below remains open. |

Result delivery is checked against `StateContext::apply_pop()`: the pause
state is destroyed, PlayState's `on_resume()` runs, then the result callback
runs. `open_pause_menu()` therefore closes the input gate before either
RESUME or RESTART can return input to the match. The result callback reports
a changed palette before acting on QUIT. Deferral preserves the menu and its
action callback until the enclosing update returns.

The reviewed comments remove repository-split/first-client narratives,
banner-extraction history, stale method/object censuses and the false claim
that single-view scenes do no culling. They retain the mechanisms a reader
needs to copy: borrowed lifetimes, explicit layout, text measurement, scene
registration order, typed results and input gating.

## Separate behaviour gap: banner culling extent

`TopOutBanner::bounds()` currently covers its background quad, x = 500 to
780 in board space. The current keyboard message's measured width is 379,
and the touch message's width is 353, using the checked-in
`jetbrains_mono_bold_16.spritefont` metrics. Their floored centred x-ranges
are 450 to 829 and 463 to 816 respectively. Both extend outside the quad.

This is reproducible from the checked-in DXTKfont data without a device:
glyph count is a little-endian uint32 at byte 8; 32-byte glyph records start
at byte 12. Each record contains the codepoint at offset 0, rectangle
left/top/right/bottom at offsets 4/8/12/16, x/y offsets at 20/24 and x advance
at 28. For the exact two ASCII banner strings, apply
`Font::for_each_glyph()`'s pen walk and `Font::measure()`'s maximum right edge:
add/clamp x offset, include `x + glyph_width` except for blank glyphs no larger
than one texel, then advance by `glyph_width + x_advance`. These metrics are
integral-valued. `RenderResources::measure_text()` delegates to that measure.

`Scene::draw_views()` culls every GameObject using
`cull_bounds(1.0f / camera.scale).intersects(visible)`; it has no exemption for
single-view scenes. A partial view intersecting only the protruding text can
therefore omit the banner because its quad does not intersect that view.
No failure under the sample's normal full-board arrangement or on hardware
is claimed. This audit preserves the code and discloses the limitation in
the header. [SRL-21](product/BACKLOG.md#srl-21) tracks a separate bounded fix to
make culling conservative for all drawn text and pin it with a headless case.

## Website coupling and validation

The website reads PlayState excerpts in Concepts: Game objects, Resources
and Tick; Guides: Input and Menus. It reads PauseState excerpts in Guides:
Input, Menus, and Sprites and text. These excerpts retain their exact source
paths and `from`/`to` delimiter lines. Comments inside an excerpt can change
with this audit; its declarations and executable code do not.
No direct website source excerpt names BoardView or TopOutBanner, and the
generated public reference publishes engine headers rather than these sample
headers.

**Closed 2026-10-28:** all eight files compare equal against `7838dda` after
stripping comments and whitespace; scoped `git diff --check` and the standalone
checks pass. Website check reports zero errors or warnings, all 157 website
tests pass, and the 143-page build completes with valid links and the existing
upstream Vite directive warnings. The source excerpts retain their delimiter
lines and compile into their quoting pages. No new compiler, browser, GPU,
audio, capture, pacing or hardware acceptance follows from this comment
audit. Captures are not regenerated for comments-only work.
