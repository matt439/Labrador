labrador_add_test_suite(InputTests
    SOURCES
        deadzone_tests.cpp
        direction_tests.cpp
        gamepad_tests.cpp
        keyboard_tests.cpp
        mouse_tests.cpp
        touch_tests.cpp
        vibration_tests.cpp
    LINKS LabradorEngine
)

# The browser pad's translation, which is the part of engine/input/web/ Node
# can reach with no Gamepad API. Compiled only where that backend is.
if(LABRADOR_INPUT_BACKEND STREQUAL "web")
    target_sources(InputTests PRIVATE standard_gamepad_tests.cpp)
endif()

# The XInput pad's translation, the part of engine/input/xinput/ that needs no
# pad. It includes DirectXTK's GamePad.h for the state it fills, so it finds
# the package the engine links PRIVATE: xaudio2redist first, because
# directxtk's config references it without finding it.
if(LABRADOR_INPUT_BACKEND STREQUAL "xinput")
    find_package(xaudio2redist CONFIG REQUIRED)
    find_package(directxtk CONFIG REQUIRED)
    target_sources(InputTests PRIVATE xinput_gamepad_tests.cpp)
    target_link_libraries(InputTests PRIVATE Microsoft::DirectXTK)
endif()
