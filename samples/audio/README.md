# Audio sample

A sound board with XWB one-shots, a separate direct-WAV ping and one persistent
looping voice. It loads required audio through the manifest, resolves names once, and shows play,
pause, resume, immediate stop, volume, pitch and stereo pan. The original PCM
content and its ready-to-play XWB container are checked in; a fresh clone needs
no audio authoring tools.

Build and run from the repository root in the x64 Native Tools Command Prompt.
In a starter project the repository root is `external\labrador`, and
[Run the samples](https://labradorengine.com/docs/guides/samples/) has every
step with its time, output and failures:

```bat
cmake --preset x64-debug
cmake --build --preset x64-debug --target AudioSample AudioSampleTests
ctest --preset x64-debug -R "^AudioSampleTests$" --output-on-failure
out\build\x64-debug\samples\audio\AudioSample.exe
```

| Control | Action |
| --- | --- |
| 1 / 2 | Play ping / ascending chime; successive one-shots may overlap. |
| A | Play the same ping from a standalone PCM WAV, without XWBTool. |
| 3 | Start the drone loop. |
| Space | Pause or resume the drone; a stopped drone stays stopped. |
| S | Stop the drone immediately. |
| Up / Down | Raise / lower volume. |
| Left / Right | Move the sound between stereo channels. |
| Q / E | Lower / raise pitch. |
| Escape | Quit. |

The scene fits a logical 1280 by 720 sound board to the current window or
canvas, keeping its controls, status and backend description in view on resize.

The displayed values affect the persistent voice immediately and the next
XWB or WAV one-shot at launch. A one-shot has no retained handle, so it cannot be adjusted
afterward. Losing focus pauses a playing loop and returning resumes it; a loop
the player paused remains paused. Leaving the state stops the loop while its
borrowed bank is still alive.

The normal Windows presets use XAudio2 and need an active Windows audio output
to play, the browser build uses Web Audio (below), and the `-null` presets
record commands without sound; the sample labels that mode
on screen. `SoundBank::audible()` means loaded content, and is also true under
null audio. It is not evidence that speakers produced sound.

## Finite playback check

```bat
start "" /wait out\build\x64-debug\samples\audio\AudioSample.exe --smoke-test
if %errorlevel% neq 0 echo Audio sample smoke failed
```

This check needs no graphics device or window. It loads the packaged manifest's
required XWB bank and WAV asset, plays all three one-shots, exercises the WAV's
auto-created retained effect, and starts the XWB looping voice. Both retained
effects are checked through playing, paused, resumed and stopped states. It
also opens an owned PCM bank with a 128-frame region beginning at frame 32,
destroys the caller's containers, then plays, pauses, resumes and stops that
loop through the same backend. It exits in about a second. A
missing bank or inactive audio endpoint fails the XAudio2 check rather than
accepting silence. With null audio it checks the same lifecycle and explicitly
reports no playback; it does not validate the XWB container. The `-null`
presets register that run as `AudioSmokeTests`; the XAudio2 run needs an
active endpoint and stays a check made by hand.

## In a browser

The `wasm-debug` and `wasm-release` presets build the same sound board as a
page, `AudioSample.html`, beside its `.js` and `.wasm`, playing through the
engine's Web Audio backend. `main_web.cpp` is the browser's entry point and
`web/shell.html` the development page; the manifest, the bank, WAV and font are
embedded in the module at the paths the Windows build copies them to. In a
shell where vcvars and `emsdk_env.bat` have both run, from the engine's folder
([Install Emscripten](https://labradorengine.com/docs/guides/browser/#install-emscripten)
sets it up, and
[Run the samples](https://labradorengine.com/docs/guides/samples/#in-a-browser)
says what each step prints):

```bat
cmake --preset wasm-debug
cmake --build --preset wasm-debug --target AudioSample AudioSampleTests
ctest --preset wasm-debug -R "^AudioSampleTests$" --output-on-failure
start "Sample server" "%EMSDK_PYTHON%" -m http.server 8000 --bind 127.0.0.1 --directory out\build\wasm-debug\samples\audio
```

Then open `http://127.0.0.1:8000/AudioSample.html` from that server, because
a browser will not compile a module read from `file://`, and reload it with
Ctrl+F5 after a rebuild. The controls are the table above.
**Nothing sounds until the page receives a key press, click or touch that
grants user activation; Escape does not count.** The backend unlocks audio
inside that event, and the key that does it is still read by the sample, so a
first press of 1 plays the ping. A `SoundBank::play_wave` one-shot requested
before the gesture is dropped and never replayed. An effect instance requested
through `play_effect`, including a loop, reports playing but waits to start
until audio is allowed; pausing it or stopping it immediately with
`stop_effect(effect, true)` cancels the pending start.
Escape ends the module and says so under the canvas.

There is no `--smoke-test` in a browser, because a page has no command line.
Its lifecycle is an `AudioSampleTests` case in this build instead, run under
Node: the staged manifest's required XWB and WAV loaded through the Web Audio
backend, one-shots, and retained effects played, paused, resumed and stopped.
Node has no Web Audio, so that case proves the bank is read and the states are
right, and nothing about a speaker.

`AudioSampleTests` reads the real staged manifest, XWB definition and WAV, and
refuses missing-content substitutes for required content. Native and Node test
content is staged from the same original source files as the sample. Under
null audio it also asserts the WAV's same-name wave/effect, levels, order and
lifecycle. The original board routing, loop levels and stop-on-teardown
assertions remain.

## Longer music tracks

Banks are held in memory, and this sample embeds its content in the browser
module. The [audio guide's music section](https://labradorengine.com/docs/guides/audio/#music-bank-memory-and-the-page-download)
explains stored versus decoded memory costs, the roughly 1 MB content threshold
for a separate preloaded `.data` file, and why that still loads before startup.

## Content and provenance

The `wave` manifest entry reads `waves/ping_wav.wav` into one owned memory bank.
The bank, wave and retained effect are all named `ping_wav`; no JSON bank
definition or XWBTool is needed. CMake copies or embeds `source/ping.wav` under
that asset name, so the source tree keeps just one original WAV. The reader
accepts unsigned 8-bit or signed 16-bit little-endian PCM, mono or stereo, at
8,000–96,000 Hz. A single integer-frame forward `smpl` loop is supported. With
no loop region, a looping effect repeats the whole wave.

Both audio kinds preserve content through graphics-device restoration. The
WAV's decoded samples are owned by the audio device after loading; neither the
sample nor a voice borrows the file reader's temporary storage. The native
finite check and browser page's A,S,Escape route exercise this path, while
listening remains separate evidence.

All three sounds are newly synthesized mathematical tones, with no recorded or
third-party source audio. They and their generator are covered by the root
MIT [licence](../../LICENSE). The source WAVs are mono, signed 16-bit PCM at
22,050 Hz. `ping` is a short 880 Hz tone with fades, `chime` is three rising
notes with fades, and `drone` is a one-second loop of complete 220 Hz and
330 Hz sine cycles. Their peak amplitude is below 0.3 full scale, and the
sample starts at volume 0.4.

`source/generate_waves.py` generates the WAVs with Python's standard library.
Microsoft's [XWBTool](https://github.com/microsoft/DirectXTK/wiki/XWBTool)
packages them; it is an authoring dependency only and its binary is not
distributed here. The retained bank was built with the
[may2026 release](https://github.com/microsoft/DirectXTK/releases/tag/may2026),
tool version `2026.5.8.1`, whose x64 `XWBTool.exe` SHA-256 is
`cfeddaf86b2a55b3497fbbc5757d218b25a75dd731547fd6cb7fd5ffc5688b7f`.

Regenerating needs Python 3 and XWBTool, neither of which building or running
the sample needs. On a new Windows 11 machine `python` is a stub that offers
the Microsoft Store rather than running anything; `python --version` tells
them apart, and the browser guide's
[Install Emscripten](https://labradorengine.com/docs/guides/browser/#install-emscripten)
says how to install Python.
[Install the content tools](https://labradorengine.com/docs/guides/content-tools/)
downloads XWBTool to `C:\dev\tools`, and its *Typing the programs' names* puts
that folder on a prompt's `PATH`. Then, from the repository root, which is
`external\labrador` in a starter project:

```bat
python samples\audio\source\generate_waves.py
xwbtool -f -nc -y -o samples\audio\content\sounds\tones.xwb ^
    samples\audio\source\ping.wav samples\audio\source\chime.wav ^
    samples\audio\source\drone.wav
python samples\audio\source\verify_content.py
```

The generator prints nothing. XWBTool prints a `reading` line for each WAV,
`(PCM 1 channels, 16-bit, 22050 Hz)`, then
`writing in-memory wavebank ... w/ 3 entries`. The verifier prints a line for
each wave and exits with 0:

```text
ping: 3528 samples; source PCM matches named XWB entry
chime: 9261 samples; source PCM matches named XWB entry
drone: 22050 samples; source PCM matches named XWB entry
```

[Play a sound in your game](https://labradorengine.com/docs/guides/your-sound/)
uses the standalone WAV path in a development starter. The retained
[XWB walkthrough](https://labradorengine.com/docs/guides/your-sound-xwb/)
copies this bank into a starter project and explains packing other WAVs.

`-f` retains each filename stem in the bank's name table: Labrador resolves those
names, so omitting it is a load error. `-nc` selects standard entries so the
small authoring verifier can compare every named entry's PCM to the source WAV
without implementing the rest of XWB, and so that the engine's own reader,
[`engine/audio/xwb_file.h`](../../engine/audio/xwb_file.h), can read it: that
reader refuses compact entries by name. `AudioTests` reads this bank with it and
compares every wave with its source WAV. The tool embeds a build timestamp, so
regenerations can differ there even when all source PCM is identical. The
verifier checks format, durations, names, signal levels and every payload byte;
run the XAudio2 playback check after rebuilding. Python and XWBTool are not
needed to build or run the checked-in example.

None of the three waves carries a loop region, and the verifier requires that:
the drone loops its whole wave. A WAV with a `smpl` chunk packs into an entry
with a region, which a looping effect then repeats after playing the wave from
its start. The audio guide's
[loop region section](https://labradorengine.com/docs/guides/audio/#a-loop-region-inside-a-wave)
says what of that has been checked on each platform. The finite check creates
its region in memory; nobody has listened to a region, and this sample's
checked-in sounds carry none.

The font is copied at build time from `samples/minimal/content/fonts`; its
provenance is in the root [NOTICE](../../NOTICE). The Windows build also
compiles the template's DPI awareness declaration,
`samples/minimal/dpi_aware.manifest`. On Windows the sample plays
the XWB bank through XAudio2 and DirectXTK's reader; in a browser, through the
engine's own XWB reader and Web Audio. The standalone WAV uses the shared PCM
reader and owned memory-bank seam on every backend. It introduces no codec
dependency or streamed-audio format.
