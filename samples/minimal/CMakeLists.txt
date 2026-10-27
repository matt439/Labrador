# --- MinimalSample — the minimal sample, and the new-project template ---
#
# The whole build file for a game on this engine, for building it and running
# it during development: name the sources, link the engine and the shared
# settings, mirror the content, put the rules in a library and register their
# tests and a start-up check, stage the release folder a player gets - and a
# branch for the same game in a browser.
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

# The game's rules, in a library that links the engine's maths and nothing else
# of it: MattMath and the settings carry no window, no device and no input, so
# MinimalTests below runs them with none of those (rules/motion.h says what
# moved there to make that true). samples/linesweeper's LineSweeperRules is
# the same wall around a whole game.
add_library(MinimalRules STATIC
    rules/motion.cpp
)
target_link_libraries(MinimalRules PUBLIC
    MattMath
    labrador_settings
)

# The rules' tests, with doctest from vcpkg. One ctest entry however many cases
# it holds; a new test file is listed here beside the first.
find_package(doctest CONFIG REQUIRED)
add_executable(MinimalTests
    tests/motion_tests.cpp
)
target_link_libraries(MinimalTests PRIVATE
    MinimalRules
    doctest::doctest
    labrador_settings
)
add_test(NAME MinimalTests COMMAND MinimalTests)

# A browser build is a second entry point and a page, with the content packed
# into the module at the paths the Windows build copies it to.
if(EMSCRIPTEN)
    # The tests run under Node, which reads no files here. The root's options
    # for that come after this folder is added, because the page must not
    # take them, so the test names the one it needs.
    target_link_options(MinimalTests PRIVATE -sENVIRONMENT=node)

    add_executable(MinimalSample main_web.cpp ${minimal_sources})
    target_link_libraries(MinimalSample PRIVATE
        MinimalRules LabradorEngine labrador_settings)
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

# dpi_aware.manifest declares per-monitor DPI awareness, so Windows does not
# stretch the frame on a scaled display; the file says what that means.
add_executable(MinimalSample WIN32
    main.cpp
    ${minimal_sources}
    dpi_aware.manifest
)
target_link_libraries(MinimalSample PRIVATE
    MinimalRules
    LabradorEngine
    labrador_settings
)

# The start-up check: the game hidden, a few frames drawn, and out (main.cpp,
# SmokeTest). It needs a graphics device, so a machine without one fails it,
# and its content is what the build copies below, so a missing file fails it
# naming the file. The timeout is for a loop that never draws.
add_test(NAME MinimalSmokeTests COMMAND MinimalSample --smoke-test)
set_tests_properties(MinimalSmokeTests PROPERTIES TIMEOUT 60)

# A relative manifest path resolves against the executable's directory, not
# the working directory, and the folders it names against the manifest
# (engine/app/content_root.h), so mirror the content beside the exe. It is a
# target of its own because a POST_BUILD on the exe only runs when the exe
# relinks, so an edited asset would never arrive.
# copy_directory_if_different adds and updates but never removes: a file
# deleted from content/ stays in the build folder until it is cleaned, so a
# required file goes on loading here and is missing only from a clean build,
# and an optional bank is never replaced by the silent one. A game with
# content it may delete lists its files instead (the assets guide, "Optional
# content and stale copies").
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

# The release folder a player gets, from the x64-release build:
#   cmake --install out/build/x64-release --prefix <folder>
# The program, the DLLs it loads, its content and the notices of what it
# carries. A debug build loads the debug runtime, which may not be given away.
if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    install(CODE [[message(FATAL_ERROR "Install the x64-release build.")]])
endif()
install(TARGETS MinimalSample RUNTIME DESTINATION .)
# DirectXTK and the XAudio2 redistributable, which vcpkg built as DLLs.
install(FILES $<TARGET_RUNTIME_DLLS:MinimalSample> DESTINATION .)
# The MSVC runtime, copied beside the program from Visual Studio's redist
# folder, so a player needs no installer: CMake's list of its DLLs, which
# leaves out vcruntime140_threads.dll and vccorlib140.dll.
set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
include(InstallRequiredSystemLibraries)
install(PROGRAMS ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} DESTINATION .)
install(FILES content/manifest.json DESTINATION .)
install(DIRECTORY content/fonts DESTINATION .)
# The licences of what the program carries: the engine, the libraries
# compiled into it and the DLLs beside it. The font's is fonts/OFL.txt.
get_target_property(labrador_engine_dir LabradorEngine SOURCE_DIR)
set(vcpkg_share "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share")
install(FILES "${labrador_engine_dir}/../LICENSE"
    DESTINATION notices RENAME Labrador.txt)
# NOTICE: the code in the engine that is not Labrador's to license.
install(FILES "${labrador_engine_dir}/../NOTICE"
    DESTINATION notices RENAME Labrador-NOTICE.txt)
install(FILES "${labrador_engine_dir}/../external/rapidjson/license.txt"
    DESTINATION notices RENAME rapidjson.txt)
install(FILES "${vcpkg_share}/directxtk/copyright"
    DESTINATION notices RENAME DirectXTK.txt)
install(FILES "${vcpkg_share}/directxmath/copyright"
    DESTINATION notices RENAME DirectXMath.txt)
install(FILES "${vcpkg_share}/xaudio2redist/copyright"
    DESTINATION notices RENAME XAudio2Redist.txt)
