# Seeded-random starter guide, 2026-11-17

OCA-98 teaches the existing `Random` API through
`website/src/content/docs/docs/guides/your-random.mdx`. The game state owns
the generator, consumes it only on a Space press and passes its integer roll
to `points_for_roll` in the rules library. Three rules tests supply their
values directly and add six assertions, including invalid-input refusal.
`MyGameRules` retains its original LabradorMath/settings dependency boundary.

## Inputs and version applicability

- The released `labrador-starter-v0.3.0.zip` is 9,265,469 bytes, SHA-256
  `25db98b280e50648831ca896dad5bbd7b51788a8051dd29089ce3faa410f5869`.
- The development ZIP was made from `5cff44b9` with the guide's working
  documentation changes: 10,836,599 bytes, SHA-256
  `3f4c0ef758f96a0493fb939894a0bdc3294d0be0a485e1d16a9795b32c0b2945`.
- `Random` was added in `37ad369e`; its public header exists unchanged in
  `v0.3.0`. Its current implementation differs from that tag only in a
  rationale comment. The guide requires v0.3.0 or the tested development API;
  it claims no unreleased requirement and does not change the generator.

Each ZIP was extracted into its own fresh short-path consumer project,
`C:\dev\rg98-release` and `C:\dev\rg98-dev`. The three new files were read
from the guide's titled code blocks. Its CMake listings, include/member
edits and Space-press block were applied as written. The default game and
rules targets compiled from those consumer-relative paths. Then the optional
clock constructor was copied from its own guide block and compiled and run
in both projects. The guide explicitly adds `<chrono>` and `<cstdint>` to the
source; the released template does not have the development header's includes.

## Results

| Starter | Fixed seed, native | Fixed seed, wasm/Node | Clock seed, native | Clock seed, wasm/Node |
| --- | --- | --- | --- | --- |
| Released v0.3.0 | 2/2 | 1/1 | 2/2 | 1/1 |
| Development at the recorded base | 2/2 | 1/1 | 2/2 | 1/1 |

Native uses `x64-debug`'s default D3D11/XAudio2 configuration and includes
the hidden game's smoke test. Wasm uses `wasm-debug` with WebGL2/Web Audio;
its rules program runs under Node. The standalone rules programs report
5 cases/15 assertions in the release project and 6/26 in development,
all passing. The guide states only its additional three cases/six assertions,
because existing counts differ by starter version.

All four browser pages (both inputs and seed choices) also passed the
repository's page runner in Chrome 157.0.8103.78 with
`--quit-keys Space,Space,Space,Space,Escape,Enter --frames 60 --timeout 60`.
The runner observed quit without page/console errors and named ANGLE D3D11
on the NVIDIA RTX 5080. This run did not compare browser pixels or assert a
clock seed's value. Audible output and physical input are outside this guide.

A supplemental native-null probe was added only to each disposable consumer
after the ordinary guide builds. It drives the real `HelloState` through
keyboard polling and StateContext updates, then compares its recorded glyph
texture coordinates with the expected strings. Both inputs report:

```text
PASS actual HelloState Space rolls 2,2,1,6 and scores 2,2,1,10; held Space keeps the text
```

The probe uses the null render/audio overrides and performs no pixel or
physical-key measurement. It checks the fixed-seed display path and held-key
semantics; the clock variant intentionally has no fixed output sequence.
This supplemental fixture and the edit driver remain local verification
artifacts, rather than a recurring fourth example in SM-84's selected pilot.

## Evidence and limits

The [receipt](2026-11-17-random-guide.json) retains the guide's C++ snippet
hashes, source ZIP hashes and all configure/build/test/page/probe command
arguments and exit codes. Raw logs and the one-off edit/probe sources remain
under the isolated worktree's `out/random-guide/`. Builds reused the existing
native and wasm installed vcpkg packages with manifest installation disabled;
this is source/consumer qualification, not a clean-machine dependency install.

The optional clock seed is chosen once by game code, may repeat and provides
no unpredictability or uniqueness guarantee. The guide states draw-order
requirements and the portability distinction between specified standard
engines and implementation-selected distributions. It selects no shuffle,
stream derivation, replay format, generator move or cryptographic use.

The final website gate passes its type check, all 200 tests and a 181-page
build with valid internal links. The standalone CMake citation and source
whitespace checks pass. No engine source, build default or dependency changed.
