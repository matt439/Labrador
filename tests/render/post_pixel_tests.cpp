#include <doctest/doctest.h>

#include "engine/math/rectanglef.h"
#include "engine/math/vector2f.h"
#include "engine/render/colour.h"
#include "engine/render/post_effect.h"
#include "engine/render/renderer.h"
#include "engine/render/viewport.h"
#include "tests/render/pixel_test_harness.h"

#include <algorithm>
#include <cstdlib>
#include <optional>
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

// --- the CRT (docs/effects.md#7) --------------------------------------
//
// ASSERTED ANALYTICALLY, NOT BY GOLDEN: linear filtering and exp() differ by
// device, so each case states the property it holds (docs/effects.md#10).

namespace
{
	const PostEffect crt_chain[] = { PostEffect::crt };

	const Vector2F buffer_size(static_cast<float>(buffer_side),
		static_cast<float>(buffer_side));

	CrtSettings full_screen()
	{
		CrtSettings settings;
		settings.screen = RectangleF(0.0f, 0.0f, buffer_size.x, buffer_size.y);
		return settings;
	}

	// The whole buffer one colour, through the CRT with these settings.
	void draw_flat_through_crt(Harness& harness, const Colour& colour,
		const CrtSettings& settings)
	{
		harness.renderer().set_post_values(PostEffect::crt,
			crt_values(settings, buffer_size));
		DrawList list = harness.begin();
		list.draw_sprite(harness.quad, Harness::white_texel(),
			RectangleF(0.0f, 0.0f, buffer_size.x, buffer_size.y), colour, 0.0f,
			Vector2F::zero, SpriteFlip::none, 0.0f);
		harness.end_not_comparable();
		harness.renderer().end_frame();
	}

	int channel_distance(const Pixel& a, const Pixel& b)
	{
		return std::max({ std::abs(a.r - b.r), std::abs(a.g - b.g),
			std::abs(a.b - b.b), std::abs(a.a - b.a) });
	}
}

TEST_CASE("CRT: with every term off it is the direct frame, byte for byte")
{
	Harness harness;
	draw_busy_frame(harness);
	harness.end_not_comparable();
	const std::vector<Pixel> direct = harness.snapshot();
	harness.renderer().end_frame();

	harness.renderer().set_post_effects(crt_chain);
	harness.renderer().set_post_values(PostEffect::crt,
		crt_values(full_screen(), buffer_size));
	draw_busy_frame(harness);
	harness.end_not_comparable();
	CHECK(harness.snapshot() == direct);
}

TEST_CASE("CRT: outside the screen every pixel is the direct frame's")
{
	Harness harness;
	draw_busy_frame(harness);
	harness.end_not_comparable();
	const std::vector<Pixel> direct = harness.snapshot();
	harness.renderer().end_frame();

	CrtSettings settings;
	settings.screen = RectangleF(16.0f, 16.0f, 32.0f, 32.0f);
	settings.curvature = 0.2f;
	settings.scanline_count = 4.0f;
	settings.scanline_strength = 0.5f;
	settings.mask_strength = 0.5f;
	settings.vignette = 0.5f;
	settings.aberration = 3.0f;
	settings.brightness = 1.2f;

	harness.renderer().set_post_effects(crt_chain);
	harness.renderer().set_post_values(PostEffect::crt,
		crt_values(settings, buffer_size));
	draw_busy_frame(harness);
	harness.end_not_comparable();

	int inside_changed = 0;
	for (int y = 0; y < buffer_side; ++y)
	{
		for (int x = 0; x < buffer_side; ++x)
		{
			const Pixel before =
				direct[static_cast<size_t>(y * buffer_side + x)];
			const bool inside = x >= 16 && x < 48 && y >= 16 && y < 48;
			if (inside)
			{
				inside_changed += before == harness.at(x, y) ? 0 : 1;
				continue;
			}
			CHECK_MESSAGE(harness.at(x, y) == before,
				"pixel (", x, ", ", y, ") outside the screen changed");
		}
	}
	// And the CRT did something inside it.
	CHECK(inside_changed > 100);
}

TEST_CASE("CRT: curvature blacks out what crt_source_position calls past the tube")
{
	Harness harness;
	harness.renderer().set_post_effects(crt_chain);
	CrtSettings settings = full_screen();
	settings.curvature = 0.3f;
	draw_flat_through_crt(harness, Colour::white, settings);

	CHECK(harness.at(0, 0) == black_pixel);
	CHECK(harness.at(63, 0) == black_pixel);
	CHECK(harness.at(0, 63) == black_pixel);
	CHECK(harness.at(63, 63) == black_pixel);
	CHECK(harness.at(31, 31) == white_pixel);
	CHECK(harness.at(32, 32) == white_pixel);

	// The CPU twin and the device agree on every pixel but those whose
	// centre lands on the tube's edge within float rounding.
	int disagreeing = 0;
	int black = 0;
	for (int y = 0; y < buffer_side; ++y)
	{
		for (int x = 0; x < buffer_side; ++x)
		{
			const bool past_tube = !crt_source_position(settings,
				Vector2F(static_cast<float>(x) + 0.5f,
					static_cast<float>(y) + 0.5f)).has_value();
			const Pixel pixel = harness.at(x, y);
			black += pixel == black_pixel ? 1 : 0;
			const Pixel expected = past_tube ? black_pixel : white_pixel;
			disagreeing += pixel == expected ? 0 : 1;
		}
	}
	CHECK(black > 0);
	CHECK_MESSAGE(disagreeing <= 4, disagreeing,
		" pixels disagree with crt_source_position");
}

TEST_CASE("CRT: the mask repeats every three columns, each phase one channel")
{
	Harness harness;
	harness.renderer().set_post_effects(crt_chain);
	CrtSettings settings = full_screen();
	settings.mask_strength = 1.0f;
	draw_flat_through_crt(harness, Colour::white, settings);

	for (int x = 0; x + 3 < buffer_side; ++x)
	{
		CAPTURE(x);
		CHECK(harness.at(x, 20) == harness.at(x + 3, 20));
		const Pixel pixel = harness.at(x, 20);
		const int phase = x % 3;
		if (phase == 0)
		{
			CHECK(pixel.r > pixel.g);
			CHECK(pixel.r > pixel.b);
		}
		else if (phase == 1)
		{
			CHECK(pixel.g > pixel.r);
			CHECK(pixel.g > pixel.b);
		}
		else
		{
			CHECK(pixel.b > pixel.r);
			CHECK(pixel.b > pixel.g);
		}
	}
}

TEST_CASE("CRT: scanlines repeat with their period, and thin more on dark rows")
{
	// 64 rows / 8 scanlines = 8 pixels a line.
	CrtSettings settings = full_screen();
	settings.scanline_count = 8.0f;
	settings.scanline_strength = 1.0f;

	float contrast_dark = 0.0f;
	float contrast_bright = 0.0f;
	for (const float level : { 0.3f, 0.8f })
	{
		CAPTURE(level);
		Harness harness;
		harness.renderer().set_post_effects(crt_chain);
		draw_flat_through_crt(harness, Colour(level, level, level, 1.0f),
			settings);

		int darkest = 255;
		int brightest = 0;
		for (int y = 0; y < buffer_side; ++y)
		{
			if (y + 8 < buffer_side)
			{
				CHECK(channel_distance(harness.at(32, y),
					harness.at(32, y + 8)) <= 1);
			}
			darkest = std::min(darkest, static_cast<int>(harness.at(32, y).g));
			brightest = std::max(brightest,
				static_cast<int>(harness.at(32, y).g));
		}
		REQUIRE(brightest > 0);
		const float contrast = static_cast<float>(darkest) /
			static_cast<float>(brightest);
		if (level < 0.5f)
		{
			contrast_dark = contrast;
		}
		else
		{
			contrast_bright = contrast;
		}
	}
	// A narrower beam leaves darker gaps: the dark band's ratio is lower.
	CHECK(contrast_dark < contrast_bright);
}

TEST_CASE("CRT: the vignette peaks in the centre and mirrors left to right")
{
	Harness harness;
	harness.renderer().set_post_effects(crt_chain);
	CrtSettings settings = full_screen();
	settings.vignette = 1.0f;
	draw_flat_through_crt(harness, Colour::white, settings);

	const int centre = harness.at(31, 31).r;
	for (int y = 0; y < buffer_side; ++y)
	{
		for (int x = 0; x < buffer_side; ++x)
		{
			CHECK(harness.at(x, y).r <= centre + 1);
			CHECK(channel_distance(harness.at(x, y),
				harness.at(buffer_side - 1 - x, y)) <= 1);
		}
	}
	CHECK(harness.at(0, 0).r < centre);
	for (int i = 0; i < 31; ++i)
	{
		CHECK(harness.at(i, i).r <= harness.at(i + 1, i + 1).r + 1);
	}
}

TEST_CASE("CRT: aberration is radial: none at the centre, mirrored per channel, split off it")
{
	Harness harness;
	harness.renderer().set_post_effects(crt_chain);
	CrtSettings settings = full_screen();
	settings.aberration = 4.0f;
	harness.renderer().set_post_values(PostEffect::crt,
		crt_values(settings, buffer_size));

	// A wide white band down the centre, and a thin column a quarter in from
	// each side.
	DrawList list = harness.begin();
	const RectangleF columns[] = { RectangleF(15.0f, 0.0f, 2.0f, 64.0f),
		RectangleF(29.0f, 0.0f, 6.0f, 64.0f),
		RectangleF(47.0f, 0.0f, 2.0f, 64.0f) };
	for (const RectangleF& column : columns)
	{
		list.draw_sprite(harness.quad, Harness::white_texel(), column,
			Colour::white, 0.0f, Vector2F::zero, SpriteFlip::none, 0.0f);
	}
	harness.end_not_comparable();

	// Inside the band at the centre the fringe is a fraction of a pixel and
	// every tap reads white.
	CHECK(harness.at(31, 32) == white_pixel);
	CHECK(harness.at(32, 32) == white_pixel);

	// RED IS PULLED OUTWARD AND BLUE INWARD ON BOTH SIDES, so the mirror of a
	// pixel matches it channel for channel - and off the centre a thin
	// column's edge shows red and blue apart.
	bool split = false;
	float red_x = 0.0f;
	float red_weight = 0.0f;
	float blue_x = 0.0f;
	float blue_weight = 0.0f;
	for (int x = 8; x < 24; ++x)
	{
		CAPTURE(x);
		const Pixel left = harness.at(x, 32);
		const Pixel right = harness.at(buffer_side - 1 - x, 32);
		split = split || left.r != left.b;
		CHECK(channel_distance(left, right) <= 1);
		red_x += static_cast<float>(x * left.r);
		red_weight += static_cast<float>(left.r);
		blue_x += static_cast<float>(x * left.b);
		blue_weight += static_cast<float>(left.b);
	}
	CHECK(split);

	// AND WHICH WAY: on the left of the centre the red image of the column
	// sits nearer the centre than the blue one (crt.hlsl samples red from
	// further out). Swapping the fringe's sign would mirror this.
	REQUIRE(red_weight > 0.0f);
	REQUIRE(blue_weight > 0.0f);
	CHECK(red_x / red_weight > blue_x / blue_weight);
}

TEST_CASE("CRT: every sampled position is crt_source_position's, on an offset screen")
{
	Harness harness;
	harness.renderer().set_post_effects(crt_chain);
	CrtSettings settings;
	settings.screen = RectangleF(8.0f, 8.0f, 48.0f, 48.0f);
	settings.curvature = 0.15f;
	harness.renderer().set_post_values(PostEffect::crt,
		crt_values(settings, buffer_size));

	// THE SCENE ENCODES ITS OWN COORDINATES: red is 4 * column and green
	// 4 * row, added under the premultiplied blend by tints with zero alpha.
	// A linear tap at pixel position q then reads 4 * (q - 0.5), so every
	// output pixel says where the CRT sampled it from.
	DrawList list = harness.begin();
	for (int i = 0; i < buffer_side; ++i)
	{
		const float level = static_cast<float>(4 * i) / 255.0f;
		list.draw_sprite(harness.quad, Harness::white_texel(),
			RectangleF(static_cast<float>(i), 0.0f, 1.0f, buffer_size.y),
			Colour(level, 0.0f, 0.0f, 0.0f), 0.0f, Vector2F::zero,
			SpriteFlip::none, 0.0f);
		list.draw_sprite(harness.quad, Harness::white_texel(),
			RectangleF(0.0f, static_cast<float>(i), buffer_size.x, 1.0f),
			Colour(0.0f, level, 0.0f, 0.0f), 0.0f, Vector2F::zero,
			SpriteFlip::none, 0.0f);
	}
	harness.end_not_comparable();

	int compared = 0;
	for (int y = 8; y < 56; ++y)
	{
		for (int x = 8; x < 56; ++x)
		{
			const std::optional<Vector2F> source = crt_source_position(
				settings, Vector2F(static_cast<float>(x) + 0.5f,
					static_cast<float>(y) + 0.5f));
			if (!source.has_value())
			{
				continue;
			}
			const Pixel pixel = harness.at(x, y);
			const float sampled_x = static_cast<float>(pixel.r) / 4.0f + 0.5f;
			const float sampled_y = static_cast<float>(pixel.g) / 4.0f + 0.5f;
			CAPTURE(x);
			CAPTURE(y);
			// Within a pixel: quantisation is a quarter, and the clamp to the
			// screen's texel centres moves an edge tap by at most a half.
			CHECK(std::abs(sampled_x - source->x) <= 1.0f);
			CHECK(std::abs(sampled_y - source->y) <= 1.0f);
			++compared;
		}
	}
	CHECK(compared > 1500);
}

TEST_CASE("CRT: every sampled position is crt_source_position's, on an offset screen")
{
	Harness harness;
	harness.renderer().set_post_effects(crt_chain);
	CrtSettings settings;
	settings.screen = RectangleF(8.0f, 8.0f, 48.0f, 48.0f);
	settings.curvature = 0.15f;
	harness.renderer().set_post_values(PostEffect::crt,
		crt_values(settings, buffer_size));

	// THE SCENE ENCODES ITS OWN COORDINATES: red is 4 * column and green
	// 4 * row, added under the premultiplied blend by tints with zero alpha.
	// A linear tap at pixel position q then reads 4 * (q - 0.5), so every
	// output pixel says where the CRT sampled it from.
	DrawList list = harness.begin();
	for (int i = 0; i < buffer_side; ++i)
	{
		const float level = static_cast<float>(4 * i) / 255.0f;
		list.draw_sprite(harness.quad, Harness::white_texel(),
			RectangleF(static_cast<float>(i), 0.0f, 1.0f, buffer_size.y),
			Colour(level, 0.0f, 0.0f, 0.0f), 0.0f, Vector2F::zero,
			SpriteFlip::none, 0.0f);
		list.draw_sprite(harness.quad, Harness::white_texel(),
			RectangleF(0.0f, static_cast<float>(i), buffer_size.x, 1.0f),
			Colour(0.0f, level, 0.0f, 0.0f), 0.0f, Vector2F::zero,
			SpriteFlip::none, 0.0f);
	}
	harness.end_not_comparable();

	int compared = 0;
	for (int y = 8; y < 56; ++y)
	{
		for (int x = 8; x < 56; ++x)
		{
			const std::optional<Vector2F> source = crt_source_position(
				settings, Vector2F(static_cast<float>(x) + 0.5f,
					static_cast<float>(y) + 0.5f));
			if (!source.has_value())
			{
				continue;
			}
			const Pixel pixel = harness.at(x, y);
			const float sampled_x = static_cast<float>(pixel.r) / 4.0f + 0.5f;
			const float sampled_y = static_cast<float>(pixel.g) / 4.0f + 0.5f;
			CAPTURE(x);
			CAPTURE(y);
			// Within a pixel: quantisation is a quarter, and the clamp to the
			// screen's texel centres moves an edge tap by at most a half.
			CHECK(std::abs(sampled_x - source->x) <= 1.0f);
			CHECK(std::abs(sampled_y - source->y) <= 1.0f);
			++compared;
		}
	}
	CHECK(compared > 1500);
}
