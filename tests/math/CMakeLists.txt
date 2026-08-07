find_package(doctest CONFIG REQUIRED)

add_executable(MattMathTests
    collision_tools_tests.cpp
    math_tests.cpp
    test_main.cpp
)
target_link_libraries(MattMathTests PRIVATE
    MattMath
    doctest::doctest
    artattack_settings
)

add_test(NAME MattMathTests COMMAND MattMathTests)
