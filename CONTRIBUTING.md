# Contributing

How a change reaches `master`. This is the procedure the owner and every agent
working in this tree follow; it is written here because a rule that lives only
in someone's memory cannot be followed by anyone else.

## Asking a question or requesting a change

Everything goes through the
[issue tracker](https://github.com/matt439/Labrador/issues/new/choose), which
offers three forms:

- **Question**, for whether the engine fits what you want to build or how
  something in it is meant to be used.
- **Bug or build problem**, for anything in the engine, a sample, the build,
  the documentation or the website that does not do what it says.
- **Engine request**, for an API change or a new mechanism.

A security vulnerability is not an issue: report it privately as
[SECURITY.md](SECURITY.md) describes.

**An engine request leads with its exhibit**: the code your game wrote to work
around the gap, which the change would let it delete. That is PHILOSOPHY's T1
applied to intake - mechanisms are extracted from a client game in hand - so a
request with no workaround behind it is answered as a question about whether a
client needs the change. Check the gap against current `master` rather than
the revision your project pins, because a gap master has already closed is
answered by moving the pin. Name the trade-off you think applies.

A request is weighed on the trade-offs in
[docs/design/PHILOSOPHY.md](docs/design/PHILOSOPHY.md), T1 to T12. It may be
declined, and a decline gives its reason in those terms. One person maintains
this project, and no answer time is promised for any issue.

### How a request is closed

The request form labels every request `enhancement`. Closing it adds one of
two more labels and a closing comment that is the record:

- **`answered`**: the comment names the commit on `master` that met the need.
  If the answer is consumer-visible, its [CHANGES.md](CHANGES.md) entry links
  the request, so a pin move across it shows which workaround it retires.
- **`declined`**: closed as not planned. The comment names the PHILOSOPHY
  trade-off it rests on (T1 to T12, or the section) and what would reopen it,
  such as a second client asking or a measurement moving. A decline resting on
  a measurement is re-taken when the measurement moves, not re-cited.

Requests without either label are open. These searches separate the three:

- [open](https://github.com/matt439/Labrador/issues?q=is%3Aissue+label%3Aenhancement+is%3Aopen):
  `is:issue label:enhancement is:open`
- [answered](https://github.com/matt439/Labrador/issues?q=is%3Aissue+label%3Aanswered):
  `is:issue label:answered`
- [declined](https://github.com/matt439/Labrador/issues?q=is%3Aissue+label%3Adeclined):
  `is:issue label:declined`

Before filing, the declined search shows whether a need was already turned
down and on what terms. The engine's decisions on ColourWars' reports, made
ahead of their filing, are in [the client reports table](docs/review/colourwars-reports/README.md).

The two labels are repository settings the owner creates once:

```text
gh label create answered --color 0e8a16 --description "Request met; the closing comment names the commit"
gh label create declined --color b60205 --description "Request declined; the closing comment names the trade-off and what would reopen it"
```

## The procedure

1. **Branch from `master`.** One short-lived branch per change.
2. **Make one self-contained commit.** A commit is a complete change: the
   source, the tests that pin it, and every document it makes false, together
   (CONVENTIONS, and the design documents' rule that they change by amendment
   in the same commit as the change that fights them). A story in
   [docs/product/BACKLOG.md](docs/product/BACKLOG.md) has its box ticked in the
   commit that closes it.
3. **Say what a consumer will see.** A commit that touches `engine/**/*.h`,
   `engine/CMakeLists.txt`, `cmake/settings.cmake` or `vcpkg.json` adds an
   entry to [CHANGES.md](CHANGES.md), under its `## Unreleased` heading, in the
   same commit, or says
   `not consumer-visible` in its message. CHANGES.md's opening paragraph
   defines the term and its rule says what an entry holds: for a source break,
   the old spelling, the new one and the sample diff that migrates it. Review
   checks this before merging, because nothing else does.
   A commit that changes what `samples/minimal/` does or how it builds, or an
   engine behaviour that folder depends on, in a way a copy of it would want,
   adds a line to the change list in
   [samples/minimal/README.md](samples/minimal/README.md) in the same commit.
   A commit that changes a dependency in `vcpkg.json` changes the notices list
   a game's release carries,
   [website/src/components/ReleaseNotices.astro](website/src/components/ReleaseNotices.astro),
   in the same commit; the website build fails on a package it does not cover.
4. **Open a pull request** against `master`.
5. **Wait for CI to go green.** Every configuration CI builds is a required
   check, and a change is not known to be clean until all of them pass: a
   change clean under MSVC is not yet known to be clean under clang, and an
   edit outside `website/` can fail the website build.
6. **Rebase-merge.** `gh pr merge --rebase`, or `--squash` if the branch grew
   fixup commits, so that `master` stays a linear history in which each commit
   stands alone. Merge commits are disabled in the repository's settings.

`master` is protected: it requires the checks above and a linear history, and
nothing reaches it except through a pull request. The owner can bypass the
protection for a trivial fix; nobody else does.

**The required checks are names held in GitHub's settings**, not in this tree,
so this list is the record of what those settings must hold. Each name is the
job's `name:` in its workflow, and the windows jobs are named for their preset
alone, so changing a matrix entry's `skip_tests` or `vulkan_sdk` does not
rename its check. The required contexts are exactly these thirteen:

- `changes`
- `lint`
- `windows (x64-debug)`
- `windows (x64-release)`
- `windows (x64-debug-d3d12)`
- `windows (x64-debug-gl)`
- `windows (x64-debug-vulkan)`
- `windows (x64-debug-null)`
- `wasm`
- `wasm-null`
- `wasm-consumer`
- `consumer`
- `website`

The first twelve are jobs in [.github/workflows/ci.yml](.github/workflows/ci.yml)
and the last is the job in
[.github/workflows/website.yml](.github/workflows/website.yml).

**A change that touches only documentation is linted, not built**: when
every path it changes is a `.md` file or under `docs/`, the `changes` job
says so, the build jobs report without building, and `lint` runs the
citation check, the agent brief check, the engine and LineSweeper include walls
with their offline layer fixtures, offline source
revision fixtures, the whitespace check, the flash-definition, pacing-analysis
and bundle-verification tests, and the Python tooling suite.

The Vulkan SDK download and its offline helper tests use Python 3.11 or newer. Its bounded HTTP
retry helper leaves SHA-256 validation and installation outside the retry;
authentication, transport and filesystem denials stop immediately.
Requests identify the application as `LabradorCI/1.0` with the repository URL;
HTTP 403 remains a terminal response rather than a retry condition.

The website build still runs, because it reads the design documents. `changes`
is required for that reason: if it failed, the jobs that need it would report
as skipped, and a skipped check passes.

Manual dispatches of the CI workflow and its weekly Monday 03:17 UTC schedule
always run the full build matrix, so runner/browser drift gets a run without
a source edit.

**A merge is tested once, by its pull request.** On a push to `master`, both
workflows run [tools/ci_tested_tree.py](tools/ci_tested_tree.py) first. When
a pull request merged into `master` has the pushed tree at its head, that head
contains the `master` the push replaced, and the workflow's newest run on it
succeeded, the push run builds nothing and says which pull request passed the
tree. Strict required checks are what make that the usual case. Anything
unproven builds everything: a forced or direct push, a failed or missing pull
request run, an API error, and a change to `vcpkg.json`, `.github/` or, for the
website, its lockfile, because a run on `master` is what refreshes the caches
pull requests restore. Browser ccache objects on `master` are therefore
refreshed by those runs and the weekly schedule rather than by every merge.

For a deliberate dependency or toolchain bump, follow
[Moving dependency and toolchain pins](docs/dependency-updates.md). It lists
fixed inputs and moving selectors, the derived consumer manifest, coupled
changes and the qualification each bump needs. A scheduled drift run does
not replace that verification.

A pull request's run is meant to finish in about five minutes, and it does
so on caches: the windows jobs' vcpkg binaries, and ccache for the three
browser builds. A run after a `vcpkg.json` change, or after a cache has been
evicted, builds from source and takes longer.
`gh api repos/matt439/Labrador/branches/master/protection` shows what the
settings hold, and its contexts should be this list.

**A pull request that adds, removes or renames a job or a matrix entry updates
the branch protection in the same change**, and this list with it. A removed or
renamed context leaves every later pull request waiting on a check that never
reports; an added one that is not required can fail and still merge.

**Required checks are strict**: a branch must be up to date with `master`
before it merges, so its checks have run against the tree it will produce. Two
pull requests that each pass against an older `master` can together break it -
one rewording a line a website page quotes and another adding a page that
quotes it, or one renaming a header another starts including - and only a run
on the combined tree sees that. The cost is a rebase and a fresh CI run when
`master` moves first, which a repository with one maintainer pays rarely.

## Cutting a release

A release is an annotated tag on `master`, cut by the owner, and a GitHub
release beside it. What a release promises is in
[PHILOSOPHY](docs/design/PHILOSOPHY.md), *The public face*; this is how one is
made.

**The name** is `v` followed by `vcpkg.json`'s `version-string`: `"0.1.0"` is
tagged `v0.1.0`. At a tagged commit the two always agree.

**The number.** Before 1.0, the second number moves for a batch that has any
CHANGES.md entry, and the third only for a batch with none. That is T6: a
`settings.cmake` flag or a silent behaviour change breaks a game as hard as a
source break does, so every entry counts the same. From 1.0, the first number
moves for a source break, a removed or retyped name, a moved header, or a
change to what `labrador_settings` gives the game. 1.0 itself is the first tag
cut once PHILOSOPHY's settle criterion has run.

1. **Audit the batch.** Run `git fetch origin --tags`, then list what it
   holds with
   `git log --format='%h %s' vPREV..origin/master -- 'engine/*.h' 'engine/*.cpp' engine/CMakeLists.txt cmake/settings.cmake vcpkg.json`,
   where `vPREV` is the newest tag. Every commit it prints that touches a path
   the rule names has an entry in CHANGES.md or says `not consumer-visible` in
   its message, and a change to a source file is read for a behaviour change a
   game would see, which the rule's paths do not catch. A commit the audit
   finds unclassified is given an entry, or a line under the release's heading
   saying why it has none, in the next step.
2. **Prepare the release commit**, by the procedure above. It sets
   `version-string` to the new number, and renames CHANGES.md's
   `## Unreleased` to `## vX.Y.Z (date)`, dated the day the commit is
   authored, which is the author date a rebase-merge keeps; a batch with no
   entry gets the heading with nothing under it. Its message says
   `not consumer-visible` for the `vcpkg.json` change, which CHANGES.md's
   *Entries* explains.

   **For `v0.3.0`, qualify documentation versioning before the tag is cut.**
   It is the first release with a complete immutable snapshot; `v0.1.0` and
   `v0.2.0` are not backfilled. The build, provenance, routing, search and
   selector checks are in [the versioning policy](docs/website-versioning.md).
   GitHub immutable releases are enabled on the repository; keep them on.
3. **Merge it, then wait for its push runs.** The commit's hash on `master`
   differs from the pull request's head, so find it there, and run
   `gh run list --branch master --commit <sha> --json workflowName,conclusion`
   until both workflows, `build and test` and `website`, have succeeded. A run
   that finds the pull request already passed the tree succeeds in under a
   minute without building. Merge nothing else meanwhile:
   both workflows cancel a run in progress when `master` moves. A run cancelled
   anyway is re-run with `gh run rerun <id>`, never re-merged. Only a `master`
   commit whose push runs are green is tagged. `git show <sha>:vcpkg.json`
   names the version about to be tagged.
4. **Tag that commit:** `git fetch origin`, so the clone has the commit the
   merge made, then `git tag -a vX.Y.Z <sha> -m "Labrador X.Y.Z"`, signed with
   `-s` if a key is to hand, then `git push origin vX.Y.Z`. A pushed tag is
   never moved or deleted
   ([docs/website-versioning.md](docs/website-versioning.md)), and a tag
   ruleset on `refs/tags/v*` (*Release tags are never moved or deleted*,
   in the repository's *Rules* settings) refuses both.
5. **Create the release as a draft:**
   `gh release create vX.Y.Z --verify-tag --draft --title "Labrador X.Y.Z" --notes-file <notes>`.
   Publish it with `gh release edit vX.Y.Z --draft=false` only after the
   starter ZIP and the documentation asset below are uploaded: an immutable
   release takes no asset after publication. The notes hold:
   - two sentences from *The public face*, word for word: the bold lead,
     "Releases are versioned, and before 1.0 a release promises a record, not
     compatibility.", and the one after it that begins "A 0.x tag promises one
     thing about the tag before it". `v0.1.0` has no tag before it, so its
     notes add that it records every consumer-visible change since
     ColourWars' pin, `4cefb68`, which CHANGES.md's backfill covers;
   - a link to CHANGES.md at the tag,
     `https://github.com/matt439/Labrador/blob/vX.Y.Z/CHANGES.md`, at the
     release's own heading or, for `v0.1.0`, at
     `#moving-a-pin-from-4cefb68-in-short`;
   - the settle criterion, and what of it is unmet: the date of CHANGES.md's
     newest entry, and the 0.x tags cut since it;
   - the supported revisions, as [SECURITY.md](SECURITY.md) states them;
   - a link to PHILOSOPHY at the tag;
   - one line saying that `labrador-starter-vX.Y.Z.zip` is the Get Started
     project with this release inside it, built without Git, and linking
     `https://labradorengine.com/docs/get-started/starter/`.

   **Attach the starter ZIP**, which Get Started's *Get the starter project*
   sends readers to download, from `v0.3.0` on. Make it on Windows, in an x64
   Native Tools Command Prompt, from a clean checkout of the tag; the script
   refuses a tree that is not exactly the tag, or whose `version-string` is
   not the release's. From the checkout's root:

   ```text
   git fetch origin --tags
   git checkout vX.Y.Z
   python tools\consumer_check\starter_zip.py out\release-assets --release vX.Y.Z
   ```

   It prints the ZIP's path, size and SHA-256. Check it the way a reader
   will use it, in a folder that does not exist yet, then delete the folder:

   ```text
   mkdir C:\dev\starter-check
   tar -xf out\release-assets\labrador-starter-vX.Y.Z.zip -C C:\dev\starter-check
   pushd C:\dev\starter-check\mygame
   type README.txt
   cmake --preset x64-debug
   cmake --build --preset x64-debug
   ctest --preset x64-debug
   ```

   `README.txt` must say `Labrador version: vX.Y.Z`, and ctest must pass
   both tests. Then return to the checkout's root, which `pushd` remembered,
   attach the ZIP to the draft and leave the checkout on `master`:

   ```text
   popd
   gh release upload vX.Y.Z out\release-assets\labrador-starter-vX.Y.Z.zip
   git checkout master
   rmdir /s /q C:\dev\starter-check
   ```

   Never replace it (`--clobber`). CI's `consumer` job has already built the
   same project from a ZIP of the tagged commit, made the same way, so this
   check is for the file itself.

   From `v0.3.0`, publish the release's documentation snapshot with it, by
   [the versioning policy's procedure](docs/website-versioning.md#publishing-a-releases-documentation):
   - in a clean checkout of the tag, from `website/`, run `npm ci` and
     `npm run snapshot:build -- vX.Y.Z`, review the candidate, then run
     `npm run snapshot:pack -- vX.Y.Z` and keep the entry it prints;
   - attach `release-assets/labrador-docs-vX.Y.Z.tar.gz` to the draft with
     `gh release upload vX.Y.Z release-assets/labrador-docs-vX.Y.Z.tar.gz`,
     and only then publish it with `gh release edit vX.Y.Z --draft=false`.
     Never replace an asset (`--clobber`);
   - append the entry to `website/published-versions.json` in a pull request,
     with `published` set to the release's UTC publication date, and
     rebase-merge it on green; the `website` job's `versions:verify` checks it;
   - check the deployed index, label, links, search and selector, and that
     earlier versions are unchanged. Roll back through Cloudflare's deployment
     history; never edit or remove a published entry, and correct a snapshot
     with an appended erratum or a newer release.
6. **Afterwards**, the next consumer-visible change opens `## Unreleased`
   above the release's heading, and `version-string` stays as it is until the
   next release commit changes it.

## Commit messages

An imperative subject with no full stop, then a prose body that gives the
reason for the change rather than a list of what it touched. The history is
read as the record of why the tree is the shape it is.

## Golden image review records

Review every changed PNG under `tests/render/golden/` before committing it.
After looking at an image, put one exact repo-relative path on its own line
in that commit's message:

```text
Golden-Reviewed: tests/render/golden/a-frame-nobody-drew-into-is-cleared-to-opaque-black.png
```

Use the path you reviewed. Incidental filenames, partial paths and globs do
not count. Each commit that changes an image needs its own declaration;
a later commit's list does not cover earlier changes. Renaming between golden
PNG paths is checked as removal plus addition and needs both paths. Movement
into or out of that set is an addition or deletion. For a deletion, review the old
image in its parent and the decision to remove that reference, then name the
old path. This records the review declaration; the checker cannot establish
what somebody saw or whether their judgment was sound.

From the repository root, check every introduced commit in a local range:

```text
python tools/check_golden_reviews.py --base BASE_SHA --head HEAD
```

Replace `BASE_SHA` with the actual comparison commit. Each commit is compared
with its own parent. Missing objects, shallow history and merge commits fail
explicitly; the command does not fetch history. To inspect just one commit:

```text
python tools/check_golden_reviews.py --commit HEAD
```

That second form does not check ancestors. The existing CI lint job uses full
history and the pull request's actual base/head SHAs, rather than its synthetic
merge checkout. Ordinary pushes check before/head. A first push with no before
commit, a manual run or a scheduled run explicitly checks only the event head;
those limited runs do not stand for a pull request range. Missing declarations
return exit 1; incomplete history or unsupported scope returns exit 2.
Git must support `--no-lazy-fetch`; an older Git fails before reading objects.
The checker disables implicit promisor fetches and replacement refs so its
results describe available local objects and their actual commit messages.

## Before opening a pull request

For a render change, review the matrix with
`python tools/verify_changes.py --area render`, or select render paths with
`--path`. The [verification contract](docs/verification.md) explains its JSON
output, optional read-only tooling/cache preflight and explicit `--execute`
sweep. Execution includes device and browser tests, stops at the first failure,
and requires complete registrations/results. Review its effects and existing
runtime authorization before running it. Record actual checks and unrun
qualifications separately from a printed plan or fake-command test result.

- Build and test locally on the preset closest to what you changed
  (`cmake --preset x64-debug`, `cmake --build --preset x64-debug`,
  `ctest --preset x64-debug`). A change under `engine/render/` should be
  checked against every backend, and one in `gl/` under a `wasm-*` preset too;
  CI builds all of them, but it is slower than finding out first.
- For a code change, run `python tools/ci_gap_report.py --execute` on the
  branch head and paste its output under the pull request template's
  "Configurations CI cannot run". It builds and tests what CI does not: the
  GL and Vulkan pixel and smoke tests, the release presets CI skips and
  `wasm-debug`. Name the device it ran on;
  [the procedure](docs/verification.md#configurations-ci-cannot-run) says
  what the record does and does not show.
- If you edited a public engine header, a line a website page quotes, or a
  file a design document links to, build the website (`npm ci` then
  `npm run build` inside `website/`).
- Read [docs/design/PHILOSOPHY.md](docs/design/PHILOSOPHY.md) before arguing
  for a change of direction: its trade-offs, T1 to T12, are the vocabulary
  review uses.

## The agent brief

[AGENTS.md](AGENTS.md) is the agent brief, and the only copy of it: amend
AGENTS.md alone. [CLAUDE.md](CLAUDE.md) is not a second copy; its first line is
`@AGENTS.md`, which Claude Code reads as an import of the whole brief, and
anything below that line is for Claude Code alone. The build fails if that
first line is anything else
([cmake/check_agent_brief.cmake](cmake/check_agent_brief.cmake)).
