add_executable(AudioSample WIN32
    main.cpp
    audio_state.cpp
    smoke_test.cpp
    sound_board.cpp
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

find_package(doctest CONFIG REQUIRED)
add_executable(AudioSampleTests tests.cpp sound_board.cpp)
target_link_libraries(AudioSampleTests PRIVATE
    LabradorEngine doctest::doctest labrador_settings)
target_compile_definitions(AudioSampleTests PRIVATE
    "LABRADOR_AUDIO_SAMPLE_CONTENT=\"${CMAKE_CURRENT_SOURCE_DIR}/content\"")
add_test(NAME AudioSampleTests COMMAND AudioSampleTests)

if(LABRADOR_AUDIO_BACKEND STREQUAL "null")
    target_compile_definitions(AudioSample PRIVATE LABRADOR_AUDIO_SAMPLE_NULL)
    target_compile_definitions(AudioSampleTests PRIVATE LABRADOR_AUDIO_SAMPLE_NULL)
endif()
