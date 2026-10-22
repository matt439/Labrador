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

# A real UiText, drawn, in the one configuration that can draw without a
# device. stub_widget.h says why focus and navigation stay on the stub in
# every configuration; text_widget_tests.cpp is the real-widget file it
# describes, compiled only here for the reason tests/render/CMakeLists.txt gives for
# null_tests.cpp.
if(LABRADOR_RENDER_BACKEND STREQUAL "null")
    target_sources(UiTests PRIVATE text_widget_tests.cpp)
endif()

add_test(NAME UiTests COMMAND UiTests)
