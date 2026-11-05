#pragma once

#include "bench/linesweeper_frame_result.h"

namespace linesweeper_frame_bench
{
	// Whether the benchmark window is shown. A hidden window is for
	// instrumented runs: it supplies no display-pacing evidence.
	enum class WindowVisibility
	{
		shown,
		hidden,
	};

	// Runs the LineSweeper frame workload `options` describes, warm-up and
	// retained sample, and returns the retained samples, the render device and
	// the measurement class. Writes nothing; the caller labels and publishes
	// the result.
	//
	// Throws std::runtime_error when the workload cannot be measured as
	// declared. Among the cases: the build is not Release; the CPU lacks the
	// renderer's instruction set; COM cannot be initialized (CoInitializeEx);
	// the device is not a hardware or null one; presentation-driven pacing is
	// asked of the null backend; the content fails to load; the frozen
	// workload is not exactly 9,600 live particles; the window's client area
	// is not 1280x720, or it is resized during measurement; the application is
	// suspended or the device lost during measurement; or the window closes
	// before the sample completes.
	Result measure_frames(const Options& options, WindowVisibility visibility);
}
