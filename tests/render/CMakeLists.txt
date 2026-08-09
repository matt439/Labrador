find_package(doctest CONFIG REQUIRED)

add_executable(RenderTests
    camera_tests.cpp
    camera_tools_tests.cpp
    colour_tests.cpp
    viewport_manager_tests.cpp
    test_main.cpp
)
target_link_libraries(RenderTests PRIVATE
    ArtAttackEngine
    doctest::doctest
    artattack_settings
)

add_test(NAME RenderTests COMMAND RenderTests)
