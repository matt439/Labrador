labrador_add_test_suite(UiTests
    SOURCES
        focus_tests.cpp
        navigation_tests.cpp
        widget_tests.cpp
    LINKS LabradorEngine
)

# A real UiText, drawn, in the one configuration that can draw without a
# device. stub_widget.h says why focus and navigation stay on the stub in
# every configuration; text_widget_tests.cpp is the real-widget file it
# describes, compiled only here for the reason tests/render/CMakeLists.txt gives for
# null_tests.cpp.
if(LABRADOR_RENDER_BACKEND STREQUAL "null")
    target_sources(UiTests PRIVATE text_widget_tests.cpp)
endif()
