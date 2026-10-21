# Vader & Ani: local website concepts

A bounded design study for Matthew's two Labradors on the **Labrador C++ game
engine** site. Vader is black; Anakin, called Ani, is yellow. The portraits are
generic SVG placeholders, labelled on every appearance. Character guidance now
draws on Matthew's descriptions of their real relationship. Every line of
dialogue and staged website scene is fictional; Vader's replies are not reports
of his real reactions. Their names do not imply a Star Wars theme.

No production page, navigation, site setting, dependency or engine file is changed.
This directory is outside the regular site's `src/`; the separate Astro config
builds only the study, into ignored `out/labrador-dog-concepts/`.

## Confirmed design constraint

Confirmed by the owner on 20 October 2026: **keep Vader/Ani character appearances
and dialogue out of technical reference pages.**

Home and Examples remain candidate placements. No tone has been chosen and no
production integration has been authorized. Other contexts in the prototype are
exploratory comparisons, not approved placements.

## Review

Run these from `website/`, using its existing dependencies:

```powershell
npm exec -- astro build --config explorations/labradors/astro.config.mjs
node explorations/labradors/export-review.mjs
node explorations/labradors/verify-review.mjs
```

Open `out/labrador-dog-concepts/offline/index.html` from the repository root.
It and the two adjacent files embed their styles, scripts, mark and sample image;
they work without a server. Their direction links stay within that folder.
Documentation links lead to the current public site only when clicked.

For a local HTTP preview of the build:

```powershell
npm exec -- astro preview --config explorations/labradors/astro.config.mjs --host 127.0.0.1 --port 4326
```

The routes are `/` (dry), `/duo/` (banter) and `/guides/` (warm).
Each has Home and Examples contexts, light/dark themes and a dog
presence setting: contextual asides, footer only or off. These controls do not
persist or change the real site's settings. The footer introduction opens on
request; it has no fictional dialogue. Off closes and hides it too.

## The directions

| Direction | Proposed exchange | Useful placement |
| --- | --- | --- |
| Dry sibling aside | **Vader:** "I brought a demo." **Ani:** "I'm presenting." **Vader:** "What have you prepared?" **Ani:** "An entrance." | One aside after a Home demo introduction. |
| Playful interruption | **Vader:** "Mum's going to love this." **Ani:** "I'm showing her." **Vader:** "What are you showing?" **Ani:** "Me. You can come too." | A sample preview on Examples. |
| Warm sibling guide | **Vader:** "I'll show you around." **Ani:** "I'll do it." **Vader:** "After you." **Ani:** "Only if you're coming." | Home's route into Examples. |

The Examples contexts show a second exchange for each direction, including Ani
claiming a practice turn he does not actually want. A shared
preview frame holds the existing amber mark, system type, neutral surfaces and
real sample imagery; it is a page excerpt for comparing voice and placement,
not a proposed replacement design for the homepage.

**Recommendation:** compare the dry "An entrance" interruption with the warmer
"Only if you're coming" ending. The first most directly translates the real
attention-stealing pattern; the second makes the bond visible. Vader's Mum
exchange gives him his own enthusiasm. Use at most one exchange on Home or
Examples, with the factual sample description and links outside it. The
footer-only introduction remains an option. No tone is selected.

[`CHARACTERS.md`](CHARACTERS.md) separates owner-supplied traits from invented
dialogue and records the copy rules for this relationship.

Essential instructions and links remain outside the bubbles. Technical reference
pages are excluded by the confirmed constraint above. Also avoid appearances
inside code blocks or error messages. Keep dialogue in document
flow, with no movement, autoplay, timers, overlays or floating assistant. A future
production preference should preserve the reader's choice; this prototype keeps
its controls local to the current page.

## Photos later

- One clear head-and-shoulders photo of each dog, at similar camera height and
  lighting, with room around ears and muzzle. A calm frontal or three-quarter
  view should crop well at small sizes.
- Optionally one relaxed photo of them together for the footer.
- Matthew's judgement of whether any proposed voice fits; expressions in photos
  can inform the final copy. No staged poses or costumes are needed.

The black portrait will need detail in the face and separation from dark-mode
surfaces. Keep the source photos untouched and derive crops; do not infer real
personality from coat colour. Strip location metadata from public derivatives.

## Context consulted

- Root `README.md`, `AGENTS.md`, `TODO.md`'s NOW block and `CONTRIBUTING.md`.
- `docs/design/PHILOSOPHY.md`, `ARCHITECTURE.md` and `CONVENTIONS.md`.
- `website/README.md`, `docs/website-plan.md`, current Home, Examples and Get Started.
- `docs/product/PERSONAS.md` and journeys for audience and task context. Historical
  product findings are not treated as current feature claims.
- There is no repository `.agents/` directory. The user-level synced skills
  inventory contains document/workflow skills and no additional relevant design
  skill. The available Sites skill's local-only workflow was used.

The primary reader is an outside C++ adopter deciding fit and getting a first
project running. Returning integrators need fast access to exact contracts. The
dogs add a small personal connection without becoming navigation or instructions.

## Verification and limits

See `QA.md` for exact completed checks, seven reviewed screenshots, the rendered
Chrome test procedure and remaining accessibility/zoom checks. The
browserless verification helper uses parse5, domhandler and css-select already
present in the site's locked dependency tree; it adds no packages. Source
is isolated and removable as one directory. Generated output is under `out/`.
Nothing is committed, pushed, published or deployed by this exploration.
