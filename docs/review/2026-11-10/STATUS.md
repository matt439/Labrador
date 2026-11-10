# Review of 2026-11-10: status

The live ledger for [the review](README.md). Record each resolution here in the commit that makes it: the commit and its evidence, the story the owner made of it, or the reason it was declined. The row leaves [TODO.md](../../../TODO.md) in the same commit. A finding is a question until its fix proves it.

| id | Finding | State |
|---|---|---|
| `REV#103` | [Issues #122 to #125 are open and unindexed; #124 asks for a non-const on_contact](FINDINGS.md#rev103) | Done. Issues #122 to #125 are indexed in [the reports table](../colourwars-reports/README.md) as open and undecided; `contact.md` drafts #124 with the verdict left to the owner; `device-kind.md` confirms #125 by reading (no such device was run), now `REV#104` in TODO.md; the owner's verdicts are `REV#105`. |
| `REV#14` | [Scene::update's order is published in a guide but is in neither scene.h nor a test](FINDINGS.md#rev14) | Done. `engine/scene/scene.h` states the order as contract: collision objects in insertion order, then plain objects in insertion order, kept across removal and retirement, with an object added mid-update first stepped next tick. Three cases in `tests/scene/scene_tests.cpp` pin it; SceneTests passes on the five native Debug presets and both wasm presets. |
| `REV#15` | [A minimised window runs the frame loop unthrottled](FINDINGS.md#rev15) | Done. A minimised Win32 window runs its owed fixed updates, draws nothing and sleeps in `MsgWaitForMultipleObjectsEx` until a message or the next step (`engine/app/APPLICATION.md` section 8; `WindowNotify::tick_minimized`). AppTests holds the wait arithmetic, the hidden-window loop and the native-null application loop. Observed 2026-11-10 with the minimal sample on D3D11, D3D12, GL and Vulkan (RTX 5080): CPU time fell from 36 to 73 ms a second to 0 to 9 while minimised, each drew again after restore and exited 0, and Vulkan's `validate_sync` wrote nothing. |
| `REV#16` | [Application::load_manifest and run dereference null before initialize()](FINDINGS.md#rev16) | Done. `Application::load_manifest` and `Application::run` throw `std::logic_error` naming the call and `initialize()`; `application.h` states both, `tests/app/application_order_tests.cpp` holds a case for each on the five native Debug presets, and troubleshooting quotes the messages. |
| `REV#17` | [content-tools.mdx says Labrador does not load PNG, and the picture guides still route through texconv](FINDINGS.md#rev17) | Open |
| `REV#18` | [Stale line counts in README.md and two website pages](FINDINGS.md#rev18) | Open |
| `REV#19` | [SECURITY.md is behind the readers it describes](FINDINGS.md#rev19) | Open |
| `REV#20` | [Loop regions play on Web Audio but are undocumented and untested above the reader](FINDINGS.md#rev20) | Open |
| `REV#21` | [A pad press never unlocks browser audio, and the audio contract does not say so](FINDINGS.md#rev21) | Open |
| `REV#22` | [Evaluation pages do not state that pads are XInput only](FINDINGS.md#rev22) | Open |
| `REV#23` | [Template page stays at "Loading..." when a download fails](FINDINGS.md#rev23) | Open |
| `REV#24` | [The second-manifest rule is missing from Application::load_manifest](FINDINGS.md#rev24) | Done. `Application::load_manifest`'s comment and the resources concept page state that a second manifest replaces the first as what a device restore replays; the behaviour is pinned by the existing `tests/assets/resource_loader_tests.cpp` case. |
| `REV#25` | [Linear filtering can sample neighbouring atlas frames and the guide does not say so](FINDINGS.md#rev25) | Open |
| `REV#26` | [No standing check builds a renamed starter project](FINDINGS.md#rev26) | Open |
| `REV#27` | [Development and starter page canvases have no accessible name](FINDINGS.md#rev27) | Open |
| `REV#28` | [Hand-written counts in the agent brief have a machine source and no check](FINDINGS.md#rev28) | Open |
| `REV#29` | [The engine comparison is unchecked against sources and omits SFML, which the site compares](FINDINGS.md#rev29) | Open |
| `REV#30` | [Sprite-sheet frames and strips are not checked against the atlas size](FINDINGS.md#rev30) | Open |
| `REV#31` | [No public texture-size query; a first picture needs hand-typed dimensions](FINDINGS.md#rev31) | Open |
| `REV#32` | [Text alignment is hand-rolled at each call site](FINDINGS.md#rev32) | Open |
| `REV#33` | [Device texture limits are neither queried nor named in the failure](FINDINGS.md#rev33) | Open |
| `REV#34` | [Colour hex parsing falls back to opaque black silently](FINDINGS.md#rev34) | Open |
| `REV#35` | [Random is compiled into LabradorEngine, out of reach of a math-only rules library](FINDINGS.md#rev35) | Open |
| `REV#36` | [Vector2F arithmetic is out of line and no build enables whole-program optimisation](FINDINGS.md#rev36) | Open |
| `REV#37` | [Nothing holds off the screensaver or display sleep during pad-only play](FINDINGS.md#rev37) | Open |
| `REV#38` | [A mouse click begun and ended between two polls leaves no edge; touch keeps it](FINDINGS.md#rev38) | Open |
| `REV#39` | [A game cannot see or answer a request to close the window](FINDINGS.md#rev39) | Open |
| `REV#40` | [The pad seam has no headless backend](FINDINGS.md#rev40) | Open |
| `REV#41` | [No engine diagnostic sink for non-fatal messages](FINDINGS.md#rev41) | Open |
| `REV#42` | [Process memory and time to first frame are measured nowhere natively](FINDINGS.md#rev42) | Open |
| `REV#43` | [A copyable debug readout component: frame time plus caller-supplied lines](FINDINGS.md#rev43) | Open |
| `REV#44` | [No sample adds or removes a scene object during play](FINDINGS.md#rev44) | Open |
| `REV#45` | [No engine version the code or the build can read](FINDINGS.md#rev45) | Open |
| `REV#46` | [A named configure diagnostic for a consumer manifest missing an engine dependency](FINDINGS.md#rev46) | Open |
| `REV#47` | [Browser builds from a Linux or macOS host are unverified and untested](FINDINGS.md#rev47) | Open |
| `REV#48` | [An optional starter workflow that builds the browser page and deploys it to GitHub Pages](FINDINGS.md#rev48) | Open |
| `REV#49` | [No sanitizer configuration on the device-free hosts](FINDINGS.md#rev49) | Open |
| `REV#50` | [Nothing tallies CI failures by step](FINDINGS.md#rev50) | Open |
| `REV#51` | [No lint that every source file is enumerated in a CMake file](FINDINGS.md#rev51) | Open |
| `REV#52` | [The configurations CI skips are built only at release](FINDINGS.md#rev52) | Open |
| `REV#53` | [No size budget holds the browser module's payload in CI](FINDINGS.md#rev53) | Open |
| `REV#54` | [GitHub's own traffic and download counters are not recorded at each release](FINDINGS.md#rev54) | Open |
| `REV#55` | [Texture repeat addressing for tiled and scrolling backgrounds](FINDINGS.md#rev55) | Open |
| `REV#56` | [Typed text is UTF-8 while drawn text is std::wstring](FINDINGS.md#rev56) | Open |
| `REV#57` | [A shape with a NaN coordinate stops colliding without any signal](FINDINGS.md#rev57) | Open |
| `REV#58` | [No way to show a key as the player's keyboard layout labels it](FINDINGS.md#rev58) | Open |
| `REV#59` | [Tell the game what kind of pad it has, so prompts can match](FINDINGS.md#rev59) | Open |
| `OCA-76` | [OCA-76 (engine per-user data directory): in-tree copies, IDBFS autoPersist and a JSON writer](FINDINGS.md#oca-76) | Open |
| `REV#60` | [The reduced-motion preference reaches a game only through LineSweeper's own JavaScript](FINDINGS.md#rev60) | Open |
| `REV#61` | [The starter copies two entry files and a hand-written fatal-error report](FINDINGS.md#rev61) | Open |
| `REV#62` | [Development guides use features the release starter does not have, with no note at the passage](FINDINGS.md#rev62) | Open |
| `REV#63` | [No .vsconfig or one-command route for the Visual Studio prerequisite](FINDINGS.md#rev63) | Open |
| `REV#64` | [No copyable on-screen touch controls outside LineSweeper](FINDINGS.md#rev64) | Open |
| `REV#65` | [Consumption routes beyond the submodule and the starter ZIP](FINDINGS.md#rev65) | Open |
| `REV#66` | [Prebuilt Windows sample binaries on each release](FINDINGS.md#rev66) | Open |
| `REV#67` | [Fuzz targets for the content readers](FINDINGS.md#rev67) | Open |
| `REV#68` | [No static analysis beyond the warning flags](FINDINGS.md#rev68) | Open |
| `REV#69` | [rapidjson is outside the dependency inventory; Dependabot alerts are off](FINDINGS.md#rev69) | Open |
| `REV#70` | [No tag-named rebuild or digest comparison of release assets](FINDINGS.md#rev70) | Open |
| `REV#71` | [The real thread pool is never run under a race detector](FINDINGS.md#rev71) | Open |
| `REV#72` | [No itch.io publishing guide and no test of a page in a cross-origin iframe](FINDINGS.md#rev72) | Open |
| `REV#73` | [No picture on the repository front page or in link previews, and no announcement](FINDINGS.md#rev73) | Open |
| `REV#74` | [No stated release rhythm, and no per-module stability marking for adopters](FINDINGS.md#rev74) | Open |
| `REV#75` | [No interim desktop frame-time figure is published, and no sprite-count stress sample exists](FINDINGS.md#rev75) | Open |
| `REV#76` | [No public page says how AI agents are used or what is expected of AI-assisted contributions](FINDINGS.md#rev76) | Open |
| `REV#77` | [draw_text and measure_text take const std::wstring& while the glyph walk takes a view](FINDINGS.md#rev77) | Open |
| `REV#78` | [Manifest load is one blocking call with no progress, and nothing unloads](FINDINGS.md#rev78) | Open |
| `REV#79` | [No convex N-gon, capsule or segment collision shape](FINDINGS.md#rev79) | Open |
| `REV#80` | [The one-object-for-thousands dial has no collidable form](FINDINGS.md#rev80) | Open |
| `REV#81` | [A contact carries no contact point](FINDINGS.md#rev81) | Open |
| `REV#82` | [The ISO extra key has no name; left and right modifiers are one key](FINDINGS.md#rev82) | Open |
| `REV#83` | [Window policy a game cannot state: fixed size, starting position, monitor, maximised](FINDINGS.md#rev83) | Open |
| `REV#84` | [The player's preferred language as a fact the game can read](FINDINGS.md#rev84) | Open |
| `REV#85` | [Wave banks and voices are never released while the AudioDevice lives](FINDINGS.md#rev85) | Open |
| `REV#86` | [No voice ceiling, limiter or master level in the audio path](FINDINGS.md#rev86) | Open |
| `REV#87` | [No playback position, wave duration or start offset on an effect](FINDINGS.md#rev87) | Open |
| `REV#88` | [No one-page quick reference of the game-facing API](FINDINGS.md#rev88) | Open |
| `REV#89` | [No statement on Proton, Steam Deck or Steam Remote Play Together](FINDINGS.md#rev89) | Open |
| `REV#90` | [SDL3 behind the window, input and audio seams as a route to Linux and macOS](FINDINGS.md#rev90) | Open |
| `REV#91` | [A Release game has no symbols procedure and writes no crash dump](FINDINGS.md#rev91) | Open |
| `REV#92` | [No guidance on stepping through C++ in the browser build](FINDINGS.md#rev92) | Open |
| `REV#93` | [Escape in browser full screen is unobserved and undocumented](FINDINGS.md#rev93) | Open |
| `REV#94` | [CONTRIBUTING does not say whether outside pull requests are wanted](FINDINGS.md#rev94) | Open |
| `REV#95` | [The broad phase is not benchmarked on touching static tiles or masked-out pairs](FINDINGS.md#rev95) | Open |
| `REV#96` | [No upper bound on the browser drawing buffer's pixel ratio](FINDINGS.md#rev96) | Open |
| `REV#97` | [Drawing buffer rounded from CSS size times ratio, not read from device-pixel-content-box](FINDINGS.md#rev97) | Open |
| `REV#98` | [Per-run ccache keys fill the Actions cache past 10 GB](FINDINGS.md#rev98) | Open |
| `REV#99` | [No code coverage measurement](FINDINGS.md#rev99) | Open |
| `REV#100` | [Bundle size has no budget; benchmark output is not kept](FINDINGS.md#rev100) | Open |
| `REV#101` | [No mechanical style check for the Python and Node tooling](FINDINGS.md#rev101) | Open |
| `REV#102` | [Unmeasured size options for wasm-release: Closure, emmalloc, -Os/-Oz and LTO](FINDINGS.md#rev102) | Open |
