#include "bench/linesweeper_frame_result.h"

#include <doctest/doctest.h>
#include <rapidjson/document.h>

#include <Windows.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

using linesweeper_frame_bench::FrameSample;
using linesweeper_frame_bench::Options;
using linesweeper_frame_bench::PacingMode;
using linesweeper_frame_bench::Result;
using linesweeper_frame_bench::atomic_write;
using linesweeper_frame_bench::parse_options;
using linesweeper_frame_bench::result_json;

namespace
{
	std::vector<wchar_t*> arguments(std::vector<std::wstring>& values)
	{
		std::vector<wchar_t*> pointers;
		pointers.reserve(values.size());
		for (std::wstring& value : values)
		{
			pointers.push_back(value.data());
		}
		return pointers;
	}

	std::filesystem::path temporary_result()
	{
		return std::filesystem::temp_directory_path() /
			("labrador-frame-result-" + std::to_string(GetCurrentProcessId()) +
				"-" + std::to_string(std::chrono::steady_clock::now()
					.time_since_epoch().count()) + ".json");
	}

	class RemoveResult final
	{
	public:
		explicit RemoveResult(std::filesystem::path path) : path_(std::move(path)) {}

		~RemoveResult()
		{
			std::error_code ignored;
			std::filesystem::remove(this->path_, ignored);
		}

		RemoveResult(const RemoveResult&) = delete;
		RemoveResult& operator=(const RemoveResult&) = delete;

	private:
		std::filesystem::path path_;
	};
}

TEST_SUITE("LineSweeper frame result")
{
	TEST_CASE("the command line binds output, run and release identity")
	{
		const std::filesystem::path output = temporary_result();
		REQUIRE_FALSE(std::filesystem::exists(output));
		std::vector<std::wstring> values = {
			L"LineSweeperFrameBench", L"--output", output.wstring(),
			L"--run", L"reference-001", L"--release-hash",
			std::wstring(64, L'a'), L"--warmup", L"12", L"--sample", L"345",
			L"--refresh", L"60",
		};
		std::vector<wchar_t*> argv = arguments(values);

		const Options options = parse_options(static_cast<int>(argv.size()), argv.data());

		CHECK(options.output == output);
		CHECK(options.run == "reference-001");
		CHECK(options.release_hash == std::string(64, 'a'));
		CHECK(options.warmup == 12);
		CHECK(options.sample == 345);
		CHECK(options.refresh == 60);
		CHECK(options.pacing == PacingMode::software);
	}

	TEST_CASE("presentation pacing is explicit and rejects misspelled or repeated modes")
	{
		std::vector<std::wstring> values = {
			L"LineSweeperFrameBench", L"--output", temporary_result().wstring(),
			L"--run", L"diagnostic", L"--release-hash", std::wstring(64, L'a'),
			L"--pacing", L"presentation",
		};
		std::vector<wchar_t*> argv = arguments(values);
		CHECK(parse_options(static_cast<int>(argv.size()), argv.data()).pacing ==
			PacingMode::presentation);

		values.back() = L"unpaced";
		argv = arguments(values);
		CHECK_THROWS_AS(parse_options(static_cast<int>(argv.size()), argv.data()),
			std::invalid_argument);

		values.back() = L"software";
		values.push_back(L"--pacing");
		values.push_back(L"presentation");
		argv = arguments(values);
		CHECK_THROWS_AS(parse_options(static_cast<int>(argv.size()), argv.data()),
			std::invalid_argument);
	}

	TEST_CASE("presentation diagnostics do not fabricate software waits or deadlines")
	{
		Options options;
		options.pacing = PacingMode::presentation;
		Result result;
		result.samples.push_back(FrameSample{ 1, 2, 3, 4, 10, 16, 6, 2 });
		const std::string json = result_json(options, result);
		rapidjson::Document document;
		document.Parse(json.c_str());
		REQUIRE_FALSE(document.HasParseError());
		CHECK(document["timing"]["interval_scope"].GetString() ==
			std::string("presentation-driven frame-start interval; not display scan-out"));
		CHECK(document["timing"]["pacer"].GetString() == std::string("presentation_driven"));
		CHECK(document["timing"]["deadline_policy"].GetString() ==
			std::string("none"));
		CHECK_FALSE(document["samples"][0].HasMember("pacing_wait_ns"));
		CHECK_FALSE(document["samples"][0].HasMember("start_lateness_ns"));
		CHECK_FALSE(document["summary"].HasMember("pacing_wait_ns"));
		CHECK_FALSE(document["summary"].HasMember("start_lateness_ns"));
		CHECK(document["samples"][0]["whole_frame_ns"].GetInt64() == 10);
		CHECK(document["summary"]["scheduled_interval_ns"]["p99"].GetInt64() == 16);
	}

	TEST_CASE("the JSON names scheduled cadence and every measured phase")
	{
		Options options;
		options.run = "reference-001";
		options.release_hash = std::string(64, 'b');
		options.warmup = 1;
		options.sample = 1;
		options.refresh = 60;

		Result result;
		result.samples.push_back(FrameSample{ 1, 2, 3, 4, 10, 16, 6, 2 });
		result.device.backend = "d3d11";
		result.device.api = "Direct3D 11";
		result.device.device_name = "test adapter";
		result.device.vendor_id = 1;
		result.device.device_id = 2;
		result.device.kind = labrador::RenderDeviceKind::hardware;
		result.device.present_mode = "dxgi_sync_interval";
		result.device.requested_swap_interval = 1;
		result.measurement_class = "hardware_raster";
		result.started_utc = "2030-01-01T00:00:00Z";
		result.finished_utc = "2030-01-01T00:00:01Z";

		const std::string json = result_json(options, result);
		rapidjson::Document document;
		document.Parse(json.c_str());

		REQUIRE_FALSE(document.HasParseError());
		CHECK(document["timing"]["interval_scope"].GetString() ==
			std::string("software-paced frame-start interval; not display scan-out"));
		CHECK(document["timing"]["scheduled_interval_ns"][0].GetInt64() == 16);
		CHECK(document["samples"][0]["update_ns"].GetInt64() == 1);
		CHECK(document["samples"][0]["whole_frame_ns"].GetInt64() == 10);
		CHECK(document["summary"]["scheduled_interval_ns"]["p99"].GetInt64() == 16);
		CHECK(document["timing"]["pacer"].GetString() ==
			std::string("win32_high_resolution_waitable_timer"));
		CHECK(document["timing"]["deadline_policy"].GetString() ==
			std::string("absolute_catch_up"));
		CHECK(document["samples"][0]["pacing_wait_ns"].GetInt64() == 6);
		CHECK(document["samples"][0]["start_lateness_ns"].GetInt64() == 2);
		CHECK(document["summary"]["pacing_wait_ns"]["p99"].GetInt64() == 6);
		CHECK(document["summary"]["start_lateness_ns"]["p99"].GetInt64() == 2);
		CHECK(document["render_device"]["present_mode"].GetString() ==
			std::string("dxgi_sync_interval"));
		CHECK(document["render_device"]["requested_swap_interval"].GetInt() == 1);
		CHECK(document["render_device"]["reported_swap_interval"].IsNull());
		CHECK_FALSE(document.HasMember("device"));

		result.device.backend = "gl";
		result.device.present_mode = "wgl_swap_interval";
		result.device.reported_swap_interval = 0;
		const std::string gl_json = result_json(options, result);
		document.Parse(gl_json.c_str());
		REQUIRE_FALSE(document.HasParseError());
		CHECK(document["render_device"]["reported_swap_interval"].GetInt() == 0);
	}

	TEST_CASE("atomic publication refuses to overwrite evidence")
	{
		const std::filesystem::path path = temporary_result();
		REQUIRE_FALSE(std::filesystem::exists(path));
		[[maybe_unused]] const RemoveResult cleanup(path);

		atomic_write(path, "first");

		CHECK(std::filesystem::exists(path));
		CHECK_THROWS_AS(atomic_write(path, "second"), std::runtime_error);
	}
}
