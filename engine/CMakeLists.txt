add_subdirectory(math)

# LabradorEngine — the engine static library (docs/design/ARCHITECTURE.md).
# Modules are folders; dependencies point one way, toward math. Files
# migrate in from the game as they become dependency-clean.

# xaudio2redist first: directxtk's config references Microsoft::XAudio2Redist
# but does not find it itself.
find_package(xaudio2redist CONFIG REQUIRED)
find_package(directxtk CONFIG REQUIRED)

add_library(LabradorEngine STATIC
    app/application.cpp
    app/window.cpp
    assets/asset_manifest_loader.cpp
    assets/json.cpp
    assets/resource_loader.cpp
    assets/sound_bank_loader.cpp
    assets/sprite_sheet_loader.cpp
    audio/audio_resources.cpp
    audio/sound_bank.cpp
    audio/sound_bank_object.cpp
    collision/broad_phase.cpp
    collision/contacts.cpp
    collision/narrow_phase.cpp
    collision/partitioner.cpp
    collision/resolve.cpp
    core/byte_reader.cpp
    core/moving_object.cpp
    core/state.cpp
    core/state_context.cpp
    core/thread_pool.cpp
    input/direction.cpp
    input/gamepad.cpp
    input/gamepads.cpp
    input/keyboard.cpp
    input/mouse.cpp
    input/xinput/gamepad_reader.cpp
    render/animation_object.cpp
    render/animation_strip.cpp
    render/camera.cpp
    render/camera_tools.cpp
    render/colour.cpp
    render/dds_file.cpp
    render/draw_object.cpp
    render/font.cpp
    render/label.cpp
    render/render_resources.cpp
    render/resolution_manager.cpp
    render/resource_factory.cpp
    render/sprite_font_file.cpp
    render/sprite_frame.cpp
    render/sprite_geometry.cpp
    render/sprite_sheet.cpp
    render/sprite_sheet_object.cpp
    render/text.cpp
    render/text_drop_shadow.cpp
    render/text_encoding.cpp
    render/text_object.cpp
    render/texture_data.cpp
    render/texture_object.cpp
    render/viewport.cpp
    render/viewport_manager.cpp
    render/visual.cpp
    scene/scene.cpp
    ui/button.cpp
    ui/focus.cpp
    ui/navigation.cpp
    ui/widget.cpp
)
# The include root, relative to this file and not to the top-level project.
# CMAKE_SOURCE_DIR is whoever ran cmake, so when the engine is consumed as a
# submodule - which is now the actual repo topology, not a planned one - it
# points at the *consumer's* root and every "engine/..." include stops
# resolving. CMAKE_CURRENT_SOURCE_DIR is this directory whatever is above it.
#
# It is the directory *above* engine/ because includes are written from the
# repository root (CONVENTIONS, Files), so an engine file says
# "engine/render/renderer.h" and means it. This is now also the wall
# ARCHITECTURE describes: nothing named game/ sits beside engine/ any more, so
# an engine file including a game header fails in the compiler. The
# check_engine_includes target still runs on every build, because that is only
# true when this repository is built standalone - a consumer builds it from a
# tree that may well have a game/ in it.
target_include_directories(LabradorEngine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")

# The backend, chosen at build time.
#
# ONE IMPLEMENTATION, SELECTED HERE, AND NOT AN INTERFACE. renderer.h says why:
# a virtual call per sprite is the frame-loop tax T8 refuses, and a compile-time
# choice fails at link rather than at run time (T5). This is where that choice
# is made, and it is the whole of what selecting a backend involves - three
# translation units, at most one more for the part of the API that is not about
# drawing, and whatever that backend needs to build its shader.
#
# Nothing outside engine/render/<backend>/ names either folder, which is what
# makes this a list of files rather than a set of #ifdefs.
set(LABRADOR_RENDER_BACKEND "d3d11" CACHE STRING
    "The render backend to build: d3d11, d3d12, gl, vulkan or null")
set_property(CACHE LABRADOR_RENDER_BACKEND PROPERTY STRINGS
    d3d11 d3d12 gl vulkan null)

if(LABRADOR_RENDER_BACKEND STREQUAL "d3d11")
    target_sources(LabradorEngine PRIVATE
        render/d3d11/device_resources.cpp
        render/d3d11/render_resources.cpp
        render/d3d11/renderer.cpp
        render/d3d11/texture_factory.cpp
    )
    target_link_libraries(LabradorEngine PUBLIC d3d11 dxgi dxguid)

    # Its two shaders, compiled to byte arrays this build writes and the
    # backend includes (cmake/compile_shaders.cmake). The generated headers
    # land under engine/render/d3d11/ in the build tree, so renderer.cpp
    # includes them from the repository root like everything else - and
    # check_engine_includes.cmake's rule about who may include a backend
    # header reads exactly the same for a generated one.
    #
    # THE SOURCE IS render/sprite.hlsl AND THE PROFILE IS THIS BACKEND'S, which
    # is the whole of the split. Two backends compile that one file and neither
    # owns it; what each owns is the profile it asks for and the header the
    # bytes land in. That file says why it is not two files.
    compile_hlsl(LabradorEngine
        "${CMAKE_CURRENT_SOURCE_DIR}/render/sprite.hlsl"
        vs_4_0_level_9_1 vertex_main SPRITE_VERTEX_SHADER
        "engine/render/d3d11/sprite_vertex_shader.h")
    compile_hlsl(LabradorEngine
        "${CMAKE_CURRENT_SOURCE_DIR}/render/sprite.hlsl"
        ps_4_0_level_9_1 pixel_main SPRITE_PIXEL_SHADER
        "engine/render/d3d11/sprite_pixel_shader.h")
elseif(LABRADOR_RENDER_BACKEND STREQUAL "d3d12")
    target_sources(LabradorEngine PRIVATE
        render/d3d12/device_resources.cpp
        render/d3d12/render_resources.cpp
        render/d3d12/renderer.cpp
        render/d3d12/texture_factory.cpp
    )
    target_link_libraries(LabradorEngine PUBLIC d3d12 dxgi dxguid)

    # The same source as the backend above, at the lowest profile this API has.
    # Shader model 5 is where D3D12 starts - there is no 4_0_level_9_1 to ask
    # for - and 5_1 is the one fxc emits for it. The generated headers land
    # under engine/render/d3d12/, so the two backends never share a byte array
    # even though they share every line that produced it.
    compile_hlsl(LabradorEngine
        "${CMAKE_CURRENT_SOURCE_DIR}/render/sprite.hlsl"
        vs_5_1 vertex_main SPRITE_VERTEX_SHADER
        "engine/render/d3d12/sprite_vertex_shader.h")
    compile_hlsl(LabradorEngine
        "${CMAKE_CURRENT_SOURCE_DIR}/render/sprite.hlsl"
        ps_5_1 pixel_main SPRITE_PIXEL_SHADER
        "engine/render/d3d12/sprite_pixel_shader.h")
elseif(LABRADOR_RENDER_BACKEND STREQUAL "gl")
    target_sources(LabradorEngine PRIVATE
        render/gl/gl_functions.cpp
        render/gl/render_resources.cpp
        render/gl/renderer.cpp
        render/gl/texture_factory.cpp
    )
    target_link_libraries(LabradorEngine PUBLIC opengl32)

    # No shader step. GL 3.3 has no offline shader format, so the GLSL ships
    # as source in render/gl/sprite_shader.h and the driver compiles it at
    # device creation - which that file explains at length, because it is the
    # one place a backend genuinely differs in kind rather than in spelling
    # from the two that compile HLSL above.
elseif(LABRADOR_RENDER_BACKEND STREQUAL "vulkan")
    target_sources(LabradorEngine PRIVATE
        render/vulkan/device_resources.cpp
        render/vulkan/render_resources.cpp
        render/vulkan/renderer.cpp
        render/vulkan/texture_factory.cpp
    )

    # THE ONLY BACKEND WHOSE DEPENDENCY IS NOT ALREADY ON THE MACHINE, and
    # docs/port/android.md said so before it was written: fxc and the two
    # Direct3D libraries come with the Visual Studio install and opengl32 comes
    # with Windows, where this needs the Vulkan SDK for its headers, its import
    # library and - the part that actually cost something - a dxc that can emit
    # SPIR-V. CMake's own FindVulkan reads VULKAN_SDK, which is the variable the
    # SDK's installer sets, so a machine that has it needs no further
    # configuration and one that does not fails here by name rather than at the
    # first missing symbol (T6).
    # PRIVATE, WHICH IS THE ONE THING THE INCLUDE CHECK CANNOT SEE. Linking it
    # PUBLIC put the SDK's include directory on the command line of every
    # sample, every test and every downstream client - the only backend whose
    # link line carried a header path - so a file outside
    # engine/render/vulkan/ could have said #include <vulkan/vulkan.h> and
    # compiled. cmake/check_engine_includes.cmake guards the folder, not the
    # API, and would not have caught it. Nothing outside the backend names a
    # Vulkan type: the shell hands its window handle to create_device as a
    # void*.
    find_package(Vulkan REQUIRED)
    target_link_libraries(LabradorEngine PRIVATE Vulkan::Vulkan)

    # The same source as the three backends above, through a third compiler.
    # cmake/compile_shaders.cmake carries the whole argument - why dxc and not
    # glslc, which of the two dxc.exe on a Windows machine has a SPIR-V backend
    # compiled into it, and what the three register shifts are for. Shader model
    # 6.0 because that is where dxc starts; there is no lower profile to ask
    # for, exactly as there was none below 5_1 for D3D12.
    compile_hlsl_to_spirv(LabradorEngine
        "${CMAKE_CURRENT_SOURCE_DIR}/render/sprite.hlsl"
        vs_6_0 vertex_main SPRITE_VERTEX_SHADER
        "engine/render/vulkan/sprite_vertex_shader.h")
    compile_hlsl_to_spirv(LabradorEngine
        "${CMAKE_CURRENT_SOURCE_DIR}/render/sprite.hlsl"
        ps_6_0 pixel_main SPRITE_PIXEL_SHADER
        "engine/render/vulkan/sprite_pixel_shader.h")
elseif(LABRADOR_RENDER_BACKEND STREQUAL "null")
    target_sources(LabradorEngine PRIVATE
        render/null/render_resources.cpp
        render/null/renderer.cpp
        render/null/texture_factory.cpp
    )

    # Nothing to link and nothing to compile. This backend records what it was
    # asked to draw and never draws it, so it needs no graphics library, no
    # shader and no display - which is what makes it the one configuration a
    # build machine can run from end to end.
else()
    message(FATAL_ERROR
        "LABRADOR_RENDER_BACKEND is '${LABRADOR_RENDER_BACKEND}'. "
        "It must be d3d11, d3d12, gl, vulkan or null - a backend is a folder "
        "in engine/render/ holding renderer.cpp, render_resources.cpp and "
        "texture_factory.cpp.")
endif()

# rapidjson is PRIVATE and SYSTEM. It used to be PUBLIC, so a sample that
# parses nothing still had a JSON parser's headers on its command line, and
# their warnings were measured against /W4 /WX.
#
# PRIVATE now means what it says: assets/json.h hands out JsonDocument and
# JsonValue, whose only rapidjson is a void* recovered inside json.cpp, so no
# engine header names the parser and no client needs it to include one. A
# client that parses files of its own asks for rapidjson on its own account -
# which the game does, for the save file it writes.
target_include_directories(LabradorEngine SYSTEM PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../external/rapidjson/include"
)
target_link_libraries(LabradorEngine
    PUBLIC MattMath Microsoft::DirectXTK
    PRIVATE labrador_settings
)
