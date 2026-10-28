#include "tests/render/pixel_test_harness.h"

#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/camera.h"
#include "engine/render/colour.h"
#include "engine/render/renderer.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"
#include "engine/render/texture_format.h"

#include <doctest/doctest.h>

#include <cmath>

using namespace labrador;
using namespace render_tests::pixel;

namespace
{
	// A texture that carries a mip chain, which no file in either client does.
	//
	// Level zero is sixteen pixels of red and level one is eight of blue, so
	// which level a minified draw sampled is the colour that comes back rather
	// than an inference from one. The seam's texture handover is a plain
	// aggregate (engine/render/texture_data.h), so this needs no file, no
	// tool and no new bytes in the repository - which is the only reason the
	// term below is testable at all, because there is nothing to load that
	// would exercise it.
	TextureData two_level_texture()
	{
		TextureData texture;
		texture.width = 16;
		texture.height = 16;
		texture.format = TextureFormat::r8g8b8a8_unorm;

		const TextureLevel coarse_offset =
			texture_level(texture.format, 16, 16, 0);
		texture.levels.push_back(coarse_offset);
		texture.levels.push_back(
			texture_level(texture.format, 8, 8, coarse_offset.size));

		const TextureLevel& fine = texture.levels[0];
		const TextureLevel& coarse = texture.levels[1];
		texture.pixels.resize(fine.size + coarse.size);

		for (size_t byte = fine.offset; byte < fine.offset + fine.size;
			byte += 4)
		{
			texture.pixels[byte] = 255;
			texture.pixels[byte + 3] = 255;
		}
		for (size_t byte = coarse.offset; byte < coarse.offset + coarse.size;
			byte += 4)
		{
			texture.pixels[byte + 2] = 255;
			texture.pixels[byte + 3] = 255;
		}

		return texture;
	}

	// Two by two, left column red and right column green, one level.
	//
	// FOR THE ONE TERM THAT NEEDS A TEXTURE WITH AN EDGE IN IT. Every other
	// hand-built texture here is flat, which is what makes an assertion about
	// a colour exact - and flat is exactly what a filter cannot be seen
	// through, because point and linear differ only in what they do BETWEEN
	// texels. Two columns put the boundary in the middle of a magnified draw,
	// where point answers one of the two colours and linear answers a mix of
	// them that neither texel contains.
	TextureData two_colour_texture()
	{
		TextureData texture;
		texture.width = 2;
		texture.height = 2;
		texture.format = TextureFormat::r8g8b8a8_unorm;
		texture.levels.push_back(texture_level(texture.format, 2, 2, 0));
		texture.pixels.resize(texture.levels[0].size);

		for (size_t pixel = 0; pixel < 4; pixel++)
		{
			const size_t byte = pixel * 4;
			const bool right = (pixel % 2) == 1;
			texture.pixels[byte] = right ? 0 : 255;
			texture.pixels[byte + 1] = right ? 255 : 0;
			texture.pixels[byte + 2] = 0;
			texture.pixels[byte + 3] = 255;
		}

		return texture;
	}
}

TEST_CASE("CONTRACT: the destination is in pixels, y runs down, origin top left")
{
	Harness harness;

	DrawList list = harness.begin();
	list.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 4.0f, 4.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);
	harness.end();

	// A 4x4 sprite at the origin covers the top-left corner and nothing else.
	CHECK(harness.at(0, 0) == white_pixel);
	CHECK(harness.at(3, 3) == white_pixel);
	CHECK(harness.at(4, 4) == black_pixel);

	// If y ran up, this is where it would be instead.
	CHECK(harness.at(2, buffer_side - 2) == black_pixel);
}

TEST_CASE("CONTRACT: texel (0,0) draws at the top left of the destination")
{
	Harness harness;

	DrawList list = harness.begin();
	list.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);
	harness.end();

	// The single assertion a y-inverted texture coordinate fails, and the
	// reason the test texture has four different corners.
	CHECK(harness.at(2, 2) == red_pixel);
	CHECK(harness.at(6, 2) == green_pixel);
	CHECK(harness.at(2, 6) == blue_pixel);
	CHECK(harness.at(6, 6) == white_pixel);
}

TEST_CASE("CONTRACT: flip mirrors the texture, not the destination rectangle")
{
	Harness harness;

	SUBCASE("horizontal swaps left and right, leaving top and bottom")
	{
		DrawList list = harness.begin();
		list.draw_sprite(harness.quad, Harness::whole(),
			RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
			Vector2F::zero, SpriteFlip::horizontal, 0.0f);
		harness.end();

		CHECK(harness.at(2, 2) == green_pixel);
		CHECK(harness.at(6, 2) == red_pixel);
		CHECK(harness.at(2, 6) == white_pixel);
		CHECK(harness.at(6, 6) == blue_pixel);
	}

	SUBCASE("vertical swaps top and bottom, leaving left and right")
	{
		DrawList list = harness.begin();
		list.draw_sprite(harness.quad, Harness::whole(),
			RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
			Vector2F::zero, SpriteFlip::vertical, 0.0f);
		harness.end();

		CHECK(harness.at(2, 2) == blue_pixel);
		CHECK(harness.at(6, 2) == white_pixel);
		CHECK(harness.at(2, 6) == red_pixel);
		CHECK(harness.at(6, 6) == green_pixel);
	}

	SUBCASE("both is the same as one after the other")
	{
		DrawList list = harness.begin();
		list.draw_sprite(harness.quad, Harness::whole(),
			RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
			Vector2F::zero, SpriteFlip::both, 0.0f);
		harness.end();

		CHECK(harness.at(2, 2) == white_pixel);
		CHECK(harness.at(6, 2) == blue_pixel);
		CHECK(harness.at(2, 6) == green_pixel);
		CHECK(harness.at(6, 6) == red_pixel);
	}
}

TEST_CASE("CONTRACT: a positive rotation turns the sprite clockwise on screen")
{
	Harness harness;

	// Half of pi. Not exactly, because nothing is: std::cos of it is about
	// 6e-17 rather than 0, so the far corners land a hair off the whole pixels
	// named below and the fill rule decides which side of each they fall. That
	// is the point of the case as much as the direction is - see below.
	const float quarter_turn = 1.5707963f;

	// A bar four pixels tall and sixteen wide, pivoting on its own top-left
	// corner, because a square rotated about its centre looks the same both
	// ways round and every other case in this file passes 0.0f here.
	DrawList list = harness.begin();
	list.draw_sprite(harness.quad, Harness::red_texel(),
		RectangleF(32.0f, 32.0f, 16.0f, 4.0f), Colour::white, quarter_turn,
		Vector2F::zero, SpriteFlip::none, 0.0f);
	harness.end();

	// x' = x cos - y sin, y' = x sin + y cos, with y running DOWN the screen -
	// so +x turns towards +y, which is right turning to down. The bar was
	// pointing right and now points down, four pixels wide, hanging to the
	// left of the pivot because its own height went that way.
	CHECK(harness.at(30, 40) == red_pixel);
	CHECK(harness.at(30, 46) == red_pixel);

	// Where it was before the turn, and where an anticlockwise convention
	// would have put it. Either being red is a backend that turned the other
	// way.
	CHECK(harness.at(40, 34) == black_pixel);
	CHECK(harness.at(30, 24) == black_pixel);

	// AND THIS IS THE ONLY FRAME IN THE FILE WITH EDGES THAT ARE NOT AXIS
	// ALIGNED, which is worth more than the four assertions above.
	// build_sprite_quad truncates every edge to a whole pixel, so an
	// unrotated sprite never puts one across a pixel centre and the fill rule
	// is never consulted; a turned one puts all four there at once. D3D11
	// mandates the top-left rule exactly and GL 3.3 asks only that a shared
	// edge be neither dropped nor drawn twice, leaving the tie-break to the
	// implementation, and Vulkan specifies it again - so this is a term the
	// three specifications do NOT agree on, and the golden image is what says
	// whether the four devices do. They do, and the check has to be byte
	// identity rather than a passing run: allowed_channel_drift is 8, which is
	// enough to hide a one-row tie-break disagreement, so what settles it is
	// regenerating the set on each backend and finding the file unchanged.
	// All four write this frame byte for byte on this machine's adapter. What
	// that does not settle is another driver, which is why the tie-break stays
	// written down here rather than assumed.
	CHECK(harness.ink_bounds() == RectangleI(28, 32, 4, 16));
}

TEST_CASE("CONTRACT: the source rectangle selects, and is in texels")
{
	Harness harness;

	DrawList list = harness.begin();
	list.draw_sprite(harness.quad, Harness::red_texel(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);
	harness.end();

	// One texel stretched over the whole destination, so every sample is it.
	CHECK(harness.at(1, 1) == red_pixel);
	CHECK(harness.at(6, 6) == red_pixel);
}

TEST_CASE("CONTRACT: a source rectangle outside the texture clamps to the edge texel")
{
	Harness harness;

	// TEST-GAP.md's B11. Four backends hard-code CLAMP in four vocabularies -
	// D3D11_TEXTURE_ADDRESS_CLAMP, D3D12's the same, GL_CLAMP_TO_EDGE,
	// VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE - and nothing held them together.
	// The alternative a backend could reach for is WRAP, which is a different
	// picture rather than a worse one: an atlas sampled past its edge would
	// show the far side of itself, which is the artefact CLAMP exists to stop
	// and which the sheets this engine draws are full of opportunities for.
	add_texture_asset(harness.renderer(), harness.resources(), "edge",
		two_colour_texture());
	const TextureHandle edge = harness.resources().resolve_texture("edge");

	DrawList list = harness.begin();

	// Four texels wide out of a texture two texels wide, so the right half of
	// the destination samples u in [1, 2) - off the end of the texture
	// entirely. Under CLAMP every sample there is the right-hand column;
	// under WRAP the first column would come round again.
	list.draw_sprite(edge, RectangleI(0, 0, 4, 2),
		RectangleF(0.0f, 0.0f, 32.0f, 16.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);
	harness.end();

	// The texture itself, in the left half: red then green.
	CHECK(harness.at(3, 8) == red_pixel);
	CHECK(harness.at(12, 8) == green_pixel);

	// And the overhang, which is the case. Green all the way to the right
	// edge; a wrapping sampler answers red_pixel at x=17 and x=28.
	CHECK(harness.at(17, 8) == green_pixel);
	CHECK(harness.at(28, 8) == green_pixel);
	CHECK(harness.at(31, 8) == green_pixel);

	// A negative source origin is the same question at the other end.
	DrawList second = harness.begin();
	second.draw_sprite(edge, RectangleI(-2, 0, 4, 2),
		RectangleF(0.0f, 0.0f, 32.0f, 16.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);
	harness.end();

	CHECK(harness.at(3, 8) == red_pixel);
	CHECK(harness.at(12, 8) == red_pixel);
	CHECK(harness.at(20, 8) == red_pixel);
	CHECK(harness.at(28, 8) == green_pixel);
}

TEST_CASE("CONTRACT: a minified draw samples level zero, not the chain")
{
	Harness harness;

	// Sixteen source pixels into four, so the level a mip-sampling backend
	// would pick is level one and not a near miss: log2(16/4) is 2, and the
	// chain stops at 1, so a sampler walking it lands on blue with margin.
	add_texture_asset(harness.renderer(), harness.resources(), "two_level",
		two_level_texture());
	const TextureHandle chained =
		harness.resources().resolve_texture("two_level");

	// BOTH FILTERS, because the term belongs to both samplers and because
	// TextureFilter::linear reached no device anywhere in this repository
	// until this case. Point and linear differ in which texels of a level
	// they combine; neither chooses a level.
	SUBCASE("under the point filter")
	{
		DrawList list = harness.begin();
		list.set_filter(TextureFilter::point);
		list.draw_sprite(chained, RectangleI(0, 0, 16, 16),
			RectangleF(8.0f, 8.0f, 4.0f, 4.0f), Colour::white, 0.0f,
			Vector2F::zero, SpriteFlip::none, 0.0f);
		harness.end();

		CHECK(harness.at(9, 9) == red_pixel);
		CHECK(harness.at(10, 10) == red_pixel);
	}

	SUBCASE("and under the linear filter")
	{
		DrawList list = harness.begin();
		list.set_filter(TextureFilter::linear);
		list.draw_sprite(chained, RectangleI(0, 0, 16, 16),
			RectangleF(8.0f, 8.0f, 4.0f, 4.0f), Colour::white, 0.0f,
			Vector2F::zero, SpriteFlip::none, 0.0f);
		harness.end();

		// Level zero is one flat colour, so linear filtering has nothing to
		// blend towards and the answer is exact rather than approximately red.
		// That is the whole reason the two levels are flat.
		CHECK(harness.at(9, 9) == red_pixel);
		CHECK(harness.at(10, 10) == red_pixel);
	}
}

TEST_CASE("CONTRACT: the tint multiplies the texel")
{
	Harness harness;

	DrawList list = harness.begin();
	list.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour(1.0f, 0.0f, 0.0f, 1.0f),
		0.0f, Vector2F::zero, SpriteFlip::none, 0.0f);
	harness.end();

	// White texel, red tint. A tint that replaced rather than multiplied would
	// give the same answer here, which is why the second draw below puts a
	// green tint on a red texel.
	CHECK(harness.at(4, 4) == red_pixel);

	DrawList second = harness.begin();
	second.draw_sprite(harness.quad, Harness::red_texel(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour(0.0f, 1.0f, 0.0f, 1.0f),
		0.0f, Vector2F::zero, SpriteFlip::none, 0.0f);
	harness.end();

	// Red texel times green tint is black. Replacement would give green.
	CHECK(harness.at(4, 4) == black_pixel);
}

TEST_CASE("CONTRACT: the blend equation is premultiplied alpha")
{
	Harness harness;

	DrawList list = harness.begin();
	list.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour(1.0f, 1.0f, 1.0f, 0.5f),
		0.0f, Vector2F::zero, SpriteFlip::none, 0.0f);
	harness.end();

	// THE TERM MOST LIKELY TO BE GOT WRONG, and the one a screenshot will not
	// settle because both answers look plausible.
	//
	// The source factor is one, not SRC_ALPHA. A white texel under a half-alpha
	// tint is (1,1,1,0.5) going in, so the result over black is 1*(1,1,1) +
	// (1-0.5)*(0,0,0) = white. Straight alpha would give mid grey. The engine
	// therefore expects textures whose colour is already multiplied by their
	// alpha, and a backend that picks SRC_ALPHA here is wrong even though every
	// opaque sprite in both samples would still look exactly right.
	CHECK(harness.at(4, 4) == white_pixel);
}

TEST_CASE("CONTRACT: a tint with zero alpha adds its colour")
{
	Harness harness;

	DrawList list = harness.begin();
	list.draw_sprite(harness.quad, Harness::red_texel(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour(1.0f, 1.0f, 1.0f, 1.0f),
		0.0f, Vector2F::zero, SpriteFlip::none, 0.0f);
	list.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour(0.0f, 1.0f, 0.0f, 0.0f),
		0.0f, Vector2F::zero, SpriteFlip::none, 0.0f);
	harness.end();

	// ADDITIVE BLENDING IS THIS, not a mode. Under the premultiplied equation
	// a source with zero alpha keeps all of the destination and adds all of
	// itself: 1*(0,1,0) + (1-0)*(1,0,0) is yellow. A glow or a spark drawn
	// with a zero-alpha tint brightens what is under it, and the guide to
	// sprites promises it. Straight alpha would leave the red untouched.
	CHECK(harness.at(4, 4) == Pixel{ 255, 255, 0, 255 });
	CHECK(harness.at(9, 9) == black_pixel);
}

TEST_CASE("CONTRACT: a fractional destination truncates, it does not round")
{
	Harness harness;

	DrawList list = harness.begin();
	list.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(10.9f, 0.0f, 8.0f, 4.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);
	harness.end();

	// Left edge 10.9 becomes 10. Rounding would put it at 11 and leave this
	// pixel black.
	CHECK(harness.at(9, 2) == black_pixel);
	CHECK(harness.at(10, 2) == white_pixel);

	// Right edge 18.9 becomes 18, and is exclusive.
	CHECK(harness.at(17, 2) == white_pixel);
	CHECK(harness.at(18, 2) == black_pixel);
}

TEST_CASE("CONTRACT: origin is in unscaled source texels")
{
	Harness harness;

	DrawList list = harness.begin();
	list.draw_sprite(harness.quad, Harness::whole(),
		RectangleF(8.0f, 8.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F(2.0f, 2.0f), SpriteFlip::none, 0.0f);
	harness.end();

	// An 8x8 destination over a 2x2 source is a scale of four, so an origin of
	// two source texels shifts the sprite eight destination pixels up and left
	// - putting its top-left corner exactly at (0,0).
	//
	// If origin were measured in destination pixels the shift would be two, the
	// sprite would sit at (6,6), and (1,1) would be black.
	CHECK(harness.at(1, 1) == red_pixel);
	CHECK(harness.at(6, 1) == green_pixel);
	CHECK(harness.at(1, 6) == blue_pixel);
	CHECK(harness.at(8, 8) == black_pixel);
}

TEST_CASE("CONTRACT: layer_depth does not order draws, call order does")
{
	Harness harness;

	DrawList list = harness.begin();

	// Red first at the front, green second at the back. Depth sorting would
	// leave red on top; the batch does not sort, so the later call wins.
	list.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour(1.0f, 0.0f, 0.0f, 1.0f),
		0.0f, Vector2F::zero, SpriteFlip::none, 0.0f);
	list.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour(0.0f, 1.0f, 0.0f, 1.0f),
		0.0f, Vector2F::zero, SpriteFlip::none, 1.0f);

	harness.end();

	// A backend that honoured layer_depth would answer red_pixel here, and that is a
	// difference in behaviour rather than in rounding - so if depth is ever
	// made to mean something, this is the test to change deliberately rather
	// than the one that starts failing.
	CHECK(harness.at(4, 4) == green_pixel);
}

TEST_CASE("CONTRACT: a batch longer than one vertex buffer keeps its order")
{
	Harness harness;

	// TEST-GAP.md's B7. Every rasterising backend caps a run at 2048 sprites -
	// 16-bit indices cap it at 16384 and 2048 of them is a 256KB page, which
	// all four say in their own words - and what happens at 2049 is a
	// different mechanism in each: D3D11 maps its vertex buffer with DISCARD
	// and starts again, D3D12 takes a fresh page from its per-frame ring, GL
	// and Vulkan close the run and open another. Until this case nothing
	// crossed that boundary anywhere, on any backend, in any configuration.
	//
	// WHAT IS ASSERTED IS ORDER AND COMPLETENESS, not a picture. Every sprite
	// but the last is the same colour in the same place, so the frame says
	// nothing about how many arrived; the last one is drawn somewhere else and
	// in another colour, so a wrap that dropped the tail, replayed a page
	// twice, or reordered the two pages is visible.
	const int sprites = 2050;

	DrawList list = harness.begin();
	for (int i = 0; i < sprites - 1; i++)
	{
		list.draw_sprite(harness.quad, Harness::red_texel(),
			RectangleF(0.0f, 0.0f, 16.0f, 16.0f), Colour::white, 0.0f,
			Vector2F::zero, SpriteFlip::none, 0.0f);
	}

	// The 2050th, on the far side of the wrap and on top of the pile, in a
	// colour nothing before it used.
	list.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(8.0f, 8.0f, 16.0f, 16.0f), Colour(0.0f, 1.0f, 0.0f, 1.0f),
		0.0f, Vector2F::zero, SpriteFlip::none, 0.0f);
	harness.end();

	// The pile is there.
	CHECK(harness.at(2, 2) == red_pixel);

	// The last sprite is on top of it, which is call order surviving a page
	// boundary - the thing a backend that flushed pages out of order would
	// get wrong.
	CHECK(harness.at(12, 12) == green_pixel);

	// And it is where it was asked for rather than where the first page was:
	// a wrap that reset the vertex offset without telling the draw call would
	// put this quad at the pile's position instead.
	CHECK(harness.at(20, 20) == green_pixel);
	CHECK(harness.at(2, 20) == black_pixel);
}

TEST_CASE("CONTRACT: a texture change keeps call order, it does not group runs")
{
	Harness harness;

	// TWO OF THE THREE TERMS A DRAW CALL IS KEYED ON RASTERISE HERE AND
	// NOWHERE ELSE - the five-backend sweep's finding 6 and TEST-GAP's B8 by
	// another route. A run breaks on a viewport change, a texture change or a
	// filter change; without these cases only the viewport one reaches a device
	// from this file, and the stamps are asserted on the null backend alone,
	// where nothing rasterises - so what a change of texture does to a frame is
	// checked everywhere except on the four backends that draw.
	add_texture_asset(harness.renderer(), harness.resources(), "run_red",
		flat_texture(255, 0, 0));
	add_texture_asset(harness.renderer(), harness.resources(), "run_green",
		flat_texture(0, 255, 0));
	const TextureHandle red = harness.resources().resolve_texture("run_red");
	const TextureHandle green =
		harness.resources().resolve_texture("run_green");

	DrawList list = harness.begin();

	// Three calls, three runs, and each one overlaps the one before it. A
	// backend that grouped by texture to save a flush would draw both red
	// calls and then the green one, which is a different picture rather than a
	// faster one: the last overlap would come out green.
	list.draw_sprite(red, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 16.0f, 16.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);
	list.draw_sprite(green, Harness::white_texel(),
		RectangleF(8.0f, 8.0f, 16.0f, 16.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);
	list.draw_sprite(red, Harness::white_texel(),
		RectangleF(16.0f, 16.0f, 16.0f, 16.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);

	harness.end();

	// Where only one call reached.
	CHECK(harness.at(4, 4) == red_pixel);
	CHECK(harness.at(28, 28) == red_pixel);

	// Where the second covered the first, and the third covered the second.
	// The second of these is the assertion: grouping answers green_pixel here.
	CHECK(harness.at(10, 10) == green_pixel);
	CHECK(harness.at(20, 20) == red_pixel);
}

TEST_CASE("CONTRACT: a filter change mid-list applies to what follows it")
{
	Harness harness;

	// The other half of finding 6. renderer.h says changing the filter
	// mid-list is legal and costs a flush; until this case no submitted frame
	// changed it at all, so "costs a flush" was a claim about a run stamp and
	// not about a picture.
	add_texture_asset(harness.renderer(), harness.resources(), "two_colour",
		two_colour_texture());
	const TextureHandle edge =
		harness.resources().resolve_texture("two_colour");

	DrawList list = harness.begin();

	// Sixteen pixels wide from two texels, so each texel is eight pixels and
	// the boundary between them is the middle of the sprite.
	list.set_filter(TextureFilter::point);
	list.draw_sprite(edge, RectangleI(0, 0, 2, 2),
		RectangleF(0.0f, 0.0f, 16.0f, 16.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);

	list.set_filter(TextureFilter::linear);
	list.draw_sprite(edge, RectangleI(0, 0, 2, 2),
		RectangleF(24.0f, 0.0f, 16.0f, 16.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);

	harness.end();

	// Point: a hard edge in the middle, because every sample is a texel.
	CHECK(harness.at(7, 8) == red_pixel);
	CHECK(harness.at(8, 8) == green_pixel);

	// Linear: the same two texels, mixed where the sample falls between them.
	// Asserted as a relationship rather than as a number, for the reason the
	// text cases are - what a driver rounds a weight to is not this engine's
	// decision, and allowed_channel_drift is what holds the four backends to
	// each other on it (tests/render/golden_image.cpp).
	const Pixel blended = harness.at(31, 8);
	CHECK(blended.r > 0);
	CHECK(blended.g > 0);
	CHECK(blended.b == 0);
	CHECK(blended.a == 255);

	// And still exactly the texel where the sample is outside both centres,
	// which is the CLAMP address mode answering rather than the filter.
	CHECK(harness.at(25, 8) == red_pixel);
	CHECK(harness.at(38, 8) == green_pixel);

	// THE FILTER IS PER RUN AND THE RUN IS WHAT THE CHANGE BREAKS, which is
	// what the first sprite still being hard-edged says: a backend that
	// applied the last filter to the whole frame - one sampler bound once, the
	// mistake the seam's per-run stamp exists to prevent - would have blurred
	// this boundary too.
	CHECK(harness.at(6, 8) == red_pixel);
	CHECK(harness.at(9, 8) == green_pixel);
}

TEST_CASE("CONTRACT: the camera maps the destination, and changes mid list")
{
	Harness harness;

	DrawList list = harness.begin();

	// view = (world - translation) * scale, so this puts world (4,4) at the
	// top-left of the view.
	list.set_camera(Camera(4.0f, 4.0f, 1.0f));
	list.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(4.0f, 4.0f, 4.0f, 4.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);

	// Back to screen space mid-list, which is what a HUD over a world does and
	// what set_camera being per range rather than per view exists for.
	list.set_camera(Camera::default_camera);
	list.draw_sprite(harness.quad, Harness::red_texel(),
		RectangleF(32.0f, 32.0f, 4.0f, 4.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);

	harness.end();

	// The camera moved the world sprite to the origin.
	CHECK(harness.at(1, 1) == white_pixel);
	CHECK(harness.at(5, 5) == black_pixel);

	// The screen-space sprite ignored it.
	CHECK(harness.at(33, 33) == red_pixel);
}

TEST_CASE("CONTRACT: the destination factor is INV_SRC_ALPHA, not ZERO")
{
	Harness harness;

	DrawList list = harness.begin();

	// An opaque white ground first. THE CASE ABOVE CANNOT DO THIS. It composites
	// its one sprite over the cleared frame, which is black - and black times
	// any destination factor is black, so zero and INV_SRC_ALPHA give the same
	// answer there and the RGB destination factor is undistinguished by it. The
	// alpha channel is pinned by that case; the colour channels were not pinned
	// by anything.
	list.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 16.0f, 16.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);

	// Then a half-alpha black scrim over it. Premultiplied, which for black
	// means the colour channels are zero and stay zero however the alpha moves
	// - so the source contributes nothing at all and every channel of the
	// result comes from the destination through the factor under test. A tint
	// of (1,1,1,0.5) would NOT do: it is not premultiplied, and the source term
	// alone saturates the result to white.
	list.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 16.0f, 16.0f), Colour(0.0f, 0.0f, 0.0f, 0.5f),
		0.0f, Vector2F::zero, SpriteFlip::none, 0.0f);

	harness.end();

	const Pixel scrimmed = harness.at(8, 8);

	// dst * (1 - 0.5) = mid grey. ZERO gives black and ONE gives white, so a
	// range that excludes both settles the factor without pinning a rounding
	// mode - 127 and 128 are both defensible for 0.5 * 255 and a driver is
	// entitled to either.
	CHECK(scrimmed.r > 100);
	CHECK(scrimmed.r < 155);
	CHECK(scrimmed.g == scrimmed.r);
	CHECK(scrimmed.b == scrimmed.r);

	// And the alpha is the term the case above already pins, restated here
	// because a factor change that moved both would otherwise pass one test
	// and fail neither.
	CHECK(scrimmed.a == 255);

	// A scrim is what samples/linesweeper draws over the board when the game
	// is paused, so this is live behaviour and not a hypothetical.
}

TEST_CASE("CONTRACT: a tint with more colour than alpha saturates, it is not clamped to it")
{
	Harness harness;

	// THE ONE COLOUR A PREMULTIPLIED BLEND CANNOT DESCRIBE, and Colour lets a
	// caller write it: the channels are clamped independently
	// (engine/render/colour.h), so (1, 1, 1, 0.5) is a legal tint whose colour
	// is brighter than its own alpha. Nothing in this engine premultiplies for
	// a caller and nothing rejects it, so what a backend does with it is a
	// term of the seam whether anybody meant it to be or not - and the four
	// rasterisers each hand it to a different blend unit.
	//
	// docs/review/backend-equivalence/ raised it twice as an open question and
	// this is the answer: the blend is src + dst * (1 - src.a), the source term
	// is 1.0 before the destination is even consulted, and the write to an
	// 8-bit UNORM target is where the clamp happens - so the answer is white,
	// on every backend, and no backend clamps rgb down to alpha on the way in.
	DrawList list = harness.begin();
	list.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 16.0f, 16.0f), Colour(1.0f, 1.0f, 1.0f, 0.5f),
		0.0f, Vector2F::zero, SpriteFlip::none, 0.0f);

	// And over an opaque ground, where a backend that clamped the source to
	// its alpha would leave the destination showing through at half strength.
	list.draw_sprite(harness.quad, Harness::red_texel(),
		RectangleF(32.0f, 0.0f, 16.0f, 16.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);
	list.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(32.0f, 0.0f, 16.0f, 16.0f), Colour(0.0f, 1.0f, 0.0f, 0.5f),
		0.0f, Vector2F::zero, SpriteFlip::none, 0.0f);
	harness.end();

	// Over black: the source term alone is 1.0 in every colour channel.
	const Pixel over_black = harness.at(8, 8);
	CHECK(over_black.r == 255);
	CHECK(over_black.g == 255);
	CHECK(over_black.b == 255);

	// The alpha channel is opaque as well, and for the destination's reason
	// rather than the source's: begin_frame clears to opaque black, so the
	// blend is 0.5 + 1.0 * (1 - 0.5) and lands exactly on 1. The case above
	// pins the same term from the other side, where the source alpha is what
	// the destination is asked through.
	CHECK(over_black.a == 255);

	// Over red: green saturates, red survives at half strength through
	// INV_SRC_ALPHA, and a backend that had clamped the source would answer
	// with a green no brighter than 128.
	const Pixel over_red = harness.at(40, 8);
	CHECK(over_red.g == 255);
	CHECK(over_red.r > 100);
	CHECK(over_red.r < 155);
	CHECK(over_red.b == 0);
}
