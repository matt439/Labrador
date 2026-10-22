# Drawing and drawables: reports 18, 20, 22

Reports 18, 20 and 22 from ColourWars are about what a drawable can do from
inside the engine's drawing classes. Report 18 wants to hide a scene label
while keeping its words, its box and its colour. Report 20 wants a drawable to
know which view it is drawing into. Report 22 wants to re-point a sprite or an
animation at another sheet without naming it by string. None of the three
needs `GameObject`'s virtual signature or `Scene`'s dispatch to change. Two
rules decide them. The first is that whether and how something is drawn is
decided in `draw()` (`engine/core/game_object.h:72-74`, and PHILOSOPHY,
Simulation and rendering, `docs/design/PHILOSOPHY.md:526-531`). The second is
the test in PHILOSOPHY, The public face: "whether a client would have to write
them itself" (`docs/design/PHILOSOPHY.md:792-798`). No answer here rests on
counting callers, which the same passage rules out.

Report 18 already has its mechanism, `UiText::set_hidden`, and nothing tells a
reader to look for it. It is answered with comments. Reports 20 and 22 are each
a case of the engine contradicting a rule it states. In report 20, the rule is
that the per-view draw walk must not be hand-copied (`engine/scene/scene.h:240-243`),
yet a drawable cannot get its view's camera without copying it. In report 22,
the rule is "Per-frame code touches no string-keyed map"
(`docs/design/PHILOSOPHY.md:630-631`, `:671`), yet the only way to re-point an
animation is by name. In both, the client's narrower proposal is replaced by a
shape that keeps T2 and T5. Report 20 gets a `const` reader on a list one worker
owns, not a context parameter. Report 22 gets a paired value that cannot be
mismatched, not two raw handles. The order to implement them in:

1. Report 18's comments in `label.h`, `colour.h` and `renderer.h` (XS, one
   commit, not consumer-visible).
2. Report 20, `DrawList::camera()` and `DrawList::viewport()`, as a story of
   its own (M). It touches all five render backends and the wasm build of
   `gl/`.
3. Report 22, `RenderResources::SheetFrame` and `SheetStrip`, as a story of
   its own (M). It is headless and touches no backend.

Reports 18 and 20 both edit `DrawList`'s comments in `engine/render/renderer.h`.
They can land in either order, but not from parallel branches, or one rebase
will conflict. Report 22 conflicts with neither.

Line numbers are as of `0c15f34`. This is a decision record, written ahead of
PI-09 (filing the reports) and PI-10 (answering them)
(`docs/product/BACKLOG.md:245`, `:398`). It implements nothing. GAP-REGISTER
G74 (`docs/product/GAP-REGISTER.md:1489`) is the gap these answers close.

## 18. Hiding a scene label

**Decision.** Document only. No `Label::set_visible` is added: `UiText` is
already text a scene can hold, with a `set_hidden` that keeps its words, its
box and its colour. The answer is a pointer to it from `label.h`, and a warning
at `Colour::make_transparent` and at the tint parameters that a zero-alpha tint
is not invisible under the engine's premultiplied blend.

**What the engine has now.** `Label` is `final` and draws by forwarding to
`TextObject::draw` (`engine/render/label.h:26`, `engine/render/label.cpp:29-32`).
Through `Text` it has text, colour, scale and position setters and no
visibility setter (`engine/render/text.h:33-36`). `TextObject::set_font` is
protected (`engine/render/text_object.h:88`, in the section opened at `:81`).
`set_text(L"")` draws no glyphs, but it also re-measures, so the box shrinks to
nothing (`engine/render/text_object.cpp:75-79`, `remeasure` at `:85-95`). A
scene never retires a plain object. Only collision objects are retired
(`engine/scene/scene.h:107-116`).

The report's central claim, that no engine class hides scene text while keeping
its words, box and colour, does not hold. `UiText` is `UiWidget` (through
`UiObject` to `GameObject`) plus `Text` (`engine/ui/widget.h:186-206`).
`UiObject` has `set_hidden` and `hidden` (`engine/ui/widget.h:38-39`).
`UiText::draw` returns early when hidden (`engine/ui/widget.cpp:271-278`), and
`bounds()` is `text_bounds()` either way (`engine/ui/widget.cpp:283-290`). It
is deliberately not `final` (`engine/ui/widget.h:128-142`). `Scene::add` takes
any `GameObject` (`engine/scene/scene.h:125`), and the components gallery adds
`UiText` straight to a `Scene` today (`samples/components/gallery_state.cpp:66-76`).

The report does find a real trap. `Colour::make_transparent` zeroes alpha only
(`engine/render/colour.h:77`, `engine/render/colour.cpp:159-162`). The blend is
premultiplied (`engine/render/SEAM.md:174`, pinned by `RenderPixelTests`,
"CONTRACT: the blend equation is premultiplied alpha",
`tests/render/pixel_tests.cpp:886`), so a tint of `(r, g, b, 0)` still adds
its colour over whatever is beneath it. Nothing at `Colour`, at
`DrawList::draw_sprite`'s or `draw_text`'s tint (`engine/render/renderer.h:172-179`,
`:201-208`) or at `SpriteVertex::colour` (`engine/render/sprite_vertex.h:39-41`)
says so. The only place it is written down is in the pixel tests
(`tests/render/pixel_tests.cpp:1985-1989`). The client's own workaround
happens to be right: it tints with all four channels zero. The trap is
`make_transparent`, so this fix protects the next caller rather than this one.

Two engine samples hide a scene `Label` a third way, by blanking it and keeping
the string elsewhere. `samples/minimal/states/hello_state.cpp:192` does this for
each idle touch marker, inside the per-frame loop at `:179-194`.
`samples/linesweeper/states/play_state.cpp:203-204` does it for the two hint
labels (`samples/linesweeper/states/play_state.h:112-113`), gated on a change
at `:200`. Neither needs the box kept, so for both `set_text(L"")` is a
complete answer, not a workaround.

**Why.** The mechanism the report asks for exists one class over. A hidden flag
on `Label` would leave `UiText` differing from `Label` only by a name. That is
two spellings of one mechanism with no rule for choosing between them, which T4
refuses: "duplication is the opposite of clarity at scale"
(`docs/design/PHILOSOPHY.md:91-92`). `game_object.h` already says where the
property lives: "Whether anything is drawn is a property of draw(), not of the
extent" (`engine/core/game_object.h:72-74`). `UiText::draw` is where it lives.
The UI section makes every leaf open to derivation
(`docs/design/PHILOSOPHY.md:588-593`), so a client wanting more than `UiText`
derives from it. This decline does not rest on the client count. Under The
public face's test (`docs/design/PHILOSOPHY.md:792-798`) the client does not
have to write anything itself: it changes a type.

The real fault is discoverability. `Label`'s class comment presents it as "A
string a scene can hold" (`engine/render/label.h:13`) and nothing points the
other way. The tint trap is a contract a caller is held to, and CONVENTIONS
puts such a contract "at the declaration"
(`docs/design/CONVENTIONS.md:144-148`). T6 asks that a broken contract not be
quiet (`docs/design/PHILOSOPHY.md:109-114`). A tint that looks invisible in
the code but draws on screen is the quiet kind. A named `Colour::transparent`
constant equal to `(0, 0, 0, 0)` would sit naturally in `colour.h`'s
named-colour list, but it is a new public name that nothing needs, so it is
left out (T1, `docs/design/PHILOSOPHY.md:45-47`).

**The change.** Comments only, in three published headers
(`website/src/lib/published.ts:139`, `:143`, `:144`). They are contract
sentences, never history (`docs/design/CONVENTIONS.md:139-160`):

- `engine/render/label.h`, class comment (`:13-25`). Add: a scene label that
  must stop drawing while keeping its words, its box and its colour is a
  `UiText` (engine/ui/widget.h), whose `set_hidden` skips the draw and leaves
  `bounds()` alone. `set_text(L"")` is the supported way to hide a label whose
  box may go empty. It re-measures on every call, so a caller that does it
  every frame gates it on a change, as LineSweeper's play state does. The
  sentence is a pointer, not a dependency: `engine/render` names a class in
  `engine/ui` in prose only, and must not include it, which keeps
  ARCHITECTURE's module direction intact.
- `engine/render/colour.h`, at `make_transparent` (`:77`). Add: it zeroes
  alpha and leaves the colour channels alone. Under the engine's premultiplied
  blend (engine/render/SEAM.md#5), a tint's colour channels are added whatever
  its alpha, so the tint that contributes nothing is `Colour(0, 0, 0, 0)`.
- `engine/render/renderer.h`, at `draw_sprite` (`:155-171`) and `draw_text`
  (`:181-200`). Add one clause on `tint`: it multiplies the sampled texel as
  given, nothing premultiplies it for the caller, and the blend is
  premultiplied (SEAM.md#5).

The citation form `SEAM.md#5` must resolve under
`cmake/check_doc_citations.cmake`. `## 5.` is at `engine/render/SEAM.md:164`.
Optionally, the website's text table (`website/src/content/docs/docs/guides/sprites-and-text.mdx:89-93`)
can add that a `UiText` is also the scene text that can hide. No platform,
backend or compiler concern applies: no code changes.

**Tests.** No new case, because no behaviour changes. The commit must pass the
citation check and the website build (`npm ci`, `npm run build` in `website/`),
since all three headers are published word for word. One gap is worth
recording. No test pins `UiText`'s hidden early-out or that its `bounds()`
survives hiding: `tests/ui` uses a stub widget, and `tests/ui/stub_widget.h:14-21`
says why the real ones are not built there yet. That case would live in a
null-preset-only file added to `UiTests`, and it is the natural companion to
this comment, which now presents that behaviour as the answer. It is
recommended in the same commit, which makes it no longer comments-only. If it
goes in, it runs under `x64-debug-null`, `x64-release-null` and CI's wasm null
configure.

**Consumer-visible.** No. The commit touches `engine/**/*.h`, so its message
says `not consumer-visible` (`CHANGES.md:18-22`). No `samples/minimal/README.md`
line: the sample's code does not change, and its blanking hide is now the
documented one.

**What the client could delete.** It can hold its overlay labels as `UiText`
rather than `Label`, and then replace its zero-RGBA tint and the comment that
explains it with `set_hidden`, keeping the label's real colour throughout.
What stays client policy: when the labels are created, the scene admission
that follows, and whether a shown colour is premultiplied.

**Open questions.** None block the comments. Why `Label` is `final` when
`UiText` is open is a separate question this report does not raise. The other
parts of report 18 that have not been filed are not decided here: category
order, layers, retiring plain objects and a public `set_font`.

## 20. A drawable that knows its view

**Decision.** Accept, narrowed. Add two read-only accessors to `DrawList`,
`camera()` and `viewport()`, which return the state the next draw on that list
will use. `GameObject::draw`'s signature and `Scene`'s dispatch stay as they
are. This is a story of its own.

**What the engine has now.** `GameObject::draw(DrawList&) const` takes nothing
else (`engine/core/game_object.h:57`). Its comment says the camera is on the
`DrawList` rather than a parameter (`:46-53`). `DrawList` has setters for
viewport, camera and filter, and its only reader is `valid()`
(`engine/render/renderer.h:127`, `:130`, `:142`, `:153`). `DrawList::View` is
backend-defined (`engine/render/renderer.h:213-214`). A caller outside
`draw()` can read `Scene::view(index)` (`engine/scene/scene.h:162-165`).
`cull_bounds` already takes the view's world units per pixel
(`engine/core/game_object.h:77-85`), which `Scene` passes as
`1.0f / view.camera.scale` (`engine/scene/scene.cpp:247`, `:256`). The scene
sets each view's viewport and camera before any object draws
(`engine/scene/scene.cpp:221-223`). `ViewOverlay` runs after the world objects,
is not culled and takes the view index
(`engine/scene/scene.h:69-83`, `engine/scene/scene.cpp:262-265`).

`game_object.h:48-53` says an implementation "either hands a camera straight
down or calls Camera::calculate_view_rectangle with it". Neither is possible
from a `draw()` today. The only callers of `calculate_view_rectangle` are the
backends' own `draw_sprite` (`engine/render/d3d11/renderer.cpp:393`,
`engine/render/d3d12/renderer.cpp:457`, `engine/render/gl/renderer.cpp:305`,
`engine/render/vulkan/renderer.cpp:249`, `engine/render/null/renderer.cpp:88`).
No `GameObject::draw` in `engine/` or `samples/` calls it, and none can reach
a camera to pass. The sentence describes the API this report asks for.

All five backends hold the current camera per view and reset it to
`Camera::DEFAULT_CAMERA` (members at `engine/render/d3d11/backend.h:144`,
`engine/render/d3d12/backend.h:223`, `engine/render/gl/backend.h:154`,
`engine/render/vulkan/backend.h:160`, `engine/render/null/backend.h:82`).
Each view's reset is in its `View::reset` (`engine/render/d3d11/renderer.cpp:282-290`,
`engine/render/d3d12/renderer.cpp:391-399`, `engine/render/gl/renderer.cpp:246-248`,
`engine/render/vulkan/renderer.cpp:190-192`, `engine/render/null/renderer.cpp:43-45`).
Only three of the five hold the current `Viewport`:

- `gl/`, `vulkan/` and `null/` keep a `Viewport` member
  (`engine/render/gl/backend.h:156`, `engine/render/vulkan/backend.h:162`,
  `engine/render/null/backend.h:84`). They store it as given in
  `set_viewport` (`engine/render/gl/renderer.cpp:258-266`,
  `engine/render/vulkan/renderer.cpp:202-210`, `engine/render/null/renderer.cpp:53-56`)
  and reset it to the whole drawable after `reset()`, at begin-frame and at a
  mid-frame resize restart (`engine/render/gl/renderer.cpp:749`, `:800`;
  `engine/render/null/renderer.cpp:240`, `:268`;
  `engine/render/vulkan/renderer.cpp:731`, in `reset_views` at `:724`).
- `d3d11/` turns the viewport straight into a D3D viewport and a transform and
  keeps no `Viewport` (`engine/render/d3d11/renderer.cpp:338-350`). Its default
  is not set in `View::reset` but in `View::bind`
  (`engine/render/d3d11/renderer.cpp:268-280`), which `set_view_count` calls
  with the device's screen viewport (`:956-969`), as does the resize restart
  (`:830-844`). `bind` skips a view already bound, so a later `set_viewport`
  survives a second count.
- `d3d12/` is the same shape: `set_viewport` at
  `engine/render/d3d12/renderer.cpp:404-416`, and its default in `View::begin`
  (`:322-377`, `RSSetViewports` at `:370`), called with the screen viewport by
  `set_view_count` (`:1292-1302`) and by the resize path (`:622-629`).

The engine already admits that what a drawable puts on screen depends on zoom.
A sprite's destination truncates to whole view pixels
(`engine/render/sprite_geometry.h:57-60`), and `sprite_world_bounds` widens
the cull box by that error at a given units per pixel (`:146-154`). `Visual`
and LineSweeper's particle field override `cull_bounds` for that reason
(`engine/render/visual.h:43-49`, `samples/linesweeper/presentation/particles.h:235`).

**Why.** PHILOSOPHY says the draw list "carries the camera, the viewport and
the filter" (`docs/design/PHILOSOPHY.md:529-531`), and that `draw()` "receives
what it needs as parameters" (`:526-527`). The `DrawList` is that parameter.
A reader keeps both promises: the caller still sets the context once per range
(`engine/core/game_object.h:51-53`), and the drawable may read it. Changing the
virtual to `draw(DrawList&, const DrawContext&)` would break every override,
and passing a context per object per draw is the tax T8 refuses
(`docs/design/PHILOSOPHY.md:139-142`).

A drawable that wants a minimum stroke width, or detail chosen by zoom, needs
at draw time the same number `cull_bounds` already gets. Today it can only get
that number by copying `Scene`'s per-view walk, and `scene.h` says "hand-written
copies of it diverge" (`engine/scene/scene.h:240-243`). The client's copy has
already diverged. That meets The public face's test: the client would have to
write the mechanism itself (`docs/design/PHILOSOPHY.md:792-798`). T1 holds
(`:37-40`). The engine supplies the context, and the client keeps the art
policy: projection, minimum thickness and which views.

The strongest case for declining is "All state changes - animation selection
included - happen in update()" (`docs/design/PHILOSOPHY.md:527-528`), together
with the hook `ViewOverlay` already provides. Neither holds. Choosing a
representation per view writes nothing, so it is not a state change. `update()`
cannot make the choice, because it runs once per object and the choice is per
view. `ViewOverlay` runs after the world and is not culled
(`engine/scene/scene.h:72-75`), so using it for world objects means re-walking
the cull, which is the copy `scene.h:240-243` forbids.

The reader is sound under view parallelism. A backend owes "one independently
writable recording target per view" (`docs/design/PHILOSOPHY.md:572-574`), and
one worker owns a `DrawList`'s `View`. A `const` read of it from inside `draw()`
shares nothing, so the object stays a pure read. On T8, each accessor is one
out-of-line call, because `View` is backend-defined. Only a drawable that asks
pays it, and that drawable already makes an out-of-line `draw_sprite`. It is
not a virtual call or a `std::function`, and the sprite path is unchanged. No
filter reader is asked for, so none is added (T1, `:45-47`). `viewport()` is
included because `Camera::visible_rectangle(viewport)`
(`engine/render/camera.h:50-68`) needs it. A batch drawable, which PHILOSOPHY
endorses ("A paint-tile grid is one `GameObject`",
`docs/design/PHILOSOPHY.md:469-471`), needs it to cull its own contents per
view.

**The change.**

`engine/render/renderer.h`, class `DrawList`, beside the setters (`:129-153`):

```cpp
// The camera the next draw on this list is mapped through: the last
// set_camera on it this frame, else Camera::DEFAULT_CAMERA. Valid only on a
// valid() list. A draw() may read it to choose what it draws for this view,
// and must not store it: the list is gone at the next Renderer::submit().
const Camera& camera() const;

// The pane the next draw lands in, in back-buffer pixels, as last given to
// set_viewport, else the whole back buffer the list starts with. Valid only
// on a valid() list.
const Viewport& viewport() const;
```

Also add a contract sentence near `camera()`. A draw whose drawn extent grows
as the camera zooms out, such as a minimum stroke, must report that growth
through `cull_bounds(units_per_pixel)`, or the cull may reject it at a pane
edge. A readable camera invites level-of-detail drawing outside `bounds()`,
so this sentence is part of the answer, not an extra.

`viewport()` returns the `Viewport` the caller gave, not the pixel rectangle a
backend snaps it to (`engine/render/d3d11/renderer.cpp:94-102` snaps for
Direct3D). One space per term, the same space `set_viewport` takes.

Definitions are one line each in each backend's `renderer.cpp`:
`engine/render/d3d11/`, `d3d12/`, `gl/`, `vulkan/`, `null/`. The function
returns `this->view_->camera` or `this->view_->viewport`. `d3d11/` and
`d3d12/` also gain a `Viewport viewport;` member on their `View`, next to
`camera` (`engine/render/d3d11/backend.h:144`, `engine/render/d3d12/backend.h:223`).
It is written in three places:

- in `DrawList::set_viewport`, as given;
- in `View::bind` (d3d11) and `View::begin` (d3d12) when they actually bind,
  from the screen viewport they are handed. That is
  `Viewport(vp.TopLeftX, vp.TopLeftY, vp.Width, vp.Height)`, using
  `Viewport`'s depth defaults or the D3D viewport's depth fields, whichever
  matches what `gl/` writes. Without this, `viewport()` on a fresh d3d11 or
  d3d12 list returns a default `Viewport` that is not the pane;
- on the resize restart paths listed above, the same way.

`engine/core/game_object.h:48-53`: replace the sentence that is false today
("either hands a camera straight down or calls Camera::calculate_view_rectangle
with it"). The new text says an implementation whose output depends on the
view reads `draw_list.camera()` and `draw_list.viewport()`, and does not store
them. Adding a sentence beside the old one would leave drift that
`docs/review/backend-equivalence-2` would count. The comment keeps its point
that the caller who knows the view says so once per range.

`engine/render/SEAM.md`, §6, "Why the camera is on the `DrawList`" (`:185-190`):
add that the list's camera and viewport are readable, and why a reader rather
than a parameter. Under "What each backend resets in `begin_frame`"
(`:199-215`), add that the camera and the viewport are now observable after a
reset, and what each backend resets them to.

`website/src/content/docs/docs/concepts/drawing.mdx:128-130`, which names
`set_camera`, `set_viewport` and `set_filter`, gains the two readers.

`Camera::DEFAULT_CAMERA` is one of the all-caps constants
`docs/product/STORIES.md:3026-3036` may rename. The new comment names it once,
so that story's rename touches one more line and no more.

The story does not change `Scene`, `GameObject`'s virtual or `ViewOverlay`.
Two alternatives are refused: an overload `GameObject::draw(DrawList&, const DrawContext&)`
(T8, and a source break), and a mutable per-view handoff on the object, which
races under the fan-out (T2, `docs/design/PHILOSOPHY.md:49-55`). Check on all
five backends, plus a `wasm-*` build of `gl/` under clang `-Werror`. Returning
`const&` is clean there. Also check the consumer job. The window is not
involved, so the Win32 and web windows are unaffected.

**Tests.** All in the same commit:

- `tests/render/null_tests.cpp` (in `RenderTests` under `x64-debug-null`,
  `x64-release-null` and CI's wasm null configure). Cases:
  - A fresh list reads `DEFAULT_CAMERA` and the whole buffer.
  - After `set_camera` and `set_viewport` the readers return them, including
    mid-list, and the draws recorded after them agree. A `RecordedSprite`
    stamps the viewport (`engine/render/null/recording.h:81-88`).
  - State read on one view is independent of its neighbour, in the shape of
    "state set on one view does not leak into its neighbour" (`:433`).
  - `begin_frame` resets both.
  - A mid-frame resize restart resets both while a held list stays valid, in
    the shape of "CONTRACT: a resize arriving mid-frame restarts the frame"
    (`:499`).
- `tests/scene/fanout_tests.cpp` (`SceneTests`, null presets only,
  `tests/scene/CMakeLists.txt:22-24`). A probe `GameObject` writes
  `draw_list.camera().scale` and `draw_list.viewport()` into the draw it makes,
  as its tint and destination. With two views on the pool and partitioner,
  each view's recording carries its own camera, and the serial and fan-out
  recordings are identical.
- `tests/render/pixel_tests.cpp` (`RenderPixelTests` under `x64-debug`,
  `x64-debug-d3d12`, `x64-debug-gl`, `x64-debug-vulkan`, and the wasm page in
  headless Chrome, which `RenderPixelTestsWithoutS3tc` also runs). One case
  that reads no pixels and so adds no golden image. It checks that the readers
  round-trip on a real device, before and after a `set_viewport`, and after
  `window_size_changed`. This is where d3d11's and d3d12's new member is held
  to the same answer as the other three.

**Consumer-visible.** No, by CHANGES.md's own definition: "Everything else, a
new public name included, is not, and is not listed here" (`CHANGES.md:12-14`).
Existing callers see no change in behaviour. The commit touches
`engine/**/*.h`, so its message says `not consumer-visible`. The two readers
appear in the API reference, because `renderer.h` and `game_object.h` are
published (`website/src/lib/published.ts:110`, `:139`). No
`samples/minimal/README.md` line.

**What the client could delete.** It can delete its hand-copied per-view cull
walk, which also downcasts to reach its own drawable type and checks its
collision order against the engine's. With it go the borrowed scenery mirror
that walk needs, and the camera parameter it threads into one of its draw
functions. A drawable then reads `draw_list.camera()` in its own `draw()` and
lets `Scene::draw` do the overview pass. If the choice of art is not purely by
scale, the drawable keeps a `const` flag set in `update()`. What stays client
policy: the projection, the minimum stroke, the detail chosen at each zoom and
which views get which. The deletion needs a client integration check. This
answer supplies the mechanism and does not claim the deletion works.

**Open questions.**

- Confirm that d3d11's and d3d12's screen viewport, converted to `Viewport`,
  equals the `Viewport(0, 0, w, h)` that `gl/`, `null/` and `vulkan/` write,
  depth fields included, before "the whole back buffer" goes into `renderer.h`.
  The pixel case will catch a mismatch, and it is cheaper to settle first.
- `Scene::draw`'s first view reuses the final declared renderer view, keeping
  its earlier draws (`engine/scene/scene.h:229-235`). So a list can arrive with
  an earlier caller's camera, but `Scene` sets its own before any object draws
  (`engine/scene/scene.cpp:221-223`). The reader is therefore right for scene
  objects. A `ViewOverlay` that sets the identity camera for a HUD reads the
  identity after that, which is correct and worth a sentence in the overlay
  comment.
- Scene category order and the collision-span order
  (`engine/scene/scene.h:151`, `engine/scene/scene.cpp:125-130`) remain
  separate concerns, and nothing here decides them.

## 22. Re-pointing a sprite or an animation by handle

**Decision.** Accept, narrowed. Add two paired value types,
`RenderResources::SheetFrame` and `RenderResources::SheetStrip`, each made only
by resolving a sheet and one of its elements together. `TextureObject` and
`AnimationObject` take them through new protected setters and constructors, and
the existing name setters are rebuilt on them so that a failed resolve leaves
the object unchanged. Several parts are declined: the raw two-handle overloads,
the frame-handle-only `set_frame`, handle forms on `UiTexture`, and switching
`Registry` and `NameTable` to `std::less<>` with `std::string_view`. This is a
story of its own.

**What the engine has now.** `SpriteSheetObject` keeps a sheet handle, not a
name, and says why: holding the name would mean "a std::map<std::string, ...>
descent per draw, per drawable, from every render worker at once"
(`engine/render/sprite_sheet_object.h:13-16`, member at `:43`). But every way
to change what an object draws takes names:

- `SpriteSheetObject::set_sprite_sheet(const std::string&)`
  (`engine/render/sprite_sheet_object.h:41`, `sprite_sheet_object.cpp:29-33`);
- both `TextureObject::set_frame` overloads
  (`engine/render/texture_object.h:50-52`, `texture_object.cpp:33-43`);
- `AnimationObject::set_animation_strip_and_reset`
  (`engine/render/animation_object.h:45-46`, `animation_object.cpp:140-148`);
- the `TextureObject` and `AnimationObject` constructors
  (`engine/render/texture_object.h:28-35`, `engine/render/animation_object.h:23-30`);
- `UiTexture::set_texture` and `set_sprite_frame`, which forward to
  `set_frame` (`engine/ui/widget.h:164-165`, `engine/ui/widget.cpp:199-207`).

The setters on `TextureObject` and `AnimationObject` are protected
(`engine/render/texture_object.h:37`, `engine/render/animation_object.h:31`).
Only `UiTexture`'s are public. A client reaches them by deriving, which the
client does, so what is asked for is a protected overload, not a public one.
The handles can be resolved publicly (`engine/render/render_resources.h:139-142`,
`engine/render/sprite_sheet.h:44-45`). The members they would go into are
private: `sheet_` (`engine/render/sprite_sheet_object.h:43`), `strip_`
(`engine/render/animation_object.h:62`) and `frame_`
(`engine/render/texture_object.h:80`). Within one sheet a subclass can already
draw any frame by handle, through `draw_with` (`engine/render/texture_object.h:71-77`)
and the protected `sprite_sheet()` (`engine/render/sprite_sheet_object.h:35`).
There is no equivalent for an animation strip, and none across sheets.

The name setters are not atomic. `set_frame(sheet, frame)` moves the sheet,
then resolves the frame, and throws with the sheet already moved.
`texture_object.h:45-49` admits this and relies on the throw being fatal.
`set_animation_strip_and_reset` has the same shape
(`engine/render/animation_object.cpp:143-145`).

A handle cannot carry its table. "The index is into whichever table produced
the handle and is meaningless against any other" (`engine/core/handle.h:12-14`),
and `NameTable::get` checks only that the index is in range
(`engine/core/name_table.h:63-74`). `Registry` and `NameTable` use
`std::map<std::string, int>` with the default comparator and take
`const std::string&` (`engine/core/registry.h:84`, `:117`, `:125`, `:156`;
`engine/core/name_table.h:52`, `:76`, `:83`).

**Why.** The engine's own rule is broken by the only spelling it offers.
Content and assets says "Names resolve to handles once, at load. Per-frame code
touches no string-keyed map (T7, T8)" (`docs/design/PHILOSOPHY.md:630-631`).
Performance says per-frame code performs "no string-keyed lookups" (`:671`).
`Registry` states the same rule (`engine/core/registry.h:35-37`). The client
re-points on every tick while an effect is continuous: it restarts an
animation for each contacted object, and it constructs short-lived animated
objects through the name constructor every tick while one is active. That is
per-frame code. It cannot route around the names, because the members are
private, and the only alternative is re-implementing `AnimationObject` on
`SpriteSheet::draw`. That meets The public face's test
(`docs/design/PHILOSOPHY.md:792-798`), and no count is needed. The case for
declining is real: re-pointing is an event, nobody has measured a cost, and
the draw path already holds handles. But the rule PHILOSOPHY states is about
string-keyed lookups in per-frame code, not about a measured cost, and this
is per-frame code.

The raw two-handle overloads the report proposes are declined. They would
trade a T7/T8 cost for a T2/T6 one. A frame handle from sheet A, passed with
sheet B, either draws the wrong frame silently or throws `out_of_range` far
from the mistake, whenever its index is in range for B. That is the quiet
failure T6 refuses (`docs/design/PHILOSOPHY.md:109-114`). T5 says to push the
error into the type instead (`:95-99`). A value resolved from both names at
once, with private members and `RenderResources` as its only maker, cannot be
constructed mismatched, and costs nothing at run time: it is two ints. Fixing
the name setters falls out of the same type. Resolve the pair first, then
assign, and they become atomic. Under T6 that is a correction.

The frame-handle-only `set_frame(frame_handle)` is declined. It is the unpaired
handle again, `set_frame(SheetFrame)` covers it, and within one sheet
`draw_with` already takes a frame handle (T3, `docs/design/PHILOSOPHY.md:64-67`).
The `UiTexture` forms are declined because menus re-point on input, not every
frame, and the report rates them minor. Mechanisms "generalise only as far as
real clients demand" (`docs/design/PHILOSOPHY.md:45-47`). If a per-frame
`UiTexture` caller appears, the answer is a one-line overload.

`std::less<>` with `std::string_view` is declined too. Once objects hold
handles, every remaining resolve runs at load or wiring time, which is where
T8 places flexibility ("lives at load and wiring time, never in the loop",
`docs/design/PHILOSOPHY.md:151`). Nobody has measured a cost, so changing the
public signatures of `Registry` and `NameTable` buys nothing (T3).

**The change.**

`engine/render/render_resources.h`, inside `RenderResources`, near
`resolve_sprite_sheet` (`:131-142`):

```cpp
// A frame and the sheet it was resolved against, which only
// resolve_sheet_frame can make - so the pair cannot be mismatched. It is
// meaningful only against the RenderResources that resolved it.
// Default-constructed is unresolved, and an object given one throws.
// It survives release_device_resources, because both halves name slots.
class SheetFrame
{
public:
	SheetFrame() = default;
	SpriteSheetHandle sheet() const;
	SpriteSheet::frame_handle frame() const;
	bool operator==(const SheetFrame& other) const = default;
private:
	friend class RenderResources;
	SheetFrame(SpriteSheetHandle sheet, SpriteSheet::frame_handle frame);
	SpriteSheetHandle sheet_;
	SpriteSheet::frame_handle frame_;
};

// The same, for an animation strip: strip() in place of frame().
class SheetStrip { /* ... */ };

// Load-time. Each throws std::out_of_range naming whichever of the two
// names is missing.
SheetFrame resolve_sheet_frame(const std::string& sheet_name,
	const std::string& frame_name) const;
SheetStrip resolve_sheet_strip(const std::string& sheet_name,
	const std::string& strip_name) const;
```

Declare no copy constructor or copy assignment (rule of zero). That avoids the
clang warning about an implicit copy assignment beside a declared copy
constructor that AGENTS.md names. The defaulted `operator==` relies on
`Handle`'s (`engine/core/handle.h:33`). `render_resources.h` already includes
`sprite_sheet.h` (`:7`), so `SpriteSheet::frame_handle` is in scope.

`engine/render/sprite_sheet_object.h`: a protected
`void set_sprite_sheet(RenderResources::SpriteSheetHandle sheet);` under the
same rule as the name form, called only from a setter that moves its element in
the same breath (`:37-41`). Also a protected constructor taking a
`SpriteSheetHandle`, so the subclasses' pair constructors do not resolve a
name.

`engine/render/texture_object.h`:

```cpp
TextureObject(RenderResources::SheetFrame frame,
	RenderResources* render_resources,
	const Colour& color = Colour::white,
	float rotation = 0.0f,
	const mattmath::Vector2F& origin = mattmath::Vector2F::ZERO,
	SpriteFlip flip = SpriteFlip::none,
	float layer_depth = 0.0f);
protected:
void set_frame(RenderResources::SheetFrame frame);
```

`engine/render/animation_object.h`: the same, with
`AnimationObject(RenderResources::SheetStrip strip, RenderResources* render_resources, ...)`
and a protected `void set_animation_strip_and_reset(RenderResources::SheetStrip strip);`.

Both pair setters throw on an unresolved pair at the call, not at the next
draw (T5, T6). Rebuild `set_frame(sheet_name, frame_name)` as
`set_frame(render_resources()->resolve_sheet_frame(sheet_name, frame_name))`,
and `set_animation_strip_and_reset(sheet, strip)` the same way. A bad element
name then throws before anything is assigned. Delete the "It is not atomic"
paragraph (`engine/render/texture_object.h:45-49`) in the same commit, or it
becomes drift. Keep the rest of `:40-43`, which is still true. Add to the class
comments that a pair is meaningful only against the object's own
`RenderResources`. No backend changes, and nothing on the draw path changes.

All four headers are published (`website/src/lib/published.ts:140`, `:150`,
`:151`, `:153`), so the new types need contract comments. The site build fails only on
an unlisted header, not on an uncommented type, so review has to enforce this.
The sprites guide's sentence on `set_animation_strip_and_reset`
(`website/src/content/docs/docs/guides/sprites-and-text.mdx:156`) can name the
pair form. Check on any one backend preset, a `-null` preset (for the device
loss case below) and a `wasm-*` preset (clang `-Werror`, tests under Node).
Nothing else is touched.

**Tests.** In `RenderTests`, in every preset, wasm under Node included, unless
noted. Add to `tests/render/sprite_sheet_object_tests.cpp`,
`tests/render/animation_object_tests.cpp` and, for the resolvers,
`tests/render/render_resources_tests.cpp`:

- `resolve_sheet_frame` and `resolve_sheet_strip` return the same sheet and
  element as resolving each separately.
- Each throws naming the missing sheet, and, separately, naming the missing
  element.
- A default `SheetFrame` or `SheetStrip` given to a setter or constructor
  throws.
- The pair setter gives the same source rectangle, frame size and frame time
  as the name form, and the animation is reset.
- The pair constructors equal the name constructors.
- The name setters are now atomic: a bad element name leaves the sheet, the
  element and the frame index unchanged. This is the case that pins the
  behaviour change.
- A pair resolved before `release_device_resources` draws correctly after the
  reload, in the shape of `tests/render/null_tests.cpp:721`. This one is
  null-preset only.

No engine sample uses `AnimationObject`, so these tests are the only engine
callers of the pair forms. The story's acceptance criteria should name them.
No pixel test is needed, because the draw path is unchanged.

**Consumer-visible.** Yes, a silent behaviour change. A `## Unreleased` entry
in CHANGES.md, headed by the commit's date and subject (`CHANGES.md:30-33`,
`:46-51`). It says that `TextureObject::set_frame(sheet_name, frame_name)`,
`AnimationObject::set_animation_strip_and_reset(sheet, strip)` and, through
them, `UiTexture::set_texture` now leave the object unchanged when the element
name throws, where before they left the sheet moved. A game that caught that
throw and drew on sees the old frame rather than a mismatched one. The new
names are not themselves consumer-visible (`CHANGES.md:12-14`), but the entry
may point to them as the handle path. There is no source break: the name forms
stay. No `samples/minimal/README.md` line, because the template uses neither
class.

**What the client could delete.** It can resolve one `SheetStrip` per kind of
effect when a level is built, hold them in the shared record its animated
objects already read, and pass those to the restart. The gate it added to
avoid restarting the same clip can then go. Its short-lived animated objects
can be built from a `SheetStrip` handed over by whatever spawns them, instead
of names. A character can build a table of `SheetFrame`s and `SheetStrip`s per
animation state in its constructor and share it with the drawables layered on
it, which replaces several name resolves per state change. What stays: menu
icons and menu pages re-pointing `UiTexture` by name, which is declined as
menu-only, and the client's own string-keyed registries.

**Open questions.**

- Placement. Nesting the types in `RenderResources` is defensible: it owns the
  sheet registry and is the only party that can resolve both halves. They
  could equally live beside `SpriteSheet` with `RenderResources` as a friend
  maker. CONVENTIONS' role-noun rule (`docs/design/CONVENTIONS.md:31`) is met
  either way. Settle it at review.
- Cross-table use. A pair resolved from one `RenderResources` and handed to an
  object built on another is still expressible. There is one table per process
  in practice, so the contract sentence covers it. A check would need the pair
  to carry a pointer to its table, compared against the object's
  `render_resources()`, which costs a pointer per pair. Leave it out unless a
  second table appears.
- Replacing a sheet. Adding a sheet twice under one name replaces it
  (`engine/render/render_resources.h:55-56`). A replacement with a different
  frame order invalidates element handles. That is true of the handles objects
  hold today too, so it is not new, but the pair's comment should not promise
  more than "survives a device loss".
- `Visual`'s name constructor (`engine/render/visual.h`) is not asked for and
  is left out (T1).
