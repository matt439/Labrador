# The benchmarks PHILOSOPHY's Performance section asks for.
#
# No doctest and no third-party benchmark library: what this needs is a steady
# clock, a warm-up and a median, and a dependency for that would be the
# speculative framework T1 rules out. bench/bench.h says what it asserts and
# what it only reports.
add_executable(LabradorBench
    bench.cpp
    main.cpp
    render_bench.cpp
    scene_bench.cpp
)
target_link_libraries(LabradorBench PRIVATE
    LabradorEngine
    labrador_settings
)

# The Scene::draw fan-out case, in the one configuration that can run it.
#
# Scene::draw needs a Renderer and the null backend is the only one that has
# one without a window and an adapter, so the case is a source list rather than
# an #ifdef - the same rule tests/render/CMakeLists.txt states for
# null_tests.cpp, and for the same reason: a backend is chosen at build time,
# so a file that names one compiles only when that one is built.
#
# One of the two always compiles, never both. Unlike RenderPixelTests, whose
# absence shows up as a missing ctest entry, this one would be a silently
# shorter table - so the other four configurations get a file whose whole job
# is to say the case is elsewhere.
if(LABRADOR_RENDER_BACKEND STREQUAL "null")
    target_sources(LabradorBench PRIVATE fanout_bench_null.cpp)
else()
    target_sources(LabradorBench PRIVATE fanout_bench_absent.cpp)
endif()

# Registered with ctest, which is what "run alongside the test suite" means -
# and it is what stops a benchmark from quietly failing to compile for six
# months. It asserts on complexity class rather than on wall-clock, so it is
# not a flaky test on a busy machine; see main.cpp.
add_test(NAME Benchmarks COMMAND LabradorBench)

# An absolute frame-time measurement, deliberately not a ctest. Its result is
# a property of the selected hardware, driver and backend rather than a
# machine-independent pass/fail gate. The cloud reference lane invokes this
# console target directly and retains its JSON artifact.
add_executable(LineSweeperFrameBench
    frame_statistics.cpp
    linesweeper_frame_bench.cpp
    linesweeper_frame_result.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/../samples/linesweeper/presentation/board_view.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../samples/linesweeper/presentation/particles.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../samples/linesweeper/presentation/top_out_banner.cpp"
)
target_link_libraries(LineSweeperFrameBench PRIVATE
    LineSweeperRules
    LabradorEngine
    labrador_settings
)
target_include_directories(LineSweeperFrameBench SYSTEM PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../external/rapidjson/include"
)

# The benchmark resolves the same real font and white texel as PlayState. Keep
# a private copy beside this executable so it runs from any working directory.
add_custom_command(TARGET LineSweeperFrameBench POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../samples/linesweeper/content/manifest.json"
        "$<TARGET_FILE_DIR:LineSweeperFrameBench>/manifest.json"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../samples/linesweeper/content/fonts"
        "$<TARGET_FILE_DIR:LineSweeperFrameBench>/fonts"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../samples/linesweeper/content/textures"
        "$<TARGET_FILE_DIR:LineSweeperFrameBench>/textures"
    VERBATIM
)
