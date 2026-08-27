find_package(doctest CONFIG REQUIRED)

add_executable(AudioTests
    audio_resources_tests.cpp
    sound_bank_object_tests.cpp
    sound_bank_tests.cpp
    test_main.cpp
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

add_test(NAME AudioTests COMMAND AudioTests)
