# v0.3.0 rebuild comparison — 2026-11-17

The explicitly invoked SM-91 check rebuilt both v0.3.0 assets and matched the
downloaded immutable originals by complete-file SHA-256 and byte count. This
qualifies the [manual comparison procedure](../../../release-rebuild.md) for
the source, tools and Windows CRLF convention recorded here. Nothing was
published, uploaded, signed, retagged, normalized or replaced.

## Verified identities

The [published release](https://github.com/matt439/Labrador/releases/tag/v0.3.0)
was read through GET-only public GitHub endpoints. Its saved
[release response](release.json) records immutable release ID `432411331`,
publication `2026-11-03T08:09:22Z`, `draft=false` and `prerelease=false`.
The saved [tag reference](tag-ref.json) and [annotated object](tag-object.json)
agree with the clean detached checkout:

- tag: `v0.3.0`;
- annotated tag object: `85317a889167bf2c45f9682d8fb34831bc5745de`;
- source commit: `0f4f5033b317a334d89e3cf14ab98684e0598ddd`.

The tool compares the named starter/docs pair; unrelated extra release assets
are outside this check. The documentation identity also agrees with the
maintenance checkout's unchanged `website/published-versions.json`.

| Asset | Bytes, original and rebuilt | SHA-256, original and rebuilt |
| --- | ---: | --- |
| `labrador-starter-v0.3.0.zip` (asset `704813193`) | 9,265,469 | `25db98b280e50648831ca896dad5bbd7b51788a8051dd29089ce3faa410f5869` |
| `labrador-docs-v0.3.0.tar.gz` (asset `704825288`) | 8,853,554 | `c61377d4f4cb67d9b83d65e160cf14192f75ded07771a252b6a4c02fdab46602` |

## Source, command and tools

A separate release worktree was created from a local Labrador repository:

```bat
git -c core.autocrlf=true worktree add --detach C:\Projects\Labrador-release-v030-sm91-final v0.3.0
```

From the separate maintenance worktree at base `ad711c05`, the final driver
was invoked with:

```bat
python tools\release_rebuild.py --release v0.3.0 --source C:\Projects\Labrador-release-v030-sm91-final --output C:\Projects\Labrador-agent-release-rebuild\out\v030-final-2026-11-17 --convention "Windows; git -c core.autocrlf=true worktree add --detach; raw bytes; no normalization" --execute
```

[report.json](report.json) retains the driver's own raw-file SHA-256, the
release packers and lockfile hashes, exact child commands/paths, timeouts,
exit codes, timestamps, PIDs and results. The final driver used Python 3.14.2
64-bit with zlib `1.3.1.zlib-ng`, Node 24.14.0 with zlib `1.3.1-e00f703`,
npm 11.9.0, Git 2.52.0.windows.1 and Windows 11 build 26300. No native engine
or game compilation is part of rebuilding these two distribution archives.

The [EOL inventory](checkout-eol.txt) records 1,039 tracked working-tree files
with CRLF, two with LF and 83 classified as binary. This is the release source
checkout, not the assembled ZIP entry count. The tag's own starter packer and
snapshot source/build/pack scripts were executed; the development assembler
was not substituted. `npm ci --no-audit --no-fund` installed the release's
locked dependencies. The snapshot build produced 172 HTML pages and passed
its source, output and link checks. Source verification passed before and
after packaging. The sum of recorded command runtimes was about 44 seconds;
this is contextual execution time, not a performance benchmark.

Two preparatory CRLF runs also matched while the driver was being hardened.
The retained final run used the driver SHA in its report and includes the
owned-process-tree timeout cleanup path. Those earlier outputs were kept and
were not substituted into this report.

## Retained evidence and limits

The complete local evidence remains at
`C:\Projects\Labrador-agent-release-rebuild\out\v030-final-2026-11-17`:
downloaded `originals/`, independently produced `rebuilt/`, all metadata,
stdout/stderr logs, source/EOL records and the report. The
[file identity index](local-evidence-files.json) records the raw local bytes
of all 29 files, including both original and rebuilt archives. Keep that
directory if the disposable source worktree is later removed. The adjacent
committed metadata and selected logs make the identity/result reviewable
without adding duplicate release archives to Git. Git checkout line-ending
conversion may change a text copy's bytes; the index describes the preserved
local evidence, not a future checkout of those text copies.

Twenty offline fixtures cover exact matches, equal-size mismatches, missing
inputs/tools, changed originals, malformed metadata, moved tag/commit
identities, fresh output, plan-only behavior, download limits, incomplete
builds and timeout output/owned-tree cleanup. A real child-process fixture
also checks output and exit capture. The maintenance website gate passed:
86 files with zero diagnostics, its tests, 186 generated pages and valid
internal links. `python tools/lint_local.py --base ad711c05` passed all 13
stages, including 517 Python tests (three skipped). The loaded host's full
Python suite took about 511 seconds; this is execution context, not a timing
threshold.

This run adds no LF-checkout result, other release/toolchain/platform claim,
signature or attestation. The
[2026-11-13 comparison](../../../website-versioning.md#rebuild-comparison-2026-11-13)
and its original CRLF/LF and compression caveats remain unchanged. Matching
sizes alone would not have passed. A future mismatch is retained as a raw
mismatch unless a separately named comparison explains it; it never changes
the accepted digests or publication date.
