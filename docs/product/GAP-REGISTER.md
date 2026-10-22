# Gap register

Every closable difference between what a persona needs and what the repository provides. Taken on 2026-10-13 against `master` at `0977112`. 149 gaps: 34 major, 115 minor.

Severity: **blocker**, a persona cannot complete a journey; **major**, they complete it only with significant unwritten knowledge or risk; **minor**, an annoyance. Ids are stable: G59 and G87 were refuted by the adversarial pass and are kept out rather than renumbered. "Stories" says which entries of [STORIES.md](STORIES.md) close each gap; G01–G115 came from the journeys and the walkthrough, G116–G151 from a second sweep of the areas no journey walked.

## By epic

| Epic | Outcome | Gaps |
|---|---|---|
| **E01** Know what Labrador is, and whether it fits, before installing | As an evaluator I can tell from the front door what Labrador does, where it runs, what it will never do, under what licence, and can try it, without being misled by stale or contradictory claims. | 17 |
| **E02** From an empty Windows machine to my own project, built | As an adopter I can follow Get Started from a clean machine to my own copied project building and changing, with every step, prerequisite and template comment true and nothing I must reverse-engineer. | 15 |
| **E03** Prove my own game works: tests, smoke checks and CI | As an adopter I have a test target, a headless rules test, a start-up smoke check and a CI recipe for my own repository, shaped the way the engine tests itself. | 6 |
| **E04** Content, audio and text that work on both platforms | As an adopter I can obtain the content tools, export sprites, fonts and sounds in formats the engine accepts, write text in my players' language, and get the same result on Windows and in the browser. | 9 |
| **E05** Input, menus, settings and couch play my players can use | As an adopter building a couch game I can see which controllers work, accept a pad in any slot, build a join screen and a settings screen wired to the engine, use menus by pointer as well as pad, and trust the defaults to look right. | 13 |
| **E06** Concepts and reference that tell the truth a caller needs | As an adopter or learner, each concept and reference page says what the code actually does, how I get the type, what its parameters mean, who the page is for, and where it is taught. | 15 |
| **E07** Ship a Windows release to players | As an adopter or integrator I know which backend to ship, what the player's machine needs, which files and notices go in the package, and that it runs on a clean machine at normal display scaling. | 8 |
| **E08** Ship my game to a web page | As an adopter I can add a browser build to my own project, host it with the right headers and page contract, hear its audio, and diagnose what goes wrong, from public pages alone. | 11 |
| **E09** The hosted demo is playable, understandable and kind to every player | As a visitor I can find, load and play LineSweeper on desktop or phone, read its controls and its failures, see its landing aid, and not be harmed by its effects; as maintainer I know the hosted page is tested. | 14 |
| **E10** Releases, versions, upgrades and an answered report | As an integrator or adopter I can pin a named version, read what changed and what broke, move my pin by a written procedure, and report a gap or a vulnerability through a route that gets an answer. | 11 |
| **E11** Performance and hardware claims backed by a measurement | As the maintainer, every low-tier, pacing, phone and browser claim the project makes has a named instrument, a recorded run and a stated p99 on the hardware it names. | 12 |
| **E12** A maintainer's process, CI and deploy that one person and their agents can trust | As the sole maintainer, the merge procedure, required checks, pinned inputs, site deploy, captures and review ledgers are written in the tree and checked by CI, so a change or an external drift cannot land or ship unseen. | 18 |

## E01. Know what Labrador is, and whether it fits, before installing

As an evaluator I can tell from the front door what Labrador does, where it runs, what it will never do, under what licence, and can try it, without being misled by stale or contradictory claims.

| Gap | Severity | Kind | Personas | Stories |
|---|---|---|---|---|
| [G02](#g02) Platform reach is stated several incompatible ways on the public face | major | doc-drift | The Outside C++ Adopter, The Full-Environment Seeker (anti-persona), The Native Multi-Platform Shipper (anti-persona, for now), The Sample-Reading Learner | OCA-01 |
| [G03](#g03) The Why page and PHILOSOPHY deny the split-screen sample that ships | major | doc-drift | The Outside C++ Adopter, The Full-Environment Seeker (anti-persona), The Sole Owner-Maintainer | OCA-02, OCA-05 |
| [G04](#g04) The live LineSweeper demo is not linked from the site or the README | major | onboarding | The Outside C++ Adopter, The Browser and Phone Demo Player, The Sample-Reading Learner | OCA-03 |
| [G22](#g22) The 'Introduction to C++' series T10 relies on has no artefact, plan or status | minor | missing-capability | The Sample-Reading Learner, The Outside C++ Adopter | SRL-14 |
| [G44](#g44) Design documents carry statements false even as target | minor | doc-drift | The Outside C++ Adopter, The Sample-Reading Learner, The Sole Owner-Maintainer | OCA-05 |
| [G45](#g45) Only LineSweeper is hosted, so split-screen and the other samples cannot be tried without a Windows build | minor | missing-capability | The Outside C++ Adopter, The Sample-Reading Learner | OCA-04 |
| [G55](#g55) The GitHub front door points at the private client, not the site | minor | onboarding | The Outside C++ Adopter | OCA-06 |
| [G57](#g57) The engine site has no site URL, so canonical URLs and the sitemap are unset | minor | onboarding | The Sole Owner-Maintainer, The Outside C++ Adopter | SM-31 |
| [G96](#g96) T10's 'teachable to a beginner' and Get Started's 'does not teach the language' are published unreconciled | minor | doc-drift | The Sample-Reading Learner | SRL-15 |
| [G98](#g98) T10's beginner-teachability claim has never been tested and has no recorded method | minor | verification-debt | The Sample-Reading Learner | SRL-17 |
| [G99](#g99) Which exclusions are permanent is stated inconsistently | minor | doc-drift | The Full-Environment Seeker (anti-persona) | FES-03, FES-04 |
| [G100](#g100) Neither the GitHub README nor the Get Started path states what Labrador is not | minor | onboarding | The Full-Environment Seeker (anti-persona) | OCA-06, FES-02, FES-04 |
| [G101](#g101) No evidence whether the anti-personas arrive or leave before installing | minor | verification-debt | The Full-Environment Seeker (anti-persona) | — |
| [G102](#g102) Published ARCHITECTURE states Vulkan's platform reach with no 'not built' qualifier | minor | doc-drift | The Native Multi-Platform Shipper (anti-persona, for now) | NMS-02 |
| [G103](#g103) The site gives no pointer to the platform plans or their scope | minor | doc-drift | The Native Multi-Platform Shipper (anti-persona, for now) | NMS-03 |
| [G119](#g119) No stated accessibility position for the demo, the samples or what the engine offers a game | minor | doc-drift | The Outside C++ Adopter, The Browser and Phone Demo Player, The Sole Owner-Maintainer | OCA-08, BDP-24 |
| [G149](#g149) LICENSE carries a stale addendum that breaks licence detection and names code not in the tree | minor | doc-drift | The Outside C++ Adopter, The Pinned Submodule Integrator | OCA-07 |

## E02. From an empty Windows machine to my own project, built

As an adopter I can follow Get Started from a clean machine to my own copied project building and changing, with every step, prerequisite and template comment true and nothing I must reverse-engineer.

| Gap | Severity | Kind | Personas | Stories |
|---|---|---|---|---|
| [G06](#g06) The core-longpaths alternative fixes the clone but not the build | major | doc-drift | The Outside C++ Adopter | OCA-09 |
| [G08](#g08) No CI job builds a consumer-shaped project, and the walkthrough record predates the current engine | major | verification-debt | The Outside C++ Adopter, The Pinned Submodule Integrator | OCA-11 |
| [G07](#g07) README's consumer section omits the manifest, the toolchain preset and fxc | minor | onboarding | The Outside C++ Adopter, The Pinned Submodule Integrator | OCA-10 |
| [G38](#g38) No engine-side instruction for running the engine's own suite at a consumer's pin | minor | onboarding | The Outside C++ Adopter, The Pinned Submodule Integrator | OCA-71 |
| [G40](#g40) The template's comments point at private ColourWars code | minor | doc-drift | The Outside C++ Adopter | OCA-13 |
| [G41](#g41) The 'dozen lines' build-file claim is wrong in size and silent on shipping | minor | doc-drift | The Outside C++ Adopter | OCA-14 |
| [G42](#g42) The template's CMakeLists says content resolves against the working directory | minor | doc-drift | The Outside C++ Adopter | OCA-13 |
| [G43](#g43) The template has no README, so it has neither copy instructions nor a change signal | minor | onboarding | The Outside C++ Adopter | OCA-15 |
| [G56](#g56) First-change's 'Where to go next' does not link the guides | minor | onboarding | The Outside C++ Adopter | OCA-16 |
| [G58](#g58) The VCPKG_ROOT fallback example names only VS 2022 | minor | doc-drift | The Outside C++ Adopter | OCA-17 |
| [G60](#g60) Nothing warns that the vcpkg manifest name has stricter rules than 'use your own name' | minor | onboarding | The Outside C++ Adopter | OCA-17 |
| [G61](#g61) The first-run vcpkg cost and the public-clone path were not observed | minor | verification-debt | The Outside C++ Adopter | OCA-12 |
| [G104](#g104) Configure on an unsupported host fails late and points nowhere | minor | onboarding | The Native Multi-Platform Shipper (anti-persona, for now) | NMS-04 |
| [G106](#g106) README says samples/minimal is a working example of the consumer setup, but minimal is not a consumer | minor | doc-drift | The Outside C++ Adopter, The Pinned Submodule Integrator | OCA-10 |
| [G109](#g109) No IDE or debugger workflow for the C++ game the site says your debugger works on | minor | onboarding | The Outside C++ Adopter, The Sample-Reading Learner | OCA-18 |

## E03. Prove my own game works: tests, smoke checks and CI

As an adopter I have a test target, a headless rules test, a start-up smoke check and a CI recipe for my own repository, shaped the way the engine tests itself.

| Gap | Severity | Kind | Personas | Stories |
|---|---|---|---|---|
| [G10](#g10) The consumer template has no test target and no guidance for headless rules tests | major | onboarding | The Outside C++ Adopter | OCA-29 |
| [G11](#g11) An adopter has no start-up or smoke check for their own game, page or package | major | verification-debt | The Outside C++ Adopter | OCA-30, OCA-31 |
| [G48](#g48) The null renderer is advertised but not usable from a consumer project | minor | onboarding | The Outside C++ Adopter | OCA-32 |
| [G79](#g79) No guidance for driving a consumer's gameplay with injected input in a smoke run | minor | verification-debt | The Pinned Submodule Integrator | PI-16 |
| [G88](#g88) The rules tests cannot be built without configuring the whole engine with vcpkg on Windows | minor | onboarding | The Sample-Reading Learner | SRL-18 |
| [G110](#g110) No CI recipe for an adopter's own repository | minor | onboarding | The Outside C++ Adopter, The Pinned Submodule Integrator | OCA-33 |

## E04. Content, audio and text that work on both platforms

As an adopter I can obtain the content tools, export sprites, fonts and sounds in formats the engine accepts, write text in my players' language, and get the same result on Windows and in the browser.

| Gap | Severity | Kind | Personas | Stories |
|---|---|---|---|---|
| [G12](#g12) Content tools are named but not obtainable, and the premultiplied-alpha rule is unstated | major | content-pipeline | The Outside C++ Adopter | OCA-34, OCA-35 |
| [G107](#g107) The template's only font is a Courier New atlas, and nothing tells an adopter whether they may ship it or how to replace it | major | onboarding | The Outside C++ Adopter, The Sole Owner-Maintainer | OCA-37, OCA-75 |
| [G145](#g145) MSVC reads the adopter's non-ASCII L"..." literals in the system code page, so the same source draws different text on desktop and in the browser | major | platform | The Outside C++ Adopter, The Pinned Submodule Integrator | OCA-41 |
| [G146](#g146) Nothing walks an adopter from 'my game is not in English' to text on screen | major | onboarding | The Outside C++ Adopter, The Sample-Reading Learner | OCA-42 |
| [G13](#g13) Guide samples borrow content and sources from other sample folders | minor | api-ergonomics | The Outside C++ Adopter | OCA-36 |
| [G129](#g129) The guide's XWBTool command omits -nc, so a bank packed per the guide plays on Windows and fails to load in the browser build | minor | doc-drift | The Outside C++ Adopter | OCA-38 |
| [G130](#g130) No page says which WAV formats an adopter's sounds must be exported in, or what each costs in a browser | minor | content-pipeline | The Outside C++ Adopter | OCA-39 |
| [G131](#g131) No guidance for music-sized audio: banks are in-memory only, and the template embeds all content in the browser module | minor | missing-capability | The Outside C++ Adopter | OCA-40 |
| [G147](#g147) Which scripts and characters the text path can draw at all is not stated anywhere public | minor | doc-drift | The Outside C++ Adopter | OCA-43 |

## E05. Input, menus, settings and couch play my players can use

As an adopter building a couch game I can see which controllers work, accept a pad in any slot, build a join screen and a settings screen wired to the engine, use menus by pointer as well as pad, and trust the defaults to look right.

| Gap | Severity | Kind | Personas | Stories |
|---|---|---|---|---|
| [G108](#g108) No guidance on persisting player data such as settings or saves, on desktop or in the browser | major | missing-capability | The Outside C++ Adopter | OCA-53 |
| [G47](#g47) Input guide gives no hardware caveat for the browser pad, touch or the phone layout | minor | verification-debt | The Outside C++ Adopter, The Native Multi-Platform Shipper (anti-persona, for now) | OCA-54 |
| [G111](#g111) The menus guide says a game writes its own slider and option row, and does not point at the components sample that supplies them | minor | doc-drift | The Outside C++ Adopter | OCA-52 |
| [G113](#g113) Three- and four-player layouts have no public sample or guide | minor | onboarding | The Outside C++ Adopter | OCA-47 |
| [G116](#g116) FocusStyle's default 'disabled' grey is lighter than its 'unfocused' grey, the opposite of what the menus guide says | minor | api-ergonomics | The Outside C++ Adopter, The Sample-Reading Learner | OCA-49 |
| [G121](#g121) The website never says the Windows pad path is XInput, which controllers that admits, or how slots are assigned | minor | doc-drift | The Outside C++ Adopter | OCA-44 |
| [G122](#g122) Single-player code reads only pad slot 0, and nothing tells an adopter how to accept a lone pad in any slot | minor | api-ergonomics | The Outside C++ Adopter | OCA-45 |
| [G123](#g123) No sample or guide shows a join screen: hot-plug, binding a pad to a player, and per-player menu focus are prose only | minor | onboarding | The Outside C++ Adopter | OCA-46 |
| [G124](#g124) Whether XInput honours the reader's suspend is unverified, so a backgrounded game may still take pad input | minor | verification-debt | The Outside C++ Adopter, The Sole Owner-Maintainer | OCA-48 |
| [G125](#g125) The XInput translation has no headless test and the tree records no physical-pad run of a desktop sample | minor | verification-debt | The Sole Owner-Maintainer, The Outside C++ Adopter | OCA-48, SM-40 |
| [G126](#g126) The promotion rule's test, 'copied unchanged', cannot be met by these components and has no record to be measured against | minor | other | The Sole Owner-Maintainer, The Outside C++ Adopter | SM-41 |
| [G127](#g127) Components and the menus guide are directional-only: a click or tap on a row or a slider does nothing, and the one tap pattern in the tree is not shown | minor | api-ergonomics | The Outside C++ Adopter | OCA-50 |
| [G128](#g128) Nothing wires a settings component to what the engine can actually set, and the gallery's window row offers a mode the engine does not have | minor | onboarding | The Outside C++ Adopter | OCA-51 |

## E06. Concepts and reference that tell the truth a caller needs

As an adopter or learner, each concept and reference page says what the code actually does, how I get the type, what its parameters mean, who the page is for, and where it is taught.

| Gap | Severity | Kind | Personas | Stories |
|---|---|---|---|---|
| [G36](#g36) Concept pages make false claims about what the samples do | major | doc-drift | The Sample-Reading Learner | SRL-03 |
| [G138](#g138) layer_depth is published as a sort key on every drawing page, but the engine ignores it and only a renderer-writer header says so | major | doc-drift | The Outside C++ Adopter, The Sample-Reading Learner | OCA-55 |
| [G49](#g49) Collidable's no-op set_for_deletion silently defeats the bounds sweep | minor | api-ergonomics | The Outside C++ Adopter | OCA-59 |
| [G75](#g75) Engine-private rapidjson pushes clients to vendor a second copy, and no doc says so | minor | api-ergonomics | The Pinned Submodule Integrator | PI-17 |
| [G89](#g89) Examples page understates LineSweeper's size | minor | doc-drift | The Sample-Reading Learner | SRL-04 |
| [G90](#g90) LineSweeper README's 'Still open' presentation/ count is stale | minor | doc-drift | The Sample-Reading Learner | SRL-05 |
| [G91](#g91) LineSweeper README rests decisions on internal records a cold reader cannot use | minor | doc-drift | The Sample-Reading Learner | SRL-06 |
| [G92](#g92) SCREAMING constants in published headers contradict CONVENTIONS, with no stated exception | minor | doc-drift | The Sample-Reading Learner | SRL-07 |
| [G93](#g93) The API reference does not mark which headers are for game code and which for renderer writers | minor | onboarding | The Sample-Reading Learner | SRL-08 |
| [G94](#g94) Game-facing reference pages carry backend rationale with nothing marking it skippable | minor | doc-drift | The Sample-Reading Learner | SRL-09 |
| [G95](#g95) Concepts are gated on a Windows build the pages do not need | minor | onboarding | The Sample-Reading Learner | SRL-02 |
| [G97](#g97) The copy-me sample uses intermediate C++ and engine-internal vocabulary with no guide to either | minor | onboarding | The Sample-Reading Learner | SRL-16 |
| [G139](#g139) Game-facing reference pages leave out how a game obtains the type and what its constructor parameters mean | minor | doc-drift | The Outside C++ Adopter, The Sample-Reading Learner | OCA-56 |
| [G140](#g140) Reference pages never link back to the guide or concept that teaches the type | minor | api-ergonomics | The Outside C++ Adopter, The Sample-Reading Learner | OCA-57 |
| [G141](#g141) Reference generator mislabels ApplicationOptions::max_threads and renders grouped constants as one unusable heading | minor | other | The Outside C++ Adopter, The Sample-Reading Learner, The Sole Owner-Maintainer | OCA-58 |

## E07. Ship a Windows release to players

As an adopter or integrator I know which backend to ship, what the player's machine needs, which files and notices go in the package, and that it runs on a clean machine at normal display scaling.

| Gap | Severity | Kind | Personas | Stories |
|---|---|---|---|---|
| [G14](#g14) The engine has no install or package path or file list for a Windows release | major | missing-capability | The Outside C++ Adopter, The Pinned Submodule Integrator | OCA-60, PI-11 |
| [G15](#g15) Release builds depend on the dynamic MSVC runtime, and nothing says so | major | platform | The Outside C++ Adopter | OCA-61 |
| [G16](#g16) No notice or licence guidance for what a shipped game links, desktop or browser | major | onboarding | The Outside C++ Adopter, The Pinned Submodule Integrator | OCA-62, OCA-63 |
| [G150](#g150) No guidance on which render backend a shipped Windows game should build, or what to do when the player's machine refuses it | major | doc-drift | The Outside C++ Adopter, The Pinned Submodule Integrator | OCA-65 |
| [G77](#g77) The shipped Windows ZIP is unverified on a clean machine, on target GPUs and at the current engine | minor | verification-debt | The Pinned Submodule Integrator | PI-14 |
| [G78](#g78) Hidden launch through STARTUPINFO is argued but never executed by a test | minor | verification-debt | The Pinned Submodule Integrator | PI-15 |
| [G105](#g105) No stated hardware or OS floor a game built on Labrador needs on a player's machine | minor | platform | The Outside C++ Adopter, The Pinned Submodule Integrator | OCA-64 |
| [G151](#g151) Windows DPI awareness is neither declared nor decided | minor | platform | The Outside C++ Adopter, The Pinned Submodule Integrator | OCA-66 |

## E08. Ship my game to a web page

As an adopter I can add a browser build to my own project, host it with the right headers and page contract, hear its audio, and diagnose what goes wrong, from public pages alone.

| Gap | Severity | Kind | Personas | Stories |
|---|---|---|---|---|
| [G09](#g09) No path for adding a browser build to an adopter's own project | major | onboarding | The Outside C++ Adopter | OCA-20 |
| [G39](#g39) The template's browser files are copied, edited and never compiled, with no word on them | minor | onboarding | The Outside C++ Adopter | OCA-19 |
| [G46](#g46) Emscripten setup and its pinned version are not on the website | minor | onboarding | The Outside C++ Adopter, The Native Multi-Platform Shipper (anti-persona, for now), The Sample-Reading Learner | OCA-21, OCA-54 |
| [G50](#g50) The render backend default ignores the platform, unlike audio and input | minor | api-ergonomics | The Outside C++ Adopter | OCA-22 |
| [G51](#g51) Hosting requirements live only in an internal port plan | minor | doc-drift | The Outside C++ Adopter | OCA-23, OCA-74 |
| [G52](#g52) web.md §7.2 says a wrong wasm MIME type stops the demo, but the loader falls back | minor | doc-drift | The Browser and Phone Demo Player, The Outside C++ Adopter | OCA-23 |
| [G112](#g112) Troubleshooting covers only the Windows desktop path | minor | onboarding | The Outside C++ Adopter, The Sample-Reading Learner | OCA-24 |
| [G132](#g132) The guide states the gesture rule only for the sample, not its consequence for a game's own sounds | minor | doc-drift | The Outside C++ Adopter | OCA-25 |
| [G142](#g142) The page-to-module contract has no single public statement, no stability marker, and silently ignores a misspelled callback | minor | doc-drift | The Outside C++ Adopter, The Sole Owner-Maintainer | OCA-26, OCA-74 |
| [G143](#g143) A host cannot stop a running instance; removing its canvas leaves the game loop running unseen | minor | api-ergonomics | The Outside C++ Adopter, The Sole Owner-Maintainer | OCA-26 |
| [G144](#g144) The bundle's safe-deploy checks live only in the host's own repository, and a Debug bundle is written to the same place with nothing to stop it | minor | release-process | The Sole Owner-Maintainer, The Outside C++ Adopter | SM-27 |

## E09. The hosted demo is playable, understandable and kind to every player

As a visitor I can find, load and play LineSweeper on desktop or phone, read its controls and its failures, see its landing aid, and not be harmed by its effects; as maintainer I know the hosted page is tested.

| Gap | Severity | Kind | Personas | Stories |
|---|---|---|---|---|
| [G33](#g33) The page hosts actually serve (the bundle's index.html) has no automated test | major | verification-debt | The Browser and Phone Demo Player | BDP-03, BDP-04 |
| [G34](#g34) Phone play is gated pending real-device checks, and phone performance is unmeasured | major | verification-debt | The Browser and Phone Demo Player, The Sole Owner-Maintainer | OCA-54, SM-13, BDP-09, BDP-10, SM-44 |
| [G35](#g35) LineSweeper neither pauses nor says so when the canvas loses the keyboard | minor | missing-capability | The Browser and Phone Demo Player | BDP-11 |
| [G71](#g71) No documented way to put a development page on a phone | minor | onboarding | The Sole Owner-Maintainer | SM-14 |
| [G80](#g80) Controls are stated incompletely and inconsistently, and phones see keyboard instructions | minor | onboarding | The Browser and Phone Demo Player | BDP-12, BDP-13 |
| [G81](#g81) The phone gate promises a controller but only a key lifts it | minor | other | The Browser and Phone Demo Player | BDP-14 |
| [G82](#g82) Failure messages are developer-voiced and give the player nothing to do | minor | other | The Browser and Phone Demo Player | BDP-15 |
| [G83](#g83) A context that never comes back leaves a blank canvas with no way forward | minor | other | The Browser and Phone Demo Player | BDP-16 |
| [G84](#g84) No capability check before download | minor | onboarding | The Browser and Phone Demo Player | BDP-17 |
| [G85](#g85) No best score survives a match or a reload | minor | missing-capability | The Browser and Phone Demo Player | BDP-19 |
| [G86](#g86) The phone fallback's data cost is undecided | minor | onboarding | The Browser and Phone Demo Player | BDP-20 |
| [G114](#g114) Repeated 'Play again' keeps every old instance and its WebGL2 context, and it is tested only to a second instance | minor | verification-debt | The Browser and Phone Demo Player | BDP-18 |
| [G117](#g117) LineSweeper's landing shadow is 1.3 to 2.0:1 against the well | minor | other | The Browser and Phone Demo Player, The Sample-Reading Learner | BDP-21 |
| [G118](#g118) Particle bursts have no reduced-motion path and have never been checked against flash thresholds | minor | missing-capability | The Browser and Phone Demo Player, The Sample-Reading Learner | BDP-22, BDP-23 |

## E10. Releases, versions, upgrades and an answered report

As an integrator or adopter I can pin a named version, read what changed and what broke, move my pin by a written procedure, and report a gap or a vulnerability through a route that gets an answer.

| Gap | Severity | Kind | Personas | Stories |
|---|---|---|---|---|
| [G17](#g17) No release, tag or version, and no criterion for 'the API settles' | major | release-process | The Outside C++ Adopter, The Pinned Submodule Integrator, The Sole Owner-Maintainer | OCA-67 |
| [G18](#g18) No engine-side record of consumer-visible and breaking changes | major | doc-drift | The Outside C++ Adopter, The Pinned Submodule Integrator, The Sole Owner-Maintainer | OCA-68 |
| [G19](#g19) The caller-count deletion rule cannot be met for a public repository, and there is no deprecation path | major | api-ergonomics | The Outside C++ Adopter, The Sole Owner-Maintainer | OCA-69 |
| [G20](#g20) No documented procedure for moving the engine pin | major | onboarding | The Outside C++ Adopter, The Pinned Submodule Integrator | OCA-70 |
| [G21](#g21) No public route for questions or API requests | major | onboarding | The Outside C++ Adopter, The Pinned Submodule Integrator | OCA-72 |
| [G37](#g37) Site reference and generated manifest describe master, not the reader's pin | minor | doc-drift | The Outside C++ Adopter, The Pinned Submodule Integrator | OCA-70 |
| [G53](#g53) The engine has no version identity a consumer can stamp | minor | release-process | The Pinned Submodule Integrator, The Outside C++ Adopter | PI-05 |
| [G54](#g54) The engine keeps no public record of reports received, their disposition, or which commit answered them | minor | release-process | The Pinned Submodule Integrator, The Outside C++ Adopter | PI-08 |
| [G73](#g73) The client pin lags master by 45 commits during active client development | minor | verification-debt | The Sole Owner-Maintainer | SM-36 |
| [G74](#g74) Exhibit-backed client reports sit unfiled, the packaging gap among them | minor | other | The Pinned Submodule Integrator | PI-09, PI-10 |
| [G148](#g148) No security policy and no private vulnerability-reporting route | minor | missing-capability | The Outside C++ Adopter, The Pinned Submodule Integrator, The Sole Owner-Maintainer | OCA-73 |

## E11. Performance and hardware claims backed by a measurement

As the maintainer, every low-tier, pacing, phone and browser claim the project makes has a named instrument, a recorded run and a stated p99 on the hardware it names.

| Gap | Severity | Kind | Personas | Stories |
|---|---|---|---|---|
| [G01](#g01) The low-tier measurement cannot select the integrated adapter on the only machine that has it | major | missing-capability | The Sole Owner-Maintainer | SM-01 |
| [G23](#g23) Desktop browser coverage beyond headless Chrome is unverified | major | verification-debt | The Outside C++ Adopter, The Browser and Phone Demo Player | OCA-28, SM-09, BDP-06, BDP-07 |
| [G24](#g24) No frame-time evidence on the named low tier | major | verification-debt | The Outside C++ Adopter, The Sole Owner-Maintainer | SM-07 |
| [G25](#g25) The four-core hold has no launch line, and the bench never fans out | major | doc-drift | The Sole Owner-Maintainer | SM-02 |
| [G26](#g26) Undecided which p99 counts for the pipeline-depth claims | major | verification-debt | The Sole Owner-Maintainer | SM-03 |
| [G27](#g27) No method or instrument for R5 pacing and R6 phone frame timing in a browser | major | verification-debt | The Sole Owner-Maintainer | SM-08 |
| [G28](#g28) The Safari candidate machine's macOS and Safari ceiling is unrecorded | minor | verification-debt | The Sole Owner-Maintainer | SM-09 |
| [G62](#g62) The only bench result analyser is hard-wired to the EC2 NVIDIA host | minor | missing-capability | The Sole Owner-Maintainer | SM-05 |
| [G63](#g63) No local run procedure for LineSweeperFrameBench | minor | onboarding | The Sole Owner-Maintainer | SM-04 |
| [G64](#g64) STATUS.md's low-tier box describes a README contradiction that no longer exists | minor | doc-drift | The Sole Owner-Maintainer | SM-06 |
| [G69](#g69) web.md §9 says M3 is met in Firefox on a single manual LineSweeper pass | minor | doc-drift | The Sole Owner-Maintainer | SM-10 |
| [G70](#g70) No automated non-Chromium browser run | minor | missing-capability | The Sole Owner-Maintainer | SM-11 |

## E12. A maintainer's process, CI and deploy that one person and their agents can trust

As the sole maintainer, the merge procedure, required checks, pinned inputs, site deploy, captures and review ledgers are written in the tree and checked by CI, so a change or an external drift cannot land or ship unseen.

| Gap | Severity | Kind | Personas | Stories |
|---|---|---|---|---|
| [G05](#g05) The demo is live but the repository records it as undeployed, and its deploy checks are unticked | major | doc-drift | The Browser and Phone Demo Player, The Sole Owner-Maintainer, The Outside C++ Adopter | SM-12, SM-13, SM-44 |
| [G31](#g31) GL and Vulkan pixel contracts, release builds and wasm-debug are outside CI, and local runs leave no record | major | verification-debt | The Sole Owner-Maintainer | SM-18 |
| [G29](#g29) No index of reviews and their live ledgers | minor | doc-drift | The Sole Owner-Maintainer | SM-16 |
| [G30](#g30) No driver that verifies a change across every affected configuration | minor | verification-debt | The Sole Owner-Maintainer | SM-17 |
| [G32](#g32) The offline tooling tests are not a gate, and the review's recommendation to gate them has no ledger row | minor | verification-debt | The Sole Owner-Maintainer | SM-19 |
| [G65](#g65) The branch, PR and rebase-merge procedure exists only outside the tree | minor | release-process | The Sole Owner-Maintainer | SM-20 |
| [G66](#g66) The agent brief is duplicated in CLAUDE.md and AGENTS.md with no sync check | minor | other | The Sole Owner-Maintainer | SM-22, SM-43 |
| [G67](#g67) Golden-image review leaves no trace | minor | verification-debt | The Sole Owner-Maintainer | SM-23 |
| [G68](#g68) Nothing ties the hosted LineSweeper bundle to the engine commit it should match | minor | release-process | The Sole Owner-Maintainer | SM-26 |
| [G72](#g72) The null-backends-still-red CI check needs a deliberate failure nobody has scripted | minor | verification-debt | The Sole Owner-Maintainer | SM-15 |
| [G76](#g76) The game-header include wall recognises only a folder literally named game/ | minor | other | The Pinned Submodule Integrator | PI-18 |
| [G115](#g115) No procedure or cadence for moving dependency and toolchain pins | minor | release-process | The Sole Owner-Maintainer, The Outside C++ Adopter | SM-24 |
| [G120](#g120) The website's accessibility checks are manual one-off runs, and CI cannot catch a regression | minor | verification-debt | The Sole Owner-Maintainer | SM-28 |
| [G133](#g133) The Vulkan SDK is installed at 'latest' and the runner's Chrome floats, against CI's own pinning argument, and no scheduled run separates drift from a change | minor | release-process | The Sole Owner-Maintainer | SM-25 |
| [G134](#g134) Required status checks are tied to matrix parameter strings, held only in GitHub settings, and not required to be up to date | minor | release-process | The Sole Owner-Maintainer | SM-21 |
| [G135](#g135) The website plan's preview-before-production and rollback rules have no procedure, and the preview builds they rely on are off | minor | release-process | The Sole Owner-Maintainer | SM-29 |
| [G136](#g136) Cloudflare's build settings duplicate the website gate in a dashboard nothing checks, and wrangler also publishes a workers.dev copy | minor | release-process | The Sole Owner-Maintainer | SM-30, SM-31 |
| [G137](#g137) The site's LineSweeper footage predates a sample change that moved the stills, and nothing flags a stale capture or video | minor | verification-debt | The Sole Owner-Maintainer, The Outside C++ Adopter | SM-32 |

## The gaps in full

<a id="g01"></a>
### G01. The low-tier measurement cannot select the integrated adapter on the only machine that has it

**major** · missing-capability · E11 · journeys: sm-measure-the-low-tier

Every rasterising backend prefers the high-performance or discrete GPU: DXGI HIGH_PERFORMANCE on d3d11 and d3d12, Vulkan's discrete score, and for GL whichever adapter owns the display. LineSweeperFrameBench has no adapter option. On the one owned machine that holds PHILOSOPHY's named low tier (the Ryzen 9000 iGPU), the RTX 5080 wins every time, so the maintainer cannot take the measurement at all. No document describes the alternatives (an OS graphics preference, disabling the discrete GPU, a monitor on the motherboard output). Nothing warns that cross-adapter presentation, which the EC2 run already showed costs about 125 ms a frame and pushes WGL onto GDI, would invalidate the result.

**Evidence.**

- engine/render/d3d11/device_resources.cpp:472
- engine/render/d3d12/device_resources.cpp:150
- engine/render/vulkan/device_resources.cpp:628-629
- engine/render/gl/renderer.cpp:68-85
- bench/linesweeper_frame_result.cpp:182-236
- docs/performance/2026-09-19-g6f-reference.md:209-212
- docs/port/web-to-be-tested.md:5

**Scope check.** No conflict. Adapter selection fits T5 if it is an environment variable or a bench option, not a runtime seam change. A documented OS-level procedure would not touch the engine at all. Constrained hardware is the stated positioning (PHILOSOPHY Performance). Downstream of this: G24 (low tier unmeasured) cannot close until this one does.

**Adversarial review.**

- severity blocker -> major: 'Cannot take the measurement at all' is overstated. d3d11 and d3d12 do enumerate with DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE (d3d11/device_resources.cpp:469-472; d3d12/device_resources.cpp:147-150), and Vulkan scores the discrete GPU 2 (vulkan/device_resources.cpp:628-629). But the gap's own description lists OS-level workarounds that touch no code: disabling the discrete adapter, or driving the monitor from the motherboard output. So the journey has a costly, undocumented path rather than none, which is major and not blocker. G24 stays major either way.
- severity major -> major: The measurement is blocked only in-engine, not in practice. The register's own description lists OS-level routes that need no code: disabling the discrete adapter so that it drops out of DXGI and Vulkan enumeration, or driving the display from the motherboard output. What is missing is the documented procedure and the warning about cross-adapter presentation. Missing documentation is major for a secondary persona's journey, not a blocker.

<a id="g02"></a>
### G02. Platform reach is stated several incompatible ways on the public face

**major** · doc-drift · E01 · journeys: oca-evaluate-and-decide, anti-recognise-not-for-me, anti-nms-recognise-early, srl-read-a-finished-game

Home's facts strip says 'Windows today' (index.mdx:29), and why.mdx's fit test turns away anyone who wants 'a platform other than Windows' (why.mdx:24-26). The same pages, and the README, say all six samples run in a desktop browser (index.mdx:137-139; why.mdx:193-195; README.md:55). why.mdx:170-172 goes further and says the samples run 'nearly everywhere', but the plan withholds the wasm from phones until a real iPhone and a real Android phone have played it (docs/port/web.md:738-752), and Safari is unchecked (web.md:1517). docs/website-plan.md:36 still says support 'must be stated as Windows', which predates the browser port. The result is that a skimming evaluator reads 'no' and a full reader reads 'yes'. The primary persona's success condition, a yes or no that every public page agrees with, fails. So does the anti-native-multiplatform-shipper's early exit.

**Evidence.**

- website/src/content/docs/index.mdx:29, 137-139
- website/src/content/docs/why.mdx:24-26, 170-172, 193-195
- README.md:55
- docs/port/web.md:738-752, 1517
- docs/website-plan.md:36

**Scope check.** No conflict. The browser build exists and is CI-tested (CLAUDE.md preset table, wasm-*). The fix keeps native Linux, macOS and Android stated as provisional, not supported, which is why the anti-native-multiplatform-shipper persona exists. It also has to amend website-plan.md:36.

<a id="g03"></a>
### G03. The Why page and PHILOSOPHY deny the split-screen sample that ships

**major** · doc-drift · E01 · journeys: oca-evaluate-and-decide, oca-grow-a-game, anti-recognise-not-for-me, sm-land-an-engine-change

why.mdx:98-101 says no sample in the repository draws a second view and that a public multiplayer example is 'planned'. PHILOSOPHY.md:663 makes the same claim inside a T1 argument whose premise ('no caller in this tree') is therefore now false. samples/local_multiplayer ships two views, and examples.mdx, guides/local-multiplayer.mdx and concepts/drawing.mdx:89 all describe it. why.mdx was last committed on 2026-10-13 and kept the sentence. For the primary persona split-screen is the core fit question, and the page that answers it gives the wrong answer on the live site. The page should point at the sample and say it covers two players with fixed slots (arena.h:16). Neither the citation check nor website.yml catches this kind of truth drift: they check names and structure only, by their own statement.

**Evidence.**

- website/src/content/docs/why.mdx:96-101
- docs/design/PHILOSOPHY.md:663
- website/src/content/docs/examples.mdx:109-110
- website/src/content/docs/docs/guides/local-multiplayer.mdx:11-14
- website/src/content/docs/docs/concepts/drawing.mdx:89
- samples/local_multiplayer/arena.h:16
- samples/local_multiplayer/README.md:32-40
- cmake/check_doc_citations.cmake:11-25

**Scope check.** No conflict. docs/design changes by amendment (CLAUDE.md 'Reading the documents correctly'). The T1 argument at PHILOSOPHY.md:663 has to be restated, not only the sentence deleted. Online play stays permanently out.

<a id="g04"></a>
### G04. The live LineSweeper demo is not linked from the site or the README

**major** · onboarding · E01 · journeys: oca-evaluate-and-decide, bdp-desktop-click-and-play, bdp-phone-visit, srl-read-a-finished-game

A playable LineSweeper page is served at https://mattharding.dev/demo/linesweeper/ (HTTP 200 on 2026-10-13, rechecked in this session). Neither website/src nor README.md links it: a grep for mattharding or demo/linesweeper returns nothing. The site claims all six samples run in a browser but shows only footage, so the evaluator, the demo player arriving from Labrador and the learner off Windows all conclude that trying the engine needs a Windows toolchain. docs/website-plan.md:90 itself says the link 'is a page edit, not a technical project'.

**Evidence.**

- curl https://mattharding.dev/demo/linesweeper/ -> 200 (2026-10-13)
- grep -rn 'mattharding|demo/linesweeper' website/src README.md -> no matches
- website/src/content/docs/index.mdx:37-59, 136-138
- website/src/content/docs/why.mdx:193-195
- website/src/content/docs/examples.mdx (no Play link)
- website/src/components/LineSweeperFootage.astro:20-23
- docs/website-plan.md:90

**Scope check.** No conflict. website-plan.md:90 decides the site itself need not host demos, but it plans to link the mattharding.dev demo. Linking reopens nothing, and hosting on labradorengine.com would.

<a id="g05"></a>
### G05. The demo is live but the repository records it as undeployed, and its deploy checks are unticked

**major** · doc-drift · E12 · journeys: bdp-desktop-click-and-play, bdp-phone-visit, sm-close-hardware-and-deploy-milestones, oca-evaluate-and-decide

Two journey walks rated 'no live URL' a blocker, citing docs/port/web.md:616-618 ('it is not deployed yet') and :1461 ('M5 is not done'). The URL answers 200, so the blocker was a reading of stale records, not the live state, and this gap is re-rated. What remains open is real. Every M5 deploy check in web-to-be-tested.md:86-100 is unticked on a public page: application/wasm, Brotli, a phone getting the recording and fetching no wasm, and the no-WebGL2 message. The checklist's deploy step names the PersonalWebsite branch linesweeper-demo, which was fast-forwarded into main and is gone; the actual route is main plus a manual 'npm run deploy'. website-plan.md:90 still says 'not yet deployed'.

**Evidence.**

- curl https://mattharding.dev/demo/linesweeper/ -> 200 (2026-10-13)
- docs/port/web.md:604-618, 1461, 1519
- docs/port/web-to-be-tested.md:86-100 (unticked; :88-91 names branch linesweeper-demo)
- docs/website-plan.md:90
- C:\VSProjects\PersonalWebsite git reflog 'merge linesweeper-demo: Fast-forward'; README.md:20-25

**Scope check.** No conflict. Deploying is the owner's job in another repository by design (web.md §7). Closing this means ticking the checks against the live URL and amending the records, which adds no scope.

<a id="g06"></a>
### G06. The core-longpaths alternative fixes the clone but not the build

**major** · doc-drift · E02 · journeys: get-started-build-labrador

prerequisites.mdx:53-60 offers git core.longpaths to a reader who 'would rather keep your usual folder', as though that were enough, and build-labrador.mdx:74-78 repeats it. In a 123-character parent folder, with core.longpaths true and LongPathsEnabled=1, configure printed six CMAKE_OBJECT_PATH_MAX warnings. Seven compiles under tools/linesweeper_capture and samples/local_multiplayer then failed with 'fatal error C1083: Cannot open compiler generated file: '': Invalid argument'. The same tree mapped to W:\ with subst built 288/288 with 0 warnings. The 'If something fails' list has no entry for either message, so the page's offered alternative ends in an unexplained compiler error. This was observed in the literal walkthrough.

**Evidence.**

- website/src/content/docs/docs/get-started/prerequisites.mdx:53-60
- website/src/content/docs/docs/get-started/build-labrador.mdx:72-90
- scratchpad/engine_configure.log (6 CMake Warning blocks, e.g. LineSweeperViewTests.dir 209 chars, max 250)
- scratchpad/engine_build.log (7 FAILED compiles, C1083)
- scratchpad/engine_build2.log (288/288, 0 warnings under subst)

**Scope check.** No conflict. The fix is documentation (state that a short folder is required and add a C1083 entry) or shorter object paths for tests and tools. It is in keeping with T6's loud, named failure.

<a id="g07"></a>
### G07. README's consumer section omits the manifest, the toolchain preset and fxc

**minor** · onboarding · E02 · journeys: pi-consume-library-only

README.md '## Using Labrador in a project' stops at the CMake lines. A consumer also needs three things. First, a top-level vcpkg.json naming directxtk[xaudio2redist] at Labrador's baseline, because vcpkg reads only the top-level manifest. Second, a preset that chains the vcpkg toolchain. Third, fxc on PATH for the d3d11 default. Without them configure fails at find_package(xaudio2redist). Only the website page says this, and the README neither says it nor links to that page.

**Evidence.**

- README.md:152-187
- engine/CMakeLists.txt:397-404
- website/src/content/docs/docs/get-started/your-project.mdx:88-96

**Scope check.** No conflict. It documents the existing path and adds no mechanism (T1).

**Adversarial review.**

- severity major -> minor: README.md:5 opens with 'Website and documentation: labradorengine.com', and your-project.mdx:88-96 covers the manifest, presets and toolchain in full. A reader is one link away from everything the README omits, and configure fails loudly at find_package (engine/CMakeLists.txt:401-402). The README section is incomplete, but the gap does not block the journey.

<a id="g08"></a>
### G08. No CI job builds a consumer-shaped project, and the walkthrough record predates the current engine

**major** · verification-debt · E02 · journeys: oca-start-project-first-build, pi-consume-library-only, pi-report-a-gap-upstream

The consumer contract is the PROJECT_IS_TOP_LEVEL guard, the explicit labrador_settings link, the CMAKE_CURRENT_SOURCE_DIR include root and the derived manifest. It was verified once by hand: the walkthrough files were last touched on 2026-10-04 (68b392d), and their README records four include edits where the page now lists five. ColourWars also checked it, at a pin 45 commits behind. Since then 27 commits have touched engine/, cmake/, the root CMakeLists or samples/minimal. Get Started still claims every command was run against the revision in the footer, but the footer is only the site's own build commit. A change that breaks consumers, such as a CMAKE_SOURCE_DIR use, a new required package, or a deleted or retyped public name, surfaces first for a stranger or at the next pin bump.

**Evidence.**

- website/src/walkthrough/README.md:7-21
- website/src/content/docs/docs/get-started/index.mdx:23-29
- website/src/components/Revision.astro
- git rev-list --count 68b392d..HEAD -- engine CMakeLists.txt cmake samples/minimal = 27
- git rev-list --count 4cefb68..master = 45
- .github/workflows/ci.yml and website.yml: no consumer/walkthrough/add_subdirectory build
- docs/design/PHILOSOPHY.md:742-751

**Scope check.** No conflict as long as the job builds a public out-of-tree consumer (the walkthrough files against the checkout). Building ColourWars from Labrador CI would violate PHILOSOPHY.md:747-749 ('the engine cannot reach a repository that consumes it, and should not learn how'). The cost is CI time on a Windows runner.

<a id="g09"></a>
### G09. No path for adding a browser build to an adopter's own project

**major** · onboarding · E08 · journeys: oca-start-project-first-build, oca-ship-desktop-and-browser

CLAUDE.md says minimal's main_web.cpp and CMakeLists.txt are 'the browser build a new project copies', and README.md:180-182 points to minimal as that shape. The walkthrough, however, replaces the sample's CMakeLists with a Windows-only one (no EMSCRIPTEN branch) and ships only x64 presets. To build for the browser, an adopter has to reverse-engineer three things: minimal's EMSCRIPTEN branch, the engine's wasm-base preset (the chained toolchain, the triplet, LABRADOR_RENDER_BACKEND=gl) and the shell page. Get Started never mentions wasm, Emscripten or the browser.

**Evidence.**

- README.md:180-182
- website/src/walkthrough/game-CMakeLists.txt:1-29
- website/src/walkthrough/CMakePresets.json:1-28
- samples/minimal/CMakeLists.txt:18-36
- CMakePresets.json:100-114
- grep wasm|emscripten|browser website/src/content/docs/docs/get-started/ -> no hits
- CLAUDE.md 'Build, test, run' (minimal paragraph)

**Scope check.** No conflict with a non-goal, since the browser is a supported platform the site claims. How far to go (a full consumer wasm walkthrough while M3 and M5 are open) is the owner's scope call. The smaller sibling, explaining the dead web files, is G27.

<a id="g10"></a>
### G10. The consumer template has no test target and no guidance for headless rules tests

**major** · onboarding · E03 · journeys: oca-grow-a-game, get-started-first-change

why.mdx sells LineSweeper's headless rules tests as a reason to choose Labrador. A consumer project gets none of that pattern. The walkthrough has no test target, no enable_testing(), no test preset and no ctest step, and the generated vcpkg.json strips doctest as test-only. Labrador's own enable_testing() runs at top level only. So the persona has to reverse-engineer find_package(doctest), the dependency, enable_testing and add_test from tests/linesweeper/CMakeLists.txt, and their first test also means adding a dependency.

**Evidence.**

- website/src/content/docs/why.mdx:108-133
- tests/linesweeper/CMakeLists.txt:1-31
- website/src/components/ConsumerManifest.astro:7-9, 17
- CMakeLists.txt:42-48
- website/src/walkthrough/root-CMakeLists.txt
- website/src/walkthrough/game-CMakeLists.txt
- website/src/walkthrough/CMakePresets.json:24-27 (no testPresets)
- website/src/content/docs/docs/get-started/first-change.mdx:9-10, 53-61

**Scope check.** No conflict. It is documentation plus a template test target, with no engine mechanism (T1 is not engaged). Adding doctest to the template has to be weighed against T3. The engine's own tests stay out of a consumer's ctest (CMakeLists.txt:42-48).

<a id="g11"></a>
### G11. An adopter has no start-up or smoke check for their own game, page or package

**major** · verification-debt · E03 · journeys: oca-ship-desktop-and-browser, oca-take-engine-upgrade

The engine's *PageTests (run_page.mjs in headless Chrome) are registered only when the engine is the top-level project, a stated choice ('the runner is this repository's, not the project's'). Neither the walkthrough CMake nor samples/minimal gives the adopter a launch smoke check (minimal has no smoke flag). So nothing in the adopter's own build checks that their Windows folder starts or their page loads and quits. Behaviour regressions such as 92966b0's content-root change surface only when the adopter launches by hand. The owner's client needed a follow-up fix for that one (ColourWars 1ae9797).

**Evidence.**

- CMakeLists.txt:42-47, 142-182
- tests/render/web/run_page.mjs
- grep smoke samples/minimal -> none
- website/src/content/docs/docs/get-started/your-project.mdx:119-122
- ColourWars commit 1ae9797

**Scope check.** Touches a recorded decision: the CMakeLists.txt comment keeps the runner out of consumers' builds. Closing this means amending that decision, or documenting a by-hand run_page.mjs invocation and a template smoke flag. T1 argues against a general test framework, and one documented invocation is not one. minimal's copy-me size (~560 lines) bounds any addition.

<a id="g12"></a>
### G12. Content tools are named but not obtainable, and the premultiplied-alpha rule is unstated

**major** · content-pipeline · E04 · journeys: oca-grow-a-game

The sprites guide names texconv and MakeSpriteFont but gives no link, install route or command line. The audio guide does give these for XWBTool. The engine blends premultiplied and does not premultiply on load, yet no page says a translucent texture must be premultiplied when converted (texconv -pmalpha). A stranger's first transparent sprite will fringe and nothing explains why. Get Started installs none of these tools.

**Evidence.**

- website/src/content/docs/docs/guides/sprites-and-text.mdx:16-20, 122
- engine/render/block_decode.h:32-37
- website/src/content/docs/docs/guides/audio.mdx:131-140
- docs/review/all-findings.md:9876

**Scope check.** No conflict if closed with documentation. An editor is permanently out of scope (CLAUDE.md 'Known-absent'), and an engine asset pipeline would need its own T1 argument. Neither is required.

**Adversarial review.**

- severity major -> major: Severity unchanged, with one correction. sprites-and-text.mdx:123-125 does state 'Blending is premultiplied' and tells readers to premultiply a tint colour, so the rule is not wholly unstated. What is missing is the texture consequence: that a translucent .dds must be premultiplied at conversion (block_decode.h:37 says 'Nothing is premultiplied or unpremultiplied on the way through'). texconv and MakeSpriteFont are still named with no link or command (lines 18-19).

<a id="g13"></a>
### G13. Guide samples borrow content and sources from other sample folders

**minor** · api-ergonomics · E04 · journeys: oca-grow-a-game

collision, components and local_multiplayer each embed or copy ../minimal/content/fonts and ../linesweeper/content/textures. local_multiplayer also compiles ../../tools/linesweeper_capture/png_file.cpp. A persona who copies one of these folders gets broken paths, and no guide says what to take from where. The solid-rectangle recipe depends on white.dds, which minimal (the template) does not ship.

**Evidence.**

- samples/local_multiplayer/CMakeLists.txt:24-25, 34-35, 43-47, 50, 56-61
- samples/collision/CMakeLists.txt:18-19, 48-52
- samples/components/CMakeLists.txt:19-20, 39-43
- website/src/content/docs/docs/guides/sprites-and-text.mdx:104-115
- samples/minimal/content (fonts/, manifest.json only)

**Scope check.** No conflict. The reuse was a deliberate size choice (local_multiplayer/CMakeLists.txt:50). Shipping a white texel in minimal, or stating what each sample borrows, costs nothing against T1 or T3.

**Adversarial review.**

- severity major -> minor: The borrowing is real and slightly wider than stated: collision also compiles ../../tools/linesweeper_capture/png_file.cpp (samples/collision/CMakeLists.txt:38). Copying a whole guide-sample folder still fails loudly at configure or build, naming the missing ../minimal or ../linesweeper path. The borrowed content is two files (courier_new_bold_16.spritefont and white.dds), and minimal, the documented template, borrows nothing. This is an inconvenience with a named error, so minor.

<a id="g14"></a>
### G14. The engine has no install or package path or file list for a Windows release

**major** · missing-capability · E07 · journeys: oca-ship-desktop-and-browser, pi-ship-windows-release

Labrador has no install(), export(), CPack or content-staging helper, and no page lists what a player needs: the exe, DirectXTK.dll, XAudio2_9Redist.dll, manifest.json and the content folders. The build folder mixes these with CMake intermediates and stale outputs; one example is ArtAttackSample.exe beside MinimalSample.exe. The only worked example is private: ColourWars needed 139 lines across two files (Release/backend guards, $<TARGET_RUNTIME_DLLS>, InstallRequiredSystemLibraries, a hand-kept asset list cross-checked against the manifest, licence copying). The template the primary persona copies only mirrors content for the dev loop.

**Evidence.**

- grep 'install(|CPack|export(' over CMakeLists.txt, engine/CMakeLists.txt, samples/, cmake/ -> nothing
- website/src/content/docs/docs/get-started/build-labrador.mdx:30-31
- website/src/walkthrough/game-CMakeLists.txt:15-29
- samples/minimal/CMakeLists.txt:3-4, 51-53
- ls out/build/x64-release/samples/minimal
- ColourWars cmake/release_package.cmake:3-62; cmake/release_assets.cmake:1-77
- ColourWars docs/review/gap-audit.md F8 (historical)

**Scope check.** PHILOSOPHY's non-goals rule out an editor or 'general tooling' (PHILOSOPHY.md:781-800). Packaging documentation, or a few install() lines in the template, is not that. An engine staging helper is defensible under T1 because a real client hand-wrote it, but it must be argued as demand, not assumed. T5 favours configure-time checks.

<a id="g15"></a>
### G15. Release builds depend on the dynamic MSVC runtime, and nothing says so

**major** · platform · E07 · journeys: oca-ship-desktop-and-browser

The release exe and DirectXTK.dll import MSVCP140, VCRUNTIME140 and VCRUNTIME140_1. These are not copied beside the exe, and no CMAKE_MSVC_RUNTIME_LIBRARY or static triplet is set. On a clean Windows machine without the VC++ 2015-2022 redistributable the game fails in the OS loader, before any engine code runs, and so without T6's loud, named error. Nobody has run the build on a clean machine.

**Evidence.**

- PE import table of out/build/x64-release/samples/minimal/MinimalSample.exe and DirectXTK.dll (parsed in a walk session)
- grep MSVC_RUNTIME over cmake/, CMakeLists.txt, CMakePresets.json -> no hits
- docs/design/PHILOSOPHY.md:109-122 (T6)

**Scope check.** No conflict. A stated requirement fits T6. Static linking would change DirectXTK's vcpkg triplet, a T9 'bought edge' choice to make deliberately.

<a id="g16"></a>
### G16. No notice or licence guidance for what a shipped game links, desktop or browser

**major** · onboarding · E07 · journeys: oca-ship-desktop-and-browser, pi-ship-windows-release

NOTICE covers code carried in the tree. It does not cover the binaries a desktop game ships: DirectXTK.dll (MIT, from vcpkg), XAudio2_9Redist.dll (Microsoft's redistributable terms), the CRT, or the licences of DirectXMath and rapidjson. ARCHITECTURE names those dependencies only as build inputs. In the browser, only LineSweeper gathers its notices (Emscripten, libc++, musl and others); minimal's web build, the template, does not. ColourWars recovered its list partly by reading vcpkg's per-port copyright files, which is a vcpkg detail.

**Evidence.**

- NOTICE ('Code carried in this repository')
- README.md:285-289
- docs/design/ARCHITECTURE.md:109-111
- vcpkg.json (directxtk with xaudio2redist)
- samples/linesweeper/CMakeLists.txt:119-151; samples/linesweeper/web/NOTICE.txt
- samples/minimal/CMakeLists.txt:18-36 (no notices)
- ColourWars cmake/release_package.cmake:43-54

**Scope check.** No conflict. It documents obligations the engine's own dependencies create, the redistribution half of T9's declared bought edge. It could also make tools/web_bundle usable from the template.

<a id="g17"></a>
### G17. No release, tag or version, and no criterion for 'the API settles'

**major** · release-process · E10 · journeys: oca-evaluate-and-decide, oca-take-engine-upgrade, pi-move-the-pin, sm-evolve-api-and-move-the-client

There are 0 git tags and 0 GitHub releases, vcpkg.json's 0.1.0 has never changed, and the root has no CHANGELOG. PHILOSOPHY.md:739-741 promises versioned releases with deliberate, batched, documented breaks 'once the engine/game split lands and the API settles'. The split has landed, and nothing defines 'settles', so nothing triggers the promise. The website's versioned-docs routing (website-versioning.md) is designed for a first release with no trigger. Every downstream, from an evaluator judging stability to ColourWars and any adopter moving a pin, consumes an arbitrary commit.

**Evidence.**

- git tag -> 0 entries (2026-10-13)
- gh api repos/matt439/Labrador/releases -> 0
- vcpkg.json:3
- docs/design/PHILOSOPHY.md:739-741
- docs/website-versioning.md:8-30
- website/src/components/Revision.astro:7
- website/src/content/docs/index.mdx:33, 144

**Scope check.** Cutting 1.0 now would conflict with PHILOSOPHY.md:739-741. Stating the criterion, or a 0.x tag that promises no compatibility, does not. T1 argues for a minimal process (a tag plus notes), and T9's single-maintainer capacity bounds curation.

<a id="g18"></a>
### G18. No engine-side record of consumer-visible and breaking changes

**major** · doc-drift · E10 · journeys: oca-take-engine-upgrade, pi-move-the-pin, pi-report-a-gap-upstream, sm-evolve-api-and-move-the-client

Commit subjects say what was done, not whom it breaks. Between ColourWars' pin and master there are 45 commits, 131 engine files changed and one moved source file (window.cpp to win32/window.cpp). Since 2026-09-13, 25 of 69 commits touched engine headers. Source breaks in this range, such as initialize() losing its arguments, window() returning void* and ApplicationOptions::visible replacing SW_HIDE, are recorded only as rows D2, D6 and D10-D15 of a browser-port plan (web.md §8). That plan has a stale D2 row about ColourWars and says nothing about the knock-on unused-parameter error under /W4 /WX, or that 4175640's sample diffs are the migration template. Silent behaviour changes, such as 92966b0's content-root move, produce no compile error at all. The real migration notes are written after the fact in the private client's engine-reports.md, and every 'check master first' or 'what did this bump close' means reading the log by hand.

**Evidence.**

- git log --oneline --since=2026-09-13 -> 69; -- 'engine/*.h' -> 25
- git diff --stat 4cefb68..master -- engine (131 files)
- git diff --name-status -M 4cefb68..master -- engine (R062 window.cpp)
- git show 92966b0; ColourWars 1ae9797
- docs/port/web.md:779, 797, 807-822
- git show 4175640 --stat
- docs/review/2026-09-26/FIXES.md:9
- ColourWars docs/review/engine-reports.md:17-46, 834-842
- ls CHANGELOG* -> absent

**Scope check.** No conflict. PHILOSOPHY.md:739-741 asks for 'documented' breaks. CONVENTIONS bars history in code comments, but a separate changes file is not a comment. It must stay a list and not become a deprecation framework (T1). This can be closed independently of G17, because a running list needs no tags.

<a id="g19"></a>
### G19. The caller-count deletion rule cannot be met for a public repository, and there is no deprecation path

**major** · api-ergonomics · E10 · journeys: oca-take-engine-upgrade, sm-evolve-api-and-move-the-client

PHILOSOPHY.md:742-751 deletes public names only on a caller count that names the repositories counted, and 'a count that cannot be taken is a reason to keep'. The repository is public, so an outside adopter's code can never be counted. Read literally, the rule forbids every future deletion, or else silently ignores outside callers. No deprecation step, alias period or release-boundary rule says how a name leaves. Past renames (6b5c0bd) and report 7's 'uncalled' deletions forced client rewrites.

**Evidence.**

- docs/design/PHILOSOPHY.md:742-758
- commit 6b5c0bd; ColourWars 77a7a7f, a9a7cd3
- ColourWars docs/review/engine-reports.md:496
- docs/file-length-audit.md:145, 351
- docs/website-versioning.md:8-10

**Scope check.** Deprecation shims while the API is unsettled would conflict with T1 and T3. A build step that reaches client repositories conflicts with PHILOSOPHY.md:748-750. A written policy, such as removing names only at a tagged release with a note, conflicts with neither.

<a id="g20"></a>
### G20. No documented procedure for moving the engine pin

**major** · onboarding · E10 · journeys: oca-take-engine-upgrade, pi-move-the-pin

your-project.mdx:54-56 says only that moving the pin is deliberate. No page gives the commands, the order of checks (build first, re-diff the engine's vcpkg.json since a consumer names the engine's dependencies itself, rebuild, run, diff the minimal template), or where to find what changed. In the 4cefb68..master range the manifest changed (lodepng added, directxtk platform-qualified), and only reading CMakeLists.txt:47 shows the consumer needs neither. No supported MSVC version is stated, while labrador_settings applies /W4 /WX to the adopter's code too. The only bump procedure that exists is the private ColourWars README and CLAUDE.md.

**Evidence.**

- website/src/content/docs/docs/get-started/your-project.mdx:54-56, 89-92
- git diff 4cefb68..0977112 -- vcpkg.json
- CMakeLists.txt:47
- cmake/settings.cmake:15-17
- engine/CMakeLists.txt:389-404
- website/src/content/docs/docs/troubleshooting.mdx:46-50
- C:\VSProjects\ColourWars\CLAUDE.md:154-156; ColourWars README.md '## The engine submodule'

**Scope check.** No conflict. It is one documentation page. Relaxing /WX for consumers would fight the zero-suppression rule and is not the fix.

<a id="g21"></a>
### G21. No public route for questions or API requests

**major** · onboarding · E10 · journeys: oca-evaluate-and-decide, pi-report-a-gap-upstream

The issue tracker is the only stated route, and it is named only for build and doc problems. Discussions are off, .github/ holds only workflows, and there is no CONTRIBUTING file or issue template. An evaluator with a fit question ('will four pads work in the browser?') has nowhere suitable to ask it. The API-request workflow that worked (an exhibit-backed report, answered with T-number reasoning) has run only through a private file in the client repository, read by the engine's author. Nothing tells any other integrator that a request should lead with its workaround as the exhibit, where it goes, or how it is judged.

**Evidence.**

- website/src/content/docs/docs/troubleshooting.mdx:8-11
- website/src/content/docs/docs/get-started/index.mdx:24-29
- ls .github -> workflows/ only; no CONTRIBUTING
- gh api -> has_discussions false; gh issue list --state all -> empty
- ColourWars docs/review/engine-reports.md:19-20, 48-61
- Labrador commit 343433c ('ColourWars filed this as report 7')

**Scope check.** No conflict. why.mdx:25-26 concedes there is no large community, and a question channel is not one. Publishing the exhibit rule is T1's demand test applied to intake.

<a id="g22"></a>
### G22. The 'Introduction to C++' series T10 relies on has no artefact, plan or status

**minor** · missing-capability · E01 · journeys: srl-learn-cpp-through-the-engine, oca-evaluate-and-decide

T10 makes the beginner on-ramp the price of having no scripting layer, and 'The public face' names an 'Introduction to C++' video series as the API's acceptance test. PHILOSOPHY.md:734 says it 'accompanies the engine'. Nothing exists in either repository or on the site: no outline, episode, page or milestone. A reader cannot tell a target from a deliverable.

**Evidence.**

- docs/design/PHILOSOPHY.md:182-183, 734-738
- git grep -i 'introduction to c++' -> PHILOSOPHY.md:734, docs/review/round-2/all-findings.md:46024 only
- docs/website-plan.md:59

**Scope check.** No conflict. T10 requires it. Its priority sits below the primary persona's work. The target-tense rule allows the sentence, but a status note is still owed to a reader. Closing it by producing the material or by publishing a plan are both allowed.

**Adversarial review.**

- severity major -> minor: The journey it blocks (srl-learn-cpp-through-the-engine) belongs to a tertiary persona, and its premise is a hypothesis in personas.json. docs/design is target-tense by rule, so the sentence at PHILOSOPHY.md:734 is permitted as a target. The register's own out_of_scope_check puts its priority below the primary persona's work. What is owed is a status note, which is minor.

<a id="g23"></a>
### G23. Desktop browser coverage beyond headless Chrome is unverified

**major** · verification-debt · E11 · journeys: oca-ship-desktop-and-browser, bdp-desktop-click-and-play, bdp-when-it-goes-wrong

Everything a desktop visitor or an adopter's players depend on beyond headless Chrome and SwiftShader is unchecked. That covers Safari (M3's outstanding done-when), Firefox on the bundle and sample pages (the runner cannot drive it), a physical pad, 60 Hz and high-refresh pacing (R5), HUD legibility at the site's scales, the no-WebGL2 onError message, a real GPU-reset or hidden-tab context loss, pad suspend and resume on a real tab switch, and audible Web Audio. Neither page states which browsers are supported, so an adopter cannot promise their page works in players' browsers.

**Evidence.**

- docs/port/web.md:835 ('Where it stands, 2026-10-11'), :854, :1059, :1487-1488, :1517 (M3), M4/M7a/M7b rows
- docs/port/web-to-be-tested.md:27-34, 36-42, 44-52, 92-98, 102-106

**Scope check.** No conflict. These are the documented done-whens of M3 and M4. They need hardware and the owner's time, not design. Closing them must not imply the low-tier claim (G24).

<a id="g24"></a>
### G24. No frame-time evidence on the named low tier

**major** · verification-debt · E11 · journeys: oca-evaluate-and-decide, oca-ship-desktop-and-browser

Speed is a stated reason to pick Labrador up (PHILOSOPHY.md:5-6), and constrained hardware is its positioning. The named low tier, the integrated Radeon in a Ryzen 9000 desktop package with the process held to four cores, has never been measured (PHILOSOPHY.md:681-699). The site publishes no figures (why.mdx:186-189), and the EC2 runs are explicitly not that tier. An evaluator cannot verify the speed claim, and an adopter cannot state minimum specs. LineSweeperFrameBench exists to take the measurement, but G01 blocks it.

**Evidence.**

- docs/design/PHILOSOPHY.md:5-6, 677-711
- website/src/content/docs/why.mdx:185-189
- docs/performance/2026-09-19-g6f-reference.md
- docs/review/backend-equivalence-2/STATUS.md:528-535

**Scope check.** No conflict. T8 and 'measured, not guessed' call for it. PHILOSOPHY.md:700-706 keeps absolute timing out of CTest, and a declared published measurement respects that. Depends on G01, G25, G26 and G62.

<a id="g25"></a>
### G25. The four-core hold has no launch line, and the bench never fans out

**major** · doc-drift · E11 · journeys: sm-measure-the-low-tier

PHILOSOPHY defines the low tier as including 'the process held to four cores' and says the hold changes how many workers the fan-out gets. LineSweeperFrameBench builds its Scene with no pool and one view, so it never fans out. No option, script or document gives the affinity launch line. A maintainer cannot reproduce that part of the definition as written, or tell what it is meant to measure.

**Evidence.**

- docs/design/PHILOSOPHY.md:690-696
- bench/linesweeper_frame_bench.cpp:93, 236
- engine/scene/scene.cpp:187-191
- bench/linesweeper_frame_result.cpp:182-236

**Scope check.** No conflict. It is a launch procedure or a PHILOSOPHY amendment (docs/design changes by amendment). Neither touches T1-T12.

<a id="g26"></a>
### G26. Undecided which p99 counts for the pipeline-depth claims

**major** · verification-debt · E11 · journeys: sm-measure-the-low-tier

Ratchet §5 says the whole-frame p99 is not quotable until the bench has one clock, and that an unsynchronised present needs a present-mode choice the seam does not offer. The record + submit and update p99s are quotable now. PHILOSOPHY's two pipeline-depth claims need 'a measured p99' of latency. Nothing decides whether the record + submit column meets that, or whether the one-clock change must land first.

**Evidence.**

- docs/performance/2026-09-19-g6f-ratchet.md:246-286
- docs/design/PHILOSOPHY.md:700-704
- bench/frame_statistics.h:18

**Scope check.** Adding an unsynchronised present mode is a seam change that would need arguing under T5, T8 and SEAM.md. Choosing --pacing presentation needs no code. The decision itself conflicts with nothing.

<a id="g27"></a>
### G27. No method or instrument for R5 pacing and R6 phone frame timing in a browser

**major** · verification-debt · E11 · journeys: sm-close-hardware-and-deploy-milestones

The checklist asks for 'judder or none' on 60 Hz and high-refresh displays, and for timing the 9,600-particle top-out frame on a phone. The only instrument is LineSweeperFrameBench, which is Windows-only by construction. The development page has no timing readout, and nothing defines how judder is observed or recorded. When the hardware is available, results will be ad hoc and not comparable.

**Evidence.**

- docs/port/web-to-be-tested.md:36-42, 71
- docs/port/web.md:455-461, 1060-1063, 1603-1608
- bench/CMakeLists.txt:42-46

**Scope check.** No conflict if it stays a page-side measurement or a documented protocol. A full browser benchmark harness would need its own T1 justification.

<a id="g28"></a>
### G28. The Safari candidate machine's macOS and Safari ceiling is unrecorded

**minor** · verification-debt · E11 · journeys: sm-close-hardware-and-deploy-milestones

M3's done-when depends on Safari, and the only named candidate is a 2013 MacBook Air (web.md:1059-1060). The tree records that machine only for Boot Camp and Windows use. Nowhere says which macOS and Safari it can run, so whether M3 can close on owned hardware is unknown. The checklist's Safari 18.4 note and the wasm-exceptions flag show the version ceiling matters.

**Evidence.**

- docs/port/web.md:854, 1059-1060, 1517
- docs/port/web-to-be-tested.md:29-32
- docs/survey/2026-08-26-status.md:520-530
- cmake/settings.cmake:75-77

**Scope check.** No conflict. It records a fact and does not propose buying hardware.

**Adversarial review.**

- severity major -> minor: The checklist already assigns this step: docs/port/web-to-be-tested.md:30-32 says 'The 2013 MacBook Air is the candidate; note its Safari ceiling before moving off legacy wasm exceptions, which needs Safari 18.4.' The build currently uses -fwasm-exceptions (cmake/settings.cmake:74-80), and the ceiling matters only for a future move off legacy exceptions. Recording it is part of a planned check, not an unplanned unknown.

<a id="g29"></a>
### G29. No index of reviews and their live ledgers

**minor** · doc-drift · E12 · journeys: sm-land-an-engine-change

docs/review/README.md is the 2023 ArtAttack review and indexes none of the later reviews. CLAUDE.md's map omits 2026-09-26 and gpt6. Only backend-equivalence-2 and 2026-09-26 have a STATUS.md. The open items of d3d12 (section 6) and vulkan (section 7 plus two from section 6) live only as CLAUDE.md prose and private memory. Neither a maintainer nor an agent can find from the tree which ledger is live or what is still open.

**Evidence.**

- docs/review/README.md:1-10
- docs/review/2026-09-26/STATUS.md:4
- grep '2026-09-26|gpt6' CLAUDE.md -> no match
- ls docs/review/d3d12 docs/review/vulkan docs/review/gpt6 -> no STATUS.md

**Scope check.** No conflict. It applies the repo's own status-companion rule (backend-equivalence-2 README §6). It is documentation, so T1 is not engaged.

**Adversarial review.**

- severity major -> minor: Half-overstated. The d3d12 and vulkan open items are recorded in CLAUDE.md, a tracked file (the 'Reading the documents correctly' section names d3d12 §6 and vulkan §7 plus two §6 items), so they are findable from the tree. The confirmed part: grep for '2026-09-26' or 'gpt6' in CLAUDE.md returns 0 matches, and docs/review/README.md:1-10 is still the ArtAttack review. This hampers navigation but does not hide the work.

<a id="g30"></a>
### G30. No driver that verifies a change across every affected configuration

**minor** · verification-debt · E12 · journeys: sm-land-an-engine-change

CLAUDE.md requires checking a render change against five backends, and a gl/ change under wasm as well. There are 12 presets with no workflowPresets, no script and no table mapping changed paths to the presets they require. CI's wasm null-backend run is an ad hoc -B/-D command with no preset. Each configuration needs its own environment (vcvars, VULKAN_SDK, EMSDK with Chromium), so the sweep is done from memory.

**Evidence.**

- CMakePresets.json (12 configure/test presets, no workflowPresets)
- CMakeLists.txt:105-125
- .github/workflows/ci.yml:387-394
- CLAUDE.md 'There are five render backends'

**Scope check.** Mild T1 tension if built as a framework. workflowPresets or one script over existing configurations serves a present caller, so it is not speculative.

**Adversarial review.**

- severity major -> minor: CI's matrix (.github/workflows/ci.yml:96-111, 284-297, 392) builds and tests all five debug render presets and wasm-release, plus a null-backend wasm configure, on every PR. That matrix is the cross-configuration driver for compile and headless coverage. The configurations that really go unchecked are G31's, so this gap is a convenience gap on top of G31.

<a id="g31"></a>
### G31. GL and Vulkan pixel contracts, release builds and wasm-debug are outside CI, and local runs leave no record

**major** · verification-debt · E12 · journeys: sm-land-an-engine-change

CI skips RenderPixelTests on gl and vulkan, with the stated reason that the runner has no GPU. It builds no release preset for d3d12, gl, vulkan or null, and no wasm-debug. The only evidence that these passed for a commit is prose a ledger chooses to record, so 'green on every configuration' cannot be checked after the fact.

**Evidence.**

- .github/workflows/ci.yml:33-40, 58-66, 95-113, 284-297
- docs/review/2026-09-26/FIXES.md

**Scope check.** A GPU runner costs money the persona bounds. A recorded local-run log per commit is the cheaper closure, and it conflicts with no T-number.

<a id="g32"></a>
### G32. The offline tooling tests are not a gate, and the review's recommendation to gate them has no ledger row

**minor** · verification-debt · E12 · journeys: sm-land-an-engine-change

tools/tests holds three Python suites (50 tests per FIXES.md), and neither ctest nor CI runs them. The command is documented only in FIXES.md:106. The 2026-09-26 README's priority 3 asks CI to run them, but STATUS.md tracks only R26 findings, so the ledger reads as complete while this stays open.

**Evidence.**

- ls tools/tests
- grep tools/tests|unittest|pytest in CMakeLists.txt, *.cmake, *.yml -> no match
- docs/review/2026-09-26/README.md:465-468
- docs/review/2026-09-26/STATUS.md:48-51
- docs/review/2026-09-26/FIXES.md:106

**Scope check.** No conflict. It is one CI step using standard-library unittest, which adds no supply chain.

**Adversarial review.**

- severity major -> minor: Confirmed: tools/tests holds test_benchmark_content.py, test_cloud_performance.py and test_trace_benchmark.py, and grep finds no 'tools/tests' or 'unittest' in .github, CMakeLists.txt or cmake/. But these suites test offline performance-lane tooling that no persona's product journey touches. The impact is limited to the maintainer's EC2 lane.

<a id="g33"></a>
### G33. The page hosts actually serve (the bundle's index.html) has no automated test

**major** · verification-debt · E09 · journeys: bdp-desktop-click-and-play, bdp-phone-visit, bdp-when-it-goes-wrong

LineSweeperPageTests and every other *PageTests entry load the development page, shell.html, and RenderContextLossTests uses a test shell. No ctest or CI step loads samples/linesweeper/web/index.html, the page a visitor gets. Nothing drives createLineSweeper through mount(), quit and Play again, the coarse-pointer gate and its no-wasm-fetch guarantee, the keydown restore, the import/instantiate catch, onError rendering, the onContextLost/onContextRestored status text, or 'Try again'. One manual headless run found a real defect (the overlay sat under the old canvas). run_page.mjs already serves pages, emulates touch and has --hide-extension, so it could serve web-dist/linesweeper/ and force a loss with WEBGL_lose_context.

**Evidence.**

- CMakeLists.txt:141-180 (labrador_sample_page_test --page $<TARGET_FILE:LineSweeperSample>)
- tests/render/CMakeLists.txt:234-271
- .github/workflows/ci.yml:349-360 (bundle uploaded, not exercised)
- docs/port/web.md:1474-1488
- samples/linesweeper/web/index.html:106-170
- samples/linesweeper/CMakeLists.txt:121-140

**Scope check.** No conflict. It extends the *PageTests pattern, following CLAUDE.md's rule that a broken content path fails ctest rather than the live site. No engine API is added.

<a id="g34"></a>
### G34. Phone play is gated pending real-device checks, and phone performance is unmeasured

**major** · verification-debt · E09 · journeys: bdp-phone-visit, sm-close-hardware-and-deploy-milestones

The shipped module already links touch controls and a portrait arrangement, but index.html hides Play on coarse pointers until a real iPhone and a real Android phone have played the development page. Lifting the gate also means porting shell.html's phone handling into index.html: dropping the fixed 16:9 in portrait, adding viewport-fit=cover and safe-area padding, and adding the full-screen button. R6 (the top-out frame on a phone) and the S3TC-less path's legibility, load time and memory have run only in headless Chrome or SwiftShader. No milestone covers switching the host page to playable-on-phone once M7b closes, and web.md says phones are 'where most visitors are'.

**Evidence.**

- samples/linesweeper/web/index.html:29-36, 106-115
- docs/port/web.md:738-766, 821 (D14), 1522 (M7b), 1529
- docs/port/web-to-be-tested.md:63-73, 94-95
- samples/linesweeper/README.md:549-550

**Scope check.** No conflict. D5 makes phones a planned phase. Closing it needs hardware plus a page edit, and the gate is T6 honesty while it stays open.

<a id="g35"></a>
### G35. LineSweeper neither pauses nor says so when the canvas loses the keyboard

**minor** · missing-capability · E09 · journeys: bdp-desktop-click-and-play, bdp-when-it-goes-wrong

Inferred from code; no browser run is recorded. LineSweeper overrides neither on_deactivated nor on_activated. Hiding the tab freezes the match, which resumes immediately on return, mid-fall. Clicking elsewhere on the host page blurs the canvas: keys are dropped (application.cpp:416-425) while the match keeps running. Neither the canvas nor index.html says that a click on the game gives the keys back. The engine already delivers presence changes (settle_presence -> notify_activation), and samples/audio uses the hooks.

**Evidence.**

- engine/app/application.cpp:360-425
- engine/app/application.h:376-393
- engine/core/state.h:80-91
- engine/app/web/window.cpp:376-380
- samples/audio/audio_state.cpp:108-114
- grep on_activated|on_deactivated samples/linesweeper -> none
- samples/linesweeper/web/index.html:75-76, 153-161
- docs/port/web.md:295-300, 463-467

**Scope check.** No T1 conflict, because it uses an existing hook. There is tension with samples/linesweeper/README.md:243-249 on the pause screen, which must be read and amended with any change. web.md:295-300's decision that pads follow the page, not the canvas, must be kept.

**Adversarial review.**

- severity major -> minor: Inferred from code, with no browser run, and the cost is a dropped piece in a casual demo. One correction to the gap: on_keyboard_lost (engine/app/application.cpp:416-420) calls only set_input_focus and does not reach notify_activation, so on_deactivated would not hear a canvas blur. Keyboard::just_unfocused() (engine/input/keyboard.h:261-263) does see it, so closure still needs no new engine API, but through Keyboard and not through the state hook the gap names.
- severity minor -> minor: The gap is inferred from code with no browser run recorded, and it affects only the tertiary browser-demo-player. A hidden tab freezes the match, so the game is not lost. Losing keys after a click elsewhere is recoverable by clicking the canvas. That is friction rather than a failed journey.

<a id="g36"></a>
### G36. Concept pages make false claims about what the samples do

**major** · doc-drift · E06 · journeys: srl-learn-the-engine-model

drawing.mdx:51-53 says both samples use the identity DEFAULT_CAMERA with world coordinates equal to window pixels. LineSweeper, collision and local_multiplayer (per player) use Camera::frame. resources.mdx:41 and states.mdx:114 still count two samples, and resources.mdx:41 leaves out that browser builds embed content rather than copying it. The build-time quote check cannot catch this, because the quotes still resolve and only the prose around them is wrong. The pages teach a wrong model.

**Evidence.**

- website/src/content/docs/docs/concepts/drawing.mdx:51-53
- samples/linesweeper/presentation/arrangement.cpp:27
- samples/linesweeper/states/play_state.cpp:214-215
- samples/collision/collision_state.cpp:112-115
- samples/local_multiplayer/play_state.cpp:60
- website/src/content/docs/docs/concepts/resources.mdx:41
- website/src/content/docs/docs/concepts/states.mdx:114
- samples/audio/CMakeLists.txt:23-25

**Scope check.** No conflict. It corrects site prose.

<a id="g37"></a>
### G37. Site reference and generated manifest describe master, not the reader's pin

**minor** · doc-drift · E10 · journeys: oca-start-project-first-build, pi-consume-library-only, oca-take-engine-upgrade

ConsumerManifest.astro derives the consumer's vcpkg.json, including its builtin-baseline, from the checkout the site was built from. The page says that baseline 'pins them to the versions the engine is built and tested with'. A reader at another pin may get a mismatched dependency set, with no instruction to compare against external/labrador/vcpkg.json. The site's API reference likewise documents master only, and nothing says the reader's own external/labrador headers are the reference that matches their pin. ColourWars shows the drift in practice: a manifest still named 'artattack', listing an unused doctest and missing the platform qualifier.

**Evidence.**

- website/src/components/ConsumerManifest.astro:1-29
- website/src/content/docs/docs/get-started/your-project.mdx:88-92
- website/src/components/Revision.astro:6-10
- docs/website-versioning.md:3-19
- C:\VSProjects\ColourWars\vcpkg.json; ColourWars 8b12576

**Scope check.** No conflict if closed by a note or a configure-time comparison. A package-manager abstraction would be speculative (T1). The full versioned-docs archive is deliberately deferred until releases exist (website-versioning.md:13-19).

<a id="g38"></a>
### G38. No engine-side instruction for running the engine's own suite at a consumer's pin

**minor** · onboarding · E02 · journeys: pi-consume-library-only, oca-take-engine-upgrade

The README and the website say only that the tests are left out of a consumer's build. The recipe for configuring the submodule as its own top-level project into a separate binary dir exists only in ColourWars' CLAUDE.md, and no walk session ran it. An outside consumer checking a pin bump has no engine-side instruction.

**Evidence.**

- README.md:166-169
- website/src/content/docs/docs/get-started/your-project.mdx:119-122
- C:\VSProjects\ColourWars\CLAUDE.md:108-115

**Scope check.** No conflict. It is documentation only. Keeping engine tests out of the consumer ctest stays (CMakeLists.txt:42-46).

<a id="g39"></a>
### G39. The template's browser files are copied, edited and never compiled, with no word on them

**minor** · onboarding · E08 · journeys: get-started-your-project, oca-start-project-first-build

The xcopy in your-project.mdx:60-62 copies main_web.cpp and web/shell.html into game/. The page's FileTree does not show them, and the replacement game/CMakeLists.txt has no Emscripten branch. The include-change table still asks the reader to edit main_web.cpp, which never compiles. The adopter ends up committing dead files without knowing whether to keep them. This was observed in the walkthrough commit a18f4cc.

**Evidence.**

- website/src/content/docs/docs/get-started/your-project.mdx:22-35, 58-77
- website/src/walkthrough/game-CMakeLists.txt:1-5
- website/src/walkthrough/CMakePresets.json:13-22
- samples/minimal/CMakeLists.txt:18-36
- walkthrough commit a18f4cc (game/main_web.cpp, game/web/shell.html)

**Scope check.** No conflict. Saying the browser build is left out and the files can be deleted is the T3-sized fix. Restoring the build is G09.

<a id="g40"></a>
### G40. The template's comments point at private ColourWars code

**minor** · doc-drift · E02 · journeys: oca-start-project-first-build, oca-evaluate-and-decide

After copying, the reader's own game/states/hello_state.cpp says a second draw argument is shown in 'game/objects/level.cpp'. That reads like a folder in their own project, but the file is in a private repository. 'paint tiles' (hello_state.cpp:214) is a second ColourWars reference. samples/minimal/CMakeLists.txt, which why.mdx embeds in full as proof, names 'the paint-shooter' and 'game/' (lines 9-12, 48). first-change.mdx recommends these comments as the next thing to read.

**Evidence.**

- samples/minimal/states/hello_state.cpp:211-214, 241-243
- samples/minimal/CMakeLists.txt:9-12, 48
- website/src/content/docs/why.mdx:30-34
- README.md:14-16
- website/src/content/docs/docs/get-started/first-change.mdx:67-68

**Scope check.** No conflict. The fix is a comment edit that keeps the T1 rationale and points at public samples. The site quotes samples/ passages, so anchors must be fixed in the same commit.

<a id="g41"></a>
### G41. The 'dozen lines' build-file claim is wrong in size and silent on shipping

**minor** · doc-drift · E02 · journeys: oca-evaluate-and-decide, pi-ship-windows-release

Home says a game's build file is 'a dozen lines of CMake' (index.mdx:76-77), and samples/minimal/CMakeLists.txt and ARCHITECTURE.md:566 say 'a dozen honest lines'. why.mdx embeds the file, which is 64 lines with an Emscripten branch. The claim also covers only building and dev-loop content mirroring, while a releasable package took ColourWars over 130 more lines (see G14). Rated minor because the substantive problem is G14 and this is the claim's wording.

**Evidence.**

- website/src/content/docs/index.mdx:76-77
- website/src/content/docs/why.mdx:30-34
- samples/minimal/CMakeLists.txt:3-5, 20-36
- docs/design/ARCHITECTURE.md:564-568
- ColourWars cmake/release_package.cmake + release_assets.cmake (139 lines)

**Scope check.** No conflict. docs/design changes by amendment, and either a shipping section or a reworded claim is allowed.

<a id="g42"></a>
### G42. The template's CMakeLists says content resolves against the working directory

**minor** · doc-drift · E02 · journeys: oca-start-project-first-build, oca-ship-desktop-and-browser

samples/minimal/CMakeLists.txt:47 says 'Content is resolved relative to the working directory'. Since 92966b0 the engine resolves relative content paths against the executable's directory (GetModuleFileNameW), as application.h, content_root.h, run-minimal.mdx, troubleshooting.mdx and the walkthrough's own copy all say. An adopter reading the file they copy learns the wrong rule, which matters for shortcuts and launchers.

**Evidence.**

- samples/minimal/CMakeLists.txt:47-50
- engine/app/application.h:140-145
- engine/app/content_root.h:11
- engine/app/content_root.cpp:24-43
- website/src/content/docs/docs/troubleshooting.mdx:66-68
- website/src/walkthrough/game-CMakeLists.txt:15

**Scope check.** No conflict. It is a one-line comment fix under CONVENTIONS' contract-or-rationale rule.

<a id="g43"></a>
### G43. The template has no README, so it has neither copy instructions nor a change signal

**minor** · onboarding · E02 · journeys: oca-start-project-first-build, oca-take-engine-upgrade

samples/minimal is the new-project template but has no README. Its only in-tree guidance is main.cpp:1-3. The include-root rewrite, the consumer vcpkg.json, the presets and labrador_settings live only on the website, so someone arriving by clone or submodule finds nothing to follow. The template has changed eight times, three of them since 2026-09-17, and nothing tells a copier what changed or whether any compatibility is expected. PHILOSOPHY disclaims compatibility only for components.

**Evidence.**

- ls samples/minimal (no README)
- samples/minimal/main.cpp:1-3
- website/src/content/docs/docs/get-started/your-project.mdx:58-122
- git log -- samples/minimal
- docs/design/PHILOSOPHY.md:720-733

**Scope check.** No conflict. A README that points to the website page avoids drift. The template is copied and owned (PHILOSOPHY.md:720-727), so a change note creates no compatibility promise.

<a id="g44"></a>
### G44. Design documents carry statements false even as target

**minor** · doc-drift · E01 · journeys: oca-evaluate-and-decide, srl-learn-the-engine-model, sm-land-an-engine-change

Target tense is disclosed (design/index.mdx:22-25), but some statements cannot be read as target. PHILOSOPHY.md:449 lists action mapping as engine-provided, against ARCHITECTURE.md:175-179 and CLAUDE.md 'Known-absent'. PHILOSOPHY.md:4 and :717 say the minimal sample lives beside the paint-shooter, which has been false since the split (PHILOSOPHY.md:229; ARCHITECTURE.md:28). ARCHITECTURE.md:132-133 says the presets are 'ten in all', but 12 exist. The site publishes all of these word for word.

**Evidence.**

- docs/design/PHILOSOPHY.md:4, 229, 449, 717
- docs/design/ARCHITECTURE.md:28, 132-133, 175-179
- CMakePresets.json
- website/src/content/docs/docs/design/index.mdx:22-25

**Scope check.** No conflict. Amendment in the same commit is how the target-tense convention works. The action-mapping row contradicts T1's own reasoning in ARCHITECTURE.

**Adversarial review.**

- severity minor -> minor: Severity unchanged, with one evidence correction. ARCHITECTURE.md:135-136 says 'debug and release for each of the five render backends — ten in all', and that is accurate for what it counts (10 x64 presets). Its fault is leaving out the two wasm presets, not miscounting. The PHILOSOPHY.md:4 and :717 paint-shooter statements and the :449 action-mapping row are confirmed as written.

<a id="g45"></a>
### G45. Only LineSweeper is hosted, so split-screen and the other samples cannot be tried without a Windows build

**minor** · missing-capability · E01 · journeys: oca-evaluate-and-decide, srl-learn-cpp-through-the-engine

local_multiplayer builds as a page and is CI-tested (LocalMultiplayerPageTests), as are all six samples. Only LineSweeper is hosted, so an evaluator cannot see the multi-view feature they care about, and a learner cannot watch the file they are reading run, without installing Visual Studio and vcpkg on Windows. The guide's run instructions are Windows-only.

**Evidence.**

- docs/port/web.md:1520 (M6 row)
- samples/local_multiplayer/main_web.cpp
- website/src/content/docs/docs/guides/local-multiplayer.mdx:17-26
- website/src/content/docs/docs/get-started/prerequisites.mdx:7
- website/src/content/docs/index.mdx:137

**Scope check.** Tension with docs/website-plan.md:90 ('Browser-playable demos are not a feature this site needs'). Closing this means hosting on mattharding.dev as with LineSweeper, or amending that decision. There is no T1-T12 conflict.

<a id="g46"></a>
### G46. Emscripten setup and its pinned version are not on the website

**minor** · onboarding · E08 · journeys: oca-ship-desktop-and-browser, anti-nms-recognise-early, srl-read-a-finished-game

examples.mdx:154-157 and guides/audio.mdx:29-34 tell readers to build under wasm-debug or wasm-release. The prerequisites page says everything comes 'with two installs' (Visual Studio and Git) and never mentions Emscripten or EMSDK, which the wasm presets read. The README says only 'with EMSDK set'. The known-good version (6.0.12) and the install commands exist only in CI, which has built the wasm job only on windows-latest.

**Evidence.**

- website/src/content/docs/docs/get-started/prerequisites.mdx:8-9
- website/src/content/docs/examples.mdx:154-157
- website/src/content/docs/docs/guides/audio.mdx:29-34
- README.md:103-110
- CMakePresets.json:106-108
- .github/workflows/ci.yml:242-243, 250, 265-270

**Scope check.** No conflict, since the browser is a supported target. It serves the primary persona more than the anti-persona's exit.

<a id="g47"></a>
### G47. Input guide gives no hardware caveat for the browser pad, touch or the phone layout

**minor** · verification-debt · E05 · journeys: oca-grow-a-game, anti-nms-recognise-early

The input guide presents gamepads with no platform caveat, but the browser Gamepad path has been driven only by a page script's fake pad. It also describes 'every finger on the glass', on-screen buttons and a layout 'for a phone held upright' without saying that only Chrome's emulation and Windows touch injection have driven these, and that phones are gated out of the hosted demo. A reader could take this as verified pad and phone support.

**Evidence.**

- website/src/content/docs/docs/guides/input.mdx:47-66, 124-155
- docs/port/web.md:738-752, 841, 921, 1518, 1520
- CLAUDE.md:151-152

**Scope check.** No conflict. It discloses existing verification debt, and closing the debt itself needs hardware.

<a id="g48"></a>
### G48. The null renderer is advertised but not usable from a consumer project

**minor** · onboarding · E03 · journeys: oca-grow-a-game

why.mdx sells a recording renderer for GPU-less tests, and the guides use the x64-*-null presets. A consumer is never told to set LABRADOR_RENDER_BACKEND=null (the default is d3d11), or that doing so means a second configure. The recording API they would assert on lives in engine/render/null/, which the API reference excludes by rule.

**Evidence.**

- website/src/content/docs/why.mdx:117-121
- website/src/walkthrough/CMakePresets.json
- engine/CMakeLists.txt:121-124
- website/src/lib/published.ts:76
- tests/render/null_tests.cpp:3

**Scope check.** Configure-time selection is T5 and stays. Publishing recording.h conflicts with 'backend folders hold no public API' (published.ts:76) unless it is argued as an exception. Documentation and a consumer null preset do not conflict.

<a id="g49"></a>
### G49. Collidable's no-op set_for_deletion silently defeats the bounds sweep

**minor** · api-ergonomics · E06 · journeys: oca-grow-a-game

CollisionObject::set_for_deletion defaults to doing nothing, and the scene's bounds sweep retires out-of-world objects only by calling it. A collidable that overrides only the required pure virtuals is never retired when it leaves the world. The collision guide never mentions for_deletion or set_for_deletion, and the sample's Body returns false unconditionally. A persona's projectile is never removed, and no message says why.

**Evidence.**

- engine/collision/collision_object.h:52-60
- engine/scene/scene.cpp:113-139
- samples/collision/body.cpp:94
- website/src/content/docs/docs/guides/collision.mdx:38-120
- website/src/content/docs/docs/concepts/tick.mdx:96-102

**Scope check.** Documenting it conflicts with nothing. Changing the default is a T3 argument and an API break for ColourWars, which needs weighing (see G19).

<a id="g50"></a>
### G50. The render backend default ignores the platform, unlike audio and input

**minor** · api-ergonomics · E08 · journeys: oca-ship-desktop-and-browser

LABRADOR_RENDER_BACKEND defaults to d3d11 even under Emscripten, and only an unknown name is refused. By contrast, audio refuses xaudio2 in a browser build and input picks web by default. An adopter whose browser preset omits the render backend gets an undocumented failure rather than T6's loud configure-time refusal. The exact failure was not observed.

**Evidence.**

- engine/CMakeLists.txt:121-122, 269-275, 307-313, 341-349

**Scope check.** No conflict. Defaulting to gl or refusing Direct3D/Vulkan under Emscripten matches T5, T6 and the other two axes.

<a id="g51"></a>
### G51. Hosting requirements live only in an internal port plan

**minor** · doc-drift · E08 · journeys: oca-ship-desktop-and-browser

What a host must send (.wasm as application/wasm, no COOP/COEP, 'wasm-unsafe-eval' under a CSP) is written only in web.md §7.2, for mattharding.dev's Worker. The public site says only 'serve the folder over HTTP'. A reusable host bundle (ES module, notices, BUILD.json) exists only for LineSweeper, and minimal's output is a development shell.

**Evidence.**

- docs/port/web.md:662-688
- website/src/content/docs/examples.mdx:154-157
- samples/linesweeper/CMakeLists.txt:119-151
- samples/minimal/CMakeLists.txt:31-34

**Scope check.** No conflict, as long as it stays guidance and not a hosting service.

<a id="g52"></a>
### G52. web.md §7.2 says a wrong wasm MIME type stops the demo, but the loader falls back

**minor** · doc-drift · E08 · journeys: bdp-when-it-goes-wrong

web.md §7.2 calls 'Content-Type: application/wasm' 'the one header the demo cannot start without'. The generated loader catches an instantiateStreaming failure and retries through ArrayBuffer instantiation, so a host that sends the wrong type still runs the game, with console errors and a slower start. The curl -I check is still worth doing, but the predicted failure is wrong.

**Evidence.**

- docs/port/web.md §7.2 first bullet
- out/build/wasm-debug/web-dist/linesweeper/linesweeper.js:557-568
- docs/port/web-to-be-tested.md:88-91

**Scope check.** No conflict. It is an amendment to a port document.

<a id="g53"></a>
### G53. The engine has no version identity a consumer can stamp

**minor** · release-process · E10 · journeys: pi-ship-windows-release

There are no tags, project() has no VERSION, and vcpkg.json's 0.1.0 has not changed across 45 commits. A consumer can trace a build to the engine only by running git inside the submodule, and a source archive loses the revision entirely. bundle.mjs stamps commit and dirty state for LineSweeper's web bundle but offers no equivalent (a CMake variable or generated header) to clients. ColourWars wrote its own.

**Evidence.**

- git tag -> empty
- CMakeLists.txt:6 (project without VERSION)
- vcpkg.json version-string 0.1.0
- tools/web_bundle/bundle.mjs:21-26, 123-147
- ColourWars cmake/write_build_info.cmake:3-29

**Scope check.** No conflict. A revision variable is small and serves two existing consumers, the web bundle and clients. It can be closed independently of G17.

<a id="g54"></a>
### G54. The engine keeps no public record of reports received, their disposition, or which commit answered them

**minor** · release-process · E10 · journeys: pi-report-a-gap-upstream, pi-move-the-pin

Answers to client reports exist only as commit messages (343433c names 'report 7'; f94ac81). No Labrador document lists requests received, answered or declined, or the T-reason for each. An integrator cannot see whether a gap was already declined, or cite a precedent. Each pin bump's closures have to be reconstructed by hand from engine commits: one report was 'Never filed. The engine had already fixed it in 881d3c7'. The only status record is the private client ledger.

**Evidence.**

- grep 'engine-reports|report [0-9]' docs/ engine/ -> no hits
- Labrador commits 343433c, f94ac81
- ColourWars docs/review/engine-reports.md:9, 380-546, 389, 658-705, 834-842
- ColourWars CLAUDE.md:149-156

**Scope check.** It must not become heavyweight process (T1). A labelled public issue tracker (G21), or a line in release notes, serves. The client's own ledger stays the client's by design.

<a id="g55"></a>
### G55. The GitHub front door points at the private client, not the site

**minor** · onboarding · E01 · journeys: oca-evaluate-and-decide

The GitHub About description names the private paint-shooter as the proof that the engine works. The homepage field is empty and there are no topics. README.md:7-16 also opens on the private client. A stranger arriving from GitHub, which is this journey's trigger, reads first about something they cannot see, and gets no sidebar link to the site.

**Evidence.**

- gh api repos/matt439/Labrador -> description '...a paint-shooter in its own repository to prove it works', homepage null, topics []
- README.md:7-16

**Scope check.** No conflict. T1's client-as-proof argument can stay in the README body.

<a id="g56"></a>
### G56. First-change's 'Where to go next' does not link the guides

**minor** · onboarding · E02 · journeys: oca-grow-a-game

The handoff from Get Started to growing a game offers Examples, Why, API reference and Design, but not the Guides, which are what that journey needs. They are reachable only from the sidebar or examples.mdx's table.

**Evidence.**

- website/src/content/docs/docs/get-started/first-change.mdx:65-75
- website/src/content/docs/examples.mdx:105-109

**Scope check.** No conflict.

<a id="g57"></a>
### G57. The engine site has no site URL, so canonical URLs and the sitemap are unset

**minor** · onboarding · E01 · journeys: sm-close-hardware-and-deploy-milestones, anti-recognise-not-for-me

The site has been live since 8 October 2026, and astro.config.ts has no 'site' key. The website README carries the fix as an open follow-up. Canonical URLs and the sitemap depend on it, which affects outside adopters finding the docs.

**Evidence.**

- website/README.md:4, 214-218
- website/astro.config.ts (no 'site:')
- docs/website-plan.md:122

**Scope check.** No conflict.

<a id="g58"></a>
### G58. The VCPKG_ROOT fallback example names only VS 2022

**minor** · doc-drift · E02 · journeys: get-started-prerequisites

prerequisites.mdx:13 accepts 'Visual Studio 2022 or newer', but the fallback example at :43 is a ...\2022\Community\VC\vcpkg path. On Visual Studio 18 (2026) the folder is ...\18\Community\VC\vcpkg. vcvars set the variable during the walkthrough, but a reader who needs the fallback copies a path that does not exist.

**Evidence.**

- website/src/content/docs/docs/get-started/prerequisites.mdx:13, 43
- walkthrough check.bat output: VCPKG_ROOT=C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg

**Scope check.** No conflict; this is a wording fix.

<a id="g60"></a>
### G60. Nothing warns that the vcpkg manifest name has stricter rules than 'use your own name'

**minor** · onboarding · E02 · journeys: get-started-your-project

your-project.mdx:18 says to use your own name wherever 'mygame' appears, which includes the generated vcpkg.json 'name'. vcpkg restricts that field to lowercase letters, digits and hyphens, while the walkthrough's CMake project is 'MyGame'. A CamelCase name in the manifest would probably fail configure. This was not exercised; the risk comes from vcpkg's documented rule.

**Evidence.**

- website/src/content/docs/docs/get-started/your-project.mdx:18, 88
- website/src/components/ConsumerManifest.astro:24-29
- website/src/walkthrough/root-CMakeLists.txt:2

**Scope check.** No conflict; this is a one-sentence fix.

<a id="g61"></a>
### G61. The first-run vcpkg cost and the public-clone path were not observed

**minor** · verification-debt · E02 · journeys: get-started-build-labrador

build-labrador.mdx:30-32 says the first configure takes 'several minutes'. In the walkthrough, vcpkg restored all 7 packages from an existing local binary cache in 129 ms, and the clone came from a local path. So a stranger's first-run time, any build-from-source failures (such as DirectXTK) and the claim that it was checked 'from the public repository exactly as the page describes' are unconfirmed.

**Evidence.**

- scratchpad/engine_configure.log:22 'Restored 7 package(s) ... in 129 ms'
- website/src/content/docs/docs/get-started/build-labrador.mdx:30-32
- website/src/content/docs/docs/get-started/index.mdx:25-28

**Scope check.** No conflict. It needs a clean cache (VCPKG_DEFAULT_BINARY_CACHE override) or a CI job such as G08's, not a code change.

<a id="g62"></a>
### G62. The only bench result analyser is hard-wired to the EC2 NVIDIA host

**minor** · missing-capability · E11 · journeys: sm-measure-the-low-tier

Lock detection, pooled summaries and host attestation live in tools/cloud_performance/analysis.py. It raises unless nvidia-smi identity, an NVIDIA display and the configured NVIDIA vendor id are present. A local Radeon run produces raw JSON, but no tool applies the ratchet's presentation-lock verdict to it.

**Evidence.**

- tools/cloud_performance/analysis.py:254-259, 364-397, 455-456
- tools/cloud_performance/config.example.json:50-62

**Scope check.** No conflict. T1 argues for factoring out only the lock-verdict function rather than building a general local lane.

<a id="g63"></a>
### G63. No local run procedure for LineSweeperFrameBench

**minor** · onboarding · E11 · journeys: sm-measure-the-low-tier

The bench's run procedure is written for the EC2 lane. Nothing gives the local command line for a desktop measurement: the required --output, --run and --release-hash, the sample count, the pacing choice, and one build per backend preset, with D3D12 and Vulkan at minimum for the pipeline-depth claims.

**Evidence.**

- bench/linesweeper_frame_result.cpp:236-240
- bench/CMakeLists.txt:45-86
- tools/cloud_performance/README.md:328-331
- samples/linesweeper/README.md:561-566

**Scope check.** No conflict.

<a id="g64"></a>
### G64. STATUS.md's low-tier box describes a README contradiction that no longer exists

**minor** · doc-drift · E11 · journeys: sm-measure-the-low-tier

The box says samples/linesweeper/README.md 'contradicts itself about the reference machine, 18 lines apart'. The two current passages are 93 lines apart and agree. STATUS.md is the one file kept current, so the box overstates what is open and sends the maintainer looking for an edit that has already been made.

**Evidence.**

- docs/review/backend-equivalence-2/STATUS.md:528-535
- samples/linesweeper/README.md:461-463, 554-566

**Scope check.** No conflict; it is the owner's own status-companion mechanism.

<a id="g65"></a>
### G65. The branch, PR and rebase-merge procedure exists only outside the tree

**minor** · release-process · E12 · journeys: sm-land-an-engine-change

In practice changes go through a branch, a PR and a rebase-merge on green CI, with master protected (PR #3; ci.yml triggers on pull_request). No tracked file states this: there is no CONTRIBUTING.md, and nothing in CLAUDE.md, AGENTS.md, README or docs. Another agent or contributor cannot learn it from the repository.

**Evidence.**

- gh pr view 3 --json commits
- git log 66a63f9 d169ea9 0977112
- .github/workflows/ci.yml:12-17
- grep rebase|'pull request'|'branch protection' CLAUDE.md README.md docs/ -> no match

**Scope check.** No conflict. It is one paragraph and could share a CONTRIBUTING file with G21.

<a id="g66"></a>
### G66. The agent brief is duplicated in CLAUDE.md and AGENTS.md with no sync check

**minor** · other · E12 · journeys: sm-land-an-engine-change

The two 602-line files differ only in their title line. Nothing fails when they diverge, so every amendment has to be made twice, and drift goes unnoticed.

**Evidence.**

- CLAUDE.md:1
- AGENTS.md:1
- diff --strip-trailing-cr CLAUDE.md AGENTS.md -> line 1 only
- tools/cloud_performance/release.py:19

**Scope check.** No conflict. A check in the style of check_engine_includes, or having one file point to the other, is the simpler model (T3).

<a id="g67"></a>
### G67. Golden-image review leaves no trace

**minor** · verification-debt · E12 · journeys: sm-land-an-engine-change

CLAUDE.md requires that every regenerated golden image be looked at, but nothing records that this was done. The only artefacts are git's binary diff and CI's failure-only golden-actual upload.

**Evidence.**

- CLAUDE.md RenderPixelTests paragraph
- CI .github/workflows/ci.yml:202-211, 363-368

**Scope check.** No conflict. A commit-message convention or a ledger line closes it without tooling (T3).

<a id="g68"></a>
### G68. Nothing ties the hosted LineSweeper bundle to the engine commit it should match

**minor** · release-process · E12 · journeys: sm-close-hardware-and-deploy-milestones

The bundle in PersonalWebsite was built from Labrador e7888bd, 23 commits behind master. CI uploads a fresh linesweeper-web artifact on every run, but neither repository says when to re-sync the host or how to notice the live demo is stale.

**Evidence.**

- C:\VSProjects\PersonalWebsite public/demo/linesweeper/BUILD.json (commit e7888bd)
- git rev-list --count e7888bd..master = 23
- docs/port/web.md:1475-1479
- .github/workflows/ci.yml:349-360

**Scope check.** No conflict, as long as the fix is a documented step or check. Automated cross-repo deploys would fight web.md §7 and T1.

<a id="g69"></a>
### G69. web.md §9 says M3 is met in Firefox on a single manual LineSweeper pass

**minor** · doc-drift · E11 · journeys: sm-close-hardware-and-deploy-milestones

web.md:839 says M3 'is met in Chrome and Firefox'. M3's done-when is end-to-end play, and the Firefox evidence is one hand-run headless LineSweeper check. The checklist still owes the other pages and the shell CSS in Firefox. Its Firefox list omits the components gallery, and it still says 'five *PageTests' where there are six.

**Evidence.**

- docs/port/web.md:839, 1045-1049, 1517
- docs/port/web-to-be-tested.md:12-14, 29-30, 102-106

**Scope check.** No conflict.

<a id="g70"></a>
### G70. No automated non-Chromium browser run

**minor** · missing-capability · E11 · journeys: sm-close-hardware-and-deploy-milestones

run_page.mjs drives only CDP, so every page test, WebWindowTests and the pixel pages run only in Chromium. Firefox can be checked only by hand and Safari not at all, so a Firefox regression is found manually if it is found at all.

**Evidence.**

- docs/port/web-to-be-tested.md:102-106
- tests/render/web/run_page.mjs (CDP only)

**Scope check.** Possible T1 tension: a WebDriver BiDi path is a second runner protocol and must be argued on a regression it would have caught.

<a id="g71"></a>
### G71. No documented way to put a development page on a phone

**minor** · onboarding · E09 · journeys: sm-close-hardware-and-deploy-milestones

The M7b and audible-audio checks need the development page served to a device on the network. The only serve recipe in the tree is a local 'python -m http.server' for the audio sample. Nothing covers binding to the LAN, secure-context requirements, or remote-debugging a phone's console, which the page checks rely on to catch errors.

**Evidence.**

- samples/audio/README.md:69-72
- docs/port/web-to-be-tested.md:63-73

**Scope check.** No conflict; this is documentation only.

<a id="g72"></a>
### G72. The null-backends-still-red CI check needs a deliberate failure nobody has scripted

**minor** · verification-debt · E12 · journeys: sm-close-hardware-and-deploy-milestones

The open push item cannot be ticked until a page step fails. The condition exists (ci.yml:388), but nothing describes how to provoke a failure on a throwaway branch to observe it, so the box stays open indefinitely.

**Evidence.**

- docs/port/web-to-be-tested.md:15-16
- .github/workflows/ci.yml:388

**Scope check.** No conflict.

<a id="g73"></a>
### G73. The client pin lags master by 45 commits during active client development

**minor** · verification-debt · E10 · journeys: sm-evolve-api-and-move-the-client

ColourWars committed on 2026-10-12 and 2026-10-13 without moving its pin, which sits 45 commits behind master, before the whole browser port and the components sample. Each bump is validated by hand in prose; the last records 'Gameplay remains unverified by this check'. The longer the lag, the larger the next migration.

**Evidence.**

- git -C C:\VSProjects\ColourWars submodule status (4cefb68)
- git rev-list --count 4cefb68..master = 45
- ColourWars 53bf172, 2c5703f, b2daa72
- ColourWars fa513d4 engine-reports.md diff

**Scope check.** A check driven from Labrador conflicts with PHILOSOPHY.md:748-750. The closure belongs on the client side (a ColourWars job against master, or a cadence), outside this repository. It is recorded because the owner holds both roles.

<a id="g74"></a>
### G74. Exhibit-backed client reports sit unfiled, the packaging gap among them

**minor** · other · E10 · journeys: pi-report-a-gap-upstream, pi-ship-windows-release

Seven reports (6, 9, 10, 11, 12, 13 and 14) are drafts in the client's ledger. Two were rechecked at master and still hold. Report 10: read_back_buffer is usable only between submit and end_frame, with no client hook. Report 11: F10 and a lone Alt fall through to DefWindowProc with no SC_KEYMENU opt-out. Each keeps a client workaround alive: a second full draw for screenshots, and a WH_GETMESSAGE hook over the engine's pump. The Windows packaging gap (gap-audit F8) was never drafted at all, although release_package.cmake is a ready exhibit. The fix is to file each, or to decline each upstream with a reason.

**Evidence.**

- ColourWars docs/review/engine-reports.md:9, 222-283 (no packaging report)
- ColourWars docs/review/gap-audit.md F8
- engine/render/renderer.h:405-417
- engine/app/application.cpp:325-328
- engine/app/application.h:41-72
- engine/app/win32/window.cpp:680-704, 940-950

**Scope check.** Each ask is weighed on its own. read_back_buffer is 'NOT A FRAME-PATH CALL' (T8), so a capture hook must stay out-of-band. Reports 6 (audio bus) and 12 (JSON write) touch the read-only asymmetry ARCHITECTURE and T12 settle. The gap is closed by deciding, not necessarily by implementing.

<a id="g75"></a>
### G75. Engine-private rapidjson pushes clients to vendor a second copy, and no doc says so

**minor** · api-ergonomics · E06 · journeys: pi-consume-library-only

rapidjson is deliberately PRIVATE to LabradorEngine. A client that parses its own files has to bring its own: ColourWars carries a byte-identical vendored copy that can drift from the engine's. No consumer doc says a client must supply its own parser. Only that documentation half is the gap.

**Evidence.**

- engine/CMakeLists.txt:365-377
- C:\VSProjects\ColourWars\game\CMakeLists.txt:128-132; editor/CMakeLists.txt:3-4
- diff -rq Labrador/external/rapidjson ColourWars/external/rapidjson -> identical

**Scope check.** Making rapidjson PUBLIC would undo a stated decision (no engine header names the parser, as with DirectXTK at engine/CMakeLists.txt:378-389). Only documentation is in scope.

<a id="g76"></a>
### G76. The game-header include wall recognises only a folder literally named game/

**minor** · other · E12 · journeys: pi-consume-library-only

check_engine_includes matches only '#include "game/..."' or '../game/...'. A consumer who follows the README's own 'yourgame/' example or the website's 'mygame' naming gets no protection. The practical risk is low, because it matters only if engine files are edited inside a client's submodule. Still, the comment's claim that the wall 'does not depend on who is building' is narrower than it reads.

**Evidence.**

- cmake/check_engine_includes.cmake:15-36
- README.md:178-181
- CMakeLists.txt:13-19

**Scope check.** No conflict. Under T1 the simpler fix is to reword the comment and leave the regex.

<a id="g77"></a>
### G77. The shipped Windows ZIP is unverified on a clean machine, on target GPUs and at the current engine

**minor** · verification-debt · E07 · journeys: pi-ship-windows-release

ColourWars CI proves only that the extracted ZIP starts and shuts down on a runner with VS installed and a reduced PATH, and Release may skip when there is no hardware adapter. Nothing records a run on Windows 10 or 11 without VS, on the low-tier GPU, or with gameplay. Packaging at Labrador master, 45 commits past the pin with engine/CMakeLists.txt and vcpkg.json changed, is unverified.

**Evidence.**

- ColourWars .github/workflows/ci.yml:108-126
- ColourWars docs/review/engine-reports.md:847-860
- git diff --stat 4cefb68..0977112 -- engine/CMakeLists.txt vcpkg.json cmake/

**Scope check.** No conflict. The low-tier positioning argues for it. It is related to G15 but can be closed independently.

<a id="g78"></a>
### G78. Hidden launch through STARTUPINFO is argued but never executed by a test

**minor** · verification-debt · E07 · journeys: pi-move-the-pin

Since D2 the engine decides the first ShowWindow command, relying on SW_SHOWDEFAULT honouring the launcher's STARTUPINFO. A consumer whose CI launches with Start-Process -WindowStyle Hidden depends on that, as ColourWars' check-startup.ps1 does. In-tree callers use ApplicationOptions::visible instead, and no AppTests case launches a child process.

**Evidence.**

- engine/app/win32/window.cpp:361-368
- git show 4cefb68:engine/app/window.cpp:237-238
- docs/port/web.md:779, 797
- ColourWars scripts/check-startup.ps1

**Scope check.** No conflict. A behavioural test for a public contract is what CLAUDE.md asks for, and it adds no API.

<a id="g79"></a>
### G79. No guidance for driving a consumer's gameplay with injected input in a smoke run

**minor** · verification-debt · E03 · journeys: pi-move-the-pin

After a bump the client can prove the game builds and the title screen renders, but not that gameplay works. Synthetic key presses did not advance the title, which the ledger attributes to press-then-later-release sampling rather than a regression. The engine documents no way for a client to inject input that survives a frame's sampling, though its own browser pages do this through window_input.mjs.

**Evidence.**

- ColourWars docs/review/engine-reports.md:850-862
- tests/app/web/window_input.mjs (CLAUDE.md WebWindowTests paragraph)

**Scope check.** Tension with T1 if this grew into a client replay or test framework. Guidance stays in scope, and much of the fix may belong to the client.

<a id="g80"></a>
### G80. Controls are stated incompletely and inconsistently, and phones see keyboard instructions

**minor** · onboarding · E09 · journeys: bdp-desktop-click-and-play, bdp-phone-visit

The page says 'Up or X rotates', and the game says 'Z X rotate'. Only the page mentions Escape. Neither surface mentions Down (soft drop), Shift or the left shoulder (hold), or P and Start (pause), and the page gives no pad controls although the module reads a pad. On a coarse-pointer device the status line still lists arrow keys, Space, C and Escape, under a column-width 16:9 poster on an upright screen.

**Evidence.**

- samples/linesweeper/states/play_state.cpp:32-37, 55-65, 79-89, 375-378
- samples/linesweeper/web/index.html:51, 75-76, 108-115

**Scope check.** No T1 conflict, because this is text and not a rebinding layer. Changing the hint strings changes the capture images (play_state.cpp:32-33), which must be regenerated and reviewed.

<a id="g81"></a>
### G81. The phone gate promises a controller but only a key lifts it

**minor** · other · E09 · journeys: bdp-phone-visit

The coarse-pointer sentence says the page 'asks for a keyboard or a controller', but the only listener that restores Play is a one-shot keydown. A phone visitor with a Bluetooth pad has no way through, although input/web/ would play it.

**Evidence.**

- samples/linesweeper/web/index.html:71-72, 108-115
- grep gamepad samples/linesweeper/web/ -> none

**Scope check.** No conflict. It is a page listener or a wording change.

<a id="g82"></a>
### G82. Failure messages are developer-voiced and give the player nothing to do

**minor** · other · E09 · journeys: bdp-when-it-goes-wrong

Load failures show raw browser or Emscripten text, such as 'Aborted(...)'. The WebGL2 failure explains the cause but suggests no action, and mid-game failures say things like 'see Application::run'. 'Try again' is offered even for no-WebGL2, where a retry cannot succeed, and a game that never started is reported as 'stopped'.

**Evidence.**

- samples/linesweeper/web/index.html:148-152, 164-169
- engine/render/gl/web_context.cpp:316-321
- engine/app/web/window.cpp:340-352, 500-505
- out/build/wasm-debug/web-dist/linesweeper/linesweeper.js:448

**Scope check.** T6 is satisfied by the engine's worded reasons and must stay. Player-facing mapping belongs in index.html, which raises no conflict.

<a id="g83"></a>
### G83. A context that never comes back leaves a blank canvas with no way forward

**minor** · other · E09 · journeys: bdp-when-it-goes-wrong

By design (web.md §7.3), a WebGL2 context that is never restored leaves the canvas blank. index.html's 'waiting for them to come back' is styled as normal status rather than as an error, unlike shell.html's. It has no timeout, no 'Try again' and no reload, although the page already re-mounts a fresh canvas for Play again.

**Evidence.**

- samples/linesweeper/web/index.html:153-158
- samples/linesweeper/web/shell.html:138-147
- docs/port/web.md §7.3

**Scope check.** No conflict. It is page-level and leaves SEAM.md and DeviceNotify untouched.

<a id="g84"></a>
### G84. No capability check before download

**minor** · onboarding · E09 · journeys: bdp-when-it-goes-wrong

Inferred, not observed. index.html checks only the coarse-pointer gate before Play. A browser lacking wasm exception handling (-fwasm-exceptions) or WebGL2 fetches the module first and then fails, with raw compile-error text in the first case.

**Evidence.**

- samples/linesweeper/web/index.html:106-115, 164-169
- cmake/settings.cmake:75-80
- docs/port/web-to-be-tested.md:31-32

**Scope check.** No conflict if it stays in the host page and downloads nothing before the click (index.html:9).

<a id="g85"></a>
### G85. No best score survives a match or a reload

**minor** · missing-capability · E09 · journeys: bdp-desktop-click-and-play

Nothing in samples/linesweeper persists a score across a match, Play again or a reload. This is an observed absence, not a recorded decision.

**Evidence.**

- grep localStorage|high score samples/linesweeper -> none
- samples/linesweeper/README.md:17-21
- samples/linesweeper/web/index.html:15-20

**Scope check.** Likely conflicts with T1 if it grew into an engine persistence facility. A page-level best score would not. It should be decided in the sample README, not assumed wanted.

<a id="g86"></a>
### G86. The phone fallback's data cost is undecided

**minor** · onboarding · E09 · journeys: bdp-phone-visit

mattharding.dev's page is said to show phones a 4.3 MB muted looped recording, and whether that size is acceptable is an open box. Labrador's own site plays footage only on request. Nothing here says whether the demo's video autoplays or preloads, so 'without a wasted download' cannot be confirmed.

**Evidence.**

- docs/port/web.md:758-765
- docs/port/web-to-be-tested.md:99-100
- docs/website-plan.md:106-111

**Scope check.** No conflict. It is the owner's decision on the other repository's page.

<a id="g88"></a>
### G88. The rules tests cannot be built without configuring the whole engine with vcpkg on Windows

**minor** · onboarding · E03 · journeys: srl-read-a-finished-game

LineSweeperTests needs only the rules library, doctest and labrador_settings, but the only route to it is the root CMakeLists, which always adds engine/. Every preset requires VCPKG_ROOT. No page says whether a non-Windows host works, for example the wasm presets running tests under Node.

**Evidence.**

- tests/linesweeper/CMakeLists.txt:3-23
- CMakeLists.txt:40, 134
- CMakePresets.json:11
- website/src/content/docs/docs/get-started/prerequisites.mdx:8
- .github/workflows/ci.yml:27, 243

**Scope check.** A rules-only build entry point for a tertiary persona is in tension with T1. Documenting the existing path conflicts with nothing.

<a id="g89"></a>
### G89. Examples page understates LineSweeper's size

**minor** · doc-drift · E06 · journeys: srl-read-a-finished-game, srl-learn-cpp-through-the-engine

examples.mdx says LineSweeper is about 4,000 lines plus about 2,000 of tests. The tree has 4,784-4,850 lines of .h and .cpp (the two counts differ by whether web/ is excluded) and 2,470 lines of tests, so the reading budget is understated by about a fifth. personas.json repeats the stale figure.

**Evidence.**

- website/src/content/docs/examples.mdx:54-55
- find samples/linesweeper -name '*.h' -o -name '*.cpp' | xargs cat | wc -l -> 4784 (excl. web) / 4850
- tests/linesweeper -> 2470

**Scope check.** No conflict.

<a id="g90"></a>
### G90. LineSweeper README's 'Still open' presentation/ count is stale

**minor** · doc-drift · E06 · journeys: srl-read-a-finished-game

The layer-rule item says presentation/ 'has five files in it rather than three'. It holds 12 files (7 units), so the item understates how much an unchecked rule covers.

**Evidence.**

- samples/linesweeper/README.md:568-570
- ls samples/linesweeper/presentation (12 files)

**Scope check.** No conflict.

<a id="g91"></a>
### G91. LineSweeper README rests decisions on internal records a cold reader cannot use

**minor** · doc-drift · E06 · journeys: srl-read-a-finished-game

The README exists so a cold reader need not reconstruct decisions, yet several rest their reasons on unpublished internal records instead of stating them: docs/survey/2026-08-26.md, the status survey, docs/review/gpt6, and 'CLAUDE.md, Known-absent'. CLAUDE.md is an assistant notes file, not a design document.

**Evidence.**

- samples/linesweeper/README.md:3-6, 257, 362, 431, 453, 581-582

**Scope check.** No conflict, provided reasons are restated or re-pointed rather than deleted (CLAUDE.md: deleting a false sentence is not answering it). Citations must pass check_doc_citations.cmake.

<a id="g92"></a>
### G92. SCREAMING constants in published headers contradict CONVENTIONS, with no stated exception

**minor** · doc-drift · E06 · journeys: srl-learn-the-engine-model

CONVENTIONS sets constants to snake_case and forbids SCREAMING ones, but 26 engine-header constants are SCREAMING: Vector2F::ZERO, DIRECTION_*, PI, EPSILON, Camera::DEFAULT_CAMERA and ViewportManager::DIVIDER_THICKNESS among them. Concepts teaches DEFAULT_CAMERA by name, so a learner copying the engine's style cannot tell which is right. Re-rated from major to minor: it is a style contradiction a reader completes the journey through.

**Evidence.**

- docs/design/CONVENTIONS.md:23, 79-82, 204
- engine/render/colour.h:85
- engine/math/vector2f.h:133-142
- engine/math/scalar.h:8-9, 74
- engine/render/camera.h:86
- engine/render/viewport_manager.h:40
- engine/collision/resolve.h:31
- website/src/content/docs/docs/concepts/drawing.mdx:51

**Scope check.** Renaming breaks ColourWars (G19's problem). Amending CONVENTIONS is cheaper, and T12 (no invented dialect) argues for a rule that matches the code. The choice is the owner's.

<a id="g93"></a>
### G93. The API reference does not mark which headers are for game code and which for renderer writers

**minor** · onboarding · E06 · journeys: srl-learn-the-engine-model

published.ts lists resource_factory.h and throw_if_failed.h next to Label and Camera, with no audience field. reference/index.mdx:47-51 names some porter-facing headers but not these two, although resource_factory.h says a client never sees it.

**Evidence.**

- website/src/lib/published.ts:137-175
- website/src/content/docs/docs/reference/index.mdx:47-51
- engine/render/resource_factory.h:60-64
- engine/render/throw_if_failed.h:3, 10-12

**Scope check.** No conflict. The headers stay published (reference/index.mdx:48-50).

<a id="g94"></a>
### G94. Game-facing reference pages carry backend rationale with nothing marking it skippable

**minor** · doc-drift · E06 · journeys: srl-learn-the-engine-model

The RenderResources page publishes about 30 lines on D3D12 descriptor heaps, frames in flight and resource lifetimes under release_device_resources. Only the shell calls that function. A learner reading 'from the headers alone' has to sort contract from backend rationale unaided.

**Evidence.**

- engine/render/render_resources.h:69-122, 81-82
- website/src/content/docs/docs/reference/index.mdx:55-61
- docs/design/PHILOSOPHY.md:713-716

**Scope check.** Tension with CONVENTIONS (rationale belongs in comments) and with the ApogeeVGC relocation evidence. The precondition contract stays in the header, and only backend detail could move to SEAM.md or be marked shell-only.

<a id="g95"></a>
### G95. Concepts are gated on a Windows build the pages do not need

**minor** · onboarding · E06 · journeys: srl-learn-the-engine-model

concepts/index.mdx:11-13 assumes the reader has built and run minimal, which needs the Windows-only Get Started. The concept pages quote their source inline, so the gate is stricter than they need.

**Evidence.**

- website/src/content/docs/docs/concepts/index.mdx:11-13
- website/src/content/docs/docs/get-started/index.mdx:3, 8
- website/src/content/docs/docs/concepts/drawing.mdx:58

**Scope check.** No conflict. It is a wording fix, with a link to a hosted build once G45 exists.

<a id="g96"></a>
### G96. T10's 'teachable to a beginner' and Get Started's 'does not teach the language' are published unreconciled

**minor** · doc-drift · E01 · journeys: srl-learn-cpp-through-the-engine

The site publishes PHILOSOPHY's beginner yardstick, and a Get Started page that assumes C++ and declines to teach it. The design index routes newcomers to Get Started. Nothing tells a non-C++ reader where to learn first or that the on-ramp is planned.

**Evidence.**

- website/src/content/docs/docs/get-started/index.mdx:18-21
- website/src/content/docs/docs/design/index.mdx:31-33
- docs/design/PHILOSOPHY.md:179-180

**Scope check.** No conflict. The fix belongs on Get Started or a status note, not a rewrite of T10, since design docs are target-tense.

<a id="g97"></a>
### G97. The copy-me sample uses intermediate C++ and engine-internal vocabulary with no guide to either

**minor** · onboarding · E06 · journeys: srl-learn-cpp-through-the-engine

minimal's 560 lines use inheritance and overrides, unique_ptr beside borrowed raw pointers, a this-capturing lambda, std::span, a SAL-annotated wWinMain and using-directives. Its comments speak of deferred contexts, dynamic vertex buffers and fan-out. The site glosses only two of these features.

**Evidence.**

- samples/minimal/main.cpp:13-63
- samples/minimal/states/hello_state.h:51
- samples/minimal/states/hello_state.cpp:16-17, 144, 178-187
- website/src/content/docs/docs/get-started/first-change.mdx:30
- website/src/content/docs/docs/concepts/game.mdx:86

**Scope check.** Tension with the primary persona, who does not need language glosses, and with CONVENTIONS' rationale comments. Close it on the site, not by thinning the sample (T4, T10).

<a id="g98"></a>
### G98. T10's beginner-teachability claim has never been tested and has no recorded method

**minor** · verification-debt · E01 · journeys: srl-learn-cpp-through-the-engine

T10's acceptance test, a feature explained in an introductory series, has never run, and no other check is recorded: no beginner walkthrough and no surface audit. Published headers explain calls such as draw_sprite (eight positional parameters) in fan-out and worker terms.

**Evidence.**

- docs/design/PHILOSOPHY.md:734-738
- engine/render/renderer.h:155-171
- docs/review/round-2/all-findings.md:14662, 30068 (historical)

**Scope check.** No conflict. T10 says the fix for a failed test is simplifying the API. Depends on G22.

<a id="g99"></a>
### G99. Which exclusions are permanent is stated inconsistently

**minor** · doc-drift · E01 · journeys: anti-recognise-not-for-me

PHILOSOPHY's Non-goals says data files plus a text editor are 'the toolchain for now'. The home page says 'Not an editor' unqualified, but lists only online play and 3D as 'for good', and CLAUDE.md (unpublished) calls the editor exclusion permanent. why.mdx:24 says Labrador 'does not suit you yet', and :142 calls Godot and Unreal 'the better choice today', both suggesting gaps that may close. why.mdx:26 says 'Several of those are permanent' without naming them. The owner has to decide the editor's status first, and then why.mdx can name which exclusions are permanent.

**Evidence.**

- docs/design/PHILOSOPHY.md:781-792
- website/src/content/docs/index.mdx:99, 140
- website/src/content/docs/why.mdx:24-26, 142
- CLAUDE.md 'Known-absent, on purpose'
- docs/review/gpt6/README.md:345-347

**Scope check.** No conflict. It settles the wording of existing non-goals (T7, T10, T12) by amendment. It must not be closed by promising tooling.

<a id="g100"></a>
### G100. Neither the GitHub README nor the Get Started path states what Labrador is not

**minor** · onboarding · E01 · journeys: anti-recognise-not-for-me

The README has no exclusion list. The hero's Get Started button leads to a page with no link to Why, and the next page has the reader install Visual Studio. So reaching the exit before installing depends on which link the reader clicks first.

**Evidence.**

- README.md:1-52
- website/src/content/docs/index.mdx:9-14
- website/src/content/docs/docs/get-started/index.mdx:8-21
- website/src/content/docs/docs/get-started/prerequisites.mdx:7-13

**Scope check.** No conflict. It must stay a one-line pointer, not a disclaimer wall in front of the primary persona.

<a id="g101"></a>
### G101. No evidence whether the anti-personas arrive or leave before installing

**minor** · verification-debt · E01 · journeys: anti-recognise-not-for-me

The site is static with no analytics, so whether full-environment seekers arrive, and whether the exclusions turn them away, cannot be observed.

**Evidence.**

- docs/website-plan.md:90
- personas.json anti-full-environment-seeker assumptions

**Scope check.** Analytics would be a new third-party feature website-plan.md does not require. Accepting the gap is a valid closure.

<a id="g102"></a>
### G102. Published ARCHITECTURE states Vulkan's platform reach with no 'not built' qualifier

**minor** · doc-drift · E01 · journeys: anti-nms-recognise-early

ARCHITECTURE.md:465-467 says Vulkan 'reaches Android, Linux and, through MoltenVK, the Apple platforms', with only the site-wide target-tense warning as qualifier. PHILOSOPHY.md:395-404 at least says it changes the cost of the goal, not its status. A reader who lands on the page from search reads support.

**Evidence.**

- docs/design/ARCHITECTURE.md:463-470
- docs/design/PHILOSOPHY.md:395-404
- website/src/content/docs/docs/design/index.mdx:20-25

**Scope check.** Inserting status into docs/design would fight the target-tense rule. The fix belongs on the site side, or should be target-tense intent wording.

<a id="g103"></a>
### G103. The site gives no pointer to the platform plans or their scope

**minor** · doc-drift · E01 · journeys: anti-nms-recognise-early

why.mdx says the native platforms are 'not ... yet' but never links docs/port/. android.md says 'Nothing here has been built', defers iOS and stops short of APK packaging and store compliance. No Linux or macOS plan exists. A shipper cannot tell what 'yet' means.

**Evidence.**

- website/src/content/docs/why.mdx:191-198
- README.md:78-81
- docs/port/android.md:3-5, 622-642
- ls docs/port/

**Scope check.** No conflict, as long as no schedule is invented. The aim is an accurate turn-away.

<a id="g104"></a>
### G104. Configure on an unsupported host fails late and points nowhere

**minor** · onboarding · E02 · journeys: anti-nms-recognise-early

No preset has a hostSystemName condition, so on Linux or macOS the vcpkg toolchain runs and installs dependencies first. The first refusal is probably engine/CMakeLists.txt:103-107, which names no supported host, preset or document. Never run on a non-Windows host, so the first error is unverified.

**Evidence.**

- CMakePresets.json:1-45, 106-108
- cmake/settings.cmake:14-60
- engine/CMakeLists.txt:88-126

**Scope check.** No conflict. T5 favours an early, named refusal. It must not grow into a Linux build, which is unplanned.

<a id="g105"></a>
### G105. No stated hardware or OS floor a game built on Labrador needs on a player's machine

**minor** · platform · E07 · journeys: oca-ship-desktop-and-browser, pi-ship-windows-release

Each backend has a hard floor in code. d3d11 requests feature levels down to 10_0 (engine/render/d3d11/device_resources.cpp:119-120) and compiles shaders at 4_0_level_9_1 (engine/CMakeLists.txt:148-155). d3d12 needs shader model 5_1. GL needs a 3.3 core context through WGL. Vulkan refuses any device without timelineSemaphore (engine/render/vulkan/device_resources.cpp:620-626). No public page states these floors or a minimum Windows version. A grep of website/src/content/docs for 'feature level', 'GL 3.3' or 'Vulkan 1.2' in guides or troubleshooting finds nothing, and the only Windows version named is the one the guide was checked on (get-started/index.mdx:25). An adopter cannot write minimum requirements for their store page or say which backend to ship for older GPUs. This is separate from G24, which is about speed on the low tier, not about whether a device is accepted at all.

**Evidence.**

- engine/render/d3d11/device_resources.cpp:119-120
- engine/CMakeLists.txt:148-155, 167-174
- engine/render/vulkan/device_resources.cpp:620-626
- website/src/content/docs/docs/get-started/index.mdx:25
- grep -rniE 'feature level|GL 3\.3|Vulkan 1\.[23]' website/src/content/docs/docs/guides website/src/content/docs/docs/troubleshooting.mdx -> no matches

**Scope check.** No conflict. It documents floors the code already enforces, and T6 favours stating them. It adds no API and no backend.

**Answer, Windows half (2026-10-22).** The owner set the engine's Windows floor at Windows 10 version 1607, and the Win32 window imports what it needs from that release statically: `AdjustWindowRectExForDpi`, `GetDpiForWindow` and `GetDpiForSystem`, which make its client size hold in a per-monitor-aware process (ColourWars report 17, decided in docs/review/colourwars-reports/window.md). Below 1607 a game built on the engine fails at load, naming a missing user32 entry point. Nothing defines `_WIN32_WINNT`; the SDK's default declares the functions. Before this the de facto floor was Windows 8, from the static import of `GetPointerType`. CHANGES.md carries the floor as its own entry. The hardware floors, and the page OCA-64 asks for, are still owed.

<a id="g106"></a>
### G106. README says samples/minimal is a working example of the consumer setup, but minimal is not a consumer

**minor** · doc-drift · E02 · journeys: pi-consume-library-only, oca-start-project-first-build

README.md '## Using Labrador in a project' lists three consumer requirements: the PROJECT_IS_TOP_LEVEL exclusion, linking labrador_settings, and your own include root via target_include_directories. It then says 'samples/minimal is a working example of all three'. samples/minimal/CMakeLists.txt has no add_subdirectory, no PROJECT_IS_TOP_LEVEL and no target_include_directories, because it is built inside the engine tree. The website says as much: the include root is something 'the sample did not need because Labrador's root already was' (your-project.mdx:74-77). A reader who follows the README to minimal for the consumer shape finds none of it. This is separate from G07, which is about what the README omits, not about this false pointer.

**Evidence.**

- README.md:152-181 ('samples/minimal is a working example of all three')
- grep 'target_include_directories|add_subdirectory|PROJECT_IS_TOP_LEVEL' samples/minimal/CMakeLists.txt -> no matches
- website/src/content/docs/docs/get-started/your-project.mdx:74-77

**Scope check.** No conflict. A one-sentence README fix that points at the website walkthrough (or website/src/walkthrough/) as the consumer example.

<a id="g107"></a>
### G107. The template's only font is a Courier New atlas, and nothing tells an adopter whether they may ship it or how to replace it

**major** · onboarding · E04 · journeys: oca-start-project-first-build, oca-ship-desktop-and-browser, sm-close-hardware-and-deploy-milestones

samples/minimal/content/fonts/courier_new_bold_16.spritefont is the only font in the template the primary persona copies. collision, components, local_multiplayer and audio embed the same file, and the hosted LineSweeper wasm embeds it too. NOTICE says it is an atlas built from the Courier New system font with MakeSpriteFont and is redistributed 'on the same footing as any other asset built from a licensed system font', and that the font 'is not Labrador's to license'. It does not name the font's licence terms or say whether a third party's game may redistribute the atlas. your-project.mdx copies the content folder and sprites-and-text.mdx says nothing about font licensing. So an adopter ships a rasterised commercial typeface with no statement of whether that is allowed, and has no instruction to swap in a font they hold rights to. G16 covers notices for linked binaries, not content the template carries.

**Evidence.**

- NOTICE:154-160 ('### Fonts')
- samples/minimal/content/fonts/courier_new_bold_16.spritefont
- samples/collision/CMakeLists.txt:28; samples/components/CMakeLists.txt:27; samples/local_multiplayer/CMakeLists.txt:34; samples/audio/CMakeLists.txt:36; samples/linesweeper/CMakeLists.txt:65
- samples/linesweeper/web/NOTICE.txt:31-32
- grep -i 'courier|licen' website/src/content/docs/docs/get-started/your-project.mdx website/src/content/docs/docs/guides/sprites-and-text.mdx -> no matches

**Scope check.** No conflict. It can be closed by a note on the template and the sprites guide, or by replacing the atlas with one built from an openly licensed font. Either is documentation or content, with no engine mechanism (T1). The golden images and capture images that draw text would have to be regenerated and reviewed if the atlas changes.

<a id="g108"></a>
### G108. No guidance on persisting player data such as settings or saves, on desktop or in the browser

**major** · missing-capability · E05 · journeys: oca-grow-a-game, oca-ship-desktop-and-browser

The engine's asset module only reads: ARCHITECTURE says the save file is the one JSON 'this project writes, and assets only reads', and that save file lives in the private client. samples/components ships a volume-style slider and an option row whose values a game would keep between runs. No guide or concept page says where a game should write settings or saves, or that it should use the standard library to do so. In the browser, nothing in engine/ or samples/ uses IDBFS or localStorage, so anything a page writes to Emscripten's in-memory file system is lost on reload. G85 records only LineSweeper's missing best score, and G74 the private client's JSON-write report. Neither covers the adopter's general question.

**Evidence.**

- docs/design/ARCHITECTURE.md:401-403
- examples.mdx:112-122 (components: option row, slider)
- grep -rn 'IDBFS|localStorage|persist' engine samples (*.cpp, *.h, *.html) -> no storage use
- grep -n 'save|persist' website/src/content/docs/docs/guides/*.mdx, docs/concepts/*.mdx -> no persistence guidance
- cmake/settings.cmake:79 (Emscripten link flags; no FS persistence flags)

**Scope check.** An engine save facility would need a T1 demand argument and touches the read-only asymmetry that ARCHITECTURE and T12 settle (see G74, report 12). A guide saying 'write with std::ofstream to a user folder; in a browser, use IDBFS or localStorage' conflicts with nothing.

<a id="g109"></a>
### G109. No IDE or debugger workflow for the C++ game the site says your debugger works on

**minor** · onboarding · E02 · journeys: oca-start-project-first-build, oca-grow-a-game

why.mdx:41-42 sells 'your debugger, profiler and editor work on your game as they would on any other C++ program'. Get Started and the README are command-line only, though. No page says to open the folder in Visual Studio (which reads CMakePresets.json), pick a preset and a startup item, set breakpoints, or where the working directory and content root fall when launched from the IDE. No page says the Direct3D debug layers appear only when Windows' optional 'Graphics Tools' feature is installed (d3d12/device_resources.cpp:104). No page says what to keep (PDBs) for diagnosing a crash in a shipped build. There is no .vs or .vscode configuration in the tree.

**Evidence.**

- website/src/content/docs/why.mdx:41-42
- grep -i 'open folder|startup item|debugger|launch.vs|vscode' website/src/content/docs README.md -> only why.mdx:41 and concepts/tick.mdx:32
- engine/render/d3d11/device_resources.cpp:28-34, 88-91
- engine/render/d3d12/device_resources.cpp:104-113
- ls .vs .vscode -> absent

**Scope check.** No conflict. It is one Get Started page section. Checking in IDE project files would contradict 'no project format' (why.mdx:36), so the fix is prose, not config.

<a id="g110"></a>
### G110. No CI recipe for an adopter's own repository

**minor** · onboarding · E03 · journeys: oca-grow-a-game, oca-take-engine-upgrade, pi-move-the-pin

The site's only CI mention is the engine's own (why.mdx:178). An adopter with the engine as a submodule has no example workflow showing a recursive submodule checkout, the vcvars or VCPKG_ROOT setup on a hosted Windows runner, fxc on PATH, the vcpkg binary cache, and running their own ctest. Labrador's ci.yml solves each of these but is shaped for the engine's matrix. G08 is the engine checking a consumer shape, and G11 is a smoke check's existence. Neither gives the adopter a workflow to copy.

**Evidence.**

- grep -rln -i 'github actions|continuous integration|\bCI\b' website/src/content/docs -> why.mdx only
- website/src/content/docs/why.mdx:178
- .github/workflows/ci.yml:27, 243 (windows-latest, engine matrix)
- website/src/walkthrough/ (no workflow file)

**Scope check.** No conflict, as long as it is a documented example and not a reusable action the engine maintains (T1). A CI that reaches client repositories from Labrador would fight PHILOSOPHY.md:747-749, and this does not.

<a id="g111"></a>
### G111. The menus guide says a game writes its own slider and option row, and does not point at the components sample that supplies them

**minor** · doc-drift · E05 · journeys: oca-grow-a-game

menus.mdx:39-41 says 'a game's own widgets (a slider, a row with a label and a value) are classes the game writes'. samples/components (66a63f9, the newest sample) ships exactly an option row and a slider to copy, and examples.mdx:112-137 describes them, but the menus guide neither links nor names them. The reader on the guide that needs them is not routed to them.

**Evidence.**

- website/src/content/docs/docs/guides/menus.mdx:39-41
- grep -n 'option|slider|samples/components' menus.mdx -> line 40 only, no link
- website/src/content/docs/examples.mdx:112-137
- samples/components/option_row.h; samples/components/slider.h

**Scope check.** No conflict. It is a one-line link.

<a id="g112"></a>
### G112. Troubleshooting covers only the Windows desktop path

**minor** · onboarding · E08 · journeys: oca-ship-desktop-and-browser, srl-read-a-finished-game

Every troubleshooting.mdx entry concerns cloning, configuring, building or running a Windows exe. There is none for the browser build, though examples.mdx and guides/audio.mdx send readers to the wasm presets. Missing cases include EMSDK unset, the page opened from file:// rather than over HTTP, no Chromium found so the *PageTests are silently absent (ctest shows 17 entries, not 27), a no-WebGL2 browser, and audio silent until the first key. None of the errors these produce is listed.

**Evidence.**

- website/src/content/docs/docs/troubleshooting.mdx headings at lines 13-96 (all desktop)
- website/src/content/docs/examples.mdx:154-157
- website/src/content/docs/docs/guides/audio.mdx:29-34
- CLAUDE.md preset table (wasm: 27 entries, 17 where configure finds no Chromium)

**Scope check.** No conflict. It is documentation and pairs with G46.

<a id="g113"></a>
### G113. Three- and four-player layouts have no public sample or guide

**minor** · onboarding · E05 · journeys: oca-evaluate-and-decide, oca-grow-a-game

The engine offers ScreenLayout::three_player and four_player and four pad slots, and the primary persona's core case is couch split-screen. The public sample and guide cover two players with fixed slots. The guide says joining and reassignment are the game's (local-multiplayer.mdx:56) but never shows a three- or four-pane layout, what fills the spare quadrant in three-player, or view_capacity beyond 2. The only consumer that exercises up to four players is the private ColourWars. An evaluator cannot see the four-player case work, and an adopter has to infer it from viewport_manager.h.

**Evidence.**

- engine/render/screen_layout.h:5-11
- engine/input/gamepads.h:28 (max_count = 4)
- engine/render/viewport_manager.h:21-49
- website/src/content/docs/docs/guides/local-multiplayer.mdx:11-14, 56, 76, 91
- samples/local_multiplayer/arena.h:16

**Scope check.** No conflict with T1 if closed by a guide section or a layout switch in the existing sample, which has present callers. A player-management service would be speculative, and the guide already declines it (local-multiplayer.mdx:56-57).

<a id="g114"></a>
### G114. Repeated 'Play again' keeps every old instance and its WebGL2 context, and it is tested only to a second instance

**minor** · verification-debt · E09 · journeys: bdp-desktop-click-and-play, bdp-when-it-goes-wrong

By design each Play again calls the factory for a whole new wasm instance on a new canvas. The old instance keeps the WebGL2 context it made, and its Application lives in a static whose destructor never runs (web.md:713-722; index.html:15-20). The recorded check mounted one second instance (web.md:1478). Nothing records memory, or whether the old instances are collected, after several rounds. Browsers cap live WebGL contexts and lose the oldest past the cap, so an orphaned instance could receive onContextLost after enough rounds. That consequence is a hypothesis; it has not been observed.

**Evidence.**

- docs/port/web.md:713-722, 1478-1481
- samples/linesweeper/web/index.html:15-20
- docs/port/web-to-be-tested.md:93 ('play, quit, play again', once)
- cmake/settings.cmake:79 (ALLOW_MEMORY_GROWTH)

**Scope check.** No conflict. It is a measurement, plus at most a page-side cleanup (losing the old context with WEBGL_lose_context, dropping references). It is page-level and leaves SEAM.md alone. It could ride on G33's bundle-page test.

<a id="g115"></a>
### G115. No procedure or cadence for moving dependency and toolchain pins

**minor** · release-process · E12 · journeys: sm-land-an-engine-change, oca-take-engine-upgrade

vcpkg.json's builtin-baseline has been d92484ed since the first CMake commit (7f25d0d, 2026-08-07). The emsdk version is pinned only in ci.yml:250 and in CLAUDE.md prose. Both CI jobs run on windows-latest, so a runner-image change (MSVC, Windows SDK, fxc) can break the build with no commit in the repository. There is no dependabot or renovate configuration, and no document says how to bump the baseline, emsdk or the website's Node and Astro dependencies, or what to re-run afterwards: all five backends, the golden set, and the wasm pages. A bump also changes what an adopter's derived manifest pins (G37).

**Evidence.**

- vcpkg.json:15
- git log -p -- vcpkg.json (baseline unchanged since 7f25d0d)
- .github/workflows/ci.yml:27, 243, 250
- ls .github -> workflows/ only (no dependabot.yml)
- CLAUDE.md 'Build, test, run' (emsdk 6.0.12)

**Scope check.** No conflict. It is a short procedure, possibly a pinned runner image. Automated bump bots add a supply-chain surface that T9 would want argued, and a written procedure does not.

<a id="g116"></a>
### G116. FocusStyle's default 'disabled' grey is lighter than its 'unfocused' grey, the opposite of what the menus guide says

**minor** · api-ergonomics · E05 · journeys: oca-grow-a-game, srl-learn-the-engine-model

FocusStyle defaults to focused = Colour::white, unfocused = Colour::gray, disabled = Colour::dark_gray (engine/ui/focus.h:20-28). In the X11/CSS names the engine uses, dark_gray is 0xa9 and gray is 0x80 (engine/render/colour.cpp:226,255), so the disabled row is the lighter one: 8.9:1 on black against 5.3:1 for an enabled unfocused row, and only 2.35:1 below focused white. The menus guide tells adopters the opposite ('dark grey for one that is disabled', website/src/content/docs/docs/guides/menus.mdx:56-57). A player would read the unavailable row as nearest to selected, defeating the reason focus.cpp:162-166 gives for letting disabled outrank focused. No sample shows the defaults (tests/ui/focus_tests.cpp:20 overrides disabled; no sample calls set_enabled(false)), so it has never been seen. Computed from constants, not observed on screen.

**Evidence.**

- engine/ui/focus.h:18-28
- engine/render/colour.cpp:226 (dark_gray 0xa9)
- engine/render/colour.cpp:255 (gray 0x80)
- website/src/content/docs/docs/guides/menus.mdx:56-57
- engine/ui/focus.cpp:162-170
- tests/ui/focus_tests.cpp:20

**Scope check.** In scope: one default constant or one guide sentence on existing mechanism; no policy added (T1), the simpler model (T3). Not action mapping, not an editor.

<a id="g117"></a>
### G117. LineSweeper's landing shadow is 1.3 to 2.0:1 against the well

**minor** · other · E09 · journeys: bdp-desktop-click-and-play

The landing shadow keeps 28% of the piece colour (shadow_alpha 0.28, samples/linesweeper/presentation/board_view.cpp:70-71) over a well of (12,14,18) (board_view.cpp:65), blended premultiplied (palette.h:62-67). Computed contrast against the empty well: J 1.28, T 1.30, Z 1.33, L 1.67, S 1.72, O 1.97, I 2.02 - all below the 3:1 WCAG 1.4.11 asks of a graphic the player needs. The shadow is the genre's landing aid; for J, T and Z it is near-invisible to a low-vision player on the hosted demo. palette.h:11-13 already isolates colour so a cell's look can change without touching rules. Nothing records why 0.28 was chosen. Computed from constants; not looked at on screen for this purpose.

**Evidence.**

- samples/linesweeper/presentation/board_view.cpp:65 (well_colour 12,14,18)
- samples/linesweeper/presentation/board_view.cpp:70-71 (shadow_alpha 0.28)
- samples/linesweeper/presentation/board_view.cpp:215-216
- samples/linesweeper/presentation/palette.h:26-39,62-67
- samples/linesweeper/presentation/palette.h:11-13

**Scope check.** Sample policy, not engine work (T1). One constant or an outline drawn with the sprite verb already used (T3/T4). No settings screen or remapping.

<a id="g118"></a>
### G118. Particle bursts have no reduced-motion path and have never been checked against flash thresholds

**minor** · missing-capability · E09 · journeys: bdp-desktop-click-and-play, srl-read-a-finished-game

Line clears throw sparks and a top-out throws 9,600 particles in one frame (samples/linesweeper/presentation/particles.h:50-58), drawn additively so overlaps saturate towards white (palette.h:79-82). The README records the top-out burst once made the banner unreadable for a second (samples/linesweeper/README.md:470-475). Neither the hosted page nor the module reads prefers-reduced-motion: the page's only matchMedia is pointer: coarse (samples/linesweeper/web/index.html:108), and no file under engine/, samples/ or website/src reads the motion preference. No document records a WCAG 2.3.1 flash check. The site itself honours reduced motion (Starlight CSS; LineSweeperFootage.astro:8-10); the demo is the only animated surface without a switch.

**Evidence.**

- samples/linesweeper/presentation/particles.h:50-58
- samples/linesweeper/presentation/palette.h:79-82
- samples/linesweeper/README.md:470-475
- samples/linesweeper/web/index.html:108 (only matchMedia call)
- No 'prefers-reduced-motion' in engine/, samples/ or website/src (grep)

**Scope check.** T1 keeps an OS-preference layer out of the engine. The answer is sample-side: main_web.cpp or the page reads the media query and the sample throws fewer sparks. Policy in the sample; the engine changes nothing.

<a id="g119"></a>
### G119. No stated accessibility position for the demo, the samples or what the engine offers a game

**minor** · doc-drift · E01 · journeys: oca-evaluate-and-decide, bdp-desktop-click-and-play, srl-read-a-finished-game

The website's accessibility is verified and recorded (docs/website-plan.md:131,141-147,155-159); nothing says what the hosted demo, samples or engine aim for. PHILOSOPHY has no accessibility mention in its trade-offs, UI section or non-goals (docs/design/PHILOSOPHY.md:583-612,781+), nor do README.md or web.md section 7. Consequences: game state never reaches assistive technology (the canvas's only accessible name is 'LineSweeper', index.html:129; the role=status line carries only instructions, loading and errors, index.html:75,145-157), and nothing says this is deliberate; palette.h:11-13 anticipates a colour-blind palette with no trigger or owner; an adopter judging whether their game can meet a platform's accessibility expectations has no page saying what the engine provides (brightness-based focus, a Tab-safe canvas, three input devices, fixed bindings by design) and what is the game's job. The current state is accidental, not decided.

**Evidence.**

- docs/design/PHILOSOPHY.md (no accessibility mention; UI at :583-612, Non-goals at :781)
- samples/linesweeper/web/index.html:75,129,145-157
- samples/linesweeper/presentation/palette.h:11-13
- docs/website-plan.md:131 (site-only accessibility record)
- engine/app/web/dom_events.cpp:183-191 (Tab kept, undocumented as an accessibility property)

**Scope check.** A statement, not a feature: mechanism vs game policy is T1 applied. Remappable controls stay known-absent and the statement should say so. No screen-reader layer or accessibility framework is proposed (speculative under T1).

<a id="g120"></a>
### G120. The website's accessibility checks are manual one-off runs, and CI cannot catch a regression

**minor** · verification-debt · E12 · journeys: sm-land-an-engine-change

The axe-core WCAG 2.2 A/AA runs, keyboard focus checks and contrast fixes were done by hand on 4 and 8 October and recorded as prose (docs/website-plan.md:131,141-147,155-159,184,203-204). website.yml runs only npm ci, astro check, npm test and astro build (.github/workflows/website.yml:51-62); website/tests holds only code-links.test.mjs and reference.test.mjs; Cloudflare's production build runs the same (website-plan.md:118). A new colour in site.css, a Starlight or Mermaid upgrade that changes theme greys (MarkdownContent.astro:31-33 already patches one), or a page without alt text all deploy green. Because the site is built from engine files, engine-side commits also produce pages nobody re-scans. There is no re-run procedure and no record of when it last ran against the current site.

**Evidence.**

- .github/workflows/website.yml:51-62
- website/tests/ (code-links.test.mjs, reference.test.mjs only)
- docs/website-plan.md:118 (Workers build command)
- docs/website-plan.md:131,141-147,155-159
- website/src/components/overrides/MarkdownContent.astro:31-33

**Scope check.** T6 and the repository's habit of turning checks into build failures argue for one headless axe pass over dist/ in website.yml; the fallback is a written re-run procedure in website/README.md. Not general tooling.

<a id="g121"></a>
### G121. The website never says the Windows pad path is XInput, which controllers that admits, or how slots are assigned

**minor** · doc-drift · E05 · journeys: oca-native-gamepad-local-play

The input guide presents gamepads with no desktop platform facts ('Gamepads has four slots, numbered 0 to 3', input.mdx:49-51). It never says Windows reads pads through XInput via DirectXTK, so only XInput-compatible controllers appear (engine/input/xinput/gamepad_reader.cpp:3-5,21-41; gamepad_reader.h:55-62). It omits that a slot is the system-assigned XInput user index, held only while connected, and that a replugged pad may return in a different slot - stated only in engine/input/gamepads.h:19-24 and gamepad_reader.h:39-44. 'XInput' appears nowhere under website/src/content/docs; publicly only at README.md:199 and samples/local_multiplayer/README.md:20-21. For couch play on players' own controllers, which pads work is a fit-decision fact, and the ignored-pad troubleshooting entry does not raise it (troubleshooting.mdx:89-94). Desktop counterpart of G47. No non-Xbox controller tested; the class limit is a property of the API.

**Evidence.**

- website/src/content/docs/docs/guides/input.mdx:47-65
- website/src/content/docs/docs/troubleshooting.mdx:89-94
- engine/input/gamepads.h:19-24
- engine/input/gamepad_reader.h:39-44, 55-62
- engine/input/xinput/gamepad_reader.cpp:3-5, 16-18, 64-87
- README.md:199
- Grep for 'xinput' (case-insensitive) under website/src/content/docs returns nothing

**Scope check.** A disclosure paragraph in an existing guide. Supporting more pad APIs would need its own T1/T5 reason and is not proposed.

<a id="g122"></a>
### G122. Single-player code reads only pad slot 0, and nothing tells an adopter how to accept a lone pad in any slot

**minor** · api-ergonomics · E05 · journeys: oca-native-gamepad-local-play

Every one-player sample and every snippet the input guide quotes hard-codes slot 0: minimal (samples/minimal/states/hello_state.cpp:136,164; quoted at input.mdx:58-72), LineSweeper (play_state.cpp:259,377,401; pause_state.cpp:273-288) and the components gallery (gallery_state.cpp:166,201,222). Slots are XInput user indices held while connected (gamepads.h:19-24), so a player whose only pad sits in slot 1+ (for example after the slot-0 pad was unplugged) is ignored. The site's only answer is troubleshooting.mdx:93-94 ('check it is in slot 0 if the game reads slot 0'), which says neither how a player tells or changes the slot nor how a game avoids the problem. The template passes this on. Closing it is a guide paragraph and optionally a template change (OR the edges of all four slots, or bind to the first slot that presses). Inferred from code and the slot rule; no multi-pad run.

**Evidence.**

- samples/minimal/states/hello_state.cpp:125-136, 164
- samples/linesweeper/states/play_state.cpp:259, 377, 401
- samples/linesweeper/states/pause_state.cpp:273-288
- samples/components/gallery_state.cpp:166, 199-222
- website/src/content/docs/docs/guides/input.mdx:58-62
- website/src/content/docs/docs/troubleshooting.mdx:93-94
- engine/input/gamepads.h:19-24

**Scope check.** A documented pattern and/or template edit stays inside T1 (samples are present callers). An engine 'any pad' or player-assignment service would be speculative and is not proposed. Not action mapping: bindings stay in game code.

<a id="g123"></a>
### G123. No sample or guide shows a join screen: hot-plug, binding a pad to a player, and per-player menu focus are prose only

**minor** · onboarding · E05 · journeys: oca-native-gamepad-local-play

The engine supplies the pieces for drop-in couch play: just_connected/just_disconnected 'for ... "Player 2, press Start"' (input.mdx:64-65), the disconnect-while-held spelling (gamepad.h:113-118), and FocusGroup's per-slot cursors and live enablement (focus.h:56-64,125-143). No code an adopter can read uses them: grep for just_connected|just_disconnected under samples/ returns nothing; every FocusGroup in samples/ uses one slot (pause_state.h:126-129; gallery_state.cpp:129-133); local_multiplayer fixes players to slots 0 and 1 and defers joining to game policy (local-multiplayer.mdx:56-57). The menus guide gives split-screen menus three sentences (menus.mdx:99-103) and does not say how an input source maps to a focus slot (pad slot, or a keyboard half as in local_multiplayer/controls.cpp:13-16), that each player needs their own DirectionRepeat, or what to do when a pad drops mid-menu. Only tests/ui/focus_tests.cpp:263-310 exercises two cursors. The only known consumer that assembles this is private ColourWars. Related to G113 (3-4 pane layouts); one join-and-select screen in local_multiplayer could close both.

**Evidence.**

- website/src/content/docs/docs/guides/input.mdx:64-65
- website/src/content/docs/docs/guides/menus.mdx:99-103
- website/src/content/docs/docs/guides/local-multiplayer.mdx:56-57
- engine/input/gamepad.h:113-118
- engine/ui/focus.h:56-64, 125-143
- samples/linesweeper/states/pause_state.h:126-129
- samples/components/gallery_state.cpp:129-133
- tests/ui/focus_tests.cpp:263-310
- Grep for just_connected|just_disconnected under samples/ returns no matches

**Scope check.** Within T1 if closed by extending an existing sample: the APIs exist and are unexemplified. A join or player-management service in the engine would be speculative and local-multiplayer.mdx:56-57 already declines one. Local only; online play stays out.

<a id="g124"></a>
### G124. Whether XInput honours the reader's suspend is unverified, so a backgrounded game may still take pad input

**minor** · verification-debt · E05 · journeys: oca-native-gamepad-local-play

Application suspends the gamepad reader when the window leaves the foreground or is minimised (engine/app/application.cpp:360-372), intending that a backgrounded game does not act on the pad (gamepad_reader.h:116-122). The same header disclaims it: 'Whether XInput honours a suspend on a current Windows is the backend's affair and is not verified in this tree' (gamepad_reader.h:124-132). The backend forwards straight to DirectX::GamePad::Suspend/Resume (xinput/gamepad_reader.cpp:118-126). If suspend does not take effect, a player who alt-tabs and keeps using the pad drives the game behind the other window; only phantom presses on return are guaranteed suppressed (gamepad.h:85-101). No test or recorded manual check settles it, and the input guide's focus-loss note covers only keys (input.mdx:44-45). Not run.

**Evidence.**

- engine/input/gamepad_reader.h:116-138
- engine/app/application.cpp:360-372
- engine/input/xinput/gamepad_reader.cpp:118-126
- website/src/content/docs/docs/guides/input.mdx:44-45

**Scope check.** A one-time recorded manual check with a pad plus a guide sentence. If it fails, a game-side gate on core/state.h on_deactivated uses an existing hook; no engine API.

<a id="g125"></a>
### G125. The XInput translation has no headless test and the tree records no physical-pad run of a desktop sample

**minor** · verification-debt · E05 · journeys: oca-native-gamepad-local-play

The browser pad translation is held by tests/input/standard_gamepad_tests.cpp (buttons by index, +y sign, slot compaction, a fifth pad waiting), compiled only for the web input backend (tests/input/CMakeLists.txt:13-17). The desktop translation in engine/input/xinput/gamepad_reader.cpp:23-56,58-89 (fourteen button bits, one y negation, DEAD_ZONE_NONE) has no counterpart, though every Windows adopter uses it. The local multiplayer smoke 'does not test physical input' (local-multiplayer.mdx:126-127) and its README verification names keyboard/stick routing only through LocalMultiplayerTests (samples/local_multiplayer/README.md:70-79). The browser side records its hardware debt (docs/port/web.md:1518); the desktop side records neither a physical-pad check nor its absence. A swapped bit or lost negation would pass every ctest entry.

**Evidence.**

- engine/input/xinput/gamepad_reader.cpp:23-89
- tests/input/CMakeLists.txt:3-17
- tests/input/standard_gamepad_tests.cpp:44-197
- website/src/content/docs/docs/guides/local-multiplayer.mdx:116-127
- samples/local_multiplayer/README.md:70-79

**Scope check.** A test that builds DirectX::GamePad::State by hand needs only a small extraction inside the backend folder; a recorded pad check needs hardware the owner has. No new API.

<a id="g126"></a>
### G126. The promotion rule's test, 'copied unchanged', cannot be met by these components and has no record to be measured against

**minor** · other · E05 · journeys: oca-grow-a-game, sm-evolve-api-and-move-the-client

PHILOSOPHY promotes a component every client copies unchanged into engine/ui/ and keeps an edited one as a component (docs/design/PHILOSOPHY.md:603-609). The test cannot fire: (1) styling is hard-coded in the .cpp files with instructions to change it (samples/components/option_row.cpp:13-15 value_colour 'Change it to your game's'; slider.cpp:17-20), and PHILOSOPHY.md:600 says styling belongs to the game, so every faithful copy is edited and the rule does not separate styling edits from behaviour edits; (2) nothing records which client copied what or changed what, and outside adopters have no channel (related G21, G54); (3) samples/components/README.md:23-34 offers ColourWars' option row as 'the first evidence', 'the same shape', but that OptionRow takes an Adjust callback and MenuStyle, draws UiTextDropShadow and holds no choices or index (ColourWars game/states/option_row.h:11-37) - a different ownership model, i.e. an edit, and unverifiable publicly (compare G40). The owner has a rule that cannot be evaluated, with its first data point misdescribed.

**Evidence.**

- docs/design/PHILOSOPHY.md:600 ('Styling and content belong to the game')
- docs/design/PHILOSOPHY.md:603-609 (copied unchanged -> mechanism; edited -> policy)
- samples/components/option_row.cpp:13-15 (constexpr value_colour, 'Change it to your game's')
- samples/components/slider.cpp:17-20 (track_colour, 'Change both to your game's')
- samples/components/README.md:23-34 (rule restated; ColourWars 'same shape' claim)
- C:/VSProjects/ColourWars/game/states/option_row.h:11-37 (Adjust callback, MenuStyle, UiTextDropShadow, no choices/index)

**Scope check.** Argues for less machinery: a T3 amendment that styling edits do not count, or styling as a constructor parameter like FocusStyle (engine/ui/focus.h:18-29), plus a README correction. A formal uptake ledger may exceed capacity; 'promote when a second public client exists' is the T1 demand test.

<a id="g127"></a>
### G127. Components and the menus guide are directional-only: a click or tap on a row or a slider does nothing, and the one tap pattern in the tree is not shown

**minor** · api-ergonomics · E05 · journeys: oca-grow-a-game, oca-ship-desktop-and-browser

A component reads no input; the page drives it only through FocusGroup::move and adjust(Direction) (samples/components/README.md:36-53; gallery_state.cpp:196-230). engine/ui/ has no pointer or hit-test entry point (grep for mouse|touch|pointer|hit in engine/ui/*.h finds nothing). ComponentsGallery.html therefore cannot be used with a mouse or finger, and its help line names only keys and pad (gallery_state.cpp:69-71). The primary persona ships to the browser where mouse users click settings screens. LineSweeper's pause menu already hand-rolls tap-to-choose with bounds() (samples/linesweeper/states/pause_state.cpp:221-247), but the menus guide, which follows that file, never shows tapped_row and says nothing on mouse or touch. No component maps a point to a value (e.g. slider value from x). PHILOSOPHY frames UI for controller games (PHILOSOPHY.md:585-588), so it is not a contradiction, but nothing tells a browser adopter that pointer menus are theirs to build or how.

**Evidence.**

- engine/ui/focus.h:80-140 (FocusGroup API: add, focused, set_focused, move, activate; no point query)
- grep -iE 'mouse|touch|pointer|hit' engine/ui/*.h -> no matches
- samples/components/gallery_state.cpp:69-71, 196-230 (keys and pad only)
- samples/linesweeper/states/pause_state.cpp:221-247 (tapped_row hand-rolled hit test)
- website/src/content/docs/docs/guides/menus.mdx (no mention of touch, mouse or tapped_row)
- website/src/content/docs/docs/guides/input.mdx:121-151
- docs/design/PHILOSOPHY.md:585-588

**Scope check.** An engine pointer-focus mechanism would need a T1 demand argument. The cheap fix is a menus-guide section quoting tapped_row plus optionally a slider value_at(x) in the component. Hit-testing, not rebinding; action mapping stays known-absent.

<a id="g128"></a>
### G128. Nothing wires a settings component to what the engine can actually set, and the gallery's window row offers a mode the engine does not have

**minor** · onboarding · E05 · journeys: oca-grow-a-game, oca-ship-desktop-and-browser

The gallery stops each callback at a status line (gallery_state.cpp:85-121; gallery_state.h:21-24) though its rows are labelled for real settings (Window: Windowed/Full screen/Borderless; Music and Effects volume). The engine has the calls: Application::set_fullscreen, fullscreen() and set_resolution for 'its own options menu' (engine/app/application.h:182-197), and SoundBank per-play volume and set_effect_volume (engine/audio/sound_bank.h:125-136). No guide or sample connects them; grep of website docs for fullscreen|full screen finds nothing. set_fullscreen is a boolean between borderless full screen and a window (application.h:188-191), so the three-way row shows a distinction an adopter cannot implement. In a browser the platform may refuse or leave full screen and fullscreen() follows it (application.h:193-197), so the row must be re-synced with set_index, which fires no callback (option_row.h:36-37) - the subtle case a copy gets wrong. There is no master/category volume, so an Effects slider means scaling every play_wave call in game code, fine by T1 but unsaid.

**Evidence.**

- samples/components/gallery_state.cpp:80-121
- samples/components/gallery_state.h:21-24
- engine/app/application.h:182-197
- engine/audio/sound_bank.h:125-136
- samples/components/option_row.h:36-37, 71-72 (set_index fires no callback)
- grep -rni 'fullscreen|full screen' website/src/content/docs/docs -> no matches

**Scope check.** Sample and documentation work (T3, T4): two window modes, wired to set_fullscreen and re-synced each tick, and a sentence on applying a slider to SoundBank volumes. A volume-bus mechanism would need its own T1 argument. Persisting the values is G108.

<a id="g129"></a>
### G129. The guide's XWBTool command omits -nc, so a bank packed per the guide plays on Windows and fails to load in the browser build

**minor** · doc-drift · E04 · journeys: oca-grow-a-game, oca-ship-desktop-and-browser

The audio guide's only packing command is `xwbtool -f -o tones.xwb ping.wav chime.wav drone.wav` (audio.mdx:135), calling out only -f. The sample README says -nc is what lets the engine's own reader read the bank, because it refuses compact entries (samples/audio/README.md:118-124; engine/audio/xwb_file.h:75-82; xwb_file.cpp:406-409). The browser backend reads every bank with that reader (webaudio/audio_device.cpp:23-28,608), while Windows uses DirectXTK's (README.md:131-133). So a guide-following adopter sees it work on desktop and meets the failure on their first browser build. The error is loud and names the cause (T6 honoured), but the published instruction produces content for one of the two platforms the guide claims. No site page mentions -nc or compact entries. Severity re-checked to minor: the persona completes the journey because the throw names the refusal and the fix is one flag. XWBTool not run; the compact default rests on the repository's own statements.

**Evidence.**

- website/src/content/docs/docs/guides/audio.mdx:131-140
- samples/audio/README.md:110-124
- engine/audio/xwb_file.h:75-84
- engine/audio/xwb_file.cpp:406-409
- engine/audio/webaudio/audio_device.cpp:23-28, 608
- grep -rn -i 'compact|-nc' website/src/content -> no hits

**Scope check.** One flag and one sentence (T3). Making the reader accept compact banks would need its own T1 argument. Related G12 and G112.

<a id="g130"></a>
### G130. No page says which WAV formats an adopter's sounds must be exported in, or what each costs in a browser

**minor** · content-pipeline · E04 · journeys: oca-grow-a-game

The audio guide says to combine WAVs (audio.mdx:131-133) with no source-format rule. The reader's limits appear only in a header comment and throws: PCM 8/16-bit or MS-ADPCM, at most two channels, xWMA and XMA refused (engine/audio/xwb_file.h:75-84; xwb_file.cpp:223-305). The browser backend fails a wave whose sample rate AudioBuffer refuses (webaudio/audio_device.cpp:26-28). The sample's own format (mono 16-bit PCM at 22,050 Hz, samples/audio/README.md:94-95) is a description, not a requirement. Nothing guides size: ADPCM decodes to about four times its file size (xwb_file.h:66-73), and in a browser every wave becomes a float AudioBuffer. An adopter exporting with an audio tool's defaults learns the rules one load error at a time. Not observed with a non-conforming WAV.

**Evidence.**

- website/src/content/docs/docs/guides/audio.mdx:122-140
- engine/audio/xwb_file.h:66-84
- engine/audio/xwb_file.cpp:223-305
- engine/audio/webaudio/audio_device.cpp:23-28
- samples/audio/README.md:92-98

**Scope check.** A short 'export as' paragraph documenting what the engine already enforces (T6). No new capability.

<a id="g131"></a>
### G131. No guidance for music-sized audio: banks are in-memory only, and the template embeds all content in the browser module

**minor** · missing-capability · E04 · journeys: oca-grow-a-game, oca-ship-desktop-and-browser

sound_bank.h:128-129 names music as what an effect instance is for and audio_device.h:165-166 designs the gesture wait around music started at boot, but the guide scopes streaming banks out (audio.mdx:139-140) and no page says what to do for a multi-minute track. On desktop the whole track sits in an in-memory XAudio2 bank; in the browser the template embeds the whole content folder in the module (samples/minimal/CMakeLists.txt:31-32) and decodes each wave to 16-bit then to a float AudioBuffer (webaudio/audio_device.cpp:24-27). The only threshold, switching to --preload-file past about 1 MB, is internal (docs/port/web.md:481-489; samples/audio/CMakeLists.txt:19-20). An adopter adding background music to the template embeds multi-megabyte audio in the page download with no warning, against the constrained-hardware positioning. Related G51, G09.

**Evidence.**

- engine/audio/sound_bank.h:128-129
- engine/audio/audio_device.h:159-167
- website/src/content/docs/docs/guides/audio.mdx:139-140
- samples/minimal/CMakeLists.txt:31-32
- engine/audio/webaudio/audio_device.cpp:23-28
- docs/port/web.md:481-489
- samples/audio/CMakeLists.txt:19-20

**Scope check.** A streaming loader needs a T1 demand argument and web.md:487-489 already declines an asynchronous loader under T3. Documenting the in-memory cost and the --preload-file switch is the T3-sized answer.

<a id="g132"></a>
### G132. The guide states the gesture rule only for the sample, not its consequence for a game's own sounds

**minor** · doc-drift · E08 · journeys: oca-ship-desktop-and-browser

audio.mdx:29-32 says a browser plays nothing until the first key, click or touch and that the engine waits. The contract a game depends on is only in audio_device.h:159-167, on the AudioDevice reference page meant for clients bypassing SoundBank (audio.mdx:116-118): a play_wave one-shot before the gesture is dropped, not queued, while a looping effect started at boot reports playing and begins when allowed; Escape does not count as a gesture (webaudio/audio_device.cpp:48-52). sound_bank.h, which the guide teaches, does not mention it. A title-screen jingle played as a one-shot is heard on desktop and never in the browser, with no page on the guide's path explaining why. Not observed in a browser.

**Evidence.**

- website/src/content/docs/docs/guides/audio.mdx:29-34, 116-120
- engine/audio/audio_device.h:159-167
- engine/audio/webaudio/audio_device.cpp:39-52
- grep -n -i 'gesture|allowed|browser' engine/audio/sound_bank.h -> no hits

**Scope check.** Two sentences on the guide or one in sound_bank.h. The troubleshooting entry for silence is part of G112 and not re-reported.

<a id="g133"></a>
### G133. The Vulkan SDK is installed at 'latest' and the runner's Chrome floats, against CI's own pinning argument, and no scheduled run separates drift from a change

**minor** · release-process · E12 · journeys: sm-land-an-engine-change

ci.yml pins Emscripten because a compiler that moves under -Werror turns green to red with no commit to blame (ci.yml:239-241), yet the Vulkan job fetches sdk.lunarg.com/.../latest/windows/vulkan_sdk.exe (ci.yml:165), which supplies the SPIR-V dxc and the loader nine test executables need (ci.yml:71-80). The wasm pixel steps hold sixty goldens against whatever Chrome/SwiftShader windows-latest carries, and SwiftShader rounding has already missed the allowance once (ci.yml:310-318). run_page.mjs logs the renderer string, not the browser version (run_page.mjs:76-78), and no run records the SDK version. No workflow has a schedule trigger (ci.yml:13-17; website.yml:25-29), so external drift first appears on an unrelated PR. G115 names the vcpkg baseline, emsdk and runner image; this is the two inputs it does not name plus the missing detection.

**Evidence.**

- .github/workflows/ci.yml:165 (vulkan_sdk.exe from /latest/)
- .github/workflows/ci.yml:236-241 (pinning rationale given for emsdk only)
- .github/workflows/ci.yml:310-318 (SwiftShader rounding history)
- .github/workflows/ci.yml:13-17 and .github/workflows/website.yml:25-29 (no schedule:)
- tests/render/web/run_page.mjs:76-78
- docs/review/2026-09-26/extension/evidence/vulkan-stream-loader.txt:51 (local SDK 1.4.357.0, the only recorded version)

**Scope check.** CI hygiene for the existing build: pin the SDK URL and add a weekly scheduled run (T1-sized). No non-goal touched.

<a id="g134"></a>
### G134. Required status checks are tied to matrix parameter strings, held only in GitHub settings, and not required to be up to date

**minor** · release-process · E12 · journeys: sm-land-an-engine-change

Branch protection requires contexts named after every matrix field in ci.yml:95-113 (e.g. 'windows (x64-debug-vulkan, -E RenderPixelTests, true)'), plus 'wasm' and a bare 'build' for the website job (website.yml:36-37). Editing a skip_tests or vulkan_sdk value renames the check and every PR waits on a context that never reports unless the owner (enforce_admins false) edits settings by hand; a new matrix entry (such as G31's release presets) is not required until added; any future job named 'build' would satisfy the website requirement. strict is false, so a PR green against an older master is not re-checked: a PR rewording a quoted sample line and one adding a page that quotes it can each pass and leave master's site build and the Cloudflare deploy red. None of this is written in the tree. Related to G65 but distinct: this is the coupling between workflow edits and protection settings.

**Evidence.**

- gh api repos/matt439/Labrador/branches/master/protection: contexts list, strict:false, enforce_admins:false
- .github/workflows/ci.yml:95-113
- .github/workflows/website.yml:36-37 (job id 'build')

**Scope check.** Process only, no feature.

<a id="g135"></a>
### G135. The website plan's preview-before-production and rollback rules have no procedure, and the preview builds they rely on are off

**minor** · release-process · E12 · journeys: sm-close-hardware-and-deploy-milestones

docs/website-plan.md:218 says a preview should be reviewed before production and :132 says to retain a known working deployment for rollback. Cloudflare preview builds are off (website/README.md:201) and Workers Builds deploys every master commit straight to production (README:191). The only preview is website.yml's dist artifact (website.yml:61-65), which must be downloaded and served locally, and no document names it as the review step. Nothing says how to roll back (dashboard or wrangler rollback) or what to check on the live site. The pre-launch validation (axe AA in both themes, no sideways scroll at 360 px, focus order; website-plan.md:131) is not in the gate (website.yml:51-59; see G120). docs/website-versioning.md:60-62 states rollback only for a future first engine release.

**Evidence.**

- docs/website-plan.md:131, 132, 218
- website/README.md:191-212 (Preview builds: Off; no rollback text)
- .github/workflows/website.yml:51-65
- docs/website-versioning.md:60-62

**Scope check.** Reconcile plan with practice by writing the procedure or amending the plan (T3). Not G57 (site URL) or G115 (pin cadence).

<a id="g136"></a>
### G136. Cloudflare's build settings duplicate the website gate in a dashboard nothing checks, and wrangler also publishes a workers.dev copy

**minor** · release-process · E12 · journeys: sm-close-hardware-and-deploy-milestones

The production build command, root directory, branch and NODE_VERSION exist only in Cloudflare's dashboard, mirrored as prose in website/README.md:194-201, and repeat website.yml's steps (website.yml:51-59). A gate change (new npm script, a Node major in setup-node) must be made again by hand in the dashboard; no file says so or checks it, and the deploy is the copy that matters. wrangler.jsonc sets workers_dev: true (website/wrangler.jsonc:4), so the site is also served on the account's workers.dev hostname; with no site key (G57) neither host carries a canonical URL. The workers.dev host was not fetched because the subdomain is unrecorded.

**Evidence.**

- website/README.md:191-212
- website/wrangler.jsonc:1-9 (workers_dev: true)
- .github/workflows/website.yml:44-59
- website/astro.config.ts (no site key; see G57)

**Scope check.** Hosting configuration for the existing site. G57 covers the missing site URL; the duplicate host and unchecked dashboard gate are new.

<a id="g137"></a>
### G137. The site's LineSweeper footage predates a sample change that moved the stills, and nothing flags a stale capture or video

**minor** · verification-debt · E12 · journeys: sm-land-an-engine-change, oca-evaluate-and-decide

The site's frames and video come from tools/linesweeper_capture and are regenerated by hand (website/README.md:126-160). Commit 5af01f6 (2026-10-11) changed LineSweeper's centred text and regenerated linesweeper-pause.png and -top-out.png but not the MP4, last committed at 7166490 (2026-10-08), which shows the top-out effect (README:155) and so very likely still draws pre-fix text. 98a0665 and d169ea9 have since changed LineSweeper presentation and states. Nothing in website.yml, ci.yml or the capture tool compares checked-in stills or video against the sample at the built commit, and regeneration needs an x64-release build and a device CI's website job lacks. LineSweeperCapture was not re-run (local binary predates 5af01f6). Related G67.

**Evidence.**

- git log -1 -- website/src/assets/footage -> 7166490 2026-10-08
- git show 5af01f6 --stat (captures/linesweeper-pause.png and -top-out.png changed; footage untouched)
- git log 5af01f6..HEAD -- samples/linesweeper/presentation samples/linesweeper/states -> d169ea9, 98a0665
- website/README.md:126-160
- out/build/x64-release/tools/linesweeper_capture/LineSweeperCapture.exe dated 2026-10-10

**Scope check.** Content freshness for the existing showcase. Cheapest answer: a rule or STATUS-style box that a presentation change regenerates stills and footage in the same commit.

<a id="g138"></a>
### G138. layer_depth is published as a sort key on every drawing page, but the engine ignores it and only a renderer-writer header says so

**major** · doc-drift · E06 · journeys: oca-grow-a-game, srl-learn-the-engine-model

Every game-facing drawing API takes a layer_depth (DrawList::draw_sprite/draw_text, DrawObject, Label and Text constructors). The caller-facing comment on draw_sprite says 'CONSTRAINT: sort depth is per draw, not per object' (engine/render/renderer.h:155-171), and Concepts > Drawing implies depth orders draws when it differs (website/src/content/docs/docs/concepts/drawing.mdx:35-37). In fact no backend reads it and call order alone decides - stated in sprite_vertex.h:22-31 (a header the reference index sends game readers away from), in the unpublished d3d11/renderer.cpp:380-386, and in the test title tests/render/pixel_tests.cpp:950. An adopter who gives a background a lower depth than the player gets no effect and no error. Fix: one contract sentence above draw_sprite/draw_text, one in draw_object.h, and a corrected clause in drawing.mdx (T6). Severity kept major: the published contract on the core drawing call is misleading and the truth needs knowledge only renderer-writer material holds. Not G36 (sample claims) or G94 (backend rationale).

**Evidence.**

- engine/render/renderer.h:155-171 (draw_sprite comment 'sort depth is per draw')
- engine/render/draw_object.h:20,28,34
- engine/render/label.h:30-38
- engine/render/sprite_vertex.h:22-31 ('no sort mode that consults it')
- engine/render/d3d11/renderer.cpp:380-386 (std::ignore = layer_depth)
- tests/render/pixel_tests.cpp:950
- website/src/content/docs/docs/concepts/drawing.mdx:35-37

**Scope check.** Documenting that depth is ignored is T6; adding a depth sort would be a feature for T1 to weigh and is not proposed.

<a id="g139"></a>
### G139. Game-facing reference pages leave out how a game obtains the type and what its constructor parameters mean

**minor** · doc-drift · E06 · journeys: oca-grow-a-game, srl-learn-the-engine-model

The reference index promises 'the exact contract of every public type and function' (reference/index.mdx:12). Walked without a sample: (1) Gamepads publishes a public constructor taking GamepadReader* and a public poll() (engine/input/gamepads.h:31,36) and never says a game gets it from Application::gamepads() (application.h:243); only the input guide says so (input.mdx:11-12), and Keyboard, Mouse and Touch have the same omission. (2) Text has no class comment (engine/render/text.h:11); neither Text nor Label says what font_name names, where RenderResources* comes from, that an unknown name throws std::out_of_range (render_resources.h:131-133), what position anchors, or that rotation is radians. (3) State and init() are uncommented (engine/core/state.h:34); when init runs is only on StateContext (state_context.h:65-73). (4) Camera's DEFAULT_CAMERA and calculate_* members are uncommented (camera.h:47,82,86). (5) Of six pages walked only Application shows usage code. The site build fails on an unlisted header but not on an uncommented public declaration, so the gaps recur. Severity re-checked to minor: the guides route an adopter to most of these facts. Related to G93/G94 but distinct: the needed fact is missing, not surplus.

**Evidence.**

- website/src/content/docs/docs/reference/index.mdx:12
- engine/input/gamepads.h:31,36
- engine/app/application.h:243
- website/src/content/docs/docs/guides/input.mdx:11-12
- engine/render/text.h:11
- engine/render/render_resources.h:131-133
- engine/core/state.h:34; engine/core/state_context.h:65-73
- engine/render/camera.h:47,82,86
- website/src/lib/reference.ts, linkSymbols comment

**Scope check.** Header comments are contract under CONVENTIONS and T6; no feature implied. A coverage count printed by the site build is optional and small.

<a id="g140"></a>
### G140. Reference pages never link back to the guide or concept that teaches the type

**minor** · api-ergonomics · E06 · journeys: oca-grow-a-game, srl-learn-the-engine-model

Guide-to-reference links are automatic and dense; the reverse does not exist. A generated page links only to About the reference, its module, related types and GitHub includer files (website/src/lib/reference.ts, layout()). Counting /docs/guides/ and /docs/concepts/ links in the built pages for input/gamepads, scene/scene, render/label, render/camera, app/application and render/renderer gives zero each; module overviews link Guides and Concepts only as roots (moduleReferencePage). A reader landing on Gamepads from search cannot reach guides/input, where app->gamepads() and the binding pattern are; one on Scene cannot reach concepts/tick. The referenceLinks plugin already computes every guide mention (markdown-plugins.ts), so an 'Explained in' list is generator work that fails the build if stale. Not G56 or G111.

**Evidence.**

- website/src/lib/reference.ts, layout()
- website/src/lib/reference.ts, moduleReferencePage
- website/src/lib/markdown-plugins.ts, referenceLinks
- dist content-area count: 0 guide or concept links on input/gamepads, scene/scene, render/label, render/camera, app/application, render/renderer

**Scope check.** Website generator work, not an editor or tooling environment.

<a id="g141"></a>
### G141. Reference generator mislabels ApplicationOptions::max_threads and renders grouped constants as one unusable heading

**minor** · other · E06 · journeys: srl-learn-the-engine-model

Three generator defects, none in the headers. (1) cpp-header.ts declarationName takes the identifier before the first '(' (cpp-header.ts:137-153), so 'int max_threads = default_thread_count();' (engine/app/application.h:71) is published under '### min_threads · default_thread_count': max_threads has no heading or anchor and a free function is named as a member. (2) Colour's named colours share one comment group, giving one h3 listing about 140 names with a 1,726-character anchor that also floods 'On this page'; Vector2F's constants do the same on a smaller scale. (3) The Renderer page publishes the uncommented backend accessor 'Impl* impl() const;' (renderer.h:440) on a page recommended to beginners (related G93). Fixes: strip an '= ...' initialiser in declarationName with a case in website/tests/reference.test.mjs, and cap names per heading.

**Evidence.**

- website/src/lib/cpp-header.ts:137-153 (declarationName)
- engine/app/application.h:70-71
- dist/docs/reference/app/application: heading '### min_threads · default_thread_count'
- dist/docs/reference/render/colour: h3 anchor of 1,726 characters
- engine/render/renderer.h:440 (Impl* impl() const)
- website/tests/reference.test.mjs

**Scope check.** Site generator correctness. Not in the register (G94 is content, not generator output).

<a id="g142"></a>
### G142. The page-to-module contract has no single public statement, no stability marker, and silently ignores a misspelled callback

**minor** · doc-drift · E08 · journeys: oca-ship-desktop-and-browser, sm-close-hardware-and-deploy-milestones

A page mounting an engine module owes it Module.canvas and up to four callbacks, spread over five places: the published window.h names only onQuit and onError (engine/app/window.h:230-247); onContextLost/Restored are described only in the unpublished engine/render/gl/backend.h:192-198; the full list is in comments in samples/linesweeper/web/index.html:22-27, each sample's shell.html, and internal web.md section 7.3 (docs/port/web.md:690-733). grep of website/src for createLineSweeper|onContextLost|Module.canvas finds nothing. Each callback is read with typeof Module.X === 'function' (window.cpp:127-137; web_context.cpp:32-42), so a host writing onerror or onContextlost gets no error and the visitor sees a stopped or blank canvas - the failure index.html:25-27 says the status line exists to prevent. BUILD.json carries no contract marker (tools/web_bundle/bundle.mjs:143-150). Related G09, G51, G18.

**Evidence.**

- engine/app/window.h:230-247 (onError, onQuit only)
- engine/render/gl/backend.h:192-198
- engine/app/web/window.cpp:28-34, 127-137
- engine/render/gl/web_context.cpp:32-42
- samples/linesweeper/web/index.html:22-27
- docs/port/web.md:690-733 (section 7.3)
- grep -rn 'createLineSweeper|onContextLost|Module.canvas' website/src -> no matches
- tools/web_bundle/bundle.mjs:143-150

**Scope check.** One paragraph in the published window.h or on the site closes the documentation half; an optional dev-build warning for unknown on* keys is T6 and adds no engine API. A versioned compatibility promise should wait for G17.

<a id="g143"></a>
### G143. A host cannot stop a running instance; removing its canvas leaves the game loop running unseen

**minor** · api-ergonomics · E08 · journeys: oca-ship-desktop-and-browser

An instance stops only from inside the game: Application::quit reaches Window::close, which cancels the main loop and removes listeners (engine/app/application.cpp:231-234; engine/app/web/window.cpp:485-497), or an exception through fail(). The module exports nothing a page can call (the only EMSCRIPTEN_KEEPALIVE is a test hook, tests/app/web/window_page.cpp:224; samples/linesweeper/main_web.cpp:31-66 exports nothing). The loop runs on requestAnimationFrame (window.cpp:800-805) and a detached canvas makes measure() return false (window.cpp:411-415), so by reading - not observed - a host that removes a playing canvas (SPA route change, closed modal, restart button) keeps the frame loop, the WebGL2 context and window-level mouse listeners. index.html avoids it only by offering Play again after onQuit/onError (index.html:143-152); neither it nor web.md section 7.3 says a page must not drop a running game. Related G114 (stopped instances).

**Evidence.**

- engine/app/application.cpp:231-234
- engine/app/web/window.cpp:407-415, 485-497, 800-805, 363-372
- tests/app/web/window_page.cpp:224
- samples/linesweeper/main_web.cpp:31-66
- samples/linesweeper/web/index.html:15-20, 124-126, 141-159
- docs/port/web.md:719-728

**Scope check.** T1: the one real host never drops a running game, so a host-stop API waits for a client. In proportion: a sentence in index.html and web.md 7.3 and optionally a line in minimal's main_web.cpp showing how to export quit(). Does not touch the seam.

<a id="g144"></a>
### G144. The bundle's safe-deploy checks live only in the host's own repository, and a Debug bundle is written to the same place with nothing to stop it

**minor** · release-process · E08 · journeys: sm-close-hardware-and-deploy-milestones, oca-ship-desktop-and-browser

BUILD.json exists so a host can check what it deploys (web.md:649-651), but the checks using it are only in the owner's PersonalWebsite sync-demo.mjs: refuse dirty, warn on non-Release, verify SHA-256, hash the js/wasm pair into an immutable folder (sync-demo.mjs:11-12,61-82). web.md states only the dirty rule. bundle.mjs writes BUILD.json and never reads it (tools/web_bundle/bundle.mjs:143-151). linesweeper_web_dist is ALL under both wasm presets (samples/linesweeper/CMakeLists.txt:118-151), so wasm-debug writes web-dist/linesweeper/ too: the one in this checkout is Debug, dirty, with linesweeper.wasm at 11,634,781 bytes (2,073,836 Brotli) against the 394 KB/123 KB release figures (web.md:633-634). Any other host - the owner on a fresh machine, or an adopter copying the shape - has no in-tree way to catch a dirty or Debug bundle, a missing file, or a mismatched js/wasm pair, which fixed names and the relative import (index.html:122) make easy under caching. Related G51, G68, G17.

**Evidence.**

- out/build/wasm-debug/web-dist/linesweeper/BUILD.json (build_type Debug, dirty true, linesweeper.wasm 11634781 bytes / 2073836 brotli)
- docs/port/web.md:630-651, 672-681
- samples/linesweeper/CMakeLists.txt:118-151
- tools/web_bundle/bundle.mjs:143-151
- C:\VSProjects\PersonalWebsite scripts/sync-demo.mjs:11-12, 61-82
- samples/linesweeper/web/index.html:122

**Scope check.** A documented check or a small verify mode on bundle.mjs, not a deploy pipeline; automated cross-repo deploy would fight web.md section 7 and T1.

<a id="g145"></a>
### G145. MSVC reads the adopter's non-ASCII L"..." literals in the system code page, so the same source draws different text on desktop and in the browser

**major** · platform · E04 · journeys: oca-grow-a-game, oca-ship-desktop-and-browser

labrador_settings, the INTERFACE target every game links, gives MSVC /W4 /WX /permissive- /sdl /fp:precise and no /utf-8 (cmake/settings.cmake:15-19). Without a BOM MSVC decodes source in the ANSI code page (1252 here); clang under the wasm presets always reads UTF-8. The guide tells adopters to write text as L"..." (sprites-and-text.mdx:51). Compiling L"café あ" from a UTF-8 file with the template's MSVC flags compiled silently even under /WX and produced 0063 0061 0066 00C3 00A9 0020 00E3 0081 201A (mojibake); with /utf-8 it produced 0063 0061 0066 00E9 0020 3042. So a localised literal is right in the browser and silently wrong on Windows, hidden until an adopter's atlas holds é or あ. No page mentions source encoding. Every engine .cpp/.h is pure ASCII, so adding /utf-8 changes nothing in the engine build. Major: silent, cross-platform divergence that needs unwritten knowledge.

**Evidence.**

- cmake/settings.cmake:15-19 (no /utf-8 or /source-charset anywhere in cmake/ or root CMakeLists.txt)
- website/src/content/docs/docs/guides/sprites-and-text.mdx:51
- Scratchpad test u8test/t.cpp, cl /std:c++20 /W4 /WX /permissive- /sdl, ACP 1252: without /utf-8 -> 0063 0061 0066 00C3 00A9 0020 00E3 0081 201A; with /utf-8 -> 0063 0061 0066 00E9 0020 3042
- Python scan of engine/**/*.cpp,*.h for bytes > 0x7F -> 0 files

**Scope check.** One compile option on the shared settings target, load-bearing like /fp:precise; serves T6 (same source, same text). No new mechanism (T1).

<a id="g146"></a>
### G146. Nothing walks an adopter from 'my game is not in English' to text on screen

**major** · onboarding · E04 · journeys: oca-grow-a-game

widen(), read_json_file, can_render, keyboard.typed() and a reader for any MakeSpriteFont atlas exist; no guide joins them. (1) The template's only atlas holds 95 glyphs, U+0020-U+007E (parsed from the file header), so é, ß, ñ or kana draw as '?'; the guide says only that a font draws the characters it was made with (sprites-and-text.mdx:52), and that MakeSpriteFont defaults to ASCII is only in text_encoding.h:21-22 and tests/render/font_tests.cpp:320-321. (2) No page says a new atlas needs MakeSpriteFont's character-region option or that one atlas is one pixel size (the localisation half of G12's fix). (3) widen() bridges UTF-8 from JSON string tables and keyboard.typed() to the wstring text API, and appears in neither sprites-and-text.mdx nor input.mdx (input.mdx:38-41 hands the reader UTF-8 with no link); no sample calls typed(), can_render or widen(). (4) No page suggests calling can_render on every localised string at load, its stated purpose (render_resources.h:189-193). A localising adopter finds out at runtime that players see question marks. Related G12, G107.

**Evidence.**

- samples/minimal/content/fonts/courier_new_bold_16.spritefont header: 95 glyphs, chars 0x20-0x7E
- website/src/content/docs/docs/guides/sprites-and-text.mdx:19, 51-54
- website/src/content/docs/docs/guides/input.mdx:38-41
- website/src/content/docs/docs/concepts/resources.mdx:101-105
- engine/render/text_encoding.h:17-29
- engine/render/render_resources.h:189-193
- grep -rn 'widen|typed()|can_render' samples website/src/content -> no use

**Scope check.** Documentation over existing primitives. PHILOSOPHY.md:601-602 already places text in data (T7), so JSON -> widen -> can_render at load is the design as written. A string-table facility would need its own T1 argument and is not proposed.

<a id="g147"></a>
### G147. Which scripts and characters the text path can draw at all is not stated anywhere public

**minor** · doc-drift · E04 · journeys: oca-evaluate-and-decide, oca-grow-a-game

The glyph walk (engine/render/font.h:147-190) advances one UTF-16 code unit at a time with x_advance and left bearing only: no kerning, shaping, right-to-left ordering, combining marks or surrogate-pair decoding. Arabic, Hebrew, Devanagari, Thai and emoji cannot be drawn correctly with any atlas; Latin with precomposed accents, Cyrillic, Greek and CJK can if the atlas holds them. That T1/T3 boundary is recorded only as a test (tests/render/font_tests.cpp:309-345) and a closed review item (docs/review/backend-equivalence-2/STATUS.md G5b). Cross-platform inconsistency: widen() always produces surrogate pairs but wchar_t is 32-bit under clang (text_encoding.cpp:54-58), so an astral L"..." literal is one unit in the browser and two on Windows, changing first_unrenderable's index and the stand-in count (font.h:119-129; render_resources.h:197-200). An adopter planning a localised release has no public statement of which languages are in reach.

**Evidence.**

- engine/render/font.h:147-190
- engine/render/font.h:119-129 and render_resources.h:197-200
- tests/render/font_tests.cpp:309-345
- engine/render/text_encoding.cpp:54-69
- grep -i 'locali|unicode|right-to-left' website/src/content/docs and docs/design -> no statement of text scope

**Scope check.** Stating the boundary conflicts with nothing; shaping and bidi would be a large mechanism needing a T1 demand argument and T3 favours the simpler model. Documenting literals beyond the BMP as unsupported fits T6.

<a id="g148"></a>
### G148. No security policy and no private vulnerability-reporting route

**minor** · missing-capability · E10 · journeys: oca-evaluate-and-decide, pi-report-a-gap-upstream

The repository is public and offered as the base for shipped games, yet has no SECURITY.md and GitHub private vulnerability reporting is off ({"enabled":false}). The only channel is the public issue tracker (see G21), so a memory-safety bug in a content reader, the JSON manifest loader or the web loader can only be disclosed publicly. No document, header or page states whether content files (.dds, .spritefont, .xwb, manifest.json) are trusted input, which matters to an adopter loading mods, user levels or fetched content. The readers are hardened and tested against malformed input, so the gap is the undeclared posture and the missing route, not known-unsafe code; no fuzzing or sanitizer configuration backs a stronger claim. A few-line SECURITY.md plus enabling private reporting costs almost nothing.

**Evidence.**

- git ls-files | grep -iE 'security|contribut|code_of_conduct|issue|dependabot|codeowners' -> no policy or template files
- gh api repos/matt439/Labrador/private-vulnerability-reporting -> {"enabled":false}
- gh api repos/matt439/Labrador -> has_issues true, has_discussions false, dependabot_security_updates disabled
- grep -rniE 'trusted|untrusted|threat|attacker' engine docs/design website/src/content/docs README.md -> only engine/render/viewport.h:55 (unrelated)
- grep -rniE 'fuzz|sanitiz|ASan' cmake CMakePresets.json .github/workflows/ci.yml -> no matches

**Scope check.** Not speculative code, so T1 does not argue against it. Distinct from G21 (questions), G54 (report record) and G115 (dependabot for pins).

<a id="g149"></a>
### G149. LICENSE carries a stale addendum that breaks licence detection and names code not in the tree

**minor** · doc-drift · E01 · journeys: oca-evaluate-and-decide, pi-ship-windows-release

LICENSE is the MIT text followed by a paragraph saying the licence covers 'the engine, the sample, the paint-shooter's source and the documentation' and pointing to NOTICE for undistributed content (LICENSE:22-28). The paint-shooter has not been here since the split, and NOTICE:6-9 says those sections went with game/ to its own repository. Because of the addendum GitHub reports the licence as NOASSERTION rather than MIT, so an adopter's legal or dependency scanner sees 'unknown licence' though the site says MIT (website/src/content/docs/index.mdx:32). The file has not changed since 2a75ae5; the fix is to delete or move one paragraph.

**Evidence.**

- LICENSE:22-28
- NOTICE:6-9
- gh api repos/matt439/Labrador -> license.spdx_id 'NOASSERTION'
- website/src/content/docs/index.mdx:32 ('MIT licence')
- git log --oneline -- LICENSE -> 2a75ae5 only

**Scope check.** Not G16 (shipped-binary notices) or G55 (GitHub About text). No non-goal touched.

<a id="g150"></a>
### G150. No guidance on which render backend a shipped Windows game should build, or what to do when the player's machine refuses it

**major** · doc-drift · E07 · journeys: oca-ship-desktop-and-browser, pi-ship-windows-release

The backend is compile-time by design (T5, T8), so a shipping adopter must choose, and nothing tells them which. The site lists five with no recommendation (build-labrador.mdx:58-67); README and PHILOSOPHY explain four exist to prove the seam (README.md:75-77; PHILOSOPHY.md:349-352) without drawing the conclusion. The only shipped game chose privately (ColourWars cmake/release_package.cmake:14-16: d3d11 and xaudio2 only). The template's presets set no LABRADOR_RENDER_BACKEND and your-project.mdx never says that variable is how a consumer picks (website/src/walkthrough/CMakePresets.json). With no in-process fallback, the release d3d11 path throws 'No Direct3D hardware device found' (engine/render/d3d11/device_resources.cpp:161-165), shown raw in a MessageBoxA (samples/minimal/main.cpp:58-60); the site's advice, update the driver or use a debug build (troubleshooting.mdx:96-98), is a developer remedy. The T5-consistent option of shipping two executables (d3d11 plus gl) is unmentioned. Inferred: a Vulkan build links the loader implicitly (engine/CMakeLists.txt:241) and would fail in the OS loader before main with no T6 message. Floors are G105.

**Evidence.**

- website/src/content/docs/docs/get-started/build-labrador.mdx:58-70
- website/src/walkthrough/CMakePresets.json (no LABRADOR_RENDER_BACKEND)
- grep -niE 'backend|renderer' website/src/content/docs/docs/get-started/your-project.mdx -> no matches
- README.md:75-87, 128-135
- docs/design/PHILOSOPHY.md:339-352
- engine/render/d3d11/device_resources.cpp:161-165
- website/src/content/docs/docs/troubleshooting.mdx:96-98
- samples/minimal/main.cpp:54-62
- engine/CMakeLists.txt:121-124, 241
- ColourWars cmake/release_package.cmake:14-16

**Scope check.** A shipping note, not runtime switching, which T5/T8 rule out. Cites G105 for floors; G50 and G82 cover the browser.

<a id="g151"></a>
### G151. Windows DPI awareness is neither declared nor decided

**minor** · platform · E07 · journeys: oca-ship-desktop-and-browser

The Win32 window code calls no DPI API (no SetProcessDpiAwarenessContext, GetDpiForWindow or WM_DPICHANGED; grep for 'dpi' in engine/app finds only the web window's devicePixelRatio at engine/app/web/window.cpp:160), and no sample or engine target embeds an application manifest. A shipped game is therefore DPI-unaware, and on 125-150% scaled laptop displays Windows bitmap-stretches it, blurring the point-sampled text the pixel contract and capture README take care to place on whole pixels. Unaware may be the right call for the low tier (fewer pixels), but no document records either choice or tells an adopter whether to add a manifest. Not observed on a scaled display; inferred from the absent calls.

**Evidence.**

- grep -rniE 'dpi' engine/app -> only engine/app/web/window.cpp:160
- grep -rnE '_WIN32_WINNT|SetProcessDpiAwarenessContext|GetDpiForWindow|\.manifest|supportedOS' engine cmake samples CMakeLists.txt -> only engine/render/d3d11/device_resources.cpp:427
- engine/app/win32/pointer_input.cpp:22-31 (WM_POINTER, Windows 8+, undeclared floor; see G105)

**Scope check.** Not in the register; G105 covers GPU and OS floors, not display scaling. Not a non-goal: a decision plus a sentence, or a manifest.

## Left to another persona, or deliberately not storied

| Gap | Persona that passed on it | Why |
|---|---|---|
| G05 | The Outside C++ Adopter | Serves the sole-maintainer and browser-demo-player most: ticking deploy checks and amending records is the owner's deploy work (web.md section 7). The adopter's value (a link to try the demo) is OCA-03. |
| G22 | The Outside C++ Adopter | Serves the sample-reading-learner most: the Introduction to C++ series is T10's beginner on-ramp, ranked below the primary persona's work. |
| G24 | The Outside C++ Adopter | Serves the sole-maintainer most (E11): the low-tier measurement is hardware work blocked on G01. The adopter-facing floors are OCA-64; publishing a speed figure follows the measurement. |
| G52 | The Outside C++ Adopter | Folded into OCA-23: the adopter's value is the corrected public statement; the web.md amendment rides along. Not a separate story. |
| G53 | The Outside C++ Adopter | Serves the pinned-integrator most: stamping an engine revision into a shipped build is their release journey (pi-ship-windows-release); the adopter's version need is OCA-67. |
| G54 | The Outside C++ Adopter | Serves the pinned-integrator most: a public record of reports and their disposition is their report-a-gap journey; the adopter's intake need is OCA-72. |
| G57 | The Outside C++ Adopter | Serves the sole-maintainer most: the site key, canonical URLs and sitemap are site operations; the adopter benefits only indirectly through search. |
| G115 | The Outside C++ Adopter | Serves the sole-maintainer most: a dependency and toolchain bump procedure is engine maintenance. The adopter's half (the derived manifest at their pin) is in OCA-70. |
| G126 | The Outside C++ Adopter | Serves the sole-maintainer most: the promotion rule is the owner's decision rule for engine/ui. An adopter copies a component either way; a T3 amendment or uptake record is maintainer work. |
| G137 | The Outside C++ Adopter | Serves the sole-maintainer most: keeping footage in step with the sample is a capture-and-commit rule under E12; the evaluator's harm is small and goes away when the rule holds. |
| G144 | The Outside C++ Adopter | Serves the sole-maintainer most: bundle.mjs and its BUILD.json checks are LineSweeper's host bundle for the owner's site. The adopter has no bundle of their own; their hosting and notices needs are OCA-23 and OCA-63. |
| G07 | The Pinned Submodule Integrator | Left to outside-cpp-adopter. The missing manifest, preset and fxc instructions block a first-time consumer; this persona already has a working manifest and preset, so the value does not differ. Manifest drift at a pin move is covered by PI-03. |
| G106 | The Pinned Submodule Integrator | Left to outside-cpp-adopter. The false pointer to samples/minimal misleads someone learning the consumer shape; this persona already builds as a consumer, so it gains nothing different. |
| G38 | The Pinned Submodule Integrator | Left to outside-cpp-adopter. This persona already holds the recipe for running the engine suite at its pin (ColourWars CLAUDE.md:108-115); the value of putting it upstream goes to an adopter without it. |
| G110 | The Pinned Submodule Integrator | Left to outside-cpp-adopter. This persona already runs its own CI (ColourWars CLAUDE.md:94-121); a copyable workflow is worth the same to it and more to a new adopter. |
| G105 | The Pinned Submodule Integrator | Left to outside-cpp-adopter. This persona has already chosen d3d11/xaudio2 and enforces it (ColourWars cmake/release_package.cmake:13-16), so the published hardware and OS floors serve it no differently from any shipping adopter. |
| G150 | The Pinned Submodule Integrator | Left to outside-cpp-adopter. The backend choice is already made privately here (release_package.cmake:13-16). Advice on which backend to ship, and on shipping two executables (T5/T8), serves the adopter who has not chosen yet. |
| G151 | The Pinned Submodule Integrator | Left to outside-cpp-adopter. A DPI-awareness decision or manifest helps every Windows game the same way, and this persona's need is no different. |
| G145 | The Pinned Submodule Integrator | Left to outside-cpp-adopter. This persona is Windows-only with no browser build, and the /utf-8 fix is about the desktop and the browser drawing the same text. If the flag reaches this persona's build when the pin moves, PI-01's changes list covers that. |
| G148 | The Pinned Submodule Integrator | Left to outside-cpp-adopter and sole-maintainer. A private vulnerability route serves an outside reporter. This persona is the engine's own author and needs no private channel, and its request route is PI-07. |
| G107 | The Sole Owner-Maintainer | Serves the outside-cpp-adopter most: they ship the template's Courier New atlas. The fix (a licence note or an openly licensed replacement atlas) is one change, and it also covers the hosted demo, so a sole-maintainer story would duplicate the same value. |
| G141 | The Sole Owner-Maintainer | Reference-generator defects hurt readers (outside-cpp-adopter, sample-reading-learner) and sit in E06. The maintainer gets no distinct value from the same fix, so it belongs to those personas. |
| G142 | The Sole Owner-Maintainer | The page-to-module contract statement serves an outside host most (E08). The owner's own host already uses the full contract, so the maintainer's value is the same fix with no distinct outcome. |
| G143 | The Sole Owner-Maintainer | Left to outside-cpp-adopter. T1: the one real host never drops a running game, so a host-stop API waits for a client. The proportionate sentence in index.html and web.md 7.3 serves adopters copying the shape, not the maintainer. |
| G52 | The Browser and Phone Demo Player | The fix amends web.md section 7.2's claim about the wasm MIME type; the value is to a host or adopter writing a page, not to a player, who sees the game run either way. Leave to outside-cpp-adopter (E08). BDP-02 still checks the header on the live URL. |
| G05 | The Browser and Phone Demo Player | Partly storied: BDP-02 covers the visitor-facing deploy checks. The records half (web.md:616-618 and :1461 still saying undeployed, M5 status) serves the sole-maintainer and should be storied there. |
| G23 | The Browser and Phone Demo Player | Partly storied: BDP-05..07 and BDP-04 cover Safari, Firefox, a physical pad, pacing and context loss. Audible Web Audio does not apply because LineSweeper has no audio (pause_state.cpp:309-311). The 'which browsers are supported' statement for adopters' players goes to outside-cpp-adopter (E08). |
| G02 | The Sample-Reading Learner | Same fix serves the outside-cpp-adopter most: one platform statement across the front door is the evaluator's yes/no. The learner's need (seeing the sample off Windows) is covered by SRL-01 and SRL-02. |
| G44 | The Sample-Reading Learner | Amending PHILOSOPHY.md:4, :449, :717 and ARCHITECTURE's preset count gives the learner the same value it gives the adopter and maintainer; left to outside-cpp-adopter (E01) and sole-maintainer. |
| G46 | The Sample-Reading Learner | Emscripten setup on the site serves the outside-cpp-adopter shipping to the browser. The learner's need to see the game run is met more cheaply by hosted pages (SRL-01, SRL-13) than by a local wasm build. |
| G109 | The Sample-Reading Learner | The IDE and debugger section is one Get Started page whose value is the same for both personas; it serves the outside-cpp-adopter growing a game most. |
| G112 | The Sample-Reading Learner | Browser-build troubleshooting serves the adopter running wasm presets; same fix for the learner. Left to outside-cpp-adopter (E08). |
| G116 | The Sample-Reading Learner | One default constant or one menus-guide sentence; the value (a correct disabled grey and guide) is the same for the learner, and it serves the adopter building menus most (E05). |
| G117 | The Sample-Reading Learner | Changing the landing shadow's contrast serves the browser-demo-player; the README line recording why belongs in the same commit, so storying it here would duplicate. Left to browser-demo-player (E09). |
| G118 | The Sample-Reading Learner | A reduced-motion path for the particle bursts serves the browser-demo-player's safety; the learner gains only the same sample-side pattern. Left to browser-demo-player (E09). |
| G138 | The Sample-Reading Learner | Documenting that layer_depth is ignored is the same three sentences for both personas and serves the adopter writing draws most; left to outside-cpp-adopter (E06). |
| G146 | The Sample-Reading Learner | Walking from non-English text to the screen serves the localising adopter; the learner gets the same guide. Left to outside-cpp-adopter (E04). |
