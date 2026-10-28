# Components: small pieces a game copies rather than links - see README.md.
#
# The components themselves are a static library only so that the gallery and
# the tests compile them once. A game that takes one copies its .h and .cpp
# into its own tree and lists them there; nothing outside this folder links
# ComponentsExample.
add_library(ComponentsExample STATIC
    animation_preview.cpp nine_slice.cpp option_row.cpp slider.cpp)
target_link_libraries(ComponentsExample PUBLIC LabradorEngine labrador_settings)

set(components_texture_files
    "${CMAKE_CURRENT_SOURCE_DIR}/content/textures/panel.dds"
    "${CMAKE_CURRENT_SOURCE_DIR}/content/textures/animation_preview.dds"
    "${CMAKE_CURRENT_SOURCE_DIR}/content/textures/animation_preview.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/content/textures/animation_preview.LICENSE.txt"
)

if(EMSCRIPTEN)
    add_executable(ComponentsGallery main_web.cpp gallery_state.cpp)
    target_link_libraries(ComponentsGallery PRIVATE ComponentsExample labrador_settings)

    # The gallery and tests use the same texture-folder paths in the module.
    set_target_properties(ComponentsGallery PROPERTIES SUFFIX ".html")
    target_link_options(ComponentsGallery PRIVATE
        "SHELL:--embed-file ${CMAKE_CURRENT_SOURCE_DIR}/content/manifest.json@/manifest.json"
        "SHELL:--embed-file ${CMAKE_CURRENT_SOURCE_DIR}/content/textures@/textures"
        "SHELL:--embed-file ${CMAKE_CURRENT_SOURCE_DIR}/../minimal/content/fonts@/fonts"
        "SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
        -sENVIRONMENT=web
    )
    set_property(TARGET ComponentsGallery APPEND PROPERTY LINK_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
        "${CMAKE_CURRENT_SOURCE_DIR}/content/manifest.json"
        ${components_texture_files}
        "${CMAKE_CURRENT_SOURCE_DIR}/../minimal/content/fonts/jetbrains_mono_bold_16.spritefont"
    )
elseif(WIN32)
    # The template's DPI awareness declaration (samples/minimal/dpi_aware.manifest).
    add_executable(ComponentsGallery WIN32 main.cpp gallery_state.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../minimal/dpi_aware.manifest")
    target_link_libraries(ComponentsGallery PRIVATE ComponentsExample labrador_settings)

    # Reuse the minimal sample's font. The white texel the bars are drawn from
    # is the engine's own (white_texture_name); the panel and animation atlas
    # belong to this sample. This is the single copy owner for gallery/tests.
    add_custom_target(components_content ALL
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/content/manifest.json"
            "$<TARGET_FILE_DIR:ComponentsGallery>/manifest.json"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/content/textures"
            "$<TARGET_FILE_DIR:ComponentsGallery>/textures"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/../minimal/content/fonts"
            "$<TARGET_FILE_DIR:ComponentsGallery>/fonts"
        VERBATIM
    )
    add_dependencies(ComponentsGallery components_content)
endif()

# Widget/timing cases use in-memory content. Reader cases use the delivered
# DDS/JSON. Null recorder cases use no window; the native-null gallery case
# uses a hidden window, with neither an Application loop nor an API device.
find_package(doctest CONFIG REQUIRED)
add_executable(ComponentsTests
    animation_preview_tests.cpp
    animation_preview_content_tests.cpp
    nine_slice_tests.cpp
    option_row_tests.cpp
    slider_tests.cpp
    test_main.cpp
)
target_link_libraries(ComponentsTests PRIVATE
    ComponentsExample doctest::doctest labrador_settings)
if(NOT EMSCRIPTEN AND LABRADOR_RENDER_BACKEND STREQUAL "null")
    target_sources(ComponentsTests PRIVATE animation_preview_null_tests.cpp)
endif()
if(NOT EMSCRIPTEN
    AND LABRADOR_RENDER_BACKEND STREQUAL "null"
    AND LABRADOR_AUDIO_BACKEND STREQUAL "null")
    target_sources(ComponentsTests PRIVATE gallery_animation_tests.cpp gallery_state.cpp)
endif()
if(TARGET components_content)
    add_dependencies(ComponentsTests components_content)
endif()
# Node runs the shared readers against the same texture-folder contents.
if(EMSCRIPTEN)
    target_link_options(ComponentsTests PRIVATE
        -sENVIRONMENT=node
        "SHELL:--embed-file ${CMAKE_CURRENT_SOURCE_DIR}/content/textures@/textures"
    )
    set_property(TARGET ComponentsTests APPEND PROPERTY LINK_DEPENDS
        ${components_texture_files})
endif()
add_test(NAME ComponentsTests COMMAND ComponentsTests)
