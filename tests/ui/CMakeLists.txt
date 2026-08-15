find_package(doctest CONFIG REQUIRED)

add_executable(UiTests
    focus_tests.cpp
    navigation_tests.cpp
    widget_tests.cpp
    test_main.cpp
)
target_link_libraries(UiTests PRIVATE
    LabradorEngine
    doctest::doctest
    labrador_settings
)

add_test(NAME UiTests COMMAND UiTests)
