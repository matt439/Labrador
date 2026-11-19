# Held touch controls

`TouchButton` and `TouchStick` are independent values a game copies. Each owns
one contact id, takes positions supplied by the page, and reads no input
device. The page chooses geometry, contact acceptance, overlap precedence and
what the values do. Neither component draws or chooses a game action.

The existing ComponentsGallery has a separate **Touch controls (F2)** page.
Its stick moves a marker, A colours it and B makes it larger. Several fingers
can combine these actions. A left mouse button previews one control. Escape,
gamepad B or the page's Back button returns to the original gallery; the
settings rows and their focus order keep their existing controls.

## Copy the values and their tests

These files were added after the v0.3 starter. Obtain them from a current
development source checkout first. Keeping your game's v0.3 engine is fine:
the two values use its existing math API. Set `LABRADOR_TOUCH_SOURCE` to the
development checkout's `samples\components` folder; if your starter's own
`external\labrador` already contains the files, use
`external\labrador\samples\components` instead. This source folder supplies
copies; it does not change which engine your game links.

From the starter project's folder in the x64 Native Tools Command Prompt,
replace the example source path with that folder:

```bat
set "LABRADOR_TOUCH_SOURCE=C:\dev\labrador-dev\samples\components"
mkdir game\touch
copy "%LABRADOR_TOUCH_SOURCE%\touch_button.h" game\touch\
copy "%LABRADOR_TOUCH_SOURCE%\touch_button.cpp" game\touch\
copy "%LABRADOR_TOUCH_SOURCE%\touch_stick.h" game\touch\
copy "%LABRADOR_TOUCH_SOURCE%\touch_stick.cpp" game\touch\
copy "%LABRADOR_TOUCH_SOURCE%\touch_button_tests.cpp" game\touch\
copy "%LABRADOR_TOUCH_SOURCE%\touch_stick_tests.cpp" game\touch\
copy "%LABRADOR_TOUCH_SOURCE%\test_main.cpp" game\touch\main.cpp
```

Change each copied `.cpp`'s component include from
`samples/components/touch_button.h` or `samples/components/touch_stick.h` to
`game/touch/touch_button.h` or `game/touch/touch_stick.h`, including the tests.
The headers depend only on the engine's math headers. The two values do not
depend on each other, PointerCapture, the Gallery or the pure pointer query.
Keep either pair on its own if that is all your game needs. The namespace
`components` can be renamed in your copies.

In `game/CMakeLists.txt`, add `touch/touch_button.cpp` and
`touch/touch_stick.cpp` to both the native and browser `add_executable(MyGame ...)`
source lists. Paths there are relative to `game/`, so omit the `game/` prefix.
Add this separate test target at the end of the project's
root `CMakeLists.txt`, after `add_subdirectory(game)`, using the copied
`main.cpp` for doctest's entry point:

```cmake
find_package(doctest CONFIG REQUIRED)
add_executable(MyGameTouchTests
    game/touch/main.cpp
    game/touch/touch_button.cpp
    game/touch/touch_stick.cpp
    game/touch/touch_button_tests.cpp
    game/touch/touch_stick_tests.cpp
)
target_include_directories(MyGameTouchTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(MyGameTouchTests PRIVATE
    LabradorEngine doctest::doctest labrador_settings)
add_test(NAME MyGameTouchTests COMMAND MyGameTouchTests)
set_tests_properties(MyGameTouchTests PROPERTIES TIMEOUT 60)
```

Use the existing engine target name `LabradorEngine`; the components are no
new library dependency. Run:

```bat
cmake --preset x64-debug
cmake --build --preset x64-debug --target MyGameTouchTests
ctest --preset x64-debug -R "^MyGameTouchTests$" --output-on-failure
```

The target has fourteen cases and needs no window, graphics device, audio,
asset files or real contact. Copying only one value also means dropping the
other value's source and test from this target. A missing include means one
of the copied paths still points to the sample. An unresolved component
method usually means its `.cpp` was omitted from that target's list. These
tests can also build under the browser preset and run in Node.

## Let the page route contacts

Read `TouchDemoState::update_touch()` in `touch_demo_state.cpp` for the
sample's complete routing loop. Each update does these in order:

1. For every owned id, find that id in `touch.state()`. Pass its converted
   position to `move()` if it remains active. Call `end()` for
   `touch.released(id)`; otherwise call `cancel()` because it disappeared
   without a release. Only a normal Back release completes the Back action.
   It uses the last sampled position: Touch retains no final-up position,
   so an outside move and lift entirely between polls cannot cancel that
   sampled inside hold. A sampled outside move does cancel completion.
2. Consider `begin()` only for points where `touch.pressed(point.id)` is true.
   Pass `TouchPoint::id`, which is the engine's stable nonzero id, rather than
   a raw platform id or the point's array position.
3. Choose at most one target for that fresh point. This page calls
   `hit_pointer_target()` over Back, B, A and Stick, in reverse draw order.
   A busy first hit rejects the new finger rather than falling through to
   another target. Those rectangles are disjoint here, but the order is
   explicit if a game later overlaps them.

The reusable components use the existing math and work with v0.3. The pure
`hit_pointer_target()` query used by this development Gallery is newer; a
game copying just the two values can choose its initial target by calling
their `begin()` methods in its own fixed precedence instead. Copying the
whole Gallery additionally requires the current engine API it includes.

Touch positions are client pixels, whereas the sample's regions are in a
1280 by 720 world. `refresh_view()` stores the same fitted Camera and Viewport
that `draw()` uses, and `world_point()` calls
`camera_.world_position(point.position, viewport_)` before passing a position
to either value. A split-screen page must first select the view it means;
the coordinate conversion itself does not decide which view owns a point.
The browser shell gives the game canvas `touch-action: none`, as LineSweeper
does, so dragging a control does not become browser pan/zoom and cancel all
its contacts. Keep that canvas rule when copying the page; it changes no
browser or device setting outside the game's canvas.

## Ownership, outside movement and cancellation

Each control accepts one fresh nonzero id while idle. Its activation
rectangle includes its left/top edges and excludes its right/bottom edges.
A second finger on a busy control is rejected. Ending or cancelling its
owner never adopts another finger that is already down; acceptance needs a
later `begin()` chosen by the page. Foreign ids do nothing.

A button reports `held()` while its owner is inside its rectangle. Sliding
outside clears held but keeps the owner, so sliding back in restores held.
A stick uses its fixed rectangle midpoint and half its shorter side as its
radius. It follows its owner outside the rectangle, clamps radially to a
vector of length at most one, and uses +y down. A game's thresholds, dead
zones, axes and bindings are its own policy; this sample applies the vector
directly to the marker. End and cancel return either control to neutral.

Regions require finite coordinates and endpoints with nonnegative extents;
empty geometry disables a control. Nonempty sticks also require a
representable positive radius and centre. Invalid geometry or an invalid
position for the owner throws `std::invalid_argument` before changing state.
Changing a region cancels its contact; assigning the identical region keeps
it. The headers state the details, and the copied tests cover them.

This page also cancels **all** values on focus/capture loss, suspension,
return, or a changed camera/viewport, and waits until all old contacts and
the left mouse button clear before accepting another press. The mouse's
button epoch retains a quick canvas blur/focus between polls. A cancelled
Back contact cannot become a click. Touch takes priority over the mouse;
mouse events made from touch are suppressed during touch and its following
tick. Another game may choose different policies explicitly.

## What is checked

The portable tests cover simultaneous independent values, stable distinct
ids, half-open edges, outside movement/re-entry, release/cancel without
adoption, changed geometry, invalid input, extreme finite coordinates and a
grid of radially bounded stick values. The native-null Gallery fixtures
drive the actual Application feeds, resize and focus notifications, mouse
takeover, return routes and repeated const drawing. The existing
`ComponentsPointerPageTests` additionally enters this page in Chromium and
drives multiple emulated contacts through its real pointer feed, plus
deliberate individual `pointercancel` and same-turn blur/focus events.

These are synthetic checks. Real-glass, pen and phone acceptance remain
open under [WEB#5](../../docs/port/web-to-be-tested.md) and
[SM-13](../../docs/product/STORIES.md#sm-13); this page makes no phone support
claim. The native sample's real touch feed still needs real hardware too.
