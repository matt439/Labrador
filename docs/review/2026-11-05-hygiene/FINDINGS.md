# Hygiene review findings, 2026-11-05

Every finding kept by the [review](README.md) at `5cbc6b3`, grouped by its
theme and ordered by severity, then path. Each entry gives the reviewer's
title, why it matters, the suggested fix and the skeptic's verdict. Line
numbers describe that revision. [findings.json](findings.json) also holds the
verbatim evidence and the skeptic's reasoning.

## H1. Engine behaviour and contract fixes outside render

- **medium** · convention · `engine/assets/sound_bank_loader.cpp:38` —
  Sound-bank definitions silently ignore unknown keys, unlike the manifest and
  sprite-sheet loaders.
  All three are hand-edited, fixed-schema content files. A typo'd extra field
  in an effect instance, such as a misspelled volume or looping key a person
  expects to work, is accepted silently. That is the quiet disagreement T6
  forbids, and the sibling loaders and their headers argue against it
  explicitly. *Fix:* Apply reject_unknown_keys to the definition root and to
  each effect instance, document it in sound_bank_loader.h's throw contract,
  update json_object_keys.h's and published.ts's 'manifest and sprite-sheet
  loaders' wording, and add tests to match the other two loaders.
- **medium** · bug · `engine/audio/null/audio_device.cpp:167` — Null play_voice
  ignores DirectXTK's Play transitions it claims to transcribe.
  Suppose a voice is paused (or playing) without a loop and play_effect(effect,
  true) is called. The null backend then reports is_effect_looping()==true,
  while XAudio2 and Web Audio report false. The null backend exists so headless
  tests can assert what a real backend would do, so this divergence can pass a
  test that a real device would fail. *Fix:* Mirror the web/DirectXTK branches.
  Paused goes to playing with looping unchanged. Playing is left unchanged.
  Only stopped goes to playing with looping = loop. Keep recording the call,
  and add a null test for play_voice on a paused looping/non-looping voice.
  *(Severity corrected by the skeptic.)*
- **medium** · backend-divergence ·
  `engine/audio/webaudio/audio_device.cpp:297` — One-shots during suspension
  are dropped on Web Audio but queued by XAudio2; the seam says nothing.
  Application keeps ticking updates while minimised. Native one-shots fired
  then are started on a stopped XAudio2 engine and plausibly all play together
  at resume, which is the 'plays late plays wrong' outcome the seam rejects
  elsewhere. The browser drops them instead. Callers get different audible
  behaviour per backend, and nothing documents it. *Fix:* Decide the rule in
  audio_device.h (most likely: a one-shot asked for while suspended is dropped)
  and implement it in XAudio2 with a suspended flag, which DeviceRecovery
  already tracks. Then document it beside the gesture paragraph and in
  SoundBank::play_wave.
- **medium** · error-handling · `engine/audio/xaudio2/audio_device.cpp:120` —
  XAudio2 accepts a streaming wave bank at load and later fails with an
  anonymous error.
  audio_device.h promises that a container this backend can read but not play
  fails at open_wave_bank with a runtime_error naming the file or wave (T6). A
  streaming bank (xwb_file.h says the engine's reader accepts 'in-memory or
  streaming') opens without error on XAudio2. It then throws
  std::runtime_error("WaveBank::CreateInstance") from create_voice, or
  "WaveBank::Play" from the const play_wave path, during gameplay. Neither
  message names the bank or the wave. The Web Audio backend plays the same
  file, so the backends also disagree on the same content. *Fix:* After
  constructing the WaveBank, check bank->IsStreamingBank() and throw
  std::runtime_error("Failed to open wave bank: " + path + " is a streaming
  bank; re-pack it in-memory"), or whatever the decided policy is. Add an
  XAudio2-only test with a streaming fixture. *(Severity corrected by the
  skeptic.)*
- **medium** · bug · `engine/audio/xaudio2/audio_device.cpp:181` — play_wave
  out of range is silently dropped on XAudio2 but throws on null and Web Audio;
  the null comment says otherwise.
  SoundBank::play_wave validates only handle.valid(), not that the index
  belongs to this bank. A WaveHandle resolved from a larger bank and passed to
  a smaller one throws std::out_of_range under null audio (CI) and Web Audio,
  but plays nothing with no error on the shipping Windows backend. This is
  exactly the 'silence with no error' that sound_bank.h says the engine exists
  to prevent. The null backend's comment justifying its throw describes the
  real backend wrongly. *Fix:* Give the XAudio2 play_wave (and create_voice,
  before the unsigned cast) the same bounds check and std::out_of_range message
  the other two backends use, for example by checking against WaveBank's entry
  count. Correct the null comment to say the check is the seam's, not the
  API's.
- **medium** · bug · `engine/collision/broad_phase.cpp:47` — Grid cell
  arithmetic is undefined for nonfinite or very large boxes.
  Converting a NaN or out-of-range float to int32_t is undefined behaviour. The
  subtraction max_x - min_x + 1 is also done in int32 and can overflow before
  the widening cast. narrow_phase.h makes nonfinite coordinates report no
  contact, and the all-pairs sweep (the specification) handles them that way.
  The grid, which is supposed to reproduce exactly the same pairs, has no
  defined behaviour for such a box. Nothing in
  tests/collision/broad_phase_tests.cpp covers NaN, infinity or huge
  coordinates. *Fix:* In collect_candidates, skip boxes with nonfinite fields
  (they can produce no contact anyway). Do the cell division in double and
  clamp it to the int32 range, or route out-of-range boxes to large_. Widen to
  int64 before subtracting. Add a randomised agreement case that includes NaN
  and huge coordinates.
- **medium** · error-handling · `engine/core/byte_reader.cpp:75` —
  read_file_bytes reports any failure to open as 'No file at', which
  contradicts its documented split between a missing file and an unreadable
  one.
  An existing file that cannot be opened (access denied, locked, or a
  directory) throws out_of_range with "No file at". That is exactly the
  'content bug' classification the header says is reserved for a missing name.
  The callers are dds_file, png_file, sprite_font_file and xwb_file, so a disk
  or permission problem gets misreported as a missing asset. *Fix:* After the
  open fails, test std::filesystem::exists(path_from_utf8(path)) with an
  error_code: throw out_of_range only when the file does not exist, and
  runtime_error ("Could not open ...") otherwise. Add a test that opens a
  directory or a locked file.
- **medium** · error-handling · `engine/core/thread_pool.cpp:75` — ThreadPool
  ignores SetThreadpoolThreadMinimum's failure and never validates its min/max
  arguments.
  SetThreadpoolThreadMinimum returns a BOOL that is FALSE when it fails, for
  example when it cannot create the minimum number of threads. That failure is
  silently lost. A direct caller passing ThreadPool(0, 0), ThreadPool(-1, 2) or
  ThreadPool(4, 2) gets negative values cast to huge DWORDs, or an inconsistent
  floor and ceiling, with no diagnostic. The wasm Impl stores the values
  unchecked, and the problem only shows up later, as Partitioner::partition's
  throw when max_num_threads() is below 1. The engine's other public
  constructors (FocusGroup, Partitioner, StateContext) throw
  std::invalid_argument at the boundary, so this one breaks the pattern. *Fix:*
  In ThreadPool::ThreadPool, before either Impl is built, throw
  std::invalid_argument unless 1 <= min_num_threads <= max_num_threads. That
  covers the Win32 and serial builds alike. In the Win32 Impl, check
  SetThreadpoolThreadMinimum's BOOL, and on failure close the pool and throw,
  as the CreateThreadpoolCleanupGroup failure path already does. Drop the
  restating end-of-line comments.
- **medium** · interface · `engine/input/gamepad_reader.h:51` — The seam
  promises suspended slots read disconnected, but XInput does nothing on
  suspend.
  The published contract states a guarantee and then disclaims it. The web
  backend keeps it with a `suspended` flag that blanks states in sample(). The
  XInput backend has no flag, and on Windows 10+ DirectXTK's Suspend is a
  no-op, so read() keeps reporting connected pads with live buttons while
  suspended. Even XInputEnable(FALSE) yields neutral-but-connected data, not
  disconnected. Callers cannot rely on the stated sentence, and the two
  backends diverge on observable behaviour. *Fix:* Mirror the web backend: add
  `bool suspended` to the XInput Impl, set it in suspend()/resume(), and have
  sample() write default GamepadStates while suspended. Then cut the
  disclaiming paragraph so the header states one true contract. Alternatively,
  file it as a behavioural decision per code-hygiene.md's rule for false
  contracts.
- **medium** · const-correctness · `engine/ui/focus.h:159` —
  FocusGroup::refresh_style is const but writes every registered widget's
  colour.
  In this engine, const signals a read that is safe to make from draw() on
  several render workers at once (game_object.h, state.h). This function
  mutates the borrowed widgets that those draws read. A page's const draw()
  could call it and race with other views without the compiler objecting. It
  compiles only because const does not propagate through the stored raw
  pointers. Its only callers are FocusGroup's own non-const mutators
  (focus.cpp:36,75,133,155), and git grep finds no caller outside focus.cpp, so
  the const qualifier enables nothing. *Fix:* Drop the const qualifier:
  refresh_style() is a mutation of what the group paints. No caller changes,
  because every existing call site is already non-const.
- **low** · ownership · `engine/app/application.cpp:97` — ~Application calls
  CoUninitialize before the audio device, renderer and window members are
  destroyed.
  The XAudio2 engine (opened under the COM apartment this class initialised)
  and the other Windows services are released after COM is uninitialised on
  this thread. That order is unsupported. The browser on_closed path (lines
  566-578) already resets each service explicitly in reverse order, so Windows
  teardown is less strict than the browser's. Impact is plausible rather than
  observed. *Fix:* Before CoUninitialize on Windows, reset the service members
  in the same reverse order the on_closed path uses (sharing one helper), or
  move COM initialisation into an RAII member declared before the services so
  it is torn down after them.
- **low** · bug · `engine/app/application.cpp:813` — window(), quit(),
  set_resolution and set_fullscreen dereference window_ unguarded, while
  set_cursor_* guard it.
  Before initialize(), window_ is null. The header says every service accessor
  answers null until then, but window() (and quit()) crash instead. Sibling
  setters on the same class handle the pre-initialize case deliberately ("a
  call before the window exists is the option it would have been"), so the
  class is inconsistent about calls made before initialize. *Fix:* Return
  nullptr from window() when window_ is null, and make quit() a no-op then.
  Alternatively, state in application.h that these members require
  initialize().
- **low** · other · `engine/app/web/window.cpp:471` — Two getBoundingClientRect
  calls per mouse or pointer event, including page-wide mousemove.
  client_position runs for every mousemove on the page's window (the listener
  is on EMSCRIPTEN_EVENT_TARGET_WINDOW, line 395), every mouse down/up and
  every touch pointer event. Each call makes two JS calls, and each of those
  builds a DOMRect and may force a layout. This engine targets low-end
  hardware, and the doubled layout query on the busiest input path is
  avoidable. *Fix:* Replace the two EM_JS functions with one that writes left
  and top through out-pointers, or returns both in a single call, so each event
  does one getBoundingClientRect.
- **low** · error-handling · `engine/app/web/window.cpp:881` — Closed-window
  guards are uneven across Window's methods.
  enter_fullscreen and set_cursor_visible return early on a closed window.
  resize_client and leave_fullscreen do not: they still measure the canvas and
  call notify->on_window_size_changed. APPLICATION.md section 7 says the closed
  window "listens to nothing", but these calls go on reporting into the owner
  between close() and on_closed. The Win32 window has the same split:
  set_cursor_visible and set_cursor_confined check `handle != nullptr`
  (win32/window.cpp:576, 585), while resize_client, enter_fullscreen and
  leave_fullscreen (501-571) pass a possibly null handle to SetWindowPos,
  IsWindowVisible and MonitorFromWindow. *Fix:* Give every mutating Window
  method the same early return: `if (closed) return;` on the web, `if (handle
  == nullptr) return;` on Win32. Then state in window.h that every call after
  close() does nothing, as close() already says of a second close.
- **low** · bug · `engine/assets/json.cpp:235` — JsonValue::number narrows any
  JSON number to float with no range check.
  json.h promises that every read is checked and that a content mistake throws
  naming the file and key (T6). A hand-typed value such as `"frame_time": 1e40`
  passes IsNumber, then becomes +inf, which is formally undefined for an
  out-of-range double-to-float conversion, with no diagnostic. It then reaches
  AnimationStrip and sprite origins. The narrow phase works hard to keep
  nonfinite values out of positions, while this reader produces them. *Fix:*
  Read GetDouble(), and fail("is out of range for a float") when the result is
  not finite or exceeds FLT_MAX. Document this next to `number` in json.h.
- **low** · interface · `engine/assets/json.h:16` — read_json_file's documented
  exceptions omit the null-path std::invalid_argument.
  The neighbouring parse_json documents its std::invalid_argument precondition,
  and this one does not. A caller catching std::runtime_error, as
  ResourceLoader's optional-bank path does, would get an exception type the
  contract never mentioned. *Fix:* Add "Throws std::invalid_argument for a null
  path" to the header comment, matching parse_json.
- **low** · const-correctness · `engine/assets/resource_loader.h:113` —
  reload_device_resources and the load_* helpers are const but fill the stores.
  The engine uses const to mean 'no observable writes': draw() is const all the
  way down, and concurrent const calls are assumed safe. A const method that
  rebuilds every GPU texture in the registry advertises a safety it does not
  have. It compiles only because the stores are held through non-const
  pointers, which is shallow const. *Fix:* Drop const from
  reload_device_resources and the four private load_* helpers. Callers already
  hold a non-const ResourceLoader.
- **low** · swallowed-error · `engine/audio/webaudio/audio_device.cpp:131` —
  Empty JS catch in detach() swallows every source.stop() error without saying
  which one it expects.
  The expected case is an InvalidStateError from stopping a source that was
  never started, but the catch also hides any other failure from stop(). Its
  neighbour at line 275 (`catch (refused)`) explains itself; this one has no
  comment. *Fix:* Add a one-line reason, and if practical rethrow anything
  other than InvalidStateError (`if (ignored.name !== 'InvalidStateError')
  throw ignored;`).
- **low** · swallowed-error · `engine/audio/xaudio2/audio_device.cpp:123` —
  Both wave-bank opens turn every std::exception, bad_alloc included, into the
  runtime_error that triggers silent optional-bank substitution.
  An out-of-memory (std::bad_alloc) or any other non-file failure while opening
  an optional bank is relabelled as 'container missing' and handled as a
  stated-optional asset. It shows up as a warning and silent audio instead of
  stopping the game. The XAudio2 path also drops the original what() entirely.
  resource_loader.cpp says only a missing, unreadable or unplayable container
  may be substituted, which these catches do not enforce. *Fix:* Catch only the
  reader's or DirectXTK's file and format failures (std::runtime_error and the
  reader's out_of_range for 'no file'), or rethrow std::bad_alloc unchanged
  before the catch-all conversion. Keep the original what() on the XAudio2 path
  as well. *(Severity corrected by the skeptic.)*
- **low** · other · `engine/collision/partitioner.cpp:22` — partition() returns
  a fresh vector, so Scene::draw allocates on the heap every frame.
  broad_phase.h cites PHILOSOPHY: "per-frame code performs no heap allocation",
  and the collision module goes out of its way to reuse buffers (find_contacts,
  BroadPhase::pairs_buffer). The fan-out helper in the same folder returns by
  value and is called once per Scene::draw with more than one view, so every
  split-screen frame allocates. It also never calls reserve. *Fix:* Have
  partition fill a caller-owned vector (clear, then push) that Scene keeps as a
  member, or return a fixed-capacity range array. The count is bounded by the
  view capacity. *(Severity corrected by the skeptic.)*
- **low** · ownership · `engine/core/byte_reader.h:29` — ByteReader borrows its
  buffer by const reference but accepts temporaries.
  `ByteReader reader(read_file_bytes(path), path);` compiles and leaves bytes_
  dangling as soon as the statement ends. That is exactly the
  read-past-a-heap-allocation this class exists to prevent. Every current
  caller (xwb_file.cpp:348, dds_file.cpp:147, sprite_font_file.cpp:54 and the
  tests) passes an lvalue, so the contract currently holds only by convention.
  The header is published as game code (published.ts:120). *Fix:* Add
  `ByteReader(std::vector<unsigned char>&&, const std::string&) = delete;` so
  the compiler enforces the lifetime rule.
- **low** · interface · `engine/core/byte_reader.h:56` — read_file_bytes'
  documented exceptions omit the std::invalid_argument that path_from_utf8
  throws.
  A caller that catches the two documented types, for example to report a
  missing asset, is not told about a third type that escapes from the same
  call. The published contract is incomplete. *Fix:* Add to the comment that a
  path containing an embedded NUL throws std::invalid_argument (file_path.h).
- **low** · bug · `engine/render/colour.cpp:86` — 0/0 in Colour division yields
  NaN, which clamp01 passes through, breaking the [0,1] invariant.
  The header promises 'each channel clamped to [0, 1]'. Colour::black /
  Colour::black, or any zero-channel divisor, produces a NaN channel that
  reaches vertex colours on every backend. Under /fp:precise the NaN
  propagates, and its effect on the pixel is backend-defined. *Fix:* Make
  clamp01 map NaN to 0, for example `!(value > 0.0f) ? 0.0f : ...`.
  Alternatively, document and test that division by a zero channel is
  undefined, or remove the Colour-by-Colour division operators if nothing uses
  them.
- **low** · other · `engine/render/sprite_geometry.cpp:251` —
  sprite_world_bounds always calls cos/sin, unlike quad_bounds, although it
  runs per object per view.
  The file's own stated T8 rationale is ignored on the path that actually
  serves the cull. It is a small, needless cost per widget per view per frame.
  *Fix:* Short-circuit rotation == 0.0f (cosine = 1, sine = 0), matching
  quad_bounds.
- **low** · error-handling · `engine/render/viewport_manager.cpp:140` —
  Out-of-range player index silently returns a fullscreen viewport; dividers
  throw for the same class of mistake.
  renderer.h:379-380 rejects exactly this: 'asking for a view nobody set up
  must not quietly return a fullscreen pane and draw a whole extra pass (T6)'.
  ViewportManager::player_viewport does it anyway, and viewport_manager.h never
  says so. The fallback is pinned by
  tests/render/viewport_manager_tests.cpp:124-141, which only checks that it is
  not the next layout's answer. all_viewports() takes no argument, so the '*4*'
  comment describes a call that cannot be made. *Fix:* Throw std::out_of_range
  for an index outside viewport_count_from_layout. Update the test to
  CHECK_THROWS, document the throw in the header, and delete the stale
  all_viewports comment. *(Severity corrected by the skeptic.)*
- **low** · interface · `engine/ui/focus.cpp:58` — FocusGroup's throw on an
  out-of-range slot is stated only in the .cpp, and set_focused applies it
  inconsistently.
  CONVENTIONS requires a contract to live at the declaration. focus.h's
  focused/set_focused/move/activate say nothing about std::out_of_range for a
  bad slot. set_focused checks the widget first, so a bad slot with a foreign
  widget returns false while the same bad slot with a held widget throws. The
  same caller mistake gets two different answers. *Fix:* Validate the slot
  first in set_focused, as focused() effectively does. In focus.h, add one line
  to the slot-taking members: "Throws std::out_of_range if slot is outside [0,
  slot_count())." Move the .cpp rationale there. *(Severity corrected by the
  skeptic.)*
- **low** · error-handling · `engine/ui/widget.cpp:67` — UiContainer::add_child
  states a non-null precondition but dereferences null immediately.
  Passing nullptr is undefined behaviour (a null dereference inside
  child->name()). There is no named diagnostic. The sibling registration API in
  the same module, FocusGroup::add, rejects null with std::invalid_argument
  (focus.cpp:26-29), as do Scene::remove and StateContext::push. T6-style loud
  failure is the module's norm, and this public entry point is the exception.
  *Fix:* Add `if (child == nullptr) throw
  std::invalid_argument("UiContainer::add_child: null child");`, change the
  header comment to say it throws, and add a widget_tests case for it.
  *(Severity corrected by the skeptic.)*

## H2. Untrusted input and exception safety

- **medium** · bug · `engine/app/win32/window.cpp:1099` — window_proc lets game
  and renderer exceptions escape into user32, against window.h's promise that
  they come out of pump_until_quit.
  Messages sent by the system (ShowWindow/SetWindowPos in resize_client, and
  DefWindowProc's modal size/move loop dispatching WM_PAINT) call window_proc
  through user32 and kernel callback frames. A C++ exception unwinding through
  them is either silently swallowed or ends the process, depending on the
  process's callback-exception policy. So a device loss or a state's update()
  throwing during a window drag is lost or crashes the game instead of reaching
  main's handler. The public contract is false for exactly the cases the
  backends try to avoid. *Fix:* Mirror the web window's guarded(): wrap the
  body of window_proc (or each notify call) in try/catch, store the first
  std::exception_ptr in Impl, call DestroyWindow/PostQuitMessage, and
  std::rethrow_exception it from pump_until_quit after the loop ends. Then the
  backend comments' 'nowhere to catch it' becomes false and can be removed or
  reworded. *(Severity corrected by the skeptic.)*
- **medium** · bug · `engine/assets/json.cpp:56` — JSON is parsed with
  rapidjson's recursive parser, so deep nesting overflows the stack instead of
  throwing.
  With the default flags, rapidjson recurses once per nesting level (ParseValue
  -> ParseArray/ParseObject). A small file of about 100,000 '[' characters
  overflows the 1 MB native stack, and wasm uses -sSTACK_SIZE=1048576
  (cmake/settings.cpp:82 is settings.cmake). The process crashes. json.h
  promises std::runtime_error naming the source for text that does not parse,
  and the loaders exist so that a hand-edited content file never crashes (T6).
  Both read_json_file and the public parse_json pass through this line. *Fix:*
  Parse with document.Parse<rapidjson::kParseIterativeFlag>(text, length),
  which uses a heap stack and reports errors normally. Optionally add a depth
  limit. Add a json_tests case that feeds 100,000 nested '[' to both entry
  points and expects runtime_error. *(Severity corrected by the skeptic.)*
- **medium** · bug · `engine/assets/sprite_sheet_loader.cpp:105` — An animation
  strip's frame_count is unbounded, so one JSON number allocates up to 32 GB
  and fails without the path.
  Only negative counts are refused. Input:
  {"name":"a","first_frame":{"x":0,"y":0,"w":0,"h":0},"frame_count":2147483647,"frame_time":0.1,"looping":false}.
  With w = 0 every x stays in range, so calculate_frame passes.
  animation_strip.cpp:79 then runs rects.reserve(2^31) of 16-byte RectangleI,
  which is 32 GB, and loops 2^31 times. The result is std::bad_alloc natively,
  or std::length_error on wasm32, neither of them the runtime_error naming path
  and strip that sprite_sheet_loader.h promises. The catch above it handles
  only out_of_range. byte_reader.h names exactly this shape ('the one place a
  corrupt file can ask for gigabytes'), and the binary readers guard against
  it. *Fix:* Bound frame_count by something the content can justify, such as a
  named maximum or the frame width against the atlas. Alternatively, stop
  caching every rect and compute frames on demand. Add a loader test with
  frame_count 2147483647 and w 0. *(Severity corrected by the skeptic.)*
- **low** · bug · `engine/app/win32/window.cpp:407` — A notify handler throwing
  during the constructor's SetWindowPos/ShowWindow leaves a live HWND whose
  user data points at freed Impl.
  WindowNotify is a public interface, and Application's on_window_size_changed
  throws std::invalid_argument for a non-positive size. If a handler throws and
  the exception makes it back out of ShowWindow, ~Impl never runs. The window
  is not destroyed, the class stays registered, and every later message to that
  window dereferences a dangling Impl*. The constructor's own comment shows the
  author knew ~Impl does not run, but the cleanup covers only one throw site.
  *Fix:* After CreateWindowExW succeeds, wrap the rest of the constructor in
  try/catch: clear GWLP_USERDATA, DestroyWindow, UnregisterClassW, then
  rethrow. Alternatively, defer notify delivery until construction completes,
  or rely on the guard from the window_proc finding. *(Severity corrected by
  the skeptic.)*
- **low** · robustness · `engine/assets/json_object_keys.h:38` — Fixed-schema
  loaders reject unknown keys but silently accept duplicate ones; the first
  value wins.
  keys() keeps duplicates, as json_tests.cpp:209 pins, and the accessors use
  FindMember, which returns the first. {"position":{"x":1,"x":2,"y":0}} in a
  sprite sheet or a repeated "optional" in a manifest group therefore loads
  using the first value, with no message. That is the 'quiet disagreement with
  a content file' sprite_sheet_loader.cpp:55-64 argues against. Unknown keys
  are refused, so a duplicate is the remaining silent hand-edit mistake. *Fix:*
  In reject_unknown_keys, also fail with where() + " repeats key '<k>'" when a
  name appears twice. Add sprite-sheet and manifest tests.
- **low** · bug · `engine/core/byte_reader.cpp:87` — The file size is narrowed
  to size_t unchecked, then the full streamoff is read into the possibly
  smaller buffer.
  On a 32-bit size_t (wasm32), a file of 4 GiB + N bytes gives an N-byte
  vector, and file.read is then asked for the full 64-bit size. That is a heap
  overrun, and the only reader in the engine without a checked bound. MEMFS
  makes such a file unlikely in the browser, but NODEFS and NODERAWFS test runs
  and any future 32-bit port do not. *Fix:* Throw runtime_error naming the path
  when size > max of size_t (or above a named content limit), and pass the
  vector's own size to read().
- **low** · robustness · `engine/render/png_file.cpp:29` — A PNG is decoded
  with no size limit, and the dimension check after decoding cannot fire.
  lodepng allocates width*height*4 from the IHDR before the engine looks at the
  dimensions. A zero-filled 30000x30000 PNG compresses to about 1-2 MB and asks
  for 3.6 GB, which is a decompression bomb. The result is bad_alloc or lodepng
  error 83 on wasm, after a long inflate. The width > INT_MAX test at line 38
  runs only after a successful decode of that size, so it is effectively dead.
  The .dds and .spritefont readers check declared sizes against the file before
  allocating; this reader does not. *Fix:* Call lodepng_inspect first, or use a
  lodepng::State with decoder.zlibsettings.max_output_size set. Refuse
  dimensions above a named texture limit before decoding, naming the path. Add
  a hostile-IHDR test to png_file_tests.cpp. *(Severity corrected by the
  skeptic.)*
- **low** · test-coverage · `tests/assets/json_tests.cpp:96` — Hostile-JSON
  shapes (deep nesting, out-of-float-range numbers) and the PNG size bomb are
  untested.
  json_tests.cpp covers NULs, BOMs, types and positions. It has no case for
  nesting depth or for a number outside float range, the two inputs that crash
  or produce non-finite values above. png_file_tests.cpp:154 tests only non-PNG
  bytes, not a valid PNG declaring huge dimensions. Without these tests, fixing
  the findings above would not be pinned. *Fix:* Add a deep-nesting case
  through parse_json and read_json_file, a number("x") case with 1e39, and a
  png_file_tests case with a crafted IHDR (for example 40000x40000) expecting
  runtime_error before allocation. *(Severity corrected by the skeptic.)*
- **low** · test-coverage · `tests/audio/xwb_file_tests.cpp:433` — Several .xwb
  header refusals have no test.
  xwb_file.cpp refuses these by name, and no subcase covers them: bank data
  shorter than 96 bytes (line 92), entry_size < 24 (line 111), a name table too
  small for entry_count*name_size or a zero name_size (line 431), zero channels
  or zero sample rate (line 239), and 8-bit or 16-bit PCM whose length is not
  whole frames (line 262). These are the header fields a hostile bank controls;
  a later edit could drop any one of them silently. *Fix:* Add a subcase per
  check using the existing build_bank and poke helpers, each asserting the
  message fragment.
- **low** · test-coverage · `tests/render/dds_file_tests.cpp:114` — Most of the
  .dds reader's named refusals are untested.
  dds_file.h promises named throws for volume textures, the DX10 header and
  unknown fourCCs. Only the cube map, no-alpha, too-many-mips and large-width
  cases are exercised. These have no test: the DX10 throw (dds_file.cpp:94), an
  unknown fourCC, a volume texture via caps2 or depth > 1, a header_size other
  than 124, a pixel_format_size other than 32, zero width or height, a file
  truncated inside the header, and the DXT2->BC2 and DXT4->BC3 collapse the
  header documents. A regression in any of them would pass CI. *Fix:* Add
  poke-based subcases on quad_bytes() for each listed field, each checking
  runtime_error and that the message names the path. Add mapping checks for
  DXT2 and DXT4 fourCCs. *(Severity corrected by the skeptic.)*
- **low** · test-coverage · `tests/render/sprite_font_file_tests.cpp:114` — The
  spritefont reader's unknown-DXGI-format and stride/rows-mismatch refusals are
  untested.
  sprite_font_file.cpp refuses an atlas format other than 28, 74 or 115
  (to_texture_format) and an atlas whose stated stride or rows disagree with
  texture_level. The second is the check its comment calls worth 'a message
  rather than a texture full of noise'. Neither path appears in the tests; only
  overflowed layouts, glyph count, glyph extents and metric finiteness are
  covered. *Fix:* Add two cases: patch the DXGI format field to an unsupported
  value, and change the stride to expected+4. Each expects runtime_error naming
  the path.

## H3. Render backend error handling and resource ownership

- **medium** · bug · `engine/render/d3d11/texture_factory.cpp:106` — D3D11
  add_texture_asset breaks the seam's 'names the texture and format' contract;
  the other four backends keep it.
  When a D3D11 device rejects a format, for example optional B4G4R4A4 support,
  the thrown message is only "Failure with HRESULT of 80070057". It gives no
  texture name and no format, which is the eight-digit-HRESULT outcome
  resource_factory.h's own comment (lines 43-45) calls unacceptable under T6.
  D3D12 fixed this for the same call, but D3D11 did not. throw_if_failed.h then
  claims the exception type alone meets the contract, but the contract is about
  the message content, not just the type. *Fix:* Do what
  d3d12/texture_factory.cpp does: capture the HRESULT from CreateTexture2D and
  CreateShaderResourceView and throw std::runtime_error naming the texture,
  format, size and HRESULT. Share format_name (see the separate finding). Then
  correct throw_if_failed.h:41-45 so it claims only the exception type, not the
  message contract. *(Severity corrected by the skeptic.)*
- **medium** · bug · `engine/render/gl/wgl_context.cpp:349` —
  wglCreateContextAttribsARB is fetched with raw wglGetProcAddress, bypassing
  the sentinel filter.
  load_entry_point (line 36-43) exists because 'ICDs may use these failure
  sentinels instead of a null pointer' (1, 2, 3, -1). On the one lookup that
  decides whether a 3.3 context can be made at all, a sentinel passes the null
  check and gets called, which crashes instead of producing the intended 'no
  WGL_ARB_create_context' message. The file also uses two inconsistent fetch
  paths. *Fix:* Use load_entry_point("wglCreateContextAttribsARB") here.
- **medium** · error-handling · `engine/render/throw_if_failed.h:64` — D3D
  HRESULT failures carry no context, while Vulkan's check_vk names the failing
  operation.
  These are the same concept with two reporting styles. A Vulkan failure says
  which call failed and with what result name. A D3D11/D3D12 failure at any of
  roughly 50 ThrowIfFailed sites, such as swap chain or PSO creation, is just a
  hex code, which is what T6 objects to. D3D12 has started writing its own
  one-off formatter (hresult_name) to get around it. *Fix:* Add an
  engine-styled `check_hr(HRESULT, const char* what)`, or a ThrowIfFailed
  overload taking `what`, that produces the check_vk message shape. Use it at
  the engine-written D3D call sites. The upstream-adopted
  d3d11/device_resources.cpp can keep its ThrowIfFailed calls.
- **low** · error-handling · `engine/render/d3d11/texture_factory.cpp:53` —
  Format switch default silently treats any unlisted format as RGBA.
  texture_format.h promises that an unsupported format is 'rejected by name ...
  rather than passed through to fail as a device error'. If an enumerator is
  added to TextureFormat, this backend compiles without warning and uploads its
  bytes as RGBA. That is wrong pixels, or a size-mismatch device error, with no
  name attached. *Fix:* List every enumerator without a default, and after the
  switch throw std::invalid_argument naming the format. MSVC's /W4 then reaches
  the throw only for values outside the enum.
- **low** · bug · `engine/render/d3d12/device_resources.cpp:469` — New size is
  committed before a rebuild that can throw, so a retry at the same size
  becomes a no-op over released back buffers.
  output_size_ is updated first. create_window_size_dependent_resources then
  resets every back buffer (line 362) before ResizeBuffers or GetBuffer can
  fail with something other than removal, such as E_OUTOFMEMORY or
  INVALID_CALL. If the exception is caught and the same size is requested
  again, both DeviceResources::window_size_changed (line 463) and
  Renderer::window_size_changed (renderer.cpp:979) treat it as unchanged and
  return false. back_buffer() is then null and back_buffer_size() reports a
  size the swap chain never reached. On the line-357 throw the back buffers
  survive, but output_size_ and screen_viewport_ disagree.
  Renderer::window_size_changed has also already called abandon_recording, so
  frame_begun stays true while no view is reopened. *Fix:* Commit output_size_
  only after the rebuild succeeds: pass width and height into
  create_window_size_dependent_resources, or restore the previous RECT on
  failure. Alternatively, leave the size marked dirty so the next call
  rebuilds. *(Severity corrected by the skeptic.)*
- **low** · error-handling · `engine/render/d3d12/device_resources.cpp:567` —
  block_until ignores WaitForSingleObjectEx's result and reports S_OK even when
  the wait failed.
  block_until carries the HRESULT for both wait_for_frame (throwing) and
  try_wait_for_gpu (bool). If the wait returns WAIT_FAILED (for example, an
  invalid or closed event handle), the function reports completion although the
  fence was never reached. Its callers then reset allocators and overwrite
  vertex pages, or release staging buffers, that the GPU may still be reading.
  This is exactly the hazard the fence exists to prevent, and nothing reports
  it. *Fix:* Check the return value. On anything other than WAIT_OBJECT_0,
  return HRESULT_FROM_WIN32(GetLastError()) (or E_FAIL), so wait_for_frame
  throws and try_wait_for_gpu answers false. *(Severity corrected by the
  skeptic.)*
- **low** · ownership · `engine/render/d3d12/device_resources.h:148` — Fence
  event is a raw HANDLE closed manually in the destructor.
  Every other resource DeviceResources owns is a ComPtr. The event is created
  at device_resources.cpp:305 and closed by hand at :82-86. RAII would remove
  the manual cleanup, and the lazy-creation branch becomes the only place that
  touches it. *Fix:* Hold it in Microsoft::WRL::Wrappers::Event (from
  <wrl/event.h>), or in a small unique_ptr with a CloseHandle deleter, and
  delete the destructor's CloseHandle block. *(Severity corrected by the
  skeptic.)*
- **low** · bug · `engine/render/d3d12/texture_factory.cpp:72` — `default:` in
  format switches silently maps any future TextureFormat to RGBA8.
  Every current TextureFormat enumerator is listed, so the default only catches
  a future one. It would then create an RGBA8 resource and copy rows sized by
  RGBA8 footprints from data laid out for another format, with no error.
  Without the default, a switch-coverage warning would flag the missing case.
  *Fix:* Remove the `default:` labels, add a throw after the switch for
  out-of-range values, and make the same change in the sibling backends that
  share the pattern. *(Severity corrected by the skeptic.)*
- **low** · error-handling · `engine/render/gl/renderer.cpp:285` —
  Shader/program objects leak when compile or link fails.
  A fragment-compile failure leaves vertex_shader undeleted. A link failure
  leaves both shaders and the program. compile_shader carefully deletes its own
  shader on failure, so the cleanup contract is half-kept. restore_device
  rethrows on a live context, so these objects can stay on a context that
  remains in use. *Fix:* Delete the shaders and program before throwing on link
  failure, and delete vertex_shader if the fragment compile throws. A scoped
  shader-name owner like texture_factory's TextureOwner would handle both.
- **low** · error-handling · `engine/render/gl/texture_factory.cpp:30` —
  compressed_format's default case silently maps any format to DXT5.
  A future block-compressed TextureFormat (for example BC7) that
  is_block_compressed accepts would upload as DXT5 and draw garbage, where the
  uncompressed path refuses unknown formats by name. The default case also
  turns off the compiler's switch-coverage warning. *Fix:* List the three cases
  explicitly and throw a named refusal after the switch, matching
  check_uncompressed.
- **low** · ownership · `engine/render/gl/texture_factory.cpp:100` —
  TextureOwner has a destructor but implicit copy operations.
  An accidental copy would delete the same GL name twice. Every other owner in
  the folder (GlTexture, both Context structs) deletes its copy constructor and
  copy assignment. *Fix:* Delete the copy constructor and copy assignment, as
  GlTexture does.
- **low** · error-handling · `engine/render/gl/wgl_context.cpp:331` —
  SetPixelFormat failure reuses the ChoosePixelFormat message.
  By this point a suitable format was found and described (l.306-328);
  SetPixelFormat typically fails because the window already has a format or for
  a GetLastError reason. The copied message misdirects diagnosis. *Fix:* Say
  SetPixelFormat failed and include GetLastError(), as configure_swap_interval
  does at l.102-105.
- **low** · ownership · `engine/render/gl/wgl_context.cpp:341` — Bootstrap
  legacy context leaks when wglMakeCurrent fails.
  When legacy is non-null and wglMakeCurrent fails, the throw drops the HGLRC.
  Context's destructor owns only gl_context, and the next failure branch (line
  352-358) deletes legacy by hand. This is manual cleanup that RAII should own,
  and one path misses it. *Fix:* Hold the bootstrap context in a small scoped
  owner, or a unique_ptr with a deleter that calls wglMakeCurrent(dc, nullptr)
  and wglDeleteContext, so every exit releases it. That also removes the two
  hand-written cleanups.
- **low** · maintainability · `engine/render/texture_data.cpp:23` — unit_bytes'
  `default:` silently sizes any future TextureFormat as 4 bytes, and
  block_decode's `default:` decodes it as BC3.
  texture_level is the single layout authority for the .dds reader, the
  .spritefont check and block_decode. With a default label, /W4 and -Wswitch
  cannot flag a new enumerator, so adding a format such as BC4 or R8 yields a
  wrong stride and size with no diagnostic. block_decode.cpp:167 has the same
  shape ('case TextureFormat::bc3_unorm: default:'). It is guarded only by a
  separate hand-kept list in is_block_compressed, and block_bytes there is also
  a 'bc1 ? 8 : 16' ternary. *Fix:* List every enumerator and drop `default:` so
  the compiler enforces exhaustiveness. Put a throw after the switch for
  out-of-range values. Do the same in block_decode's decode_block and
  block_bytes. *(Severity corrected by the skeptic.)*
- **low** · bug · `engine/render/vulkan/device_resources.cpp:540` —
  Failed-create outputs are written straight into owned members, against the
  file's own rule.
  The Vulkan spec leaves a failed command's output parameters undefined. The
  destroy paths treat any non-null value in these members as owned, so a failed
  create can hand an indeterminate handle to vkDestroy*/vkFreeMemory. The debug
  messenger case is the most direct: its VkResult is discarded and the handle
  is still destroyed later. The rule is stated and followed in one function and
  ignored in the others. *Fix:* Use the create_vulkan_buffer pattern
  everywhere: create into a local VK_NULL_HANDLE and assign it to the member or
  cleanup variable only after VK_SUCCESS. For the messenger, check the result
  and store the handle only on success. *(Severity corrected by the skeptic.)*
- **low** · error-handling · `engine/render/vulkan/device_resources.cpp:1496` —
  vkResetCommandPool failures are discarded before the buffer is begun again.
  vkResetCommandPool can return VK_ERROR_OUT_OF_DEVICE_MEMORY. When that is
  ignored, the state flags claim the buffer is in the initial state while it
  may still be executable. The next vkBeginCommandBuffer on a pool without
  RESET_COMMAND_BUFFER_BIT is then invalid usage, where the backend's T6
  convention would give a named throw. Every other API result on the frame path
  goes through check_vk. *Fix:* Wrap both calls in check_vk with an
  engine-worded message, such as "The frame's command pool could not be reset",
  before the flags are cleared. Keep std::ignore only for
  vkResetDescriptorPool, which the spec defines as always returning VK_SUCCESS.
  *(Severity corrected by the skeptic.)*
- **low** · ownership · `engine/render/vulkan/device_resources.h:173` —
  VulkanBuffer has no RAII, so every temporary buffer needs a hand-written
  try/catch.
  The struct pairs the handles but does not own them. Each of the four
  temporary-buffer sites repeats the same destroy in a catch and again on the
  success path, and SM-56's read-back fix was exactly this kind of hand-written
  cleanup going wrong. A move-only owner holding the creating device's
  shared_ptr would remove all four try/catch blocks. SM-56 deliberately ruled a
  general owner framework out of its own scope, so this needs its own story.
  *Fix:* Open a separate story for a move-only scoped owner of a local staging
  or read-back VulkanBuffer. It would hold the shared_ptr<VulkanDevice> and
  call destroy_vulkan_buffer in a noexcept destructor. Long-lived Frame and
  Renderer::Impl members can stay as they are. Keep the SM-56 regression tests
  as the acceptance. *(Severity corrected by the skeptic.)*

## H4. Build, CI and remaining tools

- **medium** · error-handling · `.github/workflows/website.yml:258` — The NVDA
  install step can hide a failed npm ci or npm install, and its packages are
  unpinned.
  On windows-latest this run block uses pwsh, and pwsh only returns the last
  native command's exit code. A failing `npm ci` or `npm install` is therefore
  ignored whenever the later commands succeed. ci.yml handles the same problem
  by checking `$LASTEXITCODE` explicitly between commands (lines 207-216). The
  `--no-save` install also takes whatever Guidepup versions are newest, so the
  screen-reader job can change behaviour with no change to the repository. That
  is the drift the rest of CI avoids by pinning versions and SHA-256 digests.
  *Fix:* Put `if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }` after each
  command, or split them into separate steps. Pin exact versions, for example
  @guidepup/guidepup@x.y.z and @guidepup/setup@x.y.z.
- **medium** · bug · `tools/flash_check/flash_check.mjs:53` — Flash check's
  verdict and exit status use the concurrent count, but the recorded decision
  says the pixel bound is the verdict.
  The tool's exit code, its final verdict line and its header contract all
  follow the loose reading that the owner rejected. A frame sequence that fails
  the pixel bound but passes the concurrent count (the test fixture at :72 is
  one) exits 0 and prints "Passes WCAG 2.3.1". A maintainer or CI step that
  trusts the exit status would accept bursts the documented policy refuses, and
  the header tells a reviewer to argue with the wrong rule. *Fix:* Base
  `passes` and the exit status on the pixel bound (`bound.transitions <= 2 *
  MAX_FLASHES`) and report the concurrent count as the diagnostic. Rewrite the
  TWO COUNTS header so the pixel bound is the verdict, matching
  samples/linesweeper/README.md. Update the test at :72 so it expects `passes
  === false`. *(Severity corrected by the skeptic.)*
- **medium** · stale-comment · `tools/linesweeper_capture/png_file.cpp:80` —
  Comments claim golden_image.cpp / RenderPixelTests use WIC; they use lodepng.
  Both comments use a precedent that no longer exists to justify a design
  choice. Anyone following the cross-reference to golden_image.cpp finds a
  different codec and no SetPixelFormat arrangement at all. *Fix:* Drop the
  references to golden_image.cpp and RenderPixelTests and state the WIC
  behaviour on its own terms, or resolve this together with the png_file.h
  finding by moving the tool to lodepng.
- **low** · supply-chain · `.github/actions/emscripten/action.yml:55` — emsdk
  manager is cloned from the head of its default branch although the comment
  calls the toolchain pinned.
  Only EMSDK_VERSION is pinned. The emsdk.py/emsdk.bat scripts that run with
  the job's permissions, and their release-tag table, are whatever the default
  branch holds on the day the cache misses. The same commit can therefore
  install differently, or break, on two runs, and the comment's claim that
  nothing outside git and the SDK is trusted to move is false. Because the
  cache key is emsdk-<version>-<os>, the change surfaces only when the key
  misses. *Fix:* Pin the manager: clone and then `git -C C:\emsdk checkout
  <sha>` (or fetch the emsdk tag matching EMSDK_VERSION), with the commit in
  ci.yml's env beside EMSDK_VERSION, and put it in the cache key. *(Severity
  corrected by the skeptic.)*
- **low** · duplication · `.github/actions/emscripten/action.yml:81` — ccache
  cache path hard-codes C:\ccache instead of reading the CCACHE_DIR it says it
  shares with ci.yml.
  The directory ccache writes to (env CCACHE_DIR) and the directory the cache
  saves are two separate literals. If CCACHE_DIR in ci.yml changes, every run
  silently caches an empty folder and the documented minutes of savings
  disappear without any failure. *Fix:* Use `path: ${{ env.CCACHE_DIR }}`, as
  the steps above already do with env.EMSDK_VERSION.
- **low** · archaeology-comment · `.github/workflows/ci.yml:1` — Workflow
  comments narrate history rather than current behaviour.
  CONVENTIONS keeps history out of comments. These passages tell how the file
  came to be rather than what it does. SM-47 cleaned build-comment archaeology
  in compile_shaders.cmake and check_doc_citations.cmake but did not cover
  ci.yml. *Fix:* Rewrite each passage as present-tense rationale. For example,
  line 386 becomes '-PassThru makes the installer's exit code observable, so a
  failed install fails here rather than as a misattributed VULKAN_SDK error
  later.' Drop the origin narratives.
- **low** · other · `.github/workflows/ci.yml:38` — The env-block comment is
  ragged and describes only part of the block it heads.
  The comment is about EMSDK and ccache pins but sits above an env block that
  starts with the Vulkan SDK and Mesa pins used by the windows matrix. The
  broken wrap ('installs / both') suggests an edit left half-done. *Fix:* Move
  the browser paragraph down beside EMSDK_VERSION, CCACHE_* and CCACHE_DIR,
  rewrap it, and give the block a one-line header saying it holds the pinned
  tool versions and digests for all jobs.
- **low** · stale-comment · `.github/workflows/ci.yml:272` — Windows job
  comment says GL and Vulkan get no device, contradicting the same block's
  llvmpipe and SwiftShader paragraphs.
  The sentence predates the software-rasteriser setup. It now states the
  opposite of what the job does, inside the comment that documents that job.
  *Fix:* Rephrase: Direct3D gets an adapter from the runner image; GL and
  Vulkan get one only from the Mesa and SwiftShader drivers the steps below
  provide. *(Severity corrected by the skeptic.)*
- **low** · duplication · `.github/workflows/ci.yml:444` — vswhere/vcvars and
  VCPKG_ROOT steps are copy-pasted into five jobs.
  The repository already uses a local composite action
  (.github/actions/emscripten) for shared setup. The vcvars lookup is a
  200-character one-liner duplicated five times, and a change to the required
  component or path has to be made in five places. *Fix:* Add a composite
  action, for example .github/actions/msvc-env, that exports VCPKG_ROOT,
  IMAGE_VERSION and VCVARS, and use it from the five Windows jobs.
- **low** · bug · `.github/workflows/master-watch.yml:28` — master-watch's
  45-minute bound is shorter than the CI jobs whose rerun it waits for.
  master_watch.py blocks on the rerun it started, because a token-started rerun
  raises no workflow_run event of its own. A rerun of a failed windows or wasm
  job may legitimately run up to 90 minutes (cold vcpkg cache), and longer for
  wasm followed by safari. The watch job is then cancelled at 45 minutes, so no
  issue is opened or commented: the second failure goes unreported. The
  comment's claim is false against ci.yml's own bounds. *Fix:* Raise
  timeout-minutes above the slowest rerun chain (for example 150: wasm 90 plus
  safari 30 plus margin). Alternatively, give wait_for_rerun its own deadline
  that reports 'rerun still running' as an issue comment rather than dying
  silently. Make the comment state the arithmetic. *(Severity corrected by the
  skeptic.)*
- **low** · duplication · `.github/workflows/website.yml:114` — The
  versions:verify base expression is copied into two steps, and the job has no
  timeout.
  The same non-trivial expression appears at lines 114 and 135. Both steps must
  agree on the base, and the header comment (lines 38-43) describes it once.
  Separately, the `website` job (line 66) has no timeout-minutes, while every
  ci.yml job and the other website.yml jobs set one. ci.yml line 245 gives the
  reason: a stalled step should fail the check instead of holding pull requests
  for six hours. *Fix:* Move BASE into the job's env (or one step's output) and
  reference it from both steps. Add a timeout-minutes to the website job.
- **low** · archaeology-comment · `CMakeLists.txt:30` — Build comments tell the
  history of earlier wording instead of the current contract.
  CONVENTIONS calls for removing archaeology, and SM-47 covered only five
  files. Narratives about earlier wording remain here and in
  .github/workflows/ci.yml:3-4 ('ARCHITECTURE said ctest runs ... while there
  was no .github/ at all. This is the CI half made true.') and ci.yml:262-263
  ('WHICH THIS FILE SAID UNTIL ONE JOB LOG WAS READ'). The constraint is worth
  keeping; the account of what the file used to say is not. *Fix:* Rewrite each
  comment as the present-tense reason (e.g. 'ALL alone covers only the default
  build; this dependency covers targeted and consumer builds') and drop the
  earlier-wording narrative and review citation.
- **low** · stale-comment · `CMakeLists.txt:59` — Citation-check rationale
  omits engine/, which the check reads and a consumer builds.
  The argument for keeping the check top-level only rests on a file list that
  leaves out engine/. engine/ is the one folder the check reads that a consumer
  does compile, so the stated rationale is false. The real reason is that the
  check guards this repository's prose, not the consumer's build. *Fix:* List
  engine/ among the folders read, and restate the reason without the 'none of
  which a consumer builds' claim. For example: a consumer should not fail its
  build over this repository's document renames.
- **low** · duplication · `CMakeLists.txt:242` — Four page tests repeat the
  run_page.mjs command instead of using labrador_sample_page_test.
  labrador_sample_page_test (lines 187-197) exists for exactly this command,
  but its quit_keys argument is required, so ComponentsPointerPageTests (242),
  LineSweeperBundlePageTests (256), LineSweeperHostPageTests (278) and
  LineSweeperPacingPageTests (291) each repeat the emulator, runner path and
  --browser arguments. A new runner-wide option would have to be added in five
  places. *Fix:* Make the function take keyword arguments (PAGE, QUIT_KEYS
  optional, then pass-through) via cmake_parse_arguments, and register all nine
  Chromium page entries through it.
- **low** · naming · `CMakePresets.json:87` — Null presets are labelled 'no
  platform API' but still build the Win32 window and the XInput/DirectXTK pad
  backend.
  Native null still links DirectXTK for XInput and opens hidden Win32 windows
  in its tests (AGENTS.md: 'hidden window, synthetic input'). The display name
  and ci.yml's claim that 'nothing in it can fail for a reason outside the
  repository' overstate what the configuration removes. *Fix:* Rename the
  display names to something like 'x64 Debug (null render and audio)'. Correct
  ci.yml:302-305 to say no graphics or audio API, while the window and the pad
  API remain.
- **low** · duplication · `CMakePresets.json:104` — Presets repeat blocks that
  inheritance could hold once: wasm-base copies base's fields, and 12 test
  presets repeat the same output block.
  The generator, binaryDir, installDir and vcpkg toolchain lines are written
  twice, in base and wasm-base. The output block is written in all 12 test
  presets, and the backend cacheVariables are repeated in each Debug/Release
  pair. A change to the build layout or the toolchain must be made in two
  places, and a new test preset can silently lose outputOnFailure. *Fix:* Add a
  hidden `common` configure preset (generator, binaryDir, installDir,
  toolchainFile) that both base (adding the Windows condition and architecture)
  and wasm-base inherit. Add a hidden test preset carrying `output` that the
  others inherit. Optionally add hidden backend mixins (d3d12, gl, vulkan,
  null) combined with debug/release through multiple `inherits`. *(Severity
  corrected by the skeptic.)*
- **low** · duplication · `bench/CMakeLists.txt:72` — LineSweeper's
  content-copy commands are repeated in three targets.
  The same three copies (manifest.json, fonts/, textures/) appear for
  LineSweeperSample (samples/linesweeper/CMakeLists.txt:204-227),
  LineSweeperFrameBench (here) and LineSweeperCapture
  (tools/linesweeper_capture/CMakeLists.txt:184-197). If the sample adds a
  content folder, all three must change, or the bench and capture quietly load
  different content from the sample they claim to match. *Fix:* Define one
  function in samples/linesweeper/CMakeLists.txt, e.g.
  linesweeper_add_content_copy(<custom-target> <exe>), and call it from the
  sample, the bench and the capture tool.
- **low** · bug · `bench/CMakeLists.txt:121` — D3D12 probe source hashes are
  computed at configure time and go stale after an edit.
  bench/d3d12_descriptor_count_main.cpp:165 and
  bench/d3d12_descriptor_query_probe.cpp:106-108 print these values as
  provenance for the source that was measured. file(SHA256) runs only at
  configure, and neither file is registered as a configure dependency (git grep
  CMAKE_CONFIGURE_DEPENDS finds nothing in the repo). So if renderer.cpp or
  device_resources.cpp is edited and then rebuilt without a reconfigure, the
  probe compiles the new code but still prints the old hash. That is false
  evidence. *Fix:* Add both files with set_property(DIRECTORY APPEND PROPERTY
  CMAKE_CONFIGURE_DEPENDS ...) so an edit triggers a reconfigure.
  Alternatively, compute the hashes in a build-time custom command that writes
  a generated header. *(Severity corrected by the skeptic.)*
- **low** · stale-comment · `bench/scene_bench.cpp:159` — 'absent from four
  presets out of five' miscounts presets; it means render backends.
  The count is wrong in the unit it names; read literally, a reader would look
  for five presets. *Fix:* Change it to 'absent from four render backends out
  of five' (or 'from every non-null configuration').
- **low** · other · `cmake/check_doc_citations.cmake:118` — PHILOSOPHY.md is
  re-read for every T-number token on every build.
  This script is an ALL target and a dependency of labrador_settings (root
  CMakeLists.txt:62-72), so it runs on every top-level build. For each T-token
  in a comment line of every engine/sample/test/bench/tools source, it reads
  the whole of PHILOSOPHY.md from disk again. The set of valid T headings is
  constant and could be read once. *Fix:* Before the source loop, read the
  '^### T[0-9]+\. ' headings once into a list of valid numbers, then test each
  token with `if(number IN_LIST valid_tradeoffs)`.
- **low** · stale-comment · `cmake/check_engine_includes.cmake:109` —
  Backend-wall failure message gives no guidance for app platform folders,
  which the wall also covers.
  The fixtures (app_root, same_app) and AGENTS.md state the wall covers
  app/win32 and app/web as well. A violation there gets advice about render,
  audio and input seams only, and nothing pointing at app/window.h. *Fix:* Add
  a line such as 'Everything that talks to the platform's window goes through
  app/window.h.'
- **low** · stale-comment · `cmake/check_whitespace.cmake:4` — Header claims
  the lint reads every first-party source; it reads five extensions in five
  folders.
  .editorconfig also makes .ts, .astro and .css tab-indented and trims trailing
  whitespace on CMake, Python and YAML. None of those are read, and website/ is
  not read at all. AGENTS.md leans on this lint ('formatting the whitespace
  lint does not catch'), so the overclaim misleads. *Fix:* State the scope
  exactly: 'This reads the tab-indented C++, HLSL and JavaScript sources under
  engine/, samples/, tests/, bench/ and tools/.' Or widen the glob to match the
  claim.
- **low** · duplication · `cmake/compile_shaders.cmake:38` — compile_hlsl and
  compile_hlsl_to_spirv repeat the same output-path and target-wiring code.
  Lines 132-136 and 152-153 match lines 189-193 and 213-214 exactly. The
  generated-root location is also hard-coded a third time in
  bench/CMakeLists.txt:114 ("${CMAKE_BINARY_DIR}/generated"). Moving the
  generated root means three edits that nothing checks against each other.
  *Fix:* Have both functions call one internal helper, e.g.
  _labrador_generated_shader(target output OUT_VAR), and keep the root in one
  variable (LABRADOR_GENERATED_ROOT) that bench/CMakeLists.txt uses.
- **low** · interface · `cmake/settings.cmake:12` — CMAKE_CXX_EXTENSIONS is a
  directory variable, not part of labrador_settings, so consumer targets don't
  get it.
  The file header says 'every real target links labrador_settings, so compiler
  strictness lives in exactly one place'. This setting, though, is a variable
  in the engine's directory scope. A game that uses add_subdirectory
  (website/src/walkthrough/root-CMakeLists.txt) and links labrador_settings
  still compiles its own targets as gnu++20 under clang/Emscripten.
  CHANGES.md:959 records this, but the source comment presents it as part of
  the shared settings. *Fix:* Say in the comment that this applies only to
  targets in this tree and that a consumer has to set it itself. Alternatively,
  add it to the walkthrough's root CMakeLists so the template carries the same
  rule.
- **low** · error-handling · `engine/CMakeLists.txt:344` — Configure never
  refuses the input backend on the wrong platform, unlike render and audio.
  The render block (lines 137-142) and the audio block (lines 324-332) both
  refuse a backend on a platform that lacks its API, by name at configure time.
  Line 313 says why: 'Each platform API is refused where it does not exist'.
  The input block claims to be 'chosen the same way' but has no such guard.
  With LABRADOR_INPUT_BACKEND=xinput under Emscripten, the first error is an
  unnamed find_package(directxtk) failure (vcpkg.json limits directxtk to
  windows). With web on native, the first error is a missing emscripten header
  at compile time. Both are the late, misattributed failures that T5/T6 and the
  neighbouring blocks exist to prevent. *Fix:* Add the matching guard:
  if(EMSCRIPTEN AND LABRADOR_INPUT_BACKEND STREQUAL "xinput") FATAL_ERROR
  naming web, and if(NOT EMSCRIPTEN AND LABRADOR_INPUT_BACKEND STREQUAL "web")
  FATAL_ERROR naming xinput. Update AGENTS.md's list of what configure refuses.
  *(Severity corrected by the skeptic.)*
- **low** · stale-comment · `engine/CMakeLists.txt:393` — The DirectXTK
  rationale is detached from its code and sits above the lodepng lookup.
  The DirectXTK block ends straight into the lodepng comment with no blank
  line, so it reads as the rationale for find_package(lodepng). The DirectXTK
  code it explains is at lines 414-424, behind a different comment. *Fix:* Move
  the 'DirectXTK IS PRIVATE' paragraph down to the conditional DirectXTK
  find/link block at line 414, and separate the lodepng comment and code from
  it.
- **low** · naming · `tests/assets/CMakeLists.txt:18` — Sibling
  test-configuration macros use opposite word order.
  These are two macros for the same idea (a test built against a null backend),
  spelled MODULE_NULL in one and NULL_MODULE in the other. A grep for one
  pattern misses the other. *Fix:* Pick one order, for example
  LABRADOR_TEST_NULL_RENDER, and rename it in tests/assets/CMakeLists.txt and
  string_table_text_tests.cpp.
- **low** · stale-comment · `tools/consumer_check/assemble.py:43` — The comment
  over PROJECT_NAME lists the engine's packages but omits directxtk.
  Its second sentence describes the dependency copy in derived_manifest, not
  the constant it sits on, and presents two packages as "every package" when
  there are three. *Fix:* Keep only the first sentence above PROJECT_NAME. If
  the dependency note is worth having, move it to derived_manifest as "every
  dependency the engine's vcpkg.json names, unchanged", without listing them.
- **low** · error-handling · `tools/flash_check/flash_check.mjs:137` — readPng
  does not check the inflated size, so a short image stream reads as black
  pixels.
  If the decompressed data is shorter than height * (stride + 1), `raw[source +
  x]` is undefined. `undefined + n` is NaN, and `NaN & 0xff` is 0, so missing
  rows become zeros with no error. A PNG without IHDR gives width = height = 0
  and an empty frame, which the analyser then refuses only by its size check.
  For a tool whose verdict is an accessibility claim, a malformed frame should
  be refused (exit 2), not analysed as dark. *Fix:* After inflating, throw when
  `width === 0` or when `raw.length !== height * (stride + 1)`, naming the
  file, in the same way the IHDR format check does.
- **low** · ownership · `tools/input_drive/hold_key.ps1:102` — Save-Client's
  GDI objects are disposed only on the success path.
  With $ErrorActionPreference = 'Stop', a failure in CopyFromScreen or Save
  skips both Dispose calls. The GetClientRect and ClientToScreen return values
  are also discarded with [void]. A failed GetClientRect produces a 0x0 Bitmap,
  and the constructor throws an unhelpful 'Parameter is not valid' instead of
  naming the window problem. *Fix:* Wrap the body in try/finally and dispose
  there. Check GetClientRect's result and throw 'Could not read the game
  window's client area' when it is false. *(Severity corrected by the
  skeptic.)*
- **low** · bug · `tools/input_drive/hold_key.ps1:105` — Bitmap.Save receives a
  path relative to PowerShell's location, but .NET resolves it against the
  process directory.
  New-Item creates the folder relative to PowerShell's current location.
  Bitmap.Save resolves the same relative path against
  [Environment]::CurrentDirectory, which PowerShell does not update on
  Set-Location. If the script is invoked with `&` or dot-sourced from a session
  that has changed directory, the folder is created in one place and Save then
  fails with a generic GDI+ error or writes elsewhere. The documented
  `powershell -File` form works only because it starts a fresh process. *Fix:*
  Resolve the output folder once after creating it, with `$OutDirectory =
  (Resolve-Path -LiteralPath $OutDirectory).Path`, and pass absolute paths to
  Save-Client.
- **low** · stale-comment · `tools/linesweeper_capture/CMakeLists.txt:33` — The
  capture tool's comment says RenderPixelTests links WIC, but it uses lodepng.
  tests/render/CMakeLists.txt:102-104 says 'lodepng is the golden set's codec,
  on every platform', and RenderPixelTests links lodepng, not windowscodecs. A
  git grep for windowscodecs/wincodec in tests/render finds nothing. So the
  comment justifies a dependency by pointing at a precedent that no longer
  exists. *Fix:* Drop the comparison and state only the current reason: the
  capture writes PNGs through WIC (png_file.cpp). *(Severity corrected by the
  skeptic.)*
- **low** · stale-comment · `tools/linesweeper_capture/png_file.h:18` — The
  rationale for WIC over lodepng is false now that the engine itself links
  lodepng.
  The comment justifies a second, Windows-only PNG writer plus a
  `windowscodecs` link by avoiding a dependency the build already pays for
  through LabradorEngine. A maintainer cannot tell whether the WIC writer is
  still deliberate, or whether it could share lodepng and drop the
  platform-specific code. The paragraph is also wrapped mid-sentence:
  "Lossless" sits alone at the end of line 24. *Fix:* Re-justify or retire the
  WIC writer. Either state the real reason it stays (for example, the engine
  links lodepng PRIVATE and the tool should not reach into it), or encode with
  lodepng and remove `windowscodecs` from the target. Rewrap the paragraph.
  *(Severity corrected by the skeptic.)*
- **low** · naming · `tools/web_bundle/bundle.mjs:219` — camelCase and
  snake_case identifiers mixed in one file; one-use temporaries.
  Within one script and its sibling, the same kind of local is named two ways.
  sourceCommit and sourceDirty exist only to be copied into the outer variables
  on the next line. *Fix:* Use snake_case (`git_root`, `same_root`) to match
  the rest of the file and check_live.mjs, and assign `commit` and `dirty`
  directly from the git calls. *(Severity corrected by the skeptic.)*
- **low** · error-handling · `tools/web_bundle/bundle.mjs:229` — The bare catch
  drops the reason a checkout's commit could not be read.
  The try also covers cases that are not 'not a checkout': `.git` exists and is
  the same root, but git is missing from PATH, the repository is corrupt, or
  HEAD is unborn. In each case BUILD.json silently records `commit: "unknown",
  dirty: null`. The failure only shows up later, when `--verify` or check_live
  refuses the bundle with 'commit must be a full Git revision', and the cause
  is gone. A non-checkout is already handled without an exception by the
  `existsSync(join(repository, '.git'))` branch. *Fix:* Keep the unknown
  fallback, but write the error to stderr: `catch (error) {
  process.stderr.write(`bundle: commit unknown: ${error.message}\n`); }`.
  Alternatively, narrow the try to the cases that are expected. *(Severity
  corrected by the skeptic.)*
- **low** · bug · `tools/web_bundle/check_live.mjs:204` — An unknown
  Content-Encoding on an identity fetch crashes the run instead of failing one
  check.
  The header says "A request that cannot be made at all is a FAIL of that
  check, not a crash", and every check runs. If a server answers the
  Accept-Encoding: identity request with a coding get() does not decode (for
  example zstd), plain.body is null. `plain.body.length` and `sha256(null)`
  then throw a TypeError at top level, so the run dies with a stack trace and
  writes no dated record. The page path already guards with `page.body?.`, but
  this loop does not. *Fix:* Have get() refuse (reject) an unrecognised coding,
  so attempt() records it as a FAIL. Alternatively, treat `plain.body === null`
  in the loop as a failed check and `continue`. *(Severity corrected by the
  skeptic.)*
- **low** · naming · `tools/web_pacing/page.js:18` — `width` names the row
  stride of the sample buffer, in a function that also uses canvas.width and
  box.width.
  Inside a geometry-tracking closure, `width` reads as a pixel width. It is
  actually the number of float fields per engine row, and the 15 must match the
  field offsets written at :170-184 and read at :83-91. *Fix:* Rename it to
  `row_fields` (or `fields_per_row`) and note that it counts the offsets
  written in labradorPacingFrame.
- **low** · duplication · `tools/web_pacing/page.js:63` — updateGeometry
  repeats its unchanged-geometry comparison twice.
  The second test is the first one plus the canvas box. Adding a field to the
  geometry record means editing both expressions identically. If they drift
  apart, a geometry change can stop starting a new segment, which corrupts what
  analyse.mjs counts as contiguous. *Fix:* Factor out `const sameBuffer = () =>
  geometry && canvas.width === ... && innerHeight ===
  geometry.viewport_css[1];` and use `sameBuffer() && box.width === ... &&
  box.height === ...` for the second test.
- **low** · magic-number · `tools/web_pacing/page_input.mjs:26` — The
  9,600-particle fixture count is hard-coded throughout the JS instruments.
  C++ names this value once, as `pacing_particle_count`
  (samples/linesweeper/web/pacing_fixture.h:12) and `expected_particles`
  (bench/linesweeper_frame_result.h:14). The JS side repeats the literal six
  times across two files, together with positional snapshot indices 9/10/11
  that must match the argument order in page.js. If the fixture changes, the JS
  checks silently become wrong unless every literal is found. *Fix:* Declare
  `const FIXTURE_PARTICLES = 9600; // pacing_fixture.h pacing_particle_count`
  once in each file, and name the snapshot indices (for example `const MARKER =
  9, LIVE = 10, DROPPED = 11`), so the C++ owner is cited in one place.
- **low** · duplication · `tools/web_pacing/page_input.mjs:112` — Three page
  drivers each retry the same navigation race with a different regex.
  All three drivers handle the same DevTools errors around a navigation, but
  they disagree on which messages are benign. page_input.mjs does not tolerate
  'Cannot find default execution context' or 'Inspected target navigated',
  which the other two accept. A Chrome that reports the race with one of those
  messages fails the Release LineSweeperPacingPageTests intermittently. The
  next driver will copy one of the three regexes at random. *Fix:* Export one
  `is_navigation_race(error)` predicate, or a `poll_across_navigation(evaluate,
  expression)` helper, from run_page.mjs, or pass it in through page_api. Use
  it in all three drivers. *(Severity corrected by the skeptic.)*

## H5. Python tooling hygiene

- **medium** · bug · `tools/master_watch.py:75` — The 'rerun did not record a
  new attempt' branch can never run, and the case it names spins until the job
  times out.
  wait_for_rerun returns only when the attempt number has risen above the first
  one. decide() returns "rerun" only for attempt 1, so lines 77-78 can never
  run. A rerun that never records a new attempt is exactly what that branch
  describes. In that case the unbounded `while True` loop keeps polling `gh run
  view` every 30 s until master-watch.yml's 45-minute timeout kills the job,
  with no message. The code claims to handle a failure it actually turns into a
  silent timeout. *Fix:* Bound wait_for_rerun with a deadline or a maximum
  number of polls. On expiry, return the run unchanged or raise a clear error,
  so the 'did not record a new attempt' outcome can actually happen. Then add a
  FakeGh case where the views never show a higher attempt.
- **medium** · duplication · `tools/verify_changes.py:239` — Required wasm
  browser registrations are listed separately from ci_gap_report and
  ctest_inventory.json, and the lists already differ.
  There are three owners of one fact: which browser tests a Chromium-equipped
  wasm build must register. The planner already omits
  ComponentsPointerPageTests, so `verify_changes --execute` would accept a
  wasm-debug sweep that silently dropped it. ci_gap_report's tuple has no drift
  test against CMakeLists or the inventory. *Fix:* Make one owner. Either
  derive both lists from tests/ctest_inventory.json (adding a wasm-debug or
  Chromium-required grouping there if needed), or have both tools share one
  module-level list with a test that checks it against the inventory and
  CMakeLists.txt's add_test/labrador_sample_page_test names.
- **low** · duplication · `tools/check_changes_rule.py:53` — Commit-walk and
  diff-tree code copied from check_golden_reviews even though the module
  already imports it.
  The commit-parent parsing and the merge-commit refusal are a policy that both
  lint checks must apply identically. With two copies, a fix to one (say, a
  signed-commit header edge case or the error wording) will not reach the
  other. *Fix:* Add `commit_parts(repository, commit) -> (parents,
  message_bytes)` and `changed_paths(repository, commit, parents, *pathspec)`
  to check_golden_reviews (or the shared Git helper suggested above), and call
  them from both checkers. *(Severity corrected by the skeptic.)*
- **low** · error-handling · `tools/check_dpi_manifest.py:153` — A folder scan
  counts a malformed GUI program as a skipped console program.
  CI runs `python tools\check_dpi_manifest.py out\build\<preset>\samples`. If a
  sample .exe is truncated or otherwise unreadable as a PE image, its error is
  swallowed and it is reported as "console programs skipped". The check still
  passes as long as one other GUI program is found. In the same loop, an
  OSError from read_bytes is not caught and ends the run with a traceback
  instead of a named failure. *Fix:* Return unparsable images separately (for
  example `found, skipped, unreadable`) and have check_paths report each one
  through `complain` and fail. Catch OSError at the same point so it produces a
  named failure. *(Severity corrected by the skeptic.)*
- **low** · duplication · `tools/check_golden_reviews.py:25` — Six separate git
  subprocess wrappers in tools/ with inconsistent hardening.
  check_demo_lag decided that ambient GIT_DIR/GIT_* variables and replace refs
  must not redirect a check. check_golden_reviews (a required CI lint) and
  check_changes_rule (which reuses it) still honour ambient GIT_* routing. The
  hardening policy depends on which script you read, and each new tool picks a
  pattern at random. *Fix:* Add a tools/git_local.py with one hardened
  `git(repository, *args, accepted=(0,), text=False)` (check_demo_lag's
  environment scrubbing plus golden's flags) and a `tree_files(root)` listing.
  Have the six call sites use it. Keep the injectable runners in tests by
  passing the function. *(Severity corrected by the skeptic.)*
- **low** · duplication · `tools/ci_gap_report.py:79` — Two independent
  ctest-JUnit parsers and two preset-sweep executors with different acceptance
  rules.
  Two tools that both report "these presets passed" judge the same JUnit output
  differently. A ctest JUnit shape change can be accepted by one and rejected
  by the other, and docs/verification.md owns only one of the two contracts.
  *Fix:* Make verification_runner's parser the single JUnit reader, returning
  per-case outcomes without raising on failure, with a strict mode for the
  planner. Have ci_gap_report group its output. Consider building
  ci_gap_report's execute on verification_runner's runner so logs and failures
  are handled one way. *(Severity corrected by the skeptic.)*
- **low** · bug · `tools/ci_tested_tree.py:165` — --forced takes any string and
  treats everything except the exact text "true" as not forced.
  "True", "1" or a typo silently means 'not a forced push'. A forced push could
  then be judged already tested, and the CI build that should run would be
  skipped. Every other boolean in tools/ uses store_true or choices. *Fix:* Use
  `choices=["true", "false"]` so argparse refuses anything else, which still
  matches what the workflow passes.
- **low** · dead-code · `tools/cloud_performance/__init__.py:3` — Package
  re-exports that nothing imports.
  `git grep -E "cloud_performance import
  (canonical|digest|load_config|validate_config)|cloud_performance\.(canonical|digest|load_config|validate_config)\b"`
  across the repo, excluding docs/review, finds no user. Every caller (the
  __main__, analysis and infrastructure modules and the tools/tests files)
  imports from `.common` directly. The re-export adds a second public spelling
  with no consumer. *Fix:* Reduce __init__.py to its docstring, or add a
  comment if the package-level API is intentional.
- **low** · duplication · `tools/cloud_performance/__main__.py:65` —
  _stack_instance repeats _validated_runner's Runner lookup with weaker checks.
  Two functions resolve the same Runner instance under different rules.
  `launch` trusts the output without checking the resource type, while
  `status`, `collect` and `stop` check both. *Fix:* In _launch, use
  _validated_runner(cloudformation, stack_name) after the create waiter and
  delete _stack_instance. Hoist the stack-name prefix into one constant used by
  _stack_name and _validated_runner.
- **low** · duplication · `tools/cloud_performance/__main__.py:79` — Stack-name
  prefix, launch-record key and default Region are repeated as literals.
  _validated_runner checks the namespace with a second copy of the prefix that
  _stack_name builds. _status rebuilds the launch key that object_keys owns.
  The Region default is written three times. If one copy changes, the stop,
  status or collect commands quietly stop matching the stacks that launch
  creates. *Fix:* Add a STACK_PREFIX constant and a DEFAULT_REGION constant.
  Build the launch key from one helper shared with object_keys, for example a
  launch_key(run_prefix) function in common.py.
- **low** · archaeology-comment · `tools/cloud_performance/__main__.py:208` —
  worker_dispatch's docstring narrates incident history instead of stating the
  contract.
  The first sentence is rationale. The rest is a story of past launches, which
  belongs in the dated performance records or git history. worker_source_info
  in common.py already states the s3:// rule in present tense. *Fix:* Keep the
  first sentence. Replace the narrative with a pointer to worker_source_info's
  docstring, or with the present-tense rule that the dispatch must use the
  HTTPS sourceInfo.
- **low** · error-handling · `tools/cloud_performance/__main__.py:525` —
  `analyze` exits 0 when analysis is withheld; `analyze-local` exits 1.
  analysis.analyze returns `_withheld(...)` with `complete: False` for a worker
  failure, a missing marker, manifest drift, identity mismatch and so on. The
  CLI still exits 0, so a script or shell that checks `$LASTEXITCODE` cannot
  tell refused evidence from a valid analysis. The sibling subcommand already
  uses the opposite contract. *Fix:* Handle `analyze` the same way as
  `analyze-local`: after writing and printing, return 0 if result["complete"]
  else 1 (or return the value from one shared helper used by both branches).
  *(Severity corrected by the skeptic.)*
- **low** · interface · `tools/cloud_performance/analysis.py:13` — analysis
  imports the private `_integer` validator from common.
  `_integer` is underscore-private in common.py, yet analysis.py uses it to
  validate every raw sample (`_integer(sample.get(phase), f"sample.{phase}")`)
  and vendor IDs. It is really part of the module's shared contract, so the
  underscore misleads anyone changing it. *Fix:* Rename it to `integer` (or
  `require_integer`) in common.py and update both modules.
- **low** · magic-number · `tools/cloud_performance/analysis.py:333` —
  Bundle-manifest scope, bundled-worker path and power-plan GUID are unnamed
  literals repeated across modules.
  The scope string has to match between the writer (release.bundle) and two
  readers. The worker path has to match between launch and analysis. A GUID
  with no name says nothing about what it is: only the error message reveals
  that it is Windows' High performance plan. *Fix:* Define RELEASE_SCOPE and
  BUNDLED_WORKER in common.py and use them everywhere. Name the GUID, e.g.
  HIGH_PERFORMANCE_POWER_PLAN = "8c5e7fda-...".
- **low** · duplication · `tools/cloud_performance/analysis.py:499` —
  Frame-period rounding is written inline twice although a helper exists.
  The cloud path and the local path must use the same rounding as the C++
  bench. Only the local path calls the helper, so a change to the rule could
  update one path and miss the other. worker.ps1:416-418 carries a fourth copy
  that must match. *Fix:* Move _target_frame_ns above its first use and call it
  at lines 499 and 679.
- **low** · dead-code · `tools/cloud_performance/trace_benchmark.ps1:83` — Null
  check on Process.ExitCode can never be true.
  System.Diagnostics.Process.ExitCode is an Int32. Once the process has exited
  it never returns null, and reading it before exit throws rather than
  returning null. The branch suggests a failure mode that cannot happen. The
  line 73 `$processHandle = $process.Handle` read, which is what actually keeps
  ExitCode available, has no comment. *Fix:* Delete the null check, and add a
  one-line comment at line 73 saying why the handle is read.
- **low** · duplication · `tools/cloud_performance/worker.ps1:448` — nvidia-smi
  query and identity parsing are written three times.
  The before and after GPU identities must be parsed identically, or
  Compare-Object will report a false change or miss a real one. Three copies of
  the parse and two of the query invite drift. The same file also has two IMDS
  functions (lines 44-58) that each fetch a token the same way. *Fix:* Add
  `Get-NvidiaSmiRows` and `ConvertTo-GpuIdentity([string[]]$Rows,
  [string]$When)` functions and call them before and after measurement. Factor
  the IMDS token fetch into one function.
- **low** · archaeology-comment · `tools/master_watch.py:3` — Module docstrings
  carry dated incident counts that will go stale.
  These are snapshots of evidence at the time the tools were written, not
  contracts. The counts are already frozen at a date, and 'before this nobody
  was told' is history. .github/workflows/master-watch.yml repeats the same
  history ('Before it, a red master run was seen only by whoever opened the
  commit'). *Fix:* State the purpose in present tense (a red master run is
  rerun once and then reported; locked-output link failures are retried once).
  Move the dated counts to the commit message or a dated doc.
- **low** · test-hygiene · `tools/tests/test_ci_download.py:18` — tools/tests
  loads the code it tests in three different ways, with the loader boilerplate
  copied 14 times.
  Some tests import the tools as a namespace package, which needs the
  repository root on sys.path. One imports a sibling test module, which needs
  tools/tests on sys.path. Most use a hand-copied four-line importlib loader,
  some with the sys.modules registration (needed for dataclasses) and some
  without. Which tests run therefore depends on how discovery is invoked, and
  each new test copies the boilerplate again. *Fix:* Add one
  load_tool(relative_path) helper, which registers the module in sys.modules,
  to a shared tools/tests fixtures module. Use it everywhere, or switch
  everything to package imports, but choose one.
- **low** · stale-comment · `tools/tests/test_ci_gap_report.py:104` — The
  comment says wasm-debug has 32 tests; the assertion below it and AGENTS.md
  say 33.
  19 headless tests plus the 14 BROWSER_TESTS is 33. AGENTS.md's preset table
  also says wasm-debug registers 33 Debug entries with Chromium. The comment's
  count is out of date and contradicts the assertion two statements later.
  *Fix:* Change the comment to say '19 of wasm-debug's 33 tests', or drop the
  number and refer to len(report.BROWSER_TESTS). *(Severity corrected by the
  skeptic.)*
- **low** · test-hygiene · `tools/tests/test_ci_tested_tree.py:35` — Git
  fixture setup is pasted into three tests, and the fourth Git-history fixture
  skips the isolation.
  The same roughly 25-line isolated-repository setUp (empty hooks, empty global
  config, gpgsign off, fixed identity) is copied into three test files.
  test_ci_tested_tree's History class is the fourth Git-history fixture and
  inherits the developer's global config and hooks. With commit.gpgsign=true or
  a global commit hook, its `git commit` calls fail or behave differently from
  the other three fixtures. *Fix:* Move an isolated-repository helper into a
  shared tools/tests fixtures module, like cloud_performance_fixtures.py. Use
  it from all four files, including History in test_ci_tested_tree.py.
- **low** · test-hygiene · `tools/tests/test_verification_runner.py:15` — Tests
  borrow fixtures by constructing another TestCase and import production code
  through a test module.
  SM-51 removed this pattern from the cloud tests: shared fixtures live in a
  fixtures module, and nothing imports production code through a test file.
  Here test_verification_runner gets the planner module through
  test_verify_changes and runs another class's setUp by hand. The bare `import
  test_verify_changes` also works only when tools/tests itself is on sys.path.
  test_ci_gap_report's MainTests builds an ExecuteTests instance just to reach
  its fake() method. *Fix:* Move fake_tooling, write_caches and the
  temporary-root setup into a module-level fixtures file, and give
  ci_gap_report's fake runner the same treatment. Have both tests import those
  helpers and load verify_changes themselves. *(Severity corrected by the
  skeptic.)*
- **low** · test-hygiene · `tools/tests/test_verify_changes.py:9` — Unused `os`
  import.
  Ruff F401 reports it, and grepping the file for `os.` finds no use. *Fix:*
  Remove the import.
- **low** · other · `tools/verify_changes.py:26` — Each configuration's backend
  is matched to it only by position in a parallel tuple; tool requirements use
  bare slices.
  The backend for each configuration is known only by its position in a second
  tuple, and zip() silently truncates if a configuration is added to one tuple
  and not the other. NATIVE[:2] means 'the Direct3D presets' and
  CONFIGURATIONS[-2:] means 'the wasm configurations', but nothing names them,
  so reordering either tuple breaks the fxc/EMSDK requirements without an
  error. Line 338 loops over a one-element tuple. The file also runs many lines
  past 150 characters, unlike its sibling tools, which makes these rules harder
  to read. *Fix:* Replace the two tuples with one mapping, e.g. CONFIGURATIONS
  = {"x64-debug": "d3d11", ...}, or zip(..., strict=True) with an assertion.
  Name the subsets (D3D_PRESETS, WASM_CONFIGURATIONS), replace the one-element
  loop with a plain if, and wrap lines to match the sibling tools. *(Severity
  corrected by the skeptic.)*
- **low** · error-handling · `tools/verify_changes.py:331` — An unreadable
  cache is followed by a mismatch for every expected key.
  After the read fails, `values` stays `{}`, and the comparison loop then adds
  a `mismatch` check for CMAKE_HOME_DIRECTORY, the generator, each backend and
  so on. Those checks report "does not match plan: actual=None" when the real
  problem is that the cache could not be read. The single-element `for
  variable, suffix in ((...),):` loop is also needless indirection. *Fix:*
  After adding the cache-read failure, `continue` past the comparisons (or put
  them in an else). Replace the one-item loop with a plain `if
  environment.get("VCPKG_ROOT"):`.

## H6. Browser test runner consolidation

- **medium** · duplication · `tests/render/web/run_safari.mjs:89` — Second
  classic-WebDriver client, Safari tab-to-links probe and driver launcher,
  forked from website/acceptance and already missing its port-race retry.
  Safari and iOS driving is the flakiest part of CI, and recent commits keep
  hardening it. Hardening that lands in website/acceptance (port-race retry, a
  stricter readiness check, richer error context) does not reach the Safari
  sample runner, and the reverse is also true. *Fix:* Have run_safari.mjs
  import WebDriverError/command, safariTabsToLinks, freePort, driverArguments
  and startDriver from website/acceptance (both are plain Node ESM with no npm
  dependencies), or move them into a shared tools/webdriver module that both
  import. Keep only the sample-specific drive_sample/drive_bundle logic in
  run_safari.
- **low** · script-hygiene · `tests/app/web/window_input.mjs:358` — Hand-rolled
  step bookkeeping copies each step's description into two strings, and one
  pair has already drifted.
  The description used in the failure message and the one in the TAP-style "ok"
  line are maintained separately. They already differ at 358/362, so a passing
  run and a failing run name the same step differently. *Fix:* Add a helper
  such as `const pass = (what, fn) => { step++; const r = fn(`step ${step},
  ${what}`); console.log(`ok ${step} - ${what}`); return r; }` and route the
  touch, full-screen and close steps through it.
- **low** · naming · `tests/render/web/bundle_host.test.mjs:56` — Fixture
  option names are camelCase while sibling fixtures use snake_case.
  CONVENTIONS makes everything except types snake_case, and every other
  test-side identifier in this unit follows it. Those that run in the page,
  such as getContext and createLineSweeper, are rightly browser API names.
  *Fix:* Rename to graphics_throws, pads_forbidden, wasm_missing,
  import_failure and callback_calls.
- **low** · duplication · `tests/render/web/bundle_input.mjs:6` —
  bundle_input.mjs and host_input.mjs open with the same driver boilerplate.
  This is the --input driver contract run_page.mjs depends on (ready,
  strict_complaints, expected_complaint, prepare). Keeping two copies invites
  one driver drifting from it. *Fix:* Export from bundle_page.mjs a
  `driver_exports(state)` (or a `bundle_driver(drive)` factory) that returns
  ready/strict_complaints/expected_complaint/prepare, and re-export it in both
  drivers.
- **low** · stale-comment · `tests/render/web/bundle_page.mjs:1` — The
  shared-steps header leaves run_safari.mjs out of the list of consumers.
  A maintainer changing the steps or PRELUDE uses this list to see who is
  affected, and it misses the Safari CI job, which also depends on the wrapper
  and on steps' DevTools method set. *Fix:* Add run_safari.mjs's bundle mode to
  the list. It serves the wrapper itself and puts PRELUDE in a <head> script
  rather than a preload script.
- **low** · stale-comment · `tests/render/web/bundle_page.mjs:244` — The
  key-hold comment calls sample_key.mjs 'the same rule' but uses a different
  minimum.
  Either the 100/150 difference is deliberate and the comment should say why,
  or it is drift. As written, the comment tells a reader the two are the same.
  *Fix:* Name both minimums as constants. Either use one value or change the
  comment to say the shape is shared but the bundle steps hold 100 ms (with the
  reason), rather than 'the same rule'.
- **low** · error-handling · `tests/render/web/devtools.mjs:39` — DevTools.send
  leaves a pending waiter when socket.send throws, unlike
  FirefoxProtocol.command.
  A send on a closing or closed socket throws inside the executor. The promise
  rejects with a bare InvalidStateError that does not name the method, and the
  entry stays in `pending` until refuse_all. firefox_protocol.mjs:267-273
  catches this case, clears the waiter and prefixes the method. The message
  handler at :12 also runs JSON.parse and the listeners unguarded, so a
  listener's throw escapes into socket dispatch. FirefoxProtocol.receive guards
  against both (:300-311). *Fix:* Wrap socket.send in try/catch, delete the
  pending id and refuse with `${method}: ${error.message}`, as FirefoxProtocol
  does. Optionally guard listener calls the same way. *(Severity corrected by
  the skeptic.)*
- **low** · duplication · `tests/render/web/run_firefox.mjs:40` — Each sample's
  quit keys and target names are kept in both CMake and SAMPLES, unchecked.
  If a sample's menu changes, for example a new pause-menu row, the Chromium
  entry must be updated in CMake and the Firefox and Safari entries in
  run_firefox.mjs. A missed copy makes one browser's entry time out.
  Separately, the Safari runner depends on the Firefox runner module only to
  get this table. *Fix:* Keep the sample table (target, quit keys, why) in one
  place, for example a small samples.json or samples.mjs beside the runners
  that run_firefox and run_safari import. Have CMake read it, or add a Node
  test that parses the labrador_sample_page_test calls in CMakeLists.txt and
  asserts they match. *(Severity corrected by the skeptic.)*
- **low** · duplication · `tests/render/web/run_firefox.mjs:177` — Three copies
  of the deadline, complaint-listener and context-destroyed boilerplate in one
  file.
  These three entries must apply the same failure policy, yet the copies have
  already diverged: only drive_window_page honours expected_complaint. A fix to
  the logging, the 10 s per-call cap or the complaint format has to be made
  three times. *Fix:* Extract a `watched_context(protocol, context, {
  timeout_ms, log, expected_complaint, what })` helper that returns {
  remaining, complaints, assert_healthy, unsubscribe }, plus an
  `open_page(protocol, context, url, preload)` for the
  subscribe/preload/activate/navigate sequence, and use them in all three
  drivers. *(Severity corrected by the skeptic.)*
- **low** · naming · `tests/render/web/run_firefox.mjs:399` — WebDriver key
  tables hold invisible private-use characters instead of \uE0xx escapes.
  In any editor or diff these values look like empty strings, so a reader
  cannot tell which WebDriver key each one is. A 'cleanup' that really empties
  them would still look like no change. The same file and its tests already use
  explicit escapes for the same values, so the inconsistency is within one
  module. *Fix:* Spell every WebDriver key as a \uE0xx escape, as KEY_VALUES at
  run_firefox.mjs:53 does, in run_firefox.mjs BUNDLE_KEYS, run_safari.mjs
  KEY_VALUES/OPTION_KEY and the safari_protocol.test.mjs expectations. Better
  still, keep one shared, named WebDriver key table that both runners import.
  *(Severity corrected by the skeptic.)*
- **low** · consistency · `tests/render/web/run_firefox.mjs:687` — Usage errors
  exit 1 (with a stack trace) in some runners and tools, and 2 in others.
  Most of the repo uses 2 for 'could not run or bad invocation' and 1 for 'ran
  and failed'. Callers and CI steps cannot rely on that split while some
  runners treat a mistyped flag as a test failure and print an irrelevant stack
  trace. *Fix:* Have run_firefox and run_safari catch argument errors
  separately: print the message without the stack and exit 2. Make the bench
  and probe usage paths and consumer_check's operational refusals exit 2 as
  well. Write the convention down in CONTRIBUTING or docs/verification.md.
  *(Severity corrected by the skeptic.)*
- **low** · duplication · `tests/render/web/run_page.mjs:135` — Five separate
  extension-to-MIME tables for serving the same wasm pages.
  These servers all serve the same Emscripten outputs and already disagree on
  the charset parameter for .js. Adding an asset type (a .data or .woff2 file a
  sample starts loading) means finding every table. *Fix:* Have the three Node
  test runners import website/acceptance/server.mjs's exported CONTENT_TYPES,
  or a shared one in tests/render/web, and drop their local tables.
  lan_serve.py can stay separate (Python), but its header could name the
  canonical table.
- **low** · other · `tests/render/web/run_page.mjs:193` — The usage message
  leaves out options the header says either form accepts.
  The usage text is what someone sees after a typo, and it does not show
  options that ctest entries use, such as --emulate-media and --expect-console
  on LineSweeperPageTests. parse_arguments also never rejects unknown option
  names, so a misspelt --expect-consol is silently ignored instead of reaching
  this message. *Fix:* Add the options shared by both forms to fail_usage.
  Consider having parse_arguments reject unknown names, as
  run_firefox/run_safari parse_options already do with an allowed set.
- **low** · duplication · `tests/render/web/run_page.mjs:280` — Two stderr
  listeners keep identical 16 KiB tails of the browser's output.
  `seen` and `stderr_tail` hold the same text. One listener that appends once
  and then tests for the DevTools line would do, and the early-exit message at
  :297-298 and diagnostics() would report the same buffer. *Fix:* Keep a single
  `stderr_tail` updated in one 'data' listener that also runs the `DevTools
  listening on` match, and use it in both the refusal message and
  diagnostics().
- **low** · duplication · `tests/render/web/run_page.mjs:405` — Module-callback
  hook written three times, with diverging order of flag and original call.
  All three hooks intercept window.Module assignment and wrap
  onRuntimeInitialized, onQuit and onError, but they disagree on semantics.
  Chromium's records quit before the page's onQuit runs, so the run counts as
  quit even if the page's own onQuit throws. The Firefox and Safari hooks
  record it only after the callback returns. The same sample can therefore pass
  in one runner and fail in another for reasons unrelated to the browser.
  *Fix:* Export one hook builder (for example module_hook(global_name, extra)
  returning the source text) from a shared module. Have the three runners
  compose it with their runner-specific counters (WebGL draw counting, input
  protocol), and pick one ordering for the flag. *(Severity corrected by the
  skeptic.)*
- **low** · bug · `tests/render/web/run_page.mjs:631` — --emulate-media is
  validated after the browser is launched; fail_usage's process.exit skips
  teardown.
  process.exit never runs the finally block at lines 779-798. A malformed
  --emulate-media therefore leaves the headless Chrome running, the HTTP server
  listening and the labrador-browser-* temporary profile on disk. All other
  argument checks run in parse_arguments before anything starts. This is the
  only usage error that leaks resources. *Fix:* Move the --emulate-media split
  and check into parse_arguments, storing the parsed name and value on `parsed`
  (for example parsed.media = { name, value }). run() then only sends
  Emulation.setEmulatedMedia. *(Severity corrected by the skeptic.)*
- **low** · bug · `tests/render/web/run_page.mjs:783` — Teardown waits the full
  5 s for an 'exit' event that has already fired.
  If the browser has already exited (a crash, or the endpoint promise refused
  because it exited early), child.once('exit') is registered after the event
  and never fires. Every such run then stalls 5 s in teardown.
  run_firefox.mjs:585-594 `exited()` checks `child.exitCode !== null ||
  child.signalCode !== null` first for this reason. *Fix:* Return early when
  child.exitCode !== null || child.signalCode !== null before waiting, as
  run_firefox's exited() does, or reuse that helper.
- **low** · duplication · `tests/render/web/run_safari.mjs:45` — WebDriver key
  table duplicated between runners; Safari's copy uses invisible Private Use
  Area literals.
  In review, in diffs and in most editors, Safari's values look like empty
  strings. A mistyped or stripped code point cannot be seen. Keeping two tables
  means a key added to one runner (Tab, KeyA) is missing from the other. *Fix:*
  Export one escaped KEY_VALUES (a superset with Tab, KeyA and the Option key)
  from a shared module, or from run_firefox.mjs next to SAMPLES. Import it in
  run_safari.mjs and write every code point as \uE0xx. *(Severity corrected by
  the skeptic.)*
- **low** · duplication · `tests/render/web/run_safari.mjs:487` —
  run_safari.mjs re-implements run_firefox.mjs's server, file list, bundle
  substitute, key actions and sample hook although it already imports SAMPLES
  from it.
  The three runners are meant to enforce one page contract across Chromium,
  Firefox and Safari. Three hand-synced copies of the hook and server drift
  silently, and they already disagree on whether a draw on a lost context
  counts as a draw. A fix to request policy or hook semantics has to be made
  three times. *Fix:* Move the page server (allow-list, substitute, content
  types), BUNDLE_FILES/artifact_files, frames_expression, key_actions and one
  parameterised SAMPLE_HOOK (with a global name and an optional context-loss
  block) into a shared module beside bundle_page.mjs, e.g.
  tests/render/web/page_harness.mjs. Import it from run_firefox, run_safari
  and, where the shapes fit, run_page. *(Severity corrected by the skeptic.)*
- **low** · other · `tests/render/web/test_window.cpp:40` — One comment line
  runs past the wrap width of the rest of the block.
  This looks like an edit that was not re-wrapped. The whitespace lint does not
  catch it, and it stands out against the file's consistent wrapping. *Fix:*
  Re-wrap lines 38-41 to the block's width.

## H7. Test flakiness and runner robustness

- **medium** · bug · `tests/input/vibration_tests.cpp:307` — Vibration cases
  rely on no pad being plugged in; with a pad in slot 0 they fail and rumble
  it.
  On the XInput build GamepadReader::vibrate calls
  DirectX::GamePad::SetVibration
  (engine/input/xinput/gamepad_reader.cpp:21-25). That call returns true for a
  connected slot, so on a developer machine with a pad in slot 0 the
  CHECK_FALSE at line 307 fails and the motors run at 0.75/0.25 until the
  reader is destroyed. The case at line 275 has the same problem:
  `REQUIRE_FALSE(pads.connected(0))` aborts it whenever a pad is present. The
  file preamble at lines 16-19 says "No case here turns a motor", which is
  false on that machine. InputTests becomes hardware-dependent, so it passes on
  CI and fails on the owner's desktop. *Fix:* Drive the suspend/resume and
  Gamepads::vibrate paths through VibrationOutput<RecordingTransport> or an
  injectable reader, as the other cases in this file do. If the real reader
  must be exercised, use a slot index that is out of range, or skip the cases
  (doctest::skip) when sample() reports a connected pad, and say so in the
  preamble. *(Severity corrected by the skeptic.)*
- **medium** · bug · `tests/render/web/run_safari.mjs:565` — start_driver leaks
  the spawned safaridriver/geckodriver when it never becomes ready.
  The child is spawned at line 554, but both throws leave start_driver before
  it returns { stop }. main() assigns `driver = await start_driver(options)`,
  so `driver` stays undefined. The finally block's `if (driver) await
  driver.stop()` therefore never kills the process. A driver that is slow to
  answer /status is left running on the runner or developer machine, holding
  its port and possibly a Safari automation session for the next run.
  website/acceptance/browsers.mjs startDriver calls killDriver on the same
  failure. *Fix:* Kill the child (with the same tree/SIGKILL escalation as
  killDriver) before every throw after spawn. Better, reuse
  website/acceptance/browsers.mjs startDriver/killDriver instead of a second
  implementation. *(Severity corrected by the skeptic.)*
- **low** · flakiness · `tests/render/web/bundle_page.mjs:183` — Waits are
  bounded by attempt counts, so their real timeout varies with each runner's
  evaluate latency; three copies with diverging error filters.
  The deadline is attempts x (20 ms + one evaluate round trip). Under CDP an
  evaluate is fast. Under Safari it is two WebDriver HTTP calls
  (run_safari.mjs:144-145 sets timeouts before every execute_async). So the
  same step has very different real deadlines per runner. The 750-attempt wait
  at line 518 must outlast the page's ten-second context timer, yet nowhere is
  it expressed in milliseconds. The same hand-rolled loop is in
  tools/save_check/reload.mjs:10-22 (250 x 20 ms) and
  tools/web_pacing/page_input.mjs:108-116 (1200 x 25 ms). Each tolerates a
  different set of navigation errors: 'Cannot find (?:default execution
  context|context with specified id)', 'Cannot find context|Inspected target
  navigated', and 'Cannot find context with specified id'. *Fix:* Replace the
  attempt counts with a deadline in milliseconds, like webdriver.mjs waitUntil,
  and state the 10 s + margin explicitly for the context-timeout case. Export
  one shared until/navigation-error predicate from bundle_page.mjs and use it
  in reload.mjs and page_input.mjs. *(Severity corrected by the skeptic.)*
- **low** · flakiness · `tests/render/web/run_firefox.mjs:672` — Firefox
  profile cleanup fails the run on a transient Windows lock, where run_page
  retries.
  run_page.mjs:788-797 explains that 'The profile can stay locked for a moment
  after the browser exits on Windows' and retries ten times, 200 ms apart. The
  Firefox runner, used locally on Windows for the Firefox* entries, makes one
  attempt and turns an EBUSY/EPERM into a test failure. Separately, whenever
  the BiDi session never connected (line 677-679), the profile is deliberately
  retained in %TEMP% on every such failure and nothing ever removes it. *Fix:*
  Use rmSync's maxRetries/retryDelay (as website/acceptance/fixture.mjs:93
  does), or share run_page's retry loop. Also give retained profiles a cleanup
  path, such as removing stale labrador-firefox-* directories on start.
  *(Severity corrected by the skeptic.)*
- **low** · flakiness · `tests/render/web/run_page.mjs:568` — Chromium runner's
  DevTools commands have no deadline, and setup runs outside the page timeout.
  DevTools.send (devtools.mjs:39) registers no timer. Only the endpoint (30 s),
  Browser.getVersion (10 s) and the later Promise.race with timeout_ms are
  bounded. If the browser stops answering during
  attach/enable/prepare/navigate, the run hangs until ctest's default 1500 s
  timeout kills node. The finally block's Browser.close, profile removal and
  child.kill then never run. firefox_protocol.mjs times out every command
  (default 10 s), and run_safari.mjs's webdriver_command defaults to 30 s.
  *Fix:* Give DevTools.send an optional per-command timeout, as
  FirefoxProtocol.command has. Alternatively, start the timeout_ms deadline
  before Target.getTargets so setup is covered by the same race. *(Severity
  corrected by the skeptic.)*
- **low** · flakiness · `website/acceptance/browsers.mjs:229` — Fixed 5-second
  sleep used as 'Safari is ready' after restarting the simulator's Safari.
  restartSimulatorSafari is the recovery path for the flaky iOS retry
  (suite.mjs:1699). It always costs 5 s, yet the sleep proves nothing about
  readiness. A failed launch is only logged, then the code sleeps and proceeds
  anyway. The first-boot path at line 218 launches the same app with no sleep
  at all, so the two paths assume different readiness. *Fix:* Replace the sleep
  with a readiness condition, such as polling the driver's /status or retrying
  newSession with a deadline. If a settle time is really needed, name it as a
  constant with its reason and use it on both launch paths. *(Severity
  corrected by the skeptic.)*
- **low** · flakiness · `website/acceptance/suite.mjs:1284` — Waiting for typed
  text into the search box uses 5 s in one check and 3 s in another.
  search() at line 1165 waits for exactly the same condition with `{ timeoutMs:
  3_000, message: 'the typed text' }` and then retypes. searchSubmission has no
  retry and a different allowance. The two literals will drift independently,
  and searchSubmission fails on the dropped-first-text simulator behaviour that
  search() documents and works around. *Fix:* Share one named constant and one
  'type and confirm it arrived, retyping once' helper between search() and
  searchSubmission().

## H8. Core, math, scene, app, audio and input test hygiene

- **medium** · archaeology-comment ·
  `tests/app/application_options_tests.cpp:70` — Test name and comment are
  defined by history ("already here", "before view_capacity joined").
  A test name is permanent output in every failure report, and this one
  describes the order in which code was written, not behaviour. CONVENTIONS: "a
  sentence about what it was belongs to the history." "The split" refers to a
  past change the reader cannot see. *Fix:* Rename it to the behaviour, for
  example "a zero frame rate, thread count, window minimum or class name is
  rejected". Reword the comments in present tense ("each of these reaches a
  divisor"; "the thread ceiling is a property of the machine, not of the view
  capacity").
- **medium** · stale-comment · `tests/app/dom_events_tests.cpp:17` —
  dom_events_tests.cpp says it is browser-only, but it is compiled on every
  platform.
  The comment states the opposite of the build. A maintainer could guard
  browser-only code in this file on that belief and break the native AppTests
  build, or overlook that native CI already covers the translator. *Fix:* Say
  that the file is compiled into AppTests everywhere and that native builds
  compile engine/app/web/dom_events.cpp into the test target. Remove 'as
  dom_events_tests.cpp is' from standard_gamepad_tests.cpp:14. Line 14 should
  also name keyboard_input_tests.cpp, beside window_tests.cpp, as the Win32
  half.
- **medium** · weak-test · `tests/assets/asset_manifest_loader_tests.cpp:95` —
  "names the path" / "names itself" / "says so" cases assert only the exception
  type.
  The case name promises a T6 diagnostic (the path in the message), but only
  the type is checked. A loader that threw `runtime_error("")` would pass. The
  same pattern appears in tests/assets/json_tests.cpp:75 ("a file that is not
  there names itself", CHECK_THROWS_AS only),
  tests/assets/sound_bank_loader_tests.cpp:388 ("a definition that is not there
  names itself") and sound_bank_loader_tests.cpp:331 ("...is a content bug, and
  it says so", whose comment adds "The throw names the file"). Sibling cases in
  these same files already use CHECK_THROWS_WITH_AS / doctest::Contains for
  exactly this purpose. *Fix:* Use CHECK_THROWS_WITH_AS(...,
  doctest::Contains("no_such_manifest.json"), std::runtime_error) and the
  equivalent for the other three. For the duplicate-effect case, also check
  that the message contains 'bang' and the definition's path.
- **medium** · archaeology-comment · `tests/audio/null_tests.cpp:19` — Audio
  test preambles narrate the survey that produced them and the state before the
  null backend existed.
  CONVENTIONS: "Archaeology ... is never a comment." These passages describe
  what the code used to be and which survey asked for it, and "only
  implementation was DirectXTK's" is no longer true (null and webaudio backends
  exist). The census numbers ("eight of thirteen", "five sites") can go stale.
  *Fix:* Keep the contract and rationale (which side of the seam the clamp is
  on, what the null model cannot assert). Delete the survey provenance, the
  "now" and "had never executed" narrative and the thirteen/eight/five census.
- **medium** · stale-comment · `tests/audio/null_tests.cpp:27` — Line-number
  citation into PHILOSOPHY.md points at the wrong text.
  The citation sends a reader to an unrelated section.
  check_doc_citations.cmake only checks document and section names, so it
  cannot catch a line number that has drifted. CONVENTIONS asks for a citation
  a machine can check. *Fix:* Cite the trade-off by its T-number or section, or
  quote the sentence without a line range. Drop the `:632-637` suffix.
  *(Severity corrected by the skeptic.)*
- **medium** · stale-comment · `tests/collision/tunnelling_tests.cpp:46` —
  Sniper test cites ColourWars files that are not in this repo and claims a
  failure mode it cannot have.
  The comment says its numbers come "from the tree", but none of the four cited
  files is in this repository. It also says the test "will fail the moment the
  content is corrected". It cannot: the four values are hard-coded `constexpr`
  literals and nothing reads game content. So the test is a permanent
  arithmetic fact about another project's tuning, and its own reminder to
  delete it can never fire. The test name and comment present it as a live
  record of 'the shipping' game, which is false here. *Fix:* Either delete the
  case (the decision belongs to ColourWars), or rewrite it as a neutral worked
  example of max_safe_speed with made-up numbers. Remove the file citations and
  the claim that it fails when content changes. *(Severity corrected by the
  skeptic.)*
- **medium** · stale-comment · `tests/core/name_table_tests.cpp:120` —
  Rationale cites a SpriteFrame RECT* that no longer exists, and the test
  checks handles, not references.
  The comment's premise is false today: sprite_frame.h explicitly says it hands
  out a RectangleI, not a RECT*. The test name promises reference stability,
  but the body holds a `Handle` and re-reads through `get()`, which is index
  stability. A reference taken before the adds would dangle on reallocation,
  which is the opposite of the name. "64 reallocations" is also wrong: 64
  push_backs do not cause 64 reallocations. *Fix:* Rename it to "a resolved
  handle stays valid as the table is filled". Drop the RECT* paragraph and the
  reallocation count. If reference stability after loading is the contract,
  cite name_table.h's own statement instead.
- **medium** · stale-comment · `tests/core/state_context_tests.cpp:48` —
  Comments cite ColourWars call-site counts, story IDs and game content that do
  not exist in the engine repo.
  Since the split, 'the game' in these engine tests means ColourWars. The 'ten
  live call sites' count, the 'C3' ID (grep of docs/design, TODO.md and
  BACKLOG.md finds no C3) and 'paint-shooter' cannot be checked from this repo.
  The level extent and set_velocity_y belong to another codebase, so these
  comments are unverifiable census claims. focus_tests' 'today' claim is a
  temporal statement already outdated by the samples. *Fix:* Restate each
  rationale in engine terms: 'world coordinates in the thousands', 'a menu
  column', 'a state transitioning inside its own update'. Delete the counts and
  the story IDs.
- **medium** · archaeology-comment · `tests/core/thread_pool_tests.cpp:10` —
  Test comments narrate history: past bugs, prior implementations, and the
  commit that added them.
  CONVENTIONS#comments: archaeology 'is never a comment'. Past-tense accounts
  of what the code used to do can only age, and several already have. SM-47
  cleaned implementation and build comments but did not cover tests. A short
  sentence in the present tense saying what each case pins would carry the same
  value. *Fix:* Run a comments-only pass over the unit's tests in SM-47's
  style. Keep the present-tense contract each case pins ("a degenerate triangle
  contains nothing") and drop the narratives of what was, was replaced, or was
  deleted.
- **medium** · test-hygiene · `tests/math/shape_intersect_tests.cpp:271` —
  'Just touching' and 'just not touching' fixtures do not sit at the boundary
  their comments name.
  Circle a has radius 10 at the origin and b has radius 5 at (10,5). The
  centres are about 11.18 apart against a radius sum of 15, so b overlaps by
  about 3.8 units. That is not 'just touching'. In the circle-triangle and
  circle-quad cases, the 'just not touching' shapes have their nearest point at
  (10,10), about 14.1 from the centre and over 4 units clear. Their 'touching'
  siblings sit on y=0. The y offset moved these fixtures away from the
  boundary, so the boundary these comments describe is never tested. *Fix:* Put
  the fixtures on the boundary: Circle(15.0f - epsilon_f, 0.0f, 5.0f) for
  touching. For the triangle and quad 'not touching' cases, use y=0 to match
  their touching siblings, e.g. Triangle((10+eps,0),(20,0),(10+eps,20)) and
  RectangleF(10+eps, 0, 10, 10).
- **medium** · test-hygiene · `tests/math/shape_tests.cpp:49` — Edge-order test
  claims every slot matches its accessor but skips Quad and RectangleRotated
  slots 1 and 2.
  The comment says order mistakes once put three of four faces on the wrong
  side, yet the test leaves edge_1/edge_2 unchecked for two of the four shapes.
  A swap of the middle two edges in Quad::edges() or RectangleRotated::edges()
  would pass while the test claims to pin the contract. *Fix:* Add
  CHECK(quad_edges[1] == quad.edge_1()); CHECK(quad_edges[2] == quad.edge_2());
  and the same two checks for rr_edges.
- **low** · convention · `tests/app/application_loop_tests.cpp:92` — Public
  data members carry the private trailing underscore.
  CONVENTIONS: private members end in `_` and public fields are bare. Here the
  suffix wrongly marks these as private while the cases read them directly.
  *Fix:* Rename them to capture_before, capture_after and steps (public, bare),
  or make them private with noun accessors. *(Severity corrected by the
  skeptic.)*
- **low** · duplication · `tests/app/application_loop_tests.cpp:169` —
  Hidden-Application option setup is copy-pasted into eight cases.
  Every new live-Application case copies four lines, and a change to the
  headless recipe (for example a new option every hidden test needs) has to be
  made eight times. *Fix:* Add a `hidden_options(const wchar_t* class_name)`
  helper in each file's unnamed namespace, as join_state_tests.cpp does.
- **low** · other · `tests/app/application_options_tests.cpp:103` — StepTimer
  cases sit in the app options test and rely on a transitive include.
  About half the case tests core's StepTimer rejecting zero, negative, infinite
  and NaN periods. It sits in an app test file and compiles only because of an
  indirect include. Removing step_timer.h from application.h would break it,
  and the core module's own coverage is split. *Fix:* Keep the
  options-validation half here. Move the SetTargetElapsed*/Tick assertions to a
  tests/core StepTimer test that includes engine/core/step_timer.h directly, or
  at least include it here.
- **low** · test-hygiene · `tests/app/content_root_tests.cpp:154` — Fixed-name
  temp folders are never cleaned up after a failure and can read leftovers from
  a crashed run.
  A failing REQUIRE skips remove_all, and the next run reuses the same folder.
  That is the failure mode the shared ScratchFile helper was written to
  prevent, so these cases bypass the suite's own fixture policy. *Fix:* Add a
  small RAII scratch-directory helper beside ScratchFile in
  tests/scratch_file.h (unique name, removal in its destructor) and use it in
  both cases. *(Severity corrected by the skeptic.)*
- **low** · stale-comment · `tests/app/startup_child.cpp:17` — The child names
  window_tests.cpp as its launcher; startup_info_tests.cpp launches it, and the
  exit codes are defined twice under different names.
  Both comments name the wrong file. That file has no names for these codes.
  The protocol is two unrelated sets of literals with different identifiers,
  and the failure code (30) is not named at all on the reading side, so the two
  sides can drift apart silently. *Fix:* Change both comments to name
  startup_info_tests.cpp. Put the three codes in one small shared header under
  tests/app/ that both translation units include, or at least use the same
  names on both sides. *(Severity corrected by the skeptic.)*
- **low** · duplication · `tests/app/startup_child.cpp:58` — The startup child
  copies Application::create_window's option mapping by hand.
  The test claims to make 'a window made as Application makes one from default
  ApplicationOptions'. Nothing ties the two lists together: a field added to
  WindowOptions and forwarded by Application would be left out here, and the
  STARTUPINFO test would keep passing on a different window from the one games
  get. *Fix:* Move the ApplicationOptions-to-WindowOptions mapping into one
  function, for example a free `window_options_from(const ApplicationOptions&,
  Vector2I client)` in the app module. Have Application::create_window and the
  child both call it. *(Severity corrected by the skeptic.)*
- **low** · archaeology-comment · `tests/app/window_tests.cpp:23` — Window and
  content-root tests narrate the review probes they came from.
  How the tests came about belongs in git history. docs/review/ is frozen, so
  these comments tie live tests to a historical ledger. *Fix:* Keep the
  present-tense statements of what each case pins. Delete the provenance
  sentences ("were probes", "the probe the review ran", "the restore fix", "The
  review's reproduction").
- **low** · duplication · `tests/app/window_tests.cpp:374` — Win32 lParam bits
  are spelled inline repeatedly, and the Loss table is defined twice.
  Magic shift counts (24 = extended, 30 = previous state, 31 = transition) are
  unexplained at each use. The duplicated table means a new focus-loss message
  has to be added in two places. *Fix:* Hoist one `focus_losses` array and
  `key_down_lparam` / `key_up_lparam` helpers (named for the
  extended/previous/transition bits) into the file's unnamed namespace.
- **low** · duplication · `tests/app/window_tests.cpp:713` — size_entry
  duplicates RecordingNotify's size format, and client_of ignores the Win32
  result.
  The log format is defined twice, so a change to one breaks comparisons
  without the cause being obvious. An unchecked GetClientRect turns a failed
  call into a 0x0 size, and the case then fails on an unrelated size mismatch.
  *Fix:* Move size_entry above RecordingNotify and have on_window_size_changed
  call it. REQUIRE the BOOL results in client_of and window_extent.
- **low** · error-handling · `tests/app/window_tests.cpp:950` — PerMonitorAware
  ignores a failed SetThreadDpiAwarenessContext.
  If the call fails, it returns NULL. The guard then 'restores' NULL, and three
  cases run on an unaware thread while appearing to test per-monitor behaviour.
  *Fix:* REQUIRE(previous_ != nullptr) in the constructor, or move the existing
  AreDpiAwarenessContextsEqual REQUIRE into the guard so that every case gets
  it.
- **low** · include-hygiene · `tests/assets/asset_manifest_loader_tests.cpp:1`
  — Include groups are run together in some files, and others rely on
  transitive standard headers.
  This is inconsistent with sibling files and with CONVENTIONS' grouped order
  (own/doctest, engine, external, standard). Leaning on transitive includes
  breaks when an engine header stops pulling them in. *Fix:* Separate the
  groups with blank lines as the other tests do, and add the missing
  <vector>/<array> includes.
- **low** · test-hygiene · `tests/assets/asset_manifest_loader_tests.cpp:187` —
  Later cases sit outside the file's TEST_SUITE.
  Running the suite by name (`-ts=AssetManifestLoaderTests`, `-ts=JsonTests`)
  silently skips these cases. That membership is accidental, the result of
  appending cases rather than a choice. *Fix:* Move the appended cases inside
  the existing TEST_SUITE blocks, or drop TEST_SUITE from these files
  consistently. *(Severity corrected by the skeptic.)*
- **low** · stale-comment · `tests/assets/resource_loader_tests.cpp:20` —
  Configuration counts in test preambles are stale ("ALL FOUR", "all six").
  These counts are fragile census claims that are already wrong, the same kind
  SM-47 removed elsewhere. The point being made is that the file runs
  everywhere, not a particular number. *Fix:* Write "in every configuration"
  (as sprite_sheet_loader_tests.cpp and sound_bank_loader_tests.cpp already do)
  and drop the number. *(Severity corrected by the skeptic.)*
- **low** · stale-comment · `tests/assets/sound_bank_loader_tests.cpp:47` —
  Comment about pinning wave order sits on the source_path assertion.
  The rationale belongs to the `definition.waves == waves` check at line 275.
  Attached to the source_path check, it misdirects the reader, and source_path
  itself gets no stated reason. *Fix:* Move the comment above line 274/275, and
  give the source_path check its own one-line reason, or none.
- **low** · duplication · `tests/audio/audio_resources_failure_tests.cpp:174` —
  Five cases repeat the hook and warning-mode bookkeeping.
  The restoration contract is the core safety property of this fixture, and it
  is written out six times. A change to what a scope restores, such as a new
  CRT setting, has to be made in every copy. *Fix:* Add a helper, for example
  `struct CrtBaseline { hook; mode; void require_restored(const
  AllocationFailureSample&) const; }`, or a function that runs one refusal and
  checks restoration. Have each case call it once. *(Severity corrected by the
  skeptic.)*
- **low** · stale-comment · `tests/audio/audio_resources_tests.cpp:25` —
  'compiles in all six configurations' matches no current census.
  The count matches neither presets, CI configurations nor backends, so a
  reader cannot check it. The claim intended is 'every configuration'. *Fix:*
  Change it to 'compiles in every configuration'.
- **low** · swallowed-error · `tests/audio/content/make_adpcm_bank.py:36` — WAV
  chunk walker silently truncates a chunk that runs past the end of the file.
  Python slicing clamps silently, so a truncated or corrupt ping_adpcm.wav
  produces a short chunk instead of an error. A data chunk cut at a block
  boundary still passes `len(payload) % block_align == 0`, and the fact-chunk
  duration (which is not checked against the payload) would then describe more
  samples than the bank holds. The result is a fixture whose decode silently
  differs from the pinned msacm32 hash. A repeated chunk tag is also silently
  overwritten by the last one. *Fix:* Raise if `at + 8 + size > len(data)` or
  if a tag repeats. Check `duration <= (len(payload) // block_align) *
  samples_per_block`.
- **low** · script-hygiene · `tests/audio/content/make_adpcm_bank.py:71` —
  Fixture generator writes unnamed format constants and validates input with
  assert.
  The WBND signature, tool and header versions, and bank flags are bare
  literals, while the script names other layout sizes (header_size, bank_size,
  entry_size). The input checks use `assert`, which `python -O` removes, so a
  wrong WAV would then produce a corrupt fixture without any error. *Fix:* Name
  the signature and version constants next to STANDARD_COEFFICIENTS. Raise
  SystemExit or ValueError for the input checks instead of using assert.
- **low** · test-hygiene · `tests/audio/null_tests.cpp:1` — Backend-specific
  tests are filed inconsistently: subfolders for render, flat files for audio,
  input and render/null.
  Finding a backend's tests needs a different rule in each module, and the null
  render backend is the only render backend whose tests are not in a backend
  folder. Two files are also both named null_tests.cpp, in different modules.
  *Fix:* Choose one rule, such as tests/<module>/<backend>/ for backend-only
  sources, and apply it when these files are next changed. Keep names and ctest
  registration the same.
- **low** · weak-test · `tests/audio/null_tests.cpp:205` — Comment says every
  recorded call names the effect's voice; only one pair is compared.
  Calls 2-7 (stop, stop, pause, resume, set_volume, set_pitch) are never
  checked for the voice they target. A SoundBank that forwarded one of those
  verbs to the wrong voice index would pass, which defeats the check the
  comment describes. *Fix:* Loop over played[1..8] and CHECK(played[i].voice ==
  played[1].voice).
- **low** · weak-test · `tests/audio/null_tests.cpp:416` — "resumes at it" is
  claimed but resume is never exercised.
  The comment promises that the paused effect resumes at the new level, but the
  case only checks that it is still paused. No resume_effect call is made, so
  nothing confirms a resume would not reapply a stale level. *Fix:* Call
  fixture.second->resume_effect(second_theme), then assert that no further
  set_voice_volume with the old level is recorded and the state is playing.
  Otherwise trim the comment to "took the level while paused".
- **low** · weak-test · `tests/audio/sound_bank_tests.cpp:154` — "still runs
  the arithmetic it owns" cannot observe the arithmetic and repeats the
  previous case.
  Its own comment says "A bank with nothing behind it still cannot show you the
  result". The case asserts only that out-of-range levels do not throw, which
  the "plays nothing, and every verb still runs" case at line 92 already
  covers. The name promises more than the case can check, and the clamp is
  really pinned in null_tests.cpp. *Fix:* Fold the out-of-range arguments into
  the line-92 case as "nonsense levels are accepted", and drop the claim about
  arithmetic. Leave the clamp to null_tests.cpp. *(Severity corrected by the
  skeptic.)*
- **low** · weak-test · `tests/audio/webaudio_tests.cpp:190` — "a group's
  volume reaches this device as plain level calls" asserts no level.
  The name claims the group product reaches the Web Audio device, but every
  assertion is CHECK_NOTHROW or a play/pause state. No volume is read back, so
  a backend that dropped or mis-multiplied the level would pass. As written,
  the case only shows that the calls do not throw. *Fix:* Rename it to what it
  shows (group calls are accepted and do not disturb voice state), or expose
  the backend's last-set voice gain to tests and assert the products (0.25,
  0.125, then 0.5). *(Severity corrected by the skeptic.)*
- **low** · test-hygiene · `tests/audio/xaudio2_recovery_tests.cpp:76` — The
  critical_error assertions check the fake's own field, which DeviceRecovery
  never reads.
  These checks can only fail if the test's own fake changes. They test nothing
  in the engine, but the case name suggests the recovery policy handles a
  critical-error flag. A reader would take it as cover for behaviour the code
  does not have. *Fix:* Remove the critical_error field and its assertions.
  Rename the case to the behaviour it actually covers, for example 'keeps
  retrying a reset that finds no output, and a fresh loss starts a new
  interval'. If the DirectXTK quirk needs recording, keep the comment in
  device_recovery.h only. *(Severity corrected by the skeptic.)*
- **low** · duplication · `tests/audio/xwb_file_tests.cpp:33` — Three tests
  hand-roll the whole-file reader that engine/core/byte_reader.h already
  exports.
  Three copies of an unchecked read (no check after `file.read`) sit beside a
  tested engine helper that reports short reads. The engine's own JSON loader
  also reads files a second way, with different exception types for a missing
  file (runtime_error vs read_file_bytes' out_of_range). *Fix:* Replace the
  three test helpers with `labrador::read_file_bytes(...)`, and move a
  `put_u32_le` into the shared tests helper header next to ScratchFile. Decide
  whether read_json_file should use read_file_bytes, and align the missing-file
  exception type if it does.
- **low** · duplication · `tests/collision/broad_phase_tests.cpp:69` —
  Hand-rolled xorshift Rng duplicates engine/core/random.h, which exists for
  exactly this reason.
  There are two sources of cross-platform deterministic randomness in one test
  directory. The engine's Random states the same rationale and has the same API
  shape (uniform_float plays the role of Rng::in). The local copy also uses
  unqualified uint32_t, where the engine spells std::uint32_t. *Fix:* Replace
  Rng with labrador::Random (seeded), and Rng::in(low, high) with
  Random::uniform_float(low, high). Delete the struct and its comment.
  *(Severity corrected by the skeptic.)*
- **low** · other · `tests/collision/broad_phase_tests.cpp:193` — Scenario
  behaviour keyed on a string comparison of its display name.
  The Scenario struct is the table that drives the loop, yet one behaviour is
  hidden behind matching its CAPTURE label. Renaming the label silently drops
  the giant, and with it the large-object coverage the comment says the
  scenario exists for. This is also the only reason <string> is included.
  *Fix:* Add a `bool with_giant` field to Scenario, set it in the table, and
  test the field.
- **low** · const-correctness · `tests/collision/broad_phase_tests.cpp:229` —
  const object declared only to be const_cast back for find_contacts.
  find_contacts takes non-const CollisionObject* (dispatch may mutate).
  Declaring the object const and casting it away is the only const_cast in the
  unit. It gains nothing and would be undefined behaviour if the callee ever
  wrote through the pointer. *Fix:* Declare `Box only(...)` without const and
  push `&only`.
- **low** · duplication · `tests/collision/contacts_tests.cpp:31` — Rectangle
  CollisionObject stub is written four times across collision and scene tests.
  CollisionObject's pure-virtual surface is duplicated four times, so any
  change to that interface (a new pure virtual, a signature change) must be
  made in four test files. tests/ui/stub_widget.h already solves the same
  problem for UiWidget with one shared stub header. *Fix:* Add
  tests/collision/stub_collider.h (alongside stub_widget.h's pattern): a
  rectangle collider with layer/mask/tag, for_deletion and a contact log. Have
  the four files use or derive from it, keeping file-specific extras (draw
  markers, make_fixed, retire_on_contact) as small subclasses. *(Severity
  corrected by the skeptic.)*
- **low** · include-hygiene · `tests/collision/partitioner_tests.cpp:92` —
  std::min/std::max and std::is_same_v used without their headers.
  Both files rely on transitive includes. contacts_tests.cpp and
  state_context_tests.cpp include <algorithm> explicitly for the same purpose.
  A change to partitioner.h or registry.h could break these tests for an
  unrelated reason. *Fix:* Add #include <algorithm> to partitioner_tests.cpp
  and #include <type_traits> to registry_tests.cpp.
- **low** · convention · `tests/core/name_table_tests.cpp:37` — PascalCase
  wrapper namespaces violate the one-lowercase-word namespace rule.
  CONVENTIONS#namespaces: 'Namespaces | one lowercase word'. These namespaces
  also add nothing, because each one wraps a TEST_SUITE of the same name. No
  sibling test file uses the pattern. *Fix:* Remove the namespace wrappers and
  keep the TEST_SUITE, as the other files do. *(Severity corrected by the
  skeptic.)*
- **low** · test-hygiene · `tests/core/registry_tests.cpp:219` — Compile-time
  type identity checked with a runtime CHECK, without <type_traits>.
  The comment itself says this is a compile-time property. Checking it at
  runtime only reports a mismatch when the test runs instead of failing the
  build, and the file relies on a transitive include for std::is_same_v. *Fix:*
  Use static_assert(std::is_same_v<...>) at namespace scope (or keep it in the
  case), and #include <type_traits>.
- **low** · duplication · `tests/core/state_context_tests.cpp:194` — Five
  copies of a comment, each claiming its own test is 'the first thing in the
  tree' to depend on a deviceless Renderer.
  A claim of being 'the first' can be true of at most one place, and here it is
  pasted five times. It is also stale: null render tests, cull/fanout/order
  tests and text_widget_tests now all depend on a deviceless Renderer. This is
  copy-paste noise that turns census archaeology into five false sentences.
  *Fix:* Delete the comment from all five sites, or keep one neutral line once,
  near a shared helper (e.g. "a Renderer needs no device to be constructed").
  *(Severity corrected by the skeptic.)*
- **low** · duplication · `tests/core/state_context_tests.cpp:590` — Two cases
  build the same throwing-init drain and differ only in how the context ends.
  These are the same fixture with two endings (scope exit and clear()). The
  setup lambda is duplicated verbatim, so a change to the failing-drain
  scenario has to be made twice. *Fix:* Fold both into one TEST_CASE with
  SUBCASE("clear()") and SUBCASE("leaving scope"), or extract a helper that
  arms the failing drain on a given state.
- **low** · stale-comment · `tests/core/thread_pool_tests.cpp:128` — Comment
  claims Scene::draw waits on the pool unconditionally; it returns early before
  touching it.
  The comment is false: with one view or fewer, Scene::draw returns before
  reaching the pool. With two or more it always submits at least one range. No
  caller performs the empty wait this comment cites as the reason for the test.
  *Fix:* Justify the case on the pool's own contract (an empty wait returns),
  or delete the Scene::draw sentence. *(Severity corrected by the skeptic.)*
- **low** · convention · `tests/input/deadzone_tests.cpp:6` — Test namespaces
  are PascalCase, against the one-lowercase-word namespace rule.
  CONVENTIONS: "PascalCase names a type ... Namespaces: one lowercase word."
  Newer siblings in the same folders (vibration_tests.cpp,
  standard_gamepad_tests.cpp, null_tests.cpp) use only an unnamed namespace, so
  there are now two styles. The TEST_SUITE string already groups the cases.
  *Fix:* Drop the named namespace wrappers. The TEST_SUITE names stay, and
  helpers go in the existing unnamed namespace. *(Severity corrected by the
  skeptic.)*
- **low** · archaeology-comment · `tests/input/deadzone_tests.cpp:23` — Input
  test comments recount defects in former client code.
  Each states what was or what someone once did, not what the code requires.
  The webaudio comment depends outright on git history, which is the
  archaeology CONVENTIONS rules out. *Fix:* Restate each as the present
  contract ("rescaled so the live range starts at 0, not offset by the
  threshold"; "a button held across a transition is not a press"). Remove the
  defect and commit references.
- **low** · archaeology-comment · `tests/input/direction_tests.cpp:12` — File
  preamble tells the history of a header's false claim instead of what the file
  tests.
  This describes what a previous version of another file wrongly said, which
  CONVENTIONS names as archaeology ("a sentence narrating what this very
  comment used to say"). A reader of the current file learns nothing about
  pad_direction or DirectionRepeat from it. *Fix:* Replace it with one
  present-tense line, for example "pad_direction and DirectionRepeat: the
  stick-to-menu-direction producer; hold-to-repeat gets the most cases because
  it is the part that goes wrong." *(Severity corrected by the skeptic.)*
- **low** · weak-test · `tests/input/direction_tests.cpp:191` — Repeat-rate
  test states an exact rhythm but accepts anything from 2 to 9 moves.
  The default rhythm over 60 frames gives a fixed count of about 7. A
  regression that halved the repeat rate, or dropped the long first delay,
  would still land inside (1, 10). The comment gives the expected count but the
  check is loose. *Fix:* Assert the exact count that the default intervals
  produce (computed from the DirectionRepeat defaults), or at least the moves
  seen before 0.4 s and after it.
- **low** · stale-comment · `tests/input/direction_tests.cpp:234` — Long-frame
  comment's arithmetic does not match the 0.65 step.
  The step is 0.65 s. Past the 0.4 s first delay that leaves 0.25 s, which is
  two and a half repeats at the 0.1 s interval, not four. Neither "a quarter of
  a second in one frame" nor "four repeats' worth" describes the value passed,
  so a reader checking the no-burst rule against the comment gets confused.
  *Fix:* Say "0.65 s in one frame: past the first delay by 0.25 s, two repeats'
  worth", or pick a step that matches the existing wording.
- **low** · duplication · `tests/input/gamepad_tests.cpp:97` — Cases repeat
  assertions already made earlier in the same file.
  This is line 81 verbatim. "an edge between two occupied frames is unaffected"
  (line 140) likewise repeats lines 80 and 91. In
  tests/input/deadzone_tests.cpp:399, `apply_deadzone(0.5f, 0.5f) == 0` repeats
  line 390. Duplicate cases inflate the suite without covering anything new,
  and they read as if they test something distinct. *Fix:* Delete the duplicate
  cases, or move their explanatory comments onto the original assertions.
- **low** · stale-comment · `tests/input/keyboard_tests.cpp:1` — Comment starts
  mid-sentence, and its "BEFORE doctest" ordering claim is contradicted by
  siblings.
  The subject of the first sentence is missing; the file has started with "//
  BEFORE" since it was created. Other files include <ostream> after doctest and
  build, so the ordering claim is wrong: the type only has to be complete where
  the assertion is instantiated. The comment also breaks CONVENTIONS' rule that
  a test opens with doctest. *Fix:* Move <ostream> into the standard-library
  group after doctest and reword: "<ostream> is needed wherever an assertion
  compares std::string_view: doctest only forward-declares basic_ostream."
- **low** · weak-test · `tests/input/keyboard_tests.cpp:85` — Bounds guard
  tests cannot fail: the bits they read are never set.
  KeyboardState::is_down guards with `if (!is_key(key)) return false;`
  (engine/input/keyboard.cpp:79). The test only sets Key::a's bit, so bit 0
  (none) and bit `count` are zero whether or not the guard exists. Remove the
  guard and the test still passes. tests/input/mouse_tests.cpp:57-65 ("the
  bounds are not buttons") has the same flaw with `at(0, 0, { MouseButton::left
  })`. The comment says the answer must be "no rather than a read of whatever
  bit they would index", but the test cannot tell those two apart. *Fix:* Build
  the state with every bit set (keys[0] = keys[1] = ~0ull; buttons = 0xFF) and
  then assert that is_down(none) and is_down(count) are false. Only the guard
  can produce that answer. *(Severity corrected by the skeptic.)*
- **low** · stale-comment · `tests/input/standard_gamepad_tests.cpp:13` — Says
  dom_events_tests.cpp is compiled into the browser build only, but it is built
  on every platform.
  The comparison is false and misstates how dom_events is built, which is the
  opposite pattern. *Fix:* Drop the comparison: "Compiled into the browser
  build only, beside the file it tests." *(Severity corrected by the skeptic.)*
- **low** · weak-test · `tests/input/standard_gamepad_tests.cpp:142` — "The
  guide button is not read" is never asserted with the guide button pressed.
  Only the button count is varied. No case presses W3C index 16 (or 6/7 as
  digital) and checks that `buttons` stays 0. "every named button has its own
  index" covers indices 0-5 and 8-15 only. A translation that mapped index 16
  onto some engine bit would pass. *Fix:* Add: standard_pad(0) with
  digitalButton[16] = true gives state.buttons == 0.
- **low** · stale-comment · `tests/input/web_vibration_tests.cpp:97` — Comment
  says gamepad_tests.cpp needs Node to have no Gamepad API; vibration_tests.cpp
  is the file that does.
  The comment points a maintainer at the wrong file as the dependent. Someone
  changing the fake-pad lifetime would check the wrong case. *Fix:* Change the
  comment to name vibration_tests.cpp (its GamepadReader/Gamepads cases that
  expect empty slots). *(Severity corrected by the skeptic.)*
- **low** · magic-number · `tests/local_multiplayer/join_state_tests.cpp:152` —
  Test hard-codes the arena's landmark colour copied from the sample source.
  If the sample's palette changes, this test fails with no clue why, or matches
  some other sprite by chance. Arena already exposes `player_colour(i)` for the
  same purpose in draw_tests.cpp, but the landmark colour is an anonymous
  literal in two places. *Fix:* Expose the landmark colour as a named Arena
  constant (for example `Arena::landmark_colour`), use it in arena.cpp, and
  compare against it here. *(Severity corrected by the skeptic.)*
- **low** · naming · `tests/math/ericson_math_tests.cpp:109` —
  Single-capital-letter constants X and Y break the snake_case constant rule.
  CONVENTIONS: constants follow value naming (snake_case), and uppercase is
  reserved for macros. 'X' and 'Y' read as macros and do not say what they are.
  `side` on the next line follows the rule. *Fix:* Rename to
  `origin_x`/`origin_y` (or `left`/`top`).
- **low** · naming · `tests/math/ericson_math_tests.cpp:363` — Legacy
  snake_case 'test_...' case names mixed with sentence names, including a
  doubled 'test_test_'.
  Doctest names are the test's documentation, and the unit has two naming
  schemes. The old names repeat the 'test_' prefix that TEST_CASE already
  implies (doubled in 'test_test_') and say nothing about the property checked.
  quad_tests.cpp:15 cites one of them by name, which keeps the old style in
  place. *Fix:* Rename the legacy cases to sentences that state the property,
  e.g. "test_2D_segment_segment reports the crossing point and parameter".
  Update quad_tests.cpp:15 in the same change.
- **low** · test-hygiene · `tests/math/ericson_math_tests.cpp:565` — Last case
  sits outside the file's TEST_SUITE.
  Every other case in the file is in TEST_SUITE("EricsonMath"). This one is
  appended after the suite's closing brace, so `-ts=EricsonMath` filtering
  skips it and the grouping is inconsistent. The same appended-after-the-fact
  pattern shows in its compressed style. *Fix:* Move the case inside the
  TEST_SUITE block.
- **low** · include-hygiene · `tests/math/matrix3x2f_tests.cpp:7` — Unused
  standard includes in three test files.
  Includes that serve nothing widen each translation unit's dependencies and
  mislead about what the file uses. This was checked by grepping each file for
  every std:: name the header provides. *Fix:* Remove the three includes.
- **low** · convention · `tests/math/shape_intersect_tests.cpp:663` — Stray
  space after the tabs in one test body, and an '00.0f' literal.
  Each body line in test_segment_rectangle_rotated_intersect has tab+tab+space
  indentation, unlike the rest of the file. `00.0f` is a typo-looking literal.
  The whitespace lint misses both. *Fix:* Remove the extra space on lines
  663-675 and write `0.0f` on line 433.
- **low** · stale-comment · `tests/scene/cull_tests.cpp:50` — Harness comment
  says 'a two-texel texture' but the harness builds an 8x8 one.
  The comment contradicts the code below it. It looks copied from
  fanout_tests.cpp's 2x2 harness. The 8x8 size is load-bearing here: the later
  rotated-visual test's '40/8 or 20/8 world units' arithmetic depends on it. A
  reader who trusts the comment gets that arithmetic wrong. *Fix:* Say 'an
  eight-texel-square texture', or drop the size from the comment. *(Severity
  corrected by the skeptic.)*
- **low** · duplication · `tests/scene/cull_tests.cpp:54` — Four per-file
  null-renderer Harness classes repeat the device/table/texture setup that
  tests/render/null_test_harness.h already shares.
  The same seam-ordering rule (resources before renderer, create_device before
  set_resources before content) and the same synthetic atlas/font are restated
  in four places. null_test_harness.h deliberately loads from ./content, so a
  second shared, file-free fixture is what is missing. The duplication drifts:
  canvas sizes, texture sizes and accessor shapes already differ for no stated
  reason. *Fix:* Add one file-free null fixture header (for example
  tests/render/null_synthetic_harness.h) that owns the resources-then-renderer
  members, a white or quad texture made from bytes, and the optional two-glyph
  font. Have cull, fanout, order and text_widget tests compose it, keeping only
  their scene/draw helpers. *(Severity corrected by the skeptic.)*
- **low** · magic-number · `tests/scene/cull_tests.cpp:147` — Hand-typed pi
  literals where labrador::pi and pi_over_2 exist.
  engine/math/scalar.h defines `pi` and `pi_over_2`, and the math tests use
  them. The literals truncate pi, so they differ from the engine's constants in
  the last bits, and they repeat a value the reader must recognise by eye.
  *Fix:* Use `pi` and `pi_over_2` (both files already `using namespace
  labrador`). *(Severity corrected by the skeptic.)*
- **low** · test-hygiene · `tests/scene/fanout_tests.cpp:389` — StateContext
  composition case sits in the fan-out file, has no thread pool, and drops the
  file's style.
  The file header says the file covers the per-view fan-out, but this case
  builds only Scene(nullptr, nullptr) and tests StateContext draw order. Its
  style also differs from every other class in the file: no `this->`, an
  unnamed `float` parameter instead of `/*dt*/`, integer literals passed to
  float constructors, and no comment saying what it pins. *Fix:* Move it beside
  order_tests.cpp's layering contracts (or into a state-composition file).
  Match the file's style and add a one-line statement of the contract it pins.
- **low** · convention · `tests/scene/fanout_tests.cpp:392` — Local SceneState
  departs from the unit's member-access and parameter style.
  Every other test double in the unit writes `this->member_`, comments out
  unused parameter names (`float /*dt*/`) and default-initialises bool members
  (`bool covers_screen_ = true;`). This one class does none of the three, so it
  reads as pasted from elsewhere. *Fix:* Write `this->covers_` and
  `this->scene_`, use `float /*dt*/`, and give `covers_` a default initializer
  (or make it const).
- **low** · stale-comment · `tests/scene/scene_tests.cpp:159` — Pointer to 'the
  closing comment in this file' lands on a comment in the middle of the file.
  Tests appended later left the cross-reference wrong, so a reader scrolling to
  the end of the file finds nothing. *Fix:* Change the pointer to name the
  content ("see the note above the end_tick contact test"), or move the
  line-360 note to the end of the file.
- **low** · include-hygiene · `tests/ui/navigation_tests.cpp:2` — Standard
  header placed before engine headers, and some files have no blank lines
  between include groups.
  CONVENTIONS#files: a test 'opens with <doctest/doctest.h> and then follows
  the same order' (engine, external, standard). navigation_tests puts <vector>
  before the engine headers. The ungrouped files break the blank-line layout
  every sibling uses. *Fix:* Move <vector> below the engine/test includes, and
  add the group-separating blank lines to the three core files.

## H9. Native render test hygiene

- **medium** · duplication · `tests/render/d3d12/presented_slots_probe.cpp:46`
  — Watchdog, fail_fast and loader_function are copy-pasted across the
  D3D12/Vulkan probes and tests.
  The four watchdogs have already drifted: the Vulkan presented-slots copy
  reorders members, starts the thread in the constructor body and uses
  different messages. Each one hard-codes "ten-second" in a string next to a
  literal `seconds(10)`. A fix to one bounded-termination path (the safety net
  that keeps these probes from hanging) will not reach the others. *Fix:*
  Extract a small tests/render/probe_watchdog.h that takes a deadline and a
  [[noreturn]] failure callback, and a tests/render/vulkan/loader_function.h.
  Have each probe include them and keep only its own fail_fast message.
- **medium** · stale-comment · `tests/render/pixel_tests.cpp:44` —
  pixel_tests.cpp preamble and case comments still describe the pre-split
  single file.
  Since SM-49 the text cases are in text_pixel_tests.cpp, and this file holds
  only the lifecycle cases. Here, "below" points at nothing. Two later cases
  also present ("read back and then presented", "submitted and never
  presented"). The file calls end_not_comparable three times (L309, L370,
  L433), not once. Of the cases here, the device-info and vsync ones are the
  two not about a pixel, and the resize case that claims the label checks
  pixels. A reader trusting these sentences gets this file's own scope and
  coverage wrong. *Fix:* Rewrite the preamble to cover what pixel_tests.cpp now
  holds (lifecycle/readback) and move the text-relationship paragraphs to
  text_pixel_tests.cpp. Correct or delete the per-case "only/one/every other
  case here" claims at L159, L306 and L325.
- **low** · stale-comment · `tests/render/CMakeLists.txt:63` — "RenderTests is
  otherwise identical in every configuration" is contradicted nine lines later.
  The WIN32-only source makes RenderTests differ between native and wasm builds
  as well, so the sentence is false. *Fix:* Say "RenderTests is otherwise
  identical in every configuration except for the Windows-only HRESULT test
  below", or move the sentence below both conditionals.
- **low** · stale-comment · `tests/render/CMakeLists.txt:77` — Calls
  RenderPixelTests 'the one entry in ctest' that can fail for reasons outside
  the repo; MinimalSmokeTests and D3D12SynchronizationTests also need a device.
  The uniqueness claim is false and conflicts with ci.yml and the minimal
  sample's CMake comment. *Fix:* Say 'one of the ctest entries that need a
  device (with MinimalSmokeTests)', or drop 'the one'.
- **low** · duplication · `tests/render/d3d12/texture_replacement_tests.cpp:31`
  — Hand-built 2x2 RGBA textures duplicate flat_texture about a dozen times in
  the same executable.
  pixel_test_harness.cpp:32 already defines
  `render_tests::pixel::flat_texture(r, g, b)`, and it is linked into the same
  RenderPixelTests binary. `solid_texture` is a byte-for-byte copy of it. `git
  grep 'texture_level(.*2, 2, 0)' tests/render` finds about 14 more inline or
  helper copies of a white 2x2 texture: presentation_lifecycle_tests.cpp:32
  white_texture, d3d12/presentation_diagnostics_tests.cpp:133,
  d3d12/submit_allocation_tests.cpp:39, d3d12/pending_submission_tests.cpp:204,
  vulkan/cleanup_failure_tests.cpp:180, vulkan/submit_failure_tests.cpp:92,
  vulkan/pending_submission_tests.cpp:334,
  vulkan/submit_allocation_tests.cpp:36, and the four probes. Some set the
  format and some rely on the default, which is already a silent difference
  between copies. *Fix:* Move flat_texture out of the pixel harness into a tiny
  shared test header, for example tests/render/test_textures.h, that every
  RenderPixelTests source and probe can include. Replace the copies with
  flat_texture(255, 255, 255) or flat_texture(r, g, b). *(Severity corrected by
  the skeptic.)*
- **low** · convention · `tests/render/d3d12/wait_observer.h:5` — Test code
  reopens the engine namespace as labrador::d3d12_wait_test.
  CONVENTIONS (Namespaces) allows no nesting under labrador except detail, and
  says code outside the engine never reopens the engine's namespace. Every
  other test helper in this unit lives in render_tests (test_window.h,
  device_fixture.h, submit_loss.h, allocation_scope.h). This is also the only
  test symbol a reader could take for engine API. *Fix:* Move it to
  render_tests::d3d12_wait (or render_tests) and update the macro in
  instrumented_device_resources.cpp L20 and the definitions in
  pending_submission_tests.cpp L233 and presented_slots_probe.cpp L468.
- **low** · ownership · `tests/render/device_fixture.h:21` — DeviceFixture
  releases textures on a live device before the renderer's teardown wait.
  The members are declared so the table outlives the renderer, which is the
  seam's constraint. render_resources.h:112-129 says a shell that releases
  textures ahead of ~Renderer::Impl, the only GPU wait, is what D3D12 reports
  as OBJECT_DELETED_WHILE_STILL_IN_USE. The destructor body runs before
  ~Renderer, so it does exactly that, on a live device.
  render_resources.h:97-102 also says the call is meant for device loss, "the
  one moment nothing is reading". Today it is safe only because each of the
  seven users (gl/d3d12/vulkan texture, replacement, cleanup and submit-failure
  cases) happens to drain or read back before the fixture goes out of scope.
  The fixture shown to backend-specific tests teaches the opposite of
  pixel_test_harness.h and null_test_harness.h. *Fix:* Delete the user-declared
  destructor and let member order handle teardown, as Harness does. If the
  Vulkan cleanup case needs teardown observed in a particular order, do that
  explicitly inside that case after renderer teardown. *(Severity corrected by
  the skeptic.)*
- **low** · test-hygiene · `tests/render/gl/texture_tests.cpp:17` — Test name
  claims "no allocated object" but asserts only that the caller's binding is
  preserved.
  Nothing checks that no GL texture object was created. The assertion is that
  GL_TEXTURE_BINDING_2D still names the sentinel, which the inline comment
  states correctly ("preserving the caller's object"). The name overstates the
  coverage. *Fix:* Rename to "GL texture refusal leaves the caller's binding
  untouched and permits retry", or add an assertion that actually observes
  allocation (for example glIsTexture on the next generated name).
- **low** · error-handling · `tests/render/golden_image.cpp:222` — A failure to
  create the golden or actual directory is discarded and reported as "could not
  write it".
  On a regeneration, or when a mismatch writes golden-actual/, a
  directory-creation failure such as a permission or path problem is thrown
  away. The user then sees only the generic "could not write it", and for the
  actual frame (line 526-530) nothing at all, because that report is also
  ignored. The cause of a failed dump is the one thing the developer needs.
  *Fix:* If create_directories sets the error_code, put error_code.message()
  into `error` and return false. When writing the actual frame fails, append a
  note to the report instead of silently omitting the frame path.
- **low** · stale-comment · `tests/render/golden_image.cpp:342` — Census "the
  set of sixty" no longer matches the golden set.
  tests/render/golden/ holds 64 PNGs (git ls-files tests/render/golden/*.png |
  wc -l). The measurement record that justifies allowed_channel_drift now cites
  a set size that is no longer true, and a census in a comment does not update
  itself. *Fix:* Drop the count ("against the whole set") or date the
  measurement ("against the 60-image set at <commit>").
- **low** · stale-comment · `tests/render/null_tests.cpp:283` — Null tests
  point at a viewport section and filter case in pixel_tests.cpp that moved to
  other files.
  After the SM-49 split, the viewport cases are in view_pixel_tests.cpp and "a
  filter change mid-list applies to what follows it" is in
  sprite_pixel_tests.cpp. pixel_tests.cpp has neither. The whole point of the
  comment is to send the reader to that coverage, and it sends them to the
  wrong file. *Fix:* Name view_pixel_tests.cpp for the viewport cases and
  sprite_pixel_tests.cpp for the filter case.
- **low** · bug · `tests/render/pixel_test_harness.cpp:178` — snapshot,
  ink_bounds and row_has_ink assume a 64x64 frame while at() follows the
  reported size.
  pixel_test_harness.h:103-106 says at() goes through the reported width "so
  that the three which do not [match buffer_side] can be written in the same
  vocabulary as the rest". The three whole-frame helpers (snapshot :175,
  ink_bounds :189, row_has_ink :220) still iterate the constant buffer_side.
  After a resize to 32x32 they would hit at()'s REQUIRE_MESSAGE "outside the
  back buffer" instead of describing the frame. No case does this yet, but the
  header invites it. *Fix:* Iterate over static_cast<int>(buffer_.x) and
  buffer_.y in all three helpers, matching at().
- **low** · stale-comment · `tests/render/pixel_test_harness.cpp:215` —
  ink_bounds comment calls right/bottom exclusive while the code treats them as
  inclusive.
  `right` and `bottom` are the last inked column and row (L206-207 assign
  `right = x`), which is why the code adds 1. The comment says the opposite of
  what the variables hold, and "everywhere else in this file" no longer has a
  referent after the split. *Fix:* Say: "right and bottom are the last inked
  column and row (inclusive); RectangleI is (x, y, width, height), hence the
  +1." *(Severity corrected by the skeptic.)*
- **low** · stale-comment · `tests/render/pixel_test_harness.h:78` — Harness
  comments point to cases "below" that live in other files, and
  golden_image.cpp names the wrong harness file.
  The harness is now its own header/.cpp pair, and the cases are split across
  five group files. No case follows these comments, the decoding harness is
  pixel_test_harness.cpp rather than pixel_tests.cpp, and five files share the
  device rather than one. The SM-49 movement kept these comments byte for byte,
  so they are false now. *Fix:* Replace each "below"/"above"/"this file" with
  the file that owns the case (pixel_tests.cpp, text_pixel_tests.cpp,
  resource_pixel_tests.cpp). In golden_image.cpp say pixel_test_harness.cpp. In
  view_pixel_tests.cpp say "RenderPixelTests has a device on all four...".
  *(Severity corrected by the skeptic.)*
- **low** · include-hygiene · `tests/render/pixel_tests.cpp:1` — Pixel group
  files and null_tests.cpp break CONVENTIONS' test include order.
  CONVENTIONS (Files) says a test "opens with <doctest/doctest.h> and then
  follows the same order". Every other test in this unit does, including
  null_resource_tests.cpp, which shares a harness. null_tests.cpp's engine
  headers are unsorted, unlike its sibling null_resource_tests.cpp. *Fix:* Open
  each pixel group file with <doctest/doctest.h>, then the engine headers, then
  the tests/ harness header, as null_resource_tests.cpp does. Sort
  null_tests.cpp's engine includes alphabetically. *(Severity corrected by the
  skeptic.)*
- **low** · archaeology-comment · `tests/render/pixel_tests.cpp:263` — Case
  comments narrate history and give a stale count.
  "Added a day after this comment" is history, not contract, which AGENTS.md
  rules out. Line 325 says "ONE OF TWO CASES HERE THAT ARE NOT ABOUT A PIXEL",
  but the file now has at least four non-pixel cases: device info, vsync
  policy, and the two resize-restart cases. Line 334 says "ALL THREE BACKENDS
  ANSWERED DIFFERENTLY" in a five-backend tree. These counts and anecdotes go
  stale whenever cases move, as SM-49 just showed. *Fix:* Rewrite as
  present-tense contract: "Harness::read_frame calls back_buffer_size on every
  frame; this case is the only one that asserts on it." Drop the case count and
  the three-backend history, or move them to the review ledger they came from.
- **low** · duplication · `tests/render/presentation_lifecycle_tests.cpp:43` —
  The 4097-sprite presented-frame loop is copied into the D3D12 diagnostics
  test.
  d3d12/presentation_diagnostics_tests.cpp:143-172 (present_frames) repeats the
  same two-view, 4097-sprite, position and colour arithmetic, with the same
  "two full vertex pages and a partial third" rationale, and adds only the slot
  counting. Both files are in RenderPixelTests. If the vertex-page size changes
  from 2048, 4097 has to be updated in two places or one test stops exercising
  the boundary it claims to. *Fix:* Share one
  `record_vertex_page_crossing_frame(renderer, texture, frame)` helper, with
  the 4097 derived from a named constant, and have the D3D12 diagnostics test
  wrap it with its slot bookkeeping.
- **low** · magic-number · `tests/render/presentation_lifecycle_tests.cpp:53` —
  4097 sprites restates the engine's 2048-sprite page size as a literal.
  The comment makes the count depend on the backends' page size
  (sprite_geometry.h `max_run_sprites = 2048`, d3d12/backend.h
  `max_page_sprites = 2048`). If the page size changes, the literal quietly
  stops crossing pages and the case loses its purpose with no build or test
  failure. *Fix:* Name it from the shared constant, for example `constexpr int
  sprites_per_view = 2 * SpriteGeometry::max_run_sprites + 1;` (or whichever
  public spelling sprite_geometry.h exposes), and reference it in the comment.
- **low** · archaeology-comment · `tests/render/renderer_seam_tests.cpp:19` —
  Seam-test preamble and cases tell what the backends used to do.
  The comments describe past backend behaviour and the comment's own history
  ("when this was written"), which CONVENTIONS forbids. Readers may also take
  them as current behaviour. *Fix:* State the rule each case holds in the
  present tense (the seam requires X on every backend) and delete the
  past-behaviour clauses. The TEST-GAP.md citations already point at the
  history.
- **low** · stale-comment · `tests/render/resolution_manager_tests.cpp:93` —
  The coercion test says the Vector2I overload picks the nearest preset, but it
  only matches exactly.
  The comment states a behaviour the code does not have, and the test exists to
  protect it from being 'unified by accident'. *Fix:* Reword it to 'exact
  preset match, otherwise 1280x720'. Also consider whether a test-only coercing
  overload should remain public at all.
- **low** · magic-number · `tests/render/sprite_geometry_tests.cpp:178` —
  Hand-typed pi literals at differing precision instead of labrador::pi /
  pi_over_2.
  engine/math/scalar.h L10-11 defines `constexpr float pi` and `pi_over_2`. At
  least 15 sites across six files restate them, and one
  (sprite_pixel_tests.cpp) uses a different digit count from the rest. The
  tests then assert rotation against a constant the engine does not use. *Fix:*
  Include engine/math/scalar.h and write `const float quarter_turn =
  labrador::pi_over_2;` / `labrador::pi` throughout the unit.
- **low** · stale-comment · `tests/render/text_pixel_tests.cpp:38` — 'Both
  clients' and 'a second backend' wording in render tests and geometry predates
  the split and the five backends.
  Since the split, this repository has one external client (ColourWars) and six
  samples, and all five backends exist. 'Both clients', 'both samples' and
  'before a second backend' name a set the reader cannot identify and state
  facts that no longer hold. *Fix:* Replace with 'every sample and client',
  'LineSweeper and ColourWars' or 'every backend', as each claim means. Recast
  'before a second backend' as the current GL/WebGL2 BC2 constraint.
- **low** · stale-comment · `tests/render/throw_if_failed_tests.cpp:22` —
  Comments claim the contract test runs in every configuration; CMake builds it
  only on WIN32, and the D3D11 count is wrong.
  The file is gated on WIN32, so the wasm configurations never compile it.
  Among native configurations, four build no D3D11 file (d3d12, gl, vulkan,
  null; engine/CMakeLists.txt L146-149 lists d3d11/ sources for one backend
  only), not two. engine/render/d3d12/*.cpp also includes throw_if_failed.h, so
  the static_assert message names one backend where there are two. Because this
  test calls itself a CONTRACT test, these are claims about its own reach.
  *Fix:* Say the file compiles in every Windows configuration (the five native
  presets). Delete or correct the "two where no D3D11 file is built" clause.
  Change the static_assert message to "how the Direct3D backends keep that
  promise". *(Severity corrected by the skeptic.)*
- **low** · naming · `tests/render/throw_if_failed_tests.cpp:34` — PascalCase
  namespace ThrowIfFailedTests, unlike every other file in the unit.
  CONVENTIONS: "Namespaces | one lowercase word" and "PascalCase names a type".
  Every other file in tests/render uses an anonymous namespace or render_tests.
  The same pattern exists in tests/assets, tests/core and tests/input (for
  example `namespace JsonTests`), so it may be worth one sweep, but within this
  unit it is the odd one out. *Fix:* Drop the wrapping namespace (the
  TEST_SUITE already groups the cases) or use an anonymous namespace like the
  sibling files. *(Severity corrected by the skeptic.)*
- **low** · archaeology-comment · `tests/render/view_pixel_tests.cpp:52` —
  Viewport section header is history ("UNTIL NOW", "when this was written").
  This is a changelog in a comment. "UNTIL NOW" and "when this was written" are
  archaeology, and "the harness above" (L53) is also stale after the split.
  Similar history appears in visual_tests.cpp L100-104 ("It reported the
  destination rectangle ... was culled") and throw_if_failed_tests.cpp L25-30
  and L60-62 ("would not have compiled as a catch of the old type"). *Fix:*
  Reduce the section header to what the viewport cases pin and where the
  production caller is covered (tests/scene/fanout_tests.cpp). Do the same for
  the visual_tests.cpp and throw_if_failed_tests.cpp history paragraphs.
- **low** · convention · `tests/render/view_pixel_tests.cpp:484` —
  Scene-overlay case ignores the file's helper, buffer_side and naming
  conventions.
  `h` is an abbreviation outside CONVENTIONS' closed allowlist. The literal 64s
  and 32s restate buffer_side. The unbraced nested loops and integer literals
  for float parameters break the style of every sibling case. "new" is a
  temporal word that will go stale. The file already has
  check_red_and_blue_frame (L22) for this analytic red/blue comparison, and it
  reports the failing pixel's coordinates. The open-coded CHECK gives a bare
  mismatch, and it is the per-pixel assertion pattern pixel_test_harness.cpp
  L162-164 says it avoids. *Fix:* Rename to `harness`, derive sizes from
  buffer_side, and replace the loop with `check_red_and_blue_frame(harness,
  RectangleI(0, 0, buffer_side, buffer_side), RectangleI(16, 16, 32, 32))`
  (blue painted over red). Drop "new" from the comment. *(Severity corrected by
  the skeptic.)*

## H10. Bench harness hygiene

- **medium** · weak-test · `bench/main.cpp:88` — A linear_phases entry with no
  results is skipped silently, so a renamed case drops its assertion.
  The complexity-class assertions match the strings in linear_phases against
  the names passed to bench::run in three other files. Renaming or mistyping
  'Scene::resolve (broad phase)' (or any of the other phases) on either side
  turns that assertion off with no output and keeps ctest green. The skip
  exists only so the two fan-out names pass in non-null builds
  (fanout_bench_absent.cpp says so), yet it applies to every phase. *Fix:*
  Fail, or at least print a 'skipped' line, when a phase has fewer than two
  results. Exempt only the two fan-out names in non-null builds, for example
  with a flag returned by run_fanout_benchmarks or a per-phase 'optional'
  marker.
- **medium** · stale-comment · `bench/scene_bench.cpp:151` — 'render cull'
  stand-in no longer matches Scene::draw_views' cull, but its comment says it
  is the same loop.
  engine/scene/scene.cpp:291-306 now culls both objects_ and collision_objects_
  with object->cull_bounds(1.0f / view.camera.scale).intersects(visible), which
  is a virtual call with a scale argument. The benchmark row named 'render cull
  (one view)' times a different, cheaper loop over half the objects and calls
  it the engine's cull. A regression in cull_bounds (for example a drawable
  whose extent grows with n) would not appear in the one cull row that builds
  under all five backends. *Fix:* Mirror the real loop: iterate both
  scene.objects() and scene.collision_objects() and test cull_bounds(1.0f /
  scale) with a stated scale. Alternatively, rename the row and comment to say
  it is a bounds()-only proxy. Either way, drop the claim that it is 'lifted
  out of' draw_views.
- **low** · build-hygiene · `bench/CMakeLists.txt:119` — Redundant
  add_dependencies on a target already linked.
  target_link_libraries(D3D12DescriptorCountProbe PRIVATE LineSweeperRules
  LabradorEngine ...) already orders LabradorEngine first. Listing it again
  implies a non-link dependency (such as the generated shader headers) that the
  reader then looks for. Only linesweeper_frame_bench_content is needed. *Fix:*
  Drop LabradorEngine from the add_dependencies call.
- **low** · weak-test · `bench/bench.cpp:106` — check_scaling passes silently
  when the small case measured zero.
  A zero median, from clock granularity or a body the optimiser removed, makes
  the complexity assertion vacuously pass with no message. The 'ok ... stays
  linear' line is then printed for a check that compared nothing. *Fix:* Return
  a failure sentence such as 'small case measured 0 ns; cannot judge scaling'
  instead of an empty string, or have main.cpp print it as skipped.
- **low** · maintainability · `bench/d3d12_descriptor_count_main.cpp:134` —
  Token-renaming macros span a whole sample-linked .cpp and every header it
  includes for the first time.
  linesweeper_frame_bench.cpp includes about 14 headers for the first time
  under these macros (scene.h, label.h, resource_factory.h, board_view.h,
  particles.h, top_out_banner.h, world.h and others). A future header that
  mentions the identifier Window (for example a Window* parameter) or
  result_json is silently rewritten in this one TU, which diverges from the
  engine's layout, an ODR violation that may not be diagnosed. The included
  file's namespace-scope 'using namespace labrador; using namespace
  linesweeper;' (lines 37-38) also leaks into the probe's own wmain. The
  renamed workload still reports 'LineSweeperFrameBench refused:' and prints
  the untagged measurement_class on success
  (linesweeper_frame_bench.cpp:508-516), so the census output mislabels itself.
  This is separate from round 1's d3d12_descriptor_count_renderer.cpp macro
  finding: this one covers the workload include in count_main. *Fix:* Give
  linesweeper_frame_bench.cpp a real seam instead. Move FrameBenchmark into a
  header or library with a window factory and a result-labelling hook, and let
  both mains call it. At minimum, include every header
  linesweeper_frame_bench.cpp uses before the #defines, as
  d3d12_descriptor_count_renderer.cpp does. *(Severity corrected by the
  skeptic.)*
- **low** · stale-comment · `bench/d3d12_descriptor_count_renderer.cpp:23` —
  Probe comment says renderer.cpp includes the FXC shader headers;
  sprite_pipeline.cpp does, and the preload list has drifted.
  engine/render/d3d12/renderer.cpp includes no shader header;
  sprite_pipeline.cpp:9-10 includes sprite_pixel_shader.h and
  sprite_vertex_shader.h. The comment's safety argument is therefore about
  nothing, and bench/CMakeLists.txt:114 adds "${CMAKE_BINARY_DIR}/generated" to
  the probe for a reason that does not exist. Line 1 also claims to 'Preload
  renderer.cpp's declaration headers', but the list omits
  engine/render/text_encoding.h (renderer.cpp:8). That header is first included
  inside the macro scope, which is exactly the hazard the preload is meant to
  prevent. Nothing keeps the list in sync with renderer.cpp's includes. *Fix:*
  Correct the comment. Remove the generated include directory from
  D3D12DescriptorCountProbe unless something needs it. Add text_encoding.h to
  the preload, and either assert the preload matches (for example a
  configure-time diff of renderer.cpp's #include lines) or state that it must
  be updated with renderer.cpp. *(Severity corrected by the skeptic.)*
- **low** · magic-number · `bench/d3d12_descriptor_query_probe.cpp:138` — Probe
  repetition count and iteration ceiling are unnamed literals.
  warmup_iterations is a named constant and printed in the header, but the 7
  repetitions, the default of 1000000 and the 100000000 ceiling (lines
  78/91-92) are bare literals and the repetition count is not written to the
  output. Anyone analysing the CSV has to infer how many repetitions it holds.
  *Fix:* Name repetitions, default_iterations and max_iterations as constexpr
  beside warmup_iterations, and print repetitions in the header block.
  *(Severity corrected by the skeptic.)*
- **low** · error-message · `bench/linesweeper_frame_result.cpp:29` — Run-id
  length limit of 96 is an unnamed literal and its rejection gives the wrong
  reason.
  A valid but 97-character --run is refused with a message about character
  classes, and the limit appears nowhere in print_usage or the message. The
  operator cannot tell what to change. *Fix:* Name the limit (constexpr
  std::size_t max_run_length = 96) and report empty, too long and bad
  characters separately.
- **low** · robustness · `bench/main.cpp:117` — Two ratio notes divide without
  the zero guard the neighbouring notes use.
  The rotated-sprite and fan-out notes skip a zero denominator (lines 153 and
  180), but these two do not. The sweep/broad-phase note also pairs rows by
  index on the assumption that both cases recorded the same counts in the same
  order. It checks only that the sizes are equal, so the printed n can belong
  to only one of the two rows. *Fix:* Factor one helper that pairs results by n
  and skips non-positive denominators, and use it for all four notes.
  *(Severity corrected by the skeptic.)*
- **low** · stale-comment · `bench/render_bench.cpp:29` — 'Half of what
  PHILOSOPHY asks for is still unmeasured after this file' is outdated history.
  bench/linesweeper_frame_bench.cpp now measures begin/record+submit/present on
  every real backend, and fanout_bench_null.cpp measures Scene::draw. The
  sentence, and the opening 'which nothing measured until this file' at line 13
  and the 'were ... four phases' account at lines 15-18, narrate history rather
  than state what this file measures. A reader is told the backend half is
  unmeasured when it is not. *Fix:* Keep the 'floor, not a frame time'
  contract. Replace the history with a pointer: backend cost is
  LineSweeperFrameBench's record_submit_ns/present_ns.
- **low** · dead-code · `bench/scene_bench.cpp:88` — BenchCollider's contacts_
  and both classes' age_ are written and never read.
  The comment says contacts are 'Counted rather than acted on', but nothing
  reads the count, so it is a dead store, as are age_ in BenchObject (line
  51/57) and BenchCollider (line 69/98). If they exist to keep the virtual
  calls from being trivial, the comment should say so. Otherwise they are noise
  that suggests a check the bench does not make. *Fix:* Either sum contacts
  into a volatile sink after the resolve case and say why, or make
  update/on_contact empty and drop the members.

## H11. Unused public engine API: remove or test

- **medium** · dead-code · `engine/math/ericson_math.cpp:158` — Public math
  functions with no caller and no test anywhere.
  I ran git grep for `sq_dist_point_AABB`, `.clamp(`, `.clamped(`,
  `Vector2F::angle(` and `set_position_from_top_right(` across Labrador
  (engine, samples, tests, bench, tools, website) and across
  C:/Projects/ColourWars excluding external/. The only hits are the definitions
  in engine/math. None of these is tested, unlike the intersects.h predicates
  that intersects.h:8 keeps "at whatever call count". The static angle(vec)
  also duplicates the member angle(). *Fix:* Delete them, or, if they are meant
  as vocabulary, give each a behavioural test, as the project rule for public
  primitives requires. *(Severity corrected by the skeptic.)*
- **medium** · dead-code · `engine/math/rectanglef.h:73` —
  RectangleF::set_position_from_top_right survived the unused-member removal
  with no caller and no test.
  Removing the bottom-left and bottom-right setters while keeping top-right
  leaves an odd, untested corner of the API that a reader would expect to have
  a purpose. *Fix:* Remove both overloads, and add them to a CHANGES.md
  source-break entry beside the other RectangleF removals. *(Severity corrected
  by the skeptic.)*
- **medium** · dead-code · `engine/math/vector2f.h:57` — Vector2F::clamp,
  Vector2F::clamped and the Vector2F-t lerp overload have no caller and no
  test.
  These are public primitives with no user and no behavioural test, which is
  the same criterion the 2026-10-27 removal applied. clamp() and clamped() also
  duplicate the same min/max arithmetic. *Fix:* Remove clamp, clamped and the
  Vector2F-t lerp overload, with a CHANGES.md source-break entry.
  Alternatively, keep any that a documented consumer need justifies and add a
  test for it. *(Severity corrected by the skeptic.)*
- **medium** · dead-code · `engine/math/vector2f.h:66` —
  Vector2F::to_unit_vector and Vector2F::unit_vector have no caller and no test
  anywhere.
  These are a second normalisation API with a deliberately different
  zero-vector contract that invents a direction. They are used by nothing, and
  normalized() carries a paragraph warning against them. That is the 'no
  caller, no test' class CHANGES.md 2026-10-27 removed, and AGENTS.md requires
  a test for every public primitive. *Fix:* Remove both members and their
  definitions. Cut the 'Note this is deliberately NOT the same contract...'
  paragraph from normalized()'s comment and the 'See normalize()'
  cross-reference. Add a CHANGES.md source-break entry in the 2026-10-27 table
  style. *(Severity corrected by the skeptic.)*
- **medium** · dead-code · `engine/render/camera.h:43` — Three of the four
  calculate_view_rectangle overloads have no callers and repeat the same
  arithmetic.
  This is untested public API. camera.cpp carries the world-to-view transform
  four times, so a fix to one copy can miss the others. The repo's rule is that
  a public primitive ships with behavioural tests. *Fix:* Remove the in-place
  overload and both static overloads, keeping the const member that returns
  RectangleF. Record the removal in CHANGES.md.
- **medium** · dead-code · `engine/render/d3d11/backend.h:79` — By-name texture
  lookup has no caller, and its 'parity' justification is false.
  Nothing calls this public member, and the comment's reason for keeping it
  says something untrue: no other backend has this overload, so removing it is
  what would bring D3D11 into line with the rest. SEAM.md §4 also describes
  this file as 'two lookups', which this dead overload makes look right. *Fix:*
  Delete the declaration at backend.h:79-84 and the definition at
  d3d11/render_resources.cpp:41-45, and check that SEAM.md §4's description of
  render_resources.cpp still holds. *(Severity corrected by the skeptic.)*
- **medium** · dead-code · `engine/render/resolution_manager.h:70` —
  set_resolution(string) and set_resolution(Vector2F) have no callers and
  silently coerce unknown input to 720p.
  This is dead public API with a silent-failure contract: any unrecognised
  string or vector becomes s_1280_720 with no error, the same coercion this
  header warns against for window sizes. It is untested and widens the surface
  the website publishes. *Fix:* Remove both overloads, the three helpers that
  serve only them, and ScreenResolution::max, with a CHANGES.md entry. If a
  string form is wanted for options files, make it throw on an unknown name
  (T6) and test it. *(Severity corrected by the skeptic.)*
- **low** · interface · `engine/math/quad.h:48` — Quad::triangle_0/triangle_1
  and Triangle::calculate_gradient are public but only used internally.
  These are implementation steps exposed as untested public API. triangles()
  and hypotenuse_gradient() are the operations clients and tests actually use.
  *Fix:* Make triangle_0/triangle_1 private, or fold them into triangles().
  Make calculate_gradient protected or private, and adjust the comment at
  triangle.h:62-66 that calls it 'the public one'.
- **low** · test-hygiene · `engine/math/scalar.h:32` — Documented contracts of
  clamp, lerp and Vector2F clamp/clamped/lerp have no tests.
  AGENTS requires behavioural tests for public primitives. The inverted-range
  clamp answer and lerp's unclamped extrapolation are documented edge behaviour
  (also published in code-hygiene-public-headers.md) that nothing pins down.
  *Fix:* Add doctest cases to tests/math for scalar clamp (including an
  inverted range), lerp outside [0,1], Vector2F::clamp/clamped/lerp, and
  sq_dist_point_AABB against closest_pt_point_AABB. *(Severity corrected by the
  skeptic.)*
- **low** · duplication · `engine/math/vector2f.h:60` — Two spellings of angle,
  and the static one has no caller.
  Two forms of the same function widen the interface and invite drift.
  matrix3x2f.h:114-115 cites 'Vector2F::angle()' as a factory/reader pairing,
  which this static overload is not. *Fix:* Remove the static angle(const
  Vector2F&). If it stays, give it a test.
- **low** · interface · `engine/render/colour.h:66` — Colour::brighten is used
  only by a test; clamp_colours is public but only an internal helper.
  brighten is dead public API, and its semantics (adds to RGB, ignores alpha)
  do not respect the premultiplied blend the engine documents. clamp_colours is
  an invariant-restoring helper exposed as API, although no caller can ever see
  an unclamped Colour through the public constructors. *Fix:* Remove brighten
  and its test, and make clamp_colours private. Add a CHANGES.md entry.
  *(Severity corrected by the skeptic.)*
- **low** · dead-code · `engine/render/sprite_frame.h:25` —
  SpriteFrame(position, size, origin) Vector2F constructor has no callers.
  An unused float-to-int rectangle constructor on a public type, whose
  truncation behaviour is unstated and untested. *Fix:* Remove it, with a
  CHANGES.md line.
- **low** · dead-code · `engine/render/sprite_geometry.h:143` —
  sprite_quad_bounds is public API with no non-test caller; the cull uses
  sprite_world_bounds.
  Its long comment (lines 124-142) still says this function is what stops a
  cull from dropping a sprite. It is not on any cull path, so a renderer writer
  reading the published header gets a false picture of which function owns cull
  bounds. It is also a second implementation of the truncated-edge bounds that
  must be kept in step with build_sprite_quad, for no engine user. *Fix:*
  Remove sprite_quad_bounds and port its tests to sprite_world_bounds or to
  build_sprite_quad's corners. Alternatively, make it a file-local test helper
  and move the cull paragraph onto sprite_world_bounds. *(Severity corrected by
  the skeptic.)*
- **low** · interface · `engine/render/sprite_sheet_object.h:59` — Name form of
  set_sprite_sheet has no engine caller and is exactly the 'never on its own'
  misuse its comment forbids.
  An unused protected entry point that, when called alone, leaves the
  subclass's frame handle indexing the wrong sheet. It widens the interface for
  a test only. *Fix:* Remove the name overload and test through the handle form
  or TextureObject::set_frame. Fix line 40 to mention only SheetFrame.
  *(Severity corrected by the skeptic.)*
- **low** · dead-code · `engine/ui/focus.h:159` — FocusGroup::refresh_style is
  public for a use case no consumer has, and it is untested.
  The public contract describes a team-tint use that ColourWars, the one real
  multi-slot consumer, does not have. Nothing tests the repaint as a public
  operation, though AGENTS.md requires a public primitive to ship with
  behavioural tests. *Fix:* Make refresh_style private, or add a behavioural
  test that recolours a widget and calls refresh_style to restore it, if the
  consumer use is real. *(Severity corrected by the skeptic.)*
- **low** · interface · `engine/ui/widget.h:79` — UiContainer::children() is
  non-const, copies the whole list on every call, and has no caller.
  A read-only accessor that cannot be called on a const container is a
  const-correctness hole. It also allocates on every call. Under PHILOSOPHY's
  rule, having no caller is not by itself a reason to remove a public name, but
  the non-const signature does carry a cost. *Fix:* Make it `const
  std::vector<std::pair<std::string, UiObject*>>& children() const` or return a
  std::span. This is a public header, so record the signature change in
  CHANGES.md. *(Severity corrected by the skeptic.)*
- **low** · dead-code · `engine/ui/widget.h:125` — UiTexture::set_height has no
  caller or test, and it is RectangleF::set_height's only caller.
  Two public setters exist only to call each other, and neither is tested. This
  is the 'no caller, no test' class CHANGES.md removed on 2026-10-27. *Fix:*
  Remove UiTexture::set_height. Keep RectangleF::set_height only if a test
  covers it, since set_size and the height field already cover the need.
  *(Severity corrected by the skeptic.)*

## H12. LineSweeper sample and its tests

- **medium** · bug · `samples/linesweeper/main.cpp:41` — Tick-rate comment says
  120 FPS runs at half speed; the game would actually run at double speed.
  At target_fps = 120, tick() runs 120 times a second. Gravity, lock delay and
  auto-repeat would all run twice as fast, not half as fast. This is the
  comment that explains why the value is pinned, and it states the effect
  backwards. 'Only checks it is positive' is also out of date: validate()
  checks timer resolution too. *Fix:* Change both main.cpp and the README to
  say "runs at double speed" (or "runs every ticks-denominated duration at
  twice the intended rate"). Drop or reword "only checks it is positive", for
  example to "validate() checks only that it is a usable step". *(Severity
  corrected by the skeptic.)*
- **medium** · duplication ·
  `samples/linesweeper/presentation/particles.cpp:192` — The spark-size formula
  is copied between update()'s cull extent and draw().
  bounds() and cull_bounds() must contain every quad draw() emits. The shrink
  curve is spelled out twice with unnamed constants, so tuning it in draw()
  alone would make the cull rectangle too small, and sparks at the edge of the
  field would be culled without any error. *Fix:* Add a small helper in the
  anonymous namespace, such as `float spark_side(const Particle&)`, with the
  0.35/0.65 floor and range as named constants, and call it from both places.
- **medium** · duplication ·
  `samples/linesweeper/presentation/particles.cpp:468` — ParticleField
  hand-rolls the SplitMix32 stream that labrador::Random now owns.
  Presentation is allowed to depend on the engine, so this is now a second copy
  of an engine primitive. The rules layer cannot link the engine and must keep
  its own copy, but the particle field has no such restriction. The engine
  comment presents LineSweeper as the motivating client of an API that the
  sample never adopted. *Fix:* Replace `rng_`, next_random(), random_unit() and
  random_signed() with a `labrador::Random random_` member. Use unit(), and
  uniform_float(-1, 1) or `unit() * 2 - 1`. Keep the 'second stream, never
  World::rng' rationale in particles.h. Leave tick.cpp's copy, because the
  rules layer may not include the engine.
- **medium** · stale-comment ·
  `samples/linesweeper/presentation/particles.h:143` — Particle units are
  documented as back-buffer pixels, but sparks live in board space.
  Since the arrangement camera was added, positions, speeds, sizes and gravity
  are board units that the camera scales. They match pixels only at exactly
  1280x720. A reader tuning sizes for a phone would be misled. *Fix:* Say
  "board units" (and board units per second, or per second squared) in
  particles.h:143 and :162 and particles.cpp:17, as board_view.cpp:53-54 does
  for shadow_edge_width.
- **medium** · duplication ·
  `samples/linesweeper/presentation/top_out_banner.cpp:97` — The banner
  computes its panel rectangle and text origin twice, once for drawing and once
  for bounds.
  bounds() and cull_bounds() are built from panel_bounds() and text_bounds_,
  but draw() works the same geometry out again on its own. SM-48 already filed
  a banner-extent defect (SRL-21), so these values need to stay in agreement,
  and a change to one copy would not reach the others. *Fix:* Have draw() call
  this->panel_bounds() for the quad. Store the text origin in update(), or
  derive it from text_bounds_, so draw, bounds and cull all read one value.
- **medium** · stale-comment · `samples/linesweeper/rules/world.h:152` — The
  'fourth assert' that comments and the README point to is actually the third.
  Three files send the reader to the wrong assert, the sizeof check. These
  comments exist to say which line enforces the no-padding and no-float
  invariant. *Fix:* Refer to the assert by name, not by position: "the
  has_unique_object_representations_v assert". Update world.h:152 and :196,
  tables.h:329, README.md:264 and world.cpp:12 to match.
- **medium** · duplication · `samples/linesweeper/web/pacing_fixture.cpp:12` —
  pacing_world() is a verbatim copy of the frame bench's build_workload World.
  The browser and native measurements are both meant to describe the same
  workload (9,600 particles), but two hand-copied fixtures can drift apart
  without either test noticing. pacing_fixture.cpp is pure rules code with no
  Emscripten dependency, but it lives under web/ and is compiled only in the
  browser build, so the bench cannot reuse it. *Fix:* Make pacing_world()
  (rules-only) available to native builds, for example in a small shared
  fixture source both targets compile. Have the bench call it instead of
  filling world_ by hand.
- **medium** · duplication · `samples/linesweeper/web/pacing_state.cpp:39` —
  The pacing fixture copies play_state.cpp's hint strings, positions and asset
  names, and the copy is not listed with the others.
  These are four hand-maintained copies, and the comment that warns maintainers
  names only two. Changing a binding hint would leave the pacing page measuring
  a stale frame. The literal 344/-60/-36 also repeats board_view.cpp's
  hold_origin.x. *Fix:* Move the two hint strings and their board-space
  positions into one presentation header, e.g. presentation/hints.h, and
  include it from play_state.cpp, pacing_state.cpp, the capture tool and the
  bench. At least add pacing_state.cpp to the warning comment.
- **medium** · duplication · `tests/linesweeper/particle_draw_tests.cpp:23` —
  Null-renderer setup copy-pasted three times in one file and again in two
  harnesses.
  This is five copies of the same device, white-texture and font setup across
  three files, and two of the copies are identical Harness classes. Any change
  to the null renderer's setup (create_device arguments, texture_level) has to
  be made five times, and the copies already drift: only the Harness copies
  load the font. *Fix:* Put one Harness in a shared tests/linesweeper header
  for the null-render cases: white texture, optional font,
  draw(DrawList-drawable) and draw(Scene). Use it from board_view_tests,
  top_out_banner_tests and particle_draw_tests.
- **medium** · test-hygiene · `tests/linesweeper/view_tests.cpp:397` — Subcase
  claims a per-cell comparison with a clear but asserts none.
  The name states a policy, that a shattered row out-sparks a cleared row per
  cell, which nothing checks. If shatter tuning dropped below clear tuning,
  this subcase would still pass while claiming to guard against exactly that.
  The neighbouring case at line 273 does make the equivalent lock-versus-clear
  comparison. *Fix:* Add CHECK(field.live() / well_columns > one_row_cleared()
  / well_columns) (move one_row_cleared above this case), or rename the subcase
  to what it asserts: 'a topped-out row shatters every one of its cells'.
- **low** · stale-comment · `samples/linesweeper/CMakeLists.txt:201` — The
  content-copy rationale says "Two samples" but the repo has six.
  The argument rests on a sample count that is no longer true. 'the other
  sample' no longer identifies anything. *Fix:* Rephrase without a count, for
  example 'Each sample keeps its own content; at this handful, independence is
  cheaper than the coupling.'
- **low** · stale-comment · `samples/linesweeper/CMakeLists.txt:219` — The
  build comment sends readers to board_view.cpp for the draw_rect rationale,
  which is not there.
  The cross-reference leads nowhere. The doc-citation check only checks
  document names, so it cannot catch a reference to the wrong file. *Fix:*
  Point the comment at README.md (its section on the white texel / draw_rect)
  instead of board_view.cpp.
- **low** · stale-comment · `samples/linesweeper/README.md:580` — The README
  credits code to the wrong files and miscounts the input tables.
  The design record sends readers to the wrong file for two claims. The
  input-map bullet opens with a count that its own last paragraph contradicts.
  *Fix:* Change 581 to name play_state.cpp's hint labels. Change 644 to
  top_out_banner.cpp. Change 1110 to three tables (keyboard, pad, touch) or
  describe them without a count. *(Severity corrected by the skeptic.)*
- **low** · stale-comment · `samples/linesweeper/presentation/layout.h:20` —
  layout.h says the banner layout belongs to board_view.cpp.
  This is left over from before the banner moved out of BoardView
  (README.md:777 "The banner had to leave the board view"). *Fix:* Drop 'and
  the banner' or say 'and the banner is top_out_banner.cpp's'.
- **low** · stale-comment · `samples/linesweeper/presentation/palette.h:177` —
  The engine's premultiplied-blend contract is explained through DirectXTK
  internals that only one backend uses.
  The rationale names one backend's implementation detail as the reason,
  although four of the five backends do not use DirectXTK SpriteBatch. A
  comment that can cite a named contract should cite it instead of re-deriving
  it (CONVENTIONS, Comments). *Fix:* Replace the DirectXTK paragraph with a
  citation, e.g. "draw_sprite blends premultiplied (engine/render/SEAM.md#5):
  dst = src.rgb + dst.rgb * (1 - a)", and keep the consequence for callers.
  *(Severity corrected by the skeptic.)*
- **low** · duplication · `samples/linesweeper/presentation/particles.cpp:108`
  — row_full() is written twice: in tick.cpp over a World and in particles.cpp
  over a cell array.
  The particle field rebuilds the board as it was at lock to decide which rows
  the rules cleared. If the two definitions of 'full' ever diverge, the sparks
  come from the wrong rows, which is exactly the class of bug the TickResult
  change (G6-11) was made to remove. world.h already hosts pure cell queries
  that presentation may call (in_well, cell_index). *Fix:* Declare one
  `constexpr bool row_full(const std::array<std::uint8_t, well_columns *
  well_rows>& cells, int y)` in world.h beside in_well, and call it from both
  tick.cpp and particles.cpp.
- **low** · archaeology-comment ·
  `samples/linesweeper/presentation/particles.h:16` — Presentation headers
  explain themselves through history and predictions instead of current
  rationale.
  CONVENTIONS forbids history in comments, and SM-48 did not cover these files.
  The 35.4 ns per sprite is a machine-specific measurement embedded in a
  header, and it will go stale without anyone noticing. *Fix:* Rewrite in the
  present tense: keep the rationale (one object for the whole field, no engine
  change needed, the cost is about a fiftieth of a frame) and cite README
  sections for the history and measurements. Remove the 'predicted', 'until
  this file did' and 'before there was a camera' narration. *(Severity
  corrected by the skeptic.)*
- **low** · stale-comment · `samples/linesweeper/presentation/particles.h:225`
  — ParticleField says BoardView needs the resource table to measure text;
  BoardView measures nothing.
  The contrast that justifies ParticleField taking a handle instead of the
  table rests on a false statement about its sibling. *Fix:* Say BoardView
  takes the table to resolve its font and texture by name (and draws text), or
  contrast with TopOutBanner, which does measure text.
- **low** · dead-code · `samples/linesweeper/rules/tick.cpp:197` — try_move()
  returns a bool that no caller reads.
  The unused return value suggests that a blocked move matters to the caller,
  but it does not. That is a small cost in reading the code that teaches the
  rules. *Fix:* Make it `void`, or use the result if a blocked shift is meant
  to affect the shift timer.
- **low** · stale-comment · `samples/linesweeper/rules/world.cpp:5` — world.cpp
  claims to be the rules layer's only translation unit and says tick.cpp is
  still to come.
  This is history left in an implementation comment, and it is now false: there
  are three translation units and tick.cpp has existed for a long time. *Fix:*
  Change it to say that, like tables.cpp and tick.cpp, this file includes
  nothing from engine/, or delete the sentence.
- **low** · archaeology-comment · `samples/linesweeper/rules/world.h:241` —
  Rules and state comments narrate past bugs and the review that found them.
  These describe what the code used to do and how a review found it, which
  belongs in git history (CONVENTIONS, Comments). The present-tense rationale
  (a tick locks and clears in one step; a held key is a press after resume) is
  enough without the narrative. *Fix:* Rewrite as conditions: "Reconstructing
  from shadow(before) is wrong whenever a piece moves on its locking tick" and
  "Without the mask, a key held through RESUME is a press to tick()". Drop the
  review citations and the 'found the leak' sentences.
- **low** · interface · `samples/linesweeper/states/pause_state.cpp:157` — The
  COLOURS row carries a placeholder PauseChoice::resume that is never used.
  The table suggests that activating COLOURS resumes the game, but it actually
  cycles the palette. A reader, or a later edit that drops the index special
  case, would wire up the wrong behaviour. *Fix:* Make the choice optional
  (`std::optional<PauseChoice>`, std::nullopt for COLOURS) and branch on that
  instead of on colours_row_index. *(Severity corrected by the skeptic.)*
- **low** · duplication · `samples/linesweeper/states/pause_state.cpp:216` —
  arrange_screen() is copied identically into PlayState, PauseState and
  PacingState.
  There are three copies of the per-tick layout policy. A change to how a scene
  is framed, such as adding a second view, has to be made three times. *Fix:*
  Add one free function, for example `Arrangement
  arrange_scene(labrador::Scene&, const labrador::Application&)` in
  arrangement.h/.cpp, that returns the arrangement after installing its view,
  and call it from all three states.
- **low** · duplication · `samples/linesweeper/states/play_state.cpp:29` —
  Asset-name constants are copied into five files, and the comment that lists
  the copies names only one.
  Renaming the white texture or the font means editing six files, and the
  comment that lists the copies is out of date. *Fix:* Declare `inline const`
  asset names once in a presentation header (e.g. layout.h or a small assets
  header) and use them everywhere, or at least fix the comment so it does not
  list copies.
- **low** · include-hygiene · `samples/linesweeper/states/play_state.h:15` —
  Include order breaks the sample's own sorted pattern; main.cpp uses std::move
  without <utility>.
  Small inconsistencies with sibling files. The missing <utility> relies on a
  transitive include. *Fix:* Move rules/world.h above the states/ includes in
  play_state.h. Sort main.cpp's standard headers and add <utility>.
- **low** · magic-number · `samples/linesweeper/web/pacing_fixture.cpp:51` —
  The fixture's failure message hard-codes 9,600 instead of using
  pacing_particle_count.
  If the constant changes, the error message will state a different expected
  value from the one actually checked. *Fix:* Build the message with
  std::to_string(pacing_particle_count).
- **low** · error-handling · `samples/linesweeper/web/pacing_observer.cpp:67` —
  pacing_mode() casts an unchecked page integer straight into PacingMode.
  Any value other than 0-3 becomes an unnamed enumerator. main_web.cpp treats
  every value except `disabled` as a request to start the pacing observer, so a
  mistyped mode silently runs the instrument against ordinary play instead of
  failing. The engine's stated contract (T6) is that a broken contract fails
  loudly. *Fix:* Range-check the integer and throw std::runtime_error (which
  main_web reports through Module.onError) for unknown values, or map them
  explicitly with a switch. *(Severity corrected by the skeptic.)*
- **low** · include-hygiene · `samples/linesweeper/web/pacing_state.cpp:33` —
  pacing_state.cpp uses std::ignore and std::uint64_t without their headers.
  Both rely on transitive includes from libc++ that the standard does not
  guarantee. *Fix:* Include <tuple> and <cstdint>.
- **low** · magic-number · `samples/linesweeper/web/test_input.cpp:49` — Web
  test hooks hard-code counts that already have a named source.
  Adding a key to sampled_keys without editing the JavaScript literal would
  reject valid phases. The error text and the warm-up length can drift from the
  constants they describe. *Fix:* Pass sampled_keys.size() into
  linesweeper_test_input_begin, build the message from
  std::to_string(pacing_particle_count), and name the warm-up as a constexpr
  next to PacingState.
- **low** · build-hygiene · `tests/linesweeper/CMakeLists.txt:19` —
  Repository-root include directories duplicate what LineSweeperRules already
  exports PUBLIC.
  Both targets already get the repository root through LineSweeperRules' PUBLIC
  usage requirement, so the explicit blocks are dead configuration. The comment
  explaining them suggests they are needed, which misleads anyone trimming the
  build. *Fix:* Delete both PRIVATE repository-root include blocks and the
  comment, or keep one comment noting that the root comes from
  LineSweeperRules.
- **low** · build-hygiene · `tests/linesweeper/CMakeLists.txt:66` — rapidjson
  include path added unconditionally, but only the WIN32-only sources use it.
  On wasm builds the target gets a third-party include path that none of its
  sources use. That hides which sources actually depend on rapidjson, the
  opposite of what the engine's 'PRIVATE and SYSTEM' rationale is trying to
  achieve. *Fix:* Move the SYSTEM rapidjson include into the existing
  `if(WIN32)` block, next to the frame-result sources that need it.
- **low** · duplication · `tests/linesweeper/board_view_tests.cpp:36` —
  Seven-kind array duplicated and magic 7 used instead of kind_count/size().
  world.h provides linesweeper::kind_count. The literal 7 and 8 and the copied
  array would drift silently if the kinds changed, and the loop bound repeats
  the array's size instead of reading it. *Fix:* Size the array with
  linesweeper::kind_count, loop to kinds.size(), and use kind_count + 1 for
  `drawn`. Optionally share the array through well_fixtures.h.
- **low** · include-hygiene · `tests/linesweeper/frame_result_tests.cpp:1` —
  Uses std::invalid_argument/std::runtime_error without <stdexcept>.
  The file compiles only through transitive includes. Siblings
  frame_statistics_tests.cpp and pacing_fixture_tests.cpp include <stdexcept>
  for the same assertions. *Fix:* Add #include <stdexcept>.
- **low** · weak-test · `tests/linesweeper/pacing_fixture_tests.cpp:25` —
  "benchmark's exact" world is checked against hand-copied literals, not
  against the benchmark.
  The same world is built twice: by pacing_world()
  (samples/linesweeper/web/pacing_fixture.cpp:12-37) and by
  LineSweeperFrameBench::build_workload()
  (bench/linesweeper_frame_bench.cpp:190-213). The test restates the values a
  third time as literals and never compares against the bench. If either
  builder changes, the claimed parity breaks silently, or the test simply goes
  on pinning whatever pacing_world() returns. It also spells the kind count as
  a bare `% 7` instead of kind_count. *Fix:* Have the bench call
  linesweeper::pacing_world() (or a shared helper both include), so parity
  holds by construction. Then reduce this test to the properties that matter
  and use kind_count, or rename it so it no longer claims to match the
  benchmark. *(Severity corrected by the skeptic.)*
- **low** · convention · `tests/linesweeper/particle_draw_tests.cpp:99` —
  std::function wraps local lambdas where the unit elsewhere uses `const auto`;
  `size_t` used unqualified without <cstddef>.
  std::function adds type erasure (and possibly an allocation) only to avoid
  writing `auto`, which the conventions explicitly allow here, and it makes
  this file inconsistent with its siblings. The unqualified `size_t` and the
  missing standard headers depend on transitive includes, against the
  include-what-you-use style the rest of the unit follows. *Fix:* Use `const
  auto draw_at = [&](float fraction)` and `const auto draw = [&]()`, then drop
  <functional>. Write std::size_t and include <cstddef> in
  particle_draw_tests.cpp, and include <stdexcept> in frame_result_tests.cpp.
- **low** · stale-comment · `tests/linesweeper/replay_tests.cpp:71` — Comment
  names the wrong static_assert as the padding guarantee.
  This comment explains why the test's memcmp is well-defined. A reader who
  follows it lands on the size assert, which is not the assert that makes
  memcmp legal. (world.h's own heading, 'THE FOUR ASSERTS, AND THE FOURTH IS
  THE INTERESTING ONE', has the same miscount, outside this unit.) *Fix:* Name
  the trait instead of a position: "...which is what world.h's
  has_unique_object_representations_v assert is for." Fix world.h's heading in
  the same change. *(Severity corrected by the skeptic.)*
- **low** · test-hygiene · `tests/linesweeper/replay_tests.cpp:131` — Test name
  and comment claim more than the assertion proves.
  The case shows only that at least one of seeds 1..4 deals a different first
  piece from seed 0. It shows neither that the seed is the only input that
  changes the deal, nor that seed 1 specifically differs from seed 0, which is
  what the comment says. *Fix:* Rename to what is asserted, e.g. 'a different
  seed can deal a different first piece, and zero is a working seed'. Also
  assert that a seed-0 World deals a piece (current.kind != Kind::none) to back
  the xorshift remark. *(Severity corrected by the skeptic.)*
- **low** · archaeology-comment · `tests/linesweeper/resume_gate_tests.cpp:19`
  — Comments narrate review provenance and backlog IDs instead of the contract.
  CONVENTIONS forbids comments about how the code came to be. 'The leak the
  review found', 'the review's board' and a backlog ticket tag are provenance
  that git log answers. The present-tense policy each test pins is already
  stated beside them. *Fix:* Keep the present-tense policy sentences and drop
  the review/backlog provenance ('the review found', 'the review's board',
  'BDP-22:', 'as it always did'). *(Severity corrected by the skeptic.)*
- **low** · bug · `tests/linesweeper/resume_gate_tests.cpp:47` — Unsigned score
  narrowed into int and compared signed/unsigned.
  World::score is std::uint32_t. The implicit narrowing and the mixed-sign
  comparison compile only because doctest's expression decomposition moves the
  comparison into a system header, where /W4's C4018 and clang's -Wsign-compare
  are not reported. It goes against the explicit-type rule (declare the type
  the value has). *Fix:* const std::uint32_t score_before = world.score;
- **low** · magic-number · `tests/linesweeper/scoring_tests.cpp:90` — Score
  expectations hard-coded where sibling subcases use the tables.
  A retune of tables.h fails this case with bare numbers when the formula is
  right there. The file is also inconsistent: other subcases express
  expectations through line_score/spin_score/mini_spin_score. *Fix:*
  line_score[4], then line_score[4] + line_score[4] * back_to_back_numerator /
  back_to_back_denominator + combo_score. *(Severity corrected by the
  skeptic.)*
- **low** · stale-comment · `tests/linesweeper/touch_controls_tests.cpp:39` —
  Reason given for not constructing TouchControls contradicts the CMake
  comment.
  The test says a device has to fill the font. The CMake file, and
  board_view_tests.cpp's header ('A BOARDVIEW NEEDS A FONT, NOT A DEVICE'), say
  the null backend loads fonts without a device. A reader gets the wrong idea
  of why the object is absent here. *Fix:* Say it is not constructed here
  because measuring needs a loaded font, which only the null-render
  configurations provide (focus_pause_tests.cpp constructs it).
- **low** · duplication · `tests/linesweeper/touch_controls_tests.cpp:65` —
  Local `inside` predicate re-implements RectangleF::contains.
  A helper that duplicates a public engine predicate can drift silently, and
  the unit itself uses both spellings for the same question. (`overlaps`
  differs from RectangleF::intersects on whether touching edges count, so it
  has a reason to exist. It would be clearer with a comment saying that
  strictness is the point.) *Fix:* Replace `inside(a, b)` with `b.contains(a)`.
  Comment `overlaps` to say it deliberately treats shared edges as not
  overlapping, which is why it is not RectangleF::intersects.
- **low** · magic-number · `tests/linesweeper/touch_controls_tests.cpp:162` —
  Literals duplicate arrangement.h constants, and the comment misnames the 620.
  620 is linesweeper::portrait_width (arrangement.h:49), not 'the block': the
  file's own board_block (line 46) is 550 units wide. The same applies to
  1280.0f/720.0f (landscape_width exists) and 640.0f (the centre) throughout
  the file. If arrangement.h changes, these tests fail with numbers that do not
  say which constant they mirror. *Fix:* Use linesweeper::portrait_width,
  landscape_width and the like, or name local constants after them. Reword the
  comment to 'the portrait frame's 620 units (portrait_width)'. *(Severity
  corrected by the skeptic.)*
- **low** · duplication · `tests/linesweeper/view_tests.cpp:61` — View tests
  re-implement the well fixtures under a clashing name.
  Both binaries can include well_fixtures.h: it depends only on rules headers,
  and LineSweeperViewTests links LineSweeperRules. Yet the view tests keep a
  second fill_row with different semantics (no holes, kind 1) under the same
  name. particle_draw_tests.cpp (lines 273, 327, 392), pacing_fixture_tests.cpp
  (295) and board_view_tests.cpp (116) also hand-write the same
  `world.cells[static_cast<size_t>(cell_index(...))] = ...` that set_cell
  exists to replace. *Fix:* Include tests/linesweeper/well_fixtures.h in the
  view-side tests. Use set_cell/fill_row(world, y, {}), add a clear_row there
  if needed, and delete the local copies. *(Severity corrected by the
  skeptic.)*
- **low** · stale-comment · `tests/linesweeper/view_tests.cpp:103` —
  about_to_clear's comment cites assertions the function does not contain.
  The function has no assertions. The 'proof' at line 236-237 is
  REQUIRE(landed.kind == Kind::o), which checks that a piece exists, not where
  shadow() puts it. Only the later REQUIRE(world.lines == 1) shows the row
  completed, so the comment overstates what is verified. *Fix:* Point the
  comment at the case that checks the setup and what it checks (the row clears:
  world.lines == 1). Or make the shadow check real, e.g. REQUIRE(landed.y ==
  ...) and REQUIRE(landed.x == well_columns - 2). *(Severity corrected by the
  skeptic.)*
- **low** · error-handling · `tools/linesweeper_capture/capture_script.cpp:184`
  — A capture line with too many words gets a misleading arity message.
  `capture a.png paused extra` is refused with "'capture' takes 1 argument, not
  3". In fact capture takes one or two arguments (the file and an optional
  `paused`). The script's stated design goal is an error that tells the editor
  exactly what is wrong on that line. *Fix:* For capture, refuse sizes other
  than 2 and 3 with their own message: "'capture' takes a file name and
  optionally paused".
- **low** · stale-comment · `tools/linesweeper_capture/main.cpp:16` — "Same
  objects in the same order" omits PlayState's TouchControls.
  The capture is described as a faithful copy of PlayState's scene, but it
  leaves out one registered object. It happens to draw nothing until a finger
  is seen, and that is exactly the assumption a maintainer needs written down
  when PlayState's scene changes. *Fix:* Say that TouchControls is deliberately
  left out because it draws nothing until touch is seen and the captures are
  desktop frames, both in main.cpp and in the README paragraph.
- **low** · include-hygiene · `tools/linesweeper_capture/main.cpp:46` —
  main.cpp includes <Windows.h> but uses nothing from it.
  Nothing in main.cpp calls a Win32 API. wmain and wchar_t need no header, and
  the only Windows work is WIC, which is in png_file.cpp. Including <Windows.h>
  in the translation unit that also pulls in engine and sample headers brings
  in its macros (min/max, near/far and so on) for no use, against the
  convention that a file includes only what it needs. *Fix:* Remove the
  include. Confirm with an x64-release build of LineSweeperCapture.
- **low** · magic-number · `tools/linesweeper_capture/main.cpp:183` — The 60 Hz
  rate and the 1280x720 size are re-typed as literals next to the constants
  that name them.
  Each value has a named constant a few lines away, yet the printed duration
  and the refusal message re-type the numbers. If either constant changes, the
  output contradicts the behaviour. *Fix:* Add `constexpr int ticks_per_second
  = 60;`, derive tick_seconds from it, and format the messages from
  ticks_per_second, frame_width and frame_height.

## H13. Small samples and components gallery

- **medium** · convention · `samples/collision/web/shell.html:23` — Collision
  and audio pages omit touch-action: none, which the published page contract
  requires.
  Four sibling pages split two against two on a rule the website states as part
  of the page contract. On a touch device, a finger on the collision or audio
  canvas scrolls or zooms the page. Collision's header comment also says the
  page owes 'what LineSweeper's page owes', which includes touch-action: none.
  *Fix:* Add `touch-action: none;` to the canvas rule in both pages, and
  mention it in their header comments as minimal's and local_multiplayer's
  comments do.
- **medium** · false-comment · `samples/components/web/pointer_input.mjs:10` —
  Comment says ComponentsTests checks the gallery's pointer rules without a
  browser, but nothing native tests them.
  The rules for a release off the pressed row, stepping by left or right half
  of a value, Quit-by-release, the 20-unit grab margin, and keys cancelling a
  contact all live in GalleryState::update_pointer (gallery_state.cpp:326-411).
  ComponentsTests only covers PointerCapture, hit_target and
  Slider::drag_to/value_at. Its one GalleryState case
  (gallery_animation_tests.cpp) checks only the animation overlay. These rules
  run only in ComponentsPointerPageTests, which is registered only when
  configure finds a Chromium, so a regression there passes every native
  configuration. *Fix:* Change the comment to say which pieces ComponentsTests
  covers (PointerCapture, hit_target, Slider), or add a native-null
  GalleryState case. That case would feed app.mouse()/app.touch() synthetic
  events, as gallery_animation_tests.cpp already does for the keyboard, and
  assert the status lines for release-off-row, value-half stepping and
  key-cancels-drag.
- **medium** · test-coverage · `samples/local_multiplayer/README.md:116` —
  Three samples' --smoke-test modes, with real assertions, are run by hand
  only; only minimal's is in ctest.
  local_multiplayer's smoke test needs no device under null render ('null
  records the draws without pixels'), and audio's needs no graphics device.
  Both could run in CI the way the null-backend view tests do. As things stand,
  their assertions only catch regressions when someone remembers to run them.
  *Fix:* Register LocalMultiplayerSmokeTests (null render and native) and an
  AudioSmokeTests under null audio, plus CollisionSmokeTests where
  MinimalSmokeTests already runs. Update tests/ctest_inventory.json and the
  READMEs in the same commit.
- **medium** · dead-code · `samples/local_multiplayer/play_state.cpp:50` —
  PlayState's unbound, non-smoke input mode is unreachable; its quit and
  stick-reading code computes values that are always thrown away.
  Nothing ever builds bound_sources_ == false with smoke_test_ == false. In the
  smoke path, `if (!this->smoke_test_ && quit)` ignores the quit and
  `directions = { ... }` overwrites the directions. The 'pad 0 and pad 1 quit'
  branch and the read_directions(keyboard, sticks) call therefore only compute
  values nobody reads. A reader of this teaching sample sees a third input
  policy, fixed to slots 0 and 1, that the README never describes. *Fix:* Make
  the two real modes explicit: a smoke constructor (or a scripted-directions
  flag) and a bound-sources constructor. Drop the default `smoke_test = false`
  argument, and remove the else-quit branch and the unconditional
  read_directions call. The smoke title can stay as the scripted mode's label.
- **medium** · convention · `samples/minimal/states/hello_state.cpp:164` — The
  template re-measures ten labels every frame, against Label's stated contract.
  TouchState::capacity is 10 (engine/input/touch.h:59). So the new-project
  template allocates a string and walks the font for ten labels on every tick,
  even with no finger down. This is the pattern the Label header tells callers
  to avoid. Since minimal is the file people copy, the wasteful pattern spreads
  into new games. The audio sample does the same with a stringstream, every
  tick. *Fix:* Keep the last shown id or empty state per marker and call
  set_text only when it changes. In AudioState, refresh the status only after
  an input changed the board or the loop state changed.
- **low** · naming · `samples/audio/audio_state.h:11` — Three samples, three
  namespace styles; audio_sample is not one lowercase word.
  CONVENTIONS' table says a namespace is 'one lowercase word', and audio_sample
  breaks that. The collision sample alone uses three names for itself (demo,
  Example, Sample). Readers moving between samples meet naming that differs for
  no reason. *Fix:* Choose one pattern, for example
  `audiosample`/`collisionsample`/`multiplayer`, or the plain folder word
  `audio`/`collision`/`multiplayer`. Rename CollisionExample to
  CollisionSampleLibrary or similar so it matches the target.
- **low** · duplication · `samples/audio/smoke_test.cpp:52` — The audio-only
  manifest filter is duplicated between the Windows smoke test and the Web
  Audio test.
  tests.cpp:137-142 says this test takes 'The same steps smoke_test.cpp takes
  on Windows'. Those steps are copied by hand, so the two can drift (they
  already differ: only the smoke test requires `!entry.optional`). *Fix:* Move
  a small `audio_entries(const AssetManifest&)` helper next to SoundBoard,
  which both targets already link, and call it from both.
- **low** · duplication · `samples/audio/source/generate_waves.py:45` — Drone
  re-implements tone()'s sinusoid and full-scale literal inline; fade length is
  a bare literal.
  The full-scale 32767 and the sine expression are repeated rather than shared
  with tone() (line 22-24), and `fade = round(0.012 * SAMPLE_RATE)` at line 19
  is an unnamed 12 ms constant. The README's amplitude claim depends on these
  values, but they are scattered across three expressions. *Fix:* Add
  FULL_SCALE = 32767 and FADE_SECONDS = 0.012 constants, plus a
  `sine(frequency, index)` helper that both tone() and the drone use.
- **low** · error-handling · `samples/audio/source/verify_content.py:21` —
  Content verifier relies on assert statements, so running it under python -O
  checks nothing and still prints success.
  Python strips assert statements under -O or PYTHONOPTIMIZE. The README
  presents this script as the check that 'checks format, durations, names,
  signal levels and every payload byte'. In an optimized interpreter every
  check vanishes and the script still prints 'source PCM matches named XWB
  entry' and exits 0, so the failure is silently lost. *Fix:* Replace the
  asserts with a small `require(condition, message)` helper that raises an
  exception (or calls sys.exit with the message). Then the checks run whatever
  the interpreter flags. *(Severity corrected by the skeptic.)*
- **low** · duplication · `samples/audio/source/verify_content.py:21` — XWB
  layout knowledge is hand-copied between the verifier and the ADPCM fixture
  packer.
  The signature, the two versions, the bank-data struct format, the entry and
  name sizes and the segment order are each written out separately in both
  scripts, as unnamed literals. One describes what xwbtool writes and the other
  imitates it. Nothing ties them together, so a correction to one (for example,
  the seek-table segment) cannot reach the other, and a reader has to re-derive
  the layout twice. *Fix:* Name the constants once, for example in a small
  xwb_layout.py next to one script that the other imports by path, or at least
  as named module constants in both with a comment cross-referencing the twin.
  Have make_adpcm_bank optionally run the verifier's structural checks on its
  output. *(Severity corrected by the skeptic.)*
- **low** · duplication · `samples/audio/source/verify_content.py:32` — Wave
  count hard-coded as 3 beside a check that derives it from tones.json.
  The JSON definition is already the source of truth for the wave list (line 34
  compares it with count, and line 40 iterates it). The literal 3 duplicates
  it, so adding a wave to tones.json and the bank breaks the verifier for no
  reason. *Fix:* Drop `count` from the tuple check at line 32 and keep the
  JSON-derived comparison at line 34.
- **low** · weak-test · `samples/audio/source/verify_content.py:35` — Verifier
  says it accepts only `-f -nc` output but never checks the seek-table segment
  or the trailing segment layout.
  The docstring says the check 'deliberately accepts only the -f -nc output
  used by this sample'. However, segments[2] (seek tables) is never checked to
  be empty, and the segment offsets are never checked to be ordered or
  non-overlapping. A bank with xWMA-style seek data or overlapping segments
  passes the structural checks, so the claimed narrowness is weaker than
  stated. *Fix:* Assert `segments[2][1] == 0` and that the segment offsets
  increase and do not overlap, or soften the docstring. *(Severity corrected by
  the skeptic.)*
- **low** · weak-test · `samples/audio/source/verify_content.py:59` — Peak
  bound allows 0.37 full scale, but the README promises below 0.3.
  samples/audio/README.md:116-117 states 'Their peak amplitude is below 0.3
  full scale' (that is, below 9830). The verifier is the only check on the
  shipped WAVs and accepts up to 12000 (about 0.366). A regenerated WAV that
  breaks the documented property would pass. The bound is also an unexplained
  literal. *Fix:* Use a named bound derived from the documented figure, for
  example `PEAK_LIMIT = round(0.3 * 32767)` with `< PEAK_LIMIT`, or change the
  README to state what is actually verified.
- **low** · convention · `samples/audio/tests.cpp:13` — Audio test file is not
  named for its subject, and its conditional engine includes come after
  standard headers.
  CONVENTIONS says 'Tests are named for their subject: vector2_tests.cpp' and
  fixes the include order as engine, then external, then standard. Every
  sibling test (motion_tests.cpp, arena_tests.cpp) follows both rules. *Fix:*
  Rename the file to sound_board_tests.cpp (and update CMakeLists and any doc
  that cites it). Put the conditional engine includes in the engine block,
  before the standard headers.
- **low** · magic-number · `samples/collision/arena.cpp:54` — The player's
  start position is spelt twice in Arena.
  If one changes without the other, reset() puts the player somewhere other
  than where it started. arena_tests.cpp:106 and :130 depend on the value too.
  *Fix:* Name a `player_start` constant in arena.cpp and use it in both places.
- **low** · convention · `samples/collision/collision_state.cpp:44` — The
  collision sample reads only pad slot 0, contrary to the template's one-player
  pad policy.
  The template explains why a single-player game must accept every slot. This
  sibling one-player sample ignores that, so a lone pad that lands in slot 1
  cannot move or quit. The README documents 'player one's left stick', but the
  behaviour contradicts the reason the template gives. *Fix:* Sum sticks and OR
  buttons over all Gamepads::max_count slots, as minimal does (or reuse a
  combine_pads-style helper).
- **low** · magic-number · `samples/collision/collision_state.cpp:54` — Unnamed
  stick deadzone 0.2f repeated across samples.
  The minimal template names this tuning value (`constexpr float stick_deadzone
  = 0.2f;` with its rationale, hello_state.cpp:24-28, citing CONVENTIONS'
  Constants). The siblings inline it, twice within one file in controls.cpp.
  *Fix:* Add a named `stick_deadzone` constant in each sample, beside the code
  that applies it.
- **low** · magic-number · `samples/collision/collision_state.cpp:57` — Smoke
  route tick counts and the expected end position are bare numbers repeated
  across three files.
  The verified end point (968, 420) only holds if main.cpp runs exactly 315
  ticks and the segment boundaries stay 120/180/280. These three facts sit in
  separate files, and only README prose ('315 fixed steps') links them.
  Changing any one makes the smoke test throw with no hint of which number
  moved. *Fix:* Declare the route once as named constants or a small table
  (segment directions and lengths, total ticks, expected end), for example in
  collision_state.h. Use it in update(), verify_smoke_test() and main.cpp's
  loop and capture tick. Use a named step_time instead of 1.0f / 60.0f.
  *(Severity corrected by the skeptic.)*
- **low** · duplication · `samples/collision/main.cpp:23` — Four hand-rolled,
  differently-behaved `--smoke-test [--capture]` parsers and a copied capture
  loop across samples.
  The same command-line contract (used by ctest entries and READMEs) behaves
  differently per sample. For example, a quoted path with a space works through
  argv in local_multiplayer, but collision only strips quotes that wrap the
  whole remaining string. A reader learning from the samples sees four idioms
  for one job. *Fix:* Pick one parser (the CommandLineToArgvW form is the
  correct one) and one capture helper. Put them in a small shared sample
  helper, as samples already share minimal's manifest and fonts, or at least
  make the four copies identical.
- **low** · duplication · `samples/collision/main.cpp:74` — Collision and
  local_multiplayer hand-drive frames with a smoke flag in the state,
  contradicting minimal's stated pattern.
  The same begin/draw/submit/read-back/end loop is copied twice. Its tick
  counts and capture frames are bare literals, and its dt of 1.0f/60.0f is not
  tied to target_fps. The loop skips Application::run, so the smoke test checks
  less of the shell than minimal's does. Passing `smoke_test` where the value
  is always false suggests a path that does not exist. *Fix:* Either follow
  minimal's wrapper-state approach, or move the frame loop into one helper with
  named tick and capture constants. Pass `false` explicitly at collision
  main.cpp:95, or drop the parameter once the flag leaves the state. *(Severity
  corrected by the skeptic.)*
- **low** · other · `samples/collision/main.cpp:95` — The interactive path
  passes smoke_test, which is always false there.
  The variable can only be false on this line. Passing it suggests that the
  interactive run can follow the scripted route, which it never does. *Fix:*
  Pass `false` explicitly, or give CollisionState a default/named factory for
  interactive play.
- **low** · error-handling · `samples/collision/main.cpp:100` — Startup and
  usage failures are invisible when any argument is given to the WIN32 samples.
  All three are `WIN32` subsystem executables (see each CMakeLists
  `add_executable(... WIN32 ...)`). Run from a console without redirection,
  they have no stderr. A mistyped flag (`CollisionSample.exe --smoketest`) or a
  failure during a `--smoke-test` run that nobody redirected exits 1 with no
  output at all. The usage message thrown at collision/main.cpp:59,
  audio/main.cpp:35 and local_multiplayer/main.cpp:62 is effectively never
  seen. The minimal template uses a different rule (`if (!smoke_test)`), so the
  four samples disagree on when a failure is shown. *Fix:* Gate the message box
  on 'not a smoke test' as minimal does, so a usage error is always shown.
  Alternatively, attach to the parent console
  (AttachConsole(ATTACH_PARENT_PROCESS)) before writing stderr. Use the same
  rule in all four samples. *(Severity corrected by the skeptic.)*
- **low** · convention · `samples/components/animation_preview_tests.cpp:1` —
  Four test files open with the subject header instead of <doctest/doctest.h>.
  CONVENTIONS (Files) says: "A test has no header of its own, so it opens with
  <doctest/doctest.h> and then follows the same order." Half of this folder's
  tests follow that rule and half do not, and a reader copying a test into a
  game gets whichever style they happened to pick. *Fix:* Move `#include
  <doctest/doctest.h>` to the first line of the four files, followed by the
  sample headers, then engine, external and standard headers, as the other four
  tests do.
- **low** · magic-number · `samples/components/gallery_state.cpp:107` — Most of
  the gallery's layout is bare literals sitting next to the named layout
  constants.
  The file names label_x/value_x/row_spacing as "manual layout", but the panel
  rectangle, the heading and help lines, the preview label positions (which
  must match loop/once_preview_bounds at :61-62) and the status row 6 are raw
  numbers. The panel.dds frame facts (16 texels, 6-texel corners) are repeated
  from README.md:174. Moving one element means hunting for related numbers
  elsewhere in the file. *Fix:* Add named constants for the panel source and
  border (next to the texture they describe), the panel rectangle, the header
  line y values, and the preview column x. Derive the preview labels from the
  preview bounds, and name the status row.
- **low** · false-comment · `samples/components/gallery_state.cpp:128` — The
  reason given for building the status text before the rows is wrong.
  The row callbacks only run from adjust()/drag_to() during update(), never
  while the rows are being built. Slider's constructor fires nothing, and the
  window_mode_->set_index at line 158 is silent by contract. The callbacks read
  this->status_ when they run, so the order in which init() builds things does
  not matter to them. A maintainer could be misled into preserving or debugging
  an ordering that has no effect. *Fix:* Delete the comment, or replace it with
  the real constraint if there is one (for example, draw order in the scene).
- **low** · magic-number · `samples/components/gallery_state.cpp:154` — "Index
  1 means full screen" is written three times, tied to the order of the
  window_modes list.
  The mapping between window_modes and fullscreen is repeated as bare literals
  in the callback, in init and in every update. Reordering or extending
  window_modes silently breaks all three places. The website's
  settings-and-saves.mdx excerpts end on this exact text, so it is also quoted.
  *Fix:* Name the index once, for example `constexpr size_t fullscreen_choice =
  1;`, next to window_modes. Use it in all three places and update the website
  excerpt bounds in the same commit.
- **low** · magic-number · `samples/components/gallery_state.cpp:196` —
  Pointer-row bookkeeping uses a hard-coded count, and treats the last row as
  Quit by position.
  The loop bound repeats the array length by hand, and update_pointer decides
  which row is Quit by its position in the list. This gallery is meant to be
  copied and extended, and adding a row after Quit, or a row without updating
  the 5, quietly breaks hit-testing or the Quit action. *Fix:* Loop with
  `std::size(rows)` (or a range-for that tracks the row number). Keep a
  `UiWidget* quit_` member and compare `pressed == this->quit_` instead of
  using pointer_widgets_.back().
- **low** · other · `samples/components/gallery_state.cpp:267` — The window
  row's text is rewritten and re-measured every tick, even when nothing
  changed.
  Every tick copies a wstring and re-walks the glyphs to re-measure text that
  has not changed. The README (lines 156-158) presents this as the pattern to
  copy, so a game with many rows that follow outside state would repeat that
  work on every row, every tick. *Fix:* Only call set_index when the value
  differs: `const size_t wanted = fullscreen ? 1 : 0; if (window_mode_->index()
  != wanted) window_mode_->set_index(wanted);`. Alternatively, make
  OptionRow::set_index return early when index == index_.
- **low** · duplication · `samples/components/gallery_state.cpp:416` — The
  1280x720 layout rectangle and viewport/camera derivation is written twice and
  must stay in sync.
  Pointer hit-testing works only if world_point uses exactly the camera
  refresh_view gave the scene; the comment at line 415 says so. Two literal
  copies of the layout size and the viewport construction can drift silently,
  and the 1280/720 literals are the gallery's one layout constant beside the
  named label_x/row_spacing set. pointer_input.mjs also mirrors these numbers.
  *Fix:* Add `const RectangleF layout_bounds(0.0f, 0.0f, 1280.0f, 720.0f);` to
  the anonymous namespace next to the other layout constants, and a small
  helper that returns the (viewport, camera) pair. Have both refresh_view and
  world_point use it. *(Severity corrected by the skeptic.)*
- **low** · weak-test · `samples/components/gallery_state.cpp:437` — The
  gallery's set_filter(point) does nothing, and the test that claims to cover
  it cannot fail.
  engine/render/sprite_geometry.h:325 resets every view's filter to
  TextureFilter::point at the start of each frame, and nothing in engine/scene
  or engine/ui sets linear. Deleting the gallery's call would change nothing,
  and the test's filter assertion would still pass. The README's claim that the
  hidden-window case checks the point filter is therefore not evidence of
  anything. *Fix:* Either remove the no-op call along with the assertion and
  the README phrase, or make the test meaningful: draw the scene with linear
  filtering first (for example, have the panel or a test overlay set linear) so
  that the overlay's set_filter has something to override, then assert point.
  *(Severity corrected by the skeptic.)*
- **low** · false-comment · `samples/components/gallery_state.h:36` — "Told
  every line the status shows" is false for the starting line.
  The first status line is built directly into the UiText at
  gallery_state.cpp:129-131 (L"Window mode applies; difficulty and volume
  sliders are examples.") and never goes through set_status(). on_status_
  therefore never hears it, and the browser console never shows it. *Fix:*
  Construct the UiText with an empty string and call set_status() with the
  starting text once on_status_ is set. Alternatively, change the comment to
  "every line set after init".
- **low** · weak-test · `samples/components/nine_slice.cpp:131` —
  NineSlice::draw and Bar::draw (the hidden check and skipping empty pieces)
  have no test.
  nine_slice_tests.cpp checks only the geometry and the pieces() accessor.
  Bar::draw (slider.cpp:60-71) is not covered either. Neither component's draw
  is recorded under the null backend, although animation_preview_null_tests.cpp
  already shows how. The hidden() early return, skipping zero-area pieces, and
  the texture, source and colour actually submitted are all unchecked. *Fix:*
  Add a native-null recording case: draw a NineSlice with a zero border and a
  squeezed destination, and assert the recorded sprite count and their source
  and destination. Draw it again after set_hidden(true) and assert nothing was
  recorded. Do the same for a Slider's track and fill.
- **low** · duplication · `samples/components/slider.cpp:147` — adjust() and
  drag_to() repeat the same set-value / show / notify step.
  The header promises that drag_to calls the callback "as adjust() does". Two
  copies of that step can drift apart in a game's edited copy, for example if
  one copy gets a sound or a clamp and the other does not. *Fix:* Add a private
  `bool change_to(int next)` that compares, assigns, calls show_value(),
  notifies and returns, and call it from both adjust() and drag_to().
- **low** · error-handling · `samples/components/slider.cpp:161` — A track with
  zero or negative width is accepted silently and turns dragging into a no-op.
  The constructor checks minimum, maximum, step and value (checked_minimum) but
  never the track. A degenerate track makes value_at always return the current
  value, so drags quietly do nothing, and fill_rectangle produces a zero or
  negative fill width. The value_at contract in slider.h:95-99 does not mention
  this case. T6 would have the constructor throw, naming the slider, as it does
  for the other arguments. *Fix:* Check `track.width > 0` (and height >= 0) in
  the constructor's validation and throw std::invalid_argument naming the
  slider. Then the guard in value_at can become an assertion of the invariant.
- **low** · stale-comment · `samples/components/slider.h:20` — Bar's comment
  says it is "twenty lines", and the comment is broken mid-sentence.
  Bar's declaration is about 22 lines and its definitions in slider.cpp are
  about 45 more, so the count is false and will drift further. The orphaned `A
  leaf` line break looks like a leftover from an edit. *Fix:* Drop the line
  count (for example, "...when the engine's set does not fit, and this one is
  small.") and reflow the paragraph.
- **low** · dead-code · `samples/components/slider.h:107` — Slider::minimum()
  and maximum() are never called, not even by tests.
  A whole-repo grep for `.minimum()`/`.maximum()` on a Slider finds no caller
  in samples, tests or website, and slider_tests.cpp never asserts them. The
  copyable surface carries untested accessors, while OptionRow's matching
  choice_count() is tested. *Fix:* Either remove them (a game that copies the
  slider can add them), or add a CHECK in slider_tests that they return the
  constructor's range.
- **low** · include-hygiene · `samples/components/slider_tests.cpp:37` —
  std::move is used without including <utility>.
  It compiles only through a transitive include. The sibling sources that call
  std::move (option_row.cpp, slider.cpp, gallery_state.cpp,
  animation_preview_null_tests.cpp) all include <utility> explicitly. *Fix:*
  Add `#include <utility>` to the standard-library block.
- **low** · bug · `samples/components/source/README.md:5` — The asset record
  says the diamond is centred, but the generator centres it at texel 7 of a
  16-texel frame.
  The centre of a 16-texel frame is 7.5. Centring at 7 makes the radius-6 frame
  span texels 1..13, which leaves one texel of margin on the left/top and two
  on the right/bottom. Either the prose is false, or the asymmetry is
  unintended and only shows in the reviewed previews. *Fix:* Either say the
  diamond is centred on texel (7, 7), or change the rule to be symmetric.
  Changing the rule changes the checked-in DDS, so regenerate and review it.
- **low** · archaeology-comment · `samples/components/source/README.md:26` —
  "Unicode-source policy remains untouched" narrates a change rather than
  stating a fact.
  "Remains untouched" describes what this change did not do, which is
  review-time history (CONVENTIONS, Comments: the test is the tense). It tells
  someone reading the content's record nothing. *Fix:* Delete the clause and
  keep "All source and JSON text is ASCII."
- **low** · magic-number ·
  `samples/components/source/generate_animation_preview.py:22` — The generator
  repeats the frame size 16 and centre 7 as bare literals beside named
  WIDTH/HEIGHT/RADII.
  WIDTH (64) is really len(RADII) * frame size, and the frame size and frame
  count are repeated in both the pixel and JSON halves. Changing the frame size
  or count means finding every literal, and the two outputs can disagree.
  *Fix:* Define `FRAME = 16` and derive `WIDTH = FRAME * len(RADII)`, `HEIGHT =
  FRAME`, and a centre constant. Use them in atlas_bytes and in sheet_text
  (`len(RADII)` for frame_count).
- **low** · magic-number ·
  `samples/components/source/generate_animation_preview.py:42` — The strip's
  frame_count and frame size are literals instead of being derived from RADII.
  The atlas has one frame per entry in RADII (atlas_bytes loops over
  enumerate(RADII)), but sheet_text() hard-codes 4. Adding a radius would make
  a wider atlas while the JSON still describes four frames. This is separate
  from the known finding about the 16 and 7 literals at line 22. *Fix:* Use
  `len(RADII)` for frame_count and a named FRAME_SIZE for the w/h fields and
  WIDTH (WIDTH = FRAME_SIZE * len(RADII)).
- **low** · magic-number · `samples/components/web/pointer_input.mjs:100` — The
  mirrored layout uses an unexplained `- 1` and mixes constant naming styles.
  The `- 1` turns the text middle (row_y + 10) into the track's middle (row_y +
  track_inset 4 + height 10 / 2 = 9), but nothing says so. The row origin 240
  and spacing 50 are inline while the x values are named. `FRAMES` is a
  function in SCREAMING case beside the lower-case function `row_middle`. A
  layout change in gallery_state.cpp would break this driver in ways that are
  hard to trace. *Fix:* Name the mirrored values (FIRST_ROW_Y, ROW_SPACING,
  TEXT_MIDDLE, TRACK_MIDDLE = 4 + 10 / 2) with a comment linking each to
  gallery_state.cpp, use `track_middle(row)` for the drag, and give the
  frame-wait builder a lower-case name such as `frames_script`. *(Severity
  corrected by the skeptic.)*
- **low** · stale-comment · `samples/components/web/shell.html:44` — The page's
  help line leaves out pointer input and R, both of which the gallery supports.
  The gallery now takes clicks, taps and slider drags (README.md:144-152,
  ComponentsPointerPageTests), and R or gamepad X restarts the previews. The
  page's own instructions describe the keyboard-only version. *Fix:* Mention
  click/tap/drag and R, or point to the in-canvas help line that
  gallery_state.cpp already draws.
- **low** · duplication · `samples/local_multiplayer/join_state.cpp:31` — Font
  asset name repeated as a literal three times in one function.
  The template teaches spelling an asset name once and resolving it once. This
  sample repeats the string four times across two files, so a font change means
  a search-and-replace. *Fix:* Add one `const std::string font_name` in an
  anonymous namespace (or a shared sample header) used by both states.
- **low** · duplication · `samples/local_multiplayer/join_state.cpp:53` —
  Keyboard bindings for the two keyboard players are written out four times.
  Movement keys appear in both controls.cpp's table and join_state's
  held_direction. The join and confirm keys (F and Enter) appear in both
  confirm_pressed and the join-input fill. Changing one binding means editing
  several functions in two files, plus the hint strings at
  join_state.cpp:193-197. This sample exists to show per-game binding policy
  (T1), so it should keep each binding in one place. *Fix:* Make one
  per-InputSource binding table (move/confirm/leave/join keys) in controls.h.
  Derive held_direction, confirm_pressed, leave_pressed, the join inputs and
  read_directions from it. *(Severity corrected by the skeptic.)*
- **low** · magic-number · `samples/local_multiplayer/join_state.cpp:102` —
  JoinInput array indexed with literal 0, 1 and +2 instead of the InputSource
  enumerators.
  join_rules.h defines the order (keyboard_left = 0, keyboard_right,
  pad_0..pad_3) as the simultaneous-join priority. Here the order is re-encoded
  as bare offsets, so reordering the enum silently mis-binds sources. The
  static_assert only checks the count. *Fix:* Index with
  `static_cast<std::size_t>(InputSource::keyboard_left)`, `...keyboard_right`
  and `...pad_0) + slot`. A small `source_index(InputSource)` helper would also
  do.
- **low** · consistency · `samples/local_multiplayer/play_state.cpp:130` —
  Sample failure reporting differs in stderr prefix, message-box title and
  condition, capitalisation, full stops, and doubles a prefix.
  These six files present themselves as parallel examples of one pattern, and a
  reader copying one picks up arbitrary differences. The doubled prefix shows
  the thrower and the catcher each assume the other adds the context. *Fix:*
  Use one form in all six: '<Target>: <message>' on stderr, '<Target> failure'
  as the box title, and the box shown exactly when the command line is empty.
  Use the same 'usage:' capitalisation everywhere, and no full stop in
  exception text, as the engine's messages do. Remove the self-prefix from the
  thrown smoke-failure messages.
- **low** · stale-comment · `samples/minimal/CMakeLists.txt:20` — Comment
  points to rules/motion.h for 'what moved there', which that header does not
  describe.
  The pointer is archaeology (the move is in README's change list for cc56ddd).
  It sends a reader to a file that does not answer it. *Fix:* Say what the
  library holds now, for example '(rules/motion.h says what the rules take and
  why they take plain values)'.
- **low** · stale-comment · `samples/minimal/CMakeLists.txt:97` — Template
  content-copy comment talks about an optional sound bank the template does not
  have.
  The minimal manifest holds only a font. The comment is copied into every new
  project, and its bank clause describes the audio sample's situation, which
  confuses a reader trying to understand the template's own copy step. *Fix:*
  Drop the bank clause, or make it explicitly conditional ('a game that adds an
  optional sound bank ...').
- **low** · duplication · `samples/minimal/main_web.cpp:25` — Every sample's
  browser main re-declares the engine's private Module.onError bridge.
  window.h documents onError as the engine's page contract (window.h:254-274),
  but only failures from inside the loop use the engine's bridge. Startup
  failures depend on each game re-implementing the same JS call correctly, so a
  page-contract change (argument shape, a new callback) must be made in seven
  places, six of them copied into users' projects. *Fix:* Expose one engine
  function for the browser build (for example in engine/app,
  `report_fatal_error(const char*)` or a helper that runs a startup callable
  and reports what it throws) that reuses labrador_tell_page_error. Reduce each
  main_web.cpp to calling it, and update the minimal README's change list.
  *(Severity corrected by the skeptic.)*
- **low** · duplication · `samples/minimal/states/hello_state.cpp:19` — The
  template's font name is spelt in two files despite a comment saying it is
  spelt once.
  The comment's 'spelt once' claim is false for the copy-from template.
  Renaming the font, which the collision and multiplayer READMEs tell adapters
  to do, means finding both copies. *Fix:* Declare the asset name once (for
  example an inline constexpr std::string_view in a small
  samples/minimal/states/assets.h) and use it from both states, or reword the
  comment.
- **low** · test-hygiene · `samples/minimal/tests/motion_tests.cpp:73` — Test
  writes a two-pad state and overwrites it before anything observes it.
  The comment says 'Two pads, then the one in slot 0 unplugged', but
  combine_pads is pure and is called only after the overwrite. The first write
  is never asserted and has no effect, so the 'two pads' half of the scenario
  is not tested. *Fix:* Either assert combine_pads on the two-pad array before
  clearing slot 0 (summed stick (-1, 1), quit requested), or delete the first
  write and reword the comment as 'slot 0 empty, a pad in slot 2'.
- **low** · consistency · `tests/local_multiplayer/CMakeLists.txt:1` — Sample
  tests are built four different ways; local_multiplayer recompiles sample
  sources by relative path.
  Which sample's tests live in tests/ and which in the sample folder, and
  whether sample code reaches its test through a library or as a list of source
  paths, follows no rule. local_multiplayer's two lists of the same .cpp files
  can drift (a file added to the sample but not to the test), and each .cpp is
  compiled twice. *Fix:* Give local_multiplayer a static library like
  CollisionExample and link it from both the sample and the test. Document
  where sample tests live (in the sample folder unless they need the shared
  harness). Rename samples/audio/tests.cpp to sound_board_tests.cpp.

## H14. Engine mechanical cleanup outside render: naming, duplication, includes

- **medium** · ownership · `engine/collision/partitioner.h:8` — Partitioner is
  filed under collision but only splits the scene's per-view draw fan-out; its
  published comment uses ColourWars vocabulary.
  ARCHITECTURE describes collision/ as "contacts, the broad phase that prunes
  the pairs, the narrow phase, manifolds, resolution", and the partitioner is
  none of these. Its consumers are scene and app (searched for "partitioner.h"
  in engine, samples, tests and bench: application.h, scene.cpp, fanout tests,
  local_multiplayer draw tests, fanout bench). The header is published in
  website/src/lib/published.ts under 'collision', and 'match' is one game's
  word in an engine contract that is published word for word. *Fix:* Give the
  class a type comment that states its role (splits an index range across
  worker threads). Replace 'first match' with neutral wording, for example 'the
  first frame that fans out'. Consider moving it next to ThreadPool or scene in
  a commit that also updates ARCHITECTURE and published.ts.
- **low** · duplication · `engine/app/application.cpp:523` — on_resuming copies
  reset_elapsed_time's body.
  The public call and the resume handler mean the same discontinuity. If
  FrameTimings gains another discontinuity-sensitive field, or the step
  position needs resetting, only one copy may get the change, and
  FrameTimings::interval's documented zero-after-resume would no longer hold.
  *Fix:* Have on_resuming call this->reset_elapsed_time().
- **low** · include-hygiene · `engine/app/application.h:28` — application.h
  includes <vector> but uses no std::vector.
  I grepped application.h for 'vector': the only matches are this include and
  engine/math/vector2i.h. No std::vector appears in the public header, which is
  published word for word to the website and included by every game. *Fix:*
  Remove the include. Build the samples and tests afterwards, in case any of
  them relied on the transitive include.
- **low** · duplication · `engine/app/content_root.cpp:3` — Platform guard
  mismatch and separator logic written twice in content_root.cpp.
  Windows.h is included under _WIN32, but the GetModuleFileNameW branch is
  chosen by `#else` of __EMSCRIPTEN__. Any other target fails with an
  undeclared-identifier error, not a clear message, although AGENTS.md treats
  Vulkan on Linux and Android as a port path. The trailing-separator test is
  also written twice, and the two copies already differ on an empty string:
  executable_directory appends a separator to an empty directory and
  resolved_under does not. *Fix:* Use `#elif defined(_WIN32)` with an `#else
  #error "executable_directory has no implementation for this platform"`.
  Factor out a small `with_trailing_separator(std::string)` helper and decide
  once what an empty directory gets. *(Severity corrected by the skeptic.)*
- **low** · duplication · `engine/app/win32/window.cpp:654` — Modifier-side
  bitfield duplicated between Win32 and web, with magic bit layout and
  re-derived scan code.
  The same encoding (shift=bits 0-1, control=2-3, alt=4-5, mask 3u) is written
  twice with unnamed numbers. Web's copy is a tested class (DomModifiers), but
  Win32's is inline in Impl with a bool return whose meaning ("forward this
  event") the name update_modifier does not say. The scan-code extraction (>>16
  & 0xff) and the extended bit (1LL << 24) also repeat
  keyboard_input.cpp:45-46, which SM-50 made the owner of scan-position
  translation. *Fix:* Extract a shared ModifierSides helper (side enum in,
  forward-or-withhold out) with named bit offsets, used by DomModifiers and the
  Win32 Impl. Have keyboard_input.h expose the scan/extended decoding or the
  right-side test so window.cpp stops re-deriving it. *(Severity corrected by
  the skeptic.)*
- **low** · duplication · `engine/assets/json.cpp:78` — read_json_file
  hand-rolls a second whole-file reader with a different missing-file exception
  type.
  engine/core/byte_reader.h already exports read_file_bytes, which every other
  content reader uses. JSON alone reads in 64 KB stack chunks and throws
  runtime_error for a missing file, while textures, fonts and banks throw
  out_of_range for the same fact. The ResourceLoader's optional-asset logic and
  callers therefore have to know which reader a file went through. *Fix:*
  Implement read_json_file as parse over read_file_bytes(path), keeping the
  'read_json_file' message prefix. Either adopt the out_of_range missing-file
  convention and update json.h:16, or state why JSON differs. *(Severity
  corrected by the skeptic.)*
- **low** · convention · `engine/assets/json_object_keys.h:13` —
  `labrador::asset_details` breaks the one-namespace rule, whose only exception
  is `detail`.
  CONVENTIONS, Namespaces: "No deeper nesting, with one exception: `detail`
  wraps internals that headers must expose but users must not touch." This is
  exactly that case, under a second invented name. *Fix:* Rename it to
  `labrador::detail` and update the ten call sites in asset_manifest_loader.cpp
  and sprite_sheet_loader.cpp.
- **low** · other · `engine/assets/json_object_keys.h:30` — Formatting
  outliers: an unbraced if, double blank lines, and engine/standard includes
  not separated.
  Every other if in the unit is braced, and every other file puts one blank
  line between include groups and before the namespace. The whitespace lint
  does not catch any of these. *Fix:* Brace the if, collapse the double blank
  lines, and separate the include groups with one blank line.
- **low** · include-hygiene · `engine/assets/resource_loader.h:4` — Public
  header includes four full headers where pointers need only forward
  declarations.
  Every includer of resource_loader.h gets the registry, font, sprite sheet,
  sound bank and sound group headers transitively, though the class touches
  these types only by pointer. ARCHITECTURE explains that this header once
  carried a backend type to every client; keeping it minimal is how that stays
  fixed. *Fix:* Forward-declare RenderResources, Renderer, AudioResources and
  AudioDevice in the header, and move the includes into resource_loader.cpp.
- **low** · include-hygiene · `engine/audio/audio_resources.h:3` — Include
  groups not separated as CONVENTIONS and every sibling file do.
  CONVENTIONS requires engine, then external, then standard library, and every
  other file in the unit separates the groups with a blank line. These two run
  the groups together, and audio_resources.h lists core before audio unlike
  sound_bank.h. *Fix:* Add blank lines between the engine, external and
  standard groups, and order the engine includes as sound_bank.h does.
- **low** · convention · `engine/audio/sound_bank.cpp:164` — Seven SoundBank
  method definitions run together without blank lines.
  The rest of this file and the whole unit separate function definitions with a
  blank line. This one block of the effect verbs does not, which makes the run
  hard to scan. The whitespace lint does not catch it. *Fix:* Put a blank line
  between each pair of definitions from play_effect through is_effect_looping.
- **low** · duplication · `engine/audio/webaudio/audio_device.cpp:553` — Null
  and Web Audio duplicate the whole name-table and handle bookkeeping.
  The only thing keeping the behaviour (and the error text T6 relies on)
  identical across backends is copy-paste plus comments asserting it. The null
  play_voice divergence above is that drift already happening. *Fix:* Move the
  name-table bank list, the bounds-checked bank()/voice() lookups and the
  three-state voice record into a shared headless helper in engine/audio/ (as
  vibration_output.h does for pads). Both backends would use it, leaving only
  the API calls in each backend. *(Severity corrected by the skeptic.)*
- **low** · naming · `engine/collision/partitioner.h:29` —
  `num_elements`/`num_partitions` use an abbreviation that is not on the
  allowlist.
  CONVENTIONS: "The allowlist of short forms is small and closed: dt, id, src,
  dst, min, max." 'num' is not on it, and these are published parameter names.
  *Fix:* Rename them `element_count` and `partition_count` in partitioner.h and
  .cpp, including the error messages.
- **low** · duplication · `engine/core/name_table.h:27` — NameTable and
  Registry duplicate their name-to-index bookkeeping verbatim.
  The headers explain why the storage differs (by value versus a nullable
  owner), but the strong-exception-guarantee insert and the resolve lookup are
  the same logic in two places. A fix to one, such as the rollback-on-throw
  that both now carry, has to be remembered in the other. *Fix:* Extract a
  small `NameIndex` helper in core that owns the std::map<std::string,int>,
  with a guarded `claim(name, push)` and a `find(name)` returning an optional
  index. Both tables would keep their own element storage and error wording.
  *(Severity corrected by the skeptic.)*
- **low** · naming · `engine/core/registry.h:57` — Lowercase type names:
  Registry::handle and Registry::entry, plus ThreadPool::Impl::task_item.
  CONVENTIONS: 'Types — classes, structs, enums, aliases' are PascalCase. The
  sibling private struct StateContext::Frame/PendingOp follows the rule. The
  public `handle` alias is used by engine/audio/sound_bank.h:71, tests and
  website/src/content/docs/docs/guides/assets.mdx:205, so renaming it is a
  source break. The private `entry` and `task_item` cost nothing to fix. *Fix:*
  Rename `entry` to `Entry` and `task_item` to `TaskItem`. For `handle`, either
  document it as a deliberate exception (a PascalCase `Handle` would shadow the
  class template) or add a PascalCase alias such as `HandleType` and deprecate
  the old one through CHANGES.md.
- **low** · naming · `engine/core/thread_pool.h:56` —
  ThreadPool::min_num_threads/max_num_threads use an off-allowlist abbreviation
  and disagree with ApplicationOptions::min_threads/max_threads.
  CONVENTIONS keeps the short-form allowlist closed (dt, id, src, dst, min,
  max), and 'num' is not on it. The same quantity is spelled two ways one call
  apart: application.cpp:208 passes options_.max_threads into the constructor,
  and Scene reads it back as max_num_threads(). *Fix:* Rename the accessors to
  min_threads()/max_threads() to match ApplicationOptions, recording it in
  CHANGES.md because thread_pool.h is public. Update scene.cpp, bench and
  thread_pool_tests.
- **low** · magic-number · `engine/input/direction.h:105` — DirectionRepeat's
  default intervals are written twice.
  The members are always set by the constructor, so their initialisers are
  redundant copies of the defaults. Changing one place and not the other makes
  the reader wonder which value applies. *Fix:* Name the defaults once, for
  example `static constexpr float default_first_delay = 0.4f`, and use them for
  the default arguments. Alternatively, drop the member initialisers or set
  them to 0.
- **low** · duplication · `engine/input/gamepad.cpp:9` — The GamepadButton bit
  encoding is written out in three places.
  GamepadState::buttons' layout ('one bit each') belongs to gamepad.h, yet each
  backend re-derives it. A change to the layout would have to be made in three
  files with no compiler help. *Fix:* Expose one inline helper beside
  GamepadState (e.g. `constexpr uint16_t gamepad_button_bit(GamepadButton)`) or
  a `set_down(GamepadButton)` member, and use it in is_down and both backends.
  *(Severity corrected by the skeptic.)*
- **low** · style · `engine/input/gamepads.h:25` — Edit artifacts left
  unreflowed comment paragraphs in public headers.
  Public header comments are published word for word on the website, so a
  visibly half-edited paragraph ships as published reference text. *Fix:*
  Reflow both paragraphs.
- **low** · duplication · `engine/input/keyboard.cpp:35` — Second UTF-8 encoder
  duplicates render/text_encoding.cpp's.
  There are two anonymous-namespace copies of the same four-branch encoder.
  keyboard.h's contract already says it matches render/text_encoding.h's
  replacement rule, so the two are meant to agree, but nothing makes them.
  input may not include render, so the shared owner would be core. *Fix:* Move
  one append_utf8, including the surrogate/out-of-range to U+FFFD substitution,
  into an engine/core helper (e.g. core/utf8.h). Have both keyboard.cpp and
  render/text_encoding.cpp call it, and keep the existing tests for each
  caller.
- **low** · magic-number · `engine/input/keyboard.cpp:377` — Focus loss clears
  key words by literal index.
  keyboard.h:121's static_assert invites extending the array ("A key set past
  128 needs a third"). This clear would then leave the third word set, which
  reintroduces the stuck-key bug the comment above it describes. *Fix:* Clear
  the whole array, e.g. `std::fill(std::begin(this->live_.keys),
  std::end(this->live_.keys), 0)`, or loop over its extent.
- **low** · magic-number · `engine/input/web/gamepad_reader.cpp:107` — Slot
  table initialised with a hand-written count of four instead of max_count.
  If Gamepads::max_count grows, the extra slots are zero-initialised. Zero is a
  valid browser pad index, not no_gamepad, so the reader would claim pad 0
  occupies them. The whole file otherwise follows max_count. *Fix:* Initialise
  from the constant, for example a constructor or helper that calls
  `slots.fill(no_gamepad)`, or a constexpr factory in standard_gamepad.h that
  returns a filled GamepadSlots.
- **low** · naming · `engine/math/ericson_math.cpp:50` — camelCase locals in
  the Ericson ports.
  CONVENTIONS: variables are snake_case. The header's "keeping its names"
  rationale (ericson_math.h:15) covers public routine names, not book-style
  locals. Neighbouring code already uses snake_case (ab_length_squared,
  twice_area). *Fix:* Rename to radius_sum and squared_distance, and dist2 to
  distance_squared. *(Severity corrected by the skeptic.)*
- **low** · convention · `engine/math/ericson_math.cpp:478` — Commented-out
  code and integer returns from a bool function.
  The header (ericson_math.h:25-29) rejects commented-out book code. `return
  1/0` in a bool function is leftover from the book's int-returning C listing.
  *Fix:* Delete the commented-out line (the a4 identity comment above it says
  the same thing) and return true/false.
- **low** · duplication · `engine/math/intersects.cpp:391` —
  quad_point_intersect triangulates although the convex-polygon test applies
  directly.
  The neighbouring quad_segment_intersect (385-388) and
  point_rectangle_rotated_intersect (488) use the one convex-polygon definition
  of inside, which ericson_math.h:220 says the engine should have. Splitting
  into two triangles does twice the work for the same answer. Quad also lacks a
  points() accessor like RectangleRotated::points(), which forces the local
  copy at 385-386. *Fix:* Add `std::span<const Point2F, 4> points() const` (or
  a const std::array&) to Quad and use point_in_convex_polygon(quad.points(),
  point). Use the same accessor in quad_segment_intersect.
- **low** · duplication · `engine/math/intersects.cpp:494` — The 'bounding
  boxes' pre-check runs a full shape-vs-box intersection, not a box test.
  rectangle_rotated_rectangle_intersect does an AABB test, eight containment
  tests and four segment-AABB tests, so the early-out costs nearly as much as
  the full query and the comment misdescribes it.
  triangle_rectangle_rotated_intersect (line 322) does the same thing by
  calling the whole rectangle_triangle_intersect as its pre-filter. *Fix:* Use
  `a.bounding_box().intersects(b.bounding_box())` here, and
  `triangle.bounding_box().intersects(rect_rotated.bounding_box())` at line
  322, so each pre-check is the cheap test its comment describes. *(Severity
  corrected by the skeptic.)*
- **low** · naming · `engine/math/intersects.h:51` — The same rotated-rectangle
  parameter has three spellings across sibling predicates.
  The sibling functions use `rectangle` for RectangleF (line 33) but `rect`
  here, and `rotated_rect` in one predicate but `rect_rotated` in six others.
  `rect` is off the allowlist. These are published declarations. *Fix:* Use
  `rectangle` and `rotated_rectangle` (or `rectangle_rotated`) consistently in
  intersects.{h,cpp}.
- **low** · duplication · `engine/math/quad.cpp:43` — Quad(const
  RectangleRotated&) validates twice and copies through accessors.
  rectangle.quad() already constructs a validated Quad (and throws if it is
  invalid), so the second is_valid() can never fail and the four copies are
  boilerplate. *Fix:* Delegate: `Quad::Quad(const RectangleRotated& rectangle)
  : Quad(rectangle.quad()) {}`.
- **low** · interface · `engine/math/quad.h:14` — Quad promises a
  clockwise-from-top-left order that nothing enforces, and constructor
  parameters are numbered 1-4 while accessors are 0-3.
  Quad::is_valid (quad.cpp:81-120) tests only convexity and accepts either
  winding and any start vertex. ericson_math.h:145 says "nothing in this
  library constrains the order a caller lists vertices in". The header comment
  reads like a contract but is not one. The declaration names point1..point4,
  while the definition (quad.cpp:14-17) uses point0..point3 and the accessors
  are point_0..point_3. *Fix:* State the real contract: any convex order of
  either winding; the RectangleF form produces top-left, top-right,
  bottom-right, bottom-left. Name the parameters point_0..point_3 in both
  files. *(Severity corrected by the skeptic.)*
- **low** · duplication · `engine/math/rectangle_rotated.cpp:160` — The
  orthonormal-axes check is written out four times.
  Validity is the type's invariant. With four copies, a tolerance change, for
  example scaling as edges_valid does, has to be made consistently in four
  places. *Fix:* Add file-local helpers `axes_orthonormal(const Vector2F&,
  const Vector2F&)` and `half_widths_positive(const Vector2F&)`, and have the
  setters and the *_valid members all call them. *(Severity corrected by the
  skeptic.)*
- **low** · naming · `engine/math/rectangle_rotated.h:30` — Axes and half
  extents are exposed as Point2F although they are directions and extents.
  The same struct names its storage Vector2F and its API Point2F. The library
  uses the alias to say 'position', and Matrix3x2F's
  transform_point/transform_vector split depends on that distinction. *Fix:*
  Spell the axis and extent accessors and setters as Vector2F. Keep Point2F for
  center and the corners.
- **low** · naming · `engine/math/rectanglef.h:28` — Off-allowlist
  abbreviations in public parameters (horiz/vert, hw_extents).
  CONVENTIONS keeps a closed list of short forms (dt, id, src, dst, min, max),
  and these names are published on the website reference. `hw_extents` /
  `hw_extents_` also differs from the accessor half_extents(), and
  half_y_width() calls a height a width. *Fix:* Use half_width/half_height and
  half_extents throughout. Rename the private hw_extents_ to half_extents_.
- **low** · naming · `engine/math/shape.h:15` — Shape::AABB_intersects breaks
  the snake_case rule and has a pointer overload.
  CONVENTIONS: functions are snake_case, and acronyms are cased as words.
  Ericson's "keep the book's names" rationale covers ported routines, not this
  engine-authored member. Its one production caller
  (engine/collision/contacts.cpp:24) uses the pointer overload, which
  dereferences without a contract on null. The reference overload just forwards
  to it. *Fix:* Rename to `bounding_boxes_intersect(const Shape&)`, keep the
  reference form only, and update contacts.cpp, with a CHANGES.md entry.
  *(Severity corrected by the skeptic.)*
- **low** · include-hygiene · `engine/math/triangle.cpp:6` — Unused and missing
  includes in triangle.cpp, intersects.cpp and inflate.cpp.
  Own-header-first only proves headers compile alone if each file includes what
  it uses. inflate.cpp gets std::span only through ericson_math.h, and
  triangle.cpp pays for a heavy header it does not use. *Fix:* Remove
  ericson_math.h from triangle.cpp and shape.h from intersects.cpp. Add <span>
  and <cstddef> to inflate.cpp.
- **low** · naming · `engine/math/triangle.cpp:188` — find_hypotenuse returns
  the right-angle vertex and takes a redundant Triangle parameter.
  The name and its comment say it finds the hypotenuse, but it returns a vertex
  index. That confusion already produced the leg-instead-of-hypotenuse bug
  described at lines 150-159. It is also a const member that ignores `this` and
  reads its argument instead. *Fix:* Rename it to right_angle_vertex() and drop
  the parameter (read this->angle_N()), or make it a file-local free function
  taking the Triangle. Delete the 'find the hypotenuse' comment. *(Severity
  corrected by the skeptic.)*
- **low** · naming · `engine/math/vector2f.h:153` — Uppercase parameter names
  on the free vector operators.
  Parameters are snake_case per CONVENTIONS. Uppercase single letters read like
  macros or types. vector2i.h:38-44 and both .cpp files repeat the pattern, and
  every other function in the module uses a/b or other. *Fix:* Rename to a/b
  (and `scale` for the scalar) in vector2f.{h,cpp} and vector2i.{h,cpp}.
- **low** · include-hygiene · `engine/ui/navigation.cpp:101` — navigation.cpp
  uses std::min without including <algorithm>.
  The file compiles only through a transitive include. Under the clang/libc++
  wasm build, or after an unrelated header cleanup, this can break. Engine
  files include their standard headers explicitly elsewhere. *Fix:* Add
  #include <algorithm>.
- **low** · convention · `engine/ui/widget.cpp:10` — widget.cpp is the only
  engine source using namespace-scope `using namespace labrador;` and MSVC
  `#pragma region`.
  CONVENTIONS allows `using namespace labrador;` only in a .cpp outside the
  engine. Every sibling .cpp puts its anonymous namespace inside `namespace
  labrador`. The region pragmas are IDE folding markers no other engine file
  uses, and they are mis-nested against the namespace braces. *Fix:* Move the
  anonymous namespace (with Children) inside `namespace labrador`, drop the
  using-directive and remove the #pragma region/endregion lines.
- **low** · duplication · `engine/ui/widget.cpp:67` — UiContainer caches each
  child's name beside the pointer although UiObject's name is immutable.
  The pair stores a second copy of data the child already owns and can never
  change. Every add_child allocates a string, and the public children() type
  exposes the redundancy. remove_child(name) could compare child->name()
  directly. *Fix:* Store std::vector<UiObject*> and compare against
  child->name() in remove_child(const std::string&). Change children()'s return
  type to match, in the same CHANGES.md entry as the const fix. *(Severity
  corrected by the skeptic.)*
- **low** · include-hygiene · `engine/ui/widget.h:7` — widget.h includes
  camera.h but names no Camera, and uses std::pair without <utility>.
  An unused include in a published header adds coupling for every widget
  consumer and misleads a reader about what widgets depend on. The genuinely
  used <utility> arrives only transitively. *Fix:* Remove `#include
  "engine/render/camera.h"` and add `#include <utility>`.

## H15. Render backend mechanical dedupe and convention

- **medium** · duplication · `engine/render/null/renderer.cpp:283` —
  set_view_count/view() validation is copied verbatim into all five render
  backends, including a history sentence in the runtime message.
  These are the seam's argument contracts, not backend behaviour, yet five
  copies have to stay identical by hand; null/renderer.cpp:294 says "The same
  throw the other four make". The view() message also records what an old
  version did ("used to answer with a fullscreen pane"), which is archaeology
  in a string a client will see. AGENTS.md says to keep history out of the
  code. *Fix:* Move the range/touched checks and messages into a shared
  headless helper in engine/render/ (for example check_view_count(count,
  capacity, current, touched-predicate) and check_view_index). Each backend
  then calls it before its own binding work. Drop the 'used to answer' clause
  from the message.
- **low** · coupling · `engine/render/d3d11/backend.h:237` — Back-buffer format
  is passed as a literal while read_back_buffer hard-codes its BGRA swizzle.
  The format is a constructor argument, but read_back_buffer assumes a 4-byte
  BGRA layout without consulting it. Changing the argument would silently swap
  channels in read-back, and the argument repeats DeviceResources' own default.
  *Fix:* Name one constant, for example `back_buffer_format` in backend.h. Use
  it for construction and either derive the swizzle from it or
  static_assert/check it in read_back_buffer.
- **low** · include-hygiene · `engine/render/d3d11/device_resources.cpp:9` —
  Unused DirectXMath include and using-directive.
  This is left over from the upstream sample. It pulls a large math header into
  the backend and opens a namespace directive for no use, while SEAM.md §3
  states that DirectXTK and DirectX:: types are off the render path. *Fix:*
  Delete `#include <DirectXMath.h>` and `using namespace DirectX;`, and record
  the trim in NOTICE if it tracks changes to the adopted file.
- **low** · duplication · `engine/render/d3d11/device_resources.cpp:464` — The
  adapter-enumeration loop body appears twice in GetHardwareAdapter.
  Two copies of the selection rule (skip software adapters, log, take the
  first) can drift apart, for example by adding a filter to one loop only.
  *Fix:* Extract `bool is_usable_adapter(IDXGIAdapter1*)`, which does the
  GetDesc1, software check and debug log, and call it from both loops.
- **low** · dead-code · `engine/render/d3d11/device_resources.h:59` —
  backBufferCount and minFeatureLevel parameters are never passed, and the 9_x
  feature levels are unreachable.
  The header trims upstream capabilities elsewhere "rather than left for the
  next reader to prove dead a second time", yet it keeps configurable
  parameters and array entries that nothing exercises. The truncation loop and
  its "minFeatureLevel too high" throw exist only to serve them. *Fix:* Turn
  the back-buffer count and the 10_0 floor into fixed constants, drop the 9_x
  entries and the truncation loop, and remove the paragraph that explains the
  dead entries.
- **low** · interface · `engine/render/d3d11/device_resources.h:74` —
  HandleDeviceLost and UpdateColorSpace are public but only called from inside
  the class.
  Public re-entrant device-loss and factory-refresh entry points invite outside
  calls that would skip Renderer::Impl's ordering. The header itself argues
  that unused surface is a promise a backend has to keep (lines 81-86). *Fix:*
  Move both declarations into the private section.
- **low** · dead-code · `engine/render/d3d11/device_resources.h:132` —
  m_d3dFeatureLevel is written but never read.
  This is leftover state from the trim the header describes. The accessor went,
  but the member it read stayed. That conflicts with the note's own rule that
  unread state is a promise to keep, and a reader has to prove it dead again.
  *Fix:* Remove the member and its initializer, and pass nullptr for
  pFeatureLevel in both D3D11CreateDevice calls. If the selected level is worth
  reporting, put it in RenderDeviceInfo instead.
- **low** · includes · `engine/render/d3d11/render_resources.cpp:2` — Unused
  vector2f.h include, and backend.h includes font.h without using it.
  The include lists don't match what each file actually uses: renderer.cpp gets
  Font through backend.h, and null/backend.h relies on getting Vector2F
  transitively. This hides real dependencies from the include-wall reasoning in
  ARCHITECTURE. *Fix:* Drop vector2f.h from both render_resources.cpp files.
  Move font.h into the renderer.cpp files that use Font, and include vector2f.h
  directly in null/backend.h.
- **low** · duplication · `engine/render/d3d11/renderer.cpp:113` —
  Pixels-to-clip transform copied into four backends, though the comment calls
  it fixed seam arithmetic.
  AGENTS.md says quad arithmetic is shared headless code. Here one decision (y
  runs down, scale 2/size, offsets -1/+1) has four hand copies that only the
  pixel goldens keep in line, and a 50-line justification comment lives in only
  one of the copies. *Fix:* Add a small headless function next to
  sprite_geometry.h, for example `std::array<float,4> pixels_to_clip(float
  width, float height)`, with a unit test. Have each backend fill its constant
  buffer or push constant from it, and move the rationale to that function.
  *(Severity corrected by the skeptic.)*
- **low** · other · `engine/render/d3d11/renderer.cpp:426` — Texture size is
  re-queried through COM on every sprite draw; the other four backends cache
  it.
  Every sprite on the hot path (9,600 particles in LineSweeper's top-out) costs
  a GetResource call, a QueryInterface, a GetDesc and two Releases. The stated
  reason, that a cached size would be a second thing to keep right across
  device loss, does not hold. D3D12 keeps the dimensions inside the same table
  entry as the resource, so device loss releases both together. D3D11 is the
  only backend that differs here. *Fix:* Store a small D3d11Texture
  {ComPtr<ID3D11ShaderResourceView>, width, height} in the Registry, as d3d12,
  gl, vulkan and null do. Fill it in add_texture_asset from the description it
  already builds, and read size() in draw_sprite and draw_text. Delete
  Impl::texture_size. *(Severity corrected by the skeptic.)*
- **low** · structure · `engine/render/d3d11/renderer.cpp:712` — "Is there a
  device" is checked three different ways in one backend.
  An empty string in a description struct is used as a lifecycle flag, and it
  diverges from the device pointer during a device loss (OnDeviceLost clears
  device_info first). During that window device_info() throws "requires
  create_device", which is misleading. The null backend uses an explicit bool.
  *Fix:* Add one `bool has_device() const` on Renderer::Impl (based on
  GetD3DDevice) and use it at all sites, or an explicit device_created flag as
  the null backend does.
- **low** · duplication · `engine/render/d3d11/renderer.cpp:806` — Clear colour
  written out twice in one file.
  The clear colour is a term RenderPixelTests pins, and line 842 says so. It is
  spelled out separately in window_size_changed (line 806) and begin_frame
  (line 845). If one copy changes, a resized frame and a normal frame clear to
  different colours. *Fix:* Declare one file-scope `const float clear_colour[4]
  = { 0.0f, 0.0f, 0.0f, 1.0f };` in the anonymous namespace, move line 842's
  comment onto it, and use it in both places.
- **low** · naming · `engine/render/d3d12/backend.h:46` — D3d12Texture breaks
  CONVENTIONS' trademark spelling and the unit's own D3DDeviceNotify.
  One backend spells one trademark two ways, against the documented example.
  *Fix:* Rename it to D3D12Texture (32 uses in engine/render/d3d12,
  resource_factory.h and tests/render/d3d12). If the rename is not worth the
  churn, record the exception.
- **low** · redundancy · `engine/render/d3d12/device_resources.cpp:46` — Fence
  and state arrays initialized three times: value-init, constructor loop, then
  create_device_resources.
  frame_fences_ is zeroed by `{}` and zeroed again in the loop.
  create_device_resources (lines 297-301) then resets it a third time, which is
  the only reset that matters because it runs per device. back_buffer_states_
  is likewise reset in create_window_size_dependent_resources and
  handle_device_lost. The duplicates hide which reset carries the invariant.
  *Fix:* Drop the `frame_fences_[i] = 0` line from the constructor loop, or use
  default member initializers. Keep the per-device reset as the single owner.
- **low** · duplication · `engine/render/d3d12/device_resources.cpp:148` —
  hardware_adapter repeats the same filter-and-probe loop body for two
  enumeration APIs.
  Lines 148-181 and 186-204 share the same GetDesc1, software-skip and
  D3D12CreateDevice probe. When IDXGIFactory6 exists and finds nothing, the
  second loop re-enumerates the same adapters and probes them again. The
  function also returns through a raw IDXGIAdapter1** out parameter
  (device_resources.h:127) with Detach(), where returning a ComPtr would be the
  RAII form. *Fix:* Extract a local 'acceptable(IDXGIAdapter1*)' predicate used
  by both loops. Run the EnumAdapters1 loop only when the IDXGIFactory6 query
  fails. Return ComPtr<IDXGIAdapter1> instead of writing through
  IDXGIAdapter1**. *(Severity corrected by the skeptic.)*
- **low** · convention · `engine/render/d3d12/device_resources.cpp:223` —
  Debug-only code is gated inconsistently on _DEBUG and NDEBUG.
  The debug layer and DXGI info queue follow _DEBUG, while the WARP fallback
  and the D3D12 info queue follow NDEBUG, all in one file. A configuration
  defining neither macro would get a mixed set, and the reader has to check
  that the two always agree. *Fix:* Pick one macro (the AGENTS-documented
  "debug permits WARP fallback" suggests _DEBUG, matching the D3D11 file) and
  one spelling for every debug-only block. *(Severity corrected by the
  skeptic.)*
- **low** · needless-state · `engine/render/d3d12/device_resources.h:36` —
  Configurable back-buffer format that read-back's hard-coded BGRA swizzle
  silently depends on.
  The only construction passes the default value. The parameter, the
  back_buffer_format_ member and the accessor are configurability nobody uses.
  Renderer::read_back_buffer (renderer.cpp:1263-1275) assumes 4-byte BGRA
  without consulting back_buffer_format(), so any other value would quietly
  break the RGBA read-back contract. *Fix:* Make the format a named constant in
  DeviceResources, used by the swap chain, the RTV and the PSO. Otherwise, have
  read_back_buffer assert or branch on back_buffer_format().
- **low** · encapsulation · `engine/render/d3d12/device_resources.h:54` —
  handle_device_lost and move_to_next_frame are public but called only inside
  DeviceResources.
  A repo-wide grep finds callers only at device_resources.cpp:353, 381, 524 and
  530. In particular, nothing in renderer.cpp or tests/render/d3d12/ calls
  them. Leaving them public invites a caller to advance the frame index or
  trigger a full device rebuild outside present and resize, which bypasses the
  fence and back-buffer-state bookkeeping those paths keep consistent. *Fix:*
  Move both declarations to the private section.
- **low** · duplication · `engine/render/d3d12/renderer.cpp:28` —
  selected_device creates a second DXGI factory and re-finds the adapter
  DeviceResources already chose.
  DeviceResources already owns factory_ and picks the adapter in
  hardware_adapter() or the WARP branch, then lets it go. renderer.cpp
  recreates a factory (with CreateDXGIFactory1, not the debug-flagged
  CreateDXGIFactory2) and reruns a LUID lookup plus a WARP special case to
  recover the same adapter. That is two places deciding adapter identity, with
  a fallback path that exists only to reverse-engineer the first. *Fix:* Keep
  the selected IDXGIAdapter1, or its DXGI_ADAPTER_DESC1, in DeviceResources
  when the device is created, expose it through an accessor, and build
  RenderDeviceInfo from that.
- **low** · magic-number · `engine/render/d3d12/renderer.cpp:276` —
  Root-parameter indices and constant count are bare literals coupled to
  sprite_pipeline.cpp.
  Since SM-58 moved the root signature into its own file, the layout lives in
  sprite_pipeline.cpp and is consumed as unnamed 0/1/2/4 in renderer.cpp.
  Reordering parameters in one file silently breaks the other. *Fix:* Declare
  named constants in sprite_pipeline.h, e.g. `static const UINT
  transform_parameter = 0, texture_parameter = 1, sampler_parameter = 2,
  transform_constants = 4;`, and use them in both files and for
  View::transform's size. *(Severity corrected by the skeptic.)*
- **low** · magic-number · `engine/render/d3d12/renderer.cpp:669` — Sampler
  heap size 2 is a literal tied implicitly to sampler()'s point/linear offsets.
  The count, the creation order (point then linear, at 709-714) and the lookup
  offset have to agree, and nothing names that link. *Fix:* Name the slots,
  e.g. `static const int point_sampler_slot = 0, linear_sampler_slot = 1,
  sampler_count = 2;`, and use them in creation and in sampler().
- **low** · duplication · `engine/render/d3d12/renderer.cpp:842` —
  on_device_lost resets view batching fields by hand that View::reset already
  resets.
  View::reset (renderer.cpp:367-376) clears batch, batch_texture, page and
  page_position. on_device_lost repeats four of those assignments inline. A
  field later added to View's per-frame state has to be remembered in both
  places. create_device_dependent_resources already calls view.reset() on the
  recreated views. *Fix:* Call view->reset() here, after the list, allocator
  and page releases. If camera/filter/touched must deliberately survive a loss,
  split View::reset into a batch-state reset that both paths call.
- **low** · duplication · `engine/render/d3d12/texture_factory.cpp:79` —
  format_name(TextureFormat) is duplicated in D3D12, Vulkan and a test, though
  it has no API types and could be shared.
  d3d12's to_dxgi_format comment explains why the DXGI mapping cannot be shared
  across the backend wall, but that reason does not apply to format_name: it
  maps only the engine's own TextureFormat to a string. Adding a format means
  changing three switches, and the D3D11 fix above would add a fourth. *Fix:*
  Declare `const char* format_name(TextureFormat)` beside the enum in
  engine/render/texture_format.h (with a definition in shared render code), and
  use it in both backends and the test.
- **low** · duplication · `engine/render/d3d12/texture_factory.cpp:226` —
  Upload-heap and buffer resource descriptions are hand-written again although
  renderer.cpp has helpers for them.
  renderer.cpp:112-151 already defines heap_properties(), buffer_description()
  and create_buffer(device, heap, bytes, state) in an anonymous namespace. The
  staging buffer at texture_factory.cpp:226-244 is exactly
  create_buffer(device, D3D12_HEAP_TYPE_UPLOAD, total_bytes,
  D3D12_RESOURCE_STATE_GENERIC_READ), and the default heap at line 174 is
  heap_properties(D3D12_HEAP_TYPE_DEFAULT). These are two copies of the same
  backend-private descriptions in one folder. SEAM.md sanctions duplication
  between backends, not within one. *Fix:* Move heap_properties,
  buffer_description and create_buffer into a small backend-private header or
  into backend.h. Use them in both translation units.
- **low** · naming · `engine/render/draw_object.h:22` — Constructor parameter
  spelt `color` in six render classes but `colour` in Label and everywhere
  else.
  Parameter names are published in the reference pages, and the engine's own
  spelling is 'colour'. Mixing the two across a single class hierarchy is the
  inconsistency CONVENTIONS' 'boring everywhere' (T4) argues against. The
  unused includes are the same kind of small drift. *Fix:* Rename the
  parameters to `colour` (a non-breaking change), and drop the two unused
  includes.
- **low** · dead-code · `engine/render/gl/gl_functions.h:71` — Unused constants
  GL_MAJOR_VERSION_, GL_MINOR_VERSION_ and the GLintptr alias.
  The header presents itself as an exact inventory of the GL vocabulary the
  engine uses. These three entries are used by nothing. *Fix:* Delete the two
  constants and the GLintptr alias.
- **low** · dead-code · `engine/render/gl/gl_functions.h:91` — Four loaded GL
  entry points are never called, and their absence fails device creation.
  load_gl_functions throws 'This driver has no <name>' for any missing entry in
  the list. Unused entries therefore narrow driver reach for nothing, and the
  header's claim to be 'The parts of OpenGL this engine uses, and no more'
  (line 10) is false. gl_function_count() also overstates the count in the
  error message. *Fix:* Remove the four entries. Alternatively, give Impl an
  explicit release of program/VAO/buffers/samplers that uses them, such as
  before replacing a context in a second create_device. Do one or the other,
  not both. *(Severity corrected by the skeptic.)*
- **low** · duplication · `engine/render/gl/renderer.cpp:118` — shader_log and
  program_log are the same function twice.
  Two 12-line bodies differ only in the two GL query functions, including the
  duplicated fallback text '(the driver offered no message)'. *Fix:* Write one
  helper parameterised on the two query function pointers, or a template over
  them.
- **low** · convention · `engine/render/gl/renderer.cpp:127` — size_t and
  std::size_t mixed within the file.
  The file includes <cstddef> and uses std::size_t in some places and the
  unqualified global in others; texture_factory.cpp likewise mixes std::size_t
  (l.77) and size_t (l.192). Inconsistent spelling of one type in one unit.
  *Fix:* Use std::size_t throughout the GL folder.
- **low** · magic-number · `engine/render/gl/renderer.cpp:299` — Vertex
  attribute indices 0/1/2 are bare literals in three places that must agree.
  The comment at l.102-103 says 'The attribute indexes ... are what has to
  agree with it', yet the agreement is three separate sets of literals; a
  reorder in one place silently mis-binds attributes. *Fix:* Name them once in
  the anonymous namespace (position_attribute = 0, colour_attribute = 1,
  texcoord_attribute = 2) and use the names at all three sites. *(Severity
  corrected by the skeptic.)*
- **low** · duplication · `engine/render/gl/renderer.cpp:329` — Quad
  index-buffer construction copied verbatim in four backends.
  AGENTS.md says quad arithmetic is shared headless code and backends supply
  only devices and buffers. The winding (0,1,2 / 1,3,2) must agree with
  sprite_geometry's corner order, but four copies each state it on their own.
  The corner count of 4 is also hard-coded in RunRecorder::record
  (sprite_geometry.h `/ 4`). *Fix:* Move the per-sprite constants and a
  `std::vector<unsigned short> quad_indices(int sprites)` helper into
  engine/render/sprite_geometry.h, next to max_run_sprites, and have each
  backend upload its result. *(Severity corrected by the skeptic.)*
- **low** · duplication · `engine/render/gl/renderer.cpp:667` —
  Clear-and-reset-views block duplicated between window_size_changed and
  begin_frame.
  The comment at line 641-666 says the resize path is there to make the frame
  look exactly like a freshly begun one. Keeping two copies invites them to
  drift, for example in clear colour or default viewport. *Fix:* Extract an
  Impl member, e.g. `void restart_frame()`, that does the clear and the view
  reset, and call it from both places. *(Severity corrected by the skeptic.)*
- **low** · naming · `engine/render/gl/texture_factory.cpp:24` — S3TC constants
  neither follow value naming nor restate the API name.
  CONVENTIONS allows SCREAMING only for a constant restating a platform API's
  own name so it can be searched; the registry names are
  GL_COMPRESSED_RGBA_S3TC_DXT1_EXT etc. gl_functions.h's scheme is the full API
  name plus '_' (GL_VERTEX_SHADER_), and web_context.cpp uses the exact
  UNMASKED_VENDOR_WEBGL. This trio matches neither. *Fix:* Spell them
  GL_COMPRESSED_RGBA_S3TC_DXT1_EXT_ etc., following gl_functions.h's suffix
  convention.
- **low** · convention · `engine/render/gl/web_context.cpp:104` — Two
  consecutive anonymous namespaces.
  Back-to-back unnamed namespaces with nothing between them; wgl_context.cpp
  uses two only because the Context definition must sit between them. Here it
  is just noise. *Fix:* Merge them into one anonymous namespace.
- **low** · magic-number · `engine/render/null/renderer.cpp:26` — Corner count
  is a bare 4 in the null backend, but d3d11 names it.
  d3d11/renderer.cpp:85 names the same quantity `vertices_per_sprite`. Here the
  loop bound repeats RecordedSprite::corners[4] by hand, so the two can drift
  apart without notice. *Fix:* Use `std::size(sprite.corners)` (or std::copy_n
  with it) so the bound comes from the array.
- **low** · duplication · `engine/render/null/renderer.cpp:237` — The view
  reset plus full-screen viewport loop is written out twice.
  The resize-restart path is meant to put the views in the state begin_frame
  leaves them in. With two copies, a field added to one loop can be missed in
  the other, which would bring back the divergence the long comment above it
  describes. *Fix:* Extract a private helper such as
  `Renderer::Impl::reopen_views()` and call it from both places.
- **low** · duplication · `engine/render/resolution_manager.cpp:196` — Preset
  table spelt out four times; the ivec ladder hardcodes numbers instead of
  using the one mapping.
  Adding a preset means editing four or five parallel ladders, and the ivec one
  does not even reuse convert_resolution_to_ivec. The -1 sentinels turn an
  out-of-range enum into a negative size with no error. These converters are
  also private const members that read no state. *Fix:* Keep one constexpr
  table of {ScreenResolution, Vector2I, name} and derive every conversion by
  searching it. Make the converters static or file-local, and throw for an enum
  value that is not in the table. *(Severity corrected by the skeptic.)*
- **low** · naming · `engine/render/sprite_sheet.h:29` — Public type aliases
  are snake_case, against CONVENTIONS and their PascalCase sibling.
  CONVENTIONS' table says 'Types — classes, structs, enums, aliases |
  PascalCase'. RenderResources::SheetFrame's accessor returns
  `SpriteSheet::frame_handle` right next to `SpriteSheetHandle`, so one API
  mixes the two forms. *Fix:* Rename them to FrameHandle and StripHandle;
  references are confined to render_resources.h, sprite_sheet.*,
  texture_object.* and one test. Record the source break in CHANGES.md.
- **low** · other · `engine/render/vulkan/device_resources.cpp:398` — The
  destructor waits twice, and the second wait makes the first redundant.
  vkDeviceWaitIdle is strictly stronger than the timeline wait, and both
  results are discarded, so the first call does nothing useful here. In
  create_window_size_dependent_resources the try-wait result selects the
  device-lost branch, so the pair is meaningful there but not in the
  destructor. The comment above defends only one wait. *Fix:* Remove the
  try_wait_for_gpu call from ~DeviceResources, or state why both are needed.
- **low** · magic-number · `engine/render/vulkan/device_resources.cpp:1083` —
  Surface 'extent decided by swapchain' sentinel spelt 0xFFFFFFFFu in two
  places.
  The same special value appears in two functions with its meaning explained
  only once. A named constant states it once. *Fix:* Add a constant in the
  anonymous namespace, e.g. `const uint32_t extent_chosen_by_swapchain =
  UINT32_MAX;`, and use it at both sites.
- **low** · duplication · `engine/render/vulkan/device_resources.cpp:1940` —
  wait_for_gpu and try_wait_for_gpu duplicate the same timeline wait.
  There are three copies of the wait construction, and the guard conditions
  already differ: try_wait checks owner_, wait_for_gpu does not. The D3D12
  file, which this backend says it transliterates, has one implementation and
  builds the throwing form on top of it. *Fix:* Add a private helper, `VkResult
  wait_for_value(uint64_t) noexcept`, and build wait_for_frame, wait_for_gpu
  (check_vk on its result) and try_wait_for_gpu (== VK_SUCCESS) on it.
- **low** · dead-code · `engine/render/vulkan/renderer.cpp:58` —
  selected_device's "fifo" default is always overwritten, and the setup pair is
  duplicated.
  The default never reaches a caller. Because the same two lines are copied
  into create_device and on_device_restored, a future change to one can miss
  the other. *Fix:* Pass the present-mode name into selected_device, or have it
  take the DeviceResources, and delete the overwrite at both call sites.

## H16. Website source and scripts

- **medium** · duplication · `website/scripts/build-snapshot.mjs:14` — Git
  environment sanitising is copied three times, and only one copy explains why.
  This is a security-relevant invariant: the verified checkout must not be
  redirected by a caller's GIT_* variables. It is written three times, and only
  snapshot-source explains it. A future hardening, such as a new variable to
  set, has to be found and applied in all three. *Fix:* Export gitEnvironment()
  from snapshot-source.mjs. In build-snapshot.mjs use `{ ...gitEnvironment(),
  LABRADOR_DOCS_TAG: tag }`, and in gitIn use `env: gitEnvironment()`.
- **medium** · error-handling ·
  `website/scripts/verify-published-versions.mjs:152` — GitHub releases API
  call is the only network check in the verifier without retry.
  A transient 5xx, 429 or network error from api.github.com fails the
  publication check at once. The same run retries the asset download, the live
  index and erratum links with backoff, so the policy is inconsistent and a
  flaky API answer blocks a release-append PR. *Fix:* Route checkGitHubRelease
  through requestRetried (passing sleep through verifyPublishedVersions), and
  report attempts with statusFound as the other checks do.
- **medium** · duplication ·
  `website/scripts/verify-published-versions.mjs:192` — The starter-ZIP and
  docs-asset filename rules are each written in two places.
  Both copies of starterAsset have the same name and body. The verifier's doc
  comment says the name is the one 'src/lib/starter.ts links', which states a
  coupling the code does not enforce. Likewise, the docs-asset name that
  pack-snapshot writes and the one published-versions validates against are
  separate literals. *Fix:* Export starterAsset and a docsAsset(tag) from
  scripts/published-versions.mjs, which src/lib/versions.ts already imports
  from. Use them in starter.ts, verify-published-versions.mjs,
  pack-snapshot.mjs and validateRelease.
- **medium** · magic-number · `website/src/lib/reference-pages.ts:229` — One
  header's audience caveat is hard-coded inside the generic page generator.
  published.ts is the declared single owner of what each header is and who it
  is for. This special case puts per-header editorial data in layout code,
  where no one adding or renaming a header in published.ts will see it. If
  resource_factory.h is renamed, the note silently disappears. *Fix:* Add an
  optional `note` (or `audienceNote`) to the REFERENCE_MODULES header tuple or
  the ReferenceHeader in published.ts, set it on resource_factory.h, and render
  header.note generically.
- **low** · duplication · `website/acceptance/fixture.mjs:54` — Empty manifest
  written as a hand-formatted literal although canonicalize is imported.
  The manifest's bytes are validated as canonical text. A hand-written copy of
  the canonical form becomes invalid as soon as the key order or schema
  changes, and the fixture build then fails far from the cause. *Fix:* Use
  `canonicalize({ schemaVersion: 1, releases: [], errata: [] })`, or export
  EMPTY_MANIFEST once from published-versions.mjs and import it in both places.
- **low** · duplication · `website/acceptance/screen-reader.mjs:58` —
  driverPath re-implements browsers.mjs resolveDriver.
  The screen-reader check already imports startDriver and killDriver from
  browsers.mjs but keeps its own driver lookup, only because DRIVERS has no
  edge entry. The two lookups can diverge, for example in platform handling,
  since this copy always appends .exe. *Fix:* Add an edge entry (msedgedriver,
  EDGEWEBDRIVER) to browsers.mjs's DRIVERS table, or let resolveDriver accept a
  { name, folderVariable } entry, and call it from screen-reader.mjs.
- **low** · duplication · `website/acceptance/screen-reader.mjs:139` —
  Hand-rolled polling loop and private delay() duplicate webdriver.mjs's
  waitUntil.
  The file already imports from ./webdriver.mjs, which exports waitUntil(check,
  { timeoutMs, intervalMs, message }) with the same 10 s / 100 ms defaults. A
  second copy of the polling logic is one more place for deadline behaviour to
  diverge. *Fix:* Use waitUntil(() => session.executeScript(...), { message:
  'the selector to list the versions' }) and drop the local loop.
- **low** · stale-comment · `website/acceptance/suite.mjs:22` — The header's
  'three exceptions' to read-only page scripts leaves out the scripts that
  scroll the page.
  The paragraph is the suite's statement of how far it intervenes in the page
  under test, so a closed count is a claim a reviewer relies on. Scrolling by
  script is an intervention that the count omits. *Fix:* List script scrolling
  (and the error listener) among the exceptions, or drop the fixed count and
  say that page scripts read state except where they listed.
- **low** · duplication · `website/acceptance/suite.mjs:53` — Suite and
  screen-reader check restate fixture facts that fixture.mjs exports.
  suite.mjs already imports FIXTURE_ONLY_PATH and FIXTURE_ONLY_HEADING from
  fixture.mjs, but it retypes the tags while its comment points at fixture.mjs
  as their source. screen-reader.mjs retypes both the path and a tag. If the
  fixture's tags change, the checks fail confusingly instead of following the
  fixture. *Fix:* Import FIXTURE_TAGS and derive RELEASES, LATEST and OLDEST by
  sorting newest first. In screen-reader.mjs use FIXTURE_ONLY_PATH.slice(1) and
  the oldest fixture tag.
- **low** · dead-code · `website/acceptance/webdriver.mjs:138` — WebDriver
  Session methods with no callers: back, elementText, elementAttribute,
  windowRect.
  Untested protocol wrappers in a client whose header says every command is
  relied on with its own timeout. They are surface to maintain across three
  drivers with no check that they work. *Fix:* Delete the four methods, or
  cover them in tests/acceptance-webdriver.test.mjs if they are kept for future
  checks.
- **low** · duplication · `website/scripts/snapshot-archive.mjs:129` — Three
  near-identical symlink-refusing tree walkers, and byteCompare twice.
  All three walk a built site, refuse symlinks and special entries, and yield
  forward-slash relative paths. They differ only in error wording, so a fix to
  one, such as the handling of a new special-entry type, does not reach the
  others. src/lib has its own fourth to sixth recursive walks
  (reference-indexes.ts:15, :146, :214; IncludeChanges.astro:18). *Fix:* Export
  one regularFiles(directory, describe) from snapshot-archive.mjs, the lowest
  module, plus byteCompare, and use them in snapshot-output.mjs and
  assemble-versions.mjs. *(Severity corrected by the skeptic.)*
- **low** · duplication · `website/scripts/snapshot-output.mjs:13` — Stable-tag
  and full-commit regexes and semver comparison are re-implemented across the
  release scripts.
  The Node-only scripts already import each other (published-versions imports
  releaseVersion from snapshot-source), yet the tag grammar appears four times
  there, twice in one file, and the commit grammar four times.
  version-switch.mjs has a reason to stand alone, since it is frozen into
  snapshots and runs in browsers. The scripts have no such reason. *Fix:* In
  snapshot-source.mjs, export STABLE_TAG and FULL_COMMIT regexes plus
  isStableTag. Use them in snapshot-output.mjs, published-versions.mjs and
  verify-published-versions.mjs, and keep only version-switch.mjs's deliberate
  copy. *(Severity corrected by the skeptic.)*
- **low** · duplication · `website/scripts/verify-published-versions.mjs:29` —
  describeError, retry backoff and isInside are copied between scripts and
  acceptance modules.
  The comment claims a shared backoff, but the code only shares the retry
  count. If the base delay changes in one file, the other silently diverges.
  *Fix:* Export a `retryDelayMs(attempt)` (or FETCH_BACKOFF_MS) next to
  FETCH_RETRIES in assemble-versions.mjs and use it in both loops. Export
  describeError from published-versions.mjs for the scripts.
- **low** · magic-number · `website/src/components/overrides/Banner.astro:24` —
  Unexplained 500 ms delay before filling the live region.
  The header explains why the region is filled after load, not why it waits a
  further half second. This value determines whether screen readers announce
  the notice (the acceptance suite's noticeAnnounceable and SINCE_LOAD measure
  it), so a maintainer cannot tell whether 0, 100 or 1000 ms would be safe.
  *Fix:* Name it, e.g. `const ANNOUNCE_DELAY_MS = 500;`, with a one-line
  rationale or a citation of the measurement that chose it.
- **low** · convention · `website/src/components/overrides/Sidebar.astro:18` —
  Desktop breakpoint written as 50em here and 50rem everywhere else.
  Inside media queries em and rem both resolve against the initial font size,
  so the two values agree today. The spelling still differs from every sibling,
  and a search for the site's breakpoint (50rem) misses this script. *Fix:* Use
  '(min-width: 50rem)' to match Starlight's and the site's other breakpoints.
- **low** · convention · `website/src/lib/content-loader.ts:12` — Relative
  imports inside src/lib mix extensionless and .ts specifiers.
  The .ts form is the one node --test can load directly (the tests import
  src/lib modules). Sibling modules in one folder disagree on the spelling. A
  module written in the extensionless style cannot be pulled into a node --test
  run without changing its imports. *Fix:* Use the explicit `.ts` extension for
  all relative imports in src/lib, as most of the folder already does.
  *(Severity corrected by the skeptic.)*
- **low** · dead-code · `website/src/lib/cpp-header.ts:65` — Parsed fields that
  nothing reads: HeaderDocument.path/includes and TypeItem.head.
  The parser deliberately does the minimum and fails on what it does not
  understand. Fields that are populated but never consumed widen its apparent
  contract and invite readers to rely on, for example, an include list nothing
  tests. *Fix:* Remove `path`, `includes` (with the #include match at line 204)
  and `head`, or add the consumer they were meant for.
- **low** · other · `website/src/lib/excerpt.ts:54` — excerpt.ts's dedent
  strips only tabs while the header reader's strips tabs and spaces.
  SourceFile quotes JSON and CMake files as well as C++. An excerpt that starts
  inside a space-indented block would keep its leading spaces, while find() in
  the same file treats spaces as indentation. All current from/to excerpts
  start at column 0, so nothing is wrong yet, but the two rules disagree within
  one module. *Fix:* Use /^[\t ]*/ in excerpt.ts's dedent, matching its own
  find() and cpp-header.ts.
- **low** · duplication · `website/src/lib/markdown-plugins.ts:88` — Identical
  mdxJsxFlowElement and mdxJsxTextElement handlers, and a second HTML escaper.
  Any change to which JSX attributes are routed through sitePath must be made
  twice. The mermaid escaper restates escapeHtml. *Fix:* Define one
  rewriteJsxAttributes(node, ctx) function and assign it to both visitor keys.
  Have mermaidBlocks use escapeHtml from reference-prose.ts (or move escapeHtml
  to a small shared module).
- **low** · stale-comment · `website/src/lib/published.ts:8` — Header comment
  points readers to a file that does not exist (remark-checkout-links.ts).
  The comment names the owner of the link-resolution rule, and that owner does
  not exist. A maintainer who follows it finds nothing. The code it describes
  moved to markdown-plugins.ts and is now a Sätteri plugin, not a remark
  plugin. *Fix:* Change the citation to `(markdown-plugins.ts, checkoutLinks)`.
  *(Severity corrected by the skeptic.)*
- **low** · duplication · `website/src/lib/reference-mentions.ts:49` —
  Explanation index restates the rendering plugins' language list and JSX-link
  rule instead of sharing them.
  The file's contract is that mentions count "Only links that the authored page
  renders", using "the same resolvers as their Markdown and highlighting
  plugins". The language set and the JSX-link test are copied, not shared. If
  one copy changes, a reference page's "Explained in" list will claim or miss
  links the guide does not actually render, and no test pins the two copies
  together. *Fix:* Export CPP_LANGUAGES and an isJsxLinkName(name) helper from
  reference-mentions.ts (which has no Astro dependencies), and use them in
  code-links.ts and in markdown-plugins.ts's enclosing(). *(Severity corrected
  by the skeptic.)*
- **low** · error-handling · `website/src/lib/reference-prose.ts:36` — A
  missing header path in a header comment is silently left unlinked, while the
  same path in a guide fails the build.
  Public header comments are published word for word, and the site's stated
  rule (PHILOSOPHY T6, applied in codeLink, resolveRelative and RepoLink) is
  that a reference to a file that has gone fails the build. Here, a header
  comment that names a renamed or deleted engine/....h or .md file is published
  as plain text and nothing reports it. That is the stale-contract case the
  site otherwise refuses. *Fix:* Throw, naming the header comment's source, as
  codeLink does. If some comment needs to name a nonexistent path, make that an
  explicit allow-list rather than a silent fallback. *(Severity corrected by
  the skeptic.)*
- **low** · dead-code · `website/src/lib/reference.ts:22` — Facade exports
  explanationSources and referencePages have no callers; referencePages is dead
  everywhere.
  The facade exists to publish the reference module's surface, and SM-52 made
  it the single entry point. Exporting unused functions makes that surface look
  larger than it is. The whole referencePages chain (interface member,
  implementation, facade) is unreachable. *Fix:* Remove the two facade exports.
  Delete referencePages from reference-pages.ts and from the ReferencePages
  interface. Keep explanationSources internal to createReferenceIndexes; it can
  stay on ReferenceIndexes only if a caller needs it. *(Severity corrected by
  the skeptic.)*
- **low** · duplication · `website/src/lib/starter.ts:13` — The GitHub
  repository URL is defined five times, including twice in src/lib.
  starter.ts already imports from ./repository but defines a second constant
  under a different name, with no trailing slash. The scripts define it again,
  with a trailing slash. Renaming or moving the repository means editing seven
  places, using two naming schemes and two slash conventions. *Fix:* In
  starter.ts, import GITHUB_REPOSITORY from ./repository and drop
  REPOSITORY_URL; update StarterDownload.astro to match. Export one
  REPOSITORY_URL from scripts/published-versions.mjs and use it in
  verify-published-versions.mjs and snapshot-output.mjs, deriving the path
  forms from it. *(Severity corrected by the skeptic.)*
- **low** · test-hygiene · `website/tests/pack-snapshot.test.mjs:32` —
  write/temporaryDirectory test helpers copy-pasted into five test files
  despite a shared fixture module.
  Only assemble-versions.test and verify-published-versions.test use the shared
  helpers. The others carry byte-identical copies, including the tmpdir-escape
  assertions in the cleanup. A change to cleanup safety therefore has to be
  made six times. *Fix:* Import temporaryDirectory and write from
  ./fixtures/snapshot-site.mjs in the remaining test files, passing each file's
  prefix. Consider exporting the release-page template as well. *(Severity
  corrected by the skeptic.)*
- **low** · test-hygiene · `website/tests/reference-mentions.test.mjs:15` —
  Test name says 'six teaching pages' but the fixture covers six headers and
  four pages.
  There are four distinct pages (input, drawing, sprites-and-text, game). The
  count in the name is wrong, and a reader may look for six pages under test.
  *Fix:* Rename it to e.g. 'known guide and concept API links generate return
  links on six reference headers', or drop the count.
- **low** · duplication · `website/tests/snapshot-source.test.mjs:31` —
  Temp-directory cleanup helper copied into four test files although the
  fixture exports it.
  website/tests/fixtures/snapshot-site.mjs:16 exports temporaryDirectory(t,
  prefix) and write(), with the same guarded cleanup. snapshot-source.test.mjs,
  snapshot-output.test.mjs:30, snapshot-archive.test.mjs:14 and
  pack-snapshot.test.mjs:39 each re-implement it inline. A change to the
  cleanup guard has to be made five times. *Fix:* Import temporaryDirectory and
  write from ./fixtures/snapshot-site.mjs in the four files.

## H17. Repository configuration

- **low** · config · `.editorconfig:5` — trim_trailing_whitespace applies to
  upstream licence texts that carry trailing blanks.
  samples/linesweeper/content/fonts/OFL.txt,
  samples/minimal/content/fonts/OFL.txt and tests/render/content/OFL.txt
  contain trailing whitespace. An editor that honours .editorconfig rewrites
  these upstream licence texts on save. .gitattributes protects
  tools/web_bundle/Microsoft-LICENSE.txt 'byte for byte' for the same reason,
  but nothing protects the OFL copies. The [*] section also leaves *.mts
  (website/src/lib/version-switch.d.mts) without an indent rule. *Fix:* Add a
  section such as `[{**/OFL.txt,LICENSE,NOTICE,**/*LICENSE*.txt}]` with
  trim_trailing_whitespace = false and insert_final_newline = unset. Add mts to
  the tab-indented glob.
- **low** · config · `.gitignore:4` — Root .gitignore is the generic Visual
  Studio template, whose folder patterns already swallowed tree folders.
  The template's ~350 lines (ASP.NET, Azure, BizTalk, NuGet and similar) have
  nothing to do with a CMake/Ninja tree. Its bare folder patterns ([Rr]elease/,
  [Dd]ebug/, x64/, [Bb]in/, [Ll]og/, packages/) silently drop any real folder
  with those names. That already happened with win32/, which needed exceptions,
  and a samples/**/win32/ folder would still be lost. The 'Get latest'
  instruction invites overwriting the file, which would erase those project
  exceptions. out/ is also listed twice (line 36 and line 374). *Fix:* Replace
  the file with the entries this tree actually produces: out/, .vs/, *.user,
  __pycache__/, node_modules/, .claude/worktrees/ and the like. Drop the 'Get
  latest' line and the redundant duplicate out/.
- **low** · config · `.gitignore:371` — Global *.xwb/*.wav ignore covers the
  tracked test bank tests/audio/content/ping_adpcm.xwb.
  The audio tests depend on this tracked test bank. Edits to the tracked file
  still show, but any new bank or reference WAV added beside it for the
  bank-reader tests (AGENTS: PCM/MS-ADPCM checked against original WAVs) is
  silently skipped by `git add`. The commit then passes locally and fails in
  CI. samples/audio needed its own un-ignore for the same reason; tests/audio
  has none. *Fix:* Drop the global audio-file ignores, which have no build
  output to cover because the banks are authored inputs. Otherwise add
  `!tests/audio/content/*.xwb` (and any WAV inputs) the way
  samples/audio/.gitignore does. *(Severity corrected by the skeptic.)*

## H18. Orphaned website exploration

- **low** · stale-comment · `website/explorations/labradors/CHARACTERS.md:62` —
  CHARACTERS.md omits the dry direction's Examples exchange that the prototype
  renders.
  README.md:63 says CHARACTERS.md 'separates owner-supplied traits from
  invented dialogue and records the copy rules', and it is presented as the
  copy record. One of the five fictional exchanges shown on the page is missing
  from it. *Fix:* Add the dry Examples exchange to CHARACTERS.md's 'Proposed
  exchanges' section, or delete the study.
- **low** · stale-comment · `website/explorations/labradors/QA.md:48` — QA
  record names a heading the browser test does not check.
  The recorded evidence does not match the assertion actually run (the Examples
  context heading is "Pick a sample"). The record has drifted from the code it
  describes. *Fix:* Change QA.md to say the active Examples heading "Pick a
  sample" remains present.
- **low** · stale-comment · `website/explorations/labradors/QA.md:75` — QA.md
  says the regular website check/build is broken, citing a path that no longer
  exists.
  CI's website workflow runs `npm run gate` (check, tests and build) on every
  PR, so the production build works. The checkout is now C:/Projects/Labrador.
  Anyone reading this record would believe the site's build is broken and needs
  restoring before integration. *Fix:* Delete the 'Existing website
  verification limit' section, or reduce it to a dated note that the failure
  was local to that environment and has since been resolved. *(Severity
  corrected by the skeptic.)*
- **low** · dead-code · `website/explorations/labradors/README.md:10` — Whole
  study directory is orphaned: nothing builds, runs, links or tracks it.
  It has 1,077 lines in the public repo, including personal family copy, and
  nothing keeps them in step with the site: no CI job, no TODO/BACKLOG item, no
  mention in website-plan.md. Its scripts depend on run_page.mjs's --input
  contract and on transitive packages, so they can break silently. The site's
  own tsconfig `include: ["**/*"]` probably also pulls its .astro/.ts files
  into `npm run check`, which would make an orphan a potential gate failure.
  *Fix:* Choose one of two options. (a) Delete website/explorations/ if the
  owner has no further use for the voice study. (b) Keep it, but record it as
  an owner decision in docs/website-plan.md or TODO.md, add `explorations` to
  website/tsconfig.json's exclude (or confirm that the check is wanted), and
  add a package.json script for its build and verify steps. *(Severity
  corrected by the skeptic.)*
- **low** · stale-comment · `website/explorations/labradors/README.md:94` —
  README keeps agent-session process history rather than the study's content.
  This is a transcript of how one agent session worked, including its tooling
  environment and the skills available to it. It says nothing about the study,
  and the repo's own reading rules keep history out of living documents. *Fix:*
  Delete the 'Context consulted' section, or reduce it to the design documents
  that informed the study.
- **low** · stale-comment · `website/explorations/labradors/README.md:116` —
  README claims nothing was committed, but the study is on master.
  This is false: commit 9162f44 "feat: add Labrador design study components and
  pages" committed all 16 files and they are on master in a public repository.
  A reader relying on this sentence would think the directory is local scratch.
  *Fix:* Delete the sentence, or replace it with a dated statement that the
  study is committed for reference only and is not part of the site build or
  deployment. *(Severity corrected by the skeptic.)*
- **low** · duplication · `website/explorations/labradors/export-review.mjs:12`
  — The route/file list is hard-coded in four places instead of derived from
  data.ts.
  data.ts `directions` is the source of truth for the routes, and the `dry` →
  '/' special case is repeated in [...direction].astro:7 and
  ConceptPreview.astro:41. Adding or renaming a direction requires five
  coordinated edits, and missing one of them fails silently. *Fix:* Export a
  `route(direction)` / `offlineFile(direction)` helper from data.ts (or a small
  shared .mjs), and derive the lists from it.
- **low** · stale-comment ·
  `website/explorations/labradors/export-review.mjs:43` — Generated offline
  README promises screenshots this script never produces, in a folder it never
  creates.
  export-review.mjs writes offline/README.txt unconditionally, before any
  browser review runs. The screenshots are written by browser-review.mjs to
  out/labrador-dog-concepts/screenshots (browser-review.mjs:9), which is a
  sibling of offline/ and not inside it. So the README asserts evidence that
  may not exist, at a location the reader of offline/ cannot see. The status
  text is also hard-coded rather than derived. *Fix:* Remove the evidence
  claims from the generated README.txt. If they are kept, write them from
  browser-review.mjs after the screenshots exist, and use the correct relative
  path ../screenshots. *(Severity corrected by the skeptic.)*
- **low** · duplication ·
  `website/explorations/labradors/framed-captures.mjs:24` — Element lookup and
  double-rAF wait copied across evaluate strings and scripts.
  Identical page-side snippets are pasted as string literals.
  browser-review.mjs already wraps the rAF wait in `ready()`, but
  framed-captures.mjs repeats the raw string three times. framed-captures.mjs
  also duplicates browser-review.mjs's theme/presence selection logic (lines
  14-18 vs browser-review.mjs:14). *Fix:* Do the scroll and the rect
  measurement in a single evaluate in framed-captures.mjs, add a local
  `ready()` helper, or share a small helpers module between the two driver
  scripts.
- **low** · dead-code ·
  `website/explorations/labradors/src/components/DogAside.astro:12` —
  `long-form` condition is always true, so the `.guides .conversation` grid
  rule is dead.
  Every `home` and `examples` array in data.ts has more than two lines, so
  `long-form` is always applied. The more specific rule at line 106 then always
  overrides line 139's two-column grid, which can never take effect. Both the
  branch and the rule mislead a reader. *Fix:* Drop the `long-form` conditional
  and apply the single-column rule unconditionally, then delete
  concepts.css:139 (and simplify line 166).
- **low** · duplication ·
  `website/explorations/labradors/src/components/DogAside.astro:14` — The same
  'has this dog spoken before' scan is computed twice per line.
  The same predicate is duplicated inline, and the name mapping `line.dog ===
  'vader' ? 'Vader' : 'Ani'` is repeated on lines 16 and 18 and again in
  DogPortrait.astro:4. Changing one copy and not the others would desynchronise
  the portrait/reply logic. *Fix:* Compute `const reply = ...` once per line,
  for example with a map to `{ ...line, reply }` in the frontmatter, and export
  the display-name mapping from data.ts.
- **low** · dead-code ·
  `website/explorations/labradors/src/styles/concepts.css:99` — Unused CSS
  classes `.feature-line` and `.guide-links`.
  No .astro or .ts file in the study emits class feature-line or guide-links
  (grep over src/components, src/pages and data.ts). They are left over from an
  earlier layout. Line 95's selector list also carries `.feature-line p`.
  *Fix:* Delete lines 99-100 and 149, and remove `.feature-line p` from line
  95.
- **low** · ownership · `website/explorations/labradors/verify-review.mjs:7` —
  Script imports domhandler and css-select, which are not declared in
  website/package.json.
  Neither package is listed in website/package.json (devDependencies has parse5
  but not these two). They resolve only because another package's dependency
  tree, through package-lock.json's node_modules/css-select and
  node_modules/domhandler, hoists them to the top level. Any upstream bump or
  dedupe change can break the script without warning. The README (line 113)
  presents this as a virtue rather than a risk. *Fix:* Either declare both
  packages as website devDependencies, or delete the script along with the
  study. *(Severity corrected by the skeptic.)*

## H19. Live document drift

- **medium** · stale-doc · `README.md:295` — README's wasm ctest counts are one
  short and leave out the Firefox entries.
  README tells a reader to check that ctest's last line says 'out of N'. A
  correct wasm build reports a number that differs from README's, because
  ComponentsPointerPageTests was added and the README was not updated. On a
  machine with Firefox the number is 8 higher again, and README never mentions
  those entries. *Fix:* Change this to 33 in Debug and 34 in Release with a
  Chromium, add that a found Firefox registers eight more Firefox* entries, or
  point to AGENTS.md's table instead of repeating the numbers.
- **medium** · stale-doc · `docs/design/ARCHITECTURE.md:277` — Tree diagram
  says docs/review/ holds findings against the current code.
  The design document tells readers the review folder describes the current
  tree, which is the opposite of the rule in the brief and the README. A reader
  who trusts it would act on findings that have already been fixed. *Fix:*
  Change the annotation to say what the folder is: dated reviews kept as
  written, with INDEX.md and each folder's STATUS/README giving current
  disposition.
- **medium** · stale-doc · `docs/design/ARCHITECTURE.md:377` — Module table
  leaves out the browser input and audio backends and the lodepng dependency.
  The dependency table is the stated authority for which external APIs each
  module may use. 'XInput inside xinput/ only' is now false, and the new
  engine-wide third-party dependency is missing from both the table and the
  'bought edge' bullet. *Fix:* Add 'browser Gamepad API inside web/' to input,
  'Web Audio inside webaudio/' to audio, and lodepng to render (as rapidjson is
  listed for assets). List doctest and lodepng in the vcpkg.json bullet.
- **medium** · stale-doc · `docs/verification.md:269` — verification.md says
  wasm-debug has 13 browser tests; ci_gap_report.py requires 14.
  This document defines the gap-report contract. Its count no longer matches
  the tool's list, so a reader checking a record against the document will see
  a mismatch even when the tool is right. *Fix:* Change the count to 14, or
  drop the number and refer to BROWSER_TESTS in tools/ci_gap_report.py.
- **medium** · stale-doc · `engine/render/SEAM.md:364` — SEAM.md says there are
  sixty-three golden frames; there are sixty-four.
  The seam charter's statement that all backends reproduce the golden set
  exactly now quotes a count that is wrong, and it is copied onto the public
  site. *Fix:* Change both places to sixty-four, or state the property without
  a count, as SEAM §4 already does for line counts.
- **medium** · stale-doc · `engine/render/SEAM.md:371` — SEAM.md says only the
  two Direct3D backends rasterise on CI; GL and Vulkan now draw there too.
  The present-tense description of which backends CI holds to the golden set is
  wrong. A reader would think GL and Vulkan golden regressions are only caught
  locally. *Fix:* Say that all four rasterising backends run RenderPixelTests
  on CI (GL on llvmpipe, Vulkan on SwiftShader). If the Direct3D rationale for
  the fourth backend is worth keeping, state it as a reason, not as a
  description of CI today.
- **low** · stale-doc · `docs/design/ARCHITECTURE.md:519` — ARCHITECTURE says
  the include wall covers subfolders of render/ and audio/ only; it covers
  every module's subfolders.
  The design document understates what the check enforces. The earlier
  paragraph about the window move relies on app/win32 being walled, which this
  sentence does not cover. *Fix:* Say that any subfolder of an engine module
  (render, audio, input, app) is a platform/backend folder whose headers are
  its own.
- **low** · stale-doc · `docs/design/ARCHITECTURE.md:565` — ARCHITECTURE says
  the shell asks the machine 'exactly one question'; APPLICATION.md says two.
  Two authoritative design documents contradict each other on the same claim.
  The one in ARCHITECTURE became false when content_root was added. *Fix:*
  Rewrite the ARCHITECTURE sentence to match APPLICATION.md §1: two questions,
  the processor count and the executable's location, each sizing or deciding
  one thing. *(Severity corrected by the skeptic.)*
- **low** · stale-doc · `engine/render/SEAM.md:377` — Docs say
  end_not_comparable covers three frames; six call sites use it.
  Both briefs describe the exemption from golden comparison as a closed set of
  three resize frames. It now also covers three analytic 64x64 cases, so a
  reviewer counting exemptions would be misled. *Fix:* Describe it as the
  harness header does: three resize frames plus the analytic composition and
  viewport cases, each asserted pixel by pixel.
- **low** · stale-doc · `samples/linesweeper/README.md:632` — BlendMode
  rationale still describes DirectXTK SpriteBatch's Begin.
  The reason given for declining BlendMode cites a library constraint the
  engine no longer has. The real cost, a flush per state change because each
  draw call binds one state, is not stated. *Fix:* Restate it in terms of the
  engine's own batch: a state change flushes the view's batch (renderer.h:
  'Changing it mid-list is legal and costs a flush'), so a per-particle blend
  toggle would multiply draw calls.
- **low** · stale-doc · `tools/linesweeper_capture/README.md:36` — Capture
  README says the tool is built in every rasterising preset; it is
  Windows-only.
  A reader looking for LineSweeperCapture in a rasterising browser build will
  not find it, and the README gives the wrong reason. *Fix:* Say it is built in
  every Windows preset with a rasterising backend, and not in -null or wasm
  presets.

## H20. Engine comment truth outside render (math, core, ui, collision, assets, input, audio, app)

- **medium** · stale-comment · `engine/app/application.h:345` — Devices are
  polled once per update step, not once per frame as the contract says.
  Application::update (application.cpp:410-413) polls
  gamepads/keyboard/mouse/touch, and it runs once per fixed step inside
  timer_.Tick. A rendered frame can run 0, 1 or several updates;
  FrameTimings::updates in this same header says so. A catch-up frame polls
  several times, and a zero-update frame polls none. In this header "frame"
  means the rendered frame, so the contract is wrong in the place a caller
  reads it. The input headers (keyboard.h:146,172, mouse.h:140, touch.h:128,
  gamepads.h:19) repeat the wording. *Fix:* Say "once per fixed update, before
  that update's states". Correct the input headers' "once per frame" in the
  same change.
- **medium** · false-contract · `engine/audio/audio_device.h:154` — Seam says
  audio suspends on foreground loss under GamepadReader's contract; it does
  neither.
  An alt-tabbed game keeps playing sound, which contradicts the clause. "Same
  contract as GamepadReader's" is also wrong on substance. GamepadReader's
  suspend stops output and 'resume replays nothing'. XAudio2's Suspend/Resume
  (StopEngine/StartEngine) and Web Audio's context.suspend/resume resume every
  voice where it stopped. A reader of the seam cannot tell when audio actually
  goes quiet or what comes back. *Fix:* Rewrite the clause to state the actual
  trigger: minimised, power-suspended or page hidden, and not foreground loss.
  State that resume continues persistent voices from where they were. Drop the
  GamepadReader equivalence, or change Application if foreground loss really
  should silence audio. *(Severity corrected by the skeptic.)*
- **medium** · false-contract · `engine/audio/audio_resources.h:53` — Claim
  that the AudioDevice releases a replaced bank's voices is false; replaced
  looping voices keep sounding.
  Suppose a bank is replaced or emptied while one of its effects is playing
  with loop=true. The SoundBank and its EffectHandles are gone, but the device
  voice keeps looping, and nothing in the engine can stop it until the device
  dies. The wave bank memory is also held for the device's lifetime. The
  comment presents this as handled. (The known finding at this line is only
  about the block being attached to no declaration.) *Fix:* Either stop every
  effect voice in ~SoundBank (it holds the device pointer and the voice
  handles) or add a release verb to the seam. At minimum, state in
  add_sound_bank's contract that replacing a bank does not stop or free its
  voices. *(Severity corrected by the skeptic.)*
- **medium** · stale-comment · `engine/audio/webaudio/audio_device.cpp:72` —
  Web Audio says its non-immediate stop matches the null backend, but null
  keeps answering playing.
  The comment tells a maintainer that null and Web Audio agree on this
  transition. In fact null behaves like XAudio2 here: it answers playing
  indefinitely. Someone writing a cross-backend test from this comment would
  expect `stopped` under null and get `playing`. *Fix:* Remove the clause about
  null, or replace it with an accurate one: null has no clock, so it keeps
  answering playing as XAudio2 does, and only Web Audio reports the natural
  end.
- **medium** · stale-comment · `engine/collision/collision_object.h:27` —
  Contract says the broad phase indexes GameObject::bounds(); BroadPhase reads
  shape()->bounding_box().
  This is a published public-header contract. A game could reasonably make
  bounds() larger or smaller than its shape, expecting the broad phase to
  follow it, but the broad phase ignores bounds() entirely, and so does
  contacts.cpp's AABB filter, which uses shape()->AABB_intersects. *Fix:* Say
  that the broad phase and the AABB filter both index shape()->bounding_box(),
  and that bounds() is GameObject's extent for culling and layout, which
  collision does not read.
- **medium** · stale-comment · `engine/collision/narrow_phase.cpp:21` — Polygon
  rationale says Shape::edges() returns a std::vector; Shape has no edges() and
  each shape's edges() returns std::array.
  The comment justifies the Polygon struct with an allocation that does not
  exist and a virtual function that was deliberately removed. A reader who
  trusts it learns something false about the math module's API. *Fix:* Restate
  the rationale in present terms: Shape exposes no virtual edge list, so the
  narrow phase copies each shape's corners into one fixed-size Polygon and
  treats every shape type the same way. Drop the claim about std::vector and
  heap allocation. *(Severity corrected by the skeptic.)*
- **medium** · stale-comment · `engine/input/direction.h:66` — direction.h
  quotes gamepad.h text that no longer exists, and direction.cpp points to a
  warning 'two lines above'.
  A verbatim quotation in a published header that matches nothing in the cited
  file misleads more than a paraphrase does. The .cpp pointer sends a
  maintainer to the wrong place. Both broke when SM-45 rewrote gamepad.h.
  *Fix:* Replace the quotation with a paraphrase of what gamepad.h now says
  (edge queries are free functions over two snapshots; Gamepads forwards to
  them). Point direction.cpp at GamepadState's stick comment rather than at
  'two lines above'.
- **medium** · stale-comment · `engine/input/mouse.h:9` — mouse.h and touch.h
  defer to keyboard.h reasoning that SM-45 removed.
  These are published public headers. They point readers at keyboard.h for an
  explanation "at length" that SM-45 deliberately cut. mouse.h and touch.h were
  outside SM-45's six-header scope, so their references were left dangling.
  *Fix:* Either state each reason in a sentence where it is cited (feeding is
  public because `input` cannot friend an `app` class, and test or replay
  feeders need it; focus loss clears buttons because the up event goes to
  another window), or cite only what keyboard.h still says. Remove "at length
  and will not repeat".
- **medium** · stale-comment · `engine/math/ericson_math.cpp:17` —
  test_AABB_AABB is documented as a sphere-vs-AABB test.
  This comment was copied from test_circle_AABB (line 124). It names a sphere
  and a parameter `s` that this function does not have. The function is the
  box-vs-box filter behind RectangleF::intersects, used by the broad phase and
  the view cull, so the false description sits on one of the most-read
  functions in the file. *Fix:* Replace it with "Whether boxes a and b overlap,
  boundary included", or delete it and rely on the in-body NaN note. *(Severity
  corrected by the skeptic.)*
- **medium** · archaeology-comment · `engine/math/ericson_math.cpp:26` —
  Ericson ports narrate former code and past bugs.
  These comments describe what the code was, which CONVENTIONS says belongs in
  `git log`. Lines 368-373 also cite `Triangle().contains(p)`, a member that no
  longer exists (shape.h:93 says there is no contains(const Point2F&)). SM-47
  cleaned rectangle_rotated.cpp but not this file. *Fix:* Keep the
  present-tense rationale: the accepting form makes NaN report no hit; the
  guard prevents 0/0 for a degenerate edge; an all-zero polygon encloses
  nothing. Remove the then/now narration and the stale contains() reference.
- **medium** · stale-comment · `engine/math/ericson_math.cpp:496` —
  test_point_triangle says it computes barycentric coordinates; it deliberately
  does not.
  The header (ericson_math.h:220-224) says explicitly that this is NOT the
  barycentric form, because that form divides by zero on collinear triangles.
  The .cpp comment states the opposite of the contract. *Fix:* Delete the
  two-line comment. The header owns the contract.
- **medium** · stale-comment · `engine/math/inflate.cpp:45` — Comment says
  narrow_phase.cpp uses signed_area; it does not.
  The comment backs a design decision with a cross-module usage that does not
  exist. A reader who goes looking in narrow_phase for agreement will not find
  it, and someone changing signed_area may think collision depends on it.
  *Fix:* Delete the narrow_phase clause. Keep the justification that
  signed_area is ericson_math's definition of 'encloses nothing', shared with
  point_in_convex_polygon and test_segment_convex_polygon.
- **medium** · stale-comment · `engine/math/intersects.cpp:476` —
  point_rectangle_rotated_intersect cites allocations that no longer exist,
  plus history.
  Quad keeps its points in fixed storage, Quad::triangles() returns
  std::array<Triangle, 2>, and RectangleRotated::quad() builds from cached
  corners. Building and triangulating a Quad allocates nothing, so the
  "allocate twice" argument is false today. "now answers false rather than
  throwing" is history. *Fix:* Keep the reason that still holds: Quad's
  constructor throws on a default-constructed rectangle's coincident corners.
  Drop the allocation claim and the "now" clause.
- **low** · stale-comment · `engine/app/APPLICATION.md:28` — View cost stated
  in D3D11-only terms.
  Deferred contexts and SpriteBatch are D3D11 mechanisms. D3D12, Vulkan and GL
  record per-view state differently, and the null backend builds no device
  objects. The shell's rationale document should state the seam-level cost,
  since application.h:90-93 already says it generically. *Fix:* "every view
  costs the backend's per-view recording state (SEAM.md), built eagerly..."
- **low** · stale-comment · `engine/app/application.cpp:139` — "four panes at
  the very most" contradicts the configurable view_capacity.
  view_capacity is any value of at least 1 (validate()). Four is only its
  default. Calling four a hard maximum misstates the option the comment cites.
  *Fix:* "...to draw a frame whose pane count the game states in
  view_capacity."
- **low** · archaeology-comment · `engine/app/application.cpp:401` — Comment
  points to gamepads.h for detectors "this replaced".
  This is history (what the code replaced). gamepads.h:14-21 gives the
  present-tense rationale and mentions no "two" detectors that were replaced,
  so the pointer is also inaccurate. *Fix:* "...true - gamepads.h says why the
  engine polls and a game does not."
- **low** · stale-comment · `engine/app/application.cpp:598` — Says frame
  arithmetic is create_window's job, but Window does it.
  Application::create_window (lines 180-203) only copies options into
  WindowOptions. The client-to-outer conversion is
  outer_size_for_client/Impl::outer_size in engine/app/win32/window.cpp, and
  window.h:144 says "Turning one into the other is this class's job and nobody
  else's". The comment sends the reader to the wrong file. *Fix:* Say "that is
  the Window's job (window.h, WindowOptions::client_size)". *(Severity
  corrected by the skeptic.)*
- **low** · archaeology-comment · `engine/app/application.cpp:615` —
  on_device_lost narrates a past bug.
  "left every object holding a freed SoundBank*" describes what an earlier
  version did. CONVENTIONS forbids archaeology comments, and the present-tense
  reason (audio is not device-owned, and banks are borrowed by objects) fits in
  one clause. *Fix:* "Only the resources the device holds; audio is not one,
  and objects hold borrowed SoundBank pointers across a device loss."
- **low** · stale-comment · `engine/app/application.cpp:679` — "EIGHT
  FORWARDERS" comment heads a block of thirteen.
  The block below has on_key_down, on_key_up, on_text, on_mouse_move,
  on_mouse_button_down, on_mouse_button_up, on_mouse_capture_lost,
  on_mouse_wheel, on_mouse_wheel_horizontal and four touch forwarders: 13 in
  all. The count went stale as text, capture-loss, horizontal-wheel and touch
  handlers were added. The comment also sits between partitioner() and the
  forwarders, while keyboard(), mouse(), touch() and gamepads() are defined
  after them, so the accessor block is split in two. *Fix:* Drop the number
  ("THE FORWARDERS, AND NOT ONE DECISION IN THEM") and move the keyboard(),
  mouse(), touch(), gamepads() and window() accessors up to sit with the other
  accessors.
- **low** · stale-comment · `engine/app/application.h:420` — Citation
  APPLICATION.md#5 does not support the privacy claim.
  APPLICATION.md §5 argues why the callbacks stay on Application rather than
  moving into Window. It says nothing about why the overrides are private or
  about reaching them through a base pointer. The citation check passes because
  the section exists, but the claim has no source. *Fix:* Drop the citation,
  since the sentence is self-contained, or add the private-override rationale
  to §5.
- **low** · stale-comment · `engine/app/web/dom_events.h:12` — Points to
  win32/window.cpp for translation that SM-50 moved to helpers.
  Key-position and UTF-16 translation now live in win32/keyboard_input.cpp, and
  touch translation in win32/pointer_input.cpp, the direct counterparts of
  dom_events. The pointer names the dispatcher, not the counterpart. *Fix:*
  "...the browser half of engine/app/win32/keyboard_input.h and
  pointer_input.h."
- **low** · archaeology-comment · `engine/app/win32/window.cpp:340` —
  Constructor comment repeats outer_size_for_client's rationale as history.
  outer_size_for_client (lines 70-77) already gives the same 1264x681 rationale
  in the present conditional. This copy is in the past tense ("came out",
  "nothing said so"), so it is archaeology and a duplicate that can drift.
  *Fix:* Cut it to "CreateWindowExW takes an outer rect; outer_size_for_client
  says why the conversion is needed" and keep the part about the WS_POPUP
  branch and the system DPI.
- **low** · stale-comment · `engine/app/win32/window.cpp:373` — Win32 window
  comment names D3D11's private DeviceResources as the renderer's behaviour.
  The Win32 window drives all four Windows render backends. The contract it
  depends on is Renderer::window_size_changed answering false before
  create_device, which renderer.h:340-343 states.
  DeviceResources::WindowSizeChanged exists only in
  engine/render/d3d11/device_resources.cpp:331. The window's justification
  therefore cites one backend's internals, which do not describe D3D12, GL or
  Vulkan. *Fix:* Cite the seam instead: "Renderer::window_size_changed rebuilds
  nothing before create_device and answers false (renderer.h)". Describe the
  stretch generically, as a back buffer created at the preset and scaled to the
  monitor.
- **low** · stale-comment · `engine/app/win32/window.cpp:705` — WM_MOVE
  rationale counts backends as if there were three.
  There are five backends now. Vulkan answers back_buffer_size from its
  engine-owned colour target (vulkan/renderer.cpp:1243-1253, "for a reason no
  other backend has"), and null answers from stored width/height. "Two of the
  backends" plus GL was the D3D11/D3D12/GL era, so the comment now misdescribes
  which backends behave which way. *Fix:* Remove the count. Either say "the D3D
  backends answer from a swap chain and Vulkan from its colour target; GL
  answers from the window" or cite SEAM.md#6, which renderer.h already names as
  owning each backend's answer. *(Severity corrected by the skeptic.)*
- **low** · archaeology-comment · `engine/assets/asset_manifest_loader.cpp:34`
  — Default justified by what "every existing manifest already meant".
  'already meant' refers to manifests that existed before `optional` did, which
  is history. The contract is stated in asset_manifest.h and
  asset_manifest_loader.h. *Fix:* Cut it to 'Absent means required
  (asset_manifest.h).' or remove the comment.
- **low** · stale-comment · `engine/assets/sound_bank_loader.cpp:113` — "both
  of this tree's" audio backends: the tree has three.
  This is a stale count. The engine has three audio backends selected by
  LABRADOR_AUDIO_BACKEND (xaudio2, webaudio, null), so the claim that the
  branch is unreachable is made about a backend set that no longer exists.
  *Fix:* Write 'which every backend in this tree does', or name the three
  backends, after confirming that each one checks the list on open. *(Severity
  corrected by the skeptic.)*
- **low** · archaeology-comment · `engine/assets/sprite_sheet_loader.cpp:71` —
  Comment narrates a removed SpriteFrame member, using a non-root path.
  'already existed ... is gone too' describes what the code used to be, which
  CONVENTIONS says is never a comment. The citation `render/sprite_frame.h` is
  also not written from the repository root, unlike every other path in this
  unit (for example `engine/render/sprite_geometry.h` in the same comment).
  *Fix:* Write it in the present tense: 'SpriteFrame has no member to hold it
  (engine/render/sprite_frame.h).'
- **low** · stale-comment · `engine/audio/audio_resources.h:53` — A comment
  block in a public header is attached to no declaration.
  It sits between add_sound_bank and the SOUND GROUPS block, separated from
  both by blank lines, so the website reference reader and a maintainer cannot
  tell which declaration it qualifies. Its last sentence repeats
  add_sound_bank's own contract. *Fix:* Fold the first two sentences into
  add_sound_bank's comment (or the class comment) and delete the repeated last
  sentence.
- **low** · stale-comment · `engine/audio/null/audio_device.cpp:18` — null
  backend cites an argument in recording.h that SM-46 removed, and still counts
  two backends.
  SM-46 rewrote recording.h, and this pointer in its sibling .cpp now leads to
  text that is not there. The "two backends" wording predates
  engine/audio/webaudio/. *Fix:* Drop the "recording.h carries the argument"
  sentence, or state the rationale here. Change line 147-150 to "the same words
  every backend uses", as webaudio's comment does. *(Severity corrected by the
  skeptic.)*
- **low** · archaeology-comment · `engine/audio/sound_bank.cpp:41` — "in every
  build now" implies an earlier state.
  "now" contrasts the code with a previous version. CONVENTIONS' tense test
  says that belongs in history, not a comment. *Fix:* Drop "now": "every play
  below rejects an unresolved handle in every build".
- **low** · stale-comment · `engine/audio/sound_bank.h:26` — "four or five
  things an audio API actually does" understates the seam.
  The count misdescribes the seam's width to a reader deciding what a new
  backend must implement. *Fix:* Drop the count, for example 'for what an audio
  API actually does (audio_device.h)'.
- **low** · archaeology-comment · `engine/audio/xaudio2/audio_device.cpp:15` —
  XAudio2 backend's file comment narrates what the code used to be.
  CONVENTIONS' Comments section says history ("what it used to be") is never a
  comment. android.md §3.2 is now headed "BUILT, 2026-08-27", so the quoted
  "provably false today" describes a past state. The include-check name is also
  missing its `.cmake` extension, so the path cannot be checked. *Fix:* Keep
  only the present-tense constraint: this is the only file that includes
  <Audio.h>, and cmake/check_engine_includes.cmake forbids backend includes
  from outside this folder. Drop the android.md narrative. *(Severity corrected
  by the skeptic.)*
- **low** · stale-comment · `engine/audio/xaudio2/audio_device.cpp:31` — "six
  lines from the library" line count is false.
  `#include <Audio.h>` is at line 5 and Impl's ordered members are at lines
  55-61, about fifty lines apart. This is the kind of fragile line-count claim
  SM-47 removed elsewhere. *Fix:* Say "in Impl's declaration order, in this
  file" and drop the count.
- **low** · duplication · `engine/audio/xaudio2/audio_device.cpp:47` — "The one
  place the container's spelling appears" is said in two places.
  The comment is false by its own count. Both backends depend on the same
  spelling, and nothing ties the two copies together. *Fix:* Put the container
  path helper beside read_xwb_file in engine/audio/xwb_file.h (the format's
  owner) and call it from both backends.
- **low** · archaeology-comment · `engine/collision/contacts.cpp:71` — "The
  loops this replaces did neither" is history.
  It describes code that no longer exists. git log answers that question; the
  first sentence is the whole rationale. *Fix:* Delete the second sentence.
- **low** · duplication · `engine/collision/contacts.h:43` — Benchmark figure
  is restated in contacts.h as well as broad_phase.h.
  The same measured number lives in two published headers, so a re-measurement
  has to find both. CONVENTIONS prefers citing over re-deriving. *Fix:* Keep
  the numbers in broad_phase.h only, and in contacts.h say 'all-pairs, O(n^2);
  broad_phase.h gives the measured cost'.
- **low** · stale-comment · `engine/collision/narrow_phase.cpp:44` — Engine
  comment refers to "this game's collision traffic".
  The engine has no game, and this is the only 'this game' in engine/ (git grep
  "this game" -- engine). It is a leftover from before the ColourWars split and
  asserts a workload the engine cannot know. *Fix:* Write 'on rectangle pairs,
  the commonest pairing in tile-based content', or drop the clause.
- **low** · stale-comment · `engine/collision/narrow_phase.cpp:133` — Claims
  the Quad constructor allocates; Quad has fixed storage.
  The allocation claim is false (Quad stores `Point2F points_[4]` and is_valid
  says "no allocation"). Only the double validation is real, so the stated
  reason for this code shape is half wrong. *Fix:* Keep only the true reason:
  building a Quad would validate convexity twice, through quad() and then the
  converting constructor, while the corners and axes are already valid. Remove
  'allocates'. *(Severity corrected by the skeptic.)*
- **low** · stale-comment · `engine/collision/narrow_phase.cpp:462` — Early-out
  comment says "all seven" axes; quad against quad tests eight.
  Polygon::max_points is 4 and a quad contributes four axes (fill_axes(polygon,
  4)), so a quad-quad pair has eight candidate axes. A rectangle pair has four.
  'Seven' matches only triangle against quad, so the number misleads anyone
  reasoning about worst-case cost. *Fix:* Say "rather than every axis of both
  shapes" instead of quoting a count.
- **low** · stale-comment · `engine/collision/narrow_phase.cpp:474` —
  Zero-normal fallback gives a reason that measurable() has already ruled out.
  measurable() already returns early when a shape has no axes or no interior,
  and every non-separated axis test produces a unit normal of ±axis. The stated
  reason therefore cannot hold here. The only remaining way in is a penetration
  that is not below FLT_MAX. The same check at line 429 in circle_polygon has
  the same problem. *Fix:* Rewrite both comments to state the real residual
  case (no axis produced a penetration below the sentinel), or remove the guard
  and assert that least was updated. *(Severity corrected by the skeptic.)*
- **low** · naming · `engine/collision/resolve.h:50` — Precondition bullet for
  min_axis_alignment is garbled.
  The checked condition (resolve.cpp:51) is `std::abs(dot(normal, axis)) >=
  min_axis_alignment`. 'no more than X away from being useless' reads as the
  opposite of a lower bound on the cosine and does not say what is measured.
  *Fix:* Write: '|dot(normal, axis)| is at least min_axis_alignment.'
- **low** · stale-comment · `engine/core/random.cpp:12` — The comment credits
  the mixing constants to Mulberry and frames them as migration history.
  As far as I know, Mulberry32 uses the 0x6D2B79F5 increment and an imul-based
  mixer. 0x21F0AAAD/0x735A2D97 look like the two-round 32-bit integer hash from
  Chris Wellons' hash-prospector, so the comment sends a reader who checks the
  provenance to the wrong source. 'Already used' and 'a game moving onto this'
  are migration history. LineSweeper has not moved onto it either:
  samples/linesweeper/rules/tick.cpp:51 and presentation/particles.cpp:473
  still carry their own copies. *Fix:* State the algorithm and its source in
  present tense, e.g. "Weyl increment 0x9E3779B9, then the hash-prospector
  two-round 32-bit mixer. These are the same draws as LineSweeper's rules-side
  generator." Drop the migration framing. *(Severity corrected by the
  skeptic.)*
- **low** · stale-comment · `engine/core/state.h:17` — State::update points at
  a rationale in GameObject::update that the SM-45 audit removed.
  The reader is sent to a declaration that no longer gives any reason. The
  citation checker cannot catch this because the pointer is prose, not a
  document path. *Fix:* Cite only the source that still holds the reason:
  "Frame time is a parameter, not shared state (docs/design/PHILOSOPHY.md,
  Services and lifetimes)." *(Severity corrected by the skeptic.)*
- **low** · naming · `engine/core/state.h:88` — The public State header uses
  'frame' for a stack entry, a term defined only in StateContext's private
  section.
  In a header whose neighbouring comments talk about drawing frames, 'every
  frame is told' reads as 'every rendered frame'. The meaning intended is every
  state on the stack, and StateContext::Frame is private. state_context.h:52,
  61 and 66 do the same in public comments ('which frame is being replaced',
  'after this frame is popped'). *Fix:* Say 'every state on the stack is told'
  and 'the state that most needs to hear this'. In state_context.h's public
  comments, prefer 'stack entry' or 'pushed state' over 'frame'.
- **low** · archaeology-comment · `engine/core/state_context.cpp:32` —
  ~StateContext comment narrates the former defaulted destructor and the bug it
  had.
  CONVENTIONS and AGENTS say comments state current contract or rationale, not
  history. SM-45 cleaned the header and SM-47 cleaned scene.cpp, but this
  implementation comment still tells the story of the previous code. *Fix:*
  Reduce it to the present-tense reason: the destructor calls clear() so that
  states are destroyed top-down, in dependency order, for every owner.
  Application also calls clear() earlier, before its own members are destroyed.
- **low** · stale-comment · `engine/input/touch.h:73` — TouchState::cancelled
  defers to a reason Mouse::cancel_buttons does not give.
  The cross-reference points at text that was trimmed. The rationale now exists
  only here, and it is attributed to the wrong place. (This is distinct from
  the known mouse/touch→keyboard.h deferral.) *Fix:* Drop the attribution and
  keep the sentence's own reason, or point to wherever the cancel rationale now
  lives.
- **low** · archaeology-comment · `engine/math/intersects.cpp:141` —
  Former-code narratives in math .cpp files not covered by SM-47.
  SM-47 cleaned this pattern out of rectangle_rotated.cpp only. These comments
  narrate removed code or past bugs, and AGENTS says to keep history out of the
  code. Some are already false, for example test_circle_circle now has a
  caller. *Fix:* Rewrite each as a present-tense statement of the rule and its
  reason, for example 'closed clip so a vertex-to-vertex traversal is caught'.
  Drop the history; the commit log keeps it.
- **low** · stale-comment · `engine/math/matrix3x2f.h:114` — Comment's
  justification for translation() describes Vector2F::angle wrongly.
  Vector2F::angle has no factory form. Its two overloads, `float angle() const`
  and `static float angle(const Vector2F& vec)` (vector2f.h:60-61), both read
  an angle. The factory is unit_vec_from_angle. The precedent the comment cites
  for overloading a factory and a reader under one name does not exist. *Fix:*
  Drop the Vector2F::angle sentence, or cite a real factory/reader pair.
  *(Severity corrected by the skeptic.)*
- **low** · stale-comment · `engine/math/matrix3x2f.h:132` — References to
  "scalar.h's ordering" of tolerances that scalar.h no longer presents as an
  ordering.
  scalar.h:19-29 is a list of "Related thresholds [that] serve different
  purposes", not an ordering. It includes min_axis_alignment, which it says
  bounds a direction and "is not a rounding tolerance". So "every tolerance ...
  bounds a LENGTH" is false against the list it cites. *Fix:* Refer to
  scalar.h's list of related thresholds, and say the default and segment
  tolerances are absolute lengths, without claiming every listed threshold is
  one.
- **low** · stale-comment · `engine/math/triangle.cpp:82` — The comment refers
  to an angles() function that does not exist.
  A reader looking for angles() will not find it. The invariant is about the
  three angle_N() values. *Fix:* Say 'makes angle_0() + angle_1() + angle_2()
  sum to pi'.
- **low** · archaeology-comment · `engine/math/triangle.cpp:155` —
  edge_opposite_vertex comment narrates a fixed bug.
  This is past-tense bug narration in the code. Lines 150-153 already give the
  present-tense rationale (vertex index vs edge index). triangle.h:62-65 has
  the same kind of comment about a removed redeclaration: "A byte-for-byte copy
  would hide the public one". *Fix:* Delete lines 155-159. Reduce
  triangle.h:62-65 to nothing, or to a contract statement. *(Severity corrected
  by the skeptic.)*
- **low** · stale-comment · `engine/math/vector2f.h:17` — Vector2F constructor
  comment says "explicit, all of them" above a non-explicit constructor, and
  argues from Quad.
  The two-float constructor is not explicit, and the brace-initialised
  constants in vector2f.cpp rely on that. The comment also reasons about Quad's
  constructors, which belong in quad.h. A reader of Vector2F gets a false "all
  of them" and a rationale for a different type. *Fix:* Say "every
  single-argument (converting) constructor is explicit" and move the Quad
  sentence to quad.h:23-24.
- **low** · stale-comment · `engine/ui/focus.h:158` — refresh_style comment
  says widgets are painted back to 'one of two colours', but FocusStyle has
  three.
  The comment predates the disabled state and now misdescribes the repaint this
  published header documents. *Fix:* Say 'back to the style's focused,
  unfocused or disabled colour'.
- **low** · duplication · `engine/ui/navigation.cpp:33` — The edges-not-centres
  rationale is written out twice, in navigation.h and navigation.cpp.
  The same paragraph in two places will drift. The header, which is published,
  is the owner of the rule. *Fix:* Cut the .cpp copy to one line pointing at
  rule 1 in navigation.h.
- **low** · archaeology-comment · `engine/ui/widget.cpp:110` —
  UiContainer::draw and cull_bounds comments describe earlier behaviour and an
  earlier API.
  Both comments narrate removed code: a container that ignored hidden(), and a
  'predicate form' of bounds that no longer exists. That forces readers to
  reconstruct history to understand the current rule. *Fix:* State the current
  rule only: a hidden container draws nothing, and bounds are the union of
  children with positive area.

## H21. Render comment truth and archaeology (shared seam and five backends)

- **medium** · stale-comment · `engine/render/d3d11/backend.h:48` —
  RenderResources::Impl class comment says fonts and sheets live here; the
  class body says they do not.
  The class has exactly one member table, `textures`, and
  engine/render/render_resources.h:35-37 says the font and sheet tables are
  members of RenderResources. Lines 48-57 describe an earlier three-table
  layout. Line 64 contradicts them and adds archaeology ('now'). A reader of
  the backend's central header gets a false picture of where resources are
  stored. *Fix:* Replace lines 48-57 and 63-64 with one present-tense
  statement: Impl holds only the texture table, keyed by D3D11 views; fonts and
  sheets are RenderResources' own members (render_resources.h). The null
  backend's equivalent comment at null/backend.h:54-56 has the right shape.
  *(Severity corrected by the skeptic.)*
- **medium** · archaeology-comment ·
  `engine/render/d3d12/device_resources.cpp:30` — Comment narrates what its own
  earlier sentence said.
  CONVENTIONS' Never list says outright: "a comment narrating what the code
  used to be", and a sentence narrating what this comment used to say "has lost
  the thread twice". The tree-wide census ("the only NEGATIVE low-tier
  sentence") is also unverifiable. *Fix:* Keep one present-tense rationale:
  feature level 11_0 is D3D12's minimum, so this backend excludes pre-D3D12
  hardware that the D3D11 and GL backends still reach. The named low tier is
  12_2. Drop the rest.
- **medium** · stale-comment · `engine/render/d3d12/device_resources.cpp:231` —
  WARP comment says CI skips GL RenderPixelTests and D3D12 is only the second
  CI-checked backend.
  CI now runs GL RenderPixelTests on Mesa llvmpipe and Vulkan on SwiftShader
  (AGENTS.md says the same), so both halves of the claim are false. A reader
  would wrongly conclude that CI never checks GL pixels. *Fix:* Cut it to the
  current rationale: debug builds fall back to WARP so a GPU-less machine can
  still run the D3D12 pixel contract. Drop the GL narrative and the backend
  ordinal. *(Severity corrected by the skeptic.)*
- **medium** · stale-comment · `engine/render/d3d12/device_resources.cpp:589` —
  wait_for_gpu comment contradicts the resize comment about E_OUTOFMEMORY.
  The two comments make opposite claims about the same function. The code's
  fallback to E_FAIL actually handles the non-removal case, so the first
  sentence of the wait_for_gpu comment is false. *Fix:* Rewrite it as: "A
  failed wait is reported as the device-removed reason when the device has
  gone, else as E_FAIL; Signal/SetEventOnCompletion failures on a live device
  lose their own HRESULT here." Alternatively, keep the failing HRESULT and
  throw that. *(Severity corrected by the skeptic.)*
- **medium** · archaeology-comment · `engine/render/d3d12/renderer.cpp:997` —
  Resize comment argues against frame_open(), a function that no longer exists.
  The comment describes the behaviour of removed code in the present tense, so
  a reader goes looking for frame_open(). That is the archaeology CONVENTIONS
  says to remove. *Fix:* State the current rule only. Restart is keyed on
  Impl::frame_begun, the begin_frame..end_frame interval renderer.h names,
  because 'a list is recording' is false between begin_frame and the first
  set_view_count.
- **medium** · stale-comment · `engine/render/d3d12/texture_factory.cpp:94` —
  Hard-coded line counts and the cross-reference to renderer.h are false.
  Current counts are vulkan 399, d3d12 346, gl 229, d3d11 116 and null 49.
  Three of the five numbers are wrong. Grepping renderer.h (and engine/,
  docs/design and website/src) for these counts or for a list of
  texture-factory sizes finds nothing, so 'renderer.h lists in one place and
  holds all five against' points at text that does not exist. Line counts in
  comments go stale on every edit. *Fix:* Remove the numbers and the renderer.h
  claim. Keep only the reason: no initial-data parameter, so the backend
  stages, copies and waits itself.
- **medium** · stale-comment · `engine/render/gl/texture_factory.cpp:132` — DDS
  census in comments is false against the current trees.
  Two fragile counts that drifted: this repo alone now has three uncompressed
  sample textures, so 'only the two' and '41 of 43' are both wrong, and they
  span a separate repository nobody updates this comment against. *Fix:* Drop
  the counts; state the rationale qualitatively (font atlases and most art are
  block compressed, so refusing would leave a no-S3TC context without text) and
  that the BGRA swap is a load-path cost only.
- **medium** · stale-comment · `engine/render/resource_factory.h:87` —
  add_texture_asset's contract says only two engine callers exist and that a
  client never sees it; both claims are false.
  Public header prose is published word for word on the website. This contract
  tells readers that a function is internal when the site's own testing guide
  teaches clients to call it. The 'two of these / one' census is also stale
  since add_white_texture was added. *Fix:* Rewrite the class-level paragraph
  so it names three engine-side functions and one backend function. Change line
  87 to say the engine's loaders call it, and that tests and tools may call it
  directly to install decoded TextureData. Update the website page in the same
  commit. *(Severity corrected by the skeptic.)*
- **medium** · archaeology-comment · `engine/render/sprite.hlsl:57` —
  sprite.hlsl explains what earlier versions of itself said and where it used
  to live.
  CONVENTIONS lists exactly this in its Never section: 'a comment narrating
  what the code used to be', and 'a sentence narrating what this very comment
  used to say has lost the thread twice'. This file is outside SM-47's targets,
  so the case is not tracked. *Fix:* Keep the facts in the present tense: the
  D3D11 floor is 10.0, the shader is shared by three HLSL backends, and two
  multiply-adds replace a 4x4 matrix. Delete the 'used to', 'still did not
  move' and SpriteBatch clauses.
- **medium** · stale-comment · `engine/render/sprite_geometry.cpp:158` — Worked
  example contradicts itself: claims x=10.9 gives 7, then says x=10.9
  demonstrates nothing.
  The comment backs a pixel-contract rule (edges first, then the difference)
  with an example that is false, and then admits it in a parenthetical. Width 8
  at x=10.9 truncates to 8 either way. The comment also carries history ('the
  rectangle went through a RECT of four truncated longs'). A maintainer cannot
  tell which sentence to trust. *Fix:* Replace it with a correct example, such
  as x=10.6 width=7.6: edges give 18-10=8 but trunc(width)=7. Alternatively,
  cite viewport.h's 721-row split, which is correct. Drop the RECT history and
  the parenthetical.
- **medium** · stale-comment · `engine/render/texture_data.h:37` — TextureData
  says the engine loads only two things, which leaves out PNG.
  This comment is false against the current loader and is part of the published
  renderer-writer reference. It gives the wrong count for the very point it
  makes: one creation function for every file kind. *Fix:* Say: "Every texture
  source this engine reads - a .dds, a .png and the atlas inside a .spritefont
  - comes out as one of these."
- **medium** · stale-comment · `engine/render/texture_format.h:8` —
  TextureFormat rationale was written before PNG support and is now false.
  The engine now loads three file kinds, and a third reader
  (read_png_file/decode_png), plus block_decode, produces TextureFormat. Every
  PNG texture arrives as r8g8b8a8_unorm, so the claim that no file says it is
  false. This published header describes a format set that no longer exists.
  *Fix:* Name PNG as a third source that states no DXGI number. Count the
  producers correctly (the .dds, .spritefont and .png readers, plus the block
  decoder). Say that r8g8b8a8_unorm is what every PNG and every decoded block
  texture becomes. *(Severity corrected by the skeptic.)*
- **medium** · stale-comment · `engine/render/vulkan/backend.h:32` — Header
  says the Vulkan preset skips RenderPixelTests in CI, but CI draws them on
  SwiftShader.
  This is the backend's top-level rationale header. It states a false
  verification fact that contradicts CI, AGENTS.md ("its Vulkan job on Chrome's
  SwiftShader, and fails if RenderPixelTests names another device") and a
  comment in the same folder. A reader could conclude that Vulkan pixel
  regressions are not caught in CI. *Fix:* Replace the sentence: CI runs
  RenderPixelTests on Chrome's SwiftShader ICD, which is a software device and
  not a real driver, and ci_gap_report.py covers the real-driver run.
  *(Severity corrected by the skeptic.)*
- **medium** · archaeology-comment ·
  `engine/render/vulkan/device_resources.cpp:1194` — create_swapchain and
  create_window_size_dependent_resources narrate a past bug.
  Both passages describe what the code used to do ("did not come across", "was
  not", "threw", "did not have") instead of the current contract, which
  CONVENTIONS bans. *Fix:* State the current rule: a device loss from
  vkCreateSwapchainKHR, when a swapchain existed, answers false and recovers
  rather than throwing, because this path runs inside a window procedure.
  Delete the history clauses at 1196-1203 and 1409-1410.
- **medium** · archaeology-comment ·
  `engine/render/vulkan/device_resources.cpp:1529` — execute() comment narrates
  a lost line from the D3D12 transliteration.
  The rationale (advancing the counter before a failed submit leaves waits
  hanging on a value nothing will signal) is present-tense and worth keeping.
  The story of what the transliteration lost is archaeology that belongs to git
  log. *Fix:* Cut it to: the counter advances only after a successful submit,
  because a non-loss submit failure would otherwise leave timeline_value_
  naming a value nothing signals, and wait_for_gpu would hang.
- **medium** · archaeology-comment ·
  `engine/render/vulkan/device_resources.cpp:1786` — Acquire-barrier comment
  narrates the old code and carries a stale barrier count.
  "This was TOP_OF_PIPE" is archaeology. The 'five other' census is already
  wrong, and 'exactly one' is false because transition_colour can emit
  TOP_OF_PIPE. The real rationale (the barrier's source stage must equal the
  semaphore wait stage) is buried in it. *Fix:* Keep only the present-tense
  rule: the source stage must be the acquire semaphore's wait stage, because a
  TOP_OF_PIPE source is not ordered after the presentation engine's read.
  Delete the history and the count.
- **medium** · stale-comment · `engine/render/vulkan/device_resources.h:153` —
  block_compression comment repeats the stale 41-of-43 .dds census and miscites
  texture_format.h.
  The count is ColourWars-era and false here, and the cited document does not
  support it. *Fix:* Replace it with the claim texture_format.h actually makes:
  compressed art is the common case and every font atlas is block compressed.
- **medium** · archaeology-comment ·
  `engine/render/vulkan/render_resources.cpp:99` — Archaeology plus a false
  claim about a using-declaration.
  "Counting them as one line was a claim texture_factory.cpp then cited"
  narrates what an earlier comment said, which CONVENTIONS' Never section bans.
  The using-declaration does not exist, and "fourteen-line" is a line census of
  another file. *Fix:* Keep the rationale: no descriptor slot, because sets are
  per run from a frame-reset pool. Delete the leak-history sentence, the line
  census and the last sentence, and drop 'and using-declaration'.
- **medium** · stale-comment · `engine/render/vulkan/texture_factory.cpp:92` —
  User-facing throw cites a stale '41 of the 43 .dds' content census.
  The diagnostic shown to a user on a non-BC device gives a number that is
  false for this repository and for any consumer. It is a census, which
  CONVENTIONS says rots. *Fix:* Drop the count: "...most mobile GPUs do not,
  and compressed art and every font atlas need it - so a device without it has
  no text and, for most content, no art." *(Severity corrected by the
  skeptic.)*
- **low** · archaeology-comment · `engine/render/colour.cpp:106` — set_from_hex
  comment narrates former std::stoi implementation.
  This describes code that no longer exists rather than the current rule: the
  digits are validated before any are read, so a malformed string becomes
  opaque black without throwing. *Fix:* Replace it with one sentence stating
  the current rule. Drop the 'three copies' history in render_resources.cpp.
- **low** · archaeology-comment · `engine/render/d3d11/backend.h:23` —
  Include-wall comment refers to 'how the wall was climbed last time', and line
  274 refers to the replaced CommonStates.
  'last time' is history. The comment also says it states the rule rather than
  a list, yet two lines earlier it gives a count ('The three .cpp beside this
  file'). Line 274-275 ('This replaced DirectX::CommonStates, which offered
  twenty-odd and was asked for four') is the same kind of history. *Fix:* Keep
  the rule (no file outside this folder includes anything in it;
  check_engine_includes.cmake enforces it). Drop 'last time', the CommonStates
  sentence and the paragraph about how the comment is phrased.
- **low** · archaeology-comment · `engine/render/d3d11/backend.h:164` — `bound`
  field comment tells the story of a past leak, and renderer.cpp repeats it.
  The incident narrative and its numbers (sixteen views, 1,800 commands a
  second) describe an earlier begin_frame. The invariant is that a bound
  context must be finished or discarded, and set_view_count binds.
  renderer.cpp:855-863 tells the same history again ('that is the fix rather
  than an optimisation ... stranded two commands per idle view per frame'), so
  the backend carries two copies of it. *Fix:* Keep the invariant (a bound
  deferred context accumulates commands until finished, so only set_view_count
  binds and submit finishes every bound view) and delete the incident narrative
  from both places. *(Severity corrected by the skeptic.)*
- **low** · archaeology-comment ·
  `engine/render/d3d11/device_resources.cpp:526` — UpdateColorSpace comment
  talks about a removed c_EnableHDR flag, and its same-size caller is
  unreachable.
  c_EnableHDR no longer exists, so the first part is archaeology. The second
  part admits that the same-size UpdateColorSpace branch in WindowSizeChanged
  (lines 340-345) is unreachable, because Renderer::window_size_changed
  (renderer.cpp:757-762) makes the same comparison before calling.
  UpdateColorSpace is also public (device_resources.h:77), but `git grep
  UpdateColorSpace` finds only callers inside device_resources.cpp. *Fix:*
  State only the current behaviour (sRGB always, refresh a stale factory). Move
  UpdateColorSpace to private. Either remove the unreachable same-size branch
  or stop describing it.
- **low** · archaeology-comment · `engine/render/d3d11/device_resources.h:79` —
  Accessor block comment is a census of what was removed.
  A count of removed accessors is archaeology and is exactly the kind of method
  census SM-46 removed elsewhere. The 'TRIMMED' block at lines 30-50 has the
  same problem ('Three capabilities came from the upstream sample ... are gone
  rather than passed DXGI_FORMAT_UNKNOWN'). The no-depth-buffer point there is
  cited by sprite.hlsl:61 and sprite_vertex.h:24, so it is a rationale worth
  keeping, but it should be in the present tense. *Fix:* Rewrite lines 30-50 as
  present-tense rationale (no depth buffer because the renderer is 2D; no
  tearing or HDR flags; flip-model is a bool) and delete lines 81-86. A
  one-line statement that these are the only accessors is enough. *(Severity
  corrected by the skeptic.)*
- **low** · archaeology-comment · `engine/render/d3d11/render_resources.cpp:56`
  — RenderResources section comment tells the history of its own past
  restatements.
  'What stayed', 'now' and 'rather than being restated in three comments'
  describe earlier states of the file. CONVENTIONS calls a comment narrating
  what comments used to say 'lost the thread twice'. *Fix:* Reduce it to:
  'These need Impl complete or touch the texture table (render_resources.h).'
- **low** · convention · `engine/render/d3d11/renderer.cpp:70` — Unwrapped
  reflow lines in comments (also at 432, backend.h:213 and
  null/texture_factory.cpp:35).
  These comment lines run about 100-130 columns in files otherwise wrapped near
  78. They look like edit leftovers that the whitespace lint does not catch.
  renderer.cpp:432 also adds archaeology ('DirectXTK did exactly this'). The
  same overlong lines are at d3d11/backend.h:213 and
  null/texture_factory.cpp:35. *Fix:* Rewrap these lines to the files' width
  and drop the DirectXTK sentence.
- **low** · archaeology-comment · `engine/render/d3d11/renderer.cpp:278` —
  FinishCommandList comment cites ColourWars' former Level/MenuPage code.
  Level and MenuPage are game classes from before the split. They exist nowhere
  in this repo; grep of engine/ and samples/ finds no such types. The sentence
  is archaeology a reader cannot follow up. The rationale for FALSE is already
  in the first three lines. *Fix:* Delete the last two sentences and keep the
  rationale for FALSE. *(Severity corrected by the skeptic.)*
- **low** · stale-comment · `engine/render/d3d11/renderer.cpp:416` — Sampler
  and rasterizer comments are framed against removed DirectXTK CommonStates and
  SpriteBatch.
  The "state cache" and SpriteBatch no longer exist in the tree. Both comments
  justify the current code against what it replaced, so a reader needs the
  history to follow the rationale. Round 1 flagged only backend.h:274 for
  CommonStates. *Fix:* State the present rationale without the comparison, for
  example "Remade with the device in create_device_dependent_resources; nothing
  outside Impl holds one" and "No culling: a sprite quad's winding never
  changes, because a flip mirrors texcoords only."
- **low** · stale-comment · `engine/render/d3d11/renderer.cpp:476` — 'both
  samples' is stale; there are six samples (same text in the d3d12, gl and
  vulkan blend comments).
  AGENTS.md lists six samples. The count comes from an earlier tree and is
  copied in d3d12/sprite_pipeline.cpp:135, gl/renderer.cpp:399 and
  vulkan/renderer.cpp:437. renderer.cpp:61 has a similar stale framing ('A
  second backend writes its own') when five backends exist. *Fix:* Say 'every
  opaque sprite renders identically either way' with no count, and change all
  four backends together.
- **low** · archaeology-comment · `engine/render/d3d11/renderer.cpp:975` —
  submit() comment tells the history of the protocol that was once hand-written
  in four places.
  This is past-tense history of client code that has since moved out. Only the
  next sentence, about view order, is a contract. *Fix:* Reduce the paragraph
  to: "Record, finish, execute, release, in view order - the only ordering
  guarantee the seam makes."
- **low** · stale-comment · `engine/render/d3d11/renderer.cpp:1071` — The
  marker-guard comment tells the story of a past crash.
  AGENTS.md and CONVENTIONS say comments state contract or rationale, not
  history. This comment narrates a bug that has been fixed rather than stating
  why the guard exists. It is a separate location from the round-1 history
  findings. *Fix:* Reduce it to the rationale: the annotation object exists
  only with a device, and renderer.h makes markers legal before create_device,
  so these return early without a device.
- **low** · archaeology-comment · `engine/render/d3d11/texture_factory.cpp:73`
  — Device-null guard comment describes a past access violation.
  The rationale (ThrowIfFailed cannot catch a null-device dereference) is
  useful, but it is told as a past incident ('was an access violation'). The
  same pattern is in renderer.cpp:1071-1077 ('a marker before create_device
  dereferenced a null ComPtr here'). *Fix:* State it in the present tense:
  'GetD3DDevice() is null before create_device, and the calls below would
  dereference it before ThrowIfFailed could see an HRESULT.' Make the same
  change to the marker comment.
- **low** · archaeology-comment · `engine/render/d3d12/device_resources.cpp:62`
  — Destructor comment carries commit history and a destructor census.
  "From the same commit" and "arrived with the fifth backend" are history, and
  the four-destructor count is a census nothing checks. The useful content is
  one sentence: wait for the GPU before releasing objects it may still be
  reading. *Fix:* Reduce it to the contract: release must wait for queued work,
  the wait is non-throwing because the destructor is noexcept (T6), and
  ~Renderer::Impl does the same for the lists and pages it owns. *(Severity
  corrected by the skeptic.)*
- **low** · archaeology-comment ·
  `engine/render/d3d12/device_resources.cpp:325` — Resize-wait comment is
  written as a history of earlier bugs.
  Past-tense narration of earlier defects and earlier wording is archaeology
  under CONVENTIONS. The present-tense rule (recover only when
  GetDeviceRemovedReason fails, otherwise throw) is buried in it. *Fix:*
  Rewrite in the present tense: the wait must not throw, so device removal can
  reach handle_device_lost; a failed wait on a live device throws, because
  continuing would release a buffer in use. Drop the past-tense clauses at the
  other two sites.
- **low** · stale-comment · `engine/render/d3d12/device_resources.cpp:395` —
  Says pixel_tests.cpp calls end_frame exactly once; it calls it three times.
  This census of test code is false since the pixel-test reorganisation
  (SM-49), and nothing checks it. *Fix:* Drop the test census. Keep "flip-model
  presents discard the presented buffer, so read-back must precede end_frame".
  *(Severity corrected by the skeptic.)*
- **low** · archaeology-comment · `engine/render/d3d12/render_resources.cpp:70`
  — Slot-reuse rationale narrates history and repeats rationale held in three
  other places.
  "Is now written there" and "Without it a re-load took a fresh slot" describe
  how the code got here. The same slot-lifetime rationale also appears in
  backend.h:207-210, texture_factory.cpp:311-320 and render_resources.h:97-109,
  so the four copies can drift apart. *Fix:* Keep the contract at backend.h
  (texture_capacity / descriptor_slot) and the seam's precondition in
  render_resources.h. Cut this block and texture_factory.cpp's paragraph to
  one-line citations of those. *(Severity corrected by the skeptic.)*
- **low** · stale-comment · `engine/render/d3d12/renderer.cpp:316` —
  Allocator-reset comment calls the resize wait a full wait_for_gpu; it is the
  non-throwing try_wait_for_gpu.
  create_window_size_dependent_resources calls `this->try_wait_for_gpu()`
  (device_resources.cpp:348). That function returns false instead of throwing,
  and the resize then branches to handle_device_lost or ThrowIfFailed(E_FAIL).
  A reader checking the allocator-reset precondition follows the name to the
  throwing function and misses the branch where the wait did not complete.
  *Fix:* Name try_wait_for_gpu and say that a failed wait never reaches
  open_frame: it either recovers through handle_device_lost or throws.
- **low** · stale-comment · `engine/render/d3d12/renderer.cpp:776` — A second
  length-ranking claim about texture_factory.cpp is duplicated in renderer.cpp.
  This file-size ranking across backends repeats the claim in
  texture_factory.cpp:94 and vulkan/texture_factory.cpp:114. Every such copy
  has to be kept true by hand whenever any of the five factories changes
  length. The index-buffer upload explanation does not need it. *Fix:* Drop the
  ranking sentences and keep the point that the index buffer follows the same
  staging-copy-wait steps as texture uploads.
- **low** · duplication · `engine/render/gl/backend.h:102` — The same comment
  is repeated on the class and its first member.
  The repeated sentence adds nothing and can drift. *Fix:* Keep the class-level
  comment and drop the repeat on add_texture.
- **low** · archaeology-comment · `engine/render/gl/gl_functions.h:141` —
  Comment explains a function by the comment it replaced.
  'a comment kept getting wrong' is the history CONVENTIONS forbids ('a
  sentence narrating what this very comment used to say'). Line 32 already
  states the point without history. *Fix:* Reduce to 'How many entry points the
  list above declares, counted from the list (T5).'
- **low** · stale-comment · `engine/render/gl/renderer.cpp:271` — Section
  comment's list of context-defined members is incomplete.
  Both context files also define tell_host_lost, tell_host_restored and
  device_string (wgl_context.cpp l.232-246, web_context.cpp l.194-247;
  backend.h l.214 and l.291-292 say 'Defined by the context'). A reader looking
  for those in renderer.cpp is told they are not context members. *Fix:* Either
  add the three missing members or replace the enumeration with 'every member
  backend.h marks "Defined by the context"'. *(Severity corrected by the
  skeptic.)*
- **low** · stale-comment · `engine/render/gl/renderer.cpp:325` — Comment
  points to the wrong place for the run-length cap.
  DrawList::View::draw (renderer.cpp:180-184) only forwards to record(). The
  cap `this->open.sprites < max_run_sprites` is in RunRecorder::record in
  engine/render/sprite_geometry.h:293, so the comment sends a reader to the
  wrong function. *Fix:* Cite RunRecorder::record
  (engine/render/sprite_geometry.h) instead.
- **low** · archaeology-comment · `engine/render/gl/renderer.cpp:374` — Sampler
  comment narrates the backends' past disagreement on mip levels.
  Chronology of a prior D3D11 behaviour; the current fact is only that these
  are the non-mipmap filters so level zero is sampled. texture_factory.cpp
  l.177 ('belt and braces since the seam settled on level zero') carries the
  same history. *Fix:* Keep 'these are the non-mipmap min filters, which is
  this backend's share of the seam's level-zero term (d3d11/renderer.cpp argues
  it)' and drop the 'has always / until' narrative here and in
  texture_factory.cpp.
- **low** · archaeology-comment · `engine/render/gl/renderer.cpp:419` — Comment
  records how the backend first failed.
  The rationale (core profile rejects GL_TEXTURE_2D, errors queue) stands
  alone; the last sentence is history, which CONVENTIONS says is never a
  comment. *Fix:* Delete 'That is exactly how this backend first failed.'
- **low** · stale-comment · `engine/render/null/render_resources.cpp:42` — 'a
  third copy' of forwarding calls dates from when there were fewer backends.
  There are five backends, so a per-backend forwarding page would have five
  copies, not three. The sentence argues against a design that no longer exists
  and contradicts 'the other four' in the line before it. The paragraph is also
  mostly argument against a rejected alternative, which render_resources.h
  already owns. *Fix:* Replace it with a one-line pointer: 'The same short list
  as every backend; render_resources.h states the rule.' *(Severity corrected
  by the skeptic.)*
- **low** · stale-comment · `engine/render/null/renderer.cpp:31` — Claims a
  sentence appears 'in all four folders', but GL and Vulkan do not have it.
  `git grep -n 'ONE TEXTURE PER\|one texture per' engine/render` finds the
  sentence only in d3d11/renderer.cpp:134 and d3d12/renderer.cpp:187, not in
  gl/ or vulkan/. Line 34 is also an unwrapped reflow, about 100 columns,
  against the file's roughly 78-column wrap. *Fix:* Delete 'which is the same
  sentence in all four folders' and rewrap the paragraph.
- **low** · archaeology-comment · `engine/render/null/renderer.cpp:226` —
  Resize comment tells the story of a past bug under this backend.
  'got std::logic_error ... under this backend' is the history of a fixed bug.
  The current fact is that reset clears `touched`, so a relayout after a resize
  may lower the view count. null/backend.h:112-116 has the same kind of story
  ('This backend answered that correctly by accident'). *Fix:* State the
  invariant: 'reset() clears touched, so a shell that re-lays out after this
  returns true may lower the view count.' Delete the 'by accident' clause in
  backend.h.
- **low** · archaeology-comment · `engine/render/render_resources.cpp:97` — The
  text-query comment refers to the three backend copies that no longer exist.
  'Having three copies' describes a past state in which each backend had its
  own copy. A reader of the one current copy gains nothing from it. *Fix:* Keep
  only the present-tense rationale (measure_text is a term of the pixel
  contract and is computed once from Font). Re-wrap lines 17-22.
- **low** · archaeology-comment · `engine/render/resolution_manager.cpp:55` —
  Setter comment narrates history ('deliberately unchanged', 'has always
  meant', 'change of heart').
  The current contract (it coerces unknown sizes to 1280x720; use
  set_resolution_exactly for a live size) is already in the header. Framing it
  as 'unchanged' and 'always' is history. *Fix:* Cut to a present-tense line,
  e.g. "Coerces to a preset, 1280x720 for an unknown size;
  set_resolution_exactly is the non-coercing path."
- **low** · archaeology-comment · `engine/render/sprite_frame.h:30` — 'Not a
  RECT*' comments argue against a DirectXTK-era signature that is long gone.
  No RECT exists anywhere above the backends, so the comment argues with code
  that no longer exists. 'The conversion happens once, in the backend' no
  longer describes anything either: backends receive SpriteVertex corners, not
  a rectangle. *Fix:* Delete both sentences, or replace them with the actual
  contract: the returned reference lives as long as the frame or strip.
- **low** · stale-comment · `engine/render/throw_if_failed.h:11` — Includer
  census and 'compiles in every configuration' are both false.
  These are fragile census claims, and both are now wrong. The second
  overstates test coverage of the exception contract. *Fix:* Drop the
  per-backend count and say the test compiles in every Windows configuration,
  not in browser builds.
- **low** · dead-code · `engine/render/viewport.cpp:38` — Commented-out
  function left in source.
  Leftover code with no declaration in viewport.h. It is noise to every reader
  of the file. *Fix:* Delete the four lines.
- **low** · stale-comment · `engine/render/viewport_manager.cpp:156` —
  Viewport-manager comments name calls and access that do not exist.
  Three comments in a 183-line file describe behaviour that does not exist. The
  'asking for *4*' note is left over from a fixed defect
  (tests/render/viewport_manager_tests.cpp:79 records it). That makes it
  archaeology dressed as a warning. *Fix:* Change the note to say
  player_viewport(4) answers fullscreen. Reword the set_viewport sentence to
  say callers use these Viewports in index order. Drop the 'test can drive'
  rationale, or make calculate_viewport static and say only that it reads no
  member.
- **low** · archaeology-comment ·
  `engine/render/vulkan/device_resources.cpp:124` — Validation callback comment
  frames itself by backend authoring order.
  "Fifth" and "fourth was" refer to the order the backends were written, not to
  anything in the code. *Fix:* Lead with the present-tense point: Vulkan's
  validation layers report through a callback and check every barrier, layout
  and descriptor, so this forwards them to the debugger.
- **low** · archaeology-comment ·
  `engine/render/vulkan/device_resources.cpp:819` — create_render_pass comment
  records what the code had first.
  "what this had first" is history. The weighed alternative (a clearing render
  pass) is a legitimate rationale without it. *Fix:* Drop "and what this had
  first".
- **low** · stale-comment · `engine/render/vulkan/device_resources.cpp:2207` —
  'THREE OTHER SITES ... in the same words' undercounts the folder's
  put-it-back catch sites.
  The census is already stale; the index-buffer staging in renderer.cpp:566-612
  has exactly this shape. The sites are not worded the same either. *Fix:*
  Remove the count and list: "VulkanBuffer and raw handles have no destructor,
  so every throwing path releases them by hand."
- **low** · stale-comment · `engine/render/vulkan/device_resources.h:22` — The
  comment cites renderer.h for a decision that SEAM.md §4 owns.
  The cross-reference sends a reader to the caller-contract header, which does
  not contain the decision. AGENTS.md says SEAM.md holds the rationale and
  renderer.h holds the caller contract. *Fix:* Change the citation to
  engine/render/SEAM.md#4.
- **low** · stale-comment · `engine/render/vulkan/device_resources.h:207` —
  frame_count declaration carries a verification procedure and evidence caveats
  rather than a contract.
  Test coverage, how to run validation and evidence limits are none of
  CONVENTIONS' contract or rationale categories at a constant. They belong in
  docs/review/vulkan or docs/verification.md, and here they will go stale
  against the tests. *Fix:* Keep the rationale (two frames, not the swapchain
  image count). Move the testing and validation guidance to
  docs/review/vulkan/README.md and cite it.
- **low** · archaeology-comment · `engine/render/vulkan/renderer.cpp:584` —
  Index-upload barrier comment says 'The asymmetry was the finding'.
  "The asymmetry was the finding" refers to a review event, not the code. The
  rationale before it stands on its own. *Fix:* Delete the final sentence; keep
  the reason that a host wait is not an availability operation.
- **low** · stale-comment · `engine/render/vulkan/renderer.cpp:1388` — Marker
  comment says 'the other three backends that say it'; only D3D12 and GL do.
  The count is wrong; the same comment later admits D3D11 has working markers.
  *Fix:* Say 'the D3D12 and GL backends', or drop the count.
- **low** · stale-comment · `engine/render/vulkan/texture_factory.cpp:64` —
  Comment says texture_format.h counts b4g4r4a4 uses; it does not.
  The citation points at a document that does not carry the claim, so a reader
  cannot check it. *Fix:* Drop the parenthesis, or cite texture_format.h for
  what it actually says (present because MakeSpriteFont offers it). *(Severity
  corrected by the skeptic.)*
