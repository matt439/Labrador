# Compare a release rebuild

This is an explicitly invoked, compare-only check of an already published
immutable Labrador release. It rebuilds the starter ZIP and documentation
archive using **that tag's own scripts**. It downloads and retains the
published originals, checks their recorded SHA-256 identities, and compares
the rebuilt complete files. It creates no tag, release, upload, signature,
attestation, schedule or manifest change.

The [2026-11-17 v0.3.0 run](verification/release-rebuild/2026-11-17/README.md)
qualifies the procedure below on one Windows checkout/toolchain convention.
The [earlier manual comparison](website-versioning.md#rebuild-comparison-2026-11-13)
remains its own dated evidence.

## Prepare a named source

Use a separate, fresh checkout of a stable annotated release tag. Fetching
tags is a deliberate manual step; this tool never fetches or changes them.
For the qualified Windows CRLF convention, from a local Labrador checkout:

```bat
git fetch origin --tags
git -c core.autocrlf=true worktree add --detach C:\dev\labrador-v030-rebuild v0.3.0
```

Choose a new path for each run. The command-line `-c` applies only to this
checkout operation; it does not change Git's global or repository settings.
The tool records actual `git ls-files --eol` results. Its `--convention`
description records how the checkout was made and never converts any file.

Use the current `tools/release_rebuild.py` from your maintenance checkout,
passing the release checkout separately. Do not substitute a development
`starter_zip.py --engine <old-checkout>`: the development assembler can have
changed since the release. The release must supply its own starter and
snapshot source/build/pack scripts, and its version must agree with the tag.

Install Python, Git, Node and npm before invoking the run. The current tool
uses Node and npm's adjacent `node_modules/npm/bin/npm-cli.js`, as the
standard Windows Node install provides. Missing tools or release inputs are
refusals, not passing comparisons. The documentation asset must already be
listed in the current maintenance checkout's append-only
`website/published-versions.json`. `--manifest` can name an explicitly chosen
copy; its values are retained and must agree with GitHub and the release
source. Unset `LABRADOR_COMMIT`, `LABRADOR_COMMIT_DATE` and
`LABRADOR_DOCS_TAG` overrides.

## Plan and explicitly execute

From the maintenance checkout, this prints the plan without creating output,
downloading or building:

```bat
python tools\release_rebuild.py --release v0.3.0 --source C:\dev\labrador-v030-rebuild --output C:\dev\rebuild-evidence\v030-first --convention "Windows; git -c core.autocrlf=true worktree add; raw bytes"
```

Repeat the same command with `--execute` to perform the bounded run. The
evidence directory must not exist and must be outside the release checkout.
Do not reuse a checkout containing `website/dist/snapshot` or the packed
documentation asset; those are preserved output, not fresh build input.

The execution:

1. Checks the clean local annotated tag and HEAD, then reads GitHub's release,
   tag reference and annotated tag object through GET-only endpoints. The
   release must be published, immutable and not a prerelease. The local and
   remote tag objects and source commits must agree.
2. Requires one starter ZIP and one documentation archive at their canonical
   release URLs, with GitHub SHA-256 digests and byte counts. The documentation
   manifest's commit/digest/size must also agree. Downloads are bounded and the
   received originals must match before any packaging command runs.
3. Runs the release's own snapshot source gate, then its starter packer;
   installs the locked website dependencies with `npm ci --no-audit --no-fund`;
   runs `snapshot:build` and `snapshot:pack`; and rechecks source identity.
   `npm ci` installs dependencies for this isolated build. It does not change
   the tracked lockfile or install an operating-system service.
4. Preserves `originals/`, `rebuilt/`, metadata, EOL evidence, exact commands,
   tool versions, stdout/stderr and `report.json`. Each command has its own
   timeout; source/tool probes have 60 seconds, starter/pack 180 seconds and
   dependency installation/documentation build 300 seconds each. A download
   has a 30-second socket timeout and a 120-second streaming bound.

The exit code is **0** for both exact matches, **1** for a completed comparison
with a mismatch, and **2** for incomplete/refused execution. A command timeout
retains its captured output and incomplete report. A prerequisite failure
before the evidence directory can be created is printed to stderr. Existing
evidence is never overwritten or removed by this tool.

## Read the result

The comparison rule is exact complete-file SHA-256 and byte count. Equal
sizes alone are insufficient. A mismatch retains both byte sequences and
their different digests; it never replaces the accepted release identity.
The published originals remain under `originals/`, even if a later build
command fails. Keep the evidence directory when cleaning up its disposable
source worktree.

Working-tree line endings affect the starter ZIP: the published v0.3.0 ZIP
was made with CRLF text. The tag's packer reads working-tree bytes, so an LF
checkout can differ without a source-commit change. Archive compression also
depends on the Python/Node zlib implementation; the report records both.
The documentation build depends on the tag's lockfile and tools, including
its installed native packages. One successful run does not establish other
tags, tools, operating systems or checkout conventions.

Investigate a mismatch using the preserved artifacts and command logs. If a
content-level comparison would help, name its separate rule and preserve the
raw mismatch; do not normalize line endings, timestamps or archive entries
and then label the original assets reproducible. The date printed by the
snapshot packer is the day it ran, not a replacement publication date. Never
copy it or a rebuilt digest into the published manifest as part of this check.
