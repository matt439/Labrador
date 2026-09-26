cmake_minimum_required(VERSION 3.26)
project(LabradorContentCopyProbe LANGUAGES CXX)
add_executable(LineSweeperFrameBench main.cpp)
# Same target-level POST_BUILD pattern as bench/CMakeLists.txt:66-75.
add_custom_command(TARGET LineSweeperFrameBench POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/textures"
        "$<TARGET_FILE_DIR:LineSweeperFrameBench>/textures"
    VERBATIM
)
