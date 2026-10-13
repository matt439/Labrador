# --- MinimalSample — the minimal sample, and the new-project template ---
#
# The whole build file for a game on this engine, for building it and running
# it during development: name the sources, link the engine and the shared
# settings, mirror the content - and a branch for the same game in a browser.
# A release a player installs needs install rules as well, which this file
# does not have.
# There is no project file format and no code generation (ARCHITECTURE.md,
# A game project).
#
# It is a permanent target rather than a documentation snippet because a
# second client is the only thing that keeps the engine/game boundary honest
# (PHILOSOPHY T1). It links LabradorEngine and no game's code, so if anything
# it uses ever moves out of the engine and into a game, this stops building.
set(minimal_sources
    states/confirm_state.cpp
    states/hello_state.cpp
)

# A browser build is a second entry point and a page, with the content packed
# into the module at the paths the Windows build copies it to.
if(EMSCRIPTEN)
    add_executable(MinimalSample main_web.cpp ${minimal_sources})
    target_link_libraries(MinimalSample PRIVATE LabradorEngine labrador_settings)
    set_target_properties(MinimalSample PROPERTIES SUFFIX ".html")
    # Neither the page nor the content is a source, so a change to either
    # relinks only because it is named here.
    set_property(TARGET MinimalSample APPEND PROPERTY LINK_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
        "${CMAKE_CURRENT_SOURCE_DIR}/content/manifest.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/content/fonts/jetbrains_mono_bold_16.spritefont"
    )
    target_link_options(MinimalSample PRIVATE
        "SHELL:--embed-file ${CMAKE_CURRENT_SOURCE_DIR}/content@/"
        "SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
        -sENVIRONMENT=web)
    return()
endif()

add_executable(MinimalSample WIN32
    main.cpp
    ${minimal_sources}
)
target_link_libraries(MinimalSample PRIVATE
    LabradorEngine
    labrador_settings
)

# A relative manifest path resolves against the executable's directory, not
# the working directory, and the folders it names against the manifest
# (engine/app/content_root.h), so mirror the content beside the exe. It is a
# target of its own because a POST_BUILD on the exe only runs when the exe
# relinks, so an edited asset would never arrive.
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
