# MattMath — the math static library. Depends on nothing
# (docs/design/ARCHITECTURE.md, the module table), which as of the conversion
# strip is literally true: no DirectXTK, no D3D11, no Windows headers. The
# conversions that used to require them live at the backend's edge, in
# engine/render/d3d11/renderer.cpp.

add_library(MattMath STATIC
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
target_include_directories(MattMath PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../..")
target_link_libraries(MattMath PRIVATE labrador_settings)
