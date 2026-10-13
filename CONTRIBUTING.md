# Contributing

How a change reaches `master`. This is the procedure the owner and every agent
working in this tree follow; it is written here because a rule that lives only
in someone's memory cannot be followed by anyone else.

## The procedure

1. **Branch from `master`.** One short-lived branch per change.
2. **Make one self-contained commit.** A commit is a complete change: the
   source, the tests that pin it, and every document it makes false, together
   (CONVENTIONS, and the design documents' rule that they change by amendment
   in the same commit as the change that fights them). A story in
   [docs/product/BACKLOG.md](docs/product/BACKLOG.md) has its box ticked in the
   commit that closes it.
3. **Open a pull request** against `master`.
4. **Wait for CI to go green.** Every configuration CI builds is a required
   check, and a change is not known to be clean until all of them pass: a
   change clean under MSVC is not yet known to be clean under clang, and an
   edit outside `website/` can fail the website build.
5. **Rebase-merge.** `gh pr merge --rebase`, or `--squash` if the branch grew
   fixup commits, so that `master` stays a linear history in which each commit
   stands alone. Merge commits are disabled in the repository's settings.

`master` is protected: it requires the checks above and a linear history, and
nothing reaches it except through a pull request. The owner can bypass the
protection for a trivial fix; nobody else does.

**The required checks are names held in GitHub's settings**, not in this tree:
the six `windows (...)` matrix jobs and `wasm` from
[.github/workflows/ci.yml](.github/workflows/ci.yml), and the website's
`build` from [.github/workflows/website.yml](.github/workflows/website.yml). A
matrix entry's check name is built from its values, so a pull request that
adds, removes or changes one must have the branch protection updated to match,
or every later pull request waits on a check that never reports.

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
