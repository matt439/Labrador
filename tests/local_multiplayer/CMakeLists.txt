labrador_add_test_suite(LocalMultiplayerTests
    SOURCES
        arena_tests.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../../samples/local_multiplayer/arena.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../samples/local_multiplayer/controls.cpp"
    LINKS LabradorEngine
)
if(LABRADOR_RENDER_BACKEND STREQUAL "null")
    target_sources(LocalMultiplayerTests PRIVATE draw_tests.cpp)
endif()
