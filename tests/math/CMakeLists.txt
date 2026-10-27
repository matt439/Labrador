labrador_add_test_suite(LabradorMathTests
    SOURCES
        constant_initialisation_tests.cpp
        ericson_math_tests.cpp
        inflate_tests.cpp
        matrix3x2f_tests.cpp
        quad_tests.cpp
        rectangle_rotated_tests.cpp
        rectanglef_tests.cpp
        shape_intersect_tests.cpp
        shape_tests.cpp
        triangle_tests.cpp
        vector2_tests.cpp
    LINKS LabradorMath
)
