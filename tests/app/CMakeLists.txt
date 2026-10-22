find_package(doctest CONFIG REQUIRED)

# The shell's tests. Three files run on every platform. The fourth is the
# platform's own window: on Windows a real one, hidden and driven through its
# message procedure, and in a browser build the translation of DOM events,
# which is the part of the web window Node can reach with no DOM.
add_executable(AppTests
    application_options_tests.cpp
    content_root_tests.cpp
    frame_timer_tests.cpp
    test_main.cpp
)
if(EMSCRIPTEN)
    target_sources(AppTests PRIVATE dom_events_tests.cpp)
else()
    target_sources(AppTests PRIVATE window_tests.cpp)
endif()

# Application's own loop, run with a hidden window: only where the renderer
# and the audio are both null, so it needs no device and no audio endpoint,
# and never in a browser, where run() does not return.
if(NOT EMSCRIPTEN AND LABRADOR_RENDER_BACKEND STREQUAL "null"
    AND LABRADOR_AUDIO_BACKEND STREQUAL "null")
    target_sources(AppTests PRIVATE application_loop_tests.cpp)
endif()
target_link_libraries(AppTests PRIVATE
    LabradorEngine
    doctest::doctest
    labrador_settings
)

add_test(NAME AppTests COMMAND AppTests)

# WebWindowTests - the web window's own state, which AppTests cannot reach
# under Node: focus and blur, the capture analogue, which defaults the canvas
# takes, the size it measures before each frame, touch, visibility and full
# screen. A page (web/window_page.cpp) that takes its canvas with a real
# Window and writes down every WindowNotify call, and a script
# (web/window_input.mjs) that dispatches real input at it over the DevTools
# protocol and says what each must have produced. The render tests' runner
# runs it, with --input, and the same rule holds as for their pages: absent
# rather than skipped where configure found no Chromium (the root
# CMakeLists).
#
# Not built against any one render backend: a window draws nothing.
if(EMSCRIPTEN)
    add_executable(WebWindowTests web/window_page.cpp)
    target_link_libraries(WebWindowTests PRIVATE
        LabradorEngine
        labrador_settings
    )
    set_target_properties(WebWindowTests PROPERTIES SUFFIX ".html")
    # The directory's Node options are overridden, as RenderPixelTests'
    # are (tests/render/CMakeLists.txt). Without EXIT_RUNTIME, unlike
    # theirs: the script closes the window and then asks the page what it
    # heard afterwards, and a runtime that exited when the loop stopped
    # would end the run before the answer.
    target_link_options(WebWindowTests PRIVATE
        -sENVIRONMENT=web
        -sNODERAWFS=0
        "SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
    )
    set_property(TARGET WebWindowTests APPEND PROPERTY LINK_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
    )
    if(LABRADOR_TEST_BROWSER)
        add_test(NAME WebWindowTests
            COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR}
                "${PROJECT_SOURCE_DIR}/tests/render/web/run_page.mjs"
                --browser "${LABRADOR_TEST_BROWSER}"
                --page "$<TARGET_FILE:WebWindowTests>"
                --input "${CMAKE_CURRENT_SOURCE_DIR}/web/window_input.mjs"
                --timeout 60
        )
    endif()
endif()
