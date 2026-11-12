# Work audit: 2026-11-12

Audited: master at `3fe88f64`, TODO.md and every document its rows point to.

An audit of whether [TODO.md](../../../TODO.md) holds enough work that needs no
call from the owner. Before it, the only such rows were `REV#104` and `REV#106`
to `REV#117`. NOW also pointed at Later's next batch and at the hygiene lane,
but neither had an open item: every unticked Later story waits on the owner, on
hardware or on ColourWars, and every unit of [Code hygiene](../../code-hygiene.md)
has closed. [STATUS.md](STATUS.md) is the live ledger,
[FINDINGS.md](FINDINGS.md) holds each new row with its evidence, and TODO.md
indexes the open ones as `REV#122` to `REV#158`.

## Method

Twelve read-only agents built nothing and changed nothing. Two checked each of
the thirteen open rows against the tree. One read every `DECISION` and
`DEFERRED` row for a next step that needs no owner call: a measurement, a
reproduction or a fix to a record. One read the owner documents (BACKLOG.md,
Code hygiene, the review index and its ledgers, the beginner-guide audit,
web-to-be-tested.md, CHANGES.md, docs/verification.md) for open work with no
row, and grepped the source for work markers. Four proposed new work, one lens
each: engine correctness and tests; adopters and the guides; tooling, CI and
release; constrained hardware and the browser. Each lens's proposals went to a
separate agent told to refute them: already tracked, false in the tree,
against PHILOSOPHY's T1 or a recorded decision, or of no use. The audit's
author then read the survivors.

## What it found

**The thirteen open rows hold.** Each still describes the tree. Twelve can be
done by an agent; three of them (`REV#110`, `REV#113`, `REV#114`) need a
Windows Sandbox run. `REV#112` cannot be reproduced in Sandbox, which cannot
switch Smart App Control on.

**Sixteen owner rows begin with agent work.** Their next step is a
measurement, a reproduction or a record fix, and the call comes after it. They
are now `OPEN` in TODO.md, each clause naming that step and the call that
follows: `REV#33`, `REV#36`, `REV#37`, `REV#38`, `REV#47`, `REV#49`, `REV#53`,
`REV#63`, `REV#68`, `REV#69`, `REV#70`, `REV#72`, `REV#80`, `REV#97`,
`REV#101`, `REV#121`. `OCA-76`'s caller-free slice, walking the starter
through IDBFS, is the same kind of step.

**Rows that had gone stale** were corrected in TODO.md. `GAP#2` waited on
`OCA-40` and `GAP#12` named `OCA-43`, both closed. `GAP#6` did not say its
gate (`OCA-80`) had landed. `GAP#8`'s premise, that both clients hand-roll
easing, is false: neither does. `IDEA#13` now has the second copy its
promotion test asks for, in the components gallery. `IDEA#14`'s present
interval has landed as `set_vsync`. `IDEA#19` rides `GAP#5` in the
recommendations, not `GAP#8`. `REV#9` did not name `REV#40`, which would
unblock its pad half. `REV#98`'s cache figures were re-measured. The review
index, AGENTS.md's reading paragraph and the 2026-09-26 ledger each had a
closed item described as open.

**`OCA-28` was met and is ticked.** The website's platform-support section
lists every browser with how and when it was checked, and agrees with
web.md section 9 once web.md records the Safari 27.0.1 play of 2026-11-03.

**The GitHub counters for v0.3.0 were recorded** before the 14-day window
dropped release day: [the record](../2026-11-10/2026-11-12-github-counters.md),
under `REV#54`, whose release-procedure step stays the owner's.

**Thirty-seven new rows**, 31 needing no call, are in
[FINDINGS.md](FINDINGS.md): ten on engine correctness and tests, seven on the
guides, eleven on tooling, CI and release, and nine on constrained hardware and
the browser.

**No work hides in comments.** No `TODO`, `FIXME` or `HACK` marker is in
`engine/`, `samples/`, `tools/`, `tests/`, `bench/`, `cmake/` or
`website/src`; the only `XXX` matches are piece diagrams in LineSweeper's
rules table.

## Not taken

- **An instrument for SM-07's GPU-execution and latency claims.** Refuted: the
  tree already has a WPR GPU-trace wrapper around the frame bench
  (`tools/cloud_performance/trace_benchmark.ps1`, its README's trace section).
  What is left is narrower: making that wrapper usable locally and extracting
  per-frame GPU-busy and present-to-display from its trace. That waits for a
  reason to rehearse before `SM-01`'s Radeon session.
- **Running REV#102's size flags now.** Its deferral waits for a budget;
  `REV#155`'s attribution and `REV#53`'s measured brief come first.
