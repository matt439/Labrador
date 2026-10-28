# The whole game, with no engine in the binary.
#
# NOTE WHAT IS NOT LINKED: LabradorEngine. This target has LineSweeperRules,
# doctest and the settings interface, so there is no window to open, no device
# to create and no renderer to fail. That is the property the rules library was
# made a library for, and it is what makes a replay test - four thousand ticks
# of scripted input, compared with std::memcmp - a test that can run on a build
# agent with no display attached.
labrador_add_test_suite(LineSweeperTests
    SOURCES
        replay_tests.cpp
        resume_gate_tests.cpp
        scoring_tests.cpp
        tick_tests.cpp
        world_tests.cpp
    LINKS LineSweeperRules
)

# The tests include their own fixtures header from the repository root, the
# same way everything else does.
target_include_directories(LineSweeperTests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../.."
)

# --- LineSweeperViewTests - the presentation, with no DEVICE ---
#
# TWO BINARIES BECAUSE THE SAMPLE HAS TWO WALLS, and they are different walls.
# The target above links no engine, which is what proves the game is playable
# with nothing underneath it. This one links the engine; its portable cases
# create no window, adapter or device. Native null configurations also exercise
# the states with a hidden window, as specified below. PHILOSOPHY, Tests and
# toolchain, asks that code below the platform edge be testable headlessly.
#
# Three presentation files are in it everywhere. The board view needs the
# resource table to measure text, and measuring walks a loaded font atlas, so a
# BoardView is constructed only where the null renderer can load one (below).
# The particle field takes a resolved texture handle instead - an integer
# nothing in update() reads - so the ten thousand particles, the compaction and
# the event reconstruction all run. The screen's arrangement and the touch
# controls' hit test are arithmetic on a buffer size, and run as well; the
# TouchControls object beside them is constructed by the native-null state
# cases, where the sample font can be loaded without a graphics device.
#
# The sample's own sources are compiled into this target rather than linked,
# because LineSweeperSample is a WIN32 executable and presentation/ is not a
# library. Listing these sources is cheaper than another library to justify.
# The browser pacing fixture also runs here against the real particle field,
# without constructing its device-bound BoardView or application state.
labrador_add_test_suite(LineSweeperViewTests
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/../../bench/frame_statistics.cpp"
        frame_statistics_tests.cpp
        pacing_fixture_tests.cpp
        palette_tests.cpp
        touch_controls_tests.cpp
        view_tests.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../../samples/linesweeper/presentation/arrangement.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../samples/linesweeper/presentation/particles.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../samples/linesweeper/presentation/touch_controls.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../samples/linesweeper/web/pacing_fixture.cpp"
    LINKS LineSweeperRules LabradorEngine
)
target_include_directories(LineSweeperViewTests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../.."
)
target_include_directories(LineSweeperViewTests SYSTEM PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../external/rapidjson/include"
)

# The frame bench's pacer and result file, which are Win32 code and are
# tested where the bench builds (bench/CMakeLists.txt).
if(WIN32)
    target_sources(LineSweeperViewTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../../bench/frame_pacer.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../bench/linesweeper_frame_result.cpp"
        frame_pacer_tests.cpp
        frame_result_tests.cpp
    )
endif()

# The native null backends also exercise the real play/pause states against
# synthetic input and activation. A hidden window supplies Application's
# services; no graphics device, audio endpoint or application loop is used.
if(WIN32 AND LABRADOR_RENDER_BACKEND STREQUAL "null" AND
        LABRADOR_AUDIO_BACKEND STREQUAL "null")
    target_sources(LineSweeperViewTests PRIVATE
        focus_pause_tests.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../../samples/linesweeper/presentation/top_out_banner.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../samples/linesweeper/states/play_state.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../samples/linesweeper/states/pause_state.cpp"
    )
    target_compile_definitions(LineSweeperViewTests PRIVATE
        LINESWEEPER_CONTENT_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/../../samples/linesweeper/content/"
    )
endif()

# Under the null renderer the field's draws and the whole board are recorded
# rather than rasterised. The board loads the sample's own font from the source
# tree, which a browser build's Node test reads through NODERAWFS.
if(LABRADOR_RENDER_BACKEND STREQUAL "null")
    target_sources(LineSweeperViewTests PRIVATE
        board_view_tests.cpp
        particle_draw_tests.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../../samples/linesweeper/presentation/board_view.cpp"
    )
    target_compile_definitions(LineSweeperViewTests PRIVATE
        LINESWEEPER_FONT_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/../../samples/linesweeper/content/fonts/"
    )
endif()
