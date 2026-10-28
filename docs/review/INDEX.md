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
| [d3d12/](d3d12/README.md) | Fourth-backend review | [STATUS.md](d3d12/STATUS.md) adjudicates section 6, the shader-profile experiment, submission allocation reuse, full patterned mip read-back and bounded heap-query measurements. A real display-change session remains. |
| [vulkan/](vulkan/README.md) | Fifth-backend review at `4d91b91` | [CANDIDATES.md](vulkan/CANDIDATES.md) and [41-65](vulkan/CANDIDATES-41-65.md) preserve and adjudicate every original. [Vulkan tracking below](#vulkan-tracking) owns remaining follow-ups and the two section-6 questions, including the platform re-entry condition. |
| [gpt6/](gpt6/README.md) | Review at `9dea4516` on 2026-09-17 | The eleven numbered findings are closed in [the resolution map below](#gpt-6-resolution-map). REV#7's bounded lifecycle observations are below; other opportunities remain in TODO.md as REV#8 to REV#10 and the low-tier record. |
| [2026-09-26/](2026-09-26/README.md) | September 26 review, supplement and reproduction evidence | [STATUS.md](2026-09-26/STATUS.md) is live and [FIXES.md](2026-09-26/FIXES.md) records all 23 numbered resolutions. The four hardening candidates, manual limits and tooling CI follow-up remain separately tracked; see below. |
| [colourwars-reports/](colourwars-reports/README.md) | Engine decisions on private client reports, assessed at `0c15f34` | Its README's decision table is live: all fifteen originals are answered and withdrawn from filing. Report 19's first-frame evidence is still absent; newer reports 23 and 25 remain. PI-09's approved disclosure boundary excludes private client detail. |
| [Vulkan device ownership](vulkan/2026-10-28-device-ownership.md) | Source-only SM-53 review at `15c6979`, 2026-10-28 | [Code hygiene](../code-hygiene.md#sm-53-vulkan-devicebuffer-ownership-review) closes the keep verdict; SM-56 owns the separate texture-publication and read-back-generation fixes/qualification. |
| [Renderer construction](2026-10-28-renderer-construction.md) | Source-only SM-54 review at `8048e24`, 2026-10-28 | [Code hygiene](../code-hygiene.md#sm-54-renderer-construction-review) closes the per-backend verdicts; SM-56/57 own confirmed exception gaps and SM-58 the selected D3D12 pipeline cut. |
| [Web Audio transport](2026-10-28-webaudio-transport.md) | Source-only SM-55 review at `8048e24`, 2026-10-28 | [Code hygiene](../code-hygiene.md#sm-55-web-audio-transport-review) closes the keep verdict. No extraction is selected; browser graph/unlock and WEB#3 listening acceptance remain separate. |
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
- [x] **6.2** [Ordinary getter-loop timings and the real workload's query
  census](../performance/2026-10-26-d3d12-descriptor-queries.md) support retaining
  the current calls on the named runtime. The count-based model is not a
  measured frame cost or predicted caching gain; other devices remain unmeasured.
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

The 2026-10-28 [device ownership](vulkan/2026-10-28-device-ownership.md) and
[construction](2026-10-28-renderer-construction.md) source reviews add three
confirmed exception-cleanup gaps, independent of the historical candidates:
wrapper allocation after texture raw-handle cleanup ends, read-back cleanup
against a replaced device when recovery throws, and a vertex module left
without cleanup if pixel-module creation throws. **SM-56** in
[Code hygiene](../code-hygiene.md#sm-56-vulkan-exceptional-cleanup) owns source
fixes, deterministic error-path coverage and render/synchronization qualification.
These source reviews ran no device or injected failure.

SM-56's source fixes and a compiled, unexecuted shader-failure case now have
[separate progress and remaining checks](../code-hygiene.md#sm-56-vulkan-exceptional-cleanup).
The native builds/headless runs there do not close the GPU error-path or
synchronization requirements.

Source: [Vulkan review](vulkan/README.md), sections 6 and 7, and its
[all-findings.md](vulkan/all-findings.md) final candidate list. The verified
must-fix, should-fix and minor findings have their follow-up commits; the
later G9 closure is `6168700`. Those repairs do not turn unverified candidates
into verified findings.

- [x] **6.1 / REV#4: minimize severity, 2026-10-27.** `MinimalSample` on
  `x64-debug-vulkan` at `a26e889`, on the RTX 5080 with NVIDIA 616.92, ran
  under the validation layer (core and synchronization validation, errors and
  warnings logged to a file) and the API dump layer. `ShowWindow` minimized and
  restored it once, then three more times in a second run. Each minimize got
  `VK_ERROR_OUT_OF_DATE_KHR` from `vkQueuePresentKHR`, never from acquire.
  The rebuild then found a zero extent and released the swapchain, and every
  minimized frame submitted and probed surface capabilities without acquiring.
  Each restore created a swapchain, after which acquire and present returned
  `VK_SUCCESS`. Validation reported nothing, and the sample quit with exit
  code 0. The throw no longer happens. With no present to wait on, though, the
  minimized loop was unthrottled: 8,429 frames in about 3.3 seconds.
- [ ] **6.2 / REV#5: surface-platform questions, deferred.** Reopen with the
  Android port: check the format fallback's copy-versus-blit argument,
  `compositeAlpha` against supported modes, and whether `preTransform` must
  be applied. No supported desktop configuration establishes those answers.
- [x] **6.3: index-upload dependency.** `ecba539` adds the
  `TRANSFER_WRITE` to `INDEX_READ` barrier. This candidate also appears in
  section 7's frozen list and must not be counted as a fresh missing barrier.
- [x] **6.4: disclose thin frames-in-flight coverage.** `b4bcda0` adds the
  missing disclosure. This closes the documentation question, while the
  bounded lifecycle execution evidence is recorded under **REV#7**, below.
- [x] **Section 7: source adjudication.** All 65 originals are preserved
  verbatim and in order in [1-40](vulkan/CANDIDATES.md) and
  [41-65](vulkan/CANDIDATES-41-65.md), with current source dispositions.
  Confirmed fixes include injected submission cleanup, measured allocation
  reuse, instance diagnostics, exact CI filters, unused internal accessors
  and contract/prose corrections. The historical count is not a defect count.
- [x] **REV#6: bounded follow-ups closed, 2026-10-27.** Candidate 20 now
  follows Vulkan's documented instance lifecycle contract: a `pNext`
  registration covers instance creation/destruction, and the persistent
  messenger covers calls between them. Both use the same stateless callback
  with null user data. The [candidate record](vulkan/CANDIDATES.md#candidate-20)
  links the primary contract and the registration/lifetime regression evidence.
  Device-selection policy (18) is documented without changing scoring. Transfer
  resilience (47) has bounded offline-tested retries, the reset ordering (60)
  is retained across distinct exception boundaries, and the early frame alias
  (62) is removed. Platform questions (19/21) remain REV#5;
  candidate 51's pending-work observation is recorded under closed REV#7;
  real device-removal
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

**Instance diagnostics and shutdown verification, 2026-10-27.** Candidate 20's
Debug regression passes all three cases and 63 assertions, after failing 21
assertions against the old registration. Its synthetic callbacks establish
registration and callable lifetime, not actual layer-message delivery to the
debugger. The stateless callback and null user data also remain valid when a
retained texture releases the shared instance after its renderer.

The full verification exposed a separate shutdown defect: `MinimalSmokeTests`
failed with `VK_ERROR_SURFACE_LOST_KHR` in both Debug and Release, including
with the lifecycle registration disabled. An update had closed the window
before that tick rendered. The shell now completes owed fixed updates, then
checks the neutral window handle before rendering. The
[native-null loop regression](../../tests/app/application_loop_tests.cpp)
fails four assertions on the old path and passes all 15 focused assertions
with the guard; the full `AppTests` suite passes 61 cases and 849 assertions.
Both the native window and browser canvas return a null handle after closing.

Final configure/build/CTest runs pass on `x64-debug` (20),
`x64-debug-d3d12` (21), `x64-release-gl` (20), `x64-release-null` (19), and both
`x64-debug-vulkan` and `x64-release-vulkan` (20 each), including the repaired
smoke path. `wasm-release` passes all 31 entries, including all sample pages,
and the website check, 21 tests and 146-page build pass. All four standalone
CMake checks pass. Local receipts are under
`out/instance-diagnostics-20261027/`; these runs add no physical pad, phone,
display-change or device-removal acceptance.

The Debug Vulkan pixel suite also passes under the review's synchronization
and best-practices settings with independent layer stdout logging: 61 cases,
49,680 assertions, zero validation errors and zero synchronization hazards.
It emits 794 performance advisories, including the known small dedicated
allocations. This executed sweep covers the tested paths, not real device
removal or every possible instance-lifecycle diagnostic.

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
| **REV#7**, GPT-6 opportunity 2, closed 2026-10-26 | [Presentation lifecycle evidence below](#presentation-lifecycle-evidence): both backends have actual presented-slot pending observations, controlled reuse/resize/teardown waits and final-device observations. Runtime and diagnostic limits stay explicit. |
| **REV#8**, GPT-6 opportunity 4 | Bounded XAudio2 silent-output retries and six deterministic policy cases are implemented; native XAudio2/null audio regressions pass. Real endpoint loss/reappearance, retained-bank playback and suspension/resume remain unverified. |
| **REV#9**, GPT-6 opportunity 3 | TODO.md: replay/file-version work is deferred until a client needs it (T1). |
| **REV#10**, GPT-6 opportunity 5 | TODO.md: the overlay remains deferred until a client needs it. `Application::frame_timings()` supplies phase measurements, not the proposed distribution overlay or low-tier p99. |
| **REV#11**, September 26 hardening | [September 26 README](2026-09-26/README.md#lower-priority-hardening-and-unverified-candidates): [hardening verification below](#hardening-verification) records all four candidate dispositions and their test limits. |
| **REV#12**, September 26 manual limits | [STATUS.md](2026-09-26/STATUS.md) and [FIXES.md](2026-09-26/FIXES.md): real device removal, interactive minimize/restore/drag, physical layouts/device changes and real sleep/wake need a person and devices. |
| **SM-19**, September 26 priority 3 | [STATUS.md](2026-09-26/STATUS.md): the offline Windows CI gate is implemented. The 2026-10-27 local run passed all 81 tests with no skips, superseding the local execution-policy blocker; on the same day a deliberately failing throwaway test turned the [lint job](https://github.com/matt439/Labrador/actions/runs/40470680489/job/122076786279) red, closing the story. |

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
[d3d12/presentation_diagnostics_tests.cpp](../../tests/render/d3d12/presentation_diagnostics_tests.cpp)
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
the next CPU operation. That sequence alone left outstanding-work proof open;
the bounded observations below extend it without inferring pending work from
test shape or clean captured diagnostics.

**Instrumented D3D12 pending-work waits, 2026-10-26.**
The isolated `D3D12SynchronizationTests` executable compiles the actual
[DeviceResources translation unit](../../engine/render/d3d12/device_resources.cpp)
through [a test wrapper](../../tests/render/d3d12/instrumented_device_resources.cpp).
Only its `WaitForSingleObjectEx` call is redirected; production sources,
class layout and the normal engine binary are unchanged. Normal static linking
selects that object without whole-archive or duplicate-definition suppression.
The wrapper's defined `wait_for_frame` and external bounded-wait reference,
the recorded link command and successful runtime observations establish that
the isolated executable used the instrumented object.

[Three cases](../../tests/render/d3d12/pending_submission_tests.cpp) enqueue an
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
Those are this probe's limits; the later observations below extend its scope.

**Instrumented Vulkan pending-work waits, 2026-10-26.**
[Three Vulkan cases](../../tests/render/vulkan/pending_submission_tests.cpp)
record real drawing, then hold its actual execute submission behind a test
timeline witness. A scoped `vkQueueSubmit` adapter adds only that timeline
wait at `ALL_COMMANDS`, retaining the command buffer, engine timeline signal
and fence. This is submission instrumentation as well as wait observation.
A binary bridge cannot depend on a future host semaphore signal
([VUID 03238](https://docs.vulkan.org/spec/latest/chapters/cmdbuffers.html#VUID-vkQueueSubmit-pWaitSemaphores-03238)).

At the backend's actual `vkWaitSemaphores` entry, each case requires witness
zero, an engine counter below the requested target and `VK_TIMEOUT` from a
real zero-time query. One worker then signals its gate exactly once, and the
adapter requires normal completion within five seconds. The worker's absolute
ten-second deadline stays armed through the operation and separate fenced
queue-tail retirement. Scoped cases exercise same-index `begin_frame`, a real
hidden-window resize and renderer destruction. Unarmed submit/wait/idle calls
forward their original arguments and timeouts. The subsequent device-idle
calls occur after timeline completion; they do not prove pending presentation
work. The shared device owner outlives test semaphores and retirement.

Debug and Release each pass the three cases and 45 assertions. Their full
native runs pass 163 headless render cases, plus 60 Debug and 59 Release pixel
cases. The Debug run uses the review's shell-visible synchronization settings:
zero validation errors, zero hazards, zero validation warnings and 791
performance advisory lines. No golden images changed. Timeout and fail-fast
branches were reviewed, not deliberately triggered in these normal runs.

This adds controlled evidence that the timeline waits precede reuse and
resource teardown while submissions are pending. It does not establish an
unmodified submission/infinite wait, concurrent work in both presented slots,
pending presentation at device idle or final device destruction. The later
observations below extend this evidence without changing those limits.

**D3D12 destruction callback observation, 2026-10-26.**
The opt-in [D3D12DestructionProbe](../../tests/render/d3d12/destruction_probe.cpp)
links the ordinary engine. It requires Debug, queries `ID3D12InfoQueue1` and
`ID3DDestructionNotifier` on the actual device, and registers separate message
and destruction callbacks. Both diagnostic interfaces are released before
the workload. An exact INFO control is observed before that release; a second
temporary query and exact INFO control after the workload checks persistence
of the original registration. That second interface also ends before teardown.
No diagnostic COM owner spans renderer or retained-texture release.

[Microsoft's message-callback contract](https://devblogs.microsoft.com/directx/d3d12-debug-layer-message-callback/)
allows another callback thread and forbids D3D calls there. The probe copies
messages into fixed slots published with release/acquire atomics. Callback
state deliberately lives until process exit and owns no device reference.
Overflow, incomplete records, null/truncated descriptions or warnings and
worse fail the observation. The separate
[destruction notification](https://learn.microsoft.com/en-us/windows/win32/api/d3dcommon/nf-d3dcommon-id3ddestructionotifier-registerdestructioncallback)
supplies an object-destruction witness rather than an inference from releasing
known test owners. A ten-second watchdog covers workload and teardown; it
terminates only this probe process on expiry. No artificial queue gate or
device loss is introduced.

The Debug run exited 0 on NVIDIA GeForce RTX 5080, hardware Direct3D 12,
vendor/device `10DE:2C02`, Windows NT `10.0.26200.0`. Both interface queries
and registrations returned `S_OK`. Both application-message calls returned
`S_FALSE` (`00000001`, successful); their exact callbacks were nevertheless
observed once each. Six ordinary hidden presents across a real 64-to-32 resize
visited slots 0 and 1 four and two times. No per-frame read-back was used.

The snapshot contains 81 complete records, all INFO, with zero overflow,
truncation or warning/error/corruption messages. Callback phase tags include
36 renderer-release and 11 retained-texture-release records. The separate
device destruction callback fired once during retained-texture release.
This closes the retained-observer-owner limitation for this D3D12 observation.
Initialization before registration, complete callback quiescence/coverage
during destruction and deterministic pending work in both presented slots are
not established. Slot visits alone are not overlap evidence. Vulkan's final
device boundary is not covered by this probe.

Local receipts are `out/overnight-2026-10-26/d3d12-destruction-*`; tested HEAD
was `4ef0372dc18b6eb228206dc6847d8e51ae46121e` with only this probe and its
target added. Probe source SHA-256 was
`25D944D992780656DCDE028C30F5D37B384234B103408FE975BDD7E94C350154`.
Both Debug and Release targets compiled; Release explicitly refuses the run
and supplies no diagnostic acceptance. The target is `EXCLUDE_FROM_ALL` and
outside CTest because these optional interfaces have a newer runtime
requirement than the engine's Windows 10 floor. Default test counts stay put.
The four standalone CMake checks and website check, 21 contract tests and
production build also pass; no hosted CI run is claimed.
To repeat the bounded observation from a developer shell:

```text
cmake --build --preset x64-debug-d3d12 --target D3D12DestructionProbe --parallel 2
out\build\x64-debug-d3d12\tests\render\D3D12DestructionProbe.exe
```

The destruction and presented-slot probes capture an absolute watchdog deadline before
spawning their workers and check it before accepting completion and after joining
the workers. This prevents a delayed worker from accepting an already-expired
run through its completion predicate. The destruction observation repeated
after that change with the same 81 INFO records, both controls and the single
texture-release destruction witness; its Debug exit is 0 and Release refusal
is 1. Updated source SHA-256 is
`E62704DA4246F04EE0A21DCE968734FC4E8E7F0CA70A119B95523F38D6E86889`;
receipts use `d3d12-destruction-final-*` in the same local directory.

**D3D12 presented-slot pending-work observation, 2026-10-26.**
The optional [D3D12PresentedSlotsProbe](../../tests/render/d3d12/presented_slots_probe.cpp)
uses the existing instrumented DeviceResources translation unit and its own
bounded wait observer. It warms six ordinary presents, requires both actual
swapchain indices and drains before holding the queue on a CPU-owned fence.
Each slot's witness is queued before its draw submission. Both real
`end_frame()` calls must return with distinct submitted indices while the
gate remains held; only then is the final reuse wait observed.

A sole worker CPU-signals the gate once. Rescue expiry and observer claims
share a mutex and deadline check, so a rescue cannot masquerade as verified
pending work. A two-second rescue, an earlier backend wait or a first Present
that retains its slot produces exit 2, **unverified**, after release and drain.
These can be legitimate presentation constraints rather than engine defects.
A ten-second process watchdog bounds the workload and cleanup. Tail-fence
retirement precedes renderer/resource destruction on normal or unwinding armed
exits; fatal timeouts terminate only this process.

Both Debug and Release exit 0 on the RTX 5080 hardware backend. In both runs,
the gated frames submit/present slots `0, 1`, both witnesses remain `0`, and
the next `begin_frame()` reuses slot `0`. At the actual backend wait, the
requested timeout is `INFINITE`, alertable is `FALSE` and a zero-time event
probe returns `WAIT_TIMEOUT` (`258`). The observer requests the only CPU gate
signal; the actual five-second-bounded wait returns `WAIT_OBJECT_0` (`0`).
The later queue tail and both witnesses finish at exactly `1`, excluding
device-removal's `UINT64_MAX` as completion.

This establishes two queued, unretired submissions in distinct presented
slots at the instrumented reuse boundary on this runtime. It does not prove
simultaneous GPU execution, physical display overlap, saturation or an
unmodified infinite wait. This probe captures no Debug InfoQueue; the separate
diagnostic observations above retain their own workload boundaries. Vulkan's
corresponding observation is recorded below.

Final receipts use `d3d12-presented-slots-final-*` under
`out/overnight-2026-10-26/`; the source manifest records HEAD
`45b03ad2778f9ab2de49f660f31b4440b9c073fb` plus the local probe changes.
Probe SHA-256 is
`5D4DD5851656F84D8CBFA74E21897A2CDD0742FB26046B9919D5D1844CC637C5`.
The target is `EXCLUDE_FROM_ALL`, outside CTest and changes no default test
counts or required CI contexts. Both configurations compile, four standalone
CMake checks and the website check, 21 contract tests and build pass.
Run the bounded probe from a developer shell:

```text
cmake --build --preset x64-debug-d3d12 --target D3D12PresentedSlotsProbe --parallel 2
out\build\x64-debug-d3d12\tests\render\D3D12PresentedSlotsProbe.exe
```

**Vulkan device-destruction callback observation, 2026-10-26.**
The optional [VulkanDestructionProbe](../../tests/render/vulkan/destruction_probe.cpp)
links the ordinary engine, borrows raw instance/device identities without a
shared owner, and requires the actual Debug validation messenger. It adds one
independent instance messenger with process-lifetime, preallocated callback
storage. Two exact INFO controls bracket six ordinary hidden presents and a
real 64-to-32 resize. Renderer release precedes retained-texture release.
No read-back drain, queue gate or device loss is introduced.

Executable definitions intercept only destruction of the captured identities;
all other calls forward unchanged. Actual DLL exports are resolved before
engine creation. The matching `vkDestroyDevice` records entry and return
around the real loader call, preserving its arguments. The matching
`vkDestroyInstance` removes only the test messenger, using its original NULL
allocator, before forwarding the original call. This respects the
[device-child](https://docs.vulkan.org/refpages/latest/refpages/source/vkDestroyDevice.html)
and [instance-child](https://docs.vulkan.org/refpages/latest/refpages/source/vkDestroyInstance.html)
lifetime requirements. The callback itself makes
[no Vulkan calls](https://docs.vulkan.org/refpages/latest/refpages/source/PFN_vkDebugUtilsMessengerCallbackEXT.html),
copies descriptions and publishes each fixed record with release/acquire
atomics. A ten-second absolute watchdog with completion checks covers the
workload and cleanup; fatal expiry terminates only this probe process.

The Debug run exited 0 on NVIDIA GeForce RTX 5080, hardware Vulkan 1.4.351,
vendor/device `10DE:2C02`, with SDK `1.4.357.0`. It used the review's section-2
settings, including `validate_sync = true`, best practices and shell-visible
logging, through the task-owned `vulkan-validation/vk_layer_settings.txt`.
The extra messenger was created successfully. Its snapshot contains nine
complete records: both exact INFO controls and seven performance-only
advisories. It has zero errors, general/validation warnings, overflow or
null/truncated descriptions. The full shell log separately reports zero
validation errors, sync hazards or validation warnings and ten performance
advisories, including messages before the test messenger existed.

The captured device's real destruction call entered and returned exactly once
during retained-texture release. The instance hook entered/returned once and
removed the test messenger once before forwarding actual instance destruction. This observes
the final device boundary without retaining its engine owner. Initialization
before the test messenger, diagnostics from actual instance destruction,
complete callback quiescence and presented-slot pending work remain outside
this observation. The snapshot is not a claim about every possible message.

Local receipts use `vulkan-destruction-*` under `out/overnight-2026-10-26/`.
The tested base was `3e1820ca3a071d8caf28e2f8f8e60a013c02c57d` with this probe
and its optional target added. Probe source SHA-256 is
`2FB5E6C0D547F278967E0137927301C0C00E887BE2CD8CABA0BC14C0105B3013`.
Debug and Release compile; Release refuses execution with exit 1 and supplies
no validation acceptance. The target is `EXCLUDE_FROM_ALL`, outside CTest and
does not change the 20 default tests or required CI contexts. All four standalone
CMake checks and the website check, 21 contract tests and build pass. To repeat from
a developer shell, point `VK_LAYER_SETTINGS_PATH` at a directory containing
the section-2 settings and run:

```text
cmake --build --preset x64-debug-vulkan --target VulkanDestructionProbe --parallel 2
out\build\x64-debug-vulkan\tests\render\VulkanDestructionProbe.exe
```

**Vulkan presented-slot pending observation and REV#7 closure, 2026-10-26.**
The optional [VulkanPresentedSlotsProbe](../../tests/render/vulkan/presented_slots_probe.cpp)
uses ordinary renderer calls and no artificial hold. After warming and
draining, it tries at most four pairs with 1, 64, 256 or 1,024 full-viewport
sprites per frame in a requested 1280-by-720 hidden fixture. It borrows the
two actual Frame identities and copies their fresh timeline targets after
both `end_frame()` calls. Exact-forward `vkQueuePresentKHR` observations require
one successful call per frame on the same valid queue/swapchain; merely
advancing the frame ring without a swapchain cannot pass.

At the exact next production `vkWaitSemaphores` reuse entry, the probe requires
the actual device, one shared timeline and the expected reuse target. Its
strict oracle requires the timeline counter below both saved targets and a
real zero-time wait on their minimum returning `VK_TIMEOUT`. Retirement of
either target or a race to completion does not qualify; if none of the bounded
pairs qualifies, the process exits 2, **unverified**. Only this scoped
reuse wait gets a five-second bound; unarmed calls keep their original
arguments and timeouts. A ten-second absolute watchdog, including completion
checks, covers workload and cleanup. On normal completion the queue drains
before engine owners are released.

No future host signal, event or submit mutation generates pending work.
[Host event signaling](https://docs.vulkan.org/refpages/latest/refpages/source/vkSetEvent.html)
cannot release an event already awaited by a pending command buffer, and
[presentation's binary-semaphore dependencies](https://docs.vulkan.org/refpages/latest/refpages/source/vkQueuePresentKHR.html)
also constrain future-signal gates. Those invalid designs were not used for
this probe. The current observation changes no presentation arguments.

Debug and Release both exited 0 on the RTX 5080 hardware device, Vulkan
1.4.351. In each run the one-sprite pair stayed unverified: its counter was
already 6 (Debug) or 5 (Release), against targets 5 and 6. Both then observed
the 64-sprite pair on actual slots 0 and 1, two `VK_SUCCESS` Present calls,
targets `7, 8`, reuse target `7`, and counter `6`. The zero-time wait ran and
returned `VK_TIMEOUT` (`2`); the actual bounded reuse wait then returned
`VK_SUCCESS` (`0`). Its production timeout request was `UINT64_MAX`.
The Debug section-2 layer run reports zero validation errors, sync hazards or
validation warnings and eight performance advisories. Release supplies no
validation-layer claim.

This is a witnessed pending instant in two ordinary presented slots, with a
strict oracle. It is not deterministic pending generation, simultaneous GPU
execution, display overlap, saturation, pacing acceptance or a portable CI
timing guarantee. The executable remains outside default builds and CTest;
other runtimes may correctly return unverified. Local receipts use
`vulkan-presented-slots-*` under `out/overnight-2026-10-26/`, with tested base
`7775e5b02e29607ed28c5ce21198be84dbc91942` plus this probe and target. Source
SHA-256 is `9EE54DEF9A7AA0ED3B9C2BA65D9EFFEE9BFF49AFDC387CAF4C5C05827183BE38`.
Both targets compile; the four standalone CMake checks and website check,
21 contract tests and build pass. Default Vulkan CTest entries remain 20;
required CI contexts are unchanged and no hosted run is claimed.
To repeat, select the section-2 layer settings for Debug and run:

```text
cmake --build --preset x64-debug-vulkan --target VulkanPresentedSlotsProbe --parallel 2
out\build\x64-debug-vulkan\tests\render\VulkanPresentedSlotsProbe.exe
```

Taken together, the native presented/resize sequences, controlled pending
reuse/resize/teardown waits, separate diagnostic observations, actual final
device boundaries and the presented-slot observations close the bounded
verification opportunity. The D3D12 ordering probe still captures no InfoQueue;
its separate diagnostics keep their stated scopes. No callback-quiescence,
physical-device-removal or human acceptance is added by closing REV#7.

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
