# Reviewed Action selectors and alerts-only reporting

SM-92 prepares reviewed upstream Action sources and records an independent
repository-settings change. The workflow source has local validation;
**hosted execution of these new selectors remains unrun** until the owner
publishes the branch and its required checks execute. Earlier green runs
used older selectors and do not qualify this update.

## Selected sources, 17 November 2026

The canonical selection is [.github/action-versions.json](../.github/action-versions.json).
Each release tag was read from its official `actions/*` repository, resolved
to its full commit through GitHub's tag-reference API, and its tagged
`action.yml` compared with the same file at that commit. Workflow references
use the full commit, with the release version beside it for review. These
are deliberate source choices, not moving-major runtime-label substitutions.

| Action | selected release | commit |
|---|---|---|
| checkout | [7.0.1](https://github.com/actions/checkout/releases/tag/v7.0.1) | `3d3c42e5aac5ba805825da76410c181273ba90b1` |
| cache (including post-save) and restore | [6.1.0](https://github.com/actions/cache/releases/tag/v6.1.0) | `55cc8345863c7cc4c66a329aec7e433d2d1c52a9` |
| setup-node | [7.1.0](https://github.com/actions/setup-node/releases/tag/v7.1.0) | `949feb2413d6458794dcd2491c4babbbce0c15c1` |
| upload-artifact | [7.0.2](https://github.com/actions/upload-artifact/releases/tag/v7.0.2) | `cf430e030ddbb5b0abf93d22962f4752f3646cd9` |
| download-artifact | [8.0.2](https://github.com/actions/download-artifact/releases/tag/v8.0.2) | `9000827ccba6bdab643e8b6fd33ac0654aef8333` |

The selected metadata declares `node24`; the selected packages use ESM. The
documented minimum Actions Runner is **2.327.1**. Checkout's authenticated
Git commands from a Docker container require 2.329.0; these workflows have
no container jobs. Hosted runner labels remain moving selections, and their
actual runtime evidence belongs to the first published execution. This does
not change `website/.node-version`, Node engine ranges or Emscripten's Node.

## Behaviour reviewed before selection

- [Checkout](https://github.com/actions/checkout/blob/v7.0.1/README.md)
  moves persisted credentials into a separate file under `RUNNER_TEMP` and
  refuses unsafe fork-PR checkout in privileged event contexts by default.
  Every use explicitly keeps `allow-unsafe-pr-checkout: false`. The privileged
  `master watch` still checks out `master`, and its trigger/permissions stay
  unchanged. Ordinary PR checkout remains an unprivileged PR-event path.
- [Setup-node](https://github.com/actions/setup-node/blob/v7.1.0/README.md)
  adds automatic npm-cache detection and removes a dummy auth-token fallback.
  Every use has `package-manager-cache: false`; existing explicit `cache: npm`
  and lockfile paths remain. No use sets `registry-url`, `always-auth` or a
  publishing token, so no registry-auth migration is selected.
- [Upload 7](https://github.com/actions/upload-artifact/releases/tag/v7.0.0)
  can upload one file without wrapping it, which would ignore the chosen
  artifact name. Every use explicitly sets `archive: true`, preserving the
  named zip-archive handoff and directory/multiple-file uploads. Names,
  paths, retention, hidden-file and missing-file choices remain as authored.
- [Download 8](https://github.com/actions/download-artifact/releases/tag/v8.0.0)
  understands direct uploads and changes digest mismatch from a warning to
  an error. Every use keeps `skip-decompress: false` for the existing archive
  handoff and explicitly requires `digest-mismatch: error`. A bad digest is
  a failure, not a warning bypass.
- [Cache](https://github.com/actions/cache/blob/v6.1.0/README.md) retains the
  v2 service interface while moving its runtime/dependencies to Node 24/ESM.
  Existing cache keys, paths and save/restore conditions remain. Reader-only
  `cache/restore` uses remain that sub-action; they do not become writers.

The same selectors and guards cover the required CI/website workflows,
the optional native-null sanitizer and weekly configurations, the shared
Emscripten composite, and both ordinary/optional starter CI recipes. No job,
matrix, required context, permission, trigger or schedule is selected by
this Action update. The sanitizer and weekly source are prerequisites from
their own owners, not new settings changes made here.

`tools/check_action_selectors.py` is a compiler-free, network-free source
check. It rejects refs outside the reviewed inventory, mutable version tags,
unreviewed sub-actions and missing/changed migration guards. It also checks
optional starter recipes and repository-owned local Action paths. Its
literal `uses`/`with` reader ignores block-scalar script text and refuses
unproven guard shapes; it is not a general YAML evaluator, a proof that an
upstream review occurred, or hosted runtime qualification. Offline fixtures
exercise good/bad refs, guard scope and duplication, named/quoted steps,
script text, local paths and actual repository source. The suite runs with
the existing offline tooling tests; no new required job is added.

## Actual alerts-only setting change

The initial authenticated audit established admin access to the exact public
repository `matt439/Labrador`. `GET /repos/matt439/Labrador/vulnerability-alerts`
returned 404 with the explicit disabled message. Both the repository's
`security_and_analysis.dependabot_security_updates` and
`GET /repos/matt439/Labrador/automated-security-fixes` reported automatic
security updates disabled.

The accepted alerts-only scope was applied with exactly one write:

```text
gh api --hostname github.com --method PUT -H "X-GitHub-Api-Version: 2026-03-10" repos/matt439/Labrador/vulnerability-alerts
```

That request returned 204. Subsequent GETs returned 204 for alerts enabled,
`enabled: false, paused: false` for automatic security fixes and `disabled`
in the repository security setting. The auto-merge setting remained false.
No automatic-update, merge, workflow dispatch, repository-message or other
setting endpoint was written. GitHub documents the alerts and security-fix
controls as [separate endpoints](https://docs.github.com/en/rest/repos/repos#enable-vulnerability-alerts).

One initial post-enable page at `2026-11-16T19:26:02Z` returned 26 open alerts,
below the 100-entry cap, all against `website/package-lock.json`. One was
labelled critical for dompurify; that is advisory metadata, not a claim
about exploitability or production exposure. The npm graph is handled by a
separate reviewed dependency triage, not mixed into these Action pins.
Alerts do not establish coverage for every vcpkg or vendored-source input.
The recorded rapidjson commit/tree and engine baseline are preserved.

## Qualification boundaries

Local validation checks the source inventory, guard refusal fixtures,
required-context/weekly/sanitizer contracts, golden-history checkout scope,
starter assembly and website generation. A whole-workflow structural
comparison against the pre-update source permits only the reviewed refs and
explicit migration inputs; it retains other workflow behaviour. The [weekly inventory](verification-weekly.md)
reflects the registered Debug browser tests.

Actionlint was not available on PATH or in the previous ignored `out/lint`
location, so it is not recorded as run. No new standing lint tool or toolchain
pin is selected. Source validation cannot execute GitHub's cache
or artifact service or prove runner compatibility. Keep the full published
required matrix, the sanitizer/weekly non-required runs, privileged watcher
behaviour, cache save/restore and artifact handoffs explicit until observed
on these exact selectors. The optional starter browser recipe still needs
its own hosted proof. No push, PR, dispatch or publication is performed here.
