# Optional game-owned recipe. Include after project() and before the engine
# and game subdirectories, then opt in with -DMYGAME_RELEASE_SYMBOLS=ON.
include_guard(DIRECTORY)

option(MYGAME_RELEASE_SYMBOLS "Keep full symbols for native MSVC Release" OFF)

if(MYGAME_RELEASE_SYMBOLS)
    if(NOT WIN32 OR EMSCRIPTEN OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC"
            OR NOT CMAKE_BUILD_TYPE STREQUAL "Release")
        message(FATAL_ERROR
            "MYGAME_RELEASE_SYMBOLS requires a native Windows MSVC Release build. "
            "Use the x64-release preset, or configure this option OFF.")
    endif()
    add_compile_options(/Zi /ZH:SHA_256)
    # /DEBUG changes the linker's optimisation defaults; restate Release's
    # elimination/folding and non-incremental link choices after it.
    add_link_options(/DEBUG:FULL /OPT:REF /OPT:ICF /INCREMENTAL:NO)
endif()
