add_executable(LocalMultiplayerSample WIN32
    arena.cpp
    controls.cpp
    main.cpp
    play_state.cpp
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
