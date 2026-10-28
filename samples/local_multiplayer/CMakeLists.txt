# One sample and two entry points: main.cpp is wWinMain, with the smoke test
# and the capture, and main_web.cpp is a browser's main. Everything else is
# the same files. LocalMultiplayerTests links the arena, controls and pure join
# rules in every build; native null adds the real menu routing cases.
set(local_multiplayer_sources
    arena.cpp
    controls.cpp
    join_rules.cpp
    join_state.cpp
    play_state.cpp
)

if(EMSCRIPTEN)
    add_executable(LocalMultiplayerSample
        main_web.cpp
        ${local_multiplayer_sources}
    )
    target_link_libraries(LocalMultiplayerSample PRIVATE LabradorEngine labrador_settings)

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
        ${local_multiplayer_sources}
        "${CMAKE_CURRENT_SOURCE_DIR}/../minimal/dpi_aware.manifest"
    )
    target_link_libraries(LocalMultiplayerSample PRIVATE LabradorEngine labrador_settings shell32)
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
endif()
