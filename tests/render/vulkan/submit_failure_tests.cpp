#include <doctest/doctest.h>
#include "tests/render/device_fixture.h"

#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/colour.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"
#include "engine/render/texture_format.h"

#include <vulkan/vulkan.h>
#include <windows.h>

#include <bit>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
	template<typename Function>
	Function loader_function(const char* name)
	{
		const HMODULE module = GetModuleHandleW(L"vulkan-1.dll");
		const FARPROC address = module == nullptr ? nullptr : GetProcAddress(module, name);
		if (address == nullptr)
		{
			throw std::runtime_error(std::string("Vulkan test export missing: ") + name);
		}
		static_assert(sizeof(Function) == sizeof(FARPROC));
		return std::bit_cast<Function>(address);
	}

	struct VulkanCalls
	{
		PFN_vkAllocateDescriptorSets allocate =
			loader_function<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets");
		PFN_vkCmdBeginRenderPass begin =
			loader_function<PFN_vkCmdBeginRenderPass>("vkCmdBeginRenderPass");
		PFN_vkCmdEndRenderPass end =
			loader_function<PFN_vkCmdEndRenderPass>("vkCmdEndRenderPass");
	};

	const VulkanCalls& real_calls()
	{
		static const VulkanCalls calls;
		return calls;
	}

	struct SubmitProbe
	{
		int fail_on = 0;
		int allocations = 0;
		int injected = 0;
		int begins = 0;
		int ends = 0;
		VkCommandBuffer open_pass = VK_NULL_HANDLE;
	};

	thread_local SubmitProbe* active_probe = nullptr;

	class ProbeScope
	{
	public:
		explicit ProbeScope(SubmitProbe& probe) : calls_(real_calls()), probe_(probe)
		{
			if (active_probe != nullptr)
			{
				throw std::logic_error("Vulkan submit probes cannot nest");
			}
			active_probe = &probe;
		}
		~ProbeScope()
		{
			// The failing baseline is measured before this cleanup. Do not let
			// its unclosed pass become invalid subsequent commands or teardown.
			if (this->probe_.open_pass != VK_NULL_HANDLE)
			{
				this->calls_.end(this->probe_.open_pass);
			}
			active_probe = nullptr;
		}
		ProbeScope(const ProbeScope&) = delete;
		ProbeScope& operator=(const ProbeScope&) = delete;

	private:
		const VulkanCalls& calls_;
		SubmitProbe& probe_;
	};

	labrador::TextureData white_texture()
	{
		labrador::TextureData texture;
		texture.width = 2;
		texture.height = 2;
		texture.levels.push_back(labrador::texture_level(texture.format, 2, 2, 0));
		texture.pixels.resize(texture.levels[0].size, 255);
		return texture;
	}

	void draw_two_runs(labrador::Renderer& renderer, labrador::TextureHandle texture)
	{
		using namespace labrador;
		using namespace labrador;
		renderer.set_view_count(1);
		DrawList list = renderer.view(0);
		list.draw_sprite(texture, RectangleI(0, 0, 2, 2),
			RectangleF(4.0f, 4.0f, 16.0f, 16.0f), Colour::red, 0.0f,
			Vector2F::zero, SpriteFlip::none, 0.0f);
		list.set_filter(TextureFilter::linear);
		list.draw_sprite(texture, RectangleI(0, 0, 2, 2),
			RectangleF(28.0f, 4.0f, 16.0f, 16.0f), Colour::lime, 0.0f,
			Vector2F::zero, SpriteFlip::none, 0.0f);
	}
}

// These executable-local symbols forward to the DLL exports, not themselves.
// No engine injection API or actual allocation exhaustion is involved.
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkAllocateDescriptorSets(
	VkDevice device, const VkDescriptorSetAllocateInfo* allocation, VkDescriptorSet* sets)
{
	if (active_probe != nullptr)
	{
		++active_probe->allocations;
		if (active_probe->allocations == active_probe->fail_on)
		{
			++active_probe->injected;
			return VK_ERROR_OUT_OF_DEVICE_MEMORY;
		}
	}
	return real_calls().allocate(device, allocation, sets);
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdBeginRenderPass(
	VkCommandBuffer commands, const VkRenderPassBeginInfo* pass, VkSubpassContents contents)
{
	real_calls().begin(commands, pass, contents);
	if (active_probe != nullptr)
	{
		++active_probe->begins;
		active_probe->open_pass = commands;
	}
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdEndRenderPass(VkCommandBuffer commands)
{
	real_calls().end(commands);
	if (active_probe != nullptr)
	{
		++active_probe->ends;
		active_probe->open_pass = VK_NULL_HANDLE;
	}
}

TEST_CASE("Vulkan submit closes its render pass when descriptor allocation throws")
{
	using namespace labrador;
	int fail_on = 1;
	SUBCASE("first run") {}
	SUBCASE("second run after an earlier draw") { fail_on = 2; }
	CAPTURE(fail_on);
	render_tests::DeviceFixture fixture;
	add_texture_asset(fixture.renderer, fixture.resources, "submit_failure_white", white_texture());
	const TextureHandle texture = fixture.resources.resolve_texture("submit_failure_white");
	fixture.renderer.begin_frame();
	draw_two_runs(fixture.renderer, texture);
	{
		SubmitProbe probe;
		probe.fail_on = fail_on;
		ProbeScope scope(probe);
		CHECK_THROWS_WITH_AS(fixture.renderer.submit(),
			"A run's descriptor set could not be allocated (VK_ERROR_OUT_OF_DEVICE_MEMORY).",
			std::runtime_error);
		CHECK(probe.allocations == fail_on);
		CHECK(probe.injected == 1);
		CHECK(probe.begins == 1);
		CHECK(probe.ends == probe.begins);
		CHECK(probe.open_pass == VK_NULL_HANDLE);
		if (probe.ends != probe.begins || probe.open_pass != VK_NULL_HANDLE)
		{
			return;
		}
	}

	// Complete the failed frame without asserting that its draws were atomic.
	std::vector<unsigned char> pixels;
	fixture.renderer.read_back_buffer(pixels);
	fixture.renderer.end_frame();
	fixture.renderer.begin_frame();
	draw_two_runs(fixture.renderer, texture);
	fixture.renderer.submit();
	fixture.renderer.read_back_buffer(pixels);
	std::vector<unsigned char> expected(64 * 64 * 4, 0);
	for (int y = 0; y < 64; y++)
	{
		for (int x = 0; x < 64; x++)
		{
			const size_t offset = static_cast<size_t>(y * 64 + x) * 4;
			expected[offset + 3] = 255;
			if (y >= 4 && y < 20 && x >= 4 && x < 20) expected[offset] = 255;
			if (y >= 4 && y < 20 && x >= 28 && x < 44) expected[offset + 1] = 255;
		}
	}
	CHECK(pixels == expected);
	fixture.renderer.end_frame();
}
