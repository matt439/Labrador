# Do the five render backends draw the same picture?

> Read-only sweep by 350 agents, 2026-08-28, against the tree at `ca5b2e3`.
> `engine/render/` is 18,919 lines across 91 files: `d3d11/` (2,204),
> `d3d12/` (3,019), `gl/` (1,670), `vulkan/` (4,819) and `null/` (693), held
> against the seam in [renderer.h](../../../engine/render/renderer.h) and the
> shared pixel arithmetic beside it.
>
> Nothing was built and nothing was run — no `cmake`, no `ctest`, no device, no
> validation layer. Every claim here comes from reading source.
>
> **This document is the sweep as written and is not updated as findings are
> fixed.** That disclaimer is the same one [the 2026-08-19
> audit](../backend-equivalence/README.md) carried, and §6 below is about what
> went wrong with it last time.

**163 candidate divergences raised across 40 axes, 61 refuted under two lenses,
and the invariant holds. All three defects the 2026-08-19 audit found have been
fixed. What has not kept up is the seam's prose about itself: 113 comments in
`engine/render/` no longer match the code, and 48 of those are a single species
— a sentence that counts the backends while legislating a term.**

| Document | What it holds |
|---|---|
| **README.md** (this file) | Method, the verdict, the three defects re-adjudicated, the eight findings, and what the folder should become |
| [INVENTORY.md](INVENTORY.md) | The 40 axes at five backends — 29 of the audit's re-run, 11 it could not have had. The reference half |
| [DRIFT.md](DRIFT.md) | The 113 comments the code no longer matches, ranked |
| [LEDGER.md](LEDGER.md) | The 2026-08-19 audit and the 2026-08-26 survey, adjudicated item by item — 106 items |
| [GAPS.md](GAPS.md) | What this sweep missed, capped off, and could not do |

---

## 1. Why this sweep and not another

The 2026-08-26 survey asked what to build next and its nine work items all
landed. Its spine is exhausted, so repeating it would have spent a budget
rediscovering two decisions already written down. What justified a sweep instead
was a narrower and checkable unknown.

`docs/review/backend-equivalence/` **does not know that two of the five backends
exist.** Zero case-insensitive matches for `vulkan` or `d3d12` across all five of
its files, against phrases that hard-code three throughout — *"two of the
three"*, *"a file two of three folders do not have"*, *"only null has three"*. It
was written against `57b65b3`; `d3d12/` landed the next day and `vulkan/` the day
after. It read 4,336 lines of backend. There are now 18,919.

That alone would be unremarkable, because a review is historical and says so. The
sharp part was that [CLAUDE.md](../../../CLAUDE.md) called its `DRIFT.md` *"a
live list of comments in `engine/render/` that the code no longer matches"* while
`DRIFT.md:3-4` said of itself *"Not updated as findings are fixed."* Both cannot
be true, and nobody could say which of its items were live. §6 answers that, and
the answer is not the one either sentence predicts.

## 2. How it was done

| Phase | Agents | What it did |
|---|---|---|
| **Ruler** | 5 | Extracted the falsifiable contract as it stands now — `renderer.h`, the shared headers, the five `backend.h` files, the current test corpus, and every render claim in the design documents. Everything downstream was judged against that written ruler rather than against an agent's priors. |
| **Drift adjudication** | 28 + 26 | One agent per `DRIFT.md` claim, each locating the sentence *by text* (the line numbers are nine days stale), reading the code beneath it, and running `git log -S` to find the commit that moved it. Then a second agent per contested verdict, defaulting to refuting the first. |
| **Axes** | 40 | One agent per equivalence axis, each holding **all five backends at once** for that axis. The 29 axes are the audit's own list re-checked and extended; eleven more are terms only D3D12 and Vulkan introduce. |
| **Verification** | 129 | Every candidate attacked by two agents under distinct lenses — **does the code actually say this** (re-open every quoted line and read the enclosing function) and **can any caller tell** (a pixel, a recorded sprite, an exception, a lifetime, a hang, a leak). Default verdict REFUTED. Both had to fail to refute. |
| **Held-over candidates** | 26 | Two lenses each over 13 candidates the axis pass raised and never reached. |
| **Comment corpus** | 12 + 47 | Twelve partitions covering all 91 files, plus the four test files and the three CMake scripts, read comment by comment against the code beneath. Then an adversarial check on the highest-severity findings. |
| **Ledger** | 11 | The audit's three open documents and the survey's two render decisions, adjudicated item by item — 106 items. |

**The verification layer did the work again.** 61 of the 65 candidates that
reached it died, and the reasons are worth more than the survivors: the
difference was inert (a depth term with no depth buffer), out of domain (a value
no caller can construct), unreachable on real content, already pinned by a test,
or already documented as intentional with reasons the code still matches.

**One methodological note, because it explains two numbers that look
inconsistent.** On the divergence side 4 of 65 candidates survived — 6%. On the
documentation side 44 of 47 survived — 94%. That is not a lax check on the second
pass. A drifted comment is decidable by opening two files and counting; a
behavioural divergence has to be traced to somebody who could observe it, and
most cannot be. The two rates measure different kinds of claim.

## 3. The verdict

`NOTHING A BACKEND DOES DECIDES WHERE A PIXEL GOES` is true at five backends.
All five call the same `build_sprite_quad`/`build_glyph_quad` with the same
arguments, derive the pixels-to-clip constant from the same `Viewport::pixel_rect()`
integers, build the same index buffer, and specify the same premultiplied blend
terms. The one term a backend still decides is where a pane sits in the buffer,
and there are now three answers to it rather than two — Direct3D measures down
from the top, GL up from the bottom, and Vulkan hands the rasteriser a negative
viewport height. All three are declared.

**And the sweep is not the only thing saying so any more.** When the 2026-08-19
audit ran, its own [TEST-GAP.md](../backend-equivalence/TEST-GAP.md) had to
argue that *"no backend A agrees with backend B statement is expressible
in-process"* and propose a golden-image mode as the only mechanism that would
catch the whole class. That mechanism shipped. Four rasterising backends are now
held byte-for-byte to one checked-in image set, which is a stronger statement
than anything in this document, because it is executable.

## 4. The three defects, re-adjudicated

The audit found three. **All three are fixed**, and two were fixed before the two
new backends were written, which is why those backends came back clean — they
were built against a seam that already stated the rule.

### A. **FIXED** — `f22c968`, and wider than the fix the audit proposed

GL anchored every pane to a cached `Impl::height` that the shell guaranteed would
go stale during a window drag. The cache is gone. The flip now takes a live
`GetClientRect` through `drawable_size()`, read once per frame
(`gl/renderer.cpp:596`, `:814`, `:267-276`).

**Neither new backend has it, and for two different reasons.** D3D12's placement
is height-free like D3D11's — verified rather than assumed. Vulkan's flip is
*pane-local*: with a negative viewport height the origin is the pane's own bottom
edge, so the defect's shape cannot be written there at all. No backend now places
a pane against a height it does not own.

*Residual:* the sentence that fix amended,
[sprite_geometry.h:29-35](../../../engine/render/sprite_geometry.h), counts four
backends and enumerates two of the three answers.

### B. **FIXED** — `cc02678`, on the day the audit was written

D3D11 deferred an abandoned frame into the next one where GL and null discarded
it. The three-way split is now a five-way agreement: **all five drop it, by five
different mechanisms.** D3D12 closes its lists without executing them; Vulkan
resets the command pool and the descriptor pools and deliberately forgets the
tracked image layout.

The audit's worry that this was reachable from a worker exception has gone up
rather than down — the fan-out is real now (`90ce3d0`,
[fanout_tests.cpp](../../../tests/scene/fanout_tests.cpp)) and the throw site is
unchanged. But all five now handle the resulting state, and the audit's
*"Coverage: none. Both harnesses always pair begin with submit"* is false: three
cases pair a begin with no submit, and the pixel one is byte-pinned by a golden
across four rasterisers. **That case is what caught Vulkan's synchronisation bug
under the validation layer** — the test the audit asked for found the defect the
audit could not have predicted.

*Residual:* [renderer.h:367-379](../../../engine/render/renderer.h), the
`begin_frame` paragraph and the only place this rule is written down, says the
five backends have three things to drop. There are four kinds, and Vulkan is the
fourth.

### C. **FIXED** — and the rule went onto the seam, just not the seam the audit named

`add_texture_asset` before `create_device` gave three answers: GL threw by name,
D3D11 access-violated, null silently succeeded. **All five now refuse with
`std::runtime_error` naming the texture.** D3D12 refuses with one device check
covering everything else it has to be null; Vulkan's `device()` accessor is the
null-safe one. The ordering rule was written down — onto
[resource_factory.h](../../../engine/render/resource_factory.h) rather than
`renderer.h`, which is arguably the better home, and TEST-GAP's case **A3 landed
in exactly the vehicle it proposed**.

*Residual:* three of the five guard comments still count three or four backends.

**Notice the shape.** Three defects, three fixes, three residuals, and all three
residuals are the same thing.

## 5. The eight findings

These are the candidates that survived two adversarial lenses. **None of them
puts a wrong pixel on a screen**, which is the ranking the audit used and it
still applies. Every one was narrowed by its verifiers — several substantially —
and the narrow form is what is stated here.

1. **CI runs `RenderPixelTests` in the one configuration where the WARP fallback
   is compiled out.** `ci.yml:86-88` gives `x64-release` `skip_tests: ''`, while
   `d3d11/device_resources.cpp:162-191` puts the WARP fallback in the `#else` of
   an `#if defined(_DEBUG)`. Four other statements in the tree assert the premise
   under which that job's `create_device` must throw on a GPU-less runner. They
   cannot all be true. **This is the one finding that is not a question about the
   source but about the build machine, and it is settled by looking at one job
   log.**
2. **D3D12 leaks a descriptor-heap slot per texture name** out of a fixed 256 on
   any `release_device_resources()` that is not a device loss —
   `next_texture_slot` is a bump allocator reset only by device creation and
   device loss. Narrowed to an unstated precondition on public seam API rather
   than a live defect.
3. **D3D12's `handle_device_lost` can tear down with a live device still
   executing.** One of its three call sites — the ordinary `WM_EXITSIZEMOVE`
   resize path, where `try_wait_for_gpu()` answered false — reaches it with no
   GPU wait.
4. **Vulkan's `abandon_commands` discards tracked image layout on a frame that
   submitted but never presented.** The residual half of the Vulkan review's
   C4/C5, on the branch the applied fix deliberately kept.
5. **`check_engine_includes.cmake` hard-codes its module as a `(render|audio)`
   alternation**, so `engine/input/xinput/` — the third platform seam
   ARCHITECTURE names — is outside the folder wall, and CLAUDE.md repeats the
   false consequence clause in the section listing what fails the build.
6. **Two of the three terms a draw call is keyed on have never been exercised
   with a device.** No submitted frame in `pixel_tests.cpp` changes texture or
   filter mid-list; only the viewport break has ever rasterised. Both stamps are
   asserted on null.
7. **Re-loading a texture name inside an open frame is undefined and differs.**
   `resource_factory.h` states the `create_device` ordering rule and is silent on
   the `begin_frame`/`submit` interval; the named contract for re-loading a name
   is pinned only outside an open frame.
8. **D3D12's texture factory discards its `HRESULT`**, reporting
   `E_OUTOFMEMORY`, `E_INVALIDARG`, `DXGI_ERROR_DEVICE_REMOVED` and an
   unsupported format as one sentence naming only the format and the dimensions.
   T6, and the only one of the five factories whose message both asserts a cause
   and carries no code.

## 6. What the adjudication says about this folder

All 28 of `DRIFT.md`'s claims were adjudicated. **26 came back
`FIXED_AND_AMENDED`.** One is still live (D9, half of it — the command-list
lifetime headline was replaced but the D3D11 vocabulary above it was not), and
one was **wrong when the audit wrote it** (D3 conflated two propositions and the
audit's own citation refutes it).

So `CLAUDE.md`'s *"live list"* is wrong, but not in the direction anyone would
have guessed. The list is not stale in the sense of having rotted — it is
**spent**. The tree worked through it in the ordinary course of business, one
amendment at a time, and no document recorded that. Nobody was ignoring
`DRIFT.md`; everybody was fixing it and nobody was ticking it off.

That is an argument about mechanism rather than diligence, and the mechanism
already exists in this repository. `docs/survey/2026-08-26-status.md` was created
nine days ago for exactly this problem — a companion that carries the live state
beside a document that is frozen by design. The `d3d12/` and `vulkan/` reviews
solved it a third way, by having CLAUDE.md name the commits that applied them.

**The recommendation is to correct the sentence and not to invent a fourth
convention.** `CLAUDE.md:263` should say what is true: that `DRIFT.md` is
historical like everything else in `docs/review/`, that it was worked through,
and that this document is where the render folder's live drift now lives. A
review that acquires a status companion, a naming convention *and* a round two is
three ways of recording one fact, and T3 says take the simpler model.

## 7. What is not here

Named so the absence is deliberate.

- **Nothing was run.** Same limit as last time, and it bites hardest on finding
  1, which one CI log settles, and on the two Vulkan findings, which the
  validation layer with `validate_sync` would settle — the sweep
  [docs/review/vulkan/](../vulkan/) §2 describes and which is not what the layers
  check by default.
- **No red team.** The 2026-08-19 audit ran a second adversarially-framed pass
  over its 18 highest-risk axes and it earned its keep. This sweep did not get
  one, so the axes rest on a single hunting pass plus verification.
- **39 of the 113 drift findings were never adversarially checked** — all
  medium, concentrated in `gl/` (10) and the test files (10). They are
  unexamined, not cleared, and [DRIFT.md](DRIFT.md) marks each one.
- **The 85 low-severity candidates from the axis pass** were not verified either.
  Given 61 of 65 higher-ranked ones died, the prior on them is that almost all
  are inert.
- **No fixes.** Nothing in this sweep changed a comment or a line of code, for
  the reason the last one gave: a review that quietly rewrites the thing it is
  reviewing cannot be checked afterwards.
