# LineSweeper iOS Simulator pilot

**SM-97 status, 2026-11-20.** The pilot is integrated. An actual run requires
macOS with Xcode, an available iPhone Simulator, enabled safaridriver and a
clean development wasm build. The Windows 11 integration host (10.0.26300)
has neither `xcrun` nor `safaridriver`. The execution command exits 2 on this
host before creating an output directory or starting a session.

[The manual tool](../../tools/ios_simulator/README.md) defaults to a plan. Its
receipt records the served files and compiled source identity, Simulator and
Safari versions, startup, touch play and quit, forced context loss and
restoration, timings, screenshots and failures. The pilot uses the website's
Simulator setup and adds its own hooks to the Safari sample harness. The shared
quit hook still records the first onQuit boundary for the desktop collision
sample.

The 22 pilot fixtures cover the shared hook, the WebDriver route, build and
browser checks, total setup/play budget, cancellation and cleanup. The 33
Safari protocol fixtures cover the shared runner. Both sets use stand-ins for
Safari and Simulator. Startup cost, input, layout, audio and memory on iOS are
still unmeasured. The story stays open for the actual run.

Record every attempt with its receipt, browser and runtime identities, timings
and reviewed screenshots. SM-13 still requires the [real-iPhone checks](web-to-be-tested.md#needs-real-phones-m7b)
before BDP-10 can lift the phone gate. The pilot is manually invoked.
