# One sample and two entry points: main.cpp is wWinMain, with the smoke test
# and the capture, and main_web.cpp is a browser's main. The arena, its bodies
# and the state are the same files, and CollisionSampleTests links the arena
# in every build.
add_library(CollisionExample STATIC body.cpp arena.cpp)
target_link_libraries(CollisionExample PUBLIC LabradorEngine labrador_settings)

if(EMSCRIPTEN)
    add_executable(CollisionSample main_web.cpp collision_state.cpp)
    target_link_libraries(CollisionSample PRIVATE CollisionExample labrador_settings)

    # A page, its loader and the module, as LineSweeper's are. The content is
    # the three things the Windows build copies beside its executable,
    # embedded at the same relative paths, so the one manifest serves both.
    set_target_properties(CollisionSample PROPERTIES SUFFIX ".html")
    target_link_options(CollisionSample PRIVATE
        "SHELL:--embed-file ${CMAKE_CURRENT_SOURCE_DIR}/content/manifest.json@/manifest.json"
        "SHELL:--embed-file ${CMAKE_CURRENT_SOURCE_DIR}/../minimal/content/fonts@/fonts"
        "SHELL:--embed-file ${CMAKE_CURRENT_SOURCE_DIR}/../linesweeper/content/textures@/textures"
        "SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
        -sENVIRONMENT=web
    )
    # Neither the page nor the content is a source, so a change to either
    # relinks only because it is named here.
    set_property(TARGET CollisionSample APPEND PROPERTY LINK_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
        "${CMAKE_CURRENT_SOURCE_DIR}/content/manifest.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/../minimal/content/fonts/jetbrains_mono_bold_16.spritefont"
        "${CMAKE_CURRENT_SOURCE_DIR}/../linesweeper/content/textures/white.dds"
    )
elseif(WIN32)
    add_executable(CollisionSample WIN32 main.cpp collision_state.cpp)
    target_link_libraries(CollisionSample PRIVATE CollisionExample labrador_settings)
    # The capture writes its PNG through WIC, which is Windows', and reads back
    # a device, which the null renderer does not have.
    if(NOT LABRADOR_RENDER_BACKEND STREQUAL "null")
        target_sources(CollisionSample PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../../tools/linesweeper_capture/png_file.cpp")
        target_compile_definitions(CollisionSample PRIVATE COLLISION_SAMPLE_CAPTURE)
        target_link_libraries(CollisionSample PRIVATE windowscodecs)
    endif()

    # Reuse the existing font and white texel; the output is self-contained.
    add_custom_target(collision_content ALL
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/content/manifest.json"
            "$<TARGET_FILE_DIR:CollisionSample>/manifest.json"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/../minimal/content/fonts"
            "$<TARGET_FILE_DIR:CollisionSample>/fonts"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/../linesweeper/content/textures"
            "$<TARGET_FILE_DIR:CollisionSample>/textures"
        VERBATIM
    )
    add_dependencies(CollisionSample collision_content)
endif()

find_package(doctest CONFIG REQUIRED)
add_executable(CollisionSampleTests arena_tests.cpp)
target_link_libraries(CollisionSampleTests PRIVATE
    CollisionExample doctest::doctest labrador_settings)
# Under Node, as every other browser-build test runs. The root's link options
# for that are set after this folder is added, because the page above must not
# take them, so the test names the one it needs: it reads no files.
if(EMSCRIPTEN)
    target_link_options(CollisionSampleTests PRIVATE -sENVIRONMENT=node)
endif()
add_test(NAME CollisionSampleTests COMMAND CollisionSampleTests)
