# Work audit of 2026-11-12: status

The live ledger for [the audit](README.md). Record each resolution here in the commit that makes it: the commit and its evidence, the story the owner made of it, or the reason it was declined. The row leaves [TODO.md](../../../TODO.md) in the same commit. A finding is a question until its fix proves it.

| id | Finding | State |
|---|---|---|
| `REV#122` | [The circle predicates disagree on a negative or non-finite radius](FINDINGS.md#rev122) | Open |
| `REV#123` | [narrow_phase's documented throw for a shape with no type is untested](FINDINGS.md#rev123) | Open |
| `REV#124` | [Camera::frame accepts non-finite rectangles and can return the zero scale its header rules out](FINDINGS.md#rev124) | Open |
| `REV#125` | [A NaN volume, pitch or pan passes the audio clamp and reaches the backends](FINDINGS.md#rev125) | Open |
| `REV#126` | [Nothing asserts that a warmed frame allocates nothing](FINDINGS.md#rev126) | Open |
| `REV#127` | [The text glyph walk and Font::wrap have no complexity rows in the benchmarks](FINDINGS.md#rev127) | Open |
| `REV#128` | [StepTimer::Tick has no deterministic test, and its snap compare can overflow](FINDINGS.md#rev128) | Open |
| `REV#129` | [RectangleI and Vector2I have no behavioural tests](FINDINGS.md#rev129) | Open |
| `REV#130` | [Whether RectangleI and Vector2I refuse non-finite, out-of-range and zero-divisor input](FINDINGS.md#rev130) | Open |
| `REV#131` | [Two collision tests build a near-miss and never check it, and a test preamble's count is stale](FINDINGS.md#rev131) | Open |
| `REV#132` | [Nothing checks the guides' 'search for' anchors, and one has already drifted](FINDINGS.md#rev132) | Open |
| `REV#133` | [Whether each code-editing guide's finished files are compiled](FINDINGS.md#rev133) | Open |
| `REV#134` | [The testing guide never drives a screen with keys from a compiled test](FINDINGS.md#rev134) | Open |
| `REV#135` | [No page explains judder on a fast display or shows drawing between steps](FINDINGS.md#rev135) | Open |
| `REV#136` | [A guide to random numbers in the starter, after REV#35](FINDINGS.md#rev136) | Open |
| `REV#137` | [Copying the components into a game leaves their tests behind](FINDINGS.md#rev137) | Open |
| `REV#138` | [Hand-written declaration listings in the concept and guide pages are unchecked](FINDINGS.md#rev138) | Open |
| `REV#139` | [The verification planner models render paths only](FINDINGS.md#rev139) | Open |
| `REV#140` | [The configurations only ci_gap_report runs have no inventory, and the brief contradicts itself](FINDINGS.md#rev140) | Open |
| `REV#141` | [The release procedure has no preflight](FINDINGS.md#rev141) | Open |
| `REV#142` | [Whether CHANGES.md's rule covers every build file a consumer's configure runs](FINDINGS.md#rev142) | Open |
| `REV#143` | [No doctest suite has a ctest timeout](FINDINGS.md#rev143) | Open |
| `REV#144` | [CONTRIBUTING's required-check list is held to the workflows by nothing](FINDINGS.md#rev144) | Open |
| `REV#145` | [No single local command runs the lint job, and its three copies drift](FINDINGS.md#rev145) | Open |
| `REV#146` | [The verification preflight does not refuse an inherited MSYSTEM](FINDINGS.md#rev146) | Open |
| `REV#147` | [Whether build time is measured, as PHILOSOPHY's maintained property needs](FINDINGS.md#rev147) | Open |
| `REV#148` | [The verification runner records the revision and device as uncollected](FINDINGS.md#rev148) | Open |
| `REV#149` | [v0.4.0 has no preparation row](FINDINGS.md#rev149) | Open |
| `REV#150` | [CI's Firefox draws nothing because it has no software WebGL2](FINDINGS.md#rev150) | Open |
| `REV#151` | [No browser but Chromium has read back a pixel](FINDINGS.md#rev151) | Open |
| `REV#152` | [Whether LineSweeper runs in Safari on the iOS Simulator ahead of SM-13](FINDINGS.md#rev152) | Open |
| `REV#153` | [The slow-device case has no rehearsal under throttling](FINDINGS.md#rev153) | Open |
| `REV#154` | [No test reads the wasm heap's size](FINDINGS.md#rev154) | Open |
| `REV#155` | [Nobody knows what linesweeper.wasm is made of](FINDINGS.md#rev155) | Open |
| `REV#156` | [The WebGL context leaves powerPreference unstated](FINDINGS.md#rev156) | Open |
| `REV#157` | [The bundle's context restore has never met a real driver reset](FINDINGS.md#rev157) | Open |
| `REV#158` | [Report 19's first-frame evidence can be captured here](FINDINGS.md#rev158) | Open |
| `REV#159` | [CI's Safari job fails intermittently on the components gallery page](FINDINGS.md#rev159) | Open. Not only the gallery: with the page hook from `f0198ff1`, LineSweeper once received every key while one frame gap was 644 ms, longer than the runner's 150 ms hold. Sending each key level as its own WebDriver action, with or without confirming it in the page's key log, made the audio page fail every run: once its first key starts Web Audio, later keys go missing. A CI experiment on 2026-11-12 ran the audio page three times each way: one action list passed three of three; separate actions, with fixed waits or with page queries, failed three of three, with the window focused and an output device present. `run_safari.mjs` keeps the single list and holds each key 250 ms, below the 400 ms menu repeat, and a failed run prints its phase and the page's timed keys, focus, audio state and longest frame gap. It closes when CI's Safari job has run without this failure for a week. |
