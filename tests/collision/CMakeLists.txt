find_package(doctest CONFIG REQUIRED)

add_executable(CollisionTests
    partitioner_tests.cpp
    test_main.cpp
)
target_link_libraries(CollisionTests PRIVATE
    ArtAttackEngine
    doctest::doctest
    artattack_settings
)

add_test(NAME CollisionTests COMMAND CollisionTests)
