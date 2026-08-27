find_package(doctest CONFIG REQUIRED)

add_executable(AssetsTests
    asset_manifest_loader_tests.cpp
    json_tests.cpp
    resource_loader_tests.cpp
    sprite_sheet_loader_tests.cpp
    test_main.cpp
)
# No rapidjson here. These tests used to ask for it themselves, because the
# assets module's public JSON type was a rapidjson::Document; it is JsonValue
# now, and this target is the check that a client of the engine's loaders needs
# nothing but the engine.
target_link_libraries(AssetsTests PRIVATE
    LabradorEngine
    doctest::doctest
    labrador_settings
)

add_test(NAME AssetsTests COMMAND AssetsTests)
