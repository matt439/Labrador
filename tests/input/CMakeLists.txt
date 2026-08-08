find_package(doctest CONFIG REQUIRED)

add_executable(InputTests
    deadzone_tests.cpp
    gamepad_tests.cpp
    test_main.cpp
)
target_link_libraries(InputTests PRIVATE
    ArtAttackEngine
    doctest::doctest
    artattack_settings
)

add_test(NAME InputTests COMMAND InputTests)
