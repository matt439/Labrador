add_subdirectory(math)

# LabradorEngine — the engine static library (docs/design/ARCHITECTURE.md).
# Modules are folders; dependencies point one way, toward math. A game is its
# own repository and consumes this one as a submodule (AGENTS.md,
# ARCHITECTURE.md).

add_library(LabradorEngine STATIC
    app/application.cpp
    app/content_root.cpp
    app/frame_timer.cpp
    assets/asset_manifest_loader.cpp
    assets/json.cpp
    assets/resource_loader.cpp
    assets/sound_bank_loader.cpp
    assets/sprite_sheet_loader.cpp
    audio/audio_resources.cpp
    audio/sound_bank.cpp
    audio/xwb_file.cpp
    collision/broad_phase.cpp
    collision/contacts.cpp
    collision/narrow_phase.cpp
    collision/partitioner.cpp
    collision/resolve.cpp
    core/byte_reader.cpp
    core/file_path.cpp
    core/random.cpp
    core/state.cpp
    core/state_context.cpp
    core/thread_pool.cpp
    input/direction.cpp
    input/gamepad.cpp
    input/gamepads.cpp
    input/keyboard.cpp
    input/mouse.cpp
    input/touch.cpp
    render/animation_strip.cpp
    render/block_decode.cpp
    render/camera.cpp
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
# submodule it points at the *consumer's* root and every "engine/..." include stops
# resolving. CMAKE_CURRENT_SOURCE_DIR is this directory whatever is above it.
#
# It is the directory *above* engine/ because includes are written from the
# repository root (CONVENTIONS, Files), so an engine file says
# "engine/render/renderer.h" and means it. This is also the wall ARCHITECTURE
# describes: nothing named game/ sits beside engine/, so an engine file
# including a game header fails in the compiler. The
# check_engine_includes target still runs on every build, because that is only
# true when this repository is built standalone - a consumer builds it from a
# tree that may well have a game/ in it.
target_include_directories(LabradorEngine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")

# The shell's window, which is the platform's: app/win32/ or app/web/, behind
# the one neutral app/window.h (its header says why the class is not in either
# folder). Chosen by the platform being built for rather than by a cache
# variable like the three backends below, because neither half can be built
# for the other's platform - there is no choice to make.
if(EMSCRIPTEN)
    target_sources(LabradorEngine PRIVATE
        app/web/dom_events.cpp
        app/web/window.cpp
    )
elseif(WIN32)
    target_sources(LabradorEngine PRIVATE
        app/win32/pointer_input.cpp
        app/win32/system_command.cpp
        app/win32/window.cpp
    )
else()
    message(FATAL_ERROR
        "There is no window for this platform. engine/app/ has a Win32 "
        "window and a browser one, and a third is a folder beside them.")
endif()

# The backend, chosen at build time.
#
# ONE IMPLEMENTATION, SELECTED HERE, AND NOT AN INTERFACE. renderer.h says why:
# a virtual call per sprite is the frame-loop tax T8 refuses, and a compile-time
# choice fails at link rather than at run time (T5). This is where that choice
# is made, and it is the whole of what selecting a backend involves - three
# translation units, at most one more for the part of the API that is not about
# drawing (for gl/, one of its two context files, chosen by platform below;
# docs/port/web.md, D1), and whatever that backend needs to build its shader.
#
# Nothing outside engine/render/<backend>/ names any of the five folders, which
# is what makes this a list of files rather than a set of #ifdefs.
#
# gl IS THE DEFAULT IN A BROWSER BUILD, as webaudio is for audio below: WebGL2
# is the only graphics API a page has, so a browser preset that names no
# backend gets the one that draws there. The three whose API a browser does
# not have are refused there by name, because asking for one would otherwise
# surface as a missing header or library long after configure (T5, T6).
if(EMSCRIPTEN)
    set(labrador_default_render_backend "gl")
else()
    set(labrador_default_render_backend "d3d11")
endif()
set(LABRADOR_RENDER_BACKEND "${labrador_default_render_backend}" CACHE STRING
    "The render backend to build: d3d11, d3d12, gl, vulkan or null")
set_property(CACHE LABRADOR_RENDER_BACKEND PROPERTY STRINGS
    d3d11 d3d12 gl vulkan null)

if(EMSCRIPTEN AND LABRADOR_RENDER_BACKEND MATCHES "^(d3d11|d3d12|vulkan)$")
    message(FATAL_ERROR
        "LABRADOR_RENDER_BACKEND is ${LABRADOR_RENDER_BACKEND} in a browser "
        "build (Emscripten), which has neither Direct3D nor Vulkan. It must "
        "be gl, which draws through WebGL2 there and is the default, or null.")
endif()

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
    # is the whole of the split. Three backends compile that one file and none
    # of them owns it - d3d11 here at 4_0_level_9_1, d3d12 below at 5_1, and
    # vulkan at 6_0 through the Vulkan SDK's dxc; what each owns is the profile
    # it asks for and the header the bytes land in. That file says why it is
    # not three files.
    compile_hlsl(LabradorEngine
        "${CMAKE_CURRENT_SOURCE_DIR}/render/sprite.hlsl"
        vs_4_0_level_9_1 vertex_main sprite_vertex_shader
        "engine/render/d3d11/sprite_vertex_shader.h")
    compile_hlsl(LabradorEngine
        "${CMAKE_CURRENT_SOURCE_DIR}/render/sprite.hlsl"
        ps_4_0_level_9_1 pixel_main sprite_pixel_shader
        "engine/render/d3d11/sprite_pixel_shader.h")
elseif(LABRADOR_RENDER_BACKEND STREQUAL "d3d12")
    target_sources(LabradorEngine PRIVATE
        render/d3d12/device_resources.cpp
        render/d3d12/render_resources.cpp
        render/d3d12/renderer.cpp
        render/d3d12/texture_factory.cpp
    )
    target_link_libraries(LabradorEngine PUBLIC d3d12 dxgi dxguid)

    # The same source as the backend above, with 5_1 selected for D3D12.
    # This is a selected shader profile, not an API minimum; the creation-only
    # probe in docs/review/d3d12/STATUS.md records what was measured.
    # The generated headers land
    # under engine/render/d3d12/, so the two backends never share a byte array
    # even though they share every line that produced it.
    compile_hlsl(LabradorEngine
        "${CMAKE_CURRENT_SOURCE_DIR}/render/sprite.hlsl"
        vs_5_1 vertex_main sprite_vertex_shader
        "engine/render/d3d12/sprite_vertex_shader.h")
    compile_hlsl(LabradorEngine
        "${CMAKE_CURRENT_SOURCE_DIR}/render/sprite.hlsl"
        ps_5_1 pixel_main sprite_pixel_shader
        "engine/render/d3d12/sprite_pixel_shader.h")
elseif(LABRADOR_RENDER_BACKEND STREQUAL "gl")
    target_sources(LabradorEngine PRIVATE
        render/gl/render_resources.cpp
        render/gl/renderer.cpp
        render/gl/texture_factory.cpp
    )

    # ONE BACKEND, TWO CONTEXTS, AND A BUILD COMPILES ONE OF THEM
    # (docs/port/web.md, D1): a 3.3 core context through WGL on Windows, and
    # WebGL2 on the page's canvas in a browser. render/gl/backend.h says what
    # each one answers. Chosen by the platform, like the window above, because
    # neither can be built for the other's.
    if(EMSCRIPTEN)
        target_sources(LabradorEngine PRIVATE
            render/gl/web_context.cpp
        )
        # WebGL2 and only WebGL2: GLES 3.0 is the dialect the backend is
        # written to, and a WebGL1 fallback would be a second one. PUBLIC
        # because these are link options, and the link is the executable's.
        target_link_options(LabradorEngine PUBLIC
            -sMIN_WEBGL_VERSION=2
            -sMAX_WEBGL_VERSION=2
        )
    else()
        target_sources(LabradorEngine PRIVATE
            render/gl/wgl_context.cpp
        )
        target_link_libraries(LabradorEngine PUBLIC opengl32)
    endif()

    # No shader step. Neither GL 3.3 nor WebGL2 has an offline shader format,
    # so the GLSL ships as source in render/gl/sprite_shader.h and is compiled
    # at device creation - by the driver on Windows, by the browser in a page -
    # which that file explains at length, because it is the one place a backend
    # genuinely differs in kind rather than in spelling from the two that
    # compile HLSL above.
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
    find_package(Vulkan 1.2 REQUIRED)
    target_link_libraries(LabradorEngine PRIVATE Vulkan::Vulkan)

    # Compile the same sprite.hlsl used by Direct3D to SPIR-V. The helper in
    # cmake/compile_shaders.cmake restricts dxc lookup to the Vulkan SDK and
    # sets the descriptor binding shifts. The selected profiles here are 6_0;
    # this is a build choice, not a measured minimum for the rendering contract.
    compile_hlsl_to_spirv(LabradorEngine
        "${CMAKE_CURRENT_SOURCE_DIR}/render/sprite.hlsl"
        vs_6_0 vertex_main sprite_vertex_shader
        "engine/render/vulkan/sprite_vertex_shader.h")
    compile_hlsl_to_spirv(LabradorEngine
        "${CMAKE_CURRENT_SOURCE_DIR}/render/sprite.hlsl"
        ps_6_0 pixel_main sprite_pixel_shader
        "engine/render/vulkan/sprite_pixel_shader.h")
elseif(LABRADOR_RENDER_BACKEND STREQUAL "null")
    target_sources(LabradorEngine PRIVATE
        render/null/render_resources.cpp
        render/null/renderer.cpp
        render/null/texture_factory.cpp
    )

    # Nothing to link and nothing to compile. This backend records what it was
    # asked to draw and never draws it, so it needs no graphics library, no
    # shader and no display - which is what makes this the backend a build
    # machine can run from end to end.
else()
    message(FATAL_ERROR
        "LABRADOR_RENDER_BACKEND is '${LABRADOR_RENDER_BACKEND}'. "
        "It must be d3d11, d3d12, gl, vulkan or null - a backend is a folder "
        "in engine/render/ holding renderer.cpp, render_resources.cpp and "
        "texture_factory.cpp.")
endif()

# The audio backend, chosen the same way and for the same reasons.
#
# A SECOND AXIS, NOT A SECOND VALUE ON THE FIRST ONE. Which audio API a build
# opens has nothing to do with which graphics API it draws with, so they are two
# cache variables and a preset sets whichever it means. What ties them in
# practice is the debug/release `-null` pair, which sets both: those presets are
# the configurations a machine with no hardware at all can run end to end, and
# audio was the half of that claim which was not true.
#
# ONE TRANSLATION UNIT PER BACKEND, which is the whole of what selecting one
# involves - an audio API is a smaller surface than a graphics API, and
# engine/audio/audio_device.h is exactly as wide as what the thing does. The
# null backend adds recording.h beside its own, exactly as engine/render/null/
# does, so a test can read back what it was asked to play.
#
# webaudio IS THE DEFAULT IN A BROWSER BUILD, as web is for input: it is the
# only one of the three that makes a sound there. Under Node, where a browser
# build's tests run, it finds no Web Audio and plays nothing while still
# reading and checking every bank, so the default costs the tests nothing. Each
# platform API is refused where it does not exist, because asking for one is a
# missing header there rather than an answer.
if(EMSCRIPTEN)
    set(labrador_default_audio_backend "webaudio")
else()
    set(labrador_default_audio_backend "xaudio2")
endif()
set(LABRADOR_AUDIO_BACKEND "${labrador_default_audio_backend}" CACHE STRING
    "The audio backend to build: xaudio2, webaudio or null")
set_property(CACHE LABRADOR_AUDIO_BACKEND PROPERTY STRINGS xaudio2 webaudio null)

if(LABRADOR_AUDIO_BACKEND STREQUAL "xaudio2" AND EMSCRIPTEN)
    message(FATAL_ERROR
        "LABRADOR_AUDIO_BACKEND is xaudio2 in a browser build, which has no "
        "XAudio2. It must be webaudio or null here.")
elseif(LABRADOR_AUDIO_BACKEND STREQUAL "webaudio" AND NOT EMSCRIPTEN)
    message(FATAL_ERROR
        "LABRADOR_AUDIO_BACKEND is webaudio, which only a browser build "
        "(Emscripten) has. It must be xaudio2 or null here.")
endif()

if(LABRADOR_AUDIO_BACKEND STREQUAL "xaudio2")
    target_sources(LabradorEngine PRIVATE
        audio/xaudio2/audio_device.cpp
    )
elseif(LABRADOR_AUDIO_BACKEND STREQUAL "webaudio")
    target_sources(LabradorEngine PRIVATE
        audio/webaudio/audio_device.cpp
    )
elseif(LABRADOR_AUDIO_BACKEND STREQUAL "null")
    target_sources(LabradorEngine PRIVATE
        audio/null/audio_device.cpp
    )
else()
    message(FATAL_ERROR
        "LABRADOR_AUDIO_BACKEND is '${LABRADOR_AUDIO_BACKEND}'. "
        "It must be xaudio2, webaudio or null - a backend is a folder in "
        "engine/audio/ holding audio_device.cpp.")
endif()

# The input backend, a third axis chosen the same way. Only the pad is behind
# it: the keyboard and the mouse are fed by the window rather than read from a
# pad API, so they are engine code on every platform and need no backend
# (ARCHITECTURE.md, the fed devices). xinput is DirectXTK's GamePad, and web is
# the browser Gamepad API, which is polled exactly as sample() asks.
if(EMSCRIPTEN)
    set(labrador_default_input_backend "web")
else()
    set(labrador_default_input_backend "xinput")
endif()
set(LABRADOR_INPUT_BACKEND "${labrador_default_input_backend}" CACHE STRING
    "The input backend to build: xinput or web")
set_property(CACHE LABRADOR_INPUT_BACKEND PROPERTY STRINGS xinput web)

if(LABRADOR_INPUT_BACKEND STREQUAL "xinput")
    target_sources(LabradorEngine PRIVATE
        input/xinput/gamepad_reader.cpp
        input/xinput/xinput_gamepad.cpp
    )
elseif(LABRADOR_INPUT_BACKEND STREQUAL "web")
    target_sources(LabradorEngine PRIVATE
        input/web/gamepad_reader.cpp
        input/web/standard_gamepad.cpp
    )
else()
    message(FATAL_ERROR
        "LABRADOR_INPUT_BACKEND is '${LABRADOR_INPUT_BACKEND}'. "
        "It must be xinput or web - a backend is a folder in engine/input/ "
        "holding gamepad_reader.cpp.")
endif()

# rapidjson is PRIVATE and SYSTEM, so a game that parses nothing has no JSON
# parser's headers on its command line, and their warnings are not measured
# against /W4 /WX.
#
# PRIVATE means what it says: assets/json.h hands out JsonDocument and
# JsonValue, whose only rapidjson is a void* recovered inside json.cpp, so no
# engine header names the parser and no client needs it to include one. A
# client that parses files of its own asks for rapidjson on its own account -
# which the game does, for the save file it writes.
target_include_directories(LabradorEngine SYSTEM PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../external/rapidjson/include"
)
# DirectXTK IS PRIVATE. Linked PUBLIC, it would put <Audio.h> and <GamePad.h>
# on the command line of every sample, every test and every downstream game.
# No public engine header names a DirectX:: type: the audio seam
# (engine/audio/audio_device.h) and the input seam
# (engine/input/gamepad_reader.h) keep them behind backend folders, so nothing
# above one needs its headers - which is the same claim the Vulkan block above makes, checked
# the same way, and the reason cmake/check_engine_includes.cmake guards
# engine/audio/<backend>/ as well as engine/render/<backend>/.
target_link_libraries(LabradorEngine
    PUBLIC LabradorMath
    PRIVATE labrador_settings
)

# And only the two backends that call it link it at all: audio/xaudio2/ and
# input/xinput/. A build with neither - the browser's - has no DirectXTK to
# find, and vcpkg.json does not install it there.
if(LABRADOR_AUDIO_BACKEND STREQUAL "xaudio2" OR
    LABRADOR_INPUT_BACKEND STREQUAL "xinput")
    # xaudio2redist first: directxtk's config references
    # Microsoft::XAudio2Redist but does not find it itself.
    find_package(xaudio2redist CONFIG REQUIRED)
    find_package(directxtk CONFIG REQUIRED)
    target_link_libraries(LabradorEngine PRIVATE Microsoft::DirectXTK)
endif()
