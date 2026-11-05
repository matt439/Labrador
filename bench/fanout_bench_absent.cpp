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
// linear_phases, marked as fan-out rows. fanout_measured() answering false is
// what lets main.cpp pass over those two alone; any other phase that recorded
// fewer than two results fails there.

std::string run_fanout_benchmarks()
{
	return "note    the Scene::draw fan-out is measured against the null "
		"backend alone; configure a -null preset for it";
}

bool fanout_measured()
{
	return false;
}
