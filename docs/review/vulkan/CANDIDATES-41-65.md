# Vulkan candidate adjudication: 41-65

Source adjudication on 2026-10-26 of ordinals **41-65** in the frozen review's
[65 never-verified candidates](all-findings.md#never-verified--ranked-below-the-budget).
These ordinals preserve the original bullet order, not severity or new issue
IDs. Each original bullet is copied verbatim, including its historical path
and line number. The frozen review and its evidence remain unchanged.

This is the independently reviewed continuation of [CANDIDATES.md](CANDIDATES.md).
Current function and
file names below identify the inspected source; historical line numbers in
the originals are not current navigation instructions.

## Scope and limits

The initial pass read source, build declarations, test declarations and
documentation. Dated follow-ups separately record the actual offline tests,
device runs and injected-failure checks performed while resolving candidates.
Source reading alone supplies no runtime or hosted CI acceptance.
"Already resolved" describes the current implementation or wording, without
attributing it to an unverified fix commit. "Confirmed" is limited to the
stated source condition; it is not a reproduced device failure or measured
performance effect. Existing successful rendering and synchronization checks
do not establish unexecuted error paths or platform behavior.

## Adjudicated candidates

### Candidate 41

Original, copied verbatim:

- **[minor]** `tests/scene/scene_tests.cpp:324` — Half of one sentence in tests/scene/scene_tests.cpp was updated to five and the other half left at four, four lines from a third count that is also stale

**Fixed coverage comment.** The drawing paragraph in
[scene_tests.cpp](../../../tests/scene/scene_tests.cpp) names the existing
`fanout_tests.cpp`, `cull_tests.cpp` and `order_tests.cpp` coverage and their
null-only registration in
[tests/scene/CMakeLists.txt](../../../tests/scene/CMakeLists.txt), without stale
backend counts or a claim that those tests are still owed.

Follow-up on 2026-10-26: native null `SceneTests` passes all 32 cases and 265
assertions. Non-comment source lines are unchanged. This is headless recording
coverage, not a graphics-device run or new coverage on every backend.

### Candidate 42

Original, copied verbatim:

- **[minor]** `engine/render/sprite_vertex.h:10` — sprite_vertex.h's enumeration of how backends bind the vertex fields is false: there are four backends with a vertex buffer and a third mechanism

**Corrected in the current comments.**
[sprite_vertex.h](../../../engine/render/sprite_vertex.h) now accounts for
the two Direct3D backends, GL and Vulkan, with semantic, name and location
bindings, and distinguishes the C++ member offsets from the separate HLSL
shader-location ABI. In
[`Renderer::Impl::create_device_dependent_resources`](../../../engine/render/vulkan/renderer.cpp),
each attribute has an explicit location (0, 1 or 2) and an offset computed with
`offsetof(SpriteVertex, position/colour/texcoord)`, so changing the C++ member
order does not silently renumber the locations. The declaration-order
requirement documented in [sprite.hlsl](../../../engine/render/sprite.hlsl)
concerns its `VertexIn` shader struct. No struct layout, shader input, profile
or binding changed, and no reordering experiment was performed here.

### Candidate 43

Original, copied verbatim:

- **[minor]** `.github/workflows/ci.yml:6` — ci.yml's opening claim that everything it builds comes from the repository or vcpkg.json was contradicted by this commit's own step and not amended with it

**Fixed declared-input comment.** The opening of
[.github/workflows/ci.yml](../../../.github/workflows/ci.yml) distinguishes
checkout sources and manifest package dependencies from runner build tools
and the declared Vulkan SDK, Emscripten and ccache installations. The current
install steps support that description; their present download availability
was not checked.

Follow-up on 2026-10-26: all non-comment workflow lines are unchanged,
including the job/check names, matrix, commands and exact-name exclusions.
The two offline CTest-filter regression cases pass. No fetch or hosted CI run
was performed for this comment correction.

### Candidate 44

Original, copied verbatim:

- **[minor]** `engine/CMakeLists.txt:186` — engine/CMakeLists.txt:186 says the Vulkan backend compiles "the same source as the three backends above" when only two of the three compile it

**Corrected in the current comments.**
The Vulkan branch in [engine/CMakeLists.txt](../../../engine/CMakeLists.txt)
now says the source is shared with "the two backends above that compile it",
explicitly noting that GL compiles none of it. That correctly identifies
D3D11 and D3D12. The next phrase now says "second compiler": both Direct3D
branches use `fxc` and Vulkan uses `dxc`. Candidate 61 is the same correction;
no compiler, profile or generated shader bytes changed.

### Candidate 45

Original, copied verbatim:

- **[minor]** `.github/workflows/ci.yml:90` — RenderPixelTests is excluded from the GL and Vulkan legs by an unanchored ctest regex, which will silently take any future test whose name contains it

**Fixed against future overmatching; no current wrong exclusion established.**
The Windows GL and Vulkan matrix entries in
[.github/workflows/ci.yml](../../../.github/workflows/ci.yml) now use
`-E "^(RenderPixelTests|MinimalSmokeTests)$"`, preserving the two exact
exclusions and all job/check names.

Follow-up offline verification on 2026-10-26:
[test_ci_filters.py](../../../tools/tests/test_ci_filters.py) extracts both
declared filters and asks actual CTest for JSON selection from a
`LANGUAGES NONE` fixture. The original filters wrongly remove seven fixture
names containing the excluded spellings; the anchored filters retain them
and exclude only the two exact names. Both unittest cases pass. Fixture
commands would fail if executed; `--show-only` selects without running them.
This establishes selection behavior, not a present native-test omission,
native execution or a hosted CI result.

### Candidate 46

Original, copied verbatim:

- **[minor]** `engine/CMakeLists.txt:183` — find_package(Vulkan REQUIRED) asks for no version, so the backend's deliberate Vulkan 1.2 floor is not enforced at configure time

**Configure-time floor enforced and tested with the real finder.**
[engine/CMakeLists.txt](../../../engine/CMakeLists.txt) now calls
`find_package(Vulkan 1.2 REQUIRED)`. In
[device_resources.cpp](../../../engine/render/vulkan/device_resources.cpp),
`MIN_API_VERSION` is `VK_API_VERSION_1_2`; `create_instance` requests it and
`select_physical_device` rejects older API versions. The configure requirement
provides an earlier diagnostic for insufficient headers; it does not replace
runtime device admission or the separate SDK `dxc` check.

[test_vulkan_sdk_version.py](../../../tools/tests/test_vulkan_sdk_version.py)
extracts the production request and invokes CMake's real `FindVulkan` with
`LANGUAGES NONE`, synthetic header version metadata and an empty import
library. Before the fix, the 1.1.130 rejection assertion failed because
configure accepted it. All four cases now pass: 1.1.130 rejects with a minimum
diagnostic, 1.2.0 and 1.4.357 accept with correct imported paths, and an
unversioned control still accepts the same old metadata. The installed real
1.4.357 SDK also configures successfully. No compiler, loader, device or SDK
installer acceptance follows from these metadata fixtures.

### Candidate 47

Original, copied verbatim:

- **[minor]** `.github/workflows/ci.yml:129` — The only network fetch outside the cached vcpkg step is uncached and unretried, so a LunarG outage reds the workflow on a commit that changed nothing

**Transfer resilience remains limited; the exclusivity claim is obsolete.**
The Vulkan SDK step in
[.github/workflows/ci.yml](../../../.github/workflows/ci.yml) now downloads a
pinned version and checks its SHA-256, but its `Invoke-WebRequest` has no
explicit retry and no installer cache surrounds it. A service or network
failure can therefore fail the step; no outage was induced or observed by
this pass. Emscripten/ccache and other workflow inputs also use the network,
so this is no longer the only non-vcpkg fetch. Bounded download retries are a
useful follow-up; pinning and digest checks should remain, and installation
failures should still stop the job rather than silently continue.

### Candidate 48

Original, copied verbatim:

- **[should-fix]** `tests/render/golden_image.h:14` — golden_image.h — the file that states the golden set's purpose — still says three rasterising backends, three configurations and two shader profiles

**Already resolved.** [golden_image.h](../../../tests/render/golden_image.h)
now names four rasterising backends, HLSL at three profiles plus its GLSL
transliteration, and four runs/configurations sharing one golden set. The
historical counts are absent. This is a documentation check, not a new
cross-backend golden-image comparison.

### Candidate 49

Original, copied verbatim:

- **[minor]** `tests/render/null_tests.cpp:439` — null_tests.cpp's abandoned-frame comment now reads 'Two of the four ... and the third', which does not add up, and the split it describes is wrong

**Already resolved.** The abandoned-frame case in
[null_tests.cpp](../../../tests/render/null_tests.cpp) now says three of five
backends (GL, null and Vulkan) keep recordings in vectors and the remaining
two record command lists. The inconsistent "two ... third" count and old
division are gone. The test body remains an executable declaration of that
contract; it was not run by this adjudication.

### Candidate 50

Original, copied verbatim:

- **[minor]** `.github/workflows/ci.yml:64` — ci.yml says none of the eleven other test executables would start without the Vulkan loader; two of them link no engine

**Fixed loader-requirement wording.** The Vulkan installation comment in
[.github/workflows/ci.yml](../../../.github/workflows/ci.yml) now requires
the loader for executables whose linked objects import Vulkan entry points,
without an exception roster. Linking the static `LabradorEngine` target alone
does not establish those imports. The build declares `STATIC`, links
`Vulkan::Vulkan`, and the backend calls Vulkan entry points directly.

The configured SDK is 1.4.357.0, with its `Lib/vulkan-1.lib` and shader compiler
present locally. This is source/configuration evidence; no loader-absent
executable launch or SDK installation was tested. Candidate 59 is the same
historical claim and shares this correction.

### Candidate 51

Original, copied verbatim:

- **[minor]** `engine/render/vulkan/device_resources.h:180` — The frames-in-flight ring reaches index 1 for the first time and never wraps, and nothing in engine/render/vulkan/ discloses it where D3D12's equivalent gap was accepted only because it was disclosed

**Disclosure corrected; actual overlap remains an execution question.** The
`FRAME_COUNT` comment in
[device_resources.h](../../../engine/render/vulkan/device_resources.h)
explicitly names the native no-read-back presentation sequences and warns that
their shape does not prove a busy GPU at each boundary. `DeviceResources::present` in
[device_resources.cpp](../../../engine/render/vulkan/device_resources.cpp)
advances `frame_index_` modulo `FRAME_COUNT`; the repeated loop in
[LineSweeperFrameBench](../../../bench/linesweeper_frame_bench.cpp) calls
`end_frame`. The source permits repeated wrapping and discloses the limit.
[presentation_lifecycle_tests.cpp](../../../tests/render/presentation_lifecycle_tests.cpp)
now wraps the ring without per-frame read-back, resizes and destroys the
renderer without a final explicit wait. The Vulkan full pixel suite passes
under synchronization validation, but that still does not establish concurrent
execution of both slots or close REV#7's deterministic outstanding-work proof.

### Candidate 52

Original, copied verbatim:

- **[should-fix]** `engine/render/vulkan/texture_factory.cpp:110` — vulkan/texture_factory.cpp is the longest of the five, not the second longest, and renderer.h's measured list says it is shorter than D3D12 while carrying a stale D3D12 figure

**Already resolved.** The current Vulkan
[texture_factory.cpp](../../../engine/render/vulkan/texture_factory.cpp)
calls itself the longest, and [SEAM.md](../../../engine/render/SEAM.md),
section 4, states the same ordering without keeping a table of measured line
counts. The obsolete public-header measured list is absent. This is a source
and rationale comparison; code length alone establishes no performance or
ownership correctness result.

### Candidate 53

Original, copied verbatim:

- **[should-fix]** `engine/render/vulkan/device_resources.h:340` — vulkan/device_resources.h ships four accessors with no caller and one member written but never read, which renderer.h says a backend written to this seam does not do

**Removed four unused accessors; the unread-member half was already resolved.**
[DeviceResources](../../../engine/render/vulkan/device_resources.h) no longer
exposes `recording()`, `queue()`, `colour_format()` or `frame_index()` after a
current caller check across engine, tests, samples, benchmarks, tools and
website sources, including the Vulkan auxiliary tests. The internal state
members, frame reset behavior and live D3D12 accessors are unchanged.
Candidates 27 and 40 cover the same removed `recording()` accessor. The
formerly write-only `swapchain_format_` retains its real read in
[`DeviceResources::present`](../../../engine/render/vulkan/device_resources.cpp):
the same-size image-copy path also requires the swapchain and colour formats
to agree, otherwise it blits. Deleting that format member would remove a live
condition, so it remains. This internal source cleanup establishes no new
runtime or performance result.

### Candidate 54

Original, copied verbatim:

- **[should-fix]** `engine/render/renderer.h:437` — renderer.h's back_buffer_size contract enumerates only the pre-Vulkan backends, and both mechanisms it names predict the wrong answer for the fifth

**Corrected in the current contract rationale.**
[`Renderer::back_buffer_size`](../../../engine/render/renderer.h) now explains
that Vulkan reports its engine-owned colour target's extent and presentation
scales that target when the swapchain size differs. The
Vulkan implementation in
[renderer.cpp](../../../engine/render/vulkan/renderer.cpp) explicitly returns
`device_resources.colour_extent()`, the engine-owned colour target, rather
than the independently sized swapchain. That agrees with the header's first
rule, "the size of the buffer". No size query, signature or generic caller
contract changed, and no new resize execution is claimed.

### Candidate 55

Original, copied verbatim:

- **[should-fix]** `engine/render/d3d12/renderer.cpp:266` — d3d12/renderer.cpp:266 and d3d12/backend.h:34 still claim engine-owned synchronisation is unique to D3D12, which the commit's own "transliterated line for line" falsifies

**Resolved in the setup rationale and remaining header bullet.**
[`DrawList::View::begin`](../../../engine/render/d3d12/renderer.cpp) now
explicitly says Vulkan asks the same command-pool reuse question as D3D12's
command allocator. The introduction to
[d3d12/backend.h](../../../engine/render/d3d12/backend.h) also explains that
the fifth backend answers this synchronization obligation. Its following
bullet now says the engine owns synchronization here as it does in Vulkan,
matching that introduction and Vulkan's timeline/frame implementation. This
was the same stale uniqueness claim also corrected in the architecture tree
under candidate 58, not evidence that either synchronization implementation
failed.

### Candidate 56

Original, copied verbatim:

- **[minor]** `engine/render/vulkan/renderer.cpp:963` — submit() heap-allocates a std::vector every drawing frame to hold a running total a local could carry, against PHILOSOPHY's no-allocation-per-frame rule

**Measured frame-path allocation removed; no timing gain claimed.**
At baseline `e0c4a14`, Vulkan `Renderer::submit` constructed a fresh
`std::vector<size_t>` on every nonempty drawing frame solely to retain view
vertex prefixes. This violated PHILOSOPHY's per-frame allocation rule.
[renderer.cpp](../../../engine/render/vulkan/renderer.cpp) now carries the
prefix in a scalar, advancing once by each view's entire uploaded vertex
count before walking its runs, including skipped geometry and empty views.
Candidate 16's pass-unwind guard is unchanged.

Follow-up on 2026-10-26:
[vulkan_submit_allocation_tests.cpp](../../../tests/render/vulkan_submit_allocation_tests.cpp)
calibrates a scoped MSVC Debug DLL CRT allocation hook, warms four presented
frames with identical capacities, and measures only three subsequent
`submit()` calls. Each baseline call reports two normal-block allocation
attempts totaling 40 requested bytes; each corrected call reports zero.
Calibration, logging, assertions, read-back and presentation stay outside the
measured scope, and the previous hook is restored. This observes allocation
attempts on that submitting thread in the Debug CRT, not every driver/system
heap, release-build allocator or frame path, and it measures no speedup.

The analytic pixel case remains compiled outside Debug CRT builds. It uses
distinct uploaded vertices in an entirely skipped first view, an empty middle
view, and two differently filtered visible runs, then compares every RGBA
byte. It passes before and after the correction without new golden images.
All five native render checks pass, including full pixel suites on all four
rasterizers. Vulkan passes 163 headless render cases and 53 pixel cases with
shell-visible synchronization validation: zero errors/hazards and 738
performance advisory lines. The injected-failure tests for candidate 16 also
pass. No driver-allocation, timing or physical-platform acceptance follows.

### Candidate 57

Original, copied verbatim:

- **[minor]** `engine/render/renderer.h:461` — renderer.h's read_back_buffer paragraph names two of the three backends that swizzle, while vulkan/device_resources.h says the swizzle is now "the same sentence in three files"

**Already resolved.** The `read_back_buffer` contract in
[renderer.h](../../../engine/render/renderer.h) now promises RGBA regardless
of backend storage and leaves conversion costs to backend documentation. It
does not enumerate a stale number of swizzling backends. The Vulkan
implementation in [renderer.cpp](../../../engine/render/vulkan/renderer.cpp)
still converts its BGRA read-back to RGBA; no new conversion defect is
established by the historical enumeration complaint.

### Candidate 58

Original, copied verbatim:

- **[should-fix]** `docs/design/ARCHITECTURE.md:112` — ARCHITECTURE's tree block was not amended by this commit: it omits render/vulkan/, miscounts the presets, and says fxc compiles the shader for two backends

**The listed tree defects are resolved; the exclusivity claim is corrected.**
The tree in [ARCHITECTURE.md](../../design/ARCHITECTURE.md) includes
`render/vulkan/`, twelve configurations including the browser pair, and the
two shader compilers. Its D3D12 entry now describes engine-owned
synchronisation as also present in Vulkan. The D3D12 backend's fence waits
and the Vulkan backend's timeline waits support that description.

Only the tree's two explanatory lines changed. This is no new synchronization
runtime result; candidate 55's separate D3D12 backend-header wording remains
outside this correction.

### Candidate 59

Original, copied verbatim:

- **[minor]** `.github/workflows/ci.yml:65` — "none of the eleven other test executables in this preset would start" is false: MattMathTests and LineSweeperTests link no engine and import no Vulkan loader

**Fixed with candidate 50; duplicate historical claim.** The Vulkan
installation comment in
[.github/workflows/ci.yml](../../../.github/workflows/ci.yml) limits the loader
requirement to executables whose linked objects import Vulkan entry points.
It no longer treats linking a static engine archive as sufficient evidence
and has no fragile exception roster. Candidate 50 records the source and
configured-SDK evidence; no loader-absent launch was performed.

### Candidate 60

Original, copied verbatim:

- **[minor]** `engine/render/vulkan/renderer.cpp:658` — begin_frame resets every view twice, because abandon_recording() and open_frame() both call reset_views()

**Confirmed duplicated work; benefit of removal unmeasured.** In
[renderer.cpp](../../../engine/render/vulkan/renderer.cpp),
`Renderer::begin_frame` calls `Impl::abandon_recording`, which calls
`reset_views()`, then calls `Impl::open_frame`, which calls `reset_views()`
again. `reset_views` clears each view's vertices, runs and pending run state
and installs its full-buffer viewport. `abandon_recording` has no other
current caller; `open_frame` is also used to restart an active resized frame.
A bounded cleanup can retain one reset on the normal path while preserving
the resize restart and command-abandon ordering. No CPU saving, changed
complexity or visible rendering effect has been measured in this pass.

### Candidate 61

Original, copied verbatim:

- **[minor]** `engine/CMakeLists.txt:186` — engine/CMakeLists.txt's Vulkan block says sprite.hlsl is "the same source as the three backends above, through a third compiler"; two of the three share it and dxc is the second compiler

**Corrected in the current comments; duplicate of candidate 44.** The Vulkan branch in
[engine/CMakeLists.txt](../../../engine/CMakeLists.txt) now says "the same
source as the two backends above that compile it", correctly excluding GL.
The next line calls `dxc` a "second compiler". The two Direct3D branches
use `fxc` and Vulkan uses `dxc`, with separate profiles; GL's embedded GLSL
does not turn `dxc` into a third compiler of `sprite.hlsl`. No compiler or
profile changed and this correction claims no new shader build.

### Candidate 62

Original, copied verbatim:

- **[minor]** `engine/render/vulkan/device_resources.cpp:1545` — present() binds the same Frame reference twice under two names, so the code reads as if the frame in flight changed when it cannot have

**Confirmed redundant aliases on the surviving presentation path.**
[`DeviceResources::present`](../../../engine/render/vulkan/device_resources.cpp)
binds `Frame& frame = this->frame()` for the first acquire and later
`Frame& current = this->frame()` for the submission's acquire semaphore.
An out-of-date rebuild that loses the device returns before reaching the
second binding, and a successful swapchain-only rebuild does not advance
`frame_index_`. Both references therefore denote the same slot on paths
reaching the later submission. Using a single clearly scoped frame reference
or reading the first semaphore directly is a bounded clarity cleanup. No
misbound semaphore or runtime failure was established by the duplicate name.

### Candidate 63

Original, copied verbatim:

- **[minor]** `NOTICE:103` — NOTICE's new "Toolchains, not code" section says nothing in it is linked into a build product, then lists import libraries and says the build links one

**Fixed factual distinction; licence and redistribution statements unchanged.**
[NOTICE](../../../NOTICE)'s section opening now names separately installed
tools, headers and import libraries, distinguishing running tools from linking
the platform import libraries named below. This agrees with the Vulkan
branch's `Vulkan::Vulkan` link and the Direct3D branches' platform libraries in
[engine/CMakeLists.txt](../../../engine/CMakeLists.txt).

Only the opening factual paragraph changed. The existing licence and
redistribution statements below it remain unchanged; this correction draws
no new licensing conclusion and changes no agreement or dependency.

### Candidate 64

Original, copied verbatim:

- **[minor]** `README.md:53` — README's "four of the five exist to prove the seam carries a backend" sweeps in the D3D11 backend the seam was cut around, contradicting its own list two lines above

**Already resolved.** [README.md](../../../README.md) no longer says "four of
the five exist to prove the seam carries a backend". Its build overview names
the five implementations and directs the reader to ARCHITECTURE for their
roles. The historical misclassification of D3D11 is absent; no new caller or
runtime defect is raised by the removed sentence.

### Candidate 65

Original, copied verbatim:

- **[minor]** `docs/design/PHILOSOPHY.md:359` — PHILOSOPHY says the resize contract held "not by luck"; android.md and the commit message, amended in the same commit, record the same fact as luck

**Corrected in the current design wording; no resize defect established.**
[PHILOSOPHY.md](../../design/PHILOSOPHY.md) now states the causal explanation:
the resize contract holds because Vulkan draws into an engine-owned image
and handles swapchain changes within presentation.
[android.md](../../port/android.md), section 5, records that the same design
was chosen for another reason and this result fell out of it, calling that
"luck rather than foresight". Both descriptions give the same current
mechanism without a conflicting claim of design foresight; the dated port
account is preserved. No resize behavior changed, and no Android resize,
surface rotation or real-device recovery acceptance is supplied here.
