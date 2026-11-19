#include <doctest/doctest.h>

#include "engine/render/material.h"
#include "engine/render/device_unavailable.h"
#include "engine/render/renderer.h"
#include "tests/render/pixel_test_harness.h"

#include "materials/constant_material.h"

#include <stdexcept>
#include <string>
#include <tuple>

using namespace labrador;
using namespace render_tests::pixel;

TEST_CASE("MATERIAL: a GL compiler failure names the material and rolls back registration")
{
	Harness harness;
	Renderer& renderer = harness.renderer();
	const unsigned char broken[] = "this is not valid GLSL\n";
	try
	{
		std::ignore = renderer.add_material(
			{ MaterialKind::glsl, broken, "broken_game_material" });
		FAIL("a malformed material was accepted");
	}
	catch (const RenderDeviceUnavailable&)
	{
		FAIL("a malformed authored shader became device unavailability");
	}
	catch (const std::runtime_error& error)
	{
		CHECK(std::string(error.what()).find("broken_game_material") !=
			std::string::npos);
	}
	const MaterialHandle handle = renderer.add_material(constant_material);
	CHECK(handle.index() == 0);
	renderer.set_post_material(handle);
	harness.begin();
	harness.end_not_comparable();
	CHECK(harness.at(32, 32) == green_pixel);
}
