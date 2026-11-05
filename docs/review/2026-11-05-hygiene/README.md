# Code hygiene review — 2026-11-05

Reviewed revision: `5cbc6b3`, master, clean tree.

A read-only review of every tracked first-party file for maintainability:
dead code, duplication, lost errors, ownership, comments that are false or
historical, conventions, magic numbers and weak tests, plus any defect found
on the way. The findings describe that revision and are not rewritten as
fixes land. [STATUS.md](STATUS.md) is the live ledger.
[FINDINGS.md](FINDINGS.md) lists every kept finding with its fix;
[findings.json](findings.json) adds the verbatim evidence and the skeptic's
reasoning for each.

## Assessment

The tree is in good shape and is not decaying. No finding survived at high
severity, and nothing found loses data or crashes normal play. Five in six
findings are low severity. The cost is spread thinly rather than concentrated:
625 findings over about 150,000 lines.

The largest category is comment truth. Censuses that have gone stale ("both
samples", "41 of 43 .dds", golden-frame and ctest counts), history kept in
comments, and pointers to rationale that SM-45 to SM-47 moved or removed
survive across the engine, the backends, the tests and the build files. The
citation lint proves names resolve, not that the prose is true, and these
comments are what it cannot see. The second category is duplication: test
scaffolding, three browser runners forked from each other, six git wrappers in
the Python tools, website release helpers, and small pieces copied into all
five render backends beyond what SEAM.md decides to duplicate.

The behaviour defects are few and bounded, and worth taking first:

- Audio backends disagree with each other and with their seam: XAudio2 drops
  an out-of-range `play_wave` that null and Web Audio throw on, accepts a
  streaming bank and fails later without a name, and queues one-shots during
  suspension that Web Audio drops; null `play_voice` ignores the transitions
  it claims to transcribe; `audio_resources.h` claims replaced voices are
  released. `audio_device.h` describes suspension it does not perform.
- XInput does not report suspended slots as disconnected, as
  `gamepad_reader.h` promises.
- Win32 `window_proc` lets game and renderer exceptions escape into user32,
  against `window.h`'s promise that they come out of `pump_until_quit`.
- JSON parsing is recursive, so deep nesting overflows the stack instead of
  throwing; an animation strip's `frame_count` is unbounded.
- Format switches in D3D11, D3D12 and GL map an unknown format silently to
  RGBA or DXT5. D3D12's `block_until` discards the wait result; a D3D12
  resize commits its new size before a rebuild that can throw.
- GL leaks shader objects and its bootstrap context on failure paths and
  fetches `wglCreateContextAttribsARB` without the sentinel filter.
- `ThreadPool` ignores a failed `SetThreadpoolThreadMinimum`;
  `read_file_bytes` reports every open failure as a missing file; broad-phase
  cell arithmetic is undefined for non-finite boxes.
- LineSweeper's `main.cpp` says a 120 Hz tick rate would run at half speed;
  it would run at double speed.
- Tooling: `master_watch.py`'s rerun branch can never run;
  `flash_check.mjs`'s verdict uses the count its recorded decision rejects;
  the website NVDA step can hide a failed `npm ci`; master-watch's timeout is
  shorter than the jobs it waits for; `run_safari.mjs` leaks a driver that
  never becomes ready.
- Tests that assert less than they claim: the shape edge-order test skips
  slots, intersection fixtures sit off the boundary they name, a LineSweeper
  view subcase asserts nothing, `bench/main.cpp` silently drops the
  complexity assertion of a renamed case, and the XInput vibration cases
  rumble a pad that is plugged in.

## Method

Round one split the tree into 22 units, from each engine module and render
backend to the samples, test folders, tools, build and CI files and the
website. Each unit was read in full twice: once for structure and robustness,
once against [CONVENTIONS](../../design/CONVENTIONS.md) and for comment truth.
Four sweeps looked across units: duplication, unused public API (searching
ColourWars before calling engine API dead), comment claims that can be
checked, and inconsistency between sibling backends. A completeness critic
then named what round one missed: `bench/`, Python outside `tools/`,
`website/explorations/`, repository configuration, readers of untrusted bytes,
exception safety, flakiness, live documents, and five units whose structure
pass came back thin. Round two covered each of those.

Every batch of findings went to an independent skeptic told to refute it:
re-open the cited lines, search the whole repository for dead-code claims,
check convention claims against CONVENTIONS, check duplication against
SEAM.md and the backend headers, and drop anything [code hygiene](../../code-hygiene.md),
TODO.md or a live review ledger already tracks. Of 845 reported
findings, 41 were refuted and 1 was already tracked;
283 were kept with their severity corrected down. Duplicates
reported by more than one reviewer were merged, leaving 625:
105 medium and 520 low.

**Limits.** Nothing was built, run or launched; every finding is a reading of
source, and one skeptic checked each. A finding is a well-supported question
until the change that fixes it proves it, as [the index](../INDEX.md) says of
every review. File length was out of scope: the [re-audit of the same
day](../../code-hygiene.md#file-length-re-audit-2026-11-05) owns it.

## Themes

Each theme is a candidate story a maintainer can close in one bounded change
or a short series, following [code hygiene's rules](../../code-hygiene.md#rules-for-the-work):
behaviour separate from mechanical movement, tests in the same commit as a
behaviour change, and a CHANGES.md entry for anything consumer-visible. The
order is a recommendation: behaviour and test-strength fixes first, then the
API trim, then mechanical deduplication and renames, and the comment batches
last, so that moves do not invalidate corrected comments. The owner places
any of them in BACKLOG.md.

| Theme | Name | Findings | Medium | Size |
|---|---|---:|---:|---|
| [H1](#h1-engine-behaviour-and-contract-fixes-outside-render) | Engine behaviour and contract fixes outside render | 27 | 10 | L |
| [H2](#h2-untrusted-input-and-exception-safety) | Untrusted input and exception safety | 11 | 3 | M |
| [H3](#h3-render-backend-error-handling-and-resource-ownership) | Render backend error handling and resource ownership | 17 | 3 | M |
| [H4](#h4-build-ci-and-remaining-tools) | Build, CI and remaining tools | 40 | 3 | M |
| [H5](#h5-python-tooling-hygiene) | Python tooling hygiene | 25 | 2 | M |
| [H6](#h6-browser-test-runner-consolidation) | Browser test runner consolidation | 20 | 1 | M |
| [H7](#h7-test-flakiness-and-runner-robustness) | Test flakiness and runner robustness | 7 | 2 | S |
| [H8](#h8-core-math-scene-app-audio-and-input-test-hygiene) | Core, math, scene, app, audio and input test hygiene | 70 | 11 | L |
| [H9](#h9-native-render-test-hygiene) | Native render test hygiene | 26 | 2 | M |
| [H10](#h10-bench-harness-hygiene) | Bench harness hygiene | 11 | 2 | S |
| [H11](#h11-unused-public-engine-api-remove-or-test) | Unused public engine API: remove or test | 17 | 7 | M |
| [H12](#h12-linesweeper-sample-and-its-tests) | LineSweeper sample and its tests | 49 | 10 | M |
| [H13](#h13-small-samples-and-components-gallery) | Small samples and components gallery | 53 | 5 | M |
| [H14](#h14-engine-mechanical-cleanup-outside-render-naming-duplication-includes) | Engine mechanical cleanup outside render: naming, duplication, includes | 40 | 1 | M |
| [H15](#h15-render-backend-mechanical-dedupe-and-convention) | Render backend mechanical dedupe and convention | 42 | 1 | M |
| [H16](#h16-website-source-and-scripts) | Website source and scripts | 27 | 4 | M |
| [H17](#h17-repository-configuration) | Repository configuration | 3 | 0 | XS |
| [H18](#h18-orphaned-website-exploration) | Orphaned website exploration | 13 | 0 | S |
| [H19](#h19-live-document-drift) | Live document drift | 11 | 6 | S |
| [H20](#h20-engine-comment-truth-outside-render-math-core-ui-collision-assets-input-audio-app) | Engine comment truth outside render (math, core, ui, collision, assets, input, audio, app) | 55 | 13 | M |
| [H21](#h21-render-comment-truth-and-archaeology-shared-seam-and-five-backends) | Render comment truth and archaeology (shared seam and five backends) | 61 | 19 | M |

### H1. Engine behaviour and contract fixes outside render

Real behaviour defects and unenforced contracts in non-render engine code:
- ThreadPool ignores the result of SetThreadpoolThreadMinimum.
- add_child dereferences null.
- refresh_style is const yet writes.
- ByteReader accepts temporaries.
- read_file_bytes misreports open failures and omits documented exceptions.
- Broad-phase cell arithmetic is undefined for nonfinite boxes.
- JSON numbers narrow to float silently.
- Sound-bank keys are ignored silently.
- XAudio2 handles streaming banks and out-of-range play_wave differently from
null and Web Audio.
- XInput does not report suspended slots as disconnected.
- Window guards are uneven, and window_ is dereferenced without a check.
- Viewport out-of-range returns fullscreen.
- Colour 0/0 gives NaN.
- Partitioner, sprite_world_bounds and getBoundingClientRect do avoidable
per-frame work.

**Proposed story:** Several small behaviour commits, each with a behavioural
test and a CHANGES.md entry where the change is consumer-visible. Keep them
separate from any mechanical moves. Start with cross-backend agreement (audio
play_wave and streaming banks, gamepad suspend), then input validation
(ThreadPool, broad phase, JSON, Colour, sound-bank keys), then the guard and
const-correctness fixes, and finally the measured per-frame allocation and trig
work.

### H2. Untrusted input and exception safety

Readers of external bytes and code that crosses C callbacks. JSON parses
recursively, so deep nesting overflows the stack instead of throwing; an
animation strip's frame_count is unbounded; read_file_bytes narrows the file
size unchecked. Win32 window_proc lets game and renderer exceptions escape into
user32 against window.h's promise, and a throwing notify handler during
construction leaves an HWND pointing at freed state.

**Proposed story:** Behaviour commits with negative tests: a nesting limit or
iterative parse for JSON, a bound on authored counts named with the file,
checked size narrowing, and an exception barrier in window_proc that hands the
failure to pump_until_quit as window.h states. Each consumer-visible refusal
gets a CHANGES entry.

### H3. Render backend error handling and resource ownership

Format switches whose default case silently maps unknown formats to RGBA or
DXT5 in D3D11, D3D12 and GL. Other items:
- block_until discards the wait result.
- D3D12 holds a raw fence HANDLE.
- Vulkan writes failed-create outputs into owned members, ignores
vkResetCommandPool failures and has no RAII for its buffers.
- GL fetches wglCreateContextAttribsARB without the sentinel filter, leaks the
bootstrap context, shader and program objects, and gives TextureOwner implicit
copy operations.
- A SetPixelFormat failure reuses another message.
- D3D11 add_texture_asset breaks the seam's naming contract.
- D3D HRESULT failures carry no operation context.

**Proposed story:** Fix this per backend in behaviour commits: exhaustive
format switches that throw on unknown values, RAII for the D3D12 fence event,
GL objects and Vulkan buffers, propagated wait and reset results, and an
operation-named ThrowIfFailed overload. Verify across the five native Debug
presets plus wasm-debug GL, and re-run the Vulkan validate_sync procedure for
the Vulkan changes.

### H4. Build, CI and remaining tools

Problems in CMake, workflows and non-Python tools:
- CMake: probe hashes are computed at configure time, input backends are not
validated, CMAKE_CXX_EXTENSIONS does not travel with labrador_settings,
content-copy and page-test commands repeat, shader helpers duplicate, and
PHILOSOPHY.md is re-read per token.
- Workflows: the NVDA step can hide npm failures and is unpinned, setup steps
are copied into five jobs, timeouts are missing or too short (master-watch),
and comments are stale or historical, including the WIC claims, the null preset
labels and the whitespace scope.
- Tool scripts: flash_check's verdict contradicts its recorded decision, PNG
reads are unchecked, check_live crashes, PowerShell paths and GDI disposal are
wrong, and pacing literals and the geometry test are duplicated.

**Proposed story:** Make the CI and workflow correctness fixes first (NVDA
step, timeouts, a composite setup action). Any job rename needs the owner's
branch-protection update. Then fix the CMake build behaviour (probe hashes,
input backend refusal, extensions on the target, with a CHANGES entry for
settings.cmake). Then fix the tool bugs (flash_check verdict, PNG size,
check_live, hold_key). The comment corrections, including the WIC to lodepng
ones, go last. Run the standalone lint checks and the Node flash, pacing and
bundle suites.

### H5. Python tooling hygiene

There are six git subprocess wrappers with different hardening, and a commit
walk copied between checkers. Two JUnit parsers and preset executors apply
different acceptance rules, and the wasm registration lists diverge from
ci_gap_report and ctest_inventory. Bugs: master_watch has an unreachable rerun
branch, analyze returns the wrong exit status, and DPI scans miscount. Other
problems: private imports across modules, positional parallel tuples, repeated
literals and nvidia-smi queries, unused exports and imports, test loader
boilerplate copied 14 times, and incident history in docstrings.

**Proposed story:** Add a single tools/_git.py and a single JUnit/preset-sweep
helper. Derive verify_changes' wasm entries from ctest_inventory.json. Fix
master_watch, the analyze exit code and the DPI scan as behaviour commits with
tests. Then do the tools/tests loader consolidation and docstring cleanup. Run
the offline Python suites.

### H6. Browser test runner consolidation

run_safari.mjs forks run_firefox.mjs's server, file list, bundle translation,
sample hook and frame wait, and also forks website/acceptance's WebDriver
client, now missing its port-race retry. Other duplication: five MIME tables,
quit keys kept in both CMake and SAMPLES, deadline and listener boilerplate,
two stderr tails, and the module-hook copies. There are small bugs too: usage
validation happens after launch, teardown waits on an exit that has already
happened, and DevTools.send leaks its waiter. WebDriver key tables use
invisible private-use literals.

**Proposed story:** Extract shared modules for serving, MIME types, the sample
hook and frame wait, bundle steps, and the WebDriver client, used by the
Chromium, Firefox and Safari runners and by website acceptance. Have CMake and
SAMPLES read one quit-key source. Fix the three runner bugs separately. Verify
with BrowserProtocolTests, wasm-debug page tests and the CI safari job.

### H7. Test flakiness and runner robustness

Fixed sleeps used as synchronisation, polls without deadlines, timeouts that
differ for the same operation across runners, start_driver leaking a
safaridriver or geckodriver that never becomes ready, and temporary folders
left behind.

**Proposed story:** Replace sleeps with condition waits under one deadline
helper shared with the runner consolidation, kill the driver on every failure
path, and clean temporary folders. Verify with BrowserProtocolTests and the
page tests.

### H8. Core, math, scene, app, audio and input test hygiene

Comments in these suites cite ColourWars files and counts that are not in this
repository, give stale configuration censuses, or tell history. Some fixtures
sit off the boundaries their names claim, and the shape-edge test skips slots.
Rng, CollisionObject stubs and null-renderer harnesses are duplicated, and
three tests hand-roll a file reader. Other items:
- Cases sit outside their TEST_SUITE.
- Namespaces are PascalCase, public members carry trailing underscores, and
include groups are wrong.
- Temporary folders are not cleaned up.
- The XAudio2 recovery assertion tests the fake's own field.
- Test files are laid out inconsistently.

**Proposed story:** First, fix weak tests (shape edge order, boundary fixtures,
the tunnelling constants, the recovery assertion) as test-behaviour changes.
Second, consolidate fixtures into shared test-support headers (engine Random,
byte_reader, null harness). Third, rewrite the comments. If case names change,
update ctest_inventory.json; keep doctest case names otherwise.

### H9. Native render test hygiene

Problems in tests/render (excluding the web runners):
- Stale comments from before the SM-49 split, plus history in seam and viewport
comments.
- Probe Watchdog, fail_fast and loader_function scaffolding copied across D3D12
and Vulkan.
- Hand-built 2x2 textures and a copied 4097-sprite loop.
- The harness assumes 64x64 frames, and ink_bounds' comment disagrees with its
code.
- Golden-directory errors are discarded.
- DeviceFixture has a teardown-order problem.
- Include order, namespaces and pi literals do not follow conventions.

**Proposed story:** Make one shared test-support header for probe scaffolding
and flat textures, preserving test names and registration. Make the harness
follow the reported buffer size. Fix the fixture teardown order and the
golden-directory error. Then rewrite the stale and archaeology comments.
Confirm with RenderPixelTests on D3D11, D3D12 and null; no golden image should
change.

### H10. Bench harness hygiene

bench/: the render-cull row no longer mirrors Scene::draw_views, main.cpp
silently skips the complexity assertion of any phase with fewer than two
results (so a renamed case drops its check), the D3D12 descriptor probes rename
tokens with macros over included engine sources, and comments carry stale
history.

**Proposed story:** Make a missing phase fail except the two fan-out names in
non-null builds; mirror the real cull or rename the row; give the frame bench a
seam instead of macro-renamed includes; then the comments. Run the null and
D3D11 bench entries.

### H11. Unused public engine API: remove or test

Public declarations with no caller and no test, or used only by tests. These
include Vector2F clamp/lerp/unit_vector/angle spellings,
RectangleF::set_position_from_top_right, the unused Ericson functions, and
Camera's three extra calculate_view_rectangle overloads. ResolutionManager's
string and Vector2F setters silently coerce to 720p. The list continues with
Colour::brighten/clamp_colours, sprite_quad_bounds, SpriteFrame's Vector2F
constructor, the name form of set_sprite_sheet, UiContainer::children,
UiTexture::set_height, FocusGroup::refresh_style, Quad::triangle_0/1, the D3D11
texture(name) overload, and untested scalar contracts.

**Proposed story:** A single consumer-visible API-trim commit. For each symbol,
decide between deletion and making it private or internal, and add behavioural
tests for anything kept, such as the scalar clamp/lerp contracts. The commit
needs a CHANGES.md entry naming the old and new spellings. Update
website/published.ts and any quoting pages, and run website gate plus all
presets.

### H12. LineSweeper sample and its tests

ParticleField hand-rolls SplitMix32 instead of using labrador::Random. Hint
strings and asset names are copied into four or more files, including the
capture tool. Other duplication: the pacing world copies the bench, the banner
and spark-size geometry is computed twice, arrange_screen is copied, and
row_full is written twice. The tick-rate comment states the wrong direction
(#300). There are stale 'fourth assert', 'only translation unit', 'two samples'
and particle-unit comments, plus history in comments. The tests duplicate null
setup and well fixtures and contain weak assertions, along with magic counts,
missing includes and redundant CMake include paths.

**Proposed story:** Read the sample README first. One behaviour-neutral commit
adopts labrador::Random. Its replay determinism must be proven byte-identical,
or the change is treated as a behaviour change. A second commit brings asset
and hint constants and shared geometry into one owner each, used by the states,
the pacing fixture and the capture tool, with regenerated captures reviewed if
pixels move. Then strengthen the tests, rewrite the comments and README, and
tidy CMake.

### H13. Small samples and components gallery

Problems in minimal, collision, audio, local_multiplayer and components:
- Four differently-behaved --smoke-test parsers.
- Every browser main re-declares the onError bridge.
- local_multiplayer has an unreachable input mode.
- The minimal template re-measures its labels every frame, against Label's
contract.
- Collision and audio pages lack touch-action: none.
- The collision sample reads only pad slot 0.
- Deadzones, route ticks and start positions are magic numbers.
- Bindings and font names are duplicated.
- verify_content.py relies on assert.
- The components layout and camera are duplicated, and the slider tail is
repeated.
- A Python generator and an asset README disagree.
- Some comments are stale.

**Proposed story:** Template-facing fixes go first: label measurement, the pad
policy, touch-action and the onError bridge exported once by the engine. Each
gets a samples/minimal README change-list line. Then unify the smoke-test
argument parsing, remove the dead input mode, name the constants and fix the
comments. Run each sample's PageTests and smoke tests.

### H14. Engine mechanical cleanup outside render: naming, duplication, includes

Behaviour-neutral tidying in math, core, ui, collision, assets, input, audio
and app. The naming problems are camelCase Ericson locals, uppercase operator
parameters, AABB_intersects, off-allowlist abbreviations such as horiz/vert,
num_ and min_num_threads, lowercase nested types, and asset_details breaking
the one-namespace rule. The duplication covers the orthonormal check written
four times, the Quad validation, the GamepadButton bit encoding, the second
UTF-8 encoder, Registry/NameTable bookkeeping, the modifier-side tracking,
on_resuming and content_root. Missing and unused includes, widget.cpp's
using-directive and pragma regions, Partitioner's folder, and commented-out
Ericson code complete the list.

**Proposed story:** Mechanical commits only. First, private and implementation
renames and dedupe that are not consumer-visible. Second, public renames
(AABB_intersects, find_hypotenuse, parameter names) with a CHANGES entry and
the sample migration. Third, moving Partitioner out of collision as a pure move
with its CMake source lists updated. Run include and website checks.

### H15. Render backend mechanical dedupe and convention

Code copied across the five backends: the set_view_count/view validation, the
quad index buffer and format_name. Within single backends: the GL
clear-and-reset block and shader/program logs, D3D12's adapter loops, upload
descriptions and View::reset fields, and Vulkan's wait_for_gpu pair, double
destructor wait and present-mode default. Also covered: unused GL entry points
and constants, the unused D3D11 feature level and DirectXMath include,
needlessly public D3D11 methods, D3D11's per-sprite texture size re-query,
magic root and attribute indices and sentinels, D3d12Texture casing, the
snake_case aliases and the `color` parameter spelling, ResolutionManager's
preset table, and inconsistent _DEBUG/NDEBUG gating and size_t spellings.

**Proposed story:** Hoist the API-free pieces (view-count validation with a
history-free message, quad indices, format_name) into shared render code under
SEAM.md's rules, without adding virtual dispatch (T8). Then do per-backend
private dedupe and named constants. Public alias and parameter renames need a
CHANGES entry. Check every native Debug preset plus wasm-debug.

### H16. Website source and scripts

Duplication in the website:
- Git environment sanitising is copied into three scripts.
- Stable-tag, commit and semver logic is written several times.
- There are three tree walkers.
- Starter ZIP and docs-asset filename rules live in two places.
- The repository URL is defined five times.
- The MDX handlers and the fence-language list are duplicated.
- Test helpers and acceptance utilities are copied.
Other problems: the releases API call has no retry, resource_factory.h copy is
hard-coded in the page generator, and the header parser fills unused fields.
The facade and WebDriver have unused exports, a stale remark-checkout-links
reference and an inconsistent breakpoint and dedent.

**Proposed story:** Add a shared scripts/lib for git environment, release tags,
tree walking, retry and error description, used by the release and snapshot
scripts and acceptance. Have single owners for the starter and repository
constants. Move per-header copy out of the generator into published.ts data.
Remove dead exports and fields. Fix the missing-header link to fail as guides
do. Run npm run gate.

### H17. Repository configuration

Configuration files that disagree with each other or the checks: .editorconfig,
.gitattributes and .gitignore against check_whitespace.cmake, CMakePresets
repetition inheritance could remove, and templates referencing checks that no
longer exist.

**Proposed story:** One configuration commit; CMakePresets changes must keep
every preset name, since CI and documents name them.

### H18. Orphaned website exploration

website/explorations/labradors/ was added in one commit and nothing outside it
references, builds or tests it; its scripts and data have drifted from the site
they studied.

**Proposed story:** The owner decides: delete it (the commit remains the
record), or move it to a dated record with a README saying it is not
maintained. Either way nothing else changes.

### H19. Live document drift

Live documents whose checkable claims no longer match the tree: README and
docs/verification.md browser ctest counts, SEAM.md's golden-frame count and its
statement that only Direct3D rasterises on CI, ARCHITECTURE's module table and
tree diagram, and sample README claims.

**Proposed story:** One documentation commit correcting each claim against
ctest_inventory.json, ci.yml and the tree, preferring statements that do not
decay over counts. Run the website gate where a page quotes the changed text.

### H20. Engine comment truth outside render (math, core, ui, collision, assets, input, audio, app)

False or stale claims and archaeology in non-render engine comments. Examples:
comments that point to rationale SM-45/SM-46 removed, gamepad.h quotations that
no longer exist, a broad-phase contract naming bounds(), 'seven' SAT axes,
backend censuses, 'EIGHT FORWARDERS' over thirteen, and polling 'once per
frame'. Some are in public headers that the website publishes word for word.

**Proposed story:** A comments-only SM-47-style batch over
engine/{math,core,ui,collision,assets,input,audio,app}. Check each claim
against the code and the cited passage. Rewrite false contracts to describe
current behaviour; where a contract needs a behaviour decision (for example
#126's polling cadence), file it in the behaviour theme instead. Remove history
and keep the one owner of duplicated rationale. Change no tokens. Run citation,
whitespace and website checks, and review the published header pages.

### H21. Render comment truth and archaeology (shared seam and five backends)

Render-side comments that tell history or carry false censuses. Examples: 'both
samples', the '41 of 43 .dds' and line-count tallies, 'CI skips
RenderPixelTests' for Vulkan and GL, a texture-format rationale written before
PNG support, add_texture_asset's 'only engine callers', frame_open() which no
longer exists, sprite.hlsl's self-history, and D3D12 and Vulkan fix narratives.

**Proposed story:** A comments-only batch over engine/render (an SM-46/SM-47
continuation). Replace censuses and line counts with statements that do not
decay, or delete them. Correct the CI and seam claims against ci.yml and
SEAM.md. Remove history and the commented-out function. Keep the D3D12/Vulkan
synchronisation contracts intact and re-read them against the Vulkan review.
Run all five native Debug builds to confirm no tokens changed.
