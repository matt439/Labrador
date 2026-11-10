# First-run review: 2026-11-10

Reviewed: master at `3067c67c`, the live site built from it, and the released
`labrador-starter-v0.3.0.zip` (tag commit `0f4f5033`).

A review of what Labrador is like to install and use for the first time, for
the reader the owner set on 2026-10-30
([BACKLOG.md](../../product/BACKLOG.md#decisions-only-the-owner-can-make)):
someone with very little programming experience who wants to play around. The
findings describe that day and are not rewritten as work lands.
[STATUS.md](STATUS.md) is the live ledger, [FINDINGS.md](FINDINGS.md) holds
every finding with its evidence, and [TODO.md](../../../TODO.md) indexes the
open ones as `REV#106` to `REV#121`.

It differs from [the audit of 2026-10-30](../../beginner-guides.md) and from
[the review of the same day](../2026-11-10/README.md), which built and ran
nothing: here the released ZIP was downloaded and built, and six of the ten
readers followed the pages in their own copy of it.

## Method

**A real run.** The released ZIP was downloaded with the starter page's own
`curl` line, extracted with Windows' `tar`, configured, built and tested in a
prompt set up by `vcvars64.bat`, with vcpkg's download and binary caches
pointed at empty folders. Configure took 93 seconds, the build 7 and the tests
under one; both tests passed. The build folder was 557 MB and vcpkg's
downloads 633 MB. Git was installed on that machine, so vcpkg's private copy
of Git was not fetched.

**Ten readers.** Each walked one part of the path as that reader: the front
door and the release page; Install the tools; Get the starter project; Make a
first change and Read a build error; editing, debugging and the concept pages;
Troubleshooting, by producing first-hour failures and looking each message up;
the content guides; the gameplay guides; the gap between the site and the
release; and the cost of the route beside seven other frameworks, from their
documentation of that day. Six built, in folders of their own under `C:\dev`,
from the released ZIP. Each returned findings with what it ran apart from what
it read.

**Ten sceptics.** A second agent for each lens tried to refute every blocker
and major, re-running what was cheap. No finding was refuted outright; 31 were
corrected, and several grades were lowered. A completeness
pass then looked for the parts of a first week nobody had read, and returned
seven findings, which no sceptic checked.

## Limits

- Readers and verifiers were AI agents. No person new to a command prompt has
  followed the pages.
- Nobody opened the game's window. Changes were checked by building, by ctest
  and by `--smoke-test`; what a change looks like was not seen.
- The Visual Studio installer was not walked. An unattended install of the
  C++ workload was started in Windows Sandbox, on a clean Windows 11 with no
  Git, to time it and then follow the starter page there. It had reached
  package 279 of 515 after 43 minutes when it was stopped, so it measured
  nothing: neither a time nor a size is taken from it. Sandbox's disk is slow
  and that pace says nothing about a real machine.
- Everything ran on one fast desktop with Visual Studio 2026 and an English
  Windows 11. `REV#112` to `REV#114` are questions from reading, not
  observations.
- Statements about other frameworks come from their documentation as read on
  2026-11-10; their times are estimates.
- Line numbers are those of `3067c67c`.

## Assessment

**A near-beginner on Windows 11 who follows Get Started gets a running game
and a first change.** No blocker was found. Once the tools are installed, the
released starter goes from download to two passing tests in under two minutes
on this desktop. Every output box that was compared with a real run matched
it: the prompt's banner and check, the folder listing, the failed-test box,
and the five errors of the deliberate mistake, line for line. The stated times
are honest or conservative. Make a first change and Read a build error are the
strongest pages on the path.

**Nearly all of the cost is Visual Studio.** Beside LÖVE, Godot, raylib's
installer and pygame, the route sits in the heaviest group with SDL3 and
MonoGame, and is the only one that needs one particular command prompt out of
several that look alike. The site is honest about the cost once the reader
reaches step 1 of the second page, and silent before it (`REV#111`; `REV#63`
and `REV#120` weigh what could cut it).

**Day two is worse than day one, because the site is ahead of the download.**
`REV#62` recorded this from one inferred instance. Followed in a v0.3.0
starter, five guides tell the reader something that release cannot do, with no
note at the passage: a PNG picture, the two-player edit, a window size option,
the rule page's test counts and a line of the browser page. The evidence is
added to that row; the decision stays the owner's.

**Three defects were found by running, not reading.** A 32-bit prompt builds
the `x64-debug` preset without complaint and poisons the build folder
(`REV#106`). Windows' `tar` cannot extract at the root of the `subst` drive
that Install the tools recommends (`REV#107`). Moving a starter to a newer
release leaves the reader's Git history and top-level files behind
(`REV#108`).

**The rest are stumbles a reader recovers from:** messages with no
Troubleshooting entry, links that cross into the Git route, and guides that
interact in ways no page states.

## The route beside others

From each project's own first-run documentation on 2026-11-10. Times are the
reviewer's estimates for a beginner, not measurements.

| Route | Installs | Download | Terminal | Steps to own code in a window | Time |
|---|---|---|---|---|---|
| Kaplay playground | 0 | 0 | no | 1 | 1 to 2 min |
| LÖVE 11.5 | 1 | 4.2 MB | no | 4 | 5 to 10 min |
| Godot 4.7.2 | 0, extract and run | 86 MB | no | 7, in the editor | 15 to 30 min |
| raylib 6.2 installer | 1 | 833 MB | no | about 4 | 15 to 30 min |
| pygame | 2 | not measured | yes | 4 commands and a file | 15 to 30 min |
| MonoGame | 2 to 3 | not measured | yes | about 12 | 1 to 2 h |
| SDL3 with Visual Studio | 1 and source | several GB | no | 7, in the IDE | 1 to 3 h |
| Labrador starter | 1 | several GB, an 8.8 MB ZIP, about 200 MB at configure | yes, throughout | about 20 actions and 11 command boxes | 1 to 3 h, nearly all of it the install |

## Counts

| Disposition | Findings | TODO rows |
|---|---:|---|
| Fix, no owner call | 37 | `REV#106` to `REV#111`, `REV#115` to `REV#117` |
| Verify first: from reading only | 4 | `REV#112` to `REV#114` |
| The owner's decision | 8 | `REV#118` to `REV#121` |
| Evidence added to an existing row | 18 | `REV#62`, `REV#63`, `REV#66` |
| Not taken | 1 | none |
| Polish, listed and not carried | 33 | none |

The ten readers, their verifiers and the completeness pass returned 101
findings, counting each lens's own; several rows merge the same fact seen
through two lenses.
