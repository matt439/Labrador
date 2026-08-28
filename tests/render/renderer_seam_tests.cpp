#include <doctest/doctest.h>

#include "engine/render/render_resources.h"
#include "engine/render/renderer.h"
#include "engine/render/resource_factory.h"
#include "engine/render/texture_data.h"
#include "engine/render/texture_format.h"

#include <stdexcept>
#include <string>

// The seam's rules that need no device, asserted in every configuration.
//
// COMPILED UNCONDITIONALLY, unlike null_tests.cpp beside it, and that is the
// point of a separate file. It names no backend type and creates no device, so
// the same assertions run against every backend there is - which
// is the only way this repository can say "all five answer this the same way"
// at all. RenderPixelTests needs an adapter and is not built against null;
// null_tests.cpp compiles only against null. Between them they never hold two
// backends to one statement - but something else does now, and this file was
// the only mechanism that could when this was written: tests/render/golden/ is
// one set of images that all four rasterising backends are held to, byte for
// byte, which is a comparison across processes where this is a comparison
// across configurations. The two answer different halves. This file says the
// five agree about a refusal; the golden set says the four agree about a
// pixel.
//
// WHAT BELONGS HERE is anything a Renderer answers before create_device, and
// anything that is a throw rather than a pixel. What does not is anything that
// needs a frame - the moment a device is required, this file cannot run in the
// configuration whose whole purpose is having no graphics API.

namespace
{
	using namespace labrador;

	// Four opaque pixels, built by hand.
	//
	// NO FILE AND NO DEVICE. The seam's texture handover is a plain aggregate
	// (engine/render/texture_data.h), so a test can produce one directly and a
	// backend cannot tell it from one the .dds reader made. It is never
	// uploaded here - every case below is refused before anything looks at the
	// bytes - but it has to be valid, or a passing test would only prove that
	// something rejected it for the wrong reason.
	TextureData two_by_two()
	{
		TextureData texture;
		texture.width = 2;
		texture.height = 2;
		texture.format = TextureFormat::r8g8b8a8_unorm;
		texture.levels.push_back(texture_level(texture.format, 2, 2, 0));
		texture.pixels.assign(texture.levels[0].size, 0xFFu);
		return texture;
	}
}

TEST_CASE("CONTRACT: a texture loaded before there is a device is refused, by name")
{
	// A renderer that has never been given one. Every real backend holds a null
	// pointer here - a device, a WGL context, a VkDevice - and the null backend
	// holds a flag, because it has nothing to be null.
	// The table before the renderer, so it dies after one - render_resources.h
	// states the ordering as a term of the seam. Nothing here has a device for
	// it to matter to; it is written this way so that the one file a reader
	// consults for what the seam answers does not model the order backwards.
	RenderResources resources;
	Renderer renderer;
	renderer.set_resources(&resources);

	const TextureData texture = two_by_two();

	// THE ORDERING IS A RULE OF THE SEAM AND IT USED TO BE WRITTEN NOWHERE.
	// Three backends answered this three ways: gl threw, d3d11 dereferenced a
	// null device inside the D3D runtime and took the process with it, and null
	// - the configuration CI runs end to end - succeeded and handed back a
	// handle that resolved and drew. The permissive one being the one that
	// always runs is the part that matters: a client could keep the rule wrong
	// through every test it has and find out on the machine it ships to.
	CHECK_THROWS_AS(
		add_texture_asset(renderer, resources, "quad", texture),
		std::runtime_error);

	// And the name is in the message, because that is what the throw is for
	// (T6). A load ordering mistake is a client bug, and "which texture" is the
	// whole of what makes it findable.
	try
	{
		add_texture_asset(renderer, resources, "quad", texture);
		FAIL("add_texture_asset did not throw");
	}
	catch (const std::runtime_error& error)
	{
		const std::string message = error.what();
		CHECK(message.find("quad") != std::string::npos);
		CHECK(message.find("create_device") != std::string::npos);
	}

	// Nothing was added under that name either. A backend that threw after
	// putting a half-built resource in the table would leave the loader's next
	// resolve answering with it.
	CHECK_THROWS_AS(std::ignore = resources.resolve_texture("quad"),
		std::out_of_range);
}
