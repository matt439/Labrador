# Additional weekly configurations

`.github/workflows/weekly-configurations.yml` runs the five configurations
outside ordinary required pull-request CI. It shares the existing Monday
03:17 UTC timing and supports explicit manual dispatch. It has no push or
pull-request trigger. Its five `weekly native (...)` / `weekly browser (...)`
contexts are non-required; the twenty required contexts in
[CONTRIBUTING](../CONTRIBUTING.md#the-procedure) remain unchanged.

| Configuration | Required entries | Hosted execution |
|---|---:|---|
| `x64-release-d3d12` | 23 | Explicit WARP, checked by name and software classification. |
| `x64-release-gl` | 22 | Mesa llvmpipe, checked by name. |
| `x64-release-vulkan` | 22 | Chrome's SwiftShader, checked by name. |
| `x64-release-null` | 23 | Recorded render/audio; no graphics or audio API device. |
| `wasm-debug` | 52 | Chromium pages and eleven Firefox entries, with Firefox on ANGLE/WARP. |

Each job checks its exact existing `tests/ctest_inventory.json` selection
before running the whole configured suite, without CTest filters or
exclusions. Missing Chromium or Firefox therefore fails registration instead
of silently reducing browser coverage. Full test output is retained in JUnit
and `LastTest.log`; browser verbose output preserves the browser/device
evidence. Result artifacts are retained for fourteen days, including failed
golden images and native device reports where produced.

## Configuration and qualification boundaries

Native Release explicitly selects the
[qualified MSVC IPO policy](performance/native-release-ipo.md). GL and
WebGL2 enable the material-tool feature for their authored fixtures. Vulkan
uses the pinned SDK and the same software-driver setup as ordinary CI.
Emscripten, ccache, Vulkan and Mesa pins match `ci.yml`; offline fixtures
refuse divergence. The Debug browser build has its own
`wasm-debug-weekly-gl` ccache namespace, while reusing ordinary non-sanitized
vcpkg binaries and the installed SDK cache.

The [sanitizer workflow](verification-asan.md#scheduled-execution) stays
separate, on Monday 03:47 UTC or explicit dispatch. It still selects
`x64-debug-null-asan`, its 24-entry inventory, the instrumented dependency
triplet, allocation-failure coverage and detection/instrumentation probes.
The additional weekly workflow neither replaces that selection nor adds its
context to branch protection. The different start time can reduce setup
contention; it does not guarantee that the workflows cannot overlap.

Hosted software results establish neither physical-GPU equivalence nor
listening, real-pad, phone or display-pacing acceptance. The same seven
release/driver obligations remain in `tools/ci_gap_report.py`: real-driver
Debug GL/Vulkan pixel and smoke tests, the four additional native Release
suites and the full Debug browser suite. A scheduled run of an earlier
commit cannot qualify a later release candidate. The
[release/driver procedure](verification.md#release-and-real-driver-qualification)
therefore remains required before a release is tagged.

## Runtime and cost evidence

Actual hosted time and cache cost for these additional profiles are
unmeasured until the first published run. The workflow permits four native
jobs alongside one browser job, subject to GitHub's available concurrency.
Each job has a ninety-minute timeout; builds and vcpkg use two workers.
Native jobs do not cancel each other after a failure. These bounds are not
a measured duration or a billing estimate.

The first hosted qualification records each job's source SHA, runner image,
toolchain/device evidence, exact result count, cache state and setup,
configure, build and test durations from the job log and retained artifacts.
Later tuning uses those observations; the existing ordinary-CI timing
record is not a measurement of this lane or of Release IPO on a hosted runner.

## Verification and publication state, 2026-11-17

The offline `test_weekly_configurations.py` fixtures hold schedule-only
selection, the five preset inventories, complete CTest execution, tool pins,
software-device setup, the twenty required contexts and ASan separation.
`test_ci_gap_report.py` continues to derive the local release/driver gap from
ordinary required CI, independently of this weekly coverage.

```text
python -m unittest tools.tests.test_weekly_configurations tools.tests.test_required_checks tools.tests.test_check_ctest_inventory tools.tests.test_ci_gap_report
python tools/ci_gap_report.py
```

On 2026-11-17, the 39 focused workflow, required-context, inventory and gap
fixtures passed. All thirteen local lint stages passed, including the
505-test offline Python suite with one environment-dependent skip. The three
changed YAML documents parsed, both verification tools printed their plans
without executing them, and independent source review found no actionable
workflow defect. The combined website gate remains part of integration.

The November 17 local qualification preceded workflow publication.

**First hosted qualification, checked 2026-11-20.** GitHub listed the workflow
as active on `master` at `f7b717c4` and reported zero runs. After its first
run, retain the URL, the five results and their timing and cache evidence here
before closing SM-89. [Weekly configuration runs](https://github.com/matt439/Labrador/actions/workflows/weekly-configurations.yml)
