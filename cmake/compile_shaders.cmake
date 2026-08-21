# Shader compilation, at build time.
#
# WHY BUILD TIME AND NOT RUN TIME. Compiling HLSL in the process needs
# D3DCompiler_47.dll, which is a redistributable rather than a guaranteed part
# of Windows, and it moves a failure that belongs to the build - a typo in a
# shader - into the first frame of a shipped game. T5's preference is the same
# one that made the backend a compile-time choice: fail at build, not at run.
#
# WHY NOT CHECKED-IN BYTECODE. A generated artifact in the tree is a file that
# can disagree with its source and nothing notices. fxc is in every Windows SDK
# and on the PATH of every environment that can build this project at all,
# which is the same bet the repository already makes on cl.exe.
#
# WHAT COMES OUT is a C header holding a byte array, which the backend includes
# and hands to whatever its API wants bytecode in - CreateVertexShader on D3D11,
# a field of D3D12_GRAPHICS_PIPELINE_STATE_DESC on D3D12, which has no such
# call. No file is read at run time and nothing has to be deployed beside the
# executable.

# compile_hlsl(<target> <source> <profile> <entry> <symbol> <output>)
#
# `symbol` is the name of the byte array in the generated header, and `output`
# is that header's path relative to the generated include root - so a backend
# includes it the way it includes anything else, from the repository root
# (CONVENTIONS).
function(compile_hlsl target source profile entry symbol output)
    # Looked for here rather than when this file is included, so that a build
    # of a backend that has no HLSL in it does not require a Direct3D shader
    # compiler to be present. find_program caches, so the second call is free.
    find_program(FXC_EXECUTABLE
        NAMES fxc
        DOC "The Direct3D shader compiler, from the Windows SDK"
        REQUIRED
    )

    set(generated_root "${CMAKE_BINARY_DIR}/generated")
    set(generated_file "${generated_root}/${output}")

    get_filename_component(generated_directory "${generated_file}" DIRECTORY)
    file(MAKE_DIRECTORY "${generated_directory}")

    add_custom_command(
        OUTPUT "${generated_file}"
        COMMAND "${FXC_EXECUTABLE}"
            /nologo
            /T ${profile}
            /E ${entry}
            /Fh "${generated_file}"
            /Vn ${symbol}
            "${source}"
        DEPENDS "${source}"
        COMMENT "Compiling ${entry} (${profile}) from ${source}"
        VERBATIM
    )

    target_sources(${target} PRIVATE "${generated_file}")
    target_include_directories(${target} PRIVATE "${generated_root}")
endfunction()

# compile_hlsl_to_spirv(<target> <source> <profile> <entry> <symbol> <output>)
#
# The same source, the same generated shape, a different compiler and one more
# decision. sprite.hlsl serves a third backend unchanged - the Vulkan one - and
# what that costs is written here rather than in the shader, which is what
# that file promises: a backend owns the profile and how it binds b0, and
# neither reaches the source.
#
# WHY dxc AND NOT glslc. The source is HLSL, character for character the same
# file the two Direct3D backends compile. glslc compiles GLSL, so reaching for
# it would mean a second copy of the shader in a second language - which is
# what render/gl/sprite_shader.h costs and says it costs. dxc emits SPIR-V from
# the HLSL that is already here.
#
# IT IS LOOKED FOR IN THE VULKAN SDK AND NOWHERE ELSE, WHICH IS NOT FUSSINESS.
# There are two dxc.exe on a normal Windows developer's machine. The Windows
# SDK ships one beside fxc, and after vcvars64 that is the one PATH finds
# first; it lists every -spirv flag in its own help text and then answers
#
#     dxc failed : SPIR-V CodeGen not available.
#                  Please recompile with -DENABLE_SPIRV_CODEGEN=ON.
#
# because the backend was never compiled into it. A find_program that searched
# PATH would pick that one on the machine this was written on. So the hint is
# the Vulkan SDK's own Bin, NO_DEFAULT_PATH, and the error below names
# VULKAN_SDK rather than "dxc" - a build that cannot find a SPIR-V compiler
# should say which one it wanted (T6).
#
# THE THREE SHIFTS ARE THE BINDING, AND THEY BELONG HERE. HLSL has a register
# space per resource kind - b for constant buffers, t for textures, s for
# samplers - and Vulkan has one binding number per descriptor set. So b0, t0
# and s0 all arrive at set 0, binding 0, which is three resources in one slot
# and a descriptor set layout that cannot be written. The shifts spread them to
# 0, 1 and 2. This is the same decision D3D12 makes by handing b0 four root
# constants where D3D11 gives it a constant buffer, and it is made in the same
# place: engine/CMakeLists.txt picks the profile, this function fixes the
# binding, and sprite.hlsl says neither.
function(compile_hlsl_to_spirv target source profile entry symbol output)
    # Looked for at the first call rather than when this file is included, so a
    # build of a backend with no SPIR-V in it needs no Vulkan SDK. find_program
    # caches, so the second call is free.
    find_program(DXC_SPIRV_EXECUTABLE
        NAMES dxc
        HINTS "$ENV{VULKAN_SDK}/Bin"
        NO_DEFAULT_PATH
        DOC "The DirectX shader compiler from the Vulkan SDK, which emits SPIR-V"
    )
    if(NOT DXC_SPIRV_EXECUTABLE)
        message(FATAL_ERROR
            "No SPIR-V compiler. This backend compiles engine/render/sprite.hlsl "
            "to SPIR-V with the Vulkan SDK's dxc, which is looked for in "
            "$ENV{VULKAN_SDK}/Bin and deliberately not on PATH - the Windows "
            "SDK ships a dxc.exe of the same name that PATH finds first and "
            "that has no SPIR-V backend compiled in. Install the Vulkan SDK "
            "(https://vulkan.lunarg.com/) so that VULKAN_SDK is set.")
    endif()

    set(generated_root "${CMAKE_BINARY_DIR}/generated")
    set(generated_file "${generated_root}/${output}")

    get_filename_component(generated_directory "${generated_file}" DIRECTORY)
    file(MAKE_DIRECTORY "${generated_directory}")

    add_custom_command(
        OUTPUT "${generated_file}"
        COMMAND "${DXC_SPIRV_EXECUTABLE}"
            -nologo
            -spirv
            -T ${profile}
            -E ${entry}
            -fvk-b-shift 0 0
            -fvk-t-shift 1 0
            -fvk-s-shift 2 0
            -Fh "${generated_file}"
            -Vn ${symbol}
            "${source}"
        DEPENDS "${source}"
        COMMENT "Compiling ${entry} (${profile}, SPIR-V) from ${source}"
        VERBATIM
    )

    target_sources(${target} PRIVATE "${generated_file}")
    target_include_directories(${target} PRIVATE "${generated_root}")
endfunction()
