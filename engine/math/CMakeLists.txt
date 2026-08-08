# MattMath — the math static library. Depends on nothing
# (docs/design/ARCHITECTURE.md, the module table), which as of the conversion
# strip is literally true: no DirectXTK, no D3D11, no Windows headers. The
# conversions that used to require them live at the backend's edge, in
# engine/render/d3d11/renderer.cpp.

add_library(MattMath STATIC
    ericson_math.cpp
    matt_math.cpp
)
target_include_directories(MattMath PUBLIC "${CMAKE_SOURCE_DIR}")
target_link_libraries(MattMath PRIVATE artattack_settings)
