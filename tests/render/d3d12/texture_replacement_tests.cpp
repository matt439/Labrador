#include <doctest/doctest.h>
#include "tests/render/allocation_scope.h"
#include "tests/render/device_fixture.h"

#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/colour.h"
#include "engine/render/d3d12/backend.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"
#include "engine/render/texture_format.h"

#include <cstddef>
#include <new>
#include <string>
#include <vector>

// The refusal is a Debug CRT allocation hook, so this regression exists only
// where allocation_scope.h does: a Debug build against the DLL CRT.
#if defined(_MSC_VER) && defined(_DEBUG) && defined(_DLL)

namespace
{
	using namespace labrador;
	using render_tests::AllocationSample;
	using render_tests::AllocationScope;

	constexpr int buffer_size = 64;

	TextureData solid_texture(unsigned char red, unsigned char green, unsigned char blue)
	{
		TextureData texture;
		texture.width = 2;
		texture.height = 2;
		texture.format = TextureFormat::r8g8b8a8_unorm;
		texture.levels.push_back(texture_level(texture.format, 2, 2, 0));
		texture.pixels.resize(texture.levels[0].size);
		for (size_t i = 0; i < texture.pixels.size(); i += 4)
		{
			texture.pixels[i + 0] = red;
			texture.pixels[i + 1] = green;
			texture.pixels[i + 2] = blue;
			texture.pixels[i + 3] = 255;
		}
		return texture;
	}

	std::vector<unsigned char> solid_frame(unsigned char red, unsigned char green,
		unsigned char blue)
	{
		std::vector<unsigned char> frame(buffer_size * buffer_size * 4);
		for (size_t i = 0; i < frame.size(); i += 4)
		{
			frame[i + 0] = red;
			frame[i + 1] = green;
			frame[i + 2] = blue;
			frame[i + 3] = 255;
		}
		return frame;
	}

	// Draws the texture over the whole target, so every pixel samples through
	// the descriptor its slot currently holds.
	std::vector<unsigned char> draw_texture(render_tests::DeviceFixture& fixture,
		TextureHandle texture)
	{
		fixture.renderer.begin_frame();
		fixture.renderer.set_view_count(1);
		{
			DrawList list = fixture.renderer.view(0);
			list.draw_sprite(texture, RectangleI(0, 0, 2, 2),
				RectangleF(0.0f, 0.0f, static_cast<float>(buffer_size),
					static_cast<float>(buffer_size)),
				Colour::white, 0.0f, Vector2F::zero, SpriteFlip::none, 0.0f);
		}
		fixture.renderer.submit();
		std::vector<unsigned char> pixels;
		fixture.renderer.read_back_buffer(pixels);
		fixture.renderer.end_frame();
		return pixels;
	}
}

TEST_CASE("D3D12 failed same-name replacement wrapper keeps the old texture and descriptor")
{
	render_tests::DeviceFixture fixture;
	const std::string name = "d3d12_replacement";
	RenderResources::Impl& table = *fixture.resources.impl();

	add_texture_asset(fixture.renderer, fixture.resources, name, solid_texture(255, 0, 0));
	const TextureHandle handle = fixture.resources.resolve_texture(name);
	const int slot = table.descriptor_slot(name);
	REQUIRE(slot >= 0);
	CHECK(draw_texture(fixture, handle) == solid_frame(255, 0, 0));

	// Locate the wrapper allocation in an identical successful same-name
	// reload. Registry replacement of an existing name allocates nothing, so
	// this one attempt is the whole of the throwing preparation/publication.
	AllocationSample located;
	located.watched_bytes = sizeof(D3d12Texture);
	{
		AllocationScope scope(located);
		add_texture_asset(fixture.renderer, fixture.resources, name, solid_texture(255, 0, 0));
	}
	REQUIRE_FALSE(located.hook_conflict);
	REQUIRE(located.watched_attempts == 1);
	REQUIRE(table.descriptor_slot(name) == slot);

	const D3d12Texture* original = table.textures.get(name);
	ID3D12Resource* const original_resource = original->resource();
	CHECK(draw_texture(fixture, handle) == solid_frame(255, 0, 0));

	// The same reload with different pixels, refused at the wrapper. The
	// replacement resource is fully uploaded by then, and unwinds with it.
	AllocationSample refused;
	refused.refused_attempt = located.watched_attempt;
	{
		AllocationScope scope(refused);
		CHECK_THROWS_AS(add_texture_asset(fixture.renderer, fixture.resources, name,
			solid_texture(0, 255, 0)), std::bad_alloc);
	}
	REQUIRE_FALSE(refused.hook_conflict);
	CHECK(refused.refused == 1);
	CHECK(refused.refused_bytes == sizeof(D3d12Texture));

	// The old object, its resource and its slot are still published, and the
	// shader-visible descriptor still names that resource: drawing reads red,
	// not the released green replacement.
	CHECK(table.textures.get(name) == original);
	CHECK(table.texture(handle) == original);
	CHECK(original->resource() == original_resource);
	CHECK(original->slot() == slot);
	CHECK(table.descriptor_slot(name) == slot);
	CHECK(draw_texture(fixture, handle) == solid_frame(255, 0, 0));

	// The next successful replacement reuses the same slot and is drawn.
	add_texture_asset(fixture.renderer, fixture.resources, name, solid_texture(0, 255, 0));
	CHECK(table.descriptor_slot(name) == slot);
	CHECK(table.texture(handle) != nullptr);
	CHECK(draw_texture(fixture, handle) == solid_frame(0, 255, 0));
}

#endif
