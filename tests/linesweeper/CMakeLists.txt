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
