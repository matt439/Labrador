# Current PR qualification, 2026-11-17

The owner requested a PR for the integrated branch after clearing the
previous autonomous goal. This record qualifies runtime source
`fadd1a6381c8c9dd90c9d6a4daf2fd55b4646c48`; the later bookkeeping change
only reconciles documentation. It integrates no isolated feature and makes
no release, deployment or hardware-acceptance claim.

The desktop device was the RTX 5080 with Windows driver `32.0.16.1692`.
The browser receipts identify Chrome `157.0.8103.78` on ANGLE hardware and
Firefox `160.0`; these versions are observations from this run.

## Complete local gates

`verify_changes.py --execute` completed all seven render/app tracks with a
clean source checkout. Its cache, post-build cache, exact selected
registrations and JUnit result checks passed, with no tests filtered out.
The source revision remained unchanged throughout the run.

| Selection | Result |
|---|---|
| `x64-debug` | 22/22; complete configure/build/CTest and registration/cache checks |
| `x64-debug-d3d12` | 23/23; complete configure/build/CTest and registration/cache checks |
| `x64-debug-gl` | 22/22; complete configure/build/CTest and registration/cache checks |
| `x64-debug-vulkan` | 22/22; complete configure/build/CTest and registration/cache checks |
| `x64-debug-null` | 23/23; complete configure/build/CTest and registration/cache checks |
| `wasm-debug` | 49/49; complete configure/build/CTest and registration/cache checks |
| `wasm-release-null` | 34/34; complete configure/build/CTest and registration/cache checks |

The five native Debug selections total 112/112; both complete browser tracks
add 49/49 and 34/34. Native rasterizing tests and the WebGL pages identify
their actual devices in the retained report/JUnit. Null uses no graphics or
audio API. Chromium and the registered Firefox entries keep their own
browser/renderer evidence; this is not physical-pad, listening or phone proof.

Current Vulkan `validate_sync` uses the shell-visible layer settings and
passes RenderPixelTests: 93 cases, 101,152 assertions, zero skipped or failed,
with no Validation Error or SYNC-HAZARD. The selected device is the RTX 5080;
the test elapsed 23.99 seconds. The wrapper report separately records its
whole-command duration and exact argv. Known layer warnings remain in the
unabridged log; their presence is not relabelled as a validation error.

The full local lint passed all thirteen stages over `f5c17f32..fadd1a63`:
635 Python tests with one environment-dependent skip. The website gate passed
check, 215 tests, a 196-page build, internal links, original preview-image
verification and archived snapshot assembly. These are local gates; hosted
required PR checks retain their own final source and result.

## Story closures and limits

This closes SM-86, SM-87, OCA-107, OCA-113 and OCA-115's combined-source
integration gates, plus SM-90's website and SM-93's full-lint gate. Their
focused and dated measurements keep their original source epochs. Display
metadata is nominal, cursor read-back is requested state, icon metadata is
not physical scaling, and API texture bounds/synthetic loss callbacks do not
measure a device's physical ceiling or hardware loss. No counter collection,
new benchmark or informal hardware session was performed for these closures.

The earlier failed collision protocol fixture at `7c830100` remains in the
[handoff](../2026-11-17-overnight-handoff.md). `fadd1a63` aligns its outline
key route with the shared Chromium/Firefox/Safari expectations and weekly
inventory; the fresh complete suite supersedes that failure for this source.
The [first checkpoint](2026-11-17-overnight-checkpoint.md) retains earlier
Release and ASan epochs. This PR run does not establish current-source native Release,
ASan, final-consumer or release-candidate qualification. Required hosted CI,
SM-89's first hosted weekly receipt, physical/phone/pad/listening/display
acceptance and owner-cut release steps remain separate. Isolated pending
scopes and unchanged deferred work are not closed by this record.

## Retained receipts

The execution report and seven JUnit files are under
`out/verification/pr-current-20261117/`. Full stdout/stderr and registration
receipts sit beside them. `out/overnight/pr-lint.log`, `pr-website.log`,
`pr-vulkan-sync.log` and `pr-vulkan-sync-report.json` retain the other gates.
Ignored local receipts are evidence in this checkout, not promised release
assets or files a remote checkout contains.

| Receipt | SHA-256 |
|---|---|
| `out/verification/pr-current-20261117/report.json` | `71e809ef3a496a0a23421c70ae638305154b5d9028a61d2d4b6ec28b4a11759e` |
| `out/overnight/pr-vulkan-sync.log` | `296797b987aa73f0ba6f9c50b361d92a0b41c24a1e6b1cb9a4aeb86f6cbc0fe8` |
| `out/overnight/pr-vulkan-sync-report.json` | `728ecddb431278df24c9fa3df44fd3d3991d845ef08b8dd4f4c47685b7fc765d` |
| `out/overnight/pr-lint.log` | `93dc9434132dd1eaeba4c77fb9e9b02ebfe4b1f251e3f0d32e9774c8a1b7be94` |
| `out/overnight/pr-website.log` | `ab3c70af5135c1f78b5a4895adb88dbeaca3d27da3415cf1b385c2e20ae4c136` |
| `out/verification/pr-current-20261117/x64-debug-junit.xml` | `23b0507e5d873c80632c031fac6ddd63462a789522c28a81d7e4dd9d8645e22c` |
| `out/verification/pr-current-20261117/x64-debug-d3d12-junit.xml` | `b1d15f964d90a301ea0e841a740cc5d728ba50d7ab932f35a8fff7b77132bd08` |
| `out/verification/pr-current-20261117/x64-debug-gl-junit.xml` | `1cb73b80ae1e25c0c1dd1920c803812591b37233459e57bd86861da2566f7642` |
| `out/verification/pr-current-20261117/x64-debug-vulkan-junit.xml` | `b6eba33e10200ca69eac29b0034d42199ddb4fdc0fa6b1ce3d9922e7439284a5` |
| `out/verification/pr-current-20261117/x64-debug-null-junit.xml` | `2c7088385a7b743795b89d9328644ffd02bb2a1f87ffebb65a89acb802cf0743` |
| `out/verification/pr-current-20261117/wasm-debug-junit.xml` | `20c97e21a3b2b038a2dbfa49ca2cf8962c976f1088608d52cbab4adb00130310` |
| `out/verification/pr-current-20261117/wasm-release-null-junit.xml` | `64d9fbde353e0d92f61473974206a1e2a69ec4a628d9ace760ecd26f4cec78c1` |
