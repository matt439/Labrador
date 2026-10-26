#include "engine/render/d3d12/backend.h"
#include "tests/render/test_window.h"

#include <Windows.h>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <stdexcept>

namespace
{
	using GetterValue = std::uint64_t;
	volatile GetterValue observed_checksum = 0;
	constexpr std::uint64_t warmup_iterations = 100000;

	bool release_build()
	{
#if defined(NDEBUG)
		return true;
#else
		return false;
#endif
	}

	struct Sample
	{
		LONGLONG ticks;
		GetterValue checksum;
	};

	template <typename Getter>
	Sample measure(std::uint64_t iterations, Getter getter)
	{
		GetterValue checksum = 0;
		LARGE_INTEGER start = {};
		LARGE_INTEGER finish = {};
		if (!QueryPerformanceCounter(&start))
			throw std::runtime_error("QueryPerformanceCounter failed");
		for (std::uint64_t index = 0; index < iterations; ++index)
			checksum += getter();
		if (!QueryPerformanceCounter(&finish))
			throw std::runtime_error("QueryPerformanceCounter failed");
		observed_checksum = checksum;
		return { finish.QuadPart - start.QuadPart, checksum };
	}

	template <typename Getter>
	void report(const char* name, int repetition, std::uint64_t iterations,
		GetterValue expected, Getter getter)
	{
		const Sample sample = measure(iterations, getter);
		if (sample.checksum != expected * iterations)
			throw std::runtime_error("Descriptor checksum changed during the probe");
		std::printf("%s,%d,%llu,%lld,%llu\n", name, repetition,
			static_cast<unsigned long long>(iterations), sample.ticks,
			static_cast<unsigned long long>(sample.checksum));
	}
}

int main(int argc, char* argv[])
{
	try
	{
		if (!release_build())
			throw std::runtime_error("D3D12DescriptorQueryProbe requires Release");
		std::uint64_t iterations = 1000000;
		if (argc > 2)
			throw std::invalid_argument("Usage: D3D12DescriptorQueryProbe [iterations]");
		if (argc == 2)
		{
			char* end = nullptr;
			errno = 0;
			iterations = std::strtoull(argv[1], &end, 10);
			if (errno != 0 || end == argv[1] || *end != '\0' ||
				iterations == 0 || iterations > 100000000)
				throw std::invalid_argument("iterations must be between 1 and 100000000");
		}
		render_tests::TestWindow window(320, 200);
		labrador::Renderer renderer;
		renderer.create_device(window.handle(), 320, 200, 1);
		labrador::Renderer::Impl& impl = *renderer.impl();
		const labrador::RenderDeviceInfo& device = renderer.device_info();
		const LUID luid = impl.device_resources.device()->GetAdapterLuid();
		LARGE_INTEGER frequency = {};
		if (!QueryPerformanceFrequency(&frequency))
			throw std::runtime_error("QueryPerformanceFrequency failed");
		std::printf("source_revision=%s\nrenderer_sha256=%s\ndevice_resources_sha256=%s\n",
			LABRADOR_DESCRIPTOR_SOURCE_REVISION, LABRADOR_DESCRIPTOR_RENDERER_SHA256,
			LABRADOR_DESCRIPTOR_DEVICE_SHA256);
		std::printf("device=%s\napi=%s\nkind=%d\nvendor_id=%u\ndevice_id=%u\n"
			"adapter_luid=%08lx:%08lx\ncompiler_msc=%d\nqpc_frequency=%lld\n"
			"warmup_iterations=%llu\n", device.device_name.c_str(), device.api.c_str(),
			static_cast<int>(device.kind), device.vendor_id,
			device.device_id, static_cast<unsigned long>(luid.HighPart),
			luid.LowPart, _MSC_FULL_VER, frequency.QuadPart,
			static_cast<unsigned long long>(warmup_iterations));
		std::puts("scope=unmodified engine methods; single calling thread; no affinity/power changes");
		std::puts("baseline=volatile cached handle load; includes loop/checksum work; no timing thresholds");
		std::puts("configure-time source hashes require a fresh configure after source edits");

		const auto sampler = [&impl]() { return impl.sampler(labrador::TextureFilter::point).ptr; };
		const auto texture = [&impl]() { return impl.texture_slot_gpu(0).ptr; };
		const auto target = [&impl]() { return impl.device_resources.back_buffer_view().ptr; };
		volatile GetterValue cached_sampler = sampler();
		volatile GetterValue cached_texture = texture();
		volatile GetterValue cached_target = target();
		const auto sampler_load = [&cached_sampler]() { return cached_sampler; };
		const auto texture_load = [&cached_texture]() { return cached_texture; };
		const auto target_load = [&cached_target]() { return cached_target; };
		static_cast<void>(measure(warmup_iterations, sampler));
		static_cast<void>(measure(warmup_iterations, texture));
		static_cast<void>(measure(warmup_iterations, target));
		static_cast<void>(measure(warmup_iterations, sampler_load));
		static_cast<void>(measure(warmup_iterations, texture_load));
		static_cast<void>(measure(warmup_iterations, target_load));
		std::puts("method,repetition,iterations,raw_qpc_ticks,checksum");
		for (int repetition = 0; repetition < 7; ++repetition)
		{
			report("sampler_point", repetition, iterations, cached_sampler, sampler);
			report("sampler_cached_load", repetition, iterations, cached_sampler, sampler_load);
			report("texture_slot_gpu_0", repetition, iterations, cached_texture, texture);
			report("texture_cached_load", repetition, iterations, cached_texture, texture_load);
			report("back_buffer_view", repetition, iterations, cached_target, target);
			report("target_cached_load", repetition, iterations, cached_target, target_load);
		}
		return 0;
	}
	catch (const std::exception& error)
	{
		std::fprintf(stderr, "D3D12DescriptorQueryProbe refused: %s\n", error.what());
		return 1;
	}
}
