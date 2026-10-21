# Concept verification — 20 October 2026

## Completed

- Isolated Astro production build: three pages (`/`, `/duo/`, `/guides/`).
- All three built routes return HTTP 200 from the local-only Astro preview.
  Loopback verification required running the temporary server and read-only
  requests outside the sandbox; the server is stopped after validation.
- `astro check --config explorations/labradors/astro.config.mjs`: 34 files,
  zero errors, warnings or hints. This includes the site's Astro files.
- The repository's `npm test`: all 12 website tests pass.
- Offline export: each page embeds CSS, script, logo and any sample image.
  Internal direction links point to adjacent offline files. No new packages,
  fonts, image downloads, telemetry or storage are required.
- Browserless checks of the **built** pages and script: 36 combinations of
  three directions, two themes, two contexts and three presence settings.
  Context changes show the selected excerpt; footer-only hides dialogue;
  off hides both treatments and closes the disclosure. Essential next-step
  links remain visible. This uses a selector/event adapter, not a browser DOM.
- Generated HTML checks: one h1, unique IDs, valid labelled-by targets,
  document language, noindex, current-direction indication, image alt
  attributes, placeholder labels, embedded resources and resolving offline links.
- Text palette calculations: minimum 5.71:1 in light mode and 7.25:1 in dark
  mode across the primary, muted and link text on the study's four surfaces.
  The placeholder labels use white on #202124. Select controls have a stronger
  border than decorative dividers. Calculations do not replace rendered checks.
- Independent source review of native controls, hidden contexts, focus styles,
  in-flow bubbles, mobile breakpoints and 320px width arithmetic. Placeholder
  labels were increased to about 10–11px after review. No animation is present.
- Repository safety: the initial tracked tree was clean; no existing tracked
  file was edited. All authored files are under `website/explorations/labradors/`;
  generated output is ignored under `out/labrador-dog-concepts/`.

## Rendered Chrome review

The connected-browser inventory is empty and the in-app browser is unavailable.
The installed repository runner `tests/render/web/run_page.mjs` and installed
Chrome provide an independent local test path, with no new setup or packages.
The runner uses a temporary browser profile and closes it at the end.

- Headless Chrome 155: 144 rendered combinations, across all three directions,
  widths 320/360/768/1440 CSS pixels, both themes, both page contexts and all
  presence settings. No page overflow or clipped visible portraits, bubbles,
  header or preview headings. All images load and reduced motion has no animation.
- Real keyboard input: radio arrow-key selection, footer Enter/Space toggling,
  Tab movement from the skip link to the header and a visible focus outline.
- Chrome accessibility tree: hidden dog asides and footer are absent when off;
  the active Get Started heading remains present.
- Seven screenshots captured and visually reviewed: each direction at desktop
  light and phone dark, plus the open footer-only treatment on a dark phone.
  The longest warm-guide bubble and all placeholder labels remain readable.
- The runner reports no browser warnings or errors. Three JSON reports retain
  every rendered combination and the tested browser user agent.

Reproduce from the repository root after building and exporting the study:

```powershell
node website/explorations/labradors/prepare-browser-review.mjs
foreach ($page in @('index','duo','guides')) {
    node tests/render/web/run_page.mjs --browser 'C:/Program Files/Google/Chrome/Application/chrome.exe' --page "out/labrador-dog-concepts/browser-harness/$page.html" --input website/explorations/labradors/browser-review.mjs --timeout 60 --browser-warnings fail
}
```

The preparation step adds the runner's lifecycle promise to temporary copies;
the offline deliverables and production source are not altered by the harness.
The responsive conversation stacks at tablet widths as well as on phones.

Remaining checks before production integration: actual browser zoom at 200%,
full keyboard traversal of every control, a full accessibility scanner and
manual reading-order/screen-reader review. The completed tree and keyboard
checks are useful evidence, not a claim of full accessibility certification.

## Existing website verification limit

The regular `npm run check` and `npm run build`, with the original untouched
`astro.config.ts`, stop while loading that config:

```text
require is not defined
C:/VSProjects/Labrador/website/node_modules/source-map-js/lib/source-map-generator.js:11:17
```

The isolated study build and its explicit-config type check pass. The default
site's dependency/config-loading issue has not been changed in this bounded design
task. Its normal production build must be restored and rechecked before any future
integration into production. The isolated dev server also reported a missing
`shiki` dependency; review uses the built static study instead.
