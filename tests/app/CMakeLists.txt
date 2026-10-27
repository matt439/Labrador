# The shell's tests, including the pure DOM-event translator on every platform.
# Native builds compile that portable source into this test target only, while
# browser builds already link it from the engine. Windows also exercises a real
# hidden window through its message procedure. The production backend selection
# is unchanged, and the translator needs no browser or Emscripten API.
labrador_add_test_suite(AppTests
    SOURCES
        application_options_tests.cpp
        content_root_tests.cpp
        dom_events_tests.cpp
        frame_timer_tests.cpp
    LINKS LabradorEngine
)
if(NOT EMSCRIPTEN)
    target_sources(AppTests PRIVATE
        window_tests.cpp
        ../../engine/app/web/dom_events.cpp
    )
endif()

# Live Application tests, with a hidden window: only where the renderer and
# audio are both null, so they need no device and no audio endpoint. The loop
# case cannot run in a browser, where run() does not return; the resize case
# drives a native HWND and its real size notification.
if(NOT EMSCRIPTEN AND LABRADOR_RENDER_BACKEND STREQUAL "null"
    AND LABRADOR_AUDIO_BACKEND STREQUAL "null")
    target_sources(AppTests PRIVATE
        application_loop_tests.cpp
        application_resize_tests.cpp
    )
endif()

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
