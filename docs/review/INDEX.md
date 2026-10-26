# Review index

This is the current map of reviews and their live ledgers. The review text is
historical: its findings, counts, source paths and line numbers describe the
revision it reviewed. Do not rewrite that text as fixes land. Update the
owning status file, or the tracking section below where no separate status
file exists, in the same commit as a resolution. Record evidence or a reasoned
rejection; an unchecked candidate is a question to verify, not a confirmed bug.

[TODO.md](../../TODO.md) indexes open work, and
[BACKLOG.md](../product/BACKLOG.md) orders the stories. This file identifies
the evidence and owner; it does not put a frozen review ahead of that order.

## Every entry in this folder

| Entry | Historical source | Current status owner and what remains |
|---|---|---|
| [README.md](README.md) | The original ArtAttack review, before the repository split | [IMPLEMENTED.md](IMPLEMENTED.md) records all 36 critical fixes; [round-2/PLAN.md](round-2/PLAN.md), section 6, closes the extraction plan and its tail. Frozen; no independent engine queue remains from this review. Old game paths refer to ColourWars. |
| [all-findings.md](all-findings.md) | Detailed findings from the original ArtAttack review | Frozen companion to README.md; use the same remediation records, not its original severity counts as a current backlog. |
| [findings.json](findings.json) | Raw original ArtAttack findings | Frozen companion data; same status owners as README.md. |
| [IMPLEMENTED.md](IMPLEMENTED.md) | Original-review remediation record, dated 2026-08-09 | Frozen final record. Its old game-side residuals are outside this standalone engine; the round-2 plan records the completed engine extraction. |
| [round-2/](round-2/README.md) | Second review and appendices, before the split | [PLAN.md](round-2/PLAN.md), section 6: the plan and its tail are spent, nothing open. Frozen. |
| [rtcd/](rtcd/README.md) | *Real-Time Collision Detection* candidate sweep | Frozen candidate list, not an implementation plan. Decisions belong to [round-2/PLAN.md](round-2/PLAN.md), whose engine work is complete. Reopen a technique only against a current need. |
| [backend-equivalence/](backend-equivalence/README.md) | Three-backend audit at `57b65b3`; INVENTORY, TEST-GAP, DRIFT and GAPS are frozen too | Superseded by the five-backend sweep. Use [backend-equivalence-2/STATUS.md](backend-equivalence-2/STATUS.md) and its [LEDGER.md](backend-equivalence-2/LEDGER.md) adjudication; the original DRIFT.md is not live. |
| [backend-equivalence-2/](backend-equivalence-2/README.md) | Five-backend sweep at `ca5b2e3` | [STATUS.md](backend-equivalence-2/STATUS.md) is live: G6a now has its native-null live Application/viewport regression; the named low-tier p99 remains. See [remaining verification](#remaining-verification-and-hardware). |
| [d3d12/](d3d12/README.md) | Fourth-backend review | [STATUS.md](d3d12/STATUS.md) adjudicates section 6, the shader-profile experiment, measured submission allocation reuse and full patterned mip read-back. Heap-query cost and a real display-change session remain. |
| [vulkan/](vulkan/README.md) | Fifth-backend review at `4d91b91` | [CANDIDATES.md](vulkan/CANDIDATES.md) and [41-65](vulkan/CANDIDATES-41-65.md) preserve and adjudicate every original. [Vulkan tracking below](#vulkan-tracking) owns remaining follow-ups and the two section-6 questions, including the platform re-entry condition. |
| [gpt6/](gpt6/README.md) | Review at `9dea4516` on 2026-09-17 | The eleven numbered findings are closed in [the resolution map below](#gpt-6-resolution-map). Opportunities remain in TODO.md as REV#7 to REV#10 and the low-tier record; they are separate from those findings. |
| [2026-09-26/](2026-09-26/README.md) | September 26 review, supplement and reproduction evidence | [STATUS.md](2026-09-26/STATUS.md) is live and [FIXES.md](2026-09-26/FIXES.md) records all 23 numbered resolutions. The four hardening candidates, manual limits and tooling CI follow-up remain separately tracked; see below. |
| [colourwars-reports/](colourwars-reports/README.md) | Engine decisions on private client reports, assessed at `0c15f34` | Its README's decision table is live. Report 19 still needs evidence of a visible launch before the first present. PI-09's public filing/disclosure decision belongs to the owner. Keep private client detail out of this tree. |
| INDEX.md | This map | Live. Maintain this roster when adding or retiring a review, and the tracking below when resolving an item it owns. |

## D3D12 tracking

The confirmed findings were addressed by the nine-commit series from
`6ae4a15` through `06f0b5f`. Commit `e92a982` explicitly resolves section 6's
items 7 and 8, the shared-header/backend counts and design-document counts.
The [live status](d3d12/STATUS.md) now adjudicates the seven behavior questions,
retaining their original numbers. Source duplication alone established no
defect for items 1 and 4. A creation-only experiment refutes item 6's claimed
5_1 minimum for the named D3D12 configuration; the selected build profiles
remain unchanged and the ledger records the experiment's limits.

Source: [D3D12 review, section 6](d3d12/README.md#6-not-verified).
Work index: **REV#3** in TODO.md.

- [x] **6.1** Both view-opening loops and the existing restart cases were
  checked; no behavior mismatch was found. No count-only refactor is owed.
- [ ] **6.2** Check per-flush descriptor-heap-start calls and the analogous
  back-buffer-view calls before changing their caching.
- [x] **6.3** A warmed Debug CRT regression reproduced the temporary array's
  allocations; retained scratch capacity removes them in the measured scope.
  Analytic images preserve declared-view order and empty-view behavior.
- [x] **6.4** The descriptions are field-equivalent. Their duplication stays
  inside the backend, without a demonstrated behavior defect.
- [ ] **6.5** Check the D3D11 factory-refresh branch against current resize
  callers, including monitor moves and no-size-change arrivals.
- [x] **6.6** The current shader/pipeline accepted full 4_0, 5_0 and 5_1 on the
  recorded RTX 5080 configuration. Comments now state selected targets and
  rendered-output tolerance instead of a minimum or identical arithmetic.
- [x] **6.9** Real-factory patterned RGBA and BC1/2/3 chains are copied back
  at every mip, including block tails, and compared row by row after removing
  API padding. The ledger states the tested formats, dimensions and limits.

## Vulkan tracking

Source: [Vulkan review](vulkan/README.md), sections 6 and 7, and its
[all-findings.md](vulkan/all-findings.md) final candidate list. The verified
must-fix, should-fix and minor findings have their follow-up commits; the
later G9 closure is `6168700`. Those repairs do not turn unverified candidates
into verified findings.

- [ ] **6.1 / REV#4: minimize severity.** Run a sample under the Vulkan
  validation layers and record whether acquire or present returns
  `OUT_OF_DATE` while minimized, and the resulting behavior. This needs the
  actual interactive sequence; a source reading does not supply the result.
- [ ] **6.2 / REV#5: surface-platform questions, deferred.** Reopen with the
  Android port: check the format fallback's copy-versus-blit argument,
  `compositeAlpha` against supported modes, and whether `preTransform` must
  be applied. No supported desktop configuration establishes those answers.
- [x] **6.3: index-upload dependency.** `ecba539` adds the
  `TRANSFER_WRITE` to `INDEX_READ` barrier. This candidate also appears in
  section 7's frozen list and must not be counted as a fresh missing barrier.
- [x] **6.4: disclose thin frames-in-flight coverage.** `b4bcda0` adds the
  missing disclosure. This closes the documentation question, while the
  actual lifecycle coverage remains **REV#7**, below.
- [x] **Section 7: source adjudication.** All 65 originals are preserved
  verbatim and in order in [1-40](vulkan/CANDIDATES.md) and
  [41-65](vulkan/CANDIDATES-41-65.md), with current source dispositions.
  Confirmed fixes include injected submission cleanup, measured allocation
  reuse, instance diagnostics, exact CI filters, unused internal accessors
  and contract/prose corrections. The historical count is not a defect count.
- [ ] **REV#6: remaining bounded follow-ups.** The instance-callback question
  (20) remains open without an established application lifecycle contract.
  Device-selection policy (18) is documented without changing scoring. Transfer
  resilience (47) has bounded offline-tested retries, the reset ordering (60)
  is retained across distinct exception boundaries, and the early frame alias
  (62) is removed. Platform questions (19/21) remain REV#5;
  actual overlapping-work proof (51) remains REV#7; real device-removal
  recovery (1) remains REV#12. Source dispositions do not supply those actual
  platform or device sessions.
- [x] **Candidate 46: configure-time SDK minimum.** The real CMake finder
  now rejects synthetic 1.1 headers and accepts 1.2/newer metadata; an
  unversioned control demonstrates the original gap. All four offline cases
  and configure against the installed 1.4.357 SDK pass. This proves header
  admission, not loader, driver or SPIR-V compiler availability.
- [x] **Candidate 34: visible oversized-viewport behavior, 2026-10-26.**
  Analytic pixel tests cover right/bottom overflow, negative origins, panes
  wholly outside each edge and zero/quantized-zero extents followed by a valid
  pane. Every output pixel is checked without new goldens. All four native
  rasterizers pass, retaining local marker size and placement while clipping
  visible output to the buffer. Vulkan's full suite reports zero validation
  errors or synchronization hazards, with 769 performance advisories. This
  settles visible output on the tested configuration, not writes outside an
  attachment or every driver's behavior.

## GPT-6 resolution map

The numbered findings in [gpt6/README.md](gpt6/README.md) describe the reviewed
revision. These commits answer all eleven; they are an implementation record,
not a new run of the review. The September 26 review's
[historical findings and evidence limits](2026-09-26/README.md#historical-findings-and-remaining-evidence-limits)
also records the earlier repairs as present.

| Findings | Resolution commit |
|---|---|
| G6-01, G6-06: native window lifetime and restore dimensions | `fb542b1` |
| G6-02: ordinary state-stack destruction | `eb7336f` |
| G6-03: constant initialization | `e03606f` |
| G6-04: draw bounds and culling agree | `8dcab46` |
| G6-05: segment/corner traversal | `d373f71` |
| G6-07: pause close input leaking into play | `8e74546` |
| G6-08: pause opened under a held direction | `1a45474` |
| G6-09: content rooted beside the executable | `92966b0` |
| G6-10: targeted builds run architecture checks | `3a70b82` |
| G6-11: actual locked-piece particle reconstruction | `984cf13` |

## Remaining verification and hardware

These entries point to their existing live owners. They do not claim that a
green headless suite supplies a human session or a hardware measurement.

| Work | Live owner and remaining evidence |
|---|---|
| **REV#1**, five-backend G6a, closed 2026-10-26 | [backend-equivalence-2/STATUS.md](backend-equivalence-2/STATUS.md), section 3: native-null `AppTests` resizes a live Application and compares the actual HWND size with both caches. Raster-backend/interactive acceptance is not claimed. |
| Named low-tier p99, **SM-07** | The same STATUS.md, section 4: the Radeon/four-core measurement, after SM-01 to SM-05. EC2's L4 results do not substitute for this machine. |
| **REV#7**, GPT-6 opportunity 2 | [Presentation lifecycle evidence below](#presentation-lifecycle-evidence): native sequences, Debug diagnostics and instrumented D3D12 pending-work waits pass. Two-slot overlap and Vulkan pending-work proof remain; retained queues exclude final device destruction. |
| **REV#8**, GPT-6 opportunity 4 | Bounded XAudio2 silent-output retries and six deterministic policy cases are implemented; native XAudio2/null audio regressions pass. Real endpoint loss/reappearance, retained-bank playback and suspension/resume remain unverified. |
| **REV#9**, GPT-6 opportunity 3 | TODO.md: replay/file-version work is deferred until a client needs it (T1). |
| **REV#10**, GPT-6 opportunity 5 | TODO.md: the overlay remains deferred until a client needs it. `Application::frame_timings()` supplies phase measurements, not the proposed distribution overlay or low-tier p99. |
| **REV#11**, September 26 hardening | [September 26 README](2026-09-26/README.md#lower-priority-hardening-and-unverified-candidates): [hardening verification below](#hardening-verification) records all four candidate dispositions and their test limits. |
| **REV#12**, September 26 manual limits | [STATUS.md](2026-09-26/STATUS.md) and [FIXES.md](2026-09-26/FIXES.md): real device removal, interactive minimize/restore/drag, physical layouts/device changes and real sleep/wake need a person and devices. |
| **SM-19**, September 26 priority 3 | [STATUS.md](2026-09-26/STATUS.md): the offline Windows CI gate is implemented; the deliberately failing throwaway-branch CI observation remains open. Local PowerShell execution-policy limits are recorded there. |

When a status owner closes an item, update its TODO row and story box in the
same commit. Leave the historical finding readable, including any rejected
premise, so later work can distinguish a repair from a disproved claim.

## Presentation lifecycle evidence

**REV#7, partial result, 2026-10-26.** Native `RenderPixelTests` now presents
six frames, resizes the real hidden window, presents six more and checks one
final analytic image. Each frame uses two views and crosses two full vertex
pages plus a partial third. A separate case presents six frames and destroys
the renderer without any read-back. Resources outlive the renderer in both
cases. The tests are native-only and add no new golden images.

Both cases and the full pixel suite pass on D3D11, D3D12, GL and Vulkan.
Vulkan runs with the review's synchronization/best-practices layer settings;
that lifecycle batch's full suite reports zero validation errors or synchronization
hazards and 696 performance advisories, mostly small dedicated allocations.
The D3D12 debug interface was available (`D3D12GetDebugInterface` returned
`S_OK`) and the debug backend enabled it, but that initial run did not capture
its device diagnostic queue as a test assertion.

**Debug D3D12 diagnostic follow-up, 2026-10-26.**
[d3d12_presentation_diagnostics_tests.cpp](../../tests/render/d3d12_presentation_diagnostics_tests.cpp)
requires a real `ID3D12InfoQueue` and first retrieves an exact application INFO
message as a calibration control. Missing Debug diagnostics fail the case;
Release omits these two cases. Scoped empty storage/retrieval filters expose
all workload messages and are restored afterward. Initialization before the
calibration clear is outside this capture; cumulative filter/discard counters
are compared against the snapshot after that clear.

The lifecycle case presents six frames through two views, each crossing two
full vertex pages and a partial third, resizes the hidden window, then presents
six more frames. Actual slot observations record three uses of each of the two
slots before resize and three after. There is no read-back or explicit test
wait. The renderer performs its teardown wait before the test releases texture
resources and inspects the retained queue. All 176 retrieved workload messages
are INFO; none is warning/error/corruption, filtered or discarded during the
capture. The separate calibration control and lifecycle case pass 77 and 394
assertions respectively. The full Debug D3D12 run passes 163 headless render
cases and 56 pixel cases; four CMake lints and 21 website contract tests pass.

Retaining the queue also retains the D3D12 device. This verifies diagnostics
through engine-resource release, not final device destruction; messages from
initialization before the clear are also excluded.

Removing artificial per-frame read-back drains exercises renderer-owned reuse,
resize and destruction waits. It does not prove that this particular GPU was
still busy at each boundary: a hidden window can complete a present before
the next CPU operation. Keep REV#7 open for deterministic outstanding-work
proof rather than claiming frames in flight from test shape or clean captured
diagnostics alone.

**Instrumented D3D12 pending-work waits, 2026-10-26.**
The isolated `D3D12SynchronizationTests` executable compiles the actual
[DeviceResources translation unit](../../engine/render/d3d12/device_resources.cpp)
through [a test wrapper](../../tests/render/d3d12_instrumented_device_resources.cpp).
Only its `WaitForSingleObjectEx` call is redirected; production sources,
class layout and the normal engine binary are unchanged. Normal static linking
selects that object without whole-archive or duplicate-definition suppression.
The wrapper's defined `wait_for_frame` and external bounded-wait reference,
the recorded link command and successful runtime observations establish that
the isolated executable used the instrumented object.

[Three cases](../../tests/render/d3d12_pending_submission_tests.cpp) enqueue an
external queue fence wait, a witness signal and real renderer drawing without
presenting. The witness remains exactly zero immediately before the operation
and at its observed production wait, whose event is also unsignaled. The test
then CPU-signals its gate and requires normal completion and witness value one;
device-removal sentinel values cannot pass. The operations are same-index
`begin_frame`, a real hidden-window resize and renderer destruction. A separate
queue-tail drain precedes test-owned resource cleanup.

The production request is still observed as `INFINITE` and non-alertable, but
the adapter forwards a finite five-second wait. A ten-second host watchdog is
armed before the queue wait; missing observer calls, bad waits or an expired
boundary fail the isolated process after opening its gate. The timeout/fail-fast
branches were reviewed, not deliberately triggered during these normal runs.
Debug and Release each pass all three cases and 31 assertions, together with
their normal `RenderTests` and full `RenderPixelTests`; four standalone CMake
checks also pass. The D3D12 presets now register 21 CTest entries.

This establishes pending work at the instrumented D3D12 wait boundaries. It
does not exercise an unmodified infinite wait, prove both swapchain slots
executing concurrently, or establish Vulkan's corresponding pending-work
behavior. Retained test fences/queue also exclude final device destruction.
Those limits keep REV#7 open.

## Hardening verification

The September 26 hardening candidates are separate from its 23 numbered
findings. This is the current disposition of **REV#11**:

- [x] **Transactional name insertion, 2026-10-26.** `NameTable::add` and
  `Registry::add` erase a newly inserted name if the storage append throws.
  Public-API throwing-move regressions verify failed-name absence, existing
  handles, an unrelated add and same-name retry without aliasing. Native
  `CoreTests` passes. Clang compiles the changed code, but replacing the
  existing `out/build/wasm-release/tests/core/CoreTests.js` is access denied;
  no Node runtime pass is claimed. The caller's vector element/storage
  exception guarantees remain those of the underlying vector.
- [x] **Both sides of a modifier, 2026-10-26.** Each window tracks the two
  physical sides of Shift, Control and Alt, withholding an aggregate key-up
  until both are released and clearing the tracking on focus/suspension loss.
  Native `AppTests` drives real hidden windows with both release orders,
  repeats, loss paths and window isolation. The portable DOM translator runs
  the equivalent key-edge cases in that target on every native backend and
  compiles under clang with the browser window. Browser event delivery and
  physical keyboard layouts remain the separate manual/runtime limits.
- [x] **Exceptional cleanup, 2026-10-26.** A scene submission failure drains
  callbacks already borrowing its objects before rethrowing the original
  failure. Vulkan buffer construction owns cleanup through memory selection,
  allocation, binding and mapping; failed API outputs become owned only after
  success. Upload begin/end-recording failures also free their command buffer.
  GL texture upload/table insertion already has a scoped owner. These failure
  paths were independently reviewed, without allocator/API failure injection.
  Normal scene regressions pass on all native backends; Vulkan's full pixel
  suite passes after the guards with synchronization validation: zero errors
  or synchronization hazards, 696 performance advisories. The normal-path run
  is not evidence that each exceptional branch executed.
- [x] **Glyph ink outside layout measurement, 2026-10-26.** Text caches the
  union of the layout rectangle and the cells drawing actually visits, while
  retaining the layout origin. Synthetic-font regressions cover negative y
  bearings, drawable whitespace, setter invalidation, transforms and real
  scene culling with the null renderer. `Font::measure()` stays unchanged.
  Focused tests pass on all five native backends and the changed code/tests
  compile under clang; all four native rasterizers pass the existing golden
  set. This establishes the custom-font counterexample and its fix, not a
  defect in the checked-in sample fonts.
