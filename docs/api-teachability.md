# Game-facing API teachability audit

SRL-17 is an interim source review of T10's teaching claim. It does not replace
the Introduction to C++ series named in
[PHILOSOPHY](design/PHILOSOPHY.md#the-public-face), and no beginner session or
recording is represented. The companion introduction remains unfinished as
[Get Started](../website/src/content/docs/docs/get-started/index.mdx#companion-introduction-status)
states. Passing a build cannot establish that somebody can learn this API.

## Repeatable method

1. Record the date, source revision and reviewer. Take the header inventory
   from [published.ts](../website/src/lib/published.ts): every header marked
   `game code`, including public declarations and protected extension hooks.
   Exclude private enclosing types, `detail` internals and backend folders.
   Record the excluded renderer-writer inventory separately.
2. Read every included header's declarations and caller comments. Give each
   header one ledger row with its caller-required C++ features, engine terms,
   argument-count screen and result. Distinguish using a template from
   implementing its internals, and the normal game path from optional asset,
   device or concurrency authoring. Read real callers for concerns that the
   declaration alone cannot settle.
3. For argument triage, count the formal parameters of every source-declared
   public/protected constructor, ordinary method and namespace free function.
   Defaults count. A callback counts as one outer argument; review its own
   arity and captured loans separately. Do not count an out-of-line definition
   or inherited member twice. Exclude copy/move construction, destructors and
   operators from this numeric signal, while still reviewing the language
   knowledge they require. Mark parameter packs as variadic, never a guessed
   fixed count.
4. Record the largest fixed formal count per header, and list every
   declaration with five or more parameters, including its required count
   when defaults reduce the ordinary call. Five is a repeatable review
   trigger, not an empirical limit on learning. Review repeated scalar types,
   units, ownership and callback lifetimes even below that trigger.
5. Check whether a caller can explain the choices and obligations from public
   contracts or their linked guides: coordinates, units, ownership, invalidation,
   ordering, deferred work and concurrent drawing. Ordinary C++ needing a
   worked gloss is distinct from a missing contract. Record each failed
   criterion as a concrete finding, with a filed follow-up or a reasoned
   T-number decision. Do not infer an API redesign from an argument count.
6. Have a second reviewer check the inventory, high-arity declarations,
   ownership-sensitive paths and finding dispositions. Run citation and
   website checks, inspect the published link, and record those results
   separately from the teaching verdict. A later run records its new revision
   and ledger rather than treating these results as current forever.

This method uses manual source review. The website reader supplies declaration
text and links; it does not classify C++ callables or prove the argument counts
or semantic judgments below. No new C++ parser or metrics framework is required.

## Recorded run

Agent source reviewers read the engine at
`a01726f52c1ca8e6e2880a05dbef52cd9a2bbcf4` on 29 October 2026 local time
(28 October UTC). The later SRL-15 commit changes documentation only.
Three disjoint reviews cover foundations (27 headers), drawing/collision
(32) and input/audio/UI/assets/app (25); the root reviewer reconciles them.

| Module | Included game-code headers |
| --- | ---: |
| math | 16 |
| core | 11 |
| collision | 9 |
| render | 22 |
| scene | 1 |
| input | 7 |
| audio | 5 |
| ui | 4 |
| assets | 6 |
| app | 3 |
| **Total** | **84** |

The nine renderer-writer headers excluded from the ledger are
`sprite_geometry.h`, `sprite_vertex.h`, `texture_format.h`,
`texture_data.h`, `dds_file.h`, `block_decode.h`, `sprite_font_file.h`,
`resource_factory.h` and `throw_if_failed.h`, all under `engine/render/`.
The `white_texture_name` crossover in `resource_factory.h` is an existing
game-use constant, not a callable counted here. Explicitly unpublished headers
and backend folders are outside this audience inventory. This is not an audit
of every declaration in the repository.

The ledger's maximum is the largest fixed formal count under the rule above,
not the number of arguments required for every ordinary call. `0` also covers
data-only headers. A gloss result identifies ordinary language/engine
vocabulary that an explanation should teach; it is not observed user success.
`pass`/`Clear` mean no unmitigated source-contract failure was found in that
row; `gloss`/`Gloss`/`G` add a teaching-vocabulary need. `F1`, `F2` and `F-S1`
identify the three failed criteria and their filed dispositions below.

## Header ledger and parameter screens

### Math and core

| Header | Maximum outer parameters | C++ a caller encounters | Terms, ownership and disposition |
|---|---:|---|---|
| [math/vector2f.h](../engine/math/vector2f.h) | 3 | Value constructors, references, const/static methods, overloads/operators, explicit conversion, `constexpr`, `Point2F` alias | **gloss:** vectors, dot/cross, radians, normalization and interpolation need elementary geometry. Zero-vector differences, y-down signs and constant initialization are explained; no borrowed ownership. |
| [math/vector2i.h](../engine/math/vector2i.h) | 2 | Value constructors, explicit conversion, references, operators and static value | **pass:** x/y values and named offset/scale/set; conversion is visible. The constant's initialization rationale points to Vector2F. |
| [math/rectanglef.h](../engine/math/rectanglef.h) | 4 | Derived value, `override`, overloads, const references, `std::array`, `std::span` | **gloss:** axis-aligned box, bounding box and non-owning span. The span is consumed by `bounding_box_of`, which returns a value; empty/nonfinite behavior is stated. No stored span or ambiguous resource ownership. |
| [math/rectanglei.h](../engine/math/rectanglei.h) | 4 | Value constructors/overloads, explicit conversion, references, operators | **pass:** x/y/width/height or position/size constructors; no lifetime-carrying return. |
| [math/matrix3x2f.h](../engine/math/matrix3x2f.h) | 6 | Value constructor, named static factories, const methods, operators | **gloss:** affine/row-vector composition, determinant/inverse and point-versus-direction distinction are taught in the header, with a worked composition. Raw six-element constructor meets the screen; translation/rotation/scaling factories are the shorter ordinary route (T4), so no failure filed. |
| [math/scalar.h](../engine/math/scalar.h) | 3 | Free overloads, const references, default argument, `constexpr` values | **pass:** radians, absolute epsilon and unclamped lerp have explicit semantics. Callers must choose a tolerance appropriate to coordinate range; the reason is stated. |
| [math/shape.h](../engine/math/shape.h) | 1 | Abstract base, virtual destructor, pure virtual hooks, pointer/reference overloads | **gloss:** polymorphism and AABB vocabulary; inflation geometry/containment/degeneracy are explained. No ownership transfer is implied by query parameters. Concrete shapes remain values; no clone/downcast requirement is imposed. |
| [math/shape_type.h](../engine/math/shape_type.h) | 0 | Scoped enum | **pass:** six named alternatives; no callable or lifetime requirement. |
| [math/circle.h](../engine/math/circle.h) | 3 | Derived value, `override`, accessor methods, copy/assignment, references | **pass:** center/radius constructors; private center naming is explained. Returns geometry by value. |
| [math/triangle.h](../engine/math/triangle.h) | 6 | Derived values, `override`, const reference accessors, fixed array, overloads | **gloss:** three vertices/edges and right-axis-aligned specialization. Both six-float constructors meet the screen, but the three-Vector2F forms already group each point, avoiding six scalar positions at ordinary call sites (T4). References are to the owning triangle's points; standard member-reference lifetime. |
| [math/quad.h](../engine/math/quad.h) | 4 | Derived value, explicit conversion, const reference accessors, arrays | **gloss:** clockwise point order and value conversions; point references borrow the quad's own inline storage. No heap ownership or per-view mutation API is required. |
| [math/rectangle_rotated.h](../engine/math/rectangle_rotated.h) | 4 | Derived value, const array reference, overloads/accessors, exceptions | **gloss:** OBB, orthonormal axes and half-extents. Header explains why axes change together, refusal semantics and cached points. `points()` explicitly borrows the rectangle's own storage; modifying the rectangle changes that geometry. |
| [math/segment.h](../engine/math/segment.h) | 4 | Value constructors, operators, const methods | **pass:** endpoints can be grouped as Point2F values or supplied as four coordinates; methods return values. |
| [math/intersects.h](../engine/math/intersects.h) | 4 | Free functions/overloads, const inputs, nonconst output references | **gloss:** pairwise predicates and output references (`point`, `t`, `p`). No allocator or ownership transfer; lower-level contact details are geometry knowledge, not template machinery. Named shape inputs keep the largest signature at four. |
| [math/inflate.h](../engine/math/inflate.h) | 0 | No in-scope callable; declaration is in `namespace detail` | **pass / excluded detail:** `detail::inflate_convex_polygon` is explicitly the implementation shared by concrete shapes, not caller vocabulary. Callers use Shape/concrete `inflate`; its raw pointer/count are not included in the public-call arity metric. |
| [math/ericson_math.h](../engine/math/ericson_math.h) | 6 | Free overloads, references/output references, `std::span` | **gloss:** AABB/OBB, convexity, winding, closest point and proper crossing. High arity consists of geometric vertices plus explicit outputs; boundary/degenerate behavior is largely documented. These are optional low-level book-derived routines; `intersects.h` already supplies the ordinary named-shape predicates (T1/T4), so this screen alone is not a failed beginner path. |
| [core/state.h](../engine/core/state.h) | 1 | Abstract base/override, const, pointer, protected accessor | **gloss:** state stack, suspension, activation and parallel draw. Header states context assignment before init, draw purity and who owns begin/submit/end frame. StateContext owns the state; `context()` is a borrow, not an allocation to delete. |
| [core/state_context.h](../engine/core/state_context.h) | 2 | `unique_ptr`/move ownership, templates with explicit result type, `std::function`, lambda capture, exceptions, deleted copy | **gloss:** owning stack with deferred operations, typed push/pop and result callbacks. Example shows the typed push/callback. Context entry, destruction/resume/callback order and services-before-state teardown are explicit; no hidden requirement to implement `std::any` machinery at the call site. Callback inputs are 0 or 1. |
| [core/game_object.h](../engine/core/game_object.h) | 1 | Abstract base, virtual destructor, const overrides, reference argument | **gloss:** world-space bounds, culling, world-units-per-pixel and concurrent views. Borrowed DrawList lifetime ends at the next Renderer submit, and update-versus-read-only draw is explicit. |
| [core/handle.h](../engine/core/handle.h) | 1 | Simple class template, default/explicit constructor, equality friends | **gloss:** phantom type is explained as protection against mixing resources. Handle/index validity and producing-table identity are explicit; no pointer arithmetic or metaprogramming is needed to use the typed handle. |
| [core/registry.h](../engine/core/registry.h) | 2 | Class template/default type, `unique_ptr`/move storage, returned raw pointer, exceptions; optional storage-trait extension | **gloss:** name resolve once versus indexed get per frame, stable slots across reload and changing resource pointers are explained. `kind` literal must outlive registry. `RegistryStorage`'s `decltype`/`declval` is implementation/optional customization, not knowledge needed for normal `Registry<MyResource>` calls; custom specialization is an advanced path. |
| [core/name_table.h](../engine/core/name_table.h) | 2 | Class template, values/moves, typed handle, const reference return, exceptions | **gloss:** filled once then const, stable element references and resolve/get are explained. `kind` literal outlives table; an element reference still borrows that table. No allocator policy or template metaprogramming is required. |
| [core/thread_pool.h](../engine/core/thread_pool.h) | 2 | Noncopyable owner, `std::function<void()>`, lambda capture, exceptions | **gloss:** one submitting thread, task completion wait and exception propagation are explicit. A captured borrow must live until task completion; caller can use the documented wait before borrowed state ends. Serial wasm implementation does not promise overlapping work. No platform callback types leak to the caller. |
| [core/byte_reader.h](../engine/core/byte_reader.h) | 2 | Borrowed const vector reference, value vector return, integer types, exceptions | **pass:** constructor explicitly requires bytes to outlive reader; diagnostics copy the source name. Checked little-endian cursor/read/skip/require semantics are named. It avoids requiring packed layouts/reinterpret casts at call sites. |
| [core/file_path.h](../engine/core/file_path.h) | 1 | `std::string_view`, `std::string`, `std::filesystem::path` conversions | **gloss:** non-owning view is a call input, not stored; UTF-8/API-boundary and embedded-NUL refusal are explicit. No ownership transfer. |
| [core/step_position.h](../engine/core/step_position.h) | 3 | Public aggregate, integer/float values, inline free function | **pass:** step/fraction, fixed-step rendering, interpolation/extrapolation choice and invalid clock inputs are explained. Returns a self-contained value. |
| [core/random.h](../engine/core/random.h) | 2 | Value construction/copy, integer types, const method and equality | **pass:** seeded deterministic stream, closed/half-open ranges, replay position and errors are explicit. No hidden global clock or borrowed state; algorithm detail is not required to call uniform/chance. |

#### Every declaration meeting the parameter screen

Line numbers are those read at the source snapshot, not a permanence claim.

1. `engine/math/matrix3x2f.h:59`: `constexpr Matrix3x2F(float m11, float m12, float m21, float m22, float m31, float m32)` — **6**.
2. `engine/math/triangle.h:21`: `Triangle(float x0, float y0, float x1, float y1, float x2, float y2)` — **6**.
3. `engine/math/triangle.h:55`: `TriangleRightAxisAligned(float x0, float y0, float x1, float y1, float x2, float y2)` — **6**.
4. `engine/math/ericson_math.h:56`: `bool test_circle_triangle(const Circle& s, const Point2F& a, const Point2F& b, const Point2F& c, Point2F& p)` — **5**, with `p` an output reference.
5. `engine/math/ericson_math.h:182`: `bool test_2D_segment_segment(const Point2F& a, const Point2F& b, const Point2F& c, const Point2F& d, float& t, Point2F& p)` — **6**. Header states proper crossing and that `t`/`p` are written only on true.
6. `engine/math/ericson_math.h:242`: `void closest_pt_point_segment(const Point2F& c, const Point2F& a, const Point2F& b, float& t, Point2F& d)` — **5**. Header names d and t's equation and degenerate-segment result.

No variadic in-scope declarations were found in these 27 headers. No core
callable meets the five-outer-parameter screen. The maximum is six.

### Collision, rendering and scene

| Header under engine/ | Max outer parameters; witness | Caller C++ and engine vocabulary; judgment |
| --- | --- | --- |
| [collision/collision_object.h](../engine/collision/collision_object.h) | 3; `on_contact(other, normal, penetration)` | Inheritance, pure virtual/override, const borrowed shape pointer; layer/mask/tag, normal and penetration. Comments state normal orientation, per-tick deletion and game-owned response. G. |
| [collision/collision_layer.h](../engine/collision/collision_layer.h) | 4; `layers_collide(a_layer, a_mask, b_layer, b_mask)` | Type aliases, fixed-width integers, bit masks and constexpr free function. The two-way veto is stated; no pointer lifetime. G. |
| [collision/contacts.h](../engine/collision/contacts.h) | 3; `find_contacts(objects, contacts, broad_phase)` | `span<CollisionObject* const>`, vector output reference, nullable borrowed broad phase; candidate/contact/dispatch and all-pairs. Borrow/reuse and dispatch remeasurement are stated. G. |
| [collision/manifold.h](../engine/collision/manifold.h) | 0; no declared callable | Aggregate fields; unit normal, positive penetration, merely-touching distinction. G. |
| [collision/narrow_phase.h](../engine/collision/narrow_phase.h) | 2; `narrow_phase(a, b)` | Const shape references and `optional<Manifold>`; separating axes, convexity and unsupported circles. Preconditions/throw and nullopt are explicit. G. |
| [collision/resolve.h](../engine/collision/resolve.h) | 3; `separation_along(normal, penetration, axis)` | Value-returning vector arithmetic; unit vectors, alignment, finite positive depth, separation versus gameplay movement. Checked preconditions and velocity slide are stated. G. |
| [collision/broad_phase.h](../engine/collision/broad_phase.h) | 2; `find_pairs(objects, pairs)` | Span, vector of pairs and writable vector reference; uniform grid, warm buffers, coarse candidate pairs. Public result ordering/reuse are explicit. G. |
| [collision/tunnelling.h](../engine/collision/tunnelling.h) | 3; `can_tunnel(displacement, extent_a, extent_b)` / `max_safe_speed(extent_a, extent_b, seconds_per_step)` | Constexpr scalar functions; discrete sampling and one-dimensional motion assumptions. The text explicitly refuses a diagonal-safety proof. G. |
| [collision/partitioner.h](../engine/collision/partitioner.h) | 2; both `partition` overloads | `vector<pair<int,int>>`, int versus size_t overload; half-open worker ranges. Divisor and nonnegative-size checks are explicit. G. |
| [render/renderer.h](../engine/render/renderer.h) | 8; `DrawList::draw_sprite` and `draw_text` | Handles/enums, borrowed DrawList, move-only pimpl owner, virtual DeviceNotify hook, vector output; view, world/screen, premultiplied tint, frame lifecycle and device loss. List validity and resource teardown order are explicit. Raw sprite rotation/origin terms fail F1; positional count itself is retained with the disposition below. |
| [render/render_resources.h](../engine/render/render_resources.h) | 2; add/resolve/measure operations | `unique_ptr` transfer, opaque pimpl, paired typed handles, borrowed const pointers, strings/views and npos; registry slots, font atlas, load-time resolution and device restoration. Paired handles and Renderer/table destruction order are explicit. G. |
| [render/camera.h](../engine/render/camera.h) | 3; `Camera(x, y, scale)` / output `calculate_view_rectangle` | Value structs, const/output references, overloads/static functions; world-to-view transform and inverse. Units, zero scale and fit preconditions are explicit. G. |
| [render/viewport.h](../engine/render/viewport.h) | 6; scalar `Viewport` constructor | Copyable value, overloads/default arguments; back-buffer pixels, depth range and edge quantisation. Four commonly supplied geometry arguments and two defaulted depth values. G. |
| [render/colour.h](../engine/render/colour.h) | 4; RGBA constructors | Value/constexpr/overloads, explicit string conversion, operators, optional name lookup; normalised channels, RGBA and named palette. Namespace detail clamp is implementation, not caller knowledge. G. |
| [render/label.h](../engine/render/label.h) | 9; `Label` constructor | Multiple inheritance from GameObject and Text, final/override, borrowed resource table through Text contract; scene culling versus drawing. Comment explicitly delegates constructor parameters/loan/throws to Text. G. |
| [render/text.h](../engine/render/text.h) | 9; `Text` constructor | Inheritance, using-declarations exposing selected base members, wide/narrow strings, borrowed RenderResources; world position, unscaled-texel origin, clockwise radians and eager text measurement. Loan/outlives is explicit. G. |
| [render/text_object.h](../engine/render/text_object.h) | 9; `TextObject` constructor | Virtual methods, protected extension accessors, const drawing, wide strings and handles; glyph measurement/culling/cache versus concurrent fan-out. Constructor lifetime is explained by Text's public wrapper but not repeated here; common constructor belongs in F2's DrawObject loan documentation. |
| [render/font.h](../engine/render/font.h) | 3; `Font(atlas, glyphs, line_spacing)` | Vector value ownership, borrowed glyph pointers/references, string_view, template callable; glyph atlas, bearing/advance/line spacing and code-unit limits. `for_each_glyph` has two outer arguments and calls `action(glyph, pen)` with two. G. |
| [render/text_encoding.h](../engine/render/text_encoding.h) | 1; `widen(utf8)` / `narrow(wide)` | String ownership versus string_view loan; UTF-8/UTF-16, replacement and surrogate terms. Conversion ownership, timing and unsupported rendering consequences are stated. G. |
| [render/texture_object.h](../engine/render/texture_object.h) | 8; by-name constructor | Inheritance, protected draw/setters and paired handles; source frame, resource table, per-draw locals and concurrent pure reads. Ordinary protected draw takes 2 or 3 arguments; the table loan through DrawObject fails F2. |
| [render/sprite_sheet.h](../engine/render/sprite_sheet.h) | 9; position `draw` overloads | Typed handles/name tables and const references; named frame/strip, source texels, authored versus caller origin, world destination. Defaults reduce the ordinary draw to 3 required arguments; origin composition is explicit. G; raw rotation units still depend on F1's owning primitive contract. |
| [render/sprite_sheet_object.h](../engine/render/sprite_sheet_object.h) | 7; both constructors | Inheritance/protected constructor and typed handle table association; load-time name resolution and paired sheet/element change. Retained resource-table loan fails F2. |
| [render/sprite_frame.h](../engine/render/sprite_frame.h) | 3; position/size/origin constructor | Copyable value and const reference getters; source texels, authored pivot and unsupported packer rotation. Reference points into frame value, not an external owner. G. |
| [render/animation_strip.h](../engine/render/animation_strip.h) | 4; `AnimationStrip(first_frame, frame_count, frame_time, looping)` | Value owning cached vector, const reference result; frame rectangles/timing/looping and checked frame index. Reference is explicitly into strip's cache. G. |
| [render/draw_object.h](../engine/render/draw_object.h) | 6; `DrawObject` constructor | Virtual destructor, protected extension getters/setters and retained raw pointer; tint, flip, origin and intentionally ineffective layer depth. Resource loan/outlives is absent: F2. |
| [render/visual.h](../engine/render/visual.h) | 9; both constructors | Multiple inheritance/final/overrides, overloaded geometric value inputs; cached world bounds versus quantisation padding and authored origin composition. Retained resource table inherits F2. |
| [render/border_thickness.h](../engine/render/border_thickness.h) | 0; no declared callable | Aggregate four scalar fields; left/top/right/bottom are directly readable values. No external lifetime or advanced language requirement. G. |
| [render/viewport_manager.h](../engine/render/viewport_manager.h) | 1; constructor / `player_viewport(player_num)` | Borrowed raw manager pointer, enum and vector values; player panes/dividers versus devices. Retained ResolutionManager loan/outlives is absent: F2's same contract criterion. |
| [render/resolution_manager.h](../engine/render/resolution_manager.h) | 1; all setters/conversion | Value-returning vectors/strings, overloads and enum; live extent versus last requested preset/label. Exact-size route and checked positive extents are explained. G. |
| [render/screen_resolution.h](../engine/render/screen_resolution.h) | 0; no declared callable | Scoped enum, four labels and max alias. G. |
| [render/screen_layout.h](../engine/render/screen_layout.h) | 0; no declared callable | Scoped enum of one-to-four player layouts. G. |
| [scene/scene.h](../engine/scene/scene.h) | 2; constructor / `add_view` / `draw` | unique_ptr transfer, typed returned raw loans, span/optional/vector, template argument deduction, callback/lambda; pending adds/end_tick, collision phases, per-view const fan-out and overlays. `ViewOverlay` callback arity is 2 (`view_index`, list). Loan expiry, ownership and pure-read callback conditions are explicit; implementation if-constexpr/type traits are not demanded at the call site. G. |

#### Every declaration at the five-parameter screen

Whitespace is flattened below; type/name/default spellings are the source
spellings. There are 18 declarations, all in render. Source lines are for the
audited revision; they are evidence locators rather than future selectors.

| Source | Formal / required | Exact declaration |
| --- | --- | --- |
| renderer.h:188 | 8 / 8 | `void draw_sprite(TextureHandle texture, const RectangleI& source, const RectangleF& destination, const Colour& tint, float rotation, const Vector2F& origin, SpriteFlip flip, float layer_depth);` |
| renderer.h:213 | 8 / 8 | `void draw_text(FontHandle font, const std::wstring& text, const Vector2F& position, const Colour& tint, float scale, float rotation, const Vector2F& origin, float layer_depth);` |
| viewport.h:32 | 6 / 4 | `Viewport(float x, float y, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f);` |
| label.h:42 | 9 / 4 | `Label(const std::wstring& text, const std::string& font_name, const Vector2F& position, RenderResources* render_resources, const Colour& colour = Colour::white, float scale = 1.0f, float rotation = 0.0f, const Vector2F& origin = Vector2F::zero, float layer_depth = 0.0f);` |
| text.h:27 | 9 / 4 | `Text(const std::wstring& text, const std::string& font_name, const Vector2F& position, RenderResources* render_resources, const Colour& color = Colour::white, float scale = 1.0f, float rotation = 0.0f, const Vector2F& origin = Vector2F::zero, float layer_depth = 0.0f);` |
| text_object.h:30 | 9 / 4 | `TextObject(const std::wstring& text, const std::string& font_name, const Vector2F& position, RenderResources* render_resources, const Colour& color = Colour::white, float scale = 1.0f, float rotation = 0.0f, const Vector2F& origin = Vector2F::zero, float layer_depth = 0.0f);` |
| texture_object.h:34 | 8 / 3 | `TextureObject(const std::string& sheet_name, const std::string& frame_name, RenderResources* render_resources, const Colour& color = Colour::white, float rotation = 0.0f, const Vector2F& origin = Vector2F::zero, SpriteFlip flip = SpriteFlip::none, float layer_depth = 0.0f);` |
| texture_object.h:45 | 7 / 2 | `TextureObject(RenderResources::SheetFrame frame, RenderResources* render_resources, const Colour& color = Colour::white, float rotation = 0.0f, const Vector2F& origin = Vector2F::zero, SpriteFlip flip = SpriteFlip::none, float layer_depth = 0.0f);` |
| texture_object.h:87 (protected) | 7 / 7 | `void draw_with(DrawList& draw_list, const RectangleF& destination_rectangle, SpriteSheet::frame_handle frame, const Colour& colour, const Vector2F& origin, SpriteFlip flip, float rotation) const;` |
| sprite_sheet.h:74 | 8 / 3 | `void draw(DrawList& draw_list, frame_handle frame, const RectangleF& destination, const Colour& colour = Colour::white, float rotation = 0.0f, const Vector2F& origin = Vector2F::zero, SpriteFlip flip = SpriteFlip::none, float layer_depth = 0.0f) const;` |
| sprite_sheet.h:83 | 9 / 3 | `void draw(DrawList& draw_list, frame_handle frame, const Vector2F& position, const Colour& colour = Colour::white, float rotation = 0.0f, const Vector2F& origin = Vector2F::zero, float scale = 1.0f, SpriteFlip flip = SpriteFlip::none, float layer_depth = 0.0f) const;` |
| sprite_sheet.h:103 | 8 / 3 | `void draw(DrawList& draw_list, const RectangleI& source, const RectangleF& destination, const Colour& colour = Colour::white, float rotation = 0.0f, const Vector2F& origin = Vector2F::zero, SpriteFlip flip = SpriteFlip::none, float layer_depth = 0.0f) const;` |
| sprite_sheet.h:112 | 9 / 3 | `void draw(DrawList& draw_list, const RectangleI& source, const Vector2F& position, const Colour& colour = Colour::white, float rotation = 0.0f, const Vector2F& origin = Vector2F::zero, float scale = 1.0f, SpriteFlip flip = SpriteFlip::none, float layer_depth = 0.0f) const;` |
| sprite_sheet_object.h:27 | 7 / 2 | `SpriteSheetObject(const std::string& sheet_name, RenderResources* render_resources, const Colour& color = Colour::white, float rotation = 0.0f, const Vector2F& origin = Vector2F::zero, SpriteFlip flip = SpriteFlip::none, float layer_depth = 0.0f);` |
| sprite_sheet_object.h:38 (protected) | 7 / 2 | `SpriteSheetObject(RenderResources::SpriteSheetHandle sheet, RenderResources* render_resources, const Colour& color = Colour::white, float rotation = 0.0f, const Vector2F& origin = Vector2F::zero, SpriteFlip flip = SpriteFlip::none, float layer_depth = 0.0f);` |
| draw_object.h:15 | 6 / 1 | `DrawObject(RenderResources* render_resources, const Colour& color = Colour::white, float rotation = 0.0f, const Vector2F& origin = Vector2F::zero, SpriteFlip flip = SpriteFlip::none, float layer_depth = 0.0f);` |
| visual.h:18 | 9 / 4 | `Visual(const std::string& sheet_name, const std::string& frame_name, const RectangleF& rectangle, RenderResources* render_resources, const Colour& color = Colour::white, float rotation = 0.0f, const Vector2F& origin = Vector2F::zero, SpriteFlip flip = SpriteFlip::none, float layer_depth = 0.0f);` |
| visual.h:30 | 9 / 4 | `Visual(const std::string& sheet_name, const std::string& frame_name, const RectangleRotated& rect_rotated, RenderResources* render_resources, const Colour& color = Colour::white, float rotation = 0.0f, const Vector2F& origin = Vector2F::zero, SpriteFlip flip = SpriteFlip::none, float layer_depth = 0.0f);` |

### Input, audio, UI, assets and app

| Header | Max | C++ features / caller terms and loan evidence | Verdict |
|---|---:|---|---|
| [input/keyboard.h](../engine/input/keyboard.h) | 3 | Scoped enum, value snapshot, const references, `optional<Key>` and borrowed `string_view`. Key position versus typed character, focus-gated edges, UTF-8 text and `typed()` invalidation at next poll are explicit (:14-22, :124-146, :189-193). Max is free `pressed`/`released` (:136-139). | Gloss: optional/view and text encodings; no contract failure found. |
| [input/gamepads.h](../engine/input/gamepads.h) | 3 | Borrowed shell-owned reader/snapshots, enum buttons and triggers. Slot versus player identity and frame edges are explained; reader must outlive the object (:34-36). Max is trigger queries (:56-61). | Clear: threshold and unavailable/submitted output meanings are stated. |
| [input/gamepad.h](../engine/input/gamepad.h) | 4 | Aggregate values, scoped enums, const-reference free functions. Connection-gated edges, deadzone and bounded vibration fields are defined. Max is free `trigger_pressed`/`trigger_released` (:123-126). | Clear: device identity limitations and source-combination policy are explicit. |
| [input/mouse.h](../engine/input/mouse.h) | 3 | Enum, snapshot references, integer vectors. Client pixels, wheel deltas, focus, cancelled capture and epoch semantics are explained. `Application` ownership and shell polling are stated (:133-141). Max is free button-edge queries (:116-119). | Clear: no unexplained retained ownership found. |
| [input/touch.h](../engine/input/touch.h) | 3 | Nullable pointer lookup, arrays, const `span` and range-for. Engine point IDs, cancelled contacts and focus rules are explained. The app owns/polls the borrowed device (:120-128); the returned point ranges refer to its snapshots. Max is free edges/motion (:107-115). | Gloss: span is a view; retaining ranges/pointers across polling needs ordinary view-lifetime teaching, not a new collection API. |
| [input/direction.h](../engine/input/direction.h) | 2 | Enum, references, value class with a two-argument constructor/update. Deadzone, dominant axis, repeat delay and opening-under-held-input behavior are defined (:30-80, :102-140). | Clear. |
| [input/gamepad_reader.h](../engine/input/gamepad_reader.h) | 2 | Move-only RAII seam, opaque implementation, returned value. Slot stability, sampling, suspend and vibration limits are explained. Max is `vibrate` (:90). | Gloss: move/RAII and seam vocabulary; ordinary game code uses `Gamepads`. No lifecycle puzzle is imposed on that route. |
| [audio/sound_bank.h](../engine/audio/sound_bank.h) | 5 | Typed handles, `unique_ptr`, registry values, borrowed device. Device must outlive the bank (:77-82). Wave versus retained effect, silence substitute, update-side-only const writes and group levels are explained. Max is `play_effect` (:171-172), with only effect required. | Gloss: handle, wave/voice/effect and ownership transfer; five-argument screen is disposed below. |
| [audio/audio_resources.h](../engine/audio/audio_resources.h) | 2 | Owned `unique_ptr<SoundBank>` input, borrowed raw bank result, typed group handles. Sound groups and live volume propagation are explained. Max is add-bank / set-volume (:40-41, :79). | **Fail F-S1**: caller prose promises bank lifetime to destructor but re-adding the public name replaces/destroys its bank. See evidence below. |
| [audio/sound_group.h](../engine/audio/sound_group.h) | 0 | Incomplete tag and typed-handle alias only. AudioResources owns groups; cross-owner handle misuse is expressly warned about (:17-19). | Gloss: incomplete tag/typed handle; no callable or unresolved ownership found. |
| [audio/audio_device.h](../engine/audio/audio_device.h) | 5 | Move-only RAII seam, typed handles, vectors of strings, opaque implementation pointer. Bank, voice, one-shot and recovery vocabulary is defined. Max is `play_wave` / `play_voice` (:127-134). | Gloss: advanced seam, not required per-frame game code; five-argument screen is disposed below. |
| [audio/xwb_file.h](../engine/audio/xwb_file.h) | 1 | Aggregate/vector ownership, enum, const-reference input, returned value. PCM, interleaved frames, loop region and decoding memory cost are explained. Max is `read_xwb_file` (:89); `WaveData::frames` takes zero. | Gloss: authored-audio format vocabulary; no borrowed-result trap found. |
| [ui/widget.h](../engine/ui/widget.h) | 11 | Class inheritance and virtual overrides, multiple inheritance for leaves, const draw, borrowed child pointers, vectors of pointer/name pairs. Child lifetimes and derived-member destruction are explicit (:63-66). Two leaf constructors have five required arguments followed by six defaults (:103-113, :141-151). | Gloss for inheritance/defaults. **Fail F2 for UiTexture**: its retained table loan is not stated in its own base chain. UiText differs: `Text` explicitly says table borrowed/must outlive it. |
| [ui/focus.h](../engine/ui/focus.h) | 3 | Defaulted value-style options, borrowed widget pointers and stored callable. Page/widgets must outlive FocusGroup (:74-77). Slot, disabled/refused and repaint rules are explicit. Max is `move` (:119). | Gloss: stored lambda captures; no additional loan contradiction found. |
| [ui/button.h](../engine/ui/button.h) | 2 | Stored `std::function<void()>`, copy/move semantics, borrowed widget pointer. Callable copying and captures' own sharing/borrowing semantics are explicit (:33-39), as is action survival through reentrant removal (:46-51). Max is constructor (:32). | Gloss: capture lifetime and reentrancy; documented rather than a hidden ownership puzzle. |
| [ui/navigation.h](../engine/ui/navigation.h) | 4 | Vector of borrowed pointers or rectangle values, nullable pointer / -1 result, overloads. Geometric navigation and wrap rules are specified; candidate pointers are not retained (:63-76). | Clear. |
| [assets/asset_manifest.h](../engine/assets/asset_manifest.h) | 0 | Owning strings/vectors and aggregate defaults. Manifest kind, file stem, optional content and ordered loading are defined (:8-55). | Clear: data-only surface. |
| [assets/resource_loader.h](../engine/assets/resource_loader.h) | 4 | Four service pointers, stored `std::function` callbacks, aggregate AssetKind, deleted copy, moved-in manifest value. Stores are not owned (:61-78); callback registration/device-only reload and preserved non-device loans are explained. Max is constructor (:76-78). | Gloss: stored callbacks/`this`, borrowing and device restore; explicit construction order is handled by Application. No new source-level failure found. |
| [assets/json.h](../engine/assets/json.h) | 2 | Checked node view, owning move-only document, strings/vectors, exceptions. Node lifetime is explicitly bounded by document (:30-42); parse copies inputs (:20-24), `keys()` returns independent copies (:60-64). Max is `parse_json` (:28). | Gloss: move-only owner versus borrowed view; contract is explicit. |
| [assets/asset_manifest_loader.h](../engine/assets/asset_manifest_loader.h) | 1 | C-string path, aggregate return, exceptions. JSON grouping/flattening, order, exact allowed keys and errors are shown (:7-32). | Clear: schema need not be inferred from implementation. |
| [assets/sprite_sheet_loader.h](../engine/assets/sprite_sheet_loader.h) | 2 | C-string path, typed texture handle, owning `unique_ptr` result. Parsed sprite/frame/strip data, rejected rotated frames and checked schema are described (:8-30). | Gloss: ownership transfer and source rectangles/strips; no new loan issue. |
| [assets/sound_bank_loader.h](../engine/assets/sound_bank_loader.h) | 3 | Owning definition aggregates, device pointer, typed bank handle, `unique_ptr` result. Open-before-build ordering and named failures are explained (:45-61); bank's own header states retained device loan. | Gloss: parsed definition versus opened bank; no new loan issue. |
| [app/application.h](../engine/app/application.h) | 1 | Options aggregate, `unique_ptr<State>` transfer, borrowed service pointers, chrono values, base interfaces. The three-step boot order, static browser lifetime and service loans/destruction ordering are explicit (:141-190, :292-306). Max is constructor/run/load/set calls; no call exceeds one. | Gloss: RAII, static storage, ownership transfer, fixed step/fan-out; source contracts make these teachable without allocator knowledge. This audit does not qualify browser behavior. |
| [app/content_root.h](../engine/app/content_root.h) | 2 | Owning strings and manifest values, const-reference inputs. Executable versus working-directory versus manifest-relative paths are explicitly distinguished (:9-54). Max is `resolved_under` (:46-47). | Clear. |
| [app/window.h](../engine/app/window.h) | 3 | Virtual notification interface (including protected destructor), options aggregate, deleted copy, opaque `void*` handle. Constructor notification can happen before return (:186-205); destruction stops notifying (:207-220), handles become invalid on close (:226-233), browser loop lifetime is explicit (:255-264). Max is `WindowNotify::on_touch_down/move/up` (:113-115); Window constructor is two. | Gloss: virtual interface and opaque handle; ordinary games use Application. No additional source-level failure found. |

#### All high-arity declarations in these 25 headers

Source lines below refer to the reviewed source, not a promise that line numbers
will remain stable. Defaults still count as parameters.

1. `engine/audio/sound_bank.h:171-172` — five, one required:

   ```cpp
   void play_effect(EffectHandle effect, bool loop = false,
       float volume = 1.0f, float pitch = 0.0f, float pan = 0.0f) const;
   ```

2. `engine/audio/audio_device.h:127-128` — five, all required:

   ```cpp
   void play_wave(WaveBankHandle bank, int wave_index, float volume,
       float pitch, float pan) const;
   ```

3. `engine/audio/audio_device.h:133-134` — five, all required:

   ```cpp
   void play_voice(VoiceHandle voice, bool loop, float volume, float pitch,
       float pan) const;
   ```

4. `engine/ui/widget.h:103-113` — eleven, five required:

   ```cpp
   UiTexture(const std::string& name,
       const std::string& sheet_name,
       const std::string& frame_name,
       const RectangleF& rectangle,
       RenderResources* render_resources,
       const Colour& color = Colour::white,
       bool hidden = false,
       float rotation = 0.0f,
       const Vector2F& origin = Vector2F::zero,
       SpriteFlip flip = SpriteFlip::none,
       float layer_depth = 0.0f);
   ```

5. `engine/ui/widget.h:141-151` — eleven, five required:

   ```cpp
   UiText(const std::string& name,
       const std::wstring& text,
       const std::string& font_name,
       const Vector2F& position,
       RenderResources* render_resources,
       const Colour& color = Colour::white,
       bool hidden = false,
       float scale = 1.0f,
       float rotation = 0.0f,
       const Vector2F& origin = Vector2F::zero,
       float layer_depth = 0.0f);
   ```

In these 25 service headers, the only callback aliases are `Button::Action = std::function<void()>`
(`button.h:26`, invocation arity zero) and `ResourceLoader::LoadAsset`
(`resource_loader.h:30-31`, invocation arity three: directory, name, optional).
FocusGroup's Action argument reuses Button's alias. WindowNotify's virtual
notification methods are counted directly, not as an extra callback alias.

## Call-site verdict

The run found 29 declarations at the five-parameter screen: six in math,
18 in render and five in audio/UI. The largest fixed count is 11, in the
UiTexture/UiText constructors, with five required inputs and six defaults.
No in-scope variadic callable was found. Other stored or invoked callbacks
reviewed are StateContext's result callbacks (zero or one input), ThreadPool
tasks (zero), Font's glyph visitor (two) and Scene's per-view overlay (two).
Their borrowing and completion rules matter independently of outer arity.

### The eight required draw_sprite arguments

[DrawList::draw_sprite](../engine/render/renderer.h) has no defaults:

| Position | Argument | What a caller has to distinguish |
| --- | --- | --- |
| 1 | `TextureHandle texture` | The texture slot in the attached resource table. |
| 2 | `RectangleI source` | The selected texture rectangle. The primitive should state its units. |
| 3 | `RectangleF destination` | World-space destination, mapped by the list's camera. |
| 4 | `Colour tint` | Multiplication as given under premultiplied blending; zero alpha alone does not erase RGB. |
| 5 | `float rotation` | The angle. Its units and screen direction are missing from this caller contract. |
| 6 | `Vector2F origin` | The pivot offset. Its units and relationship to the destination are missing here. |
| 7 | `SpriteFlip flip` | A distinct enum, rather than another boolean or float. |
| 8 | `float layer_depth` | Documented as ineffective for ordering: call/list order decides coverage. |

A real full call is [Bar::draw](../samples/components/slider.cpp): texture,
one-texel source, rectangle, colour, `0.0f`, `Vector2F::zero`,
`SpriteFlip::none`, `0.0f`. Its two float zeros are a reading cost; the names
in the declaration supply their roles, but cannot supply unstated units.

The minimal sample does not call this primitive directly.
[HelloState](../samples/minimal/states/hello_state.cpp) constructs Labels with
five explicit arguments (four required), then draws through
`scene_->draw(renderer)`. A Visual constructor has four required inputs;
TextureObject's protected positioned/rectangle draws take two or three.
SpriteSheet's appearance defaults reduce a draw to the list, frame/source
and destination/position. The
[strip example](../samples/components/animation_preview.cpp) supplies those
three plus its deliberate colour. Wrappers reduce ordinary calls; they do
not remove the raw primitive's contract obligation or their own formal
argument counts.

**Verdict:** the eight-position primitive is explainable as explicit typed
inputs, with a documented ordering caveat and shorter existing game paths,
but its present rotation/origin explanation fails the source-review criterion
(F1 below). This is not a finding that a particular beginner understood or
failed it. An options object or another API layer is not selected merely
from the count: T1 needs a concrete caller workaround and T4 needs an actual
clarity improvement. Keep the positional cost visible and fix its missing
contract. The two float slots deserve review in a future teaching run.

### Other long calls and ordinary C++

Matrix/triangle APIs already offer named factories or grouped point values;
the book-derived geometric routines expose vertices and outputs, with the
ordinary named-shape predicates available above them. T1/T4 do not justify
another wrapper from their count alone. Viewport's last two depth arguments
are defaulted. Text, sprite and UI constructors have appearance defaults and
named setters; a worked example should teach the identity, geometry and
resource inputs before that tail. The audio sample's
[start_loop](../samples/audio/sound_board.cpp) supplies a named handle and
named volume/pitch/pan members to its full call, while default SoundBank
playback needs just the effect handle. These screens are retained with that
T1/T4 rationale rather than filed as speculative API redesigns.

The glossary need is real: ownership versus loans, const overrides, lambda
captures, typed handles, spans, fixed steps and view fan-out occur in ordinary
game code. Existing SRL-16 owns the minimal-sample explanation. Optional
storage customization, device construction and low-level geometry have a
larger teaching burden than the first sample. This review separates those
paths; it does not award the whole API a beginner-usability pass.

## Failed criteria and disposition

All three findings are filed in
[SRL-22](product/BACKLOG.md#srl-22), which owns the exact repair and checks.
They are source-contract defects under T4/T10, not new runtime failures.

| Finding | Evidence at the audited revision | Caller effect and disposition |
| --- | --- | --- |
| F1: sprite units and pivot | `renderer.h:182-195` names rotation/origin but omits units/direction/pivot rules. Text and SpriteSheet explain their own paths, not this primitive. | A caller must inspect shared quad code to interpret arguments. SRL-22 will verify those semantics and state them beside the owning declaration. |
| F2: retained service loans | DrawObject stores RenderResources; SpriteSheetObject/TextureObject/direct TextObject retain it through that base. The sprite chain also reaches Visual and UiTexture. ViewportManager stores ResolutionManager and later dereferences it. Their owning constructor contracts do not state the loan/outlives rule. Text/UiText and Label's explicit Text delegation already do. | Pointer syntax cannot tell a caller which lifetime must be longer. SRL-22 covers these specific retained services, without a smart-pointer redesign or a claim that UiText has the same omission. |
| F-S1: bank-pointer invalidation | `audio_resources.h:43-47` promises no bank release before destruction; public add forwards to Registry, whose existing-name assignment destroys/replaces the unique_ptr. Adding null empties the slot; private bank-order prose already acknowledges replacement. | A saved raw bank pointer can dangle after re-add despite the public promise. Slot handles survive replacement; pointers do not. SRL-22 will correct getter/add invalidation wording to match the existing implementation. |

The root reviewer checked the loan stores/dereferences and Registry's
replacement against their implementations, rather than inferring ownership
from raw pointers alone. No observed dangling-pointer incident, rendering
error or audio output is claimed. Fixing these findings is outside SRL-17's
audit acceptance; filing them does not mean they are fixed.

## Verification record

Three source reviews reconcile to 84 unique audience paths; a separate
review checked that inventory, all 29 screened declarations and the three
failed-contract facets. The integration records formal/required counts for
the defaulted drawing calls, following that review. A read-only inventory
receipt compares the ledger with published.ts and records the 84 source
hashes; it does not parse C++ or prove the judgments.

All seven standalone checks pass, including citation closure. The website
reports zero type-check diagnostics, 160 passing tests and a successful
144-page/433-file build. The emitted reference-page link targets this audit
at the site's built revision; its scope and unfinished-teaching qualification
were inspected. All 84 ledger paths, unchanged header hashes and 93 local
Markdown targets reconcile. Final review's finding-label, verdict-legend and
callback-scope clarifications are applied.

The run executed no game, compiler, device, browser, SDK activation, external
network operation or stopped ownership checker. T10's recorded-series and
actual beginner teaching acceptance remain open.
