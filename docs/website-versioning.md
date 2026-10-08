# Website documentation versions

The website currently describes development Labrador. The footer says
**Development documentation (unreleased)** and links to the checkout commit.
Local edits, including changes to the website itself, add an uncommitted-changes
notice. Source links keep that commit rather than following a moving branch.

On 8 October 2026, the public repository had no tags or published GitHub
releases. This was checked with `git ls-remote --tags origin` and the GitHub
releases API. A package's `version` field, a CMake Release configuration, and a
commit are not engine releases. There is no version selector or archive yet.

## Once the engine has a release

A published engine release with a fixed tag and commit starts versioned
documentation. Each release gets a complete static snapshot built from its
own clean checkout: authored guides, generated API reference, design documents,
sample quotations, media and dependency lockfile all come from that revision.
The build must not combine released headers with development guides.

Keep the current site and `/docs/` as development documentation. Store release
snapshots under `/versions/<release-tag>/`, with the same page paths below that
prefix. For example, a release's reference lives under
`/versions/<release-tag>/docs/reference/`. These are a routing policy for the
first release, not URLs that exist today. An explicit release index and version
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

## Work required at the first release

Implement and qualify the versioned build in the same change that prepares the
first release's documentation:

1. Add a release manifest binding each published tag to its full commit and
   snapshot path. Reject a dirty checkout, a tag/commit mismatch, or an archive
   whose source identity cannot be verified. The existing `LABRADOR_COMMIT`
   override identifies an archive build; it does not prove release provenance.
2. Make generated API links, authored absolute links, sidebar links, assets,
   search results and canonical URLs respect the snapshot prefix. Build and
   validate each snapshot independently. Search within the selected version.
3. Add the release label, release index, selector and archived-page source
   links. Check switching both a shared page and a page absent from an older
   version, by keyboard and at phone width in both themes.
4. Run `npm ci`, `npm run check`, `npm test` and `npm run build` in the release checkout.
   Retain the matching engine qualification and the built site together. Check
   that API anchors and sample quotations resolve within this release, and
   exercise source links against its recorded commit.
5. Publish the reviewed snapshot without removing earlier versions. Keep the
   previous deployment available for rollback and verify the deployed version
   label, links, search and assets after publication.

The current [website workflow](../.github/workflows/website.yml) validates and
retains build artifacts; it does not deploy. Hosting and domain selection remain
the separate decisions in [the website plan](website-plan.md). Versioning does
not choose a host, publish the site or establish an engine release schedule.
