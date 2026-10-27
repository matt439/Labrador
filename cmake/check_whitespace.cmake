# .editorconfig states the tree's whitespace, and an editor that honours it
# keeps a file right. Not every editor does, and a file indented with spaces
# or carrying trailing blanks compiles exactly as well as one that is not, so
# nothing else would notice (T5). This reads every first-party source and
# names each line that breaks the two rules a reader trips over: indentation
# with spaces where the file type indents with tabs, and whitespace at the end
# of a line.
#
# The vendored files .editorconfig exempts keep their upstream spaces; NOTICE
# lists them. external/ is not read.
#
# Run with: cmake -DREPO_DIR=<path> -P cmake/check_whitespace.cmake

if(NOT DEFINED REPO_DIR)
    message(FATAL_ERROR "check_whitespace.cmake: REPO_DIR is not set")
endif()

set(tab_indented "")
foreach(folder IN ITEMS engine samples tests bench tools)
    file(GLOB_RECURSE found
        "${REPO_DIR}/${folder}/*.h"
        "${REPO_DIR}/${folder}/*.cpp"
        "${REPO_DIR}/${folder}/*.hlsl"
        "${REPO_DIR}/${folder}/*.js"
        "${REPO_DIR}/${folder}/*.mjs")
    list(APPEND tab_indented ${found})
endforeach()
list(FILTER tab_indented EXCLUDE REGEX "/node_modules/|/out/")
list(FILTER tab_indented EXCLUDE REGEX
    "/engine/core/step_timer\\.h$|/engine/render/throw_if_failed\\.h$|/engine/render/d3d11/device_resources\\.(h|cpp)$")

set(offenders "")
foreach(source IN LISTS tab_indented)
    # A checkout with autocrlf hands every line a trailing carriage return,
    # which is the platform's line ending and not trailing whitespace.
    file(STRINGS "${source}" hits ENCODING UTF-8 REGEX "^ |[ \t]\r?$")
    foreach(hit IN LISTS hits)
        string(REGEX REPLACE "\r$" "" hit "${hit}")
        file(RELATIVE_PATH name "${REPO_DIR}" "${source}")
        list(APPEND offenders "${name}: [${hit}]")
    endforeach()
endforeach()

if(offenders)
    list(JOIN offenders "\n    " report)
    message(FATAL_ERROR
        "These lines break .editorconfig: indented with spaces where the "
        "file indents with tabs, or ending in whitespace:\n"
        "    ${report}\n"
        "  Indent with tabs and strip trailing whitespace. See .editorconfig.")
endif()
