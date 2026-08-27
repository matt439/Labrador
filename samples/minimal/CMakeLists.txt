# --- MinimalSample — the minimal sample, and the new-project template ---
#
# The whole build file for a game on this engine. A dozen honest lines: name
# the sources, link the engine and the shared settings, mirror the content.
# There is no project file format and no code generation (ARCHITECTURE.md,
# A game project).
#
# It is a permanent target rather than a documentation snippet because a
# second client is the only thing that keeps the engine/game boundary honest
# (PHILOSOPHY T1). It links LabradorEngine and nothing from game/, so if a
# mechanism ever drifts back into the paint-shooter, this stops building.
add_executable(MinimalSample WIN32
    main.cpp
    states/confirm_state.cpp
    states/hello_state.cpp
)
target_link_libraries(MinimalSample PRIVATE
    LabradorEngine
    labrador_settings
)

# Content is resolved relative to the working directory, so mirror it beside
# the exe. Its own target for the same reason the game's is: a POST_BUILD on
# the exe only runs when the exe relinks, so an edited asset would never
# arrive.
add_custom_target(sample_content ALL)
add_custom_command(TARGET sample_content POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/content/manifest.json"
        "$<TARGET_FILE_DIR:MinimalSample>/manifest.json"
    VERBATIM
)
add_custom_command(TARGET sample_content POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/content/fonts"
        "$<TARGET_FILE_DIR:MinimalSample>/fonts"
    VERBATIM
)
add_dependencies(MinimalSample sample_content)
