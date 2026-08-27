find_package(doctest CONFIG REQUIRED)

add_executable(InputTests
    deadzone_tests.cpp
    direction_tests.cpp
    gamepad_tests.cpp
    keyboard_tests.cpp
    mouse_tests.cpp
    test_main.cpp
)
target_link_libraries(InputTests PRIVATE
    LabradorEngine
    doctest::doctest
    labrador_settings
)

add_test(NAME InputTests COMMAND InputTests)
