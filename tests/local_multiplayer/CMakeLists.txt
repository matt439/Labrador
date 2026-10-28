labrador_add_test_suite(LocalMultiplayerTests
    SOURCES
        arena_tests.cpp
        join_rules_tests.cpp
        joined_controls_tests.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../../samples/local_multiplayer/arena.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../samples/local_multiplayer/controls.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../samples/local_multiplayer/join_rules.cpp"
    LINKS LabradorEngine
)
if(LABRADOR_RENDER_BACKEND STREQUAL "null")
    target_sources(LocalMultiplayerTests PRIVATE draw_tests.cpp)
endif()

# Real menu routing with synthetic keyboard input and hidden Application
# services. Both selected backends are null: no graphics or audio API opens.
if(WIN32 AND LABRADOR_RENDER_BACKEND STREQUAL "null" AND
        LABRADOR_AUDIO_BACKEND STREQUAL "null")
    target_sources(LocalMultiplayerTests PRIVATE
        join_state_tests.cpp
        "${CMAKE_CURRENT_SOURCE_DIR}/../../samples/local_multiplayer/join_state.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../samples/local_multiplayer/play_state.cpp"
    )
    target_compile_definitions(LocalMultiplayerTests PRIVATE
        LOCAL_MULTIPLAYER_MANIFEST="${CMAKE_CURRENT_SOURCE_DIR}/../../samples/minimal/content/manifest.json"
    )
endif()
