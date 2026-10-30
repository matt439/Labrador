# Labrador Website Plan

**Created 4 October 2026 · amended through launch on 8 October 2026**

This plan defines Labrador’s public website: a showcase that explains why a developer would choose the engine, a practical getting-started path, and a complete documentation area. The website lives under `website/` in the Labrador repository and launched on **8 October 2026 at [labradorengine.com](https://labradorengine.com/)**, hosted by Cloudflare Workers Static Assets. The stack, initial coverage, hosting and domain are decided ([website/README.md](../website/README.md)). The repository choice and the requirements below are agreed.

## Purpose and agreed requirements

The website should help a visitor understand Labrador, decide whether it fits their game, build a first project, and return for detailed technical guidance.

Confirmed requirements:

- Showcase the engine and help developers get started.
- Clearly explain how Labrador differs from other engines and programming libraries.
- Provide both Getting Started and full documentation, in distinct sections with different purposes.
- Keep the website in this repository under `website/`, with its build and deployment independent of CMake.
- Plan hosting, domain, and web stack before implementation.

The owner purchased `labradorengine.com` and selected the Cloudflare Workers Paid plan. The exact billing amount and currency are not recorded here. The visual identity and initial documentation scope are settled below.

## Positioning and differentiation

**Proposed positioning:** A focused 2D engine for developers who want to build games in ordinary C++ and understand the machinery beneath them.

The homepage should communicate explicit ownership, freedom over game data, and a deliberate focus on local 2D games, including shared-screen and split-screen play. A dedicated “Why Labrador?” page should explain each choice, its practical benefit, and its cost.

| Message | Concrete demonstration | Trade-off to explain |
| --- | --- | --- |
| Ordinary C++ and standard tools | A small complete example from `minimal`, with its CMake integration | Game behaviour requires C++ and compilation; there is no visual editor or scripting layer. |
| Control over game data and lifetimes | LineSweeper’s copyable world and one particle object containing thousands of values | The game author chooses storage and ownership. The engine is not stack-only or free of virtual calls. |
| A focus on local 2D games | Multiple cameras and viewports drawing one scene, with controller input | Online play and 3D are outside the engine’s intended scope. The local-multiplayer sample demonstrates two players and two cameras in one shared world. |
| Rules that can be tested independently | A LineSweeper rule or replay test alongside its visible result | Separating rules from presentation is a game architecture choice, not an automatic guarantee for every client. |

Compare workflows fairly. For readers considering Godot or Unreal, explain Labrador’s code, ownership, and tooling choices. For readers considering raylib or similar libraries, explain the value of its scene orchestration, state stack, collision handling, resource management, and view system. A coding-only workflow is already part of [raylib’s positioning](https://www.raylib.com/), so “no editor” is insufficient on its own.

Multiple rendering backends and shared pixel tests support the engineering story. Any numerical performance comparison must identify the workload, hardware, configuration, and measurement. Current platform support must be stated as Windows and a desktop browser; Vulkan does not establish support for other operating systems.

Evidence: [design philosophy](design/PHILOSOPHY.md), [Scene interface](../engine/scene/scene.h), and [LineSweeper design and examples](../samples/linesweeper/README.md). The design document describes a target, so public feature claims must also be checked against current code and examples.

## Site structure and documentation

**Proposed navigation:** Home · Why Labrador? · Docs · Examples, with a prominent Get Started action and a source-repository link.

Home introduces the engine, shows real game footage, explains its main differences, and directs visitors to Get Started. Why Labrador? develops the comparisons and trade-offs. Examples explains what to copy from `minimal` and what to learn from LineSweeper.

Docs should contain distinct routes for learning and reference:

| Section | Reader need | Planned content |
| --- | --- | --- |
| Get Started | Build something successfully | Windows prerequisites, a release's starter project built without Git (or, for readers who use Git, clone and configure, run `minimal` and create an independent project), and a first visible change. |
| Concepts | Understand the engine’s model | Ownership, objects and batches, state flow, update and draw, scenes, cameras, views, and resources. |
| Guides | Complete a task | Sprites and text, input, collision, audio, assets, menus, and local multiplayer. |
| API Reference | Find an exact contract | Public types and functions, parameters, lifetimes, errors, and relevant examples, tied to an engine revision. |
| Design | Understand the reasons | Curated philosophy, architecture, conventions, and documented trade-offs. |
| Troubleshooting | Recover from a problem | Toolchain setup, dependency discovery, backend selection, asset errors, and common build failures. |

Full documentation is part of the intended site, not a page of repository links. Build a content inventory across the public modules: math, core, collision, render, scene, input, audio, ui, assets, and app. Record what exists, what needs adapting, and what must be written.

The existing `docs/` tree contains design material, historical reviews, and benchmark evidence; it is not a finished beginner manual. Keep those categories distinguishable. Draft content may be developed in stages, but define the first release’s coverage explicitly and do not label incomplete reference material as complete.

Keep documentation at `/docs/` initially. Show the documented engine version or commit. Provide search, cross-links between guides and reference entries, and an “Edit this page” link to the actual source.

## Repository and content ownership

**Agreed decision:** keep the website under `website/` in the Labrador repository. Full documentation is central to the site, so engine code, examples, and their documentation should be reviewed and updated together. One repository keeps those changes tied to one revision and simplifies publishing.

Content ownership follows the existing tree:

```text
engine/     Engine source and API comments
samples/    Working examples
docs/       Technical documentation and planning
website/    Site build, layouts, styling, showcase copy, and website media
```

- Maintain each technical explanation in one canonical location. API changes update the associated documentation and examples in the same change.
- The website build reads selected documentation, headers, and examples directly from the same checkout. Publish an explicit set of content, keeping historical reviews and planning documents out of the main documentation navigation.
- Handle relative links and referenced assets during site generation, check for broken links, and direct “Edit this page” links to the actual source file.
- Display the engine version or commit used to build the documentation. A preview of a proposed change includes the matching code and documentation revision.
- Keep web dependencies and generated output under `website/`, with a dedicated build and deployment workflow. Building or consuming Labrador through CMake must not require Node or install website dependencies.

The exact technical documentation subfolders can be settled during the content inventory. The website repository decision is closed.

## Web stack

**Decided:** Astro + TypeScript + Starlight, using static output, Markdown for ordinary documentation, and custom Astro components where a demonstration needs them. Start with ordinary CSS and a small shared set of colours, typography, and spacing. The slice is built on Astro 7 and Starlight 0.42; Astro 7’s Markdown processor is Sätteri rather than remark, so the site’s two Markdown plugins are Sätteri plugins.

Astro pre-renders pages by default. Starlight supplies documentation navigation, search, code highlighting, and other documentation features. This supports a custom showcase and structured docs within one site. [Astro rendering](https://docs.astro.build/en/guides/on-demand-rendering/), [Starlight features](https://starlight.astro.build/).

The proposed first version needs static pages, search, images, and recorded footage. Accounts, a database, and server-side application features are not currently requirements. Browser-playable demos are not a feature this site needs: the engine builds for the browser through Emscripten and WebGL2 ([the web port](port/web.md)), and LineSweeper's playable build is a bundle the engine build produces and the owner's personal site, mattharding.dev, serves as a demo page of its own, at <https://mattharding.dev/demo/linesweeper/> ([the web port](port/web.md), section 7; live, checked 13 October 2026). This site links to it from its home and Examples pages, as the README and LineSweeper's README do; a link, not a technical project.

**API reference: decided and published.** A line reader in the site build generates a page for every public header in all ten modules, which needs no tool beyond Node and fails naming the file and line on anything it does not understand. Doxygen does not read Labrador’s plain `//` comments; Clang would have to parse `<Windows.h>`. One header is left out by name, Microsoft’s `StepTimer`, which is private to `Application`, and the build fails on a public header that is neither published nor left out, so a new one cannot quietly go without a page.

The prototype’s finding was in the headers themselves: about two thirds carried history of the code, which CONVENTIONS says is never a comment, and a reference that publishes comments word for word presents that history as contract ([the inventory](website-inventory.md) has the counts). It was settled by taking the history out. Every public header was swept on 4 October 2026, comment-only: contract kept as written, rationale restated in the present tense where history had been carrying it, and the comments the sweep found to be false against the code corrected. Generated symbols still need explanatory guides alongside them, and the Concepts and Guides sections are those. They link into the reference without a link written by hand: a name set as code in a guide, a concept page or a design document links to its section, a member to its own, and each reference page ends with the samples and tests that include its header.

The visual identity is decided: the amber four-cell L and Labrador wordmark,
neutral light and dark surfaces, system sans-serif prose and monospace code.
The mark appears in both the header and favicon; its `#f09800` orange comes
from LineSweeper, with darker or lighter shades for readable link text. The
site uses real sample imagery, clear navigation and a comfortable reading
measure on desktop and phone. The four exact frames of LineSweeper are made
by `tools/linesweeper_capture/` from a scripted match played through the
sample’s rules and read back from the graphics device, so they can be
regenerated byte for byte. Home, Examples and the Menus guide use them.

Home and Examples also show a silent gameplay video, captured by the same tool
from `footage.txt`, one frame per 60 Hz presentation update. Its MP4 is
compressed for playback; the PNG stills retain the exact pixels. The player
has native controls, an existing still as its poster, a download link and a
text description. Playback starts only on request and the video is not
preloaded. Regeneration uses FFmpeg outside the normal site build; the
checked-in media keeps the website and CI independent of the engine toolchain.

## Hosting and domain

**Launched 8 October 2026:** [https://labradorengine.com/](https://labradorengine.com/), served by Cloudflare Workers Static Assets. The owner confirmed the domain purchase and Workers Paid plan. Account billing is the authority for renewal and plan costs; the earlier candidate-domain availability and price comparison is no longer the deployment record.

Cloudflare Workers Builds deploys from `master`, using `website` as the root directory and Node 24. Its build command is `npm ci && npm run check && npm test && npm run build`; its deploy command is `npx wrangler deploy`. The whole checkout remains available because site generation reads engine headers, samples and documentation outside `website/`. The committed [Wrangler configuration](../website/wrangler.jsonc) selects `dist` as the static assets directory. [website/README.md](../website/README.md) records the operational settings. `.github/workflows/website.yml` independently checks and builds the site for each pull request and retains an artifact; production deployment is handled by Cloudflare.

The build command stays the same with documentation versions. `npm run build` mounts every release listed in `website/published-versions.json` into `dist/versions/<tag>/`, so once a release is listed, Cloudflare's build downloads its snapshot from the GitHub release asset. A failed download or verification fails the build: nothing deploys, and the previous deployment stays live. This prevents a failed build from replacing production; recovery from a regression that already deployed is a separate action, a Cloudflare rollback, in the [preview and recovery procedure](../website/README.md#preview-and-recovery). Production deploys therefore depend on GitHub's release downloads. One deployment holds at most 100,000 files under Workers Paid; assembly refuses more and warns at 80,000. The checks that need history and the GitHub API run in the website workflow, not in Cloudflare's build. [The versioning policy](website-versioning.md) owns the procedure.

The live HTTPS homepage and hosted MP4 were checked on 8 October. The MP4 has a video stream and no audio track, matching the documented silent capture. This was a launch check, not a repeat of all the local browser and accessibility validation recorded below.

`site` in `website/astro.config.ts` is `https://labradorengine.com`, the one canonical host, so canonical URLs and the sitemap name it; the `workers.dev` address is turned off in the Wrangler configuration.

## Delivery and maintenance

The stages below record delivery through the 8 October 2026 launch and the ongoing maintenance work.

1. **Settle the foundation.** Choose stack, host, domain budget, and initial documentation coverage. Inventory the available guides, source comments, screenshots, and footage within the agreed repository layout. *Complete: stack chosen, inventory taken ([website-inventory.md](website-inventory.md)), coverage agreed, and Cloudflare Workers Static Assets with `labradorengine.com` selected. The owner purchased the domain and Workers Paid plan.*
2. **Build a representative slice.** Create a homepage section, a Why Labrador? example, one Get Started walkthrough, and one API reference sample. Use these to validate visual design, reading documentation from the same checkout, and the reference-generation approach. *Built: home, Why Labrador?, Examples, the full five-page Get Started path, the three design documents read from the checkout, and three reference pages. The walkthrough’s project was built from the public repository exactly as written.*
3. **Complete the agreed first release.** Write and review the selected guides and reference coverage, capture real sample media, and connect examples to their source. Keep the full documentation goal visible in the content inventory. *Written for the first release: five Concepts pages, seven Guides, a Troubleshooting page and the whole API reference, with four LineSweeper captures and a silent gameplay video. Collision, audio and local-multiplayer guides quote dedicated runnable samples, including original playable audio content.*
4. **Validate and publish.** Check the new-user walkthrough on the named Windows toolchain; verify links, code snippets, search, keyboard navigation, narrow layouts, and contrast. Configure the chosen domain and deployment workflow when moving from planning into implementation. *Validated locally on 4 October 2026, the walkthrough having been built in stage 2 and the code snippets being quoted from the checkout: every internal link and anchor resolves; search finds reference types, members and guide topics; the tab order starts at the skip link and every stop shows a focus ring; no page scrolls sideways at 360 pixels; and axe-core finds no WCAG 2.2 A or AA violation on any page in either theme. That check found dark-theme text below 4.5:1 in four places and wide tables a keyboard could not scroll, and both are fixed. Published on 8 October through Workers Builds at `https://labradorengine.com/`; live checks are scoped in Hosting and domain above. The production `site` is `https://labradorengine.com`, which gives every page its canonical URL and the build its sitemap, and the Worker serves no `workers.dev` copy.*
5. **Maintain with engine changes.** Update canonical technical content alongside relevant code, [review the built artifact and affected pages before merging](../website/README.md#preview-and-recovery), and retain a known working deployment and source revision for recovery. The same procedure owns post-deploy checks and the Cloudflare rollback steps.

The 8 October footage addition was checked separately: the Release capture
build passed, all four existing stills regenerated byte for byte, and the
video contains all 2,095 captured frames at 60 fps (34.917 seconds, 4.3 MB).
The site still builds 121 pages with valid internal links and a clean type
check. Home and Examples passed browser checks at desktop and 360-pixel
widths in both themes: no automatic media download, keyboard playback and
seeking, replay, download links, text descriptions and no horizontal overflow.
The accessibility scan found no violations; its manual caption check was
resolved against the absence of an audio track and the supplied text
alternative. Continuous playback reached the end without a media error.
Visual review also caught a clipped site title on narrow splash pages; their
navigation now takes a second header row. Home, Why, Examples and Get Started
passed header checks at 320, 360, 430 and 1440 pixels in both themes, with the
full title visible and no overflow or automated accessibility violations.

A second 8 October pass added an overview for each of the ten API modules.
The reference's module table and sidebar now lead to those overviews, each
listing the same public headers as the publishing inventory; all 93 header
pages link back to their module. The site builds 131 pages with valid internal
links and a clean type check. All ten overviews passed browser checks at 320
and 1440 pixels in both themes, including sidebar selection, search and
keyboard scrolling of wide tables. Each module-to-header-to-module navigation
path was exercised. Home, Why and Examples also gained the theme selector
their narrow headers lacked. At 320, 360, 430, 799, 800 and 1440 pixels in both
themes, the title remains visible, theme selection is keyboard-accessible and
survives reload, and the pages have no horizontal overflow or automated WCAG
2.2 A/AA violations. Desktop and phone screenshots were reviewed.

The remaining three guides were completed on 8 October with dedicated
`samples/collision`, `samples/audio` and `samples/local_multiplayer` executables.
Their code is quoted directly by the guides. Collision demonstrates filtering,
wall separation and trigger entry; audio includes original source WAVs and a
required named XWB bank; local multiplayer demonstrates two input owners and
two following views of one shared simulation. Headless tests cover the sample
behavior, with audio calls and per-view drawings observable under the null
backends.

Validation passed in D3D11 and null Debug/Release builds: 17 CTest entries per
D3D11 configuration and 16 per null configuration. All three finite sample
checks passed from an unrelated working directory in each configuration,
including actual XAudio2 bank playback and loop play/pause/resume/stop on this
machine. Collision and split-screen captures were reviewed. The audio source
generator reproduces the WAVs, and its authoring verifier matches every named
bank entry to the source PCM. Manual audio UI review confirmed the initial
layout; the keyboard/focus retest was stopped with Escape and is not claimed
as verified.

The website builds 134 pages with valid internal links and a clean type check.
The three guides, guide index and Examples passed 20 browser combinations:
1440- and 360-pixel widths, light and dark themes, no horizontal overflow or
automated WCAG 2.2 A/AA violations, and keyboard access to the skip link.
Desktop and phone screenshots were reviewed. These checks preceded the launch
recorded under Hosting and domain above.

The 8 October polish pass added API links inside C++ code blocks, including
sample excerpts and header-comment examples, and links for qualified or
unambiguous free-function names. Same-header overloads share a destination;
ambiguous names stay plain. Code links preserve highlighted text and copy
payloads, with one link per destination per block and visible keyboard focus.
The amber L identity is settled and the mark now accompanies the header's
wordmark. The footer identifies the current docs as development/unreleased;
[the release policy](website-versioning.md) defines the immutable snapshots
that start with `v0.3.0`.

Validation passed: 12 focused parser/index/rendering tests, a clean type check,
and a 134-page build with valid internal links. Eight representative routes
(Home, Why, Examples, Get Started, Collision, About the reference, UTF-8 paths
and Scene) passed 32 browser combinations at 320 and 1440 pixels in both
themes. Checks covered the complete header title and mark, horizontal overflow,
WCAG 2.2 A/AA automated checks, skip-link focus, API-link keyboard focus, copy
buttons and keyboard navigation to a function anchor. Desktop and phone
screenshots were reviewed. The documentation citation check also passed.
This pass changed website code and documentation; it did not change or
requalify engine binaries.

Proposed readiness criteria:

- A new visitor can identify what Labrador is, who it suits, its current platform support, and its meaningful trade-offs.
- Get Started takes a fresh setup through a running sample and an independently owned project.
- The website contains the agreed documentation coverage, with visible source revisions and clear boundaries around unfinished material.
- Differentiation claims link to examples or evidence; performance figures identify what was actually measured.
- Navigation, search, code blocks, images, and links work on desktop and phone.
- Content rights and attribution are checked for selected media. LineSweeper is the initial public showcase candidate; use of ColourWars assets is a separate choice because that client repository is private.

Keep the site’s build independent of the Windows engine build. Validate examples through the engine’s appropriate checks, then use web checks for page generation, links, and presentation. Review the website workflow's built artifact before merging, using the [preview and recovery procedure](../website/README.md#preview-and-recovery); Cloudflare preview builds remain off. That procedure also owns the live checks after deployment and the rollback steps.

## Decisions and remaining work

The requirements and implementation choices are established. The remaining launch follow-up is recorded alongside the settled decisions.

| Decision | Current recommendation or next action |
| --- | --- |
| Stack | **Decided:** Astro + TypeScript + Starlight, validated by the slice. |
| Hosting | **Live:** Cloudflare Workers Static Assets, deployed by Workers Builds from `master`; the owner selected Workers Paid. |
| Domain and budget | **Purchased and live:** `labradorengine.com`. Exact billing amount and currency are not recorded here. |
| Documentation release scope | **Agreed** as [the inventory’s last table](website-inventory.md) proposed. Written, including the three guides backed by the collision, audio and local-multiplayer samples. |
| API reference | **Decided:** the line reader, with history taken out of the headers first. Every public header is published. |
| Visual identity and media | **Decided:** amber four-cell L mark and Labrador wordmark, neutral light/dark surfaces, system sans-serif and monospace type. The mark is shared by the header and favicon; text uses contrast-safe amber shades. Original sample captures and footage supply the imagery. |
| Version and publishing policy | **Decided 2026-10-27:** `v0.3.0` gets the first complete immutable snapshot; `v0.1.0` and `v0.2.0` are not backfilled. The current site stays development documentation. `SITE#3` implements and verifies versioning before `v0.3.0` is cut; no release date is set. [The versioning policy](website-versioning.md) owns the routes and checks. The manifest, assembly, release index and selector exist; no snapshot is published yet. |
| Production URL metadata | **Done:** `site` in `website/astro.config.ts` is `https://labradorengine.com`; pages carry canonical links and the build writes a sitemap. `wrangler.jsonc` turns off the `workers.dev` address, so `labradorengine.com` is the only host. |

The site is published. The collision, audio and local-multiplayer guides and their worked examples are included; ongoing changes follow the maintenance stage above.

## 2026-10-26 footage refresh evidence

SM-32's fresh D3D11 Release capture completed all 2,095 frames on the desktop
RTX 5080. The encoder verified a silent 1280x720 H.264, YUV 4:2:0 candidate
at 60 fps: 34.917 seconds, 2,435,921 bytes, SHA-256
`33CD03BEA37B9AC035C2E71379B613785ED8ECD15393F5149FB6ED36EFE02255`.
The separate canonical still script regenerated `linesweeper-play.png`,
`linesweeper-pause.png`, `linesweeper-clear.png` and `linesweeper-top-out.png`
byte for byte against the checked-in assets. Selected footage frames were
inspected, but whole-clip playback review remains outstanding. The candidate
is retained locally and the checked-in MP4 has not been replaced; SM-32 stays
open. [The media procedure](../website/README.md) now states the same-commit
regeneration rule and supports encoding to a review path before replacement.

On 2026-10-27 BDP-21 changed the landing shadow to an outline, so the stills and
the footage were regenerated under that rule and the 2026-10-26 candidate is
superseded. The D3D11 Release capture on the RTX 5080 completed all 2,095 frames.
The encoder verified a silent 1280x720, 60 fps H.264 MP4 of 34.917 seconds,
2,471,464 bytes, SHA-256
`C7C8EBFDAB05CEB8E66C2B4C63C427BFE5E65923480D808781C5F60BF1B1D6D7`, which
replaces the checked-in `linesweeper.mp4`. `linesweeper-play.png`,
`linesweeper-pause.png` and `linesweeper-clear.png` changed and were each
looked at; `linesweeper-top-out.png`, which has no falling piece, regenerated
byte for byte. Selected footage frames were inspected, and the owner watched
the whole clip on 2026-10-27, which closes SM-32.

On 2026-10-28 BDP-25 added the pause menu's COLOURS row. The same D3D11 Release
capture regenerated `linesweeper-play.png`, `linesweeper-clear.png` and
`linesweeper-top-out.png` byte for byte; only `linesweeper-pause.png` changed,
and it was looked at. `footage.txt` never pauses, and all 2,095 of its frames
matched BDP-21's capture byte for byte, so `linesweeper.mp4` was not re-encoded.
