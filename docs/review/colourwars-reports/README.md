# ColourWars' reports: the engine's decisions

ColourWars, the engine's one real client, keeps a private ledger of API
complaints against Labrador, each backed by the workaround it wrote. On
2026-10-22 fifteen were live and unfiled: 6 and 9 to 22. This folder is the
engine side's answer to each, decided ahead of their filing (`PI-09`) so that
`PI-10` ([STORIES.md](../../product/STORIES.md#pi-10)) has its thinking done
and an implementer can work from it without the ledger.

**Each file was written before anything in it was implemented**, and the
table below says what has been since. A file records a decision, its argument in
PHILOSOPHY's vocabulary, the shape of the change, the tests that ship with it,
the CHANGES.md entry it needs and what it lets the client delete. Each report
was assessed against the source at `0c15f34` and then argued against by a
second reviewer. Every verdict survived, and that review's corrections are
folded into the text rather than kept as a separate debate. Line numbers are
as of `0c15f34` and will drift, so recheck a citation before acting on it.

The client's workarounds are described generically on purpose. The ledger is
private, and its owner approved only minimal engine-focused disclosure. So no
ColourWars path, name or line appears here.

## The files

| File | Reports | What they share |
|---|---|---|
| [window.md](window.md) | 11, 16, 17, 19 | `engine/app/win32/window.cpp`. On Win32 the window is sized by the same call that shows it. |
| [loop.md](loop.md) | 9, 10, 14 | `Application`'s private loop. It stays closed, and only what a client cannot build from public calls is added. |
| [input.md](input.md) | 13, 21 | Both come from the client shipping the first rebinding screen. Names are mechanism, and evaluating bindings is policy. |
| [draw.md](draw.md) | 18, 20, 22 | None changes `GameObject::draw`'s signature or `Scene`'s dispatch. |
| [content.md](content.md) | 6, 12, 15 | Above the audio seam, in `assets/json`, and in the template's CMake. Three independent PRs. |

## The decisions

This list is kept current. Tick a box in the commit that implements the report
or files its answer, and say there which commit or issue answered it.

| | # | Decision | Size | Answer |
|---|---|---|---|---|
| [x] | [16](window.md) | Accept | S | A hidden window stays hidden and inactive through `set_fullscreen(true/false)`: it is sized with `SetWindowPos` instead of `ShowWindow`. **Implement first**, because it is a contract break (`window.h`'s `visible`). Done: *Keep a hidden window hidden when it changes full screen*. Its first-size-report case found that a hidden windowed window makes no report before the constructor returns, and a hidden full-screen one reports its requested size; `window.h` now says so. |
| [x] | [11](window.md) | Accept, narrowed | S | The Win32 window always swallows a bare `SC_KEYMENU` (F10, or Alt alone), with no option. Alt+Space and Alt+F4 are unchanged. A silent behaviour change. Done: *Let F10 and Alt alone be keys, not the system menu*. No live case: a hidden window never enters the menu loop, so it could not fail. |
| [x] | [17](window.md) | Accept, narrowed | M | A per-monitor-aware process gets the window's own DPI in the frame arithmetic, and a DPI change keeps the client pixel size. No portable DPI API. Needs the owner to set a Windows floor first. Done: *Keep the client size in a per-monitor-aware process*, with the owner's floor of Windows 10 1607 and static imports (G105). |
| [ ] | [19](window.md) | Needs more evidence | — | No `show()` yet. First capture what a visible launch shows before the first present. In the meantime, 16's commit states that a window created hidden is never shown by the engine. |
| [ ] | [9](loop.md) | Document only | S | No observer hook on `Application`. A root `State` that owns a nested `StateContext` already sees every frame. Document and test that. Capture and routing are declined as game policy. |
| [x] | [10](loop.md) | Document only | S | No capture hook. Document the out-of-band frame (`begin_frame`, draw, `submit`, `read_back_buffer`, no `end_frame`), and state that a frame submitted and never presented is legal. Done: State that a frame submitted and never presented is legal |
| [ ] | [14](loop.md) | Accept, narrowed | M | `Application::frame_timings()` returns the last completed frame's CPU phase times. GPU time, history and an overlay are declined; `REV#10` stays deferred. |
| [ ] | [13](input.md) | Accept | S | Add `key_name(Key)` and `key_from_name(string_view)`, where each token is the enumerator's spelling. A **source break** through ADL for a same-named unqualified call, which the engine's own `tests/app/web/window_page.cpp` has too. The owner chooses `Key::del`'s token and the name. |
| [ ] | [21](input.md) | Decline | XS | Evaluating bindings is the game's action map, and the clients combine input sources by different rules. Replace the now-false "no client has a rebinding screen" reason wherever it is written. |
| [ ] | [18](draw.md) | Document only | XS | No `Label::set_visible`. Point `label.h` at `UiText::set_hidden`, and warn that a zero-alpha tint still adds its RGB under the premultiplied blend. |
| [ ] | [20](draw.md) | Accept, narrowed | M | Add read-only `DrawList::camera()` and `viewport()`. |
| [ ] | [22](draw.md) | Accept, narrowed | M | Add paired `RenderResources::SheetFrame` and `SheetStrip`, which cannot be mismatched. Raw two-handle overloads, the `UiTexture` forms and `std::less<>` are declined. |
| [ ] | [6](content.md) | Accept, narrowed | M | Named sound groups whose volume `SoundBank` multiplies in above the seam. No master bus, fades, ducking, listener or voice cap. No design document settles "an audio bus against T1", so a T1 decline has no passage to cite. |
| [ ] | [12](content.md) | Split | S | Accept `parse_json(text, source_name)` on the same parse path as `read_json_file`. Key enumeration becomes its own story. Raw source spans are declined. |
| [ ] | [15](content.md) | Document only | XS | No CMake staging helper. The template comment and the assets guide say `copy_directory_if_different` never deletes, and give a list-and-remove recipe. `PI-11` stays separate. |

Two corrections for whoever writes `PI-10`'s answers. First, the story frames
report 6 as a settled position against T1, but no design document settles it.
Second, its trade-offs forbid resting a decline on "only one client asks", so
report 15's decline rests on the template already being the recipe, not on
the caller count. window.md and content.md also note that STORIES.md cites the public-face
passage at PHILOSOPHY.md:752-757, which has since moved to :792-798.
