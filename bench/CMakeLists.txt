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
