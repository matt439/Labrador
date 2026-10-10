# --- LineSweeperRules — the simulation, and it links nothing ---
#
# labrador_settings is an INTERFACE target carrying compiler flags and no
# libraries, so this target has no way to reach an engine symbol: an include of
# engine/ that resolves still fails at link. That is the wall, and it is why
# the whole game is playable from tests/ with no window and no device.
#
# It is a static library rather than a folder for exactly the reason
# ARCHITECTURE gives for MattMath being one: when a wall has to be load-bearing
# rather than a convention, the escalation is cheap and it is this.
add_library(LineSweeperRules STATIC
    rules/tables.cpp
    rules/tick.cpp
    rules/world.cpp
)
target_link_libraries(LineSweeperRules PUBLIC
    labrador_settings
)
# Includes are written from the repository root (CONVENTIONS), so the directory
# above samples/ has to be on the path - the same arrangement a client project
# makes for its own tree.
target_include_directories(LineSweeperRules PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/../.."
)

# --- LineSweeperSample — the game ---
#
# One game and two entry points: main.cpp is wWinMain and main_web.cpp is a
# browser's main, and everything else is the same files. The rules above are in
# both, and in LineSweeperTests, which links them in every build.
#
# Sources are enumerated, never globbed, matching engine/CMakeLists.txt: a new
# .cpp that nobody lists is a file that silently is not compiled.
set(linesweeper_game_sources
    presentation/board_view.cpp
    presentation/particles.cpp
    presentation/top_out_banner.cpp
    states/pause_state.cpp
    states/play_state.cpp
)

if(EMSCRIPTEN)
    add_executable(LineSweeperSample
        main_web.cpp
        ${linesweeper_game_sources}
    )
    target_link_libraries(LineSweeperSample PRIVATE
        LineSweeperRules
        LabradorEngine
        labrador_settings
    )

    # A page, its loader and the module: LineSweeperSample.html, .js and
    # .wasm. The page is web/shell.html, a development page that hands the
    # module its canvas; web/shell.html says what any page owes it.
    #
    # The content is embedded in the module rather than fetched beside it,
    # mounted at the root of its file system, which is where a relative
    # manifest path resolves in a browser. At 21.6 KB a second request would
    # cost more than the bytes; docs/port/web.md puts the point at which that
    # turns round at about a megabyte.
    set_target_properties(LineSweeperSample PROPERTIES SUFFIX ".html")
    target_link_options(LineSweeperSample PRIVATE
        "SHELL:--embed-file ${CMAKE_CURRENT_SOURCE_DIR}/content@/"
        "SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
        -sENVIRONMENT=web
    )
    # Neither the page nor the content is a source, so a change to either
    # relinks only because it is named here.
    set_property(TARGET LineSweeperSample APPEND PROPERTY LINK_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
        "${CMAKE_CURRENT_SOURCE_DIR}/content/manifest.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/content/fonts/courier_new_bold_16.spritefont"
        "${CMAKE_CURRENT_SOURCE_DIR}/content/textures/white.dds"
    )
elseif(WIN32)
    add_executable(LineSweeperSample WIN32
        main.cpp
        ${linesweeper_game_sources}
    )
    target_link_libraries(LineSweeperSample PRIVATE
        LineSweeperRules
        LabradorEngine
        labrador_settings
    )

    # Its own content, not a share of the other sample's. Two samples is few enough
    # that independence is cheaper than the coupling - an engine with forty would
    # put the media in one place and pay the coupling instead.
    add_custom_target(linesweeper_content ALL)
    add_custom_command(TARGET linesweeper_content POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/content/manifest.json"
            "$<TARGET_FILE_DIR:LineSweeperSample>/manifest.json"
        VERBATIM
    )
    add_custom_command(TARGET linesweeper_content POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/content/fonts"
            "$<TARGET_FILE_DIR:LineSweeperSample>/fonts"
        VERBATIM
    )
    # One texture, and it is one white texel. Every block, panel and rule on the
    # screen is that texel with a tint on it - a solid rectangle is a sprite, and
    # the renderer seam draws sprites (presentation/board_view.cpp says why there
    # is no draw_rect and should not be one).
    add_custom_command(TARGET linesweeper_content POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/content/textures"
            "$<TARGET_FILE_DIR:LineSweeperSample>/textures"
        VERBATIM
    )
    add_dependencies(LineSweeperSample linesweeper_content)
endif()
