#include <string>

// What the other four configurations get instead of a fan-out case.
//
// bench/fanout_bench_null.cpp drives Scene::draw for real, which needs a
// Renderer, which needs a device - and the null backend is the only one that
// has a Renderer without a window and an adapter. So the case exists in one
// configuration of five, and this file is the sentence saying so where a reader
// of the table would otherwise see nothing at all.
//
// A SOURCE LIST RATHER THAN AN #ifdef, which is the choice
// tests/render/CMakeLists.txt already argues for null_tests.cpp beside
// RenderTests: a backend is chosen at build time, so a file that names one only
// compiles when that one is built. Keeping the switch in
// bench/CMakeLists.txt leaves bench/main.cpp knowing nothing about which
// backend it was compiled against, which is how the rest of that file reads.
//
// The two names the fan-out case records are still listed in main.cpp's
// LINEAR_PHASES. A phase with fewer than two results is skipped there, so they
// cost nothing here and do not have to be moved in and out with the file.

std::string run_fanout_benchmarks()
{
	return "note    the Scene::draw fan-out is measured against the null "
		"backend alone; configure x64-debug-null for it";
}
