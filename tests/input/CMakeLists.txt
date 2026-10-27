labrador_add_test_suite(InputTests
    SOURCES
        deadzone_tests.cpp
        direction_tests.cpp
        gamepad_tests.cpp
        keyboard_tests.cpp
        mouse_tests.cpp
        touch_tests.cpp
    LINKS LabradorEngine
)

# The browser pad's translation, which is the part of engine/input/web/ Node
# can reach with no Gamepad API. Compiled only where that backend is.
if(LABRADOR_INPUT_BACKEND STREQUAL "web")
    target_sources(InputTests PRIVATE standard_gamepad_tests.cpp)
endif()
