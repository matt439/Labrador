# Offline fixtures for the literal-include check. No compiler or device.
# Retained under out/linesweeper-layer-check for inspection.
cmake_minimum_required(VERSION 3.26)

if(NOT DEFINED REPO_DIR)
    message(FATAL_ERROR "check_linesweeper_layer_fixtures.cmake: REPO_DIR is not set")
endif()
file(REAL_PATH "${REPO_DIR}" REPO_DIR)
string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef fixture_suffix)
set(fixture "${REPO_DIR}/out/linesweeper-layer-check/${fixture_suffix}")
if(EXISTS "${fixture}")
    message(FATAL_ERROR "Refusing to overwrite an existing fixture: ${fixture}")
endif()
set(checker "${CMAKE_CURRENT_LIST_DIR}/check_linesweeper_layers.cmake")

function(check_fixture name relative content expected)
    set(root "${fixture}/${name}")
    file(MAKE_DIRECTORY "${root}/samples/linesweeper/rules" "${root}/samples/linesweeper/presentation")
    get_filename_component(source_dir "${root}/${relative}" DIRECTORY)
    file(MAKE_DIRECTORY "${source_dir}")
    file(WRITE "${root}/${relative}" "${content}\n")
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DREPO_DIR=${root}" -P "${checker}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(expected STREQUAL "pass")
        if(NOT result STREQUAL "0")
            message(FATAL_ERROR "Allowed fixture ${name} failed:\n${output}\n${error}")
        endif()
    else()
        if(result STREQUAL "0")
            message(FATAL_ERROR "Forbidden fixture ${name} passed")
        endif()
        string(FIND "${error}" "${relative}" file_position)
        string(FIND "${error}" "${content}" include_position)
        if(file_position EQUAL -1 OR include_position EQUAL -1)
            message(FATAL_ERROR "Fixture ${name} did not name its file and offending include:\n${error}")
        endif()
    endif()
endfunction()

check_fixture(rules_quote samples/linesweeper/rules/bad.h
    "#include \"engine/math/vector2.h\"" fail)
check_fixture(rules_angle samples/linesweeper/rules/bad.cpp
    "# include <engine/core/scalar.h>" fail)
check_fixture(rules_relative samples/linesweeper/rules/nested/bad.h
    "#include \"../../../../engine/math/vector2.h\"" fail)
check_fixture(rules_dot samples/linesweeper/rules/bad.cpp
    "#include <./engine/../engine/math/vector2.h>" fail)
check_fixture(presentation_quote samples/linesweeper/presentation/bad.h
    "#include \"samples/linesweeper/rules/tick.h\"" fail)
check_fixture(presentation_angle samples/linesweeper/presentation/bad.cpp
    "#include <samples/linesweeper/rules/tick.h>" fail)
check_fixture(presentation_relative samples/linesweeper/presentation/nested/bad.h
    "#include \"../../rules/tick.h\"" fail)
check_fixture(presentation_dot samples/linesweeper/presentation/bad.cpp
    "#include <../rules/./tick.h>" fail)
check_fixture(rules_values samples/linesweeper/rules/allowed.h
    "#include \"samples/linesweeper/rules/world.h\"\n#include <cstdint>\n#include \"../rules/tables.h\"" pass)
check_fixture(rules_similar_names samples/linesweeper/rules/allowed.cpp
    "#include \"engine_extra/math/vector2.h\"\n#include <../engines/scalar.h>" pass)
check_fixture(presentation_values samples/linesweeper/presentation/allowed.cpp
    "#include \"samples/linesweeper/rules/world.h\"\n#include <../rules/tables.h>\n#include \"engine/core/scalar.h\"" pass)
check_fixture(similar_names samples/linesweeper/presentation/allowed.h
    "#include \"../rules/tick_helpers.h\"\n#include \"../rules/tick.h.extra\"" pass)
check_fixture(states_verb samples/linesweeper/states/allowed.cpp
    "#include \"samples/linesweeper/rules/tick.h\"\n#include \"engine/app/state.h\"" pass)
check_fixture(comment samples/linesweeper/rules/allowed.cpp
    "// #include \"engine/core/scalar.h\"" pass)
message(STATUS "LineSweeper include-layer fixtures passed: ${fixture}")
