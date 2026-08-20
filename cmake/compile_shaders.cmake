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
