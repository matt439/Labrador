#include <doctest/doctest.h>

#include "engine/render/material.h"
#include "engine/render/null/recording.h"
#include "engine/render/renderer.h"
#include "tests/render/material_transition_scene.h"
#include "tests/render/null_test_harness.h"

#include <stdexcept>
#include <tuple>
#include <vector>

using namespace labrador;

TEST_CASE("MATERIAL: split views and the HUD retain their order across material changes and resize")
{
	render_tests::null_backend::Harness harness;
	Renderer& renderer = harness.renderer();
	REQUIRE(renderer.window_size_changed(64, 64));
	const MaterialHandle first = renderer.add_material(
		{ MaterialKind::null, {}, "test_material" });
	const MaterialHandle second = renderer.add_material(
		{ MaterialKind::null, {}, "reverse_material" });

	const auto check_frame = [&](int width, int height, MaterialHandle material,
		const PostValues& values)
	{
		INFO("frame ", width, "x", height, ", material ", material.index());
		renderer.begin_frame();
		render_tests::draw_material_transition_scene(renderer, harness.quad,
			RectangleI(1, 1, 1, 1), width, height, material.valid());
		renderer.submit();

		const float half = static_cast<float>(width / 2);
		const float full_height = static_cast<float>(height);
		const RectangleF pane(0.0f, 0.0f, half, full_height);
		const RectangleF viewports[] = { pane,
			RectangleF(half, 0.0f, half, full_height),
			RectangleF(0.0f, 0.0f, static_cast<float>(width), full_height) };
		const RectangleF bounds[] = { pane, pane,
			RectangleF(half - 3.0f, 5.0f, 6.0f, 3.0f) };
		const Colour colours[] = { Colour::red, Colour::blue, Colour::yellow };
		const std::vector<RecordedSprite>& sprites = recorded_sprites(renderer);
		const int view_count = material.valid() ? 3 : 2;
		CHECK(renderer.view_count() == view_count);
		REQUIRE(sprites.size() == static_cast<std::size_t>(view_count));
		for (int view = 0; view < view_count; ++view)
		{
			CAPTURE(view);
			const RecordedSprite& sprite = sprites[view];
			CHECK(sprite.view == view);
			CHECK(sprite.viewport.rectangle() == viewports[view]);
			CHECK(sprite.texture.index() == harness.quad.index());
			CHECK(sprite.corners[0].position == bounds[view].top_left());
			CHECK(sprite.corners[3].position == bounds[view].bottom_right());
			CHECK(sprite.corners[0].colour == colours[view]);
		}

		const std::vector<RecordedPass>& passes = recorded_passes(renderer);
		if (material.valid())
		{
			REQUIRE(passes.size() == 1);
			CHECK(passes[0].material == material);
			CHECK(passes[0].values.values == values.values);
			CHECK(passes[0].processed_views == 2);
			CHECK(passes[0].width == width);
			CHECK(passes[0].height == height);
		}
		else
		{
			CHECK(passes.empty());
		}
		renderer.end_frame();
	};

	PostValues first_values;
	first_values.values[0] = 1.0f;
	renderer.set_post_material(first);
	renderer.set_post_values(first, first_values);
	check_frame(64, 64, first, first_values);

	PostValues second_values;
	second_values.values[4] = 1.0f;
	renderer.set_post_material(second);
	renderer.set_post_values(second, second_values);
	check_frame(64, 64, second, second_values);
	REQUIRE(renderer.window_size_changed(48, 40));
	check_frame(48, 40, second, second_values);
	renderer.set_post_material(first);
	check_frame(48, 40, first, first_values);
	renderer.set_post_effects({});
	check_frame(48, 40, {}, {});
}

TEST_CASE("MATERIAL: null records the selected user material and live values")
{
	Renderer renderer;
	renderer.create_device(nullptr, 64, 48, 3);
	const MaterialHandle handle = renderer.add_material(
		{ MaterialKind::null, {}, "recorded_material" });
	renderer.set_post_material(handle);
	renderer.begin_frame();
	renderer.set_view_count(1);
	renderer.begin_overlay();
	PostValues values;
	values.values[0] = 0.75f;
	values.values[31] = -2.0f;
	renderer.set_post_values(handle, values);
	renderer.submit();
	REQUIRE(recorded_passes(renderer).size() == 1);
	CHECK(recorded_passes(renderer)[0].material == handle);
	CHECK(recorded_passes(renderer)[0].values.values == values.values);
	CHECK(recorded_passes(renderer)[0].processed_views == 1);
	CHECK(recorded_passes(renderer)[0].width == 64);
	CHECK(recorded_passes(renderer)[0].height == 48);
	renderer.submit();
	CHECK(recorded_passes(renderer).size() == 1);
	renderer.end_frame();

	const PostEffect identity[] = { PostEffect::identity };
	renderer.set_post_effects(identity);
	CHECK_THROWS_AS(renderer.set_post_values(handle, values),
		std::invalid_argument);
	renderer.begin_frame();
	renderer.submit();
	REQUIRE(recorded_passes(renderer).size() == 1);
	CHECK_FALSE(recorded_passes(renderer)[0].material.valid());
	renderer.end_frame();

	renderer.set_post_material(handle);
	renderer.begin_frame();
	renderer.submit();
	REQUIRE(recorded_passes(renderer).size() == 1);
	CHECK(recorded_passes(renderer)[0].material == handle);
	CHECK(recorded_passes(renderer)[0].values.values == values.values);
	renderer.end_frame();
	renderer.set_post_effects({});
	renderer.begin_frame();
	renderer.submit();
	CHECK(recorded_passes(renderer).empty());
}

TEST_CASE("MATERIAL: null preserves the active material when an operation is refused")
{
	Renderer renderer;
	Renderer other;
	renderer.create_device(nullptr, 64, 48, 1);
	other.create_device(nullptr, 64, 48, 1);
	const MaterialSource source{ MaterialKind::null, {}, "test" };
	const MaterialHandle handle = renderer.add_material(source);
	const MaterialHandle foreign = other.add_material(source);
	renderer.set_post_material(handle);
	CHECK_THROWS_AS(renderer.set_post_material(foreign),
		std::invalid_argument);
	CHECK_THROWS_AS(renderer.set_post_material(MaterialHandle{}),
		std::invalid_argument);
	renderer.begin_frame();
	CHECK_THROWS_AS(std::ignore = renderer.add_material(source),
		std::logic_error);
	CHECK_THROWS_AS(renderer.set_post_material(handle), std::logic_error);
	renderer.submit();
	REQUIRE(recorded_passes(renderer).size() == 1);
	CHECK(recorded_passes(renderer)[0].material == handle);
}
