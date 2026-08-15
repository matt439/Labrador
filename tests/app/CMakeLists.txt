find_package(doctest CONFIG REQUIRED)

add_executable(AppTests
    application_options_tests.cpp
    test_main.cpp
)
target_link_libraries(AppTests PRIVATE
    ArtAttackEngine
    doctest::doctest
    artattack_settings
)

add_test(NAME AppTests COMMAND AppTests)
