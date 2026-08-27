find_package(doctest CONFIG REQUIRED)

add_executable(SceneTests
    scene_tests.cpp
    test_main.cpp
)
target_link_libraries(SceneTests PRIVATE
    LabradorEngine
    doctest::doctest
    labrador_settings
)

# The per-view render fan-out, in the one configuration that can run it.
#
# Scene::draw takes a Renderer, and the null backend is the only one with a
# Renderer that needs no window and no adapter - so the fan-out's own tests
# compile only there, which is the rule tests/render/CMakeLists.txt already
# states for null_tests.cpp: a backend is chosen at build time, so a file that
# names one only compiles when that one is built. Everything else in SceneTests
# is configuration-independent and stays so.
if(LABRADOR_RENDER_BACKEND STREQUAL "null")
    target_sources(SceneTests PRIVATE fanout_tests.cpp)
endif()

add_test(NAME SceneTests COMMAND SceneTests)
