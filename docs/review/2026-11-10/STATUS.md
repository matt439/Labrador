# Review of 2026-11-10: status

The live ledger for [the review](README.md). Record each resolution here in the commit that makes it: the commit and its evidence, the story the owner made of it, or the reason it was declined. The row leaves [TODO.md](../../../TODO.md) in the same commit. A finding is a question until its fix proves it.

| id | Finding | State |
|---|---|---|
| `REV#103` | [Issues #122 to #125 are open and unindexed; #124 asks for a non-const on_contact](FINDINGS.md#rev103) | Done. Issues #122 to #125 are indexed in [the reports table](../colourwars-reports/README.md) as open and undecided; `contact.md` drafts #124 with the verdict left to the owner; `device-kind.md` confirms #125 by reading (no such device was run), now `REV#104` in TODO.md; the owner's verdicts are `REV#105`. |
| `REV#14` | [Scene::update's order is published in a guide but is in neither scene.h nor a test](FINDINGS.md#rev14) | Done. `engine/scene/scene.h` states the order as contract: collision objects in insertion order, then plain objects in insertion order, kept across removal and retirement, with an object added mid-update first stepped next tick. Three cases in `tests/scene/scene_tests.cpp` pin it; SceneTests passes on the five native Debug presets and both wasm presets. |
| `REV#15` | [A minimised window runs the frame loop unthrottled](FINDINGS.md#rev15) | Done. A minimised Win32 window runs its owed fixed updates, draws nothing and sleeps in `MsgWaitForMultipleObjectsEx` until a message or the next step (`engine/app/APPLICATION.md` section 8; `WindowNotify::tick_minimized`). AppTests holds the wait arithmetic, the hidden-window loop and the native-null application loop. Observed 2026-11-10 with the minimal sample on D3D11, D3D12, GL and Vulkan (RTX 5080): CPU time fell from 36 to 73 ms a second to 0 to 9 while minimised, each drew again after restore and exited 0, and Vulkan's `validate_sync` wrote nothing. |
| `REV#16` | [Application::load_manifest and run dereference null before initialize()](FINDINGS.md#rev16) | Done. `Application::load_manifest` and `Application::run` throw `std::logic_error` naming the call and `initialize()`; `application.h` states both, `tests/app/application_order_tests.cpp` holds a case for each on the five native Debug presets, and troubleshooting quotes the messages. |
| `REV#17` | [content-tools.mdx says Labrador does not load PNG, and the picture guides still route through texconv](FINDINGS.md#rev17) | Done. `content-tools.mdx` says the engine reads a PNG as it is and that the picture guide can leave `texconv` out, checked against `load_texture_asset`. The other picture guides still teach conversion first; that waits on `REV#62`. |
| `REV#18` | [Stale line counts in README.md and two website pages](FINDINGS.md#rev18) | Done. README.md, `examples.mdx` and `run-minimal.mdx` give `samples/minimal` as about 860 lines (861 over tracked `.cpp` and `.h`), README gives `engine/` about 44k and `tests/` about 42k, and `examples.mdx` LineSweeper about 5,900. No tool checks the figures. |
| `REV#19` | [SECURITY.md is behind the readers it describes](FINDINGS.md#rev19) | Done. SECURITY.md names the PNG reader as a fourth binary reader, states JSON depth as bounded by memory, and its memory bullet states the PNG side cap, `read_file_bytes`' refusal and the GL block-decode expansion without S3TC, each read in the code and tests. |
| `REV#20` | [Loop regions play on Web Audio but are undocumented and untested above the reader](FINDINGS.md#rev20) | Done. A fixture bank with a loop region is read back by AudioTests on every preset, and the Web Audio cases hold `loopStart`, `loopEnd` and the wrapped resume position under Node. `audio.mdx` states what is checked: the XAudio2 path and XWBTool's `smpl` and `wsmp` handling are read from DirectXTK's source, and XWBTool packed one `smpl` loop. The null recording carries no region because the container never crosses the seam. Nobody has listened to a region on either backend (`WEB#3`). |
| `REV#21` | [A pad press never unlocks browser audio, and the audio contract does not say so](FINDINGS.md#rev21) | Done. `audio_device.h`, `audio.mdx`, `troubleshooting.mdx`, AGENTS.md and a dated paragraph under web.md's D13 say a pad press by itself never unlocks Web Audio; an AudioTests case pins the five-event listener list under Node. Not observed with a pad in a browser, and whether a browser counts a pad press as activation is unchecked, which the documents say. No page gained a start gesture. |
| `REV#22` | [Evaluation pages do not state that pads are XInput only](FINDINGS.md#rev22) | Done. `index.mdx` has one new bullet (XInput-compatible pads on Windows, standard-layout pads in a browser) and `why.mdx` says the same with no promise on `GAP#1`, and states the drawing, text-script and in-memory wave-bank limits, each checked against the headers. The home-page list is now eight bullets, one over FES-04's seven; whether it may grow is the owner's. |
| `REV#23` | [Template page stays at "Loading..." when a download fails](FINDINGS.md#rev23) | Done. The six development shells replace "Loading..." with a failure sentence through `Module.onAbort` and page error listeners that swallow nothing; the guides say so. `MinimalLoadFailurePageTests` answers the template's `.wasm` with a 404 in Chromium and passes on both wasm presets. The loader-script, `.data` and `file://` paths are read from Emscripten 6.0.12's source and have no test. |
| `REV#24` | [The second-manifest rule is missing from Application::load_manifest](FINDINGS.md#rev24) | Done. `Application::load_manifest`'s comment and the resources concept page state that a second manifest replaces the first as what a device restore replays; the behaviour is pinned by the existing `tests/assets/resource_loader_tests.cpp` case. |
| `REV#25` | [Linear filtering can sample neighbouring atlas frames and the guide does not say so](FINDINGS.md#rev25) | Done. The filter section of `sprites-and-text.mdx` says when a sheet frame under `TextureFilter::linear` blends the texel across its edge and what to do about it. Read from `build_sprite_quad` and the four backends' samplers; no pixel run was made. |
| `REV#26` | [No standing check builds a renamed starter project](FINDINGS.md#rev26) | Done. `tools/consumer_check/rename_check.py` reads the replacement line, name and count out of `your-name.mdx`, runs it on a copy of the starter and builds and tests the result as the last step of the existing consumer job. Run end to end locally on 2026-11-10: 27 replacements, then x64-debug built and passed SkyHopperTests and SkyHopperSmokeTests. The step has not yet run on a GitHub runner. |
| `REV#27` | [Development and starter page canvases have no accessible name](FINDINGS.md#rev27) | Done. Each development shell gives its canvas an `aria-label` and its status paragraph `role="status"`; the guides quote the markup and say the template's label is a placeholder. The sample page tests pass with it; no screen reader was run. |
| `REV#28` | [Hand-written counts in the agent brief have a machine source and no check](FINDINGS.md#rev28) | Done. `cmake/check_agent_brief.cmake` derives the preset table's ctest counts from `tests/ctest_inventory.json` and fails naming the row, the stated count and the derived count; `check_agent_brief_fixtures.cmake` holds 30 cases. Counts with no inventory source (four native Release presets, `wasm-debug`, the count without a browser) are printed as not checked. |
| `REV#29` | [The engine comparison is unchecked against sources and omits SFML, which the site compares](FINDINGS.md#rev29) | Done. [docs/survey/2026-11-10.md](../../survey/2026-11-10.md) re-checks the seventeen rows for the seven engines against a source line fetched that day, corrects ten "Of 7" counts, and adds SFML 3, Allegro 5, sokol, Cute Framework, Orx and olcPixelGameEngine. Fourteen cells stay marked as resting partly on knowledge, and every "no" is the absence of a match in the files fetched. |
| `REV#30` | [Sprite-sheet frames and strips are not checked against the atlas size](FINDINGS.md#rev30) | Open |
| `REV#31` | [No public texture-size query; a first picture needs hand-typed dimensions](FINDINGS.md#rev31) | Open |
| `REV#32` | [Text alignment is hand-rolled at each call site](FINDINGS.md#rev32) | Open |
| `REV#33` | [Device texture limits are neither queried nor named in the failure](FINDINGS.md#rev33) | Partly done. The Sprites and text guide's "The files" section (website/src/content/docs/docs/guides/sprites-and-text.mdx) now states D3D12's 256-name cap (Renderer::Impl::texture_capacity, engine/render/d3d12/backend.h). It says that each texture, each sprite sheet, each font atlas and the engine's white texture take one name, which leaves 255 for the game. It also states that allocate_texture_slot throws "Texture '<name>' does not fit" at load and that reloading a name reuses its slot. Both were checked against renderer.cpp, texture_factory.cpp, resource_factory.cpp and resource_loader.cpp. Verified with the website's npm run check, test and build, plus check_whitespace, check_doc_citations and check_agent_brief. Left, the owner's call: querying each device's texture size limit (GL_MAX_TEXTURE_SIZE, feature level 10's 8192), naming it in the GL refusal, and a RenderDeviceInfo capability field. |
| `REV#34` | [Colour hex parsing falls back to opaque black silently](FINDINGS.md#rev34) | Open |
| `REV#35` | [Random is compiled into LabradorEngine, out of reach of a math-only rules library](FINDINGS.md#rev35) | Open |
| `REV#36` | [Vector2F arithmetic is out of line and no build enables whole-program optimisation](FINDINGS.md#rev36) | Open |
| `REV#37` | [Nothing holds off the screensaver or display sleep during pad-only play](FINDINGS.md#rev37) | Open |
| `REV#38` | [A mouse click begun and ended between two polls leaves no edge; touch keeps it](FINDINGS.md#rev38) | Partly done. mouse.h now states, as keyboard.h does, that a button pressed and released between two polls has no edge and that touch keeps such a tap. Whether the mouse should latch a click is still the owner's decision. |
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
| `REV#51` | [No lint that every source file is enumerated in a CMake file](FINDINGS.md#rev51) | Open. A check on 2026-11-12 matched every tracked `.cpp` under `engine/`, `tests/`, `samples/` and `bench/` to a CMake file up its folder chain and found none unlisted. |
| `REV#52` | [The configurations CI skips are built only at release](FINDINGS.md#rev52) | Open |
| `REV#53` | [No size budget holds the browser module's payload in CI](FINDINGS.md#rev53) | Open |
| `REV#54` | [GitHub's own traffic and download counters are not recorded at each release](FINDINGS.md#rev54) | Open. The window that includes v0.3.0's release day is [recorded](2026-11-12-github-counters.md), read 2026-11-12; the release-procedure step is the owner's call. |
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
| `REV#69` | [rapidjson is outside the dependency inventory; Dependabot alerts are off](FINDINGS.md#rev69) | Partly done. external/rapidjson/README.md names upstream commit 24b5e7a8 (its include/rapidjson tree matches that commit exactly) and no longer describes ArtAttack.vcxproj or C:\VSIncludes. docs/dependency-updates.md has a rapidjson row and a section on moving the vendored copy. Turning on Dependabot alerts and deciding what to do about the Node 20 action tags is still the owner's call. |
| `REV#70` | [No tag-named rebuild or digest comparison of release assets](FINDINGS.md#rev70) | Partly done. Both v0.3.0 assets were rebuilt from a detached worktree of the tag and matched GitHub's sha256 digests byte for byte. The starter ZIP from starter_zip.py --release v0.3.0 is 9,265,469 bytes with sha256 25db98b2...5869. The docs snapshot from npm ci, snapshot:build and snapshot:pack is 8,853,554 bytes with sha256 c61377d4...6602. The ZIP's bytes depend on the checkout's line endings: 1,059 of its 1,146 entries are text with CRLF line endings. starter_zip.py's docstring and tools/consumer_check/README.md now say so. docs/website-versioning.md records the comparison under "Rebuild comparison: 2026-11-13". Left, the owner's call: a compare-only release rebuild job, and attesting or signing. |
| `REV#71` | [The real thread pool is never run under a race detector](FINDINGS.md#rev71) | Open |
| `REV#72` | [No itch.io publishing guide and no test of a page in a cross-origin iframe](FINDINGS.md#rev72) | Open |
| `REV#73` | [No picture on the repository front page or in link previews, and no announcement](FINDINGS.md#rev73) | Open |
| `REV#74` | [No stated release rhythm, and no per-module stability marking for adopters](FINDINGS.md#rev74) | Open |
| `REV#75` | [No interim desktop frame-time figure is published, and no sprite-count stress sample exists](FINDINGS.md#rev75) | Open |
| `REV#76` | [No public page says how AI agents are used or what is expected of AI-assisted contributions](FINDINGS.md#rev76) | Open |
| `REV#77` | [draw_text and measure_text take const std::wstring& while the glyph walk takes a view](FINDINGS.md#rev77) | Open |
| `REV#78` | [Manifest load is one blocking call with no progress, and nothing unloads](FINDINGS.md#rev78) | Open |
| `REV#79` | [No convex N-gon, capsule or segment collision shape](FINDINGS.md#rev79) | Open |
| `REV#80` | [The one-object-for-thousands dial has no collidable form](FINDINGS.md#rev80) | Partly done. bench/scene_bench.cpp gains "Scene tick (1/8 spawned and retired)". It runs one whole tick (update, resolve, end_tick) on the static case's grid and layer split, retiring the oldest eighth of the colliders through set_for_deletion and spawning as many in their places, so the scene stays a steady rolling queue. bench/main.cpp asserts that the case stays linear. Verified on x64-debug-null: Benchmarks passed, at 712 to 871 ns per object from n=64 to n=4096. Left: whether PHILOSOPHY or scene.h should state that a CollisionObject carries one Shape and that collidable churn means one registered object each. That is the owner's call. |
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
| `REV#98` | [Per-run ccache keys fill the Actions cache past 10 GB](FINDINGS.md#rev98) | Deferred. Re-measured 2026-11-12 with `gh cache list`: 37 caches, 2.84 GB, 27 of them ccache; the oldest dates from 2026-10-26, and no expensive cache was seen evicted. |
| `REV#99` | [No code coverage measurement](FINDINGS.md#rev99) | Open |
| `REV#100` | [Bundle size has no budget; benchmark output is not kept](FINDINGS.md#rev100) | Open |
| `REV#101` | [No mechanical style check for the Python and Node tooling](FINDINGS.md#rev101) | Open |
| `REV#102` | [Unmeasured size options for wasm-release: Closure, emmalloc, -Os/-Oz and LTO](FINDINGS.md#rev102) | Open |
