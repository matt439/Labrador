find_package(doctest CONFIG REQUIRED)

add_executable(CoreTests
    name_table_tests.cpp
    registry_tests.cpp
    test_main.cpp
)
target_link_libraries(CoreTests PRIVATE
    ArtAttackEngine
    doctest::doctest
    artattack_settings
)

add_test(NAME CoreTests COMMAND CoreTests)
