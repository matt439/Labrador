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
   entry to [CHANGES.md](CHANGES.md) in the same commit, or says
   `not consumer-visible` in its message. CHANGES.md's opening paragraph
   defines the term and its rule says what an entry holds: for a source break,
   the old spelling, the new one and the sample diff that migrates it. Review
   checks this before merging, because nothing else does.
   A commit that changes what `samples/minimal/` does or how it builds, or an
   engine behaviour that folder depends on, in a way a copy of it would want,
   adds a line to the change list in
   [samples/minimal/README.md](samples/minimal/README.md) in the same commit.
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
rename its check. The required contexts are exactly these eight:

- `windows (x64-debug)`
- `windows (x64-release)`
- `windows (x64-debug-d3d12)`
- `windows (x64-debug-gl)`
- `windows (x64-debug-vulkan)`
- `windows (x64-debug-null)`
- `wasm`
- `website`

The first seven are jobs in [.github/workflows/ci.yml](.github/workflows/ci.yml)
and the last is the job in
[.github/workflows/website.yml](.github/workflows/website.yml).
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

## Commit messages

An imperative subject with no full stop, then a prose body that gives the
reason for the change rather than a list of what it touched. The history is
read as the record of why the tree is the shape it is.

## Before opening a pull request

- Build and test locally on the preset closest to what you changed
  (`cmake --preset x64-debug`, `cmake --build --preset x64-debug`,
  `ctest --preset x64-debug`). A change under `engine/render/` should be
  checked against every backend, and one in `gl/` under a `wasm-*` preset too;
  CI builds all of them, but it is slower than finding out first.
- If you edited a public engine header, a line a website page quotes, or a
  file a design document links to, build the website (`npm ci` then
  `npm run build` inside `website/`).
- Read [docs/design/PHILOSOPHY.md](docs/design/PHILOSOPHY.md) before arguing
  for a change of direction: its trade-offs, T1 to T12, are the vocabulary
  review uses.

## The agent brief

[CLAUDE.md](CLAUDE.md) and [AGENTS.md](AGENTS.md) are one brief under two
names, one for each agent that reads them. They are identical below their
title line, and the build fails if they are not
([cmake/check_agent_brief.cmake](cmake/check_agent_brief.cmake)): amend one,
copy it to the other, and keep each file's own title.
