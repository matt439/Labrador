# The benchmarks PHILOSOPHY's Performance section asks for.
#
# No doctest and no third-party benchmark library: what this needs is a steady
# clock, a warm-up and a median, and a dependency for that would be the
# speculative framework T1 rules out. bench/bench.h says what it asserts and
# what it only reports.
add_executable(ArtAttackBench
    bench.cpp
    main.cpp
    scene_bench.cpp
)
target_link_libraries(ArtAttackBench PRIVATE
    ArtAttackEngine
    artattack_settings
)

# Registered with ctest, which is what "run alongside the test suite" means -
# and it is what stops a benchmark from quietly failing to compile for six
# months. It asserts on complexity class rather than on wall-clock, so it is
# not a flaky test on a busy machine; see main.cpp.
add_test(NAME Benchmarks COMMAND ArtAttackBench)
