#include "tests/render/pixel_test_harness.h"

#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/colour.h"
#include "engine/render/renderer.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"
#include "engine/render/texture_format.h"

#include <doctest/doctest.h>

#include <stdexcept>
#include <string>
#include <tuple>

using namespace labrador;
using namespace render_tests::pixel;

namespace
{
	// One BC2 block, four by four, in quadrants: red top left and bottom
	// right, opaque black top right, and transparent bottom left.
	//
	// EVERY TEXEL IS AN ENDPOINT AND EVERY ALPHA IS 0 OR 15, which is what
	// makes it exact on every decoder: the rounding the harness takes out of
	// the font atlas lives in the two interpolated colours, and a block that
	// never indexes them has none. Red is 0xF800 and black 0x0000, so the
	// widening is exact too. And it is BC2 and no other format by its bytes:
	// read as BC3 the alpha half is two endpoints of 255 and the transparent
	// quadrant comes out opaque; read as BC1 the alpha half is taken for the
	// colour block.
	TextureData bc2_quadrant_texture()
	{
		TextureData texture;
		texture.width = 4;
		texture.height = 4;
		texture.format = TextureFormat::bc2_unorm;
		texture.levels.push_back(texture_level(texture.format, 4, 4, 0));

		texture.pixels = {
			// Explicit alpha, four bits a texel, two texels a byte, low nibble
			// first: rows 0 and 1 opaque, rows 2 and 3 transparent on the left
			// and opaque on the right.
			0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0xFF,
			// Endpoints, little-endian: red, then black.
			0x00, 0xF8, 0x00, 0x00,
			// Two bits a texel, low bits first, each row a byte: index 0 is
			// red and index 1 black. Rows 0 and 1 are red red black black,
			// rows 2 and 3 black black red red - the bottom left is black
			// under an alpha of zero, which premultiplied is transparent.
			0x50, 0x50, 0x05, 0x05,
		};
		REQUIRE(texture.pixels.size() == texture.levels[0].size);

		return texture;
	}
}

TEST_CASE("CONTRACT: a BC2 texture draws the texels its blocks encode")
{
	Harness harness;

	// EVERY FONT ATLAS IS BC2, so this is the format a backend that mapped one
	// enum to the wrong compressed format would lose all its text through -
	// and the text cases do not show it, because they draw a decoded copy
	// (the harness says why). This is what holds the device's own decoder to
	// pixels instead, on a block built so that every decoder must agree.
	add_texture_asset(harness.renderer(), harness.resources(), "bc2",
		bc2_quadrant_texture());
	const TextureHandle bc2 = harness.resources().resolve_texture("bc2");

	DrawList list = harness.begin();

	// White underneath, so that the transparent quadrant is a colour of its
	// own rather than the clear the opaque black one also looks like.
	list.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 32.0f, 32.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);

	// Point, so that every pixel asserted below is one texel and no filter
	// weight is consulted; each texel is four pixels wide.
	list.set_filter(TextureFilter::point);
	list.draw_sprite(bc2, RectangleI(0, 0, 4, 4),
		RectangleF(8.0f, 8.0f, 16.0f, 16.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);
	harness.end();

	CHECK(harness.at(10, 10) == red_pixel);
	CHECK(harness.at(21, 10) == black_pixel);
	CHECK(harness.at(10, 21) == white_pixel);
	CHECK(harness.at(21, 21) == red_pixel);

	// And the white around it, which the sprite did not reach.
	CHECK(harness.at(4, 4) == white_pixel);
	CHECK(harness.at(27, 27) == white_pixel);
}

TEST_CASE("CONTRACT: every format the seam names is uploaded, or refused by name")
{
	Harness harness;

	// TEST-GAP.md's B12, written as the one statement that is true on every
	// backend rather than as a table of which backend takes what - because
	// that table is exactly what the seam refuses to promise.
	// engine/render/texture_format.h names six formats and no backend is
	// obliged to take all six: D3D12 makes 4-bit-per-channel support optional,
	// GL has no packing with those channels in that order, and a mobile Vulkan
	// device may have no block compression at all. What every backend IS
	// obliged to do is say so - resource_factory.h promises a
	// std::runtime_error naming the texture and the format, "which is the
	// answer a backend that cannot upload block compression owes rather than a
	// blank texture".
	//
	// SO THE ASSERTION IS THE DISJUNCTION: for each of the six, either the load
	// succeeds and the name resolves, or it throws with the format in the
	// message. A backend that quietly substituted a blank texture, or threw an
	// eight-digit code, fails this on the format it cannot take - which is the
	// case nobody could reach before, because no file in either client is in
	// four-bit or in a format the local driver refuses.
	struct Named
	{
		TextureFormat format;
		const char* name;
	};

	const Named formats[] = {
		{ TextureFormat::r8g8b8a8_unorm, "r8g8b8a8_unorm" },
		{ TextureFormat::b8g8r8a8_unorm, "b8g8r8a8_unorm" },
		{ TextureFormat::b4g4r4a4_unorm, "b4g4r4a4_unorm" },
		{ TextureFormat::bc1_unorm, "bc1_unorm" },
		{ TextureFormat::bc2_unorm, "bc2_unorm" },
		{ TextureFormat::bc3_unorm, "bc3_unorm" },
	};

	for (const Named& entry : formats)
	{
		// Four by four, which is one block of every compressed format and four
		// rows of every uncompressed one, so texture_level answers a real size
		// for all six and the bytes below are a valid bit pattern in each.
		TextureData texture;
		texture.width = 4;
		texture.height = 4;
		texture.format = entry.format;
		texture.levels.push_back(texture_level(entry.format, 4, 4, 0));
		texture.pixels.assign(texture.levels[0].size, 0x7Fu);

		const std::string name = std::string("format_") + entry.name;

		try
		{
			add_texture_asset(harness.renderer(), harness.resources(), name,
				texture);

			// Uploaded. Then the name is in the table and resolves, because a
			// backend that returned quietly without adding one would pass the
			// throw half of this case for the wrong reason.
			CHECK_NOTHROW(std::ignore =
				harness.resources().resolve_texture(name));
		}
		catch (const std::runtime_error& error)
		{
			// Refused. Then the message names the format, which is what makes
			// the refusal actionable - a client learns which of its files to
			// re-export rather than that something went wrong.
			const std::string message = error.what();
			CHECK(message.find(entry.name) != std::string::npos);
			CHECK(message.find(name) != std::string::npos);
		}
	}
}

TEST_CASE("CONTRACT: re-loading a name reuses its slot, however many times")
{
	// "Re-adding a name reuses its slot" is registry.h's sentence and
	// render_resources.h repeats it, so a handle resolved before a re-load is
	// the same handle afterwards and draws the new bytes. What that has to mean
	// for a backend is that a re-load costs it nothing PER LOAD, and one of the
	// four was spending a fixed-size resource on every one: the D3D12
	// descriptor heap holds a fixed number of entries, its slot allocator is a
	// bump with no free list, and a re-load took a fresh slot and abandoned the
	// one the name already had. A client re-walking its manifest per level ran
	// that heap out with a few dozen textures live - and one manifest naming a
	// single asset as both a texture and a sprite sheet does it inside one
	// walk, because load_sprite_sheet loads the sheet's texture under the
	// sheet's own name.
	//
	// SO THE LOOP IS THE CASE, AND THE LAST DRAW IS THE ASSERTION. Three
	// hundred is comfortably past the smallest fixed capacity any backend in
	// this tree has, and a backend that leaks per load never reaches the draw:
	// the throw comes out of the load. The colour change on the final pass is
	// the other half - a slot that is reused and not rewritten would hand back
	// the first load's red.
	Harness harness;

	for (int load = 0; load < 299; load++)
	{
		add_texture_asset(harness.renderer(), harness.resources(), "reloaded",
			flat_texture(255, 0, 0));
	}
	add_texture_asset(harness.renderer(), harness.resources(), "reloaded",
		flat_texture(0, 255, 0));

	const TextureHandle reloaded =
		harness.resources().resolve_texture("reloaded");

	DrawList list = harness.begin();
	list.draw_sprite(reloaded, RectangleI(0, 0, 2, 2),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);
	harness.end();

	CHECK(harness.at(4, 4) == green_pixel);
}
