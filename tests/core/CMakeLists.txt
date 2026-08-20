find_package(doctest CONFIG REQUIRED)

add_executable(CoreTests
    name_table_tests.cpp
    registry_tests.cpp
    state_context_tests.cpp
    thread_pool_tests.cpp
    test_main.cpp
)
target_link_libraries(CoreTests PRIVATE
    LabradorEngine
    doctest::doctest
    labrador_settings
)

add_test(NAME CoreTests COMMAND CoreTests)
