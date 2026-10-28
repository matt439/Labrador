# Implementation and build-comment audit

SM-47's source inventory for the five targets named by
[Code hygiene](code-hygiene.md#sm-47-implementation-and-build-comment-archaeology),
reviewed on 2026-10-28. The work is comments only: current behavior, diagnostic
strings, vendor text and executable tokens remain the implementation owner's
verification boundary before closure.

## Scene culling

Target: [scene.cpp](../engine/scene/scene.cpp).
Evidence: `Scene::draw()`, `draw_views()`,
[Camera::visible_rectangle()](../engine/render/camera.cpp), the
[GameObject cull contract](../engine/core/game_object.h) and
[Philosophy's rendering rules](design/PHILOSOPHY.md#simulation-and-rendering).

Each view sets its draw-list camera and viewport, obtains its visible world
rectangle through the camera's inverse transform, then checks both ordinary
and collision objects before calling their draw methods. The camera divides
pane dimensions by scale; cull bounds receive `1 / scale` so view-pixel growth
can be expressed in world units. The overlay runs after those object draws.
This is current call-path evidence, not a new culling benchmark or device run.

Stable retirement preserves collision-object insertion order used by drawing;
`end_tick()` rebuilds borrowed sweep pointers after ownership changes. A single
view, absent thread pool or absent partitioner selects calling-thread drawing.
The game chooses whether to supply fan-out dependencies; a fixed object-count
crossover would turn workload-dependent scheduling into engine policy (T1).
Existing worker-drain and callback error-priority comments remain intact.

The comments retain the inverse-transform rationale and per-view culling rule.
They remove the deleted ViewportManager path, former zoom-arithmetic narrative
and a private client's dated paint-tile census.

## Rotated-rectangle geometry and equality

Target: [rectangle_rotated.cpp](../engine/math/rectangle_rotated.cpp).
Evidence: constructors, `calculate_points()`, `angle()`, `operator==()`,
`operator!=()` and the validity helpers in that source, together with
[Vector2F](../engine/math/vector2f.cpp) and
[scalar arithmetic](../engine/math/scalar.cpp).

Constructors derive corner points before validity checks that read those
corners. A segment direction is normalized for the axis representation. Signed
orientation comes from the x-axis's `atan2` angle. Equality compares center,
both axes and half extents exactly; the derived corner cache is not an
additional authoritative field, and inequality negates equality.

Setters and inflation validate candidate values before committing members and
the corner cache, preserving the prior object on rejection. `quad()` passes
cached corners to Quad's fixed-storage constructor, which still validates its
input; the comments no longer imply that this path avoids validation.

Edge validation computes lengths and unit directions once, preserving the
explicit zero-direction handling. Adjacent directions are tested for
perpendicularity; opposite-edge length tolerance scales with edge length.
Comments retain those numerical reasons without former missing-definition,
former-normalization or stale shape-count claims. The audit does not add a
finite-input validator or change any degeneracy acceptance.

## XInput snapshot translation

Target: [xinput_gamepad.h](../engine/input/xinput/xinput_gamepad.h).
Evidence: [the converter](../engine/input/xinput/xinput_gamepad.cpp),
[the reader](../engine/input/xinput/gamepad_reader.cpp),
[gamepad.h](../engine/input/gamepad.h) and the device-free fixture cases in
[xinput_gamepad_tests.cpp](../tests/input/xinput_gamepad_tests.cpp).

The reader requests DirectXTK `DEAD_ZONE_NONE`; choosing a deadzone remains
caller policy through `apply_deadzone` (T1). The converter reads a supplied
snapshot without polling hardware. A disconnected snapshot returns neutral
engine values regardless of stale supplied fields; a connected snapshot maps
buttons and triggers and negates each stick's y coordinate, leaving x intact.

The comments remove the former-client/double-copy narrative while retaining
the reason to avoid applying two deadzones. Fixtures already name each button,
combined button bits, y conversion, analogue triggers, disconnected neutrality
and the unfiltered reader constant. Reading those cases is source evidence,
not a fresh test run or physical-pad acceptance.

## Citation checking

Target: [check_doc_citations.cmake](../cmake/check_doc_citations.cmake).
Evidence: its source/document globs, path and basename resolution, section
matching, trade-off matching and final report branches, together with
[Conventions on comments](design/CONVENTIONS.md#comments).

The checker establishes name closure for the citation forms it recognizes;
it cannot establish whether the cited passage supports a claim. It scans the
listed C++ source trees, resolves repository-relative document paths or bare
filenames, checks numbered `## N.` headings and matches whole `T<number>` words
on lines containing a comment marker against Philosophy's trade-off headings.
Ambiguous bare filenames are reported without failing or checking their
numbered section; absent documents, sections or trade-offs fail. Textual anchors
are not validated. This is not a full C++ or Markdown parser.

Document enumeration excludes build output, vendored code, the independently
built website and ignored `.claude` checkouts. Hyphenated path continuation
across comment lines is rejoined before matching. Comments retain those limits
and the amendment/review obligation, while removing another project's dated
review statistics and fragile counts of current citations/documents.

## Shader compilation

Target: [compile_shaders.cmake](../cmake/compile_shaders.cmake).
Evidence: both compile functions, their `find_program` calls, command arguments,
generated outputs and private target source/include registrations;
[engine/CMakeLists.txt](../engine/CMakeLists.txt) selects profiles and callers.

Both paths produce headers containing byte arrays from the shared HLSL source,
with the source listed as a build dependency. Direct3D uses required `fxc`
lookup inside `compile_hlsl()`. Vulkan's `dxc` lookup searches
`VULKAN_SDK/Bin` with `NO_DEFAULT_PATH`, avoiding substitution of a PATH tool
whose SPIR-V capability is not established. A missing SDK compiler keeps its
existing actionable diagnostic.

Vulkan's b/t/s shifts map descriptor-set 0 to bindings 0/1/2; these backend
binding choices stay in the build function rather than the shared shader.
Lazy compiler discovery keeps other backends from requiring tools they do
not call. Comments retain build-time failure rationale (T5), explicit missing-
tool diagnostics (T6), generated-header shape and SDK lookup constraints.
They make no claim about every developer machine's PATH or the capabilities
of every alternative shader compiler.

## Website and qualification boundary

No direct source excerpt in `website/src/` or `website/tests/` selects these
five files. The changed backend header is excluded from the published reference;
the two implementation sources and CMake helpers do not themselves have
generated reference pages. Links or public declarations are unchanged.

**Closed 2026-10-28:** executable-token comparison passes the five audited
targets. The related `engine/CMakeLists.txt` Vulkan comment now points to
same-source SPIR-V compilation and SDK-only lookup, preserving its executable
tokens. All five standalone CMake checks and 64 permitted offline Python cases
pass: downloader fake-response cases, CI filters, synthetic Vulkan versions,
content mirroring, consumer packaging and cloud Config/Release/Analysis/Dispatch
fixtures. PowerShell worker/trace integrations remain stopped and were not
invoked; this is not a full tooling-suite result.

Website check reports zero errors or warnings, all 157 website tests pass, and
the 143-page build completes with valid internal links and existing upstream
Vite directive warnings. No shader compiler, browser, graphics device, audio endpoint or
physical input is started for this source audit. No new unresolved fact requires
a behavioral decision for these comment edits. Runtime/hardware acceptance and
existing evidence gaps remain in their current review ledgers and TODO.
