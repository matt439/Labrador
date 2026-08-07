find_package(doctest CONFIG REQUIRED)

# The manifest loader reads a file and parses JSON, and that is all it does -
# no device, no DirectXTK - so this target compiles the two translation units
# directly rather than linking the engine and everything the engine drags in.
add_executable(AssetsTests
    asset_manifest_loader_tests.cpp
    test_main.cpp
    "${CMAKE_SOURCE_DIR}/engine/assets/asset_manifest_loader.cpp"
    "${CMAKE_SOURCE_DIR}/engine/assets/json_loader.cpp"
)
target_include_directories(AssetsTests PRIVATE
    "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/external/rapidjson/include"
)
target_link_libraries(AssetsTests PRIVATE
    doctest::doctest
    artattack_settings
)

add_test(NAME AssetsTests COMMAND AssetsTests)
