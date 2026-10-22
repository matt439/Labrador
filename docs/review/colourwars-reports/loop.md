# Application's frame loop: reports 9, 10, 14

Reports 9, 10 and 14 from ColourWars all ask for the same thing: a client hook
inside `Application`'s private loop (`engine/app/application.cpp:282-329`,
`tick`, `update` and `render`). Report 9 wants code that runs every frame
whatever is on the stack. Report 10 wants a point between `submit` and
`end_frame` where it can read the back buffer. Report 14 wants the timing of
the frame the engine just ran. One rule decides all three. The loop stays
closed, and the engine adds to `Application` only what a client cannot build
from public calls. That rule comes from PHILOSOPHY, The public face, which asks
"whether a client would have to write them itself"
(`docs/design/PHILOSOPHY.md:795-796`), and from Structural types, under which
`Application` is machinery the game drives, with "nothing here to subclass"
(`engine/app/application.h:109`, `docs/design/PHILOSOPHY.md:489-503`), and T8
(`docs/design/PHILOSOPHY.md:139-142`).

Report 9 can be built from public calls, so it gets documentation and a test.
Report 10 can be built from public calls too, and `draw()`'s pure-read rule
makes the result exact. It gets documentation, a stated seam term and a named
contract case. Report 14 cannot be built, because `begin_frame`, `submit`,
`end_frame` and the catch-up step count all run where a client cannot reach
them. It is accepted, narrowed to a CPU-time snapshot. The order to implement
them in:

1. Report 10's term in `renderer.h` and `state.h`, with its null and pixel
   cases (S).
2. Report 9's paragraph in `state_context.h`, its test and the website bullet
   (S).
3. Report 14, as a story of its own under E10 (M).

CONTRIBUTING.md's one-self-contained-commit rule (`CONTRIBUTING.md:38`) is
easier to satisfy with 10 and 9 as separate commits. If they do share one, the
`state.h` tightening goes in that commit, because report 9's root-state
pattern must not be written down with a `draw()` that submits either.

Line numbers are as of `0c15f34`. This is a decision record, written ahead of
PI-09 (filing the reports) and PI-10 (answering them)
(`docs/product/BACKLOG.md:245`, `:398`). It implements nothing. GAP-REGISTER
G74 (`docs/product/GAP-REGISTER.md:1489-1504`) is the gap these answers close.

## 9. Code that sees every frame, and input capture

**Decision.** Document only. No `Application` observer hook is added: a root
`State` that owns a nested `StateContext` already sees every update and every
draw, under rules the stack already has. The input capture and routing half of
the report is declined as game policy.

**What the engine has now.** `Application::update` polls the four devices, then
calls `StateContext::update`, then `audio_device_->update()`, with no client
callback between them (`engine/app/application.cpp:288-306`: polls at
`:299-302`, the stack at `:304-305`, audio at `:306`). `update` and `render` are
private (`engine/app/application.h:322-323`). `tick` is a private
`WindowNotify` override (`:334`). The public section (`:126-266`) has no
callback. `StateContext` updates only its top state
(`engine/core/state_context.h:50-51`, `engine/core/state_context.cpp:45-62`).
`covers_screen` decides what is drawn, not what is updated
(`engine/core/state.h:36-48`, `engine/core/state_context.cpp:64-84`). A push
suspends the state below it (`engine/core/state_context.h:78-81`). Raw input
reads are never consumed. `pressed`, `released` and `typed()` are `const`, so
any number of readers see the same edge and the same text
(`engine/input/keyboard.h:258-259`, `:279`).

A context may already be a member of a state. `~StateContext` names "a context
that is not an Application's base - a nested one" as a case it was written for
(`engine/core/state_context.cpp:36-39`). `update`, `draw` and
`notify_activation` are public (`engine/core/state_context.h:51`, `:56`,
`:152`). Each context has its own deferral flag, so a nested stack defers its
own operations while the outer stack is still inside the root's `update()`
(`engine/core/state_context.cpp:10-27`, `:55-61`).

**Why.** The report itself calls the hook a convenience, and the client's own
exhibit builds a composed root host from existing calls that passes. T1's "Not
a licence for: speculative frameworks" (`docs/design/PHILOSOPHY.md:45-47`)
applies, and no sample has a console or an overlay. T11's price names
observers among the designs that "must be rethought"
(`docs/design/PHILOSOPHY.md:206-208`). A callback on `Application` would need a
lifetime and ordering contract of its own. Services and lifetimes says
"Construction and destruction order are designed"
(`docs/design/PHILOSOPHY.md:521-522`), so that contract would have to answer:

- May the callback push or pop?
- Does it run on an empty stack?
- Does it run before or after the audio update?
- Does it run once per catch-up step, or once per tick?

A root state answers every one of these with rules that already exist. Its
`update()` runs inside the outer stack's deferral, and once per fixed step,
after the polls and before audio, which is exactly the point the report asked
for. The nested stack defers on its own. Its `draw()` stays `const`.

The nearest precedent for adding a hook is `notify_activation`'s remembered
bool, which the engine wrote once because "every client would write the same"
one (`engine/core/state_context.h:136-144`). That precedent does not carry over.
It pairs an edge that a client cannot reconstruct afterwards, while a root
state reconstructs the requested hook point exactly. Structural types makes
`State` the interface through which the engine calls unknown game code
(`docs/design/PHILOSOPHY.md:489-490`). A state that owns the game's real stack
is that interface used as designed, not a workaround. T4 applies too: one way
to run code every frame is easier for a stranger to learn than two.

Capture and routing is declined. The boundary gives "Which actions exist, their
bindings, and any rebinding" to the game (`docs/design/PHILOSOPHY.md:449`), and
an action-mapping layer is known-absent on purpose (`AGENTS.md:634`). The policy
the report lists is the client's: priority, neutral entry, release and
fresh-confirm rules, chords, seat suppression. The report also concedes that
capture cannot tell typing a key from playing it. The mechanism under that
policy, cooperative gates over `const` raw snapshots, is already something a
client can write.

The decision does not rest on the missing second caller. A count "does not
settle a decision about what the library is for"
(`docs/design/PHILOSOPHY.md:792-798`). It rests on the client being able to
write the pattern from public calls.

**The change.** No new API.

- `engine/core/state_context.h`, class comment (after `:40`): one paragraph,
  contract only, with no history and no client named. It says a context can be
  a member of a state. A state that has to see every update and every draw,
  whatever is on top (a developer console, a frame-time overlay), is a root
  state that owns a `StateContext`:
  - its `update()` runs its own code, then calls the nested `update(dt)`;
  - its `draw()` calls the nested `draw(renderer)`, then draws over it;
  - its `on_activated` and `on_deactivated` forward to the nested
    `notify_activation(true)` and `notify_activation(false)`;
  - its `init()` calls the nested `notify_activation(this->context()->active())`,
    so a root built in the background does not report active.

  States inside it push and pop on the nested stack through their own
  `context()`. The root is the only frame on `Application`'s stack, so its own
  `covers_screen`, `on_suspend` and `on_resume` never matter. The paragraph
  also states two consequences. `Application::depth()` is 1 for as long as the
  root runs, so anything that asked `Application` for the depth asks the nested
  context instead. And a frame capture (report 10) goes in the root's
  `update()`, after the nested update has returned, never in a `draw()`. Any
  usage sketch is written as `//` lines: a `/* */` block fails the website's
  reference reader. The header is published word for word.
- `website/src/content/docs/docs/concepts/states.mdx`, under `## Patterns`
  (`:112`): a bullet, "Something that sees every frame is a root state that owns
  a stack", quoting the new test with `SourceFile`. The quote is located by the
  text of its first and last lines, so those lines in the test have to stay
  stable. It also says plainly what adopting late costs: every `push` and
  `transition_to` the game makes on `Application` moves to the nested context.
- `website/src/content/docs/docs/guides/input.mdx`, beside `## Binding tables`
  (`:84`): one or two sentences. There is no input capture, raw reads are never
  consumed, and a console gates its own readers. LineSweeper's binding table,
  which the page already quotes, is the model for routing input through one
  place.

Platforms: none are touched. The change is comments, website pages and a
headless test, with no backend involved.

**Tests.** `tests/core/state_context_tests.cpp`, in `CoreTests`, which is
headless and runs under every preset, `wasm-*` under Node included. One new
case: "a state that owns a StateContext is updated every frame whatever its own
stack holds". Model it on a root host that holds the game's stack. It pins:

- The root's own `update` runs while the nested stack has a pushed state, and
  the nested lower state is suspended.
- A `pop` issued inside the nested top's `update` is deferred until the nested
  `update` returns, while the root's frame is still live.
- `notify_activation` forwarded from the root reaches every nested frame, top
  first. This is the one piece every adopter writes.
- A root built while the application is inactive leaves the nested stack
  reporting inactive after `init()`.
- Draw order is the nested stack first, then the root's overlay. Use a
  recording `draw` against the null renderer only if the test target already
  links one. Otherwise record the order from the `draw` calls themselves.

**Consumer-visible.** Not consumer-visible. It is a comment and the website,
with no change in behaviour. The commit touches `engine/core/state_context.h`,
so its message says `not consumer-visible`. `samples/minimal/README.md` gets no
line.

**What the client could delete.** It could move its global forwarding, where
its menu base and its gameplay state each call into the overlay controller,
into one root state. That forwarding moves, it is not deleted. These stay
client policy:

- the cooperative gates;
- suppressing the release that closes the console;
- the guard on raw ownership of physical keys;
- keyboard-seat suppression;
- the policy for queued developer work;
- the per-menu update template and its pre-update hooks, which also drive
  music, motion, scripts and timing.

Adopting the root changes what `Application::depth()` returns. The client reads
it in some debug-command preconditions and in a status display, and those
would have to ask the nested context. The answer to the report should say so.

**Open questions.** None block the documentation. One would reopen the decision:
a client whose overlay needs code before the devices are polled, which no
`State` can get. That would be new evidence. Report 11 (system keys) and key
labels are out of scope, as the report itself says.

## 10. Capturing a frame from inside a running Application

**Decision.** Document only. No capture hook is added. The out-of-band frame,
`begin_frame`, the stack's `draw`, `submit`, `read_back_buffer` and no
`end_frame`, is already legal on every rasterising backend. Because `draw()` is
a pure read, it is exact. The record writes that frame down as the supported
shape, states the seam term it rests on, and says that a final `State` which
submits from its `draw()` breaks the contract.

**What the engine has now.** `Application::render` is private. It calls
`begin_frame`, the stack's `draw`, `submit`, `end_marker` and `end_frame`, with
no callback between them (`engine/app/application.cpp:309-329`,
`engine/app/application.h:323`). A second `submit` adds nothing
(`engine/render/renderer.h:383-387`). `read_back_buffer` is synchronous,
returns RGBA with the top row first, and is valid between `submit` and
`end_frame`. It stalls by construction and is "NOT A FRAME-PATH CALL"
(`engine/render/renderer.h:404-429`). The null backend refuses it
(`:412-416`).

The term that a frame submitted and never presented is legal, and leaves
nothing behind for the next one, is relied on but never stated where callers
read. `renderer.h:349-356` states only the half about a frame begun and never
submitted, and SEAM.md#6 matches that half
(`engine/render/SEAM.md:199`, "What each backend resets in `begin_frame`").
The submitted half lives in three places:

- a parenthesis in `engine/render/vulkan/device_resources.h:48-50` (in the bullet at `:39-50`) that cites
  `renderer.h` as its authority;
- the pixel harness, whose `end()` deliberately never presents
  (`tests/render/pixel_tests.cpp:376-386`), so every case begins after an
  unpresented, submitted frame;
- every capture caller in the tree. `samples/collision/main.cpp:78-91`,
  `samples/local_multiplayer/main.cpp:81-94` and
  `tools/linesweeper_capture/main.cpp:225-235` each write
  `begin_frame`, draw, `submit`, `read_back_buffer`, `end_frame` in a loop they
  own.

The alternative the client recorded, a non-covering final `State` that calls
`submit` and `read_back_buffer` from its `draw()`, runs, but it breaks the stated
contract. `engine/core/state.h:29-31` says a state "does not submit.
begin_frame / submit / end_frame belong to whoever owns the frame". `draw` is
also `const` and a pure read (PHILOSOPHY, Simulation and rendering,
`docs/design/PHILOSOPHY.md:526-528`), and that state writes pixels out through
a reference.

**Why.** `draw()` is "`const` all the way down"
(`docs/design/PHILOSOPHY.md:526`), so a second walk of the stack between two
updates produces exactly the frame the stack would draw. The redraw uses one of
the engine's own guarantees and is not an approximation. Its cost is one extra
draw when a debug command runs, which is off the frame path.

The public face test does not argue for a hook. The client already writes the
whole capture from public calls in about a dozen lines, and three programs in
this repository write the same lines. A hook would be a branch inside
`Application::render` on every frame, there only to serve a call made once.
That is "a customisation point that taxes the frame loop"
(`docs/design/PHILOSOPHY.md:139-142`). G74's scope check already says a capture
hook "must stay out-of-band" (`docs/product/GAP-REGISTER.md:1504`). T3 and T4:
one capture shape is enough. The hook would not remove the stall, the client's
flag that keeps the redraw out of its own measurements, or the PNG work, which
the report concedes. All it saves is one draw at a debug command.

The client's strongest argument is that its draw carries instrumentation, so
for that client the redraw is neither free nor exact. That is the client
writing inside `draw`, which Simulation and rendering forbids, and it does not
justify a branch in the engine's loop. The final-`State` pattern should not be
approved either. It puts `submit` inside a state's `draw`, against
`state.h:29-31`. It suspends the world beneath it, and it uses up an update.

The gap the report did find is real. The term "submitted and never presented
is legal" is relied on and not stated. Under T5 and T6, a contract callers
depend on is written where they read it.

**The change.** No new API. The work is three edits.

- `engine/render/renderer.h`, at `begin_frame` / `end_frame` (`:349-356`): add
  the missing half beside the existing paragraph, as contract. "A FRAME
  SUBMITTED AND NEVER PRESENTED IS LEGAL, and contributes nothing to the next
  one; the next begin_frame starts over." Point at SEAM.md#6 for what each
  backend resets. Do not count the backends: that is the G9 lesson, and
  `DeviceNotify` already settles rosters by citing `SEAM.md#8`. Then
  `engine/render/vulkan/device_resources.h:48-50`'s parenthesis cites a
  sentence that exists.
- `engine/render/renderer.h`, at `read_back_buffer` (`:404-429`): a paragraph
  saying that the supported way for a game to capture a frame from inside a
  running `Application` is to draw it again. Call `begin_frame`, then the
  stack's `draw` (`Application` is a `StateContext`, so
  `app.draw(*app.renderer())`), then `submit` and `read_back_buffer`, and do not
  call `end_frame`. Do it from an `update()`, after the update whose world it
  should show has finished, and never from a `draw()`. A capture issued halfway
  through the capturing state's own update draws a half-updated world. A
  capture issued during one step of a catch-up tick shows that intermediate
  world, not the one presented at the end of the tick. The paragraph promises
  the frame the stack would draw. It does not promise what was displayed. On
  the web the out-of-band frame draws into the canvas's default framebuffer
  inside the animation-frame callback, and `render()` then clears and redraws
  it before the browser composites, so keep the existing wording, which avoids
  "presented". Draw code that reads a clock or a counter breaks the pure-read
  premise, and the capture is then only as exact as that draw.
- `engine/core/state.h:29-31`: turn "it does not submit" into a contract
  sentence. A state that calls `begin_frame`, `submit` or `end_frame` from
  `draw()` is outside the contract, and a capture belongs in `update()`.
- `website/src/content/docs/docs/concepts/drawing.mdx`, inside or after
  `## The frame, from the engine's side` (`:151`): a short "Capturing a frame"
  section quoting `samples/collision/main.cpp:78-91` with `SourceFile`. Its
  first and last quoted lines then become load-bearing.

Platforms: no code changes, but the new contract case has to run on every
rasterising backend. Run `x64-debug` (d3d11), `x64-debug-d3d12`,
`x64-debug-gl` and `x64-debug-vulkan`. Run Vulkan again under the validation
layer with `validate_sync`, using the settings file in
`docs/review/vulkan/`'s section 2, because the case walks the acquire and
present path. Run `wasm-debug` as both `RenderPixelTests` and
`RenderPixelTestsWithoutS3tc`. Add the null case under `x64-debug-null` and
`x64-release-null`, and under CI's null reconfigure of `wasm-release`.

**Tests.** Two named contract cases. Each says it names the term rather than
adding coverage, because every pixel case already begins after an unpresented
frame.

- `tests/render/null_tests.cpp` (`RenderTests` under the `-null` presets).
  Write this one first. It is the one place where a leaked recording is
  directly observable without a rasteriser. Frame 1 draws sprites A, submits,
  and is never presented. Frame 2 begins, draws sprite B, and submits. Assert
  that frame 2's recording holds B alone.
- `tests/render/pixel_tests.cpp` (`RenderPixelTests`): "a frame submitted and
  never presented contributes nothing to the next". As proposed, the case
  could not fail. A full-frame red followed by a full-frame blue is all blue
  whatever leaked, so the case must be built to fail. Frame 1 draws a
  translucent sprite over a known clear, submits and reads back, with no
  `end_frame`. Frame 2 begins and draws the same translucent sprite at the same
  place. A replayed or doubled recording would composite twice and change the
  bytes: a submit flag not cleared, a d3d11 deferred list not drained, a
  Vulkan command pool not reset. Then walk the order an in-`Application`
  capture produces: an unpresented frame, a presented frame, then the next
  frame. Today only "a frame may be read back and then presented" reaches any
  part of that order. The case gains a golden PNG, and that image is looked at
  before it is committed.

**Consumer-visible.** Not consumer-visible. Behaviour does not change, and the
commit message says so because it touches `engine/render/renderer.h` and
`engine/core/state.h`. It does make binding a term that was only implied. A
future backend that broke it, such as Android's swapchain or another browser
context, would therefore be a silent behaviour change that needs its own
CHANGES.md entry. The `state.h` sentence puts the client's recorded final-State
alternative outside the contract, and the answer to report 10 has to say that
explicitly.

**What the client could delete.** Nothing. Its capture function stays, now
documented as the sanctioned shape rather than a workaround. Its flag that
keeps the redrawn frame out of its own performance and pacing records stays,
because the redraw repeats instrumentation the client keeps in `draw`. PNG
encoding, naming, paths and display policy stay. If the client moved to the
final-State alternative, it moves back.

**Open questions.** None block it. Asynchronous readback, and reading back what
was actually displayed, stay out of scope. The report agrees, and `renderer.h`
already says a readback after present is not portable (`:418-422`). If a
backend appears that cannot discard a submitted, unpresented frame, the new
stated term is what it will be held to, which is the point of stating it.

## 14. The last frame's phase timings

**Decision.** Accept, narrowed. `Application` publishes a read-only snapshot of
the CPU wall times of the last frame it completed:

- a sequence number;
- the fixed-step count;
- the interval since the previous frame;
- the `update`, `begin_frame`, `draw`, `submit` and `end_frame` durations.

This is the only part of the three reports a client cannot write itself, since
the loop is private. GPU time, distributions, counters and an overlay are
declined (T1, `TODO.md:108` REV#10).

**What the engine has now.** `Application` exposes no frame timing. `timer_` is
private (`engine/app/application.h:298`), and the public section has no
accessor for timing (`:126-266`). `Application` owns the fixed-step updates,
the device polls and the audio update, `begin_frame`, the stack's draw,
`submit` and `end_frame` (`engine/app/application.cpp:282-286`,
`:288-306`, `:309-329`). A render can follow zero, one or several updates,
because `StepTimer::Tick` catches up (`engine/core/step_timer.h:109`). `render`
returns before the first update (`engine/app/application.cpp:311-315`).
`reset_elapsed_time()` and `on_resuming()` both reset the step timer
(`:277-280`, `:405-407`).

Debug markers return no timings, and by contract a caller cannot observe them
(`engine/render/renderer.h:431-436`). How long a phase takes, and whether it
waits, is up to the backend:

- d3d11's `submit` finishes, executes and releases command lists
  (`engine/render/d3d11/renderer.cpp:989`, `FinishCommandList` at `:304`).
- d3d12's `begin_frame` waits for the GPU (`wait_for_frame` at
  `engine/render/d3d12/renderer.cpp:1223`).
- d3d11's present is `Present(1, 0)` and can block on display back-pressure
  (`engine/render/d3d11/device_resources.cpp:390-395`).
- SEAM.md#6, "What `submit` costs" (`engine/render/SEAM.md:217`), records how
  the split differs between backends.

The engine's own benchmark already writes exactly these brackets
(`bench/linesweeper_frame_bench.cpp:316-334`). It had to build its own shell
from a `Window` and a `Renderer` to get them (`:100-118`), because
`Application` gives a client no phase boundaries.

**Why.** Under The public face (`docs/design/PHILOSOPHY.md:795-796`), a client
cannot write this at all. `tick`, `update` and `render` are private
(`engine/app/application.h:322-334`). A game can time its own state update and
its own draw. It cannot time `begin_frame`, `submit` or `end_frame`, or see the
catch-up step count, without giving up `Application::run` and writing its own
shell, which is what `LineSweeperFrameBench` had to do.

There is a strict-T1 case for declining. With report 9's root state, a client
can time its whole update and its draw. The rest, begin, submit and end, then
shows up as one lump between the end of draw and the next update. That lump
mixes the work (record and submit) with the waits (`begin_frame` on d3d12, and
present). That mixing is exactly the confusion the ratchet document diagnosed
(`docs/performance/2026-09-19-g6f-ratchet.md`, sections 2 and 5): whole-frame
time misleads when two clocks meet, `update` and record-plus-submit measure the
work, and begin and present are where the waits land. So `begin_frame` is a
field of its own, separate from `draw`.

It is mechanism, the measurement of the engine's own phases, and so allowed by
T1. The ring buffer, the overlay, eligibility, deduplication and export stay
with the client. REV#10's overlay therefore stays deferred: its T1 condition,
"waits for a client that needs it", is met only for the half that observes.

A second need in this tree makes this owed by the engine rather than one
client's wish. G27 (`docs/product/GAP-REGISTER.md:736-750`) and SM-08
(`docs/product/STORIES.md:1899-1915`) ask for a way to time the 9,600-particle
top-out frame in a browser, on a phone. `LineSweeperFrameBench` is Windows-only
by construction (`bench/CMakeLists.txt:42-46`). A page script can see
animation-frame intervals, but not the CPU phases of a frame. A development
page that reads `frame_timings()` behind a query flag and logs it is that
instrument. It meets SM-08's console-logging acceptance without a browser
benchmark harness, which is what SM-08's own T1 line asks for.

T8 holds. The cost is a fixed handful of `steady_clock` reads and one small
struct write per tick, with no allocation and nothing per sprite or per object.
Nothing calls out, so it is not a customisation point.

Names follow T5 and CONVENTIONS ("Never: ... type information smuggled into a
name", `docs/design/CONVENTIONS.md:201`): durations are
`std::chrono::nanoseconds`, not `_ns` or `_ms` integers. The T3 narrowing:

- the last frame only, with no history;
- no separate valid flag, because `frame == 0` means none yet;
- nothing for a failed or abandoned frame, because a frame that throws never
  publishes.

GPU timestamps are declined for this slice. They would need a seam method on
five backends, query pools and a frame of latency, and the null backend would
have nothing to answer with.

**The change.** In `engine/app/application.h`, beside `ApplicationOptions`
(`:41`). The field comments are published word for word, so they state the
contract and never cite the ratchet as history:

```cpp
// What the last completed frame cost on the CPU, as steady_clock saw it.
// draw_time and submit_time together are the work; how it splits between
// them is the backend's. begin_frame_time and end_frame_time may contain
// waits for the GPU or the display. Not GPU time, not CPU busy time, and the
// fields need not sum to interval.
struct FrameTimings
{
    std::uint64_t frame = 0;                     // completed frames since run(); 0 until the first
    int updates = 0;                             // fixed steps this tick ran: 0, 1 or several
    std::chrono::nanoseconds interval{};         // this tick's start minus the previous frame's; zero on the first and after a discontinuity
    std::chrono::nanoseconds update_time{};      // the whole catch-up: polls, every stack update, audio
    std::chrono::nanoseconds begin_frame_time{};
    std::chrono::nanoseconds draw_time{};        // StateContext::draw, and any fan-out it joins
    std::chrono::nanoseconds submit_time{};      // submit and end_marker
    std::chrono::nanoseconds end_frame_time{};
};
```

On `Application` it adds `FrameTimings frame_timings() const;`. The name is a
noun accessor, and the result is returned by value, not by reference. The
struct is about 64 bytes. A reference would change under its reader every
frame, while a copy is safe to read from `update` or from draw workers. The
contract comment says:

- During update N, it describes frame N-1. It is written between frames.
- A frame that throws publishes nothing.
- A tick that renders nothing, before the first update, publishes nothing.
- Renderer calls a game makes itself, such as report 10's out-of-band capture,
  are not frames here. Time spent on them inside an update is part of
  `update_time`.
- Clock resolution is the platform's. In a browser `steady_clock` is
  `performance.now()`, which may be coarsened to about 100 µs without
  cross-origin isolation, so short phases can read as zero there. Say so; do
  not hide it.
- Which phase contains the waits is the backend's business (SEAM.md#6). Do not
  list the backends.

The implementation lives in `engine/app/application.cpp`, in `tick()` and
`render()`. They take time points and pass them to a small recorder in a new,
unpublished header, `engine/app/frame_timer.h`, which the reference leaves out
the way it leaves out `engine/core/step_timer.h`. The header is named as left
out in `website/src/lib/published.ts` (beside `:241`) in the same commit, or
the site build fails.

The recorder takes time points as arguments, so it can be tested without a
clock. `reset_elapsed_time()` and `on_resuming()` mark a discontinuity. The
interval is measured at tick entry, decided once and written down. On the web
that is the start of the animation-frame callback, and the browser's present
lies outside the tick. Keep the number of `steady_clock::now()` calls fixed:
under Emscripten each one is a call out to `performance.now()`.

One implementation serves the Win32 and web windows, because both reach
`Application::tick` through `WindowNotify`. Nothing under `engine/render/`
changes and no backend is touched. `application.cpp` is shared, so the change
still has to compile and pass under all five render configurations and both
`wasm-*` presets (clang, `-Werror`), with no `auto` in engine code.

This becomes its own story under E10, as PI-10 requires, size M, and it names
SM-08 as a dependant.

**Tests.**

- `AppTests`, headless under every preset, `wasm-*` under Node included. The
  recorder is fed synthetic time points, and the case checks that:
  - `frame` starts at 0 and goes up once per published frame;
  - `interval` is zero on the first frame and after a discontinuity, and is
    otherwise the difference between the injected tick starts;
  - `updates` reports 0 and 3 correctly;
  - each phase equals its injected bracket;
  - a frame begun but never published, standing in for a throw, leaves the
    previous snapshot unchanged.
- An `Application` loop case under `x64-debug-null` and `x64-release-null`, with
  a hidden window, the null renderer and null audio, so it runs on a GPU-less
  build machine. It calls `run()` with a state that records `frame_timings()`
  in `update` and quits after N frames, then asserts that:
  - `frame` goes up by exactly one between consecutive rendered frames;
  - every duration is non-negative;
  - a manual `begin_frame`, `draw`, `submit` inside `update` does not advance
    `frame`.

  No test in `tests/app/` constructs an `Application` today
  (`tests/app/CMakeLists.txt:7-21`), so this is new ground. Under a `wasm-*`
  preset, null or not, the case cannot run, because `run()` does not return in
  a browser. The web tick path is therefore covered only by the recorder test,
  plus an optional frame-sequence assertion in `WebWindowTests` or a
  `*PageTests` page.
- No `Benchmarks` assertion: this adds no complexity class and no wall-clock
  check. `LineSweeperFrameBench` is unaffected, because it does not use
  `Application`.

**Consumer-visible.** Yes, additive. `FrameTimings` and
`Application::frame_timings()` are new public names in an existing published
header. The change needs a CHANGES.md entry under `## Unreleased`, and no
migration, because nothing breaks. `samples/minimal/README.md` gets a line only
if minimal reads the snapshot, and it should not need to.

**What the client could delete.** Its paired stopwatch around the match draw,
and the draw-start state that feeds it. It could also drop its stopwatch around
the nested level update, if its overlay moves to the meaning of `Application`'s
update. It gains the `submit` / `end_frame` split, which it cannot measure
today. These stay client policy:

- its performance ring, capture and export;
- object counts;
- the interval snapshot it keeps safe for workers;
- the overlay;
- the hook through which draw data is ingested, for eligibility and
  deduplication;
- its own per-level update and debug-clock counts, because one `Application`
  update can run several level updates;
- level-build timing;
- the policy that leaves capture frames out of its records, because a capture
  inside `update` lands in `update_time`.

**Open questions.**

- Readers will add the fields up, and will quote `end_frame_time` as the cost
  of a frame. The header has to lead with "draw and submit together are the
  work, and begin and end may contain waits", as rationale and not as history.
  The alternative is one `record_submit_time` field, in the bench's and the
  ratchet's vocabulary. Keeping both fields under that lead sentence is the
  recommendation. The implementer settles it.
- Confirm the T8 cost claim with a before/after run of the null-preset loop,
  even though no assertion is added.
- A GPU-time field later would be a seam change across five backends, and a
  separate request, as the report itself says.
