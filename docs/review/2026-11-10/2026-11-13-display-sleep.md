# Display-sleep requests from a running sample, observed 2026-11-13

The first step of `REV#37`: a sample run with no input while the session's
idle time and the system execution state were sampled, and a sample page's use
of the Screen Wake Lock API checked in headless Chrome. Neither the native
sample nor the browser pages ask to keep the display on. Nothing here measures
a pad: no pad was connected, so whether Windows counts XInput reads as user
activity is still not measured. Whether to add a display hold is the owner's
call.

## What was run

- Native: `LocalMultiplayerSample.exe` from `out/build/x64-release` (D3D11 and
  XAudio2) and from `out/build/x64-release-null` (null render and audio), both
  built at `1dc8533c` and unmodified. Each was started by a scratch PowerShell
  script as its own process and ended by its PID. Nobody touched the keyboard,
  mouse or a pad during the runs.
- Every 15 seconds the script read `GetLastInputInfo` (milliseconds since the
  session's last input), `CallNtPowerInformation(SystemExecutionState)`
  (the combined `ES_*` flags all processes currently request), the
  foreground window's process and `XInputGetState` for the four slots.
- `powercfg /requests` was not run: it reports "This command requires
  administrator privileges" and the session is not elevated. The execution
  state above is the same system-wide request flags without the
  per-process attribution, so attribution here comes from comparing runs.
- The power plan's display timeout (`powercfg /q SCHEME_CURRENT SUB_VIDEO
  VIDEOIDLE`) is 300 seconds on AC and 180 on battery.
- Browser: the `wasm-release` pages `LocalMultiplayerSample.html` and
  `LineSweeperSample.html`, unmodified, served by a Node `http` server on
  `127.0.0.1:10045` and opened in headless Chrome 157.0.8103.78
  (`--headless=new`, a fresh profile) over the DevTools protocol from Node
  24.14.0. WebGL2 drew on the RTX 5080 through ANGLE's Direct3D 11 backend.
  A script added before each document wrapped `navigator.wakeLock.request`
  to count calls. After 15 seconds the script read the count, then asked the
  permission state and made a direct `request('screen')` itself. The scripts
  were scratch and are not committed.

## What happened

| Run | Duration | Idle time | Execution state |
|---|---|---|---|
| Baseline, no sample | 30 s | 362 s rising to 377 s | `0x0` |
| x64-release sample | 7 min | 392 s rising to 798 s | `0x1` (`ES_SYSTEM_REQUIRED`) from the second sample on |
| After it was ended | | 815 s | `0x0` |
| x64-release-null sample | 1 min | 844 s rising to 889 s | `0x0` |

- The idle time rose by the wall-clock interval throughout both runs, so
  nothing the engine did counted as user input. It passed the 300-second
  display timeout during the run with no request keeping the display on.
- No sample of either run showed `ES_DISPLAY_REQUIRED` (`0x2`). The engine
  makes no `SetThreadExecutionState` call, which matches the finding.
- `ES_SYSTEM_REQUIRED` appeared only while the XAudio2 build ran and not under
  null audio. That points at the open audio stream, which Windows' audio stack
  reports as a system request, not a display request; without `powercfg
  /requests` the attribution is inferred from the comparison.
- `XInputGetState` reported no pad connected in any slot, so the XInput half
  of the finding was not exercised.
- The sample's window never became the foreground window: the owner's Task
  Manager stayed in front, because Windows did not hand the foreground to a
  process started from a background script. A game started by a person
  normally takes the foreground. This does not change the result, because no
  engine code path requests a display hold in either state.
- Browser: `navigator.wakeLock` exists, the page is a secure context
  (`127.0.0.1`) and visible, and neither sample page called
  `navigator.wakeLock.request` (0 calls in each). The `screen-wake-lock`
  permission was `granted`, and a direct `request('screen')` from the probe
  succeeded (`released=false`, `type=screen`). While the probe held that lock,
  the system execution state read `0x2`, and it read `0x0` after the lock was
  released and Chrome closed. So headless Chrome on this machine passes a page's
  screen wake lock through to Windows, and the sample pages do not ask for one.

## What this does not show

- Whether Windows treats XInput polling, or a pad's buttons and sticks, as user
  activity. That needs the owner's pad and a short display timeout, and it is
  what decides whether the finding's symptom reproduces in pad-only play.
- Whether the display actually dimmed or turned off. The monitor's state was
  not read; idle time and execution state are the inputs Windows decides with.
- Headed Chrome, Firefox, Safari, a phone, or a tab that is hidden and shown
  again, where a wake lock is released and has to be requested again.
