#include <doctest/doctest.h>

#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/colour.h"
#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"
#include "engine/render/texture_format.h"
#include "tests/render/test_window.h"

#include <cstddef>
#include <memory>
#include <vector>

// Native presentation lifecycle, without a read-back draining every frame.
// Pixel tests normally read before presenting; these sequences instead leave
// synchronization to the renderer's frame reuse, resize and destructor paths.
// They do not establish how long a particular GPU remains busy. Run with
// D3D12 diagnostics and Vulkan synchronization validation to check the API
// ordering exercised here (docs/review/vulkan/README.md#2).
namespace
{
	using namespace labrador;

	constexpr int initial_size = 64;
	constexpr int resized_size = 32;
	constexpr int presented_frames = 6;

	TextureData white_texture()
	{
		TextureData texture;
		texture.width = 2;
		texture.height = 2;
		texture.format = TextureFormat::r8g8b8a8_unorm;
		texture.levels.push_back(texture_level(texture.format, 2, 2, 0));
		texture.pixels.resize(texture.levels[0].size, 255);
		return texture;
	}

	void present_frame(Renderer& renderer, TextureHandle texture, int frame)
	{
		renderer.begin_frame();
		renderer.set_view_count(2);
		for (int view = 0; view < 2; view++)
		{
			DrawList list = renderer.view(view);
			// Cross two full 2048-sprite vertex pages and a partial third in
			// each view. Reusing only an empty or single-page frame would leave
			// allocator/vertex-page lifetime paths unexercised.
			for (int sprite = 0; sprite < 4097; sprite++)
			{
				const float x = static_cast<float>((sprite + frame) % 8) * 2.0f;
				const float y = static_cast<float>((sprite / 8 + view) % 8) * 2.0f;
				list.draw_sprite(texture, RectangleI(0, 0, 2, 2),
					RectangleF(x, y, 2.0f, 2.0f),
					frame % 2 == 0 ? Colour::red : Colour::blue, 0.0f,
					Vector2F::zero, SpriteFlip::none, 0.0f);
			}
		}
		renderer.submit();
		renderer.end_frame();
	}
}

TEST_CASE("CONTRACT: consecutive presented frames survive resize without per-frame read-back")
{
	render_tests::TestWindow window(initial_size, initial_size);
	RenderResources resources;
	Renderer renderer;
	renderer.create_device(window.handle(), initial_size, initial_size, 2);
	renderer.set_resources(&resources);
	add_texture_asset(renderer, resources, "presentation_lifecycle", white_texture());
	const TextureHandle texture = resources.resolve_texture("presentation_lifecycle");

	for (int frame = 0; frame < presented_frames; frame++)
	{
		CAPTURE(frame);
		present_frame(renderer, texture, frame);
	}

	// A real window resize, followed by the shell's notification. No read
	// precedes it: the backend owns any wait its swapchain/resources require.
	REQUIRE(window.resize(resized_size, resized_size));
	CHECK(renderer.window_size_changed(resized_size, resized_size));
	CHECK(renderer.back_buffer_size() == Vector2F(
		static_cast<float>(resized_size), static_cast<float>(resized_size)));
	for (int frame = presented_frames; frame < presented_frames * 2; frame++)
	{
		CAPTURE(frame);
		present_frame(renderer, texture, frame);
	}

	// One final read checks the observable result after the presented sequence:
	// the new size, a clean clear, and a texture that survived the resize.
	// Its image is specified analytically, so there is no new golden to record.
	renderer.begin_frame();
	renderer.set_view_count(1);
	DrawList last = renderer.view(0);
	last.draw_sprite(texture, RectangleI(0, 0, 2, 2),
		RectangleF(4.0f, 4.0f, 8.0f, 8.0f), Colour::lime, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);
	renderer.submit();
	std::vector<unsigned char> pixels;
	renderer.read_back_buffer(pixels);
	std::vector<unsigned char> expected(resized_size * resized_size * 4, 0);
	for (int y = 0; y < resized_size; y++)
	{
		for (int x = 0; x < resized_size; x++)
		{
			const size_t offset = static_cast<size_t>(y * resized_size + x) * 4;
			expected[offset + 3] = 255;
			if (x >= 4 && x < 12 && y >= 4 && y < 12)
			{
				expected[offset + 1] = 255;
			}
		}
	}
	CHECK(pixels == expected);
}

TEST_CASE("CONTRACT: renderer teardown accepts presented work without an explicit read-back wait")
{
	render_tests::TestWindow window(initial_size, initial_size);
	// Resources outlive the renderer: its destructor owns the wait before
	// their textures can be released. There is no read-back anywhere in this
	// case, including after the final present and before renderer.reset().
	RenderResources resources;
	std::unique_ptr<Renderer> renderer = std::make_unique<Renderer>();
	renderer->create_device(window.handle(), initial_size, initial_size, 2);
	renderer->set_resources(&resources);
	add_texture_asset(*renderer, resources, "presentation_lifecycle", white_texture());
	const TextureHandle texture = resources.resolve_texture("presentation_lifecycle");
	for (int frame = 0; frame < presented_frames; frame++)
	{
		CAPTURE(frame);
		present_frame(*renderer, texture, frame);
	}
	CHECK_NOTHROW(renderer.reset());
	CHECK(resources.resolve_texture("presentation_lifecycle") == texture);
}
