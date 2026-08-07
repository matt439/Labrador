find_package(doctest CONFIG REQUIRED)

# Registry and NameTable are header-only and depend on nothing but the
# standard library, so this target links neither the engine nor D3D - it
# includes the two headers directly.
add_executable(CoreTests
    name_table_tests.cpp
    registry_tests.cpp
    test_main.cpp
)
target_include_directories(CoreTests PRIVATE "${CMAKE_SOURCE_DIR}")
target_link_libraries(CoreTests PRIVATE
    doctest::doctest
    artattack_settings
)

add_test(NAME CoreTests COMMAND CoreTests)
