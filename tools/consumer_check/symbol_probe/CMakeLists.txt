cmake_minimum_required(VERSION 3.26)
project(LabradorSymbolProbe LANGUAGES CXX)

if(NOT WIN32 OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The symbol probe needs native MSVC x64 and the Windows SDK.")
endif()

add_executable(LabradorSymbolProbe main.cpp)
target_compile_features(LabradorSymbolProbe PRIVATE cxx_std_20)
target_compile_definitions(LabradorSymbolProbe PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(LabradorSymbolProbe PRIVATE /W4 /WX /permissive- /sdl /utf-8 /fp:precise)
target_link_libraries(LabradorSymbolProbe PRIVATE bcrypt)
