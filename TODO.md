# TODO: open work

An index of open work, one row per item; each row points to the document that owns the detail. The rules for editing it are at the end, in [How to read this file](#how-to-read-this-file). A fresh thread starts here: read **NOW**, then the owner of the row it picks up. The order of stories and their horizons is [docs/product/BACKLOG.md](docs/product/BACKLOG.md)'s, the one file in `docs/product/` kept current, and this file does not change it.

## NOW

**The only ordering in this file.** Keep every `DECISION` row here. Keep this block to the current aim and the next few actions; results go in their owners, and a closed story is a ticked box in BACKLOG.md.

**Now is finished**: every batch of [BACKLOG.md, Now](docs/product/BACKLOG.md#now), N1 to N21, has landed, and `v0.1.0`, `v0.2.0` and `v0.3.0` are cut.

**Aim:** finish Next's remaining acceptance while taking ready Later work in
parallel, as [BACKLOG.md's scheduling amendment](docs/product/BACKLOG.md#later)
allows. Next's hardware and release gates remain open. Apart from the
re-planned rows below, what remains in Later waits on the owner, on another
story or on an outside event. `DOC#4` is ready: ColourWars' repository is public.

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

Ready agent rows from the 2026-11-03 persona re-review are listed in their sections.

**Decisions and sessions only the owner can do.** Every `DECISION` row below, in the order of the backlog's horizons:
- Next: `OCA-48` + `BDP-06` (informal play with the physical pad, which can also feel `PI-21`'s vibration and play `PI-14`'s pad match) · `SM-09` (informal play on the current MacBook Air) · `SM-01` (re-enable the Radeon in the BIOS at the next reboot, install AMD's driver, then the bench's adapter check) · `PI-14` (close as delegated to ColourWars, or wait for its `v0.3.0` run).
- Later: `WEB#3` (listen to the audio sample), `OCA-66` (look at the template at 150% scaling), `BDP-07` (judge judder at 60 Hz and at 144 Hz), `OCA-79` (authorise the starter project's first push to GitHub), `WEB#11` (re-sync and deploy the live demo).
- Other acceptance: `REV#3` (display change), `REV#8` (audio endpoint loss/recovery).
- Not yet placed: `GAP#1` to `GAP#13`, whether to take up each feature the engine comparison found; a yes becomes a story in BACKLOG.md.
- New proposals: `IDEA#1` to `IDEA#6` in [Feature candidates](docs/feature-candidates.md): typed text, rebinding, save recovery, camera following, grid-sheet authoring and gallery pointer interaction. `IDEA#7` to `IDEA#19`, from the engine review of 2026-10-31: circle collision, scene queries, contact edges, arbitrary window sizes, WAV and in-memory sounds, named-frame animations, a focus hit test, the present interval, the refresh rate, title and icon, cursor shapes, a side-by-side split and small math helpers.

## Input, menus, settings and saves

**Owner:** [STORIES.md](docs/product/STORIES.md); order in [BACKLOG.md](docs/product/BACKLOG.md), Next X13/X14 and Later L19/L20.

| id | status | pri | size | item |
|---|---|---|---|---|
| `OCA-45` | OPEN | should | S | Owner decided no second pad (2026-11-03). Left: move minimal's slot scan into a pure rules function and assert in MinimalTests a pad only in slot 1 or 3 and slot 0 emptied; then informal one-pad play. [Decision](docs/product/BACKLOG.md#next). |
| `PI-21` | OPEN | should | S | Add the missing browser-transport test (`dual-rumble` played and reset); then the owner's informal pad play, natively and in Chrome, in `OCA-48`'s session. [Re-plan](docs/review/colourwars-reports/vibration.md#acceptance-and-evidence). |

## Website and the hosted demo

**Owner:** [website/README.md](website/README.md) and [docs/website-plan.md](docs/website-plan.md); the hosted demo in [docs/port/web.md](docs/port/web.md) section 7 and [docs/port/web-to-be-tested.md](docs/port/web-to-be-tested.md).

| id | status | pri | size | item |
|---|---|---|---|---|
| `SITE#6` | OPEN | could | S | axe-core's `label-content-name-mismatch` (WCAG 2.5.3) fails Starlight's search button: its visible "Ctrl K" hint is not in its accessible name "Search". The acceptance scan leaves `site-search` out until it is fixed, by an override or upstream. |
| `SITE#5` | OPEN | should | S | Not a release gate since the owner's decision of 2026-11-03 to cut `v0.3.0` without it. Make `acceptance (ios)`, which runs only on request in [the acceptance trial workflow](.github/workflows/acceptance-trial.yml), pass reliably, then move it back into the website workflow as a required check. safaridriver on iOS 26.5 places touches at the requested point less the page's scroll offset while refusing points outside the viewport, so the release index check does not follow the card's documentation link, below the first screen, until WebKit fixes it (owner's decision, 2026-11-03). Search text is typed on the Simulator's hardware keyboard, because safaridriver's send keys inserts none. Still unreliable: of three runs on 2026-11-03, taps were taken as long presses, a navigation timed out and one session was never created. |
| `SITE#4` | OPEN | should | S | iOS Safari shows a pill naming the host above the keyboard while typing in search; tapping it, or pressing Go on an empty box, leaves an empty page. Seen 2026-11-03 on the live site and the versioned fixture, so not versioning. Pagefind's form has `action="javascript:void(0);"`, the likely cause. |
| `OCA-79` | DECISION | should | S | Push a current-release starter project, as *Put your project on GitHub* says, to a public test repository under the owner's account (owner's authorisation), and record its green `ci.yml` run in the guides. [Story](docs/product/BACKLOG.md#oca-79). |
| `WEB#11` | DECISION | should | S | Live demo is `v0.2.0`. Prepare a verified `v0.3.0` bundle; the owner syncs, updates the host page's controls, failure text, loss wait, access note, gate wording and project-page figures, and deploys. [web.md §7](docs/port/web.md#7-what-the-host-page-gets). |
| `WEB#12` | OPEN | could | S | A script checking the live demo after each deploy: content types, both Cache-Control values, Brotli, sizes and SHA-256 against the served `BUILD.json`, and the served commit; its dated output replaces M5's manual ticks. [M5](docs/port/web-to-be-tested.md). |
| `WEB#13` | OPEN | could | S | The host's page script (gate, failure UI, controls) has no automated check; `BDP-03` tests the bundle's example page. Serve the bundle page in a site wrapper, or run bundle-page cases against the host page. [web.md §7](docs/port/web.md#7-what-the-host-page-gets). |

`SM-30`, the owner-only gate follow-up in [BACKLOG.md, Later](docs/product/BACKLOG.md#later), is ready now that `SM-29` is closed.

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
| `OCA-48` | DECISION | should | S | Informal play with the physical XInput pad: an `x64-release` sample, noting whether pad input reached it with another window focused, and the pad in Chrome (`BDP-06`). Mapping is SM-40's test; `BDP-06`'s Firefox half is `WEB#10`. |
| `SM-09` | DECISION | should | S | Ceiling recorded: Safari 27.0.1 on macOS 27.0.1, above the 18.4 floor. Left: the owner's informal play on the Air; the scripted match, quit and replay is `WEB#1`'s automated run. |
| `WEB#1` | OPEN | should | M | M3's Safari half automated: safaridriver on `macos-latest` drives CI's `wasm-release` sample pages and bundle (frames, keys, quit, replay, context loss) and reads `RENDERER`. If WebGL2 is unusable there, drawing stays informal play on the Air. |
| `WEB#10` | OPEN | should | S | Add LineSweeper and the bundle page's play, quit and replay to `run_firefox.mjs`, and register its runs as ctest entries when configure finds a Firefox; CI only if the image has one. Covers `BDP-06`'s Firefox half; story `SM-11` (Later L57). |
| `WEB#14` | OPEN | should | S | Investigate driving a real context loss (`chrome://gpucrash`) and a real tab switch in Chrome over CDP for `BDP-06`; what cannot be automated stays informal play. [M5](docs/port/web-to-be-tested.md). |
| `SM-14` | OPEN | should | S | Draft the LAN-serve procedure in `docs/port/`: LAN binding, secure-context features and how to get one, Web Inspector on the current MacBook Air, and Android Chrome's steps, written but untried. Closes once used in `SM-13`'s iPhone session. |
| `WEB#3` | DECISION | should | S | Hear the audio sample through speakers (each sound, pan, pitch, volume, held keys), the first-gesture unlock in each browser, resume after a hidden tab, and `ping_adpcm.xwb` on XAudio2. Nobody has listened yet. |
| `BDP-07` | DECISION | could | M | R5, M3's second open term: collect `requestAnimationFrame` intervals by script at 60 Hz (four screens) and 144 Hz (one screen, three unplugged); the owner judges only visible judder. [Method](docs/port/pacing-method.md). |
| `SM-13` | BLOCKED | should | M | Play the development page and the live demo on the owner's iPhone and record layout, rotation, touch, legibility, load time, memory (R4) and the top-out frame (R6). After `SM-14` (serving to a phone on the LAN); R6 by [the method](docs/port/pacing-method.md). |
| `BDP-10` | BLOCKED | could | XS | Lift the bundle page's phone gate after `SM-13`; `BDP-09`'s layout is in place. The owner approved iPhone-only evidence; Android is not a prerequisite. [Decision](docs/product/BACKLOG.md#decisions-only-the-owner-can-make). |
| `SM-44` | DEFERRED | could | S | Optional future real-phone record on a mid-range Android phone in Chrome. Waits on a borrowed phone; none is accessible. Does not gate iPhone acceptance. |
| `WEB#2` | BLOCKED | could | S | A standard-mapping pad in Firefox and Safari (Chrome is `BDP-06`'s), and unplug and replug with more than four pads seen. Firefox can ride `OCA-48`'s session, Safari `WEB#1`'s; five pads are not owned. |
| `WEB#5` | BLOCKED | could | S | Touch on real glass: several fingers with distinct ids, no tap also a click, foreground loss cancelling every point, LineSweeper's buttons through `WM_POINTER`, a pen as the mouse, Firefox and Safari. Needs a touchscreen. |

## Engine verification and reviews

**Owner:** [the review index](docs/review/INDEX.md) maps every review to its live ledger and tracks the remaining D3D12 and Vulkan candidates (`REV#3` to `REV#5`).

| id | status | pri | size | item |
|---|---|---|---|---|
| `REV#8` | DECISION | could | S | Bounded XAudio2 recovery and deterministic policy tests implemented; record real endpoint loss/reappearance, retained-bank playback and suspension/resume. gpt6 opportunity 4. |
| `REV#3` | DECISION | could | M | Check factory refresh in a real display-change session. [STATUS](docs/review/d3d12/STATUS.md) records bounded descriptor-query measurements, profile evidence, allocation reuse and full patterned mip read-back. |
| `REV#12` | BLOCKED | could | M | The 2026-09-26 review's manual checks: real device removal, interactive minimise, restore and drag, physical keyboard layouts and device changes, real sleep and wake. Needs a person at the machine. |
| `REV#5` | DEFERRED | could | S | Whether the Vulkan surface-format fallback, a fixed `compositeAlpha` and an unapplied `preTransform` are defects (Vulkan review 6.2). Live only on Android; reopens with that port. |
| `REV#9` | DEFERRED | could | M | Input recording and replay for LineSweeper, with a rules and file version (gpt6 opportunity 3). T1: waits for a client that needs it. |
| `REV#10` | DEFERRED | could | M | A frame-time overlay: the distribution, sprites submitted, particles dropped (gpt6 opportunity 5). T1: waits for a client that needs it. The measurement under it is `Application::frame_timings()` (PI-19); the overlay itself still waits. |

## ColourWars and the pin

**Owner:** [docs/product/STORIES.md](docs/product/STORIES.md); the drafts in ColourWars' `docs/review/engine-reports.md`, in its own repository. [docs/review/colourwars-reports/](docs/review/colourwars-reports/README.md) records the original fifteen answers and the two newer drafts; its table is kept current.

| id | status | pri | size | item |
|---|---|---|---|---|
| `DOC#4` | OPEN | should | S | ColourWars is public (2026-11-03). Fixed: README.md (links it), AGENTS.md, colourwars-reports' README, web.md §6, review INDEX, website plan. Left: whether the site links it; report links reviewed within the 2026-10-27 disclosure boundary. |
| `PI-14` | DECISION | should | M | Sandbox run at `v0.2.0` reached a keyboard match. Radeon criterion: not run, none available; the pad match is informal play. Run and record are ColourWars'. Owner: close as delegated, or await its `v0.3.0` run. [BACKLOG](docs/product/BACKLOG.md#next). |

## Records and design decisions

**Owner:** each row's document; the owner's open decisions are [BACKLOG.md's table](docs/product/BACKLOG.md#decisions-only-the-owner-can-make).

| id | status | pri | size | item |
|---|---|---|---|---|
| `SRL-07` | BLOCKED | could | S | This tree's constants are renamed and CHANGES.md lists the public spellings; migrate the ColourWars callers at its next pin move and record the counted caller repositories. Waits on that pin move. [Scope](docs/product/BACKLOG.md#later). |
| `OCA-66` | DECISION | could | S | DPI declaration implemented. Closes when `CI#4` checks it and the owner has informally played the template on a screen at 150% scaling. [Re-plan](docs/product/BACKLOG.md#later). |

## CI

**Owner:** [.github/workflows/ci.yml](.github/workflows/ci.yml), [docs/dependency-updates.md](docs/dependency-updates.md) and the story each row names. A new required job or matrix entry needs the owner to update branch protection.

| id | status | pri | size | item |
|---|---|---|---|---|
| `CI#3` | OPEN | should | S | Check CHANGES.md's rule per commit in the lint job and the release audit: a commit touching the listed engine files touches CHANGES.md, says `not consumer-visible`, or is a recorded exception. `OCA-68`'s third criterion. [Note](docs/product/BACKLOG.md#now). |
| `CI#4` | OPEN | could | S | Assert PerMonitorV2 in the embedded manifest of the built samples and of the installed Get Started game (`check_release.py`), so CI proves the DPI declaration on every change. `OCA-66`'s automated half. |

## Features other engines have

**Owner:** [the engine comparison of 2026-10-27](docs/survey/2026-10-27.md), which holds what each involves and what was left out. Every row is the owner's call; a yes becomes a story in [BACKLOG.md](docs/product/BACKLOG.md) before the work starts.

| id | status | pri | size | item |
|---|---|---|---|---|
| `GAP#1` | DECISION | should | L | PlayStation, Switch and generic pads on Windows, and more than four: built on Windows.Gaming.Input or GameInput, or SDL's gamepad layer bought (T9). |
| `GAP#2` | DECISION | should | L | Streamed, compressed music (Ogg or MP3) on XAudio2, Web Audio and null; decide after `OCA-40` states what PCM music costs. |
| `GAP#3` | DECISION | could | L | Render to texture: integer-scaled pixel art, transitions, minimaps; five backends and new goldens. |
| `GAP#4` | DECISION | could | M | Load PNG and other common image formats, premultiplied at load, so texconv leaves the first hour. |
| `GAP#5` | DECISION | could | M | A clip rectangle inside a view, for scrolling lists and panels. |
| `GAP#6` | DECISION | could | M | Circles, polygons and debug drawing of collision shapes; rectangles and lines already draw from `engine:white`. |
| `GAP#7` | DECISION | could | M | Let a game make or update a texture from pixels at run time; `resource_factory.h` already has the backend half. |
| `GAP#8` | DECISION | could | S | Easing functions, extracted from what LineSweeper and ColourWars already hand-roll (T1). |
| `GAP#9` | DECISION | could | M | Camera rotation, with culling's visible rectangle grown to cover it. |
| `GAP#10` | DECISION | could | S | LineSweeper's particle field as a component, if a second client would copy it. |
| `GAP#11` | DECISION | could | M | Tilemaps and Tiled import; waits for a client that uses tiles. |
| `GAP#12` | DECISION | could | L | TrueType and OpenType fonts; likely folded into `OCA-43` rather than a story of its own. |
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
| `IDEA#6` | DECISION | could | S | Extend OCA-50 with gallery click/tap rows and slider dragging; keep capture policy in the page. |
| `IDEA#7` | DECISION | should | S | Circle collision in the narrow phase, which throws for a Circle today: the closest-point axis, under the same Manifold contract. |
| `IDEA#8` | DECISION | could | M | Scene queries: the first collision object along a segment, and every object overlapping a rectangle, mask-filtered and allocation-free. |
| `IDEA#9` | DECISION | could | S | A contact begin/continue/end helper over two ticks' contact lists; the collision sample derives trigger entry by hand. Needs a second client. |
| `IDEA#10` | DECISION | should | S | Arbitrary client sizes: a Vector2I set_resolution and options field beside the four 16:9 presets, which are all a game can request today. |
| `IDEA#11` | DECISION | could | M | Sounds from WAV files and from in-memory PCM through one new seam verb on all three audio backends, so a first sound needs no XWBTool. |
| `IDEA#12` | DECISION | could | M | Animation sequences of named frames with per-frame durations beside today's horizontal, uniformly timed strips. |
| `IDEA#13` | DECISION | could | S | A pointer hit test over a focus group's enabled bounds, which LineSweeper's pause menu and IDEA#6 would each write by hand. |
| `IDEA#14` | DECISION | could | M | A requested present interval and an unfocused render-rate cap; vsync is a constant the pacing records assume. The owner's call first. |
| `IDEA#15` | DECISION | could | S | The display's refresh rate on RenderDeviceInfo and in the frame bench's record, in place of the pacing method's operator note. |
| `IDEA#16` | DECISION | could | S | A run-time window title, and a window icon named in options, diagnosed when absent, in the template's resource file and the release guide. |
| `IDEA#17` | DECISION | could | S | System cursor shapes (hand, text, crosshair, move) on Win32 and the canvas; today the pointer is the arrow or hidden. |
| `IDEA#18` | DECISION | could | XS | A side-by-side two-player layout in ViewportManager, which stacks the panes today. |
| `IDEA#19` | DECISION | could | XS | RectangleF::intersection_of and shortest-arc angle helpers; could ride GAP#8. |

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
| The rest of Later (L1 to L40) | [BACKLOG.md, Later](docs/product/BACKLOG.md#later) | its next ready batch reaching the front under the scheduling amendment; the hygiene lane runs alongside product work |
| Won't this time | [BACKLOG.md](docs/product/BACKLOG.md#wont-this-time) | a re-plan of the backlog with its reason recorded there |
| web.md's milestones M1, M2, M5, M6 and its decisions | [docs/port/web.md](docs/port/web.md) sections 8 and 9 | a regression, or a change the port meets that a decision must settle |
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
