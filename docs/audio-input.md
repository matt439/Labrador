# Owned PCM audio input

`wave` manifest entries load `<directory><name>.wav` into a one-wave
SoundBank. The wave and persistent effect share the asset name. Required
errors stop loading before that AudioResources name is published; optional
missing, unreadable or malformed audio containers report a diagnostic and
substitute `SoundBank::silent()`. C++ allocation failures and invalid
programmatic data propagate. Existing `sound_bank` entries retain their XWB
definition/container route.

## Portable data and WAV profile

`engine/audio/wave_data.h` provides the existing `WaveData`, `WaveBankData`
and `WaveEncoding` names independently of a container. `xwb_file.h` continues
to include those types. A wave owns interleaved signed 16-bit samples;
`encoding` records source provenance rather than changing that representation.

The new portable input route accepts mono/stereo at 8000–96000 Hz, with
nonempty whole frames and at most 2 GiB decoded PCM per wave. That payload cap
matches XAudio2's submitted-source-buffer limit, and is checked before
publication or WAV expansion. Frame counts fit the signed browser bridge.
Bank/wave names are nonempty and NUL-free, and wave names are unique in a bank.
These are format/API limits; they impose no total device memory budget.

`read_wav_file` reads little-endian RIFF PCM8/PCM16, converting unsigned PCM8
with `(sample - 128) * 256`. It checks complete RIFF/chunk extents, WORD
padding, format/block alignment, byte rate and whole frames. Unknown metadata
chunks are skipped. Duplicate fmt/data/smpl chunks, empty data, float,
24-bit, extensible or compressed encodings fail with the path and reason.
The result has no resolution name because a WAV stores none; its caller
sets the WaveData name before opening a bank.

An optional `smpl` chunk supplies one forward integral infinite loop. Its
inclusive end becomes a frame count. A non-looping voice plays the complete
wave; a looping voice plays from the beginning, then repeats the stored
region. A zero loop length means the complete wave and ignores loop_start.
The old XWB reader/backend paths keep their existing formats and rate ranges.

## Ownership and cost

`AudioDevice::open_memory_bank(WaveBankData)` takes by value. An lvalue is
copied; `std::move` transfers its containers. On successful return the caller
can destroy or reuse its containers. No backend borrows their storage:

| Backend | Retained playback storage | Nominal sample payload |
|---|---|---|
| XAudio2 | Toolkit SoundEffects own one format/PCM allocation per wave | 2 bytes/sample plus format |
| Web Audio | Native AudioBuffers own float channel arrays before activation | 4 bytes/sample |
| null | Owned WaveBankData, including its vector capacity | 2 bytes/sample plus unused capacity |

XAudio2/Web Audio conversion temporarily holds the input's 2-byte samples
alongside the destination. An lvalue additionally retains the caller's copy;
reading a WAV also holds the source file while decoding. Browser/device
implementation overhead and resampling storage are not measured by these
payload figures. Banks and voices live with the owning device implementation;
destroying or replacing it invalidates their handles. SoundBank continues
borrowing the AudioDevice object it was constructed with. Replacing an
AudioResources bank name does not free or stop the old device-owned voices;
stop its effects first.

Null recordings expose the owned samples and loop metadata without a mixer or
clock. Web Audio copies every interleaved sample into its native channel
storage; XAudio2 hands an owning allocation to the real Toolkit constructor.
The bank tables publish only after validation and backend preparation.
AudioBuffer refusals preserve their browser reason in a named runtime_error;
C++ allocation failures propagate as bad_alloc.

## Verification boundaries

Reader fixtures compare the sample WAVs byte-for-sample with the PCM in the
Microsoft-packed XWB and test malformed prefixes, chunks, format arithmetic,
padding, loop geometry and size limits. A count-only boundary case proves the
2 GiB cap without allocating a large payload. Null and Node cases assert
owned samples after caller release, bank/wave order, levels, loop metadata,
voice lifecycle, suspension and failed-publication retry.

The audio sample retains its XWB controls and adds A for the direct WAV.
Its native finite smoke also creates a nontrivial owned PCM loop region,
releases caller containers before playback, and checks play/pause/resume/stop
on both memory and legacy routes. The copied-starter WAV fixture applies the
first-sound guide's actual C++ blocks and copy/embed/install changes; it adds
loading/refusal tests without changing the distributed starter defaults.

The real-browser driver and exact command are in
[the Web Audio qualification README](../tests/audio/webaudio/README.md).
It forwards native audio methods and verifies the WAV AudioBuffer's identity,
all 3,528 samples, output-graph connection, levels, running context and trusted
natural end, followed by genuine quit. It provides backend/processing evidence.
Listening, physical speaker quality and endpoint-loss/recovery acceptance stay
with WEB#3 and REV#8. Compressed codecs and streamed music remain separate work.

## Local qualification, 2026-11-17

This story was built in an isolated checkout based on the combined source at
`75131afd3aab93a1e471fd3f1354a6e6956725dd`, with root's optimized stack-link
repair `7765cf57` applied as validation dependency `92fb157d`. The final source
was checked with MSVC 19.51.36256.0 and Emscripten 6.0.12; compiler builds used parallelism
2 and CTest ran serially. No dependency was added or updated.

Full builds and exact inventory checks passed before these complete suites,
without test filters or exclusions:

| Configuration | Result |
|---|---|
| `x64-debug` | 22/22 |
| `x64-debug-null` | 23/23 |
| `wasm-debug`, Chromium and Firefox | 45/45 |
| `wasm-release-null` override | 31/31 |

These include the shared reader/data fixtures, unchanged legacy bank tests,
browser sample pages and the optimized stack-dependency probe. The native
endpoint smoke then exited 0 through both WAV/memory
and XWB lifecycle routes, including the caller-storage-release region check.
It used the real XAudio2 implementation; listening and audible region
boundaries were not assessed.

The fresh copied starter first passed its unchanged two native tests. After
applying the WAV guide fixture, all three native Debug tests, all three
native Release tests and both browser Node tests passed. The Release install
included the byte-identical WAV, and its installed hidden startup smoke
exited 0. The installed WAV's complete-file SHA-256 was
`4805e11da74174cea5e4f251014b3f6f49088f87e830f9e2434f81d15af2c59e`.
Its actual Chrome page accepted Space, Escape and Enter and
called `Module.onQuit` without page complaints. The Web Audio sample's
separate native-graph playback result is recorded in
[its qualification README](../tests/audio/webaudio/README.md).

Local lint passed all 13 steps, including 492 Python tooling tests with one
existing skip. The website passed its type check, 201 tests and 189-page build
with valid internal links.

Raw configure/build/test, endpoint, consumer and browser reports are retained
under `out/wav-qualification/` in the validation checkout. These local results
do not establish hosted CI, Safari, physical speaker/listening or device-loss
acceptance. No remote workflow or publication was performed.
