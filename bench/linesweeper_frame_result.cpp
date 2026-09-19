#include "bench/linesweeper_frame_result.h"

#include "bench/frame_statistics.h"

#include <Windows.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace linesweeper_frame_bench
{
	namespace
	{
		constexpr int max_frame_count = 100000;
		constexpr int max_refresh = 1000;

		bool ascii_token(const std::wstring& value)
		{
			if (value.empty() || value.size() > 96)
			{
				return false;
			}
			return std::all_of(value.begin(), value.end(), [](wchar_t character)
				{
					return (character >= L'a' && character <= L'z') ||
						(character >= L'A' && character <= L'Z') ||
						(character >= L'0' && character <= L'9') ||
						character == L'-' || character == L'_' || character == L'.';
				});
		}

		std::string ascii_bytes(const std::wstring& value)
		{
			std::string result;
			result.reserve(value.size());
			for (wchar_t character : value)
			{
				result.push_back(static_cast<char>(character));
			}
			return result;
		}

		std::string narrow_ascii(const std::wstring& value)
		{
			if (!ascii_token(value))
			{
				throw std::invalid_argument(
					"Run identifiers must contain only ASCII letters, digits, '.', '_' or '-'.");
			}
			return ascii_bytes(value);
		}

		int positive_integer(const std::wstring& value, const char* option,
			int maximum)
		{
			if (value.empty() || !std::all_of(value.begin(), value.end(),
				[](wchar_t character)
					{ return character >= L'0' && character <= L'9'; }))
			{
				throw std::invalid_argument(std::string(option) +
					" must be a positive integer.");
			}

			const std::string narrow = ascii_bytes(value);
			int parsed = 0;
			const std::from_chars_result parsed_value = std::from_chars(
				narrow.data(), narrow.data() + narrow.size(), parsed);
			if (parsed_value.ec != std::errc{} ||
				parsed_value.ptr != narrow.data() + narrow.size() ||
				parsed < 1 || parsed > maximum)
			{
				throw std::invalid_argument(std::string(option) + " must be in [1, " +
					std::to_string(maximum) + "].");
			}
			return parsed;
		}

		std::string normalized_release_hash(const std::wstring& value)
		{
			if (value.size() != 64 || !std::all_of(value.begin(), value.end(),
				[](wchar_t character)
				{
					return (character >= L'0' && character <= L'9') ||
						(character >= L'a' && character <= L'f') ||
						(character >= L'A' && character <= L'F');
				}))
			{
				throw std::invalid_argument(
					"--release-hash must be exactly 64 hexadecimal characters.");
			}

			std::string result = ascii_bytes(value);
			std::transform(result.begin(), result.end(), result.begin(),
				[](char character)
				{
					return character >= 'A' && character <= 'F'
						? static_cast<char>(character - 'A' + 'a')
						: character;
				});
			return result;
		}

		const char* device_kind(labrador::RenderDeviceKind kind)
		{
			switch (kind)
			{
			case labrador::RenderDeviceKind::hardware:
				return "hardware";
			case labrador::RenderDeviceKind::software:
				return "software";
			case labrador::RenderDeviceKind::null_device:
				return "null";
			default:
				throw std::logic_error("Unknown RenderDeviceKind.");
			}
		}

		std::vector<std::int64_t> phase_samples(
			const std::vector<FrameSample>& samples,
			std::int64_t FrameSample::* field)
		{
			std::vector<std::int64_t> values;
			values.reserve(samples.size());
			for (const FrameSample& sample : samples)
			{
				values.push_back(sample.*field);
			}
			return values;
		}

		template <typename Writer>
		void write_summary(Writer& writer,
			const std::vector<std::int64_t>& values)
		{
			const bench::FrameSummary summary = bench::summarize_frames(values);
			writer.StartObject();
			writer.Key("count");
			writer.Uint64(static_cast<std::uint64_t>(summary.count));
			writer.Key("min");
			writer.Int64(summary.minimum);
			writer.Key("p50");
			writer.Int64(summary.p50);
			writer.Key("p95");
			writer.Int64(summary.p95);
			writer.Key("p99");
			writer.Int64(summary.p99);
			writer.Key("max");
			writer.Int64(summary.maximum);
			writer.EndObject();
		}
	}

	void print_usage()
	{
		std::fwprintf(stderr,
			L"Usage: LineSweeperFrameBench --output FILE.json --run ID "
			L"--release-hash SHA256 [--warmup FRAMES] [--sample FRAMES] "
			L"[--refresh HZ] [--pacing software|presentation]\n");
	}

	Options parse_options(int argc, wchar_t* argv[])
	{
		Options options;
		bool output_seen = false;
		bool run_seen = false;
		bool release_seen = false;
		bool warmup_seen = false;
		bool sample_seen = false;
		bool refresh_seen = false;
		bool pacing_seen = false;

		for (int index = 1; index < argc; index += 2)
		{
			if (index + 1 >= argc)
			{
				throw std::invalid_argument(
					"Every command-line option requires a value.");
			}

			const std::wstring name = argv[index];
			const std::wstring value = argv[index + 1];
			if (name == L"--output" && !output_seen)
			{
				options.output = value;
				output_seen = true;
			}
			else if (name == L"--run" && !run_seen)
			{
				options.run = narrow_ascii(value);
				run_seen = true;
			}
			else if (name == L"--release-hash" && !release_seen)
			{
				options.release_hash = normalized_release_hash(value);
				release_seen = true;
			}
			else if (name == L"--warmup" && !warmup_seen)
			{
				options.warmup = positive_integer(value, "--warmup", max_frame_count);
				warmup_seen = true;
			}
			else if (name == L"--sample" && !sample_seen)
			{
				options.sample = positive_integer(value, "--sample", max_frame_count);
				sample_seen = true;
			}
			else if (name == L"--refresh" && !refresh_seen)
			{
				options.refresh = positive_integer(value, "--refresh", max_refresh);
				refresh_seen = true;
			}
			else if (name == L"--pacing" && !pacing_seen)
			{
				if (value != L"software" && value != L"presentation")
				{
					throw std::invalid_argument(
						"--pacing must be software or presentation.");
				}
				options.pacing = value == L"software"
					? PacingMode::software : PacingMode::presentation;
				pacing_seen = true;
			}
			else
			{
				throw std::invalid_argument(
					"Unknown or repeated command-line option.");
			}
		}

		if (!output_seen || options.output.empty() || !run_seen || !release_seen)
		{
			throw std::invalid_argument(
				"--output, --run and --release-hash are required.");
		}
		if (options.output.extension() != L".json")
		{
			throw std::invalid_argument("--output must name a .json file.");
		}
		if (std::filesystem::exists(options.output))
		{
			throw std::invalid_argument(
				"--output already exists; benchmark evidence is never overwritten.");
		}
		return options;
	}

	std::string result_json(const Options& options, const Result& result)
	{
		rapidjson::StringBuffer buffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
		const labrador::RenderDeviceInfo& device = result.device;
		const std::vector<FrameSample>& samples = result.samples;
		const bool software_pacing = options.pacing == PacingMode::software;

		writer.StartObject();
		writer.Key("schema_version");
		writer.Int(1);
		writer.Key("scope");
		writer.String("linesweeper_frame_benchmark");
		writer.Key("status");
		writer.String("complete");
		writer.Key("run");
		writer.String(options.run.c_str());
		writer.Key("release_sha256");
		writer.String(options.release_hash.c_str());
		writer.Key("started_utc");
		writer.String(result.started_utc.c_str());
		writer.Key("finished_utc");
		writer.String(result.finished_utc.c_str());
		writer.Key("build");
		writer.StartObject();
#ifdef NDEBUG
		writer.Key("configuration");
		writer.String("release");
#else
		writer.Key("configuration");
		writer.String("debug");
#endif
		writer.Key("render_backend");
		writer.String(device.backend.c_str());
		writer.EndObject();
		writer.Key("measurement_class");
		writer.String(result.measurement_class.c_str());

		writer.Key("render_device");
		writer.StartObject();
		writer.Key("device_name");
		writer.String(device.device_name.c_str());
		writer.Key("vendor_id");
		writer.Uint(device.vendor_id);
		writer.Key("device_id");
		writer.Uint(device.device_id);
		writer.Key("kind");
		writer.String(device_kind(device.kind));
		writer.Key("api");
		writer.String(device.api.c_str());
		writer.Key("present_mode");
		writer.String(device.present_mode.c_str());
		writer.Key("requested_swap_interval");
		if (device.requested_swap_interval)
		{
			writer.Int(*device.requested_swap_interval);
		}
		else
		{
			writer.Null();
		}
		writer.Key("reported_swap_interval");
		if (device.reported_swap_interval)
		{
			writer.Int(*device.reported_swap_interval);
		}
		else
		{
			writer.Null();
		}
		writer.EndObject();

		writer.Key("workload");
		writer.StartObject();
		writer.Key("name");
		writer.String("full_well_top_out");
		writer.Key("resolution");
		writer.StartObject();
		writer.Key("width");
		writer.Int(width);
		writer.Key("height");
		writer.Int(height);
		writer.EndObject();
		writer.Key("refresh_hz");
		writer.Int(options.refresh);
		writer.Key("warmup_frames");
		writer.Int(options.warmup);
		writer.Key("sample_frames");
		writer.Int(options.sample);
		writer.Key("live_particles");
		writer.Int(expected_particles);
		writer.Key("target_frame_ns");
		writer.Int64((1000000000LL + options.refresh / 2) / options.refresh);
		writer.Key("frame_order");
		writer.StartArray();
		writer.String("BoardView");
		writer.String("ParticleField");
		writer.String("TopOutBanner");
		writer.String("keyboard Label");
		writer.String("pad Label");
		writer.EndArray();
		writer.EndObject();

		writer.Key("timing");
		writer.StartObject();
		writer.Key("clock");
		writer.String("std::chrono::steady_clock");
		writer.Key("unit");
		writer.String("nanoseconds");
		writer.Key("summary_method");
		writer.String("nearest-rank");
		writer.Key("interval_scope");
		writer.String(software_pacing
			? "software-paced frame-start interval; not display scan-out"
			: "presentation-driven frame-start interval; not display scan-out");
		writer.Key("pacer");
		writer.String(software_pacing
			? "win32_high_resolution_waitable_timer" : "presentation_driven");
		writer.Key("deadline_policy");
		writer.String(software_pacing ? "absolute_catch_up" : "none");
		writer.Key("sample_count");
		writer.Uint64(static_cast<std::uint64_t>(samples.size()));
		writer.Key("scheduled_interval_ns");
		writer.StartArray();
		for (const FrameSample& sample : samples)
		{
			writer.Int64(sample.scheduled_interval_ns);
		}
		writer.EndArray();
		writer.EndObject();

		writer.Key("samples");
		writer.StartArray();
		for (std::size_t index = 0; index < samples.size(); ++index)
		{
			const FrameSample& sample = samples[index];
			writer.StartObject();
			writer.Key("ordinal");
			writer.Uint64(static_cast<std::uint64_t>(index));
			writer.Key("update_ns");
			writer.Int64(sample.update_ns);
			writer.Key("begin_ns");
			writer.Int64(sample.begin_ns);
			writer.Key("record_submit_ns");
			writer.Int64(sample.record_submit_ns);
			writer.Key("present_ns");
			writer.Int64(sample.present_ns);
			writer.Key("whole_frame_ns");
			writer.Int64(sample.whole_frame_ns);
			writer.Key("scheduled_interval_ns");
			writer.Int64(sample.scheduled_interval_ns);
			if (software_pacing)
			{
				writer.Key("pacing_wait_ns");
				writer.Int64(sample.pacing_wait_ns);
				writer.Key("start_lateness_ns");
				writer.Int64(sample.start_lateness_ns);
			}
			writer.EndObject();
		}
		writer.EndArray();

		writer.Key("summary");
		writer.StartObject();
		writer.Key("update_ns");
		write_summary(writer, phase_samples(samples, &FrameSample::update_ns));
		writer.Key("begin_ns");
		write_summary(writer, phase_samples(samples, &FrameSample::begin_ns));
		writer.Key("record_submit_ns");
		write_summary(writer,
			phase_samples(samples, &FrameSample::record_submit_ns));
		writer.Key("present_ns");
		write_summary(writer, phase_samples(samples, &FrameSample::present_ns));
		writer.Key("whole_frame_ns");
		write_summary(writer,
			phase_samples(samples, &FrameSample::whole_frame_ns));
		writer.Key("scheduled_interval_ns");
		write_summary(writer,
			phase_samples(samples, &FrameSample::scheduled_interval_ns));
		if (software_pacing)
		{
			writer.Key("pacing_wait_ns");
			write_summary(writer, phase_samples(samples, &FrameSample::pacing_wait_ns));
			writer.Key("start_lateness_ns");
			write_summary(writer, phase_samples(samples, &FrameSample::start_lateness_ns));
		}
		writer.EndObject();
		writer.EndObject();

		return std::string(buffer.GetString(), buffer.GetSize());
	}

	void atomic_write(const std::filesystem::path& output,
		const std::string& contents)
	{
		const std::filesystem::path parent = output.parent_path();
		if (!parent.empty())
		{
			std::filesystem::create_directories(parent);
		}
		if (std::filesystem::exists(output))
		{
			throw std::runtime_error(
				"Output appeared during the run; benchmark evidence was not overwritten.");
		}

		std::filesystem::path temporary = output;
		temporary += L".tmp." + std::to_wstring(GetCurrentProcessId());
		HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
			CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (file == INVALID_HANDLE_VALUE)
		{
			throw std::runtime_error("Could not create the temporary result file.");
		}

		bool complete = false;
		try
		{
			std::size_t written = 0;
			while (written < contents.size())
			{
				const std::size_t remaining = contents.size() - written;
				const DWORD request = static_cast<DWORD>(std::min<std::size_t>(
					remaining, static_cast<std::size_t>(MAXDWORD)));
				DWORD chunk = 0;
				if (!WriteFile(file, contents.data() + written, request, &chunk,
					nullptr) || chunk == 0)
				{
					throw std::runtime_error("Could not write the benchmark result.");
				}
				written += chunk;
			}
			if (!FlushFileBuffers(file))
			{
				throw std::runtime_error("Could not flush the benchmark result.");
			}
			if (!CloseHandle(file))
			{
				file = INVALID_HANDLE_VALUE;
				throw std::runtime_error("Could not close the benchmark result.");
			}
			file = INVALID_HANDLE_VALUE;

			if (!MoveFileExW(temporary.c_str(), output.c_str(), MOVEFILE_WRITE_THROUGH))
			{
				throw std::runtime_error(
					"Could not atomically publish the benchmark result.");
			}
			complete = true;
		}
		catch (...)
		{
			if (file != INVALID_HANDLE_VALUE)
			{
				CloseHandle(file);
			}
			if (!complete)
			{
				DeleteFileW(temporary.c_str());
			}
			throw;
		}
	}
}
