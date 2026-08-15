find_package(doctest CONFIG REQUIRED)

add_executable(RenderTests
    camera_tests.cpp
    camera_tools_tests.cpp
    colour_tests.cpp
    dds_file_tests.cpp
    font_tests.cpp
    render_resources_tests.cpp
    resolution_manager_tests.cpp
    sprite_font_file_tests.cpp
    sprite_geometry_tests.cpp
    text_encoding_tests.cpp
    viewport_manager_tests.cpp
    test_main.cpp
)
target_link_libraries(RenderTests PRIVATE
    ArtAttackEngine
    doctest::doctest
    artattack_settings
)

# The same content, for the same reason, and it is new here: a font stopped
# being a device resource, so reading one and measuring with it is now
# arithmetic this executable can do. sprite_font_file_tests.cpp reads the real
# file rather than a fixture, because a fixture could only agree with the parser
# that wrote it.
add_custom_command(TARGET RenderTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/content"
        "$<TARGET_FILE_DIR:RenderTests>/content"
)

add_test(NAME RenderTests COMMAND RenderTests)

# RenderPixelTests — the drawing half, on a real device.
#
# A separate executable from RenderTests, which is pure arithmetic and wants no
# device. This one creates a Direct3D device and a hidden window, so it is the
# one entry in ctest that can fail for a reason outside the repository: a debug
# build falls back to WARP and needs no GPU, but a release build has no such
# fallback (device_resources.cpp) and needs an adapter. Keeping it apart means
# that failure names itself instead of taking the arithmetic down with it.
add_executable(RenderPixelTests
    pixel_tests.cpp
    test_main.cpp
)
target_link_libraries(RenderPixelTests PRIVATE
    ArtAttackEngine
    doctest::doctest
    artattack_settings
)

# The test content, beside the executable. ctest runs from the build directory,
# and the harness loads "./content/" relative to it - a four-colour texture and
# a font, both owned by these tests rather than borrowed from a sample, so that
# restyling a sample cannot move a pin in here.
add_custom_command(TARGET RenderPixelTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/content"
        "$<TARGET_FILE_DIR:RenderPixelTests>/content"
)

add_test(NAME RenderPixelTests COMMAND RenderPixelTests)
