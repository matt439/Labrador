add_subdirectory(math)

# ArtAttackEngine — the engine static library (docs/design/ARCHITECTURE.md).
# Modules are folders; dependencies point one way, toward math. Files
# migrate in from the game as they become dependency-clean.

# xaudio2redist first: directxtk's config references Microsoft::XAudio2Redist
# but does not find it itself.
find_package(xaudio2redist CONFIG REQUIRED)
find_package(directxtk CONFIG REQUIRED)

add_library(ArtAttackEngine STATIC
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
    core/moving_object.cpp
    core/state.cpp
    core/state_context.cpp
    core/thread_pool.cpp
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
    render/d3d11/device_resources.cpp
    render/d3d11/render_resources.cpp
    render/d3d11/renderer.cpp
    render/d3d11/resource_factory.cpp
    render/draw_object.cpp
    render/label.cpp
    render/resolution_manager.cpp
    render/sprite_frame.cpp
    render/sprite_sheet.cpp
    render/sprite_sheet_object.cpp
    render/text.cpp
    render/text_drop_shadow.cpp
    render/text_encoding.cpp
    render/text_object.cpp
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
target_include_directories(ArtAttackEngine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")

# rapidjson is PRIVATE and SYSTEM. It used to be PUBLIC, so a sample that
# parses nothing still had a JSON parser's headers on its command line, and
# their warnings were measured against /W4 /WX.
#
# PRIVATE now means what it says: assets/json.h hands out JsonDocument and
# JsonValue, whose only rapidjson is a void* recovered inside json.cpp, so no
# engine header names the parser and no client needs it to include one. A
# client that parses files of its own asks for rapidjson on its own account -
# which the game does, for the save file it writes.
target_include_directories(ArtAttackEngine SYSTEM PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../external/rapidjson/include"
)
target_link_libraries(ArtAttackEngine
    PUBLIC MattMath Microsoft::DirectXTK d3d11 dxgi dxguid
    PRIVATE artattack_settings
)
