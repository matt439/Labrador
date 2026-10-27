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

	struct SubmitFixture
	{
		render_tests::TestWindow window{ buffer_size, buffer_size };
		RenderResources resources;
		Renderer renderer;
		TextureHandle texture;

		SubmitFixture()
		{
			renderer.create_device(window.handle(), buffer_size, buffer_size, 3);
			renderer.set_resources(&resources);
			TextureData white;
			white.width = 2;
			white.height = 2;
			white.levels.push_back(texture_level(white.format, 2, 2, 0));
			white.pixels.resize(white.levels[0].size, 255);
			add_texture_asset(renderer, resources, "submit_allocation_white", white);
			texture = resources.resolve_texture("submit_allocation_white");
		}
	};

	void record_views(SubmitFixture& fixture)
	{
		fixture.renderer.set_view_count(3);
		DrawList skipped = fixture.renderer.view(0);
		skipped.set_viewport(Viewport(0.0f, 0.0f, 0.0f, 0.0f));
		// These distinct uploaded vertices still contribute to later bases.
		skipped.draw_sprite(fixture.texture, RectangleI(0, 0, 2, 2),
			RectangleF(0.0f, 0.0f, 64.0f, 64.0f), Colour::blue, 0.0f,
			Vector2F::zero, SpriteFlip::none, 0.0f);
		// View 1 deliberately has neither vertices nor runs.
		DrawList visible = fixture.renderer.view(2);
		visible.draw_sprite(fixture.texture, RectangleI(0, 0, 2, 2),
			RectangleF(4.0f, 4.0f, 16.0f, 16.0f), Colour::red, 0.0f,
			Vector2F::zero, SpriteFlip::none, 0.0f);
		visible.set_filter(TextureFilter::linear);
		visible.draw_sprite(fixture.texture, RectangleI(0, 0, 2, 2),
			RectangleF(28.0f, 4.0f, 16.0f, 16.0f), Colour::lime, 0.0f,
			Vector2F::zero, SpriteFlip::none, 0.0f);
	}

	std::vector<unsigned char> expected_pixels()
	{
		std::vector<unsigned char> expected(buffer_size * buffer_size * 4, 0);
		for (int y = 0; y < buffer_size; y++)
		{
			for (int x = 0; x < buffer_size; x++)
			{
				const size_t offset = static_cast<size_t>(y * buffer_size + x) * 4;
				expected[offset + 3] = 255;
				if (y >= 4 && y < 20 && x >= 4 && x < 20)
				{
					expected[offset] = 255;
				}
				if (y >= 4 && y < 20 && x >= 28 && x < 44)
				{
					expected[offset + 1] = 255;
				}
			}
		}
		return expected;
	}
}

TEST_CASE("Vulkan submit preserves vertex bases across skipped, empty and multirun views")
{
	SubmitFixture fixture;
	fixture.renderer.begin_frame();
	record_views(fixture);
	fixture.renderer.submit();
	std::vector<unsigned char> pixels;
	fixture.renderer.read_back_buffer(pixels);
	CHECK(pixels == expected_pixels());
	fixture.renderer.end_frame();
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
		return previous == nullptr ? 1 : previous(operation, data, bytes, block_type, request, file, line);
	}

	// This test serializes its hook installations; competing instrumentation
	// must not replace the CRT hook concurrently. TLS limits only observations.
	// A prior hook must obey the CRT rule against recursive heap operations.
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

TEST_CASE("Vulkan warmed submit makes no normal-block Debug CRT allocation attempts")
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
	const std::vector<unsigned char> expected = expected_pixels();
	std::vector<unsigned char> pixels;
	// Present repeated identical geometry through both frame slots, retaining
	// view/run capacities, vertex/transform buffers and descriptor pools.
	for (int frame = 0; frame < 4; frame++)
	{
		fixture.renderer.begin_frame();
		record_views(fixture);
		fixture.renderer.submit();
		fixture.renderer.read_back_buffer(pixels);
		fixture.renderer.end_frame();
	}
	for (int frame = 0; frame < 3; frame++)
	{
		fixture.renderer.begin_frame();
		record_views(fixture);
		const _CRT_ALLOC_HOOK previous = _CrtGetAllocHook();
		AllocationSample sample;
		{
			AllocationScope scope(sample);
			fixture.renderer.submit();
		}
		INFO("measured frame ", frame, ", normal-block attempts ", sample.attempts,
			", requested bytes ", sample.bytes);
		REQUIRE_FALSE(sample.hook_conflict);
		REQUIRE(_CrtGetAllocHook() == previous);
		CHECK(sample.attempts == 0);
		fixture.renderer.read_back_buffer(pixels);
		CHECK(pixels == expected);
		fixture.renderer.end_frame();
	}
}
#endif
