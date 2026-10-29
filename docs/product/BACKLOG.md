# Backlog

The order of work: stories in four horizons, each with the exit criteria that end it. **This is the one file in `docs/product/` that is kept current**: tick a box in the same commit as the change that closes the story, and amend a horizon here rather than letting the work drift away from it. The other files are the design record of 2026-10-13 and are read as of that date.

Each line reads: id, MoSCoW, size, the persona it serves, title, and what it waits on. The reasoning for every placement is in the *why* under each horizon. The original stories are in [STORIES.md](STORIES.md); later additions have their scope and acceptance here.

## How the order was decided

Built from the ranked backlog after three corrections to its inputs. (a) Duplicates were merged before ranking: 23 groups fold 27 stories into their leads, so each piece of work appears once and capacity is not double-counted. (b) Every t_ref was checked against the cited text; 105 were corrected, and where a merge joined two incompatible readings (OCA-48/SM-39 on a backgrounded pad) the reading that matches the header's stated contract was taken. (c) The critique's 16 corrections and the owner's 2026-10-13 facts were applied: a physical XInput pad and an iPhone are owned, no Android phone; the MacBook Air stops at Big Sur; the iGPU needs AMD's driver before any adapter choice; ColourWars' math-split migration is done, so its next pin move is an ordinary bump. Horizons were then set by the earlier tests, in order: (1) does delay compound (change list, consumer CI, security route, agent brief); (2) does it serve the primary outside-cpp-adopter's evaluate-adopt-ship-keep path; (3) can one person with agents finish it on owned hardware. Now holds 35 stories in 20 commit batches, each one self-contained PR under the branch, PR, rebase-merge-on-green rule; only two are larger than S (OCA-68 L, OCA-11 M), and every owner-only step in Now is a sign-off or a repository setting rather than hardware. PHILOSOPHY.md amendments use separate PRs, each rebased on the last, so parallel agent branches never conflict on it. The planned order was: A (Now) OCA-05 + FES-03 + NMS-02, facts and permanence, no owner decision; B (Now, last) OCA-69 + OCA-67, the deletion rule, the settle criterion and the first 0.x tag, owner-worded; C (Next) OCA-08, the accessibility position in the UI section, owner-worded; D (Next) SM-03, the per-claim measurement decision in Performance and ratchet section 5, resolved independently of C on 2026-10-27; E (Later) SM-02, the four-core hold sentence at :691-693, after D; F (Later) SRL-14, T10 and The public face, only if the series is withdrawn. Dependency rules were checked mechanically: no dependency sits in a later horizon than its dependant or carries a weaker MoSCoW, so SM-02, SM-04, SM-05, SM-08, SM-14, BDP-04 and BDP-23 were promoted where a stronger story waits on them. Engine mechanism is added only where a present caller is recorded (PI-05, PI-11 after PI-10). No item adds an editor, scripting, 3D, online play or action mapping.

## Decisions only the owner can make

| Decision | Blocks |
|---|---|
| Decided 2026-10-27: the engine supplies input/UI mechanisms and games own accessibility choices; LineSweeper aims for keyboard/controller play, clear focus, readable contrast, reduced motion and the agreed palettes, with verified support and limitations stated | OCA-08, BDP-24, BDP-21, BDP-22, BDP-25 |
| Decided 2026-10-26: ColourWars' pin reaches each Labrador minor tag within a week of it being cut; its gameplay check is ColourWars' own, outside this repository | SM-36, PI-14 |
| Decided 2026-10-27: separate CPU phase and cadence p99s from GPU execution and pipeline latency; the ratchet document's section 5 owns the mapping and PHILOSOPHY cites it | SM-03, SM-01, SM-07, SM-02 |
| Decided 2026-10-27: retain minimal engine-focused ColourWars disclosure, with no private client paths, names or lines; recheck and file or withdraw the reports within that boundary | PI-09, PI-10, PI-11 |
| Decided 2026-10-27: iPhone evidence alone may lift the phone gate after SM-13 and BDP-09; Android is optional future evidence, not a prerequisite | BDP-10 |
| Decided 2026-10-27: keep the existing 4.3 MB on-demand phone fallback video | BDP-20 |
| Clarified 2026-10-27: the C++ introduction has substantial pre-production work but is unfinished; publish its actual scope and status, without claiming a completed engine API series | SRL-14, SRL-15, SRL-17, SRL-16 |
| Decided 2026-10-27: rename non-macro uppercase constants to snake_case across both repositories; retain CONVENTIONS and record the source migration | SRL-07 |
| Decided 2026-10-27: v0.3.0 gets the first complete immutable documentation snapshot; no earlier-release backfill; implement and verify versioning before cutting it, with no release date promised | SITE#3 |
| Decided 2026-10-27: the Windows samples/template default to per-monitor DPI awareness; implemented, and the reviewed 150% scaling run remains | OCA-66 |

`SITE#3` remains open under [the versioning policy](../website-versioning.md)
and is a prerequisite for cutting `v0.3.0`. Verified candidate builds,
versioned links and search, the release manifest, the snapshot archive,
assembly, CI verification, the release index and the version selector are
implemented and checked offline with fixtures;
[the policy's evidence](../website-versioning.md#candidate-build-evidence-2026-10-27)
distinguishes that fixture from an actual engine release. The selector, index
page and notice were qualified on assembled real snapshot output
[in headless Chrome](../website-versioning.md#headless-chrome-qualification-2026-10-27)
and [again in Chrome and Firefox](../website-versioning.md#rebuilt-fixture-qualification-2026-10-27),
as emulation. What remains is acceptance in Safari, on a real phone with touch
and with a screen reader; then `v0.3.0`'s snapshot is published
and its deploy checked. Immutable releases were enabled on 2026-10-27.

## Now

Trust and clocks: the public face and the tree's records stop misleading readers and agents, the live demo is checked and one click away, Get Started and the template work as written, and every item whose delay compounds has landed

**Done when**

- CLAUDE.md and AGENTS.md are one brief with a build-time sync check, and the branch, PR, rebase-merge procedure is written in the tree
- web.md sections 7 and 9 record the LineSweeper demo as live; its server-side deploy checks are ticked against the live URL, the phone item is recorded as emulated and linked to SM-13, a flash check of the top-out burst is recorded with its method, and only then is the demo linked from the README, the site and samples/linesweeper/README.md
- Every public page gives one platform answer (Windows and a desktop browser), with the browser pad's, touch's and phones' verification state disclosed
- No design document or published page contradicts the tree on split-screen, action mapping, the preset count, the template's neighbours or the exclusions' permanence (PHILOSOPHY amendment A), and GitHub detects the licence as MIT
- SECURITY.md exists with private vulnerability reporting enabled, and an issue template states the exhibit rule
- CHANGES.md exists with its rule, the merge procedure requires it, and its backfill covers every engine commit since ColourWars' pin 4cefb68
- Required checks survive a workflow edit, and a consumer-shaped project built against the PR's own engine tree is a required, green CI job
- Get Started, the template folder and the sprites-and-fonts guide are true as written, including the content tools and the template font's terms
- PHILOSOPHY amendment B has landed with the owner's wording: a checkable settle criterion, a deletion rule a public repository can follow, and the first 0.x tag with release notes linking CHANGES.md

**Lands as**

1. N1 Agent brief: merge procedure, then one brief with a sync check: SM-20, SM-22
2. N2 Demo records say live; server-side deploy checks ticked: SM-12
3. N3 Flash check of the top-out burst (before the link): BDP-23
   - N3b Calm the top-out by default, on the owner's strict reading of WCAG 2.3.1 (2026-10-13): BDP-23
4. N4 Link the live demo from README, site and LineSweeper README: OCA-03
5. N5 One platform statement, with the browser verification caveats: OCA-01, OCA-54
6. N6 PHILOSOPHY amendment A (first): facts and permanence: OCA-05, FES-03, NMS-02
7. N7 Why page points at the split-screen sample (after N6): OCA-02
8. N8 LICENSE says only what is true: OCA-07
9. N9 README and GitHub front door: OCA-06, OCA-10
10. N10 Concept pages say true things about the samples: SRL-03
11. N11 SECURITY.md and the request route: OCA-73, OCA-72
12. N12 CHANGES.md, its rule and the backfill to 4cefb68: OCA-68
13. N13 Required checks that survive a workflow edit: SM-21
14. N14 Consumer CI job against the PR engine tree: OCA-11
15. N15 Get Started pages true as written: OCA-09, OCA-17, OCA-14, OCA-16
16. N16 Template folder: comments and how to copy it: OCA-13, OCA-15
17. N17 Sprites and fonts guide: content tools and the template font terms: OCA-34, OCA-37
18. N18 Header contracts: layer_depth and collidable retirement: OCA-55, OCA-59
19. N19 Ledger fixes: STATUS box, Firefox claim, canonical host: SM-06, SM-10, SM-31
20. N20 PHILOSOPHY amendment B (owner-reviewed, last): deletion rule, settle criterion and first 0.x tag: OCA-69, OCA-67
21. N21 Replace the template font with JetBrains Mono Bold (the owner's choice, moved from Next's X9; after N17): OCA-75

**Stories**

- [x] **[SM-20](STORIES.md#sm-20)** Must · XS · The Sole Owner-Maintainer: Write the merge procedure into the tree
- [x] **[SM-22](STORIES.md#sm-22)** Should · XS · The Sole Owner-Maintainer: One agent brief, not two copies — after SM-20
- [x] **[SM-12](STORIES.md#sm-12)** Must · S · The Sole Owner-Maintainer: Make the records say the demo is live, and tick its server-side deploy checks against it
- [x] **[BDP-23](STORIES.md#bdp-23)** Must · S · The Browser and Phone Demo Player: Check the top-out burst against flash limits
- [x] **[OCA-03](STORIES.md#oca-03)** Must · S · The Outside C++ Adopter: Try LineSweeper in my browser from the site and the README — after SM-12, BDP-23
- [x] **[OCA-01](STORIES.md#oca-01)** Must · S · The Outside C++ Adopter: One answer to 'where does it run?' on every public page
- [x] **[OCA-54](STORIES.md#oca-54)** Should · S · The Outside C++ Adopter: Know which browser pad and touch paths have met real hardware — after OCA-01
- [x] **[OCA-05](STORIES.md#oca-05)** Must · S · The Outside C++ Adopter: Design documents say nothing that is false even as target
- [x] **[FES-03](STORIES.md#fes-03)** Should · S · The Full-Environment Seeker (anti-persona) · **Owner**: State the exclusions' permanence, the editor included, to match the recorded decision
- [x] **[NMS-02](STORIES.md#nms-02)** Should · XS · The Native Multi-Platform Shipper (anti-persona, for now): Published design pages do not read Vulkan's reach as support
- [x] **[OCA-02](STORIES.md#oca-02)** Must · S · The Outside C++ Adopter: The Why page points me at the split-screen sample that ships — after OCA-05
- [x] **[OCA-07](STORIES.md#oca-07)** Must · XS · The Outside C++ Adopter: My licence scanner sees MIT
- [x] **[OCA-06](STORIES.md#oca-06)** Should · XS · The Outside C++ Adopter · **Owner**: The GitHub front door leads me to the site, not a private game
- [x] **[OCA-10](STORIES.md#oca-10)** Must · XS · The Outside C++ Adopter: The README's consumer section is complete and points at a real consumer
- [x] **[SRL-03](STORIES.md#srl-03)** Must · S · The Sample-Reading Learner: Concept pages say true things about the samples they cite
- [x] **[OCA-73](STORIES.md#oca-73)** Must · S · The Outside C++ Adopter · **Owner**: Report a vulnerability privately and know what input is trusted
- [x] **[OCA-72](STORIES.md#oca-72)** Should · S · The Outside C++ Adopter: Ask a question or request an API change through a stated route
- [x] **[OCA-68](STORIES.md#oca-68)** Must · L · The Outside C++ Adopter: Read what changed that affects my game, from the pin I am on — after SM-20
- [x] **[SM-21](STORIES.md#sm-21)** Must · S · The Sole Owner-Maintainer · **Owner**: Required checks that survive a workflow edit
- [x] **[OCA-11](STORIES.md#oca-11)** Must · M · The Outside C++ Adopter · **Owner**: The project shape Get Started teaches is built against every engine change — after SM-21
- [x] **[OCA-09](STORIES.md#oca-09)** Must · S · The Outside C++ Adopter: Get Started tells me the truth about long paths
- [x] **[OCA-17](STORIES.md#oca-17)** Should · XS · The Outside C++ Adopter: Prerequisite and naming instructions that work as written
- [x] **[OCA-14](STORIES.md#oca-14)** Should · XS · The Outside C++ Adopter: The 'dozen lines of CMake' claim matches the file and says what shipping adds
- [x] **[OCA-16](STORIES.md#oca-16)** Could · XS · The Outside C++ Adopter: First-change sends me on to the guides
- [x] **[OCA-13](STORIES.md#oca-13)** Should · XS · The Outside C++ Adopter: The template's comments describe public code and the current content rule
- [x] **[OCA-15](STORIES.md#oca-15)** Should · S · The Outside C++ Adopter: The template folder tells me how to copy it and what changed — after OCA-13, OCA-68
- [x] **[OCA-34](STORIES.md#oca-34)** Must · S · The Outside C++ Adopter: Get the content tools and the commands to make sprites and fonts
- [x] **[OCA-37](STORIES.md#oca-37)** Must · XS · The Outside C++ Adopter: Know whether I may ship the template font, before I copy it — after OCA-34
- [x] **[OCA-55](STORIES.md#oca-55)** Should · XS · The Outside C++ Adopter: Know that layer_depth orders nothing
- [x] **[OCA-59](STORIES.md#oca-59)** Should · XS · The Outside C++ Adopter: Know that a collidable must say how it is retired
- [x] **[SM-06](STORIES.md#sm-06)** Should · XS · The Sole Owner-Maintainer: Stop the low-tier STATUS box sending me after a fixed contradiction
- [x] **[SM-10](STORIES.md#sm-10)** Should · XS · The Sole Owner-Maintainer: Make web.md's Firefox claim match what was actually checked
- [x] **[SM-31](STORIES.md#sm-31)** Could · XS · The Sole Owner-Maintainer · **Owner**: One canonical host for the engine site
- [x] **[OCA-69](STORIES.md#oca-69)** Must · S · The Outside C++ Adopter · **Owner**: Know how a public name leaves the engine
- [x] **[OCA-67](STORIES.md#oca-67)** Must · S · The Outside C++ Adopter · **Owner**: Pin a named version and know what it promises — after OCA-68, OCA-69
- [x] **[OCA-75](STORIES.md#oca-75)** Could · M · The Outside C++ Adopter · **Owner**: A template font I may ship without checking its terms — after OCA-37, OCA-34

<details><summary>Why each is placed here</summary>

| Story | Why |
|---|---|
| SM-20 | G65: the merge procedure lives only in the owner's memory, and agents carry out every other story here. One paragraph makes all of them cheaper and safer, so it goes first. |
| SM-22 | G66: two 602-line copies drift silently; a pointer or a sync check (T4, T5) stops every amendment being made twice. Lands with SM-20 and before SM-43 trims the brief. |
| SM-12 | G05 (merged with BDP-02): the records say the demo is not deployed when it is live. Server-side checks are tickable now; the phone item is emulated and its real-phone half linked to SM-13 (critique split). |
| BDP-23 | G118: the flash check must precede the link that widens the page's audience (critique reversed the dependency). Promoted to Must because the Must link waits on it; the method (LineSweeperCapture frames against WCAG 2.3.1) is named. |
| OCA-03 | G04 (merged with BDP-01, SRL-01): the live demo is the cheapest proof of the browser claim and is one page edit away (website-plan.md:90). Three placements, one commit. |
| OCA-01 | G02 (merged with NMS-01): pages disagree on where Labrador runs. One answer serves the primary persona's fit decision and the anti-persona's early exit; the Safari line stays 'not yet checked' until SM-09. |
| OCA-54 | G47 (merged with NMS-05's input.mdx part): disclose which browser pad and touch paths met real hardware. Rides the platform sweep's PR. |
| OCA-05 | G44, G03 (merged with SM-37): statements false even as target, published word for word. PHILOSOPHY amendment A, a factual edit with no owner decision (critique), lands first of the PHILOSOPHY PRs. |
| FES-03 | G99: PHILOSOPHY's 'for now' contradicts CLAUDE.md's recorded decision that an editor is permanently out. An amendment with an owner sign-off, not an open decision (critique); same PR as OCA-05. |
| NMS-02 | G102: published design pages read Vulkan's reach as support. Same PR as amendment A if PHILOSOPHY.md:395-404 is reworded; a site-side note otherwise. |
| OCA-02 | G03: the Why page says no sample draws a second view while local_multiplayer ships. Lands in or after amendment A so the page and PHILOSOPHY agree. |
| OCA-07 | G149 (merged with PI-13): a stale LICENSE paragraph breaks MIT detection and ships inside ColourWars' release ZIP. One paragraph. |
| OCA-06 | G55, G100 (merged with FES-01): the front door names a private game as proof and no exclusions. Rides the README edit; the About fields are owner-only settings. |
| OCA-10 | G07, G106: the README's consumer section is incomplete for the primary persona's adopt step. Same PR as the front-door edit. |
| SRL-03 | G36: concept pages say false things about the samples they cite, and the quote check cannot catch prose. |
| OCA-73 | G148 (merged with SM-42): a public repository with no private vulnerability route; delay compounds with exposure. The setting is owner-only. |
| OCA-72 | G21 (merged with PI-07): no stated route for questions and exhibit-backed requests. Same PR as SECURITY.md; PI-09 files into it in Next. |
| OCA-68 | G18 (merged with PI-01, SM-34, PI-02): no consumer-visible change list, and every merge without one widens the backfill (test 1). The backfill to 4cefb68 serves ColourWars' next ordinary bump. |
| SM-21 | G134: required contexts are matrix strings held only in GitHub settings; renaming an entry wedges PRs. Must precede adding the consumer job's context. |
| OCA-11 | G08 (merged with PI-06): nothing builds the project shape Get Started teaches. The only M in Now; it consumes the PR's own engine tree from a short path (critique). |
| OCA-09 | G06: Get Started's long-path advice fails as written (seven C1083 failures observed). |
| OCA-17 | G58, G60: prerequisite and naming instructions that do not work as written. Same PR as OCA-09. |
| OCA-14 | G41: 'a dozen lines of CMake' is 64 lines with an Emscripten branch. Same PR. |
| OCA-16 | G56: first-change ends without sending the reader on. One link in the same PR. |
| OCA-13 | G40, G42: the template's comments point into private ColourWars code and describe the pre-92966b0 content rule. |
| OCA-15 | G43: the template folder has no copy instructions; its change list points at CHANGES.md rather than duplicating it (T4). |
| OCA-34 | G12 (major): no content tools or commands for sprites and fonts. Moved into Now by the critique, docs-only and XS-S. |
| OCA-37 | G107 (major): every adopter who copies the template copies a font whose terms NOTICE does not settle. The disclosure moves to Now (critique split); replacing the font is OCA-75, N21. |
| OCA-55 | G138: layer_depth orders nothing and the header does not say so where a caller reads it. |
| OCA-59 | G49: a collidable's retirement hook is a silent no-op by default. Same header-contract PR as OCA-55. |
| SM-06 | G64: the low-tier STATUS box sends readers after a contradiction already fixed. |
| SM-10 | G69: web.md overstates what Firefox checked. OCA-28 waits on it. |
| SM-31 | G57, G136: two hosts for the engine site; its delay compounds as links accumulate (critique kept it). The host setting is owner-only. |
| OCA-69 | G19 (merged with SM-35): the deletion rule cannot be followed in a public repository. Owner-worded; PHILOSOPHY amendment B with OCA-67. Must because the Must tag depends on it. |
| OCA-67 | G17 (merged with PI-04, SM-33): no named version to pin. The settle criterion and the first 0.x tag land together, last in Now, once the change list and deletion rule exist so the tag promises only what the process keeps. |
| OCA-75 | G107: replacing the template atlas with an openly licensed font (critique split from OCA-37). The owner chose JetBrains Mono Bold (SIL OFL) on 2026-10-13 and moved it into Now; the cost is reviewing every regenerated golden and capture that draws text. |

</details>

## Next

Pin, test, ship: an adopter moves a pin by procedure, tests their game, makes content, ships a Windows release with its runtime and notices, starts a browser build and reads a reference that gives the contract; the owned pad, Safari and iGPU checks are taken

**Done when**

- The agent brief is a current-state brief, its narrative history moved to dated documents
- A written pin-move procedure exists and ColourWars' pin has been moved with it once, as an ordinary bump
- The template has a test target and a --smoke-test check, both built by the consumer CI job, and a copyable CI recipe is published
- Content tools, premultiplied alpha and the audio formats that load on both platforms are documented, and the template font is either replaced or its replacement declined
- A Release package staged by the template's install rules has run on a clean Windows install, and its runtime requirement, notice list, OS and hardware floor and backend choice are stated
- An adopter can install the pinned Emscripten and add a browser build to their own project from public pages, with gl as the browser default, hosting requirements and an honest browser list
- Every game-facing reference page says how a game obtains the type, links the guide that teaches it and names members correctly
- The physical-pad session, the MacBook Air's Safari ceiling and the first iGPU step (AMD driver plus per-app preference) are recorded
- The accessibility position (PHILOSOPHY amendment C) is recorded, the bundle page a host serves is a ctest entry with its failure paths, and the ColourWars drafts are filed

**Lands as**

1. X1 Trim the agent brief to current state: SM-43
2. X2 Pin-move procedure page and running the engine suite at a pin: OCA-70, OCA-71
3. X3 ColourWars: pin cadence and the first ordinary bump (ColourWars repository): SM-36
4. X4 Small truths for the exclusion seeker and the learner: FES-04, FES-02, SRL-02, SRL-04, SRL-05
5. X5 Safari ceiling on the MacBook Air, and a play if it allows: SM-09
6. X6 Physical pad session, desktop and browser: OCA-48, BDP-06
7. X7 Template test target, smoke check, cold-cache record and CI recipe: OCA-29, OCA-30, OCA-12, OCA-33
8. X8 Content and audio guides: OCA-35, OCA-38, OCA-39, OCA-36
9. X9 Replace the template font: moved to Now as N21
10. X10 Windows notices and the template install rules: OCA-62, OCA-60
11. X11 Clean-machine run, OS and hardware floor, backend choice: OCA-61, OCA-64, OCA-65
12. X12 ColourWars release on a clean machine (ColourWars repository): PI-14
13. X13 Input and menus guides: OCA-44, OCA-45, OCA-49, OCA-52
14. X14 Settings and saves guide: OCA-53
15. X15 Reference: how a game gets each type: OCA-56
16. X16 Reference generator: names, audience marker, Explained in: OCA-58, SRL-08, OCA-57
17. X17 Browser preset defaults to gl or refuses at configure: OCA-22
18. X18 Adopter browser build: Emscripten install, the page and the template web files: OCA-21, OCA-20, OCA-19
19. X19 What a web host must send: OCA-23
20. X20 Which browsers can be promised: OCA-28
21. X21 PHILOSOPHY amendment C (owner-reviewed): accessibility position: OCA-08
22. X22 Demo accessibility note: BDP-24
23. X23 Bundle page in ctest, with its failure paths: BDP-03, BDP-04
24. X24 Engine revision for consumers: PI-05
25. X25 Per-claim measurement decision (PHILOSOPHY amendment D and ratchet section 5): SM-03
26. X26 iGPU first step: AMD driver and per-app preference: SM-01
27. X27 File the ColourWars drafts upstream: PI-09
28. X28 Reviews index and CI guards: SM-16, SM-19, SM-25

**Stories**

- [x] **[SM-43](STORIES.md#sm-43)** Should · M · The Sole Owner-Maintainer: A current-state agent brief, with its narrative history moved out — after SM-22, OCA-68
- [x] **[OCA-70](STORIES.md#oca-70)** Must · S · The Outside C++ Adopter: Move my engine pin by a written procedure
- [x] **[OCA-71](STORIES.md#oca-71)** Could · XS · The Outside C++ Adopter: Run the engine's own tests at my pin — after OCA-70
- [x] **[SM-36](STORIES.md#sm-36)** Should · S · The Sole Owner-Maintainer · **Owner**: A cadence for moving ColourWars' engine pin — after OCA-70, OCA-68
- [x] **[FES-04](STORIES.md#fes-04)** Should · XS · The Full-Environment Seeker (anti-persona): 'What Labrador is not' covers every want the seeker arrives with and names the alternatives — after FES-03
- [x] **[FES-02](STORIES.md#fes-02)** Could · XS · The Full-Environment Seeker (anti-persona): Get Started points at the exclusions before it asks for Visual Studio — after FES-04
- [x] **[SRL-02](STORIES.md#srl-02)** Could · XS · The Sample-Reading Learner: Read Concepts without first building the minimal sample
- [x] **[SRL-04](STORIES.md#srl-04)** Could · XS · The Sample-Reading Learner: Examples page states LineSweeper's real reading size
- [x] **[SRL-05](STORIES.md#srl-05)** Could · XS · The Sample-Reading Learner: LineSweeper README's open question states how much the unchecked layer rule covers
- [ ] **[SM-09](STORIES.md#sm-09)** Should · S · The Sole Owner-Maintainer · **Owner**: Record the MacBook Air's Safari ceiling, and play the demo in it if it can
- [ ] **[OCA-48](STORIES.md#oca-48)** Should · S · The Outside C++ Adopter · **Owner**: One physical-pad session: trust the desktop pad mapping and know what a backgrounded game receives
- [ ] **[BDP-06](STORIES.md#bdp-06)** Should · S · The Browser and Phone Demo Player · **Owner**: Play the bundle page in Firefox, with a physical pad, through a real tab switch and a real context loss
- [x] **[OCA-29](STORIES.md#oca-29)** Must · M · The Outside C++ Adopter: A test target for my game's rules from the start — after OCA-11
- [x] **[OCA-30](STORIES.md#oca-30)** Must · S · The Outside C++ Adopter: A start-up smoke check for my Windows game — after OCA-29
- [x] **[OCA-12](STORIES.md#oca-12)** Should · S · The Outside C++ Adopter: The first-build time and public-clone path are observed from a cold cache — after OCA-11
- [x] **[OCA-33](STORIES.md#oca-33)** Should · M · The Outside C++ Adopter: A CI workflow I can copy for my game's repository — after OCA-29, OCA-30, OCA-11
- [x] **[OCA-35](STORIES.md#oca-35)** Must · XS · The Outside C++ Adopter: Know that translucent textures must be premultiplied — after OCA-34
- [x] **[OCA-38](STORIES.md#oca-38)** Should · XS · The Outside C++ Adopter: The guide's XWBTool command makes a bank that loads in the browser too
- [x] **[OCA-39](STORIES.md#oca-39)** Should · S · The Outside C++ Adopter: Export my sounds in a format the engine accepts on both platforms — after OCA-38
- [x] **[OCA-36](STORIES.md#oca-36)** Should · S · The Outside C++ Adopter: Copy a guide sample without broken paths
- [x] **[OCA-62](STORIES.md#oca-62)** Must · S · The Outside C++ Adopter: Know the notices my Windows game must ship
- [x] **[OCA-60](STORIES.md#oca-60)** Must · M · The Outside C++ Adopter: Know what files my Windows release needs and build a package — after OCA-62
- [x] **[OCA-61](STORIES.md#oca-61)** Must · S · The Outside C++ Adopter · **Owner**: My release runs on a clean Windows machine — after OCA-60
- [x] **[OCA-64](STORIES.md#oca-64)** Should · S · The Outside C++ Adopter: Know the minimum hardware and Windows my players need — after OCA-61
- [x] **[OCA-65](STORIES.md#oca-65)** Should · S · The Outside C++ Adopter: Know which render backend to ship and what to do when a machine refuses it
- [ ] **[PI-14](STORIES.md#pi-14)** Should · M · The Pinned Submodule Integrator · **Owner**: My release ZIP is known to run on a player's clean Windows machine at a current pin — after OCA-61, SM-36
- [x] **[OCA-44](STORIES.md#oca-44)** Should · XS · The Outside C++ Adopter: Know which controllers work on Windows and how slots are assigned
- [ ] **[OCA-45](STORIES.md#oca-45)** Should · S · The Outside C++ Adopter: A one-player game that accepts the pad in whatever slot it is in — after OCA-44
- [x] **[OCA-49](STORIES.md#oca-49)** Could · XS · The Outside C++ Adopter: The default disabled menu row looks unavailable
- [x] **[OCA-52](STORIES.md#oca-52)** Could · XS · The Outside C++ Adopter: The menus guide routes me to the components I can copy
- [x] **[OCA-53](STORIES.md#oca-53)** Should · M · The Outside C++ Adopter: Keep my players' settings and saves on desktop and in the browser
- [x] **[OCA-56](STORIES.md#oca-56)** Must · M · The Outside C++ Adopter: Reference pages say how I get each type and what its parameters mean
- [x] **[OCA-58](STORIES.md#oca-58)** Should · S · The Outside C++ Adopter: The Application and Colour reference pages are navigable and correctly named
- [x] **[SRL-08](STORIES.md#srl-08)** Could · S · The Sample-Reading Learner: The reference tells me which headers are for game code — after OCA-58
- [x] **[OCA-57](STORIES.md#oca-57)** Could · M · The Outside C++ Adopter: From a reference page, reach the guide that teaches the type — after OCA-56, SRL-08
- [x] **[OCA-22](STORIES.md#oca-22)** Should · S · The Outside C++ Adopter: A browser preset that omits the render backend gets GL or a named refusal at configure
- [x] **[OCA-21](STORIES.md#oca-21)** Should · S · The Outside C++ Adopter: Install the Emscripten version the engine is built with
- [x] **[OCA-20](STORIES.md#oca-20)** Should · M · The Outside C++ Adopter: Add a browser build to my own project — after OCA-22, OCA-21, OCA-11
- [x] **[OCA-19](STORIES.md#oca-19)** Could · XS · The Outside C++ Adopter: Know what to do with the browser files the template copy brings — after OCA-20
- [x] **[OCA-23](STORIES.md#oca-23)** Should · S · The Outside C++ Adopter: Know what my web host must send — after OCA-20
- [ ] **[OCA-28](STORIES.md#oca-28)** Should · S · The Outside C++ Adopter: Know which browsers my players can be promised — after SM-09, SM-10
- [x] **[OCA-08](STORIES.md#oca-08)** Should · S · The Outside C++ Adopter · **Owner**: Know what accessibility the engine gives my game and what is mine to do — after OCA-05
- [x] **[BDP-24](STORIES.md#bdp-24)** Could · XS · The Browser and Phone Demo Player: The demo says what it offers players with access needs — after OCA-08
- [x] **[BDP-03](STORIES.md#bdp-03)** Should · M · The Browser and Phone Demo Player: ctest loads and plays the bundle page a visitor gets
- [x] **[BDP-04](STORIES.md#bdp-04)** Should · M · The Browser and Phone Demo Player: The bundle page's failure paths are tested — after BDP-03
- [x] **[PI-05](STORIES.md#pi-05)** Could · S · The Pinned Submodule Integrator: The engine tells my build which revision of it I am shipping — after OCA-67
- [x] **[SM-03](STORIES.md#sm-03)** Should · XS · The Sole Owner-Maintainer · **Owner**: Assign separate measurements to CPU phases, cadence, GPU execution and pipeline latency
- [ ] **[SM-01](STORIES.md#sm-01)** Should · S · The Sole Owner-Maintainer · **Owner**: Run the frame bench on the integrated Radeon, not the RTX 5080 — after SM-03
- [x] **[PI-09](STORIES.md#pi-09)** Should · S · The Pinned Submodule Integrator: Recheck and file or withdraw the live reports within the approved disclosure boundary — after OCA-72
- [x] **[SM-16](STORIES.md#sm-16)** Should · S · The Sole Owner-Maintainer: One index of reviews and which ledger is live — after SM-06
- [x] **[SM-19](STORIES.md#sm-19)** Could · XS · The Sole Owner-Maintainer: Gate the offline tooling tests in CI — after SM-21
- [ ] **[SM-25](STORIES.md#sm-25)** Should · S · The Sole Owner-Maintainer: Pin the Vulkan SDK, record the browser, and detect drift on a schedule — after SM-21

<details><summary>Why each is placed here</summary>

| Story | Why |
|---|---|
| SM-43 | No registered gap (closest G66): the brief's narrative history fights CONVENTIONS' no-history rule and costs every agent context. Closed 2026-10-27: 680 lines to 274 initially, 285 at PR verification, with the original and relocation map in [the dated record](../2026-10-26-agent-brief-history.md#verification-and-remaining-acceptance). Local tooling, CMake, native and website checks pass; [PR #59](https://github.com/matt439/Labrador/pull/59) passes all thirteen required checks at head `0ed6b12`, including all 81 Python tests and the website's 21 tests and 146-page build. |
| OCA-70 | G20, G37 (merged with PI-03): no written pin-move procedure. Tested once on the walkthrough and once by SM-36's bump. Closed 2026-10-26: the guide *Move your engine pin*, linked from Start your own project's step 2, the guides index and the reference index. An agent followed it once, moving a v0.1.0 walkthrough project to v0.2.0; that walk found that a project started before the template's tests has no test presets, which the page now says. SM-36's bump is the second run. |
| OCA-71 | G38: the engine's suite at a pin. Rides OCA-70's page. Closed 2026-10-26: the page's last section. Run once in that walk at v0.2.0: 20 of 20 entries passed, and the consumer's and the submodule's `git status` were both clean afterwards. |
| SM-36 | G73: ColourWars' pin is 45 commits behind. The math-split migration is done (77a7a7f), so the first move is an ordinary bump; first in Next rather than Now because OCA-11 already supplies a consumer check and the bump is cheapest with OCA-68's backfill and OCA-70's procedure (critique considered Now). Closed 2026-10-26 on this side: the owner set the cadence, each new minor tag within a week of its cut, and ColourWars moved its pin to `v0.2.0` the day that tag was cut, with its CI green. Writing the rule down and any gameplay record are ColourWars' own and are not tracked here. |
| FES-04 | G99, G100: the exclusions list omits scripting, hot reload and community. Moved from Now to the top of Next (critique: anti-persona minors were crowding primary majors). |
| FES-02 | G100: Get Started asks for Visual Studio before pointing at the exclusions. |
| SRL-02 | G95: Concepts reads without building minimal. Moved to Next by the critique. |
| SRL-04 | G89: Examples misstates LineSweeper's reading size. Moved to Next by the critique. |
| SRL-05 | G90: the README's open question understates the unchecked layer rule. Moved to Next by the critique. |
| SM-09 | G28, G23 (merged with BDP-05): M3's cheapest open question, on owned hardware (Big Sur MacBook Air). Moved to Next before OCA-28 (critique). |
| OCA-48 | G124, G125 (merged with SM-39): the owner has a physical XInput pad (owner fact), so the wait is gone. One session settles mapping and backgrounded input. |
| BDP-06 | G23: Firefox, the physical pad in the browser, a real tab switch and a real context loss. The pad is owned, so it moves from Later and shares OCA-48's session. |
| OCA-29 | G10 (major): no test target for a game's rules. The consumer job (OCA-11, Now) builds it. |
| OCA-30 | G11 (major): no start-up smoke check for a Windows game. |
| OCA-12 | G61: first-build time and the public-clone path are unobserved from a cold cache; OCA-11's cold-cache log can supply it. |
| OCA-33 | G110: no CI workflow an adopter can copy; shares steps with OCA-11. Closed 2026-10-26: the walkthrough's `ci.yml` ran unchanged in matt439/labrador-mygame, the walkthrough project at v0.2.0, green twice on windows-latest (runs 40242503246, cold cache, and 40242709388, cache restored); *Check your game in CI* links both. |
| OCA-35 | G12: translucent textures must be premultiplied, and nothing says so. Follows OCA-34 (Now). Closed 2026-10-25: the sprites guide says the engine does no premultiplication at load and that texconv's `-pmalpha` does it, and Troubleshooting has a fringe-or-halo entry that points there. |
| OCA-38 | G129: the guide's XWBTool command makes a bank the browser refuses. Closed 2026-10-25: the command has `-nc` and says why. XWBTool `may2026` was run on the sample's three WAVs; the engine's reader loads its bank, and refuses the same command without `-nc` as compact entries. |
| OCA-39 | G130: which sound formats load on both platforms. Closed 2026-10-25: the audio guide lists what the reader accepts and refuses, mono or stereo, the browser's rate range, the sample's format as the default, how to export PCM and MS-ADPCM, and the browser's memory cost. A 2,000 Hz bank was loaded in headless Chrome and failed as the guide says. MS-ADPCM was found to need `adpcmencode3`: XAudio2 refuses every block size FFmpeg's encoder can write. |
| OCA-36 | G13: guide samples borrow paths that break when copied. Closed 2026-10-24: the solid-rectangle recipe uses the engine's own white texture (`white_texture_name`), so no sample borrows LineSweeper's, and the collision, components and local_multiplayer READMEs each list the paths outside their folder they still use and what a copy does about them. |
| OCA-62 | G16 (major, merged with PI-12): no notice list for what a Windows game ships. |
| OCA-60 | G14 (major): no release-package guidance or install rules. The unexplained dependency on OCA-29 is removed (critique). |
| OCA-61 | G15 (major): nobody had run a release on a clean Windows install. Windows Sandbox is interactive and owner-run. Closed 2026-10-26: the template's release folder, built at `2fdb322`, passed its smoke test in Windows Sandbox (10.0.26100, no runtime DLLs in System32, a virtual GPU over the host's), recorded on *Ship a Windows release* and in the walkthrough's README. Whether it draws on a player's GPU needs a real machine. |
| OCA-64 | G105: the OS and hardware floors the code enforces are unstated. |
| OCA-65 | G150: which render backend to ship and what a refusal looks like. |
| PI-14 | G77. Windows Sandbox half run 2026-10-28: ColourWars CI's Release ZIP (ColourWars `26a249d`, engine `1e60fb2` = v0.2.0) on Windows 11 Enterprise 10.0.26100 with no Visual C++ runtime in System32 and no Visual Studio, through `tools/consumer_check/sandbox.py`'s configuration with a scripted run. The sandbox's GPU was the host's RTX 5080 passed through (paravirtual driver 10.0.26100.1150; host driver 32.0.16.1692), so the game drew rather than refusing. Synthetic keys reached a keyboard-seat match and its pause menu; closing the window exited 0. Not checked: a match with the owner's physical pad, the Radeon iGPU (SM-01), a non-paravirtual GPU; the record itself belongs in ColourWars. |
| OCA-44 | G121: which controllers work on Windows and how slots are assigned. Closed 2026-10-26: input and troubleshooting state XInput compatibility, system user slots and replug behavior; the website type check, 14 tests and build pass. |
| OCA-45 | G122: a one-player game that accepts the pad in whatever slot it is in. Implemented 2026-10-26: minimal greeting and confirmation scan all slots; native MinimalTests, MinimalSmokeTests and InputTests pass, and the guide quotes the loop. Still open for the required dated physical two-pad/unplug run; no such run is claimed. |
| OCA-49 | G116: the default disabled row is indistinguishable. Closed 2026-10-26: default dim_gray (105) is below gray (128), an applied-paint luminance regression passes in native UiTests, the guide and CHANGES agree, and a generated palette was visually reviewed. Clang compiled the case; its link was denied replacing the existing UiTests.js output, so no browser/Node runtime pass is claimed. |
| OCA-52 | G111: the menus guide does not route to the components. |
| OCA-53 | G108 (major): settings and saves on desktop and in the browser. On 2026-10-24 its engine half, where a game may write, was split out as OCA-76 (Later); this story owns the guide and browser example. Closed 2026-10-27: the guide quotes a standard-library JSON writer, Unicode per-user Windows location, IDBFS preRun population and awaited sync. Native compile/link and website checks pass. After the earlier GPU startup denial and a reload-driver navigation fix, Chrome 156.0.8078.48 starts empty, writes volume 37 through C++, awaits sync and reloads to C++ reading {version: 1, volume: 37}. This establishes same-origin/profile reload persistence, not retention after eviction or cleared site data. |
| OCA-56 | G139 (merged with SRL-10): reference pages do not say how a game gets each type. Closed 2026-10-26: eight public headers now state input accessors/shell ownership, Text/Label font/resource/error/anchor/radian contracts, state initialization timing and camera arithmetic. Non-comment lines are unchanged. Independent implementation review, website validation, all five native backend builds and focused headless tests pass; clang compiles the affected browser sources, while its existing UiTests.js replacement is access-denied. |
| OCA-58 | G141 (merged with SRL-12): mislabelled members and an unreadable Colour page. Closed 2026-10-26: initializer calls no longer name fields, fields have individual anchors, and static constants use a compact list. Website check, 14 tests and build pass; generated Application has max_threads as its own h3 and Colour has a Constants entry with all heading anchors at most 41 characters. |
| SRL-08 | G93: which headers are for game code. Closed 2026-10-26: every published header has an explicit validated audience, reference pages show its marker and indexes group the audiences. Website type check, 21 tests and build pass; the four required helper pages show renderer-writer markers. |
| OCA-57 | G140 (merged with SRL-11): reference pages do not lead to the guide or concept that teaches them. Closed 2026-10-26: generated Explained in lists use authored guide/concept links, API code and source excerpts with the same link rules, deterministic ordering and edit invalidation. Website check, 21 tests and build pass; all six required reference pages show guide/concept links. Removal and render-order regressions are covered. |
| OCA-22 | G50: a browser preset defaults to d3d11. Moved from Later and ranked before OCA-20 (critique) so the adopter page needs no workaround. |
| OCA-21 | G46: installing the pinned Emscripten; also carries NMS-05's EMSDK and 6.0.12 criterion. |
| OCA-20 | G09 (major): no page for adding a browser build to an adopter's project. |
| OCA-19 | G39: what to do with the browser files a template copy brings. |
| OCA-23 | G51, G52: hosting requirements only in an internal plan, and a wrong claim about the MIME type. |
| OCA-28 | G23: which browsers can be promised. After SM-09 so Safari reads 'checked on X' or 'out of reach', and after SM-10 (Now). Two pages wait on it: the website's guides/browser.mdx and guides/web-host.mdx link Platform support in place of this statement, and are repointed to it in the commit that closes this story. |
| OCA-08 | G119 (merged with SM-38). Closed 2026-10-27 with the owner's approved aim in [PHILOSOPHY, UI](../design/PHILOSOPHY.md#ui) and the public Menus guide's accessibility section. The engine supplies input, focus/navigation and browser Tab preservation; games own styling, contrast, palettes, actions, bindings and any rebinding. LineSweeper aims for keyboard/controller play, clear focus, readable contrast, reduced-motion options and the full-name palette groups in BDP-25. Its documentation states verified support, planned features and remaining limitations. BDP-24 supplies the demo note. BDP-22's reduced-motion option and BDP-25's palettes have since landed; the palettes' player validation and the hardware checks remain open. |
| BDP-24 | G119. Closed 2026-10-27: the packaged host page has a visible support/limitations note referenced by Play and each fresh canvas, and the sample README agrees and assigns planned palettes to LineSweeper. The rebuilt Release bundle-page test passes note visibility/reference, replay and real browser Tab focus checks. Two desktop-width captures were reviewed; this is local bundle verification, not a live-site deployment or physical-controller, real-phone or screen-reader acceptance. [Evidence](../port/web.md#packaged-page-verification-2026-10-27). |
| BDP-03 | G33 (major): the page hosts serve had no automated test. Closed 2026-10-27: following the initial GPU startup denial, all 31 `wasm-release` entries pass locally in Chrome 156.0.8078.48 on the RTX 5080. The [PR #59 wasm job](https://github.com/matt439/Labrador/actions/runs/40419996949/job/121911813526), at initial head `0fffdf1`, also passes and reports `LineSweeperBundlePageTests` passed in 6.62 seconds on Chrome 156 with SwiftShader. [Evidence and limits](../port/web.md#packaged-page-verification-2026-10-27). |
| BDP-04 | G33: the bundle page's failure paths. Closed 2026-10-27: `LineSweeperBundlePageTests` passes locally in Chrome 156.0.8078.48 within the 31-entry `wasm-release` run, including context loss/restore, truncated-wasm failure and captured-config `onError` recovery UI. [Evidence](../port/web.md#packaged-page-verification-2026-10-27). The earlier GPU startup denial is superseded for this local check; no hosted or physical-device acceptance is added. |
| PI-05 | G53: no engine revision a consumer can stamp; two present callers (the web bundle and a client). The first tag (Now) gives the fallback value. Closed 2026-10-26: LABRADOR_SOURCE_REVISION exports a configure-time hash/dirty or unknown identity, with an own-root guard for archives. Offline checkout/consumer/dirty-refresh/worktree/unborn/archive/missing-Git and bundle fixtures pass; the existing CI lint job runs them. README documents reconfigure-before-packaging and the bundle build-time distinction. |
| SM-03 | G26. Closed 2026-10-27 by the owner's decision to split the measurements: [ratchet section 5](../performance/2026-09-19-g6f-ratchet.md#measurement-decision-sm-03) owns the field mapping; PHILOSOPHY's Performance section cites it. CPU update, recording/submission and frame-start cadence have separate p99s. GPU execution and pipeline latency require separate evidence with stated endpoints. The low-tier capture uses the existing presentation pacing and synchronised present; an unsynchronised seam option is not adopted (T5, T8). This Performance amendment was independent of OCA-08's UI position. SM-07 and the low-tier acceptance box remain open; no hardware result is added. |
| SM-01 | G01 (major): the bench cannot reach the iGPU. The XS first step (AMD driver, per-app preference, no code) moves it to Next beside SM-03 (critique, owner facts); a code option is argued only if that fails. |
| PI-09 | G74. Closed 2026-10-27 within the owner's minimal engine-focused disclosure boundary. The original fifteen are withdrawn as already answered, with individual dispositions in the [reports table](../review/colourwars-reports/README.md#filing-recheck-2026-10-27). The two newer reports were rechecked at `1b3120b`: 23 is fixed with a failing-before/passing-after native activation regression, and 25 is accepted narrowly with implementation split into PI-21. Both answers use public engine evidence only, so no duplicate issue or private exhibit is needed. |
| SM-16 | G29: one index of reviews and which ledger is live. Follows SM-06 (Now). Closed 2026-10-26: docs/review/INDEX.md maps every review entry to its live ledger or frozen disposition, owns remaining D3D12/Vulkan candidate tracking and links the verification limits. AGENTS points there; historical review README.md is unchanged. Local paths and anchors were checked. |
| SM-19 | G32: the offline tooling tests were not gated in CI. Implemented 2026-10-26: the existing Windows lint job runs the tools unittest suite as a non-tolerated step beside the flash suite; priority 3 is tracked in STATUS.md. On 2026-10-27 all 81 local Python tests pass with no skips, superseding the earlier execution-policy blocker. Closed 2026-10-27: a throwaway head of [PR #74](https://github.com/matt439/Labrador/pull/74) at `fb67d6c` added one test that fails on purpose, and the [lint job](https://github.com/matt439/Labrador/actions/runs/40470680489/job/122076786279) went red at *Run the offline tooling tests*: 82 tests ran on windows-latest, the deliberate one failed, the step exited 1 and the job concluded failure. The throwaway commit was then replaced and never merged. |
| SM-25 | G133: the Vulkan SDK floats and drift is undetected. Implemented 2026-10-26: SDK 1.4.357.0 and its official SHA-256 are pinned, Monday 03:17 UTC forces the full CI matrix, and the page runner logs browser identity. On 2026-10-27 [LunarG metadata](https://vulkan.lunarg.com/sdk/home) confirms the pin/hash. After PR #59's hosted download received HTTP 403, a local GET with default urllib reproduces Cloudflare 403/error 1010; identifying the application as LabradorCI returns 200 from the same URL. The helper now sends that application identity. The exact pin/checksum and terminal handling of HTTP 403 are unchanged. The repaired [PR #59 Vulkan job](https://github.com/matt439/Labrador/actions/runs/40422181511/job/121919076713), at head `0ed6b12`, downloads the pinned SDK, verifies its SHA-256, installs it and passes 18 enabled tests. No scheduled run exists when checked on 2026-10-27; observing one remains open. |

</details>

## Later

The browser in depth for adopters, a kinder hosted demo, the measured low tier and phones, couch and text depth, and the maintainer's guards against drift

**Scheduling amendment, 2026-10-28.** Ready Later batches may run while Next
waits on hardware, a scheduled run or an owner session. Keep their order below,
skip only the items whose own prerequisites are unmet, and return to an earlier
item when its gate clears. This does not complete Next or relax its acceptance.
TODO's NOW names the next ready batches. Later
work includes code and tools as well as guides: the join screen (L19), working
settings and pointer menus (L20), text examples (L21), strict asset keys (L26),
verification tooling (L28/L29) and the new sample checks/examples (L35).

**Maintenance lane, added 2026-10-28:** L36 to L47 may run alongside the
product batches above, in their own order and with their own prerequisites.
They cover comment audits, source/test organisation and backend responsibility
reviews under [Code hygiene](../code-hygiene.md). This makes hygiene available
without waiting for all feature stories. A long cohesive file may receive a
keep verdict; an unproven backend split is reviewed before implementation.
L41 adds the bounded shared-table exception repair selected from the source
audit on 2026-10-29, with its acceptance below.
L42 follows it with direct regression coverage of the shared byte cursor.
L43 closes confirmed path-spelling gaps in the existing engine include wall.
L44 follows the next source audit's malformed-JSON boundary repair.
L45 takes its negative authored animation-count diagnostic next.
L46 repairs the existing StepPosition range guarantee for large periods.
L47 follows the next audit's sound-bank publication invariant.

**Contract follow-up, 2026-10-29:** SRL-17's source audit files SRL-22 within
L32, after the audit and before the glossary. Correcting a false bank-pointer
lifetime promise and missing unit/loan contracts gives callers firmer rules
for that explanation; this bounded documentation repair takes priority over
adding more tutorial prose. It selects no API redesign or runtime change.

The [feature candidates](../feature-candidates.md) add six proposals beyond
the existing `GAP#` comparison. They need selection before implementation and
are not counted as ready stories. A selected proposal gets a story here, with
its acceptance and dependencies; the dated STORIES.md and comparison stay as
written.

**Done when**

- The page contract, the LineSweeper bundle's factory contract, browser troubleshooting and the browser notices are public
- The demo's failure messages are written for players, it probes support before downloading, pauses on focus loss, honours reduced motion, offers the three agreed colour-vision-deficiency palette groups with their verification status stated, and every landing shadow meets 3:1 contrast
- Separate low-tier CPU phase and cadence p99s on the integrated Radeon are published with their pacing rule and cadence verdicts, and pipeline latency has its own evidence with stated endpoints; a blocked measurement records its reason and keeps the acceptance item open
- A real iPhone has played the development page and the live demo, with the LAN-serve procedure written in that session; a pacing method exists and runs on the owner's displays are recorded
- PI-10 has decided each filed report, and only then is the engine staging helper built or declined
- Golden regenerations leave a review trace, the five-backend-plus-wasm sweep is one command per area, and site preview and rollback are written down
- LineSweeper's documented layer boundary fails the build when crossed, and a copyable animation example uses the public sprite-sheet API
- The hygiene batches remove verified comment debt, organise independent test/tool responsibilities and record ownership-based keep/split decisions for backend candidates

**Lands as**

1. L1 Page contract for an adopter module: OCA-26
2. L2 Browser troubleshooting and the gesture rule: OCA-24, OCA-25
3. L3 Browser notices: OCA-63
4. L4 Check my page loads and quits: OCA-31
5. L5 LineSweeper bundle factory contract for other hosts: OCA-74
6. L6 Verify a web bundle before deploying: SM-27
7. L7 Demo failure paths a player can act on: BDP-15, BDP-16, BDP-17
8. L8 Demo controls, focus and phone instructions: BDP-11, BDP-12, BDP-13, BDP-14
9. L9 Demo contrast, reduced motion and colour-vision-deficiency palettes: BDP-21, BDP-22, BDP-25
10. L10 Phone fallback data use: BDP-20 (closed; existing on-demand video accepted)
11. L11 Play again stays fast: BDP-18
12. L12 Host page fits a phone held upright: BDP-09
13. L13 Local bench procedure, lock verdict and four-core hold (PHILOSOPHY amendment E): SM-04, SM-05, SM-02
14. L14 Take and publish the low-tier per-claim measurements: SM-07
15. L15 Headless XInput translation test: SM-40
16. L16 Browser pacing and phone top-out method: SM-08
17. L17 Real iPhone run with the LAN-serve procedure, then lift the phone gate: SM-14, SM-13, BDP-10 (also after BDP-09)
18. L18 Pacing on visitors displays: BDP-07
19. L19 Couch play: join screen: OCA-46
20. L20 Settings screen and pointer menus: OCA-51, OCA-50
21. L21 Text beyond ASCII: OCA-41, OCA-43, OCA-42, OCA-77
22. L22 Background music cost: OCA-40
23. L23 Debugging, recording renderer and DPI: OCA-18, OCA-32, OCA-66
24. L24 Answer the filed reports, then staging: PI-10, PI-19, PI-11
25. L25 Request status labels: PI-08
26. L26 Integrator edge contracts: PI-15, PI-16, PI-17, PI-18, PI-20, PI-21
27. L27 Backend rationale out of game-facing pages: SRL-09
28. L28 Golden review trace: SM-23
29. L29 Pin procedures and configuration sweeps: SM-24, SM-17, SM-18, SM-15
30. L30 Hosted demo lag and site operations: SM-26, SM-29, SM-30, SM-32
31. L31 Platform support links and the non-Windows configure refusal: NMS-03, NMS-04
32. L32 C++ introduction status (documented; no withdrawal), the learner audit, contract fixes and glossary: SRL-14, SRL-15, SRL-17, SRL-22, SRL-16
33. L33 LineSweeper design notes and the constants rule: SRL-18, SRL-06, SRL-07
34. L34 A per-user data directory from the engine, once a second caller asks: OCA-76
35. L35 Enforce the sample's layers and exercise sprite-strip animation: SRL-19, SRL-20
36. L36 Comment audits and their bounded correctness follow-up: public contracts, backend rules, implementation/build prose and sample teaching: SM-45, SM-46, SM-47, SM-48, SRL-21
37. L37 Split independent render and cloud-tool test responsibilities: SM-49, SM-51
38. L38 Extract Win32 input translation from window lifetime: SM-50
39. L39 Separate website reference indexing, links and page assembly: SM-52
40. L40 Review backend ownership, repair confirmed exceptional paths, then take the selected pipeline cut: SM-53, SM-54, SM-55, SM-56, SM-57, SM-58
41. L41 Preserve sound-group registration after allocation failure: SM-59
42. L42 Pin the shared byte reader's boundary and byte-order contract: SM-60
43. L43 Normalize the engine include wall and pin its boundaries: SM-61
44. L44 Reject a raw NUL instead of accepting only a JSON prefix: SM-62
45. L45 Refuse negative authored animation counts before container sizing: SM-63
46. L46 Keep a valid StepPosition fraction below one after rounding: SM-64
47. L47 Keep sound-bank lookup and volume notifications consistent on failure: SM-65
48. L48 Keep effect-group assignment and notification membership consistent on failure: SM-66
49. L49 State the existing geometric navigation tie rule: SM-67
50. L50 Align audio loader and test documentation with current content: SM-68
51. L51 Refuse unrepresentable sprite-font glyph extents before narrowing: SM-69
52. L52 Clarify resource presence and retained handle contracts: SM-70
53. L53 Preserve representable animation-strip origins through intermediate arithmetic: SM-71
54. L54 Refuse non-finite stored font metrics before layout: SM-72

**Stories**

- [x] **[OCA-26](STORIES.md#oca-26)** Should · S · The Outside C++ Adopter: One public statement of what my page owes the engine module — after OCA-20
- [x] **[OCA-24](STORIES.md#oca-24)** Should · S · The Outside C++ Adopter: Troubleshoot my browser build — after OCA-20
- [x] **[OCA-25](STORIES.md#oca-25)** Could · XS · The Outside C++ Adopter: Know that a one-shot sound before the first gesture is dropped in a browser
- [x] **[OCA-63](STORIES.md#oca-63)** Could · S · The Outside C++ Adopter: Know the notices my browser build must ship — after OCA-62
- [x] **[OCA-31](STORIES.md#oca-31)** Could · S · The Outside C++ Adopter: Check my game's page loads and quits — after OCA-20
- [x] **[OCA-74](STORIES.md#oca-74)** Could · S · The Outside C++ Adopter: Embed the LineSweeper bundle on my own site from a published contract — after OCA-26, OCA-23
- [x] **[SM-27](STORIES.md#sm-27)** Could · S · The Sole Owner-Maintainer: Verify a web bundle before deploying it, from this repository — after PI-05
- [x] **[BDP-15](STORIES.md#bdp-15)** Should · S · The Browser and Phone Demo Player: Failure messages a player can act on — after BDP-04
- [x] **[BDP-16](STORIES.md#bdp-16)** Could · S · The Browser and Phone Demo Player: A graphics context that never comes back offers a way forward — after BDP-04
- [x] **[BDP-17](STORIES.md#bdp-17)** Could · S · The Browser and Phone Demo Player: Know before downloading that my browser cannot run the game — after BDP-04
- [x] **[BDP-11](STORIES.md#bdp-11)** Could · S · The Browser and Phone Demo Player: The game pauses when it loses my keys
- [x] **[BDP-12](STORIES.md#bdp-12)** Could · S · The Browser and Phone Demo Player: One complete, consistent list of controls on the page and in the game
- [x] **[BDP-13](STORIES.md#bdp-13)** Could · XS · The Browser and Phone Demo Player: A phone sees phone instructions, not keyboard ones — after BDP-12, BDP-03
- [x] **[BDP-14](STORIES.md#bdp-14)** Could · XS · The Browser and Phone Demo Player: A controller lifts the phone gate, as the page says it will
- [x] **[BDP-21](STORIES.md#bdp-21)** Could · S · The Browser and Phone Demo Player: A landing shadow I can see for every piece — after OCA-08
- [x] **[BDP-22](STORIES.md#bdp-22)** Could · S · The Browser and Phone Demo Player: Fewer sparks when I have asked my system for reduced motion — after BDP-23
- [x] **[BDP-25](#bdp-25)** Could · M · The Browser and Phone Demo Player: Select palettes intended for Protanomaly / Protanopia, Deuteranomaly / Deuteranopia and Tritanomaly / Tritanopia
- [x] **[BDP-20](STORIES.md#bdp-20)** Could · XS · The Browser and Phone Demo Player: Keep the existing 4.3 MB on-demand phone fallback video
- [x] **[BDP-18](STORIES.md#bdp-18)** Could · S · The Browser and Phone Demo Player: Playing again many times stays fast and keeps the graphics — after BDP-03
- [x] **[BDP-09](STORIES.md#bdp-09)** Could · S · The Browser and Phone Demo Player: The host page fits a phone held upright — after BDP-03
- [ ] **[SM-04](STORIES.md#sm-04)** Should · S · The Sole Owner-Maintainer: A local run procedure for LineSweeperFrameBench
- [ ] **[SM-05](STORIES.md#sm-05)** Should · S · The Sole Owner-Maintainer: Apply the ratchet's lock verdict to a non-NVIDIA local run — after SM-04
- [ ] **[SM-02](STORIES.md#sm-02)** Should · S · The Sole Owner-Maintainer: Reproduce the four-core hold, and say what the bench's fan-out measures — after SM-03
- [ ] **[SM-07](STORIES.md#sm-07)** Should · M · The Sole Owner-Maintainer · **Owner**: Take and publish low-tier CPU phase/cadence p99s and separate pipeline-latency evidence — after SM-04, SM-05, SM-02, SM-01, SM-03
- [x] **[SM-40](STORIES.md#sm-40)** Could · S · The Sole Owner-Maintainer: A headless test of the XInput translation
- [x] **[SM-08](STORIES.md#sm-08)** Should · S · The Sole Owner-Maintainer: A method for judging browser pacing (R5) and timing the phone top-out frame (R6) — after PI-19
- [ ] **[SM-14](STORIES.md#sm-14)** Should · S · The Sole Owner-Maintainer · **Owner**: Serve a development page to a phone on my network
- [ ] **[SM-13](STORIES.md#sm-13)** Should · M · The Sole Owner-Maintainer · **Owner**: Record a real iPhone run of the development page and the live demo — after SM-14, SM-08
- [ ] **[BDP-10](STORIES.md#bdp-10)** Could · XS · The Browser and Phone Demo Player: Lift the phone gate after the real iPhone and host layout pass — after SM-13, BDP-09
- [ ] **[BDP-07](STORIES.md#bdp-07)** Could · M · The Browser and Phone Demo Player · **Owner**: Smooth play on the displays visitors actually use — after SM-08
- [x] **[OCA-46](STORIES.md#oca-46)** Could · M · The Outside C++ Adopter: See a join screen for couch play I can copy
- [x] **[OCA-51](STORIES.md#oca-51)** Could · M · The Outside C++ Adopter: Wire a settings screen to what the engine can actually set — after OCA-53
  Source implemented 2026-10-29. The gallery offers Windowed/Full screen,
  requests the mode through Application and silently re-syncs from its report
  at initialization and before input each tick. Status text describes a
  request; difficulty/volume labels remain explicit callback examples with no
  sound bank. The settings guide quotes the compiling window code and takes
  the story's audio-guide alternative: existing SoundBank assignments,
  AudioResources groups and a percent-to-level slider callback, including
  retained-effect and one-shot behavior. Its two literal C++ fences compile
  together without executing them. Default/null native gallery/tests builds
  and all 25 ComponentsTests/262 assertions pass; the new case proves the real
  OptionRow's external-snapshot recipe, not GalleryState polling or a window.
  A separate source/API review found no defects. All seven standalone checks,
  website check, 160 website tests and the 143-page build pass; the built
  request/polling excerpts, audio snippets and section anchors were checked.
  Qualified 2026-10-29 at 1b5ecee. In headless Chrome 156 the wasm-release
  row entered canvas full screen. An external exitFullscreen re-synced it to
  Windowed while the status line kept the request, so no callback fired. The
  row's own Left left full screen, and Escape quit. ComponentsPageTests passes
  in wasm-release, wasm-debug and wasm-release-null. An untrusted request
  stayed deferred, so browser refusal is unobserved; it is not an acceptance
  term. Natively, the Win32 window replaced its style without `WS_VISIBLE`,
  so a shown window counted as hidden: GL drew white in full screen, and
  leaving full screen skipped the show and left a white client on every
  backend. Closed 2026-10-29: both style changes keep `WS_VISIBLE`. A new
  AppTests case takes a shown window through full screen and back and fails
  without the fix. The gallery was driven through full screen and back on
  x64-debug, -d3d12, -gl, -vulkan and x64-release, and every capture draws,
  with no white pixel, at 2560x1440 and back at 1280x720 inside a frame.
- [x] **[OCA-50](STORIES.md#oca-50)** Could · S · The Outside C++ Adopter: Make a menu work with a click or a tap
  Closed 2026-10-29. The [pointer-menu section](../../website/src/content/docs/docs/guides/menus.mdx)
  quotes `tapped_row()` and its focus-before-activation branch with live
  SourceFile anchors. It explains pressed edges, matching camera/viewport
  coordinates, target geometry, input priority and disabled activation, and
  gives the mouse route using the actual integer-to-world API. T1 keeps
  pointer policy in the game; the quoted LineSweeper helper remains touch-only.
  Missing/ambiguous start selectors were checked to fail; body edits refresh
  the live quote rather than claiming a frozen semantic checksum. Standalone
  checks, website check/160 tests and the final corrected 143-page build pass.
  The built excerpts, mouse recipe and T1 link were checked. No engine or
  gallery pointer feature or device acceptance is added.
- [ ] **[OCA-76](STORIES.md#oca-76)** Could · S · The Outside C++ Adopter: Ask the engine where my game may write, on Windows and in the browser — after OCA-53
- [x] **[OCA-41](STORIES.md#oca-41)** Could · S · The Outside C++ Adopter: My non-English literals draw the same on Windows and in the browser
- [x] **[OCA-43](STORIES.md#oca-43)** Could · S · The Outside C++ Adopter: Know which languages the text path can draw
- [x] **[OCA-42](STORIES.md#oca-42)** Could · M · The Outside C++ Adopter: Get my game's language from a string table to the screen — after OCA-41, OCA-43, OCA-34
- [x] **[OCA-77](STORIES.md#oca-77)** Could · S · The Outside C++ Adopter: Wrap a translated string to a width — after OCA-43, OCA-42
- [x] **[OCA-40](STORIES.md#oca-40)** Could · S · The Outside C++ Adopter: Know what background music costs and how to keep it out of the page download — after OCA-38
  Closed 2026-10-28. The [music section](../../website/src/content/docs/docs/guides/audio.mdx)
  states whole-bank memory on both platforms, a PCM/float size example and
  load-time overhead, embed versus preload at the existing roughly 1 MB
  content threshold, the `.data` release file and pre-startup fetch timing.
  It keeps streaming/async loading out of scope with T3; separating the download
  is not represented as on-demand playback. The sample README links the section.
  Website check, 160 tests and the 143-page build pass; the built heading/link
  and memory/preload text were checked. No backend or browser run is claimed.
- [ ] **[OCA-18](STORIES.md#oca-18)** Could · S · The Outside C++ Adopter: Debug my game in Visual Studio from the presets
  Source guide added 2026-10-29 under [Get Started](../../website/src/content/docs/docs/get-started/debug-your-game.mdx),
  with sidebar and follow-on links. It selects the copied project's x64-debug
  preset/MyGame target, quotes the actual init/update/manifest statements,
  follows executable-relative content and explains Graphics Tools and matching
  symbols. Stock Release does not request a game PDB; no new build flags or
  IDE configuration are added. The clean copied-project IDE walkthrough and
  breakpoint/content observation remain unrun; they need a person at the IDE.
  The IDE to record is Visual Studio Community 2026 18.9.2 (installation
  18.9.12120.119), read with vswhere on 2026-10-29. The story stays unchecked.
  Source review, all seven standalone checks, website check (zero diagnostics),
  160 website tests and the 144-page/433-file build pass. The generated page's
  three live excerpts, key instructions and sidebar/follow-on links were
  inspected with eight content and two navigation checks. No IDE or game ran.
- [x] **[OCA-32](STORIES.md#oca-32)** Could · S · The Outside C++ Adopter: Use the recording renderer in my own tests — after OCA-29
  Closed 2026-10-29. The [testing guide](../../website/src/content/docs/docs/guides/testing.mdx)
  adds a second consumer null-render/audio configure, matching build/test
  presets and a guarded draw-test target with the game include root. The
  inspector remains backend-internal/test-only at an engine pin; published.ts
  records that exclusion and Why links the narrowed claim to the guide.
  Its literal presets, CMake block and 79-line C++ example were extracted into
  a copied consumer. The regular configure has its original two tests and no
  draw target; null has three passing tests, including the new windowless draw
  case's 21 assertions of order, state, geometry and copied-record lifetime.
  Local dependencies were preinstalled and manifest installation disabled.
  Independent source review, standalone checks, website check/160 tests and
  the 143-page build pass; built API/source links were checked. This qualifies
  the native consumer example, not rasterization, browser or device output.
- [ ] **[OCA-66](STORIES.md#oca-66)** Could · S · The Outside C++ Adopter: Declare per-monitor DPI awareness and review the template at 150% scaling
- [ ] **[PI-10](STORIES.md#pi-10)** Could · M · The Pinned Submodule Integrator · **Owner**: An upstream answer for each filed report — after PI-09
  - Each report's decision and the commit that answered it: [the reports table](../review/colourwars-reports/README.md#the-decisions).
- [x] **[PI-19](STORIES.md#pi-19)** Should · M · The Pinned Submodule Integrator: Read what the engine's last frame cost, phase by phase
- [ ] **[PI-11](STORIES.md#pi-11)** Could · M · The Pinned Submodule Integrator: Stage the runtime files the engine makes my game need, without hand-keeping that list — after PI-10, OCA-60
- [ ] **[PI-08](STORIES.md#pi-08)** Could · S · The Pinned Submodule Integrator: See what happened to each request and which commit answered it — after OCA-72
- [x] **[PI-15](STORIES.md#pi-15)** Could · S · The Pinned Submodule Integrator: Launching my game hidden through STARTUPINFO is tested upstream
- [ ] **[PI-16](STORIES.md#pi-16)** Could · S · The Pinned Submodule Integrator: Drive my game past its title screen with injected input in a smoke run — after OCA-30
- [x] **[PI-17](STORIES.md#pi-17)** Could · XS · The Pinned Submodule Integrator: Know what JSON the engine reads for me and when my game must bring its own parser
- [x] **[PI-18](STORIES.md#pi-18)** Could · XS · The Pinned Submodule Integrator: The include wall says exactly what it protects in my build
- [x] **[PI-20](../review/colourwars-reports/content.md#12-json-from-memory-key-enumeration-source-spans)** Could · S · The Pinned Submodule Integrator: A misspelt key in a manifest or sprite sheet is refused, not read as its default
- [ ] **[PI-21](#pi-21)** Should · M · The Pinned Submodule Integrator: Send controller vibration through the engine's input seam — [scope and acceptance](../review/colourwars-reports/vibration.md)
- [x] **[SRL-09](STORIES.md#srl-09)** Could · S · The Sample-Reading Learner: Game-facing reference pages keep backend rationale out of my way — after SRL-08
  Closed 2026-10-29. RenderResources' device-release comment first says a game
  does not call it and identifies the shell callback. Its effects and actual
  device-loss precondition stay before an explicit renderer-writer marker;
  pimpl and lifetime rationale are also marked in place. The outlives and
  declaration-order contract remains visible. The allocator explanation now
  names both current reset sites instead of claiming creation is the only one.
  All 518 noncomment tokens are identical. All five native Debug RenderTests
  targets build and their headless tests pass, as do the seven standalone
  checks, website check, 160 website tests and 144-page/433-file build. The built
  reference's exact first method sentence, three markers, lifetime/precondition
  text and corrected rationale were inspected. No pixel, wasm, browser or device
  run is implied; existing qualification stops remain in force.
- [x] **[SM-23](STORIES.md#sm-23)** Should · S · The Sole Owner-Maintainer: Leave a trace that regenerated goldens were looked at
  Implemented 2026-10-29. AGENTS and its CLAUDE import require one exact
  `Golden-Reviewed: <path>` line per changed golden PNG in that commit's own
  message. The standard-library checker reads each introduced commit's actual
  parent diff/message, including root commits, deletion and rename paths;
  later aggregate declarations cannot cover earlier omissions. Missing/shallow
  history and merges fail explicitly. Nested invocation resolves the Git root,
  and Git reads disable implicit fetches, replacement refs, external diff and
  text conversion. The existing lint job gains full-history checkout and the
  check without changing jobs, matrix names or permissions. PRs use actual
  base/head SHAs; manual/scheduled/first-push single-head coverage is labeled.
  All 27 focused offline tests pass, including a replacement-ref regression.
  The cached comparison base `4d2f27f` through `c34e566` checks 30 commits with
  no changed goldens. All seven standalone checks, website check, 160 website
  tests and the 143-page build pass. Real golden files are unchanged.
  Hosted CI execution remains part of the owner's later PR; the
  checker establishes declarations, not visual judgment.
- [ ] **[SM-24](STORIES.md#sm-24)** Could · S · The Sole Owner-Maintainer: A written procedure for moving dependency and toolchain pins — after SM-25
  Source implemented 2026-10-29 in the
  [dependency/toolchain procedure](../dependency-updates.md), linked from
  CONTRIBUTING. It inventories exact pins versus moving selectors, names
  the coupled manifest/notice/migration changes and all native/golden/wasm/
  consumer/website qualification, with provider/runtime evidence kept distinct.
  SM-25's scheduled-run observation remains unfulfilled, so this story stays
  unchecked/BLOCKED on that dependency. No pin, installed toolchain, runner,
  action, provider setting or deployment changed.
  Independent source review found no issues. All 186 permitted Python cases
  pass without skips; the exact ten stopped PowerShell cases were excluded
  before execution from the 196-case inventory. Ninety offline Node cases,
  all seven standalone checks and the website's zero-diagnostic check,
  160 tests and 145-page/435-file build pass. The procedure's 31 local targets
  and CONTRIBUTING link were checked; no remote/provider response was tested.
- [x] **[SM-17](STORIES.md#sm-17)** Could · M · The Sole Owner-Maintainer: One command per area that runs every configuration a change must pass
  Planning slice implemented 2026-10-29. `tools/verify_changes.py` models the
  five native Debug selections, wasm-debug GL and CI's explicit wasm-release-null
  override without executing commands. Text/JSON output carries cwd/argv,
  runtime effects, required/forbidden registrations and unrun manual obligations.
  Optional preflight reads whitelisted tooling/environment/cache data, detects
  stale axes/compilers/emulators/browser selections, and leaves every phase
  `not-run`. Unsupported source/preset shapes and selectors fail explicitly.
  The [contract](../verification.md) limits this mapping to render paths;
  the real sweep and SM-18 remain open.
  All 31 focused synthetic tests pass and independent source review has no
  remaining findings. The actual checkout emits seven configurations/21 unrun
  phases. Read-only preflight exits 1 with eight missing current-shell tooling
  prerequisites; hashes of all seven existing caches remain unchanged. No
  compiler, SDK activation, CMake/CTest, generated module or browser was run
  by these CLI checks. This is planning evidence, not sweep qualification.
  All seven standalone checks, website check, 160 website tests and the
  143-page build pass.
  Executor source added 2026-10-29: explicit `--execute`, all-configuration
  preflight, required cache checks after configure and build, actual CTest JSON
  registrations, fresh logs/JUnit/report files and first-failure stopping.
  Disabled, skipped, missing, duplicate and failed suite results cannot become
  pass evidence from a zero exit code. The default remains a nonexecuting plan.
  All 59 focused tests pass (31 planner and 28 fake-command runner tests),
  including stale caches, interrupted commands and report-write failure.
  Independent review has no remaining findings. The actual default CLI still
  emits seven configurations/21 `not-run` phases. Closed 2026-10-29: the real
  `--execute` sweep passed all seven tracks (configure, build, test) with this
  machine's CTest JSON and JUnit accepted, and a shell without Emscripten
  failed at preflight naming the missing path before any build
  ([record](../verification.md#real-sweep-qualification-2026-10-29)). Manual
  device acceptance stays separate (SM-18).
  All seven standalone checks and website check/tests/143-page build pass.
- [ ] **[SM-18](STORIES.md#sm-18)** Could · S · The Sole Owner-Maintainer: A record of the configurations CI cannot run, per merged change — after SM-17
- [ ] **[SM-15](STORIES.md#sm-15)** Could · XS · The Sole Owner-Maintainer: Observe the null-backend wasm step running after a failed page step, and the job going red
- [x] **[SM-26](STORIES.md#sm-26)** Could · S · The Sole Owner-Maintainer: Notice when the hosted demo lags the engine — after PI-05
  Source implemented 2026-10-29. `tools/check_demo_lag.py` pins an explicit
  host checkout's HEAD and reads its committed BUILD.json, compares its clean
  full revision with local master (or an explicitly labeled ref), and counts
  only known ancestor history. Unknown/shallow/diverged history and malformed
  metadata fail explicitly. It performs no fetch, host write or deployment.
  [web.md section 7](../port/web.md#7-what-the-host-page-gets) states the re-sync
  trigger, committed-source command and distance/provenance limits.
  All 25 focused offline tests pass, including committed-versus-working data,
  real isolated histories, worktrees and Git routing/replacement isolation.
  Independent source review has no remaining findings. Closed 2026-10-29:
  `python tools/check_demo_lag.py --host-repository C:/Projects/PersonalWebsite`
  exited 0. It read host HEAD `d433560`'s committed
  `public/demo/linesweeper/BUILD.json` (clean bundle commit `1e60fb2`, v0.2.0),
  compared it with local master `b96bad6` and reported 182 commits behind. It
  says that it read committed metadata only: no live response or bundle bytes.
  All seven standalone checks, website check (zero diagnostics), 160 website
  tests and the 144-page/433-file build pass. No live request or deploy ran.
- [x] **[SM-29](STORIES.md#sm-29)** Could · S · The Sole Owner-Maintainer: A procedure for previewing and rolling back the site — after SM-31
  Source procedure added 2026-10-29 in [website/README.md](../../website/README.md#preview-and-recovery):
  select the matching successful workflow artifact, preserve its revision in
  a separate review checkout, serve its existing dist and record affected-page
  review, then check the intended production revision/routes/assets/search
  after deploy. The plan links that procedure and distinguishes a failed build
  preserving production from recovery after a regression has deployed.
  Cloudflare rollback steps were added 2026-10-29 from Cloudflare's versions,
  rollbacks and Wrangler command documentation: the dashboard Deployments
  route, `wrangler rollback <VERSION_ID> --message`, the 100-version limit and
  a version carrying its static assets. Closed 2026-10-29 with the owner's
  Cloudflare authorization. Wrangler 4.155.0 was signed in to the owner's
  account. `deployments list` and `versions list --name labrador` returned
  ten of each, and `deployments status` showed version `cedd5137` live from
  07:33 UTC. `rollback --help` and Cloudflare's command page confirm the
  documented syntax: `--message` skips the confirmation, and omitting the ID
  rolls back to the previous upload. The live footer named `b96bad6`, the
  master commit that deployment followed. An unknown route returned 404, and
  `sitemap-index.xml` named `labradorengine.com`. Website run 40836037018's
  artifact for `b96bad6` was downloaded into a fresh `b96bad6` review checkout
  and served with `npm run preview`: its footer named `b96bad6`, Home, Why,
  Docs and Versions answered 200, and an unknown route answered 404. No
  rollback was performed and the dashboard was not opened. The wrangler route
  and the version history it reads were confirmed against the real Worker.
  Independent source review, seven standalone checks, 90 offline Node tests,
  website check (zero diagnostics), 160 website tests and the 144-page/433-file
  build pass. Across two runs, all 182 permitted Python cases pass: the seven
  CMake fixtures received explicit installed tool paths after initially skipping.
  The exact ten stopped PowerShell worker/trace cases were never executed.
- [ ] **[SM-30](STORIES.md#sm-30)** Could · S · The Sole Owner-Maintainer · **Owner**: Change the website gate in one place — after SM-29
- [x] **[SM-32](STORIES.md#sm-32)** Could · S · The Sole Owner-Maintainer: Keep the site's LineSweeper footage in step with the sample
- [x] **[NMS-03](STORIES.md#nms-03)** Could · S · The Native Multi-Platform Shipper (anti-persona, for now): The Why page's Platform support section links the plans, their scope, and what would change the answer — after OCA-01
  Source added 2026-10-29. Why links the existing Android scoping record and
  states no Android build, iOS after Android, the engine-only endpoint without
  packaging/store work, and no Linux/macOS plan. Five future support conditions
  name client demand, CI preset, real-device run, measured p99 and packaging.
  Existing browser/Safari/phone qualifications remain. Tool-version advice
  links Get Started instead of repeating a year in the platform section.
  The same overview now describes the implemented multiplayer join/ready
  assignments and the full CI sweep, correcting its stale fixed-slot and
  every-change claims. No platform, support promise or schedule was added.
  Closed 2026-10-29: the built Why page links
  `https://github.com/matt439/Labrador/blob/b96bad6d1c25e4fb3d74199c76de4612f725b915/docs/port/android.md`,
  and an HTTPS HEAD request for it answered `200 OK`.
  Independent source review, all seven standalone checks, website check
  (zero diagnostics), 160 website tests and the 144-page/433-file build pass.
  The emitted section's five gates, six facts/qualification checks, local
  source target/URL and absence of schedule text were inspected, along with
  the three overview corrections. No HTTP, browser or device check ran.
- [x] **[NMS-04](STORIES.md#nms-04)** Could · S · The Native Multi-Platform Shipper (anti-persona, for now): Configuring on a non-Windows host refuses early and points at the platform statement — after OCA-01
  Source implemented 2026-10-29. The native base in the engine presets and
  copied-game walkthrough now requires hostSystemName Windows; all ten engine
  and two copied-game native choices inherit it. Wasm bases/choices stay
  unrestricted, with Linux/macOS-hosted wasm explicitly unverified. The existing
  post-toolchain fallback names Windows (Win32) and the browser (Emscripten)
  and links the platform statement. No platform implementation or pre-project
  EMSCRIPTEN guard was added. Copied projects have a minimal README migration
  note. Supported APIs, dependencies, compile options and runtime are unchanged.
  The planner models only this exact condition and validates all ten native
  and both wasm selections; default planning still emits seven tracks/21 unrun
  phases. All 63 focused planner/fake-runner tests pass. Actual Windows CMake
  listing shows all 12 engine and four copied-game presets; an isolated
  unsupported-target branch fixture checks the diagnostic. Neither is an Ubuntu
  observation. Closed 2026-10-29 on WSL 2 Ubuntu 26.04 LTS with CMake 4.2.3,
  against this checkout at `/mnt/c/Projects/Labrador`. `cmake --list-presets`
  lists only `wasm-debug` and `wasm-release`. The copied-game presets
  (`website/src/walkthrough/CMakePresets.json`, copied alone into a temporary
  folder) list the same two. `cmake --preset x64-debug` answers `Could not use
  disabled preset "x64-debug"` in both. A plain `cmake -S . -B <temp>` stops at
  `engine/CMakeLists.txt:100`, which names Windows (Win32) and the browser
  (Emscripten) and links the platform-support section. No Linux wasm configure
  was attempted, so Linux-hosted wasm stays unverified, as documented.
  All seven standalone checks and the website's zero-diagnostic check,
  160 tests and 144-page/433-file build pass. The built host note, platform
  link and copied-project condition/wasm entries were inspected. Final
  independent source review found no issues.
- [x] **[SRL-14](STORIES.md#srl-14)** Could · S · The Sample-Reading Learner: State the C++ introduction's substantial pre-production progress and unfinished status
- [x] **[SRL-15](STORIES.md#srl-15)** Could · XS · The Sample-Reading Learner: Get Started tells me where to go if I do not know C++ yet — after SRL-14
  Closed 2026-10-29. Get Started gives a small console-program preparation
  exercise, names pointers/references and object lifetime, and tells the reader
  when to return. It links the preserved companion-introduction status under
  its own heading. No external resource was selected. The adjacent CI claim
  now states the full sweep's scope rather than promising builds on docs-only
  changes. This is a preparation route, not evidence from a beginner session
  or completion of the companion series.
  All seven standalone checks and the website check, 160 tests and
  144-page/433-file build pass. The emitted preparation text, return route,
  status link/anchor and unfinished-status wording were inspected.
- [x] **[SRL-17](STORIES.md#srl-17)** Could · M · The Sample-Reading Learner: A recorded check that the game-facing API is teachable to a beginner — after SRL-14
  Closed 2026-10-29. The [interim source audit](../api-teachability.md) states
  a repeatable method and dated run over all 84 published game-code headers,
  with 29 screened declarations, language/term/loan evidence and the explicit
  eight-position draw_sprite verdict. A separate source review reconciles the
  inventory and signatures. All three failed contract facets are filed in
  SRL-22; positional API redesigns were declined for this audit with T1/T4
  reasons, rather than inferred from a count. The reference introduction links
  the run. This closes the source-audit story, not T10's unfinished series or
  acceptance with actual beginners.
  All seven standalone checks and the website's zero-diagnostic check,
  160 tests and 144-page/433-file build pass. The emitted revision-pinned
  audit link, scope and qualification were inspected. All 84 ledger paths,
  source hashes and 93 local Markdown targets reconcile.
- [x] **[SRL-22](#srl-22)** Should · S · The Outside C++ Adopter, The Sample-Reading Learner: Complete the unit and retained-loan contracts found by the source audit — after SRL-17
- [x] **[SRL-16](STORIES.md#srl-16)** Could · M · The Sample-Reading Learner: A guide to the C++ and engine terms the minimal sample uses — after SRL-17
  Closed 2026-10-29. [Reading the C++ in minimal](../../website/src/content/docs/docs/concepts/reading-minimal.mdx)
  explains all nine requested language/rendering topics through 11 live
  SourceFile excerpts, linked from first-change and the game concept page.
  It distinguishes ownership/loans, shallow const, capture/result/view lifetimes
  and Windows/D3D11 terms from neutral engine interfaces. The sample was not
  simplified or changed. Independent source review found no issues. All seven
  standalone checks and the website's zero-diagnostic check, 160 tests and
  145-page/435-file build pass. Nine emitted sections, 11 revision-pinned
  source links and both editorial incoming links were inspected. No beginner
  session, compiler, device, browser or SDK qualification is claimed.
- [x] **[SRL-18](STORIES.md#srl-18)** Could · S · The Sample-Reading Learner: Run LineSweeper's rules tests knowing what I need and where it works
  Closed 2026-10-29. The sample README owns the root configure, targeted
  LineSweeperTests build and anchored one-entry CTest recipe; Examples links
  there. It states the root engine/vcpkg configure prerequisites, wasm
  chainload and dated host/preset evidence without claiming other hosts.
  Windows Debug-null reconfigure reused installed dependencies with manifest
  installation off; the rules target and exact CTest pass on Debug-null and
  default Debug at ae82fc0. No engine/sample target, device, wasm activation
  or fresh dependency install was qualified by that check.
- [x] **[SRL-06](STORIES.md#srl-06)** Could · S · The Sample-Reading Learner: LineSweeper's design notes give their reasons without sending me to internal records
  Verified 2026-10-29 in current source. No README rewrite was needed: zero
  CLAUDE.md citations remain; all five survey/review citations already state
  the UI-client, data-layout, wrong-row, arithmetic-budget or predeclared-
  threshold reason locally. The action-map paragraph names ARCHITECTURE's
  input entry and the public boundary/source-combination rationale, matching
  report 21's current disposition. Independent source review and root's
  paragraph reads agree; frozen review records were preserved.
  Both LineSweeper documentation closures pass all seven standalone checks
  and the website's zero-diagnostic check, 160 tests and 145-page/435-file
  build. The emitted revision-pinned rules-test link, unique README anchor,
  exact recipe and both one-entry native results were inspected.
- [ ] **[SRL-07](STORIES.md#srl-07)** Could · S · The Sample-Reading Learner: Rename non-macro uppercase constants and migrate callers in both repositories
- [x] **[SRL-19](#srl-19)** Should · S · The Sample-Reading Learner: Make LineSweeper's layer boundary a build check
- [x] **[SRL-20](#srl-20)** Could · M · The Sample-Reading Learner: Copy a working sprite-strip animation example
- [x] **[SM-45](../code-hygiene.md#sm-45-public-header-contracts-and-rationale)** Should · M · The Sole Owner-Maintainer: Audit public-header contracts and oversized rationale
- [x] **[SM-46](../code-hygiene.md#sm-46-backend-recording-and-synchronization-comments)** Should · S · The Sole Owner-Maintainer: Audit backend recording and synchronization comments
- [x] **[SM-47](../code-hygiene.md#sm-47-implementation-and-build-comment-archaeology)** Should · S · The Sole Owner-Maintainer: Remove stale implementation and build-comment history
- [x] **[SM-48](../code-hygiene.md#sm-48-sample-teaching-comments)** Could · S · The Sole Owner-Maintainer: Refresh sample teaching comments against current code
- [x] **[SRL-21](#srl-21)** Should · S · The Sample-Reading Learner: Make the top-out banner's culling extent enclose its drawn text
- [x] **[SM-49](../code-hygiene.md#sm-49-pixel-test-organisation)** Should · M · The Sole Owner-Maintainer: Separate the pixel-test harness and independent contract cases
  Closed 2026-10-29: the sole harness owner and five case groups preserve 50
  complete cases, 12 subcases, 238 assertion sites and golden/content bytes.
  RenderPixelTests passes on the four rasterising Debug presets, and with and
  without S3TC on wasm-debug and wasm-release. The owner records the counts.
- [x] **[SM-51](../code-hygiene.md#sm-51-cloud-tooling-test-organisation)** Could · S · The Sole Owner-Maintainer: Split cloud tooling tests by production module
- [x] **[SM-50](../code-hygiene.md#sm-50-win32-input-translation)** Should · S · The Sole Owner-Maintainer: Extract backend-private Win32 input translation
- [x] **[SM-52](../code-hygiene.md#sm-52-website-reference-organisation)** Could · M · The Sole Owner-Maintainer: Separate reference indexing, prose linking and page assembly
- [x] **[SM-53](../code-hygiene.md#sm-53-vulkan-devicebuffer-ownership-review)** Could · M · The Sole Owner-Maintainer: Review Vulkan device/buffer ownership for a justified split
- [x] **[SM-54](../code-hygiene.md#sm-54-renderer-construction-review)** Could · M · The Sole Owner-Maintainer: Review D3D12/Vulkan pipeline construction ownership
- [x] **[SM-55](../code-hygiene.md#sm-55-web-audio-transport-review)** Could · S · The Sole Owner-Maintainer: Review the Web Audio transport and C++ lifetime boundary
- [x] **[SM-56](../code-hygiene.md#sm-56-vulkan-exceptional-cleanup)** Should · S · The Sole Owner-Maintainer: Preserve Vulkan temporary ownership through publication and recovery failures
- [x] **[SM-57](../code-hygiene.md#sm-57-d3d12-replacement-texture-publication)** Should · S · The Sole Owner-Maintainer: Keep a replacement texture's descriptor consistent if publication fails
- [x] **[SM-58](../code-hygiene.md#sm-58-d3d12-sprite-pipeline-value-owner)** Could · M · The Sole Owner-Maintainer: Extract the reviewed D3D12 root-signature/PSO value owner — after SM-57
- [x] **[SM-59](#sm-59)** Should · S · The Outside C++ Adopter, The Sole Owner-Maintainer: Leave a failed new sound-group registration absent and retryable
- [x] **[SM-60](#sm-60)** Could · S · The Sole Owner-Maintainer: Pin ByteReader's boundary, cursor and byte-order behavior directly
- [x] **[SM-61](#sm-61)** Should · S · The Sole Owner-Maintainer: Enforce the existing include wall across normalized paths and backend subtrees
- [x] **[SM-62](#sm-62)** Should · S · The Outside C++ Adopter, The Sole Owner-Maintainer: Refuse raw NUL bytes throughout a JSON input span
- [x] **[SM-63](#sm-63)** Should · S · The Outside C++ Adopter, The Sole Owner-Maintainer: Name a negative authored animation count before constructing its strip
- [x] **[SM-64](#sm-64)** Should · S · The Outside C++ Adopter, The Sole Owner-Maintainer: Preserve the stated StepPosition fraction range across valid periods
- [x] **[SM-65](#sm-65)** Should · S · The Outside C++ Adopter, The Sole Owner-Maintainer: Leave a failed first sound-bank registration absent and retryable
- [x] **[SM-66](#sm-66)** Should · S · The Outside C++ Adopter, The Sole Owner-Maintainer: Preserve an effect's previous group when first notification registration fails
- [x] **[SM-67](#sm-67)** Could · S · The Outside C++ Adopter, The Sole Owner-Maintainer: Know which candidate wins an exact navigation tie

- [x] **[SM-68](#sm-68)** Could - S - The Outside C++ Adopter, The Sole Owner-Maintainer: Read accurate audio content, loader and recording contracts

- [x] **[SM-69](#sm-69)** Should - S - The Outside C++ Adopter, The Sole Owner-Maintainer: Receive a file-named error for an unrepresentable font glyph extent

- [x] **[SM-70](#sm-70)** Could - S - The Outside C++ Adopter, The Sole Owner-Maintainer: Distinguish resource presence from retained names and handles

- [x] **[SM-72](#sm-72)** Should - S - The Outside C++ Adopter, The Sole Owner-Maintainer: Refuse non-finite stored font metrics before layout

<a id="sm-72"></a>
### SM-72. Require finite metrics in sprite-font files

Selected 2026-10-29 after SM-71, while hardware, wasm and external gates
remain open. Two independent source reviews trace the file reader through
resource publication and layout. A complete 76-byte, one-glyph RGBA file
can carry a quiet NaN in any of four metric fields while satisfying the
existing count, integer-extent, atlas and payload checks. Source arithmetic
then admits non-finite glyph positions or silently misleading measurements.
The audit executed no bad-value layout, device or floating-exception probe.

| Rank | Current evidence and value | Decision |
|---|---|---|
| 1 | Stored x_offset, y_offset, x_advance and line_spacing pass directly into font layout without a finite check; missing-glyph membership does not validate them. | Select a bounded readable-file contract refinement and field-specific diagnostics before atlas upload in the resource-factory path. |
| 2 | The preceding quad audit confirms separate float conversion of rectangle fields, so its suspected integer-intermediate problem is absent. | Leave that code unchanged; add no speculative regression or atlas constraint. |
| 3 | Programmatic Font validation and arbitrary finite-value accumulation need separate API and arithmetic decisions. | Preserve those mechanisms, every finite bit pattern and the generic byte reader's intentional IEEE bit support. |

This finite-file requirement is a new supported-content choice, motivated by
T6's loud content failures. Neither the previous header nor a verified
external MakeSpriteFont specification supplies an existing finite clause.
The reader header and CHANGES must state the selected contract and behavior
change. The canonical checked-in test font's 286 metric words were inspected
with integer exponent masks and are finite; that says nothing about other
assets, authoring tools or rendered output.

**Acceptance**

- Reject quiet NaN and either infinity in the three stored glyph metrics
  and line_spacing with std::runtime_error naming the file and exact field,
  and the zero-based glyph index for glyph fields.
- Preserve all finite metric bits, including signed zero, negative values,
  finite extremes and subnormals. Add no positivity, magnitude, normalization
  or atlas policy. Finite source values do not guarantee finite arithmetic
  for arbitrary strings or accumulated layout.
- Leave ByteReader's bit contract, programmatic Font construction and all
  layout/drawing code unchanged. Preserve the seven existing reader cases,
  including the real font and SM-69 integer-extent controls.
- Add two parser-only cases: twelve complete-file refusals and rotating
  finite-bit controls over all four fields. Never construct or lay out a
  Font from the non-finite fixtures. A safe old-parser characterization may
  run only the refusal case; its expected twelve failures establish parser
  acceptance, not a layout failure or floating-exception result.
- Run full headless RenderTests on all five native Debug selections and
  Release null after source/cache/object guards, with two build workers and
  one test worker. Run eight standalone checks and website check/tests/build.
  Keep the required wasm selections, full CI and device evidence explicit.

**Qualification**

Implemented 2026-10-29. The file reader checks all three glyph metrics
and line_spacing with isfinite before returning parsed data. Refusal names
the file and exact field, plus the zero-based glyph index for glyph metrics.
The public reader contract and CHANGES describe this new supported-file
restriction. Valid finite data is neither rewritten nor normalized. Generic
ByteReader, programmatic Font construction, layout and drawing are unchanged.

Two engine/header candidates and the single existing test-source candidate
were reviewed independently. All three original hashes and 81 named source
hashes passed fresh guards before any tracked source write. Exact reversal
preserves previous engine code, header line endings and all seven existing
reader cases; only three standard includes and two cases are added to that
test source. No new CPP enumeration or test registration was required.

The refusal case covers four fields by quiet NaN and both infinities in
twelve complete 76-byte files, checking path, field and applicable glyph
index. Twelve rotating controls inspect all four returned metric words,
giving 48 exact bit observations across signed zero, ordinary positive and
negative values, finite extremes, minimum normal values and subnormals.
The existing real-font and integer-extent cases still pass. No non-finite
Font construction/layout or floating-exception probe ran. The optional
old-parser characterization was not run, so no old-code red is claimed.

Full RenderTests pass with zero failures or skipped cases:

| Configuration | Cases | Assertions |
|---|---|---|
| Debug D3D11 | 164 | 4381 |
| Debug D3D12 | 164 | 4381 |
| Debug GL | 164 | 4381 |
| Debug Vulkan | 164 | 4381 |
| Debug null | 190 | 4512 |
| Release null | 190 | 4512 |

Builds used two workers and tests one, serially. Installed dependency-cache,
backend, frozen-source and generated-object guards passed before each run:
21 render CPP objects, or 22 with null recording. Existing Assets object and
null-only recipe macro inventories were also checked; AssetsTests was not
rerun because no asset-loader source changed. No extra fixture tests ran.
These results establish file metadata and existing headless/CPU-recording
behavior, not rasterized output or hardware acceptance.

All eight standalone checks pass, including 49 include-wall fixtures.
Website verification reports zero type diagnostics, 160 passing tests,
145 pages, 436 files and no release snapshot. All website phase exits
are zero and its runner completed. Generated reference HTML contains the
finite-metric requirement and glyph-index diagnostic contract. All other
named source hashes still match after verification.

Wasm and full-CI qualification passed on 2026-10-29. Complete RenderTests
pass under Node with zero failures or skips: wasm-release and wasm-debug
161 cases/4375 assertions, CI's wasm-release-null override 187/4506. The
three cases fewer than native are the native-only HRESULT cases. Both new
cases ran in all three. CI run 40836037123 on b96bad6, which contains this
change, passed all twelve jobs, including RenderTests in every native Debug,
x64-release, wasm and wasm-null job. No device or browser acceptance is
claimed. No general finite-output guarantee, authoring-tool specification
claim or magnitude cap is introduced.

- [x] **[SM-71](#sm-71)** Should - S - The Outside C++ Adopter, The Sole Owner-Maintainer: Cache representable strip frames without intermediate overflow

<a id="sm-71"></a>
### SM-71. Compute the complete strip frame origin before narrowing

Selected 2026-10-29 after SM-70. The bounded strip-reader audit confirmed
that private calculate_frame multiplies index by width in int before adding
the first origin through RectangleI::offset. Three frames suffice to overflow
that intermediate even when every mathematical final rectangle fits.

| Rank | Current source evidence and value | Decision |
|---|---|---|
| 1 | First x INT_MIN, width 1073741824 and count 3 yield origins INT_MIN, -1073741824 and 0, with all right edges representable. The last product is 2147483648, outside int. Existing JSON integer and strip contracts admit the fields and negative origins. | Select whole-expression wide arithmetic and successful cancellation regressions. |
| 2 | Small products can also make a final origin unrepresentable: x INT_MAX-1, width 1, count 3 ends at INT_MAX+1; the lower analogue ends at INT_MIN-1. | Define the direct constructor's error and annotate authored failures in the same bounded repair. |
| 3 | Generic RectangleI arithmetic, positive dimensions, atlas bounds and direct negative-count policy need separate decisions/evidence. | Keep out; preserve existing geometry and SM-63's authored count guard. |

Two independent source reviews establish the constructor/cache call chain,
actual integer fields, admitted schema and three-entry allocation. They also
confirm negative-product cancellation with x INT_MAX and width -1073741825.
Neither source review executed an extreme fixture or the original
overflowing expression; no old-code red is claimed. The upper/lower refusal examples do not imply that
all their derived right edges fit; only the computed origin field is in scope.

**Acceptance**

- Evaluate the complete origin-plus-index-times-width expression in int64
  with casts before arithmetic. Check the final x against the field's int
  range, then assign directly. Do not reject a wide intermediate product
  that the origin cancels back into range, or narrow an offset first.
- Cache unchanged y, width and height. Keep public frame_rect an indexed
  const reference with its existing invalid-index behavior. Prevalidate the
  last origin before capacity reservation for positive counts: the linear
  sequence's endpoints bound all origins. Zero count computes no coordinate;
  zero width and representable signed geometry remain supported. Leave the
  existing direct negative-count path untouched and unexecuted.
- Document std::out_of_range from direct construction for an unrepresentable
  cached x, with frame/index/coordinate context. The authored sheet path
  annotates that failure with file, strip location and name as runtime_error;
  do not catch unrelated allocation failures or alter the existing count guard.
- Add a focused, explicitly enumerated metadata-only RenderTests source and
  authored AssetsTests cases. Check both cancellation directions, ordinary
  and boundary/zero controls, upper/lower refusal, zero count and bad indices
  only after compiling the repaired expression. Preserve existing test bodies.
- Run complete native RenderTests and AssetsTests across all five Debug
  render backends and Release null with fresh source/cache/object/macro
  guards, two build workers and one test worker. Run eight standalone checks
  and website check/tests/build; publish the constructor contract and record
  the behavior change in CHANGES. Keep required wasm/full-CI and device
  qualification explicit; source implementation is independent of those stops.

**Qualification**

Implemented 2026-10-29. The complete coordinate is evaluated in int64
before an int-range check and direct x assignment. Positive counts validate
their last origin before capacity reservation; zero count skips coordinate
calculation. Cached y/width/height, timing, looping, public index checks and
the existing direct negative-count behavior remain unchanged. The authored
path annotates only out_of_range, leaving JSON, length_error and bad_alloc
behavior outside that catch. Both public contracts and CHANGES describe the
new behavior without a valid-content migration.

Four engine/header candidates and three test/enumeration candidates passed
independent source reviews. Fresh guards held all originals, the absent new
test path and 76 named source hashes before application. Reversal preserves
engine code outside the selected changes, the old Assets test prefix and
every old CMake byte outside one alphabetical source entry. The new CPP and
appended C++ are ASCII; existing CMake Unicode comments and line endings are
preserved. A temporary transport interruption affected a known-unexecuted
source-proof read only; state was reconciled after restoration, with no
duplicate source application or test run.

Six direct metadata cases cover both product-cancellation directions,
ordinary/signed/zero-width/exact-origin controls, upper/lower refusals, zero
count and invalid indices. Three authored cases check cancellation, a failure
after a valid preceding strip with the correct file/strip/index/coordinate
diagnostic, and zero-count unused geometry. All extremes run only against
the repaired expression. No original UB path or old-code red was executed.
These checks inspect stored rectangles, not unrelated derived edges or pixels.

Complete native suites pass with zero skipped cases:

| Configuration | Render cases/assertions | Assets cases/assertions |
|---|---|---|
| Debug D3D11 | 162 / 4012 | 79 / 637 |
| Debug D3D12 | 162 / 4012 | 79 / 637 |
| Debug GL | 162 / 4012 | 79 / 637 |
| Debug Vulkan | 162 / 4012 | 79 / 637 |
| Debug null | 188 / 4143 | 80 / 647 |
| Release null | 188 / 4143 | 80 / 647 |

Builds used two workers and tests one, serially across configurations and
suites. Each runtime followed installed-cache/backend, candidate/source,
generated-object and null-macro guards. RenderTests has 21 CPP objects or
22 with null recording; AssetsTests has seven. The existing null-only text
recipe remains absent from non-null builds. No extra fixture tests ran.
These are headless metadata and CPU-recording results, not an API device,
rasterized output or hardware acceptance.

All eight standalone checks pass, including 49 include-wall fixtures.
Website verification reports zero type diagnostics, 160 passing tests,
145 pages, 436 files and no release snapshot. All phase exits are zero,
the runner completed and the final process return is zero. Generated HTML contains the
new constructor and authored-loader contracts. Full source/candidate
identity and other boundary hashes remain verified afterward.

Wasm and full-CI qualification passed on 2026-10-29. Under Node, RenderTests
passes 161 cases/4375 assertions in wasm-release and wasm-debug and 187/4506
in CI's wasm-release-null override. AssetsTests passes 79/637 and 80/647, with
zero failures or skips. All nine new cases ran in each configuration. CI run
40836037123 on b96bad6, which contains this change, passed all twelve jobs,
including both suites in every native Debug, x64-release, wasm and wasm-null
job. No device, rasterized-output or browser acceptance is claimed. There is
no allocation cap, general RectangleI refactor, atlas/positivity policy or
new graphics feature.

<a id="sm-70"></a>
### SM-70. Clarify Registry presence and resource lifetime

Selected 2026-10-29 after SM-69's native/source checkpoint, from the
independent follow-up already ranked by SM-68 and SM-69. Their remaining
hardware, wasm and external qualification gates do not block this prose
repair. Current source and eleven existing Registry cases already establish
the behavior; no new resource API or failure policy is needed.

| Rank | Evidence and value | Decision |
|---|---|---|
| 1 | The published contains comment says a released resource is indistinguishable from loaded, but the code returns false for empty slots and existing release/refill cases assert that. The introduction also overstates live-or-throw as applying to resolve rather than get. | Select the caller-contract correction. |
| 2 | Header and test prose say device loss releases Font data. All five actual RenderResources release methods release textures while its header keeps font/sheet metadata. | Include the same-header lifetime example and generic test introduction in the bounded two-file repair. |
| 3 | Custom-storage exception behavior and extremely large table indices need additional evidence and a separate contract decision. | Do not change implementation or add speculative test infrastructure. |

**Acceptance**

- State that get returns a live resource or throws, while resolve retains
  the name-to-slot contract. Missing names and empty/released slots fail
  contains; a released name still resolves and get throws until refill.
- Use texture storage as the device-loss example. Describe the existing
  test as generic release/refill coverage, without implying an actual
  graphics-device session or that font metadata is released.
- Apply only four whole-line comment replacements in registry.h and its
  existing test introduction. Preserve declarations, directives, code,
  every one of eleven complete test bodies, original line endings and ten
  related source/fact hashes. Add no duplicate behavioral test or API.
- Run eight standalone checks and website check/tests/build; verify the
  corrected public Registry reference. Classify `not consumer-visible`
  under the existing policy, with no behavior entry or caller migration.
  Keep all actual device, browser, wasm and owner gates unchanged.

**Qualification**

Implemented 2026-10-29 as four complete line-comment replacements in
the header and the comment preceding its existing release/refill case.
Fresh original/candidate hashes and ten related source/fact hashes pass.
Exact forward/reverse reconstruction preserves all declarations, includes,
directives and executable code; all eleven complete test bodies and both
files' original ASCII/CRLF form remain unchanged. Independent review agrees
on the same candidate bytes and current contract evidence.

The get/resolve/contains implementation and existing assertions establish
the corrected presence behavior. Source checks of all five render resource
release methods support the texture example; no new device-loss session is
implied. No duplicated behavior test or new CoreTests run was needed for
this comment-only change. The Assets inline example and publication entries
are unchanged.

All eight standalone checks pass, including 49 engine include fixtures.
Website check reports zero type diagnostics, 160 tests pass and the build
produces 145 pages and 436 files with no release snapshot. All phase exits,
runner completion and the final process return are recorded as success.
Generated Registry HTML contains the corrected lookup/presence/lifetime
contracts and omits both obsolete claims. This is `not consumer-visible`
under the existing policy: there is no source or behavior migration.

The separate animation-strip source audit remains a candidate until its
small-input arithmetic exhibit and contract are independently confirmed.
No hardware, wasm, browser, network or owner gate is resumed or qualified
by this documentation work.

<a id="sm-69"></a>
### SM-69. Refuse unrepresentable sprite-font glyph extents

Selected 2026-10-29 after SM-68 from a bounded DDS/sprite-font source audit.
The existing file reader constructs RectangleI with `right - left` and
`bottom - top` evaluated in signed 32-bit arithmetic on the supported
targets. Valid signed edge fields can have an unrepresentable difference.
The count and atlas guards cannot prevent this later arithmetic overflow.

| Rank | Source evidence and value | Decision |
|---|---|---|
| 1 | Opposite int32 endpoints produce plus or minus 4,294,967,295 for either width or height. A complete 76-byte, one-glyph, 1x1 RGBA font reaches the subtraction after reserving one entry. The RectangleI constructor only stores ints. | Select the narrow arithmetic repair and fixed-code regressions. |
| 2 | Registry presence and obsolete font device-loss prose have a separately reviewed two-file correction. | Keep for an independent follow-up after this correctness repair. |

Two independent source reviews agree on all four width/height overflow
examples and the complete small-file layout. No malformed fixture was run
against the original overflowing expression, and no old-code failure is
claimed. The DDS layout guards were source-consistent in the bounded audit;
no additional format defect or arbitrary parser limit is inferred.

**Acceptance**

- Promote the signed edge operands before subtraction, compare both
  differences with RectangleI's int representation bounds, then narrow and
  construct. A rejected extent throws runtime_error naming the file and
  glyph/rectangle problem before the overflowing expression can occur.
- Preserve every representable difference, including negative and zero
  extents. Add no atlas-coordinate, Unicode, positive-glyph-area, metric or
  line-spacing policy, dependency, public API or general parser framework.
- Use the existing ScratchFile fixture and a complete 76-byte synthetic
  font to check the four overflow directions only against the fixed reader.
  Require useful extent/glyph diagnostics as well as the path, so a fixture
  that failed for some unrelated truncation would not pass. Verify ordinary,
  zero and exact signed-limit controls by reading stored metadata only.
  Preserve the existing real-font and malformed-file test bodies.
- Run complete headless RenderTests on all five native Debug render
  backends and Release null, after current source/cache/generated-object
  boundary checks. Use two build workers and one test worker, serially.
  Run eight standalone checks and website check/tests/build; keep the
  required wasm/CI and device/browser qualifications explicit and stopped.
- Record the malformed-input behavior change in CHANGES.md. Valid font
  content needs no migration; do not regenerate or claim rasterized images.

**Qualification**

Implemented 2026-10-29 with checked int64 operand promotion, int bounds
before narrowing and file/glyph/extent diagnostics. The two frozen CPP/test
candidates passed independent source review; a separate reversal reproduces
the original reader outside the conversion and every existing test body.
Both candidates are ASCII/CRLF, with no new header, translation unit, API,
dependency or registration. CHANGES records the malformed-input behavior
change and unchanged representable geometry; valid fonts need no migration.

The two added cases use complete 76-byte files. Four rows reject width and
height overflow in both directions after the guard is compiled. Four safe
controls retain ordinary geometry, zero area and both signed-limit extent
combinations. Stored coordinates/sizes, line spacing and the complete 1x1
atlas/pixel payload are checked without measuring, drawing or deriving
extreme geometry. The real-font and existing malformed-file cases remain.
No original overflowing expression was run and no old-code red is claimed.

Complete native RenderTests pass with zero skipped cases:

| Configuration | Cases | Assertions |
|---|---|---|
| Debug D3D11 | 156 | 3942 |
| Debug D3D12 | 156 | 3942 |
| Debug GL | 156 | 3942 |
| Debug Vulkan | 156 | 3942 |
| Debug null | 182 | 4073 |
| Release null | 182 | 4073 |

Builds used two workers and tests one, serially across the six selections.
Installed dependencies, backend selections, 68 named source hashes and the
two candidate hashes were guarded. Before each runtime the generated test
object inventory matched twenty CPPs, or twenty-one with null recording;
no additional fixture tests were selected. Other boundary sources held.
These are headless metadata/refusal and CPU-recording results, not graphics
API device, rasterized output or hardware acceptance.

All eight standalone checks pass, including 49 include-wall fixtures.
Website check reports zero type diagnostics, 160 tests pass and the build
produces 145 pages and 436 files with no release snapshot. Each website phase
and the full runner record success; the final process return was zero.
The existing public reader/error contract is present in generated HTML.

Wasm and full-CI qualification passed on 2026-10-29. Complete RenderTests
pass under Node with zero failures or skips: wasm-release and wasm-debug
161 cases/4375 assertions, CI's wasm-release-null override 187/4506. The
three cases fewer than native are the native-only HRESULT cases. Both new
cases ran in all three. CI run 40836037123 on b96bad6, which contains this
change, passed all twelve jobs, including RenderTests in every native Debug,
x64-release, wasm and wasm-null job. These remain headless results; no
device, rasterized-output or browser acceptance is claimed. This does not
block the independent Registry prose follow-up or reopen any deferred
owner/product decision.

<a id="sm-68"></a>
### SM-68. Align audio content and test documentation with current behavior

Selected 2026-10-29 after OCA-77's native/source checkpoint. Current TODO
acceptance still needs the stopped browser, device and SDK routes, actual
owner sessions or external events; SM-58 retains its SM-57 prerequisite.
This bounded maintenance item uses the owner's autonomous-work permission
and does not reopen any IDEA/GAP product decision.

The source audit ranked these concrete opportunities:

| Rank | Finding and value | Decision |
|---|---|---|
| 1 | The audio parser says an effect may not name a wave omitted from the array, while its implementation, public definition contract and existing effect-only-wave test deliberately use their first-seen union. Correcting the false restriction protects accepted content. | Select. |
| 2 | Registry presence wording confuses released slots with loaded resources, despite existing release/refill tests. | Keep as an independent follow-up. |
| 3 | Audio teaching prose says the repository cannot contain an XWB or build a content-backed bank, calls one test file the only observable audio coverage and overstates what null recordings prove. The sample ships original PCM/XWB and suite selection has changed. | Combine with rank 1 as one coherent audio documentation repair. |

The related public definition and builtin manifest-loader comments repeat
the false clone/container premise. Source inspection of the actual handler,
loader and all three backend open functions establishes the narrow contract:
the builtin loader parses the definition before opening a bank, including
optional content; only open-time runtime errors are in its optional-content
catch. Missing, unreadable or unplayable containers can use that optional
fallback; a declared-wave mismatch is a logic error that propagates. Manual
callers choose their own order. Null does not read a container and supplies
no missing-container error or physical listening evidence.

**Acceptance**

- Correct the evidenced comments in seven source/header/test files while
  preserving executable tokens, declarations, includes, preprocessor
  directives, test bodies, registration and accepted content. Preserve each
  file's existing line endings. Add no mirrored behavioral test or new API.
- Guard original and candidate bytes before applying. Reverse only the
  complete line-comment replacements to reproduce every original exactly,
  with no line splices. Keep the original five-file preparation frozen;
  account explicitly for the two-file supplement and its intentional public
  header replacement in the combined guards.
- Keep pure definition parsing, container reading, recorded commands and
  state, and actual listening distinct. Retain existing no-mixer/no-clock
  limits and qualify error ordering to the builtin manifest loader.
- Run eight standalone checks and website check/tests/build. Verify the
  corrected public definition contract in generated HTML and source/excerpt
  coupling. Existing implementation and test bodies are unchanged, so no
  old-code failure or new engine-runtime result is expected or claimed.
- Classify `not consumer-visible`: documentation matches existing behavior;
  no source migration or CHANGES behavior entry is required. Preserve all
  current device, browser, wasm, network and owner qualification gates.

**Qualification**

Implemented 2026-10-29 in seven files and ten complete line-comment
replacements. The original five-file preparation and earlier supplement are
preserved; the final two-file supplement is accounted for explicitly.
Original/candidate guards and exact forward/reverse reconstruction pass,
including no line splices, unchanged logging and the original line endings.
The three test files retain their complete test bodies: seven definition,
seven silent-bank and fifteen null cases. Those are source inventories,
not newly executed test totals.

The actual builtin handler/loader and three backend open paths support the
qualified ordering and failure statements. This is source evidence; no
audio device, container playback or listening session was run. The parser's
union, clamping, optional-bank substitution and configuration-specific test
registration are unchanged. No behavior entry or caller migration is needed.

All eight standalone checks report exit zero, including 49 engine include
fixtures. Preserved website logs report zero type diagnostics, 160 passing
tests and a completed 145-page build with 436 files and no release snapshot.
Artifact inspection verifies the corrected public definition contract.
The final process poll returned Unknown process id, so the wrapper exit
code is unavailable; its complete phase-success logs and generated artifact were
reconciled without rerunning checks. This receipt limitation supplies no
new engine-runtime, wasm, device, browser or physical-audio qualification.

A later source audit found an actual glyph-extent overflow in the font
reader. That bounded correctness repair takes priority over the independent
Registry prose follow-up; the owner's deferred product decisions remain out.

<a id="sm-67"></a>
### SM-67. State navigation's existing exact-tie rule

Selected 2026-10-29 after SM-66 from its second-ranked source finding. Both
navigation.h and FocusGroup::add claim ordering does not matter, but the
current walk retains the first candidate when both directional gap and
cross-axis distance tie. Enabled focus candidates retain registration order.
The wrap rule also minimizes cross-axis distance when the furthest gaps tie,
which its full-semantics prose omits. Correcting these confirmed statements
has bounded scope and no new navigation policy. The next audit returns to
OCA-41's existing literal-encoding story; lower-value loader prose remains
separate. Deferred input/product decisions and geometry redesign stay out.

**Acceptance**

- Pin the existing walk with distinct symmetric rectangles in all four
  directions, both ahead and on wrap, and reverse supplied candidates.
  Check the widget overload's null/self filtering preserves first-candidate
  pointer mapping. Check FocusGroup's enabled registration order after clear
  and reverse registration, and disabling/re-enabling a tied candidate.
- First run the new cases with original public headers and unchanged
  implementation. They should pass the existing behavior; no failing old
  implementation result is expected or claimed. Then replace both false
  order-independence statements with the first-candidate exact-tie rule and
  document wrap's existing cross-axis tie-break.
- Keep declarations, implementation, CMake and all existing tests unchanged
  apart from the added cases. Publish both corrected header contracts. This
  changes documentation and coverage only: record `not consumer-visible`
  in the commit message, with no CHANGES behavior entry.
- Run complete native default Debug, Debug null and Release null UiTests,
  eight standalone checks and website verification. Default UI cases use
  stubs; the null suites also exercise recording-renderer/font-texture APIs
  under the null-backend gate, without a window or graphics API device.
  Keep wasm/Node unrun at the standing activation stop and full CI separate.
  These cases establish no rendered presentation, hardware or pad acceptance.

**Closed 2026-10-29.** Both public headers now state that the first candidate
wins when geometric scores tie exactly; FocusGroup explains enabled
registration order and the wrap prose includes the existing cross-axis
tie-break. Four added cases pass against the original headers with 38
assertions, pinning existing behavior rather than claiming an old-code
failure. They cover all four directions, wrap and reversed lists, widget
pointer mapping past null/self entries, clear/reverse registration and
disable/re-enable filtering. Exact reverse reconstruction preserves every
original file byte outside the comment replacements and appended tests;
both implementation files and the six registration/fixture paths are unchanged.

Complete native UiTests pass with zero skips: Debug null and Release null
each 46 cases/218 assertions, default Debug 44/207. Eight standalone checks
and website verification pass: zero type diagnostics, 160 tests, 145 pages,
435 files and no snapshots. Both corrected contracts are present in the
published reference. Null recording/font cases create no window or graphics
API device; new cases use only geometry/stubs. Declarations and behavior are
unchanged. Wasm/Node remains unrun at the existing activation stop, and full
CI remains required before landing; no visual, hardware or pad claim.

<a id="sm-66"></a>
### SM-66. Publish an effect's group after notification membership

Selected 2026-10-29 after SM-65 from its fresh source shortlist:

1. **Effect-group membership:** set_effect_group stores the group before its
   first notification-list insertion can throw. Playback can then multiply
   by a failed assignment that live group updates cannot reach. A small
   ordering fix and prepared null-content failure fixture give a concrete
   public regression: take first.
2. **Navigation exact ties:** equal geometric scores retain the first supplied
   candidate despite the public order-independence claim. Pinning and stating
   the current tie rule is bounded and independent: take separately.
3. **Audio loader teaching prose:** the effect-only wave union and checked-in
   sample content contradict old comments. Correct only verified statements
   in a later bounded audit; do not change accepted content to fit the prose.

The defect is source-derived until reproduced. T2/T6 favor keeping the
observable assignment and its notification membership together. Existing
deferred product decisions, JSON numeric policy and hardware work remain
outside this slice; no new engine abstraction is needed.

**Acceptance**

- Reuse the current native Debug DLL CRT scope without changing its helper,
  optional filter, calibration or existing cases. Preconstruct the null
  device, content bank, stopped voices, handles and group levels before
  arming. Only set_effect_group executes under the hook; no playback,
  recording, separate lookup, assertion or cleanup does. Sweep observed allocation
  points through uninjected success without assuming an ordinal or real OOM.
- With original SoundBank bytes, a failed first assignment must be shown to
  affect later play/setter levels incorrectly. Restore/disarm before those
  observations. Then finish notification insertion before writing the group,
  so a refused assignment keeps its previous group and can be retried.
- Preserve validation, silent-bank behavior and successful reassignment.
  Check retry, duplicate assignment, effect-handle notification order and
  one update each to playing/paused members and the reassigned group.
- Select the null-only case with a PRIVATE AudioTests definition inside its
  existing null-backend block. Add no public macro, production test seam,
  allocator API or test entry. Keep default Debug headless and API-free.
- Publish the narrow failure contract and CHANGES correction, check excerpt
  coupling and review all changed source/test bytes. Run complete native
  Debug null, default Debug and ordinary Release null AudioTests, eight
  standalone checks and the website check/tests/build. New injection is
  native MSVC Debug DLL CRT plus null audio only. Wasm/Node remains unrun at
  the standing activation stop; full CI remains required before landing.
  Recorded null calls establish no endpoint or listening acceptance.

**Closed 2026-10-29.** `set_effect_group` now writes the group only after the
existing sorted notification insertion succeeds. Its validation, silent-bank
path, de-duplication and handle order are unchanged; the header and CHANGES
state preservation of the previous group on a refused first assignment.

With original production bytes, the new null-content regression completes
67 assertions with two actual failures: after refused insertion, play records
0.4 instead of the ungrouped 0.8 and the setter records 0.3 instead of 0.6.
The observed first eligible allocation is evidence, not a fixed test ordinal.
The original 17-assertion and filter 12-assertion calibrations pass. Exact
reverse reconstruction preserves the complete prior test file, including its
helper, filter and four existing cases. The new case arms only the stopped
effect's handle-list insertion and checks retry, no immediate push, duplicate
assignment, playing/paused fan-out and reassignment after restoration.

Complete native AudioTests pass with zero skips: Debug null 43 cases/684
assertions, default Debug 33/544 and Release null 38/309. Eight standalone
checks and website verification pass: zero type diagnostics, 160 tests, 145
pages, 435 files and no release snapshots. The published reference includes
the previous-group guarantee. The null selector is PRIVATE to AudioTests;
no engine macro, test entry or production seam was added. Controlled failure
qualifies native MSVC Debug DLL CRT plus null audio only; default Debug and
Release retain their ordinary headless coverage. Wasm/Node remains unrun at
the standing activation stop and full CI remains required before landing.
No platform audio API device, endpoint or listening result is claimed.

<a id="sm-65"></a>
### SM-65. Keep sound-bank publication and notification registration together

Selected 2026-10-29 after SM-64 from a fresh ranked source audit:

1. **Bank publication:** add_sound_bank publishes the Registry entry before
   bank_order can allocate and throw. A recovered failure can leave a bank
   readable but absent from the every-bank group-volume walk, violating the
   ledger's stated invariant. Reuse the calibrated native failure scope and
   silent banks for a bounded public-state reproduction: take first.
2. **Effect group membership:** set_effect_group assigns the record's group
   before notification-list insertion can throw. Subsequent play and live
   group updates can disagree. Its meaningful regression needs a prepared
   null-audio content/voice fixture, so take separately after the first repair.
3. **Navigation exact ties:** public prose claims declaration order is
   irrelevant, but equal geometric scores keep the first supplied candidate.
   Correcting and pinning that existing rule needs no navigation redesign.

These failures are source-derived until separately qualified. JSON numeric
acceptance, deferred feature decisions, rendering and physical hardware
remain outside this shortlist. T2/T6 favor an honest post-failure registry
and complete notification membership; no allocator or transaction framework
is needed.

**Acceptance**

- Retain the qualified AudioTests-local Debug CRT scope and original
  calibration. Exclude MSVC's allocating debug iterator proxies from the
  bank probe: its noexcept string move terminates on proxy refusal, including
  moves into a fresh Registry. Calibrate the optional allocation-size filter
  separately; its default preserves the earlier group probe. Preconstruct
  silent-bank ownership and short/long names before arming; sweep eligible
  allocation points through uninjected success, restoring/disarming before
  checks and cleanup. Reproduce actual
  late-publication failure on the original implementation, not just early
  Registry refusals; do not provoke memory exhaustion.
- Prepare a new first-add ledger name before Registry publication and remove
  it if Registry::add throws. A failed new name remains absent and retryable;
  old bank handles/pointers and ownership remain valid. Preserve successful
  first-add order, replacement/null slots and group-volume behavior.
- State the narrow failed-new-registration contract and consumer correction.
  Add no iteration/allocator API, production test seam or general framework.
  Check excerpt coupling and review all changed source/test bytes.
- Run default Debug and Debug null headless AudioTests, ordinary Release null
  cases, eight standalone checks and website verification. Retain the initial
  fan-out/order cases and add a null regression for skipped empty slots and
  first-add order after refilling a bank, without duplicate pushes. Keep injection
  limited to native MSVC Debug DLL CRT and wasm/Node unrun at its existing
  activation stop; full CI before landing. No endpoint/listening result.

**Closed 2026-10-29.** `add_sound_bank` finishes the vector growth and full
notification-name copy before Registry publication, then removes only that
new ledger entry if Registry insertion throws. Existing bank ownership,
handles/pointers, replacement/null semantics and first-add order are retained;
the public contract and CHANGES describe the failed-new-name guarantee.

The first unfiltered probe terminated in MSVC's allocating Debug string-move
proxy and is not accepted as a reproduction. Installed-STL source review
established the proxy-size exclusion; the original calibration still passes
17 assertions and the separate filter calibration passes 12. With original
production bytes, the revised short/long-name regression completes 209
assertions with nine actual missing-refusal failures: resolve/get still find
the failed name, including after another distinct registration. The observed
eligible failure points are evidence, not hardcoded allocation ordinals.
The ordinary empty/refill control passes 17 assertions and checks skipped
empty slots, retained handles and exactly one update per live bank in the
original first-add order.

After the fix, complete native AudioTests pass with zero skips: Debug null
42 cases/617 assertions, default Debug 33/544 and Release null 38/309. All
eight standalone checks and website verification pass: zero type diagnostics,
160 tests, 145 pages and 435 generated files, with no release snapshots. The
published header includes the failure guarantee. Test registration, Registry
and SoundBank source remain unchanged. Injection qualifies eligible native
MSVC Debug DLL CRT allocations, excluding debug proxies; Release checks
ordinary behavior. Wasm/Node remains unrun at the existing activation stop,
and full CI remains required before landing. No endpoint or listening claim.

<a id="sm-64"></a>
### SM-64. Keep the published step fraction below one after rounding

Selected 2026-10-29 after SM-63 from the screened core exhibit in SM-62's
audit. The remaining JSON float-range candidate introduces a new numeric
acceptance contract and compatibility questions. This item instead repairs
an existing explicit guarantee with a safe arithmetic reproduction and a
small boundary correction, so T2/T4 favor taking it first.

StepPosition promises a fraction in [0,1), but the ratio narrowed to float
can round to one for a period of 2^25 and a remainder one tick smaller.
Beyond double's exact-integer range, converted tick counts can also coincide.
StepTimer accepts any positive uint64 tick period. Ordinary Application
uses an integer target_fps of at least one, so its periods stay at or below
one second, where the existing narrowing is safe. This is a public helper's
large-period range repair, with no default Application pacing claim.

**Acceptance**

- Add safe scalar-arithmetic regressions for 2^25, periods around 2^53 and
  UINT64_MAX with period-1 remainder, preserving the maximum step count.
  Observe their failure with the original header before applying the fix.
  Do not call Tick, sleep, advance a clock, allocate a huge buffer or create
  a device to reproduce the edge.
- Keep the current ratio for results below one; if narrowing reaches one,
  answer the nearest float below one. Preserve ordinary quarter/half shares,
  tiny positive shares, invalid-input zeros and the step count. Test against
  an independent expected boundary value, not the same library operation.
- Correct false period/exact-integer comments and state the rounded-boundary
  rule. Leave StepTimer acceptance, Application source/FPS validation,
  CoreTests registration and all render paths unchanged.
- Record the consumer-visible correction; check source/excerpt coupling,
  review and run full native Debug null, default Debug and Release null
  CoreTests, eight standalone checks and website verification. Wasm/Node
  stays unrun at the existing activation gate; full CI before landing.

**Closed 2026-10-29, existing range guarantee.** With the original header
unchanged, ordinary/invalid-input controls passed (2 cases, 17 assertions)
and the boundary regression failed at all five periods (1 case, 20 assertions,
10 failures). The helper now preserves its ratio below one and uses the
nearest float below one only at the rounded boundary. The expected value
is pinned independently by an exact float literal. The false one-second and
exact-double assumptions are corrected; timer/Application/registration hashes
remain unchanged. Independent source review found no issues.

Full headless CoreTests pass in native Debug null, default Debug and Release
null: 84 cases and 4,989 assertions in each, with no skipped cases. All eight
standalone checks and the website type check, 160 tests and 145-page build
pass. The published StepPosition reference states both the fraction range
and the rounded-boundary result; CHANGES records the correction without a
caller migration. Wasm/Node remains unrun at the existing activation stop;
full CI is required before landing. No clock advancement, sleep, device,
Application pacing or display measurement was used to qualify this edge.

<a id="sm-63"></a>
### SM-63. Reject a negative authored animation count at the loader boundary

Selected 2026-10-29 after SM-62 from its second ranked source exhibit. The
sheet decoder forwards an authored int frame_count to AnimationStrip, whose
vector reserve converts it to size_t. A negative value loses the authored
source/strip/field context in a container-capacity failure. T6 favors a small
decoder check before that conversion; no render abstraction or general
allocation policy is needed.

**Acceptance**

- Inspect the original path without invoking its enormous-capacity request.
  Apply and compile the reviewed decoder guard before running the new
  negative case or full AssetsTests; no original-source runtime reproduction
  or memory-exhaustion claim is required.
- Reject negative authored counts with std::runtime_error naming source,
  animation_strips index, strip name, frame_count and signed value. Place
  the check before AnimationStrip construction. Cover -1 and INT_MIN after
  a preceding valid strip, without naming that earlier strip as the offender.
- Preserve zero as an empty authored strip and positive frame rectangles,
  timing and looping. Do not change the direct render API, positive-count
  limits, geometry arithmetic or timing policy; test without drawing.
- State the public failure/zero contract and consumer correction. Use the
  existing owned scratch-file fixture and default TextureHandle, with no
  device creation. Check excerpt coupling, review and run full native Debug
  null, default Debug and Release null AssetsTests, eight standalone checks
  and website verification. Keep wasm/Node unrun at its existing activation
  gate and full CI required before landing.

**Closed 2026-10-29, authored-input diagnostic.** The decoder now checks the
signed count before constructing AnimationStrip and reports its source,
indexed strip, name, field and value. Source review covered the original
conversion path without executing that oversized request. Each native build
recorded the guarded decoder compilation before the negative cases ran.
The -1/INT_MIN cases and zero/positive controls pass, preserving all tested
rectangles, timing and looping. Direct render implementation/header hashes
remain unchanged. Independent source review found no issues.

Full headless AssetsTests pass in native Debug null, default Debug and
Release null: 73 cases and 557 assertions in each, with no skipped cases.
All eight standalone checks and the website type check, 160 tests and
145-page build pass. No affected CPP/test excerpts require a page edit;
the published loader reference includes negative-count refusal and zero as
an empty strip. CHANGES records the correction; valid content and caller code
need no migration. Wasm/Node remains unrun at the standing Emscripten
activation stop, and full CI is required before landing. No strip is drawn
and no graphics device is created by these new cases.

<a id="sm-62"></a>
### SM-62. Reject malformed JSON bytes after an otherwise valid root

Selected 2026-10-29 after SM-61 from a fresh ranked internal-source audit:

1. **Raw NUL JSON input:** the shared parser's bounded RapidJSON stream uses
   the same sentinel for an input NUL and EOF. A completed root followed by
   a raw NUL can therefore hide a suffix. The existing edge test pins only
   file/text parity. This conflicts with reading the full input as valid JSON,
   benefits all content readers, and has a small shared repair: select first.
2. **Negative authored animation count:** the sheet loader forwards a negative
   integer into a size_t reserve request, losing source/strip/field context.
   A bounded decoder check can preserve current zero/positive behavior.
3. **JSON float range:** number() narrows finite doubles without a float-range
   check. This needs an explicit selected range contract and compatibility
   review; it is smaller in code than in contract impact.

A large-period StepPosition float-rounding edge was also screened. The
standalone helper can round to one despite its stated fraction contract, but
ordinary Application's integer target_fps keeps periods at or below one
second, where this conversion is safe; do not call it a default pacing fault.
These are source-derived exhibits until individually reproduced or qualified,
with no competitor or deferred-feature claim. T2/T6 favor rejecting damaged
authored input at its source rather than quietly ignoring bytes.

**Acceptance**

- First add an explicit raw-NUL regression through both parse_json and
  read_json_file and observe the valid-root suffix/terminal-NUL acceptance
  against unchanged production source. Distinguish those cases from quoted
  NUL/whitespace inputs that already fail but lack the new diagnostic.
- Reject literal zero bytes anywhere in the shared parser's supplied span,
  naming caller, source and byte offset. Keep the check bounded by the view,
  including a nonterminated subspan with NUL/junk outside it.
- Preserve valid escaped U+0000 strings and member names, trailing JSON
  whitespace and existing BOM behavior. Do not edit the vendor parser or
  bundle numeric-range, animation-count or Unicode-policy changes.
- Update the public contract and consumer change record. Use existing owned
  scratch files for generated test content, no live user data or new fixture
  framework. Run headless native Debug null, default Debug and Release null
  AssetsTests, all eight standalone checks and website verification. Record
  wasm/Node unrun at the existing SDK activation stop; full CI before landing.

**Closed 2026-10-29, shared input validation.** With production source still
unchanged, four valid-input controls passed (4 cases, 15 assertions), while
the explicit refusal case failed (1 case, 48 assertions, 12 failures).
Completed-root NUL variants lacked both text and file refusals; already
invalid quoted/whitespace variants lacked the raw-NUL description. Fixture
labels were corrected to owned strings so logs name each variant; inputs and
assertions stayed unchanged. A bounded raw-byte check in the shared parser
now refuses the first zero at its byte offset. No vendor code or other
numeric/loader policy changed.

Full headless AssetsTests pass in native Debug null, default Debug and
Release null: 70 cases and 522 assertions in each, with no skipped cases.
All eight standalone checks and the website type check, 160 tests and
145-page build pass. Independent source review found no remaining issues;
the published JSON reference carries the raw-NUL and bounded-view contract,
and CHANGES explains the malformed-input correction. Valid JSON and template
code need no migration. Wasm/Node remains unrun at the existing Emscripten
activation access stop, and full CI remains required before landing. The
tests use their own temporary files and establish no device/browser result.

<a id="sm-61"></a>
### SM-61. Hold the include boundary across equivalent path spellings

Selected 2026-10-29 after SM-60 from the third item in SM-59's ranked source
shortlist. The engine checker matches only a narrow literal path shape.
Dot segments and headers nested inside a backend can evade it; a real sibling
spelling such as ../d3d12/backend.h is missed from a d3d11 source. Conversely,
a nested source inside its own backend would be refused because ownership
is compared with the immediate directory rather than the subtree. T4/T5
favor making the existing promised boundary consistent, without changing
which modules depend on which or emulating a compiler.

**Acceptance**

- Add compiler-free fixtures and first demonstrate the current gaps and false
  refusal against the unchanged checker. Include existing passing controls
  for vendor paths and generated same-backend headers.
- Normalize literal, single-line forward-slash operands. Preserve the named
  game-tree rule, including its existing leading ../ forms. For backend .h
  paths, resolve repository-root engine spellings and explicit dot-relative
  forms; compare source ownership by module/backend path segments and allow
  any nested source/header within that same subtree.
- Give normalized engine-root spellings precedence over source-relative
  fallback. For that fallback, require the backend owner directory to exist,
  not the included header: this avoids inventing an owner from a dotted
  vendor path while retaining generated headers in actual backend folders.
- Cover quote/angle forms, whitespace, .h/.cpp sources, game and backend dot
  paths, nested headers, sibling-relative paths, all four platform-bearing
  modules and a synthetic new module. Keep unrelated names, comments,
  vendor includes and generated same-owner headers allowed. Do not invent
  a backend by concatenating arbitrary vendor tokens with the source path.
- Keep original source/include diagnostics. Document literal-lint limits:
  no macro/continuation/block-comment preprocessing, symlink resolution,
  arbitrary compiler include roots or guessing other client folder names.
  Preserve the repository-root include convention.
- Add the fixture command to the existing CI lint step and standalone brief;
  do not change job/matrix identities or branch protection. Run the complete
  fixtures, actual repository check, standalone lints, a targeted build that
  depends on the checker and website verification. No compiler is needed by
  the fixtures and no device/browser/network acceptance is claimed.

**Closed 2026-10-29, source-boundary enforcement.** With the original checker
unchanged, focused fixtures reproduced three missed forbidden includes
(dot-game, nested backend and sibling-relative forms) and the false refusal
of a nested source in its own backend. Vendor, generated-header and legacy
game-parent controls passed. The reviewed fix normalizes operands, compares
whole ownership subtrees, gives engine-root spellings precedence and limits
source-relative backend detection to actual owner directories. Included
headers may remain generated and absent.

All 49 compiler-free fixtures and the actual repository scan pass, including
dotted shared/vendor paths, missing generated headers and synthetic new
modules. All eight standalone checks and a native Debug CoreTests build
through its checker dependency pass; the latter ran the three build checks
without recompiling or rerunning unchanged C++ tests. Website type checking,
160 tests and the 145-page build pass, with the scope/limits present in the
published architecture page. Independent source review found no remaining
issues. The existing lint step now runs the fixtures; all other workflow text,
job identities and matrices are unchanged. Full CI remains required before
landing. These are literal-source checks, with no device, browser or network
qualification.

<a id="sm-60"></a>
### SM-60. Exercise the shared byte reader's public contract directly

Selected 2026-10-29 after SM-59 from the second item in its ranked source
shortlist. DDS, sprite-font and XWB readers use ByteReader, but CoreTests has
no direct cases for its cursor and refusal contract. Its public comment also
still describes two formats and two render-only callers. T2/T6 favor small
in-memory boundary cases that protect all three readers; correcting the
stale counts needs no API or parser abstraction.

**Acceptance**

- Add the test source explicitly to CoreTests. Exercise known little-endian
  unsigned, signed and float bit patterns, mixed reads/skips and non-consuming
  require calls through public results, position and remaining counts.
- Cover each truncated read/skip/require, unchanged cursor after boundary
  refusal and a subsequent valid read; zero-byte operations on empty buffers
  and at EOF; maximum size_t requests after advancing the cursor, without a
  large allocation. Protect the subtraction-based guard's behavior without
  asserting implementation spelling.
- Prove read_bytes returns owned storage; keep the borrowed source alive and
  unchanged throughout reader use. Check source and useful boundary detail
  in an error without pinning the entire sentence. Make no allocation-failure
  guarantee for read_bytes, which advances before constructing its result.
- Replace stale caller counts with the byte cursor's role. Keep production
  declarations and implementation unchanged unless a test demonstrates an
  actual defect, which needs its own reviewed scope and consumer record.
- Review and run native Debug null, default Debug and Release null CoreTests,
  standalone checks and website verification. Record the stopped wasm/Node
  axis explicitly; full CI remains required before landing. These cases need
  no file mutation, browser or graphics/audio device.

**Closed 2026-10-29, direct contract coverage.** Eleven new cases include
six independent truncated-operation subcases, stored integer/float bit
patterns, mixed cursor movement, zero-byte empty/EOF boundaries, oversized
requests, result ownership and diagnostic context. Independent source review
found no issues. CoreTests passes in native Debug null, default Debug and
Release null: 82 cases and 4,955 assertions in each, with no skipped cases.
All seven standalone checks and the website type check, 160 tests and
145-page build pass. The published reference includes the corrected role
comment and new test include site. Header declarations and production
implementation are unchanged. Wasm/Node remains unrun at the existing
Emscripten activation access stop; full CI is required before landing.

<a id="sm-59"></a>
### SM-59. Preserve sound-group names and volume slots on allocation failure

Selected 2026-10-29 after ready source slices were completed and the remaining
acceptance depended on runtime, network, owner or private-client gates. The
ranked internal-source shortlist was:

1. **Sound-group insertion rollback:** add_sound_group inserts a name before
   growing its volume vector. A failed growth leaves a reserved name whose
   index can alias the next successful group. This is a concrete caller-state
   defect, with the settings guide as a client and Registry's rollback as an
   existing pattern. Select it first, subject to qualifying fault injection.
2. **Direct ByteReader contract cases:** three format readers share it, but
   no direct suite pins refusal/cursor/EOF/huge-count behavior; its two-render-
   callers comment is stale. Low risk and the preferred fallback if the first
   item's test instrumentation cannot be qualified.
3. **Normalized engine-include fixtures:** strengthen the existing literal
   include wall against equivalent path spellings and nested backend paths,
   with compiler-free refusal fixtures. Valuable, but broader tooling-policy
   review than the directly observed table invariant.

No competitor claim or deferred feature decision is used. T4/T6 favor a name
table whose failed mutation leaves an honest, reusable state; T1 needs no
production allocator option or transaction framework for this repair.

**Acceptance**

- First qualify a small AudioTests-local native Debug CRT failure scope:
  current thread only, one selected ordinary allocation refused, no assertions
  or allocation inside the hook, previous-hook restoration/conflict checks
  and a control proving refusal. Do not provoke real memory exhaustion.
- Demonstrate the current bug through public AudioResources state, then keep
  the inserted map iterator and erase that new entry if volume insertion
  throws. Preserve existing groups/volumes, successful indexes, defaults,
  clamping and bank behavior. No production test seam or new abstraction.
- After injected failure, the attempted name is absent, an old group's
  handle/volume is intact, another group is distinct, and retry succeeds
  independently. Sweep observed allocation points rather than assuming one
  library's allocation ordinal is portable. Disarm before checks/cleanup.
- State the failure contract and consumer-visible correction with no caller
  migration. Run default/null native headless AudioTests and Release ordinary
  cases, standalone checks and website verification. Record the stopped wasm
  axis and Debug-only injection scope explicitly; claim no device/listening
  qualification. If injection cannot be qualified, retain that precise gate
  and take the second shortlisted item independently.

**Closed 2026-10-29, bounded source repair.** The AudioTests-local failure
scope first passed its one-shot/restoration control (1 case, 17 assertions).
With the production implementation unchanged, the public-state regression
then failed at an observed vector-growth allocation: the attempted name
remained resolvable and its retry was rejected. Keeping the inserted map
iterator and erasing it on failed volume insertion makes both new cases pass
(2 cases, 87 assertions), without changing the successful path or adding a
production allocation seam. Independent source review found no remaining
issues. The public header states the failure guarantee and CHANGES records
the correction with no migration.

Full headless AudioTests pass in native Debug null (39 cases, 379 assertions),
default Debug XAudio2 (31 cases, 323 assertions) and Release null (37 cases,
292 assertions), with no skipped cases. All seven standalone checks and the
website type check, 160 tests and 145-page build pass. Failure injection is
qualified only for the native MSVC Debug DLL CRT; Release runs ordinary
cases. Wasm/Node was not run because the existing Emscripten activation access
stop remains in force. Full CI remains required before landing; these results
make no audio-device, playback or listening claim.

<a id="srl-22"></a>
### SRL-22. Complete the source audit's unit and retained-loan contracts

Filed 2026-10-29 from [SRL-17](../api-teachability.md#failed-criteria-and-disposition).
T4/T10 require a caller to learn units and lifetimes from the public contract;
T1 does not call for a new wrapper or ownership framework to supply missing
words. Scope is the three confirmed documentation facets, with unchanged
implementation and declarations.

**Acceptance**

- Verify shared sprite geometry and backend calls, then state source-rectangle
  units, rotation units/direction and origin/pivot/scaling rules beside
  DrawList::draw_sprite. Keep its established world-space, tint and ordering
  rules consistent; do not infer semantics from a similarly named text API.
- State the retained RenderResources loan in DrawObject and the public
  SpriteSheetObject/TextureObject/TextObject constructor path, so Visual and
  UiTexture inherit an explicit contract. Preserve valid placeholders and the
  Text/UiText rule already present. State ViewportManager's retained
  ResolutionManager loan and its required lifetime.
- Correct AudioResources getter/add wording to describe owned banks,
  same-name replacement and null-add invalidation of raw pointers/references,
  distinct from stable slot handles. Remove the contradicted
  no-release-until-destructor claim; do not change playback or add restrictions
  unsupported by the implementation.
- Independently compare comments with implementations and relevant existing
  fixtures. Prove non-comment C++ tokens unchanged, run affected headless
  checks, all standalone checks and the website check/tests/build. Record
  stopped device/wasm checks explicitly rather than claiming them.

**Closed 2026-10-29.** Seven header contracts now state the sprite's source
units, clockwise radians and mapped/truncated pivot; retained drawable/viewport
service loans; and bank replacement/null-add invalidation distinct from stable
slot handles. The source-ratio formula is explicitly qualified to nonzero
source dimensions, preserving the existing zero-axis guard. Default/null
appearance-only placeholders remain valid. Independent source review found no
remaining issue after that qualification; root also checked all five backend
forwarders and shared camera/quad arithmetic.

All nonblank, non-full-line-comment bytes compare identically after line-ending
normalisation across the seven headers; declarations and implementations are
unchanged. RenderTests and AudioTests build and pass on all five native Debug
backends and Release-null (12 headless CTest entries). Existing tests cover
geometry, defaults and bank replacement; null-add wording is source-verified,
not claimed as a dedicated passing audio case. All seven standalone checks
pass. No graphics/audio API device, pixel run, browser, wasm activation or
listening acceptance was performed; their previous stops remain.
The website's zero-diagnostic check, 160 tests and 144-page/433-file build
pass; 17 contract-text assertions across all seven generated reference pages
confirm that the corrected rules are published.

<a id="srl-19"></a>
### SRL-19. Make LineSweeper's layer boundary a build check

Added 2026-10-28. The sample's [layer rules](../../samples/linesweeper/README.md#three-layers-and-the-middle-one-may-not-see-the-rules-verbs)
and [PlayState](../../samples/linesweeper/states/play_state.h) define the
boundary. This changes enforcement of that existing rule and needs neither
hardware nor a design decision.

**Acceptance**

- A standalone CMake check rejects includes of `engine/` from `rules/` and
  `rules/tick.h` from `presentation/`, in headers and sources, with the file
  and offending include in its error. Define the accepted include spellings
  and cover quoted, angle-bracketed and relative attempts as appropriate to
  the existing include wall; do not imply a complete C++ dependency analyser.
- The current sample passes. Synthetic fixtures demonstrate both prohibited
  directions fail and allowed includes pass, with no graphics or compiler.
- Wire it into builds that actually include LineSweeper and the existing lint
  path, without adding a required CI job or making a minimal consumer build
  depend on sample sources. Check the copied-consumer shape.
- Update the sample README and PlayState's comment to state the enforced
  boundary and its limits. Run the standalone checks and offline tooling tests.

**Closed 2026-10-28.** The standalone checker rejects literal engine includes
from rules and tick includes from presentation, normalising root and relative
paths. Fourteen compiler-free fixtures prove prohibited and allowed directions
and error details. The rules library owns the build dependency, so targeted
sample/tests run it; the existing lint job runs it and its fixtures without
adding a job or test entry. The README names lexical limits.

All standalone checks, Debug-null rules/view builds and both LineSweeper test
entries pass. The copied minimal consumer has no LineSweeper/check target,
builds with preinstalled dependencies and null render/audio, registers exactly
its two tests and passes both. That is consumer-shape and headless evidence,
not the separate Release/device/Sandbox qualification. The website check,
160 tests and 143-page build pass, as do 90 offline Node cases. All 71 permitted
Python tooling cases pass across the main run and the seven installed-path
follow-ups; ten previously stopped PowerShell process cases remain unexecuted.
Local logs and fixture receipts are under `out/loop-2026-10-28-2114/`.

<a id="srl-20"></a>
### SRL-20. Copy a working sprite-strip animation example

Filed 2026-10-28 after the [sprites guide](../../website/src/content/docs/docs/guides/sprites-and-text.mdx)
pointed callers at the private `AnimationStrip::calculate_frame` and no sample
demonstrated the strip path. The public accessor is `frame_rect`; the existing
components gallery is the client, with elapsed time, looping and restart
policy kept in sample code (T1).

**Acceptance**

- A copyable gallery example resolves a named animation strip and draws the
  public `frame_rect` through SpriteSheet. Include redistributable content and
  the loader data needed to reproduce it; add no engine animation manager.
- Headless tests cover frame boundaries, looping, one-shot completion, restart
  and a step crossing several frames. Define the example's treatment of
  invalid time inputs and prove drawing does not advance the animation.
- Replace the private-method instruction with the public accessor and quote
  the compiling example from the guide. Update the components README and any
  other page whose claim that no sample uses a sprite sheet becomes false.
- Build and exercise the gallery natively and in the browser, run its headless
  and page checks plus the website checks, and review its rendered frames.
  Review is separate from the headless timing tests. No renderer change is
  needed; any later expansion of that scope follows the five-backend rule.

**Source/native progress, 2026-10-29:** the copyable AnimationPreview retains a
paired SheetStrip and game-owned bounded time. Negative/nonfinite steps fail
before mutation; zero is accepted; looping, full-duration one-shot completion,
large steps and restart have deterministic tests. Const drawing calls the
public `frame_rect` and SpriteSheet source-rectangle overload. The gallery
owns loop/once values, draws through the existing Scene overlay with point
filtering, and restarts on R/gamepad X without changing the settings rows.
Original MIT four-frame DDS/JSON content has a standard-library generator and
licence/provenance; its output reproduces exactly. Source excerpts replace the
private-method instruction and the no-sample-sheet claim.

Default Debug builds and all 38 ComponentsTests/369 assertions pass.
Debug-null builds and all 42 cases/447 assertions pass, covering pure timing,
delivered-file readers, repeated/two-view recordings, the actual manifest's
selected sprite-sheet entry and a deliberate texture-table reload. A hidden
native-null render/audio fixture additionally exercises the real GalleryState
overlay and R press/hold snapshots with no Application loop or API device.
The two real-file path cases also pass from an unrelated working directory.
These are separate from physical recovery, concurrent execution and rendered
appearance. A separate source review has no remaining findings.
Release-null gallery/tests builds and the same 42 cases/447 assertions also
pass, including finite/NaN boundary behavior under the release settings.
All seven standalone checks, website check, 160 website tests and the 143-page
build pass. The built guide/example excerpts and removal of the stale private
method/no-sample instructions were checked in the generated HTML.

**Closed 2026-10-29 at 1b5ecee.** The x64-release gallery and the Debug
D3D11, D3D12, GL and Vulkan galleries draw both previews point-filtered inside
the panel. Release frames about 130 ms apart show the loop stepping through
radii 2, 4, 6 and 4 while the one-shot holds radius 4. R restarts both previews
and reports it on the status line. In headless Chrome 156 (WebGL2, RTX 5080),
the wasm-release page was driven with real key events and its frames reviewed:
after R both previews show frame 0 together, then the loop wraps while the
one-shot holds its last frame. ComponentsPageTests passes in wasm-release,
wasm-debug and wasm-release-null. ComponentsTests passes 38 cases/369
assertions in each wasm configuration and x64-release, and 42/447 in
x64-debug-null. The website check, 160 tests and the 145-page build pass.
Native fullscreen-exit painting belongs to OCA-51, not this story.

<a id="srl-21"></a>
### SRL-21. Enclose every banner draw in its culling extent

Added 2026-10-28 from [SM-48's source audit](../code-hygiene-sample-comments.md#separate-behaviour-gap-banner-culling-extent).
The background spans x = 500 to 780; the keyboard and touch messages extend
beyond it. Scene culls every object, so a partial view through only protruding
text can omit the banner. This is a bounded correction to the existing
GameObject contract and runs with the maintenance lane; it selects no new
presentation policy. Normal full-board or hardware failure is not claimed.

**Acceptance**

- Cache a conservative text envelope during update, including glyph bearings,
  and union it with the existing panel for bounds. Include the panel's existing
  view-pixel quantization allowance in scale-aware cull bounds. Keep strings,
  layout, tint, draw order and the panel rectangle unchanged.
- Native-null behavioural cases exercise partial views on both text overhangs,
  keyboard/touch cache changes and live/top-out transitions. Bounds enclose
  recorded geometry and normal full-board recordings preserve geometry/order.
- Regenerate `LineSweeperCapture` on `x64-release` and review every changed
  image as AGENTS requires. Keep that qualification explicit if GPU startup
  remains denied; headless recordings do not replace it.

**Source implemented 2026-10-28:** update caches the full text layout/glyph-cell
union, including bearings, and bounds combines it with the existing panel.
Scale-aware culling adds only the panel's existing quantization allowance.
The draw method, text, positions, panel rectangle and ordering are unchanged.
Five new native-null cases pass in Debug-null LineSweeperViewTests: overhang
views at several zooms, recorded-corner containment, negative glyph bearings,
cache refresh and full-view geometry/order. Standalone checks pass.
**Closed 2026-10-29:** `LineSweeperCapture` on `x64-release` (D3D11, NVIDIA
GeForce RTX 5080, hardware) regenerated the four website captures from
`website.txt`. Each matches the committed PNG byte for byte, so no image changed.
The top-out frame was looked at: the banner text draws in full beyond its panel.

<details><summary>Why each is placed here</summary>

| Story | Why |
|---|---|
| OCA-26 | G142, G143. Closed 2026-10-28: [Build for the browser](https://labradorengine.com/docs/guides/browser/#the-page-and-what-it-owes-the-module) is the public canvas/four-callback contract, including exact names, startup versus runtime errors, frame-boundary context notifications and quit before DOM removal. `window.h`, the minimal shell and web.md section 7.3 point there. Semantics checked against the window, sample entry point and renderer; the running-canvas removal consequence is established by code inspection, not a new device observation. |
| OCA-24 | G112. Closed 2026-10-28: Troubleshooting publishes all five browser cases from local reproductions. The dated story's 17/27 test-count comparison is superseded by 19 without Chromium, 32 Debug and 33 Release with it; copied adopter projects have their own rules test. [Verification](../port/2026-10-28-browser-troubleshooting.md) records the actual messages, configure-only count checks and runtime failures. |
| OCA-25 | G132. Closed 2026-10-28: the audio guide, sample README and SoundBank method contracts state dropped one-shots, waiting retained effects and the Escape exception. [Verification](../port/2026-10-28-browser-troubleshooting.md#browser-audio-before-activation) observes the running sample's source starts before and after activation: no stale one-shot replay, loop starts, fresh ping works, Escape does not unlock. This is scheduling evidence; physical listening remains WEB#3. |
| OCA-63 | G16. Closed 2026-10-28: [Build for the browser](https://labradorengine.com/docs/guides/browser/#gather-the-browser-release-and-its-notices) publishes the existing bundler's release command and full notice inputs. A fresh copied current project built in Release, passed its rules test, packaged exactly five files and drew/quit in Chrome. All licence texts, byte counts, hashes and Brotli sizes were verified. The literal command also passed with v0.2.0's packaging inputs and current runtime fixtures; this does not claim a legacy game build. LineSweeper now carries the engine's attribution record and preserved Microsoft timer-code MIT text, and its rebuilt BundlePageTests passed. |
| OCA-31 | G11. Closed 2026-10-28: the public browser guide documents the repository runner against a consumer's built page, the quit protocol and pass/fail diagnostics. A copied minimal page outside the checkout passed in headless Chrome; injecting the documented `console.error` failed with exit 1. |
| OCA-74 | G142, G51. Closed 2026-10-28: the public LineSweeper bundle guide states the default factory, canvas, callbacks, quit/replay lifetime, files, BUILD fields and hosting rules. Its literal host example outside the checkout drew, quit through `onQuit` and replayed on a fresh canvas in headless Chrome without complaints. The bundle example and web.md section 7 point to it. |
| SM-27 | G144. Closed 2026-10-28: `bundle.mjs --verify` checks a copied LineSweeper bundle read-only, refusing dirty/unknown provenance, non-Release builds, missing or unrecorded files and mismatched hashes/sizes. web.md section 7.1 states every rule. Offline fixtures pass, and the existing Debug bundle is rejected for its build type. |
| BDP-15 | G82. Source and offline cases implemented 2026-10-28: separate download, startup and runtime advice; raw diagnostics remain in the console. Closed 2026-10-29: `LineSweeperBundlePageTests` passed in `wasm-release` and `wasm-debug` (headless Chrome 156, ANGLE D3D11, RTX 5080) [with these cases](../port/2026-10-28-demo-host.md#runtime-acceptance-2026-10-29). Each class has its own in-browser case: a 404 loader download, truncated wasm at startup, `onError` at runtime, and the two unsupported-browser probes. |
| BDP-16 | G83. Bounded warning and safe document-reload fallback implemented 2026-10-28, with offline recovery-race cases. After ten seconds the page offers Try again, which calls the instance's sample-owned `_linesweeper_quit()` bridge, hears `onQuit` and mounts a fresh canvas and instance. Without a working bridge the offer is Reload page. Closed 2026-10-29: `LineSweeperBundlePageTests` passed in `wasm-release` and `wasm-debug` (headless Chrome 156, ANGLE D3D11, RTX 5080) [with these cases](../port/2026-10-28-demo-host.md#runtime-acceptance-2026-10-29). The page case loses a real context, never restores it, and asserts the remount draws frames and quits. A real driver reset is not claimed. [Evidence and boundary](../port/2026-10-28-demo-host.md#bounded-graphics-warning-bdp-16). |
| BDP-17 | G84. Source and offline cases implemented 2026-10-28: click-only legacy wasm-exception and scratch-WebGL2 probes stop unsupported browsers before import; scratch contexts are released. Closed 2026-10-29: `LineSweeperBundlePageTests` passed in `wasm-release` and `wasm-debug` (headless Chrome 156, ANGLE D3D11, RTX 5080) [with these cases](../port/2026-10-28-demo-host.md#runtime-acceptance-2026-10-29). No real legacy browser was used. |
| BDP-11 | G35. Source and five native-null state cases implemented 2026-10-28; Debug-null LineSweeperViewTests passes. QUIT is the fourth row: Down three times, then Enter. Closed 2026-10-29: `LineSweeperBundlePageTests` passed in `wasm-release` and `wasm-debug` (headless Chrome 156, ANGLE D3D11, RTX 5080) [with these cases](../port/2026-10-28-demo-host.md#runtime-acceptance-2026-10-29). The blur and synthetic hide/return quits need no Escape. A real tab switch remains `BDP-06`'s. |
| BDP-12 | G80. The host names every keyboard/controller action; bindings and in-game hint strings are unchanged. Offline policy checks pass; the desktop bundle-page assertion passes. Closed 2026-10-29: `LineSweeperBundlePageTests` passed in `wasm-release` and `wasm-debug` (headless Chrome 156, ANGLE D3D11, RTX 5080) [with these cases](../port/2026-10-28-demo-host.md#runtime-acceptance-2026-10-29). No hint string changed, so no capture moved. |
| BDP-13 | G80. The coarse-pointer host shows device instructions before the gate opens. Offline policy checks pass; the touch and desktop page cases pass. Closed 2026-10-29: `LineSweeperBundlePageTests` passed in `wasm-release` and `wasm-debug` (headless Chrome 156, ANGLE D3D11, RTX 5080) [with these cases](../port/2026-10-28-demo-host.md#runtime-acceptance-2026-10-29). Real-phone layout remains `SM-13`'s. |
| BDP-14 | G81. A pressed controller button exposes Play without fetching the module; connection alone keeps the gate. Offline policy checks pass; the fake-pad bundle-page case passes. Closed 2026-10-29: `LineSweeperBundlePageTests` passed in `wasm-release` and `wasm-debug` (headless Chrome 156, ANGLE D3D11, RTX 5080) [with these cases](../port/2026-10-28-demo-host.md#runtime-acceptance-2026-10-29). Physical-pad qualification remains `OCA-48`'s. |
| BDP-21 | G117: a landing shadow every player can see. OCA-08's approved position removes its decision prerequisite. Closed 2026-10-27: each shadow cell is a two-pixel outline in the piece's full-strength colour around a 28% inside; edges measure 3.62:1 (J) to 13.54:1 (I) against the well, and `palette_tests.cpp` in LineSweeperViewTests fails any kind below 3:1. The three moved stills were reviewed and the footage regenerated; the sample README's *The landing shadow is an outline* records the reason and limits. Low-vision player validation is not claimed. |
| BDP-22 | G118: fewer sparks under reduced motion. Follows BDP-23 (Now). The default is already calm: N3b passes the top-out on the strict, pixel-by-pixel reading of WCAG 2.3.1 for every player, so reduced motion goes further for those who ask rather than being what makes the demo safe. Closed 2026-10-27: `main_web.cpp` reads `prefers-reduced-motion` once at start, logs `LineSweeper: motion reduced` or `full`, and makes the field with `Motion::reduced`, which throws `sparks_per_cell`'s count, an eighth of each burst's sparks per cell rounded up, at unchanged heat. LineSweeperViewTests holds the reduced count for a landing, a row and a whole-well top-out; `LineSweeperPageTests` asserts the full line under emulated `no-preference`, because a CI runner's Chrome reported `reduce` without emulation, and `LineSweeperReducedMotionPageTests` the reduced one under `Emulation.setEmulatedMedia`. The default path is unchanged, so no capture moves; LineSweeperCapture was not re-run. Nothing in `engine/` changed. |
| <a id="bdp-25"></a>BDP-25 | Added 2026-10-27 from the owner's palette decision under OCA-08. LineSweeper owns three selectable palette groups, named in full: Protanomaly / Protanopia, Deuteranomaly / Deuteranopia and Tritanomaly / Tritanopia, alongside its existing palette. This is sample-owned presentation work. Acceptance: selection consistently changes all seven piece presentations without changing rules; each palette is checked with the landing-shadow contrast and flash criteria, and every changed capture is reviewed as the sample README requires. Record the chosen colours, review method and limits; simulation is a development aid, while player validation remains separately identified ([XAG 103](https://learn.microsoft.com/en-us/gaming/accessibility/xbox-accessibility-guidelines/103)). No particular colour values, persistence, automatic detection or engine palette API is selected by this decision. Closed 2026-10-28: the pause menu's COLOURS row steps through STANDARD and the three groups, and PlayState lends the selection to the board and particle field; it is not in `World` or a recording. `board_view_tests.cpp` (null render) asserts all seven pieces, shadow edges and fills change while positions and everything else stay; LineSweeperCapture printed identical tick, score and lines in every palette. `LineSweeperPalettePageTests` steps the palette from the menu with Left, Right and Enter in Chromium and reads the logged choice. `palette_tests.cpp` holds every edge at 3:1 against the well and the grid line (lowest 3.26, the standard palette's J; among the three new palettes, 3.30, deutan S) and no saturated red. Six flash runs pass with general pixel bound at most 1.5 and red 0. Only the pause still changed, and every palette still was reviewed. The sample README's *Colour-vision palettes* records colours, method and limits. Player validation is not claimed and has no story; nothing is saved or detected. |
| BDP-20 | G86. Closed 2026-10-27: the owner accepted the existing 4,305,947-byte (4.3 MB) video. The [live-check ledger](../port/web-to-be-tested.md) already records controls and preload=none with no video fetched before interaction in emulation. Keep that on-demand behaviour and the existing encode; no replacement or deployment is required by this decision. |
| BDP-18 | G114: repeated plays; measure before fixing. Closed 2026-10-29. The twenty-round measurement in [web-to-be-tested.md](../port/web-to-be-tested.md#repeated-play-bdp-18-measured-2026-10-29) showed one live context and one 17.1 MB module heap kept per finished game, and Chrome's context warning at round 17. The cause was engine-side: a browser `Application` is never destroyed, so its Web Audio listeners kept every instance reachable. The engine now releases a quit game's states and services ([APPLICATION.md §7](../../engine/app/APPLICATION.md)), and the page loses the previous context on Play again. After the change, every round keeps one context and one heap. `LineSweeperBundlePageTests` asserts five rounds. The story placed cleanup in the page (T1). The engine part is needed because only the engine can remove its own listeners. |
| BDP-09 | G34: the host page fits a phone held upright; needed before BDP-10. Closed 2026-10-29. `index.html` has `viewport-fit=cover`, safe-area padding, a 16:9 stage limited by the viewport height in landscape, a portrait stage without the 16:9, and a full-screen button only where the element Fullscreen API exists. `LineSweeperBundlePageTests` asserts a canvas wholly on screen and taller than wide at 390x844 (touch emulated, gate lifted by a key), and a 16:9 canvas within 1 px wholly on screen at 1280x720. Screenshots at both sizes and at 844x390 were looked at. Emulation only: real-phone layout stays `SM-13`'s. |
| SM-04 | G63: a local run procedure for the frame bench. Promoted to Should because SM-07 depends on it. |
| SM-05 | G62: the lock verdict on a non-NVIDIA run. Promoted to Should because SM-07 depends on it (critique). |
| SM-02 | G25: what the bench's fan-out measures; PHILOSOPHY amendment E at :691-693, after SM-03's amendment D (Next). Promoted to Should (critique). |
| SM-07 | G24 (major): the low-tier measurements the performance identity rests on. Follows SM-03's per-claim mapping and SM-01's adapter selection; owner-run on the iGPU. CPU update, recording/submission and cadence p99s complete only their part. The pipeline-latency claims and the low-tier acceptance box remain open until separate evidence with stated endpoints exists. |
| SM-40 | G125. Closed 2026-10-27: the translation is `engine/input/xinput/xinput_gamepad.h`'s `gamepad_state_from_xinput`, and InputTests under the XInput backend fills DirectXTK's state by hand to assert each of the fourteen button bits alone and together, both sticks' y negation, the analogue triggers, a disconnected pad's neutral state and `DEAD_ZONE_NONE`. |
| SM-08 | G27: a method for browser pacing (R5) and the phone top-out frame (R6). Promoted to Should because SM-13 depends on it. Closed 2026-10-29. [The method](../port/pacing-method.md) names the console rAF procedure, the `?pacing=` engine-frame export, observed judder and both results templates. In headless Chrome 156 on `wasm-release`, `LineSweeperPacingPageTests` logged per-frame intervals and the marked top-out frame (9,600 live, 0 dropped, CPU 1.6 and 1.5 ms) for both fixture modes, and `LineSweeperPageTests` passed. No display or phone result: those are BDP-07's and SM-13's. |
| SM-14 | G71: serve a development page to a phone. Out of deferred: the owner has an iPhone; written and tried in SM-13's session. |
| SM-13 | G34, G05 (merged with BDP-08): a real iPhone run. Split from the Android half (SM-44, deferred) per the owner facts. |
| BDP-10 | Moved from Deferred to Later on 2026-10-27: the owner has no Android access and accepts iPhone-only evidence for lifting the phone gate. The gate stays closed until SM-13 and BDP-09 pass, then its implementation and public support wording are updated together. SM-44 is optional future evidence; this decision verifies no device and makes no Android support claim. |
| BDP-07 | G23: pacing on the displays visitors use; may need a borrowed high-refresh display. |
| OCA-46 | G123. Closed 2026-10-29 at 1b5ecee: a two-player JoinState uses source-owned assignments, separate FocusGroup slots and DirectionRepeat clocks, readiness and observed-disconnect handling. The Menus and Local multiplayer guides link the copyable code and state the slot-identity limit. The x64-release D3D11 sample and the wasm-release page in headless Chrome 156 were driven with real key events through joining, separate cursors, leaving, rejoining, READY and the arena, and every frame was looked at. LocalMultiplayerPageTests passes in wasm-release, wasm-debug and wasm-release-null. LocalMultiplayerTests passes 20 cases/84 assertions in wasm-release, wasm-debug and x64-release, 21/831 in wasm-release-null and 25/21258 in x64-debug-null. No physical pad or Gamepad API source has driven the menu. |
| OCA-51 | G128: a settings screen wired to what the engine can set. Follows OCA-53 (Next). Implementation note 2026-10-28: the dated story predates sound groups; use the existing SoundBank group controls where appropriate rather than adding another volume bus. Keep fullscreen re-sync and callback behavior in the sample. |
| OCA-50 | G127: a menu that works with a click or a tap, in game code. An engine hit test over `bounds()` was weighed on 2026-10-24 and declined: the one caller, LineSweeper's pause menu (`tapped_row`), sizes each tap target from the row's centre, the row spacing and a minimum width rather than from its bounds alone, because a finger needs more than the text's extent. The target is policy, so the hit test stays the game's (T1; PHILOSOPHY, UI). |
| OCA-76 | G108: the engine half of OCA-53, split out on 2026-10-24. ColourWars is the recorded caller (`game/directory_consts.cpp` reads `%LOCALAPPDATA%` and falls back beside the executable), Windows-only; it waits for a second caller, or a browser one, before it is built. |
| OCA-41 | G145. Source implemented 2026-10-29 after SM-67: shared MSVC UTF-8 source/execution encoding, copied-template note and guide guidance. The permitted scratch consumer reproduces nine wrong wide units before the flag and six correct units afterward; narrow UTF-8 bytes and inherited-option propagation pass. Six bounded native builds, two headless Core suites, lints and website checks pass. The same scratch source built with emcmake in Debug and Release prints six correct wide units and nine UTF-8 bytes under Node 24.19.0 and exits 0. CI run 40836037123 on b96bad6 passes all 12 jobs. [Scope and qualification](../2026-10-29-literal-encoding.md). No owner-reserved non-ASCII C++ test source is committed. |
| OCA-43 | G147. Closed 2026-10-27: the *Sprites and text* guide's *Which languages the text path can draw* and `engine/render/font.h`'s header state that the walk has no kerning, shaping, bidirectional ordering or placed combining marks, list the scripts in and out of reach, and say why a character beyond the basic plane draws as two stand-ins on Windows and one in the browser. No script beyond ASCII has been drawn. |
| OCA-42 | G146. Closed 2026-10-29: the text guide quotes a compiled JSON-to-owned-wide-Text helper, explains glyph checks, paths and packaging, and Input links it. Complete AssetsTests pass natively (Debug 76/585, Debug null and Release null 77/595) and under Node (wasm-release and wasm-debug 79/637, wasm-release-null 80/647), zero skips. A scratch Get Started consumer with a MakeSpriteFont `may2026` atlas of printable ASCII plus U+00E9 drew `Café` on D3D11 and in Chrome WebGL2, and `can_render` rejected U+03A9 at unit 0. CI run 40836037123 passed all 12 jobs. [Scope and qualification](../2026-10-29-string-table-text.md). No engine string-table facility was added. |
| OCA-77 | G146, G147. Source implemented 2026-10-29 after the OCA-42 recipe: shared glyph placement, an owned measured wrap at U+0020 spaces, headless cases and a handle-based query. [Scope and qualification](../2026-10-29-word-wrap.md). Fifteen new cases pass complete RenderTests on all five native Debug backends and Release null, with zero skips; lints, website checks and both published query contracts pass. On 2026-10-29 all fifteen ran and passed under Node in wasm-release, wasm-debug and CI's wasm-release-null override. CI run 40836037123 passed all twelve jobs at a commit containing the change, so the story is closed. Original weighing: word wrap to a measured width, weighed on 2026-10-24 after a review asked for wrap and alignment helpers. Wrap is cheap - `Font::measure` already lays out line feeds, so a wrap only chooses where to insert them - but where a line may break depends on the script, which OCA-43 settles, and no caller in either client wraps by width yet: the one wrap there is a debug console cutting at a column count on purpose. It follows OCA-43 and lands with or after OCA-42, whose translated strings are the first caller. An alignment helper was declined in the same weighing: centring is one line at each of the half-dozen sites that do it, and `renderer.h` already says to `std::floor` a centred point-sampled position. |
| OCA-40 | G131: what background music costs in the page download. Follows OCA-38 (Next). |
| OCA-18 | G109: debugging in Visual Studio from the presets. |
| OCA-32 | G48: the recording renderer in an adopter's tests. Follows OCA-29 (Next). |
| OCA-66 | G151. Decided 2026-10-27: use per-monitor DPI awareness for the Windows samples/template to avoid Windows scaling their output. The engine's per-monitor window sizing support already exists; add the default declaration and adopter guidance, then record a reviewed 150% scaling run with date and commit. Keep the story open until that implementation and evidence exist; no scaled-display result is implied by the decision. Implemented 2026-10-27: `samples/minimal/dpi_aware.manifest` declares PerMonitorV2 with a PerMonitor fallback, and all six samples and the Get Started project embed it. The release guide's *Scaled displays* section states the choice. The running template was per-monitor v2 on a 100% display; the 150% run remains. |
| PI-10 | G74: an upstream answer for each filed report. Follows PI-09 (Next); the decisions are the owner's. |
| PI-19 | G74, G27: report 14, accepted narrowed and split out of PI-10 on 2026-10-22. Should, because SM-08 depends on it. |
| PI-11 | G14: the engine staging helper, only if PI-10 accepts the packaging request. Follows OCA-60 (Next). |
| PI-08 | G54: request status on the tracker. Follows OCA-72 (Now). |
| PI-15 | G78. Closed 2026-10-27: AppTests on every Windows preset launches `StartupInfoChild`, which makes its window as Application does from default ApplicationOptions with no device, once with STARTUPINFO's `SW_HIDE` and once with `SW_SHOWNORMAL`, and asserts with `IsWindowVisible` that the first stays hidden and the second is shown. Not compiled under Emscripten. |
| PI-16 | G79: drive a game past its title screen in a smoke run. Follows OCA-30 (Next). |
| PI-17 | G75. Closed 2026-10-27: README's *Using Labrador in a project* and the *Settings and saves* guide say that `read_json_file` and `parse_json` read JSON, that rapidjson is linked `PRIVATE` and named by no engine header, as `engine/CMakeLists.txt` links it, and that a game writing JSON brings its own writer. |
| PI-18 | G76. Closed 2026-10-27: `cmake/check_engine_includes.cmake` states that its pattern matches only a folder named `game/`, quoted, angle-bracketed or reached through `../`, and that a client folder with another name is not covered. The check stays the one name (T5). |
| <a id="pi-20"></a>PI-20 | Split out of report 12 on 2026-10-22 and argued in its decision record, which stands in for a STORIES.md entry: object-member enumeration and unknown-key refusal in manifest/sprite-sheet loaders (T6). Implemented 2026-10-29 with copied `JsonValue::keys()` names and one assets-private helper at nine fixed schemas; custom kinds/names and game-owned JSON remain unrestricted. CHANGES.md describes the migration. [Implementation and checks](../2026-10-29-strict-asset-keys.md) record 66 cases/444 assertions passing in native Debug, Debug-null and Release-null. On 2026-10-29 the twelve new cases ran and passed under Node in wasm-release, wasm-debug and CI's wasm-release-null override. CI run 40836037123 passed all twelve jobs at a commit containing the change, so the story is closed. |
| <a id="pi-21"></a>PI-21 | Split out of report 25 on 2026-10-27: slot-addressed, two-motor vibration transport and explicit stop, with game-owned timing, effects, preferences and routing. The [decision record](../review/colourwars-reports/vibration.md) stands in for a STORIES.md entry and owns acceptance. Settle portable lifetime/result semantics before publishing an API; validate requests, report unavailable output, cancel on suspend/teardown without stale replay, and test observable native/browser transport. Physical output evidence stays separate. Added to L26 after PI-20; the current Next order is unchanged. Implemented 2026-10-27: `Gamepads::vibrate(slot, Vibration)` sends both motors a request bounded to `max_vibration_seconds` and answers `submitted` or `unavailable`; `stop_vibration` stops it. `engine/input/vibration_output.h` holds the shared lifetime (expiry, a pad leaving, suspension, destruction, no replay) and InputTests holds it against a recording transport, with validation and the empty-slot answers on each backend's own reader. XInput sends `SetVibration`; the browser plays `dual-rumble` and resets it. The physical session, natively and in a browser, remains. |
| SRL-09 | G94: backend rationale out of a game reader's way. Follows SRL-08 (Next). |
| SM-23 | G67: regenerated goldens leave a review trace. |
| SM-24 | G115: a written procedure for moving dependency and toolchain pins. Follows SM-25 (Next). |
| SM-17 | G30: one command per area for every configuration. |
| SM-18 | G31: a record of what CI cannot run, per merged change. |
| SM-15 | G72: watch the null-backend wasm step run after a failed page step. |
| SM-26 | G68: notice when the hosted demo lags the engine. Follows PI-05 (Next). |
| SM-29 | G135: site preview and rollback. Follows SM-31 (Now). |
| SM-30 | G136: the website gate in one place; dashboard settings are owner-only. |
| SM-32 | G137: the site's LineSweeper footage in step with the sample. Closed 2026-10-27: website/README.md states the same-commit regeneration rule, and BDP-21's commit regenerates the stills and MP4 from current master on x64-release. The owner watched the whole clip; the three changed stills were looked at. [Evidence](../website-plan.md#2026-10-26-footage-refresh-evidence). |
| NMS-03 | G103: Platform support links the plans and what would change the answer. Follows OCA-01 (Now). |
| NMS-04 | G104: x64-* presets refuse a non-Windows host; wasm-* stays available and is stated as unverified (critique). |
| SRL-14 | G22. Closed 2026-10-27: the owner clarified substantial progress and an unfinished course, and Get Started now states its status. The course checkout's README, production outline and rehearsal owner describe one roughly two-hour introduction for people who already program, with storyboard, demos, graphics and shooting plans prepared; rehearsal and recording remain. Its scope is memory, tooling, stack, pointers, heap, aggregates, ownership and the build pipeline; the first teaching section explains RAM as numbered bytes and addresses. This amends the dated story's three status choices to include actual work in progress. The source copy gives no course repository link or completion claim; deployment is separate. PHILOSOPHY's introductory engine API teaching target remains; the language introduction does not establish its fulfilment, and there is no withdrawal amendment. |
| SRL-15 | G96: where to go without C++. |
| SRL-17 | G98: an interim teachability audit. |
| SRL-16 | G97: a glossary of the terms minimal uses. |
| SRL-18 | G88: run LineSweeper's rules tests knowing what is needed. |
| SRL-06 | G91: LineSweeper's design notes give reasons without internal records. |
| SRL-07 | G92. Decided 2026-10-27: rename non-macro uppercase constants to snake_case, preserving CONVENTIONS' distinction between values and macros. Inventory current declarations rather than assuming the historical count of 26, migrate engine/sample/test/website uses and the ColourWars callers, and record every old/new spelling and the counted caller repositories. The source break needs CHANGES.md and the public sample migration. Keep this story open for implementation and its affected build, test and website checks. Implemented in this tree 2026-10-27 by *Spell constants in snake_case*: engine, samples, tests and benches, with platform-API restatements kept as CONVENTIONS now allows and the public renames in CHANGES.md. The ColourWars migration and the counted caller record remain. |

</details>

## Won't this time

Held on T1 and capacity grounds: each needs recorded demand, a regression it would have caught, or a decision, before it is worth one person's capacity

**Done when**

- Each item's revisit trigger is recorded in its why, and none is reopened until that trigger exists

**Lands as**

1. W (not scheduled): SM-11, SM-28, OCA-04, OCA-47, BDP-19, SM-41

**Stories**

- [ ] **[SM-11](STORIES.md#sm-11)** Won't (this time) · M · The Sole Owner-Maintainer: Run the six sample pages in Firefox without hand-driving each
- [ ] **[SM-28](STORIES.md#sm-28)** Won't (this time) · M · The Sole Owner-Maintainer: Catch a website accessibility regression in CI
- [ ] **[OCA-04](STORIES.md#oca-04)** Won't (this time) · L · The Outside C++ Adopter · **Owner**: Watch the split-screen sample and the small samples run in a browser
- [ ] **[OCA-47](STORIES.md#oca-47)** Won't (this time) · M · The Outside C++ Adopter: See three- and four-player layouts work
- [ ] **[BDP-19](STORIES.md#bdp-19)** Won't (this time) · S · The Browser and Phone Demo Player · **Owner**: Decide whether a best score survives, and if so keep it
- [ ] **[SM-41](STORIES.md#sm-41)** Won't (this time) · S · The Sole Owner-Maintainer: A component promotion rule that can be evaluated

<details><summary>Why each is placed here</summary>

| Story | Why |
|---|---|
| SM-11 | G70: a WebDriver BiDi runner is a second protocol to maintain. Revisit when a Firefox-only regression it would have caught is recorded; BDP-06's manual record is the fallback. |
| SM-28 | G120: a CI accessibility scan buys axe-core (T9 says buy) but costs CI time. Revisit when a manual WCAG check finds a regression, or OCA-08's position names the site as an aim. |
| OCA-04 | G45 (merged with SRL-13): hosting more samples is an L bundle generalisation plus owner deploys. Revisit after SM-12's checks, SM-27's verify and a recorded request for a hosted second sample. |
| OCA-47 | G113: three- and four-player layouts exist and were extracted from a four-pane client (PHILOSOPHY.md:662-667); showing them is not urgent. Revisit with OCA-46's join screen. |
| BDP-19 | G85: whether a best score survives is a decision nobody has asked for. Revisit if the owner decides yes. |
| SM-41 | G126: a component promotion rule that can be evaluated. Revisit when a second client copies a component (PHILOSOPHY.md:603-611). |

</details>

## Deferred

Not in any horizon: each waits on something outside the backlog.

| Story | Why | Unblocked by |
|---|---|---|
| [SM-44](STORIES.md#sm-44) Record a real Android phone run of the development page and the live demo | No Android phone is accessible. This is optional future evidence and does not gate iPhone acceptance (owner decision 2026-10-27). | A borrowed Android phone; SM-14 written and SM-08 available, as SM-13 uses them |

## Risks to the plan

- Now holds 35 stories in 20 batches. It fits a few weeks only if each batch is one PR; split further and the branch, PR, green-CI, rebase-merge overhead crowds out the compounding items (OCA-68, OCA-11, OCA-73, SM-21). OCA-68's backfill (L) and OCA-11's consumer job (M) are the schedule risks inside Now.
- Owner review is the binding constraint, not agent time: every owner sign-off and repository setting in Now is made, amendment B has landed and `v0.1.0` is cut on it, and the hosted demo is redeployed and linked: Now is finished. Next's owner-gated batches (X3, X5, X6, X21, X25, X27) are where it binds now.
- PHILOSOPHY.md is touched by six PRs across three horizons. They are serialised A to F and each rebases on the last; a parallel branch that edits PHILOSOPHY outside that order will conflict and should be refused at review.
- The primary persona is unevidenced: no issues, analytics or independent adopter. Next spends real capacity on a stranger's ship path. Mitigation: most of Now is truth fixes that also serve the agents who read the same documents, and PI-09/PI-10 are the only stories backed by recorded demand.
- Doc edits can fail the website build by design: quoted lines and public header comments are read at build time. Header-contract stories (OCA-55, OCA-59, OCA-56) and the brief trim (SM-43) must fix any quoting page in the same commit.
- The consumer CI job adds build minutes and a vcpkg cold cache to every PR. If it is slow it will tempt someone to drop it; cache it deliberately and keep SM-21's documented contexts in place.
- Hardware-bound claims may stay open: the iGPU p99 needs AMD's driver first and may still need a code option if the per-app preference fails; the Big Sur MacBook Air's Safari may sit below the wasm-exceptions floor. BDP-10 now waits only on the real iPhone and host layout checks; Android remains unverified without an accessible device.
- Windows Sandbox (OCA-61, PI-14) proves the loader and runtime requirements on a clean OS, but its default virtual GPU is the host's adapter behind a paravirtual driver (Release has no WARP fallback, so `--vgpu Disable` gives a loud refusal); G77's target-GPU half stays open until a real machine runs it.
- Linking the live demo widens its exposure before Safari, real phones and the player-facing failure messages are settled. BDP-23's flash check now precedes the link, and the platform statement names the phone gate plainly.

## What the adversarial review changed

A critic reviewed the first draft of this backlog; these are its corrections and how each was applied.

| Correction | Applied |
|---|---|
| Merge the 23 duplicate groups into their leads (orchestrator list) | Done. 27 stories folded (NMS-01, SM-37, BDP-01, SRL-01, SRL-13, PI-13, PI-06, PI-04, SM-33, PI-01, SM-34, PI-02, SM-35, PI-03, PI-07, SM-42, SRL-10, SRL-11, SRL-12, PI-12, SM-38, SM-39, NMS-05, FES-01, BDP-02, BDP-08, BDP-05). Each lead keeps its id, lists merged_from, takes the folded personas as also_serves, unions gap and journey ids (plus the journeys and gaps each group named), and carries a rewritten acceptance list that takes the folded stories' needed criteria. Folded ids appear nowhere in the backlog; every depends_on that named one was re-pointed at its lead. |
| Apply the 105 t_ref fixes from the citation check | Done: each listed story's t_refs replaced by its corrected list. For merged leads the corrected lists of lead and folded stories were combined and deduplicated by hand. OCA-01, not on the list, carried the same T6-for-page-wording misuse as NMS-01, so it took NMS-01's corrected refs. OCA-62's 'the bought edge carries its terms' was reworded in OCA-61's corrected form for the same reason. OCA-48 and SM-39's corrected refs contradict each other on a backgrounded pad; SM-39's reading (gamepad_reader.h:50-52 states an engine contract, so a failure is fixed in engine/input/xinput/) was taken and OCA-48's acceptance rewritten to match. |
| Critique 1: OCA-02/SM-37 misread PHILOSOPHY.md:663 as resting on 'no caller in this tree' | Applied. OCA-05 (lead of SM-37) amends only the factual sentence, keeps the passage's refusal of the caller-count argument word for word, says local_multiplayer strengthens 'the mechanism stays', and carries no owner decision. OCA-02 dropped its PHILOSOPHY criterion and lands in or after OCA-05's commit. |
| Critique 2: the matt_math.h risk and SM-36's why are stale; consider a pin move in Now | Applied with owner confirmation (ColourWars 77a7a7f; pin 4cefb68, 45 behind). The risk is deleted and replaced by a risk about the stale MEMORY.md index line. SM-36's acceptance and notes call the next move an ordinary bump. Considered for Now and placed first in Next instead: OCA-11 (Now) already supplies the consumer check the critique wanted, and the bump is cheapest once OCA-68's backfill (Now) and OCA-70's procedure exist. PI-14 follows it in Next. |
| Critique 3: G116-G151 unregistered | Void: register-final.json holds them (G124, G125, G148, G149 and the rest were checked by id). No story is marked 'unregistered gap'; the add_to_lead notes asking to register G124, G125, G148 and G149 were dropped for the same reason. |
| Critique 4: BDP-23's dependency points the wrong way and its method is unnamed | Applied. OCA-03 now depends on BDP-23; BDP-23 is promoted to Must (the Must link waits on it) and stays in Now because G118 is registered. Its acceptance names the method: LineSweeperCapture frames from a checked-in script, analysed frame by frame with the WCAG relative-luminance formula against WCAG 2.3.1's three-flashes threshold. |
| Critique 5: SM-12/BDP-02's Now exit criterion cannot be met (phone item; stale deploy step) | Applied. SM-12 (lead of BDP-02) ticks the server-side checks now, records the phone item under Chrome emulation labelled as such, links its real-phone half to SM-13, and rewrites the deploy step to name PersonalWebsite main plus npm run deploy. The Now exit criterion says 'server-side checks ticked'. |
| Critique 6: SM-01 skips the cheap precondition (AMD driver) | Applied with the owner fact (1002:13C0 on the Microsoft Basic Display Adapter; Windows' per-app GPU preference is a no-code route). SM-01 gains an XS first step: install AMD's driver, then try the per-app preference; a code option is argued in its own story only if that fails, read at device creation or launch. Size M to S, moved from Later to Next beside SM-03, owner-only. PI-14's iGPU criterion now records which driver it ran on. |
| Critique 7: SM-09/BDP-05 (Safari on the MacBook Air) belong earlier | Applied with the owner fact (Big Sur ceiling). SM-09, now the lead of BDP-05, moves to Next before OCA-28; it records the exact Safari ceiling from Apple's update source (the brief's 'Safari 14-ish' is to be confirmed, since Big Sur took later Safari releases) and plays the demo only if the ceiling meets the wasm-exceptions floor. OCA-28 and OCA-01 say 'checked on Safari X' or 'out of reach on owned hardware' rather than 'unchecked'. |
| Critique 8: SM-39 invents a no-pad constraint | Answered by the owner: a physical XInput pad is owned. SM-39 is folded into OCA-48, which moves from Later to Next with the 'blocked until a pad is to hand' text removed. BDP-06 moves to Next too and can share the session. SM-40 stays separate (no hardware needed). |
| Critique 9: NMS-04 would refuse the wasm presets on non-Windows hosts | Applied. The story was already scoped to x64-* presets; acceptance now also requires the wasm-* presets to carry no host condition and the docs to call them unverified, not refused, on a non-Windows host, with an optional recorded Linux configure attempt. |
| Critique 10: OCA-22 should precede OCA-20 | Applied. OCA-22 moves from Later to Next as Should, ranked before OCA-20, and OCA-20 depends on it; OCA-20's acceptance no longer has to explain the d3d11 default. |
| Critique 11: split OCA-37 (font terms) | Applied. OCA-37 becomes the disclosure (XS, Must, Now, in the sprites-and-fonts batch with OCA-34). Replacing the font is new story OCA-75 (M, Could, Next, owner decision), because it means regenerating and reviewing every golden and capture that draws text. |
| Critique 12: FES-03 is an amendment to a recorded decision, not an open owner decision | Applied. FES-03 cites CLAUDE.md's 'permanently out of scope' and needs an owner sign-off only; it is off the owner-decision blockers (listed as a sign-off) and lands in PHILOSOPHY amendment A with OCA-05. |
| Critique 13: four parallel PHILOSOPHY edits in Now will conflict | Applied and extended. All PHILOSOPHY amendments are sequenced as separate PRs: A (Now: OCA-05, FES-03, NMS-02), B (Now, last: OCA-69, OCA-67, owner-worded, with the first tag), C (Next: OCA-08), D (Next: SM-03, if it lands in PHILOSOPHY), E (Later: SM-02, after D), F (Later: SRL-14, only if withdrawn). The method states the order. |
| Critique 14: the consumer CI job would build against master | Applied to OCA-11 (lead of PI-06): the consumer takes the PR's own checked-out engine tree and runs from a short workspace path. Size stays M. |
| Critique 15: MoSCoW inconsistencies along dependency chains; OCA-60's unexplained OCA-29 dependency | Applied. SM-02 and SM-05 promoted to Should (SM-07), BDP-04 to Should (BDP-15); also SM-04 and SM-08 to Should and BDP-23 to Must, found by the same check. OCA-60 no longer depends on OCA-29. The build script checks every dependency for horizon and MoSCoW order and fails on a violation. |
| Critique 16: anti-persona and tertiary minors in Now while primary majors wait | Applied. FES-02, FES-04, SRL-02, SRL-04 and SRL-05 open Next (X4). FES-01 stays in Now inside OCA-06's README edit, SM-31 stays. The freed slots take OCA-37's disclosure and OCA-34 (G12 major). FES-04's note no longer asks to land in FES-03's commit; it completes the list in a follow-up PR that keeps FES-03's permanence markers. |
| Process gap: stories have no acceptance criteria | Void: stories.json has acceptance criteria on every story; the merged and edited stories carry rewritten ones. |
| Process gap: owner-only actions not separated from agent work | Applied: owner_only is set on 31 stories (decisions, repository settings, hardware in hand, the PersonalWebsite and ColourWars-side actions), and owner_decisions lists 16 decisions with what each blocks. Stories whose only change is owner_only are carried by owner_only_ids rather than repeated in changed_stories. |
| Critique unstoried item: the agent brief's narrative history | Added SM-43 (sole-maintainer, M, Next rank 1, depends on SM-22 and OCA-68), registered against G66 as the nearest gap; no registered gap covers the trimming itself. |
| Critique unstoried item: a host-site integrator embedding linesweeper.js | Added OCA-74 (outside-cpp-adopter, also serves browser-demo-player; S, Could, Later, depends on OCA-26 and OCA-23), registered against G142 and G51. Persona argued in its notes: a host integrator codes against a module contract on the adopter's ship-to-the-web journey, and the bundle is the shape an adopter copies. |
| Owner fact: iPhone owned, no Android phone | SM-13 (lead of BDP-08) becomes the iPhone run (title, i_want and acceptance rewritten) and leaves deferred for Later, with SM-14 (written in the same session) and SM-08 promoted to Should. The Android run is new story SM-44, deferred. BDP-10 stays deferred, needing SM-44 or the owner's decision that iPhone evidence suffices (an owner decision). OCA-54's note says 'not yet played on a real iPhone' is now closable. |
| Owner fact: physical XInput pad owned | OCA-48 (with SM-39) and BDP-06 move to Next; see critique 8. |
| Owner fact: MacBook Air runs up to Big Sur | SM-09 (with BDP-05) moves to Next and is sized S; see critique 7. |
| Owner fact: iGPU on the Microsoft Basic Display Adapter until AMD's driver is installed | SM-01's first step and PI-14's driver line; see critique 6. |
| Owner fact: ColourWars' math-split migration is done; pin 4cefb68, 45 behind | SM-36 reworded to an ordinary bump; risk replaced; OCA-68's backfill serves that bump; see critique 2. |
| Journey id 'oca-native-gamepad-local-play' (cited by SM-39 and SM-40, and named in the OCA-48 merge instruction) is not defined in journeys.json | Replaced by the defined journey oca-grow-a-game on OCA-48 and SM-40. |
