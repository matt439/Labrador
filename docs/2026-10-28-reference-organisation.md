# Reference generator organisation and parity

SM-52 separates the website's reference generator behind its existing
`website/src/lib/reference.ts` exports. The header parser, published-header
policy, Markdown plugin consumers and content loader are unchanged.

| File | Responsibility |
|---|---|
| `reference.ts` | The existing exported facade and lazy service composition. |
| `reference-indexes.ts` | Header discovery and the single cache/reset owner: symbols, members, free functions, includers, trade-offs, explanations and excerpt dependencies. |
| `reference-prose.ts` | Escaped comment prose, examples and links. It owns no cache. |
| `reference-pages.ts` | Declaration layout, deferred page bodies and reference/module assembly. |
| `reference-types.ts` | Shared interfaces, without runtime state. |

Indexes receive a lazy layout callback. They inspect its headings without
evaluating the prose bodies that resolve those indexes. Service construction
does not invoke that callback. No lower module imports the facade; the actual
local import graph is acyclic across twelve reachable modules.

`forgetIndexes()` retains its reset behavior. Old maps held by callers are not
cleared; later getters build fresh maps. Excerpt dependencies remain empty
until a page's guide return links render, and reset with the other indexes.
Header enumeration and `publishedPages()` remain fresh reads. The content
loader still resets before regenerating pages and adds excerpt watches after
regeneration; this change does not widen its existing watcher coverage.

## Frozen-source comparison

The comparison holds HEAD at
`8048e2448cb813adf1ac2f916ce8cb2ef4058308` on the existing local work branch,
using Node `v24.14.0`. No commit is made between captures. A real parity
harness and cache regression tests are added **before** the baseline, so both
builds truthfully say `modified: true`. No revision environment override or
output normalization is used.

The one-off harness runs from `website/` after each fresh ordinary build:

```text
npm run build
node scripts/reference-parity.mjs capture ../out/sm52-parity-before

# Extract the generator without changing its other source inputs.
npm run check
node --test --test-concurrency=2 tests/*.test.mjs
npm run build
node scripts/reference-parity.mjs compare ../out/sm52-parity-before ../out/sm52-parity-after
```

Both captures retain revision metadata, ordered symbol/function maps, resolved
type/member/function/header links, all reference and module Markdown,
explanation source/dependency lists and a complete built-file path/hash
manifest. The harness also verifies HEAD and input bytes before and after
capturing. Exact equality holds for:

- 1,014 non-generator input files, including the harness and tests;
- 93 reference pages and 10 module pages;
- all 431 built files, including 143 HTML pages, search files and assets;
- the exported maps, link results, dependency lists and revision metadata.

The only input-file exemptions are the five generator files in the table
above. TypeScript syntax-tree comparison independently preserves 38 moved
function bodies. `explainedIn()` is split into the same cached index
construction and its original HTML formatter; the other moved bodies are
unchanged.

## Checks and retained evidence

The website type check reports zero errors, warnings or hints. All 160 website
tests pass with at most two test workers. Three new tests cover cold/warm/reset
equality, initialization and rendering order, map identity and borrowed-map
retention, deferred explanation discovery, and published-inventory changes.
The 143-page static build passes internal links and assembles zero release
snapshots. The published-release manifest is empty; no release is downloaded.
Vite emits its existing upstream `astro:head-inject` directive warnings in both
builds. No browser, graphics, Emscripten, deployment or hardware result is
claimed.

Local evidence is retained under ignored `out/`:

- `sm52-parity-before/` and `sm52-parity-after/`: raw comparison records;
- `sm52-module-graph.json`: the local dependency graph;
- `sm52-function-bodies.json`: moved-body comparison;
- `sm52-parity-proof.json`: counts and frozen revision;
- `sm52-parity-harness.mjs`: the byte-exact one-off harness.

The harness SHA-256 is
`3a057218f602eed7a5cc4d768e02174bddb5b3df10d2aca075fae6da3a6b5288`,
matching its recorded bytes at the original
`website/scripts/reference-parity.mjs` path in both captures. The archived
copy preserves that original file, including its original relative imports;
the commands above describe where it ran. It is not shipped as a general
website command because its branch/dirty-state guards describe this specific
extraction experiment.

After the strict comparison, the website harness is removed and this dated
record is added. That proof-scaffold cleanup is an explicit input difference,
separate from the equal before/after generator comparison. The delivered
build's final path/hash comparison is recorded in `out/sm52-delivered-check.json`.

**Final cleanup verification:** the delivered 143-page build passes and all
431 output paths and hashes equal the original baseline. The complete input
comparison differs only by the removed website harness and this new dated
record; HEAD remains `8048e24`. The final check records both differences
explicitly and preserves the original frozen comparison records.
