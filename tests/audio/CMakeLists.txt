find_package(doctest CONFIG REQUIRED)

add_executable(AudioTests
    audio_resources_tests.cpp
    sound_bank_object_tests.cpp
    sound_bank_tests.cpp
    test_main.cpp
    xwb_file_tests.cpp
)
# No find_package for DirectXTK here, and what that sentence means has
# changed. It used to say: this target gets <Audio.h> whether it wants it or
# not, because LabradorEngine carries Microsoft::DirectXTK forward as a PUBLIC
# dependency - and it wants it, because DirectX::SoundState is the return type
# of one of the methods below, so a headless test of an engine module has to
# compile a platform header in order to name what it was told. That was
# docs/survey/2026-08-26.md 3.4's finding stated as a build file.
#
# It is now the finding's absence. The seam took the last DirectX:: type out of
# the engine's public headers, DirectXTK is PRIVATE on LabradorEngine, and
# nothing in this folder includes an audio API's header. There is nothing to ask
# for here because there is nothing left to name.
target_link_libraries(AudioTests PRIVATE
    LabradorEngine
    doctest::doctest
    labrador_settings
)

# The banks xwb_file_tests.cpp reads, beside the executable, and the WAVs it
# holds their samples to. ctest runs from this directory and the cases load
# "./content/" relative to it, which under Node is the same directory through
# NODERAWFS. tones.xwb and its three sources are the audio sample's, copied
# rather than referred to, so the test reads exactly what that sample ships;
# ping_adpcm.xwb is this folder's own, and make_adpcm_bank.py beside it says
# how it was made. One target, for the reason tests/render/CMakeLists.txt
# gives for its own content.
add_custom_target(audio_test_content ALL)
add_custom_command(TARGET audio_test_content POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory
        "$<TARGET_FILE_DIR:AudioTests>/content"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/content/ping_adpcm.xwb"
        "${PROJECT_SOURCE_DIR}/samples/audio/content/sounds/tones.xwb"
        "${PROJECT_SOURCE_DIR}/samples/audio/source/ping.wav"
        "${PROJECT_SOURCE_DIR}/samples/audio/source/chime.wav"
        "${PROJECT_SOURCE_DIR}/samples/audio/source/drone.wav"
        "$<TARGET_FILE_DIR:AudioTests>/content"
    VERBATIM
)
add_dependencies(AudioTests audio_test_content)

# The null backend's own tests, in the one configuration that has it.
#
# The same rule tests/render/CMakeLists.txt states for its own null_tests.cpp,
# and for the same reason: a backend is chosen at build time, so a test that
# names one only compiles when that one is built. Everything above is identical
# in every configuration.
#
# THIS IS THE FILE docs/survey/2026-08-26.md 3.4b EXISTS TO PRODUCE. It is
# where the eight methods 3.4a found to have no observable behaviour say what
# they did, and where the five level clamps that had never executed are read
# back off the numbers that reached the device.
if(LABRADOR_AUDIO_BACKEND STREQUAL "null")
    target_sources(AudioTests PRIVATE null_tests.cpp)
endif()

# Deterministic retry policy; no endpoint or DirectXTK graph is created.
if(LABRADOR_AUDIO_BACKEND STREQUAL "xaudio2")
    target_sources(AudioTests PRIVATE xaudio2_recovery_tests.cpp)
endif()

# The Web Audio backend's, by the same rule. It is a browser build's, whose
# tests run under Node with no Web Audio, so these are what that backend answers
# without making a sound; webaudio_tests.cpp says what is left for a page.
if(LABRADOR_AUDIO_BACKEND STREQUAL "webaudio")
    target_sources(AudioTests PRIVATE webaudio_tests.cpp)
endif()

add_test(NAME AudioTests COMMAND AudioTests)
