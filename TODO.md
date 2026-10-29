# TODO: open work

An index of open work, one row per item; each row points to the document that owns the detail. The rules for editing it are at the end, in [How to read this file](#how-to-read-this-file). A fresh thread starts here: read **NOW**, then the owner of the row it picks up. The order of stories and their horizons is [docs/product/BACKLOG.md](docs/product/BACKLOG.md)'s, the one file in `docs/product/` kept current, and this file does not change it.

## NOW

**The only ordering in this file.** Keep every `DECISION` row here. Keep this block to the current aim and the next few actions; results go in their owners, and a closed story is a ticked box in BACKLOG.md.

**Now is finished**: every batch of [BACKLOG.md, Now](docs/product/BACKLOG.md#now), N1 to N21, has landed, and `v0.1.0` and `v0.2.0` are cut.

**Aim:** finish Next's remaining acceptance while taking ready Later work in
parallel, as [BACKLOG.md's scheduling amendment](docs/product/BACKLOG.md#later)
allows. Next's hardware and release gates remain open. The immediate queue is:

1. **L7 (`BDP-15`, `BDP-16`, `BDP-17`): finish context recovery and browser acceptance.** Failure wording and support probes have [source and offline coverage](docs/port/2026-10-28-demo-host.md); generated-module/browser checks remain open.
2. **L8 (`BDP-11` to `BDP-14`): run packaged-browser acceptance for focus, controls and phone instructions.** [Source checks](docs/port/2026-10-28-demo-host.md#focus-controls-and-the-phone-gate-l8) cover the state transitions and host policy.
3. **L11 (`BDP-18`): check repeated play stays fast.**

**Available after those batches:** text examples and validation tools are
indexed below. The animation example and gallery settings have source/native
checks and await rendered/browser acceptance.
Pointer-menu and recording-renderer guides and LineSweeper's include-layer
check are complete; strict asset keys have source/native coverage and await
wasm qualification. Follow Later's order and each story's prerequisites;
an open hardware gate does not block unrelated work.
**Hygiene has a separate lane:** [Code hygiene](docs/code-hygiene.md) owns the
remaining backend failure regressions and qualification; `SRL-21` awaits its
capture review and both test splits await their stopped runtime checks.
BACKLOG's L36 to L52 let it run alongside product work.
`SM-25` still needs observation of a scheduled run; no manual dispatch is
required by this plan.

**Before `v0.3.0` is cut:** `SITE#3` finishes accepting the selector and index in real browsers, on a phone and with a screen reader, then publishes the first snapshot. The owner chose `v0.3.0`, with no backfill for earlier releases; [the policy](docs/website-versioning.md) owns the release checks.

**Decisions and sessions only the owner can do.** Every `DECISION` row below, in the order of the backlog's horizons:
- Next: `OCA-48` + `BDP-06` (the physical pad session, which can also feel `PI-21`'s vibration, and play `PI-14`'s pad match) · `SM-09` (the MacBook Air's Safari ceiling) · `SM-01` (Radeon driver and per-app GPU selection).
- Later: `WEB#3` (listen to the audio sample), `OCA-66` (review the template at 150% scaling).
- Other acceptance: `SITE#3` (remaining browser/device/screen-reader acceptance and publication), `REV#3` (display change), `REV#8` (audio endpoint loss/recovery).
- Not yet placed: `GAP#1` to `GAP#13`, whether to take up each feature the engine comparison found; a yes becomes a story in BACKLOG.md.
- New proposals: `IDEA#1` to `IDEA#6` in [Feature candidates](docs/feature-candidates.md): typed text, rebinding, save recovery, camera following, grid-sheet authoring and gallery pointer interaction.

## Code hygiene

**Owner:** [Code hygiene](docs/code-hygiene.md) holds the original review
targets and acceptance; new bounded repairs have their owners linked below.
[BACKLOG.md, Later](docs/product/BACKLOG.md#later), L36 to L52, orders the
maintenance lane. Counts prompt review, not automatic splitting.

| id | status | pri | size | item |
|---|---|---|---|---|
| `SRL-21` | BLOCKED | should | S | Bounds fix and native-null regressions pass; regenerate and review Release captures after GPU access is restored. [Evidence](docs/product/BACKLOG.md#srl-21). |
| `SM-49` | BLOCKED | should | M | Harness and five case groups preserve source/asset inventories; all native builds/headless checks pass. Pixel execution and wasm GL/S3TC qualification remain at stopped device/Emscripten gates. [Evidence](docs/code-hygiene.md#sm-49-pixel-test-organisation). |
| `SM-51` | BLOCKED | could | S | Subject split preserves 41 methods; 71 permitted cases pass. Full suite/Infrastructure qualification waits on stopped PowerShell fixtures. [Evidence](docs/code-hygiene.md#sm-51-cloud-tooling-test-organisation). |
| `SM-69` | BLOCKED | should | S | Glyph-extent overflow guard and native regressions pass all five Debug render backends and Release null; wasm/full-CI qualification remains open. [Evidence](docs/product/BACKLOG.md#sm-69). |
| `SM-56` | OPEN | should | S | Three Vulkan cleanup fixes compile on all native backends; finish wrapper/recovery failure regressions and stopped device qualification. Shader-failure case compiles but is unrun. |
| `SM-57` | OPEN | should | S | D3D12 replacement publication now precedes its descriptor write; finish failure regression and stopped reload/descriptor/device qualification. Native builds/headless checks pass. |
| `SM-58` | OPEN | could | M | Extract the reviewed D3D12 root-signature/PSO value owner after SM-57; keep frame lifetime coordinated. |

## Input, menus, settings and saves

**Owner:** [STORIES.md](docs/product/STORIES.md); order in [BACKLOG.md](docs/product/BACKLOG.md), Next X13/X14 and Later L19/L20.

| id | status | pri | size | item |
|---|---|---|---|---|
| `OCA-45` | BLOCKED | should | S | Any-slot sample and guide implemented; record a physical two-pad run after unplugging slot 0, with date and commit. |
| `PI-21` | DECISION | should | S | Vibration implemented and headlessly tested; record each motor, stop, background, disconnect, reconnect and shutdown on a physical pad, natively and in Chrome. Can ride `OCA-48`'s session. [Acceptance](docs/review/colourwars-reports/vibration.md#acceptance-and-evidence). |
| `OCA-46` | BLOCKED | could | M | Join screen, routing and native-null cases pass; desktop visual review and browser build/runtime acceptance remain at the stopped GPU/Emscripten gates. [Evidence](docs/product/BACKLOG.md#later). |
| `OCA-51` | BLOCKED | could | M | Two-mode fullscreen request/re-sync and the sound-group guide are implemented; native builds, headless row tests and guide compilation pass. Actual fullscreen/browser acceptance remains at stopped gates. [Evidence](docs/product/BACKLOG.md#later). |

## Content, text and sample examples

**Owner:** [BACKLOG.md, Later](docs/product/BACKLOG.md#later), L21 to L23,
L26 and L35. Existing story acceptance is in [STORIES.md](docs/product/STORIES.md);
the two new stories have their detail in BACKLOG.md.

| id | status | pri | size | item |
|---|---|---|---|---|
| `OCA-41` | BLOCKED | could | S | UTF-8 settings and the permitted native scratch consumer pass; complete wasm/Node and full-CI acceptance. The committed-literal reservation is retained. [Record](docs/2026-10-29-literal-encoding.md). |
| `OCA-42` | BLOCKED | could | M | Compiled JSON-to-owned-text guide, glyph checks and null submissions pass; finish authored matching-atlas and browser acceptance. [Record](docs/2026-10-29-string-table-text.md). |
| `OCA-77` | BLOCKED | could | S | Measured word wrapping and fifteen new cases pass all five native Debug backends and Release null; wasm/full-CI qualification remains open. [Record](docs/2026-10-29-word-wrap.md). |
| `PI-20` | BLOCKED | could | S | Key enumeration and strict manifest/sprite-sheet fields pass native Debug/null/Release cases; wasm/Node qualification remains at stopped Emscripten gates. [Evidence](docs/2026-10-29-strict-asset-keys.md). |
| `SRL-20` | BLOCKED | could | M | Copyable strip player, gallery overlay, original content and native timing/loader/recording/state checks are implemented. Wasm/page checks and rendered-frame review remain at stopped gates. [Evidence](docs/product/BACKLOG.md#srl-20). |

## Website and the hosted demo

**Owner:** [website/README.md](website/README.md) and [docs/website-plan.md](docs/website-plan.md); the hosted demo in [docs/port/web.md](docs/port/web.md) section 7 and [docs/port/web-to-be-tested.md](docs/port/web-to-be-tested.md).

| id | status | pri | size | item |
|---|---|---|---|---|
| `SITE#3` | DECISION | must | M | Left: Safari, real-phone touch and screen-reader acceptance; publish `v0.3.0`'s snapshot and check the deploy. [Scope and checks](docs/website-versioning.md). |
| `OCA-18` | BLOCKED | could | S | Visual Studio guide and live breakpoint/content excerpts are implemented; follow it in a clean copied project, hit the breakpoint and load content, recording date and exact IDE version. [Evidence](docs/product/BACKLOG.md#later). |
| `SM-26` | BLOCKED | could | S | Committed-host lag checker and re-sync trigger are implemented with 25 passing focused tests. Actual check stopped at Labrador's Git ownership gate before reading the host; owner-context qualification remains. [Contract](docs/port/web.md#7-what-the-host-page-gets). |
| `SM-29` | BLOCKED | could | S | Artifact-preview and production-check steps are documented; the plan distinguishes failed-build preservation from rollback. Current-provider rollback steps and live acceptance remain unverified. [Procedure](website/README.md#preview-and-recovery). |
| `NMS-03` | BLOCKED | could | S | Why links the unbuilt Android plan and states its scope and five support gates; the remote GitHub response remains unverified under the network stop. [Evidence](docs/product/BACKLOG.md#later). |

`SM-30` remains the owner-only gate follow-up after `SM-29` in [BACKLOG.md, Later](docs/product/BACKLOG.md#later).

## CI

**Owner:** [.github/workflows/ci.yml](.github/workflows/ci.yml) for the `CI#` rows; stories in [docs/product/STORIES.md](docs/product/STORIES.md).

| id | status | pri | size | item |
|---|---|---|---|---|
| `SM-25` | WAITING | should | S | Observe the next scheduled Actions run; repaired SDK download and tests passed PR CI. [Evidence](docs/product/BACKLOG.md#next). |
| `WEB#8` | OPEN | could | XS | Optional: run CI once with `allowed_channel_drift` at 0 to measure the runner's adapter, so the allowance can come down from 8. |
| `SM-17` | OPEN | could | M | Render planner, preflight and explicit first-failure executor are implemented with fake-command checks; complete real CTest/report compatibility and the sweep after stopped runtime/toolchain gates. [Scope](docs/verification.md). |
| `SM-18` | BLOCKED | could | S | Record device checks CI cannot run for each change; after SM-17. |
| `SM-24` | BLOCKED | could | S | Pin/selector inventory, coupled consumer updates and full qualification procedure are documented. Preserve the after-SM-25 scheduled-run observation gate. [Procedure](docs/dependency-updates.md). |
| `NMS-04` | BLOCKED | could | S | Native engine/copied-game presets have the Windows host gate, with planner, diagnostic and Windows listing checks. Record the required real Ubuntu/WSL listing; non-Windows wasm remains unverified. [Evidence](docs/product/BACKLOG.md#later). |

`SM-15` remains in [BACKLOG.md, Later](docs/product/BACKLOG.md#later).

## Performance

**Owner:** [docs/review/backend-equivalence-2/STATUS.md](docs/review/backend-equivalence-2/STATUS.md)'s low-tier box; [the measurement decision](docs/performance/2026-09-19-g6f-ratchet.md#measurement-decision-sm-03) assigns each claim its evidence. [The workload/cadence record](docs/performance/2026-10-26-workload-and-cadence.md) owns the completed PERF#1 to PERF#3 corrections; read the ratchet document before the hardware tables.

| id | status | pri | size | item |
|---|---|---|---|---|
| `SM-01` | DECISION | should | S | Select the Radeon iGPU using AMD's driver and Windows' per-app GPU preference, then verify the adapter chosen by the bench. |
| `SM-07` | BLOCKED | should | M | Record Radeon CPU-phase and cadence p99s plus separate pipeline-latency evidence under the four-core hold (D3D12/Vulkan). Needs `SM-01`, `SM-02`, `SM-04`, `SM-05`; EC2 runs do not count. |

## Browsers, phones and hardware

**Owner:** [docs/port/web-to-be-tested.md](docs/port/web-to-be-tested.md) for the `WEB#` rows and M3's done-when in [docs/port/web.md](docs/port/web.md) section 9; stories in [docs/product/STORIES.md](docs/product/STORIES.md). The owner's hardware is listed at the top of BACKLOG.md.

| id | status | pri | size | item |
|---|---|---|---|---|
| `OCA-48` | DECISION | should | S | One owner session with the physical XInput pad: on the desktop, each button and stick direction and a backgrounded sample (`OCA-48`); the bundle page in Firefox, and in Chrome the browser pad, a real tab switch and a real context loss (`BDP-06`). |
| `SM-09` | DECISION | should | S | The owner records the highest macOS and Safari the 2013 MacBook Air takes, and plays the live demo there if it meets the wasm-exceptions floor. |
| `WEB#1` | BLOCKED | should | M | M3's Safari half: all six sample pages in desktop Safari (drawing, keys, quit, context loss and restore), and whether Safari masks `RENDERER`. Blocked on `SM-09`'s ceiling; R5 is `BDP-07`'s. |
| `WEB#3` | DECISION | should | S | Hear the audio sample through speakers (each sound, pan, pitch, volume, held keys), the first-gesture unlock in each browser, resume after a hidden tab, and `ping_adpcm.xwb` on XAudio2. Nobody has listened yet. |
| `SM-08` | BLOCKED | should | S | Instrument/fixture source and headless checks implemented; generated-output review and Release flag/PageTests runtime acceptance await access. [Method and limits](docs/port/pacing-method.md); `BDP-07`/`SM-13` wait on it. |
| `BDP-07` | BLOCKED | could | M | Measure the fixed 60 Hz step under `requestAnimationFrame` on a 60 Hz and a high-refresh display (R5), M3's second open term. After `SM-08`; may need a borrowed display. |
| `WEB#6` | BLOCKED | could | XS | Record hosted-stage scaling, zoom and HUD legibility after local HTTPS access is restored. The read-only HEAD was socket-permission denied. [Evidence](docs/port/web-to-be-tested.md). |
| `SM-13` | BLOCKED | should | M | Play the development page and the live demo on the owner's iPhone and record layout, rotation, touch, legibility, load time, memory (R4) and the top-out frame (R6). After `SM-14` (serving to a phone on the LAN) and `SM-08`. |
| `BDP-10` | BLOCKED | could | XS | Lift the bundle page's phone gate after `SM-13` and `BDP-09`. The owner approved iPhone-only evidence; Android is not a prerequisite. [Decision](docs/product/BACKLOG.md#decisions-only-the-owner-can-make). |
| `SM-44` | DEFERRED | could | S | Optional future real-phone record on a mid-range Android phone in Chrome. Waits on a borrowed phone; none is accessible. Does not gate iPhone acceptance. |
| `WEB#2` | BLOCKED | could | S | A standard-mapping pad in Firefox and Safari (Chrome is `BDP-06`'s), and unplug and replug with more than four pads seen. Firefox can ride `OCA-48`'s session; Safari waits on `SM-09`, and five pads are not owned. |
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

**Owner:** [docs/product/STORIES.md](docs/product/STORIES.md); the drafts in ColourWars' `docs/review/engine-reports.md`, which is private. [docs/review/colourwars-reports/](docs/review/colourwars-reports/README.md) records the original fifteen answers and the two newer drafts; its table is kept current.

| id | status | pri | size | item |
|---|---|---|---|---|
| `DOC#4` | WAITING | should | S | When ColourWars' repository goes public: sweep "private" and "not public" mentions outside dated records, link it from the README and site, and review report links within the approved disclosure boundary. |
| `PI-14` | DECISION | should | M | Sandbox run reached a keyboard match and exited 0 (2026-10-28, [BACKLOG](docs/product/BACKLOG.md#next)); left: one match on the owner's pad, the Radeon run after `SM-01`, and the record in ColourWars. |

## Records and design decisions

**Owner:** each row's document; the owner's open decisions are [BACKLOG.md's table](docs/product/BACKLOG.md#decisions-only-the-owner-can-make).

| id | status | pri | size | item |
|---|---|---|---|---|
| `SRL-07` | BLOCKED | could | S | This tree's constants are renamed and CHANGES.md lists the public spellings; migrate the ColourWars callers at its next pin move and record the counted caller repositories. Needs the private repository. [Scope](docs/product/BACKLOG.md#later). |
| `OCA-66` | DECISION | could | S | The samples/template declare per-monitor DPI awareness and the release guide states it; review and record the template at 150% scaling. [Scope](docs/product/BACKLOG.md#later). |

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
| The rest of Later (L1 to L40) | [BACKLOG.md, Later](docs/product/BACKLOG.md#later) | its next ready batch reaching the front under the scheduling amendment; the hygiene lane runs alongside product work |
| Won't this time | [BACKLOG.md](docs/product/BACKLOG.md#wont-this-time) | a re-plan of the backlog with its reason recorded there |
| web.md's milestones M1, M2, M5, M6 and its decisions | [docs/port/web.md](docs/port/web.md) sections 8 and 9 | a regression, or a change the port meets that a decision must settle |
| macOS and iOS | [docs/port/android.md](docs/port/android.md) sections 5 and 6 | a plan document; none is in the tree |
| The EC2 performance lane | [tools/cloud_performance/README.md](tools/cloud_performance/README.md) | a new declared reference run, or a regression question |
| round-2 plan, the RTCD list, the first review, backend-equivalence | [docs/review/round-2/PLAN.md](docs/review/round-2/PLAN.md), [docs/review/rtcd/](docs/review/rtcd/), [docs/review/IMPLEMENTED.md](docs/review/IMPLEMENTED.md) | nothing: all landed or superseded; the game-side rows live in ColourWars |
| The 2026-09-26 review's numbered findings and hardening candidates | [docs/review/2026-09-26/STATUS.md](docs/review/2026-09-26/STATUS.md) and [the review index](docs/review/INDEX.md#hardening-verification) | nothing: all 23 implemented and four hardening candidates addressed; `REV#12` remains |
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
