# Three-guide compilation pilot, 2026-11-17

SM-84 qualifies the numbered main procedures in `your-rule.mdx`,
`your-screen.mdx` and `your-collision.mdx` against an extracted development
starter. It closes REV#133's selected pilot. It does not extend compilation
coverage to every guide, intermediate edit or optional variant.

## Source and procedure

The source was the isolated worktree based on `5cff44b9`, with the pilot's
working changes. `starter_zip.py` produced a 10,847,687-byte development ZIP,
SHA-256 `3b9baad69c1582dd0b3b84d56240a2bd5fc8bef2e4547e33ee45b92d7f17a959`.
Windows `tar` extracted it, and `guide_check.py --from` copied that project
to the new `C:\dev\g84-clean`. The source project was never changed.

The [receipt](2026-11-17-guide-pilot.json) records guide, tool and generated
source hashes, all 19 command argument lists, elapsed seconds and exit codes.
The complete generated changed sources and raw logs remain under that copy's
`out/guide-pilot/`; the CI step retains the same folder as its artifact.
The receipt describes the actual pilot working source, rather than implying
that the untouched base commit contains the new tooling.

The copy used the ordinary `x64-debug` consumer preset, with explicit null
render/audio overrides, Ninja parallelism 2 and preinstalled packages from
the existing native vcpkg install. Manifest installation was disabled for
this local proof. CMake identified MSVC 19.51.36256.0. The build directory
started empty; these are elapsed command times on a working desktop with
other builds running, not a stable performance benchmark.

## Result and measured cost

| Phase | Build seconds | Configure, build, inventory and tests seconds | Result |
| --- | ---: | ---: | --- |
| Unchanged starter, cold native-null build | 24.702 | 26.540 | 2/2 consumer entries |
| Rule guide, incremental | 3.814 | 4.244 | 2/2; includes the guide's own new rules tests |
| Screen guide, incremental | 8.186 | 8.749 | 3/3; includes Enter/Escape state checks |
| Collision guide, incremental | 7.490 | 8.016 | 3/3; includes wall stopping, sliding and contact tint |

The negative compiler proof took 0.493 s and exited 2. It replaced the
generated rule's `RectangleF` constructor with `SM84_BROKEN_GUIDE`; MSVC
reported `game\rules\bounds.cpp(13): error C3861`, naming that identifier.
Restoring the exact source bytes, rebuilding and rerunning its 2/2 entries
took 1.256 s. All 19 commands together took 49.298 s; that sum excludes
ZIP creation, extraction, source copying and Python's recipe/file work.

The normal pilot adds one clean native-null consumer build and three
incremental guide builds to the existing `consumer` job. Baseline, rename,
release and shader checks retain their projects and commands. No job,
required context or recurring schedule is added. Hosted timing and the
normal vcpkg cache/manifest path have not been run for this new step; the
local package reuse is not a hosted CI measurement.

## Maintenance boundary

The three recipes currently check 39 guide/source selectors: 11 for rules,
12 for screens and 16 for collision. Each example changes five project
files. Complete new C++ files come directly from titled guide blocks;
build lists, entry points and inserted statements come from the same
procedure's snippets and SourceFile excerpts. Missing or ambiguous matches
stop the check. No second maintained copy of each finished game is checked in.

The initial implementation is 632 lines across five parser, recipe, runner
and state-fixture files, plus 134 lines of offline tests. That is a concrete
upkeep cost for only three guides. The eight offline cases cover recipes
against actual assembled sources, refusal on a changed anchor or missing/
duplicate block, optional-variant isolation, exact test inventories,
negative-proof false success, failure receipts and source restoration.

State checks exercise the finished game code, rather than another example:
Enter destroys the title and enters the greeting; Escape on the title quits
immediately. The collision state's real input, update, resolve and drawing
path records a player stopped at x=768 beside the wall at x=800, sliding
along it, gold on contact and blue after release. The null backend records
these facts without rasterization or physical input.

Wider coverage is not selected. First observe the published step's runtime
and recipe changes during ordinary maintenance, then weigh any additional
guide individually. This pilot does not qualify the released v0.3.0 starter,
browser compilation/pages, visuals, pads, the optional edits after each
procedure or the interactive Notepad/command-prompt walkthrough. Existing
dated manual runs retain their own evidence and limits.

## Tooling and website checks

The eight focused offline tooling cases pass. The website gate passes:
type checking, 200 tests and a 180-page build with internal links valid.
The whole local lint runner passes its first eleven steps. Its final Python
phase runs 404 tests with one skip and one failure in the unchanged baseline
`test_ci_build_retry`: the scripted-pad consumer step added a sixth native
build line while that check still expects five, and that line omits its
retry wrapper. This pilot adds no direct build line to that list. The
integration needs the separately tracked CI repair before claiming a passing
whole lint run; the failure is not relabelled as a pass here.
