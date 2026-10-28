# LineSweeper's two include boundaries, independent of compiler include roots.
# Only literal, single-line #include directives in .h/.cpp files are read.
# Both quote and angle forms use forward slashes; dot components are normalised
# against the repository include root and the including file's directory.
# Conditional directives are checked even in inactive branches. This does not
# preprocess block comments. Macro includes, line continuations, generated
# files and transitive dependencies are outside this lexical check.
# See samples/linesweeper/README.md, Three layers.
# Run with cmake -DREPO_DIR=<checkout> -P this-file.
cmake_minimum_required(VERSION 3.26)

if(NOT DEFINED REPO_DIR)
    message(FATAL_ERROR "check_linesweeper_layers.cmake: REPO_DIR is not set")
endif()
file(REAL_PATH "${REPO_DIR}" REPO_DIR)
set(sample_dir "${REPO_DIR}/samples/linesweeper")
foreach(layer IN ITEMS rules presentation)
    if(NOT IS_DIRECTORY "${sample_dir}/${layer}")
        message(FATAL_ERROR "Missing LineSweeper layer directory: ${sample_dir}/${layer}")
    endif()
endforeach()

set(offenders "")
foreach(layer IN ITEMS rules presentation)
    file(GLOB_RECURSE layer_sources "${sample_dir}/${layer}/*.h" "${sample_dir}/${layer}/*.cpp")
    foreach(source IN LISTS layer_sources)
        file(STRINGS "${source}" includes
            REGEX "^[ \t]*#[ \t]*include[ \t]*[\"<][^\">]+[\">]")
        foreach(line IN LISTS includes)
            string(REGEX MATCH "[\"<]([^\">]+)[\">]" ignored "${line}")
            set(include_path "${CMAKE_MATCH_1}")
            get_filename_component(source_dir "${source}" DIRECTORY)
            set(forbidden FALSE)
            foreach(include_root IN ITEMS "${REPO_DIR}" "${source_dir}")
                set(target_path "${include_root}/${include_path}")
                cmake_path(NORMAL_PATH target_path)
                if(layer STREQUAL "rules")
                    set(engine_dir "${REPO_DIR}/engine")
                    cmake_path(IS_PREFIX engine_dir "${target_path}" NORMALIZE forbidden_target)
                else()
                    set(forbidden_target FALSE)
                    if(target_path STREQUAL "${sample_dir}/rules/tick.h")
                        set(forbidden_target TRUE)
                    endif()
                endif()
                if(forbidden_target)
                    set(forbidden TRUE)
                endif()
            endforeach()
            if(forbidden)
                string(STRIP "${line}" line)
                file(RELATIVE_PATH relative "${REPO_DIR}" "${source}")
                list(APPEND offenders "${relative}\n      ${line}")
            endif()
        endforeach()
    endforeach()
endforeach()

if(offenders)
    list(JOIN offenders "\n    " report)
    message(FATAL_ERROR
        "LineSweeper layers: rules/ cannot include engine/; presentation/ cannot include rules/tick.h.\n"
        "    ${report}\n"
        "  states/ owns input and rules verbs. See samples/linesweeper/README.md, Three layers.")
endif()
