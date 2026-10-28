# SM-55 Web Audio transport review

Source review dated 2026-10-28 at revision
`8048e2448cb813adf1ac2f916ce8cb2ef4058308`.
Scope: `engine/audio/webaudio/audio_device.cpp`, its public contract, callers,
source registration and current test/acceptance records. No source or build
change and no new playback feature follows from this review.

Sources: [Web Audio implementation](../../engine/audio/webaudio/audio_device.cpp),
[device contract](../../engine/audio/audio_device.h),
[Node tests](../../tests/audio/webaudio_tests.cpp) and
[audio sample](../../samples/audio/README.md). Line numbers name the reviewed
revision. [Code hygiene](../code-hygiene.md#sm-55-web-audio-transport-review)
owns the organisation verdict; existing browser/listening gates remain separate.

## Verdict: keep the current file and ownership boundary

The file already has a transport boundary: global prefixed EM_JS functions
forward into one `Module.labradorAudio` object, followed by the C++ AudioDevice
implementation. JavaScript owns browser contexts, buffers, nodes and playhead
arithmetic. C++ owns validated handle lookup, wave-name tables and the
synchronous three-valued state/loop contract. The existing named bridge is the
cut; neither side calls through the other side's private layout.

Moving the transport into a second translation unit could improve navigation,
but would add private C-linkage declarations and source/link retention work
without removing a state owner or lifetime dependency. Moving just part of
the object would split closures that share context/voice state. Moving the
reported state into JavaScript to justify the split would instead change the
contract and make it depend on asynchronous browser completion. No alternate
transport, independently reusable owner or simpler lifecycle is established
by this review. File length alone does not justify that change (CONVENTIONS,
Files; T4), and no frame-path allocation/dispatch framework is needed (T8).

Keep the install object, its thin EM_JS wrappers and AudioDevice implementation
in `audio_device.cpp`. No extraction story, new public API or CMake change is
selected. The review can close with this keep verdict while the existing
browser and listening acceptance remains open.

## Owner and protocol inventory

| Owner | Source-established responsibility |
|---|---|
| `labrador_audio_install`, `audio_device.cpp:89-423` | Installs `Module.labradorAudio` once per compiled module and captures Web Audio capability, gesture names and transport helper functions. The object holds the JavaScript device table. |
| JavaScript device, `:209-244` | Owns one context reference, suspension flag, bank/voice arrays and one capture-phase gesture listener. Device close removes its listeners, detaches persistent voices, requests context close and clears its table slot. |
| JavaScript bank/wave, `:247-291` | Holds AudioBuffers, durations and optional loop-region times. PCM16 samples are copied from the wasm heap into browser float channel buffers. |
| JavaScript persistent voice, `:309-405` | Holds bank/wave identity, volume/rate/pan, loop/active/ended flags, playhead offset/start time and current source/gain/panner nodes. These are transport state, not a second C++ SoundState enum. |
| EM_JS entry points, `:425-496` | Forward device/bank/voice IDs, levels and sample pointers to the installed object. C++ calls these functions directly; the JS object does not read C++ struct members. |
| `AudioDevice::Impl`, `:529-588` | Owns the JS device ID, C++ wave-name tables and WebVoice records. It opens the transport in its constructor and closes it in its destructor. Bounds/unresolved-handle checks precede ordinary voice operations. |
| `WebVoice`, `:520-527` | Holds checked bank/wave identity, reported SoundState and the public looping flag. It does not own browser nodes or calculate elapsed playback time. |

On successful bank/voice creation, both languages append in matching order.
That index protocol matters more than physical file placement; splitting files
does not eliminate it. The review does not add an exception-atomic publication
guarantee that the current contract does not state.

`open_wave_bank()` (`:599-662`) reads/validates the XWB through shared engine
code, checks every declared wave name, converts each wave through the bridge
and then stores the file-order names. Its temporary `WaveBankData` owns PCM
during the load; after return the C++ bank table keeps names and the browser
keeps copied float buffers. Playback does not revisit that temporary PCM.
There is no streaming owner or per-bank release API in this backend.

## Gesture and unlock lifecycle

`open()` registers capture listeners for keydown, mousedown, pointerdown,
pointerup and touchend only when the Web Audio capability test succeeds and
`window` exists (`:93-105`, `:209-228`). Each device owns its listener function,
so closing one device removes that device's subscriptions rather than another
device's. The capability result is captured when the module transport installs.

`grants_activation()` (`:101-106`) uses `navigator.userActivation.isActive`
when available; the fallback excludes Escape keydown. `unlock()` (`:187-204`)
does nothing until a bank exists, lazily constructs an AudioContext, starts
active pending voices and then requests suspend/resume according to the
device flag/context state. Opening a bank after prior page activation also
attempts unlock through `hasBeenActive` (`:247-254`).

The source therefore preserves the public pre-gesture distinction:

- `play_wave()` (`:294-307`) drops a one-shot when there is no context, the
  device is suspended or its wave has no AudioBuffer. There is no pending
  one-shot queue to replay on a later gesture.
- A persistent play marks the JS voice active; `start()` returns while no
  context exists. C++ still reports playing. Unlock later attempts to start
  that voice. Pause and immediate stop cancel its pending active start.
- With no usable Web Audio at all, the transport still stores names/voice
  state and never creates a context. This is the Node path, not evidence of
  browser output or successful autoplay permission.

The gesture listener belongs to the backend rather than the engine Window.
It calls no C++ state callback and adds no browser-only verb to AudioDevice.
That retains the platform/API boundary described by ARCHITECTURE, Modules,
and the public pre-gesture contract in `engine/audio/audio_device.h`.

## Node, playhead and callback lifetime

`connect()` (`:140-155`) constructs a source, gain and stereo panner and links
them to the device context destination. A source is used for one start. A
persistent `start()` (`:157-185`) installs its nodes on the voice, stores the
context start time and begins at the saved offset. An already-ended nonlooping
offset marks the voice ended instead of creating another source.

`position()` (`:108-120`) derives playback position from saved offset,
context elapsed time and rate, wrapping a looping region when applicable.
Pause saves that position and detaches its nodes; resume creates another
source from the saved position. A rate change rebases position/start time
before updating playbackRate (`:358-390`). C++ maps pitch through `exp2`
(`:508-511`); volume and pan update the current gain/panner when present.

`detach()` (`:122-138`) clears the voice's source reference and its onended
handler before stopping/disconnecting the source and disconnecting the gain
and panner. The natural-end callback checks that it still names the voice's
current source before detaching and setting an ended flag. It does not call
into a possibly destroyed C++ object. Fire-and-forget nodes use their own
onended closure to disconnect their graph and are not added to the persistent
voice table (`:294-307`).

`ended()` (`:400-405`) consumes a pending ended indication. C++ `settled()`
(`:578-587`) asks for it only while its reported state is playing and then
changes that state to stopped. This handshake is deliberate: browser nodes
own actual completion, while game-visible state transitions remain synchronous
and testable without an AudioContext. It is not a reason to copy all JS
playhead/node state into C++ or all C++ SoundState logic into JS.

AudioDevice's defaulted move operations transfer its Impl owner. Destruction
requests close through the same owner (`:529-544`, `:590-597`). Listener
removal, persistent-voice detachment and the context-close request are visible
in source; completion of asynchronous browser teardown is not established by
this review.

## Playback state and suspension

`play_voice()` (`:708-733`) applies levels first. A paused voice resumes its
existing offset/loop setting, a stopped voice starts from zero with the new
loop flag, and an already-playing voice is not restarted. `stop_voice()`
(`:735-751`) stops immediately when requested; otherwise it exits an active
loop, and a nonlooping voice continues its tail. Pause/resume only act on the
corresponding reported state. These rules remain in C++ and agree with the
existing Node state tests; this review runs none of those tests.

Device suspension (`:407-421`, `:808-816`) sets the transport's suspension
flag and requests AudioContext suspend/resume when a context exists. It does
not turn every C++ playing voice into a paused SoundState. The sample's own
pause-on-focus policy is separate from the backend context suspension.
`AudioDevice::update()` has no transport pump; completion is observed on a
state query and browser callbacks perform the graph work.

## Failure and unavailable-output boundaries

Reviewed explicit failure handling:

- Shared XWB read errors become `std::runtime_error` naming the container;
  a definition naming an absent wave remains `std::out_of_range`
  (`:605-637`). This preserves optional-bank fallback versus invalid content.
- AudioBuffer construction refusal rolls back the just-added JS bank and
  returns failure (`:272-280`); C++ reports the bank, wave and sample rate
  (`:640-658`). The review makes no claim about a universal browser rate range.
- Invalid bank/voice indices and unresolved handles fail through the C++
  checked tables (`:551-574`, `:675-705`). Level-clamping policy remains
  above this backend in SoundBank.
- The deliberate detach path catches a source-stop exception locally.
  Context/node creation and asynchronous context resume/suspend/close are
  otherwise browser operations; the current calls do not establish a general
  promise-rejection recovery policy. Do not describe them as verified recovery
  or silently add retries as part of file organization.

No additional defect is filed from speculative browser outcomes or allocation
failure assumptions in this scope. Source organization can be decided without
inventing runtime evidence. Existing limitations stay with their current
browser/audio acceptance owners.

## Validation boundary and reopening conditions

`engine/CMakeLists.txt:323-345` selects this source only for the Emscripten
Web Audio backend. `tests/audio/CMakeLists.txt` selects `webaudio_tests.cpp`
for that backend; those tests run under Node with no AudioContext. They cover
container/name checking, handles and state transitions. The audio sample's
Node lifecycle check likewise cannot establish a moved or unchanged Web Audio
graph, gesture policy, transport output or browser teardown.

`docs/port/web.md` retains historical Chrome scratch-page AnalyserNode checks
for unlock, pitch, pan, pause/resume, non-immediate loop exit and context
suspension. That is dated measurement evidence, not a run at this revision or
listening. The same record limits Firefox's sample-page checks and leaves
Safari/phone coverage open. `samples/audio/README.md` and WEB#3 still own
actual listening acceptance. This review neither closes WEB#3 nor claims an
AnalyserNode measurement is a person hearing output.

No extraction is selected. Reopen only for a concrete lifecycle/testing or
second-consumer need. A future private transport extraction would need its own
bounded story, Emscripten source enumeration and an unchanged typed bridge and
index protocol. It must keep the JS object's shared closures together, install
once, remove only its own listeners, preserve device/voice moves and teardown,
and avoid duplicating authoritative state.

That future story would require actual browser regressions for pre-gesture
one-shot dropping, pending persistent voices and their cancellation, a first
gesture's sound, two-device independence, natural end, stale callbacks,
pause/resume offset and pitch rebasing, loop regions/exit, suspension/resume
and teardown. Observable transport/node output must be checked in a browser;
native/null and Node state/handle checks remain useful companions. Listening
is a separate WEB#3 activity, and results must state browser/device scope.

This source-only review runs no build, Node/browser audio test, AudioContext,
speakers, generated game JavaScript, GPU startup, PowerShell fixture, network
operation or credential access. The current Emscripten environment-write stop
and other actual stopped operations remain in force.
