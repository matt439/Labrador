# Instrumentation is an isolated maintainer configuration. A consumer adding
# the engine as a subdirectory keeps its own compiler/runtime policy.
option(LABRADOR_ASAN "Instrument the standalone native-null Debug test configuration" OFF)

if(LABRADOR_ASAN)
    if(NOT PROJECT_IS_TOP_LEVEL OR NOT WIN32 OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC"
        OR NOT CMAKE_BUILD_TYPE STREQUAL "Debug"
        OR NOT LABRADOR_RENDER_BACKEND STREQUAL "null"
        OR NOT LABRADOR_AUDIO_BACKEND STREQUAL "null")
        message(FATAL_ERROR "LABRADOR_ASAN requires the standalone MSVC native-null Debug configuration. Use --preset x64-debug-null-asan.")
    endif()
    if(NOT VCPKG_TARGET_TRIPLET STREQUAL "x64-windows-asan"
        OR NOT CMAKE_CXX_FLAGS MATCHES "/fsanitize=address"
        OR CMAKE_CXX_FLAGS_DEBUG MATCHES "/RTC")
        message(FATAL_ERROR "LABRADOR_ASAN requires instrumented dependencies and compatible compiler flags. Use --preset x64-debug-null-asan; do not disable STL annotations.")
    endif()
endif()
