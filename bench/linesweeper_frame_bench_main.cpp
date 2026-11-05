#include "bench/linesweeper_frame_bench.h"
#include "bench/linesweeper_frame_result.h"

#include <cstdio>
#include <exception>

// A command line it cannot run exits 2 with the usage, before anything
// starts; a run that fails exits 1 with the reason.
int wmain(int argc, wchar_t* argv[])
{
	linesweeper_frame_bench::Options options;
	try
	{
		options = linesweeper_frame_bench::parse_options(argc, argv);
	}
	catch (const std::exception& error)
	{
		linesweeper_frame_bench::print_usage();
		std::fprintf(stderr, "LineSweeperFrameBench refused: %s\n", error.what());
		return 2;
	}
	try
	{
		const linesweeper_frame_bench::Result result =
			linesweeper_frame_bench::measure_frames(options,
				linesweeper_frame_bench::WindowVisibility::shown);
		linesweeper_frame_bench::atomic_write(options.output,
			linesweeper_frame_bench::result_json(options, result));

		std::printf("Wrote %d retained frames to %ls (%s, %s).\n",
			options.sample, options.output.c_str(),
			result.device.backend.c_str(), result.measurement_class.c_str());
		return 0;
	}
	catch (const std::exception& error)
	{
		std::fprintf(stderr, "LineSweeperFrameBench failed: %s\n", error.what());
		return 1;
	}
}
