# Release-only IPO for this directory and the targets it creates below it.
# A consumer's parent directory and targets keep their own CMake policy.
option(LABRADOR_NATIVE_RELEASE_IPO
    "Enable interprocedural optimization for native MSVC Release targets" OFF)

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
