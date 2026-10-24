# Components: small pieces a game copies rather than links - see README.md.
#
# The components themselves are a static library only so that the gallery and
# the tests compile them once. A game that takes one copies its .h and .cpp
# into its own tree and lists them there; nothing outside this folder links
# ComponentsExample.
add_library(ComponentsExample STATIC option_row.cpp slider.cpp)
target_link_libraries(ComponentsExample PUBLIC LabradorEngine labrador_settings)

if(EMSCRIPTEN)
    add_executable(ComponentsGallery main_web.cpp gallery_state.cpp)
    target_link_libraries(ComponentsGallery PRIVATE ComponentsExample labrador_settings)

    # A page, its loader and the module, as the collision sample's are, with
    # the same two things packed at the same relative paths.
    set_target_properties(ComponentsGallery PROPERTIES SUFFIX ".html")
    target_link_options(ComponentsGallery PRIVATE
        "SHELL:--embed-file ${CMAKE_CURRENT_SOURCE_DIR}/content/manifest.json@/manifest.json"
        "SHELL:--embed-file ${CMAKE_CURRENT_SOURCE_DIR}/../minimal/content/fonts@/fonts"
        "SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
        -sENVIRONMENT=web
    )
    set_property(TARGET ComponentsGallery APPEND PROPERTY LINK_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
        "${CMAKE_CURRENT_SOURCE_DIR}/content/manifest.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/../minimal/content/fonts/jetbrains_mono_bold_16.spritefont"
    )
elseif(WIN32)
    add_executable(ComponentsGallery WIN32 main.cpp gallery_state.cpp)
    target_link_libraries(ComponentsGallery PRIVATE ComponentsExample labrador_settings)

    # Reuse the minimal sample's font. The white texel the bars are drawn from
    # is the engine's own (white_texture_name).
    add_custom_target(components_content ALL
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/content/manifest.json"
            "$<TARGET_FILE_DIR:ComponentsGallery>/manifest.json"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/../minimal/content/fonts"
            "$<TARGET_FILE_DIR:ComponentsGallery>/fonts"
        VERBATIM
    )
    add_dependencies(ComponentsGallery components_content)
endif()

# Every component's behaviour, with no window, no device and no content on
# disk: test_content.h builds a font from glyphs in memory.
find_package(doctest CONFIG REQUIRED)
add_executable(ComponentsTests
    option_row_tests.cpp
    slider_tests.cpp
    test_main.cpp
)
target_link_libraries(ComponentsTests PRIVATE
    ComponentsExample doctest::doctest labrador_settings)
# Under Node, as every other browser-build test runs, and for the collision
# sample's reason this names its own environment: it reads no files.
if(EMSCRIPTEN)
    target_link_options(ComponentsTests PRIVATE -sENVIRONMENT=node)
endif()
add_test(NAME ComponentsTests COMMAND ComponentsTests)
