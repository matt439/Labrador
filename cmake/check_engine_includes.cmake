# Architecture's engine/game and backend-folder boundaries, independent of
# a consumer's compiler include roots. Sources are .h/.cpp; backend targets
# are .h. Only literal single-line, forward-slash includes are read.
# Repository-root engine spellings and explicit dot-relative operands are
# normalized lexically. Other vendor tokens are not source-relative guesses.
# Source-relative backend targets need a real owner directory, but the
# included header itself may be generated and need not exist.
# Macro includes, continuations, block-comment preprocessing, symlinks,
# transitive includes and arbitrary compiler search paths are outside this
# source lint. See docs/design/ARCHITECTURE.md, Modules.
# Run: cmake -DENGINE_DIR=<checkout>/engine -P this-file.
cmake_minimum_required(VERSION 3.26)

if(NOT DEFINED ENGINE_DIR)
    message(FATAL_ERROR "check_engine_includes.cmake: ENGINE_DIR is not set")
endif()
file(REAL_PATH "${ENGINE_DIR}" ENGINE_DIR)
get_filename_component(repo_dir "${ENGINE_DIR}" DIRECTORY)
set(game_dir "${repo_dir}/game")
file(GLOB_RECURSE engine_sources "${ENGINE_DIR}/*.h" "${ENGINE_DIR}/*.cpp")

set(game_offenders "")
set(backend_offenders "")
foreach(source IN LISTS engine_sources)
    get_filename_component(source_dir "${source}" DIRECTORY)
    file(RELATIVE_PATH relative_source "${repo_dir}" "${source}")
    file(STRINGS "${source}" includes
        REGEX "^[ \t]*#[ \t]*include[ \t]*[\"<][^\">]+[\">]")
    foreach(line IN LISTS includes)
        string(REGEX MATCH "[\"<]([^\">]+)[\">]" ignored "${line}")
        set(include_path "${CMAKE_MATCH_1}")
        string(FIND "${include_path}" "\\" backslash_position)
        cmake_path(IS_ABSOLUTE include_path absolute_include)
        if(NOT backslash_position EQUAL -1 OR absolute_include)
            continue()
        endif()
        set(normalized_include "${include_path}")
        cmake_path(NORMAL_PATH normalized_include)

        # Keep the named game/ rule, including leading ../ spellings. Do not
        # guess other client folder names such as yourgame/ or src/ (T5).
        set(game_forbidden FALSE)
        if(normalized_include MATCHES "^(\\.\\./)*game/")
            set(game_forbidden TRUE)
        endif()

        set(target_paths "")
        set(source_relative_target "")
        if(normalized_include MATCHES "^engine/")
            # A normalized repository-root spelling has one interpretation.
            # Trying it beneath source_dir too would invent a nested backend.
            list(APPEND target_paths "${repo_dir}/${normalized_include}")
        elseif(include_path MATCHES "^\\.\\.?/" OR normalized_include MATCHES "^\\.\\.?/")
            # Both explicit include roots are checked, without pretending an
            # arbitrary token such as rapidjson/error/en.h is under source_dir.
            set(source_relative_target "${source_dir}/${include_path}")
            cmake_path(NORMAL_PATH source_relative_target)
            list(APPEND target_paths "${repo_dir}/${include_path}" "${source_relative_target}")
        endif()

        set(backend_forbidden FALSE)
        foreach(target_path IN LISTS target_paths)
            cmake_path(NORMAL_PATH target_path)
            cmake_path(IS_PREFIX game_dir "${target_path}" NORMALIZE game_target)
            if(game_target)
                set(game_forbidden TRUE)
            endif()
            cmake_path(IS_PREFIX ENGINE_DIR "${target_path}" NORMALIZE engine_target)
            if(NOT engine_target)
                continue()
            endif()
            file(RELATIVE_PATH engine_relative "${ENGINE_DIR}" "${target_path}")
            if(engine_relative MATCHES "^([^/]+)/([^/]+)/.+[.]h$")
                set(backend_dir "${ENGINE_DIR}/${CMAKE_MATCH_1}/${CMAKE_MATCH_2}")
                # Source-relative interpretation needs an actual owner folder,
                # not an invented assets/external/ from a dotted vendor path.
                # The included file may be generated and need not exist.
                if(target_path STREQUAL source_relative_target AND NOT IS_DIRECTORY "${backend_dir}")
                    continue()
                endif()
                # Ownership is the whole subtree, including nested sources
                # and headers. The path-prefix check respects segment ends.
                cmake_path(IS_PREFIX backend_dir "${source}" NORMALIZE same_backend)
                if(NOT same_backend)
                    set(backend_forbidden TRUE)
                endif()
            endif()
        endforeach()

        string(STRIP "${line}" line)
        if(game_forbidden)
            list(APPEND game_offenders "${relative_source}\n      ${line}")
        elseif(backend_forbidden)
            list(APPEND backend_offenders "${relative_source}\n      ${line}")
        endif()
    endforeach()
endforeach()

if(game_offenders)
    list(JOIN game_offenders "\n    " report)
    message(FATAL_ERROR
        "Dependencies point one way. These engine files include a game header:\n"
        "    ${report}\n"
        "  See docs/design/ARCHITECTURE.md, Modules.")
endif()
if(backend_offenders)
    list(JOIN backend_offenders "\n    " report)
    message(FATAL_ERROR
        "A backend header is for its own folder. These files reach across:\n"
        "    ${report}\n"
        "  Everything that draws goes through DrawList, and everything that\n"
        "  builds a resource on a device goes through render/resource_factory.h.\n"
        "  Everything that makes a noise goes through audio/audio_device.h.\n"
        "  Everything that reads a pad goes through input/gamepad_reader.h.\n"
        "  See docs/design/ARCHITECTURE.md, Modules.")
endif()
