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

TEST_CASE("CONTRACT: a view capacity below one is refused, and it is invalid_argument")
{
	// TEST-GAP.md's A1, and what it is for is the ratchet rather than the
	// behaviour: the check exists five times by hand-copy, one line at the top
	// of each create_device, and until now nothing asserted any of them. A
	// backend written next year gets this wrong silently - a capacity of zero
	// then means a renderer with no views at all, and every view() call after
	// it is an out_of_range a client reads as its own mistake.
	//
	// THE WINDOW IS NULL AND THAT IS THE POINT. The refusal happens before
	// anything touches it, on every backend, so this case needs no device and
	// runs in all five configurations - which is the only place a statement
	// about all five can be made.
	Renderer renderer;

	CHECK_THROWS_AS(renderer.create_device(nullptr, 64, 64, 0),
		std::invalid_argument);
	CHECK_THROWS_AS(renderer.create_device(nullptr, 64, 64, -1),
		std::invalid_argument);

	// And it left nothing half-made behind it.
	CHECK(renderer.view_count() == 0);
}

TEST_CASE("CONTRACT: a renderer with no device has no views")
{
	// TEST-GAP.md's A5, and the smallest claim in this file: the answer falls
	// out of a default-initialised member rather than out of a hand-written
	// guard per backend, so there is no copy to drift. What it pins is that
	// view() refuses rather than answering with a fullscreen pane, which is
	// what the seam says and what one backend used to do instead.
	Renderer renderer;

	CHECK(renderer.view_count() == 0);
	CHECK_THROWS_AS(std::ignore = renderer.view(0), std::out_of_range);
}

TEST_CASE("CONTRACT: window_size_changed before there is a device rebuilds nothing")
{
	// TEST-GAP.md's A4. A shell can be sent a WM_SIZE between making its window
	// and making its device - Win32 allows it and nothing in engine/app/
	// forbids it - and four of the five backends answered that by walking a
	// path with a null device in it. The seam says the answer is false, and
	// this is where the five are held to it.
	Renderer renderer;

	CHECK_FALSE(renderer.window_size_changed(640, 480));

	// Twice, with a different size, because the early-out that makes the first
	// one false must not be the "nothing changed" comparison - that one would
	// answer true here on any backend that had already recorded a size.
	CHECK_FALSE(renderer.window_size_changed(1280, 720));
	CHECK(renderer.view_count() == 0);
}

TEST_CASE("CONTRACT: a marker is legal before there is a device")
{
	// TEST-GAP.md's A2, and it is the executable half of the decision
	// docs/review/backend-equivalence-2/ asked for: markers stay on the seam
	// and renderer.h now calls them advisory. An advisory call that
	// access-violates on one backend is not advisory, and that is exactly what
	// begin_marker did on d3d11 - ID3DUserDefinedAnnotation is made with the
	// device, so the ComPtr holding it was null and the forward dereferenced
	// it. The other four discarded the call.
	//
	// THERE IS NOTHING TO OBSERVE AND THAT IS THE ASSERTION. A marker leaves
	// no trace a test can read on any of the five - the one backend that
	// forwards them forwards them to a tool that is not running - so what this
	// case can say is that the calls are reachable, in any order, before a
	// device exists and outside a frame, and that nothing throws. Without it
	// the seam's one advisory-capability claim would be unexecutable in every
	// configuration.
	Renderer renderer;

	CHECK_NOTHROW(renderer.begin_marker(L"before any device"));
	CHECK_NOTHROW(renderer.set_marker(L"still no device"));
	CHECK_NOTHROW(renderer.end_marker());

	// Unpaired, because the seam does not say a backend tracks nesting and the
	// four that discard cannot. A caller that ends a marker it never began gets
	// the same nothing.
	CHECK_NOTHROW(renderer.end_marker());
}
