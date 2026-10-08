# Labrador Website Plan

**Planning draft · 4 October 2026 · amended through stage 4, and on 8 October 2026 for sample footage**

This plan defines Labrador’s public website: a showcase that explains why a developer would choose the engine, a practical getting-started path, and a complete documentation area. The website lives under `website/` in the Labrador repository. The stack and the first release’s coverage are decided, and most of that release is written ([website/README.md](../website/README.md)); hosting and the domain remain open. The repository choice and the requirements below are agreed.

## Purpose and agreed requirements

The website should help a visitor understand Labrador, decide whether it fits their game, build a first project, and return for detailed technical guidance.

Confirmed requirements:

- Showcase the engine and help developers get started.
- Clearly explain how Labrador differs from other engines and programming libraries.
- Provide both Getting Started and full documentation, in distinct sections with different purposes.
- Keep the website in this repository under `website/`, with its build and deployment independent of CMake.
- Plan hosting, domain, and web stack before implementation.

There is no existing domain or hosting account selected for this project. Domain, budget, branding, implementation dates, and launch scope remain open.

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

Multiple rendering backends and shared pixel tests support the engineering story. Any numerical performance comparison must identify the workload, hardware, configuration, and measurement. Current platform support must be stated as Windows; Vulkan does not establish support for other operating systems.

Evidence: [design philosophy](design/PHILOSOPHY.md), [Scene interface](../engine/scene/scene.h), and [LineSweeper design and examples](../samples/linesweeper/README.md). The design document describes a target, so public feature claims must also be checked against current code and examples.

## Site structure and documentation

**Proposed navigation:** Home · Why Labrador? · Docs · Examples, with a prominent Get Started action and a source-repository link.

Home introduces the engine, shows real game footage, explains its main differences, and directs visitors to Get Started. Why Labrador? develops the comparisons and trade-offs. Examples explains what to copy from `minimal` and what to learn from LineSweeper.

Docs should contain distinct routes for learning and reference:

| Section | Reader need | Planned content |
| --- | --- | --- |
| Get Started | Build something successfully | Windows prerequisites, clone and configure, run `minimal`, create an independent project, and make a first visible change. |
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

The proposed first version needs static pages, search, images, and recorded footage. Accounts, a database, and server-side application features are not currently requirements. Browser-playable engine demos would be a separate technical project because Labrador currently targets Windows.

**API reference: decided and published.** A line reader in the site build generates a page for every public header in all ten modules, which needs no tool beyond Node and fails naming the file and line on anything it does not understand. Doxygen does not read Labrador’s plain `//` comments; Clang would have to parse `<Windows.h>`. One header is left out by name, Microsoft’s `StepTimer`, which is private to `Application`, and the build fails on a public header that is neither published nor left out, so a new one cannot quietly go without a page.

The prototype’s finding was in the headers themselves: about two thirds carried history of the code, which CONVENTIONS says is never a comment, and a reference that publishes comments word for word presents that history as contract ([the inventory](website-inventory.md) has the counts). It was settled by taking the history out. Every public header was swept on 4 October 2026, comment-only: contract kept as written, rationale restated in the present tense where history had been carrying it, and the comments the sweep found to be false against the code corrected. Generated symbols still need explanatory guides alongside them, and the Concepts and Guides sections are those. They link into the reference without a link written by hand: a name set as code in a guide, a concept page or a design document links to its section, a member to its own, and each reference page ends with the samples and tests that include its header.

The visual direction is provisional: readable C++ examples and actual sample imagery, with clear navigation and comfortable reading on desktop and phone. The imagery is four exact frames of LineSweeper, made by `tools/linesweeper_capture/` from a scripted match played through the sample’s rules and read back from the graphics device, so they can be regenerated byte for byte. The home page, Examples and the Menus guide use them.

Home and Examples also show a silent gameplay video, captured by the same tool
from `footage.txt`, one frame per 60 Hz presentation update. Its MP4 is
compressed for playback; the PNG stills retain the exact pixels. The player
has native controls, an existing still as its poster, a download link and a
text description. Playback starts only on request and the video is not
preloaded. Regeneration uses FFmpeg outside the normal site build; the
checked-in media keeps the website and CI independent of the engine toolchain.

## Hosting and domain

**Hosting recommendation, still open:** Cloudflare Workers Static Assets. GitHub Pages is the main alternative if keeping publishing close to GitHub is preferred. The site is plain static output, so neither choice changes it. `.github/workflows/website.yml` checks and builds it on every push and keeps the result as an artifact; it deploys nothing.

Cloudflare currently charges nothing for static asset requests and storage; build and platform limits still apply, while requests that invoke application code have separate pricing. A small static site can therefore target zero recurring hosting cost within those limits. GitHub Pages supports custom domains and is available for public repositories on GitHub Free. [Cloudflare billing](https://developers.cloudflare.com/workers/static-assets/billing-and-limitations/), [GitHub Pages](https://docs.github.com/en/pages/getting-started-with-github-pages/what-is-github-pages).

**Domain candidates, checked 4 October 2026:** none of `labradorengine.com`, `labradorengine.dev` or `labradorengine.org` is registered; each registry’s RDAP service answered *not found*. (`labradorengine.io` is also free; `labrador.dev` is taken.) A name the registry does not hold can still be reserved or premium-priced, which only a registrar’s own search shows, so the price is confirmed at the point of purchase.

| Name | Cloudflare, first year | Cloudflare, renewal | Porkbun, first year | Porkbun, renewal |
| --- | --- | --- | --- | --- |
| `.com` | US$10.46 | US$10.46 | US$11.08 | US$11.08 |
| `.dev` | US$12.20 | US$12.20 | US$8.75 | US$12.87 |
| `.org` | US$8.50 | US$11.20 | US$7.98 | US$11.84 |

Cloudflare publishes no per-name prices on its own pages; its figures here are from a third-party mirror of its at-cost list ([cfdomainpricing.com](https://cfdomainpricing.com/), entries dated 17 September to 2 October 2026). Porkbun’s are from its public pricing API, read on 4 October 2026. Prices exclude any tax. Verisign raises the `.com` wholesale price on 1 November 2026, which should take Cloudflare’s `.com` to about US$11.17. Over five years each name costs US$52–61 at either registrar, so the name matters more than the registrar. A `.dev` name only works over HTTPS, because the whole domain is on the browsers’ HSTS preload list; both candidate hosts serve HTTPS, so that constrains nothing here.

Cloudflare Registrar is a candidate for keeping domain and DNS management together. It registers and renews domains at cost, but requires Cloudflare DNS. The registrar and website host are separate choices even if both use Cloudflare. [Registrar pricing model](https://developers.cloudflare.com/registrar/), [DNS requirement](https://developers.cloudflare.com/registrar/get-started/transfer-domain-to-cloudflare/).

Expected recurring costs are the domain renewal, about US$11–13 a year at the prices above, plus any deliberately selected media, email, or paid hosting services. Plan sample downloads and larger videos separately from ordinary page assets.

## Delivery and maintenance

The following stages are proposed; no dates are assigned.

1. **Settle the foundation.** Choose stack, host, domain budget, and initial documentation coverage. Inventory the available guides, source comments, screenshots, and footage within the agreed repository layout. *Stack chosen, inventory taken ([website-inventory.md](website-inventory.md)) and coverage agreed; host and domain open.*
2. **Build a representative slice.** Create a homepage section, a Why Labrador? example, one Get Started walkthrough, and one API reference sample. Use these to validate visual design, reading documentation from the same checkout, and the reference-generation approach. *Built: home, Why Labrador?, Examples, the full five-page Get Started path, the three design documents read from the checkout, and three reference pages. The walkthrough’s project was built from the public repository exactly as written.*
3. **Complete the agreed first release.** Write and review the selected guides and reference coverage, capture real sample media, and connect examples to their source. Keep the full documentation goal visible in the content inventory. *Written: five Concepts pages, seven Guides, a Troubleshooting page and the whole API reference, with four LineSweeper captures and a silent gameplay video. Collision, audio and local-multiplayer guides quote dedicated runnable samples, including original playable audio content.*
4. **Validate and publish.** Check the new-user walkthrough on the named Windows toolchain; verify links, code snippets, search, keyboard navigation, narrow layouts, and contrast. Configure the chosen domain and deployment workflow when moving from planning into implementation. *Validated on 4 October 2026, the walkthrough having been built in stage 2 and the code snippets being quoted from the checkout: every internal link and anchor resolves; search finds reference types, members and guide topics; the tab order starts at the skip link and every stop shows a focus ring; no page scrolls sideways at 360 pixels; and axe-core finds no WCAG 2.2 A or AA violation on any page in either theme. That check found dark-theme text below 4.5:1 in four places and wide tables a keyboard could not scroll, and both are fixed. Publishing waits on a host and a domain.*
5. **Maintain with engine changes.** Update canonical technical content alongside relevant code, preview affected website pages in the same change, and retain a known working deployment for rollback.

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
Desktop and phone screenshots were reviewed. Hosting and publishing remain
the separate decisions below.

Proposed readiness criteria:

- A new visitor can identify what Labrador is, who it suits, its current platform support, and its meaningful trade-offs.
- Get Started takes a fresh setup through a running sample and an independently owned project.
- The website contains the agreed documentation coverage, with visible source revisions and clear boundaries around unfinished material.
- Differentiation claims link to examples or evidence; performance figures identify what was actually measured.
- Navigation, search, code blocks, images, and links work on desktop and phone.
- Content rights and attribution are checked for selected media. LineSweeper is the initial public showcase candidate; use of ColourWars assets is a separate choice because that client repository is private.

Keep the site’s build independent of the Windows engine build. Validate examples through the engine’s appropriate checks, then use web checks for page generation, links, and presentation. A website preview should be reviewed before the corresponding production deployment.

## Open decisions

The requirements and the use of `website/` in this repository are established; these implementation choices still need settling.

| Decision | Current recommendation or next action |
| --- | --- |
| Stack | **Decided:** Astro + TypeScript + Starlight, validated by the slice. |
| Hosting | Cloudflare Workers Static Assets; compare with GitHub Pages. |
| Domain and budget | All three candidates are unregistered; prices are above. Choose a name. |
| Documentation release scope | **Agreed** as [the inventory’s last table](website-inventory.md) proposed. Written, including the three guides backed by the collision, audio and local-multiplayer samples. |
| API reference | **Decided:** the line reader, with history taken out of the headers first. Every public header is published. |
| Visual identity and media | Provisional: the accent and the favicon come from LineSweeper’s own palette and L piece. Four exact stills and a silent gameplay video from `tools/linesweeper_capture/`. |
| Version and publishing policy | Every page shows the commit it was built from, and the pages read from the checkout read it at that commit. Release-specific documentation waits on releases. |

What stands between the validated site and publishing it is the owner’s to decide: a host and a domain. The collision, audio and local-multiplayer guides and their worked examples are included.
