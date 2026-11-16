# vcpkg may choose its host triplet without publishing VCPKG_HOST_TRIPLET to
# CMake. Respect the explicit and environment selections, then use its native
# architecture/OS default rather than a cross-build's target triplet.
function(labrador_material_host_triplet result)
    if(VCPKG_HOST_TRIPLET)
        set(host_triplet "${VCPKG_HOST_TRIPLET}")
    elseif(NOT "$ENV{VCPKG_DEFAULT_HOST_TRIPLET}" STREQUAL "")
        set(host_triplet "$ENV{VCPKG_DEFAULT_HOST_TRIPLET}")
    else()
        string(TOLOWER "${CMAKE_HOST_SYSTEM_PROCESSOR}" architecture)
        if(architecture MATCHES "^(amd64|x86_64|x64)$")
            set(architecture x64)
        elseif(architecture MATCHES "^(aarch64|arm64)$")
            set(architecture arm64)
        elseif(architecture MATCHES "^(i[3-6]86|x86)$")
            set(architecture x86)
        endif()
        if(CMAKE_HOST_WIN32)
            set(host_system windows)
        elseif(CMAKE_HOST_APPLE)
            set(host_system osx)
        else()
            string(TOLOWER "${CMAKE_HOST_SYSTEM_NAME}" host_system)
        endif()
        set(host_triplet "${architecture}-${host_system}")
    endif()
    set(${result} "${host_triplet}" PARENT_SCOPE)
endfunction()

# labrador_add_material(<target> <name> SOURCE <pixel.hlsl>
#     [DEPENDS <included.hlsli>...])
#
# The source defines float4 material(MaterialInput input). The engine's frame
# supplies all registers and the pixel entry point. A game includes the
# generated materials/<name>.h and passes labrador::<name> to add_material.
function(labrador_add_material target name)
    cmake_parse_arguments(PARSE_ARGV 2 MATERIAL "" "SOURCE" "DEPENDS")
    if(NOT TARGET ${target})
        message(FATAL_ERROR "labrador_add_material: no target named ${target}")
    endif()
    if(NOT name MATCHES "^[a-z][a-z0-9_]*$")
        message(FATAL_ERROR "labrador_add_material: ${name} must be a snake_case C++ name")
    endif()
    if(NOT MATERIAL_SOURCE OR MATERIAL_UNPARSED_ARGUMENTS OR MATERIAL_KEYWORDS_MISSING_VALUES)
        message(FATAL_ERROR "labrador_add_material(${target} ${name}) needs SOURCE <file> and optional DEPENDS <files>")
    endif()
    get_filename_component(source "${MATERIAL_SOURCE}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    if(NOT EXISTS "${source}")
        message(FATAL_ERROR "labrador_add_material: source does not exist: ${source}")
    endif()
    get_property(source_root GLOBAL PROPERTY LABRADOR_SOURCE_ROOT)
    get_property(generated_root GLOBAL PROPERTY LABRADOR_GENERATED_ROOT)
    set(frame "${source_root}/engine/render/post_frame.hlsli")
    set(embed_script "${source_root}/cmake/embed_material.cmake")
    set(output "${generated_root}/materials/${name}.h")
    set(work "${CMAKE_CURRENT_BINARY_DIR}/materials/${name}")
    file(MAKE_DIRECTORY "${work}")
    set(wrapper "${work}/pixel.hlsl")
    # configure_file leaves an unchanged wrapper's timestamp alone.
    set(wrapper_contents "#include \"${frame}\"\n#include \"${source}\"\n")
    file(CONFIGURE OUTPUT "${wrapper}" CONTENT "${wrapper_contents}" @ONLY)
    set(dependencies "${source}" "${frame}" "${wrapper}" "${embed_script}")
    set(included_files "")
    foreach(dependency IN LISTS MATERIAL_DEPENDS)
        get_filename_component(dependency_path "${dependency}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        list(APPEND dependencies "${dependency_path}")
        list(APPEND included_files "${dependency_path}")
    endforeach()
    set(signature "${source};${included_files}")
    get_property(already_added GLOBAL PROPERTY "LABRADOR_MATERIAL_${name}")
    if(already_added)
        if(NOT already_added STREQUAL signature)
            message(FATAL_ERROR "labrador_add_material: material ${name} already names another source or dependency list")
        endif()
        add_dependencies(${target} labrador_material_${name})
        labrador_add_generated_shader(${target} "${output}")
        return()
    endif()
    set_property(GLOBAL PROPERTY "LABRADOR_MATERIAL_${name}" "${signature}")

    if(LABRADOR_RENDER_BACKEND STREQUAL "null")
        add_custom_command(OUTPUT "${output}"
            COMMAND "${CMAKE_COMMAND}" "-DNAME=${name}" -DKIND=null
                "-DOUTPUT=${output}" -P "${embed_script}"
            DEPENDS ${dependencies}
            COMMENT "Embedding ${name} (null material)" VERBATIM)
    elseif(LABRADOR_RENDER_BACKEND MATCHES "^d3d(11|12)$")
        find_program(FXC_EXECUTABLE NAMES fxc NO_CMAKE_FIND_ROOT_PATH)
        if(NOT FXC_EXECUTABLE)
            message(FATAL_ERROR "Material ${name} needs fxc from the Windows SDK. Configure from the x64 Native Tools Command Prompt.")
        endif()
        if(LABRADOR_RENDER_BACKEND STREQUAL "d3d11")
            set(profile ps_4_0)
        else()
            set(profile ps_5_1)
        endif()
        set(binary "${work}/pixel.dxbc")
        add_custom_command(OUTPUT "${output}"
            COMMAND "${FXC_EXECUTABLE}" /nologo /T ${profile}
                /E post_pixel_main /Fo "${binary}" "${wrapper}"
            COMMAND "${CMAKE_COMMAND}" "-DNAME=${name}" "-DKIND=${LABRADOR_RENDER_BACKEND}"
                "-DINPUT=${binary}" "-DOUTPUT=${output}" -P "${embed_script}"
            DEPENDS ${dependencies} BYPRODUCTS "${binary}"
            COMMENT "Compiling ${name} (${profile})" VERBATIM)
    elseif(LABRADOR_RENDER_BACKEND STREQUAL "vulkan" OR LABRADOR_RENDER_BACKEND STREQUAL "gl")
        # Host tools, including during an Emscripten cross-build. A caller
        # may name an installation explicitly; a Vulkan SDK is a fallback.
        labrador_material_host_triplet(host_triplet)
        set(host_tools "${VCPKG_INSTALLED_DIR}/${host_triplet}/tools")
        find_program(LABRADOR_MATERIAL_DXC NAMES dxc
            HINTS "${host_tools}/directx-dxc" "$ENV{VULKAN_SDK}/Bin"
            NO_CMAKE_FIND_ROOT_PATH
            DOC "DXC with SPIR-V support, for authored post materials")
        if(NOT LABRADOR_MATERIAL_DXC)
            message(FATAL_ERROR "Material ${name} needs DXC with SPIR-V support. Enable the vcpkg materials feature (-DVCPKG_MANIFEST_FEATURES=materials), install the Vulkan SDK, or set LABRADOR_MATERIAL_DXC to dxc.")
        endif()
        set(binary "${work}/pixel.spv")
        set(compile_spirv "${LABRADOR_MATERIAL_DXC}" -nologo -spirv
            -T ps_6_0 -E post_pixel_main -fvk-b-shift 0 0
            -fvk-t-shift 1 0 -fvk-s-shift 5 0 -Fo "${binary}" "${wrapper}")
        if(LABRADOR_RENDER_BACKEND STREQUAL "vulkan")
            add_custom_command(OUTPUT "${output}"
                COMMAND ${compile_spirv}
                COMMAND "${CMAKE_COMMAND}" "-DNAME=${name}" -DKIND=spirv
                    "-DINPUT=${binary}" "-DOUTPUT=${output}" -P "${embed_script}"
                DEPENDS ${dependencies} BYPRODUCTS "${binary}"
                COMMENT "Compiling ${name} (SPIR-V)" VERBATIM)
        else()
            find_program(LABRADOR_SPIRV_CROSS NAMES spirv-cross
                HINTS "${host_tools}/spirv-cross" "$ENV{VULKAN_SDK}/Bin"
                NO_CMAKE_FIND_ROOT_PATH DOC "SPIRV-Cross, for authored post materials")
            if(NOT LABRADOR_SPIRV_CROSS)
                message(FATAL_ERROR "Material ${name} needs spirv-cross. Enable the vcpkg materials feature (-DVCPKG_MANIFEST_FEATURES=materials), install the Vulkan SDK, or set LABRADOR_SPIRV_CROSS to spirv-cross.")
            endif()
            if(EMSCRIPTEN)
                set(dialect --es --version 300)
            else()
                set(dialect --no-es --version 330)
            endif()
            set(glsl "${work}/pixel.glsl")
            add_custom_command(OUTPUT "${output}"
                COMMAND ${compile_spirv}
                COMMAND "${LABRADOR_SPIRV_CROSS}" "${binary}" ${dialect}
                    --flatten-ubo --rename-interface-variable in 0 post_uv --output "${glsl}"
                COMMAND "${CMAKE_COMMAND}" "-DNAME=${name}" -DKIND=glsl
                    "-DINPUT=${glsl}" "-DOUTPUT=${output}" -P "${embed_script}"
                DEPENDS ${dependencies} BYPRODUCTS "${binary}" "${glsl}"
                COMMENT "Compiling ${name} (GLSL)" VERBATIM)
        endif()
    else()
        message(FATAL_ERROR "labrador_add_material: unsupported render backend ${LABRADOR_RENDER_BACKEND}")
    endif()
    add_custom_target(labrador_material_${name} DEPENDS "${output}")
    add_dependencies(${target} labrador_material_${name})
    labrador_add_generated_shader(${target} "${output}")
endfunction()
