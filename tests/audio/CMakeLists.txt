find_package(doctest CONFIG REQUIRED)

add_executable(AudioTests
    audio_resources_tests.cpp
    sound_bank_object_tests.cpp
    sound_bank_tests.cpp
    test_main.cpp
)
# No find_package for DirectXTK here, and it is worth saying why rather than
# leaving it to look tidy: LabradorEngine carries Microsoft::DirectXTK forward
# as a PUBLIC dependency, so this target gets <Audio.h> whether it wants it or
# not. It wants it. DirectX::SoundState is the return type of one of the
# methods below, so a headless test of an engine module has to compile a
# platform header in order to name what it was told. That is docs/next.md
# section 3.4's finding stated as a build file.
target_link_libraries(AudioTests PRIVATE
    LabradorEngine
    doctest::doctest
    labrador_settings
)

add_test(NAME AudioTests COMMAND AudioTests)
