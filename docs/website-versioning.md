# Website documentation versions

The default website build describes development Labrador. The footer says
**Development documentation (unreleased)** and links to the checkout commit.
Local edits, including changes to the website itself, add an uncommitted-changes
notice. Source links keep that commit rather than following a moving branch.

On 8 October 2026, the public repository had no tags or published GitHub
releases. This was checked with `git ls-remote --tags origin` and the GitHub
releases API. A package's `version` field, a CMake Release configuration, and a
commit are not engine releases. There is no version selector or archive yet.

## First documentation snapshot: v0.3.0

On 27 October 2026 the owner chose `v0.3.0` as the first release with a
complete, immutable documentation snapshot. `v0.1.0` and `v0.2.0` will not be
backfilled. Each snapshot is built from its release's own clean checkout:
authored guides, generated API reference, design documents, sample quotations,
media and dependency lockfile all come from that revision. The build must not
combine released headers with development guides.

`SITE#3` in [TODO.md](../TODO.md) tracks the implementation and qualification,
which must be complete before `v0.3.0` is cut. The decision sets no release
date. Until the first snapshot is published, the whole site remains development
documentation.

Keep the current site and `/docs/` as development documentation. Store release
snapshots under `/versions/<release-tag>/`, with the same page paths below that
prefix. For example, a release's reference lives under
`/versions/<release-tag>/docs/reference/`. These are a routing policy for the
first snapshotted release, not URLs that exist today. An explicit release index and version
selector list only snapshots that were successfully built and published, plus
Development; a tag by itself does not add an entry.

Every release page shows its release tag and full source-commit link. Source
links resolve to that commit. Archived pages offer a source link instead of an
edit link to the development branch. The version selector keeps the current
page only when it exists in the chosen version; otherwise it takes the reader
to that version's documentation overview and says why.

Published snapshot paths are permanent. Publishing a newer release preserves
older snapshots. Their presence is a record of the API at that release, not a
promise that the release is still supported. Corrections to a frozen snapshot
are dated errata linked beside it, rather than a silent replacement with newer
documentation. A release tag must not be moved to rewrite a snapshot.

## Release-source verification

The first `SITE#3` slice provides a read-only source gate. From `website/` in
the release's checkout, after the owner has created its annotated tag, run:

```text
npm run --silent snapshot:source -- v0.3.0
```

[snapshot-source.mjs](../website/scripts/snapshot-source.mjs) writes a JSON
record to standard output: schema version, tag, full commit, committer date
and intended `/versions/<tag>/` path. Failure exits nonzero, writes its reason
to standard error and emits no source record. The command creates no files,
tags or builds and publishes nothing. A record identifies candidate source;
it is not an entry in the future published-release index, engine qualification
or a successful website build. Do not list a release as published from this
record alone.

Verification requires the checkout's own Git root, a stable tag at or after
`v0.3.0`, an annotated tag whose name and peeled commit match the request and
`HEAD`, a matching `vcpkg.json` version and a tracked website dependency
lockfile. Tracked changes, untracked files, ignored additions in the website's
root or input folders, hidden index flags, tracked symlinks and submodules are
refused. Ignored build output and installed dependencies remain permitted.
Git replacement objects and caller-supplied Git routing variables cannot
substitute another source tree. `LABRADOR_COMMIT` and `LABRADOR_COMMIT_DATE`
must be unset; this first gate requires Git and refuses source archives.

The source checks and command-line behavior have offline fixtures in
[snapshot-source.test.mjs](../website/tests/snapshot-source.test.mjs), run by
`npm test`. Candidate snapshot builds and routing are implemented below. The
published-release manifest, selector, preservation and publication qualification
remain open. No release snapshot is published and no version selector exists yet.

## Candidate snapshot builds

From `website/` in the release's clean checkout, with dependencies installed
using its tracked lockfile, run:

```text
npm ci
npm run snapshot:build -- v0.3.0
```

[build-snapshot.mjs](../website/scripts/build-snapshot.mjs) verifies the release
source, runs that checkout's Astro build in release mode, checks the output,
and verifies the source again before writing `dist/snapshot/snapshot-build.json`.
It fails if the source changes during the build or checks. The output is a flat
candidate site to be mounted at `/versions/<tag>/`; it does not include a second
copy of that prefix as a directory. The JSON contains the source record and
page/link/search counts. It is candidate build evidence, not a published-release
manifest or engine qualification.

The build sets `LABRADOR_DOCS_TAG` for its child process. Configuring Astro with
that variable also invokes the source gate, so running Astro directly cannot
substitute an unverified tag or the development archive override. Each release
build reads the guides, API, design documents, samples, media and lockfile from
its own checkout. Its pages show the release tag and full commit link, and
**View source** replaces the edit link. Development retains its existing label
and edit links.

Astro's base path handles bundled assets, sidebar routes and search loading.
The site's Markdown, MDX, reference and code-link renderers route their links
through the same prefix; route middleware covers hero and pagination links
outside Markdown. [snapshot-output.mjs](../website/scripts/snapshot-output.mjs)
then parses every emitted HTML page, checks its label and full source commit,
requires its canonical URL, and checks internal links, images, video posters
and `srcset` targets against the isolated output. Current-source links must use
the full release commit. A guide's deliberate historical comparison uses an
explicit `data-release-example` link to a stable release tag; it cannot point
at a branch or an edit page.

The output check also runs the emitted Pagefind JavaScript/Wasm API in an
isolated worker whose fetches can read only this snapshot. It retrieves all
indexed pages, performs an indexed-term query, and checks every returned URL.
The existing build validator checks internal anchors and sample quotations.
These checks do not establish browser search-UI, keyboard, theme or phone
acceptance, or verify that a fixture commit exists on GitHub.

`dist/snapshot/` is disposable candidate output. A development `npm run build`
replaces `dist/`, so retain reviewed candidates elsewhere before building
development documentation. Publication still needs the preservation mechanism
below; neither command deploys or changes a published index.

### Candidate build evidence: 2026-10-27

A separate local Git fixture copied this change's complete source, set a
synthetic `0.3.0` package version and annotated tag, and installed its exact
lockfile with `npm ci`. `snapshot:build` passed: 146 HTML pages, 26,997 checked
internal references and 145 searchable pages. Its full internal link/anchor
validation passed. The fixture exposed and corrected hero frontmatter routing
and authored links that had followed `master`. The development build and the
release-to-development transition were also checked, together with the website
type check and offline source/routing/output suites.

This is qualification of the candidate-build mechanism with synthetic source,
not an engine release, a published snapshot or real-device UI acceptance.
The attempted visual preview stopped because this Windows browser does not
support Computer Use URL-policy enforcement; visual acceptance remains open.
The real release's own clean checkout and source links must still pass the
release procedure.

## Work required for v0.3.0

Implement and qualify the versioned build before cutting `v0.3.0`. Prepare and
publish the first snapshot with these checks:

1. Add a release manifest binding each successfully published tag to its full
   commit and snapshot path. Candidate source/build records do not establish
   publication. Keep the existing source and output gates in the pipeline.
2. Preserve and assemble independently built snapshots with development
   documentation. Refuse a changed replacement at an existing published path;
   retain older releases and isolate each version's search index.
3. Add the release index and selector, retaining release labels and archived-page
   source links. Check switching both a shared page and a page absent from an older
   version, by keyboard and at phone width in both themes.
4. Run `npm ci`, `npm run check`, `npm test` and `npm run build` in the release checkout.
   Retain the matching engine qualification and the built site together. Check
   that API anchors and sample quotations resolve within this release, and
   exercise source links against its recorded commit.
5. Publish the reviewed snapshot without removing earlier versions. Keep the
   previous deployment available for rollback and verify the deployed version
   label, links, search and assets after publication.

The current [website workflow](../.github/workflows/website.yml) validates and
retains build artifacts; it does not deploy. Hosting and the domain are recorded
in [the website plan](website-plan.md). Versioning does
not choose a host, publish the site or establish an engine release schedule.
