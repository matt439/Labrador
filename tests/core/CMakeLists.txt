labrador_add_test_suite(CoreTests
    SOURCES
        byte_reader_tests.cpp
        diagnostic_tests.cpp
        file_path_tests.cpp
        name_table_tests.cpp
        partitioner_tests.cpp
        random_tests.cpp
        registry_tests.cpp
        state_context_tests.cpp
        step_position_tests.cpp
        step_timer_tests.cpp
        thread_pool_tests.cpp
        utf8_tests.cpp
        version_tests.cpp
    LINKS LabradorEngine
)
target_compile_definitions(CoreTests PRIVATE
    LABRADOR_EXPECTED_VERSION_MAJOR=${LABRADOR_VERSION_MAJOR}u
    LABRADOR_EXPECTED_VERSION_MINOR=${LABRADOR_VERSION_MINOR}u
    LABRADOR_EXPECTED_VERSION_PATCH=${LABRADOR_VERSION_PATCH}u
    LABRADOR_EXPECTED_VERSION_STRING="${LABRADOR_VERSION_STRING}"
    LABRADOR_EXPECTED_VERSION_IS_DEVELOPMENT=$<BOOL:${LABRADOR_VERSION_IS_DEVELOPMENT}>
)
