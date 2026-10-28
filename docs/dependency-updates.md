# Moving dependency and toolchain pins

Use this procedure when changing a dependency, SDK, compiler selection or
website build tool. Choose the new input deliberately, record what changed,
then qualify the resulting engine and consumer. A version label in a proposed
command is not an installation or test result.

The inventory below was read on 29 October 2026. Its linked source files own
the values; update this record if a bump makes a displayed value false.
SM-25's SDK pin, browser logging and weekly schedule are implemented, but
observation of a scheduled run remains open in
[BACKLOG](product/BACKLOG.md#next). This procedure does not close that gate
or establish that any new toolchain has passed the checks below.

## What is fixed, and what still moves

| Input | Source of truth and current selection | Meaning |
| --- | --- | --- |
| Engine packages | [vcpkg.json](../vcpkg.json): builtin-baseline `d92484ed3c5020c6679d095ad3e5add907887b62`; directxtk with xaudio2redist on Windows, doctest and lodepng | The full baseline commit fixes the registry baseline used to resolve the manifest. There are currently no top-level overrides or features. |
| Installed vcpkg | [CMakePresets.json](../CMakePresets.json): VCPKG_ROOT; CI uses VCPKG_INSTALLATION_ROOT | A toolchain location, not an exact installed vcpkg version. |
| Emscripten | [.github/workflows/ci.yml](../.github/workflows/ci.yml): EMSDK_VERSION, currently 6.0.12 | Exact SDK release shared by the browser jobs. |
| SDK bootstrap manager | [.github/actions/emscripten/action.yml](../.github/actions/emscripten/action.yml): shallow clone of emsdk's default branch, then install/activate EMSDK_VERSION | The selected SDK release is fixed; the manager's checkout is not pinned to a Git commit. |
| Vulkan SDK | ci.yml: VULKAN_SDK_VERSION, currently 1.4.357.0, and VULKAN_SDK_SHA256 | Versioned Windows installer and checked archive digest. Keep the two fields together. |
| ccache | ci.yml: CCACHE_VERSION, currently 4.14.1, and CCACHE_SHA256; installed by the local Emscripten action | Exact Windows x86_64 archive and checked digest. |
| Native build requirements | [CMakeLists.txt](../CMakeLists.txt), [engine/CMakeLists.txt](../engine/CMakeLists.txt), [cmake/settings.cmake](../cmake/settings.cmake) | CMake 3.26 minimum, C++20 and Vulkan 1.2 minimum are requirements, not installed tool versions. |
| Runner images | ci.yml and [website.yml](../.github/workflows/website.yml): windows-latest and ubuntu-latest | Moving labels. Visual Studio/MSVC, Windows SDK, CMake, Ninja, vcpkg, Python and installed Chrome may change with the image. |
| Remote Actions | Both workflows and the local Emscripten action use checkout, cache, cache/restore, upload-artifact and setup-node at @v4 where applicable | Major tags, not immutable action commit references. The local composite action itself comes from this checkout. |
| Website CI Node | website.yml: setup-node node-version 24 | A major-version selector, not an exact patch. |
| Supported website Node | [package.json](../website/package.json) and the lock's root engines: `^22.18.0 || >=24.0.0` | A compatibility range. There is no website .node-version, .nvmrc or packageManager/npm version pin. |
| Website packages | package.json and [package-lock.json](../website/package-lock.json) | Ranges express requested versions; the version-3 lock records the exact graph, resolved archives and integrity values used by npm ci. Neither pins Node/npm itself. |
| Production Node/deploy tool | [website/README.md](../website/README.md#hosting-and-deployment): recorded dashboard NODE_VERSION=24 and npx wrangler deploy | Owner/provider settings and a deployment-tool selection. Wrangler is not a dependency in the website's manifest/lock. This file does not establish the dashboard's current state or pin the fetched tool. |
| Worker compatibility | [wrangler.jsonc](../website/wrangler.jsonc): compatibility_date 2026-10-08 | Worker compatibility configuration, distinct from the build Node version. |

The website's [Emscripten reader](../website/src/lib/emscripten.ts) reads
EMSDK_VERSION from ci.yml; its browser guide therefore follows that source.
Keep current hand-written installation guidance, including AGENTS.md, true
when the SDK changes. The browser runner records the browser product/version;
that evidence does not pin Chrome. SDK Node and runner Node are different
inputs: Emscripten's emulator/page commands use the SDK's Node, while the
website CI selects its own Node and native lint uses the runner's.

## Plan and record a bump

1. Name the old input, chosen new input and reason. Review the chosen release's
   official notes, compatibility requirements and checksums, and retain those
   sources with the result. Do not choose a version merely because a moving
   tag resolved to it. A digest computed only from an untrusted download does
   not independently establish the intended release's bytes.
2. Identify coupled source and consumer changes below. Separate unrelated
   bumps when that makes a failure attributable. Keep required job names,
   matrices, permissions and failure reporting stable; this procedure does
   not require a new CI job or a branch-protection change.
3. Prepare and review the source change, then perform the shared qualification
   checklist. Record unavailable runtime or provider evidence explicitly.
   A source procedure, a cached build and a real device check establish
   different facts. Preserve existing local work and shared caches.

Fetching, installation, SDK activation, real devices, CI dispatch and deployment
each need the applicable access and authorization. A listed verification step
does not grant it or reopen a stopped action. Use the existing command owners
rather than a different route around an access failure.

## vcpkg and the manifest a game receives

Select the full baseline commit and review its affected port versions, selected
features, platform conditions and any overrides. Change vcpkg.json rather than
substituting the runner's vcpkg checkout revision for the manifest baseline.

vcpkg reads the top-level project's manifest. The public
[ConsumerManifest component](../website/src/components/ConsumerManifest.astro)
and [consumer assembler's derived_manifest](../tools/consumer_check/assemble.py)
both copy the engine's dependencies and builtin-baseline, excluding lodepng
because only engine tests use it. They keep doctest for the copied game's
tests and preserve dependency objects such as directxtk's Windows condition
and xaudio2redist feature. Keep both derivation rules in agreement.

A baseline move therefore changes what a newly generated adopter manifest
pins. An existing game's vcpkg.json is not rewritten by moving its engine
submodule: compare and update that top-level manifest too, as
[Move your engine pin](../website/src/content/docs/docs/guides/move-pin.mdx)
describes. Verify the real copied consumer, not just the engine build.

Neither current derivation copies top-level overrides or features. If a future
manifest needs them, define what the game must carry, update both producers
and their checks in the same change, and inspect the generated manifest.
Do not assume every new vcpkg field propagates automatically.

For a dependency, feature, override or baseline change, add the migration to
[CHANGES.md](../CHANGES.md) under its dependency rule and update
[ReleaseNotices.astro](../website/src/components/ReleaseNotices.astro) in the
same commit. Review the installed DLLs, licences and
[release file list](../website/src/walkthrough/release-files.txt). A change a
copied minimal project would want also goes in its
[migration list](../samples/minimal/README.md#a-copy-is-yours).
Other API, compile-flag and silent supported-runtime changes follow CHANGES.md's
own definition; a tool selector change alone does not automatically meet it.

The CI binary-cache keys include the manifest hash. A changed exact key and
possible restore from an older cache do not prove a fresh dependency build.
Use an isolated clean build/install tree and document cache use; do not delete
another project's cache or overwrite an existing configure tree merely to
force a qualification run.

## SDK and cache-tool releases

**Emscripten:** change EMSDK_VERSION in ci.yml and follow the local composite
action's existing install/activate sequence when installation is authorized.
Check the imported guide value and hand-written references. The SDK cache key
contains the selected SDK version; ccache's compiler check is
`string:emsdk-<version>`. Verify both clean and warm-cache browser builds, and
record the actual compiler/SDK Node versions and produced bundle identity.

**Vulkan:** change VULKAN_SDK_VERSION and its independently checked official
VULKAN_SDK_SHA256 together. Review the constructed versioned Windows-X64 URL,
download and installer result, header/library discovery and shader compilation.
The workflow uses [ci_download.py](../tools/ci_download.py); digest checking and
installation remain outside its bounded retry. Authentication, transport and
filesystem denials are terminal. Do not remove checksum/failure checks to make
a new download pass.

The Vulkan 1.2 find_package minimum is not the SDK release number. Shader
compilation requires the SDK's own Bin/dxc with SPIR-V support,
[deliberately excluding PATH](../cmake/compile_shaders.cmake). Windows SDK's
identically named dxc is not a replacement. Direct3D uses fxc from the Windows
SDK. Record which tools were actually used; a successful header-version fixture
does not qualify a compiler, loader or driver.

**ccache:** change CCACHE_VERSION and the official archive digest together.
Preserve the Windows x86_64 archive layout, extracted executable path and
EM_COMPILER_WRAPPER contract. Compare clean and warm builds under the selected
SDK. Archive version, compiled-object cache keys and the SDK compiler-check
string serve different purposes; changing one does not establish the others'
compatibility.

## Runner, Actions, Node and npm selections

Review a runner-label or action-version change against the actual tools and
behaviour it supplies. Record the runner image and compiler/SDK/CMake/Ninja,
vcpkg, Python and browser versions in qualification. Preserve the existing
required checks. The weekly drift run can reveal movement in unchanged labels;
it is not evidence that a deliberate bump has already passed.

For Node, review website.yml's selector, package.json and lock-root engines,
README guidance and the separately recorded production setting together.
Source changes do not update the Cloudflare dashboard. Its current setting
and any provider procedure need owner/provider qualification; SM-30 remains
separate. Do not change deployment, preview or compatibility settings as an
incidental Node update.

For npm dependencies, select the intended direct versions/ranges, use the chosen
supported Node/npm to update them and regenerate package-lock.json, then review
both files. Inspect transitive changes, engine/peer requirements, resolved
archives, integrity fields and changed output. Do not hand-edit the transitive
graph. Validate the checked-in result with `npm ci`, then `npm run check`,
`npm test` and `npm run build` from website/. `npm ci` consumes the chosen lock;
it is not the step that selects updated dependency versions.

Do not rewrite existing entries in
[published-versions.json](../website/published-versions.json) to match the new
toolchain. Its append-only release snapshots carry immutable source/asset
identity, not a mutable dependency selection. A new snapshot follows the
[versioning policy](website-versioning.md).

## Qualification for every real bump

Retain the full CI matrix and the additional local device checks below. A
documentation-only implementation of this procedure needs source/lint/website
checks, not a pretend dependency bump or an unauthorized runtime sweep.

- Run all seven [standalone checks](../AGENTS.md#build-checks), applicable
  offline tooling regressions and current required CI contexts. Record the
  actual commands and any stopped cases; do not report a permitted subset as
  a full suite.
- Configure, build and test all five native Debug render selections:
  x64-debug, x64-debug-d3d12, x64-debug-gl, x64-debug-vulkan and x64-debug-null.
  Retain x64-release and the Release/consumer checks CI runs. Include further
  Release/device qualification demanded by the affected owner.
- Run the rasterizing golden/pixel set on applicable backends with devices.
  CI's GL/Vulkan skips do not establish those results, and native null records
  draws rather than pixels. Compare existing goldens; do not regenerate them
  to hide a failure. Intentional changed PNGs need visual review and exact
  per-commit declarations under
  [Golden image review records](../CONTRIBUTING.md#golden-image-review-records).
  Apply Vulkan's [synchronization procedure](review/vulkan/README.md) where
  required by the render verification owner.
- Build/test wasm-debug and wasm-release GL/Web Audio, plus CI's explicit
  wasm-release-null override build. Cover SDK Node tests and the registered
  pixel/no-S3TC, context-loss, window and sample pages, the packaged LineSweeper
  bundle and Release pacing-protocol cases. Preserve the registration and
  device requirements in [AGENTS.md](../AGENTS.md#verification-boundaries).
- Run the website's clean install, check, tests and build. Inspect affected
  reference pages, source excerpts, notices, generated consumer manifest and
  static assets. Release metadata/live-index checks need their own applicable
  network access and release scope; a site build does not replace them.
- Follow [consumer_check/README.md](../tools/consumer_check/README.md) for the
  copied native project's exact two-test inventory/run, Release install/file
  list/start-up and browser consumer build/Node/page checks. Engine tests alone
  cannot establish consumer-relative paths, dependencies or distribution files.

[Verification](verification.md) owns the render planner and its optional
executor. Default planning establishes no result. Use `--execute` only when
its entire mixed runtime is authorized and available; do not invoke it merely
because this procedure links it. Its remaining real-sweep qualification stays
explicit.

Record source revision, old/new inputs, provenance, actual tool/runner/browser
and device identities, cache conditions, commands and results. Keep real pads,
listening, phones, Safari, physical pacing, clean-machine Sandbox and production
provider/deploy checks distinct. Do not claim hardware or release acceptance
from compile-only/headless/emulated results. Unavailable checks remain named
gates for the owner; the implemented source work and its local checks can still
be reviewed and committed independently.
