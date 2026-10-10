cmake_minimum_required(VERSION 3.26)

# Labrador — a 2D game engine, its samples, its tests and its benchmarks.
# docs/design/ARCHITECTURE.md describes the shape; this file is the whole of
# the top-level build.
project(Labrador LANGUAGES CXX)

include(cmake/settings.cmake)
include(cmake/compile_shaders.cmake)

# The module graph, enforced. Runs on every build.
#
# Standalone, the compiler now enforces this too: there is no game/ beside
# engine/, so an engine file including a game header cannot resolve it. The
# grep stays because that is only true standalone — a client building this as
# a subdirectory does so from a tree that may well have a game/ in it, and
# that client's include roots are not the engine's business. Belt, braces, and
# it costs one build step.
add_custom_target(check_engine_includes ALL
    COMMAND "${CMAKE_COMMAND}"
        "-DENGINE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/engine"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_engine_includes.cmake"
    COMMENT "Checking that no engine file includes a game header"
    VERBATIM
)

# AND EVERY REAL TARGET DEPENDS ON IT, which ALL alone did not give. ALL puts
# a target in the default build and nowhere else: `ninja LabradorEngine`, an
# IDE building the one target it has open, a client building its game target
# - none of those is the default build, and every one of them skipped the
# check while this file said "runs on every build"
# (docs/review/gpt6/README.md, G6-10). labrador_settings is the interface
# target every real target links (cmake/settings.cmake), and a dependency
# on an interface library is followed to whatever links it, so this one line
# is the dependency for the engine, the samples, the tests, the benchmark
# and a consumer's game alike. It is deliberately here rather than on
# LabradorEngine: MattMath is under engine/ too and builds without it.
add_dependencies(labrador_settings check_engine_includes)

add_subdirectory(engine)

# A client consuming this as a submodule wants the library and nothing else:
# its ctest should not run the engine's tests, and it should certainly not
# build the engine's samples. Without this guard, adding Labrador as a
# subdirectory would otherwise drag in the sample and test targets and put
# the repository's tests in the client's ctest.
if(PROJECT_IS_TOP_LEVEL)
    enable_testing()

    # Citations from code to a document, enforced. Runs on every build.
    #
    # Top-level only, unlike the wall above it. That one guards a hazard a
    # consumer really has - their tree may well have a game/ beside this one -
    # where this guards the engine's own prose against the engine's own edits.
    # A client that renames nothing and cites nothing should not be handed a
    # build failure for a section number this repository moved, and the files
    # it reads are tests/, samples/, bench/ and tools/, none of which a
    # consumer builds.
    add_custom_target(check_doc_citations ALL
        COMMAND "${CMAKE_COMMAND}"
            "-DREPO_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_doc_citations.cmake"
        COMMENT "Checking that every document a comment cites resolves"
        VERBATIM
    )

    # The same dependency the include check has, inside the same guard, so
    # a targeted build here runs it and a consumer's never sees it.
    add_dependencies(labrador_settings check_doc_citations)

    add_subdirectory(samples/linesweeper)
    add_subdirectory(bench)

    # Windows only, each until it has a reason not to be: four samples whose
    # only entry point is wWinMain, and a capture tool that reads back a
    # device (docs/port/web.md, section 6 says which sample goes next).
    if(WIN32)
        add_subdirectory(samples/minimal)
        add_subdirectory(samples/collision)
        add_subdirectory(samples/audio)
        add_subdirectory(samples/local_multiplayer)
        add_subdirectory(tools/linesweeper_capture)
    endif()

    # A browser build's tests run under Node, which ctest reaches through the
    # emulator Emscripten's toolchain sets. NODERAWFS hands them the real file
    # system, so a harness that loads "./content/" beside itself reads the
    # same files it reads on Windows, from the same build directory.
    if(EMSCRIPTEN)
        add_link_options(-sNODERAWFS=1 -sENVIRONMENT=node)
    endif()

    add_subdirectory(tests/app)
    add_subdirectory(tests/assets)
    add_subdirectory(tests/audio)
    add_subdirectory(tests/collision)
    add_subdirectory(tests/core)
    add_subdirectory(tests/input)
    add_subdirectory(tests/linesweeper)
    add_subdirectory(tests/local_multiplayer)
    add_subdirectory(tests/math)
    add_subdirectory(tests/render)
    add_subdirectory(tests/scene)
    add_subdirectory(tests/ui)
endif()
