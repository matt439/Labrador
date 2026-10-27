#include <doctest/doctest.h>
#include "tests/render/test_window.h"

#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/colour.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"
#include "engine/render/texture_format.h"
#include "engine/render/viewport.h"

#if defined(_MSC_VER) && defined(_DEBUG) && defined(_DLL)
#include <crtdbg.h>
#include <atomic>
#endif

#include <cstddef>
#include <stdexcept>
#include <vector>

using namespace labrador;
using namespace labrador;

namespace
{
	constexpr int buffer_size = 64;
	constexpr int view_capacity = 4;
	constexpr int measured_view_counts[] = { 4, 3, 1, 4 };

	struct SubmitFixture
	{
		render_tests::TestWindow window{ buffer_size, buffer_size };
		RenderResources resources;
		Renderer renderer;
		TextureHandle texture;

		SubmitFixture()
		{
			renderer.create_device(window.handle(), buffer_size, buffer_size,
				view_capacity);
			renderer.set_resources(&resources);
			TextureData white;
			white.width = 2;
			white.height = 2;
			white.format = TextureFormat::r8g8b8a8_unorm;
			white.levels.push_back(texture_level(white.format, 2, 2, 0));
			white.pixels.resize(white.levels[0].size, 255);
			add_texture_asset(renderer, resources, "d3d12_submit_white", white);
			texture = resources.resolve_texture("d3d12_submit_white");
		}
	};

	void record_views(SubmitFixture& fixture, int view_count)
	{
		fixture.renderer.set_view_count(view_count);
		// Record in reverse view order. View 1 stays empty when declared;
		// lowering the next frame's count leaves the later views undeclared.
		if (view_count > 3)
		{
			DrawList last = fixture.renderer.view(3);
			last.draw_sprite(fixture.texture, RectangleI(0, 0, 2, 2),
				RectangleF(24.0f, 16.0f, 24.0f, 24.0f), Colour::blue, 0.0f,
				Vector2F::zero, SpriteFlip::none, 0.0f);
		}
		if (view_count > 2)
		{
			DrawList middle = fixture.renderer.view(2);
			middle.set_viewport(Viewport(32.0f, 0.0f, 32.0f, 64.0f));
			middle.draw_sprite(fixture.texture, RectangleI(0, 0, 2, 2),
				RectangleF(0.0f, 8.0f, 32.0f, 24.0f), Colour::lime, 0.0f,
				Vector2F::zero, SpriteFlip::none, 0.0f);
		}
		DrawList first = fixture.renderer.view(0);
		first.draw_sprite(fixture.texture, RectangleI(0, 0, 2, 2),
			RectangleF(4.0f, 4.0f, 36.0f, 24.0f), Colour::red, 0.0f,
			Vector2F::zero, SpriteFlip::none, 0.0f);
	}

	std::vector<unsigned char> expected_pixels(int view_count)
	{
		std::vector<unsigned char> expected(buffer_size * buffer_size * 4, 0);
		for (int y = 0; y < buffer_size; y++)
		{
			for (int x = 0; x < buffer_size; x++)
			{
				const size_t offset = static_cast<size_t>(y * buffer_size + x) * 4;
				expected[offset + 3] = 255;
				if (view_count > 3 && x >= 24 && x < 48 && y >= 16 && y < 40)
				{
					expected[offset + 2] = 255;
				}
				else if (view_count > 2 && x >= 32 && x < 64 && y >= 8 && y < 32)
				{
					expected[offset + 1] = 255;
				}
				else if (x >= 4 && x < 40 && y >= 4 && y < 28)
				{
					expected[offset] = 255;
				}
			}
		}
		return expected;
	}
}

TEST_CASE("D3D12 submit executes only declared views in view order, including empty views")
{
	SubmitFixture fixture;
	std::vector<unsigned char> pixels;
	for (const int view_count : measured_view_counts)
	{
		CAPTURE(view_count);
		fixture.renderer.begin_frame();
		record_views(fixture, view_count);
		fixture.renderer.submit();
		fixture.renderer.read_back_buffer(pixels);
		CHECK(pixels == expected_pixels(view_count));
		fixture.renderer.end_frame();
	}
}

#if defined(_MSC_VER) && defined(_DEBUG) && defined(_DLL)
// Observe only normal-block Debug CRT attempts on this submitting thread.
// This cannot observe every driver/system heap or establish a timing gain.
namespace
{
	struct AllocationSample
	{
		size_t attempts = 0;
		size_t bytes = 0;
		bool hook_conflict = false;
	};
	thread_local AllocationSample* active_sample = nullptr;
	std::atomic<_CRT_ALLOC_HOOK> previous_hook = nullptr;
	static_assert(std::atomic<_CRT_ALLOC_HOOK>::is_always_lock_free);
	std::atomic_flag hook_owned = ATOMIC_FLAG_INIT;

	int __cdecl observe_allocation(int operation, void* data, size_t bytes,
		int block_type, long request, const unsigned char* file, int line)
	{
		if (active_sample != nullptr && _BLOCK_TYPE(block_type) == _NORMAL_BLOCK &&
			(operation == _HOOK_ALLOC || operation == _HOOK_REALLOC))
		{
			++active_sample->attempts;
			active_sample->bytes += bytes;
		}
		const _CRT_ALLOC_HOOK previous = previous_hook.load(std::memory_order_relaxed);
		return previous == nullptr ? 1 : previous(operation, data, bytes,
			block_type, request, file, line);
	}

	// TLS limits observations, not ownership of the process-wide hook. These
	// scopes serialize their installations and preserve any prior hook; other
	// instrumentation must not replace it concurrently or recurse into the heap.
	class AllocationScope
	{
	public:
		explicit AllocationScope(AllocationSample& sample) : sample_(sample)
		{
			if (hook_owned.test_and_set(std::memory_order_acquire))
				throw std::logic_error("Allocation probes cannot overlap");
			try
			{
				this->previous_ = _CrtGetAllocHook();
				if (this->previous_ == observe_allocation)
					throw std::logic_error("Unexpected allocation probe hook");
				previous_hook.store(this->previous_, std::memory_order_relaxed);
				const _CRT_ALLOC_HOOK replaced = _CrtSetAllocHook(observe_allocation);
				if (replaced != this->previous_)
				{
					if (_CrtGetAllocHook() == observe_allocation)
						_CrtSetAllocHook(replaced);
					throw std::logic_error("Allocation hook changed concurrently");
				}
				active_sample = &sample;
			}
			catch (...)
			{
				hook_owned.clear(std::memory_order_release);
				throw;
			}
		}
		~AllocationScope()
		{
			active_sample = nullptr;
			if (_CrtGetAllocHook() == observe_allocation)
				_CrtSetAllocHook(this->previous_);
			else
				this->sample_.hook_conflict = true;
			hook_owned.clear(std::memory_order_release);
		}
		AllocationScope(const AllocationScope&) = delete;
		AllocationScope& operator=(const AllocationScope&) = delete;
	private:
		AllocationSample& sample_;
		_CRT_ALLOC_HOOK previous_ = nullptr;
	};
}

TEST_CASE("D3D12 warmed submit makes no normal-block Debug CRT allocation attempts")
{
	const _CRT_ALLOC_HOOK initial = _CrtGetAllocHook();
	AllocationSample calibration;
	size_t control_value = 0;
	{
		AllocationScope scope(calibration);
		const std::vector<size_t> control(32, 7);
		control_value = control[9];
	}
	REQUIRE_FALSE(calibration.hook_conflict);
	REQUIRE(_CrtGetAllocHook() == initial);
	REQUIRE(calibration.attempts > 0);
	REQUIRE(calibration.bytes >= 32 * sizeof(size_t));
	REQUIRE(control_value == 7);

	SubmitFixture fixture;
	std::vector<unsigned char> pixels;
	const std::vector<unsigned char> full_frame = expected_pixels(view_capacity);
	// Warm repeated complete frames with every view declared and all geometry
	// recorded. This does not prove which swapchain slots a hidden Present uses.
	for (int frame = 0; frame < 4; frame++)
	{
		fixture.renderer.begin_frame();
		record_views(fixture, view_capacity);
		fixture.renderer.submit();
		fixture.renderer.read_back_buffer(pixels);
		CHECK(pixels == full_frame);
		fixture.renderer.end_frame();
	}
	for (const int view_count : measured_view_counts)
	{
		fixture.renderer.begin_frame();
		record_views(fixture, view_count);
		const _CRT_ALLOC_HOOK previous = _CrtGetAllocHook();
		AllocationSample sample;
		{
			AllocationScope scope(sample);
			fixture.renderer.submit();
		}
		INFO("declared views ", view_count, ", normal-block attempts ", sample.attempts,
			", requested bytes ", sample.bytes);
		REQUIRE_FALSE(sample.hook_conflict);
		REQUIRE(_CrtGetAllocHook() == previous);
		CHECK(sample.attempts == 0);
		fixture.renderer.read_back_buffer(pixels);
		CHECK(pixels == expected_pixels(view_count));
		fixture.renderer.end_frame();
	}
}
#endif
