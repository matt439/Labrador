# Labrador Website Plan

**Planning draft · 4 October 2026**

This plan defines Labrador’s public website: a showcase that explains why a developer would choose the engine, a practical getting-started path, and a complete documentation area. The website will live under `website/` in the Labrador repository. Astro with Starlight and static hosting on Cloudflare remain recommendations; the repository choice and the requirements below are agreed.

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
| A focus on local 2D games | Multiple cameras and viewports drawing one scene, with controller input | Online play and 3D are outside the engine’s intended scope. A public multiplayer demonstration still needs selecting or creating. |
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

**Recommendation, still open:** Astro + TypeScript + Starlight, using static output, Markdown for ordinary documentation, and custom Astro components where a demonstration needs them. Start with ordinary CSS and a small shared set of colours, typography, and spacing.

Astro pre-renders pages by default. Starlight supplies documentation navigation, search, code highlighting, and other documentation features. This supports a custom showcase and structured docs within one site. [Astro rendering](https://docs.astro.build/en/guides/on-demand-rendering/), [Starlight features](https://starlight.astro.build/).

The proposed first version needs static pages, search, images, and recorded footage. Accounts, a database, and server-side application features are not currently requirements. Browser-playable engine demos would be a separate technical project because Labrador currently targets Windows.

**API reference generation remains unresolved.** Test a small public-header slice with a candidate generator before selecting the toolchain. Check symbol links, code examples, contract text, and integration with the documentation layout. Generated symbols need explanatory guides alongside them; extraction alone does not make a complete manual.

The visual direction remains open. Start from readable C++ examples and actual sample imagery, with clear navigation and comfortable reading on desktop and phone.

## Hosting and domain

**Hosting recommendation, still open:** Cloudflare Workers Static Assets. GitHub Pages is the main alternative if keeping publishing close to GitHub is preferred.

Cloudflare currently charges nothing for static asset requests and storage; build and platform limits still apply, while requests that invoke application code have separate pricing. A small static site can therefore target zero recurring hosting cost within those limits. GitHub Pages supports custom domains and is available for public repositories on GitHub Free. [Cloudflare billing](https://developers.cloudflare.com/workers/static-assets/billing-and-limitations/), [GitHub Pages](https://docs.github.com/en/pages/getting-started-with-github-pages/what-is-github-pages).

**Domain candidates to investigate:** `labradorengine.com`, `labradorengine.dev`, and `labradorengine.org`. Availability and prices have not been checked. Compare registration and renewal prices, including currency and applicable tax, before choosing a name and annual budget.

Cloudflare Registrar is a candidate for keeping domain and DNS management together. It registers and renews domains at cost, but requires Cloudflare DNS. The registrar and website host are separate choices even if both use Cloudflare. [Registrar pricing model](https://developers.cloudflare.com/registrar/), [DNS requirement](https://developers.cloudflare.com/registrar/get-started/transfer-domain-to-cloudflare/).

Expected recurring costs are the domain renewal plus any deliberately selected media, email, or paid hosting services. No exact annual total is established yet. Plan sample downloads and larger videos separately from ordinary page assets.

## Delivery and maintenance

The following stages are proposed; no dates are assigned.

1. **Settle the foundation.** Choose stack, host, domain budget, and initial documentation coverage. Inventory the available guides, source comments, screenshots, and footage within the agreed repository layout.
2. **Build a representative slice.** Create a homepage section, a Why Labrador? example, one Get Started walkthrough, and one API reference sample. Use these to validate visual design, reading documentation from the same checkout, and the reference-generation approach.
3. **Complete the agreed first release.** Write and review the selected guides and reference coverage, capture real sample media, and connect examples to their source. Keep the full documentation goal visible in the content inventory.
4. **Validate and publish.** Check the new-user walkthrough on the named Windows toolchain; verify links, code snippets, search, keyboard navigation, narrow layouts, and contrast. Configure the chosen domain and deployment workflow when moving from planning into implementation.
5. **Maintain with engine changes.** Update canonical technical content alongside relevant code, preview affected website pages in the same change, and retain a known working deployment for rollback.

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
| Stack | Astro + TypeScript + Starlight; validate with a representative page and reference sample. |
| Hosting | Cloudflare Workers Static Assets; compare with GitHub Pages. |
| Domain and budget | Check the three candidate names and renewal costs before choosing. |
| Documentation release scope | Inventory all public modules and agree the content required for the first release. |
| API reference | Evaluate generation from the current headers and comments; select the renderer after a small prototype. |
| Visual identity and media | Choose logo treatment, typography, colours, and sample captures. |
| Version and publishing policy | Begin with one documented engine revision; decide when release-specific documentation and automated updates are worthwhile. |

The next planning step is to settle the web stack and hosting, check domain availability and cost, and define the initial documentation coverage. Those decisions make the first implementation stage concrete.
