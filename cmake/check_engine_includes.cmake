# docs/design/ARCHITECTURE.md: an engine file including a game header "fails to
# compile, and that is the feature (T5)". It does not fail yet - both libraries
# publish ${CMAKE_SOURCE_DIR} as a PUBLIC include directory, so the include
# resolves. Narrowing the include roots is a later, larger edit; until then this
# check is what makes the sentence true.
#
# Run with: cmake -DENGINE_DIR=<path> -P cmake/check_engine_includes.cmake

if(NOT DEFINED ENGINE_DIR)
    message(FATAL_ERROR "check_engine_includes.cmake: ENGINE_DIR is not set")
endif()

file(GLOB_RECURSE engine_sources "${ENGINE_DIR}/*.h" "${ENGINE_DIR}/*.cpp")

set(offenders "")
foreach(source IN LISTS engine_sources)
    # Both spellings: the root-relative "game/..." the include roots make
    # possible, and the "../game/..." a relative include would reach for.
    file(STRINGS "${source}" hits
        REGEX "^[ \t]*#[ \t]*include[ \t]*[\"<](\\.\\./)*game/")
    foreach(hit IN LISTS hits)
        string(STRIP "${hit}" hit)
        list(APPEND offenders "${source}\n      ${hit}")
    endforeach()
endforeach()

if(offenders)
    list(JOIN offenders "\n    " report)
    message(FATAL_ERROR
        "Dependencies point one way. These engine files include a game header:\n"
        "    ${report}\n"
        "  See docs/design/ARCHITECTURE.md, The module graph.")
endif()
