find_package(doctest CONFIG REQUIRED)

add_executable(InputTests
    deadzone_tests.cpp
    direction_tests.cpp
    gamepad_tests.cpp
    keyboard_tests.cpp
    mouse_tests.cpp
    test_main.cpp
)

# The browser pad's translation, which is the part of engine/input/web/ Node
# can reach with no Gamepad API. Compiled only where that backend is.
if(LABRADOR_INPUT_BACKEND STREQUAL "web")
    target_sources(InputTests PRIVATE standard_gamepad_tests.cpp)
endif()
target_link_libraries(InputTests PRIVATE
    LabradorEngine
    doctest::doctest
    labrador_settings
)

add_test(NAME InputTests COMMAND InputTests)
