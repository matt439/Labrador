# Improvement and feature review: 2026-11-10

Reviewed revision: `e1fa9a8d`, master, clean tree.

A read-only review of the whole project for improvements and missing
features, with comparable projects read to show what Labrador lacks. The
findings describe that revision and are not rewritten as work lands.
[STATUS.md](STATUS.md) is the live ledger, [FINDINGS.md](FINDINGS.md) holds
every finding with its evidence, and [TODO.md](../../../TODO.md) indexes the
open ones as `REV#14` to `REV#103` and `OCA-76`.

## Method

Nine reviewers read the tree and returned 120 candidates. Five read
Labrador alone: render and assets; core, collision, scene and math; input,
audio, UI and the application; the adopter's path through the samples, the
starter and the website; and tests, tooling and CI. Four compared it with
other projects, reading their current documentation: C and C++ frameworks
(raylib, SDL3 and its satellite libraries, SFML 3, Allegro 5, sokol and
smaller ones), frameworks in other languages that the same audience chooses
between (LÖVE, MonoGame and FNA, libGDX, HaxeFlixel, Macroquad, Bevy's 2D,
pygame), browser engines (Phaser, PixiJS, Kaplay, and the web exports of
Godot and Defold), and the same projects as open-source projects: how they
are consumed, released and found.

Each reviewer first read TODO.md's `GAP#` and `IDEA#` rows,
[the comparison of 2026-10-27](../../survey/2026-10-27.md),
[Feature candidates](../../feature-candidates.md),
[the recommendations of 2026-11-04](../../2026-11-04-decision-recommendations.md)
and PHILOSOPHY's non-goals, and was asked for what those do not hold.

Nine sceptical verifiers then tried to refute every candidate against the
code and the records, merged duplicates and corrected overstatements. A
tenth looked for areas the review had not read and returned six findings of
its own. What follows is the corrected statement of each, not the original
claim.

## Limits

- Nothing was built, run or measured. Every finding is from reading source,
  documents, Git history and GitHub's read-only API. A finding is a question
  until its fix proves it.
- Reviewers and verifiers were AI agents. The verifier's reading is the
  evidence; no person has checked each one.
- Statements about other projects come from the documentation the reviewer
  read on 2026-11-10 and were spot-checked, not exhaustively confirmed.
- Where a finding leans on the client, it says only what this tree already
  publishes or a public issue states.
- Line numbers are those of `e1fa9a8d`.

## Assessment

The engine's core is sound: the review found no defect that loses data or
corrupts play. What it found falls into five groups.

**Seven small things are wrong today** (`REV#14` to `REV#19` and `REV#103`). A minimised
window runs the frame loop with no wait. `Application::load_manifest` and
`run` dereference null when `initialize()` was skipped, where their
neighbours are guarded. One guide page says the engine does not load PNG,
which stopped being true on 2026-11-04, and `SECURITY.md` describes a JSON
reader that was replaced on 2026-11-05. `Scene::update`'s order is published
in a guide and pinned by no test. Four open client issues are indexed
nowhere. README's line counts have drifted.

**The engine lacks a few small mechanisms that every comparable framework
has and that a present caller in this tree works around.** A texture's size
cannot be asked, so the guides have a beginner type it by hand. A game
cannot see a request to close its window. The pad seam has no headless
implementation, though render and audio have one and PHILOSOPHY requires it
of a seam. Engine diagnostics have no sink a game can read. There is no
version a consumer's code or build can read. Text alignment is re-derived
at each call site.

**Several parked items have new evidence, and two have met the condition they waited on.** `IDEA#8`
and `IDEA#11` each named a prerequisite that has since landed. `OCA-76`
still waits for a second caller: the in-tree copies are its own guide and example.
`REV#9`, `REV#10` and `GAP#6` waited for a client that needs them, and the
client has built each for itself without filing a report, which under T1 moves none of them; only `REV#9`'s row gains the fact. The calls stay the
owner's.

**The adopter's path is deep and nobody has walked it.** GitHub showed no
stars, no forks and eight unique visitors in the fortnight to 2026-11-10,
and one download of the starter ZIP. The front page has no picture, the
site has no stated limits page, five of six samples cannot be tried without
building, and the only consumption routes are a submodule and the ZIP. More
onboarding work returns little until someone arrives.

**The quality gates stop at warnings and tests.** No sanitizer, static
analyser or fuzz target runs, the configurations CI skips are built only at
release, and the browser module has grown across four recorded
measurements with no budget.

## Counts

| Disposition | Findings | TODO rows |
|---|---:|---|
| Fix now: verified, small, no owner call | 7 | `REV#14` to `REV#19`, `REV#103` |
| Other rows needing no owner call | 10 | `REV#20` to `REV#29` |
| The owner's decision: stories, scope and policy calls, low priority | 60 | `REV#30` to `REV#94` less the six below, `OCA-76` |
| Deferred until a measurement or caller shows the need | 14 | `REV#78`, `REV#81`, `REV#84` to `REV#87`, `REV#95` to `REV#102` |
| Evidence added to an existing row or story | 5 | `IDEA#8`, `IDEA#11`, `REV#9` amended; OCA-04 has no row |
| Not taken | 8 | none |

The 120 candidates became 104 findings once duplicates were merged, six of
them the completeness pass's own.
