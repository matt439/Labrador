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
    assets/asset_manifest_loader.cpp
    assets/json_loader.cpp
    assets/resource_loader.cpp
    assets/sound_bank_loader.cpp
    assets/sprite_sheet_loader.cpp
    audio/audio_resources.cpp
    audio/sound_bank.cpp
    audio/sound_bank_object.cpp
    collision/partitioner.cpp
    core/moving_object.cpp
    core/state.cpp
    core/state_context.cpp
    core/thread_pool.cpp
    render/animation_object.cpp
    render/animation_strip.cpp
    render/camera_tools.cpp
    render/device_resources.cpp
    render/draw_object.cpp
    render/drawer.cpp
    render/render_resources.cpp
    render/resolution_manager.cpp
    render/sprite_frame.cpp
    render/sprite_sheet.cpp
    render/sprite_sheet_object.cpp
    render/text.cpp
    render/text_drop_shadow.cpp
    render/text_encoding.cpp
    render/text_object.cpp
    render/texture_object.cpp
    render/viewport_manager.cpp
    render/visual.cpp
    ui/widget.cpp
)
target_include_directories(ArtAttackEngine PUBLIC
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/external/rapidjson/include"
)
target_link_libraries(ArtAttackEngine
    PUBLIC MattMath Microsoft::DirectXTK d3d11 dxgi dxguid
    PRIVATE artattack_settings
)
