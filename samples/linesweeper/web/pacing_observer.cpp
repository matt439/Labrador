#include "samples/linesweeper/web/pacing_observer.h"

#include "engine/app/application.h"
#include "engine/render/renderer.h"

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#ifndef LABRADOR_PACING_SOURCE_REVISION
#define LABRADOR_PACING_SOURCE_REVISION "unknown"
#endif
#ifndef LABRADOR_PACING_BUILD_TYPE
#define LABRADOR_PACING_BUILD_TYPE "unknown"
#endif

EM_JS_DEPS(linesweeper_pacing, "$UTF8ToString");

EM_JS(int, linesweeper_pacing_mode, (), {
	return Module["labradorPacingMode"] || 0;
});

EM_JS(void, linesweeper_pacing_build, (const char* revision, const char* build,
	const char* backend, const char* api, const char* device_name), {
	if (typeof Module["labradorPacingBuild"] === "function") {
		Module["labradorPacingBuild"](UTF8ToString(revision), UTF8ToString(build),
			UTF8ToString(backend), UTF8ToString(api), UTF8ToString(device_name));
	}
});

EM_JS(int, linesweeper_pacing_frame, (double timestamp, double frame, int updates,
	double interval, double update, double begin, double draw, double submit,
	double end, double top_out_frame, int live, double dropped), {
	if (typeof Module["labradorPacingFrame"] !== "function") return 0;
	return Module["labradorPacingFrame"](timestamp, frame, updates, interval,
		update, begin, draw, submit, end, top_out_frame, live, dropped) ? 1 : 0;
});

namespace linesweeper
{
	namespace
	{
		struct Observer
		{
			labrador::Application* app = nullptr;
			const PacingSnapshot* snapshot = nullptr;
		};

		bool observe(double timestamp, void* user_data)
		{
			const Observer& observer = *static_cast<const Observer*>(user_data);
			const labrador::FrameTimings timings = observer.app->frame_timings();
			const PacingSnapshot& snapshot = *observer.snapshot;
			return linesweeper_pacing_frame(timestamp,
				static_cast<double>(timings.frame), timings.updates,
				static_cast<double>(timings.interval.count()),
				static_cast<double>(timings.update_time.count()),
				static_cast<double>(timings.begin_frame_time.count()),
				static_cast<double>(timings.draw_time.count()),
				static_cast<double>(timings.submit_time.count()),
				static_cast<double>(timings.end_frame_time.count()),
				static_cast<double>(snapshot.top_out_frame),
				snapshot.live_particles,
				static_cast<double>(snapshot.dropped_particles)) != 0;
		}
	}

	PacingMode pacing_mode()
	{
		return static_cast<PacingMode>(linesweeper_pacing_mode());
	}

	void observe_pacing(labrador::Application& app, const PacingSnapshot& snapshot)
	{
		// run() unwinds main; this callback and its borrowed data must outlive it.
		static Observer observer;
		observer.app = &app;
		observer.snapshot = &snapshot;
		const labrador::RenderDeviceInfo& device = app.renderer()->device_info();
		linesweeper_pacing_build(LABRADOR_PACING_SOURCE_REVISION,
			LABRADOR_PACING_BUILD_TYPE, device.backend.c_str(), device.api.c_str(),
			device.device_name.c_str());
		emscripten_request_animation_frame_loop(observe, &observer);
	}
}
