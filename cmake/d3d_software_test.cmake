# Explicit software qualification, never an ordinary Release fallback.
# Included after LABRADOR_RENDER_BACKEND is selected, including by consumers.
option(LABRADOR_D3D_SOFTWARE_TEST
    "Force Microsoft WARP for explicit native Direct3D software tests" OFF)

if(LABRADOR_D3D_SOFTWARE_TEST AND
        (NOT WIN32 OR EMSCRIPTEN OR
         NOT LABRADOR_RENDER_BACKEND MATCHES "^(d3d11|d3d12)$"))
    message(FATAL_ERROR
        "LABRADOR_D3D_SOFTWARE_TEST requires native Windows d3d11 or d3d12. "
        "It forces the WARP software rasteriser for explicit tests; leave it "
        "OFF for other backends and ordinary Release consumers.")
endif()

# Private copies of a backend .cpp in a test/probe need the same selection as
# the library. Nothing exports this implementation definition to game code.
function(labrador_apply_d3d_software_test target)
    if(LABRADOR_D3D_SOFTWARE_TEST)
        target_compile_definitions(${target} PRIVATE LABRADOR_D3D_SOFTWARE_TEST=1)
    endif()
endfunction()
