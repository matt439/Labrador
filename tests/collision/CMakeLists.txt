labrador_add_test_suite(CollisionTests
    SOURCES
        broad_phase_tests.cpp
        contacts_tests.cpp
        narrow_phase_tests.cpp
        partitioner_tests.cpp
        resolve_tests.cpp
        tunnelling_tests.cpp
    LINKS LabradorEngine
)
