find_package(doctest CONFIG REQUIRED)

# The shell's tests. Two files run on every platform. The third is the
# platform's own window: on Windows a real one, hidden and driven through its
# message procedure, and in a browser build the translation of DOM events,
# which is the part of the web window Node can reach with no DOM.
add_executable(AppTests
    application_options_tests.cpp
    content_root_tests.cpp
    test_main.cpp
)
if(EMSCRIPTEN)
    target_sources(AppTests PRIVATE dom_events_tests.cpp)
else()
    target_sources(AppTests PRIVATE window_tests.cpp)
endif()
target_link_libraries(AppTests PRIVATE
    LabradorEngine
    doctest::doctest
    labrador_settings
)

add_test(NAME AppTests COMMAND AppTests)
