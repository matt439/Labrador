#include "bench/d3d12_descriptor_count_state.h"
#include "bench/linesweeper_frame_result.h"

#include "engine/app/window.h"

#include <Windows.h>

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

	labrador::WindowOptions hidden_options(const labrador::WindowOptions& options)
	{
		labrador::WindowOptions hidden = options;
		hidden.visible = false;
		return hidden;
	}

	class DescriptorCountWindow final : public labrador::Window
	{
	public:
		DescriptorCountWindow(const labrador::WindowOptions& options,
			labrador::WindowNotify* notify) : Window(hidden_options(options), notify)
		{
			RECT client = {};
			if (!GetClientRect(static_cast<HWND>(this->handle()), &client))
			{
				throw std::runtime_error("The descriptor census could not query its hidden client "
					"area");
			}
			// The normal benchmark receives WM_SIZE when shown. Counting uses
			// a hidden window and supplies its actual queried size instead.
			notify->on_window_size_changed(static_cast<int>(client.right - client.left),
				static_cast<int>(client.bottom - client.top));
		}
	};
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

namespace linesweeper_frame_bench
{
	std::string instrumented_result_json(const Options& options, const Result& result)
	{
		Result tagged = result;
		tagged.measurement_class = "instrumented_descriptor_counts_not_performance_acceptance";
		return result_json(options, tagged);
	}
}

// Reuse the real workload, options, content loading and lifecycle. Only its
// entry point, hidden startup and result labelling differ; no game code is
// copied here. Hidden startup cannot supply display-pacing acceptance.
#define wmain descriptor_count_workload_main
#define result_json instrumented_result_json
#define Window DescriptorCountWindow
#include "bench/linesweeper_frame_bench.cpp"
#undef Window
#undef result_json
#undef wmain

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
		const int result = descriptor_count_workload_main(argc, argv);
		if (result != 0)
		{
			return result;
		}
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
		std::fprintf(stderr, "D3D12DescriptorCountProbe refused: %s\n", error.what());
		return 1;
	}
}
