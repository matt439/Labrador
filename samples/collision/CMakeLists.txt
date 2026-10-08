add_library(CollisionExample STATIC body.cpp arena.cpp)
target_link_libraries(CollisionExample PUBLIC LabradorEngine labrador_settings)

add_executable(CollisionSample WIN32 main.cpp)
target_link_libraries(CollisionSample PRIVATE CollisionExample labrador_settings)
if(NOT LABRADOR_RENDER_BACKEND STREQUAL "null")
    target_sources(CollisionSample PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../../tools/linesweeper_capture/png_file.cpp")
    target_compile_definitions(CollisionSample PRIVATE COLLISION_SAMPLE_CAPTURE)
    target_link_libraries(CollisionSample PRIVATE windowscodecs)
endif()

# Reuse the existing font and white texel; the output is self-contained.
add_custom_target(collision_content ALL
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/content/manifest.json"
        "$<TARGET_FILE_DIR:CollisionSample>/manifest.json"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../minimal/content/fonts"
        "$<TARGET_FILE_DIR:CollisionSample>/fonts"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../linesweeper/content/textures"
        "$<TARGET_FILE_DIR:CollisionSample>/textures"
    VERBATIM
)
add_dependencies(CollisionSample collision_content)

find_package(doctest CONFIG REQUIRED)
add_executable(CollisionSampleTests arena_tests.cpp)
target_link_libraries(CollisionSampleTests PRIVATE
    CollisionExample doctest::doctest labrador_settings)
add_test(NAME CollisionSampleTests COMMAND CollisionSampleTests)
