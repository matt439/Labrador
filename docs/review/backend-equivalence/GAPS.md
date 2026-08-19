# What this audit missed, capped off, and got wrong

> Part of the [backend equivalence audit](README.md). Read-only, 2026-08-19,
> against the tree at `57b65b3`. Not updated as findings are fixed.

Written by the audit's own completeness critic, plus one correction found by hand
afterwards. It is the most useful document here for whoever runs the next pass,
because it says where to start.

---

## What the audit got wrong

**It never opened `WM_EXITSIZEMOVE`.** Six separate write-ups argued defect A —
the GL backend flipping every viewport against a stale cached height — and all
six analysed the renderer and the `WM_SIZE` suppression without checking whether
anything corrects the cache when the drag ends. Something does:
`engine/app/window.cpp:408-417` calls `GetClientRect` and forwards the real
client size on `WM_EXITSIZEMOVE`. The displacement is therefore **transient**,
lasting the drag and self-correcting on mouse-release, not persistent. The
finding survives — every frame of the drag is wrong, and the restore-from-minimise
trigger is genuinely persistent — but one severity grade of it did not.

The lesson generalises: the audit was decomposed by *render axis*, so every agent
that touched defect A entered through `engine/render/gl/` and reasoned outward.
None was asked to read `engine/app/window.cpp` end to end. A decomposition by
subsystem has a blind spot exactly at the subsystem boundary, and this is what it
looks like.

## What was capped off

Verification ran at most two candidates per axis, ranked by the hunting agent's
own severity. **16 high-or-medium candidates were raised and never verified:**

| Axis | Unverified |
|---|---|
| `factory-split` | 3 |
| `seam-hygiene` | 3 |
| `error-parity` | 2 |
| `pixel-logic-leak` | 2 |
| `format-mapping`, `upload-mechanics`, `batch-breaks`, `resize`, `gl-entrypoints`, `threading` | 1 each |

Given that 61 of the 71 candidates that *were* verified died, the prior on these
is that most are inert too — but they are unexamined, not cleared, and the two
axes with three apiece are the two where the hunting agents were most confident.

## The method's own limits

- **No agent ran anything.** Every claim is from reading. Where the audit says
  two backends agree bit-for-bit, that is an argument about source, not a
  measurement of two frame buffers. The golden-image proposal in
  [TEST-GAP.md](TEST-GAP.md) exists because nothing short of it converts these
  arguments into evidence.
- **The axes were chosen before the reading.** They came from the seam's own
  documented surface, so an invariant the seam never names had no agent assigned
  to it. Items 1, 2 and 4 below are all of that shape.
- **Twelve `engine/render/` translation units were never opened by any axis**, and
  the object layer that every client actually draws through is among them —
  item 8.

## Where the next pass should start

Roughly in order of how likely a real divergence is.

1. **The rasteriser fill rule against deliberately fractional edges.** `depth-raster` enumerated every state field and `index-topology` confirmed the winding, but nothing asked *which pixels a triangle covers*. `build_sprite_quad` truncates every edge to an integer (`sprite_geometry.cpp:128-131`) so an edge never crosses a pixel centre — but `build_scaled_quad` truncates nothing, so **every glyph quad has fractional edges**, and it is already reachable: `sprite_font_file_tests.cpp:93` pins `line_spacing == 24.1667f`, so `pixel_tests.cpp:708`'s `L"A\nA"` puts a second-line top edge at `y + 24.1667`. D3D11 mandates the top-left rule exactly; GL 3.3 §3.5.1 requires only that a shared edge be neither double-covered nor missed, leaving the tie-break implementation-dependent. Rotation makes it worse and reaches no device in any test. This is one of two places where a genuine d3d11-vs-gl pixel difference is plausible, because it turns on API behaviour *specified differently* rather than on code the audit could diff.
2. **Blend-result clamping, and GL never learning its framebuffer format.** `Colour` clamps per channel independently (`colour.cpp:163-168`), so a legal tint of `(1,1,1,0.5)` gives `src.rgb > src.a` and `src + dst*(1-a)` exceeds 1.0 over any non-black destination — every scrim and HUD panel. D3D11 clamps because the target is a named `DXGI_FORMAT_B8G8R8A8_UNORM`; GL asks `ChoosePixelFormat` for `cColorBits=32, cAlphaBits=8` (`gl/renderer.cpp:280-291`) and **never calls `DescribePixelFormat` to learn what it got**. The second plausible place for a real difference.
3. **`measure_text`, `can_render`, `first_unrenderable` and the whole public `RenderResources` surface are implemented three times inside the backend wall** (`d3d11/render_resources.cpp:139`, `gl:142`, `null:128`). `measure_text` is a pinned contract term (`pixel_tests.cpp:574`) and `pixel-logic-leak` missed the file entirely. They agree today; nothing structural holds them together and each preset compiles only its own copy.
4. **`engine/render/sprite_sheet.cpp:19-24`** — `destination_from` is `build_scaled_quad`'s size arithmetic written a second time, producing a `RectangleF` that then goes through `build_sprite_quad`, which **truncates**. So a sprite at position+scale truncates and a glyph at the same position and scale does not. The contract has a term for the second and none for the first. No axis opened the file, there is no `sprite_sheet_tests.cpp`, and this is the overload the entire object layer draws through.
5. **`engine/render/text_encoding.cpp:3` includes `<Windows.h>`** and is in the *unconditional* source list (`engine/CMakeLists.txt:56`) — compiled into the null configuration, whose stated point is having no graphics API. Sharper: `widen()` is the only producer of surrogate pairs in the tree and `font.h:152-155` walks per UTF-16 code unit with a bare `static_cast<char32_t>`, drawing two stand-ins. That crossing was never run.
6. **Where a `Viewport` comes from.** All six defect-A write-ups analysed the renderer's cached size; none asked the producer. `ViewportManager::fullscreen_viewport()` sizes from `resolution_manager_->resolution_vec()`, **not** from `Renderer::back_buffer_size()` — two independent caches of "the size", kept in step only because `Application::on_window_size_changed` happens to write both (`application.cpp:407-410`). The startup path is the untested one: if the real client area ever differs from the preset without a `WM_SIZE` correcting it — fullscreen-at-startup is the obvious candidate — GL is displaced *permanently*.
7. **Dead and unfinished code inside `engine/render/`.** `draw_object.cpp:68-71` — `set_draw_rotation_by_rectangle_rotated` is a public setter with an empty body and a `// TODO`, silently discarding its argument. `sprite_frame.h:53-55` stores `origin_` and `rotated_` with **no accessor for either**, so an atlas frame packed rotated 90° draws unrotated. `rotation_origin.h` has zero users anywhere.
8. **Twelve `.cpp` under `engine/render/` with neither an axis nor a test file**: `animation_object`, `animation_strip`, `draw_object`, `label`, `sprite_frame`, `sprite_sheet`, `sprite_sheet_object`, `text`, `text_drop_shadow`, `text_object`, `texture_object`, `visual`. Three carry pixel arithmetic that reaches the seam — item 4 above, `text_drop_shadow.cpp:83-88` (two `draw_text` calls at a separate offset and scale, a relationship no contract term describes), and `texture_object.cpp:45-70` (routes every object draw through both `SpriteSheet::draw` overloads, i.e. both rounding behaviours).
