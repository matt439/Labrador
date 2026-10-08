find_package(doctest CONFIG REQUIRED)
add_executable(LocalMultiplayerTests
    arena_tests.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../../samples/local_multiplayer/arena.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../samples/local_multiplayer/controls.cpp"
)
target_link_libraries(LocalMultiplayerTests PRIVATE
    LabradorEngine doctest::doctest labrador_settings)
if(LABRADOR_RENDER_BACKEND STREQUAL "null")
    target_sources(LocalMultiplayerTests PRIVATE draw_tests.cpp)
endif()
add_test(NAME LocalMultiplayerTests COMMAND LocalMultiplayerTests)
