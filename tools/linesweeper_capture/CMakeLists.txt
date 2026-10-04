# LineSweeperCapture - exact PNGs of the LineSweeper sample, for the website.
#
# Not a ctest entry: it needs a device, and what it produces is images for a
# person to look at rather than a pass or a fail. README.md beside this file
# says how to run it.
#
# NOT BUILT UNDER THE NULL BACKEND AT ALL, rather than built to refuse at
# startup. Its whole output is Renderer::read_back_buffer, which the null
# backend refuses by design (renderer.h), so that binary could only ever print
# an error. The condition is one line and the absence is visible - there is no
# target to run - which is the precedent RenderPixelTests sets in
# tests/render/CMakeLists.txt for the same call.
if(NOT LABRADOR_RENDER_BACKEND STREQUAL "null")
    # The sample's presentation and its pause screen are compiled in rather
    # than linked, as the frame benchmark does with the first three:
    # LineSweeperSample is a WIN32 executable and presentation/ is not a
    # library. The rules are a library, and are linked.
    add_executable(LineSweeperCapture
        capture_script.cpp
        main.cpp
        png_file.cpp
        scripted_match.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../../samples/linesweeper/presentation/board_view.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../samples/linesweeper/presentation/particles.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../samples/linesweeper/presentation/top_out_banner.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../samples/linesweeper/states/pause_state.cpp"
    )
    target_link_libraries(LineSweeperCapture PRIVATE
        LineSweeperRules
        LabradorEngine
        labrador_settings
        # The in-box PNG codec, as RenderPixelTests links it for its golden set.
        windowscodecs
    )

    # The checked-in script, addressed where it lives rather than copied
    # beside the executable, so an edit to it is the next run's script with no
    # build in between. A different script can still be named on the command
    # line.
    target_compile_definitions(LineSweeperCapture PRIVATE
        LINESWEEPER_CAPTURE_SCRIPT="${CMAKE_CURRENT_SOURCE_DIR}/website.txt"
    )

    # The sample's own manifest and content, beside the executable, because
    # Application::load_manifest resolves a relative path against the
    # executable and the captures must load exactly what the sample loads. Run
    # even when nothing relinks, so a content change reaches the next capture.
    add_custom_target(linesweeper_capture_content ALL)
    add_custom_command(TARGET linesweeper_capture_content POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/../../samples/linesweeper/content/manifest.json"
            "$<TARGET_FILE_DIR:LineSweeperCapture>/manifest.json"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/../../samples/linesweeper/content/fonts"
            "$<TARGET_FILE_DIR:LineSweeperCapture>/fonts"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/../../samples/linesweeper/content/textures"
            "$<TARGET_FILE_DIR:LineSweeperCapture>/textures"
        VERBATIM
    )
    add_dependencies(LineSweeperCapture linesweeper_capture_content)
endif()
