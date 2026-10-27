labrador_add_test_suite(SceneTests
    SOURCES scene_tests.cpp
    LINKS LabradorEngine
)

# The per-view render fan-out and the cull, in the one configuration that can
# run either.
#
# Scene::draw takes a Renderer, and the null backend is the only one with a
# Renderer that needs no window and no adapter - so the tests that go through
# Scene::draw compile only there, which is the rule tests/render/CMakeLists.txt
# already states for null_tests.cpp: a backend is chosen at build time, so a
# file that names one only compiles when that one is built. Everything else in
# SceneTests is configuration-independent and stays so.
if(LABRADOR_RENDER_BACKEND STREQUAL "null")
    target_sources(SceneTests PRIVATE fanout_tests.cpp cull_tests.cpp order_tests.cpp)
endif()
