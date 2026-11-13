# MinimalStateTests - the template's real states driven by synthetic keys, in
# an Application with a hidden window and both backends null, so no graphics
# or audio API opens. Native only: a browser build's Application::run() owns
# the loop, and this steps the states by hand.
#
# Here rather than in samples/minimal because the starter project is made from
# that folder and registers only the game's two tests; the testing guide shows
# a game how to add this one to its own null build. It is not on the shared
# harness, tests/test_main.cpp, because the guide shows the whole file, main
# included. The timeout is the shared suites' (tests/CMakeLists.txt).
if(WIN32 AND LABRADOR_RENDER_BACKEND STREQUAL "null"
    AND LABRADOR_AUDIO_BACKEND STREQUAL "null")
    set(minimal_dir "${CMAKE_CURRENT_SOURCE_DIR}/../../samples/minimal")
    add_executable(MinimalStateTests
        state_tests.cpp
        "${minimal_dir}/states/confirm_state.cpp"
        "${minimal_dir}/states/hello_state.cpp"
    )
    target_link_libraries(MinimalStateTests PRIVATE
        MinimalRules
        LabradorEngine
        doctest::doctest
        labrador_settings
    )
    target_compile_definitions(MinimalStateTests PRIVATE
        GAME_MANIFEST="${minimal_dir}/content/manifest.json"
    )
    add_test(NAME MinimalStateTests COMMAND MinimalStateTests)
    set_tests_properties(MinimalStateTests PROPERTIES TIMEOUT 60)
endif()
