labrador_add_test_suite(CoreTests
    SOURCES
        file_path_tests.cpp
        name_table_tests.cpp
        random_tests.cpp
        registry_tests.cpp
        state_context_tests.cpp
        step_position_tests.cpp
        thread_pool_tests.cpp
    LINKS LabradorEngine
)
