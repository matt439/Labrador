# LineSweeper iOS Simulator pilot

`pilot.mjs` is an explicitly invoked local pilot for Safari in one named iPhone
Simulator. Its default is a plan: no driver, Simulator, server or output folder
starts without `--execute`. It adds no job, schedule or required check.

Run it from a checkout containing this tool on an authorized Mac with Xcode,
the selected available iPhone Simulator and an already enabled safaridriver.
The existing website Simulator setup owns boot/session retries and driver
cleanup. This pilot supplies a total setup-and-play budget across those retries;
it does not enable safaridriver, install a runtime or change Xcode settings.

Use the development `LineSweeperSample` page from a clean GL/Web Audio wasm
build containing the pointer observation exports. The packaged module excludes
those exports and retains its phone gate. A build artifact copied from the
existing wasm builder is sufficient; this tool does not build or download it.
Preserve its `samples/linesweeper/LineSweeperSample.{html,js,wasm}` layout.

```sh
node tools/ios_simulator/pilot.mjs \
  --build /path/to/wasm-artifact \
  --source-commit 0123456789abcdef0123456789abcdef01234567 \
  --udid 12345678-1234-1234-1234-123456789abc \
  --output out/ios-pilot-001
```

Replace the example identities with the build's full clean source commit and
an actual available Simulator UUID. Inspect the printed plan, then run the same
command with `--execute`. The output directory must be new and its parent must
exist; previous evidence is never overwritten. `--timeout 240` is the default
total budget in seconds; 30–600 is allowed. Driver/input cleanup and one failed
page snapshot have separate bounded commands and may add about 35 seconds.
Ctrl+C requests cancellation and follows the same cleanup path. The selected
Simulator may remain booted and Safari open, matching the existing setup;
the tool stops its own automation session/driver and loopback server.

The receipt contains the three served files' SHA-256 hashes and sizes, the
compiled clean source/build/backend stamp, macOS/Xcode/architecture, explicit
Simulator UUID/runtime/name, Safari version and user agent, drawing buffer and
CSS viewport, per-step wall time, bounded setup logs, failures and cleanup.
The compiled stamp reports the configure-time source identity; the receipt does
not independently rebuild or prove the artifact's source provenance. The served
bytes are checked again at the end. A dirty/mismatched source stamp,
wrong backend/browser, missing observations, page warning/error, failed selected
step or cleanup problem fails the receipt. An unavailable lifecycle extension
fails that selected check; it is not silently omitted.

The route waits for actual WebGL draws, sends trusted W3C touch actions to
pause and resume, sends a DROP touch while play-state draws continue, forces
`WEBGL_lose_context` and waits for both DOM and engine callbacks plus restored
draws, then touches QUIT and checks `onQuit` and stopped drawing. It captures
paused, playing and restored PNGs for subsequent visual review. The authored
control dimensions are fixture-checked against the sample header; its actual
camera supplies the coordinate conversion. No synthetic touch event or game
action is dispatched through script. The play probe observes input delivery
and state/draw continuation; it does not read back the board or assert a score.

The existing ordinary-play startup stamp supplies build identity. Its observer
stops on the first callback; this pilot collects no pacing samples. No full-well
fixture or sampled-key game gate is selected. Navigation/input is not retried
by this route; existing setup retries and their cost remain in the log.

Simulator results cannot close the real-iPhone, physical touch, audio/listening,
rotation/safe-area, display pacing, long-session memory, genuine GPU loss or
host-site phone-gate checks. The report names these omissions. Review every
captured PNG and retain failed attempts when measuring reliability and cost.
Do not turn one result into a recurring lane without a later owner decision.

Offline qualification starts no browser or Simulator:

```text
node --test tools/ios_simulator/pilot.test.mjs tests/render/web/safari_protocol.test.mjs
```

The website gate imports the pilot fixtures too. Protocol fixtures are labelled
`protocol-fixture` and use fake driver responses; they establish runner behavior,
not Safari or iOS support. Actual-run status and remaining prerequisites are in
[the live pilot record](../../docs/port/ios-simulator-pilot.md).
