#include <doctest/doctest.h>

#include "engine/render/material.h"
#include "engine/render/null/recording.h"
#include "engine/render/renderer.h"

#include <stdexcept>
#include <tuple>

using namespace labrador;

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
