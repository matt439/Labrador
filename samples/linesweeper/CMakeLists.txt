# xaudio2redist first: directxtk's config references Microsoft::XAudio2Redist
# but does not find it itself.
find_package(xaudio2redist CONFIG REQUIRED)
find_package(directxtk CONFIG REQUIRED)

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
# Sources are enumerated, never globbed, matching engine/CMakeLists.txt: a new
# .cpp that nobody lists is a file that silently is not compiled.
add_executable(LineSweeperSample WIN32
    main.cpp
    presentation/board_view.cpp
    presentation/particles.cpp
    presentation/top_out_banner.cpp
    states/pause_state.cpp
    states/play_state.cpp
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
