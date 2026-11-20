#include <doctest/doctest.h>

#include "engine/render/material.h"
#include "engine/render/material_table.h"
#include "engine/render/renderer.h"
#include "tests/render/material_transition_scene.h"
#include "tests/render/pixel_test_harness.h"

#include "materials/constant_material.h"
#include "materials/reverse_material.h"
#include "materials/test_material.h"

#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

using namespace labrador;
using namespace render_tests::pixel;

namespace
{
	void draw_pattern(Harness& harness)
	{
		DrawList list = harness.begin();
		const Colour colours[] = { Colour::red, Colour::lime,
			Colour::blue, Colour::white };
		for (int y = 0; y < 2; ++y)
		{
			for (int x = 0; x < 2; ++x)
			{
				list.draw_sprite(harness.quad, Harness::white_texel(),
					RectangleF(static_cast<float>(x * 32),
						static_cast<float>(y * 32), 32.0f, 32.0f),
					colours[y * 2 + x], 0.0f, Vector2F::zero,
					SpriteFlip::none, 0.0f);
			}
		}
	}

	void check_pattern(const Harness& harness, bool swapped)
	{
		for (int y = 0; y < buffer_side; ++y)
		{
			for (int x = 0; x < buffer_side; ++x)
			{
				const Pixel colours[] = { red_pixel, green_pixel,
					blue_pixel, white_pixel };
				Pixel expected = colours[(y / 32) * 2 + x / 32];
				if (swapped)
				{
					expected = Pixel{ expected.b, expected.r, expected.g,
						expected.a };
				}
				CHECK_MESSAGE(harness.at(x, y) == expected,
					"pixel (", x, ", ", y, ") should be ", expected);
			}
		}
	}
}

TEST_CASE("MATERIAL: authored sampling and parameters draw exact top-down colours")
{
	Harness harness;
	Renderer& renderer = harness.renderer();
	const MaterialHandle handle = renderer.add_material(test_material);
	renderer.set_post_material(handle);

	// The default values leave both sampling paths unchanged.
	draw_pattern(harness);
	harness.end_not_comparable();
	check_pattern(harness, false);
	renderer.end_frame();

	draw_pattern(harness);
	PostValues values;
	values.values[0] = 1.0f;
	renderer.set_post_values(handle, values);
	harness.end_not_comparable();
	check_pattern(harness, true);
	renderer.end_frame();

	// A built-in replaces the authored pass, and switching back retains its
	// own parameters. No shader creation is needed when switching.
	const PostEffect identity[] = { PostEffect::identity };
	renderer.set_post_effects(identity);
	draw_pattern(harness);
	harness.end_not_comparable();
	check_pattern(harness, false);
	renderer.end_frame();
	renderer.set_post_material(handle);
	draw_pattern(harness);
	harness.end_not_comparable();
	check_pattern(harness, true);
	renderer.end_frame();

	renderer.set_post_effects({});
	draw_pattern(harness);
	harness.end_not_comparable();
	check_pattern(harness, false);
}

TEST_CASE("MATERIAL: a constant shader may optimize away every frame resource")
{
	Harness harness;
	Renderer& renderer = harness.renderer();
	const MaterialHandle handle = renderer.add_material(constant_material);
	renderer.set_post_material(handle);
	harness.begin();
	renderer.begin_overlay();
	DrawList overlay = harness.view(1);
	overlay.draw_sprite(harness.quad, Harness::red_texel(),
		RectangleF(8.0f, 16.0f, 4.0f, 2.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);
	harness.end_not_comparable();
	for (int y = 0; y < buffer_side; ++y)
	{
		for (int x = 0; x < buffer_side; ++x)
		{
			const bool in_overlay = x >= 8 && x < 12 && y >= 16 && y < 18;
			const Pixel expected = in_overlay ? red_pixel : green_pixel;
			CHECK_MESSAGE(harness.at(x, y) == expected,
				"pixel (", x, ", ", y, ") should be ", expected);
		}
	}
}

TEST_CASE("MATERIAL: registration owns the bytes and name through later draws")
{
	Harness harness;
	Renderer& renderer = harness.renderer();
	MaterialHandle handle;
	{
		std::string name = "temporary_authored_material";
		std::vector<unsigned char> bytes(constant_material.bytes.begin(),
			constant_material.bytes.end());
		handle = renderer.add_material(
			{ constant_material.kind, bytes, name });
	}
	renderer.set_post_material(handle);
	harness.resize_window(48, 40);
	REQUIRE(renderer.window_size_changed(48, 40));
	harness.begin();
	harness.end_not_comparable();
	REQUIRE(harness.buffer_size() == Vector2F(48.0f, 40.0f));
	CHECK(harness.at(0, 0) == green_pixel);
	CHECK(harness.at(47, 39) == green_pixel);
}

TEST_CASE("MATERIAL: renderer ownership and frame boundaries are enforced")
{
	Harness harness;
	MaterialTable other;
	Renderer& renderer = harness.renderer();
	const MaterialHandle handle = renderer.add_material(constant_material);
	const MaterialHandle foreign = other.add(
		{ MaterialKind::null, {}, "foreign_material" }, MaterialKind::null,
		true, false);
	renderer.set_post_material(handle);
	CHECK_THROWS_AS(renderer.set_post_material(foreign),
		std::invalid_argument);
	CHECK_THROWS_AS(renderer.set_post_material(MaterialHandle{}),
		std::invalid_argument);
	CHECK_THROWS_AS(renderer.set_post_values(foreign, PostValues{}),
		std::invalid_argument);
	harness.begin();
	CHECK_THROWS_AS(std::ignore = renderer.add_material(constant_material),
		std::logic_error);
	CHECK_THROWS_AS(renderer.set_post_material(handle), std::logic_error);
	harness.end_not_comparable();
	CHECK(harness.at(32, 32) == green_pixel);
}

TEST_CASE("MATERIAL: split world and flat HUD survive switches, resize and removal")
{
	Harness harness;
	Renderer& renderer = harness.renderer();
	const MaterialHandle forward = renderer.add_material(test_material);
	const MaterialHandle reverse = renderer.add_material(reverse_material);

	const auto check_frame = [&](const std::string& phase, int width, int height,
		Pixel left, Pixel right, bool draw_hud = true)
	{
		renderer.begin_frame();
		render_tests::draw_material_transition_scene(renderer, harness.quad,
			Harness::white_texel(), width, height, draw_hud);
		harness.end_not_comparable();
		REQUIRE(harness.buffer_size() == Vector2F(static_cast<float>(width),
			static_cast<float>(height)));
		for (int y = 0; y < height; ++y)
		{
			for (int x = 0; x < width; ++x)
			{
				const bool hud = draw_hud && x >= width / 2 - 3 && x < width / 2 + 3 &&
					y >= 5 && y < 8;
				const Pixel world = x < width / 2 ? left : right;
				const Pixel expected = hud ? Pixel{ 255, 255, 0, 255 } : world;
				REQUIRE_MESSAGE(harness.at(x, y) == expected,
					phase, " pixel (", x, ", ", y, ") should be ", expected);
			}
		}
		renderer.end_frame();
	};

	PostValues forward_values;
	forward_values.values[0] = 1.0f;
	renderer.set_post_material(forward);
	renderer.set_post_values(forward, forward_values);
	check_frame("forward", buffer_side, buffer_side, green_pixel, red_pixel);

	PostValues reverse_values;
	reverse_values.values[4] = 1.0f;
	renderer.set_post_material(reverse);
	renderer.set_post_values(reverse, reverse_values);
	check_frame("reverse", buffer_side, buffer_side, blue_pixel, green_pixel);

	harness.resize_window(48, 40);
	REQUIRE(renderer.window_size_changed(48, 40));
	check_frame("resized reverse", 48, 40, blue_pixel, green_pixel);

	renderer.set_post_material(forward);
	check_frame("reselected forward", 48, 40, green_pixel, red_pixel);

	renderer.set_post_effects({});
	check_frame("direct without HUD", 48, 40, red_pixel, blue_pixel, false);
}
