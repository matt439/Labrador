# Website dependency triage — 2026-11-17

The first bounded alert read after SM-92 enabled alerts returned 26 open npm
advisories, all for `website/package-lock.json`. This local repair updates the
current website graph. It neither closes remote alerts before publication nor
establishes that an advisory's exploit prerequisites are present in Labrador.
[The Action/settings record](action-runtime-updates.md) owns the separate
alerts-only change and its pending hosted qualification.

## Selection and evidence

The [captured primary advisory ranges](security/2026-11-17-website-dependencies.json)
record all 26 GitHub advisory URLs, affected package/ranges, first-patched
versions, the original matching package paths and the lockfile hashes. Every
resolved path, including nested copies, was compared: all 26 match the original
lock and none match the candidate. This is a comparison against that captured
set, not a standing vulnerability scanner or an exploitability test.

| Package | Original → repaired lock | Reason/path |
|---|---|---|
| Astro | 7.3.5 → 7.4.12 | Two Astro advisories; retains the supported 7.x direct range. |
| Mermaid | 12.1.0 → 12.3.0 | Supported 12.x minor brings upstream's compatible KaTeX and DOMPurify ranges. |
| DOMPurify | 3.4.16 → 3.4.18 | Three advisories fixed from 3.4.17; browser Mermaid dependency. |
| KaTeX | 0.16.47 → 0.18.10 | Advisory fixed from 0.18.2; selected through Mermaid, without a forced transitive override. |
| Undici npm package | 8.11.2 → 8.12.4 | Eight advisories fixed on 8.x from 8.12.2; Astro/unifont path. |
| fast-uri | 3.1.8 → 3.1.9 | Four advisories; AJV/yaml-language-server check tooling. |
| postcss-selector-parser | 6.1.4 → 6.1.6 | Advisory fixed on 6.x from 6.1.5; Expressive Code/postcss-nested build path. |
| Vite | 8.3.2 → 8.3.8 | Six advisories fixed on this line from 8.3.3; Astro development/build tool. |
| Zod | 4.6.5 → 4.6.7 | Advisory fixed from 4.6.6; Astro and sitemap paths. |

The direct minimums become `astro: ^7.4.12` and `mermaid: ^12.3.0`. The lock
was selected with `npm update --package-lock-only --ignore-scripts --no-audit
--no-fund astro mermaid dompurify undici fast-uri postcss-selector-parser vite
zod katex`, then reviewed and consumed by `npm ci`; no `audit fix --force`,
transitive override or engine/vcpkg baseline change was used. All 99 changed/new
resolved versions retain npm registry URLs and integrity values. The supported
direct minors also update transitive compiler, Shiki, rolldown, devalue, elkjs,
unifont and queue packages; those internal major versions are upstream's graph,
not independently forced compatibility choices. The custom root Satteri
processor remains on its existing requested range and passes the site gate.

[Mermaid 12.3.0](https://github.com/mermaid-js/mermaid/releases/tag/mermaid%4012.3.0)
records the sanitizer, ELK and legacy-renderer changes.
[Astro 7.4.0](https://github.com/withastro/astro/releases/tag/astro%407.4.0),
[7.4.7](https://github.com/withastro/astro/releases/tag/astro%407.4.7) and
[7.4.12](https://github.com/withastro/astro/releases/tag/astro%407.4.12)
record the Node minimum, image-endpoint repair and dependency selection changes.
The chosen package's actual engines require Node `>=22.23.4`; the website's
advertised 22.x minimum is corrected accordingly. `.node-version` stays `24`.
Local checks used Node 24.14.0; the 22.x compatibility route was not run.

## Exposure boundaries

The current [Astro configuration](../website/astro.config.ts) builds static
files, and [wrangler.jsonc](../website/wrangler.jsonc) serves `dist` as Worker
Assets. There is no `@astrojs/node` adapter in this graph. Astro/Vite's Node
development and request handlers are not the deployed static application's
request handlers. That configuration observation does not dismiss development,
build-input or future adapter exposure.

Mermaid is different: the site's
[Markdown wrapper](../website/src/components/overrides/MarkdownContent.astro)
dynamically imports it in the reader's browser on diagram pages, with
`securityLevel: 'strict'`. Its sanitizer/math dependencies therefore belong to
the browser-loaded graph. The project sets no DOMPurify hooks, `IN_PLACE`,
`SAFE_FOR_TEMPLATES` or XHTML parser mode. Those observations and the regression
below do not prove or refute each advisory's full exploit prerequisites.

fast-uri/AJV belongs to the check tool; selector parsing belongs to CSS build
tooling; external Undici comes through Astro's font tooling. Node's global
`fetch`, used by release/acceptance scripts, uses Node's separately bundled
Undici. Repairing the npm lock does not replace that runtime component.

## Local qualification

- Clean `npm ci --ignore-scripts --no-audit --no-fund` passed, followed by
  `npm run gate`: type checking, all 202 website tests and a 186-page static
  build with valid internal links and the unchanged immutable release mounted.
- `npm audit --json` returned exit 0 and zero reported vulnerabilities for the
  candidate at this check. That registry response is separate from the captured
  26-range comparison and is not a promise about future advisories or runtimes.
- Real Chrome 157.0.8103.78 rendered both architecture diagrams and the tick
  diagram in light/dark/light themes. The optional
  [diagram driver](../website/acceptance/diagram-input.mjs) adds a response-only
  math/script-shaped-label fixture to exercise the actual bundled KaTeX and
  sanitizer path. It requires new SVGs on theme changes, mathematical output,
  inactive script-shaped content and no browser complaints. This is a runtime
  compatibility regression, not a proof of advisory exploitability.
- An offline delayed-redraw regression holds the old complete SVG across two
  replies; the driver must wait for its replacement within the same deadline.
  The theme wait cannot pass by observing the previous theme's complete DOM.

From the repository root, after building the site, run each page:

```text
node tests/render/web/run_page.mjs --browser <chrome.exe> --root website/dist --page website/dist/docs/design/architecture/index.html --input website/acceptance/diagram-input.mjs --timeout 90
node tests/render/web/run_page.mjs --browser <chrome.exe> --root website/dist --page website/dist/docs/concepts/tick/index.html --input website/acceptance/diagram-input.mjs --timeout 90
```

The driver changes only the served response. It adds no production page,
required CI check, workflow schedule or outside request.

## Remaining owner work

1. **Node runtime patch qualification.** The local Node 24.14.0 reports bundled
   Undici 7.21.0. The official
   [Node 24.22.3 security release](https://nodejs.org/en/blog/release/v24.22.3/)
   includes Undici 7.30.2 and other security repairs. Qualify an up-to-date
   supported Node patch on the host and provider build, retaining exact version
   evidence; do not treat this lock repair or a major-only selector as that
   observation. The Node executable/system installation and production settings
   have not been changed here. Node 22 and actual hosted runtime evidence remain
   unrun; the package engine range is a compatibility floor, not a security
   support promise for every historical patch.
2. **Immutable v0.3.0 and v0.4.0 browser assets.** Both tags' locks record
   Mermaid 12.1.0, DOMPurify 3.4.16 and KaTeX 0.16.47. Their archived
   documentation remains mounted byte-for-byte by [the versioning policy](website-versioning.md). Current graph
   repairs cannot replace those bytes. Assess the archived browser entrypoints
   and advisory prerequisites, then select any erratum or hosting mitigation
   explicitly. This triage makes no claim that the archived site is exploitable
   or safe. No tag, archive, manifest hash, erratum or deployed policy was changed.
3. **Publication qualification.** Fresh hosted website CI and deployment/live
   verification follow the existing owner procedure when publication is chosen.
   No push, PR, remote dispatch, alert dismissal or deployment happened here.
