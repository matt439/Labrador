find_package(doctest CONFIG REQUIRED)

add_executable(AssetsTests
    asset_manifest_loader_tests.cpp
    test_main.cpp
)
# These tests read a rapidjson::Document out of the loaders, which is still
# the assets module's public JSON type, so they ask for rapidjson themselves -
# the engine stopped passing it on. See engine/CMakeLists.txt.
target_include_directories(AssetsTests SYSTEM PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../external/rapidjson/include"
)
target_link_libraries(AssetsTests PRIVATE
    ArtAttackEngine
    doctest::doctest
    artattack_settings
)

add_test(NAME AssetsTests COMMAND AssetsTests)
