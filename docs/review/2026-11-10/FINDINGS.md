# Review of 2026-11-10: findings

Every kept finding of [the review](README.md), grouped by area and in the order of its `TODO.md` id within each, then the evidence added to rows `TODO.md` already had, then what was not taken. Each describes revision `e1fa9a8d` and is not rewritten as work lands; [STATUS.md](STATUS.md) is the live ledger.

## Engine: render and assets

### REV#30: Sprite-sheet frames and strips are not checked against the atlas size

<a id="rev30"></a>
*worth a story · DECISION · should · S*

`decode_sprite_frames` in `engine/assets/sprite_sheet_loader.cpp` builds each `RectangleI` with no sign, zero-area or extent check, and `decode_animation_strips` checks only `frame_count` and a zero-area first frame. `ResourceLoader::load_sprite_sheet` (`engine/assets/resource_loader.cpp:131-141`) passes only the handle, so the loader has no size to check against. All four rasterising backends clamp, so an out-of-atlas rectangle smears edge texels silently; `website/src/content/docs/docs/guides/your-picture.mdx:519-522` documents that symptom. The null texture keeps its size (`engine/render/null/backend.h:28-40`), so a check is headless-testable.

The claim that this was never recorded is wrong: BACKLOG SM-71's decision table, rank 3 (`docs/product/BACKLOG.md:1502`), keeps positive dimensions and atlas bounds out pending a separate decision. Negative origins and widths are a test-pinned contract (`tests/assets/sprite_sheet_loader_tests.cpp:342`), so a refusal is a behaviour change. It fits T6 and T7 and depends on the texture-size query.

### REV#31: No public texture-size query; a first picture needs hand-typed dimensions

<a id="rev31"></a>
*worth a story · DECISION · should · S*

`engine/render/render_resources.h` offers `resolve_texture`, `resolve_sprite_font`, `resolve_sprite_sheet`, `measure_text`, `wrap_text` and `can_render`, and no size accessor. Every backend already holds the size in `ResolvedTexture` (`engine/render/sprite_geometry.h:205-209`), null included (`engine/render/null/backend.h:28-40`). The guides make the game carry it: `website/src/content/docs/docs/guides/your-picture.mdx:262-293`, `:346-349` and `:417`, with the quiet failure documented at `:519-522`, and `sprites-and-text.mdx:451`. `TextureObject` (`engine/render/texture_object.h:30-44`) requires a sheet and a frame name, so there is no whole-texture drawable.

Two things were bundled. The query is mechanism across five backends with a null test, fits T1, T6 and T10, and is the prerequisite for the atlas-bounds check. A ready-made picture object is a separate policy call under PHILOSOPHY's UI and components rule. Neither is recorded in TODO.md, BACKLOG.md, the survey of 2026-10-27 or the feature candidates.

### REV#32: Text alignment is hand-rolled at each call site

<a id="rev32"></a>
*worth a story · DECISION · should · S*

`engine/render/font.h:133-161` offers measure, wrap and `first_unrenderable`, and no alignment. `engine/ui/widget.h` has `set_position_at_center` only on `UiTexture` (`engine/ui/widget.cpp:215`), not `UiText`. LineSweeper centres by hand three times: `samples/linesweeper/states/pause_state.cpp:63-74` and `samples/linesweeper/presentation/top_out_banner.cpp:83` floor the result, and `samples/linesweeper/presentation/touch_controls.cpp:189-192` does not. `engine/render/renderer.h:212-219` documents the half-pixel trap each site re-derives.

The unfloored site draws at a non-unit scale, where the header states no rule, so it is an inconsistency and not a demonstrated defect. OCA-77 delivered wrapping only. PHILOSOPHY's UI section keeps layout manual, so this stays a pure helper over a measured line, in the engine or as a component; T1's extraction evidence exists.

### REV#33: Device texture limits are neither queried nor named in the failure

<a id="rev33"></a>
*worth a story · DECISION · could · S*

`engine/render/texture_data.h:67-77` fixes `max_texture_dimension` at 16384 on the D3D11 feature level 11_0 guarantee and notes that level 10 stops at 8192, while `engine/render/d3d11/device_resources.cpp:66-71` accepts 10_0 and 10_1 devices. Nothing in `engine/render` queries `GL_MAX_TEXTURE_SIZE`; the GL and WebGL upload's only diagnosis is `engine/render/gl/texture_factory.cpp:238-243`, which names the texture and its size and no limit. `RenderDeviceInfo` (`engine/render/renderer.h:91-107`) has no capability fields. D3D12's 256-name cap (`engine/render/d3d12/backend.h:217-221`) throws a clear sentence (`engine/render/d3d12/renderer.cpp:482-490`) but appears in no website guide.

The D3D12 half is already loud and recorded as EP-1 in `docs/review/backend-equivalence-2/LEDGER.md`; the per-device size limit is unrecorded. No shipped texture is near a limit, and phone limits were not measured. The cheapest slice names the queried limit in the GL refusal and adds one guide line. It fits T6 and SEAM.md's description fields.

### REV#34: Colour hex parsing falls back to opaque black silently

<a id="rev34"></a>
*worth a story · DECISION · could · XS*

`Colour::set_from_hex` (`engine/render/colour.cpp:104-121`) turns anything that is not six or eight hex digits, including `#ff8800`, into `Colour::black` without error. The header states it (`engine/render/colour.h:59-64`) and `tests/render/colour_tests.cpp:72` pins it, while `colour_from_name` returns `std::optional` (`colour.h:233-237`). Eight-digit alpha is stored unpremultiplied with no comment. It is a deliberate contract that sits against T6, not a slip.

Two claims were wrong. It was recorded: finding 36 of the first review (`docs/review/all-findings.md:3117-3126`) and round 2 (`docs/review/round-2/all-findings.md:4099`, `:6038`); `docs/review/round-2/PLAN.md:1037-1041` fixed only the throw and kept the fallback. No open row exists. And the fix is not free of callers: any caller that must not let a typo become black has to validate the hex text itself before constructing a `Colour`. That is the case for an additive `colour_from_hex`; changing the constructor is a break to name in CHANGES.md.

### REV#55: Texture repeat addressing for tiled and scrolling backgrounds

<a id="rev55"></a>
*owner's decision · DECISION · could · M*

`TextureFilter {point, linear}` is the whole sampler vocabulary on the seam (`engine/render/renderer.h:56-67`). Every backend creates clamp-only samplers: `engine/render/d3d11/renderer.cpp:484-486`, `engine/render/d3d12/renderer.cpp:674-676` with `sampler_count = 2` (`engine/render/d3d12/backend.h:260`), `engine/render/gl/renderer.cpp:403-416` and `engine/render/vulkan/renderer.cpp:505-509`. A source rectangle wider than the texture smears the edge, as `website/src/content/docs/docs/guides/your-picture.mdx:521` says. The feature is absent from the survey's rows and its Not-now list (`docs/survey/2026-10-27.md:39-64`), TODO.md, BACKLOG.md and the feature candidates. raylib's `SetTextureWrap` was checked; other engines were not re-fetched.

No sample and no client scrolls or tiles a background. A wrap mode is mechanically compatible with T8 as one more draw-list state, but costs goldens on four rasterisers and works only for whole textures, not atlas frames. Under T1 and the every-genre non-goal it is a `GAP#` row that waits on a client, not work to build.

### REV#56: Typed text is UTF-8 while drawn text is std::wstring

<a id="rev56"></a>
*owner's decision · DECISION · could · L*

The facts hold. `Keyboard::typed()` returns UTF-8 as `std::string_view` (`engine/input/keyboard.h:189-193`); `Text`, `TextObject`, `Label`, `UiText` and `draw_text` take `std::wstring` (`engine/render/text.h:27`, `engine/render/text_object.h:35`, `engine/render/label.h:42`, `engine/ui/widget.h:140-154`, `engine/render/renderer.h:225`); the bridge is `engine/render/text_encoding.h`, which notes that `wchar_t` is 16 or 32 bits by platform.

This is neither unrecorded nor undecided. `engine/render/SEAM.md:304-309` and `renderer.h:209-210` decide a wide entry point with no narrow overload. `IDEA#1` (`docs/feature-candidates.md:58-74`) already scopes a field that edits UTF-8 and widens to draw, one conversion per edit and not a measured cost. `PORT#2` expects `char16_t` on the render API for Android. Unifying the type breaks every text-taking public header and has no caller until one of those rows is taken, so it is a pre-1.0 question for the owner to settle with them.

### REV#77: draw_text and measure_text take const std::wstring& while the glyph walk takes a view

<a id="rev77"></a>
*low priority · DECISION · could · S*

The inconsistency is real. `DrawList::draw_text` (`engine/render/renderer.h:225-232`), `RenderResources::measure_text` (`engine/render/render_resources.h:253-254`) and `record_text` (`engine/render/sprite_geometry.h:236-246`) take `const std::wstring&`, while `wrap_text`, `can_render` and `first_unrenderable` (`render_resources.h:260`, `:289-291`) and `Font::measure`, `wrap` and `for_each_glyph` (`engine/render/font.h:135`, `:149`, `:179`) take `std::wstring_view`. Five backend definitions would change.

The cost was overstated. `samples/linesweeper/presentation/board_view.cpp:72-81` rebuilds its strings only when a value changes, and no allocation has been measured. `TextObject::set_text` (`engine/render/text_object.h:94`) is virtual and stores a `std::wstring`, so changing it gains nothing and breaks overriders. The change fits T11 and keeps SEAM.md's wide entry point, needs a CHANGES.md entry, and hurts no present caller if left.

### REV#78: Manifest load is one blocking call with no progress, and nothing unloads

<a id="rev78"></a>
*low priority · DEFERRED · could · M*

Verified: `engine/assets/resource_loader.h:93-102` loads every asset a manifest names in one call, and a second manifest replaces the first as the thing a restore replays. Neither `engine/render/render_resources.h`, `engine/audio/audio_resources.h` nor `resource_loader.h` has an unload or remove verb; `release_device_resources` is device-loss plumbing, and re-adding a name replaces it (`engine/render/render_resources.h:57`). No record mentions loading screens, progress or unloading.

Corrections: that no messages are pumped during the load was not traced. It follows from the samples calling `load_manifest` before `run()`, which is their pattern and not an engine constraint; a state could call it later, still blocking. The unload half fights PHILOSOPHY's "Names resolve to handles once, at load" (`docs/design/PHILOSOPHY.md:661`) and the restore replay keyed on names (`engine/assets/resource_loader.h:104-110`). It needs a client first (T1). The progress half fits T7 and T8. Comparables were not re-fetched.

## Engine: core, collision and scene

### REV#103: Issues #122 to #125 are open and unindexed; #124 asks for a non-const on_contact

<a id="rev103"></a>
*fix now · OPEN · should · S*

`engine/collision/collision_object.h:49-50` declares `on_contact(const CollisionObject& other, ...)`. `engine/collision/contacts.cpp:111-114` calls it from `Contact::a` and `Contact::b`, which `engine/collision/contacts.h:21-22` declares as non-const pointers, so the constness is introduced only at the virtual's signature. Issue #124 reports that this forces a cast in the client; its own text offers a decline on T8 or on the told-exactly-once rule, and concedes that a side channel settled after `resolve()` would work, which `Scene::contacts()` provides. Issues #122 to #125 are all open, created 2026-11-05, and no Markdown file in the tree names them. The statement that PI-10 is closed is at `docs/product/BACKLOG.md:515`, not in TODO.md. #125 is a bug report (a software driver reported as `RenderDeviceKind::hardware`); its claim was not verified here. Indexing is bookkeeping AGENTS.md requires of the reports table. Changing the signature is a source break and the engine's call; T1's client-in-hand test is met, and T2 and T5 argue for it.

### REV#14: Scene::update's order is published in a guide but is in neither scene.h nor a test

<a id="rev14"></a>
*fix now · OPEN · should · XS*

`engine/scene/scene.cpp:122-134` updates the collision objects first and the plain objects second. `engine/scene/scene.h:212` says only that phase 1 steps every object in the world once. `tests/scene/scene_tests.cpp:181-201` checks that each kind is stepped once, not the order between kinds, and no test in `tests/scene` asserts update order; the draw order, by contrast, has a contract test at `tests/scene/order_tests.cpp:117`. The original claim that no guide mentions the order is wrong: `website/src/content/docs/docs/concepts/tick.mdx:65` states it. The order is therefore a published statement with no header sentence and no test behind it. Insertion order within each kind is also unstated in the header. Not recorded elsewhere. This fits T3 and the rule that the header is the caller's contract; `scene.h` comments are published word for word, so the added sentence states contract, never history.

### REV#35: Random is compiled into LabradorEngine, out of reach of a math-only rules library

<a id="rev35"></a>
*worth a story · DECISION · should · S*

`engine/CMakeLists.txt:28` compiles `core/random.cpp` into LabradorEngine. `engine/core/random.h` offers `next`, `uniform_int`, `unit`, `uniform_float`, `chance` and `position`: no shuffle, pick or derived stream. `samples/minimal/CMakeLists.txt:29` links MinimalRules to LabradorMath alone, so the template's rules cannot use it. `samples/linesweeper/rules/tick.cpp:33-50` repeats the same SplitMix32 and `:75-104` hand-rolls a Fisher-Yates bag; `engine/core/random.cpp:14-16` admits the duplication. The original claim overstated two things. The client links LabradorEngine and so can already reach Random, and no report asks for a move, so the client is no evidence for placement. A move would not help LineSweeper's rules, which link nothing and are refused engine includes by `cmake/check_linesweeper_layers.cmake`. A move also changes `docs/design/ARCHITECTURE.md:153` and is a consumer-visible include-path change. Not recorded; `docs/survey/2026-10-27.md:34` lists seeded RNG as done. Placement serves T9; shuffle has one in-tree copy; a weighted pick has none and needs a client first.

### REV#36: Vector2F arithmetic is out of line and no build enables whole-program optimisation

<a id="rev36"></a>
*worth a story · DECISION · should · S*

`engine/math/vector2f.h` declares every operator and accessor; only the two float constructors at lines 24-25 are constexpr and inline. `engine/math/vector2f.cpp` defines `+=`, `-=`, `*=`, `/=`, `length` and the rest out of line. No CMakeLists.txt, `cmake/` script or `CMakePresets.json` mentions INTERPROCEDURAL, LTCG, `/GL` or `-flto`, so neither MSVC nor emcc can inline them across translation units. LineSweeper's particle step uses them per particle: the integration at `samples/linesweeper/presentation/particles.cpp:178` and the bounds lead at `:193`. The cost is an inference; nothing was built or measured, and the frame bench's update phase may show it is negligible beside draw submission. No document records a decision either way. T8 favours the direct path, but `docs/design/PHILOSOPHY.md:708` requires a profile first, so the story is a measurement and then a decision. Enabling IPO is a `cmake/settings.cmake` change, consumer-visible, with a CHANGES.md entry and a link-time cost; inlining touches headers the website reference parses.

### REV#57: A shape with a NaN coordinate stops colliding without any signal

<a id="rev57"></a>
*owner's decision · DECISION · could · S*

`engine/collision/broad_phase.cpp:159-165` skips any object whose bounding box has a NaN field, and `engine/collision/narrow_phase.h:71-74` and `:83-84` reject non-finite coordinates. `Scene::in_bounds` (`engine/scene/scene.cpp:113-120`) reaches `test_AABB_AABB`, written in accepting form at `engine/math/ericson_math.cpp:26-39`, so a NaN answers false. At the end of the tick (`engine/scene/scene.cpp`, about lines 156-166), only when bounds are set, such an object gets `set_for_deletion(true)`, a no-op by default (`engine/collision/collision_object.h:70`). An object that records the flag vanishes; a default one stays, uncollidable and silent. No test in `tests/scene` mentions NaN. Each step is deliberate and documented (`engine/collision/broad_phase.h:44-45`), so this is not code contradicting its contract. What is missing is a signal. T6 argues for one; the total predicates argue the behaviour is intended. A count beside `large_object_count()` or a Debug throw is the owner's call, and a throw needs a CHANGES.md entry. One finite check per object does not trouble T8. Not recorded elsewhere.

### REV#79: No convex N-gon, capsule or segment collision shape

<a id="rev79"></a>
*low priority · DECISION · could · M*

`engine/math/shape_type.h` enumerates rectangle, circle, triangle, quad, rotated rectangle and none, and `engine/collision/narrow_phase.h` names those as the supported set. No capsule or convex polygon appears in TODO.md, BACKLOG.md, `docs/feature-candidates.md`, the survey or the recommendations of 2026-11-04. `engine/math/ericson_math.h:212` does hold a span-based convex polygon test. The claim that an N-gon is mostly a new shape type and a row in the pairing table is unverified: a variable-size shape meets PHILOSOPHY's fixed-size-geometry rule and the per-frame no-allocation rule at `docs/design/PHILOSOPHY.md:702`. No client or sample asks for any of the three. Under T1 and the every-genre non-goal at `docs/design/PHILOSOPHY.md:889`, this waits for a client that needs it.

### REV#80: The one-object-for-thousands dial has no collidable form

<a id="rev80"></a>
*low priority · DECISION · could · S*

`docs/design/PHILOSOPHY.md:471-476` makes both ends of the spectrum first-class, one registered object standing for thousands of values, and names `CollisionObject` among the interfaces at `:468`. `CollisionObject::shape()` returns one shape per object, `Scene::add` takes a `unique_ptr`, and `engine/scene/scene.h:166-167` says removal is not for thousands a tick because those are values inside one object. That arrangement has no collidable form. `bench/scene_bench.cpp` uses a static grid, and its comment at `:27-33` puts a real match at about fifty objects; no churn case exists. PHILOSOPHY's own example is a `GameObject`, so the text does not strictly promise batched collision: this is an unstated limit, not a false statement. Not recorded elsewhere. Stating the limit fits T3 and an honest public face. A batch interface has no caller and needs a client first (T1), and a churn bench follows the representative-scenes rule.

### REV#81: A contact carries no contact point

<a id="rev81"></a>
*low priority · DEFERRED · could · S*

`engine/collision/manifold.h` holds only a normal and a penetration; `Contact` in `engine/collision/contacts.h` holds the two objects, the normal and the penetration; `on_contact` receives the other object, the normal and the penetration. No contact-point wording appears in TODO.md, `docs/feature-candidates.md`, the survey, the recommendations, `engine/collision` or the client reports. The claim that a representative point is a free by-product of the separating-axis pass is unverified; circles and containment cases each need their own exact definition under T3. No client has filed for it, and issues #122 to #125 do not ask for it. It needs a client first (T1). If #124 forces a break to `on_contact`'s signature, that is the moment to ask whether a point rides it, and not before.

### REV#95: The broad phase is not benchmarked on touching static tiles or masked-out pairs

<a id="rev95"></a>
*low priority · DEFERRED · could · S*

`engine/collision/contacts.cpp:15-37` runs `layers_collide` inside `measure_pair`, after the broad phase has emitted the pair. `engine/collision/broad_phase.cpp`'s `append_cell_pairs` emits a pair whenever two boxes intersect under the closed test (`engine/math/ericson_math.cpp:35-39`: boxes that share only an edge intersect), so edge-sharing and corner-sharing tiles all form pairs every tick, and the grid is rebuilt on each `find_pairs`. `bench/scene_bench.cpp:36-41` uses 48-unit boxes on 64-unit cells, mostly not touching, and `engine/collision/broad_phase.h:28-30` says that is the world the grid was built for. The pair counts and costs in the original claim are estimates, not measurements; a masked-out pair costs two virtual calls before rejection. The named caller does not exist: `GAP#11` is recommended for decline at `docs/2026-11-04-decision-recommendations.md:408-411` because no client uses tiles. Not recorded elsewhere. A bench case fits the representative-scenes rule, but no present client has such a world (T1).

## Engine: input, window and application

### REV#15: A minimised window runs the frame loop unthrottled

<a id="rev15"></a>
*fix now · OPEN · should · S*

Traced. `Window::pump_until_quit` (`engine/app/win32/window.cpp:559-593`) calls `tick()` whenever `PeekMessage` finds nothing, with no wait. `Application::tick` (`engine/app/application.cpp:399-421`) returns early only for a null window handle, and `Application::render` (450-477) gates only on the first update. `on_suspending` (543-551) sets `shown_` false, which reaches pads, focus, states and audio but never the loop. The repository's own measurement is in `docs/review/INDEX.md:93-104`: on Vulkan, 8,429 minimised frames in about 3.3 seconds, in an entry closed on the throw alone.

Only Vulkan was measured; the D3D11, D3D12 and GL spin is inferred. `docs/review/backend-equivalence-2/INVENTORY.md:75` notes the D3D pair survive on `DXGI_STATUS_OCCLUDED` being a success code, and no engine source names it. Skipping render alone leaves a busy loop of fixed updates. Hidden smoke tests and capture tools run with `visible = false` and must keep rendering.

No open row owns it; `IDEA#14` covers only an unfocused cap, and its decline recommendation (`docs/2026-11-04-decision-recommendations.md:213-228`) rests on vsync capping a native window, which is false while minimised. It is the shell's loop and needs no public API, on one condition: `engine/core/state.h:80-85` publishes that a state not told of the background keeps being updated, so the wait must keep fixed updates at their rate. Stopping updates while minimised is a behaviour change and the owner's call.

### REV#16: Application::load_manifest and run dereference null before initialize()

<a id="rev16"></a>
*fix now · OPEN · should · XS*

In `engine/app/application.cpp:239-249`, `load_manifest` calls `resource_loader_->load_manifest(...)` with no check, and at `:251-257` `run` calls `window_->pump_until_quit()` after entering the first state, whose `init()` has already seen null services. Both members are null until `initialize()`, as `engine/app/application.h` says. The neighbouring calls are guarded: `quit()` checks `window_` (`:261-267`), and the resolution, fullscreen and cursor calls record the request (`:272-279`; `tests/app/application_resize_tests.cpp:219`). These two have neither a guard nor a sentence in the header, and no test covers the misuse.

A reader who deletes or reorders `app.initialize()` in the copied `main.cpp` gets an access violation with no message, which `troubleshooting.mdx:672-685` can describe only generically. Nothing records this. T6 asks for a throw naming the step, as `initialize()` already does. A header sentence needs a CHANGES.md entry or `not consumer-visible` under the changes rule.

### REV#24: The second-manifest rule is missing from Application::load_manifest

<a id="rev24"></a>
*low priority · OPEN · could · XS*

That a second `load_manifest` replaces the restore replay list is a documented, test-pinned contract: `engine/assets/resource_loader.h:93-96` states it and `tests/assets/resource_loader_tests.cpp:169-190` holds it. Round 2 found it (`docs/review/round-2/all-findings.md:17004`) and regraded it to medium (`:17685`) because it is documented and needs a device loss. Every sample calls `Application::load_manifest` exactly once. `engine/render/render_resources.h:97-109` declines live unload by name.

The new evidence is weak: WebGL context restore makes the loss routine in the browser, but no sample has two manifests. One true gap remains: the rule is written only on `ResourceLoader::load_manifest`; the public comment games actually read (`engine/app/application.h:187-196`) and the website guides do not carry it. Repeating the sentence there needs no decision. Appending needs a client first (T1); throwing on the second call fits T6 but reverses the recorded contract and is the owner's call.

### REV#37: Nothing holds off the screensaver or display sleep during pad-only play

<a id="rev37"></a>
*worth a story · DECISION · should · S*

The absence is verified. A search of `engine/`, `samples/`, `docs/`, `website/src`, TODO.md and CHANGES.md for `SetThreadExecutionState`, `SC_SCREENSAVE`, `SC_MONITORPOWER`, `wakeLock`, `ES_DISPLAY`, screensaver and wake lock finds nothing. `engine/app/win32/window.cpp:1262-1268` handles `WM_SYSCOMMAND` only for bare menu activation, and `engine/app/web/window.cpp:412` registers `visibilitychange` without requesting a wake lock. `Application::settle_presence` (`engine/app/application.cpp:509-529`) already computes foreground and shown, the natural gate.

The symptom is not verified. Nobody has run a pad-only session against a short display timeout, natively or in a browser, and whether Windows counts XInput reads as user activity is inferred, not measured. The supportable claim is display dim, display off or screensaver, not system sleep. In a browser the lock drops when the tab hides, must be requested again and can be refused.

No record exists. It fits as a platform mechanism behind the app seam with pad-first callers in `samples/local_multiplayer` and LineSweeper; T1 is met only if the symptom reproduces. An `ApplicationOptions` switch needs a CHANGES.md entry.

### REV#38: A mouse click begun and ended between two polls leaves no edge; touch keeps it

<a id="rev38"></a>
*worth a story · DECISION · should · S*

The code is as described. `Mouse::on_button_down` and `on_button_up` (`engine/input/mouse.cpp:80-97`) set and clear a live bit that `poll` samples once per fixed update, so a down and up delivered between two polls leave nothing. `Keyboard` behaves the same (`engine/input/keyboard.cpp:70-93`, 135-145) and says so: `engine/input/keyboard.h:152` states the limit, and `website/src/content/docs/docs/guides/testing.mdx:465-470` tells readers to hold a key about 100 ms. Touch chose the opposite on purpose (`engine/input/touch.h:140-144`). `mouse.h` states no limit and has no latch.

The claimed frequency is not supported. A physical click spans several 60 Hz polls; loss needs a hitch longer than the click, or a synthetic down and up posted together. The touchpad and 30 Hz claims are unmeasured.

The key half is an accepted limit, also in `docs/product/STORIES.md:1744-1748`; changing it reverses a recorded decision and is the owner's call. The mouse half has no record. It fits T2 given the touch precedent and the pointer menus of OCA-50 and OCA-83, and is a silent behaviour change needing a CHANGES.md entry.

### REV#39: A game cannot see or answer a request to close the window

<a id="rev39"></a>
*worth a story · DECISION · should · S*

Verified on Win32. No engine source names `WM_CLOSE`; the dispatch list in `engine/app/win32/window.cpp:1313-1360` has `WM_DESTROY` and `WM_NCDESTROY` only, so the X button, Alt+F4 and taskbar close reach `DefWindowProc`, and `WM_DESTROY` posts the quit (1172-1178). `engine/app/window.h:88` documents that Alt+F4 still closes the window. `WindowNotify` has only the browser-only, after-the-fact `on_closed` (`engine/app/window.h:54-62`), and `engine/core/state.h` has no close hook. The web window registers no `pagehide` or `beforeunload` (`engine/app/web/window.cpp:396-412`).

The claim that a game cannot flush a save is overstated. `~Application` clears the state stack first so states are destroyed against live services (`engine/app/application.cpp:93-103`), and in a browser states already receive `on_deactivated` when the tab hides. What is missing is the ability to ask or decline, and an explicit notification. No sample has a save to lose.

No record exists beyond the settings-and-saves guide's `beforeunload` caution. It fits T1: today the engine decides the policy. Callers are the template's ConfirmState and `IDEA#3` if taken. A public virtual needs a CHANGES.md entry and a line in `samples/minimal/README.md`.

### REV#40: The pad seam has no headless backend

<a id="rev40"></a>
*worth a story · DECISION · should · M*

`LABRADOR_INPUT_BACKEND` offers only `xinput` and `web` (`engine/CMakeLists.txt:369-396`), so every native preset, the two `-null` presets included, builds the XInput reader and polls the real machine (`engine/app/application.cpp:168-169`, `:440`). Render and audio each have a recording `null/` backend, and `docs/design/PHILOSOPHY.md:852-857` says a seam ships with its headless implementation or it has not shipped. Keyboard, Mouse and Touch have public feeds; `engine/input/gamepad_reader.h` has no injection point. `tests/input/vibration_tests.cpp:304-316` uses the machine's own reader and returns early when every slot is occupied, and `:354-361` says its suspended case cannot catch a regression of the native flag.

This is not a claim that pad logic is untested: the pure functions over GamepadState are tested headlessly and the web reader runs against a fake under Node. The gap is the native reader and anything reading pads through Gamepads or Application. Nothing records it; `REV#9`, `OCA-45` and `PORT#2` are adjacent only. It fits T5 and T8 as a build-selected backend, and is the substrate `REV#9` would need.

### REV#58: No way to show a key as the player's keyboard layout labels it

<a id="rev58"></a>
*owner's decision · DECISION · could · M*

Verified. `Key` is a position (`engine/input/keyboard.h:15-18`), and `key_name` is a name, not a label: what a player is shown is the game's (`engine/input/keyboard.h:90-98`). No engine source uses `MapVirtualKey`, `ToUnicode`, `GetKeyNameText`, `getLayoutMap` or `INPUTLANGCHANGE`. The published guide says that on a French keyboard `Key::a` is the key marked Q (`website/src/content/docs/docs/guides/input.mdx:66`, 233), yet the samples print fixed letters (`samples/minimal/states/hello_state.cpp:91`, `samples/local_multiplayer/join_state.cpp:195-197`), which are wrong on AZERTY.

The proposed size of S is too small; M is more honest. `navigator.keyboard.getLayoutMap` is Chromium-only and asynchronous, so Firefox and Safari need a stated fallback. Win32 needs dead-key-safe translation and a refresh on layout change, and a label needs glyphs in the game's font atlas, which the engine cannot promise. The comparison with other libraries was not re-fetched.

No row records it; it sits beside `IDEA#2` and `REV#12`'s physical-layout check. It fits T1 as a fact query with no map, but its only real caller is `IDEA#2`, itself undecided, so it rides that story.

### REV#59: Tell the game what kind of pad it has, so prompts can match

<a id="rev59"></a>
*owner's decision · DECISION · could · M*

Partly verified. `GamepadState` (`engine/input/gamepad.h:48-68`) carries connection, sticks, triggers and buttons, and no identity. The web reader accepts any pad the browser reports with the standard mapping (`engine/input/web/gamepad_reader.cpp:161-168`), so a DualSense or Switch pad is playable in a browser build, while the samples print Xbox letters as fixed text (`samples/minimal/states/confirm_state.cpp:33`, `samples/local_multiplayer/join_state.cpp:195-197`).

The proposal understates the work. Deriving a label style from `Gamepad.id` is a heuristic over an unstandardised string whose format differs between Chromium, Firefox and Safari; it is not a platform fact. On every native build the answer is the constant Xbox, so the only caller is a web build with a non-Xbox pad, which nobody has run: real pads in browsers are still an open check in `docs/port/web-to-be-tested.md`. The samples draw letters as text, and their fonts have no PlayStation symbols.

No record exists; the nearest is `GAP#1`. The shape is mechanism only, as T1 asks, but its client test is weak: no observed non-Xbox player and no sample ready to draw other prompts.

### OCA-76: OCA-76 (engine per-user data directory): in-tree copies, IDBFS autoPersist and a JSON writer

<a id="oca-76"></a>
*owner's decision · DECISION · could · S*

OCA-76 is open in Later (`docs/product/BACKLOG.md:555,697,2614`; `docs/product/STORIES.md:1549`), waiting for a second caller or a browser one. The tree holds two hand-written copies of the location code, `website/src/content/docs/docs/guides/your-settings.mdx:124` and `tools/save_check/settings.cpp:22`, and the browser mount in `tools/save_check/persist.js:1-24`. The wait condition is not newly met: those copies are OCA-53's own guide and example, written when OCA-76 was split out, and `docs/2026-11-04-decision-recommendations.md:465-474` postdates them and still records the wait. No sample in the tree saves. Emscripten's IDBFS `autoPersist` option is real and unmentioned in the repository, but it removes only the write-side sync: the restore before `main` still needs the pre-js, and it gives no completion result, which conflicts with the story's explicit flush. An engine JSON writer contradicts `README.md:179-184`, "writing it is yours", and is a separate scope call. The record also covers IDEA#3, BDP-19 and GUIDE#12's leftover (`docs/beginner-guides.md:415`). Under T1 the story needs its client first.

### REV#60: The reduced-motion preference reaches a game only through LineSweeper's own JavaScript

<a id="rev60"></a>
*owner's decision · DECISION · could · S*

LineSweeper reads `prefers-reduced-motion` through a sample-owned `EM_JS` call (`samples/linesweeper/main_web.cpp:12`, `:41-45`, `:128`) and chooses `Motion::reduced` from it. Nothing under `engine/` reads the media query or Windows' `SPI_GETCLIENTAREAANIMATION`, and the sample's native `main.cpp` does not either, so the same game honours the setting in a browser and not as an executable. A game from the starter must write its own `EM_JS` and its own Win32 call, which is platform code in game files.

The browser half was a recorded scope decision, not an oversight: BDP-22's notes in `docs/product/STORIES.md:2939` put the desktop setting out of scope, and `docs/product/BACKLOG.md:2596` records that nothing in `engine/` changed. Only the native omission and the absence of a portable fact are new, and no `GAP#` or `IDEA#` row proposes one. It fits if kept to a fact, the choice staying the game's (T1). It needs the owner because the 2026-10-27 accessibility decision drew the line at input, focus and navigation.

### REV#82: The ISO extra key has no name; left and right modifiers are one key

<a id="rev82"></a>
*low priority · DECISION · could · S*

Verified. `engine/input/keyboard.h:20-21` states that left and right modifiers are combined, citing T3, and the enum (22-88) has no ISO 102nd key, no Windows key and no menu key. A search of `engine/` for `OEM_102`, `IntlBackslash`, `VK_LWIN`, `VK_RWIN` and `VK_APPS` finds nothing, so an ISO keyboard's key beside left Shift translates to `Key::none`.

The modifier half is not unrecorded. `docs/review/colourwars-reports/input.md` (around lines 64, 78 and 124-125) discusses the combined modifiers and records that a future split adds new keys (`left_shift` and so on); `docs/review/2026-09-26/README.md:437` also notes it. The route is decided and the split waits for a client (T1); no sample has a two-players-on-one-keyboard mode asking for it. The two halves are separate: the ISO key is a plain hole in a set meant to name positions and is additive under `key_name`'s stated token rule. The Windows key is arguably best left unnamed, since the operating system owns it. The ISO, Windows and menu keys have no record.

### REV#83: Window policy a game cannot state: fixed size, starting position, monitor, maximised

<a id="rev83"></a>
*low priority · DECISION · could · M*

Verified. `engine/app/win32/window.cpp:369-370` always uses `WS_OVERLAPPEDWINDOW` when not full screen, and `CreateWindowExW` at 388-391 always passes `CW_USEDEFAULT` for position. `ApplicationOptions` (`engine/app/application.h:47-101`) has title, class name, resolution, full screen, visibility, cursor options, `target_fps`, vsync, thread counts, `view_capacity` and minimum window size, and nothing for resizability, position, monitor or maximised. Neither `application.h` nor `window.h` exposes a monitor or desktop-size query, and `docs/feature-candidates.md:263-264` leaves mode enumeration as a separate, later question.

This is a bundle of four unrelated options. The engine deliberately makes layouts follow the client size (`IDEA#10`, landed as OCA-81), which weakens the case for a fixed-size window, and remembered positions bring stale-monitor handling under per-monitor DPI.

`IDEA#10` and `IDEA#16` record adjacent parts; the four options themselves have no record. T1 asks for a client first: no sample, report or story names any of them.

### REV#84: The player's preferred language as a fact the game can read

<a id="rev84"></a>
*low priority · DEFERRED · could · S*

Verified. A search of `engine/` and `samples/` for `navigator.language`, `GetUserPreferredUILanguages`, `GetUserDefaultLocale` and `GetLocaleInfo` finds nothing. A search of TODO.md, `docs/product/BACKLOG.md`, `docs/feature-candidates.md`, the survey, the decision recommendations and `docs/design/PHILOSOPHY.md` for locale, preferred language or system language finds no row and no decision. The localisation stories left the string table and the choice of language to the game, and PHILOSOPHY's non-goals do not exclude a query.

The proposal concedes its own weakness: nothing in the tree ships two languages, so nothing would call it, and its stated medium value is generous on that basis.

It is mechanism only and not excluded, but T1 asks for a client first, and there is no localised sample or consumer.

### REV#96: No upper bound on the browser drawing buffer's pixel ratio

<a id="rev96"></a>
*low priority · DEFERRED · could · S*

`Window::Impl::measure` in `engine/app/web/window.cpp:440-469` sets the drawing buffer to the CSS size times `emscripten_get_device_pixel_ratio()`, rounded, with only the minimum window size as a floor (lines 310, 453-456). There is no upper bound, option or `Module` property, and nothing in TODO.md, BACKLOG.md, the feature candidates or the survey records one. R6 (`docs/port/web.md:1147-1150`) and `docs/port/web-to-be-tested.md:136` leave the 9,600-particle phone frame unmeasured. The original claim called a ratio cap the cheapest lever for R6; that is not established. R6 as written concerns single-threaded particle work and the buffer upload, and a cap reduces fragment cost only. No measurement shows a phone over budget, and a page can already reduce pixels by giving the canvas a smaller CSS box, at a cost in layout. Under T1 the client would be a failed `SM-13` measurement, which has not happened. The record is a contingent answer under R6, not work to build.

### REV#97: Drawing buffer rounded from CSS size times ratio, not read from device-pixel-content-box

<a id="rev97"></a>
*low priority · DEFERRED · could · S*

`engine/app/web/window.cpp:452-456` rounds the CSS size times `devicePixelRatio`, and `docs/port/web-as-built.md:119-123` records the choice of measuring each frame over a ResizeObserver without mentioning `device-pixel-content-box`. The platform fact is sound: at a fractional box or ratio the rounded buffer can differ by one pixel from the box the browser snaps to, which causes a resample, and Safari lacks `device-pixel-content-box`. The symptom is predicted only; no such blur has been observed in this tree. The original claim that WEB#6 used only integer CSS sizes is wrong: `docs/port/web-to-be-tested.md:75-91` includes fractional cases (1000x563, 1186x667, 593x334, 536x302) and reports text at 1.25x, 1.5x and 150% and 200% zoom as clean. The uneven grid lines it records are attributed to drawing 1280x720 content smaller. The Firefox version the claim quoted for the feature is unverified. The change is internal to the web window and adds no API, but it would add a second code path where the as-built record chose one.

## Engine: audio and diagnostics

### REV#20: Loop regions play on Web Audio but are undocumented and untested above the reader

<a id="rev20"></a>
*worth a story · OPEN · should · S*

Verified: `engine/audio/xwb_file.h:32-35` and `engine/audio/xwb_file.cpp:462-484` parse and bounds-check `loop_start` and `loop_length`. `engine/audio/webaudio/audio_device.cpp:156-157,273-281,646-647` pass them to `loopStart` and `loopEnd`, and the position at `:119-121` wraps within the region. The fields appear in tests only in `tests/audio/xwb_file_tests.cpp`; the null backend's recorded sound carries only `loop`. Neither the website guides nor `samples/audio/README.md` mention a loop region, and `samples/audio/source/verify_content.py:91` requires the sample's waves to have none, so no shipped content exercises it.

Not verified: the XAudio2 half, which goes through DirectXTK's reader, and whether xwbtool writes a region from a WAV's `smpl` chunk. A guide cannot promise the feature before an authoring check. The only record is `docs/api-teachability.md:218`, which lists the term. It fits the rule that a seam ships with its headless implementation and its documented contract, adds no mechanism, and bears on GAP#2's order.

### REV#21: A pad press never unlocks browser audio, and the audio contract does not say so

<a id="rev21"></a>
*worth a story · OPEN · should · S*

Confirmed by reading; not run in a browser. `engine/audio/webaudio/audio_device.cpp:102-103` listens for `keydown`, `mousedown`, `pointerdown`, `pointerup` and `touchend` only. `unlock()` (`:197-214`) runs from that listener (`:228-231`) or from `add_bank` when `navigator.userActivation.hasBeenActive` (`:263-269`); nothing polls. A Gamepad API press dispatches no DOM event, so a player who uses only a pad never unlocks audio in any browser, which is wider than the original Safari and Firefox framing. One-shots are dropped meanwhile and persistent voices wait, per `engine/audio/audio_device.h:172-180`. It has not been seen because most players click or press a key first. `audio.mdx:35-52` and the header say "key press, click or touch"; `docs/port/web.md` D13 (`:983`) and `WEB#3` do not cover the pad case; `samples/minimal/web/shell.html:43-49` starts on load. Couch play is the stated centre, so this is a main case. The fix is a documentation sentence and, at the owner's choice, a page-level start gate; an engine path would teach audio about pad input against D13 and the dependency direction. The header edit falls under the CHANGES rule.

### REV#41: No engine diagnostic sink for non-fatal messages

<a id="rev41"></a>
*worth a story · DECISION · should · S*

Verified: the engine has 13 `OutputDebugStringA/W` sites (`engine/render/d3d11/device_resources.cpp:90,121,135,191,262,415`, `engine/render/d3d12/device_resources.cpp:134,233,533`, `engine/render/vulkan/device_resources.cpp:147-149,533`) and 5 `fprintf(stderr)` sites (`engine/app/web/window.cpp:381,556,560,567`, `engine/assets/resource_loader.cpp:181`). No engine header declares a log or sink. The loader's notice, an optional sound bank replaced by `SoundBank::silent()`, carries a comment at `engine/assets/resource_loader.cpp:172-180` saying a person should be able to find out. Issue #123 describes the client keeping its own failure log; #125 is adjacent.

Corrections: in a Windows-subsystem release build stderr is not lost, only unseen by a player who double-clicks; `website/src/content/docs/docs/guides/windows-release.mdx:67` relies on redirecting it. The minimal template's `try` covers `app.run()`, so a throw during play still reaches its message box. No record holds this item. It fits as one replaceable function (T6, T1) and must not grow into macros or channels (T12). The raylib comparable was not re-fetched.

### REV#42: Process memory and time to first frame are measured nowhere natively

<a id="rev42"></a>
*worth a story · DECISION · could · S*

LineSweeperFrameBench's record holds per-frame phase times, the device and timestamps only (`bench/linesweeper_frame_result.h:24-51`). A search of `bench/`, `tools/`, `engine/`, `samples/` and `tests/` finds no process-memory query and no time-to-first-frame mark, and `docs/performance/`, BACKLOG.md and TODO.md have no start-up or memory row for Windows. The design sizes memory by argument: the 256 KB vertex page per view is a budget checkable against a written spec (`docs/design/PHILOSOPHY.md:731-733`), and the low tier draws on shared system memory (`:715-717`). MinimalSmokeTests bounds start-up only with a 60 s timeout (`samples/minimal/CMakeLists.txt`).

There is no evidence that memory or start-up is a problem today; the finding is that nothing would show it if it were. Only the phone's load time and memory are recorded, in `SM-13`'s row. It fits the Performance section's rule that each claim gets its own evidence, as a hardware observation outside ctest and not a threshold. The SM-01 and SM-07 session would otherwise not collect these numbers.

### REV#85: Wave banks and voices are never released while the AudioDevice lives

<a id="rev85"></a>
*low priority · DEFERRED · could · M*

Verified: `engine/audio/audio_device.h:117-147` has `open_wave_bank`, `create_voice`, the play, stop, pause, resume and set verbs and the state queries, and no close or destroy verb. XAudio2 only appends (`engine/audio/xaudio2/audio_device.cpp:150`, `:182`); Web Audio appends at `engine/audio/webaudio/audio_device.cpp:270,342`, and its only shrink is `:295` on a refused load.

Correction: this is a documented contract, not a hidden defect. `engine/audio/audio_resources.h:53-59` states that replacing or emptying a bank name neither stops nor frees the previous bank's voices, that an orphaned loop keeps sounding, and tells the caller to stop a bank's effects first. The growth applies only to a game that replaces banks repeatedly, and no sample does; no report asks for it. No record holds it as an item. It needs a client first (T1), fits the one-owner rule if taken, and overlaps the unload half of the manifest finding.

### REV#86: No voice ceiling, limiter or master level in the audio path

<a id="rev86"></a>
*low priority · DEFERRED · could · M*

Verified: XAudio2 creates its engine with the default flags, plus the debug flag under a debugger (`engine/audio/xaudio2/audio_device.cpp:94-107`), with no mastering limiter and no voice-pool ceiling anywhere in `engine/`. One-shots are one play call each (`engine/audio/xaudio2/wave_bank_policy.h:83-96`). Web Audio connects each panner straight to the destination (`engine/audio/webaudio/audio_device.cpp:163`), with no compressor or master gain. `AudioResources` has groups only (`engine/audio/audio_resources.h:63-74`).

Corrections: clipping and voice exhaustion are inferred, not observed. Nobody has listened to the output (`WEB#3`, `TODO.md:78`) and no report of clipping exists. A game already has a master volume by putting every wave and effect in one group; only ungrouped sounds are fixed at full level, and that is documented. The adjacent record is the survey's "Not now" at `docs/survey/2026-10-27.md:63`. It needs a client first (T1), and a ceiling's number is policy that belongs to the caller. The Godot comparable was not re-fetched.

### REV#87: No playback position, wave duration or start offset on an effect

<a id="rev87"></a>
*low priority · DEFERRED · could · M*

Verified: `engine/audio/audio_device.h:138-147` and `engine/audio/sound_bank.h:174-183` are the whole voice verb list: play, stop, pause, resume, volume, pitch, pan, state and is-looping. No header under `engine/audio/` has a position, duration, seek or offset verb. The Web Audio backend already computes a position internally (`engine/audio/webaudio/audio_device.cpp:112-124`, started at `:183`), and the shared reader in `engine/audio/xwb_file.h` knows frame counts, so duration is headless and cheap. `engine/audio/audio_resources.h:72` leaves fades to the game.

Corrections: there is no client evidence. No sample, issue or client report times anything to audio. The XAudio2 half is unverified: DirectXTK's effect instance exposes no direct position query or start offset, so the work may be larger than it looks. No record holds this item. It needs a client first (T1) and would ride GAP#2 if that is taken. Comparables were not re-fetched.

## Adopter path and documentation

### REV#17: content-tools.mdx says Labrador does not load PNG, and the picture guides still route through texconv

<a id="rev17"></a>
*fix now · OPEN · should · XS*

Confirmed. `website/src/content/docs/docs/guides/content-tools.mdx:10` reads "Labrador does not load PNG pictures, font files or WAV sounds directly". The page was last touched on 2026-10-30 and PNG loading landed on 2026-11-04 with OCA-82, so the sentence is false for pictures on the development site, and its table row at `:15` still sends both picture guides through `texconv`. This is a miss under the same-commit rule in `AGENTS.md`; OCA-82 itself closed correctly against its acceptance (`docs/product/BACKLOG.md:1239`), which asked only that the picture guide offer keeping the PNG. The sentence and table are the XS fix. Beyond it, `your-picture.mdx:21-22` and `:56-61`, `your-animation.mdx:49-51`, `your-window.mdx:612-615` and the card at `guides/index.mdx:37` still teach conversion first, and no sample ships a `.png`. Making PNG the main path is a small story that should wait for, or carry, the version note of the entry on development guides, because the v0.3.0 starter cannot load PNG.

### REV#18: Stale line counts in README.md and two website pages

<a id="rev18"></a>
*fix now · OPEN · should · XS*

Partly confirmed. `README.md:29` says `samples/minimal` "is about 760 lines, its test and start-up check included"; the tracked `.cpp` and `.h` files there now total 861. The figure is repeated at `website/src/content/docs/examples.mdx:22` and `website/src/content/docs/docs/get-started/run-minimal.mdx:37`, so three places need the correction, not two. `README.md:53` gives `tests/` as about 38k lines against 41,978. `README.md:51`'s 42k for `engine/` against 43,726 is within its tilde and is not stale. Nothing in `tools/`, `cmake/` or the website libraries checks these numbers. Two parts of the original claim do not stand: the live-demo link is already at `README.md:33-34`, and the absence of an image is a presentation choice for the owner, not a defect; an image would also have to be kept current. Not recorded. The "copy it" promise leans on minimal's size being stated truthfully.

### REV#22: Evaluation pages do not state that pads are XInput only

<a id="rev22"></a>
*worth a story · OPEN · could · S*

Confirmed for pads. `website/src/content/docs/index.mdx:142-154`, "What Labrador is not", has seven bullets and none on pads or audio. `why.mdx:24` says "with friends ... with gamepads" and `:109` "Four gamepad slots" without naming XInput, and the page has no limits section. The XInput statement is only at `docs/guides/input.mdx:257` and `troubleshooting.mdx:657`, where an evaluator does not look. `audio.mdx:328-330` does state in-memory banks only. `GAP#1` records the pad feature as an open decision; the missing statement on the evaluation pages is not recorded. The other limits the original claim listed (camera rotation, custom shaders, render targets, clip rectangles, the glyph range) were not verified individually and each needs checking against the headers before it is listed. Fits T6 and is documentation only. The pad line alone is XS; its wording says what is true today and promises nothing while `GAP#1` is undecided.

### REV#25: Linear filtering can sample neighbouring atlas frames and the guide does not say so

<a id="rev25"></a>
*low priority · OPEN · could · XS*

`build_sprite_quad` in `engine/render/sprite_geometry.cpp` (texture coordinates at about line 112) maps exact source-rectangle edges with no inset, and clamp addressing protects only the atlas's outer edge. Under `TextureFilter::linear` with scaling or rotation, edge fragments blend up to half a texel past the frame. No guide, render header, `engine/render/SEAM.md` or sample README mentions bleed, gutters or frame spacing; `website/src/content/docs/docs/guides/sprites-and-text.mdx:435-454` is silent, and `engine/render/renderer.h:61-62` remarks only that linear makes tile seams shimmer.

No published example is wrong: the guide's linear example at `:451` draws a whole texture, which clamp protects, and no sample draws a sheet frame under linear outside a null test. This is a missing caveat, not a false statement. A paragraph fits T2 with no code or golden cost; an inset in the shared arithmetic would change the pixel contract and is not proposed. Not recorded elsewhere.

### REV#43: A copyable debug readout component: frame time plus caller-supplied lines

<a id="rev43"></a>
*worth a story · DECISION · could · S*

Verified: nothing ready-made shows a running value on screen or in a log. `samples/components` holds an option row, slider, nine-slice, animation preview and pointer capture only. `website/src/content/docs/docs/get-started/debug-your-game.mdx` (737 lines) teaches the debugger and mentions the Output window only for build and validation output (`:77`, `:167`, `:587`). The parts exist: `Application::frame_timings()` (`engine/app/application.h:301`) and `Label` (`engine/render/label.h:35`).

Correction: a beginner is not without any route; a game can construct its own `Label` and set its text today. What is missing is a documented pattern. This is the narrower slice of REV#10 (deferred, `TODO.md:97`), so it is adjacent to a recorded deferral, and GAP#6's option B (`docs/2026-11-04-decision-recommendations.md:143-158`) is the precedent for a debug component with no engine change. It fits as a copied-and-owned component with nothing in the frame loop (T8). Whether the starter guides count as REV#10's awaited client is the owner's call.

### REV#44: No sample adds or removes a scene object during play

<a id="rev44"></a>
*worth a story · DECISION · could · M*

Confirmed. Every `Scene::add` in non-test sample code is in set-up (`samples/audio/audio_state.cpp:24-46`, `samples/collision/arena.cpp:20-26`, `samples/linesweeper/play_state.cpp:95-152`, `samples/local_multiplayer/play_state.cpp:35`, and minimal's hello state). No sample removes a scene object, and the only `for_deletion()` in sample code is `samples/collision/body.cpp:93`, which returns false. `website/src/content/docs/docs/guides/collision.mdx:129-132` shows a two-line Projectile snippet that is not quoted from compiled source, while `why.mdx:185-187` presents deferred adds and removals as a selling point. The mechanism has engine tests but no sample client and no quoted guide. The original claim overstated two points: `your-level.mdx:480-481` and `:519` already show a number as text and a win loop. Timers, score, restart as a taught pattern and `engine/core/random.h` in a guide are absent as claimed. Not recorded anywhere. Fits T1 as a second client with no engine change. A seventh sample adds ctest entries, page tests, inventory and website pages, so the size of the sample set is the owner's call.

### REV#61: The starter copies two entry files and a hand-written fatal-error report

<a id="rev61"></a>
*owner's decision · DECISION · could · M*

Verified: `samples/minimal/main.cpp` is 158 lines, with `wWinMain` at `:100-101`, a smoke-test wrapper, a `try`/`catch` that also covers `app.run()`, and a `MessageBoxA` titled "Sample - startup failure" at `:153`. `samples/minimal/main_web.cpp` is 63 lines, with `EM_JS` glue at `:23-29` and the static Application at `:48`. The engine has no fatal-report or run helper.

Corrections: the box title is not an unrecorded gap. Published guides already tell the adopter to change it (`website/src/content/docs/docs/get-started/debug-your-game.mdx:374`, `website/src/content/docs/docs/guides/windows-release.mdx:219-221`), and `docs/beginner-guides.md:738` records the confusion. The two-file anatomy is a stated design: `docs/design/ARCHITECTURE.md:624-628` and `docs/port/web.md:589-594`. An entry-point macro runs against T12 and CONVENTIONS; a run function would amend ARCHITECTURE. T10 argues for it; copy-and-own argues against. The smallest defensible slice is a fatal-report function shared with the diagnostic sink above. Comparables were not re-fetched.

### REV#62: Development guides use features the release starter does not have, with no note at the passage

<a id="rev62"></a>
*owner's decision · DECISION · could · M*

Confirmed. `docs/website-versioning.md:3-6` and `:30` keep the root site and `/docs/` as development documentation, an owner decision of 2026-10-27 (also `docs/website-plan.md:236`). `website/src/components/StarterDownload.astro` hands out the newest release's asset, v0.3.0, and master is 85 commits past that tag. `website/src/content/docs/docs/guides/your-picture.mdx:89-98`, "Or keep it a PNG", carries no version note; a reader on the v0.3.0 starter who follows it should meet a texture-not-found stop. That outcome is inferred from v0.3.0 having no PNG reader and was not run. The existing mitigations (the footer's commit, `guides/your-version.mdx`, `your-project.mdx:120-123`) are not at the point of use. The gap is zero on the day of a tag, so "every reader" overstates it. Not recorded as a gap. Fits T6 and the near-beginner floor. Defaulting starter readers to the release snapshot reopens the recorded decision; a banner or a per-passage note does not, and one aside on the PNG passage is XS.

### REV#63: No .vsconfig or one-command route for the Visual Studio prerequisite

<a id="rev63"></a>
*owner's decision · DECISION · could · S*

Confirmed. No `.vsconfig` is tracked and no document mentions one; Build Tools appear only in `tools/cloud_performance/README.md:30`. `website/src/content/docs/docs/get-started/prerequisites.mdx:47-73` has the reader tick the workload and verify three components by eye, and records about 16 GB. That figure was measured with five workloads and two compilers (`:342-348`), so the minimal selection is smaller and unmeasured. Microsoft's documentation confirms that a first install accepts a configuration file on the bootstrapper's command line.

Microsoft documents the missing-components prompt only for opening a solution. The starter is a CMake folder driven from a command prompt, so a file in the project would likely do nothing by itself. A configuration file plus one documented command is Microsoft's mechanism, not a bundled installer, but it borders the decision at `docs/product/BACKLOG.md:25` and is the owner's call. Component ids would then track Visual Studio versions. It fits T9 and T10 and touches only the starter and a guide.

### REV#64: No copyable on-screen touch controls outside LineSweeper

<a id="rev64"></a>
*owner's decision · DECISION · could · M*

The only on-screen controls in the tree are LineSweeper's, `samples/linesweeper/presentation/touch_controls.h` and `.cpp` (319 lines), whose `Control` enum is that game's eight buttons and which depends on the sample's `arrangement.h`. The starter draws a marker for touch and nothing else (`samples/minimal/states/hello_state.cpp:91`, `:163-183`), so the page a beginner publishes cannot be moved or quit on a touch-only device. `samples/components` has no touch button or virtual stick; its pointer capture follows one contact, where held controls need several. The Input guide says only that on-screen controls are the game's (`website/src/content/docs/docs/guides/input.mdx:454-461`).

No document records this; `IDEA#13` and OCA-83 concern menu pointers. It fits PHILOSOPHY's UI section as a component, with T1 keeping what a rectangle means in the game, and a second client is needed before anything moves into `engine/`. Real-glass behaviour is unverified for every touch path (`WEB#5`, `SM-13`), so this is copyable code and headless tests, not a claim of phone support.

### REV#88: No one-page quick reference of the game-facing API

<a id="rev88"></a>
*low priority · DECISION · could · S*

Confirmed, at low priority. Three candidates asked for the same page and differ only in mechanism: generated from the header reader, or from the sample excerpts the site already quotes. No cheat sheet or quick reference exists in `docs/`, `docs/product/`, `website/src/content` or `TODO.md`. `website/src/content/docs/docs/reference/index.mdx` offers a module table, one page per header and six pages "you will use most", and each module has an Overview page, which serves part of the need, as does site search. G22's companion introduction is adjacent and unfinished. The header counts the candidates quote disagree (84 game-facing headers against 93 pages) and were not recounted. "Small" is optimistic for the header-generated form, which needs a curated name list and a signature renderer: S to M. Fits T10 only if generated; a hand-written list is a second copy that rots, against the reference's word-for-word rule. No engine change. A new page is the owner's scope call.

### REV#89: No statement on Proton, Steam Deck or Steam Remote Play Together

<a id="rev89"></a>
*low priority · DECISION · could · S*

Confirmed as omissions. No `.md` or `.mdx` file mentions Steam Deck, Proton, Remote Play, Steamworks or Wine. `docs/design/PHILOSOPHY.md:873-879` makes online play a permanent non-goal and couch multiplayer the niche, and `website/src/content/docs/why.mdx:249-256` says there is no Linux or macOS port plan. Valve's partner page says Remote Play Together is enabled automatically for games listed with a local multiplayer capability. Neither subject needs engine code.

Nothing can be claimed yet. No Deck or Linux machine is on the owner's hardware list and emulation is not acceptance, so a Proton line could only say unverified. That the feature needs no netcode is an inference from streaming, not Valve's statement, and the link to `GAP#1` through Steam Input is unchecked. Remote Play Together is a store feature of a shipped game, outside the engine's scope. The honest form is one or two sentences labelled as outside the engine and unverified, worded as a route and not as platform support.

## Consumption, build and platforms

### REV#26: No standing check builds a renamed starter project

<a id="rev26"></a>
*low priority · OPEN · could · S*

Confirmed, at low priority. `tools/consumer_check/starter_zip.py` takes a directory, `--release` and `--engine`, and `assemble.py` a project and `--engine`; neither takes a name, so the starter is always MyGame. `website/src/content/docs/docs/guides/your-name.mdx` is 332 lines and its step 2 is one long PowerShell line that reports "Replaced MyGame 27 times". Neither `tools/consumer_check` nor `.github/workflows/ci.yml` builds a renamed project; `docs/beginner-guides.md:447-472` records agents following the guide on `x64-debug` and `x64-release`, a single audit and not a standing check. The guide works as written. A rename script shipped inside the starter would reach readers only from the next release's ZIP and is new tooling, which is the owner's call; `docs/design/PHILOSOPHY.md:882-883` allows such a script. The value that needs no call is the missing test: the consumer job can run the guide's own replacement and build the result. Not recorded.

### REV#45: No engine version the code or the build can read

<a id="rev45"></a>
*worth a story · DECISION · should · S*

Verified. `CMakeLists.txt:6` is `project(Labrador LANGUAGES CXX)` with no `VERSION`. A search of `CMakeLists.txt`, `engine/CMakeLists.txt` and `cmake/*.cmake` for `LABRADOR_VERSION` and `version.h` finds nothing, and `engine/core` has no version header, although tags `v0.1.0`, `v0.2.0` and `v0.3.0` exist. PHILOSOPHY's public-face section (`docs/design/PHILOSOPHY.md:791-799`, 836-841) says that before 1.0 a name leaves with no alias and no `[[deprecated]]`, so a consumer has no compile-time handle on which side of a break it is on. `cmake/source_revision.cmake` records a git revision for the bench and the LineSweeper bundle's `BUILD.json`, which covers part of the bug-report case but is not a release version readable from engine code.

It is not quite XS. A constant in the tree must agree with an owner-cut tag, so CONTRIBUTING.md's release procedure gains a bump step and ideally a lint check, and whether the tree carries the last or the next version between tags is a policy choice.

No record exists. It fits T5 and the promise that a release is a record; callers are the starter project and the client's pin moves. It needs a CHANGES.md entry.

### REV#46: A named configure diagnostic for a consumer manifest missing an engine dependency

<a id="rev46"></a>
*worth a story · DECISION · should · S*

The candidate has two halves. The install helper is not new: it is PI-11, open at `docs/product/BACKLOG.md:780` and parked by the owner on 2026-11-03 until a request is filed (`docs/product/BACKLOG.md:515`). `samples/minimal/CMakeLists.txt:119-153` does hard-code the runtime DLLs, system libraries and six notice files, and `cmake/*.cmake` holds no consumer helper.

The second half is unrecorded. `engine/CMakeLists.txt:416` and `:440-441` are bare `find_package(... CONFIG REQUIRED)` calls, and `samples/minimal/README.md:52-55` records that a copy lacking `lodepng` stops at `find_package(lodepng)`. The pin-move guide's third step is a manual `vcpkg.json` comparison. A message naming the missing dependency and pointing at the engine's `vcpkg.json` fits T6 with no new mechanism; its callers are the starter and the client at each pin move. Only a missing dependency should be fatal: a consumer may run a newer baseline. The claim that every dependency change edits every game's CMake file is overstated: `lodepng` changed only the manifest.

### REV#47: Browser builds from a Linux or macOS host are unverified and untested

<a id="rev47"></a>
*worth a story · DECISION · could · M*

Confirmed. In `CMakePresets.json` the native `base` preset carries a Windows host condition (`:9-13`) and `wasm-base` (`:111-121`) carries none. Every build job in `.github/workflows/ci.yml` runs on windows-latest (`:144`, `:242`, `:586`, `:813`, `:875`, `:1003`); `:85` is an Ubuntu path filter and `:764` runs Safari on macOS from Windows-built artifacts. NMS-04, closed, went as far as listing presets under WSL 2 and states that Linux-hosted wasm stays unverified (`docs/product/BACKLOG.md:971-995`); the optional recorded configure attempt at `:2727` was never taken.

This is not a defect: the site says unverified and that is true. The claim that only the Windows content tools block it is a guess, since no build was run and the build does not use those tools. The size is M, not S: first-run fallout, a guide for a non-cmd shell and the page runner's browser discovery. A CI job needs the owner to edit required checks. It fits T5 and adds no platform; no non-Windows adopter has been observed (T1).

### REV#48: An optional starter workflow that builds the browser page and deploys it to GitHub Pages

<a id="rev48"></a>
*worth a story · DECISION · could · M*

Confirmed. `website/src/walkthrough/ci.yml` has a single windows-latest job that configures, builds and tests `x64-debug`, with no wasm or Pages step. `website/src/content/docs/docs/guides/your-web-page.mdx:10-30` is a manual route: local Emscripten, `wasm-release`, five files copied into a second repository, about 30 minutes the first time and 3 for each update. The engine's own `.github/workflows/ci.yml` has `wasm` and `wasm-consumer` jobs (`:586`, `:875`), the latter building the starter as a page, so a recipe exists. No story or TODO row covers it.

The guide's two-repository shape is deliberate: the game's repository can stay private, and free Pages needs a public one. A deploying workflow changes that trade and must say so. The starter's existing workflow has never run on GitHub from a ZIP-made project (OCA-79, open, the owner's), so this follows OCA-79, and it needs an emsdk install and caching the walkthrough file lacks. It is a copyable example, not engine mechanism; demand is unobserved (T1).

### REV#65: Consumption routes beyond the submodule and the starter ZIP

<a id="rev65"></a>
*owner's decision · DECISION · could · S*

The absences are confirmed. `LabradorEngine` has no `install()` or `export()` rule (the only install rules are the sample's, `samples/minimal/CMakeLists.txt:124-152`), no package config and no vcpkg port, and `README.md:156-206` documents only `add_subdirectory` of a submodule and the starter ZIP. `CMakeLists.txt:48` guards tests and samples behind `PROJECT_IS_TOP_LEVEL`, so FetchContent at a tag is plausibly fine, but nobody has run it. That is inference, not measurement.

FetchContent does not remove the cost the candidates cite: vcpkg reads only the top-level manifest, so the consumer still copies dependencies and baseline by hand. The public walkthrough repository is not a template and sits one release behind by recorded intent: OCA-79 (`TODO.md:56`) excludes its pin cadence, and the template flag is an owner account action that fits inside it. A package or port has no caller and sits against ARCHITECTURE's source-built game project. No defect and no false public statement. T1: needs a client first.

### REV#66: Prebuilt Windows sample binaries on each release

<a id="rev66"></a>
*owner's decision · DECISION · could · M*

Release v0.3.0 lists two assets, the docs tarball and the starter ZIP, and `CONTRIBUTING.md:294-364` describes attaching only those. Install rules exist only in `samples/minimal/CMakeLists.txt:124-146`. No prebuilt sample binary is recorded in TODO.md, BACKLOG.md, the survey, the feature candidates, the decision recommendations, PHILOSOPHY or the website plan; OCA-60 is a game's own release folder and OCA-04 is browser hosting. The cost is larger than one extra release step: install rules for each further sample with its content, runtime libraries and notices, a notice check like `website/src/walkthrough/release-files.txt`, a clean-machine start check, and a place in the pre-publish gate because a published release is immutable. Unsigned executables meet SmartScreen, and Release has no WARP fallback, so an evaluator without a GPU gets a failure dialog. The suggested use as a Proton check is speculative; the tree makes no Proton claim. Nothing conflicts with PHILOSOPHY and the engine does not change, but it adds owner release work and a support surface with no recorded request, the same class of call as OCA-04.

### REV#90: SDL3 behind the window, input and audio seams as a route to Linux and macOS

<a id="rev90"></a>
*low priority · DECISION · could · M*

Partly confirmed. `website/src/content/docs/why.mdx:255-256` says there is no Linux or macOS port plan; `TODO.md:183` lists macOS and iOS as waiting on a plan document; `engine/CMakeLists.txt:97-106` refuses anything but Win32 and Emscripten. SDL appears in the records only as `GAP#1` option B in `docs/2026-11-04-decision-recommendations.md`, a gamepad layer with no SDL window, so the three-seam framing is new.

The absence is recorded and deliberate, not a gap. NMS-03 closed by stating there is no plan, the engine comparison of 2026-10-27 left out platforms beyond Windows and the browser, and the Why page's first support condition is that a game needs the platform. The recommendation excluded an SDL window on purpose. Buying SDL's window and audio sits against T9's reading for audio and against engine-owned window code; the candidate presents it as a neutral purchase. There is no client and no Linux hardware (T1). At most the `GAP#1` decision text gains a paragraph.

### REV#91: A Release game has no symbols procedure and writes no crash dump

<a id="rev91"></a>
*low priority · DECISION · could · S*

`x64-release` sets only `CMAKE_BUILD_TYPE=Release` (`CMakePresets.json:23-27`) and `cmake/settings.cmake:21-48` adds no debug-information flag, so a release game has no PDB; there is no RelWithDebInfo preset. The site says so at `website/src/content/docs/docs/get-started/debug-your-game.mdx:609-623` and gives no option, command or file to keep. Nothing in `engine/app` or the template writes a minidump or installs an unhandled-exception filter; the template turns C++ exceptions into a message box, but an access violation closes the game with no message (`troubleshooting.mdx:672-685`).

The guide marks the section as one a beginner can skip, so the omission is partly deliberate and matters at the ship step, not in the first hour. `docs/beginner-guides.md:834` already records the missing procedure as a minor audit finding; no story or TODO row exists, and the minidump half is unrecorded. The symbols procedure fits as a build setting and a guide. A dump writer in the engine needs a client first (T1) and would be a consumer-visible change.

## CI and quality

### REV#19: SECURITY.md is behind the readers it describes

<a id="rev19"></a>
*fix now · OPEN · should · XS*

`SECURITY.md:76-78` says JSON nesting depth is not limited and that rapidjson's recursive parser can exhaust the stack. `engine/assets/json.cpp:58-62` parses with `kParseIterativeFlag`, `engine/assets/json.h:20-21` states that depth is bounded by memory, and `tests/assets/json_tests.cpp` parses 100000 nested brackets. `SECURITY.md:29-31` lists `.dds`, `.spritefont`, `.xwb` and the manifest JSON and speaks of three binary readers; `engine/render/png_file.h` is a fourth content reader, reached from `resource_factory.cpp:78-92` when `name.png` exists and `name.dds` does not. The memory bullet is also behind: a PNG is capped at 16384 a side and `read_file_bytes` refuses a file larger than `size_t`.

The JSON sentence makes the policy more pessimistic than the code, not less safe; the real omission is the PNG surface. Nothing records this: the hygiene review and `TODO.md` do not mention `SECURITY.md`. It is a T6 and T4 record fix. The suggested lint tying reader changes to `SECURITY.md` is not needed for it.

### REV#49: No sanitizer configuration on the device-free hosts

<a id="rev49"></a>
*worth a story · DECISION · should · M*

No preset, CMake file or workflow mentions a sanitizer or `/analyze`. `SECURITY.md:68-71` states the absence, and the security-policy story (OCA-73, SM-42; `docs/product/STORIES.md:1490-1497`) records it as the posture, declining under T1 only a sandboxing or fuzzing framework. Commit `8362e2dd` records three reader bugs found by reading: a `ByteReader` that could bind a temporary buffer, a `size_t` narrowing before a full-length read, and a 30000-pixel PNG ending in `bad_alloc`. `x64-debug-null` and the wasm Node suites need no device.

A sanitizer sees only inputs a test executes, and those bugs sat on untested inputs, so it complements the hand cases. MSVC has ASan and libFuzzer but no UBSan or TSan. MSVC ASan may refuse to link vcpkg's static lodepng without a triplet or annotation opt-out; that is inferred, not built. A scheduled job needs no new required check. It fits T5 and costs no frame time (T8); it changes a stated posture, so the owner decides.

### REV#50: Nothing tallies CI failures by step

<a id="rev50"></a>
*worth a story · DECISION · could · S*

The last 150 `ci.yml` runs (2026-10-27 to 2026-11-09) are 110 successes, 28 failures and 12 cancelled. The failing runs hold 39 failed job-steps: 14 are the headless Chrome sample-page step, 3 Safari sample pages, 3 the wasm consumer page, 3 the Chrome pixel contract, 3 the wasm-null test step and 1 Firefox. `tools/ci_build_retry.py` and `tools/master_watch.py` handle single events; `ci.yml:56-63` holds one hand-measured rate. Hygiene theme H7, closed under SM-75, fixed specific waits, not measurement.

The original headline, red one time in five with browser pages the dominant flake, is not established. Most failures sit on work-in-progress refs, four of the fourteen Chrome failures are `workflow_dispatch` runs on a CI-tuning branch, and only three failures were on master. The tally cannot separate flakes from real breaks. A report script fits T6 and the existing `tools/` pattern; it touches no engine surface.

### REV#51: No lint that every source file is enumerated in a CMake file

<a id="rev51"></a>
*worth a story · DECISION · could · S*

`AGENTS.md` states both hazards: a new `.cpp` left out of its CMake list is silently absent from the build, and the ctest inventory sees entries, not the doctest cases inside them. `tools/check_ctest_inventory.py:1-7` says the same and `docs/code-hygiene.md:25-27` calls a split that silently drops a test a regression. None of the `cmake/check_*.cmake` scripts or `tools/*.py` checks enumeration. A basename scan found 335 tracked `.cpp` files under `engine/`, `tests/`, `samples/` and `bench/`, none unnamed by a CMake file, and 1,212 `TEST_CASE` occurrences.

No dropped file or case was cited, so the evidence is a stated hazard with zero current orphans. The silent case is mostly a test or sample source; an omitted engine source normally fails at link. A basename match is weak, so the lint needs a per-directory rule. The orphan half fits T6 and the house lint pattern. The case-count floor needs built executables per configuration and a number every test-adding commit updates; T1 argues against it. Nothing records either.

### REV#52: The configurations CI skips are built only at release

<a id="rev52"></a>
*worth a story · DECISION · could · S*

`ci.yml`'s Windows matrix (lines 313-336) is `x64-debug`, `x64-release`, `x64-debug-d3d12`, `x64-debug-gl`, `x64-debug-vulkan` and `x64-debug-null`; the wasm jobs build `wasm-release` and a `wasm-release-null` override. `docs/verification.md:252-253` and `CONTRIBUTING.md:455-458` record that the four other release presets and `wasm-debug` are built only by `tools/ci_gap_report.py` at release step 3. `ci.yml` already has a weekly cron (line 17) and `workflow_dispatch`, and its changes job runs the full matrix on those events. The gap is recorded as a release step; a scheduled build is not.

No Release-only or `wasm-debug`-only break was cited, so finding breaks at the last step is a risk, not an observed event; `docs/verification.md:297` records one `wasm-debug` configure failure, which was environmental. Schedule-only matrix entries still appear as job names, so branch protection is untouched only if they are separate, non-required contexts. It fits `ci.yml`'s own argument about backends rotting and claims no hardware.

### REV#67: Fuzz targets for the content readers

<a id="rev67"></a>
*owner's decision · DECISION · could · M*

No fuzz, mutation or random-input test exists; `SECURITY.md:70-71` says the tests are hand-written cases. The readers are byte-in functions (`engine/core/byte_reader.h`, `decode_png` in `engine/render/png_file.h`, `parse` over a `string_view` in `engine/assets/json.h`), so harnesses are feasible, and MSVC supports libFuzzer with ASan.

The security-policy story declined a fuzzing framework under T1 (`docs/product/STORIES.md:1497`), and `SECURITY.md:29-36` declares content trusted input, so fuzzing would test a promise the policy deliberately does not make. The original claim was wrong in places: the `.dds`, `.spritefont`, PNG and JSON readers are all in the wasm build, the `.xwb` reader being only the one used solely there; the shipped demo embeds its content and does not fetch it (`docs/port/web.md` section 7); and a missing glyph-rectangle validation is not something fuzzing finds without an oracle. No open row exists.

### REV#68: No static analysis beyond the warning flags

<a id="rev68"></a>
*owner's decision · DECISION · could · S*

No `.clang-tidy`, no `/analyze` and no CodeQL workflow are tracked; `.github/workflows` holds only `ci.yml`, `master-watch.yml` and `website.yml`, and the repository has no code-scanning analysis. Secret scanning and push protection are already enabled. `tools/` holds 11,062 lines of Python; tracked `.mjs` is 17,252 lines including the website, not the 8,700 first claimed.

The value is asserted, not measured: no evidence was given that `/analyze` finds anything in this tree. CodeQL's default setup for C++ needs a build it can observe, or build-mode none, which on this Windows and vcpkg tree is not the single click the claim implied. It fits T5 in principle, but a noisy analyser in vendored DirectX-derived files collides with the zero-suppressions rule, and with no recorded defect T1 weighs more here than for sanitizers. A new job or repository setting is the owner's call. Nothing records it.

### REV#69: rapidjson is outside the dependency inventory; Dependabot alerts are off

<a id="rev69"></a>
*owner's decision · DECISION · should · S*

Two things are separate here. The record fix: `docs/dependency-updates.md` has no rapidjson row although `external/rapidjson` is vendored (`5f3e4cb0`, 2026-08-06), and `external/rapidjson/README.md` says "Vendored from master on 2026-08-06" with no commit id and still describes `ArtAttack.vcxproj` and `C:\VSIncludes`, neither of which exists here. That is an XS false-document fix needing no owner call. Upstream has no tagged release since 1.1.0, so the gap is the missing commit id, not a pin that should move.

The owner's part: Dependabot alerts and security updates are disabled, with 587 `node_modules` entries in `website/package-lock.json`. That is not unrecorded: G115 (SM-24) preferred the written procedure to a bot and reserved T9 on automated bumps. Enabling alerts adds no bot pull requests; version updates for actions would need that argument made. The Node 20 notice on `actions/checkout@v4`, `download-artifact@v4` and `setup-node@v4` is a warning; they already run on Node 24.

### REV#70: No tag-named rebuild or digest comparison of release assets

<a id="rev70"></a>
*owner's decision · DECISION · could · M*

`CONTRIBUTING.md`'s release procedure (about lines 222-375) is manual: `ci_gap_report` locally, the tag ("signed with -s if a key is to hand"), the draft release, then the starter ZIP from `tools/consumer_check/starter_zip.py --release` and the docs snapshot, uploaded by hand. No workflow has a tag or release trigger. `v0.3.0` is an annotated, unsigned tag; its release is immutable with two assets and GitHub-computed sha256 digests.

The claim that nothing rebuilds the assets is too strong. CI's `consumer` job makes the starter ZIP from every checkout, keeps it as the `labrador-starter` artifact, then builds and runs it. What is missing is only the tag-named rebuild and digest comparison. Unsigned tags are a documented choice. `starter_zip.py` claims the same tree gives the same bytes, not machine-independent deflate output, so a compare may fail for benign reasons; that is unproven. It fits the 2026-11-03 direction that checklists become automation, with tag and publish staying owner-cut.

### REV#71: The real thread pool is never run under a race detector

<a id="rev71"></a>
*owner's decision · DECISION · could · L*

`engine/core/thread_pool.cpp:34-180` is the Win32 `CreateThreadpool` implementation and `:181-236` the non-Windows single-thread one, chosen because `docs/port/web.md` declines pthreads (lines 359 and 1161). MSVC has no ThreadSanitizer and Emscripten documents only ASan, UBSan and LSan, so no configured build can race-check the per-view fan-out; the wasm null tests run it on one thread.

`AGENTS.md`'s statement that native and wasm null tests cover the fan-out is accurate: they cover the behaviour, and no race tool was ever claimed. Of the two options offered, a pthreads wasm test build gains nothing, so there is one: a Linux clang TSan build, which needs a `std::thread` `ThreadPool` and a Linux configure path. NMS-04 (`docs/product/BACKLOG.md:973`, `2644`) makes x64 presets refuse a non-Windows host and `BACKLOG.md:958` records no Linux plan. No race has been recorded (T1). MSVC ASan would at least catch lifetime errors in the real pool. Nothing records the gap.

### REV#98: Per-run ccache keys fill the Actions cache past 10 GB

<a id="rev98"></a>
*low priority · DEFERRED · could · XS*

The repository's Actions cache holds 10,692,489,771 bytes in 233 caches. Three ccache prefixes account for 200 of them and 7.45 GB: `ccache-wasm-null` 70 (3.61 GB), `ccache-wasm-gl` 59 (2.96 GB) and `ccache-wasm-consumer` 71 (0.88 GB); 139 caches (5.70 GB) are on pull-request refs. `.github/actions/emscripten/action.yml:80-85` keys on `run_id` and `run_attempt` with a prefix restore key, and its comment says every run saves.

Calling this a defect overstates it. GitHub evicts the least recently accessed entries first, so what goes is stale per-run ccache on closed pull requests, not the Vulkan installer, emsdk or vcpkg caches every run touches. No eviction of an expensive cache was observed. The costs are churn and a risk to rarely read caches. Saving only on master or schedule would end a pull request's second push reusing its first push's objects. Nothing records it; no tenet is in play.

### REV#99: No code coverage measurement

<a id="rev99"></a>
*low priority · DEFERRED · could · S*

No coverage tooling exists: nothing in `cmake/`, `tools/`, `.github/` or `CMakePresets.json` names gcov, llvm-cov, OpenCppCoverage or a coverage preset, and every "coverage" hit in the tree is prose. The tree holds 1,212 `TEST_CASE` occurrences.

This is not a defect. The payoff claimed, finding unexercised device-loss and endpoint-recovery paths, would need a device backend run under a coverage tool; a null-preset run cannot reach device code at all. Those paths are already listed by name as unexercised in `TODO.md`'s `REV#` rows, so a report would mostly confirm recorded rows. Under T1 it is a one-off measurement with no decision waiting on it. Nothing records the absence, and nothing needs to until a question arises that a coverage figure would answer.

### REV#100: Bundle size has no budget; benchmark output is not kept

<a id="rev100"></a>
*low priority · DEFERRED · could · S*

`ci.yml:552-558` runs `LabradorBench` on each native preset as "Reported, not gated" with no artifact. `tools/web_bundle/bundle.mjs` computes raw and Brotli sizes into `BUILD.json` (lines 110-114, 245) with no threshold, and nothing compares a bundle with a baseline. `docs/port/web.md:706-722` keeps one dated measurement: `linesweeper.wasm` at 404 KB, 125 KB Brotli, 153 KB to play.

The claim that no artifact is uploaded is wrong for the bundle: the wasm job uploads `web-dist/linesweeper/`, `BUILD.json` included, as the `linesweeper-web` artifact on every run, so a per-run series exists while artifacts last. `LabradorBench` prints object counts and timings (`bench/bench.cpp:76-97`), not allocation counts, so the machine-independent series said to be lost mostly does not exist. A Brotli count moves with the Node and zlib version (`web.md:757-758`), so a baseline needs a tolerance. A size budget fits the constrained-hardware concern; keeping bench timings fights `ci.yml`'s position that shared-runner timings are not comparable.

### REV#101: No mechanical style check for the Python and Node tooling

<a id="rev101"></a>
*low priority · DEFERRED · could · S*

Tracked style configuration is `.editorconfig` only: no `.clang-format`, `.clang-tidy`, `pyproject.toml`, `ruff.toml`, eslint or actionlint configuration. `cmake/check_whitespace.cmake` is the only style check in the lint job (`ci.yml:151-231`). An untracked `.ruff_cache` sits at the repository root and the 2026-11-05 hygiene findings cite a Ruff F401, so ruff has been run ad hoc. `tools/` holds 11,062 lines of Python. The design documents never mention a formatter.

The website already has its own gate, so "CI runs none" applies to `tools/` and `tests/render/web`, not to all JavaScript; the count of `auto` matches was not re-verified. A pinned ruff or actionlint step fits T5 for the tooling but adds a tool pin to `docs/dependency-updates.md` and a lint-job dependency, and only one F401 finding motivates it. A `.clang-format` conflicts in practice with word-for-word header publishing and golden excerpts, and the original proposal itself recommends declining it. Nothing records either.

## Browser build

### REV#23: Template page stays at "Loading..." when a download fails

<a id="rev23"></a>
*worth a story · OPEN · should · S*

`samples/minimal/web/shell.html:35` sets a static "Loading...", and its `Module` object (lines 44-57) defines only `onRuntimeInitialized`, `onQuit` and `onError`. No sample shell defines `onAbort`, `setStatus` or `monitorRunDependencies`, and no page-level `error` or `unhandledrejection` listener exists outside the test shells (`tests/render/web/shell.html:44`, `tests/app/web/shell.html:62`). A failed fetch aborts in Emscripten's loader before the engine's catch edge exists, so `Module.onError` is never called. This is traced from the shell, not observed on a running page. The guides document the symptom as troubleshooting: `audio.mdx:313-316`, `browser.mdx:779`, `your-web-page.mdx:343` and `web-host.mdx:68-69` under `website/src/content/docs/docs/guides/`. A developer is not left blind, since the console shows the 404 and the page runner fails on it; the gap is the player-facing line on an adopter's deployed page. `samples/linesweeper/web/index.html:255-257,360,382` already handles failed loads. `your-web-page.mdx:60` quotes the shell, so the guide changes in the same commit. It is T6 applied to page code, as `docs/port/web.md` section 7.3 frames it, with no engine API.

### REV#27: Development and starter page canvases have no accessible name

<a id="rev27"></a>
*low priority · OPEN · could · XS*

All six development shells carry a bare `<canvas id="canvas" tabindex="0">`: `samples/minimal/web/shell.html:34`, `samples/audio/web/shell.html:38`, `samples/collision/web/shell.html:32`, `samples/components/web/shell.html:31`, `samples/linesweeper/web/shell.html:89` and `samples/local_multiplayer/web/shell.html:31`. Their status paragraph has no role, and no shell contains an `aria-` attribute or `role=`. The bundle page does both: `samples/linesweeper/web/index.html:110` sets `role=status` and lines 278-279 label and describe the canvas. The owner's decision of 2026-10-27 (`docs/product/BACKLOG.md:15`, OCA-08) leaves accessibility choices to games, and nothing records the template page's canvas name; SM-28 is website accessibility in CI and OCA-08 and BDP-24 cover the position and the demo note. The benefit is small: a label on the template is a placeholder the adopter must edit, as the title already is. `website/src/content/docs/docs/guides/your-web-page.mdx:60` quotes the shell markup, so the excerpt changes in the same commit. It is page code only and consistent with the recorded position.

### REV#53: No size budget holds the browser module's payload in CI

<a id="rev53"></a>
*worth a story · DECISION · should · S*

The module's recorded size has risen: 356,750 bytes (`docs/port/web-as-built.md:238`, M3), 403,759 at `c369e8b` (`docs/port/web-as-built.md:688`, `docs/port/web.md:706-710`), 426,100 (`website/src/content/docs/docs/guides/linesweeper-bundle.mdx:80`) and 476,013 (`docs/port/web-to-be-tested.md:465,553`). A local `wasm-release` bundle of 2026-11-05 reads 148,215 bytes under Brotli for the module and 27,837 for the loader, so the play download is about 176 KB. `docs/port/web.md:720` still says 153 KB in the present tense; its table is labelled `c369e8b`, so the sentence is dated, not false. The M3 figure predates the MODULARIZE link, so the rise of a third is approximate. `tools/web_bundle/bundle.mjs:110-121,245` verifies sizes with no budget, and `.github/workflows/ci.yml` only uploads the bundle. Nothing records a budget. It fits PHILOSOPHY as tooling that makes a claim checkable, as `tests/ctest_inventory.json` does, inside the existing wasm job. The budget figure is the owner's.

### REV#72: No itch.io publishing guide and no test of a page in a cross-origin iframe

<a id="rev72"></a>
*owner's decision · DECISION · could · M*

Confirmed. itch.io appears nowhere under `website/src`, `TODO.md` or `docs/product/BACKLOG.md`; `docs/beginner-guides.md:773` listed it as one candidate host and closed with the GitHub Pages guide. No test, tool or guide mentions an iframe, while `engine/app/web/window.cpp:352-355` and `:416-418` anticipate "an iframe without permission". `your-web-page.mdx:118` and `:142` already stage the page as `index.html` with five files, so the upload shape exists and no rename problem remains. itch's published limits were read from its HTML5 page: `index.html` required, 1,000 files, 500 MB extracted, `.br` files served with Brotli encoding. `tools/web_bundle/bundle.mjs:110-114` computes Brotli sizes and writes no `.br` files. Storage partitioning and focus inside a frame are general knowledge, not observed here. The guide fits the exhaustive-guide decision, but the outside adopter is a chosen persona without observed demand and verification needs the owner's itch account. The iframe page test has value for any embed; it and a bundle target add ctest and CI surface. Guide alone is S.

### REV#92: No guidance on stepping through C++ in the browser build

<a id="rev92"></a>
*low priority · DECISION · could · S*

Partly confirmed, at low priority. Nothing in `docs/`, `website/src/content` or `cmake/` mentions source maps, DWARF, Chrome's C/C++ DevTools support or stepping through C++ in a browser, and where `printf` and `stderr` output goes is not stated. The claim that a blank canvas has no next step is refuted: `website/src/content/docs/docs/guides/browser.mdx:251` says start-up errors reach the console and the page, `:633-644` documents `Module.onError` and the blank canvas after context loss, `:781` tells the reader to press F12, and `troubleshooting.mdx:581-584`, `:759` and `:799` cover browser failure messages. Those cases closed under OCA-24 (G112) and BDP-15 (G82), `docs/product/BACKLOG.md:2582-2588`. Step debugging is not recorded. The debug module's size was not re-measured, and no `-g` flag is set explicitly in `cmake/`. The engine's own teaching is that rules are debugged natively (`troubleshooting.mdx:685`), the cheaper answer for a near-beginner, so a short section fits the exhaustive-guide decision; adding it is a scope call.

### REV#93: Escape in browser full screen is unobserved and undocumented

<a id="rev93"></a>
*low priority · DECISION · could · XS*

Confirmed, at low priority. `docs/port/web-as-built.md:81-83` records that headless Chrome does not leave full screen on a dispatched Escape, so Escape itself is a person's check. `website/src/content/docs/docs/guides/your-window.mdx:448` and `:687` say F11 in a browser was not checked and `:700` that leaving full screen was not tried. No guide says what a player sees when the browser takes Escape to leave full screen while a game binds it to pause or quit. The engine half, the fullscreen flag following the browser, was checked in headless Chrome per the same record. No sample page enters full screen by default (`your-window.mdx:290`), so today the collision arises only in an adopter's game that calls `set_fullscreen` on the web. No `TODO.md` row exists. Bindings are the game's (T1), so this is an observation and a sentence, not mechanism; Keyboard Lock is Chromium-only page code and is not added. It belongs with the owner's informal browser play, not a checklist.

### REV#102: Unmeasured size options for wasm-release: Closure, emmalloc, -Os/-Oz and LTO

<a id="rev102"></a>
*low priority · DEFERRED · could · S*

`cmake/settings.cmake:85-92` passes only `-fwasm-exceptions`, `-sSTACK_SIZE=1048576` and `-sALLOW_MEMORY_GROWTH=1`; `engine/CMakeLists.txt` adds the WebGL version bounds and `samples/linesweeper/CMakeLists.txt:166-170` the embed, `ENVIRONMENT=web`, `MODULARIZE`, `EXPORT_ES6` and `EXPORT_NAME`. No CMake file, preset or `docs/port/` document mentions Closure, a malloc choice, `-Os`, `-Oz`, LTO or `wasm-opt`, so none has been tried or recorded. The loader is 120,764 bytes raw and 27,837 under Brotli, about a sixth of the 176 KB play download, not a fifth. Nothing here is a defect: it is an experiment list whose outcome is unknown. Closure carries a real risk against the `EM_JS` bodies and the `Module.*` page contract, a flag change in `cmake/settings.cmake` is consumer-visible under the CHANGES.md rule, and emmalloc requires the null fan-out and particle benchmarks to be run again. It fits the constrained-hardware position and adds no API. It is best taken with or after the size budget of `REV#53`, so the measured deltas have somewhere to be held.

## Project and process

### REV#28: Hand-written counts in the agent brief have a machine source and no check

<a id="rev28"></a>
*low priority · OPEN · could · M*

The measurements reproduce. Tracked `.md` files plus `docs/` outside `website/` come to 148,623 lines against 43,726 lines of engine `.h` and `.cpp`; `docs/review/round-2/all-findings.md` is 48,758 lines and `docs/product/BACKLOG.md` 2,744. Since 2026-10-01 the tally is 1,357 file touches for document paths and 1,177 for `engine/` paths; these count file paths, not commits, and the engine figure is 1,177, not the 1,159 claimed. `docs/code-hygiene.md:21` already states that citation lint checks names, not truth. Nothing generates or lints the ctest counts in `AGENTS.md`'s preset table: `cmake/check_agent_brief.cmake` checks only CLAUDE.md's first line. That part holds, has a present reader in every agent thread, and fits the tree's lint habit. No false statement is shown by this finding itself. Moving historical reviews out of the tree is not proposed: `docs/review/INDEX.md` and the GAP-REGISTER ids that BACKLOG and STORIES cite would break, and AGENTS.md keeps reviews in-tree as history. Nearest records are SM-43 and `docs/2026-10-26-agent-brief-history.md`.

### REV#29: The engine comparison is unchecked against sources and omits SFML, which the site compares

<a id="rev29"></a>
*low priority · OPEN · could · S*

`docs/survey/2026-10-27.md:8-15` names its set as raylib, SDL3, MonoGame/FNA, LÖVE, Godot 4, Defold and Bevy, and says the other engines' side was written from knowledge and not re-checked, so its counts are approximate. `docs/2026-11-04-decision-recommendations.md:13-15` repeats that caveat for SDL3, GameInput and stb. Only raylib and SDL3 in the set are C or C++ code-first. The survey names no SFML, Allegro, sokol, Cute Framework, Orx or olcPixelGameEngine, yet `website/src/content/docs/why.mdx:178` compares "With raylib, SDL and SFML" and `docs/product/PERSONAS.md:137` names SFML, so the public page weighs a framework the survey never examined. Nothing is false today, because the limit is stated in both documents; this is a quality-of-evidence improvement with no existing row. That those frameworks would surface new items is the reviewer's inference and was not checked. A new dated survey file, not an edit of the dated one, fits "Reading records". Its reader is the owner deciding the `GAP#` rows that lean on the "Of 7" counts.

### REV#54: GitHub's own traffic and download counters are not recorded at each release

<a id="rev54"></a>
*worth a story · DECISION · should · XS*

G101 (`docs/product/GAP-REGISTER.md:1893`) and the exclusion at `docs/product/PERSONAS.md:611` close site analytics by accepting the gap; neither considered GitHub's own counters, which need no tracking. They were readable on 2026-11-10: 119 views from 8 unique visitors, 4,858 clones from 463 unique cloners, 1 download of the starter ZIP and 101 of the documentation tarball. The traffic API retains 14 days, so the series is lost unless written down. `docs/product/PERSONAS.md:90` already records stars, issues and forks by hand, so the practice exists informally, but `CONTRIBUTING.md`'s release procedure has no step for it. Three limits apply. The traffic endpoints need push access, so this is an owner step and cannot be a pull-request check. Clone counts and the tarball's downloads are dominated by CI and the website build, so only views, unique visitors and the starter ZIP's count mean anything. With 8 unique visitors the series dates an absence and steers nothing yet. The reader is the owner at a backlog re-plan. No story exists.

### REV#73: No picture on the repository front page or in link previews, and no announcement

<a id="rev73"></a>
*owner's decision · DECISION · could · S*

Read with `gh` on 2026-11-10: 0 stars, 0 forks and 0 watchers; 119 views from 8 unique visitors over 14 days; the v0.3.0 starter ZIP has 1 download; all four open issues (#122 to #125) are the owner's. `README.md` has no image or badge, though `README.md:33` links the live demo. `website/public` holds only `favicon.svg`, and nothing in `website/astro.config.ts` or `website/src` sets an `og:image`. The site's home page does carry LineSweeper captures (`website/src/content/docs/index.mdx:24-44`), so the missing picture is on the repository front page and in link previews, not on the site. `docs/product/PERSONAS.md:82-90` records the same zero counts and that the persona is unevidenced; the link to the client is `DOC#4`, already recommended. A README image and an `og:image` fit the website plan's showcase rule. An announcement or listing sits close to the "marketing push" that `docs/product/PERSONAS.md:610` excludes for now, so it is the owner's call. Comparable projects' figures were not verified.

### REV#74: No stated release rhythm, and no per-module stability marking for adopters

<a id="rev74"></a>
*owner's decision · DECISION · could · S*

Tags are v0.1.0 on 2026-10-13, v0.2.0 on 2026-10-26 (its `CHANGES.md` heading says 2026-10-25) and v0.3.0 on 2026-11-03. `CHANGES.md` has 34 entries under v0.3.0 (line 322) and 30 under Unreleased (line 81) a week later. `docs/design/PHILOSOPHY.md:791-810` says a release before 1.0 promises a record, not compatibility, that a tag is the batch for breaking changes, and gives the settle criterion of twelve weeks and two 0.x tags with no entry. The reference pages under `website/src/content/docs/docs/reference/` carry no stable or provisional marking. This is not a defect: the design declines compatibility before 1.0 and publishes the one signal it believes in, recorded by OCA-67 and OCA-69, with SM-36 holding the pin cadence. Much of Unreleased comes from the one-off hygiene review, so the weekly rate is not a steady state. A hand-kept stability map would be a new promise the record does not back, and `CHANGES.md` entries are not attributable to paths. A stated rhythm is compatible with the design and is the owner's policy call. The count of source-breaking entries was not verified.

### REV#75: No interim desktop frame-time figure is published, and no sprite-count stress sample exists

<a id="rev75"></a>
*owner's decision · DECISION · could · S*

`website/src/content/docs/why.mdx:211-214` says Labrador publishes no frame-time figures yet and that the named low tier is unmeasured. `docs/website-plan.md:38` and `:218` require only that a figure name its workload, hardware, configuration and measurement. Desktop and EC2 `LineSweeperFrameBench` records exist in `docs/performance/2026-09-19-g6f-ratchet.md` (lines 178-179, for example). SM-07, with G24, owns the low-tier figure and is blocked on SM-01; no interim figure is recorded anywhere. The original claim underweights why the page is silent. PHILOSOPHY's Performance section says every figure in the tree is a property of a fast desktop and that no number is a floor until low-tier evidence exists. A desktop figure is evidence for the wrong claim under the constrained-hardware positioning, invites the pacing-for-work misreading the ratchet document warns of, and measures one 9,600-particle view, not general throughput. The website plan permits publishing; whether it is wanted is the owner's call. The stress sample has no client (T1); `bench/render_bench.cpp` measures sprite arithmetic only, and no comparable was read.

### REV#76: No public page says how AI agents are used or what is expected of AI-assisted contributions

<a id="rev76"></a>
*owner's decision · DECISION · could · XS*

Since 2026-10-11, 54 of 419 commits have Claude as author and 365 the owner, and 288 of the 419 messages carry a Claude co-author trailer, so roughly two thirds of public-era commits name an agent; the original count of 54 understates it. `AGENTS.md` and `CLAUDE.md` sit at the root, and `CONTRIBUTING.md:3` and `:471-478` describe the agent brief. `README.md`, `website/src/content/docs/index.mdx` and `why.mdx` do not mention agents. The only written acknowledgement is in `docs/product/PERSONAS.md`, which says one maintainer works largely through agents. PHILOSOPHY's T9 says the author understands the code completely and can defend it line by line. Bevy's CONTRIBUTING links an AI usage policy, checked through the GitHub API. Nothing public is false, so whether silence is a problem is judgement. A plain statement fits PERSONAS' rule that public pages claim only what the tree backs. How T9 reads against that history is the owner's statement to make: an agent-drafted claim of line-by-line understanding could not be verified from the tree. No row or story covers this.

### REV#94: CONTRIBUTING does not say whether outside pull requests are wanted

<a id="rev94"></a>
*low priority · DECISION · could · S*

Read with `gh`: the community profile scores 85 with no code of conduct, Discussions is off, the wiki is on, the default "good first issue" and "help wanted" labels exist and none of the four open issues carries them, and `.github` has no `FUNDING.yml`. `CONTRIBUTING.md:3` addresses the owner and every agent; line 119 says a fork's workflows wait for the owner's approval, and line 33 promises no answer time. No sentence says whether outside pull requests are wanted. The original claim overstates the silence: CONTRIBUTING's first section, from OCA-72, is written for outsiders and states the exhibit rule and that requests may be declined. It also omits the for-now exclusion of community infrastructure at `docs/product/PERSONAS.md:610`, and `docs/product/STORIES.md:1476` already leaves Discussions as an owner settings choice. A one-paragraph statement on outside pull requests is plain and cheap, and its content is the owner's policy. The rest has no present caller: first issues for a contributor who does not exist are speculative under T1. Comparable projects were not verified.

## Evidence added to rows TODO.md already had

No new row: the row named carries the item, and its clause was amended where the evidence changes what it should say.

### The client has built its own equivalents of REV#9, REV#10 and GAP#6

`TODO.md:96-97` mark `REV#9` and `REV#10` "T1: waits for a client that needs it", and `TODO.md:126` lists `GAP#6` as DECISION. The client has since built its own equivalent of each. None was filed as a report, and none shows a forced workaround. `REV#10` is already answered: `docs/review/colourwars-reports/loop.md:368-370` and `:419-421` kept the overlay with the client and shipped only `Application::frame_timings()` (PI-19), and the row says so. For `GAP#6`, `docs/2026-11-04-decision-recommendations.md:143-156` already recommends a copyable outline component; the client's overlay supports that answer and does not change it. For `REV#9`, what a game's own replay records is game policy under T1; `engine/app/application.cpp` (about lines 194-196) still fixes the step with no pause or step control. Under T1 a client that built the thing itself without a forced workaround has no standing in the reports ledger; only `REV#9`'s row gains a fact.

### IDEA#8's prerequisite has landed, and its stated evidence understates the work

`docs/2026-11-04-decision-recommendations.md:113-128` defers `IDEA#8` until `IDEA#7` lands. `IDEA#7` landed as OCA-80 (`docs/product/BACKLOG.md:1103`, ticked), and `TODO.md:149` does not yet say the condition is met. The slice at `docs/feature-candidates.md:197-222` asks for the hit point and the distance along the segment, but its present-evidence sentence understates the work. In `engine/math/intersects.h` every segment-against-shape predicate returns bool; only `segments_intersect(a, b, float& t, Point2F& p)` reports where. In `engine/math/ericson_math.h`, `test_segment_AABB` and `test_segment_convex_polygon` return bool; the latter computes its entry and exit parameters at `engine/math/ericson_math.cpp:361-413` and discards them. `engine/collision/broad_phase.h` exposes a cell size, a large-object count and a pairs buffer; its index is rebuilt privately on each `find_pairs`, so there is no region query to reuse. Size M may be low. A hit normal is not part of `IDEA#8`. The public-face argument stands and T3 is intact; no client has filed for it, so it stays the owner's selection.

### IDEA#11, WAV and in-memory sounds: its stated gate has been met

Verified: `docs/2026-11-04-decision-recommendations.md:30` and `:318-331` recommend IDEA#11's option A as could, M, after GAP#4. GAP#4 is closed as OCA-82 (`docs/product/BACKLOG.md:1107` ticked; `TODO.md:36`). `TODO.md:151` still lists IDEA#11 as a decision with no story. `website/src/content/docs/docs/get-started/your-sound.mdx` is 984 lines, the longest file under the guides and get-started folders. It requires xwbtool with `-nc` (`:478`, `:498`, error entry at `:850-853`) and an FFmpeg install for compressed sound and for 24-bit or 32-bit-float WAV (`:654-676`). No engine header has a RIFF or WAV reader.

Correction: moving the grade from could to should is a judgment, not a finding; the record's grade is could. The record is IDEA#11 (`docs/feature-candidates.md:45,272`). It fits as recorded (T9, a seam verb with a null implementation, T6 refusals), and every starter copy is a caller. Taking it and its grade are the owner's selection. Comparables were not re-fetched.

### REV#9, input record and replay: pad replay needs a scriptable pad seam first

Verified: REV#9 is deferred on T1 (`TODO.md:96`). The keyboard is fed through `on_key_down`, `on_key_up` and `on_text` and latched in `poll()` (`engine/input/keyboard.h:155-175`), and Application polls devices once per fixed update (`engine/app/application.h:356-360`). That raylib ships automation-event recording is from knowledge and was not re-fetched.

Corrections: the claim that pads already go through a seam overstates readiness. `GamepadReader` is one concrete, build-selected class with a private implementation and no scriptable form, which is what open issue #122 reports. Pad replay therefore needs #122's seam first. No new client demand for replay is shown; a comparable having a feature is not a T1 client, and the claim that smoke tests would shrink is speculative. The recorded deferral stands, and the online-play non-goal is untouched. The one new fact for the row is the pad prerequisite.

### OCA-04: two of three revisit conditions for hosting more samples are met

OCA-04 sits in Won't this time (`docs/product/BACKLOG.md:2673,2684`), to be revisited after SM-12's checks, SM-27's verify and a recorded request for a hosted second sample. SM-12 (`docs/product/BACKLOG.md:95`) and SM-27 (`:595`) are ticked; no request is recorded. `docs/website-plan.md:92` says the site does not need browser-playable demos, a standing decision any change must amend. The original claims overstate what is paid. CI's Firefox has no WebGL2 and runs only `FirefoxWebWindowTests`; Chromium runs the six sample pages and the `safari` job runs them from the wasm artifacts (`.github/workflows/ci.yml:727,751-784`). Deploys, a click-to-load host page for each sample and per-deploy checks are not paid, and a web ZIP on the release is not playable from `file://`. Samples are 438 to 516 KB each (`docs/port/web-to-be-tested.md:458-462`). No engine change is involved; it is a scope and deploy-cost call. TODO.md gives Won't this time no row, so the evidence belongs with the backlog's trigger.

## Not taken

Refuted, settled by a recent decision, or already recorded with nothing new to add. No row.

### Screenshot to a PNG file

Already settled: a screenshot to a file is in the Not-now list of `docs/survey/2026-10-27.md:61-64`, each item waiting for a client under T1, and neither proposal found a game-side caller. The enabling facts are accurate: lodepng is linked into the engine (`engine/CMakeLists.txt:416-420`), `engine/render/png_file.cpp` only decodes, `engine/render/renderer.h:434-452` documents the capture recipe and `read_back_buffer`, and a lodepng writer exists only in the test harness (`tests/render/golden_image.cpp:214-223`).

One claim needs qualifying. The writer in `tools/linesweeper_capture/png_file.cpp:65-79` uses the Windows Imaging Component, so it is not portable to a browser build; sharing one writer means replacing it in a tool whose README owns repeatability, a larger change than stated. Null read-back throws, so the feature is not headless-testable end to end. T9 no longer objects; if a client appears, a `samples/components` piece is the conservative form.

### Math helpers beyond IDEA#19: reflect, move-towards, length clamp, remap, degrees

`engine/math/scalar.h` has clamp, `are_equal`, lerp and constants; `engine/math/vector2f.h` has a static lerp and no reflect, min, max, clamp or move-towards; `engine/math/resolve.h` has `separation`, `separation_along` and `slide`. `IDEA#19` (`docs/feature-candidates.md:422-437`) covers only `RectangleF::intersection_of` and two angle helpers, and is recommended for deferral. No sample hand-rolls a reflection or bounce. The proposal overlooked a settled decision: `CHANGES.md:119-121` records the removal on 2026-11-05 of `Vector2F::clamp`, `Vector2F::clamped` and other members because nothing called or tested them. Adding component-wise clamp and its relatives reverses that without a caller, and T1 asks for a client first. A reflect helper could ride a sample that bounces a circle, when one exists. Dropped.

### A countdown or cooldown value type

`engine/core` has no timer or cooldown type; `engine/core/step_timer.h` is the shell's clock. A search of the samples finds no accumulate-and-compare timer. The candidate cited only client-side code. The client has filed no report for this, and nothing forces a workaround. The adjacent rows are `GAP#8` (`TODO.md:128`) and `IDEA#19`, recommended for decline and deferral at `docs/2026-11-04-decision-recommendations.md:429-438` for want of in-tree copies. What a timer means is game policy under T1, and client-only copies without a forced workaround or a filed report have no standing in the reports ledger. It rides or falls with the owner's `GAP#8` call. Dropped.

### Scene has no liveness query for the pointers add() returns

`engine/scene/scene.h` has no `contains()` or `will_retire()`. The comment on `add()` at `:113` says the pointer stops being valid when the object leaves the scene, and `remove()` throws for a pointer the scene does not hold (`:162-165`). The end of the tick (`engine/scene/scene.cpp`, about lines 154-175) flags out-of-bounds collision objects and then erases those whose `for_deletion()` is true. `for_deletion()` and `in_bounds()` are public, so the retire set can be stated from them, and `engine/scene/scene.h:194-198` and `:204-210` already describe bounds retirement. No report has been filed for a liveness query and the client has no forced workaround, so T1 is not met. What remains is one explicit header sentence naming the retire set, a nicety and not an API need. A `contains()` walk would also need T8 wording like `remove()`'s. Dropped.

### GAP#2: the browser's own decoder and the memory cost of PCM music

Both points are already in the record. `docs/2026-11-04-decision-recommendations.md:340-342` gives option B as a decoder bought natively and the browser's own media element for the web, so the browser third needs no bought decoder; `:70-73` and `:335-336` carry the 30 and 60 MiB figures. `website/src/content/docs/docs/guides/audio.mdx:285-290` states 60.6 MiB of float buffers in addition to the bank file in Emscripten's in-memory filesystem. The code matches: `engine/audio/webaudio/audio_device.cpp:285-300` copies into audio buffers.

Correction: the in-memory copy is not specific to embedding; a preloaded file lands in the same filesystem, as the guide's wording covers. The one new item is a comparable about latency, which does not bear on this decision. The record is GAP#2 (`TODO.md:123`, `docs/survey/2026-10-27.md:46`, recommendations `:333-349`) and OCA-40. The call stays the owner's and still needs a client with music (T1).

### GAP#5 and GAP#6 read together as a route to third-party debug UI

Verified: `DrawList`'s recording surface is `set_viewport`, `set_camera`, `set_filter`, `draw_sprite` and `draw_text` (`engine/render/renderer.h:129-232`), with no clip rectangle or triangle path. No document in the repository mentions an immediate-mode UI library. GAP#5 and GAP#6 are recorded (`TODO.md:125-126`); the recommendations defer GAP#5 until a client scrolls a list (`docs/2026-11-04-decision-recommendations.md:440-444`) and choose GAP#6's option B, a copyable line-quad debug component with no engine change, rejecting new geometry against T8's one sprite pipeline (`:143-158`).

Correction: the claim that none of the named consumers can be written outside the engine is false. Collision outlines are recommended as a component from line quads, and a frame-time readout needs only `Label` and `frame_timings()`. Only a bought immediate-mode UI library needs a triangle list. This is a new framing, not evidence of a client (T1), and the recorded T8 objection stands. REV#10 (`TODO.md:97`) is adjacent.

### A cross-platform .spritefont baking tool (GAP#12 option B)

Already recorded with no new evidence. `docs/2026-11-04-decision-recommendations.md`, under `GAP#12` (`TODO.md:132`), records option B, a command-line tool that bakes a `.spritefont` from a TTF on any platform, as the revisit path once MakeSpriteFont is the beginner's last downloaded tool after `GAP#4` and `IDEA#11` land. `GAP#4` has landed as OCA-82 (`docs/product/BACKLOG.md:1107`); `IDEA#11` is still a `DECISION` row (`TODO.md:151`), so one of two conditions holds. The guide facts are accurate: `website/src/content/docs/docs/guides/your-font.mdx:38-39` and `:293-295`. The silent box is a third-party tool's behaviour that the guide documents, not an engine T6 violation, and the tool would need a bought rasteriser.

A side observation outside this candidate: `website/src/content/docs/docs/guides/content-tools.mdx:10` still says Labrador does not load PNG pictures directly, while `samples/minimal/README.md:52-55` records that the engine reads PNG textures. That sentence is `REV#17`.

### GAP#1: the browser build reads pads the native XInput build does not

The asymmetry is real. `engine/input/web/standard_gamepad.h:18-23` accepts any pad the browser maps to the W3C standard layout and reports others absent, and `website/src/content/docs/docs/guides/input.mdx:257` says that on Windows only XInput-compatible controllers appear. The claim that the record omits this is wrong for the recommendation: `docs/2026-11-04-decision-recommendations.md:263-265`, under GAP#1, says XInput reads only Xbox-style pads and that the browser backend already reads any pad the Gamepad API maps. It is absent only from the one-line row at `TODO.md:122`, which is an index and does not carry reasons. The recommendation is already to buy SDL3's gamepad layer, gated on the owner having a non-Xbox pad, and that gate is unchanged. The one new thought, using the same pad's browser readings as an acceptance oracle for the native work, is a note for the eventual story and not evidence for the decision. It fits T9 as recorded. Nothing is added to `GAP#1`.
