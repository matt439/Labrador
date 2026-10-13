# One sample and two entry points: main.cpp is wWinMain, with the smoke test
# and the capture, and main_web.cpp is a browser's main. Everything else is
# the same files, and LocalMultiplayerTests links arena.cpp and controls.cpp in
# every build.
set(local_multiplayer_sources
    arena.cpp
    controls.cpp
    play_state.cpp
)

if(EMSCRIPTEN)
    add_executable(LocalMultiplayerSample
        main_web.cpp
        ${local_multiplayer_sources}
    )
    target_link_libraries(LocalMultiplayerSample PRIVATE LabradorEngine labrador_settings)

    # A page, its loader and the module, as LineSweeper's are. The content is
    # the three things the Windows build copies beside its executable,
    # embedded at the same relative paths, so the one manifest serves both.
    set_target_properties(LocalMultiplayerSample PROPERTIES SUFFIX ".html")
    target_link_options(LocalMultiplayerSample PRIVATE
        "SHELL:--embed-file ${CMAKE_CURRENT_SOURCE_DIR}/content/manifest.json@/manifest.json"
        "SHELL:--embed-file ${CMAKE_CURRENT_SOURCE_DIR}/../minimal/content/fonts@/fonts"
        "SHELL:--embed-file ${CMAKE_CURRENT_SOURCE_DIR}/../linesweeper/content/textures@/textures"
        "SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
        -sENVIRONMENT=web
    )
    # Neither the page nor the content is a source, so a change to either
    # relinks only because it is named here.
    set_property(TARGET LocalMultiplayerSample APPEND PROPERTY LINK_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
        "${CMAKE_CURRENT_SOURCE_DIR}/content/manifest.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/../minimal/content/fonts/jetbrains_mono_bold_16.spritefont"
        "${CMAKE_CURRENT_SOURCE_DIR}/../linesweeper/content/textures/white.dds"
    )
elseif(WIN32)
    add_executable(LocalMultiplayerSample WIN32
        main.cpp
        ${local_multiplayer_sources}
    )
    target_link_libraries(LocalMultiplayerSample PRIVATE LabradorEngine labrador_settings shell32)
    if(NOT LABRADOR_RENDER_BACKEND STREQUAL "null")
        target_sources(LocalMultiplayerSample PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../../tools/linesweeper_capture/png_file.cpp")
        target_compile_definitions(LocalMultiplayerSample PRIVATE LOCAL_MULTIPLAYER_CAPTURE)
        target_link_libraries(LocalMultiplayerSample PRIVATE windowscodecs)
    endif()

    # Reuse the repository's existing font and white texel; the output is self-contained.
    add_custom_target(local_multiplayer_content ALL)
    add_custom_command(TARGET local_multiplayer_content POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/content/manifest.json"
            "$<TARGET_FILE_DIR:LocalMultiplayerSample>/manifest.json"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/../minimal/content/fonts"
            "$<TARGET_FILE_DIR:LocalMultiplayerSample>/fonts"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/../linesweeper/content/textures"
            "$<TARGET_FILE_DIR:LocalMultiplayerSample>/textures"
        VERBATIM
    )
    add_dependencies(LocalMultiplayerSample local_multiplayer_content)
endif()
