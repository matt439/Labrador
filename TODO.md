# TODO: open work

An index of open work, one row per item; each row points to the document that owns the detail. The rules for editing it are at the end, in [How to read this file](#how-to-read-this-file). A fresh thread starts here: read **NOW**, then the owner of the row it picks up. The order of stories and their horizons is [docs/product/BACKLOG.md](docs/product/BACKLOG.md)'s, the one file in `docs/product/` kept current, and this file does not change it.

## NOW

**The only ordering in this file.** Keep every `DECISION` row here. Keep this block to the current aim and the next few actions; results go in their owners, and a closed story is a ticked box in BACKLOG.md.

**Now is finished**: every batch of [BACKLOG.md, Now](docs/product/BACKLOG.md#now), N1 to N21, has landed, and `v0.1.0`, `v0.2.0` and `v0.3.0` are cut.

**Aim:** finish Next's remaining acceptance while taking ready Later work in
parallel, as [BACKLOG.md's scheduling amendment](docs/product/BACKLOG.md#later)
allows. Next's hardware and release gates remain open. Apart from the
re-planned rows below, what remains in Later waits on the owner, on another
story or on an outside event.

Otherwise follow Later's order and each story's prerequisites; an open hardware
gate does not block unrelated work.
**Hygiene has a separate lane:** [Code hygiene](docs/code-hygiene.md) owns it;
BACKLOG's L36 to L54 let it run alongside product work.

**Beginner guides are done:** every `GUIDE#` unit of
[Beginner guides](docs/beginner-guides.md) has closed. What the runs could not
check is in its *Not verified by the audit*; `GUIDE#12` left saving through
IDBFS from the starter unguided, and `IDEA#4`, which *A camera that follows
the player* links, waits on the owner.

**Owner checklists become automation** (decided 2026-11-03, [BACKLOG](docs/product/BACKLOG.md#decisions-only-the-owner-can-make)): a listable check is an automated suite; the owner's part is informal play on real hardware. Each owner row below is re-planned that way when it is taken up; `SITE#3`'s selector checks were the first, now [website/acceptance/](website/acceptance/) in CI.

**Open agent rows:** `REV#104`, a verified defect in how the Direct3D backends classify a software adapter (issue #125); `REV#106` to `REV#117` of [the first-run review](#first-run-review-of-2026-11-10), `REV#106` to `REV#108` first; the `OPEN` rows of [the review of 2026-11-10](#review-of-2026-11-10) and `OCA-76`, each a measurement, reproduction or record fix that comes before the owner's call; and the `OPEN` rows of [the work audit](#work-audit-of-2026-11-12), `REV#122` to `REV#159`. Later and the hygiene lane have no ready item ([the audit](docs/review/2026-11-12-work-audit/README.md)).

**Decisions and sessions only the owner can do.** Every `DECISION` row below, in the order of the backlog's horizons:
- Next: `OCA-48` + `BDP-06` (informal play with the physical pad, which can also feel `PI-21`'s vibration, play `PI-14`'s pad match and close `OCA-45`) · `SM-09` (informal play on the current MacBook Air) · `SM-01` (re-enable the Radeon in the BIOS at the next reboot, install AMD's driver, then the bench's adapter check) · `PI-14` (close as delegated to ColourWars, or wait for its `v0.3.0` run).
- Later: `SM-14` (try the LAN-serve draft with the iPhone, then `SM-13`), `WEB#3` (listen to the audio sample), `OCA-66` (look at the template at 150% scaling), `BDP-07` (judge judder at 60 Hz and at 144 Hz), `OCA-79` (authorise the starter project's first push to GitHub), `SM-30` (set Cloudflare's build command to `npm ci && npm run gate` and delete its `NODE_VERSION` variable).
- Other acceptance: `REV#3` (display change), `REV#8` (audio endpoint loss/recovery), `WEB#12` (run the live-demo check after the next deploy), `SITE#4` (search on the iPhone after the next deploy).
- Not yet placed: `GAP#1` to `GAP#13` except `GAP#4` (OCA-82), whether to take up each feature the engine comparison found; a yes becomes a story in BACKLOG.md.
- [Recommendations of 2026-11-04](docs/2026-11-04-decision-recommendations.md) give options and one verdict for every `GAP#` and `IDEA#` row below; the calls stay the owner's.
- Review of 2026-11-10: every `DECISION` row of [that section](#review-of-2026-11-10), 46 between `REV#30` and `REV#94`: whether to take up each gap [the review](docs/review/2026-11-10/README.md) verified, stories and owner's calls first, then low priority; a yes becomes a story in BACKLOG.md. Fifteen more rows there and `OCA-76`, with `REV#121` of the first-run review, take the owner's call after their `OPEN` first step.
- Work audit of 2026-11-12: `REV#130`, `REV#133`, `REV#136`, `REV#142`, `REV#147` and `REV#152` in [that section](#work-audit-of-2026-11-12).
- First-run review of 2026-11-10: `REV#118` to `REV#121` in [that section](#first-run-review-of-2026-11-10), and its run evidence for `REV#62`, `REV#63` and `REV#66`.
- Client reports: `REV#105`, the verdicts on issues #122 to #124 in [the reports table](docs/review/colourwars-reports/README.md).
- New proposals: `IDEA#1` to `IDEA#5` in [Feature candidates](docs/feature-candidates.md): typed text, rebinding, save recovery, camera following and grid-sheet authoring. `IDEA#8` to `IDEA#19`, except `IDEA#10`, from the engine review of 2026-10-31: scene queries, contact edges, WAV and in-memory sounds, named-frame animations, a focus hit test, the present interval, the refresh rate, title and icon, cursor shapes, a side-by-side split and small math helpers.

## Input, menus, settings and saves

**Owner:** [STORIES.md](docs/product/STORIES.md); order in [BACKLOG.md](docs/product/BACKLOG.md), Next X13/X14 and Later L19/L20.

| id | status | pri | size | item |
|---|---|---|---|---|
| `OCA-45` | DECISION | should | S | Headless coverage landed (2026-11-04): minimal's `combine_pads` rule and its MinimalTests case for a pad alone in slot 1 or 3 and slot 0 emptied. Left: the owner's informal one-pad play, in `OCA-48`'s session; the unplug-slot-0 path stays unseen on hardware. [Decision](docs/product/BACKLOG.md#next). |
| `PI-21` | DECISION | should | S | Browser-transport test landed 2026-11-04 (`dual-rumble` played, reset on stop, suspension and teardown, against a fake actuator under Node). Left: the owner's informal pad play, natively and in Chrome, in `OCA-48`'s session. [Re-plan](docs/review/colourwars-reports/vibration.md#acceptance-and-evidence). |

## Website and the hosted demo

**Owner:** [website/README.md](website/README.md) and [docs/website-plan.md](docs/website-plan.md); the hosted demo in [docs/port/web.md](docs/port/web.md) section 7 and [docs/port/web-to-be-tested.md](docs/port/web-to-be-tested.md).

| id | status | pri | size | item |
|---|---|---|---|---|
| `SITE#4` | DECISION | should | S | The search form no longer has Pagefind's `javascript:` action; acceptance check 15 holds it. The Simulator never reproduced the empty page. Left: the owner's informal search on the iPhone after the next deploy. |
| `OCA-79` | DECISION | should | S | Push a current-release starter project, as *Put your project on GitHub* says, to a public test repository under the owner's account (owner's authorisation), and record its green `ci.yml` run in the guides. [Story](docs/product/BACKLOG.md#oca-79). |
| `WEB#12` | DECISION | could | S | `check_live.mjs` passes against the host's Worker on loopback; cloud sessions cannot reach the site. Left: run it against the live demo after the next deploy and keep its dated output in [M5](docs/port/web-to-be-tested.md) in place of the manual ticks. |
| `SM-30` | DECISION | could | S | `npm run gate` and `website/.node-version` landed; CI runs both. Left: the owner sets Cloudflare's build command to `npm ci && npm run gate` and deletes `NODE_VERSION`; the next production log showing Node 24 and the gate closes it. [Story](docs/product/BACKLOG.md#later). |

## Performance

**Owner:** [docs/review/backend-equivalence-2/STATUS.md](docs/review/backend-equivalence-2/STATUS.md)'s low-tier box; [the measurement decision](docs/performance/2026-09-19-g6f-ratchet.md#measurement-decision-sm-03) assigns each claim its evidence. [The workload/cadence record](docs/performance/2026-10-26-workload-and-cadence.md) owns the completed PERF#1 to PERF#3 corrections; read the ratchet document before the hardware tables.

| id | status | pri | size | item |
|---|---|---|---|---|
| `SM-01` | DECISION | should | S | Owner session (decided 2026-11-03, no re-plan): re-enable the integrated Radeon in the BIOS at the next reboot, install AMD's driver, then run the bench's adapter check on it. [SM-01](docs/product/BACKLOG.md#next). |
| `SM-07` | BLOCKED | should | M | Low-tier CPU-phase and cadence p99s plus separate pipeline-latency evidence under the four-core hold (D3D12/Vulkan). Needs `SM-01`'s run on the Radeon; EC2 runs do not count. See the held Vulkan pairing in [the procedure](docs/performance/local-frame-bench.md). |

## Browsers, phones and hardware

**Owner:** [docs/port/web-to-be-tested.md](docs/port/web-to-be-tested.md) for the `WEB#` rows and M3's done-when in [docs/port/web.md](docs/port/web.md) section 9; stories in [docs/product/STORIES.md](docs/product/STORIES.md). The owner's hardware is listed at the top of BACKLOG.md.

| id | status | pri | size | item |
|---|---|---|---|---|
| `OCA-48` | DECISION | should | S | Informal play with the physical XInput pad: an `x64-release` sample, noting whether pad input reached it with another window focused, and the pad in Chrome (`BDP-06`). Mapping is SM-40's test; `BDP-06`'s Firefox half runs as `FirefoxLineSweeperBundlePageTests`. |
| `SM-09` | DECISION | should | S | Ceiling recorded: Safari 27.0.1 on macOS 27.0.1, above the 18.4 floor. Left: the owner's informal play on the Air. CI's `safari` job plays the bundle and the six sample pages in Safari 26.6 ([record](docs/port/web-to-be-tested.md#safari-sample-checks-web1)). |
| `SM-14` | DECISION | should | S | Drafted, untried: [lan-serve.md](docs/port/lan-serve.md) and `tools/lan_serve.py` (LAN binding, secure context, mkcert HTTPS, Web Inspector, Android Chrome). Left: follow it in `SM-13`'s iPhone session, correct it, tick the box. |
| `WEB#3` | DECISION | should | S | Hear the audio sample through speakers (each sound, pan, pitch, volume, held keys), the first-gesture unlock in each browser, resume after a hidden tab, and `ping_adpcm.xwb` on XAudio2. Nobody has listened yet. |
| `BDP-07` | DECISION | could | M | R5, M3's second open term: collect `requestAnimationFrame` intervals by script at 60 Hz (four screens) and 144 Hz (one screen, three unplugged); the owner judges only visible judder. [Method](docs/port/pacing-method.md). |
| `SM-13` | BLOCKED | should | M | Play the development page and the live demo on the owner's iPhone and record layout, rotation, touch, legibility, load time, memory (R4) and the top-out frame (R6). After `SM-14` (serving to a phone on the LAN); R6 by [the method](docs/port/pacing-method.md). |
| `BDP-10` | BLOCKED | could | XS | Lift the bundle page's phone gate after `SM-13`; `BDP-09`'s layout is in place. The owner approved iPhone-only evidence; Android is not a prerequisite. [Decision](docs/product/BACKLOG.md#decisions-only-the-owner-can-make). |
| `SM-44` | DEFERRED | could | S | Optional future real-phone record on a mid-range Android phone in Chrome. Waits on a borrowed phone; none is accessible. Does not gate iPhone acceptance. |
| `WEB#2` | BLOCKED | could | S | A standard-mapping pad in Firefox and Safari (Chrome is `BDP-06`'s), and unplug and replug with more than four pads seen. Firefox can ride `OCA-48`'s session, Safari the owner's play on the Air; five pads are not owned. |
| `WEB#5` | BLOCKED | could | S | Touch on real glass: several fingers with distinct ids, no tap also a click, foreground loss cancelling every point, LineSweeper's buttons through `WM_POINTER`, a pen as the mouse, Firefox and Safari. Needs a touchscreen. |

## Engine verification and reviews

**Owner:** [the review index](docs/review/INDEX.md) maps every review to its live ledger and tracks the remaining D3D12 and Vulkan candidates (`REV#3` to `REV#5`).

| id | status | pri | size | item |
|---|---|---|---|---|
| `REV#8` | DECISION | could | S | Bounded XAudio2 recovery and deterministic policy tests implemented; record real endpoint loss/reappearance, retained-bank playback and suspension/resume. gpt6 opportunity 4. |
| `REV#3` | DECISION | could | M | Check factory refresh in a real display-change session. [STATUS](docs/review/d3d12/STATUS.md) records bounded descriptor-query measurements, profile evidence, allocation reuse and full patterned mip read-back. |
| `REV#12` | BLOCKED | could | M | The 2026-09-26 review's manual checks: real device removal, interactive minimise, restore and drag, physical keyboard layouts and device changes, real sleep and wake. Needs a person at the machine. |
| `REV#5` | DEFERRED | could | S | Whether the Vulkan surface-format fallback, a fixed `compositeAlpha` and an unapplied `preTransform` are defects (Vulkan review 6.2). Live only on Android; reopens with that port. |
| `REV#9` | DEFERRED | could | M | Input recording and replay for LineSweeper, with a rules and file version (gpt6 opportunity 3). T1: waits for a client that needs it, and no report asks for it; pad replay also needs a scriptable `GamepadReader` (issue #122), which `REV#40` proposes. |
| `REV#10` | DEFERRED | could | M | A frame-time overlay: the distribution, sprites submitted, particles dropped (gpt6 opportunity 5). T1: waits for a client that needs it. The measurement under it is `Application::frame_timings()` (PI-19); the overlay itself still waits. |

## ColourWars and the pin

**Owner:** [docs/product/STORIES.md](docs/product/STORIES.md); the drafts in ColourWars' `docs/review/engine-reports.md`, in its own repository. [docs/review/colourwars-reports/](docs/review/colourwars-reports/README.md) records the original fifteen answers, the two newer drafts and the four issues filed on 2026-11-05; its table is kept current.

| id | status | pri | size | item |
|---|---|---|---|---|
| `PI-14` | DECISION | should | M | Sandbox run at `v0.2.0` reached a keyboard match. Radeon criterion: not run, none available; the pad match is informal play. Run and record are ColourWars'. Owner: close as delegated, or await its `v0.3.0` run. [BACKLOG](docs/product/BACKLOG.md#next). |
| `REV#104` | OPEN | should | S | Both Direct3D backends take `RenderDeviceKind` from `DXGI_ADAPTER_FLAG_SOFTWARE` alone, so an unflagged CPU rasteriser reads as hardware (issue #125). Selection using a fix is the owner's. [Record](docs/review/colourwars-reports/device-kind.md). |
| `REV#105` | DECISION | should | S | Verdicts on issues #123 and #124 ([draft](docs/review/colourwars-reports/contact.md)), and whether `REV#40` meets #122, whose tests link the shipping build. [Table](docs/review/colourwars-reports/README.md). |

## Records and design decisions

**Owner:** each row's document; the owner's open decisions are [BACKLOG.md's table](docs/product/BACKLOG.md#decisions-only-the-owner-can-make).

| id | status | pri | size | item |
|---|---|---|---|---|
| `SRL-07` | BLOCKED | could | S | This tree's constants are renamed and CHANGES.md lists the public spellings; migrate the ColourWars callers at its next pin move and record the counted caller repositories. Waits on that pin move. [Scope](docs/product/BACKLOG.md#later). |
| `OCA-66` | DECISION | could | S | DPI declaration implemented, and CI reads it from each sample and the installed Get Started game. Closes when the owner has informally played the template on a screen at 150% scaling. [Re-plan](docs/product/BACKLOG.md#later). |

## Features other engines have

**Owner:** [the engine comparison of 2026-10-27](docs/survey/2026-10-27.md), which holds what each involves and what was left out; [the comparison of 2026-11-10](docs/survey/2026-11-10.md) re-checks its counts against sources and adds six C and C++ frameworks. Every row is the owner's call; a yes becomes a story in [BACKLOG.md](docs/product/BACKLOG.md) before the work starts.

| id | status | pri | size | item |
|---|---|---|---|---|
| `GAP#1` | DECISION | should | L | PlayStation, Switch and generic pads on Windows, and more than four: built on Windows.Gaming.Input or GameInput, or SDL's gamepad layer bought (T9). |
| `GAP#2` | DECISION | should | L | Streamed, compressed music (Ogg or MP3) on XAudio2, Web Audio and null. `OCA-40` has stated what PCM music costs; the 2026-11-04 recommendation is to defer until a client ships a track. |
| `GAP#3` | DECISION | could | L | Render to texture: integer-scaled pixel art, transitions, minimaps; five backends and new goldens. |
| `GAP#5` | DECISION | could | M | A clip rectangle inside a view, for scrolling lists and panels. |
| `GAP#6` | DECISION | could | M | Circles, polygons and debug drawing of collision shapes; rectangles and lines already draw from `engine:white`. Its gate, circle collision (`IDEA#7`, OCA-80), has landed; the recommended slice is debug drawing. |
| `GAP#7` | DECISION | could | M | Let a game make or update a texture from pixels at run time; `resource_factory.h` already has the backend half. |
| `GAP#8` | DECISION | could | S | Easing functions. The premise is false: neither LineSweeper nor ColourWars hand-rolls an easing curve (checked 2026-11-12), and the 2026-11-04 recommendation declines it (T1). |
| `GAP#9` | DECISION | could | M | Camera rotation, with culling's visible rectangle grown to cover it. |
| `GAP#10` | DECISION | could | S | LineSweeper's particle field as a component, if a second client would copy it. |
| `GAP#11` | DECISION | could | M | Tilemaps and Tiled import; waits for a client that uses tiles. |
| `GAP#12` | DECISION | could | L | TrueType and OpenType fonts. `OCA-43` has closed, so there is nothing to fold it into; a yes is a story of its own. |
| `GAP#13` | DECISION | could | M | Relative mouse motion: Win32 raw input, and a browser pointer lock granted only inside an input event. |

## New feature proposals

**Owner:** [Feature candidates](docs/feature-candidates.md), with current-code
evidence, bounded scope and proposed acceptance. Selection creates or amends
a story in BACKLOG.md before implementation; these are separate from the
ready queue above.

| id | status | pri | size | item |
|---|---|---|---|---|
| `IDEA#1` | DECISION | could | M | A copyable single-line typed-text field using Keyboard::typed and documented font limits. |
| `IDEA#2` | DECISION | could | M | A game-owned keyboard rebinding screen with capture, cancel, reset and duplicate handling. |
| `IDEA#3` | DECISION | could | M | A versioned save/recovery example that preserves prior data on write or migration failure. |
| `IDEA#4` | DECISION | could | S | A dead-zone/bounded camera-follow component in the local-multiplayer sample. |
| `IDEA#5` | DECISION | could | S | A deterministic grid-sheet JSON authoring command using the current asset schema. |
| `IDEA#8` | DECISION | could | M | Scene queries: the first collision object along a segment, and every object overlapping a rectangle, mask-filtered and allocation-free. Its gate, IDEA#7, has landed (OCA-80). Segment hits need math that reports where, and BroadPhase has no region query, so M may be low. |
| `IDEA#9` | DECISION | could | S | A contact begin/continue/end helper over two ticks' contact lists; the collision sample derives trigger entry by hand. Needs a second client. |
| `IDEA#11` | DECISION | could | M | Sounds from WAV files and from in-memory PCM through one new seam verb on all three audio backends, so a first sound needs no XWBTool. The recommended gate, `GAP#4` (OCA-82), has landed; selection is the owner's. |
| `IDEA#12` | DECISION | could | M | Animation sequences of named frames with per-frame durations beside today's horizontal, uniformly timed strips. |
| `IDEA#13` | DECISION | could | S | A pointer hit test over a focus group's enabled bounds. Two copies now exist, LineSweeper's pause menu and the components gallery's `hit_target` (OCA-83): the components README's promotion test. The 2026-11-04 decline predates the second. |
| `IDEA#14` | DECISION | could | S | An unfocused render-rate cap. The requested present interval has landed (`ApplicationOptions::vsync`, `Renderer::set_vsync`); what an unfocused, shown window costs has not been measured. |
| `IDEA#15` | DECISION | could | S | The display's refresh rate on RenderDeviceInfo and in the frame bench's record, in place of the pacing method's operator note. |
| `IDEA#16` | DECISION | could | S | A run-time window title, and a window icon named in options, diagnosed when absent, in the template's resource file and the release guide. |
| `IDEA#17` | DECISION | could | S | System cursor shapes (hand, text, crosshair, move) on Win32 and the canvas; today the pointer is the arrow or hidden. |
| `IDEA#18` | DECISION | could | XS | A side-by-side two-player layout in ViewportManager, which stacks the panes today. |
| `IDEA#19` | DECISION | could | XS | RectangleF::intersection_of and shortest-arc angle helpers; the 2026-11-04 recommendation defers them to ride `GAP#5`, whose clip rectangle wants `intersection_of` first. |

## Review of 2026-11-10

**Owner:** [the review of 2026-11-10](docs/review/2026-11-10/README.md): [FINDINGS.md](docs/review/2026-11-10/FINDINGS.md) holds each row's evidence under its id, and [STATUS.md](docs/review/2026-11-10/STATUS.md) records each resolution. A `DECISION` row is the owner's call, and a yes becomes a story in [BACKLOG.md](docs/product/BACKLOG.md) before the work starts. A `DEFERRED` row waits for a measurement or a caller that shows the need.

| id | status | pri | size | item |
|---|---|---|---|---|
| `REV#30` | DECISION | should | S | `sprite_sheet_loader.cpp` checks no frame or strip against the atlas size or for sign and zero area; a refusal rewrites the negative-rectangle cases SM-71 kept, and needs a texture size at the loader. |
| `REV#31` | DECISION | should | S | `RenderResources` has no texture-size accessor although every backend holds the size, so the guides make a game type width and height by hand; a whole-texture scene object is a separate engine-or-component call. |
| `REV#32` | DECISION | should | S | `font.h` measures and wraps but offers no aligned origin for a measured line; LineSweeper centres text by hand at three sites, one of them without the floor the other two apply. |
| `REV#33` | OPEN | could | S | No backend queries the device's texture size limit, and no guide states D3D12's 256-name cap. First step, no call: state that cap and its throw in a guide. Querying the limit, naming it in the GL refusal and a capability field stay the owner's. |
| `REV#34` | DECISION | could | XS | `Colour::set_from_hex` turns any malformed string, including one with a leading `#`, into opaque black; an optional-returning `colour_from_hex` beside `colour_from_name` is additive, a constructor change a source break. |
| `REV#35` | DECISION | should | S | Random (engine/core/random.h) is in LabradorEngine, so a rules library linking only LabradorMath, as minimal's does, cannot use it; it has no shuffle and no way to derive a second stream. Move and extend, or leave. |
| `REV#36` | OPEN | should | S | Vector2F's operators are out of line in `engine/math/vector2f.cpp` and no preset enables IPO or LTO. First step, no call: time LineSweeperFrameBench's update phase with IPO off and on. The owner decides with the number. |
| `REV#37` | OPEN | should | S | No display-sleep hold while a game is shown (`win32/window.cpp`, `web/window.cpp`). First step, no call: run a sample with no input, watch idle time and `powercfg /requests`, and check headless Chrome's wake lock. The pad half needs the owner's pad. |
| `REV#38` | DECISION | should | XS | `Mouse` drops a press and release that both arrive between two polls, where `Touch` keeps the tap; `engine/input/mouse.h` now states the limit as `keyboard.h` does. Whether to latch a click as touch does is the owner's call (a silent behaviour change needing a CHANGES.md entry). |
| `REV#39` | DECISION | should | S | `engine/app/win32/window.cpp` has no `WM_CLOSE` case and `WindowNotify` and `State` have no close-request hook, so the X button and Alt+F4 bypass `samples/minimal`'s ConfirmState; the game can neither ask nor decline. |
| `REV#40` | DECISION | should | M | A `null` pad backend for `LABRADOR_INPUT_BACKEND`: a GamepadReader that answers scripted GamepadStates and records vibration requests, so native tests can drive pads through Gamepads and Application. |
| `REV#41` | DECISION | should | S | The engine has no diagnostic sink: 13 OutputDebugString sites in the Direct3D and Vulkan backends and 5 stderr sites write where a game cannot redirect them; one replaceable function, default destination the owner's call. |
| `REV#42` | DECISION | could | S | Record peak working set, private bytes at exit and process start to first `end_frame` in `bench/linesweeper_frame_result.h`, and print them from `--smoke-test`, before the SM-01 and SM-07 Radeon session. |
| `REV#43` | DECISION | could | S | `samples/components` has no debug readout (frame time from `Application::frame_timings()` plus caller lines) and no guide shows a running value on screen; taking it partly reopens `REV#10`'s deferral. |
| `REV#44` | DECISION | could | M | No sample adds or removes a Scene object during play, and collision.mdx's Projectile snippet is not compiled source; decide between a small spawn-and-retire sample and one retiring object in the collision sample. |
| `REV#45` | DECISION | should | S | `CMakeLists.txt` declares no project VERSION and `engine/core` has no version header, so a consumer cannot test which release it builds against; the value between tags and the release procedure's bump step are the owner's. |
| `REV#46` | DECISION | should | S | `engine/CMakeLists.txt` names no missing vcpkg dependency when the engine is not top level: a consumer manifest lacking one stops at a bare `find_package`. The install helper stays PI-11, parked. |
| `REV#47` | OPEN | could | M | No `wasm-*` preset has been built on a non-Windows host. First step, no call: install emsdk 6.0.12 in this machine's WSL 2 Ubuntu, then configure, build and run the Node ctest, and record it. Any CI job stays the owner's. |
| `REV#48` | DECISION | could | M | `website/src/walkthrough/ci.yml` has no wasm or Pages job; `your-web-page.mdx` publishes by hand into a second repository. A copyable deploy workflow would follow OCA-79 and state the public-repository trade. |
| `REV#49` | OPEN | should | M | ASan and UBSan as a scheduled job is the owner's call; it amends `SECURITY.md:68-71` and the inventory. First step, no call: one local ASan run of `x64-debug-null`, recording whether vcpkg's static lodepng links and what it reports. |
| `REV#50` | DECISION | could | S | Owner call: a `tools/` report over `gh` run data that counts failed job-steps by name and ref, with offline tests; no tool aggregates failures today and `ci.yml:56-63` holds one hand-measured rate. |
| `REV#51` | DECISION | could | S | Owner call: a compiler-free lint, with failing fixtures, that every tracked `.cpp` under `engine/`, `tests/`, `samples/` and `bench/` is named by its CMake file. A check on 2026-11-12 found none unlisted, so it guards a regression only. |
| `REV#52` | DECISION | could | S | Owner call: build the four other release presets and `wasm-debug` on `ci.yml`'s weekly schedule as non-required jobs; needs `tests/ctest_inventory.json` lists and `ci_gap_report`'s gap definition changed together. |
| `REV#53` | OPEN | should | S | `tools/web_bundle/bundle.mjs` holds no size budget; `linesweeper.wasm` is about 487,000 bytes. First step, no call: a brief with raw and Brotli sizes, the growth by tag and two or three figures, after `REV#155`. The figure is the owner's. |
| `REV#54` | DECISION | should | XS | v0.3.0's counters, 2026-10-29 to 2026-11-11, are recorded beside the review. Left: whether `CONTRIBUTING.md`'s release procedure gains a step recording views, unique visitors and the starter ZIP's downloads; the traffic API needs the owner's access. |
| `REV#55` | DECISION | could | M | Texture repeat addressing: `TextureFilter` is the seam's whole sampler vocabulary and every backend clamps; a wrap mode costs goldens on four rasterisers, works only for whole textures, and has no caller yet. |
| `REV#56` | DECISION | could | L | Whether the public text type stays `std::wstring` while `Keyboard::typed()` returns UTF-8; SEAM.md decides the wide entry point, so a change is a source break best settled with `PORT#2` and `IDEA#1`. |
| `REV#57` | DECISION | could | S | A CollisionObject whose shape holds a NaN forms no pair and, with Scene bounds set, is flagged for retirement; nothing throws, counts or is tested. Whether resolve() reports it is undecided. |
| `REV#58` | DECISION | could | M | No layout-label query beside `key_name` in `engine/input/keyboard.h`; sample help text prints fixed US letters. Decide with `IDEA#2`, its only real caller, with a stated fallback for Firefox and Safari. |
| `REV#59` | DECISION | could | M | `GamepadState` carries no pad identity, so a browser build with a PlayStation or Switch pad still shows the samples' fixed Xbox letters. Decide with `GAP#1`; native builds would always answer Xbox. |
| `OCA-76` | OPEN | could | S | Waits for a second caller. Its caller-free slice needs no call: walk the starter's browser build through IDBFS (`GUIDE#12`'s leftover) with and without `autoPersist`, and record it. An engine JSON writer needs README's boundary amended. |
| `REV#60` | DECISION | could | S | A read-only reduced-motion fact on Application or the window, read from the browser media query and from Windows' client-area animation setting; LineSweeper's native `main.cpp` ignores the setting today. |
| `REV#61` | DECISION | could | M | Whether the engine offers a shared fatal-error report (title, message) for `samples/minimal/main.cpp` and `main_web.cpp`, or a run helper; the two-file anatomy is ARCHITECTURE's stated design and would be amended. |
| `REV#62` | DECISION | could | M | Five guides use features newer than the starter's release with no note at the passage ([run in a v0.3.0 starter](docs/review/2026-11-10-first-run/FINDINGS.md#rev62)); choose a banner or per-passage notes without reopening the 2026-10-27 default. |
| `REV#63` | OPEN | could | M | Whether the starter carries a `.vsconfig` and one installer command, against the 2026-10-30 'no bundled installer' decision. First step, no call: one Sandbox run installing from a candidate `.vsconfig`, then building the v0.3.0 starter, timed. |
| `REV#64` | DECISION | could | M | A copyable touch-control component in `samples/components`: held button rectangles answering a bitset per step and a stick region answering a Vector2F, with headless tests over synthetic TouchPoints. |
| `REV#65` | DECISION | could | S | Whether to document and test FetchContent, flag the walkthrough repository as a GitHub template, or add install/export or a vcpkg port; `README.md` documents only the submodule and the starter ZIP. |
| `REV#66` | DECISION | could | M | Releases attach only the docs tarball and the starter ZIP, and only `samples/minimal/CMakeLists.txt` has install rules; whether to ship prebuilt Windows sample binaries is the owner's scope call. |
| `REV#67` | DECISION | could | M | Owner call: whether to reverse the security-policy story's T1 refusal of a fuzzing framework and add libFuzzer targets for the content readers; depends on the sanitizer configuration. |
| `REV#68` | OPEN | could | S | Whether to add `/analyze`, a `.clang-tidy` or CodeQL is the owner's. First step, no call: one `/analyze` run over engine and tests in a scratch build, counting first-party and vendored warnings and checking each first-party one. |
| `REV#69` | OPEN | should | S | Record fix, no call: add rapidjson to `docs/dependency-updates.md`, and make `external/rapidjson/README.md` name its upstream commit and stop describing files not in this tree. Then the owner's call on Dependabot alerts and Node 20 tags. |
| `REV#70` | OPEN | could | M | A compare-only release rebuild job, and attesting or signing, are the owner's. First step, no call: rebuild the v0.3.0 starter ZIP and docs snapshot from the tag in a worktree and compare their sha256 with the release's digests. |
| `REV#71` | DECISION | could | L | Owner call: a race check of `engine/core/thread_pool.cpp`'s fan-out needs a `std::thread` pool and a Linux clang ThreadSanitizer build, which NMS-04's refusal of non-Windows hosts rules out today; no race is recorded. |
| `REV#72` | OPEN | could | M | No test loads a page in a cross-origin iframe, no guide publishes to itch.io and `bundle.mjs` writes no `.br` files. First step, no call: drive the bundle once inside a cross-origin iframe and record what fails. Each change is a call. |
| `REV#73` | DECISION | could | S | `README.md` has no image and the site sets no `og:image`, though LineSweeper captures exist; whether to add both, and whether to announce or list the project, is the owner's call. |
| `REV#74` | DECISION | could | S | No document states a release rhythm beyond `SM-36`'s pin cadence, and the website reference marks nothing stable or provisional; PHILOSOPHY promises a record, not compatibility, before 1.0. |
| `REV#75` | DECISION | could | S | `why.mdx` publishes no frame-time figure while `SM-07` is blocked; whether to publish a labelled desktop figure from the frame bench's records in the meantime is the owner's call. |
| `REV#76` | DECISION | could | XS | `README.md`, the site's home page and `why.mdx` say nothing about agent use, though public history and `AGENTS.md` show it; the statement and any contributor policy are the owner's to write. |
| `REV#77` | DECISION | could | S | `DrawList::draw_text`, `RenderResources::measure_text` and `record_text` take `const std::wstring&` while `Font` and `wrap_text` take `std::wstring_view`; a source change on five backends with no measured cost. |
| `REV#78` | DEFERRED | could | M | `ResourceLoader::load_manifest` loads everything in one blocking call with no progress report, and no resource type has an unload verb; waits for a client with a long load or level-scoped content. |
| `REV#79` | DECISION | could | M | engine/math/shape_type.h has no convex N-gon, capsule or segment shape; no client or sample asks for one, and a variable-size shape meets the no-allocation rule. |
| `REV#80` | OPEN | could | S | A CollisionObject carries one Shape, which PHILOSOPHY and `scene.h` do not state. First step, no call: a spawn-and-retire churn case in `bench/scene_bench.cpp` asserting its complexity class. Stating the limit stays the owner's. |
| `REV#81` | DEFERRED | could | S | Manifold, Contact and on_contact carry a normal and a penetration but no contact point; no client has asked. Revisit only if #124 breaks on_contact's signature. |
| `REV#82` | DECISION | could | S | `Key` in `engine/input/keyboard.h` has no ISO 102nd key, so the key beside left Shift reads as `Key::none` and cannot be bound; the left and right modifier split stays deferred for want of a client. |
| `REV#83` | DECISION | could | M | `ApplicationOptions` has no option for a fixed-size window, a starting position, a monitor or a maximised start, and no monitor query is exposed; no client asks for any. If one is taken it rides `IDEA#16`. |
| `REV#84` | DEFERRED | could | S | No query for the player's preferred language on Win32 or in the browser shell under `engine/app/`; nothing in the tree ships two languages, so it waits for a localised sample or client. |
| `REV#85` | DEFERRED | could | M | `AudioDevice` has no verb to close a wave bank or destroy a voice, so a game that replaces banks grows both lists until the device is destroyed; waits for a client that swaps banks. |
| `REV#86` | DEFERRED | could | M | The audio backends set no voice ceiling, limiter or master gain, and ungrouped sounds in `AudioResources` play at full level; inferred, nobody has heard clipping. Follows `WEB#3`'s listening and a client. |
| `REV#87` | DEFERRED | could | M | `AudioDevice` and `SoundBank` expose no playback position, wave duration or start offset; the XAudio2 cost is unmeasured. Waits for a client that times something to audio, and would ride `GAP#2`. |
| `REV#88` | DECISION | could | S | The website has no one-page quick reference of the game-facing API; if wanted, it is generated from the header reader or from sample excerpts, never hand-written. |
| `REV#89` | DECISION | could | S | `why.mdx` and the local-multiplayer guide say nothing of Proton, Steam Deck or Remote Play Together; a sentence would name each as outside the engine and unverified. No Deck or Linux machine is to hand. |
| `REV#90` | DECISION | could | M | `docs/2026-11-04-decision-recommendations.md` GAP#1 option B does not note that SDL would also reach other platforms; at most one paragraph there. No Linux or macOS plan exists, by recorded decision. |
| `REV#91` | DECISION | could | S | A documented cache variable or preset that emits a PDB beside an otherwise identical Release build, kept out of the install folder and stated in the release guide; a minidump writer is a separate question. |
| `REV#92` | DECISION | could | S | No guide covers source maps, DWARF or stepping through C++ in a browser, or says where printf and stderr go; a short section would say debug natively first, then how when a fault is browser-only. |
| `REV#93` | DECISION | could | XS | What a player sees on Escape in browser full screen is unobserved (web-as-built.md lines 81-83; your-window.mdx lines 448, 687, 700); observe it in the owner's informal browser play, then one sentence in the guides. |
| `REV#94` | DECISION | could | S | `CONTRIBUTING.md` has no sentence on whether outside pull requests are wanted; a code of conduct, the wiki, Discussions and FUNDING are repository settings and policy only the owner sets. |
| `REV#95` | DEFERRED | could | S | bench/scene_bench.cpp has no touching-tile case, and layers_collide runs after BroadPhase has emitted the pair; a bench case or change waits for a client with a tile world and a profile. |
| `REV#96` | DEFERRED | could | S | `engine/app/web/window.cpp`'s `measure` has no upper bound on device pixel ratio; a cap is a contingent answer to R6, deferred until `SM-13`'s phone measurement shows a frame over budget. |
| `REV#97` | OPEN | could | S | `engine/app/web/window.cpp:452-456` rounds CSS size times ratio instead of reading `device-pixel-content-box`. Reproduce at ratios 1.25, 1.5 and 1.75 through CDP against a ResizeObserver; a change follows only an observed blur. |
| `REV#98` | DEFERRED | could | XS | `.github/actions/emscripten/action.yml:80-85` saves a ccache per run and attempt; re-measured 2026-11-12 at 37 caches, 2.84 GB, 27 of them ccache. Deferred until an expensive cache is seen evicted, since restore-only changes documented reuse. |
| `REV#99` | DEFERRED | could | S | No coverage tooling exists in `cmake/`, `tools/`, `.github/` or `CMakePresets.json`; deferred until a decision waits on a coverage figure, since the dark backend recovery paths are already named in the `REV#` rows. |
| `REV#100` | DEFERRED | could | S | `tools/web_bundle/bundle.mjs` writes raw and Brotli sizes to `BUILD.json` with no threshold; the budget is `REV#53`'s call, and a Brotli tolerance is deferred until that budget is taken. Bench output stays ungated. |
| `REV#101` | OPEN | could | XS | Only `.editorconfig` and `cmake/check_whitespace.cmake` check style. One run of pinned ruff over `tools/` and actionlint over `.github/`, outside CI, either records a defect, which fires the deferral, or confirms it. No `.clang-format`. |
| `REV#102` | DEFERRED | could | S | Closure, emmalloc, `-Os`/`-Oz` and LTO are untried for `wasm-release` (`cmake/settings.cmake:85-92`); measure them once a size budget exists to hold the result. |

## First-run review of 2026-11-10

**Owner:** [the first-run review of 2026-11-10](docs/review/2026-11-10-first-run/README.md): [FINDINGS.md](docs/review/2026-11-10-first-run/FINDINGS.md) holds each row's evidence under its id, and [STATUS.md](docs/review/2026-11-10-first-run/STATUS.md) records each resolution. An `OPEN` row needs no owner call; three say they are unverified and are reproduced before anything is written. A `DECISION` row is the owner's call.

| id | status | pri | size | item |
|---|---|---|---|---|
| `REV#106` | OPEN | should | S | The starter's `x64-debug` preset builds a 32-bit game in a 32-bit prompt without complaint; the next x64 build fails with about 1,400 linker lines that reconfiguring does not cure. Guard the compiler in CMake and add an `LNK4272` entry. |
| `REV#107` | OPEN | should | XS | `prerequisites.mdx`'s *Keeping a deep folder* sends readers to the root of a `subst` drive, where Windows' `tar` cannot extract the starter ZIP, and `starter.mdx`'s boxes hard-code `C:\dev`. |
| `REV#108` | OPEN | should | S | `starter-update.mdx` carries only `game` into the new project: `.git`, `web-release.bat` and top-level pictures stay in `mygame-old`, which the page then invites the reader to delete. |
| `REV#109` | OPEN | should | S | Troubleshooting lacks entries for messages produced in a v0.3.0 starter: the moved-folder `CMakeCache.txt` error, what follows a failed configure, deep-folder outcomes, a failed download, the debug runtime box and an unsaved edit. |
| `REV#110` | OPEN | should | S | `prerequisites.mdx` step 1 says nothing of the installer's screens before the Workloads tab, nothing marks where a starter reader is finished, and the installer is still unwalked: one clean-machine install settles it. |
| `REV#111` | OPEN | could | S | No page states a Windows version, Arm, S mode or administrator rights before the install, and the total disk, download and time first appear in step 1 of the second page. |
| `REV#112` | OPEN | could | S | Unverified: Smart App Control or Defender blocking the reader's own `MyGame.exe` has no page, and `starter.mdx` step 7 would send them to rebuild. Reproduce on such a machine, then write the entry. |
| `REV#113` | OPEN | could | S | Unverified: on a Windows that is not in English, cmd, vcpkg and the compiler print translated messages no page quotes. Check whether `VSLANG=1033` restores the English text, then say so in `prerequisites.mdx`. |
| `REV#114` | OPEN | could | S | Unverified: what a build prints after a Visual Studio update is not recorded, and no page says how to remove everything or where vcpkg's cache is. Apply one update to a built starter, then write both. |
| `REV#115` | OPEN | could | XS | The release page lists the docs tarball first and *Source code (zip)* at the starter's size with no page recognising a wrong pick, and `starter.mdx` step 3 run again overwrites an edited `mygame` silently. |
| `REV#116` | OPEN | could | S | Eight findability stumbles: the home card and Examples' button describe the Git route, *Edit and debug*'s next page is the Git route's first, the guides index is one ungrouped grid of 42 cards, and errata link the wrong pages. |
| `REV#117` | OPEN | could | S | Day-two guides interact unstated: four pages rely on the start-up test after *Add a screen* changed it, *Bigger text* times out the levels page's test, the build keeps a swapped-out texture, and a JPG works where the page says PNG only. |
| `REV#118` | DECISION | could | S | The starter ZIP is the whole engine repository (1,146 files, 16 the reader's), and `game\README.md` and its comments still call the project the minimal sample; whether `starter_zip.py` slims or rewrites is a scope call. |
| `REV#119` | DECISION | could | M | No page reads the starter's own three files at the 2026-10-30 floor, *Make a first change* routes a beginner to comments written for engine programmers, and Get Started uses about 40 terms with no glossary. |
| `REV#120` | DECISION | could | S | Whether the starter carries batch files that find Visual Studio and run configure, build, test and play, removing the x64 Native Tools prompt from the route; it borders the 2026-10-30 guided-terminal decision. |
| `REV#121` | OPEN | could | S | The first configure downloads a 53 MB CMake and a 117 MB PowerShell that Visual Studio 2026 has just installed. Find out why vcpkg fetches them and whether a setting reuses Visual Studio's; changing the starter is the owner's call. |

## Work audit of 2026-11-12

**Owner:** [the work audit of 2026-11-12](docs/review/2026-11-12-work-audit/README.md): [FINDINGS.md](docs/review/2026-11-12-work-audit/FINDINGS.md) holds each row's evidence under its id, and [STATUS.md](docs/review/2026-11-12-work-audit/STATUS.md) records each resolution. An `OPEN` row needs no owner call; a `DECISION` row is the owner's, and a yes becomes a story in [BACKLOG.md](docs/product/BACKLOG.md) before the work starts.

| id | status | pri | size | item |
|---|---|---|---|---|
| `REV#124` | DECISION | should | S | Non-finite and zero/non-finite-scale refusals landed. Left for the owner: is a negative (mirroring) `Camera` scale supported? A hand-built one gives a negative-extent `visible_rectangle`, and Scene's cull (`engine/scene/scene.cpp`) then intersects nothing, so the view goes blank without any signal. |
| `REV#126` | OPEN | should | M | PHILOSOPHY.md:702 forbids per-frame heap allocation and four headers claim it, but only D3D12 and Vulkan submit tests count allocations. Count across warmed Scene ticks, contacts, the four-view fan-out and LineSweeper's top-out on native null. |
| `REV#130` | DECISION | could | S | RectangleI's and Vector2I's float conversions are undefined for NaN or out-of-range input, and Vector2I division has no zero or `INT_MIN / -1` guard. Refuse with `invalid_argument` (T2) or state a precondition; SM-71 left it to its own decision. |
| `REV#132` | OPEN | should | S | About 71 guide steps send the reader to a quoted line; nothing checks them, and `your-components.mdx:306` no longer matches `hello_state.cpp:152` on master. Fix it, and add a website test that each anchor occurs in a starter-bound file or earlier on its page. |
| `REV#133` | DECISION | could | M | The day-two guides teach by about 150 inline C++ blocks nothing compiles; each rests on one dated agent run. Compiling a finished copy per guide in the consumer job is a lasting maintenance and CI cost; REV#44 is one instance. |
| `REV#134` | OPEN | should | S | `testing.mdx:461` says the start-up test presses no keys and teaches only out-of-process SendInput; `focus_pause_tests.cpp` shows the in-process route with public API. Add a native-null test driving minimal's HelloState and a guide section quoting it. |
| `REV#135` | OPEN | could | S | `tick.mdx:30` says owed steps run 'usually once per frame', false at 144 Hz, and no page mentions `step_position`. Correct it and teach interpolating the greeting, including `application.h`'s rule that a paused state must not use the fraction. |
| `REV#136` | DECISION | could | S | `random.h` is published and explains why `<random>`'s distributions differ between MSVC and the browser, but no guide uses it. A guide's main paragraph depends on REV#35 (whether Random moves to LabradorMath), so sequence it after that call. |
| `REV#137` | OPEN | could | S | PHILOSOPHY defines a component as header, source and tests, but `your-components.mdx:57-61` copies no tests, and the starter's test target cannot link them. Add an optional step with an engine-linked MyGameComponentTests, checked in a starter. |
| `REV#138` | OPEN | could | S | `drawing.mdx:116-123`, `resources.mdx:58-60` and `assets.mdx:107-121, 144-150` condense public declarations by hand; REV#77's change to `draw_text` would leave them silently wrong. Excerpt what can be, check the rest against the header reader. |
| `REV#139` | OPEN | should | M | `verify_changes.py` accepts only `--area render` and refuses other paths, so AGENTS.md's audio obligations are rebuilt from prose each time. Add audio, input and app areas over the existing tracks, with planner tests and AGENTS.md's table amended. |
| `REV#140` | OPEN | should | S | Four native Release presets and `wasm-debug` have no inventory list, so `ci_gap_report` passes a run that registers less; AGENTS.md says `wasm-debug` has 34 entries and, later, 33. Add the lists without a schedule (REV#52 keeps that). |
| `REV#141` | OPEN | should | M | CONTRIBUTING's release procedure is a long hand list (version-string, CHANGES heading, minimal change-list ids, notes wording) that no tool checks, against the 2026-11-03 checklists decision. A read-only `tools/release_check.py` with an offline test, and `--notes`. |
| `REV#142` | DECISION | could | XS | `check_changes_rule.py` covers engine headers, `engine/CMakeLists.txt`, `settings.cmake` and `vcpkg.json`, but a consumer's configure also runs the root CMakeLists, three `cmake/` scripts and `engine/math/CMakeLists.txt`. Widening it is the owner's policy. |
| `REV#143` | OPEN | could | XS | `labrador_add_test_suite` registers suites with a bare `add_test` and no preset sets a timeout, while the D3D12 and Vulkan backends wait without bound; a device hang holds a job to its 90-minute limit unnamed. Add a measured default TIMEOUT. |
| `REV#144` | OPEN | could | S | CONTRIBUTING's twenty required contexts were checked once for SM-21; a renamed job or matrix value leaves every PR waiting. An offline test deriving contexts from `ci.yml` and `website.yml` and comparing both ways, in the existing tooling step. |
| `REV#145` | OPEN | could | S | The lint job's steps are listed in AGENTS.md, `ci.yml` and CONTRIBUTING, and they drift: `CONTRIBUTING.md:467` still says `npm run build` where AGENTS.md says `npm run gate`. A `tools/lint_local.py` held to `ci.yml` by a test. |
| `REV#146` | OPEN | could | XS | A recorded sweep failed because a cmd started from Git Bash inherited MSYSTEM, which leaves `emcc` off PATH; preflight reads no MSYSTEM. Fail before any build when a wasm track is planned and it is set, in both tools. |
| `REV#147` | DECISION | could | S | PHILOSOPHY.md:863-864 calls a build-time regression a defect, but nothing measures one. A report over `.ninja_log` and depfiles is agent work; recording baselines per release or warning in CI is the owner's. |
| `REV#148` | OPEN | could | S | `verification_runner.py:181` hard-codes `source_revision` and `device_identity` as uncollected, and `ci_gap_report` records no device, so whoever pastes the report names the adapter by hand. Collect `git` HEAD and RenderPixelTests' device line. |
| `REV#149` | OPEN | should | S | 31 entries sit under CHANGES.md's Unreleased since v0.3.0, ColourWars' pin cadence follows each minor tag within a week, and SRL-07 waits on that move. Run `ci_gap_report --execute` and the release checks; the tag is the owner's. |
| `REV#150` | OPEN | should | S | CI runs only `FirefoxWebWindowTests` because the runner's Firefox makes no WebGL2 context without preferences. Try WARP or forced-WebGL preferences behind an opt-in runner flag, prove it locally, then run the seven drawing entries; record a negative result too. |
| `REV#151` | OPEN | could | M | Safari and Firefox runs prove drawing and keys but read back nothing; `RenderPixelTests` and `WebWindowTests` run only in Chromium. Run both pages in CI's Safari job, recording per-case drift on the Apple GPU; Firefox follows REV#150. |
| `REV#152` | DECISION | could | M | Every phone check of the game is Chrome emulation; the website already drives Safari on the iOS Simulator, but the game does not. A second Simulator lane adds the flakiness CI already retries around, so it is the owner's call. |
| `REV#153` | OPEN | could | S | R6 and the phone's load time are open with no rehearsal of a slow device, and nothing uses CDP's CPU or network throttling. Run the R6 fixture at 1x, 4x and 6x and the bundle under slow 4G, labelled as desktop emulation, not phone evidence. |
| `REV#154` | OPEN | could | S | Every build allows memory growth, but no test reads `WebAssembly.Memory`'s `byteLength`; R4 has no desktop baseline. Extend the bundle test's existing memory hook to record each sample's heap after warm-up and at quit, before any gate. |
| `REV#155` | OPEN | should | S | `linesweeper.wasm` grew from 403,759 bytes at v0.1.0 to about 487,000; REV#53's budget and REV#102's flags wait on numbers nobody has. Attribute code bytes by module at each tag with names kept, in a dated record; change no flags. |
| `REV#156` | OPEN | could | XS | `web_context.cpp:286-306` says every context attribute is stated, but `powerPreference`, which picks the GPU on a hybrid laptop, is left to the default. Make it explicit and record what Chrome selects under each value; choosing another is the owner's. |
| `REV#157` | OPEN | could | S | The bundle page's context loss is forced through the WebGL extension and has run on SwiftShader only. Run it headed in Chrome on this desktop's GPU, including a GPU process crash, and record the result in web-to-be-tested.md. |
| `REV#158` | OPEN | could | S | ColourWars report 19's next step is evidence: what a default-visible `x64-release` minimal launch shows before the first present, then a hidden launch shown natively. The engine's show-after-first-frame shape waits on it; capturing it needs no call. |
| `REV#159` | OPEN | should | S | CI's `safari` job lost quit keys on several sample pages. Pages whose keys start Web Audio or full screen now get one action list, the others one confirmed key at a time, each the form that passed there; failures print the page's keys and focus. Close after a week clean. |

## Ports

**Owner:** [docs/port/android.md](docs/port/android.md), section 4.

| id | status | pri | size | item |
|---|---|---|---|---|
| `PORT#1` | DEFERRED | could | M | A KTX2 reader beside `dds_file.h` with the ETC2 formats (spine item 3.1a, headless, blocks nothing), and an ETC2 encode of the sprite sheet to judge its hard alpha edges. Until the Android port is taken up. |
| `PORT#2` | DEFERRED | could | XL | The device-bound half of the Android spine: an arm64-v8a NDK preset, `char16_t` on the render API, a push-only pad seam (the port re-takes report 21's decline of an action map against android.md's argument rather than presuming the layer), the activity shell with surface loss. Waits on an Android device. |

## Areas with no open rows

A new row for one of these gets its section back.

| area | owner | a row returns with |
|---|---|---|
| The rest of Next (X1 to X28) | [BACKLOG.md, Next](docs/product/BACKLOG.md#next) | its batch reaching the front; the rows above are the batches NOW names, and the Next stories that the owner or an outside event gates, or that a row here pairs with |
| Content, text and sample examples (L21 to L23, L26, L35) | [BACKLOG.md, Later](docs/product/BACKLOG.md#later) | a new text, content or sample-example story reaching the front |
| Code hygiene (L36 to L54) | [Code hygiene](docs/code-hygiene.md) and [BACKLOG.md, Later](docs/product/BACKLOG.md#later) | a new bounded repair or review story reaching the front |
| The rest of Later (L1 to L35) | [BACKLOG.md, Later](docs/product/BACKLOG.md#later) | its next ready batch reaching the front under the scheduling amendment; the hygiene lane runs alongside product work |
| CI (`CI#`) | [.github/workflows/ci.yml](.github/workflows/ci.yml), [docs/dependency-updates.md](docs/dependency-updates.md) | a new CI check or job; a new required job or matrix entry needs the owner to update branch protection |
| Won't this time | [BACKLOG.md](docs/product/BACKLOG.md#wont-this-time) | a re-plan of the backlog with its reason recorded there |
| web.md's milestones M1, M2, M5, M6 and its decisions | [docs/port/web.md](docs/port/web.md) sections 8 and 9, and [docs/port/web-as-built.md](docs/port/web-as-built.md) for what each measured | a regression, or a change the port meets that a decision must settle |
| macOS and iOS | [docs/port/android.md](docs/port/android.md) sections 5 and 6 | a plan document; none is in the tree |
| The EC2 performance lane | [tools/cloud_performance/README.md](tools/cloud_performance/README.md) | a new declared reference run, or a regression question |
| round-2 plan, the RTCD list, the first review, backend-equivalence | [docs/review/round-2/PLAN.md](docs/review/round-2/PLAN.md), [docs/review/rtcd/](docs/review/rtcd/), [docs/review/IMPLEMENTED.md](docs/review/IMPLEMENTED.md) | nothing: all landed or superseded; the game-side rows live in ColourWars |
| The 2026-09-26 review's numbered findings and hardening candidates | [docs/review/2026-09-26/STATUS.md](docs/review/2026-09-26/STATUS.md) and [the review index](docs/review/INDEX.md#hardening-verification) | nothing: all 23 implemented and four hardening candidates addressed; `REV#12` remains |
| Beginner guides (`GUIDE#1` to `GUIDE#13`) | [Beginner guides](docs/beginner-guides.md) | a new procedure a beginner cannot follow, or a page that stops matching the starter |
| The historical repository split, file-length tripwire and survey decisions | [docs/repo-split.md](docs/repo-split.md), [docs/file-length-audit.md](docs/file-length-audit.md), [docs/survey/2026-08-26-status.md](docs/survey/2026-08-26-status.md) | nothing: original work and StateContext's keep verdict stand; current hygiene work is indexed above |

---

## How to read this file

Rules for writing it: read them before editing a row, not before picking one.

**This file is an index, not a work order.** One row per open item: an id, a status, two grades, one clause and a pointer. Why an item matters, how to build it and its acceptance criteria belong in its owner, usually its story in [docs/product/STORIES.md](docs/product/STORIES.md); a second copy here goes stale unnoticed. [docs/product/BACKLOG.md](docs/product/BACKLOG.md) owns the order of the stories and their horizons, and this file does not reorder them: NOW names the next few stories in the backlog's order, and a reorder the owner makes is amended into BACKLOG.md. A row with no story goes into NOW by urgency.

1. **A row leaves this file in the same commit that closes it in its owner.** For a story, tick its box in BACKLOG.md; for anything else, record the result where the row points. No closed rows stay here. An emptied section becomes one line under "Areas with no open rows".
2. **The owner is the source of truth.** When this file and its owner disagree, this file is stale. For a story, its box in BACKLOG.md says whether it is open; STORIES.md holds its detail as of 2026-10-13 and is not edited.
3. **When part of an item lands, replace the clause with what is left;** do not append to it. Keep a row under 300 characters.
4. **A new story is a line in BACKLOG.md before it is a row here**, added by amending a horizon there (STORIES.md is the record of 2026-10-13 and is not edited). Work with no story gets a prefixed id owned by its document:
   - `CI#`: [.github/workflows/ci.yml](.github/workflows/ci.yml), or the document the row names.
   - `WEB#`: [docs/port/web-to-be-tested.md](docs/port/web-to-be-tested.md) and [docs/port/web.md](docs/port/web.md).
   - `SITE#`: the website page or plan the row names, under [website/](website/) or [docs/website-plan.md](docs/website-plan.md).
   - `PERF#`: [docs/performance/](docs/performance/).
   - `REV#`: the review under [docs/review/](docs/review/) the row names.
   - `PORT#`: [docs/port/android.md](docs/port/android.md).
   - `DOC#`: the record the row names.
   - `GAP#`: [the engine comparison of 2026-10-27](docs/survey/2026-10-27.md).
   - `IDEA#`: [Feature candidates](docs/feature-candidates.md), selected into BACKLOG.md before implementation.
   - `GUIDE#`: [Beginner guides](docs/beginner-guides.md).
5. **NOW changes only when its items finish or the owner reorders them.**

**`id`:** a stable handle, not an order. A story's id where one owns the item (`OCA-03`), else a prefixed one from rule 4; a retired id is never reused. Other ids an item covers are named in its clause.

**`status`**, a closed list:
- `OPEN`: ready to work.
- `DECISION`: the next step needs the owner: a call, a repository setting, a deploy, or a session with hardware the owner has. Always listed in NOW.
- `WAITING`: waits for an event outside the work: a PR merging, a deploy.
- `BLOCKED`: cannot be worked today: a prerequisite story is open, or it needs hardware that is not to hand.
- `DEFERRED`: paused on purpose, until the condition in its clause holds.

**`pri`:** the story's MoSCoW priority as BACKLOG.md gives it: `must` · `should` · `could`. A row with no story gets the grade its owner implies, and what the backlog put in Won't this time never gets a row.

**`size`:** as in BACKLOG.md: `XS` one small commit · `S` a session or less · `M` a few sessions · `L` a week or more · `XL` several weeks.

**The repository is public.** Write as the design documents do: plain, exact, the present state rather than its history, and the owner as "the owner". Nothing about ColourWars beyond what this tree already publishes.
