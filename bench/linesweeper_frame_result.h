#pragma once

#include "engine/app/window_display_info.h"
#include "engine/render/renderer.h"
#include "tools/native_process_observation.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace linesweeper_frame_bench
{
	inline constexpr int width = 1280;
	inline constexpr int height = 720;
	inline constexpr int expected_particles = 9600;

	enum class PacingMode
	{
		software,
		presentation,
	};

	struct Options
	{
		std::filesystem::path output;
		std::string run;
		std::string release_hash;
		int warmup = 1800;
		int sample = 3600;
		int refresh = 60;
		PacingMode pacing = PacingMode::software;
		bool process_observations = false;
	};

	struct FrameSample
	{
		std::int64_t update_ns = 0;
		std::int64_t begin_ns = 0;
		std::int64_t record_submit_ns = 0;
		std::int64_t present_ns = 0;
		std::int64_t whole_frame_ns = 0;
		std::int64_t scheduled_interval_ns = 0;
		std::int64_t pacing_wait_ns = 0;
		std::int64_t start_lateness_ns = 0;
	};

	struct Result
	{
		std::vector<FrameSample> samples;
		labrador::RenderDeviceInfo device;
		labrador::WindowDisplayInfo display_start;
		labrador::WindowDisplayInfo display_end;
		std::string measurement_class;
		std::string started_utc;
		std::string finished_utc;
		std::string source_revision = "unknown";
		native_process_observation::Observation process_observations;
	};

	Options parse_options(int argc, wchar_t* argv[]);
	void print_usage();
	std::string result_json(const Options& options, const Result& result);
	void atomic_write(const std::filesystem::path& output,
		const std::string& contents);
}
