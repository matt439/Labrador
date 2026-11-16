# Desktop text field — 2026-11-17

OCA-109 qualification on Windows 11, from detached source base `5cff44b9` plus
the text-field changes in the commit containing this record. The adjacent
`receipt.json` records the source, font, build-contract, output and image
SHA-256 identities. This is a desktop editing example with game-owned focus
and bindings. It adds no renderer text API or engine action map.

## Checks

The isolated worktree was `C:\Projects\Labrador-agent-textfield`. Visual Studio
2026 Community's x64 toolchain, CMake 4.3.1-msvc1 and the installed native
vcpkg dependencies built `ComponentsGallery` and `ComponentsTests` under
`x64-debug`; `ComponentsTests` passed. Under `x64-debug-null`, the same suite
passed 70 cases / 817 assertions, including the real GalleryState input and
null-render fan-out/caret cases. These hidden-window cases use synthetic
input; they are not physical-keyboard measurements.

Emscripten 6.0.12 and its Node 24.19.0 built `ComponentsGallery` and
`ComponentsTests` under `wasm-debug`; the portable component suite passed.
Desktop Chrome 157.0.8103.78 ran both the new text route and the unchanged
pointer route successfully, reporting ANGLE on NVIDIA GeForce RTX 5080 through
D3D11. The text route uses canvas keydown/up events separated by rendered
frames. It checks R as text, end backspace, missing-glyph refusal, Enter save,
Escape cancellation without quit, opener isolation, canvas blur cancellation,
unfocused input, an outside press, 12-character refusal, and quit afterwards.

The browser command was, from the repository root:

```bat
C:\Tools\emsdk\node\24.19.0_64bit\node.exe C:\Projects\Labrador\tests\render\web\run_page.mjs --browser "C:\Program Files\Google\Chrome\Application\chrome.exe" --page C:\Projects\Labrador-agent-textfield\out\build\wasm-debug\samples\components\ComponentsGallery.html --input C:\Projects\Labrador-agent-textfield\samples\components\web\text_field_input.mjs --timeout 60
```

For the unchanged pointer route, substitute `pointer_input.mjs`. The runner
was the read-only current root version, whose hash is in the receipt. Its
foreground-tab and isolated-profile native-occlusion fixes are validation
dependencies; the older runner at the detached base can stall callbacks on
this host. The story does not copy those unrelated fixes. The profiles were
temporary runner profiles, not the owner's normal Chrome profile.

The website gate passed: 81 checked files with zero errors/warnings/hints,
200 tests, 180 generated pages and valid internal links. Compiler-free lint
and the offline tooling suites are checked by `tools/lint_local.py` for the
containing commit; the source files are explicitly enumerated in CMake.

## Visible appearance

Computer Use inspected the owned native D3D11 gallery window and a separately
profiled Chrome for Testing 151.0.7922.34 window serving the same wasm build
from a loopback-only HTTP server. The portable browser executable was a
byte-identical renamed copy already prepared for OCA-101; no browser extension
or normal browser profile was needed here. All owned windows and the server
were closed after observation.

| Image | Observed result |
| --- | --- |
| [native-edit-limit.png](native-edit-limit.png) | A bounded draft, visible end caret and 12-character refusal; helper/status text fits inside the panel. |
| [native-cancelled.png](native-cancelled.png) | An outside press restored `Player`, removed the caret and showed `Name kept: Player`. |
| [browser-edit.png](browser-edit.png) | Typed R appears in the draft with the end caret; the help/status lines fit at the browser's scaled canvas size. |
| [browser-cancelled.png](browser-cancelled.png) | An outside press restored `Player` and showed the cancellation status. |

The native window has a 1280×720 client area; the browser window snapshot is
1570×1093 and contains only the isolated local test page and its browser
chrome. The screenshots are unedited per-window PNG snapshots, not golden
comparisons, scan-out/pacing measurements or a claim about every display.
Very short native Computer Use command-key pulses were not reliably sampled
by the existing polled keyboard; native command behavior is established by
the separately polled GalleryState cases, not by those pulses. No key-edge
semantics were changed for this example.

The delivered font measured `Playera12345|` at 167 font texels. The field uses
a 180-texel budget; the 0.7-scale right-column details have a 19.712-unit line
height and fit inside their 260-unit column. The original five row positions
remain unchanged. These layout decisions are specific to this gallery/font.

## Scope

The component keeps valid UTF-8, converts and measures on edits, and draws
cached wide text. It refuses malformed UTF-8, controls and line separators,
non-BMP characters, missing real glyphs and length/width overflow. The gallery
font is ASCII-only and says so on screen. Portable fixtures additionally
exercise two- and three-byte BMP deletion and actual glyph coverage despite a
fallback glyph. Code points are not graphemes; this adds no selection,
clipboard, shaping, bidi, combining placement, IME-composition API or mobile
soft-keyboard promise. No phone, physical controller or touchscreen was used.
