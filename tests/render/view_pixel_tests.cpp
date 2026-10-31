#include "tests/render/pixel_test_harness.h"

#include "engine/core/game_object.h"
#include "engine/math/rectanglef.h"
#include "engine/math/rectanglei.h"
#include "engine/math/vector2f.h"
#include "engine/render/camera.h"
#include "engine/render/colour.h"
#include "engine/render/renderer.h"
#include "engine/render/resource_factory.h"
#include "engine/scene/scene.h"

#include <doctest/doctest.h>

#include <memory>

using namespace labrador;
using namespace render_tests::pixel;

namespace
{
	void check_red_and_blue_frame(const Harness& harness,
		const RectangleI& red, const RectangleI& blue)
	{
		const float size = static_cast<float>(buffer_side);
		REQUIRE(harness.buffer_size() == Vector2F(size, size));
		REQUIRE(harness.byte_count() == buffer_side * buffer_side * 4);
		for (int y = 0; y < buffer_side; ++y)
		{
			for (int x = 0; x < buffer_side; ++x)
			{
				Pixel expected = black_pixel;
				if (x >= red.x && x < red.x + red.width &&
					y >= red.y && y < red.y + red.height)
				{
					expected = red_pixel;
				}
				if (x >= blue.x && x < blue.x + blue.width &&
					y >= blue.y && y < blue.y + blue.height)
				{
					expected = blue_pixel;
				}
				CHECK_MESSAGE(harness.at(x, y) == expected,
					"pixel (", x, ", ", y, ") should be ", expected);
			}
		}
	}
}

// --- the viewport -----------------------------------------------------------
//
// NOTHING HERE CALLED set_viewport UNTIL NOW, which is why the two device
// backends were able to disagree about a pane for as long as they did. The
// harness above builds one integral 64x64 view, every sample takes its
// resolution from a Vector2I, and the seam's only production caller of
// set_viewport is engine/scene/scene.cpp, which no test reached when this was
// written. So the pane arithmetic was exercised by neither the pixel contract
// nor the recording. One test reaches that call site now -
// tests/scene/fanout_tests.cpp drives Scene::draw in the null configuration -
// so the production path is covered there and the pane it asks for is covered
// here.
//
// THE RULE ITSELF IS PINNED HEADLESSLY, in tests/render/viewport_tests.cpp,
// which runs in every configuration including the null one. What that file
// cannot see is whether a backend USES it - that is one line in each of two
// files and needs a rasteriser to observe. These two cases are that
// observation, and they are the reason the file is worth the device.

TEST_CASE("CONTRACT: a viewport offsets the pane and scales the whole of it")
{
	Harness harness;

	DrawList list = harness.begin();

	// Deliberately not symmetric in y. A pane at (32, 8) 32x24 sits at rows
	// 8..31 measured from the top and rows 32..55 measured from the bottom, so
	// a backend that forgot GL's origin flip - or applied it twice - lands
	// somewhere these assertions can tell apart. A centred pane could not.
	list.set_viewport(Viewport(32.0f, 8.0f, 32.0f, 24.0f));
	list.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 32.0f, 24.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);
	harness.end();

	// The pane is filled corner to corner. THE FAR CORNER IS THE ASSERTION
	// THAT MATTERS: a backend that offset the rasteriser correctly but built
	// its pixels-to-clip transform from the back buffer instead of the pane
	// would put this sprite in the top left quarter of the pane and leave
	// (63, 31) black. That is exactly the shape of the bug the GL backend had.
	CHECK(harness.at(32, 8) == white_pixel);
	CHECK(harness.at(63, 31) == white_pixel);
	CHECK(harness.at(47, 20) == white_pixel);

	// And nothing outside it. A viewport confines; it does not merely offset.
	CHECK(harness.at(31, 8) == black_pixel);
	CHECK(harness.at(32, 7) == black_pixel);
	CHECK(harness.at(32, 32) == black_pixel);
	CHECK(harness.at(0, 0) == black_pixel);
}

TEST_CASE("CONTRACT: a viewport confines a sprite bigger than its pane")
{
	Harness harness;

	// TEST-GAP.md's B2, and it is the half of "a viewport confines" that the
	// case above cannot reach: there the sprite exactly fills its pane, so a
	// backend that only OFFSET the pane and never clipped it would pass. Here
	// the sprite is twice the pane in both axes and starts outside it, so what
	// is being asserted is the clip.
	//
	// AND NO BACKEND OWNS THE ANSWER, which is why it belongs in a golden. The
	// clip is not a scissor anybody sets: the pane's width and height are the
	// pixels-to-clip constant (sprite_geometry.h), so a destination outside the
	// pane is outside clip space and the rasteriser drops it. That is the same
	// mechanism on all four, and this frame is where they are held to it.
	DrawList list = harness.begin();
	list.set_viewport(Viewport(16.0f, 16.0f, 16.0f, 16.0f));
	list.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(-8.0f, -8.0f, 32.0f, 32.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);
	harness.end();

	// Every pixel of the pane is covered, corners included.
	CHECK(harness.at(16, 16) == white_pixel);
	CHECK(harness.at(31, 31) == white_pixel);
	CHECK(harness.at(24, 24) == white_pixel);

	// And not one pixel outside it, on any side. A sprite that started eight
	// pixels above and to the left of the pane would paint into the rows and
	// columns before it if nothing clipped.
	CHECK(harness.at(15, 16) == black_pixel);
	CHECK(harness.at(16, 15) == black_pixel);
	CHECK(harness.at(32, 31) == black_pixel);
	CHECK(harness.at(31, 32) == black_pixel);
	CHECK(harness.at(8, 8) == black_pixel);
}

TEST_CASE("CONTRACT: off-buffer viewports preserve placement and clip to the buffer")
{
	struct PaneCase
	{
		const char* name;
		Viewport viewport;
		RectangleI red;
		RectangleI blue;
	};
	const PaneCase cases[] = {
		{ "past right and bottom", Viewport(12, 8, 80, 72),
			RectangleI(12, 8, 52, 56), RectangleI(24, 18, 10, 8) },
		{ "before left and top", Viewport(-8, -6, 32, 24),
			RectangleI(0, 0, 24, 18), RectangleI(4, 4, 10, 8) },
		{ "past all four edges", Viewport(-8, -6, 80, 72),
			RectangleI(0, 0, 64, 64), RectangleI(4, 4, 10, 8) },
		{ "wholly right", Viewport(80, 8, 32, 24),
			RectangleI::zero, RectangleI::zero },
		{ "wholly below", Viewport(12, 72, 32, 24),
			RectangleI::zero, RectangleI::zero },
		{ "wholly left", Viewport(-40, 8, 32, 24),
			RectangleI::zero, RectangleI::zero },
		{ "wholly above", Viewport(12, -30, 32, 24),
			RectangleI::zero, RectangleI::zero }
	};
	Harness harness;
	for (const PaneCase& pane : cases)
	{
		CAPTURE(pane.name);
		DrawList list = harness.begin();
		list.set_viewport(pane.viewport);
		// The red sprite exceeds every pane. The blue marker keeps its local
		// pixel size and offset even when only part of the pane is on screen;
		// cropping must not relocate or rescale it to fit the visible portion.
		list.draw_sprite(harness.quad, Harness::white_texel(),
			RectangleF(-64, -64, 192, 192), Colour(1.0f, 0.0f, 0.0f, 1.0f), 0.0f,
			Vector2F::zero, SpriteFlip::none, 0.0f);
		list.draw_sprite(harness.quad, Harness::white_texel(),
			RectangleF(12, 10, 10, 8), Colour(0.0f, 0.0f, 1.0f, 1.0f), 0.0f,
			Vector2F::zero, SpriteFlip::none, 0.0f);
		// Every expected pixel follows from the rectangles above, so no golden
		// image is needed to accept output from the renderer under test.
		harness.end_not_comparable();
		check_red_and_blue_frame(harness, pane.red, pane.blue);
	}
}

TEST_CASE("CONTRACT: empty viewports draw nothing and leave the next pane usable")
{
	const Viewport empty_panes[] = {
		Viewport(12, 8, 0, 24), Viewport(12, 8, 32, 0),
		Viewport(5.2f, 5.2f, 0.5f, 24),
		Viewport(5.2f, 5.2f, 32, 0.5f)
	};
	Harness harness;
	for (const Viewport& pane : empty_panes)
	{
		CAPTURE(pane.width);
		CAPTURE(pane.height);
		DrawList list = harness.begin();
		list.set_viewport(pane);
		list.draw_sprite(harness.quad, Harness::white_texel(),
			RectangleF(0, 0, 64, 64), Colour(1.0f, 0.0f, 0.0f, 1.0f), 0.0f,
			Vector2F::zero, SpriteFlip::none, 0.0f);
		list.set_viewport(Viewport(0, 0, 64, 64));
		list.draw_sprite(harness.quad, Harness::white_texel(),
			RectangleF(4, 5, 8, 6), Colour(0.0f, 0.0f, 1.0f, 1.0f), 0.0f,
			Vector2F::zero, SpriteFlip::none, 0.0f);
		// Analytic readback also covers panes whose edges quantize to no width
		// or height, without generating a reference from that readback.
		harness.end_not_comparable();
		check_red_and_blue_frame(harness, RectangleI::zero,
			RectangleI(4, 5, 8, 6));
	}
}

TEST_CASE("CONTRACT: two panes splitting a fraction cover every row between them")
{
	Harness harness;

	DrawList list = harness.begin();

	// 64 rows split at 31.5, which is what an odd client height does to a
	// two-player layout and what no integral test can produce. Each edge
	// truncates and the size is the difference (Viewport::pixel_rect), so the
	// panes come out 31 rows and 33 rows and meet exactly at row 31.
	//
	// TRUNCATING THE POSITION AND THE SIZE SEPARATELY - which is what a
	// backend converting inline does - gives 31 rows and 32 rows. Those cover
	// rows 0..30 and 31..62, and row 63 belongs to neither.
	list.set_viewport(Viewport(0.0f, 0.0f, 64.0f, 31.5f));
	list.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 64.0f, 31.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);

	list.set_viewport(Viewport(0.0f, 31.5f, 64.0f, 32.5f));
	list.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 64.0f, 33.0f), Colour::white, 0.0f,
		Vector2F::zero, SpriteFlip::none, 0.0f);

	harness.end();

	// EVERY ROW, stated as a loop rather than as three sampled rows, because
	// the failure this exists to catch is a single row of clear colour and
	// sampling is how it survived this long.
	for (int y = 0; y < buffer_side; y++)
	{
		CHECK_MESSAGE(harness.at(32, y) == white_pixel,
			"row ", y, " is not covered by either pane");
	}

	// Changing the viewport mid-list is legal and costs a flush, so the two
	// panes have to be two runs. If the second set_viewport had not flushed,
	// the first pane's sprite would have been rasterised under the second
	// pane's transform and the top of the frame would be black.
	CHECK(harness.at(0, 0) == white_pixel);
	CHECK(harness.at(63, 63) == white_pixel);
}

// THE FOUR CASES BELOW ARE THE FIRST MULTI-VIEW ANYTHING ON A RASTERISER.
//
// Every other case in this file fills one view, and the harness could not do
// otherwise until its capacity went from 1 to 4 - so the machinery the backends
// share least had until then been exercised by the null backend's recording
// tests and by nothing else. D3D11 gives each view a deferred context and binds
// a render target and a viewport into it, then finishes and executes one
// command list per view; D3D12 gives each view a command list and an allocator
// per frame in flight and hands the finished lists to its queue as one array;
// GL and Vulkan each give a view a vector of runs and replay them in view
// order, the second into one command buffer for the whole frame. Those are not
// four spellings of one thing, and view order, view isolation and what a view starts
// a frame holding are decided separately in each. What makes these worth
// writing now rather than then is the golden set:
// a multi-view frame is one more image, so agreeing about split-screen is
// checked at the same resolution as agreeing about one sprite.

TEST_CASE("CONTRACT: views draw in view order, whatever order they were filled")
{
	Harness harness;

	// Filled 1 then 0, submitted 0 then 1. The two orders are opposite on
	// purpose: everywhere else in this file they are the same order, so a
	// backend that replayed in the order the calls arrived would pass every
	// other case here and lose split-screen's layering entirely.
	DrawList zero = harness.begin(2);
	DrawList one = harness.view(1);

	one.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 64.0f, 64.0f), Colour(1.0f, 0.0f, 0.0f, 1.0f),
		0.0f, Vector2F::zero, SpriteFlip::none, 0.0f);
	zero.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 64.0f, 64.0f), Colour(0.0f, 0.0f, 1.0f, 1.0f),
		0.0f, Vector2F::zero, SpriteFlip::none, 0.0f);

	harness.end();

	// View 1 went last and is opaque, so view 1 is the whole frame. Call order
	// would have left this blue.
	CHECK(harness.at(0, 0) == red_pixel);
	CHECK(harness.at(32, 32) == red_pixel);
	CHECK(harness.at(63, 63) == red_pixel);
}

TEST_CASE("CONTRACT: view state belongs to its view and not to the frame")
{
	Harness harness;

	// Both subcases put view 0's state somewhere that would MOVE view 1's
	// sprite if it leaked, rather than somewhere that would merely change it.
	// A leak that displaced a pane by nothing is the leak that survives.
	SUBCASE("a pane set on one view does not confine its neighbour")
	{
		DrawList zero = harness.begin(2);
		zero.set_viewport(Viewport(0.0f, 0.0f, 32.0f, 64.0f));
		zero.draw_sprite(harness.quad, Harness::white_texel(),
			RectangleF(0.0f, 0.0f, 32.0f, 64.0f),
			Colour(1.0f, 0.0f, 0.0f, 1.0f), 0.0f, Vector2F::zero,
			SpriteFlip::none, 0.0f);

		// No set_viewport at all, so this view's pane is the whole buffer and
		// its sprite is the far half of it. Under view 0's pane the same
		// destination is off the right-hand edge of a 32-wide view and clips
		// away to nothing.
		DrawList one = harness.view(1);
		one.draw_sprite(harness.quad, Harness::white_texel(),
			RectangleF(32.0f, 0.0f, 32.0f, 64.0f),
			Colour(0.0f, 0.0f, 1.0f, 1.0f), 0.0f, Vector2F::zero,
			SpriteFlip::none, 0.0f);

		harness.end();

		CHECK(harness.at(16, 32) == red_pixel);
		CHECK(harness.at(48, 32) == blue_pixel);
	}

	SUBCASE("a camera set on one view does not move its neighbour")
	{
		// view = (world - translation) * scale, so view 0's sprite is at world
		// x 32 and lands at the left of the frame.
		DrawList zero = harness.begin(2);
		zero.set_camera(Camera(32.0f, 0.0f, 1.0f));
		zero.draw_sprite(harness.quad, Harness::white_texel(),
			RectangleF(32.0f, 0.0f, 32.0f, 64.0f),
			Colour(1.0f, 0.0f, 0.0f, 1.0f), 0.0f, Vector2F::zero,
			SpriteFlip::none, 0.0f);

		// The same rectangle, no camera, so it stays where it was written. A
		// leaked camera would put it on top of view 0's instead.
		DrawList one = harness.view(1);
		one.draw_sprite(harness.quad, Harness::white_texel(),
			RectangleF(32.0f, 0.0f, 32.0f, 64.0f),
			Colour(0.0f, 0.0f, 1.0f, 1.0f), 0.0f, Vector2F::zero,
			SpriteFlip::none, 0.0f);

		harness.end();

		CHECK(harness.at(16, 32) == red_pixel);
		CHECK(harness.at(48, 32) == blue_pixel);
	}
}

TEST_CASE("CONTRACT: a view starts every frame holding nothing")
{
	Harness harness;

	// A pane in the corner and a camera off the origin, both on view 0.
	DrawList first = harness.begin();
	first.set_viewport(Viewport(32.0f, 0.0f, 32.0f, 64.0f));
	first.set_camera(Camera(8.0f, 8.0f, 1.0f));
	first.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(8.0f, 8.0f, 32.0f, 64.0f), Colour(1.0f, 0.0f, 0.0f, 1.0f),
		0.0f, Vector2F::zero, SpriteFlip::none, 0.0f);
	harness.end();

	// The same view index, the next frame, and it sets neither. The five
	// backends reset in two places - begin_frame on gl, null and vulkan, and
	// set_view_count's rebind on the two Direct3D ones - so this is the term
	// that says the five arrive at the same state rather than at five
	// defensible ones.
	DrawList second = harness.begin();
	second.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 64.0f, 64.0f), Colour(0.0f, 0.0f, 1.0f, 1.0f),
		0.0f, Vector2F::zero, SpriteFlip::none, 0.0f);
	harness.end();

	// Corner to corner. A pane that survived would leave the left half black;
	// a camera that survived would leave the far corner black, because the
	// sprite would start eight pixels up and to the left of where it says.
	CHECK(harness.at(0, 0) == blue_pixel);
	CHECK(harness.at(32, 32) == blue_pixel);
	CHECK(harness.at(63, 63) == blue_pixel);
}

TEST_CASE("CONTRACT: declaring another view leaves the filled ones alone")
{
	Harness harness;

	// A pane that is NOT where its sprite is written, which is what makes this
	// case able to see the failure at all: the destination is the left half of
	// a pane that sits in the right half of the buffer, so a view whose pane
	// was quietly put back to fullscreen draws in the wrong half rather than
	// in the same half by coincidence.
	DrawList zero = harness.begin();
	zero.set_viewport(Viewport(32.0f, 0.0f, 32.0f, 64.0f));
	zero.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 32.0f, 64.0f), Colour(1.0f, 0.0f, 0.0f, 1.0f),
		0.0f, Vector2F::zero, SpriteFlip::none, 0.0f);

	// Raising the count mid-frame is legal and a scene does it whenever a
	// second player joins a running match. On D3D11 this is the call that
	// binds a render target and the screen viewport into a view's deferred
	// context, and it must bind the NEW one only.
	harness.renderer().set_view_count(2);

	DrawList one = harness.view(1);
	one.draw_sprite(harness.quad, Harness::white_texel(),
		RectangleF(0.0f, 0.0f, 32.0f, 64.0f), Colour(0.0f, 0.0f, 1.0f, 1.0f),
		0.0f, Vector2F::zero, SpriteFlip::none, 0.0f);

	harness.end();

	// View 0 in the right half under its own pane, view 1 in the left half
	// under the whole buffer. A rebind would have put both in the left half
	// and view 1 replays second, so the right half would be black.
	CHECK(harness.at(48, 32) == red_pixel);
	CHECK(harness.at(16, 32) == blue_pixel);
}

TEST_CASE("CONTRACT: a list reads back its camera and viewport, on a device")
{
	// NOT ABOUT A PIXEL EITHER, and so no golden image: nothing is drawn and
	// nothing is read back. It is here because it is the one file with a
	// device on all four rasterising backends, and the answer is not reached
	// the same way on all of them - gl and vulkan reset a Viewport kept beside
	// the camera, while the two Direct3D backends write theirs when a view is
	// bound, from the screen viewport they bind it with. This holds the four
	// to one answer: the whole back buffer to start with, what was given after
	// a set_viewport, and the whole of the new buffer after a resize restarts
	// the frame. null_tests.cpp asks the same of the backend without a device.
	Harness harness;
	Renderer& renderer = harness.renderer();
	const float size = static_cast<float>(buffer_side);

	DrawList list = harness.begin();
	CHECK(list.camera() == Camera::default_camera);
	CHECK(list.viewport() == Viewport(0.0f, 0.0f, size, size));

	// A fractional pane, which the Direct3D backends snap before the
	// rasteriser sees it: the reader answers as given, not as snapped.
	const Viewport pane(8.0f, 16.5f, 24.0f, 31.5f);
	const Camera zoomed(2.0f, 4.0f, 3.0f);
	list.set_viewport(pane);
	list.set_camera(zoomed);
	CHECK(list.viewport() == pane);
	CHECK(list.camera() == zoomed);

	// The window first, then the shell says so, for the reason "a resize
	// arriving mid-frame restarts the frame" gives.
	harness.resize_window(buffer_side / 2, buffer_side / 2);
	CHECK(renderer.window_size_changed(buffer_side / 2, buffer_side / 2));

	REQUIRE(list.valid());
	CHECK(list.camera() == Camera::default_camera);
	CHECK(list.viewport() ==
		Viewport(0.0f, 0.0f, size / 2.0f, size / 2.0f));

	renderer.submit();
}

TEST_CASE("a fullscreen scene overlays both split viewports after their drawing")
{
	class Solid final : public GameObject
	{
	public:
		Solid(TextureHandle texture, RectangleF rectangle) : texture_(texture), rectangle_(rectangle) {}
		void update(float) override {}
		RectangleF bounds() const override { return rectangle_; }
		void draw(DrawList& list) const override
		{
			list.draw_sprite(texture_, RectangleI(0, 0, 2, 2), rectangle_,
				Colour::white, 0, Vector2F::zero, SpriteFlip::none, 0);
		}
	private:
		TextureHandle texture_;
		RectangleF rectangle_;
	};
	Harness h;
	add_texture_asset(h.renderer(), h.resources(), "base-red", flat_texture(255, 0, 0));
	add_texture_asset(h.renderer(), h.resources(), "overlay-blue", flat_texture(0, 0, 255));
	Scene base(nullptr, nullptr);
	base.add(std::make_unique<Solid>(h.resources().resolve_texture("base-red"), RectangleF(0, 0, 32, 64)));
	base.end_tick();
	base.add_view(Viewport(0, 0, 32, 64));
	base.add_view(Viewport(32, 0, 32, 64));
	Scene overlay(nullptr, nullptr);
	overlay.add(std::make_unique<Solid>(h.resources().resolve_texture("overlay-blue"), RectangleF(16, 16, 32, 32)));
	overlay.end_tick();
	overlay.add_view(Viewport(0, 0, 64, 64));
	h.renderer().begin_frame();
	base.draw(h.renderer());
	overlay.draw(h.renderer());
	// This new composition contract is asserted at every pixel, so it needs
	// no generated reference image to accept the implementation's output.
	h.end_not_comparable();
	for (int y = 0; y < 64; ++y)
	for (int x = 0; x < 64; ++x)
		CHECK(h.at(x, y) == ((x >= 16 && x < 48 && y >= 16 && y < 48) ? blue_pixel : red_pixel));
}
