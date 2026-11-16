# Nominal window-display metadata qualification, 2026-11-17

[SM-86](../product/BACKLOG.md#sm-86) adds configured display facts through
`Window::display_info()` and `Application::display_info()`. This records
synthetic behavior and a bounded native query. It contains no physical
refresh, VRR, scan-out, audible, pad or pacing acceptance.

## Contract and selection

Each native call selects the window's current monitor with
`MonitorFromWindow(..., MONITOR_DEFAULTTONEAREST)`, reads its logical GDI
device name through `GetMonitorInfoW`, and queries that named device's
`ENUM_CURRENT_SETTINGS` with `EnumDisplaySettingsExW`. The result is a copy,
queried on the owning window/application thread. There is no event cache:
the next call follows a move or a mode-only change without relying on
`WM_DISPLAYCHANGE`, whose documented notification concerns resolution.

The monitor id is UTF-8 session metadata, not a stable physical-panel id.
Refresh is the API's whole-Hz configured integer. A missing window/monitor,
failed identity lookup or invalid device name yields unknown fields. Mode
failure can preserve the id while refresh remains unknown. Missing
`DM_DISPLAYFREQUENCY`, default sentinels 0/1 and values outside `int` remain
unknown; there is no guessed 60 Hz or arbitrary 1000 Hz cap.

Microsoft owns the [monitor-selection contract](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-monitorfromwindow),
[logical device identity](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-monitorinfoexw),
[current-mode query](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enumdisplaysettingsexw)
and [default-frequency sentinels](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-devmodew).

Browser/windowless/closed queries remain unknown. A native null renderer still
has a real Win32 window and can therefore report its OS-configured metadata;
this says nothing about rendering or presentation by the null backend.

## Deterministic and compiled checks

- Platform-call fixtures check the actual window handle, nearest-monitor
  flag, returned monitor, device name, structure sizes and current-mode
  selection. They cover monitor changes, same-monitor mode changes, a
  successful lookup followed by failure, missing field, 0/1 defaults,
  representable bounds, oversized values, empty/unterminated names and no
  platform call for a null handle.
- Portable AppTests check unknown metadata before a window exists. Native
  null tests verify Application forwarding and unknown metadata after close.
  Native-null AppTests and LineSweeperViewTests pass 2/2.
- Result tests serialize known, unknown and partial snapshots, retain both
  sampling labels, and keep `workload.refresh_hz` and timing semantics
  unchanged. The collector is compiled with its queries outside retained
  phase/interval endpoints. No sustained frame-benchmark run was made.
- Python analysis fixtures validate optional records, retain them per run,
  label matching/mixed/partial contexts and prove measured summaries,
  targets, verdicts and cloud pooling behavior remain unchanged. Legacy
  records keep their existing output shape. All 39 analysis tests pass.
- Wasm AppTests pass under Node. The real browser Window page runs its new
  unknown-metadata assertion; a bounded metadata/close driver passes in
  Chrome 157.0.8103.78 and observes both page quit and owner closure. The
  registered full WebWindowTests sequence still fails at this desktop's
  known full-screen `TypeError: not granted`, after 46 passing input checks.
  That input/full-screen remainder is not weakened or claimed passed.

## Bounded native session

Source is the implementation described by *Report nominal window display
metadata*, based on `5cff44b9c6da777479229832abb40bfc0fc6e65c`, in
`C:/Projects/Labrador-agent-refresh`. The x64-debug-null build uses MSVC
19.51.36256.0 and installed vcpkg packages reused read-only. The test window
is hidden and never requests activation. No display setting is changed.

`AppTests --test-case="native window display metadata matches*" --success`
passes 1 case / 4 assertions. Its public getter and direct selected-monitor
current-mode lookup agree:

| Field | Observed |
|---|---|
| Logical monitor id | `\\.\DISPLAY2` |
| Current mode pixels | 2560 × 1440 |
| Nominal configured whole Hz | 59 |
| Closed-window result | empty id / unknown rate |

The adjacent read-only `Win32_VideoController` record names NVIDIA GeForce
RTX 5080, Windows driver `32.0.16.1692`, and 2560 × 1440 / 59. It corroborates
those mode fields; it does not identify a physical panel or establish an
independently measured/fractional rate. A physical cross-monitor move and a
real mode change were not performed; their query refresh behavior is covered
by the deterministic fixtures.

Logs remain in this worktree's ignored `out/refresh-live-query.log`,
`refresh-live-cim.json`, native/wasm logs and `refresh-browser-metadata.log`.
The [pacing method](../port/pacing-method.md) and
[native collection procedure](local-frame-bench.md) retain the operator's
physical-display and mode record. The combined app verification matrix,
website gate and lint remain the branch's ordinary integration gates.

## Combined qualification, 2026-11-17

The combined seven-track app sweep, local lint and website gate subsequently
passed at `fadd1a63`, closing SM-86. [The PR record](../verification/2026-11-17-pr-current.md)
retains that integration evidence. The native mode observations above retain
their earlier source and do not become measured pacing or physical
monitor-transfer/mode-change acceptance.
