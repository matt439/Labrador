cmake_minimum_required(VERSION 3.26)

# Labrador — a 2D game engine, its sample, its tests and its benchmarks.
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

add_subdirectory(engine)

# A client consuming this as a submodule wants the library and nothing else:
# its ctest should not run the engine's tests, and it should certainly not
# build the engine's samples. Without this guard, adding Labrador as a
# subdirectory drags in doctest, eleven test targets, a benchmark and two
# sample executables, and puts them all in the client's ctest.
if(PROJECT_IS_TOP_LEVEL)
    enable_testing()

    add_subdirectory(samples/minimal)
    add_subdirectory(samples/linesweeper)
    add_subdirectory(bench)
    add_subdirectory(tests/app)
    add_subdirectory(tests/assets)
    add_subdirectory(tests/collision)
    add_subdirectory(tests/core)
    add_subdirectory(tests/input)
    add_subdirectory(tests/linesweeper)
    add_subdirectory(tests/math)
    add_subdirectory(tests/render)
    add_subdirectory(tests/scene)
    add_subdirectory(tests/ui)
endif()
