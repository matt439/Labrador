# Personas

Who Labrador is for, who it is not for, and how sure the repository can be of each. Taken on 2026-10-13 against `master` at `0977112`. [README.md](README.md) says how this set was made and how to read it.

Every claim below either cites the tree or sits under **Assumptions**, which are hypotheses to validate, not facts. No persona is a real person, and none has invented quotes, demographics or numbers.

## The set

| Tier | Persona | One line |
|---|---|---|
| primary | [The Outside C++ Adopter](#outside-cpp-adopter) | A C++ developer who did not write Labrador. |
| secondary | [The Pinned Submodule Integrator](#pinned-integrator) | A developer whose shipping game lives in its own repository and consumes Labrador as a pinned git submodule. |
| secondary | [The Sole Owner-Maintainer](#sole-maintainer) | The engine's owner and, on the evidence, its only maintainer. |
| tertiary | [The Browser and Phone Demo Player](#browser-demo-player) | A prospective visitor who would meet Labrador only as a game. |
| tertiary | [The Sample-Reading Learner](#sample-reading-learner) | Someone who reads LineSweeper, the components, the guides and the published headers to understand how a 2D game is structured in plain C++, not to ship one on Labrador. |
| anti | [The Full-Environment Seeker (anti-persona)](#anti-full-environment-seeker) | A team or individual who wants a complete environment: a visual editor, a scripting or visual-scripting layer, hot reload and reflection-driven property editing, or 3D or online play. |
| anti | [The Native Multi-Platform Shipper (anti-persona, for now)](#anti-native-multiplatform-shipper) | A developer who must ship native builds on Android, iOS, macOS or Linux now. |

## Why these tiers

**How the set was built.** Seven personas came from merging the three drafts and checking the load-bearing claims against the tree on 2026-10-13:
- **why.mdx:98-101** still says no sample draws a second view.
- **PHILOSOPHY.md:3-6** says the outside audience is for 'eventually'.
- **`git tag`** returns nothing.
- **Commit authorship:** 284 commits are by the owner's name and 2 by a second handle.
- **ci.yml:19-20** names one maintainer.
- **samples/minimal** has no README.
- **.github** contains only workflows/.

The skeptic draft has the decisive point: everything the repository does today serves one person in two roles, the maintainer and their own client. Everything the public face says is written for a stranger who has not appeared. The tiers below keep both facts visible rather than letting one hide the other.

**Primary: The Outside C++ Adopter.** It merges the evidence-first draft's Template Copier and Fit-Deciding Evaluator, the positioning-first draft's Constrained-Hardware Couch-Game Developer, and the skeptic's Hypothetical Outside Adopter. All four are one person at different stages: evaluate, copy, grow. They are primary even though unevidenced, for three reasons:
- The website, Get Started, the reference and the samples exist for them.
- PHILOSOPHY's stated reasons the engine is worth picking up are about them: a documented API, a sample game, teaching material and speed (PHILOSOPHY.md:3-6).
- The gap analysis has to measure the repository against someone. Every claim that they exist sits under assumptions.

**Primary: The Pinned Submodule Integrator.** ColourWars is the only consumer with real evidence: an exhibit ledger, five pin-bump commits, and source breaks that changed PHILOSOPHY's deletion rule (742-752). It shares an author with the engine, so it validates the integration path and not independent demand. That caveat is carried as the persona's first assumption. Two corrections from the drafts are kept:
- The pin is no longer stuck behind the math split. 77a7a7f migrated it, so the memory note is stale.
- The pin is 45 commits and 131 engine files behind master, and nobody has checked whether the client still builds there.

**Secondary: The Sole Owner-Maintainer.** The skeptic ranked this persona primary. I placed it secondary because it is the decider and the constraint on capacity, not an audience the product is designed toward. T9 makes the engine the maintainer's to defend, but T1 and PHILOSOPHY:3 point the product at clients and strangers. Its stories bound the backlog: hardware verification debt, drift, local multi-SDK checking. They likely belong in an internal track. Outside contributors are folded in as a hypothesis, because there is no CONTRIBUTING.md, the rules are addressed to agents, and no outside author is evidenced.

**Tertiary: The Browser and Phone Demo Player.** This persona is real in intent and heavily built for: touch, portrait layout, S3TC fallback, a 151 KB click-to-load module. But it is undeployed, phones are gated off, and the design documents treat it as a demo channel rather than a named user. The claim that most visitors are on phones has no data behind it.

**Tertiary: The Sample-Reading Learner.** This combines the skeptic's Promised Beginner with the reader arriving from Unreal. T10 makes the beginner the acceptance test for the API, but the promised C++ series has no artefact, and Get Started says it does not teach the language. Under T10's own test, the header density counts as a finding, not a preference.

**Considered and not made personas:**
- **The low-tier player:** the tier is named but unmeasured, so it is an assumption inside the adopter, demo-player and anti-native personas.
- **The host-site embedder:** the only named host is the owner's own site.
- **The Android porter:** nothing is built for Android.
- **ColourWars' Release ZIP players:** folded loosely into the demo player.

**Anti-personas.** The skeptic's split is better grounded than one combined anti-persona or three separate ones, because the two kinds of exclusion differ in permanence:
- **The Full-Environment Seeker** wants an editor, scripting, hot reload, 3D or online play. Each is a permanent non-goal grounded in T10, T12 and PHILOSOPHY's non-goals (785-797), and the site already turns them away.
- **The Native Multi-Platform Shipper** is excluded only for now. engine/CMakeLists.txt:103-106 refuses other platforms, Android is unbuilt, and MoltenVK and the low tier are unmeasured. The persona exists so that Vulkan-as-a-route and constrained-hardware statements are not read as present support, and it records the condition under which it would be promoted.

**Tensions for the next stages:**
1. The differentiator (constrained hardware, under T8) is unmeasured and nearly absent from the public face.
2. The browser is a headline capability, but the consumer walkthrough is Windows/x64 only and has no test target, despite LineSweeper's headless-rules pattern.
3. PHILOSOPHY's boundary table promises action mapping and mixing, which do not exist; this is a design-tense gap.
4. The public face contradicts itself (why.mdx:98-101), which undercuts the docs-match-code promise. This is a T6-style trust failure.
5. With no tags or releases, source breaks fall on the integrator and would fall on any adopter.
6. Every external persona is a hypothesis: there are no analytics, no issues and no independent adopter.

<a id="outside-cpp-adopter"></a>
## The Outside C++ Adopter

**Tier:** primary · **id:** `outside-cpp-adopter`

A C++ developer who did not write Labrador. They want to build a local, possibly split-screen, 2D sprite game in ordinary C++ and be able to read and test everything underneath it. They evaluate the engine on the public face, copy samples/minimal into their own repository with the engine as a git submodule, and grow it into a game, possibly with a browser build too. Every public artefact (the website, Get Started, the API reference, the samples) is written for this person, but the repository records no instance of them: PHILOSOPHY calls outside developers an intended audience 'eventually'. They are primary as the target the public face is built for, not as a validated user. This persona merges the drafts' Template Copier, Fit-Deciding Evaluator, Constrained-Hardware Couch-Game Developer and Hypothetical Outside Adopter.

**Context.** STATED: starts from a Windows machine with Visual Studio 2022 or newer and its C++ workload, Git for Windows and the x64 Native Tools Command Prompt, which sets VCPKG_ROOT when Visual Studio's vcpkg package manager component is installed; Get Started says how to add the component or set the variable otherwise (website/src/content/docs/docs/get-started/prerequisites.mdx:8-46; corrected 2026-10-30, when the record's 'which sets VCPKG_ROOT' was found to hold only with that component). Arrives through the GitHub README or the website, which compares Labrador with Godot, Unreal, raylib, SDL and SFML (website/src/content/docs/why.mdx:135-172). Native builds are Windows-only: engine/CMakeLists.txt:103-106 stops configure on any platform other than Win32 or Emscripten. Optionally targets a WebGL2 page (samples/minimal/main_web.cpp:1-3; why.mdx:191-195). The stated sizing target for their players is the named low tier (docs/design/PHILOSOPHY.md:677-690), which is unmeasured (PHILOSOPHY.md:691-699). STATED: everyone who writes behaviour for a Labrador game writes C++ (PHILOSOPHY.md:173). Team size is not stated anywhere.

**Skills.** *Amended 2026-10-30 by the owner's decision (BACKLOG.md, Decisions only the owner can make):* the floor is someone with very little programming experience who wants to play around with the engine and is not expected to get much done. Terminal commands and manual setup are acceptable, but every procedure has an exhaustive guide, with every command in a copyable block and nothing left to infer; experienced users skip the guides. Starting needs no Git: a release ZIP is downloaded, extracted, built and played, and Git stays the route for experienced users (OCA-78). The rest of this paragraph is the 2026-10-13 record. The stated floor was 'some C++, and how to use a command prompt'. Get Started explains every command but does not teach the language (website/src/content/docs/docs/get-started/index.mdx:18-21). They must follow CMake and vcpkg steps in a vcvars shell (prerequisites.mdx:22-46). A browser build also needs Emscripten with EMSDK set (README.md 'Prerequisites'). Get Started does not mention the content tools (texconv, MakeSpriteFont, xwbtool); a grep of website/src/content/docs/docs/get-started/ finds none.

### Goals

- Decide quickly and honestly whether Labrador fits their game and what each choice costs (website/src/content/docs/why.mdx:10-26; website/src/content/docs/index.mdx:65-111, 133-145)
- Get from nothing to their own running project in their own repository (get-started/index.mdx:8-16; get-started/your-project.mdx steps 1-8, lines 39-110)
- Build a game played on one machine, alone or with friends on a shared or split screen with gamepads, with a camera for each view and menu focus for each player (why.mdx:20; PHILOSOPHY.md:585-588, 785-788; engine/render/viewport_manager.h:13-51; engine/ui/focus.h:59-64)
- Test game rules headlessly like any other code, following LineSweeper's rules-library pattern (why.mdx:22, 108-133; samples/linesweeper/README.md 'The rules are a static library that links nothing')
- Read and understand any part of the engine under their game (why.mdx:10-11, 19; PHILOSOPHY.md:83-87 T4, 714-716)
- Have the game run well on modest hardware. Speed is stated as a reason to choose the engine (PHILOSOPHY.md:139-142 T8, 677-690)
- Optionally ship the same game source to a browser (samples/minimal/main_web.cpp:1-3; README.md:180-183; engine/app/application.h:148-161)
- Get load-time errors that name the file or resource (PHILOSOPHY.md:109-122 T6; engine/render/render_resources.h:130-133)

### Frustrations

- The public face contradicts the tree. why.mdx:98-101 says 'Neither sample in the repository draws a second view yet... A public multiplayer example is planned', while samples/local_multiplayer ships and is listed in website/src/content/docs/examples.mdx:110. The platform is stated inconsistently: the Home facts strip says 'Windows today' (index.mdx:29) and why.mdx:24-25 rules out 'a platform other than Windows', while index.mdx:137-139 and why.mdx:191-195 say all six samples run in a browser.
- The design documents are written in the target tense and promise what the code deliberately lacks. PHILOSOPHY.md:449 says the engine provides 'action mapping', and ARCHITECTURE.md:175-179 says the action map is deliberately not built.
- samples/minimal has no README (directory listing: CMakeLists.txt, content, main.cpp, main_web.cpp, states, web). Its header comment says only 'copy samples/minimal/, rename the target' (samples/minimal/main.cpp:1-3). The include rewrites, the include root, the consumer vcpkg.json and the presets are on the website only (your-project.mdx:64-96).
- The consumer walkthrough drops the browser build and tests. Its presets are x64-debug and x64-release only, and its game CMakeLists has no EMSCRIPTEN branch and no test target, so the main_web.cpp they copy is not built (website/src/walkthrough/CMakePresets.json; website/src/walkthrough/game-CMakeLists.txt vs samples/minimal/CMakeLists.txt:18-36). README.md:180-183 still presents minimal as the shape of a browser build.
- Setup is heavy and Windows-only: specifically the x64 Native Tools prompt, VCPKG_ROOT, long paths, fxc, and a first configure that takes 'several minutes' (prerequisites.mdx:22-60; troubleshooting.mdx 'Setting up and cloning', 'Configuring'; build-labrador.mdx:31)
- There is no solid-shape draw: LineSweeper draws its whole screen from one white texel (samples/linesweeper/README.md 'The whole screen is one white texel'). The content formats are .dds, .spritefont and .xwb (engine/render/dds_file.h; engine/render/sprite_font_file.h; engine/audio/xwb_file.h:56-64), and Get Started does not say how to produce them.
- API surprises. layer_depth orders nothing on any backend (engine/render/texture_object.h:59-60). A plain GameObject is never retired from a Scene: only collision objects are swept (engine/scene/scene.cpp:127-146; engine/scene/scene.h:115-134). Camera offers no point inverse from screen to world (engine/render/camera.h:50-52, 82-84). The D3D11 clear colour is hard-coded black (engine/render/d3d11/renderer.cpp:832-833). draw_sprite takes eight positional arguments (engine/render/renderer.h:163-171). A first collidable needs nine pure virtuals (engine/core/game_object.h:44,57,75; engine/collision/collision_object.h:30-55).
- If they count rules in ticks, nothing in the engine pins target_fps, and changing it silently changes the game's speed (engine/app/application.h:58-60; samples/linesweeper/main.cpp:37-43). minimal does not warn about it. The impact on them is a hypothesis.
- Template comments point at code they cannot see: 'game/objects/level.cpp has one' (samples/minimal/states/hello_state.cpp:243) and the paint-shooter and game/ (samples/minimal/CMakeLists.txt:11-12), both in the private ColourWars repository (README.md:7-15)
- The documented path applies the engine's zero-suppression /W4 /WX to their own game code, and the docs ask them not to turn it off (README.md:170-174; your-project.mdx:83-86; troubleshooting.mdx 'A warning stops the build'). Nothing enforces it: the engine links labrador_settings PRIVATE (engine/CMakeLists.txt:391).
- There are no releases to pin to. `git tag` returns nothing (run 2026-10-13), and docs/website-versioning.md:8-11 records no tags or GitHub releases. Versioned releases are promised only once the API settles (PHILOSOPHY.md:739-741; index.mdx:33).
- The constrained-hardware differentiator is unmeasured: 'no number here is a floor' (PHILOSOPHY.md:691-699). The site says it publishes no frame-time figures (why.mdx:186-189).

### What Labrador offers them

- A fit page in which every choice states its cost, with comparisons that concede where Godot, Unreal, raylib, SDL and SFML are better (why.mdx:10-26, 135-172)
- A template of about 560 lines that the engine's own build compiles as a permanent second client, and an eight-step submodule walkthrough whose include changes and consumer manifest are generated from the checkout (README.md:17-20; samples/minimal/CMakeLists.txt:1-12; your-project.mdx:39-110; website/src/components/IncludeChanges.astro; website/src/components/ConsumerManifest.astro)
- Split-screen machinery: one- to four-player layouts, per-view fan-out and focus per input slot (engine/render/viewport_manager.h:13-51; engine/scene/scene.h:51-67; engine/ui/focus.h:59-64; samples/local_multiplayer)
- Headless null backends and a rules-library pattern (PHILOSOPHY.md:762-767; why.mdx:108-133; samples/linesweeper/README.md 'The rules are a static library that links nothing')
- Seven task guides, five concept pages and a generated reference for every public header (website/src/content/docs/docs/index.mdx:16; website/src/content/docs/docs/guides/; website/src/content/docs/docs/concepts/; website/src/lib/published.ts)
- Settings-menu components to copy rather than link, with no compatibility promise (samples/components/README.md:1-33; PHILOSOPHY.md:731-735)
- Troubleshooting organised by error message (website/src/content/docs/docs/troubleshooting.mdx)
- A promise that each Get Started step was run as written, with failures treated as page bugs (get-started/index.mdx:23-30)

### What would make them leave

- Getting stuck before a first green build on environment, prompt or VCPKG_ROOT problems. This is a hypothesis, based on how much of troubleshooting.mdx ('Setting up and cloning', 'Configuring') covers them.
- Finding a public claim that the code contradicts, which undercuts the site's promise that its pages match the code (why.mdx:98-101 vs examples.mdx:110; website/README.md)
- Finding that their browser or test target has to be reverse-engineered from in-tree sample CMake (website/src/walkthrough/game-CMakeLists.txt vs samples/minimal/CMakeLists.txt:18-36). This is a hypothesis.
- Needing an editor, scripting, hot reload, rebinding, 3D, online play or a native platform other than Windows (why.mdx:24-26; PHILOSOPHY.md 'Non-goals, today'; CLAUDE.md 'Known-absent, on purpose')
- Source breaks with no release to pin to (git tag empty; PHILOSOPHY.md:739-741). This is a hypothesis.
- Finding that the low-tier performance claim does not hold on their target machine (PHILOSOPHY.md:691-699). This is a hypothesis.

### Evidence

| Claim | Source |
|---|---|
| The public face addresses developers who want to write games in ordinary C++ and understand the machinery beneath them | website/src/content/docs/index.mdx:3,7; website/src/content/docs/why.mdx:10-11; docs/website-plan.md:23 |
| Outside developers are an intended but future audience ('intended, eventually') | docs/design/PHILOSOPHY.md:3-4 |
| minimal is the stated new-project template | samples/minimal/main.cpp:1-3; README.md:17-20; PHILOSOPHY.md:717-719 |
| The Why page's claim that no sample draws a second view is stale | website/src/content/docs/why.mdx:98-101 vs website/src/content/docs/examples.mdx:110 |
| The walkthrough is Windows/x64 only and has no test target or browser branch | website/src/walkthrough/CMakePresets.json; website/src/walkthrough/game-CMakeLists.txt |
| No release tags | `git tag` returns 0 entries (run 2026-10-13); docs/website-versioning.md:8-11 |
| The GPT-6 review proposed a tested external-project starter and reproducible content preparation | docs/review/gpt6/README.md:320 (opportunity 1), :345 (opportunity 6) |
| The template once failed 'in the first thirty seconds' without a pad, and has since been fixed | docs/survey/2026-08-26.md §2.2 (line 87); docs/survey/2026-08-26-status.md |
| Engine only, not the consumer, links labrador_settings PRIVATE | engine/CMakeLists.txt:391 |
| Public names are deleted only on a caller count that names the repositories counted; a count that cannot be taken is a reason to keep | docs/design/PHILOSOPHY.md:742-752 |

### Assumptions to validate

- That this person exists at all. The repository cites no outside user, issue, download or analytics: README.md:7-15 names only ColourWars, and git shortlog on 2026-10-13 shows 284 commits by the owner and 2 by a second identity with the same handle.
- That the couch-multiplayer audience and the constrained-hardware audience are the same people, rather than two audiences the positioning combines
- That they want a browser build of their own game and not only the demos. This is inferred from main_web.cpp being what 'a new project that wants a page copies'; the walkthrough leaves it out, so demand is untested.
- That they work alone or in a small team. The repository never states team size; it states only that all behaviour is C++ (PHILOSOPHY.md:173).
- That ordinary C++ without scripting attracts them rather than being something they tolerate
- That evaluators and adopters share one journey closely enough to be one persona (evaluate, then copy). If the reader who has not yet adopted turns out to be a separate audience, split them out.
- That they would accept native builds being Windows-only
- That they will need content tooling beyond what the samples ship (.dds, .spritefont, .xwb authoring). Nothing records this as a demand.
- That the API surprises listed under frustrations would actually trouble an adopter. They are observed in the code, but no user has reported them.

<a id="pinned-integrator"></a>
## The Pinned Submodule Integrator

**Tier:** secondary · **id:** `pinned-integrator`

A developer whose shipping game lives in its own repository and consumes Labrador as a pinned git submodule. They file reports, with their workaround as the exhibit, instead of patching the engine, and they move the pin deliberately. ColourWars, a private local split-screen paint-shooter for up to four players, is the only instance, and the engine's author writes it, so this persona is one hat of the same human as the Sole Owner-Maintainer. It is the only evidence of real use and the proving ground for the submodule path an outside adopter would take. It validates that integration path, not independent demand, and PHILOSOPHY makes its needs subordinate to the engine's.

**Context.** Private repository C:\VSProjects\ColourWars, engine at external/labrador pinned to 4cefb68 (git ls-tree HEAD external/labrador). That pin is 45 commits behind Labrador master 0977112, and the range changes 131 engine files, including engine/ui/widget.h, engine/ui/navigation.h, engine/app/content_root.* and engine/render/text_encoding.* (git diff --stat 4cefb68..0977112 -- engine). Whether the client builds at master is unverified. Windows-only, on the d3d11/xaudio2 defaults, with no browser build (ColourWars CLAUDE.md:13-21). It ships a Release ZIP for Windows 10/11 x64 through its own install rules (ColourWars README.md:210-245; cmake/release_package.cmake). It has no ctest of its own. Acceptance and startup checks run through PowerShell scripts in its own CI (ColourWars CLAUDE.md:94-121).

**Skills.** Expert C++ with deep knowledge of the engine's internals (same author, per both repositories' git authors). They write deliberately full-OOP game code, which PHILOSOPHY's T11 scoping permits (ColourWars CLAUDE.md:264-295). They argue reports from PHILOSOPHY's stated positions and the exhibit rule (ColourWars docs/review/engine-reports.md:48-61, 598-602).

### Goals

- Consume the library only, without the engine's tests or samples and without a duplicate settings target (Labrador CMakeLists.txt:42-47; ColourWars CLAUDE.md:144-147)
- Move the pin forward deliberately and learn what each move breaks or closes (ColourWars README.md:247-278; CLAUDE.md:149-156)
- Get engine gaps answered upstream by filing exhibit-backed reports (ColourWars CLAUDE.md:149-156; engine-reports.md:48-61)
- Ship a distributable Windows build that players run without Visual Studio or vcpkg (ColourWars README.md:210-245)
- Trace a shipped build to exact game and engine revisions (ColourWars README.md:226-230)
- Serve couch-multiplayer players: shared music/SFX gain without per-emitter mixing (engine-reports.md:86-100) and per-view cameras that stay correct if zoom is added (engine-reports.md:556-567)

### Frustrations

- Pin moves are manual. One (77a7a7f) was a large source break, and later ones cost an include (a9a7cd3), a loading-order fix (1ae9797) or nothing. Each one needs a build, manual verification and a re-read of the report ledger (README.md:247-278; engine-reports.md:658-680, 736-775)
- Source breaks forced coordinated rewrites. artattack:: became labrador:: in about forty files, the matt_math.h umbrella became thirteen header pairs and twelve client files were re-pointed (commit 77a7a7f), and FocusGroup::activate's new return type broke five call sites (engine-reports.md:423-428)
- Public API the client called was deleted as apparently uncalled: RectangleF::contains, area and inflate_to_size, and Vector2F::rotate_vector (commit 77a7a7f; engine-reports.md:496-521). A header move broke game/pch.h (engine-reports.md:671-677)
- Seven reports are live unfiled drafts with exhibits: 6 audio bus/master gain, 9 app-level frame hook/input capture, 10 frame readback, 11 F10/lone Alt to the system menu, 12 read-only path-only JSON, 13 key names, 14 frame stats (engine-reports.md:9, 86-376)
- Labrador defines no install(), export() or package config, and no content-staging helper. The client hand-enumerates content copies and wrote its own release packaging (gap-audit.md:389 F8; game/CMakeLists.txt:175-227; cmake/release_package.cmake)
- Reports have to be rechecked against engine master rather than the pin. Report 1 described a bug already fixed (engine-reports.md:40-46)
- Post-bump gameplay is still verified by hand. One UI-automation attempt failed to advance the title screen, and the record says this is not evidence of an input regression (engine-reports.md:858-862)
- CameraTools' viewport size is in world units while the client passes pixels, which is correct only because it never zooms (engine-reports.md:556-567; engine/render/camera_tools.h:12-29)

### What Labrador offers them

- Samples, tests, bench and tools are gated behind PROJECT_IS_TOP_LEVEL, so the submodule brings only the library and labrador_settings (Labrador CMakeLists.txt:42-47)
- The include-wall check is a dependency of labrador_settings, so it runs in the client's targeted build (Labrador CMakeLists.txt:11-38; engine-reports.md:740-746)
- Fast answers to exhibit-backed reports: six of the seven filed were implemented the day they were filed (engine-reports.md:58-61, 380-546). Owner self-service, see assumptions
- Scene, state stack, ResourceLoader::register_kind, a content root beside the executable with public pieces for a custom loading order, and optional silent sound banks (ColourWars CLAUDE.md:123-134, 174-239; engine-reports.md:748-773)
- A written deletion rule: a public name is removed only on a caller count that names the repositories counted (PHILOSOPHY.md:742-752)
- Copyable components such as OptionRow, shaped from the client's own copy (samples/components/README.md:22-33)

### What would make them leave

- Breaks arriving faster than pin moves can absorb them, freezing the client on an old pin. No tags exist (git tag is empty), and versioned, batched releases are promised only once the API settles (PHILOSOPHY.md:739-741)
- Exhibit-backed reports no longer being acted on, so workarounds become permanent (hypothesis)
- A deletion of API the client calls, because the cross-repository caller count is a discipline rather than a check (PHILOSOPHY.md:742-752)

### Evidence

| Claim | Source |
|---|---|
| ColourWars consumes Labrador as a submodule and owes the engine reports rather than patches | C:\VSProjects\ColourWars\CLAUDE.md:136-159 |
| The paint-shooter is the first client and the proof, the engine is meant for developers other than its author, and the engine wins conflicts | docs/design/PHILOSOPHY.md:3-9 |
| Seven live reports (6, 9-14), drafted but not filed upstream | C:\VSProjects\ColourWars\docs\review\engine-reports.md:9, 67-376 |
| After 77a7a7f the pin moved five more times: 64ed674, a9a7cd3, 1ae9797, 70efadf, fa513d4 (now 4cefb68) | ColourWars git log -- external/labrador |
| Same human authors both repositories | ColourWars git log authors (Matt Harding, matt439); Labrador git user Matt Harding |
| Labrador has no install()/export(); the client wrote its own packaging using TARGET_RUNTIME_DLLS | Labrador CMakeLists.txt and engine/CMakeLists.txt (no install()/export() calls); ColourWars cmake/release_package.cmake:20-30; gap-audit.md:389 (F8) |
| Up to four players, local split-screen | C:\VSProjects\ColourWars\README.md:3, 34 |

### Assumptions to validate

- That a second, independent integrator would hit the same frictions. The only one shares an author with the engine and controls upstream, so the turnaround and goodwill may not generalise
- That reports 6 and 9-14 are validated needs and not one game's preferences, since they have not been filed upstream
- That integrators would want tagged releases more than raw-commit pins. This is inferred from the friction and from PHILOSOPHY.md:739-741, not stated by the client
- That gap-audit items F6 (rumble) and F10 (colour transform) are still open. They date from 2026-08-11 and have not been rechecked against master
- That post-bump gameplay verification is generally hard to automate. One failed attempt is recorded
- That the client's Win32/GDI level editor reflects an engine gap. The repository does not say (ColourWars editor/CMakeLists.txt:5-11)

<a id="sole-maintainer"></a>
## The Sole Owner-Maintainer

**Tier:** secondary · **id:** `sole-maintainer`

The engine's owner and, on the evidence, its only maintainer. They work largely through AI agents (Claude and Codex) steered by CLAUDE.md and AGENTS.md, make every recorded design decision, and hold the hardware and the deploy. They keep five render, three audio and two input backends, a browser build, a website and a review ledger consistent with each other. They are also the author of the only client, ColourWars, so they are the same person as the pinned integrator in a second role. Their capacity and hardware bound the backlog. The tier is secondary because the public artefacts are written for outside adopters, but this is the only user of the repository the evidence shows. Outside human contributors appear nowhere and are only a hypothesis.

**Context.** One maintainer: .github/workflows/ci.yml:19-20 ('a repository with one maintainer'). Commit authorship: 284 of 286 commits under the owner's name and 2 under a second handle (git log --format=%an, 2026-10-13). 262 commit bodies carry a Co-Authored-By: Claude trailer. AGENTS.md is a Codex brief that mirrors CLAUDE.md. Hardware is the desktop and its integrated Radeon, which is the named low tier as a configuration (docs/survey/2026-08-26-status.md decision 1; PHILOSOPHY.md:677-710). A 2013 MacBook Air is a blocked candidate, and a Pi 3B+ is on the wrong axis (same decision). Deploying to mattharding.dev is the owner's and is not done (docs/port/web.md §7 and the M5 row at :1519, :1461-1462). The owner answers design decisions on the record (docs/port/web.md §8). There is no CONTRIBUTING.md, and .github holds only workflows/ (checked 2026-10-13).

**Skills.** Strong C++20, graphics APIs (D3D11/12, GL, Vulkan, WebGL2), CMake, vcpkg, Emscripten and CI, evidenced by sole authorship of the tree. They use the T1-T12 review vocabulary (PHILOSOPHY.md:25-29). They commission and adjudicate adversarial reviews rather than, as far as the evidence shows, writing every one by hand (docs/review/*; commit trailers).

### Goals

- Keep an engine its author understands completely and can defend line by line (PHILOSOPHY.md:153-169, T9)
- Treat a gap between a document and the code as a defect, and keep live state in status companions beside frozen reviews (docs/survey/2026-08-26.md §7; docs/review/backend-equivalence-2/README.md §6)
- Take a p99 on the named low tier, and settle the frame bench's two clocks so that the p99 can be quoted (docs/review/backend-equivalence-2/STATUS.md:528; docs/performance/2026-09-19-g6f-ratchet.md §5)
- Close the web milestones that wait on hardware, a browser or the deploy: Safari, R5 pacing, real phones, touchscreens and pads, and M5 live (docs/port/web.md §9; docs/port/web-to-be-tested.md)
- Evolve the API without stranding clients, deleting a public name only on a caller count that says which repositories were counted (PHILOSOPHY.md:742-752)
- Eventually make the engine worth picking up for developers other than its author (PHILOSOPHY.md:3-6)

### Frustrations

- Verification that only hardware, a browser or a person can supply: no low-tier p99 (backend-equivalence-2/STATUS.md:528), Safari unchecked and no real phone, touchscreen or physical pad (docs/port/web-to-be-tested.md; CLAUDE.md), Web Audio output heard by nobody (CLAUDE.md, AudioTests paragraph), and two frames in flight never run (docs/review/vulkan/README.md:203, :610)
- Unverified review questions remain: 65 Vulkan §7 items and the two open §6 items, and 7 D3D12 §6 behaviour items (CLAUDE.md, 'Reading the documents correctly'; docs/review/vulkan/README.md:601, :627)
- Prose drifts faster than code. A sweep once found 113 drifted comments in engine/render/, all since applied (backend-equivalence-2 README:19; STATUS.md). Fresh drift is already visible: ARCHITECTURE.md:133-134 says 'ten in all' against 12 configure presets in CMakePresets.json, and PHILOSOPHY.md:663 says no sample has a second view while samples/local_multiplayer draws two
- Green checks are narrower than they look. The citation check proves name closure only (cmake/check_doc_citations.cmake:11-25), and the include check does not enforce the module table (docs/review/2026-09-26/README.md, 'Architecture, maintenance, and test priorities' item 5)
- Full local verification of a render change needs the Vulkan SDK, EMSDK, a Chromium, vcvars and real drivers, and the browser page tests are absent rather than skipped where no Chromium is found (CLAUDE.md preset table; CMakeLists.txt:105-125)
- No build step can count client callers, so deleting a name rests on a manual count that goes stale. Live client API was deleted twice in one afternoon (PHILOSOPHY.md:742-752; docs/file-length-audit.md step 4, rectangle_point_intersect note)

### What Labrador offers them

- T1-T12 as a shared review vocabulary for themselves and their agents (PHILOSOPHY.md:25-29; CLAUDE.md; AGENTS.md)
- Checks that fail the build: zero-suppression warnings under MSVC and clang, include walls, document citations, and website quote checks (cmake/settings.cmake; cmake/check_engine_includes.cmake; cmake/check_doc_citations.cmake; .github/workflows/website.yml)
- A golden image set held byte for byte across the four rasterising backends, and CI over six x64 presets plus the wasm build and a null-backend wasm configure (tests/render/golden/; .github/workflows/ci.yml:96-111, :284-392)
- Frozen reviews with live status companions (docs/review/backend-equivalence-2/STATUS.md; docs/survey/2026-08-26-status.md)

### What would make them leave

- Hypothesis, stated nowhere: the upkeep of five backends, two compilers, the website and two ports grows past what one person and their agents can carry, and the positioning ends up resting on unmeasured claims
- Losing the ability to defend every line, for example through bought middleware in the core (PHILOSOPHY.md:153-169, T9)

### Evidence

| Claim | Source |
|---|---|
| One maintainer | .github/workflows/ci.yml:19-20 |
| Near-sole authorship | git log --format=%an (2026-10-13): 284 commits under the owner's name, 2 under a second handle |
| Heavy AI-agent co-authorship | git log --format=%B: 262 'Co-Authored-By: Claude' trailers; AGENTS.md is a Codex brief |
| The low tier is a desktop configuration; the Air is a blocked candidate; the Pi cannot help | docs/survey/2026-08-26-status.md decision 1; PHILOSOPHY.md:677-710 |
| The deploy is the owner's and is not done | docs/port/web.md §7 (:604-620), :1461-1462, M5 row :1519 |
| The owner answers design decisions on the record | docs/port/web.md §8 (:767-831) |
| Two open boxes in the live render ledger | docs/review/backend-equivalence-2/STATUS.md:445, :528 |
| No scaffolding for human contributors | ls .github shows only workflows/; no CONTRIBUTING.md (2026-10-13) |

### Assumptions to validate

- That maintainer capacity, not ideas, is the binding constraint on the backlog
- That the maintainer counts as a user for backlog purposes; their stories may belong in an internal track
- That outside human contributors will come. None appear in the evidence, so CONTRIBUTING and local-check guidance are hypothetical needs
- That the owner wants outside adopters soon rather than 'eventually' (PHILOSOPHY.md:3)
- That the owner's needs and the outside adopter's needs coincide; nothing in the repository tests this
- That the second commit handle is the same person (likely, but not verified)
- That the owner runs reviews personally rather than only commissioning and adjudicating agent-run ones

<a id="browser-demo-player"></a>
## The Browser and Phone Demo Player

**Tier:** tertiary · **id:** `browser-demo-player`

A prospective visitor who would meet Labrador only as a game. They would play LineSweeper on the owner's personal site, at mattharding.dev's /demo/linesweeper/, or on another host that embeds the ES module. On desktop they would use keys or a pad. On a phone they would eventually use touch. They never see the engine. They are the end user of the browser build's input, pacing, error and context-loss promises, and the public proof of the browser claim. Today they cannot reach the game: the deploy is not live, phones are gated off the host page, and Safari, real phones and physical pads are unchecked.

**Context.** The deploy is not live, so this persona is prospective. web.md §7 says 'it is not deployed yet', the M5 row's done-when is 'The demo plays from the live URL', and web-to-be-tested.md 'Needs the deploy' is unticked. Labrador's own website does not host the demo and shows footage instead (docs/website-plan.md:44, :90). Their browser would be desktop Chrome or Firefox, which were checked headlessly (CLAUDE.md), or Safari, which was never checked (web-to-be-tested.md 'Needs Safari'). On a phone, the bundle's page shows a poster and a line asking for a keyboard or controller, and mattharding.dev's page shows the poster and a 4.3 MB recording (web.md §7.4). Only the development page, samples/linesweeper/web/shell.html, lets a phone play. The build is single-threaded wasm, and a visitor fetches 151 KB under Brotli, only after a click (web.md §7.1, §7.3).

**Skills.** None beyond playing a falling-block game. The in-game hint lines name the keyboard and pad controls in landscape (samples/linesweeper/states/play_state.cpp:34-38). The host page's status line names the keys too, including Escape to pause (samples/linesweeper/web/index.html, status paragraph).

### Goals

- Start playing without installing anything or downloading the game before they choose to (samples/linesweeper/web/index.html header 'NOTHING IS DOWNLOADED UNTIL A CLICK'; web.md §7.3)
- Play with the device they have: keyboard, pad or touch (samples/linesweeper/states/play_state.cpp:55-95 binding tables; samples/linesweeper/README.md 'Input is read as held, never as pressed', 'A phone held upright')
- Be told why when their device cannot run the game, or when the graphics are lost (web.md §7.3, onError and onContextLost)
- Hypothesis: smooth, stable pacing. web.md R5 is the related risk, and it has not been measured on real displays.

### Frustrations

- Phones are gated off the host page although the module links touch controls and a portrait layout. The page holds them back 'until a real iPhone and a real Android phone have played it' (samples/linesweeper/web/index.html 'A PHONE IS ASKED FIRST'; web.md §7.4)
- Browsers and devices that are unchecked are the risk this player would carry. Safari was never checked. Firefox is outside the automated page runner. R5 pacing has not been measured on real displays. The pad path was driven only by a page script's fake pad, and touch only by Chrome emulation (docs/port/web-to-be-tested.md 'Needs Safari', 'Firefox, which the page runner cannot drive', 'Needs real displays (R5)', 'Needs a gamepad (M4)', 'Needs a touchscreen (M7a)'; web.md §9 'Where it stands')
- The in-game hints leave out pause and are hidden in portrait and after any touch. The in-game rotate keys ('Z X rotate') differ from the host page's ('Up or X rotates') (play_state.cpp:34-38, 197-205; index.html status line)
- There is no rebinding, by design (play_state.cpp binding-table comment citing T1; CLAUDE.md 'Known-absent, on purpose')
- LineSweeper has no audio (pause_state.cpp:309-311; web.md §10). No score persistence was found in samples/linesweeper. That is an observed absence, not a documented decision.

### What Labrador offers them

- A start-on-click page that fetches only the HTML and a 15 KB poster before the click (samples/linesweeper/web/index.html; web.md §7.1, §7.3)
- Art and text that still draw on a browser without S3TC, through CPU block decode, held to the golden images by RenderPixelTestsWithoutS3tc (engine/render/block_decode.h; web.md D14; CLAUDE.md)
- Touch controls and a portrait arrangement linked into the module (samples/linesweeper/presentation/touch_controls.cpp, arrangement.cpp). The safe-area and portrait page handling lives only on the development page (samples/linesweeper/web/shell.html:35-55), and the host page holds all of it behind the gate (web.md §7.4)
- Restore after a lost WebGL2 context, with the page told at both ends. A failure the engine cannot recover from reaches the page as words through onError (web.md §7.3; CLAUDE.md, the RenderContextLossTests paragraph)

### What would make them leave

- Hypothesis: a broken or janky first frame on an unchecked browser or device, which no real-device test has yet ruled out (web-to-be-tested.md)
- Hypothesis: on a phone, being shown a recording or asked for a keyboard instead of a game (web.md §7.4)
- Hypothesis: a blank canvas after a context loss that never comes back, where the page's message is the only explanation (web.md §7.3)

### Evidence

| Claim | Source |
|---|---|
| The port's goal is a sample running in a browser tab, embedded as a playable demo on a static Astro site served by Cloudflare | docs/port/web.md, introduction before §1 |
| The host is mattharding.dev at /demo/linesweeper/, and the deploy is the owner's and not done | docs/port/web.md §7 and the M5 row in §9 Milestones; docs/port/web-to-be-tested.md 'Needs the deploy (M5, the owner's)' |
| Labrador's own site does not need browser-playable demos and links to the owner's site instead | docs/website-plan.md:90 |
| 'Most visitors open the host site on a phone' is stated without data | docs/port/web.md §7.4 and D5 |
| Phones stay gated on the host page until a real iPhone and a real Android phone have played the game | docs/port/web.md §7.4; samples/linesweeper/web/index.html header |
| A visitor fetches 151 KB under Brotli after the click | docs/port/web.md §7.1 |
| web.md §1 still describes phones as not load-bearing for a desktop v1, which is in tension with D5 and §7.4 | docs/port/web.md §1 'Not checked, and not load-bearing' |

### Assumptions to validate

- That players are an audience Labrador serves rather than a demo channel for the owner's site. PHILOSOPHY names 'the player' only as the arbiter of T3, and no design document names a browser or phone audience
- That most visitors to the host site are on phones. web.md §7.4 and D5 assert this without citing any data
- That the demo brings in engine adopters. Its purpose relative to the engine is not stated, and Labrador's site shows footage instead (website-plan.md:44, :90)
- That a phone demo is evidence for the constrained-hardware positioning. PHILOSOPHY's 'low tier' is a desktop Radeon iGPU, not a phone, and R6 says the phone frame is unmeasured
- That players want score persistence, audio in LineSweeper, or visible pause hints. Nothing in the repository records any player asking for these
- That smooth pacing is what they would notice first (web.md R5 is a risk, not a measured complaint)

<a id="sample-reading-learner"></a>
## The Sample-Reading Learner

**Tier:** tertiary · **id:** `sample-reading-learner`

Someone who reads LineSweeper, the components, the guides and the published headers to understand how a 2D game is structured in plain C++, not to ship one on Labrador. The repository deliberately serves the reader of samples: LineSweeper is defined as the sample meant to be read, and its README records every decision with its cost. The C++ beginner, whom T10 makes the yardstick for the game-facing API, has no on-ramp in the repository. The 'Introduction to C++' series is a stated target with no artefact, Get Started assumes some C++, and the published headers carry backend rationale that PHILOSOPHY's 'stranger starts from the headers' intent does not anticipate.

**Context.** Meets the material on the website: Examples ('LineSweeper: the one you read', website/src/content/docs/examples.mdx:50-75), Concepts, Guides and the generated API reference (website/src/content/docs/docs/reference/index.mdx). The site describes LineSweeper as about 4,000 lines plus about 2,000 of tests (examples.mdx:53-55). The current tree is somewhat larger. Concepts and Guides assume the minimal sample has been built (concepts/index.mdx:10-12), and building means the Windows toolchain (get-started/prerequisites.mdx). Team size, background and platform are unknown.

**Skills.** Not evidenced. The repository states two different bars. T10 says the game-facing API must 'stay teachable to a beginner' (docs/design/PHILOSOPHY.md:178-180). Get Started asks for 'Some C++, and how to use a command prompt' and 'does not teach the language' (website/src/content/docs/docs/get-started/index.mdx:18-21).

### Goals

- Understand why a finished game is shaped as it is: three layers, ticks, a counter RNG, a particle field (samples/linesweeper/README.md '## The decisions'; examples.mdx:68-75)
- Understand the engine's model and the reasons behind it (website/src/content/docs/docs/concepts/index.mdx:8-45; website/src/content/docs/docs/design/index.mdx)
- Read any part of the engine underneath a game (website/src/content/docs/why.mdx:20)
- Learn from the headers alone, as PHILOSOPHY intends (docs/design/PHILOSOPHY.md:714-716)
- Hypothesis: learn C++ itself through the engine's API, which is T10's stated target (PHILOSOPHY.md:734-738)

### Frustrations

- Published caller-facing headers carry backend internals: D3D12 descriptor-heap slots and Vulkan/D3D12 resource lifetimes in render_resources.h, and load-bearing member declaration order in application.h. Header comments are published word for word (engine/render/render_resources.h:64-118; engine/app/application.h:208-305; website/src/content/docs/docs/reference/index.mdx:53-60). This sits in tension with PHILOSOPHY.md:714-716. That a learner finds it frustrating is a hypothesis.
- The reference lists porter-facing headers next to game-facing ones: throw_if_failed.h, sprite_vertex.h, block_decode.h and resource_factory.h (website/src/lib/published.ts:166-173). resource_factory.h itself says 'a client never sees this' (engine/render/resource_factory.h:64).
- The code contradicts a stated naming rule a learner may be copying: CONVENTIONS forbids SCREAMING constants (docs/design/CONVENTIONS.md:79-81, 204), yet the math library uses Vector2F::ZERO (engine/math/circle.h:32; engine/math/quad.h:58)
- The beginner on-ramp T10 names has no artefact. The 'Introduction to C++' series appears only at PHILOSOPHY.md:734 and in a historical review, and website-plan.md:59 says the docs tree 'is not a finished beginner manual'

### What Labrador offers them

- A whole game meant to be read, whose README records each decision with its reasons and its still-open questions (PHILOSOPHY.md:720-727; samples/linesweeper/README.md)
- Components small enough to read in one sitting, with a gallery that runs them (PHILOSOPHY.md:728-733)
- Concepts pages and task guides that quote the samples' real code (concepts/index.mdx; guides/index.mdx:11-13)
- An Unreal-to-Labrador translation table (docs/design/ARCHITECTURE.md:552-562, '## A game project')
- A stated commitment to ordinary C++ with no dialect (PHILOSOPHY.md T10 170-190, T12 243-250)
- Loud failures that name the file (PHILOSOPHY.md T6 109-122)

### What would make them leave

- Hypothesis: having to read backend internals to understand a caller-facing header
- Hypothesis: having to install the Windows toolchain before they can follow Concepts, which assume a built minimal sample (concepts/index.mdx:10-12; prerequisites.mdx)
- Hypothesis: for a true beginner, finding no material that teaches the language (get-started/index.mdx:18-21)

### Evidence

| Claim | Source |
|---|---|
| LineSweeper is the sample meant to be read; minimal is the one copied | docs/design/PHILOSOPHY.md:720-727; website/src/content/docs/examples.mdx:14-17, 50-55; CLAUDE.md (opening section) |
| A stranger is meant to start from the headers | docs/design/PHILOSOPHY.md:714-716 |
| The game-facing API must stay teachable to a beginner, and a C++ series is its stated acceptance test | docs/design/PHILOSOPHY.md:178-180, 734-738 |
| Get Started assumes some C++ and does not teach the language | website/src/content/docs/docs/get-started/index.mdx:18-21 |
| The docs tree is not a finished beginner manual | docs/website-plan.md:59 |
| Header comments are published word for word as contract and rationale | website/src/content/docs/docs/reference/index.mdx:53-60; CLAUDE.md 'The website' |
| Porter-facing headers are in the published list | website/src/lib/published.ts:166-173; engine/render/resource_factory.h:60-66 |
| The Unreal translation table | docs/design/ARCHITECTURE.md:552-562 |
| The site warns that the design documents describe the target, not current status | website/src/content/docs/docs/design/index.mdx:22-27 |

### Assumptions to validate

- That people read the samples to learn, rather than only as adopters evaluating the engine. The repository records no such reader, and without that distinction this persona overlaps the Outside C++ Adopter.
- That the 'Introduction to C++' series is still intended. No plan or artefact exists in the repository.
- That the beginner is a real near-term audience rather than a design yardstick. PHILOSOPHY frames the beginner as the API's test.
- That the Unreal-arriving reader is a real audience. The table sits in the project-setup section of ARCHITECTURE and may serve adopters rather than learners.
- That two namespaces (by rule, CONVENTIONS.md:92-93), wide-only text (a stated CONSTRAINT, engine/render/renderer.h:173-179) and an eight-parameter draw_sprite (renderer.h:164-171) are learner friction rather than acceptable T10 cost.
- That readers take target-tense design documents, or their amendment history (PHILOSOPHY.md:327-338), as current status despite the site's disclaimer.
- That beginners and readers from Unreal share enough needs to be one persona. If the series is pursued, the beginner may need a persona of their own.

<a id="anti-full-environment-seeker"></a>
## The Full-Environment Seeker (anti-persona)

**Tier:** anti · **id:** `anti-full-environment-seeker`

A team or individual who wants a complete environment: a visual editor, a scripting or visual-scripting layer, hot reload and reflection-driven property editing, or 3D or online play. They may also want a large community with an asset store. The site already tells them Labrador does not suit them and names Godot and Unreal as the better choice today. The exclusions are not all of one kind. Online play is permanent. 3D is a design constant. Scripting is declined by T10, and reflection and hot reload by T12. The editor is a non-goal 'for now' in PHILOSOPHY but permanent in CLAUDE.md. Community and an asset store are simply absent. Keeping this persona explicit stops backlog items, especially content tooling, from creeping toward an environment.

**Context.** Developers who expect an editor-centred workflow of the kind why.mdx describes for Godot and Unreal: 'an editor, an asset pipeline, an object model built around the editor, scripting ... and export to many platforms' (website/src/content/docs/why.mdx:137-142). Team size, background and prior tools are not evidenced.

**Skills.** Not evidenced. They are assumed to rely on visual or non-C++ authoring for at least some contributors (see assumptions).

### Goals

- Lay out levels visually and let designers change behaviour without compiling (why.mdx:140-142)
- Iterate with hot reload and reflection-driven property editors (PHILOSOPHY.md:261-264, 775-777)
- Build online-multiplayer or 3D games (PHILOSOPHY.md:785-790)

### Frustrations

- No scripting or visual scripting. Contributors who don't know C++ 'must learn it', and behaviour changes cost a compile (PHILOSOPHY.md:171-186, T10)
- No reflection, property editors or hot reload (PHILOSOPHY.md:261-264 T12; 775-777)
- No editor: 'data files plus a text editor are the toolchain for now' (PHILOSOPHY.md:791-792; index.mdx 'What Labrador is not')
- No online play, permanently, and no 3D (PHILOSOPHY.md:785-790; index.mdx 'What Labrador is not')
- A far smaller community and no asset store (why.mdx:24-26, 171)

### What Labrador offers them

- An early, honest 'does not suit you yet' (why.mdx:24-26) and 'What Labrador is not' (index.mdx:131-145), with Godot and Unreal named as 'the better choice today' (why.mdx:137-142)
- Data-driven tuning through JSON read at startup, so a change costs a restart rather than a rebuild (PHILOSOPHY.md:124-127 T7, 183-186; why.mdx:47; engine/assets/json.h). This is not the editing environment they expect.

### What would make them leave

- Reading the Why page or 'What Labrador is not'. Leaving there is the intended outcome (why.mdx:24-26, 137-142; index.mdx:131-145).

### Evidence

| Claim | Source |
|---|---|
| Online play is permanent; 3D is a design constant; an editor is a non-goal 'for now' | docs/design/PHILOSOPHY.md:781-792 ('Non-goals, today') |
| CLAUDE.md lists online play, 3D, an editor and a scripting layer as permanently out of scope | CLAUDE.md 'Known-absent, on purpose' |
| No scripting layer; the programmer/designer split is rejected | docs/design/PHILOSOPHY.md:171-186 (T10) |
| No reflection, property editors or hot reload, with Unreal as the anti-example | docs/design/PHILOSOPHY.md:243-264 (T12); 775-777 |
| The site states the non-fit and names Godot and Unreal | website/src/content/docs/why.mdx:24-26, 137-151 |
| The home page lists 'Not an editor' and 'Not 3D, and not online' | website/src/content/docs/index.mdx:131-145 |
| Content tooling is framed as onboarding, 'not a call for an editor' | docs/review/gpt6/README.md:345-347 (opportunity 6) |

### Assumptions to validate

- That this group is a meaningful share of site visitors. There is no data.
- That the group often has a programmer/designer split, or members who author mainly outside C++. T10 rejects the split, but nothing records such users.
- That they come from Godot, Unreal or similar environments. The repo names Godot and Unreal only as comparisons, not as visitors' origins.
- That they value ecosystem and breadth over readability
- That content-tooling requests (scripted DDS/spritefont generation, gpt6 opportunity 6) can be served without drifting toward an editor. This depends on resolving whether the editor is a 'for now' or a permanent non-goal (PHILOSOPHY.md:791-792 vs CLAUDE.md).

<a id="anti-native-multiplatform-shipper"></a>
## The Native Multi-Platform Shipper (anti-persona, for now)

**Tier:** anti · **id:** `anti-native-multiplatform-shipper`

A developer who must ship native builds on Android, iOS, macOS or Linux now. The repo names Vulkan as the route to those platforms and positions the engine for constrained hardware. In practice the build refuses any platform other than Win32 or Emscripten. Android is scoped, and two items on its spine exist, but Android itself is unbuilt. iOS is deferred until after Android, Linux has no preset, and MoltenVK is unmeasured. Even the planned Android port stops at 'the engine runs' and excludes packaging and store work. Unlike the full-environment seeker, this exclusion is provisional. The persona exists so that statements about a route are not read as support, and so the backlog does not take on native platform shipping by accident.

**Context.** Hypothetical: a developer or team whose release targets include native mobile or non-Windows desktop builds. The only non-Windows reach the repo offers is the browser build through Emscripten and WebGL2.

**Skills.** Not evidenced; see assumptions.

### Goals

- One codebase producing native builds on phones and on desktops other than Windows (hypothesis; the repo states the intent to reach Android in docs/port/android.md:8-9, not this person's goal)
- Evidence that the engine performs on the constrained or mobile hardware they ship to (hypothesis; the repo's own gap is docs/port/web.md:1605-1608 R6)

### Frustrations

- The engine refuses to configure for any platform other than Win32 or Emscripten: 'There is no window for this platform' (engine/CMakeLists.txt:103-107)
- No preset exists for Android, Linux or macOS. CMakePresets.json defines only x64-* and wasm-* configurations
- Android is scoped but not built. The scoping header says 'Nothing here has been built' (docs/port/android.md:3-5). The audio seam (§3.2, BUILT 2026-08-27) and the Vulkan backend (§3.5) have since landed, but no Android shell, preset or content path exists
- iOS is deferred 'in this pass' until after Android (docs/port/android.md §6, lines 626-628). Linux appears only as a reach of Vulkan (android.md §5:564)
- MoltenVK coverage is the one unmeasured claim the Apple route rests on (docs/port/android.md §5:561-568; CLAUDE.md 'Known-absent, on purpose')
- Even the Android port excludes touch-first UI, thermal budget, APK packaging and store compliance, and ends at 'the engine runs' (docs/port/android.md §6:633-635)
- Native mobile content (ETC2 in a KTX2 container) is planned but not built (docs/port/content-probe.md §6)
- The constrained hardware the engine positions itself for has not been measured, and no p99 exists (docs/port/web.md:1605-1608 R6; docs/design/PHILOSOPHY.md:695-699; docs/review/backend-equivalence-2/STATUS.md:528 open box)

### What Labrador offers them

- The browser build as the only non-Windows route today, with all six samples running in a browser (CLAUDE.md wasm paragraph; website/src/content/docs/why.mdx:193-195)
- A partial phone path through the browser: touch input, a portrait layout and CPU decode of block-compressed textures (CLAUDE.md; docs/port/web.md D14:821). It is held back on the host page until real phones have played (web.md §7.4:738-758)
- Honest statements that support means Windows and that Vulkan is 'the route to them, not support for them' (docs/website-plan.md:36; website/src/content/docs/why.mdx:195-198)

### What would make them leave

- Hypothesis: reading statements about platform reach as present-tense support, then finding no build or preset for their platform

### Evidence

| Claim | Source |
|---|---|
| Configure fails for any platform other than Win32 or Emscripten | engine/CMakeLists.txt:103-107 |
| Only x64 and wasm presets exist | CMakePresets.json (preset names) |
| Android is decided as intent and scoped, not built | docs/port/android.md:3-9 |
| Part of the Android spine has landed since the scoping | docs/port/android.md §3.2 heading (line 182, 'BUILT, 2026-08-27'); §3.5 |
| iOS is deferred until after Android, and the port excludes packaging and store work | docs/port/android.md §6 (lines 622-645) |
| MoltenVK is the unmeasured claim | docs/port/android.md §5:561-568; CLAUDE.md 'Known-absent, on purpose' |
| The constrained hardware the engine positions for is unmeasured | docs/port/web.md:1605-1608 (R6); docs/design/PHILOSOPHY.md:677-699 |
| Public support is stated as Windows, with Vulkan as a route only | docs/website-plan.md:36; website/src/content/docs/why.mdx:193-198 |
| Phones are a planned next phase, and the host page shows them a poster for now | docs/port/web.md:782 (D5); §7.4:738-758 |

### Assumptions to validate

- That developers with native multi-platform requirements evaluate Labrador at all. No such person appears in the repo
- Their skills (competent C++, platform packaging) and their team shape are not evidenced
- That the persona moves from anti toward secondary once Android is built and a low-tier or phone p99 is taken. android.md calls mobile 'decided' but gives no schedule, and §6 suggests a shipper would still lack packaging and store work
- That developers who need constrained hardware are drawn by the positioning at all. On the public site it appears only in why.mdx:186-189, and a grep of website/src/content/docs finds no other mention

## Guardrails: what the backlog will not take on

The anti-personas exist to keep these out. Each is argued in PHILOSOPHY's vocabulary; *permanent* ones are design constants, *for-now* ones carry the condition that would reopen them.

| Won't | Why | Permanence | Source |
|---|---|---|---|
| No visual editor or GUI content tool, in the engine, the samples or the website. This covers a level editor, a sprite or atlas GUI, a property inspector and an asset browser. Content-tooling gaps (G12 texconv/MakeSpriteFont, G107 font replacement, G130 WAV export rules, G146 building a non-ASCII atlas) close only as documented command lines or small scripts that run in a terminal. | PHILOSOPHY.md:791-792 makes the editor a non-goal and CLAUDE.md:601 calls it permanently out of scope. T7 is the substitute: content is data edited in a text editor. docs/review/gpt6/README.md:347 already frames content tooling as 'an onboarding improvement, not a call for an editor'. The Full-Environment Seeker's first goal is visual layout (why.mdx:139-142), so an editor-shaped tool would be built for the anti-persona at the primary persona's expense, with no client demand (T1). ColourWars' private level editor is not a reason to promote one. | permanent | docs/design/PHILOSOPHY.md:791-792; CLAUDE.md:601; docs/review/gpt6/README.md:345-347; register G12, G99; added G130, G146 |
| No scripting or visual-scripting language, no bindings for other languages, and no behaviour DSL inside JSON. That holds even where a JSON-driven feature, such as a settings screen (G128), a join screen (G123) or a localisation table (G146), would make one tempting. | T10: everyone who writes behaviour writes C++, and when the API is too hard the fix is to simplify it, never to add a language (PHILOSOPHY.md:171-192). T7's 'Not a licence for' rules out logic smuggled into data (PHILOSOPHY.md:132-135). why.mdx:170-172 states 'C++ only, with no bindings'. | permanent | docs/design/PHILOSOPHY.md:171-192 (T10), :124-135 (T7); CLAUDE.md:601; website/src/content/docs/why.mdx:170-172 |
| No reflection, macro annotation, code generator, property editor, automatic serialisation or hot-reload machinery. G108 (persisting settings and saves) closes as a documented pattern over engine/assets/json.h written by hand in game code, not as a serialisation layer. | T12 names all four things reflection buys (automatic serialisation, property editors, network replication, hot reload) and says the engine declines them (PHILOSOPHY.md:243-264). PHILOSOPHY.md:775-777 says there is no hot-reload machinery and that the loop is edit, build, run. The seeker's second goal (reflection-driven iteration) is exactly what T12 prices out. | permanent | docs/design/PHILOSOPHY.md:243-264 (T12), :775-777; register G108 |
| No 3D: no depth-sorted 3D pipeline, camera or math. G138 (layer_depth is ignored) closes by documenting that call order decides. It is not a step toward a depth buffer or a 3D scene. | PHILOSOPHY.md:788-789: MattMath is a 2D library and 'the dimension is a design constant, not a parameter'. index.mdx:99 tells evaluators 3D is out 'for good'. Adding a depth sort would be a separate T1 decision, and G138's own scope check declines one. | permanent | docs/design/PHILOSOPHY.md:788-789; website/src/content/docs/index.mdx:99, 143; added G138 |
| No online play: no netcode, replication, rollback determinism, network lobby, matchmaking or remote join. The couch-play gaps (G113 three- and four-player layouts, G123 join screen, G122 any-slot pad) stay local, on one machine. | PHILOSOPHY.md:783-786: 'Online play. Permanent, not provisional... no design tax is paid for netcode, replication, or rollback determinism, ever.' Being excellent at couch play is the stated niche, so a join screen must not grow a network half. | permanent | docs/design/PHILOSOPHY.md:783-786; CLAUDE.md:601; register G113; added G122, G123 |
| No action-mapping or rebinding layer in the engine, and that includes one arriving through accessibility (G119), the join screen (G123), any-slot pad handling (G122) or a settings component (G128). Bindings stay in game code. G119's accessibility statement should say that remappable controls are the game's job. | CLAUDE.md:581-585 lists action mapping as known-absent on purpose: 'neither client has a rebinding screen, so a binding table would be the speculative framework T1 rules out'. The full-environment expectation of an input-action editor meets T1 and capacity at the same point. | for-now | CLAUDE.md:581-585 ('Known-absent, on purpose'); T1 PHILOSOPHY.md:35-47; added G119, G122, G123, G128 |
| No engine asset pipeline, import step, streaming loader or string-table facility built to match the 'asset pipeline' that complete environments offer. G131 (music-sized audio) closes by documenting the in-memory cost and the --preload-file switch. G146 closes with JSON, widen() and can_render at load. | why.mdx:139 names 'an asset pipeline' as a feature of Godot and Unreal that Labrador is not. web.md:487-489 already declines an asynchronous loader under T3 until something needs it. G12's scope check says an engine asset pipeline would need its own T1 argument. T1 forbids building one on speculation, and the owner's capacity is the binding constraint. | for-now | website/src/content/docs/why.mdx:137-142; docs/port/web.md:485-489; register G12; added G131, G146 |
| No ECS or editor-centred object model added to make Labrador resemble a complete environment. | PHILOSOPHY.md:793-794 lists framework maximalism as a non-goal: 'Composition where it pays, not an ECS for its own sake' (T4). The object model a complete environment builds 'around the editor' (why.mdx:139-140) has no editor here to serve. | for-now | docs/design/PHILOSOPHY.md:793-794 (T4); website/src/content/docs/why.mdx:139-140 |
| No community or ecosystem infrastructure (an asset store, plugin registry, forum or marketing push) built to win over full-environment seekers. A public route for questions (G21) and a security route (G148) serve the primary persona and are not this. | why.mdx:24-26 names a large community and an asset store as things Labrador does not offer, and the right response is to say so rather than to build them. With one maintainer working largely through agents, ecosystem building would starve the primary persona's E01-E08 work. T1: nothing a client needs depends on it. | for-now | website/src/content/docs/why.mdx:24-26, 170-172; register G21; added G148 |
| No analytics, tracking or third-party measurement added to find out whether anti-personas arrive or leave. G101 closes by accepting the gap and recording it. | The site is deliberately static, with no server-side features required (docs/website-plan.md:90). G101's own scope check says analytics would be a new third-party feature the plan does not require, and that accepting the gap is a valid closure. Under the capacity constraint, the turn-away is judged by whether the pages are true, not by counting visitors. | for-now | docs/website-plan.md:90; register G101 |
| Exclusions must not be softened to widen appeal. No 'coming soon', 'yet' or roadmap hint goes on a permanent exclusion, and no page implies an editor, scripting, 3D or online play may arrive. | The seeker's success condition is leaving without having been told anything that later turns out false (journey anti-recognise-not-for-me). why.mdx:24 currently attaches 'yet' to permanent items (G99). T6's honesty applies to the public face as much as to the code: a vague promise is a quiet failure for a reader deciding fit. | permanent | website/src/content/docs/why.mdx:24-26; docs/design/PHILOSOPHY.md:781-792; register G99 |
| No Android, iOS, macOS or Linux shell, preset or port work in this backlog. That includes the remaining Android spine (android.md §4: 3.6 build and core, 3.1a KTX2 and ETC2, 3.3 input, 3.4 shell). It waits for the promotion condition below. | android.md:3-5 says 'Nothing here has been built' and gives no schedule. finishing its right-hand branch needs hardware (android.md §4). No client targets a native non-Windows platform: the only shipped client packages d3d11 and xaudio2 (G150 evidence, ColourWars cmake/release_package.cmake:14-16). T1 forbids generalising before a real client demands it (PHILOSOPHY.md:46-48, 795-799). The owner's capacity is spoken for by the primary persona's Windows and browser path. | for-now | docs/port/android.md:3-12, 501-513; engine/CMakeLists.txt:103-107; docs/design/PHILOSOPHY.md:46-48, 795-799; added G150 |
| G104's fix stays an early, named refusal. It must not grow into a Linux or macOS build, a partial non-Windows desktop window, or CI on a non-Windows build host, beyond checking that the refusal fires. | G104's own scope check: 'T5 favours an early, named refusal. It must not grow into a Linux build, which is unplanned.' docs/port/ holds no Linux or macOS plan (directory: android.md, content-probe.md, web.md, web-to-be-tested.md). | for-now | register G104; docs/port/ listing; T5 PHILOSOPHY.md:95-107 |
| Nothing on the public face states or implies present native support for Android, iOS, macOS or Linux. That rules out 'runs nearly everywhere', unqualified 'reaches Android' sentences, and phone support before the phone gate lifts. Vulkan is always described as 'the route, not support'. | website-plan.md:36 says 'Vulkan does not establish support for other operating systems', and why.mdx:197-198 says 'Treat Vulkan as the route to them, not as support for them'. G02 and G102 show the line is already leaking (why.mdx:170-172 'nearly everywhere'; ARCHITECTURE.md:465-467). The anti-persona exists so that route statements are not read as support (personas.json). Quiet over-claiming breaks T6. | for-now | docs/website-plan.md:36; website/src/content/docs/why.mdx:170-172, 191-198; docs/design/ARCHITECTURE.md:463-470; register G02, G102 |
| No publishing of phone or low-tier performance figures, and no 'runs well on constrained hardware' claim, until a recorded p99 exists on the named hardware. | PHILOSOPHY.md:695-699: 'Until that exists no number here is a floor.' web.md:1605-1608 (R6) says the low tier is not measured and the bench's numbers do not transfer. why.mdx:185-189 already says no figures are published. The shipper drawn by the positioning must not be served a figure from the wrong machine (G24, G27, G34). | for-now | docs/design/PHILOSOPHY.md:677-699; docs/port/web.md:1603-1608; website/src/content/docs/why.mdx:185-189; register G24, G27, G34 |
| No phone-first or packaging work for native mobile: touch-first UI, thermal budget, input latency, APK or IPA packaging, or store compliance. Equally, no GLES backend or sixth backend as a hedge to reach a platform. | android.md §6 places all of these out of scope on purpose and ends the port at 'the engine runs' (android.md:622-645). A GLES hedge is 'the cost Vulkan exists to avoid'. These are what a shipper would most need, which is why they must not enter the backlog by accident. | for-now | docs/port/android.md:622-645 |
| No runtime backend switching or one-binary-everywhere layer offered as an answer to platform reach or driver refusal. G150 closes as a shipping note, for example two executables, d3d11 plus gl. | T5 and T8: the backend is chosen at configure time, and asking for one that was not built is a link error (CLAUDE.md, backends paragraph). Renderer is concrete, not an abstract base, because T8 does not permit a virtual call per sprite. G150's own scope check rules runtime switching out. | permanent | docs/design/PHILOSOPHY.md:95-107 (T5), :137-151 (T8); CLAUDE.md 'There are five render backends'; added G150 |
| No invented schedule or date for any native platform on the site, the README or the port documents. | G103's scope check: 'No conflict, as long as no schedule is invented.' android.md calls mobile 'decided' but schedules nothing (android.md:8-9). A date that capacity cannot keep is a later false statement to the very persona the turn-away serves. | permanent | register G103; docs/port/android.md:8-12 |

### When the Native Multi-Platform Shipper would be served

The Native Multi-Platform Shipper moves from anti-persona to a served persona, at secondary tier and never above the Outside C++ Adopter, only for a named native platform P (Android first, per docs/port/android.md §6), and only when all six of the following hold, each recorded in the tree.

1. **Demand (T1).** A real client game, either ColourWars or a named outside adopter's project, commits to shipping on P. The commitment is recorded as a client report or exhibit. Today the only shipped client packages d3d11 and xaudio2 alone (ColourWars cmake/release_package.cmake:14-16), and T1 forbids generalising before a client demands it (PHILOSOPHY.md:46-48, 795-799).
2. **Build.** engine/CMakeLists.txt:103-107 accepts P, CMakePresets.json has a preset for P, and CI builds that preset and runs its headless ctest entries on every pull request, as the -null and wasm presets do today.
3. **Device.** The Android spine (android.md §4: 3.6 build and core, 3.1a KTX2 and ETC2, 3.3 input, 3.4 shell) is built, and LineSweeper or minimal has run on a physical P device, with a dated record in the tree. For an Apple platform, the MoltenVK claim (android.md:561-568) must also be measured.
4. **Performance.** There is a recorded p99 frame time on the named low tier (PHILOSOPHY.md:677-699; G24, blocked today by G01) and on a real device of P for the R6 top-out (docs/port/web.md:1605-1608).
5. **Shipping.** A written packaging path for P exists (APK or IPA, signing, notices). android.md:638-640 currently ends the port at 'the engine runs', without packaging or store work. Without this item the persona can be promoted only as a 'native builder', not as a shipper.
6. **Public statement.** why.mdx 'Platform support' and docs/website-plan.md:36 are amended in the same commit to list P as supported, citing items 2 to 4.

Until all six hold, Vulkan stays 'the route, not support' everywhere. Partial progress, such as an Android preset without a device run, or a phone p99 without client demand, leaves the persona an anti-persona. Opening a platform track at all is the owner's capacity decision and should be recorded in android.md before any of this work enters the backlog.

The browser's phone gate lifting (G34) does not count toward promotion: it serves the Browser Demo Player, not native shipping.
