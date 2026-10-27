# One sample and two entry points: main.cpp is wWinMain, with the smoke test,
# and main_web.cpp is a browser's main. Everything else is the same files, and
# AudioSampleTests links sound_board.cpp in every build.
set(audio_sample_sources
    audio_state.cpp
    sound_board.cpp
)

if(EMSCRIPTEN)
    add_executable(AudioSample
        main_web.cpp
        ${audio_sample_sources}
    )
    target_link_libraries(AudioSample PRIVATE LabradorEngine labrador_settings)

    # A page, its loader and the module, as LineSweeper's are. The content is
    # the three things the Windows build copies beside its executable,
    # embedded at the same relative paths, so the one manifest serves both.
    # The bank is 70 KB, still far below the megabyte at which
    # docs/port/web.md would fetch content rather than embed it.
    set_target_properties(AudioSample PROPERTIES SUFFIX ".html")
    target_link_options(AudioSample PRIVATE
        "SHELL:--embed-file ${CMAKE_CURRENT_SOURCE_DIR}/content/manifest.json@/manifest.json"
        "SHELL:--embed-file ${CMAKE_CURRENT_SOURCE_DIR}/content/sounds@/sounds"
        "SHELL:--embed-file ${CMAKE_CURRENT_SOURCE_DIR}/../minimal/content/fonts@/fonts"
        "SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
        -sENVIRONMENT=web
    )
    # Neither the page nor the content is a source, so a change to either
    # relinks only because it is named here.
    set_property(TARGET AudioSample APPEND PROPERTY LINK_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
        "${CMAKE_CURRENT_SOURCE_DIR}/content/manifest.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/content/sounds/tones.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/content/sounds/tones.xwb"
        "${CMAKE_CURRENT_SOURCE_DIR}/../minimal/content/fonts/jetbrains_mono_bold_16.spritefont"
    )
elseif(WIN32)
    # The template's DPI awareness declaration (samples/minimal/dpi_aware.manifest).
    add_executable(AudioSample WIN32
        main.cpp
        smoke_test.cpp
        ${audio_sample_sources}
        "${CMAKE_CURRENT_SOURCE_DIR}/../minimal/dpi_aware.manifest"
    )
    target_link_libraries(AudioSample PRIVATE LabradorEngine labrador_settings shell32)

    add_custom_target(audio_sample_content ALL
        COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/content"
            "$<TARGET_FILE_DIR:AudioSample>"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/../minimal/content/fonts"
            "$<TARGET_FILE_DIR:AudioSample>/fonts"
        VERBATIM
    )
    add_dependencies(AudioSample audio_sample_content)
endif()

find_package(doctest CONFIG REQUIRED)
add_executable(AudioSampleTests tests.cpp sound_board.cpp)
target_link_libraries(AudioSampleTests PRIVATE
    LabradorEngine doctest::doctest labrador_settings)
target_compile_definitions(AudioSampleTests PRIVATE
    "LABRADOR_AUDIO_SAMPLE_CONTENT=\"${CMAKE_CURRENT_SOURCE_DIR}/content\"")
add_test(NAME AudioSampleTests COMMAND AudioSampleTests)

# In a browser build the tests run under Node and read the content where it is
# checked in. The root CMakeLists.txt gives every test these two options, but
# only to the folders it adds after it says so, and this one is added before.
if(EMSCRIPTEN)
    target_link_options(AudioSampleTests PRIVATE -sNODERAWFS=1 -sENVIRONMENT=node)
endif()

if(LABRADOR_AUDIO_BACKEND STREQUAL "null")
    target_compile_definitions(AudioSample PRIVATE LABRADOR_AUDIO_SAMPLE_NULL)
    target_compile_definitions(AudioSampleTests PRIVATE LABRADOR_AUDIO_SAMPLE_NULL)
elseif(LABRADOR_AUDIO_BACKEND STREQUAL "webaudio")
    target_compile_definitions(AudioSample PRIVATE LABRADOR_AUDIO_SAMPLE_WEBAUDIO)
    target_compile_definitions(AudioSampleTests PRIVATE LABRADOR_AUDIO_SAMPLE_WEBAUDIO)
endif()
