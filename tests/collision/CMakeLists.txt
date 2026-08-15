find_package(doctest CONFIG REQUIRED)

add_executable(CollisionTests
    broad_phase_tests.cpp
    contacts_tests.cpp
    narrow_phase_tests.cpp
    partitioner_tests.cpp
    resolve_tests.cpp
    test_main.cpp
    tunnelling_tests.cpp
)
target_link_libraries(CollisionTests PRIVATE
    LabradorEngine
    doctest::doctest
    labrador_settings
)

add_test(NAME CollisionTests COMMAND CollisionTests)
