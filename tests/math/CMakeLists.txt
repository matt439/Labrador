find_package(doctest CONFIG REQUIRED)

add_executable(MattMathTests
    ericson_math_tests.cpp
    inflate_tests.cpp
    quad_tests.cpp
    rectangle_rotated_tests.cpp
    rectanglef_tests.cpp
    shape_intersect_tests.cpp
    shape_tests.cpp
    test_main.cpp
    triangle_tests.cpp
    vector2_tests.cpp
)
target_link_libraries(MattMathTests PRIVATE
    MattMath
    doctest::doctest
    artattack_settings
)

add_test(NAME MattMathTests COMMAND MattMathTests)
