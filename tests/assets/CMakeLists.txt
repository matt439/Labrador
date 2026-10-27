# No rapidjson here. The assets module's public JSON type is JsonValue, and
# this target is the check that a client of the engine's loaders needs
# nothing but the engine.
labrador_add_test_suite(AssetsTests
    SOURCES
        asset_manifest_loader_tests.cpp
        json_tests.cpp
        resource_loader_tests.cpp
        sound_bank_loader_tests.cpp
        sprite_sheet_loader_tests.cpp
    LINKS LabradorEngine
)
