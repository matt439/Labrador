# LabradorMath — the math static library. Depends on nothing
# (docs/design/ARCHITECTURE.md, the module table), and literally: no DirectXTK,
# no D3D11, no Windows headers. Conversions to API types live at each
# backend's edge.

add_library(LabradorMath STATIC
    circle.cpp
    ericson_math.cpp
    inflate.cpp
    intersects.cpp
    matrix3x2f.cpp
    quad.cpp
    rectangle_rotated.cpp
    rectanglef.cpp
    rectanglei.cpp
    scalar.cpp
    segment.cpp
    shape.cpp
    triangle.cpp
    vector2f.cpp
    vector2i.cpp
)
# Relative to this file, not to the top-level project: see the note in
# engine/CMakeLists.txt.
target_include_directories(LabradorMath PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../..")
target_link_libraries(LabradorMath PRIVATE labrador_settings)
