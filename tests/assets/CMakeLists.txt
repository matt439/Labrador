find_package(doctest CONFIG REQUIRED)

add_executable(AssetsTests
    asset_manifest_loader_tests.cpp
    test_main.cpp
)
target_link_libraries(AssetsTests PRIVATE
    ArtAttackEngine
    doctest::doctest
    artattack_settings
)

add_test(NAME AssetsTests COMMAND AssetsTests)
