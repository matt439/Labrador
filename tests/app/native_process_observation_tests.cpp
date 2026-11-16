#include <doctest/doctest.h>

#include "tools/native_process_observation.h"

#include <chrono>
#include <string>

using native_process_observation::Observation;
using native_process_observation::resolve_startup;

TEST_CASE("native process startup endpoints preserve missing failed and backwards values")
{
	Observation observation;
	resolve_startup(observation);
	CHECK(observation.startup_status == "first_completed_frame_unavailable");
	CHECK_FALSE(observation.process_start_to_first_completed_frame_100ns);
	observation.first_completed_frame_utc_100ns = 40;
	observation.process_times_error = 5;
	resolve_startup(observation);
	CHECK(observation.startup_status == "process_creation_unavailable");
	CHECK(observation.process_times_error == 5);
	observation.process_creation_utc_100ns = 50;
	resolve_startup(observation);
	CHECK(observation.startup_status == "utc_clock_precedes_process_creation");
	CHECK_FALSE(observation.process_start_to_first_completed_frame_100ns);
	observation.first_completed_frame_utc_100ns = 75;
	resolve_startup(observation);
	CHECK(observation.process_start_to_first_completed_frame_100ns == 25);
	observation.first_completed_frame_utc_100ns = 50;
	resolve_startup(observation);
	CHECK(observation.process_start_to_first_completed_frame_100ns == 0);
	CHECK(observation.startup_status == "observed");
}

TEST_CASE("UTC conversion uses the FILETIME origin and JSON strings escape control characters")
{
	const std::chrono::system_clock::time_point unix_epoch;
	CHECK(native_process_observation::utc_filetime_ticks(unix_epoch) ==
		116444736000000000ULL);
	CHECK(native_process_observation::utc_filetime_ticks(unix_epoch +
		std::chrono::seconds(2)) == 116444736020000000ULL);
	CHECK_FALSE(native_process_observation::utc_filetime_ticks(unix_epoch -
		std::chrono::seconds(1)));
	CHECK_FALSE(native_process_observation::utc_filetime_ticks(
		std::chrono::system_clock::time_point::min()));
	const std::optional<std::uint64_t> largest = native_process_observation::utc_filetime_ticks(
		std::chrono::system_clock::time_point::max());
	if constexpr (std::ratio_greater_v<std::chrono::system_clock::period, std::ratio<1, 10000000>>)
	{
		CHECK_FALSE(largest);
	}
	else
	{
		CHECK(largest.has_value());
	}
	CHECK(native_process_observation::json_string("a\"b\\c\n") == "\"a\\\"b\\\\c\\u000a\"");
	const std::string json = native_process_observation::json(Observation());
	CHECK(json.find("\"peak_working_set_bytes\":null") != std::string::npos);
	CHECK(json.find("\"memory_status\":\"not_requested\"") != std::string::npos);
}

TEST_CASE("native process collection states unsupported platforms and uses process creation")
{
	const Observation observation = native_process_observation::capture(
		std::chrono::system_clock::now(), "test_before_report_assembly");
	CHECK(observation.requested);
#if defined(_WIN32)
	REQUIRE(observation.process_creation_utc_100ns.has_value());
	FILETIME created = {}, exited = {}, kernel = {}, user = {};
	REQUIRE(GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user));
	const std::uint64_t expected =
		(static_cast<std::uint64_t>(created.dwHighDateTime) << 32) | created.dwLowDateTime;
	CHECK(*observation.process_creation_utc_100ns == expected);
	CHECK(observation.memory_status == "observed");
	CHECK(observation.peak_working_set_bytes.has_value());
	CHECK(observation.private_bytes_at_exit_endpoint.has_value());
	CHECK(observation.memory_sample_utc_100ns.has_value());
#else
	CHECK(observation.memory_status == "unsupported_platform");
	CHECK(observation.startup_status == "unsupported_platform");
	CHECK_FALSE(observation.peak_working_set_bytes);
	CHECK_FALSE(observation.process_creation_utc_100ns);
#endif
}
