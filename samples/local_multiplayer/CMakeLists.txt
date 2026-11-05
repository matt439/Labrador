# One sample and two entry points: main.cpp is wWinMain, with the smoke test
# and the capture, and main_web.cpp is a browser's main. Everything else is
# the same files, in a library so that the sample and LocalMultiplayerTests
# (tests/local_multiplayer/) compile them once and cannot list them apart.
# The tests reach the arena, controls and pure join rules in every build;
# native null adds the real menu routing cases.
add_library(LocalMultiplayerExample STATIC
    arena.cpp
    controls.cpp
    join_rules.cpp
    join_state.cpp
    play_state.cpp
)
target_link_libraries(LocalMultiplayerExample PUBLIC LabradorEngine labrador_settings)

if(EMSCRIPTEN)
    add_executable(LocalMultiplayerSample main_web.cpp)
    target_link_libraries(LocalMultiplayerSample PRIVATE LocalMultiplayerExample labrador_settings)

    # A page, its loader and the module, as LineSweeper's are. The content is
    # the two things the Windows build copies beside its executable,
    # embedded at the same relative paths, so the one manifest serves both.
    set_target_properties(LocalMultiplayerSample PROPERTIES SUFFIX ".html")
    target_link_options(LocalMultiplayerSample PRIVATE
        "SHELL:--embed-file ${CMAKE_CURRENT_SOURCE_DIR}/content/manifest.json@/manifest.json"
        "SHELL:--embed-file ${CMAKE_CURRENT_SOURCE_DIR}/../minimal/content/fonts@/fonts"
        "SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
        -sENVIRONMENT=web
    )
    # Neither the page nor the content is a source, so a change to either
    # relinks only because it is named here.
    set_property(TARGET LocalMultiplayerSample APPEND PROPERTY LINK_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
        "${CMAKE_CURRENT_SOURCE_DIR}/content/manifest.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/../minimal/content/fonts/jetbrains_mono_bold_16.spritefont"
    )
elseif(WIN32)
    # The template's DPI awareness declaration (samples/minimal/dpi_aware.manifest).
    add_executable(LocalMultiplayerSample WIN32
        main.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../minimal/dpi_aware.manifest"
    )
    target_link_libraries(LocalMultiplayerSample PRIVATE LocalMultiplayerExample labrador_settings shell32)
    if(NOT LABRADOR_RENDER_BACKEND STREQUAL "null")
        target_sources(LocalMultiplayerSample PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../../tools/linesweeper_capture/png_file.cpp")
        target_compile_definitions(LocalMultiplayerSample PRIVATE LOCAL_MULTIPLAYER_CAPTURE)
        target_link_libraries(LocalMultiplayerSample PRIVATE windowscodecs)
    endif()

    # Reuse the minimal sample's font; the output is self-contained. The white
    # texel the arena is drawn from is the engine's own (white_texture_name).
    add_custom_target(local_multiplayer_content ALL)
    add_custom_command(TARGET local_multiplayer_content POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/content/manifest.json"
            "$<TARGET_FILE_DIR:LocalMultiplayerSample>/manifest.json"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/../minimal/content/fonts"
            "$<TARGET_FILE_DIR:LocalMultiplayerSample>/fonts"
        VERBATIM
    )
    add_dependencies(LocalMultiplayerSample local_multiplayer_content)

    # The smoke test under null render, which records the draws with no
    # device: the scripted players and both views, checked and out (main.cpp,
    # PlayState::verify_smoke_test). A rasterising preset's run, and its
    # capture, are by hand (README.md, Verification).
    if(LABRADOR_RENDER_BACKEND STREQUAL "null")
        add_test(NAME LocalMultiplayerSmokeTests COMMAND LocalMultiplayerSample --smoke-test)
        set_tests_properties(LocalMultiplayerSmokeTests PROPERTIES TIMEOUT 60)
    endif()
endif()
