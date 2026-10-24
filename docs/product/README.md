# Product design

Who Labrador is for, what each of them is trying to do with it, where the
repository falls short of that, and the order in which to close the gaps. The
three design documents in `docs/design/` say what the engine is; this folder
says who it is for, and it is argued in their vocabulary. A placement here that
fights a trade-off is either wrong or the trade-off is, and the trade-off is
amended in `PHILOSOPHY.md`, not here.

| File | What it holds | Kept current? |
|---|---|---|
| [PERSONAS.md](PERSONAS.md) | Seven personas: one primary (the outside adopter, by the owner's decision), two secondary (the maintainer and the integrator, the only users the evidence shows), two tertiary and two anti, each with its evidence and the assumptions still to validate; the guardrails the anti-personas impose | No: record of 2026-10-13 |
| [JOURNEYS.md](JOURNEYS.md) | Twenty-one journeys, step by step, with what happens today; and a literal run of Get Started | No |
| [GAP-REGISTER.md](GAP-REGISTER.md) | 149 gaps (34 major, 115 minor) in twelve epics, each with evidence, a scope check and the stories that close it | No |
| [STORIES.md](STORIES.md) | 162 user stories with Given/When/Then acceptance criteria, sizes and the trade-offs that bear on them | No |
| [BACKLOG.md](BACKLOG.md) | The stories in four horizons (Now, Next, Later, Won't this time) with exit criteria, commit batches, owner decisions and risks | **Yes**: tick a box in the commit that closes the story |

Everything except `BACKLOG.md` is a dated record, like `docs/review/`: it was
taken against `master` at `0977112` on 2026-10-13 and is read as of that date.
Line numbers in it drift as the tree moves; the ids (G01, OCA-03, E05) do not.

## The finding that shapes the rest

The repository's own record shows one person in two roles: the owner, who
maintains the engine, and the same owner writing ColourWars, its only client.
Everything public - the website, Get Started, the reference, the samples - is
written for an outside developer who appears nowhere in that record: no tags,
no issues, no contributing guide, and PHILOSOPHY says strangers are for
"eventually". The owner decided the process should still design for that
stranger, the **Outside C++ Adopter**, with the maintainer as the constraint on
capacity, so the persona that drives priority is a hypothesis. PERSONAS.md says
so for every claim about them, and BACKLOG.md's Now horizon is mostly fixes
that pay off whether the stranger arrives or not: a public face that agrees
with the tree serves the agents that read it as much as any adopter.

## How it was made

Four multi-agent passes, each checked adversarially before the next began. The
owner was asked to settle two points along the way: which persona leads, and
what hardware is in hand.

1. **Evidence and personas.** Eight readers each took one slice of the tree
   (design documents, public face, samples, ports, the ColourWars checkout,
   reviews and surveys, the API surface, the build) and returned cited
   evidence. Three independent drafts of a persona set came from different
   angles: evidence first, positioning first, and a skeptic asking who is
   actually served. One synthesis merged them, and a skeptic per persona opened
   every citation and moved what it could not find to *assumptions*.
2. **Journeys and gaps.** One mapper per persona and one verifier per journey
   that walked every touchpoint and tried to refute the mapper. In parallel, an
   agent followed Get Started literally in a scratch directory and built,
   tested and ran its own copied project. The raw gaps were deduplicated into a
   register, which a refuter and a completeness critic then reviewed: two gaps
   were refuted (G59, G87, whose ids are kept out rather than reused) and
   thirteen re-rated.
3. **Coverage, stories and priority.** Nine agents walked the areas no journey
   had: accessibility, native pads, the components, audio, site and toolchain
   upkeep, the reference pages, a third-party host, text and saves, and public
   repository hygiene. That added G116-G151. One story writer and one skeptic
   per persona followed. Two independent prioritisations (by value to the
   adopter, and by cost and capacity) were synthesised into one backlog and
   critiqued.
4. **Reconciliation.** Stories two personas wanted from one change were merged
   (23 groups); every trade-off a story cites was checked against PHILOSOPHY's
   text (105 corrected); the critique's corrections were applied, and each is
   logged at the end of BACKLOG.md. The dependency order and MoSCoW consistency
   were then checked mechanically.

## Limits

- **Every external persona is a hypothesis.** There are no analytics, no
  issues and no independent adopter to interview. The assumptions under each
  persona are what a first real user would confirm or refute, and the backlog
  should be re-ranked when one appears.
- **The Get Started run was warm.** It cloned from the local repository rather
  than GitHub and reused an existing vcpkg binary cache, so first-run cost is
  unobserved (G61).
- **Hardware facts are as the owner gave them on 2026-10-13:** a physical XInput
  pad and an iPhone, no Android phone; the 2013 MacBook Air stops at Big Sur;
  the desktop's integrated Radeon needs AMD's driver before it can be measured.
- **Sizes are estimates** for one person working with agents, not measurements.
