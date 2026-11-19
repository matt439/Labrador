# Best-effort keyboard labels — 2026-11-17

OCA-111 implementation and focused qualification on Windows 11. The source
base is OCA-110 `77399933`, whose parents contain OCA-109, plus the current
ISO-position interface from `75131afd` (local dependency `ae4cdbda`). The
commit containing this record adds labels without changing key ordinals,
stable tokens or the serialized `GalleryBindings` table. `receipt.json`
records exact build-input/output/image identities and browser-run commands.

The combined seven-track input/app sweep and whole-head lint remain owned by
the integration branch. OCA-111 stays open until those required checks pass.
This record does not qualify a physical non-US keyboard, keycap legends,
every IME profile, Safari or a complete current browser layout map.

## Contract and platform routes

`Keyboard::label(Key)` optionally returns a borrowed UTF-8 view, valid until
the next input poll or destruction. Shell feeds copy into staged storage;
`poll()` publishes the final map and advances `label_revision()` once only
when its visible contents differ. Unseen/unavailable/invalid keys have no
label. Font selection, conversion, width and stable-token fallback remain
page policy, independent of the input query and binding identity.

Native labels use the calling thread's current HKL, reversed through the
existing physical scan translator. Character translation uses a neutral
256-byte state and `ToUnicodeEx` flag 4, within the existing Windows 10
version 1607 floor. Named controls use `GetKeyNameTextW`, with aggregate
modifier names. Queries load or activate no layout and make no clearing
translation after a dead-key result. A dead, failed, invalid or ambiguous
character result is unavailable. A pending platform dead-key buffer can
still influence a result; this is best effort rather than a keycap promise.
The native window refreshes at first use, differing HKL, activation and
`WM_INPUTLANGCHANGE`, preserving default child-message propagation. Not
every IME profile change provides that notification.

The browser route requests neither `getLayoutMap` nor Keyboard Lock. It
records only last-observed single Unicode scalar labels from the focused
canvas's own key events. Shift/Ctrl/Alt/Meta/CapsLock, repeats, composition,
ASCII space, C0/C1 controls, line separators, named/dead keys, malformed
UTF-16, ambiguous text and unknown code positions provide no new label.
Canvas blur and hiding discard observations; a different label for an already observed position
discards the older cache before retaining the new fact. An optional
`layoutchange` event invalidates only where an EventTarget is actually
exposed. Current Chromium provides no such notification, so unseen or
undetected layout changes retain the explicit unavailable/last-observed
limits. No layout family is guessed and no binding is remapped.

Primary platform references: [ToUnicodeEx](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-tounicodeex),
[MapVirtualKeyExW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-mapvirtualkeyexw),
[GetKeyNameTextW](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getkeynametextw),
[WM_INPUTLANGCHANGE](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-inputlangchange),
and the [Chromium Keyboard interface](https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/blink/renderer/modules/keyboard/keyboard.idl).

## Focused automated checks

MSVC 14.51.36231, CMake 4.3.1-msvc1 and the installed native vcpkg dependencies
built the two native configurations. Emscripten 6.0.12 built wasm Debug with
the standalone GL test tools from `VCPKG_MANIFEST_FEATURES=materials`.
CTest's configured Node was 24.14.0. Commands from the isolated worktree,
after vcvars/emsdk setup as appropriate:

```bat
cmake --preset x64-debug-null
cmake --build --preset x64-debug-null --target InputTests AppTests ComponentsTests ComponentsGallery --parallel 2
ctest --test-dir out/build/x64-debug-null -R "^(InputTests|AppTests|ComponentsTests)$" -V
cmake --preset x64-debug
cmake --build --preset x64-debug --target InputTests AppTests ComponentsTests ComponentsGallery --parallel 2
ctest --test-dir out/build/x64-debug -R "^(InputTests|AppTests|ComponentsTests)$" -V
cmake --preset wasm-debug -DVCPKG_MANIFEST_FEATURES=materials
cmake --build --preset wasm-debug --target InputTests AppTests ComponentsTests ComponentsGallery WebWindowTests --parallel 2
ctest --test-dir out/build/wasm-debug -R "^(InputTests|AppTests|ComponentsTests)$" -V
```

All three entries passed in each configuration:

| Configuration | Input cases/assertions | App cases/assertions | Component cases/assertions |
| --- | --- | --- | --- |
| Native null Debug | 123 / 13,620 | 130 / 2,134 | 97 / 1,834 |
| Native D3D11 Debug | 123 / 13,620 | 114 / 1,637 | 78 / 1,485 |
| WebGL2 wasm Debug under Node | 134 / 13,661 | 39 / 520 | 78 / 1,485 |

Eight input cases cover staged publication, copied buffers/view lifetime,
unknown values, UTF-8 facts, delta preservation, invalidation/coalescing,
revision changes and independent focus/key/token state. Nine SDK-boundary
cases cover the physical reverse mapping/ISO position, canonical aliases,
neutral state, flag 4, wide handle identity, dead/error/ambiguous/malformed
UTF-16 and current-HKL smoke without keyboard-state/layout changes. A real
hidden native Window case refreshes a replaced fixture label using the
existing HKL in language/activation messages, without loading a layout.

The portable browser translator filters unsupported facts. Six copied helper
cases exercise `widen`/round-trip validity, renderable accents, font/stand-in
refusal, width/conversion bounds and a required usable token fallback. The
native-null real GalleryState case keeps an R binding while displaying Q,
falls back from an available accent absent from the delivered ASCII font,
and refreshes on invalidation. These native input snapshots and SDK stubs
are explicitly synthetic; the existing modal capture/text regressions pass.

## Browser and visible caller proof

Desktop Chrome 157.0.8103.78 reported ANGLE on NVIDIA GeForce RTX 5080 through
D3D11. The final five routes used the read-only root page runner from
`59634ce46e7efd054110c150acf6a497ca8a5c40`. Each command captured hashes for
the runner and its imported local modules before and after execution; all
were unchanged and every exit was zero. The runner is a qualification
dependency, not copied into this story's source diff. The receipt retains
all argument arrays. The main command was:

```bat
node C:\Projects\Labrador\tests\render\web\run_page.mjs --browser "C:\Program Files\Google\Chrome\Application\chrome.exe" --page out/build/wasm-debug/samples/components/ComponentsGallery.html --input samples/components/web/rebinding_input.mjs --timeout 60 --browser-warnings fail
```

The ordinary route checks label/token separation, physical R continuing to
restart despite a Q label, ASCII-font fallback for an injected accent,
modified/composing/dead/ambiguous/malformed/unknown fact refusal, focus
invalidation and fresh real canvas key events. Guarded map/lock methods
remain uncalled. Explicit DOM code/character pairs are synthetic layout
evidence, not a change to the host layout. A second run adds
`--query layout-events=1` to install an isolated EventTarget fixture before
the engine starts and exercise optional notification/invalidation. It does
not claim Chromium natively exposes that event. The existing text/pointer
drivers also passed unchanged. `WebWindowTests` passed all 55 steps using
its normal warning policy, including the intentionally refused fullscreen
request and expected failure diagnostic; strict sample routes reject
browser warnings.

Computer Use inspected the owned native D3D11 gallery. Its active-layout
query produced the separate `Label: r / token r (layout)` line; helpers,
label information, action status and Name's column fit. Original Quit
closed the owned window. Native label lookup was not replaced for this
visible check. No system layout or normal browser/profile setting changed.

| Image | Observed result |
| --- | --- |
| [native-label.png](native-label.png) | Actual native caller displays an available label separately from token `r`; the added information line fits. |
| [browser-font-fallback.png](browser-font-fallback.png) | An injected available accent remains a layout fact while the ASCII gallery displays `r`, explicitly marked `font fallback`. |

Images are unedited captures: native per-window 1282×752 and browser page
764×485. They are appearance evidence, not golden comparisons or physical
layout acceptance. The owned native window and disposable browser profiles
were closed after observation.

The website gate passed with 82 checked files and no diagnostics, 200 Node
tests, 181 built pages and valid internal links; the final build also passed.
The isolated site used its own `npm ci`, borrowing only the already
integrated OCA-93 `GuideVersionNote.astro` dependency (`95108c33`), omitted
from this change. Standalone lint steps 3–11 passed; per-commit declarations
are checked on this commit. At the integration owner's request, no second
full Python discovery was run alongside its checkpoint lint. Whole-head
lint and the combined seven-track sweep remain open acceptance.
