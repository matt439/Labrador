# docs/design/ARCHITECTURE.md: an engine file including a game header "fails to
# compile, and that is the feature (T5)". It does not fail yet, and E1 - the
# item that was going to fix it - established why not rather than fixing it.
#
# Includes are written from the repository root (CONVENTIONS, Files), so an
# engine file says "engine/render/renderer.h". That means the engine's own
# include root has to be the directory above engine/ - and in this tree, that
# directory also holds game/. There is no include path that admits the first
# spelling and refuses the second while the two are siblings. What closes it is
# the repo split: engine/ and samples/ in one repository, the paint-shooter in
# its own, consuming the engine as a submodule. E1 made the include roots
# relative so that split is a move and not a build rewrite.
#
# Until then, this is the wall. It runs on every build, which is the property
# that matters: the rule is enforced, not merely written down.
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
