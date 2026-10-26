# Backlog

The order of work: 164 stories in four horizons, each with the exit criteria that end it. **This is the one file in `docs/product/` that is kept current**: tick a box in the same commit as the change that closes the story, and amend a horizon here rather than letting the work drift away from it. The other files are the design record of 2026-10-13 and are read as of that date.

Each line reads: id, MoSCoW, size, the persona it serves, title, and what it waits on. The reasoning for every placement is in the *why* under each horizon; stories are in [STORIES.md](STORIES.md).

## How the order was decided

Built from the ranked backlog after three corrections to its inputs. (a) Duplicates were merged before ranking: 23 groups fold 27 stories into their leads, so each piece of work appears once and capacity is not double-counted. (b) Every t_ref was checked against the cited text; 105 were corrected, and where a merge joined two incompatible readings (OCA-48/SM-39 on a backgrounded pad) the reading that matches the header's stated contract was taken. (c) The critique's 16 corrections and the owner's 2026-10-13 facts were applied: a physical XInput pad and an iPhone are owned, no Android phone; the MacBook Air stops at Big Sur; the iGPU needs AMD's driver before any adapter choice; ColourWars' math-split migration is done, so its next pin move is an ordinary bump. Horizons were then set by the earlier tests, in order: (1) does delay compound (change list, consumer CI, security route, agent brief); (2) does it serve the primary outside-cpp-adopter's evaluate-adopt-ship-keep path; (3) can one person with agents finish it on owned hardware. Now holds 35 stories in 20 commit batches, each one self-contained PR under the branch, PR, rebase-merge-on-green rule; only two are larger than S (OCA-68 L, OCA-11 M), and every owner-only step in Now is a sign-off or a repository setting rather than hardware. PHILOSOPHY.md is amended in a fixed order of separate PRs, each rebased on the last, so parallel agent branches never conflict on it: A (Now) OCA-05 + FES-03 + NMS-02, facts and permanence, no owner decision; B (Now, last) OCA-69 + OCA-67, the deletion rule, the settle criterion and the first 0.x tag, owner-worded; C (Next) OCA-08, the accessibility position in the UI section, owner-worded; D (Next) SM-03, the p99 column in Performance, only if the owner's decision lands in PHILOSOPHY rather than the ratchet document; E (Later) SM-02, the four-core hold sentence at :691-693, after D; F (Later) SRL-14, T10 and The public face, only if the series is withdrawn. Dependency rules were checked mechanically: no dependency sits in a later horizon than its dependant or carries a weaker MoSCoW, so SM-02, SM-04, SM-05, SM-08, SM-14, BDP-04 and BDP-23 were promoted where a stronger story waits on them. Engine mechanism is added only where a present caller is recorded (PI-05, PI-11 after PI-10). No item adds an editor, scripting, 3D, online play or action mapping.

## Decisions only the owner can make

| Decision | Blocks |
|---|---|
| Record the accessibility position: what the engine provides, whether the hosted demo has an aim, and who owns the colour-blind palette | OCA-08, BDP-24, BDP-21 |
| Decided 2026-10-26: ColourWars' pin reaches each Labrador minor tag within a week of it being cut; its gameplay check is ColourWars' own, outside this repository | SM-36, PI-14 |
| Decide which p99 column the pipeline-depth claims are judged by, and whether it lands in PHILOSOPHY or the ratchet document | SM-03, SM-01, SM-07, SM-02 |
| What private ColourWars detail may be published when the seven drafts and the packaging gap are filed, then accept, decline or split each report | PI-09, PI-10, PI-11 |
| Whether one phone platform's evidence (the iPhone) is enough to lift the phone gate, or the Android run is required | BDP-10 |
| The 'Introduction to C++' series: not started, planned with an outline, or withdrawn | SRL-14, SRL-15, SRL-17, SRL-16 |
| Constants naming: amend CONVENTIONS to the 26 SCREAMING constants or rename them across both repositories | SRL-07 |
| Whether the site starts versioning at the first 0.x tag (docs/website-versioning.md) |  |
| Whether the default DPI awareness stays unaware or becomes per-monitor aware | OCA-66 |

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
25. X25 p99 column decision (PHILOSOPHY amendment D if it lands there): SM-03
26. X26 iGPU first step: AMD driver and per-app preference: SM-01
27. X27 File the ColourWars drafts upstream: PI-09
28. X28 Reviews index and CI guards: SM-16, SM-19, SM-25

**Stories**

- [ ] **[SM-43](STORIES.md#sm-43)** Should · M · The Sole Owner-Maintainer: A current-state agent brief, with its narrative history moved out — after SM-22, OCA-68
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
- [ ] **[OCA-08](STORIES.md#oca-08)** Should · S · The Outside C++ Adopter · **Owner**: Know what accessibility the engine gives my game and what is mine to do — after OCA-05
- [ ] **[BDP-24](STORIES.md#bdp-24)** Could · XS · The Browser and Phone Demo Player: The demo says what it offers players with access needs — after OCA-08
- [x] **[BDP-03](STORIES.md#bdp-03)** Should · M · The Browser and Phone Demo Player: ctest loads and plays the bundle page a visitor gets
- [x] **[BDP-04](STORIES.md#bdp-04)** Should · M · The Browser and Phone Demo Player: The bundle page's failure paths are tested — after BDP-03
- [x] **[PI-05](STORIES.md#pi-05)** Could · S · The Pinned Submodule Integrator: The engine tells my build which revision of it I am shipping — after OCA-67
- [ ] **[SM-03](STORIES.md#sm-03)** Should · XS · The Sole Owner-Maintainer · **Owner**: Decide which p99 the pipeline-depth claims are judged by
- [ ] **[SM-01](STORIES.md#sm-01)** Should · S · The Sole Owner-Maintainer · **Owner**: Run the frame bench on the integrated Radeon, not the RTX 5080 — after SM-03
- [ ] **[PI-09](STORIES.md#pi-09)** Should · S · The Pinned Submodule Integrator · **Owner**: File the seven live drafts and the packaging gap upstream — after OCA-72
- [x] **[SM-16](STORIES.md#sm-16)** Should · S · The Sole Owner-Maintainer: One index of reviews and which ledger is live — after SM-06
- [ ] **[SM-19](STORIES.md#sm-19)** Could · XS · The Sole Owner-Maintainer: Gate the offline tooling tests in CI — after SM-21
- [ ] **[SM-25](STORIES.md#sm-25)** Should · S · The Sole Owner-Maintainer: Pin the Vulkan SDK, record the browser, and detect drift on a schedule — after SM-21

<details><summary>Why each is placed here</summary>

| Story | Why |
|---|---|
| SM-43 | No registered gap (closest G66): the brief's narrative history fights CONVENTIONS' no-history rule and costs every agent context. Implemented 2026-10-26: 680 lines to 274 initially, 285 at PR verification, with the original and relocation map in [the dated record](../2026-10-26-agent-brief-history.md). On 2026-10-27 all 81 Python tests, four CMake checks, native checks and website check/21 tests/146-page build pass. PR CI verification remains. |
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
| PI-14 | G77: ColourWars' ZIP has never run on a player's clean machine. No longer blocked on the migration; follows the bump. |
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
| OCA-08 | G119 (merged with SM-38): the accessibility position. PHILOSOPHY amendment C, owner-worded, after A and B. |
| BDP-24 | G119: the demo states what it offers players with access needs, following the recorded position. |
| BDP-03 | G33 (major): the page hosts serve had no automated test. Closed 2026-10-27: following the initial GPU startup denial, all 31 `wasm-release` entries pass locally in Chrome 156.0.8078.48 on the RTX 5080. The [PR #59 wasm job](https://github.com/matt439/Labrador/actions/runs/40419996949/job/121911813526), at initial head `0fffdf1`, also passes and reports `LineSweeperBundlePageTests` passed in 6.62 seconds on Chrome 156 with SwiftShader. [Evidence and limits](../port/web.md#packaged-page-verification-2026-10-27). |
| BDP-04 | G33: the bundle page's failure paths. Closed 2026-10-27: `LineSweeperBundlePageTests` passes locally in Chrome 156.0.8078.48 within the 31-entry `wasm-release` run, including context loss/restore, truncated-wasm failure and captured-config `onError` recovery UI. [Evidence](../port/web.md#packaged-page-verification-2026-10-27). The earlier GPU startup denial is superseded for this local check; no hosted or physical-device acceptance is added. |
| PI-05 | G53: no engine revision a consumer can stamp; two present callers (the web bundle and a client). The first tag (Now) gives the fallback value. Closed 2026-10-26: LABRADOR_SOURCE_REVISION exports a configure-time hash/dirty or unknown identity, with an own-root guard for archives. Offline checkout/consumer/dirty-refresh/worktree/unborn/archive/missing-Git and bundle fixtures pass; the existing CI lint job runs them. README documents reconfigure-before-packaging and the bundle build-time distinction. |
| SM-03 | G26: which p99 the pipeline-depth claims are judged by. An owner decision; PHILOSOPHY amendment D only if it lands there. |
| SM-01 | G01 (major): the bench cannot reach the iGPU. The XS first step (AMD driver, per-app preference, no code) moves it to Next beside SM-03 (critique, owner facts); a code option is argued only if that fails. |
| PI-09 | G74: seven live drafts and the packaging gap in ColourWars' private ledger, filed into the route OCA-72 opens. What private detail is published is the owner's. |
| SM-16 | G29: one index of reviews and which ledger is live. Follows SM-06 (Now). Closed 2026-10-26: docs/review/INDEX.md maps every review entry to its live ledger or frozen disposition, owns remaining D3D12/Vulkan candidate tracking and links the verification limits. AGENTS points there; historical review README.md is unchanged. Local paths and anchors were checked. |
| SM-19 | G32: the offline tooling tests were not gated in CI. Implemented 2026-10-26: the existing Windows lint job runs the tools unittest suite as a non-tolerated step beside the flash suite; priority 3 is tracked in STATUS.md. On 2026-10-27 all 81 local Python tests pass with no skips, superseding the earlier execution-policy blocker. The deliberately failing throwaway-branch CI observation remains. |
| SM-25 | G133: the Vulkan SDK floats and drift is undetected. Implemented 2026-10-26: SDK 1.4.357.0 and its official SHA-256 are pinned, Monday 03:17 UTC forces the full CI matrix, and the page runner logs browser identity. On 2026-10-27 [LunarG metadata](https://vulkan.lunarg.com/sdk/home) confirms the pin/hash and the installer's local HEAD returns 200, superseding the earlier local socket denial. PR #59's [hosted Vulkan job](https://github.com/matt439/Labrador/actions/runs/40419996949/job/121911813573) receives HTTP 403 before installation; its cause is not established. The scheduled Actions observation remains open. |

</details>

## Later

The browser in depth for adopters, a kinder hosted demo, the measured low tier and phones, couch and text depth, and the maintainer's guards against drift

**Done when**

- The page contract, the LineSweeper bundle's factory contract, browser troubleshooting and the browser notices are public
- The demo's failure messages are written for players, it probes support before downloading, pauses on focus loss, honours reduced motion, and every landing shadow meets 3:1 contrast
- A low-tier p99 on the integrated Radeon is published with its pacing rule and lock verdict, or the reason it could not be taken is recorded
- A real iPhone has played the development page and the live demo, with the LAN-serve procedure written in that session; a pacing method exists and runs on the owner's displays are recorded
- PI-10 has decided each filed report, and only then is the engine staging helper built or declined
- Golden regenerations leave a review trace, the five-backend-plus-wasm sweep is one command per area, and site preview and rollback are written down

**Lands as**

1. L1 Page contract for an adopter module: OCA-26
2. L2 Browser troubleshooting and the gesture rule: OCA-24, OCA-25
3. L3 Browser notices: OCA-63
4. L4 Check my page loads and quits: OCA-31
5. L5 LineSweeper bundle factory contract for other hosts: OCA-74
6. L6 Verify a web bundle before deploying: SM-27
7. L7 Demo failure paths a player can act on: BDP-15, BDP-16, BDP-17
8. L8 Demo controls, focus and phone instructions: BDP-11, BDP-12, BDP-13, BDP-14
9. L9 Demo contrast and reduced motion: BDP-21, BDP-22
10. L10 Phone fallback data use (PersonalWebsite repository): BDP-20
11. L11 Play again stays fast: BDP-18
12. L12 Host page fits a phone held upright: BDP-09
13. L13 Local bench procedure, lock verdict and four-core hold (PHILOSOPHY amendment E): SM-04, SM-05, SM-02
14. L14 Take and publish the low-tier p99: SM-07
15. L15 Headless XInput translation test: SM-40
16. L16 Browser pacing and phone top-out method: SM-08
17. L17 Real iPhone run with the LAN-serve procedure: SM-14, SM-13
18. L18 Pacing on visitors displays: BDP-07
19. L19 Couch play: join screen: OCA-46
20. L20 Settings screen and pointer menus: OCA-51, OCA-50
21. L21 Text beyond ASCII: OCA-41, OCA-43, OCA-42, OCA-77
22. L22 Background music cost: OCA-40
23. L23 Debugging, recording renderer and DPI: OCA-18, OCA-32, OCA-66
24. L24 Answer the filed reports, then staging: PI-10, PI-19, PI-11
25. L25 Request status labels: PI-08
26. L26 Integrator edge contracts: PI-15, PI-16, PI-17, PI-18, PI-20
27. L27 Backend rationale out of game-facing pages: SRL-09
28. L28 Golden review trace: SM-23
29. L29 Pin procedures and configuration sweeps: SM-24, SM-17, SM-18, SM-15
30. L30 Hosted demo lag and site operations: SM-26, SM-29, SM-30, SM-32
31. L31 Platform support links and the non-Windows configure refusal: NMS-03, NMS-04
32. L32 C++ series status (PHILOSOPHY amendment F only if withdrawn) and the learner path: SRL-14, SRL-15, SRL-17, SRL-16
33. L33 LineSweeper design notes and the constants rule: SRL-18, SRL-06, SRL-07
34. L34 A per-user data directory from the engine, once a second caller asks: OCA-76

**Stories**

- [ ] **[OCA-26](STORIES.md#oca-26)** Should · S · The Outside C++ Adopter: One public statement of what my page owes the engine module — after OCA-20
- [ ] **[OCA-24](STORIES.md#oca-24)** Should · S · The Outside C++ Adopter: Troubleshoot my browser build — after OCA-20
- [ ] **[OCA-25](STORIES.md#oca-25)** Could · XS · The Outside C++ Adopter: Know that a one-shot sound before the first gesture is dropped in a browser
- [ ] **[OCA-63](STORIES.md#oca-63)** Could · S · The Outside C++ Adopter: Know the notices my browser build must ship — after OCA-62
- [ ] **[OCA-31](STORIES.md#oca-31)** Could · S · The Outside C++ Adopter: Check my game's page loads and quits — after OCA-20
- [ ] **[OCA-74](STORIES.md#oca-74)** Could · S · The Outside C++ Adopter: Embed the LineSweeper bundle on my own site from a published contract — after OCA-26, OCA-23
- [ ] **[SM-27](STORIES.md#sm-27)** Could · S · The Sole Owner-Maintainer: Verify a web bundle before deploying it, from this repository — after PI-05
- [ ] **[BDP-15](STORIES.md#bdp-15)** Should · S · The Browser and Phone Demo Player: Failure messages a player can act on — after BDP-04
- [ ] **[BDP-16](STORIES.md#bdp-16)** Could · S · The Browser and Phone Demo Player: A graphics context that never comes back offers a way forward — after BDP-04
- [ ] **[BDP-17](STORIES.md#bdp-17)** Could · S · The Browser and Phone Demo Player: Know before downloading that my browser cannot run the game — after BDP-04
- [ ] **[BDP-11](STORIES.md#bdp-11)** Could · S · The Browser and Phone Demo Player: The game pauses when it loses my keys
- [ ] **[BDP-12](STORIES.md#bdp-12)** Could · S · The Browser and Phone Demo Player: One complete, consistent list of controls on the page and in the game
- [ ] **[BDP-13](STORIES.md#bdp-13)** Could · XS · The Browser and Phone Demo Player: A phone sees phone instructions, not keyboard ones — after BDP-12, BDP-03
- [ ] **[BDP-14](STORIES.md#bdp-14)** Could · XS · The Browser and Phone Demo Player: A controller lifts the phone gate, as the page says it will
- [ ] **[BDP-21](STORIES.md#bdp-21)** Could · S · The Browser and Phone Demo Player: A landing shadow I can see for every piece — after OCA-08
- [ ] **[BDP-22](STORIES.md#bdp-22)** Could · S · The Browser and Phone Demo Player: Fewer sparks when I have asked my system for reduced motion — after BDP-23
- [ ] **[BDP-20](STORIES.md#bdp-20)** Could · XS · The Browser and Phone Demo Player · **Owner**: The phone fallback does not spend my data without asking
- [ ] **[BDP-18](STORIES.md#bdp-18)** Could · S · The Browser and Phone Demo Player: Playing again many times stays fast and keeps the graphics — after BDP-03
- [ ] **[BDP-09](STORIES.md#bdp-09)** Could · S · The Browser and Phone Demo Player: The host page fits a phone held upright — after BDP-03
- [ ] **[SM-04](STORIES.md#sm-04)** Should · S · The Sole Owner-Maintainer: A local run procedure for LineSweeperFrameBench
- [ ] **[SM-05](STORIES.md#sm-05)** Should · S · The Sole Owner-Maintainer: Apply the ratchet's lock verdict to a non-NVIDIA local run — after SM-04
- [ ] **[SM-02](STORIES.md#sm-02)** Should · S · The Sole Owner-Maintainer: Reproduce the four-core hold, and say what the bench's fan-out measures — after SM-03
- [ ] **[SM-07](STORIES.md#sm-07)** Should · M · The Sole Owner-Maintainer · **Owner**: Take and publish the low-tier p99 — after SM-04, SM-05, SM-02, SM-01, SM-03
- [ ] **[SM-40](STORIES.md#sm-40)** Could · S · The Sole Owner-Maintainer: A headless test of the XInput translation
- [ ] **[SM-08](STORIES.md#sm-08)** Should · S · The Sole Owner-Maintainer: A method for judging browser pacing (R5) and timing the phone top-out frame (R6) — after PI-19
- [ ] **[SM-14](STORIES.md#sm-14)** Should · S · The Sole Owner-Maintainer · **Owner**: Serve a development page to a phone on my network
- [ ] **[SM-13](STORIES.md#sm-13)** Should · M · The Sole Owner-Maintainer · **Owner**: Record a real iPhone run of the development page and the live demo — after SM-14, SM-08
- [ ] **[BDP-07](STORIES.md#bdp-07)** Could · M · The Browser and Phone Demo Player · **Owner**: Smooth play on the displays visitors actually use — after SM-08
- [ ] **[OCA-46](STORIES.md#oca-46)** Could · M · The Outside C++ Adopter: See a join screen for couch play I can copy
- [ ] **[OCA-51](STORIES.md#oca-51)** Could · M · The Outside C++ Adopter: Wire a settings screen to what the engine can actually set — after OCA-53
- [ ] **[OCA-50](STORIES.md#oca-50)** Could · S · The Outside C++ Adopter: Make a menu work with a click or a tap
- [ ] **[OCA-76](STORIES.md#oca-76)** Could · S · The Outside C++ Adopter: Ask the engine where my game may write, on Windows and in the browser — after OCA-53
- [ ] **[OCA-41](STORIES.md#oca-41)** Could · S · The Outside C++ Adopter: My non-English literals draw the same on Windows and in the browser
- [ ] **[OCA-43](STORIES.md#oca-43)** Could · S · The Outside C++ Adopter: Know which languages the text path can draw
- [ ] **[OCA-42](STORIES.md#oca-42)** Could · M · The Outside C++ Adopter: Get my game's language from a string table to the screen — after OCA-41, OCA-43, OCA-34
- [ ] **[OCA-77](STORIES.md#oca-77)** Could · S · The Outside C++ Adopter: Wrap a translated string to a width — after OCA-43, OCA-42
- [ ] **[OCA-40](STORIES.md#oca-40)** Could · S · The Outside C++ Adopter: Know what background music costs and how to keep it out of the page download — after OCA-38
- [ ] **[OCA-18](STORIES.md#oca-18)** Could · S · The Outside C++ Adopter: Debug my game in Visual Studio from the presets
- [ ] **[OCA-32](STORIES.md#oca-32)** Could · S · The Outside C++ Adopter: Use the recording renderer in my own tests — after OCA-29
- [ ] **[OCA-66](STORIES.md#oca-66)** Could · S · The Outside C++ Adopter: Know whether my game is crisp on a scaled display
- [ ] **[PI-10](STORIES.md#pi-10)** Could · M · The Pinned Submodule Integrator · **Owner**: An upstream answer for each filed report — after PI-09
  - Each report's decision and the commit that answered it: [the reports table](../review/colourwars-reports/README.md#the-decisions).
- [x] **[PI-19](STORIES.md#pi-19)** Should · M · The Pinned Submodule Integrator: Read what the engine's last frame cost, phase by phase
- [ ] **[PI-11](STORIES.md#pi-11)** Could · M · The Pinned Submodule Integrator: Stage the runtime files the engine makes my game need, without hand-keeping that list — after PI-10, OCA-60
- [ ] **[PI-08](STORIES.md#pi-08)** Could · S · The Pinned Submodule Integrator: See what happened to each request and which commit answered it — after OCA-72
- [ ] **[PI-15](STORIES.md#pi-15)** Could · S · The Pinned Submodule Integrator: Launching my game hidden through STARTUPINFO is tested upstream
- [ ] **[PI-16](STORIES.md#pi-16)** Could · S · The Pinned Submodule Integrator: Drive my game past its title screen with injected input in a smoke run — after OCA-30
- [ ] **[PI-17](STORIES.md#pi-17)** Could · XS · The Pinned Submodule Integrator: Know what JSON the engine reads for me and when my game must bring its own parser
- [ ] **[PI-18](STORIES.md#pi-18)** Could · XS · The Pinned Submodule Integrator: The include wall says exactly what it protects in my build
- [ ] **[PI-20](../review/colourwars-reports/content.md#12-json-from-memory-key-enumeration-source-spans)** Could · S · The Pinned Submodule Integrator: A misspelt key in a manifest or sprite sheet is refused, not read as its default
- [ ] **[SRL-09](STORIES.md#srl-09)** Could · S · The Sample-Reading Learner: Game-facing reference pages keep backend rationale out of my way — after SRL-08
- [ ] **[SM-23](STORIES.md#sm-23)** Should · S · The Sole Owner-Maintainer: Leave a trace that regenerated goldens were looked at
- [ ] **[SM-24](STORIES.md#sm-24)** Could · S · The Sole Owner-Maintainer: A written procedure for moving dependency and toolchain pins — after SM-25
- [ ] **[SM-17](STORIES.md#sm-17)** Could · M · The Sole Owner-Maintainer: One command per area that runs every configuration a change must pass
- [ ] **[SM-18](STORIES.md#sm-18)** Could · S · The Sole Owner-Maintainer: A record of the configurations CI cannot run, per merged change — after SM-17
- [ ] **[SM-15](STORIES.md#sm-15)** Could · XS · The Sole Owner-Maintainer: Observe the null-backend wasm step running after a failed page step, and the job going red
- [ ] **[SM-26](STORIES.md#sm-26)** Could · S · The Sole Owner-Maintainer: Notice when the hosted demo lags the engine — after PI-05
- [ ] **[SM-29](STORIES.md#sm-29)** Could · S · The Sole Owner-Maintainer: A procedure for previewing and rolling back the site — after SM-31
- [ ] **[SM-30](STORIES.md#sm-30)** Could · S · The Sole Owner-Maintainer · **Owner**: Change the website gate in one place — after SM-29
- [ ] **[SM-32](STORIES.md#sm-32)** Could · S · The Sole Owner-Maintainer: Keep the site's LineSweeper footage in step with the sample
- [ ] **[NMS-03](STORIES.md#nms-03)** Could · S · The Native Multi-Platform Shipper (anti-persona, for now): The Why page's Platform support section links the plans, their scope, and what would change the answer — after OCA-01
- [ ] **[NMS-04](STORIES.md#nms-04)** Could · S · The Native Multi-Platform Shipper (anti-persona, for now): Configuring on a non-Windows host refuses early and points at the platform statement — after OCA-01
- [ ] **[SRL-14](STORIES.md#srl-14)** Could · S · The Sample-Reading Learner · **Owner**: Know whether the 'Introduction to C++' series exists, is planned or is dropped
- [ ] **[SRL-15](STORIES.md#srl-15)** Could · XS · The Sample-Reading Learner: Get Started tells me where to go if I do not know C++ yet — after SRL-14
- [ ] **[SRL-17](STORIES.md#srl-17)** Could · M · The Sample-Reading Learner: A recorded check that the game-facing API is teachable to a beginner — after SRL-14
- [ ] **[SRL-16](STORIES.md#srl-16)** Could · M · The Sample-Reading Learner: A guide to the C++ and engine terms the minimal sample uses — after SRL-17
- [ ] **[SRL-18](STORIES.md#srl-18)** Could · S · The Sample-Reading Learner: Run LineSweeper's rules tests knowing what I need and where it works
- [ ] **[SRL-06](STORIES.md#srl-06)** Could · S · The Sample-Reading Learner: LineSweeper's design notes give their reasons without sending me to internal records
- [ ] **[SRL-07](STORIES.md#srl-07)** Could · S · The Sample-Reading Learner · **Owner**: One naming rule for constants that matches the published headers

<details><summary>Why each is placed here</summary>

| Story | Why |
|---|---|
| OCA-26 | G142, G143: no public page contract; a misspelled callback gives a blank canvas. Follows OCA-20 (Next). |
| OCA-24 | G112: browser-build troubleshooting. Follows OCA-20 (Next). |
| OCA-25 | G132: a one-shot sound before the first gesture is dropped, silently. |
| OCA-63 | G16: the browser build's notices. Follows OCA-62 (Next). |
| OCA-31 | G11: check a game's page loads and quits by hand. Follows OCA-20 (Next). |
| OCA-74 | G142, G51: the LineSweeper bundle's factory contract for a host other than mattharding.dev (new; critique's unstoried list). Links OCA-26 and OCA-23 rather than repeating them. |
| SM-27 | G144: verify a web bundle before deploying, from this repository. Follows PI-05 (Next). |
| BDP-15 | G82: failure messages a player can act on. Follows BDP-04 (Next). |
| BDP-16 | G83: a context that never returns offers a way forward. |
| BDP-17 | G84: know before downloading that a browser cannot run the game. |
| BDP-11 | G35: pause when the keys are lost. |
| BDP-12 | G80: one consistent controls list; bindings stay fixed. |
| BDP-13 | G80: phones see phone instructions. |
| BDP-14 | G81: a controller lifts the phone gate as the page says; moot if BDP-10 lands soon. |
| BDP-21 | G117: a landing shadow every player can see. Follows OCA-08's position (Next). |
| BDP-22 | G118: fewer sparks under reduced motion. Follows BDP-23 (Now). The default is already calm: N3b passes the top-out on the strict, pixel-by-pixel reading of WCAG 2.3.1 for every player, so reduced motion goes further for those who ask rather than being what makes the demo safe. |
| BDP-20 | G86: the phone fallback plays footage without asking. In the owner's PersonalWebsite repository. |
| BDP-18 | G114: repeated plays; measure before fixing. |
| BDP-09 | G34: the host page fits a phone held upright; needed before BDP-10. |
| SM-04 | G63: a local run procedure for the frame bench. Promoted to Should because SM-07 depends on it. |
| SM-05 | G62: the lock verdict on a non-NVIDIA run. Promoted to Should because SM-07 depends on it (critique). |
| SM-02 | G25: what the bench's fan-out measures; PHILOSOPHY amendment E at :691-693, after SM-03's amendment D (Next). Promoted to Should (critique). |
| SM-07 | G24 (major): the low-tier p99 the performance identity rests on. Also follows SM-01 and SM-03 (Next); owner-run on the iGPU. |
| SM-40 | G125: a headless test of the XInput translation, a regression guard needing no hardware. |
| SM-08 | G27: a method for browser pacing (R5) and the phone top-out frame (R6). Promoted to Should because SM-13 depends on it. Procedure, reducer and development-only fixture/export source implemented; generated-output and browser acceptance remain in [the method](../port/pacing-method.md). |
| SM-14 | G71: serve a development page to a phone. Out of deferred: the owner has an iPhone; written and tried in SM-13's session. |
| SM-13 | G34, G05 (merged with BDP-08): a real iPhone run. Split from the Android half (SM-44, deferred) per the owner facts. |
| BDP-07 | G23: pacing on the displays visitors use; may need a borrowed high-refresh display. |
| OCA-46 | G123: a join screen for couch play, as a sample piece first. |
| OCA-51 | G128: a settings screen wired to what the engine can set. Follows OCA-53 (Next). |
| OCA-50 | G127: a menu that works with a click or a tap, in game code. An engine hit test over `bounds()` was weighed on 2026-10-24 and declined: the one caller, LineSweeper's pause menu (`tapped_row`), sizes each tap target from the row's centre and the row spacing rather than its bounds, because a finger needs more than the text's extent. The target is policy, so the hit test stays the game's (T1; PHILOSOPHY, UI). |
| OCA-76 | G108: the engine half of OCA-53, split out on 2026-10-24. ColourWars is the recorded caller (`game/directory_consts.cpp` reads `%LOCALAPPDATA%` and falls back beside the executable), Windows-only; it waits for a second caller, or a browser one, before it is built. |
| OCA-41 | G145: non-English literals differ between Windows and the browser without /utf-8. |
| OCA-43 | G147: which languages the text path can draw. |
| OCA-42 | G146: a string table to the screen. |
| OCA-77 | G146, G147: word wrap to a measured width, weighed on 2026-10-24 after a review asked for wrap and alignment helpers. Wrap is cheap - `Font::measure` already lays out line feeds, so a wrap only chooses where to insert them - but where a line may break depends on the script, which OCA-43 settles, and no caller in either client wraps by width yet: the one wrap there is a debug console cutting at a column count on purpose. It follows OCA-43 and lands with or after OCA-42, whose translated strings are the first caller. An alignment helper was declined in the same weighing: centring is one line at each of the half-dozen sites that do it, and `renderer.h` already says to `std::floor` a centred point-sampled position. |
| OCA-40 | G131: what background music costs in the page download. Follows OCA-38 (Next). |
| OCA-18 | G109: debugging in Visual Studio from the presets. |
| OCA-32 | G48: the recording renderer in an adopter's tests. Follows OCA-29 (Next). |
| OCA-66 | G151: crispness on a scaled display; the default is undecided. |
| PI-10 | G74: an upstream answer for each filed report. Follows PI-09 (Next); the decisions are the owner's. |
| PI-19 | G74, G27: report 14, accepted narrowed and split out of PI-10 on 2026-10-22. Should, because SM-08 depends on it. |
| PI-11 | G14: the engine staging helper, only if PI-10 accepts the packaging request. Follows OCA-60 (Next). |
| PI-08 | G54: request status on the tracker. Follows OCA-72 (Now). |
| PI-15 | G78: hidden launch through STARTUPINFO tested upstream. |
| PI-16 | G79: drive a game past its title screen in a smoke run. Follows OCA-30 (Next). |
| PI-17 | G75: what JSON the engine reads for a game. |
| PI-18 | G76: the include wall says exactly what it protects. |
| PI-20 | Split out of report 12 on 2026-10-22 and argued in its decision record, which stands in for a STORIES.md entry: a `JsonValue` accessor for an object's member names, and unknown-key rejection in `asset_manifest_loader.cpp` and `sprite_sheet_loader.cpp`, whose optional keys (`optional`, `origin`, `rotated`) take their default when misspelt (T6). Decide first whether rejection is per loader or one shared helper, because that decides the accessor's shape. A silent behaviour change, so it needs a CHANGES.md entry. |
| SRL-09 | G94: backend rationale out of a game reader's way. Follows SRL-08 (Next). |
| SM-23 | G67: regenerated goldens leave a review trace. |
| SM-24 | G115: a written procedure for moving dependency and toolchain pins. Follows SM-25 (Next). |
| SM-17 | G30: one command per area for every configuration. |
| SM-18 | G31: a record of what CI cannot run, per merged change. |
| SM-15 | G72: watch the null-backend wasm step run after a failed page step. |
| SM-26 | G68: notice when the hosted demo lags the engine. Follows PI-05 (Next). |
| SM-29 | G135: site preview and rollback. Follows SM-31 (Now). |
| SM-30 | G136: the website gate in one place; dashboard settings are owner-only. |
| SM-32 | G137: the site's LineSweeper footage in step with the sample. |
| NMS-03 | G103: Platform support links the plans and what would change the answer. Follows OCA-01 (Now). |
| NMS-04 | G104: x64-* presets refuse a non-Windows host; wasm-* stays available and is stated as unverified (critique). |
| SRL-14 | G22: the C++ series' status, an owner decision; PHILOSOPHY amendment F only if withdrawn. |
| SRL-15 | G96: where to go without C++. |
| SRL-17 | G98: an interim teachability audit. |
| SRL-16 | G97: a glossary of the terms minimal uses. |
| SRL-18 | G88: run LineSweeper's rules tests knowing what is needed. |
| SRL-06 | G91: LineSweeper's design notes give reasons without internal records. |
| SRL-07 | G92: one constants rule matching the headers; amend or rename is the owner's choice. |

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
| [SM-44](STORIES.md#sm-44) Record a real Android phone run of the development page and the live demo | Needs a real Android phone, and the owner has none (owner facts 2026-10-13). Buying hardware is not proposed. | A borrowed Android phone; SM-14 written and SM-08 available, as SM-13 uses them |
| [BDP-10](STORIES.md#bdp-10) Lift the phone gate once real phones have played | Lifting the phone gate needs recorded passes on the two devices web.md:748-750 names. The iPhone half (SM-13) is reachable; the Android half (SM-44) is not. | SM-13, SM-44 and BDP-09; or SM-13, BDP-09 and the owner's recorded decision that one platform's evidence is enough |

## Risks to the plan

- Now holds 35 stories in 20 batches. It fits a few weeks only if each batch is one PR; split further and the branch, PR, green-CI, rebase-merge overhead crowds out the compounding items (OCA-68, OCA-11, OCA-73, SM-21). OCA-68's backfill (L) and OCA-11's consumer job (M) are the schedule risks inside Now.
- Owner review is the binding constraint, not agent time: every owner sign-off and repository setting in Now is made, amendment B has landed and `v0.1.0` is cut on it, and the hosted demo is redeployed and linked: Now is finished. Next's owner-gated batches (X3, X5, X6, X21, X25, X27) are where it binds now.
- PHILOSOPHY.md is touched by six PRs across three horizons. They are serialised A to F and each rebases on the last; a parallel branch that edits PHILOSOPHY outside that order will conflict and should be refused at review.
- The primary persona is unevidenced: no issues, analytics or independent adopter. Next spends real capacity on a stranger's ship path. Mitigation: most of Now is truth fixes that also serve the agents who read the same documents, and PI-09/PI-10 are the only stories backed by recorded demand.
- Doc edits can fail the website build by design: quoted lines and public header comments are read at build time. Header-contract stories (OCA-55, OCA-59, OCA-56) and the brief trim (SM-43) must fix any quoting page in the same commit.
- The consumer CI job adds build minutes and a vcpkg cold cache to every PR. If it is slow it will tempt someone to drop it; cache it deliberately and keep SM-21's documented contexts in place.
- Hardware-bound claims may stay open: the iGPU p99 needs AMD's driver first and may still need a code option if the per-app preference fails; the Big Sur MacBook Air's Safari may sit below the wasm-exceptions floor; the Android phone is not owned, so BDP-10 waits on a borrowed device or an owner exception.
- Windows Sandbox (OCA-61, PI-14) proves the loader and runtime requirements on a clean OS, but its GPU is virtual and Release has no WARP fallback, so the gameplay half may show only a loud refusal; G77's target-GPU half stays open until a real machine runs it.
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
