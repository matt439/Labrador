# TODO: open work

An index of open work, one row per item; each row points to the document that owns the detail. The rules for editing it are at the end, in [How to read this file](#how-to-read-this-file). A fresh thread starts here: read **NOW**, then the owner of the row it picks up. The order of stories and their horizons is [docs/product/BACKLOG.md](docs/product/BACKLOG.md)'s, the one file in `docs/product/` kept current, and this file does not change it.

## NOW

**The only ordering in this file.** Keep every `DECISION` row here. Keep this block to the current aim and the next few actions; results go in their owners, and a closed story is a ticked box in BACKLOG.md.

**Now is finished**: every batch of [BACKLOG.md, Now](docs/product/BACKLOG.md#now), N1 to N21, has landed, and `v0.1.0` and `v0.2.0` are cut.

**Aim:** the backlog's Next horizon, *Pin, test, ship*: "an adopter moves a pin by procedure, tests their game, makes content, ships a Windows release with its runtime and notices, starts a browser build and reads a reference that gives the contract". [BACKLOG.md, Next](docs/product/BACKLOG.md#next) owns its *Done when* list and its batches, X1 to X28; this block names the first ones the owner does not gate, in that order (X3, X5 and X6 are the owner's and are listed below).

1. **`SM-43` (X1): brief trimmed; full tooling-suite acceptance is blocked by Windows script execution policy.** [Verification](docs/2026-10-26-agent-brief-history.md#verification-and-remaining-acceptance).
2. **`OCA-44`, `OCA-45`, `OCA-49` (X13): controller slots, any-slot single-player input and disabled menu rows.** Earlier unclosed batches need owner hardware or another repository.
3. **`OCA-53` (X14): settings and saves on Windows and in the browser.**

**Decisions and sessions only the owner can do.** Every `DECISION` row below, in the order of the backlog's horizons:
- Next: `SM-03` (which p99 column) · `OCA-08` (accessibility position, amendment C) · `PI-09` (what ColourWars detail may be published) · `OCA-48` + `BDP-06` (the physical pad session) · `SM-09` (the MacBook Air's Safari ceiling).
- Later: `BDP-20` (the phone fallback video) · `BDP-10` (whether the iPhone alone lifts the phone gate) · `SITE#3` (versioning at the first tag) · `SRL-14` (the C++ series) · `SRL-07` (constants naming) · `OCA-66` (DPI awareness) · `WEB#3` (listen to the audio sample).

## Input, menus, settings and saves

**Owner:** [STORIES.md](docs/product/STORIES.md); order in [BACKLOG.md, Next](docs/product/BACKLOG.md#next), X13 and X14.

| id | status | pri | size | item |
|---|---|---|---|---|
| `OCA-44` | OPEN | should | XS | Document Windows controller support and slot assignment in the input guide. |
| `OCA-45` | OPEN | should | S | Make the minimal sample accept any connected controller slot; verify the physical two-pad slot-0 unplug case. |
| `OCA-49` | OPEN | could | XS | Make the default disabled menu row visibly unavailable. |
| `OCA-53` | OPEN | should | M | Add a settings/saves guide with per-user Windows JSON and verified IDBFS reload persistence. |

## Website and the hosted demo

**Owner:** [website/README.md](website/README.md) and [docs/website-plan.md](docs/website-plan.md); the hosted demo in [docs/port/web.md](docs/port/web.md) section 7 and [docs/port/web-to-be-tested.md](docs/port/web-to-be-tested.md).

| id | status | pri | size | item |
|---|---|---|---|---|
| `SM-32` | OPEN | could | S | Re-encode `linesweeper.mp4` (still Courier; the stills are JetBrains Mono) with LineSweeperCapture on `x64-release` and FFmpeg, review it, and state the same-commit rule in website/README.md. |
| `SITE#3` | DECISION | could | XS | The owner decides whether the site starts versioned snapshots at the first 0.x tag ([docs/website-versioning.md](docs/website-versioning.md)). |
| `BDP-20` | DECISION | could | XS | The owner decides whether the 4.3 MB phone fallback video stays, or swaps in a smaller encode with `npm run sync-demo -- <bundle> --video <mp4>`. |

The rest of the site's operations stories (`SM-26`, `SM-27`, `SM-29`, `SM-30`) are in [BACKLOG.md, Later](docs/product/BACKLOG.md#later).

## CI

**Owner:** [.github/workflows/ci.yml](.github/workflows/ci.yml) for the `CI#` rows; stories in [docs/product/STORIES.md](docs/product/STORIES.md).

| id | status | pri | size | item |
|---|---|---|---|---|
| `SM-25` | OPEN | should | S | Pin the Vulkan SDK to an exact version instead of `latest`, log the browser's product and version from `run_page.mjs`, and add a scheduled run so outside drift is a red run of its own. |
| `SM-19` | OPEN | could | XS | Run `python -m unittest discover` over `tools/tests` on every push, on the Windows runner, and add a ticked row for README priority 3 to `docs/review/2026-09-26/STATUS.md`. |
| `CI#2` | OPEN | could | XS | Run `node --test tools/flash_check/flash_check.test.mjs` in CI beside `SM-19`'s step; the WCAG flash definitions are otherwise held to their synthetic frames only when someone runs it. Owned by samples/linesweeper/README.md. |
| `WEB#8` | OPEN | could | XS | Optional: run CI once with `ALLOWED_CHANNEL_DRIFT` at 0 to measure the runner's adapter, so the allowance can come down from 8. |

`SM-15`, `SM-17`, `SM-18`, `SM-23` and `SM-24` are in [BACKLOG.md, Later](docs/product/BACKLOG.md#later).

## Performance

**Owner:** [docs/review/backend-equivalence-2/STATUS.md](docs/review/backend-equivalence-2/STATUS.md)'s low-tier box; the `PERF#` rows in [docs/performance/](docs/performance/), the ratchet document first.

| id | status | pri | size | item |
|---|---|---|---|---|
| `SM-03` | DECISION | should | XS | The owner decides which p99 column the pipeline-depth claims are judged by, and whether that lands in PHILOSOPHY (amendment D) or the ratchet document's section 5. It unblocks `SM-01`, `SM-02` and `SM-07`. |
| `SM-07` | BLOCKED | should | M | Take and publish the low-tier p99 on the Radeon iGPU under the four-core hold, D3D12 and Vulkan, and tick STATUS.md's box. Needs `SM-03`, `SM-01` (AMD's driver, per-app GPU preference), `SM-02`, `SM-04`, `SM-05`; EC2 runs do not count. |
| `PERF#1` | OPEN | should | XS | Record that `full_well_top_out` changed (no stack at top-out, smaller sparks), so later runs are not compared with `g6f-reference-003`/`004`; consider a new workload id in the result JSON. |
| `PERF#2` | OPEN | could | XS | Make the ratchet document's section 5 name the fields `analysis.py` emits (`presentation_wait_candidate_repetitions`, `work_limited`, `ambiguous_cadence`), not `presentation_locked_repetitions`. |
| `PERF#3` | OPEN | could | S | Reconcile the ratchet document's section 5 ("should not pool a locked repetition with a free one") with `analysis.py`, which pools every repetition and only warns: report the pools apart, or amend section 5. |

## Browsers, phones and hardware

**Owner:** [docs/port/web-to-be-tested.md](docs/port/web-to-be-tested.md) for the `WEB#` rows and M3's done-when in [docs/port/web.md](docs/port/web.md) section 9; stories in [docs/product/STORIES.md](docs/product/STORIES.md). The owner's hardware is listed at the top of BACKLOG.md.

| id | status | pri | size | item |
|---|---|---|---|---|
| `OCA-48` | DECISION | should | S | One owner session with the physical XInput pad: on the desktop, each button and stick direction and a backgrounded sample (`OCA-48`); the bundle page in Firefox, and in Chrome the browser pad, a real tab switch and a real context loss (`BDP-06`). |
| `SM-09` | DECISION | should | S | The owner records the highest macOS and Safari the 2013 MacBook Air takes, and plays the live demo there if it meets the wasm-exceptions floor. |
| `WEB#1` | BLOCKED | should | M | M3's Safari half: all six sample pages in desktop Safari (drawing, keys, quit, context loss and restore), and whether Safari masks `RENDERER`. Blocked on `SM-09`'s ceiling; R5 is `BDP-07`'s. |
| `WEB#3` | DECISION | should | S | Hear the audio sample through speakers (each sound, pan, pitch, volume, held keys), the first-gesture unlock in each browser, resume after a hidden tab, and `ping_adpcm.xwb` on XAudio2. Nobody has listened yet. |
| `WEB#4` | OPEN | could | S | Run `WebWindowTests` and the five other sample pages in Firefox, with the shell's CSS; only LineSweeper's development page has run there. |
| `SM-08` | OPEN | should | S | A repeatable method for judging browser frame pacing (R5) and timing the 9,600-particle top-out on a phone (R6). `BDP-07` and `SM-13` wait on it. |
| `BDP-07` | BLOCKED | could | M | Measure the fixed 60 Hz step under `requestAnimationFrame` on a 60 Hz and a high-refresh display (R5), M3's second open term. After `SM-08`; may need a borrowed display. |
| `WEB#6` | OPEN | could | XS | Check the demo's stage at 1x, 1.25x, 1.5x and 2x scaling and under browser zoom, and record whether the HUD is readable below 1280x720. |
| `SM-13` | BLOCKED | should | M | Play the development page and the live demo on the owner's iPhone and record layout, rotation, touch, legibility, load time, memory (R4) and the top-out frame (R6). After `SM-14` (serving to a phone on the LAN) and `SM-08`. |
| `BDP-10` | DECISION | could | XS | The owner decides whether the iPhone's evidence alone may lift the bundle page's phone gate, or an Android run (`SM-44`) is required. The lift itself also waits on `SM-13` and `BDP-09`. |
| `SM-44` | DEFERRED | could | S | The same real-phone record on a mid-range Android phone in Chrome. Waits on a borrowed phone; none is owned. |
| `WEB#2` | BLOCKED | could | S | A standard-mapping pad in Firefox and Safari (Chrome is `BDP-06`'s), and unplug and replug with more than four pads seen. Firefox can ride `OCA-48`'s session; Safari waits on `SM-09`, and five pads are not owned. |
| `WEB#5` | BLOCKED | could | S | Touch on real glass: several fingers with distinct ids, no tap also a click, foreground loss cancelling every point, LineSweeper's buttons through `WM_POINTER`, a pen as the mouse, Firefox and Safari. Needs a touchscreen. |

## Engine verification and reviews

**Owner:** each row's review under [docs/review/](docs/review/); story `SM-16` (a `docs/review/INDEX.md`, [BACKLOG.md, Next](docs/product/BACKLOG.md#next)) would track `REV#3` to `REV#6`.

| id | status | pri | size | item |
|---|---|---|---|---|
| `REV#7` | OPEN | should | M | Presentation-lifecycle coverage: several presented frames with no per-frame read-back (real frames in flight on D3D12 and Vulkan), a resize, and teardown with work outstanding, under D3D12 debug and Vulkan `validate_sync`. gpt6 opportunity 2. |
| `REV#1` | OPEN | could | S | An AppTests case where a live `Application` with a real window resizes and `fullscreen_viewport()` still matches `back_buffer_size()`; backend-equivalence-2's G6a, its last open section-3 box. |
| `REV#8` | OPEN | could | S | Recover XAudio2 when its output device goes away: `AudioDevice::update()` ignores `Update()`'s false and never calls `IsCriticalError()`/`Reset()`. Then check it by hand on a real endpoint. gpt6 opportunity 4. |
| `REV#11` | OPEN | could | M | Check the 2026-09-26 review's four unverified hardening candidates: one bit per modifier for both sides, `name_table`/`registry` inserting the index first, cleanup when a draw or resource creation throws, negative glyph y offsets. |
| `REV#4` | OPEN | could | XS | Minimise a sample on `x64-debug-vulkan` under the validation layers and record whether acquire or present returns `OUT_OF_DATE`, and how severe the minimise throw is (Vulkan review 6.1). |
| `REV#3` | OPEN | could | M | Check the D3D12 review's seven unverified behaviour questions in its section 6 (the view-open loop, per-flush heap-start calls, the per-submit list vector, the buffer helper, the factory-refresh branch, `sprite.hlsl`'s profile claims, multi-mip footprints). |
| `REV#6` | OPEN | could | L | Check the Vulkan review's 65 never-verified findings (section 7); about a third are counts or comment claims settled by reading. The index-buffer barrier has since been applied. |
| `REV#12` | BLOCKED | could | M | The 2026-09-26 review's manual checks: real device removal, interactive minimise, restore and drag, physical keyboard layouts and device changes, real sleep and wake. Needs a person at the machine. |
| `REV#5` | DEFERRED | could | S | Whether the Vulkan surface-format fallback, a fixed `compositeAlpha` and an unapplied `preTransform` are defects (Vulkan review 6.2). Live only on Android; reopens with that port. |
| `REV#9` | DEFERRED | could | M | Input recording and replay for LineSweeper, with a rules and file version (gpt6 opportunity 3). T1: waits for a client that needs it. |
| `REV#10` | DEFERRED | could | M | A frame-time overlay: the distribution, sprites submitted, particles dropped (gpt6 opportunity 5). T1: waits for a client that needs it. The measurement under it is `Application::frame_timings()` (PI-19); the overlay itself still waits. |

## ColourWars and the pin

**Owner:** [docs/product/STORIES.md](docs/product/STORIES.md); the drafts in ColourWars' `docs/review/engine-reports.md`, which is private. The engine's decision on each of the fifteen live drafts, made ahead of filing so `PI-10` can be implemented from it, is [docs/review/colourwars-reports/](docs/review/colourwars-reports/README.md), whose table is kept current.

| id | status | pri | size | item |
|---|---|---|---|---|
| `PI-09` | DECISION | should | S | The owner decides what private ColourWars detail may be published; then the live drafts (fifteen on 2026-10-22, the packaging gap among them) are rechecked against master and filed through the stated route, or withdrawn with a reason. `PI-10` answers them. |
| `DOC#4` | WAITING | should | S | When ColourWars' repository goes public (the owner expects it within weeks of 2026-10-13): sweep the tree's "private" and "not public" mentions of it outside dated records, link the repository from the README and the site, and recheck `PI-09`, which then narrows to what its drafts say. |
| `PI-14` | OPEN | should | M | A ColourWars Release ZIP at a current pin, run (or refused loudly) on a clean Windows install without Visual Studio, with edition, GPU, driver and commits named; `tools/consumer_check/sandbox.py` runs it. |

## Records and design decisions

**Owner:** each row's document; the owner's open decisions are [BACKLOG.md's table](docs/product/BACKLOG.md#decisions-only-the-owner-can-make).

| id | status | pri | size | item |
|---|---|---|---|---|
| `OCA-08` | DECISION | should | S | The owner records the accessibility position for PHILOSOPHY amendment C, after B: what the engine provides, whether the hosted demo has an aim, who owns the colour-blind palette. The flash reading is decided. Unblocks `BDP-24`, `BDP-21`. |
| `SRL-14` | DECISION | could | S | The owner says whether the "Introduction to C++" series is not started, planned with an outline, or withdrawn. |
| `SRL-07` | DECISION | could | S | The owner chooses between amending CONVENTIONS to accept the 26 SCREAMING constants and renaming them in both repositories. |
| `OCA-66` | DECISION | could | S | The owner decides whether the default DPI awareness stays unaware or becomes per-monitor aware. |
| `SM-43` | BLOCKED | should | M | Brief and history relocation implemented; full tools-suite acceptance needs Windows to permit the PowerShell fixtures ([verification](docs/2026-10-26-agent-brief-history.md#verification-and-remaining-acceptance)). |
| `DOC#2` | OPEN | could | XS | `docs/survey/2026-08-26-status.md`'s section-6 item 4 still opens "Still unmade" above its own "Decided and built, 2026-10-11" (D9). Its decision 2, whether markers stay on the seam, is still unmade and has no other owner. |
| `DOC#3` | OPEN | could | XS | The file-length audit's tripwire has fired: `tests/core/state_context_tests.cpp` is 865 lines against about 650. Re-ask whether `StateContext` has a second job and record the answer in `docs/file-length-audit.md`. |

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
| The public face and the adopter path | [STORIES.md](docs/product/STORIES.md); order in [BACKLOG.md, Next](docs/product/BACKLOG.md#next) | an adopter-path story the owner gates, or NOW naming one |
| The rest of Next (X1 to X28) | [BACKLOG.md, Next](docs/product/BACKLOG.md#next) | its batch reaching the front; the rows above are the batches NOW names, and the Next stories that the owner or an outside event gates, or that a row here pairs with |
| Later (L1 to L33) | [BACKLOG.md, Later](docs/product/BACKLOG.md#later) | Next finishing |
| Won't this time | [BACKLOG.md](docs/product/BACKLOG.md#wont-this-time) | a re-plan of the backlog with its reason recorded there |
| web.md's milestones M1, M2, M5, M6 and its decisions | [docs/port/web.md](docs/port/web.md) sections 8 and 9 | a regression, or a change the port meets that a decision must settle |
| macOS and iOS | [docs/port/android.md](docs/port/android.md) sections 5 and 6 | a plan document; none is in the tree |
| The EC2 performance lane | [tools/cloud_performance/README.md](tools/cloud_performance/README.md) | a new reference run after `SM-03`, or a regression question |
| round-2 plan, the RTCD list, the first review, backend-equivalence | [docs/review/round-2/PLAN.md](docs/review/round-2/PLAN.md), [docs/review/rtcd/](docs/review/rtcd/), [docs/review/IMPLEMENTED.md](docs/review/IMPLEMENTED.md) | nothing: all landed or superseded; the game-side rows live in ColourWars |
| The 2026-09-26 review's numbered findings | [docs/review/2026-09-26/STATUS.md](docs/review/2026-09-26/STATUS.md) | nothing: all 23 implemented; only `REV#11`, `REV#12` and `SM-19` remain |
| The repository split, the file-length audit's steps | [docs/repo-split.md](docs/repo-split.md), [docs/file-length-audit.md](docs/file-length-audit.md) | nothing beyond `DOC#3` |

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
