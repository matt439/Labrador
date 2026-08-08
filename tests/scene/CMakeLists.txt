find_package(doctest CONFIG REQUIRED)

add_executable(SceneTests
    scene_tests.cpp
    test_main.cpp
)
target_link_libraries(SceneTests PRIVATE
    ArtAttackEngine
    doctest::doctest
    artattack_settings
)

add_test(NAME SceneTests COMMAND SceneTests)
