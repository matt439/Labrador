# The native-null sanitizer lane links lodepng's static objects. Its code and
# STL container annotations must be instrumented like the executable. The
# header-only doctest and rapidjson compile with the repository's flags.
# Native null still calls XInput through the source-built DirectXTK DLL.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

if(PORT STREQUAL "lodepng" OR PORT STREQUAL "directxtk")
    set(VCPKG_C_FLAGS "/fsanitize=address")
    set(VCPKG_CXX_FLAGS "/fsanitize=address")
    set(VCPKG_CMAKE_CONFIGURE_OPTIONS
        "-DCMAKE_C_FLAGS_DEBUG=/MDd /Z7 /Ob0 /Od"
        "-DCMAKE_CXX_FLAGS_DEBUG=/MDd /Z7 /Ob0 /Od"
        "-DCMAKE_EXE_LINKER_FLAGS_DEBUG=/debug /INCREMENTAL:NO")
endif()
