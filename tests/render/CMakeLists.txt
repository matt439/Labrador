find_package(doctest CONFIG REQUIRED)

add_executable(RenderTests
    camera_tests.cpp
    camera_tools_tests.cpp
    colour_tests.cpp
    dds_file_tests.cpp
    font_tests.cpp
    render_resources_tests.cpp
    renderer_seam_tests.cpp
    resolution_manager_tests.cpp
    sprite_font_file_tests.cpp
    sprite_geometry_tests.cpp
    text_encoding_tests.cpp
    viewport_manager_tests.cpp
    viewport_tests.cpp
    test_main.cpp
)
target_link_libraries(RenderTests PRIVATE
    LabradorEngine
    doctest::doctest
    labrador_settings
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

# The null backend's own tests, in the one configuration that has it.
#
# A backend is chosen at build time, so a test that names one only compiles
# when that one is built - which is the same rule a client lives under and is
# why this is a source list and not an #ifdef. RenderTests is otherwise
# identical in every configuration.
if(LABRADOR_RENDER_BACKEND STREQUAL "null")
    target_sources(RenderTests PRIVATE null_tests.cpp)
endif()

add_test(NAME RenderTests COMMAND RenderTests)

# RenderPixelTests — the drawing half, on a real device.
#
# A separate executable from RenderTests, which is pure arithmetic and wants no
# device. This one creates a device and a hidden window, so it is the one entry
# in ctest that can fail for a reason outside the repository: a Direct3D debug
# build falls back to WARP and needs no GPU, but a release build has no such
# fallback (device_resources.cpp) and the OpenGL backend has none at all, so
# both need an adapter. Keeping it apart means that failure names itself
# instead of taking the arithmetic down with it.
#
# NOT BUILT AT ALL AGAINST THE NULL BACKEND, rather than built and excluded. It
# records what it was asked to draw and never draws it, so read_back_buffer
# throws saying so - and a test that cannot pass is better absent than skipped,
# because a skipped test is a line in a log nobody reads and a missing one is a
# question somebody asks.
if(NOT LABRADOR_RENDER_BACKEND STREQUAL "null")
    add_executable(RenderPixelTests
        golden_image.cpp
        pixel_tests.cpp
        test_main.cpp
    )
    target_link_libraries(RenderPixelTests PRIVATE
        LabradorEngine
        doctest::doctest
        labrador_settings
        # The in-box PNG codec, for the golden set. A test executable that
        # already opens a window is not made less portable by reading an image
        # with the imaging component of the same operating system, and the
        # alternative - a raw dump of every frame - is a golden set no reviewer
        # can look at (tests/render/golden_image.h).
        windowscodecs
    )

    # The golden set, addressed where it lives rather than copied beside the
    # executable like content/ is, and golden_image.cpp says why: a
    # regeneration has to write into the source tree, because that is the only
    # place the new images can be reviewed before they are committed.
    target_compile_definitions(RenderPixelTests PRIVATE
        LABRADOR_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden"
    )

    # The test content, beside the executable. ctest runs from the build
    # directory, and the harness loads "./content/" relative to it - a
    # four-colour texture and a font, both owned by these tests rather than
    # borrowed from a sample, so that restyling a sample cannot move a pin in
    # here.
    add_custom_command(TARGET RenderPixelTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/content"
            "$<TARGET_FILE_DIR:RenderPixelTests>/content"
    )

    add_test(NAME RenderPixelTests COMMAND RenderPixelTests)
endif()
