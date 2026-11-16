# Release-only IPO for this directory and the targets it creates below it.
# A consumer's parent directory and targets keep their own CMake policy.
set(labrador_native_release_ipo_default OFF)
if(PROJECT_IS_TOP_LEVEL AND WIN32 AND NOT EMSCRIPTEN
    AND CMAKE_CXX_COMPILER_ID STREQUAL "MSVC"
    AND CMAKE_BUILD_TYPE STREQUAL "Release")
    set(labrador_native_release_ipo_default ON)
endif()
option(LABRADOR_NATIVE_RELEASE_IPO
    "Enable interprocedural optimization for native MSVC Release targets"
    "${labrador_native_release_ipo_default}")

if(LABRADOR_NATIVE_RELEASE_IPO)
    if(NOT WIN32 OR EMSCRIPTEN OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        message(FATAL_ERROR
            "LABRADOR_NATIVE_RELEASE_IPO requires native Windows MSVC. "
            "Browser and other compiler IPO qualification is separate; "
            "configure this option OFF for those builds.")
    endif()
    include(CheckIPOSupported)
    check_ipo_supported(RESULT labrador_ipo_supported
        OUTPUT labrador_ipo_error LANGUAGES CXX)
    if(NOT labrador_ipo_supported)
        message(FATAL_ERROR
            "Native MSVC Release IPO is unavailable: ${labrador_ipo_error}\n"
            "Configure -DLABRADOR_NATIVE_RELEASE_IPO=OFF to retain ordinary compilation.")
    endif()
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()
