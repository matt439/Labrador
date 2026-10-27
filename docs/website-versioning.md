# Website documentation versions

The default website build describes development Labrador. The footer says
**Development documentation (unreleased)** and links to the checkout commit.
Local edits, including changes to the website itself, add an uncommitted-changes
notice. Source links keep that commit rather than following a moving branch.

A package's `version` field, a CMake Release configuration, a commit and a tag
are not published documentation. A release has documentation on the site only
when [the published-release manifest](#published-release-manifest) lists it.
`v0.1.0` and `v0.2.0` are tagged and have GitHub releases, but no snapshots.
The manifest lists no releases yet, so the release index at `/versions/` and
the version selector offer Development alone.

## First documentation snapshot: v0.3.0

On 27 October 2026 the owner chose `v0.3.0` as the first release with a
complete, immutable documentation snapshot. `v0.1.0` and `v0.2.0` will not be
backfilled. Each snapshot is built from its release's own clean checkout:
authored guides, generated API reference, design documents, sample quotations,
media and dependency lockfile all come from that revision. The build must not
combine released headers with development guides.

`SITE#3` in [TODO.md](../TODO.md) tracks the qualification and first
publication that remain, which must be complete before `v0.3.0` is cut. The
decision sets no release date. Until the first snapshot is published, the whole
site remains development documentation.

Keep the current site and `/docs/` as development documentation. Store release
snapshots under `/versions/<release-tag>/`, with the same page paths below that
prefix. For example, a release's reference lives under
`/versions/<release-tag>/docs/reference/`. `/versions/` itself is the release
index, built only by the development site; no `/versions/<release-tag>/` path
exists until its snapshot is published. The release index and version selector
list only snapshots that were successfully built and published, plus
Development; a tag by itself does not add an entry.

Every release page shows its release tag and full source-commit link. Source
links resolve to that commit. Archived pages offer a source link instead of an
edit link to the development branch. The version selector keeps the current
page only when it exists in the chosen version; otherwise it takes the reader
to that version's documentation overview and says why.

Published snapshot paths are permanent. Publishing a newer release preserves
older snapshots. Their presence is a record of the API at that release, not a
promise that the release is still supported. Corrections to a frozen snapshot
are dated errata listed beside it on the release index, rather than a silent
replacement with newer documentation. A release tag must not be moved to
rewrite a snapshot, and a published entry or asset is never edited or replaced.

## Release-source verification

The source gate is read-only. From `website/` in the release's checkout, after
the owner has created its annotated tag, run:

```text
npm run --silent snapshot:source -- v0.3.0
```

[snapshot-source.mjs](../website/scripts/snapshot-source.mjs) writes a JSON
record to standard output: schema version, tag, full commit, committer date
and intended `/versions/<tag>/` path. Failure exits nonzero, writes its reason
to standard error and emits no source record. The command creates no files,
tags or builds and publishes nothing. A record identifies candidate source;
it is not an entry in the published-release manifest, engine qualification
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
must be unset; this gate requires Git and refuses source archives.

The source checks and command-line behavior have offline fixtures in
[snapshot-source.test.mjs](../website/tests/snapshot-source.test.mjs), run by
`npm test`.

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
manifest entry or engine qualification.

The build sets `LABRADOR_DOCS_TAG` for its child process. Configuring Astro with
that variable also invokes the source gate, so running Astro directly cannot
substitute an unverified tag or the development archive override. Each release
build reads the guides, API, design documents, samples, media and lockfile from
its own checkout. Its pages show the release tag and full commit link, and
**View source** replaces the edit link. Development retains its existing label
and edit links. A release build has no `/versions/` page of its own: its
selector and footer link to the development site's index.

Astro's base path handles bundled assets, sidebar routes and search loading.
The site's Markdown, MDX, reference and code-link renderers route their links
through the same prefix; route middleware covers hero and pagination links
outside Markdown. [snapshot-output.mjs](../website/scripts/snapshot-output.mjs)
then parses every emitted HTML page, checks its label and full source commit,
requires its canonical URL, and checks internal links, images, video posters
and `srcset` targets against the isolated output. Current-source links must use
the full release commit. A guide's deliberate historical comparison uses an
explicit `data-release-example` link to a stable release tag; it cannot point
at a branch or an edit page. The one link allowed outside the prefix is an
`<a data-version-index>` whose target is exactly
`https://labradorengine.com/versions/`; any other target with that attribute,
and every unmarked link outside the prefix, is refused.

The output check also runs the emitted Pagefind JavaScript/Wasm API in an
isolated worker whose fetches can read only this snapshot. It retrieves all
indexed pages, performs an indexed-term query, and checks every returned URL.
The existing build validator checks internal anchors and sample quotations.
These checks do not establish browser search-UI, keyboard, theme or phone
acceptance, or verify that a fixture commit exists on GitHub.

`dist/snapshot/` is disposable candidate output. A development `npm run build`
replaces `dist/`, so pack a reviewed candidate before building development
documentation. Neither command deploys or changes the published manifest.

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

## Published-release manifest

[website/published-versions.json](../website/published-versions.json),
committed on `master`, is the only record of which snapshots are published.
Only development builds read it; a release build never does, and the copy at a
release's own tag is irrelevant. Schema version 1 has exactly these keys:

```json
{
  "schemaVersion": 1,
  "releases": [
    {
      "tag": "v0.3.0",
      "commit": "<full lowercase commit>",
      "sourceDate": "YYYY-MM-DD",
      "path": "/versions/v0.3.0/",
      "asset": "labrador-docs-v0.3.0.tar.gz",
      "bytes": 1234567,
      "sha256": "<64 lowercase hex digits of the asset>",
      "published": "YYYY-MM-DD"
    }
  ],
  "errata": [
    {
      "tag": "v0.3.0",
      "date": "YYYY-MM-DD",
      "title": "One line",
      "href": "/docs/..."
    }
  ]
}
```

[published-versions.mjs](../website/scripts/published-versions.mjs) validates
it. The text must be canonical: that key order, two-space indentation, LF line
endings, one trailing newline, no byte-order mark. `.gitattributes` keeps the
file LF on Windows checkouts. Unknown and missing keys are refused at every
level. Each tag is a stable tag at or after `v0.3.0`, listed once; `path` and
`asset` are derived from it; `commit` is a full lowercase hash; dates are real
calendar dates; `bytes` is positive and at most 256 MiB. An erratum names a
listed tag, has a one-line title, and links either to a development site path
outside `/versions/` or to `https://github.com/matt439/Labrador/`.

The download URL is never stored. It is always
`https://github.com/matt439/Labrador/releases/download/<tag>/<asset>`, so
editing the manifest cannot redirect a download.

The manifest is append-only. Every earlier release and erratum stays
byte-identical at its position; new ones are only appended, and a tag that was
ever listed cannot be added again. Append order is publication order. The
index and selector sort releases by version, so a backport such as `v0.3.1`
may be appended after `v0.4.0`. There is no withdrawal: a bad publication is
rolled back at the host and corrected with an erratum or a newer release.

## Packing a snapshot

```text
npm run snapshot:pack -- v0.3.0
```

[pack-snapshot.mjs](../website/scripts/pack-snapshot.mjs) verifies the release
source again, requires `dist/snapshot/snapshot-build.json` to record that same
source and verification, and verifies the candidate output again, with the
same result. It then writes
`website/release-assets/labrador-docs-<tag>.tar.gz`, refusing to overwrite an
existing file, and prints the manifest entry for it on standard output. The
`published` date in that entry is the day the command ran, in UTC. The command
never uploads, tags, edits the manifest or deploys. `release-assets/` is
ignored by Git and outside the source gate's inputs.

[snapshot-archive.mjs](../website/scripts/snapshot-archive.mjs) owns the
format: gzip around a deterministic subset of ustar, holding regular files
only, sorted by byte order, with fixed modes, owners and times. A path that does
not fit ustar's name fields is refused rather than written with an extension
header. Before decompressing, the reader checks the archive's length and
SHA-256 against the manifest. It then decompresses with a 512 MiB cap and
refuses: a second gzip member or trailing bytes; links, directories, extension
headers and devices; a bad header checksum or field; an entry over 25 MiB or
more than 20,000 entries; unsorted or duplicate paths; absolute, `..`,
backslash, colon, control-character, trailing-dot and Windows-reserved
names; the characters Windows forbids in names (`< > " | ? *`) and `#`, which
ends a URL's path as `?` does; a path under `versions/`; Cloudflare's
`_headers`, `_redirects` and `.assetsignore`; paths that collide after NFC
normalization and case folding, or a folder spelled two ways after them; and a
file that is also a folder. A snapshot must hold `index.html`, `404.html`, `docs/index.html`,
Pagefind's entry files and `snapshot-build.json`. Extraction writes into a new
`.<tag>.partial/` folder and never follows or overwrites anything.

## Assembly in the development build

`npm run build` is `astro build` followed by
[assemble-versions.mjs](../website/scripts/assemble-versions.mjs). Assembly
refuses to run when `LABRADOR_DOCS_TAG` is set; `snapshot:build` runs Astro
itself and never assembles. It validates the manifest and requires the
development output to be in place, with no leftover `dist/snapshot/` and
nothing in `dist/versions/` except the release index page.

For each listed release, in manifest order, assembly downloads the asset from
its GitHub release, or reads it from `LABRADOR_SNAPSHOT_CACHE/<asset>` when
that folder holds it. A download must end at an HTTPS URL, has a 120-second
timeout, is aborted once it exceeds the pinned size, and is retried up to
three times for a network error or a 5xx response only; a hash mismatch is
never retried. The archive is read as above, its `snapshot-build.json` must
name the entry's tag, commit, source date and path, and the extracted tree is
verified again with the output check before it is renamed to
`dist/versions/<tag>/`.

Assembly then writes `dist/versions/index.json` from the trees that mounted,
and checks that `dist/versions/` holds exactly the listed tags, the index page
and that file; that development's Pagefind files are byte-identical before and
after, so the development search index cannot list a snapshot's pages; and
that the deployment has at most 100,000 files, warning at 80,000. Any failure
removes the partial folders and exits nonzero, naming the tag and step. The
host then does not deploy, and the previous deployment stays live.

With no listed releases, assembly makes no network or Git request; it adds only
`/versions/index.html` and `/versions/index.json` to the development output.
Once a release is listed, every production build needs GitHub's release
downloads, and fails closed rather than deploy without a listed release.

## CI verification

The [website workflow](../.github/workflows/website.yml) checks out full
history and runs, after `npm test` and before `npm run build`:

```text
npm run versions:verify -- --base <commit>
```

[verify-published-versions.mjs](../website/scripts/verify-published-versions.mjs)
compares the manifest with the one at the base commit. The workflow passes the
pull request's base, or the previous commit of a push to `master` that was not
forced; every other run passes an empty base, because a branch push may be
forced and a forced push names an old tip the checkout no longer has. An empty
or zero base, or one that does not resolve in the checkout, falls back to
`git merge-base HEAD origin/master` with a message saying so, and a base with
no manifest counts as empty. The change must be append-only. For each
appended release, the GitHub release for its tag must be published, not a
prerelease and immutable, with exactly one uploaded asset of that name whose
size and SHA-256 digest match the entry; origin's annotated tag must peel to
the entry's commit; and the asset is downloaded and staged through the same
reader, extraction and output check as assembly. Finally, it reads the live
`https://labradorengine.com/versions/index.json`, retrying network errors,
timeouts, 5xx and 429 answers with assembly's backoff; 200 and 404 are
decisive. The index may be absent only while the base lists no release. At
every position both list, the live releases in publication order must have
this manifest's tag, commit and hash; either list may be longer, because the
live site lags a change that appends and a branch may lag `master`. When the
change appends a release, a disagreement or an unreadable index fails the
step, since publication depends on it. Otherwise the step reports it as a
warning, a GitHub Actions annotation, and passes, so a live-site outage or
rollback cannot block the change that repairs it.

After `npm run build`, the workflow runs the script's post-build phase:

```text
npm run versions:verify -- --base <commit> --dist dist
```

It checks only the errata appended since the base. A site path, without its
fragment, must be a page or file the development build emitted outside
`versions/`, and its fragment must be an element id in that page. A
repository URL must answer HTTP 200 to HEAD, or to GET where HEAD is refused,
with the same retries. Earlier errata are not checked again: their links
cannot change, so a later rename of a target must not block every deployment.

The check needs history and the network, so it is not part of `npm test` or of
the host's build. It does not compare the entry's `published` date with the
GitHub release's.

## Runtime contract, permanent from v0.3.0

Every snapshot freezes the code that reads these, so from `v0.3.0` they do not
change; a breaking change needs a new file name or parameter.

- `/versions/index.json`, written only by assembly on the development site and
  fetched literally, with `cache: 'no-cache'`, by every version's selector:

  ```json
  {
    "schemaVersion": 1,
    "versions": [
      { "id": "development", "label": "Development", "path": "/", "commit": "<full>", "pages": ["", "docs/", "..."] },
      { "id": "v0.4.0", "label": "v0.4.0", "path": "/versions/v0.4.0/", "commit": "<full>", "sha256": "<asset hash>", "published": "YYYY-MM-DD", "latest": true, "pages": ["..."] }
    ],
    "publishedOrder": ["v0.3.0", "v0.4.0"]
  }
  ```

  Development comes first, then releases newest first. `pages` lists each
  version's page routes relative to its `path`, without `404.html`;
  development's omits `/versions/`. `commit` is omitted from Development when
  the build has no full commit. Readers refuse another `schemaVersion`, ignore
  unknown fields, drop an entry whose `id` is neither `development` nor a
  stable tag or whose `path` does not match it, and take the latest release
  from the versions themselves.
- `data-version-index` marks the one link a snapshot may make outside its
  prefix, to exactly `https://labradorengine.com/versions/`.
- `?version-missing=<route>&from=<id>` on a version's `docs/` overview tells it
  which page the reader came from. The notice shows the route only if it is at
  most 200 letters, digits, `.`, `_`, `/` and `-` with no `..` or `//`, and
  `from` only if it is `development` or a stable tag; otherwise it shows a
  general sentence. The query is removed from the address after reading.

## Release index and selector

`/versions/` is a development page listing Development, with its commit, and
every published release newest first: a "Latest release" badge, the published
and source dates, links to the release's documentation, home page and source
tree, its full commit, its archive and SHA-256, and its errata. Until the first
release it says that no release documentation has been published. It is not
searchable.

The selector sits at the top of the sidebar on desktop and at the top of the
phone menu, once per page. Without JavaScript it is a `<details>` naming the
current version with a link to `/versions/`. With JavaScript, opening it, or
focusing or pointing at it, fetches the index; it then lists Development and
each release, the current one as text marked `aria-current`, and each other
version's link to the same page and anchor where that version has the page.
Otherwise the link goes to that version's overview and says so in visible text
that is part of its name. If the fetch fails the static list remains. Escape
closes it and returns focus, the arrow keys move between its links, and focus
leaving it closes it. The splash pages have no sidebar; every page's footer
links to `/versions/`. A version's overview shows the arrival notice above.

## Publishing a release's documentation

The owner publishes a snapshot as part of
[cutting the release](../CONTRIBUTING.md#cutting-a-release), after the tag is
pushed. GitHub immutable releases must be enabled on the repository first;
without them `versions:verify` refuses every entry. On 27 October 2026 the
repository setting was not enabled.

1. In a clean checkout of the tag, from `website/`: `npm ci`, then
   `npm run snapshot:build -- vX.Y.Z`. Review the candidate.
2. `npm run snapshot:pack -- vX.Y.Z`. Keep the printed entry.
3. Create the GitHub release as a draft with its notes,
   `gh release create vX.Y.Z --verify-tag --draft ...`, then attach the asset,
   `gh release upload vX.Y.Z release-assets/labrador-docs-vX.Y.Z.tar.gz`.
   An immutable release accepts no asset after publication, and an asset is
   never replaced: never use `--clobber`.
4. Publish the release: `gh release edit vX.Y.Z --draft=false`.
5. On a branch from `master`, append the printed entry to
   `website/published-versions.json`, with `published` set to the release's
   publication date in UTC, and open a pull request. CI runs
   `versions:verify` against it.
6. Rebase-merge on green. Cloudflare's build assembles and deploys the site.
7. Check the deployment: `/versions/index.json` lists the release with its
   commit and hash; `/versions/vX.Y.Z/` shows its label and full commit link;
   its search returns only its own pages; the selector switches a shared page
   and an absent one both ways; a missing page under the prefix gets a 404; and
   earlier versions are unchanged. If the deployment is wrong, roll back
   through Cloudflare's deployment history, then fix forward with an erratum or
   a newer release; the manifest entry is never edited or removed.

An erratum is published by appending it to `errata` in a pull request; CI
checks its link after the build, as above.

## Qualification evidence: 2026-10-27

Items 1 and 2 below, and the code of item 3, are implemented. Their offline
suites under `website/tests/` (manifest, archive, pack, assembly, CI
verification, selector logic and the output check's exemption) inject fetch,
Git and the clock, and run in `npm test`. The assembly and CI verification
tests use a generated fixture snapshot that passes the output check. A local
`npm run build` built the development site with an empty manifest and made no
network request: `dist/versions/` held only `index.html` and `index.json`, and
the index listed Development alone. A local `versions:verify` with an empty
base fell back to the merge base and accepted the live site's 404 for the
index.

### Headless Chrome qualification: 2026-10-27

This is emulation, not real-browser or real-device acceptance. A scratch Git
fixture copied the working tree, tagged and built two real `snapshot:build`
outputs, `v0.3.0` (146 pages) and `v0.4.0` (147, adding one guide that
`v0.3.0` lacks), and packed both. Listed with a synthetic erratum in the
fixture's own manifest, they were mounted by `astro build` and assembly with
`LABRADOR_SNAPSHOT_CACHE`: 1,319 files, and an index listing Development,
`v0.4.0` and `v0.3.0`. A local static server answered a missing path with 404
and the nearest `404.html`, emulating the host's 404 page setting. Headless
Chrome was driven over the DevTools protocol with real key events.

These passed:

- A shared page and heading switched through all three versions in both
  directions with Tab, Enter and the arrow keys alone, landing on the exact
  page and hash.
- The page absent from `v0.3.0` led to its overview from Development and from
  `v0.4.0`, with the visible note in the link's name, the query removed from
  the address, the canonical URL unchanged and the `role=status` notice naming
  the page; switching back from the overview worked.
- Space and Enter open the selector, the current version is text marked
  `aria-current`, the arrow keys stop at both ends, Escape returns focus to the
  summary, focus leaving closes it and the focus outline is 2 px.
- At 320, 360, 430 and 1440 pixels in both themes the page never scrolled
  sideways; on phones the selector is first in the menu, and every item is at
  least 44 px tall.
- Without JavaScript the selector links to `/versions/`, which lists every
  version newest first with the latest-release badge and the erratum.
- Search on each version, through Starlight's search box, returned only that
  version's pages.
- With `/versions/index.json` blocked the static list remained, without errors.
- A missing page under `/versions/v0.3.0/` got 404 and that release's 404 page,
  in emulation.

axe-core 4.10.3 found one defect: in the dark theme at 1440 pixels the
selector's absent-page note measured 4.48:1 on the sidebar, below 4.5:1. The
note and the "Checking versions" status now use `--sl-color-gray-2`, as the
links do; the fixed build has not been scanned again. axe found no other
violation on the selector pages, the notice or `/versions/` at 1440 and 360
pixels in either theme. The run also showed that from `/versions/`, which is
in no version, the selector reported the list page missing from every release;
it now opens each release's overview from there with no notice.

Not established: how screen readers announce the notice, which is filled in
after load; real phones and touch; Safari; Firefox; and the host's own 404
handling under `/versions/<tag>/`, unverified until the first publication.

## Work required for v0.3.0

Qualify the versioned build before cutting `v0.3.0`, then publish the first
snapshot with these checks:

1. A release manifest binds each published tag to its full commit, snapshot
   path and asset hash. *Implemented, as above.*
2. Independently built snapshots are preserved and assembled with development
   documentation; a changed replacement is refused, older releases are
   retained and each version's search index is isolated. *Implemented, as
   above.*
3. The release index and selector keep release labels and archived-page source
   links. *Implemented, and qualified in headless Chrome as above; acceptance
   open.* Acceptance repeats those checks on an assembled build in Safari and
   Firefox and on a real phone with touch, and checks with a screen reader that
   the arrival notice is announced.
4. Run `npm ci`, `npm run check`, `npm test` and `npm run build` in the release checkout.
   Retain the matching engine qualification and the built site together. Check
   that API anchors and sample quotations resolve within this release, and
   exercise source links against its recorded commit.
5. Publish the reviewed snapshot by the procedure above, without removing
   earlier versions. Keep the previous deployment available for rollback and
   verify the deployed version label, links, search and assets after
   publication.

Hosting and the domain are recorded in [the website plan](website-plan.md).
Versioning does not choose a host or establish an engine release schedule.
