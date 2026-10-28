# Compiler-free engine include-wall fixtures, retained under out/ for review.
# CASE optionally selects one exact named case for a focused regression run.
# Run: cmake -DREPO_DIR=<checkout> [-DCASE=<name>] -P this-file.
cmake_minimum_required(VERSION 3.26)

if(NOT DEFINED REPO_DIR)
    message(FATAL_ERROR "check_engine_include_fixtures.cmake: REPO_DIR is not set")
endif()
file(REAL_PATH "${REPO_DIR}" REPO_DIR)
set(checker "${REPO_DIR}/cmake/check_engine_includes.cmake")
if(NOT EXISTS "${checker}")
    message(FATAL_ERROR "Missing engine include checker: ${checker}")
endif()
string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef fixture_suffix)
set(fixture "${REPO_DIR}/out/engine-include-check/${fixture_suffix}")
if(EXISTS "${fixture}")
    message(FATAL_ERROR "Refusing to overwrite an existing fixture: ${fixture}")
endif()
set(selected_count 0)

function(check_fixture name relative content expected kind)
    if(DEFINED CASE AND NOT CASE STREQUAL name)
        return()
    endif()
    set(root "${fixture}/${name}")
    if(EXISTS "${root}")
        message(FATAL_ERROR "Refusing to overwrite an existing case: ${root}")
    endif()
    file(MAKE_DIRECTORY "${root}/engine" "${root}/game")
    # Relative backend cases explicitly model the owner directory, while
    # leaving the included header absent to cover generated headers.
    foreach(owner IN LISTS ARGN)
        file(MAKE_DIRECTORY "${root}/engine/${owner}")
    endforeach()
    get_filename_component(source_dir "${root}/${relative}" DIRECTORY)
    file(MAKE_DIRECTORY "${source_dir}")
    file(WRITE "${root}/${relative}" "${content}\n")
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DENGINE_DIR=${root}/engine" -P "${checker}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(expected STREQUAL "pass")
        if(NOT result STREQUAL "0")
            message(FATAL_ERROR "Allowed fixture ${name} failed:\n${output}\n${error}")
        endif()
    else()
        if(result STREQUAL "0")
            message(FATAL_ERROR "Forbidden fixture ${name} passed")
        endif()
        string(REGEX MATCH "#[ \t]*include[^\n]*" include_line "${content}")
        string(STRIP "${include_line}" include_line)
        string(FIND "${error}" "${relative}" file_position)
        string(FIND "${error}" "${include_line}" include_position)
        if(kind STREQUAL "game")
            set(reason "Dependencies point one way.")
        else()
            set(reason "A backend header is for its own folder.")
        endif()
        string(FIND "${error}" "${reason}" reason_position)
        if(file_position EQUAL -1 OR include_position EQUAL -1 OR reason_position EQUAL -1)
            message(FATAL_ERROR "Fixture ${name} did not name its source, include and boundary:\n${error}")
        endif()
    endif()
    math(EXPR next_count "${selected_count} + 1")
    set(selected_count "${next_count}" PARENT_SCOPE)
endfunction()

check_fixture(game_quote engine/core/bad.h
    "#include \"game/objects/level.h\"" fail game)
check_fixture(game_angle engine/core/bad.cpp
    "  # include <game/objects/level.h>" fail game)
check_fixture(game_parents engine/core/bad.h
    "#include \"../../../game/objects/level.h\"" fail game)
check_fixture(game_other_extension engine/core/bad.cpp
    "#include \"game/objects/level.hpp\"" fail game)
check_fixture(game_leading_dot engine/core/bad.cpp
    "#include \"./game/objects/level.h\"" fail game)
check_fixture(game_embedded_collapse engine/core/bad.h
    "#include <engine/../game/objects/level.h>" fail game)
check_fixture(game_normalized_parents engine/core/bad.cpp
    "#include \".././game/objects/level.h\"" fail game)
# From root/engine/core, ../../../<case>/./game reaches root/game. The
# operand does not start with game/ after its leading parent components.
check_fixture(game_source_target engine/core/bad.h
    "#include \"../../../game_source_target/./game/level.h\"" fail game)
check_fixture(game_similar_name engine/core/allowed.h
    "#include \"game_extra/objects/level.h\"" pass none)
check_fixture(game_other_client_name engine/core/allowed.cpp
    "#include \"yourgame/objects/level.h\"" pass none)

check_fixture(render_quote engine/app/bad.h
    "#include \"engine/render/d3d11/backend.h\"" fail backend)
check_fixture(render_angle_spacing engine/app/bad.cpp
    "\t# \tinclude <engine/render/d3d12/device_resources.h>" fail backend)
check_fixture(audio_angle engine/app/bad.h
    "#include <engine/audio/null/recording.h>" fail backend)
check_fixture(input_root engine/core/bad.cpp
    "#include \"engine/input/xinput/xinput_gamepad.h\"" fail backend)
check_fixture(app_root engine/core/bad.h
    "#include \"engine/app/win32/keyboard_input.h\"" fail backend)
check_fixture(new_module_root engine/core/bad.cpp
    "#include \"engine/future/platform/helper.h\"" fail backend)
check_fixture(render_leading_dot engine/app/bad.h
    "#include \"./engine/render/d3d11/backend.h\"" fail backend)
check_fixture(render_middle_dot engine/app/bad.cpp
    "#include \"engine/render/./d3d11/backend.h\"" fail backend)
check_fixture(render_parent_collapse engine/app/bad.h
    "#include <engine/render/d3d11/../d3d12/backend.h>" fail backend)
check_fixture(render_nested_header engine/app/bad.cpp
    "#include \"engine/render/d3d11/detail/helper.h\"" fail backend)
check_fixture(relative_sibling engine/render/d3d11/bad.cpp
    "#include \"../d3d12/backend.h\"" fail backend render/d3d12)
check_fixture(relative_shell engine/app/bad.h
    "#include \"../render/d3d11/backend.h\"" fail backend render/d3d11)
check_fixture(relative_collapse engine/app/bad.cpp
    "#include \"../render/d3d11/../d3d12/backend.h\"" fail backend render/d3d12)
check_fixture(relative_leading_dot engine/render/bad.h
    "#include \"./d3d11/backend.h\"" fail backend render/d3d11)
check_fixture(relative_generated_header engine/render/d3d11/bad.cpp
    "#include \"../d3d12/sprite_vertex_shader.h\"" fail backend render/d3d12)
check_fixture(relative_new_module engine/core/bad.h
    "#include \"../future/platform/generated.h\"" fail backend future/platform)

check_fixture(same_backend_root engine/render/d3d11/allowed.cpp
    "#include \"engine/render/d3d11/backend.h\"" pass none)
check_fixture(same_nested_header engine/render/d3d11/allowed.h
    "#include \"engine/render/d3d11/detail/helper.h\"" pass none)
check_fixture(same_nested_source engine/render/d3d11/detail/allowed.cpp
    "#include \"engine/render/d3d11/backend.h\"" pass none)
check_fixture(same_nested_relative engine/render/d3d11/detail/allowed.h
    "#include \"../backend.h\"" pass none)
check_fixture(same_generated_header engine/render/d3d11/allowed.cpp
    "#include \"engine/render/d3d11/sprite_vertex_shader.h\"" pass none)
check_fixture(same_dotted_root_backend engine/render/d3d11/allowed.cpp
    "#include \"./engine/render/d3d11/backend.h\"" pass none)
check_fixture(same_audio engine/audio/null/allowed.cpp
    "#include \"engine/audio/null/recording.h\"" pass none)
check_fixture(same_app engine/app/win32/allowed.cpp
    "#include \"engine/app/win32/keyboard_input.h\"" pass none)
check_fixture(same_input engine/input/web/allowed.cpp
    "#include \"engine/input/web/standard_gamepad.h\"" pass none)
check_fixture(same_new_module engine/future/platform/detail/allowed.h
    "#include \"engine/future/platform/helper.h\"" pass none)
check_fixture(backend_prefix engine/render/d3d11_extra/bad.cpp
    "#include \"engine/render/d3d11/backend.h\"" fail backend)
check_fixture(module_prefix engine/render_extra/d3d11/bad.h
    "#include \"engine/render/d3d11/backend.h\"" fail backend)

check_fixture(vendor_three_components engine/assets/allowed.cpp
    "#include <rapidjson/error/en.h>" pass none)
check_fixture(vendor_dotted_repo engine/assets/allowed.cpp
    "#include \"./external/rapidjson/include/rapidjson/error/en.h\"" pass none)
check_fixture(vendor_dotted_include engine/assets/allowed.h
    "#include <./rapidjson/error/en.h>" pass none)
check_fixture(vendor_two_components engine/render/d3d11/allowed.cpp
    "#include <DirectXTK/SpriteBatch.h>" pass none)
check_fixture(shared_root_headers engine/app/allowed.h
    "#include \"engine/render/renderer.h\"\n#include \"engine/audio/audio_device.h\"" pass none)
check_fixture(shared_dotted_root engine/app/allowed.h
    "#include \"./engine/math/vector2.h\"" pass none)
check_fixture(shared_collapsed_root engine/core/allowed.cpp
    "#include \"./engine/../engine/audio/audio_device.h\"" pass none)
check_fixture(line_comment engine/core/allowed.cpp
    "// #include \"game/objects/level.h\"" pass none)
check_fixture(macro_operand engine/core/allowed.h
    "#include GAME_HEADER" pass none)
check_fixture(inactive_branch engine/app/bad.cpp
    "#if 0\n#include \"engine/render/d3d11/backend.h\"\n#endif" fail backend)
check_fixture(other_target_extension engine/app/allowed.h
    "#include \"engine/render/d3d11/backend.h.extra\"" pass none)

if(selected_count EQUAL 0)
    message(FATAL_ERROR "Unknown engine include fixture CASE: ${CASE}")
endif()
message(STATUS "Engine include-wall fixtures passed: ${selected_count} cases at ${fixture}")
