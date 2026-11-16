#pragma once

#if defined(_WIN32)
#include <Windows.h>
#include <psapi.h>
#endif

#include <chrono>
#include <cstdint>
#include <limits>
#include <optional>
#include <ratio>
#include <string>
#include <string_view>
#include <utility>

namespace native_process_observation
{
	// Reporting data only: resident peak, private committed bytes at one
	// explicitly named live-process endpoint, and UTC startup endpoints.
	struct Observation
	{
		bool requested = false;
		std::string memory_status = "not_requested";
		std::string startup_status = "not_requested";
		std::optional<std::uint64_t> peak_working_set_bytes;
		std::optional<std::uint64_t> private_bytes_at_exit_endpoint;
		std::optional<std::uint64_t> memory_sample_utc_100ns;
		std::optional<std::uint64_t> process_creation_utc_100ns;
		std::optional<std::uint64_t> first_completed_frame_utc_100ns;
		std::optional<std::uint64_t> process_start_to_first_completed_frame_100ns;
		std::uint32_t memory_error = 0;
		std::uint32_t process_times_error = 0;
		std::uint32_t process_id = 0;
		std::string memory_endpoint;
	};

	inline std::optional<std::uint64_t> utc_filetime_ticks(
		std::chrono::system_clock::time_point point)
	{
		using FileTicks = std::chrono::duration<std::int64_t, std::ratio<1, 10000000>>;
		static_assert(std::ratio_less_equal_v<std::chrono::system_clock::period, std::ratio<1>>);
		constexpr std::uint64_t epoch_offset = 116444736000000000ULL;
		constexpr std::uint64_t ticks_per_second = 10000000;
		const std::chrono::system_clock::duration duration = point.time_since_epoch();
		if (duration < std::chrono::system_clock::duration::zero())
		{
			return std::nullopt;
		}
		// Split before converting: wasm's microsecond clock can overflow a
		// signed 100 ns duration_cast at an extreme endpoint.
		const std::chrono::seconds whole = std::chrono::duration_cast<std::chrono::seconds>(duration);
		const std::uint64_t room = std::numeric_limits<std::uint64_t>::max() - epoch_offset;
		const std::uint64_t seconds = static_cast<std::uint64_t>(whole.count());
		if (seconds > room / ticks_per_second)
		{
			return std::nullopt;
		}
		const std::uint64_t base = seconds * ticks_per_second;
		const std::uint64_t remainder = static_cast<std::uint64_t>(
			std::chrono::duration_cast<FileTicks>(duration - whole).count());
		if (remainder > room - base)
		{
			return std::nullopt;
		}
		return base + remainder + epoch_offset;
	}

	// Pure endpoint handling. Unknown, failed or backwards wall-clock results
	// stay absent; zero is a valid elapsed value, not an unavailable sentinel.
	inline void resolve_startup(Observation& observation)
	{
		observation.process_start_to_first_completed_frame_100ns.reset();
		if (!observation.first_completed_frame_utc_100ns)
		{
			observation.startup_status = "first_completed_frame_unavailable";
		}
		else if (!observation.process_creation_utc_100ns)
		{
			observation.startup_status = "process_creation_unavailable";
		}
		else if (*observation.first_completed_frame_utc_100ns <
			*observation.process_creation_utc_100ns)
		{
			observation.startup_status = "utc_clock_precedes_process_creation";
		}
		else
		{
			observation.process_start_to_first_completed_frame_100ns =
				*observation.first_completed_frame_utc_100ns -
				*observation.process_creation_utc_100ns;
			observation.startup_status = "observed";
		}
	}

	inline Observation capture(
		std::optional<std::chrono::system_clock::time_point> first_completed,
		std::string memory_endpoint)
	{
		Observation observation;
		observation.requested = true;
		observation.memory_endpoint = std::move(memory_endpoint);
#if defined(_WIN32)
		observation.process_id = GetCurrentProcessId();
		PROCESS_MEMORY_COUNTERS_EX counters = {};
		counters.cb = sizeof(counters);
		if (GetProcessMemoryInfo(GetCurrentProcess(),
			reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)))
		{
			observation.memory_status = "observed";
			observation.peak_working_set_bytes = counters.PeakWorkingSetSize;
			observation.private_bytes_at_exit_endpoint = counters.PrivateUsage;
		}
		else
		{
			observation.memory_status = "GetProcessMemoryInfo_failed";
			observation.memory_error = GetLastError();
		}
		FILETIME sampled = {};
		GetSystemTimePreciseAsFileTime(&sampled);
		observation.memory_sample_utc_100ns =
			(static_cast<std::uint64_t>(sampled.dwHighDateTime) << 32) |
			sampled.dwLowDateTime;
		FILETIME created = {}, exited = {}, kernel = {}, user = {};
		if (GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user))
		{
			observation.process_creation_utc_100ns =
				(static_cast<std::uint64_t>(created.dwHighDateTime) << 32) |
				created.dwLowDateTime;
		}
		else
		{
			observation.process_times_error = GetLastError();
		}
		if (first_completed)
		{
			observation.first_completed_frame_utc_100ns =
				utc_filetime_ticks(*first_completed);
		}
		resolve_startup(observation);
#else
		static_cast<void>(first_completed);
		observation.memory_status = "unsupported_platform";
		observation.startup_status = "unsupported_platform";
#endif
		return observation;
	}

	inline std::string json_string(std::string_view text)
	{
		std::string result = "\"";
		constexpr char digits[] = "0123456789abcdef";
		for (unsigned char character : text)
		{
			if (character == '"' || character == '\\')
			{
				result += '\\';
				result += static_cast<char>(character);
			}
			else if (character < 32)
			{
				result += "\\u00";
				result += digits[character >> 4];
				result += digits[character & 15];
			}
			else
			{
				result += static_cast<char>(character);
			}
		}
		return result + '"';
	}

	inline std::string nullable(std::optional<std::uint64_t> value)
	{
		return value ? std::to_string(*value) : "null";
	}

	inline std::string json(const Observation& observation)
	{
		return "{\"requested\":" + std::string(observation.requested ? "true" : "false") +
			",\"memory_status\":" + json_string(observation.memory_status) +
			",\"startup_status\":" + json_string(observation.startup_status) +
			",\"memory_endpoint\":" + json_string(observation.memory_endpoint) +
			",\"peak_working_set_bytes\":" + nullable(observation.peak_working_set_bytes) +
			",\"private_bytes_at_exit_endpoint\":" + nullable(observation.private_bytes_at_exit_endpoint) +
			",\"memory_sample_utc_100ns\":" + nullable(observation.memory_sample_utc_100ns) +
			",\"process_creation_utc_100ns\":" + nullable(observation.process_creation_utc_100ns) +
			",\"first_completed_frame_utc_100ns\":" + nullable(observation.first_completed_frame_utc_100ns) +
			",\"process_start_to_first_completed_frame_100ns\":" + nullable(observation.process_start_to_first_completed_frame_100ns) +
			",\"memory_error\":" + std::to_string(observation.memory_error) +
			",\"process_times_error\":" + std::to_string(observation.process_times_error) +
			",\"process_id\":" + std::to_string(observation.process_id) +
			",\"startup_clock\":\"UTC wall clock; subject to clock adjustment\"," +
			"\"startup_origin\":\"GetProcessTimes process creation, not entry point\"," +
			"\"frame_endpoint\":\"after first successful end_frame, retained only for an accepted frame; not scan-out\"}";
	}
}
