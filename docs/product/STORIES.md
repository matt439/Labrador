# User stories

160 stories, each written for one persona, checked by a second agent for INVEST and for fit with PHILOSOPHY, and then reconciled: stories that two personas wanted from the same change were merged into one, which lists the others under *also serves*. Taken on 2026-10-13 against `master` at `0977112`. Their order of work is [BACKLOG.md](BACKLOG.md); the gaps they close are in [GAP-REGISTER.md](GAP-REGISTER.md). Two were added after: PI-19, on 2026-10-22, split out of PI-10 when report 14 was accepted; and OCA-76, on 2026-10-24, split out of OCA-53 when its engine half was weighed.

Sizes are for one person working with agents: XS under an hour, S half a day, M one to two days, L a week. **Owner** marks a story an agent cannot finish alone: a decision, a repository setting, hardware in hand, or a deploy from a private repository.

## Index

| Persona | Stories |
|---|---|
| [The Outside C++ Adopter](#stories-outside-cpp-adopter) | 75 |
| [The Pinned Submodule Integrator](#stories-pinned-integrator) | 11 |
| [The Sole Owner-Maintainer](#stories-sole-maintainer) | 37 |
| [The Browser and Phone Demo Player](#stories-browser-demo-player) | 20 |
| [The Sample-Reading Learner](#stories-sample-reading-learner) | 13 |
| [The Full-Environment Seeker (anti-persona)](#stories-anti-full-environment-seeker) | 3 |
| [The Native Multi-Platform Shipper (anti-persona, for now)](#stories-anti-native-multiplatform-shipper) | 3 |

<a id="stories-outside-cpp-adopter"></a>
## The Outside C++ Adopter

<a id="oca-01"></a>
### OCA-01. One answer to 'where does it run?' on every public page

**As** a C++ developer deciding whether Labrador fits my game, **I want** every public page to give the same answer about which platforms Labrador runs on today and which are only planned, **so that** a quick skim and a full read lead me to the same decision.

S · docs · E01 Know what Labrador is, and whether it fits, before installing · gaps G02 · **Now**

*Also serves:* The Native Multi-Platform Shipper (anti-persona, for now) (merged from NMS-01)

**Acceptance**

- Given `git grep -n -i 'nearly everywhere' -- website/src README.md`, When it runs, Then it returns nothing
- Given why.mdx:24-26 and :191-198, index.mdx:29 and :137-139, and README.md:55, When they are read side by side, Then each names Windows plus a desktop browser (through WebGL2), none implies phones or native non-Windows platforms, and grepping website/src/content/docs and README.md for 'Windows today' and 'other than Windows' finds no hit that disagrees
- Given docs/website-plan.md:36, When the change lands, Then it is amended in the same commit to 'Windows and a desktop browser; Vulkan does not establish support for other operating systems'
- Given why.mdx's fit test (:24-26), When a reader who wants a browser build reads it, Then it no longer turns them away, and its 'Platform support' section says phones are a planned phase and the hosted demo shows them a poster until real phones have played it (web.md:738-752), worded to follow that gate until G34 closes
- Given the Safari wording, When it is written, Then it says neither 'supported' nor 'unchecked' as a final word: until SM-09 records the MacBook Air's Safari it says 'not yet checked', and SM-09's result replaces it with 'checked on Safari X' or 'out of reach on owned hardware'
- Given website.yml on the PR, When it runs, Then it is green (a page that quotes an edited line is fixed in the same commit)

**Trade-offs**

- T4: a skim and a full read give a cold reader the same answer
- PHILOSOPHY.md:398-404: Vulkan changes the cost of the cross-platform goal, not its status, so no public page may read reach as support

**Notes.** Merged from NMS-01 (same edit to the same pages, G02); NMS-05's why.mdx phone-and-poster sentence also lands here. Evidence: index.mdx:29, 137-139; why.mdx:24-26, 170-172, 191-198; README.md:55; web.md:738-752 (phones withheld), :1517 (Safari unchecked); website-plan.md:36. The same sentence serves the primary persona's fit decision and the anti-native-multiplatform-shipper's early exit. Owner facts 2026-10-13: an iPhone is owned, so 'not yet played on a real iPhone' is closable (SM-13); no Android phone is owned. Lands before the PHILOSOPHY amendment A batch, which edits why.mdx:24-26 again for FES-03. The original T6 citation was replaced: T6 governs run-time failure, not page wording (the same correction the t_ref check made to NMS-01).

<a id="oca-02"></a>
### OCA-02. The Why page points me at the split-screen sample that ships

**As** a developer planning a couch split-screen game, **I want** the Why page and the design documents to say that a public two-view sample exists and what it covers, **so that** I answer my core fit question correctly instead of reading that multiplayer is only 'planned'.

S · docs · E01 Know what Labrador is, and whether it fits, before installing · gaps G03 · **Now**

**Acceptance**

- Given website/src/content/docs/why.mdx, When I grep for 'planned' and 'second view', Then no sentence says no sample draws a second view, and the page links guides/local-multiplayer and names samples/local_multiplayer
- Given the page, When I read the multiplayer paragraph, Then it states the sample covers two players on fixed slots (samples/local_multiplayer/arena.h:16, player_count = 2) and that joining and more players are the game's
- Given OCA-05's PHILOSOPHY.md:663 amendment, When this story lands, Then it lands in that commit or after it, so the Why page and the design document agree

**Trade-offs**

- T1: PHILOSOPHY.md:661-667 keeps the view axis because it was extracted from a four-pane client, and it explicitly refuses 'no caller in this tree' as an argument; only the factual premise at :663 ('Neither sample in this repository has a second view') is now false because samples/local_multiplayer ships, so that sentence is amended against the shipping sample in the same commit and T1's conclusion stands unchanged

**Notes.** Evidence: why.mdx:98-101 ('Neither sample in the repository draws a second view yet ... A public multiplayer example is planned'); examples.mdx:110; concepts/drawing.mdx:89. Its former PHILOSOPHY.md:663 criterion moved to OCA-05 (the critique restates it as a factual amendment, not a re-argued T1). Online play stays out.

<a id="oca-03"></a>
### OCA-03. Try LineSweeper in my browser from the site and the README

**As** an evaluator without a Windows toolchain installed, **I want** a link to the playable LineSweeper page from the site's home or examples page and from the README, **so that** I can see a Labrador game running before I install anything.

S · content · E01 Know what Labrador is, and whether it fits, before installing · gaps G04 · **Now**

*Also serves:* The Browser and Phone Demo Player, The Sample-Reading Learner (merged from BDP-01, SRL-01)

**Acceptance**

- Given the built website (npm ci && npm run build in website/), When dist/index.html and dist/examples/index.html are searched, Then each contains an anchor to https://mattharding.dev/demo/linesweeper/ whose link text says the game plays in a browser and does not promise touch play (until BDP-10), with a note that phones get a poster (web.md:758)
- Given examples.mdx '## LineSweeper: the one you read' (:50), When I read it, Then the demo link comes before the numbered reading order (:76-82)
- Given README.md, When grepped for 'mattharding.dev/demo/linesweeper', Then a match is in the section that introduces LineSweeper; and samples/linesweeper/README.md has a match within its first 20 lines
- Given the linked URL, When curl -I is run on the day of the change, Then it answers HTTP 200, and the commit message or website-plan.md records that date; on a desktop browser the game starts after a key or click
- Given docs/website-plan.md:90, When the change lands, Then its 'not yet deployed' wording is amended in the same commit
- Given BDP-23's recorded flash check, When this link is merged, Then that record already exists (the link widens the page's audience, so the check comes first)
- Given website.yml on the PR, When it runs, Then it passes

**Trade-offs**

- docs/website-plan.md:90: the demo is served by mattharding.dev, and linking to it from this site is a page edit; its 'not yet deployed' wording is amended in the same commit
- T1: linking an existing host adds no machinery; hosting on the engine's own site would reopen website-plan.md:90

**Notes.** Merged from BDP-01 and SRL-01: one link (G04), three placements, one commit. Depends on BDP-23 (critique: the flash check must precede the link that raises the live page's traffic) and on SM-12 (records say the demo is live). The demo URL answered HTTP 200 on 2026-10-13. Rewording a quoted line can fail the site build, so build the site before committing. The deploy record (G05) is SM-12's.

<a id="oca-04"></a>
### OCA-04. Watch the split-screen sample and the small samples run in a browser

**As** an evaluator whose game is couch split-screen, **I want** to try the two-view local_multiplayer sample in a browser page, **so that** I can judge the multi-view feature I care about without building on Windows first.

L · process · E01 Know what Labrador is, and whether it fits, before installing · gaps G45 · **Won't this time** · **Owner**

*Also serves:* The Sample-Reading Learner (merged from SRL-13)

**Acceptance**

- Given a web-dist bundle step generalised from samples/linesweeper/CMakeLists.txt's linesweeper_web_dist, When a wasm-release build runs, Then bundles exist for local_multiplayer and for the agreed small samples (proposed: minimal, collision and the components gallery; the set is negotiable)
- Given the owner's hosting at mattharding.dev, When each hosted page is requested, Then it answers HTTP 200 and runs in headless Chrome with no console errors; local_multiplayer draws two views and moves each player from its own keys
- Given website/src/content/docs/examples.mdx (:19 minimal, :101 collision, :112 components, and the local_multiplayer entry), When I read each section, Then it links that sample's hosted page and says which input it takes (the gallery takes keys and a pad, not a mouse; only minimal takes touch)
- Given docs/website-plan.md (around :90), When I read it, Then it records which samples are hosted, where and why, amended in the same change
- Given ctest under wasm-release with a Chromium at the commit the bundles were built from, When LocalMultiplayerPageTests, MinimalPageTests, CollisionPageTests and ComponentsPageTests run, Then they pass

**Trade-offs**

- T1: hosts existing CI-tested pages; no new engine work
- T1: generalise LineSweeper's web-dist bundle step only to the samples a reader is actually pointed at, as far as real demand goes and no further

**Notes.** Merged from SRL-13: the same bundle-generalisation change serves the adopter (seeing split screen) and the learner (watching the samples they read). The deploy is an owner-only action in the PersonalWebsite repository. Still Won't this time: it waits behind LineSweeper's deploy checks (SM-12), the bundle verify (SM-27, G144) and recorded demand. SRL-02's paragraph gains the hosted pages in this story's commit if it lands.

<a id="oca-05"></a>
### OCA-05. Design documents say nothing that is false even as target

**As** an evaluator reading PHILOSOPHY and ARCHITECTURE on the site, **I want** the design documents to stop listing action mapping as provided, placing minimal beside the paint-shooter, and miscounting presets, **so that** I am not misled about what the engine deliberately lacks.

S · docs · E01 Know what Labrador is, and whether it fits, before installing · gaps G44, G03 · **Now**

*Also serves:* The Sole Owner-Maintainer (merged from SM-37)

**Acceptance**

- Given docs/design/PHILOSOPHY.md, When I grep for 'action mapping', Then no line (currently the table row at :449) lists it as engine-provided, and any mention agrees with ARCHITECTURE.md:175-178
- Given PHILOSOPHY.md, When I grep for 'beside the paint-shooter', Then neither :4 nor :717 places samples/minimal beside it
- Given PHILOSOPHY.md:663, When I read it, Then only the sentence 'Neither sample in this repository has a second view' is replaced, by the fact that samples/local_multiplayer reaches the fan-out branch (play_state.cpp:21 passes thread_pool() and partitioner(); the browser build uses a serial pool, main_web.cpp:43-44), and the passage says this strengthens 'the mechanism stays'
- Given the passage around :661-667, When I read it after the change, Then its refusal of 'no caller in this tree' as an argument is kept word for word and T1's conclusion is not re-argued
- Given ARCHITECTURE.md's CMakePresets.json line (:135-136, 'ten in all'), When compared with CMakePresets.json, Then the count matches its 12 non-hidden configure presets, wasm-debug and wasm-release included
- Given the top-level build and website.yml, When they run on the PR, Then check_doc_citations and the site build both pass

**Trade-offs**

- T1: the action map is deliberately not built because no client has a rebinding screen, so a binding table would be the speculative framework T1's 'Not a licence for' clause rules out (ARCHITECTURE.md:175-178); the boundary table row at PHILOSOPHY.md:449 contradicts that and is amended in the same commit
- T1 (PHILOSOPHY.md:662-668): the argument already refuses 'no caller in this tree' and stands; only the factual sentence at :663-664 is false, since local_multiplayer reaches the fan-out branch, so the amendment is factual and strengthens 'The mechanism stays'

**Notes.** Merged from SM-37 (OCA-05's three corrections plus PHILOSOPHY.md:663). Per the critique, :663 is a factual amendment, not a re-argued T1: the 're-argue, not delete' and CLAUDE.md G9-analogy language is dropped and the story carries no owner decision. PHILOSOPHY amendment A: lands first of the PHILOSOPHY edits, in one PR with FES-03 (and NMS-02 if its PHILOSOPHY wording is chosen), before the owner-reviewed amendment B (OCA-67 + OCA-69). OCA-02 is not folded: it takes the why.mdx half and lands in this commit or after it. The site publishes these documents word for word. ARCHITECTURE preset line corrected from :132-133 to :135-136.

<a id="oca-06"></a>
### OCA-06. The GitHub front door leads me to the site, not a private game

**As** a developer who arrives on the GitHub repository first, **I want** the repository's About text, homepage link and README opening to describe Labrador and send me to the site, **so that** the first thing I read is something I can see and try.

XS · process · E01 Know what Labrador is, and whether it fits, before installing · gaps G55, G100 · **Now** · **Owner**

*Also serves:* The Full-Environment Seeker (anti-persona) (merged from FES-01)

**Acceptance**

- Given `gh api repos/matt439/Labrador`, When I read description and homepage, Then the description does not name the private paint-shooter as proof, homepage is the engine site's URL, and at least one topic is set (owner-only settings, recorded in the commit message)
- Given README.md's first screen (lines 1-30), When read, Then it describes Labrador before mentioning the private client, and contains one line, at most two, naming no editor, no scripting language, no hot reload, 2D only and local play only, linking /why/ and the home page's 'What Labrador is not'
- Given the new line, When read, Then it adds no disclaimer block above the build instructions (G100 scope check)
- Given website.yml and check_doc_citations, When CI runs on the PR, Then both pass

**Trade-offs**

- T10: the line names 'no scripting' as a decision, not a missing feature
- T12: names no hot reload and no reflection as the price already paid
- T1: the client-as-proof argument can stay in the README body

**Notes.** Merged from FES-01 (same front-door edit: GitHub About plus the README's first screen, G55 and G100). Kept in Now because it rides the README edit (critique). The About description, homepage and topics are owner-only settings changes. Evidence: README.md:7-16; README.md:1-52 has no exclusion list today.

<a id="oca-07"></a>
### OCA-07. My licence scanner sees MIT

**As** a developer whose company scans dependencies' licences, **I want** the LICENSE file to be a licence a scanner recognises as MIT, **so that** adopting Labrador does not raise an 'unknown licence' flag that contradicts the site.

XS · docs · E01 Know what Labrador is, and whether it fits, before installing · gaps G149 · **Now**

*Also serves:* The Pinned Submodule Integrator (merged from PI-13)

**Acceptance**

- Given LICENSE, When I read it, Then it contains no paragraph naming 'the paint-shooter's source' (LICENSE:22-28 is gone or moved to NOTICE)
- Given the pushed master, When `gh api repos/matt439/Labrador --jq .license.spdx_id` runs after GitHub re-detects, Then it prints MIT
- Given NOTICE, When I read it, Then every pointer the removed paragraph carried (third-party code, undistributed content) is still reachable from NOTICE itself and consistent with NOTICE:6-9

**Notes.** Merged from PI-13 (identical LICENSE edit, G149; registered in register-final.json, spot-checked at LICENSE:25). Ship-side consequence: ColourWars' cmake/release_package.cmake:43 copies external/labrador/LICENSE into its release ZIP, so the fix reaches that ZIP at its next pin bump (SM-36). No T1-T12 trade-off governs a licence-text fix. The only commit touching LICENSE is 2a75ae5.

<a id="oca-08"></a>
### OCA-08. Know what accessibility the engine gives my game and what is mine to do

**As** a developer who must meet a platform's or players' accessibility expectations, **I want** one public statement of what the engine provides (focus by brightness, a Tab-safe canvas, keyboard, mouse, touch and pads, fixed bindings by design) and what is the game's job, **so that** I can judge before adopting whether my game can meet those expectations.

S · docs · E01 Know what Labrador is, and whether it fits, before installing · gaps G119 · **Next** · **Owner**

*Also serves:* The Sole Owner-Maintainer (merged from SM-38)

**Acceptance**

- Given docs/design/PHILOSOPHY.md's UI section (:583-612), When I read it, Then it states what the engine provides (brightness-based focus, Tab kept for the browser, three input devices) against what a game decides, and that remappable controls stay known-absent (T1)
- Given that statement, When I read it, Then it names whether the hosted demo has a stated accessibility aim and who owns the colour-blind palette palette.h:11-13 anticipates
- Given website/src/content/docs, When I grep for 'accessib', Then one published page or section states the engine's provisions and the game's responsibilities and agrees with the PHILOSOPHY sentence
- Given check_doc_citations and website.yml, When they run, Then both pass

**Trade-offs**

- T1: mechanism in the engine, policy in the game; PHILOSOPHY.md:600 'Styling and content belong to the game'; no screen-reader layer proposed
- CLAUDE.md Known-absent: action mapping (remappable controls); PHILOSOPHY.md:449 says the opposite until OCA-05 lands, so this story depends on OCA-05

**Notes.** Merged from SM-38: OCA-08 already required the PHILOSOPHY sentence, and that sentence is SM-38's recorded position, so both close in one commit. The position is the owner's, so it needs an owner sign-off. It is PHILOSOPHY amendment C (Next), after amendments A and B. BDP-24 (the demo page's own note) and BDP-21 (shadow contrast) are not folded: they act on the position in other files and depend on this story. Evidence: dom_events.cpp:183-191 keeps Tab; palette.h:11-13.

<a id="oca-09"></a>
### OCA-09. Get Started tells me the truth about long paths

**As** an adopter who keeps projects in a deep folder, **I want** Get Started to say that git core.longpaths fixes the clone but not the build, and what to do instead, **so that** I do not follow the offered alternative into an unexplained compiler error.

S · docs · E02 From an empty Windows machine to my own project, built · gaps G06 · **Now**

**Acceptance**

- Given prerequisites.mdx and build-labrador.mdx, When I read the long-path passages, Then they say a short checkout folder (or a subst drive) is required for the build, not only core.longpaths
- Given troubleshooting.mdx, When I search for 'C1083' and 'CMAKE_OBJECT_PATH_MAX', Then each has an entry naming the cause and the fix
- Given a 120-plus-character parent folder with core.longpaths true, When I follow the page's stated route, Then the x64-debug build completes

**Trade-offs**

- T6: name the failure the reader will meet

**Notes.** Evidence: prerequisites.mdx:53-60; build-labrador.mdx:74-78. The walkthrough observed six CMAKE_OBJECT_PATH_MAX warnings and seven C1083 failures in tools/linesweeper_capture and samples/local_multiplayer, and 288/288 under subst. Alternative fix: shorter object paths for tests and tools, which is the maintainer's call.

<a id="oca-10"></a>
### OCA-10. The README's consumer section is complete and points at a real consumer

**As** an adopter who reads the README instead of the website, **I want** the README's 'Using Labrador in a project' to name everything a consumer needs and to point at a working consumer example, **so that** configure does not fail at find_package and I do not go looking for consumer setup in a sample that has none.

XS · docs · E02 From an empty Windows machine to my own project, built · gaps G07, G106 · **Now**

**Acceptance**

- Given README.md '## Using Labrador in a project', When I read it, Then it names the top-level vcpkg.json, a preset that chains the vcpkg toolchain, and fxc on PATH, or links the your-project page that does
- Given the same section, When I grep for 'samples/minimal is a working example', Then the sentence is gone and the pointer is to the website walkthrough or website/src/walkthrough/

**Trade-offs**

- T1: documents the existing path, adds no mechanism

**Notes.** Evidence: samples/minimal/CMakeLists.txt has no add_subdirectory, PROJECT_IS_TOP_LEVEL or target_include_directories; your-project.mdx:74-77.

<a id="oca-11"></a>
### OCA-11. The project shape Get Started teaches is built against every engine change

**As** an adopter following Get Started months after it was written, **I want** the consumer project the walkthrough describes to be built in CI against the current engine, **so that** a change that breaks consumers is caught before I meet it.

M · build-ci · E02 From an empty Windows machine to my own project, built · gaps G08 · **Now** · **Owner**

*Also serves:* The Pinned Submodule Integrator (merged from PI-06)

**Acceptance**

- Given .github/workflows/ci.yml, When a pull request runs, Then a job assembles the Get Started project from website/src/walkthrough exactly as your-project.mdx describes (engine at external/labrador, samples/minimal copied to game/, the page's include edits, the derived vcpkg.json) and builds its x64-debug preset, at least linking the game
- Given that job, When it sets up the consumer, Then the engine it consumes is the PR's own checked-out tree (as the submodule or source path), not a clone of master, and the job runs from a short workspace path so the long-path failure the walkthrough hit cannot occur
- Given the consumer build, When `ctest -N` runs in its build directory, Then it lists zero tests, holding the PROJECT_IS_TOP_LEVEL guard (CMakeLists.txt:42-47)
- Given a throwaway PR that uses CMAKE_SOURCE_DIR inside engine/ where CMAKE_CURRENT_SOURCE_DIR belongs, When CI runs once, Then the consumer job fails, and that run is linked from the PR that adds the job
- Given the job definition, When it is read, Then it fetches nothing from ColourWars or any other client repository
- Given website/src/walkthrough/README.md's four include edits and the page's five, When the job lands, Then they are reconciled, and Get Started's 'every command was run against the footer revision' claim says what CI checks and what was checked by hand, and when
- Given branch protection, When the job is green on master, Then its context is in the required list SM-21 documents

**Trade-offs**

- T5: a consumer break (CMAKE_SOURCE_DIR use, new required package, deleted public name the template calls) fails in the engine's own CI, the earliest stage that can catch it, rather than at a client's next pin move
- T1: uses the existing public walkthrough as the consumer; no new framework
- PHILOSOPHY.md:744-749 (The public face): the engine cannot reach a repository that consumes it and should not learn how, so the job builds the walkthrough against the PR's own engine tree and fetches nothing from a client

**Notes.** Merged from PI-06 (same CI job, G08). Depends on SM-21 (G134): adding the required-check context is a branch-protection setting only the owner can change. Cost: one more Windows runner job and a vcpkg cold cache; cache deliberately. minimal has no smoke flag yet, so this checks the build, not a run (OCA-30 adds the run in Next). Walkthrough files last touched 2026-10-04 (68b392d).

<a id="oca-12"></a>
### OCA-12. The first-build time and public-clone path are observed from a cold cache

**As** an adopter cloning Labrador for the first time, **I want** Get Started's first-configure time and its 'from the public repository' claim to come from a run with an empty vcpkg binary cache and a public clone, **so that** I know how long to wait and that no package fails to build from source.

S · verification-hardware · E02 From an empty Windows machine to my own project, built · gaps G61 · **Next**

**Acceptance**

- Given a run with VCPKG_DEFAULT_BINARY_CACHE pointed at an empty folder and a clone from the public GitHub URL, When it configures and builds x64-debug, Then the wall time and any failures are recorded in website/README.md or the walkthrough README with the date, machine and commit
- Given build-labrador.mdx:30-32, When I read it, Then its time statement agrees with that record

**Trade-offs**

- T6: a stated cost the reader can plan for

**Notes.** The walkthrough restored 7 packages from a local cache in 129 ms and cloned from a local path. If OCA-11's CI job runs with a cold cache, its log can supply the record instead.

<a id="oca-13"></a>
### OCA-13. The template's comments describe public code and the current content rule

**As** an adopter reading the comments in the files I copied, **I want** the template's comments to point at code I can open and to state how content paths resolve today, **so that** I neither look for files in a private repository nor learn the wrong rule for launchers and shortcuts.

XS · code · E02 From an empty Windows machine to my own project, built · gaps G40, G42 · **Now**

**Acceptance**

- Given samples/minimal, When I grep for 'game/objects', 'paint-shooter', 'paint tiles' and 'game/', Then no comment points at ColourWars code; each points at a public sample or states the rationale alone
- Given samples/minimal/CMakeLists.txt, When I read the content comment (currently :47, 'resolved relative to the working directory'), Then it says relative content resolves against the executable's directory
- Given the website build, When it runs on that commit, Then it passes with any moved quote anchors fixed

**Trade-offs**

- T4: the next reader is a stranger seeing the code cold, who cannot open a pointer into private ColourWars code; the 'contract or rationale, never history' rule the entry quotes is CONVENTIONS', not T4's

**Notes.** Evidence: samples/minimal/states/hello_state.cpp (game/objects/level.cpp, 'paint tiles' near :214); samples/minimal/CMakeLists.txt:9-12, 47-48 (checked). Since 92966b0 the content root is the exe's directory. why.mdx embeds this CMakeLists in full.

<a id="oca-14"></a>
### OCA-14. The 'dozen lines of CMake' claim matches the file and says what shipping adds

**As** an evaluator weighing the build cost, **I want** the claim about a game's build file to match the file the site shows and to say that a release package needs more, **so that** I do not discover the shipping cost only after adopting.

XS · docs · E02 From an empty Windows machine to my own project, built · gaps G41 · **Now**

**Acceptance**

- Given index.mdx (near :76), samples/minimal/CMakeLists.txt and ARCHITECTURE.md (near :566), When I grep for 'dozen', Then each remaining claim agrees with the embedded file's actual length (64 lines today) or is reworded
- Given the reworded claim, When I read it, Then it states that it covers building and dev-loop content, and links the release-package page once OCA-60 exists

**Trade-offs**

- T3: reword rather than build a helper

**Notes.** The embedded file is 64 lines with an Emscripten branch. ARCHITECTURE changes by amendment.

<a id="oca-15"></a>
### OCA-15. The template folder tells me how to copy it and what changed

**As** an adopter who arrives by clone or submodule rather than the website, **I want** a README in samples/minimal that says how to turn it into my project and lists notable changes to the template, **so that** I can copy it correctly and see when my copy has fallen behind.

S · docs · E02 From an empty Windows machine to my own project, built · gaps G43 · **Now**

**Acceptance**

- Given samples/minimal/README.md, When I open it, Then it links the your-project page for include rewrites, the consumer vcpkg.json and presets
- Given the README, When I read it, Then it has a dated list of template changes a copier would want, with commit ids, and says the template is copied and owned, with no compatibility promise (PHILOSOPHY.md:720-727)
- Given the website build and the doc-citation check, When they run on that commit, Then both are green

**Trade-offs**

- T4: point at the website page rather than duplicate it; T4's 'Not a licence for' clause says duplication is the opposite of clarity at scale

**Notes.** The template changed eight times, three since 2026-09-17. The cost is the habit of updating the change list in the same commit as a template change. Overlaps OCA-68's changes file; one may point at the other.

<a id="oca-16"></a>
### OCA-16. First-change sends me on to the guides

**As** an adopter who has just built and changed my copy, **I want** first-change's 'Where to go next' to link the guides, **so that** I find the pages for growing a game without hunting the sidebar.

XS · docs · E02 From an empty Windows machine to my own project, built · gaps G56 · **Now**

**Acceptance**

- Given website/src/content/docs/docs/get-started/first-change.mdx, When I read 'Where to go next', Then it links /docs/guides/
- Given the built page, When I click that link, Then the guides index opens

**Notes.** One link.

<a id="oca-17"></a>
### OCA-17. Prerequisite and naming instructions that work as written

**As** an adopter following Get Started on a current Visual Studio, **I want** the VCPKG_ROOT fallback to show a path that exists on any supported Visual Studio, and the 'use your own name' step to warn about vcpkg's name rule, **so that** copying an example path or my project name does not break configure.

XS · docs · E02 From an empty Windows machine to my own project, built · gaps G58, G60 · **Now**

**Acceptance**

- Given prerequisites.mdx (near :43), When I read the VCPKG_ROOT fallback, Then it shows a path pattern valid for both Visual Studio 2022 and Visual Studio 18, or tells me how to find it
- Given your-project.mdx (near :18), When I read the rename step, Then it says the vcpkg.json 'name' takes only lowercase letters, digits and hyphens

**Trade-offs**

- T6: name the rule before the failure

**Notes.** G60's failure was not exercised; the rule comes from vcpkg's documented manifest name grammar. Confirm it once with an uppercase name before publishing.

<a id="oca-18"></a>
### OCA-18. Debug my game in Visual Studio from the presets

**As** an adopter told my debugger works on my game as on any C++ program, **I want** a short page on opening my project in Visual Studio, choosing a preset and startup item, where content resolves when launched there, getting the Direct3D debug layer, and keeping PDBs for a shipped build, **so that** I can step through my game and diagnose a crash without guessing.

S · docs · E02 From an empty Windows machine to my own project, built · gaps G109 · **Later**

**Acceptance**

- Given the Get Started section, When I search for 'Visual Studio' and 'breakpoint', Then one page covers open-folder with CMakePresets.json, picking a startup item, and the content root under the IDE
- Given that page, When I read it, Then it names the Windows 'Graphics Tools' optional feature for debug layers and says to keep the release PDBs
- Given a clean copy of website/src/walkthrough, When the page's steps are followed once, Then a breakpoint in the copied hello_state hits and the game finds its content, and the run is recorded with date and Visual Studio version

**Trade-offs**

- T12: why.mdx:36-42 rests 'your debugger, profiler and editor work on your game' on T12 and T10 - an ordinary C++ program with no project format - so the page shows it with stock Visual Studio and adds no checked-in IDE config

**Notes.** Evidence: why.mdx:41-42, 36 ('no project format'); engine/render/d3d12/device_resources.cpp:104. The page is prose, not checked-in IDE config.

<a id="oca-19"></a>
### OCA-19. Know what to do with the browser files the template copy brings

**As** an adopter who copied samples/minimal by the your-project steps, **I want** the page to say that main_web.cpp and web/shell.html are copied but not built in the Windows-only walkthrough, and whether to delete or keep them, **so that** I do not edit and commit dead files without knowing why.

XS · docs · E08 Ship my game to a web page · gaps G39 · **Next**

**Acceptance**

- Given your-project.mdx, When I read the copy step and FileTree, Then they either show main_web.cpp and web/shell.html with a note that they are for a browser build, or tell me to delete them
- Given the include-change table, When I read it, Then it does not ask me to edit main_web.cpp without saying it does not compile in this setup

**Trade-offs**

- T3: say it rather than build it; restoring the build is OCA-20

**Notes.** Observed in walkthrough commit a18f4cc; your-project.mdx:60-62. If OCA-20 lands first, this story becomes 'link to the browser page'.

<a id="oca-20"></a>
### OCA-20. Add a browser build to my own project

**As** an adopter who wants the same game on a web page, **I want** a guide that takes my copied project from the Windows walkthrough to a wasm preset, an EMSCRIPTEN branch and a shell page that builds and loads, **so that** I do not have to reverse-engineer minimal's CMake branch and the engine's wasm-base preset.

M · docs · E08 Ship my game to a web page · gaps G09 · **Next**

**Acceptance**

- Given the site, When I search for 'Emscripten' under docs/get-started or guides, Then one page shows the consumer's wasm preset (toolchain chained, triplet), the EMSCRIPTEN branch of game/CMakeLists.txt and the shell page, and needs no LABRADOR_RENDER_BACKEND workaround because OCA-22 makes gl the browser default
- Given website/src/walkthrough with the page's wasm preset, When I configure and build it with EMSDK set, Then a .html is produced that, served over HTTP, draws the template's frame in headless Chrome with no console error
- Given the page, When I read it, Then it names what is still unverified for browsers (links OCA-28's statement)

**Trade-offs**

- T5: the backend stays a configure-time choice in the preset
- T3: copy minimal's shape rather than add a helper

**Notes.** Evidence: README.md:180-182; samples/minimal/CMakeLists.txt EMSCRIPTEN branch; website/src/walkthrough has x64 presets only. Depends on OCA-22 (critique: fixing the d3d11 default first removes a workaround this page would otherwise document), OCA-21 (setup) and OCA-11. Scope (a full walkthrough while M3/M5 are open) is the owner's call. OCA-31 can be the headless-Chrome check.

<a id="oca-21"></a>
### OCA-21. Install the Emscripten version the engine is built with

**As** an adopter starting a browser build, **I want** a public page that says to install Emscripten, which version CI pins, how to set EMSDK and how it combines with the vcvars shell, **so that** the wasm presets configure on my machine the first time.

S · docs · E08 Ship my game to a web page · gaps G46 · **Next**

**Acceptance**

- Given the site's prerequisites or a browser-build page, When I search for 'EMSDK', Then it shows the install commands and names the pinned version, matching the pin in .github/workflows/ci.yml
- Given those commands on a machine without emsdk, When I follow them and configure wasm-debug, Then configure succeeds
- Given prerequisites.mdx's 'two installs' sentence, When I read it, Then it says the browser build needs a third

**Trade-offs**

- T6: a missing EMSDK should be met with the page that names it

**Notes.** The version is in .github/workflows/ci.yml (emsdk pin, 6.0.12) and CLAUDE.md prose. A site build that reads the pin from ci.yml would avoid drift; optional.

<a id="oca-22"></a>
### OCA-22. A browser preset that omits the render backend gets GL or a named refusal at configure

**As** an adopter writing my own browser preset, **I want** configure to pick the GL backend under Emscripten, or refuse Direct3D and Vulkan there with a message, **so that** I get a loud, named failure instead of an obscure one later.

S · code · E08 Ship my game to a web page · gaps G50 · **Next**

**Acceptance**

- Given a wasm configure with LABRADOR_RENDER_BACKEND unset, When configure runs, Then it selects gl, or fails with a message naming the variable and gl
- Given a wasm configure with LABRADOR_RENDER_BACKEND=d3d11, When configure runs, Then it stops with a message, as audio does for xaudio2
- Given the existing presets, When CI runs, Then all jobs stay green

**Trade-offs**

- T5: the choice stays compile-time, and an impossible backend is caught at the earliest stage that can catch it - configure - rather than as a missing header or symbol later
- T6: refuse with a message naming the variable and gl, as audio already does for xaudio2 under Emscripten (engine/CMakeLists.txt:304-314); input only defaults by platform (:340-344) and has no such refusal, so render would match the audio axis, not both

**Notes.** engine/CMakeLists.txt:121 defaults the cache variable to d3d11 regardless of platform (checked); audio (:304-314) defaults by platform and refuses the wrong one, input (:340-344) only defaults. The exact current failure under wasm was not observed. Critique: moved to Next and ranked before OCA-20, so the adopter's browser page does not have to document a workaround for a default an S fix removes.

<a id="oca-23"></a>
### OCA-23. Know what my web host must send

**As** an adopter putting my game's page on my own host, **I want** a public page listing the headers and settings a host needs (wasm MIME type, no COOP/COEP needed, CSP 'wasm-unsafe-eval') and what happens if each is wrong, **so that** I can configure any host without reading an internal port plan.

S · docs · E08 Ship my game to a web page · gaps G51, G52 · **Next**

**Acceptance**

- Given the site, When I search for 'application/wasm', Then a published page lists the host requirements from web.md section 7.2
- Given that page, When I read the MIME entry, Then it says a wrong type falls back to slower instantiation with console errors rather than stopping the page, as observed once by serving with a wrong type
- Given docs/port/web.md section 7.2, When I read it, Then the 'cannot start without' claim is amended to match

**Trade-offs**

- T1: guidance, not a hosting service

**Notes.** G52 is folded in because the public page must state the corrected consequence; the internal amendment rides along.

<a id="oca-24"></a>
### OCA-24. Troubleshoot my browser build

**As** an adopter whose browser build or page misbehaves, **I want** troubleshooting entries for EMSDK unset, a page opened from file://, page tests silently absent without a Chromium, no WebGL2, and silent audio before the first key, **so that** each error I meet has a named cause and fix.

S · docs · E08 Ship my game to a web page · gaps G112 · **Later**

**Acceptance**

- Given website/src/content/docs/docs/troubleshooting.mdx, When I look for a browser section, Then it has an entry for each of the five cases with the message or symptom a reader sees, copied from a real run of each
- Given the ctest-count entry, When I read it, Then it says 17 entries instead of 27 means no Chromium was found and names LABRADOR_TEST_BROWSER

**Trade-offs**

- T6: a named failure is only loud if the reader can look it up

**Notes.** Messages should be copied from a real run of each case, not paraphrased. The silent-audio entry should link OCA-25's guide paragraph.

<a id="oca-25"></a>
### OCA-25. Know that a one-shot sound before the first gesture is dropped in a browser

**As** an adopter who plays a jingle on my title screen, **I want** the audio guide (and sound_bank.h) to say that a one-shot played before the page's first key, click or touch is dropped, a looping effect waits, and Escape does not count, **so that** I design my title screen to be heard in the browser too.

XS · docs · E08 Ship my game to a web page · gaps G132 · **Later**

**Acceptance**

- Given website/src/content/docs/docs/guides/audio.mdx, When I read the browser paragraph (near :29-34), Then it states the drop/wait rule for a game's own sounds and the Escape exception
- Given engine/audio/sound_bank.h, When I grep for 'gesture', Then a contract sentence is present
- Given the audio sample page in a browser, When a one-shot is played before any gesture, Then it is not heard later, observed once before publishing

**Trade-offs**

- T6 (Not a licence for, PHILOSOPHY.md:119-122): a browser withholding audio until a gesture is the world being the world, a state rather than an error, so the engine rightly does not throw, and the contract has to say so instead
- PHILOSOPHY.md:714-716 (The public face): every public header documents its contract, so sound_bank.h must state the drop/wait rule and the Escape exception

**Notes.** Evidence: engine/audio/audio_device.h:159-167; engine/audio/webaudio/audio_device.cpp:39-52. Header comments are published word for word.

<a id="oca-26"></a>
### OCA-26. One public statement of what my page owes the engine module

**As** an adopter writing the page that hosts my game, **I want** a single published description of Module.canvas and the four callbacks (onQuit, onError, onContextLost, onContextRestored), including that a page must not remove a running game's canvas, **so that** my page reports failures instead of showing a blank canvas, and does not leak a hidden running game.

S · docs · E08 Ship my game to a web page · gaps G142, G143 · **Later**

**Acceptance**

- Given the site, When I search for 'onContextLost' and 'Module.canvas', Then one page lists the canvas and all four callbacks with when each is called, and says callbacks are matched by exact name with no warning for a misspelling
- Given engine/app/window.h's published comment (currently :230-247, onQuit and onError only), When I read it, Then it names all four or links that page
- Given that page, When I read it, Then it says removing a running game's canvas leaves the frame loop, the WebGL2 context and window listeners running, and that the game must quit first

**Trade-offs**

- T6: a misspelled callback and a dropped canvas are silent failures today
- T1 (Not a licence for: speculative frameworks): a host-stop API waits for a client that demands it
- PHILOSOPHY.md:739-741 (The public face): no compatibility promise until releases are versioned (G17)

**Notes.** Evidence: engine/app/window.h:230-247; engine/render/gl/backend.h:192-198; engine/app/web/window.cpp:127-137, 411-415, 485-497, 800-805; web_context.cpp:32-42; engine/app/application.cpp:231-234; samples/linesweeper/web/index.html:22-27; docs/port/web.md:690-733. Merged from OCA-27. Optional follow-ups, not acceptance: a dev-build console warning for unknown Module.on* keys (T6), and a line in samples/minimal/main_web.cpp showing how to export a function that calls Application::quit. The leak is by reading and was not observed.

<a id="oca-28"></a>
### OCA-28. Know which browsers my players can be promised

**As** an adopter shipping my game to a web page, **I want** a public statement of which browsers and devices the browser build has been checked on and which it has not, **so that** I can state honest requirements on my own page.

S · docs · E08 Ship my game to a web page · gaps G23 · **Next**

**Acceptance**

- Given the site, When I search for 'Safari' and 'Firefox', Then one page lists each browser as checked (with how and when) or not yet checked
- Given the Safari entry, When read, Then it says 'checked on Safari X (macOS Y)' or 'out of reach on owned hardware', per SM-09's record, rather than 'unchecked'
- Given that list, When compared with docs/port/web.md section 9, Then they agree
- Given that page, When I read it, Then it does not imply any low-tier speed figure

**Trade-offs**

- T6: an unchecked browser stated is better than one discovered

**Notes.** The hardware checks themselves (Safari, physical pad, pacing, audible audio) are the maintainer's work under E11. Depends on SM-09 (critique: so the Safari line can be definite) and SM-10 (web.md's Firefox claim made to match what ran).

<a id="oca-29"></a>
### OCA-29. A test target for my game's rules from the start

**As** an adopter who chose Labrador for headless rules tests, **I want** the consumer template to come with a test target and a guide showing a rules library tested like LineSweeper's, **so that** my first test needs no reverse-engineering of tests/linesweeper/CMakeLists.txt.

M · code · E03 Prove my own game works: tests, smoke checks and CI · gaps G10 · **Next**

**Acceptance**

- Given website/src/walkthrough, When I configure and build it, Then it has enable_testing(), a rules static library, a doctest test target and a test preset
- Given that project, When I run ctest --preset for it, Then one example test runs and passes and no engine test appears
- Given the guides, When I search for 'ctest', Then a page explains the rules-library pattern for a consumer, including adding doctest to the consumer vcpkg.json

**Trade-offs**

- T3 / PHILOSOPHY.md:720-727 (The public face): adding doctest to the template weighed against the smallest thing that runs and is copied
- T1: no engine test mechanism; a rules library tested in game code is the game's pattern
- CMakeLists.txt:42-48 (not a trade-off): engine tests stay out of a consumer's ctest

**Notes.** why.mdx sells this pattern. website/src/components/ConsumerManifest.astro:17 strips doctest as TEST_ONLY (checked) and would need changing.

<a id="oca-30"></a>
### OCA-30. A start-up smoke check for my Windows game

**As** an adopter taking engine upgrades, **I want** the template to have a smoke flag that starts, draws a few frames and quits, registered as a test, **so that** a behaviour change like a moved content root fails my ctest instead of my first manual launch.

S · code · E03 Prove my own game works: tests, smoke checks and CI · gaps G11 · **Next**

**Acceptance**

- Given samples/minimal, When I run MinimalSample.exe --smoke-test (or the chosen flag), Then it exits 0 after a bounded number of frames
- Given the consumer walkthrough, When I run ctest, Then a smoke entry runs the game executable
- Given content that is missing, When the smoke runs, Then it exits non-zero with the file named

**Trade-offs**

- T6: load errors that name the file, now checked automatically
- T1: one flag in the template, not a test framework

**Notes.** Other samples already have --smoke-test (samples/audio). It needs a device, so a GPU-less CI runner may need a caveat. The page half is OCA-31.

<a id="oca-31"></a>
### OCA-31. Check my game's page loads and quits

**As** an adopter with a browser build, **I want** a documented way to run the engine's page runner (run_page.mjs) against my own built page, **so that** my page is checked the way the engine checks its samples.

S · docs · E03 Prove my own game works: tests, smoke checks and CI · gaps G11 · **Later**

**Acceptance**

- Given a public page or a README in tests/render/web, When I read it, Then it shows a command that runs run_page.mjs against an out-of-tree page and what pass and fail mean
- Given the minimal template's wasm page and that command, When I run it, Then it passes when the page quits
- Given the same page with a deliberate console.error added, When I run the command, Then it fails

**Trade-offs**

- T1 (Not a licence for: speculative frameworks): document a by-hand invocation rather than ship a consumer-facing runner
- CMakeLists.txt:148-150 (not a trade-off): 'the runner is this repository's, not the project's', a decision this story respects rather than reverses

**Notes.** The root CMakeLists.txt comment keeps the runner out of consumers' builds, and this story respects that. The minimal page already reaches Module.onQuit: MinimalPageTests passes only on onQuit (CMakeLists.txt:168), so the template needs no new way out.

<a id="oca-32"></a>
### OCA-32. Use the recording renderer in my own tests

**As** an adopter who wants GPU-less tests of what my game draws, **I want** a guide that says how a consumer configures LABRADOR_RENDER_BACKEND=null in a second preset and what it can assert on, **so that** the recording renderer the Why page sells is usable from my project.

S · docs · E03 Prove my own game works: tests, smoke checks and CI · gaps G48 · **Later**

**Acceptance**

- Given the site, When I search for 'LABRADOR_RENDER_BACKEND=null', Then a page shows a consumer null preset and says it is a second configure
- Given that page, When I read what to assert on, Then it either links a published recording API or states that the API is backend-internal and how a consumer may still use it, with the published.ts decision recorded

**Trade-offs**

- T5: backend selection stays configure-time
- PHILOSOPHY.md:762-767 (Tests and toolchain): a seam ships with its headless implementation, which is what makes the null backend a consumer's testing tool
- website/src/lib/published.ts 'Backend folders hold no public API' (not a trade-off): publishing recording.h would be an argued exception to it

**Notes.** Negotiable: if publishing the recording API is refused, the page must say so plainly rather than sell it, and why.mdx's claim should be narrowed in the same commit.

<a id="oca-33"></a>
### OCA-33. A CI workflow I can copy for my game's repository

**As** an adopter with Labrador as a submodule, **I want** an example GitHub Actions workflow that checks out submodules, sets up vcvars, VCPKG_ROOT and fxc, caches vcpkg and runs my ctest, **so that** my repository is checked on every push without solving each step myself.

M · docs · E03 Prove my own game works: tests, smoke checks and CI · gaps G110 · **Next**

**Acceptance**

- Given the site or website/src/walkthrough, When I look, Then an example workflow file exists with those steps
- Given that file copied into a test repository holding the walkthrough project, When pushed, Then the workflow runs green on windows-latest, with the run URL recorded
- Given the page, When I read it, Then it says the engine does not maintain a reusable action

**Trade-offs**

- T1 (Not a licence for: speculative frameworks): a documented example, not a maintained action

**Notes.** The engine's ci.yml solves each step but is shaped for its own matrix. Verifying it needs one throwaway repository run. OCA-11's job can share the steps.

<a id="oca-34"></a>
### OCA-34. Get the content tools and the commands to make sprites and fonts

**As** an adopter making my first sprite and font, **I want** the sprites guide to say where to get texconv and MakeSpriteFont and give a working command line for each, **so that** I can produce .dds and .spritefont files the engine reads.

S · docs · E04 Content, audio and text that work on both platforms · gaps G12 · **Now**

**Acceptance**

- Given guides/sprites-and-text.mdx, When I read it, Then it links the official source of texconv and MakeSpriteFont and gives one command for each
- Given those commands run on a PNG and a system font, When the outputs are loaded by the template, Then they draw

**Trade-offs**

- T1: documentation, not an asset pipeline
- PHILOSOPHY.md:791-792 (Non-goals, today): an editor or general tooling is a non-goal 'for now' (CLAUDE.md calls the editor permanently out; the two disagree)
- T9: texconv and MakeSpriteFont are bought tools at the format edge, so the guide points at them rather than replacing them

**Notes.** The audio guide already does this for XWBTool and is the model. The premultiplied-alpha half is OCA-35; the character-region option is OCA-42.

<a id="oca-35"></a>
### OCA-35. Know that translucent textures must be premultiplied

**As** an adopter converting a sprite with soft edges, **I want** the sprites guide to say the engine blends premultiplied and does not premultiply on load, and which texconv option does it, **so that** my first transparent sprite does not fringe without explanation.

XS · docs · E04 Content, audio and text that work on both platforms · gaps G12 · **Next**

**Acceptance**

- Given guides/sprites-and-text.mdx, When I search for 'premultiplied', Then the rule and the texconv -pmalpha option are stated
- Given troubleshooting.mdx, When I search for 'fringe' or 'halo', Then an entry points at that rule

**Trade-offs**

- T3: state the convention rather than convert on load

**Notes.** Split from OCA-34 because it is independently valuable and testable.

<a id="oca-36"></a>
### OCA-36. Copy a guide sample without broken paths

**As** an adopter who copies collision, components or local_multiplayer as a starting point, **I want** each sample to say what it borrows from other sample folders, and the template to ship the white texel the solid-rectangle recipe needs, **so that** my copy builds and my first solid rectangle draws.

S · content · E04 Content, audio and text that work on both platforms · gaps G13 · **Next**

**Acceptance**

- Given samples/collision, samples/components and samples/local_multiplayer, When I read each README or CMakeLists header, Then it lists every ../ path it embeds or compiles
- Given samples/minimal/content, When I list it, Then a textures/white.dds is present (today it holds only fonts/ and manifest.json), or the solid-rectangle recipe in sprites-and-text.mdx says where to get one
- Given CI, When it runs, Then all presets, including MinimalPageTests, stay green

**Trade-offs**

- T3: state the borrowing; the reuse was a deliberate size choice (local_multiplayer/CMakeLists.txt:50)

**Notes.** collision/CMakeLists.txt:29 and local_multiplayer/CMakeLists.txt:35 embed ../linesweeper/content/textures/white.dds (checked). local_multiplayer also compiles tools/linesweeper_capture/png_file.cpp. Adding white.dds to minimal enlarges its embedded wasm content by one tiny file.

<a id="oca-37"></a>
### OCA-37. Know whether I may ship the template font, before I copy it

**As** an adopter about to ship a game built from the template, **I want** a clear statement of whether the Courier New atlas may go in my game and the steps to build an atlas from a font I hold rights to, **so that** I do not redistribute a typeface without knowing its terms.

XS · content · E04 Content, audio and text that work on both platforms · gaps G107 · **Now**

**Acceptance**

- Given samples/minimal (README or content/fonts) and guides/sprites-and-text.mdx, When I search for 'licence' near the font, Then a statement says the atlas courier_new_bold_16.spritefont is built from Courier New, that its redistribution terms are not Labrador's to grant (NOTICE:154-160) and are the adopter's to check, and links NOTICE
- Given that statement, When I read it, Then it says how to replace the atlas before shipping, with OCA-34's MakeSpriteFont command, and that the hosted LineSweeper wasm embeds the same atlas
- Given website.yml, When it runs, Then it passes

**Trade-offs**

- T6: a licensing problem discovered at ship time is the quiet, expensive failure, doubly so for a user who did not write the engine; state it up front
- PHILOSOPHY.md:714-716 (The public face): what the template ships, its font included, is documented for a stranger

**Notes.** Critique split: the disclosure moves to Now's sprites-and-fonts batch beside OCA-34, because every adopter who copies samples/minimal today copies this atlas; replacing it with an openly licensed font is OCA-75 in Next. The atlas is embedded by collision, components, local_multiplayer, audio and the hosted LineSweeper wasm.

<a id="oca-38"></a>
### OCA-38. The guide's XWBTool command makes a bank that loads in the browser too

**As** an adopter packing sounds by the audio guide, **I want** the guide's xwbtool command to include -nc and say why, **so that** my bank does not work on Windows and then fail on my first browser build.

XS · docs · E04 Content, audio and text that work on both platforms · gaps G129 · **Next**

**Acceptance**

- Given website/src/content/docs/docs/guides/audio.mdx (near :135), When I read the command, Then it includes -nc and a sentence on compact entries
- Given the guide's exact command run with XWBTool on three WAVs, When the resulting bank is loaded by the engine's own reader (engine/audio/xwb_file.h) in a scratch program or a wasm build, Then it loads
- Given the same command without -nc, When the bank is loaded by that reader, Then it is refused with an error that names compact entries

**Trade-offs**

- T3: one flag; making the reader accept compact banks would need a T1 argument
- T6: the existing throw already names the refusal

**Notes.** Evidence: samples/audio/README.md:118-124; engine/audio/xwb_file.h:75-82; xwb_file.cpp:406-409. XWBTool was not run in the review; this story's acceptance runs it.

<a id="oca-39"></a>
### OCA-39. Export my sounds in a format the engine accepts on both platforms

**As** an adopter exporting WAVs from an audio tool, **I want** a short 'export as' paragraph naming the accepted formats (PCM 8/16-bit or MS-ADPCM, at most two channels, sample rates a browser accepts) and what each costs in memory in a browser, **so that** I learn the rules before the load errors.

S · docs · E04 Content, audio and text that work on both platforms · gaps G130 · **Next**

**Acceptance**

- Given guides/audio.mdx, When I search for 'ADPCM', Then the accepted formats, the refused ones (xWMA, XMA) and the channel limit are listed
- Given the same paragraph, When I read it, Then it says ADPCM decodes to about four times its size and the browser holds each wave as a float buffer
- Given the sample's format (mono 16-bit PCM at 22,050 Hz), When the paragraph recommends a default, Then it is that or justified
- Given a bank with a sample rate AudioBuffer refuses, When loaded once in a browser before publishing, Then the stated failure matches what is observed

**Trade-offs**

- T6: documents what the reader already enforces

**Notes.** Evidence: engine/audio/xwb_file.h:66-84; xwb_file.cpp:223-305; webaudio/audio_device.cpp:23-28.

<a id="oca-40"></a>
### OCA-40. Know what background music costs and how to keep it out of the page download

**As** an adopter adding a multi-minute music track, **I want** the audio guide to say banks are held in memory, the template embeds content in the browser module, and when to switch to --preload-file, **so that** I do not ship megabytes of embedded audio to constrained players unknowingly.

S · docs · E04 Content, audio and text that work on both platforms · gaps G131 · **Later**

**Acceptance**

- Given guides/audio.mdx, When I search for 'music', Then a section states the in-memory cost on both platforms and the embed-vs-preload choice with the threshold web.md uses
- Given that section, When I read it, Then it says streaming banks are out of scope and why, citing T3

**Trade-offs**

- T3: web.md:487-489 declines an asynchronous loader
- T1: a streaming loader needs a demand argument

**Notes.** Evidence: samples/minimal/CMakeLists.txt:31-32; docs/port/web.md:481-489; samples/audio/CMakeLists.txt:19-20.

<a id="oca-41"></a>
### OCA-41. My non-English literals draw the same on Windows and in the browser

**As** an adopter writing L"café" in my source, **I want** MSVC to read my source as UTF-8 the way clang does, **so that** localised text is not silently mangled on Windows only.

S · code · E04 Content, audio and text that work on both platforms · gaps G145 · **Later**

**Acceptance**

- Given cmake/settings.cmake, When I read labrador_settings' MSVC options, Then /utf-8 is present with a comment saying why
- Given a checked-in test case (for example beside the text_encoding tests) whose source holds L"café あ" as UTF-8 without a BOM, When it runs under x64-debug and under a wasm preset, Then it asserts the code units 0063 0061 0066 00E9 0020 3042 and passes on both
- Given CI, When all presets build, Then they stay green with zero warnings

**Trade-offs**

- T6: the same source must give the same text
- T1: one compile option, no mechanism

**Notes.** Measured in the scratchpad (u8test): without /utf-8 under ACP 1252 the literal compiled silently under /WX as mojibake. Every engine .cpp/.h is ASCII, so the engine build is unchanged. /utf-8 also sets the execution character set, so adopters' narrow literals become UTF-8 too; say so in the comment. The test puts the tree's first non-ASCII source byte under tests/; the owner decides whether that is acceptable, and the fallback is a recorded scratch-consumer check.

<a id="oca-42"></a>
### OCA-42. Get my game's language from a string table to the screen

**As** an adopter whose game is not in English, **I want** a guide that walks from a UTF-8 string table (or keyboard.typed()) through widen() and an atlas with the right character regions to can_render checks at load, **so that** my players see my text, not question marks.

M · docs · E04 Content, audio and text that work on both platforms · gaps G146 · **Later**

**Acceptance**

- Given guides/sprites-and-text.mdx or a new section, When I search for 'widen' and 'can_render', Then both are shown in one worked path with read_json_file
- Given that page, When I read the atlas part, Then it says the template atlas holds U+0020-U+007E only, gives the MakeSpriteFont character-region option, and says one atlas is one pixel size
- Given input.mdx's typed() passage, When I read it, Then it links that section
- Given the guide's code, When compiled once in a scratch consumer with a non-ASCII string table and a matching atlas, Then the text draws and can_render reports a missing glyph for a string outside the atlas

**Trade-offs**

- T7: text in data, as PHILOSOPHY.md:601-602 already places it
- T1: no string-table facility proposed

**Notes.** Evidence: engine/render/text_encoding.h:17-29; engine/render/render_resources.h:189-193; samples/minimal/content/fonts/courier_new_bold_16.spritefont (95 glyphs). Depends on OCA-41 for Windows literals and OCA-34 for the MakeSpriteFont command.

<a id="oca-43"></a>
### OCA-43. Know which languages the text path can draw

**As** an adopter planning a localised release, **I want** a public statement of what the glyph walk does and does not do (no kerning, shaping, right-to-left, combining marks or surrogate pairs) and so which scripts are in reach, **so that** I know before adopting whether my target languages are possible.

S · docs · E04 Content, audio and text that work on both platforms · gaps G147 · **Later**

**Acceptance**

- Given the site, When I search for 'right-to-left' or 'shaping', Then a page states the boundary and lists scripts in reach given an atlas (Latin with precomposed accents, Cyrillic, Greek, CJK) and out of reach
- Given that page, When I read it, Then it says characters beyond the Basic Multilingual Plane are unsupported and why counts differ between Windows and the browser
- Given engine/render/font.h, When I read its published comment, Then it agrees

**Trade-offs**

- T3: the simpler model is chosen; state it
- T1: shaping and bidi would need a demand argument
- T6: an unsupported literal stated, not discovered

**Notes.** Evidence: engine/render/font.h:119-129, 147-190; tests/render/font_tests.cpp:309-345; text_encoding.cpp:54-69.

<a id="oca-44"></a>
### OCA-44. Know which controllers work on Windows and how slots are assigned

**As** an adopter planning couch play on players' own controllers, **I want** the input guide to say Windows reads pads through XInput, so only XInput-compatible controllers appear, and that a slot is the system's user index, which can change on replug, **so that** I can tell players which pads work and explain a moved slot.

XS · docs · E05 Input, menus, settings and couch play my players can use · gaps G121 · **Next**

**Acceptance**

- Given website/src/content/docs/docs/guides/input.mdx, When I grep for 'XInput', Then a paragraph states the API, the controller class it admits, and the slot rule
- Given troubleshooting.mdx's ignored-pad entry (near :89-94), When I read it, Then it mentions the XInput limit

**Trade-offs**

- T1: supporting other pad APIs would need its own reason; not proposed

**Notes.** Evidence: engine/input/gamepads.h:19-24 (checked: 'it is not stable across a replug'); engine/input/gamepad_reader.h:39-44, 55-62; xinput/gamepad_reader.cpp:3-5. No non-Xbox controller tested; the class limit is a property of the API.

<a id="oca-45"></a>
### OCA-45. A one-player game that accepts the pad in whatever slot it is in

**As** an adopter making a single-player game, **I want** the template and input guide to show how to accept a lone pad in any slot, **so that** a player whose pad sits in slot 1 is not ignored.

S · code · E05 Input, menus, settings and couch play my players can use · gaps G122 · **Next**

**Acceptance**

- Given guides/input.mdx, When I read the gamepad section, Then it shows a pattern that reads all four slots' edges or binds to the first slot that presses, and says why reading slot 0 alone fails
- Given samples/minimal/states/hello_state.cpp, When I read its pad handling (currently :136, 164), Then it uses that pattern, and the guide quotes it by anchor so the website build fails if the two drift
- Given the built template with two pads connected and the slot-0 pad then unplugged, When the remaining pad's stick and B are used, Then the marker moves and the quit dialog opens, recorded once with the date and commit

**Trade-offs**

- T1: a documented pattern in game code; an engine 'any pad' service would be speculative

**Notes.** Slot 0 is hard-coded at samples/minimal/states/hello_state.cpp:136, 164; LineSweeper play_state.cpp:259, 377, 401; gallery_state.cpp:166, 201, 222. Gamepads is built only over the real backend's GamepadReader (gamepads.h:31) and minimal has no test target, so a headless test is not available without new seams; the check is a recorded physical run. Needs two pads, or one pad plus a second controller to occupy slot 0. Not action mapping: bindings stay in game code.

<a id="oca-46"></a>
### OCA-46. See a join screen for couch play I can copy

**As** an adopter building drop-in couch multiplayer, **I want** a sample screen where players press Start to join, are bound to a slot, and each moves their own menu cursor, with a pad dropping mid-menu handled, **so that** I can read working code for the pieces the engine supplies instead of assembling them from prose.

M · code · E05 Input, menus, settings and couch play my players can use · gaps G123 · **Later**

**Acceptance**

- Given samples/local_multiplayer, When I grep for just_connected or just_disconnected, Then a join screen uses them
- Given that screen, When I read it, Then it maps an input source (pad slot or keyboard half) to a FocusGroup slot and gives each player their own DirectionRepeat
- Given the join rules written as a function over per-slot connected and pressed values (the way multiplayer::read_directions takes stick values), When LocalMultiplayerTests runs, Then cases drive a join, a per-player selection and a disconnect mid-menu and pass
- Given guides/menus.mdx, When I read the split-screen part (near :99-103), Then it links the screen

**Trade-offs**

- T1: joining and slot binding are game policy (local-multiplayer.mdx:56-57 says so), so they belong in a sample, not an engine service
- PHILOSOPHY.md:603-611 (UI, components): a compound more than one game wants is tried as a copied sample piece before anyone argues it into the engine

**Notes.** Evidence: engine/ui/focus.h:56-64, 125-143; engine/input/gamepad.h:113-118; tests/ui/focus_tests.cpp:263-310; samples/local_multiplayer/controls.h (input passed as values, so it is testable without a pad). Can be shaped to also host OCA-47's layouts. Local only; online play stays out. Look at the screen on a desktop build once, since the drawing is not asserted.

<a id="oca-47"></a>
### OCA-47. See three- and four-player layouts work

**As** an adopter whose couch game seats up to four, **I want** the local_multiplayer sample and guide to show three- and four-pane layouts, what fills the spare quadrant, and view capacity beyond two, **so that** I can see my four-player case work without inferring it from viewport_manager.h.

M · code · E05 Input, menus, settings and couch play my players can use · gaps G113 · **Won't this time**

**Acceptance**

- Given samples/local_multiplayer, When I switch its player count (a key or a flag), Then it draws three_player and four_player layouts
- Given LocalMultiplayerTests, When it runs, Then the pane rectangles for three and four players are asserted
- Given guides/local-multiplayer.mdx, When I read it, Then it shows the layout calls and says what the spare quadrant holds

**Trade-offs**

- PHILOSOPHY.md:662-667: the multi-pane axis was extracted from a four-pane client, and 'no caller in this tree' is refused as an argument, so this story shows the layouts rather than justifies them
- PHILOSOPHY.md:720-727 (The public face): a sample is read to learn what a finished game on the engine looks like

**Notes.** Fixed slots are acceptable here; joining is OCA-46. Arena::player_count is a constexpr 2 (arena.h:16), so the change reaches the arena's types. Captures or goldens that show the sample may need regenerating; look at every changed image.

<a id="oca-48"></a>
### OCA-48. One physical-pad session: trust the desktop pad mapping and know what a backgrounded game receives

**As** an adopter whose players use controllers on Windows, **I want** a recorded run of a desktop sample with a physical pad, covering buttons, stick direction and alt-tab, and a guide sentence on what a backgrounded game does with the pad, **so that** I know the pad path my players use has been seen to work.

S · verification-hardware · E05 Input, menus, settings and couch play my players can use · gaps G124, G125 · **Next** · **Owner**

*Also serves:* The Sole Owner-Maintainer (merged from SM-39)

**Acceptance**

- Given the owner's physical XInput pad and an x64-release sample, When it is played, alt-tabbed away from and the buttons pressed, Then a record in the tree (samples/local_multiplayer/README.md) names the pad model, the Windows build, the date, the commit, each button and stick direction checked and the result
- Given that record, When I read the alt-tab item, Then it says whether pad input reached the game while another window had focus
- Given engine/input/gamepad_reader.h:54-62, When the session is recorded, Then the 'not verified in this tree' disclaimer is replaced by the observed suspend behaviour, with no history in the comment
- Given suspend did not hold, When the result is recorded, Then a defect is opened against engine/input/xinput/ because the reader's own contract (gamepad_reader.h:50-52, 'A suspended reader answers disconnected for every slot') is broken, and guides/input.mdx's focus-loss note (near :44-45) states what reaches a game until it is fixed, without asking every game to write a gate
- Given suspend held, When guides/input.mdx's focus-loss note is read, Then it covers pads and says a backgrounded game receives neutral pad input

**Trade-offs**

- T6: replace the 'not verified in this tree' disclaimer (gamepad_reader.h:54-62) with an observed result
- T1: if suspend fails, the reader's own contract (gamepad_reader.h:50-52) is broken, and the fix is mechanism inside engine/input/xinput/, not a gate every game writes
- PHILOSOPHY.md:691-694: a path sized against hardware is not proven on it until a recorded run says so

**Notes.** Merged from SM-39 (same physical-pad session, the physical half of G124 and G125; both registered). Owner fact 2026-10-13: the owner has a physical XInput pad, so the 'blocked until a pad is to hand' wait is dropped and the story is in Next. The t_ref check produced two incompatible readings (OCA-48's 'a game-side gate on on_deactivated' against SM-39's 'the reader's contract is broken, fix it in xinput/'); SM-39's is taken because gamepad_reader.h:50-52 states the contract, and the acceptance was rewritten to match. The disclaimer's line reference was corrected from :124-132 (the file has 76 lines) to :54-62. SM-40 (headless XInput translation test) is not folded: it is a regression guard needing no hardware. BDP-06's browser-pad half can share the session.

<a id="oca-49"></a>
### OCA-49. The default disabled menu row looks unavailable

**As** an adopter using FocusGroup's default colours, **I want** a disabled row to look dimmer than an enabled unfocused one, as the menus guide says, **so that** my players do not read an unavailable choice as nearly selected.

XS · code · E05 Input, menus, settings and couch play my players can use · gaps G116 · **Next**

**Acceptance**

- Given engine/ui/focus.h FocusStyle, When I read the defaults, Then disabled is darker than unfocused, or the guide is corrected to match a deliberate choice with the reason
- Given guides/menus.mdx (near :56-57, 'dark grey for one that is disabled'), When I read it, Then it agrees with the defaults
- Given UiTests, When it runs, Then a case asserts the default ordering focused > unfocused > disabled in luminance

**Trade-offs**

- T1: no policy added; the default is a fallback, and a game supplies its own style
- PHILOSOPHY.md:600 (UI): styling belongs to the game, so the fix is to make the engine's default agree with its own guide, not a styling feature

**Notes.** FocusStyle defaults are white, gray and dark_gray (focus.h:20-28, checked); dark_gray is 0xa9 and gray is 0x80 (engine/render/colour.cpp:226, 255). Computed, not seen on screen; look at it once. ColourWars supplies its own disabled colour (game/presentation.h:46), so this change does not reach it.

<a id="oca-50"></a>
### OCA-50. Make a menu work with a click or a tap

**As** an adopter shipping to the browser, where players click settings screens, **I want** the menus guide to show how to choose a row by pointer, using the pattern LineSweeper's pause menu already uses, **so that** my menus work with a mouse or a finger as well as a pad.

S · docs · E05 Input, menus, settings and couch play my players can use · gaps G127 · **Later**

**Acceptance**

- Given guides/menus.mdx, When I search for 'tapped_row', Then a section quotes the pause-menu hit test (samples/linesweeper/states/pause_state.cpp:221-247) by anchor and explains it
- Given that section, When I read it, Then it says engine/ui has no pointer entry point and that pointer menus are the game's to build, citing T1
- Given the website build, When the quoted lines change in pause_state.cpp, Then the build fails until the guide is fixed (the existing quote mechanism)

**Trade-offs**

- T1: an engine pointer-focus mechanism needs a demand argument from real clients; the guide does not
- PHILOSOPHY.md:585-588 (UI): the engine solves focus and navigation for controller games; pointer selection is game code over bounds(), as LineSweeper's pause menu does

**Notes.** PHILOSOPHY.md:585-588 frames UI for controller games. Optional follow-up, not acceptance: a slider value-from-x function in samples/components/slider.h with a ComponentsTests case, and click support in the gallery. Not rebinding; action mapping stays absent.

<a id="oca-51"></a>
### OCA-51. Wire a settings screen to what the engine can actually set

**As** an adopter building an options menu, **I want** the gallery or a guide to connect the window row to Application::set_fullscreen and the volume sliders to SoundBank volumes, with the window row offering only modes the engine has, **so that** my settings screen does what it says, including when the browser leaves full screen.

M · code · E05 Input, menus, settings and couch play my players can use · gaps G128 · **Later**

**Acceptance**

- Given samples/components/gallery_state.cpp, When I read the window row, Then it offers two modes, calls set_fullscreen, and re-syncs with set_index from fullscreen() each tick
- Given the effects slider, When it moves, Then the gallery's sound plays at the scaled volume, or a guide shows the scaling in game code
- Given ComponentsTests, When it runs, Then a case asserts the re-sync (the row follows a full-screen state it did not set, without firing its callback)
- Given the site, When I search for 'set_fullscreen', Then a guide shows it

**Trade-offs**

- T6: a settings row must not offer a mode the engine cannot set; a choice that silently does nothing is the quiet failure T6 rules out, so two modes, not three
- T1: no volume bus without a demand argument; scaling a volume is game code over SoundBank

**Notes.** Evidence: engine/app/application.h:182-197; engine/audio/sound_bank.h:125-136; samples/components/option_row.h:36-37 (set_index fires no callback). Persisting the values is OCA-53.

<a id="oca-52"></a>
### OCA-52. The menus guide routes me to the components I can copy

**As** an adopter reading the menus guide, **I want** the guide to link the components sample's option row and slider where it says a game writes its own widgets, **so that** I copy them instead of writing them from scratch.

XS · docs · E05 Input, menus, settings and couch play my players can use · gaps G111 · **Next**

**Acceptance**

- Given guides/menus.mdx (near :39-41), When I read the widgets sentence, Then it names and links samples/components and the examples page section

**Notes.** One link.

<a id="oca-53"></a>
### OCA-53. Keep my players' settings and saves on desktop and in the browser

**As** an adopter whose game has options and progress, **I want** a guide saying where a game should write settings and saves on Windows and how to make them survive a reload in a browser, **so that** my players' choices persist on both platforms.

M · docs · E05 Input, menus, settings and couch play my players can use · gaps G108 · **Next**

**Acceptance**

- Given the guides, When I search for 'save', Then a page shows writing JSON with the standard library to a per-user folder on Windows
- Given the same page, When I read the browser part, Then it shows a mount that persists (IDBFS or the current Emscripten equivalent) and the sync step
- Given that browser code built into a scratch page, When the page reloads in headless Chrome, Then the written value is read back

**Trade-offs**

- T1: an engine save facility needs a demand argument from real clients
- T12 (price, PHILOSOPHY.md:261-263): no reflection means serialisation is written by hand, so a save is game code writing JSON with the standard library
- ARCHITECTURE module table: assets only reads; writing settings and saves is the game's

**Notes.** In ARCHITECTURE, assets only reads, and the save file lives in the private client. LineSweeper's best score (G85) is separate.

<a id="oca-54"></a>
### OCA-54. Know which browser pad and touch paths have met real hardware

**As** an adopter reading the input guide, **I want** the guide to say the browser pad has only been driven by a scripted fake pad and touch only by emulation and injection, with phones gated out of the demo, **so that** I do not take those paths as verified on my players' devices.

S · docs · E05 Input, menus, settings and couch play my players can use · gaps G47, G46, G34 · **Now**

*Also serves:* The Native Multi-Platform Shipper (anti-persona, for now) (merged from NMS-05)

**Acceptance**

- Given website/src/content/docs/docs/guides/input.mdx:47-66 and :124-155, When read, Then each of the pad, touch and the phone (portrait) layout carries one sentence naming its verification state: the browser pad has met only a scripted fake, touch only Chrome emulation and Windows injection, and the portrait layout only emulation
- Given those sentences, When compared with docs/port/web.md section 9 (:1518, :1520) and CLAUDE.md's input paragraph, Then they agree, and they agree with OCA-28's browser list once it exists
- Given npm run build in website/, When it runs, Then it passes

**Trade-offs**

- PHILOSOPHY.md:684-694 (Performance): state what has been verified and what has only been sized or emulated, as the low-tier passage does, rather than letting a feature list imply hardware support
- T4: each device's verification state said plainly where a shipper reads the feature

**Notes.** Merged from NMS-05, a bundle: its input.mdx part is this story; its Emscripten criterion (EMSDK and the pinned 6.0.12, matching ci.yml:242-243) belongs to OCA-21, which already requires it; its why.mdx phone-and-poster sentence belongs to OCA-01's platform sweep. G34 and G46 are carried as disclosure only. Owner facts: an iPhone is owned, so 'not yet played on a real iPhone' is now a closable statement (SM-13); the Android half remains without owned hardware (SM-44). Closing the debt itself is the maintainer's hardware work.

<a id="oca-55"></a>
### OCA-55. Know that layer_depth orders nothing

**As** an adopter giving my background a lower depth than my player, **I want** the drawing contract to say that call order alone decides what is on top and layer_depth is ignored, **so that** I order my draws correctly instead of trusting a parameter that has no effect.

XS · docs · E06 Concepts and reference that tell the truth a caller needs · gaps G138 · **Now**

**Acceptance**

- Given engine/render/renderer.h above draw_sprite and draw_text, When I read the comment, Then it states depth is ignored and order decides
- Given engine/render/draw_object.h, When I read it, Then it says the same
- Given concepts/drawing.mdx (near :35-37), When I read it, Then it no longer implies depth orders draws
- Given the website build, When it runs, Then it is green

**Trade-offs**

- T6: a parameter with no effect must say so
- T1: a depth sort would be a feature to weigh, not proposed

**Notes.** Evidence: engine/render/sprite_vertex.h:22-31 (checked: 'layer_depth does not order draws, call order does'); d3d11/renderer.cpp:380-386; tests/render/pixel_tests.cpp:950. Header comments are published word for word.

<a id="oca-56"></a>
### OCA-56. Reference pages say how I get each type and what its parameters mean

**As** an adopter looking up a type without opening a sample, **I want** game-facing reference pages to say where the object comes from and what each constructor parameter means, **so that** the reference gives the contract it promises.

M · docs · E06 Concepts and reference that tell the truth a caller needs · gaps G139 · **Next**

*Also serves:* The Sample-Reading Learner (merged from SRL-10)

**Acceptance**

- Given engine/input/gamepads.h, keyboard.h, mouse.h and touch.h, When I read each class comment, Then it names the Application accessor a game uses (Application::gamepads(), application.h:243, and its keyboard, mouse and touch counterparts) and says the constructor and poll() belong to the shell
- Given engine/render/text.h and label.h, When I read them, Then they say font_name names a loaded font, where the RenderResources pointer comes from, that an unknown name throws std::out_of_range (render_resources.h:131-133), what position anchors, and that rotation is in radians
- Given engine/core/state.h and engine/render/camera.h, When I read them, Then State and init() (:34) say when init runs, and DEFAULT_CAMERA and the calculate_* members (:47, :82, :86) each carry a comment
- Given the new comments, When read, Then they state contract, not history, because headers publish word for word
- Given website.yml, When it runs, Then it passes and the published reference pages show the new text

**Trade-offs**

- PHILOSOPHY.md:714-716 (The public face): every public header states its contract and intent; a stranger starts from the headers, not the implementation
- T4: the reader should not need the sample to use the type
- T6: the unknown-font-name throw is part of the contract and is stated where a caller reads it

**Notes.** Merged from SRL-10 (same header comments, G139). Optional and not acceptance: a site-build count of uncommented public declarations to stop recurrence.

<a id="oca-57"></a>
### OCA-57. From a reference page, reach the guide that teaches the type

**As** an adopter who lands on a reference page from search, **I want** each reference page to list the guides and concept pages that mention its type, **so that** I can find the usage and pattern, not only the declarations.

M · code · E06 Concepts and reference that tell the truth a caller needs · gaps G140 · **Next**

*Also serves:* The Sample-Reading Learner (merged from SRL-11)

**Acceptance**

- Given the built site, When I open the reference pages for input/gamepads, scene/scene, render/label, render/camera, app/application and render/renderer, Then each shows an 'Explained in' list with at least one link under /docs/guides/ or /docs/concepts/ for every type a guide or concept page links
- Given a guide that stops mentioning a type, When the site builds, Then that type's list no longer shows the guide (generated in reference.ts layout() from referenceLinks, not hand-written)
- Given website/tests, When npm test runs, Then a case asserts the list for at least one known guide-to-type and one concept-to-type mention

**Trade-offs**

- T4: the reader moves from the exact contract to the explanation without searching for it
- T5: the site build computes the list from the pages' own links, so the build rather than a maintainer keeps it true

**Notes.** Merged from SRL-11 (same generated list, G140). The referenceLinks plugin in website/src/lib/markdown-plugins.ts already computes every guide mention; layout() in website/src/lib/reference.ts renders the page.

<a id="oca-58"></a>
### OCA-58. The Application and Colour reference pages are navigable and correctly named

**As** an adopter looking up max_threads or a named colour, **I want** every member to have its own correct heading and grouped constants to read as a list, **so that** I can find and link the setting I need.

S · code · E06 Concepts and reference that tell the truth a caller needs · gaps G141 · **Next**

*Also serves:* The Sample-Reading Learner (merged from SRL-12)

**Acceptance**

- Given dist/docs/reference/app/application, When I read its headings, Then '### max_threads' exists with an anchor and no heading names default_thread_count as a member
- Given website/tests/reference.test.mjs, When npm test runs, Then a case asserts that a declaration with an '= initialiser(...)' is named by its identifier
- Given dist/docs/reference/render/colour, When I measure every heading anchor, Then none exceeds 100 characters, and no single 'On this page' entry lists the roughly 140 named colours one by one

**Trade-offs**

- T5: a test case on the declaration parser catches a wrong member name before the site publishes it
- T4: headings name the member a reader is looking for and stay readable

**Notes.** Merged from SRL-12 (same declarationName fix in website/src/lib/cpp-header.ts:137-153 and the same Colour grouping, G141). Anchor cap reconciled at 100 characters (OCA-58's figure, the stricter of the two). SRL-12's Renderer::impl() criterion (renderer.h:440) is left to SRL-08's audience marker: it is G93's question, not this fix.

<a id="oca-59"></a>
### OCA-59. Know that a collidable must say how it is retired

**As** an adopter whose projectiles leave the world, **I want** the collision guide to say that set_for_deletion does nothing by default and a collidable must override it for the bounds sweep to retire it, **so that** my projectiles are removed instead of silently living on.

XS · docs · E06 Concepts and reference that tell the truth a caller needs · gaps G49 · **Now**

**Acceptance**

- Given guides/collision.mdx, When I search for 'set_for_deletion', Then the rule is stated
- Given engine/collision/collision_object.h (set_for_deletion, currently :60, an empty virtual), When I read its comment, Then it states the default and the consequence

**Trade-offs**

- T6: a silent no-op must be documented
- T1: whether leaving the world retires an object is the game's rule; the engine supplies the sweep and the hook, so the default stays empty and is documented
- PHILOSOPHY.md:742-751: changing the hook's default changes behaviour for callers this repository cannot count

**Notes.** Documentation only. Changing the default is a separate decision that touches ColourWars (G19).

<a id="oca-60"></a>
### OCA-60. Know what files my Windows release needs and build a package

**As** an adopter shipping a Windows game, **I want** a page listing what a player's folder must contain and template install() lines that produce it, **so that** I ship a folder that runs without hand-picking files from the build tree.

M · code · E07 Ship a Windows release to players · gaps G14 · **Next**

**Acceptance**

- Given the site, When I search for 'release package', Then a page lists the exe, DirectXTK.dll, XAudio2_9Redist.dll, manifest.json and content folders
- Given website/src/walkthrough, When I run cmake --install for x64-release into an empty folder, Then the listed files appear, no file outside the listed set is installed, and the game starts from that folder

**Trade-offs**

- T1: one client (ColourWars, 139 hand-written lines) has staged a release; until a second needs the same shape, a few install() lines in the template are the game's, and an engine staging helper would generalise past what clients have shown
- T9: lists the bought DLLs the player needs

**Notes.** Not general tooling (PHILOSOPHY.md:781-800). Depends on OCA-61 for the runtime and OCA-62 for notices. The list may grow by the notice files once OCA-62 lands.

<a id="oca-61"></a>
### OCA-61. My release runs on a clean Windows machine

**As** an adopter shipping to players who may lack the VC++ redistributable, **I want** to be told my game needs the MSVC runtime and how to ship or install it, checked once on a clean machine, **so that** players do not see an OS loader error before any engine code runs.

S · verification-hardware · E07 Ship a Windows release to players · gaps G15 · **Next** · **Owner**

**Acceptance**

- Given the release-package page, When I read it, Then it names MSVCP140, VCRUNTIME140 and VCRUNTIME140_1 and the chosen remedy (redistributable, app-local copy or static runtime)
- Given a clean Windows VM or Windows Sandbox with no redistributable, When the packaged minimal game is launched, Then it starts, and the run is recorded with date and commit

**Trade-offs**

- T6: a loader failure happens before any engine code and names nothing, so the requirement must be stated where the adopter reads it
- T9: the MSVC runtime and DirectXTK are the bought platform edge; how they reach a player (redistributable, app-local or static) is part of shipping them

**Notes.** Nobody has run the build on a clean machine. Windows Sandbox may lack a hardware adapter; the debug WARP fallback or a d3d11 adapter check may decide which build is tried.

<a id="oca-62"></a>
### OCA-62. Know the notices my Windows game must ship

**As** an adopter shipping a Windows game, **I want** a list of the licences of what a desktop game links or carries (DirectXTK, XAudio2 redistributable, CRT, DirectXMath, rapidjson) and where to find each text, **so that** my release meets its redistribution obligations.

S · docs · E07 Ship a Windows release to players · gaps G16 · **Next**

*Also serves:* The Pinned Submodule Integrator (merged from PI-12)

**Acceptance**

- Given NOTICE or the release-package page, When I read it, Then for the d3d11 + xaudio2 default it lists DirectXTK, DirectXMath, XAudio2Redist (Microsoft's redistributable terms), rapidjson and the MSVC runtime, each with its licence and the file to copy
- Given the other render and audio backends, When the list is read, Then it states what each adds to or removes from it
- Given the vcpkg ports in the root vcpkg.json, When compared with the list, Then every runtime dependency appears
- Given the list, When read, Then a sentence beside it says it is updated in the same commit as any vcpkg.json dependency change, and `git log -- vcpkg.json` after it lands shows each dependency change touching the list

**Trade-offs**

- T9: DirectXTK, DirectXMath, XAudio2Redist, rapidjson and the MSVC runtime are the bought platform and format edge; shipping them, their notices included, is part of buying them
- PHILOSOPHY.md:714-716 (The public face): what reaches a stranger's players is documented for that stranger

**Notes.** Merged from PI-12 (same desktop notices list, G16). OCA-63 (browser notices) stays separate. ColourWars recovered its list partly from vcpkg's per-port copyright files (release_package.cmake:43-54). Having OCA-60's install step copy the notice files is an optional follow-up.

<a id="oca-63"></a>
### OCA-63. Know the notices my browser build must ship

**As** an adopter hosting my game's page, **I want** the template's browser build to gather the licence notices of what the module links, as LineSweeper's bundle does, **so that** my page meets its obligations without reading tools/web_bundle.

S · build-ci · E08 Ship my game to a web page · gaps G16 · **Later**

**Acceptance**

- Given the minimal template's wasm build, When it builds, Then a notices file listing Emscripten, libc++, musl and the others is written beside the page, or a published page says how to produce it
- Given the browser-build guide (OCA-20), When I read it, Then it links that file or step

**Trade-offs**

- T9: bought runtime terms
- T1: reuse tools/web_bundle rather than a new tool

**Notes.** Split from OCA-62 because it ships independently and serves the browser path.

<a id="oca-64"></a>
### OCA-64. Know the minimum hardware and Windows my players need

**As** an adopter writing a store page's minimum requirements, **I want** a page stating each backend's floor (D3D feature level 10_0, shader model 5_1 for d3d12, GL 3.3 core, Vulkan with timeline semaphores) and the minimum Windows version, **so that** I can state honest requirements.

S · docs · E07 Ship a Windows release to players · gaps G105 · **Next**

**Acceptance**

- Given the site, When I search for 'feature level' and 'GL 3.3', Then one page lists each backend's floor, each citing the code that enforces it
- Given that page, When I read the Windows line, Then it names a minimum version with its reason (e.g. WM_POINTER needs Windows 8)

**Trade-offs**

- T6: floors the code already enforces, stated

**Notes.** Evidence: d3d11/device_resources.cpp:119-120; engine/CMakeLists.txt:148-155; vulkan/device_resources.cpp:620-626; app/win32/pointer_input.cpp:22-31. Speed on the low tier (G24) is separate and unmeasured; the page should say so. Can share a page with OCA-65.

<a id="oca-65"></a>
### OCA-65. Know which render backend to ship and what to do when a machine refuses it

**As** an adopter choosing a backend for my release, **I want** a recommendation for which backend a shipped Windows game should build, how a consumer sets it, and what a player sees when their machine refuses it, **so that** I ship the right build and players get a usable message.

S · docs · E07 Ship a Windows release to players · gaps G150 · **Next**

**Acceptance**

- Given the site, When I read build-labrador or the release page, Then it recommends a default backend with its reason and says the choice is LABRADOR_RENDER_BACKEND in my preset
- Given your-project.mdx, When I grep for 'LABRADOR_RENDER_BACKEND', Then it is mentioned with a link to that recommendation
- Given that page, When I read the fallback part, Then it describes shipping two executables (e.g. d3d11 and gl) as the T5-consistent option and quotes the message a refused d3d11 device shows today

**Trade-offs**

- T5: no runtime switching
- T8: no virtual call per sprite
- T6: a refused device names itself

**Notes.** Evidence: build-labrador.mdx:58-70; your-project.mdx has no backend mention; d3d11/device_resources.cpp:161-165; samples/minimal/main.cpp:54-62; engine/CMakeLists.txt:121 (default d3d11), :241 (Vulkan loader linked implicitly, inferred to fail before main). Optional follow-up, not acceptance: make the template's MessageBox text player-facing.

<a id="oca-66"></a>
### OCA-66. Know whether my game is crisp on a scaled display

**As** an adopter whose players use laptops at 125-150% scaling, **I want** a recorded decision on DPI awareness and what my game should do about it, **so that** my point-sampled text is not blurred by Windows without my knowing.

S · verification-hardware · E07 Ship a Windows release to players · gaps G151 · **Later**

**Acceptance**

- Given docs/design or the release page, When I search for 'DPI', Then the engine's choice (aware or not) and its reason are stated
- Given that choice, When the template runs on a display set to 150%, Then the result is recorded (screenshot looked at, date, commit)
- Given a decision to be aware, When the template builds, Then a manifest or API call declares it and the 150% run shows unblurred text

**Trade-offs**

- PHILOSOPHY.md:680-685: the low tier is a two-CU shared-memory Radeon at 1280x720, so rendering fewer pixels (unaware) may suit it, an unmeasured claim per :691-694
- T6: Windows silently blurring a DPI-unaware window is graceful degradation; an undecided default is an unstated one

**Notes.** A grep for 'dpi' in engine/app finds only the web window's devicePixelRatio (engine/app/web/window.cpp:160). Not observed on a scaled display; the desktop's Windows scaling setting can stand in for a laptop.

<a id="oca-67"></a>
### OCA-67. Pin a named version and know what it promises

**As** an adopter deciding whether Labrador is stable enough, **I want** a tagged 0.x version with a sentence saying what it promises, and a stated criterion for when the API settles, **so that** I pin something named rather than an arbitrary commit.

S · process · E10 Releases, versions, upgrades and an answered report · gaps G17 · **Now** · **Owner**

*Also serves:* The Pinned Submodule Integrator, The Sole Owner-Maintainer (merged from PI-04, SM-33)

**Acceptance**

- Given docs/design/PHILOSOPHY.md:739-741, When I read the amended passage, Then 'the API settles' has a stated, checkable criterion chosen by the owner (for example a named set of modules with no breaking CHANGES entry for N weeks, or a client pinned at a tag), amended in the same commit as the first tag
- Given `git tag`, When it runs after this story, Then at least one 0.x tag exists on a commit whose CI is green, and vcpkg.json's version-string matches it
- Given the GitHub release for that tag, When I open it, Then its notes state the 0.x promise (no compatibility promised; breaks recorded in CHANGES.md) and link CHANGES.md (OCA-68)
- Given the criterion is not yet met, When I read PHILOSOPHY, Then it names what is unmet, and `git tag` matches the document either way
- Given check_doc_citations and website.yml, When they run on the PR, Then both pass

**Trade-offs**

- PHILOSOPHY.md:739-741 (The public face): releases are versioned once the API settles, so a 0.x tag promises nothing until a stated criterion makes 'settles' checkable; the criterion is amended in the same commit as the first tag
- T1 (Not a licence for speculative frameworks): a tag plus notes is the minimal release process; no release branches or backports

**Notes.** Merged from PI-04 and SM-33: one change, a settle criterion plus the first 0.x tag and release. The criterion's wording needs the owner's sign-off. Ordered after OCA-68 (a running change list) and OCA-69 (a deletion rule), because a tag with no follow-through would promise a stability the process cannot keep. Lands in PHILOSOPHY amendment B with OCA-69, the one owner-reviewed PHILOSOPHY PR, after amendment A (OCA-05 + FES-03). Cutting 1.0 now would contradict :739-741. Whether the site starts versioning at 0.x (docs/website-versioning.md) is an owner decision recorded separately.

<a id="oca-68"></a>
### OCA-68. Read what changed that affects my game, from the pin I am on

**As** an adopter moving my engine pin, **I want** a running list in the engine of consumer-visible and breaking changes, with the commit and the migration, **so that** I can plan an upgrade without diffing every changed engine file.

L · docs · E10 Releases, versions, upgrades and an answered report · gaps G18 · **Now**

*Also serves:* The Pinned Submodule Integrator, The Sole Owner-Maintainer (merged from PI-01, SM-34, PI-02)

**Acceptance**

- Given the repository root, When I list it, Then CHANGES.md exists, newest first, and its opening paragraph defines consumer-visible: a source break, a removed or retyped public name, a new required package or vcpkg.json change, a moved header, a cmake/settings.cmake compile-option change, or a silent behaviour change such as 92966b0's content-root move
- Given CHANGES.md, CLAUDE.md and AGENTS.md (one brief after SM-22), and the merge procedure (SM-20), When read, Then each states the rule: a commit touching engine/**/*.h, engine/CMakeLists.txt, cmake/settings.cmake or vcpkg.json adds an entry or says 'not consumer-visible' in its message
- Given `git log <introducing-commit>..HEAD -- 'engine/*.h' engine/CMakeLists.txt cmake/settings.cmake vcpkg.json`, When each listed commit is checked, Then it either adds an entry or its message contains 'not consumer-visible'
- Given a source-break entry, When I read it, Then it names the old spelling, the new spelling and the sample diff that is its migration template; and no entry text has been copied into a header comment (grep engine/)
- Given `git log --format=%h 4cefb68..<first-entry commit> -- engine cmake CMakeLists.txt vcpkg.json`, When each hash is searched for in the backfill section, Then every hash appears in an entry or in an explicit 'not consumer-visible' list
- Given the backfill, When I read it, Then it covers at least: Application::initialize() losing its arguments (and the unused-parameter error that causes under /W4 /WX), window() returning void*, ApplicationOptions::visible replacing SW_HIDE, window.cpp moving to win32/window.cpp, 92966b0's content-root move, and lodepng plus platform-qualified directxtk in vcpkg.json with a note that a consumer needs neither; it cites 4175640's sample diffs as migration templates
- Given docs/port/web.md's D2 row (:779), When the backfill lands, Then that row is corrected or points at the backfill
- Given the next merged PR that changes a declaration in a header listed in website/src/lib/published.ts, When git show runs on its commit, Then the same commit adds a CHANGES.md entry naming the change

**Trade-offs**

- PHILOSOPHY.md:739-741 (The public face): breaking changes are deliberate, batched and documented, not incidental; a newest-first changes file is the 'documented' half, kept from now on rather than reconstructed
- T1 (Not a licence): no speculative deprecation framework in engine code; a flat list suffices
- T4: an entry is written for an integrator who did not write the change (old spelling, new spelling, migration diff); a silent behaviour change such as 92966b0's content-root move is the case no compiler will report

**Notes.** Merged from PI-01 (the file and its rule), SM-34 (the brief and the merge procedure state it) and PI-02 (the backfill to 4cefb68). Sized L: the backfill classifies 45+ commits and 131 changed engine files, a good agent task (diff public headers per commit). CHANGES.md is not a code comment, so CONVENTIONS' no-history rule does not apply. With ColourWars' math-split migration done (77a7a7f), the backfill serves the next ordinary bump from 4cefb68 (SM-36). Depends on SM-20 so the merge procedure it amends exists. OCA-15's template change list points here. Delay compounds: every merge without an entry widens the backfill, so it is in Now.

<a id="oca-69"></a>
### OCA-69. Know how a public name leaves the engine

**As** an adopter whose code calls engine names nobody upstream can count, **I want** a written rule for removing a public name that accounts for callers outside the counted repositories, **so that** I know when a name I use may disappear and how I will hear.

S · docs · E10 Releases, versions, upgrades and an answered report · gaps G19 · **Now** · **Owner**

*Also serves:* The Sole Owner-Maintainer (merged from SM-35)

**Acceptance**

- Given docs/design/PHILOSOPHY.md:742-758, When I read the deletion rule, Then it says which repositories are counted and how uncountable outside callers are covered (for example: removal only at a tagged release, with a CHANGES.md entry)
- Given the amendment, When I read it beside :750-751 ('A count that cannot be taken is a reason to keep, not a reason to guess'), Then it extends that rule rather than loosening it, and says how an outside caller hears of a removal
- Given the amendment, When I read it, Then it introduces no deprecation shims while the API is unsettled and no build step that reaches a client repository
- Given check_doc_citations, When the top-level build runs, Then it passes

**Trade-offs**

- PHILOSOPHY.md:742-751 (The public face): a count that cannot be taken is a reason to keep, not to guess; the amendment says how an outside caller hears of a removal without weakening that
- PHILOSOPHY.md:739-741: breaking changes are deliberate, batched and documented, so a removal lands at a release with a changes entry rather than behind a shim
- T1 (Not a licence): no deprecation-shim framework while the API is unsettled
- PHILOSOPHY.md:748-750: no build step reaches client repositories; this stays a written discipline

**Notes.** Merged from SM-35 (same PHILOSOPHY amendment, G19). The wording needs the owner's sign-off. Lands in PHILOSOPHY amendment B with OCA-67's criterion, the critique's one owner-reviewed PHILOSOPHY batch, after amendment A. A build step that reaches client repositories would fight PHILOSOPHY.md:748-750.

<a id="oca-70"></a>
### OCA-70. Move my engine pin by a written procedure

**As** an adopter taking a newer Labrador, **I want** a page with the commands and order of checks for a pin bump, including re-diffing the engine's vcpkg.json and the minimal template, and the supported MSVC versions, **so that** I upgrade the way the engine expects rather than by trial.

S · docs · E10 Releases, versions, upgrades and an answered report · gaps G20, G37 · **Next**

*Also serves:* The Pinned Submodule Integrator (merged from PI-03)

**Acceptance**

- Given your-project.mdx (or a page it links), When I search for 'move the pin', Then it gives the git commands to fetch and check out a new engine commit in external/labrador and the order: read CHANGES.md, diff external/labrador/vcpkg.json between the old and new pins, configure, build under /W4 /WX, run your own tests, commit the pin
- Given the page, When it discusses the manifest, Then it says the consumer's builtin-baseline and dependencies are compared with external/labrador/vcpkg.json at the reader's pin, and that the site's ConsumerManifest is derived from master
- Given the API reference index or the pin page, When read, Then a sentence says the headers under external/labrador are the reference at the reader's pin and the site documents master (G37)
- Given the page, When it names the toolchain, Then it states the last MSVC toolset known to build the engine, says CI floats on windows-latest, and says /W4 /WX applies to the reader's code through labrador_settings and is kept, not relaxed
- Given the walkthrough, When an agent follows the page once against it, Then the procedure works as written, and `npm run build` in website/ passes

**Trade-offs**

- T5: warnings-as-errors is one of the walls (PHILOSOPHY.md:104-105); /WX reaches consumer code through labrador_settings and is kept, so the page says so rather than relaxing it
- PHILOSOPHY.md:739-741 (The public face): changes are documented, so a pin bump follows a written procedure that reads them
- T4: written for a stranger taking a newer engine cold
- T1: a page, not a package-manager abstraction or a configure-time manifest checker

**Notes.** Merged from PI-03 (same page, G20 and G37). OCA-71 (run the engine's own suite at a pin, G38) is a separate deliverable that rides in the same commit. The only procedure today is private (ColourWars CLAUDE.md); ColourWars' own manifest drift (still named 'artattack') is the exhibit for the manifest step. SM-36's first ordinary bump is the second real run of this procedure. CI's runner pin is G115, not this story.

<a id="oca-71"></a>
### OCA-71. Run the engine's own tests at my pin

**As** an adopter checking a pin bump, **I want** an engine-side instruction for configuring the submodule as its own top-level project into a separate build folder and running its ctest, **so that** I can see the engine passes at my pin on my machine.

XS · docs · E10 Releases, versions, upgrades and an answered report · gaps G38 · **Next**

**Acceptance**

- Given README.md or the pin-procedure page, When I search for it, Then the commands are shown
- Given a consumer checkout with external/labrador, When I run them, Then ctest runs the engine's entries without touching the consumer build

**Trade-offs**

- T1: documentation only; engine tests stay out of a consumer's ctest (CMakeLists.txt:42-46)

**Notes.** The recipe exists only in ColourWars' CLAUDE.md and was never run in a walk; run it once.

<a id="oca-72"></a>
### OCA-72. Ask a question or request an API change through a stated route

**As** an evaluator or adopter with a fit question or a missing piece, **I want** a stated place to ask and a short rule for how an API request is judged (lead with your workaround as the exhibit, answered with T-numbers), **so that** I know where to go and what a good request looks like.

S · process · E10 Releases, versions, upgrades and an answered report · gaps G21 · **Now**

*Also serves:* The Pinned Submodule Integrator (merged from PI-07)

**Acceptance**

- Given .github/ISSUE_TEMPLATE, When I open a new issue, Then an engine-request template asks for the need, the client workaround as the exhibit, the engine revision checked against (master, not the pin) and the T-number the requester thinks applies
- Given CONTRIBUTING.md or the site's troubleshooting page or Get Started index, When I look for how to reach the project, Then it names the route for questions and requests as well as for build and doc problems
- Given that text and the template, When I read them, Then they state the exhibit rule, that a request is weighed on PHILOSOPHY's trade-offs and may be declined with a reason, and promise no answer time

**Trade-offs**

- T1: the exhibit rule is T1's demand test applied to intake: a request names the client workaround it would delete

**Notes.** Merged from PI-07 (same route and template, G21). Whether to enable GitHub Discussions is an owner-only settings choice and not required. The exhibit rule today lives in ColourWars docs/review/engine-reports.md; one report described a bug master had already fixed, hence 'checked against master'.

<a id="oca-73"></a>
### OCA-73. Report a vulnerability privately and know what input is trusted

**As** an adopter who may load mods, user levels or fetched content, **I want** a security policy that says whether content files are treated as trusted input and gives a private reporting route, **so that** I can judge the risk of loading untrusted content and report a flaw without disclosing it publicly.

S · process · E10 Releases, versions, upgrades and an answered report · gaps G148 · **Now** · **Owner**

*Also serves:* The Sole Owner-Maintainer (merged from SM-42)

**Acceptance**

- Given the repository root, When I look, Then SECURITY.md exists naming the supported revision (master, or the latest tag once OCA-67 lands), the private reporting route, and the trust posture of .dds, .spritefont, .xwb and manifest.json content
- Given SECURITY.md, When I read it, Then it claims no more than the tree backs: hardened, tested readers that throw at load naming the file, and no fuzzing or sanitizer configuration
- Given `gh api repos/matt439/Labrador/private-vulnerability-reporting`, When called after the owner enables it, Then it returns enabled: true

**Trade-offs**

- T6: malformed .dds/.spritefont/.xwb/manifest.json content is a load-time throw naming the file, not a limp; that, and no more (no fuzzing or sanitizer runs exist), is the trust posture SECURITY.md can state
- T4: written for a stranger seeing the engine cold; the posture and the private route are stated rather than left to be inferred
- T1: no speculative code (no sandboxing or fuzzing framework); the story is a policy file and a repository setting

**Notes.** Merged from SM-42 (same file and setting, G148; registered and spot-checked: no SECURITY.md exists). Enabling private vulnerability reporting is an owner-only repository setting, separate from the agent-written file. Rides one PR with OCA-72's request route.

<a id="oca-74"></a>
### OCA-74. Embed the LineSweeper bundle on my own site from a published contract

**As** a C++ developer who ships a game to a web page, or who hosts the LineSweeper bundle on a site other than mattharding.dev, **I want** one published page stating the linesweeper.js module's factory contract and what a host must serve, **so that** I can host the bundle, or copy its shape for my own game's bundle, without reading a sample page's comments and an internal port plan.

S · docs · E08 Ship my game to a web page · gaps G142, G51 · **Later**

*Also serves:* The Browser and Phone Demo Player

**Acceptance**

- Given the site, When I search for 'createLineSweeper', Then one page states the factory's contract: it is the default ES-module export of linesweeper.js; Module.canvas is required; onQuit, onError, onContextLost and onContextRestored are optional, each with when it is called; callbacks are matched by exact name with no warning for a misspelling; and a running instance cannot be stopped from the page, only quit from inside the game
- Given that page, When I read the bundle section, Then it lists what web-dist/linesweeper/ holds (linesweeper.js, linesweeper.wasm, the example index.html, the licence notices, BUILD.json) and what BUILD.json's fields mean (commit, dirty, build type, each file's raw and Brotli size)
- Given that page, When I read the hosting section, Then it states what a host must send (application/wasm, no COOP/COEP needed, 'wasm-unsafe-eval' under a CSP, cache rules for the hashed folder) and links OCA-23's page rather than restating it
- Given samples/linesweeper/web/index.html:1-40 and docs/port/web.md section 7, When the page lands, Then both point at it as the public statement, and the page agrees with them
- Given a scratch page outside the tree that imports the bundle exactly as the published page says, When it is served over HTTP and loaded in headless Chrome, Then the game draws and quits through onQuit with no console errors
- Given website.yml, When it runs, Then it passes

**Trade-offs**

- PHILOSOPHY.md:714-716 (The public face): what a host owes the module is a contract, and a contract is documented where a stranger reads it, not in a sample page's comments and an internal port plan
- T1: documents the existing bundle; a host-stop API or a second bundle shape waits for a host that needs it
- T6: a misspelled callback is silently ignored today, so the published contract says matching is by exact name

**Notes.** Added from the critique's unstoried list. Persona argued: the outside-cpp-adopter, not the browser-demo-player. A host integrator writes code against a module's contract, which is a developer's task on the adopter's oca-ship-desktop-and-browser journey; LineSweeper's bundle is the only reusable host bundle in the tree (G51), so it is the shape an adopter copies for their own game's bundle. The player benefits indirectly (another host can carry the demo), hence also_serves. Registered against G142 (the page-to-module contract has no single public statement; its evidence already cites index.html:22-27 and createLineSweeper) and G51 (hosting requirements only in the internal plan). Different from OCA-26, which states the generic page contract for an adopter's own module; this story publishes the distributed bundle's factory and files and depends on OCA-26 and OCA-23 so it can link rather than repeat them.

<a id="oca-75"></a>
### OCA-75. A template font I may ship without checking its terms

**As** a C++ developer who copied samples/minimal into my own repository, **I want** the template, and the samples that embed the same atlas, to draw text from an openly licensed font, **so that** the font I copy is one I may redistribute in my own game without asking anyone.

M · content · E04 Content, audio and text that work on both platforms · gaps G107 · **Next** · **Owner**

**Acceptance**

- Given samples/minimal/content/fonts, When listed, Then its atlas is built from a font whose licence permits redistribution (for example under the SIL Open Font License), the licence text sits beside it and NOTICE names it
- Given every sample and test that embeds the old atlas, When this lands, Then each uses the new one or states why not, and NOTICE:154-160 no longer needs to disclaim the template's font
- Given RenderPixelTests and LineSweeperCapture, When the goldens and captures that draw text are regenerated (LABRADOR_GOLDEN_DUMP=1 on x64-release), Then every changed image is reviewed before the commit, and all four rasterising backends plus the wasm pixel page stay byte-identical within ALLOWED_CHANNEL_DRIFT
- Given OCA-37's disclosure, When this lands, Then it is replaced in the same commit by a line naming the new font and its licence

**Trade-offs**

- T6: a licensing problem discovered at ship time is the quiet, expensive failure, doubly so for a user who did not write the engine; an openly licensed atlas removes it rather than only stating it
- CLAUDE.md (RenderPixelTests, LineSweeperCapture): every golden and capture that draws text is regenerated and each changed image reviewed; a regeneration that is not looked at turns the contract into a recording

**Notes.** Split from OCA-37 per the critique: the disclosure is OCA-37 (Now); this replacement is Next. Owner decision: whether to replace the font at all, and with which. Cost is mostly review: regenerating and looking at every golden image and capture that draws text, including the text cases that decode their atlas in the test.

<a id="oca-76"></a>
### OCA-76. Ask the engine where my game may write, on Windows and in the browser

**As** an adopter whose game keeps settings or saves on both platforms, **I want** the engine to say where per-user data goes - a known folder on Windows, a persistent mount in a browser - beside where it already says the content is, **so that** my save code has no platform branch of its own.

S · code · E05 Input, menus, settings and couch play my players can use · gaps G108 · **Later**

**Acceptance**

- Given engine/app/content_root.h, When I read it, Then a function names a per-user directory for a game's name: a folder under the user's local application data on Windows, and a directory on a persistent mount in a browser
- Given a browser build, When the game writes a file there and calls the engine's flush, Then a reload of the page reads the same bytes back, checked by a headless-Chrome page test
- Given the guide OCA-53 writes, When this lands, Then its platform branches are replaced by the call

**Trade-offs**

- T1: the location is a platform fact like the content root; what is written there, and its format, stay the game's
- T12 (price): serialisation is still written by hand; this is only where the file goes
- ARCHITECTURE module table: assets only reads, so the call lives in app beside content_root.h, not in assets

**Notes.** Split from OCA-53 on 2026-10-24. Present caller: ColourWars' `game/directory_consts.cpp`, which reads `%LOCALAPPDATA%` itself and falls back to beside the executable; it is Windows-only, so the browser half has no caller yet. Built when a second client asks, or when a browser game does. A review of the same date said the browser mount is shell code no game can write; that does not hold, because a game owns its `main_web.cpp`.

<a id="stories-pinned-integrator"></a>
## The Pinned Submodule Integrator

<a id="pi-05"></a>
### PI-05. The engine tells my build which revision of it I am shipping

**As** a pinned submodule integrator shipping a Windows ZIP, **I want** the engine to expose its own revision (commit and dirty state, or a version) to my CMake build, **so that** my build-info file names the engine revision without my build running git inside the submodule, and still says something truthful when built from a source archive.

S · code · E10 Releases, versions, upgrades and an answered report · gaps G53 · **Next**

**Acceptance**

- Given a consumer project that add_subdirectory()s Labrador, when it configures, then a documented CMake variable or generated header (name negotiable) holds the engine commit hash and whether the engine tree is dirty
- Given the engine copied without a .git directory, when the consumer configures, then the variable holds an explicit 'unknown' value (or the version from vcpkg.json) rather than an empty string
- Given tools/web_bundle/bundle.mjs, when it writes BUILD.json, then it reads the same source of truth or a comment there says why it does not
- Given CI on the engine, when a configure check or test runs, then it asserts the value equals `git rev-parse HEAD` for the checkout

**Trade-offs**

- T1: serves two present callers, the web bundle and a client, so it is demand not speculation
- T5: a wrong or stale revision is caught by a configure-time/CI check against git rev-parse HEAD rather than discovered in a shipped build
- T6: a tree with no .git is expected absence, a state rather than a broken contract (T6's 'Not a licence for'), so configure does not fail; it records an explicit 'unknown' (or the vcpkg.json version) instead of an empty string

**Notes.** Evidence: CMakeLists.txt project() without VERSION; bundle.mjs stamps commit and dirty for LineSweeper only; ColourWars cmake/write_build_info.cmake wrote its own, which this lets it delete. Independent of PI-04; if PI-04 lands first, the tag is the natural 'unknown-commit' fallback.

<a id="pi-08"></a>
### PI-08. See what happened to each request and which commit answered it

**As** a pinned submodule integrator, **I want** a public record of each request's disposition: answered (with the commit), declined (with the T-reason), or open, **so that** at a pin move I can see which of my workarounds the engine has made unnecessary, and before filing I can see whether a need was already declined.

S · process · E10 Releases, versions, upgrades and an answered report · gaps G54 · **Later**

**Acceptance**

- Given a request filed through the route in PI-07, when it is closed, then the closing record names either the answering commit or the PHILOSOPHY trade-off it was declined under
- Given the repository's issue labels (or a short ledger file), when I filter, then answered, declined and open requests are separable
- Given the changes list (PI-01), when an entry answers a request, then it links that request

**Trade-offs**

- T1: labels on the existing tracker, not a heavyweight process or a separate ledger
- PHILOSOPHY.md:757-758 (The public face): a decline resting on a measurement is re-taken when the measurement moves, not re-cited, so a declined request's record names the trade-off and what would reopen it

**Notes.** Evidence: today the only answers are commit messages (343433c 'report 7', f94ac81), and the only status record is the client's private ledger. The client keeps its own ledger. Ships with GitHub labels alone if PI-07 lands first.

<a id="pi-09"></a>
### PI-09. File the seven live drafts and the packaging gap upstream

**As** a pinned submodule integrator with seven drafted reports and an unfiled packaging exhibit, **I want** each draft filed upstream, or withdrawn in my ledger with a reason, after rechecking it against engine master, **so that** my live workarounds have an owner and a pending answer instead of becoming permanent by default.

S · process · E10 Releases, versions, upgrades and an answered report · gaps G74 · **Next** · **Owner**

**Acceptance**

- Given ColourWars engine-reports.md reports 6, 9, 10, 11, 12, 13 and 14, when this story is done, then each is either filed as a Labrador issue carrying its exhibit and the master commit it was rechecked at, or marked withdrawn in the ledger with the reason
- Given ColourWars cmake/release_package.cmake and release_assets.cmake, when this story is done, then a packaging request (gap-audit F8) is filed with them as the exhibit
- Given each filed issue, when read, then it states the client workaround it would delete

**Trade-offs**

- T1: each is a claim of demand from a present caller, filed so it can be weighed
- T8: report 10 asks around read_back_buffer, which renderer.h marks NOT A FRAME-PATH CALL; the filing should say an out-of-band hook is acceptable

**Notes.** Reports 10 and 11 were already rechecked at master and still hold. Most of the work is rechecking the other five; if one needs a reproduction, split it out. Can be filed as plain GitHub issues today without waiting for PI-07's template. Persona assumption flag: these are one game's needs until weighed. Deciding them is PI-10.

<a id="pi-10"></a>
### PI-10. An upstream answer for each filed report

**As** a pinned submodule integrator, **I want** each filed report decided upstream, either implemented or declined with the trade-off that declines it, **so that** I can delete the workaround or keep it on purpose, instead of carrying it pending.

M · process · E10 Releases, versions, upgrades and an answered report · gaps G74 · **Later** · **Owner**

**Acceptance**

- Given the reports filed in PI-09, when this story is done, then every one is closed as answered with a commit, or declined with a cited PHILOSOPHY T-number or ARCHITECTURE section, or explicitly accepted and split into its own story
- Given a declined report that touches a settled position (report 6 audio bus against T1; report 12 JSON write against the read-only asset asymmetry in ARCHITECTURE), when the decline is written, then it cites the passage it rests on
- Given an implemented answer, when it lands, then it has behavioural tests in the same commit and an entry in the changes list

**Trade-offs**

- T1: deciding is the close; implementing is not presumed; a decline weighs mechanism against game policy
- PHILOSOPHY.md:792-798 (The public face): a decline may not rest on 'only one client asks', because the count does not settle what the library is for; the question is whether a client would have to write it itself
- T8: a capture hook for report 10 must stay out of the frame path (renderer.h:415 marks read_back_buffer NOT A FRAME-PATH CALL, citing T8)

**Notes.** Size M covers the decisions only. An accepted report bigger than XS becomes its own story; packaging (F8), if accepted, is PI-11.

<a id="pi-11"></a>
### PI-11. Stage the runtime files the engine makes my game need, without hand-keeping that list

**As** a pinned submodule integrator maintaining 139 lines of release packaging, **I want** the engine to supply what it alone knows about a shipped Windows build - its runtime DLLs, the redistributable runtime, and where its loader expects content beside the executable - in a form my install rules can call, **so that** my release package stays right when the engine's dependencies or content-root rules change at a pin move, and I can delete the part of my packaging that duplicates engine knowledge.

M · code · E07 Ship a Windows release to players · gaps G14 · **Later**

**Acceptance**

- Given a documented engine CMake function (name negotiable) called on a game target, when the consumer runs `cmake --install` on an x64-release (d3d11 + xaudio2) build, then the install folder holds the exe, DirectXTK.dll, XAudio2_9Redist.dll and the MSVC runtime DLLs, and a CI test (in PI-06's consumer job or an engine test) asserts that file list
- Given the function called with a content directory and a list of files the game names, when installed, then those files land at the path the engine's content-root resolution reads beside the executable, and the function chooses no files itself
- Given a configuration whose runtime is the debug CRT, when the install rules are generated or run, then they fail with FATAL_ERROR naming the debug CRT as not redistributable; a Release-only rule stays in the game's own CMake
- Given the function called from a project that is not top level, when the consumer configures, then it adds nothing to the engine's own install and no entries to the consumer's ctest

**Trade-offs**

- T1: defensible only as demand, because a real client hand-wrote it (ColourWars cmake/release_package.cmake:20-30); which assets ship and Release-only packaging stay the game's policy
- T5: configure-time refusal of a non-redistributable runtime
- T6: a debug-CRT package fails loudly

**Notes.** Start only if PI-10 accepts the packaging request. If it is declined, this story is dropped from the backlog and the decline is recorded under PI-10. ColourWars' release_package.cmake refuses non-x64-release (line 11) and non-d3d11/xaudio2 (line 16): those are its policy and stay there. The docs-only half, a page listing what a player needs, is storied for the outside adopter.

<a id="pi-14"></a>
### PI-14. My release ZIP is known to run on a player's clean Windows machine at a current pin

**As** a pinned submodule integrator shipping to players without Visual Studio, **I want** a recorded run of my Release ZIP, built at a current engine pin, on Windows without Visual Studio, including gameplay, **so that** I know players can run what I ship, and the record says which engine revision and machine it held for.

M · verification-hardware · E07 Ship a Windows release to players · gaps G77 · **Next** · **Owner**

**Acceptance**

- Given a ZIP built at an engine pin no older than the latest tag (or master if none), When extracted on a Windows 10 or 11 x64 install with no Visual Studio or vcpkg (a fresh VM or Windows Sandbox counts), Then the game reaches gameplay and quits cleanly, or, with no usable device, refuses loudly with a named reason
- Given the run, When recorded in the client repository, Then the record names the Windows edition and build, the GPU and driver (or that the sandbox had no hardware adapter), the game and engine commits, what was played (at least one match with the owner's physical pad), and what was not checked
- Given the reference machine's integrated Radeon, When the same ZIP runs on it, Then the record states the result and the driver it ran on (the AMD driver per SM-01, not the Microsoft Basic Display Adapter), or states that it was not run

**Trade-offs**

- T6: a machine with no usable device (e.g. Windows Sandbox without GPU passthrough, where Release has no WARP fallback) must be seen to fail loudly with a named reason, not silently; 'doubly so for a user who didn't write the engine'
- PHILOSOPHY.md:677-690 (the low tier): the iGPU run is on the named configuration only once the AMD driver replaces the Microsoft Basic Display Adapter, and the record says which driver it ran on

**Notes.** ColourWars CI proves only start-and-shutdown on a runner with VS and a reduced PATH. The record lives in ColourWars. Depends on SM-36's ordinary bump (the math-split migration is done, so it is no longer blocked) and on OCA-61. Windows Sandbox has no hardware GPU passthrough by default, and Release has no WARP fallback, so the sandbox run may only prove a loud refusal; the GPU line of the record must say which.

<a id="pi-15"></a>
### PI-15. Launching my game hidden through STARTUPINFO is tested upstream

**As** a pinned submodule integrator whose CI starts the game with Start-Process -WindowStyle Hidden, **I want** the engine's promise that its first ShowWindow honours the launcher's STARTUPINFO to be held by a test, **so that** a pin move cannot silently make my hidden startup check pop a window or behave differently.

S · code · E07 Ship a Windows release to players · gaps G78 · **Later**

**Acceptance**

- Given AppTests under a Windows preset (including the -null presets), when it runs, then a case launches a child process (a small test executable creating a Window with default ApplicationOptions, no device) with STARTUPINFO wShowWindow = SW_HIDE and STARTF_USESHOWWINDOW, and asserts with IsWindowVisible that the child's window is not visible after its first ShowWindow
- Given the same case with SW_SHOWNORMAL, when it runs, then the window is visible, so the test distinguishes the two
- Given tests/app/CMakeLists.txt, when read, then the child executable's source is listed explicitly and sits in the non-EMSCRIPTEN branch beside window_tests.cpp
- Given the wasm presets, when AppTests is built, then the case is not compiled and nothing else changes

**Trade-offs**

- T5: the STARTUPINFO promise at engine/app/win32/window.cpp:363-368 becomes a failing test the build runs, rather than an argument
- PHILOSOPHY.md:762-767 (Tests and toolchain): the case needs no device, so it runs under the -null Windows presets, and like window_tests.cpp it is compiled only in the non-EMSCRIPTEN branch where the Win32 window exists

**Notes.** Contract at engine/app/win32/window.cpp:363-368 (SW_SHOWDEFAULT 'asks for whatever the process that started this one put in its STARTUPINFO'). Consumer exhibit: ColourWars scripts/check-startup.ps1. tests/app/CMakeLists.txt:12-16 branch on EMSCRIPTEN only, so the -null Windows presets compile window_tests.cpp and are where this device-free case belongs. Hidden-desktop CI runners may make visibility flaky; assert on IsWindowVisible, not a screenshot.

<a id="pi-16"></a>
### PI-16. Drive my game past its title screen with injected input in a smoke run

**As** a pinned submodule integrator verifying gameplay after a pin move, **I want** to know how injected key and pad input must be timed to survive the engine's per-frame sampling, **so that** my post-bump smoke check can reach gameplay instead of stopping at the title screen.

S · docs · E03 Prove my own game works: tests, smoke checks and CI · gaps G79 · **Later**

**Acceptance**

- Given a published page (troubleshooting or a testing guide), when I read it, then it states that key state is visible only at the next poll, once per tick, so a key pressed and released between two polls is not seen, and a press must be held across at least one poll and released in a later one
- Given the page, when it gives an example, then it includes a script (PowerShell with SendInput or equivalent) that holds an arrow key against the Release MinimalSample.exe long enough to move its marker, and a before/after screenshot pair whose difference shows the move
- Given the page, when it describes what the engine's own browser pages do, then it points at tests/app/web/window_input.mjs as the browser counterpart

**Trade-offs**

- T1: guidance, not a replay or test framework in the engine

**Notes.** Sampling claim verified at engine/input/keyboard.h:208-213. Minimal has no smoke flag; held movement through keyboard_direction (samples/minimal/states/hello_state.cpp:53-69) is the visible effect. Exhibit: ColourWars engine-reports.md, where Enter/Space presses did not advance the title. The automation itself stays the client's.

<a id="pi-17"></a>
### PI-17. Know what JSON the engine reads for me and when my game must bring its own parser

**As** a pinned submodule integrator parsing and writing my own data files, **I want** the consumer documentation to say that the engine's read_json_file covers reading, that rapidjson itself is private, and that writing JSON (a save file) needs a parser the client brings, **so that** I decide deliberately how to bring one, or find I need none, rather than discovering it by a missing include and vendoring a copy that drifts from the engine's.

XS · docs · E06 Concepts and reference that tell the truth a caller needs · gaps G75 · **Later**

**Acceptance**

- Given README.md '## Using Labrador in a project' or your-project.mdx, when I read it, then a sentence says reading a JSON file is engine/assets/json.h's read_json_file with JsonValue, rapidjson is PRIVATE to LabradorEngine and no engine header names it, and a client that writes JSON brings its own parser (from vcpkg or vendored)
- Given engine/CMakeLists.txt's rapidjson block (target_include_directories ... SYSTEM PRIVATE), when compared, then the documented statement matches the link visibility
- Given `npm run build` in website/ if the page changed, when it runs, then it passes

**Trade-offs**

- T9: the parser is a bought edge the engine keeps private on purpose
- T1: documentation only; making rapidjson PUBLIC would undo a stated decision

**Notes.** engine/CMakeLists.txt:366-374 states the decision ('A client that parses files of its own asks for rapidjson on its own account - which the game does, for the save file it writes'). engine/assets/json.h:16 publishes read_json_file. Exhibit: ColourWars carries a byte-identical vendored rapidjson for its save file.

<a id="pi-18"></a>
### PI-18. The include wall says exactly what it protects in my build

**As** a pinned submodule integrator relying on the include wall running in my targeted build, **I want** check_engine_includes.cmake's comment to state that it catches only a client folder literally named game/, **so that** I know what the wall does and does not protect if engine files are ever edited inside my submodule.

XS · docs · E12 A maintainer's process, CI and deploy that one person and their agents can trust · gaps G76 · **Later**

**Acceptance**

- Given cmake/check_engine_includes.cmake, when I read the 'SO WHY THIS STILL RUNS' comment, then it states the regex matches only '#include "game/..."' and '../game/...' and that a client folder with another name is not covered
- Given a top-level build after the edit, when check_doc_citations and check_engine_includes run, then both pass

**Trade-offs**

- T5: 'not a licence for enforcement cleverness; the walls are boring': keep the simple game/ regex and reword the claim rather than generalising the check
- T4: the comment says what the code does: it catches only a client folder literally named game/

**Notes.** Verified: cmake/check_engine_includes.cmake:15-21 says the wall 'does not depend on who is building', and the regex at :33-34 matches only game/. README's 'yourgame/' and the website's 'mygame' are missed. ColourWars uses game/, so it is covered today; low risk.

<a id="pi-19"></a>
### PI-19. Read what the engine's last frame cost, phase by phase

**As** a pinned submodule integrator whose performance overlay times the frame it can see, **I want** `Application` to publish the CPU times of the last frame it completed - its fixed-step count, the interval since the frame before, and the update, begin_frame, draw, submit and end_frame phases - **so that** I can delete the stopwatches I keep around my own draw and see the submit and end_frame split, which no client can measure because the loop is private.

M · code · E10 Releases, versions, upgrades and an answered report · gaps G74, G27 · **Later**

**Acceptance**

- Given a running Application, when a state's update calls `frame_timings()`, then it returns by value the last completed frame's sequence number, fixed-step count, interval and five phase durations as `std::chrono::nanoseconds`, with `frame == 0` until the first frame
- Given a frame that throws, a tick that renders nothing, or a frame a game draws itself inside an update, when it ends, then no frame is published, and time spent in the last is part of `update_time`
- Given `reset_elapsed_time()` or the window coming back from hidden, when the next frame is published, then its interval is zero
- Given AppTests under every preset, wasm-* under Node included, then the arithmetic is held with injected time points; and under the two -null presets an Application loop with a hidden window holds that `frame` rises by exactly one per rendered frame and that a capture inside an update does not advance it

**Trade-offs**

- T1: mechanism, the measurement of the engine's own phases; the ring, overlay, eligibility and export stay the client's, so REV#10's overlay stays deferred
- T8: a fixed handful of steady_clock reads and one struct write per tick, nothing per sprite, and nothing called out
- T3: the last frame only, no valid flag (frame 0 means none yet), no GPU time

**Notes.** Report 14, accepted narrowed and split out of PI-10 as its acceptance requires (docs/review/colourwars-reports/loop.md). SM-08 depends on it: a development page that reads `frame_timings()` behind a query flag and logs it is the browser instrument SM-08 asks for. GPU timestamps would be a seam change across five backends and a separate request.

<a id="stories-sole-maintainer"></a>
## The Sole Owner-Maintainer

<a id="sm-01"></a>
### SM-01. Run the frame bench on the integrated Radeon, not the RTX 5080

**As** a sole maintainer who owns the only machine holding PHILOSOPHY's named low tier, **I want** a way to make LineSweeperFrameBench draw on the Ryzen 9000 iGPU, and a way to know the result was not taken across adapters, **so that** the low-tier p99 can be taken at all on hardware I already have.

S · code · E11 Performance and hardware claims backed by a measurement · gaps G01 · **Next** · **Owner**

**Acceptance**

- Given the desktop's integrated Radeon (PCI 1002:13C0) on the Microsoft Basic Display Adapter driver, When the owner installs AMD's driver, Then Device Manager names the Radeon and the driver version is recorded (step one, XS)
- Given the driver, When Windows' per-app graphics preference sets LineSweeperFrameBench.exe to power saving and the display is attached to the iGPU's output, Then the bench's result JSON records the created device as the Radeon iGPU (bench/linesweeper_frame_result.cpp:182-236) for each rasterising backend tried, with no code change
- Given the procedure document, When read, Then it states that cross-adapter presentation invalidates the run, cites the ~125 ms/frame and GDI evidence at docs/performance/2026-09-19-g6f-reference.md:209-212, and says how to confirm the display is on the iGPU
- Given the OS preference fails for a backend, When a code option is argued, Then it is split into its own story, read only at device creation or launch (a bench option or environment variable), and no signature in engine/render/renderer.h changes

**Trade-offs**

- PHILOSOPHY.md:677-690 (the low tier): the named configuration is the 1002:13C0 iGPU on a real driver, which holds only after the AMD driver replaces the Microsoft Basic Display Adapter; installing it is step one
- T1: no engine mechanism without demand: install the driver, then try Windows' per-app GPU preference (no code); a bench --adapter option is argued only if that fails
- T8: any code option is read at device creation or launch, never a renderer.h signature or a per-frame choice ('flexibility that survives lives at load and wiring time, never in the loop'); the low-tier p99 is the measurement the performance positioning rests on

**Notes.** Critique: the iGPU runs on the Microsoft Basic Display Adapter until the AMD driver is installed, so neither an OS preference nor a code option yields a hardware adapter before that. The cheap first step (driver plus per-app preference, no code) moves this story to Next beside SM-03; the seam argument is conditional on it failing. Each backend prefers the discrete GPU: d3d11/device_resources.cpp:472, d3d12/device_resources.cpp:150, vulkan/device_resources.cpp:628-629, gl/renderer.cpp:68-85 (GL follows the adapter that owns the display). Blocks SM-07.

<a id="sm-02"></a>
### SM-02. Reproduce the four-core hold, and say what the bench's fan-out measures

**As** a sole maintainer taking the low-tier measurement, **I want** a launch line that holds the bench to four cores, and a definition that agrees with what the bench actually exercises, **so that** the low tier is reproducible exactly as PHILOSOPHY defines it.

S · docs · E11 Performance and hardware claims backed by a measurement · gaps G25 · **Later**

**Acceptance**

- Given the bench run procedure, When I read it, Then it contains a concrete command that launches LineSweeperFrameBench with a four-core affinity (e.g. start /affinity with a stated mask) and says how to confirm the hold
- Given PHILOSOPHY.md:691-693 says the hold changes how many workers the fan-out gets, When I compare it with bench/linesweeper_frame_bench.cpp (one Scene, no pool), Then either the bench builds its Scene with a pool or PHILOSOPHY is amended in the same commit to say the bench measures the single-view path
- Given the change, When the top-level build runs, Then check_doc_citations passes

**Trade-offs**

- PHILOSOPHY.md:645-661: the fan-out is opt-in and the dial is the client's; LineSweeper's bench builds one Scene with no pool and takes the serial path, so amend :691-693 to say the four-core hold bounds a single-view frame rather than adding a pool the sample does not use
- PHILOSOPHY.md:662-667: that passage refuses 'no caller in this tree' as an argument, so this story changes only what the bench claims to measure, not whether the fan-out mechanism stays (local_multiplayer now reaches it)
- T8: the low-tier p99 is the measurement the performance identity rests on, so its definition must match what the bench actually runs

**Notes.** engine/scene/scene.cpp:187-191 is the early-out taken when there is no pool. Design docs change by amendment in the same commit.

<a id="sm-03"></a>
### SM-03. Decide which p99 the pipeline-depth claims are judged by

**As** a sole maintainer, **I want** a recorded decision on whether the record + submit p99 satisfies PHILOSOPHY's 'measured p99', or whether the one-clock bench change must land first, **so that** the first low-tier run produces a number I can quote rather than one I must re-argue.

XS · process · E11 Performance and hardware claims backed by a measurement · gaps G26 · **Next** · **Owner**

**Acceptance**

- Given docs/performance/2026-09-19-g6f-ratchet.md §5 and PHILOSOPHY.md:696-698, When I read either, Then one of them states which column (record + submit, update, or whole-frame after a one-clock change) counts for each pipeline-depth claim, and the other cites it
- Given the decision, When I read docs/review/backend-equivalence-2/STATUS.md's low-tier box, Then it names the chosen column
- Given the decision rejects an unsynchronised present mode, Then it says so with the T5/T8 reason, or if it adopts one, it names SEAM.md as needing amendment

**Trade-offs**

- T8: an unsynchronised present mode is a run-time customisation on the frame path, so it has to pay for itself and is a seam change to argue; choosing --pacing presentation needs no code
- PHILOSOPHY.md:691-699 (Performance): the pipeline-depth claims are latency and need a measured p99; until one exists no number is a floor
- PHILOSOPHY.md:700-706 (Performance): the bench says what each phase measures and calls none of it scan-out, so a quoted p99 must name its clock

**Notes.** A decision only, with no code, unless the one-clock option is chosen; that option would be a separate story.

<a id="sm-04"></a>
### SM-04. A local run procedure for LineSweeperFrameBench

**As** a sole maintainer or one of their agents, **I want** the exact desktop command lines for a frame-bench run, separate from the EC2 lane, **so that** a local run is repeatable without reverse-engineering the cloud scripts.

S · docs · E11 Performance and hardware claims backed by a measurement · gaps G63 · **Later**

**Acceptance**

- Given the bench documentation, When I read it, Then it lists the required --output, --run and --release-hash arguments, the sample count, the pacing choice, and the presets to build (at minimum x64-release-d3d12 and x64-release-vulkan)
- Given a Release build on the desktop, When an agent follows the procedure verbatim, Then it produces a result JSON with no undocumented step

**Trade-offs**

- PHILOSOPHY.md:700-710 (Performance): absolute frame timing is a declared hardware experiment outside CTest, tied to one named machine, driver, build and display session, so a local run is a procedure that records those, not a copy of the EC2 lane
- T1 (not a licence for speculative frameworks): no local lane beyond what the one run needs

**Notes.** Sources: bench/linesweeper_frame_result.cpp:236-240, bench/CMakeLists.txt:45-86, tools/cloud_performance/README.md:328-331, samples/linesweeper/README.md:561-566. It can carry SM-02's affinity line.

<a id="sm-05"></a>
### SM-05. Apply the ratchet's lock verdict to a non-NVIDIA local run

**As** a sole maintainer measuring on a Radeon, **I want** the presentation-lock verdict and pooled summary for a local result JSON, **so that** a Radeon run is read with the same rule that explained the EC2 long waits.

S · code · E11 Performance and hardware claims backed by a measurement · gaps G62 · **Later**

**Acceptance**

- Given a result JSON from a host with no nvidia-smi and a non-NVIDIA vendor id, When I run the documented analysis command, Then it prints the lock verdict and pooled p99s without raising
- Given tools/tests, When python -m unittest discover tools/tests runs, Then a case feeds a non-NVIDIA result and asserts the verdict

**Trade-offs**

- T1: factor out only the lock-verdict function, not a general local lane

**Notes.** The host attestation raises at tools/cloud_performance/analysis.py:254-259, 364-397 and 455-456.

<a id="sm-06"></a>
### SM-06. Stop the low-tier STATUS box sending me after a fixed contradiction

**As** a sole maintainer who treats STATUS.md as the one live ledger, **I want** the low-tier box to describe only what is actually open, **so that** I and my agents do not hunt for an edit that has already been made.

XS · docs · E11 Performance and hardware claims backed by a measurement · gaps G64 · **Now**

**Acceptance**

- Given docs/review/backend-equivalence-2/STATUS.md:528-535, When I read the box, Then it no longer claims samples/linesweeper/README.md contradicts itself 18 lines apart
- Given the box, Then it still names the unnamed CPU half and the missing p99, and points at the instruments it depends on (SM-01 to SM-05)

**Trade-offs**

- PHILOSOPHY.md:752-758 (The public face): a decision resting on a measurement has to be re-taken when the measurement moves, not re-cited; the box must follow the fixed contradiction

**Notes.** The two current README passages (461-463, 554-566) agree. The box is unticked until SM-07.

<a id="sm-07"></a>
### SM-07. Take and publish the low-tier p99

**As** a sole maintainer whose positioning is constrained hardware, **I want** a recorded frame-time result on the named low tier, **so that** the speed claim and the pipeline-depth claims rest on a measurement, not an intention.

M · verification-hardware · E11 Performance and hardware claims backed by a measurement · gaps G24 · **Later** · **Owner**

**Acceptance**

- Given SM-01 to SM-05 are done, When I run the bench on the Radeon iGPU under the four-core hold for D3D12 and Vulkan, Then a docs/performance/<date>-low-tier.md exists recording device string, affinity, preset, commit, sample count and the p99 of the column chosen in SM-03
- Given the result, When I read STATUS.md's low-tier box, Then it is ticked with a link to that document, or states the specific failure
- Given the result document, Then it says what it does not measure (scan-out, per CLAUDE.md's bench paragraph)

**Trade-offs**

- PHILOSOPHY.md:677-699 (Performance): the low tier is the named 1002:13C0 configuration, the two pipeline-depth claims need a measured p99 on it, and until then no number is a floor
- PHILOSOPHY.md:700-710 (Performance): an EC2 reference profile is not the low tier
- T8: speed is part of the engine's identity, which is why the low-tier claim has to be proven on the low tier, not only sized against it

**Notes.** Depends on SM-01 to SM-05. Absolute timing stays out of ctest (PHILOSOPHY.md:700-706). Publishing the figures on why.mdx is the adopter's story.

<a id="sm-08"></a>
### SM-08. A method for judging browser pacing (R5) and timing the phone top-out frame (R6)

**As** a sole maintainer who will one day hold the right displays and phones, **I want** a defined, repeatable way to observe and record judder and the 9,600-particle frame time in a browser, **so that** results taken on different days and devices are comparable rather than ad hoc.

S · docs · E11 Performance and hardware claims backed by a measurement · gaps G27 · **Later**

**Acceptance**

- Given docs/port/web-to-be-tested.md R5/R6 rows, When I read them, Then each names the instrument (e.g. a development-page timing readout behind a query flag, or a stated DevTools procedure), what counts as judder, and a results table template
- Given the instrument is code, When a wasm-release dev page is loaded with the flag in headless Chrome, Then it logs per-frame intervals and the top-out frame time to the console, and LineSweeperPageTests still passes

**Trade-offs**

- T1: a page-side measurement or protocol, not a browser benchmark harness
- T8: pacing is a performance claim

**Notes.** LineSweeperFrameBench is Windows-only (bench/CMakeLists.txt:42-46). See web.md:455-461, 1060-1063 and 1603-1608.

<a id="sm-09"></a>
### SM-09. Record the MacBook Air's Safari ceiling, and play the demo in it if it can

**As** a sole maintainer planning the Safari milestone, **I want** the highest macOS and Safari the 2013 MacBook Air can run, recorded, **so that** I know whether M3 can close on owned hardware before spending time on it.

S · docs · E11 Performance and hardware claims backed by a measurement · gaps G28, G23 · **Next** · **Owner**

*Also serves:* The Browser and Phone Demo Player (merged from BDP-05)

**Acceptance**

- Given the 2013 MacBook Air on Big Sur, When Apple's own software update is run to its end, Then docs/port/web.md section 9 records the exact highest macOS and Safari versions it takes, with that source (the brief's 'Safari 14-ish' is confirmed or corrected: Big Sur received later Safari releases)
- Given that version, When compared with the wasm-exceptions floor (cmake/settings.cmake:75-77; web-to-be-tested.md:29-32's 18.4 note, and the legacy exceptions the build emits), Then the record says whether it meets it
- Given it is below the floor, When recorded, Then web.md section 9 says 'out of reach on owned hardware' and the play criteria below are closed as blocked with that reason
- Given it meets the floor, When a person plays one match on the live URL or the bundle page, quits and plays again, Then the Safari and macOS versions and the result are recorded under 'Needs Safari' in web-to-be-tested.md, and a failure includes the console output captured from Safari's Web Inspector
- Given any pass, When it is recorded, Then the record says it is evidence for that Safari version only, not current Safari

**Trade-offs**

- PHILOSOPHY.md:685-688 (Performance): naming hardware nobody can boot leaves every number where it was while reading as though something had been decided, so say plainly whether the Big Sur MacBook Air's Safari can settle M3's Safari check
- docs/port/web.md M3 done-when (and CLAUDE.md): Safari must be checked before M3 is called done
- PHILOSOPHY.md:691-699 (by analogy): sized against is not proven on; a pass on an older Safari under Big Sur is evidence for that version only

**Notes.** Merged from BDP-05: SM-09 records the ceiling and BDP-05's play criteria run if the ceiling allows (G28, and G23's Safari item). Owner fact: the machine stops at Big Sur. Moved to Next before OCA-28 (critique), so the browser list can say 'checked on Safari X' or 'out of reach on owned hardware' rather than 'unchecked'. Records a fact; does not propose buying hardware.

<a id="sm-10"></a>
### SM-10. Make web.md's Firefox claim match what was actually checked

**As** a sole maintainer, **I want** §9 and the checklist to state Firefox coverage as it is, **so that** I do not read M3 as closer than it is.

XS · docs · E11 Performance and hardware claims backed by a measurement · gaps G69 · **Now**

**Acceptance**

- Given docs/port/web.md:839, When I read it, Then it no longer says M3 'is met in Chrome and Firefox' unless every page's Firefox row in web-to-be-tested.md is ticked
- Given docs/port/web-to-be-tested.md:102-106, Then its Firefox list includes the components gallery and the count of *PageTests is six

**Trade-offs**

- None of T1-T12: documentation accuracy; cf. PHILOSOPHY.md:685-688, a claim must not read as though something had been decided when it was not

**Notes.** web-to-be-tested.md:12-14 also still says 'the five *PageTests'.

<a id="sm-11"></a>
### SM-11. Run the six sample pages in Firefox without hand-driving each

**As** a sole maintainer with no time to repeat a manual Firefox pass per change, **I want** a repeatable Firefox check of the six sample pages, **so that** a Firefox-only regression is found before a visitor finds it.

M · build-ci · E11 Performance and hardware claims backed by a measurement · gaps G70 · **Won't this time**

**Acceptance**

- Given a wasm-release build and a Firefox install, When I run the documented command or procedure, Then each of the six pages is loaded, its frames observed and onQuit reached, with pass/fail per page
- Given the result, Then a dated row is recorded in web-to-be-tested.md
- Given the automated path is chosen, Then its T1 argument (the regression it would have caught) is written beside it; otherwise the procedure is a written manual checklist

**Trade-offs**

- T1 (not a licence for speculative frameworks): a second runner protocol (WebDriver BiDi) needs a regression it would have caught
- None of T1-T12 for the fallback: a written checklist is a process choice, argued on cost

**Notes.** run_page.mjs drives only the Chrome DevTools Protocol. Safari stays manual (SM-09).

<a id="sm-12"></a>
### SM-12. Make the records say the demo is live, and tick its server-side deploy checks against it

**As** a sole maintainer who deployed the demo, **I want** the M5 deploy checks run against the live URL and the stale 'not deployed' records amended, **so that** neither I nor a journey walk reads a blocker that does not exist, and the real open checks are visible.

S · verification-hardware · E12 A maintainer's process, CI and deploy that one person and their agents can trust · gaps G05 · **Now**

*Also serves:* The Browser and Phone Demo Player (merged from BDP-02)

**Acceptance**

- Given docs/port/web.md:616-618, :1461 and docs/website-plan.md:90, When the change lands, Then none says the demo is not deployed, and web.md section 9 records it as live with the date checked
- Given the live URL, When curl -I is run with Accept-Encoding: br against the page and against linesweeper.wasm, Then Content-Type, both Cache-Control values (immutable under m/<hash>/ for the wasm, no-cache for the page) and the Content-Encoding served are pasted beside web-to-be-tested.md:88-91, and the box is ticked when the type is application/wasm and both Cache-Control values match
- Given Chrome started with --disable-webgl2 on the live URL, When Play is clicked, Then the no-WebGL2 message appears and :95 is ticked with the run recorded
- Given the phone-fallback item (:94), When run under Chrome's touch emulation, Then the network log shows no request for linesweeper.js or linesweeper.wasm, the run is recorded as emulated, and the real-phone half stays open, linked to SM-13
- Given the checklist's deploy step, When read, Then it names PersonalWebsite main plus 'npm run deploy', not the deleted linesweeper-demo branch
- Given every other check in :86-100, When this lands, Then each is ticked with evidence, marked failed with what was seen, or marked blocked naming what it waits on (Safari: SM-09; a real phone: SM-13; a GPU reset: BDP-06)

**Trade-offs**

- PHILOSOPHY.md:752-758 (The public face): when the state a record rests on moves (the demo is live), the record is re-taken, not re-cited
- docs/port/web.md section 7 and :1462: M5's done-when is a live URL; the deploy checks at web-to-be-tested.md:86-100 are recorded against it

**Notes.** Merged from BDP-02 (same checklist against the same live URL, G05). Critique split: the server-side checks are ticked now; the phone item is checked under emulation and labelled so, its real-phone half linked to SM-13. The deploy itself was the owner's (PersonalWebsite main plus npm run deploy) and has happened; this story needs only the URL. G52 (web.md wrongly says a bad MIME type stops the demo) stays with OCA-23; this check records the header either way.

<a id="sm-13"></a>
### SM-13. Record a real iPhone run of the development page and the live demo

**As** a sole maintainer closing M7b, **I want** the owner's iPhone to have played the development page and the live demo, with results recorded, **so that** the phone gate in index.html has half the evidence it waits on, and the iPhone half of M7b is closed on owned hardware.

M · verification-hardware · E09 The hosted demo is playable, understandable and kind to every player · gaps G34, G05 · **Later** · **Owner**

*Also serves:* The Browser and Phone Demo Player (merged from BDP-08)

**Acceptance**

- Given the development page (wasm-release shell.html build) served over HTTPS to the owner's iPhone in Safari, following SM-14's LAN-serve and remote-inspector procedure, When a person plays a match by touch in portrait and in landscape, Then the device, iOS version, Safari version and outcome are recorded against web-to-be-tested.md:63-73, and the boxes are ticked or a gap opened
- Given the iPhone, When the page loads, Then time to first frame and the tab's memory from the remote inspector are recorded; and at the top-out frame, R6 (whether it holds frame rate, by SM-08's method) and the legibility of the CPU-decoded atlas text are recorded
- Given the iPhone on the live mattharding.dev page, When it loads, Then the remote inspector's network log shows the poster loading and no linesweeper.wasm request, and the iPhone half of web-to-be-tested.md:94 is ticked
- Given the record, When read, Then it says plainly that it is evidence for iOS Safari on that device and version only, and that the Android half (SM-44) is still open

**Trade-offs**

- PHILOSOPHY.md:685-699 (Performance): an unmeasured claim must not read as decided; until real-phone runs exist M7b's gate stays open
- PHILOSOPHY.md:688-699 (Performance): a software rasteriser fails in a different shape from real hardware, so phone behaviour that has run only in headless Chrome and SwiftShader is sized against phones, not proven on them
- T6: the gate is honest while this is open: a loud refusal rather than limping on a device nobody has checked

**Notes.** Merged from BDP-08 (same real-phone session, G34, and the real-phone half of G05's :94). Owner facts 2026-10-13: an iPhone is owned and no Android phone, so the story is split: this iPhone run leaves 'deferred' and sits in Later behind SM-14 and SM-08; the Android run is SM-44 and stays deferred until a device is borrowed. BDP-10 (lifting the gate) needs both unless the owner decides one platform's evidence is enough.

<a id="sm-14"></a>
### SM-14. Serve a development page to a phone on my network

**As** a sole maintainer with a phone in hand, **I want** a documented way to serve a built page to a LAN device and see its console, **so that** phone and audible-audio checks can catch the console errors the page tests rely on.

S · docs · E09 The hosted demo is playable, understandable and kind to every player · gaps G71 · **Later** · **Owner**

**Acceptance**

- Given the documentation, When I read it, Then it gives a LAN-bound serve command, the secure-context rule for a LAN IP and how to satisfy it (HTTPS), and remote-debug steps for iOS Safari (tried) and Android Chrome (written, untried until SM-44)
- Given a wasm-release build, When the owner follows it with the iPhone, Then MinimalSample.html loads on the phone on the same network and its console is visible in desktop Safari's Web Inspector

**Trade-offs**

- None of T1-T12: documentation of a local procedure; cf. PHILOSOPHY.md:747-750, what no build step can enforce is a discipline, which is why it is written down

**Notes.** Written in SM-13's iPhone session so the procedure is tried rather than imagined. Owner fact: an iPhone is owned, so this leaves 'deferred'; the Android steps stay untried until SM-44. Remote inspection of iOS Safari needs a Mac; the 2013 MacBook Air is that Mac. The only recipe today is python -m http.server, in samples/audio/README.md:69-72.

<a id="sm-15"></a>
### SM-15. Observe the null-backend wasm step running after a failed page step, and the job going red

**As** a sole maintainer, **I want** a scripted way to make a page step fail on a throwaway branch, **so that** the open CI box can be ticked instead of waiting indefinitely.

XS · process · E12 A maintainer's process, CI and deploy that one person and their agents can trust · gaps G72 · **Later**

**Acceptance**

- Given web-to-be-tested.md:15-16, When I read it, Then it names the deliberate failure to introduce and the throwaway-branch procedure
- Given that procedure is run, Then one linked CI run shows a page step failed, the 'Configure, build and test with the null backends' step (ci.yml:387-394, guarded by the if: at :388) still ran, and the wasm job concluded failure, and the box is ticked with that run's link

**Trade-offs**

- T6: quiet failure is the most expensive kind; a null-backend step that silently does not run after a failed page step, or a job that stays green, would be one, so it is observed going red

**Notes.** Delete the throwaway branch afterwards; nothing merges.

<a id="sm-16"></a>
### SM-16. One index of reviews and which ledger is live

**As** a sole maintainer steering agents through several reviews, **I want** a single file that lists every review, whether it is frozen, where its live status lives and what is still open, **so that** neither I nor an agent relies on CLAUDE.md prose or private memory to know what is open.

S · docs · E12 A maintainer's process, CI and deploy that one person and their agents can trust · gaps G29 · **Next**

**Acceptance**

- Given a new docs/review/INDEX.md, When I read it, Then it lists every entry of docs/review (2026-09-26, backend-equivalence, backend-equivalence-2, d3d12, gpt6, round-2, rtcd, vulkan, and the root README.md as the historical ArtAttack review) with its status file or 'frozen, nothing open'
- Given d3d12 §6, vulkan §7 and vulkan's two open §6 items, Then each is tracked in a STATUS.md or in the index itself
- Given CLAUDE.md and AGENTS.md, Then their review map points at docs/review/INDEX.md
- Given docs/review/README.md, Then it is unchanged (historical)

**Trade-offs**

- CLAUDE.md, Reading the documents correctly: one STATUS.md is the live ledger, ticked in the same commit as the change; the reviews are historical (not a T1-T12 trade-off)

**Notes.** docs/review/README.md is the frozen ArtAttack review and must not become the index. check_doc_citations will check the index's links.

<a id="sm-17"></a>
### SM-17. One command per area that runs every configuration a change must pass

**As** a sole maintainer landing a render change, **I want** to run every affected preset from one command, with the mapping from paths to presets written down, **so that** the five-backend-plus-wasm sweep is not done from memory.

M · build-ci · E12 A maintainer's process, CI and deploy that one person and their agents can trust · gaps G30 · **Later**

**Acceptance**

- Given CMakePresets.json, When I run the documented workflow or script for engine/render, Then it configures, builds and tests the five Windows render presets and a wasm preset, stopping on the first failure and naming it
- Given a table in CLAUDE.md (and AGENTS.md) mapping engine/ subfolders to required presets, When I read it, Then gl/ changes list the wasm presets
- Given the CI null-backend wasm configure (ci.yml:387-394), Then it is expressible as a preset or the script runs it
- Given a shell missing VULKAN_SDK or EMSDK, When I run the script, Then it fails before building and names the missing variable

**Trade-offs**

- T1: workflowPresets over existing configurations serve a present caller; no framework
- T6: a missing environment (VULKAN_SDK, EMSDK, fxc, a Chromium) fails loudly, naming what is missing
- T5: the build is the cheapest reviewer and the only tireless one

**Notes.** Each configuration needs its own environment (vcvars, VULKAN_SDK, EMSDK). A new preset means updating CLAUDE.md's preset table.

<a id="sm-18"></a>
### SM-18. A record of the configurations CI cannot run, per merged change

**As** a sole maintainer, **I want** evidence that GL and Vulkan pixel tests, release presets and wasm-debug passed for a commit, **so that** 'green on every configuration' can be checked after the fact.

S · process · E12 A maintainer's process, CI and deploy that one person and their agents can trust · gaps G31 · **Later**

**Acceptance**

- Given a merged PR touching engine/render, When I open it, Then its description carries a ctest summary per preset for those outside CI (x64-debug-gl and x64-debug-vulkan RenderPixelTests, the release presets, wasm-debug) or says which were not run
- Given .github, Then a pull-request template prompts for that section

**Trade-offs**

- PHILOSOPHY.md:704-706 (Performance): device-bound evidence belongs to one named machine, driver and session, so it is recorded per change rather than gated in portable CI
- None of T1-T12 for the cost choice against a paid GPU runner

**Notes.** Could be SM-17's script output pasted into the PR. See ci.yml:33-40, 58-66, 95-113 and 284-297.

<a id="sm-19"></a>
### SM-19. Gate the offline tooling tests in CI

**As** a sole maintainer, **I want** tools/tests to run on every push, **so that** a break in the performance or bundle tooling is caught when it happens, and the review's priority 3 is closed on the ledger.

XS · build-ci · E12 A maintainer's process, CI and deploy that one person and their agents can trust · gaps G32 · **Next**

**Acceptance**

- Given ci.yml, When a push runs, Then a step runs python -m unittest discover over tools/tests
- Given a deliberately failing test on a throwaway branch, Then that job is red
- Given docs/review/2026-09-26/STATUS.md, Then a row tracks README priority 3 and is ticked

**Trade-offs**

- T9: standard-library unittest adds no supply chain
- T6

**Notes.** Today the command appears only in FIXES.md:106. tools/tests imports only the standard library and tools.cloud_performance. It ships worker_failure_fixture.ps1, so run the step on the Windows runner.

<a id="sm-20"></a>
### SM-20. Write the merge procedure into the tree

**As** a sole maintainer briefing agents and any future contributor, **I want** the branch, PR and rebase-merge-on-green procedure stated in a tracked file, **so that** an agent learns it from the repository rather than from my memory.

XS · docs · E12 A maintainer's process, CI and deploy that one person and their agents can trust · gaps G65 · **Now**

**Acceptance**

- Given CONTRIBUTING.md (or a CLAUDE.md/AGENTS.md section), When I read it, Then it states: branch from master, open a PR, rebase-merge on green CI, master protected, one self-contained commit per change
- Given grep for 'rebase' in CLAUDE.md, README.md and CONTRIBUTING.md, Then it matches

**Trade-offs**

- PHILOSOPHY.md:747-750 (The public face): what no build step can enforce is a discipline, which is why it is written down

**Notes.** Could share a file with the question route (G21) and SM-42.

<a id="sm-21"></a>
### SM-21. Required checks that survive a workflow edit

**As** a sole maintainer editing CI, **I want** required status checks that do not silently rename or go missing when I change a matrix value, and a written note of how protection is configured, **so that** a CI edit cannot leave every PR waiting on a context that never reports, and two PRs cannot each pass and leave master red.

S · build-ci · E12 A maintainer's process, CI and deploy that one person and their agents can trust · gaps G134 · **Now** · **Owner**

**Acceptance**

- Given ci.yml, When I change a skip_tests or vulkan_sdk value, Then the check context names are unchanged (e.g. a stable matrix name field)
- Given website.yml, Then its job id is unique rather than 'build'
- Given a tracked document, When I read it, Then it lists the required contexts, the strict (up-to-date) setting and its reason, and the rule that adding a matrix entry means adding its context
- Given gh api repos/matt439/Labrador/branches/master/protection, Then its contexts match that list

**Trade-offs**

- T6: a check that cannot report should fail visibly, not wait forever

**Notes.** With strict false, a PR that breaks a quote and a PR that adds the quote can both pass. Decide whether to enable strict, or accept it and write down why.

<a id="sm-22"></a>
### SM-22. One agent brief, not two copies

**As** a sole maintainer amending agent instructions, **I want** CLAUDE.md and AGENTS.md to be unable to drift apart unnoticed, **so that** every amendment is made once and both agents read the same rules.

XS · build-ci · E12 A maintainer's process, CI and deploy that one person and their agents can trust · gaps G66 · **Now**

**Acceptance**

- Given either option, When the two files differ anywhere below line 1 (the title), Then the top-level build fails naming both files; or AGENTS.md is a pointer to CLAUDE.md that duplicates no rules
- Given the current tree, Then the build passes
- Given tools/cloud_performance/release.py:19 lists both files, Then python -m unittest discover tools/tests still passes

**Trade-offs**

- T4 (not a licence for copy-paste): duplication is the opposite of clarity at scale, so one brief and a pointer, not two copies
- T5: a check in the style of check_engine_includes; the build is the cheapest reviewer and the only tireless one
- T6: when the copies drift, the check names the file

**Notes.** Today the files differ only on line 1 ('notes for Claude' against 'notes for Codex'); both are 602 lines.

<a id="sm-23"></a>
### SM-23. Leave a trace that regenerated goldens were looked at

**As** a sole maintainer delegating to agents, **I want** a visible record, per commit, of which golden images changed and that each was reviewed, **so that** the pixel contract does not become a recording of whatever the code does now.

S · build-ci · E12 A maintainer's process, CI and deploy that one person and their agents can trust · gaps G67 · **Later**

**Acceptance**

- Given CLAUDE.md and AGENTS.md, When I read the golden paragraph, Then it states the convention: a commit changing a file in tests/render/golden/ names each changed image in its message with a line saying it was looked at
- Given a PR whose commit changes tests/render/golden/*.png without naming each changed file in that commit's message, When CI runs, Then a check fails naming the unlisted images
- Given a PR that names every changed image, Then the check passes

**Trade-offs**

- CLAUDE.md, RenderPixelTests: review every image a regeneration changes; one that is not looked at turns the contract into a recording (not a T1-T12 trade-off)
- T5: a small diff-and-grep check makes the build or CI the reviewer, with no image tooling
- T6: an unreviewed regeneration fails visibly, naming the images

**Notes.** CI uploads golden-actual only on failure (ci.yml:202-211, 363-368). The check reads git diff --name-only and the commit messages over the PR range.

<a id="sm-24"></a>
### SM-24. A written procedure for moving dependency and toolchain pins

**As** a sole maintainer, **I want** one place that lists every pin, how to move it, and what to re-run afterwards, **so that** a bump is deliberate and fully verified rather than a surprise or a guess.

S · docs · E12 A maintainer's process, CI and deploy that one person and their agents can trust · gaps G115 · **Later**

**Acceptance**

- Given the procedure document, When I read it, Then it names the vcpkg builtin-baseline (vcpkg.json:15), emsdk (ci.yml:250 and CLAUDE.md), the runner image, Node and the website's npm dependencies, and the Vulkan SDK
- Given each pin, Then it states the bump steps and the re-run list (all five backends, the golden set, the wasm pages, the website build)
- Given the procedure, Then it says a baseline move changes what an adopter's derived manifest pins

**Trade-offs**

- PHILOSOPHY.md:747-750 (The public face): what no build step can enforce is a discipline, which is why it is written down
- T9: the pinned dependencies are the bought platform and format edge, so moving them is deliberate (a bump bot would be a tooling choice to argue on its own terms, not a T9 question)

**Notes.** The baseline has not changed since 7f25d0d. The Vulkan SDK pin itself is SM-25.

<a id="sm-25"></a>
### SM-25. Pin the Vulkan SDK, record the browser, and detect drift on a schedule

**As** a sole maintainer, **I want** CI inputs pinned like emsdk is, the browser version recorded, and a scheduled run, **so that** an external change shows up as its own red run, not on an unrelated PR.

S · build-ci · E12 A maintainer's process, CI and deploy that one person and their agents can trust · gaps G133 · **Next**

**Acceptance**

- Given ci.yml, When I read the Vulkan job, Then the SDK URL names an exact version, not /latest/ (ci.yml:165)
- Given a wasm page test run, Then run_page.mjs logs the browser product and version (e.g. from CDP Browser.getVersion) beside the renderer string
- Given ci.yml, Then it has a schedule: trigger (e.g. weekly), and a scheduled run appears in the Actions history

**Trade-offs**

- T6: drift must fail where it can be blamed, naming the tool and version

**Notes.** The pinning rationale is already written for emsdk at ci.yml:236-241. Chrome on windows-latest may not be pinnable; recording its version is the minimum.

<a id="sm-26"></a>
### SM-26. Notice when the hosted demo lags the engine

**As** a sole maintainer who deploys the demo by hand, **I want** a stated re-sync trigger and a quick way to see how far the live bundle is behind master, **so that** the public demo does not silently lag fixes.

S · process · E12 A maintainer's process, CI and deploy that one person and their agents can trust · gaps G68 · **Later**

**Acceptance**

- Given web.md §7, When I read it, Then it states when the host must be re-synced (e.g. any change under samples/linesweeper or engine/ that a wasm build includes)
- Given the documented command, When I run it, Then it reads the deployed BUILD.json commit (from the live URL, or from the host repository's committed public/demo/linesweeper/BUILD.json), says which source it read, and prints how many commits that commit is behind master

**Trade-offs**

- T1: a documented check, not an automated cross-repo deploy (web.md §7)

**Notes.** According to PersonalWebsite's public/demo/linesweeper/BUILD.json, the live bundle was built from e7888bd, 23 commits behind master. Nobody has checked whether BUILD.json is served at the live URL.

<a id="sm-27"></a>
### SM-27. Verify a web bundle before deploying it, from this repository

**As** a sole maintainer deploying from any machine, **I want** an in-tree check that refuses a dirty, Debug, incomplete or mismatched bundle, **so that** a safe deploy does not depend on a script that lives only in my website's repository.

S · code · E08 Ship my game to a web page · gaps G144 · **Later**

**Acceptance**

- Given a web-dist/linesweeper directory, When I run the documented verify command (e.g. node tools/web_bundle/bundle.mjs --verify <dir>), Then it exits non-zero for build_type Debug, dirty true, a missing file, or a file whose hash disagrees with BUILD.json, and zero for a clean Release bundle
- Given out/build/wasm-debug/web-dist/linesweeper, Then verify rejects it
- Given web.md §7, Then it states every rule the verify applies

**Trade-offs**

- T6: refuse loudly rather than deploy a Debug bundle
- T1: a verify mode, not a deploy pipeline

**Notes.** Today the rules live in PersonalWebsite scripts/sync-demo.mjs:11-12 and 61-82. Optionally, stop linesweeper_web_dist being part of ALL under wasm-debug (samples/linesweeper/CMakeLists.txt:118-151). An adopter who copies the shape is the outside-cpp-adopter's value.

<a id="sm-28"></a>
### SM-28. Catch a website accessibility regression in CI

**As** a sole maintainer whose site is built partly from engine files, **I want** the WCAG checks I ran by hand to run on every website build, **so that** a colour, theme upgrade or engine-side commit cannot ship an inaccessible page green.

M · build-ci · E12 A maintainer's process, CI and deploy that one person and their agents can trust · gaps G120 · **Won't this time**

**Acceptance**

- Given website.yml, When it runs, Then a step scans the built dist/ with axe-core at WCAG 2.2 A/AA in light and dark themes
- Given a throwaway branch that adds a colour failing contrast in site.css, Then the website job is red
- Given website/README.md, Then it states how to re-run the scan locally and the date and commit of the last full run

**Trade-offs**

- T5: a manual WCAG check becomes a build failure; the build is the cheapest reviewer
- T9: accessibility scanning is edge tooling where reimplementation teaches nothing, so axe-core is bought, and the only cost to weigh is CI time

**Notes.** The manual runs are recorded at docs/website-plan.md:131, 141-147 and 155-159. If a headless scan costs too much, the fallback is the written procedure alone.

<a id="sm-29"></a>
### SM-29. A procedure for previewing and rolling back the site

**As** a sole maintainer with production deploys on every master commit, **I want** the preview-before-production and rollback rules turned into steps, or the plan amended to match practice, **so that** a bad deploy can be prevented or undone without working it out under pressure.

S · docs · E12 A maintainer's process, CI and deploy that one person and their agents can trust · gaps G135 · **Later**

**Acceptance**

- Given website/README.md, When I read it, Then it says how to review website.yml's dist artifact before merging, how to roll back (dashboard or wrangler rollback), and what to check on the live site after deploy
- Given docs/website-plan.md:132 and :218, Then they agree with that procedure or are amended

**Trade-offs**

- No trade-off engaged: a website operations procedure; docs/website-plan.md:132 and :218 are the plan text to reconcile with website/README.md:194-201

**Notes.** Cloudflare preview builds are off (website/README.md:201).

<a id="sm-30"></a>
### SM-30. Change the website gate in one place

**As** a sole maintainer, **I want** the Cloudflare production build to run the same gate as website.yml from a file in the tree, **so that** a gate change cannot be made in CI and forgotten in the dashboard that actually deploys.

S · build-ci · E12 A maintainer's process, CI and deploy that one person and their agents can trust · gaps G136 · **Later** · **Owner**

**Acceptance**

- Given the change, When I read website/package.json, Then one npm script is the whole gate, and website.yml calls it
- Given website/README.md, Then the Cloudflare build command is recorded as that one script, and the Node version is read from a tracked file both use
- Given a new check added to that script, Then no dashboard edit is needed for production to run it

**Trade-offs**

- T4: one gate definition; a gate duplicated between website.yml and the dashboard is the duplication T4 calls the opposite of clarity

**Notes.** The dashboard settings exist only as prose, at website/README.md:194-201. The workers.dev half of G136 is SM-31.

<a id="sm-31"></a>
### SM-31. One canonical host for the engine site

**As** a sole maintainer, **I want** the site to declare its URL and be served on one host, **so that** canonical URLs and the sitemap exist and a duplicate workers.dev copy does not compete with it.

XS · code · E12 A maintainer's process, CI and deploy that one person and their agents can trust · gaps G57, G136 · **Now** · **Owner**

**Acceptance**

- Given website/astro.config.ts, Then it has a site key, and built pages carry rel=canonical and a sitemap is emitted
- Given website/wrangler.jsonc, Then workers_dev is false, or the README records why it stays true and the canonical tag points at the real host

**Trade-offs**

- No trade-off engaged: site configuration; the open follow-up is website/README.md:214-218 (G57)

**Notes.** G57 is an open follow-up at website/README.md:214-218. Adopters who find the docs benefit equally; it is one change.

<a id="sm-32"></a>
### SM-32. Keep the site's LineSweeper footage in step with the sample

**As** a sole maintainer, **I want** a rule, and a current set, so stills and video show the sample as built, **so that** the showcase does not show text and effects the code no longer draws.

S · content · E12 A maintainer's process, CI and deploy that one person and their agents can trust · gaps G137 · **Later**

**Acceptance**

- Given website/README.md, When I read it, Then it states that a change under samples/linesweeper/presentation or states regenerates the stills and the MP4 in the same commit, reviewed
- Given current master, When LineSweeperCapture is run on x64-release, Then the stills and footage are regenerated and committed, and the commit names each changed file as reviewed

**Trade-offs**

- PHILOSOPHY.md:762-767: headless testing covers what is below the platform edge; the capture needs a device, so the guard is a reviewed regeneration rule, not a CI comparison
- CLAUDE.md (LineSweeperCapture): regenerate on x64-release and look at every image that moved before committing it

**Notes.** The footage was last committed at 7166490; 5af01f6, 98a0665 and d169ea9 have changed the presentation since. The local LineSweeperCapture binary predates 5af01f6, so rebuild it first.

<a id="sm-36"></a>
### SM-36. A cadence for moving ColourWars' engine pin

**As** a sole maintainer in the role of the client's author, **I want** a stated rule for how far the pin may lag before I move it, **so that** migrations stay small instead of growing to 45 commits.

S · process · E10 Releases, versions, upgrades and an answered report · gaps G73 · **Next** · **Owner**

**Acceptance**

- Given a ColourWars document, When I read it, Then it states the cadence (for example at most N engine commits behind, or before each client feature lands) and the validation a bump runs (a build at master, the client's checks, and a re-read of engine-reports.md)
- Given the document, When read, Then it records the pin's current lag (`git rev-list --count <pin>..master`; 4cefb68, 45 commits on 2026-10-13) and states that the math-split migration is done (ColourWars 77a7a7f), so the next move is an ordinary bump
- Given the first move under the rule, When it is made by OCA-70's procedure reading OCA-68's backfill, Then the pin is at a current master or the first 0.x tag, the client builds and its checks pass, and what gameplay was and was not verified is recorded
- Given later engine merges, When `git rev-list --count <pin>..master` is checked, Then it stays within the stated bound or the document records the exception

**Trade-offs**

- PHILOSOPHY.md:747-749: the engine cannot reach a consuming repository and should not learn how, so a lag bound is kept in ColourWars rather than enforced from Labrador (an inference from that passage, not its subject)
- PHILOSOPHY.md:739-741: documented, batched breaking changes are what keep each pin move small

**Notes.** Owner fact 2026-10-13: ColourWars' math-split migration is done (77a7a7f, 'Move the pin forty-three commits, and take the name the engine took'); the pin is 4cefb68, 45 commits behind, so the next move is an ordinary bump. The MEMORY.md index line 'stuck behind the math split' is stale. The work lives in ColourWars. Placed first in Next rather than Now: OCA-11's consumer job (Now) already supplies a consumer check, and the bump is cheapest once OCA-68's backfill and OCA-70's procedure exist; the gameplay check is the owner's.

<a id="sm-40"></a>
### SM-40. A headless test of the XInput translation

**As** a sole maintainer, **I want** the desktop pad translation held by a test the way the browser's is, **so that** a swapped bit or lost y negation fails ctest instead of reaching players.

S · code · E05 Input, menus, settings and couch play my players can use · gaps G125 · **Later**

**Acceptance**

- Given InputTests under an x64 preset, When it runs, Then cases build a DirectX::GamePad::State by hand and assert each of the fourteen button bits, the stick y sign and the dead-zone mode
- Given a deliberately swapped bit in the translation, Then InputTests fails
- Given the translation is extracted to a header inside engine/input/xinput/ (as engine/input/web/standard_gamepad.h is), Then the new test is listed in tests/input/CMakeLists.txt under if(LABRADOR_INPUT_BACKEND STREQUAL "xinput"), and check_engine_includes passes

**Trade-offs**

- PHILOSOPHY.md:762-767: everything below the platform edge is testable headlessly; the XInput-state-to-GamepadState translation is below it
- T5: keep the extraction inside engine/input/xinput/, where check_engine_includes enforces the wall at build time

**Notes.** The counterpart is tests/input/standard_gamepad_tests.cpp, compiled only for the web backend (tests/input/CMakeLists.txt:13-17). check_engine_includes globs only engine/ (line 29), so a test including a backend header is the existing precedent, not a violation.

<a id="sm-41"></a>
### SM-41. A component promotion rule that can be evaluated

**As** a sole maintainer who owns the rule for what moves into engine/ui/, **I want** the 'copied unchanged' test to separate styling from behaviour and name what triggers promotion, **so that** the rule can fire, and its first data point is described truly.

S · docs · E05 Input, menus, settings and couch play my players can use · gaps G126 · **Won't this time**

**Acceptance**

- Given PHILOSOPHY.md:603-609, When I read it, Then it says styling edits do not count as edits (or styling is a parameter) and names the trigger (e.g. a second public client)
- Given samples/components/README.md:23-34, Then it no longer calls ColourWars' option row 'the same shape', or says how it differs (Adjust callback, MenuStyle, no choices or index)

**Trade-offs**

- PHILOSOPHY.md:603-610 (T1): a component copied unchanged by every client is mechanism and promotes; one each game edits is policy; the amendment makes that test evaluable
- PHILOSOPHY.md:600: styling belongs to the game, so styling edits do not count against 'unchanged'
- T1 (not a licence for speculative frameworks): promotion on demonstrated demand, not an uptake ledger

**Notes.** Evidence: option_row.cpp:13-15, slider.cpp:17-20, PHILOSOPHY.md:600.

<a id="sm-43"></a>
### SM-43. A current-state agent brief, with its narrative history moved out

**As** a sole owner-maintainer who works through agents that read AGENTS.md (which CLAUDE.md imports) cold at the start of every task, **I want** the agent brief to say what is true of the tree now, with the narrative of how each line came to be moved to dated documents, **so that** every agent spends its context on current rules and facts, and the brief stops contradicting CONVENTIONS' rule that comments are contract or rationale, never history.

M · docs · E12 A maintainer's process, CI and deploy that one person and their agents can trust · gaps G66 · **Next**

**Acceptance**

- Given AGENTS.md (the one brief, which CLAUDE.md imports), When grepped for narrative markers ('used to read', 'used to call', 'until 20', 'This line used to', 'That sentence was'), Then no hit narrates a past state of the tree
- Given each paragraph removed or shortened, When the PR is read, Then its description maps it to its new home (an existing dated document such as docs/survey/, a review folder's STATUS.md or README, or a new dated note under docs/) or says it was already recorded there, so no reason is lost: moved, not deleted
- Given the current facts the brief carries (the build commands, the preset table and ctest counts, the five render, three audio and two input backends, 'What will fail the build', the rules that are not checked, the website's coupling and the review-folder reading rules), When the trimmed brief is compared with the original, Then each is still present and true at the merge commit
- Given the brief's length before and after, When the PR is opened, Then both line counts are recorded, with a target of at most about 350 lines against today's 602 (negotiable)
- Given the top-level build, website.yml and `python -m unittest discover tools/tests` (release.py:19 lists both files), When they run on the PR, Then they pass, including check_agent_brief and check_doc_citations

**Trade-offs**

- T4: the next reader is an agent reading the brief cold; a current-state brief is clarity for that reader, and two copies of a narrative are the duplication T4 calls the opposite of clarity
- CONVENTIONS.md (not a T1-T12 trade-off): a comment is contract or rationale, never the history of the code; the brief is the agents' standing comment on the tree, so its history moves to dated documents
- CLAUDE.md, Reading the documents correctly: deleting a false sentence is not the same as answering the question it got wrong, so a narrative that carries a reason is moved and linked, not dropped

**Notes.** Added from the critique's unstoried list (register.json 'uncovered': 'the 600-line CLAUDE.md as the agents' brief, its narrative history versus CONVENTIONS' no-history rule'). No registered gap covers the trimming; the closest is G66 (the brief duplicated in CLAUDE.md and AGENTS.md with no sync check), which SM-22 closes and this story builds on, so it is registered against G66 as its nearest gap. Examples of narrative to move: 'This paragraph used to read...', 'The module was a hard-coded (render|audio) until 2026-08-28...', 'That sentence was one finding short until 2026-08-29...', 'This line used to call its DRIFT.md a live list...'. Every later story is carried out by agents reading this brief, so it is a capacity multiplier; it opens Next after SM-22 and after OCA-68 has added the changes rule to the brief. The owner reviews the PR as with any change to the brief.

<a id="sm-44"></a>
### SM-44. Record a real Android phone run of the development page and the live demo

**As** a sole owner-maintainer closing M7b, **I want** the same real-phone record SM-13 makes on the iPhone, made on an Android phone in Chrome, **so that** the phone gate's second named device has evidence, and BDP-10 can lift it without an owner exception.

S · verification-hardware · E09 The hosted demo is playable, understandable and kind to every player · gaps G34, G05 · **Deferred** · **Owner**

*Also serves:* The Browser and Phone Demo Player

**Acceptance**

- Given a borrowed Android phone with the development page served over HTTPS by SM-14's procedure, When a person plays a match by touch in portrait and in landscape, Then the device, Android version, Chrome version and outcome are recorded against web-to-be-tested.md:63-73
- Given the phone, When the page loads and reaches the top-out, Then time to first frame, tab memory from chrome://inspect, R6 by SM-08's method and the legibility of the CPU-decoded atlas text are recorded
- Given the phone on the live mattharding.dev page, When it loads, Then the network log shows the poster and no linesweeper.wasm request, and the Android half of web-to-be-tested.md:94 is ticked

**Trade-offs**

- PHILOSOPHY.md:688-699 (Performance): a software rasteriser fails in a different shape from real hardware, so phone behaviour that has run only in headless Chrome and SwiftShader is sized against phones, not proven on them
- T6: the gate is honest while this is open: a loud refusal rather than limping on a device nobody has checked

**Notes.** Split from SM-13 (which absorbed BDP-08) because the owner has an iPhone and no Android phone (owner facts 2026-10-13). Deferred until a device is borrowed; buying hardware is not proposed.

<a id="stories-browser-demo-player"></a>
## The Browser and Phone Demo Player

<a id="bdp-03"></a>
### BDP-03. ctest loads and plays the bundle page a visitor gets

**As** a desktop visitor clicking Play on the hosted page, **I want** the exact page and module the host serves to be exercised before it ships, **so that** a broken host page fails the build instead of failing me on the live site.

M · build-ci · E09 The hosted demo is playable, understandable and kind to every player · gaps G33 · **Next**

**Acceptance**

- Given a wasm-* build where configure found a Chromium, when ctest -N runs, then an entry (e.g. LineSweeperBundlePageTests) is listed that serves out/build/<preset>/web-dist/linesweeper/ and loads its index.html, not shell.html
- Given that entry, when it runs, then it clicks Play, waits for frames from the createLineSweeper instance, sends the sample's way out (Escape, Down, Down, Enter), observes onQuit, clicks Play again and observes frames from the second instance; a console error, a browser warning or onError fails it
- Given the entry run with touch emulation (Emulation.setTouchEmulationEnabled, via an input script like tests/app/web/window_input.mjs or a new run_page.mjs flag), when the page settles, then Play is hidden and no request for linesweeper.wasm is made; after a synthetic keydown Play is shown
- Given CI's wasm job, when it runs, then the new entry appears in its ctest output, and CLAUDE.md's wasm entry count (27, and the list of *PageTests) is updated in the same commit

**Trade-offs**

- T6: a broken content path fails ctest rather than the live site, the rule CLAUDE.md states for *PageTests
- T3: extend run_page.mjs and the existing page-test registration rather than add a browser test framework

**Notes.** run_page.mjs already serves pages, drives CDP and fails on console errors and warnings. Its touch emulation lives in the --input driver script, not the base runner. The failure paths are BDP-04. The manual run in web.md:1474-1488 is the script this automates.

<a id="bdp-04"></a>
### BDP-04. The bundle page's failure paths are tested

**As** a visitor whose browser fails or takes the graphics away, **I want** the build to check the page's failure messages and recovery buttons, **so that** when something goes wrong I see the message and the button the page promises, not a stuck or overlapped canvas.

M · build-ci · E09 The hosted demo is playable, understandable and kind to every player · gaps G33 · **Next**

**Acceptance**

- Given the bundle-page entry from BDP-03, when a case forces a context loss with WEBGL_lose_context.loseContext() and later restoreContext(), then the onContextLost and onContextRestored status texts appear in that order and frames resume
- Given a case that serves a truncated linesweeper.wasm, when Play is clicked, then the error status is shown with a 'Try again' button, and document.elementFromPoint at the button's centre returns the button (no canvas overlays it)
- Given a case that wraps createLineSweeper to capture the config object the page passes and, after the first frame, calls that object's onError with a test message, then the status line shows the error state and elementFromPoint at 'Try again' returns the button (the overlay defect recorded at web.md:1481-1483 does not recur)
- Given ctest under a wasm-* preset, when these cases run, then each one passes or fails the entry; none is reported as a warning

**Trade-offs**

- T6: a visitor cannot report a failure path back, so the build needs a check that fails loudly
- T3: the same runner with extra cases, as RenderContextLossTests already does

**Notes.** Depends on BDP-03. Calling the page's own onError is how this can be tested: the module exports nothing that makes the engine fail on demand, and the page's rendering is where the recorded defect was. The message wording is BDP-15's. These assertions pin whatever text index.html shows now, and BDP-15 updates them.

<a id="bdp-06"></a>
### BDP-06. Play the bundle page in Firefox, with a physical pad, through a real tab switch and a real context loss

**As** a desktop visitor on Firefox or with a controller plugged in, **I want** the hosted page to have been played in Firefox, with a real pad, across a real tab switch, and through a real loss of the graphics context, **so that** the input, focus and recovery paths I use have been tried on real hardware, not only with a script's fake pad.

S · verification-hardware · E11 Performance and hardware claims backed by a measurement · gaps G23 · **Next** · **Owner**

**Acceptance**

- Given the bundle page served from web-dist/linesweeper/ in desktop Firefox, when a person plays a match, quits and plays again, then the run and the Firefox version are recorded in web-to-be-tested.md
- Given a physical pad in Chrome, when a person plays a match using only the pad (move, rotate, soft and hard drop, hold, pause), then each action is observed, and the pad model is recorded under 'Needs a gamepad (M4)'
- Given the game running with a pad, when the tab is hidden and shown again, then the record says whether pad input was ignored while hidden and accepted afterwards
- Given the game running in desktop Chrome, when chrome://gpucrash is opened in another tab, then whether the page's lost and restored messages appear and the game resumes is recorded against web-to-be-tested.md:97

**Trade-offs**

- CLAUDE.md (input backends): the web pad path has been driven only by a page script's fake pad, never a physical one; the owner has one
- docs/port/web-to-be-tested.md 'Needs a gamepad (M4)' and :97: the records this run fills
- No PHILOSOPHY trade-off engaged: a hardware verification record

**Notes.** Owner fact 2026-10-13: a physical pad is owned, so the pad half is not blocked and this story moves to Next; it can share OCA-48's session but tests a different path (the browser Gamepad API, tab hide and show). The desktop XInput counterpart (G124/G125) is OCA-48 and SM-40. G23's audible Web Audio item does not apply here: LineSweeper has no audio (samples/linesweeper/states/pause_state.cpp:309-311).

<a id="bdp-07"></a>
### BDP-07. Smooth play on the displays visitors actually use

**As** a desktop visitor on a 60 Hz or high-refresh monitor, **I want** the demo's frame pacing to have been measured on real 60 Hz and high-refresh displays, **so that** pieces fall smoothly on my screen rather than stuttering.

M · verification-hardware · E11 Performance and hardware claims backed by a measurement · gaps G23 · **Later** · **Owner**

**Acceptance**

- Given the bundle page on a real 60 Hz display and on a real display above 60 Hz, when a match is played for at least two minutes on each while a page script collects requestAnimationFrame timestamps (or a DevTools performance trace is taken), then p50 and p99 frame-start intervals per display are recorded in docs/port/web.md under R5 (:1603), naming the browser and the display's refresh rate
- Given the recorded numbers, when R5's status line and the M3 status at web.md:840 and :854 are read, then they say 'measured' or name the defect found, not 'unmeasured'

**Trade-offs**

- PHILOSOPHY.md:675, 691-699 (Performance): pacing is a hypothesis until measured; a latency claim needs a measured p99 and no number is a floor until it exists
- T8: high frame rates are part of the engine's identity, so a high-refresh display is where per-frame cost shows

**Notes.** Read docs/performance/2026-09-19-g6f-ratchet.md before interpreting any p99: a software pacer on a synchronised present ratchets. Nothing records whether the owner has a display above 60 Hz, so that half may need borrowed hardware. This result must not be cited as the low-tier claim (G24). This story measures; it does not change code.

<a id="bdp-09"></a>
### BDP-09. The host page fits a phone held upright

**As** a phone visitor, **I want** the hosted page to lay the game out for my screen, clear of the notch, with a full-screen button where the browser allows one, **so that** I can see and touch the whole board in portrait.

S · code · E09 The hosted demo is playable, understandable and kind to every player · gaps G34 · **Later**

**Acceptance**

- Given samples/linesweeper/web/index.html after this change, when it is read, then it has viewport-fit=cover, safe-area padding, no fixed 16:9 in portrait, and a full-screen button shown only where the element Fullscreen API exists, ported from shell.html:24-55
- Given the bundle-page ctest entry (BDP-03) with a 390x844 touch-emulated viewport and the gate bypassed by a keydown, when the game draws, then the canvas's getBoundingClientRect lies inside the viewport and is taller than it is wide
- Given the same entry at a 1280x720 desktop viewport, when the game draws, then the canvas's bounding box keeps a 16:9 aspect ratio (within 1 px) and lies inside the viewport

**Trade-offs**

- T3: port what shell.html already does rather than design a new layout
- T1: page policy; the engine changes nothing

**Notes.** Can land before BDP-08 while the gate stays in place; lifting the gate needs it. The portrait arrangement itself is already in the module (samples/linesweeper/README.md 'A phone held upright', :478).

<a id="bdp-10"></a>
### BDP-10. Lift the phone gate once real phones have played

**As** a phone visitor, **I want** a Play button I can tap on the hosted page, **so that** I can play by touch instead of watching a poster.

XS · code · E09 The hosted demo is playable, understandable and kind to every player · gaps G34 · **Deferred**

**Acceptance**

- Given recorded passes on an iPhone (SM-13) and an Android phone (SM-44), or the owner's recorded decision that one platform's evidence is enough, and BDP-09 merged, When index.html's coarse-pointer branch (:106-111) is changed, Then Play is shown on a touch device without a key, and the header comment's 'asks for a keyboard or a controller' rationale (:30, :72) is replaced by the evidence it now rests on
- Given the bundle-page ctest entry with touch emulation, When Play is tapped, Then frames arrive and a synthetic tap on a touch control reaches the game (the piece moves, observed by the quit path or a frame difference the entry asserts)
- Given docs/port/web.md section 7.4 and the M7b row, When this lands, Then in the same commit they say the hosted page plays on phones and name the devices it rests on

**Trade-offs**

- T6: the gate is a loud refusal on unchecked devices instead of a silent limp, and it stays while the devices are unchecked
- PHILOSOPHY.md:688-699 (Performance): software rendering is not the device; the gate goes only with a recorded play on real phones
- T3: one branch removed

**Notes.** Deferred: web.md:748-750's gate names a real iPhone and a real Android phone, and no Android phone is owned. The iPhone half (SM-13) can now be done; lifting the gate needs SM-44 too, unless the owner decides one platform's evidence suffices (an owner decision). Also blocked on BDP-09. The mattharding.dev page's own phone fallback (the video) is in the owner's other repository; see BDP-20.

<a id="bdp-11"></a>
### BDP-11. The game pauses when it loses my keys

**As** a desktop visitor who clicks elsewhere on the host page or switches tabs mid-match, **I want** the match to pause when my keys stop reaching the game, and not to resume mid-fall when I come back, **so that** I do not lose a piece to input I cannot give.

S · code · E09 The hosted demo is playable, understandable and kind to every player · gaps G35 · **Later**

**Acceptance**

- Given LineSweeper in a wasm build, when the canvas loses keyboard focus during play (detected with Keyboard::just_unfocused(), engine/input/keyboard.h:263), then the play state enters the pause menu on that tick
- Given a page case (in LineSweeperPageTests or the BDP-03 entry) that blurs the canvas during play, refocuses it, then sends Down, Down, Enter with no Escape, then onQuit is called, which can only happen if the game was already in the pause menu
- Given the same case with page visibility faked hidden then shown (as WebWindowTests already fakes visibility), when the keys are sent the same way, then onQuit is called
- Given samples/linesweeper/README.md's 'There is a pause screen' section (:243), when this lands, then it is amended in the same commit to say why focus loss pauses
- Given web.md:295-300, when this lands, then pads still follow page visibility, not canvas focus (pad behaviour unchanged)

**Trade-offs**

- T1: sample policy over an existing engine signal; no new engine API
- T6 (Not a licence for): losing the keys is a state, like a disconnected controller's neutral input, not an error; pausing makes that state visible instead of dropping keys silently, and nothing throws

**Notes.** Critic correction in register.json: on_keyboard_lost (application.cpp:416-420) does not reach notify_activation, so on_deactivated alone would miss a canvas blur. Use Keyboard::just_unfocused for the blur and on_deactivated for the hide. Headless Chrome cannot really hide a page, so visibility is faked. Any change to hint or pause text changes the capture images; regenerate on x64-release and review them.

<a id="bdp-12"></a>
### BDP-12. One complete, consistent list of controls on the page and in the game

**As** a desktop visitor, **I want** the page and the game to name the same keys, including soft drop, hold, pause and the pad buttons, **so that** I can find every move without guessing.

S · content · E09 The hosted demo is playable, understandable and kind to every player · gaps G80 · **Later**

**Acceptance**

- Given samples/linesweeper/web/index.html's status line and play_state.cpp's hint strings, when they are compared against the binding tables in play_state.cpp:55-95, then every bound action (move, rotate both ways, soft drop, hard drop, hold, pause) is named on at least one surface, and no action is given a different key on the two surfaces
- Given the bundle page on a desktop pointer, when it loads, then the status line names the pad controls as well as the keys, and the BDP-03 entry asserts that text
- Given the hint strings have changed, when LineSweeperCapture is re-run on x64-release, then the changed images are looked at and committed in the same commit, and the website builds

**Trade-offs**

- T3: text only - one corrected list, not new bindings
- T1 (Not a licence for speculative frameworks): bindings stay fixed and rebinding stays known-absent, because no client has a rebinding screen
- T4: one list a visitor can read; two lists that disagree are the duplication T4 calls the opposite of clarity

**Notes.** Not an action-mapping layer: the binding tables stay as they are. The site's footage video may then lag the stills (G137, maintainer).

<a id="bdp-13"></a>
### BDP-13. A phone sees phone instructions, not keyboard ones

**As** a phone visitor, **I want** the page to tell me what my device can do (tap controls, or that touch play is coming) rather than list arrow keys, **so that** the page makes sense on the device in my hand.

XS · code · E09 The hosted demo is playable, understandable and kind to every player · gaps G80 · **Later**

**Acceptance**

- Given the bundle page under touch emulation, when it loads, then the status line contains none of 'Space', 'Escape' or 'arrow', and describes the touch or controller route
- Given a desktop pointer, when the page loads, then the status line is the desktop text from BDP-12
- Given the bundle-page ctest entry, when it runs under both emulations, then it asserts both texts

**Trade-offs**

- T3: the one matchMedia branch already at index.html:108

**Notes.** Depends on BDP-03. Word it so it stays true before and after BDP-10 lifts the gate, or update it in BDP-10.

<a id="bdp-14"></a>
### BDP-14. A controller lifts the phone gate, as the page says it will

**As** a phone or tablet visitor with a Bluetooth controller, **I want** pressing a button on my controller to show Play, as the page promises, **so that** I can play with the device the page told me to bring.

XS · code · E09 The hosted demo is playable, understandable and kind to every player · gaps G81 · **Later**

**Acceptance**

- Given the bundle page under touch emulation, when a page-script fake pad (as already used for M4) connects and presses a button, then Play is shown, and no linesweeper.wasm or linesweeper.js request was made before that
- Or, if rewording is chosen instead: index.html:30 and :72 no longer mention a controller, and the bundle-page entry asserts the new wording

**Trade-offs**

- T6: the page must not promise a route it does not offer
- T1: a page listener only

**Notes.** Moot once BDP-10 lifts the gate; take it only if BDP-10 is more than a short way off. The implementer chooses between the listener and the rewording, and the check covers whichever is chosen. Chrome exposes a pad only after a button press, so the listener uses gamepadconnected or polls getGamepads().

<a id="bdp-15"></a>
### BDP-15. Failure messages a player can act on

**As** a visitor whose browser or device cannot run the game, **I want** a plain message that says what happened and what I can do (another browser, turn on hardware acceleration, reload), and no 'Try again' where retrying cannot work, **so that** I am not left with 'Aborted(...)' or 'see Application::run'.

S · code · E09 The hosted demo is playable, understandable and kind to every player · gaps G82 · **Later**

**Acceptance**

- Given index.html, when onError or a load failure fires, then the visible status line shows a sentence in the player's terms for the failure class: no WebGL2, failed load, or stopped mid-game. The engine's own text still goes to the console, and optionally into a collapsed details element
- Given the no-WebGL2 class, when it is shown, then no 'Try again' button is offered
- Given a failure before the first frame, when it is shown, then it does not say the game 'stopped'
- Given the bundle-page failure cases (BDP-04), when they run, then they assert the player's wording for each class

**Trade-offs**

- T6: the engine's worded reasons stay, and stay loud; turning them into a player sentence is page policy
- T1: no engine change

**Notes.** Classify by where the page failed (import, instantiate, onError after the first frame), not by message text. Developers must still be able to see the engine's message.

<a id="bdp-16"></a>
### BDP-16. A graphics context that never comes back offers a way forward

**As** a visitor whose browser took the graphics away and never gave them back, **I want** to be told after a while that it has not come back, and offered a restart, **so that** I am not left staring at a blank canvas with a status that looks normal.

S · code · E09 The hosted demo is playable, understandable and kind to every player · gaps G83 · **Later**

**Acceptance**

- Given index.html, when onContextLost fires, then the status line gets the error styling, as shell.html:138-142 does
- Given no onContextRestored within a stated timeout (for example 10 s, with the value in a comment), when it elapses, then 'Try again' is shown, and pressing it mounts a fresh canvas and instance
- Given onContextRestored arrives before the timeout, when it fires, then the warning clears and no button appears
- Given the bundle-page ctest (BDP-04), when a case calls loseContext() and never restores, then it observes the button and a successful remount with frames

**Trade-offs**

- T6: a blank canvas is the silent failure the status line exists to prevent
- T1: page-level; SEAM.md and DeviceNotify are untouched

**Notes.** The remount reuses the Play again path. See BDP-18 on old instances accumulating.

<a id="bdp-17"></a>
### BDP-17. Know before downloading that my browser cannot run the game

**As** a visitor on an older or locked-down browser, **I want** the page to tell me, before it downloads anything, that my browser lacks WebGL2 or wasm exception handling, **so that** I do not wait for a download that ends in a compile error.

S · code · E09 The hosted demo is playable, understandable and kind to every player · gaps G84 · **Later**

**Acceptance**

- Given index.html, when Play is clicked, then before it imports linesweeper.js it tests for WebGL2 (getContext('webgl2') on a scratch canvas) and for wasm exception handling (WebAssembly.validate on a minimal module using the exception encoding the build currently emits)
- Given either test fails, when Play is clicked, then BDP-15's message for players is shown, and the network log shows no request for linesweeper.js or linesweeper.wasm
- Given the page loads and nothing is clicked, then no scratch context is created (the page's rule that nothing loads before a click still holds)
- Given the bundle-page ctest, when a case stubs HTMLCanvasElement.prototype.getContext to return null for 'webgl2', then it asserts the message and that no module request was made

**Trade-offs**

- T6: fail with a reason before the cost, not after
- T3: two feature probes in the page, nothing in the engine

**Notes.** The gap is inferred, not observed. The build uses -fwasm-exceptions (cmake/settings.cmake:75) and currently emits legacy exceptions (web-to-be-tested.md:31-32). A probe written for the newer exnref encoding would turn away browsers that can run the game, so keep the probe bytes beside a comment naming the flag and the encoding, and change them together.

<a id="bdp-18"></a>
### BDP-18. Playing again many times stays fast and keeps the graphics

**As** a visitor who plays several rounds in one visit, **I want** each Play again to work as well as the first, however many I play, **so that** the tenth round does not stutter or go blank.

S · verification-hardware · E09 The hosted demo is playable, understandable and kind to every player · gaps G114 · **Later**

**Acceptance**

- Given the bundle page in headless Chrome with HTMLCanvasElement.prototype.getContext wrapped to record every context created, when a scripted run does Play, quit, Play again twenty times, then the JS heap (CDP Runtime.getHeapUsage) and the number of recorded contexts that are not isContextLost() after each round are recorded in web-to-be-tested.md
- Given that run, when any round gets an unprompted onContextLost or Chrome logs 'Too many active WebGL contexts', then a gap is opened with the round number
- Given a page-side cleanup is adopted (after onQuit, lose the old context with WEBGL_lose_context and drop the old instance's references), when the run repeats, then at most one recorded context is live after each round, and the bundle-page ctest asserts a run of at least five rounds

**Trade-offs**

- PHILOSOPHY.md:675 (Performance): optimisation follows a profile - measured, not guessed; the re-fetch and the contexts that build up across plays are measured before they are fixed
- T1: the cleanup is in the page; the engine's static Application stays as web.md:713-722 describes

**Notes.** register.json calls the context-cap consequence a hypothesis. Builds on BDP-03's harness. run_page.mjs already fails on a browser warning, so Chrome's context-cap warning fails the run by itself. Measure first; how much cleanup to do depends on what the measurement shows.

<a id="bdp-19"></a>
### BDP-19. Decide whether a best score survives, and if so keep it

**As** a desktop visitor who plays a few matches, **I want** to see my best score from earlier rounds and earlier visits, **so that** I have something to beat.

S · code · E09 The hosted demo is playable, understandable and kind to every player · gaps G85 · **Won't this time** · **Owner**

**Acceptance**

- Given samples/linesweeper/README.md, when this story is taken, then a section records the decision (keep a page-level best score, or not and why) before any code
- Given the decision is to keep one, when a LineSweeperTests case runs, then it asserts the rule (best = max(previous best, final score), unchanged by a lower score)
- Given the decision is to keep one, when a page case tops out a match by repeated hard drops and then reloads, then localStorage holds the expected key with a value at least the final score, and the value survives the reload; what is drawn is checked by looking
- Given localStorage throws (stubbed in a page case), when a match ends, then no console error is logged and Play again still works
- Given the change, when engine/ is diffed, then nothing in it changed and no engine persistence facility was added

**Trade-offs**

- T1: a page- or sample-level score, never an engine persistence facility; the demand is assumed, not recorded (personas.json assumptions)
- T3: one number in localStorage

**Notes.** Open to negotiation by design: the register says to decide this, not to assume it is wanted. If the decision is 'no', the story closes on the README section alone. Adopter save data is G108, a separate story. A score drawn in a captured frame changes the capture images.

<a id="bdp-20"></a>
### BDP-20. The phone fallback does not spend my data without asking

**As** a phone visitor on a metered connection, **I want** the 4.3 MB recording the host page shows phones to load only when I ask for it, or its size to be a recorded decision, **so that** opening the demo on a phone does not cost me megabytes I did not choose to spend.

XS · process · E09 The hosted demo is playable, understandable and kind to every player · gaps G86 · **Later** · **Owner**

**Acceptance**

- Given the live mattharding.dev page under touch emulation, when it loads and nothing is tapped, then the network log shows either no video bytes fetched, or a recorded decision to autoplay at web-to-be-tested.md:99-100 with the size and the reason
- Given web-to-be-tested.md:99-100, when this lands, then its box is ticked with the decision and the date

**Trade-offs**

- T3: match Labrador's own site, which plays footage only on request (website-plan.md:106-111)

**Notes.** The page lives in the owner's PersonalWebsite repository; any change there is the owner's. Shrinks once BDP-10 gives phones the game itself.

<a id="bdp-21"></a>
### BDP-21. A landing shadow I can see for every piece

**As** a visitor, including one with low vision, playing LineSweeper, **I want** the landing shadow to stand out from the empty well for every piece colour while still looking unlike a real piece, **so that** I can see where a J, T or Z piece will land.

S · code · E09 The hosted demo is playable, understandable and kind to every player · gaps G117 · **Later**

**Acceptance**

- Given the shadow's drawing in board_view.cpp and the well colour (board_view.cpp:65), when a LineSweeperViewTests case computes, with palette.h's premultiplied arithmetic, the WCAG contrast of the shadow's visible edge (its fill, or an outline if one is added) against the well for all seven kind_colour values, then each is at least 3:1
- Given the change, when the shadow is compared with the live piece of the same kind, then it is still visibly distinct (a fill alpha below 1, or an outline-only shadow), and the README says which
- Given the change, when LineSweeperCapture is re-run on x64-release, then every image that moved is looked at and committed in the same commit
- Given samples/linesweeper/README.md, when this lands, then it records why the shadow looks as it does and the 3:1 target

**Trade-offs**

- T1: sample policy; presentation/palette.h:11-13 already isolates colour
- T3: one constant, or an outline drawn with the sprite call already used
- T5: a test that fails if a later palette edit drops below 3:1 - the earliest stage that can catch it, not a reviewer's eye

**Notes.** From palette.h:26-39: J at full strength is only about 3.6:1 against the well and T about 4.0:1, so raising shadow_alpha alone would need about 0.85 and make the shadow nearly a piece. An outline is probably the answer. This is computed from constants, not observed; look at it after the change. Also relevant to sample-reading-learner, but the value here is the player's.

<a id="bdp-22"></a>
### BDP-22. Fewer sparks when I have asked my system for reduced motion

**As** a visitor who has set prefers-reduced-motion, **I want** line clears and the top-out burst to be calmer, **so that** the demo does not throw thousands of saturating particles at me.

S · code · E09 The hosted demo is playable, understandable and kind to every player · gaps G118 · **Later**

**Acceptance**

- Given the browser build, when matchMedia('(prefers-reduced-motion: reduce)') matches at start, then main_web.cpp passes a reduced-motion flag to the presentation and writes one console.info line naming it; a page case under CDP Emulation.setEmulatedMedia with prefers-reduced-motion: reduce asserts that line
- Given LineSweeperViewTests, when the particle field is driven with the reduced setting through a line clear and a top-out, then the particles spawned are at most the reduced count stated in the code
- Given no preference, when LineSweeperCapture is re-run on x64-release, then no image moves
- Given the change, when engine/ is diffed, then nothing in it changed

**Trade-offs**

- T1: the OS preference is read by the sample's main_web.cpp or page, not by an engine layer
- T3: a count, not an effects system

**Notes.** The query can be read in main_web.cpp with EM_JS. The site already honours reduced motion (LineSweeperFootage.astro:8-10); the demo is the only animated surface without a switch. The desktop build's system animation setting is out of scope for this persona.

<a id="bdp-23"></a>
### BDP-23. Check the top-out burst against flash limits

**As** a visitor sensitive to flashing, **I want** the demo's bursts to have been checked against the WCAG 2.3.1 three-flashes threshold, **so that** playing it is not a seizure risk to me.

S · verification-hardware · E09 The hosted demo is playable, understandable and kind to every player · gaps G118 · **Now**

**Acceptance**

- Given tools/linesweeper_capture driven by a checked-in script that reaches a line clear and a top-out, When it writes consecutive frames at 60 frames per second, Then the frames are analysed for general and red flashes over every one-second window against WCAG 2.3.1's three-flashes threshold, using the WCAG relative-luminance formula frame by frame (or a named tool that implements it)
- Given the analysis, When it completes, Then the method, the script, the commit and the result are recorded in samples/linesweeper/README.md
- Given a failure, When it is recorded, Then a gap is opened naming the frames, the record says whether BDP-22's reduced path should become the default, and OCA-03's link waits on the decision

**Trade-offs**

- PHILOSOPHY.md:691-699 (Performance): a claim is not made until measured - calling the burst flash-safe needs a recorded method and result, not an assumption
- T6: if the burst fails the check, the page says so instead of shipping it quietly

**Notes.** Critique: the dependency is reversed, so OCA-03 (the front-door link) now depends on this check; G118 is registered (minor), so the story stays in Now as Must because the Must link waits on it. The method is named: LineSweeperCapture frames are deterministic and come from a checked-in script. README.md:470-475 records that the top-out burst once made the banner unreadable for a second; additive blending saturates towards white (palette.h:79-82).

<a id="bdp-24"></a>
### BDP-24. The demo says what it offers players with access needs

**As** a visitor who relies on keyboard-only play, a screen reader or a colour-vision aid, **I want** the demo page to say plainly what it supports and what it does not, **so that** I can tell from one sentence whether I can play it, instead of finding out by trying.

XS · content · E09 The hosted demo is playable, understandable and kind to every player · gaps G119 · **Next**

**Acceptance**

- Given samples/linesweeper/web/index.html, when it loads, then a note says what the demo supports (keyboard, pad, and touch once BDP-10 lands; Tab is left to the browser so focus can always leave the game) and what it does not (game state is not read out; controls are fixed by design). The note is reachable by a screen reader, either through aria-describedby on the canvas or as a visible paragraph
- Given samples/linesweeper/README.md, when it is read, then a short accessibility section says the same, says fixed bindings are a decision (action mapping is known-absent), and records whether the colour-blind palette anticipated at palette.h:11-13 has an owner
- Given the bundle-page ctest, when it runs, then it asserts the note is present and referenced

**Trade-offs**

- T1: a statement, not a screen-reader layer or accessibility framework
- T6: state the limit instead of leaving it accidental

**Notes.** Tab: browser_keeps_key leaves Tab to the browser (engine/app/web/dom_events.cpp:183-191). G119 is also storied for outside-cpp-adopter (what the engine offers a game) and sole-maintainer (a PHILOSOPHY position); here the value is the player's decision whether to play. Must not promise remapping.

<a id="stories-sample-reading-learner"></a>
## The Sample-Reading Learner

<a id="srl-02"></a>
### SRL-02. Read Concepts without first building the minimal sample

**As** a reader who wants Labrador's model, possibly on macOS or Linux, **I want** the Concepts index to tell me what I actually need before reading, which is not a Windows build, **so that** I am not turned away by a precondition the pages do not need, since every concept page quotes its code inline.

XS · docs · E06 Concepts and reference that tell the truth a caller needs · gaps G95 · **Next**

**Acceptance**

- Given website/src/content/docs/docs/concepts/index.mdx, when I read its opening paragraph (currently :11-14), then it no longer says the pages 'assume you have built and run the minimal sample'
- Given the same paragraph, then it says the pages quote the sample code they discuss, and presents building minimal as optional with a link to /docs/get-started/
- Given the same paragraph, then it links every sample page hosted at that commit (at least https://mattharding.dev/demo/linesweeper/, which is live) as a way to see code run without building
- Given the change, when website.yml runs, then it passes

**Trade-offs**

- T3: a wording fix, not a non-Windows build path

**Notes.** From the journey: the concept pages embed SourceFile quotes, so they read without a build. If SRL-13 lands later, its hosted pages are added to this paragraph in that story's commit.

<a id="srl-03"></a>
### SRL-03. Concept pages say true things about the samples they cite

**As** a learner checking a concept page against the sample I am reading, **I want** each statement about what the samples do to match the samples, **so that** I learn the real coordinate and content model instead of a wrong one I would then copy.

S · docs · E06 Concepts and reference that tell the truth a caller needs · gaps G36 · **Now**

**Acceptance**

- Given concepts/drawing.mdx, when I read the paragraph after the DEFAULT_CAMERA sentence (currently :51-54), then it no longer says 'Both samples use it'. Each sample it names must do what it says, checked against the source: minimal keeps the list's starting identity camera (samples/minimal/states/hello_state.cpp:115-119); LineSweeper (presentation/arrangement.cpp:27), collision (collision_state.cpp:112) and the components gallery (gallery_state.cpp:247) use Camera::frame; local_multiplayer builds a camera per player (arena.cpp:58)
- Given concepts/resources.mdx (:41) and concepts/states.mdx (:114), when I grep them and drawing.mdx for 'Both samples' or 'two samples', then there are no matches, and each sentence that replaces them names the samples it means
- Given concepts/resources.mdx, when I read how content reaches the build, then it says a browser build embeds content in the module rather than copying it beside an executable (samples/minimal/CMakeLists.txt:31-32; samples/audio/CMakeLists.txt:22-24)
- Given the change, when website.yml runs, then it passes

**Trade-offs**

- T6: a page that says something false about code fails its reader silently, and the quote check cannot catch prose

**Notes.** The quote anchors still resolve, so no build fails today; only the surrounding prose is wrong. A claim that names its samples, rather than counting them, does not go stale when a seventh sample is added.

<a id="srl-04"></a>
### SRL-04. Examples page states LineSweeper's real reading size

**As** a reader planning how long it takes to read LineSweeper, **I want** the size the Examples page gives to match the tree, **so that** I can budget my reading time honestly.

XS · docs · E06 Concepts and reference that tell the truth a caller needs · gaps G89 · **Next**

**Acceptance**

- Given website/src/content/docs/examples.mdx, when I read the LineSweeper size sentence (currently :54-55), then the code figure is within 10% of the line count of every .h and .cpp under samples/linesweeper (cat ... | wc -l) at that commit, and the test figure is within 10% of the same count over tests/linesweeper
- Given the change, when website.yml runs, then it passes

**Trade-offs**

- T3: a rounded figure, not a computed one, unless the owner wants the site build to count it

**Notes.** The register counted 4,784-4,850 lines of code and 2,470 lines of tests, against the page's 'about 4,000' and 'about 2,000'. An optional, negotiable alternative is to have the site build compute the figure so it cannot go stale.

<a id="srl-05"></a>
### SRL-05. LineSweeper README's open question states how much the unchecked layer rule covers

**As** a learner reading LineSweeper's 'Still open' list, **I want** the layer-rule item to describe presentation/ as it is now, **so that** I understand how much code an unchecked rule already spans.

XS · docs · E06 Concepts and reference that tell the truth a caller needs · gaps G90 · **Next**

**Acceptance**

- Given samples/linesweeper/README.md, when I read the 'Whether the layer rule gets a build check' item (currently :568-570), then any count it gives for presentation/ equals what ls samples/linesweeper/presentation lists at that commit (12 files today), or the item makes its point without a count that can go stale
- Given the item, then it still argues that the check is owed (the sentence is corrected, not deleted)
- Given the change, when the top-level build runs check_doc_citations.cmake, then it passes

**Trade-offs**

- T5: a rule that holds only by review and could be two greps in the build is what T5 says the build should catch; understating how much the rule covers weakens that argument

**Notes.** The README says 'five files'; the folder holds 12 (6 translation-unit pairs plus layout.h and palette.h). Per CLAUDE.md, deleting a false sentence is not the same as answering it.

<a id="srl-06"></a>
### SRL-06. LineSweeper's design notes give their reasons without sending me to internal records

**As** a cold reader of LineSweeper's README, without access to the maintainer's working notes, **I want** each decision's reason stated in the README or in a published document, **so that** I can follow every decision from the README alone, which the README's opening (:3-5) says it is for.

S · docs · E06 Concepts and reference that tell the truth a caller needs · gaps G91 · **Later**

**Acceptance**

- Given samples/linesweeper/README.md, when I grep it for 'CLAUDE.md', then there are no matches
- Given each place the README currently cites docs/survey/ or docs/review/gpt6 (today :257, :362, :431, :453, :585), when I read that paragraph, then it states in its own words, in a sentence or two, the finding or reason the citation supports; the citation may stay as a source
- Given the action-mapping sentence (currently :581), then it cites docs/design/ARCHITECTURE.md (the known-absent reasoning at around :176) instead of an assistant notes file, and keeps the reason as re-taken when report 21 was declined (bindings and any rebinding are the game's by PHILOSOPHY's boundary table, and the clients combine sources by different rules), not the earlier one that no client has a rebinding screen
- Given the change, when the top-level build runs check_doc_citations.cmake, then it passes

**Trade-offs**

- T1: the action-mapping reason should be the T1 argument as ARCHITECTURE states it, not a pointer to working notes

**Notes.** Restate or re-point; never delete the reason. Line numbers are as of 0977112.

<a id="srl-07"></a>
### SRL-07. One naming rule for constants that matches the published headers

**As** a learner copying the engine's naming style into my own code, **I want** the constants rule in CONVENTIONS and the constants in the public headers to agree, **so that** I can tell which spelling is right without guessing which source is stale.

S · docs · E06 Concepts and reference that tell the truth a caller needs · gaps G92 · **Later** · **Owner**

**Acceptance**

- Given docs/design/CONVENTIONS.md '## Constants and enumerators' (:77-84) and the 'SCREAMING constants' rule (:204), when I list every all-caps identifier declared in engine/**/*.h that is not a macro, then each one falls under a rule CONVENTIONS states: either an amended rule names the exception and its reason, or no such identifiers remain
- Given the owner chooses to rename rather than amend, when I read the commit message, then it lists each old name with its new one, so ColourWars' later pin bump has a migration record
- Given the change, when the top-level build runs, then check_doc_citations.cmake and the website build pass

**Trade-offs**

- T11: CONVENTIONS (:79-82) gives the constants rule its reason (constants are values and take value naming), so an amendment or a rename has to answer that reason
- T4: a stranger reading the headers cold should meet one rule that matches the code, not two sources to reconcile
- PHILOSOPHY.md:741-750 (The public face): if the owner renames, deleting a public name needs a caller count that names the repositories counted, ColourWars included, which is what the migration record in the commit message serves

**Notes.** The owner chooses between amending and renaming. The register counted 26 SCREAMING constants (Vector2F::ZERO, DIRECTION_*, PI, EPSILON, Camera::DEFAULT_CAMERA, ViewportManager::DIVIDER_THICKNESS among them), and concepts/drawing.mdx teaches DEFAULT_CAMERA by name. Design documents change by amendment in the same commit (CLAUDE.md, 'Reading the documents correctly'). ColourWars is pinned behind master, so a rename does not break it immediately.

<a id="srl-08"></a>
### SRL-08. The reference tells me which headers are for game code

**As** a learner reading the API reference to understand what a game calls, **I want** each reference page to say whether it is for game code or for renderer writers, **so that** I can skip resource_factory.h, throw_if_failed.h and the like without first reading them to find out.

S · code · E06 Concepts and reference that tell the truth a caller needs · gaps G93 · **Next**

**Acceptance**

- Given website/src/lib/published.ts, when I read each reference header entry, then it carries an audience value (game code or renderer writer)
- Given the built reference, when I open the pages for resource_factory.h, throw_if_failed.h, sprite_vertex.h and block_decode.h, then each shows a visible renderer-writer marker, and the reference index lists the two groups separately
- Given a header added to published.ts without an audience, when npm test or the site build runs, then it fails and names the header
- Given the change, when website.yml runs, then it passes

**Trade-offs**

- T5: a header added without an audience fails the site build, the earliest stage that can catch it, as an unlisted header already does
- T4: the reader sees the split instead of inferring it from comment text

**Notes.** The headers stay published (reference/index.mdx:45-50 says why). resource_factory.h itself says a client never sees it (:60-64). It pairs with SRL-09 and SRL-12, whose impl() marker can use this audience value, but lands independently.

<a id="srl-09"></a>
### SRL-09. Game-facing reference pages keep backend rationale out of my way

**As** a learner reading RenderResources and similar pages from the headers alone, as PHILOSOPHY intends, **I want** backend lifetime and descriptor-heap rationale on game-facing pages either moved to the seam's charter or marked as for renderer writers, **so that** I can read the contract a game is held to without sorting it from Direct3D 12 and Vulkan internals.

S · docs · E06 Concepts and reference that tell the truth a caller needs · gaps G94 · **Later**

**Acceptance**

- Given engine/render/render_resources.h, when I read the comment above release_device_resources (around :69-122), then it first states the caller contract and who calls it (the shell). Any backend rationale beyond that is either in engine/render/SEAM.md under a numbered section the header cites, or under a line that marks it as for renderer writers
- Given the built RenderResources reference page, when I read the release_device_resources section, then its first sentence says a game does not call it
- Given the change, when the top-level build runs check_doc_citations.cmake and website.yml runs, then both pass

**Trade-offs**

- T4: contract first, rationale where its reader looks
- PHILOSOPHY.md:714-716 (The public face): the caller contract stays in the public header, because a stranger starts from the headers

**Notes.** The register records a tension. CONVENTIONS says rationale belongs in comments, and the ApogeeVGC relocation evidence found relocated docs drifting while a citation linter stayed green. Marking in place is the lower-risk option; moving text to SEAM.md must keep a citation that the check verifies.

<a id="srl-14"></a>
### SRL-14. Know whether the 'Introduction to C++' series exists, is planned or is dropped

**As** a beginner who read T10's promise of an on-ramp, **I want** a public statement of the series' status, **so that** I can tell a target from something I can use today, and look elsewhere if it does not exist.

S · docs · E01 Know what Labrador is, and whether it fits, before installing · gaps G22 · **Later** · **Owner**

**Acceptance**

- Given the built site, when I read Get Started's index or the page that publishes PHILOSOPHY, then a sentence states the series' status as one of: not started, planned with an outline, or withdrawn. The existing mention in T10 does not count
- Given the status is 'withdrawn', then PHILOSOPHY.md T10 (:179-183) and 'The public face' (:734-738) are amended in the same commit with the reason, and T10's acceptance test is restated
- Given the status is 'planned', then a document in the repository names at least the series' scope and the topic of its first episode
- Given the change, when check_doc_citations.cmake and website.yml run, then both pass

**Trade-offs**

- T10: the series is the stated price of having no scripting layer and the API's acceptance test, so withdrawing it changes T10's argument

**Notes.** Producing the series is XL and outside the primary persona's work (register G22 critic note); this story covers only the status. Design documents are written in the target tense, so PHILOSOPHY's sentence may stay as a target, and the reader gets the status elsewhere.

<a id="srl-15"></a>
### SRL-15. Get Started tells me where to go if I do not know C++ yet

**As** a beginner arriving at Get Started from the design index, **I want** the page that says it does not teach the language to tell me what to do first, **so that** I am not stopped by two published bars that disagree: T10's 'teachable to a beginner' and Get Started's 'does not teach the language'.

XS · docs · E01 Know what Labrador is, and whether it fits, before installing · gaps G96 · **Later**

**Acceptance**

- Given website/src/content/docs/docs/get-started/index.mdx '## What you need to know already' (:18-21), when I read it, then it says what a reader who does not know C++ should do before continuing
- Given SRL-14's status sentence exists at that commit, then this section links it; otherwise the section makes no claim about the series
- Given the change, when website.yml runs, then it passes

**Trade-offs**

- T10: reconciles the yardstick with the page's scope without rewriting T10, which is in the target tense

**Notes.** The owner chooses which external starting point to name, if any; the story does not require recommending a particular resource.

<a id="srl-16"></a>
### SRL-16. A guide to the C++ and engine terms the minimal sample uses

**As** a learner with some C++ reading the 560-line minimal sample, **I want** one page that explains each language feature and engine term minimal relies on, pointing at the line that uses it, **so that** I can read the copy-me sample without stopping to research each construct.

M · content · E06 Concepts and reference that tell the truth a caller needs · gaps G97 · **Later**

**Acceptance**

- Given the site, when I open the new page or section, then it covers at least: inheritance and override; unique_ptr beside borrowed raw pointers (confirm_state.h:37, hello_state.h:52); a lambda capturing this (hello_state.cpp:144); std::span (hello_state.cpp:178); the SAL-annotated wWinMain (main.cpp:20); the using-directive in a .cpp (main.cpp:18); and the engine terms deferred context (hello_state.cpp:236), dynamic vertex buffer (main.cpp:35) and fan-out (hello_state.h:51)
- Given each entry, then it quotes the minimal line it explains through the site's SourceFile mechanism, so a change to that line fails the site build
- Given samples/minimal, when I diff it against the commit before this story, then no sample code was removed to simplify it
- Given the change, when website.yml runs, then it passes

**Trade-offs**

- T4: explain the sample rather than thin it
- T10: the API stays ordinary C++, so a gloss of ordinary C++ serves it
- T12: no dialect to explain, only the language

**Notes.** Close the gap on the site, not in the sample (register G97). The primary persona does not need language glosses, so the page sits off their path, for example linked from first-change.mdx and concepts/game.mdx, which already gloss two features. Line references are as of 0977112.

<a id="srl-17"></a>
### SRL-17. A recorded check that the game-facing API is teachable to a beginner

**As** a learner relying on T10's claim that the game-facing API stays teachable to a beginner, **I want** a written method for testing that claim and the results of one run, **so that** I can tell whether the claim has been checked and which calls are known to be hard.

M · process · E01 Know what Labrador is, and whether it fits, before installing · gaps G98 · **Later**

**Acceptance**

- Given the repository, when I look under docs/, then a document states a repeatable method, for example a surface audit of every game-facing header: parameter count per call, the C++ features a caller needs, and the engine terms used in caller-facing comments
- Given that document, then it records one dated run with its findings, including a verdict on draw_sprite's eight positional parameters (renderer.h:164-171)
- Given each finding the run marks as failing, then it is filed as a gap or decided against with a T-number
- Given the built site, then the run is reachable from a published page (for example a link from Get Started's 'What you need to know already' or the reference index)
- Given the change, when check_doc_citations.cmake and website.yml run, then both pass

**Trade-offs**

- T10: the game-facing API must stay teachable to a beginner, and if it is not, the fix is simplifying the API, never a scripting layer
- PHILOSOPHY.md:734-738 (The public face): the series is T10's named acceptance test and has never run; this audit is an interim check, not that test
- T4: the audit measures clarity at the call site

**Notes.** No beginner user is recorded, so the method must not depend on inventing one; an audit an agent can run fits within capacity. Independent of SRL-14: the series is T10's named test, and this is an interim check. Fixing what the audit finds is out of this story's scope.

<a id="srl-18"></a>
### SRL-18. Run LineSweeper's rules tests knowing what I need and where it works

**As** a learner who wants to run the headless match replays I am reading, **I want** a public page to tell me the exact commands to build and run LineSweeperTests, and which hosts are known to work, **so that** I can run the rules tests without discovering by failure that the whole engine, vcpkg and Windows are needed.

S · docs · E03 Prove my own game works: tests, smoke checks and CI · gaps G88 · **Later**

**Acceptance**

- Given samples/linesweeper/README.md or examples.mdx, when I read it, then it gives the configure command, a build limited to the test target (cmake --build --preset <preset> --target LineSweeperTests) and ctest --preset <preset> -R LineSweeperTests
- Given the same text, then it states that the root CMakeLists.txt always adds engine/ (:40) and that every preset reads VCPKG_ROOT for its toolchain (CMakePresets.json:11), so configure needs the engine's dependencies even though the test links only the rules library (tests/linesweeper/CMakeLists.txt:11-23)
- Given the same text, then it names the hosts and presets on which this has been run (the Windows presets, and the wasm presets under Node if checked) and says other hosts are unverified
- Given the change, when the top-level build and website.yml run, then both pass

**Trade-offs**

- T1: a rules-only build entry point for a tertiary persona has no client demand behind it, so document the existing path
- T4: state the build's real prerequisites and commands plainly for a reader seeing the repository cold

**Notes.** Negotiable: whether to try a non-Windows host under a wasm preset and record the result. A separate rules-only CMake entry point is not proposed (T1).

<a id="stories-anti-full-environment-seeker"></a>
## The Full-Environment Seeker (anti-persona)

<a id="fes-02"></a>
### FES-02. Get Started points at the exclusions before it asks for Visual Studio

**As** a Full-Environment Seeker who clicks the hero's Get Started button before reading Why, **I want** the Get Started index, which comes before 'Install the tools', to say in one sentence what kind of toolchain this is (C++ in your own IDE, JSON in a text editor, command-line converters) and link 'What Labrador is not', **so that** I learn there is no editor before I install Visual Studio, whichever link I clicked first.

XS · docs · E01 Know what Labrador is, and whether it fits, before installing · gaps G100 · **Next**

**Acceptance**

- Given website/src/content/docs/docs/get-started/index.mdx, when built, then before the first LinkCard it contains a sentence naming the toolchain shape and a link to /#what-labrador-is-not or /why/
- Given the built site, when /docs/get-started/ is opened, then the link resolves (npm run build in website/ passes and the link checker in website/tests passes)
- Given the change, then prerequisites.mdx is unchanged in its install steps, and the primary persona's five-step path keeps its five cards

**Trade-offs**

- T7: names data in a text editor as the authoring loop in place of an editor
- T10: C++ in your own IDE is the only behaviour language

**Notes.** get-started/index.mdx:8-21 currently goes straight to the five steps and to 'What you need to know already'. This is the cheapest part of G100's 'reaching the exit depends on which link the reader clicks first'.

<a id="fes-03"></a>
### FES-03. State the exclusions' permanence, the editor included, to match the recorded decision

**As** a Full-Environment Seeker reading the Why page and the home page, **I want** every public statement of the exclusions to say the same thing about which ones are permanent (online play, 3D, scripting, reflection and hot reload, and the editor, as CLAUDE.md already records) and which is provisional (native platforms), **so that** I can tell 'never' from 'not yet' and do not wait for an editor that is not coming, or rule out one that is.

S · docs · E01 Know what Labrador is, and whether it fits, before installing · gaps G99 · **Now** · **Owner**

**Acceptance**

- Given CLAUDE.md's 'Known-absent, on purpose' (online play, 3D, an editor and a scripting layer are 'permanently out of scope'), When docs/design/PHILOSOPHY.md 'Non-goals, today' (:781-792) is amended, Then it agrees, citing that decision rather than re-making it, and the owner signs off the wording
- Given website/src/content/docs/why.mdx:24-26, When read, Then 'yet' no longer qualifies the permanent exclusions, the permanent ones are named rather than counted, and only the platform is marked provisional
- Given index.mdx 'What Labrador is not', When read, Then each item carries a permanent or provisional marker consistent with why.mdx and PHILOSOPHY
- Given `git grep -n -i 'for now' -- docs/design/PHILOSOPHY.md website/src/content/docs/index.mdx website/src/content/docs/why.mdx`, When it runs, Then no hit qualifies an exclusion PHILOSOPHY calls permanent
- Given CI, When it runs, Then website.yml and check_doc_citations both pass

**Trade-offs**

- T7: data plus a text editor is the toolchain (Non-goals, PHILOSOPHY.md:791-792), and the amendment says whether 'for now' still holds
- T10 and T12: scripting, reflection and hot reload are named as permanent prices, not gaps

**Notes.** Critique: the editor's permanence is already decided in the tree (CLAUDE.md, 'Known-absent, on purpose'); G99's inconsistency is that PHILOSOPHY.md:781-792 still says 'the toolchain for now'. So this is an amendment bringing the target document in line with a recorded decision, with an owner sign-off rather than an open decision, and it is off the owner-decision list. PHILOSOPHY amendment A, with OCA-05, lands first of the PHILOSOPHY edits. FES-04 (Next) then edits the same list for completeness. Content tooling stays command-line either way.

<a id="fes-04"></a>
### FES-04. 'What Labrador is not' covers every want the seeker arrives with and names the alternatives

**As** a Full-Environment Seeker who scrolls to the foot of the home page, **I want** the 'What Labrador is not' list to cover scripting, hot reload and reflection-driven editing, and the small community, as well as editor, 3D, online and platforms, and to link the Why page's Godot and Unreal comparison, **so that** one list answers my whole checklist and tells me where to go instead.

XS · docs · E01 Know what Labrador is, and whether it fits, before installing · gaps G99, G100 · **Next**

**Acceptance**

- Given website/src/content/docs/index.mdx:129-147, when built, then the list names: no editor; no scripting language; no hot reload or reflection-driven property editing; 2D only; local play only; Windows plus a desktop browser; a small community with no asset store
- Given the same section, then it links /why/#with-godot-and-unreal (or the anchor the build generates for that heading), and the link resolves in the built site
- Given the list, then it stays at seven bullets or fewer, one line each, so the primary persona's 'Start here' cards remain on the following screen
- Given npm run build in website/, then it passes

**Trade-offs**

- T10: no scripting stated as a decision
- T12: no reflection or hot reload stated as the price already paid

**Notes.** The journey walk found the list omits scripting, hot reload and community and does not link Why or the alternatives (journeys.json anti-recognise-not-for-me, step 3). FES-03 (Now, amendment A) puts the permanence markers on the existing list; this story, moved to the top of Next by the critique, completes the list in a follow-up PR that keeps those markers.

<a id="stories-anti-native-multiplatform-shipper"></a>
## The Native Multi-Platform Shipper (anti-persona, for now)

<a id="nms-02"></a>
### NMS-02. Published design pages do not read Vulkan's reach as support

**As** a Native Multi-Platform Shipper who lands on the published ARCHITECTURE or PHILOSOPHY page from a search, **I want** the sentence saying Vulkan reaches Android, Linux and the Apple platforms to read as intent, with a pointer to what is built, **so that** I do not take a target-tense design sentence for present support.

XS · docs · E01 Know what Labrador is, and whether it fits, before installing · gaps G102 · **Now**

**Acceptance**

- Given docs/design/ARCHITECTURE.md:463-470 and PHILOSOPHY.md:395-404, when read, then the reach sentence is worded as the API meant to carry the engine there (target tense) rather than 'reaches', or the published page for each carries a site-side note linking /why/#platform-support
- Given the change, then no status line ('not built as of <date>') is inserted into docs/design, respecting the target-tense rule (G102 scope check)
- Given check_doc_citations.cmake and website.yml, then both pass

**Trade-offs**

- PHILOSOPHY.md:398-404: the passage already says Vulkan changes the cost of the cross-platform goal, not its status; the wording or site-side note carries that to a reader who lands mid-page
- T4: route versus support said plainly where a cold reader lands

**Notes.** website/src/content/docs/docs/design/index.mdx:20-25 carries a site-wide target-tense warning that a search landing skips. Prefer the site-side note if the owner wants ARCHITECTURE's wording left alone.

<a id="nms-03"></a>
### NMS-03. The Why page's Platform support section links the plans, their scope, and what would change the answer

**As** a Native Multi-Platform Shipper who reads 'not supported or built today' and wants to know what 'yet' means, **I want** the Platform support section to link docs/port/android.md on GitHub and state, with no date, that nothing for Android is built, that iOS follows Android, that the plan ends at 'the engine runs' without packaging or store work, that Linux and macOS have no plan, and what would have to be true before a platform is called supported, **so that** I can decide now whether to check back, and against what.

S · docs · E01 Know what Labrador is, and whether it fits, before installing · gaps G103 · **Later**

**Acceptance**

- Given website/src/content/docs/why.mdx 'Platform support', when built, then it links docs/port/android.md and states the four facts above, each traceable to android.md:3-5, :626-627 and :638-640, and to the docs/port/ listing
- Given `git grep -n -E '20[0-9]{2}|Q[1-4]|soon' -- website/src/content/docs/why.mdx` within that section, then no schedule or date for a native platform appears
- Given the section, then it carries a short 'supported when' list matching this backlog's promotion condition (client demand, a preset in CI, a device run, a measured p99, a packaging path)
- Given npm run build, then it passes and the GitHub link resolves

**Trade-offs**

- T1: the 'supported when' list makes client demand the trigger, never speculation (Non-goals, PHILOSOPHY.md:795-797)
- T4: names the absence, packaging and store work included, plainly for a reader deciding fit

**Notes.** No schedule may be invented (G103 scope check). android.md:8-9 calls mobile 'decided' only as intent. Keep to one short section, not a new page: capacity.

<a id="nms-04"></a>
### NMS-04. Configuring on a non-Windows host refuses early and points at the platform statement

**As** a Native Multi-Platform Shipper who ignores the site and runs `cmake --preset` on Linux or macOS, **I want** the x64 presets to be unavailable off Windows, and the engine's platform refusal to name the supported platforms and link the Platform support section, **so that** I learn it in seconds, not after vcpkg has installed dependencies.

S · build-ci · E02 From an empty Windows machine to my own project, built · gaps G104 · **Later**

**Acceptance**

- Given CMakePresets.json, When read, Then every x64-* configure preset carries a condition on ${hostSystemName} equal to Windows, and the wasm-* presets carry no host condition
- Given an ubuntu-latest runner or WSL, When `cmake --list-presets` runs in a clone, Then no x64-* preset is listed and both wasm-* presets are, and the result is recorded in the PR or commit message
- Given the docs (build-labrador.mdx or README), When they mention a non-Windows host, Then they say the wasm-* presets there are unverified, not refused; and if one Linux wasm configure is attempted, its result is recorded
- Given engine/CMakeLists.txt:103-107, When it fails, Then the FATAL_ERROR names 'Windows (Win32) and the browser (Emscripten)' as the supported platforms and gives the site's Platform support URL or README section
- Given the change, When reviewed, Then no Linux, macOS or other window, preset or shell source is added (see the G104 won't)

**Trade-offs**

- T5: push the error to the earliest stage, which is preset selection before any toolchain runs
- T6: the refusal names what is supported and where to read why

**Notes.** Critique: a blanket refusal would close the Emscripten route, which needs no fxc and has never been tried on Linux or macOS (G104 'never run'); the story was already scoped to x64-* presets, and now also states that wasm-* on a non-Windows host is unverified rather than refused. The first error on a non-Windows host has never been observed, so the acceptance records one observation. Must not grow into a Linux build.
