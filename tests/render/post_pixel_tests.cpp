#include <doctest/doctest.h>

#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "engine/render/colour.h"
#include "engine/render/post_effect.h"
#include "engine/render/renderer.h"
#include "engine/render/viewport.h"
#include "tests/render/pixel_test_harness.h"

#include <stdexcept>
#include <vector>

using namespace labrador;
using namespace render_tests::pixel;

// The post stage's cases (docs/effects.md#10). Every frame here
// is asserted against another frame or a known colour, never a golden: the
// golden half of the anchor is RenderPixelTestsThroughPost.

namespace
{
	const PostEffect identity_chain[] = { PostEffect::identity };

	// Two panes, both filters, a rotation, a tint and text: enough that a
	// redirect losing a row, a column, a channel or the blend would show.
	void draw_busy_frame(Harness& harness)
	{
		DrawList left = harness.begin(2);
		left.set_viewport(Viewport(0.0f, 0.0f, 32.0f, 64.0f));
		left.set_filter(TextureFilter::linear);
		left.draw_sprite(harness.quad, Harness::whole(),
			RectangleF(1.0f, 3.0f, 29.0f, 23.0f), Colour::white, 0.0f,
			Vector2F::zero, SpriteFlip::none, 0.0f);
		left.set_filter(TextureFilter::point);
		left.draw_text(harness.font, L"Ag", Vector2F(2.0f, 34.0f),
			Colour(0.5f, 0.75f, 1.0f, 1.0f), 1.0f, 0.0f, Vector2F::zero, 0.0f);

		DrawList right = harness.view(1);
		right.set_viewport(Viewport(32.0f, 0.0f, 32.0f, 64.0f));
		right.draw_sprite(harness.quad, Harness::whole(),
			RectangleF(16.0f, 20.0f, 20.0f, 12.0f), Colour::white, 0.6f,
			Vector2F(1.0f, 1.0f), SpriteFlip::horizontal, 0.0f);
		right.draw_sprite(harness.quad, Harness::white_texel(),
			RectangleF(4.0f, 40.0f, 24.0f, 16.0f),
			Colour(0.25f, 0.0f, 0.5f, 0.5f), 0.0f, Vector2F::zero,
			SpriteFlip::none, 0.0f);
	}
}

TEST_CASE("POST: the identity chain reproduces the direct frame byte for byte")
{
	Harness harness;

	draw_busy_frame(harness);
	harness.end_not_comparable();
	const std::vector<Pixel> direct = harness.snapshot();
	harness.renderer().end_frame();

	harness.renderer().set_post_effects(identity_chain);
	draw_busy_frame(harness);
	harness.end_not_comparable();
	const std::vector<Pixel> through_post = harness.snapshot();
	harness.renderer().end_frame();

	// ZERO DRIFT, not the golden comparator's one: the same device drew both.
	REQUIRE(direct.size() == through_post.size());
	int differing = 0;
	for (std::size_t i = 0; i < direct.size(); ++i)
	{
		if (!(direct[i] == through_post[i]))
		{
			++differing;
		}
	}
	CHECK_MESSAGE(differing == 0, differing, " pixels differ through identity");
}

TEST_CASE("POST: removing the chain draws straight into the back buffer again")
{
	Harness harness;
	harness.renderer().set_post_effects(identity_chain);
	harness.renderer().set_post_effects({});

	DrawList list = harness.begin();
	list.draw_sprite(harness.quad, Harness::red_texel(),
		RectangleF(0.0f, 0.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);
	harness.end_not_comparable();

	CHECK(harness.at(0, 0) == red_pixel);
	CHECK(harness.at(8, 8) == black_pixel);
}

TEST_CASE("POST: overlay views draw over the chain's output, after it")
{
	for (const bool chain : { false, true })
	{
		CAPTURE(chain);
		Harness harness;
		if (chain)
		{
			harness.renderer().set_post_effects(identity_chain);
		}

		DrawList scene = harness.begin();
		scene.draw_sprite(harness.quad, Harness::red_texel(),
			RectangleF(0.0f, 0.0f, 64.0f, 64.0f), Colour::white, 0.0f,
			Vector2F::zero, SpriteFlip::none, 0.0f);

		harness.renderer().begin_overlay();
		REQUIRE(harness.renderer().view_count() == 2);
		DrawList overlay = harness.view(1);
		overlay.draw_sprite(harness.quad, Harness::white_texel(),
			RectangleF(8.0f, 16.0f, 4.0f, 2.0f), Colour::white, 0.0f,
			Vector2F::zero, SpriteFlip::none, 0.0f);
		harness.end_not_comparable();

		for (int y = 0; y < buffer_side; ++y)
		{
			for (int x = 0; x < buffer_side; ++x)
			{
				const bool in_overlay = x >= 8 && x < 12 && y >= 16 && y < 18;
				const Pixel expected = in_overlay ? white_pixel : red_pixel;
				CHECK_MESSAGE(harness.at(x, y) == expected,
					"pixel (", x, ", ", y, ") should be ", expected);
			}
		}
	}
}

TEST_CASE("POST: a frame with no views still presents the chain over black")
{
	Harness harness;
	harness.renderer().set_post_effects(identity_chain);

	// A frame that drew red, so the back buffer holds something to lose.
	DrawList list = harness.begin();
	list.draw_sprite(harness.quad, Harness::red_texel(),
		RectangleF(0.0f, 0.0f, 64.0f, 64.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);
	harness.end_not_comparable();
	REQUIRE(harness.at(32, 32) == red_pixel);
	harness.renderer().end_frame();

	harness.renderer().begin_frame();
	harness.renderer().submit();
	harness.end_not_comparable();
	CHECK(harness.at(0, 0) == black_pixel);
	CHECK(harness.at(32, 32) == black_pixel);
	CHECK(harness.at(63, 63) == black_pixel);
}

TEST_CASE("POST: a frame begun, drawn and never submitted contributes nothing")
{
	Harness harness;
	harness.renderer().set_post_effects(identity_chain);

	DrawList abandoned = harness.begin();
	abandoned.draw_sprite(harness.quad, Harness::red_texel(),
		RectangleF(0.0f, 0.0f, 64.0f, 64.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);

	DrawList list = harness.begin();
	list.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 4.0f, 4.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);
	harness.end_not_comparable();

	CHECK(harness.at(0, 0) == white_pixel);
	CHECK(harness.at(32, 32) == black_pixel);
}

TEST_CASE("POST: the chain survives a resize, at the new size")
{
	Harness harness;
	harness.renderer().set_post_effects(identity_chain);
	harness.resize_window(48, 40);
	REQUIRE(harness.renderer().window_size_changed(48, 40));

	DrawList list = harness.begin();
	list.draw_sprite(harness.quad, Harness::red_texel(),
		RectangleF(40.0f, 32.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);
	harness.end_not_comparable();

	REQUIRE(harness.buffer_size() == Vector2F(48.0f, 40.0f));
	CHECK(harness.at(47, 39) == red_pixel);
	CHECK(harness.at(40, 32) == red_pixel);
	CHECK(harness.at(39, 31) == black_pixel);
}

TEST_CASE("POST: the chain and the overlay refuse what they refuse")
{
	Harness harness;
	Renderer& renderer = harness.renderer();

	CHECK_THROWS_AS(renderer.begin_overlay(), std::logic_error);

	harness.begin();
	CHECK_THROWS_AS(renderer.set_post_effects(identity_chain),
		std::logic_error);
	renderer.begin_overlay();
	CHECK_THROWS_AS(renderer.begin_overlay(), std::logic_error);
	renderer.submit();
	renderer.end_frame();

	// The capacity is four; four declared views leave no room for an overlay.
	harness.begin(4);
	CHECK_THROWS_AS(renderer.begin_overlay(), std::out_of_range);
	renderer.submit();
	renderer.end_frame();
}

TEST_CASE("POST: an overlay view above a count lowered earlier still draws over the chain")
{
	Harness harness;
	harness.renderer().set_post_effects(identity_chain);
	Renderer& renderer = harness.renderer();

	// Views 1 and 2 are bound to the scene, then dropped undrawn.
	DrawList scene = harness.begin(3);
	renderer.set_view_count(1);
	scene.draw_sprite(harness.quad, Harness::red_texel(),
		RectangleF(0.0f, 0.0f, 64.0f, 64.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);

	renderer.begin_overlay();
	renderer.set_view_count(3);
	DrawList hud = harness.view(2);
	hud.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(4.0f, 4.0f, 8.0f, 8.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);
	harness.end_not_comparable();

	CHECK(harness.at(6, 6) == white_pixel);
	CHECK(harness.at(40, 40) == red_pixel);
}
