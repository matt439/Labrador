find_package(doctest CONFIG REQUIRED)

# The whole game, with no engine in the binary.
#
# NOTE WHAT IS NOT LINKED: LabradorEngine. This target has LineSweeperRules,
# doctest and the settings interface, so there is no window to open, no device
# to create and no renderer to fail. That is the property the rules library was
# made a library for, and it is what makes a replay test - four thousand ticks
# of scripted input, compared with std::memcmp - a test that can run on a build
# agent with no display attached.
add_executable(LineSweeperTests
    replay_tests.cpp
    scoring_tests.cpp
    test_main.cpp
    tick_tests.cpp
    world_tests.cpp
)
target_link_libraries(LineSweeperTests PRIVATE
    LineSweeperRules
    doctest::doctest
    labrador_settings
)

# The tests include their own fixtures header from the repository root, the
# same way everything else does.
target_include_directories(LineSweeperTests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../.."
)

add_test(NAME LineSweeperTests COMMAND LineSweeperTests)

# --- LineSweeperViewTests - the presentation, with no DEVICE ---
#
# TWO BINARIES BECAUSE THE SAMPLE HAS TWO WALLS, and they are different walls.
# The target above links no engine, which is what proves the game is playable
# with nothing underneath it. This one links the engine and still creates no
# window, no adapter and no device, which is the other half of the same
# discipline: PHILOSOPHY, Tests and toolchain, asks that everything below the
# platform edge be testable headlessly, and a sample is below it too.
#
# It is one file, and presentation/particles.cpp is the only thing in it. The
# board view needs the resource table to measure text and measuring walks an
# atlas a device filled, so a BoardView cannot be constructed here and what it
# draws is checked by looking at it. The particle field takes a resolved
# texture handle instead - an integer nothing in update() reads - so the ten
# thousand particles, the compaction and the event reconstruction all run.
#
# The sample's own sources are compiled into this target rather than linked,
# because LineSweeperSample is a WIN32 executable and presentation/ is not a
# library. One file is cheaper to list than a third library is to justify; if
# a second presentation file ever becomes testable, that is the moment to
# revisit it.
add_executable(LineSweeperViewTests
    test_main.cpp
    view_tests.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../../samples/linesweeper/presentation/particles.cpp"
)
target_link_libraries(LineSweeperViewTests PRIVATE
    LineSweeperRules
    LabradorEngine
    doctest::doctest
    labrador_settings
)
target_include_directories(LineSweeperViewTests PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../.."
)

add_test(NAME LineSweeperViewTests COMMAND LineSweeperViewTests)
