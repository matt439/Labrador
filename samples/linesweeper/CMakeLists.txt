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
    presentation/arrangement.cpp
    presentation/board_view.cpp
    presentation/particles.cpp
    presentation/top_out_banner.cpp
    presentation/touch_controls.cpp
    states/pause_state.cpp
    states/play_state.cpp
)

if(EMSCRIPTEN)
    # Game sources are compiled once and linked twice. The shared entry file
    # is compiled per executable so only the development page can opt into
    # pacing instrumentation; the host module excludes that code.
    add_library(LineSweeperWeb OBJECT
        ${linesweeper_game_sources}
    )
    target_link_libraries(LineSweeperWeb PRIVATE
        LineSweeperRules
        LabradorEngine
        labrador_settings
    )

    # The content is embedded in the module rather than fetched beside it,
    # mounted at the root of its file system, which is where a relative
    # manifest path resolves in a browser. At 29 KB a second request would
    # cost more than the bytes; docs/port/web.md puts the point at which that
    # turns round at about a megabyte.
    set(linesweeper_content_files
        "${CMAKE_CURRENT_SOURCE_DIR}/content/manifest.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/content/fonts/jetbrains_mono_bold_16.spritefont"
        "${CMAKE_CURRENT_SOURCE_DIR}/content/textures/white.dds"
    )

    # A page, its loader and the module: LineSweeperSample.html, .js and
    # .wasm. The page is web/shell.html, a development page that hands the
    # module its canvas; web/shell.html says what any page owes it.
    add_executable(LineSweeperSample
        main_web.cpp
        web/pacing_fixture.cpp
        web/pacing_observer.cpp
        web/pacing_state.cpp
        $<TARGET_OBJECTS:LineSweeperWeb>
    )
    target_link_libraries(LineSweeperSample PRIVATE
        LineSweeperRules
        LabradorEngine
        labrador_settings
    )
    target_compile_definitions(LineSweeperSample PRIVATE
        LABRADOR_DEVELOPMENT_PACING=1
        LABRADOR_PACING_SOURCE_REVISION="${LABRADOR_SOURCE_REVISION}"
        LABRADOR_PACING_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
    )
    set_target_properties(LineSweeperSample PROPERTIES SUFFIX ".html")
    target_link_options(LineSweeperSample PRIVATE
        "SHELL:--embed-file ${CMAKE_CURRENT_SOURCE_DIR}/content@/"
        "SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
        "SHELL:--pre-js ${PROJECT_SOURCE_DIR}/tools/web_pacing/page.js"
        -sENVIRONMENT=web
    )
    # Neither the page nor the content is a source, so a change to either
    # relinks only because it is named here.
    set_property(TARGET LineSweeperSample APPEND PROPERTY LINK_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
        "${PROJECT_SOURCE_DIR}/tools/web_pacing/page.js"
        ${linesweeper_content_files}
    )

    # THE MODULE A HOST PAGE IMPORTS: linesweeper.js, an ES module whose
    # default export is the factory createLineSweeper, and linesweeper.wasm
    # beside it. Each call of the factory is an instance of its own - its own
    # memory, its own static Application, its own Module object, which is
    # where the window finds the canvas and the page's callbacks - so a page that
    # wants a second game after QUIT calls it again with a fresh canvas
    # (web/index.html does).
    add_executable(LineSweeperModule main_web.cpp $<TARGET_OBJECTS:LineSweeperWeb>)
    target_link_libraries(LineSweeperModule PRIVATE
        LineSweeperRules
        LabradorEngine
        labrador_settings
    )
    set_target_properties(LineSweeperModule PROPERTIES
        OUTPUT_NAME linesweeper
        SUFFIX ".js"
    )
    target_link_options(LineSweeperModule PRIVATE
        "SHELL:--embed-file ${CMAKE_CURRENT_SOURCE_DIR}/content@/"
        -sENVIRONMENT=web
        -sMODULARIZE=1
        -sEXPORT_ES6=1
        -sEXPORT_NAME=createLineSweeper
    )
    set_property(TARGET LineSweeperModule APPEND PROPERTY LINK_DEPENDS
        ${linesweeper_content_files}
    )

    # THE BUNDLE A HOST SERVES, in web-dist/linesweeper/ under the build
    # directory: the module, an example page that mounts it on a click, a
    # poster, the third-party notices for what the module links, and
    # BUILD.json naming the commit it was built from and the size of each file
    # raw and under Brotli. Rewritten on every build, because the commit can
    # change when no file does. tools/web_bundle/bundle.mjs says what each
    # argument is; Node is the one Emscripten's toolchain already runs.
    set(linesweeper_dist "${CMAKE_BINARY_DIR}/web-dist/linesweeper")
    set(emscripten_libraries "${EMSCRIPTEN_ROOT_PATH}/system/lib")
    add_custom_target(linesweeper_web_dist ALL
        COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR}
            "${PROJECT_SOURCE_DIR}/tools/web_bundle/bundle.mjs"
            --out "${linesweeper_dist}"
            --repository "${PROJECT_SOURCE_DIR}"
            --emscripten "${EMSCRIPTEN_ROOT_PATH}"
            --build-type "${CMAKE_BUILD_TYPE}"
            --file "index.html=${CMAKE_CURRENT_SOURCE_DIR}/web/index.html"
            --file "linesweeper.js=$<TARGET_FILE:LineSweeperModule>"
            --file "linesweeper.wasm=$<TARGET_FILE_DIR:LineSweeperModule>/linesweeper.wasm"
            --file "poster.png=${PROJECT_SOURCE_DIR}/website/src/assets/captures/linesweeper-play.png"
            --notice "${CMAKE_CURRENT_SOURCE_DIR}/web/NOTICE.txt"
            --licence "Labrador=${PROJECT_SOURCE_DIR}/LICENSE"
            --licence "RapidJSON=${PROJECT_SOURCE_DIR}/external/rapidjson/license.txt"
            --licence "JetBrains Mono=${CMAKE_CURRENT_SOURCE_DIR}/content/fonts/OFL.txt"
            --licence "Emscripten=${EMSCRIPTEN_ROOT_PATH}/LICENSE"
            --licence "libc++=${emscripten_libraries}/libcxx/LICENSE.TXT"
            --licence "libc++abi=${emscripten_libraries}/libcxxabi/LICENSE.TXT"
            --licence "libunwind=${emscripten_libraries}/libunwind/LICENSE.TXT"
            --licence "compiler-rt=${emscripten_libraries}/compiler-rt/LICENSE.TXT"
            --licence "musl=${emscripten_libraries}/libc/musl/COPYRIGHT"
        DEPENDS LineSweeperModule
        COMMENT "Bundling LineSweeper for a host page"
        VERBATIM
    )
elseif(WIN32)
    # The template's DPI awareness declaration (samples/minimal/dpi_aware.manifest).
    add_executable(LineSweeperSample WIN32
        main.cpp
        ${linesweeper_game_sources}
        "${CMAKE_CURRENT_SOURCE_DIR}/../minimal/dpi_aware.manifest"
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
