#include "bench/d3d12_descriptor_count_state.h"
#include "bench/linesweeper_frame_bench.h"
#include "bench/linesweeper_frame_result.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
	struct QueryCounts
	{
		std::uint64_t sampler = 0;
		std::uint64_t texture = 0;
		std::uint64_t target = 0;
		std::uint64_t unexpected = 0;
	};
	std::atomic<std::uint64_t> sampler_queries{ 0 };
	std::atomic<std::uint64_t> texture_queries{ 0 };
	std::atomic<std::uint64_t> target_queries{ 0 };
	std::atomic<std::uint64_t> unexpected_queries{ 0 };
	std::atomic<bool> active{ false };
	std::vector<QueryCounts> rows;
	std::size_t rows_used = 0;
	bool incomplete = false;

	// The JSON's classification, so that nothing reads an instrumented run as
	// performance or pacing acceptance.
	const char* const measurement_class =
		"instrumented_descriptor_counts_not_performance_acceptance";
}

namespace descriptor_query_counts
{
	void gpu_query(const char* function) noexcept
	{
		if (!active.load(std::memory_order_relaxed))
		{
			return;
		}
		if (std::strcmp(function, "sampler") == 0)
		{
			sampler_queries.fetch_add(1, std::memory_order_relaxed);
		}
		else if (std::strcmp(function, "texture_slot_gpu") == 0)
		{
			texture_queries.fetch_add(1, std::memory_order_relaxed);
		}
		else
		{
			unexpected_queries.fetch_add(1, std::memory_order_relaxed);
		}
	}
	void back_buffer_query() noexcept
	{
		if (active.load(std::memory_order_relaxed))
		{
			target_queries.fetch_add(1, std::memory_order_relaxed);
		}
	}
	void begin_frame() noexcept
	{
		if (active.load(std::memory_order_relaxed))
		{
			incomplete = true;
		}
		sampler_queries.store(0, std::memory_order_relaxed);
		texture_queries.store(0, std::memory_order_relaxed);
		target_queries.store(0, std::memory_order_relaxed);
		unexpected_queries.store(0, std::memory_order_relaxed);
		active.store(true, std::memory_order_relaxed);
	}
	void end_frame() noexcept
	{
		if (!active.exchange(false, std::memory_order_relaxed) || rows_used >= rows.size())
		{
			incomplete = true;
			return;
		}
		rows[rows_used++] = { sampler_queries.load(std::memory_order_relaxed),
			texture_queries.load(std::memory_order_relaxed),
			target_queries.load(std::memory_order_relaxed),
			unexpected_queries.load(std::memory_order_relaxed) };
		if ((rows_used & (rows_used - 1)) == 0)
		{
			std::fprintf(stderr, "Completed %zu census frames.\n", rows_used);
		}
	}
}

// The real workload, options, content loading and lifecycle, through
// LineSweeperFrameBench's measure_frames; no game code is copied here. Only
// the hidden window and the result's classification differ, and hidden
// startup cannot supply display-pacing acceptance.
int wmain(int argc, wchar_t* argv[])
{
	try
	{
		std::setvbuf(stdout, nullptr, _IONBF, 0);
		const linesweeper_frame_bench::Options options =
			linesweeper_frame_bench::parse_options(argc, argv);
		rows.resize(static_cast<std::size_t>(options.warmup) +
			static_cast<std::size_t>(options.sample));
		std::puts("COUNTS ONLY: instrumented timings are intrusive; no pacing, low-tier or p99 "
			"acceptance.");
		linesweeper_frame_bench::Result result = linesweeper_frame_bench::measure_frames(
			options, linesweeper_frame_bench::WindowVisibility::hidden);
		result.measurement_class = measurement_class;
		linesweeper_frame_bench::atomic_write(options.output,
			linesweeper_frame_bench::result_json(options, result));
		std::printf("Wrote %d retained frames to %ls (%s, %s).\n",
			options.sample, options.output.c_str(),
			result.device.backend.c_str(), result.measurement_class.c_str());

		if (incomplete || active.load(std::memory_order_relaxed) || rows_used != rows.size())
		{
			throw std::runtime_error("Descriptor census did not capture exactly the requested "
				"frames");
		}
		std::string csv = "# instrumented getter invocations, not production timings\n"
			"# source_revision=" LABRADOR_DESCRIPTOR_SOURCE_REVISION "\n"
			"# renderer_sha256=" LABRADOR_DESCRIPTOR_RENDERER_SHA256 "\n"
			"# device_resources_sha256=" LABRADOR_DESCRIPTOR_DEVICE_SHA256 "\n"
			"frame,retained,sampler_calls,texture_slot_gpu_calls,back_buffer_view_calls\n";
		for (std::size_t index = 0; index < rows_used; ++index)
		{
			const QueryCounts& row = rows[index];
			if (row.unexpected != 0)
			{
				throw std::runtime_error("Unexpected GPU heap query call shape");
			}
			if (row.sampler == 0 || row.sampler != row.texture || row.target == 0)
			{
				throw std::runtime_error("The workload did not observe paired GPU "
					"getters and an RTV getter");
			}
			csv += std::to_string(index) + "," +
				(index >= static_cast<std::size_t>(options.warmup) ? "1," : "0,") +
				std::to_string(row.sampler) + "," + std::to_string(row.texture) + "," +
				std::to_string(row.target) + "\n";
		}
		std::filesystem::path counts_path = options.output;
		counts_path += L".descriptor-counts.csv";
		linesweeper_frame_bench::atomic_write(counts_path, csv);
		std::printf("Wrote %zu invocation-count rows to %ls.\n", rows_used, counts_path.c_str());
		return 0;
	}
	catch (const std::exception& error)
	{
		std::fputs("D3D12DescriptorCountProbe takes LineSweeperFrameBench's options.\n",
			stderr);
		linesweeper_frame_bench::print_usage();
		std::fprintf(stderr, "D3D12DescriptorCountProbe refused: %s\n", error.what());
		return 1;
	}
}
