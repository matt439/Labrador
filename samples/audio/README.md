# Audio sample

A sound board with two one-shots and one persistent looping voice. It loads a
required `SoundBank` through the manifest, resolves names once, and shows play,
pause, resume, immediate stop, volume, pitch and stereo pan. The original PCM
content and its ready-to-play XWB container are checked in; a fresh clone needs
no audio authoring tools.

Build and run from the repository root in the normal developer shell:

```powershell
cmake --preset x64-debug
cmake --build --preset x64-debug --target AudioSample AudioSampleTests
ctest --preset x64-debug -R '^AudioSampleTests$' --output-on-failure
& ./out/build/x64-debug/samples/audio/AudioSample.exe
```

| Control | Action |
| --- | --- |
| 1 / 2 | Play ping / ascending chime; successive one-shots may overlap. |
| 3 | Start the drone loop. |
| Space | Pause or resume the drone; a stopped drone stays stopped. |
| S | Stop the drone immediately. |
| Up / Down | Raise / lower volume. |
| Left / Right | Move the sound between stereo channels. |
| Q / E | Lower / raise pitch. |
| Escape | Quit. |

The displayed values affect the persistent voice immediately and the next
one-shot at launch. A one-shot has no retained handle, so it cannot be adjusted
afterward. Losing focus pauses a playing loop and returning resumes it; a loop
the player paused remains paused. Leaving the state stops the loop while its
borrowed bank is still alive.

The normal Windows presets use XAudio2 and need an active Windows audio output
to play, the browser build uses Web Audio (below), and the `-null` presets
record commands without sound; the sample labels that mode
on screen. `SoundBank::audible()` means loaded content, and is also true under
null audio. It is not evidence that speakers produced sound.

## Finite playback check

```powershell
$audio = Start-Process ./out/build/x64-debug/samples/audio/AudioSample.exe `
    -ArgumentList '--smoke-test' -WindowStyle Hidden -Wait -PassThru
if ($audio.ExitCode -ne 0) { throw 'Audio sample smoke failed' }
```

This check needs no graphics device or window. It loads the packaged manifest's
required bank, plays both one-shots, starts the looping voice and asserts its
playing, paused, resumed and stopped states. It exits in about a second. A
missing bank or inactive audio endpoint fails the XAudio2 check rather than
accepting silence. With null audio it checks the same lifecycle and explicitly
reports no playback; it does not validate the XWB container.

## In a browser

The `wasm-debug` and `wasm-release` presets build the same sound board as a
page, `AudioSample.html`, beside its `.js` and `.wasm`, playing through the
engine's Web Audio backend. `main_web.cpp` is the browser's entry point and
`web/shell.html` the development page; the manifest, the bank and the font are
embedded in the module at the paths the Windows build copies them to. In a
shell where vcvars and `emsdk_env.bat` have both run:

```powershell
cmake --preset wasm-debug
cmake --build --preset wasm-debug --target AudioSample AudioSampleTests
ctest --preset wasm-debug -R '^AudioSampleTests$' --output-on-failure
python -m http.server --directory ./out/build/wasm-debug/samples/audio
```

Then open `AudioSample.html` from that server, because a browser will not
compile a module read from `file://`. The controls are the table above.
**Nothing sounds until the page's first key press, click or touch**, which is
the browser's rule: the backend makes its audio context inside that event, and
the key that does it is still read by the sample, so a first press of 1 plays
the ping. Escape ends the module and says so under the canvas.

There is no `--smoke-test` in a browser, because a page has no command line.
Its lifecycle is an `AudioSampleTests` case in this build instead, run under
Node: the packaged manifest's required bank loaded through the Web Audio
backend, both one-shots, and the drone played, paused, resumed and stopped.
Node has no Web Audio, so that case proves the bank is read and the states are
right, and nothing about a speaker.

`AudioSampleTests` reads the real manifest and definition and refuses a silent
bank. Under null audio it also asserts which waves the sample plays, the loop
lifecycle, the levels sent to both kinds of voice and the stop on teardown.

## Content and provenance

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

To regenerate with that tool on `PATH`, from the repository root:

```powershell
python samples/audio/source/generate_waves.py
xwbtool -f -nc -y -o samples/audio/content/sounds/tones.xwb `
    samples/audio/source/ping.wav samples/audio/source/chime.wav `
    samples/audio/source/drone.wav
python samples/audio/source/verify_content.py
```

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

The font is copied at build time from `samples/minimal/content/fonts`; its
provenance is in the root [NOTICE](../../NOTICE). On Windows the sample plays
the bank through XAudio2 and DirectXTK's reader; in a browser, through the
engine's own reader and Web Audio. It introduces no audio backend or engine
format of its own.
