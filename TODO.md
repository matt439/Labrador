# TODO: open work

An index of open work, one row per item; each row points to the document that owns the detail. The rules for editing it are at the end, in [How to read this file](#how-to-read-this-file). A fresh thread starts here: read **NOW**, then the owner of the row it picks up. The order of stories and their horizons is [docs/product/BACKLOG.md](docs/product/BACKLOG.md)'s, the one file in `docs/product/` kept current, and this file does not change it.

## NOW

The [November 20 integration record](docs/verification/2026-11-20-todo-integration.md)
holds the combined local results. The client tracker responses are ready for
the owner. The next section tracks the remaining hosted, Simulator and
attended checks in [BACKLOG order](docs/product/BACKLOG.md#later).

### Client tracker responses

The [client report ledger](docs/review/colourwars-reports/README.md#issues-filed-on-the-tracker-2026-11-05)
owns the dispositions. Follow [the tracker procedure](CONTRIBUTING.md#how-a-request-is-closed).

| id | status | pri | size | item |
|---|---|---|---|---|
| `#122` | OPEN | should | XS | Record the v0.4.0 scripted input answer and the declined Registry rider. [Scope](docs/review/colourwars-reports/scripted-pad.md). |
| `#123` | OPEN | should | XS | Record the shipped exception type and the startup cases mapped to it on master. [Scope](docs/review/colourwars-reports/device-unavailable.md). |
| `#124` | OPEN | should | XS | Record the mutable contact counterpart and its dispatch and migration rules. [Scope](docs/review/colourwars-reports/contact.md). |
| `#125` | OPEN | should | XS | Record default Release software refusal and the explicit WARP test option. [Scope](docs/review/colourwars-reports/device-kind.md). |

## November 16 Later batches

[BACKLOG.md](docs/product/BACKLOG.md#later) owns the order and acceptance.
The [November 17 handoff](docs/2026-11-17-overnight-handoff.md#completed-isolated-scopes-awaiting-integration)
identifies the source of the retained changes. The
[November 20 verification](docs/verification/2026-11-20-todo-integration.md)
records the integration. The [v0.4.0 record](docs/verification/releases/v0.4.0.md)
identifies what shipped.

| id | status | pri | size | item |
|---|---|---|---|---|
| `OCA-108` | BLOCKED | should | L | Qualify a bought platform pad backend with a real non-Xbox pad once one is available. [Scope](docs/product/BACKLOG.md#oca-108). |
| `SM-89` | WAITING | could | M | Retain the first hosted results for all five configurations of the published weekly workflow. [Qualification](docs/verification-weekly.md). |
| `SM-92` | WAITING | should | M | Retain hosted execution on the reviewed Action pins for the weekly and sanitizer workflows and the optional starter browser recipe. [Qualification](docs/action-runtime-updates.md#hosted-qualification-2026-11-20). |
| `OCA-119` | BLOCKED | could | S | Record actual Escape behaviour from a trusted key in a headed browser. [Attempt](docs/port/2026-11-17-browser-escape-attempt.md). [Scope](docs/product/BACKLOG.md#oca-119). |
| `SM-95` | BLOCKED | could | XS | Add the AI-use disclosure when the owner supplies public wording and review/authorship claims. [Scope](docs/product/BACKLOG.md#sm-95). |
| `SM-97` | BLOCKED | could | M | Run the integrated pilot on a Mac with Xcode, safaridriver and an available iPhone Simulator. [Pilot](docs/port/ios-simulator-pilot.md). |

## Input, menus, settings and saves

**Owner:** [STORIES.md](docs/product/STORIES.md); order in [BACKLOG.md](docs/product/BACKLOG.md), Next X13/X14 and Later L19/L20.

| id | status | pri | size | item |
|---|---|---|---|---|
| `OCA-45` | BLOCKED | should | S | Owner-attended slot acceptance shares OCA-48's pad play; headless cases stand, no second pad is required and physical unplug-slot-0 remains unverified. [Scope](docs/product/BACKLOG.md#next). |
| `PI-21` | BLOCKED | should | M | Physical native/Chrome rumble acceptance shares the pad session; automated transport evidence does not establish output or feel. [Scope](docs/review/colourwars-reports/vibration.md#acceptance-and-evidence). |

## Website and the hosted demo

**Owner:** [website/README.md](website/README.md) and [docs/website-plan.md](docs/website-plan.md); the hosted demo in [docs/port/web.md](docs/port/web.md) section 7 and [docs/port/web-to-be-tested.md](docs/port/web-to-be-tested.md).

| id | status | pri | size | item |
|---|---|---|---|---|
| `SITE#4` | BLOCKED | should | S | The search fix is deployed. On a real iPhone, press Return in search and record the result, browser, date and deployed revision. [Evidence](docs/port/web-to-be-tested.md#approved-acceptance-actions-2026-11-16). |
| `WEB#12` | WAITING | could | S | After the separately hosted demo deploy, run check_live.mjs for the intended commit and retain dated output/failures; loopback evidence is separate. [Scope](docs/port/web-to-be-tested.md). |
| `SM-30` | BLOCKED | could | S | Workers Builds read is denied to the saved OAuth credential; dashboard access is unavailable. Reconcile after access, then wait for a normal production log. [Evidence](website/README.md#sm-30-access-check-2026-11-16). |

## Performance

**Owner:** [docs/review/backend-equivalence-2/STATUS.md](docs/review/backend-equivalence-2/STATUS.md)'s low-tier box; [the measurement decision](docs/performance/2026-09-19-g6f-ratchet.md#measurement-decision-sm-03) assigns each claim its evidence. [The workload/cadence record](docs/performance/2026-10-26-workload-and-cadence.md) owns the completed PERF#1 to PERF#3 corrections; read the ratchet document before the hardware tables.

| id | status | pri | size | item |
|---|---|---|---|---|
| `SM-01` | BLOCKED | should | S | Owner attends Radeon firmware/driver setup, then adapter and display-path proof before SM-07 measurements; other hardware acceptance is unchanged. [Scope](docs/product/BACKLOG.md#next). |
| `SM-07` | BLOCKED | should | M | Low-tier CPU-phase and cadence p99s plus separate pipeline-latency evidence under the four-core hold (D3D12/Vulkan). Needs `SM-01`'s run on the Radeon; EC2 runs do not count. See the held Vulkan pairing in [the procedure](docs/performance/local-frame-bench.md). |

## Browsers, phones and hardware

**Owner:** [docs/port/web-to-be-tested.md](docs/port/web-to-be-tested.md) for the `WEB#` rows and M3's done-when in [docs/port/web.md](docs/port/web.md) section 9; stories in [docs/product/STORIES.md](docs/product/STORIES.md). The owner's hardware is listed at the top of BACKLOG.md.

| id | status | pri | size | item |
|---|---|---|---|---|
| `OCA-48` | BLOCKED | should | S | Shared owner-attended native Release/Chrome pad play, retaining actual focus/background observations; mapping remains automated. [Scope](docs/product/BACKLOG.md#next). |
| `BDP-06` | BLOCKED | should | S | Play the bundle in Chrome with the owned pad during OCA-48. The Firefox bundle, Chromium tab switch and GPU-process loss already have automated coverage. [Scope](docs/product/BACKLOG.md#next). |
| `SM-09` | BLOCKED | should | S | Keep the remaining informal Air/Safari acceptance; preserve the brief recorded play and automated Safari checks as separate evidence. [Scope](docs/product/BACKLOG.md#next). |
| `SM-14` | BLOCKED | should | S | Use and correct the LAN-serving draft during SM-13's real-iPhone session; optional Android paths do not gate iPhone acceptance. [Scope](docs/port/lan-serve.md). |
| `WEB#3` | BLOCKED | should | S | Keep real native/browser listening, sharing the audio/recovery session; record only output actually heard and leave unavailable paths unverified. [Scope](docs/port/web-to-be-tested.md). |
| `BDP-07` | BLOCKED | could | M | Automate real 60/144 Hz capture while the owner judges judder; nominal refresh and frame intervals do not prove scan-out. [Scope](docs/port/pacing-method.md). |
| `SM-13` | BLOCKED | should | M | Use the SM-14 draft during iPhone play of the development page and live demo. Record layout, rotation, touch, legibility, load time, memory (R4) and the top-out frame (R6). [Method](docs/port/pacing-method.md). |
| `BDP-10` | BLOCKED | could | XS | Lift the bundle page's phone gate after `SM-13`; `BDP-09`'s layout is in place. The owner approved iPhone-only evidence; Android is not a prerequisite. [Decision](docs/product/BACKLOG.md#decisions-only-the-owner-can-make). |
| `SM-44` | DEFERRED | could | S | Optional future real-phone record on a mid-range Android phone in Chrome. Waits on a borrowed phone; none is accessible. Does not gate iPhone acceptance. |
| `WEB#2` | BLOCKED | could | S | A standard-mapping pad in Firefox and Safari (Chrome is `BDP-06`'s), and unplug and replug with more than four pads seen. Firefox can ride `OCA-48`'s session, Safari the owner's play on the Air; five pads are not owned. |
| `WEB#5` | BLOCKED | could | S | Touch on real glass: several fingers with distinct ids, no tap also a click, foreground loss cancelling every point, LineSweeper's buttons through `WM_POINTER`, a pen as the mouse, Firefox and Safari. Needs a touchscreen. |

## Engine verification and reviews

**Owner:** [the review index](docs/review/INDEX.md) maps every review to its live ledger and tracks the remaining display and Vulkan platform checks (`REV#3` and `REV#5`).

| id | status | pri | size | item |
|---|---|---|---|---|
| `REV#8` | BLOCKED | could | S | In the attended audio session, observe actual endpoint disappearance/reappearance and retained-bank playback recovery; policy tests stay separate. [Scope](docs/review/INDEX.md#remaining-verification-and-hardware). |
| `REV#3` | BLOCKED | could | M | Record real monitor/topology-change recovery, including D3D11's same-size factory-refresh case, in the display session. [Scope](docs/review/d3d12/STATUS.md). |
| `REV#12` | BLOCKED | could | M | The 2026-09-26 review's manual checks: real device removal, interactive minimise, restore and drag, physical keyboard layouts and device changes, real sleep and wake. Needs a person at the machine. |
| `REV#5` | DEFERRED | could | S | Whether the Vulkan surface-format fallback, a fixed `compositeAlpha` and an unapplied `preTransform` are defects (Vulkan review 6.2). Live only on Android; reopens with that port. |
| `REV#9` | DEFERRED | could | M | LineSweeper input recording, replay and file versioning wait for a game that needs them. SM-78 supplies the scripted pad source. [Review](docs/review/INDEX.md#remaining-verification-and-hardware). |
| `REV#10` | DEFERRED | could | M | A frame-time overlay: the distribution, sprites submitted, particles dropped (gpt6 opportunity 5). T1: waits for a client that needs it. The measurement under it is `Application::frame_timings()` (PI-19); the overlay itself still waits. |

## Records and design decisions

**Owner:** each row's document; [BACKLOG.md's table](docs/product/BACKLOG.md#decisions-only-the-owner-can-make) records owner choices and their acceptance boundaries.

| id | status | pri | size | item |
|---|---|---|---|---|
| `OCA-66` | BLOCKED | could | S | Keep informal template acceptance at actual 150% scaling in the display session; completed manifest checks do not prove appearance. [Scope](docs/product/BACKLOG.md#later). |
| `PI-11` | DEFERRED | could | M | Revisit runtime-file staging when a client files an exhibit-backed packaging request. [Decision](docs/product/BACKLOG.md#decisions-only-the-owner-can-make). |

## Features other engines have

**Owner:** [the comparison's selection record](docs/survey/2026-10-27.md#selection-record-of-2026-11-16)
holds the current dispositions and revisit conditions. Its original comparison
and [the re-check of 2026-11-10](docs/survey/2026-11-10.md) remain historical
evidence. Selected scopes are stories above; declined proposals remain in
the owner record rather than as closed rows here.

| id | status | pri | size | item |
|---|---|---|---|---|
| `GAP#2` | DEFERRED | should | L | Until a game has a track with a demonstrated download or memory need; streaming stays separate from PCM WAV input. [Decision](docs/survey/2026-10-27.md#selection-record-of-2026-11-16). |
| `GAP#3` | DEFERRED | could | L | P4 waits for a selected offscreen-view, minimap or integer-scaling use case; completed post/material work and the roadmap stand. [Decision](docs/survey/2026-10-27.md#selection-record-of-2026-11-16). |
| `GAP#5` | DEFERRED | could | M | Until a game needs scrolling content; then define nested/empty clips and view coordinates together. [Decision](docs/survey/2026-10-27.md#selection-record-of-2026-11-16). |
| `GAP#7` | DEFERRED | could | M | Additional pixel-texture APIs wait for a procedural/update caller; public creation and between-frame replacement already exist. [Decision](docs/survey/2026-10-27.md#selection-record-of-2026-11-16). |
| `GAP#9` | DEFERRED | could | M | Until a client needs rotating views; transforms and culling must be designed together. [Decision](docs/survey/2026-10-27.md#selection-record-of-2026-11-16). |
| `GAP#10` | DEFERRED | could | S | Until a second client would copy the particle field; keep LineSweeper's workload choices local. [Decision](docs/survey/2026-10-27.md#selection-record-of-2026-11-16). |
| `GAP#13` | DEFERRED | could | M | Until a real client needs unbounded mouse motion; raw-input/pointer-lock lifecycle is then part of the scope. [Decision](docs/survey/2026-10-27.md#selection-record-of-2026-11-16). |

## New feature proposals

**Owner:** [Feature candidates](docs/feature-candidates.md), with current-code
evidence, bounded scope and proposed acceptance. Every current selection call
is settled. The remaining rows here are deferred with named triggers; accepted
implementation scopes are stories in the batches above.

| id | status | pri | size | item |
|---|---|---|---|---|
| `IDEA#8` | DEFERRED | could | M | Until a concrete hitscan or region-query caller; circle collision has landed, but nearest-hit geometry and spatial queries still need design. [Decision](docs/feature-candidates.md#idea8-scene-queries-segment-hit-and-region-overlap). |
| `IDEA#9` | DEFERRED | could | S | Until a second client needs contact begin/end edges and their lifetime contract; the current sample transition stays local. [Decision](docs/feature-candidates.md). |
| `IDEA#12` | DEFERRED | could | M | Until a client supplies a nonuniform or exported animation needing a wider schema; keep the uniform-strip tool bounded. [Decision](docs/feature-candidates.md). |
| `IDEA#14` | DEFERRED | could | S | Until an unfocused-but-visible cost measurement or client need justifies a render cap; vsync and minimised sleeping do not settle it. [Decision](docs/feature-candidates.md). |
| `IDEA#18` | DEFERRED | could | XS | Until a selected game or guide needs left/right panes; retain today's top/bottom layout. [Decision](docs/feature-candidates.md#idea18-a-side-by-side-two-player-layout). |
| `IDEA#19` | DEFERRED | could | XS | Rectangle intersection can ride clipping; shortest-arc helpers wait for actual aiming/turning code. No easing prerequisite. [Decision](docs/feature-candidates.md). |

## Review of 2026-11-10

**Owner:** [the review of 2026-11-10](docs/review/2026-11-10/README.md): [FINDINGS.md](docs/review/2026-11-10/FINDINGS.md) holds each row's evidence under its id, and [STATUS.md](docs/review/2026-11-10/STATUS.md) records each resolution. A `DECISION` row is the owner's call, and a yes becomes a story in [BACKLOG.md](docs/product/BACKLOG.md) before the work starts. A `DEFERRED` row waits for a measurement or a caller that shows the need.

| id | status | pri | size | item |
|---|---|---|---|---|
| `REV#35` | DEFERRED | should | S | Owner deferred 2026-11-16 until a real math-only rules caller needs Random; no speculative move, shuffle or derived streams. [Decision](docs/review/2026-11-10/STATUS.md). |
| `REV#37` | DEFERRED | should | S | Until pad-only play reproduces a display-sleep symptom; fold the observation into OCA-48's session before selecting a hold. [Decision](docs/review/2026-11-10/STATUS.md). |
| `REV#43` | DEFERRED | could | S | Owner deferred 2026-11-16 until a real game needs a recurring diagnostic panel; Label and frame timings remain available. [Decision](docs/review/2026-11-10/STATUS.md). |
| `REV#53` | DEFERRED | should | S | Owner deferred 2026-11-16 until a measured download/startup target or demonstrated size regression justifies a hard budget; keep reporting sizes. [Decision](docs/review/2026-11-10/STATUS.md). |
| `REV#55` | DEFERRED | could | M | Until a whole-texture tiling caller needs repeat addressing; repeating an atlas frame remains a separate problem. [Decision](docs/review/2026-11-10/STATUS.md). |
| `REV#59` | DEFERRED | could | M | After OCA-108 has physical evidence and reliable mapping facts, with an unknown fallback; browser IDs alone do not qualify family prompts. [Decision](docs/review/2026-11-10/STATUS.md). |
| `OCA-76` | DEFERRED | could | S | Waits for a second caller. The IDBFS walk of the starter was done on 2026-11-13 (beginner-guides GUIDE#12): it needs flushSave or autoPersist to survive a reload. An engine JSON writer needs README's boundary amended. |
| `REV#61` | DEFERRED | could | M | Until accepted starter work demonstrates recurring fatal-reporting defects; retain explicit native/browser entry points and separate non-fatal diagnostics. [Decision](docs/review/2026-11-10/STATUS.md). |
| `REV#65` | DEFERRED | could | S | Owner deferred 2026-11-16 until a real consumer needs a specific integration route beyond the starter ZIP or source/submodule. [Decision](docs/review/2026-11-10/STATUS.md). |
| `REV#66` | DEFERRED | could | M | Owner deferred 2026-11-16 until a concrete Windows evaluation or distribution need cannot be met by the browser route. [Decision](docs/review/2026-11-10/STATUS.md). |
| `REV#67` | DEFERRED | could | M | Reassess when a concrete parser-coverage gap justifies a maintained fuzzing corpus. SM-82's local ASan qualification is complete. [Decision](docs/review/2026-11-10/STATUS.md). |
| `REV#68` | DEFERRED | could | S | Owner deferred 2026-11-16 until an analyzer demonstrates useful findings with manageable upkeep, or a recurring defect class warrants one. [Decision](docs/review/2026-11-10/STATUS.md). |
| `REV#71` | DEFERRED | could | L | Until a concrete race investigation or selected native port justifies a Linux/TSan path; ASan is not race-freedom evidence. [Decision](docs/review/2026-11-10/STATUS.md). |
| `REV#75` | DEFERRED | could | S | A promotional performance number waits for low-tier evidence or an explicitly scoped qualified comparison; dated technical records remain available. [Decision](docs/review/2026-11-10/STATUS.md). |
| `REV#77` | DEFERRED | could | S | Until a borrowed-slice caller or measured allocation cost justifies public text-parameter changes; retain current callers meanwhile. [Decision](docs/review/2026-11-10/STATUS.md). |
| `REV#78` | DEFERRED | could | M | `ResourceLoader::load_manifest` loads everything in one blocking call with no progress report, and no resource type has an unload verb; waits for a client with a long load or level-scoped content. |
| `REV#79` | DEFERRED | could | M | Until a client needs a specific additional collision shape with concrete geometry, allocation and lifetime requirements. [Decision](docs/review/2026-11-10/STATUS.md). |
| `REV#81` | DEFERRED | could | S | Revisit contact-point data when a caller needs it. PI-25's accepted callback change excludes contact points. [Scope](docs/review/colourwars-reports/contact.md). |
| `REV#83` | DEFERRED | could | M | Select individual window options only for a caller's need; title/icon work does not select size, position, monitor or maximised-start policy. [Decision](docs/review/2026-11-10/STATUS.md). |
| `REV#84` | DEFERRED | could | S | No query for the player's preferred language on Win32 or in the browser shell under `engine/app/`; nothing in the tree ships two languages, so it waits for a localised sample or client. |
| `REV#85` | DEFERRED | could | M | `AudioDevice` has no verb to close a wave bank or destroy a voice, so a game that replaces banks grows both lists until the device is destroyed; waits for a client that swaps banks. |
| `REV#86` | DEFERRED | could | M | The audio backends set no voice ceiling, limiter or master gain, and ungrouped sounds in `AudioResources` play at full level; inferred, nobody has heard clipping. Follows `WEB#3`'s listening and a client. |
| `REV#87` | DEFERRED | could | M | `AudioDevice` and `SoundBank` expose no playback position, wave duration or start offset; the XAudio2 cost is unmeasured. Waits for a client that times something to audio, and would ride `GAP#2`. |
| `REV#95` | DEFERRED | could | S | bench/scene_bench.cpp has no touching-tile case, and layers_collide runs after BroadPhase has emitted the pair; a bench case or change waits for a client with a tile world and a profile. |
| `REV#96` | DEFERRED | could | S | `engine/app/web/window.cpp`'s `measure` has no upper bound on device pixel ratio; a cap is a contingent answer to R6, deferred until `SM-13`'s phone measurement shows a frame over budget. |
| `REV#98` | DEFERRED | could | XS | `.github/actions/emscripten/action.yml:80-85` saves a ccache per run and attempt; re-measured 2026-11-12 at 37 caches, 2.84 GB, 27 of them ccache. Deferred until an expensive cache is seen evicted, since restore-only changes documented reuse. |
| `REV#99` | DEFERRED | could | S | No coverage tooling exists in `cmake/`, `tools/`, `.github/` or `CMakePresets.json`; deferred until a decision waits on a coverage figure, since the dark backend recovery paths are already named in the `REV#` rows. |
| `REV#100` | DEFERRED | could | S | `tools/web_bundle/bundle.mjs` writes raw and Brotli sizes to `BUILD.json` with no threshold; the budget is `REV#53`'s call, and a Brotli tolerance is deferred until that budget is taken. Bench output stays ungated. |
| `REV#101` | DEFERRED | could | XS | Pinned ruff 0.14.5 and actionlint 1.7.8 with shellcheck found no defect on 2026-11-13 (STATUS records each class of noise), so no style check is added; it reopens when a defect slips through. No .clang-format. |
| `REV#102` | DEFERRED | could | S | Closure, emmalloc, `-Os`/`-Oz` and LTO are untried for `wasm-release` (`cmake/settings.cmake:85-92`); measure them once a size budget exists to hold the result. |

## First-run review of 2026-11-10

The [first-run findings](docs/review/2026-11-10-first-run/FINDINGS.md) identify the reader's problem for each row. The [status ledger](docs/review/2026-11-10-first-run/STATUS.md) records which Windows environment each reproduction needs.

| id | status | pri | size | item |
|---|---|---|---|---|
| `REV#112` | BLOCKED | could | S | Capture a genuine Smart App Control or Defender refusal of a reader's own starter-built MyGame.exe before writing recovery guidance. [Finding](docs/review/2026-11-10-first-run/FINDINGS.md#rev112). |
| `REV#113` | BLOCKED | could | S | Compare default cmd, vcpkg and compiler output with VSLANG=1033 on a non-English Windows installation with localized tools. [Finding](docs/review/2026-11-10-first-run/FINDINGS.md#rev113). |
| `REV#114` | BLOCKED | could | S | Record the first rebuild of an existing starter across an update to the same Visual Studio instance, then document recovery and cleanup. [Finding](docs/review/2026-11-10-first-run/FINDINGS.md#rev114). |

## Work audit of 2026-11-12

**Owner:** [the work audit of 2026-11-12](docs/review/2026-11-12-work-audit/README.md): [FINDINGS.md](docs/review/2026-11-12-work-audit/FINDINGS.md) holds each row's evidence under its id, and [STATUS.md](docs/review/2026-11-12-work-audit/STATUS.md) records each resolution. An `OPEN` row needs no owner call; a `DECISION` row is the owner's, and a yes becomes a story in [BACKLOG.md](docs/product/BACKLOG.md) before the work starts.

| id | status | pri | size | item |
|---|---|---|---|---|
| `REV#159` | WAITING | should | S | Check for a week without another SafariDriver bundle-session creation failure. Inspect each executed attempt and retry through 09:54 Brisbane time on November 24. [Evidence](docs/port/web-to-be-tested.md#hosted-safari-stability-2026-11-20). |

## Ports

**Owner:** [docs/port/android.md](docs/port/android.md), section 4.

| id | status | pri | size | item |
|---|---|---|---|---|
| `PORT#1` | DEFERRED | could | M | A KTX2 reader beside `dds_file.h` with the ETC2 formats (spine item 3.1a, headless, blocks nothing), and an ETC2 encode of the sprite sheet to judge its hard alpha edges. Until the Android port is taken up. |
| `PORT#2` | DEFERRED | could | XL | An Android device is needed for the NDK preset, render text, pad seam and activity surface-loss work. Revisit report 21's input decision when the port starts. [Plan](docs/port/android.md). |

## Areas with no open rows

A new row for one of these gets its section back.

| area | owner | a row returns with |
|---|---|---|
| Game-authored post materials | [OCA-84](docs/product/BACKLOG.md#oca-84) and [effects.md §16](docs/effects.md#16-game-material-implementation-amendment) | a regression or another shader-authoring requirement |
| The rest of Next (X1 to X28) | [BACKLOG.md, Next](docs/product/BACKLOG.md#next) | a new story selected for this horizon |
| Content, text and sample examples (L21 to L23, L26, L35) | [BACKLOG.md, Later](docs/product/BACKLOG.md#later) | a new text, content or sample-example story reaching the front |
| Code hygiene (L36 to L54) | [Code hygiene](docs/code-hygiene.md) and [BACKLOG.md, Later](docs/product/BACKLOG.md#later) | a new bounded repair or review story reaching the front |
| The rest of Later (L1 to L35) | [BACKLOG.md, Later](docs/product/BACKLOG.md#later) | a new story selected for these batches |
| CI (`CI#`) | [.github/workflows/ci.yml](.github/workflows/ci.yml), [docs/dependency-updates.md](docs/dependency-updates.md) | a new CI check or job; a new required job or matrix entry needs the owner to update branch protection |
| Won't this time | [BACKLOG.md](docs/product/BACKLOG.md#wont-this-time) | a re-plan of the backlog with its reason recorded there |
| web.md's milestones M1, M2, M5, M6 and its decisions | [docs/port/web.md](docs/port/web.md) sections 8 and 9, and [docs/port/web-as-built.md](docs/port/web-as-built.md) for what each measured | a regression, or a change the port meets that a decision must settle |
| macOS and iOS | [docs/port/android.md](docs/port/android.md) sections 5 and 6 | a plan document; none is in the tree |
| The EC2 performance lane | [tools/cloud_performance/README.md](tools/cloud_performance/README.md) | a new declared reference run, or a regression question |
| round-2 plan, the RTCD list, the first review, backend-equivalence | [docs/review/round-2/PLAN.md](docs/review/round-2/PLAN.md), [docs/review/rtcd/](docs/review/rtcd/), [docs/review/IMPLEMENTED.md](docs/review/IMPLEMENTED.md) | nothing: all landed or superseded; the game-side rows live in ColourWars |
| The 2026-09-26 review's numbered findings and hardening candidates | [docs/review/2026-09-26/STATUS.md](docs/review/2026-09-26/STATUS.md) and [the review index](docs/review/INDEX.md#hardening-verification) | nothing: all 23 implemented and four hardening candidates addressed; `REV#12` remains |
| Beginner guides (`GUIDE#1` to `GUIDE#13`) | [Beginner guides](docs/beginner-guides.md) | a new procedure a beginner cannot follow, or a page that stops matching the starter |
| The historical repository split, file-length tripwire and survey decisions | [docs/repo-split.md](docs/repo-split.md), [docs/file-length-audit.md](docs/file-length-audit.md), [docs/survey/2026-08-26-status.md](docs/survey/2026-08-26-status.md) | nothing: original work and StateContext's keep verdict stand |

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
   - `#NNN`: the public Labrador issue number for a pending response in the [client report ledger](docs/review/colourwars-reports/README.md).
   - `GAP#`: [the engine comparison of 2026-10-27](docs/survey/2026-10-27.md).
   - `IDEA#`: [Feature candidates](docs/feature-candidates.md), selected into BACKLOG.md before implementation.
   - `GUIDE#`: [Beginner guides](docs/beginner-guides.md).
5. **NOW changes only when its items finish or the owner reorders them.**

**`id`:** a stable handle, not an order. A story's id where one owns the item (`OCA-03`), else a prefixed one from rule 4; a retired id is never reused. Other ids an item covers are named in its clause.

**`status`**, a closed list:
- `OPEN`: ready to work.
- `DECISION`: an unresolved choice or authorisation needs the owner. Once its scope is approved, execution and acceptance use the statuses below. Always listed in NOW.
- `WAITING`: waits for a defined event outside the work, such as a PR merging or the relevant deployment; approval supplies no event result.
- `BLOCKED`: cannot be worked today because a prerequisite is open, required hardware or owner attendance is unavailable, or explicitly required owner input is missing. An approved attended session stays open here until its observation is recorded.
- `DEFERRED`: paused on purpose, until the condition in its clause holds.

**`pri`:** the story's MoSCoW priority as BACKLOG.md gives it: `must` · `should` · `could`. A row with no story gets the grade its owner implies, and what the backlog put in Won't this time never gets a row.

**`size`:** as in BACKLOG.md: `XS` one small commit · `S` a session or less · `M` a few sessions · `L` a week or more · `XL` several weeks.

**The repository is public.** Write as the design documents do: plain, exact, the present state rather than its history, and the owner as "the owner". Nothing about ColourWars beyond what this tree already publishes.
